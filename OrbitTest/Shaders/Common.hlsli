// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// Common.hlsli : colour space and bloom storage helpers shared by the post shaders.
//-------------------------------------------------------------------------------------

// Exact sRGB curve, the one three.js uses for hex colours and its OutputPass
float3 SRGBToLinear( float3 c )
{
    float3 lo = c / 12.92;
    float3 hi = pow( ( c + 0.055 ) / 1.055, 2.4 );
    return lerp( lo, hi, step( 0.04045, c ) );
}

float3 LinearToSRGB( float3 c )
{
    float3 lo = c * 12.92;
    float3 hi = 1.055 * pow( max( c, 0.000001 ), 1.0 / 2.4 ) - 0.055;
    return lerp( lo, hi, step( 0.0031308, c ) );
}

// Bloom buffers are 8-bit (filterable), so linear light 0..BLOOM_RANGE is stored
// square-rooted to keep precision in the dim tails of the glow
static const float BLOOM_RANGE = 4.0;

float4 BloomEncode( float3 v )
{
    return float4( sqrt( saturate( v / BLOOM_RANGE ) ), 1.0 );
}

float3 BloomDecode( float3 s )
{
    return s * s * BLOOM_RANGE;
}

// Colour theme (SETTINGS > THEME): turns every hue by fTurns of a full circle, keeping
// saturation and lightness (HSL), on the final sRGB colour after the TV trims. Purple
// is 0, so the default look is left untouched. fTurns of 10 or more means a fixed
// hue (10 + hue / 360, see Post_SetTheme): every colour takes that hue instead.
//
// Written without if / else on purpose: an earlier version with an if-chain per
// channel came out of the Xbox compiler snapping every colour to pure yellow or cyan.
// This is the usual branch-free form (hue / chroma / lightness, after Ian Taylor), and
// gives the same result as the HSL maths in the theme mockup.
float3 ThemeSRGB( float3 c, float fTurns )
{
    // Hue, chroma (max - min) and lightness
    float4 P = ( c.g < c.b ) ? float4( c.b, c.g, -1.0, 2.0 / 3.0 ) : float4( c.g, c.b, 0.0, -1.0 / 3.0 );
    float4 Q = ( c.r < P.x ) ? float4( P.x, P.y, P.w, c.r ) : float4( c.r, P.y, P.z, P.x );
    float  C = Q.x - min( Q.w, Q.y );
    float  H = abs( ( Q.w - Q.y ) / ( 6.0 * C + 1e-10 ) + Q.z );
    float  L = Q.x - C * 0.5;

    // Turned, or set outright
    H = frac( ( fTurns >= 10.0 ) ? fTurns - 10.0 : H + fTurns );

    // Back to RGB with the same chroma and lightness
    float3 rgb = saturate( float3( abs( H * 6.0 - 3.0 ) - 1.0,
                                   2.0 - abs( H * 6.0 - 2.0 ),
                                   2.0 - abs( H * 6.0 - 4.0 ) ) );
    float3 o = ( rgb - 0.5 ) * C + L;
    return ( fTurns == 0.0 || C < 0.0001 ) ? c : o;
}

// TV trims for colours that are already sRGB (the HUD): red gain in linear light,
// then the output gamma exponent, then the theme. Same correction FinalPS applies to
// the scene. vTrims: x = theme turns, y = red gain, z = gamma exponent
float3 TrimSRGB( float3 s, float4 vTrims )
{
    float3 lin = SRGBToLinear( s );
    lin.r *= vTrims.y;
    return ThemeSRGB( pow( LinearToSRGB( saturate( lin ) ), vTrims.z ), vTrims.x );
}
