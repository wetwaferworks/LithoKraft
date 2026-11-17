// Dropped items (P-1..P-6, 2026-11-17; JD rulings 2:07 AM: magnet on, merge radius about one block,
// 8-minute despawn kept, items saved as entities).
//
// Everything here is plain functions over the id-ordered entity table. There are no locks, no RNG,
// and nothing iterates an unordered container into a result:
// - physics runs once per world tick (cItemBehavior, entities.h) in entity-id order;
// - merging plans pairs from a sorted cell grid, in id order;
// - the floating model is a pure function of (entity id, age in ticks, count, position), so GL,
//   VK and the software renderer get the same vertices;
// - the save record lists items in id order.
//
// Clean room: written from the LithoKraft design notes (study Part C), no outside code.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "litholattice/entity/entity.h"
#include "litholattice/render/renderer.h"
#include "litholattice/storage/storage.h"

namespace lithokraft::itemdrop {

// ---- tuning: seconds and blocks; ticksFor() converts at the world's TPS ----
inline constexpr double kLifeSeconds = 480.0;             ///< JD: 8-minute despawn
inline constexpr double kPlayerDropDelaySeconds = 2.0;    ///< a thrown stack can't be re-grabbed at once
inline constexpr double kNaturalDropDelaySeconds = 0.25;  ///< a popped block
inline constexpr double kGravity = 22.0;                  ///< blocks/s^2 (unchanged from v1)
inline constexpr double kTerminalSpeed = 24.0;            ///< blocks/s
inline constexpr double kAirDragPerSecond = 0.6;          ///< v *= 1 - k dt
inline constexpr double kGroundDragScale = 20.0;          ///< on the ground v *= 1 - (1 - friction) k dt
inline constexpr double kBounce = 0.35;                   ///< landing rebound
inline constexpr double kBounceMinSpeed = 4.0;            ///< slower landings just stop
inline constexpr double kBuoyancy = 27.0;                 ///< up accel while the item's centre is in liquid (> gravity: floats)
inline constexpr double kLiquidDragPerSecond = 4.0;
inline constexpr double kMaxRiseSpeed = 1.5;              ///< blocks/s, rising through liquid
inline constexpr double kHalfWidth = 0.15;                ///< item AABB (0.3 block)
inline constexpr double kHeight = 0.3;
inline constexpr double kMagnetRadius = 4.0;              ///< JD: items fly to the player
inline constexpr double kMagnetPull = 14.0;
inline constexpr double kMergeRadius = 1.0;               ///< JD: about one block
inline constexpr double kMergeHeight = 0.5;
inline constexpr uint32_t kMergeMovingEvery = 2;          ///< ticks
inline constexpr uint32_t kMergeRestEvery = 40;           ///< ticks, spread by id
inline constexpr double kPlayerHalfWidth = 0.3, kPlayerHeight = 1.8;
inline constexpr double kPickupReachH = 1.0, kPickupReachV = 0.5;
// Floating look (CloudAtlas suggestion, a starting point for JD).
inline constexpr float kDrawScale = 0.30f;
inline constexpr float kHover = 0.08f;
inline constexpr float kBobAmplitude = 0.10f;
inline constexpr float kBobPeriodSeconds = 2.0f;
inline constexpr float kSpinDegreesPerSecond = 40.0f;
inline constexpr double kPickupFlightSeconds = 0.15;

inline uint32_t ticksFor(double seconds, int tps) {
  const double t = seconds * double(tps < 1 ? 1 : tps) + 0.5;
  return t <= 0 ? 0u : t >= 4294967295.0 ? 0xFFFFFFFFu : uint32_t(t);
}

// ---- P-3 merge ----
struct MergeCandidate {
  uint64_t id = 0;
  double x = 0, y = 0, z = 0;
  uint32_t data = 0, count = 0, cap = 0;
  bool moving = false;
  bool payload = false;  ///< carries a data stack (name / certificates)
};
struct MergePair {
  uint64_t survivor = 0, absorbed = 0;
  bool operator==(const MergePair& o) const { return survivor == o.survivor && absorbed == o.absorbed; }
};
/// Same-identity test for two payload-carrying items (both have payloads when called).
using SameIdentityFn = std::function<bool(uint64_t a, uint64_t b)>;
/// Plans this tick's merges. `items` must be in ascending id order. An item takes part when its
/// cadence is due (moving: every kMergeMovingEvery ticks; at rest: every kMergeRestEvery ticks,
/// offset by id), or always when `all`. A due item looks for the nearest match within `radius`
/// horizontally and kMergeHeight vertically (ties: lower id); the bigger stack survives (ties:
/// lower id) and stays where it is. Counts never exceed `cap`. Pairs come out in the order they
/// were made; later pairs see the earlier counts.
std::vector<MergePair> planMerges(const std::vector<MergeCandidate>& items, uint64_t worldTick, double radius,
                                  const SameIdentityFn& same, bool all = false);

// ---- P-4 pickup ----
/// Does the item AABB at (ex, ey, ez) (ey = its base) touch the player AABB at feet (px, py, pz),
/// enlarged by kPickupReachH sideways and kPickupReachV up and down?
inline bool inPickupReach(double px, double py, double pz, double ex, double ey, double ez) {
  const double h = kPlayerHalfWidth + kPickupReachH + kHalfWidth;
  if (ex - px > h || px - ex > h || ez - pz > h || pz - ez > h) return false;
  return ey + kHeight >= py - kPickupReachV && ey <= py + kPlayerHeight + kPickupReachV;
}

// ---- P-5 render ----
struct ItemDropVertex {
  float x, y, z;   ///< model space, blocks (footprint 0..1, y up from the base)
  uint8_t u, v;    ///< 0..255, v = 0 at the top of the image
  uint8_t slot;    ///< face slot: 0 top, 1 bottom, 2 front (north), 3 back (south), 4 left (west), 5 right (east)
};
struct ItemDropModelRow {
  const char* id;
  uint8_t shape;  ///< 0 full block, 1 half slab, 2 component slab, 3 special, 4 foliage
  uint32_t first, count;
  float min[3], max[3];
};
struct ItemDropModel {
  const ItemDropVertex* verts = nullptr;
  uint32_t count = 0;
  uint8_t shape = 0;
  const char* id = "";
};
/// The CloudAtlas models (generated table, sorted by id).
size_t modelCount();
const ItemDropModelRow& modelRow(size_t i);
/// A unit cube, for items without a model.
const ItemDropModel& fallbackCube();
/// Item id -> model, built once from a resolver (string id -> item id; nullopt when unknown).
class cModelIndex {
 public:
  void build(const std::function<bool(const char* stringId, uint32_t* itemId)>& resolve);
  const ItemDropModel* find(uint32_t itemId) const;  ///< nullptr: no model for this item
  size_t size() const { return byItem_.size(); }
 private:
  std::vector<std::pair<uint32_t, ItemDropModel>> byItem_;  // sorted by item id
};

/// Copies drawn for a stack (1, 2-5, 6-20, 21+ -> 1..4).
inline int copiesFor(uint32_t count) { return count <= 1 ? 1 : count <= 5 ? 2 : count <= 20 ? 3 : 4; }
/// 64-bit mix of the entity id (splitmix64 finaliser): the per-item phase and copy offsets.
uint64_t mixId(uint64_t id);
/// sin(2 pi turns) by a fixed polynomial (no libm, same bits on every platform).
float sinTurns(float turns);
inline float cosTurns(float turns) { return sinTurns(turns + 0.25f); }

struct DropPose {
  float x = 0, y = 0, z = 0;  ///< base centre, world blocks
  float yawTurns = 0;         ///< 0..1
  float scale = kDrawScale;
};
/// The poses of every copy of one item: a pure function of (id, age, count, position).
/// `ageTicks` is the entity age in world ticks; `tps` the world rate.
/// `ageTicks` may carry a fraction (the partial tick between two sim ticks, render only).
int posesFor(uint64_t id, double ageTicks, int tps, uint32_t count, double x, double y, double z, DropPose out[4]);
/// P-5 interpolation weight: how far the frame is past the last item tick, 0..1, from the time
/// since that tick and the tick rate. 1 when `deterministic` (bench / golden runs).
inline double tickAlpha(double secondsSinceTick, int tps, bool deterministic) {
  if (deterministic || !(secondsSinceTick > 0)) return deterministic ? 1.0 : 0.0;
  const double a = secondsSinceTick * double(tps < 1 ? 1 : tps);
  return a >= 1.0 ? 1.0 : a;
}
/// Appends one posed model. `layers[slot]` is the texture layer per face slot. Vertices are
/// encoded in 1/16 block from the entity batch origin (the GAME-H1 contract).
void appendModel(const ItemDropModel& m, const uint16_t layers[6], const DropPose& p, float ox, float oy, float oz,
                 std::vector<litholattice::PackedVertex>& out);

// ---- P-6 save record ----
struct SavedItem {
  uint64_t id = 0;
  double x = 0, y = 0, z = 0, vx = 0, vy = 0, vz = 0;
  uint32_t ageTicks = 0, count = 0, delayTicks = 0;
  bool onGround = false;
  std::string item;  ///< stable string id
  bool hasData = false;
  std::string name;
  std::vector<std::string> certificates;
};
inline constexpr uint32_t kItemsMagic = 0x5449444Cu;  // "LDIT"
inline constexpr uint16_t kItemsVersion = 1;
/// The items file body (magic, CRC, versioned body), items in id order.
litholattice::storage::Bytes encodeItems(std::vector<SavedItem> items);
bool decodeItems(const litholattice::storage::Bytes& file, std::vector<SavedItem>* out, std::string* err);

}  // namespace lithokraft::itemdrop
