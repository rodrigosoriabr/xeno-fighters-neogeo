#!/usr/bin/env python3
"""Pixel-art UI drawn in code (crisper than GPT art at these sizes): lifebar and power gauge frames,
timer shield, select cursor, and name plates rendered from Arial Black with a metallic gradient.

Every function returns (rgb float array, alpha float array) at final pixel size, ready for
make_screens.build(). Geometry matches the fix-layer cells hud.c fills in (see the comments)."""
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

FONT = "/System/Library/Fonts/Supplemental/Arial Black.ttf"

DARK = (14, 12, 28)
STEEL0 = (38, 40, 64)
STEEL1 = (70, 74, 110)
STEEL2 = (120, 126, 170)
GOLD0 = (120, 70, 10)
GOLD1 = (220, 150, 30)
GOLD2 = (255, 230, 120)
VIOLET = (170, 60, 255)
CYAN = (60, 220, 255)


def to_arrays(img):
    a = np.asarray(img).astype(np.float64)
    return a[..., :3], a[..., 3] / 255.0


def bevel_rect(d, box, fill, light, dark, width=1):
    x0, y0, x1, y1 = box
    d.rectangle(box, fill=fill)
    for i in range(width):
        d.line([(x0 + i, y1 - i), (x0 + i, y0 + i), (x1 - i, y0 + i)], fill=light)
        d.line([(x0 + i, y1 - i), (x1 - i, y1 - i), (x1 - i, y0 + i)], fill=dark)


