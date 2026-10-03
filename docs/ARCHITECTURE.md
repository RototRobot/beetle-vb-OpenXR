# Architecture reference

## Emulator and player

Beetle VB OpenXR uses the libretro API between the emulator core and standalone
player. Imported emulation sources are in `mednafen/`, `libretro-common/`, and
the root libretro files. The player is in `frontend/`.

`CoreHost` loads the core DLL and manages ROM contents, callbacks, native stereo
frames, audio, and persistence. One core host runs at a time because the core
uses process-global state. The library switches cartridges within one player
and OpenXR session.

## Stereo presentation

The core uses side-by-side output with zero separation. Visible 768x224
XRGB8888 frames are split into two owned 384x224 RGBA images using the reported
row pitch. Null video callbacks reuse the previous frame.

The OpenXR backend uses Direct3D 11 on the runtime-selected GPU. Separate
left/right quad layers share a screen pose in local space. Head tracking changes
the viewpoint of the virtual screen; the game retains its original cameras and
stereo imagery. Recenter updates the screen pose.

Native game/settings frames use 384x224 swapchains. Library frames use a separate
1536x896 pair. Frame metadata selects the matching upload pitch, image extent,
and swapchain pair. The desktop preview packs both eyes side by side.

## Timing and audio

The emulation clock follows the core's reported frame rate independently of
headset refresh. Pause, focus changes, and content transitions reset timing
accumulation. XAudio2 receives stereo PCM and uses the Windows default playback
device selected at startup. Pause discards queued audio.

## Input and menus

Keyboard, XInput, and OpenXR actions publish physical sources. Stable source IDs
map to fourteen Virtual Boy inputs in separate device-family maps. Default
menu/library navigation stays independent of gameplay remapping. Opposed game
directions cancel; VR menu gestures lock to one axis until centered.

Binding capture waits for neutral controls, consumes one source, and requires
release before another wizard step. Focus/profile/reconnect gates suppress held
VR controls. Settings and library revisions refresh paused screens without
requiring an emulated frame.

## Library and persistence

ROM and artwork trees are scanned recursively, with a 10,000-entry limit per
root. Inaccessible descendants and Windows reparse points are skipped. ROMs
retain full paths. Covers match unique exact stems, then unique normalized
titles. Windows Imaging Component decodes visible covers into a bounded cache.

Preferences use `settings.ini`; remembered folders use `library.ini`. Cartridge
saves and state slots are keyed by ROM contents. Save-state envelopes include
ROM/core identities, bounded payloads, native eye images, and a checksum. A
rollback checkpoint protects the active game when the core rejects a state.
Writes use temporary sibling files and atomic replacement.

## Build and checks

CMake presets build the imported core, frontend libraries, executables, and
CTest suite. OpenXR SDK release 1.1.54 supplies the loader and headers. The
offline preset builds the core and supporting tests without OpenXR. Source
provenance and licensing are in [PROVENANCE.md](PROVENANCE.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md).
