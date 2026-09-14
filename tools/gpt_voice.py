#!/usr/bin/env python3
"""Announcer and fighter voice lines with OpenAI text-to-speech (key from OPENAI_API_KEY).

  gpt_voice.py out_dir voice "instructions" name=text [name=text ...]

Writes <out_dir>/<name>.wav (24 kHz mono PCM) for each line."""
import json, os, ssl, sys, urllib.request

import certifi  # python.org Python ships without CA certificates

CONTEXT = ssl.create_default_context(cafile=certifi.where())
out_dir, voice, instructions, *lines = sys.argv[1:]
for line in lines:
    name, text = line.split("=", 1)
    body = {"model": "gpt-4o-mini-tts", "voice": voice, "input": text, "instructions": instructions, "response_format": "wav"}
    req = urllib.request.Request("https://api.openai.com/v1/audio/speech", data=json.dumps(body).encode(),
                                 headers={"Authorization": f"Bearer {os.environ['OPENAI_API_KEY']}", "Content-Type": "application/json"})
    try:
        data = urllib.request.urlopen(req, timeout=120, context=CONTEXT).read()
    except urllib.error.HTTPError as e:
        sys.exit(f"{name}: HTTP {e.code}: {e.read().decode()[:300]}")
    open(os.path.join(out_dir, name + ".wav"), "wb").write(data)
    print(name, len(data))
