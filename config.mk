# Auto-generated config file for ngdevkit-examples
# This file only holds configuration for all the build rules
# It expects ngdevkit binary to be in your PATH
# Re-generate with ./configure

# ngdevkit dependencies
PKGCONFIG=/usr/local/bin/pkg-config
PYTHON=/Library/Frameworks/Python.framework/Versions/3.12/bin/python3
ZIP=/usr/bin/zip

# ngdevkit toolchain
M68KAR=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-ar
M68KAS=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-as
M68KGCC=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-gcc
M68KGXX=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-g++
M68KLD=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-ld
M68KOBJCOPY=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-objcopy
M68KRANLIB=/Users/rodrigosoria/neodev/local/bin/m68k-neogeo-elf-ranlib
Z80SDAR=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdar
Z80SDAS=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdasz80
Z80SDCC=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdcc
Z80SDLD=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdldz80
Z80SDOBJCOPY=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdobjcopy
Z80SDRANLIB=/Users/rodrigosoria/neodev/local/bin/z80-neogeo-ihx-sdranlib

# ngdevkit tools
PALTOOL=/Users/rodrigosoria/neodev/local/bin/paltool.py
TILETOOL=/Users/rodrigosoria/neodev/local/bin/tiletool.py
ADPCMTOOL=/Users/rodrigosoria/neodev/local/bin/adpcmtool.py
VROMTOOL=/Users/rodrigosoria/neodev/local/bin/vromtool.py
FURTOOL=/Users/rodrigosoria/neodev/local/bin/furtool.py
NSSTOOL=/Users/rodrigosoria/neodev/local/bin/nsstool.py
SOUNDTOOL=/Users/rodrigosoria/neodev/local/bin/soundtool.py
ROMTOOL=/Users/rodrigosoria/neodev/local/bin/romtool.py

# ngdevkit build flags
NGCFLAGS=`$(PKGCONFIG) --cflags ngdevkit`
NGLDFLAGS=`$(PKGCONFIG) --libs ngdevkit`
NGLIBDIR=`$(PKGCONFIG) --variable=libdir ngdevkit`
NGZ80INCLUDEDIR=`$(PKGCONFIG) --variable=z80includedir ngdevkit`
NGZ80LIBDIR=`$(PKGCONFIG) --variable=z80libdir ngdevkit`
# this variable must be resolved here as it is used as a base
# directory for dependencies (nullsound, nullbios)
NGSHAREDIR=$(shell $(PKGCONFIG) --variable=sharedir ngdevkit)

# ROM files to use
AES_BIOS=$(NGSHAREDIR)/aes.zip
MVS_BIOS=$(NGSHAREDIR)/neogeo.zip

# additional dependencies
CONVERT=/Users/rodrigosoria/neodev/local/bin/magick

# any additional config or dependencies can be added below
SOX=/Users/rodrigosoria/neodev/local/bin/sox
RSYNC=/usr/bin/rsync
GNGEO=

# GnGeo config
GNGEO_DATA=
GNGEO_GLSL=
GNGEO_SHADER_PATH=
GLSL_SHADER_PATH=
SHADER_PATH=
SHADER=noop.glslp

# OS-specific
ENABLE_MSYS2=
ENABLE_MINGW=no
GNGEO_INSTALL_PATH=
