/* Cronopio video backend — replaces Crispy's SDL i_video.c.
 *
 * Owns I_VideoBuffer and the screen-dimension globals. The engine renders an
 * 8bpp indexed image into I_VideoBuffer; the cart's frame() pulls it out via
 * engine_framebuffer() and presents through platform.c. We therefore render
 * at the vanilla 320x200 (hires off, widescreen off) so I_VideoBuffer is
 * exactly DOOM_W*DOOM_H bytes (see platform.h: mode B scales 200->240).
 */
#include "platform.h"

#include "doomtype.h"
#include "i_video.h"
#include "i_truecolor.h"
#include "m_fixed.h"
#include "crispy.h"

#include <stdlib.h>
#include <string.h>

/* ---- screen dimension globals (defined in i_video.c originally) -------- */
int SCREENWIDTH;
int SCREENHEIGHT;
int SCREENHEIGHT_4_3;
int NONWIDEWIDTH;
int WIDESCREENDELTA;

pixel_t *I_VideoBuffer = NULL;

boolean screenvisible = true;
int     usegamma = 0;
int     usemouse = 0;

/* config-knob globals other TUs read (kept harmless) */
char   *video_driver = "";
char   *window_position = "center";
int     screen_width = 320;
int     screen_height = 240;
int     fullscreen = 0;
int     aspect_ratio_correct = 1;
int     integer_scaling = 0;
int     smooth_pixel_scaling = 0;
int     vga_porch_flash = 0;
int     force_software_renderer = 0;
int     png_screenshots = 0;
int     vanilla_keyboard_mapping = 1;
boolean screensaver_mode = false;
unsigned int joywait = 0;

fixed_t fractionaltic;

byte gamma2table[18][256];

/* ----------------------------------------------------------------------- */

void I_GetScreenDimensions(void)
{
    /* Two render modes (selected at compile time by DOOM_H in platform.h):
     *   Mode C (DOOM_H == 240): native 320x240. DOOM renders straight into the
     *     320x240 CRON_FB (see I_InitGraphics) so the GPU accelerators
     *     (cron_tcol/tspan, which write CRON_FB) and C-side drawing share one
     *     persistent buffer — no per-frame rescale.
     *   Mode B (DOOM_H == 200): vanilla 320x200 into a separate buffer that
     *     plat_present scales to 240 (pixel-faithful CRT 4:3, no accelerators). */
    SCREENWIDTH      = ORIGWIDTH;        /* 320 */
#ifdef CRON_ACCEL
    /* 200 (fullscreen 3D view at screenblocks 10) + 32 (status bar) = 232. With
     * SCREENHEIGHT=232, R_SetViewSize gives viewheight = 232-32 = 200, and the
     * bar sits at rows 200..231; plat_present centers the 232 rows in 240. */
    SCREENHEIGHT     = ORIGHEIGHT + 32;          /* 232 (200 view + 32 ST_HEIGHT) */
#else
    SCREENHEIGHT     = ORIGHEIGHT;       /* 200 — mode B (stretched to 240) */
#endif
    SCREENHEIGHT_4_3 = ORIGHEIGHT_4_3;   /* 240 */
    NONWIDEWIDTH     = SCREENWIDTH;
    WIDESCREENDELTA  = 0;
}

void I_SetGammaTable(void)
{
    /* Identity gamma — engine_palette delivers the already-set palette. */
    for (int i = 0; i < 18; ++i)
        for (int j = 0; j < 256; ++j)
            gamma2table[i][j] = (byte)j;
}

