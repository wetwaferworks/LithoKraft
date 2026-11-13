# Phase 2 Readiness Audit — Arena & Multi-Draw, From the Source

*Registrar's read-only audit of the local tree (`engine/src/render/gl33/`), 2026-10-23. Prepared while JD steps out; no code changed. Purpose: state exactly what Phase 2 (Archie's mega-call) touches, correcting two assumptions in the original brief with source facts.*

## What already exists

- **The arena is built and is the renderer's default.** `GL33Renderer(bool arena)`, `useArena_ = true`; one VBO + one VAO, first-fit free ranges, growth by doubling (`glCopyBufferSubData`), initial capacity 32 MB of `PackedVertex`. Web runs `--gl-arena off` by flag, not by missing code.
- **Arena draws use a shared pattern index buffer.** Every sub-chunk mesh is quad soup (4 verts/quad); its indices are always the same pattern, served by one shared EBO (`ebo_`) bound into the arena VAO. The draw is:
  `glDrawElementsBaseVertex(GL_TRIANGLES, g.indexCount, GL_UNSIGNED_INT, nullptr, GLint(g.first))`
  — the **base vertex** selects the mesh's vertex range; there is **no per-mesh index data anywhere**.
- **Per-mesh uniforms, exactly as the census counted:** `glUniform3f(uOffset_)` (chunk min corner) + four `setEffect4` calls (`uPulseP0/P1`, `uTraceP0/P1` — Uniform4fv each, engine-side last-value cached, but the blended values genuinely differ per mesh). ≈4 Uniform4fv + 1 Uniform3f per draw — matches TB1's itch census (35–51k Uniform4fv/s).

## Corrections to the brief's assumptions

1. **"Bake the vertex offset into 32-bit indices" is unnecessary here** — there are no per-mesh indices to bake. The real blocker is different: `WEBGL_multi_draw`'s `multiDrawElementsWEBGL` has **no per-draw base-vertex parameter**, and base-vertex is the arena's only addressing mechanism.
2. **The state-cache half of Phase 1 already existed** in the driver (VAO/program/texture/buffer binds all cached) — TB3's remaining Phase-1 win was dispatch, and its dedup meter proved per-mesh uniform values change ~99% of the time. Phase 2's instanced-data move is therefore not an optimization of the current design; it replaces the addressing scheme.

## The three viable Phase 2 shapes (decision for JD + Archie)

- **A. Instanced quad draw.** One `glDrawElementsInstanced` for all classic meshes: instance = one quad; the shared pattern EBO + `gl_VertexID` decode picks the corner; per-quad instanced attributes carry the vertex base and the mesh's offset/pulse/trace data (or a mesh-id attribute indexing a per-frame UBO/texture). One draw call, zero per-mesh uniforms, no BaseVertex. Cost: real shader surgery on `chunk_classic` + a per-frame instanced-attribute pack; per-mesh effect data duplicated per quad unless indirection is used.
- **B. Expanded arrays + `multiDrawArraysWEBGL`.** Store 6 verts per quad instead of 4 (+50% vertex memory/traffic), keep per-draw first/count arrays, move uniforms to an instanced attribute as in A. Simpler shader story than A, pays in memory bandwidth — the thing the software renderer path is already sensitive to.
- **C. Phase-1-only on web.** Keep per-mesh draws on the web build; arena + (native) batching stays a desktop story. The web build's 30 fps bar is already met by JD's own standard; Phase 2 becomes a stretch goal, not runway-critical.

## Registrar's note

Phase 2 is engine + shader work under any shape — it belongs on the Christmas runway behind JD's lock, sequenced after the Redstone→Trace layer. The audit's value today: nobody has to guess at the arena anymore; it is quad-pattern + base-vertex, and shape A is the only variant that preserves both the memory layout and the one-call goal.
