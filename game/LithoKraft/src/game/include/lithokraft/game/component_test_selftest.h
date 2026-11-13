// Component Test world self-test (2026-11-12, JD t5295u / t5296u). Headless and deterministic: the
// pad grid, the flat Wafer Silicon generator (far chunks too), the spiral labels, the components
// on the ground, the world picker entry and the --component-test flag. Returns failures.
#pragma once

namespace lithokraft {
int runComponentTestSelfTest();
}  // namespace lithokraft
