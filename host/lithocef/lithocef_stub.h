/* lithocef_stub.h — test-side surface of the STUB backend only.
 *
 * The stub has no hardware layer under it, so tests (and benches) play
 * the hardware layer's part: they inject input events at the same point
 * a real backend's OS hooks would deliver them, and the stub forwards
 * them to the registered input sink exactly as a CEF backend would.
 * Nothing in this header is part of ILithoCEF; production consumers use
 * ILithoCEF.h alone.
 */
#pragma once

#include "ILithoCEF.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Deliver one event as if the hardware layer produced it. Returns 1
 * when the registered sink consumed it, 0 when it was dropped (no sink
 * registered, or the sink refused — counted in dropped_inputs()). */
int lithocef_stub_inject_event(LithoCefShell* shell, const LithoCefInputEvent* ev);

/* The payload URI recorded by the most recent boot_engine() call
 * ("" when booted with a NULL payload). Owned by the shell. */
const char* lithocef_stub_boot_payload(LithoCefShell* shell);

#ifdef __cplusplus
} /* extern "C" */
#endif
