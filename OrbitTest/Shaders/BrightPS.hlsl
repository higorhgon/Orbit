// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// BrightPS.hlsl : UnrealBloomPass luminosity high pass (threshold 0.45, smooth
// width 0.01), reading the full-res scene into the half-res bright buffer.
// The scene texture is fp16 and is point sampled, so the 2x2 average three.js
// gets from bilinear filtering is done by hand.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Scene : register(s0);
float4    g_vTexel : register(c0);   // xy = 1 / scene size

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float2 o = g_vTexel.xy * 0.5;
    float3 c = ( tex2D( g_Scene, UV + float2( -o.x, -o.y ) ).rgb
               + tex2D( g_Scene, UV + float2(  o.x, -o.y ) ).rgb
               + tex2D( g_Scene, UV + float2( -o.x,  o.y ) ).rgb
               + tex2D( g_Scene, UV + float2(  o.x,  o.y ) ).rgb ) * 0.25;

    float luma = dot( c, float3( 0.299, 0.587, 0.114 ) );
    float a = smoothstep( 0.45, 0.46, luma );
    return BloomEncode( c * a );
}
