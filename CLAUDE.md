# Xeno Fighters (Neo Geo)

- `source ~/neodev/env.sh` before `make`. Never run brew. Never commit ROMs or `build/`.
- Test headless in MAME with the real BIOS (`~/neodev/mame-run.sh . <secs> $PWD/test/x.lua real`); the terminal cannot record the screen, check snapshots. After gameplay changes run `test/soak.lua` (0 STUCK).
- Release = `make` (without `EXTRA`) + `python3 tools/package.py`; the SCREEN_TEST build must never be packaged.
- OpenAI key lives in `~/.zshrc`: run GPT tools via `zsh -ic '...'`. Never read keys from files.
- Sprite/palette ranges are documented at the top of `src/video.h`; traps in global skill `neogeo-homebrew`. Brain: `~/Obsidian/xeno-fighters/`.
