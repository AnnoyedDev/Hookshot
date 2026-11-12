/* view dists, port of hotoni's Graphics-Menu per-frame hook (exl inline @ main+0x31d3200); menu hooks dropped */
#include "hos.h"
#include "../gpu/vk/vk_api.h"
#include <string.h>

int rx_option(int id, int otherwise);

#define MAIN(off) (0x80020000ULL + (off))
#define FOLIAGE_ROOT MAIN(0x4e22540)
#define SCENE_ROOT   MAIN(0x4e3e258)
#define QUALITY_ROOT MAIN(0x4e97dd8)
#define VT_ROOT      MAIN(0x4b24800)
#define VT_ROOT_SUB  MAIN(0x4b24a70)
#define VT_HOLDER    MAIN(0x4bf9010)
#define VT_FOLIAGE   MAIN(0x4b23228)
#define VT_FOL_PARAM MAIN(0x4b235d0)
#define VT_SCENE     MAIN(0x4b895b0)
#define VT_RENDER    MAIN(0x4b8bc00)
#define VT_LOD       MAIN(0x4b87a88)
#define VT_CHAR_SH   MAIN(0x4b8a3e8)
#define VT_CHAR_SH_P MAIN(0x4b86828)
#define VT_SHADOW    MAIN(0x4b8a0a8)

typedef struct {
    uint64_t at;
    int n;
    float orig[8];
    float last;
    int valid;
} Scaled;

static float getf(Ctx *c, uint64_t a) { uint32_t b = recomp_load32(c, a); float f; memcpy(&f, &b, 4); return f; }
static void putf(Ctx *c, uint64_t a, float f) { uint32_t b; memcpy(&b, &f, 4); recomp_store32(c, a, b); }

/* orig * scale each frame, no compounding; recapture on new obj or game write */
static void scale_floats(Ctx *c, Scaled *s, uint64_t a, int n, float scale) {
    if (!s->valid || s->at != a || getf(c, a) != s->last) {
        s->at = a, s->n = n, s->valid = 1;
        for (int i = 0; i < n; i++) s->orig[i] = getf(c, a + 4 * i);
    }
    for (int i = 0; i < s->n; i++) {
        const float v = s->orig[i] * scale;
        if (getf(c, a + 4 * i) != v) putf(c, a + 4 * i, v);
    }
    s->last = getf(c, a);
}

static uint64_t linked(Ctx *c, uint64_t slot, uint64_t vt) {
    const uint64_t o = recomp_load64(c, slot);
    return o && recomp_load64(c, o) == vt ? o : 0;
}

static Scaled fol_a, fol_b, grass, shadow, lod_s, char_sh;
static int active, contact_off, quality_saved;
static uint32_t quality_game;

void hos_oot_gfx(Ctx *c) {
    const int fol = rx_option(HOS_OPT_GFX_FOLIAGE, 100), obj = rx_option(HOS_OPT_GFX_OBJECTS, 100);
    const int gra = rx_option(HOS_OPT_GFX_GRASS, 100), sha = rx_option(HOS_OPT_GFX_SHADOWS, 100);
    const int contact = rx_option(HOS_OPT_GFX_CONTACT_SHADOWS, 1), low = rx_option(HOS_OPT_GFX_BASE_QUALITY, 0);
    if (!active && fol == 100 && obj == 100 && gra == 100 && sha == 100 && contact && !low) return;   /* untouched until 1st chg */
    active = 1;
    const float F = fol / 100.0f, O = obj / 100.0f, G = gra / 100.0f, S = sha / 100.0f;

    const uint64_t root = linked(c, FOLIAGE_ROOT, VT_ROOT);
    const uint64_t sub = root ? linked(c, root + 0xf0, VT_ROOT_SUB) : 0;
    const uint64_t holder = sub ? linked(c, sub + 0x2a90, VT_HOLDER) : 0;
    const uint64_t foliage = holder ? linked(c, holder + 0x150, VT_FOLIAGE) : 0;
    if (foliage) {
        const uint64_t a = linked(c, foliage + 0x40, VT_FOL_PARAM), b = linked(c, foliage + 0x48, VT_FOL_PARAM);
        if (a) scale_floats(c, &fol_a, a + 0x38, 8, F); else fol_a.valid = 0;
        if (b) scale_floats(c, &fol_b, b + 0x38, 8, F); else fol_b.valid = 0;
        scale_floats(c, &grass, foliage + 0x68, 1, G);
    }

    const uint64_t scene = linked(c, SCENE_ROOT, VT_SCENE);
    const uint64_t render = scene ? linked(c, scene + 0x1e8, VT_RENDER) : 0;
    if (render) {
        const uint64_t h = linked(c, render + 0xd50, VT_HOLDER), lod = h ? linked(c, h + 0x150, VT_LOD) : 0;
        if (lod) scale_floats(c, &lod_s, lod + 0x48, 3, O); else lod_s.valid = 0;
        const uint64_t ch = linked(c, render + 0xba8, VT_CHAR_SH);
        const uint64_t chh = ch ? linked(c, ch + 0xc18, VT_HOLDER) : 0;
        const uint64_t chp = chh ? linked(c, chh + 0x150, VT_CHAR_SH_P) : 0;
        if (chp) scale_floats(c, &char_sh, chp + 0x90, 2, S); else char_sh.valid = 0;   /* game: 30m / 25m */
        const uint64_t sh = linked(c, render + 0xb30, VT_SHADOW);
        if (sh) scale_floats(c, &shadow, sh + 0xb4, 1, S); else shadow.valid = 0;   /* game: 160m */
        const uint64_t ss = recomp_load64(c, render + 0xc08);
        if (ss) {
            const uint64_t flags = recomp_load8(c, ss + 0x318);
            if (!contact && (flags & 1)) recomp_store8(c, ss + 0x318, flags & ~1ULL), contact_off = 1;
            else if (contact && contact_off) recomp_store8(c, ss + 0x318, flags | 1), contact_off = 0;
        }
    }

    const uint64_t q = recomp_load64(c, QUALITY_ROOT);   /* +0x20 = 0: game's handheld preset */
    if (q) {
        if (low) {
            if (!quality_saved) quality_game = recomp_load32(c, q + 0x20), quality_saved = 1;
            recomp_store32(c, q + 0x20, 0);
        } else if (quality_saved) {
            recomp_store32(c, q + 0x20, quality_game), quality_saved = 0;
        }
    }
}
