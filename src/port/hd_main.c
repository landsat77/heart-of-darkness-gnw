/*
 * Heart of Darkness (hode) for Retro-Go SD (Game & Watch), as a GWHB
 * homebrew: /homebrews/HeartOfDarkness.bin, the game's packs in
 * /homebrews/HeartOfDarkness/ (setup.hod, rock.hod ... dark.hod, made by
 * tools/hod_pack.py from the PC game) and its cutscenes (hod.paf, from the
 * CD, optional) beside them.
 *
 * This file is the platform (hd_plat.h): packs cached in external flash
 * and read in place, the 256 x 192 picture scaled 5/4 to the 320 x 240 LCD,
 * sound (the game's 22050 Hz stereo mixer, down-mixed), buttons, the
 * Retro-Go menu. The game itself runs as a blocking loop (hd_game.cpp);
 * every sleep() of the game runs audio periods here.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

#include "common.h"
#include "gw_lcd.h"
#include "gw_audio.h"
#include "rom_manager.h"
#include "odroid_system.h"
#include "appid.h"
#include "odroid_overlay.h"
#include "odroid_settings.h"
#include "gw_malloc.h"
#include "rg_storage.h"

#ifndef HOST_BUILD
#include "odroid_sdcard.h"
#include "gw_core_bridge.h"
#else
#include <stdlib.h>
#include "host_compat.h"
#endif

#include "hd_plat.h"
#include "hd_heap.h"
#include "hd_game.h"
#include "hd_fs.h"

#define APP_ID APPID_HOMEBREW
#define AUDIO_PERIOD (HD_SAMPLE_RATE / 50)	/* 441 samples, 20 ms */
#define AHB_MARGIN (16 * 1024)
#define RAM_MARGIN (16 * 1024)			/* RAM_EMU left to the firmware */

#ifdef HOST_BUILD
static char save_dir[] = "host_saves/hode";
#else
static char save_dir[] = RG_BASE_PATH_SAVES "/hode";
#endif
static char data_dir[256];

#ifdef HOST_BUILD
static void test_input(odroid_gamepad_state_t *j);
static void test_step(void);
#endif

/* --- packs --- */

#define MAX_PACKS 12
static struct {
	char name[16];
	const uint8_t *data;
	uint32_t size;
} packs[MAX_PACKS];

