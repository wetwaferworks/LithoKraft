# DRAFT — LithoLattice / LithoKraft: Open, Source-Available, and Closed Headers

**DRAFT — for JD to see and mold. Not a decision document. Nothing in §2 is locked.**
Date: 2026-10-14 · Composed from the actual tree at `~/workspace/litholattice/` (read-only; no code changed).
Ancestor document: `docs/SDK.md` (API framework design draft, 2026-10-09), whose Half-Life-SDK model this draft updates against the code as it stands today.

---

## 0. What JD is looking at

The end-state picture, from a modder's chair. A modder (or licensee) receives a package — call it `LithoLattice-SDK/` — containing:

- `include/` — the **open headers** (§2, Tier A): the C-ABI mod interface, the manifest and permission contracts, the data-format and registry headers a mod compiles against. Declarations and contracts only — no engine implementation.
- `lib/` — the prebuilt engine library to link against.
- `docs/` — the SDK manual (the grown-up form of `docs/SDK.md`).
- `sample-game/` — LithoKraft's own game-layer source as the documented reference implementation ("mod #1"), under its own mod license.

They never receive: the engine `.cpp` files (those move only under the source-available license, Tier B), the Timing Handshake internals, or anything that can *produce* a signature. Verification is public — anyone can check who vouched for a mod. Signing is not — only JD's offline key can vouch. That asymmetry is the integrity story of the whole split, and §3 is devoted to it.

---

## 1. Inventory — the header surface as it exists today

### 1.1 Engine (`engine/src/`, namespace `litholattice::`)

30 modules. **247 public headers** under `*/include/litholattice/<module>/` (229 API headers + 18 `*_selftest.h`), **20 private headers** that already live outside the include trees (GL/Vulkan internals, `noise_impl.h`, `graph_nodes.h`, vendored NanoSVG), and **222 `.cpp` files**.

| Group | Modules (public headers, excl. selftest) | Representative headers |
|---|---|---|
| Core & platform | core (13), platform (1), defrag (1), index (1) | `core/json.h`, `core/job_system.h`, `core/tick_arena.h`, `platform/window.h` |
| World & storage | world (12), storage (7), subdiv (2) | `world/block.h`, `world/chunk_map.h`, `world/dimension.h`, `storage/file_contract.h`, `storage/block_names.h`, `subdiv/octree.h` |
| Simulation | sim (12), entity (4), projectile (4), ai (4) | `sim/tick_interfaces.h` (ITickable), `sim/world_simulation.h`, `sim/control_stream.h`, `sim/idemo_record.h`, `sim/timing_handshake.h` |
| Rendering | render (35), render_vk (2), sprite (4), texture (2), anim (2) | `render/renderer.h` (IRenderer), `render/mesher.h`, `render/software_renderer.h`, `sprite/sprite_registry.h` |
| Player, camera, input | player (11), camera (1), input (12), console (6) | `player/iplayer.h` (IPlayer), `camera/camera.h` (ICamera), `input/action_map.h`, `input/mod_input.h`, `console/iworldquery.h` |
| Content pipeline | procgen (17), assets (9), systerian (1), ui (16), audio (15) | `procgen/pipeline.h`, `procgen/litho_formula*.h`, `assets/mod_manifest.h`, `assets/resource_pack.h`, `systerian/isysterian.h` (ISysterian), `ui/widget.h` |
| Mods & scripting | mods (10), script (3) | `mods/mod_interface.h` (C ABI), `mods/extensible_interface.h` (IExtensible), `mods/mod_permissions.h`, `mods/mod_safety.h`, `script/script_engine.h`, `script/script_binding_allowlist.h` |
| Agents & net | mcp (8), net (10) | `mcp/server.h` (injectable seam interfaces), `net/protocol.h`, `net/confinement.h`, `net/handshake.h` |
| Integrity | crypto (4), assets (signing parts) | `crypto/ed25519.h`, `crypto/sha256.h`, `crypto/sha512.h`, `crypto/signing.h`, `assets/mod_signature.h` |

