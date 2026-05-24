/* Headless PAD-injection harness: holds a sequence of 12-button pad masks
 * (each held a few frames, then released) on cron_pad(0) and screenshots the
 * final framebuffer. Since the cart is now pad-only (no raw keyboard), this is
 * the way to drive its input from the command line.
 *
 *   headless_pad cart.bin out.ppm mask1 mask2 ...      (masks are hex/dec)
 *
 * Bits (CRON_BTN_*): UP=0x001 DOWN=0x002 LEFT=0x004 RIGHT=0x008
 *                    A=0x010 B=0x020 X=0x040 Y=0x080
 *                    L=0x100 R=0x200 START=0x400 SELECT=0x800
 */
#include "console.h"
#include "syscalls.h"
#include "cvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_frame_ms = 0;
uint64_t cronopio_platform_ticks_ms(void) { return g_frame_ms; }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s cart.bin out.ppm mask...\n", argv[0]); return 1; }
    int HOLD = getenv("HOLD") ? atoi(getenv("HOLD")) : 8;   /* frames each mask is held */
    int GAP  = getenv("GAP")  ? atoi(getenv("GAP"))  : 10;  /* frames released between masks */
    int nmask = argc - 3;
    int step = HOLD + GAP;
    int warm = 90;                 /* let the title settle */
    int frames = warm + nmask * step + 40;
    /* HOLDALL=1 holds mask[0] every frame; FRAMES=N sets the exact final frame.
     * (Used to A/B sub-tic interpolation: same frame, capped vs uncapped.) */
    int holdall = getenv("HOLDALL") ? 1 : 0;
    if (getenv("FRAMES")) frames = atoi(getenv("FRAMES"));

    FILE* f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
    uint8_t* blob = malloc(n); if (fread(blob,1,n,f)!=(size_t)n){return 1;} fclose(f);

    struct cvm_image img;
    if (cvm_load(blob, n, &img) != CVM_OK) { fprintf(stderr,"load fail\n"); return 1; }
    static cronopio_console_t console;
    cronopio_console_init(&console);
    cronopio_resolve_video_regions(&img, &console);
    cronopio_syscalls_install(&img, &console);
    int32_t ret=0; cvm_run(&img, &ret);

    for (int fr = 0; fr < frames && !console.cart_exited; ++fr) {
        g_frame_ms = (uint64_t)fr * 1000u / 60u;
        console.pad_cur[0] = 0;
        int t = fr - warm;
        if (holdall && fr >= warm) console.pad_cur[0] = (uint32_t)strtoul(argv[3], NULL, 0);
        else if (t >= 0) {
            int idx = t / step;
            int phase = t % step;
            if (idx < nmask && phase < HOLD)
                console.pad_cur[0] = (uint32_t)strtoul(argv[3 + idx], NULL, 0);
        }
        cronopio_console_begin_frame(&console);
        if (console.frame_fn_index > 0) { int32_t r=0; cvm_call(&img,(uint32_t)console.frame_fn_index,NULL,0,&r); }
        cronopio_console_end_frame(&console);
    }

    static uint32_t rgba[CRONOPIO_FB_BYTES];
    cronopio_console_blit_rgba(&console, img.heap, rgba);
    FILE* p = fopen(argv[2], "wb");
    fprintf(p, "P6\n%d %d\n255\n", CRONOPIO_SCREEN_W, CRONOPIO_SCREEN_H);
    for (int i=0;i<CRONOPIO_FB_BYTES;++i){ uint32_t px=rgba[i]; uint8_t r[3]={px>>16,px>>8,px}; fwrite(r,1,3,p); }
    fclose(p);
    printf("wrote %s (%d masks, %d frames)\n", argv[2], nmask, frames);
    return 0;
}
