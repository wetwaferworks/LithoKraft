// Cortana 2026-10-15 2010 [GAME]: 3D voxel foliage models + shared instance meshes.
// See lithokraft/content/foliage.h for the instancing design.
#include "lithokraft/content/foliage.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <litholattice/procgen/litho_formula_voxelize.h>
#include "litholattice/render/micro_mesher.h"

#ifndef LITHOKRAFT_MODELS_DIR
#define LITHOKRAFT_MODELS_DIR ""
#endif

namespace lithokraft::foliage {
namespace {

constexpr double kPi = 3.14159265358979323846;

// Dense authoring grid: 16 x (16 * blocks) x 16 level-4 cells.
struct Grid {
  int nb;
  std::vector<uint8_t> v;
  explicit Grid(int blocks) : nb(blocks), v(size_t(16 * 16 * 16 * blocks), 0) {}
  bool in(int x, int y, int z) const { return x >= 0 && x < 16 && z >= 0 && z < 16 && y >= 0 && y < 16 * nb; }
  void set(int x, int y, int z, uint8_t m) { if (in(x, y, z)) v[(size_t(y) * 16 + size_t(z)) * 16 + size_t(x)] = m; }
  uint8_t get(int x, int y, int z) const { return in(x, y, z) ? v[(size_t(y) * 16 + size_t(z)) * 16 + size_t(x)] : 0; }
  std::vector<model::Model> build(const std::string& name) const {
    std::vector<model::Model> out;
    for (int b = 0; b < nb; ++b) {
      model::cModelBuilder mb(name + "_" + std::to_string(b), "Cortana for JD (foliage 2026-10-15)");
      for (int y = 0; y < 16; ++y)
        for (int z = 0; z < 16; ++z)
          for (int x = 0; x < 16; ++x)
            if (const uint8_t m = get(x, b * 16 + y, z)) mb.setCell(4, x, y, z, m);
      out.push_back(mb.build());
    }
    return out;
  }
};

int rnd(double v) { return int(std::lround(v)); }

// Small deterministic integer hash for authored jitter (no RNG state, no clocks).
uint32_t hash3(uint32_t a, uint32_t b, uint32_t c) {
  uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u ^ (c + 0x165667B1u) * 0xC2B2AE3Du;
  h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
  return h;
}

// ---- Tron digital-nature brief (JD 2026-10-15 20:16): real plants computerized for a wafer.
// Dark bodies, kGlow edge/trace lines, geometric (Manhattan / orthogonal) forms. Shapes only;
// colours are placeholders (CloudAtlas owns the textures, one slot per material id).

void box(Grid& g, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t m) {  // inclusive
  for (int y = y0; y <= y1; ++y)
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x) g.set(x, y, z, m);
}

// Cattail: stems are straight leads; the seed head is a radial capacitor can (3x3 body with
// glowing end bands and a polarity stripe), the spike above is the lead pin; blades are
// orthogonally routed traces (go up, jog one cell, go up) with glowing tips.
std::vector<model::Model> cattail() {
  Grid g(2);
  struct Stalk { int x, z, top; };
  for (const Stalk s : {Stalk{7, 7, 30}, Stalk{11, 10, 26}, Stalk{4, 10, 23}}) {
    box(g, s.x, 0, s.z, s.x, s.top - 12, s.z, mat::kStem);
    const int h0 = s.top - 11, h1 = s.top - 4;
    box(g, s.x - 1, h0, s.z - 1, s.x + 1, h1, s.z + 1, mat::kCattailHead);  // capacitor can
    for (int y : {h0, h1}) {  // glowing end bands (corners trimmed: a round can)
      box(g, s.x - 1, y, s.z, s.x + 1, y, s.z, mat::kGlow);
      box(g, s.x, y, s.z - 1, s.x, y, s.z + 1, mat::kGlow);
    }
    box(g, s.x + 1, h0 + 1, s.z + 1, s.x + 1, h1 - 1, s.z + 1, mat::kGlow);  // polarity stripe
    box(g, s.x, h1 + 1, s.z, s.x, s.top - 1, s.z, mat::kStem);                 // lead pin
    g.set(s.x, s.top, s.z, mat::kGlow);
  }
  struct Blade { int x, z, dx, dz, len; };
  for (const Blade b : {Blade{8, 8, 1, 0, 17}, Blade{6, 6, -1, 0, 13}, Blade{9, 5, 0, -1, 19}, Blade{6, 9, 0, 1, 11},
                        Blade{12, 12, 1, 1, 14}}) {
    int px = b.x, pz = b.z;
    for (int i = 0; i < b.len; ++i) {
      if (i > 0 && i % 5 == 0) {  // a 45-degree jog, drawn as two orthogonal steps
        g.set(px, i, pz, mat::kLeaf);
        px += b.dx; pz += b.dz;
      }
      g.set(px, i, pz, i == b.len - 1 ? mat::kGlow : mat::kLeaf);
    }
  }
  return g.build("foliage_cattail");
}

// Bush -> "routing moss" mound: a stepped (ziggurat) dark mound, its top surface laid out like
// circuit routing (Manhattan glow traces between via pads), a few berry-red "LED" pads.
std::vector<model::Model> bush() {
  Grid g(1);
  for (int lvl = 0; lvl < 5; ++lvl) box(g, 1 + lvl, lvl, 1 + lvl, 14 - lvl, lvl, 14 - lvl, lvl == 0 ? mat::kBark : mat::kLeaf);
  auto topY = [&](int x, int z) { for (int y = 15; y >= 0; --y) if (g.get(x, y, z)) return y; return -1; };
  auto trace = [&](int x0, int z0, int x1, int z1) {  // L-route: along x, then along z, on the surface
    for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) { const int y = topY(x, z0); if (y >= 0) g.set(x, y, z0, mat::kGlow); }
    for (int z = std::min(z0, z1); z <= std::max(z0, z1); ++z) { const int y = topY(x1, z); if (y >= 0) g.set(x1, y, z, mat::kGlow); }
  };
  trace(5, 5, 10, 9); trace(10, 5, 6, 10); trace(3, 8, 7, 12); trace(12, 3, 12, 11);
  static const int kPads[5][2] = {{5, 5}, {10, 9}, {7, 12}, {12, 11}, {3, 8}};
  for (const auto& p : kPads) {  // via / LED pads
    const int y = topY(p[0], p[1]);
    if (y >= 0 && y < 15) g.set(p[0], y + 1, p[1], (p[0] + p[1]) % 2 ? mat::kBerry : mat::kGlow);
  }
  return g.build("foliage_bush");
}

