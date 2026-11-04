// World catalog: see cWorldCatalog.h. 2026-10-30 (JD).
#include "lithokraft/game/cWorldCatalog.h"
#include <cerrno>
#include <cstdio>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace lithokraft {

cWorldCatalog::cWorldCatalog() {
  const std::vector<std::string> examples{"spawn", "testarea", "stand", "screen16", "grove", "display", "framebuffer",
                                          "fb-wall", "wall-pixel", "pattern", "sawtooth", "redirector", "litho",
                                          "circuits", "wire-torture", "clocks", "pixel-display"};
  std::vector<std::string> bolt{"spawn", "lab"};
  for (int i = 1; i <= 10; ++i) bolt.push_back("b" + std::to_string(i));
  for (int i = 1; i <= 9; ++i) bolt.push_back("a" + std::to_string(i));
  bolt.push_back("r0"); bolt.push_back("hud");
  entries_ = {
      {"default", "Default world", {}, {"spawn"}},
      {"testworld", "Test World", {"--test-world"}, {"spawn", "testarea", "grove"}},
      {"testworld-examples", "Test World + Examples", {"--test-world-examples"}, examples},
      {"wire-torture", "Wire Torture", {"--bench-scene", "wire-torture"}, {"spawn"}},
      {"flatlab", "Flat Lab (Trace)", {"--flat-lab"}, examples},
      {"boltlab", "Bolt PCHIE Lab", {"--bolt-lab"}, bolt},
  };
}

const cWorldCatalog& cWorldCatalog::instance() {
  static const cWorldCatalog c;
  return c;
}

int cWorldCatalog::indexFor(const std::string& benchScene, const std::string& dimension, bool examples) const {
  std::string id;
  if (benchScene == "boltlab") id = "boltlab";
  else if (benchScene == "testworld" && dimension == "flatlab") id = "flatlab";
  else if (benchScene == "testworld") id = examples ? "testworld-examples" : "testworld";
  else if (benchScene == "wire-torture") id = "wire-torture";
  else if (benchScene == "default" && dimension == "wafer") id = "default";
  for (size_t i = 0; i < entries_.size(); ++i)
    if (entries_[i].id == id) return int(i);
  return -1;
}

bool cWorldCatalog::launch(const std::string& exe, const std::vector<std::string>& args) const {
  std::vector<std::string> all{exe};
  all.insert(all.end(), args.begin(), args.end());
#if defined(_WIN32)
  std::vector<std::string> quoted;
  for (const auto& a : all) {
    std::string q = "\"";
    for (char c : a) { if (c == '"') q += '\\'; q += c; }
    if (q.back() == '\\') q += '\\';
    q += '"';
    quoted.push_back(std::move(q));
  }
  std::vector<const char*> av;
  for (const auto& q : quoted) av.push_back(q.c_str());
  av.push_back(nullptr);
  if (_spawnv(_P_NOWAIT, exe.c_str(), av.data()) == -1) { std::fprintf(stderr, "world picker: launch failed (errno %d)\n", errno); return false; }
  return true;
#else
  std::vector<char*> av;
  for (auto& a : all) av.push_back(a.data());
  av.push_back(nullptr);
  execv(exe.c_str(), av.data());
  std::fprintf(stderr, "world picker: launch failed (errno %d)\n", errno);
  return false;
#endif
}

}  // namespace lithokraft