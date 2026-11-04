# Engine-wide thread ownership audit: LithoLattice / LithoKraft (2026-10-15, 15:10 ET, Cortana)

**Scope.** Every `std::thread` creation site outside the tests, the cross-thread handoffs between them, and the locks they use. The first pass was the audio deep review JD asked for; JD then widened it to the whole engine, since the game will eventually run on cluster machines. I fixed only clear, safe bugs. The hash `aa82e7afb1eff9d3` was re-checked after the fixes on both SIMD paths.

**Depth, stated honestly.** *Read* means I read the code. *Skimmed* means I only read the headers and comments. No sanitizer was run, because TSan isn't available on MSVC. A Linux `-DLITHOLATTICE_SANITIZE=thread` run is the recommended next step.

## 1. Threads and owners

| Thread | Created at | Owns | Depth |
|---|---|---|---|
| **Main / game** | main.cpp | Window, input consumption, Game::update/render, renderer backends, uploadMeshes(), mesh publish staging (meshBatches_), camera controller, AI host advance, MCP frame consumption | Read (paths touched today) |
| **Sim** | sim_thread.cpp:238 (`SimThread::start`) | chunks_ (ChunkMap) and the tick. Every mutation runs under `blockMutex_` (recursive): the pre-phase drain, ticks, the post phase, flushDirty() snapshots | Read |
| **Job pool workers** (N) | job_system.h:287 | Mesh jobs (meshSubChunk on immutable MeshInput snapshots), worldgen. Output goes to a HandoffQueue (meshOut_, pbrMeshOut_) | Read |
| **IO workers** | job_system.h:299 | Storage IO when a job system is supplied | Skimmed |
| **Storage thread** | storage.cpp:344 (only when there is no job system) | Region flush queue | Skimmed |
| **SW render band pool** (renderThreads−1) | software_renderer.cpp SwBandPool | Rasterizes a frame in strips. run() is serialized by runM_, so the game view and the MCP vision capture take turns | Read |
| **Audio device callback** (miniaudio) | miniaudio_output.cpp | AudioEngine::render(): pops the SPSC command queue, mixes, writes the tap ring | Read |
| **Audio streamer worker** | streaming.cpp:22 | Decodes chunks into a mutex-guarded queue | Read |
| **Occlusion thread** | occlusion_thread.cpp:27 | Raycasts occlusion queries and writes atomic gains | Read (structure) |
| **Input thread** | input_thread.cpp:16 | OS event pump into a bounded queue drained by the tick ("event queue full" logs in the selftest are expected) | Skimmed |
| **MCP HTTP accept/handlers** | http_server.cpp:236 | MCP requests. Handlers must be thread-safe (server.h layering note) | Skimmed |
| **MCP stdio** | main.cpp:811/817 | MCP over stdio | Skimmed |

## 2. Cross-thread handoffs

| From → To | Mechanism | Notes |
|---|---|---|
| Game → Sim (edits, ray, player chunk) | Control stream, drained in the PRE phase under blockMutex_ and stamped (tick, seq) | Deterministic by design |
| Sim → Game (stats, hit, inspect line) | `TripleBuffer<SimStats>`, lock-free | Single reader: `stats()` is "render thread only" |
| Game/MCP → world reads | `SimThread::getBlock()` takes blockMutex_ | Race-free, but contends with the tick |
| Sim → Jobs (remesh) | flushDirty() copies a sub-chunk plus its neighbours into a pooled MeshInput (under blockMutex_), then submit_ | Jobs never touch live world data. **New today:** batch id/size per flush |
| Jobs → Game (meshes) | HandoffQueue → uploadMeshes() on the main thread | **New today:** whole flush batches (2–64 sub-chunks) land in the same frame |
| Jobs → Registry (block defs) | Was a shared_lock per voxel read. **Now** `ContentRegistry::StableDefScope`: a thread-local copy per job, one shared lock per job | Verify mode: 71,555 meshes, 0 mismatches |
| Game → Audio | `SpscQueue<AudioCommand,1024>`, single producer, with a reserve for NoteOff/master gain | Single producer: **only the game thread may call post()** |
| Audio → Game/MCP (tap) | Lock-free ring (tapHead_/tapTail_) | **Single reader**; see flag F-3 |
| Streamer worker → Audio | Mutex-guarded queue plus atomic counters. Cursor resets go through pendingReset_ (applied on the audio thread) | Read; OK |
| Game → Occlusion | Mutex for structural changes, published snapshot, atomic per-query fields | Read; OK |
| Game / MCP vision → Band pool | SwBandPool::run() takes runM_; one frame at a time; run returns only when all bands are done | Capture always sees a fully resolved frame |

## 3. Fixed today (clear, safe; hash unchanged)

