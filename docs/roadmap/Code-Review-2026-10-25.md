# Code review: everything touched 2026-10-25 (b706863..3ced0ab)

Executive direction, vision and verification: JD. Systems architecture and optimization counsel: Archie (co-architect: bit-packing schemes, pipeline strategy, diagnostic triage). Lead engine implementation and empirical profiling: Cortana. Lineage: inspired by id Software's id Tech 6 (Doom 2016) and MegaTexture.

## Scope and method

- Reviewed at about 8:05 AM ET, 2026-10-25.
- **Static review only.** I read `git diff b706863 3ced0ab` and the current files. JD was playing at the time, so there were no builds, runs or benches.
- Line numbers refer to the tree at `3ced0ab`.
- **No fixes have been made.** JD verifies first, then we fix together.

| Area | Files | Commits |
|---|---|---|
| Vertex pulling, buckets, blend split | `vk_renderer.cpp`, `packed_quad64.h`, `vk_pipelines.cpp/.h`, `vk_device.cpp/.h`, `vk_vertex_arena.cpp/.h`, `chunk_vert_body.glsl`, `renderer.h` | `03af049`, `25a40b7`, `62bc967` |
| Phase 4.0 instrumentation, 4.2a fast cull | `vk_renderer.cpp`, render_vk `CMakeLists.txt`, `main.cpp` | `2f6fe56`, `585db08` |
| Bench streamer, sprint/whip paths | `game.cpp`, `game.h`, `main.cpp` | `e2a84e1` |
| Flat-frag test, sub-cell census | `chunk.frag`, `CMakeLists.txt`, `vk_pipelines.cpp`, `packed_quad64.h` | `62bc967` |
| Counters | `BUILD_COUNTER.txt/.log` | `3ced0ab` |

## Severity counts

| Severity | Count |
|---|---|
| Bug (confirmed defect) | 0 |
| Correctness risk | 6 |
| Perf | 7 |
| Cleanup | 8 |

No confirmed defects were found. The blend split was checked against `chunk_shading.glsl:166`: in pass 1 everything except alpha-blend faces is discarded, so leaving the pulled opaque quads out of the blend pass changes nothing on screen.

## Correctness risks

**R1. Phase 3 bucket visibility test: face parity not verified.** `vk_renderer.cpp:1290`
- Evidence: the test is `(face & 1u) ? cam < min+16 : cam > min`. That is only right if odd faces are the minus side. Nothing in the diff ties this to the mesher's face enum.
- Impact: with `LL_VK_BUCKETS=1`, the wrong parity would drop exactly the visible faces. It is off by default.
- Fix: add a `static_assert` or a unit test against the mesher's face constants, and a one-chunk visual check.

**R2. Bit-identical check uses `memcmp` on a struct.** `packed_quad64.h:145`
- Evidence: the exact-match guard depends on `GpuVertex12` having no padding.
- Fix: add `static_assert(sizeof(GpuVertex12) == 12)` and `std::has_unique_object_representations_v<GpuVertex12>`.

**R3. No check on the GPU's storage-buffer size limit.** `vk_device.cpp:225`, `updateArenaSet` (`vk_renderer.cpp:1248`)
- Evidence: arena blocks are bound with `VK_WHOLE_SIZE`, but `pull_` only checks dynamic indexing and the descriptor count, not `maxStorageBufferRange` against the block size.
- Fix: also require `maxStorageBufferRange >= block size` before enabling pull.

**R4. Possible integer overflow in the draw offset.** `vk_renderer.cpp:1281` (and the bucket commands right after it)
- Evidence: `int32_t(m.pullFirst * 4)` is fine with 32 MB blocks, but there is no guard if the block size ever grows past about 4 GB.
- Fix: assert `pullFirst < 2^29`.

**R5. Arena block count read without ordering.** `updateArenaSet` reads `blockCount()` with relaxed ordering, while `addBlock` (`vk_vertex_arena.cpp:88-89`) publishes the count after writing `blocks_[i]`.
- Evidence: on x86 this is benign. In C++ terms the read should be acquire so the main thread never sees a count without its buffer.
- Fix: `blockCount()` should use `memory_order_acquire`.

**R6. Arena "reserved MB" in the stream log assumes 32 MB blocks.** `vk_renderer.cpp:1039`
- Evidence: `cap = blockCount * 32.0` is hard-coded. If the block size changes, the arena-full percentage that Archie triages from becomes wrong.
- Fix: use the arena's real block size.

## Perf

**P1. Pull encoding can run on the main thread.** `uploadChunkMesh` (`vk_renderer.cpp:208` onward) and the device-only fallback in `prepareChunkMeshAsync` (`:283`)
- Evidence: `allocateMesh` calls `buildPulled` wherever it is invoked.
- When there is no direct upload, or the slice lands on a device-only block, the encode runs on the main thread. In the second case the worker has already encoded once, then frees the slice, and the main thread encodes again.
- This matches Archie's r64 hypothesis.
- Fix: run `buildPulled` only on the workers and pass the `PulledMesh` along with the prepared mesh. Never re-encode on the main thread.

**P2. Each quad tries up to 81 UV codes.** `packed_quad64.h:137` onward
- Evidence: every quad that reaches this step tries up to 81 UV codes, with 4 decodes plus a `memcmp` per try.
- Fix: queued as Q3 below (derive the codes algebraically in O(1), and keep the check as a debug-only assert).