The I-family interface surface a third party would program against, as it stands: **IRenderer, IPlayer (+ IPlayerListener), ICamera (+ ICameraTarget / ICameraWorld), ISysterian, IDemoRecord, ITickable, IWorldSimulation**, the procgen stage interfaces (`IClimateStage`, `IBiomeSelector`, `ITerrainStage`, `ISurfaceRule`, `ICarver`, `IFeature`, `INoise` — in `procgen/pipeline.h` / `noise.h`), the MCP seam interfaces (`ISimStatus`, `IWorldReader`, … in `mcp/server.h`), `console::IWorldQuery`, and the two C-ABI facets (**IMod** in `mods/mod_interface.h`, **IExtensible** in `mods/extensible_interface.h`).

### 1.2 The modding framework as built

- **Manifest (canonical):** `assets/mod_manifest.h` — schema-1 JSON: `id`, `name`, semver `version`, `type` (content / script / native), `deps`, `permissions` as `{api, reason}` pairs, plus `sealed` and `hotSwap` flags. This is the system the game actually uses (content packs, boot seal, hot-swap decisions in `game/LithoKraft/src/app/main.cpp`).
- **C-ABI mod interface:** `mods/mod_interface.h` — `LITHOLATTICE_SDK_VERSION 1`; no STL/exceptions/RTTI across the boundary; opaque handles; versioned structs (size first); a 12+-bit permission enum (`kPermBlocks`, `kPermWorldRead/Write`, `kPermTick`, `kPermEvents`, `kPermUI`, `kPermInput`, `kPermAssets`, `kPermNet`, `kPermConsole`, `kPermPlayerControl`, `kPermPilotSenses`, …). Static linking today, designed DLL-ready (Half-Life-SDK style).
- **Extensions:** `mods/extensible_interface.h` + `extension_registry.h` / `extension_loader.h` / `extension_runtime.h` / `extension_budget.h` — a mod can host sub-mods ("extensions") with declared slots and per-tick budget shares.
- **Second loader (staged):** `mods/mod_loader.h` — a different manifest vocabulary (`"sdk"` + permission names → kPerm bits) with a deny-by-default permission gate (rationale shown at install; a denied *critical* bit refuses the load, a denied non-critical bit is stripped). **Honestly flagged in its own header as STAGED / UNWIRED**: no game code loads mods through it; it is exercised by selftests only, and any future script/native wiring migrates onto the canonical manifest first.
- **Scripting:** `script/script_engine.h` — sandboxed AngelScript host. One engine instance per mod; scripts receive only allowlisted bindings gated on the mod's permission bits (`script_binding_allowlist.h`); per-tick CPU budget and memory quota; overruns throttle, then suspend, under a logged suspension/recovery policy (`suspension_policy.h`, `mods/mod_safety.h`). No file, network, or system access from scripts.
- **Agent surface:** `mcp/server.h` — MCP server (stdio transport wired; streamable-HTTP present) built entirely on injectable seam interfaces, so the MCP library has no link dependency on engine internals; agent actions are gated on the same `kPerm*` bits (player control and pilot senses are separate, deny-by-default permissions).

### 1.3 The game (`game/LithoKraft/`, namespace `lithokraft::`)

81 headers, 106 `.cpp` files. Layout:

- Public-by-location: `include/lithokraft/kharge.h` (1), `src/content/include/lithokraft/content/` (15), `src/game/include/lithokraft/game/` (45). Internal-by-location: 20 headers sitting beside their `.cpp` files under `src/app/`, `src/content/{components,model,vegetation}/`, `src/headless/`.
- Content registry headers (`content/`): `blocks.h`, `items.h`, `component.h` / `components.h`, `signal.h` / `signal_world.h`, `appearance.h`, `certificates.h`, `etching` support, `music.h`, `sfx.h`, `silica.h`, model & vegetation headers.
- Game systems headers (`game/`): `game.h`, `engine.h`, `player.h` / `player_adapter.h`, `inventory.h`, `dimensions.h`, `world_save.h`, `signing.h` (certificate-table flow), `fragment_*.h`, `ai_host.h`, MCP adapters (`mcp_player_control.h`, `mcp_senses_adapter.h`), UI/settings headers.
- Per the SDK design, LithoKraft is **mod #1**: it is meant to use only the public engine API, so its source doubles as the sample game (see §2 — this is a proposal with a real cost, flagged there).

---

## 2. The split — PROPOSAL for JD's red pen

