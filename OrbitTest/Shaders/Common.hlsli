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

// TV trims for colours that are already sRGB (the HUD): red gain in linear light,
// then the output gamma exponent. Same correction FinalPS applies to the scene.
// vTrims: y = red gain, z = gamma exponent
float3 TrimSRGB( float3 s, float4 vTrims )
{
    float3 lin = SRGBToLinear( s );
    lin.r *= vTrims.y;
    return pow( LinearToSRGB( saturate( lin ) ), vTrims.z );
}
