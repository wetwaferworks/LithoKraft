# Archie's Brief — Multi-Draw Architecture (relayed by JD, 2026-10-23 ~10:08 EDT)

Archie's two-phase plan to collapse per-mesh bridge chatter, written against Elfie's case report:

**Phase 1 — Composite "Draw Packets" (zero engine mesh changes; GL shim + lkboot.js). Target ~28 → ~45–50 FPS.**
Per frame, each visible sub-chunk currently costs 4–8 ring records (BindVertexArray, Uniform4fv ×~2, DrawElements). Emit/detect one composite opcode `OP_DRAW_SUBCHUNK` (fixed 48–64 B payload: vaoId, indexCount, indexOffset, uniformLoc0, vec4_0, uniformLoc1, vec4_1). In lkboot: decode 1 record instead of 5; zero-GC `gl.uniform4fv(loc, sharedFloatView, floatOffset, 4)` offset overload; cache lastBoundVAO/lastProgram in the executor to skip redundant DOM calls. Also: check why Uniform4fv fires 3–4× per mesh and cache unchanged values in lkboot.

**Phase 2 — WEBGL_multi_draw mega-call (engine chunk renderer + lkboot). Target locked 60+ FPS, JS draw overhead ~25 ms/frame → <0.5 ms/frame.**
One `multiDrawElementsWEBGL` per frame for the whole visible list. Blockers and unlocks:
1. All draws in a mega-call share ONE VAO → turn `--gl-arena` ON for web (or bucket arenas): suballocate chunk meshes into 1–4 large arena VBOs/EABs (16–32 MB). WebGL2 lacks BaseVertex → bake the base-vertex offset into the index buffer at upload time with UNSIGNED_INT indices (works on 100% of WebGL2, no extension needed).
2. No Uniform4fv between draws in a mega-call → move per-chunk vec4 data (world offset + effect params) into an instanced attribute (divisor=1) or UBO array; `multiDrawElementsInstancedWEBGL` increments gl_InstanceID. Eliminates all ~50k Uniform4fv/s.
3. Engine culler emits one flat list per frame: counts[], indexByteOffsets[], perChunkUniformData[]; one BufferSubData + one multi-draw call across the ring → ~5–10 ring records/frame total.

Summary table (Archie's): records/frame ~6–12k → ~1.2–1.5k (P1) → ~5–10 (P2); Uniform4fv/s 35–51k → 35–51k zero-GC (P1) → 0 (P2); engine changes zero (P1) / arena + packed vec4 array (P2); expected FPS ~25–30 → ~45–50 → 60+ locked.

*Registrar's notes (Elfie, 2026-10-23): (a) Phase 1 as written coalesces at the ring producer; the equally zero-engine-change route is executor-side coalescing — since the flicker fix, the page holds COMPLETE frames before executing them, so the per-mesh record pattern can be fused at flush time in lkboot with the same effect and no producer change. (b) The 35–51k Uniform4fv figure counts sync-classified records arriving; the executor's value-hash dedup already suppresses some GL-level repeats — the win Phase 1 buys is decode+dispatch volume, which is the real cost. (c) FPS figures are Archie's estimates, direction endorsed, numbers to be proven by census.*
