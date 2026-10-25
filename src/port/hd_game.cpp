/*
 * hode's main() for the Game & Watch (src/hode/main.cpp without the
 * command line and hode.ini): the menu, then the levels, until the player
 * quits. Progress is kept by the game in setup.cfg (save folder).
 */
#include "game.h"
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
	do {
		g->loadSetupCfg(resume);
		if (resume) {
			Menu *m = new Menu(g, g->_paf, g->_res, g->_video);
			const bool runGame = m->mainLoop();
			delete m;
			if (!runGame) {
				break;
			}
		}
		bool levelChanged = false;
		while (!g_system->inp.quit && level < kLvl_test) {
			g->displayLoadingScreen();
			g->mainLoop(level, checkpoint, levelChanged);
			if (resume) {
				g->saveSetupCfg();
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
