// Offline world file check (2026-10-31, JD: Save World verification). Opens every region file
// (r.X.Z.llr) under a world folder with the engine's region reader and reads back every present
// column record (header, location table, record framing, sidecars). Read-only.
#pragma once
#include <string>

namespace lithokraft {

class cWorldFileCheck final {
 public:
  struct Result {
    int regions = 0, columns = 0, errors = 0;
    std::string firstError;
    bool ok() const { return errors == 0 && regions > 0; }
  };
  static Result check(const std::string& worldDir);
};

}  // namespace lithokraft
