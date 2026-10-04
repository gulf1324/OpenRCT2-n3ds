#!/usr/bin/env python3
"""Packs the built game into a CIA, the file that installs to the 3DS HOME menu.

  python scripts/make_cia.py          writes build/openrct2/openrct2.cia
  python scripts/make_cia.py put      also uploads it to /cias on the SD card (ftpd running on the 3DS).
                                      Then on the 3DS: FBI > SD > cias > openrct2.cia > Install CIA
  python scripts/make_cia.py cxi      also writes a .cxi, which emulators open without installing

What goes in:
  build/openrct2/openrct2.elf   the game (cmake --build build/openrct2 first)
  cia/openrct2.rsf              program information and permissions (New 3DS only, 124 MB, 804 MHz)
  cia/icon.png, cia/banner.png  the HOME menu icon and the top screen banner (the OpenRCT2 logo)
  gamedata/rct2/Data/css17.dat  the banner's sound: the first 2.9 s of the RCT2 title music, if your RCT2
                                folder is copied (or linked) to gamedata/rct2; silence without it
The start-up logo is makerom's homebrew logo.
Tools: makerom, bannertool (scripts/setup_ciatools.sh), smdhtool (devkitPro).
The installed game reads its data from /3ds/openrct2 on the SD card.
"""
import os, shutil, struct, subprocess, sys, time, wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build", "openrct2")
OUT = os.path.join(BUILD, "cia")
CIA_DIR = os.path.join(ROOT, "cia")
HOST_BIN = os.path.join(ROOT, "tools", "host", "bin")
SMDHTOOL = os.path.join(ROOT, "tools", "devkitpro", "tools", "bin", "smdhtool.exe")

# Name, description and author shown by the HOME menu
NAME = "OpenRCT2"
DESCRIPTION = "OpenRCT2 v0.0.5 for New 3DS"
AUTHOR = "OpenRCT2 developers, n3ds port"
REMOTE_DIR = "/cias"


def run(*args):
    print(">", " ".join(os.path.basename(args[0]) if i == 0 else a for i, a in enumerate(args)))
    subprocess.run(args, check=True)


def need(path, hint):
    if not os.path.exists(path):
        sys.exit("missing: %s\n  %s" % (path, hint))
    return path


def write_banner_sound(path):
    """The banner's sound as a 16 bit stereo 44100 Hz WAV (the format homebrew usually gives bannertool; the
    HOME menu takes up to 3 s): the start of the title music, faded out, or one second of silence."""
    rate, seconds, fade_in, fade_out = 44100, 2.9, 0.01, 0.7
    source = os.path.join(ROOT, "gamedata", "rct2", "Data", "css17.dat")
    if os.path.exists(source):
        with wave.open(source, "rb") as w:
            if (w.getnchannels(), w.getsampwidth(), w.getframerate()) != (2, 2, 22050):
                sys.exit("css17.dat is not 16 bit stereo 22050 Hz")
            raw = w.readframes(int(seconds * 22050))
        samples = struct.unpack("<%dh" % (len(raw) // 2), raw)
        count = len(samples) // 2
        frames = []
        for i in range(count):
            # Fade in and out: the end is a cut in the middle of the music
            moment = i / 22050
            gain = min(1.0, moment / fade_in, (count / 22050 - moment) / fade_out)
            frames.append((samples[2 * i] * gain, samples[2 * i + 1] * gain))
        # 22050 Hz -> 44100 Hz: the mean of its neighbours goes between every two frames
        doubled = []
        for i, (left, right) in enumerate(frames):
            following = frames[i + 1] if i + 1 < count else (left, right)
            doubled.append((left, right))
            doubled.append(((left + following[0]) / 2, (right + following[1]) / 2))
        frames = doubled
    else:
        print("no gamedata/rct2/Data/css17.dat: the banner is silent")
        frames = [(0, 0)] * rate
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<hh", int(round(left)), int(round(right))) for left, right in frames))


def main():
    args = sys.argv[1:]
    if any(a not in ("put", "cxi") for a in args):
        sys.exit(__doc__)

    makerom = need(os.path.join(HOST_BIN, "makerom.exe"), "bash scripts/setup_ciatools.sh")
    bannertool = need(os.path.join(HOST_BIN, "bannertool.exe"), "bash scripts/setup_ciatools.sh")
    need(SMDHTOOL, "bash scripts/setup_toolchain.sh")
    elf = need(os.path.join(BUILD, "openrct2.elf"), "cmake --build build/openrct2")
    rsf = need(os.path.join(CIA_DIR, "openrct2.rsf"), "")
    icon = need(os.path.join(CIA_DIR, "icon.png"), "")
    banner_png = need(os.path.join(CIA_DIR, "banner.png"), "")
    os.makedirs(OUT, exist_ok=True)

    smdh = os.path.join(OUT, "icon.smdh")
    run(SMDHTOOL, "--create", NAME, DESCRIPTION, AUTHOR, icon, smdh)

    sound = os.path.join(OUT, "banner.wav")
    write_banner_sound(sound)
    banner = os.path.join(OUT, "banner.bnr")
    run(bannertool, "makebanner", "-i", banner_png, "-a", sound, "-o", banner)

    common = ["-elf", elf, "-rsf", rsf, "-icon", smdh, "-banner", banner, "-exefslogo", "-target", "t"]
    cia = os.path.join(BUILD, "openrct2.cia")
    run(makerom, "-f", "cia", "-o", cia, *common)
    print("wrote %s (%.1f MB)" % (cia, os.path.getsize(cia) / 1e6))
    if "cxi" in args:
        cxi = os.path.join(BUILD, "openrct2.cxi")
        run(makerom, "-f", "ncch", "-o", cxi, *common)
        print("wrote", cxi)

    if "put" in args:
        # Keep the .elf of the same build: a crash dump can only be read with it
        keep = os.path.join(ROOT, "crashdumps", "elf")
        os.makedirs(keep, exist_ok=True)
        shutil.copy2(elf, os.path.join(keep, "openrct2-%s.elf" % time.strftime("%Y%m%d-%H%M%S")))

        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import ftplib
        import n3ds_ftp
        f = n3ds_ftp.connect()
        try:
            f.mkd(REMOTE_DIR)
        except ftplib.error_perm:
            pass    # already there
        n3ds_ftp.put(f, cia, REMOTE_DIR)
        f.quit()
        print("On the 3DS: FBI > SD > cias > openrct2.cia > Install CIA")
    return 0


if __name__ == "__main__":
    sys.exit(main())
