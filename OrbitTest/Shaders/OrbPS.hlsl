// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// OrbPS.hlsl : shell colour (prototype coreMat fragment shader): fresnel rim, hot
// facing core on pulse, thinned where it faces the camera.
//
// The prototype blends additively (SRC_ALPHA, ONE) into a linear half-float target,
// so troughs in the noise go negative and darken what is behind. Our linear 7e3
// target can't take negative colour, so we blend SRCBLEND = ONE, DESTBLEND = SRCALPHA:
//   rgb   = the positive part, added on top
//   alpha = how much of the backdrop survives, standing in for the negative part
//-------------------------------------------------------------------------------------

float4 g_vColor  : register(c0);   // uColor, linear
float4 g_vParams : register(c1);   // x = uPulse

struct PS_IN
{
    float3 Normal : TEXCOORD0;
    float3 View   : TEXCOORD1;
    float  Disp   : TEXCOORD2;
};

// Linear blue of the nucleus (#2a0058) the shell mostly sits over
static const float NUCLEUS_BLUE = 0.0976;

float4 main( PS_IN In ) : COLOR
{
    float pulse = g_vParams.x;
    float ndv   = dot( In.Normal, In.View );

    float rim  = pow( 1.0 - max( ndv, 0.0 ), 2.2 );
    float core = 0.25 + In.Disp * 1.8;
    float face = pow( max( ndv, 0.0 ), 3.0 );

    float3 col = g_vColor.rgb * ( core + rim * 1.6 + pulse * 0.8 )
               + float3( 0.45, 0.12, 0.85 ) * face * pulse * 0.6;
    col *= lerp( 1.0, 0.3, face );

    float3 lin = col * ( 0.55 + rim * 0.45 );
    float  fKeep = saturate( 1.0 + min( lin.b, 0.0 ) / NUCLEUS_BLUE );

    return float4( max( lin, 0.0 ), fKeep );
}
