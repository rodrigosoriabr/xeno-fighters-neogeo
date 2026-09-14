#!/usr/bin/env python3
"""S-ROM (fix layer, 8x8 tiles) for the HUD: ngdevkit's base font plus lifebars, timer digits, power
gauge and win marks from tile 0x500 on (0-0x4ff hold the base text fonts).

  make_fix.py <base-srom-text-shadow.fix>

Writes build/res/s1.fix (128 KB S-ROM), build/res/hud_tiles.h (tile numbers + fix palettes) and
preview/hud_tiles.png."""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(__file__))
import neogfx as g

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIRST = 0x500
SIZE = 4096

# Fix palettes (16 colors each). Index 0 transparent.
PALETTES = {
    0: ["#000000", "#ffffff", "#101018"],                                   # base font: white + shadow
    1: ["#000000", "#08080c", "#fffce0", "#ffe040", "#ffb000", "#e07000",   # lifebar: outline, yellow ramp
        "#ff4040", "#a01010", "#1a1428", "#241c38", "#c8b8e0", "#7a6a9a", "#4a3a68", "#ffffff", "#b04000"],
    2: ["#000000", "#140800", "#fff8c0", "#ffd840", "#e8a010", "#a05808"],  # timer digits: gold
    3: ["#000000", "#060818", "#c8f4ff", "#40c8ff", "#1070e0", "#0c2a60", "#3c3c58", "#9090b8"],  # power
    4: ["#000000", "#060818", "#ffffff", "#ff80ff", "#ff20c0", "#600040", "#3c3c58", "#9090b8"],  # power MAX (cycled)
    5: ["#000000", "#200000", "#ffe0a0", "#ff6020"],                         # combo text
    6: ["#000000", "#001018", "#a0f0ff", "#40c0e0"],                         # names
    7: ["#000000", "#08080c", "#ffe0e0", "#ff6060", "#ff2020", "#b00000",   # lifebar in danger (< 25%)
        "#ffffff", "#ffa0a0", "#1a1428", "#241c38", "#c8b8e0", "#7a6a9a", "#4a3a68", "#ffffff", "#700000"],
}


def palette_words(index):
    colors = PALETTES[index] + ["#000000"] * (16 - len(PALETTES[index]))
    return [0x8000] + [g.packed15(g.to_neo([int(c[i:i + 2], 16) for i in (1, 3, 5)])) for c in colors[1:]]


def encode(tile):
    """8x8 index tile -> 32 bytes (same layout as ngdevkit tiletool.py encode_srom_tile)."""
    out = bytearray(32)
    i = 0
    for xa, xb in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            out[i] = int(tile[y][xb]) << 4 | int(tile[y][xa])
            i += 1
    return bytes(out)


tiles, names = [], {}


def add(name, tile):
    names.setdefault(name, len(tiles) + FIRST)
    tiles.append(np.array(tile, dtype=np.uint8))
    return len(tiles) - 1 + FIRST


