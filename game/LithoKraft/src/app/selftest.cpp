// Cortana 2026-10-15 0455: PORT getpid on _WIN32
// Headless checks (--selftest): signal logic, audio, input, dimension load/unload, demo circuit, mesher,
// overlay geometry and a headless Vulkan instance (skipped without a Vulkan loader).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <cstdio>
#if defined(_WIN32)
#include <process.h>  // _getpid (Windows has no unistd.h)
#define getpid _getpid
#else
#include <unistd.h>  // getpid: per-run unique temp dirs (G-M7)
#endif
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include "litholattice/core/log.h"
#include "litholattice/mods/mod_interface.h"  // kPerm* for the script sandbox
#include "lithokraft/game/build_version.h"  // 2026-10-16 (JD): build counter carry selftest
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/items.h"
#include "lithokraft/game/inventory.h"
#include "lithokraft/content/content_selftest.h"
#include "lithokraft/game/input_selftest.h"
#include "scripts_dir.h"
#include "selftest.h"
#include "handshake_test.h"
#include "control_stream_test.h"
#include "demo_record_test.h"
#include "litholattice/core/threading.h"
#include "litholattice/render/mesher.h"
#include "litholattice/assets/assets_selftest.h"
#include "lithokraft/game/resource_manager.h"
#include "litholattice/render/hud_ui_renderer.h"
#include "lithokraft/game/dimensions.h"
#include "lithokraft/game/chunk_streamer.h"
#include "lithokraft/game/world_save.h"
#include "litholattice/storage/anvil_region.h"    // P3-2 C-L3 test
#include "litholattice/storage/file_contract.h"   // P3-2 C-L2 test
#include "litholattice/world/light_engine.h"      // P3-2 B-L5 test
#include "litholattice/procgen/shrubs.h"          // spawn garden sprite pins (2026-10-13)
#include "litholattice/render/block_materials.h"
#include "litholattice/render/cpu_texture_array.h"  // Cortana 2026-10-16: BUG-0003 test
#include "litholattice/render/overlay_geometry.h"
#include "litholattice/render_vk/vulkan_renderer.h"
#include "litholattice/audio/audio_selftest.h"
#include "litholattice/input/input_selftest.h"
#include "litholattice/procgen/procgen_selftest.h"
#if defined(WS_DISABLED)
#undef WS_DISABLED
#endif
#if defined(WS_VISIBLE)
#undef WS_VISIBLE
#endif
#include "litholattice/ui/litho_gui_selftest.h"  // Litho GUI migration: replaces ui_selftest.h
#include "lithokraft/game/game_ui_selftest.h"
#include "lithokraft/game/wiring_selftest.h"
#include "lithokraft/game/component_test_selftest.h"  // 2026-11-12 (JD): Component Test world
#include "lithokraft/game/sprite_host.h"
#include "litholattice/world/chunk_swarm.h"  // R4: cSlackHarvester
#include "lithokraft/game/frame_budget.h"  // R4
#include "lithokraft/game/view_mesh_gate.h"  // R4
#include "lithokraft/game/settings.h"        // A4: render.fps_cap Settings field
#include "lithokraft/game/settings_cvars.h"  // A4: render.fps_cap cvar row
#include "litholattice/console/concmd.h"     // A4: cConsoleReg for the cvar flow check
#include "litholattice/render/frame_pacer.h"  // A4: cFramePacer drive semantics
#include "lithokraft/game/ai_host.h"         // A6: AiWorld production host smoke test
#include "lithokraft/game/deceitor.h"        // Deceitor mob behaviour test (2026-10-14)
#include "lithokraft/game/interchange.h"    // Interchange settlements test (2026-10-14)
#include "lithokraft/game/player_locomotion.h"
#include "lithokraft/content/component.h"
#include "lithokraft/game/world_save.h"
#include "litholattice/script/script_engine.h"
#include "lithokraft/content/signal.h"
#include "lithokraft/content/signal_world.h"         // t46u 6b-3b: tick-side signal init test
#include "lithokraft/game/tick_signal_posts.h"       // t46u 6b-3b
#include "litholattice/core/gate_lock.h"
#include "litholattice/sim/sim_thread.h"
#include "lithokraft/content/item_use.h"
#include "lithokraft/game/entities.h"
#include "lithokraft/game/item_drops.h"
#include "litholattice/projectile/projectile.h"
#include "litholattice/world/dimension.h"

namespace lithokraft {

static std::string stubDimension() { return "selftest"; }

/// Per-run unique temp directory (P2b-2, review G-M7): the suite's fixed
/// shared temp paths (save/scripts/logs) collided when selftest ran in
/// parallel (ctest -j2, or a manual run beside ctest), producing flaky
/// cross-talk failures. pid + steady-clock stamp + a per-process counter
/// make every run's dirs its own.
static std::filesystem::path uniqueTempDir(const char* base) {
  static std::atomic<unsigned> seq{0};
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (std::string(base) + "-" + std::to_string(::getpid()) + "-" + std::to_string(stamp) + "-" +
          std::to_string(seq.fetch_add(1)));
}

int runSelfTests(const std::string& scriptsDir, const std::vector<std::filesystem::path>& packDirs, bool timingHandshake) {
  const int signalFails = runSignalSelfTest();
  int fails = signalFails + runContentSelfTest() + runAudioSelfTest() + runInputSelfTest() + runGameInputSelfTest() + runAssetsSelfTest();
  auto check = [&](bool ok, const char* what) { std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what); if (!ok) ++fails; };
  cJobPool pool(2);
  {
    std::printf("  (the next two job errors are expected)\n"); std::fflush(stdout);
    std::atomic<int> ran{0};
    JobBatch batch(pool);
    for (int i = 0; i < 8; ++i) batch.submit([&ran, i] { ++ran; if (i == 3) throw std::runtime_error("selftest"); });
    const int failed = batch.wait();
    check(failed == 1 && ran == 8, "job batch: a throwing job is caught and counted, the batch still completes");
    pool.submit([] { throw 42; });
    pool.waitIdle();
    check(pool.failedJobs() == 1, "job pool: a throwing job does not terminate, waitIdle() still returns");
  }
  fails += procgen::runProcgenSelfTest(pool);
  fails += litholattice::runLithoGuiSelftest() ? 0 : 1;  // Litho GUI migration: bool pass, not a fail count
  fails += runGameUiSelfTest();
  fails += runWiringSelfTest();
  fails += runSpriteHostSelfTest(packDirs);
  fails += runLocomotionSelfTest();
  fails += runComponentTestSelfTest();  // 2026-11-12 (JD t5295u / t5296u)
  {  // render.fps_cap (A4 / brief B7, 2026-10-13): the game-side wiring of
     // the engine cFramePacer Ã¢â‚¬â€ Settings field, JSON persistence, the
     // render.fps_cap cvar flow, and pacer drive semantics. Deterministic
     // by construction: the pacer's returned sleep is computed from
     // explicit time points; the single measured check is bounded wide.
    using litholattice::cFramePacer;
    check(GameSettings{}.fpsCap == 30 && GameSettings{}.vsync && GameSettings{}.swapImages == 3 && GameSettings{}.streamRadius == 8 && GameSettings{}.tickRadius == 6, "pacing: defaults 30 FPS cap, V-sync on, triple buffering, render distance 8, sim distance 6 (JD 2026-10-24)");
    {  // JSON round-trip + load-side clamp (0..1000).
      const auto dir = uniqueTempDir("lithokraft-fpscap");
      const std::string path = (dir / "settings.json").string();
      GameSettings out;
      out.fpsCap = 60;
      const bool saved = out.save(path);
      check(saved && GameSettings::load(path).fpsCap == 60, "fps cap: settings.json round-trips fpsCap=60");
      std::ofstream raw(path, std::ios::trunc);
      raw << "{\"fpsCap\": 5000}\n";
      raw.close();
      check(GameSettings::load(path).fpsCap == 1000, "fps cap: settings.json load clamps 5000 -> 1000");
      std::ofstream raw2(path, std::ios::trunc);
      raw2 << "{\"fpsCap\": -5}\n";
      raw2.close();
      check(GameSettings::load(path).fpsCap == -1, "fps cap: settings.json load clamps -5 -> -1 (auto)");
    check(GameSettings{}.renderer == "vulkan", "renderer: shipped default is vulkan (JD 2026-10-30)");
    {
      GameSettings rs; rs.renderer = "vulkan";
      const std::string rp = path + ".renderer";
      check(rs.save(rp) && GameSettings::load(rp).renderer == "vulkan", "renderer: settings.json round-trips renderer=vulkan");
      std::ofstream(rp) << "{\"renderer\": \"dx12\"}\n";
      check(GameSettings::load(rp).renderer == "vulkan", "renderer: unknown value loads as vulkan (the default)");
      std::remove(rp.c_str());
    }
      std::error_code ec;
      std::filesystem::remove_all(dir, ec);
    }
    {  // Pacer semantics: off = zero wait; cap clamps; on-time frame waits
       // the computed remainder of its interval; overrun waits nothing.
      cFramePacer off;
      const auto t0 = cFramePacer::Clock::now();
      off.beginFrame(t0);
      check(!off.enabled() && off.capFps() == 0 && off.endFrameAndWait(t0) == cFramePacer::Clock::duration::zero(),
            "fps cap: pacer default off Ã¢â‚¬â€ endFrameAndWait is a no-op");
      cFramePacer p;
      p.setCapFps(5000);
      check(p.capFps() == 1000 && p.enabled(), "fps cap: pacer clamps cap to 1000");
      p.setCapFps(60);
      const auto start = cFramePacer::Clock::now();
      p.beginFrame(start);
      const auto slept = p.endFrameAndWait(start);
      const double sleptMs = std::chrono::duration<double, std::milli>(slept).count();
      const double elapsedMs = std::chrono::duration<double, std::milli>(cFramePacer::Clock::now() - start).count();
      check(sleptMs > 10.0 && sleptMs < 25.0 && elapsedMs >= 10.0 && elapsedMs < 100.0,
            "fps cap: a 60 FPS cap holds a fresh frame to its ~16.7 ms interval (bounded measured check)");
      p.beginFrame(start);
      check(p.endFrameAndWait(start + std::chrono::seconds(1)) == cFramePacer::Clock::duration::zero(),
            "fps cap: an overrun frame waits nothing (no catch-up debt)");
      p.setCapFps(0);
      check(!p.enabled(), "fps cap: setCapFps(0) disables pacing (live change back to off)");
    }
    {  // Cvar flow: render.fps_cap registered by SettingsCvars; a console
       // set flows through the apply path with the clamp, like a UI edit.
      litholattice::console::cConsoleReg reg;
      GameSettings applied;
      SettingsCvars bridge([&](const GameSettings& s) { applied = s; }, [] { return GameSettings{}; });
      bridge.registerOn(reg);
      const auto* cv = reg.findCVar("render.fps_cap");
      check(cv && cv->value == "30", "fps cap: render.fps_cap cvar registered, default 30");
      check(reg.setCVar("render.fps_cap", "120") && applied.fpsCap == 120,
            "fps cap: cvar set 120 flows through the settings apply path");
      check(reg.setCVar("render.fps_cap", "99999") && applied.fpsCap == 1000,
            "fps cap: cvar set clamps to 1000 through the apply path");
      check(reg.setCVar("render.fps_cap", "0") && applied.fpsCap == 0,
            "fps cap: cvar set 0 turns the cap off through the apply path");
      // Fix run 3: diamond is the default for both footprints; shape cvars are
      // case-insensitive and an unknown value falls back to diamond (never square).
      const auto* sa = reg.findCVar("sim.area_shape");
      const auto* ss = reg.findCVar("render.stream_shape");
      check(sa && sa->value == "diamond" && ss && ss->value == "diamond" && GameSettings{}.diamondSimArea && GameSettings{}.diamondStreamArea,
            "area shape (fix run 3): sim.area_shape and render.stream_shape default to diamond (settings + config)");
      check(reg.setCVar("sim.area_shape", "ROUND") && applied.roundSimArea && !applied.diamondSimArea &&
                reg.setCVar("sim.area_shape", "Square") && !applied.roundSimArea && !applied.diamondSimArea,
            "area shape (fix run 3): sim.area_shape parses case-insensitively");
      check(reg.setCVar("sim.area_shape", "hexagon") && applied.diamondSimArea && !applied.roundSimArea,
            "area shape (fix run 3): an unknown sim.area_shape falls back to diamond (warning logged)");
      check(reg.setCVar("render.stream_shape", "SQUARE") && !applied.diamondStreamArea &&
                reg.setCVar("render.stream_shape", "round") && applied.diamondStreamArea,
            "area shape (fix run 3): render.stream_shape is case-insensitive; unknown/round falls back to diamond");
    }
  }
  fails += runAiHostSelfTest();
  fails += runDeceitorSelfTest();
  fails += runInterchangeSelfTest();
  cDimensionReg reg = createDimensions();
  Dimension* wafer = reg.find("wafer");
  Dimension* ow = reg.find("organic");
  {  // B5 (2026-10-13, Team B worker2): cosmic + underwafer registered
     // with crust-level content; the void deliberately NOT registered
     // (infinite extent is not representable in a packPos key -- see
     // dimensions.h). Smoke: each new dimension's generator loads
     // fail-closed-clean and a real load() generates columns whose fill
     // matches its JSON's promise. PRESERVE this block in merges.
    Dimension* cos = reg.find("cosmic");
    Dimension* uw = reg.find("underwafer");
    check(cos && uw, "dimensions: cosmic and underwafer resolve in the registry (B5)");
    check(reg.find("void") == nullptr, "dimensions: void stays unregistered (infinite extent, no packPos representation)");
    check(reg.find("overworld") == ow, "dimensions: overworld alias still resolves to organic");
    check(cos && cos->generator && uw && uw->generator,
          "dimensions: cosmic/underwafer generators load without fail-closed errors");
    check(builtinWorldgenSource("cosmic") && builtinWorldgenSource("underwafer"),
          "dimensions: built-in fallback JSON exists for cosmic and underwafer");
    if (cos && cos->generator) {
      const Spawn cs = cos->load(pool, 2);
      const int sy = cos->chunks.surfaceY(int(cs.x), int(cs.z));
      check(cos->lastLoadFailures == 0 && sy >= 0 && sy < 256 && cs.y > float(sy) && cs.y <= float(sy) + 2.f,
            "dimensions: cosmic smoke load -- spawn on a crust surface inside the solid band (y < 256)");
      bool emptyAbove = true;
      for (const auto& [k, col] : cos->chunks.columns())
        for (int s = 16; s < kSubChunks; ++s)
          if (col->subs[s] && col->subs[s]->nonAir > 0) emptyAbove = false;
      check(emptyAbove, "dimensions: cosmic is all-air at y >= 256 (the Empty, as air until a medium field exists)");
    }
    if (uw && uw->generator) {
      const Spawn us = uw->load(pool, 2);
      const int sy = uw->chunks.surfaceY(int(us.x), int(us.z));
      check(uw->lastLoadFailures == 0 && sy > 0 && sy < 256 && us.y > float(sy) && us.y <= float(sy) + 2.f &&
                uw->chunks.get(int(us.x), sy, int(us.z)) == kStone,
            "dimensions: underwafer smoke load -- spawn on a stone surface in the deep band");
    }
  }

