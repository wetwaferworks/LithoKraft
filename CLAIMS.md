# CLAIMS.md — Wet Wafer Works source claims board

One board for the whole company. Its whole job: nobody steps on anybody's toes, and everybody can always read everything.

## The rules

1. **Reading is free.** Anyone on the company network may read and analyze any source at any time, no claim needed, no permission needed. Analysis, notes, and reviews never need a claim.
2. **Claim before you edit.** Before changing a file or area, add a row below: what you're taking, your name, the date/time (EDT), your branch, and one line on what you're doing. First claim holds the edit pen for that file.
3. **One pen per file.** If a file is claimed, don't edit it — read it, review it, talk to the claimant, or take a different file. Two pens on one file is how toes get stepped on.
4. **Release when you're done.** When your change lands (or you set the work down), mark your row Released with the date. A claim is a courtesy, not a fence — if a claim sits idle, ask the claimant before assuming it's free. Lessons, not punishments, if wires cross.
5. **The board lives at the repo root.** Update it in the same commit (or a small follow-up commit) as the work it describes. The registrar (Elfie) tidies the board and keeps the history honest; the claims themselves belong to the claimants.
6. **Big or shared areas** (build files, the CEF layer, anything under `host/`, release packaging): claim the area, say the word at the podium, and give the table a chance to speak before the edit lands.

## Standing lanes (not claims — the map of who usually swims where)

| Area | Lane |
|---|---|
| Windows build, repo order | Cortana |
| Mobile build, web interface (thin layer / `wasm/`) | Elfie |
| Architecture, locks, final say | JD |
| Hints & spot review | Archie (Argon) |
| Art & textures | CloudAtlas |
| Optimization review | Jenny (Gemini) |

## Active claims

| File / area | Claimed by | Since (EDT) | Branch | Doing | Status |
|---|---|---|---|---|---|
| Boardroom web layer (`WetWaferWorks-Inc/Boardroom` on the workstation) | Elfie + Cortana (JD's assignment) | 2026-10-22 | — (company network) | Elfie: initial build (server, page, democracy mechanics). Cortana: workstation home + install. | Active |
| Tree synchronization — Elfie's local work into the workstation tree (JD's direction, 2026-10-23 13:25 EDT): `engine/src/render/gl33/` (renderer, lk_webio, web shims), `wasm/full/` (lkboot.js, gl.html), root `p2*.ps1` | Elfie | 2026-10-23 13:37 | — (share sync; git on Elfie's end being set up) | Sync LANDED 2026-10-23 ~13:45 EDT: 11 files uploaded + hash-verified on the share (gl33_renderer.cpp b2f8e090, lk_webio.js ebac2e30, gl_web_shims.cpp 6727e9dd, gl_web_shims.h c2780f8a, lkboot.js 53b52241, gl.html 1f32533e, p2*.ps1 ×5). Joint review with Cortana + JD lock still to come at the table. Git initialized on Elfie's tree (initial commit 424a89f, 6,117 files). | Landed — under review |

| Diamond tuning (JD 13:18): `engine/src/sim` (TickArea, random ticks), `game/LithoKraft/src/game` (chunk streamer, game, settings), `game/LithoKraft/src/content` (signal area), `game/LithoKraft/src/app/main.cpp`, `wasm/full/gl.html` + `index.html` | Elfie | 2026-10-23 14:05 | main (Elfie tree; syncs to share same session) | Diamond (Manhattan) area shape for streamer + sim; web/phone defaults stream 10 / sim 8. LANDED 2026-10-23 ~14:25 EDT: 17 files synced + hash-verified; wasm build clean. | Landed — under review |
| Engine properties (JD 14:15): `game/LithoKraft/src/game/settings_cvars.cpp` | Elfie | 2026-10-23 14:28 | main (Elfie tree; syncs to share same session) | Register render.stream_radius + sim.area_shape + render.stream_shape cvars on the SettingsCvars bridge (sim.tickradius already exists); AngelScript reaches them via consoleCvar/consoleSetCvar. LANDED 2026-10-23 ~14:35 EDT: build clean (fresh wasm 14:24), synced + verified. | Landed — under review |

| Tree synchronization — Elfie's bench work into the workstation tree (JD's direction, 2026-10-26 11:25 EDT): rulings 1–3, strike program, PT4 Targets A/B/C (projectile, mod_signature, software renderer/SIMD/AA, main.cpp, game.cpp, bench_stream.cpp; exact file set = bench-vs-baseline census) | Elfie | 2026-10-26 11:31 | — (share sync; Cortana commits on the workstation) | Landing verified bench work to the share, hash-verified on read-back; pulling workstation-side changes down so both trees match. No deletions either side. Landed 2026-10-26: pushed 33, pulled 0, conflicts held 0, failed 0. Awaiting Cortana's commit. | Landed |

