# Player guide

Launch **Run Virtual Boy VR.cmd** for VR or **Run Virtual Boy Desktop.cmd** for
a desktop side-by-side view. Both open the game library when launched without
a ROM argument. See [library setup](LIBRARY.md) and
[default controls](../README.md#default-controls).

## Launch options

Run commands from the project root after building:

```powershell
.\build\windows\Release\beetle_vb_openxr.exe
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb"
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb" --desktop
```

| Option | Purpose |
| --- | --- |
| `--desktop` | Play without an OpenXR headset |
| `--rom-dir folder` | Choose a ROM library folder |
| `--boxart-dir folder` | Choose a local cover folder |
| `--width metres` | Set screen width, 0.3–3 metres |
| `--distance metres` | Set screen distance, 0.5–5 metres |
| `--swap-eyes` | Swap the stereo images |
| `--volume value` | Set volume from 0 to 1 |
| `--no-audio` | Disable audio output |
| `--no-vr-input` | Use keyboard/gamepad input without VR controller actions |
| `--settings-file path` | Choose a preferences file; `library.ini` uses the same folder |
| `--save-dir folder` | Choose a cartridge-save folder |
| `--state-dir folder` | Choose a save-state folder |
| `--core path` | Choose a compatible Beetle VB libretro core DLL |
| `--seconds duration` | Exit after the specified number of seconds |
| `--start-menu` | Open settings when starting a game |
| `--help` | Show command-line help |

For example:

```powershell
.\build\windows\Release\beetle_vb_openxr.exe --rom-dir "C:\Games\VB" --boxart-dir "C:\Games\VB Covers"
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb" --save-dir "C:\Games\VB Saves"
```

## Display and focus

The desktop window shows the left and right source images side by side.
Screen width, distance, recentering, and eye order are adjustable through
[settings](MENU.md) or the desktop shortcuts in the README.

Keyboard input requires the player window to have focus. Controller input
requires VR focus, or window focus in desktop mode. Opening the headset runtime's
dashboard pauses gameplay and audio. Release held controls when returning.
Choose **Resume Game** in settings to clear a manual P pause.

## Audio

The player uses the Windows default output device selected at launch. Choose
your headset or speakers in Windows before starting it. Adjust **Volume** and
**Mute** in settings, or use M to toggle mute with the player window focused.
Restart the player after changing the output device if sound remains on the
previous device. Audio errors appear in the console.

```powershell
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb" --volume 0.5
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb" --no-audio
```

## Cartridge saves

Cartridge save RAM loads automatically and saves every 30 seconds, on normal
exit, and when returning to the library. The default folder is `saves` beside
the executable. Close the player normally to save your latest progress.

Save files use a ROM-content identifier, so renaming or moving the same ROM
keeps its save. A ROM with different contents uses a different save. An existing
save with an unexpected size causes loading to fail and is preserved; back it
up before investigating the console message.

Cartridge saves and [save states](SAVE_STATES.md) use separate files. Loading a
state restores the cartridge RAM stored in that state, which can replace more
recent in-game progress when the cartridge save is next written.

## Switching games and exiting

Open settings and choose **Game Library** to return to the cover grid. Cartridge
RAM is saved before switching. If it cannot be saved, the game stays paused and
a message appears. Select **Exit Player** or close the window to exit normally.
