#!/usr/bin/env python3
"""Fight sound effects, layered from drum-machine samples in the sample library plus synthesis, and the
voice lines (OpenAI TTS takes in audio/voice_raw) with per-character treatment.

  make_sfx.py

Writes 18500 Hz mono WAVs (the ADPCM-A rate) into audio/out/a/<channel>/<name>.wav:
  1 announcer   2 fighter voices (left side)   3 fighter voices (right side / second fighter)
  4 impacts     5 swings, jumps, landings      6 menu, projectiles, super
Previews at 44.1 kHz land in preview/sfx/."""
import glob, os, shutil, sys
import numpy as np

sys.path.insert(0, os.path.dirname(__file__))
import audiolib as a

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "audio", "out", "a")
PREVIEW = os.path.join(ROOT, "preview", "sfx")
R = a.RATE

KICK = "FULL DRUM MACHINE/Roland D-10 L.A/la-natbd.wav"
SNARE = "FULL DRUM MACHINE/Roland D-10 L.A/la-hrdsd.wav"
BOOM = "FULL DRUM MACHINE/MultiMoog/MaxV - FMBOOm.AIFF"
CRACK = "FULL DRUM MACHINE/MultiMoog/MaxV - FM Crack.AIFF"
EXPLOSION_SNARE = "FULL DRUM MACHINE/MultiMoog/MaxV - Explosion Snare.AIFF"
CHINA = "FULL DRUM MACHINE/Roland D-10 L.A/la-china.wav"
ORCH = "FULL DRUM MACHINE/Roland D-10 L.A/la-hitm1.wav"
CLINK = "FULL DRUM MACHINE/MultiMoog/MaxV - Filtered Clink.AIFF"
ROWDY = "SAMPLES/2013-02/WR - Vintage Deep & Tech House/SINGLE HITS/AMBIENCE & FX/WR_VDTH_ROWDY CROWD.wav"
SMALL_CROWD = "SAMPLES/2013-02/WR - Vintage Deep & Tech House/SINGLE HITS/AMBIENCE & FX/WR_VDTH_SMALL CROWD.wav"
APPLAUSE = "SAMPLES/2013-02/WR - Vintage Deep & Tech House/SINGLE HITS/AMBIENCE & FX/WR_VDTH_APPLAUSE.wav"
CHEERS = "SAMPLES/2013-01/Vocal/Hy2rogen - Glitch & Tech Vocals 1 (WAV)/Oneshots/Cheers.wav"
CROWD_VOX = "SAMPLES/2013-03/SPF Samplers - Deep Tech House (WAV)/HITS/VOX/SPF_DTH1_CROWD_VOX_01.wav"
ALARM = "SAMPLES/2015-01/Vengeance.Sound.Trance.Sensation.Vol.1_______TOP/VTS1 138BPM - Soothsayer Kit - Bminor/VTS1 Soothsayer Kit Sound FX/VTS1 Soothsayer Kit 138 BPM Square Alarm.wav"
SIREN = "SAMPLES/2013-03/SPF Samplers - Rootstep WAV/FX/drops/spf_rs_dub siren_007.wav"


def mix(*parts):
    n = max(len(p) for p in parts)
    out = np.zeros(n)
    for p in parts:
        out[:len(p)] += p
    return out


def at(x, seconds):
    return np.concatenate([np.zeros(int(seconds * R)), x])


def swing(length, lo, hi, seed):
    """Whoosh: noise through a band that sweeps up then down, with a fast swell."""
    n = int(length * R)
    x = a.noise(length, seed)
    t = np.linspace(0, 1, n)
    out = np.zeros(n)
    chunks = 24
    for k in range(chunks):
        s, e = k * n // chunks, (k + 1) * n // chunks
        c = (lo + (hi - lo) * np.sin(np.pi * (k + 0.5) / chunks))
        out[s:e] = a.bandpass(x[max(0, s - 400):e], c * 0.7, c * 1.4)[-(e - s):]
    env = np.sin(np.pi * t) ** 2 * np.exp(-t * 1.5)
    return out * env * 3


def hit(heavy):
    kick = a.load(KICK) * a.envelope(len(a.load(KICK)), decay=0.25 if heavy else 0.12)
    snare = a.pitch(a.load(SNARE), -3 if heavy else 2)
    crack = a.load(CRACK)
    body = mix(a.pitch(kick, -5 if heavy else 0) * (1.3 if heavy else 0.8), snare * 0.7, crack * 0.5)
    slap = a.highpass(a.noise(0.06, 7), 1500) * a.envelope(int(0.06 * R), decay=0.03) * 0.6
    x = mix(body, slap)
    if heavy:
        x = mix(x, a.lowpass(a.load(BOOM), 900) * 0.8)
    x = a.drive(x, 2.5 if heavy else 1.8)
    return a.limit(a.comb_reverb(x, 0.18, 0.6))


