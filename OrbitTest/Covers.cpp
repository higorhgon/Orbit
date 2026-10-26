// Orbit, an Xbox 360 homebrew launcher.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Covers.cpp : see Covers.h. XboxUnity's cover API, the same Aurora uses:
//
//   GET /Resources/Lib/CoverInfo.php?titleid=<id>       {"Covers":[{"CoverID":"225","Rating":"5","Official":"1",...}]}
//   GET /Resources/Lib/Cover.php?cid=<CoverID>&size=large   the 900 x 600 box scan (JPEG, a few PNG)
//
// Requests are HTTP/1.0 so the reply comes whole (no chunks) and ends when the server
// closes. The sockets are marked insecure, as homebrew does, since XboxUnity is a
// plain web server and not an Xbox Live one.

#include "stdafx.h"
#include "Scan.h"
#include "Covers.h"

static const char* UNITY_HOST = "xboxunity.net";
static const char* COVERS_DIR = "game:\\Covers\\";
// Folders a cover may already be in (as Library.cpp looks for them)
static const char* c_szHaveDirs[] = { "game:\\Covers\\", "Usb0:\\Orbit\\Covers\\" };

static const int   MAX_TITLES  = 1024;
static const DWORD BUF_SIZE    = 4 * 1024 * 1024;   // a box scan is well under 2 MB
static const int   SOCK_TIMEOUT = 15000;           // ms, each send / receive
static const int   SO_MARKINSECURE = 0x5801;        // undocumented: lets a socket talk to non-Live servers

static char           s_szIds[MAX_TITLES][12];      // this pass's title IDs (copied from the library on start)
static int            s_nIds;
static volatile LONG  s_State = COVERS_IDLE;
static volatile LONG  s_nDone, s_nGot;
static BOOL           s_bNetUp = FALSE;
static BYTE*          s_pBuf = NULL;

// Covers saved and not yet handed to the shelves
static CRITICAL_SECTION s_Lock;
static BOOL             s_bLockInit = FALSE;
static char             s_szReady[MAX_TITLES][12];
static int              s_nReadyHead, s_nReadyTail;


//-------------------------------------------------------------------------------------
// Network
//-------------------------------------------------------------------------------------

// Starts XNet once and waits (up to ~20 s, the console may have just booted) for a
// cable and an address
static BOOL NetUp()
{
    if( !s_bNetUp )
    {
        XNetStartupParams xnsp;
        ZeroMemory( &xnsp, sizeof(xnsp) );
        xnsp.cfgSizeOfStruct = sizeof(xnsp);
        xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
        if( XNetStartup( &xnsp ) != 0 )
            return FALSE;
        WSADATA wsa;
        if( WSAStartup( MAKEWORD( 2, 2 ), &wsa ) != 0 )
            return FALSE;
        s_bNetUp = TRUE;
    }
    for( int t = 0; t < 80; t++ )
    {
        XNADDR xna;
        DWORD dw = XNetGetTitleXnAddr( &xna );
        if( ( XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE ) && dw != XNET_GET_XNADDR_PENDING && !( dw & XNET_GET_XNADDR_NONE ) )
            return TRUE;
        Sleep( 250 );
    }
    return FALSE;
}

static BOOL Resolve( const char* szHost, IN_ADDR* pAddr )
{
    WSAEVENT hEvent = WSACreateEvent();
    XNDNS* pDns = NULL;
    BOOL bOk = FALSE;
    if( XNetDnsLookup( szHost, hEvent, &pDns ) == 0 && pDns )
    {
        WaitForSingleObject( hEvent, 10000 );
        if( pDns->iStatus == 0 && pDns->cina > 0 )
        {
            *pAddr = pDns->aina[0];
            bOk = TRUE;
        }
        XNetDnsRelease( pDns );
    }
    WSACloseEvent( hEvent );
    return bOk;
}

