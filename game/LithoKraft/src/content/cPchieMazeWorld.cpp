// PCHIE P7 in-engine maze: see cPchieMazeWorld.h. 2026-11-10 (JD t5033u). Generic blocks only.
#include "lithokraft/content/cPchieMazeWorld.h"
#include <sstream>
#include <vector>
#include "lithokraft/content/blocks.h"

namespace lithokraft {

void cPchieMazeWorld::put(int x, int y, int z, BlockId b) {
  if (w_->set(x, y, z, b)) { w_->setState(x, y, z, 0); ++placed_; }
}

cPchieMazeWorld::Layout cPchieMazeWorld::build(ChunkMap& world, const std::string& text, int sx, int floorY, int sz) {
  Layout L;
  w_ = &world;
  placed_ = 0;
  std::istringstream in(text);
  std::string magic;
  long long seed = 0;
  in >> magic >> L.width >> L.height >> seed;
  if (magic != "PCHIE-B1" || L.width < 3 || L.height < 3 || L.width > 256 || L.height > 256) {
    L.err = "bad header (want 'PCHIE-B1 <w> <h> <seed>', 3..256)";
    return L;
  }
  std::vector<std::string> rows;
  std::string line;
  std::getline(in, line);  // rest of the header line
  while (int(rows.size()) < L.height && std::getline(in, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (line.empty()) continue;
    if (int(line.size()) != L.width) { L.err = "row " + std::to_string(rows.size()) + " has wrong width"; return L; }
    for (char c : line)
      if (c != '#' && c != '.' && c != 'S' && c != 'K' && c != 'D' && c != 'E' && c != 'P') {
        L.err = std::string("unknown cell char '") + c + "'";
        return L;
      }
    rows.push_back(line);
  }
  if (int(rows.size()) != L.height) { L.err = "too few rows"; return L; }
  int starts = 0;
  for (int y = 0; y < L.height; ++y)
    for (int x = 0; x < L.width; ++x)
      if (rows[size_t(y)][size_t(x)] == 'S') { L.startX = x; L.startY = y; ++starts; }
  if (starts != 1) { L.err = "need exactly one Start cell"; return L; }
  L.originX = sx - L.startX;
  L.originZ = sz - L.startY;
  L.floorY = floorY;
  // Clear a 2-block apron around the maze (floor kept, air above) so nothing on the plain intrudes.
  for (int z = L.originZ - 2; z < L.originZ + L.height + 2; ++z)
    for (int x = L.originX - 2; x < L.originX + L.width + 2; ++x) {
      put(x, floorY, z, kAluminumBlock);
      for (int y = floorY + 1; y <= floorY + 6; ++y) world.set(x, y, z, kAir);
    }
  for (int y = 0; y < L.height; ++y)
    for (int x = 0; x < L.width; ++x) {
      const int bx = L.originX + x, bz = L.originZ + y;
      switch (rows[size_t(y)][size_t(x)]) {
        case '#': put(bx, floorY + 1, bz, kStone); put(bx, floorY + 2, bz, kStone); break;
        case 'S': put(bx, floorY, bz, kCopperBlock); break;
        case 'E': put(bx, floorY, bz, kLamp); break;
        case 'K': put(bx, floorY + 1, bz, kLamp); break;
        case 'D': put(bx, floorY + 1, bz, kFrostedGlass); put(bx, floorY + 2, bz, kFrostedGlass); break;
        case 'P': world.set(bx, floorY, bz, kAir); world.set(bx, floorY - 1, bz, kAir); break;
        default: break;  // '.' floor
      }
    }
  L.spawnX = float(sx) + 0.5f;
  L.spawnZ = float(sz) + 0.5f;
  L.placed = placed_;
  L.ok = true;
  return L;
}

}  // namespace lithokraft
