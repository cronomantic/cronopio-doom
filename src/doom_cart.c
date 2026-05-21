/* cronopio-doom — the cartridge.
 *
 * cvm-cc compiles ONE translation unit per cartridge (there is no link step),
 * so this is a unity build: the platform layer, the WAD-ROM shim and the
 * engine binding are #included here and compiled together. When you add a
 * real DOOM source port, replace the engine_stub.c include with your port's
 * amalgamation + binding (see KICKOFF.md, "Wiring the engine").
 */
#include <cronopio.h>
#include "platform.h"
#include "engine.h"

/* ---- unity includes (implementation lives in these .c files) ---------- */
#include "platform.c"
#include "wad_rom.c"
#include "engine_stub.c"     /* <-- swap for the real engine binding */

static void setup(void) {
    plat_init();
    engine_init();
}

static void frame(void) {
    engine_input(cron_pad(0));
    engine_tick();
    plat_present(engine_framebuffer(), engine_palette());

    /* Liveness overlay — drawn straight to the framebuffer over the image.
     * Remove once the real engine renders its own HUD. */
    static const char tag[] = "CRONOPIO DOOM (engine stub)";
    cron_text(tag, (int32_t)sizeof(tag) - 1, 8, 8, 4);
}

CRONOPIO_CART_INIT(setup, frame)
