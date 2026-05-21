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

/* Render resolution. The Cronopio screen is 320x240 (4:3, square pixels). DOOM
 * renders into a DOOM_W x DOOM_H indexed buffer; plat_present maps it onto the
 * full screen at 4:3 (no letterbox). Two modes, selectable via DOOM_H:
 *
 *   B (DOOM_H = 200, default): vanilla render height. plat_present scales x1.2
 *     vertically (200 -> 240, duplicating one row in six). This reproduces the
 *     exact 4:3 look of DOOM on a CRT (where 320x200 was stretched to fill the
 *     screen) and is the cheaper hot path (~17% less rasterising).
 *   C (DOOM_H = 240): native render height — Crispy renders 240 rows for ~20%
 *     more vertical FOV. plat_present copies 1:1. Costs +20% rasterising and
 *     touches the renderer/HUD.
 *
 * (cvm-cc has no -D passthrough yet, so this is a compile-time #define rather
 * than a build flag for now.) */
#define DOOM_W       320
#ifndef DOOM_H
#define DOOM_H       200
#endif

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
