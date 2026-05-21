/* ROM-backed WAD file backend — replaces w_file_stdc.c.
 *
 * Cronopio has no filesystem. The IWAD is baked into the cartridge ROM via
 * `cronopio-cc --rom=<wad>` and exposed through cron_rom()/cron_rom_size().
 * Crispy's W_AddFile -> W_OpenFile -> stdc_wad_file.OpenFile ends up here; we
 * ignore the path and serve the single ROM image. Reads are plain memcpy from
 * the mapped ROM (we set wad.mapped so W_CacheLumpNum can map directly).
 */
#include <cronopio.h>

#include "doomtype.h"
#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

#include <string.h>

typedef struct
{
    wad_file_t wad;
} rom_wad_file_t;

static wad_file_t *W_ROM_OpenFile(const char *path)
{
    const byte *rom = (const byte *)cron_rom();
    unsigned int len = (unsigned int)cron_rom_size();

    if (rom == NULL || len < 12)
    {
        return NULL;
    }

    rom_wad_file_t *result = Z_Malloc(sizeof(rom_wad_file_t), PU_STATIC, 0);
    result->wad.file_class = &stdc_wad_file;
    /* mapped != NULL lets W_CacheLumpNum reference the ROM in place. */
    result->wad.mapped = (byte *)rom;
    result->wad.length = len;
    result->wad.path = M_StringDuplicate(path ? path : "ROM");

    return &result->wad;
}

static void W_ROM_CloseFile(wad_file_t *wad)
{
    Z_Free(wad);
}

static size_t W_ROM_Read(wad_file_t *wad, unsigned int offset,
                         void *buffer, size_t buffer_len)
{
    const byte *rom = (const byte *)cron_rom();
    size_t avail;

    if (rom == NULL || offset >= wad->length)
    {
        return 0;
    }

    avail = wad->length - offset;
    if (buffer_len > avail)
    {
        buffer_len = avail;
    }

    memcpy(buffer, rom + offset, buffer_len);
    return buffer_len;
}

/* Crispy references `stdc_wad_file` by name (w_file.c picks it). */
wad_file_class_t stdc_wad_file =
{
    W_ROM_OpenFile,
    W_ROM_CloseFile,
    W_ROM_Read,
};
