/*
 * hode's System (system.h) on the Game & Watch: an 8-bit 256 x 192 canvas
 * shown by the platform (hd_plat.h); time = sound played; sleep() runs
 * audio periods.
 */
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "hd_plat.h"

struct System_GNW : System {
	uint8_t _canvas[HD_W * HD_H];
	uint8_t _pal[256 * 3];
	int _shakeDx, _shakeDy;
	AudioCallback _audio;

	System_GNW() : _shakeDx(0), _shakeDy(0) {
		memset(_canvas, 0, sizeof(_canvas));
		memset(_pal, 0, sizeof(_pal));
		_audio.proc = 0;
		_audio.userdata = 0;
	}
	virtual void init(const char *title, int w, int h, bool fullscreen, bool widescreen, bool yuv) {}
	virtual void destroy() {}
	virtual void setScaler(const char *name, int multiplier) {}
	virtual void setGamma(float gamma) {}
	virtual void setPalette(const uint8_t *pal, int n, int depth) {
		const int shift = 8 - depth;
		for (int i = 0; i < n * 3 && i < 256 * 3; ++i) {
			int c = pal[i];
			if (shift != 0) {
				c = (c << shift) | (c >> (depth - shift));
			}
			_pal[i] = c;
		}
	}
	virtual void clearPalette() {
		memset(_pal, 0, sizeof(_pal));
	}
	virtual void copyRect(int x, int y, int w, int h, const uint8_t *buf, int pitch) {
		for (int j = 0; j < h; ++j) {
			if (y + j < 0 || y + j >= HD_H) continue;
			memcpy(_canvas + (y + j) * HD_W + x, buf + j * pitch, w);
		}
	}
	virtual void copyYuv(int w, int h, const uint8_t *y, int ypitch, const uint8_t *u, int upitch, const uint8_t *v, int vpitch) {}
	virtual void fillRect(int x, int y, int w, int h, uint8_t color) {
		for (int j = 0; j < h; ++j) {
			if (y + j < 0 || y + j >= HD_H) continue;
			memset(_canvas + (y + j) * HD_W + x, color, w);
		}
	}
	virtual void copyRectWidescreen(int w, int h, const uint8_t *buf, const uint8_t *pal) {}
	virtual void shakeScreen(int dx, int dy) {
		_shakeDx = dx;
		_shakeDy = dy;
	}
	virtual void updateScreen(bool drawWidescreen) {
		hd_plat_present(_canvas, _pal, _shakeDx, _shakeDy);
	}
	virtual void processEvents() {
		pad.prevMask = pad.mask;
		pad.mask = 0;
		inp.prevMask = inp.mask;
		inp.mask = hd_plat_buttons();
	}
	virtual void sleep(int duration) {
		const uint32_t end = hd_plat_time_ms() + duration;
		do {
			hd_plat_service();
		} while ((int32_t)(hd_plat_time_ms() - end) < 0);
	}
	virtual uint32_t getTimeStamp() {
		return hd_plat_time_ms();
	}
	virtual void startAudio(AudioCallback callback) {
		_audio = callback;
		hd_plat_set_audio(callback.proc, callback.userdata);
	}
	virtual void stopAudio() {
		hd_plat_set_audio(0, 0);
	}
	virtual void lockAudio() {
		hd_plat_audio_lock(1);
	}
	virtual void unlockAudio() {
		hd_plat_audio_lock(0);
	}
	virtual AudioCallback setAudioCallback(AudioCallback callback) {
		AudioCallback prev = _audio;
		_audio = callback;
		hd_plat_set_audio(callback.proc, callback.userdata);
		return prev;
	}
};

static System_GNW system_gnw;
System *const g_system = &system_gnw;

void System_earlyInit() {
}

void System_printLog(FILE *fp, const char *s) {
	hd_plat_log(s);
}

void System_fatalError(const char *s) {
	hd_plat_fatal(s);
}

bool System_hasCommandLine() {
	return false;
}
