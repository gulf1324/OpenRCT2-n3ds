<h1 align="center">OpenRCT2 for New Nintendo 3DS</h1>

<p align="center">
  <b>RollerCoaster Tycoon 2 on a New 3DS.</b><br>
  An unofficial port of <a href="https://github.com/OpenRCT2/OpenRCT2">OpenRCT2</a> v0.0.5: the park on the top screen,
  the windows on the touch screen, everything usable by touch and by buttons.
</p>

<p align="center">
  <b>English</b> · <a href="README.ko.md">한국어</a>
</p>

<p align="center">
  <img src="docs/title.png" width="300" alt="The title screen on the two screens of the 3DS">
  &nbsp;&nbsp;
  <img src="docs/scenarios-ko.png" width="300" alt="The scenario list, in Korean">
</p>

---

## Read this first

> [!WARNING]
> **The first start shows nothing but a black screen for about two minutes. That is normal: do not turn the console
> off and do not delete the game.** It reads all 2122 object files once and makes its lists. Every start after that
> takes a few seconds.

> [!IMPORTANT]
> - **New models only**: New 3DS, New 3DS XL, New 2DS XL. The original 3DS, 3DS XL and 2DS do not work (too slow, too
>   little memory).
> - **You need your own RollerCoaster Tycoon 2** (Steam or GOG) installed on a PC. Its data files are not in this
>   download; the installer copies them from your PC.
> - **No sound?** The game needs the sound firmware of your console, `/3ds/dspfirm.cdc`, and runs silently without
>   it, with no message. [How to make it](#no-sound).

## Install

**What you need**

| | |
|---|---|
| Console | a **New** 3DS / New 3DS XL / New 2DS XL with custom firmware (Luma3DS) and the homebrew apps **FBI** and **ftpd** |
| Game | **RollerCoaster Tycoon 2** installed on your PC (Steam or GOG). RollerCoaster Tycoon 1 is optional: its scenarios |
| PC | **Python 3** ([python.org](https://www.python.org/downloads/)). Nothing else to install |
| SD card | about **1 GB** free (about 500 MB without the ride music) |

**Four steps**

1. **Download** this repository: [ZIP](https://github.com/gulf1324/OpenRCT2-n3ds/archive/refs/heads/main.zip),
   and unpack it (or `git clone`).
2. **On the 3DS, start ftpd** and leave it open. The PC and the 3DS have to be on the same Wi-Fi.
3. **On the PC, run `install.cmd`** (double-click it) and answer its questions.
   On macOS or Linux: `python3 scripts/install.py`.
4. **On the 3DS, close ftpd and open FBI**: `SD` → `cias` → `openrct2.cia` → `Install CIA`.

OpenRCT2 is now on the HOME menu. Remember: **the first start is a black screen for about two minutes.**

<details>
<summary><b>What the installer asks</b></summary>

```
Language / 언어:  1 English  2 한국어 [1]:
Folder of RollerCoaster Tycoon 2 on this PC [C:\...\steamapps\common\Rollercoaster Tycoon 2]:
Folder of RollerCoaster Tycoon 1, for its scenarios (optional: just press Enter to skip):
Copy the ride music too? It is 440 MB: about 15 minutes more over Wi-Fi [Y/n]:
How do you want to copy?
  1  Over Wi-Fi. On the 3DS, start ftpd now and leave it open.
  2  The SD card is in this PC (much faster) [1]:
The address ftpd shows on the 3DS (for example 192.168.0.12): 192.168.0.12
2571 files, 932 MB to copy (0 files are already there).
Over Wi-Fi this takes about 31 minutes. Keep the 3DS open and on its charger.
Start? [Y/n]:
```

The folders in brackets are found for you where Steam and GOG usually put them: press Enter to take them.

</details>

> [!TIP]
> - **Over Wi-Fi the copy takes about half an hour** (about 16 minutes without the ride music). Keep the 3DS open and
>   on its charger. With the SD card in the PC it takes a minute or two: choose `2` in the installer.
> - **If the copy is interrupted, run the installer again.** It skips what is already there and goes on.
> - **The language** is chosen in the game: file menu (the disk button) → Options → the units tab. A first start
>   follows the language of the console.
> - **Saving is by hand**, from the file menu. There is no autosave.

## Controls

| Button | What it does |
|---|---|
| **Circle Pad** | cursor on the top screen; push it against the edge to move the view |
| **A** / **B** | left / right click at the cursor (hold B and move: drag the view) |
| **D-pad** | move through the controls of the window on the touch screen; with no window open, scroll the view |
| **A** / **B** with the D-pad focus | press the focused control / back |
| **Touch** | tap = click, drag = scroll a list or move a large window, hold = press and hold |
| **L** / **R** | zoom out / in |
| **X** | rotate the view |
| **Y** | close the window in front |
| **ZL** / **ZR** | rotate the object being placed / Shift |
| **START** | pause |
| **SELECT** | cancel construction |
| **START + SELECT** | quit |

## What works

Intro, title screen, the RCT2 scenarios, the RollerCoaster Tycoon 1 scenarios (if you give the installer your RCT1
folder), saving and loading, sound effects, ride music.

| | |
|---|---|
| Speed | 25 to 32 fps in a park with about 1600 guests at the default zoom; about 10 fps fully zoomed out and standing still |
| Start-up | about 8 s from the HOME menu icon to the intro (after the first start) |
| Loading a scenario | about 6 s |
| Languages | English, German, French, Spanish, Italian, Dutch, Swedish, Portuguese and the others of the game's own font, and **Korean** |
| Tested on | a New 3DS XL, with the Steam versions of RCT2 and RCT1 Deluxe. Other New models and the GOG versions should work but are untested |

Left out on purpose: multiplayer, TrueType fonts (so no Japanese, Chinese or Russian), the title sequence editor and
other PC-only options.

## If something goes wrong

| What you see | What to do |
|---|---|
| **A black screen after starting the game** | Wait. The first start takes about two minutes; so does the first start after the installer has changed the object files |
| <a name="no-sound"></a>**No sound** | `/3ds/dspfirm.cdc` is missing (the installer says so). On the 3DS hold **L + D-pad Down + Select** (the Rosalina menu of Luma3DS) → Miscellaneous options → Dump DSP firmware. Once is enough |
| **The installer cannot connect** | ftpd has to be open on the 3DS, and both devices on the same Wi-Fi. Type the address exactly as ftpd shows it on the top screen |
| **Every park fails with "Unable to load file"**, or **loading a park takes a minute or more** | The game data on the SD card is not the installer's. Run the installer again; if you copied `ObjData` by hand before, delete `/3ds/openrct2/rct2/ObjData` on the SD card first |
| **A Korean name is cut short** | Names of rides and parks hold 32 bytes in the save format: about ten Korean characters |
| **A red screen (crash)** | That is Luma3DS: press **A** there to save a dump to `/luma/dumps/arm11/` |

For a bug report, attach the game's log `/3ds/openrct2/user/log.txt` (the run before: `log_prev.txt`) and the build
name from the bottom left corner of the title screen.

---

<details>
<summary><b>What the installer puts on the SD card</b></summary>

<br>

Everything goes to `/3ds/openrct2`, and the game to `/cias/openrct2.cia`. Files that are already there with the
same size are skipped.

| On the SD card | |
|---|---|
| `/3ds/openrct2/data/` | OpenRCT2's own data (`sdcard/` of this repository) |
| `/3ds/openrct2/rct2/` | your RCT2 data. `ObjData` goes into subfolders of 48 files: the 3DS searches a folder from its start for every file it opens, and with 2122 files in one folder that takes a quarter of a second per file |
| `/3ds/openrct2/user/objdata.pak` | all object files in one file (191 MB), byte for byte, made on your PC: parks load from it in seconds |
| `/3ds/openrct2/rct1/Scenarios/` | the RCT1 scenarios, if you gave an RCT1 folder (and its title music as `rct2/Data/css50.dat`) |
| `/3ds/openrct2/user/` | the game keeps its settings, saved games and caches here |
| `/cias/openrct2.cia` | the game, from `release/` of this repository |

- **Without questions**: `python scripts/install.py --rct2 <folder> [--rct1 <folder>] [--no-music] --ip <address>`
  over Wi-Fi, or `--sd <drive>` for an SD card in the PC; `--dry-run` only says what would be copied.
- **Without CIA**: put `release/openrct2.3dsx` in `/3ds/` on the SD card and start it from the Homebrew Launcher. It
  reads the same data.
- **Custom objects**: if you change the files in `ObjData` later, run the installer again. It also removes the
  game's list of objects, which the game otherwise trusts without looking at the folder again.

</details>

<details>
<summary><b>For developers: the source and how to build it</b></summary>

<br>

| | |
|---|---|
| `external/OpenRCT2/` | the game: OpenRCT2 v0.0.5 with the port. The first commit of this repository is the unmodified v0.0.5 source, so `git diff <first commit> HEAD -- external/OpenRCT2` shows everything the port changed |
| `scripts/` | the installer, setting up the toolchain, building, sending files to the console |
| `sdcard/` | OpenRCT2's own data files (`g2.dat`, languages, title sequences), as in its v0.0.5 release |
| `release/` | the built game: `openrct2.cia`, `openrct2.3dsx` |
| `cia/`, `cmake/` | the HOME menu icon and banner, program settings, the CMake toolchain file |

The port's code is in `src/platform/n3ds.c` (paths, memory, performance log), `src/platform/n3ds_input.cpp` (input,
button navigation, output to the two screens), `src/platform/n3ds_font.c` (the Korean font),
`src/drawing/n3ds_drawing.c` (drawing at other sizes) and in `#ifdef __3DS__` blocks elsewhere; comments that start
with `n3ds port:` say why the original was changed. On top of v0.0.5 it carries about forty bug fixes from later
OpenRCT2 versions. v0.0.5 because it is the first OpenRCT2 release that runs without the original `rct2.exe`, and
it is small enough for the console: C and a little C++, software rendering, no scripting.

Building, on Windows with Git Bash. Everything is installed inside this folder (`tools/`); nothing is changed
outside it.

```
bash scripts/setup_toolchain.sh     # devkitARM, libctru (devkitPro packages, unpacked into tools/)
bash scripts/setup_hosttools.sh     # CMake 3.31, ninja
bash scripts/setup_ciatools.sh      # makerom, bannertool
bash scripts/fetch_sources.sh       # SDL2, libzip, speexdsp into external/
source ./env.sh
bash scripts/build_deps.sh          # builds those three for the 3DS
cmake -S external/OpenRCT2 -B build/openrct2 -G Ninja -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/3ds-win.cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build/openrct2        # build/openrct2/openrct2.3dsx
python scripts/make_cia.py          # build/openrct2/openrct2.cia
```

`make_cia.py` takes the banner's sound from `gamedata/rct2/Data/css17.dat` (copy or link your RCT2 folder to
`gamedata/rct2`); without it the banner is silent. Other scripts: `send.cmd` (send the .3dsx over Wi-Fi with
3dslink), `ftp.cmd` (files to and from ftpd), `crash_report.cmd` (read a Luma3DS crash dump with the `.elf` of the
same build). They take the address of the 3DS from `local.env`, which the installer writes.

</details>

## Licence and credits

GPLv3, like OpenRCT2: see [LICENSE](LICENSE). OpenRCT2 is the work of the
[OpenRCT2 developers](external/OpenRCT2/contributors.md). This port is not affiliated with or endorsed by them,
Atari, Chris Sawyer or Nintendo. RollerCoaster Tycoon is a trademark of Atari.

Built with devkitPro, libctru and SDL2. The Korean text is drawn with
[Galmuri](https://github.com/quiple/galmuri) by Lee Minseo, under the SIL Open Font License
(`external/OpenRCT2/src/platform/n3ds/galmuri-OFL.txt`).
