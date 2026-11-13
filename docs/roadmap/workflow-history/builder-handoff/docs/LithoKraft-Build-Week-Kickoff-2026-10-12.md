# LithoKraft — Build Week Kickoff Checklist (v2)
Prepared by Elfie, October 10, 2026. Refreshes the October 9 checklist ("Voxel Compute" working name — retired) with everything JD locked on October 10. For the week of October 12, when Grok Build usage resets and the builder takes over implementation. Prioritized; stop anywhere and the game is still better for it.

## Where we stand — locked baseline, Oct 10

- **Names:** LithoLattice = the moddable voxel engine. LithoKraft = the game (mod #1). K spelling; only L and K capitalized.
- **Conventions locked for the builder handoff:** `litholattice::` engine namespace, `I`-prefix interfaces, `k`-prefix constants, `-OR` component names, Trace/Signal terminology. New code already audited against them (UML + standards review delivered Oct 10).
- **Wireless signal system — DONE:** SignalTransmittor + SignalReceivor blocks, both TX modes (use toggles input/beacon, sneak+use cycles channels), link-budget radio registry, 1-tick latency, 4→16→64 channel gating, TX power 256 approved, sneak-cycles-channel UX approved, save migration, SGNL v1→v2 migration fixed with boundary tests. Single-block Dish done: `kDishSensitivity = 1024` (~5x Receivor range), channel 0 monitoring stubbed for the Daemon pipeline; multiblock Dish deferred until in-game testing.
- **Item entities — DONE sim-side:** EntityManager + ItemBehavior (gravity, rest, magnet within 4 blocks, pickup within 1.2, **8-minute despawn** — 8 on its side is the infinity symbol). Popped trace emits an item-drop event from the sim thread; the game thread spawns it with a toss.
- **Catchor rebind — DONE:** sneak+use on a bound Catchor clears the binding; on an unbound one cycles the match value. No more break-and-replace.
- **Inventory — LOCKED:** exactly 42 carry slots (8 hotbar + 2 offhands left/right + 32 main in 4 rows of 8; 42 = 101010 in binary, 32+8+2) plus 8 worn gear slots (head, chest, legs, feet, arms, face, heart, mind). **Decided Oct 10:** max stack 512; pickup fills inventory, leftovers stay in the world, full inventory leaves the item; UI hotbar fixed 9 → 8. **Still open:** offhand mechanics, gear-slot behavior, inventory UI beyond the hotbar, persistence, dedicated tests.
- **Storage primitives:** Cache (chest) + Buffer approved; **Sortor cut** — players build sorters from ICs. Principle: primitives over appliances.
- **Tests:** 333 passing (local), Linux GCC 13.3 clean (3 benign pre-existing warnings); Cortana's MSVC workstation check also green. Only failure: Vulkan init in this VM (no GPU — passes on real hardware).
- **Tick:** Moore's-law progression; 512 is the launch baseline; cap is a configurable power of 2.
- **Roadmap confirmed:** entity rendering next; finish item rendering + inventory integration (scaffolding only so far) flagged right after.
- **Audio — LOCKED (afternoon):** 3D spatial sound with voxel acoustics (reflections off solid blocks, cave echoes, wall occlusion) driven by block solidity/opacity data; AI ties in (mobs hear you, the Daemon listens).
- **Combat primitives — LOCKED (afternoon):** `BlockDef::density` (0–255, projectile resistance) + projectile `penetration` (Half-Life-style punch-through with damage falloff); engine-side `IProjectile` with `ProjectileManager` and chunk-ahead loading along deterministic paths. Weapon designs still JD's.
- **Dimensions — LOCKED (afternoon):** 5 at launch with the five-senses theme (each dimension engages a different sense) + 2 post-Daemon roadmap dimensions (5. The Ether — spiritual, friend-suggested, Chris S. from "314" inspiration; 6. The Undefined — programming-forward name, where rules break down). Fab Table 5×5 locked as the pre-lithography crafting station.
- **Outreach:** JD emailed Andreas (AngelScript); reply received; fragment-level follow-up drafted and filed — JD's lane to send.
- **Oct 10 8-item checklist (with Elfie, reviewed after each item):** item rendering scaffolding landed (IRenderer::uploadEntityMesh seam, GL dynamic VBO + camera-facing item quads; Vulkan backend is a no-op stub, GL buffer capped at 4096 vertices, texture layers still at 0 — real-GPU visual test pending); radio registry per-channel index done; cross-dimensional Signal partial (dimension byte on broadcasts, non-Daemon range halved cross-dimension — LithoKraft doesn't call setDimension yet, no dedicated test); Overclockor tiers reverted after block-registration startup aborts (base Overclockor only — tiers/tick acceleration pending); falling blocks scaffold only (no block flagged falling, placement callback not wired, no dedicated test). UML docs in progress.
- **AngelScript mod bindings:** the engine runs scripts, but only `log()`/strings are bound — no IMod interface, no block/component registration exposed. JD asked Oct 10; this is a builder task for build week (priority item 12).
- **Pattern ROM rewrite** — parked as a design conversation, not a code task (Oct 10).

## The one rule for the week

Decisions stay provisional until the prototype shows you what's best — every item below ends with something *visible and playable*, not just merged code.

## Before Monday: handoff readiness

The builder takes implementation around Oct 12. Elfie stays in the review-and-verify lane. Before the handoff:
1. Close the remaining open inventory questions (offhand mechanics, gear-slot behavior, inventory UI beyond the hotbar, persistence) — stack sizes and pickup priority were decided Oct 10; what's left gates a finished item loop.
2. Clean the leftover `vc/` folder from the retired working name before it fossilizes.
3. Confirm the standards-review UML set (`litholattice-uml-2026-10-10/`) reads as the builder's style bible for the -OR/namespace/I-prefix schemes.

## Priority order

1. **Decide the remaining inventory questions.** Offhand mechanics, gear-slot behavior, UI, and persistence — the cheapest decisions of the week and the only blockers between done code and a finished, playable item loop.
2. **Finish item rendering + inventory integration.** Scaffolding is in (Oct 10); still needed: texture layers, the Vulkan backend, raising the GL vertex cap, and a real-GPU visual test. Then: popped trace → rendered item drop → pickup into the 42-slot inventory. First visible, playable loop of the engine's whole item system.
3. **Entity rendering.** Already next on the roadmap; unblocks mobs and NPCs (Librarian, solder sprites, failure-mode mobs).
4. **Vertical slice: wire → torch → lamp at 32 TPS (GL, already running).** Lock the tick-rate baseline everything else inherits — prove against it, don't touch it.
5. **Same slice on Vulkan.** Proves the thin renderer interface holds two backends before more code piles on. Highest technical risk — retire it in week one.
6. **Core logic block set.** Torch (inverter), repeater, comparator, redirector, fast latch, switch/button, lamp, tiered lamps (16/32/64). No NAND, no clocks — players build those.
7. **Red/blue wire + 0–64 strength + visual strength readout on the wire.** Mandatory for playability; built in from the start, not a debug overlay later.
8. **Heat model v1.** Fans and heat sinks as blocks; big/fast circuits overheat and burn out. Introduce early so thermal layout feels native, not a tax added later.
9. **Tick Overclockors 64/128/256.** The early-game speed bridge. Nail "which clock wins" semantics now (Overclockor zone rate vs. world rate) so it never becomes a bug class. (Tier-3 reaches the system clock; day/night follows system time.)
10. **Bytecode save/blueprint format draft.** Compact, portable, Workshop-ready from day one. The format shapes the JIT boundary too.
11. **Lithography v1 (only if 1–10 are solid).** One shrink per build, timed printing, power-hungry, cooling-hungry. The signature mechanic — don't rush it to hit a weekly quota.
12. **AngelScript mod-API bindings.** IMod interface, block/component registration, lifecycle hooks — the engine executes scripts but mods can't register anything yet (only `log()`/strings bound). Builder task; the sealed-box sandboxing review pairs with it.

## Deliberately NOT this week

- Overclockor utility functions (cooling aura, haste) — gated behind discoverable items, which don't exist yet.
- EUV/DUV tiers, argon vents, deep-ore distribution — the survival arc tunes once the core loop is fun.
- Rocket phase — full-guidance vs. autopilot-assist gets decided after the CPU itself is playable.
- DX12 or additional renderers — deferred to post-launch by decision.
- **Launch marketing** — secrecy policy is already locked: talk only about the Wafer; four known dimensions may be announced; the Daemon and endgame stay under lock and key (fragments only). Nothing to build around that this week.
- Sortor — cut by decision; players build sorters from ICs.

## Tooling health (before kickoff)

The week runs on Grok Build (usage resets Oct 12) + the builder. Confirm: (1) the Android Grok app's "acting funny" episode (logged with Master Chief, Oct 9) has a captured symptom and a resolution, not just a parked build; (2) Grok billing and active sessions look clean — last week's credential-draining botnet targeted paid Grok users; (3) the parked procedural-generation build on TheWorkstation is unparked and resumable. A flaky tool eats build days faster than any design problem.

## The grind-gate watch

The brief's steps 4→5 (full-size ICs + clock + basic CPU *before* the first lithography machine) is the biggest quit risk in the game. This week's job is to find the smallest possible version of that climb that still feels like an achievement — the prototype will tell you where the wall is.

## Style anchor (art)

Lock one sentence before generating anything: e.g. "dark silicon wafer, copper traces, teal glow." Generate everything against it, so a future human artist elevates a coherent look instead of restarting. Plan confirmed: start AI-generated, bring in a real artist later for soul and feel. Keep AI art away from Steam capsule images.

## Addendum — the engine's biggest 48 hours (Oct 10 evening → Oct 11 morning)

Everything below landed AFTER the v2 baseline above was written. The priority order stands; this is the state the builder and Grok must start from, not the Oct 10 morning snapshot.

**Landed and green:**
- **Net stack reconciled** (35 checks): reliable+unreliable, version handshake, ChaCha20-Poly1305 chosen. Builder seams: vendor libsodium, implement ICipher, v1-peer story.
- **Sim follow-ups landed**: Trace JIT seam (with builder notes), all three signal granularities as options, fail-fast log+abort on sim corruption. Deferred to builder: `SimThread::run()` try/catch + holistic scheduler-inside-SimThread integration.
- **File hardening landed** (103/103): 3-gate `SandboxedFile` opens, 13/13 attack sims, backup depth 2–32 (default 8).
- **Mod hardening landed**: binding allowlist, CPU 1024µs + mem 1MiB quotas, SDK enforcement, save CRC quarantine, net/UI/input confinement, permission manifest. JD-decided: no std::string blind spots, auto-suspension recovery + console logging. Builder: the API binder (nothing bound anywhere yet — every binding gated on `ModInfo::grantedPermissions`) + game-side wiring (incl. `setFirstPartyIds({"lithokraft"})`).
- **Debug console landed**: HL1-style dropdown, HL1 console language, permission-gated AngelScript bindings, auto suspension recovery 64→4096 ticks. Builder: ModLoader→`setModId()` wiring, game-target TextEditor/TextBox refs.
- **lithoscript (lowercase, official) locked**: HL1-style console language, dotted naming, `wait N` included (tick-based, max 512), full HL1/Quake feature set, `query` command for agents, engine text indexer; Fragment-authored scripts = SEMI-TRUST. Machinery complete; **the surface is empty — builder populates cvars/commands** (biggest surface gap; `bind` dispatch unwired).
- **IExtensible landed**: C-ABI facet, ExtensionRegistry, depth cap 8, cycle-detecting loader, crash isolation PROVEN. Builder: wire ExtensionLoader into game boot (mod.json discovery approved).
- **Codec stubs landed**: WAV full decode by hand; FLAC/Opus/Vorbis header-parse + vendoring picks (dr_flac / libopus / stb_vorbis, no GPL). Builder: frame decoding + resampling ownership decision.
- **MCP server landed (Oct 11 ~10:35 AM EDT)**: ~7,300 lines, 18 files, zero new deps — clean-room JSON-RPC 2.0; stdio + Streamable HTTP (127.0.0.1:18420 provisional) + bearer auth; gated write tools; real conformance vs the official TS SDK; ~320 checks green. Open for JD: port, rate numbers, approval UX, Daemon-on-this-surface.
- **Launch menu landed** (53/53 UI selftests, 'A LITHOLATTICE GAME'). Pre-existing blocker: blocks.cpp content-table desync (40 rows vs guard 36 → SIGABRT) — builder fix before anything runs.
- **PBR phase 2 landed** (GL backend, per-voxel MATS v1, true refraction, tier UI wired; golden hash root-caused). Transparent tier deliberately unwired until real; Vulkan + software-refraction open.
- **Defrag service landed** (66/66 + 9): RecordStore 4 KiB pages, chunked per-tick pump, crash-safe, paired with indexer reindex.
- **Audio + threading landed**, with JD's requirement in: verbose compile-flag-gated job debug logging (job lifecycle, lock ordering, wake/sleep), zero overhead when off.

**Design-only (not built): ReadOR** — slot the Fragment in; it sends the Fragment's LithoScripts to a keyboard component feeding contraptions. Builder design work; trust model locked (agent/proxy scripts run at the *agent's* permissions).

**JD decisions since v2 (in memory; do not re-ask)**: mesher = BinaryQuad + PackedVertex coexist; region quantum 2–16 KiB (8 KiB default); atomicity = tiny journal + per-record; TickArena `Handle` = 64-bit opaque; NoteOn throttle = step down Hz; headless harness approved; FD re-validation O_NOFOLLOW + re-check; net = reliable+unreliable now, semantic versioning, encryption stubs; SIM = Trace JIT seam now, SimThread deferred to builder; signed mods = future idea. Principle: "we like options." LithoScript D1/D8/D9/D10 locked; D6 superseded (wait included); D2/D3/D4/D5/D7 still open for him.

**Current-state pointers for build week**: the 16 implementation reports + ReadOR design note + HL1 comparison live in `~/workspace/user/files/`; the itemized builder queue (15 items, dependency-ordered) is `Builder-Handoff-Inventory-2026-10-11.md` — note it was assembled 09:55 EDT, so the MCP server reads "queued" there; it's DONE as of 10:35 EDT. The consolidated handoff package is next once JD approves the pending task.

---
Grounded in: the Oct 9 kickoff checklist (`Voxel-Compute-Build-Week-Kickoff.md`), JD's 2026-10-10 locked decisions (inventory 42+8, despawn 8 min, conventions, wireless/item-entity/Catchor/Dish completions, storage primitives, test state, Moore's-law ticks), the Master Builder Brief (`Lithokraft-Master-Builder-Brief-2026-10-10.md`), and the standards/UML review delivered Oct 10, and the Oct 10 8-item checklist implementation + review pass (checklist file refreshed 11:45 AM EDT to match the corrected in-tree state).

