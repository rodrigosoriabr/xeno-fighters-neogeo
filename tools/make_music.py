#!/usr/bin/env python3
"""Fight music: a small sequencer that plays sample-library instruments (drums, Fender bass, guitar
chugs, brass stabs, orchestra hits) plus synthesized supersaw lead and pads, mixed to mono for the
Neo Geo ADPCM-B channel.

  make_music.py [track ...]

Each track is written to audio/out/b/<name>.wav at 22050 Hz (looped whole by the driver, so the
track must end exactly on the bar where it restarts) and preview/music/<name>.wav at 44.1 kHz.
Notes use MIDI numbers; one-shot chord samples are pitch-shifted by at most +-6 semitones from the
root in their file name so they keep their timbre."""
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import audiolib as a

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
R = a.RATE
S = "SAMPLES/2020-01/RSAC_ONE_SHOTS/"
D10 = "FULL DRUM MACHINE/Roland D-10 L.A/"
NOTE = {n: i for i, n in enumerate("C C# D D# E F F# G G# A A# B".split())}
NOTE.update({"Db": 1, "Eb": 3, "Gb": 6, "Ab": 8, "Bb": 10})


def m(name):
    """'A4' -> 69, 'C#5' -> 73."""
    pc, octave = (name[:2], name[2:]) if len(name) > 2 and name[1] in "#b" else (name[:1], name[1:])
    return NOTE[pc] + (int(octave) + 1) * 12


class Mix:
    def __init__(self, bpm, bars):
        self.step = 60 / bpm / 4                     # 16th note in seconds
        self.length = int(bars * 16 * self.step * R)
        self.buses = {}
        self.rng = np.random.default_rng(3)

    def bus(self, name):
        if name not in self.buses:
            self.buses[name] = np.zeros(self.length + R * 3)
        return self.buses[name]

    def place(self, bus, x, step, gain=1.0, human=0.004):
        pos = int((step * self.step + self.rng.uniform(-human, human)) * R)
        pos = max(0, pos)
        b = self.bus(bus)
        end = min(len(b), pos + len(x))
        b[pos:end] += x[:end - pos] * gain

    def render(self, levels):
        out = np.zeros(self.length + R * 3)
        for name, x in self.buses.items():
            out += x * levels.get(name, 1.0)
        # fold the tail (reverb/decay past the loop end) back onto the start so the loop is seamless
        tail = out[self.length:]
        out = out[:self.length].copy()
        out[:len(tail)] += tail
        return out


# ------------------------------------------------------------------------------------ instruments

def sample_note(path, root_pc, note, length=None, fade=0.02):
    x = a.load(path)
    shift = (note - root_pc) % 12
    if shift > 6:
        shift -= 12
    y = a.pitch(x, shift)
    if length:
        n = int(length * R)
        y = y[:n].copy()
        f = min(len(y), int(fade * R))
        if f:
            y[-f:] *= np.linspace(1, 0, f)
    return y


def supersaw(note, length, bright=3500, vibrato=True, detune=0.12, voices=5):
    n = int((length + 0.15) * R)
    t = np.arange(n) / R
    f0 = 440 * 2 ** ((note - 69) / 12)
    vib = 1 + (0.006 * np.sin(2 * np.pi * 5.5 * t) * np.clip((t - 0.15) / 0.2, 0, 1) if vibrato else 0)
    out = np.zeros(n)
    for k in range(voices):
        cents = (k - (voices - 1) / 2) * detune * 100 / max(1, (voices - 1) / 2) / 8
        f = f0 * 2 ** (cents / 1200) * vib
        phase = np.cumsum(f) / R + k * 0.137
        out += 2 * (phase % 1) - 1
    out /= voices
    sub = np.sign(np.sin(2 * np.pi * np.cumsum(f0 / 2 * vib) / R)) * 0.25
    env = a.envelope(n, 0.006, length * 1.5, curve=1.2)
    env[int(length * R):] *= np.linspace(1, 0, n - int(length * R))
    return a.lowpass((out + sub) * env, bright, order=2)


