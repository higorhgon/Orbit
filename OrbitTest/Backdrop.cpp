// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Backdrop.cpp : the prototype's scene.background canvas and its starfield.
//
//   background : BackgroundPS on a full-screen quad, no depth
//   stars      : 1400 points on a shell 18..40 out, #a995d6 at 60% opacity,
//                drawn as tiny squares (StarVS) with normal alpha blending,
//                depth tested so the orb hides the ones behind it
//

#include "stdafx.h"
#include "Screen.h"
#include "Post.h"
#include "Backdrop.h"

#include "Shaders\BackgroundPS.h"
#include "Shaders\StarVS.h"
#include "Shaders\SolidPS.h"

static const UINT NUM_STARS = 1400;

struct StarVertex
{
    float Pos[3];
    float Corner[2];
};

static D3DPixelShader*       s_pBackgroundPS = NULL;
static D3DVertexShader*      s_pStarVS       = NULL;
static D3DPixelShader*       s_pStarPS       = NULL;
static D3DVertexDeclaration* s_pStarDecl     = NULL;
static D3DVertexBuffer*      s_pStarVB       = NULL;
static D3DIndexBuffer*       s_pStarIB       = NULL;


//-------------------------------------------------------------------------------------
// Small deterministic random generator (the prototype uses Math.random; a fixed
// seed keeps the sky the same every launch)
//-------------------------------------------------------------------------------------
static DWORD s_dwSeed = 0x0B17A11u;

static float Random01()
{
    s_dwSeed = s_dwSeed * 1664525u + 1013904223u;
    return ( s_dwSeed >> 8 ) * ( 1.0f / 16777216.0f );
}


//-------------------------------------------------------------------------------------
// Name: Backdrop_Init()
//-------------------------------------------------------------------------------------
HRESULT Backdrop_Init( D3DDevice* pDevice )
{
    if( FAILED( pDevice->CreatePixelShader( g_pBackgroundPS, &s_pBackgroundPS ) ) )  return E_FAIL;
    if( FAILED( pDevice->CreateVertexShader( g_pStarVS, &s_pStarVS ) ) )             return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pSolidPS, &s_pStarPS ) ) )             return E_FAIL;

    D3DVERTEXELEMENT9 Elements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( pDevice->CreateVertexDeclaration( Elements, &s_pStarDecl ) ) )
        return E_FAIL;

    if( FAILED( pDevice->CreateVertexBuffer( NUM_STARS * 4 * sizeof(StarVertex), D3DUSAGE_WRITEONLY,
                                             0, D3DPOOL_MANAGED, &s_pStarVB, NULL ) ) )
        return E_FAIL;
    if( FAILED( pDevice->CreateIndexBuffer( NUM_STARS * 6 * sizeof(WORD), D3DUSAGE_WRITEONLY,
                                            D3DFMT_INDEX16, D3DPOOL_MANAGED, &s_pStarIB, NULL ) ) )
        return E_FAIL;

    StarVertex* pVerts;
    WORD*       pIndices;
    if( FAILED( s_pStarVB->Lock( 0, 0, (void**)&pVerts, 0 ) ) )
        return E_FAIL;
    if( FAILED( s_pStarIB->Lock( 0, 0, (void**)&pIndices, 0 ) ) )
    {
        s_pStarVB->Unlock();
        return E_FAIL;
    }

    static const float c_Corners[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
    for( UINT i = 0; i < NUM_STARS; i++ )
    {
        // Same distribution as the prototype: radius 18..40, uniform over the sphere
        float r  = 18.0f + Random01() * 22.0f;
        float th = Random01() * XM_2PI;
        float ph = acosf( 2.0f * Random01() - 1.0f );
        float x = r * sinf( ph ) * cosf( th );
        float y = r * cosf( ph );
        float z = r * sinf( ph ) * sinf( th );

        for( UINT c = 0; c < 4; c++ )
        {
            pVerts->Pos[0] = x; pVerts->Pos[1] = y; pVerts->Pos[2] = z;
            pVerts->Corner[0] = c_Corners[c][0];
            pVerts->Corner[1] = c_Corners[c][1];
            pVerts++;
        }

        WORD b = (WORD)( i * 4 );
        *pIndices++ = b; *pIndices++ = b + 1; *pIndices++ = b + 2;
        *pIndices++ = b; *pIndices++ = b + 2; *pIndices++ = b + 3;
    }

    s_pStarIB->Unlock();
    s_pStarVB->Unlock();
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Backdrop_RenderBackground()
//-------------------------------------------------------------------------------------
void Backdrop_RenderBackground( D3DDevice* pDevice )
{
    const ScreenInfo& s = g_Screen;

    // Background canvas
    float vRes[4] = { (float)s.Width, (float)s.Height, s.Unit, 0.0f };
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pDevice->SetPixelShader( s_pBackgroundPS );
    pDevice->SetPixelShaderConstantF( 0, vRes, 1 );
    Post_DrawQuad( pDevice );
}


//-------------------------------------------------------------------------------------
// Name: Backdrop_RenderStars()
//-------------------------------------------------------------------------------------
void Backdrop_RenderStars( D3DDevice* pDevice, const XMMATRIX& matViewProj )
{
    const ScreenInfo& s = g_Screen;

    // Stars: #a995d6 in linear light, 60% opacity
    float vStar[4]  = { 2.0f / s.Width, 2.0f / s.Height, s.Unit, 0.0f };
    float vColor[4] = { 0.3968f, 0.3005f, 0.6724f, 0.6f };

    pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    pDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    pDevice->SetVertexDeclaration( s_pStarDecl );
    pDevice->SetVertexShader( s_pStarVS );
    pDevice->SetPixelShader( s_pStarPS );
    pDevice->SetVertexShaderConstantF( 0, (const float*)&matViewProj, 4 );
    pDevice->SetVertexShaderConstantF( 4, vStar, 1 );
    pDevice->SetPixelShaderConstantF( 0, vColor, 1 );
    pDevice->SetStreamSource( 0, s_pStarVB, 0, sizeof(StarVertex) );
    pDevice->SetIndices( s_pStarIB );
    pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, NUM_STARS * 4, 0, NUM_STARS * 2 );

    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}