## Addendum — Oct 11 midday: repo audit, the fix program, and the newest locks

Everything below landed on Oct 11 AFTER the earlier addendum was written. The fix program is now the top of the build order; the rest of the priority list stands behind it.

**Landed / decided:**
- **Repo audit LANDED (Oct 11, 6 reviewers, read-only):** 68 findings — 1 CRITICAL, 12 HIGH, 30 MEDIUM, 25 LOW — plus 25 optimizations, 14 dead-code entries. The CRITICAL: a PackedVertex slot broke GPU vertex-attribute bindings, meaning all chunks corrupt on real GPUs (the VM never caught it). 10 proposed alpha blockers are JD's to decide — he's reviewing them now. Report: `Repo-Audit-2026-10-11.md`.
- **FIXES AUTHORIZED (JD, Oct 11 ~12:13pm):** implement all recommended fixes, most optimized; JD reviews the report and overrides any fix he disagrees with. The fix run is underway (five crews across sim, net, script/mod/MCP, audio/storage, game, plus the render section — including the critical GPU binding fix and the atomic-write world recovery). The fix report lands when the crews finish; it's the build week's day-one input.
- **WORLD PERMANENCE doctrine (JD, Oct 11):** preserve the world at the highest priority — corruption is a hard no. Design goal: players get in and never reset their worlds, all the way to the Daemon, however long that takes. The storage fixes (atomic-write recovery, backup rotation) are that doctrine in code.
- **MCP correction + next:** port LOCKED at `127.0.0.1:1337` (leet, naturally) — supersedes the "18420 provisional" line in the earlier addendum. ~320 MCP checks green, ctest 18/20. Next: MCP player-control hooks — models pilot players through the real input path, tick-aligned, permission-gated, architected for MULTIPLE pilots from day one; agent vision = downgraded software-renderer frames per pilot; agent audio = mixed output at the lowest degradation tier. Fun milestone on the books: the first agent to solo the Daemon with nothing but the MCP connection — "You didn't just build a game with an MCP interface. You built the arena."
- **Lithography machine components (JD design, Oct 11 ~9:15am):** the factory loop is locked — **FabOR/AutoFabOR pre-fabricates blank wafers** (Fab Table renamed **FabOR** to fit the -OR family) → hopper/dropper buffer line feeds them factory-style → **EtchOR** etches the **SelectOR** pattern with the **Laysor** → chips out the front. The EtchOR has ONE UI element: a name field; SelectOR wiring = 4 red + 4 blue Trace on ConnectOR top faces; Laysor contacts the EtchOR left+right only. Diagram v4/v5: `workspace/lithokraft/etchor-assembly-diagram.html`.
- **Code budget LOCKED (JD, Oct 11):** PRODUCTION code stays under **64k lines** (2^16 — the motif holds). ~47k now, ~17k headroom; tests don't count.
- **Alpha launch target: JD's birthday, 2026-10-23** — 12 days out. Post-MCP-control-hooks plan: JD + the builder go bug testing / bug finding / Trace in-game verification ("seeing it work in the game is the real key factor").
- **Builder start CONFIRMED: ~5:07 PM EDT Monday 2026-10-12** — JD gets the local build on his machine for hands-on testing/bug-finding with the proxy.
- **Builder-handoff inventory ready:** 15 builder items indexed from all 19 docs (`Builder-Handoff-Inventory-2026-10-11.md`) — the approved fix task starts instantly.
- **ReadOR (design only, Oct 11):** slot the Fragment in; it sends the Fragment's LithoScripts to a keyboard component feeding contraptions — the physical mechanism for the Fragment → LithoScript → world pipeline.

