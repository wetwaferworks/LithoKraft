#include "lithokraft/content/wire_render_probe.h"
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/signal.h"

namespace lithokraft {

int cWireRenderProbe::run(int ticks) {
  ChunkMap w;
  w.setRegistry(&blockRegistry());
  for (int cz = 0; cz < 2; ++cz)
    for (int cx = 0; cx < 12; ++cx) {
      auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz;
      w.insert(std::move(col));
    }
  const int y = 10, kE = 1;
  for (int z = 0; z < 32; ++z)
    for (int x = 0; x < 192; ++x) w.set(x, y - 1, z, kWaferSilicon);
  cSignalSim sim;
  using Cell = std::tuple<int, int, int>;
  std::vector<Cell> wires;
  auto put = [&](int x, int z, BlockId b) { w.set(x, y, z, b); sim.onBlockChanged(x, y, z, b); };
  auto putF = [&](int x, int z, BlockId b) { w.set(x, y, z, b); w.setState(x, y, z, makeState(0, kE)); sim.onBlockChanged(x, y, z, b); };
  auto trace = [&](int x0, int x1, int z) { for (int x = x0; x <= x1; ++x) { put(x, z, kRedTrace); wires.push_back({x, y, z}); } };
  // Row A (z 2): torch + 100 Trace (dies after 64), crosses 6 chunk borders.
  put(0, 2, kRedTorch); trace(1, 100, 2);
  // Row B (z 6): torch, 59 Trace, Repeator (re-powers to 64), 100 more Trace.
  put(0, 6, kRedTorch); trace(1, 59, 6); putF(60, 6, kRepeator); trace(61, 160, 6);
  // Row C (z 10): toggled torch -> Pulsor relay chain, each leg crossing chunk borders (one leg = 64 blocks).
  trace(1, 3, 10); putF(4, 10, kPulsor); trace(5, 68, 10); putF(69, 10, kPulsor); trace(70, 133, 10);
  putF(134, 10, kPulsor); trace(135, 180, 10);
  std::set<Cell> dirty;
  const cSignalSim::DirtyFn rec = [&](int x, int yy, int z) { dirty.insert({x, yy, z}); };
  auto& buf = sim.strengthBuffer();
  int missedTotal = 0, bufTotal = 0, changedTotal = 0;
  std::printf("tick | changed | missed-remesh | strbuf-diff | lit A/B/C | first missed cells\n");
  for (int t = 0; t < ticks; ++t) {
    if (t % 24 == 0) put(0, 10, kRedTorch);        // rising edge into the chain
    else if (t % 24 == 6) put(0, 10, kAir);
    std::map<Cell, CellState> before;
    for (auto& c : wires) before[c] = w.getState(std::get<0>(c), std::get<1>(c), std::get<2>(c));
    dirty.clear();
    sim.tick(w, 6, 1, 100, rec);
    int changed = 0, missed = 0, bdiff = 0, lit[3] = {0, 0, 0};
    std::vector<Cell> first;
    for (auto& c : wires) {
      const auto [x, yy, z] = c;
      const CellState s = w.getState(x, yy, z);
      if (stValue(s) > 0) ++lit[z == 2 ? 0 : z == 6 ? 1 : 2];
      if (s != before[c]) {
        ++changed;
        if (!dirty.contains(c)) { ++missed; if (first.size() < 6) first.push_back(c); }
      }
      const uint32_t slot = buf.slotOf(uint64_t(packPos(x, yy, z)));
      const int bv = slot == litholattice::TraceStrengthBuffer::kNoSlot ? 0 : int(buf.get(slot));
      if (bv != int(stValue(s))) ++bdiff;
    }
    { // per-leg first lit cell of the Pulsor chain (x 5 / 70 / 135): one tick later per stage
      const int a = stValue(w.getState(5, y, 10)) > 0, b = stValue(w.getState(70, y, 10)) > 0, c2 = stValue(w.getState(135, y, 10)) > 0;
      if (a || b || c2) std::printf("  chain t%d: leg1 %d leg2 %d leg3 %d\n", t, a, b, c2);
    }
    changedTotal += changed; missedTotal += missed; bufTotal += bdiff;
    if (changed || missed || bdiff || t < 3) {
      std::printf("%4d | %7d | %13d | %11d | %3d/%3d/%3d |", t, changed, missed, bdiff, lit[0], lit[1], lit[2]);
      for (auto& [x, yy, z] : first) std::printf(" (%d,%d,%d cx%d)", x, yy, z, x >> 4);
      std::printf("\n");
    }
  }
  std::printf("TOTAL changed %d, missed-remesh %d, strbuf-diff cell-ticks %d\n", changedTotal, missedTotal, bufTotal);
  return missedTotal;
}

}  // namespace lithokraft