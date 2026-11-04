# Fragment Markdown Feature — Implementation Report (2026-10-11)

Authorized by JD 2026-10-11 (~21:15). Three workstreams, all landed green. Full build clean; `ctest -E "engine_sim|lithokraft_selftest"` → **27/27 passed** (the 2 grandfathered failures untouched).

## Workstream 1 — Block comments in the LithoScript tokenizer

`/* */` multi-line block-comment support added to `ConsoleRegistry::tokenize`, Quake/HL1-faithful otherwise.

**Files changed:**
- `engine/src/console/include/litholattice/console/concmd.h` — new `tokenize(text, err)` overload + `extractComments(text, err)` declarations with doc comments.
- `engine/src/console/concmd.cpp` — tokenizer state machine rewritten with an `inBlock` state: `/*` opens, first `*/` closes; `//` unchanged; `//` inside `/* */` is comment text; markers inside double-quoted strings or after backslash escapes are literal. Unterminated `/*` → `*err = "unterminated block comment"`, empty result (fail-closed, never silently truncates). `executeFile` (exec path) strips block comments across whole file contents before line-splitting, so blocks span lines in `.lth` scripts; unterminated refuses the file via `print("exec: refused '<name>': unterminated block comment")`.
- `engine/src/console/console_selftest.cpp` — 4 new check sections.

**Tests:** `./tests/litholattice_tests console` → **81 [PASS], 0 [FAIL]**. New: mid-line strip (`;` inside inert), string/escape/`//` protection, unterminated error on both overloads, `extractComments` mixed/multi-line input, exec-path multi-line block across lines in a real `.lth` file. Diligence sweep: no repo `.lth`/`.cfg` contains `/*`, so no existing script changes meaning.

## Workstream 2 — Fragment markdown renderer

Markdown-in-comments pipeline producing a structured `RenderedDoc` (not HTML) for the future Fragment UI to bind to.

**Files added:**
- `game/LithoKraft/src/game/include/lithokraft/game/fragment_markdown.h` — public API (`FragmentView { Source, Rendered }`, `BlockKind`, `InlineKind`, `RenderedDoc`, `FragmentRenderResult`, `renderFragment`, `toggleChecklistItem`, `extractMarkdownSource`).
- `game/LithoKraft/src/game/fragment_markdown.cpp` — uses `ConsoleRegistry::extractComments` verbatim (no reimplementation); comment bodies joined with `\n` become the markdown source.
- `game/LithoKraft/src/game/include/lithokraft/game/fragment_markdown_selftest.h`, `game/LithoKraft/src/game/fragment_markdown_selftest.cpp`, `game/LithoKraft/src/app/fragment_markdown_selftest_main.cpp` — 27 checks, own ctest entry `lithokraft_markdown_selftest`.
- `game/LithoKraft/src/game/CMakeLists.txt`, `game/LithoKraft/src/app/CMakeLists.txt` — wired in (same pattern as the silica/fragment selftests).

**Supported (tight and correct):** ATX headings 1–6, bullet lists, numbered lists (start number kept), checklists (`[ ]`/`[x]`), fenced code blocks (unclosed fence → CodeBlock to end, documented), `---` rules, single-level blockquotes, inline code, `**bold**`, `*`/`_` italic (word-boundary rule), `[text](url)` links, backslash escapes. **Deliberately skipped:** nested lists (flattened), multi-level quotes, setext headings, tables, images, reference links, footnotes, strikethrough, HTML.

**Behavior:** `renderFragment(text, markdownEnabled, view)` — `Source` returns raw lines incl. markers; `Rendered`+enabled returns the parsed doc; `Rendered`+disabled renders each comment line as a plain paragraph (no elements). `toggleChecklistItem(doc, index)` flips checklist state (flattened doc-order index).

**Tests:** 27/27 PASS.

## Workstream 3 — Spark-gated markdown unlock

`markdown_unlocked` flag with crash-safe persistence; the seam where the future Spark-upgrade event hooks in.

**Files added:**
- `game/LithoKraft/src/game/include/lithokraft/game/fragment_state.h` — `lithokraft::FragmentState`: `markdown_unlocked()` (default false), `upgradeFragment()` (idempotent; the single general upgrade entry point — currently applies the markdown unlock; the header documents it as the seam "called by the Spark's Fragment-upgrade event (progression, TBD)" and by any future upgrade event), `serialize()`/`deserialize()` (bool+err-string, object unchanged on failure).
- `game/LithoKraft/src/game/fragment_state.cpp`, `game/LithoKraft/src/game/include/lithokraft/game/fragment_selftest.h`, `game/LithoKraft/src/game/fragment_selftest.cpp`, `game/LithoKraft/src/app/fragment_selftest_main.cpp` — 24 checks, own ctest entry `lithokraft_fragment_selftest`.
- `game/LithoKraft/src/game/CMakeLists.txt`, `game/LithoKraft/src/app/CMakeLists.txt` — wired in.

