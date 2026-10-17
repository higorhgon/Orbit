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

// TV trims applied in the final pass (see Tuning.h; the prototype has 1.0, 1.0, 1.0)
void Post_SetTrims( float fBloomStrength, float fRedGain, float fGamma );

// x = bloom strength, y = red gain, z = gamma exponent (for the HUD shaders)
void Post_GetTrims( float vTrims[4] );

// Full-target RECTLIST with uv (0,0) top-left. Caller sets the pixel shader; this
// sets the vertex shader, declaration and quad states.
void Post_DrawQuad( D3DDevice* pDevice );
