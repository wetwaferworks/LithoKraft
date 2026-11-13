// Signal simulation (sim thread only). Per world tick:
//  1) torches update from the power state of the block they sit on (one-tick inverter delay),
//  2) components (repeator, thresholder, redirector, pulsor, memory cell, fast latch, switch,
//     button, keyboard) compute their outputs from the previous tick's inputs (1-tick delay),
//  3) wire strengths are recomputed by a bucket BFS from every source (torches into their own
//     colour, components and torch-powered blocks into either colour; -1 per block; Pulsor
//     pulses travel the connected run without decay, up to 64 blocks),
//  4) lamps light / color lamps take the strongest adjacent signal. Lamps terminate signals.
// Only component cells are visited; plain terrain costs nothing. Roles come from the cBlockReg;
// component logic lives in one cCompBehavior class per block type (litholattice/sim/component.h).
#pragma once
#include <atomic>
#include <map>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <bit>
#include <vector>
#include <memory>
#include "litholattice/render/trace_strength_buffer.h"
#include "lithokraft/engine.h"
#include "lithokraft/content/component.h"
#include "lithokraft/content/scheduled_output.h"  // 2026-11-04 (JD t4304u)
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

constexpr int kButtonTicks = 10;    // button pulse length
constexpr int kPulsorAnimTicks = 7; // firing animation flag hold (renderer plays the sidecar strip)

// DEC SIXBIT-style 64-char set: code = ASCII - 32 for ' '..'_' (lowercase folds to uppercase).
// Returns -1 for characters outside the set. Keyboard pulses strength code + 1.
int sixbitCode(char32_t c);
char sixbitChar(int code);
// Color lamp palette index for strength s on an N-colour tier (WRAP mapping; -1 = off).
constexpr int colorLampIndex(int s, int n) { return s <= 0 ? -1 : (s - 1) % n; }
// Keyboard control-port codes (right side).
constexpr int kKeyEnter = 1, kKeyBackspace = 2;

/// A sound the sim wants played: cue, strength (Tone pitch / Pulsor data) and the cell position.
struct SoundEvent {
  SoundCue cue;
  int value;
  int x, y, z;
};

/// One wireless broadcast: a transmittor's output on a channel.
/// Link budget: a receivor hears it when dist <= power + receivor sensitivity.
struct RadioBroadcast {
  int x = 0, y = 0, z = 0;
  uint8_t dim = 1;       ///< dimension index (1 = Wafer/overworld)
  uint8_t channel = 1;   ///< 1..64 (0 = the Daemon, reserved)
  uint8_t strength = 0;  ///< 0..64
  uint8_t colour = 0;    ///< 1 red, 2 blue
  uint16_t power = 0;    ///< broadcast power in blocks
};

/// The wireless signal registry (game-side, sim thread only).
/// Transmittors publish during the component tick; receivors read the previous tick's
/// broadcasts (1-tick latency). Only loaded chunks are indexed: unload = silent.
class cRadioReg {
 public:
  void publish(RadioBroadcast b) { next_.push_back(b); }
  /// End-of-tick: this tick's broadcasts become readable next tick.
  void flip() {
    cur_.swap(next_);
    next_.clear();
    // Rebuild the per-channel index.
    for (auto& v : byChannel_) v.clear();
    for (const RadioBroadcast& b : cur_)
      if (b.channel < 65) byChannel_[b.channel].push_back(&b);
  }
  void clear() { cur_.clear(); next_.clear(); for (auto& v : byChannel_) v.clear(); }
  /// Strongest broadcast on `channel` within (power + sensitivity) of (x, y, z).
  /// Returns false when nothing is heard. Tie-breaks are canonical: strongest
  /// wins, then nearest, then the lowest packPos ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â so the result never depends on
  /// broadcast registration order.
  bool query(uint8_t channel, int x, int y, int z, int dim, int sensitivity,
             int& strength, int& colour) const {
    if (channel >= 65) return false;
    bool heard = false;
    int best = -1;
    int64_t bestD2 = 0;
    int64_t bestPos = 0;
    for (const RadioBroadcast* bp : byChannel_[channel]) {
      const RadioBroadcast& b = *bp;
      if (b.strength == 0) continue;
      const int64_t dx = int64_t(b.x) - x, dy = int64_t(b.y) - y, dz = int64_t(b.z) - z;
      // Cross-dimensional: the signal attenuates (half range). The Daemon (ch 0) hears all.
      int64_t reach = int64_t(b.power) + sensitivity;
      if (channel != 0 && b.dim != dim) reach /= 2;
      const int64_t d2 = dx * dx + dy * dy + dz * dz;
      if (d2 > reach * reach) continue;
      const int64_t pos = packPos(b.x, b.y, b.z);
      if (!heard || b.strength > best ||
          (b.strength == best && (d2 < bestD2 || (d2 == bestD2 && pos < bestPos)))) {
        heard = true; best = b.strength; bestD2 = d2; bestPos = pos;
        strength = b.strength; colour = b.colour;
      }
    }
    return heard;
  }

 private:
  std::vector<RadioBroadcast> cur_, next_;
  std::array<std::vector<const RadioBroadcast*>, 65> byChannel_;  ///< channel -> broadcasts
};

/// A jammer zone: blocks all radio channels except its own within `radius`.
struct JammerZone {
  int x = 0, y = 0, z = 0;
  int radius = 0;
  uint8_t channel = 1;  ///< the one channel allowed through
};

/// Active jammers, same one-tick-latency discipline as cRadioReg.
class cJammerReg {
 public:
  void publish(JammerZone j) { next_.push_back(j); }
  void flip() { cur_.swap(next_); next_.clear(); }
  void clear() { cur_.clear(); next_.clear(); }
  /// True when (x, y, z) is inside a jammer whose allowed channel isn't `channel`.
  bool jammed(int x, int y, int z, uint8_t channel) const {
    for (const JammerZone& j : cur_) {
      if (j.channel == channel) continue;  // the allowed channel passes through
      const int64_t dx = int64_t(j.x) - x, dy = int64_t(j.y) - y, dz = int64_t(j.z) - z;
      if (dx * dx + dy * dy + dz * dz <= int64_t(j.radius) * j.radius) return true;
    }
    return false;
  }

