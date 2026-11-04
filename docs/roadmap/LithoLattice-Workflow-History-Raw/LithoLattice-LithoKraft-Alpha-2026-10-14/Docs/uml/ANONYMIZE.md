# Anonymization flag list — UML diagrams

> **INTERNAL — DO NOT SHARE.** This file enumerates every sensitive item
> in the diagrams and is more revealing than the diagrams themselves.
> Scrub the diagrams using it, then keep it private.

## JD decisions (2026-10-11) — playtester package

Audience: possible playtester, on the fence, strong at redstone/logic systems.
- **Scrub story + lore progression** (items 1, 2, 3) — DONE in `lithokraft-game.dot`, PNG re-rendered.
- **Keep product identity names** (B: LithoKraft, LithoLattice, .lthm) — playtester gets the real names.
- **Keep MCP/agent-playability** (item 18) — selling point for a technical person.
- **Keep everything else** (dimensions, mechanics, tuning, -OR family, LBT, Trace names) — code fidelity for a technical reviewer.
- C (personal identifiers) and D (dates): already clean, nothing to do.

Review of all three diagrams (`engine-architecture`, `engine-procgen`,
`lithokraft-game` — PNG + DOT) for anything that should be scrubbed or
renamed before sharing externally. You decide what actually gets cut;
this list exists so nothing slips past.

**Flag counts:** A. Game design / lore IP — 18 · B. Product identity names — 3 · C. Personal identifiers — 0 found · D. Dates — 0 remaining (verified clean)

---

## A. Game design / lore IP (17)

### Story beats & progression (lithokraft-game.png)
1. **The Spark note** — *"grants upgrade #1, markdown unlock → tier 1"*
   Reveals: a named story character AND the exact first-upgrade reward (markdown) AND the tier system.
   Suggest: `External NPC — grants Fragment upgrade` (or drop the note).
2. **The Oracle note** — *"grants upgrade #2 (Organic), Underwafer checklist → tier 2"*
   Reveals: a second named character, her location (Organic), and the exact tier-2 reward (Underwafer checklist).
   Suggest: `External NPC — grants Fragment upgrade` (or drop the note).
3. **FragmentState box** — `tier() : uint8`, `markdown_unlocked()`, `upgradeFragment()`, `LFS1 v2, 8 bytes`
   Reveals: the signature progression system (tiers, markdown gating) plus save-format internals.
   Suggest: rename box to `CodexState` / `JournalState` with generic `upgrade()`; drop the byte-format line.

### Unreleased mechanics & mobs (lithokraft-game.png)
4. **Grimlores note** — *"hostile mob family (Box: reprogram/recompile/reconfigure — planned)"*
   Reveals: an unreleased mob family AND its taming mechanic AND the canonical taming vocabulary.
   Suggest: `Hostile mob family` — cut everything about the Box.
5. **EtchOR → ChipOR** — *"etching core / name field"*, *"wafer in → chip out"*, *"printed chip (name → LBT field)"*
   Reveals: the core lithography crafting loop and the name-field mechanic.
   Suggest: `Fabricator` → `Output item` with generic `input → output`.
6. **"-OR family" label** (components catalog) — *"— -OR family —"*
   Reveals: the game-wide naming convention (a deliberate signature).
   Suggest: `Component catalog`.
7. **silica box** — *"silica grass/farming simulation"*
   Reveals: unreleased farming content.
   Suggest: `Farming simulation`.
8. **LBT notes** (lithokraft-game.png AND engine-architecture.png) — *"LithoBinary Tagged (game NBT-style tagged format)"*, *"chip name → LBT field"*
   Reveals: the proprietary tagged-format name and where chip names persist.
   Suggest: `Tagged binary format` / `name field`.

### Dimension & world design (lithokraft-game.png)
9. **dimensions box** — *"Wafer / Organic / Underwafer / Void / Space"*
   Reveals: the full dimension stack by name.
   Suggest: `Dimension A / B / C / D / E` or just `dimensions`.

### Content design & tuning (engine-procgen.png)
10. **TraceTreeFeature** — *"circuit trees"*
    Reveals: a signature biome feature. Suggest: `TreeFeature`.
