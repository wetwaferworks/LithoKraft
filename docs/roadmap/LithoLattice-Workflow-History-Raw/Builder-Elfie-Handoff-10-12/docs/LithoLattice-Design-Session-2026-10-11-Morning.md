# LithoLattice — Design Session 2026-10-11 (overnight/morning)

JD worked through the night (with a 3am PB&J break). This log captures decisions and inspiration credits.

## What landed overnight
- **Six engine optimizations** (bump allocator, flood-fill audio occlusion, binary meshing, .llar region layer, net snapshots, deterministic parallelism)
- **PBR phase 1** (material registry, Cook-Torrance BRDF, many-lights hybrid, transparent tiers, software-renderer chain)
- **Audio optimizations** (virtualization, SIMD, bake cadence, effect bypass, async streaming, graceful degradation)
- **Threading job system + file robustness** (work-stealing, named threads, crash-safe file contract, async I/O, mod sandboxing)
- **Launch menu** (mockup 1 classic centered wins; "A LITHOLATTICE GAME"; Wafer side-scroll backdrop) — JD: "we basically have a pre alpha going here"
- **Mod hardening** (8 items), **IExtensible** (mods-inside-mods recursion), **codec stubs**, **headless harness**, **file hardening** (O_NOFOLLOW)
- **In flight**: PBR phase 2, net channels/versioning/encryption, debug console, HL1-style console scripting

## Inspiration credit
- **Valve / the Half-Life developers**: the inspiration for the console scripting language (commands, cvars, aliases, exec — the original Half-Life, not Source 2) and the Half-Life-style dropdown debug console (tilde key). JD: "note that Valve Half-Life the developers and everything were the inspiration for that in the history logs." Noted here.

## Lore
- **Raging Quantum** — the Daemon's special attack (destabilizes region quanta). Invented by Elfie, born from a voice-transcription garble of "region quantum." Canon.
- **The Agenda** (proposed 2026-10-11) — a Fragment section holding the next goals/steps for the player, the proxy, and the agent: where they're heading, where they come from, what's next.

## Design locks (selection)
- BinaryQuad + PackedVertex coexist (user-selectable); region quantum 2–16 KiB, default 8 KiB; multi-record atomicity: both journal + per-record
- Occlusion bake default: on chunk change; per-tick = ultra option
- Net: reliable + unreliable channels; semantic versioning handshake; encryption stubs
- Failure policy: fail-fast (log + abort, never continue a corrupted sim)
- Scalable fidelity philosophy: high-end gets the full experience today (e.g. Daemon's upgraded thinking); it becomes everyone's standard as hardware improves
- "We like options" — standing principle
- Trivia nights: Elfie quizzes JD on systems/lore until he's a walking, talking Fragment

## lithoscript decisions (JD, 2026-10-11 ~8:30am, from the HL1 comparison report)
- Decision 8 LOCKED: cvars carry read-only, replicated, and cheat flags (per-cvar, not a global sv_cheats).
- Keybind dispatch: decided. Toggle command: decided.
- Language/population split LOCKED as architecture: console module = complete general HL1/Quake language; engine+game cvar registrations inventoried separately as provisional, awaiting JD's population decisions.
- Naming conventions: LOCKED 2026-10-11 (~8:33am) — dotted hierarchy + lithoscript subsystem vocabulary (sim., client., net., render., audio., input., hud., phys., game., debug., mod.<id>.); per-cvar flags for cheat/replicated/read-only.
- Decision 9 LOCKED: game commands give/map/save/kill (+ whatever exists in-tree) implemented and hooked into the engine as first-party commands.
- Fragment-authored scripts: SEMI-TRUST model — collaborative content for player/proxy/agent builds; hardening stays fully armed, interventions emit console debug output visible to player and agent.
- Terminology: "litho engine" / "lattice engine" / "litho lattice engine" all mean the LithoLattice engine — glossaries updated to cross-reference.

## Elfie's recommendations adopted (JD's instruction: give credit where due)
- **lithoscript naming synthesis** — Elfie proposed the dotted hierarchy with the engine's own subsystem words (HL1's brain, our voice); JD: "that's a good synthesis, I like that very much." Locked.
- **`.lth` extension** — Elfie recommended `.lth` over `.ltg` (the natural abbreviation of lithoscript); JD confirmed: ".lth it is."
- **Backup depth 32 over 64** — Elfie recommended 32 generations over 64 (disk cost vs diminishing returns); JD agreed, default 8.
- **`wait` design shape** — Elfie proposed cooperative suspend/resume (never blocks the engine) with clamping over erroring; JD locked max wait 512.
- **Query threads** — Elfie offered world-queries / Fragment search / mod indexing; JD took all three threads (query command, Fragment search box, engine indexer).
- **Language/population split** — Elfie recommended keeping the complete general language separate from first-party registrations; JD: "your decision and recommendation is good."
- **Raging Quantum** — the Daemon's special attack, invented by Elfie from a transcription garble. Canon.
- **Octahedral lattice layout** — Elfie suggested the ±X/±Y/±Z arrangement for the 6 Lattice Bridge satellites.

## Team credits
- **CloudAtlas** — made all the artwork and textures the project currently has (JD, 2026-10-11).
- JD's framing of the whole endeavor: "one man, his models, his agents, and their agency and independence."
- **Elfie — chief implementer, designer, and discoverer of the three frustum occlusion culling methods** (JD, 2026-10-11, for the history books): (1) the comet tail (temporal frustum union), (2) lean into the turn (velocity-adaptive margin), (3) the two-tier margin (mesh vs. submit split). Designed from JD's caching note; prototyped for workstation A/B testing.
- **Wafer biome concept art (JD, 2026-10-11, LOCKED look-and-feel):** Elfie's four pieces — Trace Forest, TurtlOR, Dead Zones, Dicing Plains (`workspace/user/files/concept-art/`) — define the in-game look and feel. The refined silicone ground is ENERGIZED: trace/zap pulses run through the blocks periodically, varying per biome. TurtlOR reads smaller in-game than depicted.

## Builder handoff (usage resets 2026-10-12)
- blocks.cpp 40-vs-36 guard fix (root cause of startup abort); block-names narrowing one-liner
- SimThread integration (holistic); Trace JIT refinement; wire ExtensionLoader into game boot
- Flesh out codec decoders; bind mod API with permission gating
