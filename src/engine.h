/* The engine seam.
 *
 * doom_cart.c drives these five functions every frame; they are implemented
 * either by engine_stub.c (the placeholder this skeleton ships with) or, once
 * you drop a DOOM source port in, by a thin binding that forwards to it:
 *
 *   - PureDOOM:  engine_init -> doom_init(); engine_tick -> doom_update();
 *                engine_framebuffer -> a patched 8bpp getter (see KICKOFF.md);
 *                engine_input -> doom_key_down/up + doom_button_*.
 *   - Crispy/PrBoom+: engine_* call the port's D_DoomLoop step / I_* layer,
 *                with I_video/I_sound reimplemented over platform.h.
 *
 * The contract is deliberately tiny so the binding stays in one file.
 */
#ifndef CRONO_DOOM_ENGINE_H
#define CRONO_DOOM_ENGINE_H

#include <stdint.h>

/* Boot: initialise the engine and load the WAD from ROM (via wad_rom). */
void engine_init(void);

/* Advance the game by one frame and render into the engine's 320x200 buffer. */
void engine_tick(void);

/* The current 320x200 8bpp indexed frame. */
const uint8_t *engine_framebuffer(void);

/* The current 256-entry RGB palette (768 bytes), or NULL to keep the last. */
const uint8_t *engine_palette(void);

/* Feed this frame's input. `pad` is the Cronopio gamepad bitmask (CRON_BTN_*);
 * the binding may also poll cron_key(scancode) directly for keyboard play. */
void engine_input(uint32_t pad);

#endif /* CRONO_DOOM_ENGINE_H */
