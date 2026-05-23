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
 *       Merge the PWADs into the IWAD (DeuTex/`-merge` semantics) and write one
 *       IWAD. This is a port of crispy-doom's w_merge.c DoMerge applied offline
 *       and iteratively (one PWAD at a time, as the engine's W_MergeFile does):
 *         - New/replacement FLATS are inserted into the IWAD's F_START..F_END
 *           range (from the PWAD's F_/FF_ section) so the contiguous flat range
 *           vanilla relies on stays intact.
 *         - New/replacement SPRITES are merged per frame/rotation into the
 *           S_START..S_END range (full sprite-frame logic from w_merge.c).
 *         - Everything else (maps, TEXTURE1/PNAMES, sounds, music, DEHACKED,
 *           by-name patch replacements) is appended; DOOM resolves a name to
 *           the LAST match, so the PWAD wins — vanilla `-file` order.
 *       For a maps-only / by-name-replacement PWAD (no F_/S_ sections) this
 *       reduces to plain concatenation, identical to the old behaviour.
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

/* ---- DeuTex-style namespace merge (port of crispy-doom w_merge.c) -------- */

typedef struct { lump_t **lumps; int numlumps; } searchlist_t;

typedef struct {
    char sprname[4];
    char frame;
    lump_t *angle_lumps[8];
} sprite_frame_t;

static searchlist_t iwad, pwad;
static searchlist_t iwad_flats, iwad_sprites, pwad_flats, pwad_sprites;
static sprite_frame_t *sprite_frames;
static int num_sprite_frames, sprite_frames_alloced;

/* Lump names are uppercased on read and the literals here are uppercase, so a
 * plain strncmp suffices (no need for POSIX strncasecmp / <strings.h>). */
static int nmeq(const char *a, const char *b) { return strncmp(a, b, 8) == 0; }

static int find_in_list(searchlist_t *list, const char *name) {
    for (int i = 0; i < list->numlumps; ++i)
        if (nmeq(list->lumps[i]->name, name)) return i;
    return -1;
}

static int setup_list(searchlist_t *list, searchlist_t *src,
                      const char *s1, const char *e1,
                      const char *s2, const char *e2) {
    list->numlumps = 0;
    int start = find_in_list(src, s1);
    if (s2 && start < 0) start = find_in_list(src, s2);
    if (start >= 0) {
        int end = find_in_list(src, e1);
        if (e2 && end < 0) end = find_in_list(src, e2);
        if (end > start) {
            list->lumps = src->lumps + start + 1;
            list->numlumps = end - start - 1;
            return 1;
        }
    }
    return 0;
}

static int setup_lists(void) {
    if (!setup_list(&iwad_flats, &iwad, "F_START", "F_END", NULL, NULL)) return 0;
    if (!setup_list(&iwad_sprites, &iwad, "S_START", "S_END", NULL, NULL)) return 0;
    setup_list(&pwad_flats, &pwad, "F_START", "F_END", "FF_START", "FF_END");
    setup_list(&pwad_sprites, &pwad, "S_START", "S_END", "SS_START", "SS_END");
    return 1;
}

static void init_sprite_list(void) {
    if (!sprite_frames) {
        sprite_frames_alloced = 128;
        sprite_frames = malloc(sizeof(*sprite_frames) * sprite_frames_alloced);
    }
    num_sprite_frames = 0;
}

static int valid_sprite_name(const char *name) {
    if (!name[0] || !name[1] || !name[2] || !name[3]) return 0;
    if (!name[4] || !isdigit((unsigned char)name[5])) return 0;
    if (name[6] && !isdigit((unsigned char)name[7])) return 0;
    return 1;
}

static sprite_frame_t *find_sprite_frame(const char *name, int frame) {
    for (int i = 0; i < num_sprite_frames; ++i) {
        sprite_frame_t *cur = &sprite_frames[i];
        if (!strncmp(cur->sprname, name, 4) && cur->frame == frame) return cur;
    }
    if (num_sprite_frames >= sprite_frames_alloced) {
        sprite_frames_alloced *= 2;
        sprite_frames = realloc(sprite_frames,
                                sprite_frames_alloced * sizeof(*sprite_frames));
    }
    sprite_frame_t *r = &sprite_frames[num_sprite_frames++];
    memcpy(r->sprname, name, 4);
    r->frame = (char)frame;
    for (int i = 0; i < 8; ++i) r->angle_lumps[i] = NULL;
    return r;
}

static int sprite_lump_needed(lump_t *lump) {
    if (!valid_sprite_name(lump->name)) return 1;
    sprite_frame_t *sp = find_sprite_frame(lump->name, lump->name[4]);
    int a = lump->name[5] - '0';
    if (a == 0) { for (int i = 0; i < 8; ++i) if (sp->angle_lumps[i] == lump) return 1; }
    else if (sp->angle_lumps[a - 1] == lump) return 1;
    if (lump->name[6] == '\0') return 0;
    sp = find_sprite_frame(lump->name, lump->name[6]);
    a = lump->name[7] - '0';
    if (a == 0) { for (int i = 0; i < 8; ++i) if (sp->angle_lumps[i] == lump) return 1; }
    else if (sp->angle_lumps[a - 1] == lump) return 1;
    return 0;
}

static void add_sprite_lump(lump_t *lump) {
    if (!valid_sprite_name(lump->name)) return;
    sprite_frame_t *sp = find_sprite_frame(lump->name, lump->name[4]);
    int a = lump->name[5] - '0';
    if (a == 0) { for (int i = 0; i < 8; ++i) sp->angle_lumps[i] = lump; }
    else sp->angle_lumps[a - 1] = lump;
    if (lump->name[6] == '\0') return;
    sp = find_sprite_frame(lump->name, lump->name[6]);
    a = lump->name[7] - '0';
    if (a == 0) { for (int i = 0; i < 8; ++i) sp->angle_lumps[i] = lump; }
    else sp->angle_lumps[a - 1] = lump;
}

static void generate_sprite_list(void) {
    init_sprite_list();
    for (int i = 0; i < iwad_sprites.numlumps; ++i) add_sprite_lump(iwad_sprites.lumps[i]);
    for (int i = 0; i < pwad_sprites.numlumps; ++i) add_sprite_lump(pwad_sprites.lumps[i]);
}

enum { SEC_NORMAL, SEC_FLATS, SEC_SPRITES };

/* Merge one PWAD into iwad_lumps; returns a fresh lump_t* array (caller frees). */
static lump_t **do_merge(lump_t **iwad_lumps, int iwad_n,
                         lump_t **pwad_lumps, int pwad_n, int *out_n) {
    iwad.lumps = iwad_lumps; iwad.numlumps = iwad_n;
    pwad.lumps = pwad_lumps; pwad.numlumps = pwad_n;
    if (!setup_lists()) { *out_n = -1; return NULL; }  /* no F_/S_ in IWAD */
    generate_sprite_list();

    lump_t **out = calloc((size_t)(iwad_n + pwad_n), sizeof(lump_t *));
    int n = 0, sec = SEC_NORMAL;

    for (int i = 0; i < iwad_n; ++i) {
        lump_t *lump = iwad_lumps[i];
        if (sec == SEC_NORMAL) {
            if (nmeq(lump->name, "F_START")) sec = SEC_FLATS;
            else if (nmeq(lump->name, "S_START")) sec = SEC_SPRITES;
            out[n++] = lump;
        } else if (sec == SEC_FLATS) {
            if (nmeq(lump->name, "F_END")) {
                for (int k = 0; k < pwad_flats.numlumps; ++k) out[n++] = pwad_flats.lumps[k];
                out[n++] = lump;
                sec = SEC_NORMAL;
            } else if (find_in_list(&pwad_flats, lump->name) < 0) {
                out[n++] = lump;  /* IWAD-only flat: keep */
            }
        } else { /* SEC_SPRITES */
            if (nmeq(lump->name, "S_END")) {
                for (int k = 0; k < pwad_sprites.numlumps; ++k)
                    if (sprite_lump_needed(pwad_sprites.lumps[k])) out[n++] = pwad_sprites.lumps[k];
                out[n++] = lump;
                sec = SEC_NORMAL;
            } else if (sprite_lump_needed(lump)) {
                out[n++] = lump;  /* IWAD sprite frame not replaced: keep */
            }
        }
    }

    sec = SEC_NORMAL;
    for (int i = 0; i < pwad_n; ++i) {
        lump_t *lump = pwad_lumps[i];
        if (sec == SEC_NORMAL) {
            if (nmeq(lump->name, "F_START") || nmeq(lump->name, "FF_START")) sec = SEC_FLATS;
            else if (nmeq(lump->name, "S_START") || nmeq(lump->name, "SS_START")) sec = SEC_SPRITES;
            else out[n++] = lump;  /* maps, TEXTUREx/PNAMES, sounds, DEH, ... */
        } else if (sec == SEC_FLATS) {
            if (nmeq(lump->name, "FF_END") || nmeq(lump->name, "F_END")) sec = SEC_NORMAL;
        } else { /* SEC_SPRITES */
            if (nmeq(lump->name, "SS_END") || nmeq(lump->name, "S_END")) sec = SEC_NORMAL;
        }
    }

    *out_n = n;
    return out;
}

static int write_wad(const char *out_path, lump_t **lumps, int n) {
    FILE *f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "[wadtool] cannot write %s\n", out_path); return 1; }

    int32_t cur = 12;
    for (int i = 0; i < n; ++i) cur += lumps[i]->size;
    int32_t infotableofs = cur;

    unsigned char hdr[12];
    memcpy(hdr, "IWAD", 4);
    wr32(hdr + 4, n);
    wr32(hdr + 8, infotableofs);
    fwrite(hdr, 1, 12, f);

    for (int i = 0; i < n; ++i)
        if (lumps[i]->size > 0) fwrite(lumps[i]->data, 1, (size_t)lumps[i]->size, f);

    cur = 12;
    for (int i = 0; i < n; ++i) {
        unsigned char ent[16];
        if (lumps[i]->size > 0) { wr32(ent, cur); cur += lumps[i]->size; }
        else wr32(ent, 0);
        wr32(ent + 4, lumps[i]->size);
        memset(ent + 8, 0, 8);
        memcpy(ent + 8, lumps[i]->name, strnlen(lumps[i]->name, 8));
        fwrite(ent, 1, 16, f);
    }
    fclose(f);
    return 0;
}

