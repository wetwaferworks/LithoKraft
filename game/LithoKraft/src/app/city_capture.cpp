// city_capture: headless stills + flyover frames of Silicon Cities on a real Wafer
// (Cortana 2026-10-16; cities design: Argon). Same production path and software renderer as
// interchange_capture: createDimensions + Dimension::load, real mesher, real textures.
// Never opens a window. Usage: city_capture <outdir> [frames=600]
//   <outdir>/city_overview.png, city_interchange.png, city_street.png, frames/frame_NNNN.png
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "litholattice/core/math.h"
#include "litholattice/core/threading.h"
#include "litholattice/mcp/mcp_senses.h"
#include "litholattice/procgen/silicon_cities.h"
#include "litholattice/render/block_materials.h"
#include "litholattice/render/mesher.h"
#include "litholattice/render/software_renderer.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/world/dimension.h"
#include "lithokraft/content/appearance.h"  // game look: Custom blocks (ramps, trace) drawn
#include "lithokraft/content/blocks.h"
#include "lithokraft/game/dimensions.h"
#include "lithokraft/game/resource_manager.h"

namespace {
namespace fs = std::filesystem;
using litholattice::Vec3;
constexpr int kWorldChunks = 36;  // 576 blocks: sectors 0..2 on each axis
constexpr int kImgW = 1280, kImgH = 720;

// Source-root resolution, same precedence as biome_capture (2026-10-18):
// LITHO_SOURCE_ROOT wins when set; otherwise walk up from the exe's folder
// (the tree this tool was built in), then from the cwd. This copy already had
// the env first; the exe-folder step is new -- without it, a run from an
// unrelated cwd with no env set silently fell back to "." and captured with
// no assets loaded (the magenta missing-texture fill).
fs::path g_exeDir;
std::string sourceRoot() {
  auto ok = [](const fs::path& r) { std::error_code ec; return fs::exists(r / "game" / "LithoKraft" / "assets" / "worldgen" / "wafer.json", ec); };
  if (const char* env = std::getenv("LITHO_SOURCE_ROOT")) if (ok(env)) return env;
  std::error_code ec;
  for (fs::path start : {g_exeDir, fs::current_path(ec)}) {
    for (fs::path p = start; !p.empty(); p = p.parent_path()) {
      if (ok(p)) return p.string();
      if (p == p.parent_path()) break;
    }
  }
  return ".";
}
bool writeFile(const std::string& path, const std::vector<uint8_t>& d) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const size_t n = std::fwrite(d.data(), 1, d.size(), f);
  std::fclose(f);
  return n == d.size();
}
std::string readFile(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return "";
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::string s(size_t(n), '\0');
  s.resize(std::fread(s.data(), 1, size_t(n), f));
  std::fclose(f);
  return s;
}
struct Mesh { std::vector<litholattice::PackedVertex> verts; float ox, oy, oz; };

bool renderView(const std::vector<Mesh>& meshes, const std::vector<uint32_t>& tex, uint32_t layers, Vec3 eye,
                Vec3 target, const std::string& path, bool quiet = false) {
  litholattice::cSoftwareRenderer sw;
  sw.setPerPixelShading(true);
  sw.setTextures(tex.data(), layers);
  sw.resize(kImgW, kImgH);
  sw.clear(0xFF94B3DB);
  Vec3 fwd{target.x - eye.x, target.y - eye.y, target.z - eye.z};
  const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
  fwd.x /= len; fwd.y /= len; fwd.z /= len;
  const litholattice::Mat4 vp = litholattice::mul(
      litholattice::perspective(float(70.0 * 3.14159265358979 / 180.0), float(kImgW) / float(kImgH), 0.1f, 2000.f),
      litholattice::lookDir(eye, fwd));
  for (const auto& m : meshes) {
    litholattice::Mat4 t = litholattice::Mat4::identity();
    t.m[12] = m.ox; t.m[13] = m.oy; t.m[14] = m.oz;
    const litholattice::Mat4 mvp = litholattice::mul(vp, t);
    sw.renderMesh(m.verts.data(), m.verts.size(), mvp.m);
  }
  const auto& fb = sw.framebuffer();
  std::vector<uint8_t> rgb(size_t(kImgW) * kImgH * 3);
  for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
    const uint32_t p = fb.pixels[i];
    rgb[3 * i] = uint8_t(p >> 16); rgb[3 * i + 1] = uint8_t(p >> 8); rgb[3 * i + 2] = uint8_t(p);
  }
  std::vector<uint8_t> png;
  if (!litholattice::mcp::encodePngRgb(kImgW, kImgH, rgb.data(), png) || !writeFile(path, png)) return false;
  if (!quiet) std::printf("[OK] %s (%zu bytes)\n", path.c_str(), png.size());
  return true;
}
Vec3 lerp(Vec3 a, Vec3 b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }
}  // namespace

