// lithocef_smoke.cpp — exercises the ILithoCEF contract against the
// stub backend the way a real host main would: create the shell, wire
// the input sink (playing the engine-side ring writer), boot the
// engine with a payload, acquire the surface, inject input from the
// "hardware layer" (the stub's test hooks), present frames, and shut
// down in order. Prints one OK line; any contract violation fails the
// process with a nonzero exit so ctest can gate on it.
#include <cstdio>
#include <cstring>

#include "ILithoCEF.h"
#include "lithocef_stub.h"

namespace {

struct RingStand {
  int consumed = 0;
  uint64_t last_time_ns = 0;
};

// The engine side of the sink: in production this writes the event into
// the memory-mapped input ring; here it just counts and remembers the
// writer-side timestamp, as a ring writer would.
int push_event(void* user, const LithoCefInputEvent* ev) {
  auto* r = static_cast<RingStand*>(user);
  ++r->consumed;
  r->last_time_ns = ev->time_ns;
  return 1; // consumed
}

bool check(bool cond, const char* what) {
  if (!cond) std::printf("LITHOCEF SMOKE FAIL: %s\n", what);
  return cond;
}

} // namespace

int main() {
  const LithoCefApi* api = lithocef_get_api();
  bool ok = true;
  ok &= check(api != nullptr, "lithocef_get_api returned null");
  ok &= check(api->abi_version == LITHOCEF_ABI_VERSION, "ABI version mismatch");
  if (!ok) return 1;

  LithoCefConfig cfg{};
  cfg.abi_version = LITHOCEF_ABI_VERSION;
  cfg.application_name = "LithoCEF smoke";
  cfg.surface_width = 1280;
  cfg.surface_height = 720;
  cfg.preferred_surface = LITHOCEF_SURFACE_NONE;
  cfg.headless_ok = true;

  LithoCefShell* shell = api->create(&cfg);
  ok &= check(shell != nullptr, "create failed");
  if (!shell) return 1;

  // Present before boot must be refused (call-order discipline).
  ok &= check(api->present(shell) == LITHOCEF_ERR_NOT_BOOTED, "present before boot not refused");

  RingStand ring;
  LithoCefInputSink sink{&ring, &push_event};
  ok &= check(api->set_input_sink(shell, &sink) == LITHOCEF_OK, "set_input_sink failed");

  LithoCefEngineBoot boot{};
  boot.payload_uri = "lithocef://smoke/payload";
  ok &= check(api->boot_engine(shell, &boot) == LITHOCEF_OK, "boot_engine failed");
  ok &= check(api->boot_engine(shell, &boot) == LITHOCEF_ERR_INVALID, "double boot not refused");
  ok &= check(std::strcmp(lithocef_stub_boot_payload(shell), "lithocef://smoke/payload") == 0,
              "boot payload not recorded");

  LithoCefSurfaceDesc surf{};
  ok &= check(api->acquire_surface(shell, &surf) == LITHOCEF_OK, "acquire_surface failed");
  ok &= check(surf.kind == LITHOCEF_SURFACE_NONE, "stub surface is not inert NONE");
  ok &= check(surf.native_window == nullptr && surf.gl_context == nullptr && surf.texture_name == 0,
              "stub surface carries handles it does not own");
  ok &= check(surf.width == 1280 && surf.height == 720, "surface size not the requested one");
  api->release_surface(shell);

  // Hardware-layer input: three events in, all consumed; then detach the
  // sink and confirm one more is honestly counted as dropped.
  for (int i = 0; i < 3; ++i) {
    LithoCefInputEvent ev{};
    ev.kind = LITHOCEF_INPUT_KEY;
    ev.code = 0x41; /* 'A' — the stub is backend-defined by contract */
    ev.value = 1;
    ev.time_ns = 1000 + static_cast<uint64_t>(i);
    ok &= check(lithocef_stub_inject_event(shell, &ev) == 1, "injected event not consumed");
  }
  ok &= check(ring.consumed == 3, "sink did not receive all events");
  ok &= check(ring.last_time_ns == 1002, "writer-side event time not preserved");
  ok &= check(api->set_input_sink(shell, nullptr) == LITHOCEF_OK, "sink detach failed");
  LithoCefInputEvent ev{};
  ev.kind = LITHOCEF_INPUT_KEY;
  ev.value = 1;
  ok &= check(lithocef_stub_inject_event(shell, &ev) == 0, "detached-sink event not dropped");
  ok &= check(api->dropped_inputs(shell) == 1, "drop not counted");

  for (int i = 0; i < 5; ++i)
    ok &= check(api->present(shell) == LITHOCEF_OK, "present failed");
  ok &= check(api->frames_presented(shell) == 5, "frame counter wrong");
  ok &= check(api->refresh_millihertz(shell) == 0, "stub refresh should be 0 (unknown)");

  ok &= check(api->shutdown(shell) == LITHOCEF_OK, "shutdown failed");
  ok &= check(api->shutdown(shell) == LITHOCEF_ERR_NOT_BOOTED, "double shutdown not refused");
  api->destroy(shell);

  if (!ok) return 1;
  std::printf("LITHOCEF STUB SMOKE OK: contract exercised (boot, inert surface, "
              "3 events consumed + 1 drop counted, 5 frames presented)\n");
  return 0;
}
