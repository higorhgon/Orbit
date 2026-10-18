// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Post.h : the frame pipeline, following the XDK HDRLighting sample.
//
//   scene  : 7e3 (A2B10G10R10F_EDRAM) render target, linear light, resolved to an
//            fp16 texture
//   bloom  : UnrealBloomPass port (strength 1.0, radius 0.45, threshold 0.45),
//            five mips in 8-bit buffers
//   final  : scene + bloom, sRGB encode, TV trims, to the back buffer
//

#pragma once

HRESULT Post_Init( D3DDevice* pDevice );

// Sets the scene target and clears it. Draw the 3D scene in linear light after this.
void Post_BeginScene( D3DDevice* pDevice );

// Bloom and composite to the back buffer (left set as the render target). The caller
// can draw overlays, then Presents.
void Post_EndFrame( D3DDevice* pDevice );

// After Post_EndFrame: binds (TRUE) or unbinds the scene's depth buffer to the back
// buffer, so overlays can be hidden by the orb. The depth is still in EDRAM, past
// the tiles the back buffer and the bloom passes use.
void Post_SetSceneDepth( D3DDevice* pDevice, BOOL bBind );

// For the overlay shaders (HUD, panels, cases): x = theme hue turn, y = red gain,
// z = gamma exponent
void Post_GetTrims( float vTrims[4] );

// Colour theme: turns every hue on screen by this many degrees (0 = Orbit's purple),
// or with bFixed paints every colour in that one hue. Applied last, after the TV
// trims, to the scene, the ring panels and the HUD; the shelf cases and box art keep
// their real colours.
void Post_SetTheme( float fHueDegrees, BOOL bFixed );

// Full-target RECTLIST with uv (0,0) top-left. Caller sets the pixel shader; this
// sets the vertex shader, declaration and quad states.
void Post_DrawQuad( D3DDevice* pDevice );
