// Cortana 2026-10-15 1900 [GAME]: the hands-on lithography test machine (JD's wiring canon,
// see game/lithography.h). Built next to the spawn by --bench-scene litho / litho-wired.
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/circuit_scene.h"

namespace lithokraft {

int buildLithoMachine(ChunkMap& w, int cx, int y, int cz, bool wired) {
  int n = 0;
  auto put = [&](int x, int yy, int z, BlockId b, CellState s = 0) {
    w.set(x, yy, z, b);
    if (s) w.setState(x, yy, z, s);
    ++n;
  };
  for (int x = cx - 7; x <= cx + 7; ++x)
    for (int z = cz - 4; z <= cz + 3; ++z) {
      for (int yy = y + 1; yy <= y + 10; ++yy) w.set(x, yy, z, kAir);
      put(x, y, z, kWaferSilicon);  // floor (powerable silicon)
    }
  const int ey = y + 4;  // the Etchor faces north (-Z): chip out toward the spawn side
  put(cx, ey, cz, kEtchor, makeState(0, 0));
  put(cx, ey, cz + 1, kInputBuffer, makeState(8, 0));  // back buffer, front faces the Etchor, 8 wafers
  put(cx, ey + 1, cz, kBlueVia);
  put(cx, ey - 1, cz, kRedVia);
  for (int o = -1; o <= 1; ++o) put(cx + o, ey + 2, cz, kConnector);
  for (int o = -2; o <= 2; ++o) put(cx + o, ey - 2, cz, kConnector);
  // Lasers enter from the sides: Emitor -> Laysor -> outer top Connector.
  put(cx - 2, ey + 2, cz, kLaysor, makeState(0, 1));
  put(cx + 2, ey + 2, cz, kLaysor, makeState(0, 1));
  put(cx - 3, ey + 2, cz, kEmitor, makeState(0, 1));  // faces east, into the machine
  put(cx + 3, ey + 2, cz, kEmitor, makeState(0, 3));  // faces west
  for (int s : {-3, 3}) {  // Emitor pedestal; the block under each Emitor is its power block
    for (int yy = y + 1; yy <= ey - 1; ++yy) put(cx + s, yy, cz, kWaferSilicon);
    put(cx + s, ey + 1, cz, kWaferSilicon);
    if (wired) put(cx + s, ey, cz, kRedTorch);  // torch under the power block: strong power -> Emitor lit
  }
  // Feeder blocks in front of every Connector except the middle top one (it takes Trace dust).
  for (int o = -2; o <= 2; ++o) {
    put(cx + o, ey - 2, cz - 1, kWaferSilicon);
    if (wired && (o == -2 || o == 0 || o == 2)) put(cx + o, ey - 3, cz - 1, kRedTorch);  // mask 10101
  }
  for (int o : {-1, 1}) {
    put(cx + o, ey + 2, cz - 1, kWaferSilicon);
    put(cx + o, ey, cz - 1, kWaferSilicon);  // torch support
    if (wired) put(cx + o, ey + 1, cz - 1, kRedTorch);
  }
  if (wired) {  // middle top: lit Trace on top, fed by a torch-powered block beside it
    put(cx, ey + 3, cz, kRedTrace);
    put(cx, ey + 3, cz - 1, kWaferSilicon);
    put(cx, ey + 2, cz - 1, kRedTorch);
    put(cx, ey + 1, cz - 1, kWaferSilicon);
  } else {  // power sources for JD to wire from: a row of torches on the floor at the front
    for (int x = cx - 5; x <= cx + 5; x += 2) put(x, y + 1, cz - 4, kRedTorch);
  }
  return n;
}

}  // namespace lithokraft