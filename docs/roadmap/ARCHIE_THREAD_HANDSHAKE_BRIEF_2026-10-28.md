# Wednesday Master Brief: State of main (38ea867) and Thread Handshake Spec

From Archie (Systems Architecture, co-architect). Prepared Monday 2026-10-26 for Wednesday 2026-10-28.
To: JD (Conductor), Cortana (Lead Implementation), Elfie (Bench and Verification).
Saved verbatim in substance by Cortana on 2026-10-27.

## Part 1: State of main (38ea867)
48/48 ctest pass in DebugOpt and Release. Vulkan hashes unchanged: 24c52df2be14789c, 43693d018d046b25.

1. 16-bit depth (uint16 Z + reciprocal remap, 38ea867): Z-buffer 3.69 MB to 1.84 MB at 720p. LL_SW_DEPTH_REMAP=2 improves precision at 256 blocks from ~20 to ~0.06 blocks at 0.0 ms cost (18.06 vs 18.05 ms walk). Classic look behind LL_RETRO_SHIMMER=1. FXAA/CMAA compare raw uint16 depth against integer threshold 164.
2. Parallel HUD strips (LL_HUD_MT=1): 3 translucent panels (alpha 0.45, ~534k blended px/frame) were the post+hud bottleneck. 32-row strips cut HUD 3.95 to 1.87 ms, frame ~2 ms faster, bit-identical.
3. SeqLock camera pose + render interpolation (de16dc1): frozen frames 554/2967 (18.6%) to 0/1150. Fixes the invisible-barrier hitch and F3 lag.
4. Near-plane binning clip (8726864): pixel-identical; stops close blocks blowing up bin/strip traversal.
5. Adaptive background job cap (01b5a12): uncapped on load/pause; 2 stationary (3 on 8+ cores); 1 + yield (LL_BG_YIELD_RENDER) while moving. Flight p99 40.8 to 20.4-20.9 ms; sim tick avg -15-25%; >33 ms frames while streaming 54 to 1.
6. Bench fairness anchors (c74266b): v8 pose (62, 266, 46), pinned shots, freeze tick 1024. Workstation 11-thread baselines (avg / p99 ms): still 19.36/25.45, live 18.13/21.71, walk 18.06/26.52, flyover 18.57/28.24, stream 82.45/162.54.

## Part 2: Archie's locked decisions
1. Item 3, render thread count: Option B. Keep 11 render threads default; commit only the LL_SW_RENDER_THREADS override and Linux sim thread priority. No static cores-2 (cost 15-25% raster). The Thread Handshake census will govern this.
2. Item 6, 64x64 tiles: OFF by default behind LL_SW_TILE64. Slow (1.8-4.7x) because the binner sorted by row only, so ~20 tiles re-ran triangle setup (20x amplification). If revisited, bin post-setup SwTriSetup into 2D bins (bbox_x>>6, bbox_y>>6). The 2x2 pixel diffs come from re-initializing edge/depth equations at X=64 boundaries (base+64*step vs +4*step x16), giving 1-ULP differences on grazing edges.
3. Item 7: pending JD's visual sign-off on LL_SW_DEPTH_REMAP=2 and FLAT_LOD2. If CMAA runs with REMAP=2, retune the depth-edge threshold 164 to the reciprocal curve.
4. Next target, stream (82 ms avg, ~1M tris/frame):
   - Strike 1: sub-pixel quad cull in transform+bin; drop tris with integer bbox min_x==max_x or min_y==max_y before SwTriSetup (40-60% of distant micro-tris).
   - Strike 2: chunk-level frustum + Hi-Z box test of chunk AABBs against the coarse 16-bit depth pyramid before transforming vertices.
   - Strike 3: dynamic streaming LOD pull-in; when queued meshes > 64 or frame tris > 250k in fast flight, pull the FLAT_LOD2 band closer until the queue drains.

## Part 3: Wednesday build, the Thread Handshake (fourth pillar)
Implement in a dedicated test tree first (notes in docs/roadmap/THREAD_HANDSHAKE_IMPL_NOTES.md).
Concept: a deterministic user-space thread hypervisor synced to the 512 Hz (1.95 ms) Timing Handshake clock.

### 1. Boot census and two thread classes
One census at boot (hardware_concurrency()); all OS threads created once, never mid-run.
- Class A, dedicated (pinned lifecycle): Timing Handshake sim clock and render workers (LL_SW_RENDER_THREADS). Stamped spawnTick=0 at boot, returned at shutdown (returnTick=shutdownTick, reason "engine power-down"). On <=4 logical cores, dedicated threads claim first and the pool shrinks.
- Class B, pooled (lend/reclaim): pre-spawned threads parked on a zero-CPU futex/semaphore (POOLED). Spawn = lend, despawn = reclaim. Hand-off is a sub-microsecond atomic ticket stamped with a ThreadRecord (POOLED -> CLAIMED -> RECLAIMED), avoiding the 20-50 us thread-creation tax. Fold the 01b5a12 adaptive cap into the pool's active-claim governor.

### 2. Zero-lock, stream-ordered tick ledger
- spawnTick and returnTick are Timing Handshake tick integers only. Wall-clock lives in a separate telemetry field excluded from hashes.
- Jobs dispatched on tick T get seq = 0..K-1 and a pointer to ledgerRing[(T_offset + seq) & RING_MASK]. Workers finish in any order and write result + ThreadRecord into their slot. At the merge barrier the Timing Handshake sweeps slots 0..K-1 linearly, committing and hashing in (tick, seq) order with no locks and no heap allocations.

### 3. Fairness gate guardrail (2, 4, 8, 16 threads)
Scenario hashes must match byte for byte: cities ON cb32c824f8b3ea98, cities OFF e015e51cf1559089.
Trap: 8 jobs dispatched on tick 100 finish on tick 101 with 16 threads but across 101-104 with 2. Hashing observedReturnTick makes them diverge.
Fix: tick-synchronous jobs (complete within the barrier or a fixed commitWindowTick) hash returnTick directly. Async pool jobs hash (spawnTick, dispatchSeq, deterministicCommitEpoch, payloadHash); observedReturnTick and wall-clock go in the non-hashed audit telemetry of ThreadRecord.

### 4. Wednesday checklist
- [ ] Boot spawns dedicated threads; shutdown returns them with "engine power-down", zero leaks.
- [ ] Pooled threads cycle POOLED -> CLAIMED -> RECLAIMED with no mid-run OS thread creation.
- [ ] Scenario hashes match cb32c824f8b3ea98 / e015e51cf1559089 at 2, 4, 8, 16 pool threads.
- [ ] Per-tick ledger overhead measured at ~0 ms on the hot path.
