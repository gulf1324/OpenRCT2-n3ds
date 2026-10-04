#!/usr/bin/env python3
"""타이틀 화면의 RollerCoaster Tycoon 2 로고에 덧그린 픽셀을 그림 편집기로 고칠 수 있게 한다.

게임은 원본의 로고 그림(g1.dat의 23212번, 196x100)을 그리고, 이 스크립트가 만든 헤더
(external/OpenRCT2/src/windows/n3ds_title_logo_pixels.h)대로 그 위에 픽셀을 덧그리고 지운다. 원본 그림은
오른쪽 끝에서 마지막 'r'이 잘려 있어서(테두리 없이 노란 속에서 끝난다) 픽셀을 더해 마무리했다.

  python scripts/n3ds_title_logo.py export   지금 게임에 나오는 로고를 build/title_logo_edit.png 로 꺼낸다
                                             (200x106, 바탕 투명, 1배). 이 파일을 그림 편집기로 고친다
  python scripts/n3ds_title_logo.py apply    고친 그림을 원본 그림과 비교해 달라진 픽셀만 헤더에 쓴다.
                                             그다음 빌드: cmake --build build/openrct2
                                             확인용 확대 그림: build/title_logo_preview.png

고칠 때:
  - 그림 크기(200x106)를 바꾸지 말 것. 로고는 (2, 3)부터 그려져 있고 원본 그림은 x = 196에서 끝난다.
    창이 200x106이라 그 안이면 어디든 픽셀을 더할 수 있다.
  - 원본 그림의 픽셀도 다른 색으로 바꾸거나(그 위에 덧그린다) 투명으로 지울 수 있다.
  - 색은 게임 팔레트의 색으로 바뀐다. 그림 안에 이미 있는 색을 스포이트로 찍어 쓰면 그대로 들어간다.
    팔레트에 없는 색은 가장 가까운 색이 된다(apply가 알려 준다).
  - 반투명은 없다: 불투명도 절반 이상이면 그 색, 아니면 투명.
편집용 그림에는 게임 원본의 로고가 들어 있으므로 build/ 에만 두고 git에 올리지 않는다. 헤더에는 덧그린
픽셀과 지운 픽셀의 자리만 들어간다.
"""
import os, re, sys
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import n3ds_tool_pictures as tool

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HEADER = os.path.join(ROOT, "external", "OpenRCT2", "src", "windows", "n3ds_title_logo_pixels.h")
EDIT = os.path.join(ROOT, "build", "title_logo_edit.png")
PREVIEW = os.path.join(ROOT, "build", "title_logo_preview.png")

SPR_MENU_LOGO = 23212
WIDTH, HEIGHT = 200, 106            # the logo's window (title_logo.c)
# The palette colours a pixel may get: 10..229. The others are outside the game's palette (0..9,
# 246..255) or change every frame (230..245: water, chain lift), see n3ds_drawing.c
FIRST_COLOUR, LAST_COLOUR = 10, 229


def sprite_canvas(g1):
    """The logo as the game draws it into its window: palette indices, 0 = transparent."""
    rows, x_offset, y_offset = g1.sprite(SPR_MENU_LOGO)
    canvas = [[0] * WIDTH for _ in range(HEIGHT)]
    for y, row in enumerate(rows):
        for x, value in enumerate(row):
            if value and 0 <= x + x_offset < WIDTH and 0 <= y + y_offset < HEIGHT:
                canvas[y + y_offset][x + x_offset] = value
    return canvas