/* Concatenation fallback (when DeuTex merge can't run): all lumps in order. */
static lump_t **concat(lump_t **acc, int acc_n, wad_t *p, int *out_n) {
    lump_t **out = realloc(acc, (size_t)(acc_n + p->numlumps) * sizeof(lump_t *));
    for (int32_t i = 0; i < p->numlumps; ++i) out[acc_n + i] = &p->lumps[i];
    *out_n = acc_n + p->numlumps;
    return out;
}

static int cmd_merge(const char *out_path, int n, char **inputs) {
    wad_t *wads = calloc((size_t)n, sizeof(wad_t));
    if (!wads) return 1;
    for (int i = 0; i < n; i++) {
        if (read_wad(inputs[i], &wads[i]) != 0) {
            for (int k = 0; k < i; k++) free_wad(&wads[k]);
            free(wads); return 1;
        }
    }

    /* acc starts as the IWAD's lumps (as pointers). */
    int acc_n = wads[0].numlumps;
    lump_t **acc = calloc((size_t)acc_n, sizeof(lump_t *));
    for (int i = 0; i < acc_n; ++i) acc[i] = &wads[0].lumps[i];

    int deutex = 0, concatd = 0;
    for (int i = 1; i < n; ++i) {
        lump_t **pptr = calloc((size_t)wads[i].numlumps, sizeof(lump_t *));
        for (int32_t j = 0; j < wads[i].numlumps; ++j) pptr[j] = &wads[i].lumps[j];

        int merged_n = 0;
        lump_t **merged = do_merge(acc, acc_n, pptr, wads[i].numlumps, &merged_n);
        if (merged) {
            free(acc); acc = merged; acc_n = merged_n; deutex++;
        } else {
            /* IWAD has no flat/sprite sections — fall back to concatenation. */
            acc = concat(acc, acc_n, &wads[i], &acc_n); concatd++;
        }
        free(pptr);
    }

    int rc = write_wad(out_path, acc, acc_n);
    fprintf(stderr, "[wadtool] merged %d WAD(s) -> %d lumps (%d DeuTex, %d concat) -> %s\n",
            n, acc_n, deutex, concatd, out_path);
    free(acc); free(sprite_frames); sprite_frames = NULL;
    for (int i = 0; i < n; i++) free_wad(&wads[i]);
    free(wads);
    return rc;
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "identify") == 0)
        return cmd_identify(argv[2]);
    if (argc >= 4 && strcmp(argv[1], "merge") == 0)
        return cmd_merge(argv[2], argc - 3, &argv[3]);
    fprintf(stderr,
        "usage:\n"
        "  wadtool identify <wad>\n"
        "  wadtool merge <out.wad> <iwad.wad> [pwad.wad ...]\n");
    return 2;
}
