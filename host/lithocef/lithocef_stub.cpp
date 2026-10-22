// lithocef_stub.cpp — the reference/stub LithoCEF backend.
//
// Implements the full ILithoCEF contract with NO CEF dependency and no
// hardware layer at all: the surface is inert (kind NONE, or whatever
// the config asked for, with null handles), boot records its payload,
// present only counts frames, and input events arrive through
// lithocef_stub_inject_event() (lithocef_stub.h) — the test-side stand-in
// for a real backend's OS input hooks. Its job is to prove the contract
// compiles, links, and behaves, so the CEF backend can be built against
// a known-good shape.
#include "ILithoCEF.h"
#include "lithocef_stub.h"

#include <cstring>
#include <string>

struct LithoCefShell {
  std::string app_name;
  std::string boot_payload;
  LithoCefSurfaceKind preferred = LITHOCEF_SURFACE_NONE;
  int32_t width = 0;
  int32_t height = 0;
  bool headless_ok = true;
  bool booted = false;
  bool surface_held = false;
  LithoCefInputSink sink{nullptr, nullptr};
  bool sink_set = false;
  uint64_t frames = 0;
  uint64_t dropped = 0;
  std::string error;
};

namespace {

int fail(LithoCefShell* s, int code, const char* msg) {
  if (s) s->error = msg;
  return code;
}

LithoCefShell* create(const LithoCefConfig* cfg) {
  if (!cfg || cfg->abi_version != LITHOCEF_ABI_VERSION) return nullptr;
  auto* s = new LithoCefShell();
  s->app_name = cfg->application_name ? cfg->application_name : "";
  s->preferred = cfg->preferred_surface;
  s->width = cfg->surface_width > 0 ? cfg->surface_width : 1280;
  s->height = cfg->surface_height > 0 ? cfg->surface_height : 720;
  s->headless_ok = cfg->headless_ok;
  return s;
}

void destroy(LithoCefShell* s) { delete s; }

int boot_engine(LithoCefShell* s, const LithoCefEngineBoot* boot) {
  if (!s) return LITHOCEF_ERR_INVALID;
  if (s->booted) return fail(s, LITHOCEF_ERR_INVALID, "engine already booted");
  s->boot_payload = (boot && boot->payload_uri) ? boot->payload_uri : "";
  s->booted = true;
  s->error.clear();
  return LITHOCEF_OK;
}

int shutdown(LithoCefShell* s) {
  if (!s) return LITHOCEF_ERR_INVALID;
  if (!s->booted) return fail(s, LITHOCEF_ERR_NOT_BOOTED, "engine not booted");
  s->booted = false;
  return LITHOCEF_OK;
}

int acquire_surface(LithoCefShell* s, LithoCefSurfaceDesc* out) {
  if (!s || !out) return LITHOCEF_ERR_INVALID;
  // The stub owns no window and no GL: it reports an inert surface.
  // A NONE kind with the requested size is the honest answer; naming a
  // native window or texture here would be a lie the renderer would pay
  // for. (Config may still *ask* for a kind — the stub records the ask
  // and answers NONE unless headless is refused, which is an error.)
  if (s->preferred != LITHOCEF_SURFACE_NONE && !s->headless_ok) {
    return fail(s, LITHOCEF_ERR_UNSUPPORTED,
                "stub backend has no native window or texture surface");
  }
  out->kind = LITHOCEF_SURFACE_NONE;
  out->native_window = nullptr;
  out->gl_context = nullptr;
  out->texture_name = 0;
  out->width = s->width;
  out->height = s->height;
  s->surface_held = true;
  return LITHOCEF_OK;
}

void release_surface(LithoCefShell* s) {
  if (s) s->surface_held = false;
}

int set_input_sink(LithoCefShell* s, const LithoCefInputSink* sink) {
  if (!s) return LITHOCEF_ERR_INVALID;
  if (!sink) {
    s->sink = LithoCefInputSink{nullptr, nullptr};
    s->sink_set = false;
    return LITHOCEF_OK;
  }
  if (!sink->push_event) return fail(s, LITHOCEF_ERR_INVALID, "sink has no push_event");
  s->sink = *sink;
  s->sink_set = true;
  return LITHOCEF_OK;
}

int present(LithoCefShell* s) {
  if (!s) return LITHOCEF_ERR_INVALID;
  if (!s->booted) return fail(s, LITHOCEF_ERR_NOT_BOOTED, "present before boot");
  ++s->frames;
  return LITHOCEF_OK;
}

int refresh_millihertz(LithoCefShell*) { return 0; /* unknown / unbounded */ }

uint64_t frames_presented(LithoCefShell* s) { return s ? s->frames : 0; }
uint64_t dropped_inputs(LithoCefShell* s) { return s ? s->dropped : 0; }

const char* last_error(LithoCefShell* s) { return s ? s->error.c_str() : "null shell"; }

const LithoCefApi kApi = {
    LITHOCEF_ABI_VERSION,
    create, destroy, boot_engine, shutdown,
    acquire_surface, release_surface,
    set_input_sink,
    present, refresh_millihertz,
    frames_presented, dropped_inputs,
    last_error,
};

} // namespace

extern "C" const LithoCefApi* lithocef_get_api(void) { return &kApi; }

extern "C" int lithocef_stub_inject_event(LithoCefShell* s, const LithoCefInputEvent* ev) {
  if (!s || !ev) return 0;
  if (!s->sink_set) {
    ++s->dropped;
    return 0;
  }
  int consumed = s->sink.push_event(s->sink.user, ev);
  if (!consumed) ++s->dropped;
  return consumed;
}

extern "C" const char* lithocef_stub_boot_payload(LithoCefShell* s) {
  return s ? s->boot_payload.c_str() : "";
}
