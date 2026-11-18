// Control-serialization proofs (JD's Timing Handshake x control coupling,
// 2026-10-13). Three layers:
//   runControlSelfTest()  — pure cControlStream unit checks (also a --selftest
//                           section): stamping, ordering, accounting, the
//                           recording tap, the v1 format round-trip and its
//                           fail-closed parse.
//   runControlTests()     — --control-test: the determinism scenario driven
//                           through the REAL cSimThread + the serialized
//                           stream (paused + stepped, tick-indexed stamped
//                           events) in legacy and handshake modes, plus the
//                           20,000-tick scripted-input soak.
//   runControlScenario()  — --control-scenario TICKS legacy|handshake|baseline:
//                           one arm, hash only (the CLI companion).
#include "control_stream_test.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <system_error>
#include <thread>
#include <vector>

#include "handshake_test.h"
#include "litholattice/sim/control_stream.h"
#include "litholattice/sim/det_sched.h"
#include "litholattice/sim/frame_gate.h"
#include "litholattice/sim/sim_thread.h"
#include "lithokraft/content/blocks.h"

namespace lithokraft {
namespace {

using namespace litholattice;

int gFails = 0;
void check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFails;
}

// ------------------------------------------------------------ unit layer

ControlEvent editCmd(int32_t a) {
  EditCmd c{EditCmd::Kind::Command};
  c.command = SimCommand{9, 1, 2, 3, a, 0};
  return ControlEvent::editEvent(c, ControlSource::Test);
}

void testStampingAndOrder() {
  std::printf("== control stream: PRE stamping, (tick, seq) order, accounting\n");
  cControlStream cs;
  cs.submit(editCmd(1));
  cs.submit(editCmd(2));
  cs.submit(editCmd(3));
  cs.prePhase(7);
  std::vector<ControlEvent> out;
  check(cs.extractDue(6, out) == 0 && out.empty(), "control: nothing extracts before its stamp tick");
  check(cs.extractDue(7, out) == 3, "control: all three events extract at their stamp tick");
  check(out.size() == 3 && out[0].seq == 0 && out[1].seq == 1 && out[2].seq == 2 && out[0].tick == 7 &&
            out[2].tick == 7,
        "control: live events stamped (seq 0,1,2 / tick 7) in submission order");
  const ControlStreamStats st = cs.stats();
  check(st.submitted == 3 && st.stamped == 3 && st.applied == 3 && st.lateApplied == 0 && st.dropped == 0 &&
            st.orderViolations == 0 && st.pendingNow == 0,
        "control: accounting exact after a clean drain (0 late, 0 dropped, 0 violations)");
}

void testInjectedMerge() {
  std::printf("== control stream: pre-stamped injection merges by (tick, seq)\n");
  cControlStream cs;
  std::vector<ControlEvent> inj;
  ControlEvent a = editCmd(10); a.seq = 10; a.tick = 5; a.source = ControlSource::Replay;
  ControlEvent b = editCmd(11); b.seq = 11; b.tick = 3; b.source = ControlSource::Replay;
  inj.push_back(a);
  inj.push_back(b);
  cs.submitStamped(inj);
  cs.submit(editCmd(12));  // a live event submitted after the record
  cs.prePhase(9);
  std::vector<ControlEvent> out;
  check(cs.extractDue(9, out) == 3, "control: injected + live events all extract");
  check(out.size() == 3 && out[0].tick == 3 && out[0].seq == 11 && out[1].tick == 5 && out[1].seq == 10 &&
            out[2].tick == 9,
        "control: application order is (tick, seq), not arrival order");
  check(out.size() == 3 && out[2].seq == 12, "control: live seq continues past injected seqs (no collision)");
  const ControlStreamStats st = cs.stats();
  check(st.injected == 2 && st.stamped == 1 && st.applied == 3 && st.lateApplied == 2 && st.dropped == 0 &&
            st.orderViolations == 0,
        "control: past-stamp extractions are accounted as late, never dropped");
}

void testRecordingTap() {
  std::printf("== control stream: recording tap (the IDemoRecord feed)\n");
  cControlStream cs;
  cs.setRecording(true);
  cs.submit(editCmd(1));
  cs.submit(editCmd(2));
  cs.prePhase(4);
  std::vector<ControlEvent> out, rec;
  cs.extractDue(4, out);
  cs.takeRecord(rec);
  check(rec.size() == 2 && rec[0].seq == 0 && rec[1].seq == 1 && rec[0].tick == 4,
        "control: the tap reproduces the applied stream verbatim (stamps intact)");
  std::vector<ControlEvent> rec2;
  cs.takeRecord(rec2);
  check(rec2.empty(), "control: takeRecord drains (a second take is empty)");
  cs.setRecording(false);
  cs.submit(editCmd(3));
  cs.prePhase(5);
  cs.extractDue(5, out);
  cs.takeRecord(rec2);
  check(rec2.empty(), "control: recording off retains nothing");
}

bool sameEvent(const ControlEvent& a, const ControlEvent& b) {
  if (a.seq != b.seq || a.tick != b.tick || a.source != b.source || a.sourceId != b.sourceId || a.kind != b.kind)
    return false;
  switch (a.kind) {
    case ControlKind::Edit:
      return a.edit.kind == b.edit.kind && a.edit.facing == b.edit.facing && a.edit.block == b.edit.block &&
             a.edit.x == b.edit.x && a.edit.y == b.edit.y && a.edit.z == b.edit.z &&
             a.edit.command.code == b.edit.command.code && a.edit.command.x == b.edit.command.x &&
             a.edit.command.y == b.edit.command.y && a.edit.command.z == b.edit.command.z &&
             a.edit.command.a == b.edit.command.a && a.edit.command.b == b.edit.command.b;
    case ControlKind::Ray:
      return a.ray.ox == b.ray.ox && a.ray.oy == b.ray.oy && a.ray.oz == b.ray.oz && a.ray.dx == b.ray.dx &&
             a.ray.dy == b.ray.dy && a.ray.dz == b.ray.dz;
    case ControlKind::PlayerChunk:
      return a.chunk.cx == b.chunk.cx && a.chunk.cz == b.chunk.cz;
    case ControlKind::Locomotion:
      return a.loco.wishX == b.loco.wishX && a.loco.wishZ == b.loco.wishZ && a.loco.flags == b.loco.flags;
    case ControlKind::ControlFrame:
      return a.frame.moveForward == b.frame.moveForward && a.frame.moveStrafe == b.frame.moveStrafe &&
             a.frame.lookDx == b.frame.lookDx && a.frame.lookDy == b.frame.lookDy && a.frame.flags == b.frame.flags &&
             a.frame.hotbarSlot == b.frame.hotbarSlot;
  }
  return false;
}

std::vector<ControlEvent> sampleEvents() {
  std::vector<ControlEvent> v;
  ControlEvent e = editCmd(-77);
  e.seq = 0; e.tick = 100; e.source = ControlSource::Local;
  v.push_back(e);
  EditCmd use{EditCmd::Kind::Use};
  use.x = 24; use.y = 65; use.z = -30;
  ControlEvent u = ControlEvent::editEvent(use, ControlSource::McpPilot, 2);
  u.seq = 1; u.tick = 120;
  v.push_back(u);
  EditCmd place{EditCmd::Kind::Place};
  place.block = 4242; place.facing = 3;
  ControlEvent p = ControlEvent::editEvent(place, ControlSource::Script);
  p.seq = 2; p.tick = 121;
  v.push_back(p);
  ControlEvent r = ControlEvent::rayEvent(RayInput{8.5f, 70.25f, -3.5f, 0.0f, -1.0f, 0.5f}, ControlSource::Local);
  r.seq = 3; r.tick = 122;
  v.push_back(r);
  ControlEvent c = ControlEvent::chunkEvent(-7, 12, ControlSource::Local);
  c.seq = 4; c.tick = 123;
  v.push_back(c);
  ControlEvent l;
  l.kind = ControlKind::Locomotion; l.source = ControlSource::McpPilot; l.sourceId = 1;
  l.seq = 5; l.tick = 124; l.loco = LocomotionPayload{0.5f, -1.0f, 0x5};
  v.push_back(l);
  ControlEvent f;
  f.kind = ControlKind::ControlFrame; f.source = ControlSource::McpPilot; f.sourceId = 3;
  f.seq = 6; f.tick = 125; f.frame = ControlFramePayload{1.0f, -0.5f, 12.5f, -3.25f, 0x19, 4};
  v.push_back(f);
  return v;
}

void testFormat() {
  std::printf("== control stream: v1 format round-trip + fail-closed parse\n");
  const std::vector<ControlEvent> want = sampleEvents();
  ControlStreamHeader hdr;
  hdr.worldSeed = 0x1122334455667788ULL;
  hdr.mode = 1;
  hdr.tickRate = 512;
  hdr.phasesPerCycle = 4;
  hdr.declaredTicks = 2048;
  hdr.declaredEvents = want.size();
  std::vector<uint8_t> bytes;
  check(writeControlStream(hdr, want, bytes), "control: format writes");
  const ControlStreamParse p = parseControlStream(bytes.data(), bytes.size());
  check(p.ok, "control: format parses its own output");
  check(p.ok && p.header.worldSeed == hdr.worldSeed && p.header.mode == 1 && p.header.tickRate == 512 &&
            p.header.phasesPerCycle == 4 && p.header.declaredTicks == 2048 && p.header.declaredEvents == want.size(),
        "control: header round-trips (seed, mode, tick rate, phases, declared counts)");
  bool allSame = p.ok && p.events.size() == want.size();
  if (allSame)
    for (size_t i = 0; i < want.size(); ++i) allSame = allSame && sameEvent(p.events[i], want[i]);
  check(allSame, "control: every event kind round-trips field-for-field (edit/ray/chunk/locomotion/frame)");

  const ControlStreamParse trunc = parseControlStream(bytes.data(), bytes.size() - 3);
  check(!trunc.ok && trunc.events.empty(), "control: truncated stream fails closed (no partial events)");
  std::vector<uint8_t> corrupt = bytes;
  corrupt[corrupt.size() / 2] ^= 0x40;
  const ControlStreamParse bad = parseControlStream(corrupt.data(), corrupt.size());
  check(!bad.ok && bad.events.empty(), "control: a flipped payload byte fails the checksum, whole parse refused");
  std::vector<uint8_t> badMagic = bytes;
  badMagic[0] = 'X';
  const uint64_t sum = cDeterministicScheduler::hashBytes(badMagic.data(), badMagic.size() - 8);
  for (int i = 0; i < 8; ++i) badMagic[badMagic.size() - 8 + size_t(i)] = uint8_t((sum >> (8 * i)) & 0xff);
  const ControlStreamParse bm = parseControlStream(badMagic.data(), badMagic.size());
  check(!bm.ok, "control: bad magic refused even with a valid checksum");
  // Unknown kind with a valid checksum: rewrite event 0's kind byte.
  std::vector<uint8_t> badKind = bytes;
  badKind[48 + 16] = 0xEE;  // first record: header(48) + seq(8) + tick(8) -> kind byte
  const uint64_t sum2 = cDeterministicScheduler::hashBytes(badKind.data(), badKind.size() - 8);
  for (int i = 0; i < 8; ++i) badKind[badKind.size() - 8 + size_t(i)] = uint8_t((sum2 >> (8 * i)) & 0xff);
  const ControlStreamParse bk = parseControlStream(badKind.data(), badKind.size());
  check(!bk.ok && bk.events.empty(), "control: unknown event kind refused (fail-closed)");

  const std::string line = controlEventToText(want[1]);
  check(line.find("seq=1") != std::string::npos && line.find("tick=120") != std::string::npos &&
            line.find("kind=edit") != std::string::npos && line.find("mcp-pilot") != std::string::npos,
        "control: line format carries seq/tick/kind/source for inspection");
}

// ------------------------------------------------------- scenario driver

// The scripted stream: the §6.4 scenario's three inputs, expressed as
// stamped control events (plus the fixture PlayerChunk every arm shares).
//   tick 100: aim down at the first red trace -> Break
//   tick 120: Use the first repeator (explicit cell)
//   tick 150: aim again -> Place the trace back
std::vector<ControlEvent> scenarioScript(const ControlScenarioWorld& sc, bool withInputs) {
  std::vector<ControlEvent> v;
  uint64_t seq = 0;
  ControlEvent fixture = ControlEvent::chunkEvent(4, 4, ControlSource::Test);
  fixture.seq = seq++;
  fixture.tick = 0;
  v.push_back(fixture);
  if (!withInputs) return v;
  const float ox = float(sc.wireX) + 0.5f, oz = float(sc.wireZ) + 0.5f, oy = float(sc.y) + 3.0f;
  ControlEvent ray = ControlEvent::rayEvent(RayInput{ox, oy, oz, 0.f, -1.f, 0.f}, ControlSource::Test);
  ray.seq = seq++; ray.tick = 100;
  v.push_back(ray);
  ControlEvent brk = ControlEvent::editEvent(EditCmd{EditCmd::Kind::Break}, ControlSource::Test);
  brk.seq = seq++; brk.tick = 100;
  v.push_back(brk);
  EditCmd use{EditCmd::Kind::Use};
  use.x = sc.repX; use.y = sc.y; use.z = sc.repZ;
  ControlEvent u = ControlEvent::editEvent(use, ControlSource::Test);
  u.seq = seq++; u.tick = 120;
  v.push_back(u);
  ControlEvent ray2 = ray;
  ray2.seq = seq++; ray2.tick = 150;
  v.push_back(ray2);
  EditCmd place{EditCmd::Kind::Place};
  place.block = kRedTrace;
  place.facing = 0;
  ControlEvent pl = ControlEvent::editEvent(place, ControlSource::Test);
  pl.seq = seq++; pl.tick = 150;
  v.push_back(pl);
  return v;
}

struct DrivenResult {
  uint64_t hash = 0;
  ControlStreamStats stats{};
  std::vector<ControlEvent> record;
  bool stoppedCleanly = false;
  bool steppedAll = false;
  BlockId wireAfter = 0xffff;
};

DrivenResult driveScenario(int ticks, bool handshake, int workers, const std::vector<ControlEvent>& script) {
  DrivenResult res;
  ControlScenarioWorld sc = buildControlScenarioWorld();
  ChunkMap& w = sc.wafer->chunks;

  SimConfig cfg;
  cfg.tickRate = 512;
  cfg.radiusChunks = 8;
  cfg.belowWorldFill = sc.wafer->belowWorldFill;
  cfg.timingHandshake = handshake;
  cfg.handshakePhasesPerCycle = 4;
  cfg.handshakeWorkerThreads = workers;
  cSimThread st(w, blockRegistry(), std::move(sc.ws), std::move(cfg), [](std::shared_ptr<MeshInput>) {});
  cSignalWorldSim& sig = static_cast<cSignalWorldSim&>(st.simulation());

  st.controlStream().setRecording(true);
  if (!script.empty()) st.controlStream().submitStamped(script);
  st.setPaused(true);  // step-only driving: the stream, not the wall clock, decides
  st.start();

  bool ok = true;
  uint64_t stepped = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(240);
  while (stepped < uint64_t(ticks) && ok) {
    const uint64_t chunk = std::min<uint64_t>(128, uint64_t(ticks) - stepped);
    st.stepTicks(chunk);
    const uint64_t target = stepped + chunk;
    while (st.ticksStepped() < target) {
      if (std::chrono::steady_clock::now() > deadline) { ok = false; break; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stepped = target;
  }
  res.steppedAll = ok && st.stats().worldTick == uint64_t(ticks);
  res.stoppedCleanly = st.stop();
  res.stats = st.controlStream().stats();
  st.controlStream().takeRecord(res.record);
  res.wireAfter = w.get(sc.wireX, sc.y, sc.wireZ);
  res.hash = hashControlWorld(w, sig.signal(), uint64_t(ticks));
  return res;
}

bool recordMatches(const std::vector<ControlEvent>& record, const std::vector<ControlEvent>& script) {
  if (record.size() != script.size()) return false;
  for (size_t i = 0; i < script.size(); ++i)
    if (record[i].seq != script[i].seq || record[i].tick != script[i].tick || record[i].kind != script[i].kind)
      return false;
  return true;
}

void testDeterminism() {
  std::printf("== control determinism: circuit scenario through cSimThread + the stream (2048 ticks)\n");
  ControlScenarioWorld probe = buildControlScenarioWorld();
  check(probe.wireX >= 0 && probe.repX >= 0, "control: scenario world built (trace + repeator found)");
  const std::vector<ControlEvent> script = scenarioScript(probe, true);
  const std::vector<ControlEvent> fixtureOnly = scenarioScript(probe, false);
  probe.ws.reset();  // the probe world is done; arms rebuild their own

  const DrivenResult base = driveScenario(2048, false, 0, fixtureOnly);
  std::printf("  baseline (no inputs): hash=%016llx applied=%llu\n", (unsigned long long)base.hash,
              (unsigned long long)base.stats.applied);
  check(base.steppedAll && base.stoppedCleanly, "control: baseline arm stepped all 2048 ticks and stopped cleanly");
  check(base.stats.applied == fixtureOnly.size() && base.stats.dropped == 0 && base.stats.orderViolations == 0,
        "control: baseline arm applied exactly the fixture events, none dropped/misordered");

  const DrivenResult l1 = driveScenario(2048, false, 0, script);
  const DrivenResult l2 = driveScenario(2048, false, 0, script);
  const DrivenResult h2 = driveScenario(2048, true, 2, script);
  const DrivenResult h4 = driveScenario(2048, true, 4, script);
  std::printf("  script hashes: legacy=%016llx legacy#2=%016llx hs(w2)=%016llx hs(w4)=%016llx\n",
              (unsigned long long)l1.hash, (unsigned long long)l2.hash, (unsigned long long)h2.hash,
              (unsigned long long)h4.hash);
  check(l1.hash == l2.hash, "control: legacy scripted run is bit-identical across repeats");
  check(h2.hash == l1.hash && h4.hash == l1.hash,
        "control: handshake arms (2 and 4 workers) are bit-identical to legacy for the same stream");
  check(l1.hash != base.hash, "control: the scripted inputs changed the world (hash differs from no-input baseline)");
  check(l1.wireAfter == kRedTrace, "control: the tick-150 Place restored the trace through ray resolution");
  check(l1.steppedAll && h2.steppedAll && h4.steppedAll, "control: every arm stepped exactly 2048 world ticks");
  check(l1.stoppedCleanly && h2.stoppedCleanly && h4.stoppedCleanly, "control: every arm stopped cleanly");
  check(l1.stats.applied == script.size() && h4.stats.applied == script.size() &&
            l1.stats.dropped == 0 && h4.stats.dropped == 0 && l1.stats.orderViolations == 0 &&
            h4.stats.orderViolations == 0,
        "control: every scripted event applied exactly once, in order, in both modes");
  check(recordMatches(l1.record, script) && recordMatches(h4.record, script),
        "control: the recording tap replays the exact stamped stream that drove each arm");
}

// ------------------------------------------------------------------ soak

void testSoak() {
  std::printf("== control soak: 20,000 ticks, a stamped event every 25 ticks\n");
  ControlScenarioWorld probe = buildControlScenarioWorld();
  std::vector<ControlEvent> script;
  uint64_t seq = 0;
  ControlEvent fixture = ControlEvent::chunkEvent(4, 4, ControlSource::Test);
  fixture.seq = seq++;
  fixture.tick = 0;
  script.push_back(fixture);
  for (uint64_t t = 25; t < 20000; t += 25) {
    ControlEvent e;
    e.seq = seq++;
    e.tick = t;
    e.source = ControlSource::Test;
    switch ((t / 25) % 4) {
      case 0: {
        EditCmd c{EditCmd::Kind::Command};
        c.command = SimCommand{9, 0, 0, 0, int32_t(t), 0};
        e = ControlEvent::editEvent(c, ControlSource::Test);
        break;
      }
      case 1:
        e = ControlEvent::rayEvent(
            RayInput{float(probe.wireX) + 0.5f, float(probe.y) + 3.0f, float(probe.wireZ) + 0.5f, 0.f, -1.f, 0.f},
            ControlSource::Test);
        break;
      case 2: {
        EditCmd u{EditCmd::Kind::Use};
        u.x = probe.repX; u.y = probe.y; u.z = probe.repZ;
        e = ControlEvent::editEvent(u, ControlSource::Test);
        break;
      }
      default:
        e = ControlEvent::chunkEvent((t / 100) % 2 ? 5 : 4, 4, ControlSource::Test);
        break;
    }
    e.seq = seq - 1;
    e.tick = t;
    script.push_back(e);
  }
  probe.ws.reset();
  const DrivenResult r = driveScenario(20000, false, 0, script);
  std::printf("  soak: hash=%016llx applied=%llu injected=%llu maxPending=%llu record=%zu\n",
              (unsigned long long)r.hash, (unsigned long long)r.stats.applied, (unsigned long long)r.stats.injected,
              (unsigned long long)r.stats.maxPending, r.record.size());
  check(r.steppedAll, "control soak: all 20,000 ticks stepped");
  check(r.stoppedCleanly, "control soak: stopped cleanly");
  check(r.stats.injected == script.size() && r.stats.applied == script.size(),
        "control soak: every injected event applied (exhaustive, exactly-once)");
  check(r.stats.dropped == 0 && r.stats.orderViolations == 0 && r.stats.pendingNow == 0,
        "control soak: zero dropped, zero misordered, nothing left pending");
  check(recordMatches(r.record, script), "control soak: the record is the script, verbatim, end to end");
}

}  // namespace

int runControlSelfTest() {
  const int before = gFails;
  testStampingAndOrder();
  testInjectedMerge();
  testRecordingTap();
  testFormat();
  return gFails - before;
}

int runControlTests() {
  gFails = 0;
  runControlSelfTest();
  testDeterminism();
  testSoak();
  std::printf("%s\n", gFails ? "CONTROL TESTS: FAILED" : "CONTROL TESTS: OK");
  return gFails ? 1 : 0;
}

int runControlScenario(int ticks, const std::string& mode) {
  ControlScenarioWorld probe = buildControlScenarioWorld();
  const bool baseline = mode == "baseline";
  const std::vector<ControlEvent> script = scenarioScript(probe, !baseline);
  probe.ws.reset();
  const DrivenResult r = driveScenario(ticks, mode == "handshake", 2, script);
  std::printf("control-scenario ticks=%d mode=%s hash=%016llx applied=%llu dropped=%llu violations=%llu\n", ticks,
              mode.c_str(), (unsigned long long)r.hash, (unsigned long long)r.stats.applied,
              (unsigned long long)r.stats.dropped, (unsigned long long)r.stats.orderViolations);
  return (r.steppedAll && r.stoppedCleanly) ? 0 : 1;
}

// ------------------------------------------------- Force 16 (Mode 416)
// Brief: JD and Argon (WWW-LLE-416-20261117). Handshake (mutex-free published snapshots read at a
// phase boundary): JD and Elfie. The gate is render-side only, so every RenderShiftMode must end on
// the identical sim state. Each arm drives the §6.4 circuit scenario (with the scripted Break / Use /
// Place inputs) through the real cSimThread at 512 TPS, while a render thread pins the published
// front at phase boundaries, asks the FrameGate whether the completed-tick count reached a new frame
// tick, and on a frame reads the snapshot (blocks around the circuit) - read-only, never a write.

namespace {
using namespace litholattice;

struct Force16Arm {
  uint64_t hash = 0;
  uint64_t frames = 0;        // frames the gate admitted on the render thread
  uint64_t badFrameTicks = 0; // admitted ticks that were not frame ticks (must be 0)
  uint64_t reads = 0;         // snapshot block reads done by the render thread
  uint64_t lastCompleted = 0; // completed-tick count the render side saw last (must reach ticks)
  bool steppedAll = false, stoppedCleanly = false;
  double ms = 0;
};

Force16Arm driveForce16(int ticks, int mode /* -1 = no render thread */, const std::vector<ControlEvent>& script) {
  Force16Arm res;
  const auto t0 = std::chrono::steady_clock::now();
  ControlScenarioWorld sc = buildControlScenarioWorld();
  ChunkMap& w = sc.wafer->chunks;
  const int wx = sc.wireX, wy = sc.y, wz = sc.wireZ;
  SimConfig cfg;
  cfg.tickRate = 512;
  cfg.radiusChunks = 8;
  cfg.belowWorldFill = sc.wafer->belowWorldFill;
  cfg.timingHandshake = true;
  cfg.handshakePhasesPerCycle = 4;
  cfg.handshakeWorkerThreads = 4;
  cSimThread st(w, blockRegistry(), std::move(sc.ws), std::move(cfg), [](std::shared_ptr<MeshInput>) {});
  cSignalWorldSim& sig = static_cast<cSignalWorldSim&>(st.simulation());
  if (!script.empty()) st.controlStream().submitStamped(script);
  st.setPaused(true);
  st.start();

  // Single-thread WASM: start() falls back to a cooperative sim (pumpCooperative() runs the due
  // passes on this thread) and no render thread can spawn, so the render side then runs inline
  // between steps, still the sole stats() reader and still reading the published front only.
  const bool coop = st.cooperative();
  std::atomic<bool> stopRender{false};
  FrameGate gate(renderShiftModeFromInt(mode < 0 ? 0 : mode));
  uint64_t sink = 0;
  auto renderOnce = [&] {
    const uint64_t completed = st.stats().worldTick;  // published at the phase boundary (sole stats reader)
    res.lastCompleted = completed;
    if (gate.admit(completed)) {
      if (!gate.isFrameTick(gate.lastShownTick())) ++res.badFrameTicks;
      for (int dx = -2; dx <= 2; ++dx)
        for (int dz = -2; dz <= 2; ++dz) { sink += st.getBlock(wx + dx, wy, wz + dz); ++res.reads; }  // published front only
      ++res.frames;
    }
  };
  std::thread render;
  bool inlineRender = false;
  if (mode >= 0) {
    try {
      render = std::thread([&] {
        while (!stopRender.load(std::memory_order_acquire)) { renderOnce(); std::this_thread::yield(); }
      });
    } catch (const std::system_error&) {
      inlineRender = true;
    }
  }
  auto idle = [&] {  // one wait step: run the cooperative sim and the inline render side when threads are absent
    if (coop) st.pumpCooperative();
    if (inlineRender) renderOnce();
    std::this_thread::yield();
  };

  bool ok = true;
  uint64_t stepped = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(600);
  while (stepped < uint64_t(ticks) && ok) {
    // One tick per step: each step is one sim pass, so each completed tick reaches the
    // published stats at its own phase boundary (what the gate keys on in live play at 512 TPS).
    const uint64_t chunk = 1;
    st.stepTicks(chunk);
    const uint64_t target = stepped + chunk;
    while (st.ticksStepped() < target) {
      if (std::chrono::steady_clock::now() > deadline) { ok = false; break; }
      idle();
    }
    stepped = target;
  }
  if (mode < 0) {  // no render thread: this thread is the stats reader; wait for the last pass's publish
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while ((res.lastCompleted = st.stats().worldTick) < uint64_t(ticks) && std::chrono::steady_clock::now() < until) {
      if (coop) st.pumpCooperative();
      std::this_thread::yield();
    }
  }
  if (mode >= 0) {
    // Let the render side see the final published count, then stop it before the sim stops.
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < until) idle();
    stopRender.store(true, std::memory_order_release);
    if (render.joinable()) render.join();
  }
  if (sink == 0xFFFFFFFFFFFFFFFFull) std::printf(" ");  // keep the reads
  if (coop || inlineRender) std::printf("  (force16: %s%s)\n", coop ? "cooperative sim" : "threaded sim", inlineRender ? ", render side inline" : "");
  res.steppedAll = ok && st.ticksStepped() == uint64_t(ticks) && res.lastCompleted == uint64_t(ticks);
  res.stoppedCleanly = st.stop();
  res.hash = hashControlWorld(w, sig.signal(), uint64_t(ticks));
  res.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  return res;
}

int force16Fails = 0;
void f16check(bool ok, const char* what) {
  std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++force16Fails;
}

void force16GateUnit() {
  std::printf("== Force 16: FrameGate rule (frame_mask = (1u<<shift)-1, frame when (uint32_t(tick) & mask) == 0)\n");
  for (int m = 0; m < kRenderShiftModeCount; ++m) {
    const FrameGate g(renderShiftModeFromInt(m));
    char what[160];
    std::snprintf(what, sizeof what, "force16: %s mask 0x%X ratio %u:1 -> %u FPS at 512 TPS", renderShiftModeName(g.mode()),
                  g.frame_mask, g.ratio(), g.fpsAt(512));
    f16check(g.frame_mask == (1u << m) - 1u && g.fpsAt(512) == (512u >> m), what);
  }
  const FrameGate m416(RenderShiftMode::Mode416);
  f16check(m416.frame_mask == 0x1Fu && m416.fpsAt(512) == 16 && m416.ratio() == 32,
           "force16: Mode 416 = shift 5, mask 0x1F, 512 TPS / 16 FPS (32:1)");
  // One completed tick per phase boundary: exactly one frame per 32 ticks, all on frame ticks.
  for (int m = 0; m < kRenderShiftModeCount; ++m) {
    FrameGate g(renderShiftModeFromInt(m));
    uint64_t bad = 0;
    for (uint64_t t = 0; t <= 10000; ++t)
      if (g.admit(t) && !g.isFrameTick(g.lastShownTick())) ++bad;
    const uint64_t want = 10000 / (1u << m) + 1;  // frame ticks 0, 2^m, ... <= 10000
    char what[160];
    std::snprintf(what, sizeof what, "force16: %s over 0..10000 one tick per boundary admits %llu frames (want %llu), off-gate 0",
                  renderShiftModeName(g.mode()), (unsigned long long)g.admitted(), (unsigned long long)want);
    f16check(g.admitted() == want && bad == 0, what);
  }
  {  // Several ticks per pass (the sim publishes per pass): never a repeat, never off-gate, never early.
    FrameGate g(RenderShiftMode::Mode416);
    uint64_t t = 0, bad = 0, last = 0; bool any = false, early = false;
    for (int i = 0; i < 4000; ++i) {
      t += uint64_t(1 + (i * 7) % 5);
      if (g.admit(t)) {
        if (!g.isFrameTick(g.lastShownTick()) || (any && g.lastShownTick() <= last)) ++bad;
        if (g.lastShownTick() > t) early = true;
        last = g.lastShownTick(); any = true;
      }
    }
    f16check(bad == 0 && !early && g.admitted() == t / 32 + 1, "force16: Mode 416 with 1-5 ticks per pass shows each frame tick at most once, in order, none ahead of the sim");
  }
}

int runForce16(int ticks, bool allModes) {
  force16Fails = 0;
  force16GateUnit();
  std::printf("== Force 16: %d-tick circuit scenario through cSimThread (512 TPS, handshake 4x4) with a render-side gate\n", ticks);
  ControlScenarioWorld probe = buildControlScenarioWorld();
  f16check(probe.wireX >= 0 && probe.repX >= 0, "force16: scenario world built (trace + repeator found)");
  const std::vector<ControlEvent> script = scenarioScript(probe, true);
  probe.ws.reset();
  const Force16Arm none = driveForce16(ticks, -1, script);
  std::printf("  no render thread : hash=%016llx (%.0f ms)\n", (unsigned long long)none.hash, none.ms);
  f16check(none.steppedAll && none.stoppedCleanly, "force16: baseline arm stepped every tick and stopped cleanly");
  std::vector<int> modes = allModes ? std::vector<int>{0, 1, 2, 3, 4, 5} : std::vector<int>{0, 5};
  uint64_t sync512 = 0;
  for (int m : modes) {
    const Force16Arm a = driveForce16(ticks, m, script);
    if (m == 0) sync512 = a.hash;
    std::printf("  %-16s: hash=%016llx frames=%llu (max %d) reads=%llu off-gate=%llu (%.0f ms)\n",
                renderShiftModeName(renderShiftModeFromInt(m)), (unsigned long long)a.hash, (unsigned long long)a.frames,
                ticks / (1 << m) + 1, (unsigned long long)a.reads, (unsigned long long)a.badFrameTicks, a.ms);
    char what[200];
    std::snprintf(what, sizeof what, "force16: %s stepped all %d ticks, stopped cleanly, render saw frames, none off-gate",
                  renderShiftModeName(renderShiftModeFromInt(m)), ticks);
    f16check(a.steppedAll && a.stoppedCleanly && a.frames > 0 && a.frames <= uint64_t(ticks / (1 << m) + 1) && a.badFrameTicks == 0, what);
    std::snprintf(what, sizeof what, "force16: %s ends on the identical state hash as Sync512 and the no-render baseline",
                  renderShiftModeName(renderShiftModeFromInt(m)));
    f16check(a.hash == none.hash && a.hash == sync512, what);
  }
  std::printf("force16: %s (%d ticks)\n", force16Fails ? "FAILED" : "OK", ticks);
  return force16Fails;
}

}  // namespace

int runForce16Test(int ticks, bool allModes) { return runForce16(ticks, allModes); }

}  // namespace lithokraft
