#!/usr/bin/env python3
"""Arena treatment for voice lines: trim, deeper pitch, compression, stadium echo, normalize.

  voice_fx.py in_dir out_dir [--pitch 0.92]

Writes <out_dir>/<name>.wav at 18500 Hz (the Neo Geo ADPCM-A playback rate)."""
import argparse, glob, os, warnings
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly, butter, sosfilt

RATE = 18500
warnings.filterwarnings("ignore", message="Reached EOF")  # TTS streams WAV with an unknown length
p = argparse.ArgumentParser(); p.add_argument("inp"); p.add_argument("out"); p.add_argument("--pitch", type=float, default=0.92)
a = p.parse_args()
os.makedirs(a.out, exist_ok=True)
for path in sorted(glob.glob(os.path.join(a.inp, "*.wav"))):
    rate, x = wavfile.read(path)
    x = x.astype(np.float64) / 32768
    loud = np.where(np.abs(x) > 0.02)[0]
    x = x[max(0, loud[0] - 200): loud[-1] + 800]
    # lower pitch (and tempo) by playing the 24 kHz clip back slower
    x = resample_poly(x, int(1000 / a.pitch), 1000)
    x = sosfilt(butter(2, 90, "highpass", fs=rate, output="sos"), x)
    x = np.tanh(x * 3.0) / np.tanh(3.0)            # compression / grit
    y = np.concatenate([x, np.zeros(int(0.5 * rate))])
    for delay, gain in ((0.09, 0.35), (0.19, 0.22), (0.31, 0.12)):  # stadium slapback
        d = int(delay * rate)
        y[d:d + len(x)] += gain * x
    y = y / np.max(np.abs(y)) * 0.97
    wavfile.write(os.path.join(a.out, os.path.basename(path)), rate, (y * 32767).astype(np.int16))
    print(os.path.basename(path), f"{len(y) / rate:.2f}s")
