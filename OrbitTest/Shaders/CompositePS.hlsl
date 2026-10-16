// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// CompositePS.hlsl : UnrealBloomPass composite: the five blurred mips, weighted
// by lerpBloomFactor(factor) * strength, summed at half resolution.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Mip0 : register(s0);
sampler2D g_Mip1 : register(s1);
sampler2D g_Mip2 : register(s2);
sampler2D g_Mip3 : register(s3);
sampler2D g_Mip4 : register(s4);
float4    g_vFactors : register(c0);   // mips 0..3
float4    g_vFactor4 : register(c1);   // x = mip 4

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float3 c = BloomDecode( tex2D( g_Mip0, UV ).rgb ) * g_vFactors.x
             + BloomDecode( tex2D( g_Mip1, UV ).rgb ) * g_vFactors.y
             + BloomDecode( tex2D( g_Mip2, UV ).rgb ) * g_vFactors.z
             + BloomDecode( tex2D( g_Mip3, UV ).rgb ) * g_vFactors.w
             + BloomDecode( tex2D( g_Mip4, UV ).rgb ) * g_vFactor4.x;
    return BloomEncode( c );
}
