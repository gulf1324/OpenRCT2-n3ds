#!/usr/bin/env python3
"""땅·물·정리·땅 구입 도구 창의 "도구 크기" 그림과 -/+ 버튼을 3DS 아래 화면 크기로 새로 그린다.

원본 그림(g1.dat 5499~5510: 상자 44×32, 버튼 16×16)을 픽셀마다 두 번씩 늘리면 계단이 져서 거칠어 보인다.
여기서는 그림을 규칙으로 다시 만든다. 타일만 두 배(16×8)로 키우고 선은 원본 굵기 그대로 둔다
(흰색 2px, 양옆 회색 1px, 기울기 2:1). 버튼은 같은 방식(1px 테두리, 1px 빗변)으로 32×32에 그리고
-/+ 글자만 그 크기에 맞게 새로 그린다. 규칙이 맞는지는 같은 규칙으로 원본 크기를 만들어 g1.dat의 원본과
픽셀 단위로 대조해서 확인한다.

사용법:
  python scripts/n3ds_tool_pictures.py

하는 일 (인자 없음):
  1. 원본 크기(배율 1, 버튼 16)로 만든 그림을 gamedata/rct2/Data/g1.dat 의 5499~5510과 대조한다.
     하나라도 다르면 아무것도 쓰지 않고 종료 코드 1로 끝난다.
  2. external/OpenRCT2/src/drawing/n3ds_tool_pictures.h 를 쓴다 (게임이 쓰는 그림: 상자 88×64 여덟 장,
     버튼 32×32 네 장. 생성된 파일이므로 손으로 고치지 말고 이 스크립트를 고쳐서 다시 만든다).
  3. build/tool_size_compare.png 를 쓴다 (원본, 두 배로 늘린 것, 새 그림을 1배와 3배로 나란히 놓은 비교).

원본에서 규칙을 벗어난 것은 하나다: 크기 6(5509)은 크기 4 격자를 다섯 번 붙여 만든 그림인데 그중 한 장이
1px 왼쪽에 붙어 있어 좌우 대칭이 아니다(original_size_6 참고). 대조는 그 붙이기까지 그대로 재현해서 하고,
새 그림은 다른 크기와 같은 규칙으로 대칭이 되게 그린다.

Pillow 는 비교 그림(PNG)을 쓸 때만 필요하다.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
G1_PATH = os.path.join(ROOT, "gamedata", "rct2", "Data", "g1.dat")
HEADER_PATH = os.path.join(ROOT, "external", "OpenRCT2", "src", "drawing", "n3ds_tool_pictures.h")
SHEET_PATH = os.path.join(ROOT, "build", "tool_size_compare.png")

SPR_DEFAULT_PALETTE = 0x5FC
SPR_REMAP_TABLES = 4915             # + colour: 256 bytes, [243..254] are the shades of that colour
SPR_BUTTONS = 5499                  # decrease, decrease pressed, increase, increase pressed
SPR_LAND_TOOL_SIZE_0 = 5503         # the mountain tool's mark, then sizes 1..7

BOX_WIDTH, BOX_HEIGHT = 44, 32      # the tool size box at scale 1
BUTTON_SIZE = 16                    # the buttons at scale 1
WHITE, GREY = 21, 16                # lattice line: 2 px white core, 1 px grey on each side
GLYPH = 10                          # the minus and the plus

# The glyph of a button: (length, stroke, distance of its square box from the two bevelled edges).
GLYPH_16 = (6, 2, 2)                # the original
GLYPH_32 = (11, 3, 5)               # the new one
GLYPH_32_CANDIDATES = [(9, 3, 6), (11, 3, 5), (13, 3, 4)]

# Mountain mark at scale 2: a dot on every row of each line (1) or on every second row (2).
MARK_DOT_ROWS = 1

SHEET_COLOUR = 24                   # the window colour of the comparison sheet (dark brown)


# ---------------------------------------------------------------------------------------------------------------
# Pictures are lists of rows of palette indices, 0 = transparent.

def blank(width, height):
    return [[0] * width for _ in range(height)]


def paste(dst, src, x, y):
    """Draws src on dst like the game draws a sprite: index 0 leaves the pixel alone, the rest is clipped."""
    for j, row in enumerate(src):
        if not 0 <= y + j < len(dst):
            continue
        for i, c in enumerate(row):
            if c and 0 <= x + i < len(dst[0]):
                dst[y + j][x + i] = c


def doubled(picture):
    """The rejected version: every pixel repeated 2x2."""
    out = []
    for row in picture:
        wide = [c for c in row for _ in (0, 1)]
        out += [wide, list(wide)]
    return out


# ---------------------------------------------------------------------------------------------------------------
# The lattice (tool sizes 1..7).
#
# An n x n block of isometric tiles, each 8*scale wide and 4*scale high. The lines run 2 px sideways per row.
# On every row a line is the run "grey white white grey"; where runs of different lines overlap, white wins.
# The top vertex sits on the box's vertical centre line, the middle row of the lattice on row height / 2.
# The picture is cut where the white ends on the left and right (the side vertices have no grey outside, just as
# the top and bottom vertices have none above and below), and it never gets wider than the size 5 lattice: sizes
# 6 and 7 are cut at the columns where the side vertices of size 5 are. At scale 1 that is the 42 px of the
# original (5 * 8 + the 2 px of the white pair); at scale 2 it is 82 px (5 * 16 + 2), not 84, because the line
# does not get wider. The cut then falls between white pairs as in the original, never through one.

def lattice_lines(n, scale, extend=0):
    """The white pairs of the lattice as (step, row, x): x is the left pixel of the pair, relative to the top
    vertex's pair; step counts rows from the line's first vertex. Each of the n + 1 lines per direction runs
    from its vertex on the upper edge of the block to the one on the lower edge, plus `extend` rows at both ends."""
    rows_per_tile_edge = 2 * scale
    for k in range(n + 1):
        first_row = k * rows_per_tile_edge
        for step in range(-extend, n * rows_per_tile_edge + 1 + extend):
            yield step, first_row + step, -2 * first_row + 2 * step      # the line going down and right
            yield step, first_row + step, 2 * first_row - 2 * step       # the line going down and left


def lattice_geometry(n, scale):
    width, height = BOX_WIDTH * scale, BOX_HEIGHT * scale
    centre = width // 2 - 1                         # left pixel of the top vertex's white pair
    top = height // 2 - 2 * n * scale
    half = 4 * min(n, 5) * scale                    # top vertex to side vertex (or to the cut), in pixels
    return width, height, centre, top, centre - half, centre + half + 2    # right is exclusive


def lattice(n, scale, weight=1):
    """The picture of tool size n in its box. weight = thickness of the lines in rows (1 = as in the original)."""
    width, height, centre, top, left, right = lattice_geometry(n, scale)
    white = set()
    for _, row, x in lattice_lines(n, scale):
        for t in range(weight):
            white.add((centre + x, top + row + t))
            white.add((centre + x + 1, top + row + t))
    picture = blank(width, height)
    for x, y in white:
        for gx in (x - 1, x + 1):
            if (gx, y) not in white and left <= gx < right:
                picture[y][gx] = GREY
    for x, y in white:
        if left <= x < right:
            picture[y][x] = WHITE
    return picture


def crop(picture):
    """(x, y, rows) of the part of a picture that is not empty."""
    ys = [j for j, row in enumerate(picture) if any(row)]
    xs = [i for row in picture for i, c in enumerate(row) if c]
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    return x0, y0, [row[x0:x1] for row in picture[y0:y1]]


def original_size_6(slipped):
    """Sprite 5509 as its artist made it: the 42 x 25 picture is five copies of the size 4 lattice (34 x 17)
    pasted over one another. Four of them tile the 6 x 6 lattice (top, left, right, bottom); the right one sits
    1 px too far left (x = 11 instead of 12), which shifts the lines of the upper right part and leaves stray
    grey pixels beside the lines. slipped=False puts that copy where it belongs, which gives the regular lattice
    (the fifth copy, at the bottom of the stack, then hides completely)."""
    _, _, block = crop(lattice(4, 1))
    picture = blank(42, 25)
    for x, y in ((8, 6), (-4, 4), (4, 0), (11 if slipped else 12, 4), (4, 8)):      # bottom to top
        paste(picture, block, x, y)
    box = blank(BOX_WIDTH, BOX_HEIGHT)
    paste(box, picture, 1, 4)
    return box


# ---------------------------------------------------------------------------------------------------------------
# The mountain tool's mark (tool size 0), drawn by the game over a size picture.
#
# A dotted 5 x 5 lattice with the centre tile drawn solid (the size 1 picture). The dotted lines are the white
# core of the lattice lines, each continued half a tile edge past its last vertex, with only one pixel of every
# pair kept: the outer one (the left pixel left of the centre line, the right pixel right of it, both on it).
# dot_rows = 1 keeps a dot on every row of a line, as the original does (dots 2 px apart sideways).

def mountain_mark(scale, dot_rows=1, weight=1):
    width, height, centre, top, left, right = lattice_geometry(5, scale)
    picture = blank(width, height)
    for step, row, x in lattice_lines(5, scale, extend=scale):
        if step % dot_rows:
            continue
        x += centre
        for px in ([x] if x < centre else [x + 1] if x > centre else [x, x + 1]):
            if left <= px < right:
                picture[top + row][px] = WHITE
    paste(picture, lattice(1, scale, weight), 0, 0)
    return picture


# ---------------------------------------------------------------------------------------------------------------
# The buttons: a right triangle in the top left corner (decrease) or the bottom right corner (increase).
#
# Decrease: 1 px bevel along the top and the left edge with its own shade in the corner and at the two far ends,
# a 1 px diagonal, flat fill. The glyph's square box keeps `gap` pixels from the two edges; the minus is a bar
# through the middle of the box, the plus adds the upright bar. Increase is the same picture turned by 180
# degrees, with the bevel shades of the opposite state (a raised decrease button has the light bevel, a raised
# increase button the dark one, lit from the top left). The glyph does not move when the button is pressed.

BEVEL_LIGHT = (254, 252, 250, 248, 247)     # corner, edges, end of the top edge, end of the left edge, diagonal
BEVEL_DARK = (246, 248, 250, 250, 253)
FILL, FILL_PRESSED = 250, 251


def button(size, increase, pressed, glyph):
    length, stroke, gap = glyph
    corner, edge, top_end, left_end, diagonal = BEVEL_LIGHT if increase == pressed else BEVEL_DARK
    last = size - 1
    picture = blank(size, size)
    for y in range(size):
        for x in range(size - y):
            if x == 0 and y == 0:
                c = corner
            elif y == 0:
                c = top_end if x == last else edge
            elif x == 0:
                c = left_end if y == last else edge
            elif x + y == last:
                c = diagonal
            else:
                c = FILL_PRESSED if pressed else FILL
            picture[y][x] = c
    bar = gap + (length - stroke) // 2
    for i in range(length):
        for j in range(stroke):
            picture[bar + j][gap + i] = GLYPH
            if increase:
                picture[gap + i][bar + j] = GLYPH
    if increase:
        picture = [row[::-1] for row in picture[::-1]]
    return picture


def buttons(size, glyph):
    return [button(size, increase, pressed, glyph) for increase in (False, True) for pressed in (False, True)]


def size_pictures(scale, weight=1, dot_rows=1):
    return [mountain_mark(scale, dot_rows, weight)] + [lattice(n, scale, weight) for n in range(1, 8)]


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
# Validation: the rules at the original size against the original sprites.

def differences(a, b):
    return [(x, y) for y in range(len(a)) for x in range(len(a[0])) if a[y][x] != b[y][x]]


def validate(g1):
    """Prints one line per original sprite and returns True if every one of them is reproduced exactly."""
    ok = True

    def check(name, index, mine, original=None):
        nonlocal ok
        original = original or g1.in_box(index, len(mine[0]), len(mine))
        wrong = differences(mine, original)
        _, width, height, x, y = g1.entry(index)[:5]
        print("  %d %-24s %2dx%-2d at (%d,%d): %s" % (
            index, name, width, height, x, y, "match" if not wrong else "MISMATCH at %d pixels, first %s" % (
                len(wrong), wrong[:8])))
        ok = ok and not wrong
        return original

    print("Validation against %s" % os.path.relpath(G1_PATH, ROOT).replace(os.sep, "/"))
    names = ["decrease", "decrease pressed", "increase", "increase pressed"]
    for i, picture in enumerate(buttons(BUTTON_SIZE, GLYPH_16)):
        check(names[i], SPR_BUTTONS + i, picture)
    check("mountain mark", SPR_LAND_TOOL_SIZE_0, mountain_mark(1))
    for n in range(1, 8):
        if n != 6:
            check("size %d" % n, SPR_LAND_TOOL_SIZE_0 + n, lattice(n, 1))
            continue
        original = check("size 6 (five pastes)", SPR_LAND_TOOL_SIZE_0 + n, original_size_6(True))
        regular = lattice(6, 1)
        if original_size_6(False) != regular:
            print("       the same pastes with the slipped one in place do NOT give the regular lattice")
            ok = False
        else:
            wrong = differences(regular, original)
            print("       the regular lattice (= the same pastes with the slipped one in place) differs from the")
            print("       original at %d pixels, columns %d..%d, rows %d..%d of the box" % (
                len(wrong), min(x for x, _ in wrong), max(x for x, _ in wrong),
                min(y for _, y in wrong), max(y for _, y in wrong)))
    return ok


def check_output(sizes, button_pictures, g1):
    """The new pictures: dimensions, allowed indices, symmetry."""
    problems = []
    allowed = {0} | {c for i in range(8) for row in g1.sprite(SPR_LAND_TOOL_SIZE_0 + i)[0] for c in row}
    for n, picture in enumerate(sizes):
        if len(picture) != BOX_HEIGHT * 2 or any(len(row) != BOX_WIDTH * 2 for row in picture):
            problems.append("size picture %d has the wrong dimensions" % n)
        if not {c for row in picture for c in row} <= allowed:
            problems.append("size picture %d uses other indices than the originals" % n)
        if any(row != row[::-1] for row in picture):
            problems.append("size picture %d is not left-right symmetric" % n)
        _, _, body = crop(picture)
        if body != body[::-1]:
            problems.append("size picture %d is not top-bottom symmetric" % n)
    for i, picture in enumerate(button_pictures):
        used = {c for row in g1.sprite(SPR_BUTTONS + i)[0] for c in row}
        if len(picture) != BUTTON_SIZE * 2 or any(len(row) != BUTTON_SIZE * 2 for row in picture):
            problems.append("button %d has the wrong dimensions" % i)
        if not {c for row in picture for c in row} <= used:
            problems.append("button %d uses other indices than its original" % i)
        _, _, glyph = crop([[c == GLYPH for c in row] for row in picture])
        if glyph != glyph[::-1] or any(row != row[::-1] for row in glyph) or len(glyph) % 2 == 0                 or len(glyph[0]) % 2 == 0 or (i >= 2 and glyph != [list(col) for col in zip(*glyph)]):
            problems.append("button %d: the glyph is not symmetric about a centre pixel" % i)
    return problems


# ---------------------------------------------------------------------------------------------------------------
# The header

def header_text(sizes, button_pictures):
    def array(pictures):
        out = []
        for picture in pictures:
            values = [c for row in picture for c in row]
            out.append("    {\n")
            for i in range(0, len(values), 32):
                out.append("        " + " ".join("%d," % v for v in values[i:i + 32]) + "\n")
            out.append("    },\n")
        return "".join(out)

    return (
        "// Generated by scripts/n3ds_tool_pictures.py. Do not edit.\n"
        "#define N3DS_TOOL_SIZE_PICTURE_WIDTH %d\n"
        "#define N3DS_TOOL_SIZE_PICTURE_HEIGHT %d\n"
        "#define N3DS_TOOL_BUTTON_PICTURE_SIZE %d\n"
        "// [n] = tool size n (0 = the mountain tool's mark). Palette indices, 0 = transparent, row by row.\n"
        "static const uint8 N3dsToolSizePictures[%d][N3DS_TOOL_SIZE_PICTURE_WIDTH * N3DS_TOOL_SIZE_PICTURE_HEIGHT]"
        " = {\n%s};\n"
        "// Decrease, decrease pressed, increase, increase pressed. Indices 243..254 take the window's colour.\n"
        "static const uint8 N3dsToolButtonPictures[%d][N3DS_TOOL_BUTTON_PICTURE_SIZE * N3DS_TOOL_BUTTON_PICTURE_SIZE]"
        " = {\n%s};\n"
    ) % (len(sizes[0][0]), len(sizes[0]), len(button_pictures[0]), len(sizes), array(sizes),
         len(button_pictures), array(button_pictures))


# ---------------------------------------------------------------------------------------------------------------
# The comparison sheet

def control(size_layers, button_pair, scale):
    """The control as the game composes it: the inset box (dark line on the top and left, light line on the bottom
    and right), the size picture(s), then the two buttons over them: decrease 1 px inside the top left corner,
    increase 1 px inside the bottom right corner. The border and the buttons use remap indices; the size pictures
    use indices the remap table leaves alone, so the sheet draws the whole control through the table."""
    width, height = BOX_WIDTH * scale, BOX_HEIGHT * scale
    picture = blank(width, height)
    for i in range(width):
        picture[0][i], picture[height - 1][i] = 246, 252
    for j in range(height):
        picture[j][0], picture[j][width - 1] = 246, 252
    for layer in size_layers:
        paste(picture, layer, 0, 0)
    size = len(button_pair[0])
    paste(picture, button_pair[0], 1, 1)
    paste(picture, button_pair[1], width - 1 - size, height - 1 - size)
    return picture


def write_sheet(g1, path):
    from PIL import Image, ImageDraw, ImageFont

    palette = g1.palette()
    table = g1.remap_table(SHEET_COLOUR)
    background = palette[table[249]]
    font = ImageFont.load_default()
    ink, faint = (255, 255, 255), palette[table[246]]
    zoom, gap, margin, line = 3, 14, 12, 13

    original_sizes = [g1.in_box(SPR_LAND_TOOL_SIZE_0 + n, BOX_WIDTH, BOX_HEIGHT) for n in range(8)]
    original_buttons = [g1.in_box(SPR_BUTTONS + i, BUTTON_SIZE, BUTTON_SIZE) for i in range(4)]
    new_sizes = size_pictures(2, dot_rows=MARK_DOT_ROWS)
    bold_sizes = size_pictures(2, weight=2, dot_rows=MARK_DOT_ROWS)
    new_buttons = buttons(BUTTON_SIZE * 2, GLYPH_32)

    # A row of the sheet: (label, [(caption, picture, remapped)]); every picture is shown at 1:1 and zoomed.
    sections = []
    rows = []
    for n in range(8):
        versions = [("original", original_sizes[n]), ("pixel-doubled (rejected)", doubled(original_sizes[n])),
                    ("new", new_sizes[n])]
        if n == 0:
            every = ("row", "second row") if MARK_DOT_ROWS == 1 else ("second row", "row")
            versions.append(("alternative: a dot on every %s" % every[1], mountain_mark(2, 3 - MARK_DOT_ROWS)))
            label = "Size 0 = the mountain tool's mark (sprite %d). New: 16x8 tiles, a single-pixel dot on every "                     "%s of each line" % (SPR_LAND_TOOL_SIZE_0, every[0])
        else:
            versions.append(("alternative: lines 2 rows thick", bold_sizes[n]))
            label = "Size %d (sprite %d). New: 16x8 tiles, lines as in the original" % (n, SPR_LAND_TOOL_SIZE_0 + n)
        rows.append((label, [(c, p, False) for c, p in versions]))
    sections.append(("TOOL SIZE PICTURES (box 44x32 -> 88x64)", rows))

    rows = []
    names = ["Decrease", "Decrease pressed", "Increase", "Increase pressed"]
    for i in range(4):
        versions = [("original", original_buttons[i]), ("pixel-doubled (rejected)", doubled(original_buttons[i])),
                    ("new: glyph %dx%d" % GLYPH_32[:2], new_buttons[i])]
        for glyph in GLYPH_32_CANDIDATES:
            if glyph != GLYPH_32:
                versions.append(("candidate: glyph %dx%d" % glyph[:2], buttons(BUTTON_SIZE * 2, glyph)[i]))
        rows.append(("%s (sprite %d)" % (names[i], SPR_BUTTONS + i), [(c, p, True) for c, p in versions]))
    sections.append(("BUTTONS (16x16 -> 32x32), drawn through the remap table of colour %d" % SHEET_COLOUR, rows))

    rows = []
    for label, n, under in (("Size 1", 1, None), ("Size 4", 4, None), ("Size 7", 7, None),
                            ("The mountain tool's mark over size 3", 0, 3)):
        def layers(pictures, transform=lambda p: p):
            return [transform(pictures[k]) for k in ([under, n] if under else [n])]
        versions = [
            ("original", control(layers(original_sizes), original_buttons[0::2], 1)),
            ("pixel-doubled (rejected)", control(layers(original_sizes, doubled),
                                                 [doubled(b) for b in original_buttons[0::2]], 2)),
            ("new", control(layers(new_sizes), new_buttons[0::2], 2)),
            ("alternative: lines 2 rows thick", control(layers(bold_sizes), new_buttons[0::2], 2)),
        ]
        rows.append((label + ", as the game composes it: box, size picture, then the buttons over it",
                     [(c, p, True) for c, p in versions]))
    sections.append(("ASSEMBLED CONTROL (88x64 box with a 1 px inset border, decrease at (1,1), increase at (55,31))",
                     rows))

    # Layout: per row a label line, a caption line, then all versions at 1:1 followed by all versions zoomed.
    measure = ImageDraw.Draw(Image.new("RGB", (1, 1)))

    def cells(versions):
        """[(x, factor, caption, picture, remapped)], total width, picture height"""
        out, x = [], margin
        for factor in (1, zoom):
            for k, (caption, picture, remapped) in enumerate(versions):
                if factor == 1:
                    caption = "1:1" if k == 0 else ""
                else:
                    caption += " %dx" % zoom if k == 0 else ""
                out.append((x, factor, caption, picture, remapped))
                x += max(len(picture[0]) * factor, int(measure.textlength(caption, font=font))) + gap
            x += 2 * gap
        return out, x - 3 * gap + margin, max(len(p) for _, p, _ in versions) * zoom

    width = max(cells(v)[1] for _, rows in sections for _, v in rows)
    height = margin + line * 2
    for _, rows in sections:
        height += line * 2 + sum(cells(v)[2] + line * 2 + 2 + gap for _, v in rows) + gap
    image = Image.new("RGB", (width, height - gap + margin), background)
    draw = ImageDraw.Draw(image)

    y = margin
    draw.text((margin, y), "Tool size pictures and buttons for the 3DS bottom screen. Background: window colour %d, "
              "shade 249 (mid light). Every row shows its versions at 1:1 first, then the same versions in the same "
              "order at %dx." % (SHEET_COLOUR, zoom), fill=ink, font=font)
    y += line * 2
    for title, rows in sections:
        draw.text((margin, y), title, fill=ink, font=font)
        draw.line((margin, y + line, width - margin, y + line), fill=faint)
        y += line * 2
        for label, versions in rows:
            draw.text((margin, y), label, fill=ink, font=font)
            placed, _, tall = cells(versions)
            for x, factor, caption, picture, remapped in placed:
                draw.text((x, y + line), caption, fill=ink, font=font)
                for j, row in enumerate(picture):
                    for i, c in enumerate(row):
                        if c:
                            left, top = x + i * factor, y + line * 2 + 2 + j * factor
                            draw.rectangle((left, top, left + factor - 1, top + factor - 1),
                                           fill=palette[table[c] if remapped else c])
            y += line * 2 + 2 + tall + gap
        y += gap
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

    sizes = size_pictures(2, dot_rows=MARK_DOT_ROWS)
    button_pictures = buttons(BUTTON_SIZE * 2, GLYPH_32)
    problems = check_output(sizes, button_pictures, g1)
    if problems:
        print("The new pictures are wrong:\n  " + "\n  ".join(problems))
        return 1

    with open(HEADER_PATH, "w", encoding="ascii", newline="\n") as f:
        f.write(header_text(sizes, button_pictures))
    print("Wrote %s (%d size pictures %dx%d, %d buttons %dx%d)" % (
        os.path.relpath(HEADER_PATH, ROOT).replace(os.sep, "/"), len(sizes), len(sizes[0][0]), len(sizes[0]),
        len(button_pictures), len(button_pictures[0][0]), len(button_pictures[0])))
    size = write_sheet(g1, SHEET_PATH)
    print("Wrote %s (%dx%d)" % (os.path.relpath(SHEET_PATH, ROOT).replace(os.sep, "/"), size[0], size[1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
