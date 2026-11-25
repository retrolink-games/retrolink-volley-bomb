#!/usr/bin/env python3
"""Generates every Link Volley bitmap from code (no third-party art).

Designs come from the RetroLink website hero (retro-local-multiplayer/website/main.js):
the 10x10 blob, the 3x5 digit font (extended here to a full original alphabet), the sand ball,
the night scene (sky bands, stars, three ridges, trees) and the per-player palettes.

Writes 4bpp (16 colour) uncompressed BMPs + Butano JSON files into graphics/.
Run: python3 tools/make_assets.py
"""
import json
import math
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "graphics")

DUMMY = (255, 0, 255)          # palette index 0 (transparent / never drawn on backgrounds)
DARK = (0x12, 0x12, 0x16)      # --ground
SAND = (0xF2, 0xCF, 0x8A)      # --link
CREAM = (0xCD, 0xF0, 0xEE)

# Per-player colourways. P1/P2 are the site's dark-theme palettes; P3 (sand) and P4 (violet) extend them.
PLAYERS = [
    dict(colour=(0x5F, 0xD6, 0xCA), bands=["#0d1d22", "#0f2328", "#11292e", "#132f34"],
         far="#16393c", mid="#1d4f50", near="#256563", star="#cdf0ee"),
    dict(colour=(0xFF, 0x8A, 0x5C), bands=["#22130f", "#281612", "#2f1a15", "#361e18"],
         far="#3d211a", mid="#55291f", near="#6e3424", star="#f2cf8a"),
    dict(colour=(0xF2, 0xCF, 0x8A), bands=["#1f1a0d", "#251f10", "#2b2413", "#322a16"],
         far="#3d3219", mid="#57461f", near="#715b26", star="#fff3d6"),
    dict(colour=(0xB4, 0x9C, 0xFF), bands=["#16112a", "#1a1431", "#1e1738", "#221a40"],
         far="#2a2050", mid="#3a2c6e", near="#4b3a8c", star="#e4dcff"),
]


