#pragma once
// Fix (b) re-mesh churn: read-only view of which chunk columns are resident, so the
// L0 neighbour hold can ask about neighbours without knowing the chunk map type.

namespace lithokraft {

class IColumnPresence {
 public:
  virtual ~IColumnPresence() = default;
  virtual bool hasColumn(int cx, int cz) const = 0;
};

}  // namespace lithokraft
