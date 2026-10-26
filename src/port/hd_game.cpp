/*
 * hode's main() for the Game & Watch (src/hode/main.cpp without the
 * command line and hode.ini): the menu, then the levels, until the player
 * quits. Progress is kept by the game in setup.cfg (save folder).
 */
#include "game.h"
#include "level.h"
#include "menu.h"
#include "mixer.h"
#include "paf.h"
#include "util.h"
#include "resource.h"
#include "system.h"
#include "video.h"

#include "hd_plat.h"
#include "hd_game.h"

static void lockAudio(int flag) {
	if (flag) {
		g_system->lockAudio();
	} else {
		g_system->unlockAudio();
	}
}

static void mixAudio(void *userdata, int16_t *buf, int len) {
	((Game *)userdata)->mixAudio(buf, len);
}

Game *hd_game;

/* The cutscenes' picture (src/hode/paf.cpp, GNW): the shadow layer, which
   the game draws again every frame and the menus do not use. */
uint8_t *hd_cutscene_picture() {
	return hd_game->_video->_shadowLayer;
}
static bool started, inLevel;
static bool pending;
static hd_state_t pendingState;

extern "C" int hd_game_get_state(hd_state_t *st) {
	if (!inLevel || !hd_game || !hd_game->_level) {
		return 0;
	}
	st->level = hd_game->_currentLevel;
	st->checkpoint = hd_game->_level->_checkpoint;
	st->difficulty = hd_game->_difficulty;
	st->volume = hd_game->_snd_masterVolume;
	st->cutscenes = hd_game->_paf->_playedMask;
	return 1;
}

extern "C" int hd_game_set_state(const hd_state_t *st) {
	if (st->level < 0 || st->level >= kLvl_test || st->checkpoint < 0) {
		return 0;
	}
	if (started && !inLevel) {
		return 0; /* the menus, a cutscene between levels */
	}
	pendingState = *st;
	pending = true;
	if (inLevel) {
		hd_game->_endLevel = true;
	}
	return 1;
}

/* A state to play: its level and checkpoint, the rest of it set here. */
static bool takePending(Game *g, int *level, int *checkpoint) {
	if (!pending) {
		return false;
	}
	pending = false;
	*level = pendingState.level;
	*checkpoint = pendingState.checkpoint;
	if (*checkpoint >= g->_res->_datHdr.levelCheckpointsCount[*level]) {
		*checkpoint = 0;
	}
	g->_difficulty = pendingState.difficulty;
	g->_snd_masterVolume = pendingState.volume;
	g->_paf->setVolume(pendingState.volume);
	g->_paf->_playedMask = pendingState.cutscenes;
	g->_resumeGame = false;
	return true;
}

/* Where the game is (statics: part of a save state, src/port/hd_main.c) */
static bool sResume;		/* from setup.cfg, with the menu */
static int sLevel, sCheckpoint;
static bool sLevelChanged;

/* The menu and the levels until the player quits. inLevelNow: a save state
   was just loaded, the level being played goes on (Game::gnwLevelLoop()). */
static void play(Game *g, bool inLevelNow) {
	do {
		if (!inLevelNow) {
			g->loadSetupCfg(sResume);
			sLevelChanged = false;
			if (takePending(g, &sLevel, &sCheckpoint)) {
				sLevelChanged = true; /* no level intro */
			} else if (sResume) {
				Menu *m = new Menu(g, g->_paf, g->_res, g->_video);
				const bool runGame = m->mainLoop();
				delete m;
				if (!runGame) {
					break;
				}
			}
		}
		while (!g_system->inp.quit && sLevel < kLvl_test) {
			inLevel = true;
			if (inLevelNow) {
				inLevelNow = false;
				g->gnwLevelLoop();
			} else {
				g->displayLoadingScreen();
				g->mainLoop(sLevel, sCheckpoint, sLevelChanged);
			}
			inLevel = false;
			if (sResume) {
				g->saveSetupCfg();
			}
			if (takePending(g, &sLevel, &sCheckpoint)) {
				sLevelChanged = true;
				continue;
			}
			if (g->_res->_isDemo) {
				break;
			}
			sLevel = g->_currentLevel + 1;
			sCheckpoint = 0;
			sLevelChanged = true;
		}
		sLevel = 0;
	} while (!g_system->inp.quit && sResume);
	g_system->stopAudio();
}

static void startAudio(Game *g) {
	g->_mix._lock = lockAudio;
	AudioCallback cb;
	cb.proc = mixAudio;
	cb.userdata = g;
	g_system->startAudio(cb);
}

extern "C" void hd_game_run(int level, int checkpoint) {
	sResume = level < 0;
	sLevel = level < 0 ? 0 : level;
	sCheckpoint = level < 0 ? 0 : checkpoint;
	g_debugMask = 0;
	Game *g = new Game(hd_plat_data_dir(), hd_plat_save_dir(), 0);
	hd_game = g;
	g->_res->loadSetupDat();
	g_system->init("Heart of Darkness", Video::W, Video::H, false, false, false);
	startAudio(g);
	g->displayLoadingScreen();
	started = true;
	play(g, false);
}

extern "C" void hd_game_continue(void) {
	Game *g = hd_game;
	startAudio(g);
	play(g, true);
}

extern "C" int hd_game_in_level(void) {
	return inLevel && hd_game && hd_game->_level;
}
