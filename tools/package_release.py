# SPDX-License-Identifier: GPL-2.0-or-later
"""Package an already-built Windows Release from a clean source checkout."""
import argparse
import hashlib
import io
import json
import re
import subprocess
import zipfile
from pathlib import Path


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def source_entries(root, prefix):
    archive = git(root, 'archive', '--format=zip', 'HEAD')
    with zipfile.ZipFile(io.BytesIO(archive)) as source:
        return {prefix + name: source.read(name) for name in source.namelist()
                if not name.endswith('/')}


def archive_bytes(entries):
    output = io.BytesIO()
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    return output.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', default='build/windows')
    parser.add_argument('--output-dir', default='build/packages')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if git(root, 'status', '--porcelain').strip():
        raise SystemExit('Commit source changes before creating a matching release.')
    build = (root / args.build_dir).resolve()
    sdk = build / '_deps/openxr_sdk-src'
    if git(sdk, 'status', '--porcelain').strip():
        raise SystemExit('The OpenXR SDK checkout must be unmodified.')
    version = re.search(r'project\(beetle_vb_openxr VERSION ([\d.]+)',
                        (root / 'CMakeLists.txt').read_text()).group(1)
    commit = git(root, 'rev-parse', 'HEAD').decode().strip()
    sdk_commit = git(sdk, 'rev-parse', 'HEAD').decode().strip()
    name = f'Beetle-vb-OpenXR-{version}-windows-x64'
    entries = {}
    for binary in ['beetle_vb_openxr.exe', 'mednafen_vb_libretro.dll',
                   'bvb_stereo_test.exe', 'bvb_xr_probe.exe']:
        entries[binary] = (build / 'Release' / binary).read_bytes()
    for filename, desktop in [('Run Virtual Boy VR.cmd', False),
                              ('Run Virtual Boy Desktop.cmd', True)]:
        extra = ' --desktop' if desktop else ''
        launcher = ('@echo off\r\nsetlocal\r\n'
                    f'"%~dp0beetle_vb_openxr.exe"{extra} %*\r\n'
                    'if errorlevel 1 pause\r\n')
        entries[filename] = launcher.encode('ascii')
    for filename in ['README.md', 'THIRD_PARTY_NOTICES.md', 'COPYING']:
        entries[filename] = (root / filename).read_bytes()
    for filename in ['PLAYER.md', 'LIBRARY.md', 'MENU.md', 'VR_CONTROLLERS.md',
                     'CONTROLLER_MAPPING.md', 'SAVE_STATES.md', 'HEADSET_TEST.md',
                     'VALIDATION.md', 'PROVENANCE.md']:
        entries['docs/' + filename] = (root / 'docs' / filename).read_bytes()
    # Portable instructions use executables/data beside the launchers.
    for filename in list(entries):
        if filename.endswith('.md'):
            text = entries[filename].decode('utf-8')
            text = text.replace('.\\build\\windows\\Release\\', '.\\')
            entries[filename] = text.encode('utf-8')
    for filename in ['MIT.txt', 'Apache-2.0.txt', 'CC-BY-4.0.txt']:
        entries['licenses/OpenXR-' + filename] = (sdk / 'LICENSES' / filename).read_bytes()
    entries['licenses/OpenXR-COPYING.adoc'] = (sdk / 'COPYING.adoc').read_bytes()
    entries['licenses/JsonCpp-LICENSE.txt'] = (sdk / 'src/external/jsoncpp/LICENSE').read_bytes()
    entries['licenses/miniz-LICENSE.txt'] = (root / 'third_party/miniz/LICENSE').read_bytes()
    copyrights = set()
    for folder in ['src/loader', 'src/common', 'include/openxr']:
        for path in (sdk / folder).rglob('*'):
            if path.suffix not in ['.h', '.hpp', '.c', '.cpp']:
                continue
            copyrights.update(re.findall(r'(?im)^[/*\s]*(Copyright[^\r\n]+)',
                                         path.read_text(encoding='utf-8')))
    entries['licenses/OpenXR-NOTICES.txt'] = (
        'OpenXR loader, supporting code, and API headers\n'
        'Copyright notices; individual source headers accompany the matching source.\n\n'
        + '\n'.join(sorted(copyrights)) + '\n'
    ).encode('utf-8')
    entries['START HERE.txt'] = f'''Beetle VB OpenXR {version} - Windows x64
Maintained by RototRobot

1. Extract this entire ZIP into a writable folder. Keep the EXE and core DLL
   together. Do not run the player from inside the ZIP.
2. Install the Microsoft Visual C++ v14 x64 Redistributable if needed:
   https://aka.ms/vc14/vc_redist.x64.exe
   Microsoft information:
   https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist
3. Start your PC VR software and make sure its OpenXR runtime is active.
   Select your headset's Windows audio output before launching.
4. Run "Run Virtual Boy VR.cmd". For desktop play, use the Desktop launcher.
5. Choose ROM Folder, browse to your games, and select Use This Folder.
   Subfolders are included. Choose Cover Folder for your PNG/JPEG/BMP images.
   ROMs must be extracted .vb, .vboy, or .bin files.

VR menus: either stick/pad moves, right trigger selects, left trigger goes back.
During gameplay, both stick/pad clicks open settings. XInput: Back+Start.
Keyboard: F3/Esc. See README.md and docs/ for default controls and remapping.

Settings, library folders, saves, and states are created beside the executable.
Keep this folder writable and back up saves/ and states/ before updating.
No games, cover art, personal settings, or save files are included.

Matching source and the pinned OpenXR SDK are included in Source/. Extract the
source ZIP to build or modify it; follow SOURCE BUILD.md inside that archive.
COPYING and THIRD_PARTY_NOTICES.md describe licensing. Keep these notices and
the matching source when redistributing this package.

Version: {version}
Source commit: {commit}
OpenXR SDK: release-1.1.54 ({sdk_commit})
'''.encode('utf-8')
    sources = source_entries(root, 'project/')
    sources.update(source_entries(sdk, 'OpenXR-SDK/'))
    sources['SOURCE BUILD.md'] = f'''# Matching source for Beetle VB OpenXR {version}

Project commit: `{commit}`. OpenXR SDK release-1.1.54: `{sdk_commit}`.
The `project/` folder contains the frontend/core, build files, and licenses.
`OpenXR-SDK/` contains the exact loader/headers and bundled dependencies.

Install Visual Studio 2022 C++ Build Tools, a Windows SDK, and CMake 3.24+.
Open a Developer PowerShell in `project/`, then run:

```powershell
cmake --preset windows -DFETCHCONTENT_SOURCE_DIR_OPENXR_SDK=../OpenXR-SDK
cmake --build --preset windows --parallel
ctest --preset windows
```

The SDK override uses the included dependency source without a Git download.
The Release binaries are in `project/build/windows/Release/`. Source-tree
launchers run those binaries. See the project README for controls and licensing.
'''.encode('utf-8')
    source_name = f'Beetle-vb-OpenXR-{version}-source.zip'
    entries['Source/' + source_name] = archive_bytes(sources)
    hashes = {filename: hashlib.sha256(data).hexdigest()
              for filename, data in sorted(entries.items())}
    entries['release-manifest.json'] = json.dumps({
        'version': version, 'platform': 'windows-x64', 'maintainer': 'RototRobot',
        'source_commit': commit, 'openxr_sdk_commit': sdk_commit, 'sha256': hashes
    }, indent=2).encode('utf-8') + b'\n'
    # Inspect names and bytes before writing; preserve upstream copyright names.
    private_paths = re.compile(rb'(?i)(?:(?:[a-z]:[\\/]+Users[\\/]|/Users/|/home/)[a-z0-9_.-]+[\\/]|codex-clipboard-[0-9a-f]{8}-)')
    for filename, data in {**entries, **sources}.items():
        if private_paths.search(filename.encode()) or private_paths.search(data):
            raise SystemExit('Private path found in package entry: ' + filename)
        if private_paths.search(data.replace(b'\0', b'')):
            raise SystemExit('Private wide-character path found in: ' + filename)
    output = (root / args.output_dir).resolve()
    output.mkdir(parents=True, exist_ok=True)
    path = output / (name + '.zip')
    if path.exists():
        raise SystemExit('Package already exists; choose a new output directory.')
    payload = archive_bytes({name + '/' + filename: data for filename, data in entries.items()})
    path.write_bytes(payload)
    checksum = hashlib.sha256(payload).hexdigest()
    path.with_suffix('.zip.sha256').write_text(checksum + '  ' + path.name + '\n', encoding='ascii')
    print(f'Created {path.name}: {len(payload):,} bytes; {len(entries)} package files.')
    print(f'SHA-256: {checksum}')


if __name__ == '__main__':
    main()
