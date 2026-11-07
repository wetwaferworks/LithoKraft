#include <chrono>
#include <unordered_set>
#include <deque>
#include <vector>
#include "lithokraft/content/randor_rng.h"  // 2026-10-31 (JD): Randor
#include "lithokraft/content/signal.h"
#include "lithokraft/content/blocks.h"  // PlayB: kRedirectorSelect (Routor)
#include "lithokraft/content/compiled_comp_table.h"  // 2026-11-01 Option A
#include <algorithm>
#include <climits>
#include "litholattice/core/log.h"
#include "litholattice/core/threading.h"  // PlayK: cJobPool component shards
#include "litholattice/storage/storage.h"
#include "litholattice/sim/det_sched.h"
#include "lithokraft/content/component.h"
#include "lithokraft/content/pulse_rules.h"  // 2026-11-01 PlayK
#include "lithokraft/content/repeator.h"  // 2026-10-31: chain colour

// PlayL2 / MCHPRS takeaways (JD 2026-11-06) â€” folded into levers 1-12, NOT a Redpiler port:
//  1. Changed-node flush only (PR #206-style): mark/prep/flood walk changedSrc_ + marked nets, never the
//     whole graph on quiet ticks (levers 1/3/6/7). Full Redpiler/Trace-in-time IR = option 16 via lithography (compile settled Trace into a chip + set clock) — NOT an L2 deliverable.
//  2. Precomputed connections: K2 compAdj_ + PlayL2 emitNet/emitIdx + dense Comp*; fanout is edges.
//  3. Decouple sim rate from remesh/UI: signal phase must not wait on mesh rebuild (lever 5/12).
//  4. Plot-thread isolation: ownership handoff / phases only â€” no new mutex, no second world model.
#include "lithokraft/content/trace_conn.h"  // 2026-11-01 PlayL: Trace connection states
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace lithokraft {

namespace {
// 2026-10-31 (JD): switch-to-Trace diagonal feed off by default (orthogonal only).
// 2026-11-01 (JD): direct component->component signal; Switch/Button feeds ANY adjacent component; Pulsor pulses ride Trace only. LL_DIRECT_COMP=0 reverts.
const bool kEditLocal = [] { const char* e = std::getenv("LL_EDIT_LOCAL"); return !(e && e[0]=='0' && e[1]=='\0'); }();
const bool kEditLocalCheck = [] { const char* e = std::getenv("LL_EDIT_LOCAL_CHECK"); return e && e[0]=='1'; }();
const bool kDirectComp = [] { const char* e = std::getenv("LL_DIRECT_COMP"); return !(e && e[0] == '0' && e[1] == '\0'); }();
const bool kSwitchDiagonal = [] { const char* e = std::getenv("LL_SIGNAL_SWITCH_DIAGONAL"); return e && e[0] == '1' && e[1] == '\0'; }();
constexpr int kH[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int k6[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
}  // namespace

int sixbitCode(char32_t c) {
  if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
  return (c >= 32 && c <= 95) ? int(c) - 32 : -1;
}
char sixbitChar(int code) { return (code >= 0 && code < 64) ? char(code + 32) : '?'; }

std::map<uint64_t, std::vector<uint8_t>> cSignalSim::exportState() const {
  std::map<int64_t, const Comp*> sorted;  // deterministic order
  for (const auto& [k, c] : comps_) sorted.emplace(k, &c);
  std::map<uint64_t, storage::ByteWriter> cols;
  for (const auto& [k, c] : sorted) {
    int x, y, z; unpackPos(k, x, y, z);
    storage::ByteWriter& w = cols[ChunkKey::make(x >> 4, z >> 4)];
    const ComponentMemory& m = c->mem;
    w.u64(uint64_t(k));
    w.u8(m.out); w.u8(m.out2); w.u8(m.pulse ? 1 : 0); w.u8(m.prevIn); w.u8(m.prevSide); w.u8(m.prevClk);
    w.u8(m.history); w.u8(m.timer); w.u8(m.channel); w.u8(m.beacon ? 1 : 0); w.u16(m.historyHi);
    w.u16(uint16_t(std::min<size_t>(m.keys.size(), 65535)));
    for (size_t i = 0; i < m.keys.size() && i < 65535; ++i) { w.u8(m.keys[i].first); w.u8(m.keys[i].second); }
  }
  std::map<uint64_t, std::vector<uint8_t>> out;
  for (auto& [key, w] : cols) out[key] = std::move(w.out);
  return out;
}

bool cSignalSim::importState(const std::vector<uint8_t>& b) {
  struct Entry { int64_t key; ComponentMemory mem; };
  std::vector<Entry> entries;
  storage::ByteReader r(b.data(), b.size());
  while (r.ok() && !r.done()) {
    Entry e;
    e.key = int64_t(r.u64());
    ComponentMemory& m = e.mem;
    // SW-H2 trust boundary: a migrated / older-version / hand-edited (CRC-valid)
    // SGNL payload can carry outputs up to 255, but strengths are 1..kMaxSignal
    // and the BFS bucket arrays are sized kMaxSignal + 1. Clamp TO kMaxSignal
    // (never zero) so an imported output can never index a bucket out of bounds.
    m.out = uint8_t(std::min<int>(r.u8(), kMaxSignal)); m.out2 = uint8_t(std::min<int>(r.u8(), kMaxSignal));
    m.pulse = r.u8() != 0; m.prevIn = r.u8(); m.prevSide = r.u8(); m.prevClk = r.u8();
    m.history = r.u8(); m.timer = r.u8(); m.channel = r.u8(); m.beacon = r.u8() != 0; m.historyHi = r.u16();
    const uint16_t n = r.u16();
    for (uint16_t i = 0; i < n && r.ok(); ++i) { const uint8_t f = r.u8(); m.keys.emplace_back(f, r.u8()); }
    entries.push_back(std::move(e));
  }
  if (!r.ok()) return false;
  fullPass_ = true;
  for (Entry& e : entries)
    if (auto it = comps_.find(e.key); it != comps_.end()) it->second.mem = std::move(e.mem);  // component still there
  return true;
}

namespace {
// Index every signal cell of ONE column (shared by scan() and scanColumn()).

// Archie Phase 4: emitMode 1 = front? out : 0; else virtual (mode 0 = PlayJ path).
inline int compEmit(const cCompBehavior* behavior, const ComponentMemory& mem, uint8_t emitMode, int dir, CellState st) {
  if (emitMode) return (dir == stFacing(st)) ? int(mem.out) : 0;
  return behavior->emitToward(dir, st, mem);
}
inline int compEmitChan(const cCompBehavior* behavior, const ComponentMemory& mem, uint8_t traitFlags, int dir, CellState st) {
  if (traitFlags & cCompTraits::kEmitChan0) return 0;
  return behavior->emitChannel(dir, st, mem);
}
void scanOneColumn(cSignalSim& sim, const ChunkColumn& col, const cBlockReg& reg) {
  std::vector<BlockId> lut;  // StableId palette -> BlockId, resolved once per sub-chunk
  for (int sy = 0; sy < kSubChunks; ++sy) {
    const SubChunk* s = col.subs[sy].get();
    if (!s) continue;
    const bool w64 = s->blocks.width() == VoxelPalette::Width::W64;
    if (!w64) s->blocks.resolveBlockIds(reg, lut);
    for (int i = 0; i < kSubChunkVolume; ++i) {
      BlockId b;
      if (w64) {  // degenerate width: index IS the StableId (unreachable in practice)
        const auto id = reg.findByStableId(s->blocks.at(size_t(i)));
        b = id ? *id : kAir;
      } else {
        b = lut[s->blocks.rawIndex(size_t(i))];
      }
      if (!isComponent(b)) continue;
      const int x = col.cx * 16 + (i & 15), z = col.cz * 16 + ((i >> 4) & 15), y = sy * 16 + (i >> 8);
      sim.onBlockChanged(x, y, z, b);
    }
  }
}
}  // namespace

void cSignalSim::scan(const ChunkMap& w) {
  ++editGen_;  // PlayM2
  torches_.clear(); wires_.clear(); lamps_.clear(); comps_.clear(); ++compsGen_; mountFlipOk_ = false;
  const cBlockReg& reg = blockRegistry();
  // Archie PlayI: skip ONLY StagedSim (outside-sim columns parked until promote). In-sim columns
  // are Active from load (PlayI restore), so Examples in tickRadius scan/tick from the first tick.
  for (const auto& [k, col] : w.columns()) {
    if (col->state.stage() == litholattice::ChunkStage::StagedSim) continue;
    scanOneColumn(*this, *col, reg);
  }
}

void cSignalSim::scanColumn(const ChunkMap& w, int cx, int cz) {
  ++editGen_;  // PlayM2
  const ChunkColumn* col = w.column(cx, cz);
  if (!col) return;
  const bool wasValid = netsValid_;
  const bool edgeClean = edgeRelink_ && relinkNets_ && lazyNets_ && !topoDirty_ && netComps_.size() == nets_.size() && netLamps_.size() == nets_.size();
  scanOneColumn(*this, *col, blockRegistry());
  if (edgeClean && topoDirty_) {  // LL_SIGNAL_EDGERELINK: a streamed-in column re-floods only its neighbourhood (edgeSplit)
    topoDirty_ = false;
    edgeCols_.push_back((int64_t(cx) << 32) | uint32_t(cz));
    edgePending_ = true;
  }
  if (wasValid && !netsValid_) { diagCx_ = cx; diagCz_ = cz; diagKind_ = 5; diagOwn_ = true; }  // diag: load re-index
}

std::vector<uint8_t> cSignalSim::exportColumnState(int cx, int cz) const {
  const auto all = exportState();
  const auto it = all.find(ChunkKey::make(cx, cz));
  return it == all.end() ? std::vector<uint8_t>{} : it->second;
}

namespace {
bool keyInColumn(int64_t k, int cx, int cz) {
  int x, y, z;
  unpackPos(k, x, y, z);
  return (x >> 4) == cx && (z >> 4) == cz;
}
template <typename Set>
void eraseColumnKeys(Set& s, int cx, int cz) {
  for (auto it = s.begin(); it != s.end();) {
    if (keyInColumn(*it, cx, cz)) it = s.erase(it);
    else ++it;
  }
}
}  // namespace

bool cSignalSim::hasIndexedCells(int cx, int cz) const { return indexedCellCount(cx, cz) != 0; }

size_t cSignalSim::indexedCellCount(int cx, int cz) const {
  size_t n = 0;
  for (int64_t k : torches_) n += keyInColumn(k, cx, cz) ? 1 : 0;
  for (int64_t k : wires_) n += keyInColumn(k, cx, cz) ? 1 : 0;
  for (int64_t k : lamps_) n += keyInColumn(k, cx, cz) ? 1 : 0;
  for (const auto& [k, c] : comps_) n += keyInColumn(k, cx, cz) ? 1 : 0;
  return n;
}

void cSignalSim::unscanColumn(int cx, int cz) {
  ++editGen_;  // PlayM2
  // LL_SIGNAL_LAZYNETS: a column the valid nets never read (no indexed cell in it or beside it)
  // leaves the nets, their cached pointers and the incremental pass alone.
  const bool relevant = !lazyNets_ || !netsValid_ || netTouches(cx, cz);
  const bool ownCells = relinkNets_ && relevant && hasIndexedCells(cx, cz);  // LL_SIGNAL_RELINK only
  if (relevant && netsValid_) { diagCx_ = cx; diagCz_ = cz; diagKind_ = 2; diagOwn_ = hasIndexedCells(cx, cz); }
  const bool edgeDrop = ownCells && edgeRelink_ && lazyNets_ && !topoDirty_ && edgeMarkColumn(cx, cz);  // LL_SIGNAL_EDGERELINK
  // Removed wires keep a trace-strength slot in the buffer (slots are
  // never recycled); zero them so a stale strength can never be read for
  // a cell that no longer exists. A re-streamed column's wires get their
  // slots rewritten by the next flood's final-state replay.
  for (int64_t k : wires_)
    if (keyInColumn(k, cx, cz)) {
      const uint32_t slot = strengthBuf_.slotOf(uint64_t(k));
      if (slot != litholattice::TraceStrengthBuffer::kNoSlot) strengthBuf_.set(slot, 0);
    }
  eraseColumnKeys(torches_, cx, cz);
  eraseColumnKeys(wires_, cx, cz);
  eraseColumnKeys(lamps_, cx, cz);
  for (auto it = lampHold_.begin(); it != lampHold_.end();) {
    if (keyInColumn(it->first, cx, cz)) it = lampHold_.erase(it);
    else ++it;
  }
  for (auto it = lampDelay_.begin(); it != lampDelay_.end();) {  // PlayB lamp delay line
    if (keyInColumn(it->first, cx, cz)) it = lampDelay_.erase(it);
    else ++it;
  }
  // PlayJ: awake_ is epoch-flat; drop keys in this column from the live epoch list.
  {
    std::vector<int64_t> kept;
    kept.reserve(awakeKeys_.size());
    for (int64_t ak : awakeKeys_) if (!keyInColumn(ak, cx, cz)) kept.push_back(ak);
    awakeKeys_.swap(kept);
    for (int64_t ak : awakeKeys_) awakeStamp_[ak] = awakeEpoch_;
  }
  for (auto it = comps_.begin(); it != comps_.end();) {
    if (keyInColumn(it->first, cx, cz)) { it = comps_.erase(it); ++compsGen_; mountFlipOk_ = false; }
    else ++it;
  }
  std::erase_if(mountPowered_, [&](const auto& e) { return keyInColumn(e.first, cx, cz); });  // rebuilt per tick anyway; drop now
  mountFlipOk_ = false;
  std::erase_if(softPowered_, [&](const auto& e) { return keyInColumn(e.first, cx, cz); });   // PlayL: no soft level for gone cells
  // Per-tick scratch naming this column must not survive into the next
  // tick (the sim is stopped for eviction, so clearing is safe): pending
  // lamp recomputes and changed-source marks for cells that are gone.
  {
    std::vector<int64_t> kept;
    kept.reserve(dirtyLampKeys_.size());
    for (int64_t dk : dirtyLampKeys_) if (!keyInColumn(dk, cx, cz)) kept.push_back(dk);
    dirtyLampKeys_.swap(kept);
    for (int64_t dk : dirtyLampKeys_) dirtyLampStamp_[dk] = dirtyLampEpoch_;
  }
  std::erase_if(extraLamps_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  std::erase_if(changedSrc_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  std::erase_if(softChanged_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  if (!relevant) return;
  if (!relinkNets_ || (ownCells && !edgeDrop)) topoDirty_ = true;  // LL_SIGNAL_RELINK: a border-only evict keeps the flood
  srcReach_.clear(); netReach_.clear();  // PlayK: inverse reach dies with the nets
  fullPass_ = true;   // layout changed: nets and cached pointers are stale
  netsValid_ = false;
}

// LL_SIGNAL_EDGERELINK: called before the column's keys are erased, with the flood topology current
// (!topoDirty_). Records which nets lose wires; buildNets re-floods only those (edgeSplit).
bool cSignalSim::edgeMarkColumn(int cx, int cz) {
  if (netComps_.size() != nets_.size() || netLamps_.size() != nets_.size()) return false;
  edgeCols_.push_back((int64_t(cx) << 32) | uint32_t(cz));
  const auto t = std::lower_bound(netTouch_.begin(), netTouch_.end(), NetTouch{cx, cz, false},
                                  [](const NetTouch& a, const NetTouch& b) { return a.cx != b.cx ? a.cx < b.cx : a.cz < b.cz; });
  if (t != netTouch_.end() && t->cx == cx && t->cz == cz) { t->loaded = false; t->own = false; }  // a reload re-floods (scan edits)
  edgePending_ = true;
  return true;
}

// LL_SIGNAL_EDGERELINK: re-flood only the wires left in nets that lost wires to evicted columns, then
// re-order every net by its first (minimum) wire key, which is exactly the id order of a fresh flood.
// Links are not symmetric, so it refuses (full flood) when any link joins a remnant wire and a wire of
// another net in either direction: then a fresh flood could merge them differently.
bool cSignalSim::edgeSplit(ChunkMap& w) {
  // Affected: every net with a wire (current or just evicted; wireNet_ still names evicted wires) in a
  // touched column or one of its 8 neighbours. Seeds: their surviving wires plus wires no net owns yet
  // (the touched loads). The boundary test below still guards the result.
  auto colKey = [](int cx, int cz) { return (int64_t(cx) << 32) | uint32_t(cz); };
  std::unordered_set<int64_t> nearCols;
  for (int64_t c : edgeCols_) {
    const int cx = int(c >> 32), cz = int(int32_t(uint32_t(c)));
    for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx) nearCols.insert(colKey(cx + dx, cz + dz));
  }
  std::vector<char> isAff(nets_.size(), 0);
  for (const auto& [k, id] : wireNet_) {
    if (id < 0 || size_t(id) >= nets_.size()) return false;
    int x, y, z; unpackPos(k, x, y, z);
    if (nearCols.contains(colKey(x >> 4, z >> 4))) isAff[size_t(id)] = 1;
  }
  std::vector<int64_t> seeds;
  for (int64_t k : wires_) {
    const auto it = wireNet_.find(k);
    if (it == wireNet_.end() || isAff[size_t(it->second)]) seeds.push_back(k);
  }
  std::sort(seeds.begin(), seeds.end());
  const std::unordered_set<int64_t> rem(seeds.begin(), seeds.end());
  std::vector<int64_t> links, back;
  for (int64_t r : seeds) {  // boundary: no link between a remnant wire and any other wire, either way
    int x, y, z; unpackPos(r, x, y, z);
    links.clear(); wireLinks(w, x, y, z, links);
    for (int64_t n : links) if (wires_.contains(n) && !rem.contains(n)) return false;
    for (int dy = -1; dy <= 1; dy += 2) {
      const int64_t c = packPos(x, y + dy, z);
      if (!wires_.contains(c) || rem.contains(c)) continue;
      back.clear(); wireLinks(w, x, y + dy, z, back);
      if (std::find(back.begin(), back.end(), r) != back.end()) return false;
    }
    for (auto& d : kH)
      for (int dy = -1; dy <= 1; ++dy) {
        const int64_t c = packPos(x + d[0], y + dy, z + d[1]);
        if (!wires_.contains(c) || rem.contains(c)) continue;
        back.clear(); wireLinks(w, x + d[0], y + dy, z + d[1], back);
        if (std::find(back.begin(), back.end(), r) != back.end()) return false;
      }
  }
  struct Net { std::vector<int64_t> wires, lamps, comps; };
  std::vector<Net> all;
  all.reserve(nets_.size() + seeds.size());
  for (size_t i = 0; i < nets_.size(); ++i) {
    if (isAff[i]) continue;
    Net n{std::move(nets_[i]), std::move(netLamps_[i]), std::move(netComps_[i])};
    std::erase_if(n.lamps, [&](int64_t k) { return !lamps_.contains(k); });
    std::erase_if(n.comps, [&](int64_t k) { return !comps_.contains(k); });
    all.push_back(std::move(n));
  }
  std::unordered_set<int64_t> seen;
  std::vector<int64_t> stack;
  for (int64_t start : seeds) {  // same DFS as buildNets, over the remnant wires only
    if (!seen.insert(start).second) continue;
    Net n;
    std::unordered_set<int64_t> lampSet;
    stack.push_back(start);
    while (!stack.empty()) {
      const int64_t k = stack.back(); stack.pop_back();
      n.wires.push_back(k);
      int x, y, z; unpackPos(k, x, y, z);
      for (auto& d : k6) { const int64_t q = packPos(x + d[0], y + d[1], z + d[2]); if (lamps_.contains(q)) lampSet.insert(q); }
      links.clear(); wireLinks(w, x, y, z, links);
      for (int64_t q : links) if (rem.contains(q) && seen.insert(q).second) stack.push_back(q);
    }
    n.lamps.assign(lampSet.begin(), lampSet.end());
    std::sort(n.lamps.begin(), n.lamps.end());
    for (int64_t k : n.wires) {
      int x, y, z; unpackPos(k, x, y, z);
      for (auto& d : kH) if (const int64_t q = packPos(x + d[0], y, z + d[1]); comps_.contains(q)) n.comps.push_back(q);
      if (const int64_t q = packPos(x, y - 1, z); comps_.contains(q)) n.comps.push_back(q);
    }
    std::sort(n.comps.begin(), n.comps.end()); n.comps.erase(std::unique(n.comps.begin(), n.comps.end()), n.comps.end());
    all.push_back(std::move(n));
  }
  std::sort(all.begin(), all.end(), [](const Net& a, const Net& b) { return a.wires.front() < b.wires.front(); });
  nets_.clear(); netLamps_.clear(); netComps_.clear(); wireNet_.clear();
  nets_.reserve(all.size()); netLamps_.reserve(all.size()); netComps_.reserve(all.size()); wireNet_.reserve(wires_.size());
  for (Net& n : all) {
    const int32_t id = int32_t(nets_.size());
    for (int64_t k : n.wires) wireNet_[k] = id;
    nets_.push_back(std::move(n.wires)); netLamps_.push_back(std::move(n.lamps)); netComps_.push_back(std::move(n.comps));
  }
  ++edgeDrops_;
  return true;
}
bool cSignalSim::netTouches(int cx, int cz) const {
  const auto it = std::lower_bound(netTouch_.begin(), netTouch_.end(), NetTouch{cx, cz, false},
                                   [](const NetTouch& a, const NetTouch& b) { return a.cx != b.cx ? a.cx < b.cx : a.cz < b.cz; });
  return it != netTouch_.end() && it->cx == cx && it->cz == cz;
}

bool cSignalSim::netTouchUnchanged(const ChunkMap& w) const {
  for (const NetTouch& t : netTouch_)
    if ((w.column(t.cx, t.cz) != nullptr) != t.loaded) { diagCx_ = t.cx; diagCz_ = t.cz; diagOwn_ = t.own; diagKind_ = 1; return false; }
  return true;
}

void cSignalSim::onBlockChanged(int x, int y, int z, BlockId now) {
  ++editGen_;  // PlayM2
  const int64_t k = packPos(x, y, z);
  if (netsValid_) { diagKind_ = 6; diagCx_ = x >> 4; diagCz_ = z >> 4; diagOwn_ = true; }  // diag: edit (scan overrides to 5)
  const bool wasWire = wires_.contains(k);
  const SimRole r = roleOf(now);
  const bool nowWire = (r == SimRole::Wire);
  // PlayJ fix 4 (held from I2): wire membership change in one column uses edgeSplit machinery
  // (edgeMarkColumn) instead of topoDirty_ full rebuild when EDGERELINK+LAZYNETS+RELINK are clean.
  // Cross-column Trace links that fail the edgeSplit boundary test still fall back to a full flood
  // (do not worsen the known chunk-border Trace bug â€” refuse and rebuild).
  static const bool kColLocal = [] { const char* e = std::getenv("LL_COLUMN_LOCAL_NETS"); return !(e && e[0] == '0'); }();
  const bool wireEdit = wasWire || nowWire;
  const bool edgeOk = kColLocal && edgeRelink_ && relinkNets_ && lazyNets_ && !topoDirty_ && netsValid_ &&
                      wireEdit && netComps_.size() == nets_.size() && netLamps_.size() == nets_.size();
  fullPass_ = true; netsValid_ = false; editLocal_ = false;
  if (edgeOk) {
    edgeMarkColumn(x >> 4, z >> 4);
    // topoDirty_ stays false so buildNets takes the edgeSplit path
  } else {
    topoDirty_ = true;
  }
  torches_.erase(k); wires_.erase(k); lamps_.erase(k); comps_.erase(k); lampHold_.erase(k); lampDelay_.erase(k); ++compsGen_; mountFlipOk_ = false;
  if (r == SimRole::Torch) torches_.insert(k);
  else if (r == SimRole::Wire) wires_.insert(k);
  else if (r == SimRole::Lamp || r == SimRole::ColorLamp) lamps_.insert(k);
  else if (const cCompBehavior* b = cBehaviorCatalog::get().component(now)) {
        const cCompTraits& tr = cBehaviorCatalog::get().traits(now);
        Comp c{};
        c.behavior = b;
        c.traitFlags = tr.flags;
        c.quietMode = tr.quietMode();
        c.emitMode = tr.emitMode();
        c.delayFixed = tr.delayFixed;
        c.rate = classTickRate(now, b);  // PlayL Phase5: pack once
        // PlayL2 #9: tag ALU-stress hot types once at place.
        { const auto role = blockRegistry().def(now).role;
          c.hotKind = (role == SimRole::Repeator) ? 1
                    : (role == SimRole::Thresholdor) ? 2
                    : (role == SimRole::cFusor) ? 3
                    : (role == SimRole::Randor) ? 4 : 0; }
        syncCompWords(c);
        comps_[k] = c;
      }
}

// 2026-11-01 (JD placement spikes): a component / lamp / plain block edit with no wire in or under the cell leaves
// every wire link unchanged (wireLinks reads kCutsWire of a cell only for the wire directly below it), so the
// net flood is kept (relink) and only the readers of the touching nets are patched. LL_EDIT_LOCAL=0 kill.
void cSignalSim::onBlockChanged(int x, int y, int z, BlockId now, bool wireBelow) {
  ++editGen_;  // PlayM2
  const int64_t k = packPos(x, y, z);
  const SimRole r = roleOf(now);
  // 2026-11-01 (box): torch edits are local too - torches are never in a net, a reader list or a wire link
  // (only kCutsWire above a wire matters, and wireBelow excludes that); torches_ / srcReach_ / genSeeds_ are
  // rebuilt from torches_ every buildNets. LL_EDIT_LOCAL_TORCH=0: torch edits take the full path (old).
  static const bool torchLocal = [] { const char* e = std::getenv("LL_EDIT_LOCAL_TORCH"); return !(e && e[0] == '0'); }();
  const bool torchOk = torchLocal || (!torches_.contains(k) && r != SimRole::Torch);
  const bool canLocal = kEditLocal && !topoDirty_ && !wireBelow && !wires_.contains(k) && torchOk &&
                        r != SimRole::Wire && wireNet_.size() == wires_.size();
  if (!canLocal) { onBlockChanged(x, y, z, now); return; }
  if (netsValid_) { diagKind_ = 7; diagCx_ = x >> 4; diagCz_ = z >> 4; diagOwn_ = true; }  // diag: local edit
  fullPass_ = true; netsValid_ = false; editLocal_ = true;
  const bool wasComp = comps_.contains(k), wasLamp = lamps_.contains(k);
  lamps_.erase(k); comps_.erase(k); lampHold_.erase(k); lampDelay_.erase(k); torches_.erase(k); ++compsGen_; mountFlipOk_ = false;
  if (r == SimRole::Torch) torches_.insert(k);
  const bool isLamp = r == SimRole::Lamp || r == SimRole::ColorLamp;
  bool isComp = false;
  if (isLamp) lamps_.insert(k);
  else if (const cCompBehavior* b = cBehaviorCatalog::get().component(now)) {
        const cCompTraits& tr = cBehaviorCatalog::get().traits(now);
        Comp c{};
        c.behavior = b;
        c.traitFlags = tr.flags;
        c.quietMode = tr.quietMode();
        c.emitMode = tr.emitMode();
        c.delayFixed = tr.delayFixed;
        c.rate = classTickRate(now, b);  // PlayL Phase5: pack once
        // PlayL2 #9: tag ALU-stress hot types once at place.
        { const auto role = blockRegistry().def(now).role;
          c.hotKind = (role == SimRole::Repeator) ? 1
                    : (role == SimRole::Thresholdor) ? 2
                    : (role == SimRole::cFusor) ? 3
                    : (role == SimRole::Randor) ? 4 : 0; }
        syncCompWords(c);
        comps_[k] = c;
        isComp = true;
      }
  auto patch = [&](std::vector<std::vector<int64_t>>& lists, int64_t wk, bool add) {
    const auto it = wireNet_.find(wk);
    if (it == wireNet_.end() || size_t(it->second) >= lists.size()) return;
    std::vector<int64_t>& v = lists[size_t(it->second)];
    const auto p = std::lower_bound(v.begin(), v.end(), k);
    const bool has = p != v.end() && *p == k;
    if (add && !has) v.insert(p, k); else if (!add && has) v.erase(p);
  };
  if (wasComp || isComp) {  // readers: wire beside (same y) or on top
    for (auto& d : kH) if (const int64_t n = packPos(x + d[0], y, z + d[1]); wires_.contains(n)) patch(netComps_, n, false);
    if (const int64_t n = packPos(x, y + 1, z); wires_.contains(n)) patch(netComps_, n, false);
    if (isComp) {
      for (auto& d : kH) if (const int64_t n = packPos(x + d[0], y, z + d[1]); wires_.contains(n)) patch(netComps_, n, true);
      if (const int64_t n = packPos(x, y + 1, z); wires_.contains(n)) patch(netComps_, n, true);
    }
  }
  if (wasLamp || isLamp)
    for (auto& d : k6) if (const int64_t n = packPos(x + d[0], y + d[1], z + d[2]); wires_.contains(n)) patch(netLamps_, n, isLamp);
}

// Minecraft-style strong power: a lit torch (either colour) powers the solid opaque block above it.
// An attached (wall / floor / ceiling) switch that is on strongly powers its support block.
// (Attached switches / buttons are found once per pass by collectMountPowered, not per call.)
// 2026-11-01 PlayL (JD 7:43): hard power carries strength: torch below = kMaxSignal, else the
// strongest component / switch feeding the block this tick (mountPowered_ value, max-reduced).
int cSignalSim::hardPower(const ChunkMap& w, int x, int y, int z) const {
  const auto& reg = blockRegistry();
  if (!reg.hasAll(w.get(x, y, z), kSolid | kOpaque | kPowerable)) return 0;
  if (reg.def(w.get(x, y - 1, z)).role == SimRole::Torch && stValue(w.getState(x, y - 1, z)) > 0) return kMaxSignal;
  if (mountPowered_.empty()) return 0;
  const auto it = mountPowered_.find(packPos(x, y, z));
  return it == mountPowered_.end() ? 0 : int(it->second);
}

bool cSignalSim::strongPowered(const ChunkMap& w, int x, int y, int z) const { return hardPower(w, x, y, z) > 0; }

void cSignalSim::collectMountPowered(const ChunkMap& w) {
  // B-L1 acceptance (P3-2): result is a membership SET (mountPowered_); visit order cannot leak.
  // PlayL2: walk dense Comp* (sorted keys) instead of unordered comps_ hashing.
  const auto m0 = std::chrono::steady_clock::now();
  rebuildCompDense();
  mountPowered_.clear();
  const size_t n = compAllKeys_.size();
  // PlayM2 incremental mount (LL_MOUNT_INCR=0 restores the full walk): only candidates are visited.
  static const bool kIncr = [] { const char* e = std::getenv("LL_MOUNT_INCR"); return !(e && e[0] == '0'); }();
  const bool hardAny = cPulseRules::hardAnyComponent();
  if (kIncr && (mountCandGen_ != editGen_ || mountCandN_ != n || mountCandPtr_ != compAllKeys_.data() || mountCandHard_ != hardAny)) {
    mountCand_.clear();
    const auto& reg = blockRegistry();
    for (size_t i = 0; i < n; ++i) {
      int x, y, z; unpackPos(compAllKeys_[i], x, y, z);
      const BlockId b = w.get(x, y, z);
      const auto r = reg.def(b).role;
      bool cand = r == uint16_t(SimRole::Switch) || r == uint16_t(SimRole::cButton);
      if (!cand && (hardAny || r == uint16_t(SimRole::Repeator) || r == uint16_t(SimRole::Pulsor) || b == kRedirectorSelect))
        for (int d = 0; d < 4 && !cand; ++d) cand = reg.hasAll(w.get(x + kDirX[d], y, z + kDirZ[d]), kSolid | kOpaque | kPowerable);
      if (cand) mountCand_.push_back(uint32_t(i));
    }
    mountCandGen_ = editGen_; mountCandN_ = n; mountCandPtr_ = compAllKeys_.data(); mountCandHard_ = hardAny;
  }
  const size_t nIter = kIncr ? mountCand_.size() : n;
  for (size_t ii = 0; ii < nIter; ++ii) {
    const size_t i = kIncr ? size_t(mountCand_[ii]) : ii;
    const int64_t k = compAllKeys_[i];
    Comp& c = *compAllPtrs_[i];
    int x, y, z; unpackPos(k, x, y, z);
    const CellState s = w.getState(x, y, z);
    if (!stAttach(s) || !stOn(s)) continue;
    if (const auto r = blockRegistry().def(w.get(x, y, z)).role; r != uint16_t(SimRole::Switch) && r != uint16_t(SimRole::cButton)) continue;
    int ox, oy, oz;
    attachOffset(stAttach(s), ox, oy, oz);
    mountPowered_[packPos(x + ox, y + oy, z + oz)] = uint8_t(kMaxSignal);
  }
  // A lit Repeator hard-powers the solid conductive block it faces (not glass / ceramic: they are
  // not powerable), so wire and components on that block's other faces pick the signal up.
  for (size_t ii = 0; ii < nIter; ++ii) {
    const size_t i = kIncr ? size_t(mountCand_[ii]) : ii;
    const int64_t k = compAllKeys_[i];
    Comp& c = *compAllPtrs_[i];
    if (!(c.faceWord & 0xFFFFu) && !c.mem.out && !c.mem.out2) continue;  // PlayL2 #8: faceWord out|out2
    int x, y, z; unpackPos(k, x, y, z);
    if (!c.mem.out && w.get(x, y, z) != kRedirectorSelect) continue;  // PlayB: a Routor's second path hard-powers too
    // 2026-11-01 PlayL (JD, LL_HARD_ANY): any component output facing the block hard-powers it, not only a Repeator.
    if (const auto hr = blockRegistry().def(w.get(x, y, z)).role; !cPulseRules::hardAnyComponent() && hr != uint16_t(SimRole::Repeator) &&
        hr != uint16_t(SimRole::Pulsor) && w.get(x, y, z) != kRedirectorSelect) continue;  // PlayB: Pulsor / Routor always hard-power
    const CellState s = w.getState(x, y, z);
    for (int d = 0; d < 4; ++d) {
      const int es = compEmit(c.behavior, c.mem, c.emitMode, d, s);
      if (es <= 0) continue;
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      if (!blockRegistry().hasAll(w.get(nx, y, nz), kSolid | kOpaque | kPowerable)) continue;
      uint8_t& hv = mountPowered_[packPos(nx, y, nz)];  // strongest feeder wins (order-independent)
      hv = uint8_t(std::max(int(hv), std::min(es, int(kMaxSignal))));
    }
  }
  mountFlipOk_ = true;
  phase_.mount += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m0).count();
}


// 2026-11-01 (JD soft power; PlayL 7:45 same-tick): a lit flat Trace soft-powers every solid powerable
// block it touches on a CONNECTED side (either colour). Soft never feeds Trace; it lights attached lamps
// and feeds components reading that block. Strength = max over touching Trace. Hard wins at read time.
// Called AFTER this tick's flood and BEFORE lamps, so soft reflects this tick's wire values (no lag).
// Lazy: incremental cycles revisit only blocks touching wires of nets re-flooded this tick (plus edits),
// ascending key order; full passes rebuild. Changed levels add their lamps to this tick's lamp list and
// wake the components around them. LL_SOFT_POWER=0: off.
namespace {  // 2026-11-01 PlayM (measure only, LL_TICK_PROFILE): what component sleep's wake bookkeeping costs
struct WakeProf { double ms = 0; uint64_t around = 0, ticks = 0, settleIt = 0, settleComps = 0; };
WakeProf g_wp;
bool wpOn() { static const bool on = [] { const char* e = std::getenv("LL_TICK_PROFILE"); return e && e[0] == '1'; }(); return on; }
double wpNow() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct WpTimer { double s; WpTimer() : s(wpOn() ? wpNow() : 0) {} ~WpTimer() { if (wpOn()) g_wp.ms += wpNow() - s; } };
bool sameTickOn() { static const bool on = [] { const char* e = std::getenv("LL_SAME_TICK"); return !(e && e[0] == '0'); }(); return on; }
bool compTableOn() { static const bool on = [] { const char* e = std::getenv("LL_COMP_TABLE"); return !(e && e[0] == '0'); }(); return on; }
// PlayM (JD 10:30): Alternate Current-inspired Trace speedups. Each has its own switch (=0 restores the
// old path); results are identical by construction (see each site).
bool acSeedsOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_SEEDS"); return !(e && e[0] == '0'); }(); return on; }
bool acSameSeedsOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_SAME_SEEDS"); return !(e && e[0] == '0'); }(); return on; }
bool acMountSwapOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_MOUNT_SWAP"); return !(e && e[0] == '0'); }(); return on; }
bool acWakeOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_WAKE"); return !(e && e[0] == '0'); }(); return on; }
// PlayL2: reuse end-of-last-tick mountPowered_ for flip/torches (skip a full comps walk). =0 restores.
bool acMountReuseOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_MOUNT_REUSE"); return !(e && e[0] == '0'); }(); return on; }
// PlayL2: defer net zero until same-seed miss (skip zero+restore on stable nets). =0 restores.
bool acDeferZeroOn() { static const bool on = [] { const char* e = std::getenv("LL_AC_DEFER_ZERO"); return !(e && e[0] == '0'); }(); return on; }
bool hotSpecOn() { static const bool on = [] { const char* e = std::getenv("LL_HOT_SPEC"); return !(e && e[0] == '0'); }(); return on; }
bool netLevelSoaOn() { static const bool on = [] { const char* e = std::getenv("LL_NET_LEVEL_SOA"); return !(e && e[0] == '0'); }(); return on; }
bool interestCullOn() { static const bool on = [] { const char* e = std::getenv("LL_INTEREST_CULL"); return !(e && e[0] == '0'); }(); return on; }
bool emitCacheOn() { static const bool on = [] { const char* e = std::getenv("LL_EMIT_CACHE"); return !(e && e[0] == '0'); }(); return on; }  // PlayM2: default ON (via/colour guarded); was L2 default OFF — open-end viaAx must go through seed()
bool optionAOn() { static const bool on = [] { const char* e = std::getenv("LL_OPTION_A"); return !(e && e[0] == '0'); }(); return on; }
}
void cSignalSim::hsRefreshSoft(const ChunkMap& w) {
  static const bool kSoftPower = [] { const char* e = std::getenv("LL_SOFT_POWER"); return !(e && e[0] == '0'); }();
  static const bool kAutoSoft = [] { const char* e = std::getenv("LL_TRACE_AUTO_SOFT"); return e && e[0] == '1'; }();
  softChanged_.clear();
  if (!kSoftPower) { softPowered_.clear(); return; }
  const auto& reg = blockRegistry();
  auto level = [&](int64_t k) {
    int x, y, z; unpackPos(k, x, y, z);
    if (!reg.hasAll(w.get(x, y, z), kSolid | kOpaque | kPowerable)) return 0;
    int s = 0;
    for (int i = 0; i < 6; ++i) {
      const auto& d = k6[i];
      const int wx = x + d[0], wy = y + d[1], wz = z + d[2];
      if (!isFlatWire(w.get(wx, wy, wz))) continue;
      if (!cTraceConn::sideOn(w.getState(wx, wy, wz), -d[0], -d[1], -d[2])) continue;  // PlayL 7:48: connected sides only
      // 2026-11-01 (JD): an auto (never Alt-scrolled) Trace does NOT soft-power blocks; only a side the player
      // turned on does, matching the mesher (arms to blocks draw only for custom states). LL_TRACE_AUTO_SOFT=1: old.
      if (!kAutoSoft && cTraceConn::isAuto(w.getState(wx, wy, wz))) continue;
      s = std::max(s, int(stValue(w.getState(wx, wy, wz))));
    }
    return std::min(s, int(kMaxSignal));
  };
  std::vector<int64_t> cand;
  const bool full = fullPass_ || !netsValid_ || softAll_;
  if (full) {
    cand.reserve(softPowered_.size());
    for (const auto& [k, v] : softPowered_) cand.push_back(k);
    for (int64_t k : wires_) {
      int x, y, z; unpackPos(k, x, y, z);
      if (!isFlatWire(w.get(x, y, z)) || stValue(w.getState(x, y, z)) <= 0) continue;
      for (auto& d : k6) cand.push_back(packPos(x + d[0], y + d[1], z + d[2]));
    }
  } else {
    for (int32_t id : hsMarkedNets_) {
      if (size_t(id) >= nets_.size()) continue;
      for (int64_t k : nets_[size_t(id)]) {
        int x, y, z; unpackPos(k, x, y, z);
        for (auto& d : k6) cand.push_back(packPos(x + d[0], y + d[1], z + d[2]));
      }
    }
    for (int64_t k : softEdits_) {
      int x, y, z; unpackPos(k, x, y, z);
      cand.push_back(k);
      for (auto& d : k6) cand.push_back(packPos(x + d[0], y + d[1], z + d[2]));
    }
  }
  softAll_ = false; softEdits_.clear();
  std::sort(cand.begin(), cand.end());
  cand.erase(std::unique(cand.begin(), cand.end()), cand.end());
  for (int64_t k : cand) {
    const int nv = level(k);
    const auto it = softPowered_.find(k);
    const int ov = it == softPowered_.end() ? 0 : int(it->second);
    if (nv == ov) continue;
    if (nv > 0) softPowered_[k] = uint8_t(nv); else softPowered_.erase(it);
    softChanged_.push_back(k);
  }
  if (softChanged_.empty()) return;
  if (!fullPass_) {  // full passes already evaluate every lamp
    const size_t n0 = hsLampKeys_.size();
    for (int64_t k : softChanged_) {
      int x, y, z; unpackPos(k, x, y, z);
      for (auto& d : k6) if (const int64_t n = packPos(x + d[0], y + d[1], z + d[2]); lamps_.contains(n)) hsLampKeys_.push_back(n);
    }
    if (hsLampKeys_.size() != n0) {
      std::sort(hsLampKeys_.begin(), hsLampKeys_.end());
      hsLampKeys_.erase(std::unique(hsLampKeys_.begin(), hsLampKeys_.end()), hsLampKeys_.end());
    }
  }
  if (!allAwake_) { WpTimer wpt; for (int64_t k : softChanged_) wakeAround(k); }  // readers of the block tick next
}
int cSignalSim::softPower(int x, int y, int z) const {
  if (softPowered_.empty()) return 0;
  const int64_t k = packPos(x, y, z);
  if (!mountPowered_.empty() && mountPowered_.contains(k)) return 0;  // hard wins, applied at read time (never stale)
  const auto it = softPowered_.find(k);
  return it == softPowered_.end() ? 0 : int(it->second);
}

// A torch's support: below for free-standing (legacy) torches, else its attach face.
static void torchSupport(CellState s, int x, int y, int z, int& sx, int& sy, int& sz) {
  int dx, dy, dz;
  attachOffset(stAttach(s) ? stAttach(s) : 1, dx, dy, dz);
  sx = x + dx; sy = y + dy; sz = z + dz;
}

bool cSignalSim::blockPowered(const ChunkMap& w, int x, int y, int z) const {
  const auto& reg = blockRegistry();
  if (!reg.has(w.get(x, y, z), kPowerable)) return false;
  if (strongPowered(w, x, y, z)) return true;
  if (isFlatWire(w.get(x, y + 1, z)) && stValue(w.getState(x, y + 1, z)) > 0) return true;  // wire on top
  for (auto& d : kH)
    if (isFlatWire(w.get(x + d[0], y, z + d[1])) && stValue(w.getState(x + d[0], y, z + d[1])) > 0) return true;
  return false;
}

int cSignalSim::emitToward(const ChunkMap& w, int x, int y, int z, int dir) const {
  const auto it = comps_.find(packPos(x, y, z));
  return it == comps_.end() ? 0 : compEmit(it->second.behavior, it->second.mem, it->second.emitMode, dir, w.getState(x, y, z));
}

int cSignalSim::inputFrom(const ChunkMap& w, int x, int y, int z, int d) const {
  const int nx = x + kDirX[d], nz = z + kDirZ[d];
  const BlockDef nd = blockRegistry().def(w.get(nx, y, nz));
  const CellState s = w.getState(nx, y, nz);
  // SW-H2: a disk-loaded wire cell state carries a 7-bit value (65..127 are
  // representable); clamp at this sampling point so no component input Ã¢â‚¬â€ and
  // therefore no pass-through output Ã¢â‚¬â€ can ever exceed kMaxSignal.
  if (nd.role == SimRole::Wire)
    return isVia(w.get(nx, y, nz)) || !cTraceConn::sideOn(s, -kDirX[d], 0, -kDirZ[d]) ? 0 : std::min(stValue(s), kMaxSignal);
  if (nd.role == SimRole::Torch) return stValue(s) ? kMaxSignal : 0;
  if (kDirectComp && (nd.role == SimRole::Switch || nd.role == SimRole::cButton)) {  // attached or not, face-to-face
    return stOn(s) ? kMaxSignal : 0;  // read the cell state: attached switches may not sit in comps_
  }
  if (kDirectComp && nd.role == SimRole::Pulsor) {  // a component catches and drops a Pulsor pulse ...
    // ... except a Repeator (2026-11-01 JD, PlayK, LL_REP_LINK): the Pulsor's front feeds its back directly.
    if (cPulseRules::repeatorLinks() && blockRegistry().def(w.get(x, y, z)).role == SimRole::Repeator && comps_.contains(packPos(nx, y, nz)))
      return emitToward(w, nx, y, nz, dirBack(d));
    return 0;
  }
  if (comps_.contains(packPos(nx, y, nz))) return emitToward(w, nx, y, nz, dirBack(d));
  if (const int hp = hardPower(w, nx, y, nz); hp > 0) return hp;  // 2026-11-01 PlayL: hard power carries strength
  return softPower(nx, y, nz);  // 2026-11-01: soft power (0 when off)
}

// Catchor output side from its state (mirrors Catchor::outDir in logic_components.cpp).
static int emitDirOfCatchor(CellState s) {
  const int f = stFacing(s), m = stAux(s) & 3;
  return m == 1 ? (f + 3) & 3 : m == 2 ? (f + 1) & 3 : f;
}

bool cSignalSim::use(ChunkMap& w, int x, int y, int z, const DirtyFn& dirty, bool alt) {
  const auto it = comps_.find(packPos(x, y, z));
  if (it == comps_.end()) {  // using a wire: the first neighbouring component that reacts to its colour
    const BlockDef wd = blockRegistry().def(w.get(x, y, z));
    if (wd.role != SimRole::Wire) return false;
    if (alt && cTraceConn::enabled() && isFlatWire(w.get(x, y, z))) {  // 2026-11-01 PlayL: Alt-click / Alt+wheel cycles connections
      w.setState(x, y, z, cTraceConn::cycled(w.getState(x, y, z), 1));
      fullPass_ = true; netsValid_ = false; topoDirty_ = true; editLocal_ = false; softAll_ = true; mountFlipOk_ = false;
      dirty(x, y, z);
      for (auto& d : kH) dirty(x + d[0], y, z + d[1]);  // neighbours re-mesh their arms
      return true;
    }
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      const auto c = comps_.find(packPos(nx, y, nz));
      if (c == comps_.end()) continue;
      if (const auto ns = c->second.behavior->useChannel(w.getState(nx, y, nz), wd.channel)) {
        fullPass_ = true;
        w.setState(nx, y, nz, *ns);
        dirty(nx, y, nz);
        return true;
      }
    }
    return false;
  }
  std::optional<CellState> next;
  const CellState cur = w.getState(x, y, z);
  if (alt) next = it->second.behavior->useAlt(cur, it->second.mem);
  else if (roleOf(w.get(x, y, z)) == SimRole::Redirector && !(stAux(cur) & 4) && ![] { const char* e = std::getenv("LL_MUX_NOUSE"); return !(e && e[0] == '0'); }()) {  // 2026-11-01: LL_MUX_NOUSE
    // Fed by a Catchor: the Catchor is its input, so the output only cycles centre -> left ->
    // right relative to it (never back into the Catchor, never up or down).
    for (int d = 0; d < 4 && !next; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      if (roleOf(w.get(nx, y, nz)) != SimRole::Catchor || !comps_.contains(packPos(nx, y, nz))) continue;
      if (emitDirOfCatchor(w.getState(nx, y, nz)) != dirBack(d)) continue;  // its output must point here
      const int t = dirBack(d), opts[3] = {t, (t + 3) & 3, (t + 1) & 3};
      int k = 0;
      while (k < 3 && opts[k] != stFacing(cur)) ++k;
      next = makeState(stValue(cur), opts[(k + 1) % 3], stAux(cur), stOn(cur));
    }
  }
  if (!next && !alt) next = it->second.behavior->use(cur, it->second.mem);
  if (!next) return false;
  fullPass_ = true;
  w.setState(x, y, z, *next);
  dirty(x, y, z);
  return true;
}

std::string cSignalSim::inspect(const ChunkMap& w, int x, int y, int z) const {
  static constexpr const char* kDir = "NESW";
  const BlockId b = w.get(x, y, z);
  const CellState s = w.getState(x, y, z);
  const BlockDef d = blockRegistry().def(b);
  std::string r = std::format("#{} @{},{},{} state 0x{:04x} [value {} facing {} aux {} on {}]", b, x, y, z, s, stValue(s), kDir[stFacing(s)],
                              stAux(s), stOn(s) ? 1 : 0);
  const int64_t k = packPos(x, y, z);
  if (d.role == SimRole::Wire) {
    r += std::format(" | {} Trace{}", d.channel == 2 ? "blue" : "red", isVia(b) ? " (via)" : "");
    if (const auto it = wireNet_.find(k); it != wireNet_.end()) r += std::format(" net {}", it->second);
    else r += " (no net yet)";
  }
  if (const auto it = comps_.find(k); it != comps_.end()) {
    const ComponentMemory& m = it->second.mem;
    r += std::format(" | out {}{} out2 {} | edges in {} side {} clk {} | hist 0x{:02x} timer {} histHi 0x{:04x}", m.out, m.pulse ? " (pulse)" : "", m.out2,
                     m.prevIn, m.prevSide, m.prevClk, m.history, m.timer, m.historyHi);
  }
  return r;
}

void cSignalSim::keyPulse(int x, int y, int z, int front, int control) {
  if (const auto it = comps_.find(packPos(x, y, z)); it != comps_.end())
  { it->second.mem.keys.push_back({uint8_t(front), uint8_t(control)}); fullPass_ = true; }
}

namespace {  // 2026-11-01 (JD placement lag, measure only): LL_TICK_PROFILE per-cycle breakdown (sim thread only)
struct EditProf { double build = 0, full = 0, comps = 0, lamps = 0, mark = 0; int64_t t0 = 0; bool fp = false; };
EditProf g_ep;
bool epOn() { static const bool on = [] { const char* e = std::getenv("LL_TICK_PROFILE"); return e && e[0] == '1'; }(); return on; }
double epMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct EpTimer { double& acc; double s; EpTimer(double& a) : acc(a), s(epOn() ? epMs() : 0) {} ~EpTimer() { if (epOn()) acc += epMs() - s; } };
}
void cSignalSim::tick(ChunkMap& w, int pcx, int pcz, int radius, const DirtyFn& dirty) {
  // Reassembled from the hs* pieces (hs-design Ã‚Â§3.2); the handshake path in
  // cSignalWorldSim drives the identical pieces through the authority.
  hsBeginCycle(w, pcx, pcz, radius);
  hsFlipAndTorches(w, dirty);
  hsComponents(w, dirty);
  hsMark(w);
  if (!hsSkip_) {
    if (epOn()) {
      const auto w0 = std::chrono::steady_clock::now();
      if (fullPass_) hsPropagateFull(w, dirty);
      else {
        hsPrepIncremental(w);
        for (int32_t id : hsMarkedNets_) hsPropagateNet(w, id, dirty);
        hsApplyStrengthPending(&dirty);
      }
      hsRefreshSoft(w);  // 2026-11-01 PlayL: this tick's wire values, before lamps / next components read soft
      phase_.wires += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - w0).count();
      const auto s0 = std::chrono::steady_clock::now();
      hsSettle(w, dirty);
      phase_.settle += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count();
      const auto l0 = std::chrono::steady_clock::now();
      hsLampsBatch(hsLampKeys_, w, dirty);
      hsApplyLampHolds(hsTlsLampHolds());
      hsTlsLampHolds().clear();
      hsFinishCycle(w);
      phase_.lamps += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - l0).count();
    } else {
      if (fullPass_) hsPropagateFull(w, dirty);
      else {
        hsPrepIncremental(w);
        for (int32_t id : hsMarkedNets_) hsPropagateNet(w, id, dirty);
        hsApplyStrengthPending(&dirty);
      }
      hsRefreshSoft(w);  // 2026-11-01 PlayL: this tick's wire values, before lamps / next components read soft
      hsSettle(w, dirty);
      hsLampsBatch(hsLampKeys_, w, dirty);
      hsApplyLampHolds(hsTlsLampHolds());
      hsTlsLampHolds().clear();
      hsFinishCycle(w);
    }
  }
  hsFinalizeComponents();
  // Archie Phase 5: dump phase ms/tick under LL_TICK_PROFILE (every 512 ticks).
  if (epOn()) {
    static uint64_t phTicks = 0;
    static PhaseMs phSum{};
    // PlayK: takePhaseMs folds hsWiresNs_/hsLampsNs_ (pull-worker path) into the split.
    const PhaseMs snap = takePhaseMs();
    ++phTicks;
    phSum.comps += snap.comps; phSum.mark += snap.mark; phSum.wires += snap.wires;
    phSum.lamps += snap.lamps; phSum.settle += snap.settle; phSum.rebuild += snap.rebuild;
    phSum.begin += snap.begin; phSum.flip += snap.flip; phSum.finalize += snap.finalize; phSum.mount += snap.mount;
    if ((phTicks % 512) == 0) {
      const double n = double(phTicks);
      // PlayK fix 1: mark / settle / comps / lamps / rebuild readable under LL_TICK_PROFILE (epOn).
      ::litholattice::logInfo("signal phases: ticks=%llu comps %.3f mark %.3f wires %.3f lamps %.3f settle %.3f rebuild %.3f | begin %.3f flip %.3f finalize %.3f mount %.3f ms/tick (sum %.3f labeled %.3f)",
                              (unsigned long long)phTicks, phSum.comps / n, phSum.mark / n, phSum.wires / n, phSum.lamps / n,
                              phSum.settle / n, phSum.rebuild / n,
                              phSum.begin / n, phSum.flip / n, phSum.finalize / n, phSum.mount / n,
                              (phSum.comps + phSum.mark + phSum.wires + phSum.lamps + phSum.settle + phSum.rebuild + phSum.begin + phSum.flip + phSum.finalize) / n,
                              (phSum.comps + phSum.mark + phSum.wires + phSum.lamps + phSum.settle + phSum.rebuild) / n);
      phSum = {}; phTicks = 0;  // windowed avg (next 512 ticks)
    }
  }
  hsCommit();
}

uint64_t cSignalSim::simScopeFp(const ChunkMap& w, int pcx, int pcz, int radius) const {
  uint64_t fp = 0x9E3779B97F4A7C15ull;
  const int r = radius + 1;  // 1-column border: wireLinks reads neighbours across the edge
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      const int cx = pcx + dx, cz = pcz + dz;
      if (!area_.contains(cx * 16 + 8, cz * 16 + 8) && std::abs(dx) + std::abs(dz) > r) continue;
      const uint64_t p = uint64_t(reinterpret_cast<uintptr_t>(w.column(cx, cz)));
      fp = (fp ^ (p + uint64_t(dx * 131 + dz) * 0x100000001B3ull)) * 0x100000001B3ull;
    }
  return fp;
}

