/* IHwGpu.h — the GPU capability interface of the hardware layer.
 *
 * DECLARATION ONLY (2026-10-25, IHwLayer phase 2). IHwGpu is the seat for
 * hardware draw paths, starting with the Vulkan drawer (JD 2026-10-24),
 * so draw economics scale with the iron and compose with the mega-draw
 * program. Nothing in host/ implements it yet; no backend exports
 * lithohw_gpu_get_api() and nothing calls it. The census (IHwLayer.h,
 * LithoHwCensus.gpu_reachable) says whether a seat can answer.
 *
 * Like the base, plain C: no Vulkan, GL or OS types cross this header;
 * handles are opaque.
 */
#pragma once

#include "IHwLayer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LITHOHW_GPU_ABI_VERSION 1u

/* Draw APIs a machine can pay for (bit flags). */
typedef enum LithoHwGpuApiBits {
  LITHOHW_GPU_API_SOFTWARE = 1u << 0,
  LITHOHW_GPU_API_GL33 = 1u << 1,
  LITHOHW_GPU_API_GL43 = 1u << 2,   /* glMultiDrawElementsIndirect */
  LITHOHW_GPU_API_VULKAN = 1u << 3,
  LITHOHW_GPU_API_WEBGPU = 1u << 4  /* roadmap: future renderer sibling */
} LithoHwGpuApiBits;

/* What the GPU can do for draw economics. 0 = unknown / absent. */
typedef struct LithoHwGpuCaps {
  uint32_t abi_version;            /* LITHOHW_GPU_ABI_VERSION */
  uint32_t api_bits;               /* LithoHwGpuApiBits */
  uint32_t multi_draw_indirect;    /* 1 = many draws per indirect call */
  uint32_t draw_indirect_first_instance; /* 1 = firstInstance honoured in indirect draws */
  uint32_t max_draw_indirect_count;
  uint32_t vram_mb;
  uint32_t vendor_id, device_id;
  uint32_t reserved[4];
} LithoHwGpuCaps;

typedef struct LithoHwGpuApi {
  uint32_t abi_version;
  /* Fill *out for the GPU behind this shell. LITHOHW_OK, or
   * LITHOHW_ERR_UNSUPPORTED when no GPU seat answers. */
  int (*query_caps)(LithoHwShell* shell, LithoHwGpuCaps* out);
} LithoHwGpuApi;

/* Declared, not implemented (phase 2). A tenant that seats a GPU path
 * exports this; until one does, nothing may call it. */
const LithoHwGpuApi* lithohw_gpu_get_api(void);

/* ---- Phase 3 (2026-10-25): the draw-path seat. SHAPE ONLY. ----------
 * A hardware draw path (first: the Vulkan drawer) registers behind
 * IHwGpu with what it needs from the census; the layer seats the best
 * path the machine can pay for and the engine submits batches without
 * learning which path or tenant answered. See IHwGpu-DESIGN.md. Nothing
 * implements these yet; JD decides the depth. */

typedef enum LithoHwDrawPass {
  LITHOHW_DRAW_PASS_OPAQUE = 0,
  LITHOHW_DRAW_PASS_CUTOUT = 1,
  LITHOHW_DRAW_PASS_TRANSLUCENT = 2 /* submitted far -> near; order is preserved */
} LithoHwDrawPass;

/* One pass worth of draws, as the mega-draw program shapes it: a
 * geometry arena, a per-draw data buffer (offsets, pulse/trace params)
 * indexed by draw id, and an indirect command list. Handles are opaque
 * (VkBuffer / GL buffer name / CPU pointer, per path). */
typedef struct LithoHwDrawBatch {
  uint32_t pass;            /* LithoHwDrawPass */
  void* arena;              /* vertex arena block */
  void* per_draw;           /* per-draw data, draw_count records */
  void* indirect;           /* indirect commands, draw_count records */
  uint32_t draw_count;
  uint32_t face_bucket_mask; /* bits 0..5: face directions present (V4 buckets); 0x3F = all */
} LithoHwDrawBatch;

typedef struct LithoHwDrawCounters {
  uint64_t batches;         /* submit_batch calls */
  uint64_t draws;           /* logical draws (chunk meshes) */
  uint64_t api_calls;       /* draw calls that reached the driver */
  uint64_t gpu_time_ns;     /* 0 = unknown */
} LithoHwDrawCounters;

typedef struct LithoHwDrawPathOps {
  void* user;
  int (*begin_frame)(void* user);
  int (*submit_batch)(void* user, const LithoHwDrawBatch* batch);
  int (*end_frame)(void* user);
  void (*counters)(void* user, LithoHwDrawCounters* out);
} LithoHwDrawPathOps;

typedef struct LithoHwDrawPathDesc {
  uint32_t abi_version;     /* LITHOHW_GPU_ABI_VERSION */
  const char* name;         /* "vulkan-mega", "gl43-mdi", "gl33-loop", "software" */
  uint32_t api_bit;         /* one LithoHwGpuApiBits value */
  uint32_t needs_multi_draw_indirect;
  uint32_t needs_draw_indirect_first_instance;
  int32_t priority;         /* higher wins among paths the caps allow */
  LithoHwDrawPathOps ops;
} LithoHwDrawPathDesc;

/* Seat calls a GPU tenant will append to LithoHwGpuApi (declared here as
 * free functions so the table's ABI 1 stays as phase 2 left it):
 *   register: LITHOHW_OK, or LITHOHW_ERR_UNSUPPORTED when the caps
 *             cannot pay for the path's needs (the path is not seated).
 *   seated:   the path chosen for this shell, or NULL (fallback = the
 *             renderer's own per-mesh loop). */
int lithohw_gpu_register_draw_path(LithoHwShell* shell, const LithoHwDrawPathDesc* desc);
const LithoHwDrawPathDesc* lithohw_gpu_seated_draw_path(LithoHwShell* shell);
#ifdef __cplusplus
} /* extern "C" */
#endif