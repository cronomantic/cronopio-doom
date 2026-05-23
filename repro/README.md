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

- **`headless_key.c`** — holds a chosen HID scancode down (after a short warmup)
  so the cart's input path posts the matching DOOM event; screenshots the result.
  Confirms keyboard control end-to-end. Two-phase turns via `CRON_SC2`/`CRON_T1`
  env vars (hold one key, then another), and pad bits via bit 16 of the scancode.

  ```sh
  headless_key cart.bin frames scancode_hex [out.ppm]
  headless_key doom.bin 200 0x29        # hold ESC -> main menu opens
  ```

- **`headless_nav.c`** — taps a *sequence* of HID scancodes (each held a few
  frames then released) and screenshots the final frame. For driving menus.

  ```sh
  headless_nav cart.bin out.ppm sc1 sc2 sc3 ...
  ```

## Building them

They are standalone host programs (not part of `build_doom.sh`). Compile against
the Cronopio host common lib + CronoVM, e.g.:

```sh
clang -I <Cronopio>/host/common -I <Cronopio>/external/CronoVM/include \
      headless_key.c \
      <Cronopio>/build/host/libcronopio_common.a <Cronopio>/build/_cvm/libcvm.a \
      -lkernel32 -luser32 -lshell32 -o headless_key.exe
```

Convert the `.ppm` output to PNG (`magick out.ppm out.png`) to inspect visually.