def read_header():
    """([(x, top, bottom, colour)], [(x, y)]): the runs drawn and the pixels erased, from the header."""
    if not os.path.exists(HEADER):
        return [], []
    text = open(HEADER, encoding="utf-8").read()
    runs = [tuple(int(v) for v in m.groups())
            for m in re.finditer(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\},", text)]
    erased = [tuple(int(v) for v in m.groups())
              for m in re.finditer(r"\{\s*(\d+),\s*(\d+)\s*\},", text)]
    run_count = int(re.search(r"#define N3DS_TITLE_LOGO_RUN_COUNT (\d+)", text).group(1))
    erased_count = int(re.search(r"#define N3DS_TITLE_LOGO_ERASED_COUNT (\d+)", text).group(1))
    return runs[:run_count], erased[:erased_count]


def with_changes(canvas, runs, erased):
    canvas = [row[:] for row in canvas]
    for x, top, bottom, colour in runs:
        for y in range(top, bottom + 1):
            canvas[y][x] = colour
    for x, y in erased:
        canvas[y][x] = 0
    return canvas


def to_image(canvas, palette):
    image = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    pixels = image.load()
    for y in range(HEIGHT):
        for x in range(WIDTH):
            if canvas[y][x]:
                pixels[x, y] = palette[canvas[y][x]] + (255,)
    return image


def closest_colour(rgb, palette):
    best, best_error = FIRST_COLOUR, None
    for i in range(FIRST_COLOUR, LAST_COLOUR + 1):
        error = sum((a - b) ** 2 for a, b in zip(palette[i], rgb))
        if best_error is None or error < best_error:
            best, best_error = i, error
    return best, best_error


def write_preview(canvas, palette):
    """The right end of the logo enlarged, and the whole logo at twice its size, on two backgrounds."""
    zoom = 10
    left, top, right, bottom = 150, 0, WIDTH, 80
    sheet = Image.new("RGB", ((right - left) * zoom * 2 + 30, (bottom - top) * zoom + HEIGHT * 2 + 30), (30, 30, 30))
    for n, background in enumerate(((70, 100, 70), (110, 85, 60))):
        part = Image.new("RGB", ((right - left) * zoom, (bottom - top) * zoom), background)
        pixels = part.load()
        for y in range(top, bottom):
            for x in range(left, right):
                if canvas[y][x]:
                    for j in range(zoom):
                        for i in range(zoom):
                            pixels[(x - left) * zoom + i, (y - top) * zoom + j] = palette[canvas[y][x]]
        sheet.paste(part, (10 + n * (part.size[0] + 10), 10))
        whole = Image.new("RGBA", (WIDTH, HEIGHT), background + (255,))
        whole.alpha_composite(to_image(canvas, palette))
        sheet.paste(whole.convert("RGB").resize((WIDTH * 2, HEIGHT * 2), Image.NEAREST),
                    (10 + n * (part.size[0] + 10), part.size[1] + 20))
    sheet.save(PREVIEW)


def export(force):
    g1 = tool.G1(tool.G1_PATH)
    palette = g1.palette()
    runs, erased = read_header()
    image = to_image(with_changes(sprite_canvas(g1), runs, erased), palette)
    if os.path.exists(EDIT) and not force:
        if list(Image.open(EDIT).convert("RGBA").getdata()) != list(image.getdata()):
            sys.exit("%s 가 이미 있고 헤더와 다르다(고치던 그림일 수 있다).\n"
                     "덮어쓰려면: python scripts/n3ds_title_logo.py export --force" % EDIT)
    os.makedirs(os.path.dirname(EDIT), exist_ok=True)
    image.save(EDIT)
    print("wrote", EDIT, "(%dx%d, 덧그린 픽셀 %d개와 지운 픽셀 %d개 반영)"
          % (WIDTH, HEIGHT, sum(b - t + 1 for _, t, b, _ in runs), len(erased)))


def apply():
    g1 = tool.G1(tool.G1_PATH)
    palette = g1.palette()
    original = sprite_canvas(g1)
    if not os.path.exists(EDIT):
        sys.exit("없음: %s\n  먼저: python scripts/n3ds_title_logo.py export" % EDIT)
    image = Image.open(EDIT).convert("RGBA")
    if image.size != (WIDTH, HEIGHT):
        sys.exit("그림 크기가 %dx%d 이다. %dx%d 이어야 한다" % (image.size + (WIDTH, HEIGHT)))
    pixels = image.load()

    edited = [[0] * WIDTH for _ in range(HEIGHT)]       # the colour to draw over the sprite, 0 = none
    erased, inexact = [], {}
    for y in range(HEIGHT):
        for x in range(WIDTH):
            r, g, b, a = pixels[x, y]
            if a < 128:
                if original[y][x]:
                    erased.append((x, y))
                continue
            if original[y][x] and palette[original[y][x]] == (r, g, b):
                continue
            colour, error = closest_colour((r, g, b), palette)
            if error:
                inexact[(r, g, b)] = colour
            if original[y][x] and palette[original[y][x]] == palette[colour]:
                continue        # became the colour the sprite has there
            edited[y][x] = colour

    runs = []       # (x, top, bottom, colour): vertical runs of one colour
    for x in range(WIDTH):
        y = 0
        while y < HEIGHT:
            if not edited[y][x]:
                y += 1
                continue
            bottom = y
            while bottom + 1 < HEIGHT and edited[bottom + 1][x] == edited[y][x]:
                bottom += 1
            runs.append((x, y, bottom, edited[y][x]))
            y = bottom + 1

    lines = [
        "// Made by scripts/n3ds_title_logo.py of the port's repository: do not edit by hand.",
        "// The changes to the logo on the title screen (window_title_logo_paint): the picture the game",
        "// has for it (SPR_MENU_LOGO) is cut off at its right edge. Positions are in the logo's window.",
        "// To change them: n3ds_title_logo.py export, edit the picture, n3ds_title_logo.py apply.",
        "",
        "// Drawn over the picture, each run { x, top, bottom, colour }: a column of pixels of one",
        "// palette colour",
        "#define N3DS_TITLE_LOGO_RUN_COUNT %d" % len(runs),
        "static const uint8 N3dsTitleLogoRuns[%d][4] = {" % max(len(runs), 1),
    ]
    lines += ["\t{ %d, %d, %d, %d }," % run for run in runs] or ["\t{ 0, 0, 0, 0 },\t// none"]
    lines += [
        "};",
        "",
        "// Pixels of the picture that are not shown, each { x, y }",
        "#define N3DS_TITLE_LOGO_ERASED_COUNT %d" % len(erased),
        "static const uint8 N3dsTitleLogoErased[%d][2] = {" % max(len(erased), 1),
    ]
    lines += ["\t{ %d, %d }," % pixel for pixel in erased] or ["\t{ 0, 0 },\t// none"]
    lines.append("};")
    with open(HEADER, "w", encoding="utf-8", newline="") as f:
        f.write("\r\n".join(lines) + "\r\n")

    write_preview(with_changes(original, runs, erased), palette)
    print("wrote", HEADER)
    print("  덧그리는 픽셀 %d개 (세로 묶음 %d개), 지우는 픽셀 %d개"
          % (sum(b - t + 1 for _, t, b, _ in runs), len(runs), len(erased)))
    by_column = {}
    for x, top, bottom, colour in runs:
        by_column.setdefault(x, []).append("%d~%d: %d" % (top, bottom, colour) if bottom > top else "%d: %d" % (top, colour))
    for x in sorted(by_column):
        print("  x=%d  %s" % (x, ", ".join(by_column[x])))
    for (r, g, b), colour in sorted(inexact.items()):
        print("  팔레트에 없는 색 (%d, %d, %d) → 가장 가까운 %d번 %s" % (r, g, b, colour, palette[colour]))
    if erased:
        print("  지우는 원본 픽셀: %s" % ", ".join("(%d, %d)" % pixel for pixel in erased))
    print("wrote", PREVIEW)
    print("다음: cmake --build build/openrct2 (Git Bash에서 source ./env.sh 후)")


if __name__ == "__main__":
    command = sys.argv[1] if len(sys.argv) > 1 else ""
    if command == "export":
        export("--force" in sys.argv[2:])
    elif command == "apply":
        apply()
    else:
        sys.exit(__doc__)
