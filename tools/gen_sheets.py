#!/usr/bin/env python3
"""Generate a character's animation sheets from tools/sheets.json with the concept sheet as reference.

  gen_sheets.py vorax [sheet ...]     (needs OPENAI_API_KEY; 3 requests at a time)"""
import json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
spec = json.load(open(os.path.join(ROOT, "tools", "sheets.json")))
name, wanted = sys.argv[1], sys.argv[2:]
char = spec["characters"][name]
out_dir = os.path.join(ROOT, "art", "gpt", name)
os.makedirs(out_dir, exist_ok=True)

def run(sheet):
    frames = spec["sheets"][sheet].format(**char)
    prompt = spec["base"].format(**char) + frames
    out = os.path.join(out_dir, sheet + ".png")
    if os.path.exists(out):
        return sheet, "exists"
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gpt_image.py"), out, prompt, "--size", "1536x1024",
                        "--ref", os.path.join(ROOT, "art", "gpt", f"{name}_sheet.png")], capture_output=True, text=True)
    return sheet, (r.stdout + r.stderr).strip().splitlines()[-1][:80]

with ThreadPoolExecutor(3) as pool:
    for sheet, result in pool.map(run, wanted or list(spec["sheets"])):
        print(sheet, "->", result)
