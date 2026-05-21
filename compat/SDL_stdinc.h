/* SDL_stdinc.h shim — maps the few SDL stdlib wrappers used by kept code to
 * the SDK libc. (m_argv.c uses SDL_qsort.) */
#ifndef CRONOPIO_COMPAT_SDL_STDINC_H
#define CRONOPIO_COMPAT_SDL_STDINC_H

#include <stdlib.h>

#define SDL_qsort  qsort
#define SDL_malloc malloc
#ifndef SDL_free
#define SDL_free   free
#endif

#endif
