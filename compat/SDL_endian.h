/* Minimal SDL_endian.h shim for the Cronopio DOOM port.
 *
 * Crispy's i_swap.h (SHORT/LONG, used across WAD/data reading) includes
 * <SDL_endian.h> for byte-swap helpers. We have no SDL. CronoVM is
 * little-endian (CVM1, little-endian) and so is the WAD format, so the
 * little-endian swaps are the identity. Providing this header on the include
 * path lets i_swap.h compile unmodified — no edits to the vendored tree.
 */
#ifndef CRONOPIO_DOOM_SDL_ENDIAN_SHIM_H
#define CRONOPIO_DOOM_SDL_ENDIAN_SHIM_H

#include <stdint.h>

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

/* Big-endian byte swaps, used by midifile.c to read MIDI headers. The bytes
 * are shuffled through a *volatile* array on purpose: a plain shift/or swap is
 * folded by clang into the llvm.bswap intrinsic, which cvm-translate does not
 * lower. The volatile accesses block that idiom recognition. Byteswap runs only
 * a handful of times at song load, so the cost is irrelevant. */
static inline uint16_t cron_bswap16(uint16_t x)
{
    volatile uint8_t b[2];
    b[0] = (uint8_t)(x >> 8);
    b[1] = (uint8_t)x;
    return (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
}
static inline uint32_t cron_bswap32(uint32_t x)
{
    volatile uint8_t b[4];
    b[0] = (uint8_t)(x >> 24);
    b[1] = (uint8_t)(x >> 16);
    b[2] = (uint8_t)(x >> 8);
    b[3] = (uint8_t)x;
    return  (uint32_t)b[0]        | ((uint32_t)b[1] << 8)
         | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

/* Little-endian host: LE swaps are no-ops; BE swaps reverse the bytes. */
#define SDL_SwapLE16(x) (x)
#define SDL_SwapLE32(x) (x)
#define SDL_SwapBE16(x) cron_bswap16((uint16_t)(x))
#define SDL_SwapBE32(x) cron_bswap32((uint32_t)(x))

#endif /* CRONOPIO_DOOM_SDL_ENDIAN_SHIM_H */
