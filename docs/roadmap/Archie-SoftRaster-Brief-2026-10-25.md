# Brief for Archie: Bringing the Software Rasterizer Up to LithoPipeline (2026-10-25)

Executive direction, vision and verification: JD. Systems architecture and optimization counsel: Archie (Jemmy). Lead engine implementation and empirical profiling: Cortana. Lineage is inspired by id Software's id Tech 6 (Doom 2016) and MegaTexture.

**Status:** this is a draft for discussion. Everything below is a question or an option, and none of it is a decision. It came from a read-only pass over `main` (local). Nothing was built or benched for it.

**Your queue is on hold:** the arena lock test, buildPulled timing, algebraic UV, and distance shader LOD all wait until this is scoped.

## Goal
- The crew and low-end players should see the wafer at **30 FPS** on the software path.
- The minimum-spec reference is the **4 GB RTX 3050 Ti**. Its laptop CPU class is the real target for this path.
- Render distance is **8 chunks** for testing, with **32** as the sweet spot.

## 1. What software already has (more than people assume)
From `engine/src/render/software_renderer.cpp` (2,773 lines) and its headers:
- **Rasterizer:** half-space (edge-function) triangles with incremental stepping. The file header still calls it "scanline".
- **Threading:** persistent band threads, one horizontal band of rows per thread. The thread count is set automatically, clamped to 2-16, and can be overridden with `--render-threads`. There is also a detached executor (Design B) and an SPSC upload ring.
- **SIMD:** runtime dispatch across scalar, SSE2, SSE4.1 and AVX2 (`software_simd*.cpp`). There is a 4-wide quad transform and an AVX2 `shadeSpanBasic`, which covers only the plain diffuse case. Output is bit-identical by contract.
- **Culling:** frustum culling with a margin, plus the cull prototypes. Back faces are culled (`kP2FaceCull`) using face buckets, and same-block faces of see-through cubes are culled (BUG-0008).
- **Hi-Z:** per-tile max depth with lazy re-tighten. It skips occluded rows per triangle, but only after earlier draws have written depth. There is no depth prepass and no front-to-back ordering.
- **Textures:** a CPU texture array, not the GPU atlas. It has 2x2 box mips per triangle (BUG-0005), a tile cap of 32 or 64 depending on core count, and optional Morton layout for 16x16 tiles.
- **Meshing:** prefers greedy meshes. Foliage has a near/far LOD split (cross-hatch far model), which took software from 37 to 61 FPS.
- **Extras:** an AA ladder (fxaa, cmaa, ssaa2x), an ASCII tier, PBR shading (no transparency ladder) and dither.
- **Profiling:** `LL_SW_PROF` counters (scan, coverage and shade pixels, triangles, Hi-Z rows, face-cull share). `LL_SW_P2` and `LL_SW_ISA` toggle features for A/B tests.

## 2. The gap against LithoPipeline (Vulkan)
| LithoPipeline feature | Software today | Note |
|---|---|---|
| Packed12 12-byte vertex | Reads the 16-byte `PackedVertex` | Transform is cheap, so the main cost is memory footprint |
| Pull path (64-bit packed quads, `buildPulled`) | None | Only ~8% of quads qualify even on VK |
| Blend split (Phase 4 B) | Blend filter pass (`tlBlendPass`) | No sorted-blend parity is confirmed. PBR transparency tier is missing |
| Face buckets (Phase 3) | Has face-bucket culling | Parity reached |
| Hardware depth / early-Z | Hi-Z row skip, unordered | No front-to-back sort, no prepass |
| LOD | Foliage only | No distance LOD for terrain |
| Atlas / texture arrays + GPU sampling | CPU array, nearest sampling + box mips | Sampling is likely a top cost per pixel |
| GPU parallelism | Row bands | No tiled binning, so load is uneven when geometry clusters |

