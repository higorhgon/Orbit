// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Screen.h : the back buffer layout chosen for the console's video mode.
// Everything drawn should size itself from this, never from hard-coded 1280x720.

#pragma once

struct ScreenInfo
{
    UINT  Width;          // Back buffer size in pixels
    UINT  Height;
    UINT  DisplayHeight;  // What the console outputs (480, 720, 1080, ...)
    BOOL  bWidescreen;    // Display shape set in System Settings
    float DisplayAspect;  // Shape of the picture on the TV: 16:9 or 4:3. Use for projection.
    float PixelAspect;    // How wide one back buffer pixel looks on the TV (1.0 = square)
    float Unit;           // HUD scale, Height / 480 (the prototype's --u)
    RECT  SafeRect;       // 90% title-safe area; keep text and buttons inside it
};

extern ScreenInfo g_Screen;