// Fern: an H-tree clock-distribution fractal. A trunk rises; each level is an H whose bar
// alternates between the x and z axes and climbs; the leaf ends are glowing sink nodes.
void hTree(Grid& g, int cx, int y, int cz, int half, bool alongX, int depth) {
  const int dx = alongX ? half : 0, dz = alongX ? 0 : half;
  box(g, cx - dx, y, cz - dz, cx + dx, y, cz + dz, mat::kFern);  // the H bar
  for (int s : {-1, 1}) {
    const int ex = cx + s * dx, ez = cz + s * dz;
    box(g, ex, y, ez, ex, y + 2, ez, mat::kFern);  // riser to the next level
    if (depth > 1 && half > 1) hTree(g, ex, y + 2, ez, half / 2, !alongX, depth - 1);
    else g.set(ex, y + 3, ez, mat::kGlow);  // clock sink
  }
}
std::vector<model::Model> fern() {
  Grid g(1);
  box(g, 7, 0, 7, 8, 0, 8, mat::kBark);
  box(g, 7, 1, 7, 7, 4, 7, mat::kFern);
  hTree(g, 7, 5, 7, 6, true, 3);
  g.set(7, 5, 7, mat::kGlow);  // root clock buffer
  return g.build("foliage_fern");
}

// Reeds: a parallel bus -- straight vertical lines of staggered heights with via beads every
// few cells and glowing tips (signal lines in a reed bed).
std::vector<model::Model> reeds() {
  Grid g(1);
  for (uint32_t i = 0; i < 11; ++i) {
    const uint32_t h = hash3(i, 17u, 5u);
    const int x = 2 + int(i % 4u) * 4 + int(h % 2u), z = 2 + int(i / 4u) * 4 + int((h >> 8) % 3u);
    const int height = 9 + int((h >> 16) % 7u);
    const uint8_t m = i % 3u == 2u ? mat::kLeaf : mat::kReed;
    for (int y = 0; y < height; ++y) g.set(x, y, z, (y % 4 == 3) ? mat::kBark : m);  // via beads
    g.set(x, height, z, mat::kGlow);
  }
  return g.build("foliage_reeds");
}

