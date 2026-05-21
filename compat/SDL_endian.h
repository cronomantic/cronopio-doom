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

#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER  SDL_LIL_ENDIAN

/* Little-endian host: LE swaps are no-ops. (BE swaps are unused by the port,
 * but provided for completeness with a portable byte shuffle.) */
#define SDL_SwapLE16(x) (x)
#define SDL_SwapLE32(x) (x)
#define SDL_SwapBE16(x) ((Uint16)((((Uint16)(x)) << 8) | (((Uint16)(x)) >> 8)))
#define SDL_SwapBE32(x) ((Uint32)( (((Uint32)(x)) << 24) | ((((Uint32)(x)) << 8) & 0x00FF0000) | \
                                   ((((Uint32)(x)) >> 8) & 0x0000FF00) | (((Uint32)(x)) >> 24) ))

#endif /* CRONOPIO_DOOM_SDL_ENDIAN_SHIM_H */
