# Report for Archie: Mega-Pipeline Phases 2 and 3 (2026-10-25, ~6:05 AM ET)

Direction and final verify JD. Design Archie (mega-draw and Mega-Pipeline briefs). Implementation Cortana.

Commits (local, main, not pushed):
- `03af049` Phase 2, vertex pulling, `LL_VK_PULL=1` (default off). Rollback: `git reset --hard b706863`
- `25a40b7` Phase 3, camera-facing face buckets, `LL_VK_BUCKETS=1` (needs `LL_VK_PULL=1`). Rollback: `git reset --hard 03af049`

Both gates: build rc=0, ctest 48/48, hash `24c52df2be14789c` x3, cities on `24c52df2be14789c` / off `43693d018d046b25`.
Rendering only; CPU meshing and all sim hashes are unchanged.

## 1. Deviations from your briefs
- **Only axis-aligned, full-cell quads are packed.** All corners must sit on whole cells, with one face and one material. Sub-block, rotated-UV, foliage and other geometry stays on Packed12 in the same arena slice. The slice layout is [Packed12 fallback verts][material palette][packed quads].
- **The quad has no material bits.** Each quad stores a 15-bit backward distance to a per-mesh palette entry. The entry holds layer 16, value 8, mode 8, UV origin, UV sign map and faceAo high bits. The bit order differs from your table (see the header of `packed_quad64.h`).
- **Every packed quad is checked bit-exact.** It is decoded again on the CPU and kept only if its 4 Packed12 vertices come back identical. Otherwise it falls back, so the picture cannot change.
- **Meshes with alpha-blend faces stay fully on Packed12**, so the sorted blend pass is unchanged.
- **Arena blocks.** The arena array is bound as set 0, binding 6 (64 storage buffers, vertex stage). Blocks >= 64 fall back to Packed12. This needs `shaderStorageBufferArrayDynamicIndexing`.
- **Phase 3 "6 normal buckets" are face ranges inside each mesh's quad list**, not separate arenas. The arena was already shared, and MDI issues one call per arena-block run. Adjacent visible buckets merge into one indirect command.
- **Phases 2 and 3 are flags only.** The default stays mode 3 (V2b) with no pulling.

## 2. GPU time: shading-bound, not draw-bound
- Flyover r48, chunk GPU time is about 10 ms out of about 11.5 ms per frame. Mode 0 and mode 3 cost the same GPU time; mode 3 saves 15–25% CPU.
- Cutting draws or API calls (r48: 6.7k draws, about 1.2k API calls) does not move FPS. Vertex bandwidth and fragment shading dominate.

## 3. Display-mode baseline (summary)
Full table: `Display-Mode-Baseline-2026-10-25.md`.
- All modes are within ~3%.
- Borderless has the best tail. Maximized (1920x998) is 2–3% slower. Exclusive fullscreen at 144 Hz has the largest spikes.
- The code default is Maximized, per JD.

## 4. Pulled share and picture
- Only about 8% of quads qualify for packing: flyover r48 has about 0.61M pulled against about 6.0M fallback quads per frame. Most quads fail the full-cell or bit-exact rules.
- Phase 3 then skips about 50% of the pulled quads (r48: 303k drawn, 309k culled per frame).
- Frozen-time diff (`LL_VK_FREEZE_TIME=100`, 1280x720):

| Comparison | r16 differing px | r48 differing px |
|---|---|---|
| mode 3 vs pull | 0.35% (>16 levels: 0.049%) | 0.22% (>16 levels: 0.088%) |
| pull vs buckets | 0.16% | 0.28% |
| mode 3 vs buckets | 0.55% | 0.49% |
| base vs base noise floor | 0.47% (>16 levels: 0.068%) | — |

All differences are at the noise floor (animated effects); none are structural.

## 5. Results, maximized 1920x998, 30 s, 1 run each (FPS / p99 / p99.9 / max ms / GPU ms)

Flyover:

