// Block behaviours: one class per block type that does something.
//
// A block opts into a behaviour by name in the block registry (BlockDef::behavior, e.g. "repeator").
// cBehaviorCatalog resolves those names to instances once, so neither the sim nor the HUD switches
// on block types; a new block is one class plus one registry line.
//  - cBlockBehavior: describe() for the look-at readout (wire, torch, lamps, every component).
//  - cCompBehavior: also ticks, emits signal and reacts to right-click (repeator, pulsor, ...).
// Behaviours are stateless and shared; per-cell runtime data lives in ComponentMemory.
#pragma once
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "lithokraft/engine.h"
#include "lithokraft/content/blocks.h"
#include "litholattice/sim/tick_rate.h"
#include "lithokraft/content/behavior_traits.h"

namespace lithokraft {

/// Per-cell runtime memory the sim keeps for each placed component.
struct ComponentMemory {
  uint8_t out = 0, out2 = 0;   ///< outputs this tick: main port / secondary port
  bool pulse = false;          ///< main output is a no-decay pulse this tick (pulsor)
  uint8_t prevIn = 0, prevSide = 0, prevClk = 0;  ///< edge detectors
  uint8_t history = 0;         ///< repeator delay line (bit i = input i ticks ago)
  uint8_t timer = 0;           ///< button press / pulsor animation countdown
  uint16_t historyHi = 0;      ///< repeator delay line bits 16..31 (2026-10-24 JD: delays 4/8/16/32; SGNL v3). PlayM: bits 32..63 live in prevIn/prevSide/prevClk/channel (64-tick delay)
  std::deque<std::pair<uint8_t, uint8_t>> keys;  ///< keyboard: queued (front, control) pulses
  uint8_t channel = 1;         ///< radio channel for the transmittor / receivor (1..64; 0 = the Daemon, reserved)
  bool jammer = false;         ///< cOverclockor: jammer item installed (v1: toggled by use; item UI later)
  bool beacon = false;         ///< transmittor: generate its own signal instead of reading the wire
  uint8_t lastIn = 0;          ///< PlayN (JD 11-05): Repeator fresh-edge detector, last back value (not saved; 0 after load)
};

/// Signal arriving at a component this tick, by absolute direction (0 N, 1 E, 2 S, 3 W).
struct ComponentInputs {
  std::array<int, 4> side{};  ///< strength arriving from each horizontal neighbour
  int top = 0;                ///< wire lying on top of the block
  std::array<uint8_t, 4> ch{}; ///< colour channel of the wire / torch on each side (0 = none / component)
  int radioIn = 0;            ///< wireless signal on this cell's channel (receivors only)
  uint8_t radioCol = 0;       ///< colour of the received wireless signal (1 red, 2 blue)
  uint8_t muxSides = 0;       ///< 2026-10-31: Redirector only, bit d = a Repeator's front faces our side d
  bool muxOn = false;         ///< 2026-10-31: any of those Repeators is on (level-held select)
  uint8_t switchIn = 0;       ///< 2026-10-31: Repeator only, 64 when a lit Switch / Button sits behind its back port
  uint64_t rng = 0;           ///< 2026-10-31: Randor only, splitmix64 roll seed (world seed + cell + tick)
  uint8_t repSides = 0;       ///< 2026-11-01 PlayK: Catchor only, bit d = a Repeator's front faces our side d (LL_REP_LINK)
  uint8_t chainCh = 0;        ///< 2026-10-31: colour of a Repeator directly behind a Repeator (1 red, 2 blue)
  /// 2026-10-25 (JD roadmap 7, comparator-clock port): the "container level" of each horizontal
  /// neighbour (cCompBehavior::readLevel, or the wafer count of an Input Buffer), -1 = nothing
  /// readable there. Filled only for Thresholdors (the comparator reads what sits behind it).
  std::array<int, 4> level{-1, -1, -1, -1};
  /// B9 2026-11-12 (JD): Watchor only -- the cell straight ahead of its front (the eye), read in the
  /// component pass before any apply. watchValid = false: nothing was read (not a Watchor).
  BlockId watchId = 0;
  CellState watchState = 0;
  bool watchValid = false;
  /// B9 2026-11-12 (JD): Pushor only -- soft / hard power of the blocks above and below (its top / bottom
  /// faces), whether a component output drives straight into a Trace face (hard powered), and whether an
  /// adjacent (6 sides) Pushor was hard powered last tick (quasi-connectivity).
  int pushVert = 0;
  bool pushHard = false;
  bool pushQuasi = false;