void I_InitGraphics(void)
{
    /* Pin Crispy to the vanilla pipeline before any geometry is computed. */
    crispy->hires      = 0;
    crispy->widescreen = 0;
    crispy->uncapped   = 0;   /* kills camera-interp float paths (i64/f64) */
    crispy->vsync      = 0;

    I_GetScreenDimensions();
    I_SetGammaTable();

#ifdef CRON_ACCEL
    /* Accelerated mode: render the 320x200 frame straight into CRON_FB, centered
     * vertically (offset by CRON_VIEW_Y rows). The GPU rasteriser accelerators
     * (cron_tcol/tspan, which write CRON_FB) and the C-side patch drawing share
     * this one persistent buffer; plat_present is 1:1 (no rescale -> no buffer
     * mutation) and paints the top/bottom letterbox. */
    I_VideoBuffer = (pixel_t *)(plat_framebuffer()
                                + CRON_VIEW_Y * CRON_SCREEN_W);
#else
    /* Mode B: a private buffer that plat_present stretches 200->240 for 4:3. */
    if (I_VideoBuffer == NULL)
        I_VideoBuffer = (pixel_t *)malloc(SCREENWIDTH * SCREENHEIGHT * sizeof(pixel_t));
#endif
    memset(I_VideoBuffer, 0, SCREENWIDTH * SCREENHEIGHT * sizeof(pixel_t));

    screenvisible = true;
}

void I_ShutdownGraphics(void) { }

void I_GraphicsCheckCommandLine(void) { }

/* Palette: Crispy hands us 256*3 bytes (gamma-adjusted). Store for the cart. */
static byte cron_palette[256 * 3];
static int  cron_palette_valid = 0;

void I_SetPalette(byte *palette)
{
    if (palette == NULL) return;
    memcpy(cron_palette, palette, sizeof(cron_palette));
    cron_palette_valid = 1;
}

const byte *I_Cron_GetPalette(void)
{
    return cron_palette_valid ? cron_palette : NULL;
}

int I_GetPaletteIndex(int r, int g, int b)
{
    int best = 0, best_diff = 0x7fffffff;
    for (int i = 0; i < 256; ++i)
    {
        int dr = r - cron_palette[i * 3 + 0];
        int dg = g - cron_palette[i * 3 + 1];
        int db = b - cron_palette[i * 3 + 2];
        int diff = dr * dr + dg * dg + db * db;
        if (diff < best_diff) { best_diff = diff; best = i; if (!diff) break; }
    }
    return best;
}

/* I_FinishUpdate: the cart drives present(), so just mark a frame is ready. */
volatile int I_Cron_FrameReady = 0;
void I_FinishUpdate(void) { I_Cron_FrameReady = 1; }
void I_UpdateNoBlit(void) { }

void I_ReadScreen(pixel_t *scr)
{
    memcpy(scr, I_VideoBuffer, SCREENWIDTH * SCREENHEIGHT * sizeof(pixel_t));
}

void I_StartFrame(void) { }
void I_StartTic(void)   { }   /* input is fed from the cart's engine_input */
void I_StartDisplay(void) { }

void I_UpdateFracTic(void) { fractionaltic = 0; }

void I_BeginRead(void) { }
void I_EnableLoadingDisk(int xoffs, int yoffs) { (void)xoffs; (void)yoffs; }

void I_SetWindowTitle(const char *title) { (void)title; }
void I_InitWindowTitle(void) { }
void I_RegisterWindowIcon(const unsigned int *icon, int width, int height)
{ (void)icon; (void)width; (void)height; }
void I_InitWindowIcon(void) { }
void I_CheckIsScreensaver(void) { }
void I_SetGrabMouseCallback(grabmouse_callback_t func) { (void)func; }
void I_DisplayFPSDots(boolean dots_on) { (void)dots_on; }
void I_GetWindowPosition(int *x, int *y, int w, int h)
{ (void)w; (void)h; if (x) *x = 0; if (y) *y = 0; }
void I_ToggleVsync(void) { }

void I_BindVideoVariables(void) { }

// [cronopio] ENDOOM text screen: no text-mode console on Cronopio, so the
// exit-time ENDOOM display is a no-op.
void I_Endoom(byte *endoom_data) { (void)endoom_data; }

// [cronopio] Re-init graphics after a resolution/hires toggle. crispy->hires /
// widescreen are forced 0 (fixed 320x200), so the framebuffer never changes;
// no-op. (Called from m_crispy.c toggle hooks.)
void I_ReInitGraphics(void) { }
