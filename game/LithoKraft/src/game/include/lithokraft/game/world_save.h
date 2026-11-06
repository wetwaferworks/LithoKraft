// LithoKraft's saves on litholattice::storage: one region store per dimension
// (<root>/<dimension>/r.x.z.llr) carrying the engine's BLKS payload, the engine's MATS payload
// (per-voxel PBR material bytes, persisted as string IDs), plus the game's SGNL payload
// (signal-sim component memory and timers), and <root>/player.lat (position, look, dimension,
// hotbar). Encoding runs on the caller's thread; file I/O runs on each store's I/O thread.
#pragma once
#include <array>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <unordered_set>
#include <string>
#include <vector>
#include "lithokraft/content/signal.h"
#include "lithokraft/game/engine.h"
#include "lithokraft/game/inventory.h"  // PlayerMeta's v4 inventory block (P2b-1)
#include "litholattice/render/pbr_materials.h"  // cMaterialReg* (MATS payload registration)
#include "litholattice/core/math.h"
#include "litholattice/storage/block_names.h"
#include "litholattice/storage/storage.h"
#include "litholattice/storage/lf_world_store.h"  // 2026-11-01 LL_LF_SAVE / LL_LF_LOAD
#include "litholattice/sim/area_shape.h"  // Fix run 3: level.json v3 area
#include "litholattice/world/chunk_map.h"

namespace lithokraft {

class cWorldSnapshot;  // cWorldSnapshot.h (2026-10-31, Save World)

constexpr uint32_t kSignalTag = storage::fourcc("SGNL");
constexpr uint32_t kPostTextTag = storage::fourcc("PTXT");  ///< 2026-10-25 Posts: per-cell panel text

struct PlayerMeta {
  // v2 adds the stat block; v3 adds the vitals block (health/oxygen/stamina
  // moved from IStat to IVitals). v4 (P2b-1) stores the hotbar as STRING
  // ids (Appendix C M2: name-remapped on load like world blocks, aliases
  // included) and adds the inventory block below. v1–v3 files still load —
  // absent blocks read as the registered defaults, a v1–v3 hotbar reads as
  // the old raw BlockIds, and a pre-v4 file loads with an EMPTY inventory
  // (hasInventory == false; Game migrates the legacy hotbar into it).
  // v5 (wiring pass 2026-10-13, B6/B7) appends the stack-data block: per
  // data-bearing inventory slot, the Etchor name field ("name") and the
  // applied certificate id list. v1–v4 files load with plain stacks —
  // per-stack data simply did not exist before v5.
  // v6 (Cortana 2026-10-15 2255, JD 22:50) appends the prayer block: u64 playTicks (the
  // player's own persisted tick clock: the world tick is not saved), u32 prayerDay,
  // u8 prayersToday. v1-v5 files load with all three = 0 (a fresh clock, no prayers yet today).
  // v7 (Cortana 2026-10-16, JD mealtime prayers) replaces the v6 block with: u32 prayerDay (the
  // world-clock day) + u8 prayerWindows (bitmask of the 09:00/13:00/17:00 windows prayed that day).
  // The play clock is gone: time of day comes from the world clock (level.json worldTick/worldTime).
  // v6 files still load (playTicks/prayerDay/prayersToday filled; Game migrates the play clock).
  static constexpr uint16_t kVersion = 7;
  Vec3 pos{};
  float yaw = 0.f, pitch = 0.f;
  std::string dimension;
  int hot = 0;
  /// In memory: the BlockId each hotbar slot's stack places (kAir for
  /// empty slots and for non-block stacks like the Fragment). On disk in
  /// v4 these are STRING ids; in v1–v3 they were raw u16 BlockIds.
  std::vector<BlockId> hotbar;
  /// The canonical player inventory (P2b-1, Appendix G H1): every one of
  /// the 50 slots (42 carry + 8 gear, Inventory::allSlots order), written
  /// in v4 as (string id, count) per slot, empty slots explicit. On load,
  /// a stack whose string id resolves to nothing (removed item/block) is
  /// dropped with a log line — the rest of the inventory is never lost
  /// with it. Meaningful only when hasInventory is true.
  Inventory inventory;
  bool hasInventory = false;   ///< the file carried a v4 inventory block
  uint16_t fileVersion = 0;    ///< version actually read (0 = built in memory)
  /// Registered stat base values by stat NAME (player_adapter.h), e.g.
  /// {"kharge": 12}. Names are the permanent key — registry ids are
  /// registration-order and may shift as stats are added. Empty in v1 saves
  /// (and for a player who never touched a stat): everything reads as its
  /// registered default on load.
  std::map<std::string, int32_t> stats;
  /// Registered vital values by vital NAME, e.g. {"health": 40}. Vitals
  /// have no boost layer, so the stored value is the whole story. Empty in
  /// v1/v2 saves: vitals read as their registered defaults on load.
  std::map<std::string, int32_t> vitals;
  // v6 prayer block (kept LAST: PlayerMeta is aggregate-initialised positionally).
  uint64_t playTicks = 0;
  uint32_t prayerDay = 0;
  uint8_t prayersToday = 0;
  uint8_t prayerWindows = 0;  ///< v7: bitmask of the mealtime windows prayed on prayerDay
};

/// Cortana 2026-10-16 [GAME] (JD): the player save file name ("litho algorithmic type") and the
/// legacy name read when a world predates the rename.
inline constexpr const char* kPlayerFileName = "player.lat";
inline constexpr const char* kLegacyPlayerFileName = "player.dat";

class cWorldSave {
 public:
  /// `materials`: the PBR material registry (main.cpp owns it; must outlive this). When
  /// non-null the engine's MATS payload is registered, persisting per-voxel material bytes
  /// as string IDs. Null = no material persistence (tests that don't care about materials).
  explicit cWorldSave(std::filesystem::path root, const litholattice::cMaterialReg* materials = nullptr);
  const std::filesystem::path& root() const { return root_; }

