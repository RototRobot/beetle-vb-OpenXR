# VR controller guide

OpenXR controller input is enabled automatically. Suggested bindings cover
Touch-style, Valve Index, Microsoft motion controller, and HTC Vive controller
profiles. The runtime selects the active profile; available controls depend on
that profile. See the [default binding tables](../README.md#vr-controllers).

## Gameplay

Each stick or trackpad controls the corresponding Virtual Boy directional pad.
Diagonal directions are supported. Vive trackpads require finger contact and
return to neutral when released.

**VR Stick Deadzone** in settings defaults to 10% and ranges from 5% to 60% in
5% steps. Raise it for less sensitive movement. Directions release below three
quarters of the chosen value. Triggers and analog grips use 55% press and 45%
release thresholds. XInput has a separate fixed stick threshold of about 25%.

Use **Controller Mapping** in settings to change game controls. The wizard
assigns all fourteen inputs; individual rows change one input. See the
[remapping guide](CONTROLLER_MAPPING.md).

## Menu navigation

- Press both stick/pad clicks together to open or close settings.
- Move either stick/pad up/down to select a row and left/right to adjust values.
- Use the right trigger or the default A control to select.
- Use the left trigger or the default B control to go back or close.

Return the stick to center between gestures. Menu navigation stays on the
chosen axis until centered, making vertical selection less sensitive to
sideways movement. Game and audio pause while settings are open.

Menu and library controls retain their defaults when gameplay is remapped.
During binding capture, the left trigger and B can be assigned; use the settings
chord to cancel instead.

## Focus and reconnecting

Release controls briefly after starting, reconnecting a controller, or returning
from the runtime dashboard. Held controls are suppressed until released.
System/dashboard buttons are handled by the runtime.

If a controller does not respond, check the console's
`Controller left/right profile: ...` messages. A value of `none` means that hand
has no active interaction profile. Include these messages, your controller
model, and your OpenXR runtime when reporting an input problem.

Keyboard and XInput are also available. To disable VR controller actions:

```powershell
.\build\windows\Release\beetle_vb_openxr.exe "C:\Games\example.vb" --no-vr-input
```
