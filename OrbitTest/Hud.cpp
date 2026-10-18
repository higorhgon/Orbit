// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Hud.cpp : the prototype's HTML overlay (top bar, bottom bar, cards, messages), drawn
// in back buffer pixels after the scene.
//
// Sizes follow the prototype's CSS, which measures everything in a unit --u =
// height / 480 (written "9u" below; 1.5 pixels at 720p). The art was baked at
// --u = 1.5, so it is drawn at scale u / 1.5. The bars sit on the title-safe edges
// instead of the browser's 16px gutters, so a TV's overscan never cuts them.
//
// A "toast" is the short message panel that pops up above the bottom bar.
//
//   .top   : "ORBIT" (Michroma, #b98aff) + " LAUNCHER" (Share Tech Mono, #9c84c9),
//            clock on the right; 9u, letter-spacing 0.25em
//   .hud   : menu name (Michroma 9u, #b98aff, 0.3em) on the left; on the right the
//            current view's key hints in #cdb3ff with a 6px #a066ff glow,
//            0.22em spacing, 14u gaps, 30u x 22u pods with a 7u margin
//   toast  : Michroma 10u, #b98aff (problems in pink) on a plum panel just above the
//            bottom bar, 0.25s fade
//   .gameinfo : shelf title card 11% down (Orbitron 18u #f6e6ff, glow #cc33ff) and
//            a Share Tech Mono 9u line under it (#d9b8ff, 0.25em)
//   details card : the Files window's frame over a dimmed screen, label / value rows
//

#include "stdafx.h"
#include <xgraphics.h>
#include "Screen.h"
#include "Post.h"
#include "Assets.h"
#include "Hud.h"

#include "Shaders\HudVS.h"
#include "Shaders\HudSpritePS.h"
#include "Shaders\HudTextPS.h"

static const float ART_U = 1.5f;   // --u the art was baked at

enum { POD_A, POD_B, POD_X, POD_Y, POD_RB, NUM_PODS };

static D3DVertexShader*      s_pVS       = NULL;
static D3DPixelShader*       s_pSpritePS = NULL;
static D3DPixelShader*       s_pTextPS   = NULL;
static D3DVertexDeclaration* s_pDecl     = NULL;

static Font        s_Mono;
static Font        s_Display;
static BOOL        s_bMono    = FALSE;
static BOOL        s_bDisplay = FALSE;
static D3DTexture* s_pPod[NUM_PODS];
static SpriteInfo  s_PodInfo[NUM_PODS];

static char  s_szToast[64] = "";
static float s_fToastTime  = 0.0f;   // seconds left to show
static float s_fToastAlpha = 0.0f;

static Font        s_Title;
static BOOL        s_bTitle  = FALSE;
static D3DTexture* s_pCard   = NULL;
static D3DTexture* s_pCardLib = NULL;
static SpriteInfo  s_CardInfo;
static int         s_nCard   = HUD_CARD_DETAILS;
static D3DTexture* s_pDim    = NULL;
static D3DTexture* s_pToast[3] = { NULL, NULL, NULL };   // left cap, middle, right cap
static SpriteInfo  s_ToastInfo;                           // BoxW = cap width, BoxH = box height

// Bottom bar keys. The main menu shows no (B) BACK: B does nothing there.
static const HudKey c_MainKeys[] = { { HUD_POD_NONE, "\x01 \x02 MOVE" }, { HUD_POD_A, "SELECT" } };
static const int    MAX_KEYS = 8;
static HudKey       s_Keys[MAX_KEYS];
static int          s_nKeys = 0;

static char s_szMenuName[32] = "";
static char s_szInfoTitle[96] = "";
static char s_szInfoSub[96]   = "";

static const int MAX_ROWS = 6;
static int       s_nRows = 0;
static char      s_szRowLabel[MAX_ROWS][24];
static char      s_szRowValue[MAX_ROWS][96];
static HudKey    s_CardKeys[4];
static int       s_nCardKeys = 0;

void Hud_SetKeys( const HudKey* pKeys, int nKeys )
{
    s_nKeys = min( nKeys, MAX_KEYS );
    for( int i = 0; i < s_nKeys; i++ )
        s_Keys[i] = pKeys[i];
}