# --- lifebar cells, 2 rows tall (top, bottom). life px + trail px filled from the inner side.
# 16 rows over the two halves: outline, white gloss, bright band, then deeper orange to the bottom
RAMP = [1, 13, 2, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 14, 14, 1]
def bar_cell(life, trail, from_right, row):
    t = np.zeros((8, 8), np.uint8)
    for y in range(8):
        yy = y + 8 * row
        for x in range(8):
            px = 7 - x if from_right else x
            if RAMP[yy] == 1:
                t[y][x] = 1
            elif px < life:
                t[y][x] = RAMP[yy]
            elif px < life + trail:
                t[y][x] = 6 if yy < 7 else 7
            else:
                t[y][x] = 8 if ((x + yy) // 2) % 2 else 9    # dark checker in the empty part
    return t


for side, from_right in (("L", True), ("R", False)):
    for row in (0, 1):
        for life in range(9):
            for trail in range(9 - life):
                add(f"bar{side}{row}_{life}_{trail}", bar_cell(life, trail, from_right, row))
# bar caps (outer ends): ornament bracket
def cap(outer_left, row):
    t = np.zeros((8, 8), np.uint8)
    for y in range(8):
        for x in range(8):
            px = x if outer_left else 7 - x
            edge = (row == 0 and y == 0) or (row == 1 and y == 7)
            if px >= 5 and (edge or px == 5 or (row == 0 and y == 1) or (row == 1 and y == 6)):
                t[y][x] = 12 if px == 5 else 1
            elif px >= 6:
                t[y][x] = 1
            if px in (3, 4) and 2 <= y <= 5:
                t[y][x] = 10 if y < 4 else 11
    return t
for side in "LR":
    for row in (0, 1):
        add(f"cap{side}{row}", cap(side == "L", row))

# --- power gauge: one row, fill 0..8 px from the left (P1) or right (P2)
def power_cell(fill, from_right):
    t = np.zeros((8, 8), np.uint8)
    for y in range(8):
        for x in range(8):
            px = 7 - x if from_right else x
            if y in (0, 7):
                t[y][x] = 7
            elif y in (1, 6):
                t[y][x] = 1
            elif px < fill:
                t[y][x] = 2 if y == 2 else 3 if y < 5 else 4
            else:
                t[y][x] = 5 if (x + y) % 2 else 6
    return t
for side, from_right in (("L", False), ("R", True)):
    for fill in range(9):
        add(f"pow{side}_{fill}", power_cell(fill, from_right))

# --- timer digits 16x24 (2x3 tiles), rendered from Arial Black with outline
font = ImageFont.truetype("/System/Library/Fonts/Supplemental/Arial Black.ttf", 26)
for d in "0123456789":
    img = Image.new("L", (16, 24), 0)
    dr = ImageDraw.Draw(img)
    w = dr.textlength(d, font=font)
    dr.text(((16 - w) / 2, -7), d, font=font, fill=255)
    a = np.array(img) > 110
    grid = np.zeros((24, 16), np.uint8)
    for y in range(24):
        for x in range(16):
            if a[y, x]:
                grid[y, x] = 2 if y < 8 else 3 if y < 15 else 4 if y < 20 else 5
            elif a[max(0, y - 1):y + 2, max(0, x - 1):x + 2].any():
                grid[y, x] = 1
    for ty in range(3):
        for tx in range(2):
            add(f"digit{d}_{tx}{ty}", grid[ty * 8:(ty + 1) * 8, tx * 8:(tx + 1) * 8])

# --- win mark (emblem) and infinity for the timer
star = ["...33...", "..3223..", "33222233", ".322223.", "..3223..", ".32..23.", "33....33", "........"]
add("win", [[{".": 0, "3": 3, "2": 2}[c] for c in row] for row in star])
add("win_empty", [[{".": 0, "3": 5, "2": 0}[c] for c in row] for row in star])

# --- solid dark tile (letterbox bands behind story text)
add("solid", [[2] * 8 for _ in range(8)])

# --- opaque copy of the ASCII font (32-95): transparent pixels become the band color, so text typed over
# a letterbox band doesn't let the sprite art behind show through the gaps of every glyph
base_font = open(sys.argv[1], "rb").read()
def decode(index):
    raw = base_font[index * 32:(index + 1) * 32]
    t = np.zeros((8, 8), np.uint8)
    i = 0
    for xa, xb in ((4, 5), (6, 7), (0, 1), (2, 3)):
        for y in range(8):
            t[y][xa] = raw[i] & 15
            t[y][xb] = raw[i] >> 4
            i += 1
    return t
for c in range(32, 96):
    g_ = decode(c)
    g_[g_ == 0] = 2
    add(f"opaque_{c}", g_)

# --- output
base = bytearray(open(sys.argv[1], "rb").read())
rom = base + bytes(SIZE * 32 - len(base))
for i, t in enumerate(tiles):
    rom[(FIRST + i) * 32:(FIRST + i + 1) * 32] = encode(t)
out_dir = os.path.join(ROOT, "build", "res"); os.makedirs(out_dir, exist_ok=True)
open(os.path.join(out_dir, "s1.fix"), "wb").write(bytes(rom))

lines = ["/* generated by tools/make_fix.py */", "#ifndef HUD_TILES_H", "#define HUD_TILES_H"]
def first(prefix):
    return names[prefix]
lines.append(f"#define FIX_BAR_L0 {first('barL0_0_0')}  /* + bar_index(life, trail) */")
lines.append(f"#define FIX_BAR_L1 {first('barL1_0_0')}")
lines.append(f"#define FIX_BAR_R0 {first('barR0_0_0')}")
lines.append(f"#define FIX_BAR_R1 {first('barR1_0_0')}")
lines.append("/* cells for life px l (0..8) and trail px t (0..8-l) are stored in order l, then t */")
lines.append("static const u8 bar_index[9] = {" + ",".join(str(sum(9 - k for k in range(l))) for l in range(9)) + "};")
lines.append(f"#define FIX_OPAQUE_FONT {names['opaque_32']}  /* + ascii - 32, for 32..95 */")
for n in ("capL0", "capL1", "capR0", "capR1", "win", "win_empty", "solid"):
    lines.append(f"#define FIX_{n.upper()} {names[n]}")
lines.append(f"#define FIX_POW_L {names['powL_0']}  /* + fill 0..8 */")
lines.append(f"#define FIX_POW_R {names['powR_0']}")
lines.append(f"#define FIX_DIGIT {names['digit0_00']}  /* + digit * 6 + tx + ty * 2 */")
lines.append(f"#define FIX_PALETTES {len(PALETTES)}")
lines.append("static const u16 fix_palettes[] = {" + ",".join(f"0x{w:04x}" for p in range(len(PALETTES)) for w in palette_words(p)) + "};")
lines.append("#endif")
open(os.path.join(out_dir, "hud_tiles.h"), "w").write("\n".join(lines) + "\n")

# preview
cols = 32
img = Image.new("RGB", (cols * 8, -(-len(tiles) // cols) * 8), (40, 40, 60))
for i, t in enumerate(tiles):
    pal = 1 if i < names["powL_0"] - FIRST else 3 if i < names["digit0_00"] - FIRST else 2
    colors = PALETTES[pal] + ["#000000"] * 16
    rgb = np.array([[int(colors[v][k:k + 2], 16) for k in (1, 3, 5)] for v in range(16)], np.uint8)
    img.paste(Image.fromarray(rgb[t]), ((i % cols) * 8, (i // cols) * 8))
os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(os.path.join(ROOT, "preview", "hud_tiles.png"))
print(f"hud: {len(tiles)} fix tiles from 0x{FIRST:x}")
