// 2026-10-25 (JD, Posts): readout behaviours for the Standing and Wall Posts (the text itself is
// cell text, edited through kCmdPostEdit; see lithokraft/content/post.h).
#include <format>
#include "components.h"
#include "lithokraft/content/post.h"

namespace lithokraft::components {
namespace {

constexpr const char* kOctant[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

class cStandingPost final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | {} | facing {} | right-click to write", d.name, post::isUpper(s) ? "panel" : "stand",
                       kOctant[stFacing8(s) & 7]);
  }
};

class cWallPost final : public cBlockBehavior {
 public:
  std::string describe(const BlockDef& d, CellState s) const override {
    return std::format("{} | facing {} | right-click to write", d.name, facingName(stFacing(s)));
  }
};

}  // namespace

void registerPosts(const Registrar& add) {
  add("post", std::make_unique<cStandingPost>());
  add("wall_post", std::make_unique<cWallPost>());
}

}  // namespace lithokraft::components
