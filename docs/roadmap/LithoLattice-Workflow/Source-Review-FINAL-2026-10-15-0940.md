# LithoLattice / LithoKraft: Consolidated Source Review, FINAL (2026-10-15 09:40 EDT)

READ-ONLY review. No code was changed. This report supersedes the pass reports at 0902, 0909 and 0919, and the working notes; it merges all six passes.

Each finding gives file:line, the problem, a suggested fix, and **Det** (whether it touches determinism).

Determinism rule applied (JD ruling): strict determinism is required only for the sim (scenario hash `aa82e7afb1eff9d3`) and the Trace circuits. Worldgen may vary between platforms, so libm transcendental calls are LOW unless they feed the sim or circuitry.

---

## 0. Executive summary

- **Critical:** none. No finding breaks the sim hash or the Trace circuits. No finding is remotely exploitable without a granted token.
- **Highest-impact problems:**
  1. The alt-tab mouse re-capture bug.
  2. A one-command sim freeze (`query` in the console / `world_query_blocks` in MCP) caused by uint64 overflow in the cell cap.
  3. The console `coerce` dangling pointer. This is the Windows Debug test failure.
  4. Region saves are not crash-safe on in-place rewrite.
  5. Entity meshes are emitted in the wrong units, so items and drops draw in the wrong place on GL33 and software, and not at all on Vulkan.
  6. Mesher thread scaling.
  7. Software renderer correctness and cost.
- **Verdict on the architecture:** it is sound. Fail-closed storage, deny-by-default permissions, a deterministic tick core and loud-once warnings are consistent habits across the codebase. Most defects are edge holes at boundaries (overflow, lifetime, Windows-only paths), not structural problems.

---

## 1. Sim core, threading, handshake

**Critical:** none. The handshake publish/barrier/commit, the stamped control stream and the master-step body are internally consistent.

### High
- **SIM-H1** The game thread blocks on `blockMutex_` for an entire catch-up burst of up to 16 steps.
  - Where: sim_thread.cpp:565-634. getBlock callers at game.cpp:82, 1822, 1850.
  - Effect: frame hitches.
  - Fix: release the lock between master steps, or publish a per-tick read view (seqlock/COW).
  - Det: no.
- **SIM-H2** The hold path causes a catch-up burst after a stream insert.
  - Where: game.cpp:928-993; sim_thread.cpp:583-586.
  - Fix: make `holdBetweenTicks` subtract the held time from `acc_`.
  - Det: no (affects ticks per wall-second only).

### Medium
- **SIM-M1** Handshake workers wake on every publish, even with no due work, then spin for 50 µs.
  - Where: timing_handshake.cpp:546-556, 498.
  - Fix: skip `notify_all` when activeShards==0.
  - Det: no.
- **SIM-M2** `runPhase` makes two vector allocations on every phase.
  - Where: timing_handshake.cpp:519-520.
  - Fix: make them members.
  - Det: no.
- **SIM-M3** Resetting the stale-worker diagnostics races with in-flight `runItem` calls, so the watchdog can name the wrong component.
  - Where: timing_handshake.cpp:522-525.
  - Det: no.
- **SIM-M4** `takeChangedColumns()` has no lock. It is only correct when the sim is stopped.
  - Where: sim_thread.h:188.
  - Fix: take `blockMutex_` inside it. Also see SAV-M1 (the dirty set is lost on a failed save).
  - Det: no.
- **SIM-M5** `signal.cpp` copies `mountPowered_` every tick and calls `collectMountPowered` twice.
  - Where: signal.cpp:402, 375/403.
  - Fix: compute it once per tick.
  - Det: no (iteration is order-independent).

### Low
- **SIM-L1** The `blockMutex_` release/reacquire hand-off is unfair on Windows.
  - Where: sim_thread.cpp:633-638.
  - Fix: use an explicit hold-request flag.
- **SIM-L2** Chase–Lev slot read/write is a formal data race.
  - Where: chase_lev.h:75-77, 145-146.
  - Fix: store indices, not job objects.
- **SIM-L3** Possible false sharing in the SPSC queue.
  - Where: spsc_queue.h:45-47.
- **SIM-L4** Stale comment.
  - Where: signal.cpp:163-165 says "stopped"; it is now "held".

### Strengths worth keeping
- Tick-stamped control stream.
- Explicit hold/stop discipline.
- The freeze watchdog.
- `stepRefusalLogged_`/`freezeReported_` reset hygiene, verified in pass 1.

---

