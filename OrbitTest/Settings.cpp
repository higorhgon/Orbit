// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Settings.cpp : see Settings.h. Drawn in the file manager's window (600u x 330u,
// "ORBIT // SETTINGS", no drive column) with its rows: section headings in Orbitron,
// settings in Share Tech Mono with their value right aligned, the selected one on
// the lit row (.row.on). Adding a path swaps the list for a folder picker.

#include "stdafx.h"
#include <ctype.h>
#include <xgraphics.h>
#include "Screen.h"
#include "Assets.h"
#include "Hud.h"
#include "Drives.h"
#include "Scan.h"
#include "Settings.h"

static const float ART_U = 1.5f;
static const int   MAX_PICK = 512;

enum { LINE_HEADER, LINE_PATH, LINE_ADD, LINE_SCAN, LINE_CLEAR, LINE_REFRESH };
struct Line
{
    int         nKind;
    int         nPath;      // LINE_PATH: which path
    const char* szText;     // LINE_HEADER
};

static BOOL  s_bOpen    = FALSE;
static int   s_nSel     = 0;      // index into the selectable lines
static int   s_nTop     = 0;      // first visible line
static int   s_nVisible = 14;
static BOOL  s_bConfirm = FALSE;  // X pressed once on a path row
static int   s_nConfirm = -1;
static BOOL  s_bClearConfirm = FALSE;   // A pressed once on DELETE ALL PATHS

// Folder picker for "+ ADD PATH"
static BOOL     s_bPicking  = FALSE;
static int      s_nPickDrive = -1;            // -1 = choosing a drive
static char     s_szPick[MAX_PATH];
static DirEntry s_PickEntries[MAX_PICK];
static int      s_nPickEntries = 0;
static int      s_nPickRows[MAX_DRIVES];      // drive list: which drive each row is
static int      s_nPickRow  = 0;
static int      s_nPickTop  = 0;

static BOOL     s_bWasScanning = FALSE;

static D3DTexture* s_pWin   = NULL;
static SpriteInfo  s_WinInfo;
static D3DTexture* s_pRowOn = NULL;
static D3DTexture* s_pDim   = NULL;

