#!/usr/bin/env python3
"""Installs OpenRCT2 for New 3DS from this PC: asks where your RollerCoaster Tycoon 2 is and copies everything
the game needs to the SD card of the 3DS, over Wi-Fi (ftpd running on the 3DS) or to the SD card in this PC.

  python scripts/install.py                 asks what it needs (Windows: double-click install.cmd)

Without questions:
  python scripts/install.py --rct2 <folder> [--rct1 <folder>] [--no-music] (--ip <address[:port]> | --sd <path>)
                            [--lang en|ko] [--yes] [--dry-run]

What it copies (files that are already there with the same size are skipped, so it can be run again, and it
goes on where it stopped):
  /3ds/openrct2/data/               OpenRCT2's own data, from sdcard/
  /3ds/openrct2/rct2/               your RCT2 data. ObjData goes into subfolders of 48 files: the 3DS searches a
                                    folder from its start for every file it opens, and with 2122 files in one
                                    folder opening a file takes a quarter of a second
  /3ds/openrct2/user/objdata.pak    all object files in one file, byte for byte (n3ds_object_archive.py):
                                    parks load from it in seconds
  /3ds/openrct2/rct1/Scenarios/     with an RCT1 folder: its scenarios, and its title music as rct2/Data/css50.dat
  /cias/openrct2.cia                the game, to be installed with FBI on the 3DS (from release/)
"""
import argparse
import ftplib
import os
import shutil
import string
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n3ds_object_archive  # noqa: E402
import objdata_layout  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "sdcard", "3ds", "openrct2", "data")
CIA = os.path.join(ROOT, "release", "openrct2.cia")
PAK = os.path.join(ROOT, "build", n3ds_object_archive.NAME)
LOCAL_ENV = os.path.join(ROOT, "local.env")
BASE = "3ds/openrct2"
OBJECT_INDEX = BASE + "/user/objects.idx"
OBJDATA_ON_SD = BASE + "/rct2/" + objdata_layout.OBJDATA
SKIP_FOLDERS = {"install"}              # of the RCT2 folder: the installer. Files beside its folders are not needed
MUSIC_KEEP = {"css1.dat", "css17.dat"}  # the sound effects and the title music
BIG_FILE = 8 * 1024 * 1024              # from this size a file is uploaded under another name and renamed
WIFI_SPEED = 500 * 1024                 # bytes per second ftpd on a New 3DS takes, for the estimate

# Where the games usually are, under each drive
PARENTS = ["SteamLibrary\\steamapps\\common", "Program Files (x86)\\Steam\\steamapps\\common",
           "Steam\\steamapps\\common", "GOG Games", "Program Files (x86)\\GOG Galaxy\\Games", "GOG Galaxy\\Games"]
RCT2_NAMES = ["Rollercoaster Tycoon 2", "RollerCoaster Tycoon 2 Triple Thrill Pack"]
RCT1_NAMES = ["RollerCoaster Tycoon Deluxe", "RollerCoaster Tycoon"]

