/* WAD-from-ROM shim.
 *
 * Cronopio has no filesystem; the IWAD/PWAD is baked into the cartridge ROM
 * with `cronopio-cc --rom=doom.wad` and exposed via cron_rom()/cron_rom_size().
 * A DOOM source port's file I/O (fopen/fread/fseek on the WAD, or PureDOOM's
 * doom_set_file_io callbacks) is wired to these in-memory file handles.
 *
 * Only reading is supported (the ROM is read-only). Savegames/config, if you
 * want them, go through cron_save_read/cron_save_write instead.
 */
#ifndef CRONO_DOOM_WAD_ROM_H
#define CRONO_DOOM_WAD_ROM_H

#include <stdint.h>

typedef struct {
    const uint8_t *base;   /* cron_rom()        */
    uint32_t       size;   /* cron_rom_size()   */
    uint32_t       pos;    /* read cursor       */
    int            open;
} wad_file_t;

enum { WAD_SEEK_SET = 0, WAD_SEEK_CUR = 1, WAD_SEEK_END = 2 };

/* Open the cartridge ROM as a readable file. Returns 0 on success, -1 if no
 * ROM was baked in (cron_rom() == NULL). */
int wad_open(wad_file_t *f);
int wad_read(wad_file_t *f, void *dst, int n);   /* returns bytes read */
int wad_seek(wad_file_t *f, int offset, int whence);
int wad_tell(const wad_file_t *f);
int wad_eof(const wad_file_t *f);

#endif /* CRONO_DOOM_WAD_ROM_H */
