// R3 (B7, 2026-11-17): one admission order for streaming. Requests, staged promotion, view-gate
// entry, the first mesh and the upload queue all rank by this, so a column in front of the
// player always comes before an equally distant one behind, and equal ranks never fall back to
// hash-map or worker-completion order.
//
// The rank is a pure function of (centre column, view sector, footprint metric, key). It uses
// integers only, so it gives the same order on every host and every backend.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <tuple>

namespace lithokraft {

class cStreamRank final {
 public:
  static constexpr int kSectors = 16;  // view direction quantized to 22.5 degree sectors
  static constexpr int kNoDirection = -1;

  /// Lower sorts first. primary = view-weighted footprint distance, secondary = squared distance,
  /// then the column key and the sub-chunk index as tie-breaks.
  struct Key {
    uint32_t primary = 0;
    uint32_t secondary = 0;
    uint64_t column = 0;
    int32_t sub = 0;
    bool operator<(const Key& o) const {
      return std::tie(primary, secondary, column, sub) < std::tie(o.primary, o.secondary, o.column, o.sub);
    }
    bool operator==(const Key& o) const {
      return primary == o.primary && secondary == o.secondary && column == o.column && sub == o.sub;
    }
  };

  /// Sector of a yaw in radians (forward = (cos yaw, sin yaw), as in cPlayer::lookDir).
  static int sectorOf(float yaw) {
    if (!std::isfinite(yaw)) return kNoDirection;
    const double t = double(yaw) * (double(kSectors) / 6.283185307179586);
    int q = int(std::floor(t + 0.5)) % kSectors;
    return q < 0 ? q + kSectors : q;
  }
  static uint64_t columnKey(int cx, int cz) { return (uint64_t(uint32_t(cx)) << 32) | uint32_t(cz); }

  cStreamRank() = default;
  cStreamRank(int cx, int cz, int sector, bool diamond) : cx_(cx), cz_(cz), sector_(sector), diamond_(diamond) {}

  int centreX() const { return cx_; }
  int centreZ() const { return cz_; }
  int sector() const { return sector_; }
  bool diamond() const { return diamond_; }
  cStreamRank recentred(int cx, int cz) const { return cStreamRank(cx, cz, sector_, diamond_); }

  /// View weight in halves: 2 = in front (within ~60 degrees) or right next to the player,
  /// 3 = to the side, 4 = behind. A column behind ranks like one twice as far in front.
  int weight(int dx, int dz) const {
    if (sector_ < 0) return 2;
    const int64_t d2 = int64_t(dx) * dx + int64_t(dz) * dz;
    if (std::max(std::abs(dx), std::abs(dz)) <= 1) return 2;
    const int64_t dot = int64_t(dx) * kCos16[sector_] + int64_t(dz) * kSin16[sector_];  // ~16 |d| cos(angle)
    if (dot > 0 && dot * dot >= 64 * d2) return 2;  // cos >= 0.5
    if (dot >= 0) return 3;
    return 4;
  }

  Key column(int cx, int cz) const {
    const int dx = cx - cx_, dz = cz - cz_;
    const int64_t e2 = int64_t(dx) * dx + int64_t(dz) * dz, w = weight(dx, dz);
    Key k;
    // Diamond footprint: |dx|+|dz| rings (fix 3b). Square/round: Euclidean, so the weight is squared.
    k.primary = clampU32(diamond_ ? (int64_t(std::abs(dx)) + std::abs(dz)) * w : e2 * w * w);
    k.secondary = clampU32(e2);
    k.column = columnKey(cx, cz);
    return k;
  }

  /// Sub-chunk sy of column (cx, cz), seen from sub-chunk row `csy`. Height adds to the
  /// footprint distance, so the ground underfoot comes before the sky above a near column.
  Key section(int cx, int sy, int cz, int csy) const {
    const int dx = cx - cx_, dz = cz - cz_, dy = sy - csy;
    const int64_t e2 = int64_t(dx) * dx + int64_t(dy) * dy + int64_t(dz) * dz, w = weight(dx, dz);
    Key k;
    k.primary = clampU32(diamond_ ? (int64_t(std::abs(dx)) + std::abs(dz) + std::abs(dy)) * w : e2 * w * w);
    k.secondary = clampU32(e2);
    k.column = columnKey(cx, cz);
    k.sub = sy;
    return k;
  }

 private:
  static uint32_t clampU32(int64_t v) { return v < 0 ? 0u : v > int64_t(UINT32_MAX) ? UINT32_MAX : uint32_t(v); }
  // round(16 cos(k * 22.5 deg)) and round(16 sin(k * 22.5 deg)): integer tables, no libm in the order.
  static constexpr int kCos16[kSectors] = {16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6, 0, 6, 11, 15};
  static constexpr int kSin16[kSectors] = {0, 6, 11, 15, 16, 15, 11, 6, 0, -6, -11, -15, -16, -15, -11, -6};
  int cx_ = 0, cz_ = 0;
  int sector_ = kNoDirection;
  bool diamond_ = false;
};

}  // namespace lithokraft