## 2. Game (game.cpp, 2,519 lines, all read)

### High
- **GAME-H1** The entity mesh is emitted in the wrong units.
  - Where:
    - game.cpp:2455-2457 and 2504-2506 write absolute world positions in **whole blocks** as `uint16_t`.
    - Every renderer decodes `aPos` as **1/16 block, sub-chunk-local**: renderer.h:40; software_renderer.cpp:93-94; GL draws entities with `uOffset = 0` at gl33_renderer.cpp:590.
  - Effects:
    - Items and deceitor drops render at 1/16 of their true position, near the world origin.
    - Sub-block quad corners truncate, so the quads collapse to zero area.
    - Negative coordinates go through float→uint16 conversion, which is UB.
    - Vulkan draws no entities at all (vk_renderer.cpp:128-133).
  - Fix: emit `(pos - origin) * 16` relative to a per-batch origin and pass the origin through `uOffset`/meshOrigin. Alternatively, add a float entity vertex format. Clamp to range and grow the GL VBO (GL-M1).
  - Det: no.
- **GAME-H2** (pass 1, confirmed by JD on Release/Windows) The mouse never re-captures after alt-tab.
  - Where: game.cpp:1369-1370 auto-pauses on focus loss; game.cpp:1776-1777 swallows every world click while paused; game.h:138 means the cursor stays free. main.cpp:887-893.
  - Fix: a click in the world (or focus regained) resumes play. Alternatively, drop the auto-pause, compare the real cursor mode, and make Esc toggle.
  - Det: no.

### Medium
- **GAME-M1** A failed autosave is retried every frame.
  - Where: `lastSave_` is set only on success (game.cpp:902, 1151).
  - Effect: a persistent failure, e.g. the refused-load fail-closed state that 776-779 warns about, causes a sim stop/start and log spam every frame.
  - Fix: stamp `lastSave_` on every attempt and add backoff.
  - Det: no (it does disturb tick pacing).
- **GAME-M2** `takeChangedColumns()` (game.cpp:873) drains the dirty set before the save outcome is known. See SAV-M1.
  - Det: no. Data-loss risk: yes.

### Low
- `rand()` is used for item-drop velocity.
  - Where: game.cpp:2009-2011.
  - These are game-side entities, not sim or circuitry, so this is LOW under JD's rule. Use a seeded RNG if replays matter.
- genmap writes to an arbitrary path.
  - Where: game.cpp:305-366 via `writeFileBytes`, screenshot.cpp:136-159.
  - It is cheat-gated and needs full authority, but it is not sandboxed the way screenshots are.
- `writeFileBytes` calls fclose twice when fclose fails.
  - Where: screenshot.cpp:152-154.
- `setCheatsAllowed(true)` is unconditional.
  - Where: game.cpp:109.
  - Fix: tie it to a dev build/config.
- Place consumes the item even when the async sim placement fails.
  - Where: game.cpp:1630-1634.
  - Fix: refund on failure.
- Mesh pool jobs read `active_` without synchronisation.
  - Where: game.cpp:568-575.
  - Fix: make it atomic.
- Unchecked `sim_->` dereferences.
  - Where: game.cpp:1137, 1971, 2151.
- Save-on-evict runs synchronously on the main thread.
  - Where: game.cpp:1074.
  - Effect: hitches.
- `stoul` narrowing.
  - Where: game.cpp:487-491.

### Strengths worth keeping
- Disciplined sim stop/hold around streaming and eviction.
- epoch/seq mesh de-duplication.
- Per-frame upload budgets.
- AI-tick and entity catch-up caps.
- The `mcp grant` command is properly cheat-gated (game.cpp:1197-1231).

---

## 3. Saves and storage (world_save.cpp 695 lines, anvil_region.cpp 601 lines, all read)

### High
- **SAV-H1** An in-place region rewrite is not crash-safe.
  - Where: anvil_region.cpp:368-376.
  - When the new record fits the old run, `stage()` overwrites the sectors the index **currently points at**, before `commit()`. A crash mid-write tears the only copy. The CRC detects it, but the column is then Unreadable and its previous version is lost.
  - The sidecar path, `fopen(sp,"wb")` at :349, truncates the live sidecar the same way.
  - This contradicts the file's own payload-first protocol.
  - Fix: always stage into a free run (copy-on-write) and free the old run after the index commit. For sidecars, write `.tmp` + fsync + rename.
  - Det: no.

