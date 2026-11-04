# UML Update Notes — 2026-10-13 set

New set (this directory): `engine-architecture-2026-10-13`, `engine-procgen-2026-10-13`,
`lithokraft-game-2026-10-13` (`.dot` + `.png`). The 2026-10-11 files are untouched.
Derived from the current production tree (`~/workspace/litholattice`, post Timing
Handshake merge + post fix program), not copied from the old diagrams' claims.

**Rendering:** graphviz 2.43, `-Gdpi=300` native renders (engine 11996×18146,
procgen 11250×9067, game 26962×3458), delivered PNGs downscaled (Lanczos) to
phone/desktop-friendly widths — engine **7000×10589**, procgen **7000×5642**,
game **12000×1539**. Legibility verified by inspecting cropped regions of the
delivered files. Re-render the `.dot` at any dpi for full native resolution.

**Convention (new this set):** solid box/edge = implemented and wired;
**dashed box = declared / stub / not built**; each diagram carries the legend.
Anonymization per `ANONYMIZE.md` / JD's standing decision: story/lore
progression only is genericized (no Spark/Oracle/Fragment-tier internals);
product names, MCP, dimensions, mechanics, tuning, -OR family, LBT, Trace
names are kept.

---

## engine-architecture — what changed vs 2026-10-11

