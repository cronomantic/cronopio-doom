/* Headless keyboard-injection check for the cron_key -> DOOM event path.
 *
 * Same frame harness as Cronopio's headless runner, but it holds a chosen
 * HID scancode down (sets the console key bitmap) after a warmup so the cart's
 * engine_input() sees the edge via cron_key() and posts the DOOM event. Used
 * to confirm keyboard control works end-to-end without an interactive host.
 *
 *   headless_key cart.bin frames scancode_hex [out.ppm]
 *   e.g. headless_key doom.bin 200 0x29   # hold ESC -> main menu opens
 */
#include "console.h"
#include "syscalls.h"
#include "cvm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t g_frame_ms = 0;
uint64_t cronopio_platform_ticks_ms(void) { return g_frame_ms; }

static uint8_t* slurp(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    uint8_t* buf = (uint8_t*)malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f); *out_len = (size_t)n; return buf;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s cart.bin frames scancode_hex [out.ppm]\n", argv[0]); return 1; }
    int frames = atoi(argv[2]);
    int sc = (int)strtol(argv[3], NULL, 0);
    int hold_from = frames / 3;   /* warmup, then hold the key for the rest */

    size_t blob_len = 0; uint8_t* blob = slurp(argv[1], &blob_len);
    if (!blob) return 1;

    struct cvm_image img;
    if (cvm_load(blob, blob_len, &img) != CVM_OK) { fprintf(stderr, "load failed\n"); return 1; }

    static cronopio_console_t console;
    cronopio_console_init(&console);
    cronopio_resolve_video_regions(&img, &console);
    cronopio_syscalls_install(&img, &console);

    int32_t ret = 0;
    cvm_run(&img, &ret);

    for (int f = 0; f < frames && !console.cart_exited; ++f) {
        g_frame_ms = (uint64_t)f * 1000u / 60u;
        /* Hold the scancode down for the back two-thirds of the run. */
        memset(console.keys, 0, sizeof console.keys);
        console.pad_cur[0] = 0;
        if (f >= hold_from) {
            if (sc & 0x10000)              /* bit 16 set: low 8 bits are pad bits */
                console.pad_cur[0] = (uint32_t)(sc & 0xFF);
            else
                console.keys[(sc >> 3) & 31] |= (uint8_t)(1u << (sc & 7));
        }

        cronopio_console_begin_frame(&console);
        if (console.frame_fn_index > 0) {
            int32_t fr = 0;
            cvm_call(&img, (uint32_t)console.frame_fn_index, NULL, 0, &fr);
        }
        cronopio_console_end_frame(&console);
    }

    if (argc >= 5) {
        static uint32_t rgba[CRONOPIO_FB_BYTES];
        cronopio_console_blit_rgba(&console, img.heap, rgba);
        FILE* p = fopen(argv[4], "wb");
        if (p) {
            fprintf(p, "P6\n%d %d\n255\n", CRONOPIO_SCREEN_W, CRONOPIO_SCREEN_H);
            for (int i = 0; i < CRONOPIO_FB_BYTES; ++i) {
                uint32_t px = rgba[i];
                uint8_t rgb[3] = { (uint8_t)(px>>16), (uint8_t)(px>>8), (uint8_t)px };
                fwrite(rgb, 1, 3, p);
            }
            fclose(p);
            printf("wrote %s\n", argv[4]);
        }
    }
    return 0;
}