### Medium
- **SAV-M1** The dirty set is lost on save failure (verified this pass).
  - game.cpp:873 drains `takeChangedColumns()` before calling `saveDimension`.
  - `saveDimension` can return early without writing anything:
    - fail-closed name-table refusal, world_save.cpp:258-263;
    - `writeBlockNames` failure, :286-293;
    - `writeWorldMeta` failure, :297-302.
  - Those columns' dirty marks are gone. They are only re-saved on eviction or a later full save, so a crash before then loses the edits.
  - Also, with `wait=false` (autosave), `lastSaveOk_` reports true before any I/O has happened (:249, :316). An async flush failure never reaches the game (WorldStore keeps pending records, but only in memory).
  - Fix: re-merge the dirty keys when `lastSaveOk_` is false, and surface async flush failures through a completion flag.
  - Det: no.
- **SAV-M2** `exporting_` is shared across dimensions.
  - Where: world_save.cpp:305-307, 376-378.
  - Each dimension's WorldStore has its own I/O thread, so a clear from dimension A can erase B's SGNL export mid-encode, and the column saves without signal state.
  - Fix: make the export state per-store, or capture it by value in the encode job.
  - Det: no. It affects circuit state persistence, though not the live sim.
- **SAV-M3** Every save deep-copies every non-air SubChunk and re-reads `blocks.names`.
  - Where: world_save.cpp:258, 265-283.
  - Fix: copy only dirty columns and cache the remap check.
- **SAV-M4** file_contract does not `_commit` on Windows, so writes are not durable. anvil_region.cpp:70-72 already does it.
  - Fix: mirror anvil.
- **SAV-M5** 18 narrow-path `fopen(path.string().c_str())` sites fail on non-ASCII Windows profile paths.
  - Examples: anvil_region.cpp:249, 254, 349, 406, 529.
  - Fix: `_wfopen` / `std::filesystem` streams.

### Low
- `WorldSave::flush()` iterates `stores_` without `storesMutex_`.
  - Where: world_save.cpp:440-443.
- `readPlayer` does not reject non-finite position/yaw/pitch.
  - Where: world_save.cpp:591.
  - A CRC-valid NaN position puts the player in an undefined state.
- `fileSize()` ignores seek/tell errors (-1 becomes a huge size).
  - Where: anvil_region.cpp:42-50.
- `open()` has a TOCTOU between `exists` and the `w+b` create.
  - Where: anvil_region.cpp:249-255.
  - Fix: use `"wx"`-style exclusive create.
- `allocRun` is a linear scan of every sector per write, and the commit verify re-reads each record (2× I/O).
  - Where: anvil_region.cpp:293-307, 399-430.

### Strengths worth keeping
- Fail-closed block-name table: an unreadable table never lets generated terrain overwrite saved data.
- Commit order of payload, then verify, then timestamp, then 4-byte location entry. Malformed index entries are quarantined and counted.
- Torn-tail truncation is provably safe.
- `player.dat` v4/v5 uses string ids, has CRC, write-side count guards and exact-consumption reads.
- The generation counter resumes on open.

---

## 4. Rendering: software renderer and mesher

### High
- **RND-H1** The mesher takes a `shared_mutex` per voxel, so it gets slower with more threads (about 4× slower on 12 threads).
  - Where: block.h:717,725 → `blockDef()` shared_lock (352/510/640); mesher.cpp:109; mesh_input.cpp.
  - Fix: take one per-job immutable DefTable snapshot and use a flat flags LUT.
  - Det: no.
- **RND-H2** The software renderer has no near-plane clipping, so geometry next to the camera disappears.
  - Where: software_renderer.cpp:91-110.
  - Fix: homogeneous clip against w=near.
  - Det: no.
- **RND-H3** The rasterizer is slow (the 134 ms/frame lever).
  - Where: software_renderer.cpp:146-178, 217-304.
  - Problems: per-pixel divides, no incremental edges or tiles, affine UVs, per-fragment parameter rebuilds.
  - Fix: invArea, fixed-point edges, 64×64 tiles on the JobPool, perspective-correct varyings.
- **RND-H4** There is no front-to-back sort; meshes draw in std::map key order.
  - Where: software_renderer.h:239-240.
  - Fix: sort visible chunks by distance.

### Medium
- **RND-M1** SSAA2x reallocates the framebuffer and depth buffer every frame.
  - Where: software_renderer.cpp:1038, 1088-1093.

### Low
- The strip tint is "reddish" for both strip modes in flat mode.
  - Where: software_renderer.cpp:197-211.
  - Verify red_via/blue_via with per-pixel captures.
