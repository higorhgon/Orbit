// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// LabelPS.hlsl : a menu panel (MeshBasicMaterial with an sRGB canvas texture,
// transparent, fog). The texture is premultiplied sRGB; colour is taken back to
// straight linear, dimmed by material.color, fogged, and output premultiplied
// for SRCBLEND = ONE, DESTBLEND = INVSRCALPHA.
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

sampler2D g_Tex     : register(s0);
float4    g_vParams : register(c0);   // x = colour level, y = fog near, z = fog far, w = most fog allowed
float4    g_vFog    : register(c1);   // fog colour, linear
float4    g_vOut    : register(c2);   // x = theme turns, y = red gain, z = gamma exponent, w = 1 when drawing
                                      // straight to the back buffer (after bloom), 0 into the scene

float4 main( float2 UV : TEXCOORD0, float Depth : TEXCOORD1 ) : COLOR
{
    float4 t = tex2D( g_Tex, UV );
    float  a = t.a;
    float3 c = SRGBToLinear( t.rgb / max( a, 0.001 ) ) * g_vParams.x;
    c = lerp( c, g_vFog.rgb, smoothstep( g_vParams.y, g_vParams.z, Depth ) * g_vParams.w );

    // On the back buffer: the same TV trims and sRGB encode FinalPS gives the scene
    if( g_vOut.w > 0.5 )
    {
        c.r *= g_vOut.y;
        c = ThemeSRGB( pow( LinearToSRGB( saturate( c ) ), g_vOut.z ), g_vOut.x );
    }
    return float4( c * a, a );
}
