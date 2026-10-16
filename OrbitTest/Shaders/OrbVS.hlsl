// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// OrbVS.hlsl : the glowing shell. Unit icosphere in, displaced along its normal by
// two octaves of simplex noise (prototype coreMat vertex shader).
//-------------------------------------------------------------------------------------

#include "Noise.hlsli"

float4x4 g_matViewProj : register(c0);
float4   g_vWorld      : register(c4);   // xyz = position, w = uniform scale
float4   g_vParams     : register(c5);   // x = uTime, y = uPulse
float4   g_vEye        : register(c6);   // camera position, world space

struct VS_OUT
{
    float4 Pos    : POSITION;
    float3 Normal : TEXCOORD0;   // world space (no rotation, uniform scale)
    float3 View   : TEXCOORD1;   // surface to eye
    float  Disp   : TEXCOORD2;
};

VS_OUT main( float3 Pos : POSITION )
{
    float t = g_vParams.x;
    float n = snoise( Pos * 1.2 + t * 0.35 ) * 0.22 + snoise( Pos * 3.5 - t * 0.6 ) * 0.06;
    n *= 1.0 + g_vParams.y * 1.5;

    // On a unit sphere the normal is the position
    float3 vWorld = ( Pos + Pos * n ) * g_vWorld.w + g_vWorld.xyz;

    VS_OUT Out;
    Out.Pos    = mul( g_matViewProj, float4( vWorld, 1.0 ) );
    Out.Normal = Pos;
    Out.View   = normalize( g_vEye.xyz - vWorld );
    Out.Disp   = n;
    return Out;
}