| Radius | Mode 3 | Mode 3 + pull (P2) | Mode 3 + pull + buckets (P3) |
|---|---|---|---|
| r8 | 523.5 / 3.90 / 4.27 / 7.06 / 1.64 | 538.0 / 3.81 / 4.18 / 13.53 / 1.60 | 550.3 / 3.73 / 4.12 / 12.14 / 1.57 |
| r32 | 144.5 / 9.30 / 10.78 / 17.57 / 6.48 | 148.2 / 9.17 / 11.45 / 27.60 / 6.46 | 144.8 / 10.11 / 17.78 / 31.08 / 6.50 |
| r48 | 85.9 / 15.14 / 15.84 / 18.03 / 10.14 | 87.3 / 15.46 / 26.90 / 33.22 / 10.28 | 84.3 / 15.36 / 16.55 / 17.71 / 10.70 |

Heavy test world:

| Radius | Mode 3 | P2 | P3 |
|---|---|---|---|
| r8 | 544.0 / 3.75 / 4.14 / 4.55 / 1.59 | 530.8 / 3.90 / 4.28 / 13.23 / 1.61 | 511.6 / 3.98 / 4.33 / 15.39 / 1.66 |
| r32 | 140.8 / 9.38 / 9.90 / 10.40 / 6.65 | 148.5 / 9.10 / 9.96 / 14.75 / 6.41 | 150.8 / 9.10 / 11.53 / 16.10 / 6.32 |
| r48 | 82.5 / 15.58 / 16.50 / 19.58 / 10.42 | 87.1 / 15.07 / 16.35 / 18.07 / 10.26 | 83.5 / 15.38 / 16.95 / 20.59 / 10.65 |

Fresh borderless baseline (1920x1080, JD's request; FPS / p99.9 / max ms):

| Scene | Radius | Mode 0 | Mode 3 | P3 (pull + buckets) |
|---|---|---|---|---|
| flyover | r8 | 518.6 / 4.33 / 5.28 | 522.3 / 4.31 / 5.28 | 515.9 / 4.40 / 10.68 |
| flyover | r32 | 146.7 / 9.79 / 15.49 | 147.7 / 9.67 / 11.00 | 153.4 / 9.70 / 14.45 |
| flyover | r48 | 87.7 / 15.97 / 17.54 | 87.0 / 16.89 / 23.46 | 88.9 / 17.67 / 18.95 |
| test world | r8 | 532.0 / 4.27 / 4.84 | 517.2 / 4.34 / 4.67 | 546.8 / 4.23 / 12.46 |
| test world | r32 | 152.2 / 9.47 / 10.65 | 146.8 / 9.74 / 9.88 | 151.6 / 9.71 / 14.37 |
| test world | r48 | 87.0 / 17.27 / 23.79 | 87.4 / 18.74 / 20.37 | 89.2 / 17.05 / 23.08 |

**Verdict.**
- P2 and P3 are within run-to-run noise: roughly 0 to +5% FPS, with GPU ms unchanged.
- There is no regression in p99. Single-run max spikes are noisy (one 10–15 ms startup hitch with pull on).
- That is expected: only about 8% of quads are pulled, and the GPU is shading-bound.
- **Recommendation:** keep both flags off by default until the pulled share is raised and benches are rerun.

## 6. Open questions for Archie
1. Raising the pulled share. Should the encoder accept sub-cell offsets (1/16-unit origin plus a per-quad scale), or per-corner UV, at a cost of 96 bits? Which fallback classes dominate (UV rotation, faceAo rotation bits, partial blocks) needs a counter. Should that be the next step?
2. Phase 3 culls about 50% of pulled quads, but the GPU time barely moves. Is that because back faces were already culled in hardware? Should bucket culling move before rasterisation for the Packed12 fallback too (sorting fallback quads by face)?
3. Given the shading-bound result, should the next effort go to fragment cost (overdraw, depth prepass, LOD) rather than to further draw or vertex work? This overlaps Elfie's streaming/culling research for Phase 4.
4. Blocks >= 64 fall back to Packed12. Should we raise the binding to 1024 (descriptor indexing) or use buffer device address?

Direction and final verify JD. Design Archie (mega-draw and Mega-Pipeline briefs). Implementation Cortana.