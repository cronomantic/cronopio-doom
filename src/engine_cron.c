/* DOOM engine binding for Cronopio (doomgeneric pattern).
 *
 * Implements the 5-function seam in engine.h by driving Crispy's split loop:
 *   engine_init()       -> D_DoomMain() runs DOOM init; our d_main.c patch
 *                          neuters the trailing infinite D_DoomLoop and returns
 *                          here with the game sitting at the title screen.
 *   engine_tick()       -> doom_tick() == one D_RunFrame() iteration.
 *   engine_framebuffer()-> I_VideoBuffer (8bpp indexed, 320x200).
 *   engine_palette()    -> the 256x3 RGB palette stored by I_SetPalette.
 *   engine_input(pad)   -> translate cron_pad/cron_key edges into DOOM key
 *                          events posted via D_PostEvent.
 */
#include <cronopio.h>

#include "engine.h"
#include "platform.h"

#include "doomtype.h"
#include "d_event.h"
#include "doomkeys.h"

/* Crispy entry points (i_main.c is excluded; we own argc/argv). */
extern void D_DoomMain(void);
extern void doom_tick(void);
extern void D_PostEvent(event_t *ev);

/* From i_video_cron.c */
extern unsigned char *I_VideoBuffer;
extern const unsigned char *I_Cron_GetPalette(void);

/* m_argv.c defines myargc/myargv; i_main.c (excluded) normally sets them.
 * We initialise them in engine_init before D_DoomMain. */
extern int    myargc;
extern char **myargv;
static char  *fake_argv[] = { (char *)"doom" };

void plat_log(const char *s); /* platform.c */

/* ----------------------------------------------------------------------- */

void engine_init(void)
{
    myargc = 1;
    myargv = fake_argv;

    plat_log("[engine] D_DoomMain start\n");
    D_DoomMain();
    plat_log("[engine] D_DoomMain returned (title ready)\n");
}

void engine_tick(void)
{
    doom_tick();
}

const uint8_t *engine_framebuffer(void)
{
    return (const uint8_t *)I_VideoBuffer;
}

const uint8_t *engine_palette(void)
{
    return (const uint8_t *)I_Cron_GetPalette();
}

/* ---- input ------------------------------------------------------------- */
/* Post a DOOM key event. data2/data3 carry the ASCII rep for menu typing. */
static void post_key(boolean down, int key, int ascii)
{
    event_t ev;
    ev.type  = down ? ev_keydown : ev_keyup;
    ev.data1 = key;
    ev.data2 = ascii;
    ev.data3 = ascii;
    ev.data4 = 0;
    ev.data5 = 0;
    ev.data6 = 0;
    D_PostEvent(&ev);
}

/* Map a gamepad bit to a DOOM key. ascii used for menu/typing where relevant. */
typedef struct { uint32_t bit; int key; int ascii; } padmap_t;

static const padmap_t padmaps[] = {
    { CRON_BTN_UP,    KEY_UPARROW,    0   },
    { CRON_BTN_DOWN,  KEY_DOWNARROW,  0   },
    { CRON_BTN_LEFT,  KEY_LEFTARROW,  0   },
    { CRON_BTN_RIGHT, KEY_RIGHTARROW, 0   },
    { CRON_BTN_A,     KEY_ENTER,      KEY_ENTER },  /* confirm / use */
    { CRON_BTN_B,     KEY_RCTRL,      0   },        /* fire */
    { CRON_BTN_X,     ' ',            ' ' },        /* use (open doors) */
    { CRON_BTN_Y,     KEY_ESCAPE,     0   },        /* menu / back */
};
#define NUM_PADMAPS (int)(sizeof(padmaps) / sizeof(padmaps[0]))

static uint32_t prev_pad = 0;

void engine_input(uint32_t pad)
{
    uint32_t changed = pad ^ prev_pad;

    for (int i = 0; i < NUM_PADMAPS; ++i)
    {
        uint32_t b = padmaps[i].bit;
        if (changed & b)
        {
            post_key((pad & b) != 0, padmaps[i].key, padmaps[i].ascii);
        }
    }

    prev_pad = pad;
}