void cSignalSim::hsBeginCycle(ChunkMap& w, int pcx, int pcz, int radius) {
  const auto b0 = std::chrono::steady_clock::now();
  if (epOn()) { g_ep = EditProf{}; g_ep.t0 = int64_t(epMs() * 1000.0); g_ep.fp = fullPass_; }
  ++tickNo_;
  // NOTE: radio_/jammers_ flips are NOT here any more Ã¢â‚¬â€ they are hsCommit()
  // at cycle end, in both modes (D12; see signal.h).
  hsReason_ = (fullPass_ ? 4u : 0u) | (netsValid_ ? 0u : 8u);  // LL_TICK_PROFILE spike reason (measure only)
  if (area_.pcx != pcx || area_.pcz != pcz || area_.radius != radius) { if (netsValid_) diagKind_ = 4; fullPass_ = true; netsValid_ = false; hsReason_ |= 1u; }
  // 2026-10-31 (JD Trace seam fix): a column replaced in place (evict K + load K, reload, demote/promote
  // sub swap) keeps the count but frees the sub-chunks the cached net pointers write into, so the run went
  // dark from that column on until an edit rebuilt the nets. structureGen() names every such change.
  static const bool genCheck = [] { const char* e = std::getenv("LL_SIGNAL_GENCHECK"); return !(e && e[0] == '0'); }();  // default on; =0 kill switch
  static const bool genIncr = [] { const char* e = std::getenv("LL_SIGNAL_GENINCR"); return !(e && e[0] == '0'); }();  // incremental re-flood; =0 kill (full pass)
  bool genChanged = false;
  if (genCheck && w.structureGen() != netStructGen_) {  // only a replaced column the pointers reach matters (circuit-free streaming stays cheap)
    // 2026-10-31 (JD PlayC TPS regression): incremental. Only a column that holds circuit cells and was
    // replaced by a different live column counts (an evicted one is the colsChanged path); its nets are
    // re-flooded from seeds in that column (genSeeds_), batched once per tick, not a full pass.
    genSeedCols_.clear();
    for (const auto& [ck, cp] : netColPtrs_) {
      const ChunkColumn* now = w.column(int(ck >> 32), int(int32_t(uint32_t(ck))));
      if (now != cp && now != nullptr) { genChanged = true; if (genIncr) genSeedCols_.push_back(ck); }
    }
    if (!genChanged && netsValid_) netStructGen_ = w.structureGen();
  }
  bool colsChanged = w.columns().size() != netColumns_ || genChanged;
  if (genChanged) {  // replaced sub-chunks hold fresh (zero) wire state: re-flood the nets in those columns
    genSeeds_.clear();
    if (genIncr) {
      auto inCol = [&](int64_t k) {
        int x, y, z; unpackPos(k, x, y, z);
        const int64_t ck = (int64_t(x >> 4) << 32) | uint32_t(z >> 4);
        return std::binary_search(genSeedCols_.begin(), genSeedCols_.end(), ck);
      };
      for (int64_t k : wires_) if (inCol(k)) genSeeds_.push_back(k);
      for (int64_t k : torches_) if (inCol(k)) genSeeds_.push_back(k);
      for (int64_t k : lamps_) if (inCol(k)) genSeeds_.push_back(k);
      for (const auto& [k, c] : comps_) if (inCol(k)) genSeeds_.push_back(k);
    }
    ++genEvents_;
    if (!genIncr || genSeeds_.size() > 8192) { fullPass_ = true; genSeeds_.clear(); }  // =0 kill / huge swap: legacy full re-flood
  }
  if (simScope_ && colsChanged) {  // Gate 1: only the sim area (+1 border) counts
    const uint64_t fp = simScopeFp(w, pcx, pcz, radius);
    if (netsValid_ && fp == areaFp_ && !genChanged) { netColumns_ = w.columns().size(); colsChanged = false; hsReason_ |= 64u; }
    areaFp_ = fp;
  }
  if (colsChanged) {  // columns loaded / dropped: cached pointers stale
    if (lazyNets_ && netsValid_ && !genChanged && netTouchUnchanged(w)) { netColumns_ = w.columns().size(); hsReason_ |= 32u; }  // LL_SIGNAL_LAZYNETS  // PlayL2 #5: keep lazy/edge path (column streaming hygiene); no deeper world rewrite this pass.
    else {
      if (!lazyNets_) diagKind_ = 3;
      bool topo = !(relinkNets_ && lazyNets_ && (netsValid_ || (edgeRelink_ && !topoDirty_)));  // EDGERELINK: topoDirty_ already covers own-cell loads/evicts  // LL_SIGNAL_RELINK: only own-cell columns change the flood
      if (!topo) for (const NetTouch& t : netTouch_) if (t.own && (w.column(t.cx, t.cz) != nullptr) != t.loaded) { topo = true; break; }
      if (topo) topoDirty_ = true;
      netsValid_ = false; hsReason_ |= 2u;
    }
  }
  area_ = {pcx, pcz, radius, roundArea_, diamondArea_};  // F56-7 + diamond
  { static const bool prof = [] { const char* e = std::getenv("LL_TICK_PROFILE"); return e && e[0] == '1'; }();  // diag: simulated columns per tick
    if (prof) {
      static uint64_t cnt = 0, sum = 0; static int mn = 1 << 30, mx = 0; int c = 0;
      for (int dz = -radius; dz <= radius; ++dz)
        for (int dx = -radius; dx <= radius; ++dx)
          if (w.column(pcx + dx, pcz + dz) && area_.contains((pcx + dx) * 16 + 8, (pcz + dz) * 16 + 8)) ++c;
      ++cnt; sum += uint64_t(c); mn = std::min(mn, c); mx = std::max(mx, c);
      if (cnt % 4096 == 0) { ::litholattice::logInfo("signal simcols: ticks=%llu min=%d avg=%.1f max=%d radius=%d", (unsigned long long)cnt, mn, double(sum) / double(cnt), mx, radius); }
    }
  }
  changedSrc_.clear(); extraLamps_.clear(); softChanged_.clear();
  if (!genSeeds_.empty()) {  // replaced-column seeds: their nets and lamps recompute this pass
    for (int64_t k : genSeeds_) { changedSrc_.push_back(k); if (lamps_.contains(k)) extraLamps_.push_back(k); }
    genSeeds_.clear();
  }
  if (tickNo_ >= nextHoldExpiry_) {  // afterglows ending this tick need a recompute; find the next end
    nextHoldExpiry_ = UINT64_MAX;
    for (const auto& [k, end] : lampHold_) {
      if (end <= tickNo_) extraLamps_.push_back(k);
      else nextHoldExpiry_ = std::min(nextHoldExpiry_, end);
    }
  }
  // PlayB (JD t4339u): lamps with a delayed state due this tick recompute (and show it) this pass.
  for (const auto& [k, d] : lampDelay_) extraLamps_.push_back(k);
  const bool wakeAll = allAwake_ || fullPass_ || !compSleep_;
  allAwake_ = wakeAll;  // read by tickComponents
  hsSkip_ = false;
  if (epOn()) phase_.begin += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - b0).count();
}

