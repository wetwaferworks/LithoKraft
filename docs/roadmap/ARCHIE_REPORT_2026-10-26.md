# Report for Archie: optimization items 1–7 (2026-10-26)

From Cortana, for JD to hand to Archie. Workstation main is at `38ea867`. ctest passes 48/48 in DebugOpt and Release. Handshake hashes are unchanged. Nothing has been pushed.

## Results

- **Item 1, SeqLock camera pose + interpolation (`de16dc1`):** frozen frames went from 554/2967 to 0/1150.
- **Item 2, near-plane binning clip (`8726864`):** pixel-identical and perf-neutral. `LL_SW_NEARBIN_OLD=1` reverts it.
- **Item 3, render threads = cores−2:** NOT committed. Drawing got 15–25% slower and the sim gained nothing. Recommendation (b): keep 11 threads as the default and commit only the `LL_SW_RENDER_THREADS` override plus the Linux sim priority. This needs your call.
- **Item 4, adaptive background cap (`01b5a12`):**
  - Behavior: uncapped while loading or paused, cap 2 when standing still (3 on 8+ cores), cap 1 plus yield while moving.
  - Flying results: p99 went from 22.5–24.9 to 20.4–20.9 ms. Sim tick average fell 15–25%, and tick max went from 10–11 to 8.2–8.7 ms.
  - Trade-off: about 130 meshes are still queued at the end of a run, versus about 11 before.
  - This solves the problem item 3 was aimed at.
- **Item 5, bench fairness (`c74266b`):**
  - New: v8 anchor, pinned shots, and freeze tick 1024.
  - Baseline at 11 threads, DebugOpt (avg/p99 ms): still 19.36/25.45, live 18.13/21.71, walk 18.06/26.52, flyover 18.57/28.24, stream 82.45/162.54.
- **Item 6, Candidate D 64×64 tiles (`78639fb`):**
  - Behind `LL_SW_TILE64`, OFF by default.
  - 1.3–2.5× slower overall, and strip raster 1.8–4.7× slower.
  - Cause: the bins sort only by row, so each of the ~20 tiles across a row redoes setup for every quad in that row.
  - Also: small 2×2-block pixel diffs versus strips. Cause not confirmed.
- **Item 7, reciprocal 16-bit depth (`38ea867`):**
  - Behind `LL_SW_DEPTH_REMAP=2`, opt-in. `LL_RETRO_SHIMMER=1` restores the old look.
  - Depth step at 256 blocks: about 0.06 blocks, versus about 20 before. This fixes distant banding and the rail moiré.
  - Walk retest over 3 runs each: 18.06 vs 18.05 ms avg, so it costs nothing.
  - Default pending JD's look. CMAA edge thresholds may need retuning in this mode.

## Questions for Archie (also in docs/ROADMAP.md)

1. Item 3: approve option (b)?
2. Item 6: are per-tile bins that also sort by column worth designing? And what causes the 2×2 strip-vs-tile diffs?
3. Stream (82 ms avg, ~1M tris/frame) is the biggest target left. What should we go after next?

## Coming next

- Thread Handshake (fourth pillar), Wednesday 2026-10-28, in a test tree. Notes are in `docs/roadmap/THREAD_HANDSHAKE_IMPL_NOTES.md`. The item 3/4 thread policy folds into its dispatch pool.

Executive direction, vision and verification: JD. Systems architecture and optimization counsel: Archie (co-architect). Lead engine implementation and empirical profiling: Cortana. Lineage: inspired by id Software's id Tech 6 (Doom 2016) and MegaTexture.
