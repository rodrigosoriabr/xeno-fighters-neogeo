#!/usr/bin/env python3
"""GPT animation sheets -> Neo Geo fighter data.

  make_fighter.py vorax

Reads data/<name>.json (which figure of which sheet is each animation frame) and art/gpt/<name>/*.png,
and writes:
  build/res/<name>.tiles.npy   unique 16x16 index tiles (local numbering; the ROM builder adds a base)
  build/res/<name>.h           frames (tile columns + hurt/hit boxes), animations, both palettes
  preview/<name>_lineup.png    every frame as the Neo Geo will show it, on its ground line and anchor

Scale: the reference figure is scaled to `stance_height`; other sheets are matched to it by the
median square root of figure area (area barely changes with pose, height does), times an optional
per-sheet factor in "sheets". Anchor: x centroid of the hip/torso band, so punches and kicks extend
away from a steady body. Attack boxes: pixels in front of the idle stance's front edge."""
import colorsys, json, os, sys
import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import neogfx as g

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
KEYS = {"magenta": (255, 0, 255), "green": (0, 255, 0)}
PALETTES = 4
# must match the AN_* enum in src/fighter.h
ANIM_ORDER = ["idle", "walk", "crouch_down", "crouch", "jump_takeoff", "jump_up", "jump_apex", "jump_fall",
              "dash", "guard", "lp", "hp", "lk", "hk", "clp", "clk", "chp", "sweep", "jp", "jk",
              "hit_high", "hit_heavy", "hit_crouch", "guard_crouch", "knock", "down", "getup", "throw",
              "special1", "special2", "super", "win"]


def load_sheet(name, sheet, key):
    rgb = np.asarray(Image.open(os.path.join(ROOT, "art", "gpt", name, sheet + ".png")).convert("RGB"))
    alpha = g.key_alpha(rgb, KEYS[key])
    rgb = g.despill(rgb, alpha, KEYS[key])
    return rgb, alpha, g.components(alpha)


def figure(sheets, sheet, index):
    rgb, alpha, boxes = sheets[sheet]
    x0, y0, x1, y1 = boxes[index]
    return rgb[y0:y1, x0:x1], alpha[y0:y1, x0:x1]


def anchor_x(alpha):
    h = alpha.shape[0]
    band = alpha[int(h * 0.35):int(h * 0.7)] > 0.5
    if h < alpha.shape[1] * 0.6 or not band.any():   # lying/horizontal pose: bbox center
        return alpha.shape[1] / 2
    return np.nonzero(band)[1].mean()


def colorway(palettes, hue_shift):
    """Second player colors: rotate hue, keep lightness (mirror matches stay readable)."""
    out = []
    for pal in palettes:
        new = []
        for r, gg, b in pal:
            h, l, s = colorsys.rgb_to_hls(r / 255, gg / 255, b / 255)
            r2, g2, b2 = colorsys.hls_to_rgb((h + hue_shift) % 1.0, l, s)
            new.append((r2 * 255, g2 * 255, b2 * 255))
        out.append(g.to_neo(new))
    return out