// 2026-11-01 PlayM (JD): save -> quit -> reload probe for Trace connection states (test runs only).
// LL_CONN_PROBE=2: at tick 40 every 7th flat Trace (sorted key order) gets connection state 1 + (n % 63);
// LL_CONN_PROBE=1 or 2: at ticks 400 and every 2000 logs a census (custom count + hash of key/index),
// so a reload run's census must equal the marking run's.
void cSignalSim::connProbe(ChunkMap& w, const DirtyFn& dirty) {
  static const int mode = [] { const char* e = std::getenv("LL_CONN_PROBE"); return e ? std::atoi(e) : 0; }();
  if (mode <= 0) return;
  if (mode == 2 && tickNo_ == 40) {
    std::vector<int64_t> keys(wires_.begin(), wires_.end());
    std::sort(keys.begin(), keys.end());
    size_t n = 0, marked = 0;
    for (int64_t k : keys) {
      int x, y, z; unpackPos(k, x, y, z);
      if (!isFlatWire(w.get(x, y, z))) continue;
      if ((n++ % 7) != 0) continue;
      const CellState st = w.getState(x, y, z);
      w.setState(x, y, z, cTraceConn::withIndex(st, int(1 + (marked % 63))));
      dirty(x, y, z);
      ++marked;
    }
    fullPass_ = true; netsValid_ = false; topoDirty_ = true; softAll_ = true;
    ::litholattice::logInfo("conn probe: marked %zu of %zu flat Traces at tick %llu", marked, n, (unsigned long long)tickNo_);
  }
  if (tickNo_ == 400 || (tickNo_ % 2000) == 0) {
    std::vector<int64_t> keys(wires_.begin(), wires_.end());
    std::sort(keys.begin(), keys.end());
    uint64_t h = 1469598103934665603ull; size_t custom = 0, flat = 0;
    for (int64_t k : keys) {
      int x, y, z; unpackPos(k, x, y, z);
      if (!isFlatWire(w.get(x, y, z))) continue;
      ++flat;
      const int idx = cTraceConn::index(w.getState(x, y, z));
      if (idx == 0) continue;
      ++custom;
      h = (h ^ uint64_t(k)) * 1099511628211ull; h = (h ^ uint64_t(idx)) * 1099511628211ull;
    }
    ::litholattice::logInfo("conn probe: census tick %llu flat %zu custom %zu hash %016llx", (unsigned long long)tickNo_, flat, custom, (unsigned long long)h);
  }
}

void cSignalSim::rebuildCompDense() {
  if (compDenseGen_ == compsGen_ && compAllKeys_.size() == comps_.size()) return;
  compAllKeys_.clear();
  compAllKeys_.reserve(comps_.size());
  for (const auto& [k, c] : comps_) { (void)c; compAllKeys_.push_back(k); }
  std::sort(compAllKeys_.begin(), compAllKeys_.end());
  compAllPtrs_.resize(compAllKeys_.size());
  for (size_t i = 0; i < compAllKeys_.size(); ++i) {
    const auto it = comps_.find(compAllKeys_[i]);
    compAllPtrs_[i] = (it == comps_.end()) ? nullptr : &it->second;
  }
  compDenseGen_ = compsGen_;
}

void cSignalSim::hsFlipAndTorches(ChunkMap& w, const DirtyFn& dirty) {
  const auto f0 = std::chrono::steady_clock::now();
  connProbe(w, dirty);  // PlayM test probe (LL_CONN_PROBE, off by default)
  // PlayL2 lever 1: end-of-last-tick mounts already match live outs (mark/settle collected).
  // Torches read previous-tick hard power â€” reuse unless an edit invalidated mountFlipOk_.
  // fullPass_ always rebuilds (first tick / topology / gen swap). LL_AC_MOUNT_REUSE=0 restores.
  if (!(acMountReuseOn() && mountFlipOk_ && !fullPass_ && !mountPowered_.empty()))
    collectMountPowered(w);
  tickTorches(w, dirty);
  if (epOn()) phase_.flip += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - f0).count();
}

void cSignalSim::hsComponents(ChunkMap& w, const DirtyFn& dirty) {
  const auto c0 = std::chrono::steady_clock::now();
  tickComponents(w, dirty);
  phase_.comps += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - c0).count();
}

void cSignalSim::hsMark(ChunkMap& w) {
  const auto m0 = std::chrono::steady_clock::now();
  // Next tick's wake set: around every changed source; the readers of every re-flooded network.
  allAwake_ = fullPass_ || !compSleep_ || !netsValid_;
  awakeClear();
  auto populateWake = [this] {
    // The legacy WakeNets RAII ran at tick() exit over the marked bitset as
    // it stood then; on the skip path that bitset still holds the PREVIOUS
    // cycle's marks (they are only cleared inside the marking block below),
    // so this reads the bitset directly, not hsMarkedNets_.
    if (allAwake_) return;
    WpTimer wpt;
    for (int64_t k : changedSrc_) wakeAround(k);
    if (!acWakeOn() || fullPass_)  // AC item 1: else hsApplyStrengthPending wakes only beside wires that changed
      forEachMarkedNet([&](int32_t id) {
        if (size_t(id) < netComps_.size())
          for (int64_t ck : netComps_[size_t(id)]) awakeInsert(ck);
      });
    for (int64_t k : extraLamps_) wakeAround(k);
  };
  // AC item 3 (LL_AC_MOUNT_SWAP): swap the live map into mountPrev_ instead of copying it every
  // tick; collectMountPowered() clears and refills mountPowered_ (reusing the old buckets).
  std::unordered_map<int64_t, uint8_t> mountsCopy;
  if (acMountSwapOn()) mountPrev_.swap(mountPowered_); else mountsCopy = mountPowered_;
  const std::unordered_map<int64_t, uint8_t>& mountsBefore = acMountSwapOn() ? mountPrev_ : mountsCopy;
  collectMountPowered(w);  // after this tick's components: hard power is this tick's
  // Canonical order (packPos) for the mount diff: unordered iteration
  // depends on insertion history (review W-1). A changed strength is a change too.
  const size_t mountDiffBegin = changedSrc_.size();
  for (const auto& [k, v] : mountsBefore) if (const auto it = mountPowered_.find(k); it == mountPowered_.end() || it->second != v) changedSrc_.push_back(k);
  for (const auto& [k, v] : mountPowered_) if (!mountsBefore.contains(k)) changedSrc_.push_back(k);
  std::sort(changedSrc_.begin() + std::ptrdiff_t(mountDiffBegin), changedSrc_.end());
  if (!fullPass_ && changedSrc_.empty() && extraLamps_.empty() && softEdits_.empty() && !softAll_) {  // same result as last pass
    hsSkip_ = true;
    populateWake();
    return;
  }
  if (!fullPass_) {
    if (!netsValid_) buildNets(w);
    clearNetMarks(); dirtyLampsClear();
    for (int64_t k : changedSrc_) {
      auto [rit, fresh] = srcReach_.try_emplace(k);
      SrcReach& r = rit->second;
      if (fresh) fillSrcReach(k, r);  // PlayK fix 3: flat scratch 5x5x5, cached until nets rebuild
      for (int32_t id : r.nets) markNet(id);
      dirtyLampsInsertRange(r.lamps.begin(), r.lamps.end());
    }
    forEachMarkedNet([&](int32_t id) { dirtyLampsInsertRange(netLamps_[size_t(id)].begin(), netLamps_[size_t(id)].end()); });
    dirtyLampsInsertRange(extraLamps_.begin(), extraLamps_.end());
    // N4: lamps on solids beside marked-net wires must re-eval soft/hard even when the
    // lamp is not 6-adjacent to a wire (common: lamp on copper next to Trace). Cached
    // srcReach_/netLamps_ miss that pattern until a full remount (cull reload).
    {
      const auto& reg = blockRegistry();
      forEachMarkedNet([&](int32_t id) {
        if (size_t(id) >= nets_.size()) return;
        for (int64_t wk : nets_[size_t(id)]) {
          int x, y, z; unpackPos(wk, x, y, z);
          for (auto& d : k6) {
            const int bx = x + d[0], by = y + d[1], bz = z + d[2];
            if (!reg.hasAll(w.get(bx, by, bz), kSolid | kOpaque | kPowerable)) continue;
            for (auto& e : k6) {
              const int64_t lk = packPos(bx + e[0], by + e[1], bz + e[2]);
              if (lamps_.contains(lk)) dirtyLampsInsert(lk);
            }
          }
        }
      });
    }
  }
  // Canonical work lists for this cycle (hs-design Ã‚Â§3.2/Ã‚Â§9.6): marked nets
  // ascending by id; lamp keys sorted ascending (legacy iterated the
  // unordered sets Ã¢â‚¬â€ per-lamp writes are disjoint and hold merges are
  // commutative, so sorting changes no outcome, only the visit order).
  hsMarkedNets_.clear();
  if (!fullPass_) forEachMarkedNet([&](int32_t id) { hsMarkedNets_.push_back(id); });
  // PlayL2 #12: interest cull â€” drop marked nets with zero in-area wires (rendered play).
  // Headless ALU Stress sits inside sim distance, so this is a no-op there.
  if (interestCullOn() && !fullPass_ && !hsMarkedNets_.empty()) {
    std::vector<int32_t> kept;
    kept.reserve(hsMarkedNets_.size());
    for (int32_t id : hsMarkedNets_) {
      if (size_t(id) >= netArr_.size() || netArr_[size_t(id)].keys.empty()) continue;
      kept.push_back(id);
    }
    hsMarkedNets_.swap(kept);
  }
  hsLampKeys_.clear();
  if (fullPass_) hsLampKeys_.assign(lamps_.begin(), lamps_.end());
  else hsLampKeys_ = dirtyLampKeys_;  // already unique via epoch stamp
  std::sort(hsLampKeys_.begin(), hsLampKeys_.end());
  phase_.mark += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m0).count();
  populateWake();
}

void cSignalSim::hsFinishCycle(ChunkMap& w) {
  if (fullPass_) allAwake_ = true;  // a full pass may change any wire: everyone ticks next
  if (fullPass_ && !netsValid_) buildNets(w);
  fullPass_ = false;
}

void cSignalSim::hsCommit() {
  // PlayL2 / MCHPRS #4: cycle end is radio/jammer flip + phase dump only. No remesh, no UI, no mutex.

  if (epOn() && g_ep.t0) {
    static const double kSpike = [] { const char* e = std::getenv("LL_SPIKE_MS"); return e ? std::atof(e) : 10.0; }();
    const double tot = epMs() - double(g_ep.t0) / 1000.0;
    if (tot >= kSpike)
      ::litholattice::logInfo("signal cycle: %.2f ms fullPass=%d | buildNets %.2f propagateFull %.2f components %.2f lamps %.2f | comps %zu wires %zu lamps %zu",
                              tot, int(g_ep.fp), g_ep.build, g_ep.full, g_ep.comps, g_ep.lamps, comps_.size(), wires_.size(), lamps_.size());
  }
  // PlayK fix 1: handshake path never enters tick(), so dump phases here too (same windowed avg).
  if (epOn()) {
    static uint64_t phTicksHs = 0;
    static PhaseMs phSumHs{};
    const PhaseMs snap = takePhaseMs();
    ++phTicksHs;
    phSumHs.comps += snap.comps; phSumHs.mark += snap.mark; phSumHs.wires += snap.wires;
    phSumHs.lamps += snap.lamps; phSumHs.settle += snap.settle; phSumHs.rebuild += snap.rebuild;
    phSumHs.begin += snap.begin; phSumHs.flip += snap.flip; phSumHs.finalize += snap.finalize; phSumHs.mount += snap.mount;
    if ((phTicksHs % 512) == 0) {
      const double n = double(phTicksHs);
      ::litholattice::logInfo("signal phases: ticks=%llu comps %.3f mark %.3f wires %.3f lamps %.3f settle %.3f rebuild %.3f | begin %.3f flip %.3f finalize %.3f mount %.3f ms/tick (sum %.3f labeled %.3f)",
                              (unsigned long long)phTicksHs, phSumHs.comps / n, phSumHs.mark / n, phSumHs.wires / n, phSumHs.lamps / n,
                              phSumHs.settle / n, phSumHs.rebuild / n,
                              phSumHs.begin / n, phSumHs.flip / n, phSumHs.finalize / n, phSumHs.mount / n,
                              (phSumHs.comps + phSumHs.mark + phSumHs.wires + phSumHs.lamps + phSumHs.settle + phSumHs.rebuild + phSumHs.begin + phSumHs.flip + phSumHs.finalize) / n,
                              (phSumHs.comps + phSumHs.mark + phSumHs.wires + phSumHs.lamps + phSumHs.settle + phSumHs.rebuild) / n);
      phSumHs = {}; phTicksHs = 0;
    }
  }
  if (wpOn() && (++g_wp.ticks % 4096) == 0)
    ::litholattice::logInfo("signal wake/settle: ticks=%llu wake bookkeeping %.4f ms/tick, wakeAround %.1f/tick | settle iters %.3f/tick comps re-eval %.1f/tick cap hits %llu | AC same-seed skips %llu",
                            (unsigned long long)g_wp.ticks, g_wp.ms / double(g_wp.ticks), double(g_wp.around) / double(g_wp.ticks),
                            double(g_wp.settleIt) / double(g_wp.ticks), double(g_wp.settleComps) / double(g_wp.ticks), (unsigned long long)settleCapHits_, (unsigned long long)sameSeedSkips_.load(std::memory_order_relaxed));
  radio_.flip();    // this cycle's broadcasts become readable next cycle (1-tick wireless latency)
  jammers_.flip();  // this cycle's jammer zones become active
}

