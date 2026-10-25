/* thread_handshake.h — the Thread Handshake (JD, 2026-10-22).
 *
 * A whole thread architecture for the LithoCEF layer, designed by JD
 * in conversation on 2026-10-22 (concepted while mobile, during his
 * building's fire-alarm evacuation; implemented the same day at his
 * direction). In JD's words:
 *
 *   "You have the thread pool up at the top. It dispatches one thread
 *    to the thread dispatch pool down at the bottom and that just
 *    listens for thread needed calls from the engine."
 *
 *   "As soon as [the hardware layer] has the thread pool available,
 *    it gives the thread dispatch pool one thread to assign other
 *    threads where needed."
 *
 *   "The hardware initializes, it figures out how many threads that
 *    it have, and then down at the bottom communicating with the
 *    engine and other systems is a dispatch pool that can call the
 *    top thread pool and request more threads. And it uses one
 *    thread to listen and dispatch the threads."
 *
 * Shape, top to bottom:
 *
 *   hardware layer — at init it takes the thread census; the pool is
 *                    sized from the machine actually present (no
 *                    hard-coded counts: modest hardware, modest pool).
 *   Thread Handshake (this file) —
 *     TOP:    the thread pool (the reservoir of worker threads).
 *     BOTTOM: the thread dispatch pool, facing the engine and other
 *             systems. The pool gives it ONE thread — the listener /
 *             dispatcher — as soon as the pool is available.
 *   engine + other systems — call INTO the dispatch layer with
 *             "thread needed" requests: as many threads as they need
 *             for their kind of work and the hardware's scale.
 *
 * Design laws baked in:
 *   - One listener, one assigner. Every request is heard and granted
 *     by the single dispatcher thread, in submission order, so two
 *     systems can never race each other for the same worker and every
 *     assignment comes out in an order. (Named for the handshake it
 *     is: the Timing Handshake governs WHEN the sim steps; the Thread
 *     Handshake governs WHO does the work. Two handshakes, two layers,
 *     never conflated.)
 *   - Demand decides. No consumer gets threads by default — JD
 *     considered and rejected an automatic one-thread grant to
 *     whatever renderer is running (renderers differ too much).
 *     Consumers ask; the dispatch pool may call the top pool for more.
 *   - All-or-nothing grants. A request for N threads is granted when
 *     N workers are simultaneously free; a partial grant would let
 *     two big consumers starve each other half-armed.
 *
 * Like ILithoCEF.h, this contract is plain C so it can cross DLL
 * borders and be consumed from C or C++. It lives INSIDE the CEF
 * layer: the engine never includes it; the engine (and other systems)
 * are consumers the dispatch layer serves, reached through whatever
 * binding the shell provides.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LITHO_THREAD_HANDSHAKE_ABI_VERSION 1u

typedef struct LithoThreadHandshake LithoThreadHandshake;

/* Proof-of-work handle for one request. Owned by the handshake;
 * valid from request_threads() until the request completes or the
 * handshake is destroyed. Copy the struct by value; do not free it. */
typedef struct LithoThreadTicket {
  uint64_t id; /* 0 = no ticket (request refused synchronously) */
} LithoThreadTicket;

typedef enum LithoThreadRequestState {
  LITHO_THREAD_REQUEST_PENDING = 0,  /* queued or running */
  LITHO_THREAD_REQUEST_DONE = 1,     /* every granted worker finished */
  LITHO_THREAD_REQUEST_CANCELLED = 2 /* handshake destroyed while pending */
} LithoThreadRequestState;

/* The unit of work one granted worker runs. Invoked exactly once per
 * granted thread, on that worker thread, with worker_index in
 * [0, granted). user is the pointer passed to request_threads(). */
typedef void (*LithoThreadJobFn)(void* user, uint32_t worker_index);

typedef struct LithoThreadHandshakeDesc {
  uint32_t abi_version; /* caller sets LITHO_THREAD_HANDSHAKE_ABI_VERSION */
  /* Thread census override: 0 = ask the hardware (the platform's
   * hardware-concurrency count, floored at 1). >0 = size the pool at
   * exactly this many threads (benches, tests, deliberate caps).
   * The dispatcher is one of the pool's threads whenever the census
   * is >= 2, so worker capacity is census-1. On a one-thread machine
   * the dispatcher doubles as the only worker (the design still holds;
   * requests are served serially). */
  uint32_t census_threads;
} LithoThreadHandshakeDesc;

/* Bring the handshake up: take the census, raise the pool, and seat
 * the dispatcher at the bottom. Returns NULL on bad arguments or if
 * the pool cannot be raised. */
LithoThreadHandshake* litho_thread_handshake_create(
    const LithoThreadHandshakeDesc* desc);

/* Orderly stop: running jobs finish; still-queued requests complete
 * as CANCELLED. NULL is accepted (no-op). */
void litho_thread_handshake_destroy(LithoThreadHandshake* hs);

/* The "thread needed" call. A consumer (engine, renderer, any system)
 * asks the dispatch pool for thread_count workers to run job.
 * Returns a ticket id > 0, or 0 when the call itself is invalid
 * (null handshake/job, thread_count == 0, or thread_count greater
 * than the pool's worker capacity — a request that can never be
 * granted is refused at the door, never queued to starve).
 * The job runs on worker threads; the caller keeps user alive until
 * the ticket completes. */
LithoThreadTicket litho_thread_handshake_request_threads(
    LithoThreadHandshake* hs, uint32_t thread_count,
    LithoThreadJobFn job, void* user);

/* Wait until the ticket completes (DONE or CANCELLED). Returns the
 * final state. An unknown ticket id returns CANCELLED. */
LithoThreadRequestState litho_thread_handshake_wait(
    LithoThreadHandshake* hs, LithoThreadTicket ticket);

/* Non-blocking state read (PENDING for unknown-but-plausible ids is
 * impossible: unknown ids read CANCELLED — a ticket the handshake
 * cannot account for is not pending work). */
LithoThreadRequestState litho_thread_handshake_poll(
    LithoThreadHandshake* hs, LithoThreadTicket ticket);

/* The bench record, counters in the ILithoCEF house style. */
uint32_t litho_thread_handshake_census(const LithoThreadHandshake* hs);
uint32_t litho_thread_handshake_worker_capacity(const LithoThreadHandshake* hs);
uint32_t litho_thread_handshake_workers_busy(const LithoThreadHandshake* hs);
uint64_t litho_thread_handshake_requests_served(const LithoThreadHandshake* hs);
uint64_t litho_thread_handshake_threads_dispatched(const LithoThreadHandshake* hs);

#ifdef __cplusplus
} /* extern "C" */
#endif