def block():
    clink = a.pitch(a.load(CLINK), -2)
    metal = sum(np.sin(2 * np.pi * f * np.arange(int(0.25 * R)) / R) for f in (1480, 2210, 3130))
    metal *= a.envelope(len(metal), decay=0.08)
    thud = a.lowpass(a.load(KICK), 400) * 0.5
    return a.limit(a.comb_reverb(mix(clink, metal * 0.25, thud, a.highpass(a.noise(0.05, 3), 3000) * 0.3), 0.2, 0.5))


def land(heavy):
    x = a.lowpass(a.pitch(a.load(KICK), -7 if heavy else -2), 700)
    dust = a.lowpass(a.noise(0.25, 9), 2500) * a.envelope(int(0.25 * R), decay=0.1) * 0.25
    if heavy:
        x = mix(x * 1.4, a.lowpass(a.load(BOOM), 500))
    return a.limit(mix(x, dust))


def projectile():
    n = int(0.55 * R)
    t = np.arange(n) / R
    f = 900 * np.exp(-t * 3) + 120
    tone = np.sin(2 * np.pi * np.cumsum(f) / R) * a.envelope(n, 0.01, 0.4, curve=3)
    whoosh = swing(0.55, 600, 3000, 11)
    return a.limit(a.comb_reverb(mix(tone * 0.6, whoosh * 0.7), 0.25, 0.8))


def super_flash():
    orch = a.load(ORCH)
    rise = swing(0.4, 1000, 6000, 5)
    return a.limit(a.comb_reverb(mix(orch * 1.2, a.pitch(orch, -12) * 0.6, rise * 0.5, a.load(CHINA) * 0.5), 0.35, 1.2))


def ko_boom():
    return a.limit(a.comb_reverb(mix(a.load(BOOM) * 1.2, a.load(EXPLOSION_SNARE), a.pitch(a.load(ORCH), -5) * 0.7), 0.4, 1.4))


def blip(freq, length, sweep=0.0):
    n = int(length * R)
    t = np.arange(n) / R
    f = freq * (1 + sweep * t / length)
    return np.sign(np.sin(2 * np.pi * np.cumsum(f) / R)) * a.envelope(n, 0.001, length * 0.6) * 0.5


def fade(x, seconds, length=None):
    """Cut to `length` seconds (if given) with a fade-out over the last `seconds`."""
    if length:
        x = x[:int(length * R)]
    n = min(len(x), int(seconds * R))
    x = x.copy()
    x[-n:] *= np.linspace(1, 0, n) ** 2
    return x


def crowd(kind):
    """Arena crowd reactions layered from the library's crowd recordings, a bit of hall reverb so the
    small room recordings sound like a stadium."""
    if kind == "cheer":
        x = mix(fade(a.load(ROWDY), 0.8, 2.2), a.pitch(fade(a.load(ROWDY), 0.8, 2.2), -3) * 0.6)
    elif kind == "ooh":
        x = mix(fade(a.pitch(a.load(CROWD_VOX), -2), 0.5), fade(a.load(SMALL_CROWD), 0.5, 1.2) * 0.5)
    elif kind == "roar":
        x = mix(fade(a.load(ROWDY), 1.0, 2.6) * 1.1, at(fade(a.load(CHEERS), 0.4), 0.1) * 0.7,
                a.pitch(fade(a.load(ROWDY), 1.0, 2.6), -5) * 0.7)
    else:
        x = fade(a.load(APPLAUSE), 1.0, 3.0)
    return a.limit(a.comb_reverb(a.highpass(x, 150), 0.25, 1.6))


def warning():
    """Boss siren: the library siren over a square alarm, two pulses."""
    s = fade(a.load(SIREN), 0.3, 1.3)
    al = fade(a.load(ALARM), 0.2, 0.9) * 0.5
    return a.limit(a.drive(mix(s, al), 1.5))


def type_blip():
    return blip(2400, 0.025) * 0.5


def fire_burst():
    return a.limit(a.comb_reverb(mix(a.load(BOOM) * 1.3, a.pitch(a.load(EXPLOSION_SNARE), -4), a.lowpass(a.noise(0.9, 21), 1800) *
                                     a.envelope(int(0.9 * R), 0.005, 0.5) * 0.8), 0.35, 1.5))


def grab():
    return a.limit(mix(a.lowpass(a.noise(0.12, 4), 1800) * a.envelope(int(0.12 * R), decay=0.05), a.load(SNARE) * 0.4))


def voice(path, semitones=0.0, grit=1.0, echo=0.0, buzz=0.0, highcut=7000):
    x = a.trim(a.load(path), 0.02, 0.005)
    x = a.pitch(x, semitones)
    if buzz:
        t = np.arange(len(x)) / R
        x = x * (1 - buzz + buzz * np.sign(np.sin(2 * np.pi * 70 * t)))   # insect rasp
    x = a.highpass(x, 90)
    x = a.drive(x, grit)
    x = a.lowpass(x, highcut)
    if echo:
        x = a.comb_reverb(x, echo, 1.0)
    return a.limit(a.trim(x, 0.004, 0.02))


