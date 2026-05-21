/* SDL_version.h shim — v_video.c uses SDL_VERSION_ATLEAST in a #if. */
#ifndef CRONOPIO_COMPAT_SDL_VERSION_H
#define CRONOPIO_COMPAT_SDL_VERSION_H

#ifndef SDL_VERSION_ATLEAST
#define SDL_VERSION_ATLEAST(x, y, z) 0
#endif

#endif
