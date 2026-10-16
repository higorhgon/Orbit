// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Assets.cpp : file loading for Media\*.tex / *.fnt / *.inf. All values in the files
// are big-endian, which is the 360's native order, so they are read directly.

#include "stdafx.h"
#include <xgraphics.h>
#include "Assets.h"

//-------------------------------------------------------------------------------------
// Name: LoadFile()
// Desc: Reads game:\Media\<name> into a new[] buffer. Caller delete[]s it.
//-------------------------------------------------------------------------------------
static HRESULT LoadFile( const char* szName, BYTE** ppData, DWORD* pdwSize )
{
    char szPath[MAX_PATH];
    sprintf_s( szPath, "game:\\Media\\%s", szName );

    HANDLE hFile = CreateFile( szPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
        return E_FAIL;

    DWORD dwSize = GetFileSize( hFile, NULL );
    if( dwSize == INVALID_FILE_SIZE || dwSize == 0 || dwSize > 64 * 1024 * 1024 )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    BYTE* pData = new BYTE[dwSize];
    DWORD dwRead = 0;
    BOOL bOK = ReadFile( hFile, pData, dwSize, &dwRead, NULL );
    CloseHandle( hFile );
    if( !bOK || dwRead != dwSize )
    {
        delete[] pData;
        return E_FAIL;
    }

    *ppData = pData;
    *pdwSize = dwSize;
    return S_OK;
}

// Bounds-checked big-endian reader
struct Reader
{
    const BYTE* p;
    const BYTE* pEnd;
    BOOL        bBad;

    BOOL Has( DWORD n ) { if( (DWORD)( pEnd - p ) < n ) bBad = TRUE; return !bBad; }
    // memcpy, not pointer casts: fields are not always naturally aligned
    DWORD U32() { DWORD v = 0; if( Has( 4 ) ) { memcpy( &v, p, 4 ); p += 4; } return v; }
    float F32() { float v = 0.0f; if( Has( 4 ) ) { memcpy( &v, p, 4 ); p += 4; } return v; }
    short S16() { short v = 0; if( Has( 2 ) ) { memcpy( &v, p, 2 ); p += 2; } return v; }
    BOOL Magic( const char* sz ) { if( !Has( 4 ) || memcmp( p, sz, 4 ) != 0 ) { bBad = TRUE; return FALSE; } p += 4; return TRUE; }
};


//-------------------------------------------------------------------------------------
// Name: Assets_LoadTexture()
//-------------------------------------------------------------------------------------
HRESULT Assets_LoadTexture( D3DDevice* pDevice, const char* szName, D3DTexture** ppTexture )
{
    *ppTexture = NULL;

    char szFile[64];
    sprintf_s( szFile, "%s.tex", szName );
    BYTE* pData;
    DWORD dwSize;
    if( FAILED( LoadFile( szFile, &pData, &dwSize ) ) )
        return E_FAIL;

    Reader r = { pData, pData + dwSize, FALSE };
    r.Magic( "OTX1" );
    DWORD dwLevels = r.U32();
    const BYTE* pLevels = r.p;
    DWORD w0 = r.U32(), h0 = r.U32();
    if( r.bBad || dwLevels == 0 || dwLevels > 12 || w0 == 0 || h0 == 0 || w0 > 4096 || h0 > 4096 )
    {
        delete[] pData;
        return E_FAIL;
    }

    D3DTexture* pTex = NULL;
    if( FAILED( pDevice->CreateTexture( w0, h0, dwLevels, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_MANAGED, &pTex, NULL ) ) )
    {
        delete[] pData;
        return E_FAIL;
    }

    r.p = pLevels;
    for( DWORD lv = 0; lv < dwLevels && !r.bBad; lv++ )
    {
        DWORD w = r.U32(), h = r.U32();
        if( r.bBad || w != max( 1u, w0 >> lv ) || h != max( 1u, h0 >> lv ) || !r.Has( w * h * 4 ) )
        {
            r.bBad = TRUE;
            break;
        }

        D3DLOCKED_RECT Locked;
        pTex->LockRect( lv, &Locked, NULL, 0 );
        for( DWORD y = 0; y < h; y++ )
            memcpy( (BYTE*)Locked.pBits + y * Locked.Pitch, r.p + y * w * 4, w * 4 );
        pTex->UnlockRect( lv );
        r.p += w * h * 4;
    }

    delete[] pData;
    if( r.bBad )
    {
        pTex->Release();
        return E_FAIL;
    }

    *ppTexture = pTex;
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Assets_LoadSpriteInfo()
//-------------------------------------------------------------------------------------
HRESULT Assets_LoadSpriteInfo( const char* szName, SpriteInfo* pInfo )
{
    char szFile[64];
    sprintf_s( szFile, "%s.inf", szName );
    BYTE* pData;
    DWORD dwSize;
    if( FAILED( LoadFile( szFile, &pData, &dwSize ) ) )
        return E_FAIL;

    Reader r = { pData, pData + dwSize, FALSE };
    r.Magic( "OSP1" );
    pInfo->BoxW = r.F32();
    pInfo->BoxH = r.F32();
    pInfo->Pad  = r.F32();
    BOOL bBad = r.bBad;
    delete[] pData;
    return bBad ? E_FAIL : S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Assets_LoadFont()
//-------------------------------------------------------------------------------------
HRESULT Assets_LoadFont( D3DDevice* pDevice, const char* szName, Font* pFont )
{
    ZeroMemory( pFont, sizeof(Font) );

    char szFile[64];
    sprintf_s( szFile, "font_%s.fnt", szName );
    BYTE* pData;
    DWORD dwSize;
    if( FAILED( LoadFile( szFile, &pData, &dwSize ) ) )
        return E_FAIL;

    Reader r = { pData, pData + dwSize, FALSE };
    r.Magic( "OFN1" );
    pFont->Size   = r.F32();
    pFont->Ascent = r.F32();
    DWORD dwCount = r.U32();
    for( DWORD i = 0; i < dwCount && !r.bBad; i++ )
    {
        DWORD code = r.U32();
        Glyph g;
        g.x  = r.S16(); g.y  = r.S16();
        g.w  = r.S16(); g.h  = r.S16();
        g.ox = r.S16(); g.oy = r.S16();
        g.Advance = r.F32();
        if( !r.bBad && code < 128 )
        {
            pFont->Glyphs[code] = g;
            pFont->bHas[code] = TRUE;
        }
    }
    BOOL bBad = r.bBad;
    delete[] pData;
    if( bBad )
        return E_FAIL;

    char szTex[64];
    sprintf_s( szTex, "font_%s", szName );
    if( FAILED( Assets_LoadTexture( pDevice, szTex, &pFont->pTexture ) ) )
        return E_FAIL;

    XGTEXTURE_DESC Desc;
    XGGetTextureDesc( pFont->pTexture, 0, &Desc );
    pFont->AtlasW = (float)Desc.Width;
    pFont->AtlasH = (float)Desc.Height;
    return S_OK;
}
