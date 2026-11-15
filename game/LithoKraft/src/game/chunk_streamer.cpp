#include "lithokraft/game/chunk_streamer.h"
#include "litholattice/core/lf_job_queue.h"  // 2026-11-01 LL_LF_GEN
#include "lithokraft/game/stall_timer.h"  // 2026-11-01 LL_STALL_TIMING
#include <atomic>
#include <algorithm>
#include <chrono>
#include <thread>
#include <utility>
#include "lithokraft/game/world_save.h"
#include "litholattice/core/log.h"
#include "litholattice/world/chunk.h"

namespace lithokraft {

namespace {
/// SW-H3: RAII in-flight accounting for one streamer job. The counter is
/// decremented on EVERY exit from the job lambda — normal return, early
/// return, and exception unwinding alike (cJobPool catches the exception
/// outside the lambda, after this destructor has run). Unless the job marks
/// success, the key is also reported on the failure queue so update() can
/// un-request it and schedule a retry (B-L7); a throwing job can therefore
/// neither leak the in-flight count (which used to stall all streaming at
/// the kMaxInFlight gate) nor wedge the key in requested_ forever.
struct JobGuard {
  std::shared_ptr<std::atomic<int>> inFlight;
  std::shared_ptr<litholattice::cHandoffQueue<uint64_t>> failed;
  uint64_t key = 0;
  bool success = false;
  JobGuard(std::shared_ptr<std::atomic<int>> n, std::shared_ptr<litholattice::cHandoffQueue<uint64_t>> f, uint64_t k)
      : inFlight(std::move(n)), failed(std::move(f)), key(k) {}
  JobGuard(const JobGuard&) = delete;
  JobGuard& operator=(const JobGuard&) = delete;
  ~JobGuard() {
    if (!success) failed->push(key);
    inFlight->fetch_sub(1, std::memory_order_relaxed);
  }
};
}  // namespace

cChunkStreamer::cChunkStreamer(litholattice::cJobPool& pool, std::shared_ptr<const litholattice::cColumnGen> gen, cWorldSave* save,
                             std::string dimensionId, int radiusChunks, const std::unordered_set<uint64_t>& initial)
    : pool_(pool),
      gen_(std::move(gen)),
      save_(save),
      dimensionId_(std::move(dimensionId)),
      radius_(std::max(0, radiusChunks)),
      requested_(initial) {
  initialCount_ = initial.size();
  // B-L2 (P3-2) setup enforcement: the pre-baked columns this streamer
  // inherits must sit inside the packPos range; an out-of-range column's
  // cells would alias onto other cells' signal/light keys. Loud at setup
  // (dimension registration is the other enforcement point — a dimension
  // must declare bounds inside [-2^20, 2^20-1] blocks; see chunk.h).
  for (uint64_t key : initial) {
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    if (!centreInRange(cx, cz))
      logError("stream: dimension '%s' pre-baked column %d,%d is OUTSIDE the packPos range "
               "(blocks must stay within [%d, %d]); its cells would alias in packPos-keyed sets",
               dimensionId_.c_str(), cx, cz, litholattice::kPackPosCoordMin, litholattice::kPackPosCoordMax);
  }
}

bool cChunkStreamer::centreInRange(int cx, int cz) {
  const int64_t x0 = int64_t(cx) * 16, z0 = int64_t(cz) * 16;
  return x0 >= litholattice::kPackPosCoordMin && x0 + 15 <= litholattice::kPackPosCoordMax &&
         z0 >= litholattice::kPackPosCoordMin && z0 + 15 <= litholattice::kPackPosCoordMax;
}

cChunkStreamer::~cChunkStreamer() {
  // Jobs hold shared_ptr copies of the queues/counters, so they can always
  // finish safely; wait them out so no job touches a dead streamer — but
  // BOUNDED (SW-H3): a stuck job must not hang exit / dimension switch.
  // Mirroring the timing-handshake stop policy's spirit (bounded join, loud
  // log, no silent detach): after kDrainTimeout we log loudly and return;
  // the orphaned job still only touches shared_ptr-owned state and the
  // generator (shared_ptr), and in every Game teardown path the cWorldSave
  // and the cJobPool outlive the streamer, so it finishes harmlessly.
  const auto deadline = std::chrono::steady_clock::now() + kDrainTimeout;
  while (inFlight_->load(std::memory_order_relaxed) > 0) {
    if (std::chrono::steady_clock::now() >= deadline) {
      logError("stream: streamer destroyed with %d job(s) still in flight after %lld ms — NOT waiting longer; "
               "the job(s) finish into orphaned queues (see chunk_streamer.h)",
               inFlight_->load(std::memory_order_relaxed),
               static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(kDrainTimeout).count()));
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool cChunkStreamer::requestable(uint64_t key, std::chrono::steady_clock::time_point now) {
  if (requested_.count(key)) return false;
  if (const auto it = retries_.find(key); it != retries_.end() && now < it->second.nextTry) return false;
  return true;
}

void cChunkStreamer::pumpRequests(std::vector<std::pair<int, uint64_t>> want) {
  // B-L7: collect job failures first — un-request those keys and arm their
  // retry backoff, so a transient fault heals on a later update instead of
  // leaving a permanent hole for the session.
  {
    std::vector<uint64_t> failedKeys;
    failed_->drainInto(failedKeys);
    const auto now = std::chrono::steady_clock::now();
    for (uint64_t key : failedKeys) {
      requested_.erase(key);
      Retry& r = retries_[key];
      ++r.attempts;
      const int shift = std::min(r.attempts, 6);  // 1s, 2s, 4s ... capped below
      const auto delay = std::min(std::chrono::milliseconds(500 << shift), std::chrono::milliseconds(30000));
      r.nextTry = now + delay;
    }
  }
  // Nearest-first: when something crosses into ungenerated territory the
  // ground underfoot streams in before the horizon does.
  constexpr int kMaxInFlight = 32;  // leave pool capacity for meshing
  std::sort(want.begin(), want.end());  // (priority, key): nearest first, key tiebreak (fix 3b)
  for (const auto& [dist2, key] : want) {
    (void)dist2;
    if (inFlight_->load(std::memory_order_relaxed) >= kMaxInFlight) break;
    // Frame-aware budget (2026-10-28, see setRequestBudget): the
    // cycle's new-request grant is spent; later wants wait a frame.
    // S4: the emergency floor raises the effective grant for data /
    // collision generation admissions only (see setRequestBudgetFloor).
    const int effBudget = requestFloor_ >= 0 ? std::max(requestBudget_, requestFloor_) : requestBudget_;
    if (effBudget >= 0 && admittedThisCycle_ >= effBudget) break;
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    if (!centreInRange(cx, cz)) continue;  // B-L2: per-column edge guard (a wide radius near the bound)
    if (!requested_.insert(key).second) continue;
    inFlight_->fetch_add(1, std::memory_order_relaxed);
    ++admittedThisCycle_;
    cWorldSave* save = save_;
    const std::string dim = dimensionId_;
    auto job = [save, dim, cx, cz, key, done = done_, failed = failed_, n = inFlight_, ready = readyCount_, gen = gen_] {
      JobGuard guard{n, failed, key};  // SW-H3: decrements (and reports failure) on ALL exits
      try {
        // Disk first: a column the player edited in an earlier session must come
        // back as saved, not as fresh terrain (SW-H1: its SGNL payload rides
        // along in the result and is imported by the Game on insertion).
        if (save) {
          // 2026-11-01 LL_STALL_TIMING: aggregate streamed-load time (worker thread; a log line per 256 loads)
          const auto lt0 = cStallTimer::enabled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
          cWorldSave::LoadedColumn loaded = save->loadColumn(dim, cx, cz);
          if (cStallTimer::enabled()) {
            static std::atomic<uint64_t> ns{0}, cnt{0}, worst{0};
            const uint64_t d = uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - lt0).count());
            ns.fetch_add(d, std::memory_order_relaxed);
            for (uint64_t w = worst.load(std::memory_order_relaxed); d > w && !worst.compare_exchange_weak(w, d, std::memory_order_relaxed);) {}
            if ((cnt.fetch_add(1, std::memory_order_relaxed) + 1) % 256 == 0)
              litholattice::logInfo("stall streamed loadColumn: %llu loads, avg %.3f ms, worst %.3f ms (%s)",
                                    (unsigned long long)cnt.load(), ns.load() / 1e6 / double(cnt.load()), worst.load() / 1e6,
                                    save->lockFreeIo() ? "lock-free io" : "old io");
          }
          if (loaded.status == cWorldSave::ColumnLoad::Loaded) {
            if (loaded.column) loaded.column->state.setStage(litholattice::ChunkStage::StagedSim);  // P5: GEN -> STAGED_SIM
            done->push(StreamedColumn{std::move(loaded.column), std::move(loaded.signal), true});
            ready->fetch_add(1, std::memory_order_relaxed);
            guard.success = true;
            return;
          }
          if (loaded.status == cWorldSave::ColumnLoad::Unreadable) {
            // B-M3 fail closed: a saved copy exists but cannot be read. Leave
            // a hole and retry later — NEVER generate over the saved column.
            logError("stream: column %d,%d: saved copy unreadable — left empty, will retry (never generated over)", cx, cz);
            return;
          }
          // NotSaved: fall through to generation, the common case.
        }
        std::unique_ptr<litholattice::ChunkColumn> col = gen->generate(cx, cz);
        if (col) {
          col->state.setStage(litholattice::ChunkStage::StagedSim);  // P5: GEN -> STAGED_SIM
          done->push(StreamedColumn{std::move(col), {}, false});
          ready->fetch_add(1, std::memory_order_relaxed);
          guard.success = true;
        } else {
          logError("stream: column %d,%d failed to generate (left empty, will retry)", cx, cz);
        }
      } catch (const std::exception& e) {
        logError("stream: column %d,%d job threw: %s (left empty, will retry)", cx, cz, e.what());
      } catch (...) {
        logError("stream: column %d,%d job threw a non-std exception (left empty, will retry)", cx, cz);
      }
    };
    // 2026-11-01 (JD LL_LF_GEN=1 + LL_LF_JOBS=1): fresh generation runs on the lock-free queue.
    // A world with a save keeps cJobPool: cWorldSave::loadColumn takes loadMutex_/storesMutex_ --
    // unless the save runs the lock-free I/O path (LL_LF_LOAD / LL_LF_SAVE), which takes neither.
    static const bool kLfGen = [] { const char* e = std::getenv("LL_LF_GEN"); return !(e && e[0] == '0');  /* PlayL: default ON (JD 2026-11-01) */ }();
    // TheReefs 2026-11-14: same 0-worker LF-queue trap as mesh submit. On a
    // 4-thread host cLfJobQueue claims 0 workers and runs every generate/
    // loadColumn inline on the game thread (pumpStream pre/stream 25-55 ms
    // while flying). When the LF queue has no workers, route to the
    // dedicated lock-free mesh pool (already in the 2+1+0 budget). No new
    // locks or threads; results still land on the existing lock-free done_
    // queue and promote at the between-ticks hold.
    if (kLfGen && (!save || save->lockFreeIo()) && litholattice::cLfJobQueue::enabled()
        && litholattice::cLfJobQueue::shared().workers() > 0)
      litholattice::cLfJobQueue::shared().submit(std::move(job));
    else
      pool_.submit(std::move(job));
  }
}

int streamRequestPriority(int dx, int dz, bool diamond) {
  const int e2 = dx * dx + dz * dz;  // < 2^20 for any radius the streamer accepts (<= 512)
  return diamond ? ((std::abs(dx) + std::abs(dz)) << 20) | e2 : e2;
}

void cChunkStreamer::update(int playerCx, int playerCz) {
  admittedThisCycle_ = 0;  // a new frame cycle opens (frame-aware budget)
  if (!gen_) return;
  if (!centreInRange(playerCx, playerCz)) {  // B-L2: refuse out-of-packPos-range centres
    if (!boundWarned_) {
      boundWarned_ = true;
      logError("stream: request centre %d,%d is outside the packPos range (blocks [%d, %d]); "
               "refusing to stream there (cells would alias in packPos-keyed sets)",
               playerCx, playerCz, litholattice::kPackPosCoordMin, litholattice::kPackPosCoordMax);
    }
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::pair<int, uint64_t>> want;  // (distance^2 in chunks, key)
  for (int dz = -radius_; dz <= radius_; ++dz)
    for (int dx = -radius_; dx <= radius_; ++dx) {
      if (diamond_ && std::abs(dx) + std::abs(dz) > radius_) continue;  // diamond footprint (JD 2026-10-23)
      if ((diamond_ ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz))) > admitR_) continue;  // pool-pressure admission
      const uint64_t key = litholattice::ChunkKey::make(playerCx + dx, playerCz + dz);
      if (requestable(key, now)) want.emplace_back(streamRequestPriority(dx, dz, diamond_), key);  // fix 3b: diamond-nearest first
    }
  pumpRequests(std::move(want));
}

