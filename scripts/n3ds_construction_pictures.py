#!/usr/bin/env python3
"""놀이기구 건설 창의 그림 버튼 28개를 3DS 아래 화면용 1.25배 크기(24 → 30px)로 새로 그린다.

원본 그림(g1.dat 5137~5163, 5169: 방향 화살표, 경사, 기울기, 트랙 종류, 이전/다음, 철거, 체인, 회전)은
22×24, 24×24, 46×24 버튼용이다. 게임이 실행 중에 깨끗하게 키울 수 있는 배율은 1.5배뿐이고, 최근접 1.25배는
네 번째 줄·칸마다 한 번씩 두 번 그려져 외곽선 굵기가 들쭉날쭉해진다. 그래서 새 크기(28×30, 30×30, 58×30)에
맞는 그림을 팔레트 인덱스 그대로 따로 만든다. 섞은 색은 없다: 모든 픽셀은 투명이거나 원본이 쓰는 팔레트 색이다.

사용법:
  python scripts/n3ds_construction_pictures.py

하는 일 (인자 없음):
  1. 규칙으로 만드는 부분을 원본 크기로 만들어 gamedata/rct2/Data/g1.dat 의 원본과 픽셀 단위로 대조한다
     (경사 블록, 작은 화살표의 그림자, 기울기 받침, 나무 트랙, 이전/다음 삼각형 외곽선). 하나라도 다르면
     아무것도 쓰지 않고 종료 코드 1로 끝난다.
  2. 새 그림을 검사한다 (크기, 쓰는 색, 상자 가장자리, 좌우 짝의 대칭). 문제가 있으면 종료 코드 1.
  3. external/OpenRCT2/src/drawing/n3ds_construction_pictures.h 를 쓴다 (게임이 쓰는 그림. 생성된 파일이므로
     손으로 고치지 말고 이 스크립트를 고쳐서 다시 만든다).
  4. build/construction_pictures_compare.png 를 쓴다 (그림마다 원본, 최근접 1.25배, 새 그림을 1배와 6배로,
     그리고 창에 놓일 모습대로 이어 붙인 세 줄을 1배와 3배로).

그림을 만드는 방법은 세 가지다:
  - 외곽선은 손으로, 색은 원본에서: 노란 방향 화살표, U·O 트랙, 이전/다음, 회전. 새 크기의 외곽선을 글자로
    그려 두고('#' 외곽선, 'o' 안쪽), 안쪽 픽셀마다 "외곽선에 닿은 방향(왼쪽·오른쪽·위·아래)이 같은" 원본 픽셀 중
    가장 가까운 것의 색을 가져온다. 그래서 1px 밝은 모서리와 어두운 모서리가 1px로, 제자리에 남는다.
  - 규칙으로: 경사 블록(원본과 같은 기울기 2:1, 1:2, 4:1), 기울기 받침, 나무 트랙, 물길(잔물결이 2px로 남도록
    늘릴 줄·칸을 골라 반복), 기울어진 차(띠가 2:1 선을 따라간다), 체인(원본은 대각선으로 7px마다 같은 고리가
    반복된다. 9px 주기의 고리 하나를 그려 반복).
  - 손으로: 경사 위의 작은 화살표(그림자는 원본 규칙대로 왼쪽 아래로 한 픽셀), 수평 차, 불도저.

좌우 짝(5138/5139, 5140/5141, 5142/5143, 5153/5155, 5160/5161)은 원본에서도 모양만 거울상이고 색은 아니다
(빛이 항상 왼쪽 위에서 온다). 새 그림도 모양(외곽선과 안쪽)은 정확한 거울상으로 만들고 색은 각자의 원본을 따른다.

Pillow 는 비교 그림(PNG)을 쓸 때만 필요하다.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G1_PATH = os.path.join(ROOT, "gamedata", "rct2", "Data", "g1.dat")
HEADER_PATH = os.path.join(ROOT, "external", "OpenRCT2", "src", "drawing", "n3ds_construction_pictures.h")
SHEET_PATH = os.path.join(ROOT, "build", "construction_pictures_compare.png")

SPR_DEFAULT_PALETTE = 0x5FC
SPR_REMAP_TABLES = 4915             # + colour: 256 bytes, [243..254] are the shades of that colour
SHEET_COLOUR = 24                   # the construction window's colour (dark brown)

# The original button boxes and the new ones.
BOXES = {(22, 24): (28, 30), (24, 24): (30, 30), (46, 24): (58, 30)}


# ---------------------------------------------------------------------------------------------------------------
# Pictures are lists of rows of palette indices, 0 = transparent.

def blank(width, height):
    return [[0] * width for _ in range(height)]


def paste(dst, src, x, y):
    """Draws src on dst like the game draws a sprite: index 0 leaves the pixel alone."""
    for j, row in enumerate(src):
        for i, c in enumerate(row):
            if c:
                dst[y + j][x + i] = c


def mirrored(picture):
    return [row[::-1] for row in picture]


def transposed(picture):
    return [list(column) for column in zip(*picture)]


def art(text, legend=None):
    """A picture from rows of characters ('.' = transparent, the rest through the legend). Without a legend the
    rows themselves are returned (a shape for shaded())."""
    rows = [line.strip() for line in text.strip().split("\n")]
    assert len({len(row) for row in rows}) == 1, [len(row) for row in rows]
    if legend is None:
        return rows
    return [[0 if ch == "." else legend[ch] for ch in row] for row in rows]


# ---------------------------------------------------------------------------------------------------------------
# g1.dat

class G1:
    def __init__(self, path):
        with open(path, "rb") as f:
            self.data = f.read()
        self.count = struct.unpack_from("<I", self.data, 0)[0]
        self.base = 8 + 16 * self.count

    def entry(self, index):
        """(offset, width, height, x_offset, y_offset, flags, zoomed_offset)"""
        return struct.unpack_from("<IhhhhHH", self.data, 8 + 16 * index)

    def sprite(self, index):
        """(rows, x_offset, y_offset)"""
        d = self.data
        offset, width, height, x_offset, y_offset, flags, _ = self.entry(index)
        start = self.base + offset
        rows = blank(width, height)
        for y in range(height):
            if flags & 4:           # run length encoded: per row, runs of (length | last flag, x, pixels)
                p = start + struct.unpack_from("<H", d, start + y * 2)[0]
                while True:
                    length, x = d[p] & 0x7F, d[p + 1]
                    rows[y][x:x + length] = d[p + 2:p + 2 + length]
                    p += 2 + length
                    if d[p - 2 - length] & 0x80:
                        break
            else:
                rows[y] = list(d[start + y * width:start + (y + 1) * width])
        return rows, x_offset, y_offset

    def in_box(self, index, width, height):
        rows, x, y = self.sprite(index)
        box = blank(width, height)
        paste(box, rows, x, y)
        return box

    def palette(self):
        """256 (r, g, b). The entry holds blue, green, red triples for the indices from x_offset on."""
        offset, count, _, first, _, _, _ = self.entry(SPR_DEFAULT_PALETTE)
        raw = self.data[self.base + offset:self.base + offset + count * 3]
        colours = [(0, 0, 0)] * 256
        for i in range(count):
            colours[first + i] = (raw[i * 3 + 2], raw[i * 3 + 1], raw[i * 3])
        return colours

    def remap_table(self, colour):
        offset = self.entry(SPR_REMAP_TABLES + colour)[0]
        return self.data[self.base + offset:self.base + offset + 256]


# ---------------------------------------------------------------------------------------------------------------
# Outline by hand, colours from the original.
#
# For the pictures that are a 1 px outline around a grainy, shaded body (the direction arrows, the tube shapes,
# previous/next, rotate) the outline is drawn here at the new size: '#' outline, 'o' body. Every body pixel then
# copies the nearest original body pixel that touches the outline on the same sides (left, right, above, below).
# So the 1 px light edge and the 1 px dark edge of the original stay 1 px and stay on their sides, and the grain
# in between is the original's grain. Single pixels that come out wrong are set by hand afterwards (patched()).

def shaded(original, shape, outline):
    def sides(is_outline, x, y):
        return tuple(is_outline(x + dx, y + dy) for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)))

    def likeness(key, other):
        return sum(a and b for a, b in zip(key, other)) - sum(a != b for a, b in zip(key, other))

    height, width = len(original), len(original[0])

    def original_outline(x, y):
        return 0 <= x < width and 0 <= y < height and original[y][x] == outline

    body = {}       # sides -> [(x, y)] of the original
    for y in range(height):
        for x in range(width):
            if original[y][x] not in (0, outline):
                body.setdefault(sides(original_outline, x, y), []).append((x, y))

    new_height, new_width = len(shape), len(shape[0])

    def shape_outline(x, y):
        return 0 <= x < new_width and 0 <= y < new_height and shape[y][x] == "#"

    picture = blank(new_width, new_height)
    for y in range(new_height):
        for x in range(new_width):
            if shape[y][x] == "#":
                picture[y][x] = outline
            elif shape[y][x] == "o":
                key = sides(shape_outline, x, y)
                candidates = body.get(key)
                if not candidates:
                    # No original pixel touches the outline on exactly these sides: take the most similar ones.
                    best = max(likeness(key, other) for other in body)
                    candidates = [p for other in sorted(body) if likeness(key, other) == best for p in body[other]]
                u = (x + 0.5) * width / new_width
                v = (y + 0.5) * height / new_height
                sx, sy = min(candidates, key=lambda p: ((p[0] + 0.5 - u) ** 2 + (p[1] + 0.5 - v) ** 2, p[1], p[0]))
                picture[y][x] = original[sy][sx]
    return picture


def patched(picture, rows):
    """The picture with some rows of its body set by hand: {y: (first x, colours)}."""
    for y, (x, colours) in rows.items():
        assert all(picture[y][x:x + len(colours)]), y
        picture[y][x:x + len(colours)] = colours
    return picture


# The direction arrows (5137..5143): outline 46, gold body lit from the left. All of them have the head of the
# straight arrow (18 px across, the sides 2 px, the tip 2 px) and its 10 px shaft.
ARROW_OUTLINE = 46

SHAPE_STRAIGHT = art("""
    ........##........
    .......#oo#.......
    ......#oooo#......
    .....#oooooo#.....
    ....#oooooooo#....
    ...#oooooooooo#...
    ..#oooooooooooo#..
    .#oooooooooooooo#.
    #oooooooooooooooo#
    #oooooooooooooooo#
    .####oooooooo####.
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    ....#oooooooo#....
    .....########.....
