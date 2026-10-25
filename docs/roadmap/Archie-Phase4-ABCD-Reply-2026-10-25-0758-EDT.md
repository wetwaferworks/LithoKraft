# Archie's reply to Phase 4 A-D r32/r64 report (2026-10-25, ~7:58 AM ET, pasted by JD)

## Credit proposal (pending JD's lock)
- Executive Direction, Vision & Verification: JD
- Systems Architecture & Optimization Counsel: Archie (Co-Architect: bit-packing schemes, pipeline strategy, diagnostic triage)
- Lead Engine Implementation & Empirical Profiling: Cortana (Vulkan pipeline engineering, threading, telemetry analysis)
- Historical lineage: inspired by id Software's id Tech 6 (Doom 2016) and MegaTexture

## Answers
1. r64 regression: agrees CPU/streaming-side (GPU 11.52 -> 8.18 ms while FPS fell; r64-on still churning at end of window). First counters: per-thread buildPulled wall time + call count, and staging/upload queue stall time on main/render thread. In parallel, test split-on with arena reserve locked to 1920 MB to isolate arena thrash vs encode cost.
2. Encode cost: derive UV sign/orientation codes algebraically from face normal nibble + winding (O(1)); keep a single assert check in debug only, not 81 trial decodes in release. buildPulled must live strictly on background mesh workers, never main/upload thread.
3. Sub-cell: leave on Packed12 for good. No third wider format (layout/range/draw-call cost outweighs savings).
4. Fragment cost (~25%, resolution-independent): 2x2 helper-lane overshading on sub-pixel distant triangles plus fixed setup. Fix r64 CPU churn first; then a distance-based shader LOD beyond r32 (simplified material/FX).
5. Blend faces: keep Packed12 final for alpha-blend. Cortana's deviation is the right permanent architecture.
