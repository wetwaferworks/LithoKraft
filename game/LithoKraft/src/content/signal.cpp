#include <chrono>
#include <vector>
#include "lithokraft/content/randor_rng.h"  // 2026-10-31 (JD): Randor
#include "lithokraft/content/signal.h"
#include "lithokraft/content/compiled_comp_table.h"  // 2026-11-01 Option A
#include <algorithm>
#include <climits>
#include "litholattice/core/log.h"
#include "litholattice/storage/storage.h"
#include "litholattice/sim/det_sched.h"
#include "lithokraft/content/component.h"
#include "lithokraft/content/pulse_rules.h"  // 2026-11-01 PlayK
#include "lithokraft/content/repeator.h"  // 2026-10-31: chain colour
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
  torches_.clear(); wires_.clear(); lamps_.clear(); comps_.clear(); ++compsGen_;
  const cBlockReg& reg = blockRegistry();
  for (const auto& [k, col] : w.columns()) scanOneColumn(*this, *col, reg);
}

void cSignalSim::scanColumn(const ChunkMap& w, int cx, int cz) {
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
  eraseColumnKeys(awake_, cx, cz);
  for (auto it = comps_.begin(); it != comps_.end();) {
    if (keyInColumn(it->first, cx, cz)) { it = comps_.erase(it); ++compsGen_; }
    else ++it;
  }
  std::erase_if(mountPowered_, [&](const auto& e) { return keyInColumn(e.first, cx, cz); });  // rebuilt per tick anyway; drop now
  std::erase_if(softPowered_, [&](const auto& e) { return keyInColumn(e.first, cx, cz); });   // PlayL: no soft level for gone cells
  // Per-tick scratch naming this column must not survive into the next
  // tick (the sim is stopped for eviction, so clearing is safe): pending
  // lamp recomputes and changed-source marks for cells that are gone.
  eraseColumnKeys(dirtyLamps_, cx, cz);
  std::erase_if(extraLamps_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  std::erase_if(changedSrc_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  std::erase_if(softChanged_, [&](int64_t k) { return keyInColumn(k, cx, cz); });
  if (!relevant) return;
  if (!relinkNets_ || (ownCells && !edgeDrop)) topoDirty_ = true;  // LL_SIGNAL_RELINK: a border-only evict keeps the flood
  srcReach_.clear();  // cached reach sets reference wire nets; rebuilt with the nets
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
  const int64_t k = packPos(x, y, z);
  if (netsValid_) { diagKind_ = 6; diagCx_ = x >> 4; diagCz_ = z >> 4; diagOwn_ = true; }  // diag: edit (scan overrides to 5)
  fullPass_ = true; netsValid_ = false; topoDirty_ = true; editLocal_ = false;
  torches_.erase(k); wires_.erase(k); lamps_.erase(k); comps_.erase(k); lampHold_.erase(k); ++compsGen_;
  const SimRole r = roleOf(now);
  if (r == SimRole::Torch) torches_.insert(k);
  else if (r == SimRole::Wire) wires_.insert(k);
  else if (r == SimRole::Lamp || r == SimRole::ColorLamp) lamps_.insert(k);
  else if (const cCompBehavior* b = cBehaviorCatalog::get().component(now)) comps_[k] = Comp{b, {}};
}

// 2026-11-01 (JD placement spikes): a component / lamp / plain block edit with no wire in or under the cell leaves
// every wire link unchanged (wireLinks reads kCutsWire of a cell only for the wire directly below it), so the
// net flood is kept (relink) and only the readers of the touching nets are patched. LL_EDIT_LOCAL=0 kill.
void cSignalSim::onBlockChanged(int x, int y, int z, BlockId now, bool wireBelow) {
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
  lamps_.erase(k); comps_.erase(k); lampHold_.erase(k); torches_.erase(k); ++compsGen_;
  if (r == SimRole::Torch) torches_.insert(k);
  const bool isLamp = r == SimRole::Lamp || r == SimRole::ColorLamp;
  bool isComp = false;
  if (isLamp) lamps_.insert(k);
  else if (const cCompBehavior* b = cBehaviorCatalog::get().component(now)) { comps_[k] = Comp{b, {}}; isComp = true; }
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
  // B-L1 acceptance (P3-2): unordered comps_ iteration — the result is a
  // membership SET (mountPowered_); iteration order cannot leak anywhere.
  mountPowered_.clear();
  for (const auto& [k, c] : comps_) {
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
  for (const auto& [k, c] : comps_) {
    if (!c.mem.out || c.mem.pulse) continue;
    int x, y, z; unpackPos(k, x, y, z);
    // 2026-11-01 PlayL (JD, LL_HARD_ANY): any component output facing the block hard-powers it, not only a Repeator.
    if (!cPulseRules::hardAnyComponent() && blockRegistry().def(w.get(x, y, z)).role != uint16_t(SimRole::Repeator)) continue;
    const CellState s = w.getState(x, y, z);
    for (int d = 0; d < 4; ++d) {
      const int es = c.behavior->emitToward(d, s, c.mem);
      if (es <= 0) continue;
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      if (!blockRegistry().hasAll(w.get(nx, y, nz), kSolid | kOpaque | kPowerable)) continue;
      uint8_t& hv = mountPowered_[packPos(nx, y, nz)];  // strongest feeder wins (order-independent)
      hv = uint8_t(std::max(int(hv), std::min(es, int(kMaxSignal))));
    }
  }
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
  return it == comps_.end() ? 0 : it->second.behavior->emitToward(dir, w.getState(x, y, z), it->second.mem);
}

int cSignalSim::inputFrom(const ChunkMap& w, int x, int y, int z, int d) const {
  const int nx = x + kDirX[d], nz = z + kDirZ[d];
  const BlockDef nd = blockRegistry().def(w.get(nx, y, nz));
  const CellState s = w.getState(nx, y, nz);
  // SW-H2: a disk-loaded wire cell state carries a 7-bit value (65..127 are
  // representable); clamp at this sampling point so no component input — and
  // therefore no pass-through output — can ever exceed kMaxSignal.
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
      fullPass_ = true; netsValid_ = false; topoDirty_ = true; editLocal_ = false; softAll_ = true;
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
  // Reassembled from the hs* pieces (hs-design §3.2); the handshake path in
  // cSignalWorldSim drives the identical pieces through the authority.
  hsBeginCycle(w, pcx, pcz, radius);
  hsFlipAndTorches(w, dirty);
  hsComponents(w, dirty);
  hsMark(w);
  if (!hsSkip_) {
    if (fullPass_) hsPropagateFull(w, dirty);
    else {
      hsPrepIncremental(w);
      for (int32_t id : hsMarkedNets_) hsPropagateNet(w, id, dirty);
      hsApplyStrengthPending(&dirty);
    }
    hsRefreshSoft(w);  // 2026-11-01 PlayL: this tick's wire values, before lamps / next components read soft
    hsSettle(w, dirty);  // 2026-11-01 PlayM: components read this tick's wires (same tick)
    hsLampsBatch(hsLampKeys_, w, dirty);
    hsApplyLampHolds(hsTlsLampHolds());
    hsTlsLampHolds().clear();
    hsFinishCycle(w);
  }
  hsFinalizeComponents();
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
  if (epOn()) { g_ep = EditProf{}; g_ep.t0 = int64_t(epMs() * 1000.0); g_ep.fp = fullPass_; }
  ++tickNo_;
  // NOTE: radio_/jammers_ flips are NOT here any more — they are hsCommit()
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
    if (lazyNets_ && netsValid_ && !genChanged && netTouchUnchanged(w)) { netColumns_ = w.columns().size(); hsReason_ |= 32u; }  // LL_SIGNAL_LAZYNETS
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
  const bool wakeAll = allAwake_ || fullPass_ || !compSleep_;
  allAwake_ = wakeAll;  // read by tickComponents
  hsSkip_ = false;
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

void cSignalSim::hsFlipAndTorches(ChunkMap& w, const DirtyFn& dirty) {
  connProbe(w, dirty);  // PlayM test probe (LL_CONN_PROBE, off by default)
  collectMountPowered(w);
  tickTorches(w, dirty);
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
  awake_.clear();
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
        if (size_t(id) < netComps_.size()) awake_.insert(netComps_[size_t(id)].begin(), netComps_[size_t(id)].end());
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
    clearNetMarks(); dirtyLamps_.clear();
    for (int64_t k : changedSrc_) {
      auto [rit, fresh] = srcReach_.try_emplace(k);
      SrcReach& r = rit->second;
      if (fresh) {  // the 5 x 5 x 5 box once per source, cached until the networks change
        int x, y, z; unpackPos(k, x, y, z);
        for (int dy = -2; dy <= 2; ++dy)
          for (int dz = -2; dz <= 2; ++dz)
            for (int dx = -2; dx <= 2; ++dx) {
              const int64_t n = packPos(x + dx, y + dy, z + dz);
              if (const auto it = wireNet_.find(n); it != wireNet_.end()) {
                if (std::find(r.nets.begin(), r.nets.end(), it->second) == r.nets.end()) r.nets.push_back(it->second);
              } else if (lamps_.contains(n)) r.lamps.push_back(n);
            }
      }
      for (int32_t id : r.nets) markNet(id);
      dirtyLamps_.insert(r.lamps.begin(), r.lamps.end());
    }
    forEachMarkedNet([&](int32_t id) { dirtyLamps_.insert(netLamps_[size_t(id)].begin(), netLamps_[size_t(id)].end()); });
    dirtyLamps_.insert(extraLamps_.begin(), extraLamps_.end());
  }
  // Canonical work lists for this cycle (hs-design §3.2/§9.6): marked nets
  // ascending by id; lamp keys sorted ascending (legacy iterated the
  // unordered sets — per-lamp writes are disjoint and hold merges are
  // commutative, so sorting changes no outcome, only the visit order).
  hsMarkedNets_.clear();
  if (!fullPass_) forEachMarkedNet([&](int32_t id) { hsMarkedNets_.push_back(id); });
  hsLampKeys_.clear();
  if (fullPass_) hsLampKeys_.assign(lamps_.begin(), lamps_.end());
  else hsLampKeys_.assign(dirtyLamps_.begin(), dirtyLamps_.end());
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
  if (epOn() && g_ep.t0) {
    static const double kSpike = [] { const char* e = std::getenv("LL_SPIKE_MS"); return e ? std::atof(e) : 10.0; }();
    const double tot = epMs() - double(g_ep.t0) / 1000.0;
    if (tot >= kSpike)
      ::litholattice::logInfo("signal cycle: %.2f ms fullPass=%d | buildNets %.2f propagateFull %.2f components %.2f lamps %.2f | comps %zu wires %zu lamps %zu",
                              tot, int(g_ep.fp), g_ep.build, g_ep.full, g_ep.comps, g_ep.lamps, comps_.size(), wires_.size(), lamps_.size());
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
  int x, y, z; unpackPos(k, x, y, z);
  for (int dy = -1; dy <= 2; ++dy)  // +2: a torch powers the block above it, read beside that block
    for (int dz = -1; dz <= 1; ++dz)
      for (int dx = -1; dx <= 1; ++dx) {
        const int64_t n = packPos(x + dx, y + dy, z + dz);
        if (comps_.contains(n)) awake_.insert(n);
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
  srcReach_.clear();
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
      if (nd.role == SimRole::Wire) { c.kind[d] = isVia(nb) ? 3 : 1; c.ch[d] = nd.channel; }
      else if (nd.role == SimRole::Torch) { c.kind[d] = 2; c.ch[d] = nd.channel; }
      else if (comps_.contains(packPos(nx, y, nz)) || reg.hasAll(nb, kSolid | kOpaque | kPowerable) ||
               (kDirectComp && (nd.role == SimRole::Switch || nd.role == SimRole::cButton))) c.kind[d] = 4;
      else c.kind[d] = 0;
      if ((c.kind[d] == 1 || c.kind[d] == 2) && !c.nb[d]) ok = false;
    }
    c.top = isFlatWire(w.get(x, y + 1, z)) ? statePtr(w, x, y + 1, z) : nullptr;
    c.fast = ok;
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
    a.old.resize(a.keys.size()); a.stamp.assign(a.keys.size(), 0);
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
// all firing on one tick — load spreading, the anti-hitch half of
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
  // validated result cached (key lookup only — the cache is never
  // iterated, so no unordered order can reach execution).
  if (const auto it = declaredRates_.find(block); it != declaredRates_.end()) return it->second;
  const litholattice::TickRate declared = behavior ? behavior->tickRate() : litholattice::TickRate{};
  litholattice::TickRate resolved = declared;
  if (!declared.valid()) {
    // Logged exactly once per class: after this, the cached neutral
    // result is what every later resolution returns.
    litholattice::logError(
        "signal: invalid class tick rate for block %u (multiply %u, divide %u) — substituting the neutral rate; "
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
  }
  if (role == SimRole::Catchor && cPulseRules::repeatorLinks()) {  // 2026-11-01 (JD, PlayK): Repeator -> Catchor direct
    for (int d = 0; d < 4; ++d) {
      const int nx = x + kDirX[d], nz = z + kDirZ[d];
      if (roleOf(blockRegistry().def(w.get(nx, y, nz))) == SimRole::Repeator && stFacing(w.getState(nx, y, nz)) == dirBack(d))
        in.repSides = uint8_t(in.repSides | (1u << d));
    }
  }
  if (role == SimRole::Randor) in.rng = randor::rollSeed(randor::worldSeed().load(std::memory_order_relaxed), k, tickNo_);  // 2026-10-31 (JD)
  if (role == SimRole::Redirector || role == SimRole::cFusor) {  // 2026-10-31 (JD): Repeator on left / right = mux select / fuse reset
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
    if (nd.role == SimRole::Wire || nd.role == SimRole::Torch) in.ch[d] = nd.channel;
  }
  if (isFlatWire(w.get(x, y + 1, z))) in.top = std::min(stValue(w.getState(x, y + 1, z)), kMaxSignal);  // SW-H2 clamp
  return false;
}

void cSignalSim::tickComponents(ChunkMap& w, const DirtyFn& dirty) {
  EpTimer ept(g_ep.comps);
  // 2) Components: new outputs from the previous tick's inputs (computed first, applied together).
  struct Next { int64_t k; Comp* c; ComponentStep step; };
  std::vector<Next> compNext;
  compNext.reserve(comps_.size());
  const bool all = allAwake_, fastInputs = netsValid_ && !fullPass_;
  lastTicked_ = 0;
  const bool sameTick = sameTick_;
  compSnap_.clear(); compRecs_.clear(); compRecIdx_.clear();
  // B-L1 (P3-2): iterate a SORTED key snapshot, not the unordered_map.
  // comps_ iteration order is hash order — it leaks into externally
  // visible ordering: compNext's apply order below is the order sounds_
  // entries are appended and radio_ broadcasts are published. Sorted
  // (packPos) order makes both deterministic and identical across runs
  // and paths. Component INPUTS are all read before any apply, so the
  // iteration order cannot change any component's result — only the
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
    if (kOptionA) { if (!compTable_) compTable_ = std::make_shared<cCompiledCompTable>(); compTable_->rebuild(compOrder_); }
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
      wakeScratch_.insert(wakeScratch_.end(), awake_.begin(), awake_.end());
      std::sort(wakeScratch_.begin(), wakeScratch_.end());
      wakeScratch_.erase(std::unique(wakeScratch_.begin(), wakeScratch_.end()), wakeScratch_.end());
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
  for (int64_t k : iterKeys) {
    const auto cit = comps_.find(k);
    if (cit == comps_.end()) continue;  // LL_WAKE: a queued key whose component is gone
    Comp& c = cit->second;
    int x, y, z; unpackPos(k, x, y, z);
    if (!kTable && !all && !c.rateHeld && c.behavior->quiescent(stateOf(w, k), c.mem) && !awake_.contains(k)) continue;
    if (!area_.contains(x, z)) { if (kTable) ++compOutOfRange_; continue; }
    if (kTable && !all && !c.rateHeld && c.behavior->quiescent(stateOf(w, k), c.mem) && !awake_.contains(k)) continue;
    // Rate scaling (JD 2026-10-14): the class's effective tick rate.
    // divide > 1: this cell is pulled only on its phase tick of the
    // period; on other ticks it HOLDS — its published outputs (mem.out)
    // are read by the propagate phase exactly as before, so readers
    // always sample published state, never a half-advanced one.
    // multiply > 1 is applied at the behaviour calls below. Every
    // shipped class is neutral, so this gate never skips shipped content.
    const litholattice::TickRate rate = classTickRate(w.get(x, y, z), c.behavior);
    if (rate.divide > 1 && (tickNo_ % rate.divide) != uint64_t(ratePhaseFor(k, rate.divide))) {
      c.rateHeld = true;  // component sleep must not swallow the due tick
      if (wakeOn) {  // LL_WAKE timer: queue for its next phase tick
        const uint64_t ph = uint64_t(ratePhaseFor(k, rate.divide)), cur = tickNo_ % rate.divide;
        wakeSched(k, (ph + rate.divide - cur) % rate.divide);
      }
      continue;
    }
    c.rateHeld = false;
    ++lastTicked_;
    ComponentInputs in;
    const bool fastUsed = gatherInputs(w, k, c, x, y, z, fastInputs, true, in);
    const CellState pre = fastUsed ? *c.self : w.getState(x, y, z);
    if (sameTick) compSnap_.insert_or_assign(k, CompSnap{c.mem, pre});  // PlayM: pre-tick snapshot for the settle
    ComponentStep step{};
    // A-H3: chain CellState across sub-pulls (see gatherInputs history note).
    CellState cur = pre;
    for (uint32_t s = 0; s < rate.multiply; ++s) { step = c.behavior->tick(in, cur, c.mem); cur = step.state; }
    compNext.push_back({k, &c, step});
  }
  for (const auto& [k, cp, step] : compNext) {
    ComponentMemory& m = cp->mem;
    // SW-H2 defense-in-depth at the store point: clamp TO kMaxSignal (see history).
    const int out = std::clamp(step.out, 0, kMaxSignal), out2 = std::clamp(step.out2, 0, kMaxSignal);
    const uint8_t o0 = m.out, o20 = m.out2; const bool p0 = m.pulse;
    const bool outChanged = m.out != out || m.out2 != out2 || m.pulse != step.pulse;
    if (outChanged || step.pulse) changedSrc_.push_back(k);  // 2026-11-01 PlayK: a held Pulsor pulse re-floods its run every tick (matches a full pass)
    m.out = uint8_t(out); m.out2 = uint8_t(out2); m.pulse = step.pulse;
    int x, y, z; unpackPos(k, x, y, z);
    if (w.getState(x, y, z) != step.state) { w.setState(x, y, z, step.state); dirty(x, y, z); }
    compRecIdx_.emplace(k, compRecs_.size());
    compRecs_.push_back(CompRec{k, cp, step, o0, o20, p0, blockRegistry().def(w.get(x, y, z)).role == SimRole::cSignalTransmittor});  // PlayM: sounds / radio / wake / Option A deferred to hsFinalizeComponents
  }
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
  const bool wakeOn = wakeList_ && compTableOn(), kOptA = optionAOn();
  std::sort(compRecs_.begin(), compRecs_.end(), [](const CompRec& a, const CompRec& b) { return a.k < b.k; });
  for (const CompRec& r : compRecs_) {
    const ComponentMemory& m = r.c->mem;
    int x, y, z; unpackPos(r.k, x, y, z);
    const bool outChanged = m.out != r.out0 || m.out2 != r.out20 || m.pulse != r.pulse0;
    if (wakeOn) {
      WpTimer t;
      if (!r.c->behavior->quiescent(r.step.state, m)) wakeSchedule(r.k, 1);
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
}

bool cSignalSim::envSameTick() { return sameTickOn(); }

void cSignalSim::hsSettle(ChunkMap& w, const DirtyFn& dirty) {
  // 2026-11-01 PlayM (JD): components read THIS tick's wire values. Every component already ran once this
  // tick (tickComponents) against the previous flood. Here, the ones whose inputs may have changed since
  // re-evaluate FROM THEIR PRE-TICK SNAPSHOT with fresh inputs (still one logical tick each: delay lines,
  // timers, Pulsor holds advance exactly once), in ascending key order with every input read before any
  // apply; changed outputs re-flood (hard + soft too) and the loop repeats until nothing changes.
  // Delays are kept because a delaying component's output comes from its memory, not this tick's input.
  // A combinational loop with no delay stops at LL_SETTLE_MAX (deterministic, counted in settleCapHits).
  if (!sameTick_ || hsSkip_) return;
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
      const litholattice::TickRate rate = classTickRate(w.get(x, y, z), c.behavior);
      if (rate.divide > 1 && (tickNo_ % rate.divide) != uint64_t(ratePhaseFor(k, rate.divide))) continue;  // not its phase tick: holds
      auto sit = compSnap_.find(k);
      if (sit == compSnap_.end()) sit = compSnap_.emplace(k, CompSnap{c.mem, w.getState(x, y, z)}).first;  // slept in pass 0: state is pre-tick
      ComponentInputs in;
      gatherInputs(w, k, c, x, y, z, fastInputs, false, in);
      ComponentMemory mem = sit->second.mem;
      CellState cur = sit->second.state;
      ComponentStep step{};
      for (uint32_t s = 0; s < rate.multiply; ++s) { step = c.behavior->tick(in, cur, mem); cur = step.state; }
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
      std::unordered_set<int64_t> awakeBefore;
      if (!allAwake_) awakeBefore = awake_;
      changedSrc_ = newSrc;
      hsMark(w);  // marks the reached nets / lamps, diffs hard power (appends mount changes to changedSrc_)
      if (!allAwake_) awake_.insert(awakeBefore.begin(), awakeBefore.end());
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
    // SW-H2: guard the high end too — buckets_ has kMaxSignal + 1 entries, so
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
    if (strongPowered(w, x, y + 1, z))  // torch-powered block: colourless power to all 6 sides
      for (auto& d : k6) seed(x + d[0], y + 1 + d[1], z + d[2], kMaxSignal, 0);
  }
  std::vector<std::pair<int64_t, int>> pulses;
  // B-L1 acceptance (P3-2): this comps_ iteration stays unordered BY
  // DESIGN — it only SEEDS buckets/pulses. Bucket BFS resolves by max
  // strength and pulses flood a fixed strength, so the final wire states
  // are independent of seed order; nothing externally ordered (sounds,
  // radio) is emitted here. (tickComponents, which does emit those, uses
  // a sorted snapshot — see there.)
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
      const int s = c.behavior->emitToward(d, st, c.mem);
      if (s <= 0) continue;
      if (c.mem.pulse) pulses.push_back({packPos(x + kDirX[d], y, z + kDirZ[d]), s});
      else if (cTraceConn::sideOn(w.getState(x + kDirX[d], y, z + kDirZ[d]), -kDirX[d], 0, -kDirZ[d]))  // PlayL: connected side only
        seed(x + kDirX[d], y, z + kDirZ[d], s, uint8_t(c.behavior->emitChannel(d, st, c.mem)),
           reg.def(w.get(x, y, z)).role == SimRole::Repeator ? (kDirX[d] ? 1 : 2) : 0);  // Repeator feeds a side via end-on
      // Hard-powered block in front of a Repeator: colourless power to its other five faces.
      if (!c.mem.pulse && mountPowered_.contains(packPos(x + kDirX[d], y, z + kDirZ[d])) && (cPulseRules::hardAnyComponent() || reg.def(w.get(x, y, z)).role == SimRole::Repeator))
        for (auto& e : k6) if (!(e[1] == 0 && e[0] == -kDirX[d] && e[2] == -kDirZ[d])) seed(x + kDirX[d] + e[0], y + e[1], z + kDirZ[d] + e[2], s, 0);  // not back into the source face
    }
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
// (TraceStrengthBuffer::set is NOT a plain slot store — it mutates a shared
// dirty-word bitset, and slotFor() mutates the slot map — see
// hs-integration-notes). hsPropagateNet then applies a net's recorded seeds
// to net-local buckets, so the per-net result is the legacy result (cross-net
// interleaving in the legacy global bucket loop cannot change a net's final
// state: nets are disjoint, and a net's entries keep their relative order).

void cSignalSim::hsPrepIncremental(ChunkMap& w) {
  HsTimer timer{hsWiresNs_};
  const auto& reg = blockRegistry();
  if (hsNetWork_.size() != netArr_.size()) hsNetWork_.assign(netArr_.size(), NetWork{});
  for (int32_t id : hsMarkedNets_) {
    NetWork& nw = hsNetWork_[size_t(id)];
    nw.seeds.clear(); nw.pulses.clear(); nw.strength.clear();
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
    if (fresh) {
      int x, y, z; unpackPos(k, x, y, z);
      for (int dy = -2; dy <= 2; ++dy)
        for (int dz = -2; dz <= 2; ++dz)
          for (int dx = -2; dx <= 2; ++dx) {
            const int64_t n = packPos(x + dx, y + dy, z + dz);
            if (const auto it = wireNet_.find(n); it != wireNet_.end()) {
              if (std::find(r.nets.begin(), r.nets.end(), it->second) == r.nets.end()) r.nets.push_back(it->second);
            } else if (lamps_.contains(n)) r.lamps.push_back(n);
          }
    }
    for (int32_t id : r.nets) if (isNetMarked(id)) return true;
    return false;
  };
  for (auto& [k, lit] : torchNext_) {
    if (!lit) continue;
    if (!touchesMarked(k)) continue;
    int x, y, z; unpackPos(k, x, y, z);
    const uint8_t ch = reg.def(w.get(x, y, z)).channel;  // red torch -> red trace only
    for (auto& d : kH) seed(x + d[0], y, z + d[1], kMaxSignal, ch);
    if (strongPowered(w, x, y + 1, z))  // torch-powered block: colourless power to all 6 sides
      for (auto& d : k6) seed(x + d[0], y + 1 + d[1], z + d[2], kMaxSignal, 0);
  }
  // B-L1 acceptance (P3-2): unordered comps_ iteration is safe here for
  // the same reason as in hsPropagateFull — this loop only RECORDS seeds
  // and pulses into per-net work lists; each net's flood resolves by max
  // strength independent of record order across nets, and nets are
  // disjoint. No externally visible ordering originates here.
  for (const auto& [k, c] : comps_) {  // components drive either colour; pulses are flooded per net
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
      const int s = c.behavior->emitToward(d, st, c.mem);
      if (s <= 0) continue;
      if (c.mem.pulse) {
        const int64_t target = packPos(x + kDirX[d], y, z + kDirZ[d]);
        const auto it = wireLoc_.find(target);
        if (it != wireLoc_.end() && isNetMarked(it->second.first) &&
            !netArr_[size_t(it->second.first)].via[size_t(it->second.second)])
          hsNetWork_[size_t(it->second.first)].pulses.push_back({it->second.second, s});
      } else if (cTraceConn::sideOn(w.getState(x + kDirX[d], y, z + kDirZ[d]), -kDirX[d], 0, -kDirZ[d]))  // PlayL: connected side only
        seed(x + kDirX[d], y, z + kDirZ[d], s, uint8_t(c.behavior->emitChannel(d, st, c.mem)),
           reg.def(w.get(x, y, z)).role == SimRole::Repeator ? (kDirX[d] ? 1 : 2) : 0);  // Repeator feeds a side via end-on
      // Hard-powered block in front of a Repeator: colourless power to its other five faces.
      if (!c.mem.pulse && mountPowered_.contains(packPos(x + kDirX[d], y, z + kDirZ[d])) && (cPulseRules::hardAnyComponent() || reg.def(w.get(x, y, z)).role == SimRole::Repeator))
        for (auto& e : k6) if (!(e[1] == 0 && e[0] == -kDirX[d] && e[2] == -kDirZ[d])) seed(x + kDirX[d] + e[0], y + e[1], z + kDirZ[d] + e[2], s, 0);  // not back into the source face
    }
  }
}

// ---- propagate: one net's flood (pull-safe; touches only this net's NetArr
// span and its own NetWork record). Never touches ChunkMap structure, the
// shared buckets, slotFor/TraceStrengthBuffer, or the real DirtyFn (the
// incremental flood reports through the strength buffer only, as legacy).

void cSignalSim::hsPropagateNet(ChunkMap& /*w*/, int32_t netId, const DirtyFn& /*dirty*/) {
  HsTimer timer{hsWiresNs_};
  NetArr& a = netArr_[size_t(netId)];
  NetWork& nw = hsNetWork_[size_t(netId)];
  // AC item 4 (LL_AC_SAME_SEEDS): no pulses and the same seeds as this net's last flood since the
  // nets were built / the last full pass (netGen_) -> the flood result is the old one: restore it
  // and skip the spread. Pulses (a held Pulsor) always re-send. Wire values change only through
  // this net's flood or a full pass, so a.old is exactly that last result.
  if (acSameSeedsOn()) {
    std::sort(nw.seeds.begin(), nw.seeds.end());
    if (nw.pulses.empty() && nw.lastGen == netGen_ && nw.seeds == nw.lastSeeds) {
      for (size_t i = 0; i < a.st.size(); ++i) *a.st[i] = a.old[i];
      nw.strength.clear();
      sameSeedSkips_.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    nw.lastSeeds = nw.seeds;
    nw.lastGen = nw.pulses.empty() ? netGen_ : 0;  // pulse values are not a seed-only result
  }
  thread_local std::array<std::vector<int32_t>, kMaxSignal + 1> lb;
  for (auto& b : lb) b.clear();
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
      // thread in canonical (net, cell) order — the legacy final-loop order.
      nw.strength.push_back({a.keys[i], uint8_t(stValue(*a.st[i]))});
    }
}

void cSignalSim::hsApplyStrengthPending(const DirtyFn* dirty) {
  HsTimer timer{hsWiresNs_};
  // 2026-11-01 (JD wire render bug): the mesher bakes each Trace cell's value into the chunk mesh (no shader
  // reads the strength buffer yet), so an incremental flood that changed a wire must queue its sub-chunk for a
  // remesh, exactly as the full pass does. Serial, sim thread. LL_WIRE_REMESH=0: old (buffer only).
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
              if (const int64_t n = packPos(x + dx, y + dy, z + dz); comps_.contains(n)) awake_.insert(n);
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
  // are merged by the sim thread via hsApplyLampHolds (hs-design §3.3).
  auto one = [&](int64_t k) {
    int x, y, z; unpackPos(k, x, y, z);
    if (!area_.contains(x, z)) return;
    int v = 0;
    const LampFast* fast = nullptr;
    if (!fullPass_) if (const auto it = lampFast_.find(k); it != lampFast_.end()) fast = &it->second;
    if (fast) {  // 2026-11-01 PlayL fix: fast wires, soft and hard combine by max; v is never reset
      for (const CellState* p : fast->wires) v = std::max(v, int(stValue(*p)));
      if (!softPowered_.empty() || !mountPowered_.empty())
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
      if (*fast->self != ns) { *fast->self = ns; dirty(x, y, z); }
      return;
    }
    const bool color = reg.def(w.get(x, y, z)).role == SimRole::ColorLamp;
    CellState ns = color ? makeState(v, 0, 0, v > 0) : CellState(v > 0 ? 1 : 0);
    if (!color && reg.has(w.get(x, y, z), kDirectional))  // Cortana 2026-10-15 1900: keep the Emitor's facing
      ns = CellState(ns | (w.getState(x, y, z) & 0x0180));
    if (w.getState(x, y, z) != ns) { w.setState(x, y, z, ns); dirty(x, y, z); }
  };
  for (int64_t k : keys) one(k);
}

}  // namespace lithokraft
