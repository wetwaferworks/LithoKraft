#include "lithokraft/content/signal_world.h"
#include "lithokraft/content/post.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/item_use.h"
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215
#include "litholattice/core/log.h"
#include <chrono>
#include <cstdlib>
#include "litholattice/procgen/ramp_shape.h"  // Cortana 2026-10-16: Aluminum Ramp snapping

namespace lithokraft {

namespace {
// 2026-10-24 (JD): "Any block that's classified as a partial block is going to need a support block
// unless it's partial and solid." Partial = Shape other than Cube, from the registry. Partial and
// non-solid blocks already have their own rules (flat wires: wireSupported; LEDs, switches, plants:
// kAttachable / canAttach), so the data-driven rule only covers ones that have neither. The Memor
// is a full solid cube by its data, so JD's explicit ruling puts it on a MARKED per-block list.
bool needsFloorSupport(BlockId b) {
  const BlockDef& d = blockRegistry().def(b);
  if (b == kAir || isFlatWire(b) || (d.flags & kAttachable)) return false;
  if (d.shape != Shape::Cube && !(d.flags & kSolid)) return true;  // partial, non-solid
  // MARKED per-block list (JD 2026-10-24): Memor needs a solid block beneath; the Keyboard is a
  // non-solid cube in data but renders as a 2/16 slab, so it is partial and needs one too.
  return b == kMemor || b == kKeyboard || b == kLatchor;  // Fast Latch: slab placeholder, same rule (JD 2026-10-24)
}
bool floorSupported(const ChunkMap& w, int x, int y, int z) { return blockRegistry().has(w.get(x, y - 1, z), kSolid); }
}  // namespace

// 2026-10-25 (JD): a Via placed against a SIDE face lies on its side, open ends pointing at the
// clicked block (X or Z); against a top/bottom face it stands vertical as before.
BlockId cSignalWorldSim::placeVariant(const ChunkMap&, BlockId block, int hdx, int hdy, int hdz) const {
  if (viaAxis(block) != 0 || hdy != 0) return block;
  if (hdx) return viaSide(block, 1);
  if (hdz) return viaSide(block, 2);
  return block;
}

bool cSignalWorldSim::canPlace(const ChunkMap& w, int x, int y, int z, BlockId block) const {
  if (isFlatWire(block)) return wireSupported(w, x, y, z);
  if (needsFloorSupport(block)) return floorSupported(w, x, y, z);  // 2026-10-24: Memor floor-only
  // Proposed by JD, Built by Cortana: the Highway Ramp needs room for its second half (any side).
  // 2026-10-25 (JD, Posts): a Standing Post needs the cell above for its panel; a Wall Post needs a
  // solid block beside it to hang on (onBlockChanged turns it to face one).
  if (post::isStanding(block)) return y + 1 < kWorldHeight && w.get(x, y + 1, z) == kAir;
  if (post::isWall(block)) {
    const auto& reg = blockRegistry();
    return reg.has(w.get(x, y, z - 1), kSolid) || reg.has(w.get(x + 1, y, z), kSolid) ||
           reg.has(w.get(x, y, z + 1), kSolid) || reg.has(w.get(x - 1, y, z), kSolid);
  }
  if (block == kHighwayRamp)
    return w.get(x + 1, y, z) == kAir || w.get(x - 1, y, z) == kAir || w.get(x, y, z + 1) == kAir || w.get(x, y, z - 1) == kAir;
  return true;  // torches and switches: canAttach decides (they mount on the clicked face)
}

bool cSignalWorldSim::stillSupported(const ChunkMap& w, int x, int y, int z) const {
  const BlockId b = w.get(x, y, z);
  if (needsFloorSupport(b)) return floorSupported(w, x, y, z);  // 2026-10-24: pops (drops) like a Trace
  return !isFlatWire(b) || wireSupported(w, x, y, z);
}

bool cSignalWorldSim::canAttach(const ChunkMap& w, int x, int y, int z, BlockId block, int face) const {
  int dx, dy, dz;
  attachOffset(face, dx, dy, dz);
  if (!blockRegistry().has(w.get(x + dx, y + dy, z + dz), kSolid)) return false;  // needs a solid support
  // 2026-10-16 (JD): LEDs (torch role) mount on top, sides AND the bottom face (hanging upside-down).
  return true;  // switch: top, sides or bottom
}

void cSignalWorldSim::applyTortureGate(ChunkMap& w, const DirtyFn& dirty) {
  auto leverOn = [&](int x, int y, int z) { return w.get(x, y, z) == kSwitch && stOn(w.getState(x, y, z)); };
  const bool on = leverOn(gateX_, gateY_, gateZ_);
  // 2026-10-24 9:15 PM (JD): per-scene levers. want[n] = master || scene lever n; want[0] = master.
  std::vector<uint8_t> want(levers_.size() + 1, uint8_t(on));
  for (size_t i = 0; i < levers_.size(); ++i) want[i + 1] = uint8_t(on || leverOn(levers_[i].x, levers_[i].y, levers_[i].z));
  auto groupOf = [&](const GatedSource& g) { return (g.group > 0 && size_t(g.group) < want.size()) ? size_t(g.group) : size_t(0); };
  if (!gateSynced_) {
    // 2026-10-24 (JD 6:06 PM): the save restores its columns AFTER the test world is built, so a
    // playtest save from before the lever brings the old LEDs / Repeators back and they ran with
    // the lever off. First tick: with the lever off, lift every held source the save put back.
    gateSynced_ = true;
    {
      int lifted = 0;
      for (const GatedSource& g : gated_) {
        if (want[groupOf(g)]) continue;
        if (w.get(g.x, g.y, g.z) != g.id || !w.set(g.x, g.y, g.z, kAir)) continue;
        w.setState(g.x, g.y, g.z, CellState(0));
        onBlockChanged(w, g.x, g.y, g.z, kAir);
        dirty(g.x, g.y, g.z);
        ++lifted;
      }
      if (lifted) litholattice::logInfo("torture lever: off at load, lifted %d sources restored by the save", lifted);
    }
  }
  if (want == groupLit_) return;
  const std::vector<uint8_t> was = groupLit_.empty() ? std::vector<uint8_t>(want.size(), 0) : groupLit_;
  {  // 2026-10-25 (JD hitch report): log every lever edge; group n = the scene named in the load-time "lever ... group n" line
    std::string ch;
    for (size_t i = 0; i < want.size(); ++i)
      if (want[i] != was[i]) { ch += ch.empty() ? "" : ", "; ch += (i == 0 ? std::string("master") : "group " + std::to_string(i)) + (want[i] ? " ON" : " OFF"); }
    litholattice::logInfo("torture lever: %s (master lever %s)", ch.c_str(), on ? "on" : "off");
  }
  groupLit_ = want;
  gateLit_ = false;
  for (uint8_t v : want) gateLit_ = gateLit_ || v;  // Pattern ROM screens run while any lever is on
  for (const GatedSource& g : gated_) {
    const size_t gi = groupOf(g);
    const bool on = want[gi] != 0;
    if (on == (was[gi] != 0)) continue;
    if (w.get(g.x, g.y, g.z) != (on ? kAir : g.id)) continue;  // the player changed the cell: leave it
    const BlockId b = on ? g.id : kAir;
    if (!w.set(g.x, g.y, g.z, b)) continue;
    w.setState(g.x, g.y, g.z, on ? g.st : CellState(0));
    onBlockChanged(w, g.x, g.y, g.z, b);
    dirty(g.x, g.y, g.z);
  }
}

void cSignalWorldSim::onBlockChanged(ChunkMap& w, int x, int y, int z, BlockId now) {
  // Cortana 2026-10-16 (JD): Aluminum Ramp placement. Hanging when placed under an overhang (air
  // below, solid above); facing from the placement (kDirectional, standard facing bits) is
  // re-packed into the ramp's 5-bit state, then Minecraft-stair corner snapping runs for the new
  // ramp and re-runs for straight neighbours (corners keep their shape: their stored facing is
  // canonical, see ramp_shape.h).
  if (now == kAluminumRamp || now == kAir) {
    namespace rp = litholattice::procgen::ramp;
    const auto& reg = blockRegistry();
    auto at = [&](int ax, int az, bool hang) {
      return [&w, ax, y, az, hang](int dx, int dz) {
        if (w.get(ax + dx, y, az + dz) != kAluminumRamp) return -1;
        const uint32_t s = uint32_t(w.getState(ax + dx, y, az + dz)) & 0x1fu;
        return rp::hangingOf(s) == hang ? rp::facingOf(s) : -1;
      };
    };
    if (now == kAluminumRamp) {
      const bool hang = !reg.has(w.get(x, y - 1, z), kSolid) && reg.has(w.get(x, y + 1, z), kSolid);
      const int f = stFacing(w.getState(x, y, z));
      w.setState(x, y, z, CellState(rp::snap(f, hang, at(x, z, hang))));
    }
    static constexpr int n4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    for (const auto& d : n4) {
      const int nx = x + d[0], nz = z + d[1];
      if (w.get(nx, y, nz) != kAluminumRamp) continue;
      const uint32_t s = uint32_t(w.getState(nx, y, nz)) & 0x1fu;
      if (rp::shapeOf(s) != rp::kStraight) continue;
      const uint8_t ns = rp::snap(rp::facingOf(s), rp::hangingOf(s), at(nx, nz, rp::hangingOf(s)));
      if (ns != s) w.setState(nx, y, nz, CellState(ns));
    }
  }
  // Proposed by JD, Built by Cortana (2026-10-16): Highway Ramp pairing. Placing one puts the low
  // half in the clicked cell and the high half one cell toward the facing (or, if that is taken,
  // the clicked cell becomes the high half and the low half goes behind it; failing both, the pair
  // turns to the first free side). Breaking either half removes its partner. Never snaps corners.
  {
    namespace hw = litholattice::procgen::ramp::highway;
    if (now == kHighwayRamp) {
      const int f0 = stFacing(w.getState(x, y, z)) & 3;
      bool placed = false;
      for (int k = 0; k < 4 && !placed; ++k) {
        const int f = (f0 + (k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 3 : 2)) & 3;
        const int dx = hw::kDx[f], dz = hw::kDz[f];
        if (w.get(x + dx, y, z + dz) == kAir && w.set(x + dx, y, z + dz, kHighwayRamp)) {
          w.setState(x, y, z, CellState(hw::encode(f, 0)));
          w.setState(x + dx, y, z + dz, CellState(hw::encode(f, 1)));
          placed = true;
        } else if (k == 0 && w.get(x - dx, y, z - dz) == kAir && w.set(x - dx, y, z - dz, kHighwayRamp)) {
          w.setState(x, y, z, CellState(hw::encode(f, 1)));
          w.setState(x - dx, y, z - dz, CellState(hw::encode(f, 0)));
          placed = true;
        }
      }
      if (!placed) w.setState(x, y, z, CellState(hw::encode(f0, 0)));  // canPlace normally prevents this
    } else if (now == kAir) {
      static constexpr int h4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
      for (const auto& d : h4) {
        const int nx = x + d[0], nz = z + d[1];
        if (w.get(nx, y, nz) != kHighwayRamp) continue;
        const uint32_t s = uint32_t(w.getState(nx, y, nz)) & 0x7u;
        if (nx + hw::partnerDx(s) == x && nz + hw::partnerDz(s) == z) w.set(nx, y, nz, kAir);
      }
    }
  }
  // 2026-10-25 (JD, Posts): pairs like the Highway Ramp. Placing a Standing Post (its lower half, in
  // the clicked cell) puts the panel half above with the same facing; breaking either half removes
  // the other (the panel's text goes with it). A Wall Post turns to the nearest solid wall.
  if (post::isStanding(now)) {
    const CellState s = w.getState(x, y, z);
    if (!post::isUpper(s) && w.get(x, y + 1, z) == kAir && w.set(x, y + 1, z, now))
      w.setState(x, y + 1, z, CellState(s | post::kUpperBit));
  } else if (post::isWall(now)) {
    static constexpr int f4[4][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 0}};
    const CellState s = w.getState(x, y, z);
    const int f0 = stFacing(s);
    for (int k = 0; k < 4; ++k) {
      const int f = (f0 + (k == 0 ? 0 : k == 1 ? 1 : k == 2 ? 3 : 2)) & 3;
      if (blockRegistry().has(w.get(x + f4[f][0], y, z + f4[f][1]), kSolid)) {
        if (f != f0) w.setState(x, y, z, makeState(0, f));
        break;
      }
    }
  } else if (now == kAir) {
    if (post::isStanding(w.get(x, y + 1, z)) && post::isUpper(w.getState(x, y + 1, z))) w.set(x, y + 1, z, kAir);
    if (post::isStanding(w.get(x, y - 1, z)) && !post::isUpper(w.getState(x, y - 1, z))) w.set(x, y - 1, z, kAir);
  }
  signal_.onBlockChanged(x, y, z, now, roleOf(w.get(x, y - 1, z)) == SimRole::Wire);  // LL_EDIT_LOCAL hint
  sounds_.push_back({now == kAir ? SoundCue::Break : SoundCue::Place, 0, x, y, z});
}

