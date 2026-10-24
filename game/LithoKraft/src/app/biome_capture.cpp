// Cortana 2026-10-15 0750: PORT portable sourceRoot()
// biome_capture: headless 3D screenshots of the 5 Wafer biomes.
// Uses the REAL game worldgen (ColumnGenerator + wafer.json pipeline), the REAL mesher,
// and the software renderer. No window, no GPU. Fixed seed for reproducibility.
// Output: PNGs in the directory given as argv[1] (default: ./biome-shots).
// LF line endings.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "litholattice/core/math.h"
#include "litholattice/mcp/mcp_senses.h"  // encodePngRgb
#include "litholattice/render/block_materials.h"
#include "litholattice/render/mesher.h"
#include "litholattice/render/software_renderer.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/world/column_generator.h"
#include "litholattice/world/dimension.h"
#include "litholattice/world/mesh_input.h"
#include "lithokraft/content/appearance.h"  // Cortana 2026-10-15 1755: litho scene uses the game looks
#include "lithokraft/content/blocks.h"  // blockRegistry()
#include "lithokraft/content/foliage.h"  // Cortana 2026-10-15 2010 [TOOLS]: --foliage
#include "lithokraft/game/dimensions.h"  // createDimensions
#include "lithokraft/game/resource_manager.h"  // ResourceManager (real pack textures/materials)
#include "lithokraft/game/scout.h"  // locateBiome
#include "lithokraft/game/wafer_biomes.h"  // WaferBiome, kWaferBiomeNames
#include "lithokraft/game/map_render.h"  // renderOverviewMap, waferMapPalette

namespace {

constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
constexpr uint64_t kSeed = 20261011;  // fixed: reproducible biome screenshots
constexpr int kRegionChunks = 3;      // 3x3 chunk region per biome (48x48 blocks)
constexpr int kImgW = 1280, kImgH = 720;

bool writeFile(const std::string& path, const std::vector<uint8_t>& data) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  const size_t n = std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  return n == data.size();
}

std::string readFile(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return "";
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  std::string s(size_t(n), '\0');
  const size_t r = std::fread(s.data(), 1, size_t(n), f);
  std::fclose(f);
  s.resize(r);
  return s;
}

// The tree features (trace_tree, capacitor_cans, resonator_spire) are wired
// into wafer.json itself (2026-10-11), so the pack file is used exactly as-is.
// (An earlier version injected duplicate feature entries in-memory; that made
// the generator invalid -- "duplicate feature name 'trace_trees'" -- and the
// game silently fell back to the built-in copy of the same file.)
// Source-root resolution (2026-10-15 Windows port; precedence fixed
// 2026-10-18): LITHO_SOURCE_ROOT wins when set; otherwise walk up from the
// exe's folder (the tree this tool was built in), then from the cwd, until
// game/LithoKraft/assets/worldgen/wafer.json is found. The original port
// tried a hardcoded production-tree path FIRST, which silently captured that
// tree's content whenever it existed on the machine; it is removed entirely,
// not demoted -- a fallback that substitutes another tree's assets produces
// plausible-looking wrong pictures, the worst failure mode for a capture tool.
std::filesystem::path g_exeDir;
std::string sourceRoot() {
  namespace fs = std::filesystem;
  static std::string cached;
  if (!cached.empty()) return cached;
  auto ok = [](const fs::path& r) {
    std::error_code ec;
    return fs::exists(r / "game" / "LithoKraft" / "assets" / "worldgen" / "wafer.json", ec);
  };
  if (const char* env = std::getenv("LITHO_SOURCE_ROOT"))
    if (ok(env)) return cached = env;
  std::error_code ec;
  for (fs::path start : {g_exeDir, fs::current_path(ec)}) {
    for (fs::path p = start; !p.empty(); p = p.parent_path()) {
      if (ok(p)) return cached = p.string();
      if (p == p.parent_path()) break;
    }
  }
  return cached = ".";
}

std::string waferJson() {
  return readFile(sourceRoot() + "/game/LithoKraft/assets/worldgen/wafer.json");
}

