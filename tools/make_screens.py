#!/usr/bin/env python3
"""Screen art -> Neo Geo images: title background and logo, portraits and select icons, name plates,
HUD frames, the big arcade lettering (ROUND 1, FIGHT!, K.O. ...), intro story and ending illustrations.

  make_screens.py

Every image is its own group of frames with its own palettes. The palettes are NOT at fixed slots: the
game loads an image's palettes where the current screen needs them (screen_load() in screens.c), because
all images together need far more than the 160 palettes free for screens. build/res/screens_data.h holds
the frames, IMG_* ids, each image's palette offset/count in screen_palettes[] and screens.tiles.npy the
tiles (one bank, deduplicated across images). Missing GPT art (still being generated) falls back to a
placeholder so the game always builds."""
import os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import neogfx as g
import ui_art as ui

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "gpt")
CHARACTERS = ["vorax", "zyra", "ossk", "grumm", "krell", "nyxa", "brutok", "koral", "xal"]
PLAYABLE = CHARACTERS[:8]
KEYS = {"vorax": (255, 0, 255), "zyra": (0, 255, 0), "ossk": (255, 0, 255), "grumm": (255, 0, 255), "xal": (0, 255, 0),
        "krell": (255, 0, 255), "nyxa": (0, 255, 0), "brutok": (0, 255, 0), "koral": (255, 0, 255)}
WORDS = [["ROUND 1", "ROUND 2", "FINAL ROUND"], ["FIGHT!", "K.O.", "PERFECT"], ["TIME OVER", "YOU WIN", "YOU LOSE"],
         ["DRAW GAME", "CONTINUE?", "GAME OVER"], ["FIRST ATTACK", "COUNTER", "REVERSAL"], ["WARNING", "NEW CHALLENGER", "THE END"]]
STORY = ["intro1", "intro2", "intro3", "intro4"]


def art(path, fallback):
    p = os.path.join(ART, path)
    return p if os.path.exists(p) else os.path.join(ART, fallback)


def rgba(path, key=None, black=False):
    rgb = np.asarray(Image.open(path).convert("RGB")).astype(np.float64)
    if black:
        alpha = np.clip((rgb.max(axis=2) - 24) / 60.0, 0, 1)
    elif key:
        alpha = g.key_alpha(rgb, key)
    else:
        alpha = np.ones(rgb.shape[:2])
    return rgb, alpha


