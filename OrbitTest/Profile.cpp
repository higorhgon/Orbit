// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Profile.cpp : see Profile.h. The gamer picture is the large one, 64 x 64, read
// straight into a linear texture the HUD draws inside the badge's frame, cut to the
// frame's shape so its corners don't show past the cut corners.

#include "stdafx.h"
#include "Hud.h"
#include "Profile.h"

static const UINT PICTURE_SIZE = 64;

// The badge frame's shape as fractions of the picture, corners clockwise from the top
// left; the same as PROFILE_CUT in Tools/bake.html (keep them in step)
static const float c_Cut[8][2] =
{
    { 0.20f, 0.0f }, { 0.82f, 0.0f }, { 1.0f, 0.17f }, { 1.0f, 0.80f },
    { 0.82f, 1.0f }, { 0.22f, 1.0f }, { 0.0f, 0.82f }, { 0.0f, 0.20f },
};

// Inside the (convex) shape: on the inner side of every edge
static BOOL InsideCut( float x, float y )
{
    for( int i = 0; i < 8; i++ )
    {
        const float* a = c_Cut[i];
        const float* b = c_Cut[( i + 1 ) % 8];
        if( ( b[0] - a[0] ) * ( y - a[1] ) - ( b[1] - a[1] ) * ( x - a[0] ) < 0.0f )
            return FALSE;
    }
    return TRUE;
}

static HANDLE      s_hNotify  = NULL;
static BOOL        s_bSysUI   = FALSE;   // a system screen is showing
static int         s_nUser    = -1;      // whose badge is shown
static char        s_szName[XUSER_NAME_SIZE] = "";
static D3DTexture* s_pPicture = NULL;
static BOOL        s_bPicUsed = FALSE;   // the HUD has drawn it at least once

int Profile_SignedInUser()
{
    for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
        if( XUserGetSigninState( i ) != eXUserSigninState_NotSignedIn )
            return (int)i;
    return -1;
}

BOOL Profile_IsSystemUIOpen() { return s_bSysUI; }

BOOL Profile_ShowSignIn()
{
    return XShowSigninUI( 1, XSSUI_FLAGS_LOCALSIGNINONLY ) == ERROR_SUCCESS;
}

// Reads the signed-in profile's name and picture and passes them to the HUD
static void Refresh( D3DDevice* pDevice )
{
    s_nUser = Profile_SignedInUser();
    if( s_nUser < 0 || XUserGetName( s_nUser, s_szName, sizeof(s_szName) ) != ERROR_SUCCESS )
    {
        s_nUser = -1;
        s_szName[0] = 0;
        Hud_SetProfile( NULL, NULL );
        return;
    }

    BOOL bPicture = FALSE;
    if( s_pPicture )
    {
        // The GPU may still be drawing last frame's picture
        if( s_bPicUsed )
            pDevice->BlockUntilIdle();
        D3DLOCKED_RECT Locked;
        if( SUCCEEDED( s_pPicture->LockRect( 0, &Locked, NULL, 0 ) ) )
        {
            bPicture = XUserReadGamerPicture( s_nUser, FALSE, (BYTE*)Locked.pBits, Locked.Pitch,
                                              PICTURE_SIZE, NULL ) == ERROR_SUCCESS;
            // A, R, G, B bytes, premultiplied as the HUD expects: opaque inside the
            // frame's shape, clear outside it
            for( UINT y = 0; bPicture && y < PICTURE_SIZE; y++ )
            {
                BYTE* p = (BYTE*)Locked.pBits + y * Locked.Pitch;
                for( UINT x = 0; x < PICTURE_SIZE; x++ )
                {
                    if( InsideCut( ( x + 0.5f ) / PICTURE_SIZE, ( y + 0.5f ) / PICTURE_SIZE ) )
                        p[x * 4] = 0xFF;
                    else
                        *(DWORD*)( p + x * 4 ) = 0;
                }
            }
            s_pPicture->UnlockRect( 0 );
        }
    }
    Hud_SetProfile( s_szName, bPicture ? s_pPicture : NULL );
    s_bPicUsed |= bPicture;
}

HRESULT Profile_Init( D3DDevice* pDevice )
{
    s_hNotify = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( FAILED( pDevice->CreateTexture( PICTURE_SIZE, PICTURE_SIZE, 1, 0, D3DFMT_LIN_A8R8G8B8,
                                        D3DPOOL_MANAGED, &s_pPicture, NULL ) ) )
        s_pPicture = NULL;   // the badge shows the name only
    Refresh( pDevice );
    return S_OK;
}

void Profile_Update( D3DDevice* pDevice )
{
    if( !s_hNotify )
        return;
    DWORD dwId;
    ULONG_PTR Param;
    BOOL bChanged = FALSE;
    while( XNotifyGetNext( s_hNotify, 0, &dwId, &Param ) )
    {
        if( dwId == XN_SYS_UI )
            s_bSysUI = ( Param != 0 );
        else if( dwId == XN_SYS_SIGNINCHANGED )
            bChanged = TRUE;
    }
    if( bChanged )
        Refresh( pDevice );
}
