// Component Test world: see component_test_world.h. 2026-11-12 (JD t5295u / t5296u).
#include "lithokraft/content/component_test_world.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include "lithokraft/content/block_families.h"
#include "lithokraft/content/block_font.h"
#include "lithokraft/content/blocks.h"
#include "litholattice/world/block.h"

namespace lithokraft {

namespace {

std::string upper(std::string_view s) {
  std::string o;
  for (char c : s) o += char(std::toupper(static_cast<unsigned char>(c)));
  return o;
}

// "Signal Transmittor" -> "signal-transmittor" (the Examples /tp key rule, runs of '-' collapsed).
std::string spawnKey(std::string_view s) {
  std::string o;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (std::isalnum(u)) o += char(std::tolower(u));
    else if (!o.empty() && o.back() != '-') o += '-';
  }
  while (!o.empty() && o.back() == '-') o.pop_back();
  return o;
}

// JD's approved B9 names (2026-11-11/12). Ids are the approved ones; none is invented here. Mortor
// (no concept picked) and the Via elbow (auto-built, never placed) are deliberately absent.
struct Approved { const char* id; const char* label; bool item; };
constexpr Approved kApproved[] = {
    {"lithokraft:watchor", "WATCHOR", false},
    {"lithokraft:bitor", "BITOR", false},
    {"lithokraft:pushor", "PUSHOR", false},
    {"lithokraft:swivelor", "SWIVELOR", false},
    {"lithokraft:resin", "RESIN", true},
};

// Sides, counter-clockwise from above: north band (reads toward -x), west (+z), south (+x), east (-z).
constexpr int kOutX[4] = {0, -1, 0, 1}, kOutZ[4] = {-1, 0, 1, 0};
constexpr int kAlongX[4] = {-1, 0, 1, 0}, kAlongZ[4] = {0, 1, 0, -1};

bool planSpiral(const std::string& label, int pitch, int perSide, cComponentTestWorld::Spiral& out) {
  constexpr int lim = cComponentTestWorld::kSpiralLimit;
  auto dist = [pitch](int s) { return cComponentTestWorld::kSpiralStart + (pitch * s) / 4; };
  out = {};
  out.pitch = pitch;
  int s = 0, a = -dist(0), onSide = 0;
  for (char ch : label) {
    for (;;) {  // the glyph (3 wide) must fit on this side before the next band's far edge
      if (dist(s) + 4 > lim) return false;
      const int end = std::min(dist(s + 1) + 4, lim);
      if (a >= -lim && a + 2 <= end && onSide < perSide) break;
      a = -dist(s) + 3;  // next side starts two clear rows past this band's inner edge
      ++s;
      onSide = 0;
    }
    cComponentTestWorld::Glyph g;
    g.ch = ch;
    g.side = s;
    if (ch != ' ') {
      const char* gl = blockFont3x5Labels(ch);
      if (!gl || std::strlen(gl) < 15) return false;
      const int k = s % 4, d = dist(s);
      for (int r = 0; r < 5; ++r)      // r = 0 is the glyph's top row: nearest the component
        for (int c = 0; c < 3; ++c)    // c = 0 is its left column for a reader outside the band
          if (gl[r * 3 + c] == '#')
            g.cells.push_back({kOutX[k] * (d + r) + kAlongX[k] * (a + c), kOutZ[k] * (d + r) + kAlongZ[k] * (a + c)});
    }
    out.glyphs.push_back(std::move(g));
    a += 4;  // 3 wide + 1 gap
    ++onSide;
  }
  out.fits = true;
  return true;
}

}  // namespace

cComponentTestWorld::Spiral cComponentTestWorld::spiral(const std::string& label) {
  // Spread the name round the component: at most a quarter of it per side first (so even a short
  // name turns its corners), then the widest lap pitch that fits; longer runs per side only if needed.
  Spiral sp;
  const int n = int(label.size());
  for (int perSide = std::max(2, (n + 3) / 4); perSide <= std::max(2, n); ++perSide)
    for (int pitch = 28; pitch >= 7; --pitch)
      if (planSpiral(label, pitch, perSide, sp)) return sp;
  sp = {};
  return sp;
}

