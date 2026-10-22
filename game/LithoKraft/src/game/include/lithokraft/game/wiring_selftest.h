// Headless checks for the 2026-10-13 wiring pass, Team B items B6
// (Etchor block + etch flow) and B7 (certificate signing flow), plus the
// player.lat v5 stack-data persistence both stand on.
#pragma once

namespace lithokraft {
/// Prints one PASS / FAIL line per check; returns the number of failures.
int runWiringSelfTest();
}  // namespace lithokraft