bool cSignalWorldSim::use(ChunkMap& w, int x, int y, int z, const DirtyFn& dirty) {
  if (!signal_.use(w, x, y, z, dirty)) return false;
  sounds_.push_back({SoundCue::Click, 0, x, y, z});
  return true;
}

// ===== PlayM4 (JD t4701u): Trace-in-time in game (numpad 7 compile / 8 stop / 9 speed) =====
void cSignalWorldSim::lithoStop(int why) {
  signal_.lithoCommit(); signal_.lithoReset(); lithoOn_ = false;
  auto& h = lithoHud(); h.state = why; h.seq.fetch_add(1);
}
bool cSignalWorldSim::lithoTick(ChunkMap& w, const DirtyFn& dirty) {
  auto& h = lithoHud();
  if (!signal_.lithoValid()) { lithoStop(2); ::litholattice::logInfo("litho: chip dropped (block edit) - live sim resumes"); return false; }
  const auto t0 = std::chrono::steady_clock::now();
  const int r = signal_.lithoRun(lithoSteps_);
  if (r < 0) { lithoStop(2); return false; }
  h.runUs = float(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count());
  h.ticks.fetch_add(uint64_t(r));
  signal_.lithoWriteBack(w, dirty);
  return true;
}

void cSignalWorldSim::command(ChunkMap& w, const SimCommand& c, const DirtyFn& dirty) {
  if (c.code == kCmdLitho) {
    auto& h = lithoHud();
    if (c.a == 1) {
      if (lithoOn_) lithoStop(4);
      static const int kR = [] { const char* e = std::getenv("LL_LITHO_R"); const int v = e ? std::atoi(e) : 96; return v < 4 ? 4 : v; }();
      static const int kH = [] { const char* e = std::getenv("LL_LITHO_H"); const int v = e ? std::atoi(e) : 32; return v < 2 ? 2 : v; }();
      cSignalSim::LithoStats st;
      const bool ok = signal_.lithoCompile(w, c.x - kR, c.y - kH, c.z - kR, c.x + kR, c.y + kH, c.z + kR, &st);
      h.nodes = uint32_t(st.nodes); h.links = uint32_t(st.links); h.wires = uint32_t(st.wires); h.compileMs = float(st.compileMs);
      h.ticks = 0; h.runUs = 0.f; h.steps = uint32_t(lithoSteps_);
      if (ok && st.nodes > 0) { lithoOn_ = true; h.state = 1; }
      else { signal_.lithoReset(); h.state = 3; }
      h.seq.fetch_add(1);
      ::litholattice::logInfo("litho: compile box (%d,%d,%d)+-%d/%d -> %s nodes=%zu links=%zu wires=%zu %.2f ms", c.x, c.y, c.z, kR, kH,
                              lithoOn_ ? "chip ON" : "empty", st.nodes, st.links, st.wires, st.compileMs);
    } else if (c.a == 2) {
      if (lithoOn_) { lithoStop(4); ::litholattice::logInfo("litho: chip stopped by key - live sim resumes"); }
    } else if (c.a == 3) {
      static constexpr int kSteps[] = {1, 2, 4, 8, 16, 64};
      int i = 0; while (i < 6 && kSteps[i] != lithoSteps_) ++i;
      lithoSteps_ = kSteps[(i + 1) % 6]; h.steps = uint32_t(lithoSteps_); h.seq.fetch_add(1);
      ::litholattice::logInfo("litho: chip speed x%d ticks per world tick", lithoSteps_);
    }
    return;
  }
  if (c.code == kCmdKeyPulse) signal_.keyPulse(c.x, c.y, c.z, c.a, c.b);
  else if (c.code == kCmdAltUse) {
    // 2026-10-31 (JD, Alt+wheel): `a` = how many secondary steps (0 = 1). The 1..64 settings wrap
    // through 64 states, so 63 steps is one step back (Alt + wheel down).
    const int n = (c.a >= 1 && c.a <= 63) ? c.a : 1;
    // PlayB (JD t4347u): the Routor's modes are Alt + click only; Alt + wheel does nothing on it.
    if (c.a != 0 && w.get(c.x, c.y, c.z) == kRedirectorSelect) return;
    bool any = false;
    if (c.a != 0 && roleOf(w.get(c.x, c.y, c.z)) == SimRole::Dialor) {
      // B9 (JD 2026-11-11 A9 playtest): Alt + wheel sets the DIALOR dial, the same direction as
      // every other 1..64 setting (wheel up = +1 = a 1, wheel down = -1 = a 63). The Dialor's own
      // steps are use() = +1 and useAlt() = -1 (settled rulings, unit-tested), so exactly one
      // step each way here; 1..64 wraps inside those. Alt + click (a 0) keeps one useAlt() step.
      any = signal_.use(w, c.x, c.y, c.z, dirty, /*alt=*/c.a >= 32);
    } else {
      for (int i = 0; i < n; ++i) any = signal_.use(w, c.x, c.y, c.z, dirty, true) || any;
    }
    if (any) sounds_.push_back({SoundCue::Click, 0, c.x, c.y, c.z});
  }
  // Cortana 2026-10-15 2215 [GAME]: Repeator top-button select (JD D4) and walk-bridging (JD D9).
  else if (c.code == kCmdRepeatorSet) {
    if (blockRegistry().def(w.get(c.x, c.y, c.z)).role == uint16_t(SimRole::Repeator)) {
      const CellState s = w.getState(c.x, c.y, c.z);
      w.setState(c.x, c.y, c.z, repeator::withColour(repeator::encode(stValue(s), stFacing(s), (int(c.a) >= repeator::kSettings ? repeator::kSettings - 1 : int(c.a) < 0 ? 0 : int(c.a)), stOn(s)), repeator::colourOf(s)));
      dirty(c.x, c.y, c.z);
      sounds_.push_back({SoundCue::Click, 0, c.x, c.y, c.z});
    }
  } else if (c.code == kCmdBridgePlace) {
    const BlockId b = BlockId(c.a);
    const auto& reg = blockRegistry();
    static constexpr int n6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    bool support = false;
    for (const auto& d : n6) support = support || reg.has(w.get(c.x + d[0], c.y + d[1], c.z + d[2]), kSolid);
    if (b != kAir && b < BlockId(reg.size()) && !reg.has(b, kAttachable) && w.get(c.x, c.y, c.z) == kAir && support &&
        canPlace(w, c.x, c.y, c.z, b) && w.set(c.x, c.y, c.z, b)) {
      if (reg.has(b, kDirectional)) w.setState(c.x, c.y, c.z, makeState(0, c.b & 3));
      onBlockChanged(w, c.x, c.y, c.z, b);
      dirty(c.x, c.y, c.z);
      sounds_.push_back({SoundCue::Place, 0, c.x, c.y, c.z});
    }
  }
  // 2026-10-25 (JD, Posts): one text edit on a post (either half; the panel cell holds the text).
  else if (c.code == kCmdPostEdit) {
    const BlockId b = w.get(c.x, c.y, c.z);
    if (post::isPost(b)) {
      const int py = post::panelY(b, w.getState(c.x, c.y, c.z), c.y);
      if (post::isPost(w.get(c.x, py, c.z))) {
        const std::string* cur = w.cellText(c.x, py, c.z);
        std::string t = cur ? *cur : std::string{};
        if (post::applyEdit(t, c.a, c.b)) { w.setCellText(c.x, py, c.z, std::move(t)); dirty(c.x, py, c.z); }
      }
    }
  }
  // B1a/A3: the general per-item use seam. Dispatch fails closed (logged)
  // for items with no defined handler; a handled use clicks like block Use.
  else if (c.code == kCmdItemUse &&
           dispatchItemUse(uint32_t(c.a), ItemUseContext{w, c.x, c.y, c.z, dirty}) == ItemUseResult::Handled)
    sounds_.push_back({SoundCue::Click, 0, c.x, c.y, c.z});
}

