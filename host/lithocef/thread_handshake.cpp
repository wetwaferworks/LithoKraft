// thread_handshake.cpp — the Thread Handshake (JD, 2026-10-22).
//
// Implements thread_handshake.h: a thread pool at the top of the CEF
// layer, a dispatch pool at the bottom with ONE listener thread the
// pool gives it, and "thread needed" calls from the engine and other
// systems, granted all-or-nothing in submission order. See the header
// for the design in JD's words. Pure std::thread — the CEF layer
// speaks to the hardware layer, it does not borrow the engine's.
#include "thread_handshake.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct Request {
  uint64_t id = 0;
  uint32_t count = 0;
  LithoThreadJobFn job = nullptr;
  void* user = nullptr;
  LithoThreadRequestState state = LITHO_THREAD_REQUEST_PENDING;
  uint32_t finished = 0; /* workers that completed this request */
};

struct WorkerSlot {
  std::thread thread;
  Request* req = nullptr; /* assignment, guarded by the handshake mutex */
  uint32_t index = 0;
  bool has_work = false;
};

} // namespace

struct LithoThreadHandshake {
  uint32_t census = 0;
  uint32_t worker_capacity = 0;

  std::mutex mu;
  std::condition_variable cv; /* every state change signals here */
  std::deque<std::shared_ptr<Request>> queue;
  std::unordered_map<uint64_t, std::shared_ptr<Request>> tickets;
  uint64_t next_id = 1;
  bool stopping = false;

  std::vector<std::unique_ptr<WorkerSlot>> workers;
  std::thread dispatcher;
  uint32_t busy = 0; /* workers currently running a job */
  uint64_t served = 0;
  uint64_t dispatched = 0;

  /* One worker's life: sleep until the dispatcher assigns, run the
   * job WITHOUT the lock, report back, sleep again. Workers never
   * decide anything — assignment is the dispatcher's alone. */
  void worker_loop(WorkerSlot* slot) {
    std::unique_lock<std::mutex> lk(mu);
    for (;;) {
      cv.wait(lk, [&] { return slot->has_work || stopping; });
      if (!slot->has_work) return; /* stopping with nothing assigned */
      Request* req = slot->req;
      uint32_t index = slot->index;
      slot->has_work = false;
      ++busy;
      lk.unlock();
      req->job(req->user, index);
      lk.lock();
      --busy;
      ++req->finished;
      cv.notify_all();
    }
  }

  /* The listener. The pool gave the dispatch pool this one thread;
   * it hears every "thread needed" call in submission order and is
   * the only assigner in the building. On a one-thread machine the
   * dispatcher doubles as the only worker and runs jobs itself. */
  void dispatcher_loop() {
    std::unique_lock<std::mutex> lk(mu);
    for (;;) {
      cv.wait(lk, [&] {
        return stopping || (!queue.empty() && free_workers_locked() >= queue.front()->count);
      });
      if (stopping) {
        /* Queued requests never ran: cancel them honestly. A request
         * already granted is not in the queue (see below) and has
         * finished by the time destroy() joins this thread's caller
         * path — destroy waits for in-flight work before stopping. */
        for (auto& r : queue) r->state = LITHO_THREAD_REQUEST_CANCELLED;
        queue.clear();
        cv.notify_all();
        return;
      }
      if (queue.empty()) continue;
      auto req = queue.front();
      queue.pop_front();

      if (workers.empty()) {
        /* census == 1: run it here, serially, indices in order. */
        busy = req->count;
        dispatched += req->count;
        lk.unlock();
        for (uint32_t i = 0; i < req->count; ++i) req->job(req->user, i);
        lk.lock();
        busy = 0;
        req->finished = req->count;
      } else {
        uint32_t idx = 0;
        for (auto& slot : workers) {
          if (idx >= req->count) break;
          if (!slot->has_work) {
            slot->req = req.get();
            slot->index = idx++;
            slot->has_work = true;
          }
        }
        dispatched += req->count;
        cv.notify_all();
        cv.wait(lk, [&] { return req->finished >= req->count; });
      }
      req->state = LITHO_THREAD_REQUEST_DONE;
      ++served;
      cv.notify_all();
    }
  }

  uint32_t free_workers_locked() const {
    if (workers.empty()) return busy == 0 ? worker_capacity : 0;
    /* A worker is unavailable from assignment until its job reports
     * back: `assigned` covers set-but-not-picked-up, `busy` covers
     * picked-up-and-running. (The dispatcher grants one request at a
     * time and waits for it, so both are 0 whenever a grant is
     * considered — the count stays honest regardless.) */
    uint32_t assigned = 0;
    for (auto& slot : workers)
      if (slot->has_work) ++assigned;
    return worker_capacity - busy - assigned;
  }
};

