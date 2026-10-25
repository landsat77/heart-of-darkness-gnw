#ifndef HD_GAME_H
#define HD_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* The game (never returns before the player quits). level < 0: the menu,
   resuming from setup.cfg; else that level and checkpoint. */
void hd_game_run(int level, int checkpoint);

#ifdef __cplusplus
}
#endif

#endif