Three tiers. **(A) Open** = headers + contracts a modder compiles against; ships in the SDK package. **(B) Source-available** = full source under license (the Unreal-style model from JD's standing engine-licensing note); ships only to licensees. **(C) Closed** = never ships. The normal case in this codebase is a *straddle*: interface in A, implementation in B or C. Nothing below is decided — each row is a pencil line.

### Tier A — Open headers (the SDK package)

| Module / surface | Why it belongs here |
|---|---|
| `mods/mod_interface.h`, `mods/extensible_interface.h` | The mod contract itself; a C ABI is only useful if it's public |
| `assets/mod_manifest.h` (schema + `SignatureStatus`) | Modders must author manifests and read trust outcomes |
| `assets/mod_signature.h` — **verify API only** | Verification must be public to mean anything (see §3) |
| `mods/mod_permissions.h`, `script/script_binding_allowlist.h` | Modders declare against the permission vocabulary; scripts bind against the allowlist |
| `sim/idemo_record.h`, `sim/control_stream.h` | LLDM/LLCS are wire formats; replay tooling and agents need the contracts |
| `sim/tick_interfaces.h` (ITickable), `sim/world_simulation.h` | The sim extension points mods and games register into |
| `player/iplayer.h`, `camera/camera.h` (ICamera family) | Agent/player/camera pilots are a designed public surface |
| `console/iworldquery.h`, `mcp/server.h` seam interfaces | The agent query/control seams are deliberately dependency-free |
| `input/` action & binding headers, `ui/` widget headers, `assets/resource_pack.h`, `assets/block_model.h` | Content mods live here: actions, panels, packs, models |
| `world/block.h`, `world/dimension.h`, procgen *data* contracts (`pipeline.h` stage interfaces, `noise.h`, `generator_loader.h`) | Blocks, dimensions, and generator graphs are data-driven modding surface |
| `systerian/isysterian.h` | The codec is a player-facing feature (certificates, SVG export); its interface is a feature, not a secret |
| Game layer: `content/` registry headers + LithoKraft source as sample game | Per `docs/SDK.md`: LithoKraft is mod #1 and the documented extension point — **but see red-pen item 2** |

### Tier B — Source-available under license

| Module / surface | Why |
|---|---|
| Engine `.cpp` implementations behind the Tier-A interfaces (render backends, world/chunk machinery, storage, audio, UI, procgen implementations) | Licensees (porting, deep integration, support) need real source; the public doesn't |
| `render/` + `render_vk/` internals (incl. the 20 already-private headers: `vk_*`, `gl33/*`) | Already treated as internal by file location |
| `crypto/` implementations (`ed25519.cpp`, `sha256.cpp`, `sha512.cpp`, `signing.cpp` canonical-hash code) | Auditable by licensees; verify *API* is Tier A, the primitive implementations don't need to be public |
| `procgen/litho_formula*.h` + implementation | **Straddle, flagged:** the formula API is modder-visible surface, but the Litho Formula is signature IP — proposal: headers' *data types* in A, the formula + voxelizer implementation in B (red-pen item 3) |
| Game systems implementations (`game/` `.cpp` files) | Ship with the sample game under the mod license rather than fully open, if JD prefers |

### Tier C — Closed (never ships)

| Module / surface | Why |
|---|---|
| `sim/timing_handshake.h` + implementation, `sim/det_sched.h`, `sim/trace_jit*.h` | The Timing Handshake is the crown jewel. **Note:** `timing_handshake.h` currently sits in the *public include tree* — the split requires physically moving it (and its kin) out of `include/` before any SDK package is cut (red-pen item 1) |
| Signing *production*: `crypto/litho_sign_main.cpp` (the `litho_sign` keygen/sign/verify tool), all private-key handling | Only JD signs. The tool and the key ceremony never leave his offline machine |
| Enforcement internals: `mods/mod_safety.h` implementation, `storage/sandboxed_io.h` / `mod_sandbox.h` implementation, `net/confinement.h` implementation | The *contracts* are Tier A (mods must know the rules); the enforcement machinery stays closed so the rules can't be studied for gaps |
| Anti-cheat / authoritative-server internals in `net/` beyond the protocol contract | Same reasoning: publish the protocol, not the referee |

### Red-pen items — the choices that matter most

1. **The Timing Handshake header is public-by-location today.** `engine/src/sim/include/litholattice/sim/timing_handshake.h` sits in the include tree alongside genuinely public headers. Any packaging script that zips `include/` verbatim would ship it. The split's first mechanical act — before any license text matters — is relocating the closed sim headers to a private tree.
2. **LithoKraft as open sample game vs. held-back content.** `docs/SDK.md` promises the sample game's full source. That is the strongest possible modding accelerator — and it hands over every content system (signal components, certificates, etching, Fragment) as ready-made code. Middle path on the table: game *headers* + content registries open, game *implementations* under the mod license (Tier B mechanics, Tier A visibility).
3. **Litho Formula: how much of the signature shows.** Data contracts open (mods can *use* formula-driven generation), implementation licensed. Showing the voxelizer source is showing the trick; showing only the API keeps the trick and sells the license.

---

## 3. Integrity — the hardening layer as it stands, and the planned tier

JD's requirement: "keep the integrity of the game very honest." This section describes only what is verifiably in the tree today, then names what JD has planned — clearly separated.

### 3.1 What stands today (verified in-tree)

**Primitives** (`engine/src/crypto/`): clean-room SHA-256 / SHA-512 (FIPS 180-4, NIST vectors) and Ed25519 (RFC 8032, vector-verified and differential-tested against libsodium in development). The module's own README states the honest scope: **not constant-time; not for high-threat contexts.** Verification runs on public inputs only.

**Canonical mod hash** (`crypto/signing.h`): a byte-exact spec — recursive walk, regular files only, symlinks never followed, `signature.json` excluded, paths sorted byte-wise, SHA-256 over length-prefixed path+content pairs with the file count hashed first. The length-prefixed framing is **v2 (2026-10-13)**: v1's unframed stream was ambiguous and let a signature be transplanted between different mods (review finding H1); v2 closed it, and because no signed mod had ever shipped, every signature could be re-issued against v2 at zero cost.

**Verification** (`assets/mod_signature.h/.cpp`): `verifyModSignature()` checks a mod's `signature.json` against the public key baked into the engine. Three outcomes, by design (JD's trust model, 2026-10-12: *certificates answer WHO vouches for the mod; sandboxing answers WHAT it can do*):