extern "C" {

LithoThreadHandshake* litho_thread_handshake_create(const LithoThreadHandshakeDesc* desc) {
  if (!desc || desc->abi_version != LITHO_THREAD_HANDSHAKE_ABI_VERSION) return nullptr;
  uint32_t census = desc->census_threads;
  if (census == 0) {
    census = std::thread::hardware_concurrency();
    if (census == 0) census = 1; /* hardware declined to say: floor of 1 */
  }
  if (census > 512) census = 512; /* a pool is a reservoir, not a flood */

  auto* hs = new LithoThreadHandshake();
  hs->census = census;
  hs->worker_capacity = census >= 2 ? census - 1 : 1;

  if (census >= 2) {
    hs->workers.reserve(hs->worker_capacity);
    for (uint32_t i = 0; i < hs->worker_capacity; ++i) {
      auto slot = std::make_unique<WorkerSlot>();
      WorkerSlot* raw = slot.get();
      hs->workers.push_back(std::move(slot));
      raw->thread = std::thread([hs, raw] { hs->worker_loop(raw); });
    }
  }
  hs->dispatcher = std::thread([hs] { hs->dispatcher_loop(); });
  return hs;
}

void litho_thread_handshake_destroy(LithoThreadHandshake* hs) {
  if (!hs) return;
  {
    std::lock_guard<std::mutex> lk(hs->mu);
    hs->stopping = true;
  }
  hs->cv.notify_all();
  /* The dispatcher finishes any in-flight grant before cancelling
   * the queue: its grant wait is on job completion, not on stopping,
   * so joining here honors "running jobs finish". */
  if (hs->dispatcher.joinable()) hs->dispatcher.join();
  hs->cv.notify_all();
  for (auto& slot : hs->workers)
    if (slot->thread.joinable()) slot->thread.join();
  delete hs;
}

LithoThreadTicket litho_thread_handshake_request_threads(
    LithoThreadHandshake* hs, uint32_t thread_count, LithoThreadJobFn job, void* user) {
  LithoThreadTicket none{0};
  if (!hs || !job || thread_count == 0) return none;
  std::lock_guard<std::mutex> lk(hs->mu);
  if (hs->stopping) return none;
  if (thread_count > hs->worker_capacity) return none; /* can never be granted */
  auto req = std::make_shared<Request>();
  req->id = hs->next_id++;
  req->count = thread_count;
  req->job = job;
  req->user = user;
  hs->queue.push_back(req);
  hs->tickets[req->id] = req;
  hs->cv.notify_all();
  return LithoThreadTicket{req->id};
}

LithoThreadRequestState litho_thread_handshake_wait(LithoThreadHandshake* hs,
                                                    LithoThreadTicket ticket) {
  if (!hs) return LITHO_THREAD_REQUEST_CANCELLED;
  std::unique_lock<std::mutex> lk(hs->mu);
  auto it = hs->tickets.find(ticket.id);
  if (it == hs->tickets.end()) return LITHO_THREAD_REQUEST_CANCELLED;
  auto req = it->second;
  hs->cv.wait(lk, [&] { return req->state != LITHO_THREAD_REQUEST_PENDING; });
  return req->state;
}

LithoThreadRequestState litho_thread_handshake_poll(LithoThreadHandshake* hs,
                                                    LithoThreadTicket ticket) {
  if (!hs) return LITHO_THREAD_REQUEST_CANCELLED;
  std::lock_guard<std::mutex> lk(hs->mu);
  auto it = hs->tickets.find(ticket.id);
  if (it == hs->tickets.end()) return LITHO_THREAD_REQUEST_CANCELLED;
  return it->second->state;
}

uint32_t litho_thread_handshake_census(const LithoThreadHandshake* hs) {
  return hs ? hs->census : 0;
}
uint32_t litho_thread_handshake_worker_capacity(const LithoThreadHandshake* hs) {
  return hs ? hs->worker_capacity : 0;
}
uint32_t litho_thread_handshake_workers_busy(const LithoThreadHandshake* hs) {
  if (!hs) return 0;
  std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(hs->mu));
  return hs->busy;
}
uint64_t litho_thread_handshake_requests_served(const LithoThreadHandshake* hs) {
  if (!hs) return 0;
  std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(hs->mu));
  return hs->served;
}
uint64_t litho_thread_handshake_threads_dispatched(const LithoThreadHandshake* hs) {
  if (!hs) return 0;
  std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(hs->mu));
  return hs->dispatched;
}

} /* extern "C" */
