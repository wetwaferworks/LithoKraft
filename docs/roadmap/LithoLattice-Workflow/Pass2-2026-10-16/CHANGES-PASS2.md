# Pass 2 (2026-10-16): software renderer, ideas by Ginny (COO, Argon); built by Cortana

The tree is R:\repos\LithoLattice-Workflow\Pass2-2026-10-16, a copy of the main tree, which was not touched. JD reviews it before any merge.
Runtime switch: LL_SW_P2=<mask> (1 quad, 2 parallel, 4 facecull, 8 morton, 16 dispatch). The default is 7. LL_SW_P2=0 gives the pre-Pass-2 renderer.
Proof tool: LL_SW_AB=<frame> renders that frame with mask A (LL_SW_AB_A, default 0) and then with the current mask, and logs the pixel diff.

1. Native GreedyQuad setup (Ginny): rasterizeTrisShaded does one setup and one row walk per quad. ON. Byte-identical.
2. Lockless two-phase setup (Ginny):
   - Dynamic mesh hand-out with thread-local fill cursors.
   - Row spans moved into phase 1.
   - Clear moved into the strip workers.
   - hardware_concurrency vs band count logged.
   - ON. Byte-identical.
   - Note: the existing per-mesh CSR bins were already lockless and thread-owned. I did not use local_bins[T][strips] because it would change the draw order.
3. Face-direction culling (Ginny):
   - 6 buckets per mesh, taken from each quad's own winding, plus bucket 6 that is never culled.
   - Each bucket has a limiting plane. The eye point comes from the projection.
   - Culled quads are skipped before transform and before binning. About 45% of quads are culled on walk.
   - ON. Byte-identical.
4. Morton 16x16 texel swizzle via kSwMortonU/kSwMortonV, done at load. OPT-IN (mask 8). Byte-identical, no gain, because the tiles already sit in L1.
5. ISA dispatch scalar/SSE2/SSE4.1/AVX2 via CPUID, overridable with LL_SW_ISA:
   - Separate TUs: software_simd_sse2.cpp, software_simd_sse41.cpp, software_simd_span4.inl.
   - No arch flags. No ymm/VEX code in the baseline objects (checked with objdump).
   - The SSE2/SSE4.1 span kernels sit behind the existing LL_SW_SIMD_SHADE opt-in.
   - The 4-wide quad transform is OPT-IN (mask 16), byte-identical, no gain.
Verify (Windows and Linux): hash e015e51cf1559089 on 2 runs after every step. Selftest ALL OK. A/B pixel diff = 0 on walk/orbit for every step and every ISA.
Overall, Windows (4x8 s interleaved): mask 0 -> 7, walk 51.9 -> 61.2, orbit 51.9 -> 61.0.
Backups: engine\src\render\*.orig-2026-10-16-0822/0833/0838/0843/0852.
