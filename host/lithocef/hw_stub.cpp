// hwlayer_stub.cpp — the reference/stub LithoHw backend.
//
// Implements the full IHwLayer contract with NO CEF dependency and no
// hardware layer at all: the surface is inert (kind NONE, or whatever
// the config asked for, with null handles), boot records its payload,
// present only counts frames, and input events arrive through
// lithohw_stub_inject_event() (hw_stub.h) — the test-side stand-in
// for a real backend's OS input hooks. Its job is to prove the contract
// compiles, links, and behaves, so the CEF backend can be built against
// a known-good shape.
#include "IHwLayer.h"
#include "hw_stub.h"

#include <cstring>
#include <string>

struct LithoHwShell {
  std::string app_name;
  std::string boot_payload;
  LithoHwSurfaceKind preferred = LITHOHW_SURFACE_NONE;
  int32_t width = 0;
  int32_t height = 0;
  bool headless_ok = true;
  bool booted = false;
  bool surface_held = false;
  LithoHwInputSink sink{nullptr, nullptr};
  bool sink_set = false;
  uint64_t frames = 0;
  uint64_t dropped = 0;
  std::string error;
};

namespace {

int fail(LithoHwShell* s, int code, const char* msg) {
  if (s) s->error = msg;
  return code;
}

LithoHwShell* create(const LithoHwConfig* cfg) {
  if (!cfg || cfg->abi_version != LITHOHW_ABI_VERSION) return nullptr;
  auto* s = new LithoHwShell();
  s->app_name = cfg->application_name ? cfg->application_name : "";
  s->preferred = cfg->preferred_surface;
  s->width = cfg->surface_width > 0 ? cfg->surface_width : 1280;
  s->height = cfg->surface_height > 0 ? cfg->surface_height : 720;
  s->headless_ok = cfg->headless_ok;
  return s;
}

void destroy(LithoHwShell* s) { delete s; }

int boot_engine(LithoHwShell* s, const LithoHwEngineBoot* boot) {
  if (!s) return LITHOHW_ERR_INVALID;
  if (s->booted) return fail(s, LITHOHW_ERR_INVALID, "engine already booted");
  s->boot_payload = (boot && boot->payload_uri) ? boot->payload_uri : "";
  s->booted = true;
  s->error.clear();
  return LITHOHW_OK;
}

int shutdown(LithoHwShell* s) {
  if (!s) return LITHOHW_ERR_INVALID;
  if (!s->booted) return fail(s, LITHOHW_ERR_NOT_BOOTED, "engine not booted");
  s->booted = false;
  return LITHOHW_OK;
}

int acquire_surface(LithoHwShell* s, LithoHwSurfaceDesc* out) {
  if (!s || !out) return LITHOHW_ERR_INVALID;
  // The stub owns no window and no GL: it reports an inert surface.
  // A NONE kind with the requested size is the honest answer; naming a
  // native window or texture here would be a lie the renderer would pay
  // for. (Config may still *ask* for a kind — the stub records the ask
  // and answers NONE unless headless is refused, which is an error.)
  if (s->preferred != LITHOHW_SURFACE_NONE && !s->headless_ok) {
    return fail(s, LITHOHW_ERR_UNSUPPORTED,
                "stub backend has no native window or texture surface");
  }
  out->kind = LITHOHW_SURFACE_NONE;
  out->native_window = nullptr;
  out->gl_context = nullptr;
  out->texture_name = 0;
  out->width = s->width;
  out->height = s->height;
  s->surface_held = true;
  return LITHOHW_OK;
}

void release_surface(LithoHwShell* s) {
  if (s) s->surface_held = false;
}

int set_input_sink(LithoHwShell* s, const LithoHwInputSink* sink) {
  if (!s) return LITHOHW_ERR_INVALID;
  if (!sink) {
    s->sink = LithoHwInputSink{nullptr, nullptr};
    s->sink_set = false;
    return LITHOHW_OK;
  }
  if (!sink->push_event) return fail(s, LITHOHW_ERR_INVALID, "sink has no push_event");
  s->sink = *sink;
  s->sink_set = true;
  return LITHOHW_OK;
}

int present(LithoHwShell* s) {
  if (!s) return LITHOHW_ERR_INVALID;
  if (!s->booted) return fail(s, LITHOHW_ERR_NOT_BOOTED, "present before boot");
  ++s->frames;
  return LITHOHW_OK;
}

int refresh_millihertz(LithoHwShell*) { return 0; /* unknown / unbounded */ }

uint64_t frames_presented(LithoHwShell* s) { return s ? s->frames : 0; }
uint64_t dropped_inputs(LithoHwShell* s) { return s ? s->dropped : 0; }

const char* last_error(LithoHwShell* s) { return s ? s->error.c_str() : "null shell"; }

const LithoHwApi kApi = {
    LITHOHW_ABI_VERSION,
    create, destroy, boot_engine, shutdown,
    acquire_surface, release_surface,
    set_input_sink,
    present, refresh_millihertz,
    frames_presented, dropped_inputs,
    last_error,
};

} // namespace

extern "C" const LithoHwApi* lithohw_get_api(void) { return &kApi; }

extern "C" int lithohw_stub_inject_event(LithoHwShell* s, const LithoHwInputEvent* ev) {
  if (!s || !ev) return 0;
  if (!s->sink_set) {
    ++s->dropped;
    return 0;
  }
  int consumed = s->sink.push_event(s->sink.user, ev);
  if (!consumed) ++s->dropped;
  return consumed;
}

extern "C" const char* lithohw_stub_boot_payload(LithoHwShell* s) {
  return s ? s->boot_payload.c_str() : "";
}
