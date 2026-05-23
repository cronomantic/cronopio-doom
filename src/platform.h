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

/* Render resolution. The Cronopio screen is 320x240 (4:3, square pixels).
 *
 * ACCELERATED layout (CRON_ACCEL): a 320x200 fullscreen 3D view stacked on top
 * of the 320x32 status bar (HUD), both drawn at x1 (undeformed), centered in
 * 320x240 with a 4px black border top and bottom:
 *
 *     rows   0..3    black border (CRON_VIEW_Y = 4)
 *     rows   4..203  3D view        (320x200, viewheight=200)
 *     rows 204..235  status bar/HUD (320x32, x1)
 *     rows 236..239  black border
 *
 * So DOOM renders DOOM_H = 232 rows (200 view + 32 bar) into CRON_FB at a +4 row
 * offset; SCREENHEIGHT is 232 so screenblocks 10 gives viewheight = 232-32 = 200
 * and the bar lands at rows 200..231 (DOOM space). plat_present is 1:1 (no
 * rescale -> no mutation) and paints the 4px borders. The HUD is x1 because we
 * pin V_Init's patch-scaling dx/dy to 1 (see v_video.c) and reanchor ST_Y to
 * SCREENHEIGHT-ST_HEIGHT (see st_stuff.h).
 *
 * Undefine CRON_ACCEL for classic mode B: a private 320x200 buffer that
 * plat_present stretches x1.2 to fill 240 (pixel-faithful CRT 4:3, no GPU
 * accelerators). */
#define CRON_ACCEL   1

#define DOOM_W       320
#ifndef DOOM_H
#ifdef CRON_ACCEL
#define DOOM_H       232        /* 200 view + 32 status bar */
#else
#define DOOM_H       200        /* mode B: stretched to 240 */
#endif
#endif

/* Black border that centers the DOOM_H-tall frame in 320x240 (= 4 rows). */
#define CRON_VIEW_Y  ((CRON_SCREEN_H - DOOM_H) / 2)

void     plat_init(void);
void     plat_log(const char *s);                      /* NUL-terminated -> cron_log */
uint32_t plat_time_ms(void);

/* Loading screen. plat_boot_begin() draws an initial splash; while booting,
 * plat_log() advances a progress bar (the engine's startup log is the progress
 * source) and flushes via cron_present. plat_boot_end() leaves booting mode so
 * the game's frames take over. Wrap the blocking engine init between them. */
void     plat_boot_begin(void);
void     plat_boot_end(void);

/* The Cronopio framebuffer region (CRON_FB), resolved at cart init. The engine
 * renders DOOM_W*DOOM_H directly into it (I_VideoBuffer aliases this) so the
 * GPU rasteriser accelerators (cron_tcol/tspan, which write CRON_FB) and the
 * C-side patch drawing compose into one buffer. NULL before cron_resolve_video. */
uint8_t *plat_framebuffer(void);

/* Present a DOOM_W*DOOM_H 8bpp indexed frame and (optionally) a 256-entry
 * RGB palette (768 bytes; NULL keeps the current palette) to the Cronopio
 * framebuffer, letterboxed vertically. */
void     plat_present(const uint8_t *indexed, const uint8_t *palette_rgb);

/* Queue 16-bit signed stereo frames (interleaved L,R) to the host audio ring.
 * Returns frames accepted. See cron_stream / cron_stream_free. */
int      plat_audio_push(const int16_t *stereo, int frames);

#endif /* CRONO_DOOM_PLATFORM_H */
