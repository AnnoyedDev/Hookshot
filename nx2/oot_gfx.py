"""view dists: hos_oot_gfx (host/oot_gfx.c) @ head of f_831f3200, per frame"""

MAIN = 0x80020000
BUILD_ID = "e0f3ccfaf50065ae4ea01eb5aeb13b1a45da3348"
SITE = MAIN + 0x31d3200
WORD = 0x39788008   # ldrb w8, [x0, #0xe20]
HOOK = "{ extern void hos_oot_gfx(Ctx *c); hos_oot_gfx(c); }   /* (nx2/oot_gfx.py) */"


def install(lifter, img, build_id):
    if build_id != BUILD_ID or img.word(SITE) != WORD:
        print("view distances: unsupported render setup code, left out")
        return 0
    lifter.after[SITE] = (lifter.after[SITE] + "\n" if SITE in lifter.after else "") + HOOK
    print("view distances: render setup hooked")
    return 1
