// Cortana 2026-10-15 1900 [GAME]: lithography machine scan. See lithography.h for the canon.
#include "lithokraft/game/lithography.h"

#include <cstdlib>
#include "lithokraft/content/blocks.h"

namespace lithokraft {
namespace {

bool isStructure(BlockId b) {
  return b == kConnector || b == kRedVia || b == kBlueVia || b == kEtchor || b == kLaysor || b == kEmitor ||
         b == kInputBuffer;
}

// A Connector is fed by a strongly powered non-structure neighbour (6 sides).
bool fedByStrong(const LithoReader& w, int x, int y, int z) {
  static const int k6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  for (const auto& d : k6) {
    const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
    if (isStructure(w.block(nx, ny, nz))) continue;
    if (w.strongPowered(nx, ny, nz)) return true;
  }
  return false;
}

// The middle top Connector takes Trace dust: lit flat Trace on top of it.
bool fedByTrace(const LithoReader& w, int x, int y, int z) {
  return isFlatWire(w.block(x, y + 1, z)) && stValue(w.state(x, y + 1, z)) > 0;
}

// Laser from one side: walking outward from the outer top Connector along direction `d`,
// one or more Laysors then an Emitor that faces back along the walk and is lit (powered).
bool laserFrom(const LithoReader& w, int x, int y, int z, int d) {
  int laysors = 0;
  for (int k = 1; k <= kLithoLaserReach; ++k) {
    const int cx = x + kDirX[d] * k, cz = z + kDirZ[d] * k;
    const BlockId b = w.block(cx, y, cz);
    if (b == kLaysor) { ++laysors; continue; }
    if (b != kEmitor) return false;
    const CellState s = w.state(cx, y, cz);
    return laysors > 0 && stFacing(s) == dirBack(d) && (s & 1) != 0;  // Lamp role: lit in bit 0
  }
  return false;
}

std::string bits(uint8_t v, int n) {
  std::string s;
  for (int i = 0; i < n; ++i) s += (v >> i) & 1 ? '1' : '0';
  return s;
}

}  // namespace

LithoMachine scanLithoMachine(const LithoReader& w, int x, int y, int z) {
  LithoMachine m;
  m.x = x; m.y = y; m.z = z;
  auto fail = [&](const std::string& why) { m.formed = false; m.problem = why; return m; };
  if (w.block(x, y, z) != kEtchor) return fail("no Etchor here");
  const int f = stFacing(w.state(x, y, z)), r = dirRight(f);
  m.facing = f;
  const int fx = kDirX[f], fz = kDirZ[f], rx = kDirX[r], rz = kDirZ[r];
  m.bufX = x - fx; m.bufY = y; m.bufZ = z - fz;
  const BlockId buf = w.block(m.bufX, m.bufY, m.bufZ);
  if (buf != kInputBuffer) return fail("no Input Buffer behind the Etchor (the wafer feed)");
  if (stFacing(w.state(m.bufX, m.bufY, m.bufZ)) != f) return fail("the back Input Buffer's front must face the Etchor");
  m.wafers = stValue(w.state(m.bufX, m.bufY, m.bufZ));
  m.exitClear = w.block(x + fx, y, z + fz) == kAir;
  if (w.block(x, y + 1, z) != kBlueVia) return fail("no blue Vior (blue Via) directly above the Etchor");
  if (w.block(x, y - 1, z) != kRedVia) return fail("no red Vior (red Via) directly below the Etchor");
  for (int o = -1; o <= 1; ++o)
    if (w.block(x + rx * o, y + 2, z + rz * o) != kConnector) return fail("top row: need 3 Connectors above the blue Vior");
  for (int o = -2; o <= 2; ++o)
    if (w.block(x + rx * o, y - 2, z + rz * o) != kConnector) return fail("bottom row: need 5 Connectors below the red Vior");
  m.formed = true;
  for (int o = -1; o <= 1; ++o) {
    const int cx = x + rx * o, cz = z + rz * o;
    const bool fed = o == 0 ? fedByTrace(w, cx, y + 2, cz) : fedByStrong(w, cx, y + 2, cz);
    if (fed) m.top |= uint8_t(1u << (o + 1));
  }
  for (int o = -2; o <= 2; ++o)
    if (fedByStrong(w, x + rx * o, y - 2, z + rz * o)) m.bottom |= uint8_t(1u << (o + 2));
  m.laserRight = laserFrom(w, x + rx, y + 2, z + rz, r);
  m.laserLeft = laserFrom(w, x - rx, y + 2, z - rz, dirBack(r));
  return m;
}

bool findNearestEtchor(const LithoReader& w, int px, int py, int pz, int radius, int& ox, int& oy, int& oz) {
  int best = -1;
  for (int dy = -radius; dy <= radius; ++dy)
    for (int dz = -radius; dz <= radius; ++dz)
      for (int dx = -radius; dx <= radius; ++dx) {
        if (w.block(px + dx, py + dy, pz + dz) != kEtchor) continue;
        const int d2 = dx * dx + dy * dy + dz * dz;
        if (best < 0 || d2 < best) { best = d2; ox = px + dx; oy = py + dy; oz = pz + dz; }
      }
  return best >= 0;
}

std::string LithoMachine::describe() const {
  if (!formed) return "lithography: not formed - " + problem;
  std::string s = "lithography: formed; laser " + std::string(laserLeft ? "L" : "-") + (laserRight ? "R" : "-") +
                  ", expose(top)=" + bits(top, kLithoTopCount) + ", mask(bottom)=" + bits(bottom, kLithoBottomCount) +
                  ", code=" + std::to_string(code()) + ", wafers=" + std::to_string(wafers) +
                  (exitClear ? "" : ", FRONT BLOCKED");
  if (!(laserLeft || laserRight)) s += " | no laser: power an Emitor facing in through a Laysor";
  else if (!top) s += " | laser on, but no top Connector fed (middle takes lit Trace on top)";
  return s;
}

}  // namespace lithokraft