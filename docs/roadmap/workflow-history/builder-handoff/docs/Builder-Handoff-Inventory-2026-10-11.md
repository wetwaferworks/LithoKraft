# Builder Handoff — Inventory of Builder-Owned Items
**Assembled 2026-10-11 ~09:55 EDT (research prep for the handoff-package task JD asked Elfie to prepare; approval pending).**
Scope note: the proposal said "eight reports" — the actual corpus is **16 implementation reports + 1 design note + 1 comparison report** (below). Nothing here is invented; every item cites its report section. This is read-only prep; nothing in the tree was touched.

## Report corpus (all in `~/workspace/user/files/`)

| # | Report | Workstream state |
|---|--------|------------------|
| 1 | Optimization-Implementation-Report-2026-10-10.md | Six optimizations LANDED, tree green |
| 2 | PBR-Implementation-Report-2026-10-10.md | Phase 1 LANDED |
| 3 | Audio-Threading-Implementation-Report-2026-10-11.md | LANDED (build clean, tests pass) |
| 4 | LithoKraft-Menu-Implementation-Report-2026-10-11.md | LANDED; game can't start (finding 1) |
| 5 | PBR-Phase2-Implementation-Report-2026-10-11.md | LANDED (ctest 15/17, 2 grandfathered fails) |
| 6 | Codec-Stubs-Report-2026-10-11.md | Stubs LANDED; flesh-out is builder |
| 7 | Sim-Followups-Report-2026-10-11.md | LANDED; SimThread integration DEFERRED to builder |
| 8 | File-Hardening-Report-2026-10-11.md | LANDED (103/103 checks); Windows notes are builder |
| 9 | Headless-Harness-Report-2026-10-11.md | Code complete; re-verify after tree reconciliation |
| 10 | Mod-Hardening-Report-2026-10-11.md | Program LANDED; **API binder + game wiring are builder** |
| 11 | IExtensible-Report-2026-10-11.md | LANDED; ExtensionLoader boot wiring is builder |
| 12 | Console-Implementation-Report-2026-10-11.md | Landed items + builder follow-ups listed |
| 13 | Net-Reconciliation-Report-2026-10-11.md | LANDED (35 checks green); encryption impl is builder |
| 14 | LithoScript-Detailed-Report-2026-10-11.md | Machinery complete; **surface empty — populating it is builder** |
| 15 | MCP-Research-Report-2026-10-11.md | Research done; implementation queued FIFO behind console work |
| 16 | Debug-Console-Report-2026-10-11.md | Program LANDED (+ defrag service landed, appended) |
| — | ReadOR-Design-Note-2026-10-11.md | **Design only, not built** — ReadOR/keyboard/Fragment pipeline |
| — | HL1-vs-LithoScript-Comparison-2026-10-11.md | 10 decision points (D1, D9, D10 locked; D6/D8 superseded by later locks) |

## Builder-owned items (ordered by dependency)

### B1. blocks.cpp content-table desync — PRE-EXISTING BLOCKER
- `blocks.cpp` registers 40 rows (39 + Air) but the guard expects `kOverclockor + 1` = 36 → `std::abort()`. Grandfathered `lithokraft_selftest` SIGABRT. Additionally LithographOR/ChipOR rows are commented out, so PortOR/Fusor rows mismatch their `BlockId` constants.
- Fix: re-enable/reconcile the -OR rows, bump the guard constant. (Menu report §Findings-1; also noted in Console §98, Debug-Console §93, Net-Reconciliation §open.)
- Verify: `lithokraft_selftest` no longer SIGABRTs; game starts.

### B2. SimThread integration (deferred by JD's explicit decision)
- `DeterministicScheduler` (conflict-DAG stages over JobPool) runs standalone; `SimThread` runs its own fixed-timestep loop calling `IWorldSimulation` directly — and **has NO try/catch on the tick path** (must wrap per-tick step).
- Builder must design holistically against the real tick-loop contract: scheduler as execution engine inside `SimThread::run`, clock-domain mapping, structural commands through per-system buffers, reconcile scheduler fail-fast throw with sim-thread lifecycle. (Sim-Followups §4, §3 exception-boundary finding; Trace JIT seam wrapped with reasoning notes for the builder — TraceJitSystem, 6/6 tests.)