 private:
  std::vector<JammerZone> cur_, next_;
};

class cSignalSim {
 public:
  using DirtyFn = std::function<void(int x, int y, int z)>;  // a cell's look changed

  void scan(const ChunkMap& w);                       // index all components (after worldgen)
  /// Index ONE column's cells (SW-H1): the streaming path inserts disk-loaded
  /// columns after the one full scan() at dimension load, so each inserted
  /// column is scanned on insertion (sim stopped), then its SGNL payload is
  /// importState()d. Existing indexes for other columns are untouched. The
  /// column must be newly inserted (no stale entries can exist for it).
  void scanColumn(const ChunkMap& w, int cx, int cz);
  /// Column eviction (A2 / B-M1): drops every indexed cell of ONE column
  /// (torches / wires / lamps / components, plus lamp holds and wake
  /// entries), zeroes the removed wires' trace-strength slots, and forces
  /// a network rebuild (fullPass) so no derived net/pointer state can
  /// reference the column again. The caller MUST take the column's SGNL
  /// export first (exportColumnState() / exportState()) and save it
  /// before unscanning when the state must survive ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â after this call the
  /// column's component memory is gone from the sim. Sim stopped only,
  /// like scanColumn().
  void unscanColumn(int cx, int cz);
  /// This column's slice of exportState() (empty when it holds no
  /// components): the SGNL bytes a save-on-evict must persist BEFORE
  /// unscanColumn() drops them.
  std::vector<uint8_t> exportColumnState(int cx, int cz) const;
  /// True when the sim indexes any cell in the column (torch / wire /
  /// lamp / component). Eviction treats such a column as save-first even
  /// when its ChunkColumn::dirty flag is clear (sim fast paths write cell
  /// states through cached pointers, bypassing ChunkMap::setState).
  bool hasIndexedCells(int cx, int cz) const;
  /// Number of indexed cells in the column (tests / diagnostics).
  size_t indexedCellCount(int cx, int cz) const;
  void onBlockChanged(int x, int y, int z, BlockId now);
  // PlayM2 lithography spike (Trace-in-time, JD 2026-11-06): compile a settled region into a tickable
  // IR (component nodes + precomputed wire-distance links); place/break (editGen_) invalidates it.
  struct LithoStats { size_t nodes = 0, links = 0, wires = 0, compLinks = 0; double compileMs = 0; };
  bool lithoCompile(const ChunkMap& w, int x0, int y0, int z0, int x1, int y1, int z1, LithoStats* st = nullptr);
  int lithoRun(int ticks);           ///< ticks run; -1 = invalidated by an edit since compile (IR dropped)
  bool lithoValid() const;
  void lithoReset();
  bool lithoActive() const { return litho_ != nullptr; }
  /// PlayM4: copy each chip node's cell state into the world (dirty only on change) so the chip is visible.
  void lithoWriteBack(ChunkMap& w, const DirtyFn& dirty);
  /// PlayM4: hand the chip's component memory + tick back to the live sim (then lithoReset) before live resumes.
  void lithoCommit();
  void lithoStates(std::vector<std::pair<int64_t, uint32_t>>& out) const;  ///< (cell, state) per node, key order
  uint64_t editGen() const { return editGen_; }
  int debugInputFrom(const ChunkMap& w, int x, int y, int z, int d) const { return inputFrom(w, x, y, z, d); }
  const ComponentMemory* debugMem(int x, int y, int z) const { auto it = comps_.find(packPos(x, y, z)); return it == comps_.end() ? nullptr : &it->second.mem; }
  // 2026-11-01 (JD placement spikes): wireBelow = a wire sits at (x,y-1,z). Lets a non-wire edit keep the
  // flood topology (only that wire's links can see this cell's climb-cut property). LL_EDIT_LOCAL=0 kill.
  void onBlockChanged(int x, int y, int z, BlockId now, bool wireBelow);
  // cPlayer chunk + radius: components outside the radius are frozen.
  void tick(ChunkMap& w, int pcx, int pcz, int radius, const DirtyFn& dirty);
  /// 2026-11-07 (JD ruling): placement binding for Catchor / SignalTransmittor.
  /// A component registered since the last cycle (placement, edit, or scan) that
  /// is still unwired (aux mode 3) binds ONCE here, at the head of the cycle, to
  /// the first Trace touching a side. Power/unpower transitions NEVER bind or
  /// rebind: the tick-time auto-bind is retired, so a binding can only ever be
  /// created by a placement/edit, and afterwards only player use modifies it.
  void applyPlacementBinds(ChunkMap& w, const DirtyFn& dirty);

