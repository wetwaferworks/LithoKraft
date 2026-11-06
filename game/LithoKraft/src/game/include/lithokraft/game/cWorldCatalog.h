// World catalog (2026-10-30, JD): every world the launch menu's world picker can open, the command-line
// flags that build it, and its named spawns (the /tp marks the world's build code registers). Picking a
// different world loads it in-process from that world's flags (+ --spawn <name>; Archie Phase 2,
// JD 2026-11-04: no relaunch); the existing flags and desktop shortcuts keep working unchanged.
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

 private:
  cWorldCatalog();
  std::vector<Entry> entries_;
};

}  // namespace lithokraft