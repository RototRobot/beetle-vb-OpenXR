# In-headset settings menu

Launch **Run Virtual Boy VR.cmd** and select a game as usual. Open settings with
**both stick/pad clicks together** on VR controllers, **Back+Start together**
on an Xbox-style XInput controller, or **F3 / Esc** with
the player desktop window focused. The menu appears on the virtual screen in
both eyes. Game emulation and audio pause while it is open.

| Action | XInput controller | Keyboard |
| --- | --- | --- |
| Move between rows | D-pad / left stick | Up / down arrows |
| Change a value | Left / right | Left / right arrows |
| Select / increase value | A | Enter or K |
| Close | B or Back+Start | J, F3 or Esc |

Press and release for each step; holding a control does not repeatedly change a
setting. Gameplay controls stay suppressed until released after closing.
On VR controllers, either stick/pad navigates, right trigger selects and left
trigger closes. See [full gameplay bindings](VR_CONTROLLERS.md).

Available settings:

- **Palette:** red, white, blue, cyan, electric cyan, green, magenta or yellow,
  each against black. The chosen palette takes effect after resuming the game.
- **Screen width / distance:** physical width 0.3â€“3 metres and distance 0.5â€“5
  metres. Height follows the original aspect ratio.
- **Image zoom:** 50â€“200% within the screen. Below 100% adds black borders;
  above 100% crops the game image. Use 100% to show the complete native image.
- **Zoom filter:** crisp nearest-pixel or smooth bilinear zoom resampling.
  At 100% both preserve the native pixels. Physical screen sampling follows the
  OpenXR runtime's compositor.
- **VR Stick Deadzone:** 10% by default, adjustable 5â€“60% in 5% steps. Applies
  immediately to both VR sticks/pads and is remembered. Menu gestures stay on
  their chosen axis until centred, so navigating rows does not also change a
  value. Gameplay diagonals remain available. XInput deadzone is unchanged.
- **Swap eyes / recenter:** swap stereo images or place the screen in front of
  the current head position and direction.
- **Volume / mute:** volume changes in 5% steps; audio resumes with these values.
- **State Slot / Save State / Load State:** ten slots per game, with EMPTY/USED
  indicators and results in the menu. Loads stay paused until Resume.
  See [save-state guide](SAVE_STATES.md).
- **Controller Mapping:** open the rebind wizard or change individual inputs.
  See [capture controls and persistence](CONTROLLER_MAPPING.md).
- **Game Library:** save cartridge RAM and return to the cover grid to select
  another game. See [library controls](LIBRARY.md).
- **Resume Game / Exit Player:** resume clears a manual P pause as well; closing
  with B or the toggle preserves any manual pause. Exit saves cartridge RAM.

Preferences save automatically to `settings.ini` beside the executable
(`build/windows/Release/settings.ini` for a source build). Reopening the player loads
them. `--settings-file path` chooses another location. Removing that file resets
preferences. Command-line width/distance/volume/eye-swap values override the
loaded values; subsequent menu changes save the current preferences.
