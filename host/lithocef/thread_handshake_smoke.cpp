// thread_handshake_smoke.cpp — exercises the Thread Handshake
// (JD, 2026-10-22) the way its consumers will: take the census,
// raise the pool, seat the dispatcher, and call "thread needed".
// Proves, by counts:
//   - the hardware census sizes the pool (override for determinism);
//   - the dispatcher is one of the pool's threads (capacity = census-1);
//   - a request runs its job once per granted worker, on workers;
//   - grants are all-or-nothing and in order (a queued request does
//     not start until the request ahead of it fully releases);
//   - impossible requests are refused at the door (ticket 0);
//   - destroy finishes running jobs and cancels queued ones honestly.
// Every job self-releases on a deadline so this binary always exits;
// run it under a timeout in the harness regardless (a hang is a fail).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "thread_handshake.h"

namespace {

bool check(bool cond, const char* what) {
  if (!cond) std::printf("THREAD HANDSHAKE SMOKE FAIL: %s\n", what);
  return cond;
}

struct CountJob {
  std::atomic<int> invocations{0};
  std::thread::id ids[8];
};
void count_job(void* user, uint32_t worker_index) {
  auto* j = static_cast<CountJob*>(user);
  if (worker_index < 8) j->ids[worker_index] = std::this_thread::get_id();
  j->invocations.fetch_add(1);
}

/* A job that holds its workers until released (or a 10 s deadline,
 * so a broken build fails instead of hanging the gate). */
struct HoldJob {
  std::atomic<int> started{0};
  std::atomic<bool> release{false};
  std::atomic<bool>* all_done_flag = nullptr; /* set by last worker out */
  int expected = 0;
};
void hold_job(void* user, uint32_t) {
  auto* j = static_cast<HoldJob*>(user);
  int now = j->started.fetch_add(1) + 1;
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!j->release.load() && std::chrono::steady_clock::now() < deadline)
    std::this_thread::yield();
  if (j->all_done_flag && now == j->expected) j->all_done_flag->store(true);
}

bool wait_until(const std::atomic<int>& a, int target, int millis) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(millis);
  while (std::chrono::steady_clock::now() < deadline) {
    if (a.load() >= target) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return a.load() >= target;
}

LithoThreadHandshake* make(uint32_t census) {
  LithoThreadHandshakeDesc d{};
  d.abi_version = LITHO_THREAD_HANDSHAKE_ABI_VERSION;
  d.census_threads = census;
  return litho_thread_handshake_create(&d);
}

} // namespace

