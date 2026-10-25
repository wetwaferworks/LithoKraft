/* IHwCef.h — the CEF host tenant of the hardware layer (IHwCef).
 *
 * IHwCef is the Chromium Embedded Framework tenant behind the IHwLayer
 * base contract (IHwLayer.h) and the current working path. Everything
 * that is not CEF-specific lives in the base; what stays here is what
 * only a CEF host means:
 *
 *   - payload_uri (LithoHwEngineBoot) names the page or package the CEF
 *     host presents as the engine's first payload (the shipped web
 *     build's page today).
 *   - CEF types (CefApp, CefBrowserView, ...) stay inside the CEF
 *     backend's translation units (host/lithocef/cef); consumers of this
 *     header never see them. Like the base, this file is plain C.
 *   - The CEF backend is a standalone project (cef/CMakeLists.txt,
 *     CEF_ROOT = an extracted CEF binary distribution); the engine build
 *     never grows hooks into it.
 *
 * Runtime boot proof is still pending a real display: CefInitialize
 * stalls under a VM virtual-X/GPU stack. That stall is an environment
 * artifact, not a layer issue; do not change the layer to "fix" it.
 */
#pragma once

#include "IHwLayer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Tenant-specific settings a CEF host reads at startup. Mirrors what the
 * CEF backend's main sets on CefSettings; strings may be NULL (backend
 * default). Carried for hosts that configure the tenant through the
 * layer rather than its command line. */
typedef struct LithoHwCefSettings {
  const char* payload_url;     /* first payload; NULL = backend default */
  const char* root_cache_path; /* absolute; NULL = beside the working dir */
  bool no_sandbox;             /* true where the sandbox helper is not deployed */
} LithoHwCefSettings;

#ifdef __cplusplus
} /* extern "C" */
#endif