// Crystal shrub: bond-wire loops + a via array. A dark die on a pad, a crystal core seeded by
// the Litho Formula voxelizer (res 8, scaled into the die), eight glowing bond-wire arcs from
// the die edge down to a ring of bond pads, and a 4x4 via-pillar array at the corners.
std::vector<model::Model> crystalShrub() {
  Grid g(1);
  box(g, 1, 0, 1, 14, 0, 14, mat::kBark);   // substrate
  box(g, 5, 1, 5, 10, 2, 10, mat::kDark);   // die
  namespace lf = litholattice::procgen::litho_formula;
  lf::LithoParams th, ph;
  th.m = 6; th.n1 = 0.3; th.n2 = 0.7; th.n3 = 0.7;
  ph.m = 4; ph.n1 = 0.5; ph.n2 = 1.0; ph.n3 = 1.0;
  for (const auto& c : lf::voxelizeLithoShape(th, ph, 8).filled)  // 8^3 crystal on top of the die
    g.set(4 + c[0], 3 + c[1], 4 + c[2], (c[0] == 3 || c[0] == 4) && (c[2] == 3 || c[2] == 4) ? mat::kCrystalCore : mat::kCrystal);
  for (int k = 0; k < 8; ++k) {  // bond wires: arcs from the die edge (r=2.5) out to pads (r=6.5)
    const double a = k * kPi / 4.0 + kPi / 8.0, ca = std::cos(a), sa = std::sin(a);
    for (int t = 0; t <= 16; ++t) {
      const double s = t / 16.0, r = 2.5 + 4.0 * s, y = 2.0 + 4.0 * std::sin(kPi * s);
      g.set(rnd(7.5 + r * ca), rnd(y), rnd(7.5 + r * sa), mat::kGlow);
    }
    g.set(rnd(7.5 + 6.5 * ca), 1, rnd(7.5 + 6.5 * sa), mat::kCrystal);  // bond pad
  }
  for (int cx : {2, 12})
    for (int cz : {2, 12})
      for (int i = 0; i < 4; ++i) box(g, cx + (i & 1), 1, cz + (i >> 1), cx + (i & 1), 2 + ((i + cx) % 2), cz + (i >> 1), mat::kCrystalCore);
  return g.build("foliage_crystal_shrub");
}
// Moss (Cortana 2026-10-15 2110, JD brief "moss laid out like circuit routing"): a low carpet
// (1-2 micro-cells) of Manhattan-routed traces -- vertical trunks, horizontal branches chosen by
// a fixed hash, glowing vias at junctions and 2x2 raised pads at trace ends.
std::vector<model::Model> moss() {
  Grid g(1);
  const int trunks[4] = {2, 6, 9, 13};
  for (int t = 0; t < 4; ++t) {
    const uint32_t h = hash3(uint32_t(t), 91u, 7u);
    const int z0 = 1 + int(h % 4u), z1 = 11 + int((h >> 8) % 4u);
    box(g, trunks[t], 0, z0, trunks[t], 0, z1, mat::kLeaf);
    box(g, trunks[t] - 1, 0, z1, trunks[t], 1, z1 + 1, mat::kLeaf);  // end pad
  }
  for (int z = 2; z <= 13; z += 3)
    for (int t = 0; t < 3; ++t) {
      if (hash3(uint32_t(z), uint32_t(t), 33u) % 3u == 0u) continue;
      box(g, trunks[t], 0, z, trunks[t + 1], 0, z, mat::kLeaf);
      g.set(trunks[t], 1, z, mat::kGlow);      // via at each junction
      g.set(trunks[t + 1], 1, z, mat::kGlow);
    }
  return g.build("foliage_moss");
}

// Repeator placeholder (Cortana 2026-10-15 2215): slab + arrow + 4 buttons, active one glows.
std::vector<model::Model> repeatorModel(int active) {
  Grid g(1);
  box(g, 0, 0, 0, 15, 2, 15, mat::kDark);                 // slab body (3/16)
  box(g, 0, 3, 0, 15, 3, 0, mat::kStem); box(g, 0, 3, 15, 15, 3, 15, mat::kStem);  // rim
  box(g, 0, 3, 0, 0, 3, 15, mat::kStem); box(g, 15, 3, 0, 15, 3, 15, mat::kStem);
  box(g, 7, 3, 1, 8, 3, 13, mat::kCrystalCore);           // signal arrow back -> front (-Z)
  box(g, 6, 3, 2, 9, 3, 2, mat::kCrystalCore); box(g, 5, 3, 3, 10, 3, 3, mat::kCrystalCore);
  const int bx[4] = {2, 10, 10, 2}, bz[4] = {4, 4, 10, 10};
  for (int i = 0; i < 4; ++i) {
    const bool on = i == (active & 3);  // placeholder only (4 buttons); assets replace it
    box(g, bx[i], 3, bz[i], bx[i] + 3, on ? 4 : 3, bz[i] + 3, on ? mat::kGlow : mat::kBark);
    for (int d = 0; d <= i; ++d) g.set(bx[i] + d, on ? 5 : 4, bz[i] + 1, on ? mat::kGlow : mat::kCrystal);  // 1-4 pips
  }
  return g.build("repeator_" + std::to_string(active));
}

