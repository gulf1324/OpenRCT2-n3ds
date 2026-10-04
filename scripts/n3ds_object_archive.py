#!/usr/bin/env python3
"""RCT2의 오브젝트 파일(ObjData, 2122개 약 191MB)을 전부 파일 하나로 묶는다: objdata.pak.

3DS에서는 파일 하나를 여는 데 크기와 무관하게 약 9ms가 든다. 시나리오 하나가 오브젝트 220~540개를
쓰므로 여는 데만 몇 초다. 게임은 SD 카드의 /3ds/openrct2/user/objdata.pak 이 있으면 그 파일을 한 번만
열고 필요한 오브젝트의 바이트만 골라 읽는다 (external/OpenRCT2/src/object/N3dsObjectArchive.h).
묶음은 오브젝트 파일들의 바이트 그대로의 사본이다. 원래 파일은 SD 카드에 그대로 두고, 게임은 묶음에
없는 오브젝트를 원래 파일에서 읽는다.

  python scripts/n3ds_object_archive.py            build/objdata.pak 을 만들고 원본과 대조한다
  python scripts/n3ds_object_archive.py verify     이미 있는 build/objdata.pak 을 원본과 대조만 한다
SD 카드에 올리기: scripts\\sd_sync.cmd objpak (rct2·all 에도 들어 있다). 에뮬레이터: setup_emulator.py

형식 (리틀엔디언. 게임의 N3dsObjectArchive 와 같아야 한다):
  머리말 16바이트   "N3OA", 버전(1), 오브젝트 수, 데이터 크기
  목록   24바이트씩 오브젝트 파일의 첫 16바이트(엔트리: 플래그, 이름 8자, 체크섬), 데이터 안에서의 위치, 크기
                    엔트리의 바이트 순으로 정렬되어 있다(게임이 이분 탐색으로 찾는다)
  데이터            오브젝트 파일들, 목록 순서대로
"""
import os, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OBJDATA = os.path.join(ROOT, "gamedata", "rct2", "ObjData")
OUT = os.path.join(ROOT, "build", "objdata.pak")
NAME = "objdata.pak"

MAGIC = b"N3OA"
VERSION = 1
HEADER = struct.Struct("<4sIII")
ENTRY = struct.Struct("<16sII")
OBJECT_FILE_HEADER_SIZE = 16 + 5     # 엔트리 + 청크 머리말(인코딩 1바이트, 길이 4바이트)


def collect(objdata=OBJDATA):
    """[(엔트리 16바이트, 파일 경로, 크기)], 엔트리의 바이트 순. 하위 폴더까지 본다(SD 카드의 배치도 받는다)."""
    found = {}
    for dirpath, _, filenames in os.walk(objdata):
        for name in sorted(filenames):
            if not name.lower().endswith(".dat"):
                continue
            path = os.path.join(dirpath, name)
            size = os.path.getsize(path)
            with open(path, "rb") as f:
                head = f.read(OBJECT_FILE_HEADER_SIZE)
            if size < OBJECT_FILE_HEADER_SIZE:
                print("  건너뜀 (오브젝트 파일이 아니다):", name)
                continue
            if head[:16] in found:
                print("  건너뜀 (같은 엔트리의 파일이 이미 있다):", name)
                continue
            found[head[:16]] = (path, size)
    return [(entry, path, size) for entry, (path, size) in sorted(found.items())]


def archive_size(items):
    return HEADER.size + ENTRY.size * len(items) + sum(size for _, _, size in items)


def build(out=OUT, objdata=OBJDATA):
    """묶음을 쓰고 그 크기를 돌려준다"""
    items = collect(objdata)
    if not items:
        sys.exit("오브젝트 파일이 없다: " + objdata)
    data_size = sum(size for _, _, size in items)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    part = out + ".part"
    with open(part, "wb") as f:
        f.write(HEADER.pack(MAGIC, VERSION, len(items), data_size))
        offset = 0
        for entry, _, size in items:
            f.write(ENTRY.pack(entry, offset, size))
            offset += size
        for _, path, size in items:
            with open(path, "rb") as src:
                data = src.read()
            if len(data) != size:
                sys.exit("읽는 중에 크기가 바뀌었다: " + path)
            f.write(data)
    os.replace(part, out)
    return os.path.getsize(out)


def ensure(out=OUT, objdata=OBJDATA):
    """묶음이 없거나 크기가 원본과 안 맞으면 만든다. 크기를 돌려준다"""
    expected = archive_size(collect(objdata))
    if not os.path.exists(out) or os.path.getsize(out) != expected:
        build(out, objdata)
    return expected


def verify(pak=OUT, objdata=OBJDATA):
    """묶음의 모든 오브젝트를 원본 파일과 바이트 단위로 대조한다. 게임이 읽는 방식 그대로 찾는다"""
    items = collect(objdata)
    with open(pak, "rb") as f:
        raw = f.read()
    magic, version, count, data_size = HEADER.unpack_from(raw, 0)
    assert magic == MAGIC and version == VERSION, "머리말이 다르다"
    assert count == len(items), "오브젝트 수가 다르다: %d != %d" % (count, len(items))
    data_offset = HEADER.size + ENTRY.size * count
    assert len(raw) == data_offset + data_size, "파일 크기가 머리말과 다르다"
    entries = [ENTRY.unpack_from(raw, HEADER.size + ENTRY.size * i) for i in range(count)]
    keys = [entry for entry, _, _ in entries]
    assert keys == sorted(keys) and len(set(keys)) == count, "목록이 정렬되어 있지 않다"
    largest = 0
    for (entry, path, size), (key, offset, stored_size) in zip(items, entries):
        with open(path, "rb") as f:
            original = f.read()
        stored = raw[data_offset + offset:data_offset + offset + stored_size]
        assert key == entry == original[:16] and stored == original, "원본과 다르다: " + path
        encoding, length = struct.unpack_from("<BI", original, 16)
        assert OBJECT_FILE_HEADER_SIZE + length == len(original), "청크 하나짜리 파일이 아니다: " + path
        largest = max(largest, size)
    print("대조: 오브젝트 %d개, %.1f MB 모두 원본과 같다 (가장 큰 것 %.2f MB)" % (count, data_size / 1e6, largest / 1e6))


def main():
    command = sys.argv[1] if len(sys.argv) > 1 else "build"
    if command == "build":
        size = build()
        print("wrote %s (%.1f MB)" % (OUT, size / 1e6))
        verify()
    elif command == "verify":
        verify()
    else:
        sys.exit(__doc__)
    return 0


if __name__ == "__main__":
    sys.exit(main())