def pad(notes, length, cutoff=1800):
    n = int((length + 0.3) * R)
    t = np.arange(n) / R
    out = np.zeros(n)
    for note in notes:
        f0 = 440 * 2 ** ((note - 69) / 12)
        for d in (-0.07, 0.0, 0.07):
            phase = np.cumsum(np.full(n, f0 * 2 ** (d / 12))) / R
            out += 2 * (phase % 1) - 1
    env = np.clip(t / 0.25, 0, 1) * np.clip((length + 0.3 - t) / 0.3, 0, 1)
    return a.lowpass(out * env / (3 * len(notes)), cutoff)


def drum(name, pitch_shift=0):
    return a.pitch(a.load(D10 + name), pitch_shift)


# ------------------------------------------------------------------------------------- patterns

def parse_melody(text, start_bar, bars=None):
    """'A4:4 C5:2 D5:2 -:4' -> [(step, note or None, steps)]; durations in 16ths."""
    events, step = [], start_bar * 16
    for token in text.split():
        name, dur = token.split(":")
        dur = int(dur)
        events.append((step, None if name == "-" else m(name), dur))
        step += dur
    return events


CHORDS = {  # root, chord tones for pads
    "Am": ("A", [57, 60, 64]), "F": ("F", [53, 57, 60]), "G": ("G", [55, 59, 62]), "E": ("E", [52, 56, 59]),
    "Em": ("E", [52, 55, 59]), "Dm": ("D", [50, 53, 57]), "Bb": ("A#", [50, 53, 58]), "C": ("C", [48, 52, 55]),
}


def rock_drums(mx, bar, variant=0, crash=False, fill=False):
    s = bar * 16
    kick, snare, hat, ride, crash_s = drum("la-natbd.wav"), drum("la-hrdsd.wav"), drum("la-clhh.wav"), drum("la-mride.wav"), drum("la-crsh1.wav")
    kicks = [0, 6, 8, 11] if variant == 0 else [0, 3, 8, 10, 14]
    for k in kicks:
        mx.place("drums", kick, s + k, 1.0)
    if fill:
        for k, tom, g in ((8, "la-hpttm.wav", 0.8), (10, "la-mdtm1.wav", 0.8), (12, "la-lotm1.wav", 0.9), (13, "la-lotm1.wav", 0.7), (14, "la-hrdsd.wav", 1.0), (15, "la-hrdsd.wav", 0.9)):
            mx.place("drums", drum(tom), s + k, g)
        mx.place("drums", snare, s + 4, 1.0)
    else:
        mx.place("drums", snare, s + 4, 1.0)
        mx.place("drums", snare, s + 12, 1.0)
        if variant == 1:
            mx.place("drums", snare, s + 15, 0.35)
    for k in range(0, 16, 2):
        mx.place("hats", ride if variant == 1 else hat, s + k, 0.55 if k % 4 == 0 else 0.38)
    if crash:
        mx.place("drums", crash_s, s, 0.8)


def chug_bar(mx, bar, root, pattern="x.xx.xx.x.xx.x.x", accent=True):
    s = bar * 16
    note = NOTE[root] + 45
    for k, ch in enumerate(pattern):
        if ch == "x":
            y = sample_note(S + "RSAC_Dirty_Guitars/RSAC_Stab_Chug_A.wav", 57, note, 0.13)
            mx.place("guitar", y, s + k, 0.8 if k % 4 == 0 else 0.6)
    if accent:
        mx.place("guitar", sample_note(S + "RSAC_Dirty_Guitars/RSAC_Stab_Chug80s_A.wav", 57, note), s, 0.9)


def bass_bar(mx, bar, root, pattern="0.0.0.1.0.0.0.1."):
    """digits: 0 root, 1 octave up, 5 fifth, 7 seventh (minor), 3 minor third."""
    s = bar * 16
    base = NOTE[root] + 33
    offsets = {"0": 0, "1": 12, "5": 7, "7": 10, "3": 3, "2": 2}
    for k, ch in enumerate(pattern):
        if ch in offsets:
            y = sample_note(S + "RSAC_Dirty_Guitars/RSAC_Stab_FenderBass_A.wav", 57, base + offsets[ch], 0.19)
            mx.place("bass", y, s + k, 0.9)


