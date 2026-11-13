# Debug Console + HL1 Console Language — Implementation Report (2026-10-11)

Worker 2 of 3 (sequential; tree held alone). Built the two companion systems —
(A) the robust dropdown debug console and (B) the Half-Life 1-style console
scripting language — as one system sharing one registry, so the interfaces
match by construction.

## What was built

**New module `engine/src/console/` → `litholattice_console`** (links core, ui, input, script):

| File | Contents |
|---|---|
| `include/litholattice/console/concmd.h` / `concmd.cpp` | The HL1 language: `ConsoleRegistry` — commands, typed cvars, aliases, `exec`, `;` sequences, tokenizer, key-bind seam. Carries the Fragment/typewriter future-seam design notes (header comment, design only — no item code). |
| `include/litholattice/console/debug_console.h` / `debug_console.cpp` | `DebugConsole`: the tilde dropdown. Composes existing UI widgets (StackPanel + read-only TextEditor log view + TextBox input). Tilde toggle, input-confinement respect, history, log streaming. |
| `include/litholattice/console/script_console.h` / `script_console.cpp` | AngelScript exposure: `registerConsoleScriptBindings()` (allowlist) + `bindConsoleToScripts()` (per-mod, permission-gated). |
| `include/litholattice/console/console_selftest.h` / `console_selftest.cpp` | 18 headless checks, wired as new ctest module `engine_console`. |
| `CMakeLists.txt` | Module build. |

**Edits to existing files:**

- `src/core/include/litholattice/core/log.h` + `src/core/log.cpp` (CRLF preserved): the **engine-log ring-buffer tap** — see "Worker-3 contract" below. Also adds `logWarn` (`"WARN "` on stderr).
- `src/mods/include/litholattice/mods/mod_interface.h`: new `kPermConsole = 1 << 9` with doc comment.
- `src/mods/include/litholattice/mods/mod_permissions.h` + `src/mods/mod_permissions.cpp`: `"console"` in the canonical name table, added to `kAllPermissionBits`.
- `engine/CMakeLists.txt`: `add_subdirectory(src/console)`.
- `tests/CMakeLists.txt` + `tests/engine_tests.cpp`: new `engine_console` ctest module.
- `LithoKraft/src/game/include/lithokraft/game/game.h` + `LithoKraft/src/game/game.cpp`: minimal game wiring — console owned by `Game`, tilde hook in `routeUiInput`, `update()` in `updateUi`, `kPermConsole` granted to the game's own scripts, exec sandboxed to the settings-file dir. `lithokraft_game` links `litholattice_console`.

## Key design decisions

**Worker-3 contract (mod suspension / auto-recovery logging).** The tap in `core/log.h` is
the *general engine-log record path*, not console-private. Every `logInfo`/`logWarn`/`logError`
line is pushed into `LogTap` (capacity 512, `LogRecord{seq, level, timestamp, text}`) under the
same lock as the text log. Worker 3 must log through the normal calls:
`logWarn("modsys: suspended mod '%s': %s", modId, reason);`
`logInfo("modsys: auto-recovered mod '%s' after %u ticks", modId, ticks);`
and read records back via `engineLogTap().snapshot()` (oldest→newest; `seq` is monotonic so a
reader can track "new since X" across ring wraps). Do NOT invent a parallel log channel.

**Permission-bit choice: NEW `kPermConsole` bit** (`1 << 9`, manifest name `"console"`).
Rejected reusing an existing bit: registering console commands / driving cvars from scripts is a
distinct capability — conflating it with `kPermUI` ("can touch UI widgets") or `kPermInput`
would let a UI-only mod redefine the console surface. Name-table + `kAllPermissionBits` extended;
deny-by-default semantics unchanged (unknown names still ignored, headless still denies).

**`bind`: registry seam only, NOT live-wired.** `bind <key> "<cmd>"` validates key names via
`input::keyFromName` and stores the table (`keyBinds()`), but nothing dispatches yet — wiring it
into the game's input pump is the flagged follow-up (one call site: check pressed bound keys per
frame → `ConsoleRegistry::executeLine`). Deliberately not half-wired.