void cSignalSim::wakeAround(int64_t k) {
  if (wpOn()) ++g_wp.around;
  // PlayK fix 2+5: prefer packed adjacency (wake-only Trace / AC-style); geometric fallback.
  if (const auto it = compAdj_.find(k); it != compAdj_.end()) {
    for (int64_t n : it->second) awakeInsert(n);
    return;
  }
  int x, y, z; unpackPos(k, x, y, z);
  for (int dy = -1; dy <= 2; ++dy)  // +2: a torch powers the block above it, read beside that block
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const int64_t n = packPos(x + dx, y + dy, z + dz);
        if (comps_.contains(n)) awakeInsert(n);
      }
}

// 2026-10-25 (JD): via link rules. Vertical: same colour, at least one vertical via, no side via.
// Horizontal (same y, step dx/dz): needs a side via whose axis matches the step; the other end is a
// same-axis side via, a vertical via (pipe elbow) or a flat Trace. Symmetric; colour checked by caller.
static bool viaVertOk(BlockId self, BlockId nb) {
  return (viaAxis(self) == 0 || viaAxis(nb) == 0) && !isSideVia(self) && !isSideVia(nb);
}
static bool viaSideOk(BlockId self, BlockId nb, int dx) {
  const int ax = dx ? 1 : 2, sa = viaAxis(self), na = viaAxis(nb);
  if (sa > 0 && sa != ax) return false;
  if (na > 0 && na != ax) return false;
  return sa > 0 || na > 0;
}

void cSignalSim::wireLinks(const ChunkMap& w, int x, int y, int z, std::vector<int64_t>& out) const {
  const auto& reg = blockRegistry();
  const uint8_t channel = reg.def(w.get(x, y, z)).channel;
  const BlockId self = w.get(x, y, z);
  const bool selfVia = isVia(self);
  for (int dy = -1; dy <= 1; dy += 2) {  // via links: straight up/down, same colour, lossless
    const BlockId nb = w.get(x, y + dy, z);
    if (isWire(nb) && reg.def(nb).channel == channel && (selfVia || isVia(nb)) && viaVertOk(self, nb) &&
        cTraceConn::sideOn(w.getState(x, y, z), 0, dy, 0) && cTraceConn::sideOn(w.getState(x, y + dy, z), 0, -dy, 0)) out.push_back(packPos(x, y + dy, z));
  }
  if (selfVia) {  // vertical vias only reach sideways into a side via pointing at them
    for (auto& d : kH) {
      const BlockId nb = w.get(x + d[0], y, z + d[1]);
      if (isWire(nb) && reg.def(nb).channel == channel && viaSideOk(self, nb, d[0])) out.push_back(packPos(x + d[0], y, z + d[1]));
    }
    return;
  }
  const bool upBlocked = reg.has(w.get(x, y + 1, z), kCutsWire);
  for (auto& d : kH) {
    const int nx = x + d[0], nz = z + d[1];
    for (int dy = -1; dy <= 1; ++dy) {
      if (dy == 1 && upBlocked) continue;
      if (dy == -1 && reg.has(w.get(nx, y, nz), kCutsWire)) continue;
      const BlockDef nd = reg.def(w.get(nx, y + dy, nz));
      if (nd.role != SimRole::Wire || nd.channel != channel) continue;
      if (isVia(w.get(nx, y + dy, nz)) && !(dy == 0 && viaSideOk(self, w.get(nx, y, nz), d[0]))) continue;  // Trace feeds a via only through a side via's open end
      if (!cTraceConn::sideOn(w.getState(x, y, z), d[0], 0, d[1]) || !cTraceConn::sideOn(w.getState(nx, y + dy, nz), -d[0], 0, -d[1])) continue;  // PlayL connection states
      out.push_back(packPos(nx, y + dy, nz));
    }
  }
}

static CellState* statePtr(ChunkMap& w, int x, int y, int z) {
  if (y < 0 || y >= kWorldHeight) return nullptr;
  ChunkColumn* c = w.column(x >> 4, z >> 4);
  if (!c) return nullptr;
  SubChunk* s = c->subs[size_t(y >> 4)].get();
  return s ? &s->state[size_t(SubChunk::index(x & 15, y & 15, z & 15))] : nullptr;
}

void cSignalSim::buildNets(ChunkMap& w) {
  ++netGen_;  // AC item 4: cached seed sets refer to these nets
  EpTimer ept(g_ep.build);
  static const bool diag = [] { const char* e = std::getenv("LL_TICK_PROFILE"); return e && e[0] == '1'; }();
  const auto diagT0 = std::chrono::steady_clock::now();
  const auto rebuildT0 = std::chrono::steady_clock::now();  // PlayK fix 1
  bool relink = relinkNets_ && !topoDirty_;  // LL_SIGNAL_RELINK: flood topology unchanged
  std::vector<std::vector<int64_t>> chkNets, chkLamps, chkComps;  // LL_SIGNAL_EDGECHECK (debug)
  bool edgeChk = false;
  const bool hadEdge = edgePending_;
  if (relink && edgePending_) {  // LL_SIGNAL_EDGERELINK: split only the nets that lost wires
    if (!edgeSplit(w)) relink = false;
    else if (edgeCheck_) { edgeChk = true; chkNets = nets_; chkLamps = netLamps_; chkComps = netComps_; relink = false; }
  }
  edgePending_ = false; edgeCols_.clear();
  const bool localEdit = relink && editLocal_ && !hadEdge;  // LL_EDIT_LOCAL: cell set changed, wires did not
  const bool relinkScope = relinkScope_ && relink && !hadEdge && !editLocal_;  // LL_SIGNAL_RELINKSCOPE: area_ is current (an area change forces this rebuild)
  std::vector<int64_t> stack, links;
  srcReach_.clear(); netReach_.clear(); compAdj_.clear();  // PlayK: rebuild after flood
  if (!relink) {
  wireNet_.clear(); nets_.clear(); netLamps_.clear();
  wireNet_.reserve(wires_.size());
  // Canonical iteration order (packPos): network ids must not depend on
  // unordered_set bucket order (insertion/edit history).
  std::vector<int64_t> ordered(wires_.begin(), wires_.end());
  std::sort(ordered.begin(), ordered.end());
  for (int64_t start : ordered) {
    if (wireNet_.contains(start)) continue;
    const int32_t id = int32_t(nets_.size());
    nets_.emplace_back(); netLamps_.emplace_back();
    std::unordered_set<int64_t> lampSet;
    wireNet_[start] = id; stack.push_back(start);
    while (!stack.empty()) {
      const int64_t k = stack.back(); stack.pop_back();
      nets_[size_t(id)].push_back(k);
      int x, y, z; unpackPos(k, x, y, z);
      for (auto& d : k6) { const int64_t n = packPos(x + d[0], y + d[1], z + d[2]); if (lamps_.contains(n)) lampSet.insert(n); }
      links.clear(); wireLinks(w, x, y, z, links);
      for (int64_t n : links) if (wires_.contains(n) && wireNet_.emplace(n, id).second) stack.push_back(n);
    }
    netLamps_[size_t(id)].assign(lampSet.begin(), lampSet.end());
    std::sort(netLamps_[size_t(id)].begin(), netLamps_[size_t(id)].end());  // canonical order, as above (review W-2)
  }
  }  // !relink
  // Component input fast path: neighbour kinds and state pointers.
  for (const auto& reg = blockRegistry(); auto& [k, c] : comps_) {
    int x, y, z; unpackPos(k, x, y, z);
    if (relinkScope && !area_.contains(x, z)) { c.fast = false; continue; }  // LL_SIGNAL_RELINKSCOPE: tickComponents skips out-of-area cells
    c.fast = false;
    c.self = statePtr(w, x, y, z);
    if (!c.self) continue;
    bool ok = true;
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      const BlockId nb = w.get(nx, y, nz);
      const BlockDef nd = reg.def(nb);
      c.nb[d] = statePtr(w, nx, y, nz); c.ch[d] = 0;
      if (nd.role == SimRole::Wire) { c.kind[d] = isVia(nb) ? 3 : 1; c.ch[d] = uint8_t(cBehaviorCatalog::get().colour(nd)); }
      else if (nd.role == SimRole::Torch) { c.kind[d] = 2; c.ch[d] = uint8_t(cBehaviorCatalog::get().colour(nd)); }  // 2026-11-05 LED colour
      else if (comps_.contains(packPos(nx, y, nz)) || reg.hasAll(nb, kSolid | kOpaque | kPowerable) ||
               (kDirectComp && (nd.role == SimRole::Switch || nd.role == SimRole::cButton))) c.kind[d] = 4;
      else c.kind[d] = 0;
      if ((c.kind[d] == 1 || c.kind[d] == 2) && !c.nb[d]) ok = false;
    }
    c.top = isFlatWire(w.get(x, y + 1, z)) ? statePtr(w, x, y + 1, z) : nullptr;
    c.fast = ok;
    for (int d = 0; d < 4; ++d) { c.emitNet[d] = -1; c.emitIdx[d] = -1; }
  }
  // Readers of each network: components beside one of its wires, or under one (top input).
  if (!relink) netComps_.assign(nets_.size(), {});
  for (size_t id = 0; !relink && id < nets_.size(); ++id) {
    std::vector<int64_t>& v = netComps_[id];
    for (int64_t k : nets_[id]) {
      int x, y, z; unpackPos(k, x, y, z);
      for (auto& d : kH) if (const int64_t n = packPos(x + d[0], y, z + d[1]); comps_.contains(n)) v.push_back(n);
      if (const int64_t n = packPos(x, y - 1, z); comps_.contains(n)) v.push_back(n);
    }
    std::sort(v.begin(), v.end()); v.erase(std::unique(v.begin(), v.end()), v.end());
  }
  if (localEdit && kEditLocalCheck) {  // LL_EDIT_LOCAL_CHECK=1: patched readers must equal a fresh scan
    std::vector<std::vector<int64_t>> ec(nets_.size()), el(nets_.size());
    for (size_t id = 0; id < nets_.size(); ++id) {
      std::unordered_set<int64_t> ls;
      for (int64_t k : nets_[id]) {
        int x, y, z; unpackPos(k, x, y, z);
        for (auto& d : kH) if (const int64_t n = packPos(x + d[0], y, z + d[1]); comps_.contains(n)) ec[id].push_back(n);
        if (const int64_t n = packPos(x, y - 1, z); comps_.contains(n)) ec[id].push_back(n);
        for (auto& d : k6) { const int64_t n = packPos(x + d[0], y + d[1], z + d[2]); if (lamps_.contains(n)) ls.insert(n); }
      }
      std::sort(ec[id].begin(), ec[id].end()); ec[id].erase(std::unique(ec[id].begin(), ec[id].end()), ec[id].end());
      el[id].assign(ls.begin(), ls.end()); std::sort(el[id].begin(), el[id].end());
    }
    const bool same = ec == netComps_ && el == netLamps_;
    if (!same) ++edgeMismatch_;
    ::litholattice::logInfo("signal editlocal check: %s", same ? "MATCH" : "MISMATCH");
  }
  // Flat arrays: in-area wires only (out-of-area wires are frozen, exactly like the flood).
  const auto& reg = blockRegistry();
  const bool keepArr = localEdit && arrValid_ && netArr_.size() == nets_.size() && arrStructGen_ == w.structureGen() &&
                       arrColumns_ == w.columns().size() && arrArea_.pcx == area_.pcx && arrArea_.pcz == area_.pcz &&
                       arrArea_.radius == area_.radius && arrArea_.round == area_.round && arrArea_.diamond == area_.diamond;
  if (!keepArr) {  // LL_EDIT_LOCAL: wires, area and column pointers unchanged -> arrays still exact
  netArr_.assign(nets_.size(), {});
  wireLoc_.clear();
  for (size_t id = 0; id < nets_.size(); ++id) {
    NetArr& a = netArr_[id];
    for (int64_t k : nets_[id]) {
      int x, y, z; unpackPos(k, x, y, z);
      if (!area_.contains(x, z)) continue;
      wireLoc_[k] = {int32_t(id), int32_t(a.keys.size())};
      a.keys.push_back(k); a.st.push_back(statePtr(w, x, y, z)); a.via.push_back(uint8_t(isVia(w.get(x, y, z))));
    }
    a.old.resize(a.keys.size()); a.stamp.assign(a.keys.size(), 0); a.level.assign(a.keys.size(), 0);  // PlayL2 #11
  }
  for (size_t id = 0; id < nets_.size(); ++id) {  // CSR links (all indices known now)
    NetArr& a = netArr_[id];
    a.adjStart.reserve(a.keys.size() + 1);
    for (int64_t k : a.keys) {
      a.adjStart.push_back(int32_t(a.adj.size()));
      int x, y, z; unpackPos(k, x, y, z);
      links.clear(); wireLinks(w, x, y, z, links);
      for (int64_t n : links)
        if (const auto it = wireLoc_.find(n); it != wireLoc_.end() && it->second.first == int32_t(id)) a.adj.push_back(it->second.second);
    }
    a.adjStart.push_back(int32_t(a.adj.size()));
  }
  arrValid_ = true; arrArea_ = area_; arrStructGen_ = w.structureGen(); arrColumns_ = w.columns().size();
  // PlayL2 #7: cache emit target (net, local idx) per component dir â€” seed path skips wireLoc_ hash.
  if (emitCacheOn()) {
    for (auto& [k, c] : comps_) {
      int x, y, z; unpackPos(k, x, y, z);
      for (int d = 0; d < 4; ++d) {
        c.emitNet[d] = -1; c.emitIdx[d] = -1;
        const int64_t tk = packPos(x + kDirX[d], y, z + kDirZ[d]);
        if (const auto it = wireLoc_.find(tk); it != wireLoc_.end()) {
          c.emitNet[d] = it->second.first; c.emitIdx[d] = it->second.second;
        }
      }
    }
  }
  }  // !keepArr
  // Lamps fed only by wires get a pointer fast path; anything that can be strongly powered or sits
  // next to a component keeps the generic path.
  lampFast_.clear();
  for (int64_t k : lamps_) {
    int x, y, z; unpackPos(k, x, y, z);
    if (relinkScope && !area_.contains(x, z)) continue;  // LL_SIGNAL_RELINKSCOPE: lamp() skips out-of-area cells
    const BlockId lb = w.get(x, y, z);
    if (reg.hasAll(lb, kSolid | kOpaque | kPowerable)) continue;
    LampFast f{statePtr(w, x, y, z), {}, reg.def(lb).role == SimRole::ColorLamp};
    bool ok = f.self != nullptr;
    for (auto& d : k6) {
      const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
      const BlockId nb = w.get(nx, ny, nz);
      if (isFlatWire(nb)) { if (CellState* p = statePtr(w, nx, ny, nz)) f.wires.push_back(p); continue; }
      if (reg.has(nb, kPowerable) || comps_.contains(packPos(nx, ny, nz))) { ok = false; break; }
    }
    if (ok) lampFast_.emplace(k, std::move(f));
  }
  netColumns_ = w.columns().size();
  netStructGen_ = w.structureGen();  // 2026-10-31 (JD Trace seam fix)
  {
    // 2026-10-31 (JD TPS): every cached pointer lands in a column holding an indexed cell (neighbour wires are
    // indexed too), so only those columns are watched; far circuit-free demotes no longer trigger. LL_SIGNAL_GENOWN=0 kill.
    static const bool ownColsOnly = [] { const char* e = std::getenv("LL_SIGNAL_GENOWN"); return !(e && e[0] == '0'); }();
    std::vector<int64_t> cols;
    auto add = [&](int64_t k) {
      int x, y, z; unpackPos(k, x, y, z);
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) if (!ownColsOnly || (dx == 0 && dz == 0)) cols.push_back((int64_t((x >> 4) + dx) << 32) | uint32_t((z >> 4) + dz));
    };
    for (int64_t k : torches_) add(k);
    for (int64_t k : wires_) add(k);
    for (int64_t k : lamps_) add(k);
    for (const auto& [k, c] : comps_) add(k);
    std::sort(cols.begin(), cols.end()); cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
    netColPtrs_.clear(); netColPtrs_.reserve(cols.size());
    for (int64_t c : cols) netColPtrs_.push_back({c, w.column(int(c >> 32), int(int32_t(uint32_t(c))))});
  }
  netMarkedBits_.assign((nets_.size() + 31) / 32, 0);
  if (edgeChk) {
    const bool same = chkNets == nets_ && chkLamps == netLamps_ && chkComps == netComps_;
    if (!same) ++edgeMismatch_;
    ::litholattice::logInfo("signal edgecheck: %s (nets %zu vs flood %zu)", same ? "MATCH" : "MISMATCH", chkNets.size(), nets_.size());
  }
  ++netBuilds_; if (relink) ++netRelinks_;
  topoDirty_ = false; editLocal_ = false;
  if (relinkScope && lazyNets_) { for (NetTouch& t : netTouch_) t.loaded = w.column(t.cx, t.cz) != nullptr; } else {  // LL_SIGNAL_RELINKSCOPE: cell set unchanged, own flags hold
  netTouch_.clear();
  if (lazyNets_) {  // every column buildNets read through: indexed cells' columns and their 8 neighbours
    // 2026-10-31 (JD TPS): every cached pointer lands in a column holding an indexed cell (neighbour wires are
    // indexed too), so only those columns are watched; far circuit-free demotes no longer trigger. LL_SIGNAL_GENOWN=0 kill.
    static const bool ownColsOnly = [] { const char* e = std::getenv("LL_SIGNAL_GENOWN"); return !(e && e[0] == '0'); }();
    std::vector<int64_t> cols;
    auto add = [&](int64_t k) {
      int x, y, z; unpackPos(k, x, y, z);
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) if (!ownColsOnly || (dx == 0 && dz == 0)) cols.push_back((int64_t((x >> 4) + dx) << 32) | uint32_t((z >> 4) + dz));
    };
    for (int64_t k : torches_) add(k);
    for (int64_t k : wires_) add(k);
    for (int64_t k : lamps_) add(k);
    for (const auto& [k, c] : comps_) add(k);
    std::sort(cols.begin(), cols.end()); cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
    netTouch_.reserve(cols.size());
    for (int64_t p : cols) {
      const int cx = int(p >> 32), cz = int(int32_t(uint32_t(p)));
      netTouch_.push_back({cx, cz, w.column(cx, cz) != nullptr});
    }
    std::sort(netTouch_.begin(), netTouch_.end(), [](const NetTouch& a, const NetTouch& b) { return a.cx != b.cx ? a.cx < b.cx : a.cz < b.cz; });
    auto markOwn = [&](int64_t k) {
      int x, y, z; unpackPos(k, x, y, z);
      const auto it = std::lower_bound(netTouch_.begin(), netTouch_.end(), NetTouch{x >> 4, z >> 4, false},
                                       [](const NetTouch& a, const NetTouch& b) { return a.cx != b.cx ? a.cx < b.cx : a.cz < b.cz; });
      if (it != netTouch_.end() && it->cx == (x >> 4) && it->cz == (z >> 4)) it->own = true;
    };
    for (int64_t k : torches_) markOwn(k);
    for (int64_t k : wires_) markOwn(k);
    for (int64_t k : lamps_) markOwn(k);
    for (const auto& [k, c] : comps_) markOwn(k);
  } }  // LL_SIGNAL_RELINKSCOPE
  if (diag) {
    size_t own = 0; for (const NetTouch& t : netTouch_) own += t.own ? 1 : 0;
    ::litholattice::logInfo("signal rebuild: %.2f ms trigger=%d col=%d,%d %s nets=%zu wires=%zu comps=%zu lamps=%zu touch=%zu own=%zu cols=%zu",
                            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - diagT0).count(), diagKind_,
                            diagCx_, diagCz_, diagKind_ == 1 || diagKind_ == 2 || diagKind_ == 5 ? (diagOwn_ ? "has-cells" : "border-only") : "-",
                            nets_.size(), wires_.size(), comps_.size(), lamps_.size(), netTouch_.size(), own, w.columns().size());
  }
  diagKind_ = 0;
  netsValid_ = true;
  // PlayK fix 3+5: rebuild inverse reach + packed adjacency after nets settle.
  netReach_.assign(nets_.size(), {});
  for (const auto& [sk, r] : srcReach_)
    for (int32_t id : r.nets)
      if (size_t(id) < netReach_.size()) netReach_[size_t(id)].push_back(sk);
  rebuildCompAdj();
  phase_.rebuild += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - rebuildT0).count();
}
void cSignalSim::rebuildCompAdj() {
  // PlayL2 / MCHPRS #2: precomputed fanout edges (compAdj_). Option 16 (stable-subgraph IR /
  // Redpiler-style compile) would lower this graph to a straight-line tick â€” deferred.

  // PlayK fix 5: for each component, neighbour components inside the wake box (same geometry as wakeAround).
  compAdj_.clear();
  compAdj_.reserve(comps_.size());
  for (const auto& [k, c] : comps_) {
    (void)c;
    int x, y, z; unpackPos(k, x, y, z);
    std::vector<int64_t> adj;
    adj.reserve(16);
    for (int dy = -1; dy <= 2; ++dy)
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0 && dz == 0) continue;
          const int64_t n = packPos(x + dx, y + dy, z + dz);
          if (comps_.contains(n)) adj.push_back(n);
        }
    std::sort(adj.begin(), adj.end());
    adj.erase(std::unique(adj.begin(), adj.end()), adj.end());
    if (!adj.empty()) compAdj_.emplace(k, std::move(adj));
  }
}
void cSignalSim::fillSrcReach(int64_t k, SrcReach& r) {
  // PlayK fix 3: one 5x5x5 walk into reused scratch, then copy into the cached SrcReach.
  reachNetScratch_.clear(); reachLampScratch_.clear();
  int x, y, z; unpackPos(k, x, y, z);
  for (int dy = -2; dy <= 2; ++dy)
    for (int dz = -2; dz <= 2; ++dz)
      for (int dx = -2; dx <= 2; ++dx) {
        const int64_t n = packPos(x + dx, y + dy, z + dz);
        if (const auto it = wireNet_.find(n); it != wireNet_.end()) {
          if (std::find(reachNetScratch_.begin(), reachNetScratch_.end(), it->second) == reachNetScratch_.end())
            reachNetScratch_.push_back(it->second);
        } else if (lamps_.contains(n)) reachLampScratch_.push_back(n);
      }
  r.nets = reachNetScratch_;
  r.lamps = reachLampScratch_;
}
void cSignalSim::tickTorches(ChunkMap& w, const DirtyFn& dirty) {
  // 1) Torches: decide from the previous tick's state, then apply together.
  torchNext_.clear();
  torchNext_.reserve(torches_.size());
  for (int64_t k : torches_) {
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) continue;
    int sx, sy, sz;
    torchSupport(w.getState(x, y, z), x, y, z, sx, sy, sz);
    torchNext_.push_back({k, uint8_t(blockPowered(w, sx, sy, sz) ? 0 : 1)});  // a powered support turns it off
  }
  for (auto& [k, lit] : torchNext_) {
    int x, y, z; unpackPos(k, x, y, z);
    const CellState now = w.getState(x, y, z), next = withAttach(CellState(lit), stAttach(now));
    if (now != next) { w.setState(x, y, z, next); dirty(x, y, z); changedSrc_.push_back(k); }
  }
}

static CellState stateOf(const ChunkMap& w, int64_t k) { int x, y, z; unpackPos(k, x, y, z); return w.getState(x, y, z); }

namespace {
// Deterministic per-cell phase inside a divide period (rate scaling, JD
// 2026-10-14): divided components spread across the period instead of
// all firing on one tick Ã¢â‚¬â€ load spreading, the anti-hitch half of
// dividing. Pure function of the cell key: same world -> same phase on
// every run and platform. (Authority-registered components carry an
// explicit TickRate::phase instead; placed cells derive theirs here.)
uint32_t ratePhaseFor(int64_t key, uint32_t divide) {
  return uint32_t(litholattice::detMix64(uint64_t(key) + 0x9E3779B97F4A7C15ULL) % divide);
}
}  // namespace

litholattice::TickRate cSignalSim::classTickRate(BlockId block, const cCompBehavior* behavior) const {
  // An installed override was admitted only when valid and non-neutral
  // (setClassTickRate), so it needs no re-validation here.
  if (const auto it = classRates_.find(block); it != classRates_.end()) return it->second;
  // A-H2: the class-declared rate is validated once per BlockId and the
  // validated result cached (key lookup only Ã¢â‚¬â€ the cache is never
  // iterated, so no unordered order can reach execution).
  if (const auto it = declaredRates_.find(block); it != declaredRates_.end()) return it->second;
  // Archie Phase 4: POD rate when this behavior is the catalog component for `block`.
  litholattice::TickRate declared{};
  if (behavior) {
    const auto& cat = cBehaviorCatalog::get();
    if (cat.component(block) == behavior) declared = cat.traits(block).rate;
    else declared = behavior->tickRate();
  }
  litholattice::TickRate resolved = declared;
  if (!declared.valid()) {
    // Logged exactly once per class: after this, the cached neutral
    // result is what every later resolution returns.
    litholattice::logError(
        "signal: invalid class tick rate for block %u (multiply %u, divide %u) Ã¢â‚¬â€ substituting the neutral rate; "
        "a class rate must be powers of two with multiply <= %u and divide <= %u (review A-H2)",
        unsigned(block), declared.multiply, declared.divide, litholattice::kMaxRateMultiply,
        litholattice::kMaxRateDivide);
    resolved = litholattice::TickRate{};
  }
  declaredRates_[block] = resolved;
  return resolved;
}

