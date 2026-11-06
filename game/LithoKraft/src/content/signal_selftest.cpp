// Headless checks of every signal block's logic (run by --selftest).
#include "lithokraft/content/trace_conn.h"  // 2026-11-01 PlayL
#include "litholattice/storage/storage.h"  // 2026-11-01 PlayL: column codec roundtrip
#include "lithokraft/content/pulse_rules.h"  // 2026-11-01 PlayK
#include <cstdlib>
#include <algorithm>
#include "lithokraft/content/randor_rng.h"
#include "lithokraft/content/scheduled_output.h"  // 2026-11-04 delays
#include "components/routor.h"  // PlayB: Routor (was the Redirector)  // Randor selftest
#include <chrono>
#include <format>
#include "lithokraft/content/foliage.h"
#include <string>
#include <vector>
#include <cstdio>
#include "lithokraft/content/component.h"
#include "lithokraft/content/compiled_comp_table.h"
#include "lithokraft/content/signal.h"
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/component_models.h"  // 2026-11-05 E2 lever v2
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/circuit_scene.h"
#include "lithokraft/content/signal_world.h"
#include "lithokraft/content/post.h"  // 2026-10-25 Posts
#include "litholattice/sim/sim_thread.h"  // placeOverlapsPlayer
#include "lithokraft/content/repeator.h"  // Cortana 2026-10-15 2215

