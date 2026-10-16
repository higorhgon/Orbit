// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Backdrop.h : purple gradient background (halo rings, haze, vignette) and starfield.

#pragma once

HRESULT Backdrop_Init( D3DDevice* pDevice );

// scene.background: draw first in the scene pass
void Backdrop_RenderBackground( D3DDevice* pDevice );

// Starfield: transparent, drawn in depth order with the ring and the orb's shell
void Backdrop_RenderStars( D3DDevice* pDevice, const XMMATRIX& matViewProj );
