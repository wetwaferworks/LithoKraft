// cPlayer-centered chunk streaming for the real game path.
//
// Root cause this fixes: Dimension::load() pre-bakes exactly one fixed square of
// columns, [0, worldChunks) x [0, worldChunks), anchored at the world origin, and
// nothing in the game path ever generated or loaded another column afterwards —
// walking away from spawn meant walking into void (ChunkMap::get on a missing
// column is air, surfaceY is -1, set() fails).
//
// cChunkStreamer closes that hole by reusing the existing machinery, not a
// parallel system: the dimension's cColumnGen (same worldgen the pre-bake
// uses), the game's cJobPool, and cWorldSave (per-column disk loads through the
// existing cWorldStore / region format — the save format is unchanged). The Game
// calls update() once per frame with the player position; columns inside the
// load radius (GameConfig::tickRadius, the same radius the sim ticks) that are
// neither loaded nor in flight are requested nearest-first. Finished columns are
// handed back through takeReady() and inserted by the Game while the sim thread
// is stopped — the same stop/insert/start pattern Game::saveWorld already uses —
// because the sim thread owns the ChunkMap while it runs.
//
// Scope (deliberate, first pass): columns are never unloaded, so memory grows
// with explored area. Failures ARE retried (B-L7): a job that throws, fails to
// generate, or finds its saved copy unreadable (B-M3) reports its key back,
// update() un-requests it, and it is re-requested after an exponential
// backoff (500 ms doubling, capped at 30 s, no permanent give-up — a hole
// can always heal, and an unreadable saved column is never generated over).
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "litholattice/core/threading.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/world/chunk_ingress.h"
#include "litholattice/world/column_generator.h"
#include "lithokraft/game/stream_rank.h"

namespace lithokraft {

class cWorldSave;

/// One finished streamed column, as handed to the Game by takeReady().
struct StreamedColumn {
  std::unique_ptr<litholattice::ChunkColumn> column;
  /// The column's SGNL payload bytes when it came from disk (SW-H1): the
  /// Game scanColumn()s the inserted column and importState()s these bytes
  /// while the sim is stopped. Empty for generated columns (and for saved
  /// columns that hold no components).
  std::vector<uint8_t> signal;
  bool fromDisk = false;  ///< true = loaded from the save; false = freshly generated
};

class cChunkStreamer {
 public:
  /// `gen`: the active dimension's generator (shared with worldgen jobs).
  /// `save` / `dimensionId`: when save is non-null, a saved column on disk wins
  /// over fresh generation (player edits persist across sessions). Both must
  /// outlive the streamer. `initial`: keys of the columns the pre-bake already
  /// loaded, so they are never re-requested.
  cChunkStreamer(litholattice::cJobPool& pool, std::shared_ptr<const litholattice::cColumnGen> gen, cWorldSave* save,
                std::string dimensionId, int radiusChunks, const std::unordered_set<uint64_t>& initial);
  /// Bounded wait for in-flight jobs (SW-H3): at most kDrainTimeout, then a
  /// loud log and return — never an unbounded busy-wait. Jobs hold shared_ptr
  /// copies of the queues/counters and of the generator, so a job that
  /// outlives the streamer finishes into orphaned queues safely.
  ~cChunkStreamer();

  /// Diamond (Manhattan) footprint for the wanted set + eviction (JD 2026-10-23).
  void setDiamond(bool d) { diamond_ = d; }
  /// R3 (B7): the player's view sector (cStreamRank::sectorOf), set by the Game before update().
  /// Requests and staged promotion rank in-front columns first. kNoDirection = distance only.
  void setViewSector(int s) { viewSector_ = s; }
  int viewSector() const { return viewSector_; }
  cStreamRank rankAround(int cx, int cz) const { return cStreamRank(cx, cz, viewSector_, diamond_); }
  /// 2026-10-25 stutter pass: requests farther than this (same metric as the footprint) are not
  /// admitted; resident columns are untouched. The game lowers it while the mesh pool is > 90% full.
  void setAdmitRadius(int r) { admitR_ = r; }
  int admitRadius() const { return admitR_; }

