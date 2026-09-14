#!/usr/bin/env python3
"""GPT effect sheets (4x2 grids on black) -> Neo Geo effect animations.

  make_fx.py

Writes build/res/fx_data.h (frames, cells, palettes, animation table), build/res/fx_anims.h (FXA_* ids),
build/res/fx.tiles.npy and preview/fx.png. Glow on black becomes alpha from brightness, so soft edges
fade out instead of leaving a dark halo."""
import os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import neogfx as g

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PALETTES = 12

# name, sheet, cells (index in the 4x2 grid), target size px (longest side), frame duration, anchor
ANIMS = [
    ("LIGHT", "misc", [0, 1, 2, 3], 40, 2, "center"),
    ("HEAVY", "sparks", [0, 1, 2, 3], 72, 3, "center"),
    ("BLOCK", "sparks", [4, 5, 6, 7], 56, 3, "center"),
    ("DUST", "misc", [4, 5, 6, 7], 48, 4, "bottom"),
    ("SUPER", "super", [0, 1, 2, 3], 96, 5, "center"),
    ("EXPLOSION", "super", [4, 5, 6, 7], 88, 4, "center"),
    ("PROJ_VORAX", "projectiles", [0, 1], 56, 4, "center"),
    ("PROJ_ZYRA", "projectiles", [2, 3], 56, 4, "center"),
    ("PROJ_GRUMM", "projectiles", [4, 5], 64, 4, "bottom"),
    ("PROJ_XAL", "projectiles", [6, 7], 60, 3, "center"),
    ("PROJ_KRELL", "projectiles2", [0, 1], 56, 3, "center"),
    ("PROJ_NYXA", "projectiles2", [2, 3], 44, 2, "center"),
    ("PROJ_BRUTOK", "projectiles2", [4, 5], 64, 4, "bottom"),
    ("PROJ_KORAL", "projectiles2", [6, 7], 64, 4, "bottom"),
    ("FIREBURST", "impact", [0, 1, 2, 3], 120, 4, "center"),
    ("SLASH", "impact", [4, 5, 6, 7], 80, 2, "center"),
    ("P_EMBER", "particles", [0], 10, 1, "center"),
    ("P_RAIN", "particles", [1], 14, 1, "center"),
    ("P_BUBBLE", "particles", [2], 10, 1, "center"),
    ("P_LEAF", "particles", [3], 12, 1, "center"),
    ("P_PETAL", "particles", [4], 10, 1, "center"),
    ("P_SPARK", "particles", [5], 12, 1, "center"),
    ("P_FIREFLY", "particles", [6], 10, 1, "center"),
    ("P_STAR", "particles", [7], 10, 1, "center"),
]
# sheets still being generated fall back to an existing one, so the game always builds
FALLBACK = {"projectiles2": "projectiles", "impact": "super", "particles": "misc"}


