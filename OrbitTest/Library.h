// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Library.h : the shelf views (XBOX, XBOX 360, ARCADE, APPS), the prototype's Xbox
// library. The menu ring tucks away, the orb floats up and the shelf's items (from the
// scan, Scan.h) fan out as 3D cases in a light cover-flow.
//
//   left / right  browse        A  launch
//   X             flip to the back cover (XBOX and XBOX 360; arcade and apps have none)
//   Y             details card: title, version, size, delete
//   B             back to the menu (closes the details card first)

#pragma once

HRESULT Library_Init( D3DDevice* pDevice );

// Opens the shelf for a ring item; FALSE if that item has no shelf
BOOL    Library_Open( const char* szRingLabel );
BOOL    Library_IsOpen();

// Call when a scan has replaced the library (Scan_Poll returned TRUE)
void    Library_OnLibraryChanged();

// Input while open. nMove = -1, 0, +1 (already repeat-limited); wPressed = newly
// pressed buttons. Sets *pfPulse to 1 on launch. Closes itself on B.
void    Library_Update( float dt, int nMove, WORD wPressed, float* pfPulse );

// Loads every shelf's box art and draws the covers for items without any, so the
// shelves open without a hitch. Call once at start, after the library is loaded.
void    Library_PreloadCovers( D3DDevice* pDevice );

// Items whose covers are still to load (after a rescan replaced them); *pnTotal = all items
int     Library_CoversRemaining( int* pnTotal );

// Fills in covers still missing (after a rescan), a couple per frame, the open shelf's
// nearest first. Call before the frame's scene starts (it uses its own render target).
void    Library_PrepareCovers( D3DDevice* pDevice );

// Cases, onto the back buffer after Post_EndFrame (no depth), far to near
void    Library_Render( D3DDevice* pDevice, const XMMATRIX& matView, const XMMATRIX& matProj );