void cChunkStreamer::updateAround(int cx, int cz, int radiusChunks) {
  if (!gen_) return;
  if (!centreInRange(cx, cz)) return;  // B-L2: same refusal as update() (logged there / at setup)
  const int r = std::clamp(radiusChunks, 0, radius_);
  const auto now = std::chrono::steady_clock::now();
  std::vector<std::pair<int, uint64_t>> want;
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      if (diamond_ && std::abs(dx) + std::abs(dz) > r) continue;  // diamond footprint (JD 2026-10-23)
      if ((diamond_ ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz))) > admitR_) continue;  // pool-pressure admission
      const uint64_t key = litholattice::ChunkKey::make(cx + dx, cz + dz);
      if (requestable(key, now)) want.emplace_back(streamRequestPriority(dx, dz, diamond_), key);  // fix 3b: diamond-nearest first
    }
  pumpRequests(std::move(want));
}

void cChunkStreamer::requestColumn(int cx, int cz) {
  if (!gen_) return;
  if (!centreInRange(cx, cz)) return;  // B-L2: same refusal as update()
  const uint64_t key = litholattice::ChunkKey::make(cx, cz);
  if (!requestable(key, std::chrono::steady_clock::now())) return;
  pumpRequests({{0, key}});
}

void cChunkStreamer::forgetColumn(int cx, int cz) {
  const uint64_t key = litholattice::ChunkKey::make(cx, cz);
  requested_.erase(key);
  retries_.erase(key);
  std::erase_if(staged_, [&](const StreamedColumn& sc) {
    return sc.column && sc.column->cx == cx && sc.column->cz == cz;
  });
}

