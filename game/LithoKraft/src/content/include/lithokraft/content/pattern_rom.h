// Pattern ROM screen player (playtest 2026-10-16, JD; frames by CloudAtlas).
// A Pattern ROM holds a looping animation (frames of palette-lamp strengths plus a per-frame
// length in SIM TICKS) and plays it on a vertical screen of colour lamps. It runs inside the
// sim's tick (cSignalWorldSim::tick), so it advances once per world tick: at --tps 512 the
// loop runs 16x faster than at 32, and pausing the sim freezes it. It only rewrites cells that are
// still colour lamps (a broken / replaced pixel is skipped), and only on a frame change.
// Visual demo device, not a signal circuit: it writes lamp states directly, like the sim's own
// lamp update, and nothing reads them. Worlds without a ROM (every hash / selftest scenario) never
// touch this path.
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
#include "lithokraft/engine.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/sim/world_simulation.h"

namespace lithokraft {

struct PatternRom {
  int w = 0, h = 0;                              ///< source pixels
  std::vector<std::vector<uint8_t>> frames;      ///< w*h lamp strengths per frame, row 0 = TOP
  std::vector<int> ticks;                        ///< frame length in sim ticks (>= 1)
};

/// A screen showing a ROM: pixel (i, r) of the source covers scale x scale lamps; the screen
/// stands in the plane z = z, x from x0 rightward, y from y0 (bottom row) upward.
struct PatternRomScreen {
  PatternRom rom;
  int x0 = 0, y0 = 0, z = 0, scale = 1;
  // player state
  uint64_t tick = 0; int frame = -1;
};

/// CloudAtlas's 16x16 smiley loop (smile, blink, smile, wink, grin), lamp strengths for a lamp of
/// palette size `tier` (16 / 32 / 64). Yellow = the palette's yellow (vga64 index 60 #FFFF00 ->
/// strength 61; cga16 / vga32 index 14 #FFFF55 -> strength 15), outline = index 0 black (strength 1).
/// Frame lengths at the 32 TPS base: 600/150/600/400/700 ms -> 19/5/19/13/22 ticks.
PatternRom smileyRom(int tier);
/// Palette yellow as a lamp strength for a colour lamp of palette size `tier`.
int paletteYellowStrength(int tier);

/// Advance one sim tick; on a frame change rewrites the screen's lamps (dirty-marking them).
void tickPatternRom(ChunkMap& w, PatternRomScreen& s, const litholattice::IWorldSimulation::DirtyFn& dirty);
/// Paint frame `f` now (scene build, before the sim starts). Returns lamps written.
int paintPatternRom(ChunkMap& w, const PatternRomScreen& s, int f);

}  // namespace lithokraft
