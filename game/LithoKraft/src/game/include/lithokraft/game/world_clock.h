// Cortana 2026-10-16 [GAME] (JD 2026-10-16): the persisted world clock and the 24 h in-game day.
//
// A day lasts 32 real minutes. The sim's tick rate is player-selectable (32..512 TPS, power-of-two
// ladder), so the day is NOT a fixed tick count: each sim tick advances the clock by
// kUnitsPerSecond / TPS units (exact for every rate on the ladder), so a day is 32 real minutes at
// any rate. At the default 32 TPS that is 61,440 ticks per day (2,560 ticks per in-game hour).
// `tick` counts every sim tick ever run in this world; both values persist in level.json (v2),
// so they survive quit/reload. Deterministic: the same tick sequence at the same rates gives the
// same clock. The clock is never part of the handshake world hash.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

namespace lithokraft::worldclock {

inline constexpr uint64_t kUnitsPerSecond = 512;  ///< LCM of the 32..512 TPS ladder
inline constexpr uint64_t kRealSecondsPerDay = 32 * 60;
inline constexpr uint64_t kUnitsPerDay = kUnitsPerSecond * kRealSecondsPerDay;  // 983,040
inline constexpr int kDefaultTps = 32;
inline constexpr uint64_t kTicksPerDayAtDefaultTps = uint64_t(kDefaultTps) * kRealSecondsPerDay;  // 61,440
inline constexpr uint32_t kGameMinutesPerDay = 24 * 60;

struct Clock {
  uint64_t tick = 0;  ///< sim ticks ever run in this world
  uint64_t time = 0;  ///< kUnitsPerSecond units of real-at-rate time since the world began
};

inline uint64_t unitsPerTick(int tps) { return std::max<uint64_t>(1, kUnitsPerSecond / uint64_t(std::clamp(tps, 1, 512))); }
inline void advance(Clock& c, uint64_t ticks, int tps) { c.tick += ticks; c.time += ticks * unitsPerTick(tps); }
inline uint32_t dayOf(uint64_t time) { return uint32_t(time / kUnitsPerDay); }
/// In-game minute of the day, 0..1439 (00:00 .. 23:59).
inline uint32_t minuteOfDay(uint64_t time) { return uint32_t((time % kUnitsPerDay) * kGameMinutesPerDay / kUnitsPerDay); }
/// "HH:MM" (24 h).
inline std::string hhmm(uint64_t time) {
  const uint32_t m = minuteOfDay(time);
  char b[8];
  std::snprintf(b, sizeof b, "%02u:%02u", unsigned(m / 60), unsigned(m % 60));
  return b;
}
/// Migration of the 2026-10-15 v6 player play clock (ticks, assumed at the default 32 TPS).
inline Clock fromLegacyPlayTicks(uint64_t playTicks) { return Clock{playTicks, playTicks * unitsPerTick(kDefaultTps)}; }

}  // namespace lithokraft::worldclock
