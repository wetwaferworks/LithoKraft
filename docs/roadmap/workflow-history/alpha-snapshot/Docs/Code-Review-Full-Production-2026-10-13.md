# Full Code Review — Production Repo, Top to Bottom — 2026-10-13

**Scope:** the entire production tree `~/workspace/litholattice` (~123k LOC,
engine + game), post-Timing-Handshake-merge. **Read-only review** — no
production file was modified. Eight subsystem reviewers worked in parallel;
every finding below is stated with file:line in the per-area appendices
(Appendix A–H, the reviewers' own full reports, appended verbatim).

**Headline: 4 critical · 25 high · 53 medium · 73 low · 4 info — 159 findings
as filed.** A handful are cross-area views of the same defect (tick-desync,
the save-comparator, the mesher test) and are cross-referenced, not hidden.
The prior fix program held up well: **no previously-fixed finding was found
re-broken in substance** — but the brand-new Timing Handshake, merged hours
before this review, carries 3 of the 4 criticals, and two 2026-10-11 findings
(ground traces, parts of the cvar-clamp sweep) are still open.

---

## 0. Baselines — what was run for this review

- `--handshake-test`: **OK** in this morning's merge verification and in the
  coordinator's own runs (quiet box). **FAILED one check** in the core
  reviewer's run under full review load — the WAIT offender-attribution
  check (finding CH-H4). The test is load-sensitive; both states are real.
- `--selftest` during the review window: **872–891 PASS / 6–7 FAIL** across
  four runs. The stable failing set is the known one (4× save round-trip,
  mesher sub-chunk, Vulkan environmental); a 7th — the audio-streaming
  worker-thread check — flakes in under load, and the PASS total itself
  varies run to run (a harness-determinism note in itself, see G-M7/SW-L8).
- `ctest` (coordinator's baseline run): **42/46** — failures
  lithokraft_selftest, scout, chipor (all known, root-caused below) plus
  engine_audio, the known load flake (43/46 in this morning's merge run;
  engine_audio passes solo).
- Reviewer spot suites: engine_core + engine_sim PASS; net/script/mods/
  console/mcp/world 6/6; texture/assets/crypto/systerian 4/4; entity-area
  suites 11/11 solo; storage|engine_sim 2/2.

---

## 1. CRITICAL (4)

### C-1 (Core/Handshake) — Workers re-execute the last phase after every stop()/start()
`timing_handshake.cpp:91-145` — `start()` never resets `publishSeq_` /
`shardDoneSeq_`; fresh workers begin with `seenSeq=0`, so the first thing a
restarted worker pool does is re-run the last published phase. **Repro-
verified** with a standalone build against the production libs. The trigger
is routine, not exotic: `Game::saveWorld()` and `pumpStreaming()` stop+start
the sim on **every save and every chunk-stream batch**. With the handshake
enabled, every save replays one phase of circuit work. Default is OFF today,
so this is latent — it arms the moment JD flips `sim.timing_handshake` on.
Fix: reset sequence state in `start()` (and generation-stamp publications).

### C-2 (Core/Handshake) — stop() detaches stuck workers; use-after-free against destroyed owners
`timing_handshake.cpp:102-125` — on timeout, `stop()` **detaches** workers
that keep dereferencing the authority, the SimThread, and the world after
those owners can be destroyed. The shipped freeze test itself only survives
via a 300 ms sleep workaround. This also undercuts the FREEZE promise
("frozen world stays savable"): `stop()` *is* the save path. Fix: no-detach
stop — bounded join with a real stuck-worker protocol; a worker that cannot
be joined must keep its owners alive (shared ownership), never raw refs.

### C-3 (Core/Handshake) — Pull workers have no exception boundary
`runItem()`/`workerLoop` contain no try/catch: a throwing circuit component
escapes the worker thread → `std::terminate`, no fail-fast report. This is
exactly the failure class the 2026-10-12 CRITICAL #1 fix eliminated on the
sim thread — the new threads reintroduced it. Fix: wrap the worker loop in
the same guard + `SimCorruptionReport` path as `tickGuard`.

### C-4 (Save) — `blocks.names` is a single point of total world loss
`block_names.cpp:11-34` — the block-name table (the file that makes every
saved block ID meaningful) is written with a hand-rolled tmp+rename: **no
fsync, no backups, no `recoverCrashedWrite`** — it bypasses the file
contract every other world file uses — and it is rewritten on *every* save.
On CRC failure, loading **fails open**: `loadDimension` returns 0
("generated terrain kept") and `loadColumn` refuses every column, so intact
region data is ignored and subsequent saves progressively overwrite it with
generated terrain. One torn write to a small file can therefore cascade
into total world loss — a direct hit on the WORLD PERMANENCE doctrine.
Fix: route through the standard atomic contract (fsync + backup rotation +
crash recovery) and make load **fail closed** (refuse to save over a world
whose name table is unreadable).

---

## 2. HIGH (25)

**Timing Handshake / core (5 more):**
- CH-H1 — `resetAfterFreeze()` doesn't quiesce in-flight workers: a stale
  barrier arrival can satisfy the *next* phase → component double-execution.
- CH-H2 — The publication tuple is read non-atomically: a worker can execute
  phase N+1's work under sequence N, then execute it again.
- CH-H3 — The FREEZE watchdog is blind to affine (sim-thread) hangs and can
  false-fire / misname the culprit when affine work exceeds the ceiling.
- CH-H4 — WAIT offender attribution is wall-clock based and **flags
  innocents**: the shipped test's "no filler flagged" check failed under
  load today (filler0/filler3 crossed the threshold on preemption time).
  Ties directly to sweep decision C2 (cycle-budget accounting) — the
  accounting needs to stop charging preemption to components before
  offender flags mean anything.
- CH-H5 — Tick-stepper defects confirmed STILL-OPEN: the stepped branch
  counts master steps (`stepTicks(16)` = 1 world tick at 512 Hz EUV /
  32 TPS) and omits `runRandomTicks`; zero stepTicks test coverage. The
  Morning Report's "same pipeline as live ticks" claim is **false**.
- CH-H8 — `worldTick`/`masterStep` reset to 0 on every stop()/start():
  keyed random-tick streams replay after every save/stream cycle.
  (CH-H6/H7: SimThread exception boundary still incomplete for game hooks —
  `applyEdits`/`takeEvents`/`inspect` outside `tickGuard`; game↔SimThread
  pause desync still open and *amplified* by handshake freeze: Esc-resume
  can unpause a frozen sim without `resetHandshakeAfterFreeze()`.)

**Signal / world / worldgen (3):**
- SW-H1 — Streamed, disk-loaded columns are **never indexed by SignalSim**;
  their SGNL circuit state is silently dropped. Circuits beyond the
  pre-bake square are dead on relaunch — the known streaming caveat, now
  code-verified and severity-rated. (Same-session play unaffected.)
- SW-H2 — Signal strength is **never clamped to 1..64 at the save/disk
  trust boundary**: values 65..255 index fixed bucket arrays out of bounds.
  A corrupt or crafted world file turns into memory corruption, not a
  clean load error.
- SW-H3 — A throwing streamer job **permanently stalls streaming** and
  hangs `~ChunkStreamer` (exit / dimension switch).

**Save / permanence (3 more):**
- SV-H1 — A failed region flush is **swallowed and unretryable**
  (`storage.cpp:275-295`): `pending_` is cleared before `writeFileAtomic`;
  on failure, later flushes early-return true, in-memory reads mask the
  loss, and `Game::saveWorld` still logs "on disk" and returns true.
- SV-H2 — **The world seed is never persisted** (`--seed` only, no
  level.json): dirty-only saving regenerates untouched columns from the
  seed by design, so relaunching a non-wafer world with a different or
  absent `--seed` silently grows different terrain under saved columns.
  (The Wafer is immune — pinned seed 0, verified.)
- SV-H3 — `player.dat` bypasses the file contract (tmp+rename, no fsync,
  no backup): CRC detects a torn write, then position/hotbar/stats/vitals
  are simply gone.

**Render (5):**
- R-H1 — **Ground traces render nothing on every backend** — STILL-OPEN
  from 2026-10-11. GLSL has zero `groundTrace` code, GL uniforms resolve
  to −1, no `render.ground_traces` cvar exists, `frameParams` never sets
  the fields; Vulkan declares the uniforms and never writes them. The
  mesher bakes trace params into every ChunkMesh, into a void. (Trace
  *legibility* was JD's stated launch-critical item — the sim pulses;
  the ground does not.)
- R-H2 — **SoftwareRenderer is not a full IRenderer**: it never overrides
  `supportsPbrPath/setPbrTier/setAmbientMode/setFrustumMarginDeg/
  setCullProtoConfig/uploadPbrChunkMesh` — all silently inherit no-op
  defaults while the game calls them unconditionally. Its real PBR path
  sits outside the interface, unwired, and dead anyway (hard-coded
  `materialId=0xFFFF` forces per-fragment fallback).
- R-H3 — **Software mode — the DEFAULT backend — has no HUD, menus,
  crosshair, or highlight at all.** The game paints the full UI into
  `p.hud` every frame and the launch menu opens first; none reaches the
  screen in software mode. GL33 and Vulkan both rasterize HUD.
- R-H4 — **Vulkan silently drops sprites and traces** (zero sprite
  references in render_vk/, no log). The entities path shows the honest
  pattern (logs once) — sprites/traces don't follow it.
- R-H5 — `render.pbr_tier` setter still unclamped (the one survivor of the
  2026-10-11 clamp sweep): `render.pbr_tier 9` flows into a
  `static_cast<PbrTransparencyTier>` with no matching shader branch →
  misrendered transparency. One-line fix.

**Texture / security (1):**
- T-H1 — **The canonical mod hash is not framing-safe; signatures are
  transplantable** (`signing.cpp:52-67`): digest = `rel_path ‖ 0x00 ‖
  file_bytes` per file, no length prefix, no file count. Two different
  mods can produce the identical byte stream (worked example in
  Appendix E), so a signature issued for one mod verifies **Verified**
  for another. Fix: length-prefix both fields (requires re-issuing
  existing signatures — there are none in the wild yet, so now is the
  cheap moment).

**Net / script / mods / console (1):**
- N-H1 — **Console alias permission laundering** (`concmd.cpp:538`):
  keybinds and `wait`-suspensions were fixed to carry the caller's
  permission mask; `aliases_` is a plain `map<string,string>` with no
  mask. A restricted MCP agent or mod defines `alias x "give 123 64"`;
  when the player later types `x`, the cheat runs at full authority.
  Same pattern, third surface.

**Game (2):**
- G-H1 — **The real Inventory is never persisted**: `world_save.cpp`
  writePlayer stores pos/look/dimension/hotbar-BlockIds/stats/vitals
  only; `player_.inventory` is never put in PlayerMeta. Everything given,
  picked up, or granted — including the Fragment — vanishes on relaunch.
  The Fragment's boot re-grant masks the loss, which also falsifies the
  "a restored player keeps theirs" comment at game.cpp:449-451 across
  restarts.
- G-H2 — **Two disconnected inventories**: the real 42-slot `Inventory`
  is invisible/unusable; HUD, place/break, the creative palette, and
  player.dat all run on Game's separate 8-slot BlockId `hotbar_`. (Per-
  tool handlers are confirmed genuinely absent *and* nothing assumes
  they exist — that part is clean.)

**Entities / camera / player (2):**
- E-H1 (CAM-1) — **First-person "numerically identical" is FALSE in the
  integrated game.** `GameCameraController::viewPose()` bypasses the
  camera manager and returns `player_.pos` — which is FEET since the
  locomotion wiring (game.cpp:1335's own comment admits the interaction
  ray was migrated to `playerEyePos` for exactly this reason; the camera
  wasn't). FP renders 1.62 blocks low and never dips for sneak/crawl.
  The engine FP camera is correct and selftest-pinned — it just never
  renders. MCP pilots *do* use the eye, so agents and humans see from
  different heights.
- E-H2 (ENT-1) — **The projectile system does not exist.**
  `litholattice_projectile` is a header-only INTERFACE library;
  `ProjectileManager::spawn/tick/loadChunksAhead` are declared and never
  defined, with zero production callers. The locked-design claim
  "projectiles load chunks ahead of path" is FALSE as shipped (no caller
  is harmed today — Ping's `packet` projectiles ride the item path —
  but the engine feature is vapour).

---

## 3. MEDIUM (53) — condensed, by area

**Core/handshake:** dueFor group-0 wildcard in both directions;
phaseBudget double-division and mixed-group overrun stats; **`canTick()`
is dead code — workers use a divergent inline poll, so the design's
central claim "workers poll canTick()" is FALSE as merged**; Chase-Lev
copy-throw fix INCOMPLETE (job lost, `inFlight_` leaks, `waitIdle()` can
hang); freeze lifecycle state leaks across routine stop/start; HsStats
published by reference with no thread contract; paused/frozen sim still
iterates at masterHz and busy-spins.

**Signal/world:** no column unloading — memory grows monotonically with
explored area (quantified in Appendix B); streamed insertion is one
sim-stopped window doing the whole batch + per-sub-chunk snapshots on the
main thread (the insertion stutter, root-caused); `loadColumn` remap
failure latches for the whole session and silently downgrades saved
columns to fresh generation; TraceStrengthBuffer full-pass vs incremental
coverage diverges (latent, pre-wiring); chunk-border structure replay
can disagree between owning and replaying chunks (claims overstated).

**Save:** the 4 round-trip failures themselves (test integrity — see §5);
hotbar stores raw BlockIds, never name-remapped (a registration reorder
silently re-points every hotbar slot); `RegionFile::open` reads whole
regions with no size cap; streamed `loadColumn` decodes feed the shared
SGNL sink (residue + cross-path race, SUSPECTED); unknown blocks vanish
silently on the streaming path.

**Render:** software renderFrame does **no frustum culling at all**;
software sprite pass discards shade mode/sorting/alpha and allocates per
quad per frame; no frame-rate limiter anywhere (vsync-only, as
documented); mesher reads live registry/materials with no snapshot;
GL33 sprite VBO orphaned/reallocated every frame; Vulkan HUD overlay
silently truncates; PBR transparency tiers exist only on GL33 (tier 4 a
documented single-layer compromise there, a stub on CPU, absent on
Vulkan/software); emissive-provider refresh reads the live ChunkMap off
the mesher snapshot path (SUSPECTED threading hazard); mesher sub-chunk
selftest failure root-caused as a **test bug** (the check's `surfaceSy`
= topmost-sub assumption), mesher itself passes its unit tests.

**Texture/security:** a crafted `mod.json` (`"version": ""` on a
dependency) throws `std::out_of_range` out of the manifest parser and
**crashes the game at boot** (mechanism reproduced standalone); `.ltxr`
parser has no node/stop/input caps and unvalidated numeric params
(UB/OOM from a hostile recipe — bounded today because only first-party
recipes bake offline, but .ltxr is the designated mod format); file-size
caps are checked *after* unbounded whole-file reads (multi-GB pack file
OOMs despite the caps); the signed hash silently excludes symlinked
content the loader will read (hash ≠ effective content); the 2026-10-12
SVG float→int UB (L-assets7) is **still open** — conversion still
precedes the clamp.

**Net/script/mods:** script memory quota is defeatable (two accounting
paths clobber one shared counter); the script-UAF fix is INCOMPLETE (the
AngelScript shim's `AllocHeader` still holds a raw `ScriptEngine*` that
`freeShim` dereferences); registry thread-safety is documented, not
enforced (only half the entry points take `defMutex_`; `registerBlock`
has no thread check); no namespace-ownership validation → id squatting
under first-wins; `BlockId` silently truncates at 65,536 blocks;
restricted console callers reach destructive non-cheat commands incl.
arbitrary-path file truncation via `screenshot`; ModLoader doesn't
validate or dedupe mod ids and keys permission grants by id alone;
**net is unwired — zero socket code, zero ConnectHandshake
instantiations, no server mode: anti-cheat is documentation only**;
**MCP server still not wired into the game at all** (confirmed — the
known gap stands; adapters exist "for the builder's --mcp-stdio
wiring").

**Game:** spawn floats ~11 blocks — root-caused to a literal `+12` in
both `structures.cpp:119` and `dimensions.cpp:25` (spawn y=277 vs
platform 265; `kill` respawn inherits it); tick-debugger commands desync
Game's pause state from SimThread's (cross-ref CH-H7); **chipor selftest:
the CHECK is wrong, the registry is right** (guard demands size 55;
legitimate appends — Structure Block, Certificate Table — make it 57;
all id-level checks pass); **scout selftest: the TEST is wrong,
production is right** (scout omits the ModelResolver that dimensions.cpp
passes; a second identical omission sits latent behind the first);
**only 2 of 5 core dimensions exist in code** — Cosmic/Underwafer/Void
have no JSON, registration, or stub, yet `mcp_player_control.cpp`
already numbers them 2/4/5 (+ether 6, undefined 7); Fragment has no UI
and FragmentState has zero production callers (known, verified clean
absence).

**Entity/camera/player:** mob independence HOLDS in design (AiWorld
clamps 8–16 Hz, advances by TPS ratio — never rides 512 Hz) but AiWorld
has **no game integration at all**; live entities tick on render-frame
dt *including during pause* (vitals/locomotion/sprites correctly freeze
with worldTick); camera chunk-loading is OVERSTATED (radius/cvar exist
and the manager forwards them; no production code loads anything from
them; MCP adapter's chunkRadius is an unused hook; no hardware scaling);
`camera.third_person_distance` cvar is dead after construction; **vitals
are TPS-coupled** — oxygen/drown/stamina are tick counts calibrated at
32 TPS, so at 512 TPS the player drowns 16× faster in wall-clock time
(locomotion normalises via dtTick; vitals don't); `OcclusionThread::
gain()` still takes a mutex on the audio thread — and its header now
**falsely claims "Lock-free"**.

## 4. LOW (73) & INFO (4) — summary

Low findings are enumerated in full in the appendices. Themes: stale or
false comments/docs (fail_fast.h's contract text, the "lock-free" header,
three files still claiming the game can't start, game README drift,
SAVE_FORMAT.md drift, stale handshake deployment guidance, MCP
`net://version` still advertising protocol 1); small robustness edges
(uvarint64 non-minimal encodings, JSON parse-success ≠ valid, TickArena
residual edges, `resolveMods` unbounded recursion, double→int UB on
hostile JSON numbers, replay window advancing before AEAD auth —
latent, no cipher yet); test-quality nits (tautological river
"chunk-pure" test, brittle exact-count checks, leftover DEBUG spam);
cosmetics ("Organic World" surviving the rename, kFusor comment debris,
LithographOR's commented-out row). INFO: **LBT — the LithoBinary Tagged
format named in design docs — does not exist anywhere in the tree**
(zero grep hits; persistence is the region/LBT-less format actually
implemented); CRC32 is corruption *detection*, not authenticity;
PlayerMeta tickVitals batching artefacts; occlusion cadence arithmetic
notes.

---

## 5. Root causes & answers JD asked this review to settle

1. **The 4 save round-trip failures are NOT data loss — VERIFIED.** The
   save reviewer's diagnostic harnesses replayed the selftest scenarios
   exactly: all columns load, `importState` succeeds, signal export is
   byte-identical, **0 voxel diffs, 0 state diffs**. The only difference
   is `VoxelPalette` *insertion order* in structure/edit-bearing
   sub-chunks, and `sameCells` compares palettes by order + raw indices.
   The comparator is wrong; the data is semantically bit-perfect. (One
   real side-effect: re-saved files aren't byte-identical to originals —
   palette order differs — though semantically stable.) Fix: compare
   per-voxel `at()`+state in `sameCells`.
2. **The "no PBR material registry" gap is FIXED in production.** Main
   always builds the registry → WorldSave registers MATS (string IDs,
   256 cap, engine round-trip test passes). The selftest log line
   persists only because the selftest constructs WorldSave with
   `nullptr` materials.
3. **H2 — the protocol-version answer (literal):** `knownProtocolVersions()`
   = **{1,2,3}** exists, but **no code installs it or constructs a
   ConnectHandshake at all**. The actual default offer is **{3} ONLY**
   (empty config → ctor fills `{kProtocolVersion}` = V3). A v2-only peer
   is rejected fail-closed. Codecs: 2→V2Delta, 3→V3StableIds; **no v1
   codec exists in-tree** despite 1 being in the accepted-framing set.
   JD's decision is therefore really: {3}-only as-is, or install {1,2,3}
   *and* write the missing v1 codec path.
4. **Signing-key state — clean, with one operational fact.** Exhaustive
   sweep (every 64-hex literal in source/tests/game/tools + all three
   root snapshot zips, each derived as an Ed25519 seed and compared):
   **no private key material exists in the tree.** (No key material is
   reproduced in any review file.) The baked key **is still the test
   key**, so release builds refuse to boot by design (game main only,
   `#ifdef NDEBUG`) until JD installs his real keypair. The historical
   git-history exposure can be neither confirmed nor cleared from this
   tree (no .git here); the key is correctly treated as retired
   regardless.
5. **ChipOR and scout selftest failures are both test bugs** (see §3,
   Game) — the two cheapest "failures" on the board to close.
6. **Mesher sub-chunk failure is a test bug** (two reviewers converged
   independently): the check assumes the surface sub-chunk is the
   topmost; the mesher's own unit tests pass.

---

## 6. Claim verification — the short list

**FALSE as shipped:** ground traces render (R-H1); FP camera numerically
identical *in the game* (E-H1); projectiles load chunks ahead (E-H2);
workers poll `canTick()` (dead code, §3 Core); occlusion `gain()`
"lock-free" (header); Morning Report's "stepped ticks use the same
pipeline as live ticks" (CH-H5).
**OVERSTATED:** "full IRenderer software backend" (R-H2); "two software
surfaces agree pixel-for-pixel" (true by construction for the world pass
only; untested/false for sprites/HUD/PBR); "transparent tiers 1–4
implemented" (GL33 only); camera chunk-loading "limited radius, scales
with hardware" (no consumer, no scaling); first-wins as a system-wide
mod-conflict rule (holds in registry/extension layers, not ModLoader);
release test-key refusal "tree-wide" (one main(), release-only);
chunk-border structure replay accuracy; mob system as a *production*
system (design sound, zero game integration).
**HOLDS (verified):** Timing Handshake determinism/WAIT/FREEZE in the
shipped scenario (with the §1–2 lifecycle caveats); texture determinism
and seamless-by-construction (incl. voronoi/warp); sprite registry
first-wins/fail-closed + splitmix64 emitters; Cook–Torrance core +
K-nearest lights + flood-fill ambient modes; frustum margin clamp
0–15 and culling-never-touches-sim; BlockUpdate saturation fix; packet
bounds; script sandbox (no file/network bindings); hot-swap `def()` race
fix *for blocks* (incomplete for items); wafer seed-0 pinning; registry
refactor (56 externs = 56 adds = 56 resolveIds, order and strings exact;
`kSilicaTilledSoil→tilled_silicon` correct); xp/static as STATS
(selftest-pinned); velocity first-class + PlayerMeta v3 split; MATS
persistence; PNG bomb cap; JSON depth cap; canonicalisation/containment
for pack paths.

## 7. Regression ledger vs prior reviews (summary)

**FIXED-CONFIRMED (sample):** SimThread tick-pipeline exception boundary
(scope widened — see CH-H6/C-3); runBetweenTicks RAII; JSON depth cap;
tick_arena wraparound; math normalize NaN; OcclusionThread `start(Inline)`;
same-voxel raycast; IK NaN (one LOW residual: solveTwoBone doesn't
validate rig-internal NaN); AI path-job deadlock; AI self-despawn *in
code* (untested — INCOMPLETE); block_names truncation defect (durability
gaps remain as C-4); Anvil sector leak; IoScheduler dtor UAF; region
backups; test-seed removal + boot guard; scanMods seal; console double-
`wait` laundering; alias-cycle overflow; net integer UB; packQuat/
posToFixed; software |w| clamp sign; engine_render golden hash; registry
abort-guard removal; content_selftest density guard; 4 missing HUD
icons; evening C1 selftest segfault (suite now completes); `give 0`.
**STILL-OPEN:** ground traces (R-H1); `gain()` mutex; `setInline(false)`
wart; tick-stepper defects (CH-H5); pause desync (CH-H7); TripleBuffer
read-lifetime doc; EV-H2/M4/M5 net items; MCP net://version; silent
unterminated `/*`; bumpCVar NaN; unsandboxed `screenshot` path (now also
reachable by restricted callers — N-M6); SVG L-assets 2/3/4/5/7;
provenance textures-only; converter .png-only; Anvil readHeader
normalise; `.torn` overwrite; save-format doc drift; kFusor debris;
inventory StableId currency; FragmentState wiring; spawn float (now
root-caused, G-M1).
**REGRESSED:** the chipor registry-size guard — the same stale
exact-count pattern fixed elsewhere re-broke when legitimate blocks
were appended (G-M3). Nothing else previously fixed was found re-broken.

---

## 8. Proposed fix program (for JD's approval — NOT started)

**P0 — Handshake lifecycle pack (C-1, C-2, C-3, CH-H1, CH-H2).** One
workstream, one owner: sequence reset + generation-stamped publication
(single atomic word), no-detach stop with shared-ownership fallback,
worker exception boundary, freeze quiesce. Gate: `--handshake-test`
passes *under load*, plus a stop/start soak test (save/stream loop).
Until this lands, `sim.timing_handshake` stays default-OFF and should
be treated as unsafe to enable.
**P0 — Permanence pack (C-4, SV-H1, SV-H3, SV-H2).** blocks.names onto
the standard contract + fail-closed load; flush failure propagation to
`saveWorld`'s return; player.dat onto the contract; persist the world
seed (level.json). Gate: kill -9 mid-save drills + torn-file drills.
**P1 — Signal trust & streaming (SW-H2, SW-H1, SW-H3, SV-M3/M5).**
Clamp strength at load; index streamed columns into SignalSim (the
builder-queue item, now severity-backed); exception-guard streamer
jobs; session-latch removal. Gate: build a circuit past the pre-bake
edge, save, relaunch, circuit alive.
**P1 — WAIT accounting honesty (CH-H4 + decision C2).** Cycle-budget
accounting; stop charging preemption to components or make offender
flags advisory until accounting is deterministic. Gate: attribution
test passes under load.
**P1 — Render honesty pack (R-H1, R-H2, R-H3, R-H4, R-H5).** Ground
traces end-to-end (JD's launch-critical legibility item) OR remove the
dead wiring and the claim; IRenderer capability surface made explicit
(no silent no-ops on the default backend); HUD rasterisation in
software mode; Vulkan sprite/trace drops log once; pbr_tier clamp
(one line — do it in any pack).
**P1 — Console/mod security pack (N-H1, N-M6, T-M1, T-H1).** Alias
masks; restricted-caller command audit; mod.json exception containment
at boot; hash framing (length-prefix) + re-issue. Gate: the alias
laundering repro and the transplant pair both fail.
**P2 — Game correctness (G-H1/G-H2 [needs JD's call: which inventory
is canonical], G-M1 spawn +12, E-H1 FP eye, PLY-1 vitals TPS
normalisation, G-M3/M4 + SW-L8 test fixes, SV-M1 comparator fix).**
Several are one-liners riding on decisions, not effort.
**P2 — Wiring completions (N-M8/M9 MCP + net honesty, E-H2 projectile
implement-or-declaim, dimension registrations G-M5 [JD's Cosmic call],
camera chunk-load consumer E/CAM-2).** Each is either real work or a
doc correction — JD picks which, per item.
**P3 — Medium/low sweep** by area, in appendix order.

---

## Appendices (reviewers' full reports, verbatim)

The eight files below contain every finding with file:line, quoted
evidence, impact, and fix direction, plus each area's full claim and
regression tables.

---
## Appendix A — Core, SimThread & Timing Handshake

# Code Review 2026-10-13 — Engine Core, SimThread & the Timing Handshake (read-only)

Scope: `engine/src/core/`, `engine/src/sim/` (incl. brand-new `timing_handshake.{h,cpp}`),
plus the game-side seams the scope names (tick_freeze/tick_step/tick_unfreeze bindings,
pause interplay) and `~/workspace/experiments/pull-tick/hs-design.md` as the design source.
No files modified. Verification runs (build-linux): `ctest -R engine_core|engine_sim` —
**both PASS**; `LithoKraft --handshake-test` — **FAILED** (one WAIT check, see H4);
standalone repro for C1 compiled in /tmp against the built libs.

Severity counts: **3 critical, 8 high, 7 medium, 9 low.**

---

## CRITICAL

### C1. Handshake workers spontaneously RE-EXECUTE the last published phase after every stop()/start() — VERIFIED (repro)
- `timing_handshake.cpp:91-100` `start()` never resets `publishSeq_`, `shardDoneSeq_`,
  `arrivals_`, or `phaseOpenStamp_`; `workerLoop` (`:139-145`) starts with
  `uint64_t seenSeq = 0;` and treats any `seq != 0 && seq != seenSeq` as new work:
  ```cpp
  uint64_t seenSeq = 0;
  ...
  const uint64_t seq = publishSeq_.load(std::memory_order_acquire);
  if (seq != 0 && seq != seenSeq) { seenSeq = seq; ... runItem(...) }
  ```
- A respawned worker therefore runs every due shard component of the *stale* last
  phase immediately, outside any tick/barrier, against live world state.
- Repro (bare authority, 1 counting component, in /tmp/hs_repro.cpp):
  `runPhase` → fires=1; `stop(); start();` sleep 300 ms with **no** new runPhase →
  **fires=2**. Confirmed.
- Impact: stop/start is not exotic — `Game::saveWorld()` (game.cpp:659-663) and
  `Game::pumpStreaming()` (game.cpp:688-704) stop+start the SimThread (and thus the
  authority) on *every save and every chunk-streaming pump*. In handshake mode each
  such cycle spuriously re-fires Signal net-shard / lamp-shard work (the exact
  disjoint-write kernels the determinism story rests on) at an arbitrary wall-clock
  moment, with stale cycle state (`hsMarkedNets()` etc.). Determinism and the
  mutation-window protocol are both broken by a routine code path.
- Fix: reset publication state in `start()` (`publishSeq_=0`, zero `shardDoneSeq_`,
  `arrivals_`, `committedPhase_`) or add an epoch/generation word workers capture at
  spawn; add a stop/start regression check to handshake_test.cpp.

### C2. stop() detach leaves workers running against objects that are then destroyed — VERIFIED
- `timing_handshake.cpp:102-125`: after a global `ceiling × 2` grace, stuck workers are
  `detach()`ed and merely logged. A detached worker keeps dereferencing `this`
  (`comps_`, `shards_`, `shardDoneSeq_`, `m_`, `workerExited_`) and its `WorkFn`
  captures (Signal components capture `SignalWorldSimulation*`/`hsWorld_`; domain
  components capture `SimThread*`).
- `~TimingHandshakeAuthority()` calls `stop()` and then destroys the members the
  detached thread is using; `~SimThread`/dimension unload likewise frees the world
  (`game.cpp:646` `sim_.reset()` on dimension switch). Detach is exercised by the
  shipped test itself (freeze e2e log today: *"worker 0 still inside a component at
  stop(); detaching"*) and the test only survives because it sleeps 300 ms after
  releasing its latch before destroying the authority (handshake_test.cpp:341-342) —
  a workaround, not a guarantee.
- Design (hs-design §2.2/D4) justifies detach as "process exiting / world being torn
  down", but `SimThread::stop()` is also the *save* and *streaming* path, after which
  the same objects keep living and are restarted (see C1): a still-running "stuck"
  signal worker can then mutate world/save state concurrently with the game thread's
  supposedly consistent snapshot — directly contradicting the header promise that a
  frozen world "must stay exitable and savable" (timing_handshake.h:96-99).
- Fix: never detach into freed storage — heap-allocate shared authority state
  (`shared_ptr` state block captured by workers), or make detach imply leaking the
  authority + simulation deliberately and document it; at minimum, SimThread must
  refuse save/dimension-switch completion while a detached worker is outstanding.

### C3. Pull workers have NO exception boundary — a throwing component terminates the process with no report — VERIFIED
- `timing_handshake.cpp:81-89` `runItem()` calls `c.fn(phase, cycleTick)` bare;
  `workerLoop` (`:139-184`) contains no try/catch anywhere. An exception from any
  non-affine WorkFn escapes the `std::thread` callable → `std::terminate`, no
  SimCorruptionReport, no freeze — precisely the failure the fail-fast doctrine and
  the 2026-10-12 CRITICAL #1 fix were built to eliminate on the sim thread.
- The affine path is (accidentally) covered because SimThread wraps runPhase in
  `tickGuard`; bare-authority users and the worker path are not. A throwing
  component also never sets `finished`/arrives, so even a hypothetical catch at the
  top of the thread would leave the barrier to the watchdog.
- Fix: wrap `runItem` in try/catch; on catch, record the failure, still arrive at
  the barrier (or trip the freeze latch immediately with the exception text), and
  let the authority thread convert it via `simFailFastFromException` at the next
  runPhase boundary.

---

## HIGH

### H1. resetAfterFreeze() does not quiesce in-flight workers — stale arrival can satisfy the NEXT barrier; component double-execution — VERIFIED (code)
- `timing_handshake.h:113` `resetAfterFreeze()` is one store: `frozen_.store(false)`.
  It does not wait for, invalidate, or epoch-tag the frozen phase's workers.
- A worker that was merely *slow* (the realistic recovery case) finishes its item
  loop after unfreeze and executes workerLoop's arrival block
  (timing_handshake.cpp:161-166): `shardDoneSeq_[shard].store(seq /*stale*/);
  arrivals_.fetch_add(1);` under `m_`. If that lands after the next `runPhase` has
  reset `arrivals_=0` and published a new seq (`:218-228`), the stale increment
  counts toward the **new** phase's barrier: the authority can commit (running the
  commit hook — the game's buffer flip) while another shard is still executing the
  new phase. The stale worker then sees the new seq and runs its items a **second**
  time for that phase.
- Impact: barrier — the protocol's only synchroniser — is defeasible exactly on the
  FREEZE-recovery path the design sells as safe. `SimThread::resetHandshakeAfterFreeze`
  (sim_thread.h:152-156) adds nothing (two flag stores).
- Fix: generation-tag publications; workers capture (seq, generation) and the
  arrival block revalidates generation under `m_`; resetAfterFreeze bumps the
  generation and/or spins until in-flight workers park.

### H2. Publication tuple is read non-atomically — worker can execute phase N+1 work under seq N — VERIFIED (code construction)
- workerLoop reads `publishSeq_` (`:143`), then *separately* loads
  `publishedPhase_/publishedCycle_/publishedGroup_` (`:148-150`). If the worker is
  preempted between those loads for longer than one phase — possible precisely when
  its shard was **inactive** in phase N, so the barrier did not wait for it — the
  authority commits N, runs phase N+1's affine prologue, and publishes N+1. The
  worker resumes, reads phase N+1's tuple, executes N+1's items, then records
  completion against **seq N** and increments `arrivals_` (counting toward N+1).
  On its next loop iteration it sees seq N+1 ≠ seenSeq and executes the same items
  **again** (second arrival increment → possible early commit, compounding H1).
- Fix: seqlock-style revalidation (re-read publishSeq after the tuple; retry if
  changed), or pack phase/cycle/group into one atomic snapshot struct.

### H3. Watchdog is blind to affine hangs, and false-fires/misnames when affine work exceeds the ceiling — VERIFIED
- In `runPhase`, thread-affine items run **before** publication
  (`timing_handshake.cpp:214`), synchronously on the authority thread. The watchdog
  check exists only inside the barrier `wait_for` timeout branch (`:238-273`).
  Consequences:
  1. A hang in an affine component (Signal's flip_torches / components / mark /
     prep are all affine — the majority of the circuit work) never trips the
     watchdog at all: the sim thread hangs forever, no FREEZE, no report. The
     header's FREEZE promise (timing_handshake.h:106-110) covers only pull workers.
  2. `openStamp` is taken at runPhase entry (`:187`), *before* affine work, and the
     trip test is `t - openStamp > ceiling` — affine work legitimately exceeding
     250 ms (e.g. a first-tick full pass prep on a large circuit) makes the barrier
     check trip instantly even with instant workers, and the naming scan then picks
     `firstDue` — an innocent worker component — because nothing has "started".
- Fix: stamp the barrier window at publication (after affine), or give affine its
  own watchdog accounting; name the affine stage separately in HsFreezeInfo.

### H4. WAIT overrun attribution is falsified by the shipped test TODAY — innocent components flagged repeat offenders — VERIFIED (test run)
- `LithoKraft --handshake-test` (2026-10-13, this build): all determinism and FREEZE
  checks PASS, but `WAIT: no filler component flagged as offender` **FAILS** —
  log shows `test.filler0` and `test.filler3` both crossing the offender threshold
  (3 overruns) alongside the real burner. Suite verdict: `HANDSHAKE TESTS: FAILED`.
- Mechanism: `runItem` durations are wall-clock around `fn` (preemption/scheduling
  delay on a loaded machine is charged to the item), and runPhase attributes the
  phase's entire excess to the single max-duration component
  (timing_handshake.cpp:282-312). On any loaded/low-core machine (the target VM is
  2-core), the "slowest" slot rotates onto bystanders. Design §1.6/D10 chose
  single-component attribution; the implementation makes it unreliable in exactly
  the conditions WAIT exists for.
- Fix: attribute by summed shard work vs budget, require the offender's own work
  (not phase duration) to exceed the budget, or count an overrun for a component
  only when its own duration alone exceeds phaseBudget.

### H5. tick_step / stepped-tick defects from the ledger — BOTH CONFIRMED STILL-OPEN — VERIFIED
- `sim_thread.cpp:387-424` (paused branch):
  1. **Steps count master steps, not world ticks.** The loop runs once per
     requested step but a world tick fires only when
     `masterStep % worldDiv == 0` (`:404`). With a 512 Hz EUV domain and 32 TPS
     world (worldDiv=16), `stepTicks(16)` yields **1** world tick and
     `stepTicks(1)` usually yields **0** (depends on the masterStep residue).
     `ticksStepped_` increments only on real world ticks (`:418`), while the game
     binding reports success unconditionally (game.cpp:179-183 "stepped N ticks").
     Header claim "Step N ticks… Each step runs the full tick pipeline"
     (sim_thread.h:103-105) is false.
  2. **Engine random ticks are omitted.** The stepped branch calls
     `sim_->randomTick(...)` (`:412`) but never `runRandomTicks(...)` — the live
     branch calls both (`:439-460`). Stepped ticks therefore diverge from live
     ticks for every `kRandomTicks` block; the debugger does not reproduce the
     pipeline it debugs. (Also omitted in stepped mode: window/tick-ms accounting
     and `hsCountWindow_` domain window counting — stats-only, minor.)
  3. Design deviation: hs-design §4.2(5)/D11 required factoring one `stepOnce`
     used by both branches so this class of divergence cannot recur; the merge
     instead duplicated the body. There is **zero** stepTicks test coverage in
     engine/tests.
- Fix: single shared step body; count steps in world ticks (advance master steps
  until N world ticks have run); call runRandomTicks in the stepped branch.

### H6. SimThread::run() exception boundary is incomplete — game hooks outside tickGuard still terminate silently — VERIFIED
- `tickGuard` (sim_thread.cpp:313-324) wraps only the per-step lambda. On the same
  thread, outside it:
  - paused-branch `applyEdits(ray, kApplyAllEdits)` (`:383`) — calls
    `sim_->use/command/onBlockChanged` (game code);
  - `publishEvents()` → `sim_->takeEvents(...)` (`:289-290`, called `:486`);
  - stats block: `sim_->activeCount()` (`:502`), `sim_->inspect(...)` (`:525`);
  - `castRay` fallback (`:469`) — engine code, lower risk.
  An exception from any of these escapes `run()` → `std::terminate` with no
  corruption report — the exact scenario of the prior CRITICAL, on adjacent hooks
  of the same interface.
- Fix: widen the guard to the whole loop body (or wrap each hook call site).

### H7. Game ↔ SimThread pause desync — STILL-OPEN, amplified by the handshake — VERIFIED
- `game.cpp:164-182`: `tickFreeze`/`tickUnfreeze`/`tickStep` call
  `sim_->setPaused/stepTicks` directly, bypassing `Game::paused_`, the input
  context stack, and the pause UI. `Game::setPaused` (game.cpp:857-873) conversely
  knows nothing about the debugger freeze.
  - `tick_unfreeze` while the pause **menu** is up runs the sim behind the menu
    (violates the pauseSim invariant the menu path enforces).
  - Esc-resume after `tick_freeze` calls `Game::setPaused(false)` →
    `sim_->setPaused(false)` with **no** `resetHandshakeAfterFreeze()` — in
    handshake mode the authority latch then re-freezes on the next runPhase and
    `handleHandshakeFreeze` re-pauses the *sim* while the game believes it is
    running (game.cpp's `paused_` stays false, UI in Playing state).
  - `handleHandshakeFreeze` itself (sim_thread.cpp:113-118) sets only the
    SimThread's `paused_`; nothing notifies the game layer except the `hsFrozen`
    stats bit.
- Fix: route all four operations through one Game-level state machine
  (Playing / MenuPaused / TickFrozen / HsFrozen) that owns both flags.

### H8. worldTick and masterStep reset to 0 on every stop()/start() — keyed random streams repeat after every save/stream — VERIFIED
- `sim_thread.cpp:309`: `uint64_t masterStep = 0, worldTickN = 0, dropped = 0;`
  are **locals of `run()`**. Every `start()` (after every saveWorld /
  pumpStreaming stop, see C1) restarts the world tick at 0: edit stamps,
  `sim_->randomTick(tickIndex=worldTickN)` seeding, and
  `runRandomTicks(..., worldTickN, ...)` (splitmix keyed on tickIndex,
  random_ticks.cpp:57) replay the identical sequence, and SimStats.worldTick
  visibly goes backwards. This is the known F1 defect from the Powers-of-Two
  tick-threading analysis; still present in both legacy and handshake mode.
- Fix: hoist masterStep/worldTickN/dropped (and the accumulator/window state that
  must persist) to members initialised once at construction.

---

## MEDIUM

### M1. dueFor group semantics: caller group 0 is a wildcard in BOTH directions — VERIFIED
`timing_handshake.cpp:68`: `if (d.group != 0 && group != 0 && d.group != group)
return false;` — a `runPhase(..., group=0)` call (the API default,
timing_handshake.h:106) fires **every** grouped component (Signal group 1 *and*
domain group 2 together), and a group-0 component fires in every group. The header
documents only the component side ("0 = participates in every runPhase group").
Production never calls group 0 today, so this is latent — but the bare-authority
test API invites exactly that call. Fix: give the caller side an explicit
"kAllGroups" sentinel distinct from 0, or make group 0 match only group 0.

### M2. phaseBudget formula double-divides; overrun stats mix incompatible groups — VERIFIED
`sim_thread.cpp:64`: budget = `1e9 / masterHz / phasesPerCycle`. hs-design §1.6
defines the nominal window as the master-step duration (1/masterHz), and Signal
phases do not run at the master rate at all — they run P-per-*world-tick* inside
`sim_->tick()`. At 32 TPS world + 512 Hz EUV the budget handed to every Signal
phase is ~488 µs against a 31.25 ms world-tick envelope, and group-2 domain phases
(period 1.95 ms) share the same 488 µs budget. `hsOverrunPhases` therefore
systematically over-counts in mixed-rate configs (compounding H4's attribution
problem) and the single `HsStats` pool mixes group-1 and group-2 phases with
different natural periods. Fix: budget per group from its own period
(world tick/P for group 1, master step for group 2), or pass phaseBudget=0
(accounting off) until per-group budgets exist.

### M3. canTick() is dead code; workers do not poll it — the design's central API is untested and divergent — VERIFIED
`grep` across the tree: `canTick` has **zero** callers. workerLoop re-implements
the poll inline (seq check + due scan, timing_handshake.cpp:143-173) with
semantics that differ from canTick's documented contract ("true exactly when the
currently published phase has due work for shard that the shard has not yet
pushed") — e.g. the inline path marks `shardDoneSeq_` even for phases where the
shard had no work, and has the H2 tuple race canTick shares. Header claim
"pull worker threads poll canTick()" (timing_handshake.h:2-4; hs-design §0/§1.4)
is **false** as merged. Fix: make workerLoop call canTick (single implementation),
or delete canTick and correct the docs.

### M4. Chase-Lev copy-throw: worker now survives, but the job is lost and JobSystem's inFlight_ leaks → waitIdle() can hang forever — INCOMPLETE prior fix
Prior finding (Full-Tree low): "throwing std::function copy in steal() escapes
findJob's try/catch → worker death". Today `JobSystem::workerLoop`
(job_system.h:~387-400) wraps `findJob` in `catch (const std::exception&)` — the
worker survives *std* exceptions. But: (a) `ChaseLevDeque::popBottom`
(chase_lev.h:~89-95) copies the slot **after** decrementing `bottom_`; a throwing
copy leaves the deque with `bottom_` already rewound and the job neither returned
nor restored — the job silently vanishes while `JobSystem::inFlight_` (incremented
in `submit`) is never decremented, so `waitIdle()`/`JobFence::wait()` can hang
forever; (b) the findJob catch is std-only — a non-std exception still escapes
the worker thread → terminate. Fix: copy-before-commit in popBottom (copy into a
temp before storing bottom_, as steal already does), catch(...) in workerLoop,
and decrement inFlight on the findJob-failure path.

### M5. Freeze lifecycle state leaks across routine stop/start; the frozen master step is dropped, not re-run — VERIFIED
- Live path: when the group-2 `runPhase` freezes, `hsOk=false` skips the tick, but
  `++masterStep` still executes (sim_thread.cpp:~463) — the frozen step's domain
  work is silently dropped. hs-design §4.4(3) says "the phase that froze is re-run
  from its snapshot"; nothing re-runs it (Signal-side, `tickHandshake` forces a
  full pass on the next cycle via `invalidateNetwork` — that covers group 1 only).
- `TimingHandshakeAuthority::start()` clears `frozen_` (timing_handshake.cpp:94),
  but `SimThread::freezeReported_` is only reset in `resetHandshakeAfterFreeze()`.
  A routine save (stop/start) after a freeze therefore silently clears the latch
  while leaving `freezeReported_=true`: a *second*, distinct freeze later is
  never reported (`handleHandshakeFreeze` early-outs on the exchange).
- Fix: keep freeze state in one place; reset freezeReported_ when the latch is
  observed cleared; don't advance masterStep for a frozen step (or explicitly
  document the drop and its determinism consequence).

### M6. HsStats is plain data published by reference with no thread contract — VERIFIED
`timing_handshake.h:120` `const HsStats& stats() const` returns the live struct
that `runPhase` mutates (timing_handshake.cpp:277-312), including a
`std::vector`. SimThread reads it on the authority thread (safe), but
perf_headless.cpp:369-374 reads `sim.handshakeAuthority()->stats()` from the main
thread *while the sim is still running* (stop happens afterwards) — a formal data
race (and a reallocation hazard if `resetStats()` were ever called live).
Fix: document "authority thread only, or while stopped", or snapshot under `m_`.

### M7. Paused/frozen sim loop still iterates at masterHz; waitLoops busy-spins — VERIFIED
In the paused branch, `acc` is zeroed every iteration and the loop tail sleeps
`waitUntil(last + (step - acc))` (sim_thread.cpp:529) → period = one master step.
With a 512 Hz domain the "paused" sim wakes 512×/s to drain edits, raycast
(kLookRange=64 DDA), rebuild stats and publish — indefinitely, including while
FREEZE-frozen for inspection. `waitLoops` (sim_thread.cpp:149-157) is a pure
`yield()` spin for up to 5 s per call (used by the world-query path).
Fix: park the paused loop on a CV/condition (wake on unpause, step request, edit,
or stop); give waitLoops a short sleep backoff.

---

## LOW

### L1. Rate validation accepts rates that break the scheduler — SUSPECTED (no in-tree caller)
`validateTickRate` accepts any positive power of two (sim_thread.cpp:36-38).
`tps = 2^30` makes `step = Ns(1e9/masterHz) = 0`, and the catch-up clamp then
divides by `step` (`(acc - maxLag) / step`, sim_thread.cpp:379) → SIGFPE — the
class badRate() was created to prevent. Domain `hz` is likewise only pow2-checked:
`kMaxDomainHz = 512` (clock_domain.h:10) has **no enforcement consumer** (grep:
only the definition and the game's EUV use). All in-tree callers clamp (cvar
`nearestTps`, CLI mode check), so this is an API-level gap, not a live path.
Fix: validate against `kTickModes ∪ {512}` (or ≤ some sane bound) and
`hz <= kMaxDomainHz` in the same fail-fast style.

### L2. Stale fail-fast contract text — fail_fast.h still says the boundary doesn't exist
`core/include/litholattice/core/fail_fast.h:31-37`: "SimThread::run currently has
NO exception boundary at all (checked 2026-10-11)… BUILDER FIX: wrap the per-tick
step…". The fix landed (tickGuard); the header — which presents itself as the
production-boundary contract for the builder — still instructs as if it hadn't.
Update it (and note H6's remaining gaps there).

### L3. JSON parser: parse success ≠ valid JSON — VERIFIED
Depth cap landed (see regressions), but `Json::parse` (json.cpp:88-95) never
checks for trailing input after the first value (`{"a":1} garbage` parses true),
and numbers go through `std::strtod` (json.cpp:78-82), which accepts `NaN`,
`Infinity`, and hex floats as JSON numbers (dump() then rewrites non-finite to
`null`, silently changing semantics). Mod/config JSON is the consumer. Fix:
end-of-input check after the top value; reject non-JSON number spellings.

### L4. TickArena residual overflow/validity edges — VERIFIED
The 2026-10-12 wraparound is fixed in `allocate/allocateSlow` (see regressions),
but: `alloc<T>(n)` (tick_arena.h:~76) computes `n * sizeof(T)` with no overflow
check before calling `allocate`; `fromHandle` validates only via `assert`
(release builds return wild pointers for bad handles); a moved-from arena has
`blocks_` empty and `allocate()` dereferences `blocks_[cur_]` — UB if used.
Low: no in-tree caller passes untrusted n/handles today (grep shows no alloc<T>
users outside the arena's own tests).

### L5. TripleBuffer::read() lifetime still undocumented — STILL-OPEN prior low
threading.h:16-35 documents the SPSC latest-value protocol but still not that the
returned reference is invalidated by the next publish+write cycle (the writer's
next `writeSlot()` may be the very slot a slow reader still holds). Two
concurrent readers would also race on the plain `read_` member. Fix: one doc
paragraph (copy immediately; single reader thread).

### L6. Log "one line per call" claim is overstated — VERIFIED
log.h:1-2 promises "one line per call", but the fail-fast path passes the
multi-line `formatSimCorruptionReport` block through a single `logError`
(fail_fast.cpp:47), producing multi-line log-file entries and LogTap records
(observed verbatim in today's handshake-test output). Harmless to the tap's
record model; misleading to anyone parsing the text log line-wise.

### L7. stop() grace accounting: one global deadline for all workers; ceiling×2 can overflow — VERIFIED
timing_handshake.cpp:111: `deadline = nowNs() + ceiling_.count() * 2` is computed
once before the join loop, so worker *k* gets whatever grace earlier stuck
workers didn't consume (the header/doc read as ceiling×2 per stop, ambiguous),
and for a user-set ceiling near `nanoseconds::max` the `*2` overflows uint64.
Fix: per-worker deadline (or document the global budget); saturate the multiply.

### L8. QPC-violation tripwire can never fire — claim OVERSTATED
Both stamps come from the same monotonic `steady_clock` (timing_handshake.cpp:
14-18, 155-157) and the pull stamp is necessarily read after the open stamp was
stored, so `pullStamp < phaseOpenStamp_` is unreachable absent a broken clock.
The counter is dead code presented (timing_handshake.h:123-125, hs-design §1.2)
as a permanent protocol tripwire. It verifies clock monotonicity, not the
"two synchronized QPC calls" agreement. Keep it, but relabel honestly.

### L9. Dead / divergent handshake surface — VERIFIED
- `Config::pureSpin` is never settable from SimConfig (SimThread builds `hc`
  without it, sim_thread.cpp:59-65) — D9's A/B flag is unreachable in production
  wiring.
- `setCommitFn` (timing_handshake.h:85) silently overwrites a Config-supplied
  commitFn — Signal's registration always wins; no warning.
- `dueFor`: `HalfCycle` divider is `phasesPerCycle/2` (1→1, 5→2 — not a half);
  a descriptor cannot force divider 1 over a cadence divider because
  `if (d.phaseDivider != 1)` (timing_handshake.cpp:76) treats explicit 1 as
  "unset", contradicting the header's "general form wins".

---

## CLAIM VERIFICATION (scope area)

| Claim | Source | Verdict | Evidence |
|---|---|---|---|
| "Determinism is a property of the protocol… a worker cannot see phase N+1 until the authority has committed N" | timing_handshake.cpp:2-5 | **OVERSTATED** | Holds in the shipped scenario (256-tick hashes identical today, all worker/phase combos), but H1 (stale arrival post-unfreeze) and H2 (tuple race) are construction-level counterexamples; C1 breaks it across stop/start. |
| "Missed-window policy: WAIT (a phase extends; nothing is ever skipped)" | timing_handshake.h:5 | **HOLDS** (authority) / exception in integration | WAIT test: all 64 phases commit, hashes equal. But a FREEZE-dropped master step is never re-run (M5). |
| "FREEZE: runPhase returns false past the ceiling; the authority never force-advances" | timing_handshake.h:6-7, :104-110 | **HOLDS** for pull workers; **OVERSTATED** overall | Bare FREEZE test passes (latch, immediate false, naming of a hung worker). Affine hangs never trip it (H3). |
| Watchdog "names the slowest unfinished component" | timing_handshake.h:53-58, hs-design §1.7 | **OVERSTATED** | Correct for the bare hang test; falls back to first-due (possibly innocent) when nothing started (H3), and WAIT offender attribution flags bystanders (H4). |
| "QPC" = steady_clock, violations counted | timing_handshake.h:123-125, hs-design §1.2 | **OVERSTATED** | Same monotonic clock both sides; violation branch unreachable (L8). MSVC steady_clock being QPC-backed is an implementation fact, not something this code establishes. |
| Legacy mode: "an unconfigured SimThread constructs no authority and runs the legacy path byte-for-byte" | sim_thread.h:72-75 (SimConfig) | **HOLDS** (structurally) | handshake_ built only under `config.timingHandshake` (sim_thread.cpp:55-66); legacy branches unchanged. Caveat: the Signal hs* decomposition is shared code — its legacy equivalence is hash-proven by today's scenario run. |
| Phases run at 4/cycle, Signal needs ≥ 4 | sim_thread.h:76; hs-design §3 | **HOLDS** | tickHandshake falls back to legacy cycle with a one-time warning when phases < 4 (signal_world.cpp:168-177); authority clamps 1..64. |
| Clock domains "max 512 Hz", master = max rate, integer dividers | clock_domain.h:10-14; sim_thread.h:2-4 | **HOLDS** as arithmetic; **FALSE** as a cap | masterHz/divider math is exact for pow2 rates (sim_thread.cpp:356-364); kMaxDomainHz is never enforced (L1). The header's "STUB: no logic yet" is honest — `ClockDomain::step()` is still just the LCG placeholder, in both legacy and handshake (affine) modes. |
| "stepTicks: Each step runs the full tick pipeline" | sim_thread.h:103-105 | **FALSE** | H5: master-step counting + runRandomTicks omitted. |
| Workers "poll canTick()" | timing_handshake.h:2-4; hs-design §0 | **FALSE** | M3: zero callers; inline divergent poll instead. |
| runPhase order: "Opens phase (stamps it), runs thread-affine due items inline, waits at the barrier" | timing_handshake.h:101-106 | **HOLDS** literally — and **deviates from hs-design §1.5** (publish first, then affine). The .cpp comment (timing_handshake.cpp:202-208) justifies affine-before-publish (mark/prep must precede shard work in the same phase); the deviation is sound but has the H3 accounting consequence and should be folded back into the design doc. |
| JobSystem: "Threaded runs hash bit-identical to single-threaded (det-sched self-tests, 1/2/4/8 workers)" | job_system.h:36-40 | **HOLDS** (as tested) | `engine_sim` (includes testDetSched) PASS today; not independently re-derived. |
| JobSystem shutdown: "destructor drains already-submitted work, then joins; no detached threads" | job_system.h:26-31 | **HOLDS** by reading | workerLoop executes found jobs before the stop exit check; ioLoop drains queue before returning. (Contrast with the handshake authority's deliberate detach, C2.) |
| Chase-Lev: "pushBottom returns false when full without touching item; never allocates" | chase_lev.h:62-64 | **HOLDS**, capacity nit | Full check reserves one slot (effective 1023 of 1024) — docs say "full (1024 pending)" in job_system.h:213-215, off by one. |
| SpscQueue "wait-free, no allocation after construction" | spsc_queue.h:1-2 | **HOLDS** | Monotonic-counter ring, full-capacity usable, correct acquire/release pairing by reading. |
| TickArena "never returns nullptr; growth is a pure function of the allocation sequence" | tick_arena.h:56-67 | **HOLDS** post-fix | Throws/aborts instead of nullptr on absurd sizes; chained-block growth unchanged. Residual edges: L4. |
| TripleBuffer "writer never blocks the reader and vice versa" | threading.h:14-15 | **HOLDS** | Lock-free exchanges only; lifetime caveat L5 stands. |

## REGRESSION CHECK (prior reports, scope items only)

| Prior finding | Verdict today | Evidence |
|---|---|---|
| Full-Tree #1 CRITICAL — SimThread::run() has no exception boundary (sim_thread.cpp:231) | **FIXED-CONFIRMED for the tick pipeline; INCOMPLETE overall** | tickGuard wraps both live and stepped tick bodies (sim_thread.cpp:313-324) and converts via simFailFastFromException. Remaining gaps: H6 (hooks outside the guard) and C3 (handshake workers). fail_fast.h text not updated (L2). |
| Full-Tree medium — runBetweenTicks not exception-safe (sim_thread.cpp:55) | **FIXED-CONFIRMED** | RAII PauseGuard restores prior pause state on throw, declared before the lock block so unlock precedes restore (sim_thread.cpp:122-136). Caveat (new, low): the guard excludes the sim thread's mutation window but not in-flight handshake pull workers — between ticks no phase is in flight unless a C1/C2 stale worker exists. |
| Full-Tree #10 MEDIUM — JSON parser has no depth cap | **FIXED-CONFIRMED** | `kMaxDepth = 128` enforced in both object and array branches (json.cpp:27-35). Residual laxness is L3 (separate). |
| Full-Tree low — chase_lev.h throwing std::function copy in steal() → worker death | **INCOMPLETE** | See M4: worker survives std exceptions now; job-loss + inFlight leak + non-std escape remain. |
| Full-Tree low — core/tick_arena.cpp:88 size wraparound vs "never returns nullptr" | **FIXED-CONFIRMED** (residual L4) | bump() does checked arithmetic (tick_arena.cpp:32-38); allocateSlow aborts on `size > SIZE_MAX - align` (:100-104). |
| Full-Tree low — core/math.h normalize() of zero vector → NaN | **FIXED-CONFIRMED** | Guard returns (0,1,0) for non-finite/near-zero length (math.h:14-19). |
| Full-Tree low — TripleBuffer::read() lifetime undocumented; Game::simStats() derefs sim_ unchecked | **STILL-OPEN** (core half) | threading.h comment unchanged in substance (L5). The game-side deref is outside this scope. |
| Morning Report §5.5 — "tick debugger wired and working; SimThread steps are correct (same pipeline as live ticks)" | **FALSE / defect STILL-OPEN** | H5 disproves the "same pipeline" claim on both ledger defects; no stepTicks test exists to catch it. |
| Ledger defect — stepped branch counts master steps, stepTicks(n) advances fewer world ticks under faster domains | **STILL-OPEN — CONFIRMED in code** | H5(1). |
| Ledger defect — stepped path omits runRandomTicks | **STILL-OPEN — CONFIRMED in code** | H5(2). |
| Ledger defect — game vs SimThread pause desync | **STILL-OPEN — CONFIRMED, amplified** | H7. |
| Evening Report — all "handshake" items | **Not applicable to this scope** | Those concern the *net* protocol handshake (engine/src/net/handshake.*), a different subsystem; the Timing Handshake merged after that report was written. No overlap. |
| 2026-10-11 report — core/sim items | **None in scope** beyond grandfathered engine_sim status | `engine_sim` PASS today (ctest, 2026-10-13). |

## Verification performed
- Read in full: timing_handshake.{h,cpp}, sim_thread.{h,cpp}, clock_domain.h,
  world_simulation.h, det_sched.{h,cpp}, random_ticks.cpp; core: time.h,
  threading.h, job_system.h, chase_lev.h, spsc_queue.h, tick_arena.{h,cpp},
  json.{h,cpp}, math.h, log.{h,cpp}, fail_fast.{h,cpp}, timer_resolution,
  process_memory.cpp, thread_debug.cpp; game seams (game.cpp tick bindings +
  setPaused + save/stream stop-start, signal_world.cpp registerHandshake/
  tickHandshake/onHsCommit); hs-design.md §§0-9.
- `ctest -R engine_core|engine_sim` in build-linux: **PASS** (21.2 s / 1.5 s).
- `LithoKraft --handshake-test`: determinism scenario PASS (legacy == hs at
  w1/p4, w4/p4, w4/p8), FREEZE bare + e2e PASS, **WAIT offender check FAIL**
  (H4) → suite FAILED.
- Standalone repro of C1 compiled against build-linux libs (kept in /tmp):
  component fires again after stop/start with no new publication.
- Not run: full ctest suite, TSan/helgrind (H1/H2/M6 are code-construction
  verdicts; a TSan build would strengthen them), any live-browser/GUI path.

---
## Appendix B — Signal, World/Chunks, Worldgen

# Code Review 2026-10-13 — Signal Solver, World/Chunks, Worldgen

Reviewer scope: (a) `SignalSim` + Timing Handshake seams, (b) world/chunk layer + game `ChunkStreamer`, (c) procgen + dimensions/worldgen wiring. Read-only review of `~/workspace/litholattice`; verification runs in `build-linux` only.

**Counts: Critical 0 · High 3 · Medium 5 · Low 8.**

## Test evidence gathered this session

- Baseline ctest: 42/46 passed (91%). Failures: `engine_audio`, `lithokraft_selftest`, `lithokraft_scout_selftest`, `lithokraft_chipor_selftest` (scout/chipor/audio are other reviewers' scopes).
- My full `LithoKraft --selftest` run (`/tmp/review-2026-10-13/my-selftest.log`): 890 PASS / 7 FAIL. In-scope failures: 4× save round-trip (storage reviewer's scope; see Regression), 1× mesher check (root-caused below as a **test** bug), 1× vulkan headless (`VK_ERROR_INCOMPATIBLE_DRIVER` — VM has no driver; environment, not code). All signal checks pass; handshake equivalence passes: 256-tick scenario hash `ead266df50800279` identical for legacy, hs(w1,p4), hs(w4,p4), hs(w4,p8).
- Engine test binary (`litholattice_tests`) full run: **0 FAIL**, including all cave checks (3×3 worm/cavern replica match, no floating debris, branched-worm forks).
- `ctest -R "engine_procgen|engine_litho_formula|litho_formula_gallery"`: 3/3 pass.

---

## HIGH

### H1 — Streamed disk-loaded columns are never indexed by SignalSim; their SGNL state is silently dropped (dead circuits beyond the pre-bake square)
**VERIFIED.** `engine/src/sim/sim_thread.cpp:52` — the only scan: `sim_->scan(chunks_);` runs once in the `SimThread` constructor, i.e. at `Game::loadActive` (`game/LithoKraft/src/game/game.cpp:551ff`), after `save_->loadDimension(...)` restores the pre-bake columns and before the `ChunkStreamer` exists. `Game::pumpStreaming` (`game.cpp:681-711`) inserts streamed columns with no `scan`, no per-cell `onBlockChanged`, no `importState`.

The storage side matches: `WorldSave::loadColumn` returns only a `ChunkColumn`. The SGNL payload `load` lambda (`game/LithoKraft/src/game/world_save.cpp:47-50`) appends signal bytes into `imported_`, but `imported_` is cleared at the start of `loadDimension` (world_save.cpp:139) and drained into `signalState` only by `loadDimension` (world_save.cpp:159). The streamer path never drains it: bytes accumulate for the whole session and are discarded at the next dimension load.

Impact: any player-built circuit saved in a column outside the initial load square comes back with correct blocks but a dead sim on the next session — wires/torches/lamps/components are absent from `torches_/wires_/lamps_/comps_`, and component memory/timers are lost. In-session placements work (edits go through `onBlockChanged`), and fresh generation is unaffected (all worldgen-placed blocks are `SimRole::None`, `blocks.cpp:70-99`), which is why this hides. If the player later edits in such a column and it is saved, the sim's (empty) export overwrites the orphaned SGNL state permanently. Directly against the world-permanence doctrine.
Fix direction: on insertion in `pumpStreaming` (sim already stopped), scan the inserted column (a per-column scan entry point on `SignalSim`) and import that column's SGNL bytes — `loadColumn` needs to surface the payload bytes per column instead of appending them to a dimension-level accumulator only `loadDimension` drains.

### H2 — Signal strength is never clamped to 1..64 at the save/disk trust boundary; values 65..255 index fixed bucket arrays out of bounds
**VERIFIED.** Invariant: `kMaxSignal = 64` (`game/LithoKraft/src/content/include/lithokraft/content/blocks.h:108`); bucket arrays are sized to it: `std::array<std::vector<int64_t>, kMaxSignal + 1> buckets_;` (`signal.h:267`).

- `SignalSim::importState` (`signal.cpp:51`): `m.out = r.u8(); m.out2 = r.u8();` — raw bytes, no clamp. A migrated, older-version, or hand-edited (CRC-valid) SGNL payload can carry `out` up to 255.
- Chunk cell states on disk carry a 7-bit value field (`stValue`, `block.h:83`), so a loaded wire can hold 65..127. Pass-through behaviors copy inputs to outputs unclamped (e.g. `logic_components.cpp:39` `const int out = (pass && v > 0) ? v : 0;`, also :64, :118, :180, :389), and `tickComponents` stores them raw: `m.out = uint8_t(step.out);` (`signal.cpp:579`). `ComponentStep.out` is *documented* `0..64` (`component.h:55`) but nothing enforces it.
- Both seed lambdas guard only the low end — `signal.cpp:622-629`: `if (d.role != SimRole::Wire || ... || s <= 0) return; ... buckets_[s].push_back(...)`. With s in 65..255 this is an out-of-bounds `std::array` access (heap corruption in release builds). The incremental path has the same hole: `hsPropagateNet` applies recorded seeds via `lb[size_t(s)]` with `lb` also `kMaxSignal + 1` entries.

Normal current-version play cannot produce >64 (all internal writers stay in range), so the trigger is exactly the long-lived-world case: cross-version saves, v1→v2 migrations, edited saves. A world file must never be able to corrupt the process.
Fix direction: clamp in `seed()` (both paths) and in `importState` (`out/out2 = min(u8, kMaxSignal)`), and clamp wire cell-state values when sampled as component inputs. Cheap, defense-in-depth at every entry point.

### H3 — A throwing streamer job permanently stalls streaming and hangs `~ChunkStreamer` (exit / dimension switch)
**VERIFIED.** `game/LithoKraft/src/game/chunk_streamer.cpp:56-68`: the job lambda decrements the shared counter only at the end —
```cpp
if (save) col = save->loadColumn(dim, cx, cz);
if (!col) col = gen->generate(cx, cz);
...
n->fetch_sub(1, std::memory_order_relaxed);
```
`JobPool::run` catches job exceptions (`engine/src/core/include/litholattice/core/threading.h:85-91`), so a throw from `loadColumn` (filesystem/store errors, `bad_alloc`) or `generate` skips the `fetch_sub` forever. Consequences: (1) `inFlight_` leaks upward until `update()`'s `kMaxInFlight = 32` gate blocks all further streaming for the session; (2) the destructor busy-waits on that counter with no timeout —
```cpp
while (inFlight_->load(std::memory_order_relaxed) > 0)
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
```
— so quitting or switching dimension hangs indefinitely. The unbounded memory growth in M1 makes the `bad_alloc` trigger self-reinforcing.
Fix direction: RAII decrement (scope guard) in the job lambda; bounded wait + detach/log in the destructor, mirroring the handshake authority's stop policy.

---

## MEDIUM

### M1 — No column unloading: memory grows monotonically with explored area (quantified)
**VERIFIED.** `chunk_streamer.h:20`: "columns are never unloaded, so memory grows with explored area" — confirmed: no eviction exists anywhere in the streamer/world layer; `Dimension::unload` runs only on dimension switch/exit.

Per present `SubChunk` (4096 voxels, `chunk.h`/`voxel_palette.h`): palette indices 4 KiB at W8 (8 KiB at W16) + `state` 8 KiB (`array<CellState,4096>`) + `materials` 4 KiB ≈ **16–20 KiB**, plus palette/`indexOf_` overhead. Measured wafer columns average ≈3.1 present sub-chunks (112 subs / 36 columns in the selftest load) → **≈55–65 KiB per column** of voxel data alone; organic columns are comparable or higher. On top of that, the renderer retains a mesh per meshed sub-chunk until dimension switch, and two more structures grow monotonically: `ChunkStreamer::requested_` (never erased, `chunk_streamer.cpp:52`) and `TraceStrengthBuffer` slots (one per distinct trace cell ever propagated, never freed within a dimension session).

Defaults (`game.h:67`: `worldChunks = 16, tickRadius = 8`) pre-load 256 columns and the streamer maintains a 17×17 = 289-column area *plus all travel history*. Straight-line sprint exploration (≈5.9 new columns/s through a 17-wide strip) adds on the order of **350 columns/min ≈ 20+ MB/min of voxel data**, i.e. **>1 GB per hour** of continuous exploration, before meshes. This is a known deliberate first pass, but it is the binding constraint on long sessions and long-lived worlds.
Fix direction: distance-based eviction of clean columns (save-on-evict for dirty ones) with mesh/signal teardown hooks; at minimum, bound `requested_` by forgetting evicted keys.

### M2 — Streamed insertion happens in one sim-stopped window: whole batch + per-sub-chunk snapshots on the main thread
**VERIFIED.** `game.cpp:681-711` (`pumpStreaming`, called every frame from `Game::update`): when any column is ready it does `sim_->stop()`, inserts **all** ready columns (up to the 32-in-flight cap), synchronously builds a `snapshotForMeshing` for every present sub-chunk (≈29 KB padded copy + palette resolve each; ≈100 snapshots for a full batch), then `sim_->start()`. There is no per-frame insertion budget. `sim_->stop()` also joins the sim thread mid-step, so the stall includes up to one residual step interval. Additionally, `SimThread`'s tick counters are `run()`-locals, so every stop/start restarts them — the worldTick-reset effect already documented as F1 in the 2026-10-12 powers-of-two analysis; game consumers resync defensively, but keyed streams keyed to worldTick restart on every streamed batch.
Fix direction: insert a bounded number of columns per frame; move snapshotting off the stop window (columns are immutable once inserted — snapshot after restart is unsafe only if the sim can edit them, so snapshot-then-insert or a per-column handshake is needed); long-term, the publication model from the powers-of-two analysis removes the stop entirely.

### M3 — `WorldSave::loadColumn` remap failure latches for the whole session and silently downgrades saved columns to fresh generation
**VERIFIED.** `world_save.cpp:66ff`: `loadColumn` checks a one-time `streamRemapFailed_` latch under `streamRemapMutex_`; a single failed stable-id remap read makes every subsequent streamed load return `nullptr`, which the streamer interprets as "never saved" and generates fresh terrain (`chunk_streamer.cpp:60-61`). It is logged once as an error but is indistinguishable in behavior from an empty save slot. Combined with H1's save path, if the player edits the regenerated copy of a previously saved column and that column is later written, the earlier saved work in it is overwritten by the fresh-terrain version.
Fix direction: distinguish "no save" from "save unreadable" in `loadColumn`'s result; on unreadable, fail loudly per column (and do not mark the key as satisfactorily handled) rather than latching a silent session-wide downgrade.

### M4 — `TraceStrengthBuffer` full-pass coverage diverges from incremental; mesher slot lookup uses a different key space (latent, pre-wiring)
**VERIFIED (latent — no production consumer today).** In the full pass, the buffer is written at exactly one site, inside the BFS link-expansion lambda (`signal.cpp:694`). Seed cells, Pulsor pulse-flood cells, and cells whose strength falls to 0 without being re-propagated never update the buffer, so stale strengths persist indefinitely. The incremental path instead records **every** changed cell (`signal.cpp:856`, replayed by `hsApplyStrengthPending`), including transitions to 0. The two modes therefore leave different buffer contents for identical world state.
Two further latent inconsistencies in the same seam: the mesher looks slots up with a *local* key — `mesher.cpp:170-173`: `key = (x<<32)|(y<<16)|z` from sub-chunk-local coordinates — while the sim keys slots by world `packPos` (`signal.cpp:694`); and `TraceStrengthBuffer::slotOf()` returns 0 for "no slot", indistinguishable from real slot 0. Today this is all inert: no caller ever sets `MeshContext::strengths` (verified by grep over all `MeshContext{...}` constructions in engine+game; the mesher branch is guarded by `ctx.strengths`), and the visible path uses the baked strength. It will bite the moment the planned GPU buffer path ("Step 1… for future shader", `mesher.cpp:166-169`) is wired.
Fix direction: before wiring, unify on `packPos` keys end-to-end, make the full pass record the same changed-cell set as incremental (reuse the pending-list mechanism), and give `slotOf` a sentinel (e.g. `UINT32_MAX`) for "no slot".

### M5 — Chunk-border structure replay can disagree between owning and replaying chunks (claims overstated)
**VERIFIED.** `engine/src/procgen/stages.cpp:804-813` (`forBases`, used by TraceTree/CapacitorCan/ResonatorSpire at stages.cpp:972/1018/1042): margin bases from neighbour chunks are gated using the **clamped** in-chunk column (`lx = std::clamp(bx, 0, n-1)`) for biome weights, surface block, and `surfY`. At a simultaneous chunk+biome border the replaying chunk can therefore see a different dominant biome/surface than the owning chunk and replay (or skip) a structure the owner decided differently — e.g. branch/canopy cells without their trunk. `StructureFeature::place` and `LithoStampFeature::place` use the same clamped-column discipline for margin bases. The code comment at stages.cpp:797-802 now documents this honestly ("Known approximation… Rare, bounded to `reach` columns of the border, and fully deterministic"), and per-chunk output *is* fully deterministic — but the broader header claims that neighbours simply "agree" are **OVERSTATED**: agreement holds except at chunk+biome borders, where partial structures are possible by construction.
Fix direction: long-term, probe true biome/height by world position for margin bases (the pipeline already exposes world-position probes for biomes via `biomeAt`); short-term, align the feature-header wording with the `forBases` comment.

---

## LOW

### L1 — Component iteration order is unordered; broadcast/sound event *order* is nondeterministic across runs
**SUSPECTED (impact-bounded).** `tickComponents` and the propagate seed loops iterate `comps_` (an `unordered_map`). All state outcomes are order-independent (two-phase compute/apply, per-component cells), and `RadioRegistry::query` resolves canonically (strongest → nearest → lowest `packPos`), so world state is deterministic. What varies is the *order* of `radio_.publish()` calls and of the `sounds_` list, i.e. audible event ordering/polyphony allocation, not results. Fix: iterate a sorted key snapshot where externally visible ordering matters, or accept and document.

### L2 — `packPos` aliases beyond ±1,048,576 blocks on x/z
**VERIFIED (code), SUSPECTED (reachability).** `chunk.h:79-85`: `packPos` packs `(x + 1<<20) << 31 | (z + 1<<20) << 10 | y`. Coordinates outside ±2²⁰ wrap into neighbouring key space, colliding signal indexes. All bounded dimensions sit far inside this, but the Void is designed as infinite; a player travelling past ~1M blocks there would corrupt the signal index (and several `packPos`-keyed structures). The streamer itself imposes no bound. Fix: document/assert dimension bounds against the `packPos` range, or widen the packing before the Void ships.

### L3 — `loadBiomeDensity` scalar-`density` footgun remains (regression M7)
**VERIFIED.** `generator_loader.cpp` (`loadBiomeDensity`): `out.fill(float(j.getNum("density", 0)))` fills all 256 biome slots with the scalar before per-biome overrides. `wafer.json:2098,2127` still use `"density": 0.9` for the litho stamps; it is benign today only because their `biomes` maps enumerate all five wafer biomes (the rest at 0.0). Any future biome added to the wafer silently inherits 0.9 stamp density everywhere. Fix: require the `biomes` object when a nonzero scalar density is present, or fill with 0 and treat scalar `density` as an explicit all-biomes opt-in warning.

### L4 — River "chunk-pure" test is still tautological (regression, 2026-10-11)
**VERIFIED.** `engine/src/procgen/procgen_selftest.cpp:2484-2490`: the check generates chunk (5,5) solo, then in a 3×3 loop compares only the (5,5) hash against the solo hash — two independent generations of the same chunk. Generation has no cross-chunk state by construction, so the test cannot fail; it does not test what its name claims. Fix: generate the 3×3 into one world with a carver whose reach crosses borders and compare the assembled result against per-chunk generation (the worm/cavern replica checks nearby do this properly).

### L5 — `LightEngine` has no production callers and `addSource` does not clamp to `kMaxLight`
**VERIFIED.** `lighting.h:8`: `kMaxLight = 64`. The BFS decay math in `light_engine.cpp` is correct (decay 1 + opacity/16, opacity ≥ 64 fully opaque, signed arithmetic — the 2026-10-11 phantom-light fix is present), but `addSource` accepts `level > 64` unchecked, and grep finds no production callers of `LightEngine` at all (only comment references). Either wire it with a clamp, or mark it explicitly as staged code so the 1..64 lighting invariant isn't assumed to be enforced somewhere it isn't.

### L6 — `StructureFeature::basesInRect` over-reports: gates omitted (regression, Full-Tree)
**VERIFIED / STILL-OPEN.** `stages.cpp:1330ff`: `basesInRect` applies only the biome gate (`gateRoll >= s_.biomes.at(biome)`) plus footprint/rect overlap. `place()` additionally applies height-band, slope, water-proximity, anchor, and surface-material gates. Any overview/MCP consumer of `basesInRect` will report structures that worldgen will never place. Fix: share the gate chain between `place()` and `basesInRect`, or rename/document it as "candidate bases".

### L7 — Failed streamed columns are never retried within a session
**VERIFIED (deliberate policy, flagged for cost).** `chunk_streamer.cpp:52` inserts the key into `requested_` *before* the job runs; a load/generate failure logs "left empty" and the key stays requested forever (header states this is deliberate first-pass policy). A transient IO error therefore leaves a permanent hole in the world until restart — and with M3's latch, one bad remap read holes every not-yet-loaded saved column. Fix: erase the key on failure (with a retry cap/backoff) so transient faults heal.

### L8 — Failing "mesher produces quads for a surface sub-chunk" selftest is a test bug, not a mesher bug (root-caused)
**VERIFIED by probe.** `game/LithoKraft/src/app/selftest.cpp:86-100,280-282`: the test computes `surfaceSy` as the maximum over **all** columns, then meshes fixed column (2,2) at that height. Probe against the built libraries (wafer load(6)): global max `surfaceSy = 17` (resonator-ridge/spire columns, e.g. (0,4), which meshes 68 verts at sy 17), but column (2,2) has sub-chunks only at sy 14–16; meshing it at sy 17 correctly yields 0 verts, and at its own surface sy 16 it yields 2228 verts. The mesher and the section allocation are behaving correctly — spires legitimately push some columns to a 4th section (the neighbouring check at selftest.cpp:132 already accommodates this and passes). Fix: use column (2,2)'s own top sub-chunk (or a column attaining the max) in the mesher check.

---

## Claim verification

| Claim (source) | Verdict | Evidence |
|---|---|---|
| "The wafer is seed-independent" (`dimensions.cpp:39-41` comment; README design) | **HOLDS** | `makeGenerator`: `const uint64_t seed = (id == "wafer") ? uint64_t{0} : settings.seed;` and that seed is passed on **both** the pack-JSON path and the built-in fallback path. Downstream `ColumnGenerator::create` derives `dimensionSeed(worldSeed, dimensionId)` once; nothing re-seeds. Only production caller chain is `Game` → `resources.worldgen(cfg.seed)` → `createDimensions`. Selftest confirms empirically: seed 12345 changes organic, `waferDiff == 0` (PASS in my run). |
| Features are "deterministic regardless of generation order" / neighbours agree (feature headers, stages.h) | **HOLDS per chunk; OVERSTATED across chunk+biome borders** | All placement decisions are position-keyed (`posSeed`/`posRoll`/`cellSeed`); canopy variance uses an independent position-keyed stream (`stages.cpp:839-844`). But margin replay uses clamped columns (M5), so owning/replaying chunks can disagree at biome borders. |
| Handshake mode ≡ legacy tick (hs-design) | **HOLDS (tested scope)** | Phase mapping verified in code: affine prologue (flip@0, components@1, mark+prep@2) runs **before** publish (`timing_handshake.cpp` `runPhase`: "Affine work runs BEFORE the phase is published"), net shards pull @2, lamp shards pull @3, commit replays strength-pending (sub 2) and lamp dirties in shard order = globally ascending (sub 3); `phasesPerCycle < 4` falls back to legacy. 256-tick scenario hash identical across legacy / w1 / w4 / p4 / p8 in my run. Scope note: one scenario; equivalence is empirical, not proven for all circuits. |
| "Disk first: a column the player edited in an earlier session must come back as saved" (`chunk_streamer.cpp:58-60` comment) | **OVERSTATED** | Blocks come back as saved; the *signal simulation* of that column does not (H1) — circuits are dead and component memory is dropped. |
| Per-net pending-strength replay preserves legacy order (hs-design §3) | **HOLDS** | `hsPrepIncremental` records seeds/pulses in legacy visit order; `hsApplyStrengthPending` replays in (net ascending, then cell) order on the sim thread; `hsPropagateNet` touches only net-local state (thread-local buckets, per-net pulse stamps) and never the shared `TraceStrengthBuffer`/buckets — the deferred-replay design around the buffer's non-thread-safe `slotFor`/`set` is sound. |
| Red/blue channel isolation ("red never meets blue") | **HOLDS** | Channel checks in `seed()` (both paths), `wireLinks()`, and the full-pass `forEachLink()`; colourless (channel 0) sources seed both, by design. Vias link only vertically, same channel, lossless — in both flood implementations. |
| Signal/light range 1..64 | **HOLDS internally; NOT enforced at trust boundaries** | All internal writers stay in range; BFS decay −1/block, strongest-first, pulse reach `dist < kMaxSignal`. But imported/disk values are unclamped (H2), and `LightEngine` is unwired (L5). |
| Resonator spires vs section allocation (wafer) | **HOLDS (no bug found)** | `ColumnGenerator::generate` allocates a `SubChunk` only for non-empty 16-block layers; spire place() height guard `surfY + h >= top` present; wafer ridge (254 + ridged×16) + spire (≤16) tops out ≈286 « 768. The related selftest failure is the test's fault (L8). |

## Positive verifications (checked, sound)

- Handshake ordering: marked nets ascending via bitset scan; lamp keys `std::sort` ascending; lamp dirty replay in shard order over contiguous sorted slices = globally ascending; radio/jammer flips at cycle **end** in both modes (D12). Skip path's use of the previous cycle's mark bitset is deliberate and commented.
- Wire flood correctness at chunk borders: nets are built globally over the loaded map (`buildNets` sorts wires by `packPos` before assigning ids — ids are edit-history-independent); CSR links cross chunk borders inside the tick area; out-of-area wires are frozen consistently in both paths.
- `RadioRegistry::query` tie-break is canonical (strongest → nearest → lowest `packPos`); `buildNets` id assignment is sorted. Both 2026-10-11 fixes still present.
- Canopy per-instance variance: separate position-keyed stream (`deriveSeed(featSeed, nameHash("trace_tree_canopy"))` + `posSeed`), never perturbs trunk/branch RNG; radii clamped to the documented maxima that `reach()` covers. No `rand()`/time/unordered iteration found feeding worldgen or signal world state (grep over procgen/world/streamer/signal TUs).
- Shrubs (`GroundCoverFeature::place`, `diversity.cpp:55`): single position-keyed roll against cumulative blended density in vector order — deterministic; surface-material and height guards present.
- Biome blending/palettes: softmax blend weights with (distance, index) tie-break; game-side wafer surfaces (`wafer_biomes.cpp`) are pure hash functions of world position — seamless across chunk borders by construction.
- Wafer seed pinning: verified end-to-end at the wiring layer (see Claims).

---

## Regression check (prior reports)

| Prior finding | Verdict | Evidence |
|---|---|---|
| Evening C1 — `lithokraft_selftest` segfault; missed/misordered `setRegistry` sites in `signal_selftest.cpp` | **FIXED-CONFIRMED** | Every `ChunkMap` in the current `signal_selftest.cpp` is followed by `setRegistry(&blockRegistry())` (grep-verified pattern throughout); my full selftest run completes the entire signal section with no crash and no signal FAILs. |
| Evening H3 — Litho Formula node polar axis swapped vs voxelizer | **FIXED-CONFIRMED** | `litho_formula_node.cpp:131-132`: `phiA = asin(clamp(qy/qlen))`, `thetaA = atan2(qz, qx)` (polar axis = world Y, matching the voxelizer's formula→grid mapping). Regression test now exists and is non-spherical: `voxelizerAgreementChecks` in `litho_formula_node_selftest.cpp` (starfish preset, occupancy agreement ≥ 0.95 + per-axis surface radii). `engine_litho_formula` and gallery tests pass. |
| Evening M7 — scalar `density` fills all biome slots | **STILL-OPEN** | See L3. Loader unchanged; wafer.json still relies on full enumeration. |
| Evening M11 — per-instance Formula variety weak | **STILL-OPEN for `LithoStampFeature`** (cells fixed; position + quarter-turns only). Note: trace-tree *canopies* separately gained genuine per-instance variance (position-keyed archetype/parameter stream) — the stamp path did not. |
| Full-Tree — `basesInRect` "overview maps lie" | **STILL-OPEN** | See L6. |
| Full-Tree — margin-replay disagreement at chunk borders | **INCOMPLETE** | Behaviour unchanged; the limitation is now documented in the `forBases` comment (stages.cpp:797-802). The suggested fix (true world-position probes for margin bases) is not done. See M5. |
| Full-Tree — biome-transition in-place mutation asymmetry (`diversity.cpp`) | **FIXED-CONFIRMED** | `BiomeTransitionStage::process` snapshots `before = ctx.maps.biome` and reads in-chunk neighbours from the snapshot, out-of-chunk from the un-transitioned probe. |
| 2026-10-11 — wireless tie-break canonical; wire-net ids from sorted `packPos` | **FIXED-CONFIRMED (still present)** | Verified in current code (see Positive verifications). |
| 2026-10-11 — river "chunk-pure" test tautological | **STILL-OPEN** | See L4. |
| Morning/Evening — 3 caves checks failing/unreachable (worm replica, floating debris, branched forks) | **FIXED-CONFIRMED** | These live in the engine procgen suite; my full `litholattice_tests` run shows all cave checks PASS and 0 FAIL overall. |
| Known — "mesher surface sub-chunk" failure | **ROOT-CAUSED: test bug** | See L8. Mesher and generator are correct; the check meshes a fixed column at another column's height. |
| Known — 4× save round-trip failures | **STILL-OPEN (storage reviewer's scope)** | Reproduced in my run (same 4 checks). Note for that reviewer: the two "ticks identically after load" failures are downstream composites of the initial load mismatch (`sameCells`/`exportState` equality is a precondition inside the same checks), not independent signal-solver divergences — the signal sections proper, including handshake equivalence, all pass. |

## Notes / minor observations

- `hsPropagateNet` pulse stamps: `pulseGen` is a per-net `uint32_t` incremented per pulse; wraparound needs 2³² pulses on one net (≈97 days at 512 TPS continuous) — theoretical only.
- `packPos` y-field is 10 bits; world height 768 fits with margin.
- `ChunkStreamer`'s ≤32-in-flight cap and nearest-first request order work as documented; `JobPool` capacity sharing with meshing is respected because insertion, not generation, is the serial bottleneck (M2).
- `Dimension::load` inserts pre-bake columns in completion (nondeterministic) order into the `ChunkMap`; harmless — columns are independent and setup runs after all inserts.

---
## Appendix C — Save/Load & World Permanence

# Code Review 2026-10-13 — Save/Load & World Permanence (read-only)

Scope: `engine/src/storage/`, `game/LithoKraft/src/game/world_save.cpp`, region files,
`player.dat`/PlayerMeta, block-name table / registry ID persistence, MATS material
persistence, LBT, migration/versioning. Method: full read of the code + a live
`LithoKraft --selftest` run in `build-linux` + two purpose-built diagnostic harnesses
(in `/tmp/review-2026-10-13/diag*.cpp`, linked against the built libs, tree unmodified)
+ `ctest -R "storage|engine_sim"` (both pass).

**Counts: CRITICAL 1 · HIGH 3 · MEDIUM 5 · LOW 6 · INFO 2**

---

## HEADLINE — Root-cause of the 4 known "save/restore round-trip bit-for-bit" selftest failures: VERIFIED, and it is NOT data loss

Failing checks (live run, `game/LithoKraft/src/app/selftest.cpp:184,186,252,254`):
`save: blocks, cell states and component memory / timers restore bit for bit`,
`save: the restored world ticks identically…`, `save: save/edit/save/edit+tick/save…
matches bit for bit`, `save: the partially saved world ticks identically after load`.

**Diagnosis (VERIFIED by instrumentation, both scenarios replicated outside the tree):**
the save/load data path is semantically perfect; the *test comparator* is
representation-sensitive.

- Full-save scenario: all 36 columns load (`n == 36`), `SignalSim::importState`
  succeeds, `sig2.exportState() == stateBefore` byte-for-byte, **zero** per-voxel
  StableId diffs, **zero** cell-state diffs, **zero** sub-chunk existence diffs.
  The only diffs: 12 columns' sub-chunk 16 (the placed demo-circuit/structure
  sub-chunk) whose `VoxelPalette` contains the *same entries in a different order*.
- Partial-save scenario: same result — 10 sub-chunks palette-order-only diffs,
  0 semantic / 0 state diffs, signal export equal before and after 40 more ticks.

Mechanism:
- `sameCells` (selftest.cpp:146-160) compares `x->blocks != y->blocks`, i.e.
  `VoxelPalette::operator==` (`engine/src/world/include/litholattice/world/voxel_palette.h:150-153`),
  which compares `width_`, the **palette vector in insertion order**, and the raw
  index data — not the resolved voxel values.
- The original palette is built in worldgen/structure-placement order. The loaded
  palette is rebuilt by `storage::remapColumn` (`block_names.cpp:88-105`) calling
  `blocks.set(i, sid)` in voxel-scan order (and the BLKS disk palette itself is
  written in first-encounter scan order, `storage.cpp` `encodePalette`). Any
  sub-chunk whose blocks were placed out of scan order (structures, demo circuit,
  hand edits — exactly sub-chunk 16 and the edited columns) therefore compares
  unequal despite identical voxels. Terrain-only sub-chunks, generated in scan
  order, pass — which is why only these checks fail.
- Fix direction (test-side, preferred): make `sameCells` compare per-voxel
  `blocks.at(i)` + `state`. Alternatives: canonicalise palette order at save
  encode, or give `VoxelPalette` a semantic equality — but `operator==` as a
  representation equality is legitimately useful elsewhere; do not silently
  change its meaning. Note the one real (cosmetic) consequence: re-saving a
  loaded world does not reproduce the original file bytes (palette order
  differs), so save files are semantically stable across a round trip, not
  byte-stable.

---

## CRITICAL

### C1. `blocks.names` is a single point of total world loss: no fsync, no backup, and a corrupt table makes loading fail *open*
`engine/src/storage/block_names.cpp:11-34` (`writeBlockNames`), consumed at
`game/LithoKraft/src/game/world_save.cpp:96-115` (`loadDimension`) and `:71-90` (`loadColumn`).
VERIFIED:
```cpp
std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
f.write(...); f.close();
if (!f) { ... return false; }
std::filesystem::rename(tmp, file, ec);
```
- This is a hand-rolled tmp+rename, **not** `writeFileAtomic`: no fsync of the
  file, no fsync of the directory, **no `.bak` generations, no
  `recoverCrashedWrite`**. Every other engine file goes through the file
  contract; the one file that interprets *all* region data does not.
- It is rewritten on **every** `saveDimension` (world_save.cpp:141), maximising
  exposure. After a power loss, POSIX gives no durability for the renamed file's
  contents without fsync — the table can come back empty/torn even though the
  write "succeeded" seconds earlier.
- On CRC/magic failure `readStableRemap` returns nullopt, and the callers fail
  **open**: `loadDimension` logs and returns 0 — "generated terrain kept"
  (world_save.cpp:103) — and `loadColumn` sets `streamRemapFailed_` and returns
  nullptr for every column (world_save.cpp:77-82). The intact region files are
  simply never read. The player sees a regenerated world; playing on and saving
  then progressively overwrites region columns with generated terrain (and a
  fresh table), converting a one-file tear into permanent world loss.
- Fix: write the table via `writeFileAtomic` (fsync + 8 backups +
  recoverCrashedWrite on read, validating magic/version/CRC as the
  `regionImageIntact`-style validator); on an unreadable table with regions
  present, fail **closed** (refuse to load/save the world and quarantine,
  exactly as `RegionFile` refuses to overwrite a broken region) instead of
  regenerating over it.

## HIGH

### H1. A failed region flush is swallowed and can never be retried; the game then reports the save "on disk"
`engine/src/storage/storage.cpp:275-295` (`RegionFile::flush`), `:430-447`
(`WorldStore::flushAsync`), `game/LithoKraft/src/game/game.cpp:661-674`.
VERIFIED:
```cpp
for (auto& [s, b] : pending_) { crcs_[s] = crc32(...); records_[s] = std::move(b); }
pending_.clear();
...
return writeFileAtomic(path_, w.out, err, opts);
```
- `pending_` is merged into `records_` and cleared **before** the write. If
  `writeFileAtomic` fails (disk full, I/O error), `flush` returns false but a
  later `flush()` early-returns `true` on `pending_.empty()` — the failed
  columns are never rewritten this session (only columns dirtied *again* get a
  new `put`). In-memory `read()` serves the new records, masking the loss until
  the next process start silently resurrects the old region.
- The failure signal dies immediately: `flushAsync` only
  `std::fprintf(stderr, "storage: %s\n", …)`; `WorldStore::flush()` returns
  void; `Game::saveWorld` returns `true` unconditionally and logs
  `", on disk"` when `wait=true` (game.cpp:672-673). An exit save on a full
  disk reports success.
- Fix: only clear `pending_` after a successful write (or re-queue on failure);
  propagate a failure count/flag out of `flushAsync`/`flush()` and have
  `saveWorld` surface it (logError + return value) instead of claiming "on disk".

### H2. The world seed is never persisted — non-wafer dimensions regenerate different terrain under saved columns after a relaunch with a different/no `--seed`
`game/LithoKraft/src/app/main.cpp:474-477` (seed only from `--seed`),
`game/LithoKraft/src/game/include/lithokraft/game/game.h:66` (`seed = 0` default),
`game/LithoKraft/src/game/dimensions.cpp:39-41`, `world_save.h` (dirty-only saves),
`engine/docs/SAVE_FORMAT.md` (`level.json` still **[planned]**).
VERIFIED: there is no level/world metadata file at all; `player.dat` stores
position/dimension/hotbar/stats/vitals but no seed. The wafer is immune (pinned
to seed 0), but `organic` uses `settings.seed`. Because production saves are
dirty-only (`Game::saveWorld` passes `&dirty`), every untouched column is
*regenerated from the seed on load by design*. Concrete scenario: play organic
with `--seed 42`, exit; relaunch without `--seed` → seed 0 → all unsaved
columns regenerate as a different world, with the saved (edited) columns
embedded in mismatched terrain; subsequent dirty saves bake the corruption in.
Fix: persist the seed (and format version) in a small world-metadata file
written through the file contract (or a `player.dat`-sibling `level` record),
read it before dimension creation, and refuse/warn on a CLI seed that disagrees
with an existing save.

### H3. `player.dat` bypasses the file contract: tmp+rename with no fsync and no backup — one torn write loses position, hotbar, stats and vitals
`game/LithoKraft/src/game/world_save.cpp:172-211` (`writePlayer`).
VERIFIED: `std::ofstream` write + `std::filesystem::rename`, no fsync of file or
directory, no `.bak`, no `recoverCrashedWrite`. The CRC (line 189-196) detects
a torn file on load — and then the data is simply gone (`readPlayer` →
nullopt, game starts at spawn, game.cpp:428-429). This file now carries the
v2 stat block and v3 vitals block, i.e. character progression, not just a
respawn point. Fix: route through `writeFileAtomic` with backups and promote
the newest intact generation on read, as regions and mod save sections already
do. (Detection-without-recovery is half the permanence doctrine.)

## MEDIUM

### M1. (Test integrity) The 4 round-trip selftest failures — see HEADLINE.
The checks as written can never pass for structure-bearing worlds, so the save
section's red state trains everyone to ignore it — and would mask a *real*
future round-trip regression. Fix the comparator to semantic equality
(per-voxel `at()` + `state`); keep a separate representation check if byte
stability is ever wanted. Severity medium: the shipped data path is verified
sound; the safety net is not.

### M2. `player.dat` hotbar stores raw `BlockId`s and is never remapped through the name table
`world_save.cpp:182,229-230` (write/read `u16` hotbar ids);
`game/LithoKraft/src/game/game.cpp:438-439` applies them with only a range
check (`savedPlayer->hotbar[i] < blockRegistry().size()`). VERIFIED.
World blocks are protected by `blocks.names` (save id → string → current id),
but the hotbar is not: reorder/insert registration (a mod loading earlier, a
new vanilla block inserted mid-list against the append-only convention) and
every hotbar slot silently points at a *different, in-range* block. Renames
without an alias do the same. Stats/vitals in the same file are correctly
keyed by name — the hotbar is the inconsistent exception. Fix: store hotbar
entries as string ids (new player.dat version) or remap through
`readBlockRemap` on load.

### M3. `RegionFile::open` reads the whole region with no size cap
`engine/src/storage/storage.cpp:238`:
`const Bytes all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());`
VERIFIED. The engine has a fail-closed cap discipline (`kMaxWholeFileRead`,
64 MiB, in `file_contract.h`, used by `readFileAll`/`SandboxedFile`), but the
region loader — the parser facing the largest, most complex on-disk structures
— has none. A bloated/corrupt/hostile region file (or a runaway backup
restored by mistake) is slurped whole and kept resident in `records_`.
`regionImageIntact` (storage.cpp:31-67) does bound the *index* against file
size during recovery, and `open` bounds index entries against the byte vector
(storage.cpp:252-256), so there is no OOB — the exposure is memory exhaustion,
plus unbounded load latency. Fix: stat the file, refuse above a sane cap,
stream index-then-records instead of whole-file residency if regions grow.

### M4. Streamed `loadColumn` decodes feed the shared SGNL sink `imported_` — residue and a cross-path race (SUSPECTED)
`world_save.cpp:44-48` (SGNL `load` lambda appends to `imported_`),
`:64-94` (`loadColumn`), `:116-118,155` (`loadDimension` clears/moves it).
The header (world_save.h:71-76) promises a streamed column's SGNL payload "is
NOT imported" — true only because `loadDimension` clears the sink before its
own loads. But every streamed decode still appends into the shared vector, and
nothing serialises that against a concurrent `loadDimension`'s
clear/move-out on another thread: a streamed append landing between
`loadDimension`'s `s.flush()` and its `signalState = std::move(imported_)`
would be swept into the live sim's import blob (duplicated/foreign component
entries; `importState` ignores keys with no component, which bounds the blast
radius), and residue otherwise accumulates unboundedly across streamed loads
until the next `loadDimension`. Fix: give the decode a per-call sink (or have
the SGNL load lambda no-op unless an import is armed).

### M5. Unknown blocks vanish silently on the streaming path
`world_save.cpp:96-115` logs `remap->unknown` ("unknown block '%s' loads as
air") in `loadDimension`, but `loadColumn` (`:71-90`) caches the remap without
ever logging `unknown` — in pure streaming play, blocks removed/renamed
without an alias become air with no diagnostic anywhere. (The air-mapping
itself is the documented open fork #3 in `block_names.h:44-46`; the missing
log is the defect.) Fix: log the unknown list once when the cached remap is
built. Same gap for the `legacy` notice, which `loadColumn` does log
(world_save.cpp:84) — so the omission looks accidental, not designed.

## LOW

### L1. `readPlayer` robustness nits
`world_save.cpp:213-261`. VERIFIED: (a) trailing garbage after the vitals
block is accepted (`b.done()` never required, unlike `PayloadRegistry::decode`
and `mod_save_data`'s `decodeSection`, which both require it); (b) the file is
read with no size cap (cf. M3); (c) write side truncates silently:
`uint8_t(m.hotbar.size())` and `uint16_t(m.stats.size())` if a future caller
exceeds them. All fail-safe today (CRC gates the body); hygiene only.

### L2. `recoverCrashedWrite` overwrites the previous forensic `.torn` copy — STILL-OPEN (see regressions)
`engine/src/storage/file_contract.cpp` (promotion step): the torn primary is
renamed to `<path>.torn` unconditionally; a second torn primary destroys the
first one's evidence. POSIX rename replaces silently. Fix: suffix/rotate the
forensic copy or skip if one exists.

### L3. `AnvilRegion::readHeader` still silently "normalises" corrupt index entries to absent — STILL-OPEN
`engine/src/storage/anvil_region.cpp` `readHeader`: entries failing bounds
checks are zeroed with no log, no error, no quarantine count — indistinguishable
from never-written chunks. (The payload-first commit, per-record CRC in
`parseRecord`, sidecar caps and the in-place-shrink fix are all sound.) Note
also `AnvilRegion` still has **no production callers** (grep: only tests and
its own module); the live format is `RegionFile`. Either wire it in or keep it
explicitly experimental — today it is a second, divergent region implementation
carrying its own review surface.

### L4. No-registry BLKS save path truncates W64 StableIds to u16 — STILL-OPEN (documented unreachable)
`storage.cpp` BLKS `save`, no-registry branch: `tmp[i] = uint16_t(sub->blocks.rawIndex(i))`
— at W64 width `rawIndex` *is* the 64-bit StableId. The with-registry W64
branch handles it correctly (per-voxel `findByStableId`, unknown → air). The
comment calls the path tests/tools-only and "degenerate… unreachable in
practice"; it remains a silent-corruption trap for any future tool that saves
a W64 sub-chunk without installing a registry. Fix: fail loud (assert/empty
record + error) instead of truncating.

### L5. `SAVE_FORMAT.md` has drifted from the code
`engine/docs/SAVE_FORMAT.md`: `player.dat` documented as version 1 (code:
v3, `world_save.h` `PlayerMeta::kVersion = 3`, with v1/v2 read support);
region backups documented as "one last-known-good backup (`<file>.bak.1`)"
(code: default depth 8, `file_contract.h` `kBackupDepthDefault`); crash-safety
paragraph omits `recoverCrashedWrite`. In a permanence-critical format, the doc
is part of the recovery tooling — keep it exact.

### L6. `loadDimension`'s return count overstates success
`world_save.cpp:148-157`: columns dropped by a failed `remapColumn` are
`continue`d past `world.insert` but remain counted in `loaded`, which is what
the function returns. Callers/tests reading "N columns loaded" get the
pre-remap number. Log lines do disclose the drops; the count should too.

## INFO

### I1. LBT (LithoBinary Tagged) does not exist in the tree — nothing to audit
Repo-wide grep for `LBT` / `LithoBinary` over `engine/src`, `game/`, `docs/`:
zero hits. There is no LBT parser, no LBT writer, and no persisted ChipOR
name field (the design item in memory). Robustness of LBT parsing is therefore
vacuously fine today; when it lands it should be built on `ByteReader` +
`frameRecord` like `mod_save_data`, not a new hand-rolled cursor. Flagging so
the absence is a decision on record, not an oversight discovered later.

### I2. CRC32 is corruption *detection*, not authenticity
Region records, `player.dat`, `blocks.names`, framed records and mod sections
all use IEEE CRC32, which any writer can recompute. That is the right tool for
bit-rot/tears (the stated threat), but saves confer no tamper evidence; if
shared/downloaded worlds ever become a thing, that needs a real MAC — noting
only so the property isn't assumed.

---

## What is sound (verified, not assumed)

- **File contract** (`file_contract.cpp`): tmp → fwrite → fsync(file) →
  backup rotation (depth default 8, powers of two 2–32) → atomic rename →
  fsync(dir); temp removed on every failure path; `readFileAll` pre-checks
  size and re-checks while streaming.
- **Crash-window recovery**: `RegionFile::open` runs `recoverCrashedWrite`
  with a fail-closed validator (magic/version + every index entry in-bounds)
  before reading; promotion *copies* (never moves) the newest intact backup,
  fsyncs the copy and its directory, and moves a torn primary aside to
  `.torn`. Defrag's `RecordStore::load` uses the same helper.
- **Per-record CRC verified on read** (`RegionFile::read`, storage.cpp) —
  mismatch → nullopt + error, never garbage. Index offsets/sizes bounds-checked
  against file size at open (u64 arithmetic, no overflow window).
- **Quarantine**: a region whose header/index is unreadable sets `broken_`;
  `flush` refuses to overwrite it forever (storage.cpp `flush`), `WorldStore`
  counts and logs it and keeps the world running on other regions.
- **Payload versioning**: record version refused if newer; per-payload
  migration chain walked v→v+1 with failure = whole-record refusal; unknown
  tags skipped (forward compat); payload newer than the build refused, never
  misread (`PayloadRegistry::decode`). SGNL v1→v2 migration is bounds-checked
  and unit-tested (selftest passes both the migration and malformed-input
  checks). Deterministic encode order (payloads in `std::map` tag order —
  which also guarantees BLKS decodes before MATS, as MATS requires).
- **Registry ID persistence**: on-disk BLKS ids are interpreted *only* through
  `blocks.names` (CRC-checked; missing table = documented legacy identity
  migration). `readBlockRemap` resolves through `findById`, which is
  alias-aware with a cycle guard (`block.h` `findBlock`) — verified against
  the rename aliases registered in `blocks.cpp` (pulsar→pulsor, red_wire→
  red_trace, …). Vanilla registration is append-only by convention *and* no
  longer load-bearing for world data, because the table, not the order, is
  authoritative; duplicate string ids are first-wins, duplicate StableIds
  rejected (`ContentRegistry::insertEntry`). `resolveId` at startup aborts
  loud on an unregistered string id rather than defaulting. Unknown saved
  names → air, listed in `remap->unknown` (but see M5).
- **MATS material persistence — the known gap is FIXED in production.**
  `main.cpp` always constructs the `MaterialRegistry` (built-in default even
  when the data file is missing) and sets `cfg.materials`; `Game` passes it to
  `WorldSave` (game.cpp:426), which registers `makeMaterialPayload`. The
  payload persists **string IDs**, caps the table at 256 with overflow as an
  error, maps unknown strings to the defined fallback, and refuses material
  data with no BLKS counterpart (`material_payload.h`). Engine tests cover a
  cross-session remap round trip (`engine/tests/engine_tests.cpp` MATS
  section; `ctest engine_storage` passes). The selftest log line
  "no PBR material registry; chunk material bytes will not persist" still
  appears because the *selftest* builds `WorldSave(dir)` with the default
  `nullptr` — a test-harness gap: no game-level test asserts a material
  round trip even though the engine-level one exists.
- **`player.dat` versioning**: v1/v2/v3 all read; newer refused with an error,
  never misread; stats/vitals keyed by **name** (the correct pattern — cf. M2);
  corrupted body detected by CRC (selftest passes).
- **Mod save data** (`mod_save_data.cpp`): framed CRC records via the file
  contract, inner version + mod-id verification, quarantine instead of
  overwrite, major-version mismatch disables the mod rather than misreading,
  orphaned sections kept on disk. This is the model the game-side files
  (C1/H3) should be brought up to.
- **Signal state**: `exportState` is deterministically ordered (`std::map` by
  packed position); `importState` is fully bounds-checked via `ByteReader`
  and applies nothing on a malformed blob (entries are staged, `!r.ok()` →
  false before any mutation).
- **Parsers generally**: `ByteReader` is bounds-checked throughout;
  `decodePalette` rejects 0/oversized palettes and out-of-range packed
  indices; BLKS/MATS reject present-masks with bits ≥ `kSubChunks`;
  Anvil `parseRecord` checks length against allocation before CRC.
  Endianness is explicit little-endian everywhere (`ByteWriter`, Anvil BE
  sector offsets per Anvil convention); floats in `player.dat` via memcpy bit
  patterns (IEEE-754 assumption, same as the rest of the tree).

## Regression check vs prior reviews

| Prior finding | Source | Verdict |
|---|---|---|
| Truncated block-name table renamed over the good one (stream checked before close) | Full-Tree 2026-10-12 | **FIXED-CONFIRMED** — `block_names.cpp:28-31` now closes explicitly and checks `!f` before renaming. (The separate fsync/backup/fail-open gaps are new finding C1.) |
| Anvil in-place shrink leaks tail sectors | Full-Tree 2026-10-12 | **FIXED-CONFIRMED** — `commit()` frees `oldSector + s.sectors … oldC` tail via `freeRun`. |
| IoScheduler dtor UAF (in-flight ops capture `this`) | Full-Tree 2026-10-12 | **FIXED-CONFIRMED** — `~IoScheduler` drains on `inflight_ == 0`; `~IoFence` aborts loud rather than UAF-ing (async_io.cpp). |
| Anvil `readHeader` silent normalise; no dir fsync after commit | Full-Tree 2026-10-12 | **STILL-OPEN** — normalise still silent (L3); dir fsync happens only at file creation in `open()`. No production callers bounds the impact. |
| Second torn primary overwrites the forensic copy | Full-Tree 2026-10-12 | **STILL-OPEN** — `.torn` rename is unconditional (L2). |
| AnvilRegion has no production callers | Full-Tree 2026-10-12 | **STILL-OPEN** (confirmed by grep again today). |
| No-registry save path assumes `rawIndex == BlockId`; W64 truncation | Evening 2026-10-12 (L-world2) | **STILL-OPEN** — code unchanged, still documented unreachable (L4). |
| `writeFileAtomic` crash window → `recoverCrashedWrite` wired into `RegionFile::open` and `RecordStore::load` | Code-Review 2026-10-11 fix list | **FIXED-CONFIRMED** — both call sites present and validator-backed (storage.cpp:227, defrag.cpp:267). |
| RegionFile backups at default depth 8 (`kRegionFileBackups` removed) | File-Hardening 2026-10-11 | **FIXED-CONFIRMED** — `RegionFile::flush` goes through `writeFileAtomic` with `FileWriteOptions` default `keepBackups = kBackupDepthDefault`. |
| `engine_sim` block-names narrowing failure | Morning Report 2026-10-12 | **FIXED-CONFIRMED** — `ctest -R engine_sim` and `engine_storage` both pass today. |
| "`lithokraft_selftest` never re-run; save/load coverage unverified" | Code-Review 2026-10-11 | **CLOSED BY THIS REVIEW** — suite run today; the 4 save failures are root-caused to the test comparator (HEADLINE/M1), not to save data. |
| "chunk material bytes will not persist" (no PBR registry) | known gap | **FIXED-CONFIRMED for production** (main.cpp → cfg.materials → WorldSave MATS registration + engine MATS tests); **STILL-OPEN in the selftest harness only**, which constructs `WorldSave` without materials and asserts no material round trip. |
| LFS1 vs LBT note (no LBT in tree) | Code-Review 2026-10-11 | **UNCHANGED** — LBT still absent entirely (I1). |

## Verification performed
- `build-linux/LithoKraft --selftest`: save section — player round trip PASS,
  damaged-player.dat CRC PASS, block-name table/legacy PASS, SGNL migration
  PASS ×2; the 4 round-trip checks FAIL (root-caused above). Other failures in
  the run (audio streaming, game-ui script editor, mesher quad, headless
  Vulkan) are outside this scope.
- Two diagnostic harnesses replicating selftest.cpp:134-186 and :236-256
  exactly, comparing semantic (per-voxel StableId) vs representation
  (palette) equality and signal export equality — outputs in
  `/tmp/review-2026-10-13/diag-out.log`, sources alongside.
- `ctest -R "storage|engine_sim"`: 2/2 pass.
- No file in `~/workspace/litholattice` was modified.

---
## Appendix D — Render Stack

# Code Review 2026-10-13 — Render Stack (read-only)

Scope: `engine/src/render/` (incl. `gl33/`), `engine/src/render_vk/`, `engine/src/sprite/`,
meshers, PBR, culling, presenter/selection in `game/LithoKraft/src/app/main.cpp` + `game/game.cpp`
(renderer-facing parts only). Method: code read; `engine_render` + `engine_sprite` ctest re-run
(both PASS, 1.57 s); `litholattice_tests` render sections re-run (all render/PBR/frustum checks PASS).
No files modified.

Counts: **CRITICAL 0 · HIGH 5 · MEDIUM 9 · LOW 8** (22 findings)

---

## HIGH

### H1. Ground traces still render nothing on every backend — 2026-10-11 finding STILL-OPEN, now with more dead wiring
- GL GLSL: `grep -c groundTrace engine/assets/shaders/chunk_shading.glsl` = **0** (only
  `groundPulseColor` at :32/:156). No `groundTraceColor()`.
- VERIFIED `gl33_renderer.cpp:389,504-505`: uniforms `uGroundTraceTime/uGroundTraceOn` are
  queried/set — locations are −1 against the shipped shader, silent no-ops.
- VERIFIED game side: no `render.ground_traces` cvar anywhere (`settings_cvars.cpp` registers only
  `render.ground_pulse` :140); `Game::frameParams` (game.cpp:1497-1502) sets `groundPulse*` but
  **never** `groundTraceTimeSec/groundTraceOn` — they stay at FrameParams defaults (0, true).
- Vulkan: `vk_frame_uniforms.h:27,43-44` declares `traceTime/traceOn/traceP0/traceP1`, but
  `vk_renderer.cpp writeUniforms()` never writes trace time/on, `Mesh` stores only `pulseP0/P1`
  (:245-246) and `drawChunks` builds `ChunkPush` with pulse only (trace fields zero), and
  `vk/frame.glsl`/`vk/chunk.frag` contain no trace code at all.
- CPU mirror `groundTraceEmissive()` (ground_traces.h:131) still has no production consumer.
- Impact: a claimed, mesher-baked feature (trace params ride every ChunkMesh) is 100% inert;
  docs/headers (`ground_traces.h:23-25`, `renderer.h:138`) describe it as live. Fix: land the GLSL
  mirror + cvar + frameParams wiring + VK push, or mark not-landed in headers/docs.

### H2. SoftwareRenderer does NOT implement the PBR/frustum half of IRenderer — silently inherits no-op defaults; it is the DEFAULT backend
- VERIFIED `software_renderer.h/.cpp`: overrides only name/deviceInfo/presentMode/textureArray/
  setMaterials/loadShaders/uploadChunkMesh/uploadEntityMesh/clearChunkMeshes/renderFrame/stats/
  requestGrab/grabFramebuffer. It does **not** override `supportsPbrPath()` (→false),
  `setPbrMaterials`, `setPbrTier`, `setAmbientMode`, `setFrustumMarginDeg`, `setCullProtoConfig`,
  `cullProtoStats`, `uploadPbrChunkMesh` — all inherit the do-nothing defaults in
  `renderer.h:225-246`.
- Game calls them unconditionally: `game.cpp:378-380,954-990,1344,1466`
  (`setPbrTier/setAmbientMode/setFrustumMarginDeg/setCullProtoConfig/setPbrMaterials/
  uploadPbrChunkMesh`). On software every call is silently discarded; BinaryQuad is at least
  gated loudly (`game.cpp:373-376` logError + fallback to PackedVertex — that part is honest).
- Its real PBR path (`setShadingTier/setPbrContext`, software_renderer.h:99-106) is **not** part of
  IRenderer and is never wired by the game (game.cpp:406 comment admits "software path's unwired
  setPbrContext"); worse, `varyingsFromPackedVertex` (software_renderer.cpp:238) hard-codes
  `s.materialId = 0xFFFF`, so even if wired, `pbrFragmentShader` falls back per-fragment to basic.
  PBR on the PackedVertex/software path is therefore unreachable end-to-end (GL BinaryQuad only).
- Impact: "full IRenderer backend" is OVERSTATED (see claims table). Fix: override the setters to
  store+apply (tier→SoftwareShadingTier, ambient→PbrFragmentContext) or log once that they're
  unsupported; wire setPbrContext in main/game.

### H3. Software mode has no HUD/menu/crosshair/highlight at all — and software is the default, menu-first path
- VERIFIED `software_renderer.cpp:510-511`: `// p.hud is not drawn: no HUD rasterizer on this path`,
  and renderFrame (:489-525) also ignores `p.highlight` and `p.crosshair`. Header admits it
  (software_renderer.h:44-45).
- `Game::frameParams` paints the full UI into `p.hud` every frame (game.cpp:1502, `ui_.paint(...)`),
  and `main.cpp:608` opens the launch menu first. In `--renderer software` (default,
  main.cpp:362) none of it reaches the screen: no menu, hotbar, inventory, crosshair or block
  outline — world only. GL33 (`drawHud`, gl33_renderer.cpp:110-120,637) and Vulkan
  (vk_renderer.cpp:592-603) both rasterize HUD.
- Impact: known gap, VERIFIED still current; on the default backend the game is effectively
  unplayable as a menu-driven app. Fix: CPU HudLayer rasterizer (buildHudVertices output is
  backend-neutral triangles + font atlas — rasterize those), or stop defaulting to software.

### H4. Vulkan silently drops sprites and traces; PBR absence is only implicit
- VERIFIED: `grep sprite engine/src/render_vk/` = **zero hits**. `renderFrame` never reads
  `p.spriteQuads/spriteAtlasPages`; no log line. Sprites simply don't exist on Vulkan.
  (Contrast GL33: full pass, gl33_renderer.cpp:152-202; software: flat form, H/M below.)
- PBR: Vulkan inherits the same IRenderer no-op defaults as H2 — no `supportsPbrPath` override
  (false is accurate), but `uploadPbrChunkMesh/setPbrTier/setPbrMaterials` vanish silently;
  mitigation: game-level logError at game.cpp:373 when BinaryQuad is *selected*. If PBR meshes
  are uploaded without that gate, they're lost without a trace.
- Traces: see H1. Entities are the honourable exception: `vk_renderer.cpp:112-117` logs once
  ("entities invisible") — the pattern the others should follow.
- Fix: one-time logWarn per missing path (sprite/PBR/trace) in renderFrame/upload entry points.

### H5. `render.pbr_tier` cvar setter still unclamped — 2026-10-11 LOW STILL-OPEN, now reachable into an enum cast
- VERIFIED `settings_cvars.cpp:96-99`: `s.pbrTier = toInt(v)` with no clamp, while its neighbours
  were fixed (`render.ambient_mode` :100-104 clamps 0..1; `render.frustum_margin_deg` :105-112
  clamps 0..15). `settings.json load()` clamps, the cvar setter doesn't.
- `game.cpp:378,954`: `static_cast<PbrTransparencyTier>(settings_.pbrTier)` — `render.pbr_tier 9`
  reaches GL33 as tier 9: no shader branch matches (`uPbrTier_` compared against 0..3),
  `pbrBlend`/refraction booleans both false → misrendered transparency, no crash (as in 2026-10-11).
- Fix: `std::clamp(toInt(v),0,3)` in the setter (one line, mirroring ambient_mode).

## MEDIUM

### M1. Software renderFrame does NO frustum culling and ignores the margin entirely
VERIFIED software_renderer.cpp:489-508: iterates **all** `meshes_` and draws every one; no
`aabbInFrustumMargin`, no CullProtoState member. GL/VK both cull per chunk
(gl33_renderer.cpp:516-518,542; vk_renderer.cpp drawChunks). Correct output (depth test saves it),
but the "renderer parity / frustum margin applies to backends" claim fails and CPU cost scales
with world size, not view. Fix: reuse `aabbInFrustumMargin(p.viewProj,…,frustumMarginDeg_)` —
needs the H2 setter override first.

### M2. Software sprite pass discards shade mode, sorting and alpha
VERIFIED software_renderer.cpp:417-487 `drawSprites`: every quad becomes flat opaque PackedVerts
(`mode=kShadeTex`, face 2, value=luminance) regardless of `q.shade`
(Cutout/AlphaBlend/Additive identical), producer sort order is preserved in the buffer but the
rasterizer depth-writes everything, so blended/additive sprites occlude like opaque geometry.
Also: per quad per frame it copies the UV rect into a fresh `std::vector<uint8_t>` + constructs an
`Image` just for `averageColor()` (:460-467) — allocation + full-rect scan per sprite per frame.
Header/game docs call this the "flat average-color form" (documented simplification — hence
MEDIUM not HIGH), but FrameParams' contract ("producer-sorted") is not honoured. Fix: at minimum
skip depth-write / respect order for non-cutout; cache per-(def,frame) average in the host.

### M3. No frame-rate limiter anywhere — vsync-only pacing (VERIFIED, as documented)
Main loop (main.cpp:629-690) has no sleep/deadline: `dt` is clamped for sim (`min(0.1,…)`) but the
loop spins at full rate when vsync is off; the only pacing is `Window::swap()` swap-interval
(GL/software presenter) or Vulkan present mode. Software+vsync-off = a busy CPU rasterizer loop
at 100% core. Perf instrumentation exists (hitch log, p99) but nothing caps. Fix: optional
`render.fps_cap` deadline sleep after present (default off/0), at least for the software path.

### M4. Mesher: `static thread_local opaque[]` is safe, but mesher reads live registry/materials with no snapshot of defs
VERIFIED mesher.cpp:104: `static thread_local uint8_t opaque[kMeshPadVolume]` — fully rewritten
every call (:105), no cross-call leakage; NOT a bug. The real hazard is contractual: `MeshContext`
(mesher.h:24-31) holds raw `const BlockRegistry&/BlockMaterials&/IBlockAppearance*` read on worker
threads; snapshot discipline covers voxel data (MeshInput, see Threading) but **not** defs — this
is the Full-Tree-2026-10-12 #5 race (`refreshStaged()` in-place def mutation) viewed from the
render side: REGRESSED/ STILL-OPEN in scope terms (no epoch check in meshSubChunk; `MeshInput::epoch`
is the *dimension* epoch, dropped at game upload, never compared to a definition epoch).
Fix direction as before: shared_mutex/atomic epoch + reject/queue stale-def meshes.

### M5. GL33 sprite VBO: fixed 6144-vert allocation is immediately orphaned every frame
VERIFIED gl33_renderer.cpp:80 allocates `sizeof(SpriteVert)*6144`, then drawSprites:169-170 calls
`glBufferData(size = spriteVerts_.size()*sizeof…)` every frame — reallocating/orphaning the store
per frame (the 6144 cap does **not** truncate: size follows the vector; correctness fine, perf not).
>6144-vert frames pay a full realloc. Fix: `glBufferData` only on growth, `glBufferSubData` otherwise.

### M6. Vulkan HUD overlay silently truncates
VERIFIED vk_renderer.cpp:592-596: HUD verts are clamped to `(kOverlayBytes−kHudOffset)/sizeof(HudVertex)`
rounded down to a multiple of 6, excess **silently dropped** (no log, no stats). GL33 streams an
unbounded vector. Busy UI (inventory+tooltips) on Vulkan can lose trailing draws invisibly.
Fix: log once when `count < hudVerts_.size()`, or grow the overlay buffer.

### M7. PBR transparency tiers: only GL33 implements the ladder; CPU `shadeTransparent` TrueRefraction is still a request-stub, software implements none of it
VERIFIED pbr_transparency_tiers.h: GL33 two-pass TrueRefraction is real (gl33_renderer.cpp:
refraction branch — pass 0 opaque, `copySceneColor`, pass 1 refractive; tier→blend logic :601-627;
Simple/ScreenDoor/PbrReflections via `uPbrTier_`). CPU `shadeTransparent()` TrueRefraction branch
still returns `refractionRequested` + black (by design, header : "STUB — hand the Vulkan path…"),
Vulkan has no chunk_pbr at all, and software has no tier selection (pbr_software_shaders.h
"ULTRA TIER GAP — documented, not silent": honest). Net: tiers 1-3 (Simple/ScreenDoor/
PbrReflections) implemented **only on GL33+BinaryQuad**; tier 4 GL33-only; every other
backend/path = tier silently = whatever its classic shading does. Claim "tiers 1..4 implemented"
is OVERSTATED without the GL33+BinaryQuad qualifier.

### M8. Emissive-provider refresh reads the live ChunkMap on the game thread — snapshot discipline bypass (bounded)
VERIFIED game.cpp (update path): `emissiveProvider_->refresh(active_->chunks, fcx, fcz)` and
pbr_emissive_provider.h:118-137 `refresh(const ChunkMap&)` scans `col->subs[].blocks/materials`
directly. This is not the mesher snapshot path. It runs on the main/game thread per the comment
("checked every frame, rescanned only when needed"); if the ChunkMap owner is the sim thread in
the threaded configuration, this is an unsynchronised read of mutable voxel storage racing sim
edits. SUSPECTED (thread ownership not fully traced in this scope) — impact: torn/stale light
cache at worst (visual), not memory corruption if ChunkColumn storage is stable-addressed.
Fix: refresh from a MeshInput-style snapshot or document+assert the owner-thread contract.

### M9. `mesher surface sub-chunk` selftest failure — REPRODUCED in this review (872 PASS / 6 FAIL full run)
The full `LithoKraft --selftest` run completed during this review: **872 PASS / 6 FAIL**, and
`[FAIL] mesher produces quads for a surface sub-chunk` is among the failures (the other in-scope-adjacent
fail is missing HUD icons for `glass` + one other block; the rest are wafer-caves worldgen checks).
The check (selftest.cpp:282) meshes `snapshotForMeshing(wafer->chunks, …, 2, surfaceSy, 2, …)` where
`surfaceSy = max sy with a sub` (selftest.cpp:86-91) — i.e. it assumes the *topmost* sub-chunk of
column (2,2) contains non-air surface blocks with an exposed face. `meshSubChunk` emits nothing for
a sub-chunk that is (a) all-air above the terrain crest, (b) entirely interior/opaque-caged, or
(c) all non-Cube/Slab/Cross/Custom shapes (`mesher.cpp:146: if (def.shape != Cube && != Slab) continue;`).
`engine_render`'s own mesher tests (face culling, slab/cross, golden hash, binary agreement) all
PASS and the mesher logic itself shows no defect — SUSPECTED root cause is the test's assumption
(topmost sub ≠ surface-bearing sub after worldgen changes shifted the crest by one sub-chunk),
not the mesher; alternatively column (2,2)'s topmost sub is all-air and the snapshot's border
fill differs. Definitive split needs one diagnostic run printing `m.verts.size()` + a block census
of that sub-chunk — not done here (read-only scope, no instrumented build).
Fix direction: pick the highest sub-chunk *containing a non-air block with an exposed face*, and
print diagnostics on failure.

## LOW

- **L1. GL texture-array upload trusts tile geometry.** gl_texture_array.cpp:18-21:
  `glTexSubImage3D(…, textures.textures()[i].image.pixels().data())` with no check that each image
  is exactly `tile_×tile_`; a smaller image = OOB host read fed to GL, a larger one = silent crop.
  SUSPECTED (TextureRegistry may guarantee it upstream; no assert here). Add a size check/skip+log.
- **L2. Software FXAA is a 3×3 blur of everything**, despite the comment "on high-contrast edges
  only" (software_renderer.cpp:334-356: comment says v1 blurs everything — honest in code, but
  `applyFXAA` has no production caller found in scope; dead-ish + misnamed). Remove or gate on edge.
- **L3. `faceColor` flat path mis-maps modes**: software_renderer.cpp:159-170 compares raw
  `mode==3||mode==4` for strip tints and otherwise renders grayscale from `value` — most ShadeModes
  (Palette/Pips/Cells/Animated/Glass…) collapse to grey in flat mode. Documented "simplified", but
  MCP-vision captures (flat mode by design) therefore misrepresent tinted/palette blocks.
- **L4. Vulkan `loadShaders` returns 0 with an INFO log** (vk_renderer.cpp:106-110) — correct
  (SPIR-V built-in, pack GLSL is GL-only) and honestly logged. No action; noted so it isn't
  re-flagged as a stub.
- **L5. Cull-prototype double-decide on GL PBR path**: gl33_renderer.cpp:516 and :542 call
  `cullProto_.decide()` for the same chunk in classic + PBR loops; `decide()` increments stats
  each call (cull_prototypes.h:265ff) → `cullProtoStats()` double-counts chunks present in both
  maps, and comet/velocity state is stat-only so no visual bug. Stats-only; fix by deciding once.
- **L6. SoftwareRenderer::renderFrame counts `stats_.drawn` as meshes iterated**, including fully
  off-screen/behind-camera meshes (no culling, M1) — stats are not comparable across backends.
- **L7. Sprite registry atlas: oversize-frame path sets `page=-1`** (sprite_registry.cpp
  rebuildAtlas) forcing a fresh page after every oversize frame — correct but wasteful; and
  `roundUpPow2` caps at 8192 silently for larger requests. Cosmetic/perf only.
- **L8. `software_test.cpp` is a stub test**: `testSoftwareRenderer()` builds no triangle (the
  rasterizer calls are commented out, :35-60) and "passes" by writing a PPM of a cleared buffer.
  It proves framebuffer allocation only. Real software coverage lives in engine_tests/sprite_host
  selftest instead — rename or wire it.

---

## Claim verification

| Claim | Verdict | Evidence |
|---|---|---|
| SoftwareRenderer is a full IRenderer backend | **OVERSTATED** | Core surface (meshes/entity/frame/grab/textures) real; PBR setters, frustum margin, cull protos, PBR upload all inherited no-ops (H2); no HUD/culling (H3/M1). |
| Two software surfaces "agree pixel-for-pixel" (software_renderer.h:34) | **HOLDS (narrowly)** | Both surfaces funnel into the same `renderMesh/renderFlatQuads` rasterizer (renderFrame :489ff, direct API :273ff) — identity is by construction for the world pass. NOT established for sprites/HUD/PBR, which the surfaces treat differently/absently. No golden-hash test pins cross-surface identity. |
| Loud GL33 fallback (software presenter fail → GL33; Vulkan fail → GL33) | **HOLDS** | main.cpp:288-293 `logError("…falling back to OpenGL 3.3")` + presenter=nullptr path; :309-315 same for Vulkan with the error string. Caveat: if GL33 creation *also* fails, `d.renderer` is null and the failure surfaces later/less clearly (no explicit logError at :316). |
| Sprite registry first-wins / fail-closed | **HOLDS** | sprite_registry.cpp adoptImage: duplicate name returns existing id *before* validation/decode (registerPng pre-checks too, no decode on dup); empty name, non-finite/non-positive size, frameCount 0, SVG multi-frame, empty image, indivisible strip, explicit-id clash all `fail(err,…)`→0. Atlas: deterministic (ordered maps, shelf packing, : "pure function of the def set"), version bumps per rebuild. |
| Deterministic emitters (splitmix64, bit-exact per seed) | **HOLDS** | sprite_system.cpp: fixed 10 draws/particle in fixed order regardless of ranges; splitmix64 top-24-bit uniforms; tick is integer, spawn-order, stable erase. No platform RNG/time anywhere in the file. |
| Billboard sorting (cutout first, blend/additive back-to-front) | **HOLDS (system+GL)** | sprite_system.cpp emitQuads: cutout in spawn order, then `std::sort` by depth desc, instance-id tiebreak. GL33 draws runs in that order with per-mode blend/depth state. **FAILS on software** (M2) and **absent on Vulkan** (H4). |
| GL33 has a sprite path; Vulkan has none | **HOLDS** | GL: gl33_renderer.cpp:35,77-86,125-202 (program "sprite", atlas re-upload on version change, non-fatal if shader missing :373-375 with logError). VK: zero sprite references. Surfacing: implicit only — see H4. |
| Cook–Torrance PBR core | **HOLDS** | pbr_brdf.h/.cpp: Fresnel-Schlick + GGX D + Schlick-GGX Smith G + Lambertian, documented approximations; engine_tests BRDF known-answers all PASS in re-run. |
| K-nearest emissive analytic lights | **HOLDS** | pbr_lights.h LightCollector: bounded max-heap, stable ties, sorted nearest-first, `collectInto` allocation-free; GL uploads ≤8/frame (renderer.h kMaxPbrFrameLights); flood-fill carries diffuse, analytic lights specular-only as locked. |
| Flood-fill light levels / ambient Fraction vs NormalizedSun | **HOLDS** | pbr_lights.h irradianceFromLightLevel: diffuse=max(block,sky)/64; Fraction ambient=sky/64 (one multiply); NormalizedSun multiplies by hemisphere term. GL `setAmbientMode` real (gl33_renderer.cpp:255); software/VK inherit no-op (H2/H4). |
| Transparent tiers 1..4 implemented | **OVERSTATED** | See M7: all four only on GL33+BinaryQuad (tier 4 = single-layer compromise, documented); CPU helper's tier-4 is a request stub; VK/software none. |
| Frustum margin 0..15, clamped, never touches sim | **HOLDS** | Clamped at cvar (settings_cvars.cpp:105-112), at game read (game.cpp:1307), and in both GPU backends (gl33 :257, vk :221-223 `std::clamp(degrees,0.f,15.f)`). Culling is draw-submission only; meshes stay resident/uploaded, sim untouched (cull_prototypes.h, GL/VK loops). Exception: software backend ignores it (M1). |
| HUD/menus not rasterized in software mode | **HOLDS (gap current)** | H3. |
| No frame-rate limiter; vsync-only | **HOLDS** | M3. |
| Vulkan selftest failure on GPU-less machines is environmental | **HOLDS** | selftest.cpp:294-296: no loader → `[SKIP]` (not FAIL); loader-but-no-device → `check(vk.ok)` FAILs with summary. On this VM the probe is expected SKIP/no-device; ctest baseline shows no Vulkan test failure attributable to code. Handling is correct; a loader-present-but-deviceless CI box would still FAIL rather than SKIP — minor: consider SKIP when `n==0` devices. |
| Ground traces live (headers/docs) | **FALSE** | H1. |

## Regression check (prior reviews)

| Prior finding | Verdict |
|---|---|
| 2026-10-11: ground traces dead (no GLSL mirror, uniforms −1, cvar unregistered, CPU mirror unconsumed) | **STILL-OPEN** — re-verified in full; VK half-wiring added since, also dead (H1). |
| 2026-10-11 LOW: unclamped cvar setters incl. `render.pbr_tier 9` → misrender | **INCOMPLETE** — ambient/frustum/comet/velocity setters now clamped; `render.pbr_tier` (and `render.mesher`, `client.display_mode`, `sim.tickradius` out of scope) still raw `toInt` (H5). |
| Full-Tree 2026-10-12 LOW: software `\|w\|` clamp destroys sign → defeats near-plane rejection | **FIXED-CONFIRMED** — software_renderer.cpp transformVertex: `copysign(1e-6f,cw)`, out[3]=w, all raster paths reject `w<=0` quads/tris (renderFlatQuads/renderMesh/renderEntityVerts). |
| Full-Tree 2026-10-12: `engine_render` golden hash drift (Morning: passing) | **FIXED-CONFIRMED** — engine_render PASS in this run; mesher golden hash PASS. |
| Morning 2026-10-12: Vulkan headless `VK_ERROR_INCOMPATIBLE_DRIVER`, environmental | **STILL-OPEN (environmental, correctly handled)** — probe SKIP/no-device logic verified (claims table). |
| Morning/Evening 2026-10-12: mesher/worldgen failures ("mesher produces quads for a surface sub-chunk") | **STILL-OPEN — REPRODUCED** in this review's full selftest run (872 PASS / 6 FAIL; mesher check + icons + wafer-caves fail). Mesher unit tests all pass; root cause suspected in the selftest's surfaceSy assumption (M9). |
| Evening 2026-10-12 C1: selftest segfault masks worldgen/mesher/Vulkan checks | **FIXED-CONFIRMED (masking gone)** — the full selftest now runs to completion (872 PASS / 6 FAIL, all checks reachable, incl. Vulkan probe + sprite host checks which PASS). |
| 2026-10-11/12: IK NaN (render-adjacent) | Out of scope (anim/); Full-Tree noted the bind-pose fix comment at iksolve.cpp:207 — not re-checked. |

## Threading / snapshot discipline (summary)
Meshing proper is clean: workers consume immutable `MeshInput` snapshots
(world/mesh_input.h: "Mesh workers read only this, never the live map"), taken by the owner
thread; `meshSubChunk` is a pure function of snapshot+context apart from the benign
thread_local scratch (M4). Residual hazards are at the edges: live registry defs in MeshContext
(M4) and the emissive provider's direct ChunkMap scan (M8). Renderers themselves are
single-threaded by contract (render thread; MCP's separate SoftwareRenderer is mutex-serialized
in mcp_senses_adapter.h:173).

---
## Appendix E — Texture, Assets & Security

# Code Review 2026-10-13 — Texture Module, Asset Parsing & Security Surfaces

Scope: `engine/src/texture/`, `engine/src/assets/` (incl. `svg/`), `engine/src/systerian/`,
`engine/src/crypto/` + signing, cross-cutting parse/fuzz surfaces in that area.
Method: read-only code read; tests run only in `build-linux`
(`ctest -R "texture|assets|crypto|systerian"` → **4/4 passed**).
No private key material is reproduced anywhere in this report (locations/nature only).

Severity counts: **Critical 0 · High 1 · Medium 5 · Low 9** (plus regression/claim ledgers below).

---

## HIGH

### H1. Canonical mod hash is not framing-safe — a JD signature can be transplanted to different mod contents
- **VERIFIED** — `engine/src/crypto/signing.cpp:52-67` (`canonicalModHash`): the digest is
  `for each file: H.update(rel_path); H.update(0x00); H.update(file_bytes)` — the path is
  NUL-terminated but **file bytes have no length prefix and no terminator**, and no file
  count is hashed.
- Concrete collision (both are ordinary, valid mod trees, paths sort identically):
  - Mod A: file `a` = `""`, file `b` = `""` → byte stream `a \0 b \0`
  - Mod B: file `a` = bytes `"b\0"` (2 bytes), no file `b` → byte stream `a \0 b \0`
  - Identical SHA-256 input ⇒ identical canonical hash ⇒ a signature issued for A
    verifies **Verified** for B, whose file contents differ.
- Impact: the "Verified = JD vouches for these exact contents" guarantee does not hold.
  Anyone holding one JD-signed mod can re-partition bytes across the path/content
  boundary to produce a different mod that still verifies.
- Fix: length-prefix both fields (e.g. `u64le(len(rel)) || rel || u64le(len(bytes)) || bytes`),
  or hash per-file digests and hash the sorted digest list. Any fix changes the canonical
  spec (`signing.h`) — all existing signatures must be re-issued.

---

## MEDIUM

### M1. Crafted `mod.json` dependency version range throws `std::out_of_range` out of the parser — boot crash
- **VERIFIED** — `engine/src/assets/mod_manifest.cpp:81-82`:
  `!SemVer::parse(dep.range) && !SemVer::parse(dep.range.substr(dep.range[0] == '>' ? 2 : 1))`.
  For `{"deps":[{"id":"x","version":""}]}` the range is `""` (present-but-empty beats the
  `"*"` default in `Json::getStr`), `parse("")` fails, and `substr(1)` on an empty string
  throws. Range `">"` throws the same way (`substr(2)` on a 1-char string).
  Mechanism confirmed empirically (standalone `std::string` repro throws
  `basic_string::substr: __pos (which is 1) > this->size() (which is 0)`).
  Neither `loadModDirectory` (`mod_manifest.cpp:158ff`) nor its caller in
  `game/LithoKraft/src/app/main.cpp` (`scanMods`) catches exceptions → a single hostile
  mod folder aborts the game during the boot mod scan.
- Fix: validate range shape/length before `substr` (reject empty range, handle
  `>=`/`=`/`^` prefixes explicitly); never index/substr an unchecked string.

### M2. `.ltxr` parser: no node/stop/input caps, and most numeric params are unvalidated — UB + OOM from a hostile recipe
- **VERIFIED** — `engine/src/texture/texture_recipe.cpp`:
  - `parseRecipe` (lines 337-359) has **no cap** on node count, palette-stop count, or
    input size; every generator node pushes a `size × size` float field onto the stack
    (`texture_generator.cpp` `Ctx::stack`) and only `blend` ever pops. `parseLine`
    range-checks only `scale`, `octaves`, `posterize`, `cells`, stop positions
    (lines 318-326).
  - `lacunarity`, `gain`, `gamma`, `contrast`, `strength`, `clamp_min/max`, `sharpness`,
    `intensity`, `amount`, `value` accept any finite float. In
    `octaveParams` (`texture_generator.cpp:146-156`) `cur *= lacunarity` iterated up to
    16 octaves from `scale ≤ 1024` overflows float → `std::llround(±inf / >LLONG_MAX)`
    is **undefined behaviour**. Negative/huge `gamma`/`gain` similarly drive
    `pow`/normalisation into inf/NaN, which reaches `std::lround` in `toByte`/
    `sampleGradient` (UB on NaN/out-of-range conversion).
  - Warp `strength` is unbounded: accumulated warp offsets can make the
    `static_cast<int>(std::floor(x))` conversions in `rawNoise`/`evalVoronoi`
    out-of-range (UB).
- Impact today is bounded by exposure: recipes are consumed by the offline
  `texture_gen` tool on first-party `.ltxr` files; the header explicitly frames
  `.ltxr` as "the future player/mod authoring format", at which point this becomes a
  runtime hostile-input surface. Generation size is clamped to 4096
  (`texture_generator.cpp:448`), i.e. up to 64 MiB per stacked field — N stacked
  generator nodes = N × that, uncapped.
- Fix: cap nodes (e.g. 256) and stops per palette; range-check lacunarity (e.g.
  [1,4]), gain ([0,1]), gamma/contrast/strength/clamps to sane finite bands at parse
  time, consistent with the existing scale/octaves checks.

### M3. File-size caps are checked *after* unbounded whole-file reads
- **VERIFIED** — the 64 MiB caps live inside the decoders, but every caller slurps the
  whole file into memory first:
  - `Image::load` / `Image::loadSvg` (`engine/src/assets/image.cpp:99-120`):
    `std::vector<uint8_t> bytes((istreambuf_iterator...))` with no size check, then
    `decode`/`decodeSvg` rejects >64 MiB — after a multi-GB file is already resident.
  - `ResourcePack` `slurp` (`resource_pack.cpp:10-16`) — `pack.json`/`resources.json`
    unbounded; `PackStack::readText` same helper.
  - `readSourceBytes` (`resource_loader.cpp:246-258`, provenance path) — unbounded.
  - `canonicalModHash` (`signing.cpp:60`) — each mod file fully buffered (hashing a
    hostile "mod" of huge files is memory-proportional, at least not amplifying).
- Impact: a hostile pack/mod file of several GB is fully read before any cap fires →
  OOM DoS, despite the caps existing. Fix: stat size (or stream with a running cap)
  before/at read time in the slurp helpers and `Image::load`.

### M4. Signed-mod hash silently excludes symlinked content that the loader will happily read
- **VERIFIED (code) / SUSPECTED (exploit path)** — `signing.cpp:44-47`:
  `canonicalModHash` skips every symlink and (default `recursive_directory_iterator`,
  no `follow_directory_symlink`) never descends into symlinked directories, so their
  contents contribute nothing to the signed hash. The resource loader, by contrast,
  follows symlinks that stay inside the pack root (`ResourcePack::has`,
  `resource_pack.cpp:55-72`, canonical-containment check). A "Verified" mod can
  therefore contain a symlinked directory whose target is swapped between two
  in-mod locations (or whose in-root target is itself unsigned content) without
  changing the canonical hash.
- Fix: either refuse symlinks anywhere in a signed mod at verify time, or make the
  loader refuse to resolve pack paths through symlinks for Verified mods.

### M5. SVG intrinsic-size float→int conversion still happens *before* the clamp (regression L-assets7, still open)
- **VERIFIED** — `engine/src/assets/image.cpp:72-79` (`decodeSvg`):
  `w = int(std::ceil(svg->width)); h = int(std::ceil(svg->height));` runs on the raw
  NanoSVG dimensions, and only afterwards `w = std::clamp(w, 1, 2048)`. An SVG with
  `width="1e30"` makes the float→int conversion out-of-range — UB (in practice yields
  INT_MIN and fails safe, which is why this is Medium-Low in effect, but it is exactly
  the 2026-10-12 L-assets7 item, unfixed).
- Fix: clamp the floats first: `w = int(std::ceil(std::clamp(svg->width, 1.0f, 2048.0f)))`.

---

## LOW

### L1. Texture cache: 64-bit FNV-1a key is collision-forgeable; returned reference is not safe for off-thread use
- `texture_generator.cpp:545-574` (`generateTextureCached`): the entry stores
  `recipeHash` (FNV-1a-64 over the canonical recipe text, then mixed with seed/size)
  plus `seed` and `size`, and a hit requires all three to match — but FNV-1a is not
  collision-resistant; two different recipes with a crafted FNV collision, same seed
  and size, silently return the first recipe's pixels. Impact is visual only
  (textures are not a trust boundary) → Low.
- LRU mechanics themselves are **correct**: splice-to-front on hit, `push_front` +
  `pop_back` while `size > 64` (`:565-571`). Generation happens **under** the mutex
  (thread-safe, but bakes serialise), and the returned `const Image&` is documented
  to die on eviction — so "bake off-thread" is safe only if callers copy or finish
  reading before any other thread calls the cache. The header's contract states
  this; the thread-safety claim is therefore conditional, not absolute.
- Minor: the raw `size` argument is the key, while `generateTexture` clamps to
  4096 — `size=4096` and `size=8192` cache the same image twice.

### L2. `writePrivateKeyFile` does not force 0600 on a pre-existing file and follows symlinks
- **VERIFIED** — `signing.cpp:113-128`: `open(path, O_CREAT|O_TRUNC|O_WRONLY, 0600)`.
  The mode applies only at creation; if `<prefix>.priv` already exists with looser
  permissions (or is a symlink), the seed is written through it with permissions
  unchanged. The header comment ("mode 0600 ATOMICALLY (no umask window)") is
  overstated for the pre-existing-file case. `litho_sign keygen` also silently
  overwrites an existing keypair. Tool is offline/JD-only → Low.
  Fix: `O_EXCL` (refuse if present) or `fchmod(fd, 0600)` after open.

### L3. Boot test-key guard exists only in the game's `main()` — and only refuses in release
- **VERIFIED** — `game/LithoKraft/src/app/main.cpp:116-127`: `bakedKeyIsTestKey()` →
  `std::exit(1)` under `#ifdef NDEBUG`, loud warning otherwise. Nothing in the
  engine library, `litho_sign`, or any other entry point performs this check; any
  future/alternate executable that links `litholattice_assets` and calls
  `verifyModSignature` ships the test-key trust anchor silently. (See claim verdicts.)

### L4. Double→int conversions on hostile JSON numbers are UB before any clamp
- **SUSPECTED/VERIFIED-by-reading** — `resource_loader.cpp`: `parseTiming`
  `int(j->getNum("frametime_ms", ...))` and `int(v.num)` for frame indices;
  `ModelParser::faceRef` `uint8_t(std::clamp(int(uv->arr[i].num), 0, 16))` — the
  `int(...)` conversion happens *before* the clamp, so `1e30` in a manifest is UB.
  `mod_manifest.cpp:60` `int(j.getNum("schema", 1))` same pattern. In practice
  x86-64 yields INT_MIN and downstream checks reject/clamp, but a hostile
  `resources.json`/`mod.json` should not rely on that. Fix: clamp in double
  (`std::clamp(num, -1e6, 1e6)`) before converting. (`mergeManifests` tile_size,
  `resource_loader.cpp:44-46`, does this correctly.)

### L5. `resolveMods` dependency walk is unbounded recursion
- **VERIFIED** — `mod_manifest.cpp:117-140`: `visit()` recurses per dependency
  edge with cycle detection but no depth cap. A directory of N chained mods
  (each depending on the next) recurses N deep — same class as the alias-cycle
  issue found 2026-10-12 in the world module. Needs thousands of mod folders on
  disk → Low. Fix: iterative walk or a depth cap (~64).

### L6. `ResourcePack::has` → open is a TOCTOU pair; `path()` returns the unchecked path
- **VERIFIED** — `resource_pack.cpp:55-72` canonicalises and containment-checks,
  but consumers (`loadImage`, `readText`, `readSourceBytes`) then open
  `ResourcePack::path(rel)` (`resource_pack.h:30`, plain `dir_/rel`). A symlink
  swapped between check and open escapes the check. Requires a local attacker
  able to mutate the pack while the game loads it → Low. Note `has()` itself is
  correct: component-wise prefix comparison of two canonical paths (no string
  prefix bug), and it is re-run per lookup.

### L7. SvgRasterCache: still unbounded, still unsynchronised, key still non-canonical
- **VERIFIED** — `resource_loader.cpp:78-80, 259-285`: the shared cache is an
  `unordered_map` with `store` = `emplace` and no eviction except the
  `clearSharedSvgRasterCache()` test hook; no mutex. This is evening-report
  L-assets4 + L-assets6 unchanged (latent: all current callers are
  main-thread). `loadImage` keys it by `path.generic_string()` — the
  non-canonical `dir_/rel` — so the same file via different relative spellings
  rasterises twice (L-assets2, correctness-safe). KeyHash now includes file
  size (improvement over 2026-10-12) but still omits `mtime` (equality does
  compare it → correctness preserved, L-assets3 unchanged).

### L8. Provenance still verified for `textures` only; converter still drops `.svg` + provenance
- **VERIFIED** — `resource_loader.cpp:330-345`: `verifyTextureProvenance` is
  called only in the textures loop (evening M8 unchanged — animations/palettes
  markers silently ignored). `engine/tools/convert_pack_manifest.py:62` still
  skips every non-`.png` file and emits no `provenance` stamps (evening item 11
  unchanged); regenerating a pack still loses SVG entries. Current shipped
  packs contain zero `.svg` entries (grep of pack_16/32/64 `resources.json`),
  consistent with this gap. CRC32 provenance itself is honestly documented as
  non-cryptographic labelling and never gates loading — verified in code.

### L9. Ed25519 verify: no small-order/canonical-encoding checks on R and A (malleability, not forgery)
- **SUSPECTED** — `ed25519.cpp:471-497`: `verify` checks `S < L` and that R and
  A decompress, and compares the group equation — good basics. It does not
  reject small-order R/A encodings or non-canonical field encodings, and the
  `if (SG.inf) return true` degenerate branch accepts an identity left-hand
  side. No practical forgery was constructed (the trusted key A is fixed, and
  S=0 requires solving for R against the hash), but multiple encodings of the
  same signature may verify (malleability). For a mod-trust anchor this is
  acceptable today; note it before signatures are ever used for byte-equality
  or transcript schemes.

### Notes (checked, no finding)
- `systerian/`: pure generator (uint32/char inputs only, digits range-checked
  via `decomposeValue`/`dig()`); `svgForText` output grows linearly with input
  length and `decodeSvg` clamps the raster to 2048² — no parse surface.
- `shader_library.cpp`: `#include` expansion has a depth cap (8) and cycle
  handling via that cap; exponential include fan-out is bounded by the depth
  cap and by pack file sizes (see M3 for the unbounded-read caveat).
- `block_model.cpp`/`box_model.cpp`: name-table lookups only, no unchecked
  indexing found in scope.
- `Image::checker(size)` / `TextureRegistry` assume `tile ≥ 1`; tile size
  reaches the registry only via the range-checked manifest path (1..1024).

---

## CLAIM VERIFICATION

| Claim | Verdict | Evidence |
|---|---|---|
| Determinism (same recipe+seed+size → byte-identical) | **HOLDS, with the documented caveat** | No `rand()`, `srand`, clock, or `random_device` anywhere in `texture/` (grep clean; the only `chrono` use is selftest benchmarking). Seeding is splitmix64 integer-only (`texture_generator.cpp:37-60`); permutation table built per generation from the seed. Floats use `pow`/`sqrt` from libm — the header (`.h:9-11`) honestly scopes the guarantee to "every run of the same binary", so the claim as documented holds; any stronger cross-platform reading would be overstated. Optimised vs reference paths are cross-tested byte-identical (`texture_selftest.cpp`, and `ctest engine_texture` passes). |
| "Seamless by construction" | **HOLDS** | Noise: lattice coords wrapped mod integer period before hashing in both paths (`rawNoise`, `wrapCoord`). Voronoi: neighbour cells wrapped with `wrapCoord` and distances computed against unwrapped cell origins, so the field is exactly periodic. Warp: the warp field is itself a periodic FBM, so offsets at u=0 and u=1 are equal and displaced sample positions differ by exactly one period of the (wrapped) downstream field — warp does **not** break tiling. Grid: fractional-cell function of an integer cell count. Selftest checks exact periodicity of the primitives and image seam continuity; passes. |
| Cache correctness (64-entry LRU keyed by recipe hash/seed/size) | **HOLDS with qualifications** | LRU mechanics verified correct (L1). Key covers the full recipe because it hashes the canonical `recipeToText`, which serialises every node field. Qualifications: FNV-1a collisions are forgeable (visual impact only); raw-vs-clamped size duplicates entries; returned-reference lifetime makes off-thread use conditional. |
| Release hard-refuses on the test key (`exit(1)`) | **HOLDS for the game executable; OVERSTATED as a tree-wide claim** | Check verified: `mod_signature.cpp:109-118` `bakedKeyIsTestKey()` memcmps the baked key against the retired test key's **public** half; `main.cpp:116-127` calls `std::exit(1)` — but only under `#ifdef NDEBUG`, only in `game/LithoKraft/src/app/main.cpp`, and no build-time/CMake check exists. A debug "release" or any other binary linking the assets lib does not refuse. The baked key **today is still the test key** (`mod_signature.h:44-45` hex is identical to the test public key compared in `mod_signature.cpp:114`), so a release build currently refuses to boot by design (fail-closed) until JD installs his real key. |
| Private key material in the tree today | **NONE FOUND** | Exhaustive check: every 64-hex literal in `engine/src`, `engine/tests`, `game/`, `tools/` was derived as an Ed25519 seed (pure-Python RFC 8032 derivation) and compared against the baked public key — **0 matches** in 237 unique literals. The 64-hex literals in `crypto_selftest.cpp:39-89` are the published RFC 8032 test vectors (public, and not the baked key's seed); the signing selftests now generate **ephemeral** keypairs per run (`crypto_selftest.cpp:154-156`, `assets_selftest.cpp:325`). No `.priv`/`.key`/`.pem` files exist in the tree. No `.git` directory exists in this tree, so git-history exposure (the 2026-10-12 concern) cannot be confirmed or cleared from here — the historical seed must still be treated as compromised, which the design does (the key is retired and release-refused). The three snapshot zips at the repo root (`LithoKraft-game-src-100ecf3.zip`, `LithoLattice-engine-src-100ecf3.zip`, `litholattice-full-2026-10-11.zip`) were scanned the same way — 181 unique 64-hex literals, **0 matches** (they also predate the crypto module: no `src/crypto/` in any of them). |
| `litho_sign` tool behaviour | **HOLDS** | `litho_sign_main.cpp`: keygen/sign/verify as documented; exit 0/1/2 semantics as claimed; `verify` checks the signature names the *given* key (`signing.cpp` `verifyMod` memcmp) not just any valid signature; private keys written via the 0600 path (subject to L2). |

---

## REGRESSION LEDGER (prior reports)

| Prior finding | Verdict today |
|---|---|
| Full-tree 2026-10-12 #2 CRITICAL — test private seed in tree in plaintext; no guard refuses the test key | **FIXED-CONFIRMED (source tree)** — seed removed (exhaustive seed-derivation sweep, 0 matches); selftests use ephemeral keypairs; fail-closed boot guard added (`main.cpp:116-127`, NDEBUG `exit(1)`). Residuals: baked key is still the test key (release currently refuses to boot until replaced — by design); guard is game-main-only (L3). Zip snapshots at repo root also clean (0 seed matches over 181 unique hex literals; zips predate the crypto module). |
| Full-tree 2026-10-12 — PNG decompression bomb (`image.cpp`: 64 MB input cap doesn't bound decoded output) | **FIXED-CONFIRMED** — `image.cpp:24-40`: `stbi_info_from_memory` probes dimensions *before* decode; `w`/`h` each capped at 8192, non-positive rejected, decode only after the check. Bound is per-dimension, so worst case decoded image is 8192×8192×4 = 256 MiB — bounded, not tiny. Residual: the input file is fully slurped before the cap is consulted (M3). |
| Full-tree 2026-10-12 — resource-pack symlink escape (`resource_pack.cpp` `has()` follows symlinks) | **FIXED-CONFIRMED** — `resource_pack.cpp:55-72`: both file and pack root canonicalised, component-wise containment, regular-file check. Residuals: TOCTOU between `has()` and open (L6); signed-hash symlink exclusion (M4). |
| Full-tree 2026-10-12 #10 MEDIUM — JSON parser no depth cap (`core/json.cpp`) | **FIXED-CONFIRMED** — `json.cpp:29-35,49`: `kMaxDepth = 128`, checked on both object and array entry, fails closed. (In-scope consumers — manifests, `resources.json`, `signature.json` — all use this parser.) `Json::dump` recursion is bounded by parsed depth ≤128 for parsed trees. |
| Evening 2026-10-12 L-assets7 — float→int overflow UB on absurd SVG dimensions (`image.cpp:53-54` at the time) | **STILL-OPEN** — moved, not fixed: now `image.cpp:75-76`, conversion still precedes the clamp (M5). |
| Evening 2026-10-12 M8 — provenance verified only for `textures` | **STILL-OPEN** — `resource_loader.cpp` textures loop only (L8). Documented textures-only would also close it; currently undocumented in `RESOURCE_PACKS.md` (L-assets9 likewise still open). |
| Evening 2026-10-12 M9 — `.svg` in `animations` silently becomes a 1-frame animation | **STILL-OPEN** — no change in `loadImage`/`addAnimation` behaviour or docs found. Behaviour is benign (a tile×tile SVG passes the strip check as one frame) but still silent. |
| Evening 2026-10-12 L-assets1 — duplicate `"tile_size": 32` in `pack_32/resources.json` | **FIXED-CONFIRMED** — pack_32 `resources.json` now contains exactly one `tile_size` (line 5). |
| Evening 2026-10-12 L-assets2 — cache key doc says "absolute path", code doesn't canonicalise | **STILL-OPEN (minor)** — `loadImage` still keys the SVG cache by the raw `path.generic_string()`; duplicates possible, never stale. |
| Evening 2026-10-12 L-assets3 — `SvgRasterCache::KeyHash` omits mtime | **STILL-OPEN (minor, improved)** — hash now includes path+tile+**size**; mtime still hash-omitted but equality-compared, so correctness is preserved. |
| Evening 2026-10-12 L-assets4 — unbounded SvgRasterCache growth | **STILL-OPEN** — no eviction; only the test-hook `clear()` (L7). |
| Evening 2026-10-12 L-assets5 — stale raster on same size+mtime | **STILL-OPEN** — key is still (path, tile, size, mtime); `makeKey` still swallows fs errors via `ec` (a failed stat yields size 0 / epoch mtime, which can alias). |
| Evening 2026-10-12 L-assets6 — SvgRasterCache / `crc32Table()` thread safety | **INCOMPLETE** — `crc32Table()` race **fixed** (magic-static initialisation, `resource_loader.cpp:205-217`, comment confirms the intent); `SvgRasterCache` itself still has no synchronisation. Latent only — no threaded callers today. |
| Evening 2026-10-12 item 11 — teach `convert_pack_manifest.py` about `.svg` + provenance | **STILL-OPEN** — converter still `.png`-only, no provenance stamping (L8). |
| File-Hardening 2026-10-11 — `ModSandbox::resolve()` lexical+symlink containment, `O_NOFOLLOW` | **UNCHANGED / NOT REGRESSED in this scope** — outside the texture/assets files reviewed here; the pack-side equivalent (canonical containment) is verified above. |
| Morning 2026-10-12 — 4 missing HUD icons | Out of this review's scope (content gap, not a code finding). |

### Addendum — snapshot zips (completed)
All three root-level snapshot zips were scanned with the same seed-derivation
test as the source tree: 181 unique 64-hex literals across all text files,
**0 derive the baked test public key**, and none of the zips contains the
crypto module at all (they are 2026-10-10/11 snapshots, signing landed
2026-10-12). No private key material found in the zips.

---
## Appendix F — Net, Scripting, Mods, Console, Registry

# Code Review 2026-10-13 — Net, Scripting, Mods, Console, Registry (read-only)

Tree: `~/workspace/litholattice`. Method: code read in full for the scoped modules; no files modified.
Tests run (existing build in `build-linux`, not rebuilt): `ctest -R "engine_net|engine_script|engine_mods|engine_console|engine_mcp|engine_world"` → **6/6 passed**.

**Counts: CRITICAL 0 · HIGH 1 · MEDIUM 9 · LOW 12**

---

## ⭐ PROTOCOL-VERSION ANSWER (deliverable, JD decision H2)

Literal sets in the code today:

- `knownProtocolVersions()` = **{1, 2, 3}** — `engine/src/net/include/litholattice/net/handshake.h:162-164`:
  `return {LITHOLATTICE_PROTOCOL_V1, LITHOLATTICE_PROTOCOL_V2, LITHOLATTICE_PROTOCOL_V3};`
  This is the *accepted-framing* set a game is told to install pre-handshake. **No game/engine code installs it** (zero `ConnectHandshake`/`knownProtocolVersions` call sites outside net + tests).
- **What a handshake actually offers by default = {3} ONLY.** `HandshakeConfig::supportedProtocols` empty → ctor fills `{kProtocolVersion}`:
  - `handshake.h:260`: `std::vector<uint8_t> supportedProtocols;  ///< empty = {kProtocolVersion}`
  - `handshake.cpp:86`: `if (cfg_.supportedProtocols.empty()) cfg_.supportedProtocols = {kProtocolVersion};`
  - `protocol.h:110`: `constexpr uint8_t kProtocolVersion = LITHOLATTICE_PROTOCOL_V3;`
- No code anywhere constructs `ConnectHandshake` with `{1,2,3}` (or any list). So effective behaviour today: **v3-only offers; a v2-only peer gets `Reject(NoCommonProtocol)`, fail-closed.** Answer to H2 as-implemented: **{3}, not {1,2,3}.**
- Codec dispatch (`handshake.h:230-241`): 1→`V1Baseline` (**no v1 codec exists in-tree** — selection only), 2→`V2Delta`, 3→`V3StableIds`. Negotiation itself (`handshake.cpp:10-19`, highest of intersection, equality via `protocolCompatible`) is correct.

---

## HIGH

### H1. Console: alias definitions launder permissions across time (the keybind fix was not applied to aliases) — VERIFIED
- `engine/src/console/concmd.cpp:538-549` `setAlias()` stores only the body: `aliases_[std::move(name)] = std::move(body);` and `concmd.h` declares `std::map<std::string, std::string> aliases_;` — **no caller-mask field**, unlike `KeyBind{command, key, callerMask}` (`concmd.h`, stamped at `concmd.cpp:557`, re-installed at dispatch `concmd.cpp:822-825`).
- Alias expansion `concmd.cpp:672-685` runs the body under whatever `callerPermissions_` is current at *execution* time; the cheat gate (`concmd.cpp:694`) consults only the current mask.
- Exploit sketch: restricted caller (MCP agent via `lithoscript_exec`, or a mod via `consoleCmd`, mask = its kPerm bits, cheat-gated commands refused) runs `alias x "give 123 64"` — the `alias` builtin is not cheat-gated, definition succeeds. Later the player (full authority, `kCallerFullAuthority`) types `x`, or a player-planted keybind/suspended script invokes `x` → `give` executes with full authority. This is exactly the "permission laundering across time" pattern the same file fixed for keybinds and `wait`-suspensions.
- Fix: stamp `callerPermissions_` on alias definitions (struct like `KeyBind`), re-install on expansion; or refuse alias definitions containing cheat-gated commands from restricted callers.

## MEDIUM

### M1. Script memory quota: the two accounting paths clobber one shared counter — quota evasion — VERIFIED
- `engine/src/script/script_engine.cpp:42-58`: AS-shim path mutates the counter directly: `h->owner->memoryBytes_ += n;` / `-= n`.
- `script_engine.cpp:66-77`: operator-new path *overwrites* it: `quotaCharge(){ quotaAccount_->bytes += bytes; memoryBytes_ = quotaAccount_->bytes; }` (same in `quotaCredit`).
- The AS-shim bytes are never added to `quotaAccount_->bytes`, so any operator-new charge/credit (script `std::string` buffers do this constantly) resets `memoryBytes_` to the operator-new-only total, silently erasing all AngelScript-heap usage from the quota check in `quotaEnforce()`/`allocShim`. A script alternating AS allocations with string allocations can hold far more than its 1 MiB quota unsuspended.
- Fix: one counter. Make the AS shim go through `quotaAccount_` (charge/credit on the shared account) and derive `memoryBytes_` only from it.

### M2. Script quota UAF fix is INCOMPLETE: AngelScript shim still stores a raw `ScriptEngine*` — VERIFIED
- `script_engine.cpp:28-31`: `struct AllocHeader { size_t n; ScriptEngine* owner; };` and `freeShim` (`:60-68`) dereferences it unconditionally: `h->owner->memoryBytes_ -= …`.
- The sibling operator-new hook was fixed for exactly this hazard with a `shared_ptr<QuotaAccount>` header (`script_memory_hook.cpp:54-59, 124-135`, comment: "the engine may be destroyed already"). The AS path was not. Any AS allocation retained past `ScriptEngine` destruction (ref-counted script object/string held by a binding, module discarded on unload) frees into dead engine memory on `freeShim`.
- Fix: store the `shared_ptr<QuotaAccount>` in `AllocHeader` like the operator-new hook; drop the raw owner.

### M3. Registry thread-safety is documented, not enforced — and only half-locked — VERIFIED
- Claim/doc: `block.h` ContentRegistry "built single-threaded at startup (mod-init window), then shared read-only"; `game/LithoKraft/src/content/blocks.cpp:226-229` `registerBlock()`: "Single-threaded mod-init only". **No enforcement anywhere**: no thread-id check, assert, or lock in `registerBlock`, `ContentRegistry::addBlock/addItem/insertEntry/registerKind/addBlockAlias/findBlock`.
- The only locked operations are `blockDef()`/`findBlockByTexture()` (shared) and `stageBlockUpdate()`/`refreshStaged()` (unique) on `defMutex_`. Concurrent `addBlock` vs `findBlock` races `byKey_`/`byStable_`/`entries_`/`blockDefs_` (unordered_map/vector mutation vs read = UB). `addBlockAlias` mutates `aliases_` unlocked while `findBlock` reads it.
- Items were left out of the def()-race fix entirely: `itemDef()` (`block.h`, ItemRegistry section) returns `const ItemDef&` with no lock — a reference into a vector that `addItem` can reallocate, the exact torn-read/UAF pattern blocks fixed by returning by value.
- Fix: enforce the contract (debug thread-id assert on all mutators, or lock mutators with the existing mutex) and make `itemDef` return by value under the shared lock.

### M4. No namespace-ownership validation anywhere in block registration — first-wins enables id squatting — VERIFIED
- `game/LithoKraft/src/content/blocks.cpp:226`: `BlockId registerBlock(BlockDef def) { return mutableRegistry().add(std::move(def)); }` — no check that `def.id` belongs to the caller's namespace, no format validation at all (empty id silently becomes `anonymous:<tex|name>`, `block.h` `blockStringIdOf`).
- `ContentRegistry::addBlock` is first-wins (returns the existing id on duplicate). Combined: any registrant that runs first can pre-register `lithokraft:<future block>` or another mod's `other mod:block` with attacker-chosen flags/behaviour; the legitimate owner's later registration silently receives the squatter's id/def. Today this is latent (no mod loading is wired into the game — see M9), but the API is the documented mod door.
- Fix: require/validate a namespace prefix per registrant (mod id from its manifest) in the mod-facing registration path; reject ids outside it.

### M5. `BlockId` silently truncates at 65,536 blocks — VERIFIED
- `block.h` `addBlockWithStableId`: `const uint32_t local = static_cast<uint32_t>(blockDefs_.size()); … return BlockId(local);` with `using BlockId = uint16_t;`. The 65,537th block returns id 0 (air) while its entry's `localIndex` is 65536; `findBlock` likewise returns `BlockId(e.localIndex)`. No cap check in `addBlock`/`addItem` path for blocks. An unsealed registry (debug, or release live-ops) registering >65k blocks (a script binding for `registerBlock` exists in the allowlist, `script_binding_allowlist.cpp:24`) silently aliases new content onto existing ids.
- Fix: fail closed (throw/nullopt + log) when `blockDefs_.size() > 0xFFFF` before insertion.

### M6. Restricted console callers reach destructive non-cheat commands, incl. arbitrary-path file truncation via `screenshot` — VERIFIED
- The caller-mask model gates *only* `cheat`-flagged commands/cvars (`concmd.cpp:493, 694`; `callerHasCheatAuthority()` = `cheatsAllowed && mask == kCallerFullAuthority`). The mask's individual bits are never consulted per-command. So any caller granted `kPermConsole` (mod `consoleCmd` / MCP `lithoscript_exec`) can also run the non-cheat set, including:
  - `quit` (game quits), `save`, `map <dimension>` — `game/LithoKraft/src/game/game_commands.cpp:110-135` (registered with no flags).
  - `screenshot <path>` — `game_commands.cpp:137-190`, path flows to `game/LithoKraft/src/game/screenshot.cpp:109-120` `writeFileBytes`: `create_directories(parent)` + `fopen(path, "wb")` — **no confinement**: an absolute/`..` path truncates any file the process can write, replacing it with PNG bytes.
  - `debug.defrag <store>` start/cancel — `concmd.cpp:1300`, not cheat-gated.
- Fix: gate `screenshot`/`genmap` paths to a screenshots dir under the config/save dir (prior review's fix, still open), and consider a second flag tier (e.g. `session`/`destructive`) for quit/save/map/defrag that restricted masks cannot run.

### M7. ModLoader: mod ids are unvalidated, duplicates are not rejected, and permission grants are keyed by id alone — VERIFIED
- `engine/src/mods/mod_loader.cpp:164-176` `parseModJson`: the only id check is `if (info.id.empty())`. No `[a-z0-9_]{1,64}` validation (contrast `ExtensionRegistry::isValidId`, and the assets manifest parser which does validate). `scan()`/`load()` contain no duplicate-id rejection (contrast `ExtensionLoader::registerStaticModule`: "duplicate static module id refused (first registration wins)").
- `mod_permissions.cpp` `PermissionStore`/`ModPermissionGate::resolve` key stored grants solely by `mod.id`; a version bump with the same permission shape deliberately does not re-prompt. Two folders claiming the same id both load and both inherit the first's approved grants; a new mod squatting a previously-approved id inherits its permissions with no prompt.
- Fix: validate id format in `parseModJson`, reject duplicate ids in `load()`, and bind stored grants to id + signer/version hash where available.

### M8. Net: there is no transport, no server, and no authoritative mode in the tree — anti-cheat is documentation only — VERIFIED
- `engine/src/net/` is a codec/channel library: "TRANSPORT: none here… the game hands it to UDP later" (`protocol.h:83-85`, `channels.h:29`). Grep finds **zero** socket/sendto/recvfrom code and **zero** `ConnectHandshake` instantiations in `game/` or engine app code.
- There is no server-authoritative vs non-authoritative mode switch, no input validation, and no peer-validation code anywhere. The anti-cheat claim rests on docs: `scaling.h:21-22` "Deterministic sim means nodes can validate each other (anti-cheat becomes anti-corruption)" and `version.h:19-21` "The sim is deterministic, so the server sends inputs + deltas" — no such server or validator exists. (Game-side determinism work exists, but nothing in net consumes it.)
- Also still open by design and documented in-tree: traffic is plaintext (`encryption.h`: `NullCipher` only, `deriveKeys` returns `NotImplemented`, `encryption.cpp:96-100`), the handshake offer list is unauthenticated (downgrade, `handshake.h` deployment note), and a spoofed Reject/ServerHello can kill a handshake (`bindPeer()` mitigation exists but no caller binds a peer).
- Fix direction: none in this pass — but JD-facing docs/status should state "multiplayer not implemented; anti-cheat unimplemented", not imply a mode exists.

### M9. MCP server is not wired into the game at all (known gap — confirmed still open) — VERIFIED
- No `McpServer` or `McpHttpServer` instantiation, no `--mcp-stdio`/`--mcp-http` flag, anywhere in `game/` (grep over game sources: only adapters). `game/LithoKraft/src/app/main.cpp` arg parsing has no MCP option.
- The game-side adapters exist precisely as unwired seams: `game.h:150-157` "MCP player-control adapters, **for the builder's --mcp-stdio wiring**: fill McpBackends{...} from these" (also `mcpSenses()`).
- The engine MCP implementation itself is comparatively strong (see Positives): deny-by-default mask 0, per-tool `requiredPerms` checked before handler invocation (`server.cpp:700`), rate buckets, HTTP Bearer (256-bit, CSPRNG, constant-time compare, loopback default, session cap 64 + idle timeout in `http_server.cpp`). It is simply unreachable in the shipped binary.
- Fix: builder wiring (construct `McpServer` with the adapters + a stdio thread, or `McpHttpServer` on 127.0.0.1, behind a CLI flag, plus the `mcp grant/revoke` console commands the headers assume).

## LOW

### L1. Replay window advances before AEAD authentication (latent — no cipher yet) — VERIFIED
`encryption.cpp:113-120` `openPacket`: `replay_.accept(counter)` runs before `cipher_->open()`. A forged packet with counter = highest+1,000,000 shifts the window (`encryption.cpp:41-47`); all legitimate packets then fall >64 behind and are dropped — one unauthenticated packet kills the session. Must be revisited when the builder lands the cipher (verify-then-commit the window, or hold candidate state until auth succeeds).

### L2. `getUVarint64` still accepts non-minimal encodings — VERIFIED (regression M4, Evening)
`quantize.cpp` `getUVarint64`: `{0x85,0x00}` decodes as 5, `{0x80,0x00}` as 0. Bounds/truncation handling is correct, but non-canonical encodings break any future byte-equality / transcript-signing scheme. Reject a final zero group in multi-byte encodings, or document acceptance explicitly.

### L3. MCP `net://version` still advertises protocol 1 — VERIFIED (regression L-net1, Evening)
`engine/src/mcp/server.cpp:1854`: `o.emplace_back("protocol", JsonValue::integer(LITHOLATTICE_PROTOCOL_V1));` while `kProtocolVersion == 3`.

### L4. Handshake deployment guidance is stale — VERIFIED (regression H2 doc half, Evening)
`handshake.h:35-41` still instructs "games MUST narrow their offer list to {2}" — pre-v3 text. With the default now {3} and no v1 codec, the guidance should read {3} (or {2,3} where v2 interop is wanted).

### L5. Interactive `executeLine` silently swallows an unterminated block comment — VERIFIED (2026-10-11 LOW, still open)
`concmd.cpp:605`: `executeCommandList(tokenize(line), 0, ctx)` uses the error-dropping overload; `echo hi /* oops` runs nothing and prints nothing. The file path reports the same error. Use the two-arg `tokenize` and print the error.

### L6. `bumpCVar` maps a NaN delta to 0 instead of refusing — VERIFIED (2026-10-11 LOW, still open)
`concmd.cpp` `bumpCVar`: `if (std::isnan(nv)) { q = 0; }` — `incrementvar myint nan` (raw `strtod` accepts `nan`/`inf`) silently zeroes an int cvar; for float cvars non-finite values flow into the `%.9g` canonical form. Fail closed on non-finite delta/result.

### L7. Console `tick()` catch-up in `Game::update` is uncapped per frame — SUSPECTED
`game/LithoKraft/src/game/game.cpp:1150`: `for (; lastScriptTick_ < wt; ++lastScriptTick_) consoleRegistry_.tick();` runs on the frame thread. After a main-thread stall the sim thread may be thousands of ticks ahead (512 TPS); one frame then pumps all of them, each `tick()` also calling `defragService_->pump()` (`concmd.cpp` end of `tick()`). Per-script step/wait budgets bound script work, but there is no per-frame cap on iterations or defrag pumps. Cap catch-up per frame (e.g. 64) and carry the remainder.

### L8. `ModLoader::parseModJson` casts a JSON double to `uint32_t` unchecked — VERIFIED
`mod_loader.cpp:171-172`: `sdkStr = std::to_string(static_cast<uint32_t>(sdk->num));` — for `"sdk": 1e30` or a negative number the double→unsigned conversion is out-of-range UB (the file's own `parseUint32` exists precisely to avoid this class). Route through a range check first.

### L9. MCP HTTP has no Origin/Host validation — SUSPECTED, mitigated
`http_server.cpp` checks Bearer auth, framing, content-type and (stateless era) era headers, but never `Origin`/`Host`. DNS-rebinding is largely defeated by the 256-bit Bearer token (a browser cannot set `Authorization` cross-origin without a CORS preflight this server never grants), so this is defence-in-depth only: add an Origin/Host allowlist check before auth when the server is wired (M9).

### L10. `ScriptEngine::run()` still has no C++ exception guard — VERIFIED (prior finding, incomplete)
`script_engine.cpp` `run()` (and the file generally) contains no try/catch; the prior review's item stands. AngelScript `Execute()` itself is a C++ call — a throwing bound callback propagates through the script context, skipping the `executingCtx_` restore pairing logic in callers that assume it. Wrap `Execute()` and restore context/attribution in a guard.

### L11. `ContentRegistry::addBlock` relies on `assert` for derived stable-id collisions; release build throws instead — VERIFIED, theoretical
`block.h` `addBlock`: `assert(byStable_.find(derived) == byStable_.end()); return addBlockWithStableId(d, derived).value();` In NDEBUG the assert vanishes and a collision makes `.value()` throw `bad_optional_access` uncaught at registration. Practically unreachable for FNV-1a-64 derived ids (birthday bound ~5 billion entries, and M5's 65k cap hits first), but the fail mode should be a logged refusal, not a throw.

### L12. Two divergent mod-manifest systems coexist — VERIFIED
`engine/src/assets/mod_manifest.*` (schema-1 JSON, `permissions:[{api,reason}]` with APIs like `filesystem.save`/`network`, signature/trust model — the one the game actually uses, content packs only) vs `engine/src/mods/mod_loader.*` (different schema: `sdk`, permission *names* → kPerm bits, permission gate — used by **no game code**, selftests only). Two permission vocabularies and two trust stories for the same concept is a standing confusion/supply-chain risk when script/native loading is eventually wired. Decide which is canonical before wiring either to script/native mods.

---

## CLAIM VERIFICATION

| Claim | Verdict | Evidence |
|---|---|---|
| Handshake offers {1,2,3} | **FALSE** (as offer) | Default offer is {3} only (`handshake.cpp:86`, `protocol.h:110`). {1,2,3} is only `knownProtocolVersions()`, the accepted-framing set, installed by nobody. See ⭐ answer. |
| BlockUpdate delta codec saturation fix | **HOLDS** | `protocol.cpp:77-95` `satAdd`/`satDelta` accumulate in int64 and clamp; decode uses `satAdd` for pos/vel (`:313-330`). Quantize bounds: `quantize.cpp:77-110` (varint overflow, uvarint 5-byte cap), `:116-138` (uvarint64 10th-byte check). `packQuat` NaN guard + `posToFixed` saturation in `quantize.h:104-121` also hold. |
| Packet parsing bounds | **HOLDS** | Exact framing (`in.remaining() != h.payloadLen` → drop) in channels and snapshot receiver; header version rejection is explicit (`HeaderReject`); snapshot decode rejects id overflow, spawn-of-known, delta-for-unknown, out-of-order ids, unknown removals (`protocol.cpp:257-360`); reliable channel caps in-flight (64), reorder buffer (256) and ready queue (256, drop-counted). |
| Authoritative vs non-authoritative server modes | **FALSE / absent** | No server, transport, or mode exists (M8). |
| Anti-cheat via deterministic sim | **OVERSTATED** | Determinism exists game-side; no net-layer validation, server, or input checking consumes it (M8). |
| Script sandbox: no file/network/system from scripts | **HOLDS** (bindings surface) | Deny-by-default exact-match allowlist (`script_binding_allowlist.cpp`, `script_engine.cpp:182-186` gate in `bindGlobalFunction`); core registrations are only std-string + `log`; no file/socket/process bindings exist in the table; forbidden-token heuristic on runtime registrations. Caveats: quota evasion (M1), UAF path (M2), and native C++ mods are in-process by design (honestly documented in `confinement.h` threat model). |
| Script quotas (CPU + memory) | **OVERSTATED** | CPU path is sound (line-callback deadline every 256 lines, per-tick budget → throttle → suspend, memory-trip aborts the in-flight context). Memory path is defeatable per M1. |
| Hot-swap `def()` race (prior fix) | **HOLDS for blocks / INCOMPLETE for items** | `shared_mutex defMutex_`, `blockDef()` returns by value under shared lock, `stageBlockUpdate`/`refreshStaged` take the unique lock, epoch is `std::atomic` bumped inside the lock (`block.h` ContentRegistry hot-swap section). Items: `itemDef()` returns an unlocked `const&` (M3). |
| Sealed vs `hot_swap` flags | **HOLDS** | `game/LithoKraft/src/app/main.cpp:141-190`: any `sealed:true` seals (wins over hot_swap); release stays open only if every loaded mod is Verified **and** a verified mod sets `hot_swap`; debug open unless sealed; decision runs even with no `mods/` dir. Registry side: `seal()` makes `addBlock/addItem/stageBlockUpdate` refuse (`block.h:422,434,518,551,561`). |
| Mod conflict rule: first registered wins | **HOLDS** (registry/extension layers) / **OVERSTATED** (system-wide) | `ContentRegistry::addBlock/addItem` return the existing entry on duplicate (never overwrite); `ExtensionLoader::registerStaticModule` refuses duplicates ("first registration wins"); `ExtensionRegistry` refuses duplicate paths. But `ModLoader` has no duplicate-id rule at all (M7), and first-wins without namespace ownership enables squatting (M4). |
| Registry single-threaded mod-init contract | **OVERSTATED** | Documented in three places, enforced in none (M3). |
| Console permission-laundering fix | **HOLDS for keybinds + `wait` suspensions / FALSE as a blanket claim** | Keybind mask stamping + dispatch re-install, suspended-script mask stamping with the re-suspend ordering fix, and script-console `ScopedCallerPermissions` all verified in code. Aliases were missed (H1). |
| MCP stdio server wired in `main.cpp` | **FALSE — gap confirmed** | M9. Adapters exist; no server, no flag. |

## POSITIVES (verified, brief)
- Handshake parsers are strict (exact lengths, list cap 32, SDK-unset fails closed, client verifies the chosen version was offered).
- Mod net confinement (`net/confinement.cpp`): fail-closed, subtraction-based budget compare (wrap fixed), abuse suspension, denial-log rate limiting, mod wire types forced ≥256.
- AngelScript permission revocation genuinely unbinds via full sandbox rebuild, with a fail-closed guard against rebuilding mid-execution (`script_engine.cpp` `rebuildSandbox`).
- Console hardening budgets (`wait` clamp 512, total-wait 4096, steps 16384, suspended cap 64, alias depth 16, exec depth 8) and the Linux `exec` TOCTOU hardening (`O_NOFOLLOW` + fstat + `/proc/self/fd` re-validation) are real.
- MCP engine code (once wired): deny-by-default, per-tool permission checks, rate limits, HTTP bearer + loopback + session cap/idle expiry.
- Extension loader: cycle detection, depth cap, default-deny first-party non-propagation, rollback on failed load, exception containment per extension (`extension_runtime.cpp`).

---

## REGRESSION VERDICTS (prior reviews)

Sources: `Code-Review-Full-Tree-2026-10-12.md` (FT), `Code-Review-Morning-Report-2026-10-12.md` (MO), `Code-Review-Evening-2026-10-12.md` (EV), `Code-Review-2026-10-11.md` (11).

| Prior finding | Verdict |
|---|---|
| FT-2 CRITICAL test signing key could ship | **FIXED-CONFIRMED** — `main.cpp` `bakedKeyIsTestKey()` guard: release builds `std::exit(1)`, debug warns loudly, runs before the mods early-return. (Key itself remains in git history — rotation is JD's, already his plan.) |
| FT-3 HIGH `scanMods()` early-return skipped trust anchor + seal | **FIXED-CONFIRMED** — anchor set and seal decision run over the empty set when `mods/` is missing (`main.cpp:134-190`). |
| FT-4 / 11-HIGH console `tick()` double-`wait` permission laundering | **FIXED-CONFIRMED** — `concmd.cpp:852-868`: re-suspend is queued *before* the ambient mask is restored, with an explicit anti-laundering comment. (Sibling alias hole = new H1, **STILL-OPEN** by extension.) |
| FT-5 `def()` torn-read race + epoch | **FIXED-CONFIRMED** for blocks (see Claim table); items half **INCOMPLETE** (M3). |
| FT-H1 / EV-H1 block alias cycle → stack overflow | **FIXED-CONFIRMED** — `findBlock` is iterative with a hop cap; `addBlockAlias` rejects self-alias and cycles at registration (`block.h`). |
| FT-9 HIGH net encode/decode integer UB | **FIXED-CONFIRMED** — `satAdd`/`satDelta`, quantize overflow guards (see Claim table). |
| FT JSON parser recursion depth | **FIXED-CONFIRMED** — `core/json.cpp:27-35` depth cap 128, fail-closed. |
| FT `ui_show` binding bypassed mod-UI hardening | **FIXED-CONFIRMED** — `game/LithoKraft/src/game/ui_script_bindings.cpp:73-76` routes through `ModUiFactory::createPanel` (pre-scan, budget, badge); direct `buildWidget` explicitly prohibited in comment. |
| FT throwing console command corrupts transient state | **FIXED-CONFIRMED** — RAII `TransientGuard` in `executeLine` (`concmd.cpp:585-606`) plus `ExecDepthGuard` in `executeFile`. |
| FT ScriptEngine::run() exception skips restore | **STILL-OPEN / INCOMPLETE** — no try/catch in `script_engine.cpp` (L10). |
| FT script quota UAF on unload | **INCOMPLETE** — operator-new half fixed via `shared_ptr<QuotaAccount>`; AS-shim half still raw-pointer (M2). |
| FT `verifyModSignature` I/O error → Unsigned (fail-open) | **FIXED-CONFIRMED** — `assets/mod_signature.cpp:47-52`: filesystem error returns `Invalid`. |
| FT `packQuat` NaN / `posToFixed` overflow UB | **FIXED-CONFIRMED** — `quantize.h:104-121`, `quantize.cpp` packQuat finite guard. |
| FT confinement `windowBytes + bytes` wrap | **FIXED-CONFIRMED** — subtraction compare in `confinement.cpp` `checkSend`. |
| FT/11 MCP HTTP session table uncapped | **FIXED-CONFIRMED** — cap 64 with LRU eviction + idle timeout + authenticated DELETE (`http_server.cpp:48-52, 297-339`). |
| EV-H2 handshake "{1,2,3} offers" claim vs default {3} | **STILL-OPEN** — behaviour unchanged ({3}); the doc half also remains: deployment guidance still says narrow to {2} (L4). This review's ⭐ answer restates the literal fact for JD's H2 decision. |
| EV-M5 handshake downgrade to nonexistent V1 codec | **STILL-OPEN** (documented/accepted, KEX absent). Exposure is narrower than at EV time only because the default offer is {3}: a MITM stripping to {1} gets NoCommonProtocol against a default server; it bites only if a game configures {1,2,3}. |
| EV-M4 `getUVarint64` non-minimal encodings | **STILL-OPEN** (L2). |
| EV-L-net1 MCP `net://version` = protocol 1 | **STILL-OPEN** (L3). |
| EV-L-net2/3 "zigzag" doc error / stale "v2" protocol header | **STILL-OPEN** (cosmetic) — `protocol.h:1-2` still says "snapshot wire protocol (v2)"; the codec is DDNet-style sign-magnitude, not zigzag. |
| EV-M1 `registerKind()` "mods add kinds, no plumbing" overstatement | **FIXED-CONFIRMED (as documentation)** — `block.h` now carries the NOTE that payload storage is private and new kinds are engine-level only. |
| 11-LOW unterminated `/*` silent on interactive lines | **STILL-OPEN** (L5). |
| 11-LOW `bumpCVar` NaN → 0 | **STILL-OPEN** (L6). |
| 11-LOW `screenshot`/`genmap` unsandboxed paths | **STILL-OPEN** — `genmap` is cheat-gated; `screenshot` is not, and is now also reachable by restricted console callers (M6). |
| 11 net plaintext: spoofed Reject/ack, downgrade (accepted, builder KEX) | **STILL-OPEN** — unchanged; `bindPeer()` exists but has no caller. |
| MO registry-size selftest / narrowing fixes | Out of scope / no regression observed; `engine_world` suite passes. |

**Net regression summary: no previously-fixed in-scope finding was found re-broken in code. The open set is: EV-H2/M5/M4, L-net1/2/3, the 2026-10-11 LOWs (L5/L6/screenshot), FT script exception + quota-UAF halves — plus this review's new H1 (alias laundering), which is a sibling of a "fixed" finding, not a re-break of it.**

---
## Appendix G — Game Content, Items, Dimensions, Selftests

# Code Review 2026-10-13 — Game Content / Items / Dimensions / Selftest Quality

Scope: `game/LithoKraft/src/` content, items, inventory, dimensions, game commands, `app/` selftests, `headless/`. Read-only; test runs in `build-linux` only (chipor, scout, items, headless binaries + one full `LithoKraft --selftest` run, log at `/tmp/review-2026-10-13/selftest-run.log`).

**Counts: CRITICAL 0 · HIGH 2 · MEDIUM 7 · LOW 8**

---

## HIGH

### H1. Player inventory is NOT persisted — everything in it is lost on save/load — VERIFIED
- `game/world_save.cpp:172-210` `writePlayer()` writes exactly: version, pos, yaw/pitch, dimension, hot slot, **hotbar (BlockIds)**, v2 stats, v3 vitals. No inventory block. `readPlayer()` (:212-261) mirrors this.
- `game/game.cpp:665` builds the `PlayerMeta` the same way: `PlayerMeta m{player_.pos, …, hot_, std::vector<BlockId>(hotbar_…)}; m.stats=…; m.vitals=…;` — `player_.inventory` never appears in any save path (grep-verified).
- Impact: items from `give`, item-entity pickups (`game.cpp:1400` `player_.inventory.addItem(...)`), and the Fragment vanish on relaunch. The Fragment loss is *masked*: `game.cpp:452` re-grants on every boot because `grantStartingItems` only checks the in-memory inventory. The comment at `game.cpp:449-451` — "a restored player keeps theirs" — is **false across restarts** (true only within one process).
- Fix: add a v4 inventory block to `player.dat` (42 carry + 8 gear, itemId+count per slot), or explicitly document inventory as session-only and stop routing pickups/`give` into a black hole (see H2).

### H2. Two disconnected inventories: the real 42-slot `Inventory` is invisible and unusable; HUD/hotbar/creative bar are a separate BlockId array — VERIFIED
- `game/include/lithokraft/game/player.h:16` — `Player` holds `Inventory inventory; // 42 carry + 8 gear`.
- `game/include/lithokraft/game/game.h:372` — Game separately holds `std::array<BlockId, kHotbarSize> hotbar_{kWaferSilicon, kRedTrace, …}` (8 slots). This — not `player_.inventory` — is what the HUD renders (`game.cpp:1578` `m.hotbar = hotbar_;`), what Place/Break/PickBlock/Drop operate on (`game.cpp:834, 1007-1016, 1063-1083`), and what `player.dat` persists (H1).
- The "inventory" dialog is a creative block palette, not the player's inventory: `game/game_ui.cpp:88-117` `openInventory()` — title literally "Creative inventory", one slot per `BlockId(i+1)` in the registry, click → `pickInventoryBlock()` → writes `hotbar_` only.
- Consequence: the Fragment sits in `player_.inventory.hotbar()[0]` while the HUD's slot 0 shows Wafer Silicon. No code path displays, selects, or uses any `ItemStack`. `give` and pickups feed a store nothing reads except `countOf` in the grant check.
- Fix direction: one inventory. Either make HUD/hotbar a view over `Player::inventory` hotbar slots, or delete the pretence and label the current system "creative bar" everywhere (docs currently imply a real inventory — see Claim Verification).

### (Related, H2b) Per-tool world-interaction handlers are really absent — and nothing in production assumes they exist — VERIFIED (known gap, confirmed clean)
- Grep for `ToolAction::` outside `items.cpp`/`items.h` hits **only** `app/items_selftest_main.cpp`. No production consumer of `toolAction`, `meleeDamage`, `defense`, `projectile`, or `returnsToOwner` exists in `game/`, `app/`, or `content/`.
- Even the one farming verb that exists as a function, `silicaTill()` (`content/silica.cpp:90`), has exactly one caller: `app/silica_selftest_main.cpp:185`. SeedOR's `ToolAction::Till` is never dispatched; tilling is unreachable in-game.
- So items are registry data + models + icons only. Nothing half-wired or crashing — but any doc/README claim that the tools *do* anything is false today (see Claims).

---

## MEDIUM

### M1. Spawn floats ~11 blocks in BOTH dimensions — root cause VERIFIED: a literal `+12` in both setup functions
- Wafer: `game/structures.cpp:119` — `return {float(ox) + 18.f, float(P) + 12.f, float(oz) + 40.f, …}` where the demo platform is filled up to `P` and the circuit sits at `P + 1` (`structures.cpp:12-16`). Feet spawn at `P+12` = **11 blocks above the circuit surface**.
- Organic: `game/dimensions.cpp:22-26` — `const int h = std::max(0, w.surfaceY(c, c)); return {float(c), float(h) + 12.f, float(c), …}`. Feet 12 above the highest block (11 above the standable surface).
- Measured in this run's selftest: wafer spawn `(58.0, 277.0, 80.0)` vs platform top `P = 265`.
- `spawnPoint_` inherits the airborne point (`game.cpp:599`), so `kill`/respawn repeats the drop every time. No fall-damage system was found, so impact is a visible 11-block fall at every spawn/respawn, not damage.
- Fix: wafer `P + 1` (feet on platform; circuit row is at `P+1`, offset spawn z to a clear row if needed), organic `h + 1`. Same magic number in both places suggests the wafer's deliberate "drop-in" was copied to organic.

### M2. Tick-debugger commands desync Game pause state from SimThread pause state — VERIFIED
- `game/game.cpp:165-183`: `b.tickFreeze` does `sim_->setPaused(true)` and `b.tickUnfreeze` does `sim_->setPaused(false)` — neither touches `Game::paused_`. Compare `Game::setPaused` (`game.cpp:857-873`), which keeps `paused_`, the input context, the pause UI, and `sim_->setPaused(paused_ && cfg_.pauseSim && !benchSet_)` in sync.
- Failure modes: (a) `tick_unfreeze` while the game is pause-menu paused silently resumes the sim behind the pause menu (the exact invariant the audit-MEDIUM fix in `setPaused`/`switchDimension` protects); (b) `tick_freeze` leaves `paused_ == false`, so `Game::update` still treats the frame as playing (`playing = captured_ && !inventoryOpen_ && !paused_`), `worldQuery`'s `gamePaused` hook reports unpaused, and a later Esc-pause/unpause cycle overwrites the freeze without trace; (c) `tick_step`'s `stepTicks` auto-pauses the SimThread (`sim_thread.h:105`) with the same invisible-to-Game effect.
- The SimThread side is correct (morning report verified stepping uses the live pipeline); this is purely the game-side state mirror. Fix: route freeze/unfreeze through a Game-level `tickFrozen_` flag folded into the single `sim_->setPaused()` computation, or make the bindings call `setPaused`-adjacent logic.

### M3. ChipOR selftest registry-size guard is stale — the CHECK is wrong, the registry is right — VERIFIED (root-cause verdict)
- `app/chipor_selftest_main.cpp:62-63`: `check(reg.size() == size_t(kSilicaTilledSoil) + 1, "registry size matches canonical guard")` with a comment claiming it is "not hardcoded, so later appends don't break it".
- Reality: `kSilicaTilledSoil = 54`, so the check demands `reg.size() == 55`. Two append-only blocks landed after Tilled Silicon — Structure Block (55) and Certificate Table (56) — so `reg.size() == 57` (56 blocks + Air). Run result: `[FAIL] registry size matches canonical guard` while **every other chipor check passes**, including `id 51 = lithokraft:chipor`, `id 52 = lithokraft:chipor_viewport`, `id 50 still dead_memory_cell (no renumber)`.
- The registry itself is verified correct and append-only (see Verified-Clean). This is the *same* stale-guard pattern the morning report fixed in `content_selftest.cpp` (which now uses `< reg.size()` + `idOf` and passes) — the chipor copy was not updated when the two blocks appended.
- Fix: `reg.size() > size_t(kChipORViewport)` (or `== size_t(kCertificateTable) + 1` if an exact guard is wanted — but that re-creates the same trap on the next append).

### M4. Scout selftest: wafer generator built without a ModelResolver — the TEST is wrong, production is right — VERIFIED (root-cause verdict)
- Failure (ran it): `wafer generator failed: feature 'litho_starfish': 'lithoform_stamp' needs a ModelResolver (the game did not supply one)` → `[FAIL] locate: wafer generator builds` and `[FAIL] structures: wafer generator builds`; exit 2. All other ~85 scout checks pass.
- `game/scout_selftest.cpp:56-58` `makeWaferGen()` calls `ColumnGenerator::create(json, seed, "wafer", blockRegistry(), err, waferSurfaceRules())` — **no resolver argument**.
- Production does it right: `game/dimensions.cpp:43-52` passes `model::makeLithoStampModelResolver()` on both the pack and built-in paths. `assets/worldgen/wafer.json:2082-2083,2112` contains `litho_starfish` / `lithoform_stamp` features that require it.
- **Latent second site**: `scout_selftest.cpp:350` builds the structure-stripped comparison generator with the same omission. It is unreachable today (test returns when the first generator fails) but will fail identically the moment M4's first site is fixed — the stripped JSON still contains the stamp features (only `"structure"` features are removed, :335-347).
- Fix: pass `model::makeLithoStampModelResolver()` at both sites (include from `litho_stamp_models.h`, as dimensions.cpp does).

### M5. Dimension registry contains exactly two dimensions — Cosmic, Underwafer, and Void do not exist in code — VERIFIED
- `game/dimensions.cpp:65-82` `createDimensions()` registers only `wafer` and `organic`, plus the `overworld` alias. Repo-wide grep of game src + `assets/worldgen/` for `cosmic`/`underwafer` (outside MCP numbering, below) returns nothing; `assets/worldgen/` contains only `wafer.json` and `organic.json`. There is no Cosmic worldgen JSON, no dimension registration, no stub `Dimension` — `map cosmic` returns "unknown dimension", `--dim cosmic` logs `unknown dimension … (wafer|organic)` (`game.cpp:432`).
- Half-wired residue: `game/mcp_player_control.cpp:14-22` `dimensionNumber()` already maps `underwafer=2, cosmic=4, void=5, ether=6, undefined=7` — numbering for dimensions an MCP pilot can never be in.
- This matches the known gap; stated here as the verified claim verdict: of the 5 core design dimensions, **2 are playable**.

### M6. Fragment: item granted, but no UI exists and its state model has zero production callers — VERIFIED
- No Fragment book UI: grep of `game_ui.cpp`/`game_ui.h` finds no Fragment dialog/page; `GameUi::openInventory` is the creative palette (H2). Using the Fragment is impossible — it isn't even in the visible hotbar (H2).
- `FragmentState` (`game/fragment_state.cpp/.h`: tier, markdown unlock, LFS1 serialize/deserialize) has **zero production call sites** — grep hits only `fragment_selftest.cpp` and a comment in `items.cpp:33`. `upgradeFragment()` is called only by the selftest's simulated Spark hook. (2026-10-11 review noted the same; still open, including its missing "game thread only" doc line.)
- `FragmentGallery` is used exactly once — screenshot capture into an in-memory gallery (`game.cpp:1118-1131`, `game.h:252`) — with no UI to view it and no persistence (not in `player.dat`, H1).
- What does exist and works: markdown renderer + gallery/state data models, all selftest-covered. The missing piece is purely the player-facing wiring.

### M7. Main selftest is currently red beyond the known flakes — save round-trips ×4 and mesher fail; root cause NOT isolated — VERIFIED-OBSERVED / SUSPECTED cause
- My run (`build-linux/LithoKraft --selftest`, log `/tmp/review-2026-10-13/selftest-run.log`): 891 PASS, FAILs = audio streaming (engine, known flake class), **vulkan (environmental)**, and game-side:
  - `save: blocks, cell states and component memory / timers restore bit for bit`
  - `save: the restored world ticks identically to the original (70 more ticks)`
  - `save: save / edit / save / edit+tick / save, then regenerate + load matches bit for bit`
  - `save: the partially saved world ticks identically after load`
  - `mesher produces quads for a surface sub-chunk` (`app/selftest.cpp` ~:296)
- Baseline ctest supplied for this review also shows `lithokraft_selftest ***Failed`, and the segfault that previously masked everything past `runSignalSelfTest` is fixed (suite now runs to completion — see Regression), so these are newly *visible*, not necessarily newly *broken*.
- Suspects, not proven: (a) the selftest constructs `WorldSave save(dir)` with no material registry (log spam `save: no PBR material registry; chunk material bytes will not persist` interleaves the FAIL line) while Game passes `cfg.materials` — though `sameCells` doesn't compare materials; (b) the mesher check meshes column (2,2) at a *global* max `surfaceSy` gathered across all columns — a column whose own surface is lower yields an empty snapshot and 0 verts, i.e. the check's construction is fragile; (c) genuine round-trip regression. Needs one isolated re-run on a quiet tree before assigning blame.
- Selftest-quality aggravator: the suite uses fixed shared temp paths (`lithokraft_save_selftest`, `lithokraft-selftest-scripts`, `lithokraft-selftest-logs` in `temp_directory_path()`), so parallel selftest/ctest invocations collide — the same class as the known engine_assets/engine_audio alternating flakes.

---

## LOW

### L1. LithographOR remains a commented-out row with a dead SimRole — VERIFIED (unchanged, by design but untidy)
`content/blocks.cpp:69` — the `lithokraft:lithographor` `r.add(...)` is commented `// DISABLED`. There is no `kLithographOR` extern in `blocks.h`, yet `SimRole::LithographOR` remains in the enum (`blocks.h:107`). No id was consumed (the comment preserves the slot in source order only — re-enabling it *mid-list* would renumber PortOR onward; it must be appended at the end when it lands, and the comment doesn't warn about that).

### L2. `kFusor` header comment is still garbled copy-paste debris — VERIFIED, STILL-OPEN from Full-Tree 2026-10-12
`content/include/lithokraft/content/blocks.h:51` — one line concatenates fragments of at least five different blocks' doc comments ("circuit breaker /// defines a chip face I/O in the lithography area /// JIT-compiled Trace graph … beacon equivalent … long-range receivor … opaque insulator … address decoder…"). Cosmetic, but it's the registry's public header.

### L3. Headless harness: its blocks.cpp "guard patch" is now a no-op, and three files still claim the shipped game cannot start — VERIFIED
- `src/headless/patch_blocks.cmake` replaces the string `std::abort();  // ids in blocks.h out of sync with the rows` — that text no longer exists in `blocks.cpp` (the only abort is `resolveId`'s, differently worded, `blocks.cpp:161`). Verified: `diff blocks.cpp build-linux/game/src/headless/blocks_patched.cpp` = **one added duplicate `#include <cstdio>`** (already included at `blocks.cpp:3`), nothing else.
- Yet `headless_harness.h:13-16`, `src/headless/CMakeLists.txt`, and `patch_blocks.cmake` all still state the registry "registers 40 rows; the guard expects 36 and calls std::abort()", "the shipped game cannot start in this tree", "builder owns the fix". All false today — the registry refactor removed the guard entirely.
- Residual risk is process, not code: the harness links its own copy of blocks.cpp instead of the `lithokraft_content` object, for a reason that no longer exists, and the stale docs could send a future debugger chasing a phantom desync. The harness itself is otherwise strong (see Verified-Clean) and passes 0 failures in this run.

### L4. Game README is stale in checkable places — VERIFIED
`game/LithoKraft/README.md`: claims a **9-slot** hotbar, keys `1–9`, and a start hotbar ending "9 Color Lamp 16" — code has 8 slots (`game_ui.h:20`, `game.h:372` defaults end at `kSwitch`). Claims "there is no gravity yet, so flight is always on" (controls table + Space row) — locomotion/gravity has landed (HUD code itself notes "gravity exists now", `game.cpp` ~:1597). Uses pre-rename names "Thresholder, Pulsar, Memory Cell" in the feature list. Its two-dimension claim, however, is accurate (M5).

### L5. `give` accepts any numeric id — VERIFIED
`game/game_commands.cpp:56-80` validates parse, `id != 0`, count range — but never `isItemId(id)`. `give 999999 1` reports success and stores a stack no def, icon, or name exists for (in the already-invisible inventory, H2). The Full-Tree report's `give 0` bug is fixed (see Regression); this is the remaining half. Fix: reject `!isItemId(id)` with the usage line.

### L6. Selftest quality: brittle exact-count checks and leftover DEBUG spam — VERIFIED
- `app/selftest.cpp`: `check(lamps == 9 && lit == 7, …)`, `check(clamps == 35 && clit == 33, …)` (~:110-111) hardcode demo-circuit contents; pack checks hardcode `sh.size() == 3`, `tex.palettes().size() == 5`, `tex.textures().size() > 80`, `rm.models().size() >= 24`. Any content addition fails the suite without anything being broken — the same failure mode as M3, waiting to happen.
- `app/selftest.cpp:100,105` — unconditional `DEBUG wafer:` / `DEBUG col …` printf dump (with a per-column surface histogram loop) ships in the production selftest output.
- The spawn check (`app/selftest.cpp` ~:85-91) asserts only `sp.y > surfaceY(spawn column)` and air at the spawn cell — it compares against the *natural terrain* height at that column (252), not the demo platform the player actually stands on (265), and imposes no upper bound. It **cannot fail** for the M1 float; it passed in this run with spawn 25 blocks above the checked surface. A check of the form `sp.y <= platformTop + 2` would have caught M1 years of spawns ago.
- Vulkan probe: when a loader exists but instance creation fails environmentally (`VK_ERROR_INCOMPATIBLE_DRIVER` on this GPU-less VM) the check is a hard FAIL rather than the SKIP path used when no loader is found (`app/selftest.cpp` ~:303-307) — a permanent red on every GPU-less CI box, training readers to ignore the suite.

### L7. "Organic World" name survives the rename to "Organic" — VERIFIED, cosmetic
`game/dimensions.cpp:73` `ow->name = "Organic World"` and `dimensions.h:1-2` header comment; the dimension *id* is `organic` and the locked one-name convention says "Organic". The name surfaces in the window title/status line (`game.cpp` statusLine uses `active_->name`). The README (L4) also still says `organic = "Organic World"`.

### L8. Inventory model nit: `countOf` can overflow in theory, `addItem` count cap is per-call only — SUSPECTED/minor
`game/include/lithokraft/game/inventory.h:97-104` sums into `uint16_t` across 42 slots × 512 = 21,504 max — actually fits; withdrawn on arithmetic. (Kept as a note: no bug found in the 42-slot model itself — 8 hotbar + 2 offhand + 32 main = 42, gear 8 separate, `kMaxStack = 512`, fill order hotbar→offhands→main, version counter on mutation: all as documented and selftest-covered.)

---

## Verified clean (checked, no finding)

- **Block registry refactor — CORRECT.** Script-compared all three lists: 56 `extern const BlockId` in `blocks.h`, 56 active `r.add(def(...))` rows in `blocks.cpp` (57th grep hit is the commented LithographOR, L1), 56 `resolveId(blockRegistry(), "lithokraft:…")` definitions — **all three in identical order with identical string ids**. The task brief's "54" is just pre-Structure-Block/Certificate-Table counting. Append-only holds: ChipOR=51, ChipORViewport=52, SilicaGrass=53, Tilled=54, Structure=55, CertTable=56 (Air=0, registry size 57).
- **`kSilicaTilledSoil` mapping — CORRECT.** `blocks.cpp` registers `"lithokraft:tilled_silicon"` and `kSilicaTilledSoil = resolveId(..., "lithokraft:tilled_silicon")`; `content_selftest.cpp` asserts `reg.idOf(kSilicaTilledSoil) == "lithokraft:tilled_silicon"` and passes.
- **Items — registration CORRECT.** 15 defs in `items.cpp` (seed, Fragment, BashOR, PickOR, ChopOR, HeapOR, SeedOR, SlicOR, Firewall, Ping, Callback, DebuggOR, Sniffor, Atlas, ChipOR-block-backed), offsets `kFirstItemId+0..+13` append-only in `items.h`, all 15 also registered into the unified ContentRegistry with ChipOR `placesBlock` = ChipOR block stable id. `lithokraft_items_selftest`: **0 failures** (ran it).
- **Icons/models — PRESENT.** Every item `modelStem` resolves to a checked-in `.lthm` (incl. `chipor-demo`); every item icon resolves through the pack stack — main selftest's block-icon and item-icon checks both PASS in this run (see Regression: the 2026-10-12 missing-icons finding is fixed).
- **Fragment grant — CORRECT as far as it goes.** `inventory.h:grantStartingItems()` grants iff `countOf == 0`; called once at boot after save restore (`game.cpp:452`); items selftest verifies grant, idempotency, and hotbar-slot-0 placement *in the Inventory model* (the disconnect is H2, not the grant).
- **ChopOR tier — CORRECT.** `items.cpp`: `tier 1, material "trace_trunk"`, `ToolAction::Chop`, melee 4; items selftest verifies it is the only ChopOR and no tier ladder was invented.
- **Dimensions that exist, work.** Wafer (5 biomes, demo-circuit setup, seed-pinned — the selftest's "wafer is seed-independent, organic changes with seed" check PASSes; seed pin itself is another reviewer's scope) and Organic (plains+hills, caves/veins checks PASS, `overworld` alias PASS). `Dimension::load/unload`, F6/`map` switching with pause re-application (`game.cpp:651-652`) all present.
- **Headless harness quality — GOOD apart from L3.** Drives the real Game + real UiRoot (menu/pause/settings/focus-loss flows), plus chunk-streaming walk to +384 blocks and a streamed-column save/relaunch persistence test. Ran it: **0 failures**.
- **`game_ui_selftest.cpp`** — substantive widget-level checks (hotbar selection, inventory modal open/close semantics, settings round-trip + corrupt-file defaults, menu/pause dispatch). No can't-fail patterns spotted in skim.

## Claim verification (docs/README vs code)

| Claim | Verdict |
|---|---|
| Two dimensions (wafer, organic) playable — game README | **HOLDS** (and only two exist, M5) |
| Five core dimensions (design docs / locked design) | **OVERSTATED as a shipped feature** — 2 of 5 in code; Cosmic/Underwafer/Void have no JSON, registration, or stub (M5) |
| 15 lore tools/items registered (items workstream) | **HOLDS** — as registry data, models, icons |
| Tools usable / interact with the world | **FALSE today** — zero production handlers; not even SeedOR→`silicaTill` is wired (H2b) |
| The Fragment is the starting item, granted first tick | **HOLDS** (grant verified) — but it cannot be opened: no Fragment UI exists (M6); "open inventory → open Fragment → read/write immediately" (items.h lore) is **FALSE** |
| 42-slot inventory (8 hotbar + 2 offhand + 32 main + 8 gear) | **HOLDS as a data model; FALSE as a player feature** — invisible, unusable, unpersisted (H1/H2) |
| HUD hotbar shows your inventory | **FALSE** — HUD shows the separate creative BlockId bar (H2) |
| 9-slot hotbar / start hotbar with Color Lamp 16 (README) | **FALSE** — 8 slots (L4) |
| "No gravity yet, flight is always on" (README) | **FALSE** — locomotion/gravity landed (L4) |
| Tick debugger (tick_freeze/step/unfreeze) "wired and working" (morning report) | **OVERSTATED** — SimThread stepping works; game-side pause mirror desyncs (M2) |
| ChipOR ids 51/52, append-only, no renumbering | **HOLDS** — only its selftest size guard is stale (M3) |

## Regression verdicts (vs Code-Review 2026-10-11 / Full-Tree, Morning, Evening 2026-10-12)

| Prior finding | Verdict |
|---|---|
| Audit-era registry abort (40≠36) / registry desync (2026-10-11 §4; headless docs) | **FIXED-CONFIRMED** — guard deleted by the resolveId refactor; all 56 constants resolve; headless docs describing it are now the stale part (L3) |
| Morning Fix 5: content_selftest hardcoded registry size (53 vs 55) | **FIXED-CONFIRMED** — `content_selftest.cpp` now checks `kSilicaTilledSoil < reg.size()` + `idOf`; passes |
| ChipOR selftest stale registry-size guard (same class, "fixed" per subdivision report via canonical guard) | **REGRESSED** — the canonical-guard form (`== kSilicaTilledSoil+1`) broke again on the very next appends (Structure Block, Certificate Table); ctest red today (M3) |
| Morning/Evening: 4 missing HUD icons (chipor, chipor_viewport, silica_grass, tilled_silicon) | **FIXED-CONFIRMED** — files now exist in pack_32/pack_64 (`chipor.png`, `chipor_viewport.png`, `silica_grass.png`, `silica_tilled.png`/`tilled_silicon.png`); main selftest block-icon and item-icon checks PASS in this run |
| Evening C1: `lithokraft_selftest` SIGSEGV — 4 missed/misordered `setRegistry` sites in signal_selftest | **FIXED-CONFIRMED** — all 4 sites now call `setRegistry` before use (`signal_selftest.cpp:624, 653, 704, 713` verified in place, correctly ordered); suite runs to completion in this run (no crash) |
| Evening: 3 worldgen/mesher failures "cannot re-verify" (masked by C1) | **INCOMPLETE** — unmasked now, but the current red set is save-round-trip ×4 + mesher (M7), not clearly the same 3 cave checks (those organic cave checks PASS in this run); needs an isolated re-run |
| Full-Tree: `give 0 N` silently eats items | **FIXED-CONFIRMED** — `game_commands.cpp:70-74` rejects id 0 / count 0 / count > 65535 (but see L5 for unvalidated nonzero ids) |
| Full-Tree: copy-pasted doxygen debris on `kFusor` | **STILL-OPEN** (L2) |
| Morning #5: tick debugger wired and working | **STILL-OPEN in part** — SimThread side verified then and now; game-side `paused_` desync (M2) was not covered by that verdict |
| Evening L-game2: inventory still on old ID currency (`kFirstItemId = 1<<16`), StableId migration is a builder follow-up | **STILL-OPEN** (by design) — `inventory.h` unchanged in that respect; compounded by H1/H2 |
| Evening L-world4: items have no alias mechanism / rename migration story | **STILL-OPEN** — no `addItemAlias` anywhere (grep-verified) |
| 2026-10-11 LOW: FragmentState zero production callers; thread-safety undocumented | **STILL-OPEN** (M6) |
| 2026-10-10: PortOR/LithographOR/ChipOR/Fusor crash selftest, LithographOR disabled | **INCOMPLETE** — ChipOR fully landed and selftests green (except M3's guard); LithographOR still disabled (L1); PortOR/Fusor are live blocks |
| Spawn floats ~11 blocks (known issue) | **STILL-OPEN, ROOT-CAUSED** — literal `+12` in both dimension setups (M1) |
| Known alternating engine_assets/engine_audio ctest flakes | Game-side analogue: main selftest's audio-streaming check failed in this run while the suite's fixed temp paths (M7) give game selftests the same parallel-collision exposure; scout/chipor failures are deterministic, not flakes (reproduced standalone) |

## Root-cause verdicts (requested)

- **ChipOR check: the CHECK is wrong, the registry is right.** Guard demands size 55 from the silica-era layout; two legitimate append-only blocks (Structure 55, Certificate Table 56) make it 57. Every id-level chipor assertion passes. (M3)
- **Scout check: the TEST is wrong, production is right.** `scout_selftest.cpp:56-58` omits the `ModelResolver` that `dimensions.cpp:43-52` correctly supplies, so wafer.json's `lithoform_stamp` features fail to build. A second identical omission at `scout_selftest.cpp:350` is latent behind the first. (M4)

---
## Appendix H — Entities, Camera, Player, Audio, Misc Engine

# Code Review 2026-10-13 — Entities / AI / Camera / Player / Audio-Occlusion / Leftover Engine

Scope: engine/src/entity, ai, anim, projectile, camera, player, audio, ui, input, platform, index, defrag, subdiv + game camera_controller / player_adapter. Read-only; tests in build-linux only.
Severity counts: CRITICAL 0 · HIGH 2 · MEDIUM 6 · LOW 13 · INFO 2 (23 entries, plus clean spot-checks noted inline).
Tests: 11/11 in-scope ctest suites PASS (see Tests section).

---

## (a) Entities, AI, Projectile

### ENT-1 HIGH — Projectile system is a header with no implementation; the chunk-loading-ahead claim is FALSE as shipped
- VERIFIED. `engine/src/projectile/CMakeLists.txt`: `add_library(litholattice_projectile INTERFACE)` — include-only, no .cpp exists in the directory.
- `engine/src/projectile/include/litholattice/projectile/projectile.h:36-49` declares `ProjectileManager::spawn/despawn/tick/loadChunksAhead` — none is defined anywhere (grep over engine+game returns only the declaration). Any TU that calls one fails to link.
- Header claim (projectile.h:1-3): "Projectiles tick with the world, load chunks ahead of their path" — there is no ticking, no chunk loading, no hit/density logic. Zero production callers (game's ranged weapons are string ids in content/items, e.g. `projectile == "packet"`, unrelated).
- Impact: a locked design pillar (engine/src/sim-era interface, JD locked 2026-10-10) does not exist in code. Fix: implement ProjectileManager (manager + per-tick swept move + `loadChunksAhead` wired to the streaming seam) or downgrade the header to "interface draft, unimplemented" so no one builds on it.

### ENT-2 MEDIUM — Mob/AI ticking independence: rate HOLDS in design, but AiWorld is not integrated into the game at all
- VERIFIED rate independence: `engine/src/ai/include/litholattice/ai/ai_world.h:22` `int hz = 10; // clamped to 8..16`; ctor clamps (`ai_world.cpp:11`). `AiWorld::advance(simTicks, worldTps)` (ai_world.cpp:44-48): `acc_ += simTicks * hz / worldTps; while (acc_>=1) tick();` — mobs think at 8–16 Hz regardless of whether world TPS is 32 or 512. Nothing in ai/ references SignalSim or a 512 Hz cadence. JD's locked rule is honoured *by the AiWorld design*.
- VERIFIED non-integration: grep for `AiWorld` in `game/LithoKraft/src/game/` and `app/main.cpp` = no hits. Only consumers are `game/LithoKraft/src/app/perf_headless.cpp:235` (benchmark) and `engine/tests/engine_tests.cpp`. There are no production mobs: the game's live entities are items/falling blocks only (entities.h, kEntityItem/kEntityFallingBlock).
- Caveat: `advance()` derives AI progress from sim-tick counts, so pausing the sim pauses AI and a caller must supply worldTps honestly; the *rate* is independent, the *drive* is sim-coupled by design.
- Fix direction: when mobs land in Game, drive `advance()` from elapsed world ticks (as vitals/locomotion do) and keep hz clamp; do not call `AiWorld::tick()` per sim tick.

### ENT-3 MEDIUM — Live entities ride the render frame, not world time: they move while the sim/world is paused
- VERIFIED. `game/LithoKraft/src/game/game.cpp:1397` (updateEntities): `entities_.tick(dt);` where `dt` is Game::update's frame dt, called unconditionally at the end of update (game.cpp:1355ff sequence) — unlike locomotion/vitals/sprites, which advance off `sim_->stats().worldTick` and freeze on pause (game.cpp:1206, 1238, 1253).
- Impact: item physics/despawn timers keep running in pause/menu states where every other world system freezes — an inconsistency, and entity motion is frame-rate shaped (variable dt) rather than fixed-step. Independence from the 512 Hz circuit clock itself HOLDS (entities never touch SignalSim cadence).
- Fix: accumulate entity dt from worldTick deltas like the vitals path, with a catch-up cap like locomotion's 64.

### ENT-4 LOW — FallingBlockBehavior has no lifetime: a block that never lands falls forever
- VERIFIED. `game/LithoKraft/src/game/include/lithokraft/game/entities.h:44-61`: FallingBlock applies gravity every tick and only dies on `e.onGround`. Over the Void / unloaded columns (isSolid reads air) it falls without bound, `y` decreasing forever, one leaked entity per block. ItemBehavior by contrast has the 8-minute despawn (entities.h:26-27).
- Fix: mirror the item rule (max fall time / y floor → despawn or place).

### ENT-5 LOW — EntityManager per-tick allocation + sort; behaviour exceptions have no boundary
- VERIFIED. `engine/src/entity/entity.cpp:50-65`: `tick()` builds and sorts a `std::vector<uint64_t>` of all ids every call; `querySphere` (entity.cpp:30-48) does the same. Fine at item scale, wasteful as a mob substrate. A throwing behaviour propagates out of `tick()` mid-pass (dead-cull never runs, `age` half-advanced) — no fail-fast wrapper of the kind SimThread has.
- Fix: keep a persistent sorted id vector invalidated on spawn/cull; wrap behaviour calls or document "behaviours must not throw".

### ENT-6 SUSPECTED LOW — moveCollide can tunnel at large dt
- `engine/src/entity/physics.cpp:23-68`: each axis moves the full `v*dt` and tests only the destination AABB. Terminal fall speed is 24 b/s (entities.h); at a 250 ms hitch dt the Y step is 6 blocks and a 1-block floor between start and destination is never tested. Normal 60 fps dt (≈0.4 blocks) is safe. No dt clamp was found on the Game::updateEntities path.
- Fix: substep moveCollide so no step exceeds ~0.5 blocks, or clamp entity dt.

### AI-1 (regression item) — AI self-despawn: FIXED in code, INCOMPLETE (untested)
- VERIFIED fix: `engine/src/ai/include/litholattice/ai/ai_world.h:111-115` documents deferred despawn; `ai_world.cpp:25-33`: `if (ticking_) { pendingDespawn_.push_back(id); return true; }`; flushed at ai_world.cpp:126-129 after iteration, `ticking_=false` first. A brain despawning itself can no longer invalidate the Mob&/iterator.
- Gap: no test exercises it — grep of engine/tests/engine_tests.cpp and all selftests for a self-despawning brain = no hits. The known open item can be closed in code, but it has no regression pin.
- Minor semantics: a mob despawn-requested early in a tick still thinks/moves later that same tick (deferred), and duplicate requests pile up in pendingDespawn_ (harmless erase).

### AI-2 LOW — AiWorld::tick() path-application holds Mob* across the parallel plan phase
- VERIFIED (safe today, fragile): ai_world.cpp `want` collects `Mob*` into `std::map` (node-stable) and results are applied after the join — safe because despawns are deferred and spawns don't invalidate map nodes. Worth a comment-level guard only; no action required.

---

## (b) Camera

### CAM-1 HIGH — First-person "numerically identical" claim is FALSE in the integrated game: FP renders from the FEET, not the eye
- VERIFIED chain:
  - `game/LithoKraft/src/game/camera_controller.cpp` viewPose(): in FirstPerson it bypasses the manager entirely — `return {player_.pos, player_.forward(), player_.yaw, player_.pitch, kGameFovRad};`
  - `player_.pos` is FEET since the locomotion wiring: player_locomotion.h `position() … ///< feet center`; game.cpp:1335-1337 comment: "player_.pos is the body's feet since the locomotion wiring; the ray used to start at pos back when pos doubled as the eye" — the interaction ray was migrated to `playerEyePos(player_, locomotion_.eyeHeight())`, the camera was not.
  - `Game::frameParams` (game.cpp:1478-1481): `p.camPos = view.position; viewProj = mul(perspective(view.fovRad,…), lookDir(view.position, view.forward))` — no eye offset anywhere in the render path (grep camPos in renderers: used raw).
  - The engine FP camera itself is correct (camera.cpp FirstPerson: `eye = anchor + target->eyeOffset()`, selftest pins anchor+1.625), and the game's PlayerCameraTarget supplies the locomotion eye height via hook — but that path is never what renders: viewPose() bypasses it, and the controller additionally glues the FP engine camera each frame with `playerCam->setPose({player_.pos, …})` (feet, no eye) so even manager/blend sources carry the feet pose.
  - Stale doc: camera_controller.h:6-7 "anchor = the player eye: player_.pos, the same point Game::frameParams has always rendered from" — true before locomotion, false now. Eye heights at stake: Stand 1.62 / Sneak 1.27 / Crawl 0.4 — FP never dips for sneak/crawl either.
- Impact: FP view sits ~1.62 blocks low (at floor level); sprite buildFrame (game.cpp:1325) uses the same pose. Runtime visual confirmation not possible on this VM (no GL), but the code path is unambiguous.
- Fix: FP viewPose should be `playerEyePos(player_, currentEyeHeight)` (the controller already receives the eye-height hook), forward/yaw/pitch/FOV already match the pre-system expressions (perspective 1.2 rad — that part of the claim HOLDS).

### CAM-2 MEDIUM — Camera chunk-loading (limited, hardware-scaled): OVERSTATED — budget value exists, no loading consumer exists
- VERIFIED present: `CameraConfig::chunkRadiusChunks = 4` (camera.h:138), cvar `camera.chunk_radius` (camera_cvars.cpp:42, archived, game clamps 0..64 in settings_cvars.cpp:247), `ICamera::chunkLoadCenter/Radius`, `CameraManager::chunkLoadRadiusChunks()` (camera_manager.cpp:132), selftest pins the value 4.
- VERIFIED absent: no production code calls `chunkLoadCenter()`/`chunkLoadRadiusChunks()` to load anything (grep: only selftest + the manager forwarding). Game streaming is player-only: game.cpp `sim_->setPlayerChunk(player chunk)` + `pumpStreaming()` around the player. The MCP adapter carries `chunkRadius` labelled "streaming budget, builder hook" (mcp_senses_adapter.h:96) and never uses it in mcp_senses_adapter.cpp; pilot captures render loaded chunks + sky beyond (by design).
- Verdict: "limited radius" HOLDS as a config/default; "cameras load chunks in a limited radius, scaled with hardware" does NOT happen in code today, and there is no hardware scaling of the radius anywhere (single cvar int).
- Fix: either wire manager.chunkLoadCenter/Radius into pumpStreaming for non-player cameras (esp. pilot/free), or reword the claim to "budget reserved; loading not yet wired".

### CAM-3 MEDIUM — camera.third_person_distance cvar is dead after camera construction
- VERIFIED. `applyCameraCvars` (camera_cvars.cpp) writes `cfg.orbitDistance`, but `Camera::update` ThirdPerson uses the member `orbitDistance_` (`float allowed = orbitDistance_;`), which is copied from cfg only in the constructor (`orbitDistance_ = cfg_.orbitDistance;`) and afterwards changed only by `addZoomInput`. Same pattern for Follow (`followDistance_`). Syncing cvars per frame (GameCameraController::update applyView) therefore never moves an existing camera's boom. (chunk_radius/shake_scale/headBob/fovKick read cfg live and do work.)
- Fix: in applyCameraCvars path, add a Camera setter, or make update read cfg_.orbitDistance when no zoom has occurred.

### CAM-4 LOW — Engine FP features never reach the game FP view
- Because viewPose() hardcodes `{pos, forward, yaw, pitch, 1.2 rad}` (CAM-1), head-bob, FOV kick, trauma shake and the damped FOV in the engine FP camera have no effect on what the player sees in first person (they do affect pilot cameras and non-FP modes via the manager). The "camera system drives the view" claim should be scoped: FP is a bypass by design (for exactness), non-FP modes are manager-driven.

### CAM-5 — Claims that HOLD (verified)
- 5 modes engine-side: FirstPerson/ThirdPerson/Free/Fixed/Follow (camera.h CameraMode), all implemented in camera.cpp update(); game binds 4 (Fixed unbound in game, documented in camera_controller.h).
- Spring-arm: pull-in instant, ease-out damped (camera.cpp ThirdPerson: `if (allowed < armLength_) armLength_ = allowed; else armLength_ = damp(...,6,dt)`), probe radius honoured; selftest pins 3−0.3=2.7 in one frame.
- Trauma shake: additive clamped [0,1], applied as trauma², rotational-first, on a copy (basePose_ stays clean); selftest pins return-to-base.
- Blending: CameraManager updates every camera each frame (warm damping), blends from the source camera's *live* pose with easing curves; selftest pins midpoint/FOV/completion.
- MCP pilot cameras share the manager: VERIFIED — GameCameraController creates pilot cameras into the same CameraManager (`manager.addCamera("pilot:"+name, slot)`), updates them in manager.update, publishes resolved poses to PilotCameraRig (mutex-guarded) for MCP-thread reads. Tracking pilots are glued via setPose with snapshot + 1.62 eye (consistent); free pilots convert MCP degrees correctly.
- camera.* cvars: 6 registered, archived (chunk_radius, default_fov, third_person_distance, shake_scale, headbob, fov_kick); game view cameras deliberately keep the 1.2 rad FOV (documented).
- Damping is exp-form (frame-rate independent); follow-chase 30 vs 120 fps selftest exists.

---

## (c) Player / IPlayer adapter

### PLY-1 MEDIUM — Vitals rates are calibrated in sim ticks at 32 TPS, so oxygen/drown/stamina speed scales with the player's TPS setting
- VERIFIED. `player_adapter.h:67-71`: kOxygenDrainTicks=8 ("~16 s of air" = 64 pts × 8 ticks = 512 ticks ⇒ true only at 32 TPS), kDrownTicks=16 ("1 health / 0.5 s" ⇒ 32 TPS). `tickVitals(ticks,…)` counts *world ticks* (player_adapter.cpp), driven from `sim_->stats().worldTick` deltas (game.cpp:1238). Settings offer TPS 32..512 (settings.h: tps default 32, kSettingsTpsModes up to 512). At 512 TPS the player drowns 16× faster in wall-clock terms.
- Contrast: locomotion does it right — `dtTick = 1.0 / targetTps` (game.cpp:1218) so movement is TPS-invariant. Vitals have no such normalisation.
- Fix: express drain/refill in seconds (accumulate ticks × 1/targetTps) or pin vitals to a fixed 32 Hz sub-rate accumulator inside tickVitals.

### PLY-2 — Classification claims HOLD (verified, selftest-pinned)
- xp is a STAT: standard_stats.cpp registers "xp" on IStat, 0..INT32_MAX, default 0. Selftest pin exists exactly as claimed — player_selftest.cpp: `check(xpVitals.findVital("xp") == 0, "xp: NOT a vital (JD classification)")`, plus findStat, idempotence, "not clamped to the 64 scale" (set 100000) checks.
- static is a STAT: player_adapter.cpp registers "static" on IStat, 0..INT32_MAX, default 0, accessor staticCharge().
- kharge is a STAT: adapter registers "kharge" 0..64 default 0; vitals.h doc concurs ("Kharge … lives on IStat").
- Standard vitals: health/oxygen/stamina 0..64 default 64 on IVitals (vitals.cpp).

### PLY-3 — Velocity + persistence claims HOLD (verified)
- Velocity first-class: iplayer.h default velocity()/setVelocity() ride on PlayerState vx/vy/vz and setVelocity fires notifyStateChanged(). Serialized: PlayerState wire format includes vx/vy/vz as f64 (player_state.cpp), kSerializedSize=68, selftest round-trips vx=0.125/vy=-0.5/vz=3.75 exactly. Adapter mirrors the locomotion body's TRUE velocity via syncFromGame(...,trueVelocity) with a 512 b/s teleport guard that zeroes one sync on discontinuity.
- PlayerMeta split HOLDS: world_save.h PlayerMeta has separate `stats` and `vitals` maps; kVersion=3 (v2 added stats, v3 vitals); writePlayer emits stats block then vitals block by name; readPlayer accepts v1/v2/v3, CRC-checked; game.cpp saves via statsForSave()/vitalsForSave() and restores by name with unknown names ignored and clamping (player_adapter.cpp applySavedStats/applySavedVitals).
- Oxygen logic: tickVitals drains oxygen under liquid (breathAcc), drains health at 0 oxygen (drownAcc), refills surfaced (refillAcc), stamina drain/regen with mode-flip accumulator reset — all as documented in player_adapter.h.

### PLY-4 LOW — Notification spam + dead field + per-change string lookup
- VERIFIED. Adapter mirrorState() calls setVelocity() every Game::update, so onPlayerStateChanged fires every frame even when nothing changed (doc frames the hook as change notification). PlayerManager::routeVitalChanged does `vitals_.findVital(kHealthVitalName)` (string map lookup) on *every* vital change, while the cached member `uint32_t vitalsHealth_` (player_state.h:107) is declared and never assigned/used.
- Fix: cache the health id in vitalsHealth_ at registration; only notify on actual velocity/state change or document per-frame firing.

### PLY-5 LOW — writePlayer durability is a notch below the house file contract
- world_save.cpp writePlayer: writes .tmp then std::filesystem::rename, but no fsync of file or directory before rename (storage::writeFileAtomic, used elsewhere, does temp→fsync→rename). A power loss can still lose/corrupt player.dat where region saves would survive. CRC on read at least fails closed.

### PLY-6 INFO — tickVitals batching artefacts
- points() caps one call at 64 points but then does `acc %= perPoint`, silently discarding whole points beyond the cap after long hitches (locomotion caps catch-up explicitly instead). Also, a batch that empties oxygen mid-call starts drowning only on the next call. Both negligible at frame cadence; noted for completeness.

---

## (d) Audio / occlusion / spatial scaffolding

### AUD-1 MEDIUM (RT risk, known) — OcclusionThread::gain() still takes a mutex; its header claims "lock-free" — STILL-OPEN, doc now contradicts code
- VERIFIED. occlusion_thread.cpp gain(): `std::lock_guard<std::mutex> lock(queriesMu_);` to copy the query pointer, then an atomic load. Correct (no race) but the audio thread can block behind addQuery/removeQuery and behind threadMain's snapshot lock.
- The header has drifted past the ledger's framing: occlusion_thread.h gain() doc says "Read the current gain for a query. Lock-free, audio-thread safe." and the file header says "the audio thread just reads [atomics]. Never blocks." Both are FALSE for the current implementation.
- Fix direction (unchanged from morning report): publish the queries_ pointer array lock-free (atomic snapshot pointer / double-buffer), or change the doc and accept the mutex explicitly.

### AUD-2 — Spatial audio is UNINTEGRATED: verified, and no game path treats it as production
- VERIFIED. Grep over game/ + engine (excluding audio_selftest and the audio headers themselves): zero production callers of SpatialAudio, OcclusionThread, OcclusionField, OcclusionBakeScheduler. Game sound is SoundDirector (sound_director.cpp): its own linear `distanceGain` (1 − (d−4)/(kHearing−4)), no pan, no occlusion, no reverb.
- Reverb: the EffectChain (real Schroeder reverb in AudioEngine) is bypassed by default (`effectBypassed_{true}`) and the ONLY caller of setEffectBypassed in the tree is audio_selftest.cpp:564 — production never enables it. "Reverb estimated not applied" HOLDS, doubly: SpatialAudio::estimateReverb is never called, and the engine's actual reverb insert is never un-bypassed.
- Echo: PerceivedSound.delay is computed in perceiveImpl (dist/340×1000 ms) as metadata; nothing consumes .delay anywhere. "Echo metadata-only" HOLDS.
- perceiveImpl never sets p.reverb (stays 0; comment "Reverb set by caller via estimateReverb()" — no such caller/API path exists in perceive()).
- Positive note: nothing calls the scaffold *as if* it were production, so there is no false-live path — the risk is only in status docs claiming spatial audio exists.

### AUD-3 LOW — setInline(false) no-op wart: STILL-OPEN (verified)
- occlusion_thread.cpp: `void OcclusionThread::setInline(bool inlineMode) { if (inlineMode) { stop(); rate_.store(Inline); } }` — setInline(false) does nothing: no thread restart, rate stays Inline. Recovery requires an explicit start(). Exactly the morning-report wart.

### AUD-4 — Morning fixes verified in today's code
- start(Inline) fix: occlusion_thread.cpp start(): `if (rate == OcclusionRate::Inline) return;` *before* setting running_/spawning — a later start(Full) works. FIXED-CONFIRMED.
- voxel_raycast same-voxel fix: voxel_raycast.h has both the dist<0.001 early-out and `if (x == ex && y == ey && z == ez) return false;`. FIXED-CONFIRMED.

### AUD-5 LOW — SpatialAudio::audibleSounds sorts by raw, not perceived, volume
- spatial_audio.h audibleSounds (both overloads): filters by perceived volume > 0.01 but sorts `a.volume > b.volume` on the *source* SoundEvent volume. A distant loud event outranks a near quiet one even when its perceived volume is lower — contradicts the "loudest first (mobs prioritize)" intent. Scaffold-only today.

### AUD-6 LOW — Unsigned tick underflow in SpatialAudio::update (and cadence scheduler)
- spatial_audio.cpp update(): `currentTick - e.tick > maxAge` in uint64 — an event stamped with a future tick (world reload resets tick counters elsewhere in this engine) wraps to a huge age and is deleted instantly. OcclusionCadence::maybeBake has the same pattern (`tick - lastTick_`) — there it merely forces a bake, harmless.

### AUD-7 INFO — OcclusionThread cadence arithmetic
- threadMain: `interval = milliseconds(1000 / hz)` → 31 ms for the "32 Hz" tier (≈32.26 Hz). Powers-of-two branding aside, harmless; smoothing lerp 0.5/tick makes gain convergence rate-dependent.

---

## (e) Leftover modules (ui / input / platform / index / defrag / subdiv / anim)

### SUB-1 — Subdiv spot-check: bounds/overflow CLEAN
- octree.h/octree.cpp: kMaxLevel=6, 64³ max; inRange() rejects level∉[0,6] and coords ∉[0,2^level) on set/get; MicroCell coords are uint8 (max 63 at level 6 — fits). Node pool uses int32 indices into a vector; worst-case full tree is bounded by the 64³ leaf space.
- subdiv_store.cpp deserialize(): validates magic/version, `count > (size-10)/7` pre-check, per-leaf `n > (size-off)/5` pre-check, level ≤ 6, coords < 2^level, material ≠ 0, and `off != size` trailing-garbage rejection; builds into a temp store and only commits on success. No overflow path found. (.lthm proper lives in game content/model — outside this scope; the subdiv LTHS stream above is the engine-side format and is sound.)

### DEF-1 LOW — RecordStore::put(id, nullptr, 0) inserts from a null range
- defrag.cpp put(): guard is `(n > 0 && data == nullptr)`; with n==0 and data==nullptr it proceeds to `pg.data.insert(end, data, data + n)` — pointer arithmetic on nullptr, technically UB (benign in practice). One-line guard.

### DEF-2 / IDX-1 — Spot-checks clean
- defrag deserialize(): pageCount ≤ 1e6, slotCount ≤ kPageSize, dataLen ≤ kPageSize, and offset+length validated against page data before any copy — fail-closed throughout.
- index/text_indexer.cpp: UTF-8 decoder handles truncated/bad-continuation without OOB; addDocument/removeDocument keep docCount_ consistent (replace path decrements then increments); query ordering deterministic (score desc, DocId asc).

### UI/INPUT/PLATFORM — Spot-check, no findings at severity
- platform/window.cpp: glfwInit/create failures log and return nullptr, Vulkan loader checked before window creation. input/input_thread.cpp: thread is joined in the destructor (never detached), queue-full drops are counted and rate-limit-logged. UI text_buffer caret/offset math clamps through setCaret; no OOB pattern spotted in the sampled files. (Sampled, not exhaustive — these modules were a spot-check per scope.)

### ANIM-1 — IK NaN regression: FIXED-CONFIRMED, one residual edge
- Full-tree 2026-10-12 #7 (HIGH, iksolve NaN) is fixed: iksolve.cpp:206-209 finiteVec3 guard; solveTwoBone returns bind pose on non-finite rootPos/target/pole (iksolve.cpp:222); solveCcd/solveFabrik return no solution on non-finite target and additionally validate rig joint finiteness. Residual LOW/SUSPECTED: solveTwoBone does not validate rig-internal bone head/tail finiteness (CCD/FABRIK do via chainJoints + joint check) — a NaN rig (not NaN input) can still poison a two-bone solve.

---

## Regression verdicts (prior reports)

| Prior finding | Verdict |
|---|---|
| Morning: OcclusionThread::start(Inline) leaves occlusion dead | FIXED-CONFIRMED (occlusion_thread.cpp early return) |
| Morning: voxelRaycastBlocked same-voxel false "blocked" | FIXED-CONFIRMED (same-voxel early return) |
| Morning minor: OcclusionThread::gain() audio-thread mutex | STILL-OPEN (AUD-1; header now over-claims "lock-free") |
| Morning minor: setInline(false) no-op | STILL-OPEN (AUD-3) |
| Full-tree #7 HIGH: IK solvers NaN-poison skeleton | FIXED-CONFIRMED (ANIM-1, one rig-NaN residual) |
| Full-tree: AI tick deadlock on throwing path job (ai_world --left) | FIXED-CONFIRMED — ai_world.cpp now wraps the job in a DoneGuard whose destructor decrements `left` and notifies even on throw |
| Known open item: AI self-despawn | FIXED in code (deferred pendingDespawn_), INCOMPLETE verification — no test (AI-1) |
| 2026-10-11: EntityManager iterates in id order | Still holds (entity.cpp sorts ids in tick and querySphere) |
| Evening: engine_audio full-run failure / solo pass (load flake) | See test section below |

## Claim-verification summary

| Claim | Verdict |
|---|---|
| Mobs must NOT ride the 512 Hz circuit clock; entity updates on their own domain/rate | HOLDS for AiWorld design (8–16 Hz clamp, TPS-ratio advance) and for EntityManager (game-thread frame dt); OVERSTATED as a production statement — AiWorld has no game integration, live entities are items only, and they run on render-frame dt incl. during pause (ENT-2/ENT-3) |
| Projectiles load chunks ahead of path | FALSE — no implementation exists (ENT-1) |
| Camera: 5 modes / spring-arm / trauma shake / blending / camera.* cvars | HOLDS engine-side (game binds 4 modes; Fixed unbound) |
| First person is numerically identical to pre-system behaviour | FALSE in integrated game — FOV/forward/yaw/pitch expressions match (1.2 rad), but position is feet, not eye (CAM-1) |
| MCP pilots are engine cameras in the same manager | HOLDS |
| Camera chunk loading: limited radius, hardware-scaled | OVERSTATED — radius value/cvar exist (default 4, clamp 0..64); no loader consumes it; no hardware scaling (CAM-2) |
| xp and static are STATS not vitals; selftest pin findVital("xp")==0 | HOLDS, pin present verbatim |
| Velocity first-class: blocks/sec, serialized, change-notified | HOLDS (with per-frame notify spam, PLY-4) |
| kharge + oxygen drain/drown/refill | HOLDS in logic; rates are TPS-coupled (PLY-1) |
| PlayerMeta persists vitals and stats in separate blocks | HOLDS (v3, v1/v2 still load) |
| OcclusionThread start(Inline) fix landed | HOLDS / FIXED-CONFIRMED |
| gain() RT mutex concern still open; setInline(false) wart | STILL-OPEN both (AUD-1/AUD-3) |
| Spatial audio unintegrated; echo metadata-only; reverb estimated not applied | HOLDS — verified zero production callers; engine reverb insert also never un-bypassed in production (AUD-2) |

## Tests

Ran in build-linux only: `ctest -R "camera|player|audio|entity|ai|anim|subdiv|defrag|index|ui|input|locomotion"` — **11/11 PASS**: engine_player (incl. the findVital("xp")==0 pin), engine_locomotion, engine_camera, engine_ai, engine_anim, engine_ui, engine_audio (14.9 s, passes solo — consistent with the evening report's load-flake verdict; no full-suite run was done here), engine_input, engine_index, engine_defrag, engine_subdiv.
Notes: there is **no engine_entity ctest** — EntityManager has no dedicated suite (locomotion covers the shared physics only), and no game-level camera_controller test exists, which is exactly how CAM-1 (feet-vs-eye) survives: every camera test operates at the engine layer, where FP is correct.
