// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Sound.cpp : see Sound.h. The prototype builds these with Web Audio oscillators; here
// the same waveforms are rendered once into 16-bit PCM and handed to XAudio2.

#include "stdafx.h"
#include <xaudio2.h>
#include "Sound.h"

static const float MASTER = 0.6f;

static IXAudio2*               s_pXAudio = NULL;
static IXAudio2MasteringVoice* s_pMaster = NULL;

static const char* SOUND_FILE = "game:\\sound.txt";
static BOOL        s_bEnabled = TRUE;

// Blips: a few voices each, so quick repeats overlap instead of cutting off
static const int    VOICES_PER_SOUND = 3;
static const UINT32 BLIP_RATE = 48000;
static IXAudio2SourceVoice* s_pBlipVoice[NUM_SOUNDS][VOICES_PER_SOUND];
static int                  s_nNextVoice[NUM_SOUNDS];
static SHORT*               s_pBlip[NUM_SOUNDS];
static UINT32               s_nBlipSamples[NUM_SOUNDS];

// Hum: a 3s fade-in, then a seamless 25s loop (every part of it repeats in 25s:
// 55 x 25, 55.4 x 25, 110.2 x 25 and 0.08 x 25 are all whole cycles)
static const UINT32 HUM_RATE = 24000;   // it's all below 1 kHz after the filter
static IXAudio2SourceVoice* s_pHumVoice = NULL;
static SHORT*               s_pHum = NULL;

static const float TWO_PI = 6.28318531f;

static SHORT ToPcm( float f )
{
    f = max( -1.0f, min( 1.0f, f ) );
    return (SHORT)( f * 32767.0f );
}

// Band-limited saw (polyBLEP), phase 0..1, step = frequency / rate
static float Saw( float fPhase, float fStep )
{
    float v = 2.0f * fPhase - 1.0f;
    if( fPhase < fStep )             { float t = fPhase / fStep;         v -= t + t - t * t - 1.0f; }
    else if( fPhase > 1.0f - fStep ) { float t = ( fPhase - 1.0f ) / fStep; v -= t * t + t + t + 1.0f; }
    return v;
}

static float Triangle( float fPhase )
{
    return 1.0f - 4.0f * fabsf( fPhase - 0.5f );
}


//-------------------------------------------------------------------------------------
// Blips: the prototype's blip(freq, dur, type, vol, slide): frequency slides
// exponentially to freq * slide and the gain from vol to 0.0001 over dur
//-------------------------------------------------------------------------------------
static void AddBlip( float* pOut, UINT32 nOut, float fStartSec, float fFreq, float fDur, BOOL bSine, float fVol, float fSlide )
{
    UINT32 n0 = (UINT32)( fStartSec * BLIP_RATE );
    UINT32 n = (UINT32)( fDur * BLIP_RATE );
    float fPhase = 0.0f;
    for( UINT32 i = 0; i < n && n0 + i < nOut; i++ )
    {
        float t = (float)i / n;
        float f = fFreq * powf( fSlide, t );
        float g = fVol * powf( 0.0001f / fVol, t );
        float s = bSine ? sinf( TWO_PI * fPhase ) : Triangle( fPhase );
        pOut[n0 + i] += s * g * MASTER;
        fPhase += f / BLIP_RATE;
        fPhase -= floorf( fPhase );
    }
}