void cSignalWorldSim::onBlockPopped(ChunkMap& /*w*/, int x, int y, int z, BlockId was) {
  // Popped blocks become item drops (Minecraft-style): the game thread spawns the entity.
  if (was == kSilicaGrass) {
    // Silica grass drops seeds, not the grass block itself (pop policy: 1 seed).
    for (int i = 0; i < silicaBreakDrops(x, y, z); ++i)
      dropEvents_.push_back(SimEvent{kEvItemDrop, int(kItemSilicaSeed), x, y, z});
    return;
  }
  dropEvents_.push_back(SimEvent{kEvItemDrop, int(viaBase(was)), x, y, z});  // side vias drop the base via
}

void cSignalWorldSim::onRandomTick(ChunkMap& w, int x, int y, int z, BlockId block, uint32_t rnd,
                                         const DirtyFn& dirty) {
  if (block != kSilicaGrass) return;
  // No ILighting is plumbed into the sim path yet: pass a null query and the
  // silica logic falls back to the open-sky check (surfaceY). Uproot seed drops
  // join the normal drop-event channel.
  silicaOnRandomTick(w, blockRegistry(), x, y, z, rnd, dirty, {},
                     [&](uint32_t itemId, uint16_t count, int dx, int dy, int dz) {
                       for (uint16_t i = 0; i < count; ++i)
                         dropEvents_.push_back(SimEvent{kEvItemDrop, int(itemId), dx, dy, dz});
                     });
}

