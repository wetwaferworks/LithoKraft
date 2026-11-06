// Archie Phase 4: POD traits for cCompBehavior constants (no vtable on hot paths).
// Filled once at catalog bind from the class's virtuals; sim packs modes onto Comp at place.
#pragma once
#include <cstdint>
#include "litholattice/sim/tick_rate.h"

namespace lithokraft {

struct cCompTraits {
  litholattice::TickRate rate{};   ///< class tickRate() (A-H2 validation stays in classTickRate)
  int16_t delayFixed = -1;         ///< >=0: constant delayTicks; -1: call virtual (state-dependent)
  uint8_t flags = 0;

  static constexpr uint8_t kQueuedOutput = 1u << 0;
  static constexpr uint8_t kQuietAlways  = 1u << 1;  ///< quiescent() always true
  static constexpr uint8_t kQuietNever   = 1u << 2;  ///< quiescent() always false
  static constexpr uint8_t kEmitFrontOut = 1u << 3;  ///< emitToward == front? mem.out : 0
  static constexpr uint8_t kEmitChan0    = 1u << 4;  ///< emitChannel always 0

  bool queuedOutput() const { return (flags & kQueuedOutput) != 0; }
  bool quietAlways() const { return (flags & kQuietAlways) != 0; }
  bool quietNever() const { return (flags & kQuietNever) != 0; }
  bool emitFrontOut() const { return (flags & kEmitFrontOut) != 0; }
  bool emitChan0() const { return (flags & kEmitChan0) != 0; }

  // Packed onto Comp at place time (hot path: no flag decode, no traits pointer).
  // quietMode: 0 = call quiescent(), 1 = always quiet, 2 = never quiet
  // emitMode:  0 = call emitToward(), 1 = front? mem.out : 0
  uint8_t quietMode() const {
    if (flags & kQuietAlways) return 1;
    if (flags & kQuietNever) return 2;
    return 0;
  }
  uint8_t emitMode() const { return (flags & kEmitFrontOut) ? 1 : 0; }
};

}  // namespace lithokraft
