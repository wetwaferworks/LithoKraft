# Voxel Optimization Checklist for LithoLattice
October 10, 2026. Research from Sodium, Lithium, Starlight, and voxel engine work.

## Implementation status (2026-10-10)

- ✅ **Greedy meshing** — `greedy_mesher.cpp` : binary face detection via uint64_t bitmasks (16x16 bit per Y layer). Rectangle merging TODO.
- ✅ **Neighbor-padded buffers** — already had 18³ (16+2)³ padding in `mesh_input.h`. Documented.
- ✅ **BFS occlusion culling** — `ChunkVisibility::compute()` in `greedy_mesher.cpp`. Breadth-first search from camera, stops at opaque chunks.
- ✅ **Upload budgets** — `UploadBudget` class. Max N chunk uploads per frame.
- ✅ **LOD** — `ChunkLOD` enum + `lodForDistance()`. Full (≤4 chunks), Half (≤8), Quarter (>8).
- ✅ **Backface culling by axis** — `axis_culling.cpp` : splits mesh into 6 direction buckets, draws only 3 facing camera. ~50% fewer vertices.
- ✅ **BFS light engine** — `light_engine.cpp` : clean-room BFS propagation, 1-64 light scale, deferred updates. Inspired by Starlight's approach (LGPL-3.0), our own code.

## What we already have ✅

- **Face culling** — only emit faces adjacent to air/transparent (mesher.cpp)
- **Texture atlas** — single texture array, no per-block binds
- **PackedVertex** — 16-byte vertices, cache-friendly
- **Frustum culling** — in GL33 renderer (basic)
- **Async meshing** — mesher.cpp has threading hooks
- **Software renderer** — CPU fallback (unique advantage)

## High impact — do first 🔴

### 1. Greedy meshing
Merge adjacent same-material faces into larger quads. 90%+ triangle reduction.
- Sodium does this. The 0fps article is the canonical reference.
- Our mesher emits per-face quads; greedy would merge runs.
- **Gain**: 5-10x fewer triangles → 5-10x less vertex processing.
- **Risk**: More complex mesher, needs neighbor context.

### 2. Binary/bitwise greedy meshing
Use bitmasks for face detection instead of per-voxel loops.
- Voxelot uses `col & !(col << 1)` for face detection.
- Process 64 voxels at once via uint64_t.
- **Gain**: 2-3x faster meshing (CPU side).
- **Risk**: Fixed chunk sizes required, more complex code.

### 3. Sodium-style occlusion culling (BFS graph traversal)
Walk outward from camera through chunks; stop when hitting solid walls.
- Sodium's key technique. Skips entire hidden chunk sections.
- **Gain**: 30-50% fewer chunks meshed/uploaded in dense terrain.
- **Risk**: Needs chunk visibility graph, updates on edit.

### 4. Neighbor-padded chunk buffers
Stage meshing with (n+2)³ buffer (chunk + 1-voxel shell from neighbors).
- Avoids per-face neighbor lookups during meshing.
- **Gain**: 2x faster meshing, better cache behavior.
- **Risk**: Memory overhead (small), need neighbor sync.

## Medium impact — do second 🟡

### 5. Chunked LOD (Level of Detail)
Distant chunks use simplified meshes (2x, 4x, 8x voxel merging).
- **Gain**: Massive draw call reduction at distance.
- **Risk**: LOD popping, need transition blending.

### 6. Async chunk upload budgets
Limit mesh uploads per frame (e.g. 4 chunks/frame max).
- Prevents frame spikes when many chunks update.
- **Gain**: Smoother framerate, no hitches.
- **Risk**: Slower to show new chunks (acceptable).

### 7. Backface culling by axis (Glescraft technique)
Split chunk VBO into 6 parts (one per face direction).
Only upload/draw the 3 facing the camera.
- **Gain**: ~50% fewer vertices sent to GPU.
- **Risk**: More draw calls (6 per chunk instead of 1).

### 8. Lighting engine optimization (Starlight-style)
Our light propagation is naive flood-fill. Starlight rewrote it from scratch.
- **Gain**: 10-25x faster light updates (their claim).
- **Risk**: Major rewrite of lighting system.

## Low impact / Nice to have 🟢

### 9. Palette compression (FerriteCore-style)
Store chunk data as palette indices instead of full block IDs.
- **Gain**: 50-70% less memory for chunk storage.
- **Risk**: Indirection overhead on access.

### 10. Meshlet / GPU-driven culling
Group triangles into meshlets, cull on GPU via compute shader.
- **Gain**: Best for high-end GPUs, minimal CPU overhead.
- **Risk**: Requires compute shaders, complex.

### 11. Sparse Voxel Octree (SVO)
For infinite worlds, store only non-empty regions hierarchically.
- **Gain**: Memory efficient for sparse worlds (Space dimension!).
- **Risk**: Complex, slower random access.

## What NOT to do ❌

- **VulkanMod-style rewrites** — we already have native Vulkan, no need.
- **Immediate mode rendering** — we're already using VBOs.
- **Per-block draw calls** — we batch, don't regress.
- **Java-style GC tricks** — we're C++, memory is manual.

## Recommended order

1. Greedy meshing (biggest win)
2. Neighbor-padded buffers (makes greedy faster)
3. BFS occlusion culling (skips hidden work)
4. Upload budgets (smooths frames)
5. LOD (distance scaling)
6. Lighting rewrite (if profiling shows it's a bottleneck)

## Sources

- Sodium (CaffeineMC) — modern OpenGL renderer, chunk meshing
- Lithium (CaffeineMC) — game logic, physics, AI optimizations
- Starlight (Spottedleaf) — lighting engine rewrite
- FerriteCore (malte0811) — memory optimization
- 0fps.net "Meshing in a Minecraft Game" — greedy meshing canonical reference
- tomcc.github.io "Visibility" — Minecraft's flood-fill visibility
- Glescraft (Wikibooks) — axis-separated VBO technique