**Serialized format:** exactly 8 bytes — `"LFS1"` magic + u16 version=2 (LE) + u8 flags (bit 0 = markdown unlocked) + u8 tier. Bad magic / wrong version (v1 rejected — never shipped) / any truncation / trailing bytes → false + err, object unchanged.

**Tests:** 37/37 PASS — state machine (locked→unlocked, tier 0→1, double-upgrade no-op), exact-blob round-trips (v2 8-byte format), malformed-input battery (all 8 truncation lengths, v1 rejected), `simulateSparkUpgrade(FragmentState&)` test hook driving locked→unlocked through a save/load cycle, and the tier suite (tier starts 0, +1 per new upgrade only, synthetic prior-upgrade → tier 2, persistence, fail-closed).

## Build evidence (final verification, coordinator)
- `cmake --build . -j$(nproc)` in `~/workspace/litholattice/build-linux` → clean, exit 0.
- `ctest -E "engine_sim|lithokraft_selftest"` → **27/27 passed, 0 failed** (79s), including `engine_console`, `lithokraft_markdown_selftest`, `lithokraft_fragment_selftest`.
- Grandfathered `engine_sim` / `lithokraft_selftest` not run beyond exclusion; changes are purely additive to `litholattice_console` (behavior-preserving for valid input) and new game files.
- Follow-up verification (2026-10-11, rename + tiers + delimiter hardening): full rebuild clean; `./tests/litholattice_tests console` → ENGINE TESTS OK (incl. 2 new fail-condition checks); `./lithokraft_fragment_selftest` → 37/37 PASS; `ctest -E "engine_sim|lithokraft_selftest"` → 27/27 passed.

## Open items
1. **Spark-upgrade wiring** — `FragmentState::upgradeFragment()` is the documented seam; the Spark character/progression event doesn't exist in code yet. Nothing to integrate until it does.
2. **Checklist checked-state persistence** — `toggleChecklistItem` mutates the in-memory doc; the save system must serialize the doc/diff later (documented in the header).
3. **Fragment UI binding** — `RenderedDoc` is the model; no Fragment UI exists yet.
4. **Markdown subset** — nested lists, tables, images etc. deliberately skipped; easy to add if JD wants them.

## Decisions JD should lock
1. **Unterminated `/*` fails closed** (empty token list + `"unterminated block comment"`; exec refuses the whole file). Alternative was best-effort recovery — we chose fail-closed per the no-nonsense doctrine.
2. **`extractComments` on unterminated `/*` returns comments extracted so far** (with err set) rather than nothing — so the Fragment still renders what it can.
3. **Unclosed fenced code block runs to end of input** (documented CommonMark rule).
4. **`markdownEnabled=false` renders comment bodies as plain paragraphs** (visible but inert) rather than hiding them — JD can flip this to hidden if preferred.
5. **LFS1 8-byte format** (magic + u16 version=2 + u8 flags + u8 tier) for `FragmentState` — versioned for future Fragment fields. **Fragment tiers** (JD 2026-10-11): the Fragment has a persisted tier (starts 0, displayed on the Fragment); `upgradeFragment()` increments it once per newly applied upgrade and never on a no-op repeat call.

## Follow-ups (2026-10-11, post-report)

- **Rename** (JD): `unlockMarkdown()` → `upgradeFragment()` — the entry point is general (applies all known upgrades), not markdown-specific. Done; reflected in workstream 3 above.
- **Fragment tiers** (JD): persisted `tier` field (starts 0, displayed on the Fragment); `upgradeFragment()` bumps it once per newly applied upgrade, never on no-op repeats; LFS1 bumped to v2 8-byte format. Done; reflected in workstream 3 and decision 5 above.
- **Delimiter pairing hardening** (JD): `engine/src/console/delimiter_pairing.{h,cpp}` gained fail-closed degenerate-input guards — a 16 MiB input cap on all four public functions (`scanEndState` reports Normal; `autoCloseOnType`/`trySkipOverCloser` refuse with applied/skipped=false; `balancePastedDelimiters` returns input byte-identical). New unit tests cover empty inputs and absurd sizes. (VS Code-style skip-over landed separately in the same unit.)
- **Fragment editor autocomplete** (JD): typing `/*` auto-inserts `*/` with the cursor inside; pasting lone delimiters pairs them bidirectionally (`/*` → append `*/`, `*/` → prepend `/*`), context-aware. Implemented as the reusable `delimiter_pairing` unit for the future Fragment editor.
