// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// QuadVS.hlsl : full-screen RECTLIST pass-through (clip-space position, uv).
//-------------------------------------------------------------------------------------

struct VS_OUT
{
    float4 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT main( float2 Pos : POSITION, float2 UV : TEXCOORD0 )
{
    VS_OUT Out;
    Out.Pos = float4( Pos, 0.0, 1.0 );
    Out.UV  = UV;
    return Out;
}
