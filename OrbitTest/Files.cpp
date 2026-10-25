// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Files.cpp : see Files.h. Layout follows the prototype's CSS in --u units:
//
//   .win    600u x 330u (shrunk to the safe width on 4:3), 5u frame, 24u title bar
//   .devs   118u column: per drive a 100u x 44u block (Orbitron name 10u, a 10 step
//           meter, free space in 8u), the selected one on a lit pod (.dev.on)
//   .list   rows 16u tall every 18u: type 8u (34u column), name 10.5u, size right
//           aligned; the selected row on .row.on with "> " before the name
//   status  one line under the list: delete confirm, clipboard, row count
//

#include "stdafx.h"
#include <ctype.h>
#include <xgraphics.h>
#include "Screen.h"
#include "Assets.h"
#include "Hud.h"
#include "Drives.h"
#include "Profile.h"
#include "Files.h"

static const int MAX_ENTRIES = 1024;
static const float ART_U = 1.5f;

static BOOL      s_bOpen   = FALSE;
static int       s_nDrive  = 0;                       // index into Drives_Get
static char      s_szPath[MAX_DRIVES][MAX_PATH];      // current folder per drive
static DirEntry  s_Entries[MAX_ENTRIES];
static int       s_nEntries = 0;
static BOOL      s_bListOK  = FALSE;
static int       s_nRow     = 0;                      // 0 = ".." when not at the root
static int       s_nTop     = 0;                      // first visible row
static int       s_nVisible = 12;                     // rows that fit; set while drawing

static char      s_szClip[MAX_PATH] = "";             // clipboard: full path
static BOOL      s_bConfirm = FALSE;                  // X pressed once on s_szConfirm
static char      s_szConfirm[MAX_PATH];
static BOOL      s_bJobSeen = FALSE;                  // a job we started is running

// Typing a name (rename / new folder) on the system keyboard, after the sign-in screen
// if the keyboard wants a profile. The list takes no input until it's done.
enum { TYPE_NONE, TYPE_SIGNIN, TYPE_KEYBOARD };
enum { NAME_RENAME, NAME_NEW_FOLDER };
static const int   MAX_NAME = 128;                    // a list row's name (DirEntry::szName)
static int         s_nTyping   = TYPE_NONE;
static int         s_nNameFor  = NAME_RENAME;
static char        s_szRenaming[MAX_PATH];            // rename: the item's full path
static BOOL        s_bSawSignIn = FALSE;              // the sign-in screen has come up
static float       s_fSignInWait = 0.0f;              // seconds since asking for it
static WCHAR       s_wszDefault[MAX_NAME];
static WCHAR       s_wszTyped[MAX_NAME];
static XOVERLAPPED s_Keyboard;

static D3DTexture* s_pWin   = NULL;
static SpriteInfo  s_WinInfo;
static D3DTexture* s_pRowOn = NULL;
static D3DTexture* s_pDevOn = NULL;
static D3DTexture* s_pSegOn = NULL;
static D3DTexture* s_pSegOff = NULL;
static D3DTexture* s_pDim   = NULL;

