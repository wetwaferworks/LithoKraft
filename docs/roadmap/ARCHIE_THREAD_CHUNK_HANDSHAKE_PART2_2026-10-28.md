# Part 2: Unified Thread and Chunk Handshake (Wednesday Implementation Brief)

From Archie (co-architect), session of 2026-10-27, for Wednesday 2026-10-28. Companion to ARCHIE_THREAD_HANDSHAKE_BRIEF_2026-10-28.md. Saved by Cortana.

## 1. Root cause (Elfie's 2026-10-26 low-spec playtest)
VM LithoKraft-z16bc, 2 cores, 7 GB, software renderer, View 8, Sim 4. The 512 Hz clock held on average: 503-517 TPS, 0.803 ms avg tick, 1.591 ms p99 against a 1.953 ms budget. Main- and sim-thread starvation during chunk ingestion showed up as:
1. Startup frame spike: 197.88 ms max frame at 1.13 s on the 512 TPS walk; major hitches clustered in the first ~11 s.
2. Warmup tick drops: 76 dropped ticks in the first ~12 s, then flat at 76 for the remaining 33 s. Flyover dropped 70 ticks under heavy upload (48.8 KB/frame, chunks 290 to 335).
3. signal.mark overrun: 3 single-cycle budget overruns at 512 TPS when new chunks forced synchronous cross-chunk boundary evaluation.
4. Distant-glass x-ray: untestable on --renderer software (WARN: PBR transparency tiers not implemented on CPU path; fragments shade opaque), and the scripted path has no distant glass. Verify on Vulkan after Wednesday's block placement.

Bottleneck: background threads push finished chunks back in unpaced bursts, so the main and sim threads stall on spatial map locks, border stitching, signal.mark init and vertex buffer uploads all at once.

## 2. Four-stage lock-free chunk state machine
Each chunk column has one 64-bit atomic state word inside the Thread Handshake:

    [63..48 BorderGen (16b) | 47..32 SimTicket (16b) | 31..16 MeshGen (16b) | 15..8 DirtyFlags (8b) | 7..0 Stage (8b)]

One stage owns a chunk at a time; hand-offs go through lock-free SPSC/MPSC rings.
1. STAGE_GEN (off-thread procgen + internal pre-bake): allocate off-thread, generate wafer blocks (lithokraft:silicon, pillars, crystals, trace lines), pre-compute all internal signal.mark states while isolated.
2. STAGE_STAGED_SIM (budget-gated): pushed to the Sim Ingress Ring. At tick start, only if the previous tick cost < 1.0 ms, the sim thread pops at most 1 chunk by atomic pointer swap, links it into the tick-radius-4 spatial map, and queues its 4 outer borders into a lazy ring for signal.mark stitching.
3. STAGE_MESH (off-thread greedy/packed quads): builds software span buffers / Vulkan SSBO packed quads without locking neighbors, using atomic BorderGen tags and NEIGHBOR_DIRTY bits for lazy edges.
4. STAGE_STAGED_RENDER to STAGE_ACTIVE (time-sliced): pushed to the Render Ingress Ring. Main/render thread pops at most N chunks or K KB per frame via std::atomic::exchange; old mesh pointers go to a Deferred Free Queue so free() never runs mid-frame.

## 3. Thread hotness and dynamic swarm dispatch
One persistent pool of pre-warmed dispatch threads, owned by the Thread Handshake (no fixed render vs background split):
- Pre-warmed at boot: spawned, pinned and parked on the barrier during the title screen; memory slabs pre-allocated for tick radius 4 (81 chunks) and view radius 8 (289 chunks).
- 100% swarm on Play: no geometry yet, so all dispatch threads run STAGE_GEN to STAGE_MESH.
- Single-core pipeline locality: the thread that runs STAGE_GEN for a chunk keeps it and runs signal.mark pre-bake and STAGE_MESH while data is hot in L1/L2. No mutex hand-off, context switch or eviction.
- Frame slack harvesting (cruise): threads that finish raster strips early or sit between ticks are lent to drain GEN/MESH until the next frame/tick barrier reclaims them.

## 4. No-loading-screen Tron wafer materialization
CPU gen runs full speed; the visual reveal (STAGED_RENDER to ACTIVE) is paced separately.
1. Origin pedestal (frame 1): player in a purplish-black void (0x0A0412) at origin (0,0), the corner of chunks (0,0), (-1,0), (0,-1), (-1,-1). Only the 4 corner blocks under the player are visible: valid floor collision and 360 look with no skybox cost.
2. Intra-chunk etch: traces and blocks spread from those 4 blocks to fill the 2x2 chunk courtyard (-1,-1)..(0,0).
3. Macro Chebyshev spiral: remaining chunks ordered center-out (R = max(|dx|,|dz|), angle theta). The Render Reveal Pacer flips chunks ACTIVE along R(t) = v_etch * t. Vulkan: each chunk stores spawn_timestamp in its metadata SSBO; for the first 0.4 s of age, shaders apply a sub-block vertical snap-in and a thin neon UV laser etch pulse, settling into static PBR lithokraft:silicon and translucent crystal/glass. In --bench, v_etch = infinity for instant, hitch-free benchmarks.
4. Skybox ignition (SKYBOX_ONLINE): only after the last view-radius-8 chunk locks in, SKYBOX_ONLINE = true; stars flicker on, then the Sun and Moon (the overhead lithography UV array) sweep rim lighting and cleanroom shadows across the normal-mapped PBR wafer.