litholattice::Mat4 translation(float x, float y, float z) {
  litholattice::Mat4 t = litholattice::Mat4::identity();
  t.m[12] = x;
  t.m[13] = y;
  t.m[14] = z;
  return t;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 0 && argv[0]) g_exeDir = std::filesystem::absolute(argv[0]).parent_path();
  const std::string outDir = argc > 1 ? argv[1] : "./biome-shots";

  // Real Wafer worldgen with fixed seed (pack wafer.json used as-is; see above).
  // Note: makeGenerator pins the wafer to seed 0, so kSeed no longer affects it.
  litholattice::WorldgenSettings settings;
  settings.seed = kSeed;
  const std::string waferJsonText = waferJson();
  if (!waferJsonText.empty()) settings.generators["wafer"] = waferJsonText;
  litholattice::cDimensionReg reg = lithokraft::createDimensions(settings);
  litholattice::Dimension* wafer = reg.find("wafer");
  if (!wafer || !wafer->generator) {
    std::fprintf(stderr, "biome_capture: no wafer generator\n");
    return 1;
  }
  const auto& gen = *wafer->generator;
  const litholattice::BlockRegistry& blocks = lithokraft::blockRegistry();

  // Load the real resources exactly like the game (engine base < game pack <
  // pack_64). Previously this tool built a default BlockMaterials (every face =
  // the missing texture, layer 0) and fed the renderer a single all-white tile,
  // so every screenshot came out white regardless of the actual textures.
  // The cResourceMgr (and the materials it builds) must outlive all meshing.
  const std::string engineAssets = sourceRoot() + "/engine/assets";
  const std::string gameAssets = sourceRoot() + "/game/LithoKraft/assets";
  lithokraft::cResourceMgr rm({engineAssets, gameAssets,
                                  std::filesystem::path(gameAssets) / "packs" / "pack_64"});
  rm.reload();
  const auto materialsPtr = rm.materials();
  litholattice::MeshContext mctx{blocks, *materialsPtr, nullptr};

  // Build the software renderer's texture buffer: the fragment shader samples
  // a fixed 16x16 tile per layer (texData[layer*256 + ty*16 + tx], uint32 texels
  // packed 0xAARRGGBB -- R at >>16, matching basicFragmentShader's extraction and
  // this file's framebuffer unpacking below). Box-downsample each registry
  // texture (64x64 RGBA8 in pack_64; Image pixels are R,G,B,A bytes) to 16x16.
  const litholattice::TextureRegistry& texRegistry = rm.textures();
  const uint32_t layerCount = uint32_t(texRegistry.textures().size());
  std::vector<uint32_t> texBuffer(size_t(layerCount) * 256, 0xFFFF00FFu);
  for (uint32_t layer = 0; layer < layerCount; ++layer) {
    const litholattice::Image& img =
        texRegistry.texture(litholattice::TextureId{uint16_t(layer)}).image;
    if (img.empty() || img.width() <= 0 || img.height() <= 0) continue;
    for (int y = 0; y < 16; ++y) {
      const int y0 = y * img.height() / 16, y1 = std::max(y0 + 1, (y + 1) * img.height() / 16);
      for (int x = 0; x < 16; ++x) {
        const int x0 = x * img.width() / 16, x1 = std::max(x0 + 1, (x + 1) * img.width() / 16);
        uint32_t sum[4] = {0, 0, 0, 0};
        int n = 0;
        for (int sy = y0; sy < y1; ++sy)
          for (int sx = x0; sx < x1; ++sx, ++n) {
            const uint8_t* p = img.pixel(sx, sy);
            for (int k = 0; k < 4; ++k) sum[k] += p[k];
          }
        const uint32_t r = n ? sum[0] / uint32_t(n) : 0, g = n ? sum[1] / uint32_t(n) : 0,
                       b = n ? sum[2] / uint32_t(n) : 0, a = n ? sum[3] / uint32_t(n) : 0xFF;
        texBuffer[size_t(layer) * 256 + size_t(y) * 16 + size_t(x)] =
            (a << 24) | (r << 16) | (g << 8) | b;
      }
    }
  }
  std::printf("biome_capture: %u texture layers loaded (%s)\n", layerCount,
              rm.stackDescription().c_str());

  // Cortana 2026-10-15 1755 [TOOLS]: `biome_capture <outdir> --litho` renders only the
  // lithography test scene (litho_blocks.png) and exits; the default run is unchanged.
  // Row 1 (beam, along +X): Emitor (front east) -> 3 Laysors -> Etchor (laser port west).
  // Row 2: Selector, Connector, Input Buffer, Fabor, AutoFabor, Vior, blue/red Via.
  // Opaque quads are drawn first, then the blended Laysor quads (the renderFrame order).
  if (argc > 2 && std::string(argv[2]) == "--litho") {
    namespace lk = lithokraft;
    litholattice::ChunkMap map;
    map.setRegistry(&blocks);
    for (int dx = -1; dx <= 1; ++dx)
      for (int dz = -1; dz <= 1; ++dz)
        if (auto col = gen.generate(dx, dz)) map.insert(std::move(col));
    int surfY = 64;
    for (int y = litholattice::kWorldHeight - 12; y > 0; --y)
      if (map.get(0, y, 0) != litholattice::kAir) { surfY = y; break; }
    const int fy = surfY + 1, by = surfY + 2;
    for (int x = -8; x <= 8; ++x)
      for (int z = -6; z <= 8; ++z) {
        for (int y = fy; y <= fy + 10; ++y) map.set(x, y, z, litholattice::kAir);
        map.set(x, fy, z, lk::kInsulator);  // flat floor
      }
    auto put = [&](int x, int y, int z, litholattice::BlockId b, int facing) {
      map.set(x, y, z, b);
      map.setState(x, y, z, litholattice::CellState(facing << 7));
    };
    put(2, by, 0, lk::kEmitor, 1);  // front faces east (+X), into the Laysors
    for (int x = 3; x <= 5; ++x) put(x, by, 0, lk::kLaysor, 1);  // inside one sub-chunk: the beam trace is pad-bounded
    put(6, by, 0, lk::kEtchor, 0);  // facing north: its left (west) laser port meets the beam
    put(9, by, 0, lk::kLaysor, 1);  // unlit Laysor for comparison
    const litholattice::BlockId row2[] = {lk::kSelector, lk::kConnector, lk::kInputBuffer, lk::kFabor,
                                          lk::kAutoFabor, lk::kVior, lk::kBlueVia,
                                          lk::kRedVia};
    for (int i = 0; i < 8; ++i) put(-4 + i, by, 3, row2[i], 2);  // fronts face south (+Z), toward the camera
    litholattice::MeshContext lctx = mctx;
    lctx.appearance = &lk::blockAppearance();  // the game looks (blend, emissive, beam), as in-game
    struct LM { std::vector<litholattice::PackedVertex> op, bl; float ox, oy, oz; };
    std::vector<LM> lm;
    for (int dx = -1; dx <= 1; ++dx)
      for (int dz = -1; dz <= 1; ++dz)
        for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
          auto in = litholattice::snapshotForMeshing(map, blocks, dx, sy, dz, 0, 0);
          if (!in) continue;
          litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, lctx);
          if (m.verts.empty()) continue;
          {  // Cortana 2026-10-16: close-up capture = near LOD (foliage detail, no cross-hatch)
            std::vector<litholattice::PackedVertex> nearVerts;
            litholattice::selectLod(m.verts, /*far=*/false, nearVerts);
            m.verts.swap(nearVerts);
          }
          LM e{{}, {}, float(dx * 16), float(sy * 16), float(dz * 16)};
          for (size_t q = 0; q + 3 < m.verts.size(); q += 4)
            (m.verts[q].mode == litholattice::kShadeAlphaBlend ? e.bl : e.op)
                .insert((m.verts[q].mode == litholattice::kShadeAlphaBlend ? e.bl : e.op).end(),
                        m.verts.begin() + q, m.verts.begin() + q + 4);
          lm.push_back(std::move(e));
        }
    litholattice::cSoftwareRenderer sw;
    sw.setPerPixelShading(true);
    sw.setTextures(texBuffer.data(), layerCount);
    sw.resize(kImgW, kImgH);
    sw.clear(0xFF94B3DB);
    const litholattice::Vec3 eye{7.5f, float(by) + 5.5f, 11.5f};
    const litholattice::Vec3 target{2.5f, float(by) + 0.5f, 1.5f};
    litholattice::Vec3 fwd{target.x - eye.x, target.y - eye.y, target.z - eye.z};
    const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    fwd.x /= len; fwd.y /= len; fwd.z /= len;
    const litholattice::Mat4 vp = litholattice::mul(
        litholattice::perspective(float(60.0 * kDegToRad), float(kImgW) / float(kImgH), 0.1f, 2000.f),
        litholattice::lookDir(eye, fwd));
    size_t nBlend = 0;
    for (int pass = 0; pass < 2; ++pass)
      for (size_t i = 0; i < lm.size(); ++i) {
        const LM& m = pass == 0 ? lm[i] : lm[lm.size() - 1 - i];
        const auto& v = pass == 0 ? m.op : m.bl;
        if (v.empty()) continue;
        if (pass == 1) nBlend += v.size() / 4;
        const litholattice::Mat4 mvp = litholattice::mul(vp, translation(m.ox, m.oy, m.oz));
        sw.renderMesh(v.data(), v.size(), mvp.m);
      }
    const auto& fb = sw.framebuffer();
    std::vector<uint8_t> rgb(size_t(kImgW) * size_t(kImgH) * 3);
    for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
      const uint32_t p = fb.pixels[i];
      rgb[3 * i] = uint8_t((p >> 16) & 0xFF);
      rgb[3 * i + 1] = uint8_t((p >> 8) & 0xFF);
      rgb[3 * i + 2] = uint8_t(p & 0xFF);
    }
    std::vector<uint8_t> png;
    const std::string path = outDir + "/litho_blocks.png";
    const bool ok = litholattice::mcp::encodePngRgb(kImgW, kImgH, rgb.data(), png) && writeFile(path, png);
    std::printf("[%s] litho scene -> %s (surfaceY=%d, blended quads=%zu)\n", ok ? "OK" : "FAIL", path.c_str(),
                surfY, nBlend);
    return ok ? 0 : 1;
  }

  // Cortana 2026-10-15 2010 [TOOLS]: `biome_capture <outdir> --foliage` (1) saves the five
  // foliage models as .lthm assets into game/LithoKraft/assets/models/foliage/, (2) renders
  // one 128x128 icon per kind (magenta background; tools/foliage_icons.ps1 keys it out and
  // writes the pack icons) and (3) renders foliage_3d.png: every kind planted on real
  // generated terrain through the real mesher + game appearance.
  // Cortana 2026-10-15 2100 (review M3): --foliage renders only; --foliage-write-assets also
  // (re)writes the .lthm assets into the source tree.
  if (argc > 2 && (std::string(argv[2]) == "--foliage" || std::string(argv[2]) == "--foliage-write-assets")) {
    const bool writeAssets = std::string(argv[2]) == "--foliage-write-assets";
    namespace lk = lithokraft;
    namespace fo = lithokraft::foliage;
    const std::string modelDir = gameAssets + "/models/foliage";
    if (writeAssets) std::filesystem::create_directories(modelDir);
    int bad = 0;
    for (int k = 0; k < fo::kKindCount; ++k) {
      const auto models = fo::buildModel(fo::Kind(k));
      for (size_t i = 0; i < models.size(); ++i) {
        std::string err;
        const std::string p = fo::assetPath(modelDir, fo::Kind(k), int(i));
        if (writeAssets && !lithokraft::model::saveModel(p, models[i], &err)) { ++bad; std::printf("[FAIL] %s: %s\n", p.c_str(), err.c_str()); }
      }
    }
    for (int k = 0; k < fo::kKindCount; ++k) {  // stats after every asset is written (mesh() caches on first use)
      const auto models = fo::buildModel(fo::Kind(k));
      const fo::Mesh& fm = fo::mesh(fo::Kind(k));
      std::printf("[MODEL] %-14s blocks=%zu leaves=%zu octreeBytes=%zu quads=%zu (%zu B shared) asset=%d\n",
                  fo::kindName(fo::Kind(k)), models.size(), fm.leafCells, fm.octreeBytes, fm.quads.size(),
                  fm.quads.size() * sizeof(fo::Quad), fm.fromAsset ? 1 : 0);
    }
    litholattice::MeshContext lctx = mctx;
    lctx.appearance = &lk::blockAppearance();
    auto render = [&](litholattice::ChunkMap& map, int w, int h, uint32_t clear, litholattice::Vec3 eye,
                      litholattice::Vec3 target, float farZ, size_t* quads) {
      litholattice::cSoftwareRenderer sw;
      sw.setPerPixelShading(true);
      sw.setTextures(texBuffer.data(), layerCount);
      sw.resize(w, h);
      sw.clear(clear);
      litholattice::Vec3 fwd{target.x - eye.x, target.y - eye.y, target.z - eye.z};
      const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
      fwd.x /= len; fwd.y /= len; fwd.z /= len;
      const litholattice::Mat4 vp = litholattice::mul(
          litholattice::perspective(float(50.0 * kDegToRad), float(w) / float(h), 0.05f, farZ),
          litholattice::lookDir(eye, fwd));
      for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
          for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
            auto in = litholattice::snapshotForMeshing(map, blocks, dx, sy, dz, 0, 0);
            if (!in) continue;
            litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, lctx);
            if (m.verts.empty()) continue;
            if (quads) *quads += m.verts.size() / 4;
            const litholattice::Mat4 mvp = litholattice::mul(vp, translation(float(dx * 16), float(sy * 16), float(dz * 16)));
            sw.renderMesh(m.verts.data(), m.verts.size(), mvp.m);
          }
      const auto& fb = sw.framebuffer();
      std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
      for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
        const uint32_t p = fb.pixels[i];
        rgb[3 * i] = uint8_t((p >> 16) & 0xFF); rgb[3 * i + 1] = uint8_t((p >> 8) & 0xFF); rgb[3 * i + 2] = uint8_t(p & 0xFF);
      }
      return rgb;
    };
    auto save = [&](const std::string& path, int w, int h, const std::vector<uint8_t>& rgb) {
      std::vector<uint8_t> png;
      const bool ok = litholattice::mcp::encodePngRgb(w, h, rgb.data(), png) && writeFile(path, png);
      std::printf("[%s] %s\n", ok ? "OK" : "FAIL", path.c_str());
      if (!ok) ++bad;
    };
    const litholattice::BlockId kinds[fo::kKindCount] = {lk::kFoliageCattail, lk::kFoliageBush, lk::kFoliageFern,
                                                         lk::kFoliageReeds, lk::kFoliageCrystalShrub, lk::kFoliageMoss};
    // Icons: one instance alone high in the sky; the near far-plane clips the terrain.
    for (int k = 0; k < fo::kKindCount; ++k) {
      litholattice::ChunkMap map;
      map.setRegistry(&blocks);
      for (int dx = -1; dx <= 1; ++dx)
        for (int dz = -1; dz <= 1; ++dz)
          if (auto col = gen.generate(dx, dz)) map.insert(std::move(col));
      const int y = litholattice::kWorldHeight - 40;
      map.set(8, y, 8, kinds[k]);
      const float hgt = float(fo::kindHeight(fo::Kind(k)));
      const litholattice::Vec3 c{8.5f, float(y) + 0.5f * hgt, 8.5f};
      const float d = 1.55f * hgt + 0.6f;
      const auto rgb = render(map, 128, 128, 0xFFFF00FFu, {c.x + d * 0.75f, c.y + d * 0.45f, c.z + d}, c, 8.f, nullptr);
      save(outDir + "/icon_foliage_" + fo::kindName(fo::Kind(k)) + ".png", 128, 128, rgb);
    }
    // Scene: a planted patch on real terrain, one band per kind.
    litholattice::ChunkMap map;
    map.setRegistry(&blocks);
    for (int dx = -1; dx <= 1; ++dx)
      for (int dz = -1; dz <= 1; ++dz)
        if (auto col = gen.generate(dx, dz)) map.insert(std::move(col));
    auto surface = [&](int x, int z) {
      for (int y = litholattice::kWorldHeight - 12; y > 0; --y) {
        const litholattice::BlockId b = map.get(x, y, z);
        if (b != litholattice::kAir && !blocks.has(b, litholattice::kTransparent)) return y;
      }
      return 64;
    };
    int planted = 0;
    for (int z = -2; z <= 10; ++z)
      for (int x = -12; x <= 12; ++x) {
        const int band = std::clamp((x + 12) / 4, 0, fo::kKindCount - 1);
        if ((fo::variantAt(x * 7 + 1, 3, z * 5 + 2) + fo::variantAt(z, x, 9)) % 3 != 0) continue;
        const int s = surface(x, z);
        for (int yy = s + 1; yy <= s + 3; ++yy) map.set(x, yy, z, litholattice::kAir);
        map.set(x, s + 1, z, kinds[band]);
        ++planted;
      }
    const int s0 = surface(0, 4);
    size_t quads = 0;
    const auto rgb = render(map, kImgW, kImgH, 0xFF94B3DB, {0.5f, float(s0) + 7.f, 19.f}, {0.5f, float(s0) + 1.f, 3.f}, 2000.f, &quads);
    save(outDir + "/foliage_3d.png", kImgW, kImgH, rgb);
    std::printf("foliage scene: %d instances planted, %zu quads meshed in the 3x3 region\n", planted, quads);
    return bad ? 1 : 0;
  }

  int failures = 0;
  // Search origins spread out so the 5 biomes land in different areas.
  const double kOrigins[lithokraft::kWaferBiomeCount][2] = {
      {0.0, 0.0},      // trace_forest
      {2000.0, 0.0},   // dicing_plains
      {0.0, 2000.0},   // capacitor_groves
      {-2000.0, 0.0},  // resonator_spires
      {0.0, -2000.0},  // dead_zones
  };
  for (int b = 0; b < lithokraft::kWaferBiomeCount; ++b) {
    const std::string bname(lithokraft::kWaferBiomeNames[b]);
    // Find the biome.
    const auto loc = lithokraft::locateBiome(gen, uint8_t(b), kOrigins[b][0], kOrigins[b][1]);
    if (!loc.found) {
      std::printf("[SKIP] %s: not found within search range\n", bname.c_str());
      continue;
    }
    std::printf("[FOUND] %s at (%d, %d), %.0f blocks from origin\n", bname.c_str(), loc.x, loc.z,
                loc.distance);

    // Generate a region of chunks around it.
    litholattice::ChunkMap map;
    map.setRegistry(&blocks);
    const int ccx = loc.x >> 4, ccz = loc.z >> 4;
    const int half = kRegionChunks / 2;
    for (int dx = -half; dx <= half; ++dx)
      for (int dz = -half; dz <= half; ++dz) {
        auto col = gen.generate(ccx + dx, ccz + dz);
        if (col) map.insert(std::move(col));
      }

    // For dead zones: strip cans (screenshot purity; the region borders a grove).
    if (b == int(lithokraft::WaferBiome::DeadZones)) {
      for (int dx = -half; dx <= half; ++dx)
        for (int dz = -half; dz <= half; ++dz)
          for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 16; ++x)
              for (int z = 0; z < 16; ++z) {
                const int wx = (ccx + dx) * 16 + x, wz = (ccz + dz) * 16 + z;
                if (map.get(wx, y, wz) == 42) {
                  // Set to air via direct sub-chunk access.
                  auto* col = map.column(ccx + dx, ccz + dz);
                  if (col && col->subs[y >> 4])
                    col->subs[y >> 4]->set(x, y & 15, z, litholattice::kAirStableId);  // StableId currency, not BlockId 0
                }
              }
    }

    // Count feature blocks for diagnostics.
    int trunkCount = 0, branchCount = 0, crystalCount = 0, canCount = 0;
    for (int dx = -half; dx <= half; ++dx)
      for (int dz = -half; dz <= half; ++dz)
        for (int y = 0; y < 256; ++y)
          for (int x = 0; x < 16; ++x)
            for (int z = 0; z < 16; ++z) {
              const auto id = map.get((ccx + dx) * 16 + x, y, (ccz + dz) * 16 + z);
              if (id == 38) ++trunkCount;
              else if (id == 39) ++branchCount;
              else if (id == 42) ++canCount;
              else if (id == 43) ++crystalCount;
            }
    std::printf("  features: trunks=%d branches=%d cans=%d crystals=%d\n", trunkCount, branchCount,
                canCount, crystalCount);

    // Surface height at the biome center (for camera targeting).
    int surfY = 64;
    for (int y = 255; y > 0; --y) {
      if (map.get(loc.x, y, loc.z) != litholattice::kAir) {
        surfY = y;
        break;
      }
    }

    // Mesh every sub-chunk in the region.
    struct Mesh {
      std::vector<litholattice::PackedVertex> verts;
      float ox, oy, oz;
    };
    std::vector<Mesh> meshes;
    for (int dx = -half; dx <= half; ++dx)
      for (int dz = -half; dz <= half; ++dz)
        for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
          auto in = litholattice::snapshotForMeshing(map, blocks, ccx + dx, sy, ccz + dz, 0, 0);
          if (!in) continue;
          litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, mctx);
          if (m.verts.empty()) continue;
          Mesh mesh;
          mesh.verts = std::move(m.verts);
          mesh.ox = float((ccx + dx) * 16);
          mesh.oy = float(sy * 16);
          mesh.oz = float((ccz + dz) * 16);
          meshes.push_back(std::move(mesh));
        }
    std::printf("  meshed %zu sub-chunks\n", meshes.size());
    if (meshes.empty()) {
      std::printf("[FAIL] %s: no meshes\n", bname.c_str());
      ++failures;
      continue;
    }

    // cCamera: elevated, looking down at the biome center at an angle.
    litholattice::cSoftwareRenderer sw;
    sw.setPerPixelShading(true);  // diffuse-lit with the real pack textures
    sw.setTextures(texBuffer.data(), layerCount);
    sw.resize(kImgW, kImgH);
    sw.clear(0xFF94B3DB);  // wafer day sky
    const float cx = float(loc.x), cy = float(surfY), cz = float(loc.z);
    const litholattice::Vec3 eye{cx + 38.f, cy + 30.f, cz + 38.f};
    const litholattice::Vec3 target{cx, cy, cz};
    litholattice::Vec3 fwd{target.x - eye.x, target.y - eye.y, target.z - eye.z};
    const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    fwd.x /= len;
    fwd.y /= len;
    fwd.z /= len;
    const litholattice::Mat4 vp = litholattice::mul(
        litholattice::perspective(float(70.0 * kDegToRad), float(kImgW) / float(kImgH), 0.1f, 2000.f),
        litholattice::lookDir(eye, fwd));
    for (const auto& m : meshes) {
      const litholattice::Mat4 mvp = litholattice::mul(vp, translation(m.ox, m.oy, m.oz));
      sw.renderMesh(m.verts.data(), m.verts.size(), mvp.m);
    }

    // Save PNG.
    const auto& fb = sw.framebuffer();
    std::vector<uint8_t> rgb(size_t(kImgW) * size_t(kImgH) * 3);
    for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
      const uint32_t p = fb.pixels[i];
      rgb[3 * i] = uint8_t((p >> 16) & 0xFF);
      rgb[3 * i + 1] = uint8_t((p >> 8) & 0xFF);
      rgb[3 * i + 2] = uint8_t(p & 0xFF);
    }
    std::vector<uint8_t> png;
    if (!litholattice::mcp::encodePngRgb(kImgW, kImgH, rgb.data(), png)) {
      std::printf("[FAIL] %s: PNG encode failed\n", bname.c_str());
      ++failures;
      continue;
    }
    const std::string path = outDir + "/" + bname + ".png";
    if (!writeFile(path, png)) {
      std::printf("[FAIL] %s: write failed\n", bname.c_str());
      ++failures;
      continue;
    }
    std::printf("[OK] %s -> %s (%zu bytes)\n", bname.c_str(), path.c_str(), png.size());
  }

  std::printf("biome_capture: %d failures\n", failures);

  // Spawn capture: the 3x3 chunk region around world column (0,0), same camera
  // math as the biome shots, saved as spawn.png.
  {
    litholattice::ChunkMap map;
    map.setRegistry(&blocks);
    const int half = kRegionChunks / 2;
    for (int dx = -half; dx <= half; ++dx)
      for (int dz = -half; dz <= half; ++dz) {
        auto col = gen.generate(dx, dz);
        if (col) map.insert(std::move(col));
      }
    int surfY = 64;
    for (int y = 255; y > 0; --y) {
      if (map.get(0, y, 0) != litholattice::kAir) {
        surfY = y;
        break;
      }
    }
    struct Mesh {
      std::vector<litholattice::PackedVertex> verts;
      float ox, oy, oz;
    };
    std::vector<Mesh> meshes;
    for (int dx = -half; dx <= half; ++dx)
      for (int dz = -half; dz <= half; ++dz)
        for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
          auto in = litholattice::snapshotForMeshing(map, blocks, dx, sy, dz, 0, 0);
          if (!in) continue;
          litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, mctx);
          if (m.verts.empty()) continue;
          meshes.push_back(Mesh{std::move(m.verts), float(dx * 16), float(sy * 16), float(dz * 16)});
        }
    std::printf("  spawn: surface Y=%d at (0,0), meshed %zu sub-chunks\n", surfY, meshes.size());
    if (meshes.empty()) {
      std::printf("[FAIL] spawn: no meshes\n");
      ++failures;
    } else {
      litholattice::cSoftwareRenderer sw;
      sw.setPerPixelShading(true);
      sw.setTextures(texBuffer.data(), layerCount);
      sw.resize(kImgW, kImgH);
      sw.clear(0xFF94B3DB);  // wafer day sky
      const float cx = 0.f, cy = float(surfY), cz = 0.f;
      const litholattice::Vec3 eye{cx + 38.f, cy + 30.f, cz + 38.f};
      const litholattice::Vec3 target{cx, cy, cz};
      litholattice::Vec3 fwd{target.x - eye.x, target.y - eye.y, target.z - eye.z};
      const float len = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
      fwd.x /= len;
      fwd.y /= len;
      fwd.z /= len;
      const litholattice::Mat4 vp = litholattice::mul(
          litholattice::perspective(float(70.0 * kDegToRad), float(kImgW) / float(kImgH), 0.1f, 2000.f),
          litholattice::lookDir(eye, fwd));
      for (const auto& m : meshes) {
        const litholattice::Mat4 mvp = litholattice::mul(vp, translation(m.ox, m.oy, m.oz));
        sw.renderMesh(m.verts.data(), m.verts.size(), mvp.m);
      }
      const auto& fb = sw.framebuffer();
      std::vector<uint8_t> rgb(size_t(kImgW) * size_t(kImgH) * 3);
      for (size_t i = 0, n = rgb.size() / 3; i < n; ++i) {
        const uint32_t p = fb.pixels[i];
        rgb[3 * i] = uint8_t((p >> 16) & 0xFF);
        rgb[3 * i + 1] = uint8_t((p >> 8) & 0xFF);
        rgb[3 * i + 2] = uint8_t(p & 0xFF);
      }
      std::vector<uint8_t> png;
      const std::string path = outDir + "/spawn.png";
      if (!litholattice::mcp::encodePngRgb(kImgW, kImgH, rgb.data(), png)) {
        std::printf("[FAIL] spawn: PNG encode failed\n");
        ++failures;
      } else if (!writeFile(path, png)) {
        std::printf("[FAIL] spawn: write failed\n");
        ++failures;
      } else {
        std::printf("[OK] spawn -> %s (%zu bytes)\n", path.c_str(), png.size());
      }
    }
  }

  // Top-down overview map (colored biome layout).
  {
    lithokraft::OverviewMapSpec spec;
    spec.centerX = 0;
    spec.centerZ = 0;
    spec.zoom = 3;  // 2048x2048 blocks
    spec.granularity = 8;
    lithokraft::OverviewMapSource src;
    src.biomeColors = lithokraft::waferMapPalette();
    const auto& pipeline = wafer->generator->pipeline();
    src.sampleBiomes = [&](const int32_t* xs, const int32_t* zs, size_t n, uint8_t* out) {
      // Use the pipeline's batched biome query if available, else loop.
      for (size_t i = 0; i < n; ++i) out[i] = pipeline.biomeAt(xs[i], zs[i]);
    };
    src.structures = [](int32_t, int32_t, int32_t, int32_t) { return std::vector<lithokraft::MapMarker>{}; };
    lithokraft::OverviewMap omap;
    std::string msg;
    if (lithokraft::renderOverviewMap(spec, src, omap, msg)) {
      std::vector<uint8_t> png;
      if (litholattice::mcp::encodePngRgb(omap.width, omap.height, omap.rgb.data(), png)) {
        const std::string path = outDir + "/wafer_overview_map.png";
        if (writeFile(path, png))
          std::printf("[OK] overview map -> %s (%dx%d, %zu bytes)\n", path.c_str(), omap.width,
                      omap.height, png.size());
        else {
          std::printf("[FAIL] overview map: write failed\n");
          ++failures;
        }
      } else {
        std::printf("[FAIL] overview map: PNG encode failed\n");
        ++failures;
      }
    } else {
      std::printf("[FAIL] overview map: %s\n", msg.c_str());
      ++failures;
    }
  }

  std::printf("biome_capture: %d total failures\n", failures);
  return failures ? 1 : 0;
}