| ID | Where | Bug | Fix |
|---|---|---|---|
| T-1 | audio_engine.h | The AudioEngine held a 384 KB tap ring inline, so the whole object was about 400 KB. Stack instances (main(), selftests) overflowed the default 1 MB Windows stack when Release inlining merged frames. **This is the root cause of the 05:11 engine_audio SegFault.** | The ring is heap-allocated once at construction and zeroed. render() still never allocates. The 8 MB `/STACK` (0504) stays as a second guard. |
| T-2 | streaming.cpp | `workerId_` was written by the worker and read by the game thread with no synchronization (a data race, and a possible test flake) | Set from `worker_.get_id()` in play() on the game thread, before any reader |
| T-3 | streaming.cpp | `c.last` was read after `queue_.push(std::move(c))` | The flag is captured before the move |
| T-4 | software_renderer.cpp SwBandPool | A nested run() deadlocked on runM_. A throwing band skipped its `remaining_` decrement, so run() hung forever | Nested calls run inline (thread_local guard). Exceptions are caught per band, the counter always decrements, and the first exception is rethrown after all bands finish |
| T-5 | mesher.cpp jobDef() | The returned reference pointed into a vector that could still grow | Reserved up front for the worst case (kMeshPadVolume distinct ids), so it can never reallocate during a job |
| T-6 | mesher / registry | Per-voxel shared_lock contention: mesh rate at 12 threads was 3.5× slower than at 1 thread | StableDefScope (see §2). Mesh rate 2,154 → 61,479 sub-chunks/s at 12 threads |

## 4. Flags: undocumented sharing and contract gaps (not changed)

- **F-1: Post-tick callbacks run under blockMutex_** (inspect(), activeCount(); Elfie R1§3/§5). They must stay non-blocking and must never call back into code that waits on the game thread, or a deadlock is possible. *Action:* document this in the IWorldSimulation header (mod-SDK hardening). The new meshing hooks don't add any callbacks under the lock.
- **F-2: `SimThread::stats()` is single-reader.** If an MCP handler ever reads it directly instead of through a game-side copy, the triple buffer breaks. *Action:* add an assert on the thread id in Debug.
- **F-3: `AudioEngine::drainTap()` is single-reader** ("Game/MCP thread"). If both the game thread and an MCP thread drain it, they race on tapTail_. *Action:* confirm exactly one caller (the MCP senses adapter), and add a Debug thread-id assert.
- **F-4: `AudioEngine::music()/effects()` accessors** are documented as "tests only, not thread-safe against render()". Nothing enforces that. *Action:* make them private or friend-only, after a grep shows no game callers.
- **F-5: StableDefScope cache key** is (registry pointer, epoch, block count, storage pointer). If a registry is destroyed and a new one lands at the same address with the same values, a stale copy is theoretically possible. Verify mode would catch it. *Action:* add an instance-id counter to ContentRegistry if registries ever get recreated at runtime.
- **F-6: SoftwareRenderer state is per-instance but shared during a frame.** Band threads read the renderer's members (textures, groundFx_, mesh tables) while run() holds runM_. Mutating those setters from another thread during a frame is a race. Today all setters run on the game thread between frames, and the MCP vision renderer is a separate instance. *Action:* document "setters: owning thread, between frames".
- **F-7: Input pump overflow under load:** "event queue full" appears in the selftest. Harmless there, but the queue is bounded with a drop policy. *Action:* confirm the drop policy keeps key-up events, otherwise keys can stick.
- **F-8: The MCP HTTP/stdio handlers** were not traced call by call. Their contract is "thread-safe providers" (server.h). *Action:* a per-tool audit, best done with TSan on Linux.

## 5. Determinism guard

The scenario hash after these fixes, in Release: `aa82e7afb1eff9d3` on both the AVX2 and the forced-baseline paths (`--handshake-scenario 2048 handshake 4 4`). No sim or circuit code changed.

## 6. Follow-up batch (16:25 to 17:30 ET)

- **F-2 (fixed).** `SimThread::stats()` now has a Debug-only single-reader check: the first caller binds, and every later call asserts it comes from the same thread. The audit found a real second reader. The MCP senses adapter's tick callback (`out.tick` in the frame and audio capture, which runs on the MCP thread) called `sim_->stats().worldTick` directly. It now reads `Game::mcpWorldTick_`, an atomic the game thread sets once per `update()`. The tick stamp can now lag by up to one frame, which is within the triple buffer's own staleness.
- **F-4 (fixed as far as safe).** The only game caller, `SoundDirector`, read `music().voiceCount()`. It now uses `AudioEngine::musicVoiceCount()`; voice vectors are never resized after construction, so this is safe from any thread. `music()`/`effects()` stay public because `audio_selftest` uses them, and they are documented as TESTS ONLY.
- **F-5 (fixed).** `ContentRegistry` gets a process-unique `instanceId_` (an atomic counter that is never reused), and it is part of the StableDefScope table key. A registry rebuilt at a freed address can no longer match a stale copy.
- **F-8 (audited; documented).** These are the game-provided MCP seams and the thread each one runs on:
  - `mcpController_`: reads the `mcpControllable_` atomic. Safe.
  - `mcpView_` / `snapshotMcpPlayer()`: copies `mcpPlayerState_` under `mcpStateMutex_`, refreshed by the game thread each `update()`. Safe.
  - `mcpSenses_` tick: was a race (F-2 above); fixed.
  - `mcpSenses_` audio: `AudioEngine::drainTap()` is single-reader (F-3). The adapter is its only caller.
  - `mcpSenses_` frame capture: a separate `SoftwareRenderer` instance (F-6 documents this). The pilot rig is a `shared_ptr<PilotCameraRig>`.
  - Console exec (`IConsoleExec`): the game does not provide it (`b.consoleExec` is null, so the server answers isError). Debug commands such as `tick_step` therefore run only on the game thread.

  Still open: a per-tool TSan pass on Linux, which is not possible on this Windows box.