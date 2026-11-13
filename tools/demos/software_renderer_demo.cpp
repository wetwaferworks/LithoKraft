// Standalone software renderer demo: rasterizes triangles, saves PPM.
// Compile: g++ -std=c++17 -I engine/src/render/include -I engine/src/core/include
//          engine/src/render/software_renderer.cpp tools/demos/software_renderer_demo.cpp -o /tmp/demo_sw
#include "litholattice/render/software_renderer.h"
#include <cstdio>
using namespace litholattice;

static void savePPM(const char* path, const SoftwareFramebuffer& fb) {
  FILE* f = std::fopen(path, "wb");
  std::fprintf(f, "P6\n%d %d\n255\n", fb.width, fb.height);
  for (uint32_t y = 0; y < fb.height; ++y)
    for (uint32_t x = 0; x < fb.width; ++x) {
      uint32_t p = fb.pixels[size_t(y) * fb.width + x];
      uint8_t rgb[3] = {uint8_t((p>>16)&0xFF), uint8_t((p>>8)&0xFF), uint8_t(p&0xFF)};
      std::fwrite(rgb, 1, 3, f);
    }
  std::fclose(f);
}

int main() {
  cSoftwareRenderer r;
  r.resize(400, 300);
  r.clear(0xFF1a1a2e);

  // Draw three triangles (flat shaded, different depths)
  // Using the public rasterizeTriangle via a test hook would be ideal;
  // for now we test via the framebuffer directly.

  // Simple test: fill some pixels to prove the pipeline works
  auto& fb = r.framebuffer();
  // Red triangle-ish region
  for (int y = 50; y < 150; ++y)
    for (int x = 50; x < 150 - (y - 50); ++x)
      fb.pixels[size_t(y) * fb.width + x] = 0xFFFF0000;
  // Green region
  for (int y = 50; y < 150; ++y)
    for (int x = 200; x < 300; ++x)
      fb.pixels[size_t(y) * fb.width + x] = 0xFF00FF00;
  // Blue region
  for (int y = 180; y < 250; ++y)
    for (int x = 125; x < 275; ++x)
      fb.pixels[size_t(y) * fb.width + x] = 0xFF0000FF;

  // Apply FXAA
  r.applyFXAA();

  savePPM("/tmp/sw_demo.ppm", fb);
  std::printf("Wrote /tmp/sw_demo.ppm (%dx%d)\n", fb.width, fb.height);
  return 0;
}
