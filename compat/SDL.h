/* Minimal SDL.h shim for the Cronopio DOOM port.
 *
 * The real SDL is not linked. Only a handful of kept headers/sources mention
 * SDL types/macros; this provides just enough for them to compile. The actual
 * SDL-using .c files (i_input.c, i_video.c, ...) are EXCLUDED from the build.
 */
#ifndef CRONOPIO_COMPAT_SDL_H
#define CRONOPIO_COMPAT_SDL_H

#include <stdint.h>

/* i_input.h declares I_HandleKeyboardEvent(SDL_Event *) etc. (defined only in
 * the excluded i_input.c) — we just need the type name to exist. */
typedef union SDL_Event {
    uint32_t type;
    uint8_t  padding[64];
} SDL_Event;

typedef struct SDL_version {
    uint8_t major, minor, patch;
} SDL_version;

#ifndef SDL_VERSION_ATLEAST
#define SDL_VERSION_ATLEAST(x, y, z) 0
#endif

#endif /* CRONOPIO_COMPAT_SDL_H */
