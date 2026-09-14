#!/usr/bin/env python3
"""Runs a list of [out, prompt, size, (optional) [reference images]] image jobs (3 at a time) with tools/gpt_image.py; skips existing files."""
import json, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def run(job):
    out, prompt, size, *refs = job
    path = os.path.join(ROOT, "art", "gpt", out + ".png")
    if os.path.exists(path):
        return out, "exists"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    for attempt in range(3):
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "gpt_image.py"), path, prompt, "--size", size] + (["--ref"] + [os.path.join(ROOT, r) for r in refs[0]] if refs else []), capture_output=True, text=True)
        if os.path.exists(path):
            return out, "ok"
    return out, (r.stdout + r.stderr).strip()[-200:]

with ThreadPoolExecutor(int(os.environ.get("JOBS", "3"))) as pool:
    for out, status in pool.map(run, json.load(open(sys.argv[1]))):
        print(out, status, flush=True)