  /// 2026-10-28 frame-aware request budget (Archie: optimization
  /// design; Elfie: co-implementation; JD: direction + debugging):
  /// the max NEW column requests admitted per frame cycle — update()
  /// opens the cycle and spends first (player priority), updateAround
  /// spends what is left. -1 (the default) is the legacy behaviour:
  /// admission bounded only by the in-flight cap in pumpRequests.
  void setRequestBudget(int n) { requestBudget_ = n; }
  int requestBudget() const { return requestBudget_; }

  /// S4 emergency floor (2026-10-28, Archie ruling): a floor under the
  /// per-cycle admission of NEW generation jobs (the wafer data /
  /// collision stage). The Game sets 4 while the player is within 2
  /// chunks of unloaded ground, -1 (the default) otherwise. Admission
  /// uses max(budget, floor) when a floor is set, so the frame-aware
  /// EMA grant can never starve data generation at the world edge.
  /// This gates ONLY job admission: finished columns still pass the
  /// Game's bounded insertion (kStreamInsertBudget) and the ordinary
  /// mesh submission path — render-mesh compilation is not raised.
  void setRequestBudgetFloor(int n) { requestFloor_ = n; }
  int requestBudgetFloor() const { return requestFloor_; }

  /// Main thread, once per frame: request missing columns within the radius of
  /// the player's chunk, nearest first, bounded by the in-flight cap.
  void update(int playerCx, int playerCz);
  /// Secondary centers (P2c 2026-10-13, CAM-2 + ENT-1): request missing
  /// columns within `radiusChunks` of another center (a non-player
  /// camera's chunk-load center, a projectile's path column). Same
  /// machinery, same dedup set, same in-flight cap — callers issue the
  /// player's update() FIRST each frame, so secondary centers only ever
  /// spend the in-flight capacity the player did not use (player
  /// priority). Radius is clamped to [0, radius_] (a secondary center
  /// never loads wider than the player's own radius).
  void updateAround(int cx, int cz, int radiusChunks);
  /// Request one specific column if it is neither loaded nor in flight
  /// (the projectile manager's chunk seam lands here). Same dedup,
  /// backoff and in-flight cap as update().
  void requestColumn(int cx, int cz);
  /// Drain columns that finished loading/generating since the last call. The
  /// caller owns inserting them into the ChunkMap (sim stopped) and meshing.
  std::vector<StreamedColumn> takeReady();
  /// Bounded drain (P3-2 B-M2): like takeReady(), but returns at most `max`
  /// columns; the rest stay staged (main-thread side) for the next call, so
  /// a burst of finished columns cannot land in ONE sim-stopped insertion
  /// window. Game::pumpStreaming inserts through this with a small
  /// per-frame budget; the snapshot-then-insert discipline is unchanged
  /// (columns are fully generated/loaded before they are ever handed out).
  std::vector<StreamedColumn> takeReadyUpTo(size_t max);
  /// Thread Handshake P5 promotion pop (Archie ruling, R7): drains the Sim
  /// Ingress into staging, then hands out every staged column inside the
  /// ring-center bypass (spawnSpiralBypass: Chebyshev 1 around (centerCx,
  /// centerCz), only while `spiralActive`) regardless of budget, plus up to
  /// `budget` others in cStreamRank order around the centre (R3; was staging order, which
  /// is worker-completion order). `*bypassed` gets the bypass count
  /// (those do not spend the budget).
  std::vector<StreamedColumn> takeReadyPromote(size_t budget, int centerCx, int centerCz, bool spiralActive, size_t* bypassed);
  bool hasReady() const { return !staged_.empty() || readyCount_->load(std::memory_order_relaxed) > 0; }
  /// Column eviction (A2 / B-M1): forgets a RESIDENT column the Game has
  /// evicted — removes its requested_ entry (so a later update() may
  /// re-stream it), drops any staged (finished-but-not-yet-inserted) copy,
  /// and clears retry backoff. Main thread only, like update(). A column
  /// still in flight is NOT forgotten (its job will deliver normally and
  /// the Game's insert path dedups against the resident map).
  void forgetColumn(int cx, int cz);
  /// Columns requested so far that did not come from the pre-bake (diagnostics/tests).
  size_t streamedCount() const { return requested_.size() - initialCount_; }
  size_t requestedCount() const { return requested_.size(); }

