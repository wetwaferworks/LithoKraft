# Net + Reconciliation — Final Integration Report (2026-10-11)

Final integration coordination for the LithoLattice engine (+ LithoKraft game).
All program coordinators had landed; this pass reconciled the tree, finished
the interrupted net work, and landed JD's LithoScript hardening requirements.

## Phase 1 — Reconciliation pass

**Baseline:** full `make` exit 0 on first attempt; `ctest` **15/17** with
exactly the 2 grandfathered failures:
- `engine_sim` — block-names narrowing (builder's one-liner fix)
- `lithokraft_selftest` — SIGABRT on the blocks.cpp 40-vs-36 guard (builder's fix)

**Collisions found and fixed:** essentially none in the final state. The
night's concurrent coordinators (PBR, IExtensible, mod hardening, debug
console, file hardening, sim followups, codecs, headless harness, menu) left
the tree intact — their individual final passes each verified green, and the
one previously reported collision (the debug-console workstream's mid-flight
`debug_console.h` referencing nonexistent `ui::TextEditor`/`ui::TextBox`) was
resolved by that crew before landing. `trace_jit_basic.cpp` and the
blocks.cpp 40-vs-36 guard were untouched throughout, per constraints.

**Result:** green tree, no reconciliation surgery required. The planned
"fix integration collisions" step was a no-op — the coordinators' discipline
held.

## Phase 2 — Net resume (completed)

The net coordinator landed reliable+unreliable channels (23/23 checks,
14-byte header unchanged, channel in a flags bit); the versioning-handshake
and encryption-stub workers were interrupted by a tree collision. Both are
now finished. Channels code verified intact via green `engine_net` and built
on, not redone.

### 2a. Semantic version negotiation at connect handshake

`ClientHello`/`ServerHello` now carry the SDK major line (`u8 sdkVersion`)
alongside the engine semver (ClientHello 6+n bytes, ServerHello 6 bytes).
The net layer treats it as opaque — no new mods dependency; the game
supplies it via `HandshakeConfig::sdkVersion`.

Locked policy (documented in `handshake.h`, pinned by tests):
1. **Engine semver: major must match exactly** (minor/patch drift within a major is fine).
2. **SDK line: exact match, NOT negotiable** — new `RejectReason::IncompatibleSdkMajor`. A different SDK line may frame mod traffic differently, and the mod.json SDK-range check is a load-time gate, not a wire gate.
3. **Protocol: highest mutually compatible wins** (same-major = equality for single-integer protocol versions).

Subset offers downgrade gracefully to the highest common version; refusal only
on empty intersection, failed gates, or an agreed version selecting an
unimplemented codec. Fail-closed throughout: `kSdkVersionUnset` sentinel
refuses before any comparison; malformed hellos, unoffered versions, and
late traffic in terminal states are refused; timeouts fail clean, never hang.
Runtime exposure: `runtimeEngineVersion()`, `runtimeProtocolVersion()`,
`sdkCompatible()`.

### 2b. Encryption stubs + builder notes (new `net/encryption.h/.cpp`)

No real crypto — stubs + notes by design. Plaintext is the deliberate v1
state, gated behind the stub, never silent.

- `ICipher` interface (seal/open → `CryptoError`, `isPlaintext()` gate);
  `NullCipher` returns `NotImplemented` and never copies plaintext through;
  `CryptoSession` with `requireEncryption` fail-closed flag; `KeyExchangeMsg`
  (msg 4, size-first ephemeral key per the versioned-struct rule — shape
  implemented, key math stubbed in `deriveKeys()`).
- Real bookkeeping, not crypto: `makeNonce()` (sessionId||counter, implicit
  on wire) and `ReplayWindow` (64-bit sliding window, runs before open()).
- Overhead analysis: **ChaCha20-Poly1305 over AES-GCM** for this packet
  profile (no HW dependency, constant-time portable software, ARM-friendly —
  the primary dev machine is a mid-range phone). 16 B/pkt on the wire (tag
  only), inside the 28 B never-exceed budget (`kCryptoMaxOverheadBytes`).
- Builder notes: vendor **libsodium** first (X25519 + ChaCha20-Poly1305 +
  Ed25519 for the future signed-mods plan), monocypher as lean alt;
  KEX plugs in post-`Agreed` pre-game-traffic on the reliable channel;
  seal/open around ChannelMux/Demux, 14-byte header stays plaintext.

### Net tests

`engine_net` fully green: the 23 landed checks plus **12 new** (SDK
round-trips, both-direction SDK rejects, unset fail-closed, subset
downgrades, runtime accessors, crypto budget, NullCipher/session semantics,
nonce, replay matrix, KEX shape). Full ctest unchanged at 15/17.

## LithoScript hardening (JD's requirements, landed in this pass)

1. **Official name**: LithoScript on all user-facing references (banner,
   `exec` help, version string, archived-cvar header, console placeholder).
   API identifiers (`ConsoleRegistry`, `kPermConsole`, cvar API) kept stable
   for the builder — documented.
2. **Hardening**: alias recursion cap 16 kept; new **`kMaxExecDepth = 8`**
   exec-chain depth cap, fail-closed with a clear error (mirrors the
   IExtensible depth motif); `wait` **omitted in v1** — tree-wide search
   proved none existed, and a guard test prevents reintroduction; trust
   model documented in `concmd.h` (.lth files carry no authority, run with
   the invoking context's permissions, deny-by-default unchanged).
3. **`.lth` extension** wired through `exec` (`.cfg` kept as legacy alias,
   other extensions refused). No file dialogs reference script extensions
   (verified — nothing to wire).
4. **Debug-console hookup verified end-to-end**: test drives
   `DebugConsole::submitForTest` (input box → onSubmit → executeLine),
   executing a command, set/reading a cvar, expanding an alias. PASS.
5. **Fragment seam (design only)**: extended `concmd.h` seam note around
   JD's **ReadOR** — Fragment → ReadOR → keyboard → contraptions, plus the
   **serialization line** (Fragment as script cartridge AND data exporter
   into player-built memory). References
   [ReadOR-Design-Note-2026-10-11.md](sandbox://workspace/user/files/ReadOR-Design-Note-2026-10-11.md)
   by path, not duplicated. Builder questions recorded: keyboard as
   script-input component; agent/proxy script permission model (agent scripts
   run with the agent's permissions, never the player's); multiplayer
   authority (owner's? placer's? reader's?) so slotted Fragments can't launder
   privileges.

Console self-tests: **22/22 PASS** (18 existing + 4 new).

## Final verification

- Full `make`: exit 0, no new warnings.
- Full `ctest`: **15/17** — 15 pass, 2 fail, both grandfathered and builder-owned.
- Headless harness ctest: PASS (re-verified green post-reconciliation).

## Open questions / builder items

- Net: vendor libsodium (or monocypher), implement `ICipher` + `deriveKeys()`
  (ephemeral X25519), flip `requireEncryption`; game-side: pass
  `LITHOLATTICE_SDK_VERSION` into `HandshakeConfig`, run KEX after `Agreed`,
  surface `isPlaintext()` in the session log/server browser, decide the
  v1-peer story (narrow offer list to {2} or plug a legacy v1 codec).
- `bind` dispatch: registry seam only — flagged follow-up.
- LithoScript: `ModLoader::setModId()` wiring; confirm probation halves CPU
  only; confirm decay starts at re-admission; tilde-hold cycling fix.
- Grandfathered builder fixes: blocks.cpp 40-vs-36 guard, block-names
  narrowing one-liner.