def brass_stab(mx, step, root, length=0.25, gain=0.8):
    mx.place("brass", sample_note(S + "RSAC_Mini_Stabs/RSAC_Stab_Brass_F.wav", 53, NOTE[root] + 48, length), step, gain)


def orch_hit(mx, step, root, gain=1.0):
    mx.place("orch", sample_note(D10 + "la-hitm1.wav", 53, NOTE[root] + 48), step, gain)


def lead(mx, events, bus="lead", bright=3800, octave=0):
    for step, note, dur in events:
        if note is not None:
            mx.place(bus, supersaw(note + 12 * octave, dur * mx.step * 0.95, bright), step, 0.55, human=0.002)


# ----------------------------------------------------------------------------------------- tracks

def temple():
    """Stage theme: A minor fusion rock, 150 BPM, 32 bars (A, B, A', B')."""
    mx = Mix(150, 32)
    progression_a = ["Am", "F", "G", "E"] * 2
    progression_b = ["Dm", "Am", "Bb", "E", "Dm", "Am", "F", "E"]
    melody_a = ("A4:4 C5:2 D5:2 E5:6 D5:2 | C5:2 A4:2 C5:4 F5:4 E5:2 D5:2 | D5:4 B4:2 G4:2 D5:4 E5:2 F5:2 | "
                "E5:12 G#4:2 B4:2 | A5:4 G5:2 E5:2 A5:4 B5:2 C6:2 | C6:6 B5:2 A5:2 G5:2 F5:4 | "
                "G5:2 F5:2 E5:2 D5:2 B4:4 D5:4 | E5:16")
    melody_b = ("F5:2 E5:2 D5:4 A5:4 G5:2 F5:2 | E5:6 C5:2 A4:8 | D5:2 F5:2 A#5:4 A5:2 G5:2 F5:4 | "
                "G#5:4 B5:4 E6:8 | F5:2 A5:2 D6:2 C6:4 A5:2 F5:2 D5:2 | E5:4 A5:4 C6:4 B5:4 | "
                "A5:4 F5:4 G5:4 D5:4 | E5:8 -:8")
    for rep in (0, 1):
        base = rep * 16
        mel_a = parse_melody(melody_a.replace("|", ""), base)
        mel_b = parse_melody(melody_b.replace("|", ""), base + 8)
        for i, ch in enumerate(progression_a):
            bar = base + i
            root = CHORDS[ch][0]
            rock_drums(mx, bar, variant=0, crash=i % 4 == 0, fill=i == 7)
            chug_bar(mx, bar, root, "x.xx.xx.x.xx.x.x" if i % 2 == 0 else "x.xx.x.xx.xx.xxx")
            bass_bar(mx, bar, root, "0.0.0.1.0.0.5.1." if i % 2 == 0 else "0.0.1.0.0.3.5.7.")
            if rep == 1:
                mx.place("pad", pad(CHORDS[ch][1], 16 * mx.step), bar * 16, 0.5)
        lead(mx, mel_a, octave=0)
        for i, ch in enumerate(progression_b):
            bar = base + 8 + i
            root = CHORDS[ch][0]
            rock_drums(mx, bar, variant=1, crash=i in (0, 4), fill=i == 7)
            chug_bar(mx, bar, root, "x...x...x.x.x...", accent=i % 2 == 0)
            bass_bar(mx, bar, root, "0..0..0.0.1.0.5.")
            for k in (3, 6, 11, 14) if i % 2 else (6, 14):
                brass_stab(mx, bar * 16 + k, root, 0.18, 0.6)
            mx.place("pad", pad(CHORDS[ch][1], 16 * mx.step, 2400), bar * 16, 0.45)
        lead(mx, mel_b)
        orch_hit(mx, base * 16, "A", 1.0)
        orch_hit(mx, (base + 8) * 16, "D", 0.9)
        orch_hit(mx, (base + 15) * 16 + 8, "E", 0.9)
        orch_hit(mx, (base + 15) * 16 + 12, "E", 1.0)
    levels = {"drums": 0.9, "hats": 0.5, "guitar": 0.55, "bass": 0.8, "lead": 0.55, "pad": 0.4, "brass": 0.55, "orch": 0.6}
    return mx, levels


