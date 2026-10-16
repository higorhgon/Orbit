// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// HudSpritePS.hlsl : premultiplied sRGB sprite (button pods) with opacity and the
// TV trims, so the HUD gets the same colour correction as the scene.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Tex     : register(s0);
float4    g_vParams : register(c0);   // x = opacity, y = colour level (linear, like the menu panels)
float4    g_vTrims  : register(c1);   // y = red gain, z = gamma exponent

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float4 t = tex2D( g_Tex, UV );
    float  a = t.a * g_vParams.x;
    float3 lin = SRGBToLinear( t.rgb / max( t.a, 0.001 ) ) * g_vParams.y;
    lin.r *= g_vTrims.y;
    float3 c = pow( LinearToSRGB( saturate( lin ) ), g_vTrims.z );
    return float4( c * a, a );
}