**Priority order UPDATE:** the fix program runs first; JD's review of the fix report (with override rights) second; then the existing build order below. New work — camera system interface, IRegistry, the `render.ambient_mode` cvar (flood-fill ambient fraction default, normalize-the-sun variant as an option), console autocomplete (HL1-style tab completion) — stays concept-only until the fixes land.

**Still open for JD (do not re-ask):** the 10 alpha blockers; LithoScript D2/D3/D4/D5/D7; rate numbers / approval UX / remote-exposure story for MCP; offhand mechanics, gear-slot behavior, inventory UI beyond the hotbar, persistence; positive engine marking on system dialogs; backup disk-cost baseline testing; `SimThread::run()` try/catch (builder).

## Addendum — Oct 11 evening: fix program outcome, FOV culling, Monday's workstation agenda

Everything below happened on Oct 11 AFTER the midday addendum. The fix program is no longer "underway" — it's verified. Update the priority picture: Monday's workstation session is the next venue.

**Fix program — done and reviewed twice:**
- Fix crews finished; follow-up fix batch done ~2:18 PM EDT (all 4 questionable fixes resolved, both mediums fixed, 4 of 6 lows applied). Full rebuild clean, ctest 18/20 (only the 2 grandfathered failures), all affected suites pass.
- **Regression review #1** (2:11 PM, 4 read-only reviewers): ~62 fixes verified clean, 4 questionable, 0 broken, plus 8 fresh findings (2 medium, 6 low).
- **Regression review #2** (2:54–2:59 PM): 8 verified, 2 questionable, 0 broken. The 2 questionable: **meshKey UB** (fix landed at :331, `uploadPbrChunkMesh` :204 still has the old signed-shift form — needs a `uint64_t` cast at `gl33_renderer.cpp:204`) and **cvar clamps** (`mcp.audio_tier` + `camera.chunk_radius` hooked, but `vision_width`/`vision_height` document 16..512 with no clamp). Fresh sweep: 1 medium — `yaw` unvalidated on the `vision.set_camera` path (same NaN hole the x/y/z fix closed) — plus 3 lows (fail-closed debris, spurious log, subnormal rejection). Behavior changes: 7 clean, 3 with caveats.
- **Held (not broken, not fixed):** 2 flagged lows — `ambient_mode` on Vulkan/MCP vision (builder feature, not a fix) and `loadDirectory` symlinks (by design, local-trust dir). Carried-forward flags: KEX work, `compSleep_` default, SBO job type, permission version 2→3, backup rotation redesign, `cfg_.tps` sync, vision rate-limit flake.

