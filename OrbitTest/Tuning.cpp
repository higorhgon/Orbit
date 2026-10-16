// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Tuning.cpp : see Tuning.h. Values are kept as integer steps so the bars can be
// read back exactly ("bloom bar at 7").

#include "stdafx.h"
#include "Screen.h"
#include "Post.h"
#include "Tuning.h"

struct Trim
{
    int      Step;      // current
    int      Default;   // value Y resets to
    int      Max;       // steps run 0..Max
    float    Base;      // value at step 0
    float    Delta;     // per step
    D3DCOLOR Color;
};

// Defaults: picked by eye on a TV to match the prototype on a PC monitor (bloom 0.6,
// red 2.0, gamma 0.90); the prototype itself uses 1.0, 1.0, 1.0
static Trim s_Trims[3] =
{
    {  6,  6, 20, 0.0f, 0.10f, D3DCOLOR_XRGB( 0xdf, 0x80, 0xff ) },   // bloom: 0.0 .. 2.0
    { 26, 26, 36, 0.7f, 0.05f, D3DCOLOR_XRGB( 0xff, 0x40, 0x60 ) },   // red:   0.70 .. 2.50
    {  5,  5, 15, 0.8f, 0.02f, D3DCOLOR_XRGB( 0x80, 0xc0, 0xff ) },   // gamma: 0.80 .. 1.10
};

static BOOL s_bVisible = FALSE;

static float TrimValue( const Trim& t ) { return t.Base + t.Step * t.Delta; }

BOOL Tuning_IsVisible() { return s_bVisible; }

void Tuning_Init()
{
    Tuning_Update( 0 );
}

void Tuning_GetValues( float* pfBloom, float* pfRed, float* pfGamma )
{
    *pfBloom = TrimValue( s_Trims[0] );
    *pfRed   = TrimValue( s_Trims[1] );
    *pfGamma = TrimValue( s_Trims[2] );
}


static void Nudge( Trim& t, int nDir )
{
    t.Step = max( 0, min( t.Max, t.Step + nDir ) );
}


//-------------------------------------------------------------------------------------
// Name: Tuning_Update()
//-------------------------------------------------------------------------------------
void Tuning_Update( WORD wPressed )
{
    if( wPressed & XINPUT_GAMEPAD_BACK )
        s_bVisible = !s_bVisible;

    // Trims only change while the bars are showing
    if( !s_bVisible )
        wPressed = 0;

    if( wPressed & XINPUT_GAMEPAD_LEFT_SHOULDER )  Nudge( s_Trims[0], -1 );
    if( wPressed & XINPUT_GAMEPAD_RIGHT_SHOULDER ) Nudge( s_Trims[0], +1 );
    if( wPressed & XINPUT_GAMEPAD_DPAD_DOWN )      Nudge( s_Trims[1], -1 );
    if( wPressed & XINPUT_GAMEPAD_DPAD_UP )        Nudge( s_Trims[1], +1 );
    if( wPressed & XINPUT_GAMEPAD_DPAD_LEFT )      Nudge( s_Trims[2], -1 );
    if( wPressed & XINPUT_GAMEPAD_DPAD_RIGHT )     Nudge( s_Trims[2], +1 );
    if( wPressed & XINPUT_GAMEPAD_Y )
    {
        for( int i = 0; i < 3; i++ )
            s_Trims[i].Step = s_Trims[i].Default;
    }

    Post_SetTrims( TrimValue( s_Trims[0] ), TrimValue( s_Trims[1] ), TrimValue( s_Trims[2] ) );
}


//-------------------------------------------------------------------------------------
// Name: Tuning_DrawOverlay()
// Desc: Three rows of segments, filled up to the current step. A taller tick marks
//       the default (Y resets to it). Drawn with Clear rects, so it needs no art.
//-------------------------------------------------------------------------------------
void Tuning_DrawOverlay( D3DDevice* pDevice )
{
    if( !s_bVisible )
        return;

    const ScreenInfo& s = g_Screen;
    const float u = s.Unit;
    const LONG lSegW = (LONG)( 6.0f * u * ( 1.0f / s.PixelAspect ) );
    const LONG lSegH = (LONG)( 6.0f * u );
    const LONG lGap  = max( 1L, (LONG)( 2.0f * u ) );
    const LONG lTick = max( 1L, (LONG)( 3.0f * u ) );

    LONG x0 = s.SafeRect.left;
    LONG y  = s.SafeRect.bottom - 3 * ( lSegH + lTick * 2 + lGap * 2 );

    for( int r = 0; r < 3; r++ )
    {
        const Trim& t = s_Trims[r];
        D3DRECT On[40], Off[40], Mark;
        DWORD nOn = 0, nOff = 0;

        for( int i = 0; i <= t.Max && i < 40; i++ )
        {
            D3DRECT rc;
            rc.x1 = x0 + i * ( lSegW + lGap );
            rc.x2 = rc.x1 + lSegW;
            rc.y1 = y;
            rc.y2 = y + lSegH;
            if( i <= t.Step ) On[nOn++] = rc; else Off[nOff++] = rc;
        }

        // Default marker: a bar under the prototype's step
        Mark.x1 = x0 + t.Default * ( lSegW + lGap );
        Mark.x2 = Mark.x1 + lSegW;
        Mark.y1 = y + lSegH + lGap;
        Mark.y2 = Mark.y1 + lTick;

        if( nOff ) pDevice->Clear( nOff, Off, D3DCLEAR_TARGET, D3DCOLOR_XRGB( 0x30, 0x10, 0x48 ), 1.0f, 0L );
        if( nOn )  pDevice->Clear( nOn, On, D3DCLEAR_TARGET, t.Color, 1.0f, 0L );
        pDevice->Clear( 1, &Mark, D3DCLEAR_TARGET, D3DCOLOR_XRGB( 0xff, 0xff, 0xff ), 1.0f, 0L );

        y += lSegH + lTick * 2 + lGap * 2;
    }
}