void Hud_SetMainKeys()
{
    Hud_SetKeys( c_MainKeys, ARRAYSIZE( c_MainKeys ) );
}

void Hud_SetMenuName( const char* szName )
{
    strncpy_s( s_szMenuName, szName ? szName : "", _TRUNCATE );
}

void Hud_SetGameInfo( const char* szTitle, const char* szSub )
{
    strncpy_s( s_szInfoTitle, szTitle ? szTitle : "", _TRUNCATE );
    strncpy_s( s_szInfoSub, szSub ? szSub : "", _TRUNCATE );
}

void Hud_SetDetails( int nRows, const char* const* pszLabels, const char* const* pszValues,
                     const HudKey* pKeys, int nKeys, int nCard )
{
    s_nCard = nCard;
    s_nRows = min( nRows, MAX_ROWS );
    for( int i = 0; i < s_nRows; i++ )
    {
        strncpy_s( s_szRowLabel[i], pszLabels[i], _TRUNCATE );
        strncpy_s( s_szRowValue[i], pszValues[i], _TRUNCATE );
    }
    s_nCardKeys = min( nKeys, 4 );
    for( int i = 0; i < s_nCardKeys; i++ )
        s_CardKeys[i] = pKeys[i];
}

// Colours (sRGB, 0..1)
typedef HudColor Color;
static const Color ION      = { 0xb9 / 255.0f, 0x8a / 255.0f, 0xff / 255.0f, 1.0f };
static const Color ION_DIM  = { 0x9c / 255.0f, 0x84 / 255.0f, 0xc9 / 255.0f, 1.0f };
static const Color KEY_TEXT = { 0xcd / 255.0f, 0xb3 / 255.0f, 0xff / 255.0f, 1.0f };
static const Color KEY_GLOW = { 160 / 255.0f, 102 / 255.0f, 255 / 255.0f, 0.7f };
static const Color NO_GLOW  = { 0.0f, 0.0f, 0.0f, 0.0f };
static const Color INFO_TITLE      = { 0xf6 / 255.0f, 0xe6 / 255.0f, 0xff / 255.0f, 1.0f };   // .gameinfo .gt
static const Color INFO_TITLE_GLOW = { 0xcc / 255.0f, 0x33 / 255.0f, 0xff / 255.0f, 1.0f };
static const Color INFO_SUB        = { 0xd9 / 255.0f, 0xb8 / 255.0f, 0xff / 255.0f, 1.0f };   // .gameinfo .gs


//-------------------------------------------------------------------------------------
// Name: Hud_Init()
//-------------------------------------------------------------------------------------
HRESULT Hud_Init( D3DDevice* pDevice )
{
    if( FAILED( pDevice->CreateVertexShader( g_pHudVS, &s_pVS ) ) )            return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pHudSpritePS, &s_pSpritePS ) ) ) return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pHudTextPS, &s_pTextPS ) ) )     return E_FAIL;

    D3DVERTEXELEMENT9 Elements[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( pDevice->CreateVertexDeclaration( Elements, &s_pDecl ) ) )
        return E_FAIL;

    // Missing art just leaves those pieces out
    s_bMono    = SUCCEEDED( Assets_LoadFont( pDevice, "mono", &s_Mono ) );
    s_bDisplay = SUCCEEDED( Assets_LoadFont( pDevice, "display", &s_Display ) );

    static const char* c_szPods[NUM_PODS] = { "pod_a", "pod_b", "pod_x", "pod_y", "pod_rb" };
    for( int i = 0; i < NUM_PODS; i++ )
    {
        if( FAILED( Assets_LoadSpriteInfo( c_szPods[i], &s_PodInfo[i] ) ) ||
            FAILED( Assets_LoadTexture( pDevice, c_szPods[i], &s_pPod[i] ) ) )
            s_pPod[i] = NULL;
    }

    s_bTitle = SUCCEEDED( Assets_LoadFont( pDevice, "title", &s_Title ) );
    if( FAILED( Assets_LoadSpriteInfo( "card", &s_CardInfo ) ) || FAILED( Assets_LoadTexture( pDevice, "card", &s_pCard ) ) )
        s_pCard = NULL;
    Assets_LoadTexture( pDevice, "card_lib", &s_pCardLib );
    Assets_LoadTexture( pDevice, "dim", &s_pDim );
    if( FAILED( Assets_LoadSpriteInfo( "toast_l", &s_ToastInfo ) ) ||
        FAILED( Assets_LoadTexture( pDevice, "toast_l", &s_pToast[0] ) ) ||
        FAILED( Assets_LoadTexture( pDevice, "toast_m", &s_pToast[1] ) ) ||
        FAILED( Assets_LoadTexture( pDevice, "toast_r", &s_pToast[2] ) ) )
        s_pToast[0] = NULL;

    Hud_SetMainKeys();
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Toast
//-------------------------------------------------------------------------------------
void Hud_ShowToast( const char* szText, float fSeconds )
{
    strncpy_s( s_szToast, szText, _TRUNCATE );
    s_fToastTime = fSeconds;
}

