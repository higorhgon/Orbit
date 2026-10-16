// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// SolidVS.hlsl : plain positioned mesh (the orb's dark nucleus).
//-------------------------------------------------------------------------------------

float4x4 g_matViewProj : register(c0);
float4   g_vWorld      : register(c4);   // xyz = position, w = uniform scale

float4 main( float3 Pos : POSITION ) : POSITION
{
    return mul( g_matViewProj, float4( Pos * g_vWorld.w + g_vWorld.xyz, 1.0 ) );
}
