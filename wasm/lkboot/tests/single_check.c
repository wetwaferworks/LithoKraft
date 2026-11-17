/* single_check.c ... the single-thread fallback: spawn must fail soft
 * with EAGAIN, everything else (heap, stdio, lk_store) keeps working. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lithoweave.h"
static void* entry(void* a) { return a; }
int __main_void(void) {
  lw_descriptor_t d; lw_thread_t th;
  void* p = malloc(1 << 20);
  memset(&d, 0, sizeof d); d.entry = entry;
  if (!p) return 1;
  free(p);
  if (lw_spawn(&d, &th) != LITHOSYS_EAGAIN) return 2;
  {
    FILE* f = fopen("/s.bin", "wb");
    if (!f || fwrite("ok", 1, 2, f) != 2 || fclose(f) != 0) return 3;
  }
  /* the budget: 400 MiB more than the 256 MiB start cannot fit under 500 */
  /* the budget: a 300 MiB block makes the heap grow to about 453 MiB.
   * Under a 500 MiB budget that fits; under 450 it must fail (rc 4),
   * though both are far below the 600 MiB hard cap. */
  {
    void* volatile q = malloc((size_t)300 << 20); /* volatile: keep clang from folding malloc != NULL */
    void* volatile r;
    if (q == NULL) return 4;
    (void)r;
    free(q);
  }
  printf("single ok\n");
  return 0;
}
