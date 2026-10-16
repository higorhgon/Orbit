// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// LabelVS.hlsl : menu ring pieces (panel planes and the track). Passes view depth
// for three.js linear fog.
//-------------------------------------------------------------------------------------

float4x4 g_matWVP : register(c0);
float4x4 g_matWV  : register(c4);

struct VS_OUT
{
    float4 Pos   : POSITION;
    float2 UV    : TEXCOORD0;
    float  Depth : TEXCOORD1;   // -mvPosition.z
};

VS_OUT main( float3 Pos : POSITION, float2 UV : TEXCOORD0 )
{
    VS_OUT Out;
    Out.Pos   = mul( g_matWVP, float4( Pos, 1.0 ) );
    Out.UV    = UV;
    Out.Depth = -mul( g_matWV, float4( Pos, 1.0 ) ).z;
    return Out;
}
