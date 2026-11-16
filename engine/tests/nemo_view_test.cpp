// Nemo View checks: integer primitives, the baked depth-band table, run-length round trip,
// identical frames for every worker count, the object-id buffer in the software renderer
// (colour and depth unchanged when it is on, hidden objects never show) and a speed report.
// Registered with ctest as engine_nemo_view.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "litholattice/core/math.h"
#include "litholattice/render/nemo_view.h"
#include "litholattice/render/renderer.h"
#include "litholattice/render/software_renderer.h"

using namespace litholattice;
using namespace litholattice::nemoview;

namespace {

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("[NEMO-VIEW] %-78s %s\n", what, ok ? "PASS" : "FAIL");
  if (!ok) ++g_fail;
}

// A deterministic test scene: gradients, a bright block, depth steps and object patches.
struct Fixture {
  int w, h;
  std::vector<uint32_t> px;
  std::vector<uint16_t> depth;
  std::vector<uint8_t> obj;
  Fixture(int W, int H) : w(W), h(H), px(size_t(W) * H), depth(size_t(W) * H), obj(size_t(W) * H) {
    uint32_t s = 0x9E3779B9u;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x) {
        s = s * 1664525u + 1013904223u;
        const size_t i = size_t(y) * W + x;
        const uint32_t r = uint32_t((x * 255) / W), g = uint32_t((y * 255) / H), b = (s >> 24) & 0x3F;
        px[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
        if (y < H / 4) {
          depth[i] = 0xFFFF;  // sky
          obj[i] = 0;
        } else {
          depth[i] = uint16_t(58000 + (x * 7000) / W);
          obj[i] = (x / 40) % 3 == 0 ? uint8_t(ObjectId::Wall) : (x / 40) % 3 == 1 ? uint8_t(ObjectId::Floor) : uint8_t(ObjectId::Door);
        }
      }
  }
  SourceView view() const {
    SourceView v;
    v.width = w;
    v.height = h;
    v.pixels = px.data();
    v.depth = depth.data();
    v.objects = obj.data();
    return v;
  }
};

std::string buildWithWorkers(const SourceView& src, const FrameConfig& cfg, int workers) {
  AsciiFrame f;
  if (!prepare(src, cfg, f)) return "prepare failed";
  std::vector<std::thread> pool;
  const int per = (cfg.rows + workers - 1) / workers;
  for (int k = 0; k < workers; ++k) {
    const int r0 = k * per, r1 = std::min(cfg.rows, r0 + per);
    if (r0 >= r1) break;
    pool.emplace_back([&, r0, r1] { buildRows(src, cfg, f, r0, r1); });  // disjoint rows only
  }
  for (auto& t : pool) t.join();  // the phase boundary
  finish(f);
  return shaHex(f);
}

PackedVertex vert(float x, float y, float z, uint32_t layer, uint8_t face) {
  PackedVertex v{};
  v.x = uint16_t(x * 16.f);
  v.y = uint16_t(y * 16.f);
  v.z = uint16_t(z * 16.f);
  v.faceAo = face;
  v.mode = 0;
  v.setLayer(layer);
  return v;
}
// Both windings, so the test does not depend on the cull convention.
void quadZ(std::vector<PackedVertex>& out, float x0, float x1, float y0, float y1, float z, uint32_t layer) {
  const PackedVertex a = vert(x0, y0, z, layer, 4), b = vert(x1, y0, z, layer, 4), c = vert(x1, y1, z, layer, 4),
                     d = vert(x0, y1, z, layer, 4);
  out.insert(out.end(), {a, b, c, d});
  out.insert(out.end(), {a, d, c, b});
}
void quadY(std::vector<PackedVertex>& out, float x0, float x1, float z0, float z1, float y, uint32_t layer) {
  const PackedVertex a = vert(x0, y, z0, layer, 2), b = vert(x1, y, z0, layer, 2), c = vert(x1, y, z1, layer, 2),
                     d = vert(x0, y, z1, layer, 2);
  out.insert(out.end(), {a, b, c, d});
  out.insert(out.end(), {a, d, c, b});
}

}  // namespace