11. **CapacitorCanFeature** — *"dense can clusters"*
    Reveals: biome content design. Suggest: `ClusterFeature`.
12. **ResonatorSpireFeature** — *"8-16 block spikes"*
    Reveals: content design plus exact tuning numbers. Suggest: `SpireFeature`.
13. **StructureFeature** — *"transformer ruins, scribe monuments"*
    Reveals: lore-flavored ruin content. Suggest: `RuinFeature`.
14. **RuinsCarver** — *"fossil traces, broken vias, delamination"*
    Reveals: underground lore content (broken circuitry). Suggest: `RuinsCarver` (name only).
15. **OceanFillRule / LakeFillRule / water note** — *"max depth 196 (kMaxOceanDepth)"*, *"max depth 98 (kMaxLakeDepth)"*, *">=60 solids below max ocean (256 - 196, arithmetic guarantee)"*
    Reveals: exact world-gen tuning constants and the derived safety invariant.
    Suggest: `max depth N` (redact numbers) — or keep if tuning isn't sensitive.
16. **DungeonSlot** — *"dungeon placement"*
    Reveals: planned dungeon structures. Suggest: cut the box.

### Engine lore-tied names (engine-architecture.png)
17. **ISignalNode / ITraceJit** — *"Trace signal graph"*, *"Trace JIT seam"*
    Reveals: the Trace — the simulation's core in-fiction signal layer — by name.
    Note: these are real code class names; renaming the labels breaks code-fidelity. Flagging so you can decide.

### Agent-playability (lithokraft-game.png + engine-architecture.png)
18. **MCP boxes** — `McpPlayerController`, `McpSensesAdapter`, `IPlayerController`, `IVisionCapture`
    Reveals: the game is built to be played/seen by AI agents (controller + vision-capture interfaces) — a competitive design detail.
    Suggest: `External controller interface` / cut.

---

## B. Product identity names (5)

19. **"LithoKraft"** — diagram titles, box labels (`LithoKraftAppearance`), folder context (all three diagrams).
    Suggest: `[GAME]` or a codename.
20. **"LithoLattice"** — diagram titles (*"on the LithoLattice Engine"*), engine cluster label (all three diagrams).
    Suggest: `[ENGINE]`.
21. **".lthm v1 pipeline"** (lithokraft-game.png, model/ box)
    Reveals: the proprietary model-format name. Suggest: `Model pipeline`.
22. **"GameWorldQuery"** — contains no IP beyond the generic; fine as-is. *(Not a flag — listed to avoid false positives.)*
23. **Class names generally** (`SparseOctree`, `NoiseGraph`, `SandboxedFile`, …) — these are descriptive engineering terms, not IP. Safe.

## C. Personal identifiers (0 found)

- No names, usernames, emails, or machine paths appear in any diagram content. DOT source comments reference the tree with `~` only.
- PNG metadata verified chunk-by-chunk: only `IHDR`/`bKGD`/`IDAT`/`IEND` — no `tEXt`/`iTXt`/`tIME` chunks, no author or timestamp data.
- Minor: filesystem mtimes on the files record creation time. If sharing raw files (not just the images), `touch` them or zip first.

## D. Dates (0 remaining — verified clean)

- All `2026-10-11` strings removed from DOT sources (titles, header comments, the "decided" note) and re-rendered.
- README rewritten with no dates and no lore-name leaks.
- Verified via grep across all sources; PNG metadata has no date chunks (see C).

---

## Corrected flag counts

- **A. Game design / lore IP: 18** (items 1–18)
- **B. Product identity names: 3** (items 19, 20, 21 — item 22–23 are explicit non-flags)
- **C. Personal identifiers: 0**
- **D. Dates: 0 remaining**
- **Total actionable flags: 21**

## Suggested scrub order (highest value first)

1. Story beats: items 1, 2, 3 (Spark/Oracle/Fragment tiers — the progression design)
2. Product names: items 19, 20, 21 (LithoKraft / LithoLattice / .lthm)
3. Unreleased mechanics: items 4, 5, 8 (Grimlores+Box, EtchOR→ChipOR, LBT)
4. Dimensions: item 9
5. Content/tuning: items 6, 7, 10–16, 18
6. Judgment calls: item 17 (Trace names — code-fidelity vs. lore secrecy)
