# cronopio-doom — kickoff brief

> Paste this as the first message of the new conversation. It carries the full
> context decided while building the Cronopio SDK so the port work can start
> cold. The Cronopio side is ready; what remains is engine-side glue.

## Goal

Port DOOM to the **Cronopio** fantasy console. A cartridge is a CronoVM `.bin`
compiled from C with `cronopio-cc`. North star: DOOM runs at a playable rate.

## Status

- This repo is a **skeleton that already builds and runs** a placeholder
  "engine stub" (animated test pattern). It validates the Cronopio integration
  before any DOOM code lands.
- **The source port is not yet chosen.** Leaning decision below.

## Choosing the port

The priority is **limit-removing** (no vanilla static-array overflows:
visplanes, drawsegs, openings, savegame buffer, big-map BSP indices).

| Port | Limit-removing? | Build fit (cvm-cc = 1 TU) | Notes |
| --- | --- | --- | --- |
| **Crispy Doom** | ✅ headline feature | ✗ many files → needs amalgamation | vanilla-accurate; strip SDL/mixer/net, force 320x200 8bpp (hi-res off, no truecolor) |
| **PrBoom+ / DSDA** | ✅ + Boom/MBF + extended nodes | ✗ many files → amalgamation | most capable for modern PWADs; heaviest, most modern C |
| **PureDOOM** | ❌ vanilla limits | ✅ single header, compiles as-is | cleanest embed (all I/O via callbacks); you'd add limit-removing yourself |
| doomgeneric/Chocolate | ❌ (faithful to limits) | ✗ many files | Chocolate base; not limit-removing by design |

**Decision to make first:** limit-removing for *vanilla-style* big maps →
**Crispy**; also need *modern Boom/MBF PWADs + ZDBSP nodes* → **PrBoom+/DSDA**.
If embed simplicity outweighs limit-removing, **PureDOOM** + hand-rolled limit
patches is the least toolchain friction.

## The Cronopio contract (what the platform gives you)

Cartridge depends on the SDK via `find_package(Cronopio)` +
`cronopio_add_cartridge`. Memory map: an 8bpp indexed framebuffer region `fb`
(320x240) and a 256-entry palette region `pal`, resolved by the
`CRONOPIO_CART_INIT` macro into `CRON_FB` / `CRON_PAL`.

Syscalls relevant to DOOM (full list in the Cronopio repo `docs/syscalls.md`):

| Need | Cronopio | Used by platform.c |
| --- | --- | --- |
| log / print | `cron_log(msg,len)` | `plat_log` |
| time | `cron_time_ms()` | `plat_time_ms` |
| exit | `cron_exit(status)` | (wire I_Quit) |
| input (pad) | `cron_pad(0)` → CRON_BTN_* | `engine_input` |
| input (keyboard) | `cron_key(scancode)` 0..255 | binding polls directly |
| framebuffer | write `CRON_FB` (8bpp) | `plat_present` |
| palette | write `CRON_PAL[i]=0x00RRGGBB` | `plat_present` |
| WAD | `cron_rom()` / `cron_rom_size()` | `wad_rom.c` |
| SFX (DMX) | `cron_sample_u8(...)` (unsigned 8-bit) | TODO |
| music/mixed audio | `cron_stream(int16 stereo, n)` | `plat_audio_push` |
| column raster | `cron_tcol(...)` | TODO (R_DrawColumn) |
| span raster | `cron_tspan(...)` | TODO (R_DrawSpan) |
| heap | `cvm_alloc.h` (cvm_malloc/free over the heap) | wire Z_Malloc base |

## Hard constraints / gotchas (these bit us; plan for them)

1. **One translation unit per cartridge.** `cvm-cc` runs clang `-emit-llvm` on a
   single `.c`, then translates — there is **no linker**. Multi-file ports
   (Crispy/PrBoom+) need a **unity/amalgamation** build: one `.c` that
   `#include`s all engine sources. Watch for `static` symbols with the same
   name in different files colliding. (Alternative: teach `cvm-cc` to
   `llvm-link` multiple `.bc` — a change in the Cronopio/CronoVM repo, which you
   own, if amalgamation gets painful.)
