#!/usr/bin/env python3
"""Windows package: everything needed to play on a Windows PC, zipped as build/XenoFighters-Windows.zip.

  package_windows.py        (after `make`; needs py7zr: python3 -m pip install --user py7zr)

Contents: MAME 0.261 64-bit for Windows (the version the game is tested with; downloaded once into
build/cache and checked against the official SHA256), the cart (xenofighters.zip + neogeo.xml software
list), ngdevkit's open nullbios as neogeo.zip (redistributable, unlike the SNK BIOS), JOGAR.bat,
LEIA-ME.txt and the licenses. The .bat prefers an SNK neogeo.zip dropped into bios-snk/.

MAME tools, docs, the 144 MB debug symbols and its own hash/ folder are left out: the game only needs
mame.exe and the cart's software list."""
import hashlib, os, shutil, sys, urllib.request, zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build")
CACHE = os.path.join(BUILD, "cache")
MAME_URL = "https://github.com/mamedev/mame/releases/download/mame0261/mame0261b_64bit.exe"
MAME_SHA256 = "0678dac2c795b947b742938855ceabf1136deb9fdd994a57e69c647cc370a5d4"
NULLBIOS = os.path.expanduser("~/neodev/local/share/ngdevkit/neogeo.zip")
NGDEVKIT = os.path.expanduser("~/neodev/ngdevkit")
MAME_KEEP = ["mame.exe", "COPYING", "bgfx", "hlsl", "plugins", "artwork", "language", "ini", "ctrlr", "uismall.bdf"]
NAME = "XenoFighters-Windows"

BAT = r"""@echo off
rem Xeno Fighters - Neo Geo MVS homebrew, runs in the bundled MAME 0.261
cd /d "%~dp0mame"
set BIOS=..\bios
if exist "..\bios-snk\neogeo.zip" set BIOS=..\bios-snk
mame.exe neogeo -cart1 xenofighters -hashpath ..\roms -rompath "..\roms;%BIOS%" -skip_gameinfo
if errorlevel 1 pause
"""

README = """XENO FIGHTERS - Neo Geo MVS (homebrew)
======================================

COMO JOGAR
  1. Descompacte a pasta inteira (nao rode de dentro do zip).
  2. Clique duas vezes em JOGAR.bat.
     Se o Windows avisar "o Windows protegeu o computador": Mais informacoes > Executar assim mesmo.
  3. Aperte 5 para colocar credito e 1 para comecar.

TECLADO (padrao do MAME)
  Jogador 1: setas movem
             A = Ctrl esquerdo   (soco fraco)
             B = Alt esquerdo    (chute fraco)
             C = Espaco          (soco forte)
             D = Shift esquerdo  (chute forte)
             5 = credito, 1 = start
  Jogador 2: R F D G movem, A S Q W = botoes A B C D, 6 = credito, 2 = start
  Esc = sair   Tab = menu do MAME (troca teclas e liga controle USB em "Input (general)")
  Alt+Enter = janela / tela cheia

GOLPES (lutador olhando para a direita; A ou C = qualquer um dos dois)
{moves}

BIOS
  O pacote vem com a nullbios, uma BIOS livre (projeto ngdevkit). O MAME escreve
  "WRONG CHECKSUMS" no console por causa dela: e esperado, o jogo funciona.
  Quem tiver o neogeo.zip original da SNK pode coloca-lo em bios-snk\\ e o JOGAR.bat usa ele.

LICENCAS
  MAME: GPL-2.0 (licenses\\MAME-COPYING.txt) - https://www.mamedev.org
  nullbios / ngdevkit: LGPL-3.0 (licenses\\ngdevkit-*.txt) - https://github.com/dciabrin/ngdevkit
"""


