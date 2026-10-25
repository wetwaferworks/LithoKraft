// hwlayer_tenants_smoke.cpp — IHwLayer phase 2: the tenant seats.
// The WebKit stub refuses honestly (NULL shell, UNSUPPORTED verbs,
// counted refusals, fixed reason); the stub answers the census through
// the same base table; IHwGpu and IHwCef headers compile beside them;
// the trace gate reports its build state.
#include <cstdio>
#include <cstring>

#include "IHwLayer.h"
#include "IHwCef.h"
#include "IHwGpu.h"
#include "IHwWebkit.h"
#include "hw_log.h"

namespace {
bool check(bool cond, const char* what) {
  if (!cond) std::printf("HWLAYER TENANTS SMOKE FAIL: %s\n", what);
  return cond;
}
} // namespace

int main() {
  bool ok = true;
  LithoHwConfig cfg{};
  cfg.abi_version = LITHOHW_ABI_VERSION;
  cfg.headless_ok = true;

  const LithoHwApi* wk = lithohw_webkit_get_api();
  ok &= check(wk != nullptr && wk->abi_version == LITHOHW_ABI_VERSION, "webkit table missing or wrong ABI");
  const uint64_t before = lithohw_webkit_unsupported_calls();
  ok &= check(wk->create(&cfg) == nullptr, "webkit create pretended to work");
  ok &= check(wk->boot_engine(nullptr, nullptr) == LITHOHW_ERR_UNSUPPORTED, "webkit boot not UNSUPPORTED");
  ok &= check(wk->present(nullptr) == LITHOHW_ERR_UNSUPPORTED, "webkit present not UNSUPPORTED");
  LithoHwCensus wc{};
  ok &= check(wk->query_census(nullptr, &wc) == LITHOHW_ERR_UNSUPPORTED, "webkit census not UNSUPPORTED");
  ok &= check(lithohw_webkit_unsupported_calls() - before == 4, "webkit refusals not counted");
  ok &= check(std::strstr(wk->last_error(nullptr), "stub") != nullptr, "webkit reason not honest");

  const LithoHwApi* api = lithohw_get_api();
  ok &= check(api->abi_version == LITHOHW_ABI_VERSION && api->query_census != nullptr, "stub ABI 2 census missing");
  LithoHwShell* s = api->create(&cfg);
  ok &= check(s != nullptr, "stub create failed");
  LithoHwCensus c{};
  c.gpu_reachable = 7;
  ok &= check(api->query_census(s, &c) == LITHOHW_OK, "stub census failed");
  ok &= check(c.logical_threads >= 1, "census thread count missing");
  ok &= check(c.gpu_reachable == 0 && c.refresh_millihertz == 0 && c.physical_memory_mb == 0, "stub census guessed");
  ok &= check(api->query_census(s, nullptr) == LITHOHW_ERR_INVALID, "null census not refused");
  api->destroy(s);

  LithoHwGpuCaps caps{};
  caps.abi_version = LITHOHW_GPU_ABI_VERSION;
  LithoHwCefSettings cef{};
  (void)caps; (void)cef;

  if (!ok) return 1;
  std::printf("HWLAYER TENANTS SMOKE OK: webkit stub refused 4/4 honestly, stub census threads=%u, "
              "IHwCef/IHwGpu seated, trace %s\n", c.logical_threads, LITHOHW_TRACE_ENABLED ? "ON" : "OFF");
  return 0;
}