void cSignalWorldSim::takeEvents(std::vector<SimEvent>& out) {
  out.insert(out.end(), dropEvents_.begin(), dropEvents_.end());
  dropEvents_.clear();
  signal_.takeSounds(sounds_);
  for (const SoundEvent& s : sounds_) out.push_back({uint16_t(s.cue), s.value, s.x, s.y, s.z});
  sounds_.clear();
}


// ---- Timing Handshake integration (hs-design §3.4/§7.2) ----
// Registration order: flip+torches, components, mark, prep (all affine,
// CycleHead at their phase offsets), then W net shards and W lamp shards
// (pull workers). All Signal work is once per world tick: one cycle of P
// phases executes inside this tick() call frame (D2 corollary), so
// cTimedSim / cSimThread tick timings measure the same thing in both
// modes. Pull workers NEVER receive the real DirtyFn: net floods do not
// dirty at all, and lamp batches write into per-shard collectors that the
// sim thread replays through the real dirty fn at the phase commit, in
// canonical (shard, then ascending key) order (hs-design §3.3).

void cSignalWorldSim::registerHandshake(litholattice::cTimingHandshakeAuthority& authority) {
  hsAuthority_ = &authority;
  const int p = authority.phasesPerCycle();
  const int w = authority.workerCount();
  hsLampDirty_.assign(size_t(w), {});
  hsLampHolds_.assign(size_t(w), {});
  hsNames_.clear();
  auto name = [this](std::string n) -> const char* {
    hsNames_.push_back(std::move(n));
    return hsNames_.back().c_str();
  };
  using litholattice::CadenceClass;
  auto reg = [&](const char* n, int offset, bool affine, litholattice::cTimingHandshakeAuthority::WorkFn fn) {
    litholattice::HsComponentDesc d;
    d.name = n;
    d.cadence = CadenceClass::CycleHead;
    d.phaseDivider = uint32_t(p);
    d.phaseOffset = uint32_t(offset);
    d.threadAffine = affine;
    d.tickCritical = true;
    d.group = 1;  // signal group; cSimThread's clock domains use group 2
    authority.registerComponent(d, std::move(fn));
  };
  authority.setCommitFn([this](uint64_t phase, uint64_t cycle, uint32_t group) { onHsCommit(phase, cycle, group); });
  reg(name("signal.flip_torches"), 0, true, [this](uint64_t, uint64_t) { signal_.hsFlipAndTorches(*hsWorld_, hsDirty_); });
  reg(name("signal.components"), 1, true, [this](uint64_t, uint64_t) { signal_.hsComponents(*hsWorld_, hsDirty_); });
  reg(name("signal.mark"), 2, true, [this](uint64_t, uint64_t) { signal_.hsMark(*hsWorld_); });
  reg(name("signal.prep"), 2, true, [this](uint64_t, uint64_t) {
    if (!signal_.hsCycleSkipped() && !signal_.hsFullPass()) signal_.hsPrepIncremental(*hsWorld_);
  });
  for (int i = 0; i < w; ++i) {
    reg(name("signal.net.shard" + std::to_string(i)), 2, false,
        [this, i, w](uint64_t, uint64_t) {
          if (signal_.hsCycleSkipped() || signal_.hsFullPass()) return;
          const std::vector<int32_t>& nets = signal_.hsMarkedNets();
          const DirtyFn none = [](int, int, int) {};  // per-net floods never dirty (strength buffer only)
          for (size_t idx = size_t(i); idx < nets.size(); idx += size_t(w))
            signal_.hsPropagateNet(*hsWorld_, nets[idx], none);
        });
  }
  for (int i = 0; i < w; ++i) {
    reg(name("signal.lamps.shard" + std::to_string(i)), 3, false,
        [this, i, w](uint64_t, uint64_t) {
          if (signal_.hsCycleSkipped()) return;
          const std::vector<int64_t>& keys = signal_.hsLampKeys();
          const size_t n = keys.size(), base = w ? n / size_t(w) : 0, rem = w ? n % size_t(w) : 0;
          const size_t start = size_t(i) * base + std::min(size_t(i), rem);
          const size_t len = base + (size_t(i) < rem ? 1 : 0);
          std::vector<int64_t> batch(keys.begin() + start, keys.begin() + start + len);
          auto& collected = hsLampDirty_[size_t(i)];
          collected.clear();
          const DirtyFn collector = [&collected](int x, int y, int z) { collected.push_back(HsDirtyPos{x, y, z}); };
          signal_.hsLampsBatch(batch, *hsWorld_, collector);
          hsLampHolds_[size_t(i)] = cSignalSim::hsTlsLampHolds();  // drain this thread's hold updates
          cSignalSim::hsTlsLampHolds().clear();
        });
  }
}

