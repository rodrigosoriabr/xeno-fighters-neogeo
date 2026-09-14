# Xeno Fighters (Neo Geo MVS). Needs: source ~/neodev/env.sh
# make        assets (only what changed) + ROM zip and MAME software list in build/rom
# make run    play in MAME 0.261 with the real SNK BIOS

include config.mk
-include build/res/crom_size.mk

GAMEROM = xenofighters
GAMETITLE = Xeno Fighters
BUILD = build
RES = $(BUILD)/res
ROM = $(BUILD)/rom
PROMSIZE = 1048576
SROMSIZE = 131072
MROMSIZE = 131072
VROMSIZE = 16777216
CROMSIZE ?= 4194304

PROM1 = $(ROM)/$(GAMEROM)-p1.p1
CROM1 = $(ROM)/$(GAMEROM)-c1.c1
CROM2 = $(ROM)/$(GAMEROM)-c2.c2
SROM1 = $(ROM)/$(GAMEROM)-s1.s1
MROM1 = $(ROM)/$(GAMEROM)-m1.m1
VROM1 = $(ROM)/$(GAMEROM)-v1.v1

SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=$(BUILD)/obj/%.o)
CFLAGS = $(EXTRA) -std=gnu99 -O2 -fomit-frame-pointer -Wall -Wno-unused-function -Wno-unused-variable -Isrc -I$(RES)

# Assets first, then a second make that sees their new timestamps. (With a phony "assets" prerequisite
# make kept stale objects: a regenerated stage shifted every later tile base and the fighters were drawn
# with the wrong tiles.)
all:
	$(PYTHON) tools/build_assets.py
	$(MAKE) --no-print-directory rom

rom: $(ROM)/$(GAMEROM).zip $(ROM)/neogeo.xml
.PHONY: all rom run clean

$(BUILD)/obj/%.o: src/%.c $(wildcard src/*.h) $(wildcard $(RES)/*.h) $(RES)/snd_commands.h | $(BUILD)/obj
	$(M68KGCC) $(NGCFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/rom.elf: $(OBJS)
	$(M68KGCC) -o $@ $^ $(NGLDFLAGS)

$(PROM1): $(BUILD)/rom.elf | $(ROM)
	$(M68KOBJCOPY) -O binary -S -R .text2 --gap-fill 0xff --pad-to $(PROMSIZE) $< $@ && dd if=$@ of=$@ conv=notrunc,swab status=none

$(CROM1): $(RES)/c1.bin | $(ROM)
	cp $< $@
$(CROM2): $(RES)/c2.bin | $(ROM)
	cp $< $@
$(SROM1): $(RES)/s1.fix | $(ROM)
	cp $< $@

# Sound: vromtool packs the ADPCM samples into the V-ROM and writes their offsets; soundtool writes the
# sample commands; our generated user_commands.s adds the looping music commands (tools/make_sound.py).
$(RES)/samples.inc: $(RES)/sample-map.yaml
	$(VROMTOOL) --asm -s $(VROMSIZE) $< -o $(VROM1) -m $@
# --asm only writes the offsets; --roms writes the V-ROM itself (X in the name = ROM number)
$(VROM1): $(RES)/sample-map.yaml | $(ROM)
	$(VROMTOOL) --roms -s $(VROMSIZE) $< -o $(ROM)/$(GAMEROM)-vX.vX -n 1
$(RES)/snd_commands.inc: $(RES)/sample-map.yaml
	$(SOUNDTOOL) -z -s $< -o $@
$(RES)/snd_commands.h: $(RES)/sample-map.yaml
	$(SOUNDTOOL) -c -s $< -o $@
$(BUILD)/user_commands.rel: $(RES)/user_commands.s $(RES)/samples.inc $(RES)/snd_commands.inc
	$(Z80SDAS) -g -l -p -u -I$(NGZ80INCLUDEDIR)/nullsound -I$(RES) -o $@ $<
$(BUILD)/sound_driver.ihx: $(BUILD)/user_commands.rel assets/ngdevkit/ngdevkit-eye-catcher.lib
	$(Z80SDLD) -b DATA=0xf800 -i $@ $(NGZ80LIBDIR)/nullsound.lib $^
$(MROM1): $(BUILD)/sound_driver.ihx | $(ROM)
	$(Z80SDOBJCOPY) -I ihex -O binary $< $@ --pad-to $(MROMSIZE)

$(ROM)/$(GAMEROM).zip: $(PROM1) $(CROM1) $(CROM2) $(SROM1) $(MROM1) $(VROM1)
	$(ROMTOOL) -b cartridge -f zip -p $(PROM1) -c $(CROM1) $(CROM2) -v $(VROM1) -s $(SROM1) -m $(MROM1) -n $(GAMEROM) -o $@

$(ROM)/neogeo.xml: $(ROM)/$(GAMEROM).zip
	$(ROMTOOL) -b hash -f mame -p $(PROM1) -c $(CROM1) $(CROM2) -v $(VROM1) -s $(SROM1) -m $(MROM1) -n $(GAMEROM) -l "$(GAMETITLE)" -o $@

$(BUILD)/obj $(ROM):
	mkdir -p $@

run: all
	~/neodev/mame-run.sh . "" "" real

clean:
	rm -rf $(BUILD)/obj $(BUILD)/rom.elf $(ROM)
