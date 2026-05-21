cmake_minimum_required(VERSION 3.16)
project(cronopio-doom C)

# Needs an installed Cronopio SDK. Configure with:
#   cmake -B build -DCMAKE_PREFIX_PATH=<cronopio-install-prefix>
# (install it from the Cronopio repo with `cmake --install build --prefix ...`)
find_package(Cronopio REQUIRED)

# Which IWAD/megawad gets baked into this cartridge. One WAD per .bin; pick it
# at configure time. The engine auto-detects the game (DOOM 1 vs DOOM 2) from
# the IWAD header, so the same engine binary serves doom1, doom2 and Freedoom
# phase 1/2 — only the baked WAD changes.
#   cmake -B build -DIWAD=/path/to/doom2.wad
set(IWAD "${CMAKE_SOURCE_DIR}/wads/freedoom1.wad"
    CACHE FILEPATH "Path to the IWAD/megawad baked into the cartridge ROM")

if(NOT EXISTS "${IWAD}")
    message(WARNING
        "IWAD not found: ${IWAD}\n"
        "The cart will build without a baked WAD (the engine will have nothing "
        "to load). Set -DIWAD=<path> to a doom1/doom2/freedoom WAD.")
    set(IWAD "")
endif()

# A cartridge is a single translation unit (unity build in src/doom_cart.c).
# DOOM wants a roomy stack for BSP recursion; 32M heap covers the zone +
# decompressed lumps. (NB: a 28 MB Freedoom IWAD in ROM may need a larger heap
# reserve once the engine's zone allocator is wired — revisit at first-frame.)
cronopio_add_cartridge(doom
    SOURCES src/doom_cart.c
    HEAP_RESERVE 32M
    STACK_RESERVE 1M
    ROM "${IWAD}")