std::vector<uint64_t> selectEvictionCandidates(const std::vector<uint64_t>& residentKeys, int centerCx, int centerCz,
                                               int streamRadius, int hysteresisChunks, size_t budget, bool diamond) {
  const int limit = streamRadius + hysteresisChunks;
  std::vector<std::pair<int, uint64_t>> far;  // (distance, key)
  for (uint64_t key : residentKeys) {
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    const int d = diamond ? std::abs(cx - centerCx) + std::abs(cz - centerCz)  // Manhattan (JD 2026-10-23)
                          : std::max(std::abs(cx - centerCx), std::abs(cz - centerCz));
    if (d > limit) far.emplace_back(d, key);
  }
  std::sort(far.begin(), far.end(), [](const auto& a, const auto& b) {
    return a.first != b.first ? a.first > b.first : a.second < b.second;  // farthest first, key tiebreak
  });
  std::vector<uint64_t> out;
  for (const auto& [d, key] : far) {
    if (out.size() >= budget) break;
    out.push_back(key);
  }
  return out;
}

std::vector<StreamedColumn> cChunkStreamer::takeReadyPromote(size_t budget, int centerCx, int centerCz, bool spiralActive,
                                                             size_t* bypassed) {
  // Same staging step as takeReadyUpTo (backoff cleared on success), then the
  // P5 promotion rule: ring-center bypass first, then the per-tick budget.
  std::vector<StreamedColumn> rest = takeReadyUpTo(0);
  (void)rest;  // takeReadyUpTo(0) only stages
  std::vector<StreamedColumn> out;
  size_t nb = 0;
  if (spiralActive) {
    for (auto it = staged_.begin(); it != staged_.end();) {
      if (it->column && litholattice::spawnSpiralBypass(true, it->column->cx, it->column->cz, centerCx, centerCz)) {
        out.push_back(std::move(*it));
        it = staged_.erase(it);
        ++nb;
      } else {
        ++it;
      }
    }
  }
  const size_t n = std::min(budget, staged_.size());
  for (size_t i = 0; i < n; ++i) out.push_back(std::move(staged_[i]));
  staged_.erase(staged_.begin(), staged_.begin() + std::ptrdiff_t(n));
  if (bypassed) *bypassed = nb;
  return out;
}

