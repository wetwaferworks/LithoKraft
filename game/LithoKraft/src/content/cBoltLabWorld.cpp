// Bolt PCHIE Lab: see cBoltLabWorld.h. 2026-10-30 (JD). Generic blocks only.
#include "lithokraft/content/cBoltLabWorld.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "lithokraft/content/blocks.h"

namespace lithokraft {

const char* cBoltLabWorld::glyph(char c) {
  // 3x5 block font, top row first ('#' = block). Digits, A-Z and space.
  static const char* const digits[10] = {"####.##.##.####", ".#.##..#..#.###", "###..#####..###", "###..####..####", "#.##.####..#..#",
                                         "####..###..####", "####..####.####", "###..#..#..#..#", "####.#####.####", "####.####..####"};
  static const char* const letters[26] = {
      "####.#####.##.#", "##.#.###.#.###.", "####..#..#..###", "##.#.##.##.###.", "####..##.#..###", "####..##.#..#..",
      "####..#.##.####", "#.##.#####.##.#", "###.#..#..#.###", "..#..#..##.####", "#.##.###.#.##.#", "#..#..#..#..###",
      "#.#####.##.##.#", "##.#.##.##.##.#", "####.##.##.####", "####.####..#..", "####.##.####..#", "####.###.#.##.#",
      "####..###..####", "###.#..#..#..#.", "#.##.##.##.####", "#.##.##.##.#.#.", "#.##.##.######.#", "#.##.#.#.#.##.#",
      "#.##.#.#..#..#.", "###..#.#.#..###"};
  if (c >= '0' && c <= '9') return digits[c - '0'];
  if (c >= 'A' && c <= 'Z') return letters[c - 'A'];
  return "...............";
}

void cBoltLabWorld::put(int x, int y, int z, BlockId b) {
  if (w_->set(x, y, z, b)) { w_->setState(x, y, z, 0); ++placed_; }
}

void cBoltLabWorld::pad(int x0, int z0, int size, BlockId floor) {
  for (int z = z0; z < z0 + size; ++z)
    for (int x = x0; x < x0 + size; ++x) {
      put(x, fy_, z, floor);
      for (int y = fy_ + 1; y <= fy_ + 6; ++y) w_->set(x, y, z, kAir);
    }
}

void cBoltLabWorld::label(int x, int z, const std::string& text, BlockId b) {
  // Flat on the floor, reads left to right for a player facing +z (x decreases to the right).
  int lx = x;
  for (char ch : text) {
    const char* g = glyph(ch);
    const size_t n = std::strlen(g);
    for (int r = 0; r < 5; ++r)
      for (int col = 0; col < 3; ++col)
        if (size_t(r * 3 + col) < n && g[r * 3 + col] == '#') put(lx - col, fy_, z + (4 - r), b);
    lx -= 4;
  }
}

void cBoltLabWorld::pedestal(int x, int z, BlockId top) {
  put(x, fy_ + 1, z, kAluminumBlock);
  put(x, fy_ + 2, z, top);
}

void cBoltLabWorld::buildMaze(int x0, int z0) {
  // B1: a 15 x 15 cell maze (31 x 31 blocks, Stone walls 2 high), carved by a fixed-seed depth-first walk,
  // so it is identical every launch. Start (Copper) south-west, goal (Lamp) north-east. Two keys (Switch
  // on a pedestal) in dead ends; two doors (Frosted Glass, 2 high) across the solution corridor, each with
  // a Red Trace run from its key toward the door frame so players can see which key opens which door.
  constexpr int N = 15, W = 2 * N + 1;
  std::vector<uint8_t> open(size_t(W * W), 0);
  std::vector<int> stack{0};
  std::vector<uint8_t> seen(size_t(N * N), 0);
  seen[0] = 1;
  uint32_t rng = 0xB017u;
  auto next = [&rng]() { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
  open[size_t(1 * W + 1)] = 1;
  while (!stack.empty()) {
    const int c = stack.back(), cx = c % N, cz = c / N;
    int opts[4], k = 0;
    const int dx[4] = {1, -1, 0, 0}, dz[4] = {0, 0, 1, -1};
    for (int d = 0; d < 4; ++d) {
      const int nx = cx + dx[d], nz = cz + dz[d];
      if (nx >= 0 && nz >= 0 && nx < N && nz < N && !seen[size_t(nz * N + nx)]) opts[k++] = d;
    }
    if (!k) { stack.pop_back(); continue; }
    const int d = opts[next() % uint32_t(k)], nx = cx + dx[d], nz = cz + dz[d];
    seen[size_t(nz * N + nx)] = 1;
    open[size_t((2 * cz + 1 + dz[d]) * W + 2 * cx + 1 + dx[d])] = 1;
    open[size_t((2 * nz + 1) * W + 2 * nx + 1)] = 1;
    stack.push_back(nz * N + nx);
  }
  const int mx = x0 + 1, mz = z0 + 1;
  for (int z = 0; z < W; ++z)
    for (int x = 0; x < W; ++x)
      if (!open[size_t(z * W + x)]) { put(mx + x, fy_ + 1, mz + z, kStone); put(mx + x, fy_ + 2, mz + z, kStone); }
  // entrance south of the start cell, exit north of the goal cell
  w_->set(mx + 1, fy_ + 1, mz, kAir); w_->set(mx + 1, fy_ + 2, mz, kAir);
  w_->set(mx + W - 2, fy_ + 1, mz + W - 1, kAir); w_->set(mx + W - 2, fy_ + 2, mz + W - 1, kAir);
  put(mx + 1, fy_, mz + 1, kCopperBlock);                 // start
  put(mx + W - 2, fy_ + 1, mz + W - 2, kLamp);            // goal
  // Solution path (BFS) so doors sit on it and keys sit off it.
  std::vector<int> prev(size_t(W * W), -1);
  std::vector<int> q{1 * W + 1};
  prev[size_t(1 * W + 1)] = 1 * W + 1;
  for (size_t i = 0; i < q.size(); ++i) {
    const int c = q[i], cx = c % W, cz = c / W;
    const int nb[4] = {c + 1, c - 1, c + W, c - W};
    for (int j = 0; j < 4; ++j) {
      const int n = nb[j], nx = n % W, nz = n / W;
      if (nx < 0 || nz < 0 || nx >= W || nz >= W || std::abs(nx - cx) + std::abs(nz - cz) != 1) continue;
      if (open[size_t(n)] && prev[size_t(n)] < 0) { prev[size_t(n)] = c; q.push_back(n); }
    }
  }
  std::vector<int> path;
  for (int c = (W - 2) * W + (W - 2); c != 1 * W + 1 && c >= 0; c = prev[size_t(c)]) path.push_back(c);
  std::vector<uint8_t> onPath(size_t(W * W), 0);
  for (int c : path) onPath[size_t(c)] = 1;
  // dead ends off the path, for the keys
  std::vector<int> ends;
  for (int c = 0; c < W * W; ++c) {
    const int x = c % W, z = c / W;
    if (!open[size_t(c)] || onPath[size_t(c)] || (x % 2) == 0 || (z % 2) == 0) continue;
    int walls = 0;
    walls += !open[size_t(c + 1)]; walls += !open[size_t(c - 1)]; walls += !open[size_t(c + W)]; walls += !open[size_t(c - W)];
    if (walls == 3) ends.push_back(c);
  }
  const size_t L = path.size();
  for (int k = 0; k < 2 && L > 6; ++k) {
    const int door = path[L - 1 - (L * size_t(k + 1)) / 3];  // one third and two thirds along from the start
    put(mx + door % W, fy_ + 1, mz + door / W, kFrostedGlass);
    put(mx + door % W, fy_ + 2, mz + door / W, kFrostedGlass);
    if (!ends.empty()) {
      const int key = ends[(ends.size() * size_t(k + 1)) / 3];
      pedestal(mx + key % W, mz + key / W, kSwitch);
    }
  }
  // B6 injection signs: three marker posts with a Keyboard "sign face" at corridor corners.
  int signs = 0;
  for (size_t i = L / 5; i < L && signs < 3; i += L / 4 + 1) {
    const int c = path[i];
    put(mx + c % W, fy_ + 3, mz + c / W, kKeyboard);
    ++signs;
  }
}

void cBoltLabWorld::buildCourt(int x0, int z0) {
  // B2: three recording plinths (Structure Blocks) labelled R (real), F (flipped), X (forged), a reader
  // bench (Chipor Viewport) facing them, and the judge's Certificate Table.
  const char* tags[3] = {"R", "F", "X"};
  for (int i = 0; i < 3; ++i) {
    const int x = x0 + 26 - 10 * i, z = z0 + 18;
    pedestal(x, z, kStructureBlock);
    label(x + 1, z - 7, tags[i], kCeramic);
  }
  for (int x = x0 + 8; x <= x0 + 26; ++x) put(x, fy_ + 1, z0 + 26, kAluminumBlock);
  put(x0 + 17, fy_ + 2, z0 + 26, kChiporViewport);
  put(x0 + 17, fy_ + 1, z0 + 29, kCertificateTable);
}

void cBoltLabWorld::buildBuildOff(int x0, int z0) {
  // B3: an empty 12 x 12 build plot (Aluminum rim) and, beside it, a negative-control build: Switch ->
  // Red Trace -> Lamp with one trace cell missing, so it must never light.
  for (int i = 0; i < 12; ++i) {
    put(x0 + 4 + i, fy_, z0 + 8, kAluminumBlock); put(x0 + 4 + i, fy_, z0 + 19, kAluminumBlock);
    put(x0 + 4, fy_, z0 + 8 + i, kAluminumBlock); put(x0 + 15, fy_, z0 + 8 + i, kAluminumBlock);
  }
  const int nz = z0 + 24;
  put(x0 + 20, fy_ + 1, nz, kSwitch);
  for (int x = x0 + 21; x <= x0 + 29; ++x)
    if (x != x0 + 25) put(x, fy_ + 1, nz, kRedTrace);
  put(x0 + 30, fy_ + 1, nz, kLamp);
  label(x0 + 29, nz + 2, "NEG", kCeramic);
}

void cBoltLabWorld::buildGauntlet(int x0, int z0) {
  // B4: five identical columns (CPU, CUDA and thread counts): each a Switch -> 4 Red Trace -> Lamp lane.
  for (int i = 0; i < 5; ++i) {
    const int x = x0 + 6 + 5 * i;
    put(x, fy_ + 1, z0 + 10, kSwitch);
    for (int z = z0 + 11; z <= z0 + 14; ++z) put(x, fy_ + 1, z, kRedTrace);
    put(x, fy_ + 1, z0 + 15, kLamp);
  }
}

void cBoltLabWorld::buildGeneric(int x0, int z0, int variant) {
  // Staged station: a centre pedestal (block varies so stations are told apart) and a 2-block-high Glass
  // viewing rail along the north edge.
  static const BlockId tops[6] = {kStructureBlock, kMemor, kPulsor, kCatchor, kRedirector, kRepeator};
  pedestal(x0 + 17, z0 + 17, tops[variant % 6]);
  for (int x = x0 + 4; x <= x0 + 30; ++x) put(x, fy_ + 1, z0 + 30, kGlass);
}

void cBoltLabWorld::buildHudCorner(int x0, int z0) {
  // Reserved for the planned thought HUD: an outlined Glass frame, nothing inside (no HUD code yet).
  for (int i = 0; i < 26; ++i) {
    put(x0 + 4 + i, fy_ + 1, z0 + 8, kGlass); put(x0 + 4 + i, fy_ + 1, z0 + 33, kGlass);
    put(x0 + 4, fy_ + 1, z0 + 8 + i, kGlass); put(x0 + 29, fy_ + 1, z0 + 8 + i, kGlass);
  }
}

cBoltLabWorld::Layout cBoltLabWorld::build(ChunkMap& world, int sx, int floorY, int sz) {
  w_ = &world; fy_ = floorY; placed_ = 0;
  struct Def { const char* id; const char* name; };
  static const Def defs[] = {
      {"B1", "MAZE"}, {"B2", "COURT"}, {"B3", "BUILD"}, {"B4", "GAUNT"}, {"B5", "FREEZE"},
      {"B6", "SIGNS"}, {"B7", "BUDGET"}, {"B8", "SAFETY"}, {"B9", "MIRROR"}, {"B10", "SOAK"},
      {"A1", "REPLAY"}, {"A2", "FORK"}, {"A3", "REWIND"}, {"A4", "RECEIPT"}, {"A5", "THREADS"},
      {"A6", "HONEST"}, {"A7", "FAIR"}, {"A8", "RANKED"}, {"A9", "RECOVER"}, {"R0", "FREE"}, {"HUD", "HUD"}};
  Layout out;
  out.floorY = floorY;
  const int gx0 = sx - (kCols * kSlot) / 2, gz0 = sz + 6;
  int i = 0, generic = 0;
  for (const Def& d : defs) {
    const int x0 = gx0 + (i % kCols) * kSlot, z0 = gz0 + (i / kCols) * kSlot;
    pad(x0, z0, kPad, std::strcmp(d.id, "R0") == 0 ? kCopperBlock : kWaferSilicon);
    const std::string tag = std::string(d.id) + " " + d.name;
    label(x0 + kPad - 2, z0 + 1, tag.size() > 8 ? std::string(d.id) : tag, kCeramic);
    const std::string id = d.id;
    if (id == "B1") buildMaze(x0, z0);
    else if (id == "B2") buildCourt(x0, z0);
    else if (id == "B3") buildBuildOff(x0, z0);
    else if (id == "B4") buildGauntlet(x0, z0);
    else if (id == "B6") buildGeneric(x0, z0, 3);  // signs themselves stand inside the B1 maze
    else if (id == "HUD") buildHudCorner(x0, z0);
    else if (id != "R0") buildGeneric(x0, z0, generic++);
    Station s;
    s.id = id; s.name = d.name; s.x0 = x0; s.z0 = z0; s.x1 = x0 + kPad - 1; s.z1 = z0 + kPad - 1;
    s.spawnX = float(x0 + kPad / 2) + 0.5f; s.spawnZ = float(z0 - 2) + 0.5f;
    out.stations.push_back(s);
    ++i;
  }
  out.spawnX = float(sx) + 0.5f; out.spawnZ = float(gz0 - 4) + 0.5f;
  out.placed = placed_;
  return out;
}

}  // namespace lithokraft