int main() {
  bool ok = true;

  /* Census: default asks the hardware; the dispatcher takes a seat. */
  {
    LithoThreadHandshake* hs = make(0);
    ok &= check(hs != nullptr, "create (hardware census) failed");
    uint32_t hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    ok &= check(litho_thread_handshake_census(hs) == hw, "census != hardware count");
    uint32_t cap = litho_thread_handshake_worker_capacity(hs);
    ok &= check(cap == (hw >= 2 ? hw - 1 : 1), "worker capacity != census-1 (dispatcher seat)");
    litho_thread_handshake_destroy(hs);
  }

  /* Deterministic pool: census 4 -> 3 workers behind one dispatcher. */
  LithoThreadHandshake* hs = make(4);
  ok &= check(hs != nullptr, "create (census 4) failed");
  ok &= check(litho_thread_handshake_census(hs) == 4, "census override ignored");
  ok &= check(litho_thread_handshake_worker_capacity(hs) == 3, "capacity != 3");

  /* Refused at the door: zero, over-capacity, and jobless requests. */
  ok &= check(litho_thread_handshake_request_threads(hs, 0, &count_job, nullptr).id == 0,
              "zero-thread request not refused");
  ok &= check(litho_thread_handshake_request_threads(hs, 4, &count_job, nullptr).id == 0,
              "over-capacity request not refused");
  ok &= check(litho_thread_handshake_request_threads(hs, 1, nullptr, nullptr).id == 0,
              "jobless request not refused");

  /* One request, two workers: the job runs once per worker, on
   * worker threads (never the caller's), and the counters agree. */
  {
    CountJob job;
    auto t = litho_thread_handshake_request_threads(hs, 2, &count_job, &job);
    ok &= check(t.id != 0, "request for 2 refused");
    ok &= check(litho_thread_handshake_wait(hs, t) == LITHO_THREAD_REQUEST_DONE,
                "request for 2 did not complete DONE");
    ok &= check(job.invocations.load() == 2, "job invocations != granted workers");
    ok &= check(job.ids[0] != std::this_thread::get_id() &&
                    job.ids[1] != std::this_thread::get_id(),
                "job ran on the caller's thread");
    ok &= check(job.ids[0] != job.ids[1], "two workers, one thread");
    ok &= check(litho_thread_handshake_requests_served(hs) == 1, "served counter != 1");
    ok &= check(litho_thread_handshake_threads_dispatched(hs) == 2, "dispatched counter != 2");
    ok &= check(litho_thread_handshake_workers_busy(hs) == 0, "workers still busy after DONE");
  }

  /* All-or-nothing, in order: A holds all 3 workers; B (1 worker)
   * must not start — not even partially — until A fully releases. */
  {
    HoldJob a;
    a.expected = 3;
    std::atomic<bool> a_done{false};
    a.all_done_flag = &a_done;
    auto ta = litho_thread_handshake_request_threads(hs, 3, &hold_job, &a);
    ok &= check(ta.id != 0, "hold request A refused");
    ok &= check(wait_until(a.started, 3, 5000), "A did not take all 3 workers");

    struct Probe {
      std::atomic<bool>* a_done;
      std::atomic<bool> ran_without_a_done{false};
    } probe{&a_done};
    auto probe_job = [](void* user, uint32_t) {
      auto* p = static_cast<Probe*>(user);
      if (!p->a_done->load()) p->ran_without_a_done.store(true);
    };
    auto tb = litho_thread_handshake_request_threads(hs, 1, probe_job, &probe);
    ok &= check(tb.id != 0, "queued request B refused");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ok &= check(litho_thread_handshake_poll(hs, tb) == LITHO_THREAD_REQUEST_PENDING,
                "B started while A held every worker");
    a.release.store(true);
    ok &= check(litho_thread_handshake_wait(hs, ta) == LITHO_THREAD_REQUEST_DONE, "A not DONE");
    ok &= check(litho_thread_handshake_wait(hs, tb) == LITHO_THREAD_REQUEST_DONE, "B not DONE");
    ok &= check(!probe.ran_without_a_done.load(), "B ran before A fully released");
  }

  /* Destroy: a running job finishes; a queued request is cancelled,
   * and a thread waiting on it hears CANCELLED, not silence. */
  {
    LithoThreadHandshake* hs2 = make(2); /* capacity 1 */
    HoldJob a;
    a.expected = 1;
    auto ta = litho_thread_handshake_request_threads(hs2, 1, &hold_job, &a);
    ok &= check(ta.id != 0, "cancel-test request A refused");
    ok &= check(wait_until(a.started, 1, 5000), "cancel-test A never started");
    CountJob never;
    auto tb = litho_thread_handshake_request_threads(hs2, 1, &count_job, &never);
    ok &= check(tb.id != 0, "cancel-test request B refused");
    std::atomic<int> b_result{-1};
    std::thread waiter([&] {
      b_result.store(static_cast<int>(litho_thread_handshake_wait(hs2, tb)));
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::thread releaser([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      a.release.store(true);
    });
    litho_thread_handshake_destroy(hs2); /* blocks until A finishes */
    waiter.join();
    releaser.join();
    ok &= check(b_result.load() == LITHO_THREAD_REQUEST_CANCELLED,
                "queued request not CANCELLED at destroy");
    ok &= check(never.invocations.load() == 0, "cancelled request's job ran");
    ok &= check(a.started.load() == 1, "running job did not finish at destroy");
  }

  /* One-thread machine: the dispatcher doubles as the only worker;
   * the handshake still holds, serially. */
  {
    LithoThreadHandshake* hs1 = make(1);
    ok &= check(hs1 != nullptr, "create (census 1) failed");
    ok &= check(litho_thread_handshake_worker_capacity(hs1) == 1, "census 1 capacity != 1");
    CountJob job;
    auto t1 = litho_thread_handshake_request_threads(hs1, 1, &count_job, &job);
    auto t2 = litho_thread_handshake_request_threads(hs1, 1, &count_job, &job);
    ok &= check(litho_thread_handshake_wait(hs1, t1) == LITHO_THREAD_REQUEST_DONE, "census 1 first request failed");
    ok &= check(litho_thread_handshake_wait(hs1, t2) == LITHO_THREAD_REQUEST_DONE, "census 1 second request failed");
    ok &= check(job.invocations.load() == 2, "census 1 invocations != 2");
    litho_thread_handshake_destroy(hs1);
  }

  litho_thread_handshake_destroy(hs);

  if (!ok) return 1;
  std::printf("THREAD HANDSHAKE SMOKE OK: census-sized pool, dispatcher seated, "
              "grants all-or-nothing in order, door refusals, cancel-on-destroy, "
              "one-thread machine served serially\n");
  return 0;
}
