# Orbit

A homebrew launcher for modded Xbox 360 consoles. Orbit puts your original Xbox games, Xbox 360 games, Arcade titles and apps on 3D cover-flow shelves around a glowing orb, with a file manager and synthesised menu sounds.

> Orbit is a fan project. It is not made, endorsed or supported by Microsoft or Xbox.

## Status

Orbit is in development (v0.9). So far it has been tested on one console: an Xbox 360 S (Trinity) running BadUpdate.

- **Working:** original Xbox games, installed Xbox 360 games (Games on Demand), apps, FILES, box art.
- **Not tested yet:** Arcade games.
- **Not supported yet:** launching game discs.

If something doesn't work on your setup, please [open an issue](../../issues).

## Features

- **Main menu:** a ring of six items: XBOX, XBOX 360, ARCADE, APPS, FILES, SETTINGS.
- **Shelves:** 3D game cases (black for original Xbox, green for 360, orange for Arcade) with your box art. Games without art get a cover drawn for them.
  - **A** launch, **X** back cover, **Y** details (title, version, size, delete).
- **Launching:** loose `.xex` / `.xbe` files, installed 360 games (Games on Demand packages) and original Xbox games.
- **FILES:** browse `Hdd1`, `Usb0`–`Usb2` and `Dvd`, then copy, paste and delete. System folders are protected.
- **SETTINGS:** choose the folders Orbit scans, how deep it looks, and rescan.
- **Sorting:** items go on the right shelf by what they are, not by which folder they're in.

## Requirements

- An Xbox 360 that can run unsigned code, for example through BadUpdate, RGH or JTAG, with Dashlaunch.
- A FAT32 USB drive, or the console's hard drive.

## Install

1. Download the latest zip from [Releases](../../releases).
2. Copy `Orbit.xex` and the `Media` folder into one folder, for example `Usb0:\Apps\Orbit\`.
3. Launch `Orbit.xex` from your current dashboard. To boot straight into Orbit, set it as the default in Dashlaunch's `launch.ini`:
   ```ini
   Default = Usb:\Apps\Orbit\Orbit.xex
   ```
   Keep a button pointing at another dashboard, so you can still reach it by holding that button at boot:
   ```ini
   LBump = Usb:\Apps\Aurora\Aurora.xex
   ```
   Use a button you don't press in menus, such as LB or RB, not A or B. Dashlaunch also checks held buttons when you go back to Xbox Home, so if Aurora is on A, choosing Xbox Home with A opens Aurora instead of Orbit.

   If Orbit ever fails to start, it tries to open Aurora from `Usb0:\Apps\Aurora\Aurora.xex`, then the dashboard.

### Box art

Put covers in `Usb0:\Orbit\Covers\`, named by title ID, for example `4D5307E6.jpg` for Halo 3. Orbit expects full wrap-around scans of 900×600 pixels, laid out back | spine | front, like the ones on [xboxunity.net](https://xboxunity.net). JPG and PNG both work.

### Setting up your content paths

Orbit starts with no paths, so the shelves are empty until you tell it where your games are.

1. Open **SETTINGS** and choose **+ ADD PATH**.
2. Pick a drive, open folders with **A** until you're in the folder that holds your games, then press **Y** to add it.
3. With the new path selected, use **left / right** to set its **depth** (see below).
4. Repeat for each folder, then choose **SCAN NOW**.

Orbit puts each item on the right shelf by what it is, not by which folder it's in, so one path can fill several shelves.

#### Depth

Depth is how many folder levels Orbit looks down from the path. It stops going down as soon as it finds a game. Depth 1 means only the folder itself. Each extra level makes the scan a little slower, so use the smallest depth that reaches your games.

| What you're adding | Example layout | Depth |
|---|---|---|
| A folder of extracted 360 games | `Usb0:\Games\Halo 3\default.xex` | **2** |
| A folder of original Xbox games | `Hdd1:\Xbox\Halo\default.xbe` | **2** |
| A folder of apps (it must be named **Apps**) | `Usb0:\Apps\Aurora\Aurora.xex` | **2** |
| Installed games, Arcade and Games on Demand | `Hdd1:\Content\0000000000000000\<TitleID>\…` | **3** |
| The same, including games installed under a profile | `Hdd1:\Content` | **4** |

- Anything with a `.xex` inside a folder named **Apps** goes on the APPS shelf. Elsewhere, a `default.xex` counts as an Xbox 360 game.
- Installed content needs the same paths on each drive that has it, for example both `Hdd1:\Content\0000000000000000` and `Usb0:\Content\0000000000000000`.
- Removing a path: select it and press **X** twice. **DELETE ALL PATHS** clears the whole list (press **A** twice). Run **SCAN NOW** afterwards to update the shelves.

Orbit keeps its paths in `paths.txt` and the last scan in `library.txt`, next to `Orbit.xex`.

## Title updates and DLC

Games can be on any drive, but title updates and DLC have to be on the console's internal hard drive (`Hdd1`). The console's content system doesn't see a plain FAT32 USB drive.

| What | Where |
|---|---|
| Update files named `TU_…` (uppercase) | `Hdd1:\Cache\` |
| Update files named `tu…` (lowercase) | `Hdd1:\Content\0000000000000000\<TitleID>\000B0000\` |
| DLC | `Hdd1:\Content\0000000000000000\<TitleID>\00000002\` |

Pick the update that matches your copy's **media ID**, and take the highest version, because updates are cumulative.

## Building

You need Visual Studio 2010 and the Xbox 360 SDK (XDK). The SDK is Microsoft's and is not included here.

```
MSBuild OrbitTest.sln /p:Configuration=Release "/p:Platform=Xbox 360"
```

The shaders in `OrbitTest\Shaders` compile automatically before each build (`BuildShaders.cmd`).

The menu art in `Media\` is generated from `Tools\bake.html`. Run `node Tools/bake.js` to bake it; this uses Microsoft Edge in headless mode. `orbit-prototype.html` is the browser prototype the console version was built from.

## Credits

- Package launching uses the same method as Freestyle Dash.
- The orb's 3D simplex noise is Ashima Arts' / Ian McEwan's (MIT), ported to HLSL in `Shaders/Noise.hlsli`.
- Fonts: [Orbitron](https://github.com/theleagueof/orbitron), [Michroma](https://github.com/googlefonts/Michroma-font) and [Share Tech Mono](https://fonts.google.com/specimen/Share+Tech+Mono), all under the SIL Open Font License.

## License

Copyright (c) 2026 Noah Colbourne.

Orbit is free software under the [GNU General Public License v3.0](LICENSE) or later. You can share and change it, but if you share Orbit or anything built from it, you must keep this copyright notice, mark what you changed, and release your full source code under the same license.

Third-party parts keep their own licenses:
- The fonts in `Tools/fonts` are under the SIL Open Font License 1.1 (see the `OFL-*.txt` files beside them).
- The simplex noise in `OrbitTest/Shaders/Noise.hlsli` is Copyright (C) 2011 Ashima Arts, MIT License (notice in that file).