MOTIONS = {"236236": "meia-lua frente 2x", "236": "baixo, baixo-frente, frente", "623": "frente, baixo, baixo-frente",
           "214": "baixo, baixo-tras, tras", "66": "frente, frente", "44": "tras, tras", "2": "baixo", "6": "frente",
           "8": "cima", "4": "tras", "1": "baixo-tras", "": ""}


def move_text():
    sys.path.insert(0, os.path.dirname(__file__))
    from package import COMMON, FIGHTERS
    def line(name, spec):
        motion, _, buttons = spec.partition(" ")
        keys = " + ".join(x for x in (MOTIONS[motion], " ou ".join(buttons)) if x)
        return f"    {name:<28} {keys}"
    out = ["  Todos:"] + [line(n, s) for n, s in COMMON]
    for name, moves in FIGHTERS:
        out += [f"  {name}:"] + [line(n, s) for n, s in moves]
    return "\n".join(out)


def mame_folder():
    exe = os.path.join(CACHE, "mame0261b_64bit.exe")
    out = os.path.join(CACHE, "mame0261")
    if os.path.exists(os.path.join(out, "mame.exe")):
        return out
    os.makedirs(CACHE, exist_ok=True)
    if not os.path.exists(exe):
        print("downloading", MAME_URL)
        urllib.request.urlretrieve(MAME_URL, exe)
    data = open(exe, "rb").read()
    assert hashlib.sha256(data).hexdigest() == MAME_SHA256, "MAME download does not match the official SHA256"
    # the official .exe is a 7-Zip self-extractor: the archive starts at the 7z signature
    import py7zr
    archive = os.path.join(CACHE, "mame0261.7z")
    open(archive, "wb").write(data[data.find(b"7z\xbc\xaf\x27\x1c"):])
    with py7zr.SevenZipFile(archive) as z:
        z.extractall(out)
    os.remove(archive)
    return out


def main():
    rom = os.path.join(BUILD, "rom")
    for f in ("xenofighters.zip", "neogeo.xml"):
        assert os.path.exists(os.path.join(rom, f)), "run make first"
    mame = mame_folder()
    stage = os.path.join(BUILD, NAME)
    shutil.rmtree(stage, ignore_errors=True)
    for d in ("mame", "roms", "bios", "bios-snk", "licenses"):
        os.makedirs(os.path.join(stage, d))
    for item in MAME_KEEP:
        src = os.path.join(mame, item)
        (shutil.copytree if os.path.isdir(src) else shutil.copy2)(src, os.path.join(stage, "mame", item))
    for f in ("xenofighters.zip", "neogeo.xml"):
        shutil.copy2(os.path.join(rom, f), os.path.join(stage, "roms", f))
    shutil.copy2(NULLBIOS, os.path.join(stage, "bios", "neogeo.zip"))
    open(os.path.join(stage, "bios-snk", "COLOQUE-AQUI-O-neogeo.zip-DA-SNK.txt"), "w").write(
        "Opcional: o neogeo.zip original da SNK. Sem ele o jogo usa a nullbios da pasta bios.\r\n")
    shutil.copy2(os.path.join(mame, "COPYING"), os.path.join(stage, "licenses", "MAME-COPYING.txt"))
    for f in ("COPYING", "COPYING.LESSER"):
        shutil.copy2(os.path.join(NGDEVKIT, f), os.path.join(stage, "licenses", f"ngdevkit-{f}.txt"))
    open(os.path.join(stage, "JOGAR.bat"), "w", newline="\r\n").write(BAT)
    open(os.path.join(stage, "LEIA-ME.txt"), "w", newline="\r\n").write(README.replace("{moves}", move_text()))
    out = os.path.join(BUILD, NAME + ".zip")
    if os.path.exists(out):
        os.remove(out)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for base, _, files in os.walk(stage):
            for f in files:
                path = os.path.join(base, f)
                z.write(path, os.path.relpath(path, BUILD))
    print(f"{out}: {os.path.getsize(out) / 1e6:.1f} MB")


if __name__ == "__main__":
    main()
