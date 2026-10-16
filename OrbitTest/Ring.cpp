// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Ring.cpp : the prototype's menu ring.
//
//   ring group  : y = -0.2, rotation.y eases to -index * step
//   panels      : 2.0 x 0.667 planes 0.07 in front of points on radius 3.5;
//                 scale eases to 1.5 (selected) / 1.1. Colour level is 0.8 for all of
//                 them, so the selected one only grows (the prototype's 1.0 / 0.55,
//                 and its full fog, made the back panels too dark to read on a TV).
//                 The front panels draw after bloom, so the orb's glow can't wash
//                 them out. The prototype faces the panels outward, which hides all
//                 but the front one; here each turns to face the camera so all six
//                 can be read around the ring.
//   track       : radius 3.5 +- 0.02 annulus at y = -0.45, #b98aff at 25%
//   all of it   : transparent, no depth write, linear fog #4a0080 from 6 to 13
//

#include "stdafx.h"
#include "Assets.h"
#include "Post.h"
#include "Ring.h"

#include "Shaders\LabelVS.h"
#include "Shaders\LabelPS.h"
#include "Shaders\TrackPS.h"

static const float RADIUS      = 3.5f;
static const float STEP        = XM_2PI / RING_ITEMS;
static const float FOG_NEAR    = 6.0f;
static const float FOG_FAR     = 13.0f;
static const UINT  TRACK_SEGS  = 128;
static const float LEVEL_ACTIVE = 0.8f;    // prototype: 1.0; matched to the others
static const float LEVEL_IDLE   = 0.8f;    // prototype: 0.55, too dark to read on a TV
static const float PANEL_FOG_MAX = 0.5f;   // panels never fog more than halfway, so the back ones read

static const char* c_szLabels[RING_ITEMS] = { "XBOX", "XBOX 360", "ARCADE", "APPS", "FILES", "SETTINGS" };

struct RingVertex
{
    float Pos[3];
    float UV[2];
};

static D3DTexture*           s_pTex[RING_ITEMS];
static D3DVertexShader*      s_pLabelVS   = NULL;
static D3DPixelShader*       s_pLabelPS   = NULL;
static D3DPixelShader*       s_pTrackPS   = NULL;
static D3DVertexDeclaration* s_pDecl      = NULL;
static RingVertex            s_Track[( TRACK_SEGS + 1 ) * 2];

static int   s_nIndex = 0;
static float s_fRot   = 0.0f;
static float s_fScale[RING_ITEMS];
static float s_fLevel[RING_ITEMS];

static BOOL  s_bDepthTest = TRUE;
static float s_fRingScale = 1.0f;   // ring group scale (three.js ring.scale)
static float s_vFogColor[4];
static float s_vTrackColor[4];


static float SRGBToLinear( BYTE c )
{
    float f = c / 255.0f;
    return ( f <= 0.04045f ) ? f / 12.92f : powf( ( f + 0.055f ) / 1.055f, 2.4f );
}

// THREE.MathUtils.damp
static float Damp( float x, float y, float fLambda, float dt )
{
    return x + ( y - x ) * ( 1.0f - expf( -fLambda * dt ) );
}


