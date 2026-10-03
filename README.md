# Beetle VB OpenXR

Beetle VB OpenXR brings Virtual Boy games to PC VR through OpenXR. Our fork
combines the Beetle VB / Mednafen emulation core with a standalone player,
a stereoscopic virtual screen, and controls designed for use in a headset.

Maintained by **RototRobot**.

> [!NOTE]
> **This mod was made with heavy AI (Claude) assistance.** I want to be upfront
> about that.

Games appear on a screen positioned in your virtual space. Head tracking lets
you look around that screen while preserving the game's original stereo images.
The player also includes a desktop mode with a side-by-side view.

## Features

- Stereoscopic Virtual Boy emulation with audio and automatic cartridge saves.
- An in-headset ROM library with a cover-art grid and remembered folders.
- ROM and cover-art scanning through subfolders.
- Optional LaunchBox game-data and missing-cover downloads from the headset,
  with cached data available offline and no account or API key required.
- VR motion-controller, XInput gamepad, and keyboard controls.
- Controller remapping through a fourteen-input wizard or individual bindings.
- Eight color palettes: red, white, blue, cyan, electric cyan, green, magenta,
  and yellow, each against black.
- Adjustable screen width, distance, image zoom, and zoom filtering.
- Eye swapping, recentering, volume, mute, and an adjustable VR stick deadzone.
- Ten save-state slots per game.
- Saved preferences and automatic pause when the app loses focus.

## Requirements

- Windows x64 and a Direct3D 11-capable graphics card.
- For VR: a PC-connected headset and its active OpenXR runtime, such as SteamVR.
- VR controllers, an XInput gamepad, or a keyboard.
- Your own game files in `.vb`, `.vboy`, or `.bin` format. Extract ZIP archives
  before adding them to the library.

This is a Windows PC application. A standalone headset must connect to a PC
through its PC VR software to use it. No game ROMs or cover artwork are included.

## Getting started