Quad toQuad(const litholattice::micro::MicroQuad& m) {
  const int ax = m.dir / 2;
  const bool pos = (m.dir % 2) == 0;
  const int P[3] = {rnd(m.x * 16.0), rnd(m.y * 16.0), rnd(m.z * 16.0)};
  const int W = rnd(m.w * 16.0), H = rnd(m.h * 16.0);
  const int t0 = ax == 0 ? 1 : 0, t1 = ax == 2 ? 1 : 2;  // X -> (Y, Z), Y -> (X, Z), Z -> (X, Y)
  int lo[3], hi[3];
  lo[t0] = P[t0]; hi[t0] = P[t0] + W;
  lo[t1] = P[t1]; hi[t1] = P[t1] + H;
  if (pos) { lo[ax] = P[ax] - 1; hi[ax] = P[ax]; }
  else { lo[ax] = P[ax]; hi[ax] = P[ax] + 1; }
  Quad q{};
  for (int i = 0; i < 3; ++i) { q.lo[i] = int16_t(lo[i]); q.hi[i] = int16_t(hi[i]); }
  q.face = m.dir;
  q.material = m.material;
  return q;
}

}  // namespace

const char* kindName(Kind k) {
  switch (k) {
    case Kind::Cattail: return "cattail";
    case Kind::Bush: return "bush";
    case Kind::Fern: return "fern";
    case Kind::Reeds: return "reeds";
    case Kind::CrystalShrub: return "crystal_shrub";
    case Kind::Moss: return "moss";
  }
  return "?";
}

int kindHeight(Kind k) { return k == Kind::Cattail ? 2 : 1; }

std::vector<model::Model> buildModel(Kind k) {
  switch (k) {
    case Kind::Cattail: return cattail();
    case Kind::Bush: return bush();
    case Kind::Fern: return fern();
    case Kind::Reeds: return reeds();
    case Kind::CrystalShrub: return crystalShrub();
    case Kind::Moss: return moss();
  }
  return {};
}

std::string assetPath(const std::string& dir, Kind k, int i) {
  return dir + "/foliage_" + kindName(k) + "_" + std::to_string(i) + ".lthm";
}

// Cortana 2026-10-15 2310 [GAME] (Vulkan/foliage push): lossless coplanar quad merge. Two quads
// merge when they have the same face, material and plane, the same extent on one tangent axis,
// and touch on the other. Repeated until stable. Covered area and the set of covered texels are
// unchanged (custom faces are UV'd from their 1/16 positions), so the look is preserved and
// every instance of a kind emits fewer quads on both renderers. Deterministic (fixed order).
size_t mergeCoplanar(std::vector<Quad>& qs) {
  const size_t before = qs.size();
  bool changed = true;
  while (changed) {
    changed = false;
    for (size_t i = 0; i < qs.size(); ++i) {
      for (size_t j = i + 1; j < qs.size(); ++j) {
        Quad& a = qs[i];
        const Quad& b = qs[j];
        if (a.face != b.face || a.material != b.material) continue;
        const int ax = a.face / 2;
        if (a.lo[ax] != b.lo[ax] || a.hi[ax] != b.hi[ax]) continue;
        const int t0 = ax == 0 ? 1 : 0, t1 = ax == 2 ? 1 : 2;
        bool merged = false;
        for (int pass = 0; pass < 2 && !merged; ++pass) {
          const int t = pass == 0 ? t0 : t1, o = pass == 0 ? t1 : t0;
          if (a.lo[o] != b.lo[o] || a.hi[o] != b.hi[o]) continue;
          if (a.hi[t] == b.lo[t]) { a.hi[t] = b.hi[t]; merged = true; }
          else if (b.hi[t] == a.lo[t]) { a.lo[t] = b.lo[t]; merged = true; }
        }
        if (merged) {
          qs.erase(qs.begin() + std::ptrdiff_t(j));
          changed = true;
          --j;
        }
      }
    }
  }
  return before - qs.size();
}

Mesh meshModels(const std::vector<model::Model>& blocks, int lodLevel) {
  Mesh out;
  std::vector<litholattice::micro::MicroQuad> mq;
  const int lod = lodLevel < 0 ? 4 : lodLevel;
  for (size_t b = 0; b < blocks.size(); ++b) {
    mq.clear();
    litholattice::micro::MicroMeshParams p;
    p.originY = float(b);
    litholattice::micro::meshOctreeLOD(blocks[b].geometry, lod, p, mq);
    for (const auto& m : mq) out.quads.push_back(toQuad(m));
    out.octreeBytes += blocks[b].geometry.memoryBytes();
    out.leafCells += blocks[b].geometry.leafCount();
  }
  out.rawQuads = out.quads.size();
  mergeCoplanar(out.quads);
  return out;
}

