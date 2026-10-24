// LithoKraft's world simulation for the engine's cSimThread: the signal sim plus the game's
// placement rules and sound cues (place / break / click), behind litholattice::IWorldSim.
#pragma once
#include "litholattice/core/tick_profile.h"  // LL_TICK_PROFILE (measure only; Proposed by JD; co-implemented by JD and Cortana)
#include <memory>
#include <string>
#include <vector>
#include "lithokraft/content/pattern_rom.h"
#include "lithokraft/content/signal.h"
#include "lithokraft/content/silica.h"
#include "litholattice/sim/timing_handshake.h"
#include "litholattice/sim/world_simulation.h"

namespace lithokraft {

/// SimCommand codes LithoKraft sends through cSimThread::edit (EditCmd::Kind::Command).
enum SimCommandCode : uint16_t {
  kCmdAltUse = 2,    ///< sneak + use on the component at (x, y, z)
  kCmdKeyPulse = 1,  ///< keyboard block at (x, y, z): a = front (SIXBIT code + 1), b = control code
  kCmdItemUse = 3,   ///< per-item use dispatch (brief B1a / A3): a = itemId, (x, y, z) = target cell;
                     ///< routed to content/item_use.h's registry on the sim thread
  // Cortana 2026-10-15 2215 [GAME]:
  kCmdRepeatorSet = 4,   ///< Repeator at (x, y, z): set delay index a (0..3 = 2/4/8/16), top-button click
  kCmdBridgePlace = 5,   ///< bridge placement: put block a (facing b) into the AIR cell (x, y, z) when a
                         ///< face neighbour is solid and canPlace allows it (Bedrock walk-bridging)
};

/// SimEvent kinds above the SoundCue range (SoundCue is uint8_t).
constexpr uint16_t kEvItemDrop = 0x0100;  ///< value = BlockId dropped; the game spawns an item entity

class cSignalWorldSim final : public IWorldSim {
 public:
  void scan(const ChunkMap& world) override { signal_.scan(world); }
  bool canPlace(const ChunkMap& world, int x, int y, int z, BlockId block) const override;
  bool canAttach(const ChunkMap& world, int x, int y, int z, BlockId block, int face) const override;
  bool stillSupported(const ChunkMap& world, int x, int y, int z) const override;
  std::string inspect(const ChunkMap& w, int x, int y, int z) const override { return signal_.inspect(w, x, y, z); }
  void onBlockChanged(ChunkMap& world, int x, int y, int z, BlockId now) override;
  void onBlockPopped(ChunkMap& world, int x, int y, int z, BlockId was) override;
  /// Random-tick dispatch: silica grass growth/spread (workstream 5). The engine
  /// only calls this for kRandomTicks cells (8/section/tick); the silica logic
  /// itself never runs on the per-tick path.
  void onRandomTick(ChunkMap& world, int x, int y, int z, BlockId block, uint32_t rnd,
                    const DirtyFn& dirty) override;
  bool use(ChunkMap& world, int x, int y, int z, const DirtyFn& dirty) override;
  void command(ChunkMap& world, const SimCommand& cmd, const DirtyFn& dirty) override;
  void registerHandshake(litholattice::cTimingHandshakeAuthority& authority) override;
  void tick(ChunkMap& world, const TickArea& area, const DirtyFn& dirty) override {
    signal_.setRoundArea(area.round);  // F56-7
    const int64_t tpS = ::litholattice::tickprof::enabled() ? ::litholattice::tickprof::nowNs() : 0;  // LL_TICK_PROFILE (measure only; Proposed by JD; co-implemented by JD and Cortana)
    if (!hsAuthority_) signal_.tick(world, area.pcx, area.pcz, area.radius, dirty);
    else tickHandshake(world, area, dirty);
    const int64_t tpR = tpS ? ::litholattice::tickprof::nowNs() : 0;
    // Playtest 2026-10-16: Pattern ROM screens advance once per world tick (empty in every
    // hash / selftest world, so those ticks are unchanged).
    if (gateSet_) applyTortureGate(world, dirty);  // 2026-10-24 (JD): playtest torture lever
    if (!gateSet_ || gateLit_) for (PatternRomScreen& s : romScreens_) tickPatternRom(world, s, dirty);
    if (tpS) { auto& w = ::litholattice::tickprof::worldSub(); w.signalNs = tpR - tpS; w.romNs = ::litholattice::tickprof::nowNs() - tpR; }
  }
  /// Add a Pattern ROM screen (before the cSimThread starts; the sim thread owns it afterwards).
  void addPatternRom(PatternRomScreen s) { romScreens_.push_back(std::move(s)); }
  /// 2026-10-24 (JD): playtest torture lever. While the Switch at (x, y, z) is off, the listed
  /// LED sources are removed (air) and the Pattern ROM screens stop; switching it on puts the
  /// LEDs back (and off removes them again). Unset in every bench / hash / selftest world.
  struct GatedSource { int x, y, z; BlockId id; CellState st; };
  void setTortureGate(int x, int y, int z, std::vector<GatedSource> src) {
    gateSet_ = true; gateX_ = x; gateY_ = y; gateZ_ = z; gated_ = std::move(src);
  }
  /// True when the last handshake tick was frozen by the watchdog mid-cycle.
  bool handshakeFrozenCycle() const { return hsFrozenCycle_; }
  size_t activeCount() const override { return signal_.componentCount(); }
  void takeEvents(std::vector<SimEvent>& out) override;

 cSignalSim& signal() { return signal_; }  ///< only while the cSimThread is stopped (save / load)

 private:
  void tickHandshake(ChunkMap& world, const TickArea& area, const DirtyFn& dirty);
  void applyTortureGate(ChunkMap& world, const DirtyFn& dirty);
  bool gateSet_ = false, gateLit_ = false;
  int gateX_ = 0, gateY_ = 0, gateZ_ = 0;
  std::vector<GatedSource> gated_;
  void onHsCommit(uint64_t phase, uint64_t cycleTick, uint32_t group);
  cSignalSim signal_;
  // ---- Timing Handshake state (null authority = legacy path) ----
  litholattice::cTimingHandshakeAuthority* hsAuthority_ = nullptr;
  ChunkMap* hsWorld_ = nullptr;   // valid only inside tickHandshake
  TickArea hsArea_{};
  DirtyFn hsDirty_;               // the real dirty fn of the in-flight tick
  bool hsFrozenCycle_ = false;
  bool hsLegacyFallbackLogged_ = false;
  std::vector<std::string> hsNames_;                    // component names (desc.name points here)
  struct HsDirtyPos { int x, y, z; };
  std::vector<std::vector<HsDirtyPos>> hsLampDirty_;   // per-shard collected dirties
  std::vector<std::vector<cSignalSim::LampHoldUpdate>> hsLampHolds_;  // per-shard deferred hold merges
  std::vector<SoundEvent> sounds_;  // edit cues, in order; tick sounds are appended at takeEvents()
  std::vector<PatternRomScreen> romScreens_;  // playtest 2026-10-16: sim-tick screen animations
  std::vector<SimEvent> dropEvents_;  // item drops from popped blocks (drained at takeEvents())
};

/// A SimEvent from LithoKraft's simulation as the sound it stands for.
inline SoundEvent toSoundEvent(const SimEvent& e) { return {SoundCue(e.kind), e.value, e.x, e.y, e.z}; }

}  // namespace lithokraft