static void MakeBlips()
{
    struct Part { float fStart, fFreq, fDur; BOOL bSine; float fVol, fSlide; };
    static const Part c_Move[]   = { { 0.0f,  660.0f, 0.08f, FALSE, 0.10f, 1.2f } };
    static const Part c_Select[] = { { 0.0f,  440.0f, 0.18f, TRUE,  0.18f, 2.0f },
                                     { 0.06f, 880.0f, 0.25f, TRUE,  0.12f, 1.5f } };
    static const Part c_Back[]   = { { 0.0f,  520.0f, 0.15f, FALSE, 0.12f, 0.5f } };
    const Part* pParts[NUM_SOUNDS] = { c_Move, c_Select, c_Back };
    const int   nParts[NUM_SOUNDS] = { ARRAYSIZE( c_Move ), ARRAYSIZE( c_Select ), ARRAYSIZE( c_Back ) };

    for( int s = 0; s < NUM_SOUNDS; s++ )
    {
        float fLen = 0.0f;
        for( int p = 0; p < nParts[s]; p++ )
            fLen = max( fLen, pParts[s][p].fStart + pParts[s][p].fDur );
        UINT32 n = (UINT32)( fLen * BLIP_RATE ) + 1;
        float* pMix = new float[n];
        ZeroMemory( pMix, n * sizeof(float) );
        for( int p = 0; p < nParts[s]; p++ )
        {
            const Part& q = pParts[s][p];
            AddBlip( pMix, n, q.fStart, q.fFreq, q.fDur, q.bSine, q.fVol, q.fSlide );
        }
        s_pBlip[s] = new SHORT[n];
        for( UINT32 i = 0; i < n; i++ )
            s_pBlip[s][i] = ToPcm( pMix[i] );
        s_nBlipSamples[s] = n;
        delete[] pMix;
    }
}


//-------------------------------------------------------------------------------------
// Hum: oscillators -> low-pass (Web Audio BiquadFilter, Q 1 dB) swept by the LFO
//-------------------------------------------------------------------------------------
static const float HUM_FADE = 3.0f, HUM_LOOP = 25.0f, HUM_LEVEL = 0.08f;

static void MakeHum( UINT32* pnFade, UINT32* pnLoop )
{
    const UINT32 nWarm = HUM_RATE / 2;                       // let the filter settle first
    const UINT32 nFade = (UINT32)( HUM_FADE * HUM_RATE );
    const UINT32 nLoop = (UINT32)( HUM_LOOP * HUM_RATE );
    s_pHum = new SHORT[nFade + nLoop];

    float ph[3] = { 0.0f, 0.0f, 0.0f };
    const float fFreq[3] = { 55.0f, 55.4f, 110.2f };
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    const float fQ = powf( 10.0f, 1.0f / 20.0f );

    for( UINT32 i = 0; i < nWarm + nFade + nLoop; i++ )
    {
        float t = ( (float)i - nWarm ) / HUM_RATE;
        // New filter coefficients every 32 samples (the LFO is very slow)
        if( ( i & 31 ) == 0 )
        {
            float fc = 400.0f + 180.0f * sinf( TWO_PI * 0.08f * t );
            float w0 = TWO_PI * fc / HUM_RATE, cs = cosf( w0 ), alpha = sinf( w0 ) / ( 2.0f * fQ );
            float a0 = 1.0f + alpha;
            b0 = ( 1.0f - cs ) * 0.5f / a0; b1 = ( 1.0f - cs ) / a0; b2 = b0;
            a1 = -2.0f * cs / a0;           a2 = ( 1.0f - alpha ) / a0;
        }
        float x = Saw( ph[0], fFreq[0] / HUM_RATE ) + Saw( ph[1], fFreq[1] / HUM_RATE ) + sinf( TWO_PI * ph[2] );
        for( int k = 0; k < 3; k++ )
        {
            ph[k] += fFreq[k] / HUM_RATE;
            ph[k] -= floorf( ph[k] );
        }
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;

        if( i < nWarm )
            continue;
        UINT32 j = i - nWarm;
        // linearRampToValueAtTime(0.08, now + 3): the fade's gain rises to the loop's
        float g = HUM_LEVEL * MASTER * ( j < nFade ? (float)j / nFade : 1.0f );
        s_pHum[j] = ToPcm( y * g );
    }
    *pnFade = nFade;
    *pnLoop = nLoop;
}


//-------------------------------------------------------------------------------------
// On / off: sound.txt holds ON or OFF; anything else is on
//-------------------------------------------------------------------------------------
static void LoadEnabled()
{
    FILE* f = NULL;
    if( fopen_s( &f, SOUND_FILE, "r" ) != 0 || !f )
        return;
    char sz[16] = "";
    if( fgets( sz, sizeof(sz), f ) )
    {
        sz[strcspn( sz, "\r\n" )] = 0;
        s_bEnabled = _stricmp( sz, "OFF" ) != 0;
    }
    fclose( f );
}

