/* Implementation of the Cronopio platform layer. Compiled as part of the
 * single cartridge translation unit (see doom_cart.c) — not built standalone. */
#include "platform.h"

/* ---- loading screen ---------------------------------------------------- */
/* The engine's blocking startup (D_DoomMain: WAD load, R_Init, P_Init, ...)
 * runs inside the cart entry, before the frame loop. We draw a splash + a
 * progress bar straight into the Cronopio framebuffer and cron_present() it;
 * the host's present hook flushes mid-entry. The bar is driven by the volume
 * of startup log lines (no known total) via an asymptotic fill, throttled to
 * present only when the filled width actually changes. */
#define BOOT_BAR_X   40
#define BOOT_BAR_Y   188
#define BOOT_BAR_W   240
#define BOOT_BAR_H   10
#define BOOT_COL_BG   0      /* palette indices we set explicitly below */
#define BOOT_COL_TRK  1
#define BOOT_COL_BAR  2
#define BOOT_COL_TXT  3

static int g_booting;
static int g_boot_lines;

/* Full redraw + present. The engine overwrites the whole palette during boot
 * (V_Init / I_SetPalette), so we re-assert our four palette entries every time,
 * right before presenting — the draw is synchronous, so colours are correct at
 * present. The bar is driven by startup-log volume (no known total), an
 * asymptotic fill that never quite reaches the end until the game takes over. */
static void boot_draw(void) {
    cron_palette_set(BOOT_COL_BG,  0x000000);
    cron_palette_set(BOOT_COL_TRK, 0x303030);
    cron_palette_set(BOOT_COL_BAR, 0xB81818);   /* DOOM-ish red */
    cron_palette_set(BOOT_COL_TXT, 0xC8C8C8);

    cron_cls(BOOT_COL_BG);
    cron_text("CRONOPIO-DOOM", 13, (CRON_SCREEN_W - 13 * 8) / 2, 96,  BOOT_COL_TXT);
    cron_text("LOADING",        7, (CRON_SCREEN_W -  7 * 8) / 2, 168, BOOT_COL_TXT);

    int inner = BOOT_BAR_W - 4;
    int w = inner * g_boot_lines / (g_boot_lines + 8);    /* asymptotic */
    cron_rect(BOOT_BAR_X, BOOT_BAR_Y, BOOT_BAR_W, BOOT_BAR_H, BOOT_COL_TRK);
    if (w > 0) cron_rect(BOOT_BAR_X + 2, BOOT_BAR_Y + 2, w, BOOT_BAR_H - 4, BOOT_COL_BAR);
    cron_present();
}

void plat_boot_begin(void) {
    g_booting    = 1;
    g_boot_lines = 0;
    boot_draw();
}

void plat_boot_end(void) {
    g_booting = 0;
}

void plat_log(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    cron_log(s, n);
    if (g_booting) { ++g_boot_lines; boot_draw(); }
}

uint32_t plat_time_ms(void) {
    return cron_time_ms();
}

void plat_init(void) {
    plat_log("[cronopio-doom] platform up\n");
    /* Audio starts silent until the engine pushes a stream. */
}

uint8_t *plat_framebuffer(void) {
    return (uint8_t *)CRON_FB;
}

void plat_present(const uint8_t *indexed, const uint8_t *palette_rgb) {
    if (palette_rgb) {
        for (int i = 0; i < 256; ++i) {
            uint32_t r = palette_rgb[i * 3 + 0];
            uint32_t g = palette_rgb[i * 3 + 1];
            uint32_t b = palette_rgb[i * 3 + 2];
            CRON_PAL[i] = (r << 16) | (g << 8) | b;
        }
    }

    volatile uint8_t *fb = CRON_FB;

#ifdef CRON_ACCEL
    /* Accelerated mode: the engine rendered the 320x200 frame straight into
     * CRON_FB, vertically centered at rows [CRON_VIEW_Y, CRON_VIEW_Y+DOOM_H).
     * Present is 1:1 (no rescale -> no mutation of the persistent buffer); just
     * paint the top and bottom letterbox black so the centered frame is clean. */
    (void)indexed;
    for (int y = 0; y < CRON_VIEW_Y; ++y) {
        volatile uint8_t *row = fb + y * CRON_SCREEN_W;
        for (int x = 0; x < CRON_SCREEN_W; ++x) row[x] = 0;
    }
    for (int y = CRON_VIEW_Y + DOOM_H; y < CRON_SCREEN_H; ++y) {
        volatile uint8_t *row = fb + y * CRON_SCREEN_W;
        for (int x = 0; x < CRON_SCREEN_W; ++x) row[x] = 0;
    }
#else
    /* Mode B: render is DOOM_H tall in a private buffer — scale vertically to
     * fill 240 (4:3), duplicating one row in six. */
    for (int y = 0; y < CRON_SCREEN_H; ++y) {
        int               sy  = (y * DOOM_H) / CRON_SCREEN_H;
        volatile uint8_t *row = fb + y * CRON_SCREEN_W;
        const uint8_t    *src = indexed + sy * DOOM_W;
        for (int x = 0; x < DOOM_W; ++x) row[x] = src[x];
    }
#endif
}

int plat_audio_push(const int16_t *stereo, int frames) {
    return cron_stream(stereo, frames);
}
