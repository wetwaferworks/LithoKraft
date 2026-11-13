#include <cmath>
#include "lithokraft/game/world_save.h"
#include <stdexcept>
#include "lithokraft/game/cWorldSnapshot.h"  // 2026-10-31 Save World
#include "lithokraft/content/items.h"  // findItem / findItemByStringId (v4 inventory string ids)
#include "litholattice/storage/block_names.h"
#include "litholattice/storage/file_contract.h"
#include "litholattice/render/material_payload.h"
#include "lithokraft/content/blocks.h"
#include <charconv>
#include <cstring>
#include <fstream>
#include <iterator>
#include "litholattice/core/json.h"
#include "litholattice/core/log.h"

namespace lithokraft {

std::optional<storage::Bytes> cWorldSave::migrateSignalV1ToV2(const storage::Bytes& v1) {
  // v1 entry: u64 key, 8x u8 (out..timer), u16 n, 2*n key bytes.
  // v2 inserts u8 channel (=1) + u8 beacon (=0) after the 8 u8s, before the count.
  storage::ByteReader r(v1.data(), v1.size());
  storage::ByteWriter w;
  while (r.ok() && !r.done()) {
    w.u64(r.u64());
    for (int i = 0; i < 8; ++i) w.u8(r.u8());
    w.u8(1);  // channel: default
    w.u8(0);  // beacon: off
    const uint16_t n = r.u16();
    w.u16(n);
    for (uint16_t i = 0; i < n; ++i) { w.u8(r.u8()); w.u8(r.u8()); }
    if (!r.ok()) return std::nullopt;
  }
  if (!r.ok()) return std::nullopt;
  return w.out;
}

std::optional<storage::Bytes> cWorldSave::migrateSignalV2ToV3(const storage::Bytes& v2) {
  storage::ByteReader r(v2.data(), v2.size());
  storage::ByteWriter w;
  while (r.ok() && !r.done()) {
    w.u64(r.u64());
    for (int i = 0; i < 10; ++i) w.u8(r.u8());
    w.u16(0);  // historyHi
    const uint16_t n = r.u16();
    w.u16(n);
    for (uint16_t i = 0; i < n; ++i) { w.u8(r.u8()); w.u8(r.u8()); }
    if (!r.ok()) return std::nullopt;
  }
  if (!r.ok()) return std::nullopt;
  return w.out;
}

storage::PayloadType cWorldSave::postTextPayload() {
  storage::PayloadType t;
  t.tag = kPostTextTag;
  t.version = 1;
  t.omitEmpty = true;
  t.save = [](const ChunkColumn& c) {
    storage::ByteWriter w;
    if (c.cellText.empty()) return w.out;
    w.u16(uint16_t(std::min<size_t>(c.cellText.size(), 0xffff)));
    size_t n = 0;
    for (const auto& [k, s] : c.cellText) {
      if (n++ == 0xffff) break;
      const size_t len = std::min<size_t>(s.size(), 0xffff);
      w.u32(k); w.u16(uint16_t(len));
      for (size_t i = 0; i < len; ++i) w.u8(uint8_t(s[i]));
    }
    return w.out;
  };
  t.load = [](const storage::Bytes& b, ChunkColumn& c) {
    if (b.empty()) return true;
    storage::ByteReader r(b.data(), b.size());
    const uint16_t n = r.u16();
    for (uint16_t i = 0; i < n && r.ok(); ++i) {
      const uint32_t k = r.u32();
      const uint16_t len = r.u16();
      std::string s;
      s.reserve(len);
      for (uint16_t j = 0; j < len && r.ok(); ++j) s.push_back(char(r.u8()));
      if (r.ok() && !s.empty()) c.cellText[k] = std::move(s);
    }
    return r.ok();
  };
  return t;
}

cWorldSave::cWorldSave(std::filesystem::path root, const litholattice::cMaterialReg* materials)
    : root_(std::move(root)) {
  // Infinite Registry: the BLKS payload translates between the StableId voxel
  // storage and the BlockId disk codec through this registry.
  payloads_.setRegistry(&blockRegistry());
  storage::PayloadType sig;
  sig.tag = kSignalTag;
  sig.version = cSignalSim::kStateVersion;
  sig.migrate[1] = [](const storage::Bytes& v1) -> std::optional<storage::Bytes> {
    return migrateSignalV1ToV2(v1);
  };
  sig.migrate[2] = [](const storage::Bytes& v2) -> std::optional<storage::Bytes> {
    return migrateSignalV2ToV3(v2);
  };
  sig.save = [](const ChunkColumn& c) {
    // Archie Phase 3c: SGNL export always rides the LF encode context (no exporting_ mutex).
    const auto* m = static_cast<const std::map<uint64_t, std::vector<uint8_t>>*>(storage::cLfWorldStore::tlsEncodeContext());
    if (!m) return storage::Bytes{};
    const auto it = m->find(ChunkKey::make(c.cx, c.cz));
    return it == m->end() ? storage::Bytes{} : it->second;
  };
  sig.load = [](const storage::Bytes& b, ChunkColumn&) {
    // Archie Phase 3c: per-request sink via TLS decode context (no signalSink_ mutex).
    if (auto* sink = static_cast<std::vector<uint8_t>*>(storage::cLfWorldStore::tlsDecodeContext()))
      sink->insert(sink->end(), b.begin(), b.end());
    return true;
  };
  payloads_.add(std::move(sig));
  // 2026-10-25 (JD, Posts): "PTXT" per-cell text. v1 = u16 count, then per cell u32 key
  // (ChunkColumn::cellTextKey), u16 length, bytes. omitEmpty: columns without text encode exactly as
  // before, and a build that predates Posts skips the unknown tag (the text is simply not shown).
  payloads_.add(postTextPayload());
  // PBR chunk materials (phase 2): the engine's MATS payload persists the per-voxel material
  // bytes as string IDs (see render/material_payload.h). The registry must outlive the stores.
  if (materials) payloads_.add(litholattice::makeMaterialPayload(*materials));
  else logError("save: no PBR material registry; chunk material bytes will not persist");
}

namespace {
// Review 2026-10-16 (open item: player.lat validation): a stat/vital entry is kept only when
// the read was intact and its name is a plausible registry key (1..64 printable ASCII bytes).
// Values are clamped later by the registries' set(); unknown names are ignored there.
bool playerLatNameOk(bool readOk, const storage::Bytes& nm) {
  if (!readOk || nm.empty() || nm.size() > 64) return false;
  for (auto c : nm) if (uint8_t(c) < 0x21 || uint8_t(c) > 0x7E) return false;
  return true;
}
// Region data for `dim`: any region image (or one of its backup generations)
// under <root>/<dim>. This is what a corrupt name table puts at risk, so it
// is the condition that turns "unreadable table" into "fail closed" (C-4).
bool regionDataExists(const std::filesystem::path& root, const std::string& dim) {
  std::error_code ec;
  const auto dir = root / dim;
  if (!std::filesystem::is_directory(dir, ec)) return false;
  for (const auto& e : std::filesystem::directory_iterator(dir, ec))
    if (e.is_regular_file(ec) && e.path().filename().string().find(".llr") != std::string::npos) return true;
  return false;
}

// Any file or crash footprint (backup / stale temp) at `path`.
bool footprintExists(const std::filesystem::path& path) {
  std::error_code ec;
  if (std::filesystem::exists(path, ec)) return true;
  if (std::filesystem::exists(std::filesystem::path(path.string() + storage::kAtomicTmpSuffix), ec)) return true;
  for (unsigned i = 1; i <= storage::kBackupDepthMax; ++i)
    if (std::filesystem::exists(std::filesystem::path(path.string() + storage::kBackupSuffix + "." + std::to_string(i)), ec)) return true;
  return false;
}

bool parseU64(const std::string& s, uint64_t& out) {
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
  return ec == std::errc{} && end == s.data() + s.size();
}

// C-L1 (P3-2): player.lat size cap for every read path (validator +
// readPlayer). A v4 file is ~50 short string ids + two name maps — a few
// KiB in practice; 1 MiB is orders of headroom, and anything past it is
// corruption or hostile, refused by readFileAll before it is slurped.
constexpr uint64_t kMaxPlayerBytes = 1ULL << 20;

// player.lat intactness for recoverCrashedWrite (SV-H3): magic + the stored
// CRC over the body — the same gate readPlayer applies, at header level.
bool playerFileIntact(const std::filesystem::path& p, std::string* err) {
  storage::Bytes all;
  if (!storage::readFileAll(p, &all, err, kMaxPlayerBytes)) return false;
  if (all.size() < 12) { if (err) *err = p.string() + ": too short"; return false; }
  storage::ByteReader r(all.data(), all.size());
  const uint32_t magic = r.u32(), crc = r.u32();
  const storage::Bytes body = r.bytes();
  if (!r.ok() || magic != 0x4D504C4Cu /* "LLPM" */ || storage::crc32(body.data(), body.size()) != crc) {
    if (err) *err = p.string() + ": bad magic or CRC (torn player.lat)";
    return false;
  }
  return true;
}

// level.json intactness for recoverCrashedWrite (SV-H2).
bool worldMetaIntact(const std::filesystem::path& p, std::string* err) {
  storage::Bytes all;
  if (!storage::readFileAll(p, &all, err)) return false;
  litholattice::Json j;
  std::string jerr;
  if (!litholattice::Json::parse(std::string(all.begin(), all.end()), j, &jerr)) {
    if (err) *err = p.string() + ": " + jerr;
    return false;
  }
  const litholattice::Json* seed = j.get("seed");
  uint64_t v = 0;
  if (!seed || seed->type != litholattice::Json::Type::String || !parseU64(seed->str, v)) {
    if (err) *err = p.string() + ": no usable seed field";
    return false;
  }
  return true;
}
}  // namespace

bool cWorldSave::writeWorldMeta(std::string* err) const {
  if (!hasSeed_) return true;  // no seed wired (tools/tests): nothing to persist
  litholattice::Json j = litholattice::Json::object();
  j.add("saveFormatVersion", litholattice::Json::number(kWorldMetaVersion));
  // Decimal string, not a JSON number: Json numbers are doubles and would
  // silently round seeds above 2^53 (judgment call, SV-H2).
  j.add("seed", litholattice::Json::string(std::to_string(seed_)));
  // Cortana 2026-10-16 [GAME] (JD): the persisted world clock (v2). Decimal strings like the seed.
  if (hasClock_.load()) {
    j.add("worldTick", litholattice::Json::string(std::to_string(clockTick_.load())));
    j.add("worldTime", litholattice::Json::string(std::to_string(clockTime_.load())));
  }
  if (hasArea_) {  // Fix run 3 (v3): the area this world runs with
    j.add("simAreaShape", litholattice::Json::string(litholattice::areaShapeName(area_.sim)));
    j.add("streamAreaShape", litholattice::Json::string(litholattice::areaShapeName(area_.stream)));
    j.add("tickRadius", litholattice::Json::number(double(area_.tickRadius)));
  }
  {  // 2026-10-25 (JD): named /tp spots — Archie Phase 3b: atomic snapshot, no tpMutex_
    const auto marks = std::atomic_load_explicit(&tpMarks_, std::memory_order_acquire);
    if (marks && !marks->empty()) {
      litholattice::Json arr = litholattice::Json::array();
      for (const TpMark& m : *marks) {
        litholattice::Json o = litholattice::Json::object();
        o.add("name", litholattice::Json::string(m.name));
        o.add("dim", litholattice::Json::string(m.dim));
        o.add("x", litholattice::Json::number(m.x));
        o.add("y", litholattice::Json::number(m.y));
        o.add("z", litholattice::Json::number(m.z));
        arr.arr.push_back(std::move(o));
      }
      j.add("tpMarks", std::move(arr));
    }
  }
  const std::string text = j.dump(2);
  return storage::writeFileAtomic(root_ / "level.json", reinterpret_cast<const uint8_t*>(text.data()), text.size(), err);
}

std::optional<uint64_t> cWorldSave::readWorldSeed(const std::filesystem::path& root, std::string* err) {
  const auto path = root / "level.json";
  if (!footprintExists(path)) return std::nullopt;  // pre-metadata save: absent, not corrupt (err stays empty)
  {
    std::string rerr;
    if (!storage::recoverCrashedWrite(path, worldMetaIntact, &rerr)) {
      if (err) *err = rerr.empty() ? (path.string() + ": world metadata unreadable") : rerr;
      return std::nullopt;
    }
  }
  storage::Bytes all;
  if (!storage::readFileAll(path, &all, err)) return std::nullopt;
  litholattice::Json j;
  std::string jerr;
  if (!litholattice::Json::parse(std::string(all.begin(), all.end()), j, &jerr)) {
    if (err) *err = path.string() + ": " + jerr;
    return std::nullopt;
  }
  const litholattice::Json* seed = j.get("seed");
  uint64_t v = 0;
  if (!seed || seed->type != litholattice::Json::Type::String || !parseU64(seed->str, v)) {
    if (err) *err = path.string() + ": no usable seed field";
    return std::nullopt;
  }
  return v;
}

std::optional<cWorldSave::WorldClockMeta> cWorldSave::readWorldClock(const std::filesystem::path& root, std::string* err) {
  // Called after readWorldSeed (which already ran crash recovery on level.json).
  const auto path = root / "level.json";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return std::nullopt;
  storage::Bytes all;
  if (!storage::readFileAll(path, &all, err)) return std::nullopt;
  litholattice::Json j;
  std::string jerr;
  if (!litholattice::Json::parse(std::string(all.begin(), all.end()), j, &jerr)) {
    if (err) *err = path.string() + ": " + jerr;
    return std::nullopt;
  }
  const litholattice::Json* t = j.get("worldTick");
  const litholattice::Json* u = j.get("worldTime");
  if (!t && !u) return std::nullopt;  // v1 metadata: no clock yet (err stays empty)
  WorldClockMeta c;
  if (!t || !u || t->type != litholattice::Json::Type::String || u->type != litholattice::Json::Type::String ||
      !parseU64(t->str, c.tick) || !parseU64(u->str, c.time)) {
    if (err) *err = path.string() + ": unusable worldTick/worldTime";
    return std::nullopt;
  }
  return c;
}

std::vector<cWorldSave::TpMark> cWorldSave::readTpMarks(const std::filesystem::path& root, std::string* err) {
  std::vector<TpMark> out;
  const auto path = root / "level.json";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return out;
  storage::Bytes all;
  if (!storage::readFileAll(path, &all, err)) return out;
  litholattice::Json j;
  std::string jerr;
  if (!litholattice::Json::parse(std::string(all.begin(), all.end()), j, &jerr)) {
    if (err) *err = path.string() + ": " + jerr;
    return out;
  }
  const litholattice::Json* a = j.get("tpMarks");
  if (!a) return out;  // older level.json: no marks (err stays empty)
  if (a->type != litholattice::Json::Type::Array) { if (err) *err = path.string() + ": tpMarks is not an array"; return out; }
  for (const litholattice::Json& o : a->arr) {
    TpMark m;
    m.name = o.getStr("name", "");
    m.dim = o.getStr("dim", "");
    m.x = float(o.getNum("x", 0)); m.y = float(o.getNum("y", 0)); m.z = float(o.getNum("z", 0));
    if (!m.name.empty() && std::isfinite(m.x) && std::isfinite(m.y) && std::isfinite(m.z)) out.push_back(std::move(m));
  }
  return out;
}

std::optional<cWorldSave::WorldAreaMeta> cWorldSave::readWorldArea(const std::filesystem::path& root, std::string* err) {
  // Fix run 3. Called after readWorldSeed (crash recovery already ran on level.json).
  const auto path = root / "level.json";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) return std::nullopt;
  storage::Bytes all;
  if (!storage::readFileAll(path, &all, err)) return std::nullopt;
  litholattice::Json j;
  std::string jerr;
  if (!litholattice::Json::parse(std::string(all.begin(), all.end()), j, &jerr)) {
    if (err) *err = path.string() + ": " + jerr;
    return std::nullopt;
  }
  const litholattice::Json* sa = j.get("simAreaShape");
  const litholattice::Json* st = j.get("streamAreaShape");
  const litholattice::Json* tr = j.get("tickRadius");
  if (!sa && !st && !tr) return std::nullopt;  // v1/v2 metadata: no area (err stays empty)
  WorldAreaMeta a;
  std::string warn;
  if (sa && sa->type == litholattice::Json::Type::String) {
    bool ok = true;
    a.sim = litholattice::parseAreaShape(sa->str, &ok);
    if (!ok) warn += " unknown simAreaShape '" + sa->str + "' (using diamond);";
  } else if (sa) warn += " simAreaShape is not a string (using diamond);";
  if (st && st->type == litholattice::Json::Type::String) {
    bool ok = true;
    a.stream = litholattice::parseAreaShape(st->str, &ok, /*allowRound=*/false);
    if (!ok) warn += " unknown streamAreaShape '" + st->str + "' (using diamond);";
  } else if (st) warn += " streamAreaShape is not a string (using diamond);";
  if (tr && tr->type == litholattice::Json::Type::Number && tr->num >= 0 && tr->num <= 512) a.tickRadius = int(tr->num);
  else if (tr) warn += " unusable tickRadius (using 8);";
  if (!warn.empty() && err) *err = path.string() + ":" + warn;
  return a;
}

