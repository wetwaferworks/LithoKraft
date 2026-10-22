// Cortana 2026-10-15 1900 [GAME]: the lithography machine (mask-and-expose), JD's wiring canon.
//
// The Etchor (facing f; front = chip out) sits at the centre of a vertical stack, along
// its right axis r = dirRight(f):
//
//   y+2  [Emitor>][Laysor][Connector][Connector*][Connector][Laysor][<Emitor]  top: the EXPOSURE row
//   y+1                              [ blue Via ]                              (blue Vior)
//   y    [ back: Input Buffer (wafers) ]  [Etchor]  [ front: chip exit, air ]
//   y-1                              [ red Via  ]                              (red Vior)
//   y-2         [Connector][Connector][Connector][Connector][Connector]        bottom: the MASK row
//
// * the middle top Connector takes Trace dust: lit Trace (flat red/blue wire) on top of it.
// Every other Connector is fed by a strongly powered neighbour (a torch-powered block, a block
// with an on switch attached, a block a lit Repeator faces). Structure blocks never count as
// feeders. Lasers enter from the SIDES: an Emitor (lit, i.e. powered) facing inward through at
// least one Laysor, ending at an outer top Connector.
//
// Expose: a laser hits the top row and at least one top Connector is fed. Mask: the fed bits of
// the five bottom Connectors. The etch consumes one wafer from the back buffer (its count lives
// in the buffer's cell-state value bits) and the named chip exits the front (into the player's
// inventory this pass; no dropped-item entities exist yet). Chip code = top bits | bottom << 3.
//
// Pure reads through LithoReader, so the scan is testable with any world and runs on the sim
// thread between ticks in the game. Nothing here touches the signal sim's tick (hash-neutral).
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "lithokraft/content/blocks.h"  // BlockId, CellState

namespace lithokraft {

struct LithoReader {
  std::function<BlockId(int, int, int)> block;
  std::function<CellState(int, int, int)> state;
  std::function<bool(int, int, int)> strongPowered;  ///< SignalSim::strongPowered in the game
};

constexpr int kLithoTopCount = 3, kLithoBottomCount = 5, kLithoLaserReach = 16;
constexpr int kLithoBufferMax = 64;  ///< wafers a back buffer holds (cell-state value bits)

struct LithoMachine {
  bool formed = false;      ///< every canon block in place
  std::string problem;      ///< first missing piece when !formed
  int x = 0, y = 0, z = 0, facing = 0;
  bool laserLeft = false, laserRight = false;  ///< a lit Emitor beam reaches that side
  uint8_t top = 0;          ///< fed top Connectors, bit i = offset i-1 along r
  uint8_t bottom = 0;       ///< fed bottom Connectors, bit i = offset i-2 along r
  int bufX = 0, bufY = 0, bufZ = 0;
  int wafers = 0;           ///< wafers in the back buffer
  bool exitClear = false;   ///< the front cell is air
  bool exposed() const { return formed && (laserLeft || laserRight) && top != 0; }
  uint8_t code() const { return uint8_t(top | (bottom << 3)); }
  bool canEtch() const { return exposed() && wafers > 0 && exitClear; }
  std::string describe() const;
};

/// Scan the machine around the Etchor at (x, y, z).
LithoMachine scanLithoMachine(const LithoReader& w, int x, int y, int z);
/// Nearest Etchor within `radius` (Chebyshev) of (px, py, pz); false if none.
bool findNearestEtchor(const LithoReader& w, int px, int py, int pz, int radius, int& ox, int& oy, int& oz);
/// The buffer cell state with `count` wafers (facing and other bits kept).
inline CellState lithoBufferWithCount(CellState s, int count) {
  return CellState((s & ~CellState(0x7f)) | CellState(count < 0 ? 0 : count > kLithoBufferMax ? kLithoBufferMax : count));
}

}  // namespace lithokraft