  // ---- Timing Handshake surface (hs-design Ãƒâ€šÃ‚Â§3.2) ----
  // tick() is reassembled from exactly these pieces, in this order, so the
  // legacy and handshake paths share one implementation:
  //   hsBeginCycle; hsFlipAndTorches; hsComponents; hsMark;
  //   (fullPass ? hsPropagateFull : hsPrepIncremental + hsPropagateNet*)
  //   (+ hsApplyStrengthPending); hsLampsBatch(all); hsFinishCycle; hsCommit.
  // The radio/jammer flips live in hsCommit (cycle end) in BOTH modes (D12):
  // nothing outside tick() reads cur_ between ticks, so end-of-tick flip is
  // the same state the legacy start-of-tick flip produced for the next tick.
  void hsBeginCycle(ChunkMap& w, int pcx, int pcz, int radius);  // tick() bookkeeping (no flips)
  /// F56-7: round tick area (set by cSignalWorldSim from TickArea::round). A change forces a full pass.
  void setRoundArea(bool r) { if (r != roundArea_) { roundArea_ = r; fullPass_ = true; netsValid_ = false; } }
  void setDiamondArea(bool d) { if (d != diamondArea_) { diamondArea_ = d; fullPass_ = true; netsValid_ = false; } }  // JD 2026-10-23
  void hsFlipAndTorches(ChunkMap& w, const DirtyFn& dirty);      // collectMountPowered + tickTorches
  void hsComponents(ChunkMap& w, const DirtyFn& dirty);          // == tickComponents (timed)
  void hsMark(ChunkMap& w);                                      // wake sets + net marking + sorted work lists
  void hsPrepIncremental(ChunkMap& w);                           // reset marked nets + record per-net seeds/pulses
  void hsPropagateNet(ChunkMap& w, int32_t netId, const DirtyFn& dirty);  // one net's flood (pull-safe)
  void hsPropagateFull(ChunkMap& w, const DirtyFn& dirty);       // fullPass_ path (affine only)
  void hsLampsBatch(const std::vector<int64_t>& keys, ChunkMap& w, const DirtyFn& dirty);
  void hsFinishCycle(ChunkMap& w);                               // post-lamps tail (full-pass cleanup)
  void hsCommit();                                               // radio_/jammers_ flip
  bool hsCycleSkipped() const { return hsSkip_; }
  bool hsFullPass() const { return fullPass_; }
  /// LL_TICK_PROFILE (measure only): why the last cycle went wide. 1 area moved, 2 column count changed, 4 fullPass preset (edit/evict), 8 nets already invalid.
  uint32_t hsReason() const { return hsReason_; }
  uint32_t hsReason_ = 0;
  /// LL_SIGNAL_LAZYNETS (2026-10-30; default ON since 2026-10-31, =0 kill): a column load / evict only invalidates the
  /// nets when it changes the load state of a column that holds an indexed signal cell or borders
  /// one (the columns buildNets read through). Otherwise the cached nets stay. 32 in hsReason().
  void setLazyNets(bool on) { lazyNets_ = on; netsValid_ = false; topoDirty_ = true; }
  /// LL_SIGNAL_RELINK (2026-10-31; default ON, =0 kill; needs LL_SIGNAL_LAZYNETS for the border test): an
  /// area move or a border-only column load keeps the flood topology (nets, wire->net, net lamps,
  /// net readers) and only re-derives the pointer / in-area arrays. Same output as a full build.
  void setRelinkNets(bool on) { relinkNets_ = on; netsValid_ = false; topoDirty_ = true; }
  uint64_t netRelinks() const { return netRelinks_; }
  /// LL_SIGNAL_EDGERELINK (2026-10-31; default ON, =0 kill; needs RELINK + LAZYNETS): evicting a column whose wire nets
  /// all lie inside it (no net crosses a shared column face) drops those nets and compacts the ids instead of a
  /// full flood. Ids follow each net's min wire key, so the result equals a fresh flood. LL_SIGNAL_EDGECHECK=1
  /// floods anyway after a drop and logs MATCH / MISMATCH (debug only).
  void setEdgeRelink(bool on) { edgeRelink_ = on; netsValid_ = false; topoDirty_ = true; }
  uint64_t edgeDrops() const { return edgeDrops_; }
  uint64_t edgeCheckMismatches() const { return edgeMismatch_; }
  bool lazyNets() const { return lazyNets_; }
  uint64_t netBuilds() const { return netBuilds_; }  ///< buildNets() calls (tests / diagnostics)
  bool lazyNets_ = [] { const char* e = std::getenv("LL_SIGNAL_LAZYNETS"); return !(e && e[0] == '0' && e[1] == '\0'); }();  // 2026-10-31 (JD PlayC TPS): default ON; =0 kill
  uint64_t netBuilds_ = 0;
  bool relinkNets_ = [] { const char* e = std::getenv("LL_SIGNAL_RELINK"); return !(e && e[0] == '0' && e[1] == '\0'); }();  // 2026-10-31 (JD PlayC TPS): default ON; =0 kill
  bool topoDirty_ = true;      // flood inputs (indexed cells / their blocks) changed since the last build
  bool editLocal_ = false;   // LL_EDIT_LOCAL: only local edits since the last build (netComps_/netLamps_ patched)
  uint64_t netRelinks_ = 0;
  bool edgeRelink_ = [] { const char* e = std::getenv("LL_SIGNAL_EDGERELINK"); return !(e && e[0] == '0' && e[1] == '\0'); }();  // 2026-10-31 (JD PlayC TPS): default ON; =0 kill
  bool relinkScope_ = [] { const char* e = std::getenv("LL_SIGNAL_RELINKSCOPE"); return !(e && e[0] == '0' && e[1] == '\0'); }();  // relink: fast paths + netTouch only for the sim area (2026-10-31 JD: default ON, LL_SIGNAL_RELINKSCOPE=0 = off)
  bool edgeCheck_ = [] { const char* e = std::getenv("LL_SIGNAL_EDGECHECK"); return e && e[0] == '1' && e[1] == '\0'; }();
  bool edgePending_ = false;    // evicted columns recorded in edgeAffected_ since the last buildNets
  std::vector<int64_t> edgeCols_;  // columns streamed in / out since the last buildNets (cx << 32 | cz)
  uint64_t edgeDrops_ = 0, edgeMismatch_ = 0;
  bool edgeMarkColumn(int cx, int cz);
  bool edgeSplit(ChunkMap& w);
  struct NetTouch { int cx, cz; bool loaded; bool own = false; };
  std::vector<NetTouch> netTouch_;  // sorted (cx, cz); built by buildNets when lazyNets_
  bool netTouchUnchanged(const ChunkMap& w) const;
  // LL_TICK_PROFILE diagnostic (measure only): what forced the next buildNets.
  mutable int diagCx_ = 0, diagCz_ = 0;
  mutable int diagKind_ = 0;  // 0 other/edit, 1 load-state change of a touch column, 2 relevant evict, 3 eager count change
  mutable bool diagOwn_ = false;
  bool netTouches(int cx, int cz) const;
  const std::vector<int32_t>& hsMarkedNets() const { return hsMarkedNets_; }
  const std::vector<int64_t>& hsLampKeys() const { return hsLampKeys_; }
  uint64_t tickCount() const { return tickNo_; }
  /// Deferred lamp afterglow update produced by hsLampsBatch (the lampHold_
  /// map is sim-thread owned; pull batches return these, the sim thread
  /// merges them after the barrier ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â hs-design Ãƒâ€šÃ‚Â§3.3).
  struct LampHoldUpdate { int64_t key = 0; uint64_t end = 0; bool erase = false;
    bool delay = false; CellState ns = 0; };  ///< PlayB (JD t4339u): delay = a 1-tick lamp delay-line entry (ns shown next tick)
  void hsApplyLampHolds(const std::vector<LampHoldUpdate>& updates);
  void hsApplyStrengthPending(const DirtyFn* dirty = nullptr);  // replay per-net TraceStrengthBuffer writes in canonical order (+ remesh, 2026-11-01)
  void hsRefreshSoft(const ChunkMap& w);
  // 2026-11-01 PlayM (JD): same-tick components. After the flood + soft refresh, components whose inputs
  // changed re-evaluate from their pre-tick snapshot (one logical tick each, fixed sorted order) and the
  // changed outputs re-flood, until nothing changes (LL_SAME_TICK=0 off; LL_SETTLE_MAX, default 16).
  void hsSettle(ChunkMap& w, const DirtyFn& dirty);
  // Deferred component side effects (sounds, radio, wake timers, Option-A stage+commit) from the final steps.
  void hsFinalizeComponents();
  // PlayM: same-tick settle on/off (default from LL_SAME_TICK, on). Selftests that inject raw wire values
  // (no driver) turn it off around those blocks, since a settled tick would re-flood the injected wire to 0.
  void setSameTick(bool on) { sameTick_ = on; }
  bool sameTick() const { return sameTick_; }
  static bool envSameTick();
  uint64_t settleIterations() const { return settleIterTotal_; }
  uint64_t settleCapHits() const { return settleCapHits_; }  // 2026-11-01 PlayL: after the flood (sim thread), before lamps: same-tick soft power
  /// Thread-local hold updates of the calling thread's last hsLampsBatch:
  /// the caller drains this immediately after the call, on the same thread.
  static std::vector<LampHoldUpdate>& hsTlsLampHolds();
  size_t componentCount() const { return torches_.size() + wires_.size() + lamps_.size() + comps_.size(); }