  /// FAIL-CLOSED NAME TABLE (C-4): the semantics every method below follows.
  /// - NO blocks.names file at all (and no crash footprint of one) = the
  ///   documented legacy-identity path: a fresh world or a pre-table save.
  ///   Loads proceed with identity ids; saves write a fresh table.
  /// - A table that EXISTS but is unreadable/corrupt after crash recovery
  ///   AND region data exists for the dimension = fail closed:
  ///   loadDimension returns -1 (not 0, not "generated terrain kept"),
  ///   loadColumn reports ColumnLoad::Unreadable for every column (B-M3: the
  ///   streamer leaves a hole and retries; it never generates over the saved
  ///   column), and saveDimension
  ///   refuses (returns 0, lastSaveOk()==false, nothing is written — the
  ///   table, the regions and player.lat are all left untouched) so a
  ///   regenerated world can never overwrite the saved one. Game::saveWorld
  ///   surfaces the refusal as logError + a false return.
  /// - A corrupt table with NO region data loses nothing by proceeding:
  ///   the next save simply writes a fresh table over it.

  /// Saves `world` (blocks + `signal` state). The caller's part (the sim pause) only exports the
  /// signal memory and copies the columns to save; encoding and writing run on the I/O thread.
  /// `dirty`: save just these columns plus every column holding components (columns never
  /// changed since worldgen are regenerated from the seed on load); nullptr = every column.
  /// `wait`: block until the regions are on disk (exit); else return at once (autosave, F5).
  /// Returns the number of columns queued (0 when the save was refused or the
  /// table/metadata write failed — inspect lastSaveOk()/lastSaveError()).
  /// When a seed was set via setSeed(), the world metadata (level.json) is
  /// written through the file contract as part of the save (SV-H2).
  size_t saveDimension(const std::string& dim, const ChunkMap& world, const cSignalSim& signal, bool wait,
                       const std::unordered_set<uint64_t>* dirty = nullptr);
  /// 2026-10-31 (JD, Save World): saveDimension's disk half. saveDimension == capture a
  /// cWorldSnapshot (copies only, no I/O) + commit it. commit() does every disk step - name
  /// table check + write, level.json, region encode/write, flush when `wait` - and touches no
  /// live world memory, so the async save runs it on a pool worker. One committer at a time
  /// (Game defers save-on-evict while an async commit is in flight). Same return contract.
  size_t commit(cWorldSnapshot& snap, bool wait);
  /// Column eviction save (A2 / B-M1): saves ONLY the given columns
  /// (blocks + their SGNL state, exported from `signal` inside this call)
  /// through the same store path and fail-closed discipline as
  /// saveDimension. The caller (Game::evictFarColumns) invokes this BEFORE
  /// cSignalSim::unscanColumn drops the columns' sim state, in the same
  /// sim-stopped window as insertion. Returns the number of columns queued
  /// (0 when refused — inspect lastSaveOk()/lastSaveError()); with
  /// wait=true a 0/short return or !lastSaveOk() means the eviction of the
  /// unsaved columns must NOT proceed.
  size_t saveColumns(const std::string& dim, const std::vector<const ChunkColumn*>& cols, const cSignalSim& signal,
                     bool wait);
  /// Outcome of the most recent saveDimension: false when the save was
  /// refused (fail-closed name table), the table/metadata write failed, or —
  /// for wait=true — a region flush failed (SV-H1). For wait=false, region
  /// flushes complete asynchronously; their outcome lands in the stores'
  /// lastFlushOk() and in the bool returned by flush().
  bool lastSaveOk() const { return lastSaveOk_; }
  const std::string& lastSaveError() const { return lastSaveError_; }
  /// Replaces each column of `world` that has a saved copy; returns how many
  /// (only columns actually inserted are counted — L6). Returns -1 when the
  /// load was REFUSED (fail-closed name table, see above). The saved signal
  /// state is returned in `signalState` for cSignalSim::importState() after scan().
  int loadDimension(const std::string& dim, ChunkMap& world, std::vector<uint8_t>& signalState);

