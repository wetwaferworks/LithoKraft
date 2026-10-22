# Vulkan renderer review: 2026-10-16/17 (0.0.1.55 targeted pass + 0.0.1.56 full pass)

Reviewer: Cortana (Grok Bot). Line numbers refer to the **0.0.1.55 source, before the 0.0.1.56 fixes** (files under `engine/src/render_vk/` unless noted).

## Scope

The 0.0.1.56 pass read these in full, line by line:
- `vk_renderer.cpp` (1213 lines), `vk_pipelines.cpp` (445), `vk_swapchain.cpp`, `vk_vertex_arena.h/.cpp`, `vk_texture_array.cpp`, `vk_resources.cpp`, and `vk_frame_uniforms.h` (asserts).
- `assets/shaders/vk/chunk.vert`, `chunk.frag`, `post.vert`, `post.frag`, `frame.glsl`, and the index-bearing parts of `chunk_shading.glsl`.
- The far-LOD / near-strip code in `bench_stream.cpp`.

Not read line by line:
- `vk_device.cpp` (only validation-layer, instance and robustness bits) and `vk_loader.cpp` / `vk_probe.cpp` (loader plumbing).
- The sprite / particle / HUD shaders (small, unchanged since the 0.0.1.50 review).

**Validation layers:** not run. `VK_LAYER_KHRONOS_validation` is not installed on TheWorkstation: there is no `C:\VulkanSDK`, and the Khronos registry lists only the OBS and Steam implicit layers. Khronos' GitHub releases ship Android binaries only, so a layer-only drop-in was not possible without installing the SDK. The renderer already supports validation (`vk_device.cpp:120`), so it is one SDK install away.

## Findings, ranked