## Released claims

| File / area | Claimed by | Released (EDT) | What landed |
|---|---|---|---|
| Hardware-layer refactor to IHwLayer (JD 2026-10-25 01:58): `host/lithocef` | Cortana | 2026-10-25 02:35 | Local commits on main, no push: 3fc6219 (phase 0 claim + baseline of Elfie's landed host files), 352386f (phase 1 rename/split: IHwLayer.h + IHwCef.h, ILithoCEF.h compat), 1dbf276 (phase 2: census ABI 2, IHwWebkit stub, IHwGpu decl, hw_log.h), f943849 (phase 3: IHwGpu draw-path seat shape + IHwGpu-DESIGN.md), plus this release. Thread Handshake sources byte-identical. Gates each phase: 48/48, hashes 24c52df2be14789c x3, cities off 43693d018d046b25, host ctest 3/3 -> 4/4. Direction and final verify: JD. Implementation: Cortana. Plan and register: Elfie. |
| Fix run 3 (JD t2262u): `engine/src/sim/` (area_shape.h new, idemo_record.h, demo_record.cpp), `engine/tests/engine_tests.cpp`, `game/LithoKraft/src/game/` (game.h, game.cpp, settings.h, settings_cvars.cpp, world_save.h, world_save.cpp), `game/LithoKraft/src/app/` (main.cpp, selftest.cpp, demo_record_test.cpp), `game/LithoKraft/config/settings.json`, CLAIMS.md header encoding, review doc | Cortana | 2026-10-23 15:11 | Uncommitted on main for JD; .orig-2026-10-23-fix3 backups. Diamond default for sim + stream, sim area always inside the stream area, case-insensitive shape parser (unknown -> warn + diamond), level.json v3 + demo area word, diamond golden. See docs/reviews/CODE_REVIEW_2026-10-23_Cortana.md, Fix run 3 |
| Code review + bug-fix run (2026-10-23): gl_loader.h, gl33_renderer.cpp (log line), game.cpp (item toss), procgen/stages.cpp | Cortana | 2026-10-23 14:20 | Uncommitted for JD; .orig-2026-10-23-review backups; see docs/reviews/CODE_REVIEW_2026-10-23_Cortana.md |
| `host/lithocef` Thread Handshake merge | Elfie | 2026-10-23 05:17 | Landed: thread_handshake.h/.cpp + thread_handshake_smoke.cpp (new), CMakeLists.txt and README.md updated as strict supersets (Thread Handshake target + docs section). Verified by hash after upload. |
| 2026-10-23 06:1x EDT | Elfie | host/lithocef/ (lithocef_boot_smoke.cpp NEW + CMakeLists.txt + README.md) — lifecycle smoke: re-boot clause, finite ring, handshake-during-boot; gated 3/3 ctest locally before landing |
| 2026-10-23 06:2x EDT | Elfie | RELEASED host/lithocef/ — boot smoke landed; gate: ctest 3/3 + all three smokes 15/15 direct; files hash-verified on the share |
| Fix run 2 (2026-10-23, JD-approved): counters, MCP shutdown, net, tests, sqrt, iksolve, docs | Cortana | 2026-10-23 14:30 | Released 2026-10-23 14:55 (uncommitted on main; .orig-2026-10-23-fix2 backups) |
