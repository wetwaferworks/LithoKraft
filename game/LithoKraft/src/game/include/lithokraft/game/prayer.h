// Cortana 2026-10-15 2255 [GAME] (JD 22:38 / 22:50): "digital prayer".
// Cortana 2026-10-16 [GAME] (JD rework): prayers are mealtime prayers. A prayer completes when the
// player kneels for more than kPrayerKneelTicks consecutive sim ticks INSIDE one of the three
// in-game windows 09:00-09:32 (breakfast), 13:00-13:32 (lunch) and 17:00-17:32 (dinner). Each
// window pays at most once per in-game day (so 3 per day max); one kneel pays at most once. The
// kneel count only runs while inside a window (a kneel started before 09:00 starts counting at
// 09:00). Reward unchanged: 2% of the static needed for the next level, minimum 1.
// The day and time of day come from the persisted world clock (world_clock.h): a day is 32 real
// minutes, so each 32-game-minute window lasts 42.67 real seconds (1,365 ticks at 32 TPS).
// Pure and tick-driven: the same per-tick kneel/clock sequence gives the same grants.
#pragma once
#include <algorithm>
#include <cstdint>

#include "lithokraft/content/certificates.h"
#include "lithokraft/game/world_clock.h"

namespace lithokraft::prayer {

inline constexpr uint32_t kPrayerKneelTicks = 64;  // a prayer completes on the 65th in-window tick
inline constexpr uint8_t kPrayersPerDay = 3;
inline constexpr uint32_t kWindowMinutes = 32;
inline constexpr uint32_t kWindowStartMinute[kPrayersPerDay] = {9 * 60, 13 * 60, 17 * 60};

struct State {
  uint32_t prayerDay = 0;    ///< persisted (player.lat v7): the world day `windows` counts for
  uint8_t windows = 0;       ///< persisted: bitmask of the windows already prayed on prayerDay
  uint32_t kneelTicks = 0;   ///< transient: the current continuous in-window kneel
  bool spent = false;        ///< transient: this kneel already paid out
  uint8_t prayersToday() const { return uint8_t((windows & 1u) + ((windows >> 1) & 1u) + ((windows >> 2) & 1u)); }
};

/// The mealtime window (0 breakfast, 1 lunch, 2 dinner) the world time is in, or -1.
inline int windowAt(uint64_t worldTime) {
  const uint32_t m = worldclock::minuteOfDay(worldTime);
  for (int i = 0; i < int(kPrayersPerDay); ++i)
    if (m >= kWindowStartMinute[i] && m < kWindowStartMinute[i] + kWindowMinutes) return i;
  return -1;
}

/// Static granted by one prayer at `total` static: 2% of the next level's cost, minimum 1.
inline int32_t reward(int64_t total) {
  const int lvl = staticLevelForTotal(std::max<int64_t>(0, total));
  const int64_t span = staticTotalForLevel(lvl + 1) - staticTotalForLevel(lvl);
  return int32_t(std::max<int64_t>(1, span * 2 / 100));
}

/// Advance one sim tick at world time `worldTime` (the clock AFTER this tick).
/// Returns the static to grant this tick (0 almost always).
inline int32_t tick(State& s, bool kneeling, int64_t staticTotal, uint64_t worldTime) {
  const uint32_t day = worldclock::dayOf(worldTime);
  if (day > s.prayerDay) { s.prayerDay = day; s.windows = 0; }
  // 2026-10-16: a clock that went BACK a day (older level.json beside a newer
  // player.lat, a time command) must not reopen the windows: that day is spent.
  else if (day < s.prayerDay) { s.prayerDay = day; s.windows = 0x7; }
  if (!kneeling) { s.kneelTicks = 0; s.spent = false; return 0; }
  const int w = windowAt(worldTime);
  if (w < 0) { s.kneelTicks = 0; return 0; }  // outside every window: nothing counts
  if (s.kneelTicks < 0xFFFFFFFFu) ++s.kneelTicks;
  if (s.spent || s.kneelTicks <= kPrayerKneelTicks || (s.windows & (1u << w))) return 0;
  s.spent = true;
  s.windows = uint8_t(s.windows | (1u << w));
  return reward(staticTotal);
}

}  // namespace lithokraft::prayer
