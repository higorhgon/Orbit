// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Drives.cpp : see Drives.h.

#include "stdafx.h"
#include <stdlib.h>
#include <ctype.h>
#include "Drives.h"

//-------------------------------------------------------------------------------------
// Kernel exports (xboxkrnl.lib) that the XDK headers don't declare
//-------------------------------------------------------------------------------------
extern "C"
{
    typedef struct _OSTRING
    {
        USHORT Length;
        USHORT MaximumLength;
        PCHAR  Buffer;
    } OSTRING;

    LONG ObCreateSymbolicLink( OSTRING* pLinkName, OSTRING* pDeviceName );
    LONG ObDeleteSymbolicLink( OSTRING* pLinkName );
}

static void InitString( OSTRING* s, const char* sz )
{
    s->Buffer = (PCHAR)sz;
    s->Length = (USHORT)strlen( sz );
    s->MaximumLength = s->Length + 1;
}

// \??\Hdd1: -> \Device\Harddisk0\Partition1, replacing any link already there
static BOOL Mount( const char* szLink, const char* szDevice )
{
    OSTRING Link, Device;
    InitString( &Link, szLink );
    InitString( &Device, szDevice );
    ObDeleteSymbolicLink( &Link );
    return ObCreateSymbolicLink( &Link, &Device ) >= 0;
}


//-------------------------------------------------------------------------------------
// Drives
//-------------------------------------------------------------------------------------
struct DriveDef
{
    const char* szName;
    const char* szRoot;
    const char* szLink;
    const char* szDevice;
};

static const DriveDef c_Defs[MAX_DRIVES] =
{
    { "HDD1", "Hdd1:\\", "\\??\\Hdd1:", "\\Device\\Harddisk0\\Partition1" },
    { "USB0", "Usb0:\\", "\\??\\Usb0:", "\\Device\\Mass0" },
    { "USB1", "Usb1:\\", "\\??\\Usb1:", "\\Device\\Mass1" },
    { "USB2", "Usb2:\\", "\\??\\Usb2:", "\\Device\\Mass2" },
    { "DVD",  "Dvd:\\",  "\\??\\Dvd:",  "\\Device\\Cdrom0" },
};

static DriveInfo s_Drives[MAX_DRIVES];

void Drives_Init()
{
    for( int i = 0; i < MAX_DRIVES; i++ )
    {
        strcpy_s( s_Drives[i].szName, c_Defs[i].szName );
        strcpy_s( s_Drives[i].szRoot, c_Defs[i].szRoot );
        Mount( c_Defs[i].szLink, c_Defs[i].szDevice );
    }
    // Also the short names Dashlaunch's launch.ini uses (Hdd:, Usb:), in case
    // something launched from Orbit expects them
    Mount( "\\??\\Hdd:", "\\Device\\Harddisk0\\Partition1" );
    Mount( "\\??\\Usb:", "\\Device\\Mass0" );
    Drives_Refresh();
}

void Drives_Refresh()
{
    for( int i = 0; i < MAX_DRIVES; i++ )
    {
        ULARGE_INTEGER Free, Total;
        DriveInfo& d = s_Drives[i];
        d.bReady = GetDiskFreeSpaceExA( d.szRoot, &Free, &Total, NULL );
        d.qwFree  = d.bReady ? Free.QuadPart : 0;
        d.qwTotal = d.bReady ? Total.QuadPart : 0;
    }
}

int Drives_Count() { return MAX_DRIVES; }
const DriveInfo& Drives_Get( int i ) { return s_Drives[i]; }


//-------------------------------------------------------------------------------------
// Listing
//-------------------------------------------------------------------------------------
static int CompareEntries( const void* a, const void* b )
{
    const DirEntry* x = (const DirEntry*)a;
    const DirEntry* y = (const DirEntry*)b;
    if( x->bDir != y->bDir )
        return x->bDir ? -1 : 1;
    return _stricmp( x->szName, y->szName );
}

static BOOL IsDots( const char* sz )
{
    return strcmp( sz, "." ) == 0 || strcmp( sz, ".." ) == 0;
}

// szPath + "\" + szName, keeping one separator
static void JoinPath( char* szOut, size_t nOut, const char* szPath, const char* szName )
{
    size_t n = strlen( szPath );
    if( n && szPath[n - 1] == '\\' )
        sprintf_s( szOut, nOut, "%s%s", szPath, szName );
    else
        sprintf_s( szOut, nOut, "%s\\%s", szPath, szName );
}

