/* WAD-from-ROM shim implementation. Part of the cartridge unity TU. */
#include "wad_rom.h"
#include <cronopio.h>

int wad_open(wad_file_t *f) {
    f->base = cron_rom();
    f->size = cron_rom_size();
    f->pos  = 0;
    f->open = (f->base != 0 && f->size > 0);
    return f->open ? 0 : -1;
}

int wad_read(wad_file_t *f, void *dst, int n) {
    if (!f->open || n <= 0) return 0;
    uint32_t avail = f->size - f->pos;
    uint32_t want  = (uint32_t)n;
    if (want > avail) want = avail;
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = f->base + f->pos;
    for (uint32_t i = 0; i < want; ++i) d[i] = s[i];
    f->pos += want;
    return (int)want;
}

int wad_seek(wad_file_t *f, int offset, int whence) {
    if (!f->open) return -1;
    long base;
    switch (whence) {
        case WAD_SEEK_SET: base = 0;            break;
        case WAD_SEEK_CUR: base = (long)f->pos; break;
        case WAD_SEEK_END: base = (long)f->size; break;
        default: return -1;
    }
    long p = base + offset;
    if (p < 0) p = 0;
    if (p > (long)f->size) p = (long)f->size;
    f->pos = (uint32_t)p;
    return 0;
}

int wad_tell(const wad_file_t *f) { return (int)f->pos; }
int wad_eof (const wad_file_t *f) { return f->pos >= f->size; }
