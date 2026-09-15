# Xeno Fighters (Neo Geo MVS)

Homebrew 2D fighting game in the KOF style for the Neo Geo, built with ngdevkit. 8 playable aliens
plus the boss Xal, GPT-generated art, 9 stages with animated crowds, arcade mode with story, endings
and credits, EASY/NORMAL/HARD and STANDARD/TURBO speed. Plays in MAME 0.261 with the real SNK BIOS
and in Arcade Nostalgia as `~/Desktop/games/roms/Xeno Fighters.neocart`.

## Build and run

```sh
source ~/neodev/env.sh          # toolchain in ~/neodev/local (see ~/neodev/SETUP.md)
make                            # assets that changed + ROM in build/rom
make run                        # MAME with the real BIOS
python3 tools/package.py        # .neocart for Arcade Nostalgia
python3 tools/package_windows.py   # build/XenoFighters-Windows.zip: MAME 0.261 + cart + open nullbios + JOGAR.bat
make EXTRA=-DSCREEN_TEST        # build that walks every screen (test/screen_test.lua)
make EXTRA=-DSTAGE_TEST         # 10 s of CPU fight on each stage (test/stage_test.lua)
~/neodev/mame-run.sh . 60 $PWD/test/soak.lua real   # headless: random input, logs stuck states
```

## Layout

| Path | Role |
|---|---|
| `src/main.c` | match loop, arcade ladder, effects (cut-in, afterimage, words, crowd reactions) |
| `src/fighter.c`, `src/cpu.c`, `src/chars.c` | fighter state machine, CPU by level, roster and moves |
| `src/screens.c`, `src/story.c` | intro, title, options, select, VS, boss, win, continue, ending |
| `src/hud.c`, `src/stage.c`, `src/fx.c` | lifebars/power stocks, stage + crowd + particles, hit effects |
| `src/video.c`, `src/sound.c` | sprite/palette helpers (hardware zoom), sound commands |
| `tools/build_assets.py` | runs the converters that are out of date, assembles C-ROM |
| `tools/make_*.py` | fighter, stage, crowd, fx, fix, screens, sfx, music, sound → `build/res` |
| `tools/gen_sheets.py`, `gen_jobs.py`, `gpt_voice.py` | GPT image sheets and TTS voices (need `OPENAI_API_KEY`) |
| `art/gpt/`, `audio/voice_raw/` | generated source art and voices (expensive to regenerate: committed) |
| `audio/out/` | rendered SFX/music WAVs (committed: `make_sfx.py` reads the sample library on the external SSD) |
| `data/*.json` | per-fighter frame data and per-stage bands/crowd/particles |

Traps (sprite Y, hardware zoom, palettes, fix font, sound): global skill `neogeo-homebrew`.
