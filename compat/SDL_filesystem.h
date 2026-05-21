/* SDL_filesystem.h shim. m_config.c calls SDL_GetPrefPath for the config/music
 * pack directories. There is no filesystem on Cronopio, so return NULL (callers
 * handle NULL by skipping). */
#ifndef CRONOPIO_COMPAT_SDL_FILESYSTEM_H
#define CRONOPIO_COMPAT_SDL_FILESYSTEM_H

#include <stdlib.h>

static inline char *SDL_GetPrefPath(const char *org, const char *app)
{
    (void)org; (void)app;
    return (char *)0;
}

#ifndef SDL_free
#define SDL_free free
#endif

#endif
