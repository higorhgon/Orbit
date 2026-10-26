// Orbit, an Xbox 360 homebrew launcher. Copyright (c) 2026 Noah Colbourne.
// Licensed under the GNU General Public License v3.0 or later; see LICENSE.

// Sound.h : the prototype's audio, made the same way (synthesised, no sound files):
//
//   hum     two 55 Hz saws (one at 55.4, so they beat) and a 110.2 Hz sine through a
//           low-pass swept 400 +- 180 Hz by a 0.08 Hz LFO, fading in to 0.08 over 3s
//   move    triangle 660 Hz -> x1.2, 80ms,  volume 0.10
//   select  sine 440 Hz -> x2, 180ms, 0.18; then 60ms later sine 880 Hz -> x1.5, 250ms, 0.12
//   back    triangle 520 Hz -> x0.5, 150ms, volume 0.12
//   all through a master gain of 0.6
//
// The sounds are rendered into PCM buffers at start and played with XAudio2.
// SETTINGS can switch them all off (saved to game:\sound.txt).

#pragma once

enum { SOUND_MOVE, SOUND_SELECT, SOUND_BACK, NUM_SOUNDS };

// Starts the audio engine and the hum; quietly does nothing if audio can't start
void Sound_Init();
void Sound_Play( int nSound );

// ON / OFF for the hum and the blips; saved straight away
BOOL Sound_IsEnabled();
void Sound_SetEnabled( BOOL bEnabled );