 private:
  /// Shared request pump behind update()/updateAround()/requestColumn():
  /// drains failures (arming retry backoff), then submits generation /
  /// load jobs for the wanted columns nearest-first until the in-flight
  /// cap. `want` is (rank, key) pairs, already dedup-filtered (R3: cStreamRank, in front first).
  void pumpRequests(std::vector<std::pair<cStreamRank::Key, uint64_t>> want);
  /// True when `key` may be requested right now (not already requested,
  /// not inside a retry backoff window at `now`).
  bool requestable(uint64_t key, std::chrono::steady_clock::time_point now);
  /// B-L2 (P3-2): true when chunk (cx, cz)'s block extent fits the packPos
  /// range (chunk.h: x,z in [-2^20, 2^20-1]). Out-of-range centres are
  /// refused (their cells' packPos keys would alias onto other cells);
  /// the refusal is logged loudly once per streamer, not per frame.
  bool centreInRange(int cx, int cz);
  bool boundWarned_ = false;  ///< B-L2: the out-of-range refusal was logged
  static constexpr auto kDrainTimeout = std::chrono::seconds(5);  // SW-H3 bounded destructor wait
  struct Retry { int attempts = 0; std::chrono::steady_clock::time_point nextTry{}; };
  litholattice::cJobPool& pool_;
  std::shared_ptr<const litholattice::cColumnGen> gen_;
  cWorldSave* save_;  // null = generate-only (tests, --no-save)
  std::string dimensionId_;
  int radius_;
  int admitR_ = 1 << 20;  ///< 2026-10-25 stutter pass (see setAdmitRadius)
  int requestBudget_ = -1;    ///< frame-aware budget (see setRequestBudget)
  int requestFloor_ = -1;     ///< S4 emergency floor (see setRequestBudgetFloor); -1 = no floor
  int admittedThisCycle_ = 0; ///< admissions since the last update() opened a cycle
  bool diamond_ = false;  ///< diamond (Manhattan) stream footprint (JD 2026-10-23)
  int viewSector_ = cStreamRank::kNoDirection;  ///< R3: see setViewSector
  size_t initialCount_ = 0;
  std::unordered_set<uint64_t> requested_;  // loaded or in flight (failures are erased by update(), B-L7)
  std::unordered_map<uint64_t, Retry> retries_;  // failed keys: attempt count + backoff deadline (main thread)
  // Thread Handshake P5: the Sim Ingress - worldgen/load jobs (many producers)
  // -> main-thread promotion (one consumer). Lock-free ring, lossless overflow.
  std::shared_ptr<litholattice::cIngress<StreamedColumn>> done_ =
      std::make_shared<litholattice::cIngress<StreamedColumn>>(64);
  std::shared_ptr<litholattice::cHandoffQueue<uint64_t>> failed_ =
      std::make_shared<litholattice::cHandoffQueue<uint64_t>>();
  std::shared_ptr<std::atomic<int>> inFlight_ = std::make_shared<std::atomic<int>>(0);
  std::shared_ptr<std::atomic<int>> readyCount_ = std::make_shared<std::atomic<int>>(0);
  /// B-M2 staging for bounded drains: finished columns moved out of the
  /// lock-free done_ queue but not yet handed to the Game. Main thread only.
  std::vector<StreamedColumn> staged_;
};

/// Column eviction candidate selection (A2 / B-M1): keys of resident
/// columns whose Chebyshev chunk distance from (centerCx, centerCz)
/// exceeds streamRadius + hysteresisChunks, farthest first (ties broken
/// by key, so the choice is deterministic), truncated to `budget`.
/// Pure function of the resident key set — the Game feeds it the active
/// dimension's column keys each pump; hysteresis/budget values are the
/// caller's policy (JD-DECISION parameters, see game.cpp).
std::vector<uint64_t> selectEvictionCandidates(const std::vector<uint64_t>& residentKeys, int centerCx, int centerCz,
                                               int streamRadius, int hysteresisChunks, size_t budget, bool diamond = false);

/// Fix 3b (JD 2026-10-23, t2279u): request priority of a column at offset (dx, dz) from
/// the player chunk. Smaller loads first. Diamond streams rank by |dx|+|dz| (then dx^2+dz^2);
/// square/round streams rank by dx^2+dz^2. The streamer sorts (priority, key) pairs, so ties
/// break by chunk key: deterministic and independent of the input order.
int streamRequestPriority(int dx, int dz, bool diamond);

}  // namespace lithokraft