void Hud_Update( float dt )
{
    s_fToastTime = max( 0.0f, s_fToastTime - dt );
    float fTarget = ( s_fToastTime > 0.0f ) ? 1.0f : 0.0f;
    float fStep = dt / 0.25f;   // CSS transition: opacity .25s
    if( s_fToastAlpha < fTarget ) s_fToastAlpha = min( fTarget, s_fToastAlpha + fStep );
    else                          s_fToastAlpha = max( fTarget, s_fToastAlpha - fStep );
}


//-------------------------------------------------------------------------------------
// Drawing helpers
//-------------------------------------------------------------------------------------
struct HudVertex { float x, y, u, v; };

static const int MAX_RECTS = 64;
static HudVertex s_Verts[MAX_RECTS * 3];

// RECTLIST: top-left, top-right, bottom-left
static void PutRect( HudVertex* v, float x, float y, float w, float h,
                     float u0, float v0, float u1, float v1 )
{
    v[0].x = x;     v[0].y = y;     v[0].u = u0; v[0].v = v0;
    v[1].x = x + w; v[1].y = y;     v[1].u = u1; v[1].v = v0;
    v[2].x = x;     v[2].y = y + h; v[2].u = u0; v[2].v = v1;
}

// CSS letter-spacing adds after every character, the last one included
static float TextWidth( const Font& f, const char* sz, float fPx, float fSpacing )
{
    float fScale = fPx / f.Size, w = 0.0f;
    for( const char* p = sz; *p; p++ )
    {
        unsigned char c = (unsigned char)*p;
        if( c < 128 && f.bHas[c] )
            w += f.Glyphs[c].Advance * fScale + fSpacing;
    }
    return w;
}

static float DrawText( D3DDevice* pDevice, const Font& f, const char* sz, float x, float fBaseline,
                       float fPx, float fSpacing, const Color& Text, const Color& Glow, float fOpacity )
{
    float fScale = fPx / f.Size;
    int n = 0;
    float px = x;
    for( const char* p = sz; *p && n < MAX_RECTS; p++ )
    {
        unsigned char c = (unsigned char)*p;
        if( c >= 128 || !f.bHas[c] )
            continue;
        const Glyph& g = f.Glyphs[c];
        if( c != ' ' )
        {
            PutRect( &s_Verts[n * 3], floorf( px + g.ox * fScale + 0.5f ), floorf( fBaseline + g.oy * fScale + 0.5f ),
                     g.w * fScale, g.h * fScale,
                     g.x / f.AtlasW, g.y / f.AtlasH, ( g.x + g.w ) / f.AtlasW, ( g.y + g.h ) / f.AtlasH );
            n++;
        }
        px += g.Advance * fScale + fSpacing;
    }

    if( n )
    {
        float vText[4] = { Text.r, Text.g, Text.b, Text.a * fOpacity };
        float vGlow[4] = { Glow.r, Glow.g, Glow.b, Glow.a };
        pDevice->SetPixelShader( s_pTextPS );
        pDevice->SetPixelShaderConstantF( 0, vText, 1 );
        pDevice->SetPixelShaderConstantF( 1, vGlow, 1 );
        pDevice->SetTexture( 0, f.pTexture );
        pDevice->DrawPrimitiveUP( D3DPT_RECTLIST, n, s_Verts, sizeof(HudVertex) );
    }
    return px - x;
}

