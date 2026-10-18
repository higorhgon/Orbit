// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Settings.h : the SETTINGS window. Owns everything about where content comes from:
//
//   CONTENT PATHS  the scan paths (like Aurora's Manage Paths): left / right sets a
//                  path's depth, X removes it (press twice), "+ ADD PATH" picks a folder
//   LIBRARY        SCAN NOW (rebuilds the shelves), DELETE ALL PATHS (press A twice)
//   MENU           shows or hides each ring item but SETTINGS (A switches ON / OFF);
//                  THEME picks the colour (left / right), saved to game:\theme.txt
//   DRIVES         REFRESH DRIVES (what's connected, free space, disc in the tray)

#pragma once

HRESULT Settings_Init( D3DDevice* pDevice );
void    Settings_Open();
BOOL    Settings_IsOpen();

// nMoveX / nMoveY = -1, 0, +1 (repeat-limited); wPressed = newly pressed buttons
void    Settings_Update( float dt, int nMoveX, int nMoveY, WORD wPressed );

// The window, onto the back buffer before Hud_Render
void    Settings_Render( D3DDevice* pDevice );