TEXT = {
    "en": {
        "title": "OpenRCT2 for New 3DS: installer",
        "intro": "This copies the game and your RollerCoaster Tycoon 2 data to the SD card of your 3DS.\n"
                 "Nothing on this PC is changed outside this folder.",
        "ask_rct2": "Folder of RollerCoaster Tycoon 2 on this PC",
        "found": "found",
        "bad_rct2": "That is not a RollerCoaster Tycoon 2 folder (it needs Data\\g1.dat and ObjData). Try again.",
        "ask_rct1": "Folder of RollerCoaster Tycoon 1, for its scenarios (optional: just press Enter to skip)",
        "bad_rct1": "That folder has no Scenarios with .SC4 files. Try again, or press Enter to skip.",
        "ask_music": "Copy the ride music too? It is 440 MB: about 15 minutes more over Wi-Fi",
        "ask_how": "How do you want to copy?\n"
                   "  1  Over Wi-Fi. On the 3DS, start ftpd now and leave it open.\n"
                   "  2  The SD card is in this PC (much faster)",
        "ask_ip": "The address ftpd shows on the 3DS (for example 192.168.0.12)",
        "connecting": "Connecting to %s ...",
        "no_connection": "No connection (%s).\n  Is ftpd open on the 3DS, and are the PC and the 3DS on the same Wi-Fi?",
        "ask_sd": "Drive or folder of the SD card (for example F:\\)",
        "bad_sd": "That folder does not exist. Try again.",
        "archive": "Making the object archive (191 MB, once) ...",
        "checking": "Looking at what is already on the SD card ...",
        "flat_objdata": "The SD card has an older copy of ObjData with all files in one folder:\n  /%s\n"
                        "Delete that folder on the SD card and run this again.",
        "plan": "%d files, %.0f MB to copy (%d files are already there).",
        "plan_time": "Over Wi-Fi this takes about %d minutes. Keep the 3DS open and on its charger.",
        "ask_go": "Start?",
        "nothing": "Everything is already on the SD card.",
        "index_deleted": "Deleted the game's old object list: the next start of the game takes about two minutes.",
        "retry": "  again: %s (%s)",
        "failed": "Could not copy %s. Run this again to go on from here.",
        "progress": "  %d/%d files  %.0f/%.0f MB  %.0f KB/s  about %d min left",
        "dry": "(dry run: nothing was copied)",
        "no_dsp": "NOTE: /3ds/dspfirm.cdc is not on the SD card. Without it the game has no sound.\n"
                  "  On the 3DS: hold L + D-pad Down + Select (Rosalina menu) > Miscellaneous options >\n"
                  "  Dump DSP firmware. Once is enough.",
        "done": "Done. Now on the 3DS:",
        "done_wifi": "  1. Close ftpd and open FBI.",
        "done_sd": "  1. Put the SD card back into the 3DS and open FBI.",
        "done_steps": "  2. SD > cias > openrct2.cia > Install CIA.\n"
                      "  3. Go back to the HOME menu: OpenRCT2 is there. Start it.\n"
                      "The first start shows a black screen for about two minutes (the game lists its objects, once).",
        "yes_no": "[Y/n]",
        "no_yes": "[y/N]",
        "no_cia": "release/openrct2.cia is missing: unpack the whole download again, or build the game (README).",
    },
    "ko": {
        "title": "New 3DS용 OpenRCT2 설치",
        "intro": "게임과 RollerCoaster Tycoon 2 데이터를 3DS의 SD 카드로 복사합니다.\n"
                 "이 PC에서는 이 폴더 밖의 것을 바꾸지 않습니다.",
        "ask_rct2": "이 PC에 있는 RollerCoaster Tycoon 2 폴더의 경로",
        "found": "찾음",
        "bad_rct2": "RollerCoaster Tycoon 2 폴더가 아닙니다(Data\\g1.dat와 ObjData가 있어야 합니다). 다시 입력해 주세요.",
        "ask_rct1": "RollerCoaster Tycoon 1 폴더의 경로 (시나리오용. 없으면 그냥 Enter)",
        "bad_rct1": "그 폴더에는 .SC4 파일이 든 Scenarios가 없습니다. 다시 입력하거나 Enter로 건너뛰세요.",
        "ask_music": "놀이기구 음악도 복사할까요? 440MB라 Wi-Fi로는 약 15분이 더 걸립니다",
        "ask_how": "어떻게 복사할까요?\n"
                   "  1  Wi-Fi로. 지금 3DS에서 ftpd를 실행하고 켜 둔 채로 두세요.\n"
                   "  2  SD 카드를 이 PC에 꽂았습니다 (훨씬 빠릅니다)",
        "ask_ip": "3DS의 ftpd 화면에 나오는 주소 (예: 192.168.0.12)",
        "connecting": "%s 에 연결하는 중 ...",
        "no_connection": "연결되지 않습니다 (%s).\n  3DS에서 ftpd가 켜져 있는지, PC와 3DS가 같은 Wi-Fi에 있는지 확인해 주세요.",
        "ask_sd": "SD 카드의 드라이브나 폴더 (예: F:\\)",
        "bad_sd": "그런 폴더가 없습니다. 다시 입력해 주세요.",
        "archive": "오브젝트 묶음을 만드는 중 (191MB, 한 번만) ...",
        "checking": "SD 카드에 이미 있는 것을 확인하는 중 ...",
        "flat_objdata": "SD 카드에 파일이 한 폴더에 다 들어 있는 예전 ObjData가 있습니다:\n  /%s\n"
                        "SD 카드에서 그 폴더를 지우고 다시 실행해 주세요.",
        "plan": "복사할 파일 %d개, %.0fMB (%d개는 이미 있습니다).",
        "plan_time": "Wi-Fi로 약 %d분 걸립니다. 3DS를 열어 두고 충전기를 꽂아 두세요.",
        "ask_go": "시작할까요?",
        "nothing": "필요한 파일이 이미 SD 카드에 다 있습니다.",
        "index_deleted": "게임의 예전 오브젝트 목록을 지웠습니다: 다음 게임 시작은 약 2분 걸립니다.",
        "retry": "  다시: %s (%s)",
        "failed": "%s 을(를) 복사하지 못했습니다. 다시 실행하면 여기서부터 이어서 합니다.",
        "progress": "  파일 %d/%d  %.0f/%.0fMB  %.0fKB/s  약 %d분 남음",
        "dry": "(dry run: 아무것도 복사하지 않았습니다)",
        "no_dsp": "참고: SD 카드에 /3ds/dspfirm.cdc 가 없습니다. 없으면 게임에서 소리가 나지 않습니다.\n"
                  "  3DS에서 L + 십자키 아래 + Select (Rosalina 메뉴) > Miscellaneous options >\n"
                  "  Dump DSP firmware. 한 번만 하면 됩니다.",
        "done": "끝났습니다. 이제 3DS에서:",
        "done_wifi": "  1. ftpd를 닫고 FBI를 엽니다.",
        "done_sd": "  1. SD 카드를 3DS에 다시 꽂고 FBI를 엽니다.",
        "done_steps": "  2. SD > cias > openrct2.cia > Install CIA.\n"
                      "  3. 홈 메뉴로 돌아가면 OpenRCT2 아이콘이 있습니다. 실행하세요.\n"
                      "첫 실행은 검은 화면으로 약 2분 걸립니다(오브젝트 목록을 한 번 만듭니다).",
        "yes_no": "[Y/n]",
        "no_yes": "[y/N]",
        "no_cia": "release/openrct2.cia 가 없습니다: 받은 ZIP을 통째로 다시 풀거나, 게임을 빌드하세요(README).",
    },
}
LANG = "en"


