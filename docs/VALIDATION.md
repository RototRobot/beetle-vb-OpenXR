# Troubleshooting and diagnostics

## The VR player will not start

Start your headset's PC VR software, connect the headset, and confirm the
intended OpenXR runtime is active. Keep `beetle_vb_openxr.exe` and
`mednafen_vb_libretro.dll` together. Run the runtime diagnostic from the project
root for details:

```powershell
.\build\windows\Release\bvb_xr_probe.exe
```

`XR_ERROR_RUNTIME_UNAVAILABLE` means the OpenXR loader could not initialize the
selected runtime. Check the runtime's own setup and console messages. Restart
its PC VR software and launch the player again from your normal desktop session.

Use **Run Virtual Boy Desktop.cmd** or `--desktop` to use the library and player
without a headset.

## Games or covers are missing

Confirm the selected ROM and cover folders in the library toolbar, then select
**Rescan**. ROMs must be extracted `.vb`, `.vboy`, or `.bin` files. Images must be
PNG, JPEG, or BMP. Subfolders are included; junctions and symbolic links are
skipped. Read [cover matching](LIBRARY.md#cover-matching) if an image shows a
placeholder. Duplicate matching images are ambiguous even in different folders.

## Controls are not responding

Keep the player window focused for keyboard controls. VR and gamepad controls
require application focus. Release buttons and sticks after returning from the
runtime dashboard or reconnecting a controller. Check the profile messages in
the console and the [VR controller guide](VR_CONTROLLERS.md).

Menu controls keep their defaults even after gameplay remapping. Use
**Controller Mapping** to review the active device family's assignments.

## Audio is missing or uses the wrong device

Check Windows' selected output device before starting the player. Check **Mute**
and **Volume** in settings, and restart the player after changing the Windows
output device. The console includes audio-device errors.

## Saves do not persist

Keep the executable and its data folders in a writable location. Check the
console for save errors and close the player normally. Back up `saves/` and
`states/` before replacing files. Save states require the same ROM and core DLL;
see the [save-state guide](SAVE_STATES.md).

## Stereo image check

The [stereo screen diagnostic](HEADSET_TEST.md) displays a generated pattern
for checking eye order, colors, and depth without a game. The desktop window
shows source images side by side rather than a capture of the headset view.

## Reporting a problem

Include the application version or commit, Windows/GPU details, headset and
controller models, OpenXR runtime, game title, steps to reproduce, and relevant
console output. Describe whether the issue happens in VR, desktop mode, or both.
Do not include ROM files.

## Source-build checks

```powershell
ctest --preset windows
```

The automated suite covers stereo buffers, core loading, generated cartridge
execution, input, remapping, save states, settings, and library behavior. Tests
use generated fixtures and do not require game files or a headset. Physical
headset behavior is checked separately with the stereo diagnostic and gameplay.
