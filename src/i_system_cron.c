/* Cronopio system backend — replaces Crispy's i_system.c.
 *
 * I_Error/I_Quit funnel to cron_log + cron_exit. I_ZoneBase carves DOOM's
 * zone heap out of the cartridge heap (cvm_alloc), leaving headroom for the
 * libc malloc used by everything outside the zone (WAD directory, strings).
 */
#include <cronopio.h>

#include "doomtype.h"
#include "d_event.h"
#include "d_ticcmd.h"
#include "i_system.h"
#include "i_timer.h"

#include "cvm_alloc.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void plat_log(const char *s); /* from platform.c */

/* ---- atexit list ------------------------------------------------------- */
#define MAX_ATEXIT 32
typedef struct {
    atexit_func_t func;
    boolean       run_on_error;
} exit_func_t;

static exit_func_t exit_funcs[MAX_ATEXIT];
static int         num_exit_funcs = 0;

void I_AtExit(atexit_func_t func, boolean run_if_error)
{
    if (num_exit_funcs < MAX_ATEXIT)
    {
        exit_funcs[num_exit_funcs].func = func;
        exit_funcs[num_exit_funcs].run_on_error = run_if_error;
        ++num_exit_funcs;
    }
}

static void RunExitFuncs(boolean error)
{
    /* Run in reverse registration order, like atexit(). */
    for (int i = num_exit_funcs - 1; i >= 0; --i)
    {
        if (!error || exit_funcs[i].run_on_error)
        {
            exit_funcs[i].func();
        }
    }
    num_exit_funcs = 0;
}

/* ---- error / quit ------------------------------------------------------ */
void I_Error(const char *error, ...)
{
    char    msg[512];
    va_list args;

    va_start(args, error);
    vsnprintf(msg, sizeof(msg), error, args);
    va_end(args);

    plat_log("\n*** I_Error: ");
    plat_log(msg);
    plat_log("\n");

    RunExitFuncs(true);
    cron_exit(1);
    for (;;) { } /* NORETURN */
}

void I_Quit(void)
{
    plat_log("[cronopio-doom] I_Quit\n");
    RunExitFuncs(false);
    cron_exit(0);
    for (;;) { } /* NORETURN */
}

/* ---- zone base --------------------------------------------------------- */
byte *I_ZoneBase(int *size)
{
    /* The WAD lives in ROM, separate from the heap reserve. Leave a few MB of
     * headroom for libc malloc (WAD lump cache via Z is inside the zone, but
     * lumpinfo / strings / our buffers use malloc). */
    int heap = cvm_sys_heap_size();
    int headroom = 6 * 1024 * 1024;
    int want = heap - headroom;

    if (want < 8 * 1024 * 1024)
        want = 8 * 1024 * 1024;

    byte *base = NULL;
    while (want >= 4 * 1024 * 1024)
    {
        base = (byte *)malloc((size_t)want);
        if (base != NULL) break;
        want -= 2 * 1024 * 1024;
    }

    if (base == NULL)
    {
        I_Error("I_ZoneBase: failed to allocate zone heap");
    }

    *size = want;
    {
        char b[64];
        snprintf(b, sizeof(b), "[zone] %d MB\n", want / (1024 * 1024));
        plat_log(b);
    }
    return base;
}

void *I_Realloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size);
    if (p == NULL && size != 0)
        I_Error("I_Realloc: failed on reallocation of %u bytes", (unsigned)size);
    return p;
}

/* ---- misc -------------------------------------------------------------- */
void I_Init(void) { }

boolean I_ConsoleStdout(void) { return false; }

void I_Tactile(int on, int off, int total) { (void)on; (void)off; (void)total; }

boolean I_GetMemoryValue(unsigned int offset, void *value, int size)
{
    (void)offset; (void)value; (void)size;
    return false;
}

static ticcmd_t emptycmd;
ticcmd_t *I_BaseTiccmd(void) { return &emptycmd; }

void I_BindVariables(void) { }

void I_PrintStartupBanner(const char *gamedescription)
{
    plat_log("=================================\n ");
    if (gamedescription) plat_log(gamedescription);
    plat_log("\n=================================\n");
}

void I_PrintBanner(const char *text)
{
    if (text) { plat_log(text); plat_log("\n"); }
}

void I_PrintDivider(void)
{
    plat_log("===========================================================\n");
}
