#!/usr/bin/env python3
"""한국어 글꼴을 게임에 넣을 형식으로 굽는다: 갈무리(Galmuri) 비트맵 글꼴의 BDF → galmuri.n3kf

  python scripts/n3ds_korean_font.py

3DS 빌드에는 TrueType 엔진(SDL_ttf, FreeType)이 없다. 게임이 한국어를 그릴 때 부르는 SDL_ttf 함수 일곱 개를
external/OpenRCT2/src/platform/n3ds_font.c 가 이 파일의 비트맵으로 구현한다. 파일은 #embed로 실행 파일에 들어간다.

글꼴: 갈무리 (github.com/quiple/galmuri, SIL Open Font License 1.1, 닌텐도 DS의 글꼴을 본뜬 픽셀 글꼴).
크기 셋을 굽는다: Galmuri7(게임의 tiny·small), Galmuri9(medium: 본문), Galmuri14(big). 사용자가 고른 구성이다
(2026-10-05, 비교 그림 build/korean_font_mock.png 의 A안). BDF는 tools/.cache/fonts/galmuri 에 받아 둔다(없으면 받는다).

하는 일:
  1. 글꼴마다 아래 RANGES의 글자를 BDF에서 꺼낸다 (한글 음절 11,172자 전부, ASCII, Latin-1, 문장 부호, 자모 등).
  2. external/OpenRCT2/src/platform/n3ds/galmuri.n3kf 를 쓰고, 라이선스 글을 그 옆에 둔다 (OFL은 글꼴과 함께
     라이선스를 배포하라고 한다).
  3. 쓴 파일을 게임과 같은 방법으로 다시 읽어 모든 글자를 BDF와 픽셀 단위로 대조한다. 다르면 종료 코드 1.

파일 형식 (리틀 엔디언. n3ds_font.c 가 읽는다):
  "N3KF", 버전 uint16 (1), 글꼴 수 uint16
  글꼴마다 8바이트: 픽셀 크기 uint16, 줄 상자의 높이 uint8, 구간 수 uint8, 구간 표의 위치 uint32
  구간마다 16바이트: 첫 코드포인트 uint32, 기록들의 위치 uint32, 글자 수 uint16, 칸의 왼쪽 int8(펜에서),
                    칸의 위 int8(줄 상자 위에서), 칸의 폭 uint8, 높이 uint8, 기록의 크기 uint8, 0
  기록: 전진 폭 uint8 (0 = 이 글꼴에 없는 글자), 그다음 칸의 비트(위 줄부터, 왼쪽부터, 바이트의 높은 비트부터 이어서)
한 구간의 글자는 모두 같은 크기의 칸을 쓴다(그 구간 글자들의 잉크를 다 담는 가장 작은 칸): 글자의 기록을 곱셈
한 번으로 찾는다.
"""
import io
import os
import struct
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = "v2.40.4"
URL = "https://github.com/quiple/galmuri/releases/download/%s/Galmuri-%s.zip" % (VERSION, VERSION)
CACHE = os.path.join(ROOT, "tools", ".cache", "fonts", "galmuri")
OUT_DIR = os.path.join(ROOT, "external", "OpenRCT2", "src", "platform", "n3ds")
OUT = os.path.join(OUT_DIR, "galmuri.n3kf")
LICENCE_OUT = os.path.join(OUT_DIR, "galmuri-OFL.txt")

SIZES = [7, 9, 14]              # Galmuri7, Galmuri9, Galmuri14
RANGES = [
    (0x0020, 0x007E),           # ASCII
    (0x00A0, 0x00FF),           # Latin-1
    (0x2010, 0x2027),           # dashes, quotes, ellipsis, middle dots
    (0x20A0, 0x20BF),           # currency signs (the won sign)
    (0x3000, 0x303F),           # CJK punctuation
    (0x3131, 0x318E),           # Hangul compatibility jamo
    (0xAC00, 0xD7A3),           # Hangul syllables
    (0xFF01, 0xFF5E),           # fullwidth forms
]


def fetch():
    if all(os.path.exists(os.path.join(CACHE, "Galmuri%d.bdf" % size)) for size in SIZES):
        return
    print("download", URL)
    with urllib.request.urlopen(URL) as response:
        data = response.read()
    os.makedirs(CACHE, exist_ok=True)
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        z.extractall(CACHE)


class Bdf:
    """glyphs: codepoint -> (advance, set of (x, y) ink pixels; x from the pen, y from the top of the line box).
    Ink above or below the line box (the accents of a few Latin capitals) is left out: the game draws a line of
    text into an image of that height. clipped counts the glyphs that lost pixels."""

    def __init__(self, path):
        self.glyphs = {}
        self.clipped = 0
        ascent = descent = None
        code = advance = bbx = rows = None
        for line in open(path, encoding="utf-8", errors="replace"):
            p = line.split()
            if not p:
                continue
            if p[0] == "FONT_ASCENT":
                ascent = int(p[1])
            elif p[0] == "FONT_DESCENT":
                descent = int(p[1])
            elif p[0] == "ENCODING":
                code = int(p[1])
            elif p[0] == "DWIDTH":
                advance = int(p[1])
            elif p[0] == "BBX":
                bbx = tuple(int(v) for v in p[1:5])
            elif p[0] == "BITMAP":
                rows = []
            elif p[0] == "ENDCHAR":
                w, h, xo, yo = bbx
                ink = set()
                for j, row in enumerate(rows):
                    bits, nbits = int(row, 16), len(row) * 4
                    for i in range(w):
                        if bits >> (nbits - 1 - i) & 1:
                            ink.add((xo + i, ascent - yo - h + j))
                inside = {(x, y) for x, y in ink if 0 <= y < ascent + descent}
                self.clipped += inside != ink
                self.glyphs[code] = (advance, inside)
                code = advance = bbx = rows = None
            elif rows is not None:
                rows.append(p[0])
        assert ascent is not None and descent is not None
        self.height = ascent + descent