// GETs szPath from XboxUnity into s_pBuf. Returns the body's length (and where it
// starts), or -1 on any failure or a status other than 200.
static int HttpGet( const IN_ADDR& addr, const char* szPath, BYTE** ppBody )
{
    SOCKET s = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( s == INVALID_SOCKET )
        return -1;
    BOOL bTrue = TRUE;
    setsockopt( s, SOL_SOCKET, SO_MARKINSECURE, (const char*)&bTrue, sizeof(bTrue) );
    int nTimeout = SOCK_TIMEOUT;
    setsockopt( s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeout, sizeof(nTimeout) );
    setsockopt( s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&nTimeout, sizeof(nTimeout) );

    sockaddr_in sa;
    ZeroMemory( &sa, sizeof(sa) );
    sa.sin_family = AF_INET;
    sa.sin_port   = htons( 80 );
    sa.sin_addr   = addr;
    int nLen = -1;
    if( connect( s, (sockaddr*)&sa, sizeof(sa) ) == 0 )
    {
        char szReq[512];
        int nReq = sprintf_s( szReq, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Orbit\r\nConnection: close\r\n\r\n", szPath, UNITY_HOST );
        if( send( s, szReq, nReq, 0 ) == nReq )
        {
            DWORD dwHave = 0;
            for( ;; )
            {
                if( dwHave >= BUF_SIZE - 1 )
                {
                    dwHave = 0;   // too big to be a cover
                    break;
                }
                int n = recv( s, (char*)s_pBuf + dwHave, BUF_SIZE - 1 - dwHave, 0 );
                if( n <= 0 )
                {
                    if( n < 0 )
                        dwHave = 0;
                    break;
                }
                dwHave += n;
            }
            s_pBuf[dwHave] = 0;
            // "HTTP/1.1 200 OK\r\n...\r\n\r\n<body>"
            char* pEnd = dwHave ? strstr( (char*)s_pBuf, "\r\n\r\n" ) : NULL;
            const char* pSp = strchr( (char*)s_pBuf, ' ' );
            if( pEnd && pSp && pSp < pEnd && atoi( pSp + 1 ) == 200 )
            {
                *ppBody = (BYTE*)pEnd + 4;
                nLen = (int)( dwHave - ( *ppBody - s_pBuf ) );
            }
        }
    }
    closesocket( s );
    return nLen;
}


//-------------------------------------------------------------------------------------
// XboxUnity
//-------------------------------------------------------------------------------------

// A field's value in one cover object ("Rating":"5" or "Rating":null -> 0)
static int FieldInt( const char* pObj, const char* pObjEnd, const char* szField )
{
    const char* p = strstr( pObj, szField );
    if( !p || p > pObjEnd )
        return 0;
    p += strlen( szField );
    while( *p == '"' || *p == ':' || *p == ' ' ) p++;
    return atoi( p );
}

// The cover to take from a CoverInfo reply: official first, then the highest rating,
// then the oldest upload. 0 if there's none.
static int BestCover( char* szJson )
{
    int nBest = 0, nBestOfficial = -1, nBestRating = -1;
    for( char* p = strstr( szJson, "\"CoverID\"" ); p; p = strstr( p + 1, "\"CoverID\"" ) )
    {
        const char* pEnd = strchr( p, '}' );
        if( !pEnd )
            break;
        int nId       = FieldInt( p, pEnd, "\"CoverID\"" );
        int nOfficial = FieldInt( p, pEnd, "\"Official\"" );
        int nRating   = FieldInt( p, pEnd, "\"Rating\"" );
        if( nId <= 0 )
            continue;
        if( nOfficial > nBestOfficial ||
            ( nOfficial == nBestOfficial && ( nRating > nBestRating || ( nRating == nBestRating && nId < nBest ) ) ) )
        {
            nBest = nId;
            nBestOfficial = nOfficial;
            nBestRating = nRating;
        }
    }
    return nBest;
}

static BOOL HaveCover( const char* szId )
{
    static const char* c_szExt[] = { "jpg", "png" };
    for( int d = 0; d < ARRAYSIZE( c_szHaveDirs ); d++ )
        for( int e = 0; e < ARRAYSIZE( c_szExt ); e++ )
        {
            char szPath[MAX_PATH];
            sprintf_s( szPath, "%s%s.%s", c_szHaveDirs[d], szId, c_szExt[e] );
            if( GetFileAttributesA( szPath ) != (DWORD)-1 )
                return TRUE;
        }
    return FALSE;
}

// Writes the image to a temporary file first, so a cut-off download never shows up as a cover
static BOOL SaveCover( const char* szId, const BYTE* pData, int nLen )
{
    const char* szExt;
    if( nLen > 4 && pData[0] == 0xFF && pData[1] == 0xD8 )
        szExt = "jpg";
    else if( nLen > 8 && memcmp( pData, "\x89PNG", 4 ) == 0 )
        szExt = "png";
    else
        return FALSE;

    char szTmp[MAX_PATH], szPath[MAX_PATH];
    sprintf_s( szTmp, "%s%s.tmp", COVERS_DIR, szId );
    sprintf_s( szPath, "%s%s.%s", COVERS_DIR, szId, szExt );
    HANDLE h = CreateFileA( szTmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    if( h == INVALID_HANDLE_VALUE )
        return FALSE;
    DWORD dwWritten = 0;
    BOOL bOk = WriteFile( h, pData, nLen, &dwWritten, NULL ) && dwWritten == (DWORD)nLen;
    CloseHandle( h );
    if( bOk )
        bOk = MoveFileA( szTmp, szPath );
    if( !bOk )
        DeleteFileA( szTmp );
    return bOk;
}

static DWORD WINAPI CoversProc( LPVOID )
{
    s_State = COVERS_CONNECTING;
    IN_ADDR addr;
    if( !NetUp() || !Resolve( UNITY_HOST, &addr ) )
    {
        s_State = COVERS_NO_NETWORK;
        return 0;
    }
    s_State = COVERS_DOWNLOADING;
    CreateDirectoryA( "game:\\Covers", NULL );

    for( int i = 0; i < s_nIds; i++ )
    {
        const char* szId = s_szIds[i];
        char szPath[128];
        BYTE* pBody = NULL;
        sprintf_s( szPath, "/Resources/Lib/CoverInfo.php?titleid=%s", szId );
        int nLen = HttpGet( addr, szPath, &pBody );
        int nCover = nLen > 0 ? BestCover( (char*)pBody ) : 0;
        if( nCover )
        {
            sprintf_s( szPath, "/Resources/Lib/Cover.php?cid=%d&size=large", nCover );
            nLen = HttpGet( addr, szPath, &pBody );
            if( nLen > 0 && SaveCover( szId, pBody, nLen ) )
            {
                InterlockedIncrement( &s_nGot );
                EnterCriticalSection( &s_Lock );
                strcpy_s( s_szReady[s_nReadyTail % MAX_TITLES], szId );
                s_nReadyTail++;
                LeaveCriticalSection( &s_Lock );
            }
        }
        InterlockedIncrement( &s_nDone );
        Sleep( 200 );   // go easy on XboxUnity
    }
    s_State = COVERS_DONE;
    return 0;
}


//-------------------------------------------------------------------------------------
// Public
//-------------------------------------------------------------------------------------
void Covers_Start()
{
    if( s_State == COVERS_CONNECTING || s_State == COVERS_DOWNLOADING )
        return;
    if( !s_bLockInit )
    {
        InitializeCriticalSection( &s_Lock );
        s_bLockInit = TRUE;
    }
    if( !s_pBuf )
    {
        s_pBuf = (BYTE*)malloc( BUF_SIZE );
        if( !s_pBuf )
            return;
    }

    // Each title ID once, only those with no cover yet
    s_nIds = 0;
    for( int sh = 0; sh < NUM_SHELVES; sh++ )
        for( int i = 0; i < Scan_ItemCount( sh ) && s_nIds < MAX_TITLES; i++ )
        {
            const char* szId = Scan_GetItem( sh, i ).szTitleId;
            if( !szId[0] || HaveCover( szId ) )
                continue;
            BOOL bSeen = FALSE;
            for( int k = 0; k < s_nIds && !bSeen; k++ )
                bSeen = _stricmp( s_szIds[k], szId ) == 0;
            if( !bSeen )
                strcpy_s( s_szIds[s_nIds++], szId );
        }
    s_nDone = s_nGot = 0;
    if( s_nIds == 0 )
    {
        s_State = COVERS_IDLE;
        return;
    }

    s_State = COVERS_CONNECTING;
    HANDLE h = CreateThread( NULL, 64 * 1024, CoversProc, NULL, 0, NULL );
    if( h )
        CloseHandle( h );
    else
        s_State = COVERS_IDLE;
}

CoversState Covers_GetState() { return (CoversState)s_State; }

void Covers_GetProgress( int* pnDone, int* pnTotal, int* pnGot )
{
    if( pnDone )  *pnDone = s_nDone;
    if( pnTotal ) *pnTotal = s_nIds;
    if( pnGot )   *pnGot = s_nGot;
}

BOOL Covers_PollDone( char* szTitleId )
{
    if( !s_bLockInit )
        return FALSE;
    BOOL bAny = FALSE;
    EnterCriticalSection( &s_Lock );
    if( s_nReadyHead < s_nReadyTail )
    {
        strcpy_s( szTitleId, 12, s_szReady[s_nReadyHead % MAX_TITLES] );
        s_nReadyHead++;
        bAny = TRUE;
    }
    LeaveCriticalSection( &s_Lock );
    return bAny;
}