2. **No `int64`/`double` in the ISA.** The translator rejects i64/f64 SSA.
   - `FixedMul`/`FixedDiv` (`(int64_t)a*b>>16`) → use the runtime intrinsics
     `cvm_qmul_16_16` / `cvm_int64.h` (MULH/MULHU under the hood). Any other
     site where clang emits i64 must be rewritten similarly.
   - `float` (f32) is first-class; `double` must become `float` or use
     `cvm_float64.h`. DOOM is mostly fixed-point, so this is light.
3. **No stdio / no setjmp / no filesystem.** Route printing to `cron_log`;
   replace `I_Error`/quit-via-longjmp with `cron_exit`; read the WAD from ROM
   via `wad_rom.c`. (PureDOOM gives all of these as callbacks for free.)
4. **8bpp + palette, 320x200 letterboxed into 320x240.** Keep DOOM's indexed
   pipeline end-to-end; set `CRON_PAL` from PLAYPAL (+ colormap for palette
   flashes). Do **not** convert to RGBA — it wastes the interpreter's cycles
   and throws away the `tcol`/`tspan` fast path. (PureDOOM's
   `doom_get_framebuffer` returns RGBA; patch its `I_FinishUpdate` to expose the
   8bpp screen + palette instead.)
5. **Performance is the open risk.** It's an interpreter; DOOM's software
   renderer is the hot path. `cron_tcol` (vertical textured column w/ colormap)
   and `cron_tspan` (horizontal textured span) are host-native accelerators
   built to back `R_DrawColumn` / `R_DrawSpan`. Wiring them is the single most
   important perf lever — do it early and measure.

## Wiring the engine (replace engine_stub.c)

The seam is `engine.h` (5 functions). Per port:

- **PureDOOM**: in a binding `.c`, `#define DOOM_IMPLEMENTATION` and
  `#include "PureDOOM.h"`; set callbacks (`doom_set_print` → `plat_log`,
  `doom_set_malloc` → cvm_alloc, `doom_set_file_io` → `wad_*`,
  `doom_set_gettime` → `plat_time_ms`, `doom_set_exit` → `cron_exit`).
  `engine_init` → `doom_init`; `engine_tick` → `doom_update`; `engine_input` →
  `doom_key_down/up`/`doom_button_*`. Patch the 8bpp framebuffer getter.
- **Crispy / PrBoom+**: reimplement `i_video.c` (present via `plat_present`),
  `i_sound.c`/`i_system.c` over `platform.h`; feed the game loop one tic per
  `engine_tick`. Build via amalgamation.

Then change the `#include "engine_stub.c"` line in `src/doom_cart.c`.

## Suggested milestones

1. **Boot**: cart prints via `cron_log`, sets a frame callback. (Done — stub.)
2. **WAD in ROM**: bake `doom1.wad` with `ROM=`, read header/lumps through
   `wad_rom.c`; log the lump count.
3. **First frame**: get the engine to render the title/menu into the 8bpp
   buffer; `plat_present` shows it. (Biggest single step.)
4. **Input**: map `cron_pad`/`cron_key` to DOOM events; navigate the menu.
5. **Renderer perf**: route `R_DrawColumn`/`R_DrawSpan` to `cron_tcol`/`tspan`;
   measure frame time.
6. **Audio**: DMX SFX via `cron_sample_u8`; music via a MIDI/OPL synth on the
   cart feeding `cron_stream` (PureDOOM gives `doom_tick_midi` / a mixed sound
   buffer).
7. **Fixed-point sweep**: replace i64 sites with `cvm_qmul_16_16` / `cvm_int64.h`
   as the translator flags them.

## Open decisions

- Port choice (Crispy vs PrBoom+ vs PureDOOM) — see "Choosing the port".
- Amalgamation vs. extending `cvm-cc` with `llvm-link` (only if multi-file).
- Tic rate: DOOM is 35Hz, Cronopio frame callback is 60Hz — run DOOM logic at
  35Hz with a fractional accumulator, present every Cronopio frame.
