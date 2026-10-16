// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Files.h : the file manager, the prototype's FILES window over the dimmed menu.
//
//   left / right  switch drive          up / down  move through the list
//   A             open a folder (or launch a .xex / .xbe)
//   X             delete (press twice; protected places are refused)
//   Y             copy to the clipboard
//   RB            paste the clipboard into this folder (any drive to any drive)
//   B             up a folder; at a drive's root, back to the menu. Cancels a running job.

#pragma once

HRESULT Files_Init( D3DDevice* pDevice );
void    Files_Open();
BOOL    Files_IsOpen();

// nMoveX / nMoveY = -1, 0, +1 (repeat-limited); wPressed = newly pressed buttons
void    Files_Update( float dt, int nMoveX, int nMoveY, WORD wPressed );

// The window, onto the back buffer before Hud_Render (so toasts draw over it)
void    Files_Render( D3DDevice* pDevice );
