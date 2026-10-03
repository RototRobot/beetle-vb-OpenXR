# Controller remapping

Open settings during a game and select **Controller Mapping**. The submenu has
**Rebind Wizard**, all fourteen Virtual Boy inputs, then **Back**.

- **Rebind Wizard:** release the selecting control, then press a button or move
  a stick in the direction you want for the highlighted input. It advances
  through both directional pads, A, B, L, R, Start and Select. Release all
  controls between steps. The first binding chooses the device family for the
  whole wizard.
- **Individual mapping:** select a Virtual Boy input, release all controls, then
  press its replacement. This changes only that input on the device used.
- **Back:** returns to the main settings screen. Emulation and audio stay paused.

Left/right outside capture switches the displayed mappings between VR,
XInput and keyboard. Their bindings are saved separately; all remain usable
for gameplay. Changes save automatically in the `settings.ini`.
Multiple Virtual Boy inputs
can share one button; opposite directions still cancel each other. A default
row with `+` accepts either listed control, rather than requiring a chord.
Rebinding replaces both alternatives with the selected control.

Menu and library navigation always use the original controls. While capturing,
B and the left trigger are available for assignment. To cancel capture, use
**both VR stick/pad clicks**, **XInput Back+Start**, or **Esc/F3**. Completed wizard
steps are kept. To keep the current binding and skip a step, press **both VR
triggers together**, **XInput LB+RB together**, or **F4**. A held control never
advances multiple steps. Simultaneous inputs require release and a fresh press.

VR capture supports stick/pad directions, triggers, available face/menu buttons,
individual stick/pad clicks, and grips. Touch/Index squeeze values use 55% press
and 45% release thresholds; WMR/Vive grips use their existing click actions.
VR labels include profile aliases: left X/A/menu and Y/B/menu, and A/B/grip
labels depend on the active interaction profile. Only controls exposed by that
profile can be assigned.

XInput supports the D-pad, both sticks and clicks, A/B/X/Y, shoulders, triggers,
Start and Back. Keyboard capture supports letters, digits, numpad digits,
arrows, Enter, Space, Tab, Backspace, navigation keys, Insert and Delete while
the player window has focus. Existing window shortcuts, including P/M and
function keys, are reserved. Settings are shared across games and controller
models within each device family. Removing only the `bind_...` lines from
`settings.ini` while the player is closed restores default bindings without
resetting display preferences.
