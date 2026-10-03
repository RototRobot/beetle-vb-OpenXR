# Source provenance

## Emulator core

Upstream: [libretro/beetle-vb-libretro](https://github.com/libretro/beetle-vb-libretro).

The repository's baseline is a source archive imported on 2026-10-03. The archive
contains no upstream Git metadata, so its upstream commit identifier is unknown.
Imported copyright and license notices remain in the source tree and
[COPYING](../COPYING).

## Frontend and dependencies

The standalone host, OpenXR presentation, menus, and input code are in
`frontend/`. The Khronos OpenXR SDK is fetched at the fixed tag `release-1.1.54`.
Its notices accompany the fetched source.

vbjin-ovr is a historical reference for Virtual Boy emulation in VR. Its source,
binaries, and Oculus SDK are not part of this repository.

See [third-party notices](../THIRD_PARTY_NOTICES.md) for component licensing.
