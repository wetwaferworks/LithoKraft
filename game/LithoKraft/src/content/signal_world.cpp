#include "lithokraft/content/signal_world.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/item_use.h"
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215
#include "litholattice/core/log.h"
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
  return b == kMemor || b == kKeyboard;
}
bool floorSupported(const ChunkMap& w, int x, int y, int z) { return blockRegistry().has(w.get(x, y - 1, z), kSolid); }
}  // namespace

bool cSignalWorldSim::canPlace(const ChunkMap& w, int x, int y, int z, BlockId block) const {
  if (isFlatWire(block)) return wireSupported(w, x, y, z);
  if (needsFloorSupport(block)) return floorSupported(w, x, y, z);  // 2026-10-24: Memor floor-only
  // Proposed by JD, Built by Cortana: the Highway Ramp needs room for its second half (any side).
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
  const bool on = w.get(gateX_, gateY_, gateZ_) == kSwitch && stOn(w.getState(gateX_, gateY_, gateZ_));
  if (on == gateLit_) return;
  gateLit_ = on;
  for (const GatedSource& g : gated_) {
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
  signal_.onBlockChanged(x, y, z, now);
  sounds_.push_back({now == kAir ? SoundCue::Break : SoundCue::Place, 0, x, y, z});
}

bool cSignalWorldSim::use(ChunkMap& w, int x, int y, int z, const DirtyFn& dirty) {
  if (!signal_.use(w, x, y, z, dirty)) return false;
  sounds_.push_back({SoundCue::Click, 0, x, y, z});
  return true;
}

void cSignalWorldSim::command(ChunkMap& w, const SimCommand& c, const DirtyFn& dirty) {
  if (c.code == kCmdKeyPulse) signal_.keyPulse(c.x, c.y, c.z, c.a, c.b);
  else if (c.code == kCmdAltUse && signal_.use(w, c.x, c.y, c.z, dirty, true)) sounds_.push_back({SoundCue::Click, 0, c.x, c.y, c.z});
  // Cortana 2026-10-15 2215 [GAME]: Repeator top-button select (JD D4) and walk-bridging (JD D9).
  else if (c.code == kCmdRepeatorSet) {
    if (blockRegistry().def(w.get(c.x, c.y, c.z)).role == uint16_t(SimRole::Repeator)) {
      const CellState s = w.getState(c.x, c.y, c.z);
      w.setState(c.x, c.y, c.z, repeator::encode(stValue(s), stFacing(s), c.a & 3, stOn(s)));
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
  dropEvents_.push_back(SimEvent{kEvItemDrop, int(was), x, y, z});
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
    if (!signal_.hsCycleSkipped() && !signal_.hsFullPass()) signal_.hsApplyStrengthPending();
    else if (signal_.hsFullPass()) signal_.hsPropagateFull(*hsWorld_, hsDirty_);
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