- `swMeshKey` drops the top 8 bits of cx.
  - Where: software_renderer.cpp:481-484.
- `uploadChunkMesh` copies instead of moving.
- The legacy `applyFXAA` path is dead code.

### Strengths
- Shared overlay geometry across backends.
- A single PackedVertex contract (now documented in renderer.h).
- The cull-proto pipeline defaults off.

---

## 5. Vulkan backend (engine/src/render_vk, ~2,031 lines)

### Medium
- **VK-M1** A submit failure hangs the next frame forever.
  - Where: vk_renderer.cpp:201-214. The fence is reset at 201; if `vkQueueSubmit` fails, the next `waitSlot(UINT64_MAX)` (441) never returns.
  - Fix: reset the fence only after a successful submit, or re-signal it with an empty submit on failure.
- **VK-M2** Feature gaps.
  - Entities are invisible (128-133).
  - PBR meshes and materials are dropped (159-166).
  - They are logged once, which is good, but a Vulkan user sees no items or drops. Pair this with GAME-H1.

### Low
- The grab barrier uses `srcAccessMask = 0` after COLOR_ATTACHMENT_OUTPUT (568), so the colour writes are not made available to the copy.
  - Fix: `VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT`.
- `grabReady_` is never cleared after a read, so `grabFramebuffer` can return the previous screenshot if called before the armed frame renders (231-237).
- The results of `vkBeginCommandBuffer`/`vkEndCommandBuffer` are unchecked (513, 542).
- Mip generation blits with `VK_FILTER_LINEAR` without checking the format's BLIT/LINEAR features (vk_texture_array.cpp).
- `LoadLibraryA("vulkan-1.dll")` uses the default DLL search order (vk_loader.cpp), which allows DLL planting.
  - Fix: `LoadLibraryExA(..., LOAD_LIBRARY_SEARCH_SYSTEM32)`.
- `renderDone_` semaphores are destroyed after `vkDeviceWaitIdle` while a present may still be waiting on them (recreateSwapchain 420-433).
- Chunk meshes over `kMaxQuads` are clamped silently (150).
- The vertex arena is first-fit and linear-scan, and its blocks are never released (vk_vertex_arena.cpp).
- The sprite page upload assumes every Image is RGBA8 w*h*4 (661).

### Strengths
- RAII handles with destruction order documented by declaration order.
- Per-slot staging rings with deferred retire keyed to the fence.
- A swapchain rebuild that tolerates minimise.
- Device scoring that prefers a native driver over a layered one.
- Validation messenger wiring.

---

## 6. OpenGL 3.3 backend (gl33_renderer.cpp 890 lines, all read)

### Medium
- **GL-M1** The entity VBO is fixed at 4096 vertices and truncates silently.
  - Where: gl33_renderer.cpp:450, 460.
  - Fix: grow it like the sprite path (175-182) and warn once. Fix GAME-H1 first.

### Low
- The PBR mesh key at :322 uses a different scheme from the classic key at :467.
  - Signed shifts are defined under C++20, but sign extension lets a negative cx clobber the sy bits. Collisions only occur at implausible sy values.
  - Fix: use one shared `meshKey()` helper.
- `frameCull_` is an `unordered_map` allocated every frame (538).
- There is no GL error or debug-output checking anywhere. In particular, a failed `glBufferData` in `growArena` (766) leaves the bookkeeping larger than the real buffer.
- `copySceneColor` (`glCopyTexSubImage2D`, 313) fails on a multisampled default framebuffer.
- Animation uniform arrays are written without a bound against the shader array size (719-724); the overflow is silently ignored.

### Strengths
- Shader reload is atomic: everything is built, then swapped only if all succeed, and the optional programs degrade cleanly.
- A single arena VBO with base-vertex draws.
- The cull decision is made once per chunk per frame.
- Grow-only sprite streaming.

---

## 7. Procgen / worldgen (7,029 lines, all read)

### High
- **PG-H1** Chunk-seam bug: structures, stamps, trees, rocks, cans and spires take their base height from a height clamped to the current chunk.
  - Where: stages.cpp:989-995, 1427-1432, 1453, 1484-1486, 1791-1796; diversity.cpp:106-113.
  - Effect: features that span chunks are split or step-offset at the border.
  - Fix: evaluate the height graph at (bx, bz), the way RiverCarver's `H()` does.
  - Det: no.

