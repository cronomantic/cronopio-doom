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
#include "crispy.h"   /* crispy->input: 0 = gamepad only, 1 = gamepad + keyboard */

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

/* ---- keyboard ---------------------------------------------------------- */
/* cron_key(scancode) reports key state in the USB HID usage-ID space (the
 * CRON_KEY_* values in cronopio.h). That is exactly the space Crispy's
 * SCANCODE_TO_KEYS_ARRAY (doomkeys.h) is indexed by — the same table the
 * vendored SDL i_input.c uses — so we reuse it verbatim for a full classic
 * keyboard map (movement, weapons 1-7, use/fire/run/strafe, menu, F-keys)
 * with zero bespoke mapping. */
static const int sc_to_key[] = SCANCODE_TO_KEYS_ARRAY;
#define NUM_SCANCODES (int)(sizeof(sc_to_key) / sizeof(sc_to_key[0]))

/* Modifier keys live above SCANCODE_TO_KEYS_ARRAY (which only covers HID
 * 0..103); map them the way the vendored i_input.c TranslateKey does so
 * fire (Ctrl), run (Shift) and strafe (Alt) work. */
static const struct { int sc; int key; } kb_mods[] = {
    { CRON_KEY_LCTRL,  KEY_RCTRL  }, { CRON_KEY_RCTRL,  KEY_RCTRL  },
    { CRON_KEY_LSHIFT, KEY_RSHIFT }, { CRON_KEY_RSHIFT, KEY_RSHIFT },
    { CRON_KEY_LALT,   KEY_LALT   }, { CRON_KEY_RALT,   KEY_RALT   },
};
#define NUM_KBMODS (int)(sizeof(kb_mods) / sizeof(kb_mods[0]))

/* Per-DOOM-key state. We gather every input source into one "is this DOOM key
 * down" bitmap (key_now) and post edges against the previous frame. This
 * DEDUPES sources that map to the same DOOM key: e.g. the desktop host drives
 * both the cron_pad d-pad AND the raw arrow keys from the same physical keys,
 * which otherwise posted KEY_DOWNARROW twice and made menus skip entries. A
 * DOOM key is "down" if ANY active source holds it. */
static uint8_t key_now[256];
static uint8_t key_prev[256];
static int     key_ascii[256];

static void mark_key(int key, int ascii)
{
    if (key <= 0 || key > 255) return;
    key_now[key]   = 1;
    key_ascii[key] = ascii;   /* same key -> same ascii from any source */
}

void engine_input(uint32_t pad)
{
    for (int k = 0; k < 256; ++k) key_now[k] = 0;

    /* Gamepad — always active, the system's primary control. */
    for (int i = 0; i < NUM_PADMAPS; ++i)
        if (pad & padmaps[i].bit)
            mark_key(padmaps[i].key, padmaps[i].ascii);

    /* Keyboard — layered on top only when the Input Device setting includes it. */
    if (crispy->input != 0)
    {
        for (int sc = 0; sc < NUM_SCANCODES; ++sc)
        {
            int key = sc_to_key[sc];
            if (key && cron_key(sc))
                mark_key(key, (key >= 32 && key < 127) ? key : 0);
        }
        for (int i = 0; i < NUM_KBMODS; ++i)
            if (cron_key(kb_mods[i].sc))
                mark_key(kb_mods[i].key, 0);
    }

    /* Post one edge per DOOM key on the union of all sources. */
    for (int k = 1; k < 256; ++k)
        if (key_now[k] != key_prev[k])
        {
            post_key(key_now[k] != 0, k, key_now[k] ? key_ascii[k] : 0);
            key_prev[k] = key_now[k];
        }
}
