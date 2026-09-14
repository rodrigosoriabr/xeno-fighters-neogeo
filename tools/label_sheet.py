#!/usr/bin/env python3
"""Contact image of the figures found on GPT sheets, numbered in reading order, for writing frames.json.

  label_sheet.py art/gpt/vorax/s1_idle_walk.png [...] -o preview/labels.png"""
import argparse, sys, os
import numpy as np
from PIL import Image, ImageDraw
sys.path.insert(0, os.path.dirname(__file__))
import neogfx as g

KEYS = {"magenta": (255, 0, 255), "green": (0, 255, 0)}
p = argparse.ArgumentParser(); p.add_argument("sheets", nargs="+"); p.add_argument("-o", required=True); p.add_argument("--key", default="magenta")
a = p.parse_args()
rows = []
for path in a.sheets:
    im = np.asarray(Image.open(path).convert("RGB"))
    alpha = g.key_alpha(im, KEYS[a.key])
    boxes = g.components(alpha)
    small = Image.open(path).convert("RGB").resize((768, 512))
    d = ImageDraw.Draw(small)
    for i, (x0, y0, x1, y1) in enumerate(boxes):
        d.rectangle((x0 / 2, y0 / 2, x1 / 2, y1 / 2), outline=(255, 255, 0))
        d.text((x0 / 2 + 3, y0 / 2 + 2), f"{i} h{y1 - y0}", fill=(255, 255, 255))
    d.text((6, 496), os.path.basename(path), fill=(255, 255, 255))
    rows.append(small)
out = Image.new("RGB", (768 * 2, 512 * ((len(rows) + 1) // 2)))
for i, r in enumerate(rows):
    out.paste(r, ((i % 2) * 768, (i // 2) * 512))
out.save(a.o)