### Medium
- **PG-M1** RuinsCarver passes the start z to `tryPlace` instead of the current z.
  - Where: stages.cpp:784 `tryPlace(x, sz, …)` should be `tryPlace(x, z, …)`.
  - Effect: traces have gaps, and the void step (820-829) punches holes.
  - The trace routing itself is integer-only and platform-identical. Fixing this changes worldgen output, so re-baseline the worldgen goldens.
- **PG-M2** RiverCarver re-traces every river per chunk (stages.cpp:1582-1588, 1661-1678).
  - Fix: a per-cell cache.
- **PG-M3** WormCaveCarver replays every worm per chunk without culling (stages.cpp:338-394).
  - Fix: a bounding-box precheck plus caching.
- **PG-M4** Per-voxel hot spots:
  - WorleyOreVein does 54 hashes per voxel (diversity.cpp:213-236);
  - `NoiseNode::eval` allocates per call (graph_nodes.cpp:82);
  - OpenSimplex2S 4D does 256 lattice evaluations per sample.
- **PG-M5** The StructureRegistry singleton is unlocked, and it returns pointers into a vector that can reallocate (struct_registry.cpp). This is latent.

### Low
- **PG-L1** Worldgen makes 59 libm transcendental calls. They are LOW per JD's ruling.
  - Caveat: if the TraceTreeFeature `branch`/`tip_*` materials conduct (stages.cpp:1076-1196), raise this to MEDIUM. Check the generator JSON.
- Cellular noise loops forever on NaN (cellular_noise.cpp:73-77).
- `Rng::range` with hi<lo (seed.h).
- Unchecked `int(getNum())` casts in the loader.
- Fractal normalisation can be 0 (modifiers.cpp:18-23, 68).
- Float world coordinates beyond 2^24 (pipeline.cpp:199-200).
- The graph output index of -1 is unchecked (graph.cpp:586).
- Graph recursion depth (graph.cpp:556-577).
- Fauna tables are parsed but never used. This is the root cause of "mob densities 0": there is no spawner.
- The voxelizer fills concave shapes solid.
- The diversity self-test runs twice (procgen_selftest.cpp:3099, 3104).

### Strengths
- Excellent seed hygiene; generation is chunk-order independent.
- Integer-exact RNG and hashes.
- A fail-closed loader with precise errors and graph cycle detection.
- Sanitising constructors.

---

## 8. MCP server (engine/src/mcp, 4,543 lines, all read)

### High
- **MCP-H1** The `world_query_blocks` cell cap is bypassed by uint64 overflow, which freezes the sim.
  - Where: server.cpp:1017-1026 → game/world_query.cpp:36-56.
  - The backend pauses the sim and loops x to INT32_MAX. Only the `world_read` grant is needed.
  - Fix: check each span before multiplying, add a backend cap, and use int64 loops.
  - Det: no.
- **MCP-H2** The Windows `--mcp-stdio` transport blocks on `std::cin.read(chunk, 8192)`.
  - Where: server.cpp:1912-1918; main.cpp:801-806.
  - Fix: use `ReadFile`/`_read`.
- **MCP-H3** On Windows, `SO_REUSEADDR` lets another local process bind the same port and receive the bearer token.
  - Where: sockets.cpp:132-134.
  - Fix: `SO_EXCLUSIVEADDRUSE`.

### Medium
- **MCP-M1** Single-threaded serving lets a slow-drip client starve every other agent (http_server.cpp:874-885).
- **MCP-M2** `findSession` returns a raw pointer after unlocking, so a concurrent erase is a use-after-free (http_server.cpp:394-413, 269-285).
  - Fix: use `shared_ptr`.
- **MCP-M3** Per-pilot bucket maps grow without bound (server.cpp:1243-1268 and others).

### Low
- IPv6/localhost binds always fail.
- Sockets are not CLOEXEC.
- `stop()` has a close/poll race.
- The WSA refcount is unsynchronised.
- Duplicate JSON keys use first-wins.
- `strtod`/`snprintf` are locale-dependent.
- Requests are parsed 3-4 times.
- Audit log injection.
- Schema and code drift.
- Reversed query bounds silently scan nothing.

### Strengths
- A strict HTTP parser.
- A 256-bit token with constant-time compare.
- Origin/Host checks before auth.
- Deny-by-default agent masks.
- Writes are marshalled onto the sim thread with timeouts.
- Token buckets.
- Full audit log.

---

## 9. Console / lithoscript (engine/src/console, 2,444 lines, all read)