Extract a release ZIP into a writable folder, or [build from source](#building-from-source).
For release packages, read `START HERE.txt` for prerequisites and launch steps.

1. Start your headset's PC VR software and make sure its OpenXR runtime is active.
   Select the desired Windows audio output device before launching the player.
2. Double-click **Run Virtual Boy VR.cmd** in the project folder.
3. Select **ROM Folder**, browse to your games, and choose **Use This Folder**.
   The library includes games in subfolders and remembers your selection.
4. Select a game and press the right VR trigger, gamepad A, or Enter to play.

Choose **Covers** to add local PNG, JPEG, or BMP artwork. Match each image's
filename to the ROM's filename, for example `Game Name.vb` and `Game Name.png`.
Artwork can be organized into subfolders. Select **Rescan** after changing the
contents of either folder. Or choose **Find Data**, then **Scan Missing Data**,
to match games and download missing covers from LaunchBox. The first scan
downloads about 103 MiB; later scans reuse the cache. Existing local artwork
takes priority. See the [library guide](docs/LIBRARY.md).

For desktop play, use **Run Virtual Boy Desktop.cmd**. To launch a game directly:

```powershell
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\Virtual Boy\Game Name.vb"
# Desktop mode
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\Virtual Boy\Game Name.vb" --desktop
```

## Default controls

### Keyboard and gamepad

| Virtual Boy input | Keyboard | XInput gamepad |
| --- | --- | --- |
| Left directional pad | W / A / S / D | D-pad or left stick |
| Right directional pad | Arrow keys | Right stick |
| A / B | K / J | A / B |
| L / R | Q / E | LB / RB |
| Start / Select | Enter / Space | Start / Back |

Keyboard controls require the player window to have focus. Controller input
requires the VR app to have focus, or the player window in desktop mode.

### VR controllers

These are the default bindings for the suggested OpenXR profiles. The active
runtime determines which profile and controls are available.

| Virtual Boy input | Touch-style | Valve Index | WMR / Vive wands |
| --- | --- | --- | --- |
| Left directional pad | Left stick | Left stick | Left stick / trackpad |
| Right directional pad | Right stick | Right stick | Right stick / trackpad |
| A / B | Right A / B | Right A / B | Right / left grip click |
| L / R | Left / right trigger | Left / right trigger | Left / right trigger |
| Start | Left Y | Left B | Right menu button |
| Select | Left X | Left A | Left menu button |

Vive trackpad directions require finger contact. VR stick deadzone defaults to
10% and can be adjusted in settings. See [VR controls](docs/VR_CONTROLLERS.md).

### Settings and library navigation

| Action | VR controllers | XInput gamepad | Keyboard |
| --- | --- | --- | --- |
| Open settings during a game | Both stick/pad clicks | Back + Start | F3 or Esc |
| Move selection | Either stick/pad | D-pad / sticks | Arrow keys / WASD |
| Adjust settings | Left / right | Left / right | Left / right |
| Select | Right trigger or default A button | A | Enter or K |
| Back / close | Left trigger or default B button | B | J or Esc |

Press and release between menu actions. Opening settings pauses the game and
audio. Choose **Game Library** to return to the cover grid and select another
game, or **Resume Game** to continue playing.

**Controller Mapping** opens the rebind wizard and individual bindings. VR,
XInput, and keyboard mappings save separately. Menu navigation retains its
default controls. See [remapping instructions](docs/CONTROLLER_MAPPING.md) and
[settings](docs/MENU.md).

### Desktop shortcuts

| Key | Action |
| --- | --- |
| F1 | Recenter the virtual screen |
| F2 | Swap eye images |
| P | Pause / resume |
| M | Mute / unmute |
| + / - | Increase / decrease screen width |
| [ / ] | Decrease / increase screen distance |
| F3 / Esc | Open / close settings |

## Saves and preferences

Cartridge saves are written automatically. Save states are managed through
**State Slot**, **Save State**, and **Load State** in settings; each game has ten
slots. After loading a state, choose **Resume Game**.

Data is stored beside `beetle_vb_openxr.exe`:

| File or folder | Contents |
| --- | --- |
| `settings.ini` | Display, audio, deadzone, and controller preferences |
| `library.ini` | Remembered ROM and cover folders |
| `game-data/` | Cached LaunchBox Virtual Boy metadata and downloaded covers |
| `saves/` | Cartridge save RAM |
| `states/` | Save-state slots |

For source builds, this location is `build/windows/Release/`. Keep the player in
a writable folder and back up `saves/` and `states/` before moving or updating it.
Saves belong to ROM contents, so renaming a game preserves its saves.
Save states require the same ROM and core DLL; updating the core can make older
states incompatible. Loading a state also restores its cartridge save RAM.
See [save states](docs/SAVE_STATES.md) and [player options](docs/PLAYER.md).

## Building from source

Install Git, CMake 3.24 or newer, and Visual Studio 2022 or its Build Tools with
**Desktop development with C++** and a Windows SDK. Download or clone this
repository, then open a Developer PowerShell or Developer Command Prompt in
its root folder.

```powershell
cmake --preset windows
cmake --build --preset windows --parallel
ctest --preset windows
```

The first configure needs internet access to fetch the Khronos OpenXR SDK at
`release-1.1.54`. The loader is built with the application. The Release output
includes `beetle_vb_openxr.exe` and `mednafen_vb_libretro.dll` in
`build/windows/Release/`; keep them together. A headset is needed for VR play,
but not for building or running the automated tests.

For a core-only build without downloading OpenXR:

```powershell
cmake --preset windows-offline
cmake --build --preset windows-offline --parallel
ctest --preset windows-offline
```

The offline preset builds the emulator core and its supporting tests; use the
`windows` preset to build the VR and desktop player. The upstream Makefile is
also available for libretro core builds.

## Help

See [player options and audio setup](docs/PLAYER.md),
[library and cover matching](docs/LIBRARY.md), and
[troubleshooting](docs/VALIDATION.md). To report a problem, include the game,
headset/controller, OpenXR runtime, and relevant console output.

## Licensing

Our frontend code is licensed under **GPL-2.0-or-later**. The Beetle VB / Mednafen
core and bundled helpers retain their original copyright and license notices.
See [COPYING](COPYING) and [third-party notices](THIRD_PARTY_NOTICES.md).

The main Khronos OpenXR headers and loader are dual licensed under Apache-2.0
or MIT; this distribution uses the MIT option. Other components retain their
individual notices. ROMs and cover artwork remain separate from the
project and are subject to their owners' terms.

## Thanks

- The **Beetle VB, Mednafen, and libretro contributors** for the emulator core
  and the foundations this fork builds on:
  [Beetle VB](https://github.com/libretro/beetle-vb-libretro).
- **Khronos and the OpenXR contributors** for the standard, SDK, and loader.
- **LaunchBox and its Games Database contributors** for game metadata and
  cover art, and the **miniz contributors** for the ZIP reader.
- The **vbjin-ovr contributors** for their earlier work bringing Virtual Boy
  emulation to VR and the inspiration it provides.
- Everyone who tests the player, reports issues, and contributes improvements.
