/* Placeholder engine: proves the Cronopio integration (framebuffer, palette,
 * present loop, build) works before any DOOM code is added. Replace this file
 * with your chosen port's binding — see engine.h and KICKOFF.md.
 *
 * It renders an animated plasma into a grayscale-ramped palette so you can see
 * the cart is alive and the letterboxing is correct. */
#include "engine.h"
#include "platform.h"

static uint8_t  fb[DOOM_W * DOOM_H];
static uint8_t  pal[256 * 3];
static uint32_t tic;

void engine_init(void) {
    plat_log("[engine stub] no DOOM source linked yet — see KICKOFF.md\n");
    for (int i = 0; i < 256; ++i) {       /* simple R/G/B-ish ramp */
        pal[i * 3 + 0] = (uint8_t)i;
        pal[i * 3 + 1] = (uint8_t)((i * 2) & 0xFF);
        pal[i * 3 + 2] = (uint8_t)(255 - i);
    }
}

void engine_input(uint32_t pad) { (void)pad; }

void engine_tick(void) {
    for (int y = 0; y < DOOM_H; ++y)
        for (int x = 0; x < DOOM_W; ++x)
            fb[y * DOOM_W + x] = (uint8_t)((x + y + (tic * 2)) & 0xFF);
    ++tic;
}

const uint8_t *engine_framebuffer(void) { return fb; }
const uint8_t *engine_palette(void)     { return pal; }
