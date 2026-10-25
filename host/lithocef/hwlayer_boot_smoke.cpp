// hwlayer_boot_smoke.cpp — the IHwLayer lifecycle the first smoke does
// not reach, plus the two JD designs composing: a shell that boots,
// serves, shuts down, and boots AGAIN (the contract's re-boot clause),
// with a finite engine-side ring (drops counted at capacity, not only
// on a detached sink), and the Thread Handshake answering the engine's
// "thread needed" call during the first boot. Counters are pinned in
// the house style: frames_presented is a shell-lifetime counter, so it
// accumulates across boots (3 + 2 = 5 below) — that is the stub's
// documented semantics, asserted here so a silent change fails loudly.
#include <atomic>
#include <cstdio>
#include <cstring>

#include "IHwLayer.h"
#include "hw_stub.h"
#include "thread_handshake.h"

namespace {

struct FiniteRing {
  int capacity = 3;
  int consumed = 0;
};

int push_event(void* user, const LithoHwInputEvent* ev) {
  auto* r = static_cast<FiniteRing*>(user);
  (void)ev;
  if (r->consumed >= r->capacity) return 0; // ring full: honest refusal
  ++r->consumed;
  return 1;
}

struct JobCount { std::atomic<int> ran{0}; };
void count_job(void* user, uint32_t worker_index) {
  (void)worker_index;
  static_cast<JobCount*>(user)->ran.fetch_add(1);
}

bool check(bool cond, const char* what) {
  if (!cond) std::printf("LITHOHW BOOT SMOKE FAIL: %s\n", what);
  return cond;
}

} // namespace

int main() {
  const LithoHwApi* api = lithohw_get_api();
  bool ok = true;
  ok &= check(api != nullptr, "lithohw_get_api returned null");
  if (!ok) return 1;

  LithoHwConfig cfg{};
  cfg.abi_version = LITHOHW_ABI_VERSION;
  cfg.application_name = "LithoHw boot smoke";
  cfg.surface_width = 1280;
  cfg.surface_height = 720;
  cfg.preferred_surface = LITHOHW_SURFACE_NONE;
  cfg.headless_ok = true;

  LithoHwShell* shell = api->create(&cfg);
  ok &= check(shell != nullptr, "create failed");
  if (!shell) return 1;

  FiniteRing ring;
  LithoHwInputSink sink{&ring, &push_event};
  ok &= check(api->set_input_sink(shell, &sink) == LITHOHW_OK, "set_input_sink failed");

  // ---- Boot #1 ----
  LithoHwEngineBoot boot1{};
  boot1.payload_uri = "lithohw://boot/first";
  ok &= check(api->boot_engine(shell, &boot1) == LITHOHW_OK, "boot #1 failed");
  ok &= check(std::strcmp(lithohw_stub_boot_payload(shell), "lithohw://boot/first") == 0,
              "boot #1 payload not recorded");

  // Finite ring: 5 events in, 3 consumed, 2 refused and counted as drops.
  for (int i = 0; i < 5; ++i) {
    LithoHwInputEvent ev{};
    ev.kind = LITHOHW_INPUT_KEY;
    ev.code = 0x42;
    ev.value = 1;
    ev.time_ns = 2000 + static_cast<uint64_t>(i);
    int consumed = lithohw_stub_inject_event(shell, &ev);
    ok &= check(consumed == (i < 3 ? 1 : 0), "ring capacity result wrong");
  }
  ok &= check(ring.consumed == 3, "ring did not stop at capacity");
  ok &= check(api->dropped_inputs(shell) == 2, "capacity drops not counted");

  // The engine's "thread needed" call, answered by the dispatch pool.
  LithoThreadHandshakeDesc hdesc{};
  hdesc.abi_version = LITHO_THREAD_HANDSHAKE_ABI_VERSION;
  hdesc.census_threads = 4; // capacity = census - 1 = 3
  LithoThreadHandshake* hs = litho_thread_handshake_create(&hdesc);
  ok &= check(hs != nullptr, "handshake create failed");
  if (hs) {
    ok &= check(litho_thread_handshake_worker_capacity(hs) == 3, "worker capacity wrong");
    JobCount jobs;
    LithoThreadTicket t = litho_thread_handshake_request_threads(hs, 2, &count_job, &jobs);
    ok &= check(t.id != 0, "thread request refused at the door");
    ok &= check(litho_thread_handshake_wait(hs, t) == LITHO_THREAD_REQUEST_DONE,
                "thread request did not complete");
    ok &= check(jobs.ran.load() == 2, "job did not run once per granted worker");
    LithoThreadTicket too_big = litho_thread_handshake_request_threads(hs, 4, &count_job, &jobs);
    ok &= check(too_big.id == 0, "over-capacity request not refused at the door");
    ok &= check(litho_thread_handshake_requests_served(hs) == 1, "requests_served wrong");
    ok &= check(litho_thread_handshake_threads_dispatched(hs) == 2, "threads_dispatched wrong");
  }

  for (int i = 0; i < 3; ++i)
    ok &= check(api->present(shell) == LITHOHW_OK, "present (boot #1) failed");
  ok &= check(api->frames_presented(shell) == 3, "frames after boot #1 wrong");
  ok &= check(api->shutdown(shell) == LITHOHW_OK, "shutdown #1 failed");
  ok &= check(api->present(shell) == LITHOHW_ERR_NOT_BOOTED,
              "present after shutdown not refused");

  // ---- Boot #2: the contract's re-boot clause ----
  LithoHwEngineBoot boot2{};
  boot2.payload_uri = "lithohw://boot/second";
  ok &= check(api->boot_engine(shell, &boot2) == LITHOHW_OK, "re-boot failed");
  ok &= check(std::strcmp(lithohw_stub_boot_payload(shell), "lithohw://boot/second") == 0,
              "boot #2 payload not recorded");
  for (int i = 0; i < 2; ++i)
    ok &= check(api->present(shell) == LITHOHW_OK, "present (boot #2) failed");
  ok &= check(api->frames_presented(shell) == 5,
              "frames_presented is not the shell-lifetime counter");
  ok &= check(api->shutdown(shell) == LITHOHW_OK, "shutdown #2 failed");

  if (hs) litho_thread_handshake_destroy(hs);
  api->destroy(shell);

  if (!ok) return 1;
  std::printf("LITHOHW BOOT SMOKE OK: re-boot after shutdown, finite ring "
              "(3 consumed + 2 drops counted), thread handshake served the "
              "engine during boot (2 workers, over-capacity refused), "
              "frames cumulative 3+2=5\n");
  return 0;
}