// 2026-11-01 PlayM: one component's inputs (shared by the component pass and the same-tick settle).
bool cSignalSim::gatherInputs(ChunkMap& w, int64_t k, Comp& c, int x, int y, int z, bool fastInputs, bool publish, ComponentInputs& in) {
  const SimRole role = roleOf(blockRegistry().def(w.get(x, y, z)));
  if (role == SimRole::cSignalReceivor || role == SimRole::cDish) {
    int rs = 0, rc = 0;
    const int sens = role == SimRole::cDish ? kDishSensitivity : 0;
    if (!jammers_.jammed(x, y, z, c.mem.channel) &&
        radio_.query(c.mem.channel, x, y, z, dim_, sens, rs, rc)) { in.radioIn = rs; in.radioCol = uint8_t(rc); }
  }
  if (role == SimRole::Thresholdor && (stAux(stateOf(w, k)) & 2)) {  // 2026-10-25 roadmap 7: read mode only
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      const BlockId nb = w.get(nx, y, nz);
      if (nb == kInputBuffer) { in.level[d] = std::min(stValue(w.getState(nx, y, nz)), kMaxSignal); continue; }  // wafers held
      if (const auto nc = comps_.find(packPos(nx, y, nz)); nc != comps_.end())
        in.level[d] = nc->second.behavior->readLevel(w.getState(nx, y, nz), nc->second.mem);  // pre-tick state: order-free
    }
  }
  if (role == SimRole::Repeator) {  // 2026-10-31 (JD t3680u): a Repeator fed by a Repeator takes its colour
    const CellState rs = w.getState(x, y, z);
    const int bd = dirBack(stFacing(rs));
    const int bx = x + kDirX[bd], bz = z + kDirZ[bd];
    const CellState us = w.getState(bx, y, bz);
    const SimRole ur = roleOf(blockRegistry().def(w.get(bx, y, bz)));
    if (ur == SimRole::Repeator && stFacing(us) == stFacing(rs))
      in.chainCh = uint8_t(repeator::colourOf(us));  // upstream front faces our back
    // 2026-10-31 (JD): a Switch / Button right behind the back port powers it directly (attached
    // ones too, which otherwise only power their support) and makes the chain purple.
    if (ur == SimRole::Switch || ur == SimRole::cButton) {
      in.chainCh = 3;
      if (stOn(us)) in.switchIn = uint8_t(kMaxSignal);
    }
    // 2026-11-05 (JD): LEDs pass colour like Trace (behaviour.colour). A lit LED on the back port
    // feeds the Repeator chain colour the same way a Trace would.
    if (ur == SimRole::Torch) {
      const BlockDef& ub = blockRegistry().def(w.get(bx, y, bz));
      if (const int ledCh = cBehaviorCatalog::get().colour(ub)) in.chainCh = uint8_t(ledCh);
    }
  }
  if (role == SimRole::Catchor && cPulseRules::repeatorLinks()) {  // 2026-11-01 (JD, PlayK): Repeator -> Catchor direct
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      if (roleOf(blockRegistry().def(w.get(nx, y, nz))) == SimRole::Repeator && stFacing(w.getState(nx, y, nz)) == dirBack(d))
        in.repSides = uint8_t(in.repSides | (1u << d));
    }
  }
  if (role == SimRole::Randor) in.rng = randor::rollSeed(randor::worldSeed().load(std::memory_order_relaxed), k, tickNo_);  // 2026-10-31 (JD)
  if (role == SimRole::Redirector || role == SimRole::cFusor || role == SimRole::Repeator) {  // 2026-11-05 (JD): + Repeator side lock
    const int rf = stFacing(w.getState(x, y, z));
    for (int d : {dirLeft(rf), dirRight(rf)}) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      const CellState ns = w.getState(nx, y, nz);
      if (roleOf(blockRegistry().def(w.get(nx, y, nz))) == SimRole::Repeator && stFacing(ns) == dirBack(d)) {
        in.muxSides = uint8_t(in.muxSides | (1u << d));
        in.muxOn = in.muxOn || stOn(ns);
      }
    }
  }
  if (publish && role == SimRole::cOverclockor && c.mem.jammer)
    jammers_.publish(JammerZone{x, y, z, kJammerRadius, c.mem.channel});
  if (fastInputs && c.fast) {
    for (int d = 0; d < 4; ++d) {
      switch (c.kind[d]) {
        case 1: in.side[d] = cTraceConn::sideOn(*c.nb[d], -kDirX[d], 0, -kDirZ[d]) ? std::min(stValue(*c.nb[d]), kMaxSignal) : 0; in.ch[d] = c.ch[d]; break;  // SW-H2 clamp (see inputFrom)
        case 2: in.side[d] = stValue(*c.nb[d]) ? kMaxSignal : 0; in.ch[d] = c.ch[d]; break;
        case 3: in.ch[d] = c.ch[d]; break;
        case 4: in.side[d] = inputFrom(w, x, y, z, d); break;
        default: break;
      }
    }
    if (c.top) in.top = std::min(stValue(*c.top), kMaxSignal);  // SW-H2 clamp
    return true;
  }
  for (int d = 0; d < 4; ++d) {
    in.side[d] = inputFrom(w, x, y, z, d);
    const BlockDef nd = blockRegistry().def(w.get(x + kDirX[d], y, z + kDirZ[d]));
    if (const int sc = cBehaviorCatalog::get().colour(nd)) in.ch[d] = uint8_t(sc);  // 2026-11-05: Trace + LED colour
  }
  if (isFlatWire(w.get(x, y + 1, z))) in.top = std::min(stValue(w.getState(x, y + 1, z)), kMaxSignal);  // SW-H2 clamp
  return false;
}


namespace {
// PlayL2 #9: specialized ticks for ALU Stress diet (Repeator / Thresholdor). Fusor/Randor stay virtual
// (heavier / less frequent on the stress). Kill: LL_HOT_SPEC=0.
uint64_t hotRepLowMask(int bits) { return bits >= 64 ? ~uint64_t(0) : ((uint64_t(1) << bits) - 1u); }
uint64_t hotRepLoadLine(const ComponentMemory& m) {
  return uint64_t(m.history) | (uint64_t(m.timer) << 8) | (uint64_t(m.historyHi) << 16) | (uint64_t(m.prevIn) << 32) |
         (uint64_t(m.prevSide) << 40) | (uint64_t(m.prevClk) << 48) | (uint64_t(uint8_t(m.channel ^ 1u)) << 56);
}
void hotRepStoreLine(ComponentMemory& m, uint64_t line) {
  m.history = uint8_t(line); m.timer = uint8_t(line >> 8); m.historyHi = uint16_t(line >> 16);
  m.prevIn = uint8_t(line >> 32); m.prevSide = uint8_t(line >> 40); m.prevClk = uint8_t(line >> 48);
  m.channel = uint8_t(uint8_t(line >> 56) ^ 1u);
}
ComponentStep hotTickRepeator(const ComponentInputs& in, CellState st, ComponentMemory& m) {
  const int f = stFacing(st), idx = repeator::delayIndex(st), delay = repeator::kDelays[idx];
  const int L = dirLeft(f), R = dirRight(f);
  const bool locked = ((in.muxSides & (1u << L)) != 0 || (in.muxSides & (1u << R)) != 0) && in.muxOn;
  uint64_t line = hotRepLoadLine(m);
  if (locked) {
    const int out = (stOn(st) || m.out > 0) ? kMaxSignal : 0;
    const int col = repeator::colourOf(st);
    line = (line << 1) | (out > 0 ? 1u : 0u);
    hotRepStoreLine(m, line);
    return {out, 0, repeator::withColour(repeator::encode(out, f, idx, out > 0), col)};
  }
  static const bool kFresh = [] { const char* e = std::getenv("LL_REP_FRESH"); return !(e && e[0] == '0'); }();
  const int bv = std::max(in.back(f), int(in.switchIn));
  const bool fresh = kFresh && bv > 0 && m.lastIn > 0 && bv != m.lastIn;
  m.lastIn = uint8_t(std::min(bv, 255));
  line = (line << 1) | ((bv > 0 && !fresh) ? 1u : 0u);
  hotRepStoreLine(m, line);
  const int out = ((line >> (delay - 1)) & 1) ? kMaxSignal : 0;
  const int bch = in.ch[size_t(dirBack(f))] ? in.ch[size_t(dirBack(f))] : in.chainCh;
  const int col = bch ? bch : repeator::colourOf(st);
  return {out, 0, repeator::withColour(repeator::encode(out, f, idx, out > 0), col)};
}
ComponentStep hotTickThresholdor(const ComponentInputs& in, CellState st) {
  constexpr int kSubtract = 1, kRead = 2;
  const int f = stFacing(st), keep = stAux(st) & kRead;
  int back = in.back(f);
  if ((stAux(st) & kRead) != 0) back = std::max(back, in.level[dirBack(f)]);
  if ((stAux(st) & kSubtract) != 0) {
    const int v = std::min(back, kMaxSignal);
    const int side = std::min(std::max(in.left(f), in.right(f)), kMaxSignal);
    const int out = std::max(0, v - side);
    return {out, 0, makeState(out, f, kSubtract | keep, out > 0)};
  }
  const int v = std::min(back, kMaxSignal), lo = in.left(f), hi = in.right(f);
  const bool pass = (lo > 0 && hi > 0) ? (v >= std::min(lo, hi) && v <= std::max(lo, hi)) : v >= std::max(lo, hi);
  const int out = (pass && v > 0) ? v : 0;
  return {out, 0, makeState(v, f, keep, out > 0)};
}
}  // namespace

void cSignalSim::tickComponents(ChunkMap& w, const DirtyFn& dirty) {
  EpTimer ept(g_ep.comps);
  // 2) Components: new outputs from the previous tick's inputs (computed first, applied together).
  // PlayK fix 4: CompNext lives on the class; per-shard buffers merged in canonical key order.
  std::vector<CompNext> compNext;
  compNext.reserve(comps_.size());
  const bool all = allAwake_, fastInputs = netsValid_ && !fullPass_;
  lastTicked_ = 0;
  const bool sameTick = sameTick_;
  compSnap_.clear(); compRecs_.clear(); compRecIdx_.clear();
  // B-L1 (P3-2): iterate a SORTED key snapshot, not the unordered_map.
  // comps_ iteration order is hash order Ã¢â‚¬â€ it leaks into externally
  // visible ordering: compNext's apply order below is the order sounds_
  // entries are appended and radio_ broadcasts are published. Sorted
  // (packPos) order makes both deterministic and identical across runs
  // and paths. Component INPUTS are all read before any apply, so the
  // iteration order cannot change any component's result Ã¢â‚¬â€ only the
  // publish/sound order, which is exactly what this fixes.
  static const bool kTable = [] { const char* e = std::getenv("LL_COMP_TABLE"); return !(e && e[0] == '0'); }();
  static const bool kOptionA = [] { const char* e = std::getenv("LL_OPTION_A"); return !(e && e[0] == '0');  /* PlayL: default ON (JD 2026-11-01) */ }();  // 2026-11-01
  bool rebuiltNow = false;
  const TickArea& oa = compOrderArea_;
  if (!kTable || compOrderGen_ != compsGen_ || oa.pcx != area_.pcx || oa.pcz != area_.pcz || oa.radius != area_.radius ||
      oa.round != area_.round || oa.diamond != area_.diamond) {
    compOrder_.clear();
    for (const auto& [k, c] : comps_) {
      int x, y, z; unpackPos(k, x, y, z);
      if (!kTable || area_.contains(x, z)) compOrder_.push_back(k);  // in-range only (sim distance)
    }
    std::sort(compOrder_.begin(), compOrder_.end());
    compOrderGen_ = compsGen_; compOrderArea_ = area_; ++compTableRebuilds_;
    rebuiltNow = true;
    rebuildCompDense();
    compOrderPtrs_.resize(compOrder_.size());
    for (size_t i = 0; i < compOrder_.size(); ++i) {
      const auto it = comps_.find(compOrder_[i]);
      compOrderPtrs_[i] = (it == comps_.end()) ? nullptr : &it->second;
    }
    if (kOptionA) { if (!compTable_) compTable_ = std::make_shared<cCompiledCompTable>(); compTable_->rebuild(compOrder_); }
  } else if (compOrderPtrs_.size() != compOrder_.size()) {
    // PlayL2: pointers may be stale after a rehash without area/gen change â€” refresh.
    rebuildCompDense();
    compOrderPtrs_.resize(compOrder_.size());
    for (size_t i = 0; i < compOrder_.size(); ++i) {
      const auto it = comps_.find(compOrder_[i]);
      compOrderPtrs_[i] = (it == comps_.end()) ? nullptr : &it->second;
    }
  }
  // 2026-11-01 (JD LL_WAKE): with the wake list on, a pass that is not a full / rebuild pass iterates
  // only the queued components (timer ring slots up to this tick + awake_), sorted by coordinate (same
  // relative order as compOrder_, so apply / sound / radio order is unchanged). Each candidate still
  // goes through the exact skip test below, so the ticked set is identical (parity selftest).
  const bool wakeOn = wakeList_ && kTable;
  const bool wakeIter = wakeOn && !all && !rebuiltNow;
  if (wakeOn) {
    wakeScratch_.clear();
    // Drain every slot due since the last drain (at most the whole ring); entries are never more
    // than 63 ticks ahead, so a drained slot holds only due entries.
    if (tickNo_ > wakeDrained_) {
      const uint64_t span = std::min<uint64_t>(tickNo_ - wakeDrained_, 64);
      for (uint64_t t = tickNo_ - span + 1; t <= tickNo_; ++t) {
        auto& slot = wakeRing_[t & 63];
        if (wakeIter) wakeScratch_.insert(wakeScratch_.end(), slot.begin(), slot.end());
        slot.clear();
      }
      wakeDrained_ = tickNo_;
    }
    if (wakeIter) {
      wakeScratch_.insert(wakeScratch_.end(), awakeKeys_.begin(), awakeKeys_.end());
      std::sort(wakeScratch_.begin(), wakeScratch_.end());
      wakeScratch_.erase(std::unique(wakeScratch_.begin(), wakeScratch_.end()), wakeScratch_.end());
      // PlayL2: one hash resolve per wake key, then shards walk Contiguous Comp*.
      wakePtrs_.resize(wakeScratch_.size());
      for (size_t i = 0; i < wakeScratch_.size(); ++i) {
        const auto it = comps_.find(wakeScratch_[i]);
        wakePtrs_[i] = (it == comps_.end()) ? nullptr : &it->second;
      }
    }
  }
  auto wakeSched = [&](int64_t key, uint64_t delta) {
    const auto it = comps_.find(key);
    if (it == comps_.end()) return;
    if (delta == 0) delta = 1;
    if (delta > 63) delta = 1;  // far timers re-check next tick (the rate gate holds them)
    const uint64_t at = tickNo_ + delta;
    if (it->second.wakeAt == at) return;  // dedupe flag
    it->second.wakeAt = at;
    wakeRing_[at & 63].push_back(key);
  };
  const std::vector<int64_t>& iterKeys = wakeIter ? wakeScratch_ : compOrder_;
  // LL_TICK_PROFILE (measure only, 2026-11-01 Archie list): component call counts per tick, cumulative.
  static const bool kProfC = [] { const char* e = std::getenv("LL_TICK_PROFILE"); return e && e[0] == '1'; }();
  static uint64_t pcTicks = 0, pcIter = 0, pcRun = 0, pcComps = 0;
  if (kProfC) {
    ++pcTicks; pcIter += compOrder_.size(); pcComps += comps_.size();
    if (pcTicks % 4096 == 0)
      ::litholattice::logInfo("signal compcalls: ticks=%llu table=%s comps/tick %.1f iterated/tick %.1f dispatched/tick %.2f rebuilds %llu out-of-range %llu",
                              (unsigned long long)pcTicks, kTable ? "on" : "off", double(pcComps) / double(pcTicks), double(pcIter) / double(pcTicks),
                              double(pcRun) / double(pcTicks), (unsigned long long)compTableRebuilds_, (unsigned long long)compOutOfRange_);
  }
  const uint64_t pcRun0 = lastTicked_;
  struct RunTally { uint64_t& sum; const uint64_t& now; uint64_t base; bool on; ~RunTally() { if (on && now >= base) sum += now - base; } } pcTally{pcRun, lastTicked_, pcRun0, kProfC};
  // PlayK fix 4: shard awake list across the job pool. Contiguous ranges of sorted
  // iterKeys preserve canonical merge order (shard0 then shard1 â€¦). Per-shard
  // CompNext / wake / snap buffers; single waitIdle barrier; no mutex on the three
  // main threads. LL_COMP_SHARD=0 disables (serial). Small lists stay serial.
  static const bool kShard = [] { const char* e = std::getenv("LL_COMP_SHARD"); return !(e && e[0] == '0'); }();
  // PlayL2 lever 2: prefer dense Comp* (wakePtrs_ or compOrderPtrs_) over per-key hash find.
  const std::vector<Comp*>* iterPtrs = wakeIter ? &wakePtrs_ : &compOrderPtrs_;
  auto runRange = [&](size_t begin, size_t endEx, std::vector<CompNext>& out,
                      std::vector<std::pair<int64_t, uint64_t>>* wakes,
                      std::vector<std::pair<int64_t, CompSnap>>* snaps,
                      size_t* ticked, size_t* oor) {
    for (size_t ii = begin; ii < endEx; ++ii) {
      const int64_t k = iterKeys[ii];
      Comp* cp = (iterPtrs && ii < iterPtrs->size()) ? (*iterPtrs)[ii] : nullptr;
      if (!cp) {
        const auto cit = comps_.find(k);
        if (cit == comps_.end()) continue;  // LL_WAKE: a queued key whose component is gone
        cp = &cit->second;
      }
      Comp& c = *cp;
      int x, y, z; unpackPos(k, x, y, z);
      if (!all && !c.rateHeld && !wakeIter) {
        switch (c.quietMode) {
          case 1:
            if (!c.sched.pending() && !awakeContains(k)) continue;
            break;
          case 2:
            break;
          default:
            if (c.behavior->quiescent(stateOf(w, k), c.mem) && !c.sched.pending() && !awakeContains(k)) continue;
            break;
        }
      }
      if (!area_.contains(x, z)) { if (kTable) { if (oor) ++(*oor); else ++compOutOfRange_; } continue; }
      const BlockId bid = w.get(x, y, z);
      litholattice::TickRate rate = c.rate;
      if (const auto rit = classRates_.find(bid); rit != classRates_.end()) rate = rit->second;
      if (rate.divide > 1 && (tickNo_ % rate.divide) != uint64_t(ratePhaseFor(k, rate.divide))) {
        c.rateHeld = true;
        if (wakeOn) {
          const uint64_t ph = uint64_t(ratePhaseFor(k, rate.divide)), cur = tickNo_ % rate.divide;
          const uint64_t delta = (ph + rate.divide - cur) % rate.divide;
          if (wakes) wakes->push_back({k, delta});
          else wakeSched(k, delta);
        }
        continue;
      }
      c.rateHeld = false;
      if (ticked) ++(*ticked); else ++lastTicked_;
      ComponentInputs in;
      const bool fastUsed = gatherInputs(w, k, c, x, y, z, fastInputs, true, in);
      const CellState pre = fastUsed ? *c.self : w.getState(x, y, z);
      if (sameTick) {
        if (snaps) snaps->push_back({k, CompSnap{c.mem, pre}});
        else compSnap_.insert_or_assign(k, CompSnap{c.mem, pre});
      }
      ComponentStep step{};
      CellState cur = pre;
      // PlayL2 #9: specialized hot types (Repeator / Thresholdor). Fusor/Randor remain virtual.
      for (uint32_t s = 0; s < rate.multiply; ++s) {
        if (hotSpecOn() && c.hotKind == 1) step = hotTickRepeator(in, cur, c.mem);
        else if (hotSpecOn() && c.hotKind == 2) step = hotTickThresholdor(in, cur);
        else step = c.behavior->tick(in, cur, c.mem);
        cur = step.state;
      }
      step = scheduleOutput(k, c, w.get(x, y, z), step);
      // PlayL2 #10: timer wheel = existing wakeRing_ (64 slots). scheduleOutput/pending + wakeSchedule
      // already enqueue only the due bucket; wakeIter drains just that slot. No extra queue here.
      out.push_back({k, &c, step});
    }
  };

  unsigned nShards = 1;
  litholattice::cJobPool* pool = nullptr;
  if (kShard && iterKeys.size() >= 32) {
    pool = &litholattice::cJobPool::shared();
    nShards = std::min(8u, std::max(1u, pool->size()));
    if (nShards > unsigned(iterKeys.size())) nShards = unsigned(iterKeys.size());
    if (nShards < 2) nShards = 1;
  }
  if (nShards <= 1) {
    runRange(0, iterKeys.size(), compNext, nullptr, nullptr, nullptr, nullptr);
  } else {
    if (shardNext_.size() < nShards) shardNext_.resize(nShards);
    if (shardWake_.size() < nShards) shardWake_.resize(nShards);
    if (shardSnap_.size() < nShards) shardSnap_.resize(nShards);
    if (shardTicked_.size() < nShards) shardTicked_.assign(nShards, 0);
    else std::fill(shardTicked_.begin(), shardTicked_.begin() + nShards, size_t(0));
    if (shardOor_.size() < nShards) shardOor_.assign(nShards, 0);
    else std::fill(shardOor_.begin(), shardOor_.begin() + nShards, size_t(0));
    const size_t n = iterKeys.size(), base = n / nShards, rem = n % nShards;
    for (unsigned i = 0; i < nShards; ++i) {
      shardNext_[i].clear();
      shardWake_[i].clear();
      if (sameTick) shardSnap_[i].clear();
      const size_t start = size_t(i) * base + std::min(size_t(i), rem);
      const size_t len = base + (size_t(i) < rem ? 1 : 0);
      shardNext_[i].reserve(len);
      pool->submitUrgent([&, i, start, len] {
        runRange(start, start + len, shardNext_[i], &shardWake_[i],
                 sameTick ? &shardSnap_[i] : nullptr, &shardTicked_[i], &shardOor_[i]);
      });
    }
    pool->waitIdle();  // single atomic barrier; ownership returns to sim thread
    // Canonical merge: contiguous ranges of sorted iterKeys â†’ shard order = key order.
    size_t total = 0;
    for (unsigned i = 0; i < nShards; ++i) total += shardNext_[i].size();
    compNext.reserve(total);
    for (unsigned i = 0; i < nShards; ++i) {
      compNext.insert(compNext.end(), shardNext_[i].begin(), shardNext_[i].end());
      lastTicked_ += shardTicked_[i];
      if (kTable) compOutOfRange_ += shardOor_[i];
      for (const auto& [wk, wd] : shardWake_[i]) wakeSched(wk, wd);
      if (sameTick) for (const auto& [sk, ss] : shardSnap_[i]) compSnap_.insert_or_assign(sk, ss);
    }
  }

  for (const auto& [k, cp, step] : compNext) {
    ComponentMemory& m = cp->mem;
    // SW-H2 defense-in-depth at the store point: clamp TO kMaxSignal (see history).
    const int out = std::clamp(step.out, 0, kMaxSignal), out2 = std::clamp(step.out2, 0, kMaxSignal);
    const uint8_t o0 = m.out, o20 = m.out2; const bool p0 = m.pulse;
    const bool outChanged = m.out != out || m.out2 != out2 || m.pulse != step.pulse;
    if (outChanged || step.pulse) changedSrc_.push_back(k);  // 2026-11-01 PlayK: a held Pulsor pulse re-floods its run every tick (matches a full pass)
    m.out = uint8_t(out); m.out2 = uint8_t(out2); m.pulse = step.pulse;
    syncCompWords(*cp);  // PlayL2 #8: pack face/latch words for mount/emit
    int x, y, z; unpackPos(k, x, y, z);
    // PlayL2 #8: branchless-ish state publish (only write when bits differ).
    if (w.getState(x, y, z) != step.state) { w.setState(x, y, z, step.state); dirty(x, y, z); }
    compRecIdx_.emplace(k, compRecs_.size());
    compRecs_.push_back(CompRec{k, cp, step, o0, o20, p0, blockRegistry().def(w.get(x, y, z)).role == SimRole::cSignalTransmittor});  // PlayM: sounds / radio / wake / Option A deferred to hsFinalizeComponents
  }
}


