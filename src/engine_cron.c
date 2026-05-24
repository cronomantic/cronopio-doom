/* DOOM engine binding for Cronopio (doomgeneric pattern).
 *
 * Implements the 5-function seam in engine.h by driving Crispy's split loop:
 *   engine_init()       -> D_DoomMain() runs DOOM init; our d_main.c patch
 *                          neuters the trailing infinite D_DoomLoop and returns
 *                          here with the game sitting at the title screen.
 *   engine_tick()       -> doom_tick() == one D_RunFrame() iteration.
 *   engine_framebuffer()-> I_VideoBuffer (8bpp indexed, 320x200).
 *   engine_palette()    -> the 256x3 RGB palette stored by I_SetPalette.
 *   engine_input(pad)   -> translate cron_pad (12-button SNES-style) edges
 *                          into DOOM key events posted via D_PostEvent. The
 *                          host maps physical keyboard/controller -> pad bits,
 *                          so the cart never reads a raw keyboard (portable to
 *                          keyboard-less targets; one remap surface in the host).
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

void I_Cron_UpdateMusic(void);   /* i_sound_cron.c — MIDI sequencer pump */

void engine_tick(void)
{
    doom_tick();
    I_Cron_UpdateMusic();        /* advance music + dispatch due MIDI events */
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

/* In-game mapping for the SNES-style 12-button pad. The cart posts the
 * keycodes DOOM binds BY DEFAULT (see crispy m_controls.c), so gameplay is
 * 100% pad: the desktop host turns physical keyboard/controller presses into
 * pad bits (remappable in its F1 menu) and no raw keyboard reaches the cart.
 * Keyboard-less targets therefore play identically. */
static const padmap_t padmap_game[] = {
    { CRON_BTN_UP,     KEY_UPARROW,    0   },  /* forward      (key_up)          */
    { CRON_BTN_DOWN,   KEY_DOWNARROW,  0   },  /* back         (key_down)        */
    { CRON_BTN_LEFT,   KEY_LEFTARROW,  0   },  /* turn left    (key_left)        */
    { CRON_BTN_RIGHT,  KEY_RIGHTARROW, 0   },  /* turn right   (key_right)       */
    { CRON_BTN_L,      ',',            0   },  /* strafe left  (key_strafeleft)  */
    { CRON_BTN_R,      '.',            0   },  /* strafe right (key_straferight) */
    { CRON_BTN_A,      KEY_RCTRL,      0   },  /* fire         (key_fire)        */
    { CRON_BTN_B,      ' ',            ' ' },  /* use / open   (key_use)         */
    { CRON_BTN_X,      ']',            0   },  /* next weapon  (key_nextweapon)  */
    { CRON_BTN_Y,      '[',            0   },  /* prev weapon  (key_prevweapon)  */
    { CRON_BTN_START,  KEY_ESCAPE,     0   },  /* open menu                      */
    /* SELECT is handled specially (tap=automap / hold=autorun), not here. */
};
#define NUM_GAME (int)(sizeof(padmap_game) / sizeof(padmap_game[0]))

/* Menu / title mapping. While a menu is up, A/B must mean confirm/cancel
 * (not fire/use), so engine_input selects this table on `menuactive`. The
 * d-pad navigates; START also closes the menu. */
static const padmap_t padmap_menu[] = {
    { CRON_BTN_UP,     KEY_UPARROW,    0         },
    { CRON_BTN_DOWN,   KEY_DOWNARROW,  0         },
    { CRON_BTN_LEFT,   KEY_LEFTARROW,  0         },
    { CRON_BTN_RIGHT,  KEY_RIGHTARROW, 0         },
    { CRON_BTN_A,      KEY_ENTER,      KEY_ENTER },  /* confirm */
    { CRON_BTN_B,      KEY_ESCAPE,     0         },  /* back / cancel */
    { CRON_BTN_START,  KEY_ESCAPE,     0         },  /* close menu */
};
#define NUM_MENU (int)(sizeof(padmap_menu) / sizeof(padmap_menu[0]))

extern boolean menuactive;   /* m_menu.c — a DOOM menu is currently on screen */

/* Per-DOOM-key state. We gather all mappings into one "is this DOOM key down"
 * bitmap (key_now) and post a single edge per key against the previous frame.
 * This keeps one clean edge even if several pad bits map to the same DOOM key. */
static uint8_t key_now[256];
static uint8_t key_prev[256];
static int     key_ascii[256];

static void mark_key(int key, int ascii)
{
    if (key <= 0 || key > 255) return;
    key_now[key]   = 1;
    key_ascii[key] = ascii;   /* same key -> same ascii from any source */
}

/* SELECT in play: a quick TAP toggles the automap; HOLDING it ~0.5s toggles
 * autorun. SELECT is not a movement/combat button, so a long press never
 * interferes with play (a shoulder/strafe chord would). ~60 input frames/s. */
#define SELECT_HOLD_FRAMES 30

void engine_input(uint32_t pad)
{
    static int     select_frames   = 0;
    static boolean select_consumed = false;

    for (int k = 0; k < 256; ++k) key_now[k] = 0;

    /* SELECT long-press: only meaningful in play (ignored while a menu is up). */
    if (!menuactive && (pad & CRON_BTN_SELECT))
    {
        if (++select_frames >= SELECT_HOLD_FRAMES && !select_consumed)
        {
            mark_key(KEY_CAPSLOCK, 0);     /* held long -> toggle autorun */
            select_consumed = true;
        }
    }
    else
    {
        if (select_frames > 0 && !select_consumed && !menuactive)
            mark_key(KEY_TAB, 0);          /* short tap released -> automap */
        select_frames   = 0;
        select_consumed = false;
    }

    /* Pick the binding table by context: confirm/cancel in menus, fire/use
     * in play. Everything is a pad bit now — the host owns physical remap. */
    const padmap_t *map = menuactive ? padmap_menu : padmap_game;
    const int       n   = menuactive ? NUM_MENU    : NUM_GAME;
    for (int i = 0; i < n; ++i)
        if (pad & map[i].bit)
            mark_key(map[i].key, map[i].ascii);

    /* Post one edge per DOOM key on the union of all sources. */
    for (int k = 1; k < 256; ++k)
        if (key_now[k] != key_prev[k])
        {
            post_key(key_now[k] != 0, k, key_now[k] ? key_ascii[k] : 0);
            key_prev[k] = key_now[k];
        }
}