def t(key, *args):
    text = TEXT[LANG][key]
    return text % args if args else text


def ask(prompt, default=None):
    """One line from the player, without the quotes a dragged path comes in"""
    suffix = " [%s]" % default if default else ""
    try:
        answer = input("%s%s: " % (prompt, suffix)).strip().strip('"').strip("'").strip()
    except EOFError:
        sys.exit(1)
    return answer or (default or "")


def ask_yes(prompt, default=True):
    answer = ask("%s %s" % (prompt, t("yes_no" if default else "no_yes"))).lower()
    return default if not answer else answer[0] in ("y", "ㅛ", "예"[0], "네"[0])


# ---------------------------------------------------------------------------------------------------------------
# The game folders on this PC

def find_folder(parent, name):
    """The folder of that name in parent, whatever its case; None if there is none"""
    try:
        for entry in os.listdir(parent):
            if entry.lower() == name.lower() and os.path.isdir(os.path.join(parent, entry)):
                return os.path.join(parent, entry)
    except OSError:
        pass
    return None


def is_rct2(path):
    data = find_folder(path, "Data")
    return bool(data and os.path.isfile(os.path.join(data, "g1.dat")) and find_folder(path, objdata_layout.OBJDATA))


def rct1_scenarios(path):
    scenarios = find_folder(path, "Scenarios")
    if not scenarios:
        return []
    return [os.path.join(scenarios, n) for n in sorted(os.listdir(scenarios)) if n.lower().endswith(".sc4")]


def guess(names, valid):
    """The first usual place where one of these game folders is"""
    drives = ["%s:\\" % c for c in string.ascii_uppercase if os.path.isdir("%s:\\" % c)] if os.name == "nt" else []
    for drive in drives:
        for parent in PARENTS:
            for name in names:
                path = os.path.join(drive, parent, name)
                if os.path.isdir(path) and valid(path):
                    return path
    return None


# ---------------------------------------------------------------------------------------------------------------
# What goes where

def is_ride_music(rel):
    folder, _, name = rel.rpartition("/")
    name = name.lower()
    return folder.lower() == "data" and name.startswith("css") and name.endswith(".dat") and name not in MUSIC_KEEP


def files_of(src, top_level_files=True):
    """Relative paths ('Data/g1.dat') of the files under src"""
    out = []
    for dirpath, dirnames, filenames in os.walk(src):
        rel_dir = os.path.relpath(dirpath, src).replace(os.sep, "/")
        if rel_dir == ".":
            dirnames[:] = [d for d in dirnames if d.lower() not in SKIP_FOLDERS]
            if not top_level_files:
                filenames = []
            rel_dir = ""
        dirnames.sort()
        for name in sorted(filenames):
            out.append((rel_dir + "/" if rel_dir else "") + name)
    return out


