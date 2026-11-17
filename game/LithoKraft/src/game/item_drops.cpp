// Dropped items (P-1..P-6): merge planning, the floating model, the save record. See item_drops.h.
#include "lithokraft/game/item_drops.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <tuple>

namespace lithokraft::itemdrop {
namespace {
#include "item_drop_models.inc"  // kItemDropVertices, kItemDropModelRows (tools/gen_item_drop_models.py)

constexpr size_t kRows = sizeof(kItemDropModelRows) / sizeof(kItemDropModelRows[0]);

double frac(double v) { return v - std::floor(v); }
}  // namespace

// ---- merge ----
std::vector<MergePair> planMerges(const std::vector<MergeCandidate>& items, uint64_t worldTick, double radius,
                                  const SameIdentityFn& same, bool all) {
  std::vector<MergePair> pairs;
  const size_t n = items.size();
  if (n < 2 || !(radius > 0)) return pairs;
  // Sorted cell grid: (cell x, cell z, index). Cells are radius wide, so every match of an item
  // lies in its own or a neighbouring cell.
  std::vector<std::tuple<int64_t, int64_t, size_t>> grid;
  grid.reserve(n);
  for (size_t i = 0; i < n; ++i)
    grid.emplace_back(int64_t(std::floor(items[i].x / radius)), int64_t(std::floor(items[i].z / radius)), i);
  std::sort(grid.begin(), grid.end());
  std::vector<uint32_t> count(n);
  std::vector<char> gone(n, 0);
  for (size_t i = 0; i < n; ++i) count[i] = items[i].count;
  for (size_t i = 0; i < n; ++i) {
    if (gone[i]) continue;
    const MergeCandidate& a = items[i];
    const bool due = all || (a.moving ? worldTick % kMergeMovingEvery == 0 : (worldTick + a.id) % kMergeRestEvery == 0);
    if (!due) continue;
    const int64_t cx = int64_t(std::floor(a.x / radius)), cz = int64_t(std::floor(a.z / radius));
    size_t best = n;
    double bestD = 0;
    for (int64_t gx = cx - 1; gx <= cx + 1; ++gx)
      for (int64_t gz = cz - 1; gz <= cz + 1; ++gz) {
        auto it = std::lower_bound(grid.begin(), grid.end(), std::make_tuple(gx, gz, size_t(0)));
        for (; it != grid.end() && std::get<0>(*it) == gx && std::get<1>(*it) == gz; ++it) {
          const size_t j = std::get<2>(*it);
          if (j == i || gone[j]) continue;
          const MergeCandidate& b = items[j];
          if (b.data != a.data || b.payload != a.payload) continue;
          const double dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
          if (std::fabs(dx) > radius || std::fabs(dz) > radius || std::fabs(dy) > kMergeHeight) continue;
          const uint32_t cap = std::min(a.cap, b.cap);
          if (uint64_t(count[i]) + count[j] > cap) continue;
          if (a.payload && !(same && same(a.id, b.id))) continue;
          const double d = dx * dx + dy * dy + dz * dz;
          if (best == n || d < bestD || (d == bestD && b.id < items[best].id)) { best = j; bestD = d; }
        }
      }
    if (best == n) continue;
    const bool keepA = count[i] > count[best] || (count[i] == count[best] && a.id < items[best].id);
    const size_t s = keepA ? i : best, g = keepA ? best : i;
    count[s] += count[g];
    gone[g] = 1;
    pairs.push_back({items[s].id, items[g].id});
  }
  return pairs;
}

// ---- render ----
size_t modelCount() { return kRows; }
const ItemDropModelRow& modelRow(size_t i) { return kItemDropModelRows[i]; }

const ItemDropModel& fallbackCube() {
  static const ItemDropVertex verts[36] = {
      // top (y = 1)
      {0, 1, 1, 0, 255, 0}, {1, 1, 1, 255, 255, 0}, {1, 1, 0, 255, 0, 0}, {0, 1, 1, 0, 255, 0}, {1, 1, 0, 255, 0, 0}, {0, 1, 0, 0, 0, 0},
      // bottom (y = 0)
      {0, 0, 0, 0, 0, 1}, {1, 0, 0, 255, 0, 1}, {1, 0, 1, 255, 255, 1}, {0, 0, 0, 0, 0, 1}, {1, 0, 1, 255, 255, 1}, {0, 0, 1, 0, 255, 1},
      // north (z = 0)
      {1, 0, 0, 255, 255, 2}, {0, 0, 0, 0, 255, 2}, {0, 1, 0, 0, 0, 2}, {1, 0, 0, 255, 255, 2}, {0, 1, 0, 0, 0, 2}, {1, 1, 0, 255, 0, 2},
      // south (z = 1)
      {0, 0, 1, 0, 255, 3}, {1, 0, 1, 255, 255, 3}, {1, 1, 1, 255, 0, 3}, {0, 0, 1, 0, 255, 3}, {1, 1, 1, 255, 0, 3}, {0, 1, 1, 0, 0, 3},
      // west (x = 0)
      {0, 0, 0, 0, 255, 4}, {0, 0, 1, 255, 255, 4}, {0, 1, 1, 255, 0, 4}, {0, 0, 0, 0, 255, 4}, {0, 1, 1, 255, 0, 4}, {0, 1, 0, 0, 0, 4},
      // east (x = 1)
      {1, 0, 1, 0, 255, 5}, {1, 0, 0, 255, 255, 5}, {1, 1, 0, 255, 0, 5}, {1, 0, 1, 0, 255, 5}, {1, 1, 0, 255, 0, 5}, {1, 1, 1, 0, 0, 5},
  };
  static const ItemDropModel cube{verts, 36, 0, "fallback:cube"};
  return cube;
}

void cModelIndex::build(const std::function<bool(const char*, uint32_t*)>& resolve) {
  byItem_.clear();
  for (size_t i = 0; i < kRows; ++i) {
    const ItemDropModelRow& r = kItemDropModelRows[i];
    uint32_t item = 0;
    if (!resolve || !resolve(r.id, &item)) continue;
    byItem_.push_back({item, ItemDropModel{kItemDropVertices + r.first, r.count, r.shape, r.id}});
  }
  // Rows are in id order; a second row for the same item (an alias) never replaces the first.
  std::stable_sort(byItem_.begin(), byItem_.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  byItem_.erase(std::unique(byItem_.begin(), byItem_.end(), [](const auto& a, const auto& b) { return a.first == b.first; }),
                byItem_.end());
}

const ItemDropModel* cModelIndex::find(uint32_t itemId) const {
  auto it = std::lower_bound(byItem_.begin(), byItem_.end(), itemId, [](const auto& e, uint32_t k) { return e.first < k; });
  return (it != byItem_.end() && it->first == itemId) ? &it->second : nullptr;
}

uint64_t mixId(uint64_t z) {
  z += 0x9E3779B97F4A7C15ull;
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

float sinTurns(float turns) {
  double t = frac(double(turns));      // [0, 1)
  if (t >= 0.5) t -= 1.0;              // [-0.5, 0.5)
  if (t > 0.25) t = 0.5 - t;           // fold into [-0.25, 0.25]
  else if (t < -0.25) t = -0.5 - t;
  const double x = t * 6.283185307179586, x2 = x * x;
  // Taylor series to x^9 on [-pi/2, pi/2]: max error about 4e-6.
  const double s = x * (1.0 - x2 / 6.0 * (1.0 - x2 / 20.0 * (1.0 - x2 / 42.0 * (1.0 - x2 / 72.0))));
  return float(s);
}

int posesFor(uint64_t id, double ageTicks, int tps, uint32_t count, double x, double y, double z, DropPose out[4]) {
  const uint64_t h = mixId(id);
  const double phase = double(h & 0xFFFFu) / 65536.0;
  const double t = (ageTicks < 0 ? 0.0 : ageTicks) / double(tps < 1 ? 1 : tps);  // seconds
  const float bob = kBobAmplitude * 0.5f * (1.0f + sinTurns(float(frac(t / kBobPeriodSeconds + phase))));
  const double yaw = frac(t * (kSpinDegreesPerSecond / 360.0) + phase);
  const int n = copiesFor(count);
  for (int c = 0; c < n; ++c) {
    DropPose& p = out[c];
    p.scale = kDrawScale;
    if (c == 0) {
      p.x = float(x); p.y = float(y) + kHover + bob; p.z = float(z); p.yawTurns = float(yaw);
      continue;
    }
    const uint64_t hc = mixId(id ^ (0xA24BAED4963EE407ull * uint64_t(c)));
    const float ox = (float((hc >> 0) & 0xFFu) / 255.0f - 0.5f) * 0.16f;
    const float oz = (float((hc >> 8) & 0xFFu) / 255.0f - 0.5f) * 0.16f;
    const float oy = float((hc >> 16) & 0xFFu) / 255.0f * 0.05f;
    const float oyaw = float((hc >> 24) & 0xFFu) / 255.0f * 0.25f;
    p.x = float(x) + ox; p.y = float(y) + kHover + bob + oy; p.z = float(z) + oz;
    p.yawTurns = float(frac(yaw + oyaw));
  }
  return n;
}

void appendModel(const ItemDropModel& m, const uint16_t layers[6], const DropPose& p, float ox, float oy, float oz,
                 std::vector<litholattice::PackedVertex>& out) {
  const float c = cosTurns(p.yawTurns), s = sinTurns(p.yawTurns);
  auto enc = [](float v, float o) -> uint16_t {
    const float q = std::floor((v - o) * 16.f + 0.5f);
    return uint16_t(q < 0.f ? 0.f : q > 65535.f ? 65535.f : q);
  };
  const uint8_t mode = m.shape == 4 ? uint8_t(litholattice::kShadeCutout) : uint8_t(litholattice::kShadeTex);
  // Face index for the shading normal (+x, -x, +y, -y, +z, -z), per slot after the spin: the side
  // faces turn with the model, so each takes the nearest axis (the shade steps at 45 degrees).
  uint8_t faceOf[6];
  faceOf[0] = 2;  // top
  faceOf[1] = 3;  // bottom
  {
    static const float kSide[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};  // north, south, west, east (x, z)
    for (int k = 0; k < 4; ++k) {
      const float nx = kSide[k][0] * c - kSide[k][1] * s, nz = kSide[k][0] * s + kSide[k][1] * c;
      faceOf[2 + k] = std::fabs(nx) >= std::fabs(nz) ? (nx >= 0 ? 0 : 1) : (nz >= 0 ? 4 : 5);
    }
  }
  constexpr uint8_t kOpenAo = 3u << 3;  // bits 3-4: no corner occlusion on a floating item
  for (uint32_t i = 0; i < m.count; ++i) {
    const ItemDropVertex& v = m.verts[i];
    const float lx = (v.x - 0.5f) * p.scale, ly = v.y * p.scale, lz = (v.z - 0.5f) * p.scale;
    const float rx = lx * c - lz * s, rz = lx * s + lz * c;
    litholattice::PackedVertex q{};
    q.x = enc(p.x + rx, ox);
    q.y = enc(p.y + ly, oy);
    q.z = enc(p.z + rz, oz);
    // Model UVs are 0..255 across the face image; packed UVs count sixteenths of a tile.
    q.u = uint8_t((uint32_t(v.u) * 16u + 127u) / 255u);
    q.v = uint8_t((uint32_t(v.v) * 16u + 127u) / 255u);
    const uint8_t slot = v.slot < 6 ? v.slot : 0;
    q.setLayer(layers[slot]);
    q.faceAo = uint8_t(faceOf[slot] | kOpenAo);
    q.mode = mode;
    out.push_back(q);
  }
}

// ---- save record ----
namespace {
void putF64(litholattice::storage::ByteWriter& w, double v) { uint64_t u; std::memcpy(&u, &v, 8); w.u64(u); }
double getF64(litholattice::storage::ByteReader& r) { const uint64_t u = r.u64(); double v; std::memcpy(&v, &u, 8); return v; }
void putStr(litholattice::storage::ByteWriter& w, const std::string& s) { w.bytes(litholattice::storage::Bytes(s.begin(), s.end())); }
std::string getStr(litholattice::storage::ByteReader& r) { const auto b = r.bytes(); return std::string(b.begin(), b.end()); }
}  // namespace

litholattice::storage::Bytes encodeItems(std::vector<SavedItem> items) {
  using namespace litholattice::storage;
  std::sort(items.begin(), items.end(), [](const SavedItem& a, const SavedItem& b) { return a.id < b.id; });
  ByteWriter w;
  w.u16(kItemsVersion);
  w.u32(uint32_t(items.size()));
  for (const SavedItem& it : items) {
    w.u64(it.id);
    putF64(w, it.x); putF64(w, it.y); putF64(w, it.z);
    putF64(w, it.vx); putF64(w, it.vy); putF64(w, it.vz);
    w.u32(it.ageTicks); w.u32(it.count); w.u32(it.delayTicks);
    w.u8(it.onGround ? 1 : 0);
    putStr(w, it.item);
    w.u8(it.hasData ? 1 : 0);
    if (it.hasData) {
      putStr(w, it.name);
      w.u16(uint16_t(std::min<size_t>(it.certificates.size(), 0xFFFF)));
      for (size_t i = 0; i < it.certificates.size() && i < 0xFFFF; ++i) putStr(w, it.certificates[i]);
    }
  }
  ByteWriter file;
  file.u32(kItemsMagic);
  file.u32(crc32(w.out.data(), w.out.size()));
  file.bytes(w.out);
  return file.out;
}

bool decodeItems(const litholattice::storage::Bytes& file, std::vector<SavedItem>* out, std::string* err) {
  using namespace litholattice::storage;
  auto fail = [&](const char* why) { if (err) *err = why; return false; };
  ByteReader r(file.data(), file.size());
  const uint32_t magic = r.u32(), crc = r.u32();
  const Bytes body = r.bytes();
  if (!r.ok() || magic != kItemsMagic || crc32(body.data(), body.size()) != crc) return fail("items file: bad magic or CRC");
  ByteReader b(body.data(), body.size());
  const uint16_t v = b.u16();
  if (v != kItemsVersion) return fail("items file: unsupported version");
  const uint32_t n = b.u32();
  if (!b.ok() || n > (1u << 20)) return fail("items file: bad item count");
  std::vector<SavedItem> items;
  items.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    SavedItem it;
    it.id = b.u64();
    it.x = getF64(b); it.y = getF64(b); it.z = getF64(b);
    it.vx = getF64(b); it.vy = getF64(b); it.vz = getF64(b);
    it.ageTicks = b.u32(); it.count = b.u32(); it.delayTicks = b.u32();
    it.onGround = b.u8() != 0;
    it.item = getStr(b);
    it.hasData = b.u8() != 0;
    if (it.hasData) {
      it.name = getStr(b);
      const uint16_t nc = b.u16();
      for (uint16_t k = 0; k < nc && b.ok(); ++k) it.certificates.push_back(getStr(b));
    }
    if (!b.ok()) return fail("items file: truncated");
    if (!std::isfinite(it.x) || !std::isfinite(it.y) || !std::isfinite(it.z) || !std::isfinite(it.vx) ||
        !std::isfinite(it.vy) || !std::isfinite(it.vz))
      continue;  // a non-finite record is dropped, the rest load
    if (!items.empty() && it.id <= items.back().id) return fail("items file: ids out of order");
    items.push_back(std::move(it));
  }
  if (!b.done()) return fail("items file: trailing bytes");
  *out = std::move(items);
  return true;
}

}  // namespace lithokraft::itemdrop
