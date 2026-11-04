# Mod-Hardening Implementation Report — 2026-10-11

JD approved the full audit. All seven items implemented sequentially by four workers on the shared source tree at `~/workspace/litholattice/engine`.

## Threat model (program-wide)

Mods are untrusted third-party code running inside a game played by real users, including younger players. The engine assumes:
- A mod may be buggy (infinite loops, leaks, corrupt data) or actively malicious (data theft, spoofing, cheating).
- Native mods can call libc — OS-level sandboxing is explicitly future work (signed mods also future). Everything below is defense-in-depth *inside* the engine: deny-by-default, fail-closed, least privilege, per-mod quotas, quarantine-never-crash.
- The one unforgivable outcome: a mod corrupting or killing the user's world. Every item below fails toward "mod disabled, world loads."

## Status summary

| # | Item | Status |
|---|------|--------|
| 1 | Script binding allowlist | DONE — 31/31 checks (with item 2–3) |
| 2 | Per-mod CPU/memory quotas | DONE |
| 3 | SDK version enforcement at load | DONE |
| 4 | Save-data integrity (checksum + version + quarantine) | DONE — 16 new checks |
| 5 | Network confinement (API-only, rate-limited) | DONE — 24 new checks |
| 6 | UI confinement + mod badging | DONE — 21 new checks |
| 7 | Input confinement (no raw keys, no synthetic input) | DONE — 13 new checks |
| 8 | Permission manifest + approval flow | DONE — 29 new checks |

Out of scope by design: file sandboxing + O_NOFOLLOW/fd re-validation (separate worker), signed mods (future), SimThread integration (builder).


## Part 1 — Script sandbox binding allowlist + per-mod quotas + SDK enforcement

### Item 1 — Binding allowlist: DONE
- New `ScriptBindingAllowlist` (`src/script/include/litholattice/script/script_binding_allowlist.h` + `.cpp`): deny-by-default registry; each entry carries its required `LithoLatticeModPermission` bits.
- `ScriptEngine::raw()` **removed** — no bypass path. All binding goes through `ScriptEngine::bindGlobalFunction()`, which refuses unlisted decls and decls the mod's permission bitmask (set via new `setModPermissions()`, from mod.json) doesn't grant.
- Runtime registration additionally rejects dangerous tokens (`file`, `socket`, `exec`, …) as defense-in-depth; threat model documented in the header.
- Game call sites migrated: `game.cpp`, `ui_script_bindings.cpp`, `selftest.cpp`, `game_ui_selftest.cpp`; `docs/ARCHITECTURE.md` updated.

### Item 2 — Per-mod quotas: DONE
- CPU: the existing (never-enforced) `setModBudgetUs` (1024µs default) is now enforced per tick — `beginTick()` resets accounting; overrun → throttled (hooks skipped N ticks, logged with numbers) → suspended after N consecutive overrunning ticks (logged reason); clean tick breaks the streak; `resume()` clears.
- Memory: AngelScript's global memory functions installed once per process; 16-byte header per allocation records size + owner; thread-local attributes allocations during `Execute()` to the mod; exceeding quota → immediate suspend with logged reason.
- Defaults all powers of two (1024µs, 1MiB, 8 ticks, 4 throttles).
- Known limitation documented: the string add-on's `std::string` buffers bypass AS memory functions (script objects — the unbounded vector — are counted).

### Item 3 — SDK enforcement at load: DONE
- `ModLoader` was declared but *never implemented* — wrote `src/mods/mod_loader.cpp` (scan, mod.json parse incl. permissions, `checkCompatibility`, new `load()`); `litholattice_mods` converted from INTERFACE to a real static lib.
- Policy per JD: **major mismatch → refused, minor mismatch → warn-and-continue**.
- mod.json `"sdk"` accepts `"1"`, `"1.2.0"`, numeric `1`, or ranges like `">=1.0.0 <2.0.0"`.

### Tests
New ctest modules `engine_script` (22 checks) + `engine_mods` (9 checks) — adversarial coverage included (unlisted binding refused, CPU overrun → throttle → suspend, memory overrun → suspend, bad SDK refused/warned). **31/31 pass** (verified via standalone runner; full ctest link was blocked at the time by a concurrent worker's in-flight render edits).

### Incidental fixes
- `mod_interface.h` had a latent `sdkVersion` field/function-pointer redeclaration — the header never compiled before; renamed the field to `sdkMajor`.
- Added missing `<algorithm>` include and a missing include in the concurrent worker's `mods_selftest.cpp` (their logic untouched).

### Open questions for JD
1. Accept the `std::string` memory-quota blind spot for v1 or close it now?
2. Keep `ui_*` in the builtin allowlist table or move to runtime registration?
3. Suspension recovery stays manual-admin or auto on reload?

# Mod Hardening, Part 2 — Save-Data Integrity + Network Confinement
*(Elfie, 2026-10-11 — engine items 3–4 of the mod-hardening work)*

## 3. Save-data integrity (`ModSaveRegistry`)

**New files**
- `src/mods/include/litholattice/mods/mod_save_data.h`
- `src/mods/mod_save_data.cpp` (wired into `litholattice_mods`; the lib now links
  `litholattice_storage` PUBLIC for the CRC32 framing)

**What changed.** Every mod gets its own save-data section in the world
directory: `<world>/moddata/<modid>.llms`. Each section is checksummed and
versioned. On world load every section is verified *before* the mod's
`onWorldStart` runs; a corrupt or version-incompatible section is quarantined
and the mod is disabled for that world (logged reason). The world always loads.

Framing reuses the storage layer's own CRC32 framing (`storage::frameRecord` /
`readValidatedRecord`, "LFRM" magic, CRC over magic‖len‖payload) and atomic
writes (`storage::writeFileAtomic`: temp + fsync + rename, backup rotation at
the file-contract default of 8) — mod sections get exactly the same torn-write
and bit-rot protection as region files. The inner payload (LE `ByteWriter`)
records: format version (1), mod id, mod version (semver), the mod's own
`dataVersion` schema number, the SDK major at write time (diagnostic), and the
mod's bytes.