BOOL Drives_List( const char* szPath, DirEntry* pEntries, int nMax, int* pnCount )
{
    *pnCount = 0;
    char szFind[MAX_PATH];
    JoinPath( szFind, sizeof(szFind), szPath, "*" );

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA( szFind, &fd );
    if( hFind == INVALID_HANDLE_VALUE )
        return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_NO_MORE_FILES;

    int n = 0;
    do
    {
        if( IsDots( fd.cFileName ) || strlen( fd.cFileName ) >= sizeof(pEntries[0].szName) )
            continue;
        if( n >= nMax )
            break;
        DirEntry& e = pEntries[n++];
        strcpy_s( e.szName, fd.cFileName );
        e.bDir = ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0;
        e.qwSize = ( (ULONGLONG)fd.nFileSizeHigh << 32 ) | fd.nFileSizeLow;
    }
    while( FindNextFileA( hFind, &fd ) );
    FindClose( hFind );

    qsort( pEntries, n, sizeof(DirEntry), CompareEntries );
    *pnCount = n;
    return TRUE;
}


//-------------------------------------------------------------------------------------
// Protection
//-------------------------------------------------------------------------------------
static const char* c_szProtected[] =
{
    "badupdatepayload",                      // the BadUpdate exploit
    "content\\0000000000000000\\5841122d",   // Rock Band Blitz, which launches it
    "launch.ini",                            // Dashlaunch settings
    "apps\\orbit",                           // Orbit itself
    "peerpressure",                          // Hdd1: Peer Pressure softmod, and Orbit when it's the dashboard
    "compatibility",                         // Hdd1: original Xbox emulator
    "cache",                                 // Hdd1: system cache
    "$systemupdate",
    "content\\0000000000000000\\fffe07d1",   // Hdd1: dashboard and Xbox Live data
};

BOOL Drives_IsProtected( const char* szPath )
{
    // Path after the drive ("Usb0:\Apps\Orbit" -> "apps\orbit"), lower case, no trailing "\"
    const char* p = strchr( szPath, ':' );
    if( !p )
        return TRUE;
    p++;
    while( *p == '\\' ) p++;
    char szRel[MAX_PATH];
    size_t n = 0;
    for( ; p[n] && n + 1 < sizeof(szRel); n++ )
        szRel[n] = (char)tolower( (unsigned char)p[n] );
    while( n && szRel[n - 1] == '\\' ) n--;
    szRel[n] = 0;

    if( n == 0 )
        return TRUE;   // a drive root

    for( int i = 0; i < ARRAYSIZE( c_szProtected ); i++ )
    {
        const char* q = c_szProtected[i];
        size_t m = strlen( q );
        // The protected place itself, anything inside it, or a folder that holds it
        if( strcmp( szRel, q ) == 0 )
            return TRUE;
        if( n > m && strncmp( szRel, q, m ) == 0 && szRel[m] == '\\' )
            return TRUE;
        if( m > n && strncmp( q, szRel, n ) == 0 && q[n] == '\\' )
            return TRUE;
    }
    return FALSE;
}


//-------------------------------------------------------------------------------------
// Jobs
//-------------------------------------------------------------------------------------
static CRITICAL_SECTION s_csJob;
static BOOL             s_bJobInit = FALSE;
static JobStatus        s_Job;
static volatile BOOL    s_bCancel = FALSE;
static char             s_szJobSrc[MAX_PATH];
static char             s_szJobDst[MAX_PATH];   // copy: the new item's full path
static BYTE*            s_pBuffer = NULL;
static const DWORD      COPY_CHUNK = 1024 * 1024;

static void EnsureJobInit()
{
    if( !s_bJobInit )
    {
        InitializeCriticalSection( &s_csJob );
        ZeroMemory( &s_Job, sizeof(s_Job) );
        s_bJobInit = TRUE;
    }
}

static void SetProgress( ULONGLONG qwDone )
{
    EnterCriticalSection( &s_csJob );
    s_Job.qwDone = qwDone;
    LeaveCriticalSection( &s_csJob );
}

static void Finish( int nState, const char* szError )
{
    EnterCriticalSection( &s_csJob );
    s_Job.nState = nState;
    strcpy_s( s_Job.szError, szError ? szError : "" );
    LeaveCriticalSection( &s_csJob );
}

