/* What hode's Game & Watch port needs from the platform (hd_main.c: the
   console, and the PC build through the SDK's host harness). */
#ifndef HD_PLAT_H
#define HD_PLAT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HD_W 256
#define HD_H 192
#define HD_SAMPLE_RATE 22050

/* buttons (hd_plat_buttons) */
#define HD_UP     (1 << 0)
#define HD_RIGHT  (1 << 1)
#define HD_DOWN   (1 << 2)
#define HD_LEFT   (1 << 3)
#define HD_RUN    (1 << 4)
#define HD_JUMP   (1 << 5)
#define HD_SHOOT  (1 << 6)
#define HD_ESC    (1 << 7)

/* A pack ("setup", "rock", ...: <data dir>/<name>.hod) in memory, read in
   place; 0 if missing. Stays valid for the session. */
const uint8_t *hd_plat_pack(const char *name, uint32_t *size);

/* Folders: the packs and hod.paf; setup.cfg. */
const char *hd_plat_data_dir(void);
const char *hd_plat_save_dir(void);

/* The game's picture: 256 x 192, 8-bit (pitch bytes per line, 0: the same
   line repeated), palette 256 x RGB (8 bits), moved by (dx, dy) when the
   screen shakes. Shown now (after the previous swap). The pixels are the
   engine's buffer: they must stay as they are until the next present
   (they are drawn again behind the pause menu). */
void hd_plat_present(const uint8_t *pixels, int pitch, const uint8_t *palette, int dx, int dy);

/* Buttons held (HD_*), read at the last service. */
unsigned hd_plat_buttons(void);

/* Milliseconds of sound played: the game's clock. */
uint32_t hd_plat_time_ms(void);

/* One audio period (DMA half, ~20 ms): sound, buttons, Retro-Go menu,
   picture. Blocks until the period. */
void hd_plat_service(void);
/* Keep the sound fed during long work (no waiting): cutscene reads. */
void hd_plat_poll(void);

/* The game's mixer: 22050 Hz stereo, len = int16 values. Not called while
   locked (hd_plat_audio_lock). */
void hd_plat_set_audio(void (*proc)(void *user, int16_t *stream, int len), void *user);
void hd_plat_audio_lock(int lock);

/* Errors */
void hd_plat_fatal(const char *message);
void hd_plat_log(const char *message);

#ifdef __cplusplus
}
#endif

#endif