def lifebar_frame():
    """P1 orientation (drawn mirrored for P2). Placed at screen (0, 0). The fix lifebar covers
    x 40..143, y 8..23; the 32x32 icon sits at (4, 4); the name plate at (42, 27)."""
    img = Image.new("RGBA", (160, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # name tab under the bar: slanted dark plate with a gold edge
    d.polygon([(36, 24), (138, 24), (128, 43), (36, 43)], fill=DARK + (255,))
    d.line([(36, 43), (128, 43), (138, 24)], fill=GOLD1 + (255,))
    d.line([(37, 42), (127, 42)], fill=GOLD0 + (255,))
    # bar trough with bevel and gold lining
    d.polygon([(34, 3), (156, 3), (150, 28), (34, 28)], fill=STEEL0 + (255,))
    d.line([(34, 3), (156, 3)], fill=STEEL2 + (255,))
    d.line([(34, 28), (150, 28), (156, 3)], fill=DARK + (255,))
    d.rectangle((38, 6, 146, 25), fill=GOLD1 + (255,))
    d.rectangle((39, 7, 145, 24), fill=DARK + (255,))
    d.line([(38, 6), (146, 6)], fill=GOLD2 + (255,))
    # rivets and violet energy studs on the trough
    for x in (148, 152):
        d.point((x, 8), fill=GOLD2 + (255,))
        d.point((x - 1, 20), fill=VIOLET + (255,))
    # icon socket
    bevel_rect(d, (1, 1, 38, 38), DARK + (255,), GOLD2 + (255,), GOLD0 + (255,), 2)
    d.rectangle((3, 3, 36, 36), outline=GOLD1 + (255,))
    # spike ornament below the socket
    d.polygon([(8, 39), (31, 39), (20, 47)], fill=GOLD1 + (255,))
    d.polygon([(12, 39), (27, 39), (20, 45)], fill=GOLD0 + (255,))
    d.point((20, 42), fill=VIOLET + (255,))
    return to_arrays(img)


def timer_shield():
    """Placed at screen (136, 0); digits (fix cols 18-21, rows 2-4) cover x 144..175, y 0..23 and the
    win marks row 5 (y 24..31)."""
    img = Image.new("RGBA", (48, 48), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    outer = [(0, 0), (47, 0), (47, 30), (24, 46), (0, 30)]
    d.polygon(outer, fill=GOLD0 + (255,))
    d.polygon([(2, 0), (45, 0), (45, 29), (24, 43), (2, 29)], fill=GOLD1 + (255,))
    d.polygon([(4, 0), (43, 0), (43, 28), (24, 40), (4, 28)], fill=DARK + (255,))
    d.line([(4, 28), (24, 40), (43, 28)], fill=VIOLET + (255,))
    d.line([(2, 0), (2, 29)], fill=GOLD2 + (255,))
    # horns
    d.polygon([(0, 6), (-6 + 6, 0), (6, 0)], fill=GOLD2 + (255,))
    return to_arrays(img)


def power_frame():
    """P1 orientation, placed at screen (0, 180). Stock digit (fix cols 1-2, rows 25-27) at x 8..23,
    y 184..207; gauge cells (cols 4-13, row 27) at x 32..111, y 200..207; label row 25 at y 184."""
    img = Image.new("RGBA", (128, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # gauge trough
    d.polygon([(26, 16), (122, 16), (114, 31), (26, 31)], fill=STEEL0 + (255,))
    d.line([(26, 16), (122, 16)], fill=STEEL2 + (255,))
    d.line([(26, 31), (114, 31), (122, 16)], fill=DARK + (255,))
    d.rectangle((30, 18, 113, 29), fill=CYAN + (255,))
    d.rectangle((31, 19, 112, 28), fill=DARK + (255,))
    # label plate (fix row 25 = y 184..191)
    d.polygon([(28, 2), (78, 2), (72, 13), (28, 13)], fill=DARK + (255,))
    d.line([(28, 2), (78, 2), (72, 13)], fill=GOLD1 + (255,))
    # stock orb socket
    d.ellipse((0, 0, 31, 31), fill=GOLD0 + (255,))
    d.ellipse((2, 2, 29, 29), fill=GOLD1 + (255,))
    d.ellipse((4, 4, 27, 27), fill=DARK + (255,))
    d.arc((4, 4, 27, 27), 200, 340, fill=VIOLET + (255,))
    return to_arrays(img)


def select_cursor():
    """56x56 corner brackets around a 48x48 icon (drawn at icon position - 4)."""
    img = Image.new("RGBA", (56, 56), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for (x, y, dx, dy) in ((0, 0, 1, 1), (55, 0, -1, 1), (0, 55, 1, -1), (55, 55, -1, -1)):
        for k, color in ((0, GOLD0), (1, GOLD2), (2, GOLD1)):
            d.line([(x + dx * k, y + dy * k), (x + dx * 16, y + dy * k)], fill=color + (255,))
            d.line([(x + dx * k, y + dy * k), (x + dx * k, y + dy * 16)], fill=color + (255,))
    d.rectangle((3, 3, 52, 52), outline=CYAN + (255,))
    return to_arrays(img)


def shadow():
    """64x12 floor shadow: dark ellipse, dithered toward the edge so it reads as translucent."""
    w, h = 64, 12
    rgb = np.zeros((h, w, 3)); alpha = np.zeros((h, w))
    for y in range(h):
        for x in range(w):
            d = ((x + 0.5 - w / 2) / (w / 2)) ** 2 + ((y + 0.5 - h / 2) / (h / 2)) ** 2
            if d < 1 and (d < 0.45 or (x + y) % 2 == 0):
                rgb[y, x] = (10, 6, 20); alpha[y, x] = 1
    return rgb, alpha


def text_band(width=320, height=64):
    """Dark band behind story and dialogue text (letterbox)."""
    img = Image.new("RGBA", (width, height), DARK + (255,))
    d = ImageDraw.Draw(img)
    d.line([(0, 0), (width, 0)], fill=GOLD1 + (255,))
    d.line([(0, 1), (width, 1)], fill=GOLD0 + (255,))
    return to_arrays(img)


def plate(text, height, top=(255, 255, 230), mid=(255, 200, 40), bottom=(220, 70, 10), italic=0.22):
    """Name lettering: vertical gradient, dark red inner edge, black outline and drop shadow.
    `height` is the cap height in pixels; the image is padded to fit the outline."""
    size = int(round(height / 0.72))
    font = ImageFont.truetype(FONT, size)
    probe = Image.new("L", (size * len(text) + 40, size * 2), 0)
    pd = ImageDraw.Draw(probe)
    pd.text((20, size // 2), text, font=font, fill=255)
    box = probe.getbbox()
    mask = probe.crop(box)
    w, h = mask.size
    # shear for the italic look
    shear = int(round(h * italic))
    mask = mask.transform((w + shear, h), Image.AFFINE, (1, italic, -shear, 0, 1, 0), Image.BICUBIC)
    m = (np.asarray(mask) > 110)
    w, h = m.shape[1], m.shape[0]
    pad = 2
    H, W = h + pad * 2 + 1, w + pad * 2 + 1
    fill = np.zeros((H, W), bool)
    fill[pad:pad + h, pad:pad + w] = m
    def dilate(a):
        out = a.copy()
        out[1:] |= a[:-1]; out[:-1] |= a[1:]; out[:, 1:] |= a[:, :-1]; out[:, :-1] |= a[:, 1:]
        return out
    outline = dilate(fill)
    shadow = np.zeros_like(outline)
    shadow[1:, 1:] = outline[:-1, :-1]
    rgb = np.zeros((H, W, 3))
    alpha = np.zeros((H, W))
    rgb[shadow] = (20, 0, 30); alpha[shadow] = 1
    rgb[outline] = (0, 0, 0); alpha[outline] = 1
    ys = np.arange(H)[:, None].repeat(W, 1)
    t = np.clip((ys - pad) / max(h - 1, 1), 0, 1)
    top, mid, bottom = map(np.array, (top, mid, bottom))
    grad = np.where(t[..., None] < 0.45, top + (mid - top) * (t[..., None] / 0.45),
                    mid + (bottom - mid) * ((t[..., None] - 0.45) / 0.55))
    rgb[fill] = grad[fill]
    # inner dark edge at the bottom of each stroke
    edge = fill & ~np.vstack([fill[1:], np.zeros((1, W), bool)])
    rgb[edge] = (120, 20, 0)
    # highlight line near the top of each stroke
    hi = fill & ~np.vstack([np.zeros((1, W), bool), fill[:-1]])
    rgb[hi] = (255, 255, 255)
    return rgb, alpha