**Load outcomes** (`ModSaveStatus`): `Ok` · `Fresh` (mod installed, no section
yet — starts with empty data) · `Corrupt` (framing/CRC/inner-parse/id-mismatch
failure — quarantined, mod disabled) · `VersionIncompatible` (major mismatch,
or minor/patch change with no usable migration — quarantined, mod disabled) ·
`Orphaned` (section for a removed mod — **kept on disk, never deleted**,
logged). The quarantine contract from `file_contract.h` is honored end to
end: quarantined files are left on disk for recovery tooling, and `save()`
**refuses to overwrite a quarantined section** (fail-closed; the game surfaces
the reason and the user/tooling moves the file aside).

**Version policy / migration hook (builder seam).** Stored major ≠ running
major → `VersionIncompatible`, no cross-major migration. Same major but
different minor/patch → the registered `ModSaveMigrateFn(modId, fromVersion,
toVersion, fromDataVersion, payload)` is called; a returned payload means the
mod runs on upgraded data, `nullopt` (or no hook registered) means quarantine.
The default posture is fail-closed: the world never runs a mod on data it
cannot prove it understands. Identical semver → `Ok` regardless of
`dataVersion` (schema drift within one release is the mod's own business; the
recorded number is there if it wants it).

**Threat model.** A mod's bytes are untrusted input even when the engine wrote
them — a crash, a disk fault, or a hand-edited file must not let garbage flow
into the sim. Verification happens before any mod code sees the data; a bad
section disables exactly one mod, never the world. Mod ids are validated
(`ExtensionRegistry::isValidId`, `[a-z0-9_]{1,64}`) before they touch the
filesystem, so a hostile id cannot escape `<world>/moddata`.

**Decisions.** (1) One file per mod, not one container — a corrupt section
cannot take its neighbors down, and orphaned sections survive mod removal
independently. (2) Quarantine-not-delete everywhere, matching the storage
layer's existing contract. (3) `Fresh` is a normal outcome, not an error
(fresh install into an existing world is the common case).

## 4. Network confinement

**New files**
- `src/net/include/litholattice/net/confinement.h`
- `src/net/confinement.cpp`
- `src/net/include/litholattice/net/confinement_selftest.h`
- `src/net/confinement_selftest.cpp` (both wired into `litholattice_net`)

**API surface** (`mod_interface.h`): new permission `kPermNet = 1<<8`, a single
mod→engine entry point `netSend(mod, type, payload, len)` on
`LithoLatticeModApi`, and the engine→mod callback `onNetMessage(...)` on
`LithoLatticeMod`. Delivery is unreliable/sequenced/latest-wins (the channel
model in `channels.h`); reliable variants are a later SDK. `mod_safety.h`
rule 7 updated: no *raw* network — the engine-gated API is the sanctioned path.

**Layer 1 — API audit (compile-time).** The mods selftest pins the surface:
an audit table lists every field of `LithoLatticeModApi` (14) and
`LithoLatticeMod` (19) in declaration order; structural pins
(`offsetof(netSend)+sizeof(netSend)==sizeof(LithoLatticeModApi)`, same for
`onNetMessage`) fail if either struct grows without the table — and the audit —
being updated. Every tabled name is scanned for socket/fd/DNS/process tokens
(`socket`, `sendto`, `connect`, `bind`, `fd_`, `getaddrinfo`, …); `netSend` is
asserted to be the *sole* network entry point; `kPermNet` is asserted to be a
fresh bit. Audit method: `grep -n` over `mod_interface.h` for
`socket|sendto|recv|connect|bind|listen|accept|getaddr|inet_|ioctl|epoll|kqueue`
returns only comments and the `netSend`/`onNetMessage` docs — no primitive
anywhere in the surface. ("raw" is deliberately not a scan token: it
false-positives on `onDrawSolid`; a raw-socket-style name still trips
"socket".)

**Layer 2 — runtime guard** (`confinement.h`). `NetOrigin { Engine, Mod }`
tags traffic at the send-path choke point; `NetConfinement::checkSend` fails
closed — unregistered mod, null gate, or exceeded budget drops the datagram
with a logged reason. `ConfinedChannelMux` is the only path from here to the
wire: `sendEngine()` (ungated) vs `sendMod()` (gated). Mod wire types must be
`>= kModNetTypeBase` (256); engine `PayloadType` 1–4 are refused for mods, so
a mod can never spoof engine traffic kinds.