def hexc(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(round(a[i] * (1 - t) + b[i] * t) for i in range(3))


def write_bmp4(path, pixels, w, h, palette):
    assert len(palette) <= 16
    palette = list(palette) + [(0, 0, 0)] * (16 - len(palette))
    row_bytes = ((w + 1) // 2 + 3) & ~3
    data = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(0, w, 2):
            a = pixels[y][x]
            b = pixels[y][x + 1] if x + 1 < w else 0
            row.append((a << 4) | b)
        row += b"\0" * (row_bytes - len(row))
        data += row
    pal = b"".join(struct.pack("<BBBB", c[2], c[1], c[0], 0) for c in palette)
    offset = 14 + 40 + len(pal)
    header = struct.pack("<2sIHHI", b"BM", offset + len(data), 0, 0, offset)
    info = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 4, 0, len(data), 2835, 2835, 16, 16)
    with open(path, "wb") as f:
        f.write(header + info + pal + data)


def write_json(name, obj):
    with open(os.path.join(OUT, name + ".json"), "w") as f:
        json.dump(obj, f, indent=4)
        f.write("\n")


def canvas(w, h, v=0):
    return [[v] * w for _ in range(h)]


def rect(px, x, y, w, h, v):
    for yy in range(max(0, y), min(len(px), y + h)):
        for xx in range(max(0, x), min(len(px[0]), x + w)):
            px[yy][xx] = v


class Rand:
    """The website's seeded LCG, so the scenery matches its look."""

    def __init__(self, seed):
        self.s = seed & 0xFFFFFFFF

    def __call__(self):
        self.s = (self.s * 1664525 + 1013904223) & 0xFFFFFFFF
        return self.s / 4294967296


# ---------------------------------------------------------------- backgrounds
SCREEN_W, SCREEN_H = 240, 160
FLOOR_INSET = 12        # must match sim::FLOOR_INSET
WORLD_PERIOD = 480      # ridges are continuous across the two screens of a row


def background(slot):
    p = PLAYERS[slot]
    pal = [DUMMY] + [hexc(c) for c in p["bands"]]          # 1..4 bands
    far, mid, near = hexc(p["far"]), hexc(p["mid"]), hexc(p["near"])
    star = hexc(p["star"])
    pal += [far, mid, near, p["colour"], star, mix(pal[2], star, 0.45), SAND, mix(mid, SAND, 0.25), DARK]
    # 5 far, 6 mid, 7 near, 8 rim (player colour), 9 star, 10 speck, 11 sand, 12 dash, 13 dark
    px = canvas(256, 256, 1)
    ground_y = SCREEN_H - FLOOR_INSET
    left = SCREEN_W if slot >= 2 else 0   # layout P2 P4 / P1 P3

    # Sky bands, darkest at the top (the website's 34px steps, scaled to 160).
    for i in range(4):
        rect(px, 0, i * 30, 256, SCREEN_H - i * 30, 1 + i)

    r = Rand(11 + slot)
    for _ in range(60):
        x, y = int(r() * SCREEN_W), int(r() * SCREEN_H * 0.55)
        px[y][x] = 9 if r() > 0.8 else 10

    def ridge(seed, base, amp, colour, cap=None):
        rr = Rand(seed)
        ph = [rr() * 6.28, rr() * 6.28, rr() * 6.28]
        for x in range(SCREEN_W):
            wx = left + (SCREEN_W - 1 - x if slot & 1 else x)   # top-row screens are seen rotated 180 degrees
            t = wx / WORLD_PERIOD * math.pi * 2
            y = round(base - amp * (0.55 * math.sin(t * 2 + ph[0]) + 0.3 * math.sin(t * 5 + ph[1])
                                    + 0.15 * math.sin(t * 11 + ph[2])))
            rect(px, x, y, 1, ground_y - y, colour)
            if cap:
                px[y][x] = cap

    ridge(12, 100, 26, 5)
    ridge(13, 120, 15, 6)
    ridge(14, ground_y - 4, 5, 7, 8)

    # Little trees, from the site's near layer.
    rt = Rand(20 + slot)
    for _ in range(6):
        x, h = int(rt() * (SCREEN_W - 8)), 8 + int(rt() * 8)
        base = ground_y - 2
        rect(px, x + 2, base - h + 5, 2, h - 5, 7)
        rect(px, x + 1, base - h, 4, 1, 7)
        rect(px, x, base - h + 1, 6, 4, 7)
        rect(px, x + 1, base - h + 5, 4, 1, 7)

    # Court: mid fill, sand line, dashed line.
    rect(px, 0, ground_y, 256, FLOOR_INSET, 6)
    rect(px, 0, ground_y, 256, 1, 11)
    for x in range(0, SCREEN_W, 8):
        rect(px, x + 2, ground_y + 6, 3, 1, 12)

    # Every console draws its floor at the bottom: P3/P4 sit across the table, so their consoles show
    # their quarter of the world rotated 180 degrees (the ridges above follow that rotation to stay seamless).

    name = "bg_p%d" % (slot + 1)
    write_bmp4(os.path.join(OUT, name + ".bmp"), px, 256, 256, pal)
    write_json(name, {"type": "regular_bg"})


# ---------------------------------------------------------------- sprites
BLOB = ["0011111100", "0111111110", "1111111111", "1100000011", "1102200211",
        "1111111111", "1111111111", "0111111110", "0110000110", "0110000110"]


def blobs():
    # 32x32 frames: 0 stand, 1 squash (top rows pressed down, like the site), 2 jump (legs tucked),
    # 3 rocket (legs tucked, body raised, flame under the feet).
    flame = [
        "0034444300",
        "0003443000",
        "0000330000",
    ]
    for slot, p in enumerate(PLAYERS):
        px = canvas(32, 128)
        for frame in range(4):
            lift = -6 if frame == 3 else 0
            for r in range(10):
                for c in range(10):
                    v = BLOB[r][c]
                    if v == "0":
                        continue
                    if frame >= 2 and r >= 8:
                        continue
                    dy = 1 if (frame == 1 and r < 4) else 0
                    dy += 1 if frame >= 2 else 0
                    col = 2 if v == "2" else 1
                    rect(px, 6 + c * 2, frame * 32 + 12 + lift + (r + dy) * 2, 2, 2, col)
            if frame == 3:
                for r, row in enumerate(flame):
                    for c, ch in enumerate(row):
                        if ch != "0":
                            rect(px, 6 + c * 2, frame * 32 + 26 + r * 2, 2, 2, int(ch))
        name = "blob_p%d" % (slot + 1)
        write_bmp4(os.path.join(OUT, name + ".bmp"), px, 32, 128,
                   [DUMMY, p["colour"], DARK, (0xFF, 0xF3, 0xD6), (0xFF, 0x8A, 0x5C)])
        write_json(name, {"type": "sprite", "height": 32})


def smoke():
    # 8x8 frames: a fresh puff, a spreading one, a fading wisp. Original.
    px = canvas(8, 24)
    for f, (r, v) in enumerate([(3.2, 1), (3.8, 2), (2.6, 3)]):
        for y in range(8):
            for x in range(8):
                d = math.hypot(x + 0.5 - 4, y + 0.5 - 4)
                if d < r and (f < 2 or (x + y) % 2 == 0):
                    px[f * 8 + y][x] = v
    write_bmp4(os.path.join(OUT, "smoke.bmp"), px, 8, 24, [DUMMY, (0xE8, 0xE4, 0xDA), (0xA8, 0xA4, 0x9C), (0x6A, 0x66, 0x60)])
    write_json("smoke", {"type": "sprite", "height": 8})


def ball():
    # Frame 0: the ball. Frames 1..3: fading trail copies (no alpha on GBA sprites, so darker colours).
    px = canvas(8, 32)
    for f in range(4):
        rect(px, 0, f * 8 + 2, 8, 4, 1 + f)
        rect(px, 2, f * 8, 4, 8, 1 + f)
    pal = [DUMMY, SAND, mix(SAND, DARK, 0.45), mix(SAND, DARK, 0.65), mix(SAND, DARK, 0.8)]
    write_bmp4(os.path.join(OUT, "ball.bmp"), px, 8, 32, pal)
    write_json("ball", {"type": "sprite", "height": 8})


# 3x5 glyphs. Digits are the website's; letters and symbols are drawn for this game.
GLYPHS = {
    "0": "111101101101111", "1": "010110010010111", "2": "111001111100111", "3": "111001111001111",
    "4": "101101111001001", "5": "111100111001111", "6": "111100111101111", "7": "111001001001001",
    "8": "111101111101111", "9": "111101111001111",
    "A": "010101111101101", "B": "110101110101110", "C": "011100100100011", "D": "110101101101110",
    "E": "111100110100111", "F": "111100110100100", "G": "011100101101011", "H": "101101111101101",
    "I": "111010010010111", "J": "001001001101010", "K": "101101110101101", "L": "100100100100111",
    "M": "101111111101101", "N": "110101101101101", "O": "010101101101010", "P": "110101110100100",
    "Q": "010101101110011", "R": "110101110101101", "S": "011100010001110", "T": "111010010010010",
    "U": "101101101101111", "V": "101101101101010", "W": "101101111111101", "X": "101101010101101",
    "Y": "101101010010010", "Z": "111001010100111",
    ".": "000000000000010", ",": "000000000010100", ":": "000010000010000", "-": "000000111000000",
    "!": "010010010000010", "?": "110001010000010", "/": "001001010100100", "'": "010010000000000",
    "(": "001010010010001", ")": "100010010010100", "+": "000010111010000", "*": "000101010101000",
    "=": "000111000111000", "<": "001010100010001", ">": "100010001010100", "#": "101111101111101",
    "_": "000000000000111", "%": "101001010100101", "[": "011010010010011", "]": "110010010010110",
    "@": "010101111100011", "&": "010101010101011", "|": "010010010010010", "~": "000011110000000",
}


FONT_DIR = os.path.join(ROOT, "tools", "fonts")


def font(name, ttf, scale, cell):
    """RetroLink's brand font (Silkscreen, SIL OFL 1.1) rasterised on its native pixel grid
    (8 px size) and scaled up `scale` times into `cell` x `cell` sprite cells. Also returns the
    proportional advance widths (ASCII 32..126) for Butano's variable-width text."""
    from PIL import Image, ImageDraw, ImageFont
    f = ImageFont.truetype(os.path.join(FONT_DIR, ttf), 8)
    chars = [chr(c) for c in range(33, 127)]
    px = canvas(cell, cell * len(chars))
    widths = [max(2, round(f.getlength(" "))) * scale]
    for i, ch in enumerate(chars):
        img = Image.new("1", (12, 12), 0)
        ImageDraw.Draw(img).text((0, 7), ch, font=f, fill=1, anchor="ls")
        bbox = img.getbbox()
        adv = round(f.getlength(ch))
        widths.append(min(cell, (adv + 1) * scale if bbox else 0))
        oy = (cell - 8 * scale) // 2 + scale
        for y in range(12):
            for x in range(12):
                if img.getpixel((x, y)):
                    rect(px, x * scale, i * cell + oy + (y - 2) * scale, scale, scale, 1)
    write_bmp4(os.path.join(OUT, name + ".bmp"), px, cell, cell * len(chars), [DUMMY, CREAM])
    write_json(name, {"type": "sprite", "height": cell})
    return widths


def write_widths(fonts):
    lines = ["// Generated by tools/make_assets.py: advance widths for ASCII 32..126.",
             "#ifndef LV_FONT_WIDTHS_H", "#define LV_FONT_WIDTHS_H", "", "#include <stdint.h>", ""]
    for name, widths in fonts:
        lines.append("constexpr int8_t %s_widths[] = { %s };" % (name, ", ".join(str(w) for w in widths)))
    lines += ["", "#endif", ""]
    with open(os.path.join(ROOT, "include", "lv_font_widths.h"), "w") as fh:
        fh.write("\n".join(lines))


def explosion():
    # 32x32 frames: flash, fireball, ring of sparks, smoke. Original.
    px = canvas(32, 128)
    cx = cy = 16
    r = Rand(5)
    for f in range(4):
        oy = f * 32
        for y in range(32):
            for x in range(32):
                d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
                v = 0
                if f == 0:
                    v = 1 if d < 5 else (2 if d < 7 else 0)
                elif f == 1:
                    v = 1 if d < 6 else (2 if d < 10 else (3 if d < 12 else 0))
                elif f == 2:
                    v = 3 if 10 <= d < 14 else (4 if 7 <= d < 10 else 0)
                else:
                    v = 5 if 12 <= d < 15 and (x + y) % 3 == 0 else 0
                if v:
                    px[oy + y][x] = v
        for _ in range(10 if f >= 1 else 0):
            a = r() * math.pi * 2
            d = 8 + f * 3 + r() * 3
            x, y = int(cx + math.cos(a) * d), int(cy + math.sin(a) * d)
            if 0 <= x < 32 and 0 <= y < 32:
                px[oy + y][x] = 1 if f < 3 else 5
    pal = [DUMMY, (0xFF, 0xF3, 0xD6), SAND, (0xFF, 0x8A, 0x5C), (0xC2, 0x3B, 0x3B), (0x6A, 0x5A, 0x55)]
    write_bmp4(os.path.join(OUT, "boom.bmp"), px, 32, 128, pal)
    write_json("boom", {"type": "sprite", "height": 32})


def minimap():
    # 32x32 frames, one per player count (1..4). The 480x320 world at 1/16 scale: 30x20 inside a 1px sand border,
    # rows 5..26. Quarters with a player are tinted in that player's colour, empty ones stay dark.
    occupied = {1: [0, 1], 2: [0, 1], 3: [0, 1, 2], 4: [0, 1, 2, 3]}
    pal = [DUMMY, SAND, DARK] + [mix(p["colour"], DARK, 0.62) for p in PLAYERS] + [(0x26, 0x26, 0x2E)]
    px = canvas(32, 128)
    for f, n in enumerate(range(1, 5)):
        oy = f * 32 + 5
        rect(px, 0, oy, 32, 22, 1)
        rect(px, 1, oy + 1, 30, 20, 2)
        for slot in range(4):
            right, top = slot >= 2, slot & 1
            qx = 1 + (15 if right else 0)
            qy = oy + 1 + (0 if top else 10)
            colour = 3 + slot if slot in occupied[n] else 7
            rect(px, qx + (1 if right else 0), qy + (0 if top else 1), 14, 9, colour)
    write_bmp4(os.path.join(OUT, "minimap.bmp"), px, 32, 128, pal)
    write_json("minimap", {"type": "sprite", "height": 32})

    # 8x8 dots: 0 ball (cream, 2x2), 1..4 players (their colour, 2x2 with a dark rim for contrast).
    dots = canvas(8, 40)
    rect(dots, 3, 3, 2, 2, 1)
    for slot in range(4):
        rect(dots, 2, 8 * (slot + 1) + 2, 4, 4, 6)
        rect(dots, 3, 8 * (slot + 1) + 3, 2, 2, 2 + slot)
    dpal = [DUMMY, (0xFF, 0xF3, 0xD6)] + [p["colour"] for p in PLAYERS] + [DARK]
    write_bmp4(os.path.join(OUT, "map_dot.bmp"), dots, 8, 40, dpal)
    write_json("map_dot", {"type": "sprite", "height": 8})


def confetti():
    # 8x8 frames: 5 colours (P1..P4, cream) x 3 spins (flat, diagonal, edge-on). Original.
    shapes = [
        [(2, 3), (3, 3), (4, 3), (5, 3), (2, 4), (3, 4), (4, 4), (5, 4)],
        [(2, 5), (3, 4), (4, 3), (5, 2), (3, 5), (4, 4), (5, 3), (2, 4)],
        [(3, 2), (3, 3), (3, 4), (3, 5), (4, 2), (4, 3), (4, 4), (4, 5)],
    ]
    px = canvas(8, 8 * 15)
    for c in range(5):
        for k, shape in enumerate(shapes):
            oy = (c * 3 + k) * 8
            for x, y in shape:
                px[oy + y][x] = 1 + c
    pal = [DUMMY] + [p["colour"] for p in PLAYERS] + [(0xFF, 0xF3, 0xD6)]
    write_bmp4(os.path.join(OUT, "confetti.bmp"), px, 8, 8 * 15, pal)
    write_json("confetti", {"type": "sprite", "height": 8})


def main():
    os.makedirs(OUT, exist_ok=True)
    for slot in range(4):
        background(slot)
    blobs()
    ball()
    explosion()
    minimap()
    confetti()
    smoke()
    small = font("font_small", "Silkscreen-Regular.ttf", 2, 16)
    big = font("font_big", "Silkscreen-Bold.ttf", 4, 32)
    tiny = font("font_tiny", "Silkscreen-Regular.ttf", 1, 8)
    write_widths([("font_tiny", tiny), ("font_small", small), ("font_big", big)])
    print("assets written to", OUT)


if __name__ == "__main__":
    main()