  // Right-click on an interactive block: repeator delay cycle, switch toggle, button press,
  // keyboard capture toggle. Returns false if the block is not interactive.
  bool use(ChunkMap& w, int x, int y, int z, const DirtyFn& dirty, bool alt = false);
  /// Look-at inspector line: block #id, raw state fields, wire colour + network, component memory.
  std::string inspect(const ChunkMap& w, int x, int y, int z) const;
  /// Component sleep (default on): quiescent components skip ticks until a neighbour changes.
  void setComponentSleep(bool on) { compSleep_ = on; allAwake_ = true; }
  // 2026-11-01 (JD LL_WAKE): awake-list component dispatch (default off; LL_WAKE=1 or this setter).
  // Must give the same world as tick-everything-candidate mode (selftest: LL_WAKE parity).
  void setWakeList(bool on) { wakeList_ = on; allAwake_ = true; }
  bool wakeList() const { return wakeList_; }
  // 2026-11-01 (JD Option A, LL_OPTION_A=1): last committed component state bits (lock-free read, any thread).
  bool publishedComponentBits(int64_t key, uint32_t& out) const;
  /// Per-class tick-rate override (JD 2026-10-14 rate scaling): when set
  /// for a block id it replaces the class's declared
  /// cCompBehavior::tickRate() for every placed component of that
  /// class. This is the seam lithography (and tests) drive; shipped
  /// content sets NO overrides and every class declares the neutral
  /// rate, so the shipped behaviour is exactly the pre-scaling one.
  /// Setting a neutral or invalid rate clears the override.
  void setClassTickRate(BlockId block, litholattice::TickRate rate) {
    if (rate.valid() && !rate.neutral()) classRates_[block] = rate;
    else classRates_.erase(block);
  }
  /// Effective tick rate of one placed component: the override for its
  /// block id when present, else its class's declared tickRate().
  /// Validation (review A-H2): a class-declared rate is validated ONCE
  /// per BlockId (the catalog binds exactly one behaviour per BlockId
  /// for the sim's lifetime, so the validated result is cached). An
  /// invalid declaration (see TickRate::valid(): powers of two, multiply
  /// <= kMaxRateMultiply, divide <= kMaxRateDivide) is logged as an
  /// error on that first resolution and the NEUTRAL rate is substituted
  /// ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â mirroring the Timing Handshake authority's registration refusal
  /// (timing_handshake.cpp), which never holds an unschedulable
  /// component. Without this, multiply 0 would run zero sub-pulls and
  /// publish the default step (wiping cell state), an oversized multiply
  /// would hang the sim thread inside one firing, and a non-pow2 or
  /// oversized divide would silently starve the component.
  litholattice::TickRate classTickRate(BlockId block, const cCompBehavior* behavior) const;
  size_t lastTickedComponents() const { return lastTicked_; }
  // Keyboard: queue a 1-tick pulse (front = SIXBIT code + 1, control = right-port code; 0 = none).
  void keyPulse(int x, int y, int z, int front, int control);
  /// Wire / lamp states were written from outside the sim (tests): re-run the flood next tick.
  void invalidateNetwork() { fullPass_ = true; }
  /// Lamp afterglow: a lamp that goes dark keeps showing its last colour for n ticks (0 = off).
  /// Lamps feed nothing back into the circuit, so this is purely visual; callers pick n from the
  /// TPS (about 1/30 s) so 1-tick pulses at 512+ TPS still reach a rendered frame.
  void setLampHoldTicks(int n) { lampHoldTicks_ = n < 0 ? 0 : n; }

  /// Save support: the per-component memory (outputs, edge detectors, delay lines, timers, queued
  /// key pulses) of every component, grouped by chunk column key (ChunkKey::make), in a versioned
  /// byte layout (kStateVersion). Everything else the sim needs is in the blocks and cell states,
  /// so scan() + importState() after a load restores the sim exactly.
  static constexpr uint16_t kStateVersion = 3;  // v3 (2026-10-24): + u16 historyHi after beacon
  std::map<uint64_t, std::vector<uint8_t>> exportState() const;
  bool importState(const std::vector<uint8_t>& bytes);  ///< false on a malformed blob (nothing applied)

