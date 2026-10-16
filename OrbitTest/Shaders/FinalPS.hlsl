// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// FinalPS.hlsl : scene + bloom, then the OutputPass sRGB encode, to the back buffer.
// (The prototype's retro pass would slot in here later.)
//
// TV trims, so the TV can be matched by eye against the prototype on a PC monitor:
//   bloom strength (prototype 1.0), red gain in linear light, and an output gamma
//   exponent (below 1 lifts the mid-tones a TV darkens).
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Scene  : register(s0);   // fp16, point
sampler2D g_Bloom  : register(s1);   // half res, bilinear
float4    g_vTrims : register(c0);   // x = bloom strength, y = red gain, z = gamma exponent

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float3 c = tex2D( g_Scene, UV ).rgb + BloomDecode( tex2D( g_Bloom, UV ).rgb ) * g_vTrims.x;
    c.r *= g_vTrims.y;
    float3 s = LinearToSRGB( saturate( c ) );
    return float4( pow( s, g_vTrims.z ), 1.0 );
}