// Archie Phase 3c: legacy cWorldStore / mutex path removed. Public entry points always LF.
cWorldSave::LoadedColumn cWorldSave::loadColumn(const std::string& dim, int cx, int cz) {
  return loadColumnLf(dim, cx, cz);
}

size_t cWorldSave::saveDimension(const std::string& dim, const ChunkMap& world, const cSignalSim& signal, bool wait,
                                const std::unordered_set<uint64_t>* dirty) {
  // Capture under caller's sim window, then LF commit (Archie Phase 3c).
  std::unique_ptr<cWorldSnapshot> snap = cWorldSnapshot::capture(dim, world, signal, dirty);
  return commit(*snap, wait);
}
size_t cWorldSave::commit(cWorldSnapshot& snap, bool wait) {
  return commitLf(snap, wait);
}

size_t cWorldSave::saveColumns(const std::string& dim, const std::vector<const ChunkColumn*>& cols,
                              const cSignalSim& signal, bool wait) {
  return saveColumnsLf(dim, cols, signal, wait);
}

int cWorldSave::loadDimension(const std::string& dim, ChunkMap& world, std::vector<uint8_t>& signalState) {
  return loadDimensionLf(dim, world, signalState);
}

storage::cLfWorldStore& cWorldSave::lfStore(const std::string& dim) {
  for (auto& e : lfStores_.slot) {
    LfStoreEntry* p = e.load(std::memory_order_acquire);
    if (!p) {
      auto* fresh = new LfStoreEntry{dim, std::make_unique<storage::cLfWorldStore>(root_ / dim, payloads_)};
      if (e.compare_exchange_strong(p, fresh, std::memory_order_acq_rel)) return *fresh->store;
      delete fresh;  // another thread published this slot first; p is theirs
    }
    if (p->dim == dim) return *p->store;
  }
  throw std::runtime_error("save: more than 32 dimensions open on the lock-free store");
}

