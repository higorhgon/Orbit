// Orbit, an Xbox 360 homebrew launcher.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Covers.h : downloads missing box art from XboxUnity, like Aurora does, after each scan.
// For every title ID in the library without a cover in the covers folders, asks
// XboxUnity for its covers (official first, then the best rated) and saves the 900 x 600
// box scan as game:\Covers\<title ID>.jpg (or .png). Plain HTTP, no account needed.
// Runs on a worker thread; each cover is handed to the shelves as it arrives.

#pragma once

enum CoversState { COVERS_IDLE, COVERS_CONNECTING, COVERS_DOWNLOADING, COVERS_DONE, COVERS_NO_NETWORK };

// Starts a download pass over the library (does nothing if one is running)
void        Covers_Start();
CoversState Covers_GetState();
// Titles looked up so far / to look up, and covers saved, this pass
void        Covers_GetProgress( int* pnDone, int* pnTotal, int* pnGot );

// TRUE once for each cover saved, with its title ID in szTitleId (12 chars)
BOOL        Covers_PollDone( char* szTitleId );
