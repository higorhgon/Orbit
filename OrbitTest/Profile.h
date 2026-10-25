// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Profile.h : who is signed in, and the system screens (sign-in, keyboard, guide).
//
// Listens for the system's notifications: when a profile signs in, out or switches,
// it reads the first signed-in profile's gamertag and gamer picture and hands them to
// the HUD's profile badge (hidden when nobody is signed in). Only local profiles are
// asked for; nothing here goes online.

#pragma once

HRESULT Profile_Init( D3DDevice* pDevice );
// Once a frame: reads the system's notifications
void    Profile_Update( D3DDevice* pDevice );

// The first signed-in profile's user index (checked now, not from the last
// notification), or -1 when nobody is signed in
int     Profile_SignedInUser();

// TRUE while a system screen (guide, sign-in, keyboard) is over Orbit
BOOL    Profile_IsSystemUIOpen();

// Opens the system's sign-in screen, local profiles only. FALSE if it couldn't.
BOOL    Profile_ShowSignIn();