std::vector<StreamedColumn> cChunkStreamer::takeReady() { return takeReadyUpTo(SIZE_MAX); }

std::vector<StreamedColumn> cChunkStreamer::takeReadyUpTo(size_t max) {
  // Move everything the jobs finished into the main-thread staging buffer
  // first (this is where retry backoff is cleared, exactly as the old
  // unbounded takeReady did), then hand out at most `max`. The remainder
  // stays staged: hasReady() keeps reporting it, so the caller's next pump
  // inserts the next bounded batch — never one giant sim-stopped window.
  std::vector<StreamedColumn> freshCols;
  done_->drainInto(freshCols);
  readyCount_->fetch_sub(int(freshCols.size()), std::memory_order_relaxed);
  for (const StreamedColumn& sc : freshCols)
    if (sc.column) retries_.erase(litholattice::ChunkKey::make(sc.column->cx, sc.column->cz));  // success clears the backoff
  for (auto& sc : freshCols) staged_.push_back(std::move(sc));
  const size_t n = std::min(max, staged_.size());
  std::vector<StreamedColumn> out;
  out.reserve(n);
  for (size_t i = 0; i < n; ++i) out.push_back(std::move(staged_[i]));
  staged_.erase(staged_.begin(), staged_.begin() + std::ptrdiff_t(n));
  return out;
}

}  // namespace lithokraft