**Rate limiting** (`ModNetGate`). Per-mod budgets, powers of two per JD's
convention: `kModNetBytesPerSecDefault = 4096` (2¹²),
`kModNetPacketsPerSecDefault = 64` (2⁶). Sliding 1-second windows on the
game-provided monotonic ms clock (deterministic, fakeable — same convention
as `channels.h`; never wall-clock). Overrun → `Throttle` (dropped, logged
reason); `kModNetAbuseThrottleLimit = 16` (2⁴) throttles in one window →
network access `Suspended` (logged) until the engine calls
`resetSuspension()`. Budgets adjustable per mod via `setBudgets()`. Not
thread-safe by design — owned by the net tick, like the channels.

**Threat model (honest scope).** For native (C++) mods the API surface is the
real control — in-process code can always call libc directly; only OS-level
sandboxing changes that (future work, builder note). What this layer
guarantees: (a) the engine never forwards mod traffic except through the
gated path, (b) unbudgeted/unknown mod traffic can never reach the socket,
(c) abusive mods lose network access automatically with an audit trail.
Script mods additionally sit behind the binding allowlist, where no
raw file/network/process/system bindings exist at all.

**Decisions.** (1) Drop-on-throttle rather than queue: queued mod traffic
would let a mod build unbounded backlog in the engine; latest-wins semantics
make drops cheap. (2) Suspension is sticky until engine reset — no silent
auto-recovery that an abusive mod could ride. (3) Type reservation ≥256 keeps
mod traffic distinguishable from engine traffic on the wire without growing
the 14-byte header (no protocol bump). (4) Clock is caller-provided ms, so
tests are deterministic.

## Verification status

- `litholattice_mods`, `litholattice_net`, and 12 other engine libs: **build clean**.
- `engine_mods` ctest: **PASS** — includes the new API-surface audit (5 checks)
  and save-data integrity (16 checks: round-trip, Fresh, byte-flip corruption →
  quarantine + mod disabled + world loads + file kept + save-refuses-quarantined,
  inner id mismatch, major mismatch, minor-without-migration, migration hook
  success, declined migration, orphaned-kept, invalid-id refusal, semver parsing).
- `engine_net` ctest: **PASS** — includes the new confinement selftest
  (24 checks: registration, budgets, packet/byte overrun → throttle, 16
  throttles → suspend → engine reset, fail-closed on unregistered mod / null
  gate, engine-type spoof refused, mux-level sustained-abuse → suspension,
  window rollover, powers-of-two defaults).