// Sprite stretched over a rect, at a colour level (linear) and opacity
static void DrawSprite( D3DDevice* pDevice, D3DTexture* pTex, float x, float y, float w, float h, float fLevel,
                        float fOpacity = 1.0f )
{
    float vParams[4] = { fOpacity, fLevel, 0.0f, 0.0f };
    PutRect( s_Verts, floorf( x + 0.5f ), floorf( y + 0.5f ), w, h, 0.0f, 0.0f, 1.0f, 1.0f );
    pDevice->SetPixelShader( s_pSpritePS );
    pDevice->SetPixelShaderConstantF( 0, vParams, 1 );
    pDevice->SetTexture( 0, pTex );
    pDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, s_Verts, sizeof(HudVertex) );
}

// Baked sprite with its CSS box at (x, y); the texture overhangs the box by its glow margin
static void DrawBoxSprite( D3DDevice* pDevice, D3DTexture* pTex, const SpriteInfo& inf, float x, float y, float s, float fLevel )
{
    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( pTex, 0, &Desc );
    DrawSprite( pDevice, pTex, x - inf.Pad * s, y - inf.Pad * s, Desc.Width * s, Desc.Height * s, fLevel );
}

// Pods are shaded at the menu panels' 0.8 level so they match them
static void DrawPod( D3DDevice* pDevice, int nPod, float x, float y, float s )
{
    if( nPod >= 0 && s_pPod[nPod] )
        DrawBoxSprite( pDevice, s_pPod[nPod], s_PodInfo[nPod], x, y, s, 0.8f );
}

// A row of key hints (pod + label, or a bare label) ending at xRight; returns its width
static float DrawKeys( D3DDevice* pDevice, const HudKey* pKeys, int nKeys, float xRight, float fMid,
                       float u, float s, const float vTrims[4], BOOL bMeasureOnly )
{
    const float fPx = 9.0f * u;
    const float fGap = 14.0f * u, fPodW = 30.0f * u, fPodH = 22.0f * u, fPodMargin = 7.0f * u;
    const float fSpacing = fPx * 0.22f;
    const float fBase = fMid + fPx * 0.35f;

    float fTotal = 0.0f;
    for( int i = 0; i < nKeys; i++ )
    {
        if( i ) fTotal += fGap;
        if( pKeys[i].nPod >= 0 ) fTotal += fPodW + fPodMargin;
        fTotal += TextWidth( s_Mono, pKeys[i].szLabel, fPx, fSpacing );
    }
    if( bMeasureOnly )
        return fTotal;

    float x = xRight - fTotal;
    for( int i = 0; i < nKeys; i++ )
    {
        if( pKeys[i].nPod >= 0 )
        {
            pDevice->SetPixelShaderConstantF( 1, vTrims, 1 );   // sprite
            DrawPod( pDevice, pKeys[i].nPod, x, fMid - fPodH * 0.5f, s );
            pDevice->SetPixelShaderConstantF( 2, vTrims, 1 );   // text
            x += fPodW + fPodMargin;
        }
        x += DrawText( pDevice, s_Mono, pKeys[i].szLabel, x, fBase, fPx, fSpacing, KEY_TEXT, KEY_GLOW, 1.0f );
        x += fGap;
    }
    return fTotal;
}


//-------------------------------------------------------------------------------------
// Name: SetHudStates()
//-------------------------------------------------------------------------------------
static void SetHudStates( D3DDevice* pDevice, float vTrims[4] )
{
    const ScreenInfo& scr = g_Screen;
    float vScreen[4] = { 2.0f / scr.Width, 2.0f / scr.Height, 0.0f, 0.0f };
    Post_GetTrims( vTrims );

    pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    pDevice->SetVertexDeclaration( s_pDecl );
    pDevice->SetVertexShader( s_pVS );
    pDevice->SetVertexShaderConstantF( 0, vScreen, 1 );
    pDevice->SetPixelShaderConstantF( 2, vTrims, 1 );   // text
    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
}