uint64_t cWorldSave::lfLoadsJumped() {
  uint64_t n = 0;
  for (auto& e : lfStores_.slot)
    if (LfStoreEntry* p = e.load(std::memory_order_acquire)) n += p->store->loadsJumped();
  return n;
}

size_t cWorldSave::commitLf(cWorldSnapshot& snap, bool wait) {  // 2026-11-01 LL_LF_SAVE twin of commit()
  const std::string dim = snap.dimension();
  lastSaveOk_ = true;
  lastSaveError_.clear();
  // C-4 fail closed, BEFORE anything is written: a present-but-unreadable
  // name table (readStableRemap already attempted crash recovery) plus
  // region data for this dimension means this save would interpret — and
  // overwrite — a world we cannot read back. Refuse it whole: no new table,
  // no region writes, no metadata (Game::saveWorld also skips player.lat).
  {
    std::string nerr;
    if (!storage::readStableRemap(root_ / "blocks.names", blockRegistry(), &nerr) && regionDataExists(root_, dim)) {
      lastSaveOk_ = false;
      lastSaveError_ = "refusing to overwrite '" + dim + "': block name table unreadable and region data exists: " + nerr;
      logError("save: %s", lastSaveError_.c_str());
      return 0;
    }
  }
  auto state = snap.takeSignalState();  // captured by cWorldSnapshot (same copy rules as before)
  std::vector<std::unique_ptr<ChunkColumn>> copies = snap.takeColumns();
  const size_t n = copies.size();
  std::string nerr;
  if (!storage::writeBlockNames(root_ / "blocks.names", blockRegistry(), &nerr)) {
    // The table interprets everything written below; saving regions under a
    // table that did not land would strand them. Refuse the whole save.
    lastSaveOk_ = false;
    lastSaveError_ = nerr;
    logError("save: %s (save refused: block name table not written)", nerr.c_str());
    return 0;
  }
  // SV-H2: persist the world seed (+ format version) on every save, through
  // the same contract. Hooked here (not only in Game::saveWorld) so every
  // cWorldSave caller — game, tests, tools — leaves a self-describing world.
  if (hasSeed_ && !writeWorldMeta(&nerr)) {
    lastSaveOk_ = false;
    lastSaveError_ = nerr;
    logError("save: %s (save refused: world metadata not written)", nerr.c_str());
    return 0;
  }
  storage::cLfWorldStore& s = lfStore(dim);
  const std::shared_ptr<void> ctx(state);  // the SGNL export rides every save job (encode context)
  for (auto& c : copies) s.saveAsync(std::move(c), ctx);
  if (wait) {
    // SV-H1: a synchronous save must know whether the regions landed.
    if (!s.flush()) {
      lastSaveOk_ = false;
      lastSaveError_ = "region flush failed for '" + dim + "' (" + std::to_string(s.lastFlushFailureCount()) +
                       " region(s); pending records kept for retry)";
      logError("save: %s", lastSaveError_.c_str());
    }
  } else s.flushAsync();
  return n;
}

