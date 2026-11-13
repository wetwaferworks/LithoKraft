#pragma once
namespace lithokraft {
/// Headless FrameHashRecorder contract tests (tick-paced hashes.txt).
/// Returns the number of failed checks; every check prints [PASS]/[FAIL].
int runFrameHashRecorderSelfTest();
}  // namespace lithokraft
