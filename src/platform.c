/* Implementation of the Cronopio platform layer. Compiled as part of the
 * single cartridge translation unit (see doom_cart.c) — not built standalone. */
#include "platform.h"

void plat_log(const char *s) {
    int n = 0;
    while (s[n]) ++n;
    cron_log(s, n);
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