size_t cWorldSave::saveColumnsLf(const std::string& dim, const std::vector<const ChunkColumn*>& cols,
                              const cSignalSim& signal, bool wait) {
  // A2 / B-M1 save-on-evict: saveDimension's discipline, scoped to the
  // eviction candidates. Same fail-closed name-table gate, same
  // table/metadata-first ordering, same exporting_ enqueue protocol —
  // a column is only reported saved when this whole path succeeded, and
  // the caller must not evict a column this call did not save.
  lastSaveOk_ = true;
  lastSaveError_.clear();
  {
    std::string nerr;
    if (!storage::readStableRemap(root_ / "blocks.names", blockRegistry(), &nerr) && regionDataExists(root_, dim)) {
      lastSaveOk_ = false;
      lastSaveError_ = "refusing to save eviction columns in '" + dim + "': block name table unreadable and region data exists: " + nerr;
      logError("save: %s", lastSaveError_.c_str());
      return 0;
    }
  }
  // SGNL export happens NOW — before the caller unscans the columns —
  // and is filtered to exactly the columns being saved.
  auto state = std::make_shared<std::map<uint64_t, std::vector<uint8_t>>>();
  {
    const auto all = signal.exportState();
    for (const ChunkColumn* col : cols) {
      if (!col) continue;
      const uint64_t key = ChunkKey::make(col->cx, col->cz);
      if (const auto it = all.find(key); it != all.end()) state->emplace(key, it->second);
    }
  }
  std::vector<std::unique_ptr<ChunkColumn>> copies;
  for (const ChunkColumn* col : cols) {
    if (!col || col->edgeShell) continue;  // Q12: never save a demoted shell over real data
    auto c = std::make_unique<ChunkColumn>();
    c->cx = col->cx; c->cz = col->cz;
    for (size_t s = 0; s < col->subs.size(); ++s)
      if (col->subs[s] && col->subs[s]->nonAir) c->subs[s] = std::make_unique<SubChunk>(*col->subs[s]);
    c->materialPalette = col->materialPalette;  // interned handle: shared, not copied
    c->cellText = col->cellText;  // 2026-10-25 Posts: panel text (PTXT payload)
    copies.push_back(std::move(c));
  }
  const size_t n = copies.size();
  if (n == 0) return 0;
  std::string nerr;
  if (!storage::writeBlockNames(root_ / "blocks.names", blockRegistry(), &nerr)) {
    lastSaveOk_ = false;
    lastSaveError_ = nerr;
    logError("save: %s (eviction-column save refused: block name table not written)", nerr.c_str());
    return 0;
  }
  if (hasSeed_ && !writeWorldMeta(&nerr)) {
    lastSaveOk_ = false;
    lastSaveError_ = nerr;
    logError("save: %s (eviction-column save refused: world metadata not written)", nerr.c_str());
    return 0;
  }
  storage::cLfWorldStore& s = lfStore(dim);
  const std::shared_ptr<void> ctx(state);  // the SGNL export rides every save job (encode context)
  for (auto& c : copies) s.saveAsync(std::move(c), ctx);
  if (wait) {
    if (!s.flush()) {
      lastSaveOk_ = false;
      lastSaveError_ = "region flush failed for '" + dim + "' (" + std::to_string(s.lastFlushFailureCount()) +
                       " region(s); pending records kept for retry)";
      logError("save: %s", lastSaveError_.c_str());
    }
  } else s.flushAsync();
  return n;
}