""")

SHAPE_LEFT_CURVE = art("""
    ........##................
    .......#oo#...............
    ......#ooo#...............
    .....#oooo#...............
    ....#ooooo######..........
    ...#oooooooooooo###.......
    ..#oooooooooooooooo#......
    .#oooooooooooooooooo#.....
    #oooooooooooooooooooo#....
    #ooooooooooooooooooooo#...
    .#ooooooooooooooooooooo#..
    ..#ooooooooooooooooooooo#.
    ...#oooooooooooooooooooo#.
    ....#ooooo####oooooooooo#.
    .....#oooo#...#oooooooooo#
    ......#ooo#....#ooooooooo#
    .......#oo#.....#oooooooo#
    ........##......#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    ................#oooooooo#
    .................########.
""")

SHAPE_LEFT_CURVE_SMALL = art("""
    ........##..........
    .......#oo#.........
    ......#ooo#.........
    .....#oooo#.........
    ....#ooooo###.......
    ...#ooooooooo##.....
    ..#oooooooooooo#....
    .#oooooooooooooo#...
    #oooooooooooooooo#..
    #ooooooooooooooooo#.
    .#oooooooooooooooo#.
    ..#ooooooooooooooo#.
    ...#ooooooooooooooo#
    ....#ooooo#oooooooo#
    .....#oooo#oooooooo#
    ......#ooo#oooooooo#
    .......#oo#oooooooo#
    ........##.########.
""")

SHAPE_LEFT_CURVE_LARGE = art("""
    .#############.....
    #oooooooooooo#.....
    #oooooooooooo#.....
    #ooooooooooo#......
    #oooooooooo#.......
    #ooooooooo##.......
    #oooooooooo#.......
    #ooooooooooo#......
    #ooooooooooo#......
    #oooooooooooo#.....
    #oooooooooooo#.....
    #ooooooooooooo#....
    #oo##ooooooooo#....
    ###.#oooooooooo#...
    .....#ooooooooo#...
    ......#ooooooooo#..
    .......#oooooooo#..
    .......#ooooooooo#.
    ........#oooooooo#.
    ........#oooooooo#.
    .........#oooooooo#
    .........#oooooooo#
    .........#oooooooo#
    .........#oooooooo#
    .........#oooooooo#
    ..........########.
""")

# The tube shapes (5156 U, 5157 O): outline 82, a light green tube lit from the top left. As in the original the
# tube is as thick as the hole is wide (8 px with its outlines; 6 in the original), the hole 8 x 7 (6 x 5).
TUBE_OUTLINE = 82

SHAPE_O = art("""
    .........########.........
    .......##oooooooo##.......
    ......#oooooooooooo#......
    .....#oooooooooooooo#.....
    ....#oooooooooooooooo#....
    ...#oooooooooooooooooo#...
    ..#oooooooooooooooooooo#..
    ..#oooooo########oooooo#..
    .#oooooo#........#oooooo#.
    .#oooooo#........#oooooo#.
    .#oooooo#........#oooooo#.
    #ooooooo#........#ooooooo#
    .#oooooo#........#oooooo#.
    .#oooooo#........#oooooo#.
    .#oooooo#........#oooooo#.
    ..#oooooo########oooooo#..
    ..#oooooooooooooooooooo#..
    ...#oooooooooooooooooo#...
    ....#oooooooooooooooo#....
    .....#oooooooooooooo#.....
    ......#oooooooooooo#......
    .......##oooooooo##.......
    .........########.........
