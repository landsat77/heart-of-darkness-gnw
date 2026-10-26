# Changelog

## [v1.0.0] - 2026-10-26

- First version: Heart of Darkness (hode) as a Retro-Go SD homebrew.
- Packs made from the player's CD (`tools/hod_pack.py`), read in place
  from the external flash; v1.0, v1.1 and v1.2 data (only v1.1 tested).
- Cutscenes converted from `hod.paf` (`tools/paf_convert.py`).
- Save states: the exact moment (the engine's memory), or the level's
  last checkpoint when that is not possible; the game's own progress saves.
- A missing `setup.hod` shows a message (it was a hard fault).
