// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Ring.h : the rotating menu ring (Xbox, Xbox 360, Arcade, Apps, Files, Settings).

#pragma once

static const int RING_ITEMS = 6;

HRESULT     Ring_Init( D3DDevice* pDevice );
void        Ring_SetDepthTest( BOOL bEnable );    // debug view switch
void        Ring_SetScale( float fScale );        // whole ring; tucks to 0.001 while a shelf is open
void        Ring_Update( float dt, int nMove );   // nMove: -1, 0, +1
int         Ring_GetIndex();
const char* Ring_GetLabel( int nIndex );          // upper case, e.g. "XBOX 360"

// Transparent pieces are drawn back to front around the orb, like three.js sorts
// them, in three passes split at fSplitDepth (the orb centre):
//   RING_BACK         farther pieces, into the scene before the orb's shell
//   RING_FRONT_TRACK  the nearer track, into the scene after the shell
//   RING_FRONT_PANELS the nearer panels, onto the back buffer after bloom, so the
//                     orb's glow never washes over them
// Panels turn to face vEye.
enum { RING_BACK, RING_FRONT_TRACK, RING_FRONT_PANELS };
void Ring_Render( D3DDevice* pDevice, const XMMATRIX& matView, const XMMATRIX& matProj,
                  const XMVECTOR& vEye, int nPass, float fSplitDepth );
