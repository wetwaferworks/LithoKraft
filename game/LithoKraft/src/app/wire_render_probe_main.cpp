// 2026-11-01 (JD): standalone runner for cWireRenderProbe (not in CMake; linked against PlayN libs).
#include <cstdio>
#include "lithokraft/content/wire_render_probe.h"
int main() {
  lithokraft::cWireRenderProbe p;
  const int missed = p.run(120);
  std::printf(missed ? "WIRE RENDER PROBE: MISMATCH\n" : "WIRE RENDER PROBE: OK\n");
  return missed ? 1 : 0;
}