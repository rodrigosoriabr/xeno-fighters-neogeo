#!/usr/bin/env python3
"""Packs the built ROM as "~/Desktop/games/roms/Xeno Fighters.neocart" for Arcade Nostalgia:
xenofighters.zip + neogeo.xml (MAME software list), info.json, cover.png (title art) and moves.json
(the app's move-list page format)."""
import json, os, shutil, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.expanduser("~/Desktop/games/roms/Xeno Fighters.neocart")
sys.path.insert(0, os.path.dirname(__file__))

# the arcade port of the same attack: Arcade Nostalgia writes these keys into cfg/neogeo.cfg (A 1, B 4, C 3, D 6)
BUTTON = {"A": "P1_BUTTON1/standard", "B": "P1_BUTTON4/standard", "C": "P1_BUTTON3/standard", "D": "P1_BUTTON6/standard"}
ARROWS = {"1": "↙", "2": "↓", "3": "↘", "4": "←", "5": "·", "6": "→", "7": "↖", "8": "↑", "9": "↗"}


def parts(spec):
    """'236 AC' -> symbols for the motion, then the buttons ('AC' = A or C)."""
    motion, _, buttons = spec.partition(" ")
    out = [{"symbol": {"_0": " ".join(ARROWS[d] for d in motion)}}] if motion else []
    for i, b in enumerate(buttons):
        if i:
            out.append({"text": {"_0": " or "}})
        out.append({"button": {"label": b, "port": BUTTON[b]}})
    return out


def move(name, spec):
    return {"move": {"marker": "", "indent": 0, "name": [{"text": {"_0": name}}], "input": parts(spec)}}


COMMON = [("Light punch", " A"), ("Light kick", " B"), ("Heavy punch", " C"), ("Heavy kick", " D"),
          ("Sweep", "2 D"), ("Throw (close)", "6 C"), ("Run", "66"), ("Back step", "44"),
          ("Short hop", "8"), ("Block high", "4"), ("Block low", "1")]
FIGHTERS = [
    ("Vorax", [("Acid Spit", "236 AC"), ("Tail Riser", "623 AC"), ("SUPER: Primal Frenzy", "236236 AC")]),
    ("Zyra", [("Psi Orb", "236 AC"), ("Spiral Ascent", "623 BD"), ("SUPER: Mind Storm", "236236 AC")]),
    ("Ossk", [("Blade Cyclone", "214 BD"), ("Wing Rise", "623 AC"), ("SUPER: Hive Reaper", "236236 BD")]),
    ("Grumm", [("Crystal Quake", "236 AC"), ("Magma Charge", "214 AC"), ("SUPER: Titan Crush (close)", "236236 AC")]),
    ("Krell", [("Plasma Shot", "236 AC"), ("Jet Knee", "214 BD"), ("SUPER: Omega Blaster", "236236 AC")]),
    ("Nyxa", [("Moon Shuriken", "236 AC"), ("Crescent Rise", "623 BD"), ("SUPER: Shadow Dance", "236236 BD")]),
    ("Brutok", [("Quake Slam", "236 AC"), ("Rage Lariat", "214 AC"), ("SUPER: Gorilla Fury (close)", "236236 AC")]),
    ("Koral", [("Tide Wave", "236 AC"), ("Whirlpool", "623 AC"), ("SUPER: Leviathan Call", "236236 AC")]),
]


def main():
    rom = os.path.join(ROOT, "build", "rom")
    os.makedirs(DEST, exist_ok=True)
    for f in ("xenofighters.zip", "neogeo.xml"):
        shutil.copy(os.path.join(rom, f), os.path.join(DEST, f))
    json.dump({"title": "Xeno Fighters", "year": "2026", "manufacturer": "Rodrigo Soria"}, open(os.path.join(DEST, "info.json"), "w"))
    bg = Image.open(os.path.join(ROOT, "art", "gpt", "screens", "title_bg2.png")).convert("RGB")
    logo = Image.open(os.path.join(ROOT, "art", "gpt", "screens", "logo.png")).convert("RGB")
    cover = bg.crop((192, 0, 1344, 864)).resize((640, 480), Image.LANCZOS)
    mask = logo.convert("L").point(lambda v: 255 if v > 30 else 0)
    box = mask.getbbox()
    logo, mask = logo.crop(box), mask.crop(box)
    w = 520
    h = int(logo.height * w / logo.width)
    cover.paste(logo.resize((w, h), Image.LANCZOS), ((640 - w) // 2, 40), mask.resize((w, h)))
    cover.save(os.path.join(DEST, "cover.png"))
    sections = [{"id": 0, "title": "All fighters", "lines": [move(n, s) for n, s in COMMON]}]
    for i, (name, moves) in enumerate(FIGHTERS):
        sections.append({"id": i + 1, "title": name, "lines": [move(n, s) for n, s in moves]})
    page = {"title": "Xeno Fighters", "sections": sections, "hasMoves": True,
            "note": "Neo Geo buttons: A light punch, B light kick, C heavy punch, D heavy kick. Character facing right. "
                    "Hits and blocks fill the power gauge; each full bar stores one super (up to 3). Normals cancel into specials. "
                    "Before the ladder: pick EASY/NORMAL/HARD and STANDARD or TURBO speed."}
    json.dump(page, open(os.path.join(DEST, "moves.json"), "w"), ensure_ascii=False, indent=1)
    print("packed", DEST)


if __name__ == "__main__":
    main()
