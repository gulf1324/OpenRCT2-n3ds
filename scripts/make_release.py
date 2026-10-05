#!/usr/bin/env python3
"""Makes the download of a release: build/release/OpenRCT2-n3ds.zip, what somebody who only wants to play
needs and nothing else. No sources, nothing to compile, and on Windows nothing to install: the ZIP brings
python.org's embeddable Python for the installer.

  python scripts/make_release.py v0.1.0

In the ZIP, under one folder OpenRCT2-n3ds-<version>/, everything is where it is in the repository, so the
installer is the same file in both:
  README.txt                docs/release-readme.txt, with the version filled in
  install.cmd, scripts/     the installer and the two modules it uses
  release/                  the built game: openrct2.cia, openrct2.3dsx
  sdcard/                   OpenRCT2's own data
  python/                   the embeddable package, unpacked and unchanged (install.cmd uses it when it is there)
  LICENSE, licenses/        the GPLv3, and the licence of the Korean font in the game

The ZIP's name has no version in it: that keeps one address for the newest one,
https://github.com/gulf1324/OpenRCT2-n3ds/releases/latest/download/OpenRCT2-n3ds.zip (the README links to it).

The files are taken from the working tree, and those have to be as they are committed: the tag of the release
then names what is in the ZIP. The entries carry the time of that commit.
"""
import hashlib
import os
import re
import subprocess
import sys
import time
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build", "release")
NAME = "OpenRCT2-n3ds"

# The embeddable package of python.org. The checksum is the one python.org publishes for the file
# (https://www.python.org/api/v2/downloads/release_file/, sha256_sum).
PYTHON_VERSION = "3.14.8"
PYTHON_ZIP = "python-%s-embed-amd64.zip" % PYTHON_VERSION
PYTHON_URL = "https://www.python.org/ftp/python/%s/%s" % (PYTHON_VERSION, PYTHON_ZIP)
PYTHON_SHA256 = "a93abe456ab01bd96d7a085b3cdb6566b3063f4241360d114142fbdb07f0a310"

README = "docs/release-readme.txt"      # becomes README.txt
FILES = [                               # go where they are
    "install.cmd",
    "LICENSE",
    "scripts/install.py",
    "scripts/n3ds_object_archive.py",
    "scripts/objdata_layout.py",
    "release/openrct2.cia",
    "release/openrct2.3dsx",
]
FOLDERS = ["sdcard"]                    # with everything git has in them
ELSEWHERE = {                           # name in the ZIP: where it is
    "licenses/galmuri-OFL.txt": "external/OpenRCT2/src/platform/n3ds/galmuri-OFL.txt",
}
WINDOWS_TEXT = {"install.cmd"}          # CRLF whatever the working tree has: cmd.exe loses labels otherwise


def git(*arguments):
    return subprocess.run(["git", "-C", ROOT] + list(arguments), check=True, capture_output=True,
                          text=True, encoding="utf-8").stdout


def read(path):
    with open(os.path.join(ROOT, path), "rb") as file:
        return file.read()


def crlf(data):
    return data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")


def python_package():
    """The embeddable package, downloaded once into build/release/cache and checked every time."""
    path = os.path.join(OUT, "cache", PYTHON_ZIP)
    if not os.path.exists(path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        print("Downloading %s ..." % PYTHON_URL)
        with urllib.request.urlopen(PYTHON_URL) as response:
            data = response.read()
        with open(path, "wb") as file:
            file.write(data)
    with open(path, "rb") as file:
        found = hashlib.sha256(file.read()).hexdigest()
    if found != PYTHON_SHA256:
        sys.exit("%s is not the file python.org published:\n  sha256 %s, expected %s\nDelete it and run this again."
                 % (path, found, PYTHON_SHA256))
    return path


def main():
    if len(sys.argv) != 2 or not re.fullmatch(r"v\d+\.\d+\.\d+", sys.argv[1]):
        sys.exit("usage: python scripts/make_release.py v<major>.<minor>.<patch>    (for example v0.1.0)")
    version = sys.argv[1]

    sources = [README] + FILES + FOLDERS + list(ELSEWHERE.values())
    changed = git("status", "--porcelain", "--", *sources).strip()
    if changed:
        sys.exit("These are not as they are committed. Commit them first, so that the tag says what is in the ZIP:\n"
                 + changed)

    entries = {}                         # name in the ZIP: its bytes
    text = read(README).decode("utf-8").replace("{version}", version).replace("{python}", PYTHON_VERSION)
    entries["README.txt"] = b"\xef\xbb\xbf" + crlf(text.encode("utf-8"))   # with a BOM: Notepad then shows the Korean
    for path in FILES:
        entries[path] = crlf(read(path)) if path in WINDOWS_TEXT else read(path)
    for folder in FOLDERS:
        for path in git("ls-files", "-z", "--", folder).split("\0"):
            if path:
                entries[path] = read(path)
    for name, path in ELSEWHERE.items():
        entries[name] = read(path)
    with zipfile.ZipFile(python_package()) as package:
        for item in package.infolist():
            if not item.is_dir():
                entries["python/" + item.filename] = package.read(item)
    if "python/python.exe" not in entries:
        sys.exit("%s has no python.exe" % PYTHON_ZIP)

    stamp = time.gmtime(int(git("log", "-1", "--format=%ct").strip()))[:6]
    os.makedirs(OUT, exist_ok=True)
    target = os.path.join(OUT, NAME + ".zip")
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(entries):
            info = zipfile.ZipInfo("%s-%s/%s" % (NAME, version, name), stamp)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16      # for unzip on macOS and Linux
            archive.writestr(info, entries[name])

    parts = {}
    for name, data in entries.items():
        part = name.split("/")[0] + ("/" if "/" in name else "")
        count, size = parts.get(part, (0, 0))
        parts[part] = (count + 1, size + len(data))
    for part in sorted(parts):
        print("  %-14s %4d file%s %9.2f MB" % (part, parts[part][0], " " if parts[part][0] == 1 else "s",
                                               parts[part][1] / 1048576))
    with open(target, "rb") as file:
        data = file.read()
    print("%s\n  %s, %d files, %.2f MB, sha256 %s" % (target, version, len(entries), len(data) / 1048576,
                                                     hashlib.sha256(data).hexdigest()))
    print("  commit %s" % git("log", "-1", "--format=%h %s").strip())


if __name__ == "__main__":
    main()
