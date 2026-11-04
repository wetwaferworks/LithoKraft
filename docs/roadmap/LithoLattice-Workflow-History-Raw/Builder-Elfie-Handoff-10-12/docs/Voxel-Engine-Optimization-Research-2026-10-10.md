# Voxel Engine Optimization Research — 2026-10-10

**Purpose:** Survey of unique/interesting optimizations in voxel engines and related projects, commissioned by JD for the LithoLattice engine (~21k lines C++) and LithoKraft. Research fanned out to 7 parallel researchers (graphics, audio, pathfinding, AI, general engine, networking, storage).

**Engine context the findings are judged against:** 512 TPS deterministic simulation, deterministic-sim anti-cheat, chunked voxel worlds, voxel-based 3D spatial audio with reflections, custom bytecode world saves (SGNL v2). JD values small-and-dense, engine-level optimizations — tiny and robust over bulky.

**Methodology:** Every source below was verified from primary material (repo READMEs/docs/source files, protocol specs, author write-ups, talks). Anything that could not be verified was excluded; unverifiable items are listed in the appendix.

---

## 1. Graphics

### 1.1 Sodium — region-based, multi-draw chunk pipeline
- **What:** The reference open-source Minecraft client renderer rewrite (CaffeineMC/sodium): ~90%+ reduction in CPU render overhead via modern OpenGL usage, multi-threaded chunk meshing, and region-based buffer management.
- **How:** Chunk sections are packed into large shared GPU buffers organized as regions (8×4×8 sections per region), with an arena-style GPU allocator and compaction; chunk geometry is baked asynchronously on worker threads and uploaded via staging buffers with per-frame upload budgets; per-frame, the CPU runs a graph-based visibility flood-fill (SortedGraphIterator) doing frustum + occlusion culling, then submits all visible terrain with a single `glMultiDrawElementsBaseVertex`/`glMultiDrawElementsIndirect` call; vertices use a compact format (~20 bytes) to minimize bandwidth.
- **Sources:** [Sodium repo README](https://github.com/CaffeineMC/sodium) · [Sodium 0.1.0 release notes](https://github.com/CaffeineMC/sodium/releases/tag/mc1.16.1-0.1.0) · [dev guide: Sodium's rendering architecture](https://github.com/itsfizys/minecraft-dev-guide/blob/HEAD/21-Vulkan-And-Modern-Rendering.md)
- **Relevance to LithoLattice:** The template for our renderer — async meshing off the tick path, region-arena GPU memory, one multi-draw call per pass; meshing must never borrow tick budget at 512 TPS.

### 1.2 Nvidium — GPU-driven mesh-shader terrain backend for Sodium
- **What:** An alternate Sodium render backend (MCRcortex/nvidium; active fork denfry/nvidium) that uses NVIDIA mesh shaders to render huge render distances with near-zero CPU draw-call cost.
- **How:** It does not re-mesh: it intercepts Sodium's already-built compact chunk meshes into GPU-resident sparse bindless buffers, then draws everything with a single `glMultiDrawMeshTasksIndirectNV` call. Task+mesh shaders expand per-section quad data into triangles on-GPU; occlusion culling is GPU-driven via representative fragment tests of region/section boxes, and a compute shader builds the indirect command buffer each frame; translucent sections are sorted on-GPU. Trade-off: NVIDIA-only (Turing+, `GL_NV_mesh_shader`/`GL_NV_bindless_texture`).
- **Sources:** [Nvidium fork README](https://github.com/denfry/nvidium) · [Nvidium architecture reference](https://github.com/yumely21/yumelium/blob/HEAD/reference/nvidium-port-plan.md) · [nvidium-plus (1.21.1+) fork](https://github.com/sytereic/nvidium-plus)
- **Relevance to LithoLattice:** A long-term north star for the "GPU path" — chunk uploads as persistent GPU-resident data with all culling/submission driven on-device, freeing the CPU entirely for the 512 TPS sim.

### 1.3 Binary greedy meshing (cgerikj) — bitwise face-merge meshing at ~50–200 µs/chunk
- **What:** The fastest public greedy-meshing implementation for cubic voxels: processes 64 faces simultaneously using 64-bit bitmask operations, outputting 8-byte quads (6-bit x/y/z/width/height + 8-bit voxel type).
- **How:** Three stages: (1) build a 64×64 array of uint64 occupancy masks (0 = air, 1 = opaque) — reusable for physics/raycasts; (2) bitwise hidden-face culling (shifts/XORs over the masks) producing per-face visibility masks for all 6 faces; (3) greedy rectangle merging using word-wide row comparisons, emitting packed quads. The bundled renderer uses vertex pulling (the vertex shader expands the 8-byte quad records) and draws all chunks in one `glMultiDrawElementsIndirect` call; it also handles T-junction cracks by slightly overdrawing quad edges and computing positions in eye space.
- **Sources:** [binary-greedy-meshing README](https://github.com/cgerikj/binary-greedy-meshing) · [0fps "Meshing in a Minecraft game"](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/)
- **Relevance to LithoLattice:** Candidate core mesher — tiny code, tiny output, single-digit-microsecond remeshes mean chunk edits at 512 TPS don't stall the render thread; vertex pulling removes the need for expanded vertex buffers.

### 1.4 Voxy — far-plane LOD via merged low-detail section meshes + GPU-driven native backends
- **What:** Cortex's LOD mod for Minecraft that renders terrain far beyond vanilla render distance (configurable radius, up to 8192 sections) as merged, low-polygon geometry.
- **How:** Distant chunks are re-meshed into coarser section meshes with face occlusion/greedy merging, biome-accurate coloring, and correct translucent handling; the native OpenGL 4.6/Vulkan backends run a GPU-driven pipeline — compute-shader culling and traversal, GPU-generated indirect draw lists, multi-draw indirect, SSBO-based scene/visibility data, and GPU-driven Hi-Z traversal — while a compatibility Blaze3D renderer shows the fallback costs (one draw call per chunk, no GPU culling). Active fork "Voxyrium" adds Vulkan/MoltenVK and documents this pipeline.
- **Sources:** [MCRcortex/voxy repo](https://github.com/MCRcortex/voxy) · [Voxyrium fork README](https://github.com/debuneko233/voxy) · [NeoForge port README](https://github.com/JustinTHChapman/voxy)
- **Relevance to LithoLattice:** The pragmatic model for render distance beyond meshed chunks — coarse merged meshes far, full detail near; its GPU-driven culling is what our LOD layer should aspire to once the software renderer proves the approach.

### 1.5 Transvoxel + LodOctree — crack-free smooth-terrain LOD
- **What:** The established algorithm for smooth (non-blocky) voxel LOD: Eric Lengyel's Transvoxel meshes scalar-field voxels with special "transition cells" at LOD boundaries so adjacent levels stitch without cracks.
- **How:** The world is stored in an octree (or clipbox/concentric-box paging in newer builds); each node renders at a fixed LOD, and where a coarse node meets a fine node, transition cells generate the connecting geometry. Voxel Tools for Godot ships `VoxelMesherTransvoxel`, multi-LOD paging via `VoxelLodTerrain`, and baked AO for blocky mode — a full production implementation with C++ golden tests and a Rust port.
- **Sources:** [godot_voxel Rust port README](https://github.com/sandsaber/godot_voxel) · [upstream Voxel Tools docs](https://voxel-tools.readthedocs.io)
- **Relevance to LithoLattice:** Directly applicable if/when LithoKraft wants smooth terrain — but it belongs to the *rendering* layer, not the deterministic sim: coarse meshes are a client-side fiction over authoritative voxel data.

### 1.6 Voxel ambient occlusion (0fps / Mikola Lysenko) — per-vertex AO baked at mesh time
- **What:** The canonical cheap AO for voxel engines: compute each face-vertex's occlusion from just 3 adjacent cells, with zero view dependence and near-zero cost.
- **How:** For each vertex, sample the two side voxels and the diagonal (pushed out along the face normal): `vertexAO = (side1 && side2) ? 0 : 3 - (side1 + side2 + corner)`, yielding 4 occlusion levels mapped to brightness (e.g. 1.00/0.82/0.65/0.45). Integration with greedy meshing is simple: only merge faces whose vertices share the same AO values. A subtle fix addresses interpolation anisotropy — quad triangle diagonals are flipped so the higher-contrast diagonal isn't split (`if (a00 + a11 > a01 + a10) flip`), otherwise AO gradients visibly tear.
- **Sources:** [0fps "Ambient occlusion for Minecraft-like worlds"](https://0fps.net/2013/07/03/ambient-occlusion-for-minecraft-like-worlds/) · [binary-greedy-meshing v1.0.0 branch (baked AO)](https://github.com/cgerikj/binary-greedy-meshing/tree/v1.0.0)
- **Relevance to LithoLattice:** High visual payoff per byte — bake AO into the same compact quad records at meshing time so lighting never touches the frame or tick path.

### 1.7 Texture arrays over atlases (0fps / Mikola Lysenko)
- **What:** Texture atlases break with greedy meshing: greedy quads produce UVs > 1.0 (tiling), atlas mipmapping bleeds neighboring tiles, and automatic LOD selection goes wrong at tile borders. Array textures are the clean fix.
- **How:** Put each block texture on its own layer of a texture array (`sampler2DArray`) and index by block-type ID: mipmapping and wrapping then work correctly per layer with zero bleeding and no per-tile mipmap tricks. The article also documents the atlas workaround (per-tile periodic mipmaps + a weighted 4-tap sampling shader) for platforms without arrays.
- **Sources:** [0fps "Texture atlases, wrapping and mip mapping"](https://0fps.net/2013/07/09/texture-atlases-wrapping-and-mip-mapping/)
- **Relevance to LithoLattice:** Locks the texturing design early — texture array indexed by block ID in the quad record; no atlas seam hacks, no per-mesh material state changes, composes with the 8-byte greedy quads.

### 1.8 GPU-driven rendering: one multi-draw call + compute culling
- **What:** Three independent implementations of the same end state: the CPU submits essentially one draw call per frame while the GPU decides what's visible and what geometry gets emitted.
- **How:** VoxelMVP (OpenGL 4.6): one `glMultiDrawElementsIndirectCountARB` per frame; a compute shader frustum-culls chunks into visible draw commands using subgroup arithmetic; binary greedy meshing, persistent mapped buffers, and a toroidal chunk SSBO keep everything resident. PlanetSmith's devlog builds the same idea with octree-based frustum culling fully on-GPU plus a compressed vertex format (~10× speedup, "over a billion vertices on screen"). The Unity voxel engine splits chunks into 6 face-orientation groups so the culling shader can discard entire back-facing chunks cheaply. A compute-shader port of greedy meshing exists in Rust/WebGPU form (one workgroup per axis-plane, atomic-append quads, vertex-pull rendering).
- **Sources:** [SSeanPP/VoxelMVP README](https://github.com/SSeanPP/VoxelMVP) · ["I Built a Custom GPU-Driven Rendering Pipeline (10x Faster)" — PlanetSmith devlog](https://www.youtube.com/watch?v=LzivS_KzffY) · [unityvoxelengine rendering optimizations](https://github.com/gamengineer314/unityvoxelengine)
- **Relevance to LithoLattice:** The convergence point for our renderer v2 — per-chunk draw calls are the CPU cost that matters at high chunk counts; combine with face-orientation batching and compute culling once the CPU multi-draw path (Sodium-style) is proven.

---

## 2. Audio

### 2.1 UnityVoxelAudio — voxel flood-fill occlusion + ray-survey reverb
- **What:** Unity/FMOD integration computing realtime audio occlusion via voxel flood-fill and reverb from material rays — the closest public analog to a voxel-native spatial audio layer.
- **How:** Occlusion is computed by flood-filling outward from the listener through the voxel grid, storing distance plus the last directly visible voxel index per cell — voxel-space visibility propagation rather than per-source raycasts. Reverb is solved by casting rays from each event instance and the listener and reading back the surrounding surface materials. Reported cost: 200 looping sounds with reverb + occlusion ≈ 0.18 ms main thread constant (1.26 ms burst at start), with one worker thread kept busy on the acoustic updates.
- **Sources:** [README — performance numbers and "Technical Details"](https://github.com/zombie1111/unityvoxelaudio)
- **Relevance to LithoLattice:** Direct template for our voxel-counted reverb and wall-occlusion work — their flood-fill-from-listener approach is how our 512 TPS sim can amortize occlusion updates across ticks instead of raycasting per voice.

### 2.2 Valve Steam Audio — physics-based occlusion, reflections, convolution reverb, HRTF
- **What:** Free SDK simulating sound propagation from scene geometry and materials: HRTF binaural direct sound, raycast occlusion, partial occlusion, and convolution reverb generated from ray-traced impulse responses (realtime or baked).
- **How:** HRTF-based binaural rendering handles direct sound with low CPU overhead (claimed hundreds–thousands of sources per core). Occlusion uses your existing scene geometry (no separate occlusion volumes); partial occlusion applies to non-point sources so sound "leaks" realistically. Reverb is computed by simulating reflections off surfaces using their acoustic material properties (glass vs. carpet), producing per-scene convolution reverb — either realtime or precomputed at data points, plus ambisonics encode/decode for sound-field ambience.
- **Sources:** [User guide — spatialize source / occlusion / ambisonics](https://github.com/xorandom/steam-audio/blob/HEAD/unity/doc/guide.rst) · [official manual PDF (v2.0-beta.5)](https://github.com/ValveSoftware/steam-audio/releases/download/v2.0-beta.5/steamaudio_manual_2.0-beta.5.pdf)
- **Relevance to LithoLattice:** Validates our geometry-driven approach — our voxel-DDA occlusion and voxel-counted reverb are a dense-grid version of Steam Audio's physics-driven reverb; their partial-occlusion concept is worth stealing for doorways/edges.

### 2.3 Google Resonance Audio — ambisonics-first spatialization for hundreds of sources
- **What:** Open-source (Apache 2.0) spatial audio SDK built on higher-order ambisonics + HRTFs, designed to spatialize hundreds of simultaneous 3D sources on mobile-class CPUs, plus a constant-complexity Spectral Reverb.
- **How:** Each source is encoded into an ambisonic sound field (Ambix ACN/SN3D); a single HRTF decode turns the whole field binaural — cost scales with field order, not source count. Features include near-field diffraction rendering for close sources, source directivity and spread, and a precomputed-reverb path that shifts convolution cost offline.
- **Sources:** [Google open-source announcement](https://blog.google/products-and-platforms/products/google-ar-vr/open-sourcing-resonance-audio/) · [launch details](https://blog.google/products-and-platforms/products/google-ar-vr/resonance-audio-multi-platform-spatial-audio-scale/)
- **Relevance to LithoLattice:** Alternative to per-voice HRTF — if our voice count ever outgrows per-voice panning, an ambisonic intermediate bus gives constant-cost mixing for ambient beds and mob chatter.

### 2.4 ZzFX — seeded parametric synth in hundreds of bytes
- **What:** A procedural sound-effect synthesizer (by Frank Force, ex-DOOM/Volition) where each SFX is a short parameter array, with seeded random sounds playable from ~4 bytes; the ZzFXM companion plays MOD-pattern music from the same synth.
- **How:** One function renders a buffer: phase-modulated oscillator (square/saw/sine/noise/triangle selectable), ADSR-ish envelope, frequency slide, vibrato, noise, bit-crush, delay/echo — all from a compact parameter vector, zero audio assets. Seeded generation means an entire SFX library fits in code; ZzFXM extends it to MOD-format tracker songs with variable-length patterns and variable channel counts to avoid storing empty data.
- **Sources:** [ZzFX README](https://github.com/killedbyapixel/zzfx) · [ZzFXM README](https://github.com/killedbyapixel/zzfxm/blob/HEAD/README.md)
- **Relevance to LithoLattice:** Philosophy match for our SID3/DMG-style synth voices — ZzFX's parameter-array-as-asset model is a proven way to ship hundreds of SFX with near-zero memory, and its seeded-sound trick could give LithoKraft infinite deterministic sound variety from 64-bit seeds.

### 2.5 SoLoud — voice virtualization, priorities, and inaudible-voice policies
- **What:** Portable C++ audio engine (Jari Komppa) with explicit voice management: a bounded pool of active voices, per-voice protection, and policies for what inaudible voices do.
- **How:** `setMaxActiveVoiceCount()` caps mixed voices; when the cap hits, the engine virtualizes the quietest — `setInaudibleBehavior(handle, mustTick, kill)` chooses whether an inaudible voice keeps its playback position ticking (so it resumes correctly) or is killed, and `setProtectVoice()` pins critical voices (music, UI) against stealing. Voices are grouped into buses (music/SFX/voice) for category ducking and volume. It also ships procedural sources directly — waveform oscillators, an sfxr-style generator, C64 SID and VIC-20 chip emulation, and tracker-song playback.
- **Sources:** [SoLoud feature overview](https://soloud.handmade.network/) · [issue #205 — max-voice repro](https://github.com/jarikomppa/soloud/issues/205)
- **Relevance to LithoLattice:** The reference design for our voice-stealing policy — their "tick but don't mix" inaudible behavior is exactly what our distance-culled voices need (mobs stay in sync without costing mix time), and protect-voice covers music/UI priority.

### 2.6 miniaudio — single-header device threading, lock-free ring buffers, node graph
- **What:** Single-file C/C++ audio library (David Reid / mackron) owning the whole stack from device I/O to a high-level engine: device thread, node-graph mixer, spatializer/panner, delay/echo, waveform + noise generators, and decoders.
- **How:** The audio device runs its mixing callback on its own thread; thread handoff uses lock-free ring-buffer primitives (ma_rb commit/read), and the newer node-graph API lets DSP nodes (filters, spatializer, delay) be wired into a processing graph the device consumes. The engine layer adds sound management, resource streaming, and a 3D spatializer with attenuation models on top — all in one vendored header with zero external dependencies beyond libc and the OS audio backend.
- **Sources:** [README](https://github.com/smcpeak/miniaudio) (fork mirroring mackron's README) · [CHANGES.md](https://github.com/mackron/miniaudio/blob/HEAD/CHANGES.md)
- **Relevance to LithoLattice:** Direct model for our audio threading — our lock-free SPSC command queue mirrors miniaudio's callback-thread + ring-buffer handoff pattern, and its spatializer shows how to layer panning/attenuation as a DSP node rather than per-voice special cases.

### 2.7 "Level Up! Procedural Game Music and Audio" — Chris Nash, ADC 2025
- **What:** Audio Developer Conference 2025 talk presenting practical techniques for fully procedural, gameplay-driven music and SFX — the game controls every note of the soundtrack rather than crossfading stems.
- **How:** Covers technologies for procedural music/audio in Unity and Unreal, deconstructed through two demo games: layered/stinger arrangements escalating to live data sonification (scene-driven musical cues) and "diagetic" music where NPC performers in the world are affected by the player.
- **Sources:** [YouTube — ADC channel](https://www.youtube.com/watch?v=2Lr6yL64Ptc) ("Level Up! Procedural Game Music and Audio - Chris Nash - ADC 2025")
- **Relevance to LithoLattice:** Blueprint for making our synth engine gameplay-aware — our 512 TPS sim can drive live musical parameters (danger level → key/tempo shifts) instead of looping static tracks, and the "diagetic music" concept fits LithoKraft's diegetic world.

### 2.8 "Unbreaking Immersion" — INSIDE audio talk (Martin Stig Andersen & Jakob Schmid)
- **What:** The 2016 talk/slide deck on the 5-year audio collaboration behind Playdead's INSIDE: foley and voice design, seamless death-respawn audio, and using Wwise as a compositional tool.
- **How:** Details the Wwise setup and custom engine tooling for audio features, how foley/voice sell the character, and — critically — performance results and considerations: real measured numbers for a shipped game's audio budget, plus the death-respawn cycle handling (audio state must survive and reset cleanly with the sim).
- **Sources:** [Talk page with slide PDF](http://schmid.dk/talks/2016-06-16-wwise/schmid-Wwise_2016-INSIDE_Audio.pdf) ("Unbreaking Immersion", 2016-06-16)
- **Relevance to LithoLattice:** The shipped-game reality check — INSIDE's death-respawn audio handling is directly applicable to our Fragment-loss/despawn mechanics, and their performance discipline is the standard our audio layer should be measured against.

---

## 3. Pathfinding

### 3.1 Jump Point Search (JPS) — production C/C++ implementation with deterministic contract
- **What:** A native C/C++ JPS pathfinder for 8-connected uniform grids that claims 45–54× A* speedups with explicitly documented deterministic-lockstep support.
- **How:** JPS exploits path symmetry on uniform-cost grids: after moving from parent p to node n in some direction, n's neighbors split into "natural" (pruned — reachable cheaper through another path) and "forced" (kept — only passable around a nearby obstacle). Instead of enqueuing every neighbor, it "jumps" straight along cardinal and diagonal directions, stopping only at jump points (forced-neighbor cells, the goal, or walls). This implementation adds a lazy cardinal jump table that fills on demand, integer costs (cardinal 1000, diagonal 1414) with octile heuristic, no-corner-cutting by default, SIMD hot paths (SSE2/NEON), and obstacle edits that invalidate only affected rows/columns — no full table rebuild.
- **Sources:** [qiqian/jps](https://github.com/qiqian/jps) (README documents the deterministic-lockstep section)
- **Relevance to LithoLattice:** JPS is the cheapest near-term win for long-distance voxel paths — integer costs and fixed tie-breaking map directly onto the 512-TPS deterministic-sim requirement; the lazy jump table handles block edits without rebuilds.

### 3.2 HPA* with portal abstraction on 3D voxel-like grids
- **What:** A reusable hierarchical grid pathfinder (Bevy/Rust) that plans global routes across 2D, layered 2.5D, and 3D voxel-like grids using HPA*-style portal abstraction, with dirty-region hierarchy rebuilds and sliced/async query orchestration.
- **How:** The grid is partitioned into clusters; each cluster boundary gets "portal" (entrance) nodes, and an abstract graph connects portals between clusters. A query first A*'s the tiny abstract graph (start → portal → … → goal cluster), then refines only the concrete cells along the abstract route — so search cost scales with abstract hops, not grid cells. Changed regions mark dirty chunks and only that region's hierarchy rebuilds; queries are orchestrated as resumable/sliced work across frames, and one-to-many flow fields are built on the same grid.
- **Sources:** [julien-blanchon/saddle-ai-hpa-pathfinding](https://github.com/julien-blanchon/saddle-ai-hpa-pathfinding) — README (API section: `query_path_sliced`, `build_flow_field`, dirty-region rebuilds)
- **Relevance to LithoLattice:** The cluster→portal model is a natural fit for chunked voxel worlds (cluster size = chunk or super-chunk, portals = cross-chunk entrances); dirty-region rebuilds handle block edits without full re-bakes.

### 3.3 Flow fields inside a deterministic C++17 engine
- **What:** A C++17 engine with a deterministic, replay-verifiable fixed-step sim that bakes nav grids, runs A*, and drives crowd agents on flow fields.
- **How:** One Dijkstra flood-fill expands backward from the goal over the nav grid, writing an integration cost per cell; each cell then stores a vector pointing at its lowest-cost neighbor. All agents heading to the same goal sample that one shared field (O(1) lookup per agent) instead of running N individual A* searches — crowd cost becomes roughly constant regardless of agent count. Because the sim is fixed-step with deterministic RNG and snapshot rewind, the same field build + agent sample sequence replays bit-identically.
- **Sources:** [ikey168/charta](https://github.com/ikey168/charta) — README feature matrix · [Unity Pathfinding: DOTS, Flow Fields, RVO](https://www.youtube.com/watch?v=DoMvgunI0nI) — devlog on a Burst-compiled flow-field system for hundreds of units
- **Relevance to LithoLattice:** Flow fields are the deterministic crowd solution — one field bake per shared destination, bit-identical replay across lockstep clients; pairs naturally with the voxel engine's chunked nav bake.

### 3.4 Baritone — voxel-native A* with movement-model semantics
- **What:** The canonical Minecraft pathfinder bot: A* over live Minecraft chunks with a full movement model (jumps, sprint-jumps, parkour, ladders, swimming) and goals that can break/place blocks as part of the path.
- **How:** Search runs on the loaded chunk data as a 3D grid, but the neighbor function is the real Minecraft movement physics — edge costs encode jump costs, fall damage risk, block-breaking time, and tool availability, not just distance. Path execution is interleaved with replanning: blocked/partial paths trigger segment-level repair rather than full replans.
- **Sources:** [cabaletta/baritone](https://github.com/cabaletta/baritone) · [jtdoria/mineflayer-baritone](https://github.com/jtdoria/mineflayer-baritone) — JS port README (parkour moves, block breaking/placing, partial path handling)
- **Relevance to LithoLattice:** For a voxel game, the "neighbor function" matters more than the search algorithm — Baritone proves cost-model realism (movement primitives, dig/place costs) is where to invest once JPS/HPA* handles the search speed.

### 3.5 Time-sliced / budgeted A* across ticks
- **What:** The standard game-industry pattern for bounding pathfinding CPU: spread search work across frames with a per-frame budget and resumable search state.
- **How:** Two composable techniques. (a) Budget-gated completion: run pathfinds, check elapsed time after each, and stop pathfinding for the frame once the ms budget is exceeded — waiting NPCs move toward the target's general direction instead of blocking. (b) Token-aware resumable A*: make the search a stateful object (open list + map data + current best node); feed it from the per-frame path-request processor until the token allotment runs out, then re-enqueue unfinished requests with their state intact. Crucially, re-enqueue unfinished requests at the *front* of the queue in a fixed order so all sim clients produce identical results.
- **Sources:** [SJSU CS134 Lecture 14 — Navigation Game Loop](https://jared.finder.org/sjsu-2018spring/lecture14.pdf) · [vr48/dunecity pathfinding scaling analysis](https://github.com/vr48/dunecity/blob/HEAD/analysis/pathfinding_scaling.md)
- **Relevance to LithoLattice:** At 512 TPS, pathfinding must be tick-budgeted, not frame-budgeted — the resumable-search + front-of-queue deterministic re-enqueue pattern is directly portable to fixed-tick path budgets.

### 3.6 D* Lite — incremental replanning for dynamic voxel worlds
- **What:** An incremental heuristic search that repairs paths after obstacle changes instead of replanning from scratch, designed for agents that discover obstacles while moving.
- **How:** D* Lite searches *backward* from goal to start and maintains two values per cell — `g(n)` (current best cost-to-goal) and `rhs(n)` (one-step lookahead over neighbors). Cells where `g ≠ rhs` are inconsistent and sit in a priority queue. When an obstacle appears, only the affected cells' `rhs` values change; replanning reprocesses just the inconsistent wavefront rather than the whole grid.
- **Sources:** [harshadithya-cs50/dynamic_ugv_pathfinding](https://github.com/harshadithya-cs50/dynamic_ugv_pathfinding) — README documents the g/rhs model · [coldhac/pathfinding-workshop](https://github.com/coldhac/pathfinding-workshop)
- **Relevance to LithoLattice:** For player-driven block edits near active agents, D* Lite's wavefront repair is cheaper than HPA* dirty-region rebuilds at small edit scope; best combined with time-slicing for per-tick budget caps.

### 3.7 Subgoal graphs — optimal precomputation for 8-neighbor grids
- **What:** A preprocessing technique (Uras, Koenig, Hernández, ICAPS 2013) that compresses an 8-neighbor grid into a tiny graph over "subgoal" cells, on which A* runs near-instantly while remaining optimal.
- **How:** A cell is a subgoal if it sits at an obstacle corner (diagonal blocked, both cardinals free). Edges connect subgoals that are "direct-h-reachable" (connected by an optimal octile-distance path with no other subgoal between). A query connects start and goal into this sparse graph, A*'s it, then expands each abstract segment into concrete cells. The abstract graph is orders of magnitude smaller than the grid, and unlike HPA* the paths stay optimal.
- **Sources:** [lanboost/lanboostpathfindingsharp](https://github.com/lanboost/lanboostpathfindingsharp) — C# library; README cites the ICAPS 2013 paper
- **Relevance to LithoLattice:** Strong alternative to HPA* clusters for long cross-map paths in mostly-open voxel terrain — optimality preserved, and the subgoal set is cheap to maintain incrementally under edits.

### 3.8 Theta* / Lazy Theta* and navmesh-from-voxels (sparse voxel octrees)
- **What:** A UE5 3D pathfinding plugin built on sparse voxel octrees offering A*, Theta*, and Lazy Theta* — any-angle search that yields straight, natural paths instead of grid-constrained zigzags.
- **How:** The octree gives multi-resolution 3D space (fine leaf voxels near the agent, coarse voxels at distance). Theta* extends A* with a line-of-sight check: when expanding a neighbor, if there's direct LOS from the *parent's* parent, the path shortcuts through it — producing any-angle segments on the grid. Lazy Theta* defers the expensive LOS check until the node is actually expanded, keeping near-optimal quality at lower cost. Final paths are smoothed with Catmull-Rom curves.
- **Sources:** [titanic998/nav3d](https://github.com/titanic998/nav3d) — README (algorithm selection; sparse voxel octree core)
- **Relevance to LithoLattice:** Any-angle paths are what make voxel agents look non-robotic; LOS-shortcutting is also a cheap post-pass over JPS/HPA* waypoint output rather than a full search replacement.

---

## 4. AI

### 4.1 Sparse spatial hashing for entity queries
- **What:** `queelius/sparse_spatial_hash` — a header-only C++20 N-dimensional sparse spatial hash grid for O(1) neighbor/radius queries, explicitly listing AI pathfinding and collision broad-phase among its game use cases.
- **How:** World space is divided into uniform cells, but only occupied cells are stored in a hash table (Morton/Z-order curve ordering for cache locality). Queries hash the cells covering a radius instead of scanning all entities. It offers incremental per-entity updates (only entities that changed cells, typically ~1% per tick) instead of full rebuilds — its own benchmarks claim ~2ms incremental update vs ~80ms rebuild for 10M particles — plus inline "small vector" storage per cell (5–40% speedup, +256 bytes per occupied cell). Insert O(1), radius query O(k), build O(n); memory ~60,000× less than a dense grid in sparse worlds.
- **Sources:** [repo README](https://github.com/queelius/sparse_spatial_hash) (mechanism, benchmarks, comparison tables)
- **Relevance to LithoLattice:** Matches the "small-and-dense" ethos — a header-only, zero-overhead library for mob neighbor lookups and sound-propagation queries without inventing a bespoke structure.

### 4.2 Entity Activation Range (AI LOD / dormant AI)
- **What:** The Spigot/Paper "entity activation range" system skips AI/pathfinding ticks for entities beyond configurable per-type distances from players, with immunity checks forcing ticks for entities that are falling, damaged, or newly spawned.
- **How:** Each tick, `checkIfActive`-style logic compares entity distance to player against ranges (e.g., monsters 32, animals 32, raiders 48, misc 16). Outside range, entities skip most of their tick (AI, movement) unless "immune" (falling, in water/lava, riding, being attacked, just spawned — newly spawned get ~10s grace ticks). Community server docs also describe a variant that skips 1/4 of ticks even for in-range but non-immune entities, and Epic's Mass framework generalizes the same idea: `MassLOD` processors that tick entities at different rates based on fragment settings/LOD.
- **Sources:** [YouHaveTrouble/minecraft-optimization](https://github.com/YouHaveTrouble/minecraft-optimization) (config rationale) · [ServerCore docs — activation-range](https://github.com/Wesley1808/ServerCore/blob/main/docs/config/DEFAULT.md) (immunity checks, skip-non-immune) · [Megafunk/MassSample](https://github.com/Megafunk/MassSample) (MassLOD per-entity tick-rate processors)
- **Relevance to LithoLattice:** The reference model for AI LOD — full ticks near players, throttled tiers mid-range, dormant outside, all tick-count-driven so it can be made deterministic.

### 4.3 Per-player mob caps & section-cached spawn budgeting
- **What:** Paper's per-player mob spawning distributes mob caps per player rather than globally, and community forks cache mob counts per chunk *section* to avoid per-tick entity scans when enforcing caps.
- **How:** Instead of one global cap that lets one player's farm starve everyone else's spawns, each player's local neighborhood gets its own cap budget (with per-category limits, e.g., monsters 70, creatures 10, ambient 15, water-ambient 20). Spawn attempts check despawn ranges (soft ~30 blocks = random despawn chance, hard = instant despawn, tuned to `(simulation-distance*16)+8`) before committing. The geary-papermc fork's release notes document caching "mob cap counts per section" and validating spawn spots up front, cutting per-tick entity scans entirely.
- **Sources:** [minecraft-optimization — per-player-mob-spawns/despawn-ranges](https://github.com/YouHaveTrouble/minecraft-optimization) · [ServerCore mobcaps config](https://github.com/Wesley1808/ServerCore/blob/main/docs/config/DEFAULT.md) · [geary-papermc v0.35.0-dev.0 release notes](https://github.com/MineInAbyss/geary-papermc/releases/tag/v0.35.0-dev.0)
- **Relevance to LithoLattice:** A deterministic spawn-budget pattern — per-player caps plus chunk-local cached counts means spawn logic needs no global entity scan per tick.

### 4.4 Flow-field tiles for crowd steering at scale
- **What:** `yoreei/crowd_pathfinder` — a real C++ implementation of Elijah Emerson's "Crowd pathfinding and steering using flow field tiles" (Supreme Commander technique, Game AI Pro 360), benchmarked against stock UE5 pathfinding.
- **How:** Instead of one A* per unit, a single goal triggers: cost-field build/update (dirty-checked), then a wavefront propagation pass (two passes — a line-of-sight pass then the main pass — approximating an Eikonal solve), then conversion of the distance field into per-tile direction vectors. Every agent then just looks up its current tile's vector: O(1) steering per agent, with wave propagation cost independent of unit count. Their measured results: at 200 units, flow-tile total ~1.6ms vs ~6.3ms for stock per-agent pathfinding on simple maps (~4× faster); the gap widens on mazes.
- **Sources:** [repo README + benchmarks](https://github.com/yoreei/crowd_pathfinder) (cites Emerson, Game AI Pro 360)
- **Relevance to LithoLattice:** Deterministic, tick-friendly crowd steering — one shared, cacheable field per goal; per-mob cost is a single tile lookup per tick, ideal for budgeted 512-TPS AI.

### 4.5 Voxel DDA line-of-sight for mob sight
- **What:** `iquxae/mc-alpha-server_cpp` added native mob attacks with a voxel DDA `ray_trace_clear` function (only fluids let sight through; missing chunks read as air) used for eye-to-eye line-of-sight checks in attack decisions.
- **How:** For each attack evaluation, the server marches a 3D DDA through the voxel grid from attacker eye to target eye and tests block solidity along the way. LOS is re-verified at the exact strike instant of a windup→strike→recovery attack state machine rather than only at attack start, so cover taken mid-swing cancels the hit.
- **Sources:** [commit — "native mob attacks, arrows, and line of sight"](https://github.com/iquxae/mc-alpha-server_cpp/commit/52a8950574200ae57e54d04b5b84c2e18fc0ec78) · [voxel-game docs/INVESTIGATION.md](https://github.com/npal78229-eng/voxel-game/blob/HEAD/docs/INVESTIGATION.md)
- **Relevance to LithoLattice:** The canonical voxel-native sight check — DDA through chunk data is deterministic and cheap; re-verifying at strike instant is a determinism-safe combat pattern.

### 4.6 Batched sensory ticking + pathfinding budget caps
- **What:** A documented small-engine approach: run all NPC perception (range, FOV, LOS, hearing, night/fog penalties) on a shared 0.2s timer rather than per-frame per-NPC, and hard-cap expensive pathfinding to a fixed number of computations per frame.
- **How:** Perception becomes a scheduled batch job: every 0.2s, each NPC within relevant range runs sight cone + hearing checks once, instead of every frame. Pathfinding is queue-gated — max 4 path computations per frame regardless of NPC count, with requests queued in order (deterministic if the queue is insertion-ordered by tick). Distant NPCs degrade further: simplified state machine with no pathfinding recalculation at mid range, frozen/slow-tick at far range (three-tier distance LOD).
- **Sources:** [dev.to — character forge / game engine build (Part 5)](http://dev.to/plastikelectrik/i-built-a-local-ai-character-forge-and-a-browser-game-engine-on-a-100y-vps-no-gpu-no-excuses-1oj7) · [rowatcher.com — shared scheduler + distance-tiered NPC LOD](https://rowatcher.com/news/stop-scripting-roblox-ai-npcs-before-you-read-this)
- **Relevance to LithoLattice:** Direct template for how `audibleSounds()`-driven hunting stays budgeted: hearing/sight events computed on a fixed sensory tick (e.g., every N of 512 ticks), with per-tick query caps that are identical on every client.

### 4.7 Deterministic fixed-step sim + hash-based golden-trace tests
- **What:** The field-converged recipe for deterministic AI: fixed-timestep accumulator (Glenn Fiedler's "Fix Your Timestep!"), fixed-point/separate render interpolation, and replay/golden-trace tests that hash the world state every N ticks to catch desyncs.
- **How:** Simulation advances in exact fixed slices (e.g., 1/512s) via accumulator; rendering interpolates between the last two sim states and never feeds back into the sim (no wall-clock reads in sim code — one engine fails its build on unseeded RNG). Regression-wise, projects record "golden traces" (hashes of world state per tick) and re-run a fixed tick count (one README documents a 600-tick deterministic selftest across environments); any divergence in AI, separation, or economy shows up as a hash mismatch.
- **Sources:** [Fix Your Timestep!](https://gafferongames.com/post/fix_your_timestep/) · [simcraft docs/research/continuous-time.md](https://github.com/cemphlvn/simcraft/blob/HEAD/docs/research/continuous-time.md) · [openra2 README — 600-tick deterministic selftest](https://github.com/harshilmathur/openra2/blob/HEAD/apps/client/README.md)
- **Relevance to LithoLattice:** This is the anti-cheat foundation made explicit — fixed 512Hz sim, hash-per-tick golden tests for AI behavior, and steering/perception never running at render rate.

### 4.8 Utility AI with staggered evaluation
- **What:** AI and Games' "How Utility AI Helps NPCs Decide What To Do Next" (AI 101) explains utility scoring (The Sims lineage, Dragon Age: Inquisition's behavior-decision architecture from Game AI Pro 3) as the performance-friendly alternative to deep behavior trees for many-agent decision-making.
- **How:** Each action has considerations scored 0–1, normalized, combined (weighted sum/multiplicative), and the max (or softmax-weighted random pick) wins. Performance tricks: bucket actions by category first, evaluate utility on a slow tick separate from per-frame steering (utility decides the goal; steering executes), cache scores and re-evaluate only when relevant world state changes, and replace per-frame BT service nodes with timer-driven evaluations.
- **Sources:** [AI and Games — "How Utility AI Helps NPCs Decide What To Do Next | AI 101" (YouTube)](https://www.youtube.com/watch?v=p3Jbp2cZg3Q) · [Game AI Pro 3, Ch.31 — Dragon Age: Inquisition utility scoring (PDF)](https://www.gameaipro.com/GameAIPro3/GameAIPro3_Chapter31_Behavior_Decision_System_Dragon_Age_Inquisition%E2%80%99s_Utility_Scoring_Architecture.pdf)
- **Relevance to LithoLattice:** Utility-on-a-slow-tick gives deterministic, cheap per-mob decisions (a handful of arithmetic considerations, no tree traversal), and weighted-random selection can use the seeded sim RNG so all clients agree.

---

## 5. General Engine

### 5.1 SoA chunk storage + data-oriented design discipline
- **What:** Flat `uint8_t` arrays per attribute (SoA) instead of Block objects (AoS); the mesher streams only the arrays it needs.
- **How:** cpp-hpc-voxel-engine's ADR-001 chose SoA flat arrays for chunk voxels explicitly for cache coherency and no padding overhead, accepting that sparse per-block data (inventories) moves to a separate map. Mike Acton's CppCon 2014 talk frames the whole philosophy: design transforms around data, not object hierarchies — one mesher pass over one dense array, prefetch-friendly, no cache pollution from unused fields.
- **Sources:** [cpp-hpc-voxel-engine ARCHITECTURE.md](https://github.com/kabilankumar36/cpp-hpc-voxel-engine/blob/HEAD/ARCHITECTURE.md) · [CppCon 2014: Mike Acton "Data-Oriented Design and C++"](https://www.youtube.com/watch?v=rX0ItVEVjHc)
- **Relevance to LithoLattice:** Our 512-TPS signal sim iterates block state per tick — SoA storage means each system reads exactly what it needs with zero cache waste.

### 5.2 Paletted, bit-packed chunk sections (Minecraft's model)
- **What:** 16³ sections stored as a small local palette of unique block states + bit-packed indices sized to ceil(log2(paletteLen)), min 4 bits.
- **How:** The fandnel protocol docs detail the format: sections with one state collapse to a single palette entry (no per-block storage); thresholds flip to direct global IDs past 16 unique states. A Rust implementation (ferris-craft) shows it concretely: an enum `PalettedContainer { Single(T), Indirect { palette: SmallVec<[T;16]>, data: BitVec }, Direct { data: BitVec } }` with LSB-first packing that never spans a u64 boundary.
- **Sources:** [fandnel chunk-format.md](https://github.com/fandmc/fandnel/blob/HEAD/minecraft-java-protocol-docs/versions/1.21.8/chunk-format.md) · [ferris-craft mc-02-chunk-storage.md](https://github.com/kaelsensei/ferris-craft/blob/HEAD/skills/mc-02-chunk-storage.md)
- **Relevance to LithoLattice:** Our custom bytecode saves should use paletted sections — massive compression win on uniform regions (stone shells, air), and it decouples memory footprint from world height.

### 5.3 Worker-pool chunk pipeline with a deliberately narrow thread boundary
- **What:** A C++ voxel engine's verified threading model: worker pool does all CPU chunk work, main thread owns the GPU context exclusively, and they communicate through one bounded queue.
- **How:** From danielwliu07/voxel-engine's design doc: workers run Perlin fill + greedy mesh + visibility flood fill (~1 ms/chunk), producing `FinishedChunk` results pushed to a result queue; the main thread drains a *bounded number per frame* and performs only the VBO/VAO upload — "workers never touch GL, the main thread never meshes." Notably, they shipped a Vyukov bounded MPMC lock-free queue, benchmarked it against a mutex pool, and kept the mutex pool because at ~1 ms jobs the queue was never the bottleneck — a perfect small-and-dense lesson. The same doc documents greedy meshing (5.33x fewer quads, CI-gated), hierarchical culling, and dirty-flag-driven re-meshing only on edited chunks.
- **Sources:** [danielwliu07/voxel-engine docs/design.md](https://github.com/danielwliu07/voxel-engine/blob/HEAD/docs/design.md)
- **Relevance to LithoLattice:** This is our meshing/render threading blueprint — bounded drain prevents upload bursts from stalling the 512-TPS tick, and dirty flags mean only edited sections re-mesh.

### 5.4 enkiTS-style lightweight task scheduler (zero-allocation jobs)
- **What:** A small, permissively licensed (zlib) C/C++ task scheduler with lock-free work stealing, task priorities, and dependencies.
- **How:** enkiTS is designed for *up-front allocation friendliness* — task sets can be allocated once at startup and reused, so scheduling incurs zero allocations during steady state. C++ usage is a few lines: define an `ITaskSet::ExecuteRange`, `Initialize()`, `AddTaskSetToPipe`, `WaitforTask`. Priorities let mesh-near-player jobs preempt background generation.
- **Sources:** [dougbinks/enkiTS](https://github.com/dougbinks/enkiTS/) · [feiyunwill/enkiTS (documented fork)](https://github.com/feiyunwill/enkiTS)
- **Relevance to LithoLattice:** A few hundred lines of scheduler beats a hand-rolled thread pool — priorities for player-near chunks, zero per-tick allocation, no dependency bloat.

### 5.5 Deterministic parallelism: conflict-DAG stages + ordered command buffers
- **What:** The hard problem of threading a deterministic sim — how to run systems in parallel and still get bit-identical results.
- **How:** mattneel/gkz (a deterministic sim kernel in Zig) implements exactly this: each system declares Read/Write access in its query type; the scheduler builds a conflict-DAG into greedy stages; systems in one stage run on a thread pool because they provably write disjoint data; all structural changes are deferred to per-system command buffers drained in `(system_id, seq)` order at end-of-tick; RNG is keyed per-entity (never a shared global stream). The result: threaded per-tick hash is bit-identical to the single-threaded reference, proven by an order-permutation gate.
- **Sources:** [mattneel/gkz](https://github.com/mattneel/gkz)
- **Relevance to LithoLattice:** Deterministic-sim anti-cheat survives threading only with this discipline — disjoint writes, ordered commits, keyed RNG. Direct template for parallelizing signal propagation.

### 5.6 Fixed-timestep accumulator with spiral-of-death clamp
- **What:** Glenn Fiedler's "Fix Your Timestep!" — decouple sim rate from render rate via a time accumulator.
- **How:** Accumulate wall-clock time each frame; while the accumulator ≥ dt, take a fixed-dt step and subtract. Clamp frame time (e.g. ≤0.25 s) so a hitch can't cause a catch-up "spiral of death"; render interpolates between previous and current sim states with alpha = remainder/dt. For a deterministic sim, the critical property: only whole fixed steps are ever taken, so identical inputs → identical ticks regardless of frame rate.
- **Sources:** [Fix Your Timestep! — Gaffer on Games](https://gafferongames.com/post/fix_your_timestep/)
- **Relevance to LithoLattice:** At 512 TPS with uncapped render, the accumulator is mandatory; the anti-cheat determinism requirement means we must treat the clamp as slowdown (drop time = desync) rather than silent catch-up.

### 5.7 ECS archetype storage (Flecs) for entities
- **What:** Flecs — fast, zero-dependency C99/C++17 ECS with cache-friendly archetype/SoA tables; builds in under 5 seconds.
- **How:** Entities sharing the same component set live in one archetype table, each component stored as a dense SoA column — systems scan columns linearly with predictable prefetch. Flecs adds first-class relationships, hierarchies, prefabs, and a phased pipeline with optional multithreading and automatic deferral of structural changes inside system callbacks. Independent benchmarks confirm the archetype tradeoff: 1.4–2.7x faster bulk iteration than sparse-set ECS at 1M–10M entities, slower on structural churn.
- **Sources:** [sandermertens/flecs](https://github.com/sandermertens/flecs) · [odragon_ecs archetype-vs-sparse-set benchmarks](https://github.com/ctzcs/odragon_ecs/blob/HEAD/README.md)
- **Relevance to LithoLattice:** If entities/mobs join the 512-TPS tick, archetype tables make per-tick component scans dense and cache-linear; sparse-set designs win only if we have heavy structural churn, which voxel mobs don't.

### 5.8 Arena/bump allocation with handles instead of pointers
- **What:** Allocate per-tick or per-frame work from a linear arena (bump allocator) and hand out handles (indices) rather than pointers.
- **How:** Andre Weissflog's "Handles are the better pointers" (floooh) argues arena-based structures: one contiguous reservation, bump allocation is O(1) and nearly free, and handles (index + generation) survive arena reuse and defragmentation where raw pointers wouldn't. Per-tick pattern: bump-allocate all transient sim data from an arena, reset the arena at tick end — zero per-tick malloc/free overhead, zero fragmentation, trivial to measure.
- **Sources:** [floooh — Handles are the better pointers](https://floooh.github.io/2018/06/17/handles-vs-pointers.html)
- **Relevance to LithoLattice:** 512 ticks/sec means 512 malloc/free cycles per second per allocator — a per-tick arena reset eliminates that entire cost class for transient signal/tick data.

---

## 6. Networking

### 6.1 Quake III Arena — the classic delta-snapshot model
- **What:** The canonical reference (John Carmack via the bookofhook article): 100% unreliable UDP, one packet type — sequenced game state delta-compressed against the last client-acknowledged state.
- **How:** The server keeps a per-client backlog of sent states back to the client's last ack. Every tick it computes `deltaCompress(client.lastAckState, newState)` and sends it; the client discards out-of-order/old packets, applies the delta, and acks the new sequence. The server **never waits for an ACK** — dropped packets are handled implicitly because the next delta is simply rebased on the newest ack (deltas just grow after loss). "Reliable" data (chat) is brute-forced: re-sent on every state update until an ack ≥ its sequence arrives. Per-packet Huffman compression on top; datagrams capped at ~1400 bytes though typical payloads are a few hundred bytes; server send rate governed by `sv_fps` (20/40 Hz), clients request via `snaps`. The client runs prediction/extrapolation for the local player between snapshots.
- **Sources:** [The Quake3 Networking Model — Carmack via bookofhook, mirrored by Fabien Sanglard](https://www.fabiensanglard.net/quake3/The%20Quake3%20Networking%20Mode.html)
- **Relevance to LithoLattice:** The template for your delta-vs-acked-baseline snapshot system; proves you can delete "reliable packets" entirely. Also the warning: **snapshot send rate must be decoupled from the 512 Hz sim** — Quake sends at 20–40 Hz while sim runs faster.

### 6.2 Glenn Fiedler — "Networked Physics" (Snapshot Interpolation + Snapshot Compression)
- **What:** The deepest public write-up of tick-rate vs send-rate decoupling and bit-level snapshot packing, by the author of yojimbo/netcode.io.
- **How:** Run the sim at a fixed high rate (his example: 60 Hz physics) but send snapshots at an independent lower rate (10–60 Hz). Clients keep an interpolation buffer of 2–3 snapshots and interpolate between them with ~2–3× the send-interval of delay, which absorbs jitter and 1–2 lost packets. Bit packing: quantize positions to fixed point within world bounds (e.g. 18+18+14 bits instead of 96-bit floats), compress quaternions with the "smallest three" trick (29 bits vs 128: drop the largest component, send 2-bit index + 9 bits each of the rest), and use a 1-bit "at rest" flag to skip velocity entirely. Delta encoding: each snapshot is encoded against an acknowledged baseline; the receiver continuously acks its newest snapshot so the baseline converges to ~1 RTT old; a packet flag marks "encoded relative to initial state" before the first ack. He also covers per-snapshot encoding choice (changed-bitmask vs. relative-index list, picked per-packet by mock-encoding both).
- **Sources:** [Snapshot Interpolation](https://gafferongames.com/post/snapshot_interpolation/) · [Snapshot Compression](https://gafferongames.com/post/snapshot_compression/)
- **Relevance to LithoLattice:** Direct blueprint for the 512 TPS problem — **sim at 512 Hz, snapshot at 20–60 Hz, interpolate on clients**; his quantization math (fixed-point positions, smallest-three, at-rest flags) is directly portable to your small-dense-code style.

### 6.3 Minecraft Java Edition protocol (wiki.vg)
- **What:** TCP-based, per-packet framing with VarInt length prefixes and optional per-packet zlib compression negotiated via a threshold.
- **How:** Uncompressed frame = `[VarInt length][VarInt packet ID][data]`. After the server sends `Set Compression` with a non-negative threshold, every packet becomes `[VarInt frame length][VarInt uncompressed length][zlib(packet ID + data)]`; packets below the threshold go raw (uncompressed-length = 0), and a compressed packet whose real length is under the threshold gets you disconnected (anti-bomb check). Vanilla default threshold is 256 bytes; hard max packet 2,097,151 bytes. All multi-byte values are big-endian; block positions pack into one 64-bit int (26-bit X / 12-bit Y / 26-bit Z); entity *relative-move* packets carry position deltas as shorts (1/4096-block fixed point). Chunk data is sent as 16×16×16 sections, each a **paletted container**: bits-per-entry + palette of block-state IDs + tightly packed 64-bit longs (entries never span longs). Per-block edits go out as single/multi block-change packets rather than re-sending sections.
- **Sources:** [wiki.vg Protocol](https://c4k3.github.io/wiki.vg/Protocol.html) · [Chunk Format — paletted containers](https://minecraft.wiki/w/Minecraft_Wiki:Projects/wiki.vg_merge/Chunk_Format)
- **Relevance to LithoLattice:** VarInt framing + zlib-with-threshold is a proven lean wire model; paletted chunk sections are the right template for your chunk-streaming payloads. Note MC does **not** delta-encode at the protocol level — sections re-send whole, which is the cost your delta scheme should beat.

### 6.4 Luanti (Minetest) MTP — custom reliable UDP for a voxel game
- **What:** The closest analog to your needs: a chunked-voxel game with a hand-rolled reliable-UDP transport (MTP) under a u16-opcode command protocol.
- **How:** MTP defines 4 packet types — CONTROL (acks, pings, disconnect), ORIGINAL (unreliable), SPLIT (fragmentation/reassembly), RELIABLE (16-bit sequence numbers per channel) — multiplexed over **3 channels** with resend timeouts scaling 0.1 s → 2 s and a send window of 32–2048 packets; 7-byte base header (protocol id `0x4f457403`, peer id, channel). World transfer is MapBlock-granular (16³ = 4096 nodes): `TOCLIENT_BLOCKDATA` (0x20) sends block position + serialized MapBlock whose node data is zlib/zstd-compressed at serialization time (zstd since map format v29), while live edits stream as `TOCLIENT_ADDNODE`/`REMOVENODE` (0x21/0x22) — i.e. full-block send plus per-node deltas. Entities ("active objects") replicate via `ACTIVE_OBJECT_REMOVE_ADD` + per-object message packets; player positions use f1000 fixed point. Notably, transport-level compression was never implemented — compression happens at the serialization layer. Bulk media (textures/sounds) ships in zstd-compressed "bunches" (max ~16.7 MB per file). Byte order is MSB-first throughout.
- **Sources:** [MTP internals — doxygen for `network/mtp/internal.h`/`impl.cpp`](https://doxy.minetest.net/internal_8h.html) · [networkprotocol.h — all opcodes with wire comments](https://github.com/luanti-org/luanti/blob/master/src/network/networkprotocol.h)
- **Relevance to LithoLattice:** This is your reference design for a custom UDP voxel stack — full-block send + per-node deltas, channel separation, and serialize-time compression instead of transport compression.

### 6.5 Veloren `network` crate — prioritized stream bandwidth budgeting
- **What:** The open-source voxel RPG's modern Rust networking: bincode/serde messages over TCP or QUIC (quinn/rustls over UDP), with an optional lz-fear compression feature.
- **How:** Every message is framed into 1400-byte frames: a DataHeader frame carrying (message id, **stream id**, total length) followed by Data frames. Streams are opened with a priority 0–7 **and a guaranteed bandwidth**. The `PrioManager` runs each tick: every stream first drains its guaranteed bandwidth, then leftover bandwidth is split evenly across streams at each priority level from highest to lowest. Singleplayer is just an embedded server on the same code path. The crate ships benches, tests, and Prometheus metrics.
- **Sources:** [veloren/veloren — `network/` crate](https://github.com/veloren/veloren/tree/master/network) · [message.rs (1400-byte framing, mid/sid)](https://github.com/veloren/veloren/blob/master/network/protocol/src/message.rs) · [prio.rs (bandwidth budgeting)](https://github.com/veloren/veloren/blob/master/network/protocol/src/prio.rs)
- **Relevance to LithoLattice:** The `PrioManager` pattern — guaranteed floor + priority-ordered leftover — is exactly the bandwidth-budgeting model to copy when snapshots, chunk data, and events fight for bytes.

### 6.6 yojimbo (Glenn Fiedler) — channel-separated reliable UDP library
- **What:** A production C++ client/server networking library (≤100 players) built around explicit channel types instead of one-size-fits-all reliability.
- **How:** Secure connect tokens (netcode.io standard), encrypted + signed UDP packets, fragmentation/reassembly. Two channel flavors: **unreliable-unordered** for time-sensitive data (snapshots) and **reliable-ordered** with aggressive resend-until-ack for state that must arrive (can carry data blocks larger than MTU). Includes a bitpacker + serialization system and per-connection estimates of latency, jitter, packet loss, and bandwidth sent/received/acked. Single-threaded by design; client and server must share identical config.
- **Sources:** [mas-bandwidth/yojimbo README](https://github.com/mas-bandwidth/yojimbo/blob/HEAD/README.md)
- **Relevance to LithoLattice:** If you adopt a library instead of hand-rolling UDP, this is the C++ candidate; its channel model (unordered-fast vs ordered-reliable) maps cleanly onto snapshot vs chunk/event traffic.

### 6.7 Valve GameNetworkingSockets — lane-based bandwidth sharing
- **What:** Steam's open-source game transport: connection-oriented, message-oriented (not stream-oriented) UDP.
- **How:** Reliability uses an **ack-vector model** (from DCCP/QUIC): the receiver reports per-packet-number receive status, and the sender deduces exactly which segments to retransmit — smarter than a TCP-style sliding window. Handles fragmentation, reassembly, and retransmission; AES-GCM-256 per packet with Curve25519 key exchange. The standout: **connection lanes** — multiple message streams on one connection share bandwidth via strict priorities, weighted fair queueing, or both (`ConfigureConnectionLanes`). Optional 5 ms Nagle coalescing, rich stats API (ping, loss, queue time, pending reliable/unreliable bytes).
- **Sources:** [ValveSoftware/GameNetworkingSockets README](https://github.com/ValveSoftware/GameNetworkingSockets/blob/HEAD/README.md)
- **Relevance to LithoLattice:** Lanes with priorities/weights are the industrial version of Veloren's PrioManager — the design to study for update prioritization under a bandwidth cap.

### 6.8 Hyperion — 10k-player Minecraft server, protocol layer separated
- **What:** A Rust Minecraft server (andrewgazelka) aiming to break the Guinness record with 10k players PvPing at once.
- **How:** Wire encoding is delegated to `valence-protocol`; game logic runs on the `evenio` ECS (migrated off Bevy for performance), with SIMD-friendly data structures, heavy `#[thread_local]` use to avoid synchronization, and io_uring. Their README's networking math: bandwidth is **O(n·m)** where m = players within visibility radius — keep m constant via map sizing and bandwidth stays O(n). Planned architecture puts encryption/compression on load balancers with a direct link to the game server, keeping the sim core unburdened. Explicitly not full vanilla parity; implements only what the event needs.
- **Sources:** [andrewgazelka/hyperion README](https://github.com/gmh5225/hyperion)
- **Relevance to LithoLattice:** Proves the protocol codec can be cleanly separated from the sim core; the O(n·m) analysis is the right framing for your interest-management design (keep visible-set size m bounded, not player count n).

### 6.9 Valence — Rust Minecraft framework, viewer-gated chunk deltas
- **What:** A modular Rust framework for building Minecraft servers (Bevy ECS, moving to evenio), notable for its protocol-codec design.
- **How:** The `valence_protocol` crate defines `Encode`/`Decode` traits — encode writes to `impl Write`, decode reads from `&mut &[u8]` with lifetimes for zero-copy, plus a `Bounded<T, MAX>` type that caps allocations on decode; framing is kept separate from serialization, and packets are macro-generated from data extracted from the vanilla game. Server ticks at 20 TPS with a 256-byte compression threshold. Its chunk fan-out tracks per-client loaded/unloaded chunks with ready/unready double buffers and sends **viewer-gated deltas** — a single changed block goes as a BlockUpdate packet, multiple as a batched DeltaUpdate — with cached packet buffers for the initial chunk send.
- **Sources:** [valence-rs/valence](https://github.com/valence-rs/valence) · [docs.rs — valence::protocol::packets](https://docs.rs/valence/latest/valence/protocol/packets/index.html)
- **Relevance to LithoLattice:** Viewer-gated deltas + cached init packets are a proven interest-management pattern for voxel chunk streaming; the Encode/Decode + Bounded design translates cleanly to a C++ codec.

### 6.10 DDNet (Teeworlds) — small-dense snapshot code at 50 Hz
- **What:** A 2D twitch game whose networking docs spell out the full delta pipeline in unusually concrete terms — the closest to your "small-and-dense" ethos.
- **How:** Server simulates at 50 ticks/sec and keeps a per-client backlog of snapshots. Each outgoing snapshot is **delta-encoded against the client's last acknowledged snapshot**, then the resulting array of 32-bit ints is packed as UTF-8-like **variable integers** (continuation bit per byte, sign bit in the first byte), then run through a homebrew Huffman coder. Packet/chunk headers are documented down to the bit.
- **Sources:** [ddnet/ddnet doc — "Topic: Compression" (Delta + Variable Integers)](https://github.com/ddnet/ddnet/commit/a3661e492bf56579e0fda3902bbd034f3b978ea8) · [libtw2 protocol docs: packet, huffman, int](https://github.com/heinrich5991/libtw2)
- **Relevance to LithoLattice:** The varint + Huffman + delta-against-acked pipeline is the minimal-byte model to emulate; at 50 Hz sim it also reinforces that send rate should stay far below your 512 Hz sim.

**Video references (verified titles + URLs):**
- **"I Shot You First: Networking the Gameplay of Halo: Reach"** — David Aldridge (Bungie), GDC 2011 — https://www.youtube.com/watch?v=h47zZrqjgLc — replication vs prediction vs authority, prioritization, relevance, update periods, separate state/event/control protocols, bandwidth profiling tooling.
- **"Developer Update | Let's Talk Netcode | Overwatch"** — Tim Ford & Philip Orwig (Blizzard) — https://www.youtube.com/watch?v=vTH2ZPgYujQ — favor-the-shooter, responsiveness, server authority; the canonical tick-vs-send decoupling case: the Overwatch **server ticks at 60 Hz while client updates shipped at ~20.8 Hz** (later a 62.5 Hz high-bandwidth mode).

---

## 7. Storage Deep Dive: Minecraft's Region File System (Anvil .mca) — and what a region layer could add on top of SGNL v2

*Context: LithoKraft already uses a custom bytecode world save (SGNL v2) that beats LevelDB on compression and byte alignment, with dirty-checks determining what saves. JD asked what a region-file-style layer above the chunk system could look like — sector allocation, locality, partial writes, crash safety.*

### 7.1 Region file anatomy (verified byte-level facts)
- **Header: 8 KiB, two 4 KiB tables.** Bytes `0x0000–0x0FFF` hold 1024 chunk location entries (4 bytes each); bytes `0x1000–0x1FFF` hold 1024 last-update timestamps (4-byte big-endian Unix epoch seconds each). Chunk payloads and free space begin at `0x2000`. A chunk with a sector offset of 2 therefore begins immediately after the timestamp table. ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format))
- **Location entry → chunk grid index.** Entry for chunk `(x, z)` sits at byte offset `4 * ((x & 31) + (z & 31) * 32)` within the location table (the `& 31` avoids negative modulo in C-family languages). Each entry is 4 bytes: the **first three bytes are a big-endian sector offset** from the start of the file (in units of 4 KiB sectors), the **fourth byte is the sector count** the chunk's record occupies, rounded up. All-zero entry = chunk absent (never generated). Chunks are capped at 255 sectors = 1,044,480 bytes (~1 MiB). ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format))
- **Per-chunk record layout** (at `offset * 4096`): `[length: u32 big-endian][compression: u8][payload: (length−1) bytes]`. `length` counts the compression byte plus payload bytes, and does **not** include the sector padding — the file as a whole is padded to a 4 KiB multiple, but that padding lives outside the length field. (Corroborated byte-for-byte by independent reimplementations, e.g. [lodestone's Rust crate](https://github.com/matteopolak/lodestone/commit/129f0bbd9aab02fd9820a12e52ebf29750820a31) cited against decompiled `RegionFile.java`.)
- **Compression type byte values (verified current):** 1 = GZip (RFC1952, legacy/McRegion-era, effectively read-only), 2 = Zlib/Deflate (RFC1950 — what the official game writes by default), 3 = uncompressed (added before 1.15.1), **4 = LZ4** (added in snapshot **24w04a**, Java 1.20.5 development cycle, exposed via the dedicated-server property `region-file-compression`: `deflate`/`lz4`/`none`). **127 = reserved for custom compression**, where the id is followed by a namespaced string naming the algorithm (added 24w05a). ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format); [minecraft.wiki/w/24w04a](https://Minecraft.wiki/w/24w04a); [papermc/sectortool README](https://github.com/papermc/sectortool/blob/HEAD/README.md))
- **Oversized-chunk externalization (current vanilla):** if the compression byte has the top bit set (`& 0x80`), the chunk's real data lives in a sibling file `c.<x>.<z>.mcc` in the same directory (absolute chunk coordinates), and the in-region record is a fixed 5-byte stub (`length = 1`, no payload) occupying exactly one sector. The `.mcc` file holds the raw compressed payload with no framing of its own — the file size *is* the length. This handles chunks that would exceed the 255-sector cap. ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format); mechanics confirmed against decompiled sources in [mcadiff issue #6 audit](https://github.com/bangrocket/mcadiff/issues/6))
- **Origin note:** the design is Scaevolus's McRegion mod, adopted nearly unchanged into Beta 1.3 with only a timestamp table added; JahKob claimed ~7× faster than the previous per-chunk-file Alpha format. The on-disk region layout survived unchanged into Anvil (`.mcr` → `.mca`); only chunk *contents* changed. ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format))

### 7.2 Sector allocation & growth handling
- **Free-space tracking is a bitmap rebuilt from the header on every open.** When a region file loads, vanilla derives a free-sector BitSet from the 1024 location entries (sectors 0–1 are always reserved for the header). There is no on-disk free list; it is purely computed state. ([minecraftforum.net thread](https://www.minecraftforum.net/forums/off-topic/computer-science-and-technology/2595358-how-does-minecraft-read-and-write-nbt-so-fast))
- **In-place rewrite vs. relocate.** If a re-saved chunk's compressed size still fits its current sector run, it is overwritten in place. If it grows (or shrinks to a different sector count), the old sectors are freed and a fresh run is found by first-fit scan from sector 2 onward; if no run fits, the chunk is **appended at end-of-file** and the file grows. Files never shrink on their own — repeated grow/shrink cycles fragment the file and orphan old sector runs. (Corroborated by [minecraftforum.net](https://www.minecraftforum.net/forums/minecraft-java-edition/discussion/2820799-strange-chunk-errors-created-throughout-world-mc-1): *"the game does not erase the contents of free sectors after relocating a chunk"*)
- **Defragmentation is an explicit operation.** Snapshot 24w04a added the dedicated-server startup flag `recreateRegionFiles`, which rewrites all region files fresh and defragmented (also the recommended way to recompress after changing `region-file-compression`). ([minecraft.wiki/w/24w04a](https://Minecraft.wiki/w/24w04a))
- **Internal padding waste is real and measured.** Every chunk record is padded to a 4 KiB sector boundary, wasting on average ~2 KiB per chunk — roughly **1 MiB per full 32×32 region**, worst in areas with tiny chunks (The End). ([xymb-endcrystalme/LinearRegionFileFormatTools](https://github.com/xymb-endcrystalme/LinearRegionFileFormatTools) — the "Linear" region format drops sector padding entirely, production-running on a 3 TB world since Nov 2021)

### 7.3 Compression & sparse chunks
- **Absent chunks cost zero bytes beyond the header slot.** Ungenerated or never-written chunks have an all-zero location entry; the region file only grows as chunks are actually written. A partially explored region is far smaller than its 1024-chunk capacity — one reason real region files average ~5 MB for 1024 slots. ([minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format))
- **All-air/generated-but-empty chunks still occupy a record** (they exist in the table, compress well, typically 1 sector). Sparsity in Anvil is at the *chunk* level, not the *block* level — there is no run-length encoding of empty space inside the chunk payload itself (that's the chunk data's job via palettes).
- **Codec choice is per-chunk and server-configurable** (24w04a+): new/updated chunks use the configured codec; old chunks keep their existing codec until rewritten. ([minecraft.wiki/w/24w04a](https://Minecraft.wiki/w/24w04a))

### 7.4 Crash-safety properties (honest)
- **No journaling, no checksums on the container.** The region format has no write-ahead log, no per-record CRC, and no atomic-commit protocol. Integrity is detected only downstream: zlib's Adler-32 catches corruption for type-2 chunks; uncompressed (3) and LZ4 (4) payloads have **no integrity check at all** — a torn write reads back as garbage or crashes NBT parsing (`ZipException: incorrect data check` is the classic symptom). ([minecraftforum.net support thread](https://www.minecraftforum.net/forums/support/java-edition-support/3031125-the-game-crashed-whilst-loading-nbt-data-error))
- **Known torn-write failure modes, all observed in the wild:** location entries pointing at another chunk's data or past end-of-file ("wrong located chunks"), overlapping sector runs, header written without payload and vice versa. Vanilla's own loader recovers what it can; the community's standard response is **Minecraft Region Fixer** (scans for corrupted/wrong-located chunks, deletes or replaces them from backups) and MCEdit's region-repair pass. ([Fenixin/Minecraft-Region-Fixer](https://github.com/Fenixin/Minecraft-Region-Fixer))
- **What servers actually do about it:** regular offline backups are the de-facto durability story ("a copy taken mid-flush can catch a region file half-written" — backup tools explicitly require stopping the server first). `level.dat_old` covers one file only, not regions. ([apoptosis RUNBOOK.md](https://github.com/antifield26/apoptosis/blob/HEAD/docs/operations/RUNBOOK.md))
- **Bottom line: Anvil gives you atomicity at the chunk-record level only in the happy path, and zero durability guarantees beyond "the OS got the bytes."** Any region layer above SGNL v2 must add its own crash story; Anvil is a cautionary example, not a model.

### 7.5 What a region layer could add on top of SGNL v2 (synthesis)
1. **8 KiB header + 1024-entry location table → O(1) chunk lookup without touching payload.** Anvil's fixed header means finding any chunk in a region is one seek + one 4-byte read. For SGNL v2, a per-region index block (chunk id → byte offset/length) would decouple *locating* a chunk from *decoding* it — the dirty-check/save path benefits from the same separation: index first, payload lazily.
2. **4 KiB sector allocation with in-place rewrite → partial-write economy.** Anvil's rule (overwrite in place when the record still fits; relocate only on growth) means small edits don't rewrite the file. Our bytecode save could adopt sector-quantum allocation *without* copying Anvil's 4 KiB granularity — e.g. 512-byte or 1 KiB sectors, tuned to our average chunk record size, to cut the ~2 KiB/chunk average padding waste Anvil demonstrably suffers.
3. **Region grouping (32×32 chunks per file) → locality wins for free.** One file descriptor per 1024 chunks (vs. one per chunk), sequential sector reads for neighboring chunks, and OS page-cache friendliness since spatially adjacent chunks are byte-adjacent on disk. Packing a fixed tile of chunks per region file gets the same read-ahead and handle-count benefits — JahKob's "7× faster than per-chunk files" claim was about exactly this.
4. **Timestamp table → cheap staleness checks.** Anvil's second 4 KiB table stores per-chunk mtimes independently of payload. A region layer could keep this as a compact dirty-generation vector, letting the save scheduler skip unmodified chunks without deserializing headers — a natural complement to our existing dirty-check system (table says *which*, dirty flags say *why*).
5. **Free-sector bitmap rebuilt from header on open → no persistent allocator state.** Anvil stores no free list; the allocator's ground truth is the location table, recomputed at load. That's one fewer structure to keep consistent across crashes — worth copying: derive free space from the index, never persist it separately.
6. **Per-record compression-type byte → codec agility.** Anvil's 1-byte type tag (now with LZ4=4, none=3, custom=127) lets codecs change per chunk and per server config without a format migration. SGNL v2 could carry the same 1-byte codec tag per record, so future codec experiments (e.g. per-record zstd or uncompressed fast-path) don't touch the container.
7. **The .mcc overflow escape hatch → honest handling of the sector-count cap.** Anvil's 1-byte sector count caps inline records at ~1 MiB; instead of failing, current vanilla spills to a sidecar file with a 5-byte stub. If our region records ever have a size cap, a named-sidecar convention beats silent truncation — and the stub pattern keeps the index format fixed-width.
8. **Crash safety: do the opposite of Anvil.** Anvil's real lesson is negative — no journal, no checksums, header updated inline → the entire third-party repair-tool ecosystem exists to clean up after it. A region layer above SGNL v2 should add what Anvil lacks and what fits JD's small-and-dense values: per-record CRC32 (4 bytes/record, negligible), and a write protocol of *payload-first, header-second* with the header update as the single commit point — or a tiny journal of pending index updates replayed on open. That gives atomic chunk replacement, which Anvil never had.

**Storage sources:** [minecraft.fandom.com/wiki/Region_file_format](https://minecraft.fandom.com/wiki/Region_file_format) · [minecraft.wiki/w/24w04a](https://Minecraft.wiki/w/24w04a) · [minecraft.wiki/w/Java_Edition_1.20.5/Development_versions](https://minecraft.wiki/w/Java_Edition_1.20.5/Development_versions) · [lodestone Rust reimplementation (byte-for-byte vs decompiled RegionFile)](https://github.com/matteopolak/lodestone/commit/129f0bbd9aab02fd9820a12e52ebf29750820a31) · [rusty-clanker blueprint incl. & 0x80 external-bit semantics](https://github.com/fynnai/rusty-clanker/blob/HEAD/blueprints/M2/M2-B03-anvil-region-files.md) · [papermc/sectortool](https://github.com/papermc/sectortool) · [LinearRegionFileFormatTools](https://github.com/xymb-endcrystalme/LinearRegionFileFormatTools) · [Minecraft-Region-Fixer](https://github.com/Fenixin/Minecraft-Region-Fixer) · [anvil-parser (PyPI)](https://pypi.org/project/anvil-parser/0.7.0/) · [amulet-core (PyPI)](https://pypi.org/project/amulet-core/1.9.47/) · [majestic53/libanvil](https://github.com/majestic53/libanvil)

---

## 8. Cross-cutting notes for 512 TPS / deterministic sim

These patterns recurred across categories and map directly onto LithoLattice's hardest constraints:

1. **Tick-rate vs send-rate decoupling is non-negotiable.** Quake III sends snapshots at 20–40 Hz while simulating faster; Overwatch shipped 60 Hz sim with ~20.8 Hz client updates; Fiedler's snapshot interpolation is built around the split. At 512 TPS, the network layer must snapshot at 20–60 Hz and let clients interpolate — never ship per-tick state.
2. **Determinism survives threading only with declared conflicts + ordered commits.** mattneel/gkz (conflict-DAG stages, per-system command buffers drained in order, per-entity keyed RNG) is the template; the fixed-step/golden-trace literature (Fiedler, openra2's 600-tick selftest) is the verification story.
3. **Budget everything per tick, deterministically.** Time-sliced A* with front-of-queue deterministic re-enqueue, batched sensory ticking every N ticks, per-tick path budgets, Paper's activation ranges — every expensive system gets a per-tick budget and a fixed ordering so all clients agree.
4. **Amortize per-voice/per-entity work into shared structures.** Flood-fill occlusion from the listener (not per-source raycasts), flow fields (one bake, O(1) per agent), SoA columns (one scan per system) — the pattern is always: compute once, share across consumers.
5. **Small-and-dense beats framework-heavy.** DDNet's varint+Huffman+delta pipeline, cgerikj's bitwise greedy meshing, ZzFX's parameter-array synths, danielwliu07 keeping the mutex pool after benchmarking the lock-free queue — the survey kept confirming JD's instinct: measure, keep the tiny thing that wins.

---

## Appendix: what could not be verified (left out of the report)

- A standalone hierarchical z-buffer (Hi-Z) occlusion-culling implementation specifically for voxel engines — folded into the Nvidium/Voxy entries instead.
- A currently maintained open-source sparse-voxel-octree renderer beyond historical Gigavoxels coverage — excluded.
- Sodium's internal pipeline specifics (5-stage pipeline, arena allocator details) come from secondary reference docs, not Sodium's own documentation — treat as secondary-source.
- A YouTube upload of a GDC "Sound of INSIDE" talk — only the schmid.dk 2016 slide PDF was verifiable; cited that instead.
- miniaudio HRTF claims appeared only in a third-party issue tracker, not miniaudio's own docs — credited with panning/attenuation only.
- Spigot's primary `ActivationRange` source file (repos reorganize frequently) — grounded on community config docs instead.
- A dedicated GDC/Vault talk on distance-based AI tick-rate LOD — principle documented via UE MassLOD and community writeups; no single citable talk found.
- The Eclipse "Flow Field Pathfinding" coding-adventure video — no verifiable URL found; cited a verified Unity DOTS flow-field devlog instead.
- Minecraft's chunk-send ordering (e.g. spiral) and Luanti's exact MapBlock send scheduling — omitted for lack of primary-source verification.
- The exact snapshot that introduced Anvil's external `.mcc` files — documented as "current vanilla" without a version claim.
- PaperMC sectortool's internal "SectorFile" layout — cited only as an existing alternative, not for mechanics.
- qiqian/JPS's 45–54× speedup claim is repo-reported (June 2026, ~8 stars), not independently benchmarked.
- The CppCon 2014 Acton video URL is verified via multiple sources; the video itself was not played.

*Report compiled 2026-10-10 by Elfie (research coordinator) from 7 parallel researcher reports. All entries verified from primary sources at time of writing.*