def master(mx, levels):
    x = mx.render(levels)
    # echo on the lead is added per bus before master so drums stay dry
    x = a.highpass(x, 35)
    x = a.limit(x * 1.2, 0.97)
    return x


TRACKS = {"temple": temple}


# ------------------------------------------------------------------------------------ song engine
# The other themes are generated from a spec (tempo, progression per section, drum style, instruments)
# with melodies built from motifs: a 2-bar rhythm, chord tones on strong beats, scale steps between,
# the answer phrase landing on a long chord tone. Deterministic per seed, so rebuilds are identical.

R8 = "FULL DRUM MACHINE/Roland R-8/"
TR909 = "FULL DRUM MACHINE/Roland TR-909/"
TAIKO = R8 + "R8Taiko.wav"
THUNDER = "SAMPLES/2015-01/TRIP HOP EXPLOSION_______TOP/Instrument Multi-Samples/Thunder Drum/TRIP-thunder_drum-C2.wav"
SCALES = {"minor": [0, 2, 3, 5, 7, 8, 10], "harmonic": [0, 2, 3, 5, 7, 8, 11], "major": [0, 2, 4, 5, 7, 9, 11],
          "dorian": [0, 2, 3, 5, 7, 9, 10], "pent": [0, 3, 5, 7, 10], "phrygian": [0, 1, 3, 5, 7, 8, 11]}
QUALITY = {"": [0, 4, 7], "m": [0, 3, 7], "dim": [0, 3, 6], "sus": [0, 5, 7], "7": [0, 4, 7, 10], "m7": [0, 3, 7, 10], "5": [0, 7, 12]}
RHYTHMS = ["4 2 2 4 4", "2 2 2 2 4 4", "6 2 4 4", "4 4 2 2 4", "3 3 2 4 4", "2 2 4 2 2 4", "4 2 2 8", "2 4 2 4 4",
           "3 3 4 2 2 2", "8 2 2 4"]


def chord(name):
    """'Am' -> (root pitch class, chord pitch classes); qualities in QUALITY."""
    root = name[:2] if len(name) > 1 and name[1] in "#b" else name[:1]
    q = name[len(root):]
    pc = NOTE[root]
    return pc, [(pc + i) % 12 for i in QUALITY[q]]