def bake_font(size):
    """(pixel size, line box height, [(first, count, left, top, width, height, record size, records)])"""
    font = Bdf(os.path.join(CACHE, "Galmuri%d.bdf" % size))
    runs = []
    for first, last in RANGES:
        present = [c for c in range(first, last + 1) if c in font.glyphs]
        inked = [font.glyphs[c][1] for c in present if font.glyphs[c][1]]
        if not inked:
            continue
        left = min(x for ink in inked for x, _ in ink)
        right = max(x for ink in inked for x, _ in ink)
        top = min(y for ink in inked for _, y in ink)
        bottom = max(y for ink in inked for _, y in ink)
        width, height = right - left + 1, bottom - top + 1
        assert 0 <= top and bottom < font.height and -128 <= left and width < 256 and height < 256, (size, hex(first))
        record_size = 1 + (width * height + 7) // 8
        assert record_size < 256
        records = bytearray()
        for c in range(first, last + 1):
            record = bytearray(record_size)
            if c in font.glyphs:
                advance, ink = font.glyphs[c]
                assert 0 < advance < 256, (size, hex(c), advance)
                record[0] = advance
                for x, y in ink:
                    bit = (y - top) * width + (x - left)
                    record[1 + (bit >> 3)] |= 0x80 >> (bit & 7)
            records += record
        runs.append((first, last - first + 1, left, top, width, height, record_size, bytes(records)))
    return size, font.height, runs, font


def build():
    fonts = [bake_font(size) for size in SIZES]
    header = 8 + 8 * len(fonts)
    tables, blobs = [], []
    offset = header + sum(16 * len(runs) for _, _, runs, _ in fonts)
    table_offset = header
    out = [b"N3KF", struct.pack("<HH", 1, len(fonts))]
    for size, height, runs, _ in fonts:
        out.append(struct.pack("<HBBI", size, height, len(runs), table_offset))
        table_offset += 16 * len(runs)
        for first, count, left, top, width, cell_height, record_size, records in runs:
            tables.append(struct.pack("<IIHbbBBBB", first, offset, count, left, top, width, cell_height, record_size, 0))
            blobs.append(records)
            offset += len(records)
    data = b"".join(out + tables + blobs)
    assert len(data) == offset
    return data, fonts


def read_back(data):
    """What n3ds_font.c makes of the file: {pixel size: (height, {codepoint: (advance, ink)})}"""
    assert data[:4] == b"N3KF" and struct.unpack_from("<H", data, 4)[0] == 1
    fonts = {}
    for i in range(struct.unpack_from("<H", data, 6)[0]):
        size, height, run_count, table = struct.unpack_from("<HBBI", data, 8 + 8 * i)
        glyphs = {}
        for r in range(run_count):
            first, offset, count, left, top, width, cell_height, record_size, _ = struct.unpack_from(
                "<IIHbbBBBB", data, table + 16 * r)
            for index in range(count):
                record = data[offset + index * record_size:offset + (index + 1) * record_size]
                if record[0] == 0:
                    continue
                ink = set()
                for bit in range(width * cell_height):
                    if record[1 + (bit >> 3)] & (0x80 >> (bit & 7)):
                        ink.add((left + bit % width, top + bit // width))
                glyphs[first + index] = (record[0], ink)
        fonts[size] = (height, glyphs)
    return fonts


def main():
    if len(sys.argv) > 1:
        print(__doc__)
        return 2
    fetch()
    data, fonts = build()
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(data)
    with open(os.path.join(CACHE, "LICENSE.txt"), "rb") as src, open(LICENCE_OUT, "wb") as dst:
        dst.write(src.read())

    read = read_back(open(OUT, "rb").read())
    wrong = 0
    for size, height, runs, font in fonts:
        got_height, got = read[size]
        wanted = {c: g for c, g in font.glyphs.items() if any(first <= c <= last for first, last in RANGES)}
        wrong += got_height != height or got != wanted
        hangul = sum(1 for c in got if 0xAC00 <= c <= 0xD7A3)
        print("Galmuri%d: line box %d px, %d glyphs (%d Hangul syllables), %d runs, %d KB; %d glyphs of the whole font "
              "reach outside the line box and are cut there" % (
                  size, height, len(got), hangul, len(runs), sum(len(r[7]) for r in runs) // 1024, font.clipped))
        for first, count, left, top, width, cell_height, record_size, _ in runs:
            print("    U+%04X..%04X  cell %dx%d at (%d, %d), %d bytes a glyph" % (
                first, first + count - 1, width, cell_height, left, top, record_size))
    print("wrote %s (%d KB) and %s" % (os.path.relpath(OUT, ROOT).replace(os.sep, "/"), len(data) // 1024,
                                      os.path.relpath(LICENCE_OUT, ROOT).replace(os.sep, "/")))
    if wrong:
        print("The file does not give back the glyphs of the BDF files.")
        return 1
    print("every glyph read back from the file is pixel for pixel the one of the BDF")
    return 0


if __name__ == "__main__":
    sys.exit(main())
