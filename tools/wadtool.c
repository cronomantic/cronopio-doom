/* WAD inspection + merge helper for the Cronopio DOOM cart tooling.
 *
 * Written in C (not a scripting language) so the cart tooling adds no new
 * dependency: building a cart already needs a C compiler (clang, via the
 * Cronopio SDK build), so make_carts.sh just compiles this on first use.
 *
 *   wadtool identify <wad>
 *       Print "<type>\t<game>\t<runnable>\n":
 *         type     = IWAD | PWAD
 *         game     = doom | doom2 | tnt | plutonia | freedoom1 | freedoom2 |
 *                    freedm | chex | heretic | hexen | unknown
 *         runnable = 1 if the crispy-doom engine can run it, else 0
 *
 *   wadtool merge <out.wad> <iwad.wad> [pwad.wad ...]
 *       Concatenate the inputs' lumps in order (IWAD first, then each PWAD)
 *       into one IWAD. DOOM resolves a name to the LAST matching lump, so
 *       appended PWAD lumps override the IWAD's — reproducing vanilla `-file`
 *       order for maps, DEHACKED and by-name replacements. (DeuTex-style
 *       insertion of NEW flats/sprites into marker namespaces is NOT handled.)
 *
 * WAD format: 12-byte header (4-byte magic "IWAD"/"PWAD", int32 numlumps,
 * int32 dir offset, little-endian) + a directory of 16-byte entries
 * (int32 filepos, int32 size, 8-byte NUL-padded name).
 */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[9];     /* uppercased, NUL-terminated */
    int32_t size;
    const unsigned char *data;
} lump_t;

typedef struct {
    unsigned char *blob;
    long blen;
    char magic[5];
    int32_t numlumps;
    lump_t *lumps;
} wad_t;

static int32_t rd32(const unsigned char *p) {
    return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static void wr32(unsigned char *p, int32_t v) {
    p[0] = (unsigned char)(v & 0xff);
    p[1] = (unsigned char)((v >> 8) & 0xff);
    p[2] = (unsigned char)((v >> 16) & 0xff);
    p[3] = (unsigned char)((v >> 24) & 0xff);
}

static int read_wad(const char *path, wad_t *w) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[wadtool] cannot open %s\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 12) { fprintf(stderr, "[wadtool] %s: too small\n", path); fclose(f); return -1; }
    unsigned char *blob = malloc((size_t)len);
    if (!blob || fread(blob, 1, (size_t)len, f) != (size_t)len) {
        fprintf(stderr, "[wadtool] %s: read error\n", path);
        free(blob); fclose(f); return -1;
    }
    fclose(f);

    memset(w, 0, sizeof(*w));
    w->blob = blob; w->blen = len;
    memcpy(w->magic, blob, 4); w->magic[4] = '\0';
    if (strcmp(w->magic, "IWAD") != 0 && strcmp(w->magic, "PWAD") != 0) {
        fprintf(stderr, "[wadtool] %s: bad magic '%s'\n", path, w->magic);
        free(blob); return -1;
    }
    w->numlumps = rd32(blob + 4);
    int32_t infotableofs = rd32(blob + 8);
    if (w->numlumps < 0 || infotableofs < 0 ||
        (long)infotableofs + (long)w->numlumps * 16 > len) {
        fprintf(stderr, "[wadtool] %s: corrupt directory\n", path);
        free(blob); return -1;
    }
    w->lumps = calloc((size_t)w->numlumps, sizeof(lump_t));
    if (!w->lumps) { free(blob); return -1; }
    const unsigned char *dir = blob + infotableofs;
    for (int32_t i = 0; i < w->numlumps; i++) {
        int32_t filepos = rd32(dir + i * 16);
        int32_t size = rd32(dir + i * 16 + 4);
        const unsigned char *nm = dir + i * 16 + 8;
        lump_t *L = &w->lumps[i];
        int j = 0;
        for (; j < 8 && nm[j]; j++) L->name[j] = (char)toupper(nm[j]);
        L->name[j] = '\0';
        if (size > 0 && filepos >= 0 && (long)filepos + size <= len) {
            L->size = size; L->data = blob + filepos;
        } else {
            L->size = 0; L->data = NULL;  /* marker / out-of-range */
        }
    }
    return 0;
}

static void free_wad(wad_t *w) { free(w->lumps); free(w->blob); }

static int has(const wad_t *w, const char *name) {
    for (int32_t i = 0; i < w->numlumps; i++)
        if (strcmp(w->lumps[i].name, name) == 0) return 1;
    return 0;
}