| # | Sev | Where (0.0.1.55 lines) | Finding | Status |
|---|-----|------------------------|---------|--------|
| R1 | **High** (rare path) | vk_renderer.cpp:290-303, 586-590 | `vkResetFences` runs before `vkQueueSubmit`. A failed submit left the fence unsignaled, so the next `waitSlot` (`UINT64_MAX`) hung forever. The acquired semaphore was left signaled. VK_ERROR_DEVICE_LOST was only logged, and the game kept looping. | **FIXED F56-1**: on submit failure, an empty batch waits on the semaphore and signals the fence; the swapchain rebuilds next frame. DEVICE_LOST from acquire/submit/present/fence wait sets `deviceLost_`, logs one clear message, and stops rendering. `IRenderer::deviceLost()` makes main.cpp close through the normal shutdown path. Signing/fatal-exit path untouched. |
| R2 | **Medium** | vk_renderer.cpp:205-262 | A worker `prepareChunkMeshAsync` in flight across `clearChunkMeshes` could publish a stale slice, or leak it. | **FIXED F56-2**: a mesh epoch (`meshEpoch_`) is read before allocation and bumped by clear. Publishing an older epoch frees the slot instead of drawing it. Source-data staleness stays the caller's job (BenchStreamer `gen` check, bench_stream.cpp:278-281). |
| R3 | **Medium** (new) | vk_renderer.cpp:610-628 + main.cpp `if (w > 0 && h > 0)` | While the window is minimized, `renderFrame` is never called, but streaming keeps uploading. Every device-local upload went to staging plus a fresh 32 MB overflow buffer that is only recycled after a submit, so memory grew without bound. | **FIXED F56-5**: beyond ring + 4 overflow buffers, the slot's pending copies are flushed with one blocking submit (the slot is idle by construction) and the ring restarts. |
| R4 | Low | vk_renderer.cpp:813-822 | The screenshot copy had no TRANSFER_WRITE to HOST_READ barrier before the host read in `grabFramebuffer`. The spec requires it; it worked on coherent memory. | **FIXED F56-3** (render-only, pixel-identical). |
| R5 | Low | vk_renderer.cpp:565-570 | `recreateSwapchain` destroys the old per-image `renderDone_` semaphores after `vkDeviceWaitIdle`. That does not cover waits still pending in the presentation engine, which validation can flag on some drivers. | Open. A proper fix needs VK_EXT_swapchain_maintenance1 present fences, or deferred deletion. |
| R6 | Low | vk_renderer.cpp:283-284, vk_swapchain.cpp:79 | With a non-zero window size but a zero surface extent (some minimize paths), every frame runs `vkDeviceWaitIdle` plus a caps query and returns without presenting. The loop spins with no pacing (CPU burn while minimized). | **FIXED F56-8** (0.0.1.56-b): `recreateSwapchain` sleeps 16 ms when no swapchain can be created. |
| R7 | Low | vk_swapchain.cpp:25-31 | Falls back to `formats[0]` when there is no B8G8R8A8/R8G8B8A8 UNORM. It could be a non-UNORM format, which silently disables screenshots (`recordGrab` returns). | Open, informational. |
| R8 | Low | vk_renderer.cpp:192, 216 | Meshes over `kMaxQuads` quads are silently truncated (`indexCount = min`). | **FIXED F56-9** (0.0.1.56-b): logged once per session. Drawing unchanged. |
| R9 | Info | vk_vertex_arena.cpp:59-66, 93-109 | Arena blocks are never released, so memory stays at peak. The ReBar-to-Device downgrade is permanent for the session. `vkAllocateMemory` runs under the arena mutex, which stalls worker allocations for that call (performance only). | Informational. |
| R10 | Info | vk_swapchain.cpp:127 | The depth view uses the DEPTH aspect only. Fine for D32_SFLOAT, which every desktop GPU picks. The D32_S8 / D24_S8 fallbacks are unverified without validation layers. | Informational. |
| R11 | Info | vk_renderer.cpp:645-649 | `flushCopiesNow` is dead code (no callers). | Informational. |
| R12 | OK | vk_renderer.cpp:795-821 | Grab layout barrier: srcAccess 0 is valid. The composite pass's external dependency (vk_pipelines.cpp:146-155) makes the colour writes available, and the barrier chains at COLOR_ATTACHMENT_OUTPUT. | OK |
| R13 | OK | vk_pipelines.cpp:134-177 | The frame and blur subpass dependencies cover cross-frame write-after-write and write-after-read hazards on the shared depth, glow and blur images, and the semaphore-ordered layout transition of the swapchain image. | OK |
| R14 | OK | vk_texture_array.cpp:150-200 | Mip blit chain and coverage-mip overwrite: every level's layout and access transitions are correct. The empty registry still has the "missing" layer, so arrayLayers >= 1. | OK |
| R15 | OK | shaders | `uFxMap` index clamped 0..511; `uAnimSeq` clamped; LOD collapse is degenerate outside clip. `uTintMask[vVal & 31]` matches 32 slots (F55-3). UBO is 4480 B (< 16 KB minimum); push constants 80 B (<= 128). | OK |
| R16 | OK | bench_stream.cpp:23-28, 176-181, 240-256 | LOD hysteresis band works in both directions (33-34, 49-50). Near-strip is safe: a stripped column's centre is >= 40 blocks from the player and chunk.vert switches at 32. A stale LOD generation is discarded and its slot freed. | OK |
| R17 | OK | vk_renderer.cpp:279-282, 599-658 | Parked-upload ordering (A52-4): drained before `releaseRetired`; a slice retired at frame N is freed at N+1 or later, after its parked bytes were copied. `clearChunkMeshes` drops `parked_`. | OK |
| R18 | OK | vk_renderer.cpp:516-572, 994-1037 | Descriptor lifetime: per-frame sets are rewritten only with the device idle (texture upload, sprite atlas); the bloom sets after `waitIdle` in recreate. | OK |
| R19 | OK | vk_renderer.cpp:574, 652-658 | Retire timing: `frameNumber_ >= retiredAt + 1` exactly covers the one frame that can still be in flight. | OK |

## Found during verification (outside render_vk)

| # | Sev | Where | Finding | Status |
|---|-----|-------|---------|--------|
| R20 | **Medium** (test infra + product) | core/log.cpp:96-110 (`_wfsopen(..., _SH_DENYWR)`), main.cpp:770-771, p2logs/gate.ps1 shot loop | A second LithoKraft instance cannot open the shared `logs\lithokraft.log` while another one runs (for example JD playing). It logged `ERROR could not open log file ...; console only`, and the gate then counted errors in the *other* instance's log file. Reproduced 7/7 under a concurrent instance. This is the most likely cause of the 1-error-per-gate noise in 055a/b/c (JD was playing 0.0.1.53 then). | **FIXED F56-11** (0.0.1.56-d): the second instance logs to `logs\lithokraft.second-<ms>.log` with a WARN. Each gate shot gets its own `--log p2logs\gates\<tag>_<shot>-shot.log` and the gate counts ERRORs in that file only (a missing file fails the shot). The gate is not weakened: every ERROR still fails it. |
| R21 | Medium (pre-existing since <= 0.0.1.53) | sim thread scheduling | While a second LithoKraft (or any heavy program) starts, the running game's simulation threads were starved of CPU. The result was dropped ticks (398 in 50 s at 512 Hz with the game at normal priority) and, in some runs, a protective sim freeze until resume. Not a shared named resource: mutexes, file maps, locks, ports, settings/save writes and timer resolution were all checked. | **FIXED F57-1/F57-2** (0.0.1.57-a-simprio): the simulation threads now run one step above normal priority and are exempt from Windows background power throttling, and a stall check no longer misfires on a phase that has already completed. After the fix: 0 dropped ticks, 0 freezes. Details are in JD's private notes. |