### High
- **CON-H1** A dangling `end` pointer in `coerce` (ASan-confirmed). This is the Windows Debug self-test failure.
  - Where: concmd.cpp:363-365, 373-375.
  - Fix: keep the string alive in a named local.
- **CON-H2** `query blocks|count` has the same overflow freeze as MCP-H1.
  - Where: concmd.cpp:1468-1472.
  - It is **not cheat-gated**.

### Medium
- **CON-M1** `bind f "unbind f"` invalidates the iterator during key-bind dispatch, a use-after-free crash (concmd.cpp:861-872).
  - Fix: snapshot the binds first.
- **CON-M2** The debug console log view takes a whole-document undo snapshot per appended line and copies the full tap every frame (debug_console.cpp:224-244).
- **CON-M3** Alias arguments are re-tokenised, so quoting is lost (concmd.cpp:683-698). That allows command injection through forwarded text.

### Low
- `wait` can resume a tick early (882-913).
- Windows `strtol` is 32-bit.
- `incrementvar` accepts NaN.
- `<cerrno>` is included only on Linux.
- delimiter_pairing.cpp is dead code and comments out a lone `*/`.
- The `consoleCvar` binding reads every cvar.
- Exec completion walks the directory on every keystroke.
- Windows `exec` follows junctions.

### Strengths
- Caller masks are composed by intersection.
- Step, wait and depth budgets.
- Atomic archived cvars.
- The hardened Linux exec open.

---

## 10. UI toolkit (engine/src/ui, 2,980 lines, all read)

### Medium
- **UI-M1** A NumberField containing a lone "-" makes `std::stoi` throw, which terminates the game (controls.cpp:255-260, 293-296).
- **UI-M2** If a pointer-down handler destroys its own widget, `capture_` dangles and the next move/up is a use-after-free (ui_root.cpp:262-266). It is reachable from script UI and slot clicks.
- **UI-M3** Mod panels are not clipped, so a mod can draw fake core dialogs outside its badge (mod_ui.cpp:109-113).
- **UI-M4** ScrollView and Splitter destroy the old child immediately, while one of its handlers may still be running (containers.cpp:160, 224).

### Low
- `emit` iterates while handlers may be added.
- Undo keeps whole-document snapshots.
- Paste bypasses the ASCII filter.
- Popup actions capture `this`, which is a use-after-free if the owner is removed.
- Theme hex parsing accepts garbage.
- Per-frame row and text rebuilds.

### Strengths
- Deferred destruction.
- Mod UI budgets, the badge and strict factory keys.
- Clean focus navigation.

---

## 11. Mods (engine/src/mods, 1,647 lines; all read except mods_selftest.cpp)

Observation: `ModLoader` has **no caller in game/LithoKraft/src**. The permission and grant flow runs only in tests today, so the items below are latent until mods are wired in.

### Medium
- **MOD-M1** Any mod version change without a migrate hook quarantines the mod's save data and disables the mod for that world. Even a patch bump (1.0.0 → 1.0.1) does this.
  - Where: mod_save_data.cpp:207-228.
  - Fix: within the same major version, load the payload with a warning by default and reserve quarantine for declared breaking changes.
  - Det: no.
- **MOD-M2** The default grants store lives *inside* the mods directory (`dir + "/permission_grants.json"`, mod_loader.cpp:326). Anything that can drop a mod folder can also pre-seed a grant record for its own id and version, skipping the prompt.
  - Fix: store grants in the user profile config dir, and/or MAC the file.
  - Det: no.
- **MOD-M3** (pass 3) The mod loader uses a narrow-path `ifstream` (mod_loader.cpp:130) and follows Windows junctions in `scan()` (241-242).

### Low
- `double`→`uint32_t` casts with no range check are UB for hostile JSON. This is the same F-L8 class already fixed for `sdk`.
  - Where: mod_loader.cpp:181 (`permissionSet`); mod_permissions.cpp:94.
- `ExtensionRuntime` measures CPU only after `onTick` returns, so an infinite loop cannot be pre-empted (extension_runtime.cpp:315-325). Static modules only in v1.
- In crash containment, the crashed module's `onUnload` is called unguarded, so a throw there escapes (extension_runtime.cpp:342 → extension_loader.cpp:316-319).
- The `permissionSet` from the manifest is stored, but the grant record always writes `kPermissionSetVersion`. This is harmless, but the two paths are inconsistent (mod_permissions.cpp:190 vs 268).

