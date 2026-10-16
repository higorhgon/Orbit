// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// StarVS.hlsl : one star = a small screen-aligned square (the prototype's
// THREE.Points). Size follows three.js sizeAttenuation at 480 lines
// (size * 240 / depth, at least 1 pixel), scaled to our height.
//-------------------------------------------------------------------------------------

float4x4 g_matViewProj : register(c0);
float4   g_vStar       : register(c4);   // x = 2 / width, y = 2 / height, z = height / 480

float4 main( float3 Pos : POSITION, float2 Corner : TEXCOORD0 ) : POSITION
{
    float4 vClip = mul( g_matViewProj, float4( Pos, 1.0 ) );
    float fSize = max( 1.0, 0.06 * 240.0 / max( vClip.w, 0.001 ) ) * g_vStar.z;
    vClip.xy += Corner * 0.5 * fSize * g_vStar.xy * vClip.w;
    return vClip;
}
