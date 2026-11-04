#pragma once
// 2026-11-01 (JD): headless wire render probe. Long Trace runs (> 64) across chunk borders, a Repeator
// mid-run and a Pulsor relay chain, stepped tick by tick. Each tick compares the logic state of every
// Trace cell with what the renderer is told: a cell whose state changed must reach the dirty callback
// (that is what queues its sub-chunk for a remesh; the mesher bakes the cell state into the mesh).
// Also cross-checks the TraceStrengthBuffer slot (informational: the shader does not read it yet).
// Single-threaded, no locks.
namespace lithokraft {

class cWireRenderProbe {
 public:
  /// Runs `ticks` ticks; prints a per-tick mismatch table. Returns the number of missed-remesh cells.
  int run(int ticks);
};

}  // namespace lithokraft