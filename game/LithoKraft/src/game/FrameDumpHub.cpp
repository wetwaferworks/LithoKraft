#include "lithokraft/game/FrameDumpHub.h"

namespace lithokraft {

cFrameDumpHub& cFrameDumpHub::instance() {
  static cFrameDumpHub hub;
  return hub;
}

}  // namespace lithokraft