cWorldSave::LoadedColumn cWorldSave::loadColumnLf(const std::string& dim, int cx, int cz) {
  LoadedColumn out;
  if (!std::filesystem::exists(root_ / dim)) return out;
  const storage::StableRemap* remap = lfRemap_.p.load(std::memory_order_acquire);
  if (!remap) {
    std::string nerr;
    auto r = storage::readStableRemap(root_ / "blocks.names", blockRegistry(), &nerr);
    if (!r) {  // C-4 / B-M3: fail closed per column, re-attempted next call (no latch)
      logError("save '%s': %s; FAIL CLOSED: streamed column %d,%d is UNREADABLE (name table unreadable; not generated over)", dim.c_str(), nerr.c_str(), cx, cz);
      out.status = ColumnLoad::Unreadable;
      return out;
    }
    auto* fresh = new storage::StableRemap(std::move(*r));
    const storage::StableRemap* expect = nullptr;
    if (lfRemap_.p.compare_exchange_strong(expect, fresh, std::memory_order_acq_rel)) {
      if (fresh->legacy) logInfo("save '%s': no block name table (pre-table save); streamed ids taken as the current order", dim.c_str());
      for (const std::string& u : fresh->unknown) logError("save '%s': unknown block '%s' loads as air", dim.c_str(), u.c_str());
      remap = fresh;
    } else {
      delete fresh;
      remap = expect;
    }
  }
  std::string err;
  std::unique_ptr<ChunkColumn> col = lfStore(dim).load(cx, cz, &err, &out.signal);  // waits for itself only
  if (!col) {
    out.signal.clear();
    if (!err.empty()) {
      logError("save '%s': %s (column %d,%d UNREADABLE: not generated over)", dim.c_str(), err.c_str(), cx, cz);
      out.status = ColumnLoad::Unreadable;
    }
    return out;
  }
  std::string rerr;
  if (!storage::remapColumn(*col, *remap, &rerr)) {
    logError("save '%s': %s (column %d,%d UNREADABLE: not generated over)", dim.c_str(), rerr.c_str(), cx, cz);
    out.signal.clear();
    out.status = ColumnLoad::Unreadable;
    return out;
  }
  out.status = ColumnLoad::Loaded;
  out.column = std::move(col);
  return out;
}

int cWorldSave::loadDimensionLf(const std::string& dim, ChunkMap& world, std::vector<uint8_t>& signalState) {
  if (!std::filesystem::exists(root_ / dim)) return 0;
  std::string nerr;
  const auto remap = storage::readStableRemap(root_ / "blocks.names", blockRegistry(), &nerr);
  if (!remap) {
    // C-4 fail closed (recovery was already attempted inside the read):
    // with region data present, refuse the load outright (-1) — the caller
    // must not present generated terrain as this world, and saveDimension
    // will refuse to overwrite it. With no region data there is nothing to
    // lose, so an unreadable table alone does not block a fresh world.
    if (regionDataExists(root_, dim)) {
      logError("save '%s': %s; FAIL CLOSED: load refused (block name table unreadable, region data present; nothing loaded, nothing will be overwritten)", dim.c_str(), nerr.c_str());
      return -1;
    }
    logError("save '%s': %s; not loading (no region data present)", dim.c_str(), nerr.c_str());
    return 0;
  }
  if (remap->legacy) logInfo("save '%s': no block name table (pre-table save); ids taken as the current order, table written on the next save", dim.c_str());
  for (const std::string& u : remap->unknown) logError("save '%s': unknown block '%s' loads as air", dim.c_str(), u.c_str());
  storage::cLfWorldStore& s = lfStore(dim);
  std::vector<std::shared_ptr<storage::cLfWorldStore::LoadReq>> reqs;
  for (const auto& [k, col] : world.columns()) {
    auto r = std::make_shared<storage::cLfWorldStore::LoadReq>();
    s.loadAsync(r, col->cx, col->cz);
    reqs.push_back(std::move(r));
  }
  std::vector<uint8_t> imported;  // concatenated in request order (= the old completion order)
  int inserted = 0, bad = 0;
  for (auto& r : reqs) {
    storage::cLfWorldStore::waitFlag(r->done);
    if (!r->column) {
      if (!r->err.empty()) { ++bad; logError("save '%s': %s (column keeps generated terrain)", dim.c_str(), r->err.c_str()); }
      continue;
    }
    imported.insert(imported.end(), r->sink.begin(), r->sink.end());
    std::string rerr;
    if (!storage::remapColumn(*r->column, *remap, &rerr)) {
      logError("save '%s': %s (column dropped, generated terrain kept)", dim.c_str(), rerr.c_str());
      continue;
    }
    world.insert(std::move(r->column));
    ++inserted;
  }
  signalState = std::move(imported);
  if (bad) logError("save '%s': %d columns unreadable", dim.c_str(), bad);
  return inserted;
}

bool cWorldSave::flush() {
  bool ok = true;
  for (auto& e : lfStores_.slot)
    if (LfStoreEntry* p = e.load(std::memory_order_acquire)) ok = p->store->flush() && ok;
  return ok;
}