int main() {
  // ---- primitives ----
  check(luma8(0, 0, 0) == 0 && luma8(255, 255, 255) == 255, "integer luma spans 0..255");
  check(luma8(255, 0, 0) == 77 && luma8(0, 255, 0) == 149 && luma8(0, 0, 255) == 29, "integer luma weights 77/150/29");
  {
    bool seen[16] = {};
    for (int y = 0; y < 4; ++y)
      for (int x = 0; x < 4; ++x) seen[bayer4(x, y)] = true;
    bool all = true;
    for (bool s : seen) all = all && s;
    check(all && bayer4(4, 4) == bayer4(0, 0), "4x4 Bayer holds 0..15 once and tiles");
  }
  check(paletteIndex(0, 0, 0) == 0 && paletteIndex(255, 255, 255) == 15, "palette ends");
  check(paletteIndex(64, 0, 0) == 0, "palette tie (64,0,0) keeps the lower index");
  {
    CellFields f;
    f.glyph = '#';
    f.ink = 15;
    f.fill = 8;
    f.depth = 2;
    f.alpha = 1;
    f.object = 2;
    f.light = 11;
    f.face = uint8_t(FaceId::Top);
    f.emissive = true;
    const uint64_t w = packCell(f);
    check((w & kCellReservedMask) == 0, "packCell clears reserved bits");
    const CellFields b = unpackCell(w | kCellReservedMask);
    check(b.glyph == '#' && b.ink == 15 && b.fill == 8 && b.depth == 2 && b.alpha == 1 && b.object == 2 &&
              b.light == 11 && b.face == uint8_t(FaceId::Top) && b.emissive && !b.edge && !b.wet,
          "pack/unpack round trip; reserved bits are ignored");
    check(fillScale(uint8_t(FaceId::Top)) > fillScale(uint8_t(FaceId::Bottom)) &&
              fillScale(uint8_t(FaceId::Top)) > fillScale(uint8_t(FaceId::PosX)),
          "top faces keep a fuller material fill than sides or bottoms");
  }
  check(std::strlen(kRamp) == size_t(kRampSteps) && kRamp[0] == ' ', "ramp.v1 has 12 steps, space first");
  check(depthTableShaPrefix() == "c4b3f1700c0d9c94", "baked depth-band table SHA-256 c4b3f1700c0d9c94...");
  {
    bool mono = true;
    for (int m = 0; m < 3; ++m) {
      int last = 0;
      for (uint32_t q = 0; q < 0xFFFF; q += 7) {
        const int b = depthBand(uint16_t(q), m);
        mono = mono && b >= last && b <= 6;
        last = b;
      }
    }
    check(mono && depthBand(0xFFFF, 0) == kSkyBand && depthBand(0, 0) == 0, "depth bands rise with distance; cleared depth is sky");
  }
  // ---- run-length form ----
  {
    const std::string t = "    ....##\n&&&&&&&&&&&&|k E\n";
    std::string back;
    const std::string r = rleEncode(t);
    check(rleDecode(r, back) && back == t && r.size() < t.size(), "run-length round trip");
    check(!rleDecode("3a", back), "a count with no character fails");
  }
  // ---- frames: geometry and worker-count identity ----
  const Fixture fx(320, 180);
  const SourceView src = fx.view();
  {
    AsciiFrame f;
    std::string err;
    FrameConfig bad;
    bad.cols = 81;
    check(!prepare(src, bad, f, &err), "cells must tile the source exactly (81 cols refused)");
  }
  struct Case { const char* name; int cols, rows, cw, ch; ViewMode mode; };
  const Case cases[] = {{"80x45 objects", 80, 45, 0, 0, ViewMode::Objects},
                        {"80x45 pixels+depth", 80, 45, 0, 0, ViewMode::PixelsAndDepth},
                        {"40x45 objects", 40, 45, 0, 0, ViewMode::Objects},
                        {"fovea 32x12", 32, 12, 128, 48, ViewMode::Objects}};
  for (const Case& c : cases) {
    SourceView v = src;
    if (c.cw) {
      v.cropW = c.cw;
      v.cropH = c.ch;
      v.cropX = (v.width - c.cw) / 2;
      v.cropY = (v.height - c.ch) / 2;
    }
    FrameConfig cfg;
    cfg.cols = c.cols;
    cfg.rows = c.rows;
    cfg.mode = c.mode;
    const std::string ref = buildWithWorkers(v, cfg, 1);
    bool same = true;
    for (int wk : {2, 4, 8, 12}) same = same && buildWithWorkers(v, cfg, wk) == ref;
    char line[160];
    std::snprintf(line, sizeof line, "%s: same SHA at 1/2/4/8/12 workers (%.16s)", c.name, ref.c_str());
    check(same, line);
  }
  {
    FrameConfig cfg;
    AsciiFrame f;
    build(src, cfg, f);
    const std::string g = glyphRows(f);
    bool noDigits = true;
    for (char ch : g) noDigits = noDigits && !(ch >= '0' && ch <= '9');
    check(noDigits, "glyph text carries no digits (no coordinates can leak)");
    const std::string rec = canonicalRecord(f);
    check(rec.rfind("nemo_ascii.v2|80|45|objects|ramp.v1|pal16.v1|c4b3f1700c0d9c94|", 0) == 0, "canonical record header");
    bool reservedZero = true, hasFill = false;
    for (uint64_t w : f.cell) {
      reservedZero = reservedZero && ((w & kCellReservedMask) == 0);
      const CellFields cf = unpackCell(w);
      if (cf.depth != kSkyBand && cf.fill != 0) hasFill = true;
    }
    check(reservedZero && f.cell.size() == size_t(80 * 45), "fixture cells are packed and reserved bits are zero");
    check(hasFill, "non-sky cells carry a material fill");
    check(channelRows(f, "glyph") == glyphRows(f) && !tex8Rows(f).empty() && !qwordRows(f).empty(),
          "glyph is the default channel; tex8 and qword slices are available");
    bool solidFill = false;
    for (size_t i = 0; i < f.cell.size(); ++i) {
      const CellLook look = decodeCell(f, i, ViewStyle::Solid);
      if (look.glyph != ' ' && (look.fill & 0x00FFFFFFu) != 0) solidFill = true;
    }
    check(solidFill, "solid decoder paints a non-black fill on visible cells");
    std::printf("[NEMO-VIEW] fixture 80x45 objects sha256 %s\n", shaHex(f).c_str());
    std::printf("%s\n", g.substr(0, 81 * 14).c_str());
  }
  {
    // Pixels+depth mode ignores object ids entirely (the crutch comparison).
    Fixture other(320, 180);
    for (auto& o : other.obj) o = uint8_t(ObjectId::Key);
    FrameConfig cfg;
    cfg.mode = ViewMode::PixelsAndDepth;
    AsciiFrame a, b;
    build(src, cfg, a);
    build(other.view(), cfg, b);
    check(a.sha256 == b.sha256, "pixels+depth mode: object ids do not change the frame");
  }
  {
    // A change outside the fovea crop does not change the fovea frame.
    Fixture other(320, 180);
    for (int x = 0; x < 320; ++x) other.px[size_t(5) * 320 + x] = 0xFFFFFFFFu;  // row 5, outside the crop
    SourceView a = src, b = other.view();
    for (SourceView* v : {&a, &b}) { v->cropW = 128; v->cropH = 48; v->cropX = 96; v->cropY = 66; }
    FrameConfig cfg;
    cfg.cols = 32;
    cfg.rows = 12;
    AsciiFrame fa, fb;
    build(a, cfg, fa);
    build(b, cfg, fb);
    check(fa.sha256 == fb.sha256, "out-of-view change leaves the fovea frame unchanged");
  }
  {
    // Face ids ride along the same cells; majority wins, lower id keeps a tie.
    Fixture faced(320, 180);
    std::vector<uint8_t> faces(size_t(320) * 180, uint8_t(FaceId::Unknown));
    for (int y = 45; y < 180; ++y)
      for (int x = 0; x < 160; ++x) faces[size_t(y) * 320 + x] = uint8_t(FaceId::Top);
    SourceView v = faced.view();
    v.faces = faces.data();
    FrameConfig cfg;
    AsciiFrame f;
    build(v, cfg, f);
    int tops = 0;
    for (uint64_t w : f.cell)
      if (unpackCell(w).face == uint8_t(FaceId::Top)) ++tops;
    check(tops > 0, "majority face id is packed into the cell");
  }
  // ---- the software renderer's object-id buffer ----
  {
    std::vector<PackedVertex> mesh;
    quadY(mesh, 0.f, 16.f, 0.f, 24.f, 1.f, 3);    // floor (layer 3)
    quadZ(mesh, 5.f, 11.f, 1.f, 4.f, 14.f, 1);    // wall in front (layer 1)
    quadZ(mesh, 7.f, 9.f, 1.5f, 3.f, 10.f, 2);    // key hidden behind the wall (layer 2)
    const Vec3 eye{8.f, 2.5f, 22.f};
    const Mat4 vp = mul(perspective(70.f * 3.14159265f / 180.f, 320.f / 180.f, 0.05f, 1000.f), lookDir(eye, {0.f, 0.f, -1.f}));
    cSoftwareRenderer off, on;
    for (cSoftwareRenderer* r : {&off, &on}) {
      r->setPerPixelShading(false);
      r->resize(320, 180);
    }
    const uint8_t table[4] = {uint8_t(ObjectId::Other), uint8_t(ObjectId::Wall), uint8_t(ObjectId::Key), uint8_t(ObjectId::Floor)};
    on.setObjectCapture(table, 4);
    for (cSoftwareRenderer* r : {&off, &on}) {
      r->clear(0xFF94B3DB);
      r->renderMesh(mesh.data(), mesh.size(), vp.m);
    }
    check(off.framebuffer().pixels == on.framebuffer().pixels, "object capture on: colour output identical to off");
    check(off.depthBuffer() == on.depthBuffer(), "object capture on: depth output identical to off");
    check(off.objectBuffer().empty(), "object capture off: no object buffer kept");
    const auto& ob = on.objectBuffer();
    size_t walls = 0, keys = 0, floors = 0, sky = 0;
    for (uint8_t o : ob) {
      walls += o == uint8_t(ObjectId::Wall);
      keys += o == uint8_t(ObjectId::Key);
      floors += o == uint8_t(ObjectId::Floor);
      sky += o == 0;
    }
    char line[160];
    std::snprintf(line, sizeof line, "object ids: wall %zu, floor %zu, sky %zu px; hidden key 0 px (got %zu)", walls, floors, sky, keys);
    check(walls > 0 && floors > 0 && sky > 0 && keys == 0, line);
    SourceView v;
    v.width = 320;
    v.height = 180;
    v.pixels = on.framebuffer().pixels.data();
    v.depth = on.depthBuffer().data();
    v.objects = ob.data();
    FrameConfig cfg;
    AsciiFrame f;
    build(v, cfg, f);
    const std::string g = glyphRows(f);
    check(g.find('#') != std::string::npos && g.find('k') == std::string::npos, "rendered frame shows the wall as # and never the hidden key");
    std::printf("[NEMO-VIEW] rendered scene sha256 %s\n%s\n", shaHex(f).c_str(), g.c_str());
    // Take the wall away: the key appears.
    std::vector<PackedVertex> noWall;
    quadY(noWall, 0.f, 16.f, 0.f, 24.f, 1.f, 3);
    quadZ(noWall, 7.f, 9.f, 1.5f, 3.f, 10.f, 2);
    on.clear(0xFF94B3DB);
    on.renderMesh(noWall.data(), noWall.size(), vp.m);
    v.pixels = on.framebuffer().pixels.data();
    v.depth = on.depthBuffer().data();
    v.objects = on.objectBuffer().data();
    AsciiFrame f2;
    build(v, cfg, f2);
    check(glyphRows(f2).find('k') != std::string::npos, "with the wall gone the key shows as k");
  }
  // ---- speed (report; fails only when far off the 0.5 ms target) ----
  {
    FrameConfig cfg;
    AsciiFrame f;
    const int reps = 200;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) build(src, cfg, f);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / reps;
    char line[160];
    std::snprintf(line, sizeof line, "80x45 frame build %.3f ms single-threaded (target 0.5 ms)", ms);
    check(ms < 5.0, line);
  }
  if (g_fail) {
    std::printf("[NEMO-VIEW] %d FAILED\n", g_fail);
    return 1;
  }
  std::printf("[NEMO-VIEW] ALL PASS\n");
  return 0;
}
