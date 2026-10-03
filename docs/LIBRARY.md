# Game library and cover art

Launch **Run Virtual Boy VR.cmd** without a ROM argument to open the in-headset
library. Desktop mode uses the same library and controls.

## Choose your folders

1. Select **ROM Folder** with the right VR trigger, gamepad A, or Enter.
2. Use a stick/pad or arrows to browse. Select a folder to enter it.
   **Up One Level / Drives** reaches parent folders and then drive letters.
   Left also goes up; left trigger, B, or Esc cancels.
3. Select **Use This Folder** in the folder containing your games.
4. Select **Covers** and repeat these steps for local artwork, or use **Find Data**
   to download missing covers from LaunchBox.
5. Select a game tile and press the right trigger, A, or Enter to play.

Folders are remembered in `library.ini` beside `settings.ini`. Nearby folders
named `Roms` and `Boxart` can be suggested automatically; confirm your choices
with **Use This Folder**.

## Scanning and navigation

The library scans `.vb`, `.vboy`, and `.bin` files in the selected ROM folder and
all its subfolders. Extract ZIP archives first. Select **Rescan** after adding
or removing games or artwork.

Six covers appear per page in three columns. Up/down moves by row; left/right
moves between games and pages. Titles are centered below each cover, with the
selected title and game count below the grid. Duplicate ROM filenames remain
separate entries and launch from their original paths.

Left trigger, B, or Esc returns to the toolbar. Up from the first row also
reaches it. Left/right chooses a toolbar action, and down returns to the games.
Folder lists scroll as you move the selection.

Scans skip junctions, symbolic links, and inaccessible subfolders. Each selected
folder tree has a limit of 10,000 entries; choose a more specific folder for a
larger collection. If the selected ROM folder is unavailable, choose another
folder from the toolbar.

## Cover matching

PNG, JPEG, and BMP images are supported. The cover folder and its subfolders
are scanned. Use the ROM's filename stem for the image:

```text
ROMs/Adventure/Game Name (USA).vb
Covers/Adventure/Game Name (USA).png
```

The folder layouts do not need to match. A unique exact filename stem takes
priority. Otherwise, a unique title match can ignore case, punctuation, and
parenthesized or bracketed region/language tags. For example,
`Game Name (USA).vb` can match `Game Name (USA) (En).png`.

Multiple matching images produce a placeholder rather than a guessed cover.
Missing or unreadable artwork also produces a placeholder; the game remains
selectable. Covers retain their aspect ratio within a fixed 9:8 frame.
Local artwork takes priority over downloaded covers.

## Find game data and covers

Select **Find Data** in the library toolbar, then **Scan Missing Data**. This
optional online scan uses the public LaunchBox Games Database without an
account, subscription, or API key. It matches Virtual Boy titles and alternate
names, including homebrew listed in the database. Matching ignores case,
punctuation, and bracketed filename tags. Ambiguous and unmatched filenames
remain unchanged; use local artwork for games that are not listed.

The first scan downloads LaunchBox's complete metadata archive (about 103 MiB
at present). Allow at least 650 MiB of temporary free disk space; the archive
and unpacked XML are removed after processing. Only a small Virtual Boy catalog
and the downloaded covers remain. Future scans reuse that catalog and existing
covers. **Update Database and Scan** downloads a fresh catalog to discover new
entries or updated metadata.

Progress appears in the headset. Select **Cancel Scan**, or press left trigger,
B, or Esc while scanning, to cancel. Previously cached data remains available.
The scan runs in the background so headset rendering continues. Download errors
leave games playable; scan again to retry missing covers.

Matched games use their database title. Release year, developer, and genre for
the selected game appear on the Find Data screen when available. Publisher and
description are also retained in the local catalog. Front covers are preferred
over fan-made front covers, with North American artwork preferred when several
regions are available. Covers preserve the downloaded image's detail and aspect.

The cache is `game-data/` beside `library.ini`. Cached data works offline.
Removing this folder clears downloaded data without changing ROMs or local art.
Online requests download the catalog and selected images only: ROM contents,
hashes, filenames, and folder paths are never uploaded. LaunchBox receives the
normal network connection information and requested image filenames.

## Returning to the library

During gameplay, open settings and choose **Game Library**. The current game's
cartridge save is written before returning to the grid. Select another game to
continue in the same VR session. Release selection controls when changing
screens so they do not trigger an action in the game.

## Folder overrides

```powershell
.\build\windows\Release\beetle_vb_openxr.exe --rom-dir "C:\Games\VB" --boxart-dir "C:\Games\VB Covers"
```

These options override remembered folders for the launch. Confirm a folder in
the browser to save the choice. `--settings-file path` also relocates the
neighboring `library.ini`.