const Mesh& mesh(Kind k) {
  // Built once on first use (C++11 magic static: thread-safe for the mesh workers), then
  // read-only: every instance of a kind shares this one model + quad list.
  static const std::array<Mesh, kKindCount> all = [] {
    std::array<Mesh, kKindCount> a;
    const std::string dir = std::string(LITHOKRAFT_MODELS_DIR) + "/foliage";
    for (int i = 0; i < kKindCount; ++i) {
      const Kind kind = Kind(i);
      std::vector<model::Model> blocks;
      bool ok = !std::string(LITHOKRAFT_MODELS_DIR).empty();
      for (int b = 0; ok && b < kindHeight(kind); ++b) {
        model::LoadResult r = model::loadModel(assetPath(dir, kind, b));
        ok = r.error == model::LoadError::Ok && !r.model.geometry.empty();
        if (ok) blocks.push_back(std::move(r.model));
      }
      if (!ok) blocks = buildModel(kind);
      a[size_t(i)] = meshModels(blocks);
      a[size_t(i)].fromAsset = ok;
    }
    return a;
  }();
  return all[size_t(k)];
}

std::vector<model::Model> buildRepeatorModel(int activeIndex) { return repeatorModel(activeIndex); }

namespace {
// PlayM (2026-11-01, CloudAtlas six-button art): one shipped model per delay setting,
// assets/models/repeator/repeator_d{2,4,8,16,32,64}.lthm (setting 0..5); the placeholder fills any gap.
constexpr int kRepeatorTicks[kRepeatorStates] = {2, 4, 8, 16, 32, 64};
struct RepeatorMeshes {
  std::array<Mesh, kRepeatorStates> mesh;
  std::string report;
};
const RepeatorMeshes& repeatorMeshes() {
  static const RepeatorMeshes all = [] {
    RepeatorMeshes a;
    const std::string dir = std::string(LITHOKRAFT_MODELS_DIR) + "/repeator";
    for (int i = 0; i < kRepeatorStates; ++i) {
      std::vector<model::Model> blocks;
      bool ok = !std::string(LITHOKRAFT_MODELS_DIR).empty();
      const std::string path = dir + "/repeator_d" + std::to_string(kRepeatorTicks[i]) + ".lthm";
      if (ok) {
        model::LoadResult r = model::loadModel(path);
        ok = r.error == model::LoadError::Ok && !r.model.geometry.empty();
        if (ok) blocks.push_back(std::move(r.model));
        else a.report += "repeator_d" + std::to_string(kRepeatorTicks[i]) + ".lthm: " +
                         (r.error == model::LoadError::Ok ? std::string("empty geometry") : r.message) + "; ";
      }
      if (!ok) blocks = repeatorModel(i);
      a.mesh[size_t(i)] = meshModels(blocks);
      a.mesh[size_t(i)].fromAsset = ok;
    }
    return a;
  }();
  return all;
}
}  // namespace

const Mesh& repeatorMesh(int activeIndex) {
  return repeatorMeshes().mesh[size_t(std::clamp(activeIndex, 0, kRepeatorStates - 1))];
}

std::string repeatorModelLoadErrors() { return repeatorMeshes().report; }

int repeatorButtonAt(double lx, double lz, int facing) {
  double x = lx * 16.0, z = lz * 16.0;
  for (int t = 0; t < (facing & 3); ++t) {  // inverse of (x, z) -> (16 - z, x)
    const double nx = z, nz = 16.0 - x;
    x = nx; z = nz;
  }
  (void)z;  // PlayM: six buttons in one row across x (CloudAtlas README: button i at x 2+2i .. 3.5+2i /16)
  return std::clamp(int((x - 2.0) / 2.0), 0, kRepeatorStates - 1);
}

Quad rotated(const Quad& q, int turns) {
  Quad r = q;
  for (int t = 0; t < (turns & 3); ++t) {
    const Quad s = r;  // (x, z) -> (16 - z, x)
    r.lo[0] = int16_t(16 - s.hi[2]); r.hi[0] = int16_t(16 - s.lo[2]);
    r.lo[2] = s.lo[0]; r.hi[2] = s.hi[0];
    static const uint8_t kFace[6] = {4, 5, 2, 3, 1, 0};  // +X->+Z, -X->-Z, +Z->-X, -Z->+X
    r.face = kFace[s.face];
  }
  return r;
}

}  // namespace lithokraft::foliage