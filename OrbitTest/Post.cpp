// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Post.cpp : scene target, bloom and final composite.
//
// Render-to-texture on the 360 means drawing into an EDRAM surface and Resolve()ing
// it into a texture. Every colour surface here sits at EDRAM base 0: they are used one
// at a time and each is resolved before the next is set, the same way the XDK samples
// do it. The scene's depth buffer is our own, placed in EDRAM right after the scene
// surface (as HDRLighting does), so drawing colour can never land on depth values.
//

#include "stdafx.h"
#include <xgraphics.h>
#include "Screen.h"
#include "Post.h"

#include "Shaders\QuadVS.h"
#include "Shaders\BrightPS.h"
#include "Shaders\DownPS.h"
#include "Shaders\BlurPS.h"
#include "Shaders\CompositePS.h"
#include "Shaders\FinalPS.h"

//-------------------------------------------------------------------------------------
// UnrealBloomPass settings from the prototype: new UnrealBloomPass(res, 1.0, 0.45, 0.45)
//-------------------------------------------------------------------------------------
static const UINT  NUM_MIPS        = 5;
static const float BLOOM_RADIUS    = 0.45f;
static const UINT  c_KernelRadius[NUM_MIPS] = { 3, 5, 7, 9, 11 };
static const float c_BloomFactors[NUM_MIPS] = { 1.0f, 0.8f, 0.6f, 0.4f, 0.2f };

//-------------------------------------------------------------------------------------
// A render target and the texture it resolves into
//-------------------------------------------------------------------------------------
struct Target
{
    D3DSurface* pRT;
    D3DTexture* pTex;
    UINT        Width;
    UINT        Height;
};

static Target s_Scene;
static Target s_Bright;
static Target s_Down[NUM_MIPS];      // [0] unused: mip 0 blurs the bright buffer directly
static Target s_BlurH[NUM_MIPS];
static Target s_BlurV[NUM_MIPS];
static Target s_Bloom;

static D3DSurface*           s_pBackBuffer = NULL;
static D3DSurface*           s_pDepth      = NULL;
static D3DVertexDeclaration* s_pQuadDecl   = NULL;
static D3DVertexShader*      s_pQuadVS     = NULL;
static D3DPixelShader*       s_pBrightPS   = NULL;
static D3DPixelShader*       s_pDownPS     = NULL;
static D3DPixelShader*       s_pBlurPS     = NULL;
static D3DPixelShader*       s_pCompositePS = NULL;
static D3DPixelShader*       s_pFinalPS    = NULL;

static float s_vBlurWeights[NUM_MIPS][12];
static float s_vFactors[8];
// TV trims, picked by eye on a TV to match the prototype on a PC monitor (the
// prototype itself is 1.0, 1.0, 1.0): x = bloom strength, y = red gain in linear
// light, z = output gamma exponent (below 1 lifts the mid-tones a TV darkens)
static float s_vTrims[4] = { 0.6f, 2.0f, 0.9f, 0.0f };
static float s_fThemeTurns = 0.0f;   // colour theme: hue turn, 0 = purple (see ThemeSRGB)