//-------------------------------------------------------------------------------------
// Drawing for views with their own panels (see Hud.h)
//-------------------------------------------------------------------------------------
static float s_vDrawTrims[4];

static const Font* FontFor( int nFont, BOOL* pbOK )
{
    switch( nFont )
    {
    case HUD_FONT_DISPLAY: *pbOK = s_bDisplay; return &s_Display;
    case HUD_FONT_TITLE:   *pbOK = s_bTitle;   return &s_Title;
    default:               *pbOK = s_bMono;    return &s_Mono;
    }
}

void Hud_BeginDraw( D3DDevice* pDevice )
{
    SetHudStates( pDevice, s_vDrawTrims );
}

void Hud_BeginDrawTarget( D3DDevice* pDevice, UINT w, UINT h )
{
    SetHudStates( pDevice, s_vDrawTrims );
    float vScreen[4] = { 2.0f / w, 2.0f / h, 0.0f, 0.0f };
    pDevice->SetVertexShaderConstantF( 0, vScreen, 1 );
    s_vDrawTrims[0] = 0.0f; s_vDrawTrims[1] = 1.0f; s_vDrawTrims[2] = 1.0f; s_vDrawTrims[3] = 0.0f;
}

void Hud_EndDraw( D3DDevice* pDevice )
{
    pDevice->SetTexture( 0, NULL );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}

float Hud_TextWidth( int nFont, const char* sz, float fPx, float fSpacing )
{
    BOOL bOK;
    const Font* pFont = FontFor( nFont, &bOK );
    return bOK ? TextWidth( *pFont, sz, fPx, fSpacing ) : 0.0f;
}

float Hud_FontAscent( int nFont, float fPx )
{
    BOOL bOK;
    const Font* pFont = FontFor( nFont, &bOK );
    return bOK ? pFont->Ascent * fPx / pFont->Size : fPx * 0.8f;
}

float Hud_DrawText( D3DDevice* pDevice, int nFont, const char* sz, float x, float fBaseline,
                    float fPx, float fSpacing, const HudColor& Text, const HudColor& Glow )
{
    BOOL bOK;
    const Font* pFont = FontFor( nFont, &bOK );
    if( !bOK )
        return 0.0f;
    pDevice->SetPixelShaderConstantF( 2, s_vDrawTrims, 1 );
    return DrawText( pDevice, *pFont, sz, x, fBaseline, fPx, fSpacing, Text, Glow, 1.0f );
}

void Hud_DrawSprite( D3DDevice* pDevice, D3DTexture* pTex, float x, float y, float w, float h, float fLevel )
{
    if( !pTex )
        return;
    pDevice->SetPixelShaderConstantF( 1, s_vDrawTrims, 1 );
    DrawSprite( pDevice, pTex, x, y, w, h, fLevel );
}