namespace {
constexpr uint32_t kPlayerMagic = 0x4D504C4C;  // "LLPM"

// P2b-1: the stable string id a stack persists under in player.lat v4.
// Registered items use their ItemDef string id; block-backed stacks use
// the block's namespaced id (the same namespace blocks.names remaps world
// blocks through, aliases included). An id that resolves to NEITHER (a
// mod item removed since, corrupt entity data) writes nameless: the load
// side drops that one stack with a log line and keeps everything else.
std::string stackStringId(const ItemStack& s) {
  if (s.empty()) return {};
  if (const ItemDef* d = findItem(s.itemId)) return std::string(d->stringId);
  if (s.itemId > 0 && s.itemId < blockRegistry().size()) return blockRegistry().idOf(BlockId(s.itemId));
  return {};
}

// Load-side inverse of stackStringId: item string ids first, then block
// string ids (findById resolves rename aliases). nullopt = unknown.
std::optional<uint32_t> resolveStackStringId(const std::string& sid) {
  if (sid.empty()) return std::nullopt;
  if (const ItemDef* d = findItemByStringId(sid)) return d->itemId;
  if (const auto b = blockRegistry().findById(sid)) return uint32_t(*b);
  return std::nullopt;
}
}  // namespace

bool cWorldSave::writePlayer(const PlayerMeta& m, std::string* err) const {
  // C-L1 (P3-2) write-side count guards: the on-disk counts are u8 (hotbar)
  // and u16 (stats / vitals / inventory slots). The old code cast silently,
  // so an oversized in-memory collection would have wrapped its count and
  // written a file that reads back shifted/truncated. Refuse loudly
  // instead — no file is touched.
  if (m.hot < 0 || m.hot > 0xFF) {
    if (err) *err = "player.lat: selected hotbar slot " + std::to_string(m.hot) + " out of u8 range; refusing to write";
    return false;
  }
  if (m.hotbar.size() > 0xFF) {
    if (err) *err = "player.lat: hotbar has " + std::to_string(m.hotbar.size()) + " slots (max 255); refusing to write";
    return false;
  }
  if (m.stats.size() > 0xFFFF || m.vitals.size() > 0xFFFF) {
    if (err) *err = "player.lat: stat/vital count exceeds 65535; refusing to write";
    return false;
  }
  if (m.inventory.allSlots().size() > 0xFFFF) {
    if (err) *err = "player.lat: inventory slot count exceeds 65535; refusing to write";
    return false;
  }
  storage::ByteWriter w;
  w.u16(PlayerMeta::kVersion);
  auto f32 = [&](float v) { uint32_t u; std::memcpy(&u, &v, 4); w.u32(u); };
  f32(m.pos.x); f32(m.pos.y); f32(m.pos.z); f32(m.yaw); f32(m.pitch);
  w.bytes(storage::Bytes(m.dimension.begin(), m.dimension.end()));
  w.u8(uint8_t(m.hot));
  // v4 hotbar (Appendix C M2): STRING ids, one per slot ("" = empty), so a
  // rename or a registration reorder can never silently repoint a slot at
  // a different block the way the v1–v3 raw u16 ids could.
  w.u8(uint8_t(m.hotbar.size()));
  for (BlockId b : m.hotbar) {
    const std::string sid = b == kAir ? std::string() : blockRegistry().idOf(b);
    w.bytes(storage::Bytes(sid.begin(), sid.end()));
  }
  // v2 stat block: name -> base value pairs (std::map order = name order,
  // so the bytes are deterministic).
  w.u16(uint16_t(m.stats.size()));
  for (const auto& [name, value] : m.stats) {
    w.bytes(storage::Bytes(name.begin(), name.end()));
    w.u32(uint32_t(value));  // int32 bit pattern
  }
  // v3 vitals block: same encoding, name -> value pairs.
  w.u16(uint16_t(m.vitals.size()));
  for (const auto& [name, value] : m.vitals) {
    w.bytes(storage::Bytes(name.begin(), name.end()));
    w.u32(uint32_t(value));  // int32 bit pattern
  }
  // v4 inventory block (Appendix G H1): EVERY slot in the canonical flat
  // order (Inventory::allSlots: hotbar, offhands, main, gear), each as
  // (string id, count); empty slots are explicit ("" + 0), so the block
  // is self-describing and a dropped stack never shifts another slot.
  const auto slots = m.inventory.allSlots();
  w.u16(uint16_t(slots.size()));
  for (const ItemStack* s : slots) {
    const std::string sid = stackStringId(*s);
    w.bytes(storage::Bytes(sid.begin(), sid.end()));
    w.u16(s->empty() ? uint16_t(0) : s->count);
  }
  // v5 stack-data block (wiring pass B6/B7): only data-bearing slots are
  // listed, each as (flat slot index, name field, certificate ids). The
  // name is the Etchor name-field value (the stack's "name" field in the
  // tagged binary storage); certificates are content certificate ids.
  uint16_t nData = 0;
  for (const ItemStack* s : slots)
    if (!s->empty() && s->hasData()) ++nData;
  w.u16(nData);
  for (size_t i = 0; i < slots.size(); ++i) {
    const ItemStack* s = slots[i];
    if (s->empty() || !s->hasData()) continue;
    w.u16(uint16_t(i));
    w.bytes(storage::Bytes(s->customName.begin(), s->customName.end()));
    w.u16(uint16_t(s->certificates.size()));
    for (const std::string& c : s->certificates) w.bytes(storage::Bytes(c.begin(), c.end()));
  }
  // v7 prayer block (Cortana 2026-10-16, JD mealtime prayers): u32 prayerDay (world-clock day)
  // + u8 bitmask of the mealtime windows already prayed in that day. The v6 play clock is gone:
  // the day now comes from the world clock in level.json.
  w.u32(m.prayerDay);
  w.u8(uint8_t(m.prayerWindows & 0x7u));
  storage::ByteWriter file;
  file.u32(kPlayerMagic);
  file.u32(storage::crc32(w.out.data(), w.out.size()));
  file.bytes(w.out);
  // SV-H3: the standard file contract (fsync file + directory, backup
  // rotation) — not the old hand-rolled tmp+rename with no fsync and no
  // backup, where one torn write lost position, hotbar, stats and vitals.
  return storage::writeFileAtomic(root_ / kPlayerFileName, file.out, err);
}

