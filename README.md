# cronopio-doom

A DOOM port for the [Cronopio](https://github.com/Cronomantic/Cronopio) fantasy
console. A cartridge is a CronoVM `.bin` built from C with `cronopio-cc`; the
engine is [Crispy Doom](https://github.com/fabiangreffrath/crispy-doom)
(limit-removing, vanilla-accurate) with the SDL/mixer/net backends replaced by a
thin Cronopio platform layer.

**Status: playable.** Boots to the title and plays levels at an interactive
rate. The 3D renderer is offloaded to Cronopio's GPU primitives; keyboard and
gamepad input work; audio is wired — music plays through the host MIDI +
SoundFont synth (DOOM MUS → MIDI) and SFX through the PCM voices (DMX `DS*`).

## Layout

```
src/
  doom_cart.c        cartridge entry: wires the Cronopio frame callback to the engine
  engine_cron.c      engine seam — boots D_DoomMain, drives one frame per tick
  platform.c/.h      Cronopio platform layer: present (8bpp+palette), audio, time, log
  i_video_cron.c     framebuffer / palette presentation
  i_sound_cron.c     music (MUS→MIDI→cron_midi) + SFX (DMX→cron_pcm)
  i_input_cron.c     keyboard + gamepad → DOOM events
  i_system/timer/...  the rest of the I_* backend (no filesystem, ROM WAD, ...)
  w_file_rom.c       read the WAD from cartridge ROM (the VM has no filesystem)
  m_fixed_cvm.c      i64 fixed-point helpers via CronoVM intrinsic opcodes
compat/              tiny SDL_*/headers shims so the vendored tree builds unmodified
third_party/crispy-doom   the engine (submodule, branch cronopio-port)
third_party/Cronopio      the console SDK + hosts (submodule; nested CronoVM, TinySoundFont)
wads/                IWADs baked into carts (freedoom1.wad, freedoom2.wad)
build_doom.sh        the build (cvm-cc compiles every .c, llvm-links, cvm-translate)
repro/               headless harnesses for debugging without an interactive host
```

## Build

Everything is self-contained via submodules — clone recursively (or init after):

```sh
git clone --recurse-submodules <this repo>
# or, in an existing checkout:
git submodule update --init --recursive
```

Then:

```sh
bash build_doom.sh                              # -> doom.bin (bakes wads/freedoom1.wad)
bash build_doom.sh wads/freedoom2.wad out.bin   # a different IWAD / output
```

The first run auto-builds the Cronopio SDK tools and hosts under
`third_party/Cronopio/build` (one-time; needs CMake + Ninja + a C compiler).
The IWAD is baked into the cartridge ROM (`--rom`); one WAD per `.bin`. The
engine auto-detects the game (DOOM 1 vs DOOM 2) from the IWAD header, so the
same code serves doom1/doom2/Freedoom — only the baked WAD changes.

### Building carts for every game you have

Drop your IWADs into `wads/` and run:

```sh
bash make_carts.sh                          # one dist/<game>.bin per IWAD found
```

It identifies each WAD (DOOM, DOOM 2, TNT, Plutonia, Freedoom, FreeDM, Chex)
and names the cart accordingly; non-DOOM-engine IWADs (Heretic/Hexen) are
detected and skipped. Supplementary PWADs are merged into the IWAD and baked as
one ROM — either on the command line or via a manifest:

```sh
bash make_carts.sh mymaps doom2.wad maps.wad deh.wad   # one custom cart
```

```text
# wads/carts.txt — "<name> <iwad> [pwad...]" (paths relative to wads/)
doom2-mymaps  doom2.wad  maps.wad
```

`tools/wadtool.c` (compiled on first use; no scripting-language dependency)
does the identify/merge. PWAD merging follows DeuTex/`-merge` semantics (a port
of crispy-doom's `w_merge.c`, applied offline and per-PWAD): new/replacement
flats and sprites are merged into the IWAD's `F_START..F_END` / `S_START..S_END`
ranges (so the contiguous ranges vanilla relies on stay intact), and everything
else (maps, TEXTURE1/PNAMES, sounds, music, DEHACKED, by-name replacements) is
appended with last-wins precedence — i.e. vanilla `-file` order.

> The legacy `CMakeLists.txt` predates the multi-file build and is stale;
> `build_doom.sh` is the current path.

## Run

On the Cronopio desktop host (self-contained — no SDL2.dll, the SoundFont is
embedded in the executable):

```sh
third_party/Cronopio/build/host/desktop/cronopio.exe doom.bin
```

Headless (renders frames, no window — for testing / screenshots):

```sh
third_party/Cronopio/build/tools/headless/cronopio-headless.exe doom.bin <frames> [out.ppm]
```

## Notes

The engine lives on a fork (`cronomantic/crispy-doom`, branch `cronopio-port`)
carrying the `[cronopio]` patches; the submodule's origin is that fork. See
`TODO.txt` for the open backlog and `repro/README.md` for the headless
debugging harnesses.
