#pragma once
// 2026-10-31 (JD): F3 chunk-coordinate line: chunk (cx, cz) and local x/y/z of the player's block and the
// looked-at block, flagging a local x or z on a chunk seam (0 or 15). Live repro aid for the Trace seam fix.
#include <cstdio>
#include <string>

namespace lithokraft {

class cChunkCoordText {
 public:
  /// One block: "c 1,6 l 0,10,15 [x0 z15]" (y local within its 16-tall sub-chunk; flags only on seams).
  static std::string block(int x, int y, int z) {
    const int cx = x >> 4, cz = z >> 4, lx = x & 15, ly = y & 15, lz = z & 15;
    char b[96];
    std::snprintf(b, sizeof b, "c %d,%d l %d,%d,%d", cx, cz, lx, ly, lz);
    std::string s(b);
    std::string f;
    if (lx == 0 || lx == 15) f += lx == 0 ? "x0" : "x15";
    if (lz == 0 || lz == 15) f += std::string(f.empty() ? "" : " ") + (lz == 0 ? "z0" : "z15");
    if (!f.empty()) s += " [" + f + "]";
    return s;
  }
  /// Whole line: "chunk  player <block>  |  look <block or ->".
  static std::string line(int px, int py, int pz, bool hit, int hx, int hy, int hz) {
    return "chunk  player " + block(px, py, pz) + "  |  look " + (hit ? block(hx, hy, hz) : std::string("-"));
  }
};

}  // namespace lithokraft
