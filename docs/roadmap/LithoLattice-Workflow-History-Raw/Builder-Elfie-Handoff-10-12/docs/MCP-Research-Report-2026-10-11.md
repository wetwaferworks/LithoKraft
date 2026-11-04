# MCP Research Report — Model Context Protocol for LithoLattice
2026-10-11 · READ-ONLY scout for a clean-room implementation. No code written; existing implementations studied for understanding only.

## Executive summary

MCP (Model Context Protocol, created by Anthropic, now an open standard) is the right on-ramp: it is a JSON-RPC 2.0 protocol where a **server** (our engine) exposes **Tools** (actions), **Resources** (read-only data), and **Prompts** (reusable templates) to any **client** (Grok, Meta/Llama Stack, llama.cpp, Claude, Cursor, …). The ecosystem has converged on it — "USB-C for AI" — and every model family JD named already consumes it.

Recommendation: build a clean-room MCP server into the engine with **Streamable HTTP on localhost as the primary transport** and **stdio as the secondary** for local agents, supporting **both protocol eras** (2025-11-25 handshake + 2026-07-28 stateless). The engine has no JSON library and no socket code today, so both are clean-room work — but both are small, well-understood pieces. There is **no official C++ SDK** (Tier-1 is TypeScript/Python/Go/C#), which is exactly why clean-room is the right call rather than adopting a community one.

## 1. Protocol overview

**Roles.** Host (the AI app) → Client (protocol peer) → Server (us). Servers expose Tools/Resources/Prompts; clients may offer sampling/roots/elicitation back (we don't need those initially).

**Wire format.** JSON-RPC 2.0, method names like `tools/list`, `tools/call`, `resources/list`, `resources/read`, `prompts/list`, `prompts/get`. Tool results carry `content[]` (text/image/audio/resource_link/embedded resource) plus `isError`; a model-actionable failure is `isError: true` in the result, not a protocol error. Tools may also return `structuredContent` against an `outputSchema`.

**Two lifecycle eras** (this is the single most important compatibility fact):
- **Handshake era, `2025-11-25` and earlier** (still the widely-deployed baseline): client sends `initialize` (declares desired protocol version + capabilities), server answers with the version the connection will use, client sends `notifications/initialized`. Sessions tracked via `Mcp-Session-Id` header on HTTP.
- **Stateless era, `2026-07-28`** (current): **no `initialize` handshake**. Every request carries `_meta` with protocol version + client capabilities; every response carries server identity. Discovery via `server/discover`. Unsupported version → error `-32022`. Roots, sampling, and logging are deprecated; legacy HTTP+SSE transport is deprecated.

A modern server probes/accepts both: answer `server/discover` for modern clients, `initialize` for legacy ones.

**Capabilities.** Servers declare what they support (`tools`, `resources`, `prompts`, each with `listChanged` for change notifications).

**Spec sources.** The canonical spec lives at `modelcontextprotocol/specification` on GitHub (also published at modelcontextprotocol.io/specification, versioned per revision: `/2025-11-25/...`, `/2026-07-28/...`). The 2025-11-25 JSON schema (`schema/2025-11-25/schema.ts` in the `modelcontextprotocol` repo) is the field-level source of truth for a clean-room implementation.

## 2. Transport analysis + recommendation

| Transport | Status | Fits a running game? |
|---|---|---|
| **Streamable HTTP** (`POST /mcp`, SSE streams for server→client) | Current standard, all major clients | **Yes — primary.** Attaches to an already-running game; works over localhost and LAN; what Grok/xAI/Meta all speak remotely. |
| **stdio** (newline-delimited JSON-RPC on stdin/stdout) | Current, universal for local | **Yes — secondary.** Zero networking; ideal for local agents (llama.cpp, Grok Build CLI). Needs a dedicated mode: the game's own stdin/stdout is the game console, so stdio serving wants a `--mcp-stdio` headless flag or a companion process, not the main game loop's pipes. |
| Legacy HTTP+SSE | Deprecated in 2026-07-28 | No — don't build it. (xAI's API still accepts SSE, but Streamable HTTP covers that path.) |
| WebSocket | Community extension, not core spec | No. |

**Recommendation: Streamable HTTP on `127.0.0.1` (configurable port; provisional default `18420`) as the primary transport, stdio as the secondary.** Rationale:
- An agent connects to a *running* game — stdio can't do that (it spawns a subprocess); HTTP can.
- Every remote consumer JD named (Grok BYO, xAI API, Llama Stack) speaks Streamable HTTP. Only xAI's API also accepts SSE, and Streamable HTTP satisfies it.
- Localhost-by-default keeps the security posture sane (see §6).
- Dual-era support (initialize + server/discover) maximizes client compatibility for ~10% extra code.

**What we must build clean-room:** POSIX sockets (+ Winsock path for the Windows builder) and a minimal HTTP/1.1 request parser with SSE response streaming. The engine has no socket code today (the net module is a packet/channel abstraction, not real sockets) — this is new but small and well-understood. No JSON library is vendored today (third-party list: GLFW, Vulkan-Headers, glslang, AngelScript, miniaudio, stb) — a minimal JSON value model + JSON-RPC framing is clean-room feasible and avoids new dependencies.

## 3. C++ implementation landscape (studied, not used)

No official C++ SDK exists — SDK tiering covers TypeScript, Python, Go, C#. Community options surveyed for architecture understanding only:

- **yurirocha15/mcp-cpp-sdk** (C++20, Apache-2.0): coroutines via Boost.Asio, nlohmann/json, transports stdio/WebSocket/Streamable HTTP, claims 2025-11-25. Heaviest dependency footprint.
- **jesspig/modelcontextprotocol-cpp-sdk** (C++17, MIT): libhv + simdjson auto-fetched, all four transports, static libs. (Its README references `modelcontextprotocol/cpp-sdk` upstream — possibly donated; unconfirmed. Treat as community regardless.)
- **neumann-labs/mcp-cpp** (C++20, Apache-2.0, beta): spec-faithful to the 2025-11-25 `schema.ts` verbatim, idiomatic RAII, sanitizer-clean goals. Closest in spirit to what we'd write.
- **hkr04/cpp-mcp**: small, stdio + SSE only.
- **IBM TSAR-MCP**: zero-dependency C/C++ server SDK, edge-focused.

**Why clean-room wins here:** no official SDK to standardize on; community ones vary in completeness, pull networking/JSON dependencies the engine doesn't want, and none is C++17 (our standard — C++20 coroutines are off the table). The protocol core is ~15 RPC methods; a thin, engine-idiomatic implementation (litholattice:: namespace, k-prefix constants, fail-fast error policy) will be smaller and safer than adapting one. Interop-test against the official TypeScript SDK's client instead of depending on any C++ one.

## 4. Per-model-family compatibility

**Grok / xAI — three consumption paths, all viable:**
- **Grok Build CLI** (local-first, what JD uses): MCP servers via stdio (`command`/`args` in config) or Streamable HTTP (`grok mcp add --transport http`), with per-tool permission rules (`deny > ask > allow`). Our stdio mode fits perfectly here.
- **grok.com "Bring Your Own MCP"**: custom servers over **Streamable HTTP**, must be reachable over the public internet (localhost needs a tunnel). Tool discovery is automatic.
- **xAI API** (for the future frontier-powered Daemon): remote MCP tools via `tools: [{type: "mcp", server_url, server_label, allowed_tools, authorization}]`. **Only Streamable HTTP and SSE transports are supported** — another vote for Streamable HTTP as primary. `server_label` prefixes tool names; `allowed_tools` allowlists.

**Meta:**
- **Llama Stack**: first-class MCP tool provider (`provider_id: model-context-protocol`), configured with `mcp_endpoint.uri` + `protocol_version` + Bearer headers. Wild configs show `2026-07-28` — **Llama Stack is the reason we must support the stateless era**, with per-request `_meta` and `server/discover`.
- **llama.cpp** (dominant local inference): native MCP client in llama-server since March 2026 (PR #18655, actively extended). Local models connect via **stdio** (`--mcp-server`). Practical note: smaller local models handle tool-calling worse with huge schemas — keep our tool descriptions tight and schemas minimal.

**Frontier general (Claude, Cursor, ChatGPT, Gemini):** Streamable HTTP + the 2025-11-25 handshake era is the safe baseline; dual-era support covers everyone.

**Compatibility checklist for our server:** Streamable HTTP ✓, stdio ✓, both lifecycle eras ✓, `tools/list`+`tools/call`+`resources/*`+`prompts/*` ✓, Bearer auth header ✓, tight tool schemas ✓, `isError` (not protocol errors) for action failures ✓.

## 5. Proposed MCP surface for LithoLattice

Mapped to real engine subsystems (headers verified in-tree). Naming is provisional, dotted/lowercase.

**Tools** (model-controlled actions):
| Tool | Engine mapping | Notes |
|---|---|---|
| `lithoscript_exec` | `console/concmd.h` `ConsoleRegistry::executeLine` | The universal escape hatch: runs one lithoscript line, returns console output. Inherits all console hardening (budgets, permission gating). |
| `world_query_blocks` | `world/chunk_map.h`, `world/block.h` (+ the new `query` command backend) | Region-bounded, mandatory bounds, capped results, deterministic order. |
| `world_get_block` | `world/block.h` | Single-block read. |
| `world_set_block` | `world/block.h` | Write; requires world-write permission, rate-limited like `kPermWorldWrite`, approval-gated. |
| `world_raycast` | `world/raycast.h` | "What is at these coordinates / along this ray." |
| `entity_list` / `entity_get` | `entity/entity.h` | Radius-bounded. |
| `player_get_state` | `player/player_state.h`, `player/stat.h` | Position, stats (Kharge lives here in-game). |
| `fragment_search` | `index/text_indexer.h` | Search the player's Fragment — the agent reads the journal. |
| `sim_status` | `sim/sim_thread.h`, `sim/clock_domain.h` | Tick, TPS, paused. Read-only. |
| `mod_list` | `mods/mod_loader.h` | Installed mods + permission masks. |
| `defrag_status` | (new defrag service) | Forever-world maintenance visibility. |
| *(v2, gated)* `sim_pause`/`sim_resume`, `angelscript_eval` | `sim/sim_thread.h`, `script/script_engine.h` | High-privilege; not v1. |

**Resources** (read-only data, zero tool-call latency):
- `lithoscript://commands`, `lithoscript://cvars` — the live console surface.
- `world://dimensions` — dimension list, bounds, current dimension.
- `mods://list` — mods, versions, permissions, extension trees (`mods/extension_registry.h`).
- `game://lore-glossary` — the LithoKraft lore glossary, so agents learn the world (Kharge, the Fragment, the Daemon…).
- `net://version` — engine/SDK/protocol versions (`net/version.h`, `net/handshake.h`).

**Prompts** (reusable templates):
- `agent_onboarding` — "You are an agent inside LithoKraft. Here's how you see (query), act (tools), and build (lithoscript + lithography loop)."
- `build_contraption` — guided workflow: survey area → plan → place blocks → verify with query.
- `diagnose` — pull `sim_status` + recent engine log, summarize world health.

**Design note:** `lithoscript_exec` + tight purpose-built tools is the right split — the escape hatch covers everything, while first-class tools give models reliable, schema-validated paths for the 90% actions (see: the "tool overloading" failure mode — static data belongs in Resources, not Tools).

## 6. Security considerations

An MCP server is a **remote-control surface for the game**. Treat it like one:

- **Auth:** Bearer token on the HTTP transport (Authorization header, the xAI pattern). Token generated per session/run, shown once in the debug console; `defrag`-style, no default passwords, no token = no connection.
- **Binding:** `127.0.0.1` only by default. LAN/internet exposure is explicit opt-in with a warning; remote exposure wants TLS or a tunnel (document, don't roll our own TLS in v1).
- **Agent identity + permission gating:** every MCP connection gets an agent identity with a permission mask drawn from the existing `kPerm*` model (`mod_interface.h`: `kPermWorldRead`, `kPermWorldWrite`, `kPermConsole`, …). Each tool declares its required bits; denied calls return `isError` results, never protocol errors. The player grants the mask via an approval flow modeled on the mod permission manifest (deny-by-default).
- **`lithoscript_exec` scoping:** the console already rules that "agent scripts execute with the AGENT's permissions, never the player's" (`concmd.h`) — the MCP server must enforce exactly this: exec runs at the connection's permission mask.
- **Rate limiting:** reuse the `kPermWorldWrite` rate limiter for mutating tools; add a per-connection tool-call rate cap.
- **Determinism guard:** world-mutating calls marshal onto the sim thread (never mutate from the HTTP thread); the deterministic sim stays bit-identical.
- **Audit:** every tool call logged to the engine log (visible in the debug console, `modsys:`-style prefix).
- **Human in the loop:** the spec says clients SHOULD confirm sensitive operations — our server flags mutating tools with annotations so hosts can prompt, and the in-game approval dialog is the backstop.

## 7. Clean-room implementation plan

- **Phase 0 — JSON-RPC core.** Clean-room JSON value model + JSON-RPC 2.0 framing (~15 methods), request routing, error codes. Self-tests; interop-test against the official TypeScript SDK client (as a *test peer*, not a dependency).
- **Phase 1 — stdio + handshake era + read surface.** stdio transport behind a `--mcp-stdio` flag; `initialize`/`notifications/initialized`; `tools/list`, `tools/call`, `resources/list`, `resources/read`; the read-only tools and resources from §5; agent identity + `kPerm*` gating.
- **Phase 2 — Streamable HTTP.** Clean-room sockets (POSIX + Winsock) + minimal HTTP/1.1 + SSE streaming; `POST /mcp`; Bearer auth; localhost default; session management for the handshake era.
- **Phase 3 — Stateless era + prompts.** `server/discover`, per-request `_meta`, `resultType`, cacheable lists (`ttlMs`/`cacheScope`); `prompts/list`, `prompts/get`.
- **Phase 4 — Write tools + hardening.** Permission-gated `world_set_block` et al, approval flow, rate limits, audit log; conformance pass; docs + builder notes.

**Threading note (C++17 — no coroutines):** the MCP server runs on its own thread; reads served from snapshots/read locks, writes queued to the sim thread via the existing job system. The engine's fail-fast policy applies: malformed protocol input is rejected, never trusted.

## 8. Open questions for JD

1. Default HTTP port (provisional `18420`) and the in-game UX for approving agent connections / granting permission masks.
2. Which write tools are v1 vs v2 (`world_set_block` in v1? `sim_pause`?).
3. Whether the future frontier-powered Daemon should consume this same MCP surface (it should — one on-ramp for all agents, including the boss).
4. Remote-exposure story: tunnel recommendation vs documented-only.

---
*Sources: modelcontextprotocol/specification (GitHub) and modelcontextprotocol.io versioned spec pages; the 2025-11-25 `schema.ts`; official SDK docs (TS/Python/Go/C#); docs.x.ai (Grok MCP, remote MCP tools); Llama Stack and llama.cpp MCP documentation; community C++ implementations listed in §3 (Apache-2.0/MIT, studied for understanding only — no code taken).*
