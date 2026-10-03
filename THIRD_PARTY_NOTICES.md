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

## miniz

The frontend includes miniz 3.1.2 for reading LaunchBox metadata ZIP files.
miniz is MIT licensed; its copyright notices and license are in
`third_party/miniz/LICENSE`, with provenance in `third_party/miniz/PROVENANCE.md`.
Binary packages include the license.

## LaunchBox Games Database

Optional game metadata and cover downloads come from the
[LaunchBox Games Database](https://gamesdb.launchbox-app.com/). Thanks to
LaunchBox and its community contributors. Downloaded metadata and artwork are
separate from the emulator's GPL source; artwork retains its owners' rights.
Database records and covers are downloaded by users and are not bundled with
the emulator or its source packages.