// 2026-11-04 (JD t4304u): every component's output goes through its cScheduledOutput line.
ComponentStep cSignalSim::scheduleOutput(int64_t k, Comp& c, BlockId b, ComponentStep step) {
  (void)b;
  const bool queued = (c.traitFlags & cCompTraits::kQueuedOutput) != 0;
  const int d = !queued ? 0
      : (c.delayFixed >= 0) ? int(c.delayFixed)
      : c.behavior->delayTicks(step.state, k, tickNo_);
  const cScheduledOutput::Out raw{uint8_t(std::clamp(step.out, 0, kMaxSignal)), uint8_t(std::clamp(step.out2, 0, kMaxSignal)), step.pulse};
  const cScheduledOutput::Out pub = c.sched.step(tickNo_, d, raw);
  step.out = pub.out; step.out2 = pub.out2; step.pulse = pub.pulse;
  return step;
}

void cSignalSim::wakeSchedule(int64_t key, uint64_t delta) {  // PlayM: member twin of tickComponents' wakeSched
  const auto it = comps_.find(key);
  if (it == comps_.end()) return;
  if (delta == 0) delta = 1;
  if (delta > 63) delta = 1;
  const uint64_t at = tickNo_ + delta;
  if (it->second.wakeAt == at) return;
  it->second.wakeAt = at;
  wakeRing_[at & 63].push_back(key);
}

void cSignalSim::hsFinalizeComponents() {
  // 2026-11-01 PlayM: side effects of each component's FINAL step this tick, ascending key order
  // (= the pre-PlayM apply order), so sounds / radio publish order stay deterministic.
  const auto z0 = std::chrono::steady_clock::now();
  const bool wakeOn = wakeList_ && compTableOn(), kOptA = optionAOn();
  std::sort(compRecs_.begin(), compRecs_.end(), [](const CompRec& a, const CompRec& b) { return a.k < b.k; });
  for (const CompRec& r : compRecs_) {
    const ComponentMemory& m = r.c->mem;
    int x, y, z; unpackPos(r.k, x, y, z);
    const bool outChanged = m.out != r.out0 || m.out2 != r.out20 || m.pulse != r.pulse0;
    if (wakeOn) {
      WpTimer t;
      // PlayL Phase4 fold: one quiescent decision - reuse quietMode (Always/Never)
      // instead of a second virtual call; virtual only when quietMode==0.
      bool keepAwake = r.c->sched.pending();
      if (!keepAwake) {
        switch (r.c->quietMode) {
          case 1: break;                         // Always-quiet: no wake from quiescence
          case 2: keepAwake = true; break;       // Never-quiet
          default: keepAwake = !r.c->behavior->quiescent(r.step.state, m); break;
        }
      }
      if (keepAwake) wakeSchedule(r.k, 1);
      if (outChanged) for (int d = 0; d < 4; ++d) wakeSchedule(packPos(x + kDirX[d], y, z + kDirZ[d]), 1);
    }
    if (kOptA && compTable_) compTable_->stage(r.k, uint32_t(r.step.state));
    if (r.step.sound) sounds_.push_back({*r.step.sound, stValue(r.step.state), x, y, z});
    // Wireless: transmittors broadcast (strength = mem.out, colour = mem.out2), unless jammed.
    if (r.tx && m.out > 0 && m.channel >= 1 && m.channel <= radioChannelsUnlocked() && !jammers_.jammed(x, y, z, m.channel))
      radio_.publish(RadioBroadcast{x, y, z, dim_, m.channel, m.out, m.out2, kTransmittorPower});
  }
  compRecs_.clear(); compRecIdx_.clear(); compSnap_.clear();
  if (kOptA && compTable_) compTable_->commit();
  if (epOn()) phase_.finalize += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - z0).count();
}

bool cSignalSim::envSameTick() { return sameTickOn(); }

void cSignalSim::hsSettle(ChunkMap& w, const DirtyFn& dirty) {
  const auto settleT0 = std::chrono::steady_clock::now();  // PlayK fix 1: handshake settle split
  struct SettleTally { std::chrono::steady_clock::time_point t0; double& acc; bool on; ~SettleTally() { if (on) acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); } } settleTally{settleT0, phase_.settle, epOn()};
  // 2026-11-01 PlayM (JD): components read THIS tick's wire values. Every component already ran once this
  // tick (tickComponents) against the previous flood. Here, the ones whose inputs may have changed since
  // re-evaluate FROM THEIR PRE-TICK SNAPSHOT with fresh inputs (still one logical tick each: delay lines,
  // timers, Pulsor holds advance exactly once), in ascending key order with every input read before any
  // apply; changed outputs re-flood (hard + soft too) and the loop repeats until nothing changes.
  // Delays are kept because a delaying component's output comes from its memory, not this tick's input.
  // A combinational loop with no delay stops at LL_SETTLE_MAX (deterministic, counted in settleCapHits).
  if (hsSkip_) return;
  // N7: hard power is virtual (mountPowered_, or a torch below), so a changed
  // solid may have no CellState write to invalidate its render neighborhood.
  // hsMark has already recomputed mounts; forward that change to every reader
  // NOW, including unchanged-state neighbors and torch -> solid -> component.
  auto publishHardChanges = [&] {
    std::vector<int64_t> solids;
    const auto& reg = blockRegistry();
    auto add = [&](int x, int y, int z) {
      if (reg.hasAll(w.get(x, y, z), kSolid | kOpaque | kPowerable)) solids.push_back(packPos(x, y, z));
    };
    for (int64_t k : changedSrc_) {
      int x, y, z; unpackPos(k, x, y, z); add(x, y, z);
      if (reg.def(w.get(x, y, z)).role == SimRole::Torch) add(x, y + 1, z);
    }
    std::sort(solids.begin(), solids.end());
    solids.erase(std::unique(solids.begin(), solids.end()), solids.end());
    for (int64_t k : solids) {
      int x, y, z; unpackPos(k, x, y, z);
      if (std::find(changedSrc_.begin(), changedSrc_.end(), k) == changedSrc_.end()) changedSrc_.push_back(k);
      w.noteStateEdit(x, y, z); dirty(x, y, z);
      for (const auto& d : k6) {
        const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
        w.noteStateEdit(nx, ny, nz); dirty(nx, ny, nz);
        const int64_t n = packPos(nx, ny, nz);
        if (lamps_.contains(n)) hsLampKeys_.push_back(n);
      }
    }
    std::sort(hsLampKeys_.begin(), hsLampKeys_.end());
    hsLampKeys_.erase(std::unique(hsLampKeys_.begin(), hsLampKeys_.end()), hsLampKeys_.end());
  };
  publishHardChanges();
  if (!sameTick_) return;
  static const int kMax = [] { const char* e = std::getenv("LL_SETTLE_MAX"); const int v = e ? std::atoi(e) : 16; return v < 1 ? 1 : v; }();
  std::vector<int64_t> allSrc = changedSrc_;  // pass-0 sources, restored at the end
  std::vector<int64_t> iterSrc = changedSrc_;
  size_t softSeen = 0;
  for (int it = 0; it < kMax; ++it) {
    settleCand_.clear();
    const bool full = fullPass_ || !netsValid_;
    if (full) settleCand_ = compOrder_;
    else {
      for (int32_t id : hsMarkedNets_)
        if (size_t(id) < netComps_.size()) settleCand_.insert(settleCand_.end(), netComps_[size_t(id)].begin(), netComps_[size_t(id)].end());
      auto near = [&](int64_t k) {
        int x, y, z; unpackPos(k, x, y, z);
        static const int d6[7][3] = {{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}, {0, 1, 0}, {0, -1, 0}};
        for (const auto& d : d6) { const int64_t n = packPos(x + d[0], y + d[1], z + d[2]); if (comps_.contains(n)) settleCand_.push_back(n); }
      };
      for (int64_t k : iterSrc) near(k);
      for (size_t i = softSeen; i < softChanged_.size(); ++i) near(softChanged_[i]);
      std::sort(settleCand_.begin(), settleCand_.end());
      settleCand_.erase(std::unique(settleCand_.begin(), settleCand_.end()), settleCand_.end());
    }
    softSeen = softChanged_.size();
    if (settleCand_.empty()) break;
    const bool fastInputs = netsValid_ && !fullPass_;
    struct Re { int64_t k; Comp* c; ComponentStep step; ComponentMemory mem; };
    std::vector<Re> re;
    re.reserve(settleCand_.size());
    for (int64_t k : settleCand_) {  // phase 1: read every input (no writes)
      const auto cit = comps_.find(k);
      if (cit == comps_.end()) continue;
      Comp& c = cit->second;
      int x, y, z; unpackPos(k, x, y, z);
      if (!area_.contains(x, z)) continue;
      // A Repeator is a pure delay line: it keeps sampling at the start of the tick (pass 0), so its labelled
      // delay stays exact (re-reading here would make every Repeator one tick shorter; JD: delays stay).
      if (blockRegistry().def(w.get(x, y, z)).role == SimRole::Repeator) continue;
      // PlayL Phase5: packed rate at place; classRates_ override (RATE handshake / lithography) still wins.
    const BlockId bid = w.get(x, y, z);
    litholattice::TickRate rate = c.rate;
    if (const auto rit = classRates_.find(bid); rit != classRates_.end()) rate = rit->second;
      if (rate.divide > 1 && (tickNo_ % rate.divide) != uint64_t(ratePhaseFor(k, rate.divide))) continue;  // not its phase tick: holds
      auto sit = compSnap_.find(k);
      if (sit == compSnap_.end()) sit = compSnap_.emplace(k, CompSnap{c.mem, w.getState(x, y, z)}).first;  // slept in pass 0: state is pre-tick
      ComponentInputs in;
      gatherInputs(w, k, c, x, y, z, fastInputs, false, in);
      ComponentMemory mem = sit->second.mem;
      CellState cur = sit->second.state;
      ComponentStep step{};
      for (uint32_t s = 0; s < rate.multiply; ++s) { step = c.behavior->tick(in, cur, mem); cur = step.state; }
      step = scheduleOutput(k, c, w.get(x, y, z), step);  // 2026-11-04 (JD t4304u): same-tick settle replaces this tick's entry
      re.push_back({k, &c, step, std::move(mem)});
    }
    if (wpOn()) g_wp.settleComps += re.size();
    std::vector<int64_t> newSrc;
    for (Re& r : re) {  // phase 2: apply
      ComponentMemory& m = r.c->mem;
      const int out = std::clamp(r.step.out, 0, kMaxSignal), out2 = std::clamp(r.step.out2, 0, kMaxSignal);
      const bool outChanged = m.out != out || m.out2 != out2 || m.pulse != r.step.pulse;
      const uint8_t keepOut = m.out, keepOut2 = m.out2; const bool keepPulse = m.pulse;
      m = std::move(r.mem);
      m.out = uint8_t(out); m.out2 = uint8_t(out2); m.pulse = r.step.pulse;
      syncCompWords(*r.c);  // PlayL2 #8
      (void)keepOut; (void)keepOut2; (void)keepPulse;
      if (outChanged) newSrc.push_back(r.k);
      int x, y, z; unpackPos(r.k, x, y, z);
      if (w.getState(x, y, z) != r.step.state) { w.setState(x, y, z, r.step.state); dirty(x, y, z); }
      if (const auto ri = compRecIdx_.find(r.k); ri != compRecIdx_.end()) compRecs_[ri->second].step = r.step;
      else {
        const auto sn = compSnap_.find(r.k);
        compRecIdx_.emplace(r.k, compRecs_.size());
        compRecs_.push_back(CompRec{r.k, r.c, r.step, sn->second.mem.out, sn->second.mem.out2, sn->second.mem.pulse,
                                    blockRegistry().def(w.get(x, y, z)).role == SimRole::cSignalTransmittor});
      }
    }
    ++settleIterTotal_;
    if (wpOn()) ++g_wp.settleIt;
    if (newSrc.empty()) break;
    if (it + 1 == kMax) ++settleCapHits_;
    // re-flood from the changed outputs (and the hard-power changes they cause)
    if (full) {
      changedSrc_ = newSrc;
      collectMountPowered(w);
      hsPropagateFull(w, dirty);
      iterSrc = newSrc;
    } else {
      const std::vector<int64_t> lampsBefore = hsLampKeys_;
      std::vector<int64_t> awakeBefore;
      if (!allAwake_) awakeBefore = awakeKeys_;
      changedSrc_ = newSrc;
      hsMark(w);  // marks the reached nets / lamps, diffs hard power (appends mount changes to changedSrc_)
      publishHardChanges();  // outputs may change hard power during this same settle
      if (!allAwake_) for (int64_t ak : awakeBefore) awakeInsert(ak);
      if (!hsSkip_) {
        hsPrepIncremental(w);
        for (int32_t id : hsMarkedNets_) hsPropagateNet(w, id, dirty);
        hsApplyStrengthPending(&dirty);
      }
      hsSkip_ = false;
      std::vector<int64_t> merged;
      merged.reserve(lampsBefore.size() + hsLampKeys_.size());
      std::set_union(lampsBefore.begin(), lampsBefore.end(), hsLampKeys_.begin(), hsLampKeys_.end(), std::back_inserter(merged));
      hsLampKeys_.swap(merged);
      iterSrc = changedSrc_;
    }
    hsRefreshSoft(w);
    allSrc.insert(allSrc.end(), changedSrc_.begin(), changedSrc_.end());
  }
  changedSrc_.swap(allSrc);
}

bool cSignalSim::publishedComponentBits(int64_t key, uint32_t& out) const {
  return compTable_ && compTable_->published(key, out);
}

namespace {
// RAII ns timer into an atomic accumulator (hs* methods time themselves so
// takePhaseMs splits are real on the pull-worker path too).
struct HsTimer {
  std::atomic<uint64_t>& acc;
  std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
  ~HsTimer() {
    acc.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
                      std::chrono::steady_clock::now() - t0).count()),
                  std::memory_order_relaxed);
  }
};
}  // namespace

// ---- propagate: full pass (affine; the legacy propagateWires fullPass_ branch) ----

void cSignalSim::hsPropagateFull(ChunkMap& w, const DirtyFn& dirty) {
  ++netGen_;  // AC item 4: a full pass rewrites every wire
  EpTimer ept(g_ep.full);
  HsTimer timer{hsWiresNs_};
  const auto& reg = blockRegistry();
  // 3) Wires: reset, then bucket BFS from all sources (strongest first => max strength wins).
  std::vector<std::pair<int64_t, CellState>> old;
  old.reserve(wires_.size());
  auto resetWire = [&](int64_t k) {
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) return;
    old.push_back({k, w.getState(x, y, z)});
    w.setState(x, y, z, cTraceConn::withValue(w.getState(x, y, z), 0));  // PlayL: keep connection bits
  };
  for (int64_t k : wires_) resetWire(k);
  for (auto& b : buckets_) b.clear();
  for (auto& b : nbuckets_) b.clear();
  auto seed = [&](int x, int y, int z, int s, uint8_t channel, int viaAx = 0) {  // channel 0 = either colour; viaAx: Repeator emit axis
    const BlockDef d = reg.def(w.get(x, y, z));
    // SW-H2: guard the high end too Ã¢â‚¬â€ buckets_ has kMaxSignal + 1 entries, so
    // s in 65..255 (reachable via a disk-loaded value before the input clamps)
    // was an out-of-bounds array access.
    if (d.role != SimRole::Wire || (isVia(w.get(x, y, z)) && !(viaAx > 0 && viaAxis(w.get(x, y, z)) == viaAx)) || (channel && d.channel != channel) || s <= 0 || s > kMaxSignal) return;
    if (stValue(w.getState(x, y, z)) >= s || !area_.contains(x, z)) return;
    w.setState(x, y, z, cTraceConn::withValue(w.getState(x, y, z), s));
    buckets_[s].push_back(packPos(x, y, z));
  };
  for (auto& [k, lit] : torchNext_) {
    if (!lit) continue;
    int x, y, z; unpackPos(k, x, y, z);
    const uint8_t ch = reg.def(w.get(x, y, z)).channel;  // red torch -> red trace only
    for (auto& d : kH) seed(x + d[0], y, z + d[1], kMaxSignal, ch);
    seed(x, y - 1, z, kMaxSignal, ch);  // JD 2026-11-06: floating LED powers Trace beneath it too.
    if (strongPowered(w, x, y + 1, z))  // torch-powered block: colourless power to all 6 sides
      for (auto& d : k6) seed(x + d[0], y + 1 + d[1], z + d[2], kMaxSignal, 0);
  }
  std::vector<std::pair<int64_t, int>> pulses;
  // B-L1 acceptance (P3-2): this comps_ iteration stays unordered BY
  // DESIGN Ã¢â‚¬â€ it only SEEDS buckets/pulses. Bucket BFS resolves by max
  // strength and pulses flood a fixed strength, so the final wire states
  // are independent of seed order; nothing externally ordered (sounds,
  // radio) is emitted here. (tickComponents, which does emit those, uses
  // a sorted snapshot Ã¢â‚¬â€ see there.)
  for (const auto& [k, c] : comps_) {  // components drive either colour; pulses are flooded below
    if (!c.mem.out && !c.mem.out2) continue;
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) continue;
    const CellState st = w.getState(x, y, z);
    if (const int face = stAttach(st); face && (reg.def(w.get(x, y, z)).role == SimRole::Switch || reg.def(w.get(x, y, z)).role == SimRole::cButton)) {
      // Attached switch: powers its support (colourless, all 6 sides, like a torch-powered
      // block) and, mounted on top, the dust directly below the support (two-block reach).
      int ox, oy, oz;
      attachOffset(face, ox, oy, oz);
      const int sx = x + ox, sy = y + oy, sz = z + oz;
      for (auto& d : k6) seed(sx + d[0], sy + d[1], sz + d[2], kMaxSignal, 0);
      if (face == 1) seed(x, y - 2, z, kMaxSignal, 0);
      // 2026-10-24 (JD): an on switch also drives every Trace touching the switch itself (all 6
      // faces, red and blue alike), not only through its mount block.
      for (auto& d : k6) seed(x + d[0], y + d[1], z + d[2], kMaxSignal, 0);
      // 2026-10-24 (JD 5:57 PM) diagonal feed REMOVED 2026-10-31 (JD): Trace connects only
      // orthogonally. LL_SIGNAL_SWITCH_DIAGONAL=1 restores the old 4 same-level diagonals.
      if (kSwitchDiagonal)
        for (int ddx = -1; ddx <= 1; ddx += 2)
          for (int ddz = -1; ddz <= 1; ddz += 2) seed(x + ddx, y, z + ddz, kMaxSignal, 0);
      continue;
    }
    for (int d = 0; d < 4; ++d) {
      const int s = compEmit(c.behavior, c.mem, c.emitMode, d, st);
      if (s <= 0) continue;
      if (c.mem.pulse) pulses.push_back({packPos(x + kDirX[d], y, z + kDirZ[d]), s});
      else if (cTraceConn::sideOn(w.getState(x + kDirX[d], y, z + kDirZ[d]), -kDirX[d], 0, -kDirZ[d]))  // PlayL: connected side only
        seed(x + kDirX[d], y, z + kDirZ[d], s, uint8_t(compEmitChan(c.behavior, c.mem, c.traitFlags, d, st)),
           reg.def(w.get(x, y, z)).role == SimRole::Repeator ? (kDirX[d] ? 1 : 2) : 0);  // Repeator feeds a side via end-on
      // Hard-powered block in front of a Repeator: colourless power to its other five faces.
      if (mountPowered_.contains(packPos(x + kDirX[d], y, z + kDirZ[d])) && (cPulseRules::hardAnyComponent() || reg.def(w.get(x, y, z)).role == SimRole::Repeator || reg.def(w.get(x, y, z)).role == SimRole::Pulsor || w.get(x, y, z) == kRedirectorSelect))
        for (auto& e : k6) if (!(e[1] == 0 && e[0] == -kDirX[d] && e[2] == -kDirZ[d])) {
          const int nx = x + kDirX[d] + e[0], ny = y + e[1], nz = z + kDirZ[d] + e[2];
          seed(nx, ny, nz, s, 0);  // not back into the source face
          // N8: a solid relays the same Pulsor packet, not an attenuating DC seed.
          if (c.mem.pulse) pulses.push_back({packPos(nx, ny, nz), s});
        }
    }
  }
  // PlayN (JD hard-power): seed every face of each hard-powered block (torch semantics).
  for (const auto& [bk, hs] : mountPowered_) {
    int bx, by, bz; unpackPos(bk, bx, by, bz);
    if (!area_.contains(bx, bz)) continue;
    for (auto& d : k6) seed(bx + d[0], by + d[1], bz + d[2], int(hs), 0);
  }
  // Neighbours of a wire cell along the run (same channel; one-block stairs).
  auto forEachLink = [&](int x, int y, int z, auto&& fn) {
    const uint8_t channel = reg.def(w.get(x, y, z)).channel;
    const BlockId self = w.get(x, y, z);
    const bool selfVia = isVia(self);
    for (int dy = -1; dy <= 1; dy += 2) {  // via links (vertical, lossless)
      const BlockId nb = w.get(x, y + dy, z);
      if (isWire(nb) && reg.def(nb).channel == channel && (selfVia || isVia(nb)) && viaVertOk(self, nb) &&
          cTraceConn::sideOn(w.getState(x, y, z), 0, dy, 0) && cTraceConn::sideOn(w.getState(x, y + dy, z), 0, -dy, 0)) fn(x, y + dy, z);
    }
    if (selfVia) {
      for (auto& d : kH) {
        const BlockId nb = w.get(x + d[0], y, z + d[1]);
        if (isWire(nb) && reg.def(nb).channel == channel && viaSideOk(self, nb, d[0]) && area_.contains(x + d[0], z + d[1])) fn(x + d[0], y, z + d[1]);
      }
      return;
    }
    const bool upBlocked = reg.has(w.get(x, y + 1, z), kCutsWire);
    for (auto& d : kH) {
      const int nx = x + d[0], nz = z + d[1];
      for (int dy = -1; dy <= 1; ++dy) {
        if (dy == 1 && upBlocked) continue;        // climbing needs headroom above this wire
        if (dy == -1 && reg.has(w.get(nx, y, nz), kCutsWire)) continue;  // descending past a block edge
        const BlockDef nd = reg.def(w.get(nx, y + dy, nz));
        if (nd.role != SimRole::Wire || nd.channel != channel) continue;  // red never meets blue
        if (isVia(w.get(nx, y + dy, nz)) && !(dy == 0 && viaSideOk(self, w.get(nx, y, nz), d[0]))) continue;  // only a side via's open end
        if (!area_.contains(nx, nz)) continue;
        if (!cTraceConn::sideOn(w.getState(x, y, z), d[0], 0, d[1]) || !cTraceConn::sideOn(w.getState(nx, y + dy, nz), -d[0], 0, -d[1])) continue;  // PlayL connection states
        fn(nx, y + dy, nz);
      }
    }
  };
  for (int s = kMaxSignal; s >= 1; --s)
    for (size_t i = 0; i < buckets_[s].size(); ++i) {
      int x, y, z; unpackPos(buckets_[s][i], x, y, z);
      if (stValue(w.getState(x, y, z)) != s) continue;  // superseded by a stronger source
      const bool selfVia = isVia(w.get(x, y, z));
      forEachLink(x, y, z, [&](int nx, int ny, int nz) {
        const int ns = (selfVia || isVia(w.get(nx, ny, nz))) ? s : s - 1;  // vias are lossless
        if (ns <= 0 || stValue(w.getState(nx, ny, nz)) >= ns) return;
        w.setState(nx, ny, nz, CellState(ns));
        // B-M4 (P3-2): NO direct strengthBuf_ write here any more. The
        // incremental path records only FINAL changed cells (per-net
        // pending lists, replayed by hsApplyStrengthPending); this loop
        // wrote every transient BFS improvement, so the full pass's
        // buffer writes diverged from the incremental path's. The final
        // loop below now records the same changed-cell set instead.
        buckets_[ns].push_back(packPos(nx, ny, nz));
      });
    }
  // Pulsor pulses: the whole connected run (up to 64 blocks away) carries the data strength, no decay.
  for (auto& [k0, s] : pulses) {
    if (s <= 0 || s > kMaxSignal) continue;  // SW-H2: out-of-range pulse strengths never enter a cell
    int x, y, z; unpackPos(k0, x, y, z);
    if (!isFlatWire(w.get(x, y, z)) || !inScope(k0)) continue;
    std::vector<int64_t> frontier{k0};
    std::unordered_set<int64_t> seen{k0};
    for (int dist = 0; dist < kMaxSignal && !frontier.empty(); ++dist) {
      std::vector<int64_t> next;
      for (size_t fi = 0; fi < frontier.size(); ++fi) {  // via hops cost no reach: same frontier
        int a, b, c; unpackPos(frontier[fi], a, b, c);
        if (stValue(w.getState(a, b, c)) < s) w.setState(a, b, c, cTraceConn::withValue(w.getState(a, b, c), s));
        const bool selfVia = isVia(w.get(a, b, c));
        forEachLink(a, b, c, [&](int nx, int ny, int nz) {
          const int64_t nk = packPos(nx, ny, nz);
          if (seen.insert(nk).second) (selfVia || isVia(w.get(nx, ny, nz)) ? frontier : next).push_back(nk);
        });
      }
      frontier.swap(next);
    }
  }
  // B-M4 (P3-2): record the FINAL changed-cell set through the same
  // pending-list mechanism the incremental path uses (NetWork::strength +
  // hsApplyStrengthPending): every wire whose end-of-pass state differs
  // from its pre-pass state, in `old` (wire visit) order, with its final
  // strength. Seeds, BFS improvements and pulse floods all terminate in
  // these cells, so this set is exactly the incremental path's set.
  std::vector<std::pair<int64_t, uint8_t>> strengthPending;
  for (auto& [k, prev] : old) {
    int x, y, z; unpackPos(k, x, y, z);
    if (w.getState(x, y, z) != prev) {
      dirty(x, y, z);
      strengthPending.push_back({k, uint8_t(stValue(w.getState(x, y, z)))});
    }
  }
  for (const auto& [key, s] : strengthPending) strengthBuf_.set(strengthBuf_.slotFor(uint64_t(key)), s);
}

