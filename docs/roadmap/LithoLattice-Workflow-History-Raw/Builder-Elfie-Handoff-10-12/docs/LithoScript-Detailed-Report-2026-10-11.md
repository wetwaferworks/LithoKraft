# LithoScript — Detailed Implementation Report
**2026-10-11 · Surveyed directly from the source tree (read-only)**

LithoScript is the Half-Life 1-style console scripting language for the LithoLattice engine: commands, typed cvars, aliases, and `exec` of `.lth` script files. It is the language the debug console speaks, and it is exposed three ways: engine C++ API, the debug console UI, and AngelScript bindings. Valve and the Half-Life developers are the acknowledged inspiration (see the history log).

## 1. Module layout

All code lives in `engine/src/console/`:

| File | Role |
|---|---|
| `include/litholattice/console/concmd.h` | `ConsoleRegistry` API, `CVar`/`CVarType`, language + trust-model + seam documentation |
| `concmd.cpp` (~470 lines) | Tokenizer, typed cvars, aliases, sandboxed `exec`, archived-cvar save/load, built-in commands |
| `include/litholattice/console/script_console.h` / `script_console.cpp` | AngelScript exposure (`consoleCmd`, `consoleCvar`, `consoleSetCvar`), allowlist-gated |
| `include/litholattice/console/debug_console.h` / `debug_console.cpp` | Half-Life-style tilde dropdown console (UI framework composition) |
| `include/litholattice/console/console_selftest.h` / `console_selftest.cpp` | 22 headless checks covering the whole module |

The game wires it in `game/LithoKraft/src/game/game.cpp`: constructs `consoleRegistry_`, sets the config dir from the save path's parent, constructs `DebugConsole`, attaches it to the UI root, routes tilde/prefilter/update through the frame loop.

## 2. The language (as implemented)

**Tokenizer** (`ConsoleRegistry::tokenize`, static, test-exposed): splits a line into `;`-separated commands; whitespace-separated tokens; double-quoted strings (with backslash escapes); backslash escapes outside quotes (`\;`, `\ `); `//` starts a comment (rest of line dropped). Quake/HL1-faithful.

**Commands** (`registerCommand` / `unregisterCommand` / `hasCommand`): C++ callbacks `void(ConsoleRegistry&, const ConsoleArgs&)`. Names must match `[a-z0-9_:.]{1,64}` (lowercase; dots and colons allowed — the charset anticipates hierarchical names). A name colliding with a command, cvar, or alias is refused. Unknown commands print `Unknown command "x" (try "help")`; errors print, never throw.

**Typed cvars** (`registerCVar` / `setCVar` / `findCVar`): four types — `String`, `Int`, `Float`, `Bool`. Values are stored as strings but always in canonical form: ints via `strtol`, floats via `%.9g`, bools accept `1/true/yes/on` and `0/false/no/off` (case-insensitive) and canonicalize to `1`/`0`. Setting with a non-coercing value is refused and the old value is kept. Reading a bare cvar name prints `"name" = "value" (type[, archived])`; `name value` sets it. Each cvar carries `defaultValue`, an `archived` flag, and help text.

**Aliases** (`setAlias` / `removeAlias` / `hasAlias`): named command sequences (`alias <name> "cmd1; cmd2"`). Expansion is textual, Quake-style: extra arguments are appended to the body. Aliases cannot shadow commands or cvars (refused at define time). **Recursion cap: `kMaxAliasDepth = 16`** — `alias loop "loop"` and mutual `a→b→a` recursion terminate with a printed error.

**`exec` of `.lth` files** (`executeFile`, `setConfigDir`): runs a script from the config dir and *only* there. Sandboxing, all fail-closed with printed reasons:
- Refuses absolute paths, `..`, drive-letter forms, and names not ending in `.lth` (`.cfg` still executes as a legacy alias — the format is identical, plain text).
- Resolves the real path (`weakly_canonical`) and verifies it stays under the config dir — symlink escapes are caught.
- **Exec-chain depth cap: `kMaxExecDepth = 8`** (mirrors the IExtensible depth cap) — `a.lth → b.lth → a.lth` cycles always terminate; nesting at/past the cap is refused.
- `//` comments stripped per line; each line goes through the normal `executeLine` path.