**Tilde handling.** The `~`/`` ` `` arrives as a UI Text event (the bridge drops GraveAccent
KeyDown). `DebugConsole::handleTildeKey(ui, e)`: when closed, opens **only if
`!ui.wantsKeyboard()`** — and that check runs *before* the key-repeat debounce, so typing `~`
in a field always works. While open, the console's TextBox holds focus, so `wantsKeyboard()`
is true and keystrokes never reach mods (`ModInputRouter::shouldDeliverToMod`). 200 ms
debounce swallows key-repeat; known wart: holding tilde cycles open/close slowly (Text events
carry no repeat flag — the clean fix is mapping GraveAccent KeyDown through the bridge and
gating on non-repeat).

**Script exposure.** Three decls — `void consoleCmd(const string &in)`,
`string consoleCvar(const string &in)`, `void consoleSetCvar(const string &in, const string &in)` —
all require `kPermConsole` via the existing allowlist gate (`bindGlobalFunction` refuses
otherwise; verified denied-without-permission in tests). The game's own scripts get
`kPermConsole` in `initScripts` (first-party).

**Fragment seam (design only).** `concmd.h` documents it: `executeLine()` is the single entry,
output goes to the installed `OutputFn` sink, cvars are UI-free — a future Fragment journal /
typewriter / keyboard integration just installs its own sink and feeds lines. Suggested
follow-up: an `IConsoleDriver` interface so debug console, Fragment, and typewriter are three
drivers over one registry. No item code written.

**Cvar semantics.** Types string/int/float/bool with canonical coercion (`on/off/yes/no/true/false/1/0`
for bool); bad values rejected, old value kept. `archived` cvars round-trip via
`saveArchived` (atomic temp+rename, `name "value"` format); `exec` sandboxed: `.cfg` only,
no absolute paths, no `..`, resolved path must stay under the config dir (symlink-escape check).

**Debug builds** get extra commands (`dbg_logtap`, `dbg_registry`); the console itself ships in
all builds.

## Test results

`engine_console`: **18/18 PASS** — tap levels/order/last-512/clear; permission name table;
tokenizer (quotes, escapes, `//` comments, `;` split); command dispatch + unknown-command error +
duplicate/invalid names refused; aliases (define, nested expansion, Quake-style arg append,
recursion cap → "alias recursion too deep", no shadowing); cvar get/set + int/float/bool
coercion + bad-value rejection + archived save/load round-trip (non-archived excluded); exec
sandbox refusals (`..`, absolute, non-`.cfg`, missing); `;` ordering; bind validation;
debug-only commands; AngelScript allowlist gating + script-driven command/cvar round-trip +
**denied without `kPermConsole`**; console toggle; tilde suppressed while a field has focus;
history Up/Down; Escape close; tap records appear in the log view.

Full `ctest` (17 tests): **15 pass**; the 2 failures are the grandfathered ones, both
pre-existing and unrelated (verified): `engine_sim` block-names narrowing, and
`lithokraft_selftest` SIGABRT — backtraced to `blockRegistry()` static-init inside the game's
signal selftest (the known content-registry desync), aborting before any console code runs.
Full build (engine + game) is clean, no new warnings.

## Open questions / follow-ups

1. **Bind dispatch** (game input pump → `executeLine`) — flagged above, small.
2. **ModLoader wiring for `kPermConsole`**: gate a `registerConsoleCommand` API on
   `grantedPermissions & kPermConsole`, namespace commands `"modid:..."` (builder follow-up).
3. **Tilde hold-cycle wart** (see above) — bridge-level fix if it annoys.
4. **`toggleconsole`/`clear` commands** are registered by `DebugConsole::attach` (not by
   `registerBuiltins`) so a headless registry doesn't advertise a widget it has no handle to.
5. The game selftest abort (content-registry desync: blocks.cpp registers 40 rows, guard
   expects 36) is the builder's known blocker — untouched.
