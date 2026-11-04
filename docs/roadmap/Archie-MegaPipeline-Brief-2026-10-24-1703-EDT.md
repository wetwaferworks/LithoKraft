# Archie's Brief — Voxel Mega-Pipeline Architecture (relayed by JD, 2026-10-24 17:03 EDT)

*Brief authored by Archie (Argon), structured for Elfie, received via JD. Text below is Archie's, verbatim. Registrar's notes follow at the end.*

---

## Engineering Brief: Voxel Mega-Pipeline Architecture (For Elfie)

### 1. Mega-Meshing (64-Bit Bitwise Culling & Quad Packing)

The goal is to eliminate traditional vertex attributes (`vec3 pos`, `vec3 norm`, `vec2 uv`) and shift to **Programmatic Vertex Pulling (PVP)** where each quad is a single 32-bit or 64-bit integer stored in a mega-buffer.

* **64-Bit SWAR Face Culling:** Represent chunk columns or rows as `uint64_t` bitmasks (1 = solid, 0 = air/transparent). To find all visible faces along an axis in a single CPU instruction, bit-shift and mask against neighbors:
* **Forward faces:** `visible_mask = column & ~(column << 1)`
* **Backward faces:** `visible_mask = column & ~(column >> 1)`
* Use `__builtin_ctzll()` (Count Trailing Zeros) to iterate strictly over active visible faces, skipping empty space completely.


* **Binary Greedy Meshing:** Instead of iterating cell-by-cell in 2D to merge quads, perform bitwise ANDs across adjacent row masks to grow quad widths and heights in $O(1)$ word steps. Only merge faces that share identical material IDs and trace/logic state.
* **64-Bit Packed Quad Payload:** Pack an entire merged greedy quad into a single `uint64_t` (or `uvec2` in GLSL):
* **Bits 0–17:** Local $X, Y, Z$ chunk coordinates ($6 \text{ bits each}$ for $32^3$ or $64^3$ chunks).
* **Bits 18–29:** Quad Width & Height ($6 \text{ bits each}$, supporting up to $64 \times 64$ merged run-lengths).
* **Bits 30–32:** Face Normal ID ($0\text{–}5$ for $\pm X, \pm Y, \pm Z$).
* **Bits 33–48:** Material / Wafer Layer / Trace Mask ID.
* **Bits 49–63:** Dynamic State / Logic Gate Index / Ambient Occlusion corners ($2 \text{ bits} \times 4 \text{ corners} = 8 \text{ bits}$).


* **Vertex Pulling (Zero VAO Overhead):** Bind one shared static index buffer (`0, 1, 2, 2, 1, 3` pattern) and pull the quad data via `gl_VertexID >> 2` (or `gl_VertexIndex >> 2` in Vulkan) from a massive SSBO. Reconstruct the 4 corner offsets in the vertex shader using `gl_VertexID & 3` and the normal index.

---

### 2. Mega-Draw (Single-Call Multi-Draw Indirect)

To push past CPU driver bottlenecks and submit thousands of wafer chunks in a single state change, move all chunk buffers into a **Sub-Allocated Arena Buffer** and drive submission via indirect command buffers.

* **Persistent Mega-Buffer (Ring/Arena Allocator):** Allocate one large contiguous SSBO (e.g., 128 MB–256 MB) to hold all packed quads for all active chunks. Maintain a simple free-list or Buddy Allocator on the CPU (or GPU) that assigns each chunk a `firstIndex` / `vertexOffset` and `quadCount` inside the mega-buffer.
* **Single Dispatch Submission:**
* **Vulkan:** `vkCmdDrawIndexedIndirectCount` (or `vkCmdDrawIndirect`).
* **OpenGL 4.3+:** `glMultiDrawElementsIndirect` (or `glMultiDrawArraysIndirect`).
* **OpenGL 3.3 / WebGL / Compatibility Fallback:** Bucket multiple chunks into regional macro-chunks ($128^3$) or use instanced rendering (`glDrawArraysInstancedBaseInstance` where available, or uniform-array batching) to minimize draw calls.


* **Per-Chunk Metadata Buffer:** Instead of pushing uniforms or push constants per chunk, index a `ChunkMetadata` SSBO using `gl_DrawID` (or `gl_InstanceIndex` if encoding chunk ID in `firstInstance`). This struct holds the chunk's world-space origin and bit-pointer to its logic-state table.
* **6-Bucket Normal Splitting (Free Backface Culling):** Group quads inside each chunk into 6 contiguous sub-ranges by normal direction ($\pm X, \pm Y, \pm Z$). When generating the `DrawIndirectCommand` list each frame (either on CPU or in a tiny Compute Shader), compare the camera position to the chunk's AABB and **only emit indirect commands for the 3 to 4 face directions that can possibly face the camera**. This cuts vertex shader invocations by ~50% before rasterization even begins.

---