  int back(int facing) const { return side[dirBack(facing)]; }
  int left(int facing) const { return side[dirLeft(facing)]; }
  int right(int facing) const { return side[dirRight(facing)]; }
};

/// Sounds the sim reports to the game (which turns them into audio commands).
enum class SoundCue : uint8_t { Place, Break, Click, PulsorFire, Tone };

/// Result of one component tick.
struct ComponentStep {
  int out = 0;         ///< main output strength (0..64)
  int out2 = 0;        ///< secondary port strength (keyboard control port)
  CellState state = 0; ///< new cell state (drives the look and the HUD readout)
  bool pulse = false;  ///< `out` travels the wire run without decay for one tick
  std::optional<SoundCue> sound{};///< emitted with the new state's value (pitch / strength)
};

/// Readout of a block type. Implementations must be stateless (one instance serves every cell).
class cBlockBehavior {
 public:
  virtual ~cBlockBehavior() = default;

  /// One-line HUD readout of a cell, e.g. "Repeator | delay 4 | out 64 | facing E".
  virtual std::string describe(const BlockDef& def, CellState /*state*/) const { return std::string(def.name); }
  /// Colour this block passes to neighbouring components (0 = none, 1 red, 2 blue).
  /// Trace (cWire) and LEDs (cTorch) return BlockDef::channel; default none.
  virtual int colour(const BlockDef& /*def*/) const { return 0; }
};

/// Logic of one signal component type.
class cCompBehavior : public cBlockBehavior {
 public:

  /// Computes this tick's outputs from the previous tick's inputs; may update `mem`.
  virtual ComponentStep tick(const ComponentInputs& in, CellState state, ComponentMemory& mem) const = 0;

  /// Strength driven toward absolute direction `dir`. Default: main output on the front only.
  virtual int emitToward(int dir, CellState state, const ComponentMemory& mem) const {
    return dir == stFacing(state) ? mem.out : 0;
  }

  /// Wire colour the output toward dir may enter (0 = any, else a wire channel: 1 red, 2 blue).
  virtual int emitChannel(int /*dir*/, CellState /*state*/, const ComponentMemory& /*mem*/) const { return 0; }

  /// Using an adjacent wire of colour channel (1 red, 2 blue): nullopt if that does nothing.
  virtual std::optional<CellState> useChannel(CellState /*state*/, int /*channel*/) const { return std::nullopt; }

