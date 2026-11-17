/* thread_check.c ... lkboot thread start: spawn through lithosys_thread_spawn,
 * per-thread stack and TLS, cross-thread malloc/free on the lock-free heap,
 * lk_store from a worker thread, join through the tid cell. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lithoweave.h"

static _Thread_local int tls_value = 7;
static void* shared_blocks[8];

static void* worker(void* arg) {
  int id = (int)(intptr_t)arg, i;
  volatile char big[64 * 1024]; /* uses its own stack */
  if (tls_value != 7) return (void*)(intptr_t)-1; /* TLS initialized per thread */
  tls_value = 100 + id;
  for (i = 0; i < (int)sizeof big; i += 4096) big[i] = (char)id;
  for (i = 0; i < 2000; ++i) { void* p = malloc(16 + (i % 200)); if (!p) return (void*)(intptr_t)-2; memset(p, id, 16); free(p); }
  free(shared_blocks[id]); /* allocated on main: a cross-thread free */
  if (id == 0) { /* lk_store from a worker thread (blocks on the channel) */
    FILE* f = fopen("/thread-save.bin", "wb");
    if (!f || fwrite("T0", 1, 2, f) != 2 || fclose(f) != 0) return (void*)(intptr_t)-3;
  }
  return (void*)(intptr_t)(tls_value + big[4096]);
}

int __main_void(void) {
  lw_thread_t th[4];
  int i;
  for (i = 0; i < 4; ++i) shared_blocks[i] = malloc(64);
  for (i = 0; i < 4; ++i) {
    lw_descriptor_t d;
    memset(&d, 0, sizeof d);
    d.entry = worker; d.arg = (void*)(intptr_t)i; d.stack_size = 256 * 1024;
    if (lw_spawn(&d, &th[i]) != LITHOSYS_OK) return 10 + i;
  }
  for (i = 0; i < 4; ++i) {
    void* rv = NULL;
    if (lw_join(th[i], &rv) != LITHOSYS_OK) return 20 + i;
    if ((intptr_t)rv != 100 + i + i) return 30 + i;
  }
  if (tls_value != 7) return 40; /* main's TLS untouched */
  {
    FILE* f = fopen("/thread-save.bin", "rb");
    char b[4] = {0};
    if (!f || fread(b, 1, 2, f) != 2 || memcmp(b, "T0", 2) != 0) return 41;
    fclose(f);
  }
  return 0;
}