//-------------------------------------------------------------------------------------
// Name: Ring_Init()
//-------------------------------------------------------------------------------------
HRESULT Ring_Init( D3DDevice* pDevice )
{
    if( FAILED( pDevice->CreateVertexShader( g_pLabelVS, &s_pLabelVS ) ) )  return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pLabelPS, &s_pLabelPS ) ) )   return E_FAIL;
    if( FAILED( pDevice->CreatePixelShader( g_pTrackPS, &s_pTrackPS ) ) )   return E_FAIL;

    D3DVERTEXELEMENT9 Elements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    if( FAILED( pDevice->CreateVertexDeclaration( Elements, &s_pDecl ) ) )
        return E_FAIL;

    // Panel art; a missing file just leaves that panel out
    for( int i = 0; i < RING_ITEMS; i++ )
    {
        char szName[16];
        sprintf_s( szName, "ring%d", i );
        Assets_LoadTexture( pDevice, szName, &s_pTex[i] );
        s_fScale[i] = 1.0f;
        s_fLevel[i] = 1.0f;
    }

    // Track: strip alternating outer / inner edge, flat in XZ at y = -0.45
    for( UINT i = 0; i <= TRACK_SEGS; i++ )
    {
        float a = XM_2PI * i / TRACK_SEGS;
        float s = sinf( a ), c = cosf( a );
        RingVertex& o = s_Track[i * 2];
        RingVertex& n = s_Track[i * 2 + 1];
        o.Pos[0] = s * ( RADIUS + 0.02f ); o.Pos[1] = -0.45f; o.Pos[2] = c * ( RADIUS + 0.02f );
        n.Pos[0] = s * ( RADIUS - 0.02f ); n.Pos[1] = -0.45f; n.Pos[2] = c * ( RADIUS - 0.02f );
        o.UV[0] = o.UV[1] = n.UV[0] = n.UV[1] = 0.0f;
    }

    s_vFogColor[0] = SRGBToLinear( 0x4a ); s_vFogColor[1] = 0.0f; s_vFogColor[2] = SRGBToLinear( 0x80 ); s_vFogColor[3] = 1.0f;
    s_vTrackColor[0] = SRGBToLinear( 0xb9 ); s_vTrackColor[1] = SRGBToLinear( 0x8a ); s_vTrackColor[2] = 1.0f; s_vTrackColor[3] = 1.0f;
    return S_OK;
}


int  Ring_GetIndex() { return s_nIndex; }
void Ring_SetDepthTest( BOOL bEnable ) { s_bDepthTest = bEnable; }
void Ring_SetScale( float fScale ) { s_fRingScale = fScale; }

const char* Ring_GetLabel( int nIndex )
{
    return c_szLabels[ ( nIndex % RING_ITEMS + RING_ITEMS ) % RING_ITEMS ];
}


//-------------------------------------------------------------------------------------
// Name: Ring_Update()
//-------------------------------------------------------------------------------------
void Ring_Update( float dt, int nMove )
{
    if( nMove )
        s_nIndex = ( s_nIndex + nMove + RING_ITEMS ) % RING_ITEMS;

    // Ease the shortest way round to -index * step
    float fTarget = -s_nIndex * STEP;
    float fDiff = atan2f( sinf( fTarget - s_fRot ), cosf( fTarget - s_fRot ) );
    s_fRot += fDiff * ( 1.0f - expf( -6.0f * dt ) );
    s_fRot = fmodf( s_fRot, XM_2PI );

    for( int i = 0; i < RING_ITEMS; i++ )
    {
        BOOL bActive = ( i == s_nIndex );
        s_fScale[i] = Damp( s_fScale[i], bActive ? 1.5f : 1.1f, 8.0f, dt );
        s_fLevel[i] = Damp( s_fLevel[i], bActive ? LEVEL_ACTIVE : LEVEL_IDLE, 8.0f, dt );
    }
}


//-------------------------------------------------------------------------------------
// Name: Ring_Render()
//-------------------------------------------------------------------------------------
struct DrawItem
{
    int      nPanel;   // -1 = track
    float    fDepth;
    XMMATRIX matWorld;
};