**Archived cvars** (`saveArchived` / `loadArchived`): writes archived cvars as `name "value"` lines via atomic temp-file + rename; reading uses the same line format as `exec`. This is the persistence mechanism (the `config.lth` convention is proposed in the comparison report — not yet wired).

**Built-in commands** (`registerBuiltins(bool debugBuild)`): `help`, `echo`, `alias`, `unalias`, `exec`, `cvarlist`, `cmdlist`, `bind`, `unbind`, `version` ("LithoScript console (SDK 1)"). Debug builds add `dbg_logtap` (engine-log tap stats) and `dbg_registry` (registry counts). The debug console additionally registers `toggleconsole` and `clear` at attach time.

**Deliberate omission — `wait`:** there is intentionally no `wait`/`sleep`/`delay` command in v1. LithoScript cannot stall or pace itself, so wait-based infinite loops are impossible. A selftest explicitly guards against its reintroduction.

## 3. The three exposures

1. **Engine (C++ API):** `registerCommand` / `registerCVar` / `unregister*` on the `ConsoleRegistry` the engine owns. The registry is single-threaded by contract (game drives it on the main thread; script bindings call in on the script thread; v1 runs scripts on the main thread so no internal locking — documented in the header).
2. **Debug console (user):** tilde/`` ` `` drops a panel from the top of the screen (full width, top 42% of viewport), built from existing UI widgets (StackPanel + read-only TextEditor log view + TextBox input — composition, no new widgets). Available in **all** builds, not just debug. Input rules: tilde never opens the console while a text field/modal has focus (typing `~` in a field keeps working); tilde while open closes it (HL1 behavior); 200 ms debounce on held-tilde repeat; Up/Down walk 64-entry history; PageUp/PageDown scroll; Escape closes. Submitted lines echo Quake-style (`] line`). The engine's log streams into the view via the log tap (`LogTap`, 512-record ring in `core/log.h`); console output goes only to the view, not back into the engine log (avoids tap recursion). The input placeholder reads "type a LithoScript command (? for help)".
3. **AngelScript (scripts):** three bindings — `consoleCmd(line)` executes a console line, `consoleCvar(name)` reads a cvar (`""` when missing), `consoleSetCvar(name, value)` writes one. `registerConsoleScriptBindings()` puts the declarations on the `ScriptBindingAllowlist` (idempotent); `bindConsoleToScripts()` binds them per `ScriptEngine` per mod. **Both layers are gated on `kPermConsole`**: the allowlist entry requires the bit, and `bindGlobalFunction` refuses when the mod wasn't granted it — a script whose mod lacks the permission cannot bind them at all.

## 4. Permission gating & trust model

- **New permission bit `kPermConsole = 1 << 9`**, named `"console"` in the canonical permission table and the manifest name map (`mod_permissions.cpp`). It covers: registering console commands (namespaced `"modid:..."`), reading/writing cvars through the registry, and driving the console language from scripts. It explicitly does **not** grant file access — `exec`/`saveArchived` stay sandboxed to the engine's config dir regardless.
- **Script trust model** (documented in `concmd.h`): a `.lth` file carries no authority of its own — it runs with the invoking context's permissions. `exec` typed by the player runs as the player; a console line issued through a mod's `consoleCmd` binding runs with that mod's granted permissions. There is no command that grants permissions, so a script can never widen its own authority. Deny-by-default everywhere: no `kPermConsole`, no console access.
- **ModLoader wiring is builder follow-up**: the header notes mods register through the registry only when granted `kPermConsole` with `"modid:..."` namespacing — the ModLoader→`setModId()`/permission wiring was an open item at survey time.

## 5. Hardening summary

| Threat | Enforcement | Status |
|---|---|---|
| Alias infinite recursion (`alias x "x"`, mutual) | `kMaxAliasDepth = 16`, fail-closed with error | Implemented + tested |
| Exec cycles across files (`a.lth ↔ b.lth`) | `kMaxExecDepth = 8`, fail-closed with error | Implemented + tested |
| Path escape in `exec` (`..`, absolute, symlink) | Lexical refusal + canonical-path containment check | Implemented + tested |
| Timing loops | No `wait`/`sleep` command exists; guard test against reintroduction | Implemented + tested |
| Permission escalation via scripts | No privilege-granting command; trust = invoker's context; deny-by-default | Implemented (ModLoader wiring = builder) |
| Bad cvar values corrupting state | Typed coercion; refused sets keep the old value | Implemented + tested |
| Name squatting (alias shadowing a command) | Cross-namespace collision refusal at registration | Implemented + tested |
| `bind` abuse | Registry seam only — nothing dispatches binds yet | Seam (see §6) |

## 6. What's seam-only or not yet built

- **`bind`/`unbind` dispatch:** the registry records and validates key binds (`input::keyFromName`), but nothing dispatches them. Game-side follow-up: the input pump checks pressed bound keys per frame and feeds commands to `executeLine`. The `bind` command's own help text says "registry seam; not live-wired yet."
- **Fragment / typewriter / keyboard integration:** design only. The header documents the seam deliberately: single entry point `executeLine()`, swappable `OutputFn` sink, headless-drivable registry — a future Fragment integration constructs/shares a registry, installs its own sink (journal page), and feeds typed lines, with no console-module changes. Suggested follow-up: an `IConsoleDriver` interface so debug console, Fragment, and typewriter are three drivers over one registry.
- **ReadOR → world pipeline:** design only (see `~/workspace/user/files/ReadOR-Design-Note-2026-10-11.md`, referenced from the header, not duplicated). Open builder questions recorded in the header: the keyboard as script-input component; permission/trust model for agent- and proxy-authored scripts (standing rule: agent scripts run with the *agent's* permissions, never the player's); multiplayer authority for slotted Fragments (no privilege laundering).
- **Zero registered cvars/commands from engine or game:** verified by tree-wide grep — no `registerCVar`/`registerCommand` call sites exist outside the console module and its selftest. The language machinery is complete; the actual cvar surface (render, audio, sim, net settings) is unpopulated. Populating it is the natural next step (categories proposed in the comparison report).

## 7. Honest gaps & nits found in-tree

1. **Empty cvar surface** (§6) — the biggest gap: LithoScript today is a language with almost nothing to talk about. Engine/game settings (PBR tier, mesher selection, audio, sim) are not yet exposed as cvars.
2. **`toggleconsole` doc nit:** `concmd.h` lists `toggleconsole` among `registerBuiltins` commands, but it is actually registered by `DebugConsole::attach()`, not by `registerBuiltins`. Functionally present; the doc comment is slightly wrong.
3. **`?` placeholder vs reality:** the console input placeholder says "type a LithoScript command (? for help)" but `?` is not a registered command or alias — only `help` works.
4. **No autoexec/startup convention:** the config dir is set, but nothing auto-executes a startup script or auto-saves archived cvars. (Proposed: `autoexec.lth` + `config.lth` — see comparison report.)
5. **No `toggle`/`incrementvar`:** HL1 staples for binds; absent. (Recommended: add `toggle` — see comparison report.)
6. **No cvar flags beyond `archived`:** no `readonly`, `cheat`, `replicated` distinctions yet. The cheat-gating and multiplayer-replication models are undecided (see comparison report).
7. **Threading contract is documented, not enforced:** single-threaded-by-convention; fine for v1 (scripts run on the main thread) but worth re-checking if script execution ever moves threads.
8. **Survey caveat:** the tree was mid-reconciliation during this survey. Everything above was read directly from the files listed in §1 on 2026-10-11 ~08:05 EDT. The ModLoader `kPermConsole` wiring and any game-side cvar registrations that land after this survey are not reflected here.

## 8. Test coverage

`console_selftest.cpp` — 22 checks: log-tap ordering/capacity/clear; permission-table naming; tokenizer (quotes, escapes, `//` comments, `;` splitting); command dispatch, unknown-command errors, duplicate/invalid names refused; alias define/expand/nest/arg-append/recursion-cap/no-shadowing; cvar get/set, int/float/bool coercion, bad values rejected; archived round-trip (non-archived excluded); exec sandbox (`.lth` runs, `.cfg` legacy works, `..`/absolute/non-`.lth` refused, cycles terminate, depth cap); `wait`-absence guard; AngelScript allowlist gating + end-to-end script call; permission-denied binding refusal; console widget behavior (toggle, tilde suppression in fields, history, Escape); log-tap streaming into the view.