int main(int argc, char** argv) {
  if (argc > 0 && argv[0]) g_exeDir = fs::absolute(argv[0]).parent_path();
  const std::string outDir = argc > 1 ? argv[1] : "./city-shots";
  const int frames = argc > 2 ? std::max(0, std::atoi(argv[2])) : 600;
  fs::create_directories(outDir + "/frames");
  litholattice::procgen::setSiliconCitiesOverride(1);  // capture tool: cities always on

  litholattice::WorldgenSettings settings;
  settings.seed = 20261011;
  const std::string root = sourceRoot();
  const std::string wj = readFile(root + "/game/LithoKraft/assets/worldgen/wafer.json");
  if (!wj.empty()) settings.generators["wafer"] = wj;
  litholattice::cDimensionReg reg = lithokraft::createDimensions(settings);
  litholattice::Dimension* wafer = reg.find("wafer");
  if (!wafer || !wafer->generator) { std::fprintf(stderr, "city_capture: no wafer generator\n"); return 1; }
  litholattice::JobPool pool(2);
  wafer->load(pool, kWorldChunks);
  litholattice::ChunkMap& map = wafer->chunks;

  // Wafer generator seed is pinned to 0 (makeGenerator); dimensionSeed(0, "wafer").
  const uint64_t cs = litholattice::procgen::citySeed(litholattice::procgen::dimensionSeed(0, "wafer"));
  std::vector<litholattice::procgen::CityHub> hubs;
  for (int sz = 0; sz < 2; ++sz)
    for (int sx = 0; sx < 2; ++sx) {
      const auto h = litholattice::procgen::cityHubAt(cs, sx, sz);
      hubs.push_back(h);
      std::printf("city_capture: hub sector (%d,%d) at (%d,%d) %s bus %u pad %d %s | link E %s, S %s\n", sx, sz, h.x, h.z,
                  litholattice::procgen::cityDistrictName(h.district), h.busWidth, h.padRadius, h.live ? "LIVE" : "dormant",
                  litholattice::procgen::cityLinkConnected(cs, sx, sz, true) ? "yes" : "no",
                  litholattice::procgen::cityLinkConnected(cs, sx, sz, false) ? "yes" : "no");
    }

  const std::string engineAssets = root + "/engine/assets", gameAssets = root + "/game/LithoKraft/assets";
  lithokraft::cResourceMgr rm({engineAssets, gameAssets, fs::path(gameAssets) / "packs" / "pack_64"});
  rm.reload();
  const auto materialsPtr = rm.materials();
  const litholattice::BlockRegistry& blocks = lithokraft::blockRegistry();
  litholattice::MeshContext mctx{blocks, *materialsPtr, &lithokraft::blockAppearance()};
  const litholattice::TextureRegistry& texReg = rm.textures();
  const uint32_t layers = uint32_t(texReg.textures().size());
  std::vector<uint32_t> tex(size_t(layers) * 256, 0xFFFF00FFu);
  for (uint32_t layer = 0; layer < layers; ++layer) {
    const litholattice::Image& img = texReg.texture(litholattice::TextureId{uint16_t(layer)}).image;
    if (img.empty() || img.width() <= 0 || img.height() <= 0) continue;
    for (int y = 0; y < 16; ++y) {
      const int y0 = y * img.height() / 16, y1 = std::max(y0 + 1, (y + 1) * img.height() / 16);
      for (int x = 0; x < 16; ++x) {
        const int x0 = x * img.width() / 16, x1 = std::max(x0 + 1, (x + 1) * img.width() / 16);
        uint32_t sum[4] = {0, 0, 0, 0};
        int n = 0;
        for (int sy = y0; sy < y1; ++sy)
          for (int sx = x0; sx < x1; ++sx, ++n) { const uint8_t* p = img.pixel(sx, sy); for (int k = 0; k < 4; ++k) sum[k] += p[k]; }
        const uint32_t r = sum[0] / uint32_t(n), g = sum[1] / uint32_t(n), b = sum[2] / uint32_t(n), a = sum[3] / uint32_t(n);
        tex[size_t(layer) * 256 + size_t(y) * 16 + size_t(x)] = (a << 24) | (r << 16) | (g << 8) | b;
      }
    }
  }
  std::vector<Mesh> meshes;
  for (int cx = 0; cx < kWorldChunks; ++cx)
    for (int cz = 0; cz < kWorldChunks; ++cz)
      for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
        auto in = litholattice::snapshotForMeshing(map, blocks, cx, sy, cz, 0, 0);
        if (!in) continue;
        litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, mctx);
        if (m.verts.empty()) continue;
        meshes.push_back(Mesh{std::move(m.verts), float(cx * 16), float(sy * 16), float(cz * 16)});
      }
  std::printf("city_capture: meshed %zu sub-chunks\n", meshes.size());
  const float by = float(litholattice::procgen::SiliconCitySettings{}.baseY);
  const auto& h0 = hubs[0];
  const auto& h1 = hubs[1];
  int failures = 0;
  // Stills: overview of hub (0,0), the M1/M2 interchange corner next to it, street level on a deck.
  if (!renderView(meshes, tex, layers, {h0.x + 70.f, by + 60.f, h0.z + 70.f}, {float(h0.x), by, float(h0.z)},
                  outDir + "/city_overview.png")) ++failures;
  if (!renderView(meshes, tex, layers, {h0.x + 40.f, by + 40.f, h0.z + 60.f}, {h0.x + 20.f, by + 10.f, h0.z + 20.f},
                  outDir + "/city_interchange.png")) ++failures;
  if (!renderView(meshes, tex, layers, {(h0.x + h1.x) * 0.5f, by + 19.f, h0.z - 0.5f}, {float(h1.x), by + 12.f, float(h0.z)},
                  outDir + "/city_street.png")) ++failures;
  // Ramps: under the south M1 ramp of hub (0,0) looking up, and a close look at the ramp top.
  if (!renderView(meshes, tex, layers, {h0.x + 15.f, by - 3.f, h0.z + h0.padRadius + 14.f}, {float(h0.x), by + 6.f, h0.z + h0.padRadius + 8.f},
                  outDir + "/city_underside.png")) ++failures;
  if (!renderView(meshes, tex, layers, {h0.x + 7.f, by + 16.f, h0.z + h0.padRadius + 2.f}, {float(h0.x), by + 6.f, h0.z + h0.padRadius + 16.f},
                  outDir + "/city_ramp_close.png")) ++failures;
  // Flyover: a loop over the four hubs (0,0) -> (1,0) -> (1,1) -> (0,1) -> (0,0) at deck+40.
  const Vec3 way[] = {{float(hubs[0].x) - 40.f, by + 55.f, float(hubs[0].z) - 40.f}, {float(hubs[1].x), by + 45.f, float(hubs[1].z) - 30.f},
                      {float(hubs[3].x) + 30.f, by + 50.f, float(hubs[3].z)}, {float(hubs[2].x), by + 45.f, float(hubs[2].z) + 30.f},
                      {float(hubs[0].x) - 30.f, by + 40.f, float(hubs[0].z)}};
  constexpr int kLegs = 4;
  for (int f = 0; f < frames; ++f) {
    const float u = float(f) / float(std::max(1, frames - 1)) * kLegs;
    const int leg = std::min(kLegs - 1, int(u));
    const float t = u - float(leg), s = t * t * (3.f - 2.f * t);
    const Vec3 eye = lerp(way[leg], way[leg + 1], s);
    const Vec3 ahead = lerp(way[leg], way[leg + 1], std::min(1.f, s + 0.35f));
    const Vec3 next = way[std::min(kLegs, leg + 2 > kLegs ? kLegs : leg + 1)];
    Vec3 target{ahead.x * 0.6f + next.x * 0.4f, by + 4.f, ahead.z * 0.6f + next.z * 0.4f};
    char name[64];
    std::snprintf(name, sizeof name, "/frames/frame_%04d.png", f);
    if (!renderView(meshes, tex, layers, eye, target, outDir + name, true)) ++failures;
  }
  std::printf("city_capture: %d flyover frames, %d failures\n", frames, failures);
  return failures ? 1 : 0;
}
