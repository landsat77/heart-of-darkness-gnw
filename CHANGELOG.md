# Changelog

## [v1.0.1] - 2026-10-26

- Save states now bring back the exact moment of the save (the engine's
  memory, about 700 KB per state), also with "Resume game" in the
  launcher. A state saved during a cutscene or a menu, or one that cannot
  be restored exactly (another version of the homebrew, packs moved in the
  external flash), restarts at its level's last checkpoint as before.
- A missing or wrong `setup.hod` shows a message on the screen (it was a
  hard fault).
- `tools/hod_pack.py`: the output folder is optional (default: a
  `HeartOfDarkness` folder where the tool is run), and a clear message when
  the game's files are not found (extract `HeartOfDarkness.exe` from the CD
  with 7-Zip).

## [v1.0.0] - 2026-10-26

- First version: Heart of Darkness (hode) as a Retro-Go SD homebrew.
- Packs made from the player's CD (`tools/hod_pack.py`), read in place
  from the external flash; v1.0, v1.1 and v1.2 data (only v1.1 tested).
- Cutscenes converted from `hod.paf` (`tools/paf_convert.py`).
- Save states (level + checkpoint), the game's own progress saves.