  const Spawn sp = wafer->load(pool, 6);
  {  // Spawn resolution (hands-on loop, headless portion): the Wafer spawn returned by load()
     // sits in air above a non-air surface cell at its own column.
    const int sx = int(sp.x), sz = int(sp.z), sy = wafer->chunks.surfaceY(sx, sz);
    std::printf("  spawn: wafer load(6) -> (%.1f, %.1f, %.1f), surfaceY at that column = %d\n", sp.x, sp.y, sp.z, sy);
    // Upper bound included (P2b-2, review G-L6): feet may stand at most
    // ~1 block above the surface cell's top (sy+1 is standing ON it).
    // The old check had no upper bound and passed with the G-M1 spawn
    // floating 11+ blocks up Ã¢â‚¬â€ it could not fail for that class of bug.
    check(sy >= 0 && sp.y > float(sy) && sp.y <= float(sy) + 2.f && wafer->chunks.get(sx, sy, sz) != kAir && wafer->chunks.get(sx, int(sp.y), sz) == kAir,
          "spawn: wafer spawn resolves to air just above the surface at its column (no float)");
  }
  {  // Spawn garden (2026-10-13, JD directive): waferSetup plants the two
     // seeded Litho Formula shrubs flanking the spawn -- seed 311 three
     // cells west, seed 314 three east, standing on the platform at the
     // spawn's own cell height. Exactly one of each exists in the whole
     // map, a fresh regeneration reproduces the same cells, and the pair
     // rides the save/load round trip below (checked explicitly against
     // the restored map there). PRESERVE this block in merges.
    const int sx = int(sp.x), sz = int(sp.z), py = int(sp.y);
    check(wafer->chunks.get(sx - 3, py, sz) == kFormulaShrub311,
          "spawn garden: seed-311 formula shrub stands west of the wafer spawn");
    check(wafer->chunks.get(sx + 3, py, sz) == kFormulaShrub314,
          "spawn garden: seed-314 formula shrub stands east of the wafer spawn");
    check(wafer->chunks.get(sx, py, sz) == kAir,
          "spawn garden: the spawn cell itself stays clear");
    int n311 = 0, n314 = 0;
    const StableId sid311 = blockRegistry().stableIdOf(kFormulaShrub311);
    const StableId sid314 = blockRegistry().stableIdOf(kFormulaShrub314);
    for (const auto& [k, col] : wafer->chunks.columns())
      for (int syy = 0; syy < kSubChunks; ++syy) {
        const auto& s = col->subs[syy];
        if (!s) continue;
        for (int i = 0; i < kSubChunkVolume; ++i) {
          if (s->blocks.at(i) == sid311) ++n311;
          if (s->blocks.at(i) == sid314) ++n314;
        }
      }
    check(n311 == 1 && n314 == 1, "spawn garden: exactly one 311 and one 314 shrub in the whole wafer");
    cDimensionReg reg2 = createDimensions();
    Dimension* wafer2 = reg2.find("wafer");
    const Spawn sp2 = wafer2->load(pool, 6);
    check(int(sp2.x) == sx && int(sp2.z) == sz && int(sp2.y) == py &&
              wafer2->chunks.get(sx - 3, py, sz) == kFormulaShrub311 &&
              wafer2->chunks.get(sx + 3, py, sz) == kFormulaShrub314,
          "spawn garden: regeneration reproduces both shrubs at the same cells");
    // The sprites themselves (canonical bytes): ShrubParams defaults with
    // seed 311 / 314 -- one shared alpha mask, seed-painted colours.
    litholattice::procgen::ShrubParams p311; p311.seed = 311;
    litholattice::procgen::ShrubParams p314; p314.seed = 314;
    const auto spr311 = litholattice::procgen::generateShrubSprite(p311, litholattice::procgen::kPlantTextureSize);
    const auto spr314 = litholattice::procgen::generateShrubSprite(p314, litholattice::procgen::kPlantTextureSize);
    bool maskSame = spr311.size() == spr314.size(), colorDiff = false;
    int filled = 0;
    if (maskSame)
      for (size_t i = 0; i < spr311.size(); i += 4) {
        if ((spr311[i + 3] == 0) != (spr314[i + 3] == 0)) maskSame = false;
        if (spr311[i + 3]) { ++filled; if (spr311[i] != spr314[i] || spr311[i + 1] != spr314[i + 1] || spr311[i + 2] != spr314[i + 2]) colorDiff = true; }
      }
    check(maskSame && colorDiff && filled == 156,
          "spawn garden: seeds 311/314 share one alpha mask (156 filled px) and differ in colour");
  }
  cSignalSim sig;
  sig.scan(wafer->chunks);
  for (int i = 0; i < 12; ++i) sig.tick(wafer->chunks, 2, 2, 12, [](int, int, int) {});
  int lamps = 0, lit = 0, clamps = 0, clit = 0, subs = 0, crustSubs = 0, surfaceSy = 0;
  for (auto& [k, col] : wafer->chunks.columns())
    for (int sy = 0; sy < kSubChunks; ++sy) {
      const auto& s = col->subs[sy];
      if (!s) continue;
      ++subs; if (sy <= 16) ++crustSubs; surfaceSy = std::max(surfaceSy, sy);
      const StableId lampSid = blockRegistry().stableIdOf(kLamp);  // StableId voxel currency
      for (int i = 0; i < kSubChunkVolume; ++i) {
        if (s->blocks.at(i) == lampSid) { ++lamps; lit += s->state[i] & 1; }
        if (blockRegistry().defByStableId(s->blocks.at(i)).role == SimRole::ColorLamp) { ++clamps; clit += stValue(s->state[i]) > 0; }
      }
    }
  check(lamps == 9 && lit == 7, "wafer demo: 9 plain lamps, 7 lit at launch (repeator, comparator, redirector rows included)");
  std::printf("  wafer demo: color lamps %d, lit %d (want 35 / 33)\n", clamps, clit);  // 2026-11-04 (Elfie F4 instrumentation)
  check(clamps == 35 && clit == 33, "wafer demo: lamp wall (32) + color lamps lit at launch");
  check(surfaceSy >= 15 && crustSubs <= 36 * 3 && subs <= 36 * 4,
        "wafer: surface near layer 256 of 768, only the crust is generated (empty sections skipped; feature spires may add a 4th section)");
  {  // Save round trip: blocks, cell states and signal-sim memory restore bit for bit, and the
     // restored world keeps ticking identically to the original.
    const auto dir = uniqueTempDir("lithokraft_save_selftest");  // G-M7: per-run unique
    std::filesystem::remove_all(dir);
    auto emptyLike = [](const ChunkMap& w) {
      ChunkMap m;
      m.setRegistry(&blockRegistry());
      for (const auto& [k, col] : w.columns()) { auto c = std::make_unique<ChunkColumn>(); c->cx = col->cx; c->cz = col->cz; m.insert(std::move(c)); }
      return m;
    };
    auto sameCells = [](const ChunkMap& a, const ChunkMap& b) {
      if (a.columns().size() != b.columns().size()) return false;
      for (const auto& [k, ca] : a.columns()) {
        const auto it = b.columns().find(k);
        if (it == b.columns().end()) return false;
        for (int sy = 0; sy < kSubChunks; ++sy) {
          const SubChunk* x = ca->subs[size_t(sy)].get(); const SubChunk* y = it->second->subs[size_t(sy)].get();
          const bool ex = !x || !x->nonAir, ey = !y || !y->nonAir;
          if (ex != ey) return false;
          // SEMANTIC equality (P2b-2, review SV-M1/C-M1): compare the
          // resolved voxel at every cell (blocks.at(i)) + its state, NOT
          // the palette representation. A save/load round trip rebuilds
          // palettes in scan order, so insertion order legitimately
          // differs for structure/edit-bearing sub-chunks while every
          // voxel is identical Ã¢â‚¬â€ the old `x->blocks != y->blocks`
          // (VoxelPalette::operator==, representation equality) failed 4
          // round-trip checks on bit-perfect data. operator== itself is
          // NOT changed: representation equality is legitimately used
          // elsewhere.
          if (!ex) {
            for (int i = 0; i < kSubChunkVolume; ++i)
              if (x->blocks.at(i) != y->blocks.at(i) || x->state[size_t(i)] != y->state[size_t(i)]) return false;
          }
        }
      }
      return true;
    };
    const auto noDirty = [](int, int, int) {};
    for (int i = 0; i < 25; ++i) sig.tick(wafer->chunks, 2, 2, 12, noDirty);  // clocks mid-cycle: live memory
    const auto stateBefore = sig.exportState();
    size_t memBytes = 0; for (const auto& [k, b] : stateBefore) memBytes += b.size();
    ChunkMap restored = emptyLike(wafer->chunks);
    std::vector<uint8_t> blob;
    int n = 0;
    {
      cWorldSave save(dir);
      save.saveDimension("wafer", wafer->chunks, sig, true);
      PlayerMeta pm{{1.5f, 90.25f, -3.f}, 0.5f, -0.25f, "organic", 4, {kRedTrace, kAir, kLamp}};
      std::string err;
      const bool wrote = save.writePlayer(pm, &err);
      const auto back = save.readPlayer(&err);
      check(wrote && back && back->pos.y == 90.25f && back->dimension == "organic" && back->hot == 4 && back->hotbar == pm.hotbar,
            "save: player metadata (position, look, dimension, hotbar) round trip");
    }
    {
      cWorldSave save(dir);  // a fresh process view of the same directory
      n = save.loadDimension("wafer", restored, blob);
    }
    cSignalSim sig2;
    sig2.scan(restored);
    const bool imported = sig2.importState(blob);
    check(n == int(wafer->chunks.columns().size()) && imported && memBytes > 0 && sameCells(wafer->chunks, restored) &&
              sig2.exportState() == stateBefore,
          "save: blocks, cell states and component memory / timers restore bit for bit");
    check(restored.get(int(sp.x) - 3, int(sp.y), int(sp.z)) == kFormulaShrub311 &&
              restored.get(int(sp.x) + 3, int(sp.y), int(sp.z)) == kFormulaShrub314,
          "spawn garden: both shrubs survive the save/load round trip");
    for (int i = 0; i < 70; ++i) { sig.tick(wafer->chunks, 2, 2, 12, noDirty); sig2.tick(restored, 2, 2, 12, noDirty); }
    check(sameCells(wafer->chunks, restored) && sig2.exportState() == sig.exportState(),
          "save: the restored world ticks identically to the original (70 more ticks)");
    {  // PlayN (JD): Switch facing (4 yaws x floor / ceiling / 4 wall mounts) survives save / load
      ChunkMap m = emptyLike(wafer->chunks);
      const auto& c0 = m.columns().begin()->second;
      const int bx = c0->cx * 16, bz = c0->cz * 16, by = 100;
      int k = 0;
      for (int face = 1; face <= 6; ++face)
        for (int f = 0; f < 4; ++f, ++k) { m.set(bx + k % 16, by + 2 * (k / 16), bz + 4, kSwitch); m.setState(bx + k % 16, by + 2 * (k / 16), bz + 4, withAttach(makeState(0, f), face)); }
      cSignalSim ss; ss.scan(m);
      const auto d3 = dir / "switch_facing";
      { cWorldSave w(d3); w.saveDimension("wafer", m, ss, true); }
      ChunkMap m2 = emptyLike(wafer->chunks); std::vector<uint8_t> b3;
      { cWorldSave w(d3); w.loadDimension("wafer", m2, b3); }
      bool ok = true; k = 0;
      for (int face = 1; face <= 6; ++face)
        for (int f = 0; f < 4; ++f, ++k) {
          const CellState s = m2.getState(bx + k % 16, by + 2 * (k / 16), bz + 4);
          ok = ok && m2.get(bx + k % 16, by + 2 * (k / 16), bz + 4) == kSwitch && stFacing(s) == f && stAttach(s) == face;
        }
      check(ok, "save: Switch facing + mount face (24 combos) round trip");
    }    {  // 2026-11-01 (JD LL_LF_SAVE / LL_LF_LOAD): on-disk round trip old <-> lock-free, both
       // directions, plus the streamed path (LF loadColumn) and save-then-immediate-load ordering.
      const auto now = sig.exportState();
      for (int mode = 0; mode < 2; ++mode) {
        const auto d2 = dir / (mode == 0 ? "lf_save_old_load" : "old_save_lf_load");
        ChunkMap back = emptyLike(wafer->chunks);
        std::vector<uint8_t> b2;
        int n2 = 0;
        { cWorldSave w(d2); w.setLockFreeIo(mode == 0, mode == 0); w.saveDimension("wafer", wafer->chunks, sig, true); }
        { cWorldSave r(d2); r.setLockFreeIo(mode == 1, mode == 1); n2 = r.loadDimension("wafer", back, b2); }
        cSignalSim s3; s3.scan(back);
        check(n2 == int(wafer->chunks.columns().size()) && s3.importState(b2) && sameCells(wafer->chunks, back) && s3.exportState() == now,
              mode == 0 ? "LL_LF_SAVE: lock-free save, old load: blocks and component state bit for bit (format unchanged)"
                        : "LL_LF_LOAD: old save, lock-free load: blocks and component state bit for bit (format unchanged)");
        if (mode == 1) {
          cWorldSave r(d2); r.setLockFreeIo(true, true);
          bool all = true; int cols = 0;
          for (const auto& [k, col] : wafer->chunks.columns()) {
            auto lc = r.loadColumn("wafer", col->cx, col->cz);
            all = all && lc.status == cWorldSave::ColumnLoad::Loaded && lc.column; ++cols;
          }
          check(all && cols > 0, "LL_LF_LOAD: every column streams back through the lock-free loadColumn");
          // Save (no wait) then load at once: the load must queue behind the pending save of its region.
          const ChunkColumn* first = &*wafer->chunks.columns().begin()->second;
          std::vector<const ChunkColumn*> one{first};
          r.saveColumns("wafer", one, sig, false);
          auto lc = r.loadColumn("wafer", first->cx, first->cz);
          check(lc.status == cWorldSave::ColumnLoad::Loaded && r.flush(), "LL_LF_SAVE: save-then-immediate-load sees the saved column (FIFO behind pending saves)");
        }
      }
    }
    {
      std::fstream f(dir / "player.lat", std::ios::in | std::ios::out | std::ios::binary);
      f.seekp(12); f.put('\x7f'); f.close();
      std::string err;
      cWorldSave save(dir);
      check(!save.readPlayer(&err) && !err.empty(), "save: a damaged player.lat is detected (CRC), not misread");
    }
    {  // P2b-1 (Appendix G H1/H2 + Appendix C M2): player.lat v4 Ã¢â‚¬â€ the
       // canonical 42+8 inventory persists, the hotbar is stored as string
       // ids and name-remapped on load, and v1Ã¢â‚¬â€œv3 files keep loading.
      const auto pdir = dir / "p2b1_player";
      auto f32w = [](storage::ByteWriter& w, float v) { uint32_t u; std::memcpy(&u, &v, 4); w.u32(u); };
      auto writeRawPlayer = [&](const std::filesystem::path& d, const storage::Bytes& body) {
        std::filesystem::create_directories(d);
        storage::ByteWriter f;
        f.u32(0x4D504C4Cu);  // "LLPM"
        f.u32(storage::crc32(body.data(), body.size()));
        f.bytes(body);
        std::ofstream out(d / "player.lat", std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(f.out.data()), std::streamsize(f.out.size()));
      };
      auto slotIs = [](const Inventory& inv, int flat, uint32_t id, uint16_t count) {
        const ItemStack& s = inv.slotAt(flat);
        return s.itemId == id && s.count == count;
      };
      {  // v4 round trip: every slot kind filled Ã¢â‚¬â€ hotbar incl. the Fragment
         // and a tool, an offhand, main, and gear Ã¢â‚¬â€ comes back identical.
        PlayerMeta pm{{2.5f, 80.f, 7.f}, 0.25f, 0.5f, "wafer", 3, {kLamp, kAir, kRedTrace}};
        pm.inventory.hotbar()[0] = ItemStack{kItemFragment, 1};
        pm.inventory.hotbar()[1] = ItemStack{uint32_t(kLamp), 33};
        pm.inventory.hotbar()[3] = ItemStack{kItemBashor, 1};
        pm.inventory.offhandLeft() = ItemStack{kItemSilicaSeed, 200};
        pm.inventory.main()[5] = ItemStack{uint32_t(kChipor), 12};
        pm.inventory.gear()[kGearHead] = ItemStack{kItemAtlas, 1};
        pm.inventory.gear()[kGearMind] = ItemStack{uint32_t(kStone), 64};
        pm.hasInventory = true;
        std::string err;
        cWorldSave save(pdir);
        const bool wrote = save.writePlayer(pm, &err);
        const auto back = save.readPlayer(&err);
        bool same = back && back->fileVersion == PlayerMeta::kVersion && back->hasInventory && back->hotbar == pm.hotbar;
        if (back)
          for (int i = 0; i < kTotalSlots; ++i) {
            const ItemStack& a = pm.inventory.slotAt(i);
            same = same && slotIs(back->inventory, i, a.itemId, a.count);
          }
        check(wrote && same,
              "save (P2b-1): player.lat (current version) round-trips the full inventory (hotbar + offhand + main + gear, Fragment + tool + block stacks)");
        // The persisted Fragment is seen by the grant check: no duplicate.
        bool noDup = false;
        if (back) {
          Inventory reloaded = back->inventory;
          noDup = reloaded.countOf(kItemFragment) == 1 && !grantStartingItems(reloaded) &&
                  reloaded.countOf(kItemFragment) == 1;
        }
        check(noDup, "save (P2b-1): a saved Fragment is kept across save/load Ã¢â‚¬â€ the boot grant does not duplicate it");
      }
      {  // v3 file (hand-built, the old layout: raw u16 hotbar, stats,
         // vitals, NO inventory block): loads with an empty inventory and
         // the old raw-id hotbar behavior.
        storage::ByteWriter w;
        w.u16(3);
        f32w(w, 1.f); f32w(w, 64.f); f32w(w, 2.f); f32w(w, 0.5f); f32w(w, -0.25f);
        w.bytes(storage::Bytes{'w', 'a', 'f', 'e', 'r'});
        w.u8(2);                       // hot slot
        w.u8(3);                       // hotbar count
        w.u16(kRedTrace); w.u16(kAir); w.u16(kLamp);
        w.u16(0);                      // stats
        w.u16(0);                      // vitals
        writeRawPlayer(pdir, w.out);
        cWorldSave save(pdir);
        std::string err;
        const auto back = save.readPlayer(&err);
        check(back && back->fileVersion == 3 && !back->hasInventory && back->inventory.isEmpty() &&
                  back->hot == 2 && back->hotbar == std::vector<BlockId>({kRedTrace, kAir, kLamp}),
              "save (P2b-1): a v3 player.lat loads with an empty inventory and its raw-id hotbar intact");
      }
      {  // v4 string remap (rename simulation): the pre-rename alias
         // "lithokraft:red_wire" must follow to Red Trace in BOTH the
         // hotbar field and an inventory slot; an unknown item string
         // drops only its own stack.
        storage::ByteWriter w;
        w.u16(4);
        f32w(w, 1.f); f32w(w, 64.f); f32w(w, 2.f); f32w(w, 0.f); f32w(w, 0.f);
        w.bytes(storage::Bytes{'w', 'a', 'f', 'e', 'r'});
        w.u8(0);
        w.u8(2);  // hotbar: red_wire (alias), empty
        w.bytes(storage::Bytes{'l', 'i', 't', 'h', 'o', 'k', 'r', 'a', 'f', 't', ':', 'r', 'e', 'd', '_', 'w', 'i', 'r', 'e'});
        w.bytes(storage::Bytes{});
        w.u16(0);  // stats
        w.u16(0);  // vitals
        w.u16(uint16_t(kTotalSlots));
        auto putSlot = [&w](const char* sid, uint16_t count) {
          w.bytes(storage::Bytes(sid, sid + std::strlen(sid)));
          w.u16(count);
        };
        putSlot("lithokraft:red_wire", 7);        // slot 0: alias -> Red Trace stack
        putSlot("lithokraft:fragment", 1);        // slot 1: the Fragment
        putSlot("lithokraft:no_such_item", 5);    // slot 2: unknown -> dropped
        for (int i = 3; i < kTotalSlots; ++i) putSlot("", 0);
        writeRawPlayer(pdir, w.out);
        cWorldSave save(pdir);
        std::string err;
        const auto back = save.readPlayer(&err);
        check(back && back->hasInventory && back->hotbar.size() == 2 && back->hotbar[0] == kRedTrace &&
                  back->hotbar[1] == kAir && slotIs(back->inventory, 0, uint32_t(kRedTrace), 7) &&
                  slotIs(back->inventory, 1, kItemFragment, 1) && back->inventory.slotAt(2).empty(),
              "save (P2b-1): v4 hotbar + inventory string ids name-remap on load (rename alias follows); an unknown item drops alone");
      }
      {  // C9 (2026-11-12): v4 inventory blocks from A9 (58 slots: 16 hotbar + 2 + 4x8 main + 8 gear,
         // C9's own layout again) load as written, and 50-slot (8-slot hotbar) blocks remap section
         // by section: hotbar, hands, main and gear keep their places.
        bool allOk = true;
        for (const int layout : {kPreFourRowTotalSlots, kLegacyTotalSlots}) {
          storage::ByteWriter w;
          w.u16(4);
          f32w(w, 1.f); f32w(w, 64.f); f32w(w, 2.f); f32w(w, 0.f); f32w(w, 0.f);
          w.bytes(storage::Bytes{'w', 'a', 'f', 'e', 'r'});
          w.u8(0);
          w.u8(0);   // hotbar field: none
          w.u16(0);  // stats
          w.u16(0);  // vitals
          w.u16(uint16_t(layout));
          const int hot = layout == kLegacyTotalSlots ? kLegacyInvHotbarSlots : kInvHotbarSlots;
          for (int i = 0; i < layout; ++i) {
            uint16_t c = 0;
            if (i == 0) c = 1;                                  // hotbar 0
            else if (i == hot) c = 2;                           // Left Hand
            else if (i == hot + 2) c = 3;                       // main 0
            else if (i == hot + 2 + kLegacyMainSlots - 1) c = 4;  // main 31 (the old last main slot)
            else if (i == hot + 2 + kLegacyMainSlots) c = 5;    // gear Head
            else if (i == layout - 1) c = 6;                    // gear Mind
            const char* sid = c ? "lithokraft:red_wire" : "";
            w.bytes(storage::Bytes(sid, sid + std::strlen(sid)));
            w.u16(c);
          }
          writeRawPlayer(pdir, w.out);
          cWorldSave save(pdir);
          std::string err;
          const auto back = save.readPlayer(&err);
          const uint32_t R = uint32_t(kRedTrace);
          bool ok = back && back->hasInventory && slotIs(back->inventory, 0, R, 1) && slotIs(back->inventory, kInvHotbarSlots, R, 2) &&
                    slotIs(back->inventory, kMainFlatBase, R, 3) && slotIs(back->inventory, kMainFlatBase + 31, R, 4) &&
                    slotIs(back->inventory, kCarrySlots + kGearHead, R, 5) && slotIs(back->inventory, kCarrySlots + kGearMind, R, 6);
          if (back)
            for (int f = kMainFlatBase + 32; f < kCarrySlots; ++f) ok = ok && back->inventory.slotAt(f).empty();
          allOk = allOk && ok;
        }
        check(allOk && kTotalSlots == 58 && kMainSlots == 32 && kCarrySlots == 50,
              "save (C9 4x8): A9 58-slot inventories load as written and 50-slot (8-hotbar) ones remap into the 58-slot layout");
      }
      // C9 (2026-11-12): a B9 90-slot inventory (4x16 main, gear 82..89) loads into the 4x8 layout.
      // Hotbar / hands / main 0..31 keep their places, gear lands on 50..57, B9 main 32..63 spill (in
      // order) into the first free main, then hotbar, then hand slots; a stack with no room is logged,
      // never silently lost: the untouched B9 file stays as player.lat.b9-90slot.bak.
      auto writeB9 = [&](const std::vector<std::pair<int, uint16_t>>& filled, bool withData, int dataRaw) {
        storage::ByteWriter w;
        w.u16(withData ? 5 : 4);
        f32w(w, 1.f); f32w(w, 64.f); f32w(w, 2.f); f32w(w, 0.f); f32w(w, 0.f);
        w.bytes(storage::Bytes{'w', 'a', 'f', 'e', 'r'});
        w.u8(0);
        w.u8(0);
        w.u16(0);
        w.u16(0);
        w.u16(uint16_t(kB9TotalSlots));
        for (int i = 0; i < kB9TotalSlots; ++i) {
          uint16_t c = 0;
          for (const auto& fc : filled)
            if (fc.first == i) c = fc.second;
          const char* sid = c ? "lithokraft:red_wire" : "";
          w.bytes(storage::Bytes(sid, sid + std::strlen(sid)));
          w.u16(c);
        }
        if (withData) {
          w.u16(1);
          w.u16(uint16_t(dataRaw));
          w.bytes(storage::Bytes{'c', '9'});
          w.u16(0);
        }
        writeRawPlayer(pdir, w.out);
      };
      {
        std::error_code ec;
        std::filesystem::remove(pdir / "player.lat.b9-90slot.bak", ec);
        const int b9Main = kMainFlatBase, b9Gear = kMainFlatBase + kB9MainSlots;  // 18, 82
        writeB9({{0, 1}, {kInvHotbarSlots, 2}, {b9Main, 3}, {b9Main + 31, 4}, {b9Main + 32, 7}, {b9Main + 63, 8},
                 {b9Gear + kGearHead, 5}, {b9Gear + kGearMind, 6}},
                true, b9Main + 63);
        cWorldSave save(pdir);
        std::string err;
        const auto back = save.readPlayer(&err);
        const uint32_t R = uint32_t(kRedTrace);
        const bool kept = back && back->hasInventory && slotIs(back->inventory, 0, R, 1) && slotIs(back->inventory, kInvHotbarSlots, R, 2) &&
                          slotIs(back->inventory, kMainFlatBase, R, 3) && slotIs(back->inventory, kMainFlatBase + 31, R, 4) &&
                          slotIs(back->inventory, kCarrySlots + kGearHead, R, 5) && slotIs(back->inventory, kCarrySlots + kGearMind, R, 6);
        check(kept, "save (C9 from B9): a 90-slot B9 inventory keeps hotbar, hands and main 0..31 in place; gear 82..89 lands on 50..57");
        const bool spilled = back && slotIs(back->inventory, kMainFlatBase + 1, R, 7) && slotIs(back->inventory, kMainFlatBase + 2, R, 8);
        check(spilled, "save (C9 from B9): B9 main 32..63 stacks move, in order, to the first free C9 main slots (none deleted)");
        check(back && back->inventory.slotAt(kMainFlatBase + 2).customName == "c9",
              "save (C9 from B9): a moved B9 stack keeps its stack data (name) in its new slot");
        check(std::filesystem::exists(pdir / "player.lat.b9-90slot.bak", ec),
              "save (C9 from B9): the untouched B9 player.lat is kept as player.lat.b9-90slot.bak");
      }
      {
        std::error_code ec;
        std::filesystem::remove(pdir / "player.lat.b9-90slot.bak", ec);
        std::vector<std::pair<int, uint16_t>> filled;
        for (int i = 0; i < kInvHotbarSlots; ++i)
          if (i != 5) filled.push_back({i, 1});  // hotbar 5 free
        filled.push_back({kInvHotbarSlots, 1});  // Left Hand filled, Right Hand free
        for (int i = 0; i < 32; ++i) filled.push_back({kMainFlatBase + i, 1});
        filled.push_back({kMainFlatBase + 40, 9});
        filled.push_back({kMainFlatBase + 41, 10});
        filled.push_back({kMainFlatBase + 42, 11});
        writeB9(filled, false, 0);
        cWorldSave save(pdir);
        std::string err;
        const auto back = save.readPlayer(&err);
        const uint32_t R = uint32_t(kRedTrace);
        bool others = back.has_value();
        if (back)
          for (int i = 0; i < 32; ++i) others = others && slotIs(back->inventory, kMainFlatBase + i, R, 1);
        check(back && others && slotIs(back->inventory, 5, R, 9) && slotIs(back->inventory, kInvHotbarSlots + 1, R, 10) &&
                  slotIs(back->inventory, 0, R, 1),
              "save (C9 from B9): with the main grid full, overflow fills free hotbar slots, then the hands (nothing overwritten)");
        int total = 0;
        if (back)
          for (const auto* s : back->inventory.allSlots()) total += s->empty() ? 0 : 1;
        check(back && total == kCarrySlots && std::filesystem::exists(pdir / "player.lat.b9-90slot.bak", ec),
              "save (C9 from B9): a stack with no free slot is not loaded but stays in the kept B9 file (logged)");
      }
      {  // C9 fix 3 (JD): creative placement never takes from the stack; survival takes one.
        ItemStack one{uint32_t(kStone), 1}, full{uint32_t(kStone), 512}, s1{uint32_t(kStone), 1}, s512{uint32_t(kStone), 512};
        const bool c1 = !consumePlacedOne(one, true) && one.itemId == uint32_t(kStone) && one.count == 1;
        const bool c512 = !consumePlacedOne(full, true) && full.count == 512;
        bool many = true;
        for (int i = 0; i < 600; ++i) many = many && !consumePlacedOne(one, true);
        check(c1 && c512 && many && one.count == 1,
              "place (C9 creative): placing leaves the stack unchanged (from 1, from a full 512, after 600 places)");
        const bool v1 = consumePlacedOne(s1, false) && s1.empty();
        const bool v512 = consumePlacedOne(s512, false) && s512.count == 511 && s512.itemId == uint32_t(kStone);
        check(v1 && v512, "place (C9 survival): placing still takes exactly one (512 -> 511, 1 -> empty)");
      }
      {  // C-L1 (P3-2): a v4 body with a trailing byte (CRC covers it, so
         // only the exact-consumption check can catch it) is refused; and
         // writePlayer refuses a hotbar whose count would truncate.
        storage::ByteWriter w;
        w.u16(4);
        f32w(w, 1.f); f32w(w, 64.f); f32w(w, 2.f); f32w(w, 0.f); f32w(w, 0.f);
        w.bytes(storage::Bytes{'w', 'a', 'f', 'e', 'r'});
        w.u8(0);   // hot slot
        w.u8(0);   // hotbar count
        w.u16(0);  // stats
        w.u16(0);  // vitals
        w.u16(0);  // inventory slots
        w.u8(0xEE);  // trailing garbage
        writeRawPlayer(pdir, w.out);
        cWorldSave save(pdir);
        std::string err;
        check(!save.readPlayer(&err) && err.find("trailing") != std::string::npos,
              "save (C-L1): player.lat with trailing bytes after the last block is refused");
        PlayerMeta big;
        big.hotbar.assign(300, kLamp);
        std::string werr;
        check(!save.writePlayer(big, &werr) && !werr.empty(),
              "save (C-L1): writePlayer refuses a 300-slot hotbar instead of truncating its u8 count");
      }
    }
    {
      // Worldgen determinism: same seed -> identical columns at any thread count and on a fresh load.
      auto genWith = [&](unsigned threads) {
        cJobPool p(threads);
        cHandoffQueue<std::unique_ptr<ChunkColumn>> out;
        {
          JobBatch b(p);
          for (int z = 0; z < 4; ++z)
            for (int x = 0; x < 4; ++x) b.submit([&, x, z] { out.push(wafer->generator->generate(x + 300, z - 2)); });
        }
        std::vector<std::unique_ptr<ChunkColumn>> cols;
        out.drainInto(cols);
        ChunkMap m;
        m.setRegistry(&blockRegistry());
        for (auto& c : cols) if (c) m.insert(std::move(c));
        return m;
      };
      const ChunkMap g1 = genWith(1), g4 = genWith(4), gAll = genWith(std::max(2u, std::thread::hardware_concurrency()));
      check(g1.columns().size() == 16 && sameCells(g1, g4) && sameCells(g1, gAll),
            "worldgen: same seed gives identical columns with 1, 4 and all threads");
      cDimensionReg r1 = createDimensions(), r2 = createDimensions();
      Dimension* a = r1.find("wafer"); Dimension* b = r2.find("wafer");
      a->load(pool, 6);
      b->load(pool, 6);
      check(sameCells(a->chunks, b->chunks), "worldgen: two fresh loads of a dimension (terrain + structures) are identical");

      // Partial saves: save (nothing changed: only component columns), edit, save the changed
      // column, edit + tick, save again; a fresh load (regenerate + overlay the save) must match.
      const auto pdir = dir / "partial";
      cSignalSim sa;
      sa.scan(a->chunks);
      std::unordered_set<uint64_t> changed;
      size_t written = 0;
      {
        cWorldSave save(pdir);
        written += save.saveDimension("wafer", a->chunks, sa, true, &changed);
        a->chunks.set(5, 100, 5, kLamp);       // edit column (0,0)
        a->chunks.set(70, 90, 40, kRedTrace);   // edit column (4,2)
        changed = {ChunkKey::make(0, 0), ChunkKey::make(4, 2)};
        written += save.saveDimension("wafer", a->chunks, sa, true, &changed);
        a->chunks.set(5, 100, 5, kAir);        // undo in (0,0), new edit in (1,3)
        a->chunks.set(20, 95, 50, kLamp);
        changed = {ChunkKey::make(0, 0), ChunkKey::make(1, 3)};
        // Signal ticks change wire / lamp states: their columns get marked, as cSimThread does.
        const auto mark = [&](int x, int, int z) { changed.insert(ChunkKey::make(x >> 4, z >> 4)); };
        for (int i = 0; i < 30; ++i) sa.tick(a->chunks, 2, 2, 12, mark);
        written += save.saveDimension("wafer", a->chunks, sa, true, &changed);
      }
      cDimensionReg r3 = createDimensions();
      Dimension* c = r3.find("wafer");
      c->load(pool, 6);  // regenerated: untouched columns come from the seed
      std::vector<uint8_t> state;
      { cWorldSave save(pdir); save.loadDimension("wafer", c->chunks, state); }
      cSignalSim sc;
      sc.scan(c->chunks);
      const bool imp = sc.importState(state);
      check(written < 3 * a->chunks.columns().size() && imp && sameCells(a->chunks, c->chunks) && sc.exportState() == sa.exportState(),
            "save: save / edit / save / edit+tick / save, then regenerate + load matches bit for bit (cells, states, component memory)");
      for (int i = 0; i < 40; ++i) { sa.tick(a->chunks, 2, 2, 12, noDirty); sc.tick(c->chunks, 2, 2, 12, noDirty); }
      check(sameCells(a->chunks, c->chunks) && sc.exportState() == sa.exportState(), "save: the partially saved world ticks identically after load");
      // Namespaced ids: unique, and a pre-table (legacy) save still loads as the current order.
      std::unordered_set<std::string> ids;
      bool namespaced = true;
      for (size_t i = 1; i < blockRegistry().size(); ++i) {
        const std::string id = blockRegistry().idOf(BlockId(i));
        namespaced = namespaced && id.rfind("lithokraft:", 0) == 0 && ids.insert(id).second;
      }
      // Frozen ids: saves depend on these strings; they are explicit, not derived from textures.
      const bool frozen = blockRegistry().idOf(kWaferSilicon) == "lithokraft:silicon" && blockRegistry().idOf(kRedTorch) == "lithokraft:red_torch" &&
                          blockRegistry().idOf(kSwitch) == "lithokraft:switch" && blockRegistry().idOf(kButton) == "lithokraft:button" &&
                          blockRegistry().idOf(kLamp) == "lithokraft:lamp";
      check(frozen, "blocks: save ids are explicit and frozen (silicon, red_torch, switch, button, lamp)");
      const bool table = std::filesystem::exists(pdir / "blocks.names");
      std::filesystem::remove(pdir / "blocks.names");
      cDimensionReg r4 = createDimensions();
      Dimension* l = r4.find("wafer");
      l->load(pool, 6);
      std::vector<uint8_t> st2;
      { cWorldSave save(pdir); save.loadDimension("wafer", l->chunks, st2); }
      check(namespaced && table && sameCells(c->chunks, l->chunks),
            "save: block ids are unique 'lithokraft:' names, saves write the name table, and a legacy save (no table) migrates as the current order");
    }
    {  // P0b permanence pack (C-4 / SV-H1 / SV-H2 / SV-H3): the file contract
       // applied to blocks.names + player.lat, fail-closed loads/saves, the
       // persisted seed, and honest flush-failure reporting.
      auto corruptByte = [](const std::filesystem::path& p, std::streamoff at) {
        std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
        f.seekg(at); char ch = 0; f.get(ch); f.seekp(at); f.put(char(ch ^ 0x5A)); f.close();
      };
      auto writeGarbage = [](const std::filesystem::path& p) {
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f << "TORN-TABLE-GARBAGE-0123456789abcdef";
        f.close();
      };
      std::vector<uint8_t> blobP;
      {  // C-4 recovery: torn primary + intact backup generation -> table heals, columns load.
        const auto ndir = dir / "p0b_names_recovery";
        {
          cWorldSave save(ndir);
          save.saveDimension("wafer", wafer->chunks, sig, true);
          save.saveDimension("wafer", wafer->chunks, sig, true);  // second write rotates blocks.names.bak.1
        }
        const bool haveBackup = std::filesystem::exists(ndir / "blocks.names.bak.1");
        writeGarbage(ndir / "blocks.names");
        ChunkMap rec = emptyLike(wafer->chunks);
        int rn = 0;
        { cWorldSave save(ndir); rn = save.loadDimension("wafer", rec, blobP); }
        check(haveBackup && rn == int(wafer->chunks.columns().size()),
              "save (C-4): a torn blocks.names primary heals from the intact backup generation; every column loads");
      }
      {  // C-4 fail closed: torn table, NO intact generation, regions present.
        const auto fdir = dir / "p0b_names_failclosed";
        { cWorldSave save(fdir); save.saveDimension("wafer", wafer->chunks, sig, true); }  // one save: no table backup yet
        writeGarbage(fdir / "blocks.names");
        ChunkMap fc = emptyLike(wafer->chunks);
        int fn = 0;
        bool saveRefused = false;
        {
          cWorldSave save(fdir);
          fn = save.loadDimension("wafer", fc, blobP);
          saveRefused = save.saveDimension("wafer", wafer->chunks, sig, true) == 0 && !save.lastSaveOk();
        }
        std::ifstream tf(fdir / "blocks.names", std::ios::binary);
        const std::string tableAfter((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
        check(fn < 0 && saveRefused && tableAfter.rfind("TORN-TABLE-GARBAGE", 0) == 0,
              "save (C-4): a corrupt blocks.names with no intact generation + regions present fails closed (load refuses, save refuses, table untouched)");
      }
      {  // SV-H3: torn player.lat primary heals from the backup generation.
        const auto pdir2 = dir / "p0b_player";
        PlayerMeta pa{{1.5f, 111.f, -3.f}, 0.5f, -0.25f, "wafer", 1, {kLamp}};
        {
          cWorldSave save(pdir2);
          std::string err;
          save.writePlayer(pa, &err);
          PlayerMeta pb = pa; pb.pos.y = 222.f;
          save.writePlayer(pb, &err);  // rotates pa into player.lat.bak.1
        }
        corruptByte(pdir2 / "player.lat", 20);
        cWorldSave save(pdir2);
        std::string err;
        const auto back = save.readPlayer(&err);
        check(back && back->pos.y == 111.f, "save (SV-H3): a torn player.lat primary heals from the backup generation");
      }
      {  // SV-H2: the seed round-trips through level.json, exactly, as uint64.
        const auto sdir = dir / "p0b_seed";
        const uint64_t kSeed = 0xFFFFFFFFFFFFFFD5ull;  // > 2^53: a JSON double could not hold this exactly
        { cWorldSave save(sdir); save.setSeed(kSeed); save.saveDimension("wafer", wafer->chunks, sig, true); }
        std::string serr;
        const auto seedBack = cWorldSave::readWorldSeed(sdir, &serr);
        check(std::filesystem::exists(sdir / "level.json") && seedBack && *seedBack == kSeed,
              "save (SV-H2): the world seed persists in level.json and reads back exactly (uint64) with no CLI seed");
        std::string aerr;
        check(!cWorldSave::readWorldArea(sdir, &aerr) && aerr.empty(),
              "save (fix run 3): a save with no area (as v1/v2 wrote) reads back nullopt, no error -> configured area stands");
      }
      {  // fix run 3: level.json v3 persists the sim/stream area + tick radius
        const auto adir = dir / "p0b_area";
        cWorldSave::WorldAreaMeta am;
        am.sim = litholattice::AreaShape::Round; am.stream = litholattice::AreaShape::Square; am.tickRadius = 11;
        { cWorldSave save(adir); save.setSeed(7); save.setWorldArea(am); save.saveDimension("wafer", wafer->chunks, sig, true); }
        std::string aerr;
        const auto back = cWorldSave::readWorldArea(adir, &aerr);
        check(back && aerr.empty() && back->sim == am.sim && back->stream == am.stream && back->tickRadius == 11,
              "save (fix run 3): the sim/stream area and tick radius persist in level.json v3 and read back exactly");
        const auto odir = dir / "p0b_area_old";
        std::filesystem::create_directories(odir);
        { std::ofstream f(odir / "level.json", std::ios::binary); f << "{\"saveFormatVersion\": 2, \"seed\": \"5\"}"; }
        std::string oerr;
        check(!cWorldSave::readWorldArea(odir, &oerr) && oerr.empty(),
              "save (fix run 3): a v2 level.json (no area) is backward-compatible: nullopt, no error");
        const auto bdir = dir / "p0b_area_bad";
        std::filesystem::create_directories(bdir);
        { std::ofstream f(bdir / "level.json", std::ios::binary); f << "{\"saveFormatVersion\": 3, \"seed\": \"5\", \"simAreaShape\": \"HEX\", \"streamAreaShape\": \"Diamond\", \"tickRadius\": 6}"; }
        std::string berr;
        const auto bad = cWorldSave::readWorldArea(bdir, &berr);
        check(bad && bad->sim == litholattice::AreaShape::Diamond && bad->stream == litholattice::AreaShape::Diamond && bad->tickRadius == 6 && !berr.empty(),
              "save (fix run 3): an unknown saved shape warns and falls back to diamond; parsing is case-insensitive");
      }
      {  // SV-H1: a failed region flush surfaces as failure, and the pending
         // records kept by cRegionFile land on the retry. Injection: a
         // DIRECTORY at the region file's .tmp path makes writeFileAtomic
         // fail deterministically (even running as root, where chmod-based
         // read-only directories would not).
        const auto wdir = dir / "p0b_flush";
        bool firstOk = false, failedSave = false, retryOk = false;
        {
          cWorldSave save(wdir);
          save.saveDimension("wafer", wafer->chunks, sig, true);
          firstOk = save.lastSaveOk();
          std::filesystem::create_directories(wdir / "wafer" / "r.0.0.llr.tmp");
          save.saveDimension("wafer", wafer->chunks, sig, true);
          failedSave = !save.lastSaveOk();
          std::filesystem::remove_all(wdir / "wafer" / "r.0.0.llr.tmp");
          retryOk = save.flush();
        }
        ChunkMap after = emptyLike(wafer->chunks);
        int loadedAfter = 0;
        { cWorldSave save(wdir); loadedAfter = save.loadDimension("wafer", after, blobP); }
        check(firstOk && failedSave && retryOk && loadedAfter == int(wafer->chunks.columns().size()),
              "save (SV-H1): a failed region flush surfaces as failure (never 'on disk') and the kept pending records land on retry");
      }
    }
    {  // P1-SIGNAL gate (SW-H1 / SW-H2 / B-M3 / C-M4 / B-L7): a circuit built
       // in a column OUTSIDE the initial load square, saved, and reloaded
       // through the streaming seam Ã¢â‚¬â€ cWorldSave::loadColumn + cSignalSim::
       // scanColumn/importState, the exact calls Game::pumpStreaming makes on
       // insertion, plus a cChunkStreamer pass through the real job pool Ã¢â‚¬â€
       // must come back alive with its component memory intact. (The full
       // Game-level loop is NOT driven here: no Game/cSimThread is constructed
       // in this test, so mesh submission and the stop/start seam itself are
       // covered only by the in-game path, not by this check.)
      const auto pdirS = dir / "p1_stream";
      ChunkMap src;
      src.setRegistry(&blockRegistry());
      for (int cc : {0, 20}) {
        auto c = std::make_unique<ChunkColumn>(); c->cx = cc; c->cz = cc; src.insert(std::move(c));
      }
      const int gy = 100;  // circuit plane in column (20,20): x,z in [320,336)
      for (int z = 320; z < 336; ++z) for (int x = 320; x < 336; ++x) src.set(x, gy - 1, z, kWaferSilicon);
      src.set(322, gy, 322, kRedTorch);
      src.set(323, gy, 322, kRedTrace); src.set(324, gy, 322, kRedTrace); src.set(325, gy, 322, kRedTrace);
      src.set(326, gy, 322, kMemor); src.setState(326, gy, 322, makeState(0, 1));  // facing E: back reads the wire run, front drives on
      src.set(327, gy, 322, kRedTrace);
      src.set(328, gy, 322, kLamp);
      cSignalSim srcSim;
      srcSim.scan(src);
      for (int i = 0; i < 6; ++i) srcSim.tick(src, 20, 20, 4, noDirty);
      const auto srcState = srcSim.exportState();
      check(stValue(src.getState(326, gy, 322)) == 62 && src.getState(328, gy, 322) == 1 && !srcState.empty(),
            "p1-stream: source circuit lives before save (memor stored the decayed 62, lamp lit)");
      { cWorldSave save(pdirS); save.saveDimension("wafer", src, srcSim, true); }
      // --- the streaming seam, exactly as pumpStreaming drives it ---
      cWorldSave streamSave(pdirS);
      auto loaded = streamSave.loadColumn("wafer", 20, 20);
      check(loaded.status == cWorldSave::ColumnLoad::Loaded && loaded.column && !loaded.signal.empty(),
            "p1-stream (SW-H1/C-M4): loadColumn returns the column AND its per-column SGNL payload");
      const auto neverSaved = streamSave.loadColumn("wafer", 21, 21);
      check(neverSaved.status == cWorldSave::ColumnLoad::NotSaved && !neverSaved.column,
            "p1-stream (B-M3): a never-saved column reports NotSaved (generate fresh), distinct from unreadable");
      ChunkMap dst;
      dst.setRegistry(&blockRegistry());
      dst.insert(std::move(loaded.column));
      cSignalSim dstSim;
      dstSim.scanColumn(dst, 20, 20);
      const bool impOk = dstSim.importState(loaded.signal);
      check(impOk && dstSim.exportState() == srcState,
            "p1-stream (SW-H1): scanColumn + importState restores the streamed column's component memory bit for bit");
      for (int i = 0; i < 6; ++i) dstSim.tick(dst, 20, 20, 4, noDirty);
      check(dst.getState(328, gy, 322) == 1 && stValue(dst.getState(326, gy, 322)) == 62,
            "p1-stream (SW-H1): the streamed circuit is ALIVE after reload (lamp lit, memor still holds 62)");
      // t46u item 6b-3b: the same disk column through the REAL tick-thread seam. pumpStreaming now posts
      // scan + import to the tick thread (postColumnSignalInit); every save that follows at once must still
      // see the restored component memory: the async save's boundary-job snapshot, the pre-evict save
      // (which waits on the column's ticket), and the sync save (stop() runs the posts left over). Run with
      // the sim ticking and with it paused (parked): the posts must not wait out the park.
      for (const bool pausedRun : {false, true}) {
        const std::string tag = pausedRun ? " (sim paused)" : " (sim running)";
        ChunkMap tw;
        tw.setRegistry(&blockRegistry());
        auto ws = std::make_unique<cSignalWorldSim>();
        cSignalWorldSim* wsp = ws.get();
        litholattice::SimConfig tcfg;
        tcfg.tickRate = 64;
        tcfg.radiusChunks = 4;  // around chunk (0,0): column (20,20) is loaded but outside the tick area (state stays put)
        litholattice::cSimThread st(tw, blockRegistry(), std::move(ws), std::move(tcfg), [](std::shared_ptr<litholattice::MeshInput>) {});
        st.setPaused(pausedRun);
        st.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));  // running: a few passes; paused: parked
        auto again = streamSave.loadColumn("wafer", 20, 20);
        const std::vector<uint8_t> blob = again.signal;
        uint64_t ticket = 0;
        bool held = false, queued = false;
        {  // as pumpStreaming: insert under the between-ticks hold, publish, post the signal init
          litholattice::cGateLock<litholattice::cBlockGate> hold;
          held = st.holdBetweenTicks(hold) && again.column != nullptr;
          if (held) {
            tw.insert(std::move(again.column));
            tw.publishDirectory();
            ticket = postColumnSignalInit(st, wsp->signal(), tw, 20, 20, std::move(again.signal));
            queued = !st.tickPostDone(ticket);  // the tick thread cannot run it while we hold the world
          }
        }
        check(held && queued, ("6b-3b tick seam" + tag + ": the disk column's scan + import is queued for the tick thread, not run here").c_str());
        // (1) async save path: a boundary-job snapshot posted at once sees the restored memory.
        std::atomic<bool> snapped{false};
        std::map<uint64_t, std::vector<uint8_t>> snapState;
        const bool posted = st.postBoundaryJob([&] { snapState = wsp->signal().exportState(); snapped.store(true); });
        for (int i = 0; i < 2000 && posted && !snapped.load(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(posted && snapped.load() && snapState == srcState,
              ("6b-3b tick seam" + tag + ": an immediate async-save snapshot (boundary job) has the restored component memory").c_str());
        // (2) pre-evict save path: the ticket gates the read; once done, the saved column round-trips its circuit state.
        cSignalInitTickets tickets;
        tickets.note(litholattice::ChunkKey::make(20, 20), ticket);
        for (int i = 0; i < 2000 && tickets.pending(st, litholattice::ChunkKey::make(20, 20)); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        const bool initDone = !tickets.pending(st, litholattice::ChunkKey::make(20, 20));
        bool evSaved = false;
        const auto evDir = dir / (pausedRun ? "p6b3b_evict_paused" : "p6b3b_evict_running");
        {
          litholattice::cGateLock<litholattice::cBlockGate> hold;
          if (initDone && st.holdBetweenTicks(hold)) {
            cWorldSave evSave(evDir);
            evSaved = evSave.saveColumns("wafer", {tw.column(20, 20)}, wsp->signal(), /*wait=*/true) == 1 && evSave.lastSaveOk();
          }
        }
        bool evRoundTrip = false;
        if (evSaved) {
          cWorldSave back(evDir);
          auto rl = back.loadColumn("wafer", 20, 20);
          if (rl.status == cWorldSave::ColumnLoad::Loaded && rl.column && !rl.signal.empty()) {
            ChunkMap bm;
            bm.setRegistry(&blockRegistry());
            bm.insert(std::move(rl.column));
            cSignalSim bs;
            bs.scanColumn(bm, 20, 20);
            evRoundTrip = bs.importState(rl.signal) && bs.exportState() == srcState;
          }
        }
        check(initDone && evSaved && evRoundTrip,
              ("6b-3b tick seam" + tag + ": the pre-evict save waits for the init, then saves the restored circuit state (round-trips bit for bit)").c_str());
        // (2b) t46u 6b-4a': the evict/demote pumps' non-blocking signal-bearing query. While the answer is not
        // back the key is not covered (the pump skips it: a pending query never evicts); polling the same set
        // never re-asks; a changed set asks only for the new key; a key that left and came back is asked again.
        {
          using litholattice::ChunkKey;
          const uint64_t k20 = ChunkKey::make(20, 20), k21 = ChunkKey::make(21, 21);
          cSignalBearingQuery q(/*withExport=*/true);
          bool pendingSkips = false;
          {
            litholattice::cGateLock<litholattice::cBlockGate> hold;  // the tick side cannot answer while we hold
            if (st.holdBetweenTicks(hold)) {
              const auto& a0 = q.poll(st, wsp->signal(), {k20});
              const auto& a1 = q.poll(st, wsp->signal(), {k20});
              pendingSkips = !a0.covers(k20) && !a1.covers(k20) && q.inFlight() && q.asks() == 1;
            }
          }
          check(pendingSkips, ("6b-4a' query" + tag + ": a pending query covers nothing (the pump evicts none of its keys) and is not re-posted").c_str());
          bool answered = false;
          for (int i = 0; i < 2000 && !answered; ++i) {
            answered = q.poll(st, wsp->signal(), {k20}).covers(k20);
            if (!answered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          for (int i = 0; i < 50; ++i) q.poll(st, wsp->signal(), {k20});  // a permanently refused candidate, polled every frame
          const auto& a = q.poll(st, wsp->signal(), {k20});
          check(answered && a.bearing.count(k20) && a.exported == srcState && q.asks() == 1,
                ("6b-4a' query" + tag + ": the answer marks the column signal-bearing with its restored SGNL bytes; the same set is never re-asked").c_str());
          const bool newKeyPending = !q.poll(st, wsp->signal(), {k20, k21}).covers(k21);
          bool k21Answered = false;
          for (int i = 0; i < 2000 && !k21Answered; ++i) {
            k21Answered = q.poll(st, wsp->signal(), {k20, k21}).covers(k21);
            if (!k21Answered) std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          const auto& b = q.poll(st, wsp->signal(), {k21, k20});
          check(newKeyPending && k21Answered && q.asks() == 2 && b.covers(k20) && !b.bearing.count(k21),
                ("6b-4a' query" + tag + ": a changed candidate set re-queries (only the new key, which is not evicted until answered)").c_str());
          q.poll(st, wsp->signal(), {k21});  // 20,20 leaves the set (back in range, or evicted)...
          const bool reasked = !q.poll(st, wsp->signal(), {k20, k21}).covers(k20) && q.asks() == 3;
          check(reasked, ("6b-4a' query" + tag + ": a key that left the candidate set loses its answer and is asked again").c_str());
        }
        // (3) sync save path: drop + re-init posted, then stop() at once (as saveWorld does): stop runs them in order.
        postColumnSignalDrop(st, wsp->signal(), 20, 20);
        const uint64_t t2 = postColumnSignalInit(st, wsp->signal(), tw, 20, 20, blob);
        st.stop();
        check(st.tickPostDone(t2) && wsp->signal().exportState() == srcState,
              ("6b-3b tick seam" + tag + ": a sync save right after the posts (sim stopped) sees the restored circuit state").c_str());
      }
      {  // Dimension path on the SAME cWorldSave instance, after streamed loads.
        ChunkMap dimMap = emptyLike(src);
        std::vector<uint8_t> blob;
        const int dn = streamSave.loadDimension("wafer", dimMap, blob);
        cSignalSim dimSim;
        dimSim.scan(dimMap);
        check(dn == 2 && dimSim.importState(blob) && dimSim.exportState() == srcState,
              "p1-stream (C-M4): loadDimension on the same instance after streamed loads returns exactly the dimension's state (no residue, no cross-path sweep)");
      }
      // SW-H2: a hand-edited wire value of 100 in the streamed column clamps
      // TO 64 at the component input Ã¢â‚¬â€ the memor stores 64, never 100, and no
      // out-of-range output is produced (the pre-fix code indexed buckets_[100]).
      dstSim.setSameTick(false);  // PlayM: injected (undriven) wire value; a same-tick settle would re-flood it to 0
      dst.setState(325, gy, 322, CellState(100));
      dstSim.invalidateNetwork();
      dstSim.tick(dst, 20, 20, 4, noDirty);
      check(stValue(dst.getState(326, gy, 322)) == 64,
            "p1-stream (SW-H2): a disk-loaded wire value of 100 is clamped TO 64 at the component input (memor stores 64)");
      {  // B-M3: corrupt name table => Unreadable per column on every call
         // (fail closed, no session latch, never mistaken for "never saved").
        const auto tdirS = dir / "p1_stream_corrupt";
        { cWorldSave save(tdirS); save.saveDimension("wafer", src, srcSim, true); }  // one save: no table backup yet
        { std::ofstream f(tdirS / "blocks.names", std::ios::binary | std::ios::trunc); f << "TORN-TABLE-GARBAGE-0123456789abcdef"; }
        cWorldSave corruptSave(tdirS);
        const auto bad1 = corruptSave.loadColumn("wafer", 20, 20);
        const auto bad2 = corruptSave.loadColumn("wafer", 20, 20);
        check(bad1.status == cWorldSave::ColumnLoad::Unreadable && bad2.status == cWorldSave::ColumnLoad::Unreadable && !bad1.column,
              "p1-stream (B-M3): corrupt name table => Unreadable on every call (fail closed per column, no latch, never 'never saved')");
        // cChunkStreamer integration: the healthy path delivers disk + SGNL
        // through the real job pool (SW-H3 guard on its normal exit), and an
        // unreadable column is neither delivered nor generated over, with
        // its key un-requested for a backed-off retry (B-L7).
        cChunkStreamer good(pool, wafer->generator, &streamSave, "wafer", 0, {});
        good.update(20, 20);
        bool got = false;
        for (int i = 0; i < 500 && !got; ++i) { got = good.hasReady(); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
        auto readyCols = good.takeReady();
        check(got && readyCols.size() == 1 && readyCols[0].fromDisk && readyCols[0].column && readyCols[0].column->cx == 20 && !readyCols[0].signal.empty(),
              "p1-stream (SW-H1/H3): cChunkStreamer delivers the disk column with its SGNL payload through the job pool");
        cChunkStreamer bad(pool, wafer->generator, &corruptSave, "wafer", 0, {});
        bad.update(20, 20);
        bool erased = false;
        for (int i = 0; i < 500 && !erased; ++i) {
          bad.update(20, 20);
          erased = bad.streamedCount() == 0 && !bad.hasReady();
          if (!erased) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(erased, "p1-stream (B-M3/B-L7): an unreadable streamed column is not delivered, not generated over, and its key is un-requested for a backed-off retry");
      }
      {  // B-M2 (P3-2): takeReadyUpTo bounds every drain Ã¢â‚¬â€ a burst of
         // finished columns can no longer land in one insertion window.
        cChunkStreamer many(pool, wafer->generator, nullptr, "wafer", 1, {});
        many.update(40, 40);  // 3x3 around chunk (40,40), generate-only
        size_t total = 0;
        bool bounded = true;
        for (int i = 0; i < 1000 && total < 9; ++i) {
          auto batch = many.takeReadyUpTo(3);
          if (batch.size() > 3) bounded = false;
          total += batch.size();
          if (batch.empty()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        check(bounded && total == 9,
              "stream (B-M2): takeReadyUpTo(3) never returns more than 3, and all 9 requested columns arrive across successive pumps");
      }
    }
    std::filesystem::remove_all(dir);
  }
  {  // P3-2 (World/save tail): B-L2 packPos range, B-L5 cLightEngine clamp,
     // B-M4 trace-strength keys, C-M3 region size cap, C-L2 .torn rotation,
     // C-L3 cAnvilRegion quarantine count.
    using namespace litholattice;
    // B-L2: packPos round-trips inside its range and ALIASES outside it
    // (the alias pair is the documented hazard the bounds protect against).
    int rx = 0, ry = 0, rz = 0;
    unpackPos(packPos(-300000, 767, 262143), rx, ry, rz);
    check(packPosInRange(-300000, 767, 262143) && rx == -300000 && ry == 767 && rz == 262143 &&
              !packPosInRange(0, 0, 1 << 20) && packPos(0, 0, 1 << 20) == packPos(1, 0, -(1 << 20)),
          "packPos (B-L2): in-range coordinates round-trip exactly; z = +2^20 is out of range and aliases onto (x=1, z=-2^20)");
    // B-L5: an over-range source level clamps to kMaxLight.
    cLightEngine le(16, 16, 16);
    le.addSource(8, 8, 8, 200);
    le.propagate();
    check(le.getLight(8, 8, 8) == kMaxLight, "light (B-L5): addSource level 200 clamps to kMaxLight (64)");
    // B-M4: strength slots are keyed by WORLD packPos end to end, and
    // "no slot" is a sentinel distinct from the real slot 0.
    ChunkMap tm;
    tm.setRegistry(&blockRegistry());
    { auto c = std::make_unique<ChunkColumn>(); c->cx = 3; c->cz = 3; tm.insert(std::move(c)); }
    for (int z = 48; z < 64; ++z) for (int x = 48; x < 64; ++x) tm.set(x, 99, z, kWaferSilicon);
    tm.set(50, 100, 50, kRedTorch);
    tm.set(51, 100, 50, kRedTrace);
    tm.set(52, 100, 50, kRedTrace);
    tm.set(53, 100, 50, kLamp);
    cSignalSim ts;
    ts.scan(tm);
    for (int i = 0; i < 6; ++i) ts.tick(tm, 3, 3, 4, [](int, int, int) {});
    auto& tbuf = ts.strengthBuffer();
    const int64_t wireKey = packPos(51, 100, 50);
    const int wireVal = stValue(tm.getState(51, 100, 50));
    check(wireVal > 0 && tbuf.slotOf(wireKey) != TraceStrengthBuffer::kNoSlot &&
              tbuf.get(tbuf.slotOf(wireKey)) == wireVal,
          "trace strengths (B-M4): the full pass records the wire's final strength under its WORLD packPos key");
    check(TraceStrengthBuffer::kNoSlot != 0 && tbuf.slotOf(packPos(67, 100, 50)) == TraceStrengthBuffer::kNoSlot,
          "trace strengths (B-M4): the same local coords in another chunk have NO slot (world keying), and kNoSlot != real slot 0");
    const auto pdir = uniqueTempDir("lithokraft_p32_selftest");
    std::filesystem::create_directories(pdir);
    {  // C-M3: a region file past the 64 MiB cap is refused at open.
      const auto rp = pdir / "r.0.0.llr";
      { storage::cRegionFile rf(rp); std::string e; rf.open(&e); rf.put(0, 0, storage::Bytes{1, 2, 3}); rf.flush(&e); }
      std::filesystem::resize_file(rp, storage::cRegionFile::kMaxFileBytes + 1);
      storage::cRegionFile rf2(rp);
      std::string e2;
      check(!rf2.open(&e2) && e2.find("cap") != std::string::npos,
            "storage (C-M3): a region file over the 64 MiB cap is refused at open, before any whole-file read");
    }
    {  // C-L2: recovering twice never destroys the first .torn evidence.
      const auto tp = pdir / "torn.bin";
      auto validate = [](const std::filesystem::path& q, std::string* e) {
        storage::Bytes b;
        return storage::readFileAll(q, &b, e) && b.size() == 1;
      };
      auto readAll = [&](const std::filesystem::path& q) { storage::Bytes b; std::string e; storage::readFileAll(q, &b, &e); return b; };
      std::string e;
      storage::writeFileAtomic(tp, storage::Bytes{1}, &e);
      storage::writeFileAtomic(tp, storage::Bytes{2}, &e);  // .bak.1 = {1}
      { std::ofstream f(tp, std::ios::binary | std::ios::trunc); f << "GG"; }
      const bool r1 = storage::recoverCrashedWrite(tp, validate, &e);
      { std::ofstream f(tp, std::ios::binary | std::ios::trunc); f << "HH"; }
      const bool r2 = storage::recoverCrashedWrite(tp, validate, &e);
      check(r1 && r2 && readAll(tp) == storage::Bytes{1} && readAll(tp.string() + ".torn") == storage::Bytes{'G', 'G'} &&
                readAll(tp.string() + ".torn.1") == storage::Bytes{'H', 'H'},
            "storage (C-L2): two recoveries keep BOTH torn primaries (.torn and .torn.1); the first evidence is never overwritten");
    }
    {  // C-L3: a malformed cAnvilRegion index entry is quarantined + counted.
      const auto ap = pdir / "r.0.0.llar";
      size_t before = 0;
      { storage::cAnvilRegion ar(ap, 0, 0); std::string e; ar.open(&e); ar.put(1, 1, storage::cAnvilRegion::kCodecNone, storage::Bytes{7, 7, 7}, &e); before = ar.count(); }
      {  // entry 7 (lx=7, lz=0): nonzero offset with a zero count = malformed
        std::fstream f(ap, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(16 + 4 * 7);
        const char bad[4] = {0, 0, 5, 0};
        f.write(bad, 4);
      }
      storage::cAnvilRegion ar2(ap, 0, 0);
      std::string e;
      check(ar2.open(&e) && ar2.quarantinedEntryCount() == 1 && ar2.count() == before,
            "storage (C-L3): a malformed cAnvilRegion index entry is normalised to absent, logged and counted (not silent)");
    }
    std::filesystem::remove_all(pdir);
  }
  {  // A2 (B-M1 column eviction, 2026-10-13): the production seams composed
     // exactly as Game::evictFarColumns drives them Ã¢â‚¬â€ SGNL export before
     // unscan, save-on-evict through cWorldSave::saveColumns, ChunkMap::erase,
     // cChunkStreamer::forgetColumn, and selectEvictionCandidates bounding.
     // (The full Game loop is not constructed here; mesh retire is a
     // renderer-queue operation covered by the empty-verts contract.)
    using namespace litholattice;
    const auto evNoDirty = [](int, int, int) {};
    const auto edir = uniqueTempDir("lithokraft_a2_evict_selftest");
    std::filesystem::remove_all(edir);
    ChunkMap ew;
    ew.setRegistry(&blockRegistry());
    for (int cc : {20, 21}) {
      auto c = std::make_unique<ChunkColumn>(); c->cx = cc; c->cz = 20; ew.insert(std::move(c));
    }
    check(!ew.column(20, 20)->dirty && ew.column(20, 20)->generation == 0,
          "evict (A2): a freshly inserted column starts clean (dirty=false, generation=0)");
    const int gy = 100;
    for (int z = 320; z < 336; ++z) for (int x = 320; x < 336; ++x) ew.set(x, gy - 1, z, kWaferSilicon);
    ew.set(322, gy, 322, kRedTorch);
    ew.set(323, gy, 322, kRedTrace); ew.set(324, gy, 322, kRedTrace);
    ew.set(325, gy, 322, kMemor); ew.setState(325, gy, 322, makeState(0, 1));
    ew.set(326, gy, 322, kRedTrace);
    ew.set(327, gy, 322, kLamp);
    // A plain neighbour column in (21,20): terrain only, then left clean by
    // direct column construction (mutations below bypass the map on purpose:
    // worldgen writes columns directly, never through ChunkMap::set).
    {
      ChunkColumn plain; plain.cx = 21; plain.cz = 20;
      // Rebuild via the map so the column exists, then reset its flags to the
      // worldgen state (clean) Ã¢â‚¬â€ the fixture's terrain is seed-equivalent.
      ew.set(340, gy - 1, 322, kWaferSilicon);
      ew.column(21, 20)->dirty = false; ew.column(21, 20)->generation = 0;
    }
    check(ew.column(20, 20)->dirty && ew.column(20, 20)->generation > 0,
          "evict (A2): ChunkMap::set marks the resident column dirty and bumps its generation");
    cSignalSim esim;
    esim.scan(ew);
    for (int i = 0; i < 6; ++i) esim.tick(ew, 20, 20, 4, evNoDirty);
    check(esim.hasIndexedCells(20, 20) && !esim.exportColumnState(20, 20).empty(),
          "evict (A2): the circuit column is sim-indexed and exports per-column SGNL state");
    const uint64_t hashBefore = hashControlWorld(ew, esim, 0);
    const auto stateBefore = esim.exportState();
    // --- dirty eviction: save-on-evict, then unscan + erase ---------------
    {
      cWorldSave save(edir);
      const size_t saved = save.saveColumns("wafer", {ew.column(20, 20)}, esim, true);
      check(saved == 1 && save.lastSaveOk(),
            "evict (A2): saveColumns persists a dirty column (blocks + SGNL) before eviction");
    }
    esim.unscanColumn(20, 20);
    check(!esim.hasIndexedCells(20, 20) && esim.exportState().empty(),
          "evict (A2): unscanColumn leaves NO ghost components (export is empty for the evicted column)");
    check(ew.erase(20, 20) && !ew.contains(20, 20) && !ew.erase(20, 20),
          "evict (A2): ChunkMap::erase removes the column exactly once");
    for (int i = 0; i < 6; ++i) esim.tick(ew, 20, 20, 4, evNoDirty);
    check(esim.exportState().empty() && esim.componentCount() == 0,
          "evict (A2): ticking after eviction resurrects nothing (no ghost sim state)");
    // --- reload: the evicted column comes back bit-exact ------------------
    ChunkMap reloaded;
    reloaded.setRegistry(&blockRegistry());
    {
      auto c = std::make_unique<ChunkColumn>(); c->cx = 21; c->cz = 20; reloaded.insert(std::move(c));
      // Column (21,20) reloads as regenerated terrain (clean eviction path:
      // never saved; the fixture terrain is reproduced directly).
      reloaded.set(340, gy - 1, 322, kWaferSilicon);
      reloaded.column(21, 20)->dirty = false; reloaded.column(21, 20)->generation = 0;
    }
    cWorldSave reloadSave(edir);
    auto loadedCol = reloadSave.loadColumn("wafer", 20, 20);
    check(loadedCol.status == cWorldSave::ColumnLoad::Loaded && loadedCol.column && !loadedCol.signal.empty(),
          "evict (A2): loadColumn returns the evicted column with its SGNL payload");
    reloaded.insert(std::move(loadedCol.column));
    // Production scans per column on insertion; import the evicted
    // column's payload after its scanColumn, as pumpStreaming does.
    cSignalSim rsim2;
    rsim2.scanColumn(reloaded, 20, 20);
    const bool impOk = rsim2.importState(loadedCol.signal);
    check(impOk && rsim2.exportState() == stateBefore,
          "evict (A2): generate->mutate->evict->reload restores SGNL state bit for bit");
    check(hashControlWorld(reloaded, rsim2, 0) == hashBefore,
          "evict (A2): generate->mutate->evict->reload produces a bit-exact world hash (blocks + states + SGNL)");
    // --- clean eviction: no save needed, reload equality ------------------
    ChunkMap cleanMap;
    cleanMap.setRegistry(&blockRegistry());
    { auto c = std::make_unique<ChunkColumn>(); c->cx = 30; c->cz = 30; cleanMap.insert(std::move(c)); }
    cleanMap.set(481, 90, 481, kStone);  // fixture edit, then restore clean flags (worldgen-equivalent)
    cleanMap.column(30, 30)->dirty = false; cleanMap.column(30, 30)->generation = 0;
    cSignalSim csim;
    csim.scan(cleanMap);
    const uint64_t cleanHash = hashControlWorld(cleanMap, csim, 0);
    check(!cleanMap.column(30, 30)->dirty && !csim.hasIndexedCells(30, 30) && cleanMap.erase(30, 30),
          "evict (A2): a clean, signal-free column evicts with no save (fast path)");
    ChunkMap cleanBack;
    cleanBack.setRegistry(&blockRegistry());
    { auto c = std::make_unique<ChunkColumn>(); c->cx = 30; c->cz = 30; cleanBack.insert(std::move(c)); }
    cleanBack.set(481, 90, 481, kStone);
    cleanBack.column(30, 30)->dirty = false; cleanBack.column(30, 30)->generation = 0;
    cSignalSim csim2;
    csim2.scan(cleanBack);
    check(hashControlWorld(cleanBack, csim2, 0) == cleanHash,
          "evict (A2): clean column evict/reload equality (regenerated column hashes identically)");
    // --- bounded residency: candidate selection + forget + erase ----------
    {
      std::vector<uint64_t> keys;
      ChunkMap big;
      big.setRegistry(&blockRegistry());
      for (int cz = -20; cz <= 20; ++cz) for (int cx = -20; cx <= 20; ++cx) {
        auto c = std::make_unique<ChunkColumn>(); c->cx = cx; c->cz = cz; big.insert(std::move(c));
        keys.push_back(ChunkKey::make(cx, cz));
      }
      // cPlayer at (0,0), stream radius 8, hysteresis +2 (JD-DECISION values):
      // resident count must converge to the (2*10+1)^2 = 441 in-band columns.
      size_t guard = 0, maxBatch = 0;
      while (big.columns().size() > 441 && guard++ < 500) {
        std::vector<uint64_t> cur;
        for (const auto& [k, col] : big.columns()) cur.push_back(k);
        const auto cands = selectEvictionCandidates(cur, 0, 0, 8, 2, 4);
        if (cands.empty()) break;
        maxBatch = std::max(maxBatch, cands.size());
        for (uint64_t k : cands) big.erase(int(int32_t(k >> 32)), int(int32_t(k & 0xffffffffu)));
      }
      check(maxBatch <= 4, "evict (A2): eviction is bounded per pump (budget 4)");
      check(big.columns().size() == 441,
            "evict (A2): resident column count stays bounded (1681 explored -> 441 within radius+hysteresis)");
      const auto none = selectEvictionCandidates(keys, 0, 0, 100, 2, 4);
      check(none.empty(), "evict (A2): no candidates inside the hysteresis band (no boundary thrash)");
      // forgetColumn makes an evicted column requestable again.
      cChunkStreamer fs(pool, wafer->generator, nullptr, "wafer", 0, {ChunkKey::make(50, 50)});
      fs.forgetColumn(50, 50);
      check(fs.requestedCount() == 0, "evict (A2): cChunkStreamer::forgetColumn removes the requested_ entry (re-streamable)");
      {  // fix 3b (JD t2279u): requests go nearest-first by diamond distance, ties deterministic
        std::vector<std::pair<int, uint64_t>> order;
        for (int dz = -6; dz <= 6; ++dz)
          for (int dx = -6; dx <= 6; ++dx)
            if (std::abs(dx) + std::abs(dz) <= 6) order.emplace_back(streamRequestPriority(dx, dz, true), ChunkKey::make(dx, dz));
        std::sort(order.begin(), order.end());
        bool mono = true;
        int prev = -1;
        for (const auto& [p, k] : order) {
          const int d = std::abs(int(int32_t(k >> 32))) + std::abs(int(int32_t(k & 0xffffffffu)));
          if (d < prev) mono = false;
          prev = d;
        }
        check(order.front().second == ChunkKey::make(0, 0) && mono,
              "stream order: diamond requests go nearest-first by |dx|+|dz| from the player chunk");
        check(streamRequestPriority(3, 0, true) < streamRequestPriority(2, 2, true),
              "stream order: diamond distance ranks (3,0) before (2,2)");
        check(streamRequestPriority(2, 2, false) < streamRequestPriority(3, 0, false),
              "stream order: square/round streams keep Euclidean nearest-first");
        auto again = order;
        std::reverse(again.begin(), again.end());
        std::sort(again.begin(), again.end());
        check(again == order, "stream order: ties break by chunk key (deterministic, input-order independent)");
      }
      {  // R3 (B7, 2026-11-17): one rank for requests, promotion, view-gate entry, first mesh and upload
        const cStreamRank r(0, 0, cStreamRank::sectorOf(0.f), true);  // looking +x
        check(r.column(5, 0) < r.column(-5, 0), "stream rank: a column in front ranks before one equally far behind");
        check(r.column(9, 0) < r.column(-5, 0) && r.column(-4, 0) < r.column(9, 0),
              "stream rank: behind counts as twice as far (front 9 before behind 5, behind 4 before front 9)");
        check(r.column(7, 0) < r.column(0, 5) && r.column(0, 5) < r.column(-4, 0), "stream rank: front, then side, then behind");
        check(r.column(-1, 0) < r.column(2, 0), "stream rank: the ring around the player comes first, in any direction");
        check(cStreamRank::sectorOf(-1.5708f) == 12 && cStreamRank(0, 0, 12, true).column(0, -5) < cStreamRank(0, 0, 12, true).column(0, 5),
              "stream rank: yaw -pi/2 faces -z (cPlayer::lookDir convention)");
        check(cStreamRank::sectorOf(6.2831853f) == 0 && cStreamRank::sectorOf(-0.1f) == 0 && cStreamRank::sectorOf(0.3f) == 1,
              "stream rank: yaw quantizes to 16 sectors, wrapping");
        check(r.section(1, 4, 0, 4) < r.section(0, 15, 0, 4), "stream rank: uploads put the ground of a near column before the sky above you");
        const cStreamRank nodir(0, 0, cStreamRank::kNoDirection, true);
        check(nodir.column(5, 0).primary == nodir.column(0, 5).primary && nodir.column(-5, 0).primary == nodir.column(5, 0).primary,
              "stream rank: without a direction only distance and the key decide");
        std::vector<std::pair<cStreamRank::Key, uint64_t>> a, b;
        for (int dz = -8; dz <= 8; ++dz)
          for (int dx = -8; dx <= 8; ++dx)
            if (std::abs(dx) + std::abs(dz) <= 8) a.emplace_back(r.column(dx, dz), ChunkKey::make(dx, dz));
        b.assign(a.rbegin(), a.rend());
        std::sort(a.begin(), a.end());
        std::sort(b.begin(), b.end());
        bool unique = true;
        for (size_t i = 1; i < a.size(); ++i) if (!(a[i - 1].first < a[i].first)) unique = false;
        check(a == b && unique, "stream rank: a strict total order (no ties), the same from any input order");
      }
      {  // R4 (B8, 2026-11-17): view-gate hysteresis and mesh handover
        lithokraft::cViewMeshGate g;
        using St = lithokraft::cViewMeshGate::Step;
        if (g.enabled()) {
          g.setCentre(0, 0, 4, false);
          check(!g.inView(5, 0) && g.inKeep(5, 0) && !g.inKeep(6, 0) && g.inKeep(4, 4) && !g.inKeep(6, 1),
                "view gate: keep band is one ring past the view");
          // Model one meshed column at the view edge while the player strafes across the gate line.
          bool drawn = true; int tombstones = 0, remeshes = 0, holes = 0;
          auto sweep = [&] {
            switch (g.step(4, 0)) {
              case St::Stay: break;
              case St::Keep: g.keep(4, 0); break;
              case St::Retire: drawn = false; ++tombstones; g.retired(4, 0); break;
              case St::Enter: g.undefer(4, 0); ++remeshes; drawn = true; break;  // old mesh (if any) up until the new one
            }
            if (g.inView(4, 0) && !drawn) ++holes;
          };
          for (int i = 0; i < 10; ++i) { g.setCentre(i % 2 ? -1 : 0, 0, 4, false); sweep(); }
          check(tombstones == 0 && holes == 0 && drawn && remeshes == 4,
                "view gate: strafing across the edge never retires (no tombstones, no holes); it re-meshes on return with handover");
          g.setCentre(-1, 0, 4, false); sweep();
          check(g.kept(4, 0) && g.deferred(4, 0) && drawn, "view gate: one ring out: kept drawn, deferred (no new meshes)");
          g.setCentre(-2, 0, 4, false); sweep();
          check(!g.kept(4, 0) && g.deferred(4, 0) && !drawn && tombstones == 1, "view gate: past the band: retired once, still deferred");
          g.setCentre(-1, 0, 4, false); sweep();
          check(!drawn && g.step(4, 0) == St::Stay, "view gate: a retired column back in the band stays unmeshed (no meshing beyond the view)");
          g.setCentre(0, 0, 4, false); sweep();
          check(drawn && !g.deferred(4, 0) && !g.kept(4, 0), "view gate: back in view it meshes again");
          g.defer(6, 0);
          g.setCentre(1, 0, 4, false);
          check(g.step(6, 0) == St::Stay, "view gate: a never-meshed column in the band is not meshed (view radius unchanged)");
          g.setCentre(-1, 0, 4, false); sweep(); g.setCentre(10, 0, 4, false);
          check(g.step(4, 0) == St::Retire, "view gate: a big jump retires a kept column directly");
          g.keep(7, 7); g.forget(7, 7);
          check(!g.kept(7, 7) && !g.deferred(7, 7), "view gate: forget clears kept too");
          g.keep(7, 7); g.clear();
          check(g.keptCount() == 0 && g.deferredCount() == 0 && g.inView(99, 99), "view gate: clear empties both sets and un-centres");
        }
      }
      {  // R4 (B5/B6, 2026-11-17): stream budgets priced against the frame target, not 60 Hz
        using lithokraft::cFrameBudget;
        check(std::abs(cFrameBudget::targetMsFor(30) - 33.333) < 0.01 && std::abs(cFrameBudget::targetMsFor(0) - 33.333) < 0.01 &&
                  std::abs(cFrameBudget::targetMsFor(60) - 16.667) < 0.01,
              "frame budget: target 1000/cap; uncapped prices at the 30 FPS play target");
        check(std::abs(cFrameBudget::busyMs(33.3, 30.0) - 3.3) < 1e-9 && cFrameBudget::busyMs(10.0, 15.0) == 0.0 && cFrameBudget::busyMs(10.0, -1.0) == 10.0,
              "frame budget: busy = frame minus the pacer's sleep, never negative");
        const double t30 = cFrameBudget::targetMsFor(30);
        check(cFrameBudget::requestGrant(3.0, t30) == 10 && cFrameBudget::requestGrant(45.0, t30) == 1 && cFrameBudget::requestGrant(35.0, t30) == 2,
              "frame budget: a 3 ms frame at 30 FPS gets 10 new columns (was 2), 20 % over gets 1, just over gets 2");
        bool mono = true, floor1 = true;
        for (int i = 0, prev = 1 << 20; i <= 600; ++i) {
          const int g = cFrameBudget::requestGrant(i * 0.1, t30);
          if (g > prev) mono = false;
          if (g < 1 || g > cFrameBudget::kMaxGrant) floor1 = false;
          prev = g;
        }
        check(mono && floor1, "frame budget: the grant never rises as the frame gets busier, and stays in [1, 10]");
        check(cFrameBudget::requestGrant(std::nan(""), t30) == 2 && cFrameBudget::requestGrant(5.0, 0.0) == cFrameBudget::requestGrant(5.0, t30),
              "frame budget: NaN busy prices as a full frame; a zero target falls back to 30 FPS");
        check(std::abs(cFrameBudget::uploadMsBudget(cFrameBudget::targetMsFor(60)) - 1.5) < 0.01 &&
                  std::abs(cFrameBudget::uploadMsBudget(t30) - 3.0) < 0.01,
              "frame budget: upload commit time is 9 % of the frame (1.5 ms at 60 FPS, 3 ms at 30)");
        check(litholattice::cSlackHarvester::slicesFor(t30, cFrameBudget::busyMs(33.3, 28.0), 2.0, 0.25, 6) == 6 &&
                  litholattice::cSlackHarvester::slicesFor(1000.0 / 60.0, 33.3, 2.0, 0.25, 6) == 0,
              "frame budget: a 30 FPS frame that slept 28 ms has border slack (the old 60 Hz pricing had none)");
      }
    }
    std::filesystem::remove_all(edir);
  }
  {  // Dropped items (P-1..P-6, 2026-11-17): merge, pickup reach, the floating pose, the save record, physics
    using namespace lithokraft::itemdrop;
    const SameIdentityFn same = [](uint64_t, uint64_t) { return true; };
    {
      std::vector<MergeCandidate> c(4);
      c[0] = {1, 0.0, 64.0, 0.0, 7, 5, 64, false, false};
      c[1] = {2, 0.6, 64.0, 0.0, 7, 10, 64, false, false};
      c[2] = {3, 3.0, 64.0, 0.0, 7, 10, 64, false, false};  // too far
      c[3] = {4, 0.3, 64.0, 0.3, 8, 10, 64, false, false};  // different item
      const auto p = planMerges(c, 0, kMergeRadius, same, true);
      check(p.size() == 1 && p[0] == MergePair{2, 1}, "item drops: stacks within a block merge into the bigger one; far or different items don't");
      c[0].count = 60;
      const auto q = planMerges(c, 0, kMergeRadius, same, true);
      check(q.empty(), "item drops: a merge never goes over the stack cap");
    }
    check(inPickupReach(0, 64, 0, 1.2, 64, 0) && !inPickupReach(0, 64, 0, 2.0, 64, 0) &&
              inPickupReach(0, 64, 0, 0, 66.2, 0) && !inPickupReach(0, 64, 0, 0, 66.5, 0),
          "item drops: pickup reach is one block sideways and half a block up and down");
    {
      double worst = 0;
      for (int i = -1000; i <= 3000; ++i) {
        const double t = i / 1000.0;
        worst = std::max(worst, std::abs(double(sinTurns(float(t))) - std::sin(t * 6.283185307179586)));
      }
      check(worst < 1e-4 && sinTurns(0.25f) > 0.99999f && std::abs(sinTurns(0.5f)) < 1e-6f, "item drops: sinTurns tracks sin within 1e-4");
    }
    {
      DropPose a[4], b[4], c[4];
      const int na = posesFor(77, 1234, 32, 30, 10.5, 64.0, -3.5, a);
      const int nb = posesFor(77, 1234, 32, 30, 10.5, 64.0, -3.5, b);
      posesFor(78, 1234, 32, 30, 10.5, 64.0, -3.5, c);
      bool eq = na == 4 && nb == 4;
      for (int i = 0; i < 4 && eq; ++i)
        eq = a[i].x == b[i].x && a[i].y == b[i].y && a[i].z == b[i].z && a[i].yawTurns == b[i].yawTurns;
      check(eq && c[0].yawTurns != a[0].yawTurns && a[0].y >= 64.0f + kHover - 1e-4f && a[0].y <= 64.0f + kHover + kBobAmplitude + 1e-4f,
            "item drops: the pose is a pure function of id and age (4 copies for 30, hovering and bobbing in range)");
    }
    {
      std::vector<SavedItem> in(2);
      in[0].id = 9; in[0].x = 1.25; in[0].y = 70; in[0].z = -4.5; in[0].vy = -2; in[0].ageTicks = 100; in[0].count = 3; in[0].item = "lithokraft:stone";
      in[1].id = 4; in[1].x = 2; in[1].y = 65; in[1].z = 8; in[1].onGround = true; in[1].count = 1; in[1].delayTicks = 64; in[1].item = "lithokraft:fragment";
      in[1].hasData = true; in[1].name = "Shard"; in[1].certificates = {"cert-a", "cert-b"};
      const auto bytes = encodeItems(in);
      std::vector<SavedItem> out;
      std::string err;
      const bool ok = decodeItems(bytes, &out, &err);
      check(ok && out.size() == 2 && out[0].id == 4 && out[1].id == 9 && out[1].x == 1.25 && out[1].vy == -2 && out[1].ageTicks == 100 &&
                out[0].onGround && out[0].delayTicks == 64 && out[0].name == "Shard" && out[0].certificates.size() == 2 &&
                out[1].item == "lithokraft:stone",
            "item drops: the save record round-trips, items in id order");
      auto bad = bytes;
      bad[bad.size() - 1] ^= 0x5a;
      check(!decodeItems(bad, &out, &err), "item drops: a damaged save record is refused");
    }
    {
      bool sorted = modelCount() == 107;
      for (size_t i = 1; i < modelCount() && sorted; ++i) sorted = std::string(modelRow(i - 1).id) < std::string(modelRow(i).id);
      check(sorted, "item drops: 107 CloudAtlas models, sorted by id");
    }
    {
      lithokraft::cItemBehavior b([](int, int y, int) { return y < 60; }, [](int, int, int) { return 0.6f; },
                                  [](int, int y, int) { return y >= 60 && y <= 63; });  // water 60..63, surface at 64
      litholattice::Entity e;
      e.x = 0.5; e.y = 61.0; e.z = 0.5;
      for (int i = 0; i < 32 * 10; ++i) { b.onTick(e, 1.0 / 32); ++e.age; }
      check(!e.dead && !e.onGround && e.y > 63.0 && e.y < 64.5, "item drops: items float up to the water surface");
      litholattice::Entity d;
      d.x = 0.5; d.y = 60.0; d.z = 0.5; d.onGround = true;
      d.age = ticksFor(kLifeSeconds, 32) - 1;
      b.onTick(d, 1.0 / 32);
      const bool alive = !d.dead;
      d.age += 1;
      b.onTick(d, 1.0 / 32);
      check(alive && d.dead, "item drops: an item despawns after exactly 8 minutes of ticks");
    }
    {  // P-1 golden: a thrown item lands on the same spot, bit for bit, however the world ticks
       // group into frames (1, 2 or 4 ticks per frame: about 30, 15 and 8 fps at 32 TPS).
      auto run = [](int perFrame) {
        lithokraft::cItemBehavior b([](int, int y, int) { return y < 64; }, [](int, int, int) { return 0.6f; });
        litholattice::Entity e;
        e.x = 0.5; e.y = 66.0; e.z = 0.5; e.vx = 3.0; e.vy = 2.5; e.vz = -1.25;
        for (int frame = 0; frame * perFrame < 32 * 4; ++frame)
          for (int k = 0; k < perFrame; ++k) { b.onTick(e, 1.0 / 32); ++e.age; }
        return e;
      };
      const auto a = run(1), b2 = run(2), c4 = run(4);
      check(a.onGround && a.y >= 64.0 && a.y < 64.01 && a.x == b2.x && a.x == c4.x && a.z == b2.z && a.z == c4.z && a.y == b2.y && a.y == c4.y &&
                a.vx == c4.vx,
            "item drops: a thrown item lands on the same spot at any frame rate (fixed tick step)");
    }
    {  // P-5: the draw lerp weight
      check(tickAlpha(0.0, 32, true) == 1.0 && tickAlpha(1.0 / 64, 32, false) == 0.5 && tickAlpha(5.0, 32, false) == 1.0 &&
                tickAlpha(-1.0, 32, false) == 0.0,
            "item drops: the draw lerp is 1 in deterministic runs, a fraction of a tick otherwise, clamped");
      DropPose p0[4], p1[4], ph[4];
      posesFor(77, 40.0, 32, 1, 0, 64, 0, p0);
      posesFor(77, 41.0, 32, 1, 0, 64, 0, p1);
      posesFor(77, 40.5, 32, 1, 0, 64, 0, ph);
      const float lo = std::min(p0[0].y, p1[0].y), hi = std::max(p0[0].y, p1[0].y);
      check(ph[0].y >= lo - 1e-4f && ph[0].y <= hi + 1e-4f && ph[0].yawTurns != p0[0].yawTurns,
            "item drops: a half-tick pose sits between the two tick poses");
    }
  }
  {  // A3 (brief B1/B1a no-decision subset, 2026-10-13): the per-item use
     // registry dispatches defined handlers and fails closed for the rest.
     // (FragmentState production ownership + the markdown flag path are
     // proven against a real Game in the headless harness suite.)
    using namespace litholattice;
    ChunkMap iw;
    iw.setRegistry(&blockRegistry());
    { auto c = std::make_unique<ChunkColumn>(); c->cx = 0; c->cz = 0; iw.insert(std::move(c)); }
    iw.set(5, 64, 5, kWaferSilicon);
    iw.set(7, 64, 5, kDeadSilicon);
    const IWorldSim::DirtyFn noMark = [](int, int, int) {};
    check(hasItemUseHandler(kItemSeedor) && hasItemUseHandler(kItemSilicaSeed) && hasItemUseHandler(kItemFragment) &&
          !hasItemUseHandler(kItemBashor) && !hasItemUseHandler(999999),
          "item-use (A3/B1a): handlers registered for defined items only (Seedor, silica seed, Fragment stub)");
    check(dispatchItemUse(kItemSeedor, ItemUseContext{iw, 5, 64, 5, noMark}) == ItemUseResult::Handled &&
          iw.get(5, 64, 5) == kSilicaTilledSoil,
          "item-use (A3/B1a): Seedor dispatch tills silicon soil (ToolAction::Till behavior)");
    check(dispatchItemUse(kItemSeedor, ItemUseContext{iw, 7, 64, 5, noMark}) == ItemUseResult::Failed &&
          iw.get(7, 64, 5) == kDeadSilicon,
          "item-use (A3/B1a): Seedor on dead silicon fails (defined refusal), world unchanged");
    check(dispatchItemUse(kItemSilicaSeed, ItemUseContext{iw, 5, 64, 5, noMark}) == ItemUseResult::Handled &&
          iw.get(5, 65, 5) == kSilicaGrass,
          "item-use (A3/B1a): silica seed dispatch plants on tilled soil");
    check(dispatchItemUse(kItemFragment, ItemUseContext{iw, 5, 64, 5, noMark}) == ItemUseResult::Failed,
          "item-use (A3/B1a): the Fragment's use fails closed as decision-pending (JD C3 open verb)");
    check(dispatchItemUse(kItemBashor, ItemUseContext{iw, 5, 64, 5, noMark}) == ItemUseResult::Failed &&
          dispatchItemUse(999999, ItemUseContext{iw, 5, 64, 5, noMark}) == ItemUseResult::Failed,
          "item-use (A3/B1a): undefined items fail closed (logged), never fake success");
  }
  {  // A5 (projectile density plumbing, 2026-10-13): BlockDef::density now
     // flows from the content table into engine projectile behavior through
     // the exact seam Game wires in production (registry solidity+density).
    using namespace litholattice;
    const cBlockReg& reg5 = blockRegistry();
    check(reg5.def(kDirt).density == 20 && reg5.def(kStone).density == 50 && reg5.def(kCeramic).density == 80 &&
          reg5.def(kGlass).density == 10 && reg5.def(kChipor).density == 70 && reg5.def(kRedTrace).density == 0,
          "density (A5): material-class table values land in BlockDef (earth 20, stone 50, ceramic 80, glass 10, Chipor 70)");
    bool coverage = true;
    for (size_t i = 1; i < reg5.size(); ++i) {
      const BlockDef& d = reg5.def(BlockId(i));
      if (reg5.has(BlockId(i), kSolid) == (d.density == 0)) coverage = false;  // solid <=> density > 0
    }
    check(coverage, "density (A5): every solid block carries density > 0; every non-solid stays 0 (never consulted)");
    ChunkMap pw;
    pw.setRegistry(&reg5);
    { auto c = std::make_unique<ChunkColumn>(); c->cx = 0; c->cz = 0; pw.insert(std::move(c)); }
    cProjectileMgr::VoxelQueryFn q5 = [&pw, &reg5](int x, int y, int z) {
      cProjectileMgr::VoxelInfo v;
      const BlockId b = pw.get(x, y, z);
      v.solid = reg5.has(b, kSolid);
      v.density = reg5.def(b).density;
      return v;
    };
    auto shoot = [&](BlockId wall, uint8_t pen, double& maxX, bool& alive) {
      for (int x = 10; x <= 12; ++x) pw.set(x, 100, 0, wall);
      cProjectileMgr pm;
      pm.setVoxelQuery(q5);
      Projectile p;
      p.x = 0; p.y = 100.5; p.z = 0.5; p.vx = 2; p.penetration = pen; p.maxRange = 200;
      const uint64_t id = pm.spawn(p, nullptr);
      maxX = 0; alive = true;
      for (int i = 0; i < 40 && pm.alive(id); ++i) { pm.tick(); if (const Projectile* pr = pm.get(id)) maxX = std::max(maxX, pr->x); }
      alive = pm.alive(id);
      for (int x = 10; x <= 12; ++x) pw.set(x, 100, 0, kAir);
    };
    double mx; bool al;
    shoot(kDirt, 30, mx, al);
    check(al && mx > 13, "density (A5): penetration 30 pierces a dirt wall (density 20) Ã¢â‚¬â€ density flows from block data");
    shoot(kDirt, 10, mx, al);
    check(!al && mx < 10, "density (A5): penetration 10 stops and despawns at a dirt wall (density 20)");
    shoot(kCeramic, 30, mx, al);
    check(!al && mx < 10, "density (A5): penetration 30 stops at a ceramic wall (density 80) that dirt let through");
    shoot(kCeramic, 90, mx, al);
    check(al && mx > 13, "density (A5): penetration 90 pierces ceramic (density 80) Ã¢â‚¬â€ the pierce rule discriminates by data");
  }
  const BlockMaterials checker(blockRegistry().size());  // no pack: every face is the missing texture
  {  // Mesher check (P2b-2, review B-L8/D-M9): mesh column (2,2) at ITS OWN
     // topmost sub-chunk that yields quads. The old check meshed (2,2) at
     // the GLOBAL max surfaceSy (a resonator-spire column's height), where
     // (2,2) has no sub-chunk at all Ã¢â‚¬â€ 0 verts, a test bug, not a mesher
     // bug (the mesher's own unit tests pass). Walk (2,2)'s sub-chunks
     // from the top; the first that meshes non-empty is by construction a
     // sub-chunk containing a non-air block with an exposed face.
    const StableId airSid = blockRegistry().stableIdOf(kAir);
    int meshSy = -1;
    size_t meshVerts = 0;
    int triedCensus = 0;
    for (int sy = kSubChunks - 1; sy >= 0 && meshSy < 0; --sy) {
      const ChunkColumn* col = wafer->chunks.column(2, 2);
      if (!col || !col->subs[size_t(sy)]) continue;
      int census = 0;
      for (int i = 0; i < kSubChunkVolume; ++i)
        if (col->subs[size_t(sy)]->blocks.at(i) != airSid) ++census;
      if (census == 0) continue;
      triedCensus = census;
      const ChunkMesh m = meshSubChunk(*snapshotForMeshing(wafer->chunks, blockRegistry(), 2, sy, 2, 1, 1, wafer->belowWorldFill),
                                       MeshContext{blockRegistry(), checker, &blockAppearance()});
      if (!m.verts.empty()) { meshSy = sy; meshVerts = m.verts.size(); }
    }
    if (meshSy < 0)
      std::printf("  mesher diag: column (2,2) produced no quads at any sub-chunk (last tried census=%d non-air cells)\n", triedCensus);
    else
      std::printf("  mesher: column (2,2) own surface sub-chunk sy=%d -> %zu verts\n", meshSy, meshVerts);
    check(meshSy >= 0 && meshVerts % 4 == 0, "mesher produces quads for a surface sub-chunk");
  }
  {
    HudLayer hud;
    hud.rects.push_back({0, 0, 10, 10, 1, 1, 1, 1});
    hud.texts.push_back({4, 4, "ab"});
    std::vector<HudVertex> v;
    buildHudVertices(hud, v);
    const HudFontAtlas atlas = buildHudFontAtlas();
    check(v.size() == 30 && v[0].u < 0 && v[6].u >= 0 && atlas.pixels.size() == size_t(atlas.width) * size_t(atlas.height),
          "overlay geometry (shared by the GL and Vulkan backends): rect + shadowed text quads, font atlas");
  }
  {
    const VulkanProbe vk = probeVulkan();  // headless: no window, instance + device list only
    if (!vk.loaderFound) std::printf("  [SKIP] vulkan: %s\n", vk.summary.c_str());
    // Loader present but no usable device on this machine (GPU-less box:
    // VK_ERROR_INCOMPATIBLE_DRIVER / 0 devices) is environmental Ã¢â‚¬â€ SKIP,
    // not a permanent FAIL that trains readers to ignore the suite
    // (P2b-2, review G-L6). Genuine init failures still FAIL via vk.ok.
    else if (vk.noDevices) std::printf("  [SKIP] vulkan: no usable device on this machine (%s)\n", vk.summary.c_str());
    else check(vk.ok, ("vulkan: headless instance + physical devices (" + vk.summary + ")").c_str());
  }
  wafer->unload();
  check(wafer->chunks.columns().empty(), "wafer unload frees all chunks");

  ow->load(pool, 8);
  int biome[2] = {0, 0};
  for (auto& [k, col] : ow->chunks.columns())
    for (auto& s : col->subs)
      if (s) for (int i = 0; i < kSubChunkVolume; ++i) if (s->blocks.at(i) == blockRegistry().stableIdOf(kGrass)) ++biome[s->state[i] & 1];
  check(ow->chunks.columns().size() == 64 && reg.find("overworld") == ow && biome[0] > 0 && biome[1] > 0, "organic world loads with both biomes (plains + hills); 'overworld' alias works");
  {  // worldgen: built-ins embedded in the exe, seeding, caves + veins underground
    int caves = 0, dirtVeins = 0;
    for (auto& [k, col] : ow->chunks.columns())
      for (int z = 0; z < kChunkSize; ++z)
        for (int x = 0; x < kChunkSize; ++x) {
          const int wx = col->cx * kChunkSize + x, wz = col->cz * kChunkSize + z, top = ow->chunks.surfaceY(wx, wz);
          for (int y = 1; y < top - 12; ++y) {
            const BlockId b = ow->chunks.get(wx, y, wz);
            caves += b == kAir;
            dirtVeins += b == kDirt;
          }
        }
    check(caves > 0 && dirtVeins > 0, "organic worldgen: worm caves and dirt veins underground, surface intact");
    WorldgenSettings seeded;
    seeded.seed = 12345;
    cDimensionReg other = createDimensions(seeded);
    const auto a = ow->generator->generate(3, 3), b = other.find("organic")->generator->generate(3, 3);
    const auto w0 = wafer->generator->generate(1, 1), w1 = other.find("wafer")->generator->generate(1, 1);
    int organicDiff = 0, waferDiff = 0;
    for (int sy = 0; sy < kSubChunks; ++sy) {
      organicDiff += bool(a->subs[size_t(sy)]) != bool(b->subs[size_t(sy)]) ||
                     (a->subs[size_t(sy)] && a->subs[size_t(sy)]->blocks != b->subs[size_t(sy)]->blocks);
      waferDiff += bool(w0->subs[size_t(sy)]) != bool(w1->subs[size_t(sy)]) ||
                   (w0->subs[size_t(sy)] && w0->subs[size_t(sy)]->blocks != w1->subs[size_t(sy)]->blocks);
    }
    check(builtinWorldgenSource("organic") && organicDiff > 0 && waferDiff == 0,
          "worldgen: built-in generators embedded; world seed changes organic terrain, wafer is seed-independent");
  }
  ow->unload();

  {  // scripts folder choice (dev builds: source tree; shipping: next to the exe; --scripts overrides)
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path root = uniqueTempDir("lithokraft-selftest-scripts");  // G-M7: per-run unique
    const fs::path src = root / "src" / "scripts", exe = root / "exe" / "scripts";
    fs::create_directories(src, ec);
    fs::create_directories(exe, ec);
    const auto pick = [&](std::string_view mode, const fs::path& source) { return fs::path(resolveScriptsDir(mode, source, {root / "missing", exe}).dir); };
    check(pick("", src) == src && pick("", fs::path()) == exe && pick("", root / "gone") == exe,
          "scripts dir: dev build edits the source tree; shipping (or no source tree) uses the exe copy");
    check(pick("exe", src) == exe && pick("source", src) == src && pick("source", fs::path()).empty() &&
              pick(exe.string(), src) == exe && pick((root / "nope").string(), src).empty(),
          "scripts dir: --scripts exe|source|<dir> overrides; missing folders give none");
    fs::remove_all(root, ec);
  }

  {  // shipped packs through the resource system: engine base (shaders) < game pack < pack_64 (resources.json)
    cResourceMgr rm(packDirs);
    rm.reload();
    const bool ok = packDirs.size() >= 3;
    if (!ok) std::printf("  (engine or game assets/ not found)\n");
    const WorldgenSettings gen = rm.worldgen(1);
    check(ok && gen.generators.count("wafer") && gen.generators.count("organic"),
          "game pack: worldgen/wafer.json and worldgen/organic.json resolve through the pack stack");
    size_t icons = 0;
    for (size_t b = 1; b < blockRegistry().size(); ++b)
      if (const Image* i = rm.icon(BlockId(b)); i && i->width() >= 16) ++icons;
      else std::printf("  (no icon for %s: textures/ui/icons/%s.png)\n", blockRegistry().idOf(BlockId(b)).c_str(), cResourceMgr::iconName(BlockId(b)).c_str());
    check(ok && icons == blockRegistry().size() - 1, "hotbar: every block has a textures/ui/icons icon in the pack stack (>= 16 px; _item icons at full res, fitted by the HUD)");
    {  // Cortana 2026-10-16 (BUG-0003): the software texture array keeps the pack's full
       // resolution (it used to box-filter every layer to 16x16 = the software blur).
      cCpuTextureArray cta;
      cta.setMaxTile(64);
      cta.upload(rm.textures());
      const auto src = rm.textures().textures();
      int srcMax = 0;
      for (const auto& tx : src) srcMax = std::max(srcMax, tx.image.width());
      bool exact = cta.tileSize() == std::min(64, srcMax) && cta.data() != nullptr;
      int checked = 0;
      for (size_t L = 0; exact && L < src.size() && checked < 8; ++L) {
        const Image& im = src[L].image;
        if (im.width() != cta.tileSize() || im.height() != cta.tileSize()) continue;
        const std::string_view nm = src[L].name;
        if (nm.find("leaves") != std::string_view::npos) continue;  // coverage-adjusted alpha
        const int T = cta.tileSize();
        for (int y = 0; y < T && exact; y += 7)
          for (int x = 0; x < T && exact; x += 5) {
            const uint8_t* p = im.pixel(x, y);
            const uint32_t want = (uint32_t(p[3]) << 24) | (uint32_t(p[0]) << 16) | (uint32_t(p[1]) << 8) | p[2];
            exact = cta.data()[L * size_t(T * T) + size_t(y * T + x)] == want;
          }
        ++checked;
      }
      const std::string msg = "software textures (BUG-0003): full pack resolution kept, texel == source pixel (tile " + std::to_string(cta.tileSize()) + ")";
      check(ok && exact && checked > 0, msg.c_str());
      cCpuTextureArray c32;
      c32.setMaxTile(32);
      c32.upload(rm.textures());
      check(ok && c32.tileSize() == std::min(32, srcMax), "software textures: render.tex_res 32 cap (Chromebook tier) honoured");
    }
    {  // Item icons (items workstream, 2026-10-12): every non-block item in the
       // registry resolves a 16x16 icon through the same pack path; the
       // block-backed Chipor resolves through the block icon above.
      size_t itemIcons = 0, itemDefs = 0;
      for (const ItemDef& d : itemRegistry()) {
        if (d.itemId < kFirstItemId) continue;  // block-backed (Chipor)
        ++itemDefs;
        if (const Image* i = rm.itemIcon(d.itemId); i && i->width() == 16) ++itemIcons;
        else std::printf("  (no item icon for %.*s: textures/ui/icons/%.*s.png)\n", int(d.stringId.size()), d.stringId.data(), int(d.iconName.size()), d.iconName.data());
      }
      check(ok && itemIcons == itemDefs, "items: every non-block item has a textures/ui/icons icon in the pack stack (16 x 16 for the HUD)");
      // B9 2026-11-12 (JD): Resin is an item -- the HUD gets its pot icon, an inventory holds and stacks it.
      const Image* ri = rm.itemIcon(kItemResin);
      Inventory rinv;
      check(ok && ri && ri->width() == 16 && rinv.addItem(kItemResin, 3) == 0 && rinv.countOf(kItemResin) == 3,
            "RESIN: the item has its HUD icon (pot) and a player inventory holds / stacks it (creative Equipment lists every item)");
    }
    {  // Fresh spawn inventory (items workstream): the Fragment is granted on
       // spawn and a fresh Inventory can hold every lore tool item.
      Inventory inv;
      bool fresh = grantStartingItems(inv) && inv.countOf(kItemFragment) == 1 &&
                   inv.hotbar()[0].itemId == kItemFragment;
      for (const ItemDef& d : itemRegistry())
        if (!d.startingItem) fresh = fresh && inv.addItem(d.itemId, 1) == 0 && inv.countOf(d.itemId) >= 1;
      check(fresh, "items: fresh spawn inventory holds the Fragment + every lore tool item");
    }
    {  // 2026-10-31 (JD): inventory screen click is one-way: list slot -> hotbar copy, never a swap
      Inventory inv;
      inv.hotbar()[2] = ItemStack{uint32_t(kRedTorch), uint16_t(5)};
      inv.main()[3] = ItemStack{uint32_t(kBlueTrace), uint16_t(9)};
      const int from = kInvHotbarSlots + 2 + 3;  // flat order: hotbar, offhand L/R, main
      const bool ok = inv.assignToHotbar(from, 2) && inv.hotbar()[2].itemId == uint32_t(kBlueTrace) &&
                      inv.hotbar()[2].count == 9 && inv.main()[3].itemId == uint32_t(kBlueTrace) && inv.main()[3].count == 9 &&
                      !inv.assignToHotbar(2, 2) && !inv.assignToHotbar(-1, 0) && !inv.assignToHotbar(kTotalSlots, 0);
      bool noRedTorch = true;
      for (const ItemStack* s : inv.allSlots()) noRedTorch = noRedTorch && s->itemId != uint32_t(kRedTorch);
      check(ok && noRedTorch, "inventory: a click copies the list slot into the hotbar one-way (source kept, old hotbar stack not moved into the list)");
    }
    {
      HudLayer hud;
      lithogui::DrawList iconList;
      iconList.imageQuad({0, 0, 32, 32}, ui::iconTextureId("block:4"), 1.0f);
      cHudUiRenderer r(hud, [](int) { return std::optional<lithogui::Color>{}; },
                       [&](int id) -> const Image* { return ui::iconTextureName(id) == "block:4" ? rm.icon(kRedTorch) : nullptr; });
      r.render(iconList);
      check(hud.draws.size() == 1 && hud.draws[0].icon && hud.iconAtlas && hud.iconAtlasVersion > 0,
            "hotbar: a block icon paints as ONE textured quad (Phase B), not pixel rects or a swatch");
    }
    const cTextureReg& tex = rm.textures();
    const auto mats = rm.materials();
    auto face = [&](BlockId b, FaceSlot f, bool active) { return tex.texture(TextureId{mats->face(b, f, active).layer}).name; };
    check(ok && tex.textures().size() > 80 && tex.missingNames().empty() && rm.models().size() >= 24,
          "pack_64 resources.json: every texture loads into the registry, 24+ block models");
    {  // D9 fix 2 (JD): the shipped theme's hotbar outline is the deeper violet #8A2EFF, 3 px; generic slots stay white.
      const ui::Color hc = rm.theme().style("hotbar_slot").accent, sc = rm.theme().style("slot").accent;
      auto nearC = [](float a, float b) { return std::fabs(a - b) < 0.5f / 255.f; };
      check(rm.theme().hasStyle("hotbar_slot") && nearC(hc.r, 138.f / 255.f) && nearC(hc.g, 46.f / 255.f) && nearC(hc.b, 1.f) && nearC(hc.a, 1.f),
            "hotbar (D9): the loaded ui/theme.json hotbar outline is #8A2EFF, opaque");
      check(std::fabs(rm.theme().style("hotbar_slot").selectedWidth - 3.f) < 1e-4f && rm.theme().style("slot").selectedWidth == 0.f,
            "hotbar (D9): the loaded ui/theme.json hotbar selected outline is 3 px (selected_width); generic slots keep the legacy width");
      check(nearC(sc.r, 1.f) && nearC(sc.g, 1.f) && nearC(sc.b, 1.f), "hotbar (C9): the generic slot outline in ui/theme.json stays white");
    }
    {  // fix 3c (JD t2288u): stacked vias draw the model's connected side variants (tinted)
      auto cs = [&](BlockId b, int v) { const FaceMaterial* m = mats->connectedSide(b, v, false); return m ? tex.texture(TextureId{m->layer}).name : std::string(); };
      bool tinted = true;
      for (int v = 0; v < kConnectedSideCount; ++v)
        for (BlockId b : {kBlueVia, kRedVia})
          tinted = tinted && mats->connectedSide(b, v, false) && mats->tintSlot(mats->connectedSide(b, v, false)->layer) != 0;
      check(ok && cs(kBlueVia, kConnectedSideBottom) == "via_side_bottom_neutral" && cs(kBlueVia, kConnectedSideMid) == "via_side_mid_neutral" &&
                cs(kBlueVia, kConnectedSideTop) == "via_side_top_neutral" && cs(kBlueVia, kConnectedSideSingle) == "via_side_single_neutral" &&
                !cs(kRedVia, kConnectedSideMid).empty() && tinted && !mats->connectedSide(kRepeator, kConnectedSideMid, false),
            "vias: models' side_bottom/mid/top/single load as tinted connected side materials");
      check(viaSideVariant(true, false) == kConnectedSideBottom && viaSideVariant(true, true) == kConnectedSideMid &&
                viaSideVariant(false, true) == kConnectedSideTop && viaSideVariant(false, false) == kConnectedSideSingle,
            "vias: run rule (lowest = bottom, between = mid, highest = top, lone = single)");
    }
    check(ok && face(kRepeator, FaceSlot::Front, false) == "repeator_front_base" && face(kRepeator, FaceSlot::Top, true) == "repeator_top_d2_base" &&
              face(kThresholdor, FaceSlot::Left, false) == "thresholdor_low" && face(kThresholdor, FaceSlot::Right, false) == "thresholdor_high" &&
              face(kSwitch, FaceSlot::Bottom, false) == "switch_base" && face(kSiliconSlab, FaceSlot::Bottom, false) == "silicon_slab_top" &&
              face(kMemor, FaceSlot::Top, false) == "memor_top_empty" && face(kMemor, FaceSlot::Top, true) == "memor_top",
          "block models: front/back/left/right files, active top, side and slab fallbacks (as before the refactor)");
    // 2026-10-25 (JD, CloudAtlas Thresholdor readout): readout tops + Catchor-format mask in pack_64.
    check(ok && face(kThresholdor, FaceSlot::Top, false) == "thresholdor_top_readout" &&
              face(kThresholdor, FaceSlot::Top, true) == "thresholdor_top_readout_on" && mats->named("thresholdor_readout_mask") &&
              mats->named("thresholdor_top_readout_subtract") && mats->named("thresholdor_top_readout_subtract_on"),
          "thresholdor readout: readout tops (compare / on / subtract / subtract on) and the readout mask load");
    const auto anim = tex.findAnimation("pulsor_top_firing");
    check(ok && anim && tex.animation(*anim).frameCount == 7 && tex.animation(*anim).timing.frameMs == 31 /* 2026-10-24: Pulsor v2 strip */ &&
              mats->face(kPulsor, FaceSlot::Top, true).animated() && !mats->face(kPulsor, FaceSlot::Top, false).animated(),
          "pulsor firing strip: 7 frames at 31 ms (v2) from its .json sidecar, animated only while active");
    const auto toneAnim = tex.findAnimation("tone_block_front_playing");
    check(ok && toneAnim && tex.animation(*toneAnim).frameCount == 4 && tex.animation(*toneAnim).timing.frameMs == 100 &&
              tex.animation(*toneAnim).timing.loop && mats->face(kToneBlock, FaceSlot::Front, true).animated() &&
              face(kToneBlock, FaceSlot::Front, false) == "tone_block_front" && face(kToneBlock, FaceSlot::Top, true) == "tone_block_top_playing" &&
              face(kToneBlock, FaceSlot::Left, false) == "tone_block_side",
          "tone block (art v1.8): idle faces, playing top, 4-frame 100 ms looping front strip");
    const MaterialBindings& b = mats->bindings();
    check(ok && tex.palettes().size() == 5 && face(kRedTrace, FaceSlot::Top, false) == "missing" &&
              tex.texture(TextureId{mats->wirePiece(kBlueTrace, WirePiece::Corner).layer}).name == "blue_trace_corner" &&
              tex.palette(PaletteId{uint16_t(b.stripPalette[0])}).name == "red_trace" &&
              tex.palette(PaletteId{uint16_t(b.tierPalette[2])}).name == "vga64" &&
              tex.texture(TextureId{uint16_t(b.stripRing[1])}).name == "blue_trace_ring_mask" &&
              // WaferBeta: the Repeator's d2-d16 tint masks replace repeator_delay_mask (kShadeTraceTint)
              mats->tintSlot(mats->face(kRepeator, FaceSlot::Front, false).layer) != 0 &&
              tex.texture(TextureId{uint16_t(b.tintMask[mats->tintSlot(mats->face(kRepeator, FaceSlot::Front, false).layer) - 1])}).name == "repeator_front_tintmask",
          "materials: wire pieces, palettes and shader masks resolved from the models (no names in C++)");
    {  // 2026-11-09 wire float hunt (JD live report on build 113): the port
      // riser (trace arm), asserted HERE because only the real pack-built
      // materials resolve named("trace_arm_*")... the content selftest's
      // pack-less materials never do, so the riser branch cannot draw
      // there. Flat blue span between two bound Catchors on stone: the
      // risers stand on the wire's seat, the support top = the wire cell's
      // floor (mesh y 16), climbing to +3 (the component port pads).
      MeshInput in{};
      for (auto& id : in.id) id = kAir;
      auto at = [&](int x, int y, int z) -> BlockId& { return in.id[meshPadIndex(x + 1, y + 1, z + 1)]; };
      auto stAt = [&](int x, int y, int z) -> CellState& { return in.st[meshPadIndex(x + 1, y + 1, z + 1)]; };
      for (int x = 4; x <= 8; ++x) at(x, 0, 5) = kStone;
      at(4, 1, 5) = kCatchor; stAt(4, 1, 5) = makeState(0, 1, 0);
      at(8, 1, 5) = kCatchor; stAt(8, 1, 5) = makeState(0, 1, 0);
      for (int x = 5; x <= 7; ++x) at(x, 1, 5) = kBlueTrace;
      stAt(6, 1, 5) = makeState(40);
      const ChunkMesh m = meshSubChunk(in, MeshContext{blockRegistry(), *mats, &blockAppearance()});
      const FaceMaterial* arm = mats->named("trace_arm_blue");
      int mnY = 1 << 30, mxY = -1;
      if (arm)
        for (const PackedVertex& v : m.verts)
          if (v.layerIndex() == arm->layer) { mnY = std::min(mnY, int(v.y)); mxY = std::max(mxY, int(v.y)); }
      check(ok && arm && mxY >= 0 && mnY == 16 && mxY == 16 + 3,
            "appearance: port risers stand on the wire's seat (real materials, flat component span)");
    }
    {  // 2026-10-16 (JD): Alpha build counter - fields 0..63, carry left on 64.
      using V = std::array<int, 4>;
      check(bumpBuildVersion(V{0, 0, 1, 0}) == V{0, 0, 1, 1} && bumpBuildVersion(V{0, 0, 1, 63}) == V{0, 0, 2, 0} &&
                bumpBuildVersion(V{0, 0, 63, 63}) == V{0, 1, 0, 0} && bumpBuildVersion(V{0, 63, 63, 63}) == V{1, 0, 0, 0} &&
                bumpBuildVersion(V{63, 63, 63, 63}) == V{0, 0, 0, 0} && std::string(buildVersion()).rfind("Alpha ", 0) == 0,
            "build counter: Alpha a.b.c.d, 0.0.1.63 -> 0.0.2.0, 0.0.63.63 -> 0.1.0.0, full wrap; generated header present");
    }
    const ShaderLibrary& sh = rm.shaders();
    check(ok && sh.size() == 3 && sh.find("chunk") && sh.find("chunk")->origin == "base" &&
              sh.find("chunk")->fragment.find("uWirePal") != std::string::npos && sh.find("hud") && sh.find("line"),
          "shaders: chunk / hud / line load from assets/shaders through the pack stack");
  }
  {  // HUD readout comes from each block's behaviour class, through the registry
    const auto& cat = cBehaviorCatalog::get();
    check(cat.describe(kRepeator, makeState(64, 1, 3, true)) == "Repeator | delay 8 ticks (setting 3/6) | out 64 | facing E" &&
              cat.describe(kRedTrace, 40) == "Red Trace | strength 40" &&
              cat.describe(kColorLamp16, makeState(17, 0)) == "lamp16 | tier 16 (wrap) | strength 17 -> colour 0" &&
              cat.describe(kToneBlock, makeState(34, 2)).find("A4 (440.0 Hz)") != std::string::npos &&
              cat.describe(kWaferSilicon, 0) == "Wafer Silicon" && cat.component(kRepeator) && !cat.component(kRedTrace),
          "look-at readout: describe() per block behaviour (repeator, wire, color lamp, tone, plain)");
  }
  {  // AngelScript: hello.as loads, onLoad() runs and logs through our API
    cScriptEngine se;
    bool ok = se.init();
    se.setModPermissions(kPermWorldRead);  // currentDimension() needs world-read
    ok = ok && se.bindGlobalFunction("string currentDimension()", asFUNCTION(stubDimension), asCALL_CDECL) >= 0;
    ok = ok && se.loadDirectory(scriptsDir) && se.callHook("void onLoad()");
    check(ok && se.logCount() == 1 && se.lastLog().find("Hello") != std::string::npos,
          "AngelScript: scripts/hello.as loads and onLoad() logs");
  }
  {  // sandbox: no unregistered functions; runaway loops are aborted
    cScriptEngine se;
    se.init();
    std::printf("  (the next script error is expected)\n");
    check(!se.loadSource("sandbox", "void onLoad() { system(\"calc\"); }"), "AngelScript sandbox: unregistered system() call does not compile");
    cScriptEngine se2;
    se2.init();
    check(se2.loadSource("loop", "void onLoad() { while (true) {} }") && !se2.callHook("void onLoad()"),
          "AngelScript sandbox: infinite loop aborted by time budget");
  }
  {  // live log file: tee + per-line flush + rotation to .prev.log
    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = uniqueTempDir("lithokraft-selftest-logs");  // G-M7: per-run unique
    fs::remove_all(dir, ec);
    const fs::path f = dir / "lithokraft.log";
    auto slurp = [](const fs::path& p) { std::ifstream in(p); std::stringstream ss; ss << in.rdbuf(); return ss.str(); };
    bool ok = logOpenFile(f.string());
    logInfo("selftest run one");
    const std::string live = slurp(f);  // readable before close = flushed per line
    ok = ok && logOpenFile(f.string());
    logInfo("selftest run two");
    logCloseFile();
    const std::string cur = slurp(f), prev = slurp(dir / "lithokraft.prev.log");
    check(ok && live.find("selftest run one") != std::string::npos && live[0] == '[',
          "log file: timestamped lines, flushed per line (tail-able)");
    check(cur.find("run two") != std::string::npos && cur.find("run one") == std::string::npos &&
          prev.find("run one") != std::string::npos, "log file: previous run rotated to lithokraft.prev.log");
    fs::remove_all(dir, ec);
  }
  {  // SGNL v1 -> v2 migration: channel/beacon inserted before the key count
    storage::ByteWriter v1;
    v1.u64(0x123456789ABCDEF0ULL);
    for (int i = 0; i < 8; ++i) v1.u8(uint8_t(10 + i));  // out..timer
    v1.u16(2);
    v1.u8(7); v1.u8(8); v1.u8(9); v1.u8(10);  // two key pairs
    auto v2 = cWorldSave::migrateSignalV1ToV2(v1.out);
    bool ok = v2.has_value();
    if (ok) {
      storage::ByteReader r(v2->data(), v2->size());
      ok = r.u64() == 0x123456789ABCDEF0ULL;
      for (int i = 0; i < 8 && ok; ++i) ok = r.u8() == uint8_t(10 + i);
      ok = ok && r.u8() == 1 && r.u8() == 0;  // channel=1, beacon=0
      ok = ok && r.u16() == 2;
      ok = ok && r.u8() == 7 && r.u8() == 8 && r.u8() == 9 && r.u8() == 10;
      ok = ok && r.done();
    }
    check(ok, "save: v1 -> v2 migration inserts channel/beacon before the key count");
    storage::Bytes bad{1, 2, 3};  // truncated: must fail, not corrupt
    check(!cWorldSave::migrateSignalV1ToV2(bad).has_value(), "save: malformed v1 state fails migration cleanly");
  }
  if (timingHandshake) std::printf("  (selftest invoked with --timing-handshake: the handshake section below A/B-compares both modes in-process)\n");
  fails += runHandshakeSelfTest();
  fails += runControlSelfTest();
  fails += runDemoSelfTest();
  std::printf("%s\n", fails ? "ALL SELFTESTS: FAILED" : "ALL SELFTESTS: OK");
  return fails ? 1 : 0;
}

}  // namespace lithokraft
