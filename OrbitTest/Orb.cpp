// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Orb.cpp : the purple noise orb, ported from the prototype's coreMesh + nucleus.
//
//   shell   : icosphere (three.js IcosahedronGeometry(1, 18)), scale 1.5, noise
//             displaced in OrbVS, fresnel colour in OrbPS, additive, no depth write
//   nucleus : icosphere (IcosahedronGeometry(1, 6)), scale ~1.2, solid #2a0058,
//             drawn first so the shell glows on top of it
//
// Both draw into the linear scene target (see Post.cpp), so colours are linear.
//

#include "stdafx.h"
#include "Orb.h"

#include "Shaders\OrbVS.h"
#include "Shaders\OrbPS.h"
#include "Shaders\SolidVS.h"
#include "Shaders\SolidPS.h"

//-------------------------------------------------------------------------------------
// Mesh
//-------------------------------------------------------------------------------------
struct Mesh
{
    D3DVertexBuffer* pVB;
    D3DIndexBuffer*  pIB;
    UINT             NumVerts;
    UINT             NumTris;
};

static Mesh                  s_Shell;
static Mesh                  s_Nucleus;
static D3DVertexDeclaration* s_pDecl      = NULL;
static D3DVertexShader*      s_pOrbVS     = NULL;
static D3DPixelShader*       s_pOrbPS     = NULL;
static D3DVertexShader*      s_pSolidVS   = NULL;
static D3DPixelShader*       s_pSolidPS   = NULL;
static float                 s_vShellColor[4];   // #b98aff, linear


//-------------------------------------------------------------------------------------
// Name: SRGBToLinear()
// Desc: three.js converts hex colours to linear before they reach a shader
//-------------------------------------------------------------------------------------
static float SRGBToLinear( BYTE c )
{
    float f = c / 255.0f;
    return ( f <= 0.04045f ) ? f / 12.92f : powf( ( f + 0.055f ) / 1.055f, 2.4f );
}