- **Verified** — signature names the baked-in key and verifies → full trust.
- **Unsigned** — no `signature.json` → loads **sandboxed and clearly labeled** (boot log line included).
- **Invalid** — signature present but malformed, wrong key, or non-verifying → **refused in all builds, debug included. Fail closed.**

The same baked-in key is also the trust anchor for procgen structure signatures (`StructureRegistry::setTrustedPublicKeyHex` at boot): until it is set, no structure can verify Trusted.

**Boot seal** (`game/LithoKraft/src/app/main.cpp`): the runtime consequence of verification. A manifest's `sealed: true` always wins. In release builds the registry opens **only** when every loaded mod is Verified *and* at least one verified mod opts into hot-swap; anything else — including a stock install with zero mods — seals, fail closed. Debug builds stay open for the developer loop unless sealed.

**The test key — stated plainly.** The public key currently baked into `mod_signature.h` is a **throwaway test key** (its other half was a development keypair); it is known-compromised for production purposes and is marked `REPLACE BEFORE BETA` in the header itself, with the replacement procedure documented beside it (offline keygen with `litho_sign`, paste the public hex, rebuild, verify). The tree defends itself in the meantime: **a release build refuses to boot at all** while the test key is baked in (`bakedKeyIsTestKey()` guard in `main.cpp` — hard `exit(1)` under `NDEBUG`, loud warning in debug). **Replacing the key is JD's offline action and remains pending**; no key material of any kind appears in this draft.

**Around the signature**, the rest of the basic layer: deny-by-default permission gating with per-permission rationales; the AngelScript sandbox (allowlist-only bindings, CPU/memory quotas, suspension on abuse); sandboxed mod file I/O (`storage/sandboxed_io.h`, `storage/mod_sandbox.h`); engine-gated mod networking (no raw sockets — `net/confinement.h`, rate-limited, suspendable); fail-closed demo files (LLCS/LLDM carry FNV-1a checks and refuse torn or corrupt streams); and the world-file contract (`storage/file_contract.h`, `blocks.names` with backups and fail-closed behavior) that keeps saves honest under the world-permanence doctrine.