""")

# The U is the lower half of the O (from its widest row down) under a line that closes the two ends, as in the
# originals.
SHAPE_U = [".########........########."] + SHAPE_O[11:]

TUBE_LIGHT, TUBE_DARK = (90, 91), (86, 87, 88)


def rounded_tube(picture, shape):
    """The tube is 6 px thick inside its outlines, the original 4: light edge, two pixels of the middle shade,
    dark edge. So that the new one does not look flat with four pixels of the middle shade, the pixel next to
    the light edge takes the shade between light and middle (90) and the one next to the dark edge the shade
    between middle and dark (88)."""
    def edge(x, y):
        return any(shape[y + dy][x + dx] == "#" for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)))

    out = [row[:] for row in picture]
    for y in range(1, len(shape) - 1):
        for x in range(1, len(shape[0]) - 1):
            if shape[y][x] != "o" or edge(x, y):
                continue
            before = [picture[j][i] for i, j in ((x - 1, y), (x, y - 1)) if shape[j][i] == "o" and edge(i, j)]
            after = [picture[j][i] for i, j in ((x + 1, y), (x, y + 1)) if shape[j][i] == "o" and edge(i, j)]
            if any(c in TUBE_LIGHT for c in before):
                out[y][x] = 90
            elif any(c in TUBE_DARK for c in after):
                out[y][x] = 88
    return out


def tube(original, shape):
    """The U or the O. Set by hand: the widest row (the two points of the tube) as an even run from light to
    dark, in the U the row under it on the right (the rule takes the point above for a light edge), and one
    pixel on the left where the rounding of the lower curve starts a row too early."""
    picture = rounded_tube(shaded(original, shape, TUBE_OUTLINE), shape)
    widest = next(y for y, row in enumerate(shape) if row[0] == "#")
    patched(picture, {widest: (1, [91, 91, 90, 89, 88, 87, 86])})
    patched(picture, {widest: (18, [91, 91, 90, 89, 88, 88, 87])})
    if widest == 1:
        patched(picture, {2: (18, [91, 90, 89, 89, 88, 87])})
    return patched(picture, {widest + 3: (3, [89])})


# Rotate (5169): a red ribbon that turns round and ends in an arrowhead. Outline 10.
ROTATE_OUTLINE = 10

SHAPE_ROTATE = art("""
    ..........#######.........
    .........#ooooooo####.....
    .........#ooooooooooo##...
    .........#ooooooooooooo#..
    .........#oooooooooooooo#.
    .........#ooooooooooooooo#
    .......#.#ooooooooooooooo#
    ......##.#ooooooooooooooo#
    .....#o#.#ooooooooooooooo#
    ....#oo#.#ooooooooooooooo#
    ...#ooo#..#######oooooooo#
    ..#oooo##........####oooo#
    .#ooooooo########oooooooo#
    #oooooooooooooooooooooooo#
    #oooooooooooooooooooooooo#
    .#ooooooooooooooooooooooo#
    .#ooooooooooooooooooooooo#
    ..#ooooooooooooooooooooo#.
    ...#ooooooooooooooooooo#..
    ...#ooooooooooooooooo##...
    ....#oo##oooooooo####.....
    .....#o#.########.........
    .....#o#..................
    ......##..................
    .......#..................
""")

# Previous and next (5160, 5161): two triangles with 45 degree sides, the first one drawn over the tip of the
# second. Outline 166, grainy red body lit from the top left.
TRIANGLE_OUTLINE = 166


def double_triangle(height):
    """The shape of 5160 (pointing left) for an even height: height + 2 wide."""
    half = height // 2
    shape = [["."] * (height + 2) for _ in range(height)]
    for x0 in (half, 0):                 # the second triangle, then the first one over it
        for y in range(height):
            edge = half - 1 - y if y < half else y - half
            for x in range(edge + 1, half + 1):
                shape[y][x0 + x] = "o"
            shape[y][x0 + edge] = "#"
            if y in (0, height - 1):
                shape[y][x0 + half] = "#"
            elif x0 or y not in (half - 1, half):
                shape[y][x0 + half + 1] = "#"
    return ["".join(row) for row in shape]


# ---------------------------------------------------------------------------------------------------------------
# The slope pictures (5144..5152): a light green block seen from the side with a small golden arrow over it.
#
# The block is a bevelled shape: light line on its top, on its slope and on a left wall, dark line on its bottom
# and on a right wall, the usual corner shades. It is made by rule from its outline, so the slope keeps the
# original's gradient (2:1, 1:2, 4:1) at the new size; the rule is checked against the originals.
#
# The arrows are drawn by hand: the 1 px shaft stays 1 px, the head grows from 5 to 7 px. The four slanted ones
# are one shape turned over. The shadow (82) is the arrow moved one pixel left and one down, as in the originals.

# Block colours: fill, light line of a top or of a slope that faces right, light line of a left wall or of a slope
# that faces left, dark line, top left corner, the other corners, bottom right corner, the foot of a slope that
# faces right.
GREEN = (90, 92, 92, 88, 21, 89, 87, 92)
LILAC = (162, 163, 164, 161, 165, 162, 160, 161)
ARROW_SHADOW = 82


def block(width, ends, colours, right_wall=False):
    """A block with a vertical wall on the left: `ends` is the last column of each row from the top down to the
    row above the bottom line. right_wall=True gives the mirror image, with the wall (and its dark line) on
    the right."""
    fill, top, left, dark, top_left, corner, bottom_right, foot = colours
    slope = left if right_wall else top
    rows = []
    for r, end in enumerate(ends):
        above = ends[r - 1] if r else -1
        row = [0] * width
        for x in range(end + 1):
            if x == 0:
                c = top_left if r == 0 else left
            elif x > above or x == end:
                c = slope
            else:
                c = fill
            row[x] = c
        if not right_wall and 0 < above < end == width - 1:
            row[end] = foot
        rows.append(row)
    if right_wall or ends[0] == width - 1:      # a wall on the right (a rectangle has both)
        if right_wall:
            rows = mirrored(rows)
        for r, row in enumerate(rows):
            row[width - 1] = corner if r == 0 else dark
    rows.append([corner] + [dark] * (width - 2) + [bottom_right])
    return rows


def slope_ends(width, run, rise):
    """The rows of a slope that goes `run` pixels sideways for every `rise` rows."""
    ends, end = [], 1 if rise > 1 else 2
    while True:
        ends += [min(end, width - 1)] * (rise if ends or rise == 1 else 1)
        if end >= width - 1:
            return ends
        end += run


def with_shadow(arrow):
    """The arrow with its shadow: one column wider (on the left) and one row taller."""
    height, width = len(arrow), len(arrow[0])
    picture = blank(width + 1, height + 1)
    for y in range(height):
        for x in range(width):
            if arrow[y][x]:
                picture[y + 1][x] = ARROW_SHADOW
    paste(picture, arrow, 1, 0)
    return picture


GOLD = {str(d): 50 + d for d in range(1, 8)}

SMALL_ARROW_RIGHT = art("""
    ........5......
    ........654....
    ........76543..
    222334567655432
    ........67654..
    ........567....
    ........5......
