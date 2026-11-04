# First Day on the Wafer — 2026-10-12

**End state: build clean · selftest 840 PASS / 7 FAIL (was 830 / 9) · ctest 34/38 · Trace proven green · both wafer worldgen FAILs fixed.**

> Baseline note, honestly: the task brief quoted "ctest 32/33, selftest 4 FAILs" — that predates the tree growing to **38 ctest entries**. Every worker measured the live baseline before touching anything (ctest 34/38 with a flaky audio test, selftest 830 PASS / 9 FAIL) and the final state matches or betters it everywhere. Remaining failures are itemized in §6 — all pre-existing, none caused by this work.

---

## 1. What the Wafer looks like (before)

Frames are from `biome_capture` (real worldgen + real mesher + software renderer, untextured white geometry — they prove shape and wiring, not color). Before-frames are the seed-20261011 world; after the seed fix (§2) the canonical world is seed 0, so after-frame locations legitimately differ.

![spawn before](sandbox://workspace/user/files/wafer-frames/before/spawn.png)

**The five biomes** (climate-selected, blended edges, all surfaces are a 24-block crust over void — see §5):

| Biome | Ground | Character (before-frames) |
|---|---|---|
| **Trace Forest** | silicon | Rolling fbm hills; trace trees (trunk/branch/tips), the densest tree cover — [frame](sandbox://workspace/user/files/wafer-frames/before/biome-trace_forest.png) |
| **Dicing Plains** | scribe silicon | Dead flat (height constant 254), dicing-grid ground, scattered trees/cans — [frame](sandbox://workspace/user/files/wafer-frames/before/biome-dicing_plains.png) |
| **Capacitor Groves** | dense silicon | Gentle bumps; capacitor cans everywhere (1,245 in the sample region) — [frame](sandbox://workspace/user/files/wafer-frames/before/biome-capacitor_groves.png) |
| **Resonator Spires** | flecked silicon | Ridged heightfield up to y≈270 + crystal spires 8–16 tall — [frame](sandbox://workspace/user/files/wafer-frames/before/biome-resonator_spires.png) |
| **Dead Zones** | dead silicon | Low, cracked, still — no cans, no pulse, sparse trees — [frame](sandbox://workspace/user/files/wafer-frames/before/biome-dead_zones.png) |

Top-down: [overview map (before)](sandbox://workspace/user/files/wafer-frames/before/wafer_overview_map.png). **Spawn (0,0) was in Capacitor Groves** — not Trace Forest.

Texture reality check (full inventory done): the biome "stub" textures are **not** flat placeholders — every ground/bark/canopy PNG is fully painted provisional art awaiting the CloudAtlas pass. The genuinely broken textures were specific (§3).

---

## 2. The two wafer worldgen failures — root causes and fixes

### (a) "surface near layer 256, only the crust is generated" — **stale test bound, not a worldgen bug**
The check allowed ≤ 3 allocated sections/column (36×3 = 108); the world produces 110. Recon measured exactly why: **ResonatorSpireFeature** places crystals 8–16 tall (`stages.cpp:888-909`, `wafer.json` `resonator_spires_feat`), and on 4 columns the tips at y272–275 legitimately allocate a 4th section, +1 section from the demo circuit. The features were wired into wafer.json on 2026-10-11, after the check was written.
**Fix (test-side, no worldgen change):** the check now counts crust sections (`sy ≤ 16`) against 36×3 and allows total sections ≤ 36×4 for feature spires (`selftest.cpp` ~lines 78, 120). **PASS** at surfaceSy=17, subs=110.
*Elfie choice JD can override:* I fixed the check rather than capping spire height in wafer.json — shrinking a biome's look to satisfy bookkeeping seemed wrong.

### (d) "wafer is seed-independent" — **real code-vs-design bug, fixed**
`dimensions.cpp` `makeGenerator()` passed the world seed into **both** dimensions, so every wafer noise field re-rolled per seed (measured: 1,680 differing cells in one chunk; the JSON "seeds" are salts, not absolutes — `graph.cpp:188-191`). The README documents the opposite: *"--seed n sets the world seed (Organic…; the wafer is seed-independent)"* (README.md:133-134).
**Fix (game wiring only, engine untouched):** the wafer generator is pinned to seed 0 on both the pack and built-in paths (`dimensions.cpp` ~lines 39–48); Organic keeps the world seed. Engine seeding was deliberately left alone — `lithokraft_scout_selftest` pins the direct-API wafer at seed 12345. **PASS.**
**Flagged consequence:** in existing `--seed` worlds, unsaved wafer columns regenerate as the canonical wafer. That is the documented design, but it is a visible change for such saves.

### Bonus fix — the pack generator was being silently rejected
Every capture run logged `wafer generator from packs is invalid (features: duplicate feature name 'trace_trees'); using the built-in`. Root cause was **not** in wafer.json (single source, embedded as the built-in too): `biome_capture.cpp` was injecting a second, stale copy of `trace_trees` + `capacitor_clusters` — a leftover from before the features were wired into wafer.json. The injection is removed; the tool now passes wafer.json through as-is, the pack generator validates and is used, warning gone.

---

## 3. Textures generated

Procedural Python+PIL, rendered natively at each size (no downscale shortcuts), straight alpha, semiconductor aesthetic (purple/orange accents).

| Texture | Packs | What changed |
|---|---|---|
| `silica_grass` | 64/32/16 | **Was broken**: alpha 255 everywhere, ~75% white → rendered as white crossed planes in-game. Now a real silicon-fern sprite on transparency (65% transparent @64, **zero** white pixels) |
| `trace_tip_red` | 64/32/16 | **New.** Red glow orb (white-orange core → maroon). Red tips previously shared the blue tip's texture and rendered blue (7 red px vs 1,799 blue). Wired via one line in `blocks.cpp` (texture key split; save-safe — keys are independent of block IDs) |
| `chipor`, `portor`, `fusor`, `structure_block` | 64/32 | **New.** These had no texture and no model — magenta checker. All four now have readable die/port/breaker/frame art; models 53 → 58; all "magenta checker" warnings **gone** |
| `silicon` | 16 | **New** 16×16 render, so the commonest Wafer block isn't missing from the agent-renderer mip chain |
| icons: chipor, chipor_viewport, silica_grass, tilled_silicon, silica_tilled | 64/32 | **New** hotbar icons; missing-icon count 19 → 15 |

Resources now load **180 textures, 0 missing, 58 models**. Everything else in the packs was left as-is — the inventory proved the "stub" art is real painted art, and repainting it wholesale would have been churn, not progress.
*Elfie choice JD can override:* the red-tip wiring edits one line of `blocks.cpp`; it shipped only because build + full tests stayed green.

---

## 4. Trace — launch-critical, proven

**No Trace failure exists.** The signal suite was already strong (110 checks); the three launch behaviors were only implicitly pinned, so explicit checks were added. Signal suite now **117/117 PASS**; engine signal tests **39/39**; headless real-game harness **34/34**.

New checks (`signal_selftest.cpp`, section Q — all PASS on first run, no sim code touched):

| Check | Proves |
|---|---|
| TRACE-A1 / A2 | Red run driven 64→55 beside a parallel blue run: **every** opposite-color cell stays 0 — both directions, including a trace touching the other color's torch |
| TRACE-B1 / B2 | Decay is **exactly** 1 per block: 64@1, 63@2, 60@5, 55@10, 32@33, 25@40 — identical on red and blue |
| TRACE-C1 / C2 | Torch → 6 trace → lamp lights; first trace = 64 and **no cell exceeds 64** |
| TRACE-C3 | Breaking the run's last trace kills its lamp live; the other color's lamp stays lit (propagation is live, not latched) |

Already covered before today (all PASS): torches, stairs/cuts, vias (lossless, color-separated), repeater (re-boost 62→64 cross-color, one-way, delays), Thresholdor, Redirector, Memor, **Pulsor** (no-decay pulse, Memor-stored pulse, rising-edge), **CatchOR** (auto-wiring, output cycling, split-lane feeds), FastLatch, switch/button, Keyboard/SIXBIT, color lamps, Tone Block, wireless/jammer/radio, 1,500-tick incremental-vs-full-flood equivalence, and the spawn demo circuit running in the real generated world (9 plain lamps 7 lit; 32-lamp wall + 35 color lamps 33 lit at launch).

### Hands-on loop — how far automation actually goes

| Proven by automation | Only a human at a keyboard can confirm |
|---|---|
| Real Game boots headless on the Wafer, menu → Play, 600 frames (34/34) | Break/place **through the real player path** at the demo circuit — no headless route exists: Game exposes no public sim access, and the MCP pilot's stdio server is **not wired in `main.cpp`** (builder-owned) |
| Spawn resolves to air above the surface at its column (new check) | Render look/feel: trace glow, red-vs-blue legibility on a real GPU (this VM has none) |
| Place/break blocks at sim level (`ChunkMap::set` — the same calls the real edit drain makes) + placement rules (wire support, canPlace veto, ray-targeted place via SimThread) | Input latency, first-person readability of the demo rows, lamp readability at distance |
| Propagation in the **real generated world** (demo circuit lamps lit at launch) | Sound in the room (SFX *logic* is tested; perception isn't) |

**Known friction for the first minutes:** game spawn is the demo-circuit spawn, and it floats ~11 blocks above the demo platform — outside the 8-block reach. You'll need to drop/fly down to it. Flagged, not hidden; fixing spawn placement is a small game-code call for JD/builder.

---

## 5. First day on the Wafer — after-state

![spawn after](sandbox://workspace/user/files/wafer-frames/after/spawn.png)

Canonical seed-0 wafer, spawn column (0,0): **Capacitor Groves, surface Y=257** — and **Trace Forest's center is 9 blocks away** at (−5,−7). Within 48 blocks of spawn: **1,379 trace trunks, 1,097 branches, 983 glowing tips, 2,203 capacitor cans, 1,154 resonator crystals**. The decided opening loop — punch trees → logs → planks → sticks → ChopOR — is physically present in overwhelming quantity, plus the demo circuit at spawn with live lamps. Full after-frames: [trace_forest](sandbox://workspace/user/files/wafer-frames/after/biome-trace_forest.png) · [dicing_plains](sandbox://workspace/user/files/wafer-frames/after/biome-dicing_plains.png) · [capacitor_groves](sandbox://workspace/user/files/wafer-frames/after/biome-capacitor_groves.png) · [resonator_spires](sandbox://workspace/user/files/wafer-frames/after/biome-resonator_spires.png) · [dead_zones](sandbox://workspace/user/files/wafer-frames/after/biome-dead_zones.png) · [overview map](sandbox://workspace/user/files/wafer-frames/after/wafer_overview_map.png)

### Wired and working ✅
- 5 biomes generating and blending; per-biome surface palettes (scribe/dense/flecked/dead) applied
- Trace trees, capacitor cans, resonator spires placing — densely, including at spawn
- Surface ruins as structures: `transformer_ruin`, `dead_memory_bank`, `scribe_monument` blueprints (surface-anchored)
- Trace signal sim end-to-end (117 checks) + demo circuit live in the real world
- Silica **growth/spread/farm loop** — wired in the real sim, `lithokraft_silica_selftest` fully green
- Ground pulse: cvar registered and fed to the renderer every frame
- Pack worldgen generator validates and is actually used (no silent built-in fallback)

### What remains ❌ (gaps, precisely)
- **The underneath is empty.** Under the 24-block crust (spawn column: silicon y234–257) is **void to y0** — no carvers/caves in wafer.json (Organic has worm caves; the Wafer has none), no underground ruins layer, no deep strata, **no Substrate block**. Digging through the crust = falling into void. Fossil traces / broken vias / dead memory cells exist **only inside surface ruin blueprints**. The crust-only shape is asserted by the selftest, so filling the deep is a design decision (JD/builder), not a wiring fix. *Designed-but-unbuilt:* JD's **Substrate** base layer belongs here.
- **Silica doesn't generate.** Its scatter helpers (`silica_biome.cpp`) have no caller outside their selftest; wafer.json has no silica feature; spawn probe counted **0** silica grass. Today silica exists only after a player tills silicon and plants. One feature-wiring job for the builder.
- **Shrubs don't generate.** Engine shrub system + per-biome game data (`biome_vegetation.h`) exist; nothing calls them in worldgen; **0** shrubs placed.
- **No mobs on the Wafer.** Engine AI (`AiWorld`) exists but is instantiated only in a perf benchmark with generic wander mobs. No Wafer mob definitions, no spawn wiring in dimensions/game — and **"grimlore" has zero hits in the entire repo**. Per mandate, nothing was invented; the spawn path and content are both builder/JD work.
- **Ground traces can't be toggled.** Engine hook exists and is permanently on; there is no `render.ground_traces` game cvar (needs a settings field + cvar row + FrameParams assignment, 3 files).
- **Phosphoric acid → silicon nitride: designed today, unbuilt.** The engine has only the fluid *interface* (`FluidType` incl. `kChemical`, levels 1–16, `IFluidSim`); the game has no fluid sim implementation, no fluid block IDs, no acid pool feature, no conversion rule. "phosphor"/"nitride" have zero hits in source.
- **MCP stdio server not wired in `main.cpp`** — the pilot exists; agents can't drive the real game until the builder wires it.

*Elfie freestyle calls, all overridable:* spawn biome left as Capacitor Groves (not hostile; forest is 9 blocks away and trees are dense at spawn, so no climate nudge); no blocks added; no underground redesign attempted.

---

## 6. Test state and remaining failures

- **Build:** clean, exit 0.
- **`LithoKraft --selftest`: 840 PASS / 7 FAIL** (baseline 830 / 9; +8 new checks, +2 worldgen fixes). Remaining 7, all pre-existing: save round-trip ×4, mesher sub-chunk quads ×1, Vulkan headless ×1 (environmental — no GPU on the VM), hotbar icons ×1 (15 icons still missing for out-of-scope blocks).
- **ctest: 34/38.** Failing: `lithokraft_selftest` (above), `lithokraft_scout_selftest` (ModelResolver not supplied by its own `makeWaferGen` — one-line-class fix identified: pass `model::makeLithoStampModelResolver()` as `dimensions.cpp` does; scout's file, builder's), `lithokraft_chipor_selftest` (stale registry-size guard, pre-existing), `engine_audio` (**flaky** — passes standalone, verified twice).

---

## 7. JD hands-on checklist — your first 10 minutes

1. **Launch and spawn in.** You appear at the demo-circuit spawn, floating ~11 blocks above the platform — drop/fly down to it. (Known friction, §4.)
2. **Look around.** Capacitor grove: cans and trace trees everywhere; Trace Forest is ~9 blocks from world origin. Red tips are red now; blue tips blue.
3. **Punch a trace tree** → logs → planks → sticks → craft a **ChopOR**. The opening loop, live.
4. **Walk the demo circuit rows.** Lamps lit at launch, red and blue runs side by side, the 32-lamp wall and color lamps.
5. **Break a red trace** in a lit row — the lamp dies. **Replace it** — it relights. That's live propagation under your fingers.
6. **Build your own run:** torch → long red trace → lamp. Watch it decay over distance; lay blue trace alongside red and confirm the blue stays dark until its own torch feeds it.
7. **Play the components:** Pulsor pulse into a Memor, cycle a repeater's delay, feed a CatchOR — all present in the demo rows.
8. **Know the edges before you hit them:** no silica at spawn yet (till silicon + plant seeds to try the farm loop), no shrubs or mobs yet, and **don't dig straight down** — below the crust is void for now (§5).

---

## 8. File-level changelog

**Engine/game code**
- `game/LithoKraft/src/game/dimensions.cpp` — wafer seed pinned to 0 in `makeGenerator` (both paths), comment citing README.
- `game/LithoKraft/src/app/biome_capture.cpp` — stale duplicate feature injection removed; pack wafer.json used as-is (sha 6bdc6440… → a4b64dff…). (A temporary spawn-audit probe was added and fully removed; final file verified byte-for-byte.)
- `game/LithoKraft/src/content/blocks.cpp` — 1 line: `trace_tip_red` texture key `"trace_tip"` → `"trace_tip_red"`.
- `game/LithoKraft/src/app/selftest.cpp` — crust check restated (`crustSubs` counter; feature-spire allowance); spawn result captured + spawn-resolution check added.
- `game/LithoKraft/src/content/signal_selftest.cpp` — section Q appended: 7 TRACE checks (A1/A2, B1/B2, C1/C2/C3).

**Textures (all under `game/LithoKraft/assets/packs/`)**
- Replaced: `pack_{64,32,16}/textures/blocks/silica_grass.png`.
- Created (22 PNGs): `trace_tip_red.png` ×3 packs; `chipor/portor/fusor/structure_block.png` ×2 packs (64/32); `pack_16/.../silicon.png`; icons `chipor, chipor_viewport, silica_grass, tilled_silicon, silica_tilled` ×2 packs (64/32).
- Regenerated: `manifest.json` + `resources.json` in all 3 packs via `engine/tools/convert_pack_manifest.py` (models 53→58 in 64/32; 12→14 in 16).

**Frames**
- `~/workspace/user/files/wafer-frames/before/` — 7 frames (seed-20261011 world).
- `~/workspace/user/files/wafer-frames/after/` — 7 frames (canonical seed-0 world).

**Untouched per guardrails:** sim core/threading, save format, network protocol, `trace_jit_basic.cpp`, the extern-const block registry system (one texture-key string only), wafer.json itself. No blocks added.

**For the builder (5:07 PM):** scout ModelResolver one-liner · chipor stale registry guard · 4× save round-trip FAILs · mesher sub-chunk FAIL · silica + shrub worldgen features · Wafer mob content + spawn wiring (Grimlores undefined anywhere in-repo) · the deep-wafer decision (Substrate/ruins layer/acid pools) · `render.ground_traces` cvar wiring · MCP stdio server in `main.cpp` · spawn-height fix (spawn floats ~11 blocks above the demo platform) · 15 remaining block icons.