void cSignalWorldSim::onHsCommit(uint64_t phase, uint64_t /*cycleTick*/, uint32_t group) {
  if (group != 1 || !hsAuthority_) return;
  const int p = hsAuthority_->phasesPerCycle();
  const int sub = p > 0 ? int(phase % uint64_t(p)) : 0;
  if (sub == 2) {
    // Propagate barrier passed: replay the deferred strength writes in
    // canonical order on this (sim) thread.
    if (!signal_.hsCycleSkipped() && !signal_.hsFullPass()) signal_.hsApplyStrengthPending(&hsDirty_);
    else if (signal_.hsFullPass()) signal_.hsPropagateFull(*hsWorld_, hsDirty_);
    if (!signal_.hsCycleSkipped()) signal_.hsRefreshSoft(*hsWorld_);  // 2026-11-01 PlayL: same-tick soft, before the lamp phase
    if (!signal_.hsCycleSkipped()) signal_.hsSettle(*hsWorld_, hsDirty_);  // 2026-11-01 PlayM: same-tick components (serial, sim thread)
    signal_.hsFinalizeComponents();  // PlayM: deferred component side effects, canonical key order
  } else if (sub == 3) {
    // Lamp barrier passed: replay collected dirties through the real dirty
    // fn (canonical: shard order, ascending keys inside each batch), merge
    // the deferred lamp holds, finish the cycle, then flip (commit).
    for (auto& shardDirty : hsLampDirty_) {
      for (const auto& [x, y, z] : shardDirty) hsDirty_(x, y, z);
      shardDirty.clear();
    }
    for (auto& holds : hsLampHolds_) {
      signal_.hsApplyLampHolds(holds);
      holds.clear();
    }
    if (!signal_.hsCycleSkipped()) signal_.hsFinishCycle(*hsWorld_);
    signal_.hsCommit();
  }
}

