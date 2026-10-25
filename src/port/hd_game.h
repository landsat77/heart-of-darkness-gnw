#ifndef HD_GAME_H
#define HD_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

/* The game (never returns before the player quits). level < 0: the menu,
   resuming from setup.cfg; else that level and checkpoint. */
void hd_game_run(int level, int checkpoint);

/* Save states: where the player is (level, checkpoint, difficulty...).
   hd_game_get_state() fails (0) outside a level. hd_game_set_state()
   ends the level being played and restarts at that checkpoint; before
   hd_game_run() (resume from the launcher), the game starts there. */
typedef struct {
	int level, checkpoint, difficulty, volume;
	unsigned cutscenes;	/* cutscenes already seen */
} hd_state_t;
int hd_game_get_state(hd_state_t *state);
int hd_game_set_state(const hd_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
