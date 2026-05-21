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

#if DOOM_H == CRON_SCREEN_H
    /* Mode C: native 320x240 — straight 1:1 copy. */
    for (int y = 0; y < CRON_SCREEN_H; ++y) {
        volatile uint8_t *row = fb + y * CRON_SCREEN_W;
        const uint8_t    *src = indexed + y * DOOM_W;
        for (int x = 0; x < DOOM_W; ++x) row[x] = src[x];
    }
#else
    /* Mode B: render is DOOM_H tall (e.g. 200) — scale vertically to fill the
     * full screen height (4:3). Each output row samples source row y*DOOM_H/H
     * (nearest), which for 200->240 duplicates one row in six. No black bars. */
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
