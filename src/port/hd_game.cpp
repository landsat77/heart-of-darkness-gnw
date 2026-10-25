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

extern "C" void hd_game_run(int level, int checkpoint) {
	const bool resume = level < 0; // from setup.cfg, with the menu
	if (level < 0) {
		level = 0;
		checkpoint = 0;
	}
	g_debugMask = 0;
	Game *g = new Game(hd_plat_data_dir(), hd_plat_save_dir(), 0);
	hd_game = g;
	g->_res->loadSetupDat();
	g_system->init("Heart of Darkness", Video::W, Video::H, false, false, false);
	g->_mix._lock = lockAudio;
	AudioCallback cb;
	cb.proc = mixAudio;
	cb.userdata = g;
	g_system->startAudio(cb);
	g->displayLoadingScreen();
	started = true;
	do {
		g->loadSetupCfg(resume);
		bool levelChanged = false;
		if (takePending(g, &level, &checkpoint)) {
			levelChanged = true; /* no level intro */
		} else if (resume) {
			Menu *m = new Menu(g, g->_paf, g->_res, g->_video);
			const bool runGame = m->mainLoop();
			delete m;
			if (!runGame) {
				break;
			}
		}
		while (!g_system->inp.quit && level < kLvl_test) {
			g->displayLoadingScreen();
			inLevel = true;
			g->mainLoop(level, checkpoint, levelChanged);
			inLevel = false;
			if (resume) {
				g->saveSetupCfg();
			}
			if (takePending(g, &level, &checkpoint)) {
				levelChanged = true;
				continue;
			}
			if (g->_res->_isDemo) {
				break;
			}
			level = g->_currentLevel + 1;
			checkpoint = 0;
			levelChanged = true;
		}
		level = 0;
	} while (!g_system->inp.quit && resume);
	g_system->stopAudio();
}