### Strengths
- Validated ids.
- Duplicate-id refusal.
- Grants bound to id+version.
- Denials persist.
- Load-critical bits refuse the load.
- A firstParty claim alone never elevates.
- The share clamp, depth cap and cycle detection.
- Rollback on partial load.
- Mod save data is CRC-framed and atomic, and orphaned sections are kept.

---

## 12. Script (engine/src/script, 879 lines, all read)

### Medium
- **SCR-M1** The global `operator new`/`delete` replacement applies to the **whole process** (script_memory_hook.cpp:114-174, 185-231).
  - Every game allocation pays a ~24-40 byte header plus shared_ptr construction and destruction.
  - `QuotaAccount::bytes` is a plain `int64_t` (script_engine.h:29) that is credited from whichever thread frees the memory. Strings created by scripts and freed on worker or I/O threads are therefore a data race.
  - Fix: make `std::atomic<int64_t>`. Consider attributing through the AngelScript shims plus a custom string allocator rather than replacing global operators.
  - Det: no.

### Low
- Hook budgets and throttling are wall-clock based (line callback :24-26, `accountCpu`), so whether a hook finishes is machine-dependent.
  - Today hooks run only at load and editor time, so this is latent.
  - If a sim-tick hook ever calls `setBlock`, this becomes **Det: YES** (sim divergence). Before wiring tick hooks, switch to an instruction-count budget.
- `hookMs = 500` is far too long for a 512 TPS tick if tick hooks are ever wired (script_engine.h:144).
- In `rebuildSandbox`, the `assert` (:330) aborts Debug builds before the fail-closed path runs.
- The forbidden-token heuristic has false positives ("profile" contains "file", "environment" contains "env").
- `loadDirectory` has no file-size cap and ignores read errors (393-395).

### Strengths
- Allowlist plus permission-gated bindings.
- A true unbind via sandbox rebuild.
- An exception guard that restores `executingCtx_`.
- Quota attribution through a shared account, with no use-after-free after engine death.
- A suspension policy with backoff, probation and a circuit breaker.

---

## 13. Crypto, platform, sandbox, tests

### High
- **PLT-H1** The Windows mod/file sandbox has no reparse-point or final-path check (sandboxed_io.cpp:123-137). Exec hardening is Linux-only (concmd.cpp:104, 967).
  - Fix: `CreateFileW` with `FILE_FLAG_OPEN_REPARSE_POINT`, then `GetFinalPathNameByHandleW` and check the result against the sandbox root.

### Medium
- Windows private-key protection sets no ACL and does not refuse symlinks (signing.cpp:173-176). `std::random_device` is toolchain-dependent (124).
- The release key is a dev key, and there is no ship guard (main.cpp:205-215).
- Ed25519 has a static-init race (`curve_d`/`base_point`) and is slow (affine coordinates).
- The input timer resolution on Windows is too coarse (input_thread.h:73).
- Hard-coded Linux paths in interchange_capture.cpp:126, 177, 178.

### Low
- Ed25519 is not constant-time when signing (offline tool), accepts non-canonical x=0, rejects S=0, and has a theoretical dropped carry in `fe_reduce`.
- SHA `final()` is not re-entrant.
- Key material is not wiped (signing.cpp:137-139).
- The symlink tests need privileges on Windows.
- systerian uses an out-of-tree path.
- stdout and the logger interleave in self-test output.

### Test count (1238 vs 1239)
- −3: the failing symlink fixture skips 3 dependent checks.
- +1: the Vulkan probe.
- +1: still unattributed.
- Recommendation: print `pass/fail/skip` totals.

---

## 14. Naming (-or sweep)

- 31 files and 59 strings capitalise names ending in -or. Per the naming rule they should be lowercase (e.g. "deceitor").
- The list is in the pass-2 report. It is unchanged.

---

## 15. Architecture perspective

What I would keep, deliberately:
1. **Fail closed by default.**
   - Storage refuses to overwrite worlds it cannot read.
   - Permissions deny unless approved.
   - Loaders refuse rather than guess.
   - This is the single most valuable habit in the codebase.
2. **Deterministic core isolated from presentation.**
   - Tick-stamped control stream, integer RNG and hashes, and the sim/render split.
   - The scenario hash has held through every change reviewed.
   - Keep game-side randomness (drops, AI flavour) clearly outside it.
3. **Loud-once degradation.** Missing backend features log once instead of failing silently (Vulkan entities and PBR, HUD truncation).
4. **Capability-scoped extension.**
   - Mod, script, MCP and console all use masks composed by intersection.
   - The audit trails are consistent across all four surfaces.