//-------------------------------------------------------------------------------------
// Name: CreateTarget()
// Desc: EDRAM surface at base 0 plus the texture it resolves to
//-------------------------------------------------------------------------------------
static HRESULT CreateTarget( D3DDevice* pDevice, UINT w, UINT h, D3DFORMAT RTFormat,
                             D3DFORMAT TexFormat, Target* pTarget )
{
    pTarget->Width  = w;
    pTarget->Height = h;

    if( FAILED( pDevice->CreateTexture( w, h, 1, D3DUSAGE_RENDERTARGET, TexFormat,
                                        D3DPOOL_DEFAULT, &pTarget->pTex, NULL ) ) )
        return E_FAIL;

    D3DSURFACE_PARAMETERS Params;
    ZeroMemory( &Params, sizeof(Params) );
    Params.Base = 0;
    if( FAILED( pDevice->CreateRenderTarget( w, h, RTFormat, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &pTarget->pRT, &Params ) ) )
        return E_FAIL;

    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Post_Init()
//-------------------------------------------------------------------------------------
HRESULT Post_Init( D3DDevice* pDevice )
{
    const UINT w = g_Screen.Width, h = g_Screen.Height;

    if( FAILED( pDevice->CreateVertexShader( g_pQuadVS, &s_pQuadVS ) ) )           return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pBrightPS, &s_pBrightPS ) ) )        return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pDownPS, &s_pDownPS ) ) )            return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pBlurPS, &s_pBlurPS ) ) )            return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pCompositePS, &s_pCompositePS ) ) )  return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pFinalPS, &s_pFinalPS ) ) )          return E_FAIL;

    D3DVERTEXELEMENT9 Elements[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( pDevice->CreateVertexDeclaration( Elements, &s_pQuadDecl ) ) )
        return E_FAIL;

    if( FAILED( pDevice->GetBackBuffer( 0, 0, 0, &s_pBackBuffer ) ) )   return E_FAIL;

    // Scene: 7e3 float in EDRAM, resolved to fp16 (as in HDRLighting)
    if( FAILED( CreateTarget( pDevice, w, h, D3DFMT_A2B10G10R10F_EDRAM, D3DFMT_A16B16G16R16F, &s_Scene ) ) )
        return E_FAIL;

    // Scene depth: after the scene surface's EDRAM tiles, hierarchical Z at its start
    {
        D3DSURFACE_PARAMETERS Params;
        ZeroMemory( &Params, sizeof(Params) );
        Params.Base = XGSurfaceSize( w, h, D3DFMT_A2B10G10R10F_EDRAM, D3DMULTISAMPLE_NONE );
        Params.HierarchicalZBase = 0;
        if( FAILED( pDevice->CreateDepthStencilSurface( w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                                        &s_pDepth, &Params ) ) )
            return E_FAIL;
    }

    // Bloom chain: half res, then halving per mip (Math.round, as three.js does)
    UINT mw = ( w + 1 ) / 2, mh = ( h + 1 ) / 2;
    if( FAILED( CreateTarget( pDevice, mw, mh, D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, &s_Bright ) ) )  return E_FAIL;
    if( FAILED( CreateTarget( pDevice, mw, mh, D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, &s_Bloom ) ) )   return E_FAIL;
    ZeroMemory( &s_Down[0], sizeof(Target) );

    for( UINT i = 0; i < NUM_MIPS; i++ )
    {
        if( i > 0 )
        {
            mw = max( 1u, ( mw + 1 ) / 2 );
            mh = max( 1u, ( mh + 1 ) / 2 );
            if( FAILED( CreateTarget( pDevice, mw, mh, D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, &s_Down[i] ) ) )
                return E_FAIL;
        }
        if( FAILED( CreateTarget( pDevice, mw, mh, D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, &s_BlurH[i] ) ) )  return E_FAIL;
        if( FAILED( CreateTarget( pDevice, mw, mh, D3DFMT_A8R8G8B8, D3DFMT_A8R8G8B8, &s_BlurV[i] ) ) )  return E_FAIL;

        // Gaussian, sigma = kernel radius, normalised over the taps actually taken
        const UINT k = c_KernelRadius[i];
        float fSum = 0.0f;
        for( UINT t = 0; t < 12; t++ )
        {
            float c = 0.0f;
            if( t < k )
                c = 0.39894f * expf( -0.5f * t * t / ( (float)k * k ) ) / k;
            s_vBlurWeights[i][t] = c;
            fSum += ( t == 0 ) ? c : 2.0f * c;
        }
        for( UINT t = 0; t < 12; t++ )
            s_vBlurWeights[i][t] /= fSum;

        // lerpBloomFactor: mix(factor, 1.2 - factor, radius). Strength is applied in
        // FinalPS so it can be trimmed live.
        float f = c_BloomFactors[i];
        s_vFactors[i] = f + ( 1.2f - f - f ) * BLOOM_RADIUS;
    }
    s_vFactors[5] = s_vFactors[6] = s_vFactors[7] = 0.0f;

    // Quads use D3D10-style pixel centres so uv lands on texel centres
    pDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Post_DrawQuad()
//-------------------------------------------------------------------------------------
void Post_DrawQuad( D3DDevice* pDevice )
{
    // RECTLIST: top-left, top-right, bottom-left; the fourth corner is implied
    static const float c_Quad[3][4] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
    };

    pDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    pDevice->SetVertexDeclaration( s_pQuadDecl );
    pDevice->SetVertexShader( s_pQuadVS );
    pDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, c_Quad, sizeof(c_Quad[0]) );
}


//-------------------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------------------
static void SetViewport( D3DDevice* pDevice, UINT w, UINT h )
{
    D3DVIEWPORT9 vp;
    vp.X = 0; vp.Y = 0; vp.Width = w; vp.Height = h;
    vp.MinZ = 0.0f; vp.MaxZ = 1.0f;
    pDevice->SetViewport( &vp );
}

