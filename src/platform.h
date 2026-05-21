/* Cronopio platform layer for the DOOM port.
 *
 * The seam between a DOOM source port and the Cronopio fantasy console. The
 * engine binding (engine.c / a port's I_* implementation) talks to these
 * functions; they are the only place that touches Cronopio syscalls for
 * video/audio/time/log. Keeping them here means swapping the engine in does
 * not scatter syscall calls across the tree.
 */
#ifndef CRONO_DOOM_PLATFORM_H
#define CRONO_DOOM_PLATFORM_H

#include <cronopio.h>
#include <stdint.h>

/* DOOM renders 320x200; the Cronopio screen is 320x240, so we letterbox with
 * 20-row black bars top and bottom. */
#define DOOM_W       320
#define DOOM_H       200
#define DOOM_FB_TOP  ((CRON_SCREEN_H - DOOM_H) / 2)   /* = 20 */

void     plat_init(void);
void     plat_log(const char *s);                      /* NUL-terminated -> cron_log */
uint32_t plat_time_ms(void);

/* Present a DOOM_W*DOOM_H 8bpp indexed frame and (optionally) a 256-entry
 * RGB palette (768 bytes; NULL keeps the current palette) to the Cronopio
 * framebuffer, letterboxed vertically. */
void     plat_present(const uint8_t *indexed, const uint8_t *palette_rgb);

/* Queue 16-bit signed stereo frames (interleaved L,R) to the host audio ring.
 * Returns frames accepted. See cron_stream / cron_stream_free. */
int      plat_audio_push(const int16_t *stereo, int frames);

#endif /* CRONO_DOOM_PLATFORM_H */