CHARACTER_VOICE = {   # semitones, grit, echo, buzz
    "vorax": (-3.5, 3.0, 0.15, 0.0),
    "zyra": (0.5, 1.2, 0.35, 0.0),
    "ossk": (-1.0, 2.2, 0.12, 0.35),
    "grumm": (-7.0, 2.5, 0.2, 0.0),
    "xal": (-2.5, 1.6, 0.45, 0.0),
    "krell": (-1.5, 1.8, 0.12, 0.18),
    "nyxa": (0.5, 1.3, 0.22, 0.0),
    "brutok": (-5.0, 3.0, 0.15, 0.0),
    "koral": (0.0, 1.2, 0.3, 0.0),
}
HURT_MAX = 0.6       # seconds: long hurt takes (up to 7 s) made every knockdown a drawn-out groan


def main():
    if os.path.isdir(OUT):
        shutil.rmtree(OUT)
    sounds = {
        (4, "hit_light"): hit(False), (4, "hit_heavy"): hit(True), (4, "block"): block(),
        (5, "swing_light"): swing(0.16, 900, 3500, 1), (5, "swing_heavy"): swing(0.3, 400, 2200, 2),
        (5, "jump"): swing(0.12, 1500, 4000, 6) * 0.6, (5, "land"): land(False), (5, "land_heavy"): land(True),
        (6, "projectile"): projectile(), (6, "super"): super_flash(), (1, "ko_boom"): ko_boom(),
        (6, "grab"): grab(), (6, "cursor"): blip(1200, 0.05), (6, "select"): mix(blip(700, 0.25, 1.0), at(blip(1400, 0.2), 0.05)),
        (6, "coin"): mix(blip(1650, 0.08), at(blip(2200, 0.25), 0.08)), (6, "count"): blip(880, 0.12),
        (6, "vs"): a.limit(a.comb_reverb(mix(a.load(ORCH), a.load(CHINA) * 0.6), 0.4, 1.3)),
        (6, "crowd_cheer"): crowd("cheer"), (6, "crowd_ooh"): crowd("ooh"), (6, "crowd_roar"): crowd("roar"),
        (6, "crowd_applause"): crowd("applause"), (6, "warning"): warning(), (5, "type"): type_blip(),
        (4, "fire_burst"): fire_burst(),
    }
    for (ch, name), x in sounds.items():
        a.write(os.path.join(OUT, str(ch), name + ".wav"), a.limit(x), 18500)
        a.write(os.path.join(PREVIEW, name + ".wav"), a.limit(x), 44100)
    # announcer: already treated by tools/voice_fx.py into audio/voice (18500 Hz)
    for path in sorted(glob.glob(os.path.join(ROOT, "audio", "voice", "*.wav"))):
        shutil.copy(path, os.path.join(OUT, "1", "ann_" + os.path.basename(path)))
    # fighters: channel 2 for vorax/ossk/xal, 3 for zyra/grumm, so the usual pairs don't cut each other
    # intro narration and the boss taunt: gentler treatment, announcer channel
    for name, path in [(f"story{i}", os.path.join(ROOT, "audio", "voice_raw", "story", f"story{i}.wav")) for i in range(1, 5)] + \
                      [("xal_intro", os.path.join(ROOT, "audio", "voice_raw", "xal", "intro.wav"))]:
        if os.path.exists(path):
            x = voice(path, -2.5 if name == "xal_intro" else -1.0, 1.3, 0.35 if name == "xal_intro" else 0.18)
            a.write(os.path.join(OUT, "1", name + ".wav"), x, 18500)
            a.write(os.path.join(PREVIEW, name + ".wav"), x, 44100)
    for name, (semi, grit, echo, buzz) in CHARACTER_VOICE.items():
        ch = 3 if name in ("zyra", "grumm", "nyxa", "koral") else 2
        for line in ("special1", "special2", "super", "hurt", "win"):
            src = os.path.join(ROOT, "audio", "voice_raw", name, line + ".wav")
            if os.path.exists(src):
                x = voice(src, semi, grit, echo, buzz)
                if line == "hurt":
                    x = fade(x, 0.15, HURT_MAX)
                elif len(x) > 1.8 * R:
                    x = fade(x, 0.25, 1.8)          # a special shout must not outlast the move
                a.write(os.path.join(OUT, str(ch), f"{name}_{line}.wav"), x, 18500)
                a.write(os.path.join(PREVIEW, f"{name}_{line}.wav"), x, 44100)
    total = sum(os.path.getsize(p) for p in glob.glob(os.path.join(OUT, "*", "*.wav")))
    print(f"sfx+voices: {len(glob.glob(os.path.join(OUT, '*', '*.wav')))} samples, {total // 1024} KB PCM")


if __name__ == "__main__":
    main()