  /// World metadata (level.json, SV-H2): the world seed + save format
  /// version, written through writeFileAtomic whenever the world is saved.
  /// The seed is stored as a decimal STRING: the engine Json number type is
  /// a double and cannot hold every uint64 seed exactly.
  /// v2 (Cortana 2026-10-16): adds the world clock, "worldTick" (sim ticks ever run in this world)
  /// and "worldTime" (world_clock.h units), both decimal strings. v1 files simply have no clock.
  /// v3 (Fix run 3, 2026-10-23): the sim/stream area the world runs with,
  /// "simAreaShape" / "streamAreaShape" (square|round|diamond) and
  /// "tickRadius" (number). v1/v2 files have none: readWorldArea returns
  /// nullopt with err empty and the caller keeps its configured area.
  static constexpr int kWorldMetaVersion = 3;
  struct WorldAreaMeta {
    litholattice::AreaShape sim = litholattice::kDefaultAreaShape;
    litholattice::AreaShape stream = litholattice::kDefaultAreaShape;
    int tickRadius = 8;
  };
  void setWorldArea(const WorldAreaMeta& a) { area_ = a; hasArea_ = true; }
  static std::optional<WorldAreaMeta> readWorldArea(const std::filesystem::path& root, std::string* err);
  void setSeed(uint64_t seed) { seed_ = seed; hasSeed_ = true; }
  struct WorldClockMeta { uint64_t tick = 0, time = 0; };
  /// The clock the next level.json write carries (Game sets it before each save).
  void setWorldClock(uint64_t tick, uint64_t time) { clockTick_ = tick; clockTime_ = time; hasClock_ = true; }
  /// The saved clock, or nullopt when level.json is absent or predates v2 (err empty) / is unusable (err set).
  static std::optional<WorldClockMeta> readWorldClock(const std::filesystem::path& root, std::string* err);
  bool writeWorldMeta(std::string* err) const;  ///< no-op success when no seed was set
  /// 2026-10-25 (JD): named /tp spots of this world (optional "tpMarks" array in level.json:
  /// [{name, dim, x, y, z}]). Absent key = none (older files load unchanged).
  struct TpMark { std::string name, dim; float x = 0, y = 0, z = 0; };
  /// Archie Phase 3b: lock-free publish (game thread writes, save reads a snapshot). No tpMutex_.
  void setTpMarks(std::vector<TpMark> m) {
    std::atomic_store_explicit(&tpMarks_, std::make_shared<const std::vector<TpMark>>(std::move(m)),
                               std::memory_order_release);
  }
  static std::vector<TpMark> readTpMarks(const std::filesystem::path& root, std::string* err);
  /// The persisted seed for the save at `root`, or nullopt when there is no
  /// level.json (pre-metadata save: the caller's seed stands). A level.json
  /// that exists but is corrupt after crash recovery yields nullopt with
  /// `err` SET (distinct from absent) so the caller can warn loudly.
  static std::optional<uint64_t> readWorldSeed(const std::filesystem::path& root, std::string* err);
  /// Outcome of a streamed single-column load (B-M3): the three cases the
  /// streamer MUST distinguish. NotSaved = no saved copy exists, generate
  /// fresh. Unreadable = a saved copy (or the block-name table interpreting
  /// it) exists but cannot be read: fail loudly, NEVER generate over it and
  /// never treat the key as handled — the column stays a hole and is retried.
  enum class ColumnLoad { Loaded, NotSaved, Unreadable };
  struct LoadedColumn {
    ColumnLoad status = ColumnLoad::NotSaved;
    std::unique_ptr<ChunkColumn> column;  ///< non-null only when status == Loaded
    /// This column's SGNL payload bytes (component memory/timers for exactly
    /// this column's components, self-delimiting entries as in loadDimension's
    /// blob). Surfaced per column (SW-H1 / C-M4) so the caller can scanColumn
    /// + importState them on insertion; empty when the column had none.
    std::vector<uint8_t> signal;
  };
  /// Streaming load (chunk streaming): one saved column, remapped to the live
  /// registry, with its SGNL payload. Thread-safe LF path (Archie Phase 3c): callable from job-pool workers; SGNL sink is per-request TLS decode context (C-M4).
  LoadedColumn loadColumn(const std::string& dim, int cx, int cz);

