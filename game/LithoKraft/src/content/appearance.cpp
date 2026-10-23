#include "lithokraft/content/appearance.h"
#include <array>
#include <string>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/chipor_viewport.h"
#include "lithokraft/content/foliage.h"  // Cortana 2026-10-15 2010 [GAME]
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215 [GAME]
#include <utility>
#include "litholattice/procgen/ramp_shape.h"  // Cortana 2026-10-16: Aluminum Ramp

#include <cstdlib>
static int llP2PerfOff() { static const int v = [] { const char* e = std::getenv("LL_P2_PERF_OFF"); return e ? int(std::strtol(e, nullptr, 0)) : 0; }(); return v; }  // Cortana 2026-10-16 1520: A/B perf switch (render/display only)

namespace lithokraft {
namespace {

int tierIndex(int tier) { return tier == 64 ? 2 : tier == 32 ? 1 : 0; }

// Cortana 2026-10-15 1755 [GAME]: the Emitor beam, render only. Walks from `cell` in
// horizontal direction d (0 N -Z, 1 E +X, 2 S +Z, 3 W -X) through Laysors and reports
// whether it reaches an Emitor whose front aperture faces back along the walk (the
// beam source). Bounded by the mesh pad (+-1 outside the sub-chunk): a beam whose
// source sits further across a sub-chunk seam is not seen (documented limit). Pure
// reads of the padded input: thread-safe on mesh workers.
bool beamFrom(const CustomBlock& cell, int d) {
  for (int k = 1;; ++k) {
    const int dx = kDirX[d] * k, dz = kDirZ[d] * k;
    const int px = cell.x + 1 + dx, pz = cell.z + 1 + dz;
    if (px < 0 || px >= kMeshPad || pz < 0 || pz >= kMeshPad) return false;
    const BlockId n = cell.neighbor(dx, 0, dz);
    if (n == kEmitor) {  // Cortana 2026-10-15 1900: only a powered (lit, bit 0) Emitor fires
      const CellState es = cell.neighborState(dx, 0, dz);
      return stFacing(es) == dirBack(d) && (es & 1) != 0;
    }
    if (n != kLaysor) return false;
  }
}

}  // namespace

const IBlockAppearance& blockAppearance() {
  static const LithoKraftAppearance appearance;
  return appearance;
}

bool LithoKraftAppearance::active(const BlockDef& def, CellState st) const {
  switch (roleOf(def)) {
    case SimRole::Lamp: return (st & 1) != 0;  // lamps keep "lit" in bit 0
    case SimRole::Torch: return stValue(st) != 0;
    default: return stOn(st);
  }
}

FaceLook LithoKraftAppearance::faceLook(const BlockDef& def, CellState st, FaceSlot slot, bool active) const {
  // Glowing trace-tree tips (kTraceTipRed = 40, kTraceTipBlue = 41): always-on
  // emissive, matched by stable block id - they are SimRole::None terrain, so
  // the role switch below would never catch them.
  if (def.id == "lithokraft:trace_tip_red" || def.id == "lithokraft:trace_tip_blue")
    return {kShadeAlphaBlend, 0};  // JD Vulkan push 2026-10-16: CloudAtlas ghost shell (lens disc removed, glass core inside); Vulkan = sorted blend pass, GL / software = screen-door
  // Cortana 2026-10-15 1755 [GAME]: lithography looks. The Laysor is BLENDED
  // translucency (alpha blend, not cutout / screen-door) and must win over the glass
  // rule below; the Emitor's front aperture is a light (always lit this pass --
  // power gating comes with the Etchor logic). faceLookCell adds the beam.
  if (def.id == "lithokraft:laysor") return {kShadeAlphaBlend, 0};
  // Cortana 2026-10-15 1900: the Emitor is a Lamp now -- only its front aperture lights, and
  // only while powered (active = lit bit); other faces stay plain (not the lamp's all-faces glow).
  if (def.id == "lithokraft:emitor") return slot == FaceSlot::Front && active ? FaceLook{kShadeEmissive, 0} : FaceLook{};
  const bool top = slot == FaceSlot::Top;
  switch (roleOf(def)) {
    case SimRole::ColorLamp:
      if (active) return {uint8_t(kShadePalette16 + tierIndex(def.tier)), uint8_t(stValue(st))};
      break;
    case SimRole::Lamp:
    case SimRole::FastLatch:
      if (active) return {kShadeEmissive, 0};
      break;
    case SimRole::Repeator:
      if (top) return {kShadePips, uint8_t(repeator::delayIndex(st) + 1)};  // 2215: legacy cube path (unused for Custom)
      break;
    case SimRole::Memor:
      if (top && active) return {kShadeCells, uint8_t(stValue(st))};
      break;
    case SimRole::None:  // glass family: solid, see-through, never powered -> translucent pass
      if ((def.flags & kSolid) && (def.flags & kTransparent)) return {kShadeGlass, 0};
      break;
    default: break;
  }
  return {};
}

// Position-aware look: the Wafer's silicon surface breathes trace energy (the
// ground pulse). The per-face biome blend is baked into the ChunkMesh at mesh
// time (waferPulseParams), so the face only needs the kShadeGroundPulse tag -
// the shader reads the blended params the mesh carries. Dead Zones use dead
// silicon, which never takes this path: unpowered is the point.
FaceLook LithoKraftAppearance::faceLookAt(const BlockDef& def, CellState st, FaceSlot slot, bool active,
                                          int /*x*/, int /*y*/, int /*z*/) const {
  if (def.id == "lithokraft:silicon") return {kShadeGroundPulse, 0};
  return faceLook(def, st, slot, active);
}

// Cortana 2026-10-15 1755 [GAME]: the Emitor beam as light. A Laysor lit by a beam
// (an Emitor front facing it through a run of Laysors) takes value 1 (the renderer
// adds a violet glow); the Etchor laser port (any side face) that the beam enters
// through at least one Laysor turns emissive. Nothing else changes look here.
std::optional<FaceLook> LithoKraftAppearance::faceLookCell(const CustomBlock& cell, FaceSlot slot, bool /*active*/) const {
  if (cell.id == kLaysor) {
    for (int d = 0; d < 4; ++d)
      if (beamFrom(cell, d)) return FaceLook{kShadeAlphaBlend, 1};
    return std::nullopt;  // unlit: faceLook's plain blend
  }
  // Cortana 2026-10-15 1900: laser ports are the Left/Right sides only (Elfie review LOW-2).
  if (cell.id == kEtchor && (slot == FaceSlot::Left || slot == FaceSlot::Right)) {
    const int f = stFacing(cell.state);
    const int d = slot == FaceSlot::Right ? dirRight(f) : dirLeft(f);
    if (cell.neighbor(kDirX[d], 0, kDirZ[d]) == kLaysor && beamFrom(cell, d)) return FaceLook{kShadeEmissive, 0};
  }
  return std::nullopt;
}

// Connected via sides (same-colour via below only = top of a stack, above only = bottom, both =
// mid, none = single; `<via>_side` when a pack lacks them) and the Redirector split faces.
std::optional<FaceMaterial> LithoKraftAppearance::faceMaterial(const CustomBlock& cell, FaceSlot slot, bool active) const {
  const BlockDef def = cell.def();
  const std::string tex(def.tex);
  auto named = [&](const std::string& n) -> std::optional<FaceMaterial> {
    if (const FaceMaterial* m = cell.materials.named(n)) return *m;
    return std::nullopt;
  };
  if (isVia(cell.id) && slot != FaceSlot::Top && slot != FaceSlot::Bottom) {
    const bool up = cell.neighbor(0, 1, 0) == cell.id, dn = cell.neighbor(0, -1, 0) == cell.id;
    // fix 3c (JD t2288u): the model's own side_* faces (tinted neutral art) first; the legacy
    // coloured `<via>_side_*` textures only for packs whose model lacks them.
    const int v = viaSideVariant(up, dn);
    if (const FaceMaterial* m = cell.materials.connectedSide(cell.id, v, active)) return *m;
    static constexpr const char* kLegacy[kConnectedSideCount] = {"_side_bottom", "_side_mid", "_side_top", "_side_single"};
    return named(tex + kLegacy[v]);
  }
  if (roleOf(def) == SimRole::Redirector && (stAux(cell.state) & 4)) {  // split mode
    if (slot == FaceSlot::Top) return named(tex + "_top_split");
    if (slot == FaceSlot::Front) return named(tex + (active ? "_front_split_both_on" : "_front_split"));
  }
  return std::nullopt;
}

// Redirector: transparent direction overlays over the top face (n = front, rotated with the
// block; red and blue lanes can show together). Catchor: 7-segment readout of its match value.
void LithoKraftAppearance::meshExtra(const CustomBlock& cell, MeshWriter& out) const {
  const BlockDef def = cell.def();
  if (cell.blocks.has(cell.neighbor(0, 1, 0), kOpaque)) return;  // top face hidden
  static constexpr int lo[3] = {0, 0, 0}, hi[3] = {16, 16, 16};
  static constexpr const char* kRel = "nesw";
  const int f = stFacing(cell.state);
  const std::string tex(def.tex);
  if (roleOf(def) == SimRole::Redirector) {
    const bool on = stOn(cell.state), split = stAux(cell.state) & 4;
    auto overlay = [&](const char* colour, int rel) {
      const std::string n = tex + "_overlay_" + colour + "_" + kRel[rel] + (on ? "" : "_off");
      if (const FaceMaterial* m = cell.materials.named(n)) out.face(cell, 2, lo, hi, *m, {kShadeCutout, 0}, f);
    };
    overlay("red", 0);
    overlay("blue", split ? ((stAux(cell.state) & 3) - f) & 3 : 0);
  } else if (roleOf(def) == SimRole::Catchor) {
    if (const FaceMaterial* m = cell.materials.named(tex + "_readout_mask"))
      out.face(cell, 2, lo, hi, *m, {kShadeReadout, uint8_t((stValue(cell.state) & 127) | (stOn(cell.state) ? 128 : 0))}, f);
  }
}

namespace {
// 2026-10-16 WaferBeta: shared helpers for custom-meshed directional machines. A box given in the
// north-facing local frame (front = -Z) is rotated to the cell's facing (foliage::rotated); its face's
// texture comes from the block's own model slot. Faces whose texture has a "tint:" mask get
// kShadeTraceTint: colour from the Trace behind the back port (blue channel -> blue ramp), lit when on.
FaceLook machineTintLook(const CustomBlock& cell, const FaceMaterial& fm, bool on) {
  const uint8_t ts = cell.materials.tintSlot(fm.layer);
  if (!ts) return {};
  const int bd = dirBack(stFacing(cell.state));
  const BlockDef nb = cell.blocks.def(cell.neighbor(kDirX[bd], 0, kDirZ[bd]));
  return {kShadeTraceTint, uint8_t((ts - 1) | (nb.channel == 2 ? 0x40 : 0) | (on ? 0x80 : 0))};
}
FaceSlot localSlot(int face) {  // north-facing local frame
  switch (face) { case 2: return FaceSlot::Top; case 3: return FaceSlot::Bottom; case 5: return FaceSlot::Front;
                  case 4: return FaceSlot::Back; case 0: return FaceSlot::Right; default: return FaceSlot::Left; }
}
// One local box [lo,hi) with its six faces; `top` overrides the top material (nullptr = model slot).
void machineBox(const CustomBlock& cell, MeshWriter& out, const int lo[3], const int hi[3], bool on,
                const FaceMaterial* top = nullptr) {
  const int facing = stFacing(cell.state);
  for (int f = 0; f < 6; ++f) {
    if (f == 3 && lo[1] == 0 && cell.blocks.has(cell.neighbor(0, -1, 0), kOpaque)) continue;  // floor-hidden
    if (f == 2 && hi[1] == 16 && cell.blocks.has(cell.neighbor(0, 1, 0), kOpaque)) continue;
    foliage::Quad q{};
    for (int a = 0; a < 3; ++a) { q.lo[a] = int16_t(lo[a]); q.hi[a] = int16_t(hi[a]); }
    q.face = uint8_t(f);
    const foliage::Quad r = foliage::rotated(q, facing);
    // side faces flush with the cell wall are culled by an opaque neighbour
    static const int kN[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    const bool flush = (r.face == 0 && r.hi[0] == 16) || (r.face == 1 && r.lo[0] == 0) || (r.face == 4 && r.hi[2] == 16) || (r.face == 5 && r.lo[2] == 0);
    if (flush && cell.blocks.has(cell.neighbor(kN[r.face][0], kN[r.face][1], kN[r.face][2]), kOpaque)) continue;
    const FaceMaterial& fm = (f == 2 && top) ? *top : cell.materials.face(cell.id, localSlot(f), on);
    const int l[3] = {r.lo[0], r.lo[1], r.lo[2]}, h[3] = {r.hi[0], r.hi[1], r.hi[2]};
    out.face(cell, r.face, l, h, fm, machineTintLook(cell, fm, on), (f == 2 || f == 3) ? facing : 0);
  }
}
// Cortana 2026-10-16 [GAME] (JD): Aluminum Ramp mesh. Planar faces only: the base square, up to
// four side faces (rectangles or triangles) and the slope (one quad when straight, two triangles
// for inner / outer corners). 5 to 7 quads per ramp; full faces against opaque neighbours are
// culled. Uses MeshWriter::quad (free-corner quads; triangles repeat a corner).
void meshRamp(const CustomBlock& cell, MeshWriter& out) {
  namespace rp = litholattice::procgen::ramp;
  const uint32_t st = uint32_t(cell.state) & 0x1fu;
  const bool hang = rp::hangingOf(st);
  const FaceMaterial fm = cell.materials.face(cell.id, FaceSlot::Top, false);
  const BlockRegistry& reg = cell.blocks;
  auto opaque = [&](int dx, int dy, int dz) { return reg.has(cell.neighbor(dx, dy, dz), kOpaque); };
  // Corner (xi, zi) in order around the top: (0,0) (1,0) (1,1) (0,1).
  constexpr int kCx[4] = {0, 1, 1, 0}, kCz[4] = {0, 0, 1, 1};
  int H[4];
  for (int i = 0; i < 4; ++i) H[i] = rp::cornerHigh(st, kCx[i], kCz[i]) ? 16 : 0;
  auto Y = [&](int y) { return hang ? 16 - y : y; };
  const int up = hang ? -1 : 1;
  // Emits p (4 corners, 1/16 units) wound so its normal points along `nrm`.
  auto poly = [&](int p[4][3], int uv[4][2], int lightFace, const int nrm[3]) {
    int e1[3], e2[3], n[3];
    for (int a = 0; a < 3; ++a) { e1[a] = p[1][a] - p[0][a]; e2[a] = p[2][a] - p[0][a]; }
    n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    if (n[0] * nrm[0] + n[1] * nrm[1] + n[2] * nrm[2] < 0) {
      const bool tri = p[2][0] == p[3][0] && p[2][1] == p[3][1] && p[2][2] == p[3][2];
      if (tri) {  // (a, b, c, c) -> (a, c, b, b)
        for (int a = 0; a < 3; ++a) { std::swap(p[1][a], p[2][a]); p[3][a] = p[2][a]; }
        std::swap(uv[1][0], uv[2][0]); std::swap(uv[1][1], uv[2][1]); uv[3][0] = uv[2][0]; uv[3][1] = uv[2][1];
      } else {    // (a, b, c, d) -> (a, d, c, b)
        for (int a = 0; a < 3; ++a) std::swap(p[1][a], p[3][a]);
        std::swap(uv[1][0], uv[3][0]); std::swap(uv[1][1], uv[3][1]);
      }
    }
    out.quad(cell, p, uv, lightFace, fm, FaceLook{});
  };
  // Base (bottom upright, top when hanging).
  if (!opaque(0, -up, 0)) {
    int p[4][3], uv[4][2];
    for (int i = 0; i < 4; ++i) { p[i][0] = kCx[i] * 16; p[i][1] = Y(0); p[i][2] = kCz[i] * 16; uv[i][0] = p[i][0]; uv[i][1] = p[i][2]; }
    const int nrm[3] = {0, -up, 0};
    poly(p, uv, hang ? 2 : 3, nrm);
  }
  // Sides: N (corners 0,1), E (1,2), S (2,3), W (3,0).
  constexpr int kSideA[4] = {0, 1, 2, 3}, kSideB[4] = {1, 2, 3, 0};
  constexpr int kNx[4] = {0, 1, 0, -1}, kNz[4] = {-1, 0, 1, 0}, kLight[4] = {5, 0, 4, 1};
  for (int d = 0; d < 4; ++d) {
    const int a = kSideA[d], b = kSideB[d];
    if (!H[a] && !H[b]) continue;
    if (H[a] && H[b] && opaque(kNx[d], 0, kNz[d])) continue;
    int p[4][3] = {{kCx[a] * 16, Y(0), kCz[a] * 16}, {kCx[b] * 16, Y(0), kCz[b] * 16},
                   {kCx[b] * 16, Y(H[b]), kCz[b] * 16}, {kCx[a] * 16, Y(H[a]), kCz[a] * 16}};
    if (!H[b]) { for (int k = 0; k < 3; ++k) p[2][k] = p[3][k]; }       // triangle: repeat corner
    else if (!H[a]) { for (int k = 0; k < 3; ++k) p[3][k] = p[2][k]; }
    int uv[4][2];
    for (int i = 0; i < 4; ++i) { uv[i][0] = (d & 1) ? p[i][2] : p[i][0]; uv[i][1] = 16 - p[i][1]; }
    const int nrm[3] = {kNx[d], 0, kNz[d]};
    poly(p, uv, kLight[d], nrm);
  }
  // Slope.
  int high = 0;
  for (int i = 0; i < 4; ++i) high += H[i] ? 1 : 0;
  const int nrmUp[3] = {0, up, 0};
  auto corner = [&](int i, int (&q)[3], int (&t)[2]) { q[0] = kCx[i] * 16; q[1] = Y(H[i]); q[2] = kCz[i] * 16; t[0] = q[0]; t[1] = q[2]; };
  if (high == 2) {
    int p[4][3], uv[4][2];
    for (int i = 0; i < 4; ++i) corner(i, p[i], uv[i]);
    poly(p, uv, hang ? 3 : 2, nrmUp);
  } else if (high == 1 || high == 3) {
    int o = 0;
    for (int i = 0; i < 4; ++i) if ((H[i] != 0) == (high == 1)) o = i;
    const int b = (o + 1) & 3, c = (o + 2) & 3, d = (o + 3) & 3;
    int p[4][3], uv[4][2];
    corner(o, p[0], uv[0]); corner(b, p[1], uv[1]); corner(d, p[2], uv[2]); corner(d, p[3], uv[3]);
    poly(p, uv, hang ? 3 : 2, nrmUp);
    if (high == 3 && !opaque(0, up, 0)) {  // flat full-height triangle
      corner(b, p[0], uv[0]); corner(c, p[1], uv[1]); corner(d, p[2], uv[2]); corner(d, p[3], uv[3]);
      poly(p, uv, hang ? 3 : 2, nrmUp);
    }
  }
}

// Proposed by JD, Built by Cortana (2026-10-16): Highway Ramp mesh. One half of the paired 1-in-2
// slope: base, slope quad (corner heights from ramp::highway::height16), side trapezoids/triangles,
// and the end faces. The seam face toward the partner half is never emitted (it is interior), and
// full-height faces against opaque neighbours are culled. Same art as the Aluminum Ramp.
void meshHighwayRamp(const CustomBlock& cell, MeshWriter& out) {
  namespace hw = litholattice::procgen::ramp::highway;
  const uint32_t st = uint32_t(cell.state) & 0x7u;
  const FaceMaterial fm = cell.materials.face(cell.id, FaceSlot::Top, false);
  const BlockRegistry& reg = cell.blocks;
  auto opaque = [&](int dx, int dy, int dz) { return reg.has(cell.neighbor(dx, dy, dz), kOpaque); };
  constexpr int kCx[4] = {0, 1, 1, 0}, kCz[4] = {0, 0, 1, 1};
  int H[4];
  for (int i = 0; i < 4; ++i) H[i] = hw::height16(st, kCx[i] * 16, kCz[i] * 16);
  auto poly = [&](int p[4][3], int uv[4][2], int lightFace, const int nrm[3]) {
    int e1[3], e2[3], n[3];
    for (int a = 0; a < 3; ++a) { e1[a] = p[1][a] - p[0][a]; e2[a] = p[2][a] - p[0][a]; }
    n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
    if (n[0] * nrm[0] + n[1] * nrm[1] + n[2] * nrm[2] < 0) {
      const bool tri = p[2][0] == p[3][0] && p[2][1] == p[3][1] && p[2][2] == p[3][2];
      if (tri) {
        for (int a = 0; a < 3; ++a) { std::swap(p[1][a], p[2][a]); p[3][a] = p[2][a]; }
        std::swap(uv[1][0], uv[2][0]); std::swap(uv[1][1], uv[2][1]); uv[3][0] = uv[2][0]; uv[3][1] = uv[2][1];
      } else {
        for (int a = 0; a < 3; ++a) std::swap(p[1][a], p[3][a]);
        std::swap(uv[1][0], uv[3][0]); std::swap(uv[1][1], uv[3][1]);
      }
    }
    out.quad(cell, p, uv, lightFace, fm, FaceLook{});
  };
  if (!opaque(0, -1, 0)) {
    int p[4][3], uv[4][2];
    for (int i = 0; i < 4; ++i) { p[i][0] = kCx[i] * 16; p[i][1] = 0; p[i][2] = kCz[i] * 16; uv[i][0] = p[i][0]; uv[i][1] = p[i][2]; }
    const int nrm[3] = {0, -1, 0};
    poly(p, uv, 3, nrm);
  }
  constexpr int kSideA[4] = {0, 1, 2, 3}, kSideB[4] = {1, 2, 3, 0};
  constexpr int kNx[4] = {0, 1, 0, -1}, kNz[4] = {-1, 0, 1, 0}, kLight[4] = {5, 0, 4, 1};
  const int pdx = hw::partnerDx(st), pdz = hw::partnerDz(st);
  for (int d = 0; d < 4; ++d) {
    const int a = kSideA[d], b = kSideB[d];
    if (!H[a] && !H[b]) continue;
    if (kNx[d] == pdx && kNz[d] == pdz && cell.neighbor(pdx, 0, pdz) == cell.id) continue;  // seam
    if (H[a] == 16 && H[b] == 16 && opaque(kNx[d], 0, kNz[d])) continue;
    int p[4][3] = {{kCx[a] * 16, 0, kCz[a] * 16}, {kCx[b] * 16, 0, kCz[b] * 16},
                   {kCx[b] * 16, H[b], kCz[b] * 16}, {kCx[a] * 16, H[a], kCz[a] * 16}};
    if (!H[b]) { for (int k = 0; k < 3; ++k) p[2][k] = p[3][k]; }
    else if (!H[a]) { for (int k = 0; k < 3; ++k) p[3][k] = p[2][k]; }
    int uv[4][2];
    for (int i = 0; i < 4; ++i) { uv[i][0] = (d & 1) ? p[i][2] : p[i][0]; uv[i][1] = 16 - p[i][1]; }
    const int nrm[3] = {kNx[d], 0, kNz[d]};
    poly(p, uv, kLight[d], nrm);
  }
  {
    int p[4][3], uv[4][2];
    for (int i = 0; i < 4; ++i) { p[i][0] = kCx[i] * 16; p[i][1] = H[i]; p[i][2] = kCz[i] * 16; uv[i][0] = p[i][0]; uv[i][1] = p[i][2]; }
    const int nrm[3] = {0, 1, 0};
    poly(p, uv, 2, nrm);
  }
}

}  // namespace

// Wire: one flat piece on the cell floor, chosen from its connections and tinted by strength.
void LithoKraftAppearance::meshCustom(const CustomBlock& cell, MeshWriter& out) const {
  const BlockRegistry& reg = cell.blocks;
  const BlockDef def = cell.def();
  if (cell.id == kAluminumRamp) { meshRamp(cell, out); return; }  // Cortana 2026-10-16
  if (cell.id == kHighwayRamp) { meshHighwayRamp(cell, out); return; }  // Proposed by JD, Built by Cortana
  // Cortana 2026-10-15 2010 [GAME]: 3D foliage instance. Expands the kind's shared quad list
  // (built once, read-only) rotated by the position-hash variant; nothing per instance.
  if (cell.id >= kFoliageCattail && cell.id <= kFoliageMoss) {  // ids 82-87, consecutive
    const auto kind = foliage::Kind(cell.id - kFoliageCattail);
    const int wx = cell.in.cx * 16 + cell.x, wy = cell.in.sy * 16 + cell.y, wz = cell.in.cz * 16 + cell.z;
    const int turns = foliage::variantAt(wx, wy, wz);
    // Cortana 2026-10-15 2100 [GAME] (review M1): the slot names are built once (no per-cell
    // string allocation); the lookups stay per cell because a pack reload replaces BlockMaterials
    // (caching FaceMaterial pointers across meshes could dangle).
    constexpr int kSlots = foliage::kLastMaterial - foliage::kFirstMaterial + 1;
    static const std::array<std::string, kSlots> kNames = [] {
      std::array<std::string, kSlots> n;
      for (int m = 0; m < kSlots; ++m) n[size_t(m)] = "foliage_m" + std::to_string(foliage::kFirstMaterial + m);
      return n;
    }();
    const FaceMaterial* mats[kSlots];
    for (int m = 0; m < kSlots; ++m) mats[m] = cell.materials.named(kNames[size_t(m)]);
    // Review H2: the models stand on their cell floor; bottom faces at y=0 are hidden by solid ground.
    const bool groundBelow = reg.has(cell.neighbor(0, -1, 0), kSolid) && !reg.has(cell.neighbor(0, -1, 0), kTransparent);
    for (const foliage::Quad& q0 : foliage::mesh(kind).quads) {
      if (q0.material < foliage::kFirstMaterial || q0.material > foliage::kLastMaterial) continue;
      const FaceMaterial* fm = mats[q0.material - foliage::kFirstMaterial];
      if (!fm) continue;
      if (groundBelow && q0.face == 3 && q0.lo[1] == 0) continue;
      const foliage::Quad q = foliage::rotated(q0, turns);
      const int lo[3] = {q.lo[0], q.lo[1], q.lo[2]}, hi[3] = {q.hi[0], q.hi[1], q.hi[2]};
      FaceLook fl{};
      if (q.material == foliage::mat::kGlow) fl.mode = kShadeEmissive;  // Tron pass: glow lines self-lit
      fl.mode = uint8_t(fl.mode | kShadeLodNear);  // Cortana 2026-10-16: detail = near LOD only
      out.face(cell, q.face, lo, hi, *fm, fl);
    }
    // Cortana 2026-10-16 [GAME] (JD): far LOD cross-hatch, tagged far-only. Renderers draw
    // one set per sub-chunk by its distance (renderer.h kShadeLodNear / kShadeLodFar).
    for (const foliage::Quad& q0 : foliage::crossMesh(kind).quads) {
      if (q0.material < foliage::kFirstMaterial || q0.material > foliage::kLastMaterial) continue;
      const FaceMaterial* fm = mats[q0.material - foliage::kFirstMaterial];
      if (!fm) continue;
      const foliage::Quad q = foliage::rotated(q0, turns);
      const int lo[3] = {q.lo[0], q.lo[1], q.lo[2]}, hi[3] = {q.hi[0], q.hi[1], q.hi[2]};
      FaceLook fl{};
      if (q.material == foliage::mat::kGlow) fl.mode = kShadeEmissive;
      fl.mode = uint8_t(fl.mode | kShadeLodFar);
      out.face(cell, q.face, lo, hi, *fm, fl);
    }
    return;
  }
  // 2026-10-16 WaferBeta (JD): Repeator = textured 3/16 slab with CloudAtlas's base + tint-mask art.
  // The top is top_d{2,4,8,16}_base by delay index (replaces the pip mask + voxel buttons); the
  // tinted channels follow the Trace feeding the back port and light up when powered.
  if (roleOf(def) == SimRole::Repeator) {
    static constexpr const char* kTops[4] = {"repeator_top_d2_base", "repeator_top_d4_base", "repeator_top_d8_base", "repeator_top_d16_base"};
    static constexpr int lo[3] = {0, 0, 0}, hi[3] = {16, 3, 16};
    machineBox(cell, out, lo, hi, stOn(cell.state), cell.materials.named(kTops[repeator::delayIndex(cell.state) & 3]));
    return;
  }
  // 2026-10-16 WaferBeta: Lithographor cuboids (CloudAtlas lithographor.cuboids.json, hand-ported:
  // the engine has no loader for that format). Body, lens column, cap.
  if (cell.id == kLithographor) {
    const bool on = stOn(cell.state);
    static constexpr int b0[3] = {0, 0, 0}, b1[3] = {16, 11, 16}, l0[3] = {4, 11, 4}, l1[3] = {12, 15, 12}, c0[3] = {5, 15, 5}, c1[3] = {11, 16, 11};
    machineBox(cell, out, b0, b1, on, cell.materials.named("lithographor_side"));  // body top hidden mostly; plain
    machineBox(cell, out, l0, l1, on, cell.materials.named("lithographor_top"));
    machineBox(cell, out, c0, c1, on);
    return;
  }
  if (cell.id == kChiporViewport) {  // Chipor viewport (workstream 4): transparent top + miniature interior
    chipor::meshViewportCell(cell, out);
    return;
  }
  if (roleOf(def) != SimRole::Wire) return;
  const int ch = def.channel - 1;
  const bool upBlocked = reg.has(cell.neighbor(0, 1, 0), kCutsWire);
  int arms = 0, count = 0;
  for (int d = 0; d < 4; ++d) {
    const BlockDef nd = reg.def(cell.neighbor(kDirX[d], 0, kDirZ[d]));
    bool link = (roleOf(nd) == SimRole::Wire && nd.channel == def.channel) ||
                (roleOf(nd) != SimRole::None && roleOf(nd) != SimRole::Wire && roleOf(nd) != SimRole::Torch);
    const BlockDef up = reg.def(cell.neighbor(kDirX[d], 1, kDirZ[d]));
    const BlockDef dn = reg.def(cell.neighbor(kDirX[d], -1, kDirZ[d]));
    if (!upBlocked && roleOf(up) == SimRole::Wire && up.channel == def.channel) link = true;
    if (!(nd.flags & kCutsWire) && roleOf(dn) == SimRole::Wire && dn.channel == def.channel) link = true;
    if (link) { arms |= 1 << d; ++count; }
  }
  WirePiece piece = WirePiece::Dot;
  int rot = 0;
  if (count == 1) { piece = WirePiece::End; for (int d = 0; d < 4; ++d) if (arms >> d & 1) rot = (d + 2) & 3; }  // canonical arm S
  else if (count == 2 && (arms == 5 || arms == 10)) { piece = WirePiece::Straight; rot = arms == 10 ? 1 : 0; }
  else if (count == 2) { piece = WirePiece::Corner; for (int d = 0; d < 4; ++d) if ((arms >> d & 1) && (arms >> ((d + 1) & 3) & 1)) rot = d; }
  else if (count == 3) { piece = WirePiece::T; for (int d = 0; d < 4; ++d) if (!(arms >> d & 1)) rot = d; }  // canonical missing N
  else if (count == 4) piece = WirePiece::Cross;
  // On a floor: a strip on the cell floor, facing up. Hanging from a ceiling (nothing solid
  // below, solid above): the same strip under the support, facing down; the -Y face mirrors U,
  // so east / west arms swap and the rotation is mirrored to keep the arms on their wires.
  const bool hanging = !reg.has(cell.neighbor(0, -1, 0), kSolid) && reg.has(cell.neighbor(0, 1, 0), kSolid);
  static constexpr int lo[3] = {0, 0, 0}, hi[3] = {16, 1, 16}, clo[3] = {0, 15, 0}, chi[3] = {16, 16, 16};
  const FaceLook fl{uint8_t(ch ? kShadeStripTint1 : kShadeStripTint0), uint8_t(stValue(cell.state))};
  if (hanging) out.face(cell, 3, clo, chi, cell.materials.wirePiece(cell.id, piece), fl, (4 - rot) & 3);
  else out.face(cell, 2, lo, hi, cell.materials.wirePiece(cell.id, piece), fl, rot);
}

void LithoKraftAppearance::bindMaterials(const BlockDef& def, const BlockModel& model, MaterialBindings& b) const {
  const int paletteRow = model.palette.valid() ? int(model.palette.value) : 0;
  switch (roleOf(def)) {
    case SimRole::Wire:
      if (def.shape == Shape::Custom && def.channel >= 1 && def.channel <= 2) {  // vias (cubes) keep the strip palettes
        b.stripPalette[size_t(def.channel - 1)] = paletteRow;
        b.stripRing[size_t(def.channel - 1)] = model.mask("ring").value;
        b.stripDot[size_t(def.channel - 1)] = model.mask("dot").value;
      }
      break;
    case SimRole::ColorLamp: {
      const size_t t = size_t(tierIndex(def.tier));
      b.tierPalette[t] = paletteRow;
      b.tierMask[t] = model.mask("main").value;
      b.tierRim[t] = model.mask("rim").value;
      break;
    }
    case SimRole::Repeator: b.pipMask = model.mask("delay").value; break;  // WaferBeta: unused now (tint masks replace it)
    case SimRole::Memor: b.cellMask = model.mask("value").value; break;
    default: break;
  }
}

// Attached torches and switches: box models (JSON, the glTF-subset format of box_model.h).
// Authored standing on the support; the mesher turns them toward the attach face.
const char* const kTorchModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "torch", "slot": "top"}],
  "meshes": [{"name": "stick", "boxes": [{"min": [7, 0, 7], "max": [9, 10, 9], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";
const char* const kSwitchModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "base", "slot": "bottom"}, {"name": "lever", "slot": "top"}],
  "meshes": [{"name": "base", "boxes": [{"min": [5, 0, 4], "max": [11, 2, 12], "material": 0}]},
             {"name": "lever", "boxes": [{"min": [7, 2, 7], "max": [9, 9, 9], "material": 1}]}],
  "nodes": [{"mesh": 0}, {"mesh": 1}]})";

const char* const kButtonModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "cap", "slot": "top"}],
  "meshes": [{"name": "cap", "boxes": [{"min": [5, 0, 6], "max": [11, 2, 10], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";

const BoxModel* LithoKraftAppearance::boxModel(const BlockDef& def) const {
  static const std::optional<BoxModel> torch = BoxModel::parse(kTorchModel, nullptr);
  static const std::optional<BoxModel> sw = BoxModel::parse(kSwitchModel, nullptr);
  if (roleOf(def) == SimRole::Torch) return torch ? &*torch : nullptr;
  static const std::optional<BoxModel> button = BoxModel::parse(kButtonModel, nullptr);
  if (roleOf(def) == SimRole::Switch) return sw ? &*sw : nullptr;
  if (roleOf(def) == SimRole::Button) return button ? &*button : nullptr;
  return nullptr;
}

}  // namespace lithokraft
