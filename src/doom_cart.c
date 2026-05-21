/* cronopio-doom — the cartridge entry point.
 *
 * Multi-file build (cvm-cc links many .c via llvm-link), so this is no longer a
 * unity TU: the platform layer, engine binding, DOOM sources and SDK libc are
 * separate inputs passed to cronopio-cc (see build_doom.sh). This file only
 * wires the Cronopio frame callback to the engine seam (engine.h).
 */
#include <cronopio.h>
#include "platform.h"
#include "engine.h"

static void setup(void)
{
    plat_init();
    engine_init();
}

static void frame(void)
{
    engine_input(cron_pad(0));
    engine_tick();
    plat_present(engine_framebuffer(), engine_palette());
}

CRONOPIO_CART_INIT(setup, frame)
