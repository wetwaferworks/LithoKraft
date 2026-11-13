# MCP Implementation Report — clean-room Model Context Protocol server for LithoLattice
2026-10-11 · All 5 phases landed, tree green. Research: MCP-Research-Report-2026-10-11.md.

## Executive summary

The engine now ships a clean-room MCP server (`litholattice::mcp`, ~7,300 lines, STL-only, zero new dependencies): JSON-RPC 2.0 core, stdio + Streamable HTTP transports, both protocol eras (2025-11-25 handshake + 2026-07-28 stateless), 11 tools / 6 resources / 3 prompts, bearer-token auth, per-connection kPerm* permission gating with a deny-by-default approval flow, sim-thread-marshaled writes, rate caps, and full audit logging. **Every line was written clean-room** — existing implementations were studied for understanding only; no code was taken. Interop was verified against the real official TypeScript SDK client (v1.32.1, test peer, nothing vendored). `ctest`: 18/20 — the 2 failures are the grandfathered builder-owned ones (engine_sim block-names, lithokraft_selftest SIGABRT); no new failures. `trace_jit_basic.cpp` untouched.

## Phase 0 — JSON-RPC 2.0 core ✅
- `json.h/json.cpp`: `JsonValue` variant (null/bool/int64/double/string/array/**ordered** object), strict recursive-descent parser (fail-fast; rejects truncated input, bad escapes, lone surrogates, bad numbers, `1e400`-style out-of-range), nesting depth cap 64, canonical serializer (integral doubles keep `.0` so reparse is type-stable; out-of-int64 demotes to double, never wraps).
- `jsonrpc.h/jsonrpc.cpp`: request/response/notification framing, `RpcRouter` (method → handler), error codes -32700/-32600/-32601/-32602/-32603 and MCP `-32022`; single requests only in v1 (batches rejected -32600, documented).
- ~95 self-tests. Locked decisions: ordered objects (deterministic canonical form), notification = absent `id` (null id is a request).

## Phase 1 — stdio + handshake era + read surface ✅
- `server.h/server.cpp`: `McpServer` owning tool/resource registries; `ToolDescriptor`/`ResourceDescriptor`; `McpAgent{name, permissionMask}`.
- Handshake: `initialize` → `2025-11-25` + `serverInfo{name:"litholattice", version from net/version.h}` + capabilities; `notifications/initialized` accepted; pre-init anything-else → -32600.
- stdio: newline-delimited JSON-RPC, calling-thread blocking loop, `\r`/blank-line tolerant, 16 MiB line cap, stderr logging, per-tool-call audit (`mcp: tool <name> agent=<name> …`).
- Read tools (9): `sim_status`, `mod_list`, `player_get_state`, `entity_list`/`entity_get` (radius 1–512, fail-closed), `world_get_block`, `world_query_blocks` (via `console::IWorldQuery`; mandatory bounds, ≤kMaxQueryCells re-checked server-side), `fragment_search` (via `index::TextIndexer`, injectable corpus), `defrag_status`.
- Resources (6): `lithoscript://commands`, `lithoscript://cvars`, `world://dimensions`, `mods://list`, `game://lore-glossary` (runtime-loaded by the game, never embedded — avoids drift), `net://version`.
- Permission mapping locked: world/sim reads → `kPermWorldRead`; console surface → `kPermConsole`; mod_list/defrag_status/lore-glossary/net-version → open (mask 0); `fragment_search` → `kPermWorldRead` (dedicated bit possible later). Denials → `isError` naming the missing bit, never protocol errors.
- Layering: server never touches engine classes directly — 11 injectable seam interfaces; null seam → `isError` ("not wired"), never a crash. Zero link dependencies added to `litholattice_mcp`.
- `--mcp-stdio`: engine entry point + exact game-side recipe documented in `engine/docs/MCP-GAME-SEAM.md` (flag is 1 line; adapters are the real work — game code, builder's).

## Phase 2 — Streamable HTTP + auth ✅
- `sockets.h/sockets.cpp`: minimal TCP server (`TcpListener` + RAII `TcpConn`, MSG_NOSIGNAL, I/O timeouts). POSIX implemented and tested; `_WIN32` branch is Winsock-shaped but **unverified** — flagged for the Windows builder.
- `http.h/http.cpp`: strict HTTP/1.1 parser (request line + headers; rejects obs-fold, duplicate Content-Length, bad tokens; 64 KiB header cap). Status discipline: 404 unknown path, 405 non-POST, 411 missing Content-Length, 413 body >16 MiB, 415 wrong type, 501 chunked, 431 oversize headers, 401 bad/missing bearer (+`WWW-Authenticate`), 202 for notifications.
- `http_server.h/http_server.cpp`: `McpHttpServer` — one accept/dispatch thread, sequential keep-alive (256 req/conn cap, 5 s I/O timeouts), bearer-token auth (256-bit token from OS CSPRNG: `getrandom(2)` → `/dev/urandom`; net's `makeNonce` checked and deliberately NOT reused — it's a deterministic packet-nonce builder), session lifecycle via `Mcp-Session-Id` (missing → 400, unknown → 404), `rotateToken()`, per-session grant/revoke.
- Binds `127.0.0.1` only by default; non-loopback bind logs a LOUD warning; port taken → fail-fast (never re-picks). Default `kMcpDefaultPort = 1337` (JD-locked 2026-10-11).
- SSE framing implemented + tested; live server→client push is a later phase (no notifications exist yet).
- Bugs found and fixed in-build: parser dropped the request line's CRLF; a `rotateToken()` self-deadlock (mutex ordering) that hung the suite.

## Phase 3 — stateless era + prompts ✅
- Dual-era `handleMessage`: `params._meta` → stateless path (skip init gate, apply envelope); absent → legacy lifecycle byte-identical. `server/discover` answers pre-handshake (era-agnostic); bad version → `-32022` naming both supported versions.
- `_meta` key: `io.modelcontextprotocol/protocolVersion` wins, plain `protocolVersion` also accepted (documented leniency).
- Stateless envelope: `resultType:"complete"` + `_meta` with version + `serverInfo`; list/discover TTL 1 h public, `resources/read` TTL 0 private. Left out (documented): MRTR/`input_required`, `completions`, per-request `logLevel`.
- Unknown prompt → `-32602` (the prompt result type has no `isError` channel — verified against spec sources); unknown tools stay `isError` per Phase 1.
- Prompts are mask-0 (open static templates). Stateless HTTP shares one agent identity (documented tradeoff; finer isolation → use the handshake era). SEP-2243 header validation → `-32020` on mismatch.
- 3 prompts, grounded in the actual built names, honest about v1 limits: `agent_onboarding`, `build_contraption` (notes placement is Phase-4-gated), `diagnose`.

## Phase 4 — gated write tools + conformance ✅
- `world_set_block` (x/y/z, block 0–65535, timeout_ms default 2000): requires `kPermWorldWrite`, deny-by-default, malformed → `-32602`.
- `lithoscript_exec` (line ≤64 KiB): requires `kPermConsole`; output captured into the result. **Runs at the agent's permission mask, never the player's** — the console had no caller-permission context, so the minimal seam was added (`setCallerPermissions(mask)` / `kCallerFullAuthority` sentinel; default = legacy behavior exactly; installing any restricted mask revokes cheats fail-closed; mask stamped onto `wait`-suspended scripts and restored on resume).
- Determinism guard: new `IWorldWriter` seam — the game adapts it via `SimThread::edit()` drained at the same deterministic point as player edits; per-call completion the MCP thread blocks on; timeout → `isError`, never a hang.
- Rate limiting (`rate_limit.h`, `McpTokenBucket`, fakeable clock): 256 tool calls/sec burst 512 per connection; 32 block writes/sec burst 64. (Survey note: no reusable kPermWorldWrite limiter class existed — only the doc comment — so MCP ships its own following `net/confinement.h`'s pattern.) Exceeded → `isError` naming the limit.
- Audit: `mcp: write world_set_block agent=X (1,2,3)=5 -> ok`, `mcp: exec lithoscript_exec agent=X … -> ok`, plus `-> rate_limited` / `-> not applied:` variants.
- Left out as v2 (per plan): `sim_pause`/`sim_resume`, `angelscript_eval`.

## Conformance — real TypeScript SDK client (v1.32.1, test peer, nothing vendored, probe in /tmp only)
- **stdio**: initialize (negotiated 2025-11-25) → `tools/list` (11 tools) → `tools/call sim_status` (real JSON) → `tools/call world_set_block` ungranted (**isError naming `world_write`**) → `prompts/list` (3) → clean close. Annotations verified on the wire.
- **Streamable HTTP** (127.0.0.1:18421, bearer): same flow green; the handshake session got its own mask-0 agent and `sim_status` was **denied live** ("missing world_read") — per-session deny-by-default confirmed against a real client.
- `resources/read` on the unwired console seam returned the designed `-32603` (probe wired only `sim`) — expected, not a regression.
- Live interop against a modern stateless-era client (Llama Stack @ 2026-07-28) has NOT been run — self-tests only. Recommended follow-up.

## Test results
- `ctest -R engine_mcp`: **PASS** (~320 checks across the 5 phases, 0 failures), incl. live-loopback socket tests.
- Full `ctest`: **18/20** — failures are exactly the 2 grandfathered builder-owned ones (`engine_sim` block-names, `lithokraft_selftest` SIGABRT). No new failures.
- console module: 8 new checks pinning the caller-mask seam (legacy default, cheat revocation under restricted mask for commands AND cvars, mask survives `wait` suspension).

## The surface as built
**Tools (11):** `sim_status`, `mod_list`, `player_get_state`, `entity_list`, `entity_get`, `world_get_block`, `world_query_blocks`, `fragment_search`, `defrag_status`, `world_set_block` (gated), `lithoscript_exec` (gated, agent perms).
**Resources (6):** `lithoscript://commands`, `lithoscript://cvars`, `world://dimensions`, `mods://list`, `game://lore-glossary`, `net://version`.
**Prompts (3):** `agent_onboarding`, `build_contraption`, `diagnose`.
**Transports:** stdio (newline-delimited JSON-RPC) + Streamable HTTP (`POST /mcp`, 127.0.0.1:**1337** — JD-locked 2026-10-11, leet, naturally). **Eras:** 2025-11-25 handshake + 2026-07-28 stateless. **No legacy SSE** (deprecated, per JD).

## Game-side seam (builder's work — engine/docs/MCP-GAME-SEAM.md)
Implement the 11 adapters (~20 lines each), add the `--mcp-stdio` flag + headless boot, author the lore-glossary asset, wire the `IWorldWriter` adapter (`SimThread::edit` recipe §9b) and the `IConsoleExec` adapter (console-mutex + `setCallerPermissions` recipe §9c), decide the `mcp grant …` console command names (provisional shapes documented).

## Open questions for JD
1. Default port — LOCKED at 1337 by JD (2026-10-11). Done.
2. Rate numbers: 32 block-writes/sec (burst 64), 256 tool-calls/sec (burst 512) — sane or tune?
3. `lithoscript_exec` 64 KiB line cap; sim-write timeout 2000 ms default — OK?
4. Agent-approval UX: the console `mcp grant …` flow is provisional — how should the in-game approval dialog look?
5. Session expiry: sessions currently live until server stop — idle timeout wanted?
6. The Daemon question (still open): should the frontier-powered Daemon consume this same MCP surface? Research recommends yes — one on-ramp for all agents, including the boss.
7. Remote exposure: tunnel recommendation vs documented-only (localhost-only is the default; no TLS in v1 by design).

## Clean-room attestation
Every line in `engine/src/mcp/` was written fresh for this task. Community C++ implementations and the spec were studied for understanding only. The only external peer ever executed was the official TypeScript SDK client as a conformance probe (never vendored, never linked).

## Phase 5 — player control (pilot the player through the real input path) ✅
- New tools (5): `player_move` (sticky forward/strafe -1..1 + jump/sneak/sprint), `player_look` (relative yaw/pitch deltas), `player_action` (use / attack-break / place / drop, one frame each), `player_hotbar` (select slot 0..7), `player_release` (safety: release all held buttons). Plus the read tool `player_get_state` was extended (hotbar + selectedSlot).
- **Real control path, no cheating the sim**: `IPlayerController` (server.h) — the game adapts it against its `InputSystem` (`McpPlayerController` in game/LithoKraft, `mcp_player_control.cpp`): each submission becomes synthetic `InputEvent`s (KeyDown/KeyUp via the CURRENT keybind map, mouse-motion for look) merged at `InputSystem::update()` — the exact point keyboard input enters. Movement flows through `Player::update` (speed caps, no-faster-diagonals); break/place/use flow through the game's normal action handling into `SimThread::edit` at tick boundaries (reach checks, interactable checks, creative rules). The model plays by the same rules, same physics, same tick alignment.
- **Multi-pilot from day one** (JD-locked): every control tool takes a `pilot` id; `IPlayerController::pilotIds()` declares the set; per-pilot shadow input state (held-button diffing), per-pilot rate buckets, per-pilot audit logging. v1 wires exactly one pilot (`"player"` = the local player). Adding pilot #2 is configuration, never surgery.
- **NPC future** (not built): a companion NPC = a pilot slot driven by game AI instead of an MCP client — same addressing, same input path, no MCP permission involved.
- Permission: `kPermPlayerControl = 1<<10` (deny-by-default). Rate limit: 120/sec, burst 240 per pilot (the frame pump is the natural limiter — one staged batch per frame — the bucket stops wire-speed press/release vibration).
- Anti-cheat note (in the seam docs): a model-driven player is still a player — this grants NO new capabilities. What it does NOT stop: the model playing 24/7 with inhuman reaction time and perfect recall. That is the point (agent playground), not a bug. Server-authoritative multiplayer is out of scope for v1 (localhost-only default; a remote server would never grant this bit to an untrusted peer).
- Game adapter holds actions as sticky shadow state; `submitFrame` diffs → edge events; `releaseAll` on release/disconnect. Unknown actions (unbound keys) counted in `droppedUnbound()`, never crash.

## Phase 6 — pilot senses: agent vision + agent audio (the see→think→act loop) ✅ engine / 🔧 game adapter in progress
- New tools (4): `vision.capture_frame` (PNG from the pilot's camera), `audio.capture_chunk` (1024-frame mono PCM of what the pilot hears), `vision.set_camera` (free camera at an arbitrary pose), `vision.track_player` (back to player-following). Together with `query` (world state) and the `player_*` tools, the full agent loop is closed: **query (world) + vision (sight) + audio (hearing) + control (action)**.
- **Permission: NEW bit `kPermPilotSenses = 1<<11`** (not player_control). Rationale: observation vs actuation is the security boundary — a monitoring-only agent sees/hears without moving a muscle, and vice versa. Deny-by-default; denials name the `pilot_senses` bit.
- **Vision — downgraded by design**: software renderer, flat shading (lowest tier), default 160x90 (cvars `mcp.vision_width`/`mcp.vision_height`). PNG encoding (clean-room, ~120 lines, stored deflate blocks): JPEG would need a full DCT encoder or a vendored lib (breaks STL-only); at 160x90 the raw frame is 43 KiB and PNG overhead is small. Deterministic (same pixels → byte-identical output; viewpoint tests rely on it).
- **Vision architecture decision** (JD's perf question, answered with measurements):
  - Measured on this VM: 160x90 flat = **2.26 ms/frame**, 320x180 = **2.34 ms/frame** (16k quads, 8 sub-chunks) — vertex-bound, not fill-bound. At the 4 fps vision cap that's <1% of one core. NOT heavy.
  - Direct answer to JD: **no, dual render was NOT accounted for in the pipeline** — the game owns exactly one `IRenderer&`, and `SoftwareRenderer` isn't even an `IRenderer` (standalone CPU rasterizer). But nothing prevents it: software rendering is pure CPU (no GL context, no shared state); the game copies chunk meshes into a vision mesh cache at `uploadMeshes()` time (classic `ChunkMesh` data is always generated, regardless of the selected mesher).
  - Options: (a) dual render — chosen as the GENERAL path (works for arbitrary cameras); (b) screen downscale — only valid when pilot camera == player camera, plus GPU→CPU readback stalls; (c) low-res pass on the active renderer — needs IRenderer multi-camera surgery, and Vulkan isn't written yet. **Recommendation: hybrid — (a) now (independent cameras are the primary case per JD), (b) later as an optimization when a pilot's camera happens to match the presented view.**
- **Independent cameras** (JD-locked): each pilot gets its own camera — position + yaw/pitch + fov, placeable anywhere (agent's location, companion's, overwatch). `vision.set_camera` moves it; captures report the ACTUAL camera used. This is the first instantiation of the queued engine camera-system concept.
- **Chunk loading** (JD-locked 2026-10-11): cameras CAN load chunks, but LIMITED radius only — never the player's view/sim distances. `camera.chunk_radius` cvar (default 4, tunable, scales with hardware per the scalable-fidelity rule). A camera over unloaded territory renders sky, not a crash. (v1 world is fixed-size; the cvar is the locked budget for the streaming hook.)
- **Audio — lowest tier first**: `AudioEngine` gained a lock-free tap (`setTapEnabled`/`drainTap`, 2 s ring, SPSC, real-time safe, overflow drops oldest). The tap copies the final mono mix post-shed, pre-master-gain. **Positional by construction**: the engine's mix is already the listener's mix (the game computes per-voice distance gain from the listener before posting) — the agent hears the pilot's ears, with the 3D spatial mix baked in. There is no global mix to tap. (v1 caveat: the engine mixes for one listener; per-pilot mixes arrive with pilot #2.)
- Audio tiers: `mcp.audio_tier` 0..3 → 8/16/24/48 kHz mono s16le (default 0 = cheapest, ~16 KB/s; the cvar ramps it with no code change). Chunk = 1024 frames at the tier rate. Tier changes the actual output (tested).
- Rate limits: vision 4/sec burst 8 per pilot; audio 8/sec burst 16 per pilot; camera moves share the vision bucket. Per-pilot buckets, per-pilot audit lines.
- Engine tests: base64 vectors, PNG structure/IHDR/IEND/determinism/viewpoint-difference, permission bit + name, tier ladder, deny-by-default naming `pilot_senses`, not-wired, unknown-pilot rejection, burst-then-limited for both senses, end-to-end PNG validity through the tool path (test-only decoder checks the signature + IHDR of the returned bytes), camera set/track/deny. Audio: tap arm/drain/silence-as-zeros, nonzero note capture, tap under full shed (degradation ladder intact), disarm, overflow-bounded. `ctest`: 18/20 — only the 2 grandfathered builder failures.
- **Game adapter** (in progress, child agent): `McpSensesAdapter` in game/LithoKraft implementing both seams — vision mesh cache + owned `SoftwareRenderer` + per-pilot cameras + audio drain/downsample; cvars `mcp.vision_width/height`, `mcp.audio_tier`, `camera.chunk_radius`; `mcpSenses()` accessor on `Game` for the builder's `--mcp-stdio` host wiring (same pattern as `mcpPlayerController()`).

## Updated surface
**Tools (20):** the 11 Phase 1–4 tools + `player_move`, `player_look`, `player_action`, `player_hotbar`, `player_release` (kPermPlayerControl) + `vision.capture_frame`, `audio.capture_chunk`, `vision.set_camera`, `vision.track_player` (kPermPilotSenses).
**Sense inventory:** `query` (world state) + `vision.capture_frame` (sight) + `audio.capture_chunk` (hearing) + `player_*` (action).

## Open questions for JD (senses)
1. Vision rate 4/sec (burst 8): enough for the see→think→act loop, or tune?
2. Audio tier default 0 (8 kHz): stay cheapest, or default higher for smarter models?
3. `vision.set_camera` bounds: currently any world coords — clamp to loaded world, or leave free (sky beyond)?
4. The Daemon question extends here: Daemon consuming vision/audio through this surface — yes?

## Addendum — game adapter landed (2026-10-11 ~12:40 EDT) ✅
- **McpSensesAdapter LANDED** in `game/LithoKraft/src/game/mcp_senses_adapter.{h,cpp}` — implements `IVisionCapture` + `IAudioCapture`. `Game::mcpSenses()` accessor added (same pattern as `mcpPlayerController()`); cvars `mcp.vision_width/height`, `mcp.audio_tier`, `camera.chunk_radius` registered in `settings_cvars.cpp`; `noteMesh()`/`clearMeshes()` wired into `uploadMeshes()`/`switchDimension()`; tap armed at construction, disarmed at destruction.
- Also found-and-fixed: **`mcp_player_control.cpp` was never added to any CMakeLists** (game built against a missing TU — pre-existing gap, now linked).
- Tracking math: eye = player pos + 1.62, rad→deg yaw/pitch; per-mesh MVP bakes 16³ sub-chunk origins to match the GL backend's uOffset_; frustum-culls via `aabbInFrustum`.
- **⚠️ Engine bug surfaced by the adapter's tests (not fixed — clean-room rule; needs JD/builder decision): the software renderer's backface culling is inverted relative to the GL renderer.** Evidence: `gl33_renderer.cpp` sets `glFrontFace(GL_CCW)`; the mesher winds quads "CCW seen from outside"; a mesher-wound top-face quad viewed from above renders as SKY through `SoftwareRenderer::renderMesh` (culled) but renders correctly with reversed winding. Hypothesis: `isBackface` (`software_renderer.cpp`) computes the screen-space cross in y-down framebuffer coords but applies the y-up NDC sign convention. No engine test covers mesher→software-renderer winding. **Workaround in the adapter only**: `noteMesh()` stores each quad with v1↔v3 swapped, so agent vision matches the GL view. **If `isBackface` is ever fixed to match GL, the reversal MUST be removed** (documented loudly in `mcp_senses_adapter.h/.cpp`) — otherwise it double-inverts and hides every front face. Candidate for the fix run: engine-only, no MCP surface impact.
- **Stubbed for the builder**: `McpBackends` wiring in the `--mcp-stdio` host (wrap `Game::mcpSenses()` into `{.vision, .audio}` — one object serves both seams); `mcp.audio_tier` cvar is synced into the adapter per frame (tool handler reads the cvar or takes the tier param — no code change needed later); `camera.chunk_radius` streaming hook documented, not built (v1 world is fixed-size).
- Verification: standalone adapter test 41/41 (pilot validation on all five methods; empty-cache PNG valid — signature/IHDR/IDAT parsed by a test decoder; center pixel = sky on empty, flat gray on a mesher-wound floor quad; byte-deterministic captures; 16..512 size clamp; camera validation + trackPlayer round-trip; audio false-on-empty-tap → true with nonzero int16 energy after a posted 440 Hz tone; tier ladder 8000/48000). Full engine build clean; `ctest` 18/20 (only the 2 grandfathered builder failures; `LastTest.log` contains no adapter references).
- Environment note: the VM had lost several X11 dev headers; restored from locally cached `.deb`s (no source impact).