def main(name):
    spec = json.load(open(os.path.join(ROOT, "data", name + ".json")))
    missing = [a for a in ANIM_ORDER if a not in spec["anims"]]
    assert not missing and len(spec["anims"]) == len(ANIM_ORDER), f"animations missing or extra: {missing}"
    spec["anims"] = {a: spec["anims"][a] for a in ANIM_ORDER}
    used = sorted({step[0] for steps in spec["anims"].values() for step in steps})
    sheets = {s: load_sheet(name, s, spec["key"]) for s in used}

    # --- scale per sheet
    def sqrt_area(sheet):
        _, alpha, boxes = sheets[sheet]
        return np.median([np.sqrt((alpha[y0:y1, x0:x1] > 0.5).sum()) for x0, y0, x1, y1 in boxes])
    ref_sheet, ref_index = spec["reference"]
    _, ref_alpha = figure(sheets, ref_sheet, ref_index)
    base = spec["stance_height"] / ref_alpha.shape[0]
    scale = {s: base * sqrt_area(ref_sheet) / sqrt_area(s) * spec["sheets"].get(s, 1.0) for s in used}
    print("scale per sheet:", {s: round(v / base, 3) for s, v in scale.items()})

    # --- frames (unique figures), downscaled and cut into a bottom-aligned 16 px grid
    keys, frames = [], []
    for steps in spec["anims"].values():
        for sheet, index, *_ in steps:
            if (sheet, index) not in keys:
                keys.append((sheet, index))
    for sheet, index in keys:
        rgb, alpha = g.downscale(*figure(sheets, sheet, index), scale[sheet])
        h, w = alpha.shape
        rows, cols = -(-h // 16), -(-w // 16)
        R = np.zeros((rows * 16, cols * 16, 3)); A = np.zeros((rows * 16, cols * 16))
        R[rows * 16 - h:, :w] = rgb; A[rows * 16 - h:, :w] = alpha
        frames.append({"rgb": R, "alpha": A, "rows": rows, "cols": cols, "ax": int(round(anchor_x(alpha)))})

    # --- palettes over every opaque tile of the character
    cells = []
    for fi, f in enumerate(frames):
        for r in range(f["rows"]):
            for c in range(f["cols"]):
                a = f["alpha"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16]
                if (a > 0.5).any():
                    cells.append((fi, r, c, f["rgb"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16][a > 0.5]))
    palettes, assign = g.build_palettes([c[3] for c in cells], PALETTES)

    bank = g.TileBank()
    bank.add(np.zeros((16, 16), np.uint8))          # local tile 0 = empty
    grids = [np.zeros((f["rows"], f["cols"], 2), np.int32) for f in frames]
    for (fi, r, c, _), p in zip(cells, assign):
        f = frames[fi]
        grid = g.map_to_palette(f["rgb"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16],
                                f["alpha"][r * 16:(r + 1) * 16, c * 16:(c + 1) * 16], palettes[p])
        tile, flip = bank.add(grid)
        grids[fi][r, c] = (tile, p | flip << 4)

    # --- boxes: hurt = body bbox narrowed; hit = pixels ahead of the idle stance front edge
    idle = frames[keys.index(tuple(spec["anims"]["idle"][0][:2]))]
    idle_front = np.nonzero(idle["alpha"].max(0) > 0.5)[0].max() - idle["ax"]
    hit_frames = {(s[0], s[1]) for steps in spec["anims"].values() for s in steps if "hit" in s[3:]}
    for (sheet, index), f in zip(keys, frames):
        opaque = f["alpha"] > 0.5
        ys, xs = np.nonzero(opaque)
        top = f["rows"] * 16
        x1, x2 = xs.min() - f["ax"], xs.max() - f["ax"]
        narrow = (x2 - x1) * 0.2
        f["hurt"] = (int(max(x1 + narrow, -28)), int(ys.min() - top), int(min(x2 - narrow, 28)), 0)
        f["hit"] = (0, 0, 0, 0)
        if (sheet, index) in hit_frames:
            ahead = opaque.copy()
            ahead[:, :max(0, f["ax"] + idle_front - 6)] = False
            if ahead.sum() < 20:                       # rising attack: take the top third instead
                ahead = opaque.copy(); ahead[ys.min() + (ys.max() - ys.min()) // 3:, :] = False
            hy, hx = np.nonzero(ahead)
            f["hit"] = (int(hx.min() - f["ax"]), int(hy.min() - top), int(hx.max() - f["ax"]), int(hy.max() - top))

    # --- output
    out_dir = os.path.join(ROOT, "build", "res"); os.makedirs(out_dir, exist_ok=True)
    np.save(os.path.join(out_dir, name + ".tiles.npy"), np.array(bank.tiles, np.uint8))
    P = name.upper()
    lines = [f"/* generated by tools/make_fighter.py from data/{name}.json */",
             f"#define {P}_TILES {len(bank.tiles)}"]
    cell_words, attr_bytes, frame_rows = [], [], []
    for f, grid in zip(frames, grids):
        frame_rows.append(f"    {{{-f['ax']}, {-f['rows'] * 16}, {f['cols']}, {f['rows']}, {len(cell_words)}, "
                          f"{{{', '.join(map(str, f['hurt']))}}}, {{{', '.join(map(str, f['hit']))}}}}},")
        for c in range(f["cols"]):              # column-major: one Neo Geo sprite per column
            for r in range(f["rows"]):
                cell_words.append(int(grid[r, c, 0])); attr_bytes.append(int(grid[r, c, 1]))
    lines.append(f"static const u16 {name}_cells[] = {{" + ",".join(map(str, cell_words)) + "};")
    lines.append(f"static const u8 {name}_attrs[] = {{" + ",".join(map(str, attr_bytes)) + "};")
    lines.append(f"static const Frame {name}_frames[] = {{\n" + "\n".join(frame_rows) + "\n};")
    steps_out, anim_rows = [], []
    for anim, steps in spec["anims"].items():
        anim_rows.append(f"    {{{len(steps_out)}, {len(steps)}}},  /* {anim} */")
        for sheet, index, duration, *flags in steps:
            steps_out.append(f"{{{keys.index((sheet, index))}, {duration}, {1 if 'hit' in flags else 0}}}")
    lines.append(f"static const Step {name}_steps[] = {{" + ", ".join(steps_out) + "};")
    lines.append(f"static const Anim {name}_anims[] = {{\n" + "\n".join(anim_rows) + "\n};")
    for way, pals in (("p1", palettes), ("p2", colorway(palettes, spec.get("p2_hue", 0.45)))):
        words = []
        for pal in pals:
            words += [0x8000] + [g.packed15(c) for c in pal] + [0] * (15 - len(pal))
        lines.append(f"static const u16 {name}_palettes_{way}[{PALETTES * 16}] = {{" + ",".join(f"0x{w:04x}" for w in words) + "};")
    open(os.path.join(out_dir, name + ".h"), "w").write("\n".join(lines) + "\n")

    # --- lineup preview
    names = [f"{sheet}:{index}" for sheet, index in keys]
    per_row, cw, ch = 12, 112, 150
    img = Image.new("RGB", (per_row * cw, -(-len(frames) // per_row) * ch), (36, 30, 64))
    for i, (f, grid) in enumerate(zip(frames, grids)):
        canvas = np.zeros((f["rows"] * 16, f["cols"] * 16, 3)); mask = np.zeros(canvas.shape[:2], bool)
        for r in range(f["rows"]):
            for c in range(f["cols"]):
                tile, attr = grid[r, c]
                if tile:
                    t = bank.tiles[tile][:, ::-1] if attr >> 4 else bank.tiles[tile]
                    pal = np.vstack([np.zeros((1, 3)), palettes[attr & 15]])
                    canvas[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = pal[t]; mask[r * 16:(r + 1) * 16, c * 16:(c + 1) * 16] = t > 0
        sprite = Image.fromarray(canvas.astype(np.uint8)); m = Image.fromarray((mask * 255).astype(np.uint8))
        ox, oy = (i % per_row) * cw + cw // 2, (i // per_row) * ch + ch - 8
        img.paste(sprite, (ox - f["ax"], oy - f["rows"] * 16), m)
        from PIL import ImageDraw
        d = ImageDraw.Draw(img)
        d.line((ox - 3, oy, ox + 3, oy), fill=(255, 255, 0))
        hx1, hy1, hx2, hy2 = f["hit"]
        if hx2:
            d.rectangle((ox + hx1, oy + hy1, ox + hx2, oy + hy2), outline=(255, 60, 60))
        d.text(((i % per_row) * cw + 2, (i // per_row) * ch + 2), names[i], fill=(200, 200, 200))
    os.makedirs(os.path.join(ROOT, "preview"), exist_ok=True)
    img.resize((img.width * 2 // 2, img.height), Image.NEAREST).save(os.path.join(ROOT, "preview", name + "_lineup.png"))
    print(f"{name}: {len(frames)} frames, {len(bank.tiles)} unique tiles, {len(cell_words)} cells")


if __name__ == "__main__":
    main(sys.argv[1])
