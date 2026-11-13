// Swivelor detents (JD 2026-11-11 design session section 4, STATED): strength 0 / 16 / 32 / 64, one quarter
// turn each, 0 = home / default; heading clockwise viewed from above (CloudAtlas row B: 0 north = front,
// 16 east, 32 south, 64 west). Any other strength is not a stated detent -> -1 (OPEN).
#pragma once
namespace lithokraft::components {
constexpr int swivelorDetentQuarter(int strength) {
  return strength == 0 ? 0 : strength == 16 ? 1 : strength == 32 ? 2 : strength == 64 ? 3 : -1;
}
}  // namespace lithokraft::components
