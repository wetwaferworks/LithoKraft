// 2026-10-31 (JD): block families. Every registered block gets exactly one family, assigned once
// when the registry is first read here (after content registration) and read-only after that:
// a plain immutable table behind a C++ static-init guard, so readers never lock.
// Display grouping only: block ids, saves and goldens are untouched.
#pragma once
#include <cstdint>
#include <vector>
#include "lithokraft/content/blocks.h"

namespace lithokraft {

enum class BlockFamily : uint8_t { Trace = 0, Lamp = 1, Building = 2, Other = 3, Unassigned = 255 };

class cBlockFamilies {
 public:
  static const cBlockFamilies& get();
  BlockFamily familyOf(BlockId b) const { return size_t(b) < family_.size() ? family_[size_t(b)] : BlockFamily::Unassigned; }
  int rankOf(BlockId b) const { return size_t(b) < rank_.size() ? rank_[size_t(b)] : 0; }
  /// Every block of `f`, curated order then registry order.
  const std::vector<BlockId>& members(BlockFamily f) const { return members_[size_t(f) < 4 ? size_t(f) : 3]; }
  /// Ids with no family (the selftest requires none).
  const std::vector<BlockId>& unassigned() const { return unassigned_; }
  static const char* name(BlockFamily f);
 private:
  cBlockFamilies();
  std::vector<BlockFamily> family_;
  std::vector<int> rank_;
  std::vector<BlockId> members_[4];
  std::vector<BlockId> unassigned_;
};

}  // namespace lithokraft
