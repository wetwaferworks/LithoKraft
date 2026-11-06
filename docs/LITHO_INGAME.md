# Trace-in-time IN GAME (PlayM4, 2026-11-06)

Launch: Desktop "Play Build (M2).lnk" (same exe, rebuilt with the keys below). Load ALU Stress 128 v1 / 512 v3 as usual.

| Key | What it does |
|---|---|
| Numpad 7 | Compile a chip from every component in a box around YOU (+-96 blocks X/Z, +-32 Y; env LL_LITHO_R / LL_LITHO_H). Recompiling replaces the old chip. |
| Numpad 8 | Stop the chip; its state is handed back to the live sim, which resumes from there. |
| Numpad 9 | Chip speed: 1, 2, 4, 8, 16, 64 chip ticks per world tick (world TPS still set by F7). |

Works with NumLock on or off (raw keypad keys). Also listed in the controls panel (F4).

While the chip runs:
- The live signal tick is replaced by the chip; every component's state (Repeators, Fusors, etc.) is written into the world each tick, so you watch it like normal play.
- Tick line (bottom HUD): `LITHO chip ON  N nodes  compiled X ms  xS/tick  chip tick T  Y us/run (~Zk t/s cap)`.
- Place/break anything -> chip drops automatically, HUD says "dropped (block edit)", live sim resumes.
- Log lines start with `litho:` in logs/lithokraft.log.

Limits (spike):
- Box is around the player, not a selection (no selection tool exists yet).
- Trace wires and lamps do NOT animate while the chip runs (the chip models wire nets as precomputed links and lamps are not chip nodes). Components do.
- Everything outside the box is frozen while the chip runs (live tick is off world-wide).
- Switches/buttons/Pulsors/radio not modelled; torches frozen at compile.
- No live-vs-chip match % in game (harness proved 100%: Run Litho Spike.bat).
- Changed-node settle optimization for big chips NOT done (deferred, per "don't block numpad 7").

Files touched (backups *.s2M4.orig): input_types.h, glfw_input_backend.cpp, actions.h/.cpp, controls_panel.cpp, signal.h/.cpp, signal_world.h/.cpp, game.cpp. Patch script: D:\scratch\alu512-m2\patch_m4.py. Selftest 1537/50, identical FAIL set (D:\scratch\alu512-m4\selftest.out).