  /// v1 -> v2 signal state migration (adds channel=1, beacon=0). Exposed for tests.
  static std::optional<storage::Bytes> migrateSignalV1ToV2(const storage::Bytes& v1);
  /// v2 -> v3 (2026-10-24): appends u16 historyHi = 0 after beacon. Delay index (cell state) is kept,
  /// so an old save keeps its button; the button now means 4 / 8 / 16 / 32 ticks.
  static std::optional<storage::Bytes> migrateSignalV2ToV3(const storage::Bytes& v2);
  /// 2026-10-25 (JD, Posts): the "PTXT" payload codec (public for the selftests).
  static storage::PayloadType postTextPayload();
  /// Every queued write on disk; returns false when any store's flush failed
  /// (SV-H1 — the failed regions keep their pending records for a retry).
  bool flush();

  /// player.lat goes through the file contract (SV-H3): writePlayer uses
  /// writeFileAtomic (fsync + backup rotation); readPlayer first runs
  /// recoverCrashedWrite with a magic+CRC validator, so a torn primary
  /// heals from the newest intact backup generation instead of losing the
  /// player. writePlayer always writes v4 (string-id hotbar + the v4
  /// inventory block); readPlayer accepts v1–v4 and the CRC gate is
  /// unchanged.
  bool writePlayer(const PlayerMeta& m, std::string* err) const;
  std::optional<PlayerMeta> readPlayer(std::string* err) const;

  const storage::cPayloadReg& payloads() const { return payloads_; }

  /// Archie Phase 3c (PlayH): save/load I/O is ALWAYS lock-free (cLfWorldStore). Legacy mutex
  /// path removed. setLockFreeIo kept as a no-op for callers; lockFreeIo() always true.
  void setLockFreeIo(bool /*save*/, bool /*load*/) {}
  bool lockFreeIo() const { return true; }
  uint64_t lfLoadsJumped();  ///< loads that skipped the save FIFO (all LF stores)

 private:
  size_t commitLf(cWorldSnapshot& snap, bool wait);
  size_t saveColumnsLf(const std::string& dim, const std::vector<const ChunkColumn*>& cols, const cSignalSim& signal, bool wait);
  int loadDimensionLf(const std::string& dim, ChunkMap& world, std::vector<uint8_t>& signalState);
  LoadedColumn loadColumnLf(const std::string& dim, int cx, int cz);
  storage::cLfWorldStore& lfStore(const std::string& dim);
  std::filesystem::path root_;
  storage::cPayloadReg payloads_;
  uint64_t seed_ = 0;       ///< world seed for level.json (SV-H2); set by Game from cfg.seed
  bool hasSeed_ = false;
  std::atomic<uint64_t> clockTick_{0}, clockTime_{0};  ///< Cortana 2026-10-16: world clock for level.json v2
  std::atomic<bool> hasClock_{false};
  WorldAreaMeta area_;    ///< Fix run 3: level.json v3 area (set once by Game before any save)
  bool hasArea_ = false;
  /// Archie Phase 3b: published snapshot; setTpMarks CAS-publishes, writeWorldMeta loads.
  std::shared_ptr<const std::vector<TpMark>> tpMarks_{std::make_shared<const std::vector<TpMark>>()};
  bool lastSaveOk_ = true;  ///< outcome of the last saveDimension (see above)
  std::string lastSaveError_;
  // ---- Archie Phase 3c: LF stores only (declared last: destroyed first, before payloads_)
  struct LfStoreEntry { std::string dim; std::unique_ptr<storage::cLfWorldStore> store; };
  struct LfStores {  // published once per dimension by CAS; freed at destruction
    std::array<std::atomic<LfStoreEntry*>, 32> slot{};
    ~LfStores() { for (auto& s : slot) delete s.load(std::memory_order_acquire); }
  } lfStores_;
  struct LfRemap {  // the streamed-load remap, published once by CAS (success only, as before)
    std::atomic<const storage::StableRemap*> p{nullptr};
    ~LfRemap() { delete p.load(std::memory_order_acquire); }
  } lfRemap_;
};

}  // namespace lithokraft
