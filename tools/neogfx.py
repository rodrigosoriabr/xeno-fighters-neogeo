"""Neo Geo graphics helpers shared by the asset scripts.

Pipeline for GPT-drawn art: key out the flat background, downscale with premultiplied alpha (so no
background color bleeds into the edges), cut into 16x16 tiles, build a few 15-color palettes shared by
a whole character or stage, map every tile to its best palette, dedupe tiles (also horizontally
flipped) and encode them in C-ROM format."""
import numpy as np
from PIL import Image


# --------------------------------------------------------------------------------------------- input

def key_alpha(rgb, key, tolerance=110):
    """Alpha mask from distance to the flat background color (soft edge over 40 levels)."""
    d = np.sqrt(((rgb.astype(np.float64) - np.array(key)) ** 2).sum(axis=2))
    return np.clip((d - tolerance) / 40.0, 0.0, 1.0)


def despill(rgb, alpha, key, band=10):
    """Removes the key color that GPT antialiasing mixed into the figure's outline (pink/green fringe
    pixels in game). Only a band of `band` source pixels inside the edge is touched, so a magenta blade
    on a green-keyed fighter keeps its color. Magenta key: min(R,B) above G is spill; green: G above
    max(R,B)."""
    from scipy.ndimage import distance_transform_edt
    near_edge = distance_transform_edt(alpha >= 0.999) <= band
    out = rgb.astype(np.float64).copy()
    r, gr, b = out[:, :, 0], out[:, :, 1], out[:, :, 2]
    if key[1] > key[0]:
        spill = np.clip(gr - np.maximum(r, b), 0, None)
        gr -= np.where(near_edge, spill, 0)
    else:
        spill = np.clip(np.minimum(r, b) - gr, 0, None)
        r -= np.where(near_edge, spill, 0)
        b -= np.where(near_edge, spill, 0)
    return out


def downscale(rgb, alpha, scale):
    """Area-average with premultiplied alpha; returns (rgb float HxWx3, alpha HxW)."""
    h, w = alpha.shape
    nw, nh = max(1, round(w * scale)), max(1, round(h * scale))
    premul = rgb.astype(np.float64) * alpha[:, :, None]
    img = Image.fromarray(np.dstack([premul, alpha * 255]).clip(0, 255).astype(np.uint8), "RGBA")
    # BOX filter = area average; premultiplied so transparent pixels contribute nothing
    small = np.asarray(img.resize((nw, nh), Image.BOX)).astype(np.float64)
    a = small[:, :, 3] / 255.0
    color = np.where(a[:, :, None] > 0, small[:, :, :3] / np.maximum(a[:, :, None], 1e-6), 0)
    return color.clip(0, 255), a


def components(alpha, min_pixels=60, cols=4, rows=2):
    """Bounding boxes of the figures on a GPT sheet laid out as a cols x rows grid, in reading order.

    Pieces are found with a light dilation, then every piece joins the grid cell holding its center, so
    a detached orb or blade trail stays with its figure and two figures that touch are still split
    (a plain large dilation merged neighbours: Zyra's knockdown, Xal's super)."""
    from scipy import ndimage
    mask = alpha > 0.5
    labels, n = ndimage.label(ndimage.binary_dilation(mask, iterations=2))
    h, w = alpha.shape
    cells = {}
    for i, sl in enumerate(ndimage.find_objects(labels)):
        piece = mask[sl] & (labels[sl] == i + 1)
        count = piece.sum()
        if count < min_pixels:
            continue
        ys, xs = np.nonzero(piece)
        cy, cx = sl[0].start + ys.mean(), sl[1].start + xs.mean()
        cell = (min(rows - 1, int(cy * rows / h)), min(cols - 1, int(cx * cols / w)))
        box = (sl[1].start + xs.min(), sl[0].start + ys.min(), sl[1].start + xs.max() + 1, sl[0].start + ys.max() + 1)
        cells.setdefault(cell, []).append((box, count))
    boxes = []
    for cell in sorted(cells):
        pieces = cells[cell]
        if max(c for _, c in pieces) < 1500:     # only specks in this cell
            continue
        boxes.append((min(b[0] for b, _ in pieces), min(b[1] for b, _ in pieces),
                      max(b[2] for b, _ in pieces), max(b[3] for b, _ in pieces)))
    return boxes


# ------------------------------------------------------------------------------------------ palettes

def to_neo(rgb):
    """RGB888 -> nearest color the Neo Geo can show (5 bits per channel + shared dark bit ≈ 6 bits)."""
    return (np.round(np.asarray(rgb, dtype=np.float64) / 255 * 31) * 255 / 31)


