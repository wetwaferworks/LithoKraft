// cMcpWorldReader: game-side adapter for the MCP world_get_block seam
// (litholattice::mcp::IWorldReader), per engine docs/MCP-GAME-SEAM.md section 3
// ("IWorldReader -> SimThread::getBlock: use the existing thread-safe
// single-block read"). 2026-11-10 (JD t5033u, PCHIE P7): wired so the PCHIE
// reasoning model can read the in-engine maze back before it plays.
//
// Threading: getBlock() runs on the MCP server thread and delegates to the
// supplied reader (Game binds cSimThread::getBlock, the engine's documented
// cross-thread single-block read). This adapter adds no lock of its own and
// holds no state beyond the reader. Bulk scans are NOT what this is for.
#pragma once

#include <cstdint>
#include <functional>

#include "litholattice/mcp/server.h"

namespace lithokraft {

class cMcpWorldReader final : public litholattice::mcp::IWorldReader {
 public:
  explicit cMcpWorldReader(std::function<uint16_t(int, int, int)> read);
  uint16_t getBlock(int x, int y, int z) override;

 private:
  std::function<uint16_t(int, int, int)> read_;
};

}  // namespace lithokraft
