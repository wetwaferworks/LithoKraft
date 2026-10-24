// Pattern ROM screen player: see pattern_rom.h. Playtest 2026-10-16 (JD), frames by CloudAtlas.
#include "lithokraft/content/pattern_rom.h"
#include "lithokraft/content/blocks.h"
#include "litholattice/world/block.h"
#include <cstdio>
#include <cstdlib>

namespace lithokraft {

int paletteYellowStrength(int tier) { return tier >= 64 ? 61 : 15; }

PatternRom smileyRom(int tier) {
  static const char* const kFrames[5][16] = {
#include "smiley_rom_frames.inc"
  };
  PatternRom r;
  r.w = r.h = 16;
  r.ticks = {19, 5, 19, 13, 22};
  const uint8_t yellow = uint8_t(paletteYellowStrength(tier)), outline = 1;
  for (const auto& f : kFrames) {
    std::vector<uint8_t> px(256, 0);
    for (int y = 0; y < 16; ++y)
      for (int x = 0; x < 16; ++x) {
        const char c = f[y][x];
        px[size_t(y * 16 + x)] = c == 'Y' ? yellow : c == 'o' ? outline : 0;
      }
    r.frames.push_back(std::move(px));
  }
  return r;
}

namespace {
template <class Fn>
int writeFrame(ChunkMap& w, const PatternRomScreen& s, int f, int prev, Fn&& dirty) {
  const auto& reg = blockRegistry();
  const auto& cur = s.rom.frames[size_t(f)];
  const std::vector<uint8_t>* old = prev >= 0 ? &s.rom.frames[size_t(prev)] : nullptr;
  int n = 0;
  for (int r = 0; r < s.rom.h; ++r)
    for (int i = 0; i < s.rom.w; ++i) {
      const size_t p = size_t(r * s.rom.w + i);
      if (old && (*old)[p] == cur[p]) continue;
      const int v = cur[p];
      const CellState ns = makeState(v, 0, 0, v > 0);
      const int yTop = s.y0 + s.rom.h * s.scale - 1 - r * s.scale;  // row 0 is the top
      for (int dy = 0; dy < s.scale; ++dy)
        for (int dx = 0; dx < s.scale; ++dx) {
          const int x = s.x0 + i * s.scale + dx, y = yTop - dy;
          if (reg.def(w.get(x, y, s.z)).role != uint16_t(SimRole::ColorLamp)) continue;  // broken pixel
          if (w.getState(x, y, s.z) == ns) continue;
          w.setState(x, y, s.z, ns);
          dirty(x, y, s.z);
          ++n;
        }
    }
  return n;
}
}  // namespace

void tickPatternRom(ChunkMap& w, PatternRomScreen& s, const litholattice::IWorldSim::DirtyFn& dirty) {
  if (s.rom.frames.empty()) return;
  uint64_t loop = 0;
  for (int t : s.rom.ticks) loop += uint64_t(t < 1 ? 1 : t);
  uint64_t at = s.tick % loop;
  int f = 0;
  while (true) {
    const uint64_t len = uint64_t(s.rom.ticks[size_t(f)] < 1 ? 1 : s.rom.ticks[size_t(f)]);
    if (at < len) break;
    at -= len; ++f;
  }
  ++s.tick;
  // Every tick, full compare (only differing cells are written / dirtied): the signal sim's own
  // lamp pass (scan, neighbour updates) may zero an unfed lamp, so the ROM re-asserts its pixels
  // instead of trusting the previous frame. 1024-4096 state reads per tick.
  const int nw = writeFrame(w, s, f, -1, dirty);
  if (std::getenv("LL_ROM_DEBUG") && (s.tick < 4 || s.tick % 256 == 0)) {
    const int x = s.x0 + 7 * s.scale, y = s.y0 + s.rom.h * s.scale - 1 - 7 * s.scale;
    std::fprintf(stderr, "ROMDBG tick %llu frame %d wrote %d; cell(%d,%d,%d) block %u state 0x%04x\n",
                 (unsigned long long)s.tick, f, nw, x, y, s.z, unsigned(w.get(x, y, s.z)), unsigned(w.getState(x, y, s.z)));
  }
  s.frame = f;
}

int paintPatternRom(ChunkMap& w, const PatternRomScreen& s, int f) {
  return writeFrame(w, s, f, -1, [](int, int, int) {});
}

}  // namespace lithokraft
