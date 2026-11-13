# Debug Console + Suspension Recovery + Quota Blind-Spot Fix — Implementation Report (2026-10-11)

Three related robustness items, approved by JD, implemented sequentially by three workers on the shared tree at `~/workspace/litholattice/engine`. All build green.

Companion detail (console UI + language internals) lives in the worker-2 report: [Console-Implementation-Report-2026-10-11.md](sandbox://workspace/user/files/Console-Implementation-Report-2026-10-11.md).

## Status summary

| # | Item | Status |
|---|------|--------|
| 1 | std::string memory-quota blind spot | DONE — closed, 11 new checks |
| 2 | Auto suspension recovery + console logging | DONE — 46 new checks |
| 3 | Robust dropdown debug console | DONE — 18 new checks |
| 3b | HL1-style console scripting language | DONE — same module, three exposures |

---

## Item 1 — std::string memory-quota blind spot: CLOSED

**Problem.** Per-mod memory quotas were enforced through AngelScript's global memory-function shims, but script `std::string`/container heap buffers go through global `operator new` and escaped the quota entirely — an unbounded allocation channel.

**What was built** (`src/script/script_memory_hook.cpp`, `script_memory_internal.h`; `script_engine.cpp/.h` updated):
- All 20 replaceable global `operator new`/`delete` overloads (new/new[], `align_val_t`, nothrow, sized/unsized delete) now carry the same thread-local attribution scheme: a 24-byte header `{block, size, owner}`; when `tlsScriptAllocOwner` is set (inside mod `Execute()`), bytes charge to that mod and the quota enforces; when unset, behavior is exactly the default operators.
- No recursion: headers use malloc/free only, charge/credit are pure integer arithmetic, and `quotaEnforce()` (which may log/allocate) runs under a reentrancy guard.
- `new_handler`, overflow checks, `bad_alloc`/`nullptr` contracts preserved. No double-counting: the AS shims use malloc/free directly and never pass through the hook (verified).
- Also fixed a real wart testing exposed: `allocShim`'s suspend-message construction allocated through the hooked `operator new` with no guard → nested enforce → double suspend log. Now guarded (single suspend, one log).

**Honest limits** (documented in the header): SSO strings never heap-allocate (nothing to count — correct); threads with no attributed mod are unattributed by design; third-party libs with custom allocators bypass the hook; the first `Execute()` in a process leaves a one-time ~56 B AngelScript lazy allocation attributed to the running mod (stable, freed at shutdown — this validated the header-credit design).

**Tests:** 11 new checks green — script building 200 large strings trips a 4 KiB quota → suspended with "memory" reason (previously escaped); engine allocations outside mod execution unattributed and byte-identical; 400-iteration alloc/free fuzz balances exactly to baseline. `engine_script`: 24/24 pass, stable across 100+ runs including 30 under full CPU load.

**Incidental fix:** load testing exposed a pre-existing flake in `testScriptSandbox` (default 1024 µs wall-clock CPU budget throttled even trivial hooks ~10% of the time under load). Test-only fix: ample budget for that test. 0/30 failures under load after.

---

## Item 2 — Auto suspension recovery: DONE

**Policy** (`src/script/suspension_policy.h/.cpp`, one per mod, owned by `ScriptEngine`). All numbers powers of two, per JD's convention:
- **Cooldown with exponential backoff:** 64 → 128 → 256 → 512 → 1024 → 2048 → **4096 cap** per consecutive suspension #1–7.
- **Probation (64 ticks)** after auto-recovery — both levers: effective CPU budget **halved** (min 1 µs) *and* zero tolerance (any overrun suspends immediately, bypassing the throttle ladder). Rationale: a fixed mod passes trivially under half budget; a still-broken one trips the breaker faster, and each probation suspension still counts toward escalation. Memory quota unchanged (standing total; memory overruns already suspend immediately).
- **Decay:** every 256 consecutive clean ticks → consecutive count −1 (floor 0). A reformed mod earns its way back.
- **Escalation at 8 consecutive:** no cooldown, mod stays suspended, logged at Error — the circuit breaker against riding auto-recovery forever. Manual `resume()` (admin) clears all policy state.
- Existing throttle/suspend/resume behavior untouched.

**Logging** — all through the general engine-log tap (worker 2's contract), `modsys:` prefix, visible in the debug console: suspension at Warn (reason + numbers + "auto-recovery in N ticks (consecutive suspension #K of 8)"), recovery attempt/completion at Info, decay at Info, escalation at Error, manual resume at Info. New `ScriptEngine::setModId()` (renders `<unknown>` until the ModLoader wires it — builder follow-up).

**Tests:** 46 new checks, all PASS — backoff schedule + cap, pow2 constants, cooldown→auto-recovery→probation, halved budget (+1 µs clamp), probation immediate-suspend, 256-clean-tick decay, full 8-cycle escalation (stays suspended through 9000 ticks; resume clears everything; next suspension restarts at 64), every transition found in the tap at the right level.

---

## Item 3 — Robust dropdown debug console: DONE

**What was built** (new module `engine/src/console/` → `litholattice_console`, ctest module `engine_console`):
- `DebugConsole`: tilde-key dropdown composed from existing UI widgets (StackPanel + read-only log view + input box). Available all the time; debug builds get more verbose commands.
- Tilde opens only when `!UiRoot::wantsKeyboard()` — and the check runs *before* the key-repeat debounce, so `~` typed in a text field always types (a real bug the tests caught: the debounce used to run first and would have eaten it; reordered).
- While open, its input box holds focus so keystrokes never reach mods. History (Up/Down), scroll, Escape to close, per-frame tap streaming.
- Engine-log ring tap in `core/log.h`/`log.cpp` (last-N records with level + monotonic seq) — the general record path worker 3 logs through. `logWarn` added.
- Minimal game hook: `Game` owns registry + console, tilde routed in `routeUiInput`, per-frame `update()`, exec sandboxed to the settings dir.

**Permission gating:** new `kPermConsole = 1 << 9`, added to the canonical name table (`"console"`). Deny-by-default unchanged.

**Tests:** `engine_console` **18/18 PASS** — tap, toggle, tilde-vs-text-field confinement, history, scroll, Escape.

---

## Item 3b — HL1-style console scripting language: DONE

**The language** (`concmd.h/.cpp`): the Quake/HL1 model — console COMMANDS, typed CVARS (string/int/float/bool, get/set, archived to config), `alias` (named sequences, recursion cap 16 — infinite-alias protection), `exec` (runs `.cfg` files, sandboxed to the config dir, refuses outside paths), `;`-separated sequences, quoted/escaped tokenizer, `//` comments.

**Three exposures** (JD's spec):
1. **Engine** — clean C++ registration API for commands and cvars.
2. **User** — typed in the console; player-writable `.cfg` files.
3. **AngelScript** — `consoleCmd` / `consoleCvar` / `consoleSetCvar`, allowlist-registered, permission-gated (`kPermConsole`; denied without it — tested).

**Decisions:** `bind` (key binds) = registry seam only (key names validated via `input::keyFromName`); dispatch into the input pump flagged as follow-up, not half-wired. **Fragment/typewriter future seam** (design only, per JD): single `executeLine()` entry point + swappable output sink; suggested `IConsoleDriver` follow-up noted in the header — no item code built.

**Tests** (in the 18): alias expansion incl. recursion cap, cvar coercion + archived round-trip, exec sandbox refusals, AngelScript allowlisted + denied-without-permission, `;` ordering, tokenizer.

**Known wart:** holding tilde cycles open/close every ~200ms (Text events carry no repeat flag); clean fix is mapping GraveAccent KeyDown through the bridge — flagged for the builder.

---

## Verification

- Full engine + game build: clean, no new warnings.
- New checks: 11 (quota) + 46 (recovery) + 18 (console/language) — all green.
- `ctest`: **13/16**. The 3 failures are all attributed, none from this program:
  - `engine_render` (mesher golden hash) — the PBR worker's in-flight mesher edits; needs their deliberate golden re-record (in their scope).
  - `engine_sim` (block-names narrowing) — grandfathered, builder's fix.
  - `lithokraft_selftest` (SIGABRT) — grandfathered, builder's fix (blocks.cpp 40-vs-36 guard).
- `trace_jit_basic.cpp` untouched throughout. New files LF; edited files kept existing line endings.

## Open questions / builder follow-ups

1. ModLoader → `ScriptEngine::setModId()` wiring (logs show `<unknown>` until then).
2. Probation halves CPU budget only, not the memory quota — deliberate (memory is a standing total); confirm intended.
3. Decay clock starts at re-admission (cooldown accrues no clean-tick credit); confirm.
4. `bind` dispatch (input pump → `executeLine`) — small follow-up.
5. ModLoader wiring for `kPermConsole` (gate + `"modid:..."` namespacing).
6. Tilde-hold cycling fix (GraveAccent KeyDown mapping).
7. `engine_render` golden re-record belongs to the PBR worker.

---

# lithoscript hardening + rename (2026-10-11, worker follow-up)

> **Spelling correction (JD, later 2026-10-11): the language name is officially
> lowercase `lithoscript`, not "LithoScript". All user-facing references below
> should be read as lowercase; the rename was applied tree-wide.**

JD's five requirements, all implemented in the console module (`engine/src/console/`), full build clean, no new ctest failures.

## 1. NAME: lithoscript (lowercase, JD-locked)

User-facing references renamed; code identifiers kept (churn risk). Renamed:

- `concmd.h` module banner: "lithoscript: the console scripting language (Quake / Half-Life 1 lineage)".
- `exec` help text: "exec \<name.lth\>: runs a lithoscript script from the config dir (sandboxed; .cfg still works as a legacy alias)"; usage line "usage: exec \<name.lth\>".
- `version` command prints "lithoscript console (SDK 1)" (was "LithoLattice console (SDK 1)").
- Archived-cvar file header: "// lithoscript archived cvars (written by the console; edit freely)".
- Debug console input placeholder: "type a lithoscript command (? for help)".
- Comments in `debug_console.h`, `script_console.h`, `console_selftest.h`, `CMakeLists.txt`, and the game-side sandbox comment in `LithoKraft/src/game/game.cpp`.
- Self-test banner line.

Kept as-is (documented, not renamed): `ConsoleRegistry`, `ConsoleArgs`, `ConsoleCommandFn`, `kMaxAliasDepth`, `registerCVar`/`setCVar`, command names (`help`, `echo`, `alias`, `exec`, …), `ScriptBindingAllowlist` decls (`consoleCmd`/`consoleCvar`/`consoleSetCvar`), the `kPermConsole` bit and `"console"` permission name. Rationale: these are API identifiers other modules and the builder already depend on; the user-visible name is now lithoscript (lowercase, JD-locked) everywhere a player looks.

## 2. HARDENING

- **Alias recursion cap 16 kept** (`kMaxAliasDepth`, unchanged).
- **Exec-chain depth cap 8 ADDED** (`ConsoleRegistry::kMaxExecDepth`, new `execDepth_` member): `executeFile` refuses, fail-closed with a printed reason ("exec: refused '…': script nesting too deep (limit 8; possible cycle)"), when entered at/past depth 8. Mirrors the IExtensible depth-cap motif. a.lth → b.lth → a.lth terminates. Counter unwinds on return (verified by a post-cycle exec succeeding).
- **`wait` OMITTED in v1** (JD's recommendation at the time): a tree-wide search confirmed no `wait`/`sleep`/`delay` console command existed anywhere; nothing was removed because nothing existed. A self-test guarded against reintroduction.
  > **SUPERSEDED 2026-10-11 ~8:07am: JD reversed the omission — `wait` is now
  > implemented, hardened (tick-based, cooperative, budgeted). The omission
  > guard test was replaced with the new-behavior tests. See the `wait`
  > addendum below.**
- **Trust model documented** (in `concmd.h`): a .lth file carries no authority of its own — it runs with the invoking context's permissions (player-typed `exec` runs as the player; `consoleCmd` through a mod's binding runs with that mod's granted permissions, binding requires `kPermConsole`). No command grants permissions, so a script can never widen its own authority. Existing gating unchanged: allowlisted AngelScript bindings + `kPermConsole` for mods.

New self-tests (all PASS): alias self-reference terminates (kept from before); **mutual alias recursion a→b→a terminates** at the cap; **exec cycle across two files (a.lth↔b.lth) terminates** with the nesting error; **exec nesting deeper than 8 is rejected** (10-deep chain never reaches the bottom file); wait/delay/sleep absence guard.

## 3. FILE EXTENSION: .lth

`executeFile` accepts `.lth` as the script extension; `.cfg` still executes as a legacy alias (documented in the help text and the header). Non-script extensions (e.g. `.txt`) are refused. Docs/help text updated (see §1). File dialogs: checked — no file-dialog or file-filter code in the tree references script extensions, so nothing else needed wiring. Archived-cvar save/load format is unchanged plain text (read through `executeLine`, so it is valid lithoscript either way).

## 4. Debug-console hookup verified end-to-end

New self-test drives the real console input path — `DebugConsole::submitForTest` (input box → `onSubmit` → `executeLine`), not the registry API directly — and executes a command (`echo`), sets/reads a cvar, and defines+expands an alias. All four lines appear in history, all outputs land in the log view. **Result: PASS** — cvars, commands, and aliases are fully reachable from the debug console.

## 5. FRAGMENT SEAM — design note only (extended for ReadOR)

Extended the existing Fragment/typewriter seam note in `concmd.h` (no code built). Shape, per JD's design note at `~/workspace/user/files/ReadOR-Design-Note-2026-10-11.md` (referenced, not duplicated):

- **ReadOR** (new -OR component): player slots their Fragment in; the ReadOR reads the Fragment's lithoscript scripts and sends them to a **keyboard** component, which feeds them into contraptions built by the user, the proxy, or the agent. This is the physical mechanism for the Fragment → lithoscript → world pipeline.
- **Serialization line** (JD's extension): the ReadOR also serializes the Fragment's scripts *and data* directly out into the world — written into player-built memory chips (DRAM, SDRAM, hard drives). The Fragment is both script cartridge AND data exporter. Same pipeline, extended to data.
- Engine-side needs for all of this: already present — the single `executeLine()` entry point, the swappable output sink, the cvar API. The ReadOR/keyboard drive the same seams as the debug console, on their own registry instance (or the shared one), with no new console code.
- Open design questions left for the builder (in the header note): (1) the keyboard as a script-input component — how it accepts lithoscript and routes it into machine logic; (2) permission/trust model for agent- and proxy-authored scripts — three authors (user, proxy, agent), agent scripts execute with the AGENT's permissions never the player's; (3) multiplayer authority — whose context a Fragment-read script runs under (Fragment owner's? ReadOR placer's? reader's?) so a slotted Fragment can't become a privilege-laundering channel. The serialization format for Fragment pages (script vs data vs unlock log) and the memory-chip write interface are also builder design work.

## Verification

- Full engine + game build: clean, no new warnings (two pre-existing warnings elsewhere in the tree unchanged).
- Console checks: **22/22 PASS** (18 existing + 5 new: mutual-alias recursion, exec-cycle termination, depth>8 rejection, wait-omission guard, debug-console end-to-end; the legacy-.cfg check folded into the existing exec test).
- `ctest`: **15/17** — the only failures are the 2 grandfathered builder fixes (`engine_sim` block-names narrowing, `lithokraft_selftest` SIGABRT on the blocks.cpp 40-vs-36 guard). No new failures.
- `trace_jit_basic.cpp` and the blocks.cpp 40-vs-36 guard untouched. New test code only; edited files kept LF line endings.


---

# lithoscript: wait + HL1 completion + first-party population + query + indexer (2026-10-11, worker follow-up)

JD reversed the `wait` omission and then stacked five more work items. All are
implemented in-tree, full build clean, no new ctest failures. The language name
is officially lowercase **lithoscript** (JD correction, applied tree-wide to
docs/help/messages; code identifiers untouched).

## Locked decisions referenced (JD, 2026-10-11)

- **wait**: max **512 ticks** = 1 second of game time at 512 TPS; named constant
  `kMaxWaitTicks` (scales upward with hardware, per scalable-fidelity
  philosophy); clamp-with-console-note, never error.
- **Decision 8**: cvars carry **read-only, replicated, cheat** flags
  (per-cvar, not HL1's sv_cheats sledgehammer). Extended to commands:
  `CommandFlags{cheat}` gates `give`/`kill`.
- **Naming synthesis**: the dotted subsystem hierarchy is the LOCKED convention:
  `sim.`, `client.`, `net.`, `render.`, `audio.`, `input.`, `hud.`, `phys.`,
  `game.`, `debug.`, `mod.<id>.` Provisional names are converted; no longer
  provisional.
- **Decision 9**: first-party game commands `give`, `map`, `save`, `kill`
  (+ `pause`, `quit` found in-tree), cheat-flagged where appropriate.
- **Language/population split** (locked architecture): the console module
  implements the COMPLETE general language; engine/game registrations are a
  separate provisional first-party population, inventoried below for JD's
  review. Naming conventions were NOT churned during the provisional phase.
- **Semi-trust** for Fragment-authored scripts (locked): collaborative content,
  not hostile input. Hardening stays fully armed; every intervention prints to
  the console so the player AND the agent can see it.

## 1. `wait` — implemented, hardened (reverses the v1 omission)

`wait N` suspends the rest of the current script for N **world ticks** and
resumes cooperatively. Tick-based and deterministic — never wall-clock: the
game pumps `consoleRegistry_.tick()` once per sim world tick
(`Game::update`, follows `sim_->stats().worldTick`, resyncs on world reload,
frozen while paused). `wait 512` is exactly one second of game time at
512 TPS, on any hardware, at any frame rate. Purpose: scripts driving
user-built constructs via the ReadOR/keyboard pipeline get timing options
(wait one tick, two ticks, ...).

Caps (all named constants in `concmd.h`):

| Constant | Value | Meaning |
|---|---|---|
| `kMaxWaitTicks` | 512 (JD-locked) | max single wait; above clamps **with a console note**, not an error |
| `kMaxTotalWaitTicks` | 4096 | total suspended ticks per script invocation; exceeded = terminated |
| `kMaxScriptSteps` | 16384 | total commands per invocation; exceeded = terminated |
| `kMaxSuspendedScripts` | 64 | concurrent suspended scripts per registry |
| `kMaxAliasDepth` | 16 (kept) | alias recursion cap, preserved across suspensions |
| `kMaxExecDepth` | 8 (kept) | exec nesting cap |

Semantics: `wait 0` = documented no-op; bare `wait` = 1 tick;
negative/non-numeric = usage error. `wait` suspends — it never busy-loops and
never blocks the engine thread; other commands and the sim run normally during
a wait. The step budget is 4x the wait budget so the *wait* budget (not the
step budget) is what kills wait-ping-pongs. Alias and exec depth counters are
preserved across suspensions, so `alias a = "wait 1; a"` terminates at the
alias cap and `a.lth <-> b.lth` exec cycles with waits terminate at the
total-wait budget. Fail-closed everywhere: budget exceeded = script terminated
with a printed message, engine unaffected.

Why it can't hang: (1) suspension is cooperative — `wait` returns control to
the engine immediately; (2) every wait is bounded by 512 ticks and every
script by 4096 total wait ticks and 16384 steps — all finite, all enforced;
(3) alias/exec cycles can't reset budgets by suspending (counters live in the
`SuspendedScript` and are restored on resume); (4) there is no wall-clock or
blocking call anywhere in the path.

New self-tests (10, all PASS): wait 1 / wait N tick accuracy; wait 0 no-op,
bare wait = 1, bad args; clamp above 512 with note (`wait 999999999` cannot
hang); engine ticks continue during long waits; wait-0 self-loop terminates at
the alias cap; wait/alias ping-pong terminates (alias cap preserved across
suspension); exec cycle with waits terminates at the total-wait budget;
step budget terminates runaway invocations; budget-exceeded messages logged.

## 2. HL1/Quake language completion (console module — the complete language)

Audit against the comparison report's HL1/Quake list; every gap filled:

- `toggle <cvar> [values...]` — HL1 semantics: flips bool / int 0/1, or cycles
  through the given value list with wrap; errors on unknown cvar, string cvars
  with values, empty lists.
- `incrementvar <cvar> <min> <max> <delta>` — HL1 semantics: adds delta,
  clamps at min/max; int, float, bool (as 0/1); errors on bad args.
- `++` / `--` shorthand — `render.pbr_tier++`, `sim.tickrate--`; int and float
  cvars; errors on string/unknown/extra args. (JD explicitly wanted these.)
- `find <text>` — case-insensitive search across commands, cvars, aliases.
- `unbindall`, `aliasNames()`, `clearKeyBinds()` (registry API).
- `CVarChangeFn` onChange hook — fired from `setCVar` after every successful
  set (covers `name value`, `toggle`, `incrementvar`, `++`/`--`, archived
  loads); re-entrancy documented (the SettingsCvars bridge guards with
  `syncing_`).
- `dispatchKeyBinds(devices, textCapture)` — edge-triggered keybind dispatch
  via `DeviceState::keyPressed`; suppressed when the console (or any text
  field) has keyboard focus, same rule as tilde. Key names validated against
  the input system's naming at bind time. Aliases and `;` sequences work in
  bound commands; `wait` inside a bound command suspends cooperatively under
  the same budgets (proven by test).

**bind/unbind dispatch** (was seam-only, now real, HL1-style): `bind <key>
<command>`, `unbind <key>`, `unbindall`, `bind` lists. The console OWNS the
binding table; the input pump dispatches — `Game::update` calls
`dispatchKeyBinds(input_.devices(), ui_.root().wantsKeyboard())` after UI
routing, in the Playing state.

New self-tests (all PASS): toggle flip/cycle/wrap/errors; incrementvar
add/clamp-at-min/clamp-at-max/float/errors; ++/-- on int+float, errors on
string/unknown/args; bind→keypress→execution incl. alias + `;` + `wait` in the
bound command; unbind; unbindall; text-capture suppression; key-name
validation; onChange fires via direct set and via toggle.

## 3. First-party population (game-side registrations — for JD's review)

The language is complete and general; these are the provisional first-party
registrations, each in ONE table so renames stay mechanical.

**Settings cvars** (`game/.../settings_cvars.{h,cpp}`, new `SettingsCvars`
class): all 10 `GameSettings` fields as live cvars — get/set mutates the
ACTUAL engine state, not a copy. Console sets flow through
`Game::onSettingsChanged` (identical to a settings-file load, incl.
`saveSettings()`); UI edits flow back via `syncFrom()` with a `syncing_`
re-entrancy guard. All 10 archived → persist to `config.lth`
(`saveArchived` in `~Game()`). Names use the LOCKED dotted scheme:

| cvar | setting | notes |
|---|---|---|
| `audio.volume` | masterVolume | live |
| `audio.muted` | muted | live |
| `hud.scale` | hudScale | live |
| `client.display_mode` | displayMode | via displayApplier_ |
| `client.vsync` | vsync | restart (as with settings file) |
| `sim.tickrate` | tps | live via setTickRate |
| `sim.tickradius` | tickRadius | restart (as with settings file) |
| `sim.euv` | euv | live via toggleDomain |
| `render.mesher` | mesher | boot-only (noted, as before) |
| `render.pbr_tier` | pbrTier | live via setPbrTier |

**Game commands** (`game/.../game_commands.{h,cpp}`, Decision 9):
`give <itemId> [count]` (cheat — `Inventory::addItem`, reports leftovers),
`kill` (cheat — respawn at the dimension spawn; provisional until the
health/damage system lands), `map [dimension]` (switch via
`switchDimension`, no-arg lists via `registry_.all()`; not a cheat),
`save` (not a cheat), `pause` (toggle; not a cheat), `quit` (not a cheat).
Cheat-gating is enforced in the console module (`CommandFlags.cheat` +
`setCheatsAllowed`; the game sets `setCheatsAllowed(true)` for the local
single-player context).

**Game wiring** (`game.cpp`/`game.h`): `settingsCvars_->registerOn(...)`,
`registerGameCommands(...)`, `consoleRegistry_.setWorldQuery(...)` in the
ctor; `dispatchKeyBinds` in `update()`; `autoexec.lth` exec at startup when
present (silent when absent); `saveArchived(config.lth)` in `~Game()`;
`syncFrom(settings_)` at the end of `onSettingsChanged`.

**Trust note**: `give`/`kill` are cheat-flagged; the local single-player
registry allows cheats. Mods/AngelScript get no console access without
`kPermConsole` (unchanged, deny-by-default).

## 4. `query` — "SQL for voxels" (world queries)

The primitive the future agent/proxy builders use to SEE the world.
`engine/src/console/include/litholattice/console/iworldquery.h` is the seam:
the console module owns the `query` command, its syntax validation, and its
caps; the game registers the backend (`GameWorldQuery`,
`game/.../world_query.{h,cpp}`).

Provisional syntax (flaggable for JD):

- `query blocks <blockId> <x1> <y1> <z1> <x2> <y2> <z2>`
- `query count <blockId> <x1> <y1> <z1> <x2> <y2> <z2>`
- `query entities <type|*> <cx> <cy> <cz> <radius>`

Hard requirements, all enforced:

- **Bounds mandatory, fail-closed**: all six coords (or center+radius)
  required; missing = usage error. Strict parsing — non-numeric is an error,
  never a silent 0. No unbounded scans ("no scanning the infinite Void").
- **Capped**: `kMaxQueryCells = 4,194,304` (2^22, e.g. 256x128x128) per region;
  `kMaxQueryResults = 1024` hits per query; results report `totalMatches` +
  `truncated`.
- **Deterministic ordering**: blocks x-outermost, then y, then z; entities by
  ascending id (the backend sorts — `EntityManager::querySphere` order is
  unspecified).
- **Read-only**: the interface cannot mutate the world. Semi-trusted scripts
  may query.

Threading design (the critical point): while the sim runs, the sim thread OWNS
the chunks — `SimThread::getBlock`'s own header says "fine for a handful of
queries per frame, **not for bulk scans**", and the sim ticks without holding
`blockMutex_`. So the backend uses **pause-scan-restore**: `setPaused(true)`
→ `waitLoops(1)` settles the in-flight tick (5 s timeout = fail-closed, never
hangs) → scan `ChunkMap::get` directly (const, air for unloaded cells) → 
restore the *previous* pause state (a query issued while paused leaves the
game paused). A max-size scan is a few ms of array reads — no visible hitch,
no torn reads, no lock games. Entities live on the game thread
(`EntityManager::querySphere`), no pause needed.

Self-tests: 4 console-level (all PASS) — no-backend message; bounds
mandatory/strict numbers/region cap/subcommand validation; blocks/count/
entities round-trip through a fake backend incl. error surfacing; plus the
real `GameWorldQuery` verified standalone against a real `ChunkMap` and
`EntityManager` — x-y-z ordering, count, entity id sort, wildcard, unknown
type fails closed, no-world fails closed, no pause attempted without a sim.

## 5. Engine text indexer (new core subsystem)

`engine/src/index/` — `litholattice::index::TextIndexer`, built by a worker
to the specified API (inverted index, general: any text corpus, not just the
Fragment). Fast lookup, incremental updates (page edited → reindex that page).

API (exact):

```cpp
class TextIndexer {
 public:
  using DocId = uint64_t;
  struct Hit { DocId doc = 0; double score = 0.0; };  // score desc, doc asc on ties
  void addDocument(DocId id, std::string_view text);  // insert or replace
  void removeDocument(DocId id);                       // missing id = no-op
  std::vector<Hit> query(std::string_view q, size_t maxResults = 64) const;  // deterministic
  size_t documentCount() const;
  size_t termCount() const;  // diagnostics
  void clear();
};
```

UTF-8-safe tokenization; self-tests: index/update/remove, query latency
sanity, unicode-safe (all PASS per the worker's digest).

**Fragment search seam** (design only — the Fragment UI doesn't exist yet):
the future search button+box calls `query(boxText, 32)`; the UI maps returned
DocIds to pages. DocId assignment is the UI's business (e.g. a page index);
the indexer never interprets ids. A straight build later.

## 6. Semi-trust, precisely (JD-locked 2026-10-11)

Fragment-authored scripts (written by the player, the proxy, or an agent;
driven through the ReadOR/keyboard pipeline) are **SEMI-TRUSTED**:
collaborative content, not hostile input. What semi-trust GRANTS: the script
may drive the console and the input pipeline with the invoker's context
permissions (cheats only where the context allows). What it does NOT grant:
the hardening layer stays FULLY ARMED — step budget, total-wait budget, wait
clamp, alias/exec caps, cooperative wait, fail-closed termination, all
unchanged. A semi-trusted script runs with the invoker's permissions and can
never widen them (no command grants permissions). Every hardening intervention
prints to the console so the player AND the agent can see what happened —
exact strings (stable for agent parsing), documented in `concmd.h`:

- `lithoscript: step budget exceeded (16384 commands); script terminated`
- `lithoscript: total wait budget exceeded (4096 ticks); script terminated`
- `lithoscript: too many suspended scripts (64); script terminated`
- `wait: clamped to 512 ticks (1 second of game time at 512 TPS)`
- `alias recursion too deep (expanding '<name>')`
- `exec: refused '<file>': script nesting too deep (limit 8; possible cycle)`
- `command '<name>' requires cheats (not allowed in this context)`
- `cvar '<name>' is read-only` / `cvar '<name>' requires cheats (not allowed in this context)`

## 7. Sequenced next (not built here)

- **Defrag/restructure service**: queued AFTER the indexer (it defines the API
  defrag calls) — general engine service, Fragment first client, defrag
  triggers reindex, `defrag` lithoscript command, chunked across ticks,
  crash-safe per the file-hardening contract.
- **MCP in engine**: research landed (JSON-RPC 2.0, Streamable HTTP + stdio,
  clean-room C++); implementation queued after console work (FIFO).

## Verification

- Full engine + game build: clean (new files: `settings_cvars.{h,cpp}`,
  `game_commands.{h,cpp}`, `world_query.{h,cpp}`, `iworldquery.h`,
  `engine/src/index/*`; edited: `concmd.{h,cpp}`, `console_selftest.cpp`,
  `game.{h,cpp}`, game `CMakeLists.txt`).
- Console checks: **all PASS** (10 wait + 4 query/cheat + toggle/incrementvar/
  ++/--/bind/onChange suites; the earlier onChange test bug — a missing
  `registerBuiltins` in the test block, not the feature — was found via a
  standalone repro and fixed).
- `ctest`: **16/18** — only the 2 grandfathered builder failures
  (`engine_sim` block-names narrowing, `lithokraft_selftest` SIGABRT); the new
  `engine_index` test (text indexer) passes. No new failures.
- `trace_jit_basic.cpp` untouched. New files LF; edited files kept LF.

# Engine defrag/restructure service (2026-10-11, final authorized item)

JD's forward-thinking feature for forever-worlds: a first-class engine service
that compacts, reorders, and reclaims dead space for large persistent stores.
The Fragment is the first client; the service is general.

## 1. RecordStore — the layout assumption (builder note)

`engine/src/defrag/` — `litholattice::defrag::RecordStore`. Slotted 4 KiB
pages (powers of two throughout): 32-byte page header, 24-byte slots
(id/offset/length/live), record bytes appended. `put` inserts or replaces
(replace tombstones the old record — dead space is what defrag reclaims);
`erase` tombstones. Oversize puts (> 4040 bytes) are refused fail-closed.
A `generation()` counter bumps on every mutation. Persistence is a versioned
little-endian file (`DFRG` magic) written through `storage::writeFileAtomic`
(temp -> fsync -> atomic rename, default backup depth 8) — the crash-safe
contract. **Builder:** the Fragment's real persistence is assumed to be this
layout; if it evolves, it must honor the same session protocol
(begin/pump/cancel + reindex hook).

## 2. DefragService — chunked, crash-safe sessions

- `registerStore(name, store, filePath, reindexer?)` — the game registers the
  Fragment store at boot with its save path (the seam; nothing registered yet
  out of the box, and the command says so honestly).
- `beginDefrag(name)` — one session at a time, fail-closed.
- `pump()` — bounded work per call (64 scan pages / 256 copy records /
  256 reindex records per tick), driven from `ConsoleRegistry::tick()` so a
  ten-year Fragment never stalls the sim.
- Phases: Scan -> Copy -> Commit -> Reindex -> Done. Progress 0..1 with
  `statusLine()` one-liners for the console.
- **Mid-defrag mutation**: the start generation is snapshotted; a change forces
  a scan restart, bounded at 3 restarts, then the session fails closed
  ("store too busy") instead of livelocking.
- **Crash safety**: the live file is untouched until the atomic commit; the
  compacted image goes through `writeFileAtomic`. A stale `.tmp` is a crash
  footprint per the file contract. Cancel/failure leaves store and file
  exactly as before.
- **Reindex by design**: the optional `IReindexer` hook drives
  `reindexBegin/reindexRecord/reindexEnd`; `TextIndexerReindexer` adapts
  `index::TextIndexer` (record bytes as UTF-8 text, `clear()` + re-add,
  chunked). Defrag and index stay in sync — never one without the other.

## 3. lithoscript surface

`debug.defrag` (fits the locked `debug.` subsystem): `debug.defrag <store>`
starts a session, `debug.defrag status` prints the one-liner,
`debug.defrag cancel` aborts. Fail-closed throughout (no service / unknown
store / already running all report instead of acting).

## Verification

- New files: `engine/src/defrag/*` (`defrag.{h,cpp}`, `defrag_selftest.{h,cpp}`,
  `CMakeLists.txt`); edited: engine `CMakeLists.txt`, `tests/CMakeLists.txt`,
  `tests/engine_tests.cpp`, `concmd.{h,cpp}` (service install + tick pump +
  `debug.defrag`), `console/CMakeLists.txt`, `console_selftest.cpp` (9 wiring
  checks). New files LF; edited files kept existing endings.
- Defrag checks: **66/66 PASS** — round-trip, fragmentation accounting,
  byte-identical defrag with dead space reclaimed, reindex consistency (25/25
  docs), live-file-untouched-before-commit, mid-defrag mutation restart,
  churn-to-failure (bounded, no livelock, store intact), per-tick boundedness,
  cancel, empty store.
- Console checks: **all PASS** incl. the 9 new defrag wiring checks (missing
  service, status, start via command, completion through `tick()`, unknown
  store).
- `ctest`: **17/19** — only the 2 grandfathered builder failures
  (`engine_sim` block-names narrowing, `lithokraft_selftest` SIGABRT from the
  blocks.cpp 40-vs-36 guard). New `engine_defrag` test passes. No new
  failures. `trace_jit_basic.cpp` untouched.
- Note: full `make` (all targets) is red on GLFW in this VM (missing X11
  headers — pre-existing environment limitation, unrelated); all engine and
  headless targets build clean.
