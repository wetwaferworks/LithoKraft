# LithoKraft Bugfix Log

A permanent record of every bug found and fixed in LithoLattice / LithoKraft. This log is meant to become public-facing. Entries are never deleted; status changes are appended.

## Status legend
- **Fixed** - fixed in the build named.
- **Open** - confirmed, not fixed yet.
- **Won't fix - intended mechanic** - it works as designed. Always has a *Why* line.
- **Won't fix - different solution** - the problem is handled another way. Always has a *Why* line.
- **Happy little accident (promoted to feature)** - an accidental behaviour we keep on purpose (like Minecraft's quasi-connectivity). Always has a *How it works* line and a *Why* line.
- **Embargoed (security)** - a security hole kept private until it is patched, then published with full Discovered by / Fixed by credit.

Credit: "Discovered by" is the first reporter. "Fixed by" is filled in only once the fix ships; open entries say "Fixed by: (open)".

---

## BUG-0001 - Inventory and hotbar too small to read
- Date: 2026-10-16 12:00 ET
- Build: Alpha 0.0.1.3
- Discovered by: J.D.
- Fixed by: Cortana
- Symptom: inventory slots, block icons and the hotbar were too small to read.
- Root cause: slots were fixed at 40 UI units x the integer GUI scale, with no readability margin.
- Fix: new constant GameUi::kInventoryScale = 1.25 scales the inventory cells, slot boxes, icons (icons fill the slot) and the hotbar by 25% on all three renderers. The palette shows 5 visible rows (was 6) so it fits at 1366x768 and 1920x1080.
- Files: game/LithoKraft/src/game/game_ui.cpp, game/LithoKraft/src/game/include/lithokraft/game/game_ui.h
- Status: Fixed

## BUG-0002 - Pick block (middle click) does nothing
- Date: 2026-10-16 12:00 ET (fixed 14:53 ET)
- Build: reported Alpha 0.0.1.3, fixed in Alpha 0.0.1.23
- Discovered by: J.D.
- Fixed by: Cortana
- Symptom: middle click on a block did nothing, while flying AND (J.D. retest) while walking, e.g. on a Repeator.
- Root cause: the pick code only changed what you hold when the block was ALREADY on the hotbar, or when the one collected copy happened to land in the hotbar. The creative / test-world kit fills every hotbar slot, so the picked block was quietly added to the carry inventory and the hand never changed. Not a flight branch; flying just made it more common (aiming at blocks that aren't on the hotbar).
- Fix: hotbar hit -> select that slot; block already in the carry inventory -> swap that stack into the selected slot; creative miss -> the block goes into the selected slot and the stack that was there moves to the carry inventory (warning logged if the inventory is full).
- Test: new headless selftest drives the real Game with a real middle-click frame, walking and flying, and checks the HAND holds the looked-at block (it failed before the fix).
- Files: game/LithoKraft/src/game/game.cpp (handleWorldActions), src/headless/harness_selftest.cpp, src/headless/headless_harness.h, src/game/include/lithokraft/game/game.h (setFlyMode for the harness)
- Status: Fixed

## BUG-0006 - Unpowered Repeator (and other tint-masked machines) look red
- Date: 2026-10-16 14:45 ET
- Discovered by: J.D.
- Fixed by: Cortana
- Root cause: the kShadeTraceTint path mixed the tint mask with the Trace palette ramp's START colour when unpowered (row index 0 = dim red), so an off Repeator was red-tinted.
- Fix: no tint at all when unpowered (plain base texture = neutral grey); the colour and emissive only when powered, matching the Via rule. Same change in chunk_shading.glsl (GL + Vulkan) and the software mirror.
- Files: engine/assets/shaders/chunk_shading.glsl, engine/src/render/software_renderer.cpp
- Status: Fixed (render-only)
## BUG-0003 - Software renderer looks blurry on blocks and Trace
- Date: 2026-10-16 12:00 ET (fixed 12:45 ET)
- Build: reported in Alpha 0.0.1.3; fixed in Alpha 0.0.1.8
- Discovered by: J.D.
- Fixed by: Cortana
- Symptom: blocks and red/blue Trace looked soft on the software renderer; Vulkan looked crisp.
- Root cause: not the AA pass (render.aa defaults to off). CpuTextureArray::upload box-filtered every 32x32/64x64 pack texture down to fixed 16x16 tiles, so each texel averaged 4-16 source pixels.
- Fix: the software tile size is now chosen at runtime (16/32/64) from the loaded pack. The cap is 64 by default and 32 on machines with 4 or fewer cores (Chromebook tier); LL_SW_TEXRES=16|32|64 overrides it. Texel addressing (shift/mask/area/scale) is carried in FragmentInput and in the SSE2/SSE4.1/AVX2 span kernels. Morton swizzle stays 16x16-only, so it is off at 32/64.
- Verified: new selftest checks texel == source pixel at tile 64, plus the 32 cap. A software capture at LL_SW_TEXRES=64 vs 16 differs on 43% of sampled pixels (the art detail).
- Files: engine/src/render/software_renderer.cpp, include/.../cpu_texture_array.h, software_shaders.h, software_simd.h, software_simd_span4.inl, software_simd_avx2.cpp, game/LithoKraft/src/app/selftest.cpp
- Status: Fixed

## BUG-0004 - --ui-shot never writes its PNG unless --bench is also given
- Date: 2026-10-16 12:45 ET
- Build: fixed in Alpha 0.0.1.8
- Discovered by: Cortana
- Fixed by: Cortana
- Symptom: `LithoKraft --ui-shot file.png` ran forever and wrote nothing.
- Root cause: main.cpp only armed the UI capture inside bench mode (`uiShotDone = uiShot.empty() || benchSeconds <= 0`), and nothing in the usage text said so.
- Fix: --ui-shot without --bench now implies a 2 s self-closing bench.
- Files: game/LithoKraft/src/app/main.cpp
- Status: Fixed
## BUG-0005 - Software renderer: incoherent sparkle/tearing at draw distance
- Date: 2026-10-16 12:53 ET
- Build: reported on Alpha 0.0.1.8/0.0.1.9; fixed in Alpha 0.0.1.11 (current 0.0.1.12)
- Discovered by: J.D.
- Fixed by: Cortana
- Symptom: far terrain shimmered and looked noisy or torn in the software renderer after the 64x64 texture change.
- Root cause: the software path samples nearest-texel with no mip chain. Since BUG-0003 every texel is full-resolution (64x64), so a distant face packs many texels into each pixel and the sampled texel jumps every frame (texture aliasing). At 16x16 it was 4x milder. Not a vsync tear; VSync is a separate existing option in Settings > Video.
- Fix: CpuTextureArray builds a 2x2 box mip chain for 32/64 tiles (64->32->16->8->4). Each triangle picks a level from its texel-to-pixel ratio (0.5*log2(uv area * tile^2 / screen area), rounded). That level is applied to the scalar and SIMD span paths. 16x16 arrays keep one level, so they are bit-identical. New setting render.mipmaps (default on, live).
- Cost: walk p50 16.4 -> 17.6 ms, orbit 16.9 -> 19.0 ms (testworld 1080p; includes this build's AO + shimmer).
- Files: engine/src/render/software_renderer.cpp, include/.../cpu_texture_array.h
- Status: Fixed (code + capture; JD to confirm in play)
---

## Future: reporter badges
Player badges for the most bugs reported, the most found and fixed, and the most happy little accidents.
- Proposed names: **Top Reporter**, **Bug Squasher**, **Happy Little Accident**.
- Rule: badges count confirmed bugs only. Credit goes to the first reporter, and duplicates are merged so nobody can farm badges.
## BUG-0007 - All Repeators face the player (Discovered by J.D.) - OPEN
- Code audit: placement writes per-block facing from look (output away), delay per block in aux; sim/render read per block. Packing to nibble bits 0-3 would collide with 7-bit value field; not repacked this build.
- Not reproduced: headless harness right-click place does not land; selftest testRepeatorFacing written but disabled. Suspect: top texture arrow orientation vs local -Z front.

## BUG-0008 - Software fps 52 -> 30 after Trace Bulb shell (Discovered by Cortana's build check, Fixed by Cortana) - PARTIAL
- Cause: trace tips flagged kTerrain & ~kOpaque (overdraw of inner/neighbour faces in Trace trees) + kShadeCutout scalar per-pixel path.
- Fixed: same-block face cull for see-through cubes (30 -> ~39 fps). Remaining ~12 fps = ghost look itself (neighbour faces behind bulbs ~6, cutout pixel path ~5); needs a look-vs-fps decision (solid bulbs on software / Graphics toggle).
- BUG-0008 update (0.0.1.40): JD decided - Graphics setting 'Trace Bulb shell: Ghostly / Solid', default Ghostly. Solid restores the old opaque bulb (~44-48 fps vs ~35 Ghostly on software). Ghostly cost is accepted by design; status Resolved (by setting).

## 2026-10-16 2015: Highway Ramp / streaming pass (0.0.1.48, compile-check)
- Fixed during the pass: a second page("Graphics") call created a 5th settings tab (settings-tabs selftest FAIL). The streaming widgets now sit inside the existing Graphics page. Proposed by JD, Built by Cortana.
- Old-ramp item: 'Ramp collision and walk-up' is now done for the Highway Ramp only. The Aluminum Ramp is still a full cell.


## BUG-0009 - Trace Bulbs render as solid hot gems with Vulkan effects on (Discovered by J.D.) - FIXED
- Date: 2026-10-16 21:02 ET
- Build: 0.0.1.50 (found), 0.0.1.51 (fixed)
- Cause: blend-pass alpha and glow mask + bloom applied to the whole shell.
- Fix: shell alpha capped at 0.22 and unbloomed; only the core/lens (glow mask R) glows. Render-only, hashes unchanged.
- Fixed by: Cortana

## BUG-0010 - Bulb shell stays Solid after config.lth exec (Discovered by Cortana) - FIXED
- Date: 2026-10-16 21:05 ET
- Build: 0.0.1.51
- Cause: config.lth set `render.bulb_shell solid`, then settings restored ghostly without a remesh.
- Fix: remesh after restore (`game.cpp`).
- Fixed by: Cortana

## BUG-0011 - "out of vertex memory; sub-chunk not drawn" at Vulkan stream radius 64 (Discovered by Cortana's bench) - PARTIAL
- Date: 2026-10-16 20:56 ET
- Build: 0.0.1.50 (found), 0.0.1.51 (fix in, not benched yet)
- Fix: the vertex arena spills to host-visible memory past 70% of the device heap; stream-bench uploads capped at 6 MB/frame.
- Fixed by: (open, pending the 64-chunk re-bench)

## BUG-0012 - Smeared textures in Vulkan screenshots (Discovered by J.D.) - OPEN
- Date: 2026-10-16 21:10 ET
- Build: 0.0.1.50
- Plan: anisotropic filtering, LOD bias/mip check, sRGB vs UNORM, fog density.
- Fixed by: (open)
## BUG-0011 status update - FIXED in 0.0.1.52
- 64-chunk stream bench: 0 "out of vertex memory" (was 6709 in 0.0.1.50); the arena spills past 2752 MB device to host (288-416 MB host blocks).
- Fixed by: Cortana

## BUG-0012 status update - FIXED in 0.0.1.52 (Vulkan)
- Anisotropic 8x + LOD bias -0.25 on the block texture sampler; NEAREST magnification and UNORM kept. Before/after: /workspace/vulkan-push/052_tex_*_BEFORE/AFTER.png.
- Fixed by: Cortana

## BUG-0013 - Running config.lth rewrote settings.json (bulb shell flipped to Solid between runs) (Discovered by Cortana) - FIXED
- Date: 2026-10-16 21:45 ET
- Build: 0.0.1.51 (found), 0.0.1.52 (fixed)
- Cause: config.lth's archived bridge cvars go through onSettingsChanged, which saves settings.json. The game then restored the in-memory settings but not the file, so the next run read the stale value (render.bulb_shell Solid at 21:18:03 in the gate log).
- Fix: save settings.json again after the restore (`game.cpp`). Two gates later the setting is still ghostly.
- Fixed by: Cortana

## BUG-0014 - Distant wafer traces alias and shimmer; wafer heartbeat never bloomed (Discovered by Argon) - FIXED
- Date: 2026-10-16 21:40 ET
- Build: 0.0.1.51 (found), 0.0.1.52 (fixed)
- Fix: per-axis fwidth trace AA with energy-preserving width ratio; procGlow into fragGlow. Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; Built by Cortana.

## BUG-0015 - Radius-64 stream: ~27-32 ms/frame of main-thread mesh "upload" (Discovered by JD) - FIXED
- Date: 2026-10-16 22:20 ET
- Build: 0.0.1.52 (found), 0.0.1.53 (fixed)
- Cause (measured with the 0.0.1.53 instrumentation): `BenchStreamer::update` ran `snapshotForMeshing` for every new sub-chunk on the main thread (10-54 ms/frame). Its upload loop (17-23 ms/frame) did the per-vertex blend scan, bookkeeping and vertex-vector frees there too. The arena alloc + ReBAR memcpy was only 2-7 ms of that.
- Fix: the snapshot moved into the mesh job (shared lock on the bench map). With ReBAR/host memory, workers allocate and write their arena slice directly; the main thread only publishes handles (512/frame budget). VertexArena is now thread-safe. Main-thread upload 30.8 -> 3.0 ms/frame; sim hashes unchanged.
- Remaining: at full radius 64, the GPU (52 ms, 2.6 GB host-spilled vertices) and gameplay update (`loco` + world `pumpStreaming`) bound the frame. LOD for ring 33-64 is the next step.
- Proposed by JD, Built by Cortana

## BUG-0016 - Bench text summary line printed shifted values (avg=925760ms, p50 < p99 swapped) (Discovered by Cortana) - FIXED
- Date: 2026-10-16 23:00 ET
- Build: 0.0.1.52/0.0.1.53 (found), 0.0.1.54 (fixed)
- Cause: `main.cpp` passed `SoftwareRenderer::renderThreads()` without a `%u` in the format string, so every later field moved by one. The JSON output was always correct.
- Fix: added `rthreads=%u`. Proposed by JD, Built by Cortana.

## BUG-0015 status update (0.0.1.54)
- Main-thread upload stays at about 3.5 ms. With far LOD on, the frame is now bound by `update` (gameplay catch-up + world streaming, 40-65 ms) under worker-thread contention. Open, not render-only.


## BUG-0017 - faceAo byte: unmasked `lk.rot << 5` (mesher.cpp:77) (Discovered by Argon bit-drop review; Fixed by Cortana) - FIXED 0.0.1.55
Now `(lk.rot & 3) << 5`. Latent: rot is 0..3 today, so pixels are unchanged (tex_closeup 0.000% vs 0.0.1.54-c).

## BUG-0018 - Negative box-model coordinates wrap to uint16 65520+ (~4095 blocks away) (Discovered by Argon bit-drop review; Fixed by Cortana) - FIXED 0.0.1.55
mesher.cpp emitFace: a vertex below the sub-chunk (box min < 0 in the first cell) is clamped to 0 and one `mesher: box model face reaches ...` warning is logged. Only geometry that was already broken (rendered far away) changes. Shipped models: no warning appeared in the gate runs. Open note: faceUV `local` outside 0..16 can still push u/v outside the rect.

## BUG-0019 - TraceTint doc said 6-bit tint slot; shader reads 5 (Discovered by Argon bit-drop review; Fixed by Cortana) - FIXED 0.0.1.55 (comment only)
renderer.h now says bits 0-4 = slot 0..31. kTintSlots = 32 and appearance.cpp writes ts-1, so 5 bits is correct and no code change was needed. The stale "256-byte slices" comment in vk_renderer.cpp:913 was also corrected (64 since A54-3a).

## Review findings still OPEN (docs/VULKAN_REVIEW_2026-10-16.md, Discovered by Cortana)
- #1 High (rare): a failed vkQueueSubmit after vkResetFences leaves the slot fence unsignaled (hang or 1 s stall), and DEVICE_LOST has no handling.
- #2 Medium: a worker prepareChunkMeshAsync in flight across clearChunkMeshes can publish a stale slice or leak it.

## BUG-0020 - A failed vkQueueSubmit left the frame fence unsignaled (hang); VK_ERROR_DEVICE_LOST had no handling (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56
- Review R1 (High, rare path). vk_renderer.cpp (0.0.1.55 lines 290-303, 586-590): `vkResetFences` ran before `vkQueueSubmit`. A failed submit left the fence unsignaled, so the next `waitSlot` (UINT64_MAX) hung. DEVICE_LOST was only logged and the loop kept going.
- Fix F56-1: on submit failure, an empty recovery batch waits on the acquire semaphore and signals the fence, and the swapchain rebuilds next frame. DEVICE_LOST from acquire, submit, present or the fence wait sets `deviceLost_` and logs one plain message ("GPU device lost ... LithoKraft will close normally. Update the GPU driver or try --renderer gl33"). Rendering then stops, and `IRenderer::deviceLost()` lets main.cpp close through the normal shutdown path. The signing fatal-exit path was not touched.

## BUG-0021 - Worker direct upload in flight across clearChunkMeshes could publish a stale slice or leak it (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56
- Review R2 (Medium). Fix F56-2: `meshEpoch_` (atomic) is stamped into `PreparedChunkMesh::epoch` before the slice is allocated. `clearChunkMeshes` bumps it. `publishChunkMesh` frees an older-epoch slot instead of drawing it, and logs this once.

## BUG-0022 - Unbounded staging overflow growth while the window is minimized (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56
- Review R3 (Medium). Streaming keeps uploading while `renderFrame` is not called. Each device-local upload took a fresh 32 MB overflow buffer, and those are recycled only after a submit.
- Fix F56-5: past the ring plus 4 overflow buffers, the slot's pending copies are flushed with one blocking submit (the slot is idle) and staging restarts.

## BUG-0023 - Screenshot readback had no TRANSFER_WRITE -> HOST_READ barrier (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56
- Review R4 (Low). F56-3 adds the buffer barrier after the grab copy. Pixel-identical (tex_closeup 0.000% vs 0.0.1.55-b).

## BUG-0024 - CPU spin with a zero surface extent (minimized) (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56-b
- Review R6 (Low). Every frame ran vkDeviceWaitIdle plus a caps query with no pacing. F56-8: `recreateSwapchain` sleeps 16 ms when no swapchain can be made.

## BUG-0025 - Meshes over kMaxQuads were truncated silently (Discovered by Cortana review, Fixed by Cortana) - FIXED 0.0.1.56-b (log only)
- Review R8 (Low). F56-9: one `vulkan: sub-chunk ... has N quads; only the first M are drawn` warning per session. Drawing is unchanged. No warning appeared in the gate or bench runs.

## BUG-0026 - The LL_SIM_TICK_RADIUS log line from 0.0.1.55 never reached the log (Discovered by Cortana, Fixed by Cortana) - FIXED 0.0.1.56
- It was logged before `logOpenFile`. F56-6: after the log opens, main.cpp logs `sim tick radius N chunks (from --radius|settings.json|LL_SIM_TICK_RADIUS), square|round area (from ...)`. game.cpp logs `sim: tick area radius N, square|round ..., C chunk columns` at every sim start. Proof: every 056 bench log has both lines with the env value.

## BUG-0027 - Second LithoKraft instance: "ERROR could not open log file", and the gate read the other instance's log (Discovered by Cortana, Fixed by Cortana) - FIXED 0.0.1.56-d
- Cause: core/log.cpp opens `logs\lithokraft.log` deny-write, so while one LithoKraft runs (for example JD playing), every other instance logged `ERROR could not open log file ...; console only`. The gate counted ERRORs in the shared file, which belonged to the other instance. Reproduced on 7/7 shots with a concurrent bench (p2logs\noise).
- Fix F56-11: main.cpp falls back to `logs\lithokraft.second-<ms>.log` with a WARN. gate.ps1 gives each shot its own `--log p2logs\gates\<tag>_<shot>-shot.log` and reads only that file (a missing file is an ERROR).

## BUG-0028 - Gate shot noise: the actual text, and its classification (Discovered by Cortana; classified per JD's rule) - RESOLVED 0.0.1.56-d
- The gate now prints the ERR text. Text captured on 056d_night (00:58 ET): `ERROR [script] mod throttled after 'void onLoad()': CPU quota exceeded (1174 us > 1024 us/tick budget, overrun #1); hooks skipped for 8 ticks`. The same message appeared in 053i_stream64 (7255 us).
- Why it is benign: the only script is `scripts\hello.as`, a one-line `onLoad` greeting with no tick hooks. Its first (cold) run sometimes costs a little over the 1024 us per-tick script budget, so the budget logs a first-overrun throttle and "skips" hooks that do not exist. Shots and hashes are unaffected.
- Not changed: the script CPU budget policy (sandbox policy is JD's call; see the open items).
- Gate F56-12: only that exact message (onLoad, overrun #1) is printed as `KNOWN-BENIGN (not counted)`. Any other ERROR still fails the gate, including a second overrun, a suspension, or a throttle in any other hook.
- 0.0.1.55 a/b/c: their text was never captured. Each was either this throttle or the shared-log issue in BUG-0027 (JD was playing then).


## BUG-0029 - Starting a second LithoKraft stalled the running game's simulation (dropped ticks; sometimes a freeze until resume) (Discovered by Cortana, Fixed by Cortana) - FIXED 0.0.1.57-a
- Review R21. Pre-existing (0.0.1.53, 0.0.1.55, 0.0.1.56; radius 8/16; square and round).
- Cause: CPU starvation. A second instance's startup (world bake, streaming, 8 render threads) saturates all 12 logical CPUs, and the running game's simulation threads sat at normal priority. As a background window on the hybrid i5-12450H they were also eligible for Windows power throttling. When the process was BelowNormal (every gate/bench launch inherits that), they could wait 250-600 ms. A latent race in a sim stall check could also misfire after a long wait.
- Fix:
  - F57-1: the process opts out of background power throttling; the sim threads run one step above normal and opt out of thread power throttling.
  - F57-2: the stall check re-tests completion before acting.
  - No change to tick rate, determinism, or the signing path.
- Verify: instance A runs the test world at 512 Hz for 50 s at normal priority; B (a stream bench) starts and exits twice.
  - Before (0.0.1.56-d): 398 dropped ticks, main-thread frames up to 1.76 s.
  - After (0.0.1.57-a): 0 dropped, min 504 tps, max frame 481 ms, 0 freezes.
  - With A forced to BelowNormal: 3512 -> 218 dropped (expected: strict OS priority below normal).
  - Gate: FULL PASS (tag 057a): selftests 1344/0, hashes cb32c824f8b3ea98 / e015e51cf1559089 x2 each, round-area r4 cb32c824f8b3ea98 / 966be55811fb23a7 x2, 7 shots 0 errors (1 known-benign onLoad throttle line, not counted)
- Second-instance policy: still allowed (gate shots, benches and tools launch separate instances). Since 0.0.1.56-d it logs to its own file.

## BUG-0030: Vulkan blended bulb pass drew back faces (doubled "floating" crosses)
- Discovered by Cortana (Argon (Archie) led the investigation brief); Fixed by Cortana. Build 0.0.1.58-a-blendcull.
- Symptom: red and blue "+" crosses with halos float near Trace Bulbs, in the sky above trace trees and smeared across nearby block faces. Vulkan only, with the blended bulb pass on (--vkfx bit 64).
- Repro (bulbs_on pose, 0.0.1.57-a): turning particles off (127 -> 111) changes 2085 px (only the tiny sky sparks). Turning bloom off (123) leaves the crosses (halos only). Turning the blend pass off (63) and GL33 both show one cross per bulb in the right place. So the source is the Trace Bulb shell faces in the blend pass, not particles or foliage.
- Root cause: vk_pipelines.cpp built `chunkBlend` from a copy of `chunk` after `chunk.cullBack = false` had been set for the entity pipeline. The blend pass was therefore double-sided. Each bulb cube (ghost texture: disc + painted cross on all 6 faces) showed its far faces through its near ones without depth writes or per-face sorting, giving extra, offset crosses and flat bottom-face streaks.
- Fix: `blend.cullBack = true` (same CCW front face as the opaque chunk pass).
- Verify: after the fix the sky row shows exactly one cross per bulb, matching the blend-off and GL33 positions. VK blend-off frames vs 0.0.1.57-a: 127 px (sparks). GL33 frames are pixel-identical. Full gate passes with hashes held.
- Not changed (for JD): Argon's proposal (a), "shell outA <= 0.12, no glow mask; only the inner core glows". It is confirmed that the lens glow mask (fxGlowR, up to 0.92 alpha) stamps a flat glowing disc on every shell face. But the in-world Trace Bulb is a plain cube (trace_tip_red/blue, pack faces = *_trace_bulb_ghost). The glass-core .lthm model is only loaded by a selftest, so there is no inner core geometry to carry the glow. Applying (a) now would leave the bulbs almost invisible and dark. That needs a decision plus core meshing first.

## BUG-0031: LL_VK_PACKED12 "off"/"false"/"no" switched Packed12 ON (0.0.1.59-a)
- Discovered by Cortana (review), Fixed by Cortana. Fixed in 0.0.1.59-b. Severity Low. Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; layout correction and build by Cortana.
- **Cause:** the toggle only treated a leading '0' as off, so any other non-empty value meant on.
- **Fix:** the value is parsed: 0/off/false/no means off, 1/on/true/yes means on; anything else keeps the default and logs a warning.
- **Verify:** launches with off, false, 1, on and bogus logged packed16, packed16, packed12, packed12, and packed12 plus the warning.

## BUG-0032: misleading header comment in the Vulkan chunk vertex shader (0.0.1.59-a)
- Discovered by Cortana (review), Fixed by Cortana. Fixed in 0.0.1.59-b. Severity Info. Proposed by Argon (Archie), Chief Optimization Officer and Chief Optimization Operand, the Bitmaster and the Bitmasker; layout correction and build by Cortana.
- **Cause:** the comment said "12-byte PackedVertex". The CPU PackedVertex is 16 bytes; the shader reads its 12-byte prefix, or the real 12-byte Packed12 vertex.
- **Fix:** comment reworded (chunk_vert_body.glsl). No code change.

## F59-6: Packed12 stream-bench GPU ms looked higher (0.0.1.59-a/b) - CLOSED, not a bug
- F59-6 - Discovered by Cortana (review), Investigated by Cortana: artifact of single-window GPU sampling on a streaming bench; Packed12 is ~17% lower GPU ms at matched draw counts (121/121 windows); no code change.
- **Cause of the false alarm:** the verify `gpu=` was only the last 120-frame `vulkan perf:` window. On a streaming bench GPU ms climbs with draws, and the two runs' last windows ended at different load points (packed16's previous window was 3.15 ms at 2,069 draws).
- **Evidence:** A/B 2026-10-17 06:00-06:16 ET: 14 interleaved on/off pairs. At matched draws Packed12 is -1.7 ms at r8 round default and in the test world (fps 93.0 vs 85.9) and -3.7 ms at still r64 (fps 75.6 vs 66.7, p99 25 vs 43 ms; 16-byte spills 416 MB to host). Stream r64 shows no significant difference. Write-up: docs\F59-6_GPU_TIME_2026-10-17.md.
- **Follow-up:** BENCH lines now also report the whole-run GPU average and median, plus the last window's draw count (`engine\tools\vk-perf-summary.ps1`; Proposed by JD, Built by Cortana).


## 0.0.1.61 review (2026-10-17): consolidated build plus the experimental render-thread split (LL_THREEWAY, default off)

## BUG-0033: screenshot capture ran on the render thread and touched main-thread game state (LL_THREEWAY=1 only)
- Severity: Mid. Found by Cortana, fixed by Cortana (B61-1). Fix proposed by Cortana. Status: FIXED in 0.0.1.61. Date: 2026-10-17.
- **Cause:** with the split on, capturePendingScreenshot() ran on the render thread after renderFrame and wrote to the console, Logbook gallery and read the player pose without the main thread's ownership.
- **Fix:** the capture now runs on the main thread while the render thread is idle, one frame after the shot is armed, so it reads the armed frame (main.cpp RenderWorker; Game::screenshotPending() in game.h). The first version captured one frame too early and lost the shot; caught in testing and corrected before the final 0.0.1.61.
- **Verify:** --ui-shot PNG written with LL_THREEWAY=0 and =1; gate shots 7/7 with both.

## BUG-0034: a render-thread exception closes the game (std::terminate) with no exit save (LL_THREEWAY=1)
- Severity: Mid. Found by Cortana; save risk raised by Herominde; fix proposed by Master Chief. Status: OPEN (no fix in this pass). Date: 2026-10-17.
- **Cause:** an exception escaping renderFrame on the render thread terminates the process. (With the split off, the same exception on the main thread is also uncaught, so this is not new risk, but the split adds a second thread that can end the process at any moment, including while the main thread is writing level.json or player.lat.)
- **Save-path trace (Cortana, read-only):** every file write is crash-safe on its own. level.json and player.lat use writeFileAtomic (temp file, fsync, backup rotation to .bak.N, rename); region files are rewritten whole by the WorldStore I/O thread through the same contract; open() runs recoverCrashedWrite, which heals the rotation window. A half-written file is never left as the live copy. Remaining risks: (1) unsaved progress since the last save is lost (autosave every 300 s; the exit save never runs); (2) saves are not atomic across files: an async autosave writes player.lat right after queueing the region writes, so a crash in between can leave a new player.lat with older regions; (3) if the crash lands between backup rotation and the commit rename, recovery promotes .bak.1 (the previous version) even when the new .tmp is complete, so that region reverts one save.
- **Exploit (found by Herominde):** a crash between the player-file write and the region flush can duplicate items, the classic crash-to-dupe. The async autosave writes player.lat (inventory) right after queueing the region writes; if the process dies before the regions land, the new inventory is kept while the world reverts, so items moved from containers or the world into the inventory exist twice. A render-thread exception with LL_THREEWAY=1 is one way to trigger that crash.
- **Fix proposed by Master Chief:** catch the render-thread exception, hand it to the main thread, log it, and fall back to the single-thread path with THREEWAY off. Plus (suggested by Cortana): write player.lat only after the region flush succeeds. Status: OPEN, not implemented. Updated 2026-10-17.

## BUG-0035: streaming column inserts can land mid catch-up burst (LL_THREEWAY=1)
- Severity: Low. Found by Cortana; lag-burst test proposed by Herominde, implemented by Cortana. Status: TESTED, no divergence; kept open for design review. Date: 2026-10-17.
- **What:** with the split on, the sim lets a waiting insert or point read in between whole ticks of a catch-up burst, instead of only after the burst.
- **Test:** measure-only LL_SIM_STALL_MS=300@1536x8 (sim holds the block lock 300 ms at world tick 1536, then every 128 ticks, 8 times), hash at tick 3072 over fixed column sets around chunk (3,2). Split off: 0 mid-burst inserts (355 to 378 between iterations). Split on: 281 and 329 mid-burst inserts, the first at ticks 1540 and 1539 inside the 1536..1544 burst. All 4 runs: signal 0484786e66b9e918, r4 76c5702da72a2771, r8 61281594fe5c3dfb, r16 d8433b31f1e21560, r24 8f33c6f0656efe30. Match. Logs: parked\threeway-split-2026-10-17\round7\lagburst.

## BUG-0036: Alt+Enter display toggle while the render thread is presenting (LL_THREEWAY=1)
- Severity: Mid. Found by Cortana; fix proposed by Master Chief. Status: OPEN, untested. Date: 2026-10-17.
- **What:** the fullscreen toggle runs on the main thread while the render thread may be in present. Out-of-date swapchain is handled, but the path has not been exercised.

## BUG-0037: no debug thread-ownership asserts on renderer and sim entry points
- Severity: Low. Proposed by Master Chief. Status: OPEN (improvement). Date: 2026-10-17.
- **What:** debug-build asserts that renderer-touching and sim-owned calls run on the thread (or under the lock) that owns them would catch the BUG-0033 class early.

## BUG-0038: handoffYield assumes the block lock is held exactly one level deep
- Severity: Low. Found by Cortana. Status: OPEN. Date: 2026-10-17.
- **What:** the between-tick handoff unlocks the recursive block lock once. True on the only caller (the burst loop); a future nested caller would keep the lock held and the handoff would just time out.

## BUG-0039: LL_HASH_AT_TICK hook is not reinstalled after a dimension switch
- Severity: Low (measure-only probe). Found by Cortana. Status: OPEN. Date: 2026-10-17.

## BUG-0040: old credit format in engine\src\core\CMakeLists.txt
- Severity: Low. Found by Cortana. Status: OPEN. Date: 2026-10-17.
- **What:** the lock_timing comment still says "Built by Cortana" instead of the current credit format.

## Speed work list (0.0.1.61, not bugs)
- PERF-61-1 Signal-tick lock wait: flagged by CloudAtlas, cause found by Cortana. Long sim ticks (49 to 56 ms) are 97 to 98% inside the signal/handshake tick, which holds the block lock; main-thread lock wait on stream64 is 4.5 ms/frame (split off) and 2.7 ms/frame (split on), p99 about 50 ms.
- PERF-61-2 vk.record is about 2.7 to 2.9 ms of the 3.1 ms render-thread frame (Vulkan command recording).
- PERF-61-3 The 145 ms hitch: cause not isolated in this pass.
- PERF-61-4 Random ticks: cost not yet profiled separately.
