#include <atomic>
#include "litholattice/core/log.h"
#include "lithokraft/content/appearance.h"
#include <algorithm>
#include <array>
#include <string>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/chipor_viewport.h"
#include "lithokraft/content/foliage.h"  // Cortana 2026-10-15 2010 [GAME]
#include "lithokraft/content/post.h"  // 2026-10-25 Posts
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215 [GAME]
#include "lithokraft/content/trace_conn.h"  // 2026-11-01 PlayL: Trace connection states
#include <utility>
#include "litholattice/procgen/ramp_shape.h"  // Cortana 2026-10-16: Aluminum Ramp

#include <cstdlib>
[[maybe_unused]] static int llP2PerfOff() { static const int v = [] { const char* e = std::getenv("LL_P2_PERF_OFF"); return e ? int(std::strtol(e, nullptr, 0)) : 0; }(); return v; }  // Cortana 2026-10-16 1520: A/B perf switch (render/display only)

namespace lithokraft {
// PlayM: quads emitted from the Repeator .lthm models (selftest: proves the render path uses them).
std::atomic<uint64_t> g_repeatorModelQuads{0};
uint64_t repeatorModelQuadsEmitted() { return g_repeatorModelQuads.load(std::memory_order_relaxed); }
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
  static const cLithoKraftAppearance appearance;
  return appearance;
}

bool cLithoKraftAppearance::active(const BlockDef& def, CellState st) const {
  switch (roleOf(def)) {
    case SimRole::Lamp: return (st & 1) != 0;  // lamps keep "lit" in bit 0
    case SimRole::Torch: return stValue(st) != 0;
    default: return stOn(st);
  }
}