void Ring_Render( D3DDevice* pDevice, const XMMATRIX& matView, const XMMATRIX& matProj,
                  const XMVECTOR& vEye, int nPass, float fSplitDepth )
{
    const BOOL bBack    = ( nPass == RING_BACK );
    const BOOL bOverlay = ( nPass == RING_FRONT_PANELS );

    // ring.visible = scale > 0.01
    const float S = s_fRingScale;
    if( S <= 0.01f )
        return;

    // ring group: scale, rotation about y, then up/down (row-vector order: local first)
    XMMATRIX matRing = XMMatrixScaling( S, S, S ) * XMMatrixRotationY( s_fRot ) * XMMatrixTranslation( 0.0f, -0.2f, 0.0f );

    DrawItem Items[RING_ITEMS + 1];
    int nItems = 0;

    for( int i = 0; i <= RING_ITEMS; i++ )
    {
        DrawItem it;
        XMVECTOR vCentre;
        if( i == RING_ITEMS )
        {
            it.nPanel = -1;
            it.matWorld = matRing;
            vCentre = XMVectorSet( 0.0f, -0.45f, 0.0f, 1.0f );
        }
        else
        {
            if( !s_pTex[i] )
                continue;
            // Where the ring puts this panel, then a yaw that faces the camera
            float a = i * STEP + s_fRot;
            float px = sinf( a ) * RADIUS * S, pz = cosf( a ) * RADIUS * S;
            float fYaw = atan2f( XMVectorGetX( vEye ) - px, XMVectorGetZ( vEye ) - pz );
            float fScale = s_fScale[i] * S;
            it.nPanel = i;
            it.matWorld = XMMatrixTranslation( 0.0f, 0.0f, 0.07f )
                        * XMMatrixScaling( fScale, fScale, fScale )
                        * XMMatrixRotationY( fYaw )
                        * XMMatrixTranslation( px, -0.2f, pz );
            vCentre = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
        }

        XMVECTOR vView = XMVector3Transform( vCentre, it.matWorld * matView );
        it.fDepth = -XMVectorGetZ( vView );
        if( bBack ? ( it.fDepth > fSplitDepth ) : ( it.fDepth <= fSplitDepth ) )
        {
            // In front, the track stays in the scene and the panels go on top after bloom
            if( bBack || bOverlay == ( it.nPanel >= 0 ) )
                Items[nItems++] = it;
        }
    }

    // Far to near
    for( int i = 1; i < nItems; i++ )
        for( int j = i; j > 0 && Items[j].fDepth > Items[j - 1].fDepth; j-- )
        {
            DrawItem t = Items[j]; Items[j] = Items[j - 1]; Items[j - 1] = t;
        }

    // The back buffer pass has no depth buffer (nothing it overlaps is nearer)
    pDevice->SetRenderState( D3DRS_ZENABLE, ( s_bDepthTest && !bOverlay ) ? D3DZB_TRUE : D3DZB_FALSE );

    float vOut[4] = { 0.0f, 1.0f, 1.0f, 0.0f };
    if( bOverlay )
    {
        Post_GetTrims( vOut );
        vOut[3] = 1.0f;
    }
    pDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    pDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    pDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    pDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    pDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    pDevice->SetVertexDeclaration( s_pDecl );
    pDevice->SetVertexShader( s_pLabelVS );

    pDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    pDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    for( int n = 0; n < nItems; n++ )
    {
        const DrawItem& it = Items[n];
        XMMATRIX matWV  = it.matWorld * matView;
        XMMATRIX matWVP = matWV * matProj;
        pDevice->SetVertexShaderConstantF( 0, (const float*)&matWVP, 4 );
        pDevice->SetVertexShaderConstantF( 4, (const float*)&matWV, 4 );

        if( it.nPanel < 0 )
        {
            float vParams[4] = { 0.25f, FOG_NEAR, FOG_FAR, 0.0f };
            pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
            pDevice->SetPixelShader( s_pTrackPS );
            pDevice->SetPixelShaderConstantF( 0, vParams, 1 );
            pDevice->SetPixelShaderConstantF( 1, s_vFogColor, 1 );
            pDevice->SetPixelShaderConstantF( 2, s_vTrackColor, 1 );
            pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, TRACK_SEGS * 2, s_Track, sizeof(RingVertex) );
        }
        else
        {
            // PlaneGeometry(2.0, 0.667): top-left, bottom-left, top-right, bottom-right
            static const RingVertex c_Quad[4] =
            {
                { { -1.0f,  0.3335f, 0.0f }, { 0.0f, 0.0f } },
                { { -1.0f, -0.3335f, 0.0f }, { 0.0f, 1.0f } },
                { {  1.0f,  0.3335f, 0.0f }, { 1.0f, 0.0f } },
                { {  1.0f, -0.3335f, 0.0f }, { 1.0f, 1.0f } },
            };
            float vParams[4] = { s_fLevel[it.nPanel], FOG_NEAR, FOG_FAR, PANEL_FOG_MAX };
            pDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );   // FrontSide, as in three.js
            pDevice->SetPixelShader( s_pLabelPS );
            pDevice->SetPixelShaderConstantF( 0, vParams, 1 );
            pDevice->SetPixelShaderConstantF( 1, s_vFogColor, 1 );
            pDevice->SetPixelShaderConstantF( 2, vOut, 1 );
            pDevice->SetTexture( 0, s_pTex[it.nPanel] );
            pDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, c_Quad, sizeof(RingVertex) );
        }
    }

    pDevice->SetTexture( 0, NULL );
    pDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}
