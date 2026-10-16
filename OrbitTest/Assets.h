// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Assets.h : loads the baked art from game:\Media (written by Tools/bake.js).
// A missing or bad file returns a failure and the caller skips that element, so a
// forgotten Media folder shows up as missing pieces on screen, not a crash.

#pragma once

// .tex: linear A8R8G8B8 texture with all mip levels in the file
HRESULT Assets_LoadTexture( D3DDevice* pDevice, const char* szName, D3DTexture** ppTexture );

// .inf: the CSS layout box a sprite was drawn for, at --u = 1.5 (see Hud.cpp)
struct SpriteInfo
{
    float BoxW, BoxH;   // the element's box
    float Pad;          // transparent margin around it in the texture (for the glow)
};
HRESULT Assets_LoadSpriteInfo( const char* szName, SpriteInfo* pInfo );

// .fnt + .tex: bitmap font, R = glyph, G = glow
struct Glyph
{
    float x, y, w, h;   // cell in the atlas, pixels
    float ox, oy;       // cell offset from the pen position on the baseline
    float Advance;
};

struct Font
{
    D3DTexture* pTexture;
    float       AtlasW, AtlasH;
    float       Size;        // px the atlas was drawn at (9u at u = 1.5)
    float       Ascent;
    Glyph       Glyphs[128]; // indexed by code; codes 1 and 2 are the left / right triangles
    BOOL        bHas[128];
};
HRESULT Assets_LoadFont( D3DDevice* pDevice, const char* szName, Font* pFont );