// ---- propagate: incremental prep (affine; reset + record per-net work) ----
// The seed/pulse loops below are the legacy propagateWires loops, except the
// effects are RECORDED per net (in the exact legacy visit order) instead of
// applied into the shared bucket arrays: the shared nbuckets_ and the
// TraceStrengthBuffer are the two thread hazards of the legacy code
// (TraceStrengthBuffer::set is NOT a plain slot store Ã¢â‚¬â€ it mutates a shared
// dirty-word bitset, and slotFor() mutates the slot map Ã¢â‚¬â€ see
// hs-integration-notes). hsPropagateNet then applies a net's recorded seeds
// to net-local buckets, so the per-net result is the legacy result (cross-net
// interleaving in the legacy global bucket loop cannot change a net's final
// state: nets are disjoint, and a net's entries keep their relative order).

void cSignalSim::hsPrepIncremental(ChunkMap& w) {
  HsTimer timer{hsWiresNs_};
  const auto& reg = blockRegistry();
  if (hsNetWork_.size() != netArr_.size()) hsNetWork_.assign(netArr_.size(), NetWork{});
  // PlayL2 lever 3: clear work lists only here. Wire zero + noteStateEdit move into
  // hsPropagateNet after the same-seed check (LL_AC_DEFER_ZERO), so stable nets skip
  // zero+restore. =0 restores the legacy zero-before-seed behaviour.
  const bool deferZero = acDeferZeroOn();
  for (int32_t id : hsMarkedNets_) {
    NetWork& nw = hsNetWork_[size_t(id)];
    nw.seeds.clear(); nw.pulses.clear(); nw.strength.clear();
    if (deferZero) continue;
    NetArr& a = netArr_[size_t(id)];
    for (size_t i = 0; i < a.st.size(); ++i) { a.old[i] = *a.st[i]; *a.st[i] = cTraceConn::withValue(*a.st[i], 0); }
    // M9 (Archie ruling 4): this cycle rewrites the net's wire state through cached
    // pointers (here and in hsPropagateNet, no DirtyFn): mark each touched sub-chunk stale
    // so the CoW snapshot is republished in the next batched publish.
    int64_t lastSub = INT64_MIN;
    for (int64_t k : a.keys) {
      int x, y, z; unpackPos(k, x, y, z);
      const int64_t sub = packPos(x >> 4, y >> 4, z >> 4);
      if (sub != lastSub) { w.noteStateEdit(x, y, z); lastSub = sub; }
    }
  }
  for (auto& b : nbuckets_) b.clear();  // legacy hygiene (the global buckets are unused by the per-net flood)
  auto seed = [&](int x, int y, int z, int s, uint8_t channel, int viaAx = 0) {  // channel 0 = either colour; viaAx: Repeator emit axis
    const BlockDef d = reg.def(w.get(x, y, z));
    // SW-H2: same high-end guard as the full-pass seed (the recorded strength
    // is later applied via lb[size_t(s)] in hsPropagateNet, sized kMaxSignal + 1).
    if (d.role != SimRole::Wire || (isVia(w.get(x, y, z)) && !(viaAx > 0 && viaAxis(w.get(x, y, z)) == viaAx)) || (channel && d.channel != channel) || s <= 0 || s > kMaxSignal) return;
    const auto it = wireLoc_.find(packPos(x, y, z));
    if (it == wireLoc_.end() || !isNetMarked(it->second.first)) return;
    hsNetWork_[size_t(it->second.first)].seeds.push_back({it->second.second, s});
  };
  // AC item 2 (LL_AC_SEEDS): a source seeds only wires inside its 5 x 5 x 5 box (every seed()
  // offset below is within 2), so a source whose cached box nets (srcReach_) hold no marked net
  // adds nothing: skip it before reading its state or behaviour.
  auto touchesMarked = [&](int64_t k) {
    if (!acSeedsOn()) return true;
    auto [rit, fresh] = srcReach_.try_emplace(k);
    SrcReach& r = rit->second;
    if (fresh) fillSrcReach(k, r);  // PlayK fix 3
    for (int32_t id : r.nets) if (isNetMarked(id)) return true;
    return false;
  };
  for (auto& [k, lit] : torchNext_) {
    if (!lit) continue;
    if (!touchesMarked(k)) continue;
    int x, y, z; unpackPos(k, x, y, z);
    const uint8_t ch = reg.def(w.get(x, y, z)).channel;  // red torch -> red trace only
    for (auto& d : kH) seed(x + d[0], y, z + d[1], kMaxSignal, ch);
    seed(x, y - 1, z, kMaxSignal, ch);  // JD 2026-11-06: floating LED powers Trace beneath it too.
    if (strongPowered(w, x, y + 1, z))  // torch-powered block: colourless power to all 6 sides
      for (auto& d : k6) seed(x + d[0], y + 1 + d[1], z + d[2], kMaxSignal, 0);
  }
  // B-L1 acceptance (P3-2): unordered comps_ iteration is safe here for
  // the same reason as in hsPropagateFull Ã¢â‚¬â€ this loop only RECORDS seeds
  // and pulses into per-net work lists; each net's flood resolves by max
  // strength independent of record order across nets, and nets are
  // disjoint. No externally visible ordering originates here.
  rebuildCompDense();
  for (size_t ci = 0; ci < compAllKeys_.size(); ++ci) {  // PlayL2: dense Comp* seed gather
    const int64_t k = compAllKeys_[ci];
    Comp& c = *compAllPtrs_[ci];
    if (!c.mem.out && !c.mem.out2) continue;
    if (!touchesMarked(k)) continue;
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) continue;
    const CellState st = w.getState(x, y, z);
    if (const int face = stAttach(st); face && (reg.def(w.get(x, y, z)).role == SimRole::Switch || reg.def(w.get(x, y, z)).role == SimRole::cButton)) {
      int ox, oy, oz;
      attachOffset(face, ox, oy, oz);
      const int sx = x + ox, sy = y + oy, sz = z + oz;
      for (auto& d : k6) seed(sx + d[0], sy + d[1], sz + d[2], kMaxSignal, 0);
      if (face == 1) seed(x, y - 2, z, kMaxSignal, 0);
      // 2026-10-24 (JD): an on switch also drives every Trace touching the switch itself (all 6
      // faces, red and blue alike), not only through its mount block.
      for (auto& d : k6) seed(x + d[0], y + d[1], z + d[2], kMaxSignal, 0);
      // 2026-10-24 (JD 5:57 PM) diagonal feed REMOVED 2026-10-31 (JD): Trace connects only
      // orthogonally. LL_SIGNAL_SWITCH_DIAGONAL=1 restores the old 4 same-level diagonals.
      if (kSwitchDiagonal)
        for (int ddx = -1; ddx <= 1; ddx += 2)
          for (int ddz = -1; ddz <= 1; ddz += 2) seed(x + ddx, y, z + ddz, kMaxSignal, 0);
      continue;
    }
    for (int d = 0; d < 4; ++d) {
      const int s = compEmit(c.behavior, c.mem, c.emitMode, d, st);
      if (s <= 0) continue;
      if (c.mem.pulse) {
        const int64_t target = packPos(x + kDirX[d], y, z + kDirZ[d]);
        const auto it = wireLoc_.find(target);
        if (it != wireLoc_.end() && isNetMarked(it->second.first) &&
            !netArr_[size_t(it->second.first)].via[size_t(it->second.second)])
          hsNetWork_[size_t(it->second.first)].pulses.push_back({it->second.second, s});
      } else if (emitCacheOn() && c.emitNet[d] >= 0 && s <= kMaxSignal && isNetMarked(c.emitNet[d]) &&
                 !netArr_[size_t(c.emitNet[d])].via[size_t(c.emitIdx[d])] &&  // PlayM2: vias (viaAx) + coloured emits take seed()
                 compEmitChan(c.behavior, c.mem, c.traitFlags, d, st) == 0 &&
                 cTraceConn::sideOn(w.getState(x + kDirX[d], y, z + kDirZ[d]), -kDirX[d], 0, -kDirZ[d])) {
        hsNetWork_[size_t(c.emitNet[d])].seeds.push_back({c.emitIdx[d], s});
      } else if (cTraceConn::sideOn(w.getState(x + kDirX[d], y, z + kDirZ[d]), -kDirX[d], 0, -kDirZ[d]))  // PlayL: connected side only
        seed(x + kDirX[d], y, z + kDirZ[d], s, uint8_t(compEmitChan(c.behavior, c.mem, c.traitFlags, d, st)),
           reg.def(w.get(x, y, z)).role == SimRole::Repeator ? (kDirX[d] ? 1 : 2) : 0);  // Repeator feeds a side via end-on
      // Hard-powered block in front of a Repeator: colourless power to its other five faces.
      if (mountPowered_.contains(packPos(x + kDirX[d], y, z + kDirZ[d])) && (cPulseRules::hardAnyComponent() || reg.def(w.get(x, y, z)).role == SimRole::Repeator || reg.def(w.get(x, y, z)).role == SimRole::Pulsor || w.get(x, y, z) == kRedirectorSelect))
        for (auto& e : k6) if (!(e[1] == 0 && e[0] == -kDirX[d] && e[2] == -kDirZ[d])) {
          const int nx = x + kDirX[d] + e[0], ny = y + e[1], nz = z + kDirZ[d] + e[2];
          seed(nx, ny, nz, s, 0);  // not back into the source face
          // N8: preserve pulse transport through the solid in THIS tick's flood.
          if (c.mem.pulse) {
            const auto wi = wireLoc_.find(packPos(nx, ny, nz));
            if (wi != wireLoc_.end() && isNetMarked(wi->second.first) &&
                !netArr_[size_t(wi->second.first)].via[size_t(wi->second.second)])
              hsNetWork_[size_t(wi->second.first)].pulses.push_back({wi->second.second, s});
          }
        }
    }
  }
  // PlayN (JD hard-power): seed every face of each hard-powered block (torch semantics).
  // PlayL2 lever 3: skip mounts whose 5x5x5 box touches no marked net (AC seeds already filters
  // each seed(); this avoids unpacking every mount on quiet ticks).
  for (const auto& [bk, hs] : mountPowered_) {
    if (!touchesMarked(bk)) continue;
    int bx, by, bz; unpackPos(bk, bx, by, bz);
    if (!area_.contains(bx, bz)) continue;
    for (auto& d : k6) seed(bx + d[0], by + d[1], bz + d[2], int(hs), 0);
  }
}

// ---- propagate: one net's flood (pull-safe; touches only this net's NetArr
// span and its own NetWork record). Never touches ChunkMap structure, the
// shared buckets, slotFor/TraceStrengthBuffer, or the real DirtyFn (the
// incremental flood reports through the strength buffer only, as legacy).

void cSignalSim::hsPropagateNet(ChunkMap& w, int32_t netId, const DirtyFn& /*dirty*/) {
  HsTimer timer{hsWiresNs_};
  NetArr& a = netArr_[size_t(netId)];
  NetWork& nw = hsNetWork_[size_t(netId)];
  // AC item 4 (LL_AC_SAME_SEEDS): no pulses and the same seeds as this net's last flood since the
  // nets were built / the last full pass (netGen_) -> the flood result is the old one: restore it
  // and skip the spread. Pulses (a held Pulsor) always re-send. Wire values change only through
  // this net's flood or a full pass, so a.old is exactly that last result.
  // PlayL2: with LL_AC_DEFER_ZERO, wires were NOT cleared in prep â€” a same-seed hit leaves them
  // untouched (already the last flood). A miss zeros here before spreading.
  if (acSameSeedsOn()) {
    std::sort(nw.seeds.begin(), nw.seeds.end());
    if (nw.pulses.empty() && nw.lastGen == netGen_ && nw.seeds == nw.lastSeeds) {
      if (!acDeferZeroOn()) {
        for (size_t i = 0; i < a.st.size(); ++i) *a.st[i] = a.old[i];
      }
      nw.strength.clear();
      sameSeedSkips_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    nw.lastSeeds = nw.seeds;
    nw.lastGen = nw.pulses.empty() ? netGen_ : 0;  // pulse values are not a seed-only result
  }
  if (acDeferZeroOn()) {
    for (size_t i = 0; i < a.st.size(); ++i) { a.old[i] = *a.st[i]; *a.st[i] = cTraceConn::withValue(*a.st[i], 0); }
    int64_t lastSub = INT64_MIN;
    for (int64_t k : a.keys) {
      int x, y, z; unpackPos(k, x, y, z);
      const int64_t sub = packPos(x >> 4, y >> 4, z >> 4);
      if (sub != lastSub) { w.noteStateEdit(x, y, z); lastSub = sub; }
    }
  }
  thread_local std::array<std::vector<int32_t>, kMaxSignal + 1> lb;
  for (auto& b : lb) b.clear();
  // PlayL2 #11: optional SoA flood into a.level (contiguous bytes), then one sync to CellState*.
  const bool soa = netLevelSoaOn() && a.level.size() == a.st.size();
  if (soa) {
    std::fill(a.level.begin(), a.level.end(), uint8_t(0));
    for (const auto& [li, s] : nw.seeds) {
      if (s <= 0 || s > kMaxSignal) continue;
      if (a.level[size_t(li)] >= uint8_t(s)) continue;
      a.level[size_t(li)] = uint8_t(s);
      lb[size_t(s)].push_back(li);
    }
    for (int s = kMaxSignal; s >= 1; --s)
      for (size_t i = 0; i < lb[size_t(s)].size(); ++i) {
        const int32_t li = lb[size_t(s)][i];
        if (int(a.level[size_t(li)]) != s) continue;
        for (int32_t e = a.adjStart[size_t(li)]; e < a.adjStart[size_t(li) + 1]; ++e) {
          const int32_t nj = a.adj[size_t(e)];
          const int ns = (a.via[size_t(li)] || a.via[size_t(nj)]) ? s : s - 1;
          if (ns <= 0 || int(a.level[size_t(nj)]) >= ns) continue;
          a.level[size_t(nj)] = uint8_t(ns);
          lb[size_t(ns)].push_back(nj);
        }
      }
    for (size_t i = 0; i < a.st.size(); ++i)
      *a.st[i] = cTraceConn::withValue(*a.st[i], int(a.level[i]));
  } else {
  // Apply this net's recorded seeds, in legacy seed order, to net-local buckets.
  for (const auto& [li, s] : nw.seeds) {
    if (s <= 0 || s > kMaxSignal) continue;  // SW-H2 defense-in-depth: lb is sized kMaxSignal + 1
    CellState* p = a.st[size_t(li)];
    if (stValue(*p) >= s) continue;
    *p = cTraceConn::withValue(*p, s);
    lb[size_t(s)].push_back(li);
  }
  for (int s = kMaxSignal; s >= 1; --s)
    for (size_t i = 0; i < lb[size_t(s)].size(); ++i) {  // size re-read: lossless via links append to s
      const int32_t li = lb[size_t(s)][i];
      if (stValue(*a.st[size_t(li)]) != s) continue;
      for (int32_t e = a.adjStart[size_t(li)]; e < a.adjStart[size_t(li) + 1]; ++e) {
        const int32_t nj = a.adj[size_t(e)];
        const int ns = (a.via[size_t(li)] || a.via[size_t(nj)]) ? s : s - 1;
        CellState* p = a.st[size_t(nj)];
        if (ns <= 0 || stValue(*p) >= ns) continue;
        *p = cTraceConn::withValue(*p, ns);
        lb[size_t(ns)].push_back(nj);
      }
    }
  }  // !soa
  thread_local std::vector<int32_t> frontier, next;
  for (const auto& [li0, s] : nw.pulses) {
    if (s <= 0 || s > kMaxSignal) continue;  // SW-H2: never write an out-of-range strength into a cell
    const uint32_t gen = ++a.pulseGen;
    frontier.assign(1, li0); a.stamp[size_t(li0)] = gen;
    for (int dist = 0; dist < kMaxSignal && !frontier.empty(); ++dist) {
      next.clear();
      for (size_t fi = 0; fi < frontier.size(); ++fi) {  // via hops cost no reach: same frontier
        const int32_t li = frontier[fi];
        if (stValue(*a.st[size_t(li)]) < s) *a.st[size_t(li)] = cTraceConn::withValue(*a.st[size_t(li)], s);
        for (int32_t e = a.adjStart[size_t(li)]; e < a.adjStart[size_t(li) + 1]; ++e) {
          const int32_t nj = a.adj[size_t(e)];
          if (a.stamp[size_t(nj)] == gen) continue;
          a.stamp[size_t(nj)] = gen;
          (a.via[size_t(li)] || a.via[size_t(nj)] ? frontier : next).push_back(nj);
        }
      }
      frontier.swap(next);
    }
  }
  nw.strength.clear();
  for (size_t i = 0; i < a.st.size(); ++i)
    if (*a.st[i] != a.old[i]) {
      // Step 1: strength -> buffer slot, no remesh. Deferred: slotFor/set are
      // not thread-safe; hsApplyStrengthPending replays these on the sim
      // thread in canonical (net, cell) order Ã¢â‚¬â€ the legacy final-loop order.
      nw.strength.push_back({a.keys[i], uint8_t(stValue(*a.st[i]))});
    }
}

void cSignalSim::hsApplyStrengthPending(const DirtyFn* dirty) {
  HsTimer timer{hsWiresNs_};
  // 2026-11-01 (JD wire render bug): the mesher bakes each Trace cell's value into the chunk mesh (no shader
  // reads the strength buffer yet), so an incremental flood that changed a wire must queue its sub-chunk for a
  // remesh, exactly as the full pass does. Serial, sim thread. LL_WIRE_REMESH=0: old (buffer only).
  // PlayL2 / MCHPRS #3: dirty() here only marks for later remesh â€” must not block the signal phase
  // on mesh build / UI send (rtps vs wsr split). Ownership handoff keeps remesh off the signal critical path.
  static const bool kRemesh = [] { const char* e = std::getenv("LL_WIRE_REMESH"); return !(e && e[0] == '0'); }();
  for (int32_t id : hsMarkedNets_) {
    NetWork& nw = hsNetWork_[size_t(id)];
    for (const auto& [key, s] : nw.strength) strengthBuf_.set(strengthBuf_.slotFor(uint64_t(key)), s);
    if (kRemesh && dirty && *dirty)
      for (const auto& [key, s] : nw.strength) { int x, y, z; unpackPos(key, x, y, z); (*dirty)(x, y, z); }
    if (acWakeOn() && !allAwake_) {  // AC item 1: readers within 2 of a changed wire (a wire -> block -> reader hop)
      WpTimer wpt;
      for (const auto& [key, s] : nw.strength) {
        int x, y, z; unpackPos(key, x, y, z);
        for (int dy = -2; dy <= 2; ++dy)
          for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx)
              if (const int64_t n = packPos(x + dx, y + dy, z + dz); comps_.contains(n)) awakeInsert(n);
      }
    }
    nw.strength.clear();
  }
}

// ---- lamps ----

std::vector<cSignalSim::LampHoldUpdate>& cSignalSim::hsTlsLampHolds() {
  thread_local std::vector<LampHoldUpdate> v;
  return v;
}

void cSignalSim::hsApplyLampHolds(const std::vector<LampHoldUpdate>& updates) {
  for (const LampHoldUpdate& u : updates) {
    if (u.delay) {  // PlayB lamp delay line
      if (u.erase) lampDelay_.erase(u.key); else lampDelay_[u.key] = {u.ns, u.end};
      continue;
    }
    if (u.erase) lampHold_.erase(u.key);
    else {
      lampHold_[u.key] = u.end;
      nextHoldExpiry_ = std::min(nextHoldExpiry_, u.end);
    }
  }
}

void cSignalSim::hsLampsBatch(const std::vector<int64_t>& keys, ChunkMap& w, const DirtyFn& dirty) {
  EpTimer ept(g_ep.lamps);
  HsTimer timer{hsLampsNs_};
  const auto& reg = blockRegistry();
  std::vector<LampHoldUpdate>& holds = hsTlsLampHolds();
  holds.clear();
  // 4) Lamps (terminate the signal): plain lamps light on any adjacent power; color lamps take
  //    the strongest adjacent strength (palette index = (s-1) mod N in the renderer).
  // lampHold_ is read here (concurrent reads are safe: nobody writes it during
  // a batch) but never written: hold updates go to the thread-local list and
  // are merged by the sim thread via hsApplyLampHolds (hs-design Ã‚Â§3.3).
  // PlayB (JD t4339u): 1-tick lamp delay, on and off. The lamp shows the state computed LAST tick
  // (lampDelay_, read-only here); this tick's result is queued as a thread-local update (merged on
  // the sim thread with the holds), so the batch stays lock-free. LL_LAMP_DELAY=0: no delay.
  static const bool kLampDelay = [] { const char* e = std::getenv("LL_LAMP_DELAY"); return !(e && e[0] == '0'); }();
  auto delayed = [&](int64_t k, CellState cur, CellState want) -> CellState {
    if (!kLampDelay) return want;
    CellState shown = cur;
    const auto p = lampDelay_.find(k);
    if (p != lampDelay_.end() && p->second.second < tickNo_) shown = p->second.first;
    if (want != shown) holds.push_back({k, tickNo_, false, true, want});
    else if (p != lampDelay_.end()) holds.push_back({k, 0, true, true, 0});
    return shown;
  };
  auto one = [&](int64_t k) {
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) return;
    int v = 0;
    const LampFast* fast = nullptr;
    if (!fullPass_) if (const auto it = lampFast_.find(k); it != lampFast_.end()) fast = &it->second;
    if (fast) {  // 2026-11-01 PlayL fix: fast wires, soft and hard combine by max; v is never reset
      for (const CellState* p : fast->wires) v = std::max(v, int(stValue(*p)));
      // N4: always sample soft/hard neighbours (maps may refill mid-tick; empty-gate skipped lamp updates)
      for (auto& d : k6) v = std::max({v, softPower(x + d[0], y + d[1], z + d[2]), hardPower(w, x + d[0], y + d[1], z + d[2])});
    } else {
    v = hardPower(w, x, y, z);
    for (auto& d : k6) {
      const int nx = x + d[0], ny = y + d[1], nz = z + d[2];
      if (isFlatWire(w.get(nx, ny, nz))) v = std::max(v, int(stValue(w.getState(nx, ny, nz))));
      else if (const int hp = hardPower(w, nx, ny, nz); hp > 0) v = std::max(v, hp);
      else v = std::max(v, softPower(nx, ny, nz));  // 2026-11-01: soft-powered block lights attached lamps
    }
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      v = std::max(v, emitToward(w, nx, y, nz, dirBack(d)));
    }
    }
    if (lampHoldTicks_ > 0) {  // afterglow: hold the last lit colour for lampHoldTicks_ ticks
      if (v > 0) { holds.push_back({k, tickNo_ + uint64_t(lampHoldTicks_), false}); }
      else if (auto h = lampHold_.find(k); h != lampHold_.end()) {
        if (h->second > tickNo_) { holds.push_back({k, h->second, false}); return; }  // still glowing (legacy also re-affirms nextHoldExpiry_)
        holds.push_back({k, 0, true});  // expired: fall through and go dark
      }
    }
    if (fast) {
      CellState ns = fast->color ? makeState(v, 0, 0, v > 0) : CellState(v > 0 ? 1 : 0);
      // Cortana 2026-10-15 1900: a directional plain lamp (the Emitor) keeps its facing bits.
      // Plain non-directional lamps only ever hold 0/1, so their states are unchanged.
      if (!fast->color && (*fast->self & 0x0180) && reg.has(w.get(x, y, z), kDirectional))
        ns = CellState(ns | (*fast->self & 0x0180));
      const CellState shown = delayed(k, *fast->self, ns);
      if (*fast->self != shown) {
        *fast->self = shown;
        // N5: cached writes must invalidate CoW state immediately, like setState().
        // A fresh pin (including another sub's apron) must not wait for the render
        // divider/view-cull to rebuild this lamp's mesh. noteStateEdit is pull-safe.
        w.noteStateEdit(x, y, z);
        dirty(x, y, z);
      }
      return;
    }
    const bool color = reg.def(w.get(x, y, z)).role == SimRole::ColorLamp;
    CellState ns = color ? makeState(v, 0, 0, v > 0) : CellState(v > 0 ? 1 : 0);
    if (!color && reg.has(w.get(x, y, z), kDirectional))  // Cortana 2026-10-15 1900: keep the Emitor's facing
      ns = CellState(ns | (w.getState(x, y, z) & 0x0180));
    const CellState cur = w.getState(x, y, z), shown = delayed(k, cur, ns);
    if (cur != shown) { w.setState(x, y, z, shown); dirty(x, y, z); }
  };
  for (int64_t k : keys) one(k);
}