namespace lithokraft {

int runSignalSelfTest() {
  int fails = 0;
  auto check = [&](bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++fails;
  };
  ChunkMap w;
  w.setRegistry(&blockRegistry());
  for (int cz = 0; cz < 4; ++cz)
    for (int cx = 0; cx < 6; ++cx) {
      auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz;
      w.insert(std::move(col));
    }
  const int y = 10;  // flat floor at y-1
  for (int z = 0; z < 64; ++z)
    for (int x = 0; x < 96; ++x) w.set(x, y - 1, z, kWaferSilicon);
  cSignalSim sim;
  auto put = [&](int x, int yy, int z, BlockId b) { w.set(x, yy, z, b); sim.onBlockChanged(x, yy, z, b); };
  auto run = [&](int n) { for (int i = 0; i < n; ++i) sim.tick(w, 0, 0, 100, [](int, int, int) {}); };
  const cSignalSim::DirtyFn none = [](int, int, int) {};
  auto putF = [&](int x, int yy, int z, BlockId b, int f) { w.set(x, yy, z, b); w.setState(x, yy, z, makeState(0, f)); sim.onBlockChanged(x, yy, z, b); };
  auto setW = [&](int x, int yy, int z, int s) { w.setState(x, yy, z, CellState(s)); sim.invalidateNetwork(); };  // drive a wire for one tick
  auto val = [&](int x, int yy, int z) { return stValue(w.getState(x, yy, z)); };

  std::printf("LithoLattice sim self-test\n");
  // A) torch -> 10 red trace -> lamp, plus an unpowered blue trace alongside.
  put(0, y, 0, kRedTorch);
  for (int x = 1; x <= 10; ++x) { put(x, y, 0, kRedTrace); put(x, y, 1, kBlueTrace); }
  put(11, y, 0, kLamp);
  // B) 70-long run: signal must die after 64 blocks.
  put(0, y, 4, kRedTorch);
  for (int x = 1; x <= 70; ++x) put(x, y, 4, kRedTrace);
  put(71, y, 4, kLamp);
  // C) inverter: torch -> wire (gap) -> block -> torch on block -> wire -> lamp.
  put(0, y, 8, kRedTorch);
  put(1, y, 8, kRedTrace); put(3, y, 8, kRedTrace);  // gap at x=2
  put(4, y, 8, kWaferSilicon);
  put(4, y + 1, 8, kRedTorch);
  for (int x = 5; x <= 8; ++x) { put(x, y, 8, kWaferSilicon); put(x, y + 1, 8, kRedTrace); }
  put(9, y + 1, 8, kLamp);
  // D) staircase: wire climbs one block, then a block above cuts it.
  put(0, y, 12, kRedTorch);
  put(1, y, 12, kRedTrace); put(2, y, 12, kWaferSilicon); put(2, y + 1, 12, kRedTrace);
  put(0, y, 14, kRedTorch);
  put(1, y, 14, kRedTrace); put(1, y + 1, 14, kWaferSilicon);  // cut: block above the low wire
  put(2, y, 14, kWaferSilicon); put(2, y + 1, 14, kRedTrace);

  run(3);
  check(w.getState(0, y, 0) == 1, "torch on unpowered block is lit");
  check(w.getState(1, y, 0) == 64 && w.getState(2, y, 0) == 63 && w.getState(10, y, 0) == 55,
        "red wire strength 64 at torch, decays 1 per block (55 after 10)");
  check(w.getState(11, y, 0) == 1, "lamp lit by red wire");
  bool blueDark = true;
  for (int x = 1; x <= 10; ++x) blueDark &= w.getState(x, y, 1) == 0;
  check(blueDark, "blue wire beside red wire stays at 0 (colours never connect)");
  check(w.getState(64, y, 4) == 1 && w.getState(65, y, 4) == 0 && w.getState(71, y, 4) == 0,
        "signal reaches 64 blocks then dies; lamp at 71 stays off");
  check(w.getState(4, y + 1, 8) == 1 && w.getState(9, y + 1, 8) == 1,
        "inverter: block unpowered -> torch lit -> lamp lit");
  put(2, y, 8, kRedTrace);  // close the gap
  run(1);
  check(w.getState(4, y + 1, 8) == 1 && w.getState(3, y, 8) > 0, "tick 1 after closing gap: wire powers block, torch still lit (1-tick inverter delay)");
  run(1);
  check(w.getState(4, y + 1, 8) == 0 && w.getState(9, y + 1, 8) == 0, "tick 2: torch off, downstream lamp off");
  put(2, y, 8, kAir);  // open again
  run(2);
  check(w.getState(9, y + 1, 8) == 1, "after re-opening gap: lamp back on");
  check(w.getState(2, y + 1, 12) == 63, "wire climbs a staircase (63 one block up)");
  check(w.getState(2, y + 1, 14) == 0, "staircase cut by a block above the lower wire");
  put(10, y, 0, kAir);
  run(1);
  check(w.getState(11, y, 0) == 0, "breaking the last wire turns the lamp off");
  // E) torch colours: each torch feeds only its own wire colour; any colour powers lamps/blocks.
  put(0, y, 18, kRedTorch); put(1, y, 18, kBlueTrace);
  put(0, y, 20, kBlueTorch); put(1, y, 20, kBlueTrace); put(2, y, 20, kBlueTrace); put(3, y, 20, kLamp);
  put(5, y, 22, kBlueTorch); put(6, y, 22, kRedTrace);
  // blue trace powering the block under a red torch turns that red torch off
  put(10, y, 24, kBlueTorch); put(11, y, 24, kBlueTrace); put(12, y, 24, kWaferSilicon); put(12, y + 1, 24, kRedTorch);
  run(3);
  check(w.getState(1, y, 18) == 0, "red torch does not power blue wire");
  check(w.getState(6, y, 22) == 0, "blue torch does not power red wire");
  check(w.getState(1, y, 20) == 64 && w.getState(2, y, 20) == 63, "blue torch powers blue wire (64, 63)");
  check(w.getState(3, y, 20) == 1, "lamp lit by blue wire");
  check(w.getState(12, y + 1, 24) == 0, "blue wire powering a block turns the red torch on it off");
  // F) torch powers the solid block above it; that block emits colourless power.
  put(20, y, 26, kBlueTorch); put(20, y + 1, 26, kWaferSilicon); put(20, y + 2, 26, kRedTrace);
  put(21, y, 26, kWaferSilicon); put(21, y + 1, 26, kRedTrace); put(22, y, 26, kWaferSilicon); put(22, y + 1, 26, kLamp);
  put(19, y, 26, kWaferSilicon); put(19, y + 1, 26, kBlueTrace);
  put(30, y, 26, kRedTorch); put(30, y + 1, 26, kWaferSilicon); put(31, y, 26, kWaferSilicon); put(31, y + 1, 26, kLamp);
  put(40, y, 26, kRedTorch); put(40, y + 1, 26, kWaferSilicon); put(40, y + 2, 26, kBlueTorch);  // torch tower
  put(50, y, 26, kRedTorch); put(50, y + 1, 26, kLamp);
  put(60, y, 28, kRedTorch); put(61, y + 1, 28, kRedTrace); put(61, y, 28, kWaferSilicon);  // air above torch
  run(3);
  check(w.getState(21, y + 1, 26) == 64 && w.getState(19, y + 1, 26) == 64 && w.getState(20, y + 2, 26) == 64,
        "blue torch powers block above; block feeds red, blue and on-top wire at 64");
  check(w.getState(22, y + 1, 26) == 1, "lamp lit through wire from a torch-powered block");
  check(w.getState(31, y + 1, 26) == 1, "lamp lit directly by an adjacent torch-powered block");
  check(w.getState(40, y + 2, 26) == 0, "torch on a torch-powered block is off (torch tower)");
  check(w.getState(50, y + 1, 26) == 1, "lamp directly above a torch is lit");
  check(w.getState(61, y + 1, 28) == 0, "air above a torch carries no power");
  put(40, y, 26, kAir);
  run(2);
  check(w.getState(40, y + 2, 26) == 1, "removing the lower torch relights the tower torch");

  // G) Repeator: re-boost to 64, one-way, delay 1-8 (right-click cycles).
  constexpr int E = 1, W = 3;
  put(0, y, 34, kSwitch); for (int x = 1; x <= 3; ++x) put(x, y, 34, kRedTrace);
  putF(4, y, 34, kRepeator, E); for (int x = 5; x <= 8; ++x) put(x, y, 34, kBlueTrace);
  put(0, y, 36, kSwitch); put(1, y, 36, kRedTrace); putF(2, y, 36, kRepeator, W); put(3, y, 36, kRedTrace);
  sim.use(w, 0, y, 34, none); sim.use(w, 0, y, 36, none);
  run(6);  // 2026-10-24: fresh delay 4 (was 2)
  check(val(3, y, 34) == 62 && val(5, y, 34) == 64 && val(8, y, 34) == 61, "repeator re-boosts 62 -> 64 into the other colour");
  check(val(1, y, 36) == 64 && val(3, y, 36) == 0, "repeator is one-way (front never feeds back)");
  {  // 2026-10-24 (JD): "LED powers the Repeator" - an LED directly behind a Repeator feeds it 64.
    put(80, y, 60, kRedTorch); putF(81, y, 60, kRepeator, E); put(82, y, 60, kRedTrace);   // behind
    putF(81, y, 56, kRepeator, E); put(81, y, 57, kBlueTorch); put(82, y, 56, kRedTrace); // beside
    putF(84, y, 52, kRepeator, E); put(85, y, 52, kRedTorch); put(83, y, 52, kRedTrace);  // in front
    run(12);
    const int behind = val(82, y, 60), beside = val(82, y, 56), front = val(83, y, 52);
    put(80, y, 60, kAir);
    run(12);
    check(behind == 64 && beside == 0 && front == 0 && val(82, y, 60) == 0,
          "led -> repeator: an LED behind feeds 64; beside or in front does nothing; breaking it drops the output");
  }  put(0, y, 38, kSwitch); put(1, y, 38, kRedTrace); putF(2, y, 38, kRepeator, E); put(3, y, 38, kRedTrace);
  put(0, y, 40, kSwitch); put(1, y, 40, kRedTrace); putF(2, y, 40, kRepeator, E); put(3, y, 40, kRedTrace);
  // Cortana 2026-10-15 2215 [GAME] (JD D4): delays are 2/4/8/16; a fresh (legacy-encoded) repeator
  // reads as delay 1 -> nearest 2. One click -> 4; three more wrap 8, 16 and back to 2.
  check(repeator::delayTicks(w.getState(2, y, 38)) == 4 && repeator::delayIndex(w.getState(2, y, 38)) == 1, "fresh repeator = setting 2/6 = delay 4 (legacy 1 migrates to 4 ticks)");
  sim.use(w, 2, y, 38, none);  // 2 -> 4
  check(repeator::delayTicks(w.getState(2, y, 38)) == 8 && (w.getState(2, y, 38) & repeator::kNewEncoding),
        "right-click cycles repeator delay (4 -> 8), new encoding");
    {  // 2026-10-25 (JD): Repeator starts at button 1 / 4 ticks; each use steps 1->2->3->4->1;
       // exactly one button is pressed (3 raised + the slab = 4 pick boxes) and the sim delay matches.
      const int rx = 90, rz = 62;
      putF(rx, y, rz, kRepeator, 1); run(1);  // the sim registers the component on its next tick
      bool ok = repeator::delayIndex(w.getState(rx, y, rz)) == 1 && repeator::delayTicks(w.getState(rx, y, rz)) == 4;
      static constexpr int kWant[7] = {2, 3, 4, 5, 0, 1, 2};  // PlayM: 8, 16, 32, 64, 2, 4, 8
      for (int k = 0; k < 7; ++k) {
        sim.use(w, rx, y, rz, none);
        const CellState s = w.getState(rx, y, rz);
        litholattice::PickBox pb[8];
        const int n = pickShapeBoxes(w, blockRegistry(), rx, y, rz, kRepeator, pb, 8);
        ok = ok && repeator::delayIndex(s) == kWant[k] && repeator::delayTicks(s) == repeator::kDelays[kWant[k]] && n == 1 + (repeator::kSettings - 1 - kWant[k]);  // slab + raised (pressed = setting + 1)
      }
      check(ok, "REPEATOR: new = 4 ticks; right click cycles 4->8->16->32->64->2->4 (six settings); pressed buttons = setting number (CloudAtlas six-button art); sim delay matches");
    }
  sim.use(w, 0, y, 38, none); sim.use(w, 0, y, 40, none);
  int t4 = -1, t1 = -1;
  for (int t = 1; t <= 24; ++t) {
    run(1);
    if (t1 < 0 && val(3, y, 40) > 0) t1 = t;
    if (t4 < 0 && val(3, y, 38) > 0) t4 = t;
  }
  check(t1 > 0 && t4 - t1 == 4, "repeator delay 8 is 4 ticks slower than delay 4");
  {  // 16-tick line (history low byte + timer high byte) and the legacy migration table
    put(0, y, 42, kSwitch); put(1, y, 42, kRedTrace); putF(2, y, 42, kRepeator, E); put(3, y, 42, kRedTrace);
    put(0, y, 44, kSwitch); put(1, y, 44, kRedTrace); putF(2, y, 44, kRepeator, E); put(3, y, 44, kRedTrace);
    for (int i = 0; i < 3; ++i) sim.use(w, 2, y, 42, none);  // 2 -> 16
    check(repeator::delayTicks(w.getState(2, y, 42)) == 32, "three clicks reach delay 32");
    sim.use(w, 0, y, 42, none); sim.use(w, 0, y, 44, none);
    int a = -1, b = -1;
    for (int t = 1; t <= 50; ++t) {
      run(1);
      if (a < 0 && val(3, y, 44) > 0) a = t;
      if (b < 0 && val(3, y, 42) > 0) b = t;
    }
    check(a > 0 && b - a == 28, "repeator delay 32 (32-bit line) is 28 ticks slower than delay 4");
    const int want[8] = {4, 4, 4, 8, 8, 8, 16, 16};  // same button index as before (2/4/8 -> 4/8/16)
    bool mig = true;
    for (int d = 1; d <= 8; ++d) mig = mig && repeator::delayTicks(makeState(0, 1, uint8_t(d - 1))) == want[d - 1];
    check(mig, "legacy repeator delays 1-8 migrate to buttons 1-3 (4/8/16 ticks)");
  }
  {  // 2026-10-31 (JD t3680u): a blue-fed Repeator chain is blue end to end; a red one stays red.
    const int cz = 26;  // rows 25-29 at x <= 10 are free (z 160 was outside the test area)
    put(0, y, cz, kSwitch); for (int x = 1; x <= 3; ++x) put(x, y, cz, kBlueTrace);
    for (int x = 4; x <= 7; ++x) putF(x, y, cz, kRepeator, E);
    put(8, y, cz, kBlueTrace);
    put(0, y, cz + 2, kSwitch); for (int x = 1; x <= 3; ++x) put(x, y, cz + 2, kRedTrace);
    for (int x = 4; x <= 7; ++x) putF(x, y, cz + 2, kRepeator, E);
    put(8, y, cz + 2, kRedTrace);
    sim.use(w, 0, y, cz, none); sim.use(w, 0, y, cz + 2, none);
    run(80);
    bool blue = true, red = true;
    for (int x = 4; x <= 7; ++x) { blue = blue && repeator::isBlue(w.getState(x, y, cz)); red = red && !repeator::isBlue(w.getState(x, y, cz + 2)); }
    check(blue && red && val(8, y, cz) > 0, "REPEATOR chain colour: blue Trace -> 4 Repeators all blue (red chain stays red), output reaches the end");
    sim.use(w, 0, y, cz, none);
    run(80);
    bool kept = true;
    for (int x = 4; x <= 7; ++x) kept = kept && repeator::isBlue(w.getState(x, y, cz));
    check(kept, "REPEATOR chain colour: a depowered blue chain keeps its colour (no red flash)");
  }

  {  // 2026-11-05 (JD #5): Repeator side lock (powered Repeator facing L/R freezes out+colour).
    const int vz = 110, N = 0, E = 1;
    auto clearCell = [&](int x, int zz) { put(x, y, zz, kAir); };
    for (int x = 0; x <= 4; ++x) { clearCell(x, vz); clearCell(x, vz + 1); clearCell(x, vz + 2); }
    put(0, y, vz, kSwitch);
    put(1, y, vz, kRedTrace);
    putF(2, y, vz, kRepeator, E);
    put(3, y, vz, kRedTrace);
    putF(2, y, vz + 1, kRepeator, N);
    put(2, y, vz + 2, kSwitch);
    sim.use(w, 0, y, vz, none);
    run(40);
    const bool onBefore = val(3, y, vz) > 0;
    sim.use(w, 2, y, vz + 2, none);
    run(40);
    sim.use(w, 0, y, vz, none);
    run(40);
    const bool heldOn = val(3, y, vz) > 0;
    check(onBefore && heldOn, "REPEATOR lock-hold-on: locked ON stays ON after back input drops");
    sim.use(w, 2, y, vz + 2, none);
    run(40);
    check(val(3, y, vz) == 0, "REPEATOR lock-release: after unlock, output follows back (OFF)");
    for (int x = 0; x <= 4; ++x) { clearCell(x, vz); clearCell(x, vz + 1); clearCell(x, vz + 2); }
    put(0, y, vz, kSwitch);
    put(1, y, vz, kRedTrace);
    putF(2, y, vz, kRepeator, E);
    put(3, y, vz, kRedTrace);
    putF(2, y, vz + 1, kRepeator, N);
    put(2, y, vz + 2, kSwitch);
    run(10);
    sim.use(w, 2, y, vz + 2, none);
    run(40);
    sim.use(w, 0, y, vz, none);
    run(40);
    check(val(3, y, vz) == 0, "REPEATOR lock-hold-off: locked OFF stays OFF after back input rises");
    for (int x = 0; x <= 4; ++x) { clearCell(x, vz); clearCell(x, vz + 1); clearCell(x, vz + 2); }
  }
  // PlayM (2026-11-01): sections H..tone drive wires by injecting raw values (setW, no driver). A same-tick
  // settle re-floods an undriven wire to 0 within the tick, so these component-logic checks run with the
  // settle off (pre-PlayM timing); same-tick behaviour has its own SAME TICK tests further down.
  const bool keepSameTick = sim.sameTick();
  sim.setSameTick(false);
  // H) Thresholdor: back = input, left = LOW, right = HIGH; output = input strength.
  putF(10, y, 46, kThresholdor, E); put(9, y, 46, kRedTrace); put(10, y, 45, kRedTrace); put(10, y, 47, kRedTrace);
  put(11, y, 46, kRedTrace);
  auto thr = [&](int in, int lo, int hi) { setW(9, y, 46, in); setW(10, y, 45, lo); setW(10, y, 47, hi); run(1); return val(11, y, 46); };  // output = front wire (compare-mode state value = back input, readout)
  check(thr(30, 0, 0) == 30, "thresholder: no reference -> passes the input");
  check(thr(30, 20, 0) == 30 && thr(10, 20, 0) == 0, "thresholder: LOW ref only = threshold (in >= ref)");
  check(thr(30, 0, 40) == 0 && thr(50, 0, 40) == 50, "thresholder: HIGH ref only = threshold too");
  check(thr(30, 20, 40) == 30 && thr(45, 20, 40) == 0 && thr(10, 20, 40) == 0, "thresholder: both refs = window comparator");
  check(thr(30, 40, 20) == 30, "thresholder: swapped refs are sorted (min/max)");
  // 2026-10-25 (JD, CloudAtlas readout): compare mode shows the BACK input on the 2-digit readout
  // (state value), passing or not; the output stays the comparison result.
  check(thr(10, 20, 0) == 0 && val(10, y, 46) == 10 && !stOn(w.getState(10, y, 46)),
        "thresholder readout: compare mode, blocked -> readout shows the back input (10), output 0");
  check(thr(30, 20, 40) == 30 && val(10, y, 46) == 30 && stOn(w.getState(10, y, 46)),
        "thresholder readout: compare mode, passing -> readout 30, output 30, lit");
  thr(33, 0, 0);
  check(val(11, y, 46) == 33, "thresholder output drives its front wire at the input strength");
  // H2) 2026-10-25 (JD): subtract mode, toggled by use (aux bit 0); out = max(0, back - max(sides)).
  check(stAux(w.getState(10, y, 46)) == 0, "thresholder: placed in compare mode (aux 0)");
  // 2026-11-04 (JD t4305u): the game only sends Use through playerUsable (kInteractive); this test used
  // sim.use directly, so it passed while the in-game right click never reached the Thresholdor.
  check(playerUsable(blockRegistry(), kThresholdor), "thresholder: right click reaches it in game (shared playerUsable gate)");
  if (playerUsable(blockRegistry(), w.get(10, y, 46))) sim.use(w, 10, y, 46, none);
  check((stAux(w.getState(10, y, 46)) & 1) == 1, "thresholder: use toggles subtract mode");
  check(thr(40, 10, 0) == 30 && thr(40, 0, 25) == 15 && thr(40, 10, 25) == 15, "thresholder subtract: back - max(left, right)");
  check(thr(30, 0, 0) == 30 && thr(20, 30, 0) == 0 && thr(64, 64, 64) == 0, "thresholder subtract: clamps at 0, no side = pass-through");
  {
    bool down = true; int prevOut = 99;
    for (int s = 0; s <= 50; s += 10) { const int o = thr(50, s, 0); down = down && o == 50 - s && o < prevOut; prevOut = o; }
    check(down, "thresholder subtract: countdown 50 -> 0 as the side steps 0, 10 .. 50");
  }
  check(thr(64, 0, 0) == 64 && val(11, y, 46) == 64, "thresholder subtract: 0..64 range, drives its front wire");
  check(thr(40, 10, 0) == 30 && val(10, y, 46) == 30, "thresholder readout: subtract mode shows the forward output (40 - 10 = 30)");
  sim.use(w, 10, y, 46, none);
  check(stAux(w.getState(10, y, 46)) == 0 && thr(30, 20, 40) == 30 && thr(45, 20, 40) == 0, "thresholder: use again = compare mode, window unchanged");
  {
    // Comparator clock: torch -> back; front wire loops round into the right side. Subtract mode
    // knocks its own output down each time the fed-back side catches up, so it oscillates.
    put(50, y, 48, kRedTorch); put(51, y, 48, kRedTrace);
    putF(52, y, 48, kThresholdor, E);
    put(53, y, 48, kRedTrace); put(53, y, 49, kRedTrace); put(52, y, 49, kRedTrace);
    sim.use(w, 52, y, 48, none);
    run(8);
    int seq[48];
    for (int i = 0; i < 48; ++i) { run(1); seq[i] = val(52, y, 48); }
    int hi = 0, lo = 99, period = 0;
    for (int i = 0; i < 48; ++i) { hi = std::max(hi, seq[i]); lo = std::min(lo, seq[i]); }
    for (int p = 1; p <= 16 && !period; ++p) {
      bool same = true;
      for (int i = 16; i + p < 48; ++i) same = same && seq[i] == seq[i + p];
      if (same) period = p;
    }
    std::printf("  comparator clock out:");
    for (int i = 0; i < 16; ++i) std::printf(" %d", seq[i]);
    std::printf(" (period %d)\n", period);
    check(hi > lo && period >= 2, "thresholder subtract comparator clock oscillates with a steady period");
  }

  // I) Redirector: any side in, front out.
  putF(20, y, 46, kRedirector, E); put(20, y, 45, kBlueTrace); put(21, y, 46, kRedTrace); put(19, y, 46, kRedTrace);
  setW(20, y, 45, 25); run(1);
  check(val(20, y, 46) == 25 && val(21, y, 46) == 25, "redirector: side input 25 comes out the front");
  setW(21, y, 46, 30); run(1);
  check(val(20, y, 46) == 0, "redirector ignores signal arriving at its front");
  {  // 2026-10-31 (JD mux): red at the back, blue on one side, a clocked Repeator on the other side.
    // Level-held: Repeator off = red passes, on = blue passes. Only a Repeator switches the mode.
    int RN = 0;  // facing that points from z+1 back to z (toward the Redirector)
    for (int d = 0; d < 4; ++d) if (kDirX[d] == 0 && kDirZ[d] == -1) RN = d;
    putF(72, y, 12, kRedirector, E); put(73, y, 12, kRedTrace);                       // out the front
    put(66, y, 12, kSwitch); for (int x = 67; x <= 71; ++x) put(x, y, 12, kRedTrace);  // red, back
    put(72, y, 9, kSwitch); put(72, y, 10, kBlueTrace); put(72, y, 11, kBlueTrace);   // blue, side
    put(72, y, 16, kSwitch); put(72, y, 15, kRedTrace); put(72, y, 14, kRedTrace);    // clock source
    putF(72, y, 13, kRepeator, RN);                                                    // select, side
    sim.use(w, 66, y, 12, none); sim.use(w, 72, y, 9, none);
    run(20);
    const int redIn = val(71, y, 12), blueIn = val(72, y, 11);
    const bool distinct = redIn > 0 && blueIn > 0 && redIn != blueIn;
    check(distinct && val(72, y, 12) == redIn && val(73, y, 12) > 0, "REDIRECTOR mux: select Repeator off -> red passes out the front");
    bool alt = distinct;
    for (int half = 0; half < 6; ++half) {  // clock: toggle every 24 ticks (Repeator delay 4)
      sim.use(w, 72, y, 16, none);
      run(24);
      const int want = (half & 1) == 0 ? blueIn : redIn;
      alt = alt && val(72, y, 12) == want && val(73, y, 12) > 0;
    }
    check(alt, "REDIRECTOR mux: clocked side Repeator alternates blue / red every half-cycle (level-held)");
    // Negative: a raw Trace on the side (powered) does not switch the mode; linked behaviour stays.
    put(72, y, 13, kRedTrace); sim.use(w, 72, y, 16, none);
    run(20);
    const int sideIn = val(72, y, 13);
    check(sideIn > 0 && val(72, y, 12) == std::max(std::max(redIn, blueIn), sideIn), "REDIRECTOR mux: a powered raw Trace on the side does not switch the mode");
  }

  {  // 2026-10-31 (JD): a Switch directly behind a Repeator powers it; the chain turns purple.
    put(60, y - 1, 20, kWaferSilicon);
    w.set(60, y, 20, kSwitch); w.setState(60, y, 20, withAttach(makeState(0, E), 1)); sim.onBlockChanged(60, y, 20, kSwitch);
    putF(61, y, 20, kRepeator, E); putF(62, y, 20, kRepeator, E); put(63, y, 20, kRedTrace);
    run(4);
    sim.use(w, 60, y, 20, none);
    run(40);
    check(val(63, y, 20) > 0 && stOn(w.getState(62, y, 20)), "REPEATOR: an attached Switch right behind the back port powers it directly");
    check(repeator::colourOf(w.getState(61, y, 20)) == 3 && repeator::colourOf(w.getState(62, y, 20)) == 3,
          "REPEATOR: Switch-fed Repeator is purple and passes purple down the chain");
  }
  {  // 2026-11-01 (JD): direct comp->comp, no Trace. Attached Switch -> Repeator -> Thresholdor -> lamp.
    put(60, y - 1, 62, kWaferSilicon); put(62, y - 1, 62, kWaferSilicon);
    w.set(60, y, 62, kSwitch); w.setState(60, y, 62, withAttach(makeState(0, E), 1)); sim.onBlockChanged(60, y, 62, kSwitch);
    putF(61, y, 62, kRepeator, E); putF(62, y, 62, kThresholdor, E); put(63, y, 62, kLamp);
    // Attached Switch beside a non-Repeator component (Thresholdor) -> lamp.
    put(70, y - 1, 62, kWaferSilicon);
    w.set(70, y, 62, kSwitch); w.setState(70, y, 62, withAttach(makeState(0, E), 1)); sim.onBlockChanged(70, y, 62, kSwitch);
    putF(71, y, 62, kThresholdor, E); put(72, y, 62, kLamp);
    run(4); sim.use(w, 60, y, 62, none); sim.use(w, 70, y, 62, none); run(40);
    std::printf("  [info] direct: sw on %d rep on %d thr st %u on %d lamp %u | sw2 on %d thr2 st %u lamp2 %u\n", int(stOn(w.getState(60, y, 62))), int(stOn(w.getState(61, y, 62))), unsigned(w.getState(62, y, 62)), int(stOn(w.getState(62, y, 62))), unsigned(w.getState(63, y, 62)), int(stOn(w.getState(70, y, 62))), unsigned(w.getState(71, y, 62)), unsigned(w.getState(72, y, 62)));
    check(w.getState(63, y, 62) == 1, "DIRECT: Switch -> Repeator -> Thresholdor -> lamp lights with no Trace");
    check(w.getState(72, y, 62) == 1, "DIRECT: an attached Switch powers an adjacent Thresholdor directly");
    // Pulsor -> Repeator face to face: caught and dropped.
    put(60, y, 58, kSwitch); put(61, y, 58, kRedTrace); putF(62, y, 58, kPulsor, E); put(62, y, 57, kBlueTrace); put(62, y, 56, kSwitch);
    putF(63, y, 58, kRepeator, E); put(64, y, 58, kRedTrace);
    sim.use(w, 60, y, 58, none); run(2); int mx = 0;
    for (int i = 0; i < 12; ++i) { sim.use(w, 62, y, 56, none); run(3); mx = std::max(mx, val(64, y, 58)); }
    if (cPulseRules::repeatorLinks()) check(mx > 0, "REP LINK: a Pulsor fires straight into the back of an adjacent Repeator");
    else check(mx == 0, "DIRECT: a Pulsor pulse into an adjacent component is caught and dropped");
  }
  {  // 2026-11-01 (JD, PlayK): Switch -> Repeator -> Catchor (match 64) -> Repeator -> Trace, no Trace into the Catchor.
    put(60, y - 1, 25, kWaferSilicon);
    w.set(60, y, 25, kSwitch); w.setState(60, y, 25, withAttach(makeState(0, E), 1)); sim.onBlockChanged(60, y, 25, kSwitch);
    putF(61, y, 25, kRepeator, E);
    w.set(62, y, 25, kCatchor); w.setState(62, y, 25, makeState(64, E, 3, false)); sim.onBlockChanged(62, y, 25, kCatchor);
    putF(63, y, 25, kRepeator, E); put(64, y, 25, kRedTrace);
    run(4); sim.use(w, 60, y, 25, none); int mx = 0;
    for (int i = 0; i < 40; ++i) { run(1); mx = std::max(mx, val(64, y, 25)); }
    std::printf("  [info] replink catchor: st %u out-trace max %d\n", unsigned(w.getState(62, y, 25)), mx);
    if (cPulseRules::repeatorLinks()) check(mx > 0, "REP LINK: a Repeator feeds a Catchor directly and the Catchor passes its catch to a Repeator");
    else check(mx == 0, "REP LINK off: a Repeator does not feed a Catchor");
  }
  {  // 2026-10-31 (JD): Fusor auto-reset. Input = back; a Repeator on a side resets on its rising edge.
    int RN = 0;
    for (int d = 0; d < 4; ++d) if (kDirX[d] == 0 && kDirZ[d] == -1) RN = d;
    auto fuse = [&](int fx) {
      w.set(fx, y, 20, kFusor); w.setState(fx, y, 20, makeState(10, E)); sim.onBlockChanged(fx, y, 20, kFusor);
      put(fx - 4, y, 20, kSwitch); for (int x = fx - 3; x < fx; ++x) put(x, y, 20, kRedTrace);  // back input ~62
      put(fx + 1, y, 20, kRedTrace);                                                           // output
      put(fx, y, 23, kSwitch); put(fx, y, 22, kRedTrace);                                       // side source
    };
    fuse(70); putF(70, y, 21, kRepeator, RN);
    fuse(78); put(78, y, 21, kRedTrace);                                                       // negative: raw Trace
    sim.use(w, 66, y, 20, none); sim.use(w, 74, y, 20, none);
    run(6);
    const bool trip = (stAux(w.getState(70, y, 20)) & 1) && (stAux(w.getState(78, y, 20)) & 1) && val(71, y, 20) == 0;
    check(trip, "FUSOR: back input 62 over threshold 10 trips it and the output drops to 0");
    sim.use(w, 66, y, 20, none); sim.use(w, 74, y, 20, none);  // input off
    run(4);
    sim.use(w, 70, y, 23, none); sim.use(w, 78, y, 23, none);  // side sources on
    run(30);
    check(!(stAux(w.getState(70, y, 20)) & 1), "FUSOR: a Repeator on the side resets it on its rising edge");
    check((stAux(w.getState(78, y, 20)) & 1) != 0, "FUSOR: a powered raw Trace on the side does not reset it");
  }

  // J) Memory cell: back = write, sides = clear, front = stored value.
  putF(30, y, 46, kMemor, E); put(29, y, 46, kRedTrace); put(30, y, 45, kRedTrace); put(31, y, 46, kBlueTrace);
  setW(29, y, 46, 17); run(1);
  check(val(30, y, 46) == 17, "memory cell stores a pulse of 17");
  run(3);
  check(val(30, y, 46) == 17 && val(31, y, 46) == 17, "memory cell holds 17 and outputs it continuously");
  setW(29, y, 46, 40); run(1);
  check(val(30, y, 46) == 40, "memory cell overwritten by a new value (40)");
  setW(30, y, 45, 5); run(1);
  check(val(30, y, 46) == 0, "memory cell cleared by a side pulse (0 = empty)");

  // K) Pulsor -> 38 wires -> memory cell: the pulse keeps its strength (no decay).
  putF(2, y, 50, kPulsor, E); put(1, y, 50, kRedTrace); put(2, y, 49, kBlueTrace);
  for (int x = 3; x <= 40; ++x) put(x, y, 50, kRedTrace);
  putF(41, y, 50, kMemor, E);
  setW(1, y, 50, 23); setW(2, y, 49, 10); run(1);
  check(val(3, y, 50) == 23 && val(40, y, 50) == 23 && stOn(w.getState(2, y, 50)), "pulsar: clock edge fires data 23 down 38 wires without decay");
  run(1);
  {  // 2026-11-01 (JD, PlayK): the pulse fires on one tick and stays high cPulseRules::holdTicks() ticks (default 4).
    const int hold = cPulseRules::holdTicks();
    bool highOk = val(41, y, 50) == 23;
    for (int t = 2; t <= hold; ++t) { highOk = highOk && val(20, y, 50) == 23; run(1); }
    std::printf("  [info] pulsor hold %d: after hold val %d\n", hold, val(20, y, 50));
    check(highOk && val(20, y, 50) == 0, "PULSOR HOLD: pulse stays high for exactly the hold window, then drops; memory cell stored 23");
  }
  {  // K2 (JD 2026-10-31): Pulsor reach caps at 64 Trace blocks; block 65+ never lights.
    // Test-setup fix (2026-10-31): z=80 put block 64 (66,80) outside the sim's 100-block tick scope
    // around (0,0); z=32 keeps the whole run (to x=72) in scope with free rows on both sides.
    putF(2, y, 32, kPulsor, E); put(1, y, 32, kRedTrace); put(2, y, 31, kBlueTrace);
    for (int x = 3; x <= 72; ++x) put(x, y, 32, kRedTrace);
    setW(1, y, 32, 23); setW(2, y, 31, 10); run(1);
    check(val(3, y, 32) == 23 && val(66, y, 32) == 23, "PULSOR RANGE: blocks 1 and 64 of the run carry the pulse");
    check(val(67, y, 32) == 0 && val(72, y, 32) == 0, "PULSOR RANGE: block 65 (and beyond) never lights");
    run(2);
  }
  {  // K3 (JD 2026-10-31; z=54/53: the test map is chunks 0..3, z<64, so z=70/90 had no blocks): a Pulsor pulse floods Trace only; a Thresholdor in the line is a component that
     // takes it as normal input on its own tick (diagnostic trace printed per tick).
    putF(2, y, 54, kPulsor, E); put(1, y, 54, kRedTrace); put(2, y, 53, kBlueTrace);
    for (int x = 3; x <= 10; ++x) put(x, y, 54, kRedTrace);
    putF(11, y, 54, kThresholdor, E);
    for (int x = 12; x <= 20; ++x) put(x, y, 54, kRedTrace);
    setW(1, y, 54, 23); setW(2, y, 53, 10);
    int frontAt0 = -1, backAt0 = -1;
    for (int t = 0; t < 5; ++t) {
      run(1);
      std::printf("  [K3] tick %d: data=%d clk=%d pul=0x%x wire3=%d wire10=%d thr=%d wire12=%d wire20=%d\n", t, val(1, y, 54), val(2, y, 53), unsigned(w.getState(2, y, 54)), val(3, y, 54), val(10, y, 54), val(11, y, 54), val(12, y, 54), val(20, y, 54));
      if (t == 0) { backAt0 = val(10, y, 54); frontAt0 = val(12, y, 54); }
    }
    check(backAt0 == 23, "PULSOR: the pulse reaches the Thresholdor's back wire");
    check(frontAt0 == 0, "PULSOR: the pulse never floods through a Thresholdor (front wire dark on the pulse tick)");
  }
  // Hold the clock high with a switch on the right side: one pulse on the edge, then nothing.
  w.set(2, y, 51, kSwitch); w.setState(2, y, 51, 0); sim.onBlockChanged(2, y, 51, kSwitch);
  run(2);
  sim.use(w, 2, y, 51, none);
  int pulses = 0;
  for (int i = 0; i < 6; ++i) { setW(1, y, 50, 9); run(1); pulses += val(20, y, 50) == 9; }
  check(pulses == cPulseRules::holdTicks(), "pulsar: clock held high fires once (rising edge only), carrying the new data (9) for the hold window");
  sim.use(w, 2, y, 51, none); run(2);
  sim.use(w, 2, y, 51, none);
  pulses = 0;
  for (int i = 0; i < 4; ++i) { setW(1, y, 50, 12); run(1); pulses += val(20, y, 50) == 12; }
  check(pulses == cPulseRules::holdTicks(), "pulsar: next rising clock edge fires again (12) for the hold window");

  // L) Fast latch: each rising edge at the back toggles the front (binary 64).
  putF(50, y, 46, kLatchor, E); put(49, y, 46, kRedTrace); put(51, y, 46, kRedTrace);
  // 2026-10-31 (JD): set to 44, the Latchor fires only on a tick whose input is exactly 44.
  w.setState(50, y, 46, makeState(44, E, 0, false));
  bool lOk = true;
  for (int in : {10, 43, 44, 45, 64, 1, 44, 0}) {
    setW(49, y, 46, in); run(1);
    const int out = val(51, y, 46);
    if (out != (in == 44 ? 44 : 0)) { lOk = false; std::printf("  [L] in %d -> out %d\n", in, out); }
  }
  check(lOk, "latchor set to 44: only an input of exactly 44 fires (44, that tick); 10/43/45/64/1 give nothing");

  // 2026-11-01 (JD soft power, LL_SOFT_POWER=0 off): a lit Trace soft-powers the solid block it touches;
  // that block lights an attached lamp but never drives Trace on its other faces.
  {
    static const bool soft = [] { const char* e = std::getenv("LL_SOFT_POWER"); return !(e && e[0] == '0'); }();
    put(90, y, 60, kSwitch); put(91, y, 60, kRedTrace); put(92, y, 60, kWaferSilicon); put(93, y, 60, kLamp);
    put(92, y, 61, kBlueTrace);  // touches only the soft-powered block
    sim.use(w, 90, y, 60, none); run(3);
    if (soft) {
      std::printf("  [info] soft: wire91 %d lamp93 st %u silicon solid/opaque/powerable %d%d%d\n", val(91, y, 60), unsigned(w.getState(93, y, 60)), int(blockRegistry().has(kWaferSilicon, kSolid)), int(blockRegistry().has(kWaferSilicon, kOpaque)), int(blockRegistry().has(kWaferSilicon, kPowerable)));
      check(w.getState(93, y, 60) == 1, "soft power: Trace-touched block lights the lamp beside it");
      check(val(92, y, 61) == 0, "soft power: a soft-powered block never drives Trace");
      sim.use(w, 90, y, 60, none); run(3);
      check(w.getState(93, y, 60) == 0, "soft power: lamp goes dark when the Trace goes dark");
      // Memor front output soft-powers the block in front (z 63 row: free in this world, floor x<96 z<64).
      put(90, y, 63, kRedTrace); putF(91, y, 63, kMemor, E); put(92, y, 63, kWaferSilicon); put(93, y, 63, kLamp);
      const bool memDark = w.getState(93, y, 63) == 0;
      setW(90, y, 63, 17); run(24);  // PlayN: Memor delayTicks=8
      // PlayM (JD 12:40, option A): with LL_HARD_ANY=0 only a Repeator powers the block it faces, so the lamp stays dark.
      if (cPulseRules::hardAnyComponent()) check(memDark && w.getState(93, y, 63) == 1, "soft power: Memor output (17) soft-powers the block in front; lamp beside it lights");
      else check(memDark && w.getState(93, y, 63) == 0, "soft power off for components (LL_HARD_ANY=0): Memor front block unpowered; lamp beside it stays dark");
      // 2026-11-01 PlayL (JD, LL_HARD_ANY): a Memor output facing the block HARD-powers it, so Trace on top lights.
      put(92, y + 1, 63, kRedTrace); run(24);  // PlayN: Memor delay + hard seed
      std::printf("  [info] hard memor: block-top trace %d\n", val(92, y + 1, 63));
      if (cPulseRules::hardAnyComponent()) check(val(92, y + 1, 63) == 17, "HARD power: Memor (17) -> block -> Trace on the block lights at strength 17");
      else check(val(92, y + 1, 63) == 0, "HARD power off (LL_HARD_ANY=0): Memor front block stays soft, Trace on it stays dark");
    }
  }

  // M) Switch and button.
  put(60, y, 46, kSwitch); put(61, y, 46, kRedTrace); put(59, y, 46, kBlueTrace);
  sim.use(w, 60, y, 46, none); run(1);
  check(val(61, y, 46) == 64 && val(59, y, 46) == 64, "switch on powers wire on every side (64)");
  sim.use(w, 60, y, 46, none); run(1);
  check(val(61, y, 46) == 0, "switch off");
  put(70, y, 46, kButton); put(71, y, 46, kRedTrace);
  sim.use(w, 70, y, 46, none);
  int pressed = 0;
  for (int i = 0; i < 20; ++i) { run(1); pressed += val(71, y, 46) > 0; }
  check(pressed == kButtonTicks, "button pulses for 10 ticks");

  // N) Keyboard: SIXBIT char pulses out the front (code + 1), Enter/Backspace out the right.
  putF(80, y, 46, kKeyboard, E); put(81, y, 46, kRedTrace); put(80, y, 47, kBlueTrace);
  check(sixbitCode('A') == 33 && sixbitCode('a') == 33 && sixbitCode('0') == 16 && sixbitCode(' ') == 0 &&
        sixbitCode('_') == 63 && sixbitCode('~') == -1 && sixbitChar(33) == 'A', "SIXBIT mapping (A=33, 0=16, space=0, _=63)");
  sim.use(w, 80, y, 46, none);
  check(stOn(w.getState(80, y, 46)), "right-click toggles keyboard typing capture");
  sim.keyPulse(80, y, 46, sixbitCode('A') + 1, 0); run(1);
  const int kA = val(81, y, 46);
  run(1);
  const int kA2 = val(81, y, 46);
  sim.keyPulse(80, y, 46, 0, kKeyEnter); run(1);
  check(kA == 34 && kA2 == 0, "keyboard: 'A' = 1-tick pulse of 34 out the front");
  check(val(80, y, 47) == 1 && val(81, y, 46) == 0, "keyboard: Enter = 1 on the right control port");

  // O) Color lamps: wrap mapping, terminate the signal; plain lamp lit by a component.
  put(0, y, 56, kSwitch); for (int x = 1; x <= 4; ++x) put(x, y, 56, kRedTrace);
  put(5, y, 56, kColorLamp16); put(6, y, 56, kRedTrace);
  put(0, y, 58, kSwitch); for (int x = 1; x <= 4; ++x) put(x, y, 58, kRedTrace); put(5, y, 58, kColorLamp64);
  put(90, y, 56, kSwitch); put(91, y, 56, kLamp);
  sim.use(w, 0, y, 56, none); sim.use(w, 0, y, 58, none); sim.use(w, 90, y, 56, none);
  run(3);
  check(val(5, y, 56) == 61 && colorLampIndex(61, 16) == 12 && colorLampIndex(61, 64) == 60 && val(5, y, 58) == 61,
        "color lamps take strength 61: tier 16 -> colour 12 (wrap), tier 64 -> 60");
  check(colorLampIndex(0, 16) == -1 && colorLampIndex(1, 16) == 0 && colorLampIndex(17, 16) == 0 && colorLampIndex(33, 32) == 0,
        "wrap mapping: 0 off, 1 = first colour, N+1 wraps");
  check(val(6, y, 56) == 0, "lamps terminate the signal (wire behind a color lamp stays 0)");
  check(val(91, y, 56) == 1, "plain lamp lit by an adjacent switch");
  // P) Tone Block: back strength = pitch; a Tone cue on every change, 0 = silent; consumes the signal.
  {
    std::vector<SoundEvent> ev;
    sim.takeSounds(ev);  // everything since the start: the pulsor tests fired with data 23, 9 and 12
    int fired = 0;
    for (const SoundEvent& e : ev) fired += e.cue == SoundCue::PulsorFire && (e.value == 23 || e.value == 9 || e.value == 12);
    // 2026-10-31: + the K2 range Pulsor (data 23), now inside the tick scope -> 4 fires; + the K3 Pulsor (23) on the map at z=54 -> 5.
    check(fired == 5, "pulsar: every fire reports a PulsorFire sound cue carrying its data strength");
    ev.clear();
    putF(41, y, 60, kToneBlock, E); put(40, y, 60, kRedTrace); put(42, y, 60, kRedTrace);
    for (int i = 0; i < 3; ++i) { setW(40, y, 60, 34); run(1); }
    sim.takeSounds(ev);
    const bool on = ev.size() == 1 && ev[0].cue == SoundCue::Tone && ev[0].value == 34 && ev[0].x == 41 && val(42, y, 60) == 0;
    ev.clear();
    run(2);
    sim.takeSounds(ev);
    check(on && ev.size() == 1 && ev[0].value == 0 && val(41, y, 60) == 0,
          "tone block: s=34 -> one Tone cue (A4), held without repeats; 0 -> silent cue; output stays 0");
  }
  sim.setSameTick(keepSameTick);  // PlayM: settle back on after the injected-wire sections
  {  // 2026-11-01 (Archie list): compiled in-range component table - only components inside the tick area
     // are in the table, and nothing out of range or non-component is ever dispatched.
    ChunkMap tw;
    tw.setRegistry(&blockRegistry());
    for (int cx = 0; cx < 4; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = 0; tw.insert(std::move(col)); }
    tw.set(2, y, 2, kLatchor); tw.set(50, y, 2, kLatchor); tw.set(3, y, 2, kRedTrace); tw.set(4, y, 2, kWaferSilicon);
    cSignalSim ts;
    ts.scan(tw);
    for (int i = 0; i < 8; ++i) ts.tick(tw, 0, 0, 1, [](int, int, int) {});
    const size_t inRange = ts.compTableSize(); const uint64_t rebuilds = ts.compTableRebuilds();
    std::printf("  [comp table] in-range %zu, rebuilds %llu, out-of-range dispatches %llu (sim total %llu)\n", inRange,
                (unsigned long long)rebuilds, (unsigned long long)ts.compOutOfRangeDispatches(), (unsigned long long)sim.compOutOfRangeDispatches());
    const char* e = std::getenv("LL_COMP_TABLE");
    if (!(e && e[0] == '0'))
      check(inRange == 1 && rebuilds <= 2 && ts.compOutOfRangeDispatches() == 0 && sim.compOutOfRangeDispatches() == 0,
            "comp table: only the in-range component is listed (1 of 2), no per-tick rebuild, zero out-of-range dispatches");
  }
  std::printf("%s (%d failure%s)\n", fails ? "SELFTEST FAILED" : "SELFTEST OK", fails, fails == 1 ? "" : "s");
  {  // Attachables: a switch on top of a block powers the block (lamp beside it) and the dust
     // directly below the block; a wall torch turns off when its support is powered.
    auto putA = [&](int x, int yy, int z, BlockId b, int face) {
      w.set(x, yy, z, b); w.setState(x, yy, z, withAttach(0, face)); sim.onBlockChanged(x, yy, z, b);
    };
    put(82, y + 2, 60, kWaferSilicon);
    putA(82, y + 3, 60, kSwitch, 1);
    put(82, y + 1, 60, kRedTrace);
    put(83, y + 2, 60, kLamp);
    put(86, y, 60, kWaferSilicon);
    putA(85, y, 60, kRedTorch, 6);
    putA(86, y + 1, 60, kSwitch, 1);
    run(3);
    const bool before = val(82, y + 1, 60) == 0 && w.getState(83, y + 2, 60) == 0 && stValue(w.getState(85, y, 60)) == 1;
    sim.use(w, 82, y + 3, 60, none);
    sim.use(w, 86, y + 1, 60, none);
    run(4);
    check(before && val(82, y + 1, 60) == 64 && w.getState(83, y + 2, 60) == 1,
          "attached switch on top powers its support (lamp beside it) and the dust below the support");
    check(stValue(w.getState(85, y, 60)) == 0 && stAttach(w.getState(85, y, 60)) == 6 && stAttach(w.getState(82, y + 3, 60)) == 1,
          "wall torch turns off when its support is powered; attach faces survive ticks and toggles");
  }
  {  // 2026-10-24 (JD): a floor-mounted switch also drives the Trace beside it, red and blue alike.
    for (int x = 40; x <= 42; ++x) put(x, y - 1, 62, kWaferSilicon);
    w.set(41, y, 62, kSwitch); w.setState(41, y, 62, withAttach(0, 1)); sim.onBlockChanged(41, y, 62, kSwitch);
    put(42, y, 62, kRedTrace);
    put(40, y, 62, kBlueTrace);
    run(3);
    const bool off = w.get(41, y, 62) == kSwitch && val(42, y, 62) == 0 && val(40, y, 62) == 0;
    sim.use(w, 41, y, 62, none);
    run(4);
    check(off && val(42, y, 62) == 64 && val(40, y, 62) == 64, "attached switch powers red and blue Trace on its own sides (64)");
  }
  {  // 2026-10-31 (JD): a switch no longer powers Trace on its diagonals (was 2026-10-24 5:57 PM).
    for (int x = 44; x <= 45; ++x) for (int z = 61; z <= 62; ++z) put(x, y - 1, z, kWaferSilicon);
    w.set(44, y, 62, kSwitch); w.setState(44, y, 62, withAttach(0, 1)); sim.onBlockChanged(44, y, 62, kSwitch);
    put(45, y, 61, kRedTrace);
    run(3);
    const bool off = val(45, y, 61) == 0;
    sim.use(w, 44, y, 62, none);
    run(4);
    check(off && val(45, y, 61) == 0, "attached switch does NOT power a diagonal Trace (orthogonal only, JD 2026-10-31)");
  }
  {  // 2026-10-24 lever: on each of the 6 faces (floor / ceiling in all 4 facings) the switch keeps
     // its attach face and facing through ticks and toggles and powers its support (lamp beside it).
    const int sup[3] = {4, y + 4, 48};
    int ok = 0, total = 0;
    for (int face = 1; face <= 6; ++face)
      for (int f = 0; f < ((face <= 2) ? 4 : 1); ++f) {
        const int bx = sup[0] + 4 * total, by = sup[1], bz = sup[2];
        int dx, dy, dz;
        attachOffset(face, dx, dy, dz);
        put(bx, by, bz, kWaferSilicon);
        const int sx = bx - dx, sy = by - dy, sz = bz - dz;  // the switch cell: support is at +offset
        w.set(sx, sy, sz, kSwitch); w.setState(sx, sy, sz, withAttach(makeState(0, f), face)); sim.onBlockChanged(sx, sy, sz, kSwitch);
        const int lx = bx + (dx == 0 ? 1 : 0), lz = bz + (dx == 0 ? 0 : 1);
        put(lx, by, lz, kLamp);
        run(2);
        const bool off = w.getState(lx, by, lz) == 0;
        sim.use(w, sx, sy, sz, none);
        run(3);
        const CellState s = w.getState(sx, sy, sz);
        ++total;
        if (off && w.getState(lx, by, lz) == 1 && stOn(s) && stAttach(s) == face && stFacing(s) == f) ++ok;
      }
    check(ok == total && total == 12, "lever: all 6 faces (floor + ceiling x 4 facings) power their support, keep attach face + facing");
    const cLithoKraftAppearance look;
    const BlockDef& sd = blockRegistry().def(kSwitch);
    // 2026-11-05 (E2 components-v2): floor Switch uses .lthm on/off meshes (boxModel returns nullptr);
    // wall/ceiling and LL_COMPONENTS_V2=0 keep separate ll-box-1 on/off models.
    const bool v2lever = cComponentModels::instance().find("switch_on") && cComponentModels::instance().find("switch_off");
    const bool separate = v2lever || look.boxModel(sd, makeState(0, 0, 0, true)) != look.boxModel(sd, makeState(0));
    check(separate && look.boxModelTurns(sd, makeState(0, 3)) == 3,
          "lever: separate on / off box models; floor / ceiling turn follows the facing");
  }
  {  // Attached button (on a wall): pulses its support for kButtonTicks; the lamp beside it follows.
    put(88, y + 2, 60, kWaferSilicon);
    w.set(87, y + 2, 60, kButton); w.setState(87, y + 2, 60, withAttach(0, 6)); sim.onBlockChanged(87, y + 2, 60, kButton);
    put(88, y + 3, 60, kLamp);
    run(2);
    const bool idle = w.getState(88, y + 3, 60) == 0;
    sim.use(w, 87, y + 2, 60, none);
    run(3);
    const bool lit = w.getState(88, y + 3, 60) == 1 && stAttach(w.getState(87, y + 2, 60)) == 6;
    run(kButtonTicks + 4);
    check(idle && lit && w.getState(88, y + 3, 60) == 0, "attached button pulses its support (lamp on, then off) and keeps its attach face");
    const cLithoKraftAppearance look;
    check(look.boxModel(blockRegistry().def(kButton)) != nullptr && look.boxModel(blockRegistry().def(kSwitch)) != nullptr,
          "button and switch have ll-box-1 box models");
  }
  {  // Display bench scene: the ring clock starts and keeps firing the cache banks into the lamps.
    ChunkMap dw;
    dw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 4; ++cz)
      for (int cx = 0; cx < 4; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; dw.insert(std::move(col)); }
    buildDisplayScene(dw, 0, y, 0, 40);
    cSignalSim ds;
    ds.scan(dw);
    int litTicks = 0, edges = 0; bool prev = false;
    for (int i = 0; i < 800; ++i) {  // 2026-10-24: delays doubled (4/8/16/32), was 400
      ds.tick(dw, 0, 0, 100, [](int, int, int) {});
      const bool on = stValue(dw.getState(20, y, 1 + 5 + 2)) > 0;  // a lamp in bank 0
      litTicks += on; edges += on && !prev; prev = on;
    }
    std::printf("  display scene: lamp lit %d / 800 ticks, %d flashes\n", litTicks, edges);
    check(edges >= 8, "display scene: ring clock runs and the bank fires its stored colour repeatedly");
  }
  {  // 2026-11-01 PlayL (JD regression): a Repeator ring clock that runs THROUGH a hard-powered block
     // (Repeator -> silicon -> Repeator back) keeps running; the lamp tap keeps flashing.
    ChunkMap cw;
    cw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 2; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; cw.insert(std::move(col)); }
    for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) cw.set(x, y - 1, z, kWaferSilicon);
    buildRingClock(cw, 2, y, 2, 0);
    cw.set(2 + 4 + 2, y, 2 + 1, kWaferSilicon);  // ring trace after the east Repeator -> a solid block
    cSignalSim cs;
    cs.scan(cw);
    int rises = 0; bool prev = false;
    for (int i = 0; i < 800; ++i) {
      cs.tick(cw, 0, 0, 100, [](int, int, int) {});
      const bool on = cw.getState(2 + 4, y, 2 + 1 + 4) == 1;
      if (on && !prev) ++rises;
      prev = on;
    }
    std::printf("  [info] repeator clock through block: lamp rises %d / 800 ticks\n", rises);
    check(rises >= 4, "REGRESSION: Repeator clock through a hard-powered block keeps running (lamp tap flashes)");
  }
  {  // 2026-11-01 PlayL (JD): hard power carries STRENGTH; the strongest hard source on a block wins.
    ChunkMap cw;
    cw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 2; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; cw.insert(std::move(col)); }
    for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) cw.set(x, y - 1, z, kWaferSilicon);
    // Memor A (stores 9) faces E into the block at (6, 4); Memor B (stores 21) faces W into it from (7, 4).
    cw.set(5, y, 4, kMemor); cw.setState(5, y, 4, makeState(9, 1));
    cw.set(7, y, 4, kMemor); cw.setState(7, y, 4, makeState(21, 3));
    cw.set(6, y, 4, kWaferSilicon);
    cw.set(6, y + 1, 4, kRedTrace);  // Trace on top of the block
    cSignalSim cs;
    cs.scan(cw);
    for (int i = 0; i < 24; ++i) cs.tick(cw, 0, 0, 100, [](int, int, int) {});  // PlayN: Memor delay 8
    const int v = stValue(cw.getState(6, y + 1, 4));
    std::printf("  [info] hard strength: block-top trace %d (expect 21)\n", v);
    if (cPulseRules::hardAnyComponent()) check(v == 21, "HARD power strength: two Memors (9, 21) into one block; Trace on it carries 21 (strongest wins)");
  }
  // ---- 2026-11-01 PlayL (JD 7:45 / 7:48): same-tick soft power; Trace connection states ----
  auto plWorld = [&](ChunkMap& cw) {
    cw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 2; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; cw.insert(std::move(col)); }
    for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) cw.set(x, y - 1, z, kWaferSilicon);
  };
  const cSignalSim::DirtyFn plNone = [](int, int, int) {};
  {  // the lamp on a soft-powered block lights on the SAME tick as the Trace (no one-tick soft lag), and goes dark with it
    ChunkMap cw; plWorld(cw);
    cw.set(2, y, 8, kSwitch); cw.set(3, y, 8, kRedTrace); cw.set(4, y, 8, kWaferSilicon); cw.set(5, y, 8, kLamp);
    cSignalSim cs; cs.scan(cw);
    for (int i = 0; i < 3; ++i) cs.tick(cw, 0, 0, 100, plNone);
    cs.use(cw, 2, y, 8, plNone);
    int traceT = -1, lampT = -1;
    for (int i = 0; i < 8; ++i) {
      cs.tick(cw, 0, 0, 100, plNone);
      if (traceT < 0 && stValue(cw.getState(3, y, 8)) > 0) traceT = i;
      if (lampT < 0 && cw.getState(5, y, 8) == 1) lampT = i;
    }
    cs.use(cw, 2, y, 8, plNone);
    int traceOff = -1, lampOff = -1;
    for (int i = 0; i < 8; ++i) {
      cs.tick(cw, 0, 0, 100, plNone);
      if (traceOff < 0 && stValue(cw.getState(3, y, 8)) == 0) traceOff = i;
      if (lampOff < 0 && cw.getState(5, y, 8) == 0) lampOff = i;
    }
    std::printf("  [info] same-tick soft: trace on t%d lamp on t%d; trace off t%d lamp off t%d\n", traceT, lampT, traceOff, lampOff);
    check(traceT >= 0 && lampT == traceT, "SAME TICK: Trace -> soft block -> lamp lights on the tick the Trace lights");
    check(traceOff >= 0 && lampOff == traceOff, "SAME TICK: the soft lamp goes dark on the tick the Trace goes dark");
  }
  {  // same tick, hard: switch -> Trace -> Memor -> block; Trace on the block and the block's lamp share one tick
    ChunkMap cw; plWorld(cw);
    cw.set(1, y, 11, kSwitch); cw.set(2, y, 11, kRedTrace); cw.set(3, y, 11, kMemor); cw.setState(3, y, 11, makeState(0, 1));
    cw.set(4, y, 11, kWaferSilicon); cw.set(4, y + 1, 11, kRedTrace); cw.set(5, y, 11, kLamp);
    cSignalSim cs; cs.scan(cw);
    for (int i = 0; i < 3; ++i) cs.tick(cw, 0, 0, 100, plNone);
    cs.use(cw, 1, y, 11, plNone);
    int topT = -1, lampT = -1;
    for (int i = 0; i < 8; ++i) {
      cs.tick(cw, 0, 0, 100, plNone);
      if (topT < 0 && stValue(cw.getState(4, y + 1, 11)) > 0) topT = i;
      if (lampT < 0 && cw.getState(5, y, 11) == 1) lampT = i;
    }
    std::printf("  [info] same-tick hard: block-top trace t%d (%d) lamp t%d\n", topT, stValue(cw.getState(4, y + 1, 11)), lampT);
    if (cPulseRules::hardAnyComponent()) check(topT >= 0 && lampT == topT, "SAME TICK: hard block -> its Trace and its lamp update on the same tick");
  }
  {  // connection states: Alt-use cycles auto -> 1 .. 63 -> auto (64 states); 63 steps = one step back; strength bits kept
    ChunkMap cw; plWorld(cw);
    cw.set(3, y, 12, kRedTrace); cw.setState(3, y, 12, makeState(5));
    cSignalSim cs; cs.scan(cw);
    bool seq = cTraceConn::index(cw.getState(3, y, 12)) == 0;
    for (int i = 1; i <= 64; ++i) {
      cs.use(cw, 3, y, 12, plNone, true);
      seq = seq && cTraceConn::index(cw.getState(3, y, 12)) == (i % 64) && stValue(cw.getState(3, y, 12)) == 5;
    }
    cs.use(cw, 3, y, 12, plNone, true); cs.use(cw, 3, y, 12, plNone, true);  // -> 2
    for (int i = 0; i < 63; ++i) cs.use(cw, 3, y, 12, plNone, true);          // 63 steps = back one -> 1
    const bool back = cTraceConn::index(cw.getState(3, y, 12)) == 1;
    if (cTraceConn::enabled()) check(seq && back, "TRACE CONN: Alt-use cycles auto,1..63,auto (64 states); 63 steps = one back; strength kept");
    // save / load: the state lives in the cell and survives the column codec roundtrip
    cs.use(cw, 3, y, 12, plNone, true); cs.use(cw, 3, y, 12, plNone, true);  // -> 3 (+X, -X)
    storage::cPayloadReg codec; codec.setRegistry(&blockRegistry());
    std::string err;
    const auto rec = codec.encode(*cw.column(0, 0));
    const auto back2 = codec.decode(rec, &err);
    const CellState rs = back2 ? back2->subs[size_t(y >> 4)]->state[size_t(SubChunk::index(3, y & 15, 12))] : CellState(0);
    std::printf("  [info] trace conn roundtrip: before %d after %d (err '%s')\n", cTraceConn::index(cw.getState(3, y, 12)), cTraceConn::index(rs), err.c_str());
    if (cTraceConn::enabled()) check(back2 && cTraceConn::index(rs) == 3 && rs == cw.getState(3, y, 12), "TRACE CONN: connection state saves and loads with the world (column codec roundtrip)");
  }
  if (cTraceConn::enabled()) {  // a disconnected side neither soft-powers its block nor links wire / components
    ChunkMap cw; plWorld(cw);
    // row z 14: switch -> Trace (connects -X only) -> block -> lamp: Trace lights, block NOT soft-powered.
    cw.set(2, y, 14, kSwitch); cw.set(3, y, 14, kRedTrace); cw.set(4, y, 14, kWaferSilicon); cw.set(5, y, 14, kLamp);
    cw.setState(3, y, 14, cTraceConn::withIndex(0, 1 << 1));
    // row z 2: switch -> Trace A (auto) -> Trace B (connects +X only: not toward A) -> Trace C
    cw.set(2, y, 2, kSwitch); cw.set(3, y, 2, kRedTrace); cw.set(4, y, 2, kRedTrace); cw.set(5, y, 2, kRedTrace);
    cw.setState(4, y, 2, cTraceConn::withIndex(0, 1 << 0));
    // row z 5: switch -> Trace D (connects +X only: not toward the switch) -> Memor input never sees it
    cw.set(2, y, 5, kSwitch); cw.set(3, y, 5, kRedTrace); cw.setState(3, y, 5, cTraceConn::withIndex(0, 1 << 0));
    cSignalSim cs; cs.scan(cw);
    cs.use(cw, 2, y, 14, plNone); cs.use(cw, 2, y, 2, plNone); cs.use(cw, 2, y, 5, plNone);
    for (int i = 0; i < 6; ++i) cs.tick(cw, 0, 0, 100, plNone);
    std::printf("  [info] trace conn: z14 trace %d lamp %u | z2 A %d B %d C %d | z5 D %d\n", stValue(cw.getState(3, y, 14)), unsigned(cw.getState(5, y, 14)),
                stValue(cw.getState(3, y, 2)), stValue(cw.getState(4, y, 2)), stValue(cw.getState(5, y, 2)), stValue(cw.getState(3, y, 5)));
    check(stValue(cw.getState(3, y, 14)) > 0 && cw.getState(5, y, 14) == 0, "TRACE CONN: a disconnected side does not soft-power its block (lamp stays dark)");
    check(stValue(cw.getState(3, y, 2)) > 0 && stValue(cw.getState(4, y, 2)) == 0 && stValue(cw.getState(5, y, 2)) == 0, "TRACE CONN: a disconnected side does not link Trace");
    check(stValue(cw.getState(3, y, 5)) == 0, "TRACE CONN: a disconnected side does not link a component (switch)");
    // back to auto (63 more steps from index 2 on the z14 Trace = 1; one more = auto... cycle to 0): the lamp lights
    while (cTraceConn::index(cw.getState(3, y, 14)) != 0) cs.use(cw, 3, y, 14, plNone, true);
    for (int i = 0; i < 6; ++i) cs.tick(cw, 0, 0, 100, plNone);
    check(cw.getState(5, y, 14) == 1, "TRACE CONN: auto again -> the block is soft-powered, lamp lights");
    // PlayN (JD 13:06): a custom state whose side points at a solid block soft-powers that block. -X (switch) + +X
    // (block) = mask 0b11: the Trace stays linked to the switch and soft-powers the block at +X; lamp lights.
    while (cTraceConn::index(cw.getState(3, y, 14)) != 3) cs.use(cw, 3, y, 14, plNone, true);
    for (int i = 0; i < 6; ++i) cs.tick(cw, 0, 0, 100, plNone);
    check(stValue(cw.getState(3, y, 14)) > 0 && cw.getState(5, y, 14) == 1, "TRACE CONN: a custom side toward a solid block soft-powers it (lamp lights)");
    // and -X only again (mask 0b10): block side off -> soft power drops, lamp dark.
    while (cTraceConn::index(cw.getState(3, y, 14)) != 2) cs.use(cw, 3, y, 14, plNone, true);
    for (int i = 0; i < 6; ++i) cs.tick(cw, 0, 0, 100, plNone);
    check(stValue(cw.getState(3, y, 14)) > 0 && cw.getState(5, y, 14) == 0, "TRACE CONN: turning the block side off again un-powers the block (lamp dark)");
  }
  {  // PlayN (JD 13:13): a component's LEFT / RIGHT side input reads a solid block's soft power with strength.
     // Row: switch(4) -> Trace(5) = 64 -> Thresholdor(6, E, subtract) -> Trace(7). Side block at (6, z+sd), soft-
     // powered by a Trace run switch(3) -> (4..6, z+2sd) = 62 at (6, z+2sd). Subtract: 64 - 62 = 2 on the front Trace.
    auto sideTest = [&](int z, int sd) {
      ChunkMap cw; plWorld(cw);
      cw.set(4, y, z, kSwitch); cw.set(5, y, z, kRedTrace); cw.set(6, y, z, kThresholdor); cw.setState(6, y, z, makeState(0, 1)); cw.set(7, y, z, kRedTrace);
      cw.set(6, y, z + sd, kWaferSilicon);
      cw.set(3, y, z + 2 * sd, kSwitch); for (int x = 4; x <= 6; ++x) cw.set(x, y, z + 2 * sd, kRedTrace);
      cSignalSim cs; cs.scan(cw);
      cs.use(cw, 6, y, z, plNone);  // subtract mode
      cs.use(cw, 4, y, z, plNone); cs.use(cw, 3, y, z + 2 * sd, plNone);
      for (int i = 0; i < 8; ++i) cs.tick(cw, 0, 0, 100, plNone);
      const int sp = stValue(cw.getState(6, y, z + 2 * sd)), out = stValue(cw.getState(7, y, z));  // block soft = its Trace
      std::printf("  [info] side block (sd %d): side trace %d thr out %d\n", sd, sp, out);
      return sp > 0 && sp < 64 && out == 64 - sp;
    };
    check(sideTest(12, -1), "SIDE BLOCK: Thresholdor subtract reads a soft-powered block on its left side with strength (64 - 62 = 2)");
    check(sideTest(3, 1), "SIDE BLOCK: Thresholdor subtract reads a soft-powered block on its right side with strength (64 - 62 = 2)");
  }
  // ---- 2026-11-01 PlayM (JD): same-tick components (components read THIS tick's wire values) ----
  {  // switch -> Trace -> Memor -> Trace -> Memor -> Trace -> lamp: every Trace and the lamp light on ONE tick
    ChunkMap cw; plWorld(cw);
    cw.set(1, y, 3, kSwitch); cw.set(2, y, 3, kRedTrace);
    cw.set(3, y, 3, kMemor); cw.setState(3, y, 3, makeState(0, 1)); cw.set(4, y, 3, kRedTrace);
    cw.set(5, y, 3, kMemor); cw.setState(5, y, 3, makeState(0, 1)); cw.set(6, y, 3, kRedTrace); cw.set(7, y, 3, kLamp);
    auto runChain = [&](ChunkMap& m, std::vector<int>& onT, std::vector<uint64_t>& trace) {
      cSignalSim cs; cs.scan(m);
      for (int i = 0; i < 3; ++i) cs.tick(m, 0, 0, 100, plNone);
      cs.use(m, 1, y, 3, plNone);
      onT.assign(4, -1);
      for (int i = 0; i < 10; ++i) {
        cs.tick(m, 0, 0, 100, plNone);
        const int v[4] = {stValue(m.getState(2, y, 3)), stValue(m.getState(4, y, 3)), stValue(m.getState(6, y, 3)), int(m.getState(7, y, 3) == 1)};
        uint64_t hsh = 1469598103934665603ull;
        for (int x = 1; x <= 7; ++x) hsh = (hsh ^ m.getState(x, y, 3)) * 1099511628211ull;
        trace.push_back(hsh);
        for (int j = 0; j < 4; ++j) if (onT[size_t(j)] < 0 && v[j] > 0) onT[size_t(j)] = i;
      }
      return cs.settleIterations();
    };
    std::vector<int> a, b; std::vector<uint64_t> ta, tb;
    ChunkMap cw2; plWorld(cw2);
    for (int x = 1; x <= 7; ++x) { cw2.set(x, y, 3, cw.get(x, y, 3)); cw2.setState(x, y, 3, cw.getState(x, y, 3)); }
    const uint64_t it = runChain(cw, a, ta);
    runChain(cw2, b, tb);
    std::printf("  [info] same-tick comps: trace2 t%d trace4 t%d trace6 t%d lamp t%d (settle iters %llu)\n", a[0], a[1], a[2], a[3], (unsigned long long)it);
    static const bool same = [] { const char* e = std::getenv("LL_SAME_TICK"); return !(e && e[0] == '0'); }();
    if (same) check(a[0] >= 0 && a[1] == a[0] && a[2] == a[0] && a[3] == a[0], "SAME TICK: switch -> Trace -> Memor -> Trace -> Memor -> Trace -> lamp all update on ONE tick");
    else check(a[0] >= 0 && a[3] > a[0], "SAME TICK off (LL_SAME_TICK=0): chain takes one tick per component");
    check(a == b && ta == tb, "SAME TICK: deterministic (two sims, identical per-tick state hashes)");
  }
  {  // intended delay kept: a fresh Repeator (delay 4) still delays; lag input -> output stays its delay
    ChunkMap cw; plWorld(cw);
    cw.set(1, y, 6, kSwitch); cw.set(2, y, 6, kRedTrace); cw.set(3, y, 6, kRepeator); cw.setState(3, y, 6, makeState(0, 1));
    cw.set(4, y, 6, kBlueTrace); cw.set(5, y, 6, kLamp);
    cSignalSim cs; cs.scan(cw);
    for (int i = 0; i < 3; ++i) cs.tick(cw, 0, 0, 100, plNone);
    cs.use(cw, 1, y, 6, plNone);
    int inT = -1, outT = -1, lampT = -1;
    for (int i = 0; i < 16; ++i) {
      cs.tick(cw, 0, 0, 100, plNone);
      if (inT < 0 && stValue(cw.getState(2, y, 6)) > 0) inT = i;
      if (outT < 0 && stValue(cw.getState(4, y, 6)) > 0) outT = i;
      if (lampT < 0 && cw.getState(5, y, 6) == 1) lampT = i;
    }
    std::printf("  [info] same-tick repeator: in t%d out t%d lamp t%d (lag %d)\n", inT, outT, lampT, outT - inT);
    check(inT >= 0 && outT - inT >= 3 && lampT == outT, "SAME TICK: Repeator keeps its delay (lag >= 3 ticks at fresh delay 4); its lamp lights with its output Trace");
  }
  {  // PlayM: CloudAtlas six-button models load from assets (repeator_d2..d64.lthm), no placeholder fallback
    const std::string errs = foliage::repeatorModelLoadErrors();
    bool all = errs.empty();
    for (int i = 0; i < foliage::kRepeatorStates; ++i) all = all && foliage::repeatorMesh(i).fromAsset;
    if (!errs.empty()) std::printf("  [info] repeator model load: %s\n", errs.c_str());
    check(all, "REPEATOR six-button models: all 6 states load from assets/models/repeator (.lthm)");
  }
  {  // PlayM (JD 10:08 AM): every Repeator setting delays by exactly its labelled ticks (2/4/8/16/32/64),
     // relative to the 4-tick setting (the same circuit; only the delay differs), and a saved state round-trips.
    int lag[repeator::kSettings] = {};
    bool allOk = true;
    for (int sIdx = 0; sIdx < repeator::kSettings; ++sIdx) {
      ChunkMap cw; plWorld(cw);
      cw.set(1, y, 6, kSwitch); cw.set(2, y, 6, kRedTrace); cw.set(3, y, 6, kRepeator); cw.setState(3, y, 6, repeator::encode(0, 1, sIdx, false));
      cw.set(4, y, 6, kRedTrace);
      cSignalSim cs; cs.scan(cw);
      for (int i = 0; i < 3; ++i) cs.tick(cw, 0, 0, 100, plNone);
      cs.use(cw, 1, y, 6, plNone);
      int inT = -1, outT = -1;
      for (int i = 0; i < 100 && outT < 0; ++i) {
        cs.tick(cw, 0, 0, 100, plNone);
        if (inT < 0 && stValue(cw.getState(2, y, 6)) > 0) inT = i;
        if (outT < 0 && stValue(cw.getState(4, y, 6)) > 0) outT = i;
      }
      lag[sIdx] = (inT >= 0 && outT >= 0) ? outT - inT : -1;
      allOk = allOk && lag[sIdx] >= 0 && repeator::delayTicks(cw.getState(3, y, 6)) == repeator::kDelays[sIdx];
    }
    for (int sIdx = 0; sIdx < repeator::kSettings; ++sIdx) {
      const bool ok = allOk && lag[sIdx] - lag[1] == repeator::kDelays[sIdx] - 4;
      std::printf("  [info] repeator setting %d: delay %d ticks, lag %d\n", sIdx + 1, repeator::kDelays[sIdx], lag[sIdx]);
      check(ok, std::format("REPEATOR {}-tick setting: output lags exactly {} ticks more than the 4-tick setting", repeator::kDelays[sIdx], repeator::kDelays[sIdx] - 4).c_str());
    }
    // save compatibility: a PlayL-era new-encoding state (bit 15 clear, aux 0..3) keeps its exact delay
    bool compat = true;
    for (int raw = 0; raw < 4; ++raw) compat = compat && repeator::delayTicks(CellState(makeState(0, 1, raw) | repeator::kNewEncoding)) == (4 << raw);
    for (int sIdx = 0; sIdx < repeator::kSettings; ++sIdx) compat = compat && repeator::delayIndex(repeator::encode(0, 2, sIdx, true)) == sIdx;
    check(compat, "REPEATOR save compat: old new-encoding raw 0..3 = 4/8/16/32 ticks (unchanged); every setting encodes and decodes");
  }
  {  // 2026-10-24 test area Clocks pad: each ring clock self-runs; the latch toggles once per lap.
    for (int di = 0; di < repeator::kSettings; ++di) {  // PlayM: all six settings (2..64)
      ChunkMap cw;
      cw.setRegistry(&blockRegistry());
      for (int cz = 0; cz < 2; ++cz)
        for (int cx = 0; cx < 2; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; cw.insert(std::move(col)); }
      for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) cw.set(x, y - 1, z, kWaferSilicon);
      buildRingClock(cw, 2, y, 2, di);
      cSignalSim cs;
      cs.scan(cw);
      int toggles = 0, last = -1, period = -1, pulses = 0; bool stable = true, prevOn = false, prevLamp = false;
      const int nT = std::max(800, 7 * kRingClockPeriod[di]);
      for (int i = 0; i < nT; ++i) {
        cs.tick(cw, 0, 0, 100, [](int, int, int) {});
        const bool on = stOn(cw.getState(2 + 6, y, 2 + 4));
        // 2026-10-31 (JD Latchor ruling): the Latchor (default 64) now FIRES on each matching ring pulse
        // instead of toggling, so the lap period is measured between its rising edges.
        if (on && !prevOn) {
          if (last >= 0 && toggles >= 2) { const int p = i - last; if (period < 0) period = p; else if (p != period) stable = false; }
          last = i; ++toggles;
        }
        prevOn = on;
        const bool lamp = stValue(cw.getState(2 + 4, y, 2 + 5)) > 0;
        pulses += lamp && !prevLamp; prevLamp = lamp;
      }
      std::printf("  clocks pad: delay %d -> latch toggled %d times in %d ticks, lap period %d ticks (%s), pulse lamp %d flashes\n",
                  repeator::kDelays[di], toggles, nT, period, stable ? "steady" : "UNSTEADY", pulses);
      check(toggles >= 4 && stable && period == kRingClockPeriod[di], "clocks pad: ring clock self-runs at its labelled period");
    }
  }
  {  // Framebuffer scene: the controller ring sequences the frame cache and the screen scrolls.
    ChunkMap fw;
    fw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 8; ++cz)
      for (int cx = 0; cx < 6; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; fw.insert(std::move(col)); }
    const FramebufferLayout fl = buildFramebufferScene(fw, 0, y, 0, 64);
    cSignalSim fs;
    fs.scan(fw);
    const int memX = 4 + 2, memZ = fl.screenZ0 + 1 + 4 * 3 - 1;  // stage 3's memory cell
    int changes = 0, lampFlashes = 0, last = stValue(fw.getState(memX, y, memZ));
    bool prevLit = false;
    for (int i = 0; i < 2000; ++i) {
      fs.tick(fw, 0, 0, 100, [](int, int, int) {});
      const int v = stValue(fw.getState(memX, y, memZ));
      if (v != last) { ++changes; last = v; }
      const bool lit = stValue(fw.getState(30, y + 1, memZ + 2)) > 0;  // a lamp of stage 3
      lampFlashes += lit && !prevLit; prevLit = lit;
    }
    std::printf("  framebuffer scene: %d stages... stage-3 cell changed %d times, lamp flashed %d times in 2000 ticks\n",
                (fl.screenZ1 - fl.screenZ0) / 4, changes, lampFlashes);
    check(changes >= 20 && lampFlashes >= 20, "framebuffer scene: cache controller runs and the screen shifts frames down");
  }
  {  // 2026-10-24 (JD): the 16-wide test-area screen (same machine, pixels = 16, 4 stages) runs too.
    ChunkMap fw;
    fw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 6; ++cz)
      for (int cx = 0; cx < 6; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; fw.insert(std::move(col)); }
    const FramebufferLayout fl = buildFramebufferScene(fw, 0, y, 0, 32, 16);
    cSignalSim fs;
    fs.scan(fw);
    const int memX = 4 + 2, memZ = fl.screenZ0 + 1 + 4 * 3 - 1;  // stage 3 (the last) memory cell
    int changes = 0, lampFlashes = 0, last = stValue(fw.getState(memX, y, memZ));
    bool prevLit = false;
    for (int i = 0; i < 2000; ++i) {
      fs.tick(fw, 0, 0, 100, [](int, int, int) {});
      const int v = stValue(fw.getState(memX, y, memZ));
      if (v != last) { ++changes; last = v; }
      const bool lit = stValue(fw.getState(14, y + 1, memZ + 2)) > 0;
      lampFlashes += lit && !prevLit; prevLit = lit;
    }
    check(changes >= 20 && lampFlashes >= 20, "framebuffer scene 16 wide (test area): cache controller runs and the screen shifts frames down");
  }
  {  // Incremental re-flood == full pass: same scene, one sim forced to a full pass every tick.
    auto make = [&](ChunkMap& m) {
      for (int cz = 0; cz < 8; ++cz)
        for (int cx = 0; cx < 6; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m.insert(std::move(col)); }
      buildFramebufferScene(m, 0, y, 0, 64);
      buildDisplayScene(m, 0, y, 100, 24);
    };
    ChunkMap a, b;
    a.setRegistry(&blockRegistry());
    b.setRegistry(&blockRegistry());
    make(a); make(b);
    cSignalSim sa, sb;
    sa.scan(a); sb.scan(b);
    sa.setLampHoldTicks(5); sb.setLampHoldTicks(5);
    int mismatch = 0;
    for (int i = 0; i < 1500 && !mismatch; ++i) {
      sa.tick(a, 0, 0, 100, [](int, int, int) {});
      sb.invalidateNetwork(); sb.tick(b, 0, 0, 100, [](int, int, int) {});
      for (int z = 0; z < 128; ++z)
        for (int x = 0; x < 96; ++x)
          for (int yy = y; yy <= y + 1; ++yy)
            if (a.getState(x, yy, z) != b.getState(x, yy, z)) {
              if (!mismatch) std::printf("  [info] reflood mismatch tick %d at %d,%d,%d block %u incr %u full %u\n", i, x, yy, z, unsigned(a.get(x, yy, z)), unsigned(a.getState(x, yy, z)), unsigned(b.getState(x, yy, z)));
              ++mismatch;
            }
    }
    check(mismatch == 0, "incremental per-network re-flood matches a full pass every tick (framebuffer + display, 1500 ticks)");
  }
  {  // Upright framebuffer wall: wire stairs carry clock, data and links up the back face.
    ChunkMap vw;
    vw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 4; ++cz)
      for (int cx = 0; cx < 6; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; vw.insert(std::move(col)); }
    const FramebufferLayout vl = buildFramebufferWall(vw, 0, y, 0, 64);
    cSignalSim vs;
    vs.scan(vw);
    vs.setLampHoldTicks(17);  // as in game at 512 TPS
    int litTicks = 0;
    const int sx = 10, memY = y + 3 + 4 * 6 + 1;  // stage 6's memory cell (high up the wall)
    int changes = 0, flashes = 0, last = stValue(vw.getState(sx - 1, memY, vl.wallZ + 1));
    bool prevLit = false;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 4000; ++i) {  // 2026-10-24: delays doubled (4/8/16/32), was 2000
      vs.tick(vw, 0, 0, 100, [](int, int, int) {});
      const int v = stValue(vw.getState(sx - 1, memY, vl.wallZ + 1));
      if (v != last) { ++changes; last = v; }
      const bool lit = stValue(vw.getState(sx + 30, memY, vl.wallZ)) > 0;  // a lamp on that band
      flashes += lit && !prevLit; prevLit = lit; litTicks += lit;
    }
    std::printf("  framebuffer wall: lamp lit %d / 2000 ticks with afterglow 17\n", litTicks);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 4000.0;
    std::printf("  framebuffer wall: stage-6 cell changed %d times, lamp flashed %d times in 2000 ticks (%.3f ms/tick incl. probes, %zu blocks)\n", changes, flashes, ms, size_t(vl.blocks));
    check(changes >= 20 && litTicks > 2000,  // 2026-10-24: with 4/8/16/32 delays afterglow 17 no longer bridges the clock gap (~70% lit, flickers); was > 95%
         "framebuffer wall: vertical wire stairs carry clock + data, frames shift up the wall");
  }
  {  // Redirector split routing: each pixel lights only from its intended wire colour.
    ChunkMap rw;
    rw.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 3; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; rw.insert(std::move(col)); }
    constexpr int n = 8, x0 = 4, z0 = 4;
    buildRedirectorScene(rw, x0, y, z0, n);
    cSignalSim rs;
    rs.scan(rw);
    auto runR = [&](int k) { for (int i = 0; i < k; ++i) rs.tick(rw, 0, 0, 100, [](int, int, int) {}); };
    auto lamp = [&](int i) { return stValue(rw.getState(x0 + 2 * i + 1, y, z0 + 1)); };  // pixel i
    auto setBlock = [&](int x, int z, BlockId b) { rw.set(x, y, z, b); rs.onBlockChanged(x, y, z, b); };
    auto pattern = [&](bool red, bool blue) {
      for (int i = 0; i < n; ++i) { const bool want = (i & 1) ? blue : red; if ((lamp(i) > 0) != want) return false; }
      return true;
    };
    runR(4);
    const bool both = pattern(true, true);
    setBlock(x0 - 1, z0 + 2, kAir); runR(4);
    const bool redOnly = pattern(true, false);
    setBlock(x0 - 1, z0 + 2, kBlueTorch); setBlock(x0 - 1, z0, kAir); runR(4);
    const bool blueOnly = pattern(false, true);
    check(both && redOnly && blueOnly, "redirector scene: every pixel lights only from its own wire colour (red / blue / both)");
    setBlock(x0 - 1, z0, kRedTorch); runR(4);
    const bool lit0 = lamp(0) > 0;
    static const bool muxNoUse = [] { const char* e = std::getenv("LL_MUX_NOUSE"); return !(e && e[0] == '0'); }();  // 2026-11-01 (JD): Multiplexor has no click control
    const CellState st0 = rw.getState(x0, y, z0 + 1);
    rs.use(rw, x0, y, z0, [](int, int, int) {});  // use the red trace next to Redirector 0: red front E -> S
    runR(4);
    const CellState st = rw.getState(x0, y, z0 + 1);
    if (muxNoUse)
      check(lit0 && lamp(0) > 0 && stFacing(st) == stFacing(st0) && stAux(st) == stAux(st0),
            "multiplexor: using the adjacent red wire does nothing (no click control; lanes unchanged, pixel 0 stays lit)");
    else
    check(lit0 && lamp(0) == 0 && stFacing(st) == 2 && (stAux(st) & 3) == 0 && pattern(false, true) == false,
          "redirector: using the adjacent red wire cycles only the red front (pixel 0 goes dark, blue front unchanged)");
  }
  {  // 2026-10-31 (JD, f3fce6d): Trace across chunk borders on every axis decays exactly 1 per cell.
    ChunkMap tw;
    tw.setRegistry(&blockRegistry());
    for (int cx = -3; cx <= 5; ++cx)
      for (int cz = -3; cz <= 5; ++cz) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; tw.insert(std::move(col)); }
    bool ok = true; int firstBad = 0, badAt = 0, badGot = 0;
    struct Run { int x0, z0, dx, dz; BlockId trace, torch; };  // one y level per run (no crossings)
    const Run runs[] = {{4, 2, 1, 0, kRedTrace, kRedTorch}, {40, 6, -1, 0, kBlueTrace, kBlueTorch},
                        {2, 4, 0, 1, kRedTrace, kRedTorch}, {6, 40, 0, -1, kBlueTrace, kBlueTorch}};
    int ry = 0;
    for (const Run& r : runs) {
      const int yy = y + 3 * ry++;
      for (int i = -1; i <= 66; ++i) tw.set(r.x0 + r.dx * i, yy - 1, r.z0 + r.dz * i, kWaferSilicon);
      tw.set(r.x0, yy, r.z0, r.torch);
      for (int i = 1; i <= 66; ++i) tw.set(r.x0 + r.dx * i, yy, r.z0 + r.dz * i, r.trace);
    }
    cSignalSim ts;
    ts.scan(tw);
    for (int i = 0; i < 8; ++i) ts.tick(tw, 0, 0, 100, [](int, int, int) {});
    int ri = 0;  // run index (0-based y level; printed 1-based)
    for (const Run& r : runs) {
      const int yy = y + 3 * ri++;
      for (int i = 1; i <= 66; ++i) {
        const int want = std::max(0, 65 - i), got = stValue(tw.getState(r.x0 + r.dx * i, yy, r.z0 + r.dz * i));
        if (got != want && ok) { ok = false; firstBad = ri; badAt = i; badGot = got; }
      }
    }
    if (!ok) std::printf("  [info] trace border run %d: cell %d reads %d, wants %d\n", firstBad, badAt, badGot, std::max(0, 65 - badAt));
    check(ok, "trace: red and blue Trace runs crossing chunk borders on +X, -X, +Z, -Z decay exactly 1 per cell (64 .. 1, then dark)");
    // Live edits while the source toggles: torch off/on with a Trace cell re-placed at each border cell
    // (local 15 / 0) between ticks; after settling every run must again decay exactly 1 per cell.
    auto put = [&](int x, int yy, int z, BlockId id) { tw.set(x, yy, z, id); ts.onBlockChanged(x, yy, z, id); };
    auto tk = [&](int n) { for (int i = 0; i < n; ++i) ts.tick(tw, 0, 0, 100, [](int, int, int) {}); };
    ri = 0;
    for (const Run& r : runs) {
      const int yy = y + 3 * ri++;
      for (int cyc = 0; cyc < 3; ++cyc) {
        put(r.x0, yy, r.z0, kAir); tk(1);
        for (int i = 1; i <= 66; ++i) {
          const int x = r.x0 + r.dx * i, z = r.z0 + r.dz * i, l = (r.dx ? x : z) & 15;
          if (l != 0 && l != 15) continue;
          put(x, yy, z, kAir); tk(1); put(x, yy, z, r.trace);
        }
        put(r.x0, yy, r.z0, r.torch); tk(1 + cyc);
      }
    }
    tk(8);
    bool ok2 = true; ri = 0;
    for (const Run& r : runs) {
      const int yy = y + 3 * ri++;
      for (int i = 1; i <= 66; ++i) {
        const int want = std::max(0, 65 - i), got = stValue(tw.getState(r.x0 + r.dx * i, yy, r.z0 + r.dz * i));
        if (got != want && ok2) { ok2 = false; std::printf("  [info] trace border edit run %d: cell %d reads %d, wants %d\n", ri, i, got, want); }
      }
    }
    check(ok2, "trace: border-cell edits while the source toggles leave every run decaying exactly 1 per cell across chunk borders");
  }
  {  // PlayJ fix 4: column-local net rebuild — wire edit on a Trace run spanning a chunk border
     // must keep Trace decay 1/cell (edgeSplit or full fallback; must not worsen the seam).
    ChunkMap cw;
    cw.setRegistry(&blockRegistry());
    for (int cx = 0; cx <= 2; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = 0; cw.insert(std::move(col)); }
    const int yy = y + 20;
    for (int x = 0; x <= 40; ++x) cw.set(x, yy - 1, 2, kWaferSilicon);
    cw.set(1, yy, 2, kRedTorch);
    for (int x = 2; x <= 40; ++x) cw.set(x, yy, 2, kRedTrace);
    cSignalSim cs;
    cs.scan(cw);
    for (int i = 0; i < 8; ++i) cs.tick(cw, 0, 0, 100, [](int, int, int) {});
    auto val = [&](int x) { return stValue(cw.getState(x, yy, 2)); };
    // torch@x1; wire@x2=64, so xn = 64-(n-2). x15=51, x16=50, x17=49, x30=36.
    const bool before = val(15) == 51 && val(16) == 50 && val(17) == 49;
    auto put = [&](int x, BlockId id) { cw.set(x, yy, 2, id); cs.onBlockChanged(x, yy, 2, id); };
    put(15, kAir); put(15, kRedTrace);
    put(16, kAir); put(16, kRedTrace);
    for (int i = 0; i < 8; ++i) cs.tick(cw, 0, 0, 100, [](int, int, int) {});
    const bool after = val(15) == 51 && val(16) == 50 && val(17) == 49 && val(30) == 36;
    if (!after) std::printf("  [info] column-local border: before_ok=%d v15=%d v16=%d v17=%d v30=%d\n", int(before), val(15), val(16), val(17), val(30));
    check(before && after, "PlayJ column-local nets: Trace spanning chunk border keeps 1/cell decay after wire re-place at 15|16");
  }
  {  // 2026-10-31 (JD repro): Repeator at chunk-0 local x 15 driving blue Trace in chunk 1 (x 16..), and the
     // mirror (Repeator at x 16 facing W into x 15..), at y 266 (sub-chunk 16); toggled torch, 40 ticks.
    ChunkMap rw;
    rw.setRegistry(&blockRegistry());
    for (int cx = -2; cx <= 3; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = 5; rw.insert(std::move(col)); }
    const int ry = 266;
    for (int x = -20; x <= 50; ++x) for (int z = 88; z <= 92; ++z) rw.set(x, ry - 1, z, kWaferSilicon);
    auto putF = [&](int x, int yy, int z, BlockId b, int f) { rw.set(x, yy, z, b); rw.setState(x, yy, z, makeState(0, f)); };
    rw.set(13, ry, 88, kBlueTorch); rw.set(14, ry, 88, kBlueTrace); putF(15, ry, 88, kRepeator, 1);
    for (int x = 16; x <= 40; ++x) rw.set(x, ry, 88, kBlueTrace);
    rw.set(18, ry, 92, kBlueTorch); rw.set(17, ry, 92, kBlueTrace); putF(16, ry, 92, kRepeator, 3);
    for (int x = 15; x >= -10; --x) rw.set(x, ry, 92, kBlueTrace);
    cSignalSim rs;
    rs.scan(rw);
    for (int i = 0; i < 60; ++i) rs.tick(rw, 1, 5, 100, [](int, int, int) {});
    auto v = [&](int x, int z) { return stValue(rw.getState(x, ry, z)); };
    std::printf("  [info] repeator seam: E run x16=%d x17=%d x22=%d | W run x15=%d x14=%d x9=%d\n", v(16, 88), v(17, 88), v(22, 88), v(15, 92), v(14, 92), v(9, 92));
    check(v(16, 88) > 0 && v(17, 88) == v(16, 88) - 1 && v(22, 88) == v(16, 88) - 6,
          "trace seam 15|16: a Repeator at local x 15 drives blue Trace across the seam, decaying 1 per cell (+X)");
    check(v(15, 92) > 0 && v(14, 92) == v(15, 92) - 1 && v(9, 92) == v(15, 92) - 6,
          "trace seam 16|15: a Repeator at local x 0 facing W drives blue Trace across the seam, decaying 1 per cell (-X)");
    // Column (1, 5) replaced in place by an identical fresh copy (same column count, as a reload or an
    // evict + load in one pump does): the run past the seam must stay lit, not freeze dark on stale pointers.
    std::unique_ptr<ChunkColumn> oldCol = rw.extract(1, 5);  // kept alive: a stale write must not be a use-after-free here
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 1; col->cz = 5; rw.insert(std::move(col)); }
    for (int x = 16; x <= 31; ++x) for (int z = 88; z <= 92; ++z) rw.set(x, ry - 1, z, kWaferSilicon);
    for (int x = 16; x <= 31; ++x) rw.set(x, ry, 88, kBlueTrace);
    rw.set(18, ry, 92, kBlueTorch); rw.set(17, ry, 92, kBlueTrace); putF(16, ry, 92, kRepeator, 3);
    for (int i = 0; i < 60; ++i) rs.tick(rw, 1, 5, 100, [](int, int, int) {});
    std::printf("  [info] column replace: E run x16=%d x22=%d x31=%d | W run x15=%d\n", v(16, 88), v(22, 88), v(31, 88), v(15, 92));
    check(v(16, 88) == 64 && v(22, 88) == 58 && v(31, 88) == 49 && v(15, 92) == 64,
          "trace seam: a column replaced in place (same count) relinks; Trace past the seam carries 64 .. 49 again");
  }
  {  // Vias (vertical, lossless, colour-isolated) and glass (cuts wire stairs).
    ChunkMap vw;
    vw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; vw.insert(std::move(col)); }
    for (int x = 0; x < 12; ++x) for (int z = 0; z < 6; ++z) vw.set(x, y - 1, z, kWaferSilicon);
    vw.set(1, y, 1, kRedTorch); vw.set(2, y, 1, kRedTrace);
    for (int yy = y + 1; yy <= y + 3; ++yy) { vw.set(2, yy, 1, kRedVia); vw.set(3, yy, 1, kBlueVia); }
    vw.set(2, y + 4, 1, kRedTrace); vw.set(3, y + 4, 1, kBlueTrace);
    vw.set(4, y, 3, kRedTorch); vw.set(5, y, 3, kRedTrace); vw.set(6, y + 1, 3, kRedTrace); vw.set(5, y + 1, 3, kGlass);
    vw.set(4, y, 5, kRedTorch); vw.set(5, y, 5, kRedTrace); vw.set(6, y + 1, 5, kRedTrace);
    cSignalSim vs;
    vs.scan(vw);
    for (int i = 0; i < 4; ++i) vs.tick(vw, 0, 0, 100, [](int, int, int) {});
    const int bot = stValue(vw.getState(2, y, 1)), top = stValue(vw.getState(2, y + 4, 1));
    check(bot == 64 && top == 64 && stValue(vw.getState(2, y + 2, 1)) == 64,
          "via: a red via stack carries the wire up losslessly (64 bottom -> 64 top)");
    check(stValue(vw.getState(3, y + 4, 1)) == 0 && stValue(vw.getState(3, y + 2, 1)) == 0,
          "via: a blue via standing against a lit red via stays dark (no sideways links)");
    check(stValue(vw.getState(6, y + 1, 3)) == 0 && stValue(vw.getState(6, y + 1, 5)) == 63,
          "glass: glass over a wire blocks the stair climb (control stair without glass is lit)");
  }
  {  // 2026-10-25 (JD): side-lying Vias: horizontal runs, pipe elbows, end-only Trace / Repeator feed.
    ChunkMap vw;
    vw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; vw.insert(std::move(col)); }
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) vw.set(x, y - 1, z, kWaferSilicon);
    auto putF = [&](int x, int yy, int z, BlockId b, int f) { vw.set(x, yy, z, b); vw.setState(x, yy, z, makeState(0, f)); };
    // A: torch -> trace -> 4 side vias (X) -> trace.
    vw.set(0, y, 1, kRedTorch); vw.set(1, y, 1, kRedTrace);
    for (int x = 2; x <= 5; ++x) vw.set(x, y, 1, kRedViaX);
    vw.set(6, y, 1, kRedTrace);
    // B: elbow: side via (X) -> vertical stack up 2 -> side via (X) -> trace at the top.
    vw.set(0, y, 3, kRedTorch); vw.set(1, y, 3, kRedTrace); vw.set(2, y, 3, kRedViaX);
    for (int yy = y; yy <= y + 2; ++yy) vw.set(3, yy, 3, kRedVia);
    vw.set(4, y + 2, 3, kRedViaX); vw.set(5, y + 2, 3, kRedTrace); vw.set(5, y + 1, 3, kWaferSilicon);
    // C: a trace against a side via's closed side (Z via fed along X) and against a vertical via.
    vw.set(0, y, 5, kRedTorch); vw.set(1, y, 5, kRedTrace); vw.set(2, y, 5, kRedViaZ);
    vw.set(0, y, 7, kRedTorch); vw.set(1, y, 7, kRedTrace); vw.set(2, y, 7, kRedVia);
    // D: colour: lit red side via end-on to a blue side via.
    vw.set(0, y, 9, kRedTorch); vw.set(1, y, 9, kRedTrace); vw.set(2, y, 9, kRedViaX); vw.set(3, y, 9, kBlueViaX); vw.set(4, y, 9, kBlueTrace);
    // E: Repeator (facing E) into a side via end (X) vs into a side via's closed side (Z).
    vw.set(0, y, 11, kRedTorch); vw.set(1, y, 11, kRedTrace); putF(2, y, 11, kRepeator, 1); vw.set(3, y, 11, kRedViaX); vw.set(4, y, 11, kRedTrace);
    vw.set(0, y, 13, kRedTorch); vw.set(1, y, 13, kRedTrace); putF(2, y, 13, kRepeator, 1); vw.set(3, y, 13, kRedViaZ); vw.set(4, y, 13, kRedTrace);
    cSignalSim vs;
    vs.scan(vw);
    for (int i = 0; i < 40; ++i) vs.tick(vw, 0, 0, 100, [](int, int, int) {});
    auto v = [&](int x, int yy, int z) { return stValue(vw.getState(x, yy, z)); };
    check(v(1, y, 1) == 64 && v(3, y, 1) == 64 && v(5, y, 1) == 64 && v(6, y, 1) == 64,
          "side via: a Trace feeds a side via run through its open end; it carries horizontally, losslessly (64 -> 64)");
    check(v(2, y, 3) == 64 && v(3, y + 1, 3) == 64 && v(3, y + 2, 3) == 64 && v(4, y + 2, 3) == 64 && v(5, y + 2, 3) == 64,
          "side via: elbows: side -> vertical stack turns up, vertical -> side turns back out (pipe elbow)");
    check(v(1, y, 5) == 64 && v(2, y, 5) == 0 && v(1, y, 7) == 64 && v(2, y, 7) == 0,
          "side via: a Trace never feeds a side via's closed side, nor a vertical via sideways");
    check(v(2, y, 9) == 64 && v(3, y, 9) == 0 && v(4, y, 9) == 0,
          "side via: red and blue side vias never connect end to end");
    check(v(3, y, 11) > 0 && v(4, y, 11) > 0 && v(3, y, 13) == 0 && v(4, y, 13) == 0,
          "side via: a Repeator feeds a side via only through its open end");
    check(viaAxis(kRedVia) == 0 && viaAxis(kBlueViaZ) == 2 && viaSide(kRedVia, 1) == kRedViaX && viaBase(kBlueViaX) == kBlueVia && viaAxis(kRedTrace) == -1,
          "side via: axis / variant / drop-base helpers (existing vias stay vertical)");
  }
  {  // Wall-pixel: Catchor decoders off per-row Pulsor lines; each pixel shows its caught value,
     // lights alone within its row, left to right; rows are isolated; no wire floats.
    for (const int rows : {1, 16}) {
      ChunkMap ww;
      ww.setRegistry(&blockRegistry());
      for (int cz = 0; cz < 5; ++cz)
        for (int cx = 0; cx < 4; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; ww.insert(std::move(col)); }
      constexpr int px0 = 24, zw = 44, n = 16, yb = 60;
      buildWallPixelScene(ww, px0, yb, zw, n, rows);
      cSignalSim ws;
      ws.scan(ww);
      std::vector<int> first(size_t(n * rows), -1), count(size_t(n * rows), 0);
      bool alone = true, value = true;
      for (int t = 0; t < 2500; ++t) {
        ws.tick(ww, 0, 0, 100, [](int, int, int) {});
        for (int r = 0; r < rows; ++r) {
          int lit = 0;
          for (int i = 0; i < n; ++i) {
            const int v = stValue(ww.getState(px0 + i, yb + 1 + 2 * r, zw));
            if (!v) continue;
            ++lit; ++count[size_t(r * n + i)];
            if (v != i + 1) value = false;
            if (first[size_t(r * n + i)] < 0) first[size_t(r * n + i)] = t;
          }
          if (lit > 1) alone = false;
        }
      }
      bool all = true, ordered = true;
      for (int r = 0; r < rows; ++r)
        for (int i = 0; i < n; ++i) {
          if (count[size_t(r * n + i)] < 2) all = false;
          if (i && first[size_t(r * n + i)] <= first[size_t(r * n + i - 1)]) ordered = false;
        }
      int floating = 0;
      for (int z = 0; z < 80; ++z) for (int x = 0; x < 64; ++x) for (int yy = 1; yy < kWorldHeight - 1; ++yy)
        if (isFlatWire(ww.get(x, yy, z)) && !wireSupported(ww, x, yy, z)) ++floating;
      std::printf("  wall-pixel %d row(s): px0 fired %d, last px fired %d in 2500 ticks; %d floating wires\n",
                  rows, count[0], count[size_t(n * rows - 1)], floating);
      if (rows > 1) { std::printf("   per row (px0/px15):"); for (int r = 0; r < rows; ++r) std::printf(" %d/%d", count[size_t(r * n)], count[size_t(r * n + n - 1)]); std::printf("\n"); }
      check(all && alone && ordered && value && floating == 0,
            rows == 1 ? "wall-pixel 1 row: every pixel fires repeatedly, alone, left to right, showing its caught value"
                      : "wall-pixel 16 rows: per-row drivers + vias, every pixel of every row fires alone in order with its value");
      if (rows == 1) {
        const int m0 = stValue(ww.getState(px0 + 3, yb + 1, zw + 1));
        ws.use(ww, px0 + 3, yb + 1, zw + 1, [](int, int, int) {}, true);
        const CellState s1 = ww.getState(px0 + 3, yb + 1, zw + 1);
        check((stAux(s1) & 3) != 3 && stValue(s1) == m0 % kMaxSignal + 1, "catchor: sneak / Alt + use steps the match and keeps the binding (JD 2026-10-24)");
      }
    }
  }
  {  // Wire torture: stair / cut / ceiling / via / colour rules, one tiny fixture each.
    constexpr int Y = 60;
    auto fixture = [&](auto&& build, int ticks = 12) {
      ChunkMap m;
      m.setRegistry(&blockRegistry());
      { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; m.insert(std::move(col)); }
      for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) m.set(x, Y - 1, z, kWaferSilicon);
      build(m);
      cSignalSim s; s.scan(m);
      for (int t = 0; t < ticks; ++t) s.tick(m, 0, 0, 100, [](int, int, int) {});
      return m;
    };
    auto val = [&](const ChunkMap& m, int x, int y, int z) { return int(stValue(m.getState(x, y, z))); };
    struct StepCase { BlockId b; const char* name; };
    const StepCase steps[] = {{kWaferSilicon, "silicon"}, {kGlass, "glass"}, {kFrostedGlass, "frosted glass"}, {kCeramic, "ceramic"}, {kSiliconSlab, "silicon slab"}};
    int stairBad = 0, cutBad = 0;
    for (const auto& sc : steps) {
      // Up: torch -> A (floor) -> C on top of the step block. Down: torch on the step -> A.
      const ChunkMap up = fixture([&](ChunkMap& m) { m.set(2, Y, 4, kRedTorch); m.set(3, Y, 4, kRedTrace); m.set(4, Y, 4, sc.b); m.set(4, Y + 1, 4, kRedTrace); m.set(5, Y + 1, 4, kRedTrace); });
      const ChunkMap dn = fixture([&](ChunkMap& m) { m.set(4, Y, 4, sc.b); m.set(5, Y, 4, kWaferSilicon); m.set(6, Y + 1, 4, kRedTorch); m.set(5, Y + 1, 4, kRedTrace); m.set(4, Y + 1, 4, kRedTrace); m.set(3, Y, 4, kRedTrace); });
      const bool upOk = val(up, 4, Y + 1, 4) == 63 && val(up, 5, Y + 1, 4) == 62, dnOk = val(dn, 3, Y, 4) == 62;
      // Cut: the same climb with the block placed over the lower wire instead (blocks the stair if it cuts).
      const ChunkMap cut = fixture([&](ChunkMap& m) { m.set(2, Y, 4, kRedTorch); m.set(3, Y, 4, kRedTrace); m.set(3, Y + 1, 4, sc.b); m.set(4, Y, 4, kWaferSilicon); m.set(4, Y + 1, 4, kRedTrace); });
      const bool cuts = blockRegistry().has(sc.b, kCutsWire);
      const bool cutOk = cuts ? val(cut, 4, Y + 1, 4) == 0 : val(cut, 4, Y + 1, 4) == 63;
      std::printf("  stairs over %-13s: up %d/%d down %d | block over the lower wire -> upper %d (%s)\n", sc.name, val(up, 4, Y + 1, 4),
                  val(up, 5, Y + 1, 4), val(dn, 3, Y, 4), val(cut, 4, Y + 1, 4), cuts ? "should cut" : "should pass");
      stairBad += !(upOk && dnOk); cutBad += !cutOk;
    }
    check(stairBad == 0, "wire stairs: climb onto / descend from silicon, glass, frosted glass, ceramic and slab steps (1 per block)");
    check(cutBad == 0, "wire stairs: silicon / glass / frosted glass / ceramic over the lower wire cut the stair; a slab does not");
    // Ceiling wire: hangs under glass, carries signal sideways.
    const ChunkMap ceil = fixture([&](ChunkMap& m) {
      for (int x = 2; x <= 6; ++x) { m.set(x, Y + 3, 6, kGlass); m.set(x, Y + 2, 6, x == 2 ? kRedTorch : kRedTrace); }
    });
    check(val(ceil, 3, Y + 2, 6) == 64 && val(ceil, 6, Y + 2, 6) == 61, "ceiling wire: hangs under glass and carries the signal (64 -> 61 over 4)");
    // Vias: lossless climb, no sideways link, colours isolated; red / blue traces side by side never link.
    const ChunkMap via = fixture([&](ChunkMap& m) {
      m.set(2, Y, 8, kRedTorch); m.set(3, Y, 8, kRedTrace);
      for (int y = Y + 1; y <= Y + 8; ++y) m.set(3, y, 8, kRedVia);
      m.set(3, Y + 9, 8, kRedTrace); m.set(4, Y + 9, 8, kRedTrace); m.set(4, Y + 8, 8, kGlass); m.set(3, Y + 8, 7, kGlass);
      m.set(4, Y + 4, 8, kGlass); m.set(4, Y + 5, 8, kRedTrace);  // beside the via: must stay dark
      m.set(3, Y, 9, kBlueTrace); m.set(4, Y, 9, kBlueTrace);     // blue next to the red trace / torch
    });
    const bool viaOk = val(via, 3, Y + 9, 8) == val(via, 3, Y, 8) && val(via, 4, Y + 9, 8) == val(via, 3, Y, 8) - 1 && val(via, 4, Y + 5, 8) == 0 &&
                       val(via, 3, Y, 9) == 0 && val(via, 4, Y, 9) == 0;
    std::printf("  via: bottom %d top %d after-top %d beside %d | blue beside red %d\n", val(via, 3, Y, 8), val(via, 3, Y + 9, 8), val(via, 4, Y + 9, 8),
                val(via, 4, Y + 5, 8), val(via, 3, Y, 9));
    check(viaOk, "via: 8-high climb is lossless, nothing links to its side, blue never picks up red");
    // Repeator through a block: drives the silicon block in front (powerable) and the wire on top of it.
    const ChunkMap rep = fixture([&](ChunkMap& m) {
      m.set(2, Y, 11, kRedTorch); m.set(3, Y, 11, kRedTrace); m.set(4, Y, 11, kRepeator); m.setState(4, Y, 11, makeState(0, 1, 0));
      m.set(5, Y, 11, kWaferSilicon); m.set(5, Y + 1, 11, kRedTrace); m.set(6, Y, 11, kRedTrace);
    }, 20);
    const ChunkMap repUnder = fixture([&](ChunkMap& m) {  // wire under the block (hanging below it) and a repeator reading it
      m.set(2, Y + 3, 13, kRedTorch); m.set(3, Y + 3, 13, kRedTrace); m.set(4, Y + 3, 13, kRepeator); m.setState(4, Y + 3, 13, makeState(0, 1, 0));
      for (int x = 2; x <= 4; ++x) m.set(x, Y + 2, 13, kGlass);
      m.set(5, Y + 3, 13, kWaferSilicon); m.set(5, Y + 2, 13, kRedTrace); m.set(6, Y + 2, 13, kWaferSilicon);
      m.set(6, Y + 3, 13, kRepeator); m.setState(6, Y + 3, 13, makeState(0, 1, 0)); m.set(7, Y + 3, 13, kRedTrace); m.set(7, Y + 2, 13, kGlass);
    }, 20);
    const ChunkMap insul = fixture([&](ChunkMap& m) {  // the same into glass and ceramic: never powered
      m.set(2, Y, 14, kRedTorch); m.set(3, Y, 14, kRedTrace); m.set(4, Y, 14, kRepeator); m.setState(4, Y, 14, makeState(0, 1, 0));
      m.set(5, Y, 14, kGlass); m.set(5, Y + 1, 14, kRedTrace); m.set(6, Y, 14, kRedTrace);
      m.set(2, Y, 15, kRedTorch); m.set(3, Y, 15, kRedTrace); m.set(4, Y, 15, kRepeator); m.setState(4, Y, 15, makeState(0, 1, 0));
      m.set(5, Y, 15, kCeramic); m.set(5, Y + 1, 15, kRedTrace); m.set(6, Y, 15, kRedTrace);
    }, 20);
    std::printf("  repeator -> silicon: top %d beyond %d | under %d, repeator behind it -> %d | glass top %d beyond %d, ceramic top %d beyond %d\n",
                val(rep, 5, Y + 1, 11), val(rep, 6, Y, 11), val(repUnder, 5, Y + 2, 13), val(repUnder, 7, Y + 3, 13),
                val(insul, 5, Y + 1, 14), val(insul, 6, Y, 14), val(insul, 5, Y + 1, 15), val(insul, 6, Y, 15));
    check(val(rep, 5, Y + 1, 11) > 0 && val(rep, 6, Y, 11) > 0 && val(repUnder, 5, Y + 2, 13) > 0 && val(repUnder, 7, Y + 3, 13) > 0,
          "repeator hard-powers the silicon block it faces: wire on top, beyond, hanging underneath and a repeator behind it all pick it up");
    check(val(insul, 5, Y + 1, 14) == 0 && val(insul, 6, Y, 14) == 0 && val(insul, 5, Y + 1, 15) == 0 && val(insul, 6, Y, 15) == 0,
          "repeator into glass / ceramic: never powered, nothing beyond lights");
    // Perf: a 48 x 48 serpentine of stairs (up / down every block) flipped on / off every tick.
    {
      ChunkMap m;
      m.setRegistry(&blockRegistry());
      for (int cz = 0; cz < 4; ++cz) for (int cx = 0; cx < 4; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m.insert(std::move(col)); }
      int wires = 0;
      for (int z = 2; z < 50; ++z) for (int x = 2; x < 50; ++x) {
        m.set(x, Y - 1, z, kWaferSilicon);
        const bool up = (x + z) & 1;
        if (up) m.set(x, Y, z, kWaferSilicon);
        if ((z % 2 == 0) || x == ((z / 2) % 2 ? 2 : 49)) { m.set(x, up ? Y + 1 : Y, z, kRedTrace); ++wires; }
      }
      cSignalSim s; s.scan(m);
      const auto t0 = std::chrono::steady_clock::now();
      int lit = 0;
      for (int t = 0; t < 400; ++t) {
        for (int z = 2; z < 50; z += 2) {  // a torch at every row start: the whole field flips each tick
          const BlockId b = (t & 1) ? kAir : kRedTorch;
          m.set(1, Y - 1, z, kWaferSilicon); m.set(1, Y, z, b); s.onBlockChanged(1, Y, z, b);
        }
        s.tick(m, 0, 0, 100, [](int, int, int) {});
        if (t == 398) for (int z = 2; z < 50; ++z) for (int x = 2; x < 50; ++x) for (int yy = Y; yy <= Y + 1; ++yy) lit += isFlatWire(m.get(x, yy, z)) && stValue(m.getState(x, yy, z)) > 0;
      }
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 400;
      std::printf("  wire torture perf: %d stair wires toggled every tick: %.3f ms/tick (%.0f TPS max), %d lit at peak\n", wires, ms, 1000.0 / ms, lit);
    }
  }
  {  // Component sleep: skipping quiescent components gives exactly the same world every tick.
    struct Case { const char* name; int kind; };
    for (const Case c : {Case{"pattern wall 32", 0}, Case{"wire torture", 1}, Case{"wall-pixel 16", 2}, Case{"sawtooth", 3}}) {
      ChunkMap a, b;
      a.setRegistry(&blockRegistry());
      b.setRegistry(&blockRegistry());
      for (ChunkMap* m : {&a, &b}) {
        for (int cz = 0; cz < 7; ++cz) for (int cx = 0; cx < 7; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m->insert(std::move(col)); }
        if (c.kind == 0) buildPatternWall(*m, 16, 60, 44, 32, "circles");
        else if (c.kind == 1) buildWireTorture(*m, 8, 60, 8);
        else if (c.kind == 2) buildWallPixelScene(*m, 24, 60, 44, 16, 16);
        else buildSawtoothWall(*m, 8, 60, 20, 16, 4, "circles");
      }
      cSignalSim sa, sb; sa.setComponentSleep(true); sb.setComponentSleep(false);
      sa.scan(a); sb.scan(b);
      int diffTicks = 0; size_t ticked = 0, total = 0;
      const auto t0 = std::chrono::steady_clock::now(); double msA = 0, msB = 0;
      for (int t = 0; t < 600; ++t) {
        auto ta = std::chrono::steady_clock::now();
        sa.tick(a, 0, 0, 100, [](int, int, int) {});
        auto tb = std::chrono::steady_clock::now();
        sb.tick(b, 0, 0, 100, [](int, int, int) {});
        auto tc = std::chrono::steady_clock::now();
        msA += std::chrono::duration<double, std::milli>(tb - ta).count(); msB += std::chrono::duration<double, std::milli>(tc - tb).count();
        ticked += sa.lastTickedComponents(); total += sb.lastTickedComponents();
        bool same = true;
        if (t % 7 == 0 || t < 50)
          for (int z = 0; z < 112 && same; ++z) for (int x = 0; x < 112 && same; ++x) for (int yy = 40; yy < 200 && same; ++yy)
            if (a.getState(x, yy, z) != b.getState(x, yy, z)) same = false;
        diffTicks += !same;
      }
      (void)t0;
      std::printf("  component sleep, %s: %d differing ticks; ticked %.0f%% of components; %.3f vs %.3f ms/tick\n", c.name, diffTicks,
                  total ? 100.0 * double(ticked) / double(total) : 0.0, msA / 600, msB / 600);
      check(diffTicks == 0, "component sleep: identical world every checked tick (pattern wall, wire torture, wall-pixel, sawtooth)");
    }
  }
  {  // 2026-11-01 (JD LL_WAKE): awake-list dispatch gives the same world and state hash as candidate-everything mode.
    struct Case { const char* name; int kind; };
    bool allSame = true;
    for (const Case c : {Case{"pattern wall 32", 0}, Case{"wire torture", 1}, Case{"wall-pixel 16", 2}, Case{"sawtooth", 3}}) {
      ChunkMap a, b;
      a.setRegistry(&blockRegistry());
      b.setRegistry(&blockRegistry());
      for (ChunkMap* m : {&a, &b}) {
        for (int cz = 0; cz < 7; ++cz) for (int cx = 0; cx < 7; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m->insert(std::move(col)); }
        if (c.kind == 0) buildPatternWall(*m, 16, 60, 44, 32, "circles");
        else if (c.kind == 1) buildWireTorture(*m, 8, 60, 8);
        else if (c.kind == 2) buildWallPixelScene(*m, 24, 60, 44, 16, 16);
        else buildSawtoothWall(*m, 8, 60, 20, 16, 4, "circles");
      }
      cSignalSim sa, sb;
      sa.setComponentSleep(true); sa.setWakeList(true);
      sb.setComponentSleep(true); sb.setWakeList(false);
      sa.scan(a); sb.scan(b);
      int diffTicks = 0; size_t tickedA = 0, tickedB = 0;
      uint64_t ha = 1469598103934665603ull, hb = ha;
      for (int t = 0; t < 600; ++t) {
        sa.tick(a, 0, 0, 100, [](int, int, int) {});
        sb.tick(b, 0, 0, 100, [](int, int, int) {});
        tickedA += sa.lastTickedComponents(); tickedB += sb.lastTickedComponents();
        bool same = true;
        if (t % 7 == 0 || t < 50 || t == 599)
          for (int z = 0; z < 112; ++z) for (int x = 0; x < 112; ++x) for (int yy = 40; yy < 200; ++yy) {
            const CellState va = a.getState(x, yy, z), vb = b.getState(x, yy, z);
            if (va != vb) same = false;
            if (t == 599) { ha = (ha ^ va) * 1099511628211ull; hb = (hb ^ vb) * 1099511628211ull; }
          }
        diffTicks += !same;
      }
      std::printf("  LL_WAKE parity %s: %d differing ticks, ticked %zu vs %zu, hash %016llx vs %016llx\n", c.name, diffTicks,
                  tickedA, tickedB, (unsigned long long)ha, (unsigned long long)hb);
      allSame = allSame && diffTicks == 0 && ha == hb && tickedA == tickedB;
    }
    check(allSame, "LL_WAKE parity: identical world, final state hash and ticked count vs LL_WAKE=0 (4 scenes, 600 ticks)");
  }
  {  // 2026-11-01 (JD Option A): cCompiledCompTable publishes only at commit; values survive a relink by key.
    cCompiledCompTable t;
    t.rebuild({10, 20, 30});
    uint32_t v = 99;
    t.stage(20, 0x1234);
    const bool hidden = t.published(20, v) && v == 0;
    t.commit();
    const bool shown = t.published(20, v) && v == 0x1234;
    t.rebuild({5, 20, 40});
    const bool kept = t.published(20, v) && v == 0x1234 && !t.published(30, v) && t.size() == 3;
    check(hidden && shown && kept, "Option A table: staged simBits hidden until commit, published after, kept across relink");
  }
  {  // Redirector output is colourless: red Trace in -> blue Trace out and back; a Catchor feeds both split lanes.
    constexpr int Y = 60;
    auto run = [&](auto&& build) {
      auto m = std::make_unique<ChunkMap>();
      m->setRegistry(&blockRegistry());
      { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; m->insert(std::move(col)); }
      for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) m->set(x, Y - 1, z, kWaferSilicon);
      build(*m);
      cSignalSim s; s.scan(*m);
      for (int t = 0; t < 12; ++t) s.tick(*m, 0, 0, 100, [](int, int, int) {});
      return m;
    };
    for (const bool redIn : {true, false}) {
      const auto m = run([&](ChunkMap& w) {
        w.set(2, Y, 4, redIn ? kRedTorch : kBlueTorch); w.set(3, Y, 4, redIn ? kRedTrace : kBlueTrace);
        w.set(4, Y, 4, kRedirector); w.setState(4, Y, 4, makeState(0, 1));  // unsplit, out east
        w.set(5, Y, 4, redIn ? kBlueTrace : kRedTrace); w.set(6, Y, 4, redIn ? kBlueTrace : kRedTrace);
      });
      std::printf("  redirector %s -> %s: out trace %d, next %d\n", redIn ? "red" : "blue", redIn ? "blue" : "red", int(stValue(m->getState(5, Y, 4))), int(stValue(m->getState(6, Y, 4))));
      check(stValue(m->getState(5, Y, 4)) > 0 && stValue(m->getState(6, Y, 4)) > 0,
            redIn ? "redirector transfers red Trace in -> blue Trace out (colourless output)" : "redirector transfers blue Trace in -> red Trace out (colourless output)");
    }
    {  // Split lanes: red lane front east into blue Trace (cross-colour), blue lane front north into red Trace.
      const auto m = run([&](ChunkMap& w) {
        w.set(2, Y, 8, kRedTorch); w.set(3, Y, 8, kRedTrace);
        w.set(4, Y, 8, kRedirector); w.setState(4, Y, 8, makeState(0, 1, 4 | 0));  // red lane -> E, blue lane -> N
        w.set(5, Y, 8, kBlueTrace); w.set(4, Y, 7, kRedTrace);
      });
      check(stValue(m->getState(5, Y, 8)) > 0 && stValue(m->getState(4, Y, 7)) == 0, "split redirector: red lane out its front into blue Trace; idle blue lane stays dark");
    }
    {  // Colourless Catchor input feeds both split lanes (documented): both fronts flash.
      int north = 0, south = 0;
      auto m = std::make_unique<ChunkMap>();
      m->setRegistry(&blockRegistry());
      { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; m->insert(std::move(col)); }
      for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) m->set(x, Y - 1, z, kWaferSilicon);
      m->set(2, Y, 12, kRedTorch); m->set(3, Y, 12, kRedTrace);
      m->set(4, Y, 12, kCatchor); m->setState(4, Y, 12, makeState(kMaxSignal, 0, 3));
      m->set(5, Y, 12, kRedirector); m->setState(5, Y, 12, makeState(0, 0, 4 | 2));
      m->set(5, Y, 11, kColorLamp64); m->set(5, Y, 13, kColorLamp64);
      cSignalSim s; s.scan(*m);
      for (int t = 0; t < 12; ++t) {
        s.tick(*m, 0, 0, 100, [](int, int, int) {});
        north = std::max(north, int(stValue(m->getState(5, Y, 11)))); south = std::max(south, int(stValue(m->getState(5, Y, 13))));
      }
      check(north > 0 && south > 0, "catchor (colourless) -> split redirector: feeds both lanes, both fronts fire");
    }
  }
  {  // Wire-torture scene: field flips every tick, Catchor chain fires, hard power works, nothing floats.
    ChunkMap m;
    m.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 7; ++cz) for (int cx = 0; cx < 5; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m.insert(std::move(col)); }
    constexpr int X = 8, Yt = 60, Z = 8;
    buildWireTorture(m, X, Yt, Z);
    cSignalSim s; s.scan(m);
    int flips = 0, prev = -1, catchFires = 0, hard = 0;
    const auto t0 = std::chrono::steady_clock::now();
    constexpr int kT = 300;
    for (int t = 0; t < kT; ++t) {
      s.tick(m, 0, 0, 100, [](int, int, int) {});
      const int v = stValue(m.getState(X + 47, Yt, Z + kTortureField + 10));  // deep in the field
      if (prev >= 0 && (v > 0) != (prev > 0)) ++flips;
      prev = v;
      for (int i = 4; i <= 8; i += 2) catchFires += stValue(m.getState(X + i, Yt + 1, Z + 26)) > 0;
      hard = stValue(m.getState(X + 3, Yt + 1, Z + 15));
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / kT;
    int floating = 0, lampGlass = stValue(m.getState(X + 5, Yt, Z + 17)) + stValue(m.getState(X + 5, Yt, Z + 19));
    for (int z = 0; z < 112; ++z) for (int x = 0; x < 80; ++x) for (int yy = Yt; yy < Yt + 14; ++yy)
      if (isFlatWire(m.get(x, yy, z)) && !wireSupported(m, x, yy, z)) ++floating;
    std::printf("  wire-torture scene: field flips %d / %d ticks, catchor lamp-ticks %d, hard-powered top %d, glass/ceramic lamps %d, %d floating, %.3f ms/tick\n",
                flips, kT - 1, catchFires, hard, lampGlass, floating, ms);
    check(flips >= (kT - 1) * 9 / 10 && catchFires > 0 && hard > 0 && lampGlass == 0 && floating == 0,
          "wire-torture scene: stair field flips every tick, Catchor -> Redirector chain fires, hard power only through silicon, no floating wire");
  }
  {  // Sawtooth wall: every pixel's own wire is independent (packed red / blue side by side).
    constexpr int W = 16, Rr = 4, px0 = 8, zw = 20, yb = 60;
    auto make = [&](ChunkMap& m) {
      for (int cz = 0; cz < 3; ++cz) for (int cx = 0; cx < 3; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; m.insert(std::move(col)); }
      buildSawtoothWall(m, px0, yb, zw, W, Rr, "circles");
    };
    auto lampAt = [&](const ChunkMap& m, int i, int r) { return stValue(m.getState(px0 + i, yb + 1 + 2 * r, zw)); };
    // Full image
    ChunkMap full;
    full.setRegistry(&blockRegistry());
    make(full); cSignalSim fs; fs.scan(full);
    for (int t = 0; t < 40; ++t) fs.tick(full, 0, 0, 100, [](int, int, int) {});
    int wrongFull = 0, crosstalk = 0;
    for (int r = 0; r < Rr; ++r) for (int i = 0; i < W; ++i)
      if (lampAt(full, i, r) != std::clamp(patternColor("circles", W, i, r), 1, kMaxSignal - kSawtoothRun)) ++wrongFull;
    // One pixel at a time: clear every Memor but one; only that lamp may light.
    for (int r = 0; r < Rr; ++r) for (int i = 0; i < W; i += 3) {
      ChunkMap one;
      one.setRegistry(&blockRegistry());
      make(one);
      for (int rr = 0; rr < Rr; ++rr) for (int ii = 0; ii < W; ++ii)
        if (rr != r || ii != i) one.setState(px0 + ii, yb + 1 + 2 * rr, zw + kSawtoothRun + 1, makeState(0, 0));
      cSignalSim os; os.scan(one);
      for (int t = 0; t < 40; ++t) os.tick(one, 0, 0, 100, [](int, int, int) {});
      for (int rr = 0; rr < Rr; ++rr) for (int ii = 0; ii < W; ++ii)
        if ((rr == r && ii == i) != (lampAt(one, ii, rr) > 0)) ++crosstalk;
    }
    int floating = 0;
    for (int z = 0; z < 48; ++z) for (int x = 0; x < 48; ++x) for (int yy = 50; yy < 80; ++yy)
      if (isFlatWire(full.get(x, yy, z)) && !wireSupported(full, x, yy, z)) ++floating;
    std::printf("  sawtooth %dx%d: %d wrong colours, %d cross-talk cells, %d floating\n", W, Rr, wrongFull, crosstalk, floating);
    check(wrongFull == 0 && crosstalk == 0 && floating == 0,
          "sawtooth wall: red / blue wires packed side by side, every pixel independent (single-pixel drive lights only it), pattern colours exact");
  }
  {  // Pattern wall: each Catchor matches its pixel's pattern colour; shared drivers sweep the colours
     // up Red / Blue Via spines; every pixel lights with exactly its colour; no wire floats.
    for (const auto& [sz, pat] : {std::pair{32, "circles"}, std::pair{64, "checkers"}}) {
      ChunkMap pw;
      pw.setRegistry(&blockRegistry());
      for (int cz = 0; cz < 5; ++cz)
        for (int cx = 0; cx < 7; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; pw.insert(std::move(col)); }
      constexpr int px0 = 16, zw = 44, yb = 60;
      buildPatternWall(pw, px0, yb, zw, sz, pat);
      cSignalSim ps;
      ps.scan(pw);
      std::vector<int> count(size_t(sz * sz), 0);
      int wrong = 0;
      const auto t0 = std::chrono::steady_clock::now();
      constexpr int kTicks = 2000;
      for (int t = 0; t < kTicks; ++t) {
        ps.tick(pw, 0, 0, 100, [](int, int, int) {});
        for (int r = 0; r < sz; ++r)
          for (int i = 0; i < sz; ++i) {
            const int v = stValue(pw.getState(px0 + i, yb + 1 + 2 * r, zw - 1));  // 2026-10-24: lamps sit behind the latch Memor
            if (!v) continue;
            ++count[size_t(r * sz + i)];
            if (v != patternColor(pat, sz, i, r)) ++wrong;
          }
      }
      const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / kTicks;
      const auto t1 = std::chrono::steady_clock::now();
      for (int t = 0; t < 1000; ++t) ps.tick(pw, 0, 0, 100, [](int, int, int) {});
      { const auto p = ps.takePhaseMs(); std::printf("  pattern wall %dx%d phases (ms/tick over 3000): comps %.3f mark %.3f wires %.3f lamps %.3f\n", sz, sz, p.comps / 3000, p.mark / 3000, p.wires / 3000, p.lamps / 3000); }
      std::printf("  pattern wall %dx%d: pure sim %.3f ms/tick (1000 ticks, no probes)\n", sz, sz, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count() / 1000);
      int dark = 0, minC = 1 << 30;
      for (int c : count) { if (c < 2) ++dark; minC = std::min(minC, c); }
      int floating = 0;
      for (int z = 0; z < 80; ++z) for (int x = 0; x < 112; ++x) for (int yy = 1; yy < 200; ++yy)
        if (isFlatWire(pw.get(x, yy, z)) && !wireSupported(pw, x, yy, z)) ++floating;
      std::printf("  pattern wall %dx%d %s: %d dark, %d wrong-colour lights, min %d lights/pixel in %d ticks, %d floating, %.3f ms/tick (incl. readback)\n",
                  sz, sz, pat, dark, wrong, minC, kTicks, floating, ms);
      check(dark == 0 && wrong == 0 && floating == 0,
            sz == 32 ? "pattern wall 32x32 circles: every pixel lights repeatedly with exactly its pattern colour, no floating wire"
                     : "pattern wall 64x64 checkers: every pixel lights repeatedly with exactly its pattern colour, no floating wire");
    }
  }
  {  // Catchor wiring: auto-connects to the first wire touching a side, output centre / left / right
     // relative to that input (never up / down), state persists; a Catchor feeding a Redirector
     // makes it the Redirector's input (centre / left / right cycling only).
    ChunkMap cw;
    cw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; cw.insert(std::move(col)); }
    for (int x = 0; x < 12; ++x) for (int z = 0; z < 8; ++z) cw.set(x, y - 1, z, kWaferSilicon);
    cSignalSim cs;
    auto setB = [&](int x, int z, BlockId b, CellState st = 0) { cw.set(x, y, z, b); if (st) cw.setState(x, y, z, st); cs.onBlockChanged(x, y, z, b); };
    auto lit = [&](int x, int z) { return stValue(cw.getState(x, y, z)) > 0; };
    cw.set(2, y, 3, kCatchor); cw.setState(2, y, 3, makeState(64, 0, 3));  // unwired, catches 64
    cw.set(3, y, 3, kColorLamp64); cw.set(2, y, 2, kColorLamp64); cw.set(2, y, 4, kColorLamp64);
    cs.scan(cw);
    setB(1, 3, kRedTrace); setB(0, 3, kRedTorch);  // the first wire to touch: west side, red
    std::array<bool, 3> seen{};  // E (centre), N (left), S (right)
    auto runC = [&](int k) { seen = {}; for (int i = 0; i < k; ++i) { cs.tick(cw, 0, 0, 100, [](int, int, int) {}); seen[0] |= lit(3, 3); seen[1] |= lit(2, 2); seen[2] |= lit(2, 4); } };
    auto retrigger = [&] { setB(0, 3, kAir); runC(3); setB(0, 3, kRedTorch); runC(6); };
    runC(6);
    const CellState w0 = cw.getState(2, y, 3);
    const bool autoWired = stFacing(w0) == 1 && (stAux(w0) & 7) == 0 && seen[0] && !seen[1] && !seen[2];
    setB(2, 4, kAir); setB(2, 4, kBlueTrace); runC(3);  // a later blue trace does not rewire it
    const bool kept = cw.getState(2, y, 3) == makeState(64, 1, 0, stOn(cw.getState(2, y, 3)));
    setB(2, 4, kColorLamp64);
    cs.use(cw, 2, y, 3, [](int, int, int) {}); retrigger();
    const bool left = seen[1] && !seen[0] && !seen[2];
    cs.use(cw, 2, y, 3, [](int, int, int) {}); retrigger();
    const bool right = seen[2] && !seen[0] && !seen[1];
    cs.use(cw, 2, y, 3, [](int, int, int) {});
    const bool back = (stAux(cw.getState(2, y, 3)) & 3) == 0;
    const bool unbound = true;  // 2026-10-24 (JD): sneak / Alt + use no longer clears the binding
    cs.use(cw, 2, y, 3, [](int, int, int) {}, true);  // bound -> sneak+use steps the match (64 -> 1)
    const bool alt = stValue(cw.getState(2, y, 3)) == 1 && (stAux(cw.getState(2, y, 3)) & 3) == 0;
    cw.setState(2, y, 3, makeState(64, 1, 0));  // rebind directly for the redirector test below
    check(autoWired && kept && left && right && back && unbound && alt,
          "catchor: auto-wires to the first touching wire (west, red), keeps it; output centre -> left -> right; sneak+use clears binding, then steps match");
    // Catchor -> Redirector: replace the east lamp with a Redirector (facing east) + lamps around it.
    setB(3, 3, kRedirector, makeState(0, 1)); setB(4, 3, kColorLamp64); setB(3, 2, kColorLamp64); setB(3, 4, kColorLamp64);
    auto runR = [&](int k) { seen = {}; for (int i = 0; i < k; ++i) { cs.tick(cw, 0, 0, 100, [](int, int, int) {}); seen[0] |= lit(4, 3); seen[1] |= lit(3, 2); seen[2] |= lit(3, 4); } };
    auto again = [&] { setB(0, 3, kAir); runR(3); setB(0, 3, kRedTorch); runR(8); };
    again();
    const bool rc = seen[0] && !seen[1] && !seen[2];
    static const bool muxNoUse2 = [] { const char* e = std::getenv("LL_MUX_NOUSE"); return !(e && e[0] == '0'); }();  // 2026-11-01 (JD): Multiplexor has no click control
    if (muxNoUse2) {
      const CellState b0 = cw.getState(3, y, 3);
      const bool u = cs.use(cw, 3, y, 3, [](int, int, int) {}); again();
      check(rc && !u && stFacing(cw.getState(3, y, 3)) == stFacing(b0) && seen[0] && !seen[1] && !seen[2],
            "catchor -> multiplexor: the Catchor is its input; use does nothing (no click control), output stays centre");
    } else {
      cs.use(cw, 3, y, 3, [](int, int, int) {}); again();
      const bool rl = seen[1] && !seen[0] && !seen[2] && stFacing(cw.getState(3, y, 3)) == 0;
      cs.use(cw, 3, y, 3, [](int, int, int) {}); again();
      const bool rr = seen[2] && !seen[0] && !seen[1] && stFacing(cw.getState(3, y, 3)) == 2;
      cs.use(cw, 3, y, 3, [](int, int, int) {});
      const bool rBack = stFacing(cw.getState(3, y, 3)) == 1;  // centre again, never west into the Catchor
      check(rc && rl && rr && rBack, "catchor -> redirector: the Catchor is its input; use cycles centre -> left -> right (never back, never up/down)");
    }
  }
  {  // Wire support: placement needs a floor or a ceiling; losing both pops the wire.
    ChunkMap sw;
    sw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; sw.insert(std::move(col)); }
    cSignalWorldSim sim;
    sw.set(2, y - 1, 2, kWaferSilicon); sw.set(4, y + 1, 2, kGlass);
    const bool onFloor = sim.canPlace(sw, 2, y, 2, kRedTrace), underCeiling = sim.canPlace(sw, 4, y, 2, kRedTrace);
    const bool floating = sim.canPlace(sw, 6, y, 2, kRedTrace), viaFree = sim.canPlace(sw, 6, y, 2, kRedVia);
    sw.set(2, y, 2, kRedTrace);
    const bool kept = sim.stillSupported(sw, 2, y, 2);
    sw.set(2, y - 1, 2, kAir);
    check(onFloor && underCeiling && !floating && viaFree && kept && !sim.stillSupported(sw, 2, y, 2),
          "wire support: on a block or under a ceiling only; vias need none; removing the support un-supports it");
  }
  {  // 2026-10-24 (JD): the Memor needs a solid block beneath it; losing it pops the Memor.
    ChunkMap sw;
    sw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; sw.insert(std::move(col)); }
    cSignalWorldSim sim;
    sw.set(2, y - 1, 2, kWaferSilicon); sw.set(4, y + 1, 2, kGlass); sw.set(8, y, 2, kWaferSilicon);
    const bool onFloor = sim.canPlace(sw, 2, y, 2, kMemor);
    const bool underCeiling = sim.canPlace(sw, 4, y, 2, kMemor), floating = sim.canPlace(sw, 6, y, 2, kMemor);
    const bool besideWall = sim.canPlace(sw, 7, y, 2, kMemor);  // a wall beside it is no support
    sw.set(2, y, 2, kMemor);
    const bool kept = sim.stillSupported(sw, 2, y, 2);
    sw.set(2, y - 1, 2, kAir);
    check(onFloor && !underCeiling && !floating && !besideWall && kept && !sim.stillSupported(sw, 2, y, 2),
          "memor support: placeable only on top of a solid block; removing the block beneath un-supports it (pops)");
    const bool fullStillFree = sim.canPlace(sw, 6, y, 2, kStone);
    check(fullStillFree, "memor support: full blocks keep free placement");
  }
  {  // 2026-10-16 (JD): a Red/Blue LED hangs from the bottom face of a block (attach face 2).
    ChunkMap sw;
    sw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; sw.insert(std::move(col)); }
    cSignalWorldSim sim;
    sw.set(2, y + 1, 2, kWaferSilicon);  // the ceiling block
    const bool hangs = sim.canAttach(sw, 2, y, 2, kRedTorch, 2) && sim.canAttach(sw, 2, y, 2, kBlueTorch, 2);
    const bool noAir = !sim.canAttach(sw, 6, y, 2, kRedTorch, 2);  // still needs a solid support
    check(hangs && noAir, "LED bottom attach: Red/Blue LED hang under a solid block; no support, no attach");
  }
  {  // No bench scene leaves a wire floating.
    auto audit = [&](auto&& build) {
      ChunkMap aw;
      aw.setRegistry(&blockRegistry());
      for (int cz = 0; cz < 8; ++cz)
        for (int cx = 0; cx < 8; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; aw.insert(std::move(col)); }
      build(aw);
      int bad = 0;
      for (int z = 0; z < 128; ++z) for (int x = 0; x < 128; ++x) for (int yy = 1; yy < kWorldHeight - 1; ++yy)
        if (isFlatWire(aw.get(x, yy, z)) && !wireSupported(aw, x, yy, z)) ++bad;
      return bad;
    };
    const int c = audit([&](ChunkMap& m) { buildCircuitScene(m, 4, y, 4, 64); });
    const int d = audit([&](ChunkMap& m) { buildDisplayScene(m, 4, y, 4, 64); });
    const int f = audit([&](ChunkMap& m) { buildFramebufferScene(m, 4, y, 4, 64); });
    const int v = 0;  // legacy stacked-wire wall retired: --screen-orient vertical now builds wall-pixel
    const int r = audit([&](ChunkMap& m) { buildRedirectorScene(m, 8, y, 8, 8); });
    std::printf("  floating wires: circuits %d, display %d, framebuffer %d, wall %d, redirector %d\n", c, d, f, v, r);
    check(c + d + f + v + r == 0, "bench scenes: no floating wire (every wire on a block or under a ceiling)");

  }
  {  // Wireless: transmittor -> receivor across a gap no wire crosses.
    // TX at (10, y, 30) with a red torch to its west (binds red input); RX at (40, y, 30)
    // drives a lamp. Both on channel 1 (default). 30 blocks apart: within TX power.
    putF(10, y, 30, kSignalTransmittor, 1);  // facing E; back (W) is the input side
    put(9, y, 30, kRedTorch);
    putF(40, y, 30, kSignalReceivor, 1);
    put(41, y, 30, kRedTrace);
    put(42, y, 30, kLamp);
    run(6);
    check(val(42, y, 30) > 0, "wireless: lamp lights through transmittor -> receivor (no wire between)");
    // Channel isolation: sneak+use cycles the receivor to channel 2; the ch-1 TX goes unheard.
    sim.use(w, 40, y, 30, none, true);
    run(6);
    check(val(42, y, 30) == 0, "wireless: channel isolation (ch-2 receivor ignores ch-1 broadcast)");
    // Cycle back to channel 1 (2 -> 3 -> 4 -> 1 with 4 unlocked): lamp relights.
    sim.use(w, 40, y, 30, none, true);
    sim.use(w, 40, y, 30, none, true);
    sim.use(w, 40, y, 30, none, true);
    run(6);
    check(val(42, y, 30) > 0, "wireless: sneak+use channel cycling returns to ch 1");
    // Beacon mode: regular use toggles the transmittor to self-generate; break the torch feed.
    w.set(9, y, 30, kAir); sim.onBlockChanged(9, y, 30, kAir);
    run(3);
    check(val(42, y, 30) == 0, "wireless: input mode goes silent with no input");
    sim.use(w, 10, y, 30, none, false);  // toggle to beacon
    run(6);
    check(val(42, y, 30) > 0, "wireless: beacon mode transmits without wire input");
  }
  {  // Jammer: cOverclockor + jammer item blocks all channels except its own.
    // TX at (10, y, 50) torch-fed; RX at (40, y, 50) + lamp; cOverclockor at (25, y, 50).
    putF(10, y, 50, kSignalTransmittor, 1);
    put(9, y, 50, kRedTorch);
    putF(40, y, 50, kSignalReceivor, 1);
    put(41, y, 50, kRedTrace);
    put(42, y, 50, kLamp);
    put(25, y, 50, kOverclockor);
    run(6);
    check(val(42, y, 50) > 0, "jammer: no jammer installed, signal passes");
    sim.use(w, 25, y, 50, none, false);  // install the jammer item (v1: use toggles)
    run(6);
    check(val(42, y, 50) > 0, "jammer: jammer on ch 1 allows ch-1 traffic through");
    sim.use(w, 40, y, 50, none, true);  // receivor to ch 2; TX still on ch 1
    sim.use(w, 10, y, 50, none, true);  // TX to ch 2 as well
    run(6);
    check(val(42, y, 50) == 0, "jammer: ch-2 traffic jammed by ch-1 jammer");
    sim.use(w, 25, y, 50, none, true);  // jammer to ch 2: now the allowed channel
    run(6);
    check(val(42, y, 50) > 0, "jammer: ch-2 traffic passes when jammer allows ch 2");
  }
  {  // cRadioReg unit checks: link budget, strongest-wins, channel filter.
    cRadioReg r;
    int s = 0, c = 0;
    r.publish(RadioBroadcast{0, 10, 0, 1, 1, 64, 1, 256});
    r.publish(RadioBroadcast{200, 10, 0, 1, 1, 32, 2, 256});
    r.publish(RadioBroadcast{1000, 10, 0, 1, 1, 48, 1, 256});
    r.flip();
    bool heard = r.query(1, 150, 10, 0, 1, 0, s, c);
    check(heard && s == 64 && c == 1, "radio: strongest in-range broadcast wins");
    heard = r.query(1, 500, 10, 0, 1, 0, s, c);  // nearest (B at 200) is 300 > 256: out of range
    check(!heard, "radio: out-of-range broadcast unheard (link budget)");
    heard = r.query(1, 500, 10, 0, 1, 100, s, c);  // +100 sensitivity: 300 <= 356, hears B
    check(heard && s == 32 && c == 2, "radio: receivor sensitivity extends range (the Dish hears better)");
    heard = r.query(2, 200, 10, 0, 1, 0, s, c);
    check(!heard, "radio: channel filter (nothing on ch 2)");
    r.flip();  // no new publishes: registry goes silent
    heard = r.query(1, 200, 10, 0, 1, 0, s, c);
    check(!heard, "radio: broadcasts expire after one tick without refresh");
  }
  {  // Q) Launch-critical Trace guarantees, explicit (JD priority, 2026-10-12): colour isolation
     // in BOTH directions on parallel adjacent runs, exact 1-per-block decay at measured
     // distances, and the torch -> wire -> lamp round trip with source strength exactly 64.
    ChunkMap q;
    q.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 4; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; q.insert(std::move(col)); }
    const int qy = 10;
    for (int z = 0; z < 32; ++z)
      for (int x = 0; x < 64; ++x) q.set(x, qy - 1, z, kWaferSilicon);
    cSignalSim qs;
    auto qput = [&](int x, int z, BlockId b) { q.set(x, qy, z, b); qs.onBlockChanged(x, qy, z, b); };
    auto qrun = [&](int n) { for (int i = 0; i < n; ++i) qs.tick(q, 0, 0, 100, [](int, int, int) {}); };
    auto qval = [&](int x, int z) { return stValue(q.getState(x, qy, z)); };
    // A1/A2: parallel adjacent runs, one colour driven at a time, plus a wire of the other
    // colour touching the driving torch itself.
    qput(0, 0, kRedTorch); qput(0, 1, kBlueTrace);
    for (int x = 1; x <= 10; ++x) { qput(x, 0, kRedTrace); qput(x, 1, kBlueTrace); }
    qput(0, 4, kRedTrace); qput(0, 5, kBlueTorch);
    for (int x = 1; x <= 10; ++x) { qput(x, 5, kBlueTrace); qput(x, 4, kRedTrace); }
    qrun(3);
    bool blueDark = qval(0, 1) == 0;
    for (int x = 1; x <= 10; ++x) blueDark &= qval(x, 1) == 0;
    check(qval(1, 0) == 64 && qval(10, 0) == 55 && blueDark,
          "TRACE-A1 isolation: red run at 64..55 beside blue run - every blue cell stays 0 (incl. beside the red torch)");
    bool redDark = qval(0, 4) == 0;
    for (int x = 1; x <= 10; ++x) redDark &= qval(x, 4) == 0;
    check(qval(1, 5) == 64 && qval(10, 5) == 55 && redDark,
          "TRACE-A2 isolation: blue run at 64..55 beside red run - every red cell stays 0 (incl. beside the blue torch)");
    // B: exact decay at measured distances on both colours (strength = 65 - distance in blocks).
    qput(0, 8, kRedTorch);
    for (int x = 1; x <= 40; ++x) qput(x, 8, kRedTrace);
    qput(0, 10, kBlueTorch);
    for (int x = 1; x <= 40; ++x) qput(x, 10, kBlueTrace);
    qrun(3);
    check(qval(1, 8) == 64 && qval(2, 8) == 63 && qval(5, 8) == 60 && qval(10, 8) == 55 && qval(33, 8) == 32 && qval(40, 8) == 25,
          "TRACE-B1 decay: red run is exactly 64@1, 63@2, 60@5, 55@10, 32@33, 25@40 (1 per block, no more, no less)");
    check(qval(1, 10) == 64 && qval(2, 10) == 63 && qval(5, 10) == 60 && qval(33, 10) == 32 && qval(40, 10) == 25,
          "TRACE-B2 decay: blue run decays identically (64@1, 63@2, 60@5, 32@33, 25@40)");
    // C: torch -> wire -> lamp round trip, both colours; source strength is exactly 64, never more.
    qput(0, 12, kRedTorch);
    for (int x = 1; x <= 6; ++x) qput(x, 12, kRedTrace);
    qput(7, 12, kLamp);
    qput(0, 14, kBlueTorch);
    for (int x = 1; x <= 6; ++x) qput(x, 14, kBlueTrace);
    qput(7, 14, kLamp);
    qrun(3);
    int maxRed = 0, maxBlue = 0;
    for (int x = 1; x <= 6; ++x) { maxRed = std::max(maxRed, qval(x, 12)); maxBlue = std::max(maxBlue, qval(x, 14)); }
    check(qval(7, 12) == 1 && qval(1, 12) == 64 && maxRed == 64,
          "TRACE-C1 round trip (red): torch -> 6 trace -> lamp lit; first trace 64 and no cell exceeds 64");
    check(qval(7, 14) == 1 && qval(1, 14) == 64 && maxBlue == 64,
          "TRACE-C2 round trip (blue): torch -> 6 trace -> lamp lit; first trace 64 and no cell exceeds 64");
    q.set(6, qy, 12, kAir); qs.onBlockChanged(6, qy, 12, kAir);  // break the last red trace
    qrun(2);
    check(qval(7, 12) == 0 && qval(7, 14) == 1,
          "TRACE-C3 round trip is live: breaking the red run's last trace turns its lamp off; the blue lamp stays lit");
  }
  {
    // ROADMAP-7 (2026-10-25, JD's comparator-clock port): Timer Buffer + Thresholdor read mode.
    // N = 0, E = 1, S = 2, W = 3. Each sub-test gets a fresh world and sim.
    struct Rig {
      ChunkMap m; cSignalSim s; int y = 10;
      Rig() {
        m.setRegistry(&blockRegistry());
        auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; m.insert(std::move(col));
        for (int x = 0; x < 12; ++x) for (int z = 0; z < 12; ++z) m.set(x, y - 1, z, kWaferSilicon);
      }
      void put(int x, int z, BlockId b, CellState st = 0) { m.set(x, y, z, b); m.setState(x, y, z, st); s.onBlockChanged(x, y, z, b); }
      void run(int n) { for (int i = 0; i < n; ++i) s.tick(m, 0, 0, 100, [](int, int, int) {}); }
      int v(int x, int z) const { return stValue(m.getState(x, y, z)); }
      int aux(int x, int z) const { return stAux(m.getState(x, y, z)); }
      void use(int x, int z, bool alt = false) { s.use(m, x, y, z, [](int, int, int) {}, alt); }
    };
    constexpr int RN = 0, RE = 1;
    {
      // Timer Buffer alone (loop, step 1, reload 16): 16 -> 0 then reload; period 17 ticks.
      Rig r; r.put(5, 5, kTimerBuffer, makeState(0, RN));
      std::vector<int> seq; for (int i = 0; i < 40; ++i) { r.run(1); seq.push_back(r.v(5, 5)); }
      bool periodic = true; for (int i = 17; i < 40; ++i) periodic = periodic && seq[i] == seq[i - 17];
      bool down = true; for (int i = 1; i < 17; ++i) down = down && seq[i] == 16 - i;
      check(seq[0] == 16 && down && periodic, "ROADMAP-7 timer buffer: loop mode counts 16 -> 0 then reloads, period 17 ticks");
      check(r.aux(5, 5) == 0, "ROADMAP-7 timer buffer: placed in loop mode, step 1 (aux 0)");
      r.use(5, 5);
      check(r.aux(5, 5) == 2, "ROADMAP-7 timer buffer: use steps the divider (step 2 ticks)");
      for (int k = 0; k < 3; ++k) r.use(5, 5);
      check(r.aux(5, 5) == 1, "ROADMAP-7 timer buffer: 4 uses wrap the step and flip to once mode");
      r.run(60);
      check(r.v(5, 5) == 0 && !stOn(r.m.getState(5, 5, r.y)), "ROADMAP-7 timer buffer: once mode stops at 0 (no reload)");
      r.use(5, 5, true);
      check(r.v(5, 5) == 17, "ROADMAP-7 timer buffer: sneak+use steps reload 16 -> 17 and reloads");
      r.run(17);
      check(r.v(5, 5) == 0, "ROADMAP-7 timer buffer: once mode counts 17 down to 0 in 17 ticks");
      // Repower: torch + trace behind it (back = south, facing N).
      r.put(5, 7, kRedTorch); r.put(5, 6, kRedTrace);
      r.run(5);
      check(r.v(5, 5) == 17, "ROADMAP-7 timer buffer: powered back holds it at the reload value (repower)");
      r.put(5, 7, kAir);
      r.run(3);
      const int a = r.v(5, 5); r.run(5);
      check(a < 17 && a > 0 && r.v(5, 5) == a - 5, "ROADMAP-7 timer buffer: released, it counts down again one step a tick");
      r.run(40);
      check(r.v(5, 5) == 0, "ROADMAP-7 timer buffer: and stops at 0 again in once mode");
    }
    {
      // Thresholdor read mode: Timer Buffer (facing N, so it does not drive the Thresholdor) sits
      // behind a Thresholdor facing E. Read off: output 0. Read on: output = the count.
      Rig r; r.put(4, 5, kTimerBuffer, makeState(0, RN)); r.put(5, 5, kThresholdor, makeState(0, RE)); r.put(6, 5, kRedTrace);
      r.run(10);
      check(r.v(5, 5) == 0, "ROADMAP-7 thresholdor: read mode off (default) ignores the timer buffer level");
      r.use(5, 5, true);
      check((r.aux(5, 5) & 2) == 2 && (r.aux(5, 5) & 1) == 0, "ROADMAP-7 thresholdor: sneak+use toggles read mode (aux bit 1), subtract untouched");
      bool follows = true;
      // PlayM (JD 2026-11-01, same tick): the read now sees THIS tick's count; with LL_SAME_TICK=0 it is one tick behind.
      const bool sameT = cSignalSim::envSameTick();
      for (int i = 0; i < 40; ++i) { const int before = r.v(4, 5); r.run(1); follows = follows && r.v(5, 5) == (sameT ? r.v(4, 5) : before); }
      check(follows, sameT ? "ROADMAP-7 thresholdor: read mode outputs the timer buffer count (same tick)"
                           : "ROADMAP-7 thresholdor: read mode outputs the timer buffer count (LL_SAME_TICK=0: one tick behind)");
      r.use(5, 5);  // subtract mode, still reading
      check((r.aux(5, 5) & 3) == 3, "ROADMAP-7 thresholdor: subtract mode keeps read mode");
      r.put(5, 4, kRedTorch);  // left side (north of an E-facing Thresholdor) = 64 -> subtract to 0
      r.run(5);
      check(r.v(5, 5) == 0, "ROADMAP-7 thresholdor: subtract mode subtracts the side from the read level");
      r.use(5, 5, true);
      check((r.aux(5, 5) & 3) == 1, "ROADMAP-7 thresholdor: sneak+use again = read off, subtract kept");
    }
    {
      // Catchor readable: torch -> trace -> Catchor(match 64). Thresholdor (read, E) east of it.
      Rig r; r.put(2, 3, kCatchor, makeState(64, 0, 3)); r.put(3, 3, kThresholdor, makeState(0, RE, 2)); r.put(4, 3, kRedTrace);
      r.put(1, 3, kRedTrace); r.put(0, 3, kRedTorch);
      r.run(12);
      const int held = r.v(3, 3);
      r.run(20);
      check(held == 64 && r.v(3, 3) == 64, "ROADMAP-7 catchor: a read-mode thresholdor reads the caught value (64), steady while the input holds the match");
      r.put(0, 3, kAir);
      r.run(6);
      check(r.v(3, 3) == 0, "ROADMAP-7 catchor: input gone -> level 0 -> thresholdor off");
      // Read mode off: the Catchor's one-shot pulse only (never steady).
      Rig q; q.put(2, 3, kCatchor, makeState(64, 0, 3)); q.put(3, 3, kThresholdor, makeState(0, RE)); q.put(4, 3, kRedTrace);
      q.put(1, 3, kRedTrace); q.put(0, 3, kRedTorch);
      q.run(32);
      check(q.v(3, 3) == 0, "ROADMAP-7 catchor: read mode off -> no steady level (old behaviour)");
    }
    {
      // Comparator clock at 32 TPS: Timer Buffer loop, step 2, reload 15 -> period (15 + 1) * 2 =
      // 32 ticks = 1 s. A read-mode Thresholdor turns it into a clock: off for 2 ticks per second.
      Rig r; r.put(4, 5, kTimerBuffer, makeState(0, RN)); r.put(5, 5, kThresholdor, makeState(0, RE, 2)); r.put(6, 5, kRedTrace);
      r.use(4, 5);  // step 2
      for (int k = 0; k < 63; ++k) r.use(4, 5, true);  // reload 16 -> 64 -> wraps 1 .. 15
      r.run(70);
      std::vector<int> offs; for (int t = 0; t < 130; ++t) { r.run(1); if (r.v(5, 5) == 0) offs.push_back(t); }
      bool every32 = offs.size() >= 6; for (size_t i = 2; i < offs.size(); ++i) every32 = every32 && offs[i] - offs[i - 2] == 32;
      check(every32 && offs.size() % 2 == 0 && offs[1] - offs[0] == 1,
            "ROADMAP-7 comparator clock: timer buffer (loop, step 2, reload 15) + read thresholdor = 1 Hz at 32 TPS, endless");
    }
  }
  {  // POSTS (2026-10-25, JD; art by Cloud): Standing / Wall Posts -- placement, facings, pair, text.
    ChunkMap pw;
    pw.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; pw.insert(std::move(col)); }
    cSignalWorldSim sim;
    const int py = 20;
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) pw.set(x, py - 1, z, kWaferSilicon);
    const auto& reg = blockRegistry();
    check(reg.idOf(kVirtualPost) == "lithokraft:virtual_post" && reg.idOf(kOrganicPost) == "lithokraft:organic_post" &&
              reg.idOf(kVirtualWallPost) == "lithokraft:virtual_wall_post" && reg.idOf(kOrganicWallPost) == "lithokraft:organic_wall_post" &&
              kVirtualPost == BlockId(kTimerBuffer + 1) && kOrganicWallPost == BlockId(kTimerBuffer + 4),
          "POSTS: 4 namespaced ids appended at the END after the Timer Buffer");
    check(reg.has(kVirtualPost, kFacing8) && !reg.has(kVirtualPost, kDirectional) && reg.has(kVirtualWallPost, kDirectional) &&
              !reg.has(kVirtualWallPost, kFacing8) && reg.has(kOrganicPost, kInteractive) && reg.has(kOrganicWallPost, kInteractive) &&
              !reg.has(kVirtualPost, kOpaque) && reg.def(kVirtualPost).shape == Shape::Custom &&
              cBehaviorCatalog::get().byName("post") && cBehaviorCatalog::get().byName("wall_post"),
          "POSTS: standing = 8 facings, wall = 4 facings, both use-able custom see-through blocks with behaviours");
    // Facings: yaw snapped to the nearest 45 degrees; the octant round-trips through the cell state.
    bool snap = true;
    constexpr float kPi = 3.14159265f;
    for (int o = 0; o < 8; ++o)
      for (float d : {-20.f, 0.f, 20.f}) {
        const float a = (float(o) * 45.f + d) * kPi / 180.f;
        snap = snap && facing8FromLook(std::sin(a), -std::cos(a)) == o;
      }
    bool trip = true;
    for (int o = 0; o < 8; ++o) {
      const CellState st = withFacing8(CellState(post::kUpperBit | 0x1000), o);
      trip = trip && stFacing8(st) == o && stFacing(st) == (o >> 1) && post::isUpper(st) && stOn(st);
    }
    check(snap && trip, "POSTS: 8 facings -- yaw snaps to the nearest 45 degrees; octant round-trips (other bits kept)");
    // Placement: needs the cell above; places the panel half above with the same facing.
    pw.set(3, py + 1, 3, kGlass);
    const bool blocked = !sim.canPlace(pw, 3, py, 3, kVirtualPost), free = sim.canPlace(pw, 5, py, 5, kVirtualPost);
    pw.set(5, py, 5, kVirtualPost); pw.setState(5, py, 5, withFacing8(0, 3));
    sim.onBlockChanged(pw, 5, py, 5, kVirtualPost);
    const CellState lo = pw.getState(5, py, 5), hi = pw.getState(5, py + 1, 5);
    check(blocked && free && pw.get(5, py + 1, 5) == kVirtualPost && !post::isUpper(lo) && post::isUpper(hi) &&
              stFacing8(lo) == 3 && stFacing8(hi) == 3,
          "POSTS: a Standing Post needs room above and places stand + panel as a pair, same facing (SE)");
    // Text: either half edits the panel's text; printable ASCII only; capped at 256.
    auto edit = [&](int x, int y, int z, int ch, int ctl) { sim.command(pw, SimCommand{kCmdPostEdit, x, y, z, ch, ctl}, [](int, int, int) {}); };
    for (char c : std::string("HELLO")) edit(5, py, 5, c, post::kTypeChar);
    edit(5, py + 1, 5, 0, post::kNewline);
    edit(5, py + 1, 5, 'w', post::kTypeChar);
    edit(5, py, 5, 7, post::kTypeChar);  // control char: ignored
    const std::string* txt = pw.cellText(5, py + 1, 5);
    check(txt && *txt == "HELLO\nw" && !pw.cellText(5, py, 5), "POSTS: use on either half writes the panel cell's text (Enter = new line)");
    edit(5, py, 5, 0, post::kBackspace);
    for (int i = 0; i < 300; ++i) edit(5, py, 5, 'a' + i % 26, post::kTypeChar);
    txt = pw.cellText(5, py + 1, 5);
    check(txt && txt->size() == post::kMaxChars && txt->compare(0, 6, "HELLO\n") == 0, "POSTS: text is capped at 256 characters");
    {  // 2026-10-31 (JD, Alt+wheel): kCmdAltUse with a = 1 steps up, a = 63 steps back one.
      pw.set(9, py, 9, kLatchor); sim.onBlockChanged(pw, 9, py, 9, kLatchor);
      auto alt = [&](int n) { sim.command(pw, SimCommand{kCmdAltUse, 9, py, 9, n, 0}, [](int, int, int) {}); return stValue(pw.getState(9, py, 9)); };
      const int v0 = alt(1), v1 = alt(1), vBack = alt(63), vUp = alt(1);
      std::printf("  alt-wheel latchor: %d -> %d, back %d, up %d\n", v0, v1, vBack, vUp);
      check(v1 != v0 && vBack == v0 && vUp == v1, "alt-wheel: a 1..64 setting steps +1 (wheel up) and -1 (wheel down, 63 wrapping steps)");
    }
    const auto lines = post::layout(*txt, 16);
    const auto diag = post::layout(*txt, post::columnsFor(kVirtualPost, hi));
    check(lines.size() == 16 && lines[0] == "HELLO" && lines[1].size() == 16 && post::columnsFor(kVirtualPost, withFacing8(0, 2)) == 16 &&
              post::columnsFor(kVirtualPost, hi) == 10 && diag.size() == 16 && diag[1].size() == 10,
          "POSTS: panel layout wraps 16 columns straight on (16 x 16 = 256 cells), 10 on a diagonal, 16 rows");
    // Pair break: breaking either half removes the other, and the text goes with it.
    pw.set(5, py + 1, 5, kAir); sim.onBlockChanged(pw, 5, py + 1, 5, kAir);
    const bool upperBreak = pw.get(5, py, 5) == kAir && !pw.cellText(5, py + 1, 5);
    pw.set(7, py, 7, kOrganicPost); pw.setState(7, py, 7, withFacing8(0, 6)); sim.onBlockChanged(pw, 7, py, 7, kOrganicPost);
    pw.set(7, py + 2, 7, kOrganicPost); pw.setState(7, py + 2, 7, withFacing8(0, 1)); sim.onBlockChanged(pw, 7, py + 2, 7, kOrganicPost);
    pw.set(7, py, 7, kAir); sim.onBlockChanged(pw, 7, py, 7, kAir);
    check(upperBreak && pw.get(7, py + 1, 7) == kAir && pw.get(7, py + 2, 7) == kOrganicPost && pw.get(7, py + 3, 7) == kOrganicPost,
          "POSTS: breaking either half breaks the pair (text removed); a stacked post above is untouched");
    // Wall Post: needs a solid side block; turns to face it.
    const bool noWall = !sim.canPlace(pw, 12, py, 12, kVirtualWallPost);
    pw.set(13, py, 12, kGlass);
    const bool wall = sim.canPlace(pw, 12, py, 12, kVirtualWallPost);
    pw.set(12, py, 12, kVirtualWallPost); pw.setState(12, py, 12, makeState(0, 0)); sim.onBlockChanged(pw, 12, py, 12, kVirtualWallPost);
    edit(12, py, 12, 'X', post::kTypeChar);
    check(noWall && wall && stFacing(pw.getState(12, py, 12)) == 1 && pw.get(12, py + 1, 12) == kAir &&
              pw.cellText(12, py, 12) && *pw.cellText(12, py, 12) == "X",
          "POSTS: a Wall Post needs a solid block beside it, faces it (E), is one cell, and holds its own text");
    {  // 2026-10-25 (JD): sneak + right click reaches a component's secondary setting; overlap places refused
      check(hasSneakUse(kThresholdor, makeState(0, 0)) && hasSneakUse(kCatchor, makeState(0, 0)) &&
                !hasSneakUse(kRedTrace, 0) && !hasSneakUse(kWaferSilicon, 0),
            "SNEAK USE: Thresholdor / Catchor have a sneak + right click setting (no place); plain blocks do not");
      const float ex = 5.5f, ey = 11.62f, ez = 5.5f;  // eye of a player standing at (5.5, 10, 5.5)
      check(litholattice::placeOverlapsPlayer(5, 10, 5, ex, ey, ez) && litholattice::placeOverlapsPlayer(5, 11, 5, ex, ey, ez) &&
                litholattice::placeOverlapsPlayer(5, 10, 6, ex, ey, 5.75f) && !litholattice::placeOverlapsPlayer(6, 10, 5, ex, ey, ez) &&
                !litholattice::placeOverlapsPlayer(5, 9, 5, ex, ey, ez) && !litholattice::placeOverlapsPlayer(5, 12, 5, ex, ey, ez),
            "PLACE: a block overlapping the player's body is refused (feet, head, a side the box reaches); clear cells allowed");
    }
    {  // 2026-10-25 (JD, Post viewer): the HUD overlay shows all 256 characters, diagonal posts included
      std::string full;
      for (int i = 0; i < 256; ++i) full.push_back(char(33 + i % 94));
      const auto v = post::viewLines(full, false);
      std::string seen;
      for (size_t i = 2; i + 2 < v.size(); ++i) seen += v[i].substr(1, 16);
      bool framed = v.size() == 20 && v[1] == "+----------------+" && v[18] == v[1];
      for (size_t i = 2; i < 18; ++i) framed = framed && v[i].size() == 18 && v[i].front() == '|' && v[i].back() == '|';
      check(framed && seen == full && v[0].find("256 / 256") != std::string::npos &&
                post::layout(full, 10).size() == 16 && v.back().find("Alt") != std::string::npos,
            "POSTS viewer: framed 16 x 16 grid holds all 256 characters (a diagonal panel shows 160); Alt+use writes");
      const auto e = post::viewLines("HI\nTHERE", true);
      check(e[2] == "|HI              |" && e[3] == "|THERE           |" && e[0].find("(writing)") != std::string::npos,
            "POSTS viewer: new lines break rows, short rows padded, writing mode titled");
    }
  }
  {  // 2026-10-31 (JD double-pulse bug): count output edges at every stage, fresh world
    ChunkMap w2;
    w2.setRegistry(&blockRegistry());
    for (int cz = 0; cz < 2; ++cz)
      for (int cx = 0; cx < 3; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; w2.insert(std::move(col)); }
    for (int z = 0; z < 32; ++z) for (int x = 0; x < 48; ++x) w2.set(x, y - 1, z, kWaferSilicon);
    cSignalSim s2;
    auto p2 = [&](int x, int z, BlockId b) { w2.set(x, y, z, b); s2.onBlockChanged(x, y, z, b); };
    auto pf2 = [&](int x, int z, BlockId b, int f) { w2.set(x, y, z, b); w2.setState(x, y, z, makeState(0, f)); s2.onBlockChanged(x, y, z, b); };
    auto v2 = [&](int x, int z) { return int(stValue(w2.getState(x, y, z))); };
    // A) Switch -> trace -> 3 Repeators (each followed by a trace) -> lamp.
    p2(0, 4, kSwitch); p2(1, 4, kRedTrace);
    for (int k = 0; k < 3; ++k) { pf2(2 + 2 * k, 4, kRepeator, E); p2(3 + 2 * k, 4, kRedTrace); }
    p2(8, 4, kLamp);
    // B) Pulsor: data from an always-on Switch, clock from a toggled Switch on its side (blue).
    p2(18, 12, kSwitch); p2(19, 12, kRedTrace); pf2(20, 12, kPulsor, E);
    p2(20, 9, kSwitch); p2(20, 10, kBlueTrace); p2(20, 11, kBlueTrace);
    for (int x = 21; x <= 25; ++x) p2(x, 12, kRedTrace);
    auto tk = [&]() { s2.tick(w2, 0, 0, 100, [](int, int, int) {}); };
    tk();
    s2.use(w2, 18, y, 12, none);  // data on
    int edges[4] = {0, 0, 0, 0}, last[4] = {0, 0, 0, 0}, pEdges = 0, pLast = 0, pHigh = 0, clkRises = 0, pRun = 0, pBadRuns = 0; std::string pTrace;
    const int probe[4] = {1, 3, 5, 7};
    for (int t = 0; t < 120; ++t) {
      if (t == 2 || t == 40) s2.use(w2, 0, y, 4, none);  // chain: on at t=2, off at t=40 (one long pulse)
      if (t % 10 == 5) { s2.use(w2, 20, y, 9, none); clkRises += stOn(w2.getState(20, y, 9)) ? 1 : 0; }  // clock period 20
      tk();
      for (int i = 0; i < 4; ++i) { const int v = v2(probe[i], 4) > 0; edges[i] += v && !last[i]; last[i] = v; }
      // PlayB: skip t < 6 (before the first clock edge at t=5), the data Switch turning on with no clock is a relay-mode 1-tick pulse (by design).
      const int pv = t >= 6 && v2(23, 12) > 0; if (pv && !pLast) pTrace += " r" + std::to_string(t); if (!pv && pLast) pTrace += "/f" + std::to_string(t); pEdges += pv && !pLast; pHigh += pv;
      if (pv) ++pRun; else { if (pLast && pRun != cPulseRules::holdTicks()) ++pBadRuns; pRun = 0; }  // PlayB: every pulse is one hold window
      pLast = pv;
    }
    std::printf("  [info] edge counts: chain %d %d %d %d, pulsor %d rises %d high-ticks %d (clock rises %d)%s\n",
                edges[0], edges[1], edges[2], edges[3], pEdges, pEdges, pHigh, clkRises, pTrace.c_str());
    check(edges[0] == 1 && edges[1] == 1 && edges[2] == 1 && edges[3] == 1,
          "DOUBLE-PULSE: one input pulse -> exactly one rising edge after every Repeator stage");
    check(pEdges == clkRises && pHigh == clkRises * cPulseRules::holdTicks(),
          "DOUBLE-PULSE: Pulsor fires exactly one pulse (hold-window long) per rising clock edge");
    check(pBadRuns == 0 && clkRises > 0, "DOUBLE-PULSE (PlayB): every clock edge -> one unbroken high run of exactly holdTicks() (no blink)");
    // C) Hot swap on a running Repeator ring clock at the fastest delay (4 Repeators, button 1 =
    // 4 ticks). For every ring position (all four facings) and every phase of the lap, replace the
    // Repeator (break + place next tick, and in-place replace) mid-run; afterwards the ring must
    // still carry exactly one 1-tick pulse per lap (never two).
    int worstRises = 0, badHigh = 0, stopped = 0, cases = 0, base = -1;
    std::string hsLog;
    for (int mode = -1; mode < 2; ++mode)
      for (int pos = 0; pos < (mode < 0 ? 1 : 4); ++pos)
        for (int phase = 0; phase < (mode < 0 ? 1 : 20); ++phase) {
          ChunkMap w3;
          w3.setRegistry(&blockRegistry());
          for (int cz = 0; cz < 1; ++cz) for (int cx = 0; cx < 1; ++cx) { auto col = std::make_unique<ChunkColumn>(); col->cx = cx; col->cz = cz; w3.insert(std::move(col)); }
          for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) w3.set(x, y - 1, z, kWaferSilicon);
          cSignalSim s3;
          auto p3 = [&](int x, int z, BlockId b) { w3.set(x, y, z, b); s3.onBlockChanged(x, y, z, b); };
          auto pf3 = [&](int x, int z, BlockId b, int f) { w3.set(x, y, z, b); w3.setState(x, y, z, makeState(0, f)); s3.onBlockChanged(x, y, z, b); };
          auto t3 = [&]() { s3.tick(w3, 0, 0, 100, [](int, int, int) {}); };
          const int ox = 4, oz = 4;
          for (int i = 0; i <= 4; ++i) { p3(ox + i, oz, kRedTrace); p3(ox + i, oz + 4, kRedTrace); p3(ox, oz + i, kRedTrace); p3(ox + 4, oz + i, kRedTrace); }
          const int rx[4] = {ox + 2, ox + 4, ox + 2, ox}, rz[4] = {oz, oz + 2, oz + 4, oz + 2}, rf[4] = {1, 2, 3, 0};
          for (int i = 0; i < 4; ++i) pf3(rx[i], rz[i], kRepeator, rf[i]);
          t3();
          w3.setState(ox + 1, y, oz, CellState(64)); s3.invalidateNetwork(); t3();
          for (int t = 0; t < 40 + phase; ++t) t3();  // settle, then swap at this phase
          if (mode < 0) {} else if (mode == 0) { p3(rx[pos], rz[pos], kAir); t3(); pf3(rx[pos], rz[pos], kRepeator, rf[pos]); }
          else pf3(rx[pos], rz[pos], kRepeator, rf[pos]);
          for (int t = 0; t < 40; ++t) t3();  // let any transient pass
          int rises = 0, high = 0, last = 0;
          const int lap = 80;  // >= 4 laps of the 4 x (4 + 0) tick ring
          for (int t = 0; t < lap; ++t) {
            t3();
            const int v = stValue(w3.getState(ox + 3, y, oz)) > 0;
            rises += v && !last; high += v; last = v;
          }
          if (mode < 0) { base = rises; continue; }
          ++cases;
          worstRises = std::max(worstRises, rises);
          if (high != rises) ++badHigh;
          if (rises == 0) ++stopped;
          if (hsLog.size() < 600 && (high != rises || rises > base)) hsLog += std::format(" [m{} p{} ph{}: rises {} high {}]", mode, pos, phase, rises, high);
        }
    std::printf("  [info] HOTSWAP ring: %d cases, max rises/80t %d (unswapped %d), cases with non-1-tick pulses %d, stopped %d%s\n",
                cases, worstRises, base, badHigh, stopped, hsLog.c_str());
    check(badHigh == 0 && base > 0 && worstRises <= base, "DOUBLE-PULSE HOTSWAP: replacing any Repeator of a running fastest ring clock leaves single 1-tick pulses");
    // D) HOTSWAP2 (JD 3:13-3:15 PM): every delay (4/8/16/32), ring with and without a powered solid
    // block after a Repeator; mid-window events: add / remove / replace a neighbouring component, replace
    // the powered block, cycle a Repeator's delay a full turn. One 1-tick pulse per lap, never more.
    int h2Cases = 0, h2Bad = 0, h2Stopped = 0;
    std::string h2Log;
    for (int delay = 1; delay <= 4; ++delay)  // PlayM: settings 2..5 = 4/8/16/32 ticks (the pre-PlayM set)
      for (int solid = 0; solid < 2; ++solid)
        for (int ev = -1; ev < 6; ++ev) {
          const int period = 4 * repeator::kDelays[delay];
          for (int phase = 0; phase < (ev < 0 ? 1 : period); phase += (period >= 32 ? period / 16 : 1)) {
            ChunkMap w3;
            w3.setRegistry(&blockRegistry());
            { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; w3.insert(std::move(col)); }
            for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) w3.set(x, y - 1, z, kWaferSilicon);
            cSignalSim s3;
            auto p3 = [&](int x, int z, BlockId b) { w3.set(x, y, z, b); s3.onBlockChanged(x, y, z, b); };
            auto pf3 = [&](int x, int z, BlockId b, int f) { w3.set(x, y, z, b); w3.setState(x, y, z, makeState(0, f)); s3.onBlockChanged(x, y, z, b); };
            auto t3 = [&]() { s3.tick(w3, 0, 0, 100, [](int, int, int) {}); };
            const int ox = 4, oz = 4;
            for (int i = 0; i <= 4; ++i) { p3(ox + i, oz, kRedTrace); p3(ox + i, oz + 4, kRedTrace); p3(ox, oz + i, kRedTrace); p3(ox + 4, oz + i, kRedTrace); }
            if (solid) p3(ox + 3, oz, kWaferSilicon);  // Repeator (ox+2) hard-powers this block; the trace beyond picks it up
            const int rx[4] = {ox + 2, ox + 4, ox + 2, ox}, rz[4] = {oz, oz + 2, oz + 4, oz + 2}, rf[4] = {1, 2, 3, 0};
            for (int i = 0; i < 4; ++i) pf3(rx[i], rz[i], kRepeator, rf[i]);
            t3();
            for (int i = 0; i < 4; ++i) { s3.use(w3, rx[i], y, rz[i], none); for (int k = 0; k < (delay + 3) % 4; ++k) s3.use(w3, rx[i], y, rz[i], none); }  // fresh = idx 0 after first use? normalise below
            for (int i = 0; i < 4; ++i) for (int g = 0; g < repeator::kSettings && repeator::delayIndex(w3.getState(rx[i], y, rz[i])) != delay; ++g) s3.use(w3, rx[i], y, rz[i], none);
            t3();
            w3.setState(ox + 1, y, oz, CellState(64)); s3.invalidateNetwork(); t3();
            for (int t = 0; t < 2 * period + phase; ++t) t3();
            switch (ev) {
              case 0: p3(ox + 1, oz - 1, kLamp); break;                         // add a reader beside the ring
              case 1: p3(ox + 1, oz - 1, kLamp); t3(); p3(ox + 1, oz - 1, kAir); break;  // add then remove
              case 2: pf3(ox + 1, oz - 1, kMemor, 0); t3(); pf3(ox + 1, oz - 1, kMemor, 0); break;  // add then replace a component
              case 3: if (solid) { p3(ox + 3, oz, kAir); t3(); p3(ox + 3, oz, kWaferSilicon); } break;  // replace the powered block
              case 4: for (int k = 0; k < repeator::kSettings; ++k) s3.use(w3, rx[1], y, rz[1], none); break;  // delay full turn (PlayM: 6 steps)
              case 5: p3(ox + 2, oz - 1, kRedTrace); t3(); p3(ox + 2, oz - 1, kAir); break;  // trace touching the Repeator side
              default: break;
            }
            for (int t = 0; t < 2 * period; ++t) t3();
            int rises = 0, high = 0, last = 0;
            const int probeX = solid ? ox + 4 : ox + 3;
            for (int t = 0; t < 4 * period; ++t) {
              t3();
              const int v = stValue(w3.getState(probeX, y, oz)) > 0;
              rises += v && !last; high += v; last = v;
            }
            if (ev < 0) { if (rises != 4 || high != 4) { ++h2Bad; h2Log += std::format(" [base d{} s{}: rises {} high {}]", delay, solid, rises, high); } continue; }
            ++h2Cases;
            if (rises == 0) ++h2Stopped;
            if (rises > 4 || high != rises) {
              ++h2Bad;
              if (h2Log.size() < 900) h2Log += std::format(" [d{} s{} ev{} ph{}: rises {} high {}]", delay, solid, ev, phase, rises, high);
            }
          }
        }
    std::printf("  [info] HOTSWAP2: %d cases, bad %d, stopped %d%s\n", h2Cases, h2Bad, h2Stopped, h2Log.c_str());
    check(h2Bad == 0, "DOUBLE-PULSE HOTSWAP2: add/remove/replace beside a running ring clock (every delay, powered block) never doubles a pulse");
    // E/F (JD 3:19-3:20 PM): mid-pulse delay cycle on one Repeator at every delay and phase; and a
    // blue<->red Trace swap mid-pulse, then the ring is cut and everything must die within 32 ticks.
    int e5Cases = 0, e5Bad = 0, f6Cases = 0, f6Bad = 0;
    std::string e5Log, f6Log;
    for (int test = 0; test < 2; ++test)
      for (int delay = 1; delay <= 4; ++delay)  // PlayM: settings 2..5 = 4/8/16/32 ticks (the pre-PlayM set)
        for (int pos = 0; pos < (test ? 16 : 1); ++pos) {
          const int period = 4 * repeator::kDelays[delay];
          for (int phase = 0; phase < period; ++phase) {
            ChunkMap w3;
            w3.setRegistry(&blockRegistry());
            { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; w3.insert(std::move(col)); }
            for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) w3.set(x, y - 1, z, kWaferSilicon);
            cSignalSim s3;
            auto p3 = [&](int x, int z, BlockId b) { w3.set(x, y, z, b); s3.onBlockChanged(x, y, z, b); };
            auto pf3 = [&](int x, int z, BlockId b, int f) { w3.set(x, y, z, b); w3.setState(x, y, z, makeState(0, f)); s3.onBlockChanged(x, y, z, b); };
            auto t3 = [&]() { s3.tick(w3, 0, 0, 100, [](int, int, int) {}); };
            const int ox = 4, oz = 4;
            std::vector<std::pair<int, int>> ring;
            for (int i = 0; i < 4; ++i) { ring.push_back({ox + i, oz}); ring.push_back({ox + 4, oz + i}); ring.push_back({ox + 4 - i, oz + 4}); ring.push_back({ox, oz + 4 - i}); }
            const BlockId tr = test && (pos & 1) ? kRedTrace : kBlueTrace;
            for (auto [x, z] : ring) p3(x, z, tr);
            const int rx[4] = {ox + 2, ox + 4, ox + 2, ox}, rz[4] = {oz, oz + 2, oz + 4, oz + 2}, rf[4] = {1, 2, 3, 0};
            for (int i = 0; i < 4; ++i) pf3(rx[i], rz[i], kRepeator, rf[i]);
            t3();
            for (int i = 0; i < 4; ++i) for (int g = 0; g < repeator::kSettings && repeator::delayIndex(w3.getState(rx[i], y, rz[i])) != delay; ++g) s3.use(w3, rx[i], y, rz[i], none);
            t3();
            w3.setState(ox + 1, y, oz, CellState(64)); s3.invalidateNetwork(); t3();
            for (int t = 0; t < 2 * period + phase; ++t) t3();
            int newPeriod = period;
            if (!test) {
              s3.use(w3, rx[1], y, rz[1], none);  // one delay step mid-pulse
              newPeriod = 3 * repeator::kDelays[delay] + repeator::kDelays[repeator::nextSetting(delay)];
            } else {
              // swap one ring trace (non-Repeator cell) to the other colour, in place
              std::vector<std::pair<int, int>> tc;
              for (auto c : ring) { bool isR = false; for (int i = 0; i < 4; ++i) isR |= (c.first == rx[i] && c.second == rz[i]); if (!isR) tc.push_back(c); }
              auto [sx, sz] = tc[size_t(pos / 2) % tc.size()];
              p3(sx, sz, tr == kRedTrace ? kBlueTrace : kRedTrace);
            }
            for (int t = 0; t < 2 * newPeriod; ++t) t3();
            int rises = 0, high = 0, last = 0;
            for (int t = 0; t < 4 * newPeriod; ++t) {
              t3();
              const int v = stValue(w3.getState(ox + 3, y, oz)) > 0;
              rises += v && !last; high += v; last = v;
            }
            bool bad = rises > 4 || high != rises;
            if (test) {
              p3(ox + 1, oz, kAir);  // cut the ring = stop the clock
              for (int t = 0; t < 32 * 4; ++t) t3();
              int live = 0;
              for (int z = 0; z < 16; ++z) for (int x = 0; x < 16; ++x) live += stValue(w3.getState(x, y, z)) > 0;
              if (live) bad = true;
              ++f6Cases; if (bad) { ++f6Bad; if (f6Log.size() < 900) f6Log += std::format(" [d{} pos{} ph{}: rises {} high {} live {}]", delay, pos, phase, rises, high, live); }
            } else {
              ++e5Cases; if (bad) { ++e5Bad; if (e5Log.size() < 900) e5Log += std::format(" [d{} ph{}: rises {} high {}]", delay, phase, rises, high); }
            }
          }
        }
    std::printf("  [info] HOTSWAP-DELAY: %d cases, bad %d%s\n", e5Cases, e5Bad, e5Log.c_str());
    std::printf("  [info] HOTSWAP-TRACE: %d cases, bad %d%s\n", f6Cases, f6Bad, f6Log.c_str());
    check(e5Bad == 0, "DOUBLE-PULSE HOTSWAP-DELAY: cycling a running Repeator's delay mid-pulse never adds a pulse");
    check(f6Bad == 0, "DOUBLE-PULSE HOTSWAP-TRACE: Trace colour swap mid-pulse leaves no ghost; all activity dies 32x4 ticks after the ring is cut");
  }
  {  // PlayN (JD): Fusor adder + Breakor rename (old lithokraft:fusor id = Breakor)
    const auto& reg = blockRegistry();
    const cCompBehavior* fb = cBehaviorCatalog::get().component(kFusorSum);
    const auto oldId = reg.findById("lithokraft:fusor");
    check(fb != nullptr && oldId && *oldId == kFusor && kFusorSum != kFusor && reg.def(kFusor).name == "Breakor" && reg.def(kFusorSum).name == "Fusor" &&
              cBehaviorCatalog::get().component(kFusor) != nullptr,
          "FUSOR: new adder has a fresh id; old lithokraft:fusor saves load as the Breakor");
    bool ok = fb != nullptr;
    for (int f = 0; ok && f < 4; ++f) {
      auto run = [&](int b, int l, int r) {
        ComponentInputs in; in.side[size_t(dirBack(f))] = b; in.side[size_t(dirLeft(f))] = l; in.side[size_t(dirRight(f))] = r;
        ComponentMemory m{}; return fb->tick(in, makeState(0, f), m).out;
      };
      ok = run(10, 20, 0) == 30 && run(10, 0, 20) == 30 && run(40, 40, 0) == 64 && run(0, 0, 0) == 0 && run(5, 0, 0) == 5 && run(0, 7, 0) == 7;
    }
    check(ok, "FUSOR: front = min(64, back + side) for all 4 facings");
  }  {  // Randor (JD 2026-10-31): range bounds, reroll on a rising clock only, held output, determinism
    const cCompBehavior* rb = cBehaviorCatalog::get().component(kRandor);
    check(rb != nullptr, "RANDOR: registered as a component");
    if (rb) {
      const int f = 0;
      auto run = [&](int backN, int clk, uint64_t rng, ComponentMemory& m, CellState& st) {
        ComponentInputs in; in.side[size_t(dirBack(f))] = backN; in.side[size_t(dirLeft(f))] = clk; in.rng = rng;
        const ComponentStep r = rb->tick(in, st, m); m.out = uint8_t(r.out); st = r.state; return r.out;
      };
      bool bounds = true, cover = true;
      for (int n : {0, 1, 2, 7, 33, 64, 100}) {
        const int hi = std::clamp(n, std::max(randor::kLow, 1), 64);
        ComponentMemory m{}; CellState st = makeState(0, f);
        std::vector<int> seen(65, 0);
        for (int i = 0; i < 4000; ++i) {
          run(n, 64, randor::rollSeed(1234, 77, uint64_t(i)), m, st);
          const int v = run(n, 0, 0, m, st);
          if (v < randor::kLow || v > hi) bounds = false;
          if (v >= 0 && v <= 64) seen[size_t(v)] = 1;
          if (stValue(st) != hi) bounds = false;  // readout = the range N
        }
        for (int v = std::max(randor::kLow, 1); v <= hi; ++v) cover = cover && seen[size_t(v)];
      }
      check(bounds, "RANDOR: every roll is in kLow..N (N clamped 1..64) and the state value is N");
      check(cover, "RANDOR: every value kLow..N appears over 4000 rolls");
      {  // 2026-10-31 (JD): no value twice in a row; uniform over the allowed values
        bool noRep = true, uni = true, never0 = true;
        for (int n : {2, 7, 64}) {
          ComponentMemory m{}; CellState st = makeState(0, f);
          std::vector<int> cnt(65, 0); int prev = -1;
          const int kRolls = 20000;
          for (int i = 0; i < kRolls; ++i) {
            run(n, 64, randor::rollSeed(4321, 9, uint64_t(i)), m, st);
            const int v = run(n, 0, 0, m, st);
            if (v == prev) noRep = false;
            if (v == 0) never0 = false;
            prev = v; ++cnt[size_t(v)];
          }
          const int c = n - std::max(randor::kLow, 0) + 1;
          const double exp = double(kRolls) / c;
          for (int v = std::max(randor::kLow, 0); v <= n; ++v)
            if (cnt[size_t(v)] < exp * 0.8 || cnt[size_t(v)] > exp * 1.2) uni = false;
        }
        check(noRep, "RANDOR: never the same value twice in a row (N = 2, 7, 64; 20000 pulses each)");
        check(never0, "RANDOR: output is never 0 (range 1..N, never off)");
        check(uni, "RANDOR: no-repeat rolls stay uniform (every value within 20% of the mean)");
      }
      ComponentMemory m{}; CellState st = makeState(0, f);
      const int first = (run(64, 64, randor::rollSeed(9, 1, 1), m, st), run(64, 0, 0, m, st));
      bool held = true, edgeOnly = true;
      for (int t = 0; t < 50; ++t) held = held && run(64, 0, randor::rollSeed(9, 1, uint64_t(t + 5)), m, st) == first;
      run(64, 64, randor::rollSeed(9, 1, 100), m, st);
      const int second = m.history;
      for (int t = 0; t < 50; ++t) edgeOnly = edgeOnly && run(64, 64, randor::rollSeed(9, 1, uint64_t(t + 200)), m, st) == second;
      check(held && first > 0, "RANDOR: output held at the rolled strength with the clock low (not a pulse)");
      check(edgeOnly, "RANDOR: a clock held high does not reroll (rising edge only)");
      int changes = 0, last = second;
      for (int t = 0; t < 64; ++t) { run(64, 64, randor::rollSeed(9, 1, uint64_t(t + 500)), m, st); const int v = run(64, 0, 0, m, st); changes += v != last; last = v; }
      check(changes >= 48, "RANDOR: each rising clock edge rerolls");
      bool det = randor::rollSeed(42, 1000, 7) == randor::rollSeed(42, 1000, 7) && randor::rollSeed(42, 1000, 7) != randor::rollSeed(43, 1000, 7) &&
                 randor::rollSeed(42, 1000, 7) != randor::rollSeed(42, 1001, 7) && randor::rollSeed(42, 1000, 7) != randor::rollSeed(42, 1000, 8);
      ComponentMemory a{}, b{}; CellState sa = makeState(0, f), sb = makeState(0, f);
      for (int t = 0; t < 200; ++t) { const int c = (t & 1) ? 64 : 0; const uint64_t rs = randor::rollSeed(5, 6, uint64_t(t)); det = det && run(40, c, rs, a, sa) == run(40, c, rs, b, sb); }
      check(det, "RANDOR: splitmix64 rolls are deterministic in (world seed, cell, tick)");
      std::printf("  [info] RANDOR: kLow %d, first roll %d, second %d, rerolls %d/64\n", randor::kLow, first, second, changes);
    }
  }
  {  // 2026-11-05 (JD): Dividor, front = floor(back / 2), 1 tick, 0 in -> 0 out (no quirk modes).
    const cCompBehavior* db = cBehaviorCatalog::get().component(kDividor);
    const auto did = blockRegistry().findById("lithokraft:dividor");
    check(db != nullptr && did && *did == kDividor && blockRegistry().def(kDividor).name == "Dividor" && roleOf(blockRegistry().def(kDividor)) == SimRole::Dividor,
          "DIVIDOR: lithokraft:dividor registered with its behaviour and role");
    struct DC { int in, want; };
    const DC dcases[] = {{64, 32}, {3, 1}, {1, 0}, {0, 0}};
    for (const DC& c : dcases) {
      bool ok = db != nullptr;
      for (int f = 0; ok && f < 4; ++f) {
        ComponentInputs in; in.side[size_t(dirBack(f))] = uint8_t(c.in);
        ComponentMemory m{}; ok = db->tick(in, makeState(0, f), m).out == c.want;
      }
      char msg[96];
      std::snprintf(msg, sizeof msg, "DIVIDOR: back %d -> front %d (all four facings)", c.in, c.want);
      check(ok, msg);
    }
    bool sidesIgnored = db != nullptr;
    for (int f = 0; sidesIgnored && f < 4; ++f) {
      ComponentInputs in; in.side[size_t(dirLeft(f))] = 40; in.side[size_t(dirRight(f))] = 40;
      ComponentMemory m{}; sidesIgnored = db->tick(in, makeState(0, f), m).out == 0;
    }
    check(sidesIgnored, "DIVIDOR: side inputs are ignored (only the back is halved)");
    const int ddelay = db && db->queuedOutput() ? db->delayTicks(0, 1234, 100) : -1;
    cScheduledOutput dline;
    int dseen = -1;
    for (uint64_t t = 90; t < 200 && dseen < 0; ++t) {
      const cScheduledOutput::Out raw{uint8_t(t >= 100 ? 32 : 0), 0, false};
      if (dline.step(t, ddelay, raw).out == 32) dseen = int(t - 100);
    }
    check(ddelay == 1 && dseen == 1, "DIVIDOR DELAY: input change -> output change in exactly 1 tick");
  }
  {  // 2026-11-04 (JD t4304u): component delays through the shared cScheduledOutput line (exact tick counts).
    struct D { SimRole r; BlockId id; int want; const char* name; };
    const D table[] = {{SimRole::cFusor, kFusor, 2, "Breakor"}, {SimRole::cFusorSum, kFusorSum, 6, "Fusor"}, {SimRole::Memor, kMemor, 8, "Memor"},
                       {SimRole::Thresholdor, kThresholdor, 1, "Thresholdor"}, {SimRole::Redirector, kRedirector, 4, "Multiplexor"},
                       {SimRole::Pulsor, kPulsor, 1, "Pulsor"},  // PlayB: the Routor keeps its own 1-tick line (ROUTOR tests)
                       {SimRole::Latchor, kLatchor, 2, "Latchor (fast_latch)"}, {SimRole::Catchor, kCatchor, 1, "Catchor"},
                       {SimRole::Keyboard, kKeyboard, 6, "Keyboard"}, {SimRole::Switch, kSwitch, 2, "Switch"}, {SimRole::cButton, kButton, 2, "Button"}};
    for (const D& d : table) {
      (void)d.r;
      cScheduledOutput line;
      const cCompBehavior* cb = cBehaviorCatalog::get().component(d.id);
      if (!cb) { check(false, d.name); continue; }
      const int delay = cb->queuedOutput() ? cb->delayTicks(0, 1234, 100) : -1;
      int seen = -1;
      for (uint64_t t = 90; t < 200 && seen < 0; ++t) {
        const cScheduledOutput::Out raw{uint8_t(t >= 100 ? 40 : 0), 0, false};
        if (line.step(t, delay, raw).out == 40) seen = int(t - 100);
      }
      char msg[128];
      std::snprintf(msg, sizeof msg, "DELAY %s: input change -> output change in exactly %d ticks", d.name, d.want);
      check(delay == d.want && seen == d.want, msg);
    }
    const cCompBehavior* rb = cBehaviorCatalog::get().component(kRandor);
    bool rndOk = rb != nullptr && rb->queuedOutput();
    for (uint64_t t = 0; rb && t < 200; ++t) rndOk = rndOk && rb->delayTicks(0, 77 + int64_t(t), t) == 8;
    check(rndOk, "DELAY Randor (PlayB, JD t4331u): fixed 8 ticks for every cell and tick");
    {
      const cCompBehavior* pb = cBehaviorCatalog::get().component(kRepeator);
      check(pb && !pb->queuedOutput() && pb->delayTicks(repeator::encode(0, 0, 3, false), 1, 1) == repeator::delayTicks(repeator::encode(0, 0, 3, false)),
            "DELAY Repeator: its class reports its own setting (2..64 line kept in tick)");
    }
  }
  {  // PlayB (JD t4343u-t4349u): Routor = two paths that never mix, 1 tick, Alt + click cycles the 3 modes.
    const components::cRoutor ro;
    const int f = 0;  // north
    const int B = dirBack(f), L = dirLeft(f), R = dirRight(f);
    auto run = [&](int mode, int back, int left, int right, int ticks, ComponentMemory& m) {
      ComponentInputs in; in.side[size_t(B)] = back; in.side[size_t(L)] = left; in.side[size_t(R)] = right; in.side[size_t(f)] = 50;  // front input is never read
      const CellState st = makeState(0, f, mode, false);
      CellState o = st;
      for (int i = 0; i < ticks; ++i) { const ComponentStep k = ro.tick(in, st, m); m.out = uint8_t(k.out); m.out2 = uint8_t(k.out2); o = k.state; }
      return o;
    };
    auto em = [&](int dir, int mode, const ComponentMemory& m) { return ro.emitToward(dir, makeState(0, f, mode, false), m); };
    {  // 1-tick delay
      ComponentMemory m; run(0, 30, 0, 0, 1, m);
      const bool t1 = em(L, 0, m) == 0; run(0, 30, 0, 0, 1, m);
      check(t1 && em(L, 0, m) == 30, "ROUTOR: input -> output in exactly 1 tick");
    }
    {  // mode 1
      ComponentMemory m; run(0, 30, 0, 20, 3, m);
      check(em(L, 0, m) == 30 && em(f, 0, m) == 20 && em(R, 0, m) == 0 && em(B, 0, m) == 0,
            "ROUTOR mode 1: back -> left, right -> front, nothing else");
      ComponentMemory a; run(0, 30, 0, 0, 3, a);
      check(em(f, 0, a) == 0, "ROUTOR mode 1: paths never mix (back alone leaves the front dark)");
    }
    {  // mode 2
      ComponentMemory m; run(1, 30, 20, 0, 3, m);
      check(em(R, 1, m) == 30 && em(f, 1, m) == 20 && em(L, 1, m) == 0, "ROUTOR mode 2: back -> right, left -> front");
      ComponentMemory a; run(1, 0, 20, 0, 3, a);
      check(em(R, 1, a) == 0, "ROUTOR mode 2: paths never mix (left alone leaves the right dark)");
    }
    {  // mode 3
      ComponentMemory m; run(2, 30, 0, 20, 3, m);
      check(em(f, 2, m) == 30 && em(L, 2, m) == 20 && em(R, 2, m) == 0, "ROUTOR mode 3: back -> front, right -> left");
      ComponentMemory n; run(2, 30, 20, 0, 3, n);
      check(em(f, 2, n) == 30 && em(R, 2, n) == 20 && em(L, 2, n) == 0, "ROUTOR mode 3: left -> right");
      ComponentMemory e; run(2, 0, 0, 20, 3, e); run(2, 0, 20, 20, 3, e);  // the far side echoes back: the first side keeps driving
      check(em(L, 2, e) == 20 && em(R, 2, e) == 0 && em(f, 2, e) == 0, "ROUTOR mode 3: the side path holds its direction, no echo, never reaches the front");
    }
    {  // Alt + click cycles 1 -> 2 -> 3 -> 1; plain click does nothing
      ComponentMemory m;
      CellState st = makeState(0, f, 0, false);
      const bool m2 = components::cRoutor::mode(st = *ro.useAlt(st, m)) == 1;
      const bool m3 = components::cRoutor::mode(st = *ro.useAlt(st, m)) == 2;
      const bool m1 = components::cRoutor::mode(st = *ro.useAlt(st, m)) == 0;
      check(m2 && m3 && m1 && !ro.use(st, m), "ROUTOR: Alt + click cycles mode 1 -> 2 -> 3 -> 1 (mode in the cell state, saved); plain click does nothing");
    }
    check(std::string(blockRegistry().def(kRedirectorSelect).name) == "Routor" && playerUsable(blockRegistry(), kRedirectorSelect),
          "ROUTOR: the block is lithokraft:routor (old redirector_select alias) and reachable in game");
  }
  {  // PlayN (JD 11-05 Randor chain video): a hard-powered solid block drives Trace on ALL its other faces,
     // and a new hard-power value reaches every one of them (not only the face toward one reader).
    ChunkMap m; m.setRegistry(&blockRegistry());
    { auto col = std::make_unique<ChunkColumn>(); col->cx = 0; col->cz = 0; m.insert(std::move(col)); }
    const int yy = 10;
    for (int x = 0; x < 16; ++x) for (int z = 0; z < 16; ++z) m.set(x, yy - 1, z, kWaferSilicon);
    cSignalSim s;
    auto P = [&](int x, int z, BlockId b, CellState st = 0) { m.set(x, yy, z, b); m.setState(x, yy, z, st); s.onBlockChanged(x, yy, z, b); };
    auto R = [&](int n) { for (int i = 0; i < n; ++i) s.tick(m, 0, 0, 100, [](int, int, int) {}); };
    auto V = [&](int x, int z) { return stValue(m.getState(x, yy, z)); };
    auto drive = [&](int x, int z, int v) { m.setState(x, yy, z, CellState(v)); s.invalidateNetwork(); };
    // back Trace (2,8) -> Memor (3,8) facing E -> block (4,8) -> Trace N (4,7), S (4,9), E (5,8)
    // -> Repeators facing out (4,6) N, (4,10) S, (6,8) E -> Trace beyond each.
    P(2, 8, kRedTrace); P(3, 8, kMemor, makeState(0, 1)); P(4, 8, kWaferSilicon);
    P(4, 7, kRedTrace); P(4, 9, kRedTrace); P(5, 8, kRedTrace);
    m.set(4, yy + 1, 8, kRedTrace); m.setState(4, yy + 1, 8, 0); s.onBlockChanged(4, yy + 1, 8, kRedTrace);  // PlayN: TOP face
    P(4, 6, kRepeator, makeState(0, 0)); P(4, 10, kRepeator, makeState(0, 2)); P(6, 8, kRepeator, makeState(0, 1));
    // Randors clocked by each Repeator (side input), range from an on Switch behind, output Trace in front.
    P(4, 5, kRandor, makeState(0, 1)); P(3, 5, kSwitch); P(5, 5, kRedTrace);
    P(4, 11, kRandor, makeState(0, 1)); P(3, 11, kSwitch); P(5, 11, kRedTrace);
    P(7, 8, kRandor, makeState(0, 0)); P(7, 9, kSwitch); P(7, 7, kRedTrace);
    P(0, 8, kSwitch); P(1, 8, kRedTrace);
    R(2); for (auto [ux, uz] : {std::pair{3, 5}, {3, 11}, {7, 9}}) s.use(m, ux, yy, uz, [](int, int, int) {});
    R(4); s.use(m, 0, yy, 8, [](int, int, int) {}); R(40);  // switch -> 2 Trace -> Memor stores 63
    std::printf("  [info] memor %d back %d; block faces N %d S %d E %d; past repeators N %d S %d E %d\n", V(3, 8), V(2, 8), V(4, 7), V(4, 9), V(5, 8), V(4, 5), V(4, 11), V(7, 8));
    check(V(4, 7) == 63 && V(4, 9) == 63 && V(5, 8) == 63 && stValue(m.getState(4, yy + 1, 8)) == 63, "HARD BLOCK (JD Randor chain): Memor 63 -> block lights Trace on ALL faces (N/S/E/top) at 63");
    const int r0[3] = {V(5, 5), V(5, 11), V(7, 7)};
    std::printf("  [info] randors N %d S %d E %d\n", r0[0], r0[1], r0[2]);
    const bool rep1 = r0[0] > 0 && r0[1] > 0 && r0[2] > 0;
    check(rep1, "HARD BLOCK chain: all three Repeators past the block clock their Randor (each Randor outputs a roll)");
    P(1, 8, kSwitch); s.use(m, 1, yy, 8, [](int, int, int) {}); R(40);  // a nearer switch: Memor input 64 -> stores 64
    std::printf("  [info] memor %d; block faces after 64: N %d S %d E %d\n", V(3, 8), V(4, 7), V(4, 9), V(5, 8));
    check(V(4, 7) == 64 && V(4, 9) == 64 && V(5, 8) == 64 && stValue(m.getState(4, yy + 1, 8)) == 64, "HARD BLOCK: a new Memor value (64) reaches every face of the block (incl. top)");
    std::printf("  [info] randors after the new value: N %d S %d E %d\n", V(5, 5), V(5, 11), V(7, 7));
    check(V(5, 5) != r0[0] && V(5, 11) != r0[1] && V(7, 7) != r0[2],
          "RANDOR CHAIN (JD 11-05): a new value into the hard block = fresh Repeator edge on all three branches; every Randor re-rolls");
  }
  return fails;
}

}  // namespace lithokraft
