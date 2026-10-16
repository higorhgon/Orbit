// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// DownPS.hlsl : halves a bloom buffer with a 2x2 box (what three.js gets by
// bilinear-sampling the previous mip at the next mip's resolution).
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Src   : register(s0);
float4    g_vTexel : register(c0);   // xy = 1 / source size

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float2 o = g_vTexel.xy * 0.5;
    float3 c = BloomDecode( tex2D( g_Src, UV + float2( -o.x, -o.y ) ).rgb )
             + BloomDecode( tex2D( g_Src, UV + float2(  o.x, -o.y ) ).rgb )
             + BloomDecode( tex2D( g_Src, UV + float2( -o.x,  o.y ) ).rgb )
             + BloomDecode( tex2D( g_Src, UV + float2(  o.x,  o.y ) ).rgb );
    return BloomEncode( c * 0.25 );
}
