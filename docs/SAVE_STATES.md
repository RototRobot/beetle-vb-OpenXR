# Save states

Open settings with both VR stick/pad clicks, XInput Back+Start, or F3/Esc.

1. Select **State Slot** and use left/right to choose one of ten slots.
   The row shows EMPTY or USED for the current game.
2. Select **Save State** with the right VR trigger, gamepad A, or Enter.
   Saving replaces the selected slot; the status line shows the result.
3. Select **Load State** to restore a slot, then choose **Resume Game**.
   Release menu controls before continuing.

The selected slot is remembered. Save states belong to ROM contents, so renaming
or moving a game preserves its slots. Files are stored in `states` beside the
player executable (`build/windows/Release/states` for a source build). The folder
is created when you first save. Use `--state-dir folder` to choose another location.

## Loading and compatibility

A state restores the game and its cartridge save RAM. That restored RAM will be
written to the normal cartridge save, so loading an older state can replace
more recent in-game progress. Back up saves if you want to retain both.
Display, controller, and audio preferences keep their current values.

States require the same ROM contents and core DLL. Updating or rebuilding the
core can make older states incompatible. These files are specific to Beetle VB
OpenXR. Keep backups of cartridge saves before updating the application.

An empty slot shows EMPTY. Failed saves or loads show a message in the headset
and details in the console. Invalid state files are rejected. If restoration
fails, the player attempts to return to the current game; if that recovery also
fails, it exits without overwriting cartridge save RAM for that session.