// Colours (sRGB) from the prototype's CSS
static const HudColor C_ION      = { 0xb9 / 255.0f, 0x8a / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_BAR      = { 0xd9 / 255.0f, 0xb8 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_DIM      = { 0xa9 / 255.0f, 0x8f / 255.0f, 0xd6 / 255.0f, 1.0f };
static const HudColor C_ROW      = { 0xd9 / 255.0f, 0xc8 / 255.0f, 0xf5 / 255.0f, 1.0f };
static const HudColor C_LILAC    = { 0xf2 / 255.0f, 0xc8 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_WHITE    = { 1.0f, 1.0f, 1.0f, 1.0f };
static const HudColor C_DEV_ON   = { 0xfb / 255.0f, 0xef / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_WARN     = { 0xff / 255.0f, 0x9d / 255.0f, 0xe0 / 255.0f, 1.0f };
static const HudColor G_POD      = { 0xdf / 255.0f, 0x80 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor G_NONE     = { 0.0f, 0.0f, 0.0f, 0.0f };


//-------------------------------------------------------------------------------------
// Helpers
//-------------------------------------------------------------------------------------
static BOOL AtRoot()
{
    const char* p = strchr( s_szPath[s_nDrive], ':' );
    return !p || p[1] == 0 || ( p[1] == '\\' && p[2] == 0 );
}

static int RowCount() { return s_nEntries + ( AtRoot() ? 0 : 1 ); }

// The entry a row shows, or NULL for ".."
static const DirEntry* RowEntry( int nRow )
{
    int i = AtRoot() ? nRow : nRow - 1;
    return ( i >= 0 && i < s_nEntries ) ? &s_Entries[i] : NULL;
}

static void RowPath( int nRow, char* szOut, size_t nOut )
{
    const DirEntry* e = RowEntry( nRow );
    sprintf_s( szOut, nOut, "%s%s", s_szPath[s_nDrive], e ? e->szName : "" );
}

static void FormatSize( ULONGLONG q, char* sz, size_t n )
{
    if( q < 1024 )                          sprintf_s( sz, n, "%u B", (UINT)q );
    else if( q < 1024 * 1024 )              sprintf_s( sz, n, "%u KB", (UINT)( q / 1024 ) );
    else if( q < 1024ULL * 1024 * 1024 )    sprintf_s( sz, n, "%.1f MB", q / ( 1024.0 * 1024.0 ) );
    else                                    sprintf_s( sz, n, "%.2f GB", q / ( 1024.0 * 1024.0 * 1024.0 ) );
}

static void Upper( char* sz )
{
    for( ; *sz; sz++ )
        *sz = (char)toupper( (unsigned char)*sz );
}

static const char* BaseName( const char* szPath )
{
    size_t n = strlen( szPath );
    while( n && szPath[n - 1] == '\\' ) n--;
    const char* p = szPath + n;
    while( p > szPath && p[-1] != '\\' && p[-1] != ':' ) p--;
    return p;
}

static void Reload()
{
    s_bListOK = Drives_List( s_szPath[s_nDrive], s_Entries, MAX_ENTRIES, &s_nEntries );
    if( !s_bListOK )
        s_nEntries = 0;
    s_nRow = max( 0, min( s_nRow, RowCount() - 1 ) );
}

static void Toast( const char* szText, float fSeconds = 1.6f )
{
    char sz[128];
    strcpy_s( sz, szText );
    Upper( sz );
    Hud_ShowToast( sz, fSeconds );
}

static void UpdateKeys()
{
    JobStatus Job;
    Drives_GetJob( &Job );
    if( Job.nState == JOBSTATE_RUNNING )
    {
        static const HudKey c_Busy[] = { { HUD_POD_B, "CANCEL" } };
        Hud_SetKeys( c_Busy, ARRAYSIZE( c_Busy ) );
        return;
    }
    static const HudKey c_Keys[] =
    {
        { HUD_POD_NONE, "\x01 \x02 CHANGE DRIVES" }, { HUD_POD_NONE, "\x04 \x05 NAVIGATE DRIVE" },
        { HUD_POD_A, "OPEN" }, { HUD_POD_X, "DELETE" }, { HUD_POD_Y, "COPY" },
        { HUD_POD_LB, "RENAME" }, { HUD_POD_LT, "NEW FOLDER" }, { HUD_POD_B, "BACK" },
    };
    static const HudKey c_KeysPaste[] =
    {
        { HUD_POD_NONE, "\x01 \x02 CHANGE DRIVES" }, { HUD_POD_NONE, "\x04 \x05 NAVIGATE DRIVE" },
        { HUD_POD_A, "OPEN" }, { HUD_POD_X, "DELETE" }, { HUD_POD_Y, "COPY" }, { HUD_POD_RB, "PASTE" },
        { HUD_POD_LB, "RENAME" }, { HUD_POD_LT, "NEW FOLDER" }, { HUD_POD_B, "BACK" },
    };
    if( s_szClip[0] ) Hud_SetKeys( c_KeysPaste, ARRAYSIZE( c_KeysPaste ) );
    else              Hud_SetKeys( c_Keys, ARRAYSIZE( c_Keys ) );
}

// Puts the selection on the row named szName, if it's there
static void SelectName( const char* szName )
{
    for( int r = 0; r < RowCount(); r++ )
    {
        const DirEntry* re = RowEntry( r );
        if( re && _stricmp( re->szName, szName ) == 0 ) { s_nRow = r; break; }
    }
}


//-------------------------------------------------------------------------------------
// Typing a name
//-------------------------------------------------------------------------------------
static BOOL OpenKeyboard()
{
    int nUser = Profile_SignedInUser();
    ZeroMemory( &s_Keyboard, sizeof(s_Keyboard) );
    s_wszTyped[0] = 0;
    DWORD dw = XShowKeyboardUI( nUser >= 0 ? nUser : 0, VKBD_LATIN_FULL, s_wszDefault,
                                s_nNameFor == NAME_RENAME ? L"Rename" : L"New Folder",
                                s_nNameFor == NAME_RENAME ? L"Type the new name." : L"Type a name for the new folder.",
                                s_wszTyped, MAX_NAME, &s_Keyboard );
    if( dw != ERROR_IO_PENDING )
        return FALSE;
    s_nTyping = TYPE_KEYBOARD;
    return TRUE;
}

// Opens the keyboard starting from szDefault; if it wants a profile and nobody is
// signed in, the sign-in screen first
static void StartTyping( int nNameFor, const char* szDefault )
{
    s_nNameFor = nNameFor;
    int i = 0;
    for( ; szDefault[i] && i < MAX_NAME - 1; i++ )
        s_wszDefault[i] = (WCHAR)(unsigned char)szDefault[i];
    s_wszDefault[i] = 0;

    if( OpenKeyboard() )
        return;
    if( Profile_SignedInUser() < 0 && Profile_ShowSignIn() )
    {
        s_nTyping = TYPE_SIGNIN;
        s_bSawSignIn = FALSE;
        s_fSignInWait = 0.0f;
        return;
    }
    Toast( "COULDN'T OPEN THE KEYBOARD", 2.2f );
}

static void Cancelled()
{
    Toast( s_nNameFor == NAME_RENAME ? "RENAME CANCELLED" : "NEW FOLDER CANCELLED" );
}

// The keyboard's text: plain characters only, spaces trimmed from both ends
static BOOL TypedName( char* szOut, size_t nOut )
{
    const WCHAR* p = s_wszTyped;
    while( *p == L' ' ) p++;
    size_t n = 0;
    for( ; *p && n + 1 < nOut; p++ )
    {
        if( *p < 0x20 || *p > 0x7E )
            return FALSE;
        szOut[n++] = (char)*p;
    }
    while( n && szOut[n - 1] == ' ' ) n--;
    szOut[n] = 0;
    return TRUE;
}

// After the keyboard: rename the item or make the folder
static void FinishTyping()
{
    char szName[MAX_NAME], szWhy[64], sz[192];
    if( !TypedName( szName, sizeof(szName) ) )
    {
        Toast( "NAME HAS A CHARACTER DRIVES DON'T ALLOW", 2.2f );
        return;
    }

    if( s_nNameFor == NAME_RENAME )
    {
        char szNew[MAX_PATH];
        if( !Drives_Rename( s_szRenaming, szName, szNew, sizeof(szNew), szWhy, sizeof(szWhy) ) )
        {
            sprintf_s( sz, "CAN'T RENAME: %s", szWhy );
            Toast( sz, 2.2f );
            return;
        }
        // The clipboard follows the item (or anything inside a renamed folder)
        size_t n = strlen( s_szRenaming );
        if( _strnicmp( s_szClip, s_szRenaming, n ) == 0 && ( s_szClip[n] == 0 || s_szClip[n] == '\\' ) )
        {
            char szClip[MAX_PATH];
            sprintf_s( szClip, "%s%s", szNew, s_szClip + n );
            strcpy_s( s_szClip, szClip );
        }
        sprintf_s( sz, "RENAMED TO %s", szName );
    }
    else
    {
        if( !Drives_NewFolder( s_szPath[s_nDrive], szName, szWhy, sizeof(szWhy) ) )
        {
            sprintf_s( sz, "CAN'T MAKE FOLDER: %s", szWhy );
            Toast( sz, 2.2f );
            return;
        }
        sprintf_s( sz, "MADE FOLDER %s", szName );
    }
    Toast( sz, 2.2f );
    Reload();
    SelectName( szName );
}

// While typing: waits for the sign-in screen, then the keyboard
static void UpdateTyping( float dt )
{
    if( s_nTyping == TYPE_SIGNIN )
    {
        BOOL bUI = Profile_IsSystemUIOpen();
        s_bSawSignIn |= bUI;
        s_fSignInWait += dt;
        if( bUI )
            return;
        if( Profile_SignedInUser() >= 0 )
        {
            s_nTyping = TYPE_NONE;
            if( !OpenKeyboard() )
                Toast( "COULDN'T OPEN THE KEYBOARD", 2.2f );
        }
        else if( s_bSawSignIn || s_fSignInWait > 3.0f )
        {
            // Backed out of the sign-in screen (or it never came up)
            s_nTyping = TYPE_NONE;
            Cancelled();
        }
        return;
    }

    if( !XHasOverlappedIoCompleted( &s_Keyboard ) )
        return;
    s_nTyping = TYPE_NONE;
    if( XGetOverlappedResult( &s_Keyboard, NULL, FALSE ) == ERROR_SUCCESS )
        FinishTyping();
    else
        Cancelled();
}


//-------------------------------------------------------------------------------------
// Name: Files_Init()
//-------------------------------------------------------------------------------------
HRESULT Files_Init( D3DDevice* pDevice )
{
    Drives_Init();
    for( int i = 0; i < MAX_DRIVES; i++ )
        strcpy_s( s_szPath[i], Drives_Get( i ).szRoot );

    if( FAILED( Assets_LoadSpriteInfo( "files_win", &s_WinInfo ) ) || FAILED( Assets_LoadTexture( pDevice, "files_win", &s_pWin ) ) )
        s_pWin = NULL;
    Assets_LoadTexture( pDevice, "row_on", &s_pRowOn );
    Assets_LoadTexture( pDevice, "dev_on", &s_pDevOn );
    Assets_LoadTexture( pDevice, "seg_on", &s_pSegOn );
    Assets_LoadTexture( pDevice, "seg_off", &s_pSegOff );
    Assets_LoadTexture( pDevice, "dim", &s_pDim );
    return S_OK;
}


//-------------------------------------------------------------------------------------
// Name: Files_Open() / Files_IsOpen()
//-------------------------------------------------------------------------------------
void Files_Open()
{
    s_bOpen = TRUE;
    s_bConfirm = FALSE;
    // Start on the first drive that's ready
    if( !Drives_Get( s_nDrive ).bReady )
        for( int i = 0; i < MAX_DRIVES; i++ )
            if( Drives_Get( i ).bReady ) { s_nDrive = i; break; }
    Reload();
    Hud_SetMenuName( "FILES" );
    UpdateKeys();
}

BOOL Files_IsOpen() { return s_bOpen; }

static void Close()
{
    s_bOpen = FALSE;
    Hud_SetMenuName( NULL );
    Hud_SetMainKeys();
}


//-------------------------------------------------------------------------------------
// Name: Files_Update()
//-------------------------------------------------------------------------------------
void Files_Update( float dt, int nMoveX, int nMoveY, WORD wPressed )
{
    if( !s_bOpen )
        return;

    // The sign-in screen or keyboard is up: nothing else until it's done
    if( s_nTyping != TYPE_NONE )
    {
        UpdateTyping( dt );
        return;
    }

    // A background job: watch it finish; B cancels
    JobStatus Job;
    Drives_GetJob( &Job );
    if( Job.nState == JOBSTATE_RUNNING )
    {
        if( wPressed & XINPUT_GAMEPAD_B )
            Drives_CancelJob();
        return;
    }
    if( s_bJobSeen )
    {
        s_bJobSeen = FALSE;
        char sz[128];
        const char* szVerb = ( Job.nType == JOB_COPY ) ? "PASTED" : "DELETED";
        if( Job.nState == JOBSTATE_DONE )           sprintf_s( sz, "%s %s", szVerb, Job.szName );
        else if( Job.nState == JOBSTATE_CANCELLED ) sprintf_s( sz, Job.nType == JOB_COPY ? "PASTE CANCELLED" : "DELETE STOPPED" );
        else                                        sprintf_s( sz, "FAILED: %s", Job.szError );
        Toast( sz, 2.2f );
        // A deleted clipboard item can't be pasted any more
        if( Job.nType == JOB_DELETE && Job.nState != JOBSTATE_FAILED && s_szClip[0] &&
            GetFileAttributesA( s_szClip ) == INVALID_FILE_ATTRIBUTES )
            s_szClip[0] = 0;
        Drives_ClearJob();
        Drives_Refresh();
        Reload();
        UpdateKeys();
    }

    // Anything but a second X cancels a pending delete
    if( s_bConfirm && ( nMoveX || nMoveY || ( wPressed & ~XINPUT_GAMEPAD_X ) ) )
    {
        s_bConfirm = FALSE;
        if( wPressed & XINPUT_GAMEPAD_B )
            return;   // B just cancels the delete
    }

    if( nMoveX )
    {
        // Next drive that's there (the disc drive always counts). The list is as of the
        // last refresh (SETTINGS > REFRESH DRIVES), so switching never spins up the disc.
        for( int n = 0; n < MAX_DRIVES; n++ )
        {
            s_nDrive = ( s_nDrive + nMoveX + MAX_DRIVES ) % MAX_DRIVES;
            const DriveInfo& d = Drives_Get( s_nDrive );
            if( d.bReady || strcmp( d.szName, "DVD" ) == 0 )
                break;
        }
        s_nRow = 0;
        Reload();
        return;
    }
    if( nMoveY )
    {
        int n = RowCount();
        if( n )
            s_nRow = ( s_nRow + nMoveY + n ) % n;
    }

    const DirEntry* e = RowEntry( s_nRow );
    BOOL bUpRow = !AtRoot() && s_nRow == 0;
    char szPath[MAX_PATH];

    if( wPressed & XINPUT_GAMEPAD_A )
    {
        if( bUpRow )
        {
            wPressed = XINPUT_GAMEPAD_B;   // same as going back
        }
        else if( e && e->bDir )
        {
            char szNew[MAX_PATH];
            sprintf_s( szNew, "%s%s\\", s_szPath[s_nDrive], e->szName );
            strcpy_s( s_szPath[s_nDrive], szNew );
            s_nRow = 0;
            Reload();
        }
        else if( e )
        {
            const char* ext = strrchr( e->szName, '.' );
            if( ext && ( _stricmp( ext, ".xex" ) == 0 || _stricmp( ext, ".xbe" ) == 0 ) )
            {
                RowPath( s_nRow, szPath, sizeof(szPath) );
                XLaunchNewImage( szPath, 0 );   // only returns if the launch failed
                Toast( "COULDN'T LAUNCH THAT" );
            }
            else
            {
                Toast( "ORBIT CAN'T OPEN THIS KIND OF FILE YET" );
            }
        }
    }

    if( wPressed & XINPUT_GAMEPAD_B )
    {
        if( AtRoot() )
        {
            Close();
            return;
        }
        // Up one folder, landing on the folder we came out of
        char* szCur = s_szPath[s_nDrive];
        size_t n = strlen( szCur );
        if( n && szCur[n - 1] == '\\' ) szCur[--n] = 0;
        char szLeft[128];
        strcpy_s( szLeft, BaseName( szCur ) );
        char* p = strrchr( szCur, '\\' );
        if( p ) p[1] = 0;
        Reload();
        SelectName( szLeft );
    }

    if( ( wPressed & XINPUT_GAMEPAD_Y ) && e )
    {
        RowPath( s_nRow, s_szClip, sizeof(s_szClip) );
        char sz[160];
        sprintf_s( sz, "COPIED %s", e->szName );
        Toast( sz );
        UpdateKeys();
    }

    if( ( wPressed & XINPUT_GAMEPAD_RIGHT_SHOULDER ) && s_szClip[0] )
    {
        char szWhy[64];
        if( Drives_StartCopy( s_szClip, s_szPath[s_nDrive], szWhy, sizeof(szWhy) ) )
        {
            s_bJobSeen = TRUE;
            UpdateKeys();
        }
        else
        {
            char sz[128];
            sprintf_s( sz, "CAN'T PASTE: %s", szWhy );
            Toast( sz, 2.2f );
        }
    }

    if( ( wPressed & XINPUT_GAMEPAD_X ) && e )
    {
        RowPath( s_nRow, szPath, sizeof(szPath) );
        if( Drives_IsProtected( szPath ) )
        {
            Toast( "PROTECTED - ORBIT WON'T DELETE THIS", 2.2f );
            s_bConfirm = FALSE;
        }
        else if( !s_bConfirm || _stricmp( s_szConfirm, szPath ) != 0 )
        {
            s_bConfirm = TRUE;
            strcpy_s( s_szConfirm, szPath );
        }
        else
        {
            s_bConfirm = FALSE;
            char szWhy[64];
            if( Drives_StartDelete( szPath, szWhy, sizeof(szWhy) ) )
            {
                s_bJobSeen = TRUE;
                UpdateKeys();
            }
            else
            {
                char sz[128];
                sprintf_s( sz, "CAN'T DELETE: %s", szWhy );
                Toast( sz, 2.2f );
            }
        }
    }

    if( ( wPressed & XINPUT_GAMEPAD_LEFT_SHOULDER ) && e )
    {
        RowPath( s_nRow, szPath, sizeof(szPath) );
        if( Drives_IsProtected( szPath ) )
        {
            Toast( "PROTECTED - ORBIT WON'T RENAME THIS", 2.2f );
        }
        else
        {
            strcpy_s( s_szRenaming, szPath );
            StartTyping( NAME_RENAME, e->szName );
        }
    }

    if( wPressed & PAD_LEFT_TRIGGER )
        StartTyping( NAME_NEW_FOLDER, "New Folder" );

    // Keep the selection on screen
    if( s_nRow < s_nTop ) s_nTop = s_nRow;
    if( s_nRow >= s_nTop + s_nVisible ) s_nTop = s_nRow - s_nVisible + 1;
    s_nTop = max( 0, min( s_nTop, max( 0, RowCount() - s_nVisible ) ) );
}


//-------------------------------------------------------------------------------------
// Drawing
//-------------------------------------------------------------------------------------
// Text cut to fit fMaxW, with "..." when it doesn't
static void Fit( char* sz, size_t n, const char* szIn, int nFont, float fPx, float fSp, float fMaxW )
{
    strcpy_s( sz, n, szIn );
    if( Hud_TextWidth( nFont, sz, fPx, fSp ) <= fMaxW )
        return;
    size_t len = strlen( sz );
    while( len > 0 )
    {
        sz[--len] = 0;
        char szTry[160];
        sprintf_s( szTry, "%s...", sz );
        if( Hud_TextWidth( nFont, szTry, fPx, fSp ) <= fMaxW )
        {
            strcpy_s( sz, n, szTry );
            return;
        }
    }
}

static void Meter( D3DDevice* pDevice, float x, float y, float w, float h, int nSegs, float fFill )
{
    const float gap = max( 1.0f, w * 0.012f );
    const float sw = ( w - gap * ( nSegs - 1 ) ) / nSegs;
    int nOn = (int)( fFill * nSegs + 0.5f );
    for( int i = 0; i < nSegs; i++ )
        Hud_DrawSprite( pDevice, i < nOn ? s_pSegOn : s_pSegOff, x + i * ( sw + gap ), y, sw, h, 1.0f );
}

void Files_Render( D3DDevice* pDevice )
{
    if( !s_bOpen || !s_pWin )
        return;

    const ScreenInfo& scr = g_Screen;
    const float u0 = scr.Unit;
    const float fSafeW = (float)( scr.SafeRect.right - scr.SafeRect.left );
    const float k = min( 1.0f, fSafeW / ( 600.0f * u0 ) );
    const float u = u0 * k;                 // one CSS px of the window, in back buffer pixels
    const float s = u / ART_U;              // art scale

    const float ww = 600.0f * u, wh = 330.0f * u;
    // Centred, nudged up 12u to leave the toast room above the bottom bar
    const float wx = floorf( scr.Width * 0.5f - ww * 0.5f ), wy = floorf( scr.Height * 0.5f - wh * 0.5f - 12.0f * u );
    const float px = wx + 5.0f * u, py = wy + 5.0f * u, pw = ww - 10.0f * u, ph = wh - 10.0f * u;
    const float fBarH = 24.0f * u, fBody = py + fBarH + 2.0f * u;

    Hud_BeginDraw( pDevice );

    // Dim the menu, then the window
    Hud_DrawSprite( pDevice, s_pDim, 0.0f, 0.0f, (float)scr.Width, (float)scr.Height, 1.0f );
    {
        XGTEXTURE_DESC Desc;
        XGGetTextureDesc( s_pWin, 0, &Desc );
        Hud_DrawSprite( pDevice, s_pWin, wx - s_WinInfo.Pad * s, wy - s_WinInfo.Pad * s, Desc.Width * s, Desc.Height * s, 1.0f );
    }

    char sz[192];
    const float f9 = 9.0f * u;

    // Path, right of the title bar
    {
        char szUp[MAX_PATH];
        strcpy_s( szUp, s_szPath[s_nDrive] );
        Upper( szUp );
        Fit( sz, sizeof(sz), szUp, HUD_FONT_MONO, f9, f9 * 0.2f, pw * 0.55f );
        float w = Hud_TextWidth( HUD_FONT_MONO, sz, f9, f9 * 0.2f );
        Hud_DrawText( pDevice, HUD_FONT_MONO, sz, px + pw - 14.0f * u - w, py + fBarH * 0.5f + f9 * 0.35f, f9, f9 * 0.2f, C_BAR, G_NONE );
    }

    // Drives
    {
        float y = fBody + 9.0f * u;
        const float x = px + 9.0f * u, bw = 100.0f * u, bh = 44.0f * u;
        for( int i = 0; i < MAX_DRIVES; i++ )
        {
            const DriveInfo& d = Drives_Get( i );
            // Show drives that are there, and the disc drive even when empty
            if( !d.bReady && strcmp( d.szName, "DVD" ) != 0 && i != s_nDrive )
                continue;
            BOOL bOn = ( i == s_nDrive );
            if( bOn )
                Hud_DrawSprite( pDevice, s_pDevOn, x, y, bw, bh, 1.0f );
            const float f10 = 10.0f * u;
            Hud_DrawText( pDevice, HUD_FONT_TITLE, d.szName, x + 8.0f * u, y + 6.0f * u + Hud_FontAscent( HUD_FONT_TITLE, f10 ),
                          f10, f10 * 0.1f, bOn ? C_DEV_ON : C_DIM, bOn ? G_POD : G_NONE );
            float fUsed = ( d.bReady && d.qwTotal ) ? 1.0f - (float)( (double)d.qwFree / (double)d.qwTotal ) : 0.0f;
            Meter( pDevice, x + 8.0f * u, y + 21.0f * u, bw - 16.0f * u, 5.0f * u, 10, fUsed );
            if( d.bReady )
            {
                char szFree[32];
                FormatSize( d.qwFree, szFree, sizeof(szFree) );
                sprintf_s( sz, "%s FREE", szFree );
            }
            else
            {
                strcpy_s( sz, strcmp( d.szName, "DVD" ) == 0 ? "NO DISC" : "NOT FOUND" );
            }
            const float f8 = 8.0f * u;
            Hud_DrawText( pDevice, HUD_FONT_MONO, sz, x + 8.0f * u, y + 37.0f * u, f8, 0.0f, bOn ? C_DEV_ON : C_DIM, G_NONE );
            y += bh + 6.0f * u;
        }
    }

    // List area
    const float lx = px + 118.0f * u + 1.0f * u + 6.0f * u;
    const float lw = px + pw - 6.0f * u - lx;
    const float ly = fBody + 6.0f * u;
    const float fStatusBase = py + ph - 10.0f * u;
    const float fPitch = 18.0f * u, fRowH = 16.0f * u;
    s_nVisible = max( 1, (int)( ( fStatusBase - 14.0f * u - ly ) / fPitch ) );

    JobStatus Job;
    Drives_GetJob( &Job );
    if( Job.nState == JOBSTATE_RUNNING )
    {
        // Progress: what, a 40 step bar, how far
        float cy = ly + ( fStatusBase - ly ) * 0.35f;
        const float f10 = 10.0f * u;
        char szName[160];
        sprintf_s( sz, "%s %s", Job.nType == JOB_COPY ? "PASTING" : "DELETING", Job.szName );
        Upper( sz );
        Fit( szName, sizeof(szName), sz, HUD_FONT_MONO, f10, f10 * 0.15f, lw - 20.0f * u );
        Hud_DrawText( pDevice, HUD_FONT_MONO, szName, lx + 10.0f * u, cy, f10, f10 * 0.15f, C_WHITE, G_POD );

        float fFrac = Job.qwTotal ? (float)( (double)Job.qwDone / (double)Job.qwTotal ) : 0.0f;
        Meter( pDevice, lx + 10.0f * u, cy + 12.0f * u, lw - 20.0f * u, 8.0f * u, 40, fFrac );

        if( Job.qwTotal == 0 )
            strcpy_s( sz, "MEASURING..." );
        else if( Job.nType == JOB_COPY )
        {
            char a[32], b[32];
            FormatSize( Job.qwDone, a, sizeof(a) );
            FormatSize( Job.qwTotal, b, sizeof(b) );
            sprintf_s( sz, "%d%%   %s / %s", (int)( fFrac * 100.0f ), a, b );
        }
        else
            sprintf_s( sz, "%d%%   %u / %u ITEMS", (int)( fFrac * 100.0f ), (UINT)Job.qwDone, (UINT)Job.qwTotal );
        Hud_DrawText( pDevice, HUD_FONT_MONO, sz, lx + 10.0f * u, cy + 36.0f * u, f9, f9 * 0.2f, C_BAR, G_NONE );
    }
    else
    {
        const float fRowPx = 10.5f * u, f8 = 8.0f * u;
        const float fTypeX = lx + 10.0f * u, fNameX = fTypeX + 34.0f * u + 8.0f * u;
        const float fSizeR = lx + lw - 10.0f * u;
        const float fNameW = fSizeR - 54.0f * u - 8.0f * u - fNameX;

        int nRows = RowCount();
        if( !s_bListOK )
        {
            const DriveInfo& d = Drives_Get( s_nDrive );
            Hud_DrawText( pDevice, HUD_FONT_MONO, d.bReady ? "CAN'T READ THIS FOLDER" : ( strcmp( d.szName, "DVD" ) == 0 ? "NO DISC" : "DRIVE NOT FOUND" ),
                          fTypeX, ly + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, C_DIM, G_NONE );
        }
        else if( nRows == 0 )
        {
            Hud_DrawText( pDevice, HUD_FONT_MONO, "EMPTY", fTypeX, ly + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, C_DIM, G_NONE );
        }

        for( int r = s_nTop; r < nRows && r < s_nTop + s_nVisible; r++ )
        {
            const float y = ly + ( r - s_nTop ) * fPitch;
            const float fBase = y + fRowH * 0.5f + fRowPx * 0.35f;
            BOOL bOn = ( r == s_nRow );
            if( bOn )
                Hud_DrawSprite( pDevice, s_pRowOn, lx, y, lw, fRowH, 1.0f );

            const DirEntry* e = RowEntry( r );
            const char* szType = !e ? "UP" : ( e->bDir ? "DIR" : "FILE" );
            Hud_DrawText( pDevice, HUD_FONT_MONO, szType, fTypeX, y + fRowH * 0.5f + f8 * 0.35f, f8, f8 * 0.15f, bOn ? C_LILAC : C_DIM, G_NONE );

            char szName[160];
            sprintf_s( szName, "%s%s", bOn ? "> " : "  ", e ? e->szName : ".." );
            Fit( sz, sizeof(sz), szName, HUD_FONT_MONO, fRowPx, 0.0f, fNameW );
            Hud_DrawText( pDevice, HUD_FONT_MONO, sz, fNameX, fBase, fRowPx, 0.0f, bOn ? C_WHITE : C_ROW, bOn ? G_POD : G_NONE );

            if( e && !e->bDir )
            {
                FormatSize( e->qwSize, sz, sizeof(sz) );
                float w = Hud_TextWidth( HUD_FONT_MONO, sz, fRowPx, 0.0f );
                Hud_DrawText( pDevice, HUD_FONT_MONO, sz, fSizeR - w, fBase, fRowPx, 0.0f, bOn ? C_LILAC : C_DIM, G_NONE );
            }
        }

        // Status line: delete confirm, or the clipboard; row count on the right
        char szLeft[192];
        szLeft[0] = 0;
        const HudColor* pColor = &C_DIM;
        if( s_bConfirm )
        {
            sprintf_s( sz, "PRESS X AGAIN TO DELETE %s", BaseName( s_szConfirm ) );
            Upper( sz );
            Fit( szLeft, sizeof(szLeft), sz, HUD_FONT_MONO, f9, f9 * 0.1f, lw * 0.8f );
            pColor = &C_WARN;
        }
        else if( s_szClip[0] )
        {
            sprintf_s( sz, "CLIPBOARD: %s", s_szClip );
            Upper( sz );
            Fit( szLeft, sizeof(szLeft), sz, HUD_FONT_MONO, f9, f9 * 0.1f, lw * 0.75f );
        }
        if( szLeft[0] )
            Hud_DrawText( pDevice, HUD_FONT_MONO, szLeft, lx + 10.0f * u, fStatusBase, f9, f9 * 0.1f, *pColor, G_NONE );
        if( nRows )
        {
            sprintf_s( sz, "%d / %d", s_nRow + 1, nRows );
            float w = Hud_TextWidth( HUD_FONT_MONO, sz, f9, f9 * 0.1f );
            Hud_DrawText( pDevice, HUD_FONT_MONO, sz, lx + lw - 10.0f * u - w, fStatusBase, f9, f9 * 0.1f, C_DIM, G_NONE );
        }
    }

    Hud_EndDraw( pDevice );
}