5. **Documented fixes in place.** "Fix 2026-10-11 (…)" comments explain why code looks the way it does. Keep doing this.

Where the architecture is thin:
1. **Boundary arithmetic.**
   - The worst bugs are integer overflow at trust boundaries (query spans), lifetime at callbacks (UI capture, keybinds, alias, findSession) and Windows-only paths (stdio, sockets, sandbox, durability, wide paths).
   - Recommendation: one shared "checked box/span" type, plus a Windows CI lane that runs the live MCP and sandbox tests.
2. **Backend parity has no contract test.**
   - The entity unit bug (GAME-H1) survived because nothing renders a known entity and checks the pixels on each backend.
   - Recommendation: a tiny golden-image test per backend (software, GL, VK) with one block, one item and one sprite.
3. **Shared mutable singletons** (block-def registry lock, StructureRegistry, global operator new).
   - Prefer immutable generation snapshots handed to workers, as the mesher fix in RND-H1 would do.
4. **Save path ergonomics.**
   - Durability is excellent at the file level but weaker at the orchestration level (dirty-set handling, async result reporting, copy cost).
   - Recommendation: a single SaveTransaction object that owns the dirty set until the commit is acknowledged.

---

## 16. Consolidated top 10

1. **Alt-tab re-capture** (GAME-H2): game.cpp:1369-1370, 1776-1777; main.cpp:887-893.
2. **Query overflow sim freeze** (MCP-H1 + CON-H2): one shared checked-span fix plus a backend cap in world_query.cpp.
3. **Console `coerce` dangling pointer** (CON-H1): a two-line fix that clears the Debug failures.
4. **Region crash safety and save orchestration** (SAV-H1, SAV-M1, SAV-M4): copy-on-write sectors, tmp+rename sidecars, re-merge dirty sets on failure, Windows `_commit`.
5. **Entity mesh units** (GAME-H1) plus GL-M1 and the Vulkan entity gap (VK-M2).
6. **Mesher per-voxel lock** (RND-H1): a LUT snapshot.
7. **Software renderer:** near-plane clipping, front-to-back sort, rasterizer cost (RND-H2/H3/H4).
8. **Windows MCP pair:** the stdio read (MCP-H2) and `SO_EXCLUSIVEADDRUSE` (MCP-H3). Also the Windows sandbox (PLT-H1).
9. **Crash and use-after-free set:** UI-M1, UI-M2, CON-M1, MCP-M2, UI-M4.
10. **Worldgen correctness:** chunk-seam heights (PG-H1) and the ruins `tryPlace` z (PG-M1), with goldens re-baselined.

---

## 17. Coverage (honest)

Read line by line across six passes:

| Area | Lines |
|---|---|
| sim/handshake/threading core | ~3,400 (pass 1, incl. mesher hot paths, input/focus) |
| software renderer | all ~1,125 |
| SHA / Ed25519 | all |
| sandboxed_io, input_thread | all |
| signal.cpp | 120-439 |
| procgen | 7,029 |
| MCP | 4,543 |
| UI | 2,980 |
| console | 2,444 |
| game.cpp | 2,519 |
| vk_renderer, vk_device, vk_resources, vk_vertex_arena, vk_loader, vk_probe, vk_handles | all |
| gl33_renderer.cpp | 890 |
| mods (excluding mods_selftest.cpp) | ~1,100 |
| script | 879 |
| world_save.cpp | 695 |
| anvil_region.cpp | 601 |

Partially read:
- vk_swapchain.cpp: lines 90-147 were only partly visible.
- vk_texture_array.cpp: about 60 middle lines were cut off in tool output.
- vk_pipelines.cpp: checked for push-constant, blend, depth and cull state only.

Not read line by line:
- mods_selftest.cpp (1,541).
- gl_loader, gl_program, gl_texture_array and software_presenter (~490).
- storage.cpp, file_contract.cpp and async_io.cpp: only the targeted sections.
- audio, ai, entity, anim, texture, subdiv, defrag, index, projectile, player and assets: pattern scans only.
- The selftests other than the failing blocks.

Every area JD named (procgen, MCP, UI, console, game.cpp, Vulkan, GL33, mods, script, saves) has now been read in full, with the small exceptions above.

Verification:
- Root causes marked "confirmed" were verified by ASan (CON-H1) or by tracing the code end to end: MCP-H1/CON-H2, SAV-M1 and GAME-H1 (the decode path was checked in all three renderers).
- Nothing was executed against the live game, and no code was changed.
