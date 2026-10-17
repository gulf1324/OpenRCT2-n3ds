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

<h3 align="center">
  <a href="https://github.com/gulf1324/OpenRCT2-n3ds/releases/latest/download/OpenRCT2-n3ds.zip">⬇&nbsp; Download OpenRCT2-n3ds.zip</a>
</h3>

<p align="center">
  The game, built and ready to install. <b>Nothing to compile</b>, and on Windows nothing to install on the PC.<br>
  <a href="https://github.com/gulf1324/OpenRCT2-n3ds/releases">All versions</a> · <a href="#install">How to install</a> · <a href="#what-is-in-the-download-and-what-it-does">What it does on your PC</a>
</p>

---

## Read this first

> [!WARNING]
> **The first start shows nothing but a black screen for about two minutes. That is normal: do not turn the console
> off and do not delete the game.** It reads all 2122 object files once and makes its lists. Every start after that
> takes a few seconds.

> [!IMPORTANT]
> - **New models only**: New 3DS, New 3DS XL, New 2DS XL. The original 3DS, 3DS XL and 2DS do not work (too slow, too
>   little memory). The console needs custom firmware: if yours has none yet, start at
>   [3ds.hacks.guide](https://3ds.hacks.guide).
> - **You need your own RollerCoaster Tycoon 2** (Steam or GOG) installed on a PC. Its data files are not in this
>   download; the installer copies them from your PC.
> - **No sound?** The game needs the sound firmware of your console, `/3ds/dspfirm.cdc`, and runs silently without
>   it, with no message. [How to make it](#no-sound).

## Install

**What you need**

| | |
|---|---|
| Console | a **New** 3DS / New 3DS XL / New 2DS XL with custom firmware (Luma3DS) and the homebrew apps **FBI** and **ftpd**. [https://3ds.hacks.guide/get-started.html](https://3ds.hacks.guide/get-started.html)|
| Game | **RollerCoaster Tycoon 2** installed on your PC (Steam or GOG). RollerCoaster Tycoon 1 is optional: its scenarios |
| PC | **Windows**: nothing to install (the download brings the Python its installer runs on). macOS or Linux: Python 3 |
| SD card | about **1 GB** free (about 500 MB without the ride music) |

**Four steps**
> **ELI5 summary:** The installer takes the game and its required files from your PC and puts them on your 3DS's SD card. It also processes some of the game data so that game can run smoothly on the console. And the included Python program does this automatically for you.

1. **Download [OpenRCT2-n3ds.zip](https://github.com/gulf1324/OpenRCT2-n3ds/releases/latest/download/OpenRCT2-n3ds.zip) and unpack all of it** (right-click the ZIP → Extract All).
   `install.cmd` does not work from inside the ZIP. On a release's page, take this file, not "Source code".
2. **On the 3DS, start ftpd** and leave it open. The PC and the 3DS have to be on the same Wi-Fi.
    - Or, without the console turned on, only the console's SD card inserted in the PC. It takes a minute or two: choose `2` in this case when following the steps with the installer.
3. **On the PC, run `install.cmd`** in the unpacked folder (double-click it) and answer its questions.
   If Windows warns about a file from the internet, choose **More info** → **Run anyway**. It's a Python script installing the game.
   <br> On macOS or Linux: open cmd, run `python3 scripts/install.py`.
4. **On the 3DS, close ftpd and open FBI**: `SD` → `cias` → `openrct2.cia` → `Install CIA`.

OpenRCT2 is now on the HOME menu. Remember: **the first start is a black screen for about two minutes.**

### What is in the download, and what it does

Running a script from the internet deserves a second look. Everything here can be checked:

| In the ZIP | What it is |
|---|---|
| `install.cmd` | 44 lines of text that start `scripts/install.py`. Open it in Notepad to read it |
| `scripts/` | the installer: three Python files, plain text (about 750 lines) |
| `python/` | **for PCs without Python**: the official *Windows embeddable package (64-bit)* of Python 3.14.8, from [python.org](https://www.python.org/downloads/release/python-3148/) ([this file](https://www.python.org/ftp/python/3.14.8/python-3.14.8-embed-amd64.zip)), unpacked and unchanged |
| `release/` | the game: `openrct2.cia` and `openrct2.3dsx`, built from the source in this repository |
| `sdcard/` | OpenRCT2's own data files (languages, title sequence, extra graphics) |

**What the installer does**, and nothing else:

1. It **reads** your RollerCoaster Tycoon 2 folder. It never changes it.
2. It writes one file, `build/objdata.pak` (your object files joined into one for the optimization), inside the unpacked folder.
3. It **copies** the game and the data to the 3DS at the address you typed (FTP, to ftpd), or to the SD card drive
   you named.

It installs nothing on the PC, needs no administrator rights, changes nothing outside the unpacked folder, and
connects to nothing but your 3DS. To remove it from the PC, delete the folder.

- **Already have Python 3?** You can delete the `python` folder: `install.cmd` then uses the Python of your PC.
- **Want to check the bundled Python?** Download the file from python.org yourself and compare it with the
  `python` folder, or replace the folder with it. Its SHA-256, as python.org publishes it:
  `a93abe456ab01bd96d7a085b3cdb6566b3063f4241360d114142fbdb07f0a310`
- **Want to check the ZIP?** Its SHA-256 is on the [release's page](https://github.com/gulf1324/OpenRCT2-n3ds/releases/latest),
  beside the file.
- **Just want to build on yor own?** Everything is built from this repository: see "For developers" below.

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
>   on its charger. 
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
| **A red screen (crash)** | That is Luma3DS: press **A** there to save a dump to `/luma/dumps/arm11/` |

For a bug report, attach the game's log `/3ds/openrct2/user/log.txt` (the run before: `log_prev.txt`) and the build
name from the bottom left corner of the title screen.

---

<details>
<summary><b>For developers: the source and how to build it</b></summary>

<br>

| | |
|---|---|
| `external/OpenRCT2/` | the game: OpenRCT2 v0.0.5 with the port. The first commit of this repository is the unmodified v0.0.5 source, so `git diff <first commit> HEAD -- external/OpenRCT2` shows everything the port changed |
| `scripts/` | the installer, setting up the toolchain, building, sending files to the console |
| `sdcard/` | OpenRCT2's own data files (`g2.dat`, languages, title sequences), as in its v0.0.5 release |
| `release/` | the built game: `openrct2.cia`, `openrct2.3dsx` |
| `docs/` | the pictures of this page, and the `README.txt` of the download |
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
python scripts/make_release.py v0.1.0   # build/release/OpenRCT2-n3ds.zip: the download of a release
```

The download of a release is a part of this repository (the installer, `release/`, `sdcard/`) with python.org's
embeddable Python for Windows beside it. A copy of the repository installs the same way, with the Python 3 of
your PC.

`make_cia.py` takes the banner's sound from `gamedata/rct2/Data/css17.dat` (copy or link your RCT2 folder to
`gamedata/rct2`); without it the banner is silent. Other scripts: `send.cmd` (send the .3dsx over Wi-Fi with
3dslink), `ftp.cmd` (files to and from ftpd), `crash_report.cmd` (read a Luma3DS crash dump with the `.elf` of the
same build). They take the address of the 3DS from `local.env`, which the installer writes.

</details>

## Licence and credits

GPLv3, like OpenRCT2: see [LICENSE](LICENSE). OpenRCT2 is the work of the
[OpenRCT2 developers](external/OpenRCT2/contributors.md). This port is not affiliated with or endorsed by them,
Atari, Chris Sawyer or Nintendo. RollerCoaster Tycoon is a trademark of Atari.

Built with devkitPro, libctru and SDL2.