def plan(rct2, rct1, music):
    """[(file on this PC, path on the SD card)], in the order they are copied. The paths have no leading slash."""
    items = [(os.path.join(DATA, *rel.split("/")), BASE + "/data/" + rel) for rel in files_of(DATA)]
    table = objdata_layout.table(rct2)
    for rel in files_of(rct2, top_level_files=False):
        if music or not is_ride_music(rel):
            items.append((os.path.join(rct2, *rel.split("/")), BASE + "/rct2/" + objdata_layout.placed(rel, table)))
    items.append((PAK, BASE + "/user/" + n3ds_object_archive.NAME))
    if rct1:
        items += [(path, BASE + "/rct1/Scenarios/" + os.path.basename(path)) for path in rct1_scenarios(rct1)]
        # The RCT1 title music, under the name the game looks for among the RCT2 data
        for sub in ("Data", os.path.join("RCTdeluxe_install", "Data")):
            music_file = os.path.join(rct1, sub, "css17.dat")
            if os.path.isfile(music_file):
                items.append((music_file, BASE + "/rct2/Data/css50.dat"))
                break
    items.append((CIA, "cias/openrct2.cia"))
    return items


# ---------------------------------------------------------------------------------------------------------------
# Where it goes: the SD card in this PC, or ftpd on the 3DS. Paths are relative to the root of the SD card.
# The SD card does not tell upper from lower case, so names are compared without it.

class FolderTarget:
    def __init__(self, root):
        self.root = root

    def _path(self, rel):
        return os.path.join(self.root, *rel.split("/"))

    def size(self, rel):
        path = self._path(rel)
        return os.path.getsize(path) if os.path.isfile(path) else None

    def files_in(self, rel_dir):
        path = self._path(rel_dir)
        return [n for n in os.listdir(path) if os.path.isfile(os.path.join(path, n))] if os.path.isdir(path) else []

    def ensure_dir(self, rel_dir):
        os.makedirs(self._path(rel_dir), exist_ok=True)

    def put(self, local, rel, progress=None):
        self.ensure_dir(rel.rpartition("/")[0])
        part = self._path(rel) + ".part"
        shutil.copyfile(local, part)
        os.replace(part, self._path(rel))

    def delete(self, rel):
        if os.path.isfile(self._path(rel)):
            os.remove(self._path(rel))

    def reconnect(self):
        pass

    def close(self):
        pass


class FtpTarget:
    """ftpd on the 3DS: passive mode only, and LIST takes no path (so CWD first)"""

    def __init__(self, ip, port):
        self.ip, self.port = ip, port
        self.reconnect()

    def reconnect(self):
        self.ftp = ftplib.FTP(timeout=60)
        self.ftp.connect(self.ip, self.port)
        self.ftp.login()
        self.ftp.set_pasv(True)
        self.listings = {}      # folder -> {name in lower case: size, or None for a folder}; None if it is not there

    def _listing(self, rel_dir):
        key = rel_dir.lower()
        if key not in self.listings:
            lines = []
            try:
                self.ftp.cwd("/" + rel_dir)
                self.ftp.retrlines("LIST", lines.append)
            except ftplib.error_perm:
                self.listings[key] = None
                return None
            entries = {}
            for line in lines:      # '-rw-rw-rw- 1 3DS 3DS <size> <month> <day> <year or time> <name>'
                parts = line.split(None, 8)
                if len(parts) == 9:
                    entries[parts[8].lower()] = None if line.startswith("d") else int(parts[4])
            self.listings[key] = entries
        return self.listings[key]

    def size(self, rel):
        rel_dir, _, name = rel.rpartition("/")
        return (self._listing(rel_dir) or {}).get(name.lower())

    def files_in(self, rel_dir):
        return [n for n, size in (self._listing(rel_dir) or {}).items() if size is not None]

    def ensure_dir(self, rel_dir):
        path = ""
        for part in rel_dir.split("/"):
            parent, path = path, (path + "/" + part if path else part)
            if self._listing(path) is None:
                try:
                    self.ftp.mkd("/" + path)
                except ftplib.error_perm:
                    pass    # it is there after all
                self.listings[path.lower()] = {}
                if self.listings.get(parent.lower()) is not None:
                    self.listings[parent.lower()][part.lower()] = None

    def put(self, local, rel, progress=None):
        rel_dir, _, name = rel.rpartition("/")
        self.ensure_dir(rel_dir)
        self.ftp.cwd("/" + rel_dir)
        size = os.path.getsize(local)
        # A large file goes up under another name: what an interrupted upload leaves is not taken for the file
        upload_name = name + ".part" if size >= BIG_FILE else name
        with open(local, "rb") as f:
            self.ftp.storbinary("STOR " + upload_name, f, blocksize=64 * 1024, callback=progress)
        if upload_name != name:
            if name.lower() in self.listings[rel_dir.lower()]:
                self.ftp.delete(name)
            self.ftp.rename(upload_name, name)
        self.listings[rel_dir.lower()][name.lower()] = size

    def delete(self, rel):
        rel_dir, _, name = rel.rpartition("/")
        if self.size(rel) is not None:
            self.ftp.cwd("/" + rel_dir)
            self.ftp.delete(name)
            del self.listings[rel_dir.lower()][name.lower()]

    def close(self):
        try:
            self.ftp.quit()
        except ftplib.all_errors:
            pass


