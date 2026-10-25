# IHwGpu — draw-path seat design note (phase 3, shape only)

2026-10-25, for JD's review. Direction and final verify: JD. Implementation: Cortana. Plan and register: Elfie.

## What is seated
`IHwGpu.h` now declares the draw-path seat: `LithoHwDrawPathDesc` (name, API bit,
caps it needs, priority, ops table), `LithoHwDrawBatch` (one pass: arena, per-draw
data, indirect list, draw count, face-bucket mask) and `LithoHwDrawCounters`
(batches, logical draws, driver calls, GPU ns). Two free functions are declared:
`lithohw_gpu_register_draw_path` and `lithohw_gpu_seated_draw_path`. **Nothing implements
them**, and nothing in the engine calls them.

## How it composes
1. **Census informs the seat.** `LithoHwCensus.gpu_reachable` says a seat can answer;
   `LithoHwGpuCaps` (multi-draw indirect, firstInstance in indirect draws, max count)
   decides which registered paths the machine can pay for. Highest priority among
   affordable paths is seated; none affordable = NULL = the renderer's current
   per-mesh loop (today's behaviour, the fallback).
2. **Paths, all pipelines.** Intended registrants: `vulkan-mega` (V1–V4 of the
   Vulkan mega-draw plan: per-draw data buffer + `vkCmdDrawIndexedIndirect`, one call
   per arena block per pass), `gl43-mdi` (`glMultiDrawElementsIndirect`), `gl33-loop`
   (today's `glDrawElements` loop), `software`. The web leg is Elfie's lane and lands
   separately; it does not register through host/.
3. **Batches mirror the mega-draw shape.** Opaque / cutout / translucent passes;
   translucent batches arrive sorted far → near and the path must preserve order.
   The face-bucket mask carries V4 (six face-direction buckets, back-facing buckets
   skipped).
4. **Honest counters.** `draws` (logical meshes) vs `api_calls` (driver calls) is
   the number the program is judged by: ~7,000 calls/frame today (JD), ~20K draws at
   radius 32+ on Vulkan in tonight's benches; target under 1,000.
5. **Engine never learns the tenant.** The engine sees batches and counters only;
   which tenant or path answered stays inside host/.

## Open for JD
- Depth: implement the seat behind the Vulkan renderer (touches engine/src/render_vk,
  outside host/, so a separate lane and approval), or keep host/ shape-only.
- Whether `register`/`seated` move into the `LithoHwGpuApi` table (ABI 2) once a tenant
  implements them.
- The Thread Handshake could later grant the draw path a record-thread; not designed here
  (Thread Handshake semantics frozen).