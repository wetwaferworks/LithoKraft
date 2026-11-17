/* crt1.c ... LithoRoot's program start-up for the wasm32 module.
 *
 * The host (lkboot) instantiates the module, calls the exported
 * __wasm_call_ctors once, then calls the exported __main_argc_argv
 * (clang's wasm name for int main(int, char**)). There is no _start:
 * the module is linked --no-entry and the host owns the order.
 *
 * What a C runtime must do before any constructor allocates runs
 * here, as the FIRST constructor (priority 0, reserved for the
 * implementation, and LithoRoot is the implementation):
 *
 *   1. the default heap: WaferHeap over linear memory from
 *      __heap_base to the current end of memory, growing through
 *      lithosys_memory_grow (never the memory.grow instruction; the
 *      host enforces its 600 MiB cap and we see a refusal);
 *   2. the main thread: attached to the weave as tid 1;
 *   3. errno: armed for per-thread storage.
 *
 * No lock is installed on the heap (wh_options_t lock hooks stay
 * NULL). LithoThread's lithothread_heap_options() would back them
 * with a mutex, and the engine takes no locks of any kind (JD's
 * rule). Until WaferHeap has a lock-free shared path, the default
 * heap keeps WaferHeap's single-threaded contract; see
 * docs/TOOLCHAIN.md "Link wiring".
 */
#include <stddef.h>
#include <stdint.h>

#include "lithosys.h"
#include "lithoweave.h"
#include "waferheap.h"

void __litholibc_errno_arm_tls(void);

extern unsigned char __heap_base;

#define LR_PAGE 65536u

static void* lr__grow(wh_heap_t* heap, size_t min_extra_bytes,
                      size_t* out_region_bytes, void* user) {
  uint32_t pages, old_pages = 0;
  (void)heap;
  (void)user;
  /* Ask for the request plus 64 KiB of slack for block headers,
   * rounded to pages, and never less than 16 MiB at a time so a
   * stream of small requests does not turn into a stream of host
   * calls. */
  if (min_extra_bytes > (size_t)0xFFFFFFFFu - 2u * LR_PAGE) return NULL;
  pages = (uint32_t)((min_extra_bytes + LR_PAGE + LR_PAGE - 1u) / LR_PAGE);
  if (pages < 256u) pages = 256u;
  if (lithosys_memory_grow(pages, &old_pages) != LITHOSYS_OK) return NULL;
  *out_region_bytes = (size_t)pages * LR_PAGE;
  return (void*)(uintptr_t)((size_t)old_pages * LR_PAGE);
}

/* Start-up status, exported to the host (engine/lithoroot/CMakeLists.txt):
 * 0 ready, 1 heap only (weave attach failed), -1 crt1 never ran. */
int __lithoroot_crt_status = -1;

__attribute__((constructor(0))) static void lr__crt_start(void) {
  uintptr_t base = (uintptr_t)&__heap_base;
  uintptr_t end = (uintptr_t)__builtin_wasm_memory_size(0) * LR_PAGE;
  wh_options_t opts;
  wh_heap_t* heap;
  opts.grow = lr__grow;
  opts.grow_user = NULL;
  opts.lock = NULL;
  opts.unlock = NULL;
  opts.lock_user = NULL;
  heap = end > base ? wh_heap_create((void*)base, (size_t)(end - base), &opts) : NULL;
  if (heap == NULL) {
    static const char msg[] = "lithoroot: crt1 could not create the default heap\n";
    lithosys_console_write(msg, (uint32_t)(sizeof msg - 1u));
    lithosys_abort();
  }
  wh_set_default_heap(heap);
  __lithoroot_crt_status = 1;
  if (lw_main_thread_init() != LITHOSYS_OK) {
    __lithoroot_crt_status = 2;
    return;
  }
  __litholibc_errno_arm_tls();
  __lithoroot_crt_status = 0;
}
