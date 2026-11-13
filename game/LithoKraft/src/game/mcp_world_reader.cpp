// cMcpWorldReader: see mcp_world_reader.h. 2026-11-10 (JD t5033u, PCHIE P7).
#include "lithokraft/game/mcp_world_reader.h"

#include <utility>

namespace lithokraft {

cMcpWorldReader::cMcpWorldReader(std::function<uint16_t(int, int, int)> read) : read_(std::move(read)) {}

uint16_t cMcpWorldReader::getBlock(int x, int y, int z) { return read_ ? read_(x, y, z) : uint16_t(0); }

}  // namespace lithokraft