0.0.1.55 bit-drop items (F55-1..4: rot mask, negative box coords, TraceTint comment, slice comment) were fixed in 0.0.1.55. See BUGFIX_LOG BUG-0017..0019.

Credits: bit-drop findings Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker. Review R1-R19 and all fixes by Cortana.

## Packed12 review (0.0.1.59, 2026-10-17, Cortana)
Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; layout correction and build by Cortana. A line-by-line review of every changed file.

| ID | Severity | Finding | Status |
|---|---|---|---|
| F59-1 | Low | LL_VK_PACKED12 only treated a leading '0' as off, so "off"/"false"/"no" enabled Packed12. | Fixed in 59-b (BUG-0031) |
| F59-2 | Info | Shader header comment said "12-byte PackedVertex". | Fixed in 59-b (BUG-0032) |
| F59-3 | Info | vk_vertex_arena.cpp/.h were mixed CRLF/LF; they are now uniformly CRLF, so the diff is noisy. | Accepted (cosmetic) |
| F59-4 | Info | The perf line's upload bytes and MB/frame now count GPU bytes (12 per vertex with Packed12), so they are not 1:1 comparable with older logs. The new "mesh X MB" is live arena slices including padding. | Documented |
| F59-5 | Watch | Gate shots are not frame-deterministic (pulse, sparks, time). The on/off differences sit inside the run-to-run noise of two identical 16-byte runs, and the static tex_closeup shows 0 px > 24. An exact-zero proof needs a frozen-time capture mode. | Open |
| F59-6 | Watch | The last perf window's stream GPU ms looked higher with Packed12 (3.11 vs 2.29). F59-6 - Discovered by Cortana (review), Investigated by Cortana: artifact of single-window GPU sampling on a streaming bench; Packed12 is ~17% lower GPU ms at matched draw counts (121/121 windows); no code change. A/B 2026-10-17 06:00-06:16 ET: 14 interleaved on/off pairs. At matched draws Packed12 is -1.7 ms at r8 round default and in the test world (fps 93.0 vs 85.9) and -3.7 ms at still r64 (fps 75.6 vs 66.7, p99 25 vs 43 ms; 16-byte spills 416 MB to host). Stream r64 shows no significant difference. Write-up: docs\F59-6_GPU_TIME_2026-10-17.md. | Closed 2026-10-17 (no code change) |

**Checked, OK:**
- Bit math and masks: only unsigned values. zl >> 9 yields 0..127. The packer clamps before shifting, so nothing wraps into the layer bits.
- Layout: memcpy of PackedVertex bytes 0..11 matches the field offsets (static_assert on u at 6 and mode at 11).
- Vertex input: formats, offsets and stride match the GLSL inputs. Packed12 is stride 12 with locations 0, 1, 2, 3; location 4 is removed only in chunk12.vert.
- Entity pipeline: built from the 16-byte desc (stride 16, location 4) before the switch. chunkBlend copies the Packed12 desc, which is correct because it draws arena meshes.
- Alignment: 192 for Packed12, and block size rounded to whole slices. Slices are 192-aligned, so baseVertex = offset / 12 is exact. The 16-byte path keeps 64 and the same block size.
- Upload sizing in all three paths (in place, staged via scratch, worker-direct) is n × stride. stage() copies at once or parks a copy, so reusing the scratch buffer is safe.
- Thread safety: the worker path uses a pure packer and an atomic miss counter.
- Arena accounting: used-bytes stays correct because arena clear() is never called.
- Fallback (=0): byte-identical uploads, alignment 64, and the same shader source via the shared body.
- GL33, software, the CPU PackedVertex, meshChecksum and the sim hashes are untouched.

**Verification:** both rounds (59-a and 59-b) gave a FULL gate PASS with the toggle on and off. Hashes held on both runs: cities on cb32c824f8b3ea98, cities off e015e51cf1559089, r4 square cb32c824f8b3ea98, r4 round 966be55811fb23a7. Engine render tests are OK. The full engine suite is 2676/6; the 6 failures are the symlink sandbox tests, which need Windows symlink rights (environmental).
