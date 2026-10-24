// Playtest test world (2026-10-16, JD's 5 PM playtest): --test-world / --bench-scene testworld.
// A flat, level Wafer pad around the spawn with EVERY block in the registry (enumerated at run
// time, ids 1 .. size-1, the same list the E palette shows) standing in labeled rows, plus a
// Pattern ROM demo screen (CloudAtlas's smiley loop) behind them.
#pragma once
#include <string>
#include <vector>
#include "lithokraft/content/pattern_rom.h"
#include "lithokraft/engine.h"
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

struct TestWorldRow { std::string label; std::vector<BlockId> blocks; int z = 0; };
struct TestWorldLayout {
  int leafDemoX = 0, leafDemoZ = 0;  ///< 2026-10-16: right-most leaf clump start x, clump z
  int floorY = 0;                    ///< y of the pad's top block (stand at floorY + 1)
  float spawnX = 0, spawnZ = 0, spawnYaw = 0;
  std::vector<TestWorldRow> rows;    ///< every registry block, grouped
  std::vector<BlockId> missing;      ///< registry blocks that could not be placed (should be empty)
  PatternRomScreen screen;           ///< the demo screen (add to the sim with addPatternRom)
  int placed = 0;
};

/// The test world's starting inventory: up to n blocks, components first (wire, IO, logic,
/// lamps, machines in registry order), then Wafer Silicon, Glass, Ceramic, then the rest.
std::vector<BlockId> testWorldKit(int n);

/// Build the test world around (sx, sz) with its pad top at floorY. screenSize 32 (2x) or 64 (4x).
TestWorldLayout buildTestWorld(ChunkMap& world, int sx, int floorY, int sz, int screenSize);

/// 2026-10-24 (JD, playtest builds only): the Examples section. Every historical bench scene,
/// built by its own scene code, on flat labeled pads in a grid north (+Z) of the test world.
/// worldBlocks = loaded square [0, worldBlocks); a slot that does not fit is skipped, not clipped.
struct TestWorldExample { std::string label; int x0 = 0, z0 = 0, x1 = 0, z1 = 0, blocks = 0; bool placed = false; std::string note; };
std::vector<TestWorldExample> buildTestWorldExamples(ChunkMap& world, const TestWorldLayout& tw, int sx, int sz, int worldBlocks);

}  // namespace lithokraft