""", GOLD)

SMALL_ARROW_UP = art("""
    ...2...
    ...3...
    ..345..
    ..456..
    .45676.
    .56765.
    5676765
    ...6...
    ...5...
    ...4...
    ...3...
    ...3...
    ...2...
    ...2...
    ...2...
""", GOLD)

SMALL_ARROW_DOWN_SLOPE = art("""
    6.............
    567...........
    ..676....2....
    ....767..42...
    ......766542..
    ........66542.
    .......6766542
""", GOLD)

SMALL_ARROW_UP_SLOPE = art("""
    .......1234567
    ........34567.
    ......234567..
    ....134..67...
    ..123....7....
    123...........
    2.............
""", GOLD)

SMALL_ARROW_DOWN_STEEP = art("""
    44.....
    .6.....
    .76....
    ..6....
    ..67...
    ...6...
    ...67..
    ....6.6
    ....567
    ..23567
    ...2356
    ....235
    .....23
    ......2
""", GOLD)

SMALL_ARROW_UP_STEEP = art("""
    ......1
    .....13
    ....134
    ...1245
    ..12356
    ....456
    ....5.5
    ...53..
    ...4...
    ..53...
    ..4....
    .53....
    .4.....
    53.....
""", GOLD)


def composed(width, height, *parts):
    """A picture of width x height from parts given as (picture, x, y)."""
    picture = blank(width, height)
    for part, x, y in parts:
        paste(picture, part, x, y)
    return picture


# ---------------------------------------------------------------------------------------------------------------
# The banking pictures (5153..5155): a car (black outline, red band over a blue band) on a lilac block.
#
# The level car is drawn by hand. The banked car is the original's construction at the new size: the bands run
# along lines that climb 1 px for every 2 px, so a pixel's band depends on t = x + 2y only; the ends of the car are
# black lines at right angles to that, 1 px sideways for every 2 rows. Heads, wheel and corners are placed by hand.

CAR_BLACK = 10
CAR_COLOURS = {"#": CAR_BLACK, "a": 176, "b": 175, "c": 174, "e": 172, "f": 170, "n": 130, "m": 132, "l": 135}

LEVEL_CAR = art("""
    .....###....###.....
    ....#####..#####....
    ....#####..#####....
    ....#####..#####....
    ..################..
    ..#abbbbbbbbbbbbc#..
    ..#eeeeeeeeeeeeef#..
    ..#eeeeeeeeeeeeef#..
    ..#nnn############..
    ..#llllllllllllll#..
    ..#mmmmmmmmmmmmmm#..
    ..#mmmmmmmmmmmmmm#..
    ..##nnnnnnnnnnnn##..
    ...###........###...
""", CAR_COLOURS)

# The bands of the banked cars from the top edge down, as (first t, colours). 5153 leans left and shows its lit
# side; 5155 is its mirror image with the first pixel of each band in the shade, as in the originals.
BANDS_LEFT = ((16, (10, 10)), (18, (176, 175, 174, 172, 172, 170)), (24, (10, 10)),
              (26, (135, 136, 136, 135, 134, 134, 132, 132, 130, 130)))
BANDS_RIGHT = ((16, (10, 10)), (18, (170, 175, 174, 172, 172, 170)), (24, (10, 10)),
               (26, (132, 136, 136, 135, 134, 134, 132, 132, 130, 130)))
BANKED_LEFT_END = {8: 0, 9: 0, 10: 0, 11: 1, 12: 1, 13: 1, 14: 2}        # row -> column of the black end
BANKED_RIGHT_END = {3: 14, 4: 14, 5: 15, 6: 15, 7: 16, 8: 16, 9: 17, 10: 16}
BANKED_HEADS = art("""
    .......###.
    ......####g
    ......#####
    .###...###.
    #####......
    #####......
    .###.......
""", {"#": CAR_BLACK, "g": 82})
BANKED_WHEEL = art("""
    #m#n
    ###.
""", CAR_COLOURS)


def banked_car(bands):
    """The car of 5153 (leaning left), 20 x 17."""
    picture = blank(20, 17)
    for y in range(2, 15):
        for x in range(20):
            t = x + 2 * y
            if x < BANKED_LEFT_END.get(y, 0) or x > BANKED_RIGHT_END.get(y, 19) or (y == 2 and t > 17):
                continue
            if x in (BANKED_LEFT_END.get(y), BANKED_RIGHT_END.get(y)):
                picture[y][x] = CAR_BLACK
                continue
            for first, colours in bands:
                if first <= t < first + len(colours):
                    picture[y][x] = colours[t - first]
    paste(picture, BANKED_HEADS, 0, 0)
    paste(picture, BANKED_WHEEL, 2, 15)
    return picture


# ---------------------------------------------------------------------------------------------------------------
# The other track type pictures (5158 wooden track, 5159 water channel).

RAIL_OUTLINE, RAIL, TIE = 46, 91, 87


def rc_track(rail, inside, height):
    """The wooden track seen from above: two rails, a tie every inside + 1 rows and a brace that crosses the
    inside diagonally between two ties. The picture is cut off in its last row, as the original is."""
    period = inside + 1
    width = 2 * (rail + 2) + inside
    left = rail + 2                      # the first column between the rails' outlines
    picture = blank(width, height)
    for y in range(height):
        row = picture[y]
        step = (y - 1) % period          # 0 on a tie
        if y == 0 or y == height - 1:    # the outline above the first tie; the cut at the bottom
            for x in list(range(1, rail + 1)) + list(range(width - rail - 1, width - 1)):
                row[x] = RAIL_OUTLINE
            if y == 0:
                row[rail + 1:width - rail - 1] = [RAIL_OUTLINE] * (inside + 2)
            elif step >= 2:
                row[left + step - 2] = RAIL_OUTLINE
            continue
        for x0 in (0, width - rail - 2):
            row[x0:x0 + rail + 2] = [RAIL_OUTLINE] + [RAIL] * rail + [RAIL_OUTLINE]
        if step == 0:
            row[left - 1:left + inside + 1] = [TIE] * (inside + 2)
            continue
        if step == 1 or step == period - 1:     # the outline under and over a tie
            row[left:left + inside] = [RAIL_OUTLINE] * inside
        for x, c in ((left + step - 2, RAIL_OUTLINE), (left + step - 1, TIE), (left + step, RAIL_OUTLINE)):
            if left <= x < left + inside:
                row[x] = c
    return picture


# The water channel is the original with these columns and rows of the sprite drawn twice. They are picked in
# symmetric pairs, between the ripples, so that the 2 px ripples stay 2 px and the picture stays symmetric.
WATER_COLUMNS = [2, 5, 9, 10, 14, 17]
WATER_ROWS = [1, 4, 9, 10, 15, 18]


def stretched(original, columns, rows):
    xs = sorted(list(range(len(original[0]))) + columns)
    ys = sorted(list(range(len(original))) + rows)
    return [[original[y][x] for x in xs] for y in ys]


# ---------------------------------------------------------------------------------------------------------------
# Chain lift (5163): a chain that runs from the bottom left to the top right, and a golden arrow.
#
# The original chain is one link repeated every 7 px along the diagonal. Here the link is drawn by hand for a
# period of 9 px and repeated the same way. A link row is (first column, pixels): '0'..'9', 'A' are the eleven
# greys 119..129, 'o' is a pixel that stays empty (the hole, and the notch under the lit side), '-' belongs to
# the link before. The black shadow (10) is put by the original's rule: on every empty pixel right of or under a
# chain pixel.

CHAIN = {"0123456789A"[i]: 119 + i for i in range(11)}
CHAIN_SHADOW = 10
CHAIN_PERIOD = 9
CHAIN_ROW = 6           # the first row of the link in the middle of the picture
CHAIN_LINK = (
    (12, "22111"),
    (11, "2A731"),
    (10, "29631---00"),
    (9, "285210--000"),
    (8, "2742100-0000"),
    (7, "244210oo06430"),
    (6, "234110oo06530"),
    (6, "12111o006540"),
    (6, "o1897006440"),
    (8, "75404440"),
    (7, "54200000"),
    (6, "233"),
    (6, "23"),
    (6, "2"),
)

# The arrow with its own shadow ('#': right of the head and under the lower edges, as in the original).
CHAIN_ARROW = art("""
    .....gghhge#
    ......eeddc#
    ......eeddc#
    ......eddcc#
    .....eddbcb#
    ....edb###b#
    ...eda#.....
    ..eda#......
    .eda#.......
    eda#........
    da#.........
    ##..........