// ===================== PlayM2: lithography spike (Trace-in-time compile) =====================
// JD 2026-11-06: "lithography step" - bake a SETTLED region into a tickable IR, run it at any clock,
// drop it on place/break (batch bake like MCHPRS Redpiler, not live-incremental). LithoKraft-native:
// nodes reuse the real cCompBehavior::tick + cScheduledOutput; Trace nets are precomputed into
// (source, wire distance) links using the live flood rule (-1 per wire, vias free). Sources are
// component emits, lit torches (frozen at compile) and hard-powered blocks (mounts). Same-tick
// semantics: each tick every node evaluates from its pre-tick snapshot, then non-Repeators
// re-evaluate until outputs stop changing (cap LL_SETTLE_MAX, default 16), like hsSettle.
// Repeators sample only on settle pass 0 (live hsSettle skips them) so a hard-power->copper->Trace
// edge raised mid-settle cannot shorten their labelled delay.
// Not modelled (spike): switches/buttons/pulsors/radio, torch inversion (torches frozen), Trace colour
// filtering on seeds, tick-rate divide, lamps (not nodes), state write-back to the world.
struct LithoIR {
  struct Src { uint8_t kind; uint8_t dir; uint8_t dist; int32_t a; };  // kind 0 node emit(a,dir) 1 mount a 2 const a 3 torch a
  struct Torch { int64_t k; int32_t mount = -1; std::vector<int32_t> wires; bool lit = false; };
  struct In { uint8_t kind = 0; int32_t a = -1; std::vector<int32_t> soft; };  // 0 none 1 wire sink 2 comp 3 const 4 mount(+soft sinks)
  struct Node {
    int64_t k; const cCompBehavior* beh; uint8_t emitMode, traitFlags, hotKind; int16_t delayFixed; uint32_t multiply;
    SimRole role;
    ComponentMemory mem; CellState st; cScheduledOutput sched;
    In in[4]; int32_t topIn = -1;
    uint8_t ch[4] = {};
    int32_t chainUp = -1;
    uint8_t muxSides = 0; int32_t muxNode[4] = {-1, -1, -1, -1};
    int32_t levelNode[4] = {-1, -1, -1, -1};
  };
  std::vector<Node> nodes;
  std::vector<std::vector<Src>> sinks;
  std::vector<std::vector<std::pair<int32_t, uint8_t>>> mountFeed;  // per mount: (node, dir) feeders
  std::vector<int32_t> mountTorch;                                   // torch below the block (-1 none): lit = 64
  std::vector<Torch> torches;
  uint64_t gen = 0, tick = 0, seed = 0, tick0 = 0;
};

bool cSignalSim::lithoValid() const { return litho_ && litho_->gen == editGen_; }
void cSignalSim::lithoReset() { litho_.reset(); }
void cSignalSim::lithoStates(std::vector<std::pair<int64_t, uint32_t>>& out) const {
  out.clear(); if (!litho_) return;
  for (const auto& n : litho_->nodes) out.push_back({n.k, uint32_t(n.st)});
}

void cSignalSim::lithoWriteBack(ChunkMap& w, const DirtyFn& dirty) {  // PlayM4 (JD t4701u)
  if (!litho_) return;
  for (const auto& n : litho_->nodes) {
    int x, y, z; unpackPos(n.k, x, y, z);
    if (w.getState(x, y, z) != n.st) { w.setState(x, y, z, n.st); if (dirty) dirty(x, y, z); }
  }
}
void cSignalSim::lithoCommit() {  // PlayM4: live resumes from the chip's state (nets rebuilt next tick)
  if (!litho_) return;
  for (const auto& n : litho_->nodes)
    if (auto it = comps_.find(n.k); it != comps_.end()) { it->second.mem = n.mem; it->second.sched = n.sched; }
  if (litho_->tick > tickNo_) tickNo_ = litho_->tick;
  fullPass_ = true; netsValid_ = false;
}

bool cSignalSim::lithoCompile(const ChunkMap& w, int x0, int y0, int z0, int x1, int y1, int z1, LithoStats* st) {
  const auto t0 = std::chrono::steady_clock::now();
  auto ir = std::make_shared<LithoIR>();
  const auto& reg = blockRegistry();
  auto inBox = [&](int x, int y, int z) { return x >= x0 && x <= x1 && y >= y0 && y <= y1 && z >= z0 && z <= z1; };
  const bool hardAny = cPulseRules::hardAnyComponent();
  static const bool kAutoSoftL = [] { const char* e = std::getenv("LL_TRACE_AUTO_SOFT"); return e && e[0] == '1'; }();
  std::vector<int64_t> keys;
  for (const auto& [k, c] : comps_) { int x, y, z; unpackPos(k, x, y, z); if (inBox(x, y, z)) keys.push_back(k); }
  std::sort(keys.begin(), keys.end());
  std::unordered_map<int64_t, int32_t> idx;
  for (int64_t k : keys) {
    const Comp& c = comps_.at(k);
    int x, y, z; unpackPos(k, x, y, z);
    LithoIR::Node n{};
    n.k = k; n.beh = c.behavior; n.emitMode = c.emitMode; n.traitFlags = c.traitFlags; n.hotKind = c.hotKind; n.delayFixed = c.delayFixed;
    n.multiply = c.rate.multiply ? c.rate.multiply : 1u;
    n.role = roleOf(reg.def(w.get(x, y, z))); n.mem = c.mem; n.st = w.getState(x, y, z); n.sched = c.sched;
    idx[k] = int32_t(ir->nodes.size()); ir->nodes.push_back(std::move(n));
  }
  std::unordered_map<int64_t, int32_t> torchIdx;
  auto torchFor = [&](int64_t tk) -> int32_t {
    if (auto it = torchIdx.find(tk); it != torchIdx.end()) return it->second;
    int tx, ty, tz; unpackPos(tk, tx, ty, tz);
    LithoIR::Torch t; t.k = tk; t.lit = stValue(w.getState(tx, ty, tz)) > 0;
    const int32_t id = int32_t(ir->torches.size()); ir->torches.push_back(std::move(t)); torchIdx[tk] = id; return id;
  };
  // Mounts: solid powerable blocks next to a hard-capable node, or above a torch.
  std::unordered_map<int64_t, int32_t> mountIdx;
  auto mountFor = [&](int64_t bk) -> int32_t {
    if (auto it = mountIdx.find(bk); it != mountIdx.end()) return it->second;
    const int32_t id = int32_t(ir->mountFeed.size()); ir->mountFeed.emplace_back(); ir->mountTorch.push_back(-1); mountIdx[bk] = id;
    int bx, by, bz; unpackPos(bk, bx, by, bz);
    if (reg.def(w.get(bx, by - 1, bz)).role == SimRole::Torch) ir->mountTorch[size_t(id)] = torchFor(packPos(bx, by - 1, bz));
    return id;
  };
  for (size_t i = 0; i < ir->nodes.size(); ++i) {
    const auto& n = ir->nodes[i];
    int x, y, z; unpackPos(n.k, x, y, z);
    const BlockId b = w.get(x, y, z);
    if (!(hardAny || n.role == SimRole::Repeator || n.role == SimRole::Pulsor || b == kRedirectorSelect)) continue;
    for (int d = 0; d < 4; ++d)
      if (reg.hasAll(w.get(x + kDirX[d], y, z + kDirZ[d]), kSolid | kOpaque | kPowerable))
        ir->mountFeed[size_t(mountFor(packPos(x + kDirX[d], y, z + kDirZ[d])))].push_back({int32_t(i), uint8_t(d)});
  }
  // Seeds per wire cell (mirrors seed() in hsPrepIncremental).
  std::unordered_map<int64_t, std::vector<LithoIR::Src>> seedAt;
  auto seedOk = [&](int x, int y, int z, int viaAx) {
    const BlockId tb = w.get(x, y, z);
    if (reg.def(tb).role != SimRole::Wire) return false;
    return !(isVia(tb) && !(viaAx > 0 && viaAxis(tb) == viaAx));
  };
  for (size_t i = 0; i < ir->nodes.size(); ++i) {
    int x, y, z; unpackPos(ir->nodes[i].k, x, y, z);
    for (int d = 0; d < 4; ++d) {
      const int tx = x + kDirX[d], tz = z + kDirZ[d];
      if (!cTraceConn::sideOn(w.getState(tx, y, tz), -kDirX[d], 0, -kDirZ[d])) continue;
      if (!seedOk(tx, y, tz, ir->nodes[i].role == SimRole::Repeator ? (kDirX[d] ? 1 : 2) : 0)) continue;
      seedAt[packPos(tx, y, tz)].push_back({0, uint8_t(d), 0, int32_t(i)});
    }
  }
  for (int y = y0; y <= y1; ++y) for (int z = z0; z <= z1; ++z) for (int x = x0; x <= x1; ++x) {
    const BlockId b = w.get(x, y, z);
    if (reg.def(b).role == SimRole::Torch) {
      const int32_t ti = torchFor(packPos(x, y, z));
      for (auto& d : k6) if (seedOk(x + d[0], y + d[1], z + d[2], 0)) seedAt[packPos(x + d[0], y + d[1], z + d[2])].push_back({3, 0, 0, ti});
    }
  }
  for (const auto& [bk, id] : mountIdx) {
    int bx, by, bz; unpackPos(bk, bx, by, bz);
    for (auto& d : k6) if (seedOk(bx + d[0], by + d[1], bz + d[2], 0)) seedAt[packPos(bx + d[0], by + d[1], bz + d[2])].push_back({1, 0, 0, id});
  }
  std::unordered_map<int64_t, int32_t> sinkIdx;
  size_t wiresSeen = 0, nLinks = 0, nComp = 0;
  std::vector<int64_t> links;
  auto sinkFor = [&](int64_t start) -> int32_t {
    if (auto it = sinkIdx.find(start); it != sinkIdx.end()) return it->second;
    std::unordered_map<int64_t, int> dist; std::deque<int64_t> dq; dist[start] = 0; dq.push_back(start);
    std::vector<LithoIR::Src> srcs;
    std::unordered_set<int64_t> done;
    while (!dq.empty()) {
      const int64_t cur = dq.front(); dq.pop_front();
      if (!done.insert(cur).second) continue;
      const int cd = dist[cur];
      if (cd >= kMaxSignal) continue;
      if (auto s = seedAt.find(cur); s != seedAt.end()) for (auto e : s->second) { e.dist = uint8_t(cd); srcs.push_back(e); }
      int x, y, z; unpackPos(cur, x, y, z);
      if (!inBox(x, y, z)) continue;
      links.clear(); wireLinks(w, x, y, z, links);
      const bool cv = isVia(w.get(x, y, z));
      for (int64_t nb : links) {
        int nx, ny, nz; unpackPos(nb, nx, ny, nz);
        const int nd = cd + ((cv || isVia(w.get(nx, ny, nz))) ? 0 : 1);
        auto it = dist.find(nb);
        if (it != dist.end() && it->second <= nd) continue;
        dist[nb] = nd;
        if (nd == cd) dq.push_front(nb); else dq.push_back(nb);
      }
    }
    wiresSeen += dist.size(); nLinks += srcs.size();
    const int32_t id = int32_t(ir->sinks.size()); ir->sinks.push_back(std::move(srcs)); sinkIdx[start] = id; return id;
  };
  for (auto& n : ir->nodes) {
    int x, y, z; unpackPos(n.k, x, y, z);
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      const BlockId nb = w.get(nx, y, nz);
      const BlockDef nd = reg.def(nb);
      const CellState ns = w.getState(nx, y, nz);
      if (const int sc = cBehaviorCatalog::get().colour(nd)) n.ch[d] = uint8_t(sc);
      LithoIR::In& in = n.in[d];
      if (nd.role == SimRole::Wire) {
        if (!isVia(nb) && cTraceConn::sideOn(ns, -kDirX[d], 0, -kDirZ[d])) { in.kind = 1; in.a = sinkFor(packPos(nx, y, nz)); }
      } else if (nd.role == SimRole::Torch) {
        in.kind = 5; in.a = torchFor(packPos(nx, y, nz));
      } else if (auto it = idx.find(packPos(nx, y, nz)); it != idx.end()) {
        if (nd.role != SimRole::Pulsor && kDirectComp) { in.kind = 2; in.a = it->second; ++nComp; }
      } else if (reg.hasAll(nb, kSolid | kOpaque | kPowerable)) {
        in.kind = 4; in.a = mountFor(packPos(nx, y, nz));
        for (int i = 0; i < 6; ++i) {
          const auto& e = k6[i];
          const int wx = nx + e[0], wy = y + e[1], wz = nz + e[2];
          if (!isFlatWire(w.get(wx, wy, wz))) continue;
          if (!cTraceConn::sideOn(w.getState(wx, wy, wz), -e[0], -e[1], -e[2])) continue;
          if (!kAutoSoftL && cTraceConn::isAuto(w.getState(wx, wy, wz))) continue;
          in.soft.push_back(sinkFor(packPos(wx, wy, wz)));
        }
      }
      if (auto it = idx.find(packPos(nx, y, nz)); it != idx.end()) {
        n.levelNode[d] = it->second;
        if ((n.role == SimRole::Redirector || n.role == SimRole::cFusor || n.role == SimRole::Repeator) &&
            (d == dirLeft(stFacing(n.st)) || d == dirRight(stFacing(n.st))) && nd.role == SimRole::Repeator &&
            stFacing(ns) == dirBack(d)) { n.muxSides |= uint8_t(1u << d); n.muxNode[d] = it->second; }
      }
    }
    if (isFlatWire(w.get(x, y + 1, z))) n.topIn = sinkFor(packPos(x, y + 1, z));
    if (n.role == SimRole::Repeator) {
      const int bd = dirBack(stFacing(n.st));
      if (auto it = idx.find(packPos(x + kDirX[bd], y, z + kDirZ[bd])); it != idx.end() &&
          ir->nodes[size_t(it->second)].role == SimRole::Repeator && stFacing(ir->nodes[size_t(it->second)].st) == stFacing(n.st))
        n.chainUp = it->second;
    }
  }
  for (size_t ti = 0; ti < ir->torches.size(); ++ti) {  // support: lit = !blockPowered(support) from last tick
    int tx, ty, tz; unpackPos(ir->torches[ti].k, tx, ty, tz);
    int sx, sy, sz; torchSupport(w.getState(tx, ty, tz), tx, ty, tz, sx, sy, sz);
    if (!reg.has(w.get(sx, sy, sz), kPowerable)) continue;
    std::vector<int32_t> ws;
    if (reg.hasAll(w.get(sx, sy, sz), kSolid | kOpaque | kPowerable)) { const int32_t mi = mountFor(packPos(sx, sy, sz)); ir->torches[ti].mount = mi; }
    if (isFlatWire(w.get(sx, sy + 1, sz))) ws.push_back(sinkFor(packPos(sx, sy + 1, sz)));
    for (auto& d : kH) if (isFlatWire(w.get(sx + d[0], sy, sz + d[1]))) ws.push_back(sinkFor(packPos(sx + d[0], sy, sz + d[1])));
    ir->torches[ti].wires = std::move(ws);
  }
  if (std::getenv("LL_LITHO_DBG")) {
    for (const auto& n : ir->nodes) { int qx, qy, qz; unpackPos(n.k, qx, qy, qz); if (n.role != SimRole::Repeator || qy != 65 || qx > 110 || qz > 110) continue;
      for (int d = 0; d < 4; ++d) { const auto& li = n.in[d]; std::fprintf(stdout, "LSRC rep(%d,%d,%d) d=%d kind=%d a=%d soft=%zu\n", qx, qy, qz, d, li.kind, li.a, li.soft.size());
        if (li.kind == 1) for (const auto& sr : ir->sinks[size_t(li.a)]) { int sx2 = 0, sy2 = 0, sz2 = 0; if (sr.kind == 0) unpackPos(ir->nodes[size_t(sr.a)].k, sx2, sy2, sz2);
          std::fprintf(stdout, "   src kind=%d a=%d dir=%d dist=%d node@(%d,%d,%d)\n", sr.kind, sr.a, sr.dir, sr.dist, sx2, sy2, sz2); }
        if (li.kind == 4) for (auto [fn, fd] : ir->mountFeed[size_t(li.a)]) { int sx2, sy2, sz2; unpackPos(ir->nodes[size_t(fn)].k, sx2, sy2, sz2); std::fprintf(stdout, "   feeder (%d,%d,%d) dir=%d const=%d\n", sx2, sy2, sz2, fd, ir->mountTorch[size_t(li.a)]); } } }
    for (const auto& [bk, id] : mountIdx) { int bx, by, bz; unpackPos(bk, bx, by, bz); if (by == 65 && bx < 110 && bz < 110) { std::fprintf(stdout, "LMOUNT (%d,%d,%d) id=%d feeders:", bx, by, bz, id); for (auto [fn, fd] : ir->mountFeed[size_t(id)]) { int sx2, sy2, sz2; unpackPos(ir->nodes[size_t(fn)].k, sx2, sy2, sz2); std::fprintf(stdout, " (%d,%d,%d)d%d", sx2, sy2, sz2, fd); } std::fprintf(stdout, "\n"); } }
  }
  ir->gen = editGen_; ir->tick = tickNo_; ir->tick0 = tickNo_; ir->seed = randor::worldSeed().load(std::memory_order_relaxed);
  litho_ = ir;
  if (st) {
    st->nodes = ir->nodes.size(); st->links = nLinks; st->wires = wiresSeen; st->compLinks = nComp;
    st->compileMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  }
  return true;
}

int cSignalSim::lithoRun(int ticks) {
  if (!litho_) return -1;
  if (litho_->gen != editGen_) { litho_.reset(); return -1; }  // place/break since compile: drop the chip
  LithoIR& ir = *litho_;
  static const int kMax = [] { const char* e = std::getenv("LL_SETTLE_MAX"); const int v = e ? std::atoi(e) : 16; return v < 1 ? 1 : v; }();
  const size_t n = ir.nodes.size();
  struct Snap { ComponentMemory mem; CellState st; cScheduledOutput sched; };
  thread_local std::vector<Snap> pre; thread_local std::vector<int> sinkVal, mountVal;
  thread_local std::vector<ComponentInputs> ins;
  pre.resize(n); ins.resize(n); sinkVal.resize(ir.sinks.size()); mountVal.resize(ir.mountFeed.size());
  auto emitOf = [&](const LithoIR::Node& s, int dir) { return compEmit(s.beh, s.mem, s.emitMode, dir, s.st); };
  for (int t = 0; t < ticks; ++t) {
    ++ir.tick;
    for (size_t i = 0; i < n; ++i) pre[i] = {ir.nodes[i].mem, ir.nodes[i].st, ir.nodes[i].sched};
    auto flood = [&] {
      for (size_t mi = 0; mi < ir.mountFeed.size(); ++mi) {
        int v = (ir.mountTorch[mi] >= 0 && ir.torches[size_t(ir.mountTorch[mi])].lit) ? int(kMaxSignal) : 0;
        for (auto [nd, dd] : ir.mountFeed[mi]) v = std::max(v, std::min(emitOf(ir.nodes[size_t(nd)], dd), int(kMaxSignal)));
        mountVal[mi] = v;
      }
      for (size_t si = 0; si < ir.sinks.size(); ++si) {
        int v = 0;
        for (const auto& s : ir.sinks[si]) {
          const int src = s.kind == 0 ? std::min(emitOf(ir.nodes[size_t(s.a)], s.dir), int(kMaxSignal)) : s.kind == 1 ? mountVal[size_t(s.a)]
                        : s.kind == 3 ? (ir.torches[size_t(s.a)].lit ? int(kMaxSignal) : 0) : s.a;
          if (src - int(s.dist) > v) v = src - int(s.dist);
        }
        sinkVal[si] = v;
      }
    };
    // torches decide from the previous tick's settled state (tickTorches), then apply together
    flood();
    thread_local std::vector<uint8_t> tl; tl.resize(ir.torches.size());
    for (size_t ti = 0; ti < ir.torches.size(); ++ti) {
      const auto& tc = ir.torches[ti];
      bool pw = tc.mount >= 0 && mountVal[size_t(tc.mount)] > 0;
      for (int32_t sk : tc.wires) pw = pw || sinkVal[size_t(sk)] > 0;
      tl[ti] = (tc.mount >= 0 || !tc.wires.empty()) ? uint8_t(!pw) : uint8_t(tc.lit);
    }
    for (size_t ti = 0; ti < ir.torches.size(); ++ti) ir.torches[ti].lit = tl[ti] != 0;
    for (int it = 0; it < kMax; ++it) {
      flood();
      for (size_t i = 0; i < n; ++i) {
        const auto& nd = ir.nodes[i];
        ComponentInputs& in = ins[i]; in = ComponentInputs{};
        for (int d = 0; d < 4; ++d) {
          in.ch[d] = nd.ch[d];
          const LithoIR::In& li = nd.in[d];
          switch (li.kind) {
            case 1: in.side[d] = sinkVal[size_t(li.a)]; break;
            case 2: { const auto& s = ir.nodes[size_t(li.a)]; in.side[d] = compEmit(s.beh, s.mem, s.emitMode, dirBack(d), s.st); break; }
            case 3: in.side[d] = li.a; break;
            case 5: in.side[d] = ir.torches[size_t(li.a)].lit ? int(kMaxSignal) : 0; break;
            case 4: {
              int v = mountVal[size_t(li.a)];
              if (v <= 0) for (int32_t sk : li.soft) v = std::max(v, sinkVal[size_t(sk)]);
              in.side[d] = std::min(v, int(kMaxSignal)); break;
            }
            default: break;
          }
          if (nd.role == SimRole::Thresholdor && (stAux(pre[i].st) & 2) && nd.levelNode[d] >= 0) {
            const auto& s = pre[size_t(nd.levelNode[d])];
            in.level[d] = ir.nodes[size_t(nd.levelNode[d])].beh->readLevel(s.st, s.mem);
          }
          if (nd.muxSides & (1u << d)) { in.muxSides |= uint8_t(1u << d); in.muxOn = in.muxOn || stOn(pre[size_t(nd.muxNode[d])].st); }
        }
        if (nd.topIn >= 0) in.top = sinkVal[size_t(nd.topIn)];
        if (nd.chainUp >= 0) in.chainCh = uint8_t(repeator::colourOf(pre[size_t(nd.chainUp)].st));
        if (nd.role == SimRole::Randor) in.rng = randor::rollSeed(ir.seed, nd.k, ir.tick);
        static const bool kDbg = std::getenv("LL_LITHO_DBG") != nullptr;
        if (kDbg && ir.tick - ir.tick0 < 30 && nd.role == SimRole::Repeator) {
          int qx, qy, qz; unpackPos(nd.k, qx, qy, qz);
          if (qy == 65 && qx < 110 && qz < 110) std::fprintf(stdout, "LDBG t=%d it=%d tl=%d,%d rep(%d,%d,%d) st=%08x face=%d side=%d,%d,%d,%d kinds=%d,%d,%d,%d top=%d sw=%d mux=%d/%d\n", int(ir.tick - ir.tick0), it, int(ir.torches.size() > 0 && ir.torches[0].lit), int(ir.torches.size() > 1 && ir.torches[1].lit), qx, qy, qz, unsigned(pre[i].st), stFacing(pre[i].st),
            in.side[0], in.side[1], in.side[2], in.side[3], nd.in[0].kind, nd.in[1].kind, nd.in[2].kind, nd.in[3].kind, in.top, in.switchIn, in.muxSides, int(in.muxOn));
        }
      }
      bool changed = false;
      for (size_t i = 0; i < n; ++i) {
        auto& nd = ir.nodes[i];
        // Mirror hsSettle: a Repeator is a pure delay line — pass-0 sample only. Re-reading after
        // a same-tick hard-power->copper->Trace flood would make every Repeator one tick shorter
        // per hop (tick-25 clock drift on ALU Stress).
        if (it > 0 && nd.role == SimRole::Repeator) continue;
        const uint8_t o0 = nd.mem.out, o20 = nd.mem.out2; const bool p0 = nd.mem.pulse;
        nd.mem = pre[i].mem; nd.sched = pre[i].sched;
        ComponentStep step{}; CellState cur = pre[i].st;
        for (uint32_t s = 0; s < nd.multiply; ++s) {  // same dispatch as runRange (hot paths are the live semantics)
          if (hotSpecOn() && nd.hotKind == 1) step = hotTickRepeator(ins[i], cur, nd.mem);
          else if (hotSpecOn() && nd.hotKind == 2) step = hotTickThresholdor(ins[i], cur);
          else step = nd.beh->tick(ins[i], cur, nd.mem);
          cur = step.state; }
        const bool queued = (nd.traitFlags & cCompTraits::kQueuedOutput) != 0;
        const int dl = !queued ? 0 : (nd.delayFixed >= 0) ? int(nd.delayFixed) : nd.beh->delayTicks(step.state, nd.k, ir.tick);
        const cScheduledOutput::Out raw{uint8_t(std::clamp(step.out, 0, kMaxSignal)), uint8_t(std::clamp(step.out2, 0, kMaxSignal)), step.pulse};
        const cScheduledOutput::Out pub = nd.sched.step(ir.tick, dl, raw);
        nd.mem.out = pub.out; nd.mem.out2 = pub.out2; nd.mem.pulse = pub.pulse; nd.st = step.state;
        if (nd.mem.out != o0 || nd.mem.out2 != o20 || nd.mem.pulse != p0) changed = true;
      }
      if (!changed) break;
    }
  }
  return ticks;
}

}  // namespace lithokraft
