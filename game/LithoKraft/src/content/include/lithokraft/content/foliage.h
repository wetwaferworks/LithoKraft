// Cortana 2026-10-15 2010 [GAME]: 3D voxel foliage, placed as INSTANCES of shared models.
//
// MODELS: each foliage kind is authored once, deterministically, as level-4 (16^3 per block)
// cSparseOctree micro-cells in lithokraft::model::Model blocks stacked in y (a cattail is two
// blocks tall). The same models ship as .lthm assets in assets/models/foliage/<name>_<i>.lthm
// (biome_capture --foliage regenerates them); at first use the library loads those assets
// and falls back to the generator when they are missing or invalid.
//
// INSTANCING: one shared model + one shared pre-meshed quad list per kind (built once,
// thread-safe magic static, read-only afterwards). An instance is just the world cell: the
// foliage block id selects the model, and the variant (a quarter-turn rotation) is derived
// from the cell's world position, so an instance stores nothing beyond the 4-byte cell
// every block already has. The chunk mesher expands the shared quads at mesh time (this
// engine has no GPU instancing; chunk meshes carry the expanded quads like any block).
//
// Render-side + deterministic worldgen only: foliage blocks have no sim role, never tick.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "model.h"

namespace lithokraft::foliage {

enum class Kind : uint8_t { Cattail = 0, Bush, Fern, Reeds, CrystalShrub, Moss };  // Moss: 2110 overnight
constexpr int kKindCount = 6;

/// Foliage material ids (micro-cell materials; textures "foliage_m<id>").
namespace mat {
inline constexpr uint8_t kStem = 32, kCattailHead = 33, kLeaf = 34, kFern = 35, kReed = 36,
                         kCrystal = 37, kCrystalCore = 38, kBerry = 39, kBark = 40,
                         kGlow = 41, kDark = 42;  ///< Tron pass: glow edge/trace lines, dark die body
}
constexpr uint8_t kFirstMaterial = 32, kLastMaterial = 42;

const char* kindName(Kind k);  ///< "cattail", "bush", "fern", "reeds", "crystal_shrub"
/// Blocks tall (models taller than 1 overhang into the air above their cell).
int kindHeight(Kind k);

/// Deterministic authoring: the same call always yields identical leaf streams.
std::vector<model::Model> buildModel(Kind k);

/// One shared micro-face of a model, in 1/16-block units relative to the instance cell's
/// min corner (y may exceed 16 for tall models). Emitted as face `face` (0 +X .. 5 -Z) of
/// the box [lo, hi).
struct Quad {
  int16_t lo[3], hi[3];
  uint8_t face, material;
};
struct Mesh {
  std::vector<Quad> quads;
  size_t octreeBytes = 0;  ///< shared model memory (cSparseOctree::memoryBytes)
  size_t leafCells = 0;
  bool fromAsset = false;  ///< loaded from the shipped .lthm (else generated)
  size_t rawQuads = 0;     ///< Cortana 2026-10-15 2310: quad count before the coplanar merge
};
/// The shared mesh for kind k (built on first use from the assets or the generator).
const Mesh& mesh(Kind k);
/// Cortana 2026-10-16 [GAME] (JD): far LOD (beyond kLodDistanceBlocks) - the cross-hatch variant
/// of kind k: two crossed zero-thickness planes (x = 8, z = 8; both facings) carrying the model's
/// silhouette. Shared, built once from mesh(k). Render-only: never touches world data.
const Mesh& crossMesh(Kind k);
Mesh crossFromMesh(const Mesh& detail, int heightBlocks);
inline constexpr float kLodDistanceBlocks = 32.f;
/// Asset path of block i of kind k under `dir`.
std::string assetPath(const std::string& dir, Kind k, int i);
/// Mesh a model stack into shared quads (LOD 4 = the 1/16 vertex grid).
Mesh meshModels(const std::vector<model::Model>& blocks);

/// Instance variant: quarter turns (0..3) from the world position (deterministic).
inline int variantAt(int wx, int wy, int wz) {
  uint32_t h = uint32_t(wx) * 73856093u ^ uint32_t(wy) * 19349663u ^ uint32_t(wz) * 83492791u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
  return int(h & 3u);
}
/// Cortana 2026-10-15 2215 [GAME]: Repeator placeholder model (CloudAtlas will replace it).
/// A low slab (5/16) facing N (front = -Z) with 4 top buttons; `activeIndex` 0..3 (2/4/8/16
/// ticks) is raised and glowing. Button quadrants in authored (facing-N) coords:
/// 0 front-left (x<8, z<8), 1 front-right, 2 back-right, 3 back-left.
/// Swappable: assets/models/repeator/repeator_<i>.lthm (one per active button) override the
/// placeholder when present. Shared, built once; the facing is applied with rotated(q, facing).
const Mesh& repeatorMesh(int activeIndex);
std::vector<model::Model> buildRepeatorModel(int activeIndex);
/// Authored-quadrant button index for a top-face point (lx, lz in 0..1 cell-local) of a
/// Repeator facing `facing` (inverse of the mesh rotation).
int repeatorButtonAt(double lx, double lz, int facing);
/// Rotate a shared quad by `turns` quarter turns about the cell's vertical axis.
Quad rotated(const Quad& q, int turns);

}  // namespace lithokraft::foliage