""", {"a": 50, "b": 51, "c": 52, "d": 53, "e": 54, "g": 56, "h": 57, "#": 10})


def chain(size, period, first_row, link):
    """The links of a chain in a size x size picture, with their shadow."""
    picture = blank(size, size)
    holes = set()
    for n in range(-3, 4):
        for k, (first, pixels) in enumerate(link):
            y = first_row + k + n * period
            for i, ch in enumerate(pixels):
                x = first + i - n * period
                if 0 <= x < size and 0 <= y < size and ch != "-":
                    if ch == "o":
                        holes.add((x, y))
                    else:
                        assert picture[y][x] == 0, (x, y)
                        picture[y][x] = CHAIN[ch]
    for y in range(size):
        for x in range(size):
            if picture[y][x] not in (0, CHAIN_SHADOW):
                for sx, sy in ((x + 1, y), (x, y + 1)):
                    if sx < size and sy < size and not picture[sy][sx] and (sx, sy) not in holes:
                        picture[sy][sx] = CHAIN_SHADOW
    return picture


# ---------------------------------------------------------------------------------------------------------------
# Demolish (5162): the bulldozer, drawn by hand. Thin lines (outline, exhaust pipe, push arm) keep their 1 px;
# blade, bonnet, cab, window and tracks grow.

BULLDOZER = art("""
    ....................####..........
    ...................#ccbc#.........
    .................##ccbba#.........
    ..............###ccbba##..........
    .............#cb###cc#............
    .............#ba#..##.............
    .............#t#..........#######.
    .............#s#.........#sttttts#
    .............#r#.........#t01122t#
    .###.........#r#.........#t11223t#
    #dca#....#####q###########t12233t#
    .acca#...#tttttuuuuuttttsst22334t#
    ..abca#..#sbdd#dtsssrrrrsst23345t#
    ...acca#.#scde#etssssrrrsstuuuuut#
    ...#abca##rdee#esssssqqqqxsssssst#
    ....#bcdcbbbbcddccbaabccdysssssst#
    ....##bb#################apqsssrt#
    ...##bba##rqstqtsqtsqtsqtsqtsqtss#
    ...#aaa#.#t#####################t#
    ..#aaa#..#s#####################s#
    .#aaa#...#s#####################q#
    #aaa#.....#tqstqstqstqstqstqstqs#.
    .###.......#####################..
