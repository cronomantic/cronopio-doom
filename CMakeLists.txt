cmake_minimum_required(VERSION 3.16)
project(cronopio-doom C)

# Needs an installed Cronopio SDK. Configure with:
#   cmake -B build -DCMAKE_PREFIX_PATH=<cronopio-install-prefix>
# (install it from the Cronopio repo with `cmake --install build --prefix ...`)
find_package(Cronopio REQUIRED)

# A cartridge is a single translation unit (unity build in src/doom_cart.c).
# DOOM wants a roomy stack for BSP recursion; 32M heap covers the zone +
# decompressed lumps. Bake the WAD in with ROM=... once you have one:
#   cronopio_add_cartridge(doom SOURCES src/doom_cart.c
#       HEAP_RESERVE 32M STACK_RESERVE 1M ROM ${CMAKE_SOURCE_DIR}/doom1.wad)
cronopio_add_cartridge(doom
    SOURCES src/doom_cart.c
    HEAP_RESERVE 32M
    STACK_RESERVE 1M)
