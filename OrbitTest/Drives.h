// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Drives.h : the console's drives and the file work done on them.
//
// A title only sees game:\ until it links the devices it wants to drive names, as
// homebrew does with the kernel's ObCreateSymbolicLink:
//
//   Hdd1:  \Device\Harddisk0\Partition1   internal hard drive
//   Usb0:  \Device\Mass0  (Usb1:, Usb2:)  USB drives
//   Dvd:   \Device\Cdrom0                 disc drive
//
// Copy and delete run on a worker thread so the screen keeps drawing; the file
// manager polls Drives_GetJob() for progress. Copying works between any two drives.

#pragma once

#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)   // GetFileAttributes failure (not in the XDK headers)
#endif

static const int MAX_DRIVES = 5;

struct DriveInfo
{
    char      szName[8];     // "HDD1", for display
    char      szRoot[8];     // "Hdd1:\" for paths
    BOOL      bReady;        // mounted with media in it
    ULONGLONG qwFree;
    ULONGLONG qwTotal;
};

// Links the drive names; safe to call again to refresh free space and the disc tray
void  Drives_Init();
// Rechecks which drives are there and their free space
void  Drives_Refresh();
int   Drives_Count();
const DriveInfo& Drives_Get( int i );

// One folder's contents: folders first then files, each alphabetical. FALSE if the
// folder can't be read. Entries past nMax are dropped.
struct DirEntry
{
    char      szName[128];
    BOOL      bDir;
    ULONGLONG qwSize;
};
BOOL  Drives_List( const char* szPath, DirEntry* pEntries, int nMax, int* pnCount );

// Places Orbit refuses to delete: drive roots, the BadUpdate exploit (its payload
// folder and Rock Band Blitz, which launches it), launch.ini, Orbit itself, the
// Peer Pressure folder, and the internal drive's system folders. Copying from them is fine.
BOOL  Drives_IsProtected( const char* szPath );

// Background jobs: one at a time
enum { JOB_NONE, JOB_COPY, JOB_DELETE };
enum { JOBSTATE_IDLE, JOBSTATE_RUNNING, JOBSTATE_DONE, JOBSTATE_FAILED, JOBSTATE_CANCELLED };
struct JobStatus
{
    int       nType;
    int       nState;
    char      szName[64];       // what is being copied / deleted
    char      szError[64];      // when failed
    ULONGLONG qwTotal;          // bytes (copy) or items (delete)
    ULONGLONG qwDone;
};

// Copies szSrc (file or folder) into the folder szDestDir. Fails up front if the
// name already exists there or the destination is inside the source.
BOOL  Drives_StartCopy( const char* szSrc, const char* szDestDir, char* szWhyNot, int nWhyNot );
BOOL  Drives_StartDelete( const char* szPath, char* szWhyNot, int nWhyNot );
void  Drives_CancelJob();
void  Drives_GetJob( JobStatus* pStatus );
// Clears a finished job so the next can start
void  Drives_ClearJob();