def crop(rgb, alpha):
    ys, xs = np.nonzero(alpha > 0.5)
    return rgb[ys.min():ys.max() + 1, xs.min():xs.max() + 1], alpha[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def fit(rgb, alpha, width=None, height=None):
    h, w = alpha.shape
    scale = min(width / w if width else 1e9, height / h if height else 1e9)
    return g.downscale(rgb, alpha, scale)


def fullscreen(path, height=224):
    """1536x1024 painting -> 320x224, center column crop; `height` < 224 keeps the rows above a text band
    (rows 16..16+height of the full-size picture)."""
    rgb, a = rgba(path)
    h, w = a.shape
    cw = h * 320 // 224
    x0 = (w - cw) // 2
    rgb, a = g.downscale(rgb[:, x0:x0 + cw], a[:, x0:x0 + cw], 224 / h)
    if height < 224:
        top = min(16, 224 - height)
        rgb, a = rgb[top:top + height], a[top:top + height]
    return rgb, a


class Image16:
    """One image -> frames sharing up to 16 palettes. anchor: topleft, center (x centered), bottom."""

    def __init__(self, name, parts, palettes, anchor="topleft"):
        self.name, self.parts, self.palette_count, self.anchor = name, parts, palettes, anchor


def build(images):
    bank = g.TileBank()
    bank.add(np.zeros((16, 16), np.uint8))
    out_frames, cells_out, attrs_out, sets, previews = [], [], [], [], []
    all_words = []
    for img in images:
        frames = []
        for rgb, alpha in img.parts:
            h, w = alpha.shape
            rows, cols = -(-h // 16), -(-w // 16)
            R = np.zeros((rows * 16, cols * 16, 3)); A = np.zeros((rows * 16, cols * 16))
            R[:h, :w] = rgb; A[:h, :w] = alpha
            frames.append((R, A, rows, cols, w, h))
        tiles = []
        for fi, (R, A, rows, cols, _, _) in enumerate(frames):
            for r in range(rows):
                for c in range(cols):
                    a = A[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16]
                    if (a > 0.5).any():
                        tiles.append((fi, r, c, R[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16][a > 0.5]))
        palettes, assign = g.build_palettes([t[3] for t in tiles], img.palette_count, rounds=3)
        grids = [np.zeros((rows, cols, 2), np.int32) for _, _, rows, cols, _, _ in frames]
        for (fi, r, c, _), p in zip(tiles, assign):
            R, A = frames[fi][:2]
            grid = g.map_to_palette(R[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16], A[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16], palettes[p])
            t, flip = bank.add(grid)
            grids[fi][r, c] = (t, p | flip << 4)
        first_frame = len(out_frames)
        for (R, A, rows, cols, w, h), grid in zip(frames, grids):
            x = -(w // 2) if img.anchor == "center" else 0
            y = -(h // 2) if img.anchor == "center" else 0
            # the box fields carry the real (unpadded) size: hurt = {0, 0, w, h}
            out_frames.append(f"    {{{x}, {y}, {cols}, {rows}, {len(cells_out)}, {{0, 0, {w}, {h}}}, {{0, 0, 0, 0}}}},")
            for c in range(cols):
                for r in range(rows):
                    cells_out.append(int(grid[r, c, 0])); attrs_out.append(int(grid[r, c, 1]))
            canvas = np.zeros((rows * 16, cols * 16, 3))
            for r in range(rows):
                for c in range(cols):
                    t, attr = grid[r, c]
                    if t:
                        tt = bank.tiles[t][:, ::-1] if attr >> 4 else bank.tiles[t]
                        canvas[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = np.vstack([np.zeros((1, 3)), palettes[attr & 15]])[tt]
            previews.append((img.name, canvas))
        words = []
        for pal in palettes:
            words += [0x8000] + [g.packed15(col) for col in pal] + [0] * (15 - len(pal))
        while len(words) < img.palette_count * 16:
            words += [0x8000] + [0] * 15
        sets.append((img.name, first_frame, len(frames), len(all_words) // 16, img.palette_count))
        all_words += words
    return bank, out_frames, cells_out, attrs_out, sets, all_words, previews


def main():
    images = []
    images.append(Image16("title_bg", [fullscreen(art("screens/title_bg2.png", "screens/title_bg.png"))], 16))
    logo = crop(*rgba(os.path.join(ART, "screens", "logo.png"), black=True))
    images.append(Image16("logo", [fit(*logo, width=288)], 12, "center"))
    for c in CHARACTERS:
        rgb, alpha = rgba(art(f"screens/portrait_{c}.png", "screens/portrait_vorax.png"), KEYS[c] if os.path.exists(os.path.join(ART, f"screens/portrait_{c}.png")) else KEYS["vorax"])
        big = g.downscale(rgb, alpha, 224 / rgb.shape[0])
        bust = (big[0][:128], big[1][:128])
        images.append(Image16(f"portrait_{c}", [big, bust], 12))
    for c in CHARACTERS:
        path = art(f"screens/portrait_{c}.png", "screens/portrait_vorax.png")
        rgb, alpha = rgba(path, KEYS[c] if os.path.exists(os.path.join(ART, f"screens/portrait_{c}.png")) else KEYS["vorax"])
        # square around the head: from the top of the figure, centered on the mass of that band
        side = int(rgb.shape[1] * 0.5)
        rows = np.nonzero((alpha > 0.5).any(axis=1))[0]
        y0 = max(0, rows[0] - side // 12)
        cx = int(np.nonzero(alpha[y0:y0 + side] > 0.5)[1].mean())
        x0 = min(max(0, cx - side // 2), rgb.shape[1] - side)
        face = (rgb[y0:y0 + side, x0:x0 + side], alpha[y0:y0 + side, x0:x0 + side])
        icons = []
        for size in (48, 32):
            rgb_s, a_s = g.downscale(*face, size / side)
            h, w = a_s.shape
            back = np.linspace([70, 30, 90], [10, 8, 30], h)[:, None, :].repeat(w, 1)   # violet backing
            icons.append((rgb_s * a_s[..., None] + back * (1 - a_s[..., None]), np.ones((h, w))))
        images.append(Image16(f"icon_{c}", icons, 4))
    names = [ui.plate(c.upper(), 12) for c in CHARACTERS] + [ui.plate(c.upper(), 24) for c in CHARACTERS]
    names.append(ui.plate("VS", 40, top=(255, 255, 255), mid=(255, 80, 60), bottom=(120, 0, 40), italic=0.1))
    images.append(Image16("names", names, 3))
    images.append(Image16("hud", [ui.lifebar_frame(), ui.timer_shield(), ui.power_frame(), ui.select_cursor(), ui.text_band(), ui.shadow()], 4))
    parts = []
    for sheet, trio in enumerate(WORDS):
        path = os.path.join(ART, "screens", f"words{sheet}.png")
        if not os.path.exists(path):
            path = os.path.join(ART, "screens", "words0.png")
        rgb, alpha = rgba(path, black=True)
        h = rgb.shape[0]
        for k in range(3):
            band = crop(rgb[k * h // 3:(k + 1) * h // 3], alpha[k * h // 3:(k + 1) * h // 3])
            parts.append(fit(*band, width=256 if len(trio[k]) > 10 else 232, height=44))
    images.append(Image16("words", parts, 8, "center"))
    for s in STORY:
        images.append(Image16(f"story_{s}", [fullscreen(art(f"story/{s}.png", "screens/title_bg.png"), 168)], 16))
    for c in PLAYABLE:
        images.append(Image16(f"end_{c}", [fullscreen(art(f"story/end_{c}.png", "screens/title_bg.png"), 168)], 16))

    bank, frames, cells, attrs, sets, palette_words, previews = build(images)
    out_dir = os.path.join(ROOT, "build", "res"); os.makedirs(out_dir, exist_ok=True)
    np.save(os.path.join(out_dir, "screens.tiles.npy"), np.array(bank.tiles, np.uint8))
    word_ids = [w for trio in WORDS for w in trio]
    lines = ["/* generated by tools/make_screens.py */", "#ifndef SCREENS_DATA_H", "#define SCREENS_DATA_H",
             '#include "tile_bases.h"',
             "static const u16 screen_cells[] = {" + ",".join(map(str, cells)) + "};",
             "static const u8 screen_attrs[] = {" + ",".join(map(str, attrs)) + "};",
             "static const Frame screen_frames[] = {\n" + "\n".join(frames) + "\n};",
             "static const u16 screen_palettes[] = {" + ",".join(f"0x{w:04x}" for w in palette_words) + "};",
             "static const ImageSet screen_images = {screen_frames, screen_cells, screen_attrs, TILE_BASE_SCREENS};",
             "/* image: first frame, frames, first palette in screen_palettes (x16), palette count */",
             "typedef struct { u16 frame; u8 count; u16 palette; u8 palettes; } ScreenImage;"]
    lines.append("static const ScreenImage screen_image[] = {" + ", ".join(f"{{{f}, {n}, {p}, {k}}}" for _, f, n, p, k in sets) + "};")
    lines.append("enum { " + ", ".join(f"IMG_{name.upper()}" for name, *_ in sets) + ", IMG_COUNT };")
    lines.append("#define IMG_PORTRAIT(c) (IMG_PORTRAIT_VORAX + (c))")
    lines.append("#define IMG_ICON(c) (IMG_ICON_VORAX + (c))")
    lines.append("#define IMG_END(c) (IMG_END_VORAX + (c))")
    lines.append("#define IMG_STORY(n) (IMG_STORY_INTRO1 + (n))")
    lines.append(f"enum {{ NAME_SMALL = 0, NAME_BIG = {len(CHARACTERS)}, NAME_VS = {2 * len(CHARACTERS)} }};")
    lines.append("enum { HUD_LIFEBAR, HUD_TIMER, HUD_POWER, HUD_CURSOR, HUD_TEXT_BAND, HUD_SHADOW };")
    lines.append("enum { " + ", ".join("WORD_" + w.replace(" ", "_").replace("!", "").replace(".", "").replace("?", "") for w in word_ids) + " };")
    lines.append("#endif")
    open(os.path.join(out_dir, "screens_data.h"), "w").write("\n".join(lines) + "\n")

    # preview sheet
    width = 1000
    y, x, row_h = 0, 0, 0
    placed = []
    for name, canvas in previews:
        h, w = canvas.shape[:2]
        if x + w > width:
            x, y, row_h = 0, y + row_h + 4, 0
        placed.append((x, y, canvas)); x += w + 4; row_h = max(row_h, h)
    sheet = Image.new("RGB", (width, y + row_h), (40, 40, 60))
    for x, y, canvas in placed:
        sheet.paste(Image.fromarray(canvas.astype(np.uint8)), (x, y))
    sheet.save(os.path.join(ROOT, "preview", "screens.png"))
    print(f"screens: {len(frames)} frames, {len(bank.tiles)} tiles, {len(palette_words) // 16} palettes")


if __name__ == "__main__":
    main()
