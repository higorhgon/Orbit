// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

//-------------------------------------------------------------------------------------
// HudVS.hlsl : screen-space HUD rects, positions in back buffer pixels.
//-------------------------------------------------------------------------------------

float4 g_vScreen : register(c0);   // x = 2 / width, y = 2 / height

struct VS_OUT
{
    float4 Pos : POSITION;
    float2 UV  : TEXCOORD0;
};

VS_OUT main( float2 Pos : POSITION, float2 UV : TEXCOORD0 )
{
    VS_OUT Out;
    Out.Pos = float4( Pos.x * g_vScreen.x - 1.0, 1.0 - Pos.y * g_vScreen.y, 0.0, 1.0 );
    Out.UV  = UV;
    return Out;
}
