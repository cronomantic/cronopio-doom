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

/* --- TEMP: milestone-2 WAD-in-ROM probe ---------------------------------
 * Validates the cartridge-ROM path before the real engine lands: read the
 * WAD header baked in with `cronopio-cc --rom=<wad>` and log its magic +
 * lump count. Remove once Crispy's W_InitMultipleFiles reads the ROM itself. */
static void wad_probe(void) {
    const uint8_t *rom = cron_rom();
    uint32_t       len = cron_rom_size();
    if (!rom || len < 12) {
        plat_log("[wad] no ROM baked in (build with --rom=<wad>)\n");
        return;
    }
    /* WAD header: char id[4] ("IWAD"/"PWAD"), int32 numlumps, int32 dirofs (LE). */
    char id[5];
    id[0] = (char)rom[0]; id[1] = (char)rom[1];
    id[2] = (char)rom[2]; id[3] = (char)rom[3]; id[4] = 0;
    int32_t numlumps = (int32_t)((uint32_t)rom[4]        | ((uint32_t)rom[5] << 8) |
                                 ((uint32_t)rom[6] << 16) | ((uint32_t)rom[7] << 24));
    plat_log("[wad] ");
    plat_log(id);
    plat_log("\n");
    cron_trace_i32(0x4C50 /*'LP'*/, numlumps);   /* lump count -> stderr trace */
}

static void setup(void) {
    plat_init();
    wad_probe();
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