static void SetSampler( D3DDevice* pDevice, DWORD dwSampler, D3DBaseTexture* pTex, BOOL bLinear )
{
    DWORD dwFilter = bLinear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    pDevice->SetTexture( dwSampler, pTex );
    pDevice->SetSamplerState( dwSampler, D3DSAMP_MINFILTER, dwFilter );
    pDevice->SetSamplerState( dwSampler, D3DSAMP_MAGFILTER, dwFilter );
    pDevice->SetSamplerState( dwSampler, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    pDevice->SetSamplerState( dwSampler, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( dwSampler, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
}

static void ClearSamplers( D3DDevice* pDevice )
{
    for( DWORD i = 0; i < NUM_MIPS; i++ )
        pDevice->SetTexture( i, NULL );
}

// Draws the current pixel shader into dst and resolves it to dst's texture
static void RunPass( D3DDevice* pDevice, const Target& dst )
{
    pDevice->SetRenderTarget( 0, dst.pRT );
    SetViewport( pDevice, dst.Width, dst.Height );
    Post_DrawQuad( pDevice );
    ClearSamplers( pDevice );
    pDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, dst.pTex, NULL, 0, 0, NULL, 0.0f, 0, NULL );
}

static void SetTexel( D3DDevice* pDevice, const Target& src )
{
    float v[4] = { 1.0f / src.Width, 1.0f / src.Height, 0.0f, 0.0f };
    pDevice->SetPixelShaderConstantF( 0, v, 1 );
}


//-------------------------------------------------------------------------------------
// Name: Post_GetTrims()
//-------------------------------------------------------------------------------------
void Post_GetTrims( float vTrims[4] )
{
    for( int i = 0; i < 4; i++ )
        vTrims[i] = s_vTrims[i];
    vTrims[0] = s_fThemeTurns;   // the overlays have no bloom; x carries the theme instead
}


//-------------------------------------------------------------------------------------
// Name: Post_SetTheme()
//-------------------------------------------------------------------------------------
void Post_SetTheme( float fHueDegrees, BOOL bFixed )
{
    // A fixed hue rides 10 turns up, which ThemeSRGB reads as "set, don't add"
    s_fThemeTurns = fHueDegrees / 360.0f + ( bFixed ? 10.0f : 0.0f );
}


//-------------------------------------------------------------------------------------
// Name: Post_BeginScene()
//-------------------------------------------------------------------------------------
void Post_BeginScene( D3DDevice* pDevice )
{
    pDevice->SetRenderTarget( 0, s_Scene.pRT );
    pDevice->SetDepthStencilSurface( s_pDepth );
    SetViewport( pDevice, s_Scene.Width, s_Scene.Height );
    pDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, 1.0f, 0L );
}


//-------------------------------------------------------------------------------------
// Name: Post_EndFrame()
//-------------------------------------------------------------------------------------
void Post_EndFrame( D3DDevice* pDevice )
{
    pDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, s_Scene.pTex, NULL, 0, 0, NULL, 0.0f, 0, NULL );

    // No depth in the post passes
    pDevice->SetDepthStencilSurface( NULL );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // High pass into the half-res bright buffer
    pDevice->SetPixelShader( s_pBrightPS );
    SetSampler( pDevice, 0, s_Scene.pTex, FALSE );
    SetTexel( pDevice, s_Scene );
    RunPass( pDevice, s_Bright );

    // Mips: (downsample), horizontal blur, vertical blur
    const Target* pInput = &s_Bright;
    for( UINT i = 0; i < NUM_MIPS; i++ )
    {
        if( i > 0 )
        {
            pDevice->SetPixelShader( s_pDownPS );
            SetSampler( pDevice, 0, pInput->pTex, FALSE );
            SetTexel( pDevice, *pInput );
            RunPass( pDevice, s_Down[i] );
            pInput = &s_Down[i];
        }

        pDevice->SetPixelShader( s_pBlurPS );
        pDevice->SetPixelShaderConstantF( 1, s_vBlurWeights[i], 3 );

        float vStepX[4] = { 1.0f / s_BlurH[i].Width, 0.0f, 0.0f, 0.0f };
        pDevice->SetPixelShaderConstantF( 0, vStepX, 1 );
        SetSampler( pDevice, 0, pInput->pTex, FALSE );
        RunPass( pDevice, s_BlurH[i] );

        float vStepY[4] = { 0.0f, 1.0f / s_BlurV[i].Height, 0.0f, 0.0f };
        pDevice->SetPixelShaderConstantF( 0, vStepY, 1 );
        SetSampler( pDevice, 0, s_BlurH[i].pTex, FALSE );
        RunPass( pDevice, s_BlurV[i] );

        pInput = &s_BlurV[i];
    }

    // Composite the mips at half res
    pDevice->SetPixelShader( s_pCompositePS );
    for( UINT i = 0; i < NUM_MIPS; i++ )
        SetSampler( pDevice, i, s_BlurV[i].pTex, TRUE );
    pDevice->SetPixelShaderConstantF( 0, s_vFactors, 2 );
    RunPass( pDevice, s_Bloom );

    // Scene + bloom to the back buffer
    pDevice->SetRenderTarget( 0, s_pBackBuffer );
    SetViewport( pDevice, g_Screen.Width, g_Screen.Height );
    pDevice->SetPixelShader( s_pFinalPS );
    pDevice->SetPixelShaderConstantF( 0, s_vTrims, 1 );
    float vTheme[4] = { s_fThemeTurns, 0.0f, 0.0f, 0.0f };
    pDevice->SetPixelShaderConstantF( 1, vTheme, 1 );
    SetSampler( pDevice, 0, s_Scene.pTex, FALSE );
    SetSampler( pDevice, 1, s_Bloom.pTex, TRUE );
    Post_DrawQuad( pDevice );
    ClearSamplers( pDevice );
}


//-------------------------------------------------------------------------------------
// Name: Post_SetSceneDepth()
//-------------------------------------------------------------------------------------
void Post_SetSceneDepth( D3DDevice* pDevice, BOOL bBind )
{
    pDevice->SetDepthStencilSurface( bBind ? s_pDepth : NULL );
}
