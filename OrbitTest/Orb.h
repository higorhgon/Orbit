// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Orb.h : the purple noise orb (glowing displaced shell around a dark nucleus).

#pragma once

HRESULT Orb_Init( D3DDevice* pDevice );

// fTime  = the prototype's uTime (seconds, scaled by animation speed)
// fPulse = 0..1, set to 1 on select and decayed by the caller
// fY     = height of the orb's centre (it floats up to 0.6 while a shelf is open)
// The nucleus is opaque and draws before the transparent pieces; the shell is
// transparent and draws in depth order with them (see Ring_Render).
void Orb_RenderNucleus( D3DDevice* pDevice, const XMMATRIX& matViewProj, float fTime, float fPulse, float fY );
void Orb_RenderShell( D3DDevice* pDevice, const XMMATRIX& matViewProj, const XMVECTOR& vEye,
                      float fTime, float fPulse, float fY );
