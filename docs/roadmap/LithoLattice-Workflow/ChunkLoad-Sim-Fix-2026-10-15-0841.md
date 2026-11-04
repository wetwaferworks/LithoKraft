# Chunk-load sim fix, 2026-10-15

## Root cause
game/LithoKraft/src/game/game.cpp, Game::pumpStreaming (insertion batch) and Game::evictFarColumns: every batch called sim_->stop() then sim_->start().
SimThread::stop() (engine/src/sim/sim_thread.cpp) does running_=false, **thread_.join()** on the sim thread, then **TimingHandshakeAuthority::stop()**, a bounded join of every handshake pull worker. start() then respawns the workers and the sim thread (a new HighResTimerScope, and the pacing clock is re-based, so ticks during the gap were lost and not counted).
The main thread was blocked on thread joins, not on a mutex. This happened several times a second during streaming (5,451 stream events in 12 s).

## Change (backups .orig-2026-10-15-0825)
- sim_thread.h/.cpp: new SimThread::holdBetweenTicks(unique_lock&). It takes the existing recursive blockMutex_ (already held across the edit drain plus every tick of an iteration) and refuses, returning false, when the handshake is frozen or has an outstanding worker.
- sim_thread.cpp loop body: the post-tick section (flushDirty mesh snapshots, stats/inspect reads of chunks_) now also runs under blockMutex_. It is released before the tail wait.
- game.cpp insertion and eviction: hold the sim between ticks instead of stop()/start(). The old stop() path stays as the fail-closed fallback for a frozen or poisoned handshake. The hold is released before evictFarColumns() so its fallback can never join while it is held.
- The handshake itself is unchanged. Plain std:: types only, no #ifdefs, so GCC/Clang get the same code.

## Results (Release, software renderer, --bench 12)
| run | before TPS | after TPS (min-max) | after FPS | frame p50/p99 ms | hitches | tick avg/p99 ms |
|---|---|---|---|---|---|---|
| stream 32 | 0.2 | 32.0 (31.7-32.9) | 393 | 2.47/3.65 | 3 | 0.96/2.31 |
| stream 512 | 5.5 | 512.0 (511.9-512.2) | 483 | 1.95/3.43 | 11 | 0.57/0.71 |
| orbit 32 | 29.3 | 31.9 (31.9-32.0) | 7 | 141.3/169.1 | 5 | 0.54/0.84 |
| orbit 512 | 469.5 | 512.0 (511.9-512.2) | 7 | 141.5/169.7 | 5 | 0.40/0.47 |
Stream throughput: 25,635 stream events in 12 s vs 5,451 before. Orbit FPS is still limited by the software renderer (next step).
Hash: handshake-scenario 2048 = aa82e7afb1eff9d3 (Release and Debug). Debug ctest 41/48, the same 7 failures as the baseline.
Logs: hitch-logs\bench3-*, ctest-debug-0832.log, build-*-0827.log.
