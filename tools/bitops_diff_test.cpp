// bitops_diff_test.cpp — build-59 differential proof for the bit-ops sweep.
//
// Standalone (NOT wired into CMake/ctest, so the baseline test counts do
// not move). Build and run:
//   g++ -O2 -std=c++17 -I engine/src/core/include \
//       tools/bitops_diff_test.cpp -o /tmp/bitops_diff_test && /tmp/bitops_diff_test
//
// Every adopted call site is proven here against a VERBATIM copy of the
// original expression it replaces: same inputs, byte/value-identical
// outputs, over randomized inputs (fixed-seed PRNG, reproducible) plus
// hand-picked edges. A micro-bench (original vs header spelling) runs at
// the end; medians are printed — environment: this workstation, 2 vCPU.
//
// Held by the hash law (documented, NOT tested for adoption because the
// "optimization" would change values): re-associating/vectorizing the
// FNV-1a byte loops — the adoption here is the per-step spelling only,
// the loop order is untouched. interleave3 (subdiv/micro_table.h) is a
// documented latent bug for inputs >= 1024; not adopted, not fixed —
// any fix moves hashes and is JD's call.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "litholattice/core/bitops.h"

using namespace litholattice::bitops;

static int g_fail = 0;
#define CHECK(cond)                                                     \
  do {                                                                  \
    if (!(cond)) {                                                      \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

// Deterministic PRNG for the test itself (SplitMix64 stream).
static uint64_t g_rng = 0x123456789ABCDEF0ull;
static uint64_t rnd() {
  uint64_t z = (g_rng += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

// ---------------------------------------------------------------------------
// Verbatim originals (copied from the tree before the build-59 swaps).
// ---------------------------------------------------------------------------
namespace orig {

// procgen/seed.h mix64
constexpr uint64_t mix64(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}
// sim/det_sched.cpp detMix64 (identical body to mix64)
uint64_t detMix64(uint64_t x) {
  uint64_t z = x + 0x9E3779B97F4A7C15ULL;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}
// procgen/seed.h nameHash loop (body over bytes)
constexpr uint64_t nameHash(const char* s, size_t n) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < n; ++i) h = (h ^ uint8_t(s[i])) * 0x100000001B3ull;
  return h;
}
// sim/det_sched.cpp hashBytes loop (kFnvPrime = 1099511628211 = 0x100000001B3)
uint64_t hashBytes(const void* p, size_t n, uint64_t h) {
  const auto* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 1099511628211ULL;
  }
  return h;
}
// procgen/seed.h Rng::rotl (k in [1,63] at both call sites: 7 and 45)
constexpr uint64_t rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

// world/voxel_palette.h packed access
uint64_t loadWord(const uint8_t* base, size_t wi) {
  uint64_t v;
  std::memcpy(&v, base + wi * 8, 8);
  return v;
}
void storeWord(uint8_t* base, size_t wi, uint64_t v) { std::memcpy(base + wi * 8, &v, 8); }
uint64_t readPacked(const uint8_t* base, int w, size_t i) {
  const size_t bit = i * size_t(w);
  const size_t wi = bit >> 6, sh = bit & 63;
  uint64_t v = loadWord(base, wi) >> sh;
  if (sh + size_t(w) > 64) v |= loadWord(base, wi + 1) << (64 - sh);
  return v & ((uint64_t(1) << w) - 1);
}
void writePacked(uint8_t* base, int w, size_t i, uint64_t idx) {
  const size_t bit = i * size_t(w);
  const size_t wi = bit >> 6, sh = bit & 63;
  const uint64_t m = (uint64_t(1) << w) - 1;
  storeWord(base, wi, (loadWord(base, wi) & ~(m << sh)) | ((idx & m) << sh));
  if (sh + size_t(w) > 64) {
    const uint64_t hi = (uint64_t(1) << (sh + size_t(w) - 64)) - 1;
    storeWord(base, wi + 1, (loadWord(base, wi + 1) & ~hi) | (idx >> (64 - sh)));
  }
}

// render/software_renderer.cpp StoredQuad AO pack/expand expressions
uint8_t aoPackOne(uint8_t ao, uint8_t faceAo, int k) {
  return uint8_t(ao | (((faceAo >> 3) & 3) << (2 * k)));
}
uint8_t aoExpand(uint8_t faceRot, uint8_t aoPack, int k) {
  return uint8_t(faceRot | (((aoPack >> (2 * k)) & 3) << 3));
}
}  // namespace orig

// ---------------------------------------------------------------------------
// New spellings — exactly the compositions the swapped call sites use.
// ---------------------------------------------------------------------------
static inline uint64_t newMix64(uint64_t z) { return splitmixMix64(z + 0x9E3779B97F4A7C15ull); }
static inline uint64_t newNameHash(const char* s, size_t n) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < n; ++i) h = fnv1aStep(h, uint8_t(s[i]));
  return h;
}
static inline uint64_t newHashBytes(const void* p, size_t n, uint64_t h) {
  const auto* b = static_cast<const unsigned char*>(p);
  for (size_t i = 0; i < n; ++i) h = fnv1aStep(h, b[i]);
  return h;
}
// voxel_palette swap composition: the hi word is loaded ONLY when the
// field spills (same short-circuit as the original — an unconditional
// load could read one word past the final storage word).
static inline uint64_t newReadPacked(const uint8_t* base, int w, size_t i) {
  const size_t bit = i * size_t(w);
  const size_t wi = bit >> 6, sh = bit & 63;
  const uint64_t lo = orig::loadWord(base, wi);
  const uint64_t hi = (sh + size_t(w) > 64) ? orig::loadWord(base, wi + 1) : 0;
  return extractBits2(lo, hi, unsigned(sh), w);
}
static inline void newWritePacked(uint8_t* base, int w, size_t i, uint64_t idx) {
  const size_t bit = i * size_t(w);
  const size_t wi = bit >> 6, sh = bit & 63;
  const bool spill = sh + size_t(w) > 64;
  uint64_t lo = orig::loadWord(base, wi);
  uint64_t hi = spill ? orig::loadWord(base, wi + 1) : 0;
  insertBits2(lo, hi, unsigned(sh), w, idx);
  orig::storeWord(base, wi, lo);
  if (spill) orig::storeWord(base, wi + 1, hi);
}
static inline uint8_t newAoPackOne(uint8_t ao, uint8_t faceAo, int k) {
  return uint8_t(insertBits32(ao, faceAo >> 3, unsigned(2 * k), 2));
}
static inline uint8_t newAoExpand(uint8_t faceRot, uint8_t aoPack, int k) {
  return uint8_t(faceRot | (extractBits32(aoPack, unsigned(2 * k), 2) << 3));
}

