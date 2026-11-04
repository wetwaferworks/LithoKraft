# IExtensible Implementation Report — 2026-10-11

Mods-inside-mods recursion as a first-class engine feature. Terminology locked by JD: **"mod"** = top-level IMod package; **"extension"** = sub-mod hosted inside an IExtensible parent.

## Status: all three workstreams DONE ✅

### Worker 1 — Interface, registry, identity, depth cap
- `extensible_interface.h` (C-ABI, same rules as `mod_interface.h`): `LithoLatticeExtensible` facet (size-first), `LithoLatticeExtensionDecl` (id, versionRange, budgetShare 0–100), `LithoLatticeExtensionApi` (parent's child-facing API surface — deliberately small v1), `LithoLatticeExtensibleQuery()` helper (null query → null facet = "not extensible"), DLL-era symbol `LithoLattice_QueryExtensible`.
- mod.json schema documented: `"extensible": true`, `"extensions": [{"id": "...", "version": ">=1.0.0", "budgetShare": 25}]`.
- `ExtensionRegistry`: dotted identity paths (`lithokraft.worldedit.brushes`), `registerMod`/`registerExtension`, `find`/`depthOf`/`childrenOf`, `unregister()`, static path validators. Depth: top-level mod = 0.
- Depth cap: `kDefaultMaxExtensionDepth = 8` + runtime `setMaxExtensionDepth()` (0 = reset). Fail-closed refusal, never truncation.
- `litholattice_mods` converted to compiled static lib (coordinated with sibling hardening agent — reused its shape, no reconvert).

### Worker 2 — Recursive loader, scoped contexts, privilege, cascade unload
- `ExtensionLoader`: recursive load from facet declarations (v1 static linking, resolve by mod id); depth cap enforced with full rollback on refusal; cycle detection (self-declaration and A→B→A rejected); version-range checks (`*`, `=`, `>=`, `^`); budgetShare > 100 refused.
- `ExtensionContext`: carries ONLY the parent's filtered `childApi` — no full engine API, no sibling handles. Structural audit in tests guards the shape.
- Privilege default-deny: parent's `isFirstParty()==1` NEVER propagates; explicit per-extension grants via facet v1.1 tail (`LithoLatticeExtensibleGrantsFirstParty()`, size-gated, no wildcards, not transitive).
- Cascade unload: deepest-first, `onUnload` hooks fire, registry/contexts cleaned. Save-data policy: keyed by dotted path, persists on disk, never deleted on unload (orphaned if the extension never returns).
- mod.json `extensible`/`extensions` parsing wired into `ModLoader::parseModJson` (facet remains authoritative for v1).

### Worker 3 — Hierarchical budgets, crash isolation, LithoKraft game-side
- `ExtensionBudget`/`ExtensionRuntime`: per-node CPU quota (1024µs default, matching the script budget) + 64MiB native memory default; overrun → throttled (skipped ticks) → suspended with logged reason; siblings independent. Extension share carved from parent quota (parent-declares/engine-enforces). Sibling shares summing >100: warn-and-clamp proportional (documented default — JD can override to fail-closed).
- Crash isolation PROVEN: deliberately crashing test extension → contained (`kCrashed`), its `onUnload` ran exactly once, subtree unloaded, parent + healthy sibling kept ticking. Honest boundary: v1 catches C++ exceptions across the boundary; SIGSEGV/abort needs the DLL-era process boundary.
- LithoKraft implements IExtensible: `mod.json` gains `"extensible": true`; new `game/LithoKraft/src/game/extensible_facet.cpp` exports `LithoLattice_QueryExtensible` (0 extensions declared initially — the game is now a host). Builder seam flagged: engine `ExtensionLoader` not yet wired into the game's mod boot path (`main.cpp` only reads mod.json today).

## Verification (first-hand, 2026-10-11 ~11:00 UTC)
- Full build: clean, exit 0.
- `litholattice_tests mods`: ENGINE TESTS OK (179 checks).
- `ctest`: 13/16 — the 3 failures are exactly the grandfathered set (engine_render mesher golden hash, engine_sim block-names narrowing, lithokraft_selftest SIGABRT from blocks.cpp 40-vs-36). **No new failures.** (`engine_script` flaked once on a sibling's quota-tuning test, passed on both re-runs — unrelated to this work.)
- `trace_jit_basic.cpp` untouched.

## JD defaults — built as specified, flaggable for override
1. Discovery: parent declares extensions in mod.json (declarative).
2. Sibling communication: isolated — extensions talk only through the parent.
3. Privilege: extensions third-party by default; no silent first-party inheritance.
4. Depth cap: default 8, user-configurable, not hardcoded.
5. Budgets: parent declares share, engine enforces. Share-sum overflow: warn-and-clamp (worker 3's call — alternative was fail-closed refusal).

## Open questions
- mod.json↔facet merge strategy for the DLL era.
- Crash-mark persistence semantics (marks survive unload, clear on reload — confirm desired).
- Native 64MiB vs script 1MiB memory defaults — hardening agent may want to unify.
- Wire `ExtensionLoader` into the game's mod boot path (builder).
- Re-run ctest after all concurrent sibling agents land (tree was mid-churn throughout).