const uint8_t *hd_plat_pack(const char *name, uint32_t *size)
{
	for (int i = 0; i < MAX_PACKS; i++) {
		if (packs[i].data && strcmp(packs[i].name, name) == 0) {
			*size = packs[i].size;
			return packs[i].data;
		}
	}
	char path[300];
	snprintf(path, sizeof path, "%s/%s.hod", data_dir, name);
	FILE *f = fopen(path, "rb");
	if (!f) {
		printf("hode: %s not found\n", path);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	uint32_t n = ftell(f);
	fclose(f);
	const uint8_t *data = odroid_overlay_cache_file_in_flash(path, &n, false);
	if (!data || !n) {
		printf("hode: %s: could not be copied to the flash\n", path);
		return NULL;
	}
	for (int i = 0; i < MAX_PACKS; i++) {
		if (!packs[i].data) {
			snprintf(packs[i].name, sizeof packs[i].name, "%s", name);
			packs[i].data = data;
			packs[i].size = n;
			break;
		}
	}
	printf("hode: pack %s at %p (%lu bytes)\n", name, (const void *)data, (unsigned long)n);
	*size = n;
	return data;
}

const char *hd_plat_data_dir(void) { return data_dir; }
const char *hd_plat_save_dir(void) { return save_dir; }

/* --- picture --- */

static bool restore_pending;	/* an exact save state to restore (below) */
static void restore_and_jump(void);
static unsigned stat_frames;	/* game frames presented (log every 5 s) */
static const uint8_t *frame_pixels;
static int frame_pitch;
static uint8_t frame_palette[256 * 3];
static int frame_dx, frame_dy;
static uint16_t xmap[WIDTH];
static uint8_t ymap[HEIGHT];

static void set_clut(void)
{
	uint32_t clut[256];
	for (int i = 0; i < 256; i++) {
		const uint8_t *c = frame_palette + i * 3;
		clut[i] = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
	}
	lcd_set_clut(clut, 256);
}

/* LUT8: the game's 8-bit pixels scaled 5/4, its palette in the CLUT. */
static void blit(void)
{
	uint8_t *fb = lcd_get_active_buffer();
	set_clut();
	if (!frame_pixels) {
		memset(fb, 0, WIDTH * HEIGHT);
		return;
	}
	for (int y = 0; y < HEIGHT; y++) {
		uint8_t *d = fb + y * WIDTH;
		int sy = ymap[y] - frame_dy;
		if (sy < 0 || sy >= HD_H) {
			memset(d, 0, WIDTH);
			continue;
		}
		const uint8_t *s = frame_pixels + sy * frame_pitch;
		if (frame_dx == 0) {
			for (int x = 0; x < WIDTH; x++) d[x] = s[xmap[x]];
		} else {
			for (int x = 0; x < WIDTH; x++) {
				int sx = xmap[x] - frame_dx;
				d[x] = (sx >= 0 && sx < HD_W) ? s[sx] : 0;
			}
		}
	}
}

static void repaint(void)
{
	blit();
	common_ingame_overlay();
}

static void wait_swap(void)
{
	while (lcd_is_swap_pending()) wdog_refresh();
}

void hd_plat_present(const uint8_t *pixels, int pitch, const uint8_t *palette, int dx, int dy)
{
	frame_pixels = pixels;
	frame_pitch = pitch;
	memcpy(frame_palette, palette, sizeof frame_palette);
	frame_dx = dx;
	frame_dy = dy;
	stat_frames++;
	wait_swap();
	blit();
	common_ingame_overlay();
	lcd_swap();
}

/* --- buttons --- */

static unsigned buttons;
static odroid_gamepad_state_t joystick;
static odroid_dialog_choice_t options[1] = { ODROID_DIALOG_CHOICE_LAST };

unsigned hd_plat_buttons(void) { return buttons; }

static void read_buttons(void)
{
	odroid_input_read_gamepad(&joystick);
	common_emu_input_loop(&joystick, options, &repaint);
#ifdef HOST_BUILD
	test_input(&joystick);
#endif
	unsigned b = 0;
	if (joystick.values[ODROID_INPUT_UP]) b |= HD_UP;
	if (joystick.values[ODROID_INPUT_DOWN]) b |= HD_DOWN;
	if (joystick.values[ODROID_INPUT_LEFT]) b |= HD_LEFT;
	if (joystick.values[ODROID_INPUT_RIGHT]) b |= HD_RIGHT;
	if (joystick.values[ODROID_INPUT_A]) b |= HD_JUMP;
	if (joystick.values[ODROID_INPUT_B]) b |= HD_SHOOT;
	if (joystick.values[ODROID_INPUT_Y] || joystick.values[ODROID_INPUT_SELECT]) b |= HD_RUN;
	if (joystick.values[ODROID_INPUT_X] || joystick.values[ODROID_INPUT_START]) b |= HD_ESC;
	buttons = b;
}

/* --- PC tests (host build): scripted runs --- */

#ifdef HOST_BUILD
/*
 * HD_TICKS=n       quit after n audio periods (20 ms each)
 * HD_SCRIPT=list   "tick:buttons,..." from that tick on, buttons held
 *                  (U D L R A B T = TIME/run G = GAME/esc, - = none)
 * HD_SHOTS=dir     with HD_SHOT_AT="tick,tick,...": the LCD as PPM files
 * HD_HEAP_KB=n     heap size (default: what the console has)
 * HD_STATE=file    with HD_SAVE_AT=tick / HD_LOAD_AT=tick: save state tests
 * HD_WAV=file      the sound (raw s16 mono 22050 Hz)
 */
static bool LoadState(const char *path);
static bool SaveState(const char *path);
static int test_tick;

static bool in_list(const char *list, int value)
{
	for (const char *p = list; p && *p;) {
		if (atoi(p) == value) return true;
		p = strchr(p, ',');
		if (p) p++;
	}
	return false;
}

static void test_input(odroid_gamepad_state_t *j)
{
	const char *held = NULL;
	for (const char *p = getenv("HD_SCRIPT"); p && *p;) {
		int at = atoi(p);
		const char *colon = strchr(p, ':');
		if (!colon || at > test_tick) break;
		held = colon + 1;
		p = strchr(colon, ',');
		if (p) p++;
	}
	if (!held) return;
	for (const char *c = held; *c && *c != ','; c++) {
		switch (*c) {
		case 'U': j->values[ODROID_INPUT_UP] = 1; break;
		case 'D': j->values[ODROID_INPUT_DOWN] = 1; break;
		case 'L': j->values[ODROID_INPUT_LEFT] = 1; break;
		case 'R': j->values[ODROID_INPUT_RIGHT] = 1; break;
		case 'A': j->values[ODROID_INPUT_A] = 1; break;
		case 'B': j->values[ODROID_INPUT_B] = 1; break;
		case 'T': j->values[ODROID_INPUT_Y] = 1; break;
		case 'G': j->values[ODROID_INPUT_X] = 1; break;
		}
	}
}

static void test_step(void)
{
	const char *dir = getenv("HD_SHOTS");
	if (dir && in_list(getenv("HD_SHOT_AT"), test_tick)) {
		char path[300];
		snprintf(path, sizeof path, "%s/shot_%06d.ppm", dir, test_tick);
		FILE *f = fopen(path, "wb");
		if (f) {
			blit();
			const uint8_t *fb = lcd_get_active_buffer();
			fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
			for (int i = 0; i < WIDTH * HEIGHT; i++) fwrite(frame_palette + fb[i] * 3, 1, 3, f);
			fclose(f);
		}
	}
	const char *state = getenv("HD_STATE");
	if (state && getenv("HD_SAVE_AT") && atoi(getenv("HD_SAVE_AT")) == test_tick)
		printf("hode: save %s\n", SaveState(state) ? "ok" : "FAILED");
	if (state && getenv("HD_LOAD_AT") && atoi(getenv("HD_LOAD_AT")) == test_tick)
		printf("hode: load %s\n", LoadState(state) ? "ok" : "FAILED");
	if (getenv("HD_TICKS") && test_tick >= atoi(getenv("HD_TICKS"))) {
		printf("hode: heap used %lu KB, peak %lu KB, total %lu KB\n", (unsigned long)(hd_heap_used() / 1024),
		       (unsigned long)(hd_heap_peak() / 1024), (unsigned long)(hd_heap_total() / 1024));
		exit(0);
	}
	test_tick++;
}
#endif

/* --- sound and time --- */

static void (*audio_proc)(void *, int16_t *, int);
static void *audio_user;
static int audio_locked;
static uint64_t samples_played;
static int16_t stereo[AUDIO_PERIOD * 2];

void hd_plat_set_audio(void (*proc)(void *, int16_t *, int), void *user)
{
	audio_proc = proc;
	audio_user = user;
}

void hd_plat_audio_lock(int lock)
{
	audio_locked += lock ? 1 : -1;
	if (audio_locked < 0) audio_locked = 0;
}

uint32_t hd_plat_time_ms(void) { return (uint32_t)(samples_played * 1000 / HD_SAMPLE_RATE); }

/* One sound period into the free half of the DMA buffer. It must be done
   within 20 ms of the previous sync (or the half plays again). */
static bool period_filled;

static void fill_period(void)
{
	memset(stereo, 0, sizeof stereo);
	if (audio_proc && !audio_locked) audio_proc(audio_user, stereo, AUDIO_PERIOD * 2);
	if (!common_emu_sound_loop_is_muted()) {
		int16_t *buffer = audio_get_active_buffer();
		uint16_t length = audio_get_buffer_length();
		if (buffer && length) {
			int volume = common_emu_sound_get_volume();
			for (uint16_t i = 0; i < length && i < AUDIO_PERIOD; i++)
				buffer[i] = (int16_t)(((stereo[2 * i] + stereo[2 * i + 1]) / 2) * volume / 255);
		}
	}
#ifdef HOST_BUILD
	/* HD_WAV=file: the sound (mono, 22050 Hz, raw s16) */
	static FILE *wav;
	if (!wav && getenv("HD_WAV")) wav = fopen(getenv("HD_WAV"), "wb");
	if (wav) {
		int16_t mono[AUDIO_PERIOD];
		for (int i = 0; i < AUDIO_PERIOD; i++) mono[i] = (int16_t)((stereo[2 * i] + stereo[2 * i + 1]) / 2);
		fwrite(mono, sizeof mono, 1, wav);
		fflush(wav);
	}
#endif
	samples_played += AUDIO_PERIOD;
	period_filled = true;
}

void hd_plat_service(void)
{
	wdog_refresh();
	read_buttons();
	if (restore_pending) restore_and_jump(); /* loaded from the pause menu */
#ifdef HOST_BUILD
	test_step();
#endif
	if (!period_filled) fill_period();
#ifndef HOST_BUILD
	/* every 5 s: game frames per second, time waiting for the audio
	   (idle), heap */
	static uint32_t stat_idle, stat_start, stat_periods;
	uint32_t t0 = HAL_GetTick();
	common_emu_sound_sync(false);
	period_filled = false;
	uint32_t t1 = HAL_GetTick();
	stat_idle += t1 - t0;
	if (++stat_periods == 1) stat_start = t0;
	if (stat_periods > 250) {
		uint32_t span = t1 - stat_start;
		if (!span) span = 1;
		printf("hode: %lu.%lu fps, idle %lu%%, heap %lu/%lu KB, peak %lu KB\n", (unsigned long)(stat_frames * 1000 / span),
		       (unsigned long)(stat_frames * 10000 / span % 10), (unsigned long)(stat_idle * 100 / span),
		       (unsigned long)(hd_heap_used() / 1024), (unsigned long)(hd_heap_total() / 1024),
		       (unsigned long)(hd_heap_peak() / 1024));
		stat_frames = stat_idle = stat_periods = 0;
	}
#else
	common_emu_sound_sync(false);
	period_filled = false;
#endif
}

/* During long work (a cutscene frame read from the SD card): keep the
   sound fed without waiting. */
void hd_plat_poll(void)
{
#ifndef HOST_BUILD
	wdog_refresh();
	if (!period_filled) fill_period();
	if (dma_counter != common_emu_sound_dma_marker) {
		/* the DMA moved on: what the sync would have returned */
		common_emu_sound_dma_marker = dma_counter;
		period_filled = false;
		fill_period();
	}
#endif
}

/* --- errors --- */

void hd_plat_log(const char *message)
{
	printf("hode: %s\n", message);
}

void hd_plat_fatal(const char *message)
{
	printf("hode: fatal: %s\n", message);
#ifdef HOST_BUILD
	printf("hode: heap used %lu KB of %lu KB, largest free %lu KB\n", (unsigned long)(hd_heap_used() / 1024),
	       (unsigned long)(hd_heap_total() / 1024), (unsigned long)(hd_heap_largest_free() / 1024));
	exit(1);
#else
	char line[64];
	/* black and white in the CLUT: the game's palette may not be set yet
	   (white text would map to black) */
	{
		static const uint32_t clut[2] = { 0x000000, 0xFFFFFF };
		lcd_set_clut(clut, 2);
	}
	lcd_wait_for_vblank();
	lcd_clear_buffers();
	int y = 48;
	y += odroid_overlay_draw_text(8, y, WIDTH - 16, "Heart of Darkness:", 0xFFFF, 0x0000) + 8;
	y += odroid_overlay_draw_text(8, y, WIDTH - 16, message, 0xFFFF, 0x0000) + 8;
	snprintf(line, sizeof line, "heap %lu/%lu KB, largest free %lu KB", (unsigned long)(hd_heap_used() / 1024),
	         (unsigned long)(hd_heap_total() / 1024), (unsigned long)(hd_heap_largest_free() / 1024));
	y += odroid_overlay_draw_text(8, y, WIDTH - 16, line, 0xFFFF, 0x0000) + 8;
	odroid_overlay_draw_text(8, y, WIDTH - 16, "Press a button: back to the menu", 0xFFFF, 0x0000);
	lcd_swap();
	bool released = false;
	while (1) {
		wdog_refresh();
		HAL_Delay(50);
		odroid_input_read_gamepad(&joystick);
		bool any = false;
		for (int i = 0; i < ODROID_INPUT_MAX; i++) any |= joystick.values[i] != 0;
		if (!any) released = true;
		else if (released) odroid_system_switch_app(0);
	}
#endif
}

/* --- system callbacks --- */

/*
 * Save states.
 *
 * Exact states: taken between two frames of a level (hd_safe_point, set by
 * Game::gnwLevelLoop() around its sleep), they hold the engine's variables
 * (the hdstate_* sections: hode, hd_fs, hd_system, hd_game, hd_heap) and
 * the heap (every block's header, the used blocks' bytes). They need the
 * same build, the same heap regions and the packs at the same addresses in
 * the external flash (pointers into them): else, and for states taken
 * elsewhere (cutscene, menus), the state restarts at its checkpoint (the
 * level, checkpoint, difficulty... are always in the header).
 *
 * A load is done from our own code, never from the firmware's menu: the
 * menu's LoadState() checks the file and leaves it pending; the next
 * service (or app_main at boot, "Resume game") restores the memory and
 * jumps to app_main (hd_top), which goes on with the level
 * (hd_game_continue()).
 */
#define STATE_MAGIC "HODS"
#define STATE_VERSION 2
#define STATE_PACKS 2

extern uint8_t __start_hdstate_data[], __stop_hdstate_data[];
extern uint8_t __start_hdstate_bss[], __stop_hdstate_bss[];

int hd_safe_point; /* Game::gnwLevelLoop(): between two frames */

typedef struct {
	char magic[4];
	uint32_t version;
	uint32_t level, checkpoint, difficulty, volume, cutscenes;
	uint32_t exact;
	uint32_t build, heap_sig, data_size, bss_size;
	struct {
		char name[8];
		uint64_t addr;
		uint32_t size, pad;
	} packs[STATE_PACKS];
} state_hdr_t;

static const char *const level_packs[] = { "rock", "fort", "pwr1", "isld", "lava", "pwr2", "lar1", "lar2", "dark" };

static jmp_buf hd_top;
static char restore_path[256];

static uint32_t build_id(void)
{
	return (uint32_t)(uintptr_t)&hd_game_continue ^ (uint32_t)(uintptr_t)&hd_game_run * 31u ^
	       (uint32_t)(__stop_hdstate_data - __start_hdstate_data) * 131u ^
	       (uint32_t)(__stop_hdstate_bss - __start_hdstate_bss) * 8191u;
}

static int file_write(const void *p, size_t n, void *ctx) { return fwrite(p, 1, n, (FILE *)ctx) != n; }
static int file_read(void *p, size_t n, void *ctx) { return fread(p, 1, n, (FILE *)ctx) != n; }

static bool read_header(const char *path, state_hdr_t *h)
{
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	memset(h, 0, sizeof *h);
	size_t n = fread(h, 1, sizeof *h, f);
	fclose(f);
	if (n < 28 || memcmp(h->magic, STATE_MAGIC, 4) != 0) return false;
	if (h->version == 1) {
		h->exact = 0; /* v1: the checkpoint only */
		return true;
	}
	return h->version == STATE_VERSION && n == sizeof *h;
}

/* Can this exact state be restored here? */
static bool exact_ok(const state_hdr_t *h)
{
	if (!h->exact || h->build != build_id() || h->heap_sig != hd_heap_signature() ||
	    h->data_size != (uint32_t)(__stop_hdstate_data - __start_hdstate_data) ||
	    h->bss_size != (uint32_t)(__stop_hdstate_bss - __start_hdstate_bss))
		return false;
	for (int i = 0; i < STATE_PACKS; i++) {
		if (!h->packs[i].name[0]) continue;
		char name[9];
		memcpy(name, h->packs[i].name, 8);
		name[8] = 0;
		uint32_t size = 0;
		const uint8_t *p = hd_plat_pack(name, &size);
		if ((uint64_t)(uintptr_t)p != h->packs[i].addr || size != h->packs[i].size) {
			printf("hode: state: pack %s moved in the flash, restarting at the checkpoint\n", name);
			return false;
		}
	}
	return true;
}

static bool LoadState(const char *path)
{
	state_hdr_t h;
	if (!read_header(path, &h)) return false;
	if (exact_ok(&h)) {
		snprintf(restore_path, sizeof restore_path, "%s", path);
		restore_pending = true;
		return true;
	}
	hd_state_t st = { (int)h.level, (int)h.checkpoint, (int)h.difficulty, (int)h.volume, h.cutscenes };
	return hd_game_set_state(&st) != 0;
}

static bool SaveState(const char *path)
{
	hd_state_t st;
	if (!hd_game_get_state(&st)) return false;
	state_hdr_t h;
	memset(&h, 0, sizeof h);
	memcpy(h.magic, STATE_MAGIC, 4);
	h.version = STATE_VERSION;
	h.level = st.level;
	h.checkpoint = st.checkpoint;
	h.difficulty = st.difficulty;
	h.volume = st.volume;
	h.cutscenes = st.cutscenes;
	h.exact = hd_safe_point != 0;
	h.build = build_id();
	h.heap_sig = hd_heap_signature();
	h.data_size = (uint32_t)(__stop_hdstate_data - __start_hdstate_data);
	h.bss_size = (uint32_t)(__stop_hdstate_bss - __start_hdstate_bss);
	const char *names[STATE_PACKS] = { "setup", (st.level >= 0 && st.level < 9) ? level_packs[st.level] : "" };
	for (int i = 0; i < STATE_PACKS; i++) {
		uint32_t size = 0;
		const uint8_t *p = names[i][0] ? hd_plat_pack(names[i], &size) : NULL;
		if (!p) continue;
		strncpy(h.packs[i].name, names[i], sizeof h.packs[i].name);
		h.packs[i].addr = (uint64_t)(uintptr_t)p;
		h.packs[i].size = size;
	}
	FILE *f = fopen(path, "wb");
	if (!f) return false;
	bool ok = fwrite(&h, 1, sizeof h, f) == sizeof h;
	if (ok && h.exact) {
		ok = fwrite(__start_hdstate_data, 1, h.data_size, f) == h.data_size &&
		     fwrite(__start_hdstate_bss, 1, h.bss_size, f) == h.bss_size &&
		     hd_heap_save(file_write, f) == 0;
	}
	fclose(f);
	printf("hode: state saved (%s)\n", h.exact ? "exact" : "checkpoint");
	return ok;
}

/* The pending exact state: memory restored, then on to app_main. */
static void restore_and_jump(void)
{
	restore_pending = false;
	FILE *f = fopen(restore_path, "rb");
	if (!f) hd_plat_fatal("save state: cannot open the file again");
	state_hdr_t h;
	hd_fs_close_files();
	bool ok = fread(&h, 1, sizeof h, f) == sizeof h &&
	          fread(__start_hdstate_data, 1, h.data_size, f) == h.data_size &&
	          fread(__start_hdstate_bss, 1, h.bss_size, f) == h.bss_size &&
	          hd_heap_load(file_read, f) == 0;
	fclose(f);
	if (!ok) hd_plat_fatal("save state: the file is incomplete (memory lost)");
	hd_fs_reopen_files();
	audio_locked = 0;
	hd_safe_point = 0;
	printf("hode: state loaded (exact)\n");
	longjmp(hd_top, 1);
}

static void *Screenshot(void)
{
	lcd_wait_for_vblank();
	blit();
	return lcd_get_active_buffer();
}

static void Shutdown(void) {}

static void SleepWake(void)
{
	odroid_audio_init(HD_SAMPLE_RATE);
	audio_clear_buffers();
	audio_start_playing(AUDIO_PERIOD);
}

static void SramSave(void) {}

/* --- main --- */

void app_main(uint8_t load_state, uint8_t start_paused, int8_t save_slot)
{
#ifndef HOST_BUILD
	gw_core_bridge_init();
#endif
	if (start_paused) {
		common_emu_state.pause_after_frames = 2;
		odroid_audio_mute(true);
	} else {
		common_emu_state.pause_after_frames = 0;
	}
	common_emu_state.frame_time_10us = 2000;
	odroid_system_init(APP_ID, HD_SAMPLE_RATE);
	odroid_system_emu_init(&LoadState, &SaveState, &Screenshot, &Shutdown, &SleepWake, &SramSave, NULL);
	/* 8-bit picture: the game's palette in the CLUT, and 150 KB of the LCD
	   pool for the heap */
	lcd_setup_framebuffers(LCD_MODE_LUT8);
	lcd_clear_buffers();

	for (int x = 0; x < WIDTH; x++) xmap[x] = (uint16_t)(x * HD_W / WIDTH);
	for (int y = 0; y < HEIGHT; y++) ymap[y] = (uint8_t)(y * HD_H / HEIGHT);

	/* the packs: "/homebrews/HeartOfDarkness.bin" -> "/homebrews/HeartOfDarkness/" */
	if (!ACTIVE_FILE || !ACTIVE_FILE->path[0]) hd_plat_fatal("no path");
	snprintf(data_dir, sizeof data_dir, "%s", ACTIVE_FILE->path);
	char *dot = strrchr(data_dir, '.');
	if (dot) *dot = 0;
	odroid_sdcard_mkdir(save_dir);

	/* the heap: what is left of RAM_EMU, and DTCM */
	size_t ram = ram_get_free_size();
#ifdef HOST_BUILD
	/* the console's: RAM_EMU left after the code, and DTCM */
	size_t want = (getenv("HD_HEAP_KB") ? atoi(getenv("HD_HEAP_KB")) : 717) * 1024 + RAM_MARGIN;
	if (ram > want) ram = want;
#endif
	if (ram > RAM_MARGIN) {
		ram -= RAM_MARGIN;
		hd_heap_add(ram_malloc(ram), ram);
	}
	uint8_t *bonus = NULL;
	size_t bonus_size = 0;
	lcd_get_bonus_pool(&bonus, &bonus_size);
	if (bonus && bonus_size) hd_heap_add(bonus, bonus_size);
#ifndef HOST_BUILD
	size_t dtcm = dtc_get_free_size();
	if (dtcm > 4096) {
		dtcm -= 1024;
		hd_heap_add(dtc_malloc(dtcm), dtcm);
	}
	/* ITCM (unused: no code there) and the AHB heap, minus a margin for the
	   firmware (pause menu, files) */
	size_t itcm = itc_get_free_size();
	if (itcm > 8192) {
		itcm -= 1024;
		void *p = itc_malloc(itcm);
		if (p != (void *)0xffffffff) hd_heap_add(p, itcm);
	}
	size_t ahb = ahb_get_free_size();
	for (ahb = ahb > AHB_MARGIN ? ahb - AHB_MARGIN : 0; ahb >= 8192; ahb -= 4096) {
		void *p = ahb_malloc(ahb);
		if (p) {
			hd_heap_add(p, ahb);
			break;
		}
	}
	printf("hode: dtcm %u, itcm %u, ahb %u bytes\n", (unsigned)dtcm, (unsigned)itcm, (unsigned)ahb);
#endif
	printf("hode: heap %lu KB\n", (unsigned long)(hd_heap_total() / 1024));

	audio_start_playing(AUDIO_PERIOD);
	/* "Resume game": an exact state is restored below, a checkpoint state
	   is pending (hd_game_run() starts there) */
	/* the packs (tools/hod_pack.py, made from the player's CD) */
	{
		uint32_t n = 0;
		const uint8_t *setup = hd_plat_pack("setup", &n);
		if (!setup || n < 16 || memcmp(setup, "HODP", 4) != 0) {
			static char msg[400];
			snprintf(msg, sizeof msg, "%s/setup.hod is missing or not a pack. Make the packs from your CD with tools/hod_pack.py (INSTALL.md).", data_dir);
			hd_plat_fatal(msg);
		}
	}
	/* an exact save state loaded: the level goes on from here */
	if (setjmp(hd_top)) {
		hd_game_continue();
		odroid_system_switch_app(0);
		return;
	}
	if (load_state) odroid_system_emu_load_state(save_slot);
	if (restore_pending) restore_and_jump();
#ifdef HD_DEBUG_LEVEL
	/* debug builds (make HD_DEBUG_LEVEL=1): <data folder>/level.txt =
	   "level checkpoint" starts there without the menu */
	{
		char path[320];
		snprintf(path, sizeof path, "%s/level.txt", data_dir);
		FILE *f = fopen(path, "rb");
		if (f) {
			char line[16] = {0};
			fread(line, 1, sizeof line - 1, f);
			fclose(f);
			int v[2] = {0, 0}, n = 0;
			for (const char *c = line; *c && n < 2; c++) {
				if (*c >= '0' && *c <= '9') v[n] = v[n] * 10 + (*c - '0');
				else if (*c == ' ') n++;
			}
			hd_game_run(v[0], v[1]);
			odroid_system_switch_app(0);
		}
	}
#endif
#ifdef HOST_BUILD
	/* HD_LEVEL=level[:checkpoint]: start there without the menu */
	if (getenv("HD_LEVEL")) {
		const char *l = getenv("HD_LEVEL");
		const char *c = strchr(l, ':');
		hd_game_run(atoi(l), c ? atoi(c + 1) : 0);
	} else
#endif
	hd_game_run(-1, 0);
	odroid_system_switch_app(0);
}
