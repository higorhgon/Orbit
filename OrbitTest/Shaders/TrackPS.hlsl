// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// TrackPS.hlsl : the thin ring under the menu (#b98aff, opacity 0.25, fog),
// premultiplied output.
//-------------------------------------------------------------------------------------

float4 g_vParams : register(c0);   // x = opacity, y = fog near, z = fog far
float4 g_vFog    : register(c1);   // fog colour, linear
float4 g_vColor  : register(c2);   // linear

float4 main( float Depth : TEXCOORD1 ) : COLOR
{
    float3 c = lerp( g_vColor.rgb, g_vFog.rgb, smoothstep( g_vParams.y, g_vParams.z, Depth ) );
    return float4( c * g_vParams.x, g_vParams.x );
}
