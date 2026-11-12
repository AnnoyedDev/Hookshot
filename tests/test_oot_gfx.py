"""view dists: install gate + host apply on fake guest mem (no compounding, game writes, restore, vt chk, defaults = no access)

python -I tests/test_oot_gfx.py [--cc gcc]
"""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

RUN_ALL_ARGS = ["--cc", "gcc"]
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "nx2"))
import oot_gfx  # noqa: E402


class InstallTests(unittest.TestCase):
    def install(self, word=oot_gfx.WORD, build_id=oot_gfx.BUILD_ID, existing=None, lod_words=None):
        lods = {oot_gfx.MAIN + off: w for off, w, _, _ in oot_gfx.LOD_SITES}
        lods.update(lod_words or {})

        class Lifter:
            after = dict(existing or {})

        class Image:
            def word(self, pc):
                if pc == oot_gfx.SITE:
                    return word
                assert pc in lods
                return lods[pc]
        lifter = Lifter()
        return oot_gfx.install(lifter, Image(), build_id), lifter.after

    def test_supported_site_appends(self):
        count, after = self.install(existing={oot_gfx.SITE: "prior();"})
        self.assertEqual(count, 1)
        self.assertEqual(after[oot_gfx.SITE], "prior();\n" + oot_gfx.HOOK)
        for off, _, s, model in oot_gfx.LOD_SITES:
            self.assertEqual(after[oot_gfx.MAIN + off], oot_gfx.lod_hook(s, model))

    def test_lod_site_mismatch_skipped(self):
        bad = oot_gfx.MAIN + oot_gfx.LOD_SITES[0][0]
        count, after = self.install(lod_words={bad: 0})
        self.assertEqual(count, 1)
        self.assertNotIn(bad, after)
        self.assertEqual(len(after), len(oot_gfx.LOD_SITES))

    def test_other_build_or_code_left_out(self):
        self.assertEqual(self.install(word=0)[0], 0)
        self.assertEqual(self.install(build_id="0" * 40)[0], 0)
        self.assertEqual(self.install(word=0)[1], {})

    def test_wired(self):
        lift = (ROOT / "nx2/lift.py").read_text(encoding="utf-8")
        self.assertLess(lift.index("oot_gfx.install("), lift.index("L.discover("))
        replay = (ROOT / "gpu/vk/vk_replay.cpp").read_text(encoding="utf-8")
        for opt, field in (("FOLIAGE", "foliage"), ("OBJECTS", "objects"), ("GRASS", "grass"), ("SHADOWS", "shadows"),
                           ("CONTACT_SHADOWS", "contact_shadows"), ("BASE_QUALITY", "base_quality")):
            self.assertIn("if (id == HOS_OPT_GFX_%s) return opts.gfx_%s;" % (opt, field), replay)


HEAD = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL:%d %s\n", __LINE__, #x); exit(1); } } while (0)
typedef struct { uint64_t x[32]; unsigned char vreg[32][16]; } Ctx;
static void a64_vset64(void *v, uint64_t bits) { memcpy(v, &bits, 8); memset((char *)v + 8, 0, 8); }
enum { HOS_OPT_GFX_FOLIAGE = 53, HOS_OPT_GFX_OBJECTS, HOS_OPT_GFX_GRASS, HOS_OPT_GFX_SHADOWS, HOS_OPT_GFX_CONTACT_SHADOWS,
       HOS_OPT_GFX_BASE_QUALITY };