### 3. Procedural Mega-Texturing & State Buffers (Wafer & Logic Traces)

Rather than streaming gigabytes of traditional bitmap mega-textures, treat the wafer surface as a **Virtual Procedural Page Table + Bit-Packed Logic State Buffer**.

* **Tile-Hashed Procedural Evaluation (Zero-Memory Mega-Texture):** Use the world-space voxel coordinates and face UV to evaluate traces, vias, and silicon doping layers mathematically in the fragment shader.
* To prevent shimmering/aliasing when viewing vast wafer cities at oblique angles, compute analytical derivatives (`fwidth(uv)` or `dFdx`/`dFdy`) and apply **analytical box-filtering** (smoothstepping the trace SDF widths toward the average background emission as pixel footprint grows).


* **Virtual Atlas Caching (Optional Hybrid Path):** If complex multi-pass procedural noise or baked circuit glyphs are needed, maintain a single $4096 \times 4096$ `R8G8B8A8` or `R16UI` Residency Atlas in VRAM. Render procedural tile pages into slots of this atlas *only* when a chunk enters near-field LOD, and look them up via a tiny 3D indirection texture.
* **Decoupled Logic-State Bitbuffer (Zero-Remesh Circuit Pulses):** Never re-mesh a chunk when a logic gate, clock line, or trace changes state.
* Store circuit states in a separate, flat `uint32_t[]` or `uint64_t[]` SSBO updated each deterministic tick.
* Each packed quad carries a `StateBitIndex` in its upper bits.
* The fragment/vertex shader reads `(StateBuffer[bitIndex >> 5] >> (bitIndex & 31)) & 1u` to determine whether a trace is HIGH or LOW in real time.
* **Focus/Edit Mode Dampening:** Multiply the pulse/emission multiplier in the shader by a global uniform `u_PulseIntensity` ($0.0$ for static high-contrast CAD/Edit view, $1.0$ for full Tron simulation glow) so toggling it costs zero state rebuilds.



---

### Recommended Order of Attack for Elfie

1. **Phase 1 (Decoupled State Buffer + Pulse Scalar):** Separate the circuit power/pulse bitmask into a flat SSBO or UBO array read by the shader, and wire in the `u_PulseIntensity` scalar so you can toggle the glow immediately while testing.
2. **Phase 2 (64-Bit Packed Quads + Vertex Pulling):** Convert chunk geometry to `uvec2` SSBO pulling using a shared static index buffer.
3. **Phase 3 (Arena Allocator + Multi-Draw Indirect):** Merge individual chunk VBOs into the single Mega-Buffer and switch the render loop to `MultiDrawIndirect` with `gl_DrawID` chunk origin lookups.

---

*Registrar's notes (Elfie, 2026-10-24):*
*(a) PHASE-NUMBERING COLLISION — read carefully on Monday: this brief's Phases 1–3 are a NEW ladder (state buffer → packed quads/PVP → arena + multi-draw indirect), distinct from Archie's 2026-10-23 multi-draw brief (Phase 1 = draw packets, built as TB3; Phase 2 = `WEBGL_multi_draw` instanced mega-call, implemented, validation pending). Refer to these as "Mega-Pipeline Phases" so the two ladders never get conflated.*
*(b) Convergences with the existing tree: §2's shared static index pattern (`0, 1, 2, 2, 1, 3`) matches the arena's existing shared pattern index buffer, and the arena itself already exists (one VBO + one VAO, first-fit ranges, 32 MB initial, doubling growth) — the Mega-Buffer is an evolution of it, not a greenfield. §3's decoupled state bitbuffer generalizes what web Phase 2 already did with per-mesh pulse/trace vec4s in instanced attributes (LK_MEGADRAW, attribs 4–8): same instinct, better encoding.*
*(c) Platform grain: Programmatic Vertex Pulling from an SSBO and `glMultiDrawElementsIndirect` are GL 4.3+ / Vulkan capabilities. WebGL2 has no SSBO and no indirect multi-draw — the web path's ceiling remains the implemented `WEBGL_multi_draw` instanced design, plus §2's macro-chunk bucketing fallback. The PVP phases therefore land on Vulkan/GL43 first; web follows its own ladder. Consistent with JD's all-pipelines direction, per-pipeline shapes.*
*(d) Determinism: the §3 state bitbuffer stays safe as long as it is a snapshot the sim writes and renderers only read — pulse state must never feed back into the sim, and scenario hashes stay untouched.*
*(e) Open questions for JD/Archie on Monday: the 6-bit packed coordinate fields assume ≤64 per axis — confirm against our actual chunk dimensions; whether binary greedy meshing's merge rule (identical material AND trace/logic state) holds against the current mesher's full behavior set; how the 6-bucket normal split interacts with the existing decideOnce culling on the web path.*
*(f) Credit: brief authored by Archie; relayed by JD 2026-10-24. Implementation order is JD's Monday call.*
