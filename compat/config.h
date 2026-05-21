/* Hand-written config.h for the Cronopio DOOM port.
 *
 * Crispy normally generates this from cmake/config.h.cin via CMake/autotools.
 * We are not using its build system, so we bake the minimal set of macros the
 * DOOM build needs for a no-SDL / no-OS / no-network target. (See the survey
 * in the project memory: crispy-port-manifest.)
 */
#ifndef CRONOPIO_DOOM_CONFIG_H
#define CRONOPIO_DOOM_CONFIG_H

#define PACKAGE_NAME    "Crispy Doom"
#define PACKAGE_TARNAME "crispy-doom"
#define PACKAGE_VERSION "7.1.0"
#define PACKAGE_STRING  "Crispy Doom 7.1.0"
#define PROGRAM_PREFIX  "crispy-"

/* Optional libraries we do not link — leave undefined:
 *   HAVE_FLUIDSYNTH, HAVE_LIBSAMPLERATE, HAVE_LIBPNG, HAVE_DIRENT_H
 * Indexed 8bpp pipeline (no truecolor): leave CRISPY_TRUECOLOR undefined. */

/* The SDK libc declares strcasecmp/strncasecmp (string.h/strings.h) and
 * defines them in cvm_libc.c, so tell Crispy they're available — otherwise
 * doomtype.h remaps them to stricmp/strnicmp, which we don't provide. */
#define HAVE_DECL_STRCASECMP  1
#define HAVE_DECL_STRNCASECMP 1

#endif /* CRONOPIO_DOOM_CONFIG_H */