//-------------------------------------------------------------------------------------
// Name: CreateIcosphere()
// Desc: Unit sphere from an icosahedron whose 20 faces are each cut into an
//       nSeg x nSeg triangle grid and pushed out to the sphere, like three.js
//       PolyhedronGeometry with detail = nSeg - 1. Triangles wind counter-
//       clockwise seen from outside (three.js front faces).
//-------------------------------------------------------------------------------------
static HRESULT CreateIcosphere( D3DDevice* pDevice, UINT nSeg, Mesh* pMesh )
{
    const float t = ( 1.0f + sqrtf( 5.0f ) ) / 2.0f;
    const float c_Corners[12][3] =
    {
        { -1,  t,  0 }, {  1,  t,  0 }, { -1, -t,  0 }, {  1, -t,  0 },
        {  0, -1,  t }, {  0,  1,  t }, {  0, -1, -t }, {  0,  1, -t },
        {  t,  0, -1 }, {  t,  0,  1 }, { -t,  0, -1 }, { -t,  0,  1 },
    };
    static const WORD c_Faces[20][3] =
    {
        { 0, 11,  5 }, { 0,  5,  1 }, { 0,  1,  7 }, { 0,  7, 10 }, { 0, 10, 11 },
        { 1,  5,  9 }, { 5, 11,  4 }, { 11, 10, 2 }, { 10, 7,  6 }, { 7,  1,  8 },
        { 3,  9,  4 }, { 3,  4,  2 }, { 3,  2,  6 }, { 3,  6,  8 }, { 3,  8,  9 },
        { 4,  9,  5 }, { 2,  4, 11 }, { 6,  2, 10 }, { 8,  6,  7 }, { 9,  8,  1 },
    };

    const UINT nVertsPerFace = ( nSeg + 1 ) * ( nSeg + 2 ) / 2;
    pMesh->NumVerts = 20 * nVertsPerFace;
    pMesh->NumTris  = 20 * nSeg * nSeg;
    if( pMesh->NumVerts > 0xFFFF )
        return E_FAIL;

    if( FAILED( pDevice->CreateVertexBuffer( pMesh->NumVerts * 3 * sizeof(float), D3DUSAGE_WRITEONLY,
                                             0, D3DPOOL_MANAGED, &pMesh->pVB, NULL ) ) )
        return E_FAIL;
    if( FAILED( pDevice->CreateIndexBuffer( pMesh->NumTris * 3 * sizeof(WORD), D3DUSAGE_WRITEONLY,
                                            D3DFMT_INDEX16, D3DPOOL_MANAGED, &pMesh->pIB, NULL ) ) )
        return E_FAIL;

    float* pVerts;
    WORD*  pIndices;
    if( FAILED( pMesh->pVB->Lock( 0, 0, (void**)&pVerts, 0 ) ) )
        return E_FAIL;
    if( FAILED( pMesh->pIB->Lock( 0, 0, (void**)&pIndices, 0 ) ) )
    {
        pMesh->pVB->Unlock();
        return E_FAIL;
    }

    for( UINT f = 0; f < 20; f++ )
    {
        XMVECTOR a = XMVectorSet( c_Corners[c_Faces[f][0]][0], c_Corners[c_Faces[f][0]][1], c_Corners[c_Faces[f][0]][2], 0 );
        XMVECTOR b = XMVectorSet( c_Corners[c_Faces[f][1]][0], c_Corners[c_Faces[f][1]][1], c_Corners[c_Faces[f][1]][2], 0 );
        XMVECTOR c = XMVectorSet( c_Corners[c_Faces[f][2]][0], c_Corners[c_Faces[f][2]][1], c_Corners[c_Faces[f][2]][2], 0 );
        const UINT nBase = f * nVertsPerFace;

        // Grid point (i, j) = a + (b - a) * i/n + (c - a) * j/n, for i + j <= n
        for( UINT i = 0; i <= nSeg; i++ )
        {
            for( UINT j = 0; j <= nSeg - i; j++ )
            {
                XMVECTOR p = a + ( b - a ) * ( (float)i / nSeg ) + ( c - a ) * ( (float)j / nSeg );
                p = XMVector3Normalize( p );
                *pVerts++ = XMVectorGetX( p );
                *pVerts++ = XMVectorGetY( p );
                *pVerts++ = XMVectorGetZ( p );
            }
        }

        // Row i starts at i*(n+1) - i*(i-1)/2 within the face
        #define GRID( i, j ) (WORD)( nBase + (i) * ( nSeg + 1 ) - (i) * ( (int)(i) - 1 ) / 2 + (j) )

        // Wind every face's triangles the same way as the face itself, then flip
        // the whole face if it points inward
        XMVECTOR vFaceN = XMVector3Cross( b - a, c - a );
        BOOL bFlip = XMVectorGetX( XMVector3Dot( vFaceN, a + b + c ) ) < 0.0f;

        for( UINT i = 0; i < nSeg; i++ )
        {
            for( UINT j = 0; j < nSeg - i; j++ )
            {
                WORD i0 = GRID( i, j ), i1 = GRID( i + 1, j ), i2 = GRID( i, j + 1 );
                if( bFlip ) { WORD tmp = i1; i1 = i2; i2 = tmp; }
                *pIndices++ = i0; *pIndices++ = i1; *pIndices++ = i2;

                if( j + 1 < nSeg - i )
                {
                    WORD k0 = GRID( i + 1, j ), k1 = GRID( i + 1, j + 1 ), k2 = GRID( i, j + 1 );
                    if( bFlip ) { WORD tmp = k1; k1 = k2; k2 = tmp; }
                    *pIndices++ = k0; *pIndices++ = k1; *pIndices++ = k2;
                }
            }
        }
        #undef GRID
    }

    pMesh->pIB->Unlock();
    pMesh->pVB->Unlock();
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Orb_Init()
//-------------------------------------------------------------------------------------
HRESULT Orb_Init( D3DDevice* pDevice )
{
    ZeroMemory( &s_Shell, sizeof(s_Shell) );
    ZeroMemory( &s_Nucleus, sizeof(s_Nucleus) );

    // Shaders are compiled at build time by fxc (see the pre-build step)
    if( FAILED( pDevice->CreateVertexShader( g_pOrbVS, &s_pOrbVS ) ) )       return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pOrbPS, &s_pOrbPS ) ) )        return E_FAIL;
    if( FAILED( pDevice->CreateVertexShader( g_pSolidVS, &s_pSolidVS ) ) )   return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pSolidPS, &s_pSolidPS ) ) )    return E_FAIL;

    D3DVERTEXELEMENT9 Elements[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };
    if( FAILED( pDevice->CreateVertexDeclaration( Elements, &s_pDecl ) ) )
        return E_FAIL;

    if( FAILED( CreateIcosphere( pDevice, 19, &s_Shell ) ) )   return E_FAIL;
    if( FAILED( CreateIcosphere( pDevice, 7, &s_Nucleus ) ) )  return E_FAIL;

    s_vShellColor[0] = SRGBToLinear( 0xb9 );
    s_vShellColor[1] = SRGBToLinear( 0x8a );
    s_vShellColor[2] = SRGBToLinear( 0xff );
    s_vShellColor[3] = 1.0f;
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: DrawMesh()
//-------------------------------------------------------------------------------------
static void DrawMesh( D3DDevice* pDevice, const Mesh& mesh )
{
    pDevice->SetStreamSource( 0, mesh.pVB, 0, 3 * sizeof(float) );
    pDevice->SetIndices( mesh.pIB );
    pDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, 0, mesh.NumVerts, 0, mesh.NumTris );
}