**FOV culling — IMPLEMENTED (JD, Oct 11 ~3:11 PM):** frustum occlusion culling with configurable angular margin on all three pipelines (GL, Vulkan, software/CPU) plus agent vision cameras. New cvar `render.frustum_margin_deg` (default 5°, range 0..15). FOV cap 120 verified (60/90 typical). Sim untouched — runs independently of what's rendered. Culled chunks stay resident in memory and keep getting sim-ticked, so re-entry is free (no reload, just re-submit). Tests green. JD's term for the pop-in the margin prevents: "frustum occlusion tearing."

**Monday's workstation session agenda (JD + builder, ~5:07 PM EDT Oct 12):** live pre-alpha testing of Trace and Signal mechanics, bug finding/fixing, mechanic refinement — with regression #2's three one-liners as the fix list (uint64_t cast at `gl33_renderer.cpp:204`, yaw guard in `setCameraPose`, clamp hooks for `vision_width`/`vision_height`). JD gets the local build on his machine for hands-on testing with the proxy.

**Wafer alpha procgen — design thread open:** JD wants biomes and mobs with homes for the alpha landing (broken fragmented circuits underground telling what Trace does; silicon-type trees; fantasy feel). Ideas in discussion, nothing locked yet.

**Still concept-only until fixes land (do not re-ask):** camera system interface, IRegistry, `render.ambient_mode` cvar (flood-fill ambient fraction default, normalize-the-sun variant as option), console autocomplete (HL1-style tab completion, built as a reusable unit for the future Fragment script editor).
