# Third-party notices

## Beetle VB / Mednafen

The emulator core comes from [libretro/beetle-vb-libretro](https://github.com/libretro/beetle-vb-libretro),
which incorporates Mednafen's Virtual Boy emulation. Mednafen source includes
GPL-2.0-or-later notices. The root [COPYING](COPYING) contains the GNU General
Public License, version 2. Individual source files retain their original
copyright and license notices.

## Libretro headers and compatibility helpers

`libretro-common/` contains libretro headers and supporting code. These files
retain their individual copyright and license notices.

## Khronos OpenXR SDK

CMake fetches KhronosGroup/OpenXR-SDK at `release-1.1.54` and builds its loader.
The main OpenXR headers and loader use Apache-2.0 OR MIT; this distribution
uses their MIT option. Other SDK components retain their individual terms.
Additional component notices and licenses
are included in the fetched SDK tree under `build/windows/_deps/openxr_sdk-src/`.
Redistributions must retain the applicable notices and license files for the
components included in the distribution.

## Frontend

The Beetle VB OpenXR frontend uses GPL-2.0-or-later, as identified in its source
headers. See [COPYING](COPYING). Source provenance is recorded in
[PROVENANCE.md](docs/PROVENANCE.md).

## JsonCpp

The OpenXR loader includes JsonCpp by Baptiste Lepilleur and the JsonCpp Authors,
under its public-domain/MIT terms. Its license is included in binary packages.
