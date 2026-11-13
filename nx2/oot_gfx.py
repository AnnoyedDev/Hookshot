"""view dists: hos_oot_gfx (host/oot_gfx.c) @ head of f_831f3200, per frame; obj detail per model @ LOD calcs (coef + dist scale)"""

MAIN = 0x80020000
BUILD_ID = "e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348"
SITE = MAIN + 0x31d3200
WORD = 0x39788008   # ldrb w8, [x0, #0xe20]
HOOK = "{ extern void hos_oot_gfx(Ctx *c); hos_oot_gfx(c); }   /* (nx2/oot_gfx.py) */"
# (off, word, s, model): ldr s<s>, [x<model>, #0x44], model LOD scale, fmul'd w/ LodStartDistCoefForConsole right after
LOD_SITES = (
    (0x7f0b8, 0xbd404723, 3, 25),
    (0x84dd0, 0xbd404703, 3, 24),
    (0x875dc, 0xbd404723, 3, 25),
    (0x1215480, 0xbd4046c1, 1, 22),
    (0x31d4b54, 0xbd404503, 3, 8),
    (0x31d50ec, 0xbd404683, 3, 20),
    (0x3628b8c, 0xbd404683, 3, 20),
    (0x369881c, 0xbd4046e1, 1, 23),
)
# same 8 calcs: ldr s<s>, [render, #0xd90] (MaxLevelDistScale, per frame copy); model reg as above
DIST_SITES = (
    (0x7f020, 0xbd4d9284, 4, 25),
    (0x84d3c, 0xbd4d9284, 4, 24),
    (0x87544, 0xbd4d9284, 4, 25),
    (0x12153ec, 0xbd4d938a, 10, 22),
    (0x31d4a94, 0xbd4d92a5, 5, 8),
    (0x31d5024, 0xbd4d9265, 5, 20),
    (0x3628af8, 0xbd4d9364, 4, 20),
    (0x3698760, 0xbd4d9105, 5, 23),
)


def lod_hook(s, model, fn="hos_oot_gfx_lod"):
    return ("{ extern void %s(Ctx *c, int s, uint64_t model); %s(c, %d, c->x[%d]); }"
            "   /* (nx2/oot_gfx.py) */" % (fn, fn, s, model))


def add(lifter, pc, text):
    lifter.after[pc] = (lifter.after[pc] + "\n" if pc in lifter.after else "") + text


def install(lifter, img, build_id):
    if build_id != BUILD_ID or img.word(SITE) != WORD:
        print("view distances: unsupported render setup code, left out")
        return 0
    add(lifter, SITE, HOOK)
    lod = 0
    for sites, fn in ((LOD_SITES, "hos_oot_gfx_lod"), (DIST_SITES, "hos_oot_gfx_dist")):
        for off, word, s, model in sites:
            if img.word(MAIN + off) == word:
                add(lifter, MAIN + off, lod_hook(s, model, fn))
                lod += 1
    print("view distances: render setup hooked, object detail at %d of %d LOD sites" % (lod, len(LOD_SITES) + len(DIST_SITES)))
    return 1
