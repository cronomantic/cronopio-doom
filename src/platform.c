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

    /* top letterbox bar */
    for (int i = 0; i < DOOM_FB_TOP * CRON_SCREEN_W; ++i) fb[i] = 0;

    /* the 320x200 image, row by row */
    for (int y = 0; y < DOOM_H; ++y) {
        volatile uint8_t *row = fb + (DOOM_FB_TOP + y) * CRON_SCREEN_W;
        const uint8_t    *src = indexed + y * DOOM_W;
        for (int x = 0; x < DOOM_W; ++x) row[x] = src[x];
    }

    /* bottom letterbox bar */
    for (int i = (DOOM_FB_TOP + DOOM_H) * CRON_SCREEN_W;
         i < CRON_SCREEN_W * CRON_SCREEN_H; ++i) fb[i] = 0;
}

int plat_audio_push(const int16_t *stereo, int frames) {
    return cron_stream(stereo, frames);
}
