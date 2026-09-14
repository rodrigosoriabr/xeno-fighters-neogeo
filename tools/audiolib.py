"""Small audio toolkit for the Neo Geo sound assets: loading library samples (via sox for AIFF),
resampling, envelopes, filters, simple reverb, limiting and writing 16-bit mono WAVs."""
import os, subprocess, tempfile, warnings
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfilt, resample_poly

RATE = 44100
LIBRARY = "/Volumes/SSD DATA/MUSIC/LIBRARY AND SAMPLES"
SOX = os.path.expanduser("~/neodev/local/bin/sox")
_cache = {}

warnings.filterwarnings("ignore", message="Reached EOF")       # streamed TTS WAVs
warnings.filterwarnings("ignore", category=wavfile.WavFileWarning)


def load(path, rate=RATE):
    """Any WAV/AIFF -> mono float64 at `rate`."""
    path = path if os.path.isabs(path) else os.path.join(LIBRARY, path)
    key = (path, rate)
    if key in _cache:
        return _cache[key]
    if not path.lower().endswith(".wav"):
        tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False).name
        subprocess.run([SOX, path, "-b", "16", tmp], check=True)
        src, data = wavfile.read(tmp)
        os.unlink(tmp)
    else:
        src, data = wavfile.read(path)
    if data.dtype == np.int16:
        x = data.astype(np.float64) / 32768
    elif data.dtype == np.int32:
        x = data.astype(np.float64) / 2147483648
    elif data.dtype == np.uint8:
        x = (data.astype(np.float64) - 128) / 128
    else:
        x = data.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    if src != rate:
        g = np.gcd(int(src), int(rate))
        x = resample_poly(x, rate // g, src // g)
    _cache[key] = x
    return x


def pitch(x, semitones):
    """Resampling pitch shift (duration changes with pitch, like a sampler)."""
    if semitones == 0:
        return x
    ratio = 2 ** (semitones / 12)
    n = int(len(x) / ratio)
    return np.interp(np.arange(n) * ratio, np.arange(len(x)), x)


def lowpass(x, hz, order=2, rate=RATE):
    return sosfilt(butter(order, hz, "lowpass", fs=rate, output="sos"), x)


def highpass(x, hz, order=2, rate=RATE):
    return sosfilt(butter(order, hz, "highpass", fs=rate, output="sos"), x)


def bandpass(x, lo, hi, order=2, rate=RATE):
    return sosfilt(butter(order, [lo, hi], "bandpass", fs=rate, output="sos"), x)


def envelope(n, attack=0.002, decay=0.2, rate=RATE, curve=4.0):
    t = np.arange(n) / rate
    env = np.minimum(1.0, t / max(attack, 1e-4))
    return env * np.exp(-np.maximum(0, t - attack) * curve / max(decay, 1e-4))


def noise(seconds, seed=1, rate=RATE):
    return np.random.default_rng(seed).uniform(-1, 1, int(seconds * rate))


def reverb(x, amount=0.25, size=1.0, rate=RATE):
    """Schroeder-style: 4 combs + 2 allpasses. Cheap but enough for arena/hall air."""
    out = np.zeros(len(x) + int(rate * 1.2 * size))
    dry = np.concatenate([x, np.zeros(len(out) - len(x))])
    wet = np.zeros_like(out)
    for delay_ms, fb in ((29.7, 0.77), (37.1, 0.75), (41.1, 0.73), (43.7, 0.71)):
        d = int(delay_ms * size * rate / 1000)
        buf = dry.copy()
        for i in range(d, len(buf)):
            buf[i] += fb * buf[i - d]
        wet += buf
    wet /= 4
    for delay_ms, g in ((5.0, 0.7), (1.7, 0.7)):
        d = int(delay_ms * rate / 1000)
        y = np.zeros_like(wet)
        y[:d] = wet[:d]
        y[d:] = -g * wet[d:] + wet[:-d] + g * 0   # light diffusion
        wet = y
    return dry + amount * lowpass(wet, 6000)


def comb_reverb(x, amount=0.25, size=1.0, rate=RATE):
    """Vectorized multi-tap echo reverb (fast version used for long music mixes)."""
    out = np.concatenate([x, np.zeros(int(rate * 1.5 * size))])
    wet = np.zeros_like(out)
    taps = [(0.031, 0.5), (0.047, 0.42), (0.071, 0.36), (0.089, 0.3), (0.113, 0.26), (0.149, 0.2),
            (0.193, 0.15), (0.241, 0.11), (0.307, 0.08), (0.383, 0.05)]
    for delay, gain in taps:
        d = int(delay * size * rate)
        wet[d:d + len(x)] += gain * x
    return out + amount * lowpass(highpass(wet, 200), 5000)


def drive(x, amount=2.0):
    return np.tanh(x * amount) / np.tanh(amount)


def limit(x, ceiling=0.95):
    """Soft limiter: normalise, then tanh above the knee."""
    peak = np.max(np.abs(x)) or 1.0
    x = x / peak
    return np.tanh(x * 1.6) / np.tanh(1.6) * ceiling


def trim(x, threshold=0.01, pad=0.01, rate=RATE):
    loud = np.where(np.abs(x) > threshold)[0]
    if len(loud) == 0:
        return x[:1]
    return x[max(0, loud[0] - int(pad * rate)): loud[-1] + int(pad * rate)]


def write(path, x, rate):
    """Resample from RATE to `rate` and write 16-bit mono."""
    if rate != RATE:
        g = np.gcd(RATE, rate)
        x = resample_poly(x, rate // g, RATE // g)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    wavfile.write(path, rate, (np.clip(x, -1, 1) * 32767).astype(np.int16))
    return len(x) / rate
