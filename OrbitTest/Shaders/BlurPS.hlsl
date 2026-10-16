// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// BlurPS.hlsl : UnrealBloomPass separable gaussian. Kernel radius 3..11 by mip
// with sigma = radius; normalised weights come from the CPU, unused taps are 0.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Src      : register(s0);
float4    g_vStep    : register(c0);      // xy = direction / size
float4    g_vWeights[3] : register(c1);   // weights 0..11, packed four per register

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float3 sum = BloomDecode( tex2D( g_Src, UV ).rgb ) * g_vWeights[0].x;

    [unroll]
    for( int i = 1; i < 11; i++ )
    {
        float  w   = g_vWeights[i / 4][i % 4];
        float2 off = g_vStep.xy * i;
        sum += ( BloomDecode( tex2D( g_Src, UV + off ).rgb )
               + BloomDecode( tex2D( g_Src, UV - off ).rgb ) ) * w;
    }
    return BloomEncode( sum );
}