BOOL Sound_IsEnabled() { return s_bEnabled; }

// Off mutes the master voice, so the hum keeps its place in the loop and comes back mid-hum
void Sound_SetEnabled( BOOL bEnabled )
{
    s_bEnabled = bEnabled;
    if( s_pMaster )
        s_pMaster->SetVolume( s_bEnabled ? 1.0f : 0.0f );
    FILE* f = NULL;
    if( fopen_s( &f, SOUND_FILE, "w" ) == 0 && f )
    {
        fprintf( f, "%s\n", s_bEnabled ? "ON" : "OFF" );
        fclose( f );
    }
}


//-------------------------------------------------------------------------------------
// Name: Sound_Init()
//-------------------------------------------------------------------------------------
void Sound_Init()
{
    LoadEnabled();
    if( FAILED( XAudio2Create( &s_pXAudio, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
        return;
    if( FAILED( s_pXAudio->CreateMasteringVoice( &s_pMaster ) ) )
    {
        s_pXAudio->Release();
        s_pXAudio = NULL;
        return;
    }
    if( !s_bEnabled )
        s_pMaster->SetVolume( 0.0f );

    MakeBlips();
    WAVEFORMATEX Blip = { WAVE_FORMAT_PCM, 1, BLIP_RATE, BLIP_RATE * 2, 2, 16, 0 };
    for( int s = 0; s < NUM_SOUNDS; s++ )
        for( int v = 0; v < VOICES_PER_SOUND; v++ )
            if( FAILED( s_pXAudio->CreateSourceVoice( &s_pBlipVoice[s][v], &Blip ) ) )
                s_pBlipVoice[s][v] = NULL;

    // Hum: the fade once, then the loop forever
    UINT32 nFade, nLoop;
    MakeHum( &nFade, &nLoop );
    WAVEFORMATEX Hum = { WAVE_FORMAT_PCM, 1, HUM_RATE, HUM_RATE * 2, 2, 16, 0 };
    if( SUCCEEDED( s_pXAudio->CreateSourceVoice( &s_pHumVoice, &Hum ) ) )
    {
        XAUDIO2_BUFFER Fade = { 0 };
        Fade.AudioBytes = nFade * 2;
        Fade.pAudioData = (const BYTE*)s_pHum;
        XAUDIO2_BUFFER Loop = { 0 };
        Loop.AudioBytes = nLoop * 2;
        Loop.pAudioData = (const BYTE*)( s_pHum + nFade );
        Loop.LoopCount = XAUDIO2_LOOP_INFINITE;
        s_pHumVoice->SubmitSourceBuffer( &Fade );
        s_pHumVoice->SubmitSourceBuffer( &Loop );
        s_pHumVoice->Start( 0 );
    }
}


//-------------------------------------------------------------------------------------
// Name: Sound_Play()
//-------------------------------------------------------------------------------------
void Sound_Play( int nSound )
{
    if( !s_pXAudio || !s_bEnabled || nSound < 0 || nSound >= NUM_SOUNDS )
        return;
    IXAudio2SourceVoice* pVoice = s_pBlipVoice[nSound][s_nNextVoice[nSound]];
    s_nNextVoice[nSound] = ( s_nNextVoice[nSound] + 1 ) % VOICES_PER_SOUND;
    if( !pVoice )
        return;
    pVoice->Stop( 0 );
    pVoice->FlushSourceBuffers();
    XAUDIO2_BUFFER Buf = { 0 };
    Buf.AudioBytes = s_nBlipSamples[nSound] * 2;
    Buf.pAudioData = (const BYTE*)s_pBlip[nSound];
    Buf.Flags = XAUDIO2_END_OF_STREAM;
    pVoice->SubmitSourceBuffer( &Buf );
    pVoice->Start( 0 );
}
