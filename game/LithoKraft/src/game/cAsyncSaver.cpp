// Async world save: see cAsyncSaver.h. 2026-10-31 (JD: Save World).
#include "lithokraft/game/cAsyncSaver.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <thread>
#include "lithokraft/game/cWorldSnapshot.h"
#include "litholattice/core/log.h"

namespace lithokraft {

namespace {
double msSince(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}
uint64_t dirBytes(const std::filesystem::path& p) {
  std::error_code ec;
  uint64_t n = 0;
  for (auto it = std::filesystem::directory_iterator(p, ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
    if (it->is_regular_file(ec)) n += it->file_size(ec);
  return n;
}
}  // namespace

bool cAsyncSaver::enabled() {
  static const bool on = [] { const char* e = std::getenv("LL_SAVE_ASYNC"); return !(e && std::strcmp(e, "0") == 0); }();
  return on;
}

bool cAsyncSaver::begin(cSimThread& sim, cWorldSave& save, const ChunkMap& world,
                        const cSignalSim& signal, const std::string& dim, PlayerMeta player, const char* reason,
                        Submit submit) {
  int expect = state_.load(std::memory_order_acquire);
  if (expect == kPosted || expect == kCommitting) return false;
  if (!state_.compare_exchange_strong(expect, kPosted, std::memory_order_acq_rel)) return false;
  result_ = Result{};
  result_.reason = reason;
  result_.dim = dim;
  t0_ = std::chrono::steady_clock::now();
  auto meta = std::make_shared<PlayerMeta>(std::move(player));
  const bool posted = sim.postBoundaryJob([this, &sim, &save, &world, &signal, dim, meta, submit] {
    // ---- tick thread, tick boundary: the only moment the world is held for the save
    result_.queueMs = msSince(t0_);
    const std::unordered_set<uint64_t> dirty = sim.takeChangedColumns();
    std::shared_ptr<cWorldSnapshot> snap = cWorldSnapshot::capture(dim, world, signal, &dirty);
    result_.snapshotMs = snap->captureMs();
    result_.bytesCopied = snap->bytesCopied();
    state_.store(kCommitting, std::memory_order_release);
    submit([this, &save, snap, meta] {
      // ---- pool worker: everything that touches the disk
      const auto c0 = std::chrono::steady_clock::now();
      const size_t n = save.commit(*snap, /*wait=*/true);
      bool ok = save.lastSaveOk();
      std::string err = ok ? std::string() : save.lastSaveError();
      if (ok && !save.writePlayer(*meta, &err)) ok = false;
      result_.columns = n;
      result_.ok = ok;
      result_.error = err;
      result_.commitMs = msSince(c0);
      result_.bytesOnDisk = dirBytes(save.root() / snap->dimension());
      result_.totalMs = msSince(t0_);
      state_.store(kDone, std::memory_order_release);  // publishes result_
    });
  });
  if (!posted) { state_.store(kIdle, std::memory_order_release); return false; }
  return true;
}

std::optional<cAsyncSaver::Result> cAsyncSaver::poll() {
  if (state_.load(std::memory_order_acquire) != kDone) return std::nullopt;
  Result r = result_;
  state_.store(kIdle, std::memory_order_release);
  return r;
}

bool cAsyncSaver::settleSnapshot(cSimThread* sim, int timeoutMs) {
  const auto t0 = std::chrono::steady_clock::now();
  unsigned spins = 0;
  while (state_.load(std::memory_order_acquire) == kPosted) {
    if (msSince(t0) > timeoutMs) {
      if (sim && sim->cancelBoundaryJob()) { state_.store(kIdle, std::memory_order_release); return false; }
      // the tick thread claimed it between the load and the cancel: it is capturing now
    }
    // 2026-10-31 (close-hang audit): yield briefly, then sleep 1 ms so a long wait does not burn a core.
    if (++spins < 256) std::this_thread::yield(); else std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

void cAsyncSaver::drain(cSimThread* sim) {
  if (state_.load(std::memory_order_acquire) == kPosted && sim && sim->cancelBoundaryJob())
    state_.store(kIdle, std::memory_order_release);
  // 2026-10-31 (close-hang audit): the old loop yield-spun at 100% CPU and tried the cancel only
  // once, so a job still Posted (no tick boundary reached: the sim is stopping or stopped) spun
  // forever after the window closed. Now: retry the cancel every pass, sleep 1 ms after a short
  // yield phase, and log every 5 s while waiting. A Committing job is real disk work: wait it out.
  const auto t0 = std::chrono::steady_clock::now();
  unsigned spins = 0;
  double nextLog = 5000.0;
  while (busy()) {
    if (state_.load(std::memory_order_acquire) == kPosted && sim && sim->cancelBoundaryJob()) {
      state_.store(kIdle, std::memory_order_release);
      litholattice::logInfo("save: drain cancelled a posted async save after %.0f ms", msSince(t0));
      return;
    }
    if (++spins < 256) std::this_thread::yield(); else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if (msSince(t0) > nextLog) {
      litholattice::logError("save: drain still waiting after %.0f ms (state %d)", msSince(t0), state_.load(std::memory_order_acquire));
      nextLog += 5000.0;
    }
  }
  if (msSince(t0) > 100.0) litholattice::logInfo("save: drain waited %.0f ms for the async save", msSince(t0));
}

}  // namespace lithokraft