- **Timing Handshake is its own cluster now** (was: nothing — the algorithm
  didn't exist in-tree). TimingHandshakeAuthority owned by SimThread, pull
  workers polling `pollPhase()`, phase barrier, WAIT (overrun accounting,
  cycle-budget offender flags) and FREEZE (250 ms watchdog → the debugger's
  pause path + SimCorruptionReport). Marked **DEFAULT OFF** (cvar
  `sim.timing_handshake`, CLI `--timing-handshake`) — wired but opt-in.
- **SimThread grew up**: `stepTicks(n)` advances exact world ticks; random
  streams no longer replay after stop/start (worldTick continuity); ClockDomain
  power-of-two gears are drawn explicitly.
- **Whole new subsystems that had no boxes on 10-11** (all real CMake targets):
  camera (ICamera/Camera/CameraManager, 5 modes), player (IPlayer, vitals vs
  stats registries — xp is a stat), sprite (registry + deterministic
  SpriteSystem + runtime atlas), texture (`.ltxr` recipe generator + LRU
  cache), projectile (now a *real library* — ProjectileManager spawns/ticks
  and loads chunks ahead; on 10-11 it was header-only with zero callers),
  ai, systerian, ui toolkit, anim, index, defrag, crypto.
- **Renderer truth updated**: SoftwareRenderer is now a *full* IRenderer
  (real PBR path, frustum culling, CPU HUD rasterizer) and the default;
  SoftwarePresenter and FramePacer drawn. Ground traces render (SW + GL33;
  VK compile-verified only). TraceStrengthBuffer drawn with its pending-list
  design. **Vulkan sprite pass is a dashed box** — it does not exist.
- **Storage hardened**: the file contract (atomic write + fsync + quarantine),
  `block_names` with backup/recovery and **fail-closed** load — the 10-11
  diagram's plain "backs/scheduled by" edges hid what was a critical finding.
- **Console security drawn**: alias/keybind permission masks captured at
  definition time (the anti-laundering fix), step/wait budgets.
- **Crypto/signing drawn**: canonicalModHash's length-prefixed framing
  (signatures can't be transplanted between mods), Ed25519 verify fail-closed.
- **Net drawn honestly**: codec + ConnectHandshake state machine, and a
  dashed **"socket transport — NOT BUILT"** box. The 10-11 "platform services"
  net box implied a working transport; there is zero socket code in
  `engine/src/net`.
- **AI cluster is dashed-outlined**: AiWorld/Brain/Nav/Senses are a complete,
  selftested library with **no live-game integration** (see callouts).

## engine-procgen — what changed vs 2026-10-11

- **Worldgen is data-driven now**: `loadGenerator` (generator JSON → pipeline)
  added; StageContext + ColumnMaps replace the old per-column buffer drawing.
- **New stage interface**: IBiomePostStage + the diversity stages (ground
  cover, rock formations, transition bands, Worley ore veins, fauna hooks,
  weather surface rules) — all JSON-driven, position-pure.
- **Litho Formula cluster added** (superformula sampler, voxelizer, gallery)
  with its feeds into shrub generation; `generateShrubSprite` added (seed
  varies colour only, never the mask); ShrubParams gained the `super*` fields.
- **Structures are a system now**: StructureRegistry + StructureFeature
  placing named blueprints, LithoStampFeature stamping `.lthm` octree models
  into chunks, structure-block capture on the game side.
- Carvers, features, surface rules and tuning numbers otherwise verified
  unchanged (OceanFillRule 196 / LakeFillRule 98 kept, per JD's decision).

## lithokraft-game — what changed vs 2026-10-11

- **World/save cluster rewritten**: ChunkStreamer (disk-first via
  `WorldSave::loadColumn`, insert-only), WorldSave on the file contract,
  `level.json` seed persistence, **player.dat v4** — PlayerMeta carries
  separate vitals/stats blocks and the **persisted 42-slot Inventory**
  (the 10-12 state had inventory unpersisted and the HUD on a separate
  BlockId bar; the HUD now reads real ItemStacks).
- **Dimensions corrected**: only **Wafer and Organic are registered**.
  Cosmic / Underwafer / Void are a dashed box (not registered, no worldgen
  JSON) — the 10-11 diagram listed five dimensions as if present.
- **Signal cluster completed**: SignalSim draws its streamed-column indexing
  and the import-time strength clamp; components catalog lists the full
  -OR behavior set; blocks registry shows string-resolved ids +
  `registerBlock()` mod API + the still-DISABLED LithographOR row.
- **Player cluster completed**: PlayerAdapter (: IPlayer; xp + static +
  kharge as stats), CameraController (engine CameraManager; FP at eye
  height), PlayerLocomotion, SpriteHost (incl. Systerian glyph sprites),
  MCP pilots drawn as engine cameras in the same manager.
- **Fragment redrawn honestly**: the item is solid (granted slot 0,
  persisted, icon), but FragmentState is dashed (**zero production
  callers**) and the **Fragment book UI is a dashed NOT BUILT box**.
  The 10-11 diagram showed Game driving FragmentState; nothing does.
- **App cluster** now shows the real entry surface: `--renderer`,
  `--timing-handshake`, `--mcp-stdio` (MCP stdio is wired in main.cpp),
  selftest / handshake-test / perf / biome_capture modes.
- Certificate Table drawn with its **signing mechanic dashed (pending)**;
  per-item use handlers drawn as a dashed missing seam in content.

## Missing / stub callouts — with basis

| Callout | Where drawn | Basis |
|---|---|---|
| Fragment book UI (dialog/pages) not built | game, Fragment cluster | `game/LithoKraft/docs/FRAGMENT-UI-NEEDS.md` items 1–7; fix report deferred #5 (G-M6) |
| FragmentState has zero production callers | game, Fragment cluster | `fragment_state.h` NOTE (G-M6); grep: no production caller |
| Per-item use handlers absent (incl. SeedOR till) | game, content | FRAGMENT-UI-NEEDS item 4 (Appendix G H2b) |
| Column eviction / unloading not built | game, world cluster | Fix report deferred #1 (B-M1); `world/chunk_map.h` has no erase (grep-verified) |
| Cosmic / Underwafer / Void not registered | game, world cluster | `game/.../dimensions.h` header ledger (P2c, G-M5); fix report deferred #6 |
| Net socket transport not built | engine, net cluster | `engine/src/net/` contains no socket code (grep-verified); handshake offer is {3}-only, H2 interop is JD's decision (fix report deferred #8) |
| Vulkan sprite pass absent | engine, sprite/render | `render_vk/vk_renderer.cpp`: "Vulkan has no sprite pass (P1-RENDER H4)" + log-once warning when sprite quads arrive |
| AiWorld not integrated into the live game | engine, ai cluster | grep: the only game-side construction is `app/perf_headless.cpp` (bench harness) |
| FramePacer game-side wiring (`render.fps_cap`) pending | engine, render | Fix report deferred #3 (D-M3 remainder); engine pacer landed, default OFF |
| Certificate signing mechanic pending | game, machines | `components/certificate_table.cpp`: "when signing lands, this class grows a SimRole"; `blocks.h`: "signing station (block only; mechanic pending)" |
| No block sets `density` (projectile pierce reads it) | engine, projectile | Fix report deferred #10 |
| Mod-loader ownership enforcement open (F-M4) | engine, mods | Fix report deferred #4 — awaits a caller identity in the mod loader |
| Spatial audio / occlusion = scaffold, not live path | engine, platform | `audio/spatial_audio.h` + `occlusion_thread.h` exist; review flagged the occlusion concurrency item; not the game's live audio path |
| Timing Handshake default OFF (JD's call to enable) | both | `settings.h` `timingHandshake = false`; fix report deferred #11 |

## Structural surprises from the survey

1. **The engine doubled.** 30 CMake targets vs the 10-11 set's ~10 clusters —
   camera, player, sprite, texture, ai, systerian, ui, index, defrag, anim
   and crypto are all first-class modules now.
2. **The two "provided engine" boxes that lied by omission on 10-11**: net
   (no transport exists) and dimensions (3 of 5 core dimensions unregistered).
   Both are now drawn as they are.
3. **The Void has a representability problem**, documented in `dimensions.h`:
   its locked extent is infinite, but `packPos` keys pack 21 bits per
   horizontal axis — the Void cannot be registered until it gets a finite
   bound or a windowed coordinate scheme. Worth JD's eye before any Void work.
4. **FragmentState is an orphan** — a complete, selftested state class that
   nothing in production constructs. The Fragment's missing piece is purely
   the UI + one caller + a use-dispatch seam (needs-list is written).
5. **EtchOR has no block and ChipOR's block is deliberately inert**
   (`blocks.h`: "inert (no sim role)") — the lithography loop currently
   lives at items/blocks level; there is no machine behavior class.
6. **MCP is the only wired agent transport** (stdio, in `main.cpp`); the
   engine MCP library also carries an HTTP transport, and MCP pilot
   numbering already addresses dimensions (2/4/5/6/7) that don't exist yet.
7. **Streaming is insert-only by construction** — with no ChunkMap erase,
   no SignalSim un-index and no mesh-retire seam, eviction needs all three
   seams designed together (fix report B-M1 has the 5-step design).
