#pragma once
#include <string>
namespace lithokraft {
/// --control-test mode: control-serialization proofs (determinism of the
/// stamped stream through the real cSimThread in both modes, stream format
/// round-trips, ordering/accounting checks, the 20k-tick scripted soak).
/// Returns the number of failed checks (0 = all pass), like
/// runHandshakeTests.
int runControlTests();
/// --control-scenario TICKS legacy|handshake: the circuit-scene scenario
/// driven through the serialized control stream on the real cSimThread
/// (paused + stepped, tick-indexed stamped events), printing the outcome
/// hash. The no-input baseline prints the same scenario with an empty
/// stream. Returns 0 on success.
int runControlScenario(int ticks, const std::string& mode);
/// Selftest section: pure cControlStream unit checks (stamping, ordering,
/// accounting, format round-trip). Returns the number of failed checks;
/// every check prints a [PASS]/[FAIL] line prefixed "control:".
int runControlSelfTest();
/// Force 16 (Mode 416; brief JD and Argon, WWW-LLE-416-20261117; handshake JD and Elfie):
/// FrameGate rule checks, then the circuit scenario for `ticks` world ticks through the real
/// cSimThread with a render thread gated per RenderShiftMode. Every arm must end on the same
/// state hash as Sync512 and as a run with no render thread. allModes = all six, else Sync512 +
/// Mode416. Returns the number of failed checks. --force16-test [TICKS] [all].
int runForce16Test(int ticks, bool allModes);
}  // namespace lithokraft
