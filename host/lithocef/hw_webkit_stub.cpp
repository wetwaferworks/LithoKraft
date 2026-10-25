// hw_webkit_stub.cpp — IHwWebkit: the honest unsupported WebKit tenant.
// Every verb refuses, counts the refusal, and prints one line (refusals
// are errors, so they print in every build; loud stays loud).
#include "IHwWebkit.h"
#include "hw_log.h"

#include <atomic>
#include <cstdio>

namespace {

std::atomic<uint64_t> g_refused{0};
const char kReason[] = "IHwWebkit is a stub: WebKit tenant not implemented";

int refuse(const char* verb) {
  g_refused.fetch_add(1);
  std::fprintf(stderr, "[hw] webkit %s: %s\n", verb, kReason);
  return LITHOHW_ERR_UNSUPPORTED;
}

LithoHwShell* create(const LithoHwConfig*) { refuse("create"); return nullptr; }
void destroy(LithoHwShell*) {}
int boot_engine(LithoHwShell*, const LithoHwEngineBoot*) { return refuse("boot_engine"); }
int shutdown(LithoHwShell*) { return refuse("shutdown"); }
int acquire_surface(LithoHwShell*, LithoHwSurfaceDesc*) { return refuse("acquire_surface"); }
void release_surface(LithoHwShell*) {}
int set_input_sink(LithoHwShell*, const LithoHwInputSink*) { return refuse("set_input_sink"); }
int present(LithoHwShell*) { return refuse("present"); }
int refresh_millihertz(LithoHwShell*) { return 0; }
uint64_t frames_presented(LithoHwShell*) { return 0; }
uint64_t dropped_inputs(LithoHwShell*) { return 0; }
const char* last_error(LithoHwShell*) { return kReason; }
int query_census(LithoHwShell*, LithoHwCensus*) { return refuse("query_census"); }

const LithoHwApi kApi = {
    LITHOHW_ABI_VERSION,
    create, destroy, boot_engine, shutdown,
    acquire_surface, release_surface,
    set_input_sink,
    present, refresh_millihertz,
    frames_presented, dropped_inputs,
    last_error,
    query_census,
};

} // namespace

extern "C" const LithoHwApi* lithohw_webkit_get_api(void) {
  LITHOHW_TRACE("webkit stub table requested");
  return &kApi;
}
extern "C" uint64_t lithohw_webkit_unsupported_calls(void) { return g_refused.load(); }
extern "C" const char* lithohw_webkit_unsupported_reason(void) { return kReason; }