//-------------------------------------------------------------------------------------
// Name: SetCommonStates()
//-------------------------------------------------------------------------------------
static void SetCommonStates( D3DDevice* pDevice, const XMMATRIX& matViewProj )
{
    pDevice->SetVertexDeclaration( s_pDecl );
    pDevice->SetVertexShaderConstantF( 0, (const float*)&matViewProj, 4 );

    pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    pDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    // Front faces are counter-clockwise (right-handed, like three.js), so cull clockwise
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
}


//-------------------------------------------------------------------------------------
// Name: Orb_RenderNucleus()
//-------------------------------------------------------------------------------------
void Orb_RenderNucleus( D3DDevice* pDevice, const XMMATRIX& matViewProj, float fTime, float fPulse, float fY )
{
    SetCommonStates( pDevice, matViewProj );

    // Nucleus: breathes slowly and swells on select
    float fNucleusScale = 1.2f + sinf( fTime * 1.3f ) * 0.03f + fPulse * 0.08f;
    float vNucleusWorld[4] = { 0.0f, fY, 0.0f, fNucleusScale };
    float vNucleusColor[4] = { SRGBToLinear( 0x2a ), 0.0f, SRGBToLinear( 0x58 ), 1.0f };

    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pDevice->SetVertexShader( s_pSolidVS );
    pDevice->SetPixelShader( s_pSolidPS );
    pDevice->SetVertexShaderConstantF( 4, vNucleusWorld, 1 );
    pDevice->SetPixelShaderConstantF( 0, vNucleusColor, 1 );
    DrawMesh( pDevice, s_Nucleus );
}


//-------------------------------------------------------------------------------------
// Name: Orb_RenderShell()
//-------------------------------------------------------------------------------------
void Orb_RenderShell( D3DDevice* pDevice, const XMMATRIX& matViewProj, const XMVECTOR& vEye,
                      float fTime, float fPulse, float fY )
{
    SetCommonStates( pDevice, matViewProj );

    // Shell: glow added over the nucleus; alpha darkens it in the noise troughs (see OrbPS)
    float vShellWorld[4] = { 0.0f, fY, 0.0f, 1.5f };
    float vParams[4]     = { fTime, fPulse, 0.0f, 0.0f };
    float vEye4[4]       = { XMVectorGetX( vEye ), XMVectorGetY( vEye ), XMVectorGetZ( vEye ), 1.0f };
    float vPSParams[4]   = { fPulse, 0.0f, 0.0f, 0.0f };

    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );
    pDevice->SetVertexShader( s_pOrbVS );
    pDevice->SetPixelShader( s_pOrbPS );
    pDevice->SetVertexShaderConstantF( 4, vShellWorld, 1 );
    pDevice->SetVertexShaderConstantF( 5, vParams, 1 );
    pDevice->SetVertexShaderConstantF( 6, vEye4, 1 );
    pDevice->SetPixelShaderConstantF( 0, s_vShellColor, 1 );
    pDevice->SetPixelShaderConstantF( 1, vPSParams, 1 );
    DrawMesh( pDevice, s_Shell );

    // Leave states clean for whatever draws next
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
}
