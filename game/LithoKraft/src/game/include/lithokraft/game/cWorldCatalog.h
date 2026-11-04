// World catalog (2026-10-30, JD): every world the launch menu's world picker can open, the command-line
// flags that build it, and its named spawns (the /tp marks the world's build code registers). Picking a
// different world relaunches the exe with that world's flags (+ --spawn <name>), the same way the
// renderer switch relaunches; the existing flags and desktop shortcuts keep working unchanged.
#pragma once
#include <string>
#include <vector>

namespace lithokraft {

class cWorldCatalog final {
 public:
  struct Entry {
    std::string id, title;
    std::vector<std::string> args;    ///< command-line flags that build this world
    std::vector<std::string> spawns;  ///< "spawn" (world default) first, then named spawns
  };

  static const cWorldCatalog& instance();
  const std::vector<Entry>& entries() const { return entries_; }
  /// Index of the world a launch with these settings builds, or -1 (a bench scene not in the list).
  int indexFor(const std::string& benchScene, const std::string& dimension, bool examples) const;
  /// Start `exe` with `args` (detached). Returns false when the process could not be started.
  bool launch(const std::string& exe, const std::vector<std::string>& args) const;

 private:
  cWorldCatalog();
  std::vector<Entry> entries_;
};

}  // namespace lithokraft