### B3. Net encryption implementation
- Vendor libsodium (or monocypher); implement `ICipher` + `deriveKeys()` (ephemeral X25519); flip `requireEncryption`. ChaCha20-Poly1305 chosen, 16 B/pkt inside the 28 B budget.
- Game-side: pass `LITHOLATTICE_SDK_VERSION` into `HandshakeConfig`; run KEX after `Agreed`; surface `isPlaintext()` in session log/server browser; decide the v1-peer story (narrow offer list to {2} or plug a legacy v1 codec). (Net-Reconciliation §open.)

### B4. Mod API binder (nothing bound anywhere yet)
- No `LithoLatticeModApi` function pointer is bound (all items, not just the hardening program's). Binder must gate **every** binding on `ModInfo::grantedPermissions` — recipe documented in Mod-Hardening parts 3–4: check the bit for the entry point (`kPermUI` before `registerUiPanel`, `kPermNet` before `netSend`, `kPermConsole` before console registration), refuse/deny when absent. (Mod-Hardening §§420, 649.)

### B5. Game-side mod wiring
- Call `ModSaveRegistry::loadAll()` on world load (before mods' `onWorldStart`); `save()` on world save/autosave.
- Route addressed inbound datagrams to `onNetMessage`; register net-capable mods with `ModNetGate`; bind API `netSend` through `ConfinedChannelMux`.
- Call `setFirstPartyIds({"lithokraft"})` or first-party mods get prompted like third-party ones. (Mod-Hardening §Builder-handoff-notes.)

### B6. ExtensionLoader → game mod-boot-path wiring
- Wire `ExtensionLoader` into the game's mod boot path; mod.json↔facet merge strategy for the DLL era still open. (IExtensible §open.)

### B7. LithoScript cvar/command surface (the biggest surface gap)
- Verified by tree-wide grep: **zero `registerCVar`/`registerCommand` call sites outside the console module.** Machinery complete, surface empty. Populate engine/game settings: `render.pbr_tier`, `sim.tick_rate`, `net.snapshot_rate`, `audio.master_volume`, `ui.scale`, `mod.<modid>:<name>` (dotted naming LOCKED; categories proposed in comparison report). (LithoScript-Detailed §6.)
- `bind`/`unbind` dispatch: registry records/validates binds but nothing dispatches; input pump must check pressed bound keys per frame → `executeLine`. (Console §98; LithoScript-Detailed §6.)
- ModLoader wiring for `kPermConsole`: gate a `registerConsoleCommand` API on `grantedPermissions & kPermConsole`; namespace `"modid:..."`.
- Tilde-hold cycling fix (GraveAccent KeyDown mapping); `?` placeholder vs `help` reality; `autoexec.lth` + `config.lth` startup/shutdown conventions (D2).

### B8. PBR follow-ups
- Transparent-tier dropdown deliberately NOT wired (fake setting refused) — needs a real renderer-scoped `IRenderer::setTransparencyTier` path. (Menu report finding 2.)
- Tier 3 idle (no refractive content yet); Vulkan `vk/chunk_pbr` unwritten; software-renderer two-pass refraction open. (PBR-Phase2 §open.)

### B9. Codec flesh-out
- FLAC: vendor dr_flac (public domain/MIT-0, single header); Opus: libopus; Vorbis: stb_vorbis; no GPL. Implement frame decoding (LPC/Rice for FLAC etc.).
- Resampling ownership decision (recommendation in report: small resampler stage in the streaming worker, one place, all codecs benefit); disk vs memory streaming for large music files; game-side codec split (WAV for SFX, Opus for music — conventional). (Codec-Stubs §§39, 85.)

### B10. Windows file-hardening parity
- `O_NOFOLLOW` equivalent: `CreateFile` with `FILE_FLAG_OPEN_REPARSE_POINT` + explicit reparse-point rejection (or resolve against sandbox); fd re-check via `GetFinalPathNameByHandle`; regular-file check via `GetFileInformationByHandle`. Until implemented, non-Linux builds get resolve() + best-effort only (documented in `sandboxed_io.h`, not silently weakened). (File-Hardening §80.)

### B11. ReadOR / keyboard / Fragment script pipeline (design only)
- Builder design work, all open: (1) keyboard as script-input component — how it accepts lithoscript and routes it into machine logic; (2) permission/trust model — standing rule: agent/proxy scripts run with the *agent's* permissions, never the player's (SEMI-TRUST locked); (3) multiplayer authority for slotted Fragments — no privilege-laundering channel; (4) Fragment page serialization format (script vs data vs unlock log); (5) memory-chip write interface (DRAM/SDRAM/HDD). (ReadOR-Design-Note; Debug-Console §5.)
- Related seam-only item: `IConsoleDriver` interface so debug console, Fragment, and typewriter are three drivers over one registry. (LithoScript-Detailed §6.)

### B12. Debug-console wiring
- ModLoader → `ScriptEngine::setModId()` wiring (logs show `<unknown>` until then). (Debug-Console §93.)
- Game-target build breakage belongs to the console workstream: `debug_console.h` references `litholattice::ui::TextEditor`/`TextBox`, which don't exist yet. (PBR-Phase2 §Final-verification.)

### B13. Headless harness re-verification
- 23/29 checks at write time; the 6 failures were a test-helper bug, fixed in code — re-verify after tree reconciliation. Optional: `--bench` path coverage; hoist shared `NullRenderer` into `litholattice_render`; mesh-assertion flakiness note for CI. (Headless §open.)

### B14. MCP implementation (queued FIFO behind console work — NOT builder's yet, builder takes it later)
- Phased plan: P0 JSON-RPC core → P1 stdio+handshake+read surface → P2 Streamable HTTP → P3 stateless era+prompts → P4 write tools+hardening. Threading: own thread, reads from snapshots, writes queued to sim thread; fail-fast on malformed input.
- JD decisions still open: default HTTP port (provisional `18420`) + in-game agent-connection approval UX; which write tools are v1 vs v2; whether the frontier-powered Daemon consumes the same MCP surface (recommended: yes). (MCP-Research §§plan, 8.)

### B15. Grandfathered items (builder's baseline, unrelated to current programs)
- `engine_sim` block-names failure (`world/block.h:90` narrowing).
- `lithokraft_selftest` SIGABRT (= B1 above).

## Open questions for JD (consolidated)

**LithoScript/HL1 decisions** (Comparison report §3; statuses updated against JD's later locks):
- D2 (config conventions: `config.lth` auto-write + `autoexec.lth` at startup) — OPEN, recommended yes, small, builder-safe.
- D3 (cheat-gating model: sv_cheats analog vs permissions-only) — OPEN.
- D4 (`toggle` command) — OPEN, recommended add.
- D5 (key-bind dispatch ownership) — OPEN.
- D7 (`+`/`-` button commands) — OPEN.
- D1 (dotted naming) — LOCKED 2026-10-11 ~8:33am. D9 (give/map/save/kill first-party) — LOCKED. D10 (semi-trust, agent's permissions) — LOCKED. D6 (`wait`) — superseded: JD decided `wait N` INCLUDED, max 512 ticks, cooperative (8:07am), LANDED. D8 (cvar flags) — cheat/replicated/read-only flags LOCKED.
- Priority per report: D2 → D1(done) → D4+D5 together (the "binds feel real" HL1-feel milestone).

**Mod hardening** (Mod-Hardening §688; two resolved by JD since):
- Positive engine marking on real system dialogs (complement to mod badging) — OPEN.
- `ui_*` in builtin allowlist table vs runtime registration — OPEN.
- std::string quota blind spot — RESOLVED (fix it, no blind spots; LANDED). Suspension recovery — RESOLVED (AUTO with debug-console logging; LANDED).

**Files**: `(deleted)`-after-open fail-closed policy — add a policy flag? (default stays fail-closed). Backup disk-cost baseline at depth 8 for large region files — JD asked for it. Signed mods (esp. first-party) — later.

**Codecs**: resampling ownership; disk vs memory streaming; which codecs ship in base game data.

**Net**: v1-peer story (narrow offer list to {2} vs legacy v1 codec).

**Sim**: `kSignalNetworkRes` shared-constant home; PerNetwork canonical-id scheme; PerChunk boundary semantics.

## Process warnings for the builder (from the reports)
- **Tree-concurrency hazard**: parallel coordinators editing the same tree broke the build mid-flight twice (file-hardening worker repaired include cycles; PBR-Phase2 hit transient relink failures). Recommendation: serialize builder work or snapshot the tree during build week. (File-Hardening §99; PBR-Phase2 §Final-verification.)
- **Builder takes over ~Oct 12** when Grok/builder usage resets. In-flight now (Elfie's lane, NOT builder's): console-crew follow-ups; MCP implementation queued FIFO behind them.
- Do NOT re-guess locked design decisions: naming (`litholattice::`, k-prefix, -OR game components, Trace/Signal), 1-based indexing, 512 TPS sim / 32 Hz snapshots, fail-fast on sim corruption, AngelScript 1024µs budget, deny-by-default mod permissions.
