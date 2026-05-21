// [cronopio] Stub for i_glob.c (system file-globbing interface).
//
// The real i_glob.c walks a host directory with opendir/readdir to find
// files matching a pattern. A Cronopio cartridge has no host filesystem —
// the IWAD is baked into ROM (see w_file_rom.c / wad_rom.c) — so there is
// nothing to glob. The only callers are optional autoload scans:
//   - DEH_AutoLoadPatches (deh_main.c): scans for *.deh/*.bex/*.hhe/*.seh
//   - W_AutoLoadWADs (w_main.c): scans an autoload dir for extra PWADs
// Both loop `while ((f = I_NextGlob(g)) != NULL)` and call I_EndGlob(g) at
// the end. Returning NULL from the Start functions makes the loop body run
// zero times; I_NextGlob/I_EndGlob are NULL-safe no-ops. Net effect: no
// autoloaded files, which is exactly right for a fixed ROM cartridge.

#include <stddef.h>
#include <stdarg.h>
#include "i_glob.h"

glob_t *I_StartGlob(const char *directory, const char *glob, int flags)
{
    (void)directory;
    (void)glob;
    (void)flags;
    return NULL;
}

glob_t *I_StartMultiGlob(const char *directory, int flags,
                         const char *glob, ...)
{
    (void)directory;
    (void)flags;
    (void)glob;
    return NULL;
}

void I_EndGlob(glob_t *glob)
{
    (void)glob;
}

const char *I_NextGlob(glob_t *glob)
{
    (void)glob;
    return NULL;
}
