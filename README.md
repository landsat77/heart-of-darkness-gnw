# Heart of Darkness for Game & Watch Retro-Go SD

**Heart of Darkness** (Amazing Studio, 1998) on the Nintendo Game & Watch
(Zelda or Mario) with the
[Retro-Go SD](https://github.com/sylverb/game-and-watch-retro-go-sd)
firmware, as a **GWHB homebrew**: a `.bin` dropped in `/homebrews/`, no
firmware rebuild needed.

The game is [hode](https://github.com/usineur/hode), Gregory Montoir's
re-implementation of the Heart of Darkness engine. All eight worlds, the
monsters' AI, the sounds and the music, the cutscenes.

**The game's data is not included**: you need your own copy of the
Windows CD. A Python script makes the files the console reads from it.

The project is built from
[retro-go-sd-templates](https://github.com/sylverb/retro-go-sd-templates)
and talks to the firmware only through the firmware ABI.

## Installation

Requires Retro-Go SD **v2.0.0-rc1 or later** (firmware ABI v2), and
Python 3 on your PC.

1. Download `HeartOfDarkness-vX.Y.Z.zip` from the
   [releases](https://github.com/landsat77/heart-of-darkness-gnw/releases).
2. Get the game's files from your CD: install the game, or open the CD's
   `HeartOfDarkness.exe` with [7-Zip](https://www.7-zip.org/) and extract
   everything to a folder. You need `SETUP.DAT` and the `*_HOD.LVL`,
   `*_HOD.SSS`, `*_HOD.MST` files; the cutscenes are the CD's
   `paf\hod.paf`.
3. Make the packs (from the zip's `tools` folder):
   ```
   python hod_pack.py <game folder> <SD card>/homebrews/HeartOfDarkness --paf <CD>/paf/hod.paf
   ```
   This writes about 82 MB of level packs and converts the cutscenes
   (`hod.gwp`, a few minutes).
4. Copy the zip's `homebrews` folder to the SD card's root:
   ```
   /homebrews/HeartOfDarkness.bin          the homebrew (Homebrew tab)
   /homebrews/HeartOfDarkness/setup.hod    the packs made in step 3
   /homebrews/HeartOfDarkness/rock.hod ...
   /homebrews/HeartOfDarkness/hod.gwp
   ```
5. Start "Heart of Darkness" from the **Homebrew** tab. The packs are
   copied to the external flash the first time they are needed ("Caching
   game file", a few seconds per level).

See [INSTALL.md](INSTALL.md) (English and French).

## Controls

| Game & Watch | Heart of Darkness |
|---|---|
| D-pad | Move, climb, crouch |
| A | Jump (menus: validate) |
| B | Shoot (menus: back) |
| TIME / SELECT | Run / special power (hold) |
| GAME / START | Game menu (skip a cutscene) |
| PAUSE/SET | Retro-Go menu (save states, volume…) |

Progress is saved by the game itself at each checkpoint
(`/retro-go/saves/hode/setup.cfg`). Save states record the level and the
last checkpoint: loading one restarts there, as after a death.

## How it works

- `src/hode/`: the engine (hode, unchanged except under `#ifdef GNW`).
- `src/port/`: the Game & Watch side: `hd_main.c` (the homebrew: display,
  sound, buttons, heap), `hd_system.cpp` (hode's System), `hd_fs.cpp`
  (files from the packs), `hd_game.cpp` (hode's main loop, save states),
  `hd_heap.c` (memory).
- **Packs**: `tools/hod_pack.py` puts the setup and each level in a file of
  its own, cached in the external flash and **read in place**: sprites,
  backgrounds, masks and sounds stay in flash; only what the engine
  modifies is copied to RAM. A level needs 510 to 700 KB of RAM (the PC
  game: 6 to 16 MB).
- **Memory**: the console gives a homebrew about 1 MB of RAM in several
  pieces; the port's heap spans all of them (the main RAM left after the
  code, the half of the LCD memory that the 8-bit display mode frees, the
  CPU's tightly coupled memories).
- **Cutscenes**: the game's PAF player needs ~330 KB, more than is left
  inside a level. `tools/paf_convert.py` converts `hod.paf` once on the
  PC into a stream the console plays with one picture buffer (frame
  differences + mono sound).
- **Display**: 256 x 192 scaled to 320 x 240, 8-bit palette (the LCD's
  LUT8 mode). The game runs at its own speed, 12.5 frames per second; in
  gwemu the console is about 90 % idle.

## Building

```
make            # → build/sd/homebrews/HeartOfDarkness.bin
make host       # PC version (SDL2): ./HeartOfDarkness_host <dir>/HeartOfDarkness.bin
make docker     # the homebrew inside the Retro-Go SD builder image
```

Needs `arm-none-eabi-gcc` (hard-float, with newlib and its C++ headers),
GNU Make, Python 3 with Pillow. On Ubuntu:
`sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-dev libsdl2-dev python3-pil`.

The PC version takes the path of the `.bin` and reads the packs in the
folder of the same name beside it. Keys: arrows, X = A, Z = B, Shift =
TIME, Enter = GAME. Scripted runs for tests: `HD_SCRIPT`, `HD_SHOTS`,
`HD_LEVEL`… (see `src/port/hd_main.c` and `CLAUDE.md`).

## Credits and licences

- Heart of Darkness: © 1998 Amazing Studio / Infogrames. The game's data
  is theirs and is not distributed here.
- hode: © 2009-2011 Gregory Montoir (and contributors). **The hode repository has no licence
  file**; its source is included here unchanged except for the `GNW`
  parts, with credit to its author. If you are the author and object,
  please open an issue.
- Game & Watch port (`src/port/`, tools) and the Retro-Go SD SDK: MIT
  (`LICENSE`).
