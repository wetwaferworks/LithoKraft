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

#ifdef __cplusplus
} /* extern "C" */
#endif