static unsigned char objects[0x10000], globals[0x80000];
static int opt[6] = {100, 100, 100, 100, 1, 0}, accesses;
static void *ptr(uint64_t a, unsigned n) {
    if (a >= 0x10000000ULL && a + n <= 0x10010000ULL) return objects + (a - 0x10000000ULL);
    CHECK(a >= 0x84e40000ULL && a + n <= 0x84ec0000ULL);
    return globals + (a - 0x84e40000ULL);
}
static uint64_t recomp_load64(Ctx *c, uint64_t a) { uint64_t v; (void)c; accesses++; memcpy(&v, ptr(a, 8), 8); return v; }
static uint32_t recomp_load32(Ctx *c, uint64_t a) { uint32_t v; (void)c; accesses++; memcpy(&v, ptr(a, 4), 4); return v; }
static uint8_t recomp_load8(Ctx *c, uint64_t a) { (void)c; accesses++; return *(uint8_t *)ptr(a, 1); }
static void recomp_store32(Ctx *c, uint64_t a, uint64_t v) { uint32_t w = (uint32_t)v; (void)c; accesses++; memcpy(ptr(a, 4), &w, 4); }
static void recomp_store8(Ctx *c, uint64_t a, uint64_t v) { (void)c; accesses++; *(uint8_t *)ptr(a, 1) = (uint8_t)v; }
int rx_option(int id, int otherwise) {
    CHECK(id >= HOS_OPT_GFX_FOLIAGE && id <= HOS_OPT_GFX_BASE_QUALITY);
    CHECK(otherwise == (id == HOS_OPT_GFX_CONTACT_SHADOWS ? 1 : id == HOS_OPT_GFX_BASE_QUALITY ? 0 : 100));
    return opt[id - HOS_OPT_GFX_FOLIAGE];
}
'''

MAIN = r'''
static void site(Ctx *c) {
SITE_C
}
static void put64(uint64_t a, uint64_t v) { memcpy(ptr(a, 8), &v, 8); }
static void put32(uint64_t a, uint32_t v) { memcpy(ptr(a, 4), &v, 4); }
static void putfl(uint64_t a, float f) { memcpy(ptr(a, 4), &f, 4); }
static float fl(uint64_t a) { float f; memcpy(&f, ptr(a, 4), 4); return f; }
#define O(n) (0x10000000ULL + (n) * 0x1000ULL)
/* O(n): root sub holder fol pa pb scene render lodh lod chsh chh chp sh contact quality */
static void setup(void) {
    memset(objects, 0, sizeof objects); memset(globals, 0, sizeof globals);
    put64(0x84e42540ULL, O(0)); put64(O(0), 0x84b44800ULL);
    put64(O(0) + 0xf0, O(1)); put64(O(1), 0x84b44a70ULL);
    put64(O(2), 0x84c19010ULL); put64(O(3), 0x84b43228ULL);
    put64(O(4), 0x84b435d0ULL); put64(O(5), 0x84b435d0ULL);
    for (int i = 0; i < 8; i++) putfl(O(4) + 0x38 + 4 * i, 10.0f + i), putfl(O(5) + 0x38 + 4 * i, 100.0f + i);
    put64(O(3) + 0x40, O(4)); put64(O(3) + 0x48, O(5)); putfl(O(3) + 0x68, 2.0f);
    put64(0x84e5e258ULL, O(6)); put64(O(6), 0x84ba95b0ULL);
    put64(O(6) + 0x1e8, O(7)); put64(O(7), 0x84babc00ULL);
    put64(O(7) + 0xd50, O(8)); put64(O(8), 0x84c19010ULL); put64(O(8) + 0x150, O(9)); put64(O(9), 0x84ba7a88ULL);
    putfl(O(9) + 0x48, 1.0f); putfl(O(9) + 0x4c, 0.8f); putfl(O(9) + 0x50, 1.0f);
    put64(O(7) + 0xba8, O(10)); put64(O(10), 0x84baa3e8ULL);
    put64(O(10) + 0xc18, O(11)); put64(O(11), 0x84c19010ULL); put64(O(11) + 0x150, O(12)); put64(O(12), 0x84ba6828ULL);
    putfl(O(12) + 0x90, 30.0f); putfl(O(12) + 0x94, 25.0f);
    put64(O(7) + 0xb30, O(13)); put64(O(13), 0x84baa0a8ULL); putfl(O(13) + 0xb4, 160.0f);
    put64(O(7) + 0xc08, O(14)); *(uint8_t *)ptr(O(14) + 0x318, 1) = 0x81;
    put64(0x84eb7dd8ULL, O(15)); put32(O(15) + 0x20, 7);
}
/* sub+0x2a90 lands in O(3), no overlap w/ read fields */
#define SUB_HOLDER (O(1) + 0x2a90)
static void link_holder(void) { put64(SUB_HOLDER, O(2)); put64(O(2) + 0x150, O(3)); }
static void frame(void) { Ctx c; memset(&c, 0xab, sizeof c); Ctx before = c; site(&c); CHECK(!memcmp(&c, &before, sizeof c)); }
int main(void) {
    setup(); link_holder();
    unsigned char o0[sizeof objects], g0[sizeof globals];
    memcpy(o0, objects, sizeof objects); memcpy(g0, globals, sizeof globals);
    /* defaults: 0 access */
    accesses = 0; frame(); frame(); CHECK(accesses == 0);
    CHECK(!memcmp(objects, o0, sizeof objects) && !memcmp(globals, g0, sizeof globals));

    /* no compounding */
    opt[0] = 200; opt[1] = 300; opt[2] = 50; opt[3] = 400; opt[4] = 0; opt[5] = 1;
    for (int i = 0; i < 5; i++) frame();
    CHECK(fl(O(4) + 0x38) == 20.0f && fl(O(4) + 0x38 + 28) == 34.0f && fl(O(5) + 0x38 + 4) == 202.0f);
    CHECK(fl(O(3) + 0x68) == 1.0f);
    CHECK(fl(O(9) + 0x48) == 1.0f && fl(O(9) + 0x4c) == 0.8f && fl(O(9) + 0x50) == 1.0f);   /* LOD param never written */
    CHECK(fl(O(12) + 0x90) == 120.0f && fl(O(12) + 0x94) == 100.0f && fl(O(13) + 0xb4) == 640.0f);
    CHECK(*(uint8_t *)ptr(O(14) + 0x318, 1) == 0x80);
    uint32_t q; memcpy(&q, ptr(O(15) + 0x20, 4), 4); CHECK(q == 0);

    /* game write = new orig */
    putfl(O(13) + 0xb4, 50.0f); frame(); CHECK(fl(O(13) + 0xb4) == 200.0f);

    /* back to defaults = restore */
    opt[0] = opt[1] = opt[2] = opt[3] = 100; opt[4] = 1; opt[5] = 0;
    frame(); frame();
    CHECK(fl(O(4) + 0x38) == 10.0f && fl(O(5) + 0x38 + 4) == 101.0f && fl(O(3) + 0x68) == 2.0f);
    CHECK(fl(O(9) + 0x4c) == 0.8f && fl(O(12) + 0x90) == 30.0f && fl(O(13) + 0xb4) == 50.0f);
    CHECK(*(uint8_t *)ptr(O(14) + 0x318, 1) == 0x81);
    memcpy(&q, ptr(O(15) + 0x20, 4), 4); CHECK(q == 7);

    /* game's own char sh vals: kept at 100%, scaled rel */
    putfl(O(12) + 0x90, 12.0f);
    frame(); CHECK(fl(O(12) + 0x90) == 12.0f);
    opt[3] = 200; frame(); CHECK(fl(O(12) + 0x90) == 24.0f);
    opt[3] = 100; frame(); CHECK(fl(O(12) + 0x90) == 12.0f);

    /* bad vt = chain untouched */
    setup(); link_holder(); put64(O(3), 0x84b40000ULL); opt[0] = 200; opt[2] = 0;
    memcpy(o0, objects, sizeof objects);
    frame(); CHECK(!memcmp(objects + 0x3000, o0 + 0x3000, 0x3000));
    setup(); link_holder(); put64(O(7), 0); opt[1] = 200; opt[3] = 200;
    memcpy(o0, objects, sizeof objects);
    frame(); CHECK(!memcmp(objects + 0x8000, o0 + 0x8000, 0x7000));
    /* null roots */
    memset(globals, 0, sizeof globals); frame();
    /* obj detail per model: x factor on the scale reg, 100% neutral */
    const uint64_t plain = O(6) + 0x400;
    Ctx c; memset(&c, 0, sizeof c);
    float in = 1.5f, outf;
    opt[1] = 100; memcpy(c.vreg[3], &in, 4); hos_oot_gfx_lod(&c, 3, plain); memcpy(&outf, c.vreg[3], 4); CHECK(outf == 1.5f);
    opt[1] = 250; memcpy(c.vreg[3], &in, 4); hos_oot_gfx_lod(&c, 3, plain); memcpy(&outf, c.vreg[3], 4); CHECK(outf == 3.75f);
    memcpy(c.vreg[1], &in, 4); hos_oot_gfx_lod(&c, 1, plain); memcpy(&outf, c.vreg[1], 4); CHECK(outf == 3.75f);
    memcpy(c.vreg[1], &in, 4); hos_oot_gfx_lod(&c, 1, 0); memcpy(&outf, c.vreg[1], 4); CHECK(outf == 1.5f);
    puts("PASS: oot_gfx");
    return 0;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="gcc")
    args = parser.parse_args()
    result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(InstallTests))
    if not result.wasSuccessful():
        raise SystemExit(1)
    host = (ROOT / "host/oot_gfx.c").read_text(encoding="utf-8")
    body = "".join(line for line in host.splitlines(keepends=True) if not line.startswith("#include"))
    with tempfile.TemporaryDirectory(prefix="oot-gfx-") as temporary:
        source = Path(temporary) / "fixture.c"
        executable = Path(temporary) / "fixture.exe"
        source.write_text(HEAD + body + MAIN.replace("SITE_C", oot_gfx.HOOK), encoding="utf-8")
        subprocess.run([args.cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", str(source), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
