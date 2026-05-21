# cronopio-doom

A DOOM port for the [Cronopio](https://github.com/Cronomantic/Cronopio) fantasy
console (carts are CronoVM `.bin` files built with `cronopio-cc`).

**Status:** skeleton. The cartridge builds and runs today with a placeholder
"engine stub" that renders an animated test pattern — proving the Cronopio
integration (8bpp framebuffer, palette, present loop, build) before any DOOM
code is added. The source port to drop in is **not yet chosen**; see
[`KICKOFF.md`](KICKOFF.md).

## Layout

```
src/
  doom_cart.c     the cartridge (single unity TU); CRONOPIO_CART entry point
  platform.h/.c   Cronopio platform layer: present(8bpp+palette), audio, time, log
  wad_rom.h/.c    read the WAD from cartridge ROM (no filesystem on the VM)
  engine.h        the 5-function seam the engine binding implements
  engine_stub.c   placeholder engine (replace with the real port's binding)
third_party/      the DOOM source port goes here
KICKOFF.md        full context + plan (read this first)
```

## Build & run

Needs an installed Cronopio SDK on `CMAKE_PREFIX_PATH` (build the Cronopio repo,
then `cmake --install build --prefix <prefix>`).

```sh
cmake -B build -DCMAKE_PREFIX_PATH=<cronopio-install-prefix>
cmake --build build            # -> build/doom.bin
cronopio build/doom.bin        # run on the desktop host
```

Or one-line, no CMake (SDK `bin/` on PATH):

```sh
cronopio-cc src/doom_cart.c -o doom.bin
cronopio doom.bin
```

See [`KICKOFF.md`](KICKOFF.md) for the port plan, the Cronopio constraints, and
the `I_*`→syscall mapping.