int main() {
  // --- mix64 / detMix64 vs splitmixMix64(+golden) -------------------------
  {
    const uint64_t edges[] = {0, 1, ~0ull, 0x9E3779B97F4A7C15ull, 0x8000000000000000ull, 42};
    for (uint64_t v : edges) {
      CHECK(orig::mix64(v) == newMix64(v));
      CHECK(orig::detMix64(v) == newMix64(v));
    }
    for (int i = 0; i < 200000; ++i) {
      const uint64_t v = rnd();
      CHECK(orig::mix64(v) == newMix64(v));
      CHECK(orig::detMix64(v) == newMix64(v));
    }
    std::printf("mix64/detMix64: OK (200k random + edges)\n");
  }
  // --- nameHash / hashBytes FNV-1a step -----------------------------------
  {
    std::vector<uint8_t> buf(4096);
    for (auto& b : buf) b = uint8_t(rnd());
    for (size_t n : {size_t(0), size_t(1), size_t(7), size_t(64), size_t(4096)}) {
      CHECK(orig::nameHash((const char*)buf.data(), n) == newNameHash((const char*)buf.data(), n));
      for (uint64_t h0 : {0ull, 1469598103934665603ULL, ~0ull})
        CHECK(orig::hashBytes(buf.data(), n, h0) == newHashBytes(buf.data(), n, h0));
    }
    const char* names[] = {"organic", "ore_vein", "infinite", "", "a"};
    for (const char* s : names)
      CHECK(orig::nameHash(s, std::strlen(s)) == newNameHash(s, std::strlen(s)));
    std::printf("nameHash/hashBytes: OK\n");
  }
  // --- rotl (original defined for k in [1,63]; header also defines k=0) ---
  {
    for (int i = 0; i < 50000; ++i) {
      const uint64_t v = rnd();
      const int k = 1 + int(rnd() % 63);
      CHECK(orig::rotl(v, k) == rotl64(v, unsigned(k)));
    }
    CHECK(rotl64(0x1234ull, 0) == 0x1234ull);  // header-only defined case
    CHECK(rotr64(rotl64(0xDEADBEEFull, 13), 13) == 0xDEADBEEFull);
    std::printf("rotl64: OK (50k random, k=1..63)\n");
  }
  // --- palette readPacked/writePacked --------------------------------------
  {
    const int widths[] = {1, 2, 3, 4, 5, 7, 8, 9, 12, 15, 16, 17, 24, 32};
    for (int w : widths) {
      const size_t cells = 300;
      const size_t bytes = ((cells * size_t(w) + 63) / 64 + 1) * 8;  // +1 spill guard word
      std::vector<uint8_t> a(bytes), b(bytes);
      for (size_t k = 0; k < bytes; ++k) a[k] = b[k] = uint8_t(rnd());
      for (int trial = 0; trial < 4000; ++trial) {
        const size_t i = rnd() % cells;
        // idx deliberately wider than the field in many trials: the
        // original's high-word spill term is unmasked, and the header
        // spelling must reproduce that quirk bit-for-bit.
        const uint64_t idx = (trial % 3 == 0) ? rnd() : (rnd() & ((w >= 64) ? ~0ull : ((1ull << w) - 1)));
        orig::writePacked(a.data(), w, i, idx);
        newWritePacked(b.data(), w, i, idx);
        CHECK(a == b);
        CHECK(orig::readPacked(a.data(), w, i) == newReadPacked(b.data(), w, i));
      }
      // Full random-access read sweep on the final buffers.
      for (size_t i = 0; i < cells; ++i)
        CHECK(orig::readPacked(a.data(), w, i) == newReadPacked(b.data(), w, i));
    }
    std::printf("readPacked/writePacked: OK (14 widths x 4000 writes, full buffers byte-identical)\n");
  }
  // --- StoredQuad AO pack/expand (exhaustive) -------------------------------
  {
    for (int fr = 0; fr < 256; ++fr)
      for (int p = 0; p < 256; ++p)
        for (int k = 0; k < 4; ++k)
          CHECK(orig::aoExpand(uint8_t(fr), uint8_t(p), k) == newAoExpand(uint8_t(fr), uint8_t(p), k));
    // Pack is an accumulation over 4 corners into disjoint fields.
    for (int trial = 0; trial < 20000; ++trial) {
      uint8_t fa[4];
      for (auto& v : fa) v = uint8_t(rnd());
      uint8_t a1 = 0, a2 = 0;
      for (int k = 0; k < 4; ++k) {
        a1 = orig::aoPackOne(a1, fa[k], k);
        a2 = newAoPackOne(a2, fa[k], k);
      }
      CHECK(a1 == a2);
      // Round trip through expand with faceRot = face & 0x67 semantics.
      const uint8_t faceRot = uint8_t(fa[0] & 0x67);
      for (int k = 0; k < 4; ++k)
        CHECK(orig::aoExpand(faceRot, a1, k) == newAoExpand(faceRot, a2, k));
    }
    std::printf("AO pack/expand: OK (expand exhaustive 256x256x4, pack 20k random quads)\n");
  }
  // --- generic extract/insert vs hand expressions ---------------------------
  {
    for (int i = 0; i < 100000; ++i) {
      const uint64_t v = rnd();
      const unsigned sh = unsigned(rnd() % 40);
      const int w = 1 + int(rnd() % 24);  // sh + w <= 63: no-spill domain
      CHECK(extractBits(v, sh, w) == ((v >> sh) & ((uint64_t(1) << w) - 1)));
      const uint64_t f = rnd();
      const uint64_t m = (uint64_t(1) << w) - 1;
      CHECK(insertBits(v, f, sh, w) == ((v & ~(m << sh)) | ((f & m) << sh)));
    }
    std::printf("extractBits/insertBits: OK (100k random)\n");
  }

  if (g_fail) {
    std::printf("BITOPS DIFF TEST: %d FAILURE(S)\n", g_fail);
    return 1;
  }
  std::printf("BITOPS DIFF TEST: ALL OK\n");

  // --- micro-bench: original vs header spelling (medians) -------------------
  auto bench = [](const char* name, auto&& fn, uint64_t iters) {
    std::vector<double> ts;
    for (int run = 0; run < 7; ++run) {
      volatile uint64_t sink = 0;
      const auto t0 = std::chrono::steady_clock::now();
      sink = fn(iters);
      const auto t1 = std::chrono::steady_clock::now();
      (void)sink;
      ts.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / double(iters));
    }
    std::sort(ts.begin(), ts.end());
    std::printf("bench %-28s median %.3f ns/op\n", name, ts[ts.size() / 2]);
  };
  bench("orig mix64", [](uint64_t n) { uint64_t a = 0; for (uint64_t i = 0; i < n; ++i) a ^= orig::mix64(i + a); return a; }, 2000000);
  bench("new  mix64 (header)", [](uint64_t n) { uint64_t a = 0; for (uint64_t i = 0; i < n; ++i) a ^= newMix64(i + a); return a; }, 2000000);
  {
    std::vector<uint8_t> buf(1 << 16);
    for (auto& b : buf) b = uint8_t(rnd());
    bench("orig hashBytes 64KiB/call", [&](uint64_t) { return orig::hashBytes(buf.data(), buf.size(), 0); }, 1);
    bench("new  hashBytes 64KiB/call", [&](uint64_t) { return newHashBytes(buf.data(), buf.size(), 0); }, 1);
  }
  {
    std::vector<uint8_t> a(((300 * 16 + 63) / 64 + 1) * 8, 0);
    bench("orig readPacked w=16", [&](uint64_t n) { uint64_t s = 0; for (uint64_t i = 0; i < n; ++i) s += orig::readPacked(a.data(), 16, i % 300); return s; }, 2000000);
    bench("new  readPacked w=16", [&](uint64_t n) { uint64_t s = 0; for (uint64_t i = 0; i < n; ++i) s += newReadPacked(a.data(), 16, i % 300); return s; }, 2000000);
  }
  return 0;
}
