/* IHwWebkit.h — the WebKit / Safari view tenant of the hardware layer.
 *
 * STUB ONLY (JD, 2026-10-25): the seat for a future WebKit tenant (the
 * door into iOS; test bed JD's iPad 9th gen). Nothing is implemented.
 * lithohw_webkit_get_api() returns a full IHwLayer table whose every
 * verb answers honestly: create() returns NULL, the rest report
 * LITHOHW_ERR_UNSUPPORTED, and each refusal is counted and logged so the
 * stub can never pass for a working tenant.
 */
#pragma once

#include "IHwLayer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The WebKit tenant's table (ABI LITHOHW_ABI_VERSION). Never NULL. */
const LithoHwApi* lithohw_webkit_get_api(void);

/* Number of refused calls on the WebKit stub since process start. */
uint64_t lithohw_webkit_unsupported_calls(void);

/* The fixed reason string every refusal carries. */
const char* lithohw_webkit_unsupported_reason(void);

#ifdef __cplusplus
} /* extern "C" */
#endif