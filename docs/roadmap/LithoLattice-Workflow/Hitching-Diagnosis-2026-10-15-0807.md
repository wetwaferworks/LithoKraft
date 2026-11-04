# LithoKraft hitching diagnosis, 2026-10-15 (TheWorkstation, i5-12450H 8C/12T, 16 GB, RTX 3050 Ti)

## JD's run (game\LithoKraft\logs\lithokraft.log, 07:27:56-07:29:44 EDT)
- Launched **without flags**: renderer **software**, **windowed 1280x720** (not Vulkan, not maximized), MSVC **Debug** build, timing handshake ON.
- "32" is the **configured** tick rate (settings default tps=32), not a cap; achieved 31.9-32.0/32 once streaming settled (07:28:45-07:28:53).
- During the first ~40 s: tps 0-26/32, tick max 100-470 ms, dropped ticks climbing to 4145, frames 300-3000 ms, fps 0-4. World load 7057 ms.
- 07:28:57-07:29:43: fps=0, tps=0, dropped +~490/s: no frames presented (window likely minimised/unfocused); sim stalled with it.
- No missing textures (199 textures, 0 missing). Warnings: unknown faces side_* on red_via/blue_via models; software path has no PBR transparency; test-key warning (now fixed, see below).

## Root causes (measured)
1. **Debug build** (/Od, iterator debug, debug heap). handshake-scenario 2048: Release 2.0 s vs Debug 14.6 s (7.3x), hash aa82e7afb1eff9d3 in both. Bench (software, tps 32, stream): Debug update 108 ms/frame, sim tick 58 ms avg / 166 ms p99, worldgen 344 ms/column vs Release update 9.4 ms, tick 0.69 ms, gen 2.3 ms.
2. **Stream pumps stop and restart the sim thread** (game.cpp stream insert/evict: sim_->stop() joins the SimThread and handshake workers, then sim_->start()). While columns stream, the sim barely ticks, in Release too: bench stream path 512 TPS target -> **5.5 TPS avg** (32 target -> 0.2); orbit path (little streaming) -> **469.5/512** and 29.3/32. This is the TPS instability JD saw. Not Windows-specific code; left for Elfie (design change).
3. **Software renderer frame cost**: Release orbit at 1280x720: render phase **134 ms/frame -> 7 FPS**. This is the FPS limit for the alpha focus (software + engine).
- Timer resolution is fine: SimThread already holds timeBeginPeriod(1) (engine/src/core/timer_resolution.cpp) and yield-spins the last 1.5 ms.
- No GC (C++). There are no allocation counters or extra trace flags in the code; the logs use the built-in perf/hitch lines and bench JSON at their maximum detail.

## Sweep: Release, software renderer, --bench 15 --bench-path stream
| target TPS | achieved TPS (avg) | FPS | frame p50 ms | frame p99 ms | max ms | hitch lines | sim tick avg/p99 ms |
|---|---|---|---|---|---|---|---|
| 32 | 0.3 | 105 | 11.8 | 16.6 | 32.1 | 27 | 0.37/1.94 |
| 64 | 0.6 | 101 | 12.4 | 16.6 | 32.5 | 27 | 0.54/1.41 |
| 128 | 1.4 | 102 | 12.3 | 16.7 | 32.7 | 31 | 1.18/6.24 |
| 256 | 2.3 | 104 | 12.1 | 16.6 | 32.6 | 35 | 0.87/4.07 |
| 512 | 4.4 | 101 | 12.5 | 16.6 | 25.9 | 30 | 0.61/0.88 |
The stream camera renders almost nothing (render 0.6 ms), so FPS here overstates real play. Achieved TPS collapses because of cause 2.

Sim-only (perf-headless, Release, stress circuits, real SimThread): 32/64/128/256/512 achieved exactly, tick avg 0.50-0.74 ms, p99 1.0-1.4 ms, budget 2.4-25.4%.

Orbit (world in view, little streaming), Release software: 32 -> 29.3 TPS, 7 FPS, p50 145.6/p99 176.9 ms; 512 -> 469.5 TPS (min 431.7), 7 FPS, p50 146.7/p99 178.3 ms.

## Per-phase main-thread breakdown (ms/frame, bench JSON)
| run | input | update (sim handoff+mesh+worldgen) | upload | render | present | sim tick | worldgen/col | mesh/sub |
|---|---|---|---|---|---|---|---|---|
| Release orbit 512 | 0.10 | 10.96 | 0.06 | **134.12** | 0.48 | 0.35 | - | - |
| Release stream 512 | 0.03 | 9.36 | 0.03 | 0.60 | 0.50 | 0.69/1.58 | 2.33/4.38 | 12.6/49.9 |
| Debug stream 32 | 0.47 | **108.19** | 3.01 | 1.67 | 0.56 | **58.1/165.5** | **343.6/620.0** | - |
Workers: 12 hardware threads; timing handshake 4 phases/cycle. Mesh throughput drops with threads (1 thread 3980 sub/s, 12 threads 966 sub/s): contention, open item.

## Changes
- **Signing key**: new local dev Ed25519 key from `litho_sign keygen`. Private key outside the tree at C:\Users\jdwetwork\LithoLattice-Keys\dev-key-2026-10-15-0733 (folder ACL: JD only). New public key 04c1766ef29e93b7d36fdf185f64ee958e9dded4c8c82f0dca03029e8a30011c is baked in engine/src/assets/include/litholattice/assets/mod_signature.h (backup .orig-2026-10-15-0733). Old test key 3bb44b55... is still recognised as retired by bakedKeyIsTestKey(); the release FATAL check in main.cpp is untouched. Nothing in the tree was signed with the old key (no signature.json, no mods/), so there was nothing to re-sign. Checked: sign + verify with the new key OK; the Release exe embeds the new key.
- **biome_capture.cpp** (backup .orig-2026-10-15-0750): the source root is now resolved portably. Order: the original /home/hatch path if it exists (Linux unchanged), LITHO_SOURCE_ROOT, then walk up from the cwd/exe. Captures are in C:\Users\jdwetwork\GrokBot-Transfer\captures-2026-10-15-0749.
- Rebuilt Release (build\Release) and Debug. User PATH: build\Release first, ahead of build-debug (backup GrokBot-Transfer\UserPath-backup-2026-10-15-0741.txt).
- Debug ctest: 41/48 (same 7 as baseline: systerian, storage, litho_formula, assets, input, console, lithokraft_selftest). Scenario hash unchanged.

## Retest (normal title screen and menus)
Release software: `R:\repos\LithoLattice-Workflow\LithoLattice-LithoKraft-Full-Source-2026-10-15\build\Release\LithoKraft.exe --renderer software --display maximized`
Release Vulkan:   `R:\repos\LithoLattice-Workflow\LithoLattice-LithoKraft-Full-Source-2026-10-15\build\Release\LithoKraft.exe --renderer vulkan --display maximized`
Optional: add `--tps 512` to run the handshake target rate.

## Open
- Stream pump sim stop/start (cause 2): needs an engine design change, for example inserting columns without a full thread join/restart.
- Software rasterizer at 134 ms/frame at 720p (more when maximized at 1080p): profile the rasterizer's threading.
- Mesh scaling inverse with thread count.
- red_via/blue_via unknown faces.
- Release FATAL boot check not exercised in a window by me; key check is compile-time verified to pass.
- Logs: R:\repos\LithoLattice-Workflow\hitch-logs\