/* lowercase basename without extension into out (size cap). */
static void stem_of(const char *path, char *out, size_t cap) {
    const char *b = path, *p;
    for (p = path; *p; p++) if (*p == '/' || *p == '\\') b = p + 1;
    size_t n = 0;
    for (p = b; *p && *p != '.' && n + 1 < cap; p++) out[n++] = (char)tolower(*p);
    out[n] = '\0';
}

static const char *resolve_game(const char *path, const wad_t *w) {
    char stem[256];
    stem_of(path, stem, sizeof(stem));

    if (has(w, "FREEDOOM")) {
        if (has(w, "FREEDM")) return "freedm";
        return has(w, "MAP01") ? "freedoom2" : "freedoom1";
    }
    if (has(w, "FREEDM")) return "freedm";

    /* Raven games — NOT runnable on the doom engine. */
    if (has(w, "MUS_E1M1") || strstr(stem, "heretic")) return "heretic";
    if (has(w, "CLUS1MSG") || has(w, "WINNOWR") ||
        strstr(stem, "hexen") || strstr(stem, "hexdd")) return "hexen";

    if (has(w, "MAP01")) {  /* DOOM 2 family */
        if (strstr(stem, "tnt")) return "tnt";
        if (strstr(stem, "plut")) return "plutonia";
        return "doom2";
    }
    if (has(w, "E1M1")) {   /* DOOM 1 family */
        if (strstr(stem, "chex")) return "chex";
        return "doom";
    }
    return "unknown";
}

static int is_runnable(const char *game) {
    static const char *ok[] = {"doom", "doom2", "tnt", "plutonia",
                               "freedoom1", "freedoom2", "freedm", "chex"};
    for (size_t i = 0; i < sizeof(ok) / sizeof(ok[0]); i++)
        if (strcmp(game, ok[i]) == 0) return 1;
    return 0;
}

static int cmd_identify(const char *path) {
    wad_t w;
    if (read_wad(path, &w) != 0) return 1;
    const char *game = resolve_game(path, &w);
    int runnable = (strcmp(w.magic, "IWAD") == 0 && is_runnable(game)) ? 1 : 0;
    printf("%s\t%s\t%d\n", w.magic, game, runnable);
    free_wad(&w);
    return 0;
}

static int cmd_merge(const char *out_path, int n, char **inputs) {
    wad_t *wads = calloc((size_t)n, sizeof(wad_t));
    if (!wads) return 1;
    int32_t total = 0;
    for (int i = 0; i < n; i++) {
        if (read_wad(inputs[i], &wads[i]) != 0) {
            for (int k = 0; k < i; k++) free_wad(&wads[k]);
            free(wads); return 1;
        }
        total += wads[i].numlumps;
    }

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        fprintf(stderr, "[wadtool] cannot write %s\n", out_path);
        for (int i = 0; i < n; i++) free_wad(&wads[i]);
        free(wads); return 1;
    }

    /* Layout: header(12) + lump data + directory. */
    int32_t cur = 12;
    for (int i = 0; i < n; i++)
        for (int32_t j = 0; j < wads[i].numlumps; j++)
            cur += wads[i].lumps[j].size;
    int32_t infotableofs = cur;

    unsigned char hdr[12];
    memcpy(hdr, "IWAD", 4);
    wr32(hdr + 4, total);
    wr32(hdr + 8, infotableofs);
    fwrite(hdr, 1, 12, f);

    for (int i = 0; i < n; i++)
        for (int32_t j = 0; j < wads[i].numlumps; j++)
            if (wads[i].lumps[j].size > 0)
                fwrite(wads[i].lumps[j].data, 1, (size_t)wads[i].lumps[j].size, f);

    cur = 12;
    for (int i = 0; i < n; i++) {
        for (int32_t j = 0; j < wads[i].numlumps; j++) {
            lump_t *L = &wads[i].lumps[j];
            unsigned char ent[16];
            if (L->size > 0) { wr32(ent, cur); cur += L->size; }
            else { wr32(ent, 0); }
            wr32(ent + 4, L->size);
            memset(ent + 8, 0, 8);
            memcpy(ent + 8, L->name, strnlen(L->name, 8));
            fwrite(ent, 1, 16, f);
        }
    }
    fclose(f);
    fprintf(stderr, "[wadtool] merged %d WAD(s), %d lumps -> %s\n",
            n, (int)total, out_path);
    for (int i = 0; i < n; i++) free_wad(&wads[i]);
    free(wads);
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 3 && strcmp(argv[1], "identify") == 0 && argc == 3)
        return cmd_identify(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "merge") == 0)
        return cmd_merge(argv[2], argc - 3, &argv[3]);
    fprintf(stderr,
        "usage:\n"
        "  wadtool identify <wad>\n"
        "  wadtool merge <out.wad> <iwad.wad> [pwad.wad ...]\n");
    return 2;
}