## 3. Existing software performance numbers
- Testworld walk bench: 52 FPS (0.0.1.23), down to 30 after the Trace Bulb shell, back up to about 39. Today it is about 35 with the Ghostly shell and 44-48 with Solid (CHANGES.md, BUGFIX_LOG BUG-0008).
- Bench walk/orbit, cities on: 34.9 / 46.4 FPS (CHANGES.md).
- Handoff README: 15.2 FPS walk and orbit. The context is unclear, possibly a different radius or machine, and needs checking.
- Foliage far LOD: 37 to 61 FPS (Elfie brief, 10-16).
- WASM software on a 2-core VM at 1280x720: about 456-497 ms per frame, and 479 ms at radius 32. That is a VM number, not JD's hardware.
- **S11 has been run** (`docs/playtest/stress-2026-10-24/logs/S11-*`, 60 s windowed flyover, 8 render threads, JD's i5-12450H): **r8 avg 21.1 ms (~47 FPS), p99 31.9 ms; r16 avg 37.8 ms (~26 FPS), p99 63.6 ms**, dipping to 15 FPS late in the run. Render is ~96% of frame time; tick, upload and present are under 1 ms. So r16 already misses 30 FPS and r32 is untested; there is no number on 3050 Ti-class laptops yet.
- For comparison, Vulkan P3 at 1920x998 runs about 550 FPS at r8 and about 145 at r32, and is shading-bound.

## 4. Candidate ports, roughly ranked by gain against cost (all for discussion)
1. **Render scale (half-res plus upscale).** Pixel cost scales almost linearly, so this could be about 2-4x for the least work. The upscale filter could be bilinear, an FSR1-style pass, or reuse of the cmaa edges. *Question:* is a native-res HUD over a half-res world acceptable to JD's eye?
2. **Front-to-back chunk ordering, plus a cheap depth prepass or Hi-Z seeded from coarse occluders.** Hi-Z exists but only pays off once depth is written. Sorting chunks near to far is cheap. *Question:* sort only, or prepass, given the shading cost?
3. **Tiled, binned multithreaded raster** instead of row bands. Bin quads into 64x64 or 128x128 tiles and give threads work-stealing queues. This evens out clustered load and keeps tiles cache-resident. The cost is medium to high (a binning pass and its memory). *Question:* tile size and bin format? Should we reuse the Phase 3 face ranges as the bin input?
4. **SIMD widening.** The AVX2 span only covers basic diffuse. Cutout, strip and trace modes, mips, AO and PBR are scalar. The cutout path alone cost about 5 FPS (BUG-0008). *Question:* extend the span kernels per mode, or move to 8-wide edge evaluation over 8x8 blocks (Larrabee-style coarse/fine)?
5. **Texture sampling.** Specialise per-tile-size samplers, keep mip selection per triangle, consider a lower tile cap at distance, and reconsider Morton for 32/64 tiles. *Question:* is sampling or blending the hotter cost? `LL_SW_PROF` shade vs coverage counts should tell us.
6. **Distance LOD for terrain** (beyond foliage): coarser meshes or simplified shading past N chunks. This overlaps your held distance-shader LOD item. *Question:* should the software path be the first consumer?
7. **Quad packing reuse (Packed12 / pull).** The gain looks low: CPU transform is not the bottleneck, and only ~8% of quads pack. It might still cut memory and cache traffic at r32. *Question:* is it worth it here, or is it VK-only?
8. **Occlusion beyond Hi-Z** (chunk-level occlusion or cave culling from Elfie's Phase 4 research). This is high gain underground and at r32, but the cost is medium. It could be shared with VK.

## 5. Questions for Archie
1. Which 2-3 items above would you start with to reach 30 FPS at r32 on a 3050 Ti-class laptop CPU?
2. Should JD's main rule for this path (bit-identical A/B) be relaxed for render scale and LOD, so it becomes a by-eye verify like the VK diffs?
3. Row bands vs tiled bins: is a rewrite justified, or can the bands be made adaptive?
4. What bench protocol do you want first? I suggest S11 at r8/r16/r32, with `LL_SW_PROF`, at 1280x720 and 1920x1080. I'd run it only when JD isn't playing.
5. Should shared work (ordering, occlusion, distance LOD) be designed once for both paths?

## 6. Side question: the Vulkan stutter near XYZ 342 272 205
JD saw it this morning on the Vulkan path (PADS test area): flying into that area, the picture dropped to about 5 FPS while input stayed fluid. The HUD showed sim budget 79%, 96 dropped ticks at 512 TPS, and 42,799 meshes. This hasn't been diagnosed yet; Cortana is going through JD's clip and the logs.
- Options on the table: (a) render-thread stall on chunk mesh build/upload (`buildPulled` or the upload ring blocking the frame); (b) sim at 512 TPS crowding out the render thread on the shared CPU; (c) draw/descriptor overhead at ~43k meshes.
- Planned triage: F7 down to 64 TPS and re-fly. If the stutter mostly goes, it's (b); if not, (a) or (c).
- *Question:* which do you suspect, and would you budget mesh uploads per frame (a time slice) or move them fully off the render thread? Does the answer change how the software path should schedule its uploads?

## Constraints for any work that follows
`main` stays local. wasm/*, lk_webio.js and ILithoGui/ are off limits. Each work item gets a flag with the default off until JD verifies it by eye. Benches run only when JD isn't playing.
