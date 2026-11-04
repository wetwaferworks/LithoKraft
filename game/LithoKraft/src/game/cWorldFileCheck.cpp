// Offline world file check: see cWorldFileCheck.h. 2026-10-31 (JD: Save World verification).
#include "lithokraft/game/cWorldFileCheck.h"
#include <cstdio>
#include <filesystem>
#include "litholattice/storage/storage.h"

namespace lithokraft {

cWorldFileCheck::Result cWorldFileCheck::check(const std::string& worldDir) {
  using litholattice::storage::cRegionFile;
  Result r;
  std::error_code ec;
  const auto fail = [&r](const std::string& e) { if (r.errors++ == 0) r.firstError = e; };
  for (auto it = std::filesystem::recursive_directory_iterator(worldDir, ec); !ec && it != std::filesystem::recursive_directory_iterator();
       it.increment(ec)) {
    if (!it->is_regular_file(ec) || it->path().extension() != ".llr") continue;
    cRegionFile reg(it->path());
    std::string err;
    if (!reg.open(&err)) { fail(it->path().string() + ": " + err); continue; }
    ++r.regions;
    for (int lz = 0; lz < cRegionFile::kSpan; ++lz)
      for (int lx = 0; lx < cRegionFile::kSpan; ++lx) {
        err.clear();
        if (reg.read(lx, lz, &err)) ++r.columns;
        else if (!err.empty()) fail(it->path().string() + " column " + std::to_string(lx) + "," + std::to_string(lz) + ": " + err);
      }
  }
  if (ec) fail("scan: " + ec.message());
  return r;
}

}  // namespace lithokraft