def make_melody(rng, bars, key_pc, scale, low, high, start_bar, rest_prob=0.08):
    """bars: chord names, one per bar (length a multiple of 4)."""
    pitches = [p for p in range(low, high + 1) if (p - key_pc) % 12 in SCALES[scale]]
    events = []
    prev = pitches[len(pitches) // 2]
    for phrase in range(0, len(bars), 4):
        rhythm = [int(v) for v in RHYTHMS[rng.integers(len(RHYTHMS))].split()]
        answer = [int(v) for v in RHYTHMS[rng.integers(len(RHYTHMS))].split()]
        climb = rng.integers(2)
        for half, rh in ((0, rhythm), (1, rhythm), (2, answer), (3, [4, 4, 8] if phrase % 8 == 4 else [2, 2, 4, 8])):
            # the second bar pair of a phrase repeats the rhythm (motif), the last bar resolves
            step = 0
            for dur in rh:
                bar = phrase + half
                chord_pcs = chord(bars[bar])[1]
                strong = step % 8 == 0 or dur >= 6
                if rng.random() < rest_prob and not strong and half < 3:
                    events.append(((start_bar + bar) * 16 + step, None, dur))
                    step += dur
                    continue
                if strong or half == 3:
                    cands = [p for p in pitches if p % 12 in chord_pcs]
                else:
                    idx = min(range(len(pitches)), key=lambda i: abs(pitches[i] - prev))
                    direction = 1 if (half < 2) == bool(climb) else -1
                    move = direction * int(rng.choice([1, 1, 1, 2, -1]))
                    cands = [pitches[max(0, min(len(pitches) - 1, idx + move))]]
                target = prev + (4 if half == 1 and climb else 0)
                note = min(cands, key=lambda p: abs(p - target) + rng.random() * 2.5)
                events.append(((start_bar + bar) * 16 + step, note, dur))
                prev = note
                step += dur
    return events


def square_lead(note, length, duty=0.25, bright=4200):
    n = int((length + 0.1) * R)
    t = np.arange(n) / R
    f0 = 440 * 2 ** ((note - 69) / 12)
    vib = 1 + 0.005 * np.sin(2 * np.pi * 6 * t) * np.clip((t - 0.12) / 0.2, 0, 1)
    ph = (np.cumsum(f0 * vib) / R) % 1
    x = np.where(ph < duty, 1.0, -1.0) * 0.6 + (2 * ph - 1) * 0.4
    env = a.envelope(n, 0.004, length * 2, curve=1.0)
    env[int(length * R):] *= np.linspace(1, 0, n - int(length * R))
    return a.lowpass(x * env, bright)


def pluck(note, length, bright=0.5):
    """Karplus-Strong: kalimba/koto-like plucked string."""
    f0 = 440 * 2 ** ((note - 69) / 12)
    period = max(2, int(R / f0))
    n = int((length + 0.4) * R)
    rng = np.random.default_rng(note)
    excite = np.zeros(n)
    excite[:period] = rng.uniform(-1, 1, period)
    g = 0.5 * (0.992 + 0.006 * bright)
    den = np.zeros(period + 2)
    den[0], den[period], den[period + 1] = 1.0, -g, -g
    from scipy.signal import lfilter
    out = lfilter([1.0], den, excite)
    return out * a.envelope(n, 0.001, length + 0.3, curve=2.0)


def organ(notes, length):
    n = int((length + 0.1) * R)
    t = np.arange(n) / R
    out = np.zeros(n)
    for note in notes:
        f0 = 440 * 2 ** ((note - 69) / 12)
        for h, g in ((1, 1.0), (2, 0.5), (3, 0.3), (4, 0.2), (0.5, 0.4)):
            out += np.sin(2 * np.pi * f0 * h * t + 0.8 * np.sin(2 * np.pi * 6 * t)) * g
    env = np.clip(t / 0.02, 0, 1) * np.clip((length + 0.1 - t) / 0.1, 0, 1)
    return out * env / (2.4 * len(notes))


def synth_bass(note, length, cutoff=900):
    n = int((length + 0.05) * R)
    f0 = 440 * 2 ** ((note - 69) / 12)
    ph = (np.cumsum(np.full(n, f0)) / R) % 1
    x = (2 * ph - 1) * 0.7 + np.sign(np.sin(2 * np.pi * np.cumsum(np.full(n, f0 / 2)) / R)) * 0.3
    return a.lowpass(x * a.envelope(n, 0.003, length, curve=1.5), cutoff)


def drums_style(mx, bar, style, crash=False, fill=False):
    s = bar * 16
    if style == "rock":
        rock_drums(mx, bar, 0, crash, fill)
        return
    if style == "drive":
        rock_drums(mx, bar, 1, crash, fill)
        return
    if style == "metal":      # double kick
        for k in range(0, 16, 1 if bar % 2 else 2):
            mx.place("drums", drum("la-natbd.wav"), s + k, 0.75)
        for k in (4, 12):
            mx.place("drums", drum("la-hrdsd.wav"), s + k, 1.0)
        for k in range(0, 16, 4):
            mx.place("hats", drum("la-crsh1.wav") if (crash and k == 0) else drum("la-mride.wav"), s + k, 0.5)
        return
    if style == "halftime":
        for k in (0, 10):
            mx.place("drums", drum("la-natbd.wav", -2), s + k, 1.0)
        mx.place("drums", drum("la-hrdsd.wav", -2), s + 8, 1.0)
        for k in range(0, 16, 2):
            mx.place("hats", drum("la-clhh.wav"), s + k, 0.4)
        if crash:
            mx.place("drums", drum("la-crsh1.wav"), s, 0.8)
        if fill:
            for k in (12, 13, 14, 15):
                mx.place("drums", drum("la-lotm1.wav"), s + k, 0.8)
        return
    if style == "electro":    # four on the floor, 909
        for k in (0, 4, 8, 12):
            mx.place("drums", a.load(TR909 + "TR-909Kick 03.wav"), s + k, 1.0)
        for k in (4, 12):
            mx.place("drums", a.load(TR909 + "TR-909Clap.wav"), s + k, 0.7)
        for k in (2, 6, 10, 14):
            mx.place("hats", a.load(TR909 + "TR-909Hat O 01.wav")[:int(0.12 * R)], s + k, 0.45)
        for k in range(0, 16, 1):
            if k % 2:
                mx.place("hats", a.load(TR909 + "TR-909Hat C 01.wav"), s + k, 0.22)
        if crash:
            mx.place("drums", a.load(TR909 + "TR-909Crash.wav"), s, 0.6)
        return
    if style == "tribal":     # taiko + toms + thunder drum
        for k, g in ((0, 1.0), (3, 0.7), (6, 0.8), (8, 1.0), (11, 0.7), (14, 0.9)):
            mx.place("drums", a.load(TAIKO), s + k, g)
        for k in (4, 12):
            mx.place("drums", drum("la-hrdsd.wav", -3), s + k, 0.8)
        for k in range(0, 16, 2):
            mx.place("hats", a.load(R8 + "R8Cabasa.wav"), s + k, 0.35)
        if crash:
            mx.place("drums", a.pitch(a.load(THUNDER), 0), s, 0.9)
        if fill:
            for k in (8, 10, 12, 13, 14, 15):
                mx.place("drums", a.load(R8 + "R8Daiko1.wav"), s + k, 0.9)
        return
    if style == "soft":
        for k in (0, 7, 10):
            mx.place("drums", drum("la-natbd.wav", -1), s + k, 0.7)
        mx.place("drums", a.load(R8 + "R8FingerSnap.wav"), s + 4, 0.6)
        mx.place("drums", a.load(R8 + "R8FingerSnap.wav"), s + 12, 0.6)
        for k in range(0, 16, 2):
            mx.place("hats", a.load(R8 + "R8Cabasa.wav"), s + k, 0.25)
        if crash:
            mx.place("drums", a.load(R8 + "R8Chime.wav"), s, 0.5)


def song(spec):
    """spec: bpm, key, scale, sections [(chords per bar, drums, energy 0-2)], lead ('saw'|'square'|'pluck'|
    'organ'), octave, bass ('fender'|'synth'), rhythm ('chug'|'stab'|'arp'|'none'), pad, orch, seed."""
    rng = np.random.default_rng(spec["seed"])
    bars = [c for sec in spec["sections"] for c in sec[0]]
    mx = Mix(spec["bpm"], len(bars))
    key_pc = NOTE[spec["key"]]
    low, high = spec.get("range", (m("E4"), m("E6")))
    melody = []
    bar0 = 0
    motif_cache = {}
    for chords_, style, energy in spec["sections"]:
        key = tuple(chords_)
        if key in motif_cache and spec.get("repeat_motifs", True):
            # a repeated section replays its melody (songs need a recognisable hook), up an octave at high energy
            melody += [(st - motif_cache[key][0] * 16 + bar0 * 16, None if n is None else n + (12 if energy == 2 and n + 12 <= high + 5 else 0), d)
                       for st, n, d in motif_cache[key][1]]
        elif energy > 0:
            ev = make_melody(rng, chords_, key_pc, spec["scale"], low, high, bar0)
            motif_cache[key] = (bar0, ev)
            melody += ev
        for i, name in enumerate(chords_):
            bar = bar0 + i
            root_pc, pcs = chord(name)
            root_name = [k for k, v in NOTE.items() if v == root_pc][0]
            last = i == len(chords_) - 1
            drums_style(mx, bar, style, crash=i % 4 == 0 and energy > 0, fill=last and energy > 0)
            bass_root = root_pc + 36
            if spec.get("bass", "fender") == "fender":
                bass_bar(mx, bar, root_name, "0.0.0.1.0.0.5.1." if energy == 2 else "0...0.1.0...0.5.")
            else:
                for k in (range(0, 16, 2) if energy else (0, 8)):
                    mx.place("bass", synth_bass(bass_root + (12 if k in (6, 14) and energy == 2 else 0), mx.step * (1.8 if energy else 7.5)), bar * 16 + k, 0.8)
            rhythm = spec.get("rhythm", "chug")
            if rhythm == "chug" and energy > 0:
                chug_bar(mx, bar, root_name, "x.xx.xx.x.xx.x.x" if energy == 2 else "x...x...x.x.x...", accent=i % 2 == 0)
            elif rhythm == "stab" and energy > 0:
                for k in (0, 6, 10) if i % 2 == 0 else (3, 8, 14):
                    brass_stab(mx, bar * 16 + k, root_name, 0.2, 0.55)
            elif rhythm == "arp":
                tones = sorted({root_pc + 60 + ((p - root_pc) % 12) for p in pcs})
                seq = tones + [tones[0] + 12] + tones[::-1][1:]
                for k in range(0, 16, 2 if energy < 2 else 1):
                    mx.place("arp", pluck(seq[(k // (2 if energy < 2 else 1)) % len(seq)], mx.step * 1.5, 0.3), bar * 16 + k, 0.35)
            if spec.get("pad", True):
                voicing = sorted({(root_pc + ((p - root_pc) % 12)) + 48 for p in pcs})
                if spec.get("organ_pad"):
                    mx.place("pad", organ(voicing, 16 * mx.step), bar * 16, 0.5)
                else:
                    mx.place("pad", pad(voicing, 16 * mx.step, 2000 + 800 * energy), bar * 16, 0.5)
        if spec.get("orch") and energy > 0:
            root_name = [k for k, v in NOTE.items() if v == chord(chords_[0])[0]][0]
            orch_hit(mx, bar0 * 16, root_name, 0.9)
        bar0 += len(chords_)
    kind = spec.get("lead", "saw")
    for step, note, dur in melody:
        if note is None:
            continue
        note += 12 * spec.get("octave", 0)
        length = dur * mx.step * 0.92
        if kind == "saw":
            y = supersaw(note, length, spec.get("bright", 3800))
        elif kind == "square":
            y = square_lead(note, length)
        elif kind == "pluck":
            y = pluck(note, max(length, 0.25), 0.8)
        else:
            y = organ([note], length)
        mx.place("lead", y, step, 0.55, human=0.002)
    return mx, spec.get("levels", {"drums": 0.9, "hats": 0.5, "guitar": 0.5, "bass": 0.8, "lead": 0.55, "pad": 0.38,
                                   "brass": 0.5, "orch": 0.6, "arp": 0.5})


def section(chords_, times=1):
    return chords_.split() * times


SONGS = {
    # title: epic and slow, choir-like pads and taiko, brass hook
    "title": dict(bpm=104, key="D", scale="minor", seed=11, lead="saw", rhythm="stab", orch=True, bass="synth",
                  sections=[(section("Dm Bb C Am"), "halftime", 0), (section("Dm Bb C A"), "tribal", 1),
                            (section("Gm Dm Bb A"), "tribal", 2), (section("Dm Bb C A"), "tribal", 2)]),
    "select": dict(bpm=160, key="E", scale="minor", seed=5, lead="square", rhythm="chug", sections=[(section("Em C D B"), "drive", 2), (section("Em C D B"), "drive", 2)]),
    "sanctuary": dict(bpm=128, key="F#", scale="dorian", seed=21, lead="saw", rhythm="arp", bass="synth", bright=3000,
                      sections=[(section("F#m E Bm C#m"), "electro", 1), (section("F#m E D C#"), "electro", 2),
                                (section("Bm C#m D E"), "electro", 1), (section("F#m E D C#"), "electro", 2)]),
    "hive": dict(bpm=138, key="C", scale="harmonic", seed=31, lead="square", rhythm="chug", bass="synth",
                 sections=[(section("Cm Ab Fm G"), "drive", 1), (section("Cm Ab Bb G"), "metal", 2),
                           (section("Fm Cm Ab G"), "drive", 1), (section("Cm Ab Bb G"), "metal", 2)]),
    "mine": dict(bpm=118, key="E", scale="minor", seed=41, lead="saw", rhythm="chug", orch=True,
                 sections=[(section("E5 C5 D5 B5"), "halftime", 1), (section("E5 G5 A5 B5"), "halftime", 2),
                           (section("C5 D5 E5 E5"), "halftime", 1), (section("E5 G5 A5 B5"), "halftime", 2)]),
    "spaceport": dict(bpm=122, key="G", scale="dorian", seed=51, lead="square", rhythm="stab",
                      sections=[(section("Gm7 C7 Gm7 C7"), "electro", 1), (section("Ebm7 F7 Gm7 Gm7"), "electro", 2),
                                (section("Cm7 F7 Bbm7 D7"), "electro", 1), (section("Ebm7 F7 Gm7 Gm7"), "electro", 2)]),
    "rooftops": dict(bpm=140, key="A", scale="pent", seed=61, lead="pluck", octave=0, rhythm="arp", orch=True,
                     sections=[(section("Am G F Em"), "tribal", 1), (section("Am G F G"), "drive", 2),
                               (section("Dm Am Em Am"), "tribal", 1), (section("Am G F G"), "drive", 2)]),
    "colosseum": dict(bpm=126, key="D", scale="minor", seed=71, lead="saw", rhythm="chug", orch=True,
                      sections=[(section("Dm C Bb A"), "tribal", 1), (section("Dm F C A"), "tribal", 2),
                                (section("Gm Dm Bb A"), "tribal", 1), (section("Dm F C A"), "tribal", 2)]),
    "dome": dict(bpm=112, key="Eb", scale="major", seed=81, lead="saw", rhythm="arp", bass="synth", bright=2600,
                 sections=[(section("Eb Cm Ab Bb"), "soft", 1), (section("Eb Gm Ab Bb"), "soft", 2),
                           (section("Cm Ab Eb Bb"), "soft", 1), (section("Eb Gm Ab Bb"), "soft", 2)]),
    "boss": dict(bpm=152, key="C#", scale="phrygian", seed=91, lead="organ", rhythm="chug", organ_pad=True, orch=True,
                 range=(m("C#4"), m("C#6")),
                 sections=[(section("C#m D C#m B"), "metal", 1), (section("C#m A D C"), "metal", 2),
                           (section("F#m C#m D C"), "metal", 1), (section("C#m A D C"), "metal", 2)]),
    "ending": dict(bpm=92, key="C", scale="major", seed=101, lead="saw", rhythm="arp", bright=3000, orch=True,
                   sections=[(section("C G Am F"), "soft", 1), (section("C G F G"), "halftime", 2),
                             (section("Am F C G"), "soft", 1), (section("C G F C"), "halftime", 2)]),
}

for _name, _spec in SONGS.items():
    TRACKS[_name] = (lambda sp: (lambda: song(sp)))(_spec)


def main(names):
    for name in names or TRACKS:
        mx, levels = TRACKS[name]()
        mx.buses["lead"] = a.comb_reverb(mx.bus("lead"), 0.35, 1.1)[:len(mx.bus("lead"))]
        mx.buses["pad"] = a.comb_reverb(mx.bus("pad"), 0.3, 1.4)[:len(mx.bus("pad"))]
        x = master(mx, levels)
        seconds = a.write(os.path.join(ROOT, "audio", "out", "b", name + ".wav"), x, 22050)
        a.write(os.path.join(ROOT, "preview", "music", name + ".wav"), x, 44100)
        rms = np.sqrt(np.mean(x ** 2))
        print(f"music {name}: {seconds:.1f}s, rms {rms:.2f}")


if __name__ == "__main__":
    main(sys.argv[1:])
