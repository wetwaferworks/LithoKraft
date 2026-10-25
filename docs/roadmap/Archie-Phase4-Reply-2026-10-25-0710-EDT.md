# Archie's reply to Phase 4 questions (received via JD, 2026-10-25 07:10 EDT)
Credit: guidance by Archie (note: includes some direct answers, e.g. a 64-bit sub-cell layout and GLSL bitfieldExtract unpack, beyond the hint-only ask).

Key points:
- 91% of quads bypass 3p: 67% whole-mesh blend fallback, 24% 1/16 sub-cell.
- Packing: 64-bit fixed-point sub-grid (8/8/8 pos, 4/4 W/H, 1-bit scale flag, 3 normal, 16 material, 8 AO, 4 flags); or MicroProfileID LUT (measure unique sub-cell tuples first); use bitfieldExtract on uvec2.
- Q1 blend: split each sub-chunk into Opaque Range + Blend Range, both pulled format; measure opaque/blend ratio in rejected sub-chunks.
- Q2 sub-cell: check if they need different bindings or just fractional positions; scale flag unifies.
- Q3 arena >64 blocks: weigh web/GL parity; bigger blocks (64/128 MB) or descriptor-indexed SSBO array with {blockIdx, offset} via firstInstance/gl_DrawID; BDA lacks GL/WebGPU equivalent.
- Q4 CPU frustum 1.7 ms/25k meshes: test columns/4x4 regions first; radial iteration gives implicit near-far / far-near order, drop per-frame std::sort.
- Q5 4.1 governor: two-stage token bucket: staging bytes/frame cap + CPU microsecond deadline (~1.5 ms) on publish/evict queue.
- Q6 fragment cost: 60 s static r32/r48 at 50% resolution and with flat-color fragment stub to separate ROP/overdraw vs ALU.
- Q7 LithoTexture residency: priority = FrustumVisible + radius/distance, LRU grace for whip-pans, shared staging byte budget with voxel priority.
- Q8 mystery: why 3p does 52% more uploads and 42% more evictions at r64 (67.6 vs 92.3 FPS); dual arena entries? redundant rebuilds? pipeline switching.
- Q9 occlusion gap: pipeline-stat fragment invocations or bbox occlusion queries; % of frustum-visible chunks with 0 samples = HZB ceiling.
- Archie asks: tackle blend split (67%) or sub-cell packing (24%) first?
