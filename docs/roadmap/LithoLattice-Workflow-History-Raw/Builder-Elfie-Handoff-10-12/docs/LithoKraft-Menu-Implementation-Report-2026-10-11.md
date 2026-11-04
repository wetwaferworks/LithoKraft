# LithoKraft Launch Menu — Implementation Report 2026-10-11

JD picked mockup 1 (classic centered) with tweaks: tagline "A LITHOLATTICE GAME",
2D side-scrolling silicone Wafer backdrop. Built for real. In JD's framing:
**we basically have a pre-alpha going here.**

## What was built

**Boot flow** — LithoKraft no longer launches straight into the demo. The world
loads at boot (as before), but the game starts paused in the launch menu over a
cinematic side-scrolling camera: a slow dolly gliding ±42 blocks along x at
0.06 rad/s, looking −z across the silicon, reusing the player camera the
renderer reads. Play is an instant handoff — no reload.

**Launch menu** (`game_ui.cpp`, exact theme colors from `Theme::defaults()`):
- Big `LITHOKRAFT` title (new `"title"` UI style, textScale 4.5 — the one real
  UI-system gap this needed; added to `engine/src/ui/theme.cpp`)
- Tagline `A LITHOLATTICE GAME` (exact wording), `Divider` accent line
- Three buttons: Play Creative Testing / Settings / Exit (600×52,
  keyboard-focusable, Enter/Space work)
- Version footer bottom-left (`LITHOKRAFT_VERSION` plumbed via `GameConfig`),
  `singleplayer` bottom-right
- Esc on the menu quits (modal-aware: Esc over the settings dialog just closes it)

**Settings screen** — real `TabView` dialog (Audio / Video / Sim tabs), wired only
to settings that actually do something: master volume + %, mute, display mode,
vsync, HUD scale, tick rate, tick radius, EUV lithography toggle. Persists to
`config/settings.json` (loads at boot; CLI flags win; corrupt/missing file →
defaults; out-of-range values clamped). New files:
`game/.../include/lithokraft/game/settings.h`, `game/.../settings.cpp`.

**Pause menu** — upgraded to Resume / Settings / Exit to Menu / Quit to Desktop.
Focus-loss auto-popup preserved (now guarded to the Playing state). Exit to Menu
returns cleanly: world stays loaded, sim paused, cinematic camera resumes.

## Verification (independent)

- Full incremental build: clean, no new warnings (only the grandfathered
  `world/block.h:90` narrowing).
- `ctest`: 10/13 — the 3 failures are exactly the grandfathered ones, no new ones.
- Game UI selftests: **53/53 pass** (13 pre-existing + 40 new: settings file
  round-trip/clamp/corrupt/missing, menu construct + title/tagline/version/divider,
  Play/Settings/Exit click dispatch, Enter-key dispatch, pause 4-button routing,
  all 8 settings controls init + change reporting, Esc-closes-settings).
  - Note: the official `--selftest` binary aborts in `runSignalSelfTest` before
    reaching the game UI tests (see finding 1). Verified via a standalone runner
    with a **scratch-only** bypass (`/tmp/blocks_patched.o`); the tree is untouched.

## Findings for the builder (pre-existing, not touched per instructions)

1. **The shipped game cannot start in this tree.** `blocks.cpp` registers 39 rows
   + Air = 40, but the guard at `blocks.cpp:69` expects `kOverclockor + 1` = 36
   and calls `std::abort()`. Additionally the LithographOR/ChipOR rows are
   commented out, so PortOR/Fusor rows no longer match their `BlockId` constants.
   This is the root cause of the grandfathered `lithokraft_selftest` SIGABRT.
   The menu (and everything else) can't run until the content table is re-synced:
   re-enable/reconcile the -OR rows and bump the guard constant.
2. **PBR transparent-tier dropdown deliberately not wired.** `TransparencyTierConfig`
   exists but no renderer consumes it and `IRenderer` has no tier setter — a
   dropdown would have been a fake setting. Needs a renderer-scoped task adding a
   real `IRenderer::setTransparencyTier` path.
3. Tick radius and vsync are boot-only (`SimConfig`, `WindowDesc`); the settings UI
   labels both "(applies on restart)" and persists them correctly.
4. Input bindings already live in `config/input.json`; left out of settings.
5. Game-level (not GameUi-level) state machine isn't unit-tested headless — `Game`
   needs a renderer + window. A headless software-renderer harness would close it.

## Open questions

- Defer world load until Play vs. load at boot (chosen: boot; Play is instant)?
  Kept boot-load; no reason found to refactor.
- PBR tier wiring (see finding 2) — renderer-scoped follow-up.
- Headless `Game` harness for the state machine (see finding 5).

Mockup updated to the approved design:
`lithokraft-menu-mockups-2026-10-11/menu-mockup-1-classic.png`.