def cell_figure(rgb, index):
    """Crop cell `index` of the 4x2 grid to the bounding box of its visible pixels."""
    h, w = rgb.shape[:2]
    cx, cy = index % 4, index // 4
    cell = rgb[cy * h // 2:(cy + 1) * h // 2, cx * w // 4:(cx + 1) * w // 4].astype(np.float64)
    alpha = np.clip((cell.max(axis=2) - 20) / 70.0, 0, 1)
    ys, xs = np.nonzero(alpha > 0.3)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    a = alpha[y0:y1, x0:x1]
    # un-premultiply the glow so colors stay vivid where alpha is partial
    color = np.where(a[..., None] > 0, cell[y0:y1, x0:x1] / np.maximum(a[..., None], 1e-3), 0).clip(0, 255)
    return color, a


def main():
    sheets = {}
    frames, keys = [], []
    for name, sheet, indices, size, duration, anchor in ANIMS:
        if not os.path.exists(os.path.join(ROOT, "art", "gpt", "fx", sheet + ".png")):
            sheet = FALLBACK[sheet]
        if sheet not in sheets:
            sheets[sheet] = np.asarray(Image.open(os.path.join(ROOT, "art", "gpt", "fx", sheet + ".png")).convert("RGB"))
        figs = [cell_figure(sheets[sheet], i) for i in indices]
        longest = max(max(c.shape[:2]) for c, _ in figs)
        scale = size / longest            # same scale for the whole animation
        for color, alpha in figs:
            rgb, a = g.downscale(color, alpha, scale)
            h, w = a.shape
            rows, cols = -(-h // 16), -(-w // 16)
            R = np.zeros((rows * 16, cols * 16, 3)); A = np.zeros((rows * 16, cols * 16))
            oy, ox = (rows * 16 - h) // 2, (cols * 16 - w) // 2
            R[oy:oy + h, ox:ox + w] = rgb; A[oy:oy + h, ox:ox + w] = a
            frames.append({"rgb": R, "alpha": A, "rows": rows, "cols": cols,
                           "x": -(cols * 8), "y": -(rows * 8) if anchor == "center" else -(rows * 16)})
        keys.append((name, len(frames) - len(indices), len(indices), duration))

    cells = []
    for fi, f in enumerate(frames):
        for r in range(f["rows"]):
            for c in range(f["cols"]):
                a = f["alpha"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16]
                if (a > 0.5).any():
                    cells.append((fi, r, c, f["rgb"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16][a > 0.5]))
    palettes, assign = g.build_palettes([c[3] for c in cells], PALETTES)
    bank = g.TileBank()
    bank.add(np.zeros((16, 16), np.uint8))
    grids = [np.zeros((f["rows"], f["cols"], 2), np.int32) for f in frames]
    for (fi, r, c, _), p in zip(cells, assign):
        f = frames[fi]
        grid = g.map_to_palette(f["rgb"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16], f["alpha"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16], palettes[p])
        tile, flip = bank.add(grid)
        grids[fi][r, c] = (tile, p | flip << 4)

    out_dir = os.path.join(ROOT, "build", "res"); os.makedirs(out_dir, exist_ok=True)
    np.save(os.path.join(out_dir, "fx.tiles.npy"), np.array(bank.tiles, np.uint8))
    cells_out, attrs_out, frame_rows = [], [], []
    for f, grid in zip(frames, grids):
        box = (f["x"], f["y"], f["x"] + f["cols"] * 16, f["y"] + f["rows"] * 16)
        frame_rows.append(f"    {{{f['x']}, {f['y']}, {f['cols']}, {f['rows']}, {len(cells_out)}, {{{', '.join(map(str, box))}}}, {{0, 0, 0, 0}}}},")
        for c in range(f["cols"]):
            for r in range(f["rows"]):
                cells_out.append(int(grid[r, c, 0])); attrs_out.append(int(grid[r, c, 1]))
    words = []
    for pal in palettes:
        words += [0x8000] + [g.packed15(col) for col in pal] + [0] * (15 - len(pal))
    open(os.path.join(out_dir, "fx_anims.h"), "w").write(
        "/* generated by tools/make_fx.py */\n#ifndef FX_ANIMS_H\n#define FX_ANIMS_H\nenum {\n" +
        "".join(f"    FXA_{name},\n" for name, *_ in keys) + "    FXA_COUNT\n};\n#endif\n")
    lines = ["/* generated by tools/make_fx.py */",
             "typedef struct { u16 first; u8 count; u8 duration; } FxAnim;",
             f"#define FX_TILES {len(bank.tiles)}",
             f"#define FX_PALETTES {PALETTES}",
             "static const u16 fx_cells[] = {" + ",".join(map(str, cells_out)) + "};",
             "static const u8 fx_attrs[] = {" + ",".join(map(str, attrs_out)) + "};",
             "static const Frame fx_frames[] = {\n" + "\n".join(frame_rows) + "\n};",
             "static const FxAnim fx_anims[] = {" + ", ".join(f"{{{first}, {count}, {dur}}}" for _, first, count, dur in keys) + "};",
             "static const u16 fx_palettes[] = {" + ",".join(f"0x{w:04x}" for w in words) + "};"]
    open(os.path.join(out_dir, "fx_data.h"), "w").write("\n".join(lines) + "\n")

    img = Image.new("RGB", (8 * 104, len(keys) * 104), (40, 40, 60))
    for k, (name, first, count, _) in enumerate(keys):
        for i in range(count):
            f, grid = frames[first + i], grids[first + i]
            canvas = np.zeros((f["rows"] * 16, f["cols"] * 16, 3)); mask = np.zeros(canvas.shape[:2], bool)
            for r in range(f["rows"]):
                for c in range(f["cols"]):
                    tile, attr = grid[r, c]
                    if tile:
                        t = bank.tiles[tile][:, ::-1] if attr >> 4 else bank.tiles[tile]
                        canvas[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = np.vstack([np.zeros((1, 3)), palettes[attr & 15]])[t]
                        mask[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = t > 0
            img.paste(Image.fromarray(canvas.astype(np.uint8)), (i * 104 + 4, k * 104 + 4), Image.fromarray((mask * 255).astype(np.uint8)))
    os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
    img.save(os.path.join(ROOT, "preview", "fx.png"))
    print(f"fx: {len(frames)} frames, {len(bank.tiles)} tiles")


if __name__ == "__main__":
    main()