**P3. Wasted work once the arena has more than 64 blocks.** `allocateMesh` (about `vk_renderer.cpp:796`)
- Evidence: after the arena passes 64 blocks, any slice that lands on block 64 or higher is freed and allocated again on the plain layout. The encode for it was wasted too.
- Fix: skip the pull path early when the arena is near 64 blocks, or reserve the first 64 blocks for pulled slices.

**P4. Two full passes over each mesh's vertices.** `meshHasBlend` (`:773`) scans the whole mesh, then `buildPulled` scans it again.
- Fix: have `buildPulled` report whether it saw blend quads.

**P5. Per-mesh heap allocations in `buildPulled`.** `packed_quad64.h:159`
- Evidence: the `pal` unordered_map and the six `qs`/`qm` vectors are locals, so they allocate on every mesh. The `thread_local` `PulledMesh` only reuses its output vectors.
- Fix: move them into the `thread_local` scratch and `clear()` them, or use a small flat palette.

**P6. Blend quads go through the opaque pass only to be discarded.** The opaque pass draws the fallback range, blend quads included, and the shader discards them (`chunk_shading.glsl:166`). This predates the split, but the split now keeps every blend quad in the fallback range.
- Fix (later): order the fallback range so blend quads sit at the end, and give the opaque pass a shorter `indexCount`.

**P7. Record/command buffers reserved at 8x the mesh count with pull on.** `megaPerMesh()` (`:1245`)
- Evidence: with buckets off, at most 2 + 1 per mesh are used.
- Fix: use 3 per mesh when buckets are off, and 8 only when buckets are on.

## Cleanup

1. **C1. Flat-frag flag in shipping code.** `vk_pipelines.cpp:37`, `flatFrag()`
   - The bench-only `LL_VK_FLATFRAG` ships in Release and adds a C4996 `getenv` warning. The flat SPIR-V is compiled into every build.
   - Fix: gate it behind a bench/dev CMake option.
2. **C2. Mislabelled counter.** `perf_.fallbackQuads` (`:1454`) also counts meshes that are not pulled at all, so "packed12 fallback quads/frame" is overstated.
   - Fix: count only meshes with `pullBucket[6] > 0`.
3. **C3. Stale header comment.** `packed_quad64.h:13` still says meshes with blend faces stay entirely on Packed12; the blend split changed that. The file also has no newline at the end.
4. **C4. Stale comment in `allocateMesh`.** It says "(and no blend faces)" (`:778`), which is also outdated.
5. **C5. Unbounded census map.** `SubCellCensus::raw` (`packed_quad64.h:86`) grows without limit and only feeds a count, and the census mutex is in a render header.
   - Fix: once Archie's decision is final (sub-cell stays Packed12), remove the census or move it into a bench-only TU.
6. **C6. Duplicate cull loop.** `vk_renderer.cpp:1402` has a `for (meshes_) { if (fastCull_) break; ... }` that duplicates the fast-cull loop.
   - Fix: once 4.2a is the default, use one loop over `cullList_` only.
7. **C7. Swapped doc comments.** `perf_` struct (`:1786`): the comments on `chunkGpuMs` / `chunkCpuMs` are swapped or concatenated.
8. **C8. Counter roll process.** The engine counter was rolled by hand with `tools/build_counter.py`, and the game counter was set to 61 while `.hold` stays.
   - This matches the rules, but `docs/versioning/COUNTERS.md` (another agent's file, not edited) should get a note on the manual roll, for the registrar.

## Bench streamer and instrumentation (game side): reviewed, no issues found

- Sprint and whip paths: `benchCam_` is set on every path, the streamer follows it only when `benchSet_`, and the sim is untouched. Hashes are unchanged across all gates today.
- `main.cpp` lows: they use an index sort over a copy, so `percentile()` sorting in place does not break the frame order.

## Queued optimizations (Archie 07:58)

Source: `Archie-Phase4-ABCD-Reply-2026-10-25-0758-EDT.md`. Not started.

| # | Item | Notes |
|---|---|---|
| Q1 | Arena-lock test: split on with the arena reserve locked at 1920 MB | Tells arena thrash apart from encode cost at r64. |
| Q2 | Per-thread `buildPulled` timing (wall time and call count by thread) plus an upload/staging stall counter on the main/render thread | Ties to P1: it shows whether encoding happens on the main thread. |
| Q3 | Algebraic UV derivation | O(1) codes from the face normal and winding, replacing the 81-try search. A single bit-exact decode check stays as a debug-only assert (P2). |
| Q4 | Distance shader LOD beyond r32 | A simpler material/FX path for far chunks, aimed at the ~25% fragment cost that doesn't scale with resolution (2x2 helper-lane overshading on tiny triangles). Do it after the r64 CPU churn is fixed. |
| Decision | Sub-cell and blend faces stay on Packed12 permanently | No wider third format. The blend-split deviation is the permanent architecture. |

## Suggested fix order (for JD to confirm)

1. P1 with Q2: find where the encode runs, and make it worker-only.
2. Q1: the arena-lock test.
3. Q3 with R2.
4. R1, R3, R4 and R5 as small guard fixes.
5. P3 through P7.
6. Cleanup C1 through C8.
7. Q4 last.