FaceLook cLithoKraftAppearance::faceLook(const BlockDef& def, CellState st, FaceSlot slot, bool active) const {
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
    case SimRole::Latchor:
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
FaceLook cLithoKraftAppearance::faceLookAt(const BlockDef& def, CellState st, FaceSlot slot, bool active,
                                          int /*x*/, int /*y*/, int /*z*/) const {
  if (def.id == "lithokraft:silicon") return {kShadeGroundPulse, 0};
  return faceLook(def, st, slot, active);
}

// Cortana 2026-10-15 1755 [GAME]: the Emitor beam as light. A Laysor lit by a beam
// (an Emitor front facing it through a run of Laysors) takes value 1 (the renderer
// adds a violet glow); the Etchor laser port (any side face) that the beam enters
// through at least one Laysor turns emissive. Nothing else changes look here.
std::optional<FaceLook> cLithoKraftAppearance::faceLookCell(const CustomBlock& cell, FaceSlot slot, bool /*active*/) const {
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
std::optional<FaceMaterial> cLithoKraftAppearance::faceMaterial(const CustomBlock& cell, FaceSlot slot, bool active) const {
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
void cLithoKraftAppearance::meshExtra(const CustomBlock& cell, MeshWriter& out) const {
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
    // 2026-10-25 (JD 8:26): the readout mask is authored for the opposite quarter turn, so with
    // rotation f the digits read upside down from the input side; +2 turns them upright for every facing.
    if (const FaceMaterial* m = cell.materials.named(tex + "_readout_mask"))
      out.face(cell, 2, lo, hi, *m, {kShadeReadout, uint8_t((stValue(cell.state) & 127) | (stOn(cell.state) ? 128 : 0))}, f);  // 2026-10-31 11:20 PM: mask now turns with the quad (shader), rotation = facing
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
  // 2026-10-24 (JD 6:13 PM: red flash when a blue-fed Repeator depowers): when the back source is
  // gone or colourless (LED lifted / off), keep the colour of the Trace it drives instead of
  // falling back to red for the delay ticks it stays on.
  int ch = nb.channel;
  // 2026-10-31 (JD t3680u): a Repeator fed by a Repeator carries the chain's colour in its state.
  if (ch == 0 && roleOf(cell.def()) == SimRole::Repeator && repeator::colourOf(cell.state) > 1) ch = repeator::colourOf(cell.state);
  if (ch == 0) {
    const int fd = stFacing(cell.state);
    ch = cell.blocks.def(cell.neighbor(kDirX[fd], 0, kDirZ[fd])).channel;
  }
  // 0x20 = purple (Switch-fed Repeator chain, 2026-10-31); tint slots stay below 32.
  return {kShadeTraceTint, uint8_t(((ts - 1) & 31) | (ch == 2 ? 0x40 : 0) | (ch == 3 ? 0x20 : 0) | (on ? 0x80 : 0))};
}
// 2026-10-24 (JD): does component `nd` (state s) read or drive Trace on world side `side`
// (the side of the component facing the Trace)? Mirrors the sim (logic_components.cpp):
// Repeator / Fast Latch back in + front out; Keyboard front + right (Enter / Backspace);
// Catchor its input (back) + current output side; Redirector / Thresholdor / Pulsor / Memor all four
// (back + both sides in, front out); everything else (Switch, Button, lamps, Memor-likes) any side.
bool tracePortFaces(const BlockDef& nd, CellState s, int side) {
  const int f = stFacing(s);
  switch (roleOf(nd)) {
    case SimRole::Repeator:
    case SimRole::Latchor: return side == f || side == dirBack(f);
    case SimRole::Keyboard: return side == f || side == dirRight(f);
    case SimRole::Catchor: {
      const int m = stAux(s) & 3;
      if (m == 3) return true;  // not wired yet: binds to the first Trace it sees
      const int out = m == 1 ? (f + 3) & 3 : m == 2 ? (f + 1) & 3 : f;
      return side == dirBack(f) || side == out;
    }
    default: return true;
  }
}
FaceSlot localSlot(int face) {  // north-facing local frame
  switch (face) { case 2: return FaceSlot::Top; case 3: return FaceSlot::Bottom; case 5: return FaceSlot::Front;
                  case 4: return FaceSlot::Back; case 0: return FaceSlot::Right; default: return FaceSlot::Left; }
}
// One local box [lo,hi) with its six faces; `top` overrides the top material (nullptr = model slot).
void machineBox(const CustomBlock& cell, MeshWriter& out, const int lo[3], const int hi[3], bool on,
                const FaceMaterial* top = nullptr, int topTurns = 0, const FaceMaterial* side = nullptr) {
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
    // 2026-11-01 (box): `side` overrides the two local side faces (E / W), e.g. Fusor art.
    const FaceMaterial& fm = (f == 2 && top) ? *top : ((f == 0 || f == 1) && side) ? *side : cell.materials.face(cell.id, localSlot(f), on);
    const int l[3] = {r.lo[0], r.lo[1], r.lo[2]}, h[3] = {r.hi[0], r.hi[1], r.hi[2]};
    out.face(cell, r.face, l, h, fm, machineTintLook(cell, fm, on), (f == 2 || f == 3) ? ((facing + topTurns) & 3) : 0);
  }
}
// 2026-10-24 (CloudAtlas slab models, render only): Redirector, Thresholdor, Pulsor and Catchor draw
// as a 2/16 slab with 1/16 pads (north = front = output), instead of a full cube. Collision, flags
// and the sim are untouched; neighbours no longer hide their faces against these cells (mesher).
// The Redirector lane overlays and the Catchor readout are drawn on the slab top (y = 2).
} // namespace
// 2026-11-01 (JD, CloudAtlas lithokraft-fusor SPEC): Fusor = 2/16 slab + 4 side pads + fuse housing. LL_FUSOR_SLAB=0: cube.
static bool fusorSlab() { static const bool v = [] { const char* e = std::getenv("LL_FUSOR_SLAB"); return !(e && e[0] == '0'); }(); return v; }
bool cLithoKraftAppearance::shapedRender(const BlockDef& def) const {
  const SimRole r = roleOf(def);
  return def.shape == Shape::Cube &&
         (r == SimRole::Redirector || r == SimRole::Thresholdor || r == SimRole::Pulsor || r == SimRole::Catchor ||
          r == SimRole::Keyboard || r == SimRole::Memor ||  // 2026-10-24 7:25 PM (JD approved): CloudAtlas Memor partial block
          r == SimRole::Latchor || r == SimRole::Randor || r == SimRole::cFusorSum ||
          (r == SimRole::cFusor && fusorSlab()));  // 2026-10-24 (JD approved placeholder): CloudAtlas fastlatch.json, out N + in S pads  // 2026-10-24 (JD): Keyboard = repeater-size slab (CloudAtlas keyboard.json)
}
bool cLithoKraftAppearance::meshShaped(const CustomBlock& cell, MeshWriter& out) const {
  const BlockDef def = cell.def();
  if (!shapedRender(def)) return false;
  const SimRole r = roleOf(def);
  const bool on = active(def, cell.state);
  const int f = stFacing(cell.state);
  const FaceMaterial frontM = cell.materials.face(cell.id, FaceSlot::Front, on);
  const int slo[3] = {0, 0, 0}, shi[3] = {16, 2, 16};
  // 2026-10-25 Thresholdor subtract mode (aux bit 0): CloudAtlas's tinted top when the pack has
  // it (thresholdor_top_subtract[_on]); until then a placeholder indicator pad (below).
  const bool thrSub = r == SimRole::Thresholdor && (stAux(cell.state) & 1);
  const FaceMaterial* subTop = nullptr;
  if (thrSub) {
    // 2026-10-25 (JD, CloudAtlas readout package): prefer the readout-window subtract tops.
    subTop = on ? cell.materials.named("thresholdor_top_readout_subtract_on") : nullptr;
    if (!subTop) subTop = cell.materials.named("thresholdor_top_readout_subtract");
    if (!subTop && on) subTop = cell.materials.named("thresholdor_top_subtract_on");
    if (!subTop) subTop = cell.materials.named("thresholdor_top_subtract");
  }
  // 2026-11-01 (JD): Fusor art - tripped (aux bit 0) slab edges wear fusor_edge_tripped (pass art
  // otherwise, the model's side slot); the fuse housing's E / W sides wear fusor_housing_side.
  // LL_FUSOR_ART=0: old (edge_pass everywhere on the sides). Missing textures fall back silently.
  const FaceMaterial* fusorEdge = nullptr;
  const FaceMaterial* fusorHousing = nullptr;
  if (r == SimRole::cFusor) {
    static const bool art = [] { const char* e = std::getenv("LL_FUSOR_ART"); return !(e && e[0] == '0'); }();
    if (art) {
      // PlayO (JD): CloudAtlas Breakor art: tripped = breakor_top_tripped lid; the housing uses the model faces.
      if (stAux(cell.state) & 1) subTop = cell.materials.named("breakor_top_tripped_base");
      if (!cell.materials.named("breakor_top_closed_base")) {  // pack without Breakor art: old Fusor look
        if (stAux(cell.state) & 1) fusorEdge = cell.materials.named("fusor_edge_tripped");
        fusorHousing = cell.materials.named("fusor_housing_side");
      }
    }
  }
  machineBox(cell, out, slo, shi, on, subTop, 0, fusorEdge);
  auto pad = [&](int x0, int z0, int x1, int z1, int y1 = 3) {
    const int lo[3] = {x0, 2, z0}, hi[3] = {x1, y1, z1};
    machineBox(cell, out, lo, hi, on, &frontM);
  };
  pad(6, 0, 10, 2);    // output (front, north)
  // 2026-10-24 Keyboard (CloudAtlas keyboard.json): out_pad_north [6,2,0]-[10,3,2] (key pulses, front) and
  // ctrl_pad_east [14,2,6]-[16,3,10] (Enter / Backspace, the right side); no back input.
  if (r == SimRole::Keyboard) { pad(14, 6, 16, 10); return true; }
  if (r == SimRole::Memor) {
    // CloudAtlas memor_stored.json: slab [0,0,0]-[16,2,16], pads N out / S in / W+E clear, cell_display
    // bezel [2,2,2]-[14,3,14] whose top is the readout (the stored value, kShadeCells, as before).
    // 2026-10-24 7:52 PM CloudAtlas engine-fit memor.json: no pads; readout [4,2,4]-[12,3,12].
    const int blo[3] = {4, 2, 4}, bhi[3] = {12, 3, 12};
    const FaceMaterial& topM = cell.materials.face(cell.id, FaceSlot::Top, on);
    const BlockDef d = cell.def();
    const FaceLook rl = faceLookAt(d, cell.state, FaceSlot::Top, on, cell.in.cx * 16 + cell.x, cell.in.sy * 16 + cell.y,
                                   cell.in.cz * 16 + cell.z);
    for (int fc = 0; fc < 6; ++fc) {
      if (fc == 3) continue;
      out.face(cell, fc, blo, bhi, fc == 2 ? topM : frontM, fc == 2 ? rl : FaceLook{}, fc == 2 ? (f & 3) : 0);
    }
    return true;
  }
  pad(6, 14, 10, 16);  // input (back)
  if (r == SimRole::cFusor) {  // 2026-11-01 (JD): pads [6,2,0]-[10,3,2] style on all 4 sides; housing [4,2,6]-[12,5,10]
    pad(0, 6, 2, 10); pad(14, 6, 16, 10);
    const int hlo[3] = {4, 2, 6}, hhi[3] = {12, 5, 10};
    machineBox(cell, out, hlo, hhi, on, nullptr, 0, fusorHousing);  // N / S = fuse art (front / back slots), E / W = side, lid = top
    return true;
  }
  if (r == SimRole::Redirector) { pad(0, 6, 2, 10); pad(14, 6, 16, 10); }
  if (r == SimRole::cFusorSum) { pad(0, 6, 2, 10); pad(14, 6, 16, 10); }  // PlayO (JD): Fusor adder side inputs (W / E)
  if (r == SimRole::Thresholdor) {
    // 2026-10-24 state model (render only): the reference pads wear the LOW (left) / HIGH (right)
    // face textures, and the output pad rises a pixel while the input passes (state on).
    const FaceMaterial lowM = cell.materials.face(cell.id, FaceSlot::Left, on);
    const FaceMaterial highM = cell.materials.face(cell.id, FaceSlot::Right, on);
    const int llo[3] = {0, 2, 6}, lhi[3] = {2, 3, 10}, hlo[3] = {14, 2, 6}, hhi[3] = {16, 3, 10};
    machineBox(cell, out, llo, lhi, on, &lowM);
    machineBox(cell, out, hlo, hhi, on, &highM);
    if (on) pad(6, 0, 10, 2, 4);
    if (thrSub && !subTop) {  // placeholder: a raised LOW-textured bar across the slab centre = "minus"
      const int mlo[3] = {4, 2, 7}, mhi[3] = {12, 3, 9};
      machineBox(cell, out, mlo, mhi, on, &lowM);
    }
  }
  if (r == SimRole::Randor) {  // 2026-10-31 (JD, CloudAtlas): clock pads on both sides; top shows the range N
    const FaceMaterial sideM = cell.materials.face(cell.id, FaceSlot::Left, on);
    const int llo[3] = {0, 2, 6}, lhi[3] = {2, 3, 10}, hlo[3] = {14, 2, 6}, hhi[3] = {16, 3, 10};
    machineBox(cell, out, llo, lhi, on, &sideM);
    machineBox(cell, out, hlo, hhi, on, &sideM);
    const int dlo[3] = {0, 0, 0}, dhi[3] = {16, 2, 16};
    const int n = stValue(cell.state) > 0 && stValue(cell.state) <= kMaxSignal ? stValue(cell.state) : 1;
    const FaceMaterial& topM = cell.materials.face(cell.id, FaceSlot::Top, on);
    out.face(cell, 2, dlo, dhi, topM, {kShadeDigitsRnd, uint8_t(n | 128)}, f);
  }
  if (r == SimRole::Pulsor) pad(6, 6, 10, 10);  // gem (CloudAtlas [6,2,5.5]-[10,3,10], rounded)
  const int tlo[3] = {0, 0, 0}, thi[3] = {16, 2, 16};  // top overlays at slab height
  const std::string tex(def.tex);
  if (r == SimRole::Redirector) {
    static constexpr const char* kRel = "nesw";
    const bool split = stAux(cell.state) & 4;
    auto overlay = [&](const char* colour, int rel) {
      const std::string n = tex + "_overlay_" + colour + "_" + kRel[rel] + (on ? "" : "_off");
      if (const FaceMaterial* m = cell.materials.named(n)) out.face(cell, 2, tlo, thi, *m, {kShadeCutout, 0}, f);
    };
    overlay("red", 0);
    overlay("blue", split ? ((stAux(cell.state) & 3) - f) & 3 : 0);
  } else if (r == SimRole::Catchor || r == SimRole::Thresholdor) {
    // 2026-10-25 (JD 8:26): the readout mask is authored for the opposite quarter turn, so with
    // rotation f the digits read upside down from the input side; +2 turns them upright for every facing.
    // 2026-10-25 (JD, CloudAtlas): the Thresholdor shares the Catchor mask format; its state value is
    // the back input in compare mode (container level in read mode) and the output in subtract mode.
    if (const FaceMaterial* m = cell.materials.named(tex + "_readout_mask"))
      out.face(cell, 2, tlo, thi, *m, {kShadeReadout, uint8_t((stValue(cell.state) & 127) | (stOn(cell.state) ? 128 : 0))}, f);  /* 2026-10-31 11:20 PM (JD): the shader un-rotated the mask UV, pinning digits to world north (right only when facing N); shader now samples the rotated UV, so rotation = facing for every facing */  /* 2026-10-31 8:15 PM (JD): Thresholdor mask is authored in top-texture space (box x40..87 y80..107 = LED box), so it turns with the top exactly (facing + 0) */  // 2026-10-31 5:20 PM (JD): Thresholdor digit mask turned a further 180 deg so the value sits in the LED box; was +90 deg CCW (one extra quarter turn of the mask UV only; base art untouched)
  } else if (r == SimRole::Latchor) {
    // 2026-10-31 (JD): configured value 1..64 in the right eye square (seven-segment, purple), same
    // rotation as the base top so it reads upright facing north. Only the vertex value changes.
    const int v = stValue(cell.state) > 0 && stValue(cell.state) <= kMaxSignal ? stValue(cell.state) : kMaxSignal;
    const FaceMaterial& topM = cell.materials.face(cell.id, FaceSlot::Top, on);
    out.face(cell, 2, tlo, thi, topM, {kShadeDigits, uint8_t(v | (on ? 128 : 0))}, f);
  }
  return true;
}
namespace {
// Cortana 2026-10-16 [GAME] (JD): Aluminum Ramp mesh. Planar faces only: the base square, up to
// four side faces (rectangles or triangles) and the slope (one quad when straight, two triangles
// for inner / outer corners). 5 to 7 quads per ramp; full faces against opaque neighbours are
// culled. Uses MeshWriter::quad (free-corner quads; triangles repeat a corner).
void meshRamp(const CustomBlock& cell, MeshWriter& out) {
  namespace rp = litholattice::procgen::ramp;
  const uint32_t st = uint32_t(cell.state) & 0x1fu;
  const bool hang = rp::hangingOf(st);
  const FaceMaterial fm = cell.materials.face(cell.id, FaceSlot::Top, false);
  const cBlockReg& reg = cell.blocks;
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
  const cBlockReg& reg = cell.blocks;
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


// 2026-10-25 (JD, Posts; art by Cloud): Standing and Wall Posts. Every piece is an oriented box in an
// integer (R, F) frame: F = the facing direction, R = the viewer's right when looking along F (the
// text reads left-to-right along R on the -F face, the side the placer stood on). Straight facings
// use unit vectors; the 45-degree facings use (+-1, +-1) lattice steps, so every corner stays on the
// 1/16 vertex grid (PackedVertex precision) with no rounding. Text: one 1/16 glyph cell per char
// from the generated "post_font" atlas (16 x 16 ASCII grid, panel colours baked in).
namespace {
struct PostFrame { int c[2]; int R[2]; int F[2]; };

PostFrame postFrame(int octant) {
  PostFrame f{{8, 8}, {0, 0}, {0, 0}};
  facing8Dir(octant, f.F[0], f.F[1]);
  f.R[0] = -f.F[1]; f.R[1] = f.F[0];  // clockwise quarter turn from above
  return f;
}

int postLightFace(int nx, int ny, int nz) {
  if (ny) return ny > 0 ? 2 : 3;
  if (std::abs(nx) >= std::abs(nz)) return nx > 0 ? 0 : 1;
  return nz > 0 ? 4 : 5;
}

// p wound so its normal points along nrm (same fix-up as the ramps), then emitted.
void postQuad(const CustomBlock& cell, MeshWriter& out, int p[4][3], int uv[4][2], const int nrm[3],
              const FaceMaterial& fm, FaceLook look = {}) {
  int e1[3], e2[3];
  for (int a = 0; a < 3; ++a) { e1[a] = p[1][a] - p[0][a]; e2[a] = p[2][a] - p[0][a]; }
  const int n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
  if (n[0] * nrm[0] + n[1] * nrm[1] + n[2] * nrm[2] < 0) {
    for (int a = 0; a < 3; ++a) std::swap(p[1][a], p[3][a]);
    std::swap(uv[1][0], uv[3][0]); std::swap(uv[1][1], uv[3][1]);
  }
  for (int i = 0; i < 4; ++i) { p[i][0] = std::clamp(p[i][0], 0, 16); p[i][2] = std::clamp(p[i][2], 0, 16); }
  out.quad(cell, p, uv, postLightFace(nrm[0], nrm[1], nrm[2]), fm, look);
}

// One face of the box a in [a0,a1] (R steps) x b in [b0,b1] (F steps) x y in [y0,y1]: the quad
// spanning (s0..s1) along the face's horizontal axis and (t0..t1) in y, at the fixed coordinate.
void postSide(const CustomBlock& cell, MeshWriter& out, const PostFrame& f, bool alongR, int fixed, int s0, int s1,
              int y0, int y1, const int nrm[3], const FaceMaterial& fm, const int uvr[4], FaceLook look = {}) {
  auto P = [&](int s, int y, int (&q)[3]) {
    const int a = alongR ? s : fixed, b = alongR ? fixed : s;
    q[0] = f.c[0] + a * f.R[0] + b * f.F[0]; q[1] = y; q[2] = f.c[1] + a * f.R[1] + b * f.F[1];
  };
  int p[4][3], uv[4][2] = {{uvr[0], uvr[3]}, {uvr[2], uvr[3]}, {uvr[2], uvr[1]}, {uvr[0], uvr[1]}};
  P(s0, y0, p[0]); P(s1, y0, p[1]); P(s1, y1, p[2]); P(s0, y1, p[3]);
  postQuad(cell, out, p, uv, nrm, fm, look);
}

// A whole oriented box. `textFace` true: the -F face is left to the caller (the glyph pass).
void postBox(const CustomBlock& cell, MeshWriter& out, const PostFrame& f, int a0, int a1, int b0, int b1, int y0,
             int y1, const FaceMaterial& side, const FaceMaterial& face, bool textFace) {
  const int full[4] = {0, 0, 16, 16};
  const int nF[3] = {f.F[0], 0, f.F[1]}, nB[3] = {-f.F[0], 0, -f.F[1]};
  const int nR[3] = {f.R[0], 0, f.R[1]}, nL[3] = {-f.R[0], 0, -f.R[1]};
  postSide(cell, out, f, true, b1, a1, a0, y0, y1, nF, face, full);  // back (away from the viewer)
  if (!textFace) postSide(cell, out, f, true, b0, a0, a1, y0, y1, nB, face, full);
  postSide(cell, out, f, false, a1, b0, b1, y0, y1, nR, side, full);
  postSide(cell, out, f, false, a0, b1, b0, y0, y1, nL, side, full);
  for (int top = 0; top < 2; ++top) {  // caps
    const int y = top ? y1 : y0;
    auto P = [&](int a, int b, int (&q)[3]) {
      q[0] = f.c[0] + a * f.R[0] + b * f.F[0]; q[1] = y; q[2] = f.c[1] + a * f.R[1] + b * f.F[1];
    };
    int p[4][3], uv[4][2] = {{0, 0}, {16, 0}, {16, 16}, {0, 16}};
    P(a0, b0, p[0]); P(a1, b0, p[1]); P(a1, b1, p[2]); P(a0, b1, p[3]);
    const int n[3] = {0, top ? 1 : -1, 0};
    postQuad(cell, out, p, uv, n, side);
  }
}

// The text face of a panel box: kRows rows of glyph cells across [a0, a1] on b = b0 (facing -F).
void postText(const CustomBlock& cell, MeshWriter& out, const PostFrame& f, int a0, int a1, int b0,
              const FaceMaterial& font, const std::string* text) {
  const int cols = a1 - a0;
  const std::vector<std::string> lines = post::layout(text ? *text : std::string{}, cols);
  const int nB[3] = {-f.F[0], 0, -f.F[1]};
  for (int r = 0; r < post::kRows; ++r) {
    const std::string& line = r < int(lines.size()) ? lines[size_t(r)] : std::string{};
    const int y1 = 16 - r, y0 = y1 - 1;
    const int n = std::min<int>(int(line.size()), cols);
    for (int i = 0; i < n; ++i) {
      const int ch = uint8_t(line[size_t(i)]) & 127;
      const int uvr[4] = {ch & 15, ch >> 4, (ch & 15) + 1, (ch >> 4) + 1};
      postSide(cell, out, f, true, b0, a0 + i, a0 + i + 1, y0, y1, nB, font, uvr);
    }
    if (n < cols) {  // the rest of the row: one stretched blank (space) cell
      const int uvr[4] = {0, 2, 1, 3};
      postSide(cell, out, f, true, b0, a0 + n, a1, y0, y1, nB, font, uvr);
    }
  }
}

void meshPost(const CustomBlock& cell, MeshWriter& out) {
  const FaceMaterial wood = cell.materials.face(cell.id, FaceSlot::Left, false);
  const FaceMaterial* panelM = cell.materials.named("post_panel");
  const FaceMaterial panel = panelM ? *panelM : wood;
  const FaceMaterial* font = cell.materials.named("post_font");
  const std::string* text = cell.in.textAt(cell.x, cell.y, cell.z);
  if (post::isWall(cell.id)) {
    const PostFrame f = postFrame((stFacing(cell.state) & 3) * 2);
    postBox(cell, out, f, -8, 8, 7, 8, 0, 16, wood, panel, font != nullptr);
    if (font) postText(cell, out, f, -8, 8, 7, *font, text);
    return;
  }
  const int oct = stFacing8(cell.state) & 7;
  const PostFrame f = postFrame(oct);
  if (!post::isUpper(cell.state)) {  // the stand: a foot and a stick
    postBox(cell, out, f, -3, 3, -3, 3, 0, 1, wood, wood, false);
    postBox(cell, out, f, -1, 1, -1, 1, 1, 16, wood, wood, false);
    return;
  }
  const int hw = (oct & 1) ? 5 : 8;  // diagonal: 10 lattice steps (14.1/16 wide); straight: 16
  postBox(cell, out, f, -hw, hw, -1, 1, 0, 16, wood, panel, font != nullptr);
  if (font) postText(cell, out, f, -hw, hw, -1, *font, text);
}
}  // namespace
// Wire: one flat piece on the cell floor, chosen from its connections and tinted by strength.
void cLithoKraftAppearance::meshCustom(const CustomBlock& cell, MeshWriter& out) const {
  const cBlockReg& reg = cell.blocks;
  const BlockDef def = cell.def();
  if (cell.id == kAluminumRamp) { meshRamp(cell, out); return; }  // Cortana 2026-10-16
  if (cell.id == kHighwayRamp) { meshHighwayRamp(cell, out); return; }  // Proposed by JD, Built by Cortana
  if (post::isPost(cell.id)) { meshPost(cell, out); return; }  // 2026-10-25 Posts
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
    // PlayM: settings 0..5 = 2..64 ticks, CloudAtlas six-button tops (2026-11-01).
    static constexpr const char* kTops[repeator::kSettings] = {"repeator_top_d2_base", "repeator_top_d4_base", "repeator_top_d8_base", "repeator_top_d16_base", "repeator_top_d32_base", "repeator_top_d64_base"};
    static constexpr int lo[3] = {0, 0, 0}, hi[3] = {16, 3, 16};
    // 2026-10-31 (JD): the value-readout base is the d2 art turned 180 deg, so it draws with no extra turns
    // (art features land where they did); the delay (4/8/16/32) shows in its bottom-right window.
    // PlayM (JD 11:12): the CloudAtlas art reads backward vs the signal direction; the VISUAL (top texture,
    // digits and 3D model) is turned 180 deg (logic unchanged). kRepeatorTurn = 2 quarter turns.
    constexpr int kRepeatorTurn = 2;
    const int di = repeator::delayIndex(cell.state);
    const FaceMaterial* topM = cell.materials.named(kTops[di]);
    const foliage::Mesh& rm = foliage::repeatorMesh(di);
    if (rm.fromAsset && !rm.quads.empty()) {  // PlayM: the six-button .lthm model for this setting (assets/models/repeator)
      const int facing = (stFacing(cell.state) + kRepeatorTurn) & 3;
      for (const foliage::Quad& q : rm.quads) {
        const foliage::Quad rq = foliage::rotated(q, facing);
        const FaceMaterial& fm = (q.face == 2 && topM) ? *topM : cell.materials.face(cell.id, localSlot(q.face), stOn(cell.state));
        const int l[3] = {rq.lo[0], rq.lo[1], rq.lo[2]}, h[3] = {rq.hi[0], rq.hi[1], rq.hi[2]};
        out.face(cell, rq.face, l, h, fm, machineTintLook(cell, fm, stOn(cell.state)), (q.face == 2 || q.face == 3) ? facing : 0);
      }
      g_repeatorModelQuads.fetch_add(rm.quads.size(), std::memory_order_relaxed);
      static std::atomic<bool> logged{false};
      if (!logged.exchange(true)) ::litholattice::logInfo("repeator: six-button .lthm model in the render path (setting %d/6, %zu quads, 180-deg visual turn)", di + 1, rm.quads.size());
    } else {
      machineBox(cell, out, lo, hi, stOn(cell.state), topM, kRepeatorTurn);
    }
    // PlayM (JD 12:59, fix 1): the digits overlay sits one texel (1/16) above the model top so it never Z-fights.
    static constexpr int dlo[3] = {0, 3, 0}, dhi[3] = {16, 4, 16};
    if (topM)
      out.face(cell, 2, dlo, dhi, *topM, {kShadeDigitsRep, uint8_t(repeator::delayTicks(cell.state) | 128)}, (stFacing(cell.state) + kRepeatorTurn + 2) & 3);  // PlayN (JD 3:28 PM): digit tint overlay only, turned another 180 deg
    if (rm.fromAsset && !rm.quads.empty()) return;
    // 2026-10-24 9:30 PM (JD clock clip): four 1/16 raised delay buttons over the art's pips (local
    // x is mirrored by the 2 top turns); the first delayIndex+1 are pressed (flush, sunk art shows).
    for (int i = 0; i < repeator::kSettings; ++i) {  // PlayM fallback (no .lthm): six 1/16 buttons (x 13-2i)
      if (i <= di) continue;  // PlayM (CloudAtlas six-button art, JD-approved): setting s presses buttons 0..s (count = s + 1)
      const int bl[3] = {13 - 2 * i, 3, 7}, bh[3] = {14 - 2 * i, 4, 9};
      machineBox(cell, out, bl, bh, stOn(cell.state), topM, kRepeatorTurn);
    }
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
  int arms = 0, count = 0, portArms = 0;
  for (int d = 0; d < 4; ++d) {
    const BlockDef nd = reg.def(cell.neighbor(kDirX[d], 0, kDirZ[d]));
    const bool comp = roleOf(nd) != SimRole::None && roleOf(nd) != SimRole::Wire && roleOf(nd) != SimRole::Torch;
    // 2026-10-24 (JD): an arm runs into a component only on a face the sim really reads or drives.
    bool port = comp && tracePortFaces(nd, cell.neighborState(kDirX[d], 0, kDirZ[d]), dirBack(d));
    // 2026-11-01 (JD screenshot): Trace meeting a lamp / colour lamp / LED gets the same raised
    // connector stub (and arm) as at a switch, on every side. Render only. LL_TRACE_LAMP_ARM=0: old.
    static const bool kLampArm = [] { const char* e = std::getenv("LL_TRACE_LAMP_ARM"); return !(e && e[0] == '0'); }();
    if (kLampArm && !port) {
      const SimRole nr = roleOf(nd);
      port = nr == SimRole::Lamp || nr == SimRole::ColorLamp || nr == SimRole::Torch;
    }
    if (port) portArms |= 1 << d;
    bool link = (roleOf(nd) == SimRole::Wire && nd.channel == def.channel) || port;
    const BlockDef up = reg.def(cell.neighbor(kDirX[d], 1, kDirZ[d]));
    const BlockDef dn = reg.def(cell.neighbor(kDirX[d], -1, kDirZ[d]));
    if (!upBlocked && roleOf(up) == SimRole::Wire && up.channel == def.channel) link = true;
    if (!(nd.flags & kCutsWire) && roleOf(dn) == SimRole::Wire && dn.channel == def.channel) link = true;
    // PlayN (JD 13:07): a custom connection state with a side on toward a solid powerable block (a soft-power
    // target) draws the same arm + connector riser as at a component port. Auto keeps the old look. Visual only.
    if (!link && !cTraceConn::isAuto(cell.state) && cTraceConn::sideOn(cell.state, kDirX[d], 0, kDirZ[d]) &&
        reg.hasAll(cell.neighbor(kDirX[d], 0, kDirZ[d]), kSolid | kOpaque | kPowerable)) {
      link = port = true; portArms |= 1 << d;
    }
    // 2026-11-01 PlayL: a custom connection state draws arms only on its connected sides (both ends for Trace).
    if (link && !cTraceConn::sideOn(cell.state, kDirX[d], 0, kDirZ[d])) link = false, port = false, portArms &= ~(1 << d);
    if (link && roleOf(nd) == SimRole::Wire && !cTraceConn::sideOn(cell.neighborState(kDirX[d], 0, kDirZ[d]), -kDirX[d], 0, -kDirZ[d])) link = false;
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
  // 2026-10-24 CloudAtlas connector arms (placeholder, visual only): at each real component port
  // the strip climbs into the component: riser [6,0,0]-[10,3,1] at that edge (north-local, turned
  // to the port side), meeting the 2/16 slab pads at y 2..3 (full blocks: it just meets the face).
  if (portArms && !hanging)
    if (const FaceMaterial* am = cell.materials.named(ch ? "trace_arm_blue" : "trace_arm_red"))
      for (int d = 0; d < 4; ++d) {
        if (!(portArms >> d & 1)) continue;
        for (int f = 0; f < 6; ++f) {
          if (f == 3) continue;  // the underside sits on the floor
          foliage::Quad q{};
          q.lo[0] = 6; q.lo[1] = 0; q.lo[2] = 0; q.hi[0] = 10; q.hi[1] = 3; q.hi[2] = 1;
          q.face = uint8_t(f);
          const foliage::Quad r = foliage::rotated(q, d);
          const int l3[3] = {r.lo[0], r.lo[1], r.lo[2]}, h3[3] = {r.hi[0], r.hi[1], r.hi[2]};
          out.face(cell, r.face, l3, h3, *am, {}, 0);
        }
      }
}

void cLithoKraftAppearance::bindMaterials(const BlockDef& def, const BlockModel& model, MaterialBindings& b) const {
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
  "materials": [{"name": "gem", "slot": "top"}, {"name": "shaft", "slot": "front"}, {"name": "chip", "slot": "bottom"}],
  "meshes": [{"name": "led", "boxes": [{"min": [5, 0, 5], "max": [11, 1, 11], "material": 2},
    {"min": [7, 1, 7], "max": [9, 3, 9], "material": 1},
    {"min": [6, 3, 6], "max": [10, 4, 10], "material": 0},
    {"min": [5, 4, 5], "max": [11, 6, 11], "material": 0},
    {"min": [4, 4, 6], "max": [12, 6, 10], "material": 0},
    {"min": [6, 4, 4], "max": [10, 6, 12], "material": 0},
    {"min": [6, 6, 6], "max": [10, 7, 10], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";
// 2026-10-24 7:52 PM CloudAtlas engine-fit LED (floor + wall, gem=top chip=bottom shaft=side textures); was LED v2b: wall LED authored with the support at
// z=0 (north); the stack above the socket ledge steps +Z one per one up (~45 deg, away from the
// wall). No element rotation; the mesher turns it by yaw only.
const char* const kTorchWallModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "gem", "slot": "top"}, {"name": "shaft", "slot": "front"}, {"name": "chip", "slot": "bottom"}],
  "meshes": [{"name": "led", "boxes": [{"min": [5, 2, 0], "max": [11, 8, 1], "material": 2},
    {"min": [7, 5, 1], "max": [9, 7, 3], "material": 1},
    {"min": [6, 6, 3], "max": [10, 10, 4], "material": 0},
    {"min": [5, 6, 4], "max": [11, 12, 6], "material": 0},
    {"min": [4, 7, 4], "max": [12, 11, 6], "material": 0},
    {"min": [6, 5, 4], "max": [10, 13, 6], "material": 0},
    {"min": [6, 9, 6], "max": [10, 13, 7], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";
// 2026-10-25 Switch v2 (CloudAtlas, JD approved): lever throws along X, off leans east, on west, pivot (8,8).
// (superseded) 2026-10-24 (CloudAtlas lever, facing north): base [4,0,5]-[12,2,11]; 2x2 handle pivoting at
// y=1 on the centre line, tilted 45 degrees, drawn as a 3-step staircase (box models have no
// element rotation). Off leans back (+Z, toward the player who placed it), on leans forward (-Z).
const char* const kSwitchModel = R"({"asset":{"version":"ll-box-1"},"materials":[{"name":"base","slot":"bottom"},{"name":"shaft","slot":"front"},{"name":"tip_red","slot":"left"},{"name":"tip_blue","slot":"right"}],"meshes":[{"name":"switch","boxes":[{"min":[4,0,5],"max":[12,2,11],"material":0},{"min":[7,2,7],"max":[9,3,9],"material":1},{"min":[8,3,7],"max":[10,4,9],"material":1},{"min":[9,4,7],"max":[11,5,9],"material":1},{"min":[10,5,7],"max":[12,6,9],"material":1},{"min":[11,6,7],"max":[13,7,9],"material":1},{"min":[11,7,6],"max":[15,9,8],"material":2},{"min":[11,7,8],"max":[15,9,10],"material":3}]}],"nodes":[{"mesh":0}]})";
const char* const kSwitchOnModel = R"({"asset":{"version":"ll-box-1"},"materials":[{"name":"base","slot":"bottom"},{"name":"shaft","slot":"front"},{"name":"tip_red","slot":"left"},{"name":"tip_blue","slot":"right"}],"meshes":[{"name":"switch","boxes":[{"min":[4,0,5],"max":[12,2,11],"material":0},{"min":[7,2,7],"max":[9,3,9],"material":1},{"min":[6,3,7],"max":[8,4,9],"material":1},{"min":[5,4,7],"max":[7,5,9],"material":1},{"min":[4,5,7],"max":[6,6,9],"material":1},{"min":[3,6,7],"max":[5,7,9],"material":1},{"min":[1,7,6],"max":[5,9,8],"material":2},{"min":[1,7,8],"max":[5,9,10],"material":3}]}],"nodes":[{"mesh":0}]})";

const char* const kButtonModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "cap", "slot": "top"}],
  "meshes": [{"name": "cap", "boxes": [{"min": [5, 0, 6], "max": [11, 2, 10], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";

const char* const kButtonPressedModel = R"({"asset": {"version": "ll-box-1"},
  "materials": [{"name": "cap", "slot": "top"}],
  "meshes": [{"name": "cap", "boxes": [{"min": [5, 0, 6], "max": [11, 1, 10], "material": 0}]}],
  "nodes": [{"mesh": 0}]})";
const BoxModel* cLithoKraftAppearance::boxModel(const BlockDef& def) const {
  static const std::optional<BoxModel> torch = BoxModel::parse(kTorchModel, nullptr);
  static const std::optional<BoxModel> sw = BoxModel::parse(kSwitchModel, nullptr);
  if (roleOf(def) == SimRole::Torch) return torch ? &*torch : nullptr;
  static const std::optional<BoxModel> button = BoxModel::parse(kButtonModel, nullptr);
  if (roleOf(def) == SimRole::Switch) return sw ? &*sw : nullptr;
  if (roleOf(def) == SimRole::cButton) return button ? &*button : nullptr;
  return nullptr;
}

const BoxModel* cLithoKraftAppearance::boxModel(const BlockDef& def, CellState st) const {
  if (roleOf(def) == SimRole::cButton && stOn(st)) {
    // 2026-10-24 pressed state (render only): the cap sinks to 1/16; the top slot's active
    // material is button_top_pressed in every pack.
    static const std::optional<BoxModel> pressed = BoxModel::parse(kButtonPressedModel, nullptr);
    return pressed ? &*pressed : nullptr;
  }
  if (roleOf(def) == SimRole::Switch && stOn(st)) {
    static const std::optional<BoxModel> on = BoxModel::parse(kSwitchOnModel, nullptr);
    return on ? &*on : nullptr;
  }
  if (roleOf(def) == SimRole::Torch && stAttach(st) >= 3 && stAttach(st) <= 6) {
    static const std::optional<BoxModel> wall = BoxModel::parse(kTorchWallModel, nullptr);
    return wall ? &*wall : nullptr;
  }
  return boxModel(def);
}

int pickShapeBoxes(const litholattice::ChunkMap& w, const litholattice::cBlockReg& reg, int x, int y, int z,
                   litholattice::BlockId b, litholattice::PickBox* out, int maxBoxes) {
  const BlockDef& def = reg.def(b);
  const CellState st = w.getState(x, y, z);
  const SimRole r = roleOf(def);
  const cLithoKraftAppearance& look = static_cast<const cLithoKraftAppearance&>(blockAppearance());
  int n = 0;
  // A north-frame box [lo, hi) in 1/16 units, turned like machineBox (foliage::rotated by facing).
  auto add = [&](int x0, int y0, int z0, int x1, int y1, int z1, int turns) {
    if (n >= maxBoxes) return;
    foliage::Quad q{};
    q.lo[0] = int16_t(x0); q.lo[1] = int16_t(y0); q.lo[2] = int16_t(z0);
    q.hi[0] = int16_t(x1); q.hi[1] = int16_t(y1); q.hi[2] = int16_t(z1);
    q.face = 2;
    const foliage::Quad t = foliage::rotated(q, turns);
    for (int a = 0; a < 3; ++a) {
      const int lo = std::min(int(t.lo[a]), int(t.hi[a])), hi = std::max(int(t.lo[a]), int(t.hi[a]));
      out[n].lo[a] = float(lo) / 16.f; out[n].hi[a] = float(hi) / 16.f;
    }
    ++n;
  };
  if (def.flags & kAttachable) {  // LEDs, levers, buttons: the mesher's oriented box model
    const BoxModel* bm = look.boxModel(def, st);
    if (!bm) return -1;
    const int af = stAttach(st) ? stAttach(st) : 1;
    if (def.shape == Shape::Cross && af >= 3 && af <= 6) return -1;  // sheared wall sprite: whole cell
    const int turns = look.boxModelTurns(def, st);  // PlayN: per mount, see boxModelTurns
    for (const BoxModel::Box& box : bm->orientedBoxes(af, turns)) add(box.min[0], box.min[1], box.min[2], box.max[0], box.max[1], box.max[2], 0);
    return n;
  }
  const int f = stFacing(st);
  if (look.shapedRender(def)) {  // mirrors meshShaped: 2/16 slab + 1/16 pads
    const bool on = look.active(def, st);
    add(0, 0, 0, 16, 2, 16, f);
    add(6, 2, 0, 10, (r == SimRole::Thresholdor && on) ? 4 : 3, 2, f);  // output pad
    if (r == SimRole::Keyboard) { add(14, 2, 6, 16, 3, 10, f); return n; }
    if (r == SimRole::Memor) { add(4, 2, 4, 12, 3, 12, f); return n; }
    add(6, 2, 14, 10, 3, 16, f);  // input pad
    if (r == SimRole::Redirector || r == SimRole::Thresholdor) { add(0, 2, 6, 2, 3, 10, f); add(14, 2, 6, 16, 3, 10, f); }
    if (r == SimRole::Thresholdor && (stAux(st) & 1)) add(4, 2, 7, 12, 3, 9, f);
    if (r == SimRole::Pulsor) add(6, 2, 6, 10, 3, 10, f);
    return n;
  }
  if (r == SimRole::Repeator) {
    add(0, 0, 0, 16, 3, 16, f);
    const int di = repeator::delayIndex(st);
    for (int i = 0; i < repeator::kSettings; ++i) if (i > di) add(2 + 2 * i, 3, 7, 3 + 2 * i, 4, 9, f);  /* PlayM: 180-deg visual turn (JD 11:12) */  // PlayM: raised = buttons above the setting
    return n;
  }
  if (b == kLithographor) { add(0, 0, 0, 16, 11, 16, 0); add(4, 11, 4, 12, 15, 12, 0); add(5, 15, 5, 11, 16, 11, 0); return n; }
  if (def.shape == Shape::Slab) { add(0, 0, 0, 16, 8, 16, 0); return n; }
  return -1;
}

int cLithoKraftAppearance::boxModelTurns(const BlockDef& def, CellState st) const {
  const SimRole r = roleOf(def);
  const int af = stAttach(st) ? stAttach(st) : 1;  // PlayN (JD): wall Switches turn with the placer's look; others only on floor / ceiling (or tilted sprites)
  if (r == SimRole::Switch) return stFacing(st);
  if (af >= 3 && !(r == SimRole::Torch && def.shape == Shape::Cross)) return 0;
  if (r == SimRole::Torch) {  // 2026-10-24 LED v2 wall: support -Z/+X/+Z/-X -> yaw 0/1/2/3
    switch (stAttach(st)) { case 3: return 0; case 6: return 1; case 4: return 2; case 5: return 3; default: return 0; }
  }
  return (r == SimRole::Switch || r == SimRole::cButton) ? stFacing(st) : 0;  // 2026-10-24: floor / ceiling facing
}

}  // namespace lithokraft