""", {"#": 10, "a": 11, "b": 12, "c": 13, "d": 14, "e": 17, "p": 169, "q": 170, "r": 171, "s": 172, "t": 173,
      "u": 174, "0": 119, "1": 120, "2": 121, "3": 123, "4": 124, "5": 125, "x": 38, "y": 37})


# ---------------------------------------------------------------------------------------------------------------
# The pictures.

# sprite: (name, how it is made)
PICTURES = {
    5137: ("straight", "outline by hand, colours from the original"),
    5138: ("left curve", "outline by hand, colours from the original"),
    5139: ("right curve", "outline mirrored from 5138, colours from the original"),
    5140: ("left curve, small", "outline by hand, colours from the original"),
    5141: ("right curve, small", "outline mirrored from 5140, colours from the original"),
    5142: ("left curve, large", "outline by hand, colours from the original"),
    5143: ("right curve, large", "outline mirrored from 5142, colours from the original"),
    5144: ("slope down, steep", "block by rule, arrow by hand"),
    5145: ("slope down", "block by rule, arrow by hand"),
    5146: ("level", "block by rule, arrow by hand"),
    5147: ("slope up", "block by rule, arrow by hand"),
    5148: ("slope up, steep", "block by rule, arrow by hand"),
    5149: ("vertical rise", "block by rule, arrow by hand"),
    5150: ("vertical drop", "block by rule, arrow by hand"),
    5151: ("helix down", "block by rule, arrow by hand"),
    5152: ("helix up", "block by rule, arrow by hand"),
    5153: ("left bank", "car and block by rule, heads and wheel by hand"),
    5154: ("no bank", "car by hand, block by rule"),
    5155: ("right bank", "car mirrored from 5153 with its own shades, block by rule"),
    5156: ("U-shaped track", "outline by hand, colours from the original, second shade by rule"),
    5157: ("O-shaped track", "outline by hand, colours from the original, second shade by rule"),
    5158: ("wooden track", "by rule"),
    5159: ("water channel", "the original with chosen columns and rows repeated"),
    5160: ("previous", "outline by rule, colours from the original"),
    5161: ("next", "outline mirrored from 5160, colours from the original"),
    5162: ("demolish", "by hand"),
    5163: ("chain lift", "link by hand, repeated by rule; arrow by hand"),
    5169: ("rotate", "outline by hand, colours from the original"),
}

MIRROR_PAIRS = ((5138, 5139), (5140, 5141), (5142, 5143), (5153, 5155), (5160, 5161))
OUTLINES = {5138: 46, 5140: 46, 5142: 46, 5153: 10, 5160: 166}       # the outline colour of the first of a pair


def original_box(index):
    """The size of the button box of a sprite."""
    return (22, 24) if index <= 5143 else (46, 24) if index == 5162 else (24, 24)


def build_all(g1):
    """sprite -> the new picture, the whole button box."""
    def flip(shape):
        return [row[::-1] for row in shape]

    def arrow(index, shape):
        return shaded(g1.sprite(index)[0], shape, ARROW_OUTLINE)

    def slope(the_block, block_x, block_y, the_arrow, arrow_x, arrow_y):
        return composed(30, 30, (the_block, block_x, block_y), (with_shadow(the_arrow), arrow_x, arrow_y))

    def banking(car, the_block, block_y):
        return composed(20, block_y + len(the_block), (the_block, 0, block_y), (car, 0, 0))

    gentle, steep, helix = slope_ends(22, 2, 1), slope_ends(12, 1, 2), slope_ends(22, 4, 1)
    bar, wedge = [4] * 21, slope_ends(20, 2, 1)
    triangles = double_triangle(24)
    right, down = SMALL_ARROW_RIGHT, transposed(SMALL_ARROW_RIGHT)

    # sprite: (x, y, picture): where the picture goes in the new box
    parts = {
        5137: (5, 2, arrow(5137, SHAPE_STRAIGHT)),
        5138: (1, 2, arrow(5138, SHAPE_LEFT_CURVE)),
        5139: (1, 2, arrow(5139, flip(SHAPE_LEFT_CURVE))),
        5140: (7, 10, arrow(5140, SHAPE_LEFT_CURVE_SMALL)),
        5141: (1, 10, arrow(5141, flip(SHAPE_LEFT_CURVE_SMALL))),
        5142: (5, 2, arrow(5142, SHAPE_LEFT_CURVE_LARGE)),
        5143: (4, 2, arrow(5143, flip(SHAPE_LEFT_CURVE_LARGE))),
        5144: (0, 0, slope(block(12, steep, GREEN), 8, 4, SMALL_ARROW_DOWN_STEEP, 15, 5)),
        5145: (0, 0, slope(block(22, gentle, GREEN), 4, 14, SMALL_ARROW_DOWN_SLOPE, 10, 9)),
        5146: (0, 0, slope(block(22, [21] * 4, GREEN), 4, 21, right, 7, 12)),
        5147: (0, 0, slope(block(22, gentle, GREEN, True), 4, 14, SMALL_ARROW_UP_SLOPE, 6, 10)),
        5148: (0, 0, slope(block(12, steep, GREEN, True), 10, 4, SMALL_ARROW_UP_STEEP, 6, 5)),
        5149: (0, 0, slope(block(5, bar, GREEN), 17, 4, SMALL_ARROW_UP, 7, 7)),
        5150: (0, 0, slope(block(5, bar, GREEN), 8, 4, down, 15, 7)),
        5151: (0, 0, slope(block(22, helix, GREEN), 4, 19, right, 7, 11)),
        5152: (0, 0, slope(block(22, helix, GREEN, True), 4, 19, right, 6, 11)),
        5153: (5, 4, banking(banked_car(BANDS_LEFT), block(20, wedge, LILAC, True), 10)),
        5154: (5, 5, banking(LEVEL_CAR, block(20, [19] * 5, LILAC), 14)),
        5155: (5, 4, banking(mirrored(banked_car(BANDS_RIGHT)), block(20, wedge, LILAC), 10)),
        5156: (2, 14, tube(g1.sprite(5156)[0], SHAPE_U)),
        5157: (2, 4, tube(g1.sprite(5157)[0], SHAPE_O)),
        5158: (6, 3, rc_track(3, 9, 24)),
        5159: (2, 2, stretched(g1.sprite(5159)[0], WATER_COLUMNS, WATER_ROWS)),
        5160: (2, 3, shaded(g1.sprite(5160)[0], triangles, TRIANGLE_OUTLINE)),
        5161: (2, 3, shaded(g1.sprite(5161)[0], flip(triangles), TRIANGLE_OUTLINE)),
        5162: (10, 3, BULLDOZER),
        5163: (2, 2, composed(26, 26, (chain(26, CHAIN_PERIOD, CHAIN_ROW, CHAIN_LINK), 0, 0), (CHAIN_ARROW, 2, 0))),
        5169: (2, 3, patched(shaded(g1.sprite(5169)[0], SHAPE_ROTATE, ROTATE_OUTLINE), {20: (6, [170])})),
    }
    assert sorted(parts) == sorted(PICTURES)
    pictures = {}
    for index, (x, y, part) in parts.items():
        width, height = BOXES[original_box(index)]
        pictures[index] = composed(width, height, (part, x, y))
    return pictures


# ---------------------------------------------------------------------------------------------------------------
# Validation: the rules at the original size against the original sprites.

def validate(g1):
    """Prints one line per check and returns True if every rule reproduces its original exactly."""
    ok = True

    def report(index, what, wrong):
        nonlocal ok
        print("  %d %-34s %s" % (index, what, "match" if not wrong else "MISMATCH at %d pixels, first %s" % (
            len(wrong), sorted(wrong)[:6])))
        ok = ok and not wrong

    def part(index, colours):
        """The pixels of a sprite that have one of these colours."""
        rows = g1.sprite(index)[0]
        return {(x, y): c for y, row in enumerate(rows) for x, c in enumerate(row) if c in colours}

    def differences(index, colours, mine, x0, y0):
        mine = {(x0 + x, y0 + y): c for y, row in enumerate(mine) for x, c in enumerate(row) if c}
        theirs = part(index, colours)
        return [p for p in set(mine) | set(theirs) if mine.get(p) != theirs.get(p)]

    print("Validation against %s" % os.path.relpath(G1_PATH, ROOT).replace(os.sep, "/"))
    gentle, steep, helix = slope_ends(18, 2, 1), slope_ends(10, 1, 2), slope_ends(18, 4, 1)
    blocks = {
        5144: (block(10, steep, GREEN), 0, 0), 5145: (block(18, gentle, GREEN), 0, 4),
        5146: (block(18, [17] * 3, GREEN), 0, 7), 5147: (block(18, gentle, GREEN, True), 0, 3),
        5148: (block(10, steep, GREEN, True), 4, 0), 5149: (block(4, [3] * 17, GREEN), 7, 0),
        5150: (block(4, [3] * 17, GREEN), 0, 0), 5151: (block(18, helix, GREEN), 0, 6),
        5152: (block(18, helix, GREEN, True), 0, 6),
    }
    gold = set(GOLD.values())
    for index, (mine, x, y) in blocks.items():
        report(index, "green block by rule", differences(index, set(GREEN), mine, x, y))
        arrow = part(index, gold)
        shadow = {(x - 1, y + 1) for x, y in arrow} - set(arrow)
        report(index, "arrow shadow = arrow moved (-1, +1)", list(shadow ^ set(part(index, {ARROW_SHADOW}))))
    wedge = slope_ends(16, 2, 1)
    for index, mine, y in ((5153, block(16, wedge, LILAC, True), 8), (5154, block(16, [15] * 4, LILAC), 11),
                           (5155, block(16, wedge, LILAC), 8)):
        report(index, "lilac block by rule", differences(index, set(LILAC), mine, 0, y))
    report(5158, "wooden track by rule", differences(5158, {RAIL_OUTLINE, RAIL, TIE}, rc_track(2, 7, 20), 0, 0))
    for index, shape in ((5160, double_triangle(18)), (5161, [row[::-1] for row in double_triangle(18)])):
        original = g1.sprite(index)[0]
        wrong = [(x, y) for y, row in enumerate(original) for x, c in enumerate(row)
                 if shape[y][x] != ("." if not c else "#" if c == TRIANGLE_OUTLINE else "o")]
        report(index, "outline of the two triangles", wrong)
    u, o = g1.sprite(5156)[0], g1.sprite(5157)[0]
    report(5156, "U = lower half of the O (5157)", [(x, y) for y in range(1, 10) for x in range(20)
                                                    if u[y][x] != o[y + 7][x]])
    links = g1.sprite(5163)[0]
    greys = set(CHAIN.values())
    report(5163, "chain repeats every (-7, +7)", [(x, y) for y in range(13) for x in range(7, 20)
                                                  if links[y][x] in greys and links[y + 7][x - 7] != links[y][x]])

    print("Left/right pairs of the originals (in their boxes):")
    for a, b in MIRROR_PAIRS:
        box = original_box(a)
        first, second = g1.in_box(a, *box), mirrored(g1.in_box(b, *box))
        shape = all(bool(p) == bool(q) for r, s in zip(first, second) for p, q in zip(r, s))
        outline = all((p == OUTLINES[a]) == (q == OUTLINES[a]) for r, s in zip(first, second) for p, q in zip(r, s))
        differ = sum(p != q for r, s in zip(first, second) for p, q in zip(r, s))
        print("  %d / %d: shape %s, outline %s, colours differ at %d pixels (both lit from the left)" % (
            a, b, "mirrored" if shape else "NOT mirrored", "mirrored" if outline else "not mirrored", differ))
    return ok


def bounds(picture):
    """(x, y, width, height) of the part of a picture that is not empty."""
    ys = [j for j, row in enumerate(picture) if any(row)]
    xs = [i for row in picture for i, c in enumerate(row) if c]
    return min(xs), min(ys), max(xs) - min(xs) + 1, max(ys) - min(ys) + 1


def crop(picture):
    x, y, width, height = bounds(picture)
    return [row[x:x + width] for row in picture[y:y + height]]


def placement(picture):
    return "%dx%d at (%d,%d)" % (bounds(picture)[2:] + bounds(picture)[:2])


def check_output(g1, pictures):
    """The new pictures: sizes, colours, box edges, symmetry. Returns the list of problems."""
    problems = []

    def solid(picture):
        return [[bool(c) for c in row] for row in picture]

    for index in sorted(pictures):
        picture = pictures[index]
        box = original_box(index)
        width, height = BOXES[box]
        if len(picture) != height or any(len(row) != width for row in picture):
            problems.append("%d has the wrong size" % index)
            continue
        original = g1.in_box(index, *box)
        used = {c for row in picture for c in row}
        extra = used - {c for row in original for c in row} - {0}
        if extra or not all(0 <= c <= 255 for c in used):
            problems.append("%d uses indices its original does not use: %s" % (index, sorted(extra)))
        touches = any(picture[0]) or any(picture[-1]) or any(row[0] or row[-1] for row in picture)
        touched = any(original[0]) or any(original[-1]) or any(row[0] or row[-1] for row in original)
        if touches and not touched:
            problems.append("%d touches the edge of its box" % index)
        # A picture keeps the symmetry of its original: of the shape, and of the colours if those are symmetric.
        old, new = crop(original), crop(picture)
        for name, turn in (("left-right", mirrored), ("top-bottom", lambda p: p[::-1])):
            if solid(old) == solid(turn(old)) and solid(new) != solid(turn(new)):
                problems.append("%d: the shape is not %s symmetric any more" % (index, name))
            if old == turn(old) and new != turn(new):
                problems.append("%d: the colours are not %s symmetric any more" % (index, name))
    for a, b in MIRROR_PAIRS:
        first, second = pictures[a], mirrored(pictures[b])
        if solid(first) != solid(second):
            problems.append("%d and %d are not mirror images in shape" % (a, b))
        outline = OUTLINES[a]
        if [[c == outline for c in row] for row in first] != [[c == outline for c in row] for row in second]:
            problems.append("%d and %d are not mirror images in outline" % (a, b))
    return problems


# ---------------------------------------------------------------------------------------------------------------
# The header

def header_text(pictures):
    out = ["// Generated by scripts/n3ds_construction_pictures.py. Do not edit.\n"
           "// Palette indices, 0 = transparent, row by row. Each picture is the whole button box.\n"]
    for index in sorted(pictures):
        picture = pictures[index]
        width, height = len(picture[0]), len(picture)
        per_line = width if width <= 30 else width // 2
        values = [c for row in picture for c in row]
        out.append("static const uint8 N3dsConstructionPixels_%d[%d * %d] = {\n" % (index, width, height))
        for i in range(0, len(values), per_line):
            out.append("    " + " ".join("%d," % v for v in values[i:i + per_line]) + "\n")
        out.append("};\n")
    out.append("// sprite, width, height, pixels\n#define N3DS_CONSTRUCTION_PICTURES \\\n")
    lines = ["    { %d, %d, %d, N3dsConstructionPixels_%d }," % (index, len(pictures[index][0]), len(pictures[index]),
                                                                index) for index in sorted(pictures)]
    out.append(" \\\n".join(lines) + "\n")
    return "".join(out)


# ---------------------------------------------------------------------------------------------------------------
# The comparison sheet

def nearest(picture, width, height):
    """The thing to beat: the picture enlarged to width x height by repeating rows and columns."""
    old_height, old_width = len(picture), len(picture[0])
    return [[picture[y * old_height // height][x * old_width // width] for x in range(width)] for y in range(height)]


# The rows of buttons as they stand in the window: (title, pitch, [(sprite, extra gap before it)], pressed sprite).
ASSEMBLED = (
    ("Direction", 28, [(5140, 0), (5138, 0), (5142, 0), (5137, 0), (5143, 0), (5139, 0), (5141, 0)], 5137),
    ("Slope and chain lift", 30, [(5144, 0), (5145, 0), (5146, 0), (5147, 0), (5148, 0), (5163, 6)], 5146),
    ("Banking", 30, [(5153, 0), (5154, 0), (5155, 0)], 5154),
)
BORDER_DARK, BORDER_LIGHT = 246, 252        # shades of the window colour: the inset border of a pressed button


def write_sheet(g1, pictures, path):
    from PIL import Image, ImageDraw, ImageFont

    palette = g1.palette()
    table = g1.remap_table(SHEET_COLOUR)
    background = palette[table[249]]
    dark, light, faint = palette[table[BORDER_DARK]], palette[table[BORDER_LIGHT]], palette[table[247]]
    font = ImageFont.load_default()
    ink = (255, 255, 255)
    zoom, row_zoom, gap, margin, line = 6, 3, 10, 12, 13

    def draw_picture(draw, picture, x0, y0, factor):
        for j, row in enumerate(picture):
            for i, c in enumerate(row):
                if c:
                    left, top = x0 + i * factor, y0 + j * factor
                    draw.rectangle((left, top, left + factor - 1, top + factor - 1), fill=palette[c])

    def cell_width(index):
        old = original_box(index)
        new = BOXES[old]
        return (old[0] + 2 * new[0] + 3 * gap) + (old[0] + 2 * new[0]) * zoom + 3 * gap

    def draw_cell(draw, index, x0, y0):
        """Original, nearest 1.25x and new: at 1:1, then the same three at 6x with the box marked."""
        name, how = PICTURES[index]
        old = original_box(index)
        width, height = BOXES[old]
        original = g1.in_box(index, *old)
        draw.text((x0, y0), "%d %s: %s -> %s. %s" % (index, name, placement(original), placement(pictures[index]),
                                                      how[0].upper() + how[1:]), fill=ink, font=font)
        versions = (original, nearest(original, width, height), pictures[index])
        x, y = x0, y0 + line * 2 + 2
        for factor in (1, zoom):
            for k, picture in enumerate(versions):
                if factor == zoom:
                    draw.text((x, y - line), ("original", "nearest 1.25x", "new")[k], fill=ink, font=font)
                    draw.rectangle((x - 1, y - 1, x + len(picture[0]) * factor, y + len(picture) * factor),
                                   outline=faint)
                draw_picture(draw, picture, x, y, factor)
                x += len(picture[0]) * factor + gap

    def assembled(sprites, pitch, pressed, new):
        """A row of buttons; the pressed one has the 1 px inset border around its box."""
        boxes = []
        x = 0
        for index, before in sprites:
            old = original_box(index)
            picture = pictures[index] if new else g1.in_box(index, *old)
            scale_gap = before if new else before * 4 // 5
            x += scale_gap
            boxes.append((index, x, picture))
            x += pitch if new else len(picture[0])
        height = len(boxes[0][2])
        row = [[None] * x for _ in range(height)]
        for index, x0, picture in boxes:
            for j, line_ in enumerate(picture):
                for i, c in enumerate(line_):
                    if c:
                        row[j][x0 + i] = palette[c]
            if index == pressed:
                right, bottom = x0 + len(picture[0]) - 1, height - 1
                for i in range(x0, right + 1):
                    row[0][i], row[bottom][i] = dark, light
                for j in range(height):
                    row[j][x0], row[j][right] = dark, light
                row[bottom][x0] = light
        return row

    def draw_colours(draw, row, x0, y0, factor):
        for j, line_ in enumerate(row):
            for i, colour in enumerate(line_):
                if colour:
                    left, top = x0 + i * factor, y0 + j * factor
                    draw.rectangle((left, top, left + factor - 1, top + factor - 1), fill=colour)

    # Layout: the pictures in two columns (the wide bulldozer on a row of its own), then the assembled rows.
    narrow = [index for index in sorted(pictures) if original_box(index)[0] != 46]
    wide = [index for index in sorted(pictures) if original_box(index)[0] == 46]
    column = max(cell_width(index) for index in narrow) + 2 * gap
    cell_height = line * 2 + 2 + 30 * zoom + 2 * gap
    width = max(2 * column, max(cell_width(index) for index in wide)) + 2 * margin
    rows_of_cells = (len(narrow) + 1) // 2 + len(wide)
    assembled_height = sum(line * 2 + 30 + gap + 30 * row_zoom + 2 * gap for _ in ASSEMBLED)
    height = margin + line * 3 + rows_of_cells * cell_height + line * 2 + assembled_height + margin
    image = Image.new("RGB", (width, height), background)
    draw = ImageDraw.Draw(image)

    y = margin
    draw.text((margin, y), "Ride construction window: picture buttons at 1.25x (22x24 -> 28x30, 24x24 -> 30x30, "
              "46x24 -> 58x30). Background: window colour %d, shade 249 (mid light)." % SHEET_COLOUR,
              fill=ink, font=font)
    draw.text((margin, y + line), "Every cell: original, nearest-neighbour 1.25x, new, each as its whole button box, "
              "at 1:1 and then at %dx (the frame marks the box)." % zoom, fill=ink, font=font)
    y += line * 3
    for k, index in enumerate(narrow):
        draw_cell(draw, index, margin + (k % 2) * column, y + (k // 2) * cell_height)
    y += (len(narrow) + 1) // 2 * cell_height
    for index in wide:
        draw_cell(draw, index, margin, y)
        y += cell_height

    draw.text((margin, y), "The buttons side by side as in the window, at 1:1 and at %dx, one of them pressed (1 px "
              "inset border: dark on the top and left, light on the bottom and right). The originals at their pitch "
              "for comparison." % row_zoom, fill=ink, font=font)
    draw.line((margin, y + line, width - margin, y + line), fill=faint)
    y += line * 2
    for title, pitch, sprites, pressed in ASSEMBLED:
        new, old = assembled(sprites, pitch, pressed, True), assembled(sprites, pitch, pressed, False)
        draw.text((margin, y), "%s (pitch %d px): %s" % (title, pitch, ", ".join(
            "%d" % index if not before else "gap %d, %d" % (before, index) for index, before in sprites)),
            fill=ink, font=font)
        y += line * 2
        x = margin
        draw.text((x, y - line), "new 1:1", fill=ink, font=font)
        draw_colours(draw, new, x, y, 1)
        x += len(new[0]) + 2 * gap
        draw.text((x, y - line), "original 1:1", fill=ink, font=font)
        draw_colours(draw, old, x, y, 1)
        y += 30 + gap + line
        x = margin
        draw.text((x, y - line), "new %dx" % row_zoom, fill=ink, font=font)
        draw_colours(draw, new, x, y, row_zoom)
        x += len(new[0]) * row_zoom + 2 * gap
        draw.text((x, y - line), "original %dx" % row_zoom, fill=ink, font=font)
        draw_colours(draw, old, x, y, row_zoom)
        y += 30 * row_zoom + 2 * gap - line
    os.makedirs(os.path.dirname(path), exist_ok=True)
    image.save(path, "PNG")
    return image.size


# ---------------------------------------------------------------------------------------------------------------

def main():
    if len(sys.argv) > 1:
        print(__doc__)
        return 2
    g1 = G1(G1_PATH)
    if not validate(g1):
        print("The rules do not reproduce the originals. Nothing written.")
        return 1
    pictures = build_all(g1)
    problems = check_output(g1, pictures)
    if problems:
        print("The new pictures are wrong:\n  " + "\n  ".join(problems))
        return 1
    print("New pictures: %d, sizes and colours checked, left/right pairs are mirror images in shape and outline" %
          len(pictures))

    with open(HEADER_PATH, "w", encoding="ascii", newline="\n") as f:
        f.write(header_text(pictures))
    print("Wrote %s" % os.path.relpath(HEADER_PATH, ROOT).replace(os.sep, "/"))
    size = write_sheet(g1, pictures, SHEET_PATH)
    print("Wrote %s (%dx%d)" % (os.path.relpath(SHEET_PATH, ROOT).replace(os.sep, "/"), size[0], size[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