//-------------------------------------------------------------------------------------
// Name: Hud_Render()
//-------------------------------------------------------------------------------------
void Hud_Render( D3DDevice* pDevice )
{
    const ScreenInfo& scr = g_Screen;
    const float u = scr.Unit;
    const float s = u / ART_U;
    const float fPx = 9.0f * u;

    float vTrims[4];
    SetHudStates( pDevice, vTrims );

    const float L = (float)scr.SafeRect.left, R = (float)scr.SafeRect.right;
    const float T = (float)scr.SafeRect.top,  B = (float)scr.SafeRect.bottom;

    // Top bar: ORBIT  LAUNCHER ............................................ 01:23 PM
    float fTopBase = T + fPx * 0.85f;
    if( s_bDisplay && s_bMono )
    {
        float x = L;
        x += DrawText( pDevice, s_Display, "ORBIT", x, fTopBase, fPx, fPx * 0.25f, ION, NO_GLOW, 1.0f );
        DrawText( pDevice, s_Mono, "  LAUNCHER", x, fTopBase, fPx, fPx * 0.25f, ION_DIM, NO_GLOW, 1.0f );
    }
    if( s_bMono )
    {
        SYSTEMTIME st;
        GetLocalTime( &st );
        int nHour = st.wHour % 12;
        char szClock[16];
        sprintf_s( szClock, "%02d:%02d %s", nHour ? nHour : 12, st.wMinute, st.wHour < 12 ? "AM" : "PM" );
        float w = TextWidth( s_Mono, szClock, fPx, fPx * 0.25f );
        DrawText( pDevice, s_Mono, szClock, R - w, fTopBase, fPx, fPx * 0.25f, ION_DIM, NO_GLOW, 1.0f );
    }

    // Title card above a shelf: Orbitron title (shrunk to fit the safe width), small line under it
    if( s_bTitle && s_szInfoTitle[0] )
    {
        float fTitlePx = 18.0f * u;
        float fTitleSp = fTitlePx * 0.08f;
        float w = TextWidth( s_Title, s_szInfoTitle, fTitlePx, fTitleSp );
        if( w > R - L )
        {
            fTitlePx *= ( R - L ) / w;
            fTitleSp = fTitlePx * 0.08f;
            w = TextWidth( s_Title, s_szInfoTitle, fTitlePx, fTitleSp );
        }
        float fTop = scr.Height * 0.11f;
        float fBase = fTop + s_Title.Ascent * fTitlePx / s_Title.Size;
        DrawText( pDevice, s_Title, s_szInfoTitle, scr.Width * 0.5f - w * 0.5f, fBase, fTitlePx, fTitleSp, INFO_TITLE, INFO_TITLE_GLOW, 1.0f );

        if( s_bMono && s_szInfoSub[0] )
        {
            float fSubSp = fPx * 0.25f;
            float ws = TextWidth( s_Mono, s_szInfoSub, fPx, fSubSp );
            float fSubBase = fBase + fTitlePx * 0.35f + 6.0f * u + s_Mono.Ascent * fPx / s_Mono.Size;
            DrawText( pDevice, s_Mono, s_szInfoSub, scr.Width * 0.5f - ws * 0.5f, fSubBase, fPx, fSubSp, INFO_SUB, KEY_GLOW, 1.0f );
        }
    }

    // Bottom bar: menu name on the left, key hints on the right
    const float fBarMid = B - 11.0f * u;
    // The menu name gives way when a long row of keys needs the room (4:3)
    float fKeysW = ( s_bMono && s_nRows == 0 ) ? DrawKeys( pDevice, s_Keys, s_nKeys, R, fBarMid, u, s, vTrims, TRUE ) : 0.0f;
    if( s_bDisplay && s_szMenuName[0] &&
        L + TextWidth( s_Display, s_szMenuName, fPx, fPx * 0.3f ) + 14.0f * u < R - fKeysW )
        DrawText( pDevice, s_Display, s_szMenuName, L, fBarMid + fPx * 0.35f, fPx, fPx * 0.3f, ION, NO_GLOW, 1.0f );
    if( s_bMono && s_nRows == 0 )
        DrawKeys( pDevice, s_Keys, s_nKeys, R, fBarMid, u, s, vTrims, FALSE );

    // Details card over a dimmed screen
    if( s_nRows && s_pCard && s_bMono )
    {
        pDevice->SetPixelShaderConstantF( 1, vTrims, 1 );   // sprite
        if( s_pDim )
            DrawSprite( pDevice, s_pDim, 0.0f, 0.0f, (float)scr.Width, (float)scr.Height, 1.0f );
        const float cw = s_CardInfo.BoxW * s, ch = s_CardInfo.BoxH * s;
        const float cx = floorf( scr.Width * 0.5f - cw * 0.5f ), cy = floorf( scr.Height * 0.5f - ch * 0.5f );
        DrawBoxSprite( pDevice, ( s_nCard == HUD_CARD_LIBRARY && s_pCardLib ) ? s_pCardLib : s_pCard, s_CardInfo, cx, cy, s, 1.0f );
        pDevice->SetPixelShaderConstantF( 2, vTrims, 1 );   // text

        // Rows under the card's title bar (5u frame + 24u bar + 2u rule)
        const float fRowPx = 10.0f * u;
        float y = cy + 31.0f * u + 26.0f * u;
        for( int i = 0; i < s_nRows; i++ )
        {
            DrawText( pDevice, s_Mono, s_szRowLabel[i], cx + 20.0f * u, y, fPx, fPx * 0.2f, INFO_SUB, NO_GLOW, 1.0f );
            DrawText( pDevice, s_Mono, s_szRowValue[i], cx + 100.0f * u, y, fRowPx, fRowPx * 0.08f, INFO_TITLE, KEY_GLOW, 1.0f );
            y += 22.0f * u;
        }
        // The card's own keys along its bottom edge
        DrawKeys( pDevice, s_CardKeys, s_nCardKeys, cx + cw - 20.0f * u, cy + ch - 24.0f * u, u, s, vTrims, FALSE );
    }

    // Toast: a small plum panel centred just above the bottom bar (clear of the windows),
    // stretched to the message; problems in pink
    if( s_bDisplay && s_fToastAlpha > 0.0f && s_szToast[0] )
    {
        float fToastPx = 10.0f * u, fSp = fToastPx * 0.25f;
        float w = TextWidth( s_Display, s_szToast, fToastPx, fSp );
        const float fMaxW = ( R - L ) * 0.9f;
        if( w > fMaxW ) { fToastPx *= fMaxW / w; fSp = fToastPx * 0.25f; w = TextWidth( s_Display, s_szToast, fToastPx, fSp ); }
        const float fMidY = B - 11.0f * u - 34.0f * u;

        if( s_pToast[0] )
        {
            const SpriteInfo& ti = s_ToastInfo;
            XGTEXTURE_DESC Dl, Dr;
            XGGetTextureDesc( s_pToast[0], 0, &Dl );
            XGGetTextureDesc( s_pToast[2], 0, &Dr );
            const float fBoxH = ti.BoxH * s, fPad = ti.Pad * s;
            const float fInner = max( 0.0f, w + 20.0f * u - 2.0f * ti.BoxW * s );   // middle stretch
            const float fBoxW = 2.0f * ti.BoxW * s + fInner;
            const float x0 = floorf( scr.Width * 0.5f - fBoxW * 0.5f ) - fPad;
            const float y0 = floorf( fMidY - fBoxH * 0.5f ) - fPad;
            pDevice->SetPixelShaderConstantF( 1, vTrims, 1 );   // sprite
            DrawSprite( pDevice, s_pToast[0], x0, y0, Dl.Width * s, Dl.Height * s, 1.0f, s_fToastAlpha );
            DrawSprite( pDevice, s_pToast[1], x0 + Dl.Width * s, y0, fInner, Dl.Height * s, 1.0f, s_fToastAlpha );
            DrawSprite( pDevice, s_pToast[2], x0 + Dl.Width * s + fInner, y0, Dr.Width * s, Dr.Height * s, 1.0f, s_fToastAlpha );
            pDevice->SetPixelShaderConstantF( 2, vTrims, 1 );   // text
        }

        static const char* c_szBad[] = { "FAILED", "CAN'T", "COULDN'T", "PROTECTED", "NO ROOM", "NO PATHS", "NOT ", "THAT PATH", "ALREADY" };
        BOOL bBad = FALSE;
        for( int i = 0; i < ARRAYSIZE( c_szBad ); i++ )
            if( strncmp( s_szToast, c_szBad[i], strlen( c_szBad[i] ) ) == 0 )
                bBad = TRUE;
        static const Color TOAST_BAD = { 0xff / 255.0f, 0x9d / 255.0f, 0xe0 / 255.0f, 1.0f };
        DrawText( pDevice, s_Display, s_szToast, scr.Width * 0.5f - w * 0.5f, fMidY + fToastPx * 0.38f,
                  fToastPx, fSp, bBad ? TOAST_BAD : ION, NO_GLOW, s_fToastAlpha );
    }

    pDevice->SetTexture( 0, NULL );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}