  /// Right-click interaction. Returns the new cell state, or nullopt if the block is not usable.
  virtual std::optional<CellState> use(CellState /*state*/, ComponentMemory& /*mem*/) const { return std::nullopt; }
  /// True when, with the same inputs as last tick, tick() would change nothing (output, state,
  /// memory): the sim may then skip it until a neighbour changes (component sleep). Default: never.
  virtual bool quiescent(CellState /*state*/, const ComponentMemory& /*mem*/) const { return false; }
  /// Tick rate of this component class relative to the world tick (JD
  /// 2026-10-14, Timing Handshake rate scaling): divide = the sim pulls
  /// this class once per `divide` world ticks (its published outputs
  /// hold between pulls); multiply = the class is sub-pulled `multiply`
  /// times per world tick (ComponentMemory chains across sub-pulls; only
  /// the final sub-pull's step publishes). Default neutral (1/1): every
  /// tick, once — the pre-scaling behaviour. Lithography changes a chip
  /// class's rate by overriding THIS flag: a simple flag change on the
  /// class, nothing else (JD's ruling). Powers of two; see
  /// litholattice/sim/tick_rate.h.
  virtual litholattice::TickRate tickRate() const { return {}; }
  /// 2026-10-25 (JD roadmap 7): the level a Thresholdor in read mode reads from this cell when it
  /// sits directly behind it (Minecraft's comparator reading a container), 0..64; -1 = not readable.
  virtual int readLevel(CellState /*state*/, const ComponentMemory& /*mem*/) const { return -1; }
  /// Sneak + use (secondary setting, e.g. a Catchor's match value). Default: nothing.
  virtual std::optional<CellState> useAlt(CellState /*state*/, ComponentMemory& /*mem*/) const { return std::nullopt; }
  /// 2026-11-04 (JD t4304u / t4306u): this class's delay, ticks from input change to output change.
  /// The class decides it (state = the cell's new state, key / tick for seeded per-event delays); the
  /// sim's thin shared queue (cScheduledOutput) only releases the output on that tick. 0 = immediate.
  virtual int delayTicks(CellState /*state*/, int64_t /*cellKey*/, uint64_t /*tick*/) const { return 0; }
  /// false: the class's tick() already times its output (the Repeator's settable line); the queue
  /// passes it straight through and delayTicks() is informational.
  virtual bool queuedOutput() const { return true; }
};

/// Name -> behaviour table, and BlockId -> behaviour resolved through the block registry.
class cBehaviorCatalog {
 public:
  /// The catalog with every built-in behaviour, bound to the blocks that name them.
  static const cBehaviorCatalog& get();

  /// Behaviour of block `b` (a plain name-only readout for blocks without one).
  const cBlockBehavior& behavior(BlockId b) const;
  /// Component logic of block `b`, or nullptr if it is not a ticking component.
  const cCompBehavior* component(BlockId b) const { return b < components_.size() ? components_[b] : nullptr; }
  /// Archie Phase 4: POD traits for block `b` (empty traits for non-components).
  const cCompTraits& traits(BlockId b) const {
    static const cCompTraits kEmpty{};
    return b < traits_.size() ? traits_[b] : kEmpty;
  }
  /// Behaviour registered under `name`, or nullptr.
  const cBlockBehavior* byName(std::string_view name) const;
  /// HUD readout of a cell: the block's behaviour describes its state.
  std::string describe(BlockId b, CellState state) const { return behavior(b).describe(blockRegistry().def(b), state); }
  /// Colour a block def passes to components (via its behaviour class). Same path for Trace and LEDs.
  int colour(const BlockDef& def) const {
    if (const cBlockBehavior* b = byName(def.behavior)) return b->colour(def);
    return 0;
  }

 private:
  cBehaviorCatalog();
  void add(std::string_view name, std::unique_ptr<cBlockBehavior> behavior);

  cBlockBehavior plain_;
  std::vector<std::pair<std::string_view, std::unique_ptr<cBlockBehavior>>> named_;
  std::vector<const cBlockBehavior*> behaviors_;
  std::vector<const cCompBehavior*> components_;
  std::vector<cCompTraits> traits_;  ///< Archie Phase 4: parallel to components_
};

/// 2026-10-25 (JD): does block `b` have a secondary (sneak / Alt + right click) setting in
/// state `s`? Probes useAlt on a scratch memory copy (pure; safe on the main thread).
inline bool hasSneakUse(BlockId b, CellState s) {
  const cCompBehavior* c = cBehaviorCatalog::get().component(b);
  ComponentMemory scratch{};
  return c && c->useAlt(s, scratch).has_value();
}

/// Compass name of a facing (N, E, S, W).
const char* facingName(int facing);

}  // namespace lithokraft
