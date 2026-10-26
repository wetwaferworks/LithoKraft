# Fellowship Playtest 1 — Issue List (2026-10-26)
Builds: 38ea867 DebugOpt (Cortana, Hero, Cloud; depth mode 2), 5a94946 build 63 (Chief). AVX2 SW renderer, 7 threads. All inputs synthetic: input lag NOT judged.
Passed everywhere: no crashes, TPS 256 locked, 0 dropped ticks, mesh-verify 0 mismatches, clean quit, no LOD seams/pops, no pass-through or see-through, new palette only.

## Medium
1. Sealed spawn pocket near y=256 (Cortana).
2. Blank window at startup: ~10 s blank (Chief); stays blank until maximized (Cloud). Likely same root cause.
3. Blank `look:` readout (Cortana); red `look:` line too dense to read (Chief).
4. Coarse stipple dither at far fade edge (Cloud, Cortana "distant speckle"). Fix idea: blue-noise / smoother fade. Crop: /workspace/cloud-playtest/04_far_terrain_crop3x.png

## Low
5. Debug overlay draws over main menu; header cramped; first line clipped under header bar (Chief).
6. Ctrl-sprint drops fly mode (Cortana); Hero no-repro. Maybe timing/input order.
7. Slow start on vertical fly: ~3 blocks in first 4 s from Y 254, then ~5 b/s (Hero). Needs JD: intended?
8. Hitches: 49–86 ms on menu→Play, Esc/quit, fast turns (all). Chief perf log 18:55:55.
9. FPS 35–50 looking down from Y 400–600 (Hero); 27–35 climbing at 1920x1200 (Cloud).
10. Overlay lags real state 1–2 s (Hero; may be capture).
11. Floating chunk at horizon (Cortana).
12. Saved world sim area (r8) overrides configured r6, with warning (Chief, Hero).
13. No-audio box: ALSA card 0 warning; want graceful no-audio path (Chief).

## Info / needs JD
- Far bridge flat color band: intended LOD (Chief, Cloud agree it reads fine).
- Save/logs land in /workspace/lk/game/LithoKraft, not build folder (Hero).
- Dev key log line: expected in test builds; swap only via JD's Release password tool.

## Untested
- Distant-glass x-ray (Hero; needs JD OK for a throwaway glass test world).
- LED/Post readouts, place/break, real input lag.

Evidence: /workspace/explore-cortana/, /workspace/mc-playtest/, /workspace/cloud-playtest/.

## Evidence detail
- Chief: /workspace/mc-playtest/run.log + 00–08 *.webp; look line seen: `look: … #41 @500,265,-454 state 0x0000`.
- Hero: spawn ~447,254,-241 (slow fly-up from Y 254); log /tmp/lk.log; shots in /home/box/agent-data/agents/cfab84e7-ee18-4548-b030-d11ec2eec2bc/attachments/.
- Cloud: /workspace/cloud-playtest/.

## Fixed (2026-10-26, Cortana)
- eb1f81b3: sealed spawn pocket (open-air search), compact look line (name id @x,y,z; dense inspector behind LL_LOOK_VERBOSE=1), debug overlay hidden on menus + top padding.
- c148090: no-audio path logs one info line; saved-vs-configured sim area logged as info (behavior unchanged).
- Still open: blank startup window, far-fade dither, Ctrl-sprint dropping fly, hitches, overlay lag; slow fly start pending JD's call.