def copy_all(target, todo, wifi):
    """Copies the files of todo [(local, rel, size)]; goes on after a lost connection"""
    total = sum(size for _, _, size in todo)
    done = [0]              # bytes copied
    shown = [time.time()]
    start = time.time()

    def show(count, force=False):
        now = time.time()
        if force or now - shown[0] >= 10:
            shown[0] = now
            speed = done[0] / max(now - start, 0.1)
            print(t("progress", count, len(todo), done[0] / 1e6, total / 1e6, speed / 1024,
                    round((total - done[0]) / max(speed, 1) / 60)), flush=True)

    for count, (local, rel, size) in enumerate(todo, 1):
        for attempt in range(4):
            before = done[0]

            def progress(block, count=count):
                done[0] += len(block)
                show(count - 1)

            try:
                target.put(local, rel, progress if wifi else None)
                done[0] = before + size
                break
            except ftplib.all_errors as e:        # a tuple, with OSError in it
                done[0] = before
                if attempt == 3:
                    sys.exit(t("failed", rel))
                print(t("retry", rel, e), flush=True)
                time.sleep(3)
                try:
                    target.reconnect()
                except ftplib.all_errors:
                    pass
        show(count, force=count == len(todo))


# ---------------------------------------------------------------------------------------------------------------

def read_env():
    values = {}
    if os.path.exists(LOCAL_ENV):
        for line in open(LOCAL_ENV, encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#") and "=" in line:
                key, value = line.split("=", 1)
                values[key.strip()] = value.strip()
    return values


def write_env(ip, port):
    """Keeps the address for the next time and for the other scripts (ftp.cmd, send.cmd)"""
    values = read_env()
    values["N3DS_IP"] = ip
    values["N3DS_FTP_PORT"] = str(port)
    with open(LOCAL_ENV, "w", encoding="utf-8", newline="\n") as f:
        f.write("# The 3DS on this network (written by scripts/install.py; not tracked by git)\n")
        f.write("".join("%s=%s\n" % item for item in values.items()))


def parse_address(text):
    ip, _, port = text.replace("ftp://", "").strip("/").partition(":")
    return ip.strip(), int(port) if port.strip().isdigit() else 5000


def main():
    global LANG
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rct2", help="your RollerCoaster Tycoon 2 folder")
    parser.add_argument("--rct1", help="your RollerCoaster Tycoon 1 folder (optional: its scenarios)")
    parser.add_argument("--no-music", action="store_true", help="leave out the ride music (440 MB)")
    parser.add_argument("--ip", help="copy over Wi-Fi: the address ftpd shows on the 3DS, with :port if not 5000")
    parser.add_argument("--sd", help="copy to the SD card in this PC: its drive or folder")
    parser.add_argument("--lang", choices=sorted(TEXT), help="language of the questions")
    parser.add_argument("--yes", action="store_true", help="do not ask before copying")
    parser.add_argument("--dry-run", action="store_true", help="say what would be copied and stop")
    args = parser.parse_args()
    asked = not (args.rct2 and (args.ip or args.sd))     # the player is at the keyboard

    try:
        sys.stdout.reconfigure(errors="replace")
    except AttributeError:
        pass
    if args.lang:
        LANG = args.lang
    elif asked:
        LANG = "ko" if ask("Language / 언어:  1 English  2 한국어", "1") == "2" else "en"
    print("\n" + t("title") + "\n" + "-" * 60 + "\n" + t("intro") + "\n")
    if not os.path.isfile(CIA):
        sys.exit(t("no_cia"))

    # The games
    rct2 = args.rct2
    if not rct2:
        found = guess(RCT2_NAMES, is_rct2)
        while True:
            rct2 = ask(t("ask_rct2"), found)
            if rct2 and is_rct2(rct2):
                break
            print(t("bad_rct2"))
    elif not is_rct2(rct2):
        sys.exit(t("bad_rct2"))
    rct2 = os.path.abspath(rct2)

    rct1 = args.rct1
    if not rct1 and asked:
        found = guess(RCT1_NAMES, rct1_scenarios)
        while True:
            rct1 = ask(t("ask_rct1"), found)
            if not rct1 or rct1_scenarios(rct1):
                break
            print(t("bad_rct1"))
    elif rct1 and not rct1_scenarios(rct1):
        sys.exit(t("bad_rct1"))
    rct1 = os.path.abspath(rct1) if rct1 else None

    music = not args.no_music
    if asked and music:
        music = ask_yes(t("ask_music"))

    # The SD card
    wifi = bool(args.ip)
    if not args.ip and not args.sd:
        print()
        wifi = ask(t("ask_how"), "1") != "2"
    if wifi:
        address = args.ip
        while True:
            if not address:
                address = ask(t("ask_ip"), read_env().get("N3DS_IP"))
            ip, port = parse_address(address)
            print(t("connecting", "%s:%d" % (ip, port)), flush=True)
            try:
                target = FtpTarget(ip, port)
                break
            except ftplib.all_errors as e:        # a tuple, with OSError in it
                if args.ip:
                    sys.exit(t("no_connection", e))
                print(t("no_connection", e))
                address = None
        write_env(ip, port)
    else:
        sd = args.sd
        while not sd or not os.path.isdir(sd):
            if args.sd:
                sys.exit(t("bad_sd"))
            if sd:
                print(t("bad_sd"))
            sd = ask(t("ask_sd"))
        target = FolderTarget(os.path.abspath(sd))

    # The object archive is made on this PC, from the object files of the RCT2 folder
    objdata = find_folder(rct2, objdata_layout.OBJDATA)
    if not os.path.exists(PAK) or os.path.getsize(PAK) != n3ds_object_archive.archive_size(
            n3ds_object_archive.collect(objdata)):
        print(t("archive"), flush=True)
    n3ds_object_archive.ensure(out=PAK, objdata=objdata)

    # What is missing on the SD card
    print(t("checking"), flush=True)
    if target.files_in(OBJDATA_ON_SD):
        sys.exit(t("flat_objdata", OBJDATA_ON_SD))
    items = plan(rct2, rct1, music)
    todo = []
    for local, rel in items:
        size = os.path.getsize(local)
        if target.size(rel) != size:
            todo.append((local, rel, size))
    total = sum(size for _, _, size in todo)
    print(t("plan", len(todo), total / 1e6, len(items) - len(todo)))
    if todo:
        if wifi:
            print(t("plan_time", max(1, round(total / WIFI_SPEED / 60))))
        if args.dry_run:
            for _, rel, size in todo[:15]:
                print("  /%s (%d)" % (rel, size))
            print(t("dry"))
            target.close()
            return 0
        if asked and not args.yes and not ask_yes(t("ask_go")):
            target.close()
            return 1
        # The game trusts its list of objects and does not look at the folder again: when object files change,
        # the list goes first, so that the game makes it again at its next start
        if any(rel.lower().startswith(OBJDATA_ON_SD.lower() + "/") for _, rel, _ in todo) and \
                target.size(OBJECT_INDEX) is not None:
            target.delete(OBJECT_INDEX)
            print(t("index_deleted"))
        target.ensure_dir(BASE + "/user")
        copy_all(target, todo, wifi)
    else:
        print(t("nothing"))
        if args.dry_run:
            target.close()
            return 0

    print()
    if target.size("3ds/dspfirm.cdc") is None:
        print(t("no_dsp") + "\n")
    print(t("done"))
    print(t("done_wifi" if wifi else "done_sd"))
    print(t("done_steps"))
    target.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