std::optional<PlayerMeta> cWorldSave::readPlayer(std::string* err) const {
  // Cortana 2026-10-16 [GAME] (JD): the player save is player.lat ("litho algorithmic type").
  // A world saved before the rename only has the legacy player.dat: read it (same format, same
  // crash recovery); the next save writes player.lat, which wins from then on. The legacy file
  // is left in place untouched (never deleted), so an older build can still open the world.
  auto path = root_ / kPlayerFileName;
  if (!footprintExists(path)) {
    path = root_ / kLegacyPlayerFileName;
    if (!footprintExists(path)) return std::nullopt;  // never saved: absent, not corrupt (err stays empty)
    logInfo("save: loading legacy %s (the next save writes %s)", kLegacyPlayerFileName, kPlayerFileName);
  }
  {
    // SV-H3 crash recovery: a torn primary promotes the newest intact
    // backup generation instead of giving up. A footprint with no intact
    // generation is corrupt: nullopt + err (the CRC gate below stands).
    std::string rerr;
    if (!storage::recoverCrashedWrite(path, playerFileIntact, &rerr)) {
      if (err) *err = rerr.empty() ? "player.lat is corrupt" : rerr;
      return std::nullopt;
    }
  }
  storage::Bytes all;
  if (!storage::readFileAll(path, &all, err, kMaxPlayerBytes)) return std::nullopt;  // C-L1 size cap
  storage::ByteReader r(all.data(), all.size());
  const uint32_t magic = r.u32(), crc = r.u32();
  const storage::Bytes body = r.bytes();
  if (!r.ok() || magic != kPlayerMagic || storage::crc32(body.data(), body.size()) != crc) {
    if (err) *err = "player.lat is corrupt";
    return std::nullopt;
  }
  storage::ByteReader b(body.data(), body.size());
  PlayerMeta m;
  const uint16_t v = b.u16();
  // v1 = the original layout (no stat block); v2 adds the stat block; v3
  // adds the vitals block; v4 stores the hotbar as string ids and adds
  // the inventory block; v5 appends the stack-data block (per-stack name
  // field + certificate ids). Anything newer is a future format this
  // build cannot guess at — refuse it, never misread.
  if (v != 1 && v != 2 && v != 3 && v != 4 && v != 5 && v != 6 && v != PlayerMeta::kVersion) { if (err) *err = "player.lat version " + std::to_string(v) + " not supported"; return std::nullopt; }
  m.fileVersion = v;
  auto f32 = [&] { const uint32_t u = b.u32(); float x; std::memcpy(&x, &u, 4); return x; };
  m.pos.x = f32(); m.pos.y = f32(); m.pos.z = f32(); m.yaw = f32(); m.pitch = f32();
  const storage::Bytes dim = b.bytes();
  m.dimension.assign(dim.begin(), dim.end());
  m.hot = b.u8();
  const uint8_t n = b.u8();
  if (v >= 4) {
    // String-id hotbar: remapped through the block names on load (aliases
    // resolve renames, exactly like world blocks). An unknown name loads
    // as an empty slot, with a log line — never as a different block.
    for (uint8_t i = 0; i < n; ++i) {
      const storage::Bytes s = b.bytes();
      const std::string sid(s.begin(), s.end());
      if (sid.empty()) { m.hotbar.push_back(kAir); continue; }
      if (const auto id = blockRegistry().findById(sid)) m.hotbar.push_back(*id);
      else {
        logError("player.lat: unknown hotbar block '%s' loads as an empty slot", sid.c_str());
        m.hotbar.push_back(kAir);
      }
    }
  } else {
    // v1–v3: raw u16 BlockIds, taken as written (the pre-M2 behavior for
    // old files; Game range-checks them against the live registry).
    for (uint8_t i = 0; i < n; ++i) m.hotbar.push_back(b.u16());
  }
  if (v >= 2) {
    const uint16_t nStats = b.u16();
    for (uint16_t i = 0; i < nStats; ++i) {
      const storage::Bytes nm = b.bytes();
      const uint32_t raw = b.u32();
      int32_t value;
      std::memcpy(&value, &raw, 4);
      if (playerLatNameOk(b.ok(), nm)) m.stats[std::string(nm.begin(), nm.end())] = value;
    }
  }
  if (v >= 3) {
    const uint16_t nVitals = b.u16();
    for (uint16_t i = 0; i < nVitals; ++i) {
      const storage::Bytes nm = b.bytes();
      const uint32_t raw = b.u32();
      int32_t value;
      std::memcpy(&value, &raw, 4);
      if (playerLatNameOk(b.ok(), nm)) m.vitals[std::string(nm.begin(), nm.end())] = value;
    }
  }
  int writtenSlots = 0;  // B9 four-row: the inventory block's written length (names its layout; 0 = no block)
  std::vector<std::pair<int, int>> overflowMoved;  // C9: B9 main 32..63 raw flat -> the C9 flat it landed on
  if (v >= 4) {
    // Inventory block: (string id, count) per slot in the canonical flat
    // order. Unknown ids drop ONLY their own stack (logged); slots past
    // this build's layout are read and dropped the same way, so a longer
    // future block can never corrupt the slots that precede it.
    const uint16_t nSlots = b.u16();
    const auto slots = m.inventory.allSlots();
    // 2026-10-24: a block written with the 8-slot hotbar (50 slots) is remapped into the 16-slot layout.
    // B9 four-row (2026-11-12): a block written with the 4x8 main grid (58 slots) is remapped too:
    // hotbar, hands and main keep their positions, the gear block moves up past the 64-slot grid.
    // C9 (2026-11-12, JD: four rows of eight): the live layout is A9's 58 again. A B9 block (90: 4x16 main,
    // gear 82..89) keeps hotbar / hands / main 0..31 in place, moves gear to 50..57, and spills main 32..63
    // into free C9 slots (policy below); the untouched B9 file is kept as player.lat.b9-90slot.bak.
    writtenSlots = nSlots;
    if (nSlots != uint16_t(kTotalSlots) && (nSlots == uint16_t(kLegacyTotalSlots) || nSlots == uint16_t(kB9TotalSlots)))
      logInfo("player.lat: inventory written with %u slots; remapped into this build's %d-slot layout", unsigned(nSlots), kTotalSlots);
    if (nSlots == uint16_t(kB9TotalSlots)) {
      std::filesystem::path bak = path;
      bak += ".b9-90slot.bak";
      std::error_code ec;
      if (!std::filesystem::exists(bak, ec)) {
        std::filesystem::copy_file(path, bak, ec);
        if (ec) logWarn("player.lat: could not keep the B9 90-slot copy %s (%s)", bak.string().c_str(), ec.message().c_str());
        else logInfo("player.lat: B9 90-slot inventory; the untouched file is kept as %s", bak.string().c_str());
      }
    }
    std::vector<std::pair<int, ItemStack>> overflow;  // C9: (raw flat, stack) of B9 main slots 32..63
    for (uint16_t i0 = 0; i0 < nSlots; ++i0) {
      const int mapped = savedFlatToCurrent(int(nSlots), int(i0));
      const uint16_t i = uint16_t(mapped < 0 ? int(i0) : mapped);
      const storage::Bytes s = b.bytes();
      const uint16_t count = b.u16();
      if (!b.ok() || count == 0) continue;  // empty slot (explicit in v4)
      const std::string sid(s.begin(), s.end());
      const auto id = resolveStackStringId(sid);
      if (!id) {
        logError("player.lat: unknown item '%s' in inventory slot %u dropped on load (count %u)",
                 sid.c_str(), unsigned(i), unsigned(count));
        continue;
      }
      // Cortana 2026-10-15 1510 (GAME, Herominde H-03): clamp to the item's stack cap.
      const uint16_t cap = stackCapFor(*id);
      if (count > cap)
        logWarn("player.lat: slot %u '%s' count %u exceeds its stack cap %u; clamped", unsigned(i), sid.c_str(), unsigned(count), unsigned(cap));
      if (mapped == kSavedOverflow) { overflow.emplace_back(int(i0), ItemStack{*id, count > cap ? cap : count}); continue; }
      if (i < slots.size()) *slots[i] = ItemStack{*id, count > cap ? cap : count};
      else
        logError("player.lat: inventory slot %u ('%s' x%u) is beyond this build's layout; dropped",
                 unsigned(i), sid.c_str(), unsigned(count));
    }
    if (!overflow.empty()) {
      // C9 overflow policy (recorded in the notes): each B9 main 32..63 stack, in B9 slot order, takes the
      // first FREE C9 slot: main 18..49, then hotbar 0..15, then Left / Right Hand (16, 17). Nothing is
      // merged or overwritten. A stack with no free slot is logged and stays in player.lat.b9-90slot.bak.
      std::vector<int> order;
      for (int f = kMainFlatBase; f < kCarrySlots; ++f) order.push_back(f);
      for (int f = 0; f < kInvHotbarSlots; ++f) order.push_back(f);
      order.push_back(kInvHotbarSlots);
      order.push_back(kInvHotbarSlots + 1);
      size_t oi = 0;
      int moved = 0, lost = 0;
      for (auto& [raw, st] : overflow) {
        while (oi < order.size() && !slots[size_t(order[oi])]->empty()) ++oi;
        if (oi < order.size()) {
          *slots[size_t(order[oi])] = std::move(st);
          overflowMoved.emplace_back(raw, order[oi]);
          ++oi;
          ++moved;
        } else {
          ++lost;
          logError("player.lat: B9 main slot %d (item %u x%u) has no free C9 slot; not loaded (kept in player.lat.b9-90slot.bak)",
                   raw - kMainFlatBase, unsigned(st.itemId), unsigned(st.count));
        }
      }
      logInfo("player.lat: C9 32-slot main grid: %d B9 overflow stack(s) moved to free slots, %d without room", moved, lost);
    }
    m.hasInventory = true;
  }
  if (v >= 5) {
    // Stack-data block (B6/B7): (flat slot index, name field, certificate
    // ids) per entry. An entry naming an empty/dropped or out-of-layout
    // slot is dropped with a log line — data never reattaches to a
    // different stack, and one bad entry never costs the others.
    const uint16_t nData = b.u16();
    const auto dataSlots = m.inventory.allSlots();
    for (uint16_t i = 0; i < nData; ++i) {
      const uint16_t rawFlat = b.u16();
      int mappedD = writtenSlots > 0 ? savedFlatToCurrent(writtenSlots, int(rawFlat)) : int(rawFlat);
      if (mappedD == kSavedOverflow) {  // C9: data follows its overflow stack to the slot it landed on
        mappedD = -1;
        for (const auto& mv : overflowMoved)
          if (mv.first == int(rawFlat)) mappedD = mv.second;
      }
      const uint16_t flat = mappedD < 0 ? uint16_t(0xFFFF) : uint16_t(mappedD);
      const storage::Bytes nm = b.bytes();
      const uint16_t nCerts = b.u16();
      std::vector<std::string> certs;
      for (uint16_t j = 0; j < nCerts; ++j) {
        const storage::Bytes c = b.bytes();
        certs.emplace_back(c.begin(), c.end());
      }
      if (!b.ok()) break;
      if (flat >= dataSlots.size() || dataSlots[flat]->empty()) {
        logError("player.lat: stack data for slot %u dropped (slot empty or beyond this build's layout)",
                 unsigned(flat));
        continue;
      }
      dataSlots[flat]->customName.assign(nm.begin(), nm.end());
      dataSlots[flat]->certificates = std::move(certs);
    }
  }
  if (v == 6) {  // Cortana 2026-10-15 2255: v6 prayer block (play clock); migrated by Game
    const uint64_t lo = b.u32(), hi = b.u32();
    m.playTicks = lo | (hi << 32);
    m.prayerDay = b.u32();
    m.prayersToday = std::min<uint8_t>(b.u8(), 3);
  } else if (v >= 7) {  // Cortana 2026-10-16: v7 mealtime prayer block
    m.prayerDay = b.u32();
    m.prayerWindows = uint8_t(b.u8() & 0x7u);
    m.prayersToday = uint8_t(((m.prayerWindows >> 0) & 1u) + ((m.prayerWindows >> 1) & 1u) + ((m.prayerWindows >> 2) & 1u));
  }
  if (!b.ok()) { if (err) *err = "player.lat is truncated"; return std::nullopt; }
  // C-L1 (P3-2): require EXACT consumption. Trailing bytes after the last
  // block mean the file is not the format its version claims (a torn or
  // foreign tail the CRC happened to cover); the old check accepted them
  // silently. Refuse, never guess.
  if (!b.done()) { if (err) *err = "player.lat has trailing bytes after the last block"; return std::nullopt; }
  return m;
}

}  // namespace lithokraft
