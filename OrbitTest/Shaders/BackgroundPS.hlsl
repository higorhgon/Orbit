// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// BackgroundPS.hlsl : the prototype's makeBackground() canvas, per pixel.
// Canvas 2D mixes in sRGB, so everything is composed in sRGB and converted to
// linear at the end for the HDR scene target.
//   1. radial gradient #b300ff -> #4a0080 (45%) -> #1a0033, centred on the orb
//   2. horizon haze band in the pod rim colour
//   3. nine faint halo rings
//   4. dark vignette so the purple pools in the middle
//-------------------------------------------------------------------------------------

#include "Common.hlsli"

float4 g_vRes : register(c0);   // x = width, y = height, z = ring line width in pixels

static const float3 CORE  = float3( 179.0,  0.0, 255.0 ) / 255.0;   // #b300ff
static const float3 DEEP  = float3(  74.0,  0.0, 128.0 ) / 255.0;   // #4a0080
static const float3 INK   = float3(  26.0,  0.0,  51.0 ) / 255.0;   // #1a0033
static const float3 EDGE  = float3( 204.0, 51.0, 255.0 ) / 255.0;   // #cc33ff
static const float3 SHADE = float3(   5.0,  0.0,  12.0 ) / 255.0;

float4 main( float2 UV : TEXCOORD0 ) : COLOR
{
    float w = g_vRes.x, h = g_vRes.y;
    float2 p = UV * float2( w, h );
    float2 d = p - float2( w * 0.5, h * 0.46 );
    float  m = max( w, h );
    float  dist = length( d );

    // 1. Base gradient
    float t = saturate( dist / ( m * 0.75 ) );
    float3 col = ( t < 0.45 ) ? lerp( CORE, DEEP, t / 0.45 )
                              : lerp( DEEP, INK, ( t - 0.45 ) / 0.55 );

    // 2. Haze: alpha 0 -> 0.14 -> 0 between 50% and 75% of the height
    float ht = ( p.y - h * 0.5 ) / ( h * 0.25 );
    if( ht >= 0.0 && ht <= 1.0 )
        col = lerp( col, EDGE, 0.14 * ( 1.0 - abs( ht * 2.0 - 1.0 ) ) );

    // 3. Halo rings: ellipses (r, 0.92 r), antialiased strokes
    float q    = length( float2( d.x, d.y / 0.92 ) );
    float grad = length( float2( d.x, d.y / ( 0.92 * 0.92 ) ) ) / max( q, 0.001 );
    [unroll]
    for( int i = 1; i <= 9; i++ )
    {
        float r   = m * 0.07 * i * ( 1.0 + i * 0.06 );
        float sd  = ( q - r ) / max( grad, 0.001 );
        float cov = saturate( g_vRes.z * 0.5 + 0.5 - abs( sd ) );
        col = lerp( col, EDGE, cov * ( 0.16 - i * 0.014 ) );
    }

    // 4. Vignette
    float vt = saturate( ( dist - m * 0.3 ) / ( m * 0.45 ) );
    col = lerp( col, SHADE, 0.25 * vt );

    return float4( SRGBToLinear( col ), 1.0 );
}
