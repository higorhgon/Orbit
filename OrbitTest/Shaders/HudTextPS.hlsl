// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// HudTextPS.hlsl : bitmap font text. Atlas R = glyph coverage, G = text-shadow glow
// coverage. Glyph sits over its glow; output premultiplied with the TV trims.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Font  : register(s0);
float4    g_vText : register(c0);   // text colour (sRGB) and opacity
float4    g_vGlow : register(c1);   // glow colour (sRGB) and strength (0 = none)
float4    g_vTrims : register(c2);  // x = theme turns, y = red gain, z = gamma exponent

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float4 t = tex2D( g_Font, UV );
    float  ga = t.r;
    float  wa = t.g * g_vGlow.a * ( 1.0 - ga );
    float  a  = ga + wa;
    float3 c  = ( g_vText.rgb * ga + g_vGlow.rgb * wa ) / max( a, 0.001 );
    a *= g_vText.a;
    return float4( TrimSRGB( c, g_vTrims ) * a, a );
}