static const HudColor C_BAR   = { 0xd9 / 255.0f, 0xb8 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_DIM   = { 0xa9 / 255.0f, 0x8f / 255.0f, 0xd6 / 255.0f, 1.0f };
static const HudColor C_ROW   = { 0xd9 / 255.0f, 0xc8 / 255.0f, 0xf5 / 255.0f, 1.0f };
static const HudColor C_LILAC = { 0xf2 / 255.0f, 0xc8 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
static const HudColor C_HEAD  = { 0xf6 / 255.0f, 0xe6 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor C_WARN  = { 0xff / 255.0f, 0x9d / 255.0f, 0xe0 / 255.0f, 1.0f };
static const HudColor G_EDGE  = { 0xcc / 255.0f, 0x33 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor G_POD   = { 0xdf / 255.0f, 0x80 / 255.0f, 0xff / 255.0f, 1.0f };
static const HudColor G_NONE  = { 0.0f, 0.0f, 0.0f, 0.0f };


//-------------------------------------------------------------------------------------
// Lines
//-------------------------------------------------------------------------------------
static int BuildLines( Line* pLines )
{
    int n = 0;
    Line l;
    l.nPath = -1; l.szText = NULL;
    l.nKind = LINE_HEADER; l.szText = "CONTENT PATHS"; pLines[n++] = l;
    l.szText = NULL;
    for( int i = 0; i < Scan_PathCount(); i++ ) { l.nKind = LINE_PATH; l.nPath = i; pLines[n++] = l; }
    l.nPath = -1;
    l.nKind = LINE_ADD;     pLines[n++] = l;
    l.nKind = LINE_HEADER;  l.szText = "LIBRARY"; pLines[n++] = l; l.szText = NULL;
    l.nKind = LINE_SCAN;    pLines[n++] = l;
    l.nKind = LINE_CLEAR;   pLines[n++] = l;
    l.nKind = LINE_HEADER;  l.szText = "DRIVES"; pLines[n++] = l; l.szText = NULL;
    l.nKind = LINE_REFRESH; pLines[n++] = l;
    return n;
}

// The line index of the nSel'th selectable line
static int LineOfSelection( const Line* pLines, int nLines, int nSel )
{
    for( int i = 0, k = 0; i < nLines; i++ )
        if( pLines[i].nKind != LINE_HEADER && k++ == nSel )
            return i;
    return 0;
}

static int SelectableCount( const Line* pLines, int nLines )
{
    int k = 0;
    for( int i = 0; i < nLines; i++ )
        if( pLines[i].nKind != LINE_HEADER )
            k++;
    return k;
}

static void Upper( char* sz )
{
    for( ; *sz; sz++ )
        *sz = (char)toupper( (unsigned char)*sz );
}

static void Toast( const char* sz, float fSeconds = 1.8f )
{
    char szUp[128];
    strcpy_s( szUp, sz );
    Upper( szUp );
    Hud_ShowToast( szUp, fSeconds );
}


//-------------------------------------------------------------------------------------
// Picker
//-------------------------------------------------------------------------------------
static int PickRowCount()
{
    if( s_nPickDrive < 0 )
    {
        int n = 0;
        for( int i = 0; i < MAX_DRIVES; i++ )
            if( Drives_Get( i ).bReady )
                s_nPickRows[n++] = i;
        return n;
    }
    int nDirs = 0;
    for( int i = 0; i < s_nPickEntries; i++ )
        if( s_PickEntries[i].bDir )
            nDirs++;
    return nDirs + 1;   // ".." first
}

// Folders only (Drives_List puts them first)
static const DirEntry* PickEntry( int nRow )
{
    int i = nRow - 1;
    return ( i >= 0 && i < s_nPickEntries && s_PickEntries[i].bDir ) ? &s_PickEntries[i] : NULL;
}

static void PickReload()
{
    s_nPickEntries = 0;
    if( s_nPickDrive >= 0 )
        Drives_List( s_szPick, s_PickEntries, MAX_PICK, &s_nPickEntries );
    s_nPickRow = 0;
    s_nPickTop = 0;
}


//-------------------------------------------------------------------------------------
// Keys
//-------------------------------------------------------------------------------------
static void UpdateKeys()
{
    if( s_bPicking )
    {
        static const HudKey c_Drives[] = { { HUD_POD_NONE, "\x04 \x05 MOVE" }, { HUD_POD_A, "OPEN" }, { HUD_POD_B, "CANCEL" } };
        static const HudKey c_Folder[] = { { HUD_POD_NONE, "\x04 \x05 MOVE" }, { HUD_POD_A, "OPEN" }, { HUD_POD_Y, "ADD THIS FOLDER" }, { HUD_POD_B, "BACK" } };
        if( s_nPickDrive < 0 ) Hud_SetKeys( c_Drives, ARRAYSIZE( c_Drives ) );
        else                   Hud_SetKeys( c_Folder, ARRAYSIZE( c_Folder ) );
        return;
    }
    Line Lines[32];
    int nLines = BuildLines( Lines );
    const Line& l = Lines[LineOfSelection( Lines, nLines, s_nSel )];
    static const HudKey c_Path[]   = { { HUD_POD_NONE, "\x04 \x05 MOVE" }, { HUD_POD_NONE, "\x01 \x02 DEPTH" }, { HUD_POD_X, "REMOVE" }, { HUD_POD_B, "BACK" } };
    static const HudKey c_Action[] = { { HUD_POD_NONE, "\x04 \x05 MOVE" }, { HUD_POD_A, "SELECT" }, { HUD_POD_B, "BACK" } };
    if( l.nKind == LINE_PATH ) Hud_SetKeys( c_Path, ARRAYSIZE( c_Path ) );
    else                       Hud_SetKeys( c_Action, ARRAYSIZE( c_Action ) );
}


//-------------------------------------------------------------------------------------
// Name: Settings_Init() / Settings_Open() / Settings_IsOpen()
//-------------------------------------------------------------------------------------
HRESULT Settings_Init( D3DDevice* pDevice )
{
    if( FAILED( Assets_LoadSpriteInfo( "settings_win", &s_WinInfo ) ) || FAILED( Assets_LoadTexture( pDevice, "settings_win", &s_pWin ) ) )
        s_pWin = NULL;
    Assets_LoadTexture( pDevice, "row_on", &s_pRowOn );
    Assets_LoadTexture( pDevice, "dim", &s_pDim );
    return S_OK;
}

void Settings_Open()
{
    s_bOpen = TRUE;
    s_bPicking = FALSE;
    s_bConfirm = FALSE;
    s_bClearConfirm = FALSE;
    Hud_SetMenuName( "SETTINGS" );
    UpdateKeys();
}

BOOL Settings_IsOpen() { return s_bOpen; }


//-------------------------------------------------------------------------------------
// Name: Settings_Update()
//-------------------------------------------------------------------------------------
void Settings_Update( float dt, int nMoveX, int nMoveY, WORD wPressed )
{
    if( !s_bOpen )
        return;

    // A scan finishing: say what it found
    BOOL bScanning = Scan_IsRunning();
    if( s_bWasScanning && !bScanning )
    {
        char sz[64];
        sprintf_s( sz, "SCAN DONE: %d FOUND", Scan_FoundSoFar() );
        Toast( sz, 2.2f );
    }
    s_bWasScanning = bScanning;

    if( s_bPicking )
    {
        int n = PickRowCount();
        if( nMoveY && n )
            s_nPickRow = ( s_nPickRow + nMoveY + n ) % n;

        if( wPressed & XINPUT_GAMEPAD_A )
        {
            if( s_nPickDrive < 0 )
            {
                if( n )
                {
                    s_nPickDrive = s_nPickRows[s_nPickRow];
                    strcpy_s( s_szPick, Drives_Get( s_nPickDrive ).szRoot );
                    PickReload();
                }
            }
            else if( s_nPickRow == 0 )
                wPressed |= XINPUT_GAMEPAD_B;   // ".." is back
            else if( const DirEntry* e = PickEntry( s_nPickRow ) )
            {
                char szNew[MAX_PATH];
                sprintf_s( szNew, "%s%s\\", s_szPick, e->szName );
                strcpy_s( s_szPick, szNew );
                PickReload();
            }
            UpdateKeys();
        }
        if( wPressed & XINPUT_GAMEPAD_B )
        {
            if( s_nPickDrive < 0 )
                s_bPicking = FALSE;
            else
            {
                // Up a folder; from a drive's root back to the drive list
                size_t len = strlen( s_szPick );
                if( len && s_szPick[len - 1] == '\\' ) s_szPick[--len] = 0;
                char* p = strrchr( s_szPick, '\\' );
                if( p && strchr( s_szPick, ':' ) < p )
                {
                    p[1] = 0;
                    PickReload();
                }
                else
                {
                    s_nPickDrive = -1;
                    s_nPickRow = 0;
                }
            }
            UpdateKeys();
        }
        if( ( wPressed & XINPUT_GAMEPAD_Y ) && s_nPickDrive >= 0 )
        {
            if( Scan_AddPath( s_szPick, 2 ) )
            {
                char sz[160];
                sprintf_s( sz, "ADDED %s", s_szPick );
                Toast( sz );
                s_bPicking = FALSE;
                s_nSel = Scan_PathCount() - 1;
            }
            else
                Toast( Scan_PathCount() >= MAX_SCAN_PATHS ? "NO ROOM FOR MORE PATHS" : "THAT PATH IS ALREADY THERE" );
            UpdateKeys();
        }
        // Keep the selection on screen
        if( s_nPickRow < s_nPickTop ) s_nPickTop = s_nPickRow;
        if( s_nPickRow >= s_nPickTop + s_nVisible ) s_nPickTop = s_nPickRow - s_nVisible + 1;
        return;
    }

    Line Lines[32];
    int nLines = BuildLines( Lines );
    int nSelectable = SelectableCount( Lines, nLines );
    const Line* pLine = &Lines[LineOfSelection( Lines, nLines, s_nSel )];

    // Anything but a second X cancels a pending remove
    if( s_bConfirm && ( nMoveX || nMoveY || ( wPressed & ~XINPUT_GAMEPAD_X ) ) )
    {
        s_bConfirm = FALSE;
        if( wPressed & XINPUT_GAMEPAD_B )
            return;
    }
    // Anything but a second A cancels a pending delete-all
    if( s_bClearConfirm && ( nMoveX || nMoveY || ( wPressed & ~XINPUT_GAMEPAD_A ) ) )
    {
        s_bClearConfirm = FALSE;
        if( wPressed & XINPUT_GAMEPAD_B )
            return;
    }

    if( nMoveY )
    {
        s_nSel = ( s_nSel + nMoveY + nSelectable ) % nSelectable;
        UpdateKeys();
        return;
    }

    if( wPressed & XINPUT_GAMEPAD_B )
    {
        s_bOpen = FALSE;
        Hud_SetMenuName( NULL );
        Hud_SetMainKeys();
        return;
    }

    switch( pLine->nKind )
    {
    case LINE_PATH:
        if( nMoveX )
            Scan_SetDepth( pLine->nPath, Scan_GetPath( pLine->nPath ).nDepth + nMoveX );
        if( wPressed & XINPUT_GAMEPAD_X )
        {
            if( !s_bConfirm || s_nConfirm != pLine->nPath )
            {
                s_bConfirm = TRUE;
                s_nConfirm = pLine->nPath;
            }
            else
            {
                s_bConfirm = FALSE;
                Scan_RemovePath( pLine->nPath );
                Toast( "PATH REMOVED - SCAN TO UPDATE THE SHELVES", 2.2f );
                s_nSel = min( s_nSel, SelectableCount( Lines, BuildLines( Lines ) ) - 1 );
                UpdateKeys();
            }
        }
        break;

    case LINE_ADD:
        if( wPressed & XINPUT_GAMEPAD_A )
        {
            if( Scan_PathCount() >= MAX_SCAN_PATHS )
                Toast( "NO ROOM FOR MORE PATHS" );
            else
            {
                s_bPicking = TRUE;
                s_nPickDrive = -1;
                s_nPickRow = s_nPickTop = 0;
                UpdateKeys();
            }
        }
        break;

    case LINE_SCAN:
        if( wPressed & XINPUT_GAMEPAD_A )
        {
            if( Scan_IsRunning() )
                Toast( "ALREADY SCANNING" );
            else
            {
                Scan_Start();
                s_bWasScanning = TRUE;
            }
        }
        break;

    case LINE_CLEAR:
        if( wPressed & XINPUT_GAMEPAD_A )
        {
            if( Scan_PathCount() == 0 )
                Toast( "NO PATHS TO DELETE" );
            else if( !s_bClearConfirm )
                s_bClearConfirm = TRUE;
            else
            {
                s_bClearConfirm = FALSE;
                Scan_ClearPaths();
                s_nSel = Scan_PathCount() + 2;   // stay on this row (after + ADD PATH and SCAN NOW)
                Toast( "ALL PATHS DELETED - SCAN TO UPDATE THE SHELVES", 2.2f );
                UpdateKeys();
            }
        }
        break;

    case LINE_REFRESH:
        if( wPressed & XINPUT_GAMEPAD_A )
        {
            Drives_Refresh();
            Toast( "DRIVES REFRESHED" );
        }
        break;
    }
}


//-------------------------------------------------------------------------------------
// Drawing
//-------------------------------------------------------------------------------------
static void Fit( char* sz, size_t n, const char* szIn, float fPx, float fMaxW )
{
    strcpy_s( sz, n, szIn );
    if( Hud_TextWidth( HUD_FONT_MONO, sz, fPx, 0.0f ) <= fMaxW )
        return;
    size_t len = strlen( sz );
    while( len > 0 )
    {
        sz[--len] = 0;
        char szTry[200];
        sprintf_s( szTry, "%s...", sz );
        if( Hud_TextWidth( HUD_FONT_MONO, szTry, fPx, 0.0f ) <= fMaxW )
        {
            strcpy_s( sz, n, szTry );
            return;
        }
    }
}

static void FormatSize( ULONGLONG q, char* sz, size_t n )
{
    if( q < 1024ULL * 1024 * 1024 ) sprintf_s( sz, n, "%.0f MB", q / ( 1024.0 * 1024.0 ) );
    else                            sprintf_s( sz, n, "%.0f GB", q / ( 1024.0 * 1024.0 * 1024.0 ) );
}

void Settings_Render( D3DDevice* pDevice )
{
    if( !s_bOpen || !s_pWin )
        return;

    const ScreenInfo& scr = g_Screen;
    const float u0 = scr.Unit;
    const float fSafeW = (float)( scr.SafeRect.right - scr.SafeRect.left );
    const float k = min( 1.0f, fSafeW / ( 600.0f * u0 ) );
    const float u = u0 * k;
    const float s = u / ART_U;

    const float ww = 600.0f * u, wh = 330.0f * u;
    // Centred, nudged up 12u to leave the toast room above the bottom bar
    const float wx = floorf( scr.Width * 0.5f - ww * 0.5f ), wy = floorf( scr.Height * 0.5f - wh * 0.5f - 12.0f * u );
    const float px = wx + 5.0f * u, py = wy + 5.0f * u, pw = ww - 10.0f * u, ph = wh - 10.0f * u;
    const float fBarH = 24.0f * u, fBody = py + fBarH + 2.0f * u;

    Hud_BeginDraw( pDevice );
    Hud_DrawSprite( pDevice, s_pDim, 0.0f, 0.0f, (float)scr.Width, (float)scr.Height, 1.0f );
    {
        XGTEXTURE_DESC Desc;
        XGGetTextureDesc( s_pWin, 0, &Desc );
        Hud_DrawSprite( pDevice, s_pWin, wx - s_WinInfo.Pad * s, wy - s_WinInfo.Pad * s, Desc.Width * s, Desc.Height * s, 1.0f );
    }

    const float f9 = 9.0f * u, fRowPx = 10.5f * u;
    const float lx = px + 10.0f * u, lw = pw - 20.0f * u;
    const float ly = fBody + 8.0f * u;
    const float fPitch = 18.0f * u, fRowH = 16.0f * u;
    const float fTextX = lx + 10.0f * u, fRight = lx + lw - 10.0f * u;
    s_nVisible = max( 1, (int)( ( py + ph - 8.0f * u - ly ) / fPitch ) );
    char sz[200], szFit[200];

    if( s_bPicking )
    {
        // Bar: what's being picked
        strcpy_s( sz, s_nPickDrive < 0 ? "ADD PATH: CHOOSE A DRIVE" : s_szPick );
        Upper( sz );
        Fit( szFit, sizeof(szFit), sz, f9, pw * 0.6f );
        float w = Hud_TextWidth( HUD_FONT_MONO, szFit, f9, 0.0f );
        Hud_DrawText( pDevice, HUD_FONT_MONO, szFit, px + pw - 14.0f * u - w, py + fBarH * 0.5f + f9 * 0.35f, f9, 0.0f, C_BAR, G_NONE );

        int n = PickRowCount();
        for( int r = s_nPickTop; r < n && r < s_nPickTop + s_nVisible; r++ )
        {
            const float y = ly + ( r - s_nPickTop ) * fPitch;
            BOOL bOn = ( r == s_nPickRow );
            if( bOn )
                Hud_DrawSprite( pDevice, s_pRowOn, lx, y, lw, fRowH, 1.0f );
            if( s_nPickDrive < 0 )
            {
                const DriveInfo& d = Drives_Get( s_nPickRows[r] );
                char szFree[32];
                FormatSize( d.qwFree, szFree, sizeof(szFree) );
                sprintf_s( sz, "%s%s", bOn ? "> " : "  ", d.szName );
                Hud_DrawText( pDevice, HUD_FONT_MONO, sz, fTextX, y + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, bOn ? C_WHITE : C_ROW, bOn ? G_POD : G_NONE );
                sprintf_s( sz, "%s FREE", szFree );
                float wv = Hud_TextWidth( HUD_FONT_MONO, sz, fRowPx, 0.0f );
                Hud_DrawText( pDevice, HUD_FONT_MONO, sz, fRight - wv, y + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, bOn ? C_LILAC : C_DIM, G_NONE );
            }
            else
            {
                const DirEntry* e = PickEntry( r );
                sprintf_s( sz, "%s%s", bOn ? "> " : "  ", e ? e->szName : ".." );
                Fit( szFit, sizeof(szFit), sz, fRowPx, lw - 20.0f * u );
                Hud_DrawText( pDevice, HUD_FONT_MONO, szFit, fTextX, y + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, bOn ? C_WHITE : C_ROW, bOn ? G_POD : G_NONE );
            }
        }
        if( n == 0 || ( s_nPickDrive >= 0 && n == 1 && s_nPickEntries == 0 ) )
            Hud_DrawText( pDevice, HUD_FONT_MONO, s_nPickDrive < 0 ? "NO DRIVES FOUND" : "  (NO FOLDERS HERE)",
                          fTextX, ly + ( n ? fPitch : 0.0f ) + fRowH * 0.5f + fRowPx * 0.35f, fRowPx, 0.0f, C_DIM, G_NONE );
        Hud_EndDraw( pDevice );
        return;
    }

    Line Lines[32];
    int nLines = BuildLines( Lines );
    int nSelLine = LineOfSelection( Lines, nLines, s_nSel );
    // Scroll so the selected line (and the heading above the first one) stays on screen
    if( nSelLine < s_nTop ) s_nTop = ( nSelLine == 1 ) ? 0 : nSelLine;
    if( nSelLine >= s_nTop + s_nVisible ) s_nTop = nSelLine - s_nVisible + 1;

    for( int i = s_nTop; i < nLines && i < s_nTop + s_nVisible; i++ )
    {
        const Line& l = Lines[i];
        const float y = ly + ( i - s_nTop ) * fPitch;
        const float fBase = y + fRowH * 0.5f + fRowPx * 0.35f;
        if( l.nKind == LINE_HEADER )
        {
            const float fHead = 10.0f * u;
            Hud_DrawText( pDevice, HUD_FONT_TITLE, l.szText, lx + 2.0f * u, y + fRowH * 0.5f + fHead * 0.4f, fHead, fHead * 0.12f, C_HEAD, G_EDGE );
            continue;
        }

        BOOL bOn = ( i == nSelLine );
        if( bOn )
            Hud_DrawSprite( pDevice, s_pRowOn, lx, y, lw, fRowH, 1.0f );

        const char* szLabel = "";
        char szValue[96] = "";
        const HudColor* pValueColor = bOn ? &C_LILAC : &C_DIM;
        switch( l.nKind )
        {
        case LINE_PATH:
        {
            const ScanPath& p = Scan_GetPath( l.nPath );
            strcpy_s( sz, p.szPath );
            Upper( sz );
            szLabel = sz;
            if( s_bConfirm && s_nConfirm == l.nPath )
            {
                strcpy_s( szValue, "PRESS X AGAIN TO REMOVE" );
                pValueColor = &C_WARN;
                break;
            }
            // Which shelves this path fed in the last scan
            char szShelves[64] = "";
            if( !p.bScanned )
                strcpy_s( szShelves, "SCAN TO UPDATE" );
            else
            {
                static const char* c_szShort[NUM_SHELVES] = { "XBOX", "XBOX 360", "ARCADE", "APPS" };
                for( int sh = 0; sh < NUM_SHELVES; sh++ )
                    if( p.nFound[sh] )
                    {
                        if( szShelves[0] ) strcat_s( szShelves, ", " );
                        strcat_s( szShelves, c_szShort[sh] );
                    }
                if( !szShelves[0] )
                    strcpy_s( szShelves, "NOTHING FOUND" );
            }
            if( bOn )
                sprintf_s( szValue, "%s   \x01 DEPTH %d \x02", szShelves, p.nDepth );
            else
                sprintf_s( szValue, "%s \x03 DEPTH %d", szShelves, p.nDepth );
            if( !p.bScanned )
                pValueColor = &C_WARN;
            break;
        }
        case LINE_ADD:
            szLabel = "+ ADD PATH";
            sprintf_s( szValue, "%d / %d", Scan_PathCount(), MAX_SCAN_PATHS );
            break;
        case LINE_SCAN:
            szLabel = "SCAN NOW";
            if( Scan_IsRunning() )
                sprintf_s( szValue, "SCANNING... %d FOUND", Scan_FoundSoFar() );
            else
            {
                int n[NUM_SHELVES];
                for( int sh = 0; sh < NUM_SHELVES; sh++ ) n[sh] = Scan_ItemCount( sh );
                sprintf_s( szValue, "XBOX %d \x03 360 %d \x03 ARCADE %d \x03 APPS %d", n[0], n[1], n[2], n[3] );
            }
            break;
        case LINE_CLEAR:
            szLabel = "DELETE ALL PATHS";
            if( s_bClearConfirm )
            {
                strcpy_s( szValue, "PRESS A AGAIN TO DELETE ALL" );
                pValueColor = &C_WARN;
            }
            break;
        case LINE_REFRESH:
        {
            szLabel = "REFRESH DRIVES";
            for( int d = 0; d < MAX_DRIVES; d++ )
            {
                const DriveInfo& di = Drives_Get( d );
                if( !di.bReady )
                    continue;
                char szFree[32], szOne[48];
                FormatSize( di.qwFree, szFree, sizeof(szFree) );
                sprintf_s( szOne, "%s%s %s", szValue[0] ? " \x03 " : "", di.szName, szFree );
                strcat_s( szValue, szOne );
            }
            if( !szValue[0] )
                strcpy_s( szValue, "NO DRIVES FOUND" );
            break;
        }
        }

        float wv = Hud_TextWidth( HUD_FONT_MONO, szValue, fRowPx, 0.0f );
        char szLine[200];
        sprintf_s( szLine, "%s%s", bOn ? "> " : "  ", szLabel );
        Fit( szFit, sizeof(szFit), szLine, fRowPx, fRight - wv - 16.0f * u - fTextX );
        Hud_DrawText( pDevice, HUD_FONT_MONO, szFit, fTextX, fBase, fRowPx, 0.0f, bOn ? C_WHITE : C_ROW, bOn ? G_POD : G_NONE );
        Hud_DrawText( pDevice, HUD_FONT_MONO, szValue, fRight - wv, fBase, fRowPx, 0.0f, *pValueColor, G_NONE );
    }

    Hud_EndDraw( pDevice );
}
