// Headless checks of LithoKraft content that sits on engine interfaces: sound-effect recipes on
// the engine synth, the block appearance through the engine mesher, the registry layout.
#include "lithokraft/content/block_families.h"
#include <cstdlib>
#include "litholattice/procgen/ramp_shape.h"  // Highway Ramp checks
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include "lithokraft/content/chunk_coord_text.h"
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/content_selftest.h"
#include "lithokraft/content/foliage.h"
#include "lithokraft/content/material_bindings.h"  // JD Vulkan push  // Cortana 2026-10-15 2010 [GAME]
#include "lithokraft/content/sfx.h"
#include "litholattice/render/mesher.h"

namespace lithokraft {
namespace {

double peak(const std::vector<float>& x, size_t from, size_t to) {
  double p = 0;
  for (size_t i = from; i < to && i < x.size(); ++i) p = std::max(p, double(std::fabs(x[i])));
  return p;
}

}  // namespace

int runContentSelfTest() {
  int fails = 0;
  auto check = [&](bool ok, const char* what) {
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++fails;
  };
  std::printf("Content self-test (LithoKraft on the engine SDK)\n");
  {
    bool ok = true;
    for (Sfx s : {Sfx::Place, Sfx::Break, Sfx::Click, Sfx::PulsorFire}) {
      cAudioEngine eng(48000.f);
      eng.post(sfxCommand(s, 1.f, 30));
      std::vector<float> x(24000);
      eng.render(x.data(), x.size(), 1);
      ok = ok && peak(x, 0, 2400) > 0.05 && peak(x, 14400, 24000) == 0.0;
    }
    check(ok, "SFX place / break / click / pulsar fire: audible at once, silent within 300 ms");
  }
  {
    const cBlockReg& reg = blockRegistry();
    // Registry is append-only: every named id must resolve inside it, and ids must be dense.
    const bool idsDense = kSilicaTilledSoil < reg.size() && reg.idOf(kSilicaTilledSoil) == "lithokraft:tilled_silicon";
    check(idsDense && reg.def(kAir).flags == kTransparent && reg.def(kStone).tex == "stone" &&
              roleOf(kRedTrace) == SimRole::Wire && reg.def(kRedTrace).shape == Shape::Custom &&
              reg.def(kRedTorch).shape == Shape::Cross && reg.has(kStone, kPowerable | kCutsWire),
          "block registry: game rows after engine Air, roles / shapes / game flags round-trip");
  }
  {
    // JD materials blocks (2026-10-14): appended after the formula
    // shrubs, dense and in order, string ids round-trip, registry ends
    // at the last appended block.
    const cBlockReg& reg = blockRegistry();
    const bool appended = kCopperBlock == kFormulaShrub314 + 1 &&
                          kAluminumBlock == kCopperBlock + 1 &&
                          kGravityBlock == kAluminumBlock + 1 &&
                          kSubstrate == kGravityBlock + 1 &&
                          kInsulatnor == kSubstrate + 1 &&
                          kInsulator == kInsulatnor + 1 &&
                          kInsulatxor == kInsulator + 1 &&
                          kSelector == kInsulatxor + 1 &&  // Cortana 2026-10-15 1755: lithography blocks follow
                          reg.idOf(kCopperBlock) == "lithokraft:copper_block" &&
                          reg.idOf(kAluminumBlock) == "lithokraft:aluminum_block" &&
                          reg.idOf(kGravityBlock) == "lithokraft:gravity_block" &&
                          reg.idOf(kSubstrate) == "lithokraft:substrate" &&
                          reg.idOf(kInsulatnor) == "lithokraft:insulatnor" &&
                          reg.idOf(kInsulator) == "lithokraft:insulator" &&
                          reg.idOf(kInsulatxor) == "lithokraft:insulatxor";
    check(appended, "materials blocks: 7 JD blocks appended dense after the shrubs (ids 67-73), string ids round-trip");
    {  // Cortana 2026-10-15 2010 [GAME]: 3D foliage instances, append-only ids 82-86.
      namespace fo = lithokraft::foliage;
      const BlockId fol[6] = {kFoliageCattail, kFoliageBush, kFoliageFern, kFoliageReeds, kFoliageCrystalShrub, kFoliageMoss};
      bool ok = fol[0] == 82 && reg.size() >= size_t(kFoliageMoss) + 1;  // 2026-10-16: crystal leaves append after (88-91)
      for (int i = 0; i < 6; ++i) {
        const BlockDef d = reg.def(fol[i]);
        ok = ok && fol[i] == BlockId(82 + i) && d.shape == Shape::Custom && !(d.flags & kSolid);
      }
      check(ok, "foliage blocks: 6 appended dense (ids 82-87), Shape::Custom, non-solid");
      {  // 2026-10-16 (JD leaf pass): crystal leaves append dense after the foliage (ids 88-91).
        const char* lv[4] = {"lithokraft:crystal_leaves_a", "lithokraft:crystal_leaves_b", "lithokraft:crystal_leaves_reddom",
                             "lithokraft:crystal_leaves_bluedom"};
        bool lok = reg.size() >= size_t(kFoliageMoss) + 5;  // WaferBeta: Lithographor appended after (checked below)
        for (int i = 0; i < 4 && lok; ++i) {
          const auto id = reg.findById(lv[i]);
          const BlockDef d = id ? reg.def(*id) : BlockDef{};
          lok = id && *id == BlockId(kFoliageMoss + 1 + i) && d.shape == Shape::Cube && (d.flags & litholattice::kLeaves) &&
                !(d.flags & kOpaque);
        }
        check(lok, "crystal leaves: 4 appended dense (ids 88-91), Cube, kLeaves, not kOpaque");
      }
      {  // 2026-10-16 WaferBeta: Lithographor re-enabled at the END of the registry (id 92), Custom cuboids.
        const auto id = reg.findById("lithokraft:lithographor");
        const BlockDef d = id ? reg.def(*id) : BlockDef{};
        check(id && *id == BlockId(kFoliageMoss + 5) && reg.size() == size_t(kFoliageMoss) + 19 && kFusorSum == BlockId(kFoliageMoss + 18) && kRandor == BlockId(kFoliageMoss + 17) &&  /* 2026-10-31: + Randor */ kTimerBuffer == BlockId(kFoliageMoss + 12) && kVirtualPost == BlockId(kFoliageMoss + 13) && kOrganicWallPost == BlockId(kFoliageMoss + 16) && kRedViaX == BlockId(kFoliageMoss + 8) && kBlueViaZ == BlockId(kFoliageMoss + 11) &&  /* 2026-10-25: + 4 side vias, + Timer Buffer (roadmap 7), + 4 Posts */ d.shape == Shape::Custom &&
                  (d.flags & litholattice::kDirectional) && !(d.flags & kOpaque) && d.tex == "lithographor",
              "lithographor: id 92 (appended; Aluminum Ramp 93, Highway Ramp, 4 side Vias, Timer Buffer follow), Custom directional cuboid model, tex lithographor");
        // Cortana 2026-10-16: Aluminum Ramp appended after the Lithographor (id 93), Custom slope, not opaque.
        const auto rid = reg.findById("lithokraft:aluminum_ramp");
        const BlockDef rd = rid ? reg.def(*rid) : BlockDef{};
        check(rid && *rid == BlockId(kFoliageMoss + 6) && *rid == kAluminumRamp && rd.shape == Shape::Custom &&
                  (rd.flags & litholattice::kDirectional) && (rd.flags & kSolid) && !(rd.flags & kOpaque) && rd.tex == "aluminum_block",
              "aluminum ramp: appended last (id 93), Custom directional slope, aluminum_block texture");
        // Proposed by JD, Built by Cortana: Highway Ramp appended after the Aluminum Ramp (id 94).
        const auto hid = reg.findById("lithokraft:highway_ramp");
        const BlockDef hd = hid ? reg.def(*hid) : BlockDef{};
        check(hid && *hid == BlockId(kFoliageMoss + 7) && *hid == kHighwayRamp && hd.shape == Shape::Custom &&
                  (hd.flags & kSolid) && !(hd.flags & kOpaque) && hd.tex == "aluminum_block",
              "highway ramp: appended last (id 94), Custom 2-block slope, aluminum_block texture");
        {
          namespace hw = litholattice::procgen::ramp::highway;
          bool okH = true;
          for (int f = 0; f < 4; ++f)
            for (int h = 0; h < 2; ++h) {
              const uint8_t s = hw::encode(f, h);
              okH = okH && hw::facingOf(s) == f && hw::halfOf(s) == h;
            }
          // East-facing pair: low half rises 0 -> 8/16, high half 8 -> 16 (continuous at the seam).
          okH = okH && hw::height16(hw::encode(1, 0), 0, 8) == 0 && hw::height16(hw::encode(1, 0), 16, 8) == 8 &&
                hw::height16(hw::encode(1, 1), 0, 8) == 8 && hw::height16(hw::encode(1, 1), 16, 8) == 16 &&
                hw::height16(hw::encode(0, 0), 8, 16) == 0 && hw::height16(hw::encode(0, 1), 8, 0) == 16;
          check(okH, "highway ramp: state packing, 4 facings, continuous 1:2 surface across both halves");
        }
        const auto vp = reg.findById("lithokraft:chipor_viewport");
        check(vp && reg.def(*vp).tex == "chipor_viewport", "chipor viewport: own texture name chipor_viewport");
      }
      bool det = true, bounds = true, shared = true, rot = true;
      for (int k = 0; k < fo::kKindCount; ++k) {
        const auto a = fo::buildModel(fo::Kind(k)), b = fo::buildModel(fo::Kind(k));
        det = det && a.size() == size_t(fo::kindHeight(fo::Kind(k))) && a.size() == b.size();
        for (size_t i = 0; det && i < a.size(); ++i)
          det = !a[i].geometry.empty() && lithokraft::model::modelEquals(a[i], b[i]);
        const fo::Mesh& m = fo::mesh(fo::Kind(k));
        shared = shared && &m == &fo::mesh(fo::Kind(k)) && !m.quads.empty();
        const int top = 16 * fo::kindHeight(fo::Kind(k));
        for (const fo::Quad& q : m.quads) {
          for (int ax = 0; ax < 3; ++ax)
            bounds = bounds && q.lo[ax] >= 0 && q.hi[ax] > q.lo[ax] && q.hi[ax] <= (ax == 1 ? top : 16);
          const fo::Quad r4 = fo::rotated(q, 4), r1 = fo::rotated(q, 1);
          rot = rot && std::memcmp(&r4, &q, sizeof q) == 0 && r1.lo[0] >= 0 && r1.hi[0] <= 16 && r1.lo[1] == q.lo[1];
        }
      }
      {  // 2026-10-24 (JD): placement faces away from the player, one check per yaw quadrant
        const double kPi = 3.14159265358979323846;
        auto at = [&](double deg) { const double y = deg * kPi / 180.0; return fo::facingFromLook(std::cos(y), std::sin(y)); };
        check(at(-90) == 0 && at(-60) == 0 && at(-120) == 0, "placement facing: looking north (-Z) -> facing N");
        check(at(0) == 1 && at(30) == 1 && at(-30) == 1, "placement facing: looking east (+X) -> facing E");
        check(at(90) == 2 && at(60) == 2 && at(120) == 2, "placement facing: looking south (+Z) -> facing S");
        check(at(180) == 3 && at(150) == 3 && at(-150) == 3, "placement facing: looking west (-X) -> facing W");
      }
      check(det, "foliage models: deterministic leaf hashes, block stacks match kind heights");
      check(shared && bounds, "foliage meshes: one shared quad list per kind, inside the kind's cell column");
      // Cortana 2026-10-15 2310: the coplanar merge reduced (or kept) every kind's quad count.
      size_t raw = 0, merged = 0;
      bool fewer = true;
      for (int k = 0; k < fo::kKindCount; ++k) {
        const fo::Mesh& m = fo::mesh(fo::Kind(k));
        raw += m.rawQuads; merged += m.quads.size();
        fewer = fewer && m.quads.size() <= m.rawQuads && !m.quads.empty();
      }
      std::printf("  foliage quads: %zu raw -> %zu merged (%.0f%% fewer)\n", raw, merged,
                  raw ? 100.0 * double(raw - merged) / double(raw) : 0.0);
      check(fewer, "foliage meshes: coplanar merge never adds quads");
      check(rot, "foliage variants: quarter turns stay in the cell and 4 turns are the identity");
#ifdef LITHOKRAFT_MODELS_DIR
      // Cortana 2026-10-15 2100 (review M2): shipped .lthm assets must equal the generator.
      bool assetsOk = true;
      for (int k = 0; k < fo::kKindCount; ++k) {
        const auto gen = fo::buildModel(fo::Kind(k));
        for (size_t i = 0; i < gen.size(); ++i) {
          auto r = lithokraft::model::loadModel(fo::assetPath(std::string(LITHOKRAFT_MODELS_DIR) + "/foliage", fo::Kind(k), int(i)));
          assetsOk = assetsOk && r.error == lithokraft::model::LoadError::Ok && lithokraft::model::modelEquals(r.model, gen[i]);
        }
      }
      check(assetsOk, "foliage assets: assets/models/foliage/*.lthm match the generator (re-export with biome_capture --foliage-write-assets)");
#endif
    }
    {  // Cortana 2026-10-15 1755: lithography machine blocks [GAME], append-only ids 74-81.
      const BlockId litho[8] = {kSelector, kConnector, kLaysor, kEmitor, kInputBuffer, kFabor, kAutoFabor, kVior};
      const char* ids[8] = {"lithokraft:selector", "lithokraft:connector", "lithokraft:laysor", "lithokraft:emitor",
                            "lithokraft:input_buffer", "lithokraft:fabor", "lithokraft:autofabor", "lithokraft:vior"};
      bool dense = litho[0] == 74 && reg.size() >= size_t(kVior) + 1;  // Cortana 2026-10-15 2010: foliage appended after
      for (int i = 0; i < 8; ++i) {
        dense = dense && litho[i] == BlockId(74 + i) && reg.idOf(litho[i]) == ids[i];
        const BlockDef& d = reg.def(litho[i]);
        dense = dense && d.shape == Shape::Cube && d.behavior.empty() &&
                roleOf(d) == (litho[i] == kEmitor ? SimRole::Lamp : SimRole::None);  // 1900: Emitor is a Lamp
      }
      const BlockDef& lay = reg.def(kLaysor);
      const bool glass = (lay.flags & kSolid) && (lay.flags & kTransparent) && !(lay.flags & kOpaque);
      check(dense && glass, "lithography blocks: 8 appended dense at ids 74-81 (namespaced; Emitor = Lamp, others no sim role), Laysor see-through");
      const IBlockAppearance& ap = blockAppearance();
      // Cortana 2026-10-15 1900: the Emitor aperture lights only while powered (Lamp role).
      const BlockDef& em = reg.def(kEmitor);
      check(ap.faceLook(lay, 0, FaceSlot::Left, false).mode == kShadeAlphaBlend &&
                ap.faceLook(em, 1, FaceSlot::Front, true).mode == kShadeEmissive &&
                ap.faceLook(em, 0, FaceSlot::Front, false).mode == kShadeTex &&
                ap.faceLook(em, 1, FaceSlot::Back, true).mode == kShadeTex && roleOf(em) == SimRole::Lamp &&
                ap.active(em, CellState(1 | (1 << 7))) && !ap.active(em, CellState(1 << 7)),
            "lithography blocks: Laysor blended; the Emitor is a Lamp whose front aperture lights only when powered");
    }
    // Substrate (JD): the Wafer's bedrock layer -- semi-transparent
    // (transparent, NOT opaque) and breakable (solid, density far under
    // the 255 unbreakable class).
    const BlockDef& sub = reg.def(kSubstrate);
    check((sub.flags & kSolid) && (sub.flags & kTransparent) && !(sub.flags & kOpaque) &&
              sub.shape == Shape::Cube && sub.density < 255,
          "substrate: semi-transparent (kTransparent, not kOpaque), solid and breakable");
    // Insulators (JD): the ceramic pattern -- solid, opaque, cuts wire,
    // never powerable (a powered insulator would not be insulating).
    bool ins = true;
    for (BlockId b : {kInsulatnor, kInsulator, kInsulatxor}) {
      const BlockDef& d = reg.def(b);
      ins = ins && (d.flags & kSolid) && (d.flags & kOpaque) && (d.flags & kCutsWire) &&
            !(d.flags & kPowerable) && d.shape == Shape::Cube && roleOf(d) == SimRole::None;
    }
    check(ins, "insulators: Insulatnor / Insulator / Insulatxor solid + opaque + cuts wire, never powerable");
    // Gravity Block: JD-named, function open -- a neutral full cube with
    // no behavior and no sim role (no mechanics invented).
    const BlockDef& grav = reg.def(kGravityBlock);
    check((grav.flags & kSolid) && grav.shape == Shape::Cube && grav.behavior.empty() &&
              roleOf(grav) == SimRole::None,
          "gravity block: neutral solid placeholder, no behavior, no sim role (function JD-open)");
  }
  {
    // F-M4: registerBlock validates the namespace format — malformed ids are
    // refused (kAir, nothing registered); a well-formed namespaced id works.
    auto mkDef = [](std::string_view id) {
      return BlockDef{"Widget", kSolid, Shape::Cube, "stone", "", 0, 0, 0, 0.8f, false, 0, id};
    };
    const size_t before = blockRegistry().size();
    const bool refused = registerBlock(mkDef("")) == kAir &&
                         registerBlock(mkDef("no_colon")) == kAir &&
                         registerBlock(mkDef("Bad NS:widget")) == kAir &&
                         registerBlock(mkDef("testmod:")) == kAir &&
                         blockRegistry().size() == before &&
                         !blockRegistry().findById("testmod:widget");
    // Positive case WITHOUT polluting the shared registry: re-registering a
    // well-formed existing id passes validation and hits first-wins (returns
    // the existing id, appends nothing). A foreign-namespace APPEND cannot
    // be exercised in this process — the save selftest pins that every id in
    // the game registry is a unique 'lithokraft:' name.
    const BlockId good = registerBlock(mkDef("lithokraft:stone"));
    check(refused && good == kStone && blockRegistry().size() == before,
          "registerBlock: malformed namespace ids refused, well-formed id registers (F-M4)");
  }
  {
    // A red trace between two red traces on stone: the appearance meshes one straight floor strip.
    MeshInput in{};
    for (auto& id : in.id) id = kAir;
    auto at = [&](int x, int y, int z) -> BlockId& { return in.id[meshPadIndex(x + 1, y + 1, z + 1)]; };
    for (int x = 4; x <= 6; ++x) { at(x, 0, 5) = kStone; at(x, 1, 5) = kRedTrace; }
    in.st[meshPadIndex(6, 2, 6)] = makeState(40);
    const BlockMaterials mats(blockRegistry().size());
    const ChunkMesh plain = meshSubChunk(in, MeshContext{blockRegistry(), mats, nullptr});
    const ChunkMesh game = meshSubChunk(in, MeshContext{blockRegistry(), mats, &blockAppearance()});
    const size_t stoneQuads = plain.verts.size() / 4;  // engine alone: custom wires are not drawn
    bool tinted = false;
    for (const PackedVertex& v : game.verts) tinted = tinted || (v.mode == kShadeStripTint0 && v.value == 40);
    check(game.verts.size() / 4 == stoneQuads + 3 && tinted,
          "appearance: wire strips mesh through IBlockAppearance (3 strips, red tint, strength 40)");
  }
  {
    // Wafer ground pulse wiring through the engine mesher: silicon faces carry
    // the kShadeGroundPulse tag, Trace Bulbs (40/41) carry kShadeCutout (dithered shell).
    MeshInput in{};
    for (auto& id : in.id) id = kAir;
    auto at = [&](int x, int y, int z) -> BlockId& { return in.id[meshPadIndex(x + 1, y + 1, z + 1)]; };
    at(4, 0, 4) = kWaferSilicon;
    at(8, 0, 8) = kTraceTipRed;
    at(10, 0, 8) = kTraceTipBlue;
    const BlockMaterials mats(blockRegistry().size());
    const ChunkMesh m = meshSubChunk(in, MeshContext{blockRegistry(), mats, &blockAppearance()});
    bool siliconPulse = false, tipGlow = false;
    for (const PackedVertex& v : m.verts) {
      if (v.mode == kShadeGroundPulse) siliconPulse = true;
      if (v.mode == kShadeAlphaBlend) tipGlow = true;  // JD Vulkan push 2026-10-16: Trace Bulbs = blended ghost shell
    }
    check(siliconPulse, "appearance: wafer silicon faces tag kShadeGroundPulse (ground pulse)");
    check(tipGlow, "appearance: Trace Bulbs (40/41) tag kShadeAlphaBlend (CloudAtlas ghost shell)");
  }
  // JD Vulkan push (Cortana 2026-10-16): CloudAtlas .lthm models load with their material bindings.
  {
    const BoundModel red = loadBoundModel(std::string(LITHOKRAFT_MODELS_DIR) + "/trace_bulb/red-trace-bulb-glass-core.lthm");
    const BoundModel blue = loadBoundModel(std::string(LITHOKRAFT_MODELS_DIR) + "/trace_bulb/blue-trace-bulb-glass-core.lthm");
    const bool redOk = red.load.error == model::LoadError::Ok && red.load.model.geometry.leafCount() == 3584 && red.hasBindings &&
                       red.bindings.find(40) && red.bindings.find(41) && red.bindings.find(5) &&
                       red.bindings.find(40)->shade == "alpha_blend" && red.bindings.find(41)->glow;
    const bool blueOk = blue.load.error == model::LoadError::Ok && blue.load.model.geometry.leafCount() == 3584 && blue.hasBindings &&
                        blue.bindings.find(42) && blue.bindings.find(43) && blue.bindings.find(6);
    check(redOk && blueOk, "material bindings: CloudAtlas bulb cores load (3584 leaves) with rim 40/42, core 41/43, Trace 5/6");
    bool etched = true;
    for (const char* m : {"glass-etched", "red-trace-cross-etched", "wafer-ground-trace-etched"}) {
      const BoundModel e = loadBoundModel(std::string(LITHOKRAFT_MODELS_DIR) + "/etched/" + m + ".lthm");
      etched = etched && e.load.error == model::LoadError::Ok && e.load.model.geometry.leafCount() > 0 && e.hasBindings &&
               e.bindings.find(50) && e.bindings.find(51);
    }
    check(etched, "material bindings: etched blocks load with base 50 / line 51");
    ModelBindings bad;
    check(!loadModelBindings(materialBindingsDir(), "../evil", bad) && !loadModelBindings(materialBindingsDir(), "no-such-model", bad),
          "material bindings: traversal and missing ids rejected");
  }
  {  // 2026-10-31 (JD): F3 chunk line: chunk + local coords, seam flags at local x / z 0 and 15 (negatives too)
    const std::string a = cChunkCoordText::block(17, 266, 96), b = cChunkCoordText::block(15, 266, 47), c = cChunkCoordText::block(-1, 5, -16);
    check(a == "c 1,6 l 1,10,0 [z0]" && b == "c 0,2 l 15,10,15 [x15 z15]" && c == "c -1,-1 l 15,5,0 [x15 z0]" &&
              cChunkCoordText::line(17, 266, 96, false, 0, 0, 0) == "chunk  player c 1,6 l 1,10,0 [z0]  |  look -",
          "hud: F3 chunk line names chunk and local x/y/z, flags seams (local 0 / 15), handles negative coords");
  }
  {  // 2026-10-31 (JD): every placeable block is in the creative palette, once, in registry order
    const auto& reg = blockRegistry();
    const std::vector<BlockId> pal = paletteBlocks(), all = paletteBlocks(true);
    bool ok = all.size() == reg.size() - 1 && std::find(pal.begin(), pal.end(), kRandor) != pal.end() &&
              std::find(pal.begin(), pal.end(), kAir) == pal.end();
    size_t j = 0;
    for (size_t id = 1; id < reg.size(); ++id) {
      if (isPaletteHidden(BlockId(id))) continue;
      ok = ok && j < pal.size() && pal[j] == BlockId(id);
      ++j;
    }
    check(ok && j == pal.size(), "palette: every placeable registry block is listed once, in registry order (Randor included)");
  }
  {  // 2026-10-31 (JD): grouped palette = same blocks, families in order 0..3
    std::vector<BlockId> pal = paletteBlocks(), grp = paletteBlocksGrouped();
    bool ok = grp.size() == pal.size() && std::is_permutation(grp.begin(), grp.end(), pal.begin());
    for (size_t i = 1; i < grp.size(); ++i) ok = ok && paletteFamily(grp[i - 1]) <= paletteFamily(grp[i]);
    ok = ok && !grp.empty() && paletteFamily(grp.front()) == 0;
    check(ok, "palette grouped: same blocks, Trace family first, then lamps, building, other");
    check(cBlockFamilies::get().unassigned().empty(), "families: every registered block has a family");
  }
  std::printf(fails ? "CONTENT SELFTEST FAILED (%d)\n" : "CONTENT SELFTEST OK\n", fails);
  return fails;
}

}  // namespace lithokraft
