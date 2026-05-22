/* Headless menu-navigation harness: taps a sequence of HID scancodes (each
 * held a few frames, then released) and screenshots the final framebuffer.
 * Used to drive DOOM's menus without an interactive host.
 *
 *   headless_nav cart.bin out.ppm sc1 sc2 sc3 ...
 */
#include "console.h"
#include "syscalls.h"
#include "cvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_frame_ms = 0;
uint64_t cronopio_platform_ticks_ms(void) { return g_frame_ms; }

#define HOLD 7
#define GAP  9

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s cart.bin out.ppm sc...\n", argv[0]); return 1; }
    int nkeys = argc - 3;
    int step = HOLD + GAP;
    int warm = 90;                 /* let the title settle */
    int frames = warm + nkeys * step + 40;

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
        memset(console.keys, 0, sizeof console.keys);
        int t = fr - warm;
        if (t >= 0) {
            int idx = t / step;
            int phase = t % step;
            if (idx < nkeys && phase < HOLD) {
                int sc = (int)strtol(argv[3 + idx], NULL, 0);
                console.keys[(sc>>3)&31] |= (uint8_t)(1u<<(sc&7));
            }
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
    printf("wrote %s (%d keys, %d frames)\n", argv[2], nkeys, frames);
    return 0;
}
