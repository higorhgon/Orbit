// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// SolidPS.hlsl : flat colour, passed in already in the target's colour space (linear
// for the scene: the orb's nucleus and the stars).
//-------------------------------------------------------------------------------------

float4 g_vColor : register(c0);

float4 main() : COLOR
{
    return g_vColor;
}
