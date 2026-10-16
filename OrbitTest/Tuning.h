// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Tuning.h : a developer tool for the "TV trims", the final colour corrections (bloom
// strength, red gain, gamma) that make the console's picture on a TV match the
// prototype on a PC monitor. Hidden by default; the bars show the current values so
// good ones can be read off and made the defaults (Tuning.cpp).
//
//   Back            show / hide the bars (trims only change while shown)
//   LB / RB         bloom strength  -/+ 0.1   (0.0 .. 2.0,  default 0.6)
//   D-pad down / up red gain        -/+ 0.05  (0.70 .. 2.50, default 2.0)
//   D-pad left/right gamma exponent -/+ 0.02  (0.80 .. 1.10, default 0.90)
//   Y               reset all three to the defaults
//   X               next debug view (handled in OrbitTest.cpp)

#pragma once

// Applies the default trims to the final pass
void Tuning_Init();
void Tuning_GetValues( float* pfBloom, float* pfRed, float* pfGamma );

// wPressed = buttons newly pressed this frame
void Tuning_Update( WORD wPressed );

// TRUE while the bars are showing (the D-pad belongs to the tuning then)
BOOL Tuning_IsVisible();

// Bars in the bottom-left of the safe area; call with the back buffer set
void Tuning_DrawOverlay( D3DDevice* pDevice );