static const char* BaseName( const char* szPath )
{
    size_t n = strlen( szPath );
    while( n && szPath[n - 1] == '\\' ) n--;
    const char* p = szPath + n;
    while( p > szPath && p[-1] != '\\' && p[-1] != ':' ) p--;
    return p;
}

static BOOL IsDirectory( const char* szPath, BOOL* pbExists )
{
    DWORD a = GetFileAttributesA( szPath );
    *pbExists = ( a != INVALID_FILE_ATTRIBUTES );
    return *pbExists && ( a & FILE_ATTRIBUTE_DIRECTORY );
}

// Total bytes (bBytes) or items under szPath, the item itself included.
// Sizes come from the directory listing itself (WIN32_FIND_DATA), so files are
// never opened one by one. Opening each file made a folder with thousands of
// files (e.g. Blinx's media folder) take minutes on FAT32, since every open
// rescans the whole directory.
static ULONGLONG MeasureDir( const char* szDir, BOOL bBytes )
{
    ULONGLONG qwTotal = 0;
    char szFind[MAX_PATH], szChild[MAX_PATH];
    JoinPath( szFind, sizeof(szFind), szDir, "*" );
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA( szFind, &fd );
    if( hFind == INVALID_HANDLE_VALUE )
        return 0;
    do
    {
        if( IsDots( fd.cFileName ) )
            continue;
        if( s_bCancel )
            break;
        if( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        {
            JoinPath( szChild, sizeof(szChild), szDir, fd.cFileName );
            qwTotal += ( bBytes ? 0 : 1 ) + MeasureDir( szChild, bBytes );
        }
        else
            qwTotal += bBytes ? ( ( (ULONGLONG)fd.nFileSizeHigh << 32 ) | fd.nFileSizeLow ) : 1;
    }
    while( FindNextFileA( hFind, &fd ) );
    FindClose( hFind );
    return qwTotal;
}

static ULONGLONG Measure( const char* szPath, BOOL bBytes )
{
    // One lookup gives both the folder flag and the size
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if( !GetFileAttributesExA( szPath, GetFileExInfoStandard, &fa ) )
        return bBytes ? 0 : 1;   // as before: a missing item is one item with no bytes
    if( fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
        return ( bBytes ? 0 : 1 ) + MeasureDir( szPath, bBytes );
    return bBytes ? ( ( (ULONGLONG)fa.nFileSizeHigh << 32 ) | fa.nFileSizeLow ) : 1;
}

// Deletes szPath and everything in it. pqwDone counts items, for progress (may be NULL).
static BOOL DeleteTree( const char* szPath, ULONGLONG* pqwDone, BOOL bStopOnCancel )
{
    BOOL bExists;
    BOOL bDir = IsDirectory( szPath, &bExists );
    if( !bExists )
        return TRUE;
    SetFileAttributesA( szPath, FILE_ATTRIBUTE_NORMAL );

    if( bDir )
    {
        char szFind[MAX_PATH], szChild[MAX_PATH];
        JoinPath( szFind, sizeof(szFind), szPath, "*" );
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA( szFind, &fd );
        if( hFind != INVALID_HANDLE_VALUE )
        {
            do
            {
                if( IsDots( fd.cFileName ) )
                    continue;
                if( bStopOnCancel && s_bCancel )
                    break;
                JoinPath( szChild, sizeof(szChild), szPath, fd.cFileName );
                if( !DeleteTree( szChild, pqwDone, bStopOnCancel ) )
                {
                    FindClose( hFind );
                    return FALSE;
                }
            }
            while( FindNextFileA( hFind, &fd ) );
            FindClose( hFind );
        }
        if( bStopOnCancel && s_bCancel )
            return TRUE;
        if( !RemoveDirectoryA( szPath ) )
            return FALSE;
    }
    else if( !DeleteFileA( szPath ) )
    {
        return FALSE;
    }

    if( pqwDone )
        SetProgress( ++*pqwDone );
    return TRUE;
}

static const char* ErrorText( DWORD dwError, const char* szDefault )
{
    switch( dwError )
    {
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:   return "NOT ENOUGH SPACE";
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_INVALID_NAME:       return "NAME TOO LONG FOR THAT DRIVE";
    case ERROR_ACCESS_DENIED:      return "ACCESS DENIED";
    case ERROR_WRITE_PROTECT:      return "DRIVE IS READ ONLY";
    default:                       return szDefault;
    }
}

// Copies szSrc to szDst (the new item's full path). Returns NULL or an error.
// nIsDir: 1 or 0 when the caller already knows from its directory listing,
// -1 to look it up (saves a lookup per file in big folders).
static const char* CopyTree( const char* szSrc, const char* szDst, ULONGLONG* pqwDone, int nIsDir = -1 )
{
    BOOL bExists;
    if( nIsDir < 0 )
        nIsDir = IsDirectory( szSrc, &bExists ) ? 1 : 0;
    if( nIsDir )
    {
        if( !CreateDirectoryA( szDst, NULL ) )
            return ErrorText( GetLastError(), "COULDN'T MAKE FOLDER" );
        char szFind[MAX_PATH], szFrom[MAX_PATH], szTo[MAX_PATH];
        JoinPath( szFind, sizeof(szFind), szSrc, "*" );
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA( szFind, &fd );
        if( hFind == INVALID_HANDLE_VALUE )
            return NULL;
        const char* szErr = NULL;
        do
        {
            if( IsDots( fd.cFileName ) )
                continue;
            if( s_bCancel )
                break;
            JoinPath( szFrom, sizeof(szFrom), szSrc, fd.cFileName );
            JoinPath( szTo, sizeof(szTo), szDst, fd.cFileName );
            szErr = CopyTree( szFrom, szTo, pqwDone, ( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) ? 1 : 0 );
        }
        while( !szErr && FindNextFileA( hFind, &fd ) );
        FindClose( hFind );
        return szErr;
    }

    HANDLE hIn = CreateFileA( szSrc, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( hIn == INVALID_HANDLE_VALUE )
        return "COULDN'T READ A FILE";
    HANDLE hOut = CreateFileA( szDst, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL );
    if( hOut == INVALID_HANDLE_VALUE )
    {
        DWORD dwErr = GetLastError();
        CloseHandle( hIn );
        return ErrorText( dwErr, "COULDN'T WRITE A FILE" );
    }

    const char* szErr = NULL;
    for( ;; )
    {
        if( s_bCancel )
            break;
        DWORD dwRead = 0, dwWritten = 0;
        if( !ReadFile( hIn, s_pBuffer, COPY_CHUNK, &dwRead, NULL ) )
        {
            szErr = "COULDN'T READ A FILE";
            break;
        }
        if( dwRead == 0 )
            break;
        if( !WriteFile( hOut, s_pBuffer, dwRead, &dwWritten, NULL ) || dwWritten != dwRead )
        {
            szErr = ErrorText( GetLastError(), "COULDN'T WRITE A FILE" );
            break;
        }
        *pqwDone += dwRead;
        SetProgress( *pqwDone );
    }
    CloseHandle( hOut );
    CloseHandle( hIn );
    return szErr;
}

// Drive root of a path ("Usb0:\..." -> "Usb0:\")
static void RootOf( const char* szPath, char* szRoot, size_t nRoot )
{
    const char* p = strchr( szPath, ':' );
    size_t n = p ? (size_t)( p - szPath ) + 1 : 0;
    if( n + 2 > nRoot ) n = 0;
    memcpy( szRoot, szPath, n );
    szRoot[n] = '\\';
    szRoot[n + 1] = 0;
}

static DWORD WINAPI JobProc( LPVOID )
{
    ULONGLONG qwDone = 0;
    if( s_Job.nType == JOB_COPY )
    {
        ULONGLONG qwTotal = Measure( s_szJobSrc, TRUE );
        EnterCriticalSection( &s_csJob );
        s_Job.qwTotal = qwTotal;
        LeaveCriticalSection( &s_csJob );

        // Room for it? (a little slack for folder entries)
        char szRoot[16];
        ULARGE_INTEGER Free, Total;
        RootOf( s_szJobDst, szRoot, sizeof(szRoot) );
        if( GetDiskFreeSpaceExA( szRoot, &Free, &Total, NULL ) && Free.QuadPart < qwTotal + 1024 * 1024 )
        {
            Finish( JOBSTATE_FAILED, "NOT ENOUGH SPACE" );
            return 0;
        }

        const char* szErr = CopyTree( s_szJobSrc, s_szJobDst, &qwDone );
        if( szErr || s_bCancel )
        {
            // Take back the half-made copy (it didn't exist before we started)
            BOOL bWasCancel = s_bCancel;
            s_bCancel = FALSE;
            DeleteTree( s_szJobDst, NULL, FALSE );
            Finish( bWasCancel ? JOBSTATE_CANCELLED : JOBSTATE_FAILED, szErr );
            return 0;
        }
    }
    else
    {
        ULONGLONG qwTotal = Measure( s_szJobSrc, FALSE );
        EnterCriticalSection( &s_csJob );
        s_Job.qwTotal = qwTotal;
        LeaveCriticalSection( &s_csJob );
        if( !DeleteTree( s_szJobSrc, &qwDone, TRUE ) )
        {
            Finish( JOBSTATE_FAILED, ErrorText( GetLastError(), "COULDN'T DELETE EVERYTHING" ) );
            return 0;
        }
        if( s_bCancel )
        {
            Finish( JOBSTATE_CANCELLED, NULL );
            return 0;
        }
    }
    Finish( JOBSTATE_DONE, NULL );
    return 0;
}

static BOOL StartJob( int nType, char* szWhyNot, int nWhyNot )
{
    if( !s_pBuffer )
        s_pBuffer = new BYTE[COPY_CHUNK];

    EnterCriticalSection( &s_csJob );
    s_Job.nType  = nType;
    s_Job.nState = JOBSTATE_RUNNING;
    s_Job.qwTotal = s_Job.qwDone = 0;
    s_Job.szError[0] = 0;
    strcpy_s( s_Job.szName, sizeof(s_Job.szName), BaseName( s_szJobSrc ) );
    LeaveCriticalSection( &s_csJob );
    s_bCancel = FALSE;

    HANDLE hThread = CreateThread( NULL, 256 * 1024, JobProc, NULL, 0, NULL );
    if( !hThread )
    {
        s_Job.nState = JOBSTATE_IDLE;
        strcpy_s( szWhyNot, nWhyNot, "COULDN'T START" );
        return FALSE;
    }
    CloseHandle( hThread );
    return TRUE;
}

static BOOL JobBusy()
{
    EnsureJobInit();
    return s_Job.nState == JOBSTATE_RUNNING;
}

BOOL Drives_StartCopy( const char* szSrc, const char* szDestDir, char* szWhyNot, int nWhyNot )
{
    if( JobBusy() )
    {
        strcpy_s( szWhyNot, nWhyNot, "STILL WORKING" );
        return FALSE;
    }

    char szDst[MAX_PATH];
    JoinPath( szDst, sizeof(szDst), szDestDir, BaseName( szSrc ) );

    // Not into itself
    size_t n = strlen( szSrc );
    if( _strnicmp( szDst, szSrc, n ) == 0 && ( szDst[n] == '\\' || szDst[n] == 0 ) )
    {
        strcpy_s( szWhyNot, nWhyNot, "CAN'T PASTE INTO ITSELF" );
        return FALSE;
    }
    BOOL bExists;
    IsDirectory( szDst, &bExists );
    if( bExists )
    {
        strcpy_s( szWhyNot, nWhyNot, "ALREADY THERE" );
        return FALSE;
    }

    strcpy_s( s_szJobSrc, szSrc );
    strcpy_s( s_szJobDst, szDst );
    return StartJob( JOB_COPY, szWhyNot, nWhyNot );
}

BOOL Drives_StartDelete( const char* szPath, char* szWhyNot, int nWhyNot )
{
    if( JobBusy() )
    {
        strcpy_s( szWhyNot, nWhyNot, "STILL WORKING" );
        return FALSE;
    }
    if( Drives_IsProtected( szPath ) )
    {
        strcpy_s( szWhyNot, nWhyNot, "PROTECTED" );
        return FALSE;
    }
    strcpy_s( s_szJobSrc, szPath );
    s_szJobDst[0] = 0;
    return StartJob( JOB_DELETE, szWhyNot, nWhyNot );
}

void Drives_CancelJob()
{
    if( JobBusy() )
        s_bCancel = TRUE;
}

void Drives_GetJob( JobStatus* pStatus )
{
    EnsureJobInit();
    EnterCriticalSection( &s_csJob );
    *pStatus = s_Job;
    LeaveCriticalSection( &s_csJob );
}

void Drives_ClearJob()
{
    EnsureJobInit();
    EnterCriticalSection( &s_csJob );
    if( s_Job.nState != JOBSTATE_RUNNING )
        s_Job.nState = JOBSTATE_IDLE;
    LeaveCriticalSection( &s_csJob );
}
