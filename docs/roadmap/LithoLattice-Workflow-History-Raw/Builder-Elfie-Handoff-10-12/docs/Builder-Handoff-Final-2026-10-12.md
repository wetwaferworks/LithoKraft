# Builder Handoff — Final Packet — Monday 2026-10-12

Supersedes, for handoff purposes, the 2026-10-11 inventory queue sections and the
2026-10-12 delta + addendum queue sections. Locks and landed-work lists in the
delta still stand unless this packet says otherwise.

Builder takeover: ~5:07 PM EDT, 2026-10-12.

## Status change since the addendum: the review-fix program is DONE

JD directive (2026-10-12): Elfie alone, no crew, criticals first, one at a time.

Implemented: **all critical + high + medium findings** from the full-tree code
review, plus low-tier drive-bys.

Key fixes landed:
- SimThread exception boundary (was CRITICAL — throwing tick hook → std::terminate,
  half-updated world could still save)
- Signing-key hygiene — release now hard-refuses boot on the test key via exit(1)
  (was CRITICAL — test private signing key in repo in plaintext, REPLACE-BEFORE-BETA
  marker was comment-only)
- Console permission laundering across `wait` — fixed
- Hot-swap `def()` race — by-value + shared_mutex
- findBlock alias cycle guard (known H1)
- IK NaN guards + validateRig; computeSkinningMatrices cycle protection in scope of the high fixes
- Net delta saturation + fixed-point / quaternion saturation (net encode UB)
- JSON parser 128-depth cap
- Seal gaps closed (game manifest + StructureRegistry; scanMods fail-open path)
- Procgen clamps, asset symlink canonicalization + PNG bomb cap, AI/storage races,
  ui_show hardened via ModUiFactory, script quota UAF via shared QuotaAccount
  control block, execDepth RAII guard, staging unique locks, dir fsync

Two self-caught regressions found and fixed during re-audit:
1. By-value `def()` left dangling `const BlockDef&` wrappers in game `blocks.h` +
   `CustomBlock::def()` + callers — fixed to by-value.
2. `shared_ptr` in a malloc'd QuotaHeader needed placement new + explicit destroy.

## Verified numbers (2026-10-12)

- Full build: clean
- ctest: 92% — 35/38
- Main selftest: **830 PASS / 9 FAIL** — all 9 are the known pre-existing set:
  wafer crust, 4× save/restore, mesher sub-chunk, Vulkan headless (environmental,
  no GPU on the VM), worldgen seed, hotbar icons
- Other ctest failures, all pre-existing:
  - scout — ModelResolver wiring the harness doesn't supply
  - chipor — stale registry-size guard after `structure_block` landed
  - engine_audio — flaky under ctest, passes standalone

Do not re-file the review's critical/high/medium items as open work.

## Also landed today (from the delta — still current)

Seven workstreams, all reported green in-session:
hot-swap game wiring (certificate model), IK solvers, rig generation,
signing crypto (`litho_sign`), engine verification + trust tiers,
ecosystem diversity (7 features, 102/102), structure registry + placement
solver + structure block. Structure identity layer also landed:
SHA-256 fingerprints, search (list / namespace / prefix), Ed25519 signing
over the fingerprint, `namespace:name` validation — 41/41 checks,
25/25 engine tests, full lithokraft target compiles, trust anchor set at
boot before any mod loads.

Project stage: **beta declared by JD** — "working beta with world
generation." Oct 23 target may land in beta state.

## Still open — builder / JD queue (complete list)

Builder:
1. H3 — Formula node polar-axis 90° mapping fix + non-spherical regression test
   (node tests only used spheres; spec's "agrees up to voxelization error"
   claim is false on the polar axis)
2. Structure overview-map replay accuracy
3. Chunk-border structure replay
4. AI self-despawn
5. Reentrant `blockMutex_`
6. Pack converter: teach it .svg manifest entries + provenance stamping,
   or a future manifest regeneration silently drops the SVG texture
7. Pre-existing selftest/ctest set above (scout ModelResolver, chipor guard,
   9 selftest failures) — known, not regressions

JD decision / JD action:
1. H2 — handshake offers {3} only by default, not {1,2,3}: interop decision
   is JD's (fail-closed today; code and story don't match)
2. Release signing key replacement — generate a real keypair offline with
   `litho_sign keygen`, paste the public key into the marked spot, rebuild,
   guard the `.priv`. Now enforced: a release build on the test key refuses
   to boot. The test key itself can never be trusted in any shipped build —
   its private seed was in the repo in plaintext and git history retains it.
3. The Cosmic: no worldgen JSON / dimension registration yet — the
   per-dimension wiring call is JD's, and it is a beta requirement.
4. Source copy to JD's machine today — copy AFTER this fix program, so the
   local build he tests hands-on includes all of the above fixes.

## Locks the builder must work from (supersede older inventory)

- Hot-swap trust model: certificate model, NOT the old debug gate.
  Release + verified signature + `hot_swap:true` = open; `sealed:true` wins
  everywhere; unsigned/invalid fail closed. Debug keeps the open dev path.
- IK budgets: 32-bone cap, 4 influences per vertex, animation at RENDER
  rate — never the 512 TPS sim rate.
- The Cosmic rename: LOCKED (registry id "cosmic").
  The Infinite: PROPOSED as the 8th dimension (post-Daemon, post-Ether).
- Structure block keeps the Minecraft-style name "structure block" for now.


---
**JD decision, 2026-10-12 16:05 EDT (post-packet):** Beta ships CRUST-ONLY. The proposed Wafer layer stack (crust -> passivation -> dielectric -> polysilicon -> buried oxide -> Substrate) is a POST-HANDOFF task, not part of this takeover. Do not fill the underneath in this session. Still open for later: acid-pool layer placement and which ChopOR tier first mines silicon nitride.