std::vector<cComponentTestWorld::Pad> cComponentTestWorld::pads() {
  const auto& reg = blockRegistry();
  const cBlockFamilies& fam = cBlockFamilies::get();
  std::vector<Pad> out;
  auto have = [&out](BlockId b) {
    return std::any_of(out.begin(), out.end(), [b](const Pad& p) { return p.block == b && b != kAir; });
  };
  for (BlockFamily f : {BlockFamily::Trace, BlockFamily::Lamp})
    for (BlockId b : fam.members(f)) {
      if (b == kAir || isPaletteHidden(b) || have(b)) continue;  // side Vias are orientations of the Via
      Pad p;
      p.block = b;
      p.engineId = reg.idOf(b);
      p.label = upper(reg.def(b).name);
      p.id = spawnKey(reg.def(b).name);
      out.push_back(std::move(p));
    }
  for (const Approved& a : kApproved) {
    Pad p;
    p.approved = true;
    p.itemOnly = a.item;
    p.engineId = a.id;
    p.label = a.label;
    p.id = spawnKey(a.label);
    if (!a.item)
      if (const auto id = reg.findById(a.id)) {
        if (have(*id)) continue;  // registered with a Trace role: its family pad already exists
        p.block = *id;
      }
    out.push_back(std::move(p));
  }
  for (size_t i = 0; i < out.size(); ++i) {
    Pad& p = out[i];
    const int col = int(i) % kCols, row = int(i) / kCols;
    p.x0 = (kOriginChunkX + col * (kPadChunks + kAisleChunks)) * kChunkSize;
    p.z0 = (kOriginChunkZ + row * (kPadChunks + kAisleChunks)) * kChunkSize;
    p.cx = p.x0 + kCentre;
    p.cz = p.z0 + kCentre;
    p.spawnX = float(p.cx) + 0.5f;
    p.spawnZ = float(p.z0 - 4) + 0.5f;
  }
  return out;
}

std::vector<std::string> cComponentTestWorld::spawnIds() {
  std::vector<std::string> ids{"spawn"};
  for (const Pad& p : pads()) ids.push_back(p.id);
  return ids;
}

void cComponentTestWorld::spawn(float& x, float& z, float& yaw) {
  const int cols = std::min<int>(kCols, std::max<int>(1, int(pads().size())));
  const int gx0 = kOriginChunkX * kChunkSize, gx1 = gx0 + cols * kPitch - kAisleChunks * kChunkSize;  // grid x span
  x = float((gx0 + gx1) / 2) + 0.5f;
  z = float(kOriginChunkZ * kChunkSize - kChunkSize / 2) + 0.5f;  // middle of the north aisle
  yaw = 1.5707963f;  // looks +Z (the test world's convention)
}

void cComponentTestWorld::gridChunks(int& cx0, int& cz0, int& cx1, int& cz1) {
  const int n = int(pads().size());
  const int cols = std::min(kCols, std::max(1, n)), rows = std::max(1, (n + kCols - 1) / kCols);
  const int step = kPadChunks + kAisleChunks;
  cx0 = kOriginChunkX - kAisleChunks;
  cz0 = kOriginChunkZ - kAisleChunks;  // the spawn aisle
  cx1 = kOriginChunkX + (cols - 1) * step + kPadChunks + kAisleChunks;
  cz1 = kOriginChunkZ + (rows - 1) * step + kPadChunks + kAisleChunks;
}

int cComponentTestWorld::minWorldChunks() {
  int cx0, cz0, cx1, cz1;
  gridChunks(cx0, cz0, cx1, cz1);
  return std::max(cx1, cz1);
}

cComponentTestWorld::Layout cComponentTestWorld::build(ChunkMap& w, int floorY) const {
  Layout L;
  L.floorY = floorY;
  spawn(L.spawnX, L.spawnZ, L.spawnYaw);
  L.pads = pads();
  const auto& reg = blockRegistry();
  auto put = [&](int x, int y, int z, BlockId b, CellState st) {
    if (!w.set(x, y, z, b)) return false;
    w.setState(x, y, z, st);
    ++L.placed;
    return true;
  };
  for (const Pad& p : L.pads) {
    // The plain is already flat silicone; the pad only re-asserts its own footprint (air above,
    // silicone top), so a pad is exactly 3 x 3 chunks whatever the generator.
    for (int z = p.z0; z < p.z0 + kPad; ++z)
      for (int x = p.x0; x < p.x0 + kPad; ++x) {
        if (w.get(x, floorY, z) != kWaferSilicon) put(x, floorY, z, kWaferSilicon, 0);
        for (int y = floorY + 1; y <= floorY + 8; ++y)
          if (w.get(x, y, z) != kAir) { w.set(x, y, z, kAir); w.setState(x, y, z, 0); }
      }
    const Spiral sp = spiral(p.label);
    if (!sp.fits) { L.unfit.push_back(p.label); continue; }
    for (const Glyph& g : sp.glyphs)
      for (const auto& [dx, dz] : g.cells)
        if (put(p.cx + dx, floorY, p.cz + dz, kCeramic, 0)) ++L.labelCells;  // flush in the floor
    if (p.block != kAir && !p.itemOnly) {
      // Default facing 0 (front toward -Z, the aisle side the spawn looks from), as the test world
      // places its rows; attachables stand on the floor below them.
      const CellState st = (reg.def(p.block).flags & kAttachable) ? withAttach(0, 1) : CellState(0);
      if (put(p.cx, floorY + 1, p.cz, p.block, st)) ++L.components;
    }
  }
  return L;
}

}  // namespace lithokraft