  /// Moves the sound events produced since the last call into `out` (appends).
  void takeSounds(std::vector<SoundEvent>& out) {
    out.insert(out.end(), sounds_.begin(), sounds_.end());
    sounds_.clear();
  }

  bool blockPowered(const ChunkMap& w, int x, int y, int z) const;   // fed by wire or strongly powered
  bool strongPowered(const ChunkMap& w, int x, int y, int z) const;
  int hardPower(const ChunkMap& w, int x, int y, int z) const;  // 0 = not hard-powered, else strength
  void collectMountPowered(const ChunkMap& w);  // supports of attached switches / buttons that are on  // solid+opaque+powerable above a lit torch

 private:
  struct Comp {
    const cCompBehavior* behavior;  // shared, owned by cBehaviorCatalog
    uint8_t traitFlags = 0;   ///< copy of cCompTraits::flags at place (queued / chan bits)
    uint8_t quietMode = 0;    ///< 0=virtual quiescent, 1=always, 2=never (table-driven)
    uint8_t emitMode = 0;     ///< 0=virtual emitToward, 1=front? mem.out : 0
    uint8_t hotKind = 0;      ///< PlayL2 #9: 0=generic, 1=Repeator, 2=Thresholdor, 3=Fusor, 4=Randor
    int16_t delayFixed = -1;  ///< copy of cCompTraits::delayFixed
    litholattice::TickRate rate{};  ///< PlayL Phase5: classTickRate resolved once at place
    ComponentMemory mem;
    // PlayL2 #8: packed dual-word mirror of wire-facing + latch (synced on apply; mount/emit reads these).
    uint32_t faceWord = 0;   ///< out | (out2<<8) | (pulse?1:0)<<16 | (timer<<24)
    uint32_t latchWord = 0;  ///< history | (historyHi<<8) | (prevIn<<24) — cooldown/LFSR-ish bits
    cScheduledOutput sched;  ///< 2026-11-04 (JD t4304u): input -> output delay line (not saved: in-flight changes drop on reload)
    uint64_t wakeAt = ~uint64_t(0);  ///< LL_WAKE dedupe flag: the tick this component is already queued for
    bool rateHeld = false;  ///< divided-gated on an earlier tick: component sleep must not swallow its due tick (rate scaling)
    // Input fast path (filled by buildNets, valid while the networks are): per side 0 = reads
    // nothing, 1 = wire, 2 = torch, 3 = via (colour only), 4 = generic lookup.
    bool fast = false;
    uint8_t kind[4] = {}, ch[4] = {};
    CellState* nb[4] = {};
    CellState* top = nullptr;
    CellState* self = nullptr;
    // PlayL2 #7: emit target wire (net, local idx) per absolute dir; -1 = none (filled in buildNets).
    int32_t emitNet[4] = {-1, -1, -1, -1};
    int32_t emitIdx[4] = {-1, -1, -1, -1};
  };
  static void syncCompWords(Comp& c) {
    const ComponentMemory& m = c.mem;
    c.faceWord = uint32_t(m.out) | (uint32_t(m.out2) << 8) | (uint32_t(m.pulse ? 1u : 0u) << 16) | (uint32_t(m.timer) << 24);
    c.latchWord = uint32_t(m.history) | (uint32_t(m.historyHi) << 8) | (uint32_t(m.prevIn) << 24);
  }
  ComponentStep scheduleOutput(int64_t k, Comp& c, BlockId b, ComponentStep step);  // 2026-11-04 (JD t4304u)
  struct TickArea {  // components outside the player's ticking radius are frozen
    int pcx = 0, pcz = 0, radius = 0;
    bool round = false;  // F56-7 (Proposed by JD, Built by Cortana): same disc rule as litholattice::TickArea
    bool diamond = false;  // diamond (Manhattan), JD 2026-10-23: same rule as litholattice::TickArea
    bool contains(int x, int z) const {
      const int dx = (x >> 4) - pcx, dz = (z >> 4) - pcz;
      if (std::abs(dx) > radius || std::abs(dz) > radius) return false;
      if (diamond) return std::abs(dx) + std::abs(dz) <= radius;
      return !round || int64_t(dx) * dx + int64_t(dz) * dz <= int64_t(radius) * radius + radius;
    }
  };
  void tickTorches(ChunkMap& w, const DirtyFn& dirty);     // 1) inverters, previous-tick input
  void tickComponents(ChunkMap& w, const DirtyFn& dirty);  // 2) behaviours, two-phase apply
  void propagateWires(ChunkMap& w, const DirtyFn& dirty);  // 3) decaying BFS + no-decay pulses
  void updateLamps(ChunkMap& w, const DirtyFn& dirty);     // 4) lamps / color lamps
  int inputFrom(const ChunkMap& w, int x, int y, int z, int dir) const;  // signal arriving from neighbour in dir
  int emitToward(const ChunkMap& w, int x, int y, int z, int towardDir) const;  // component at xyz -> dir
  std::unordered_set<int64_t> torches_, wires_, lamps_;
  std::unordered_set<int64_t> bindPending_;  // 2026-11-07 (JD): Catchor/Transmittor keys registered since last cycle, awaiting their one placement bind
  std::unordered_map<int64_t, Comp> comps_;
  // PlayL2: dense Comp* parallel to a sorted all-keys snapshot (compsGen_). Mount collect,
  // seed gather, and sharded tick walk contiguous pointers instead of hashing every key.
  std::vector<int64_t> compAllKeys_;
  std::vector<Comp*> compAllPtrs_;
  std::vector<Comp*> compOrderPtrs_;  // parallel to compOrder_ (in-range)
  std::vector<Comp*> wakePtrs_;       // parallel to wakeScratch_ / iterKeys when wake-iterating
  uint64_t compDenseGen_ = 0;
  void rebuildCompDense();
  bool mountFlipOk_ = false;  // PlayL2: last collectMountPowered matches live outs (skip flip rebuild)
  // 2026-11-01 (Archie list, JD approved): compiled in-range component table. Sorted keys of the
  // components inside area_, rebuilt only when comps_ changes (compsGen_) or the tick area moves;
  // no per-tick sort / alloc, range check before quiescent(). LL_COMP_TABLE=0 = old per-tick sort.
  std::vector<int64_t> compOrder_;
  uint64_t compsGen_ = 1, compOrderGen_ = 0;
  // LL_WAKE (2026-11-01): timer ring of tick slots (64), drained up to tickNo_ each component pass.
  bool wakeList_ = [] { const char* e = std::getenv("LL_WAKE"); return !(e && e[0] == '0');  /* PlayL: default ON (JD 2026-11-01) */ }();
  std::array<std::vector<int64_t>, 64> wakeRing_;
  std::vector<int64_t> wakeScratch_;
  // 2026-11-01 PlayM: same-tick settle state (sim thread only; cleared each component pass)
  struct CompSnap { ComponentMemory mem; CellState state = 0; };
  struct CompRec { int64_t k; Comp* c; ComponentStep step; uint8_t out0, out20; bool pulse0, tx; };
  std::unordered_map<int64_t, CompSnap> compSnap_;
  std::vector<CompRec> compRecs_;
  std::unordered_map<int64_t, size_t> compRecIdx_;
  std::vector<int64_t> settleCand_;
  uint64_t settleIterTotal_ = 0, settleCapHits_ = 0;
  bool gatherInputs(ChunkMap& w, int64_t k, Comp& c, int x, int y, int z, bool fastInputs, bool publish, ComponentInputs& in,
                    std::vector<JammerZone>* jammerSink = nullptr);  // bug hunt #13: sharded ticks pass a per-shard sink; nullptr publishes straight to jammers_ (serial path only)
  void wakeSchedule(int64_t key, uint64_t delta);
  void connProbe(ChunkMap& w, const DirtyFn& dirty);  // PlayM: LL_CONN_PROBE save/reload census
  bool sameTick_ = envSameTick();  // PlayM: same-tick settle (setSameTick)
  uint64_t wakeDrained_ = 0;
  // Option A (2026-11-01): shared_ptr so cSignalSim stays movable and the type may stay incomplete here.
  std::shared_ptr<class cCompiledCompTable> compTable_;
  TickArea compOrderArea_{};
  uint64_t compOutOfRange_ = 0, compTableRebuilds_ = 0;
public:
  uint64_t compOutOfRangeDispatches() const { return compOutOfRange_; }  // must stay 0 (selftest)
  uint64_t compTableRebuilds() const { return compTableRebuilds_; }
  size_t compTableSize() const { return compOrder_.size(); }
private:
  std::unordered_map<BlockId, litholattice::TickRate> classRates_;  ///< per-class tick-rate overrides (rate scaling); empty in shipped content
  mutable std::unordered_map<BlockId, litholattice::TickRate> declaredRates_;  ///< validated class-declared rates, cached per BlockId (A-H2); key-lookup only, never iterated
  cRadioReg radio_;
  cJammerReg jammers_;
  litholattice::TraceStrengthBuffer strengthBuf_;  ///< Step 1: GPU strength slots
  uint8_t dim_ = 1;  ///< this sim's dimension (for cross-dimensional radio)
  // PlayL (JD 2026-11-01): component sleep ON in-game by default so LL_WAKE is live; LL_COMP_SLEEP=0 turns it off.
  // PlayM had flipped default OFF for determinism; Archie Phase 5 (2026-11-05): restore ON — pattern-wall
  // 14.7→2.0 ms/tick at 512 TPS; LL_COMP_SLEEP=0 remains the kill switch. Selftest/handshake must stay known-good.
  bool compSleep_ = [] { const char* e = std::getenv("LL_COMP_SLEEP"); return !(e && e[0] == '0'); }(), allAwake_ = true;
 public:
  litholattice::TraceStrengthBuffer& strengthBuffer() { return strengthBuf_; }
  void setDimension(uint8_t d) { dim_ = d; }
  /// Phase timing (ms, summed since the last call). PlayK: comps/mark/wires/lamps/settle/rebuild.
  /// PlayL2: begin/flip/finalize/mount name the former unlabeled gap (~3 ms at 512 ALU).
  struct PhaseMs {
    double comps = 0, mark = 0, wires = 0, lamps = 0, settle = 0, rebuild = 0;
    double begin = 0, flip = 0, finalize = 0, mount = 0;
  };
  PhaseMs takePhaseMs() {
    PhaseMs p = phase_; phase_ = {};
    // Wires/lamps are timed inside the hs* methods (atomically: pull workers
    // run them) so the split is real in handshake mode too, not just legacy.
    p.wires = double(hsWiresNs_.exchange(0, std::memory_order_relaxed)) / 1e6;
    p.lamps = double(hsLampsNs_.exchange(0, std::memory_order_relaxed)) / 1e6;
    return p;
  }
 private:
  PhaseMs phase_;
  std::atomic<uint64_t> hsWiresNs_{0}, hsLampsNs_{0};  // hs* method self-timing (ns)
  struct SrcReach { std::vector<int32_t> nets; std::vector<int64_t> lamps; };
  std::unordered_map<int64_t, SrcReach> srcReach_;  // per changed source: nets / lamps within 2 (cleared with the nets)
  // PlayK fix 3: inverse cache — net id -> sources whose 5x5x5 box touches that net (rebuilt with nets).
  std::vector<std::vector<int64_t>> netReach_;
  std::vector<int32_t> reachNetScratch_;   // reused 5x5x5 fill
  std::vector<int64_t> reachLampScratch_;
  // PlayJ fix 5: epoch-stamped flat vectors (clear = bump epoch; no unordered_set insert churn).
  uint32_t awakeEpoch_ = 1;
  std::vector<int64_t> awakeKeys_;                  // components to tick next tick (sleep on)
  std::unordered_map<int64_t, uint32_t> awakeStamp_; // key -> epoch (warm; no erase on clear)
  void awakeClear() { if (++awakeEpoch_ == 0) { awakeStamp_.clear(); awakeEpoch_ = 1; } awakeKeys_.clear(); }
  void awakeInsert(int64_t k) {
    uint32_t& s = awakeStamp_[k];
    if (s != awakeEpoch_) { s = awakeEpoch_; awakeKeys_.push_back(k); }
  }
  bool awakeContains(int64_t k) const {
    const auto it = awakeStamp_.find(k);
    return it != awakeStamp_.end() && it->second == awakeEpoch_;
  }
  std::vector<std::vector<int64_t>> netComps_;      // per wire network: components reading it
  // PlayK fix 5: who-reads-whom adjacency for wake fanout (rebuilt in buildNets).
  std::unordered_map<int64_t, std::vector<int64_t>> compAdj_;
  size_t lastTicked_ = 0;
  void wakeAround(int64_t k);
  void rebuildCompAdj();
  void fillSrcReach(int64_t k, SrcReach& r);  // PlayK: flat scratch 5x5x5
  // PlayK fix 4: shard scratch (warm; no hot-path heap after first use).
  std::vector<std::vector<int64_t>> shardChanged_;
  struct CompNext { int64_t k; Comp* c; ComponentStep step; };
  std::vector<std::vector<CompNext>> shardNext_;
  std::vector<std::vector<std::pair<int64_t, uint64_t>>> shardWake_;  // key,delta — merged after barrier
  std::vector<std::vector<std::pair<int64_t, CompSnap>>> shardSnap_;  // sameTick snaps — merged after barrier
  std::vector<std::vector<JammerZone>> shardJammers_;               // bug hunt #13: jammer zones per shard — published serially after the barrier (cJammerReg::next_ is sim-thread-only)
  std::vector<size_t> shardTicked_;
  std::vector<size_t> shardOor_;
  std::array<std::vector<int64_t>, kMaxSignal + 1> buckets_;  // BFS work lists by strength
  std::vector<std::pair<int64_t, uint8_t>> torchNext_;
  // 2026-11-01 (JD soft power, LL_SOFT_POWER=0 off): solid powerable blocks soft-powered by a touching lit
  // Trace or a Memor front output; value = strongest feeding strength. Max-reduction: order-independent.
  std::unordered_map<int64_t, uint8_t> softPowered_;
  bool softAll_ = true;                   // 2026-11-01 PlayL: next hsRefreshSoft rescans every wire (load / import / scan)
  std::vector<int64_t> softEdits_;        // 2026-11-01 PlayL: edited cells whose soft level is re-read this tick
  std::vector<int64_t> softChanged_;  // 2026-11-01 PlayK: soft-level changes this cycle (lamps/wake only, no net re-flood)
  int softPower(int x, int y, int z) const;
  std::unordered_map<int64_t, uint8_t> mountPrev_;  // PlayM AC item 3: last tick's mountPowered_ (swapped, not copied)
  std::unordered_map<int64_t, uint8_t> mountPowered_;  // 2026-11-01 PlayL: hard-powered block -> strongest feeder; rebuilt before torches and after components
  TickArea area_;
  // LL_EDIT_LOCAL: what the flat net arrays were built against
  TickArea arrArea_{}; uint64_t arrStructGen_ = ~uint64_t(0); size_t arrColumns_ = 0; bool arrValid_ = false;
  bool roundArea_ = false;  // F56-7
  bool diamondArea_ = false;  // diamond (Manhattan) tick area (JD 2026-10-23)
  // Incremental re-flood. Wires and lamps are a pure function of torch / component outputs,
  // attached-switch power and the block layout. Wires are grouped into networks (connected runs,
  // same link rules as the flood). Each tick only the networks within 2 blocks of a source whose
  // output changed are reset and re-flooded, and only their lamps (plus lamps near the change and
  // expiring afterglows) are updated. Nothing changed: nothing runs. Layout edits, area moves and
  // outside writes force one full pass (and a network rebuild).
  bool fullPass_ = true, netsValid_ = false;
  std::unordered_map<int64_t, int32_t> wireNet_;          // wire -> network id
  std::vector<std::vector<int64_t>> nets_, netLamps_;     // per network: wires, adjacent lamps
  std::vector<uint32_t> netMarkedBits_;  ///< 1 bit per network (Step 2: bit-packed, was vector<uint8_t>)
  /// Bitset helpers (rule 2: 32 flags per uint32).
  void markNet(int32_t id) { netMarkedBits_[size_t(id) >> 5] |= 1u << (id & 31); }
  bool isNetMarked(int32_t id) const { return (netMarkedBits_[size_t(id) >> 5] >> (id & 31)) & 1u; }
  void clearNetMarks() { std::fill(netMarkedBits_.begin(), netMarkedBits_.end(), 0); }
  template <typename F> void forEachMarkedNet(F&& f) const {
    for (size_t i = 0; i < netMarkedBits_.size(); ++i) {
      uint32_t w = netMarkedBits_[i];
      while (w) {  // iterate set bits via count-trailing-zeros
        const unsigned b = std::countr_zero(w);
        f(int32_t(i * 32 + b));
        w &= w - 1;
      }
    }
  }
  // PlayJ fix 5: dirty lamps as flat vector (sorted/unique into hsLampKeys_); epoch stamp avoids set churn.
  uint32_t dirtyLampEpoch_ = 1;
  std::vector<int64_t> dirtyLampKeys_;
  std::unordered_map<int64_t, uint32_t> dirtyLampStamp_;
  void dirtyLampsClear() { if (++dirtyLampEpoch_ == 0) { dirtyLampStamp_.clear(); dirtyLampEpoch_ = 1; } dirtyLampKeys_.clear(); }
  void dirtyLampsInsert(int64_t k) {
    uint32_t& s = dirtyLampStamp_[k];
    if (s != dirtyLampEpoch_) { s = dirtyLampEpoch_; dirtyLampKeys_.push_back(k); }
  }
  template <typename It> void dirtyLampsInsertRange(It b, It e) { for (; b != e; ++b) dirtyLampsInsert(*b); }
  std::vector<int64_t> changedSrc_, extraLamps_;  // PlayL2/MCHPRS PR#206: changed-node flush list (sources this tick)
  // Option 16 / lithography Trace-in-time: compile settled Trace (nets_/compAdj_/emitNet) into a chip + clock � deferred past L2.
  void buildNets(ChunkMap& w);
  // Flat per-network arrays for the incremental pass (no hash lookups in the flood): cached state
  // pointers (valid until the layout changes, which rebuilds them), CSR links, scratch.
  struct NetArr {
    std::vector<int64_t> keys; std::vector<CellState*> st; std::vector<int32_t> adjStart, adj;
    std::vector<CellState> old; std::vector<uint32_t> stamp; std::vector<uint8_t> via;
    std::vector<uint8_t> level;  // PlayL2 #11: SoA net power (contiguous); flood writes here, syncs to CellState*
    uint32_t pulseGen = 0;  // per-net pulse visited epoch (was the shared stampGen_ for this net's
                            // stamps; stamp arrays are per-net, so a per-net epoch is equivalent)
  };
  std::vector<NetArr> netArr_;
  // ---- handshake per-net work (hsPrepIncremental fills, hsPropagateNet consumes) ----
  bool hsSkip_ = false;                       // hsMark found nothing to do this cycle
  std::vector<int32_t> hsMarkedNets_;         // marked nets, ascending (canonical order)
  std::vector<int64_t> hsLampKeys_;           // this cycle's lamp keys, sorted ascending
  struct NetWork {
    std::vector<std::pair<int32_t, int>> seeds;    // (local idx, strength) in legacy seed order
    std::vector<std::pair<int32_t, int>> pulses;   // (local idx, strength) in legacy pulse order
    std::vector<std::pair<int64_t, uint8_t>> strength;  // pending TraceStrengthBuffer writes, cell order
    std::vector<std::pair<int32_t, int>> lastSeeds;  // PlayM AC item 4: sorted seeds of the last flood
    uint64_t lastGen = 0;                            // netGen_ of that flood (0 = none / had pulses)
  };
  uint64_t netGen_ = 1;                          // PlayM AC item 4: bumped by buildNets and every full pass
  std::atomic<uint64_t> sameSeedSkips_{0};       // PlayM AC item 4: floods skipped (pull workers: atomic)
  std::vector<NetWork> hsNetWork_;  // by net id; only marked nets are filled
  std::unordered_map<int64_t, std::pair<int32_t, int32_t>> wireLoc_;  // in-area wire -> (net, index)
  std::array<std::vector<std::pair<int32_t, int32_t>>, kMaxSignal + 1> nbuckets_;
  uint32_t stampGen_ = 0;
  // Lamps fed only by wires: cached pointers to themselves and their wire neighbours.
  struct LampFast { CellState* self; std::vector<CellState*> wires; bool color; };
  std::unordered_map<int64_t, LampFast> lampFast_;
  size_t netColumns_ = 0;
  std::vector<std::pair<int64_t, const ChunkColumn*>> netColPtrs_;  // columns the cached pointers reach (indexed cells + 8 neighbours), as built
  std::vector<int64_t> genSeedCols_;  ///< 2026-10-31: replaced circuit columns this tick (sorted, from netColPtrs_)
  std::vector<int64_t> genSeeds_;     ///< cells in those columns, re-flooded next pass (incremental GENCHECK)
  uint64_t genEvents_ = 0;            ///< replaced-column events seen (diag)
  uint64_t netStructGen_ = ~uint64_t(0);  // 2026-10-31 (JD Trace seam): ChunkMap::structureGen() the cached pointers were built against
  // Gate 1 (Archie Q0-Q17, LL_SIGNAL_SIMSCOPE=1, default off until JD locks it): the nets'
  // column-change check is scoped to the sim area plus a 1-column border. A column streamed
  // in or out far from the player no longer invalidates every net (full rebuild); only a
  // change inside the area (+border) does. areaFp_ fingerprints the resident ChunkColumn*
  // set there; simScopeFp() recomputes it (|area| hash lookups per tick).
  bool simScope_ = [] { const char* e = std::getenv("LL_SIGNAL_SIMSCOPE"); return e && e[0] == '1' && e[1] == '\0'; }();
  uint64_t areaFp_ = 0;
  uint64_t simScopeFp(const ChunkMap& w, int pcx, int pcz, int radius) const;
  void wireLinks(const ChunkMap& w, int x, int y, int z, std::vector<int64_t>& out) const;  // no area test
  bool inScope(int64_t wireKey) const {
    if (fullPass_) return true;
    const auto it = wireNet_.find(wireKey);
    return it != wireNet_.end() && isNetMarked(it->second);
  }
  int lampHoldTicks_ = 0;
  std::unordered_map<int64_t, uint64_t> lampHold_;  // lamp -> tick its afterglow ends
  /// PlayB (JD t4339u): LEDs / lamps have a 1-tick delay on both on and off. lamp -> (state to show,
  /// tick it was computed); sim-thread owned, read-only during lamp batches, merged by hsApplyLampHolds.
  std::unordered_map<int64_t, std::pair<CellState, uint64_t>> lampDelay_;
  uint64_t tickNo_ = 0, nextHoldExpiry_ = UINT64_MAX;
  uint64_t editGen_ = 0;  // PlayM2: bumped by every place/break/scan (mount candidates + lithography invalidation)
  // PlayM2 incremental mount: dense indices of comps that can ever hard-power a block (switch/button, or
  // a hard-capable comp beside a solid powerable block). Rebuilt only when editGen_/dense layout changes.
  std::vector<uint32_t> mountCand_;
  uint64_t mountCandGen_ = ~uint64_t(0); size_t mountCandN_ = ~size_t(0); const void* mountCandPtr_ = nullptr; bool mountCandHard_ = false;
  std::shared_ptr<struct LithoIR> litho_;
  std::vector<SoundEvent> sounds_;
};

// Headless checks (--selftest) of every block's logic. Returns failures.
int runSignalSelfTest();

}  // namespace lithokraft
