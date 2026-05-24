# repro/ — headless debugging harnesses

Small host programs that drive a cartridge **without an interactive window**, so
DOOM behaviour can be exercised and screenshotted from the command line. They
reuse Cronopio's headless harness (console + syscalls + a virtual 60 Hz clock)
and link against the Cronopio host libs.

> Historical note: this directory began as a minimal repro for a CronoVM
> translator bug (pointer-relocation global initialisers in multi-file builds).
> That bug is **fixed** — the full DOOM engine translates and runs — so the repro
> pair has been removed. What remains here are the live debugging tools below.

## Tools

- **`headless_pad.c`** — holds a *sequence* of 12-button pad masks (SNES-style:
  d-pad, A/B/X/Y, L/R, Start/Select) on `cron_pad(0)`, each for a few frames,
  and screenshots the final frame. The console exposes only the abstract pad
  (the host maps keyboard/controller -> pad bits; there is no raw-keyboard
  primitive), so this is the way to drive a cart's input from the command line.

  ```sh
  headless_pad cart.bin out.ppm mask1 mask2 ...   # masks hex/dec
  headless_pad doom.bin out.ppm 0x400 0x002 0x002 # START -> menu, DOWN, DOWN
  # bits: UP=0x1 DOWN=0x2 LEFT=0x4 RIGHT=0x8 A=0x10 B=0x20 X=0x40 Y=0x80
  #       L=0x100 R=0x200 START=0x400 SELECT=0x800
  ```

## Building them

They are standalone host programs (not part of `build_doom.sh`). Compile against
the Cronopio host common lib + CronoVM, e.g.:

```sh
clang -I <Cronopio>/host/common -I <Cronopio>/external/CronoVM/include \
      headless_pad.c \
      <Cronopio>/build/host/libcronopio_common.a <Cronopio>/build/_cvm/libcvm.a \
      -lkernel32 -luser32 -lshell32 -o headless_pad.exe
```

Convert the `.ppm` output to PNG (`magick out.ppm out.png`) to inspect visually.
