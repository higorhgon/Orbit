// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Hud.h : top bar (ORBIT LAUNCHER, profile badge, clock), bottom bar (menu name + button hints),
// the details card and the toast (a short message above the bottom bar), laid out in
// the prototype's --u units (see Hud.cpp).

#pragma once

HRESULT Hud_Init( D3DDevice* pDevice );
void    Hud_Update( float dt );
void    Hud_ShowToast( const char* szText, float fSeconds );

// Bottom bar key hints, right aligned: each is a button pod (or none) and a label.
// The main menu's set is the default; a view swaps in its own while it is open.
enum { HUD_POD_NONE = -1, HUD_POD_A, HUD_POD_B, HUD_POD_X, HUD_POD_Y, HUD_POD_RB, HUD_POD_LB, HUD_POD_LT };
struct HudKey
{
    int         nPod;
    const char* szLabel;   // static strings only; the HUD keeps the pointer
};
void    Hud_SetKeys( const HudKey* pKeys, int nKeys );
void    Hud_SetMainKeys();

// Profile badge in the top bar's middle: gamer picture in a frame, gamertag beside it.
// szName NULL or "" hides it; pPicture NULL shows the frame empty. The name is copied;
// the picture is drawn as is (Profile.cpp owns it).
void    Hud_SetProfile( const char* szName, D3DTexture* pPicture );

// Bottom bar menu name on the left (the prototype's .now); NULL or "" = none
void    Hud_SetMenuName( const char* szName );

// Title card above a shelf (.gameinfo): big title and a small line under it; NULL hides it
void    Hud_SetGameInfo( const char* szTitle, const char* szSub );

// Card over a dimmed screen: rows of label / value, and its own key hints along the
// bottom edge. nRows = 0 hides it. Strings are copied. The card's title bar reads
// DETAILS (a shelf item) or LIBRARY (the library updating).
enum { HUD_CARD_DETAILS, HUD_CARD_LIBRARY };
void    Hud_SetDetails( int nRows, const char* const* pszLabels, const char* const* pszValues,
                        const HudKey* pKeys, int nKeys, int nCard = HUD_CARD_DETAILS );

// Call with the back buffer set, after Post_EndFrame
void    Hud_Render( D3DDevice* pDevice );

// Drawing for views that lay out their own panels (the file manager). Positions are
// back buffer pixels; call between Hud_BeginDraw and Hud_EndDraw with the back buffer
// set. Colours are sRGB 0..1; a is opacity (for a glow, its strength; 0 = none).
enum { HUD_FONT_MONO, HUD_FONT_DISPLAY, HUD_FONT_TITLE };
struct HudColor { float r, g, b, a; };
void    Hud_BeginDraw( D3DDevice* pDevice );
// The same into a w x h render target that becomes art (a cover): no TV trims, which
// the art gets later wherever it's shown
void    Hud_BeginDrawTarget( D3DDevice* pDevice, UINT w, UINT h );
void    Hud_EndDraw( D3DDevice* pDevice );
float   Hud_TextWidth( int nFont, const char* sz, float fPx, float fSpacing );
float   Hud_FontAscent( int nFont, float fPx );
float   Hud_DrawText( D3DDevice* pDevice, int nFont, const char* sz, float x, float fBaseline,
                      float fPx, float fSpacing, const HudColor& Text, const HudColor& Glow );
// Stretched over the rect at a colour level (linear; 1 = as baked)
void    Hud_DrawSprite( D3DDevice* pDevice, D3DTexture* pTex, float x, float y, float w, float h, float fLevel );