def packed15(rgb):
    """RGB888 -> Neo Geo palette word (same rule as ngdevkit paltool.py)."""
    r, g, b = [int(c) >> 2 for c in rgb]
    dark = 1 if ((r & 1) + (g & 1) + (b & 1)) == 0 else 0
    r, g, b = r >> 1, g >> 1, b >> 1
    lsb = ((r & 1) << 2) | ((g & 1) << 1) | (b & 1)
    r, g, b = r >> 1, g >> 1, b >> 1
    return dark << 15 | lsb << 12 | r << 8 | g << 4 | b


def kmeans(points, weights, k, iterations=12, seed=1):
    rng = np.random.default_rng(seed)
    if len(points) <= k:
        return points.copy()
    # k-means++ init
    centers = [points[rng.choice(len(points), p=weights / weights.sum())]]
    for _ in range(1, k):
        d = np.min(((points[:, None, :] - np.array(centers)[None]) ** 2).sum(2), axis=1) * weights
        centers.append(points[rng.choice(len(points), p=d / d.sum())] if d.sum() > 0 else points[rng.integers(len(points))])
    centers = np.array(centers)
    for _ in range(iterations):
        assign = ((points[:, None, :] - centers[None]) ** 2).sum(2).argmin(1)
        for c in range(k):
            m = assign == c
            if m.any():
                centers[c] = (points[m] * weights[m, None]).sum(0) / weights[m].sum()
    return centers


def tile_error(pixels, palette):
    """Sum of squared error of opaque tile pixels mapped to their nearest palette color."""
    if len(pixels) == 0:
        return 0.0
    return ((pixels[:, None, :] - palette[None]) ** 2).sum(2).min(1).sum()


def build_palettes(tiles, count, colors=15, rounds=4):
    """tiles: list of (N,3) opaque pixel arrays. Returns (palettes list of (colors,3), assignment)."""
    means = np.array([t.mean(0) if len(t) else np.zeros(3) for t in tiles])
    sizes = np.array([len(t) for t in tiles], dtype=np.float64) + 1e-3
    groups = kmeans(means, sizes, count).astype(np.float64)
    assign = ((means[:, None, :] - groups[None]) ** 2).sum(2).argmin(1)
    palettes = []
    for _ in range(rounds):
        palettes = []
        for p in range(count):
            member = [tiles[i] for i in range(len(tiles)) if assign[i] == p and len(tiles[i])]
            if not member:
                member = [t for t in tiles if len(t)][:1]
            px = np.concatenate(member)
            uniq, inv, cnt = np.unique(np.round(px / 4) * 4, axis=0, return_inverse=True, return_counts=True)
            palettes.append(to_neo(kmeans(uniq, cnt.astype(np.float64), colors)))
        assign = np.array([np.argmin([tile_error(t, pal) for pal in palettes]) for t in tiles])
    return palettes, assign


# ------------------------------------------------------------------------------------------- tiles

class TileBank:
    """Unique 16x16 index tiles (0 = transparent), deduplicated with horizontal flip."""

    def __init__(self):
        self.tiles, self.lookup = [], {}

    def add(self, grid):
        key = grid.tobytes()
        if key in self.lookup:
            return self.lookup[key], 0
        flipped = grid[:, ::-1].tobytes()
        if flipped in self.lookup:
            return self.lookup[flipped], 1
        self.lookup[key] = len(self.tiles)
        self.tiles.append(grid.copy())
        return len(self.tiles) - 1, 0

    def crom(self, first_tile=0):
        """C1 and C2 ROM bytes (same encoding as ngdevkit tiletool.py encode_crom_tile)."""
        c1, c2 = bytearray(64 * first_tile), bytearray(64 * first_tile)
        for grid in self.tiles:
            flat = grid.reshape(-1)
            for start in (8, 136, 0, 128):
                off = start
                for _ in range(8):
                    planes = [0, 0, 0, 0]
                    for x in range(8):
                        col = int(flat[off])
                        for p in range(4):
                            planes[p] |= ((col >> p) & 1) << x
                        off += 1
                    c1 += bytes(planes[:2])
                    c2 += bytes(planes[2:])
                    off += 8
        return bytes(c1), bytes(c2)


def map_to_palette(rgb, alpha, palette):
    """16x16 tile -> index grid with 1..15 for opaque pixels, 0 for transparent."""
    grid = np.zeros(alpha.shape, dtype=np.uint8)
    opaque = alpha > 0.5
    if opaque.any():
        px = rgb[opaque]
        grid[opaque] = ((px[:, None, :] - palette[None]) ** 2).sum(2).argmin(1) + 1
    return grid
