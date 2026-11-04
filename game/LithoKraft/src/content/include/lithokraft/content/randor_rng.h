// Randor (JD 2026-10-31): deterministic random range block helpers. Lock-free (one relaxed atomic).
#pragma once
#include <atomic>
#include <cstdint>

namespace lithokraft::randor {

/// Lowest value a roll can produce. JD's spec: 1..N (N 1..64). CloudAtlas relayed 0..N; set this to 0
/// for that rule (N is then clamped to >= kLow as well). One constant, so the decision is one edit.
inline constexpr int kLow = 1;

/// splitmix64 (Steele, Lea, Flood 2014): public-domain mixing step.
constexpr uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

/// World seed the sim mixes into every roll (set by the game before the sim starts).
inline std::atomic<uint64_t>& worldSeed() {
  static std::atomic<uint64_t> s{0};
  return s;
}

/// Seed of the roll at packed cell key `key` on world tick `tick`.
constexpr uint64_t rollSeed(uint64_t world, int64_t key, uint64_t tick) {
  return splitmix64(splitmix64(splitmix64(world) ^ uint64_t(key)) ^ tick);
}

/// Value of a roll in kLow..n (n is clamped to kLow..64 first).
constexpr int roll(uint64_t rng, int n) {
  const int hi = n < kLow ? kLow : (n > 64 ? 64 : n);
  return kLow + int(rng % uint64_t(hi - kLow + 1));
}
// 2026-10-31 8:20 PM (JD): never the same value twice in a row, still uniform and deterministic:
// roll over the c-1 other values and skip prev (r >= prev -> r + 1), no reroll loop.
// Degenerate range (one value, N = kLow or N = 1 with kLow 1) can only repeat: returns that value.
constexpr int rollNoRepeat(uint64_t rng, int n, int prev) {
  const int hi = n < kLow ? kLow : (n > 64 ? 64 : n);
  const int c = hi - kLow + 1;
  if (c <= 1 || prev < kLow || prev > hi) return roll(rng, n);
  const int r = kLow + int(rng % uint64_t(c - 1));
  return r >= prev ? r + 1 : r;
}

}  // namespace lithokraft::randor
