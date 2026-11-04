// Async world save (2026-10-31, JD: Save World, lock-free, nobody waits).
//   main thread   begin(): sets the world clock, posts ONE tick-boundary job (cSimThread::
//                 postBoundaryJob: an atomic pointer exchange) and returns at once.
//   tick thread   at the next tick boundary (it already holds the world): takes the changed-
//                 column set, captures a cWorldSnapshot, hands it to a pool worker (submit).
//   pool worker   commits the snapshot (name table, level.json, region encode + write, flush)
//                 and player.lat, then publishes the result with one release store.
//   main thread   poll() each frame: an acquire load; reports the result once.
// The sim is never stopped or paused and the frame never blocks. The state machine is one
// std::atomic<int>; the result is plain data written before the release store that publishes it.
// LL_SAVE_ASYNC=0 restores the old stop-the-sim save for F5 / autosave.
#pragma once
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include "lithokraft/game/world_save.h"
#include "litholattice/sim/sim_thread.h"

namespace lithokraft {

class cAsyncSaver final {
 public:
  struct Result {
    bool ok = false;
    std::string reason, dim, error;
    size_t columns = 0;
    size_t bytesCopied = 0;      ///< voxel bytes the snapshot copied
    uint64_t bytesOnDisk = 0;    ///< region files of the dimension after the write
    double queueMs = 0;          ///< post -> tick boundary
    double snapshotMs = 0;       ///< tick-thread capture (the only time the world is held)
    double commitMs = 0;         ///< pool worker: encode + write + flush + player.lat
    double totalMs = 0;          ///< post -> on disk
  };
  using Submit = std::function<void(std::function<void()>)>;

  static bool enabled();  ///< LL_SAVE_ASYNC != "0" (default ON)
  bool busy() const { const int s = state_.load(std::memory_order_acquire); return s == kPosted || s == kCommitting; }
  /// Main thread. `player` is the pose/inventory at post time. Returns false (nothing posted) when
  /// a save is in flight or the sim is not running; the caller then uses the synchronous path.
  bool begin(cSimThread& sim, cWorldSave& save, const ChunkMap& world,
             const cSignalSim& signal, const std::string& dim, PlayerMeta player, const char* reason, Submit submit);
  /// Main thread, every frame: the finished result, once.
  std::optional<Result> poll();
  /// Main thread: wait until the snapshot left the world (the tick thread took it) for up to
  /// `timeoutMs`; on timeout the job is cancelled and false is returned (caller saves synchronously).
  bool settleSnapshot(cSimThread* sim, int timeoutMs);
  /// Main thread (exit / teardown / before a synchronous save): cancel a job that never reached
  /// a tick boundary, else wait for the in-flight commit to land.
  void drain(cSimThread* sim);

 private:
  enum : int { kIdle = 0, kPosted, kCommitting, kDone };
  std::atomic<int> state_{kIdle};
  Result result_;  // written by the tick thread / pool worker strictly before the release store of kDone
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace lithokraft