void cSignalWorldSim::tickHandshake(ChunkMap& world, const TickArea& area, const DirtyFn& dirty) {
  signal_.setRoundArea(area.round);  // F56-7
  signal_.setDiamondArea(area.diamond);  // diamond (JD 2026-10-23)
  const int p = hsAuthority_->phasesPerCycle();
  if (p < 4) {  // the four sub-steps need four phase slots; fall back to the legacy cycle
    if (!hsLegacyFallbackLogged_) {
      hsLegacyFallbackLogged_ = true;
      litholattice::logWarn("timing-handshake: phases/cycle %d < 4 — Signal cycle runs legacy inside handshake mode", p);
    }
    signal_.tick(world, area.pcx, area.pcz, area.radius, dirty);
    return;
  }
  hsWorld_ = &world;
  hsArea_ = area;
  hsDirty_ = dirty;
  hsFrozenCycle_ = false;
  signal_.applyPlacementBinds(world, dirty);  // 2026-11-07 (JD): placement binds land before the cycle's phases, never on power transitions
  signal_.hsBeginCycle(world, area.pcx, area.pcz, area.radius);
  const uint64_t cycle = signal_.tickCount();
  const uint64_t base = (cycle - 1) * uint64_t(p);
  for (int i = 0; i < p; ++i) {
    if (!hsAuthority_->runPhase(base + uint64_t(i), cycle, 1)) {
      // Watchdog freeze mid-cycle: nothing past the last barrier committed.
      // Force a full pass on recovery so the next cycle rebuilds cleanly.
      hsFrozenCycle_ = true;
      signal_.invalidateNetwork();
      hsWorld_ = nullptr;
      return;
    }
  }
  hsWorld_ = nullptr;
}

}  // namespace lithokraft