- Full `ctest`: 12/16 pass. The 4 failures are **not mine**: `engine_render`
  (mesher golden-hash — the render worker's mid-edit mesher work),
  `engine_sim` (block-names narrowing — grandfathered, builder's fix),
  `lithokraft_selftest` (SIGABRT — grandfathered, builder's fix),
  `lithokraft_headless_harness` (segfault — another worker's new harness).
  No new failures introduced. (Note: mid-task the full `litholattice_tests`
  link was briefly blocked by the net worker's in-flight
  `UnreliableChannel::decodeAccepted` — declared, not yet defined; I verified
  via standalone runners in `/tmp/standalone/` in the meantime. The net worker
  has since landed the definition and the tree links again; both my suites now
  pass through the real ctest path too.)

## Open questions (for JD / builder)

1. **Who drives save/load?** `ModSaveRegistry` is ready; the game still needs
   to call `loadAll()` on world load (before mods' `onWorldStart`) and
   `save()` on world save/autosave. Builder wiring.
2. **Inbound routing.** `onNetMessage` exists on the mod struct; the game must
   route addressed inbound datagrams to it (and register net-capable mods with
   `ModNetGate`, binding the API's `netSend` through `ConfinedChannelMux`).
3. **Reliable mod traffic.** v1 is unreliable/latest-wins only. If mods need
   reliable delivery later, it should ride the reliable channel with its own
   budget accounting — new SDK item, not a quiet extension.
4. **Budget tuning.** 4096 B/s + 64 pkt/s defaults are principled guesses
   (powers of two); real playtesting should confirm them, per-mod overrides
   via `setBudgets()` exist for that.
5. **Suspension UX.** Suspension is sticky until engine reset. Whether the
   game should surface/auto-expire it (and after how long) is a game-design
   call — currently it stays suspended and loudly logged.
6. **Signed mods** remain a future idea (out of scope, per brief); the
   quarantine + version machinery here is the integrity layer it would build on.
7. **moddata backup depth** inherits the file-contract default (8). If mod
   sections deserve their own depth dial, say so.

# Mod Hardening, Part 3 — UI Confinement + Badging, Input Confinement
*(Elfie, 2026-10-11 — engine items 5–6 of the mod-hardening work)*

Prior parts: item 1–2 (Worker A: script binding allowlist, per-mod CPU/memory
quotas, SDK enforcement), items 3–4 (Worker B: `ModSaveRegistry`, `ModNetGate`,
`kPermNet`) — in tree, not redone here. File sandboxing / O_NOFOLLOW / fd
re-validation is a separate worker's; signed mods are future, out of scope.

## 5. UI confinement + mod-content badging

**New files** (LF, per convention)
- `src/ui/include/litholattice/ui/mod_ui.h` — `ui::ModPanel` (final),
  `ui::ModUiFactory`, reserved-style list, budget constants, full threat
  model in the header comment.
- `src/ui/mod_ui.cpp` — implementation (wired into `litholattice_ui`).

**Changed**
- `src/ui/theme.cpp` — `Theme::defaults()` gains a `"mod_panel"` style:
  dark panel with a 2px amber border. Distinct from the system dialog look by
  construction; mods ship no theme packs so they cannot restyle this class.
- `src/ui/CMakeLists.txt` — adds `mod_ui.cpp`.
- Selftests: `src/ui/ui_selftest.cpp` (`testModUiConfinement`, 21 checks),
  `src/mods/mods_selftest.cpp` (`testModApiUiInputAudit` + token scanner).

**What changed.** Mods build UI only through the engine widget system, and
only through the confined factory:

1. **No raw draw calls.** The mod API surface (`LithoLatticeModApi`) hands
   mods no `Painter`, no `IUiRenderer`, no `Widget*`, no paint hook. The only
   UI entry point is `registerUiPanel(mod, id, title)` — strings in, void
   out. Mod-supplied layout is JSON text built by the widget factory's
   *closed* type table (no dialog/modal/popup types exist there at all).
   Widget event handlers from mod JSON are script functions only
   (`Widget::onScript` via `IScriptHost`); the C++-callable `Widget::on`
   overload is never wired by the factory. Verified by code audit; pinned by
   the token scan in `testModApiUiInputAudit`.
2. **Confined factory.** `ModUiFactory::createPanel(modId, panelId, title,
   bodyJson, err)` is the only sanctioned path for mod UI. The engine binds
   `LithoLatticeModApi::registerUiPanel` here *after* checking `kPermUI`
   (binding recipe below). It validates ids (`[a-z0-9_]{1,64}`, the
   extension-registry rule), caps titles at 128 chars, pre-scans mod JSON
   iteratively for nesting depth (>32 refused) and node count (>1024 refused)
   *before* the recursive factory runs (no stack-overflow DoS), builds via
   the closed type table, then walks the built tree rejecting the reserved
   style classes and enforcing the per-panel widget budget (256, 2^8).
   Anything fails → nullptr + reason (fail closed).
3. **Badging — engine-applied, not mod-removable.** Every mod panel root is
   a `ModPanel final : public Panel`. `final` means no subclass (and no mod)
   can override or skip the badge painting. The badge strip (amber MOD tag +
   owning mod id + title) is painted with **fixed colours, not theme
   lookups**, so no pack or mod can restyle or hide it; the panel arranges
   children strictly *below* the strip; the strip paints *after* the
   children. A mod child can never cover, remove, restyle, or opt out of
   the badge — and mods never receive a `ModPanel*` in the first place, so
   there is nothing to downcast. A fixed amber rule under the strip survives
   even a pack that recolours `"mod_panel"` into invisibility.
4. **No system-look spoofing.** The factory rejects the reserved style
   classes `"dialog"` (system modal look), `"tooltip"` (system hint look),
   `"mod_panel"`/`"mod_badge"` (engine-applied) anywhere in mod JSON.
   `attachToRoot` attaches to the UiRoot **main layer only** — mods can never
   open modals or popups, so a fake system prompt can never seize the screen.

**Threat model (fake system prompt / credential phishing).** A mod wants the
player to believe a *system* dialog is asking for a password, a key, or a
confirmation. Defences: (a) every mod-owned pixel lives under a `ModPanel`
whose MOD badge is always visible and always names the mod — attribution is
inescapable; (b) mods cannot use the system dialog/tooltip frame treatment;
(c) mods cannot open modals, so they cannot block the screen; (d) keystrokes
typed while a system modal/text field has focus never reach mods at all (see
item 6: `ModInputRouter::shouldDeliverToMod`) — even the *real* password
dialog is unreadable to mods. Honest scope, as with the net confinement: for
native (C++) mods the API surface is the real control — in-process code could
in principle call engine symbols it links against, and only OS sandboxing
(future work, builder note) changes that. What this layer guarantees: the
engine never builds mod UI except through the confined factory, and
everything it builds is badged.

**Binding recipe (for the game / builder — no API binder exists yet for ANY
`LithoLatticeModApi` field).** The game fills `api.registerUiPanel` with a
trampoline that (1) checks the mod's `kPermUI` bit, (2) resolves the mod id
from the handle, (3) calls `ui::ModUiFactory::createPanel(...)`, (4) on
success calls `ui::ModUiFactory::attachToRoot(uiRoot, ...)`. `bodyJson` is
currently unused by the C API (which passes only id+title); a future
`setUiPanelLayout`-style entry would feed it — that would grow the API struct
and must update the audit tables + pins in `mods_selftest.cpp`.

**Verification.** 21 new UI checks + 2 new mods-audit checks, all passing;
adversarial cases: reserved styles rejected (all 4); unknown widget type
rejected; hostile full-panel child cannot intersect the badge strip and the
MOD tag still paints; 64-deep JSON refused; 1100-node JSON refused;
300-widget JSON refused; `static_assert(std::is_final_v<ModPanel>)`;
attachment lands in the main layer with no modal/popup open. `ctest -R
"engine_ui|engine_input|engine_mods|engine_net|engine_script"`: 5/5 pass.

## 6. Input confinement

**New files** (LF)
- `src/input/include/litholattice/input/mod_input.h` — `ModInputRouter`,
  namespaced action registry, delivery policy, anti-cheat audit notes.
- `src/input/mod_input.cpp` — implementation (wired into `litholattice_input`).

**Changed**
- `src/input/CMakeLists.txt` — adds `mod_input.cpp`.
- Selftests: `src/input/input_selftest.cpp` (`testModInputConfinement`,
  13 checks), `src/mods/mods_selftest.cpp` (input half of
  `testModApiUiInputAudit`).

**What changed.**

1. **Game input events only.** The mod API surface exposes no `InputSystem*`,
   no `IInputBackend*`, no `DeviceState*`, and no key/mouse/gamepad state
   query of any kind. The only input-related entry points are
   `registerInputAction` (strings in, void out) and the engine→mod
   `onInputEvent(mod, key, down)` hook. Pinned by the token scan
   (`testModApiUiInputAudit`): no field name in either API struct carries a
   raw-draw, device, polling, or injection token.
2. **One-directional flow.** `ModInputRouter` is the engine-side registry of
   mod input actions and the delivery-policy check. It holds no `InputPump*`,
   no `InputSystem*`, no `IInputBackend*`, no `DeviceState*` — there is
   deliberately no inject/push/send method on the class; a synthetic event is
   inexpressible through it. Engine→mod is a call the *game* makes; mod→engine
   input does not exist.
3. **Namespaced actions.** `registerAction` (which the engine binds *after*
   checking `kPermInput`) stores actions as `"modid:action"` — a mod can
   never shadow an engine action or another mod's action. Ids are
   `[a-z0-9_]{1,64}`; duplicates, bad ids, and empty display names are
   refused (fail closed). `unregisterMod` drops a mod's actions on unload.
4. **Delivery policy.** `shouldDeliverToMod(uiWantsKeyboard)`: a key event
   reaches a mod's `onInputEvent` only when the UI does *not* want the
   keyboard (`UiRoot::wantsKeyboard()` — no modal open, no popup open, no
   text field focused). Keystrokes typed into system dialogs — passwords
   included — never reach mods.
5. **No text exfiltration by construction.** The hook signature is
   `(mod, key, down)` — no `Text`-event codepoints, no device ids. A mod
   cannot keylog typed text and cannot tell which device produced a key.

**Anti-cheat: no mod-callable input injection.** The engine's input pipeline
has exactly two event-injection points, both engine-internal and both
unreachable from mod code:
- (a) `ReplayInputBackend::push()` — the replay/testing seam. The backend
  object is owned by `InputPump` (private `unique_ptr`); the mod API surface
  exposes no path to it, and the script binding allowlist (Worker A)
  contains no input-injection binding.
- (b) `InputSystem::processFrame()` — replay/tests. Mods never receive an
  `InputSystem*`.
Engine-internal injection (replay/testing harness) stays, as required — it is
simply unreachable from mod code. Same honest-scope caveat as the net
confinement for native mods (in-process code vs. OS sandboxing, future).

**Verification.** 13 new input checks, all passing; adversarial cases: a
synthetic key event has no router method to travel through — the test pins
the engine pipeline (`DeviceState`, action state) as unchanged after a
game→mod delivery; the router works standalone with no input-system handles
(raw-device read inexpressible); duplicate/bad ids refused; namespacing
verified (`mymod:jump` vs `othermod:jump`); delivery withheld when
`uiWantsKeyboard`.

## Audit results (code audit, 2026-10-11)

- `mod_interface.h`: no field hands out a painter, renderer, widget, input
  system, backend, or device handle. `registerUiPanel`/`registerInputAction`
  are strings-in/void-out; `onInputEvent` is `(mod, key, down)` with no
  text/device payload; `onDrawSolid`/`onDrawTranslucent` are engine→mod
  render callbacks (Half-Life style) carrying no painter — deliberately not
  flagged. No socket/fd/DNS/process tokens (Worker B's audit still green).
- `src/input/`: injection points are `ReplayInputBackend::push` and
  `InputSystem::processFrame` — both public for engine/test use, both
  unreachable from the mod API surface (no handle ever handed out; ownership
  is private to `InputPump` / the game).
- `src/ui/`: the JSON factory's type table is closed (widget, panel, label,
  button, image, checkbox, progress, slider, number, slot, divider, stack,
  grid, textbox, editor, list, dropdown, scroll, splitter, tabs) — no
  dialog/modal/popup/raw-paint types exist; mod JSON wires script handlers
  only.

## Decisions

1. Badge colours fixed, not themed — attribution must survive any theme pack.
   The *frame* treatment (`"mod_panel"`) does come from the theme system, as
   the task asked, with a fixed amber rule under the badge strip as backup.
2. Mod panels can never be modal/popup — a fake system prompt must not seize
   the screen. (System dialogs keep exclusive modal rights.)
3. Keystrokes never reach mods while the UI wants the keyboard — protects
   even real system dialogs from mod keylogging.
4. DoS budgets as powers of two (JD's convention): JSON depth 32, nodes 1024,
   widgets/panel 256, title 128 chars, ids `[a-z0-9_]{1,64}`.
5. No `LithoLatticeModApi` struct changes — enforcement is engine-side, so
   Worker B's struct-size audit pins (14/19 fields) still hold untouched.
6. `ModPanel` is `final` rather than relying on a flag — the badge cannot be
   overridden, only the engine constructs it.

## Open questions / follow-ups

1. **API binder (builder).** No `LithoLatticeModApi` function pointer is
   bound yet (all items, not just mine). Binding recipe documented above;
   the binder must check `kPermUI`/`kPermInput` before calling into
   `ModUiFactory`/`ModInputRouter`.
2. **Panel content updates.** `registerUiPanel` passes only id+title today.
   If mods need to populate panels later, prefer a `setUiPanelLayout(mod,
   panelId, json)` API entry feeding `createPanel`'s `bodyJson` (already
   implemented and tested) — growing the struct requires updating the audit
   tables + pins.
3. **Found (not mine, flagging):** `ModLoader::parsePermissions` does not
   parse `"net"` → `kPermNet` (Worker B's bit) can never be granted via
   mod.json. Left untouched to avoid colliding with Worker B; needs their
   fix.
4. **System-dialog positive marking (optional).** Real system dialogs could
   carry a small engine-drawn marker (complement to the MOD badge) to make
   "this prompt is really from the system" checkable at a glance. Not
   implemented — suggest JD decide.
5. **Native-mod honest scope.** As documented: for native C++ mods the API
   surface is the control; OS-level sandboxing remains future work (builder
   note), same as the net confinement.

## In-flight collision note (2026-10-11 ~10:48 UTC)

While verifying, the extension/IExtensible worker landed mid-flight edits in
`src/mods/` (`extension_loader.cpp`, `extension_budget.cpp`,
`extension_runtime.cpp`, `extension_registry.*`, `mod_loader.h`,
`extensible_interface.h`, `src/mods/CMakeLists.txt`). Their
`extension_loader.cpp` currently does not compile (declaration mismatch vs.
their own header) — this breaks the `litholattice_mods` lib build and
therefore the `engine_mods` ctest **right now**. This is not my breakage and
I did not touch their files (per the coordinator's reconciliation plan: let
all coordinators land, then one rebuild/fix pass).

My code is unaffected and verified against their headers:
- `src/mods/mods_selftest.cpp` (with my audit additions intact — they
  appended their tests after mine) compiles clean.
- The UI/input audit was re-verified standalone against the current
  `mod_interface.h`: AUDIT OK (6/6).
- The last full green run of my modules (`ctest -R
  "engine_ui|engine_input|engine_mods|engine_net|engine_script"`: 5/5 pass;
  direct binary: ENGINE TESTS OK for ui/input/mods) was before their
  breakage. `engine_mods` cannot link until their TU is fixed — flagged for
  the reconciliation pass, not fixed here.

## Test summary

- New: 21 UI checks (`testModUiConfinement`), 13 input checks
  (`testModInputConfinement`), 2 mods audit checks
  (`testModApiUiInputAudit`) — all pass.
- `ctest -R "engine_ui|engine_input|engine_mods|engine_net|engine_script"`:
  5/5 pass. Full `ctest`: 12/16 pass; the 4 failures are **not mine**:
  `engine_render` (mesher golden hash — I touched nothing in render/meshing;
  likely the in-flight PBR worker), `engine_sim` (block-names narrowing —
  grandfathered, builder's), `lithokraft_selftest` (SIGABRT — grandfathered,
  builder's), `lithokraft_headless_harness` (SEGFAULT — the known
  coordinator-collision broken tree; I touched nothing in game/).
- Build: clean, no new warnings (checked the build log for errors/warnings
  in the touched TUs).

# Mod Hardening, Part 4 — Permission Manifest Schema + Approval Flow
*(Elfie, 2026-10-11 — engine item 7 of the mod-hardening work)*

Prior parts: items 1–2 (Worker A: script binding allowlist, per-mod
CPU/memory quotas, SDK enforcement), items 3–4 (Worker B: `ModSaveRegistry`,
`ModNetGate`, `kPermNet`), items 5–6 (Worker C: UI confinement/badging,
input confinement — see part3_ui_input.md). File sandboxing / O_NOFOLLOW /
fd re-validation is a separate worker's; signed mods are future, out of scope.

## The `parsePermissions` "net" bug (fixed)

`ModLoader::parsePermissions` was a hand-written if/else chain that never
handled `"net"`, so Worker B's `kPermNet` (1<<8) could never be granted via
mod.json — a net-requesting mod silently got zero net permission. Fixed by
routing the parser through a new canonical name table
(`mods::permissionBitForName` in `mods/mod_permissions.h`), so a permission
name can never parse to a bit the manifest schema does not know. Unknown
names are still logged and ignored (fail-closed: they never become bits).

Regression tests: `testPermissionNetRegression` (mods_selftest.cpp) and the
extended `testModLoader` fixture (`"permissions":["world_read","tick","net"]`
now expects `kPermWorldRead|kPermTick|kPermNet`).

## 7a. Manifest schema

**New files** (LF, UTF-8, `litholattice::mods` / `litholattice` namespaces,
k-prefix constants, I-prefix untouched — no interface changes were needed)
- `src/mods/include/litholattice/mods/mod_permissions.h` — schema types,
  `PermissionRequest`, `PermissionStore`, `ModPermissionGate`,
  `ModLoadAction`, `denyAllPermissions`, the canonical name table, and the
  full schema/threat-model documentation in the header comment.
- `src/mods/mod_permissions.cpp` — implementation (wired into
  `litholattice_mods`).

**Changed**
- `src/mods/include/litholattice/mods/mod_loader.h` — `ModInfo` gains
  `permissionSetVersion`, `permissionRationales` (name→WHY),
  `criticalPermissions`, `firstPartyClaim`, plus the approval-flow output
  fields `grantedPermissions` and `loadAction`. New `load()` overload
  `(dir, store, gate, prompt)`; plain `load(dir)` keeps its signature and
  applies the headless default. Top-of-file mod.json example updated.
- `src/mods/mod_loader.cpp` — `"net"` fix (above); `parseModJson` parses
  `permissionSet`, `permissionRationales`, `criticalPermissions` (masked to
  requested bits; extras logged and dropped), `firstParty`; `load()` runs
  the gate before the mod's `onLoad` and fills `grantedPermissions` /
  `loadAction`; refused mods are skipped, grants are saved.
- `src/mods/CMakeLists.txt` — adds `mod_permissions.cpp`.
- Selftests: `src/mods/mods_selftest.cpp` (6 new test functions, 29 checks),
  `tests/engine_tests.cpp` (`testModLoader` fixture extended for the net
  regression).

### mod.json permission manifest

```jsonc
{
  "id": "mymod", "version": "1.0.0", "sdk": "1", "type": "script",
  "permissionSet": 1,                       // which permission-set version
                                            // this manifest targets
  "permissions": ["blocks", "world_read", "net"],  // requested (now incl. "net")
  "permissionRationales": {                 // per-permission WHY, shown at install
    "blocks":     "Adds 40 new building blocks",
    "world_read": "Scans nearby ore for the vein HUD",
    "net":        "Syncs waypoints via the engine-gated channel only"
  },
  "criticalPermissions": ["world_write"],   // denied here -> load REFUSED
  "firstParty": false                       // CLAIM only; engine verifies
}
```

Schema rules (fail-closed, all pinned by tests):
- Unknown names in `permissions` are logged and ignored — never bits.
- A permission without a rationale parses fine; the prompt shows
  `"(no rationale given)"` so thin manifests are visible, not fatal.
- `criticalPermissions` is masked to the requested set.
- `firstParty: true` is a claim, never a grant (see below).

### The name table

`permissionBitForName` / `permissionNameForBit` are the single source of
truth for manifest permission names: blocks, world_read, world_write, tick,
events, ui, input, assets, net. `kAllPermissionBits` is their union;
`kContentSafePermissions = kPermBlocks | kPermAssets`.

## 7b. Approval flow

`ModPermissionGate::resolve(mod, store, prompt)` — the only entry point the
loader needs:

1. **Nothing requested** → `kLoad`, zero mask, no prompt.
2. **First-party verification.** Manifest claim + id on the gate's trusted
   list (game-configured via `setFirstPartyIds`, e.g. `{"lithokraft"}`) →
   grant all, logged, prompt not consulted. Claim without engine
   confirmation → logged as an error, third-party treatment. No silent
   privilege inheritance: a first-party parent never makes extensions
   first-party (the IExtensible worker's rule, untouched). The runtime C
   facet's `isFirstParty()` remains the authority for native mods at
   runtime; the manifest path covers install-time resolution.
3. **Store lookup + diff.** The prompt sees ONLY new bits: bits in the
   manifest that are not in the stored request snapshot (mod update asking
   for more), or — when the permission set moved — bits not already
   approved. Explicit denials persist across updates. Content mods
   auto-approve `kContentSafePermissions` with no prompt; every other bit,
   for any mod type, goes through the prompt path. The prompt's return is
   masked to the asked bits.
4. **Verdict.** Denied load-critical bit → `kRefuse` (logged, mod skipped).
   Other denied bits → `kLoadStripped` (logged, mod runs reduced).
   Everything granted → `kLoad`.

**UI-agnosticism:** the prompt receives a `PermissionRequest`
(mod id/version/type, set version, full requested mask, already-granted
mask, and one item per undecided permission with its rationale and a
`previouslyDenied` flag). The game renders it however it likes; the
headless default `denyAllPermissions` approves nothing.

**Headless-safe default: DENY unless approved.** `load(dir)` uses the
deny-all prompt and a store at `<dir>/permission_grants.json`. An unapproved
mod never reaches `onLoad` with ungranted permissions: the effective mask
rides on `ModInfo::grantedPermissions`, and the (builder-side) API binder
must gate every binding on it.

## 7c. Persistence

`PermissionStore` — one JSON file mapping mod id → `{version,
permissionSet, requested[], granted[], denied[]}` (names, not raw bits).
Atomic write (`writeFileAtomic`, temp + fsync + rename, no backup rotation —
grants are re-promptable derived state). Missing file = fresh store; corrupt
file = logged, treated as empty, never fatal. Re-prompt triggers: (a) mod
update requesting NEW bits (diff vs. stored request snapshot); (b)
permission-set version change (re-review of not-yet-approved bits).
Denials persist across updates; dropping a permission needs no prompt.

## Policy decisions (for JD / the builder)

1. **Deny-unless-approved, strip by default, refuse only when declared
   critical.** Headless safety beats convenience: a mod that did not get an
   explicit yes runs reduced. Refusal is opt-in per permission via
   `criticalPermissions` — the author declares hard requirements, the
   engine enforces them, the user sees the reason.
2. **Content-safe subset = {blocks, assets}.** Content mods stay silent for
   pure data; `world_read` and up go through the prompt even for content
   mods. `needsPermissionPrompt()` is unchanged.
3. **Denials are sticky across updates.** Re-prompting a user for a bit they
   already denied on every mod update would train them to click through.
   Only genuinely new bits re-prompt.
4. **First-party = verified claim, never inherited.** The manifest cannot
   self-elevate; the engine's trusted-id list is the grant. The C facet's
   `isFirstParty()` stays the runtime authority for native mods.
5. **No `LithoLatticeModApi` changes.** Enforcement is engine-side, so the
   Worker B/C struct-size audit pins still hold untouched.

## Verification

- New: 29 checks across `testPermissionNetRegression`,
  `testPermissionManifestParse`, `testPermissionApprovalFlow` (7
  sub-scenarios: headless deny, approving prompt + rationale delivery,
  critical refusal, content auto-approve, verified first-party, unverified
  claim, greedy-prompt masking), `testPermissionUpdateRepompt`,
  `testPermissionStoreRoundTrip` (incl. corrupt-file handling),
  `testPermissionLoadWiring` — all pass.
- `litholattice_tests mods`: ENGINE TESTS OK (179 PASS lines, 0 FAIL).
- `ctest`: 12/16 pass. The 4 failures are **not mine**: `engine_render`
  (mesher golden hash — the in-flight PBR worker), `engine_sim` (block-names
  narrowing — grandfathered, builder's), `lithokraft_selftest` (SIGABRT —
  grandfathered, builder's), `lithokraft_headless_harness` (SEGFAULT — the
  known coordinator-collision broken tree; nothing of mine in game/).
- Build: clean, no new warnings (only the pre-existing `block.h` narrowing
  warning). New files LF.

## Open questions / follow-ups

1. **API binder (builder).** Still no `LithoLatticeModApi` function pointer
   is bound (all items). The binder must gate every binding on
   `ModInfo::grantedPermissions` — recipe: check the bit for the entry point
   (`kPermUI` before `registerUiPanel`, `kPermNet` before `netSend`, etc.),
   refuse/deny the call when the bit is absent.
2. **Trusted first-party list (game).** The gate defaults to an empty list
   (deny-by-default). The game must call `setFirstPartyIds` (e.g.
   `{"lithokraft"}`) or first-party mods will be prompted like third-party
   ones. Also decide whether `firstParty` mods should be *shown* in the
   prompt UI as pre-approved for transparency.
3. **Native-mod honest scope.** As Workers B/C documented: for in-process
   native mods the API surface is the control; OS-level sandboxing is future
   work. The permission gate is meaningful enforcement for content/script
   mods and the trust UI for all mods.
4. **Prompt UX copy.** The game owns the dialog; suggested minimum per item:
   permission name, the author's rationale (or "(no rationale given)"),
   and whether it was denied before.
5. **Permission-set v2.** When new permissions are added, bump
   `kPermissionSetVersion`; manifests declaring an older set keep working,
   and the gate re-reviews unapproved bits automatically.

## In-flight collision note (2026-10-11)

The IExtensible worker has in-flight edits in `src/mods/`. I did not touch
their files (`extension_*`, `extensible_interface.h`); I edited only
`mod_loader.h` / `mod_loader.cpp` / `src/mods/CMakeLists.txt` (my scope —
their additions there were preserved) plus my two new files and the
selftests. Their earlier broken link was fixed before I built; the tree
builds clean and `engine_mods` passes as of this writing.

## Program-level verification

- Full engine builds clean, no new warnings. New files LF; edited files kept their existing line endings.
- Targeted ctests green: `engine_script`, `engine_mods`, `engine_ui`, `engine_input`, `engine_net` all pass (5/5 in the last full targeted run).
- Full `ctest`: 12/16 at last check. The 4 failures are not from this program: `engine_render` mesher golden (in-flight PBR worker's render edits), `engine_sim` block-names narrowing (grandfathered, builder's fix), `lithokraft_selftest` SIGABRT (grandfathered, builder's fix — blocks.cpp 40-vs-36 guard), `lithokraft_headless_harness` SEGFAULT (known coordinator-collision broken tree, game-side).
- `trace_jit_basic.cpp` untouched throughout.

## Open questions for JD (consolidated)

1. Accept the `std::string` memory-quota blind spot for v1 or close it now?
2. Keep `ui_*` in the builtin allowlist table or move to runtime registration?
3. Suspension recovery: manual-admin or auto on reload?
4. Positive engine marking on real system dialogs (complement to mod badging)?

## Builder handoff notes

- The `LithoLatticeModApi` pointer is not yet bound anywhere — the binder must gate every binding on `ModInfo::grantedPermissions` (recipes documented in parts 3–4).
- Game-side wiring still needed: `ModSaveRegistry::loadAll()` before mods' `onWorldStart`, `save()` on world save, `netSend` binding through `ConfinedChannelMux`, inbound routing to `onNetMessage`, mod registration with `ModNetGate`, `setFirstPartyIds({"lithokraft"})` or first-party mods get prompted like third-party ones.
- Budget tuning (CPU/memory/net) via playtesting; suspension UX currently sticky.
