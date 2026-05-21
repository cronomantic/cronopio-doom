# third_party/

The DOOM source port drops in here (as a git submodule or vendored copy).

This directory is intentionally empty in the skeleton. The choice of port is
still open — see [`../KICKOFF.md`](../KICKOFF.md) "Choosing the port". Once
chosen:

- **PureDOOM**: vendor `PureDOOM.h` here; the binding `#include`s it (with
  `DOOM_IMPLEMENTATION`) from the engine binding file.
- **Crispy Doom / PrBoom+**: add as a submodule, then generate an amalgamation
  (a single `.c` that includes the engine sources) because cvm-cc compiles one
  translation unit per cartridge.
