# Stereo screen diagnostic

Use the stereo diagnostic to check eye order, image orientation, depth, and
screen positioning without a game. Start your PC OpenXR runtime and connect
your headset, then run from the project root:

```powershell
.\build\windows\Release\bvb_stereo_test.exe
```

The default screen is 1.2 metres wide and 2 metres in front of your initial head
position. It remains stationary as you move your head.

## Controls

Focus the desktop diagnostic window for keyboard controls. These shortcuts
belong to the diagnostic; the game player uses F1/F2 for recentering and eye swap.

| Key | Action |
| --- | --- |
| R | Recenter the screen in front of you |
| S | Swap source images between eyes |
| + / - | Increase / decrease screen width |
| [ / ] | Decrease / increase screen distance |
| Esc or window close | Exit |

## Reading the pattern

- With your right eye closed, the upper-left label reads LEFT in red.
- With your left eye closed, it reads RIGHT in cyan.
- TOP is above the central cross and BOTTOM below it.
- Both eyes show red, green, and blue bars in that order at the bottom.
- The white cross/grid lies on the screen plane. The upper green target appears
  closer, and the lower magenta target farther away. Swapping eyes reverses
  their depth and swaps the labels.
- Recenter while looking in another direction to reposition the screen.

The desktop window shows source images side by side. To view the pattern without
VR or export an image:

```powershell
.\build\windows\Release\bvb_stereo_test.exe --preview-only
.\build\windows\Release\bvb_stereo_test.exe --export-pattern .\build\pattern.bmp
```

Optional launch arguments include `--width 1.5`, `--distance 2.5`, `--swap-eyes`,
and `--seconds 60`. Use `--help` for the full command-line summary.

If an image is missing or the diagnostic fails to start, see
[troubleshooting](VALIDATION.md). Include the console output, headset, runtime,
and GPU details when reporting a problem.
