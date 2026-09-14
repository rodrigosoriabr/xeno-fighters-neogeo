#!/usr/bin/env python3
"""Generate or edit an image with the OpenAI Images API (key from OPENAI_API_KEY).

  gpt_image.py out.png "prompt" [--size 1536x1024] [--quality high] [--model gpt-image-2] [--ref a.png ...]

With --ref the prompt is applied to the reference images (edits endpoint), which keeps a character
consistent between sheets. The prompt is saved next to the image as out.txt."""
import argparse, base64, json, os, ssl, sys, uuid, urllib.request

import certifi  # python.org Python ships without CA certificates

CONTEXT = ssl.create_default_context(cafile=certifi.where())

p = argparse.ArgumentParser()
p.add_argument("out"); p.add_argument("prompt")
p.add_argument("--size", default="1536x1024"); p.add_argument("--quality", default="high")
p.add_argument("--model", default="gpt-image-2"); p.add_argument("--ref", nargs="*", default=[])
p.add_argument("--background", default=None)
a = p.parse_args()
key = os.environ["OPENAI_API_KEY"]

if a.ref:
    boundary = uuid.uuid4().hex
    parts = []
    def field(name, value):
        parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="{name}"\r\n\r\n{value}\r\n'.encode())
    for k, v in (("model", a.model), ("prompt", a.prompt), ("size", a.size), ("quality", a.quality)):
        field(k, v)
    if a.background: field("background", a.background)
    for path in a.ref:
        parts.append(f'--{boundary}\r\nContent-Disposition: form-data; name="image[]"; filename="{os.path.basename(path)}"\r\nContent-Type: image/png\r\n\r\n'.encode()
                     + open(path, "rb").read() + b"\r\n")
    parts.append(f"--{boundary}--\r\n".encode())
    req = urllib.request.Request("https://api.openai.com/v1/images/edits", data=b"".join(parts),
                                 headers={"Authorization": f"Bearer {key}", "Content-Type": f"multipart/form-data; boundary={boundary}"})
else:
    body = {"model": a.model, "prompt": a.prompt, "size": a.size, "quality": a.quality}
    if a.background: body["background"] = a.background
    req = urllib.request.Request("https://api.openai.com/v1/images/generations", data=json.dumps(body).encode(),
                                 headers={"Authorization": f"Bearer {key}", "Content-Type": "application/json"})
try:
    res = json.load(urllib.request.urlopen(req, timeout=600, context=CONTEXT))
except urllib.error.HTTPError as e:
    sys.exit(f"HTTP {e.code}: {e.read().decode()[:500]}")
open(a.out, "wb").write(base64.b64decode(res["data"][0]["b64_json"]))
open(os.path.splitext(a.out)[0] + ".txt", "w").write(a.prompt + "\n")
print(a.out, res.get("usage", {}))
