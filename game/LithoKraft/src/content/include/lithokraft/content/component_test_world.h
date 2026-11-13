// Component Test world (2026-11-12, JD t5295u / t5296u): --component-test, "Component Test" in the
// Play world picker. A dead-flat Wafer Silicon plain (dimension "componenttest",
// assets/worldgen/componenttest.json: no procedural generation anywhere, far chunks included) with
// one 3 x 3 chunk (48 x 48) pad per Trace component, on a chunk-aligned grid with a 1-chunk
// (16-block) aisle between pads. Each pad has its component standing on the silicone at the pad
// centre and its name, in the test world's 3x5 block font (block_font.h), set flush in the floor
// in Ceramic along a square spiral around the component.
//
// Which components: the creative palette's Trace and Lamps/LEDs families (cBlockFamilies), palette-
// hidden placement variants skipped (the side Vias), registry display names uppercased -- so a new
// Trace block joins the world when it is registered. Then JD's approved B9 names that are not in
// the registry yet (Watchor, Bitor, Pushor, Swivelor, Resin): their pads and spirals are built,
// the centre stays empty until the id exists (Resin is an item, never a block). No Mortor pad (no
// concept picked) and no Via elbow pad (elbows are auto-built at a junction, not placed).
//
// Spiral: glyphs run counter-clockwise seen from above (north band right-to-left in +x terms,
// then west, south, east), each band one step further out, glyph tops toward the component. That
// is the orientation that reads upright, left to right, for a player standing OUTSIDE the band on
// the aisle (the spawn looks +Z at the north bands); clockwise with tops inward would mirror the
// text. The lap pitch is the widest (7..28 rows) that fits the name inside the pad.
#pragma once
#include <string>
#include <utility>
#include <vector>
#include "lithokraft/engine.h"
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

class cComponentTestWorld final {
 public:
  static constexpr int kPadChunks = 3;                                   ///< pad = 3 x 3 chunks
  static constexpr int kAisleChunks = 1;                                 ///< aisle between pads
  static constexpr int kPad = kPadChunks * kChunkSize;                   ///< 48 blocks
  static constexpr int kPitch = (kPadChunks + kAisleChunks) * kChunkSize;  ///< 64 blocks
  static constexpr int kCols = 7;                                        ///< pads per grid row
  static constexpr int kOriginChunkX = 1, kOriginChunkZ = 2;             ///< first pad's NW chunk
  static constexpr int kCentre = kPad / 2;                               ///< component at x0 + 24, z0 + 24
  static constexpr int kSpiralStart = 3;   ///< first band's inner row: 2 clear rings round the component
  static constexpr int kSpiralLimit = 21;  ///< glyph cells within +-21 of the centre (>= 2 silicone border)

  struct Pad {
    std::string id;        ///< named spawn / tp key ("dialor", "red-via")
    std::string label;     ///< the floor text ("DIALOR", "RED VIA")
    std::string engineId;  ///< "lithokraft:dialor" (approved id when not registered yet)
    BlockId block = kAir;  ///< the component at the centre; kAir = name-only pad
    bool approved = false; ///< one of JD's approved B9 names (registered or not)
    bool itemOnly = false; ///< Resin: an item, never placed as a block
    int x0 = 0, z0 = 0;    ///< NW corner block (chunk aligned)
    int cx = 0, cz = 0;    ///< centre cell (the component stands at cx, floorY + 1, cz)
    float spawnX = 0, spawnZ = 0;  ///< named spawn: on the aisle 4 blocks north of the pad, mid-width
  };
  struct Glyph { char ch = ' '; int side = 0; std::vector<std::pair<int, int>> cells; };  ///< (dx, dz) from the centre
  struct Spiral { bool fits = false; int pitch = 0; std::vector<Glyph> glyphs; };
  struct Layout {
    int floorY = 0;
    float spawnX = 0, spawnZ = 0, spawnYaw = 0;
    std::vector<Pad> pads;
    int placed = 0, labelCells = 0, components = 0;
    std::vector<std::string> unfit;  ///< labels that did not fit (should be empty; the selftest checks)
  };

  /// Every pad, in grid order (pure: registry + constants only).
  static std::vector<Pad> pads();
  /// Catalog spawns: "spawn" first, then one per pad (Pad::id), grid order.
  static std::vector<std::string> spawnIds();
  /// The spiral for one label, cells relative to the pad centre (pure).
  static Spiral spiral(const std::string& label);
  /// The world spawn: the north aisle, mid-grid, looking +Z at the first pad row.
  static void spawn(float& x, float& z, float& yaw);
  /// Chunk rect [cx0, cx1) x [cz0, cz1) covering every pad plus a one-chunk aisle all round.
  static void gridChunks(int& cx0, int& cz0, int& cx1, int& cz1);
  /// The smallest worldChunks extent with no pad clipped (the grid rect's far edge).
  static int minWorldChunks();

  /// Build every pad on the plain whose top block is at floorY. Columns must exist (gridChunks).
  Layout build(ChunkMap& world, int floorY) const;
};

}  // namespace lithokraft