**Terminology fence** (from the game's own headers): the in-game **certificate table** (`game/signing.h`, `content/certificates.h` — wiring pass 2026-10-13) is LithoKraft's enchanting analogue: a player spends the Static stat to apply a Certificate-of-X to an item, fail-closed on unknown certificate, bad slot, duplicate, or insufficient level. It is a game mechanic that *borrows the word* certificate. It is **not** the mod signature above and not an asset-provenance certificate; the draft keeps the three apart because JD's "certificate signing at the engine level" refers to the mod-signature system.

### 3.2 The planned tier — when financials arrive (PLANNED, unbuilt)

JD's direction (2026-10-14): once the project touches financials, the basic layer above is replaced at its seam by higher-end certificate signing and verification. `crypto/signing.h` already names the seam: *"When funding permits, real PKI certificates replace this — the verify() call site is the seam."* The upgrade dimensions, named only (designing cryptographic specifics is deliberately out of scope here):

- **Key custody** — from a single offline key file to managed custody (hardware-backed keys, a defined signing ceremony, separation of duties).
- **Verification chain** — from one baked-in self-managed public key to a certificate chain issued by a recognized CA, so "who vouches" is answered by an auditable chain instead of a hex constant.
- **Revocation** — a way to *un-vouch*: today an Invalid signature fails closed, but there is no mechanism to revoke a once-valid key or certificate; the planned tier adds revocation checking to the verify path.
- **Timestamping & build signing** — signed, timestamped builds and mod packages, so provenance covers *when* as well as *who*.
- **Process** — key rotation and re-issuance procedures (the v1→v2 re-issue was free only because nothing had shipped; after financials, rotation must be a planned operation, not an accident of timing).

Until that tier is funded and built, the shipped posture is §3.1 exactly as described — including its stated limits — and the release-boot guard ensures the test key can never silently ride into a build players receive.

---

## 4. Jenny's modifications — to be incorporated

JD (2026-10-14): Gemini (Jenny) gave excellent advice for a slight modification of the code, and it motivates part of this composition. **The specifics have not been relayed to Elfie yet, so this section is an intake slot — nothing here is invented.**

To fold her advice into this draft and the tree, the following are needed:

1. **What is the modification?** (The change itself, in her terms.)
2. **Which module(s) does it touch?** (So it can be placed in the §1 inventory and the §2 tiers.)
3. **What is the rationale?** (Performance, integrity, API shape, licensing posture?)
4. **Does it move any header between tiers?** (E.g., does it add a public header, or pull one closed?)
5. **Does it touch the integrity layer (§3)?** (If it modifies signing, verification, or the sandbox, it gets the full fail-closed review treatment before landing.)

*Slot intentionally left empty pending JD relaying Jenny's advice.*

---

## 5. Open questions for JD's red pen

1. Tier placement of `sim/timing_handshake.h` and kin: physically relocate now, or at packaging time? (Recommendation in §2: now — location is the leak.)
2. LithoKraft source: full sample-game open (per `docs/SDK.md`), or headers-open / implementation-licensed?
3. Litho Formula: API-only open, implementation licensed — or is the formula itself part of the open pitch?
4. Should the `*_selftest.h` headers (18 of them, currently inside the include trees) be excluded from any shipped package? (Presumed yes; they are test surface, not API.)
5. The staged `mods/mod_loader.h` vocabulary: keep for the future script/native era, or does the split document only the canonical manifest system? (The draft documents both and flags the staged one honestly, matching its own header.)

---

*End of draft. Prepared for JD's review; no production files were modified in its composition.*

---

## JD licensing note (dictated 2026-10-14 03:05 EDT, post-draft)

- **Source-available license = $16** at the IPO (JD's term: the *Initial
  Public Offering*): paying the $16 (the beta donation ask, same number)
  gets the closed source under the source-available tier, initially.
- **The IPO price may rise later to $32** (16 → 32, a doubling).
- Open fork for JD's daylight ratification (Cortana session, Thu
  2026-10-15): does the $16 license cover the beta era (source opens
  fully at public drop, later $32 applies to a different tier), or is
  source-available a permanent paid tier standing beside the open code?
- **Final price may be $64** (JD, 2026-10-14 03:06 EDT) — "a standard
  price for a triple A title." The ladder reads: **$16 → $32 → $64**
  (2^4 → 2^5 → 2^6).
