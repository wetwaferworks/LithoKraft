#include "litholattice/core/hitch_capture.h"  // LL_HITCH_CAPTURE
#include "litholattice/core/tick_profile.h"   // GUI lag round 2: drain/commit timing gate
#include "litholattice/core/gate_lock.h"
#include "lithokraft/content/chunk_coord_text.h"
#include "lithokraft/content/randor_rng.h"  // 2026-10-31 (JD): Randor world seed
#include "litholattice/core/work_trace.h"  // duplicate-work trace
#include <string_view>
#include "litholattice/script/script_generic.h"
#include <thread>
#include "litholattice/core/f3_trace.h"
#include "player_link.h"  // Cortana 2026-10-26: sim-thread body + interpolated display pose
#include <unordered_set>  // Cortana 2026-10-15 1455
// Cortana 2026-10-15 0825/0935/1020: CHUNK-STALL, ALT-TAB, GAME-H1
#include "lithokraft/game/game.h"
#include "lithokraft/game/exit_watchdog.h"
#include "lithokraft/game/cFolderPicker.h"        // 2026-10-31 (JD): Load World folder dialog
#include "lithokraft/game/cWorldConfigBuilder.h"  // 2026-10-31 (JD): Load World in-process
#include "lithokraft/game/ChunkMapPresence.h"
#include "lithokraft/game/FrameDumpHub.h"  // showcase --frame-dump
#include "lithokraft/game/McpDisplayVision.h"  // SCRATCH vulkfb (opt-in, LL_MCP_DISPLAY_VISION=1)
#include "lithokraft/game/NemoViewObjects.h"
#include "lithokraft/game/NemoViewWindow.h"
#include "litholattice/render/nemo_view.h"
#include "litholattice/core/threeway.h"  // LL_THREEWAY (Proposed by JD; co-implemented by JD and Cortana)
#include "litholattice/mods/mod_interface.h"  // kPerm* for the script sandbox
#include "litholattice/mods/mod_permissions.h"  // permissionBitForName (mcp grant/revoke)
#include <algorithm>
#include <chrono>  // Cortana 2026-10-15 2215: hold-to-repeat timers
#include <cctype>  // 2026-10-25: teleport name matching
#include <cmath>
#include <cstdio>
#include <cstring>  // PlayJ: strcmp(renderer_.name())
#include <cstdlib>
#include <filesystem>
#include <string>
#include "controls_panel.h"
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/post.h"  // 2026-10-25 Posts
#include "lithokraft/content/foliage.h"  // Cortana 2026-10-15 2215: repeator button picking
#include "lithokraft/game/map_render.h"
#include "lithokraft/game/scout.h"
#include "lithokraft/game/screenshot.h"  // writeFileBytes
#include "lithokraft/game/wafer_pulse.h"
#include "lithokraft/content/certificates.h"  // B7: certificate registry + static curve
#include "lithokraft/game/etching.h"          // B6: the Etchor etch flow
#include "lithokraft/game/lithography.h"      // Cortana 2026-10-15 1900: mask-and-expose machine
#include "lithokraft/game/signing.h"          // B7: certificate signing flow
#include "litholattice/mcp/mcp_senses.h"  // encodePngRgb (clean-room PNG writer)
#include "litholattice/procgen/stages.h"  // cStructureFeature (map markers)
#include "litholattice/render/ground_pulse.h"
#include "lithokraft/content/item_use.h"
#include "lithokraft/content/signal_world.h"
#include "lithokraft/content/circuit_scene.h"
#include "lithokraft/content/test_world.h"  // playtest 2026-10-16: --test-world
#include "lithokraft/content/cBoltLabWorld.h"
#include "lithokraft/content/component_test_world.h"  // 2026-11-12 (JD): --component-test
#include "lithokraft/content/cPchieMazeWorld.h"  // 2026-11-10 (JD t5033u, PCHIE P7)
#include "lithokraft/game/mcp_world_reader.h"    // 2026-11-10 (JD t5033u, PCHIE P7)
#include <fstream>
#include <sstream>
#include "lithokraft/game/cWorldCatalog.h"  // 2026-10-30 (JD): --bolt-lab
#include "lithokraft/game/dimensions.h"
#include "ui_input_bridge.h"
#include "lithokraft/game/ui_textedit.h"
#include "litholattice/core/log.h"
#include "litholattice/core/time.h"
#include "litholattice/render/mesher.h"
#include "litholattice/render/software_renderer.h"  // 2026-10-16 (JD): F3 renderer line
#include "litholattice/render/software_simd.h"     // swsimd::activePath (ISA tier)
#include "lithokraft/game/build_version.h"         // 2026-10-16 (JD): Alpha build counter
#include "lithokraft/content/signal.h"
#include "litholattice/world/chunk.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/camera/camera.h"    // engine camera system: cvars + manager types
#include "litholattice/procgen/ramp_shape.h"  // Highway Ramp walk surface
#include "litholattice/world/raycast.h"    // voxel DDA for Game::cameraRaycast
#include "litholattice/sim/area_shape.h"  // Fix run 3: shared area shapes + containment

namespace {
// Fix run 3 (2026-10-23, JD t2262u): shape views of the config bools, and
// the stream radius that always CONTAINS the sim area, so no ticking chunk
// is ever outside the resident footprint. A square stream over any sim area
// gives max(tick, stream): exactly the pre-fix-3 radius.
litholattice::AreaShape simAreaShapeOf(const lithokraft::GameConfig& c) {
  return c.diamondSimArea ? litholattice::AreaShape::Diamond
         : c.roundSimArea ? litholattice::AreaShape::Round
                          : litholattice::AreaShape::Square;
}
litholattice::AreaShape streamAreaShapeOf(const lithokraft::GameConfig& c) {
  return c.diamondStreamArea ? litholattice::AreaShape::Diamond : litholattice::AreaShape::Square;
}
int effectiveStreamRadius(const lithokraft::GameConfig& c) {
  return std::max(c.streamRadius, litholattice::minStreamRadiusCovering(simAreaShapeOf(c), c.tickRadius, streamAreaShapeOf(c)));
}
}  // namespace

#include <cstdlib>
#include "litholattice/core/lf_job_queue.h"     // 2026-11-01 LL_LF_UPLOAD
#include "lithokraft/game/stall_timer.h"         // 2026-11-01 LL_STALL_TIMING
#include "litholattice/core/atomic_gate.h"
// 2026-11-01 (JD LL_LF_UPLOAD=1 + LL_LF_JOBS=1, default off): mesh / upload-prep jobs on the
// lock-free queue (no pool mutex; with LL_LF_COWTABLE=1 the column table takes no mutex either).
template <class Pool, class F>
static void submitMeshJob(Pool& pool, F&& f) {
  static const bool lf = [] { const char* e = std::getenv("LL_LF_UPLOAD"); return !(e && e[0] == '0');  /* PlayL: default ON (JD 2026-11-01) */ }();
  // TheReefs 2026-11-14 hitch: cLfJobQueue::shared() claims 0 workers on a
  // 4-thread host (census room is 0 once main/clock/dedicated mesh are
  // reserved). enabled() is always on, and a 0-worker queue runs every
  // submit inline on the game thread — firstMesh/border remesh then pay
  // gather+mesh under the between-ticks hold (~15-47 ms). When the LF
  // queue has no workers, route to the dedicated lock-free mesh pool
  // (already in the 2+1+0 budget). No new locks; hashes unchanged.
  if (lf && litholattice::cLfJobQueue::enabled() && litholattice::cLfJobQueue::shared().workers() > 0)
    litholattice::cLfJobQueue::shared().submit(std::forward<F>(f));
  else {
    static bool logged = false;
    if (!logged) {
      logged = true;
      if (lf && litholattice::cLfJobQueue::enabled())
        litholattice::logInfo("mesh submit: LL_LF_JOBS has 0 workers; routing column meshes to the dedicated mesh pool");
    }
    pool.submit(std::forward<F>(f));
  }
}
static const char* lfIoTag() {
  static const char* t = [] {
    const char* s = std::getenv("LL_LF_SAVE"); const char* l = std::getenv("LL_LF_LOAD");
    return (!(s && s[0] == '0') || !(l && l[0] == '0')) ? "lock-free io" : "old io";  // PlayL: default ON
  }();
  return t;
}
static int llP2PerfOff() { static const int v = [] { const char* e = std::getenv("LL_P2_PERF_OFF"); return e ? int(std::strtol(e, nullptr, 0)) : 0; }(); return v; }  // Cortana 2026-10-16 1520: A/B perf switch (render/display only)

namespace lithokraft {
namespace {
// Cortana 2026-10-26 (Archie camera-jump decision): the body is owned by the sim thread while it
// runs (stepped in the player hook); the frame thread talks to it only through g_plink.
plink::Link g_plink;
void plinkPublish(const PlayerLocomotion& L, bool reset, uint64_t tick, double dt, uint64_t placeSeq) {
  plink::Snap s = g_plink.snap.load();
  const auto p = L.position();
  const auto v = L.velocity();
  if (reset) { s.prev[0] = p.x; s.prev[1] = p.y; s.prev[2] = p.z; }
  else { for (int i = 0; i < 3; ++i) s.prev[i] = s.curr[i]; }
  s.curr[0] = L.body().x; s.curr[1] = L.body().y; s.curr[2] = L.body().z;
  if (reset) for (int i = 0; i < 3; ++i) s.prev[i] = s.curr[i];
  s.vel[0] = v.x; s.vel[1] = v.y; s.vel[2] = v.z;
  s.tNs = plink::Link::nowNs(); s.dt = dt; s.tick = tick; s.placeSeq = placeSeq;
  s.eye = L.eyeHeight(); s.pose = uint8_t(L.pose()); s.flying = L.flying(); s.onGround = L.onGround(); s.placed = L.placed();
  g_plink.snap.store(s);
}
bool plinkSimOwned(const std::unique_ptr<cSimThread>& sim) { return sim && sim->running(); }
std::atomic<uint64_t> g_shotFreezeTick = [] {  // see the player hook (item 5 pinned shots)
  if (!std::getenv("LL_SW_SHOT")) return uint64_t(0);
  const char* e = std::getenv("LL_BENCH_FREEZE_TICK");
  return e ? uint64_t(std::strtoull(e, nullptr, 10)) : uint64_t(1024);
}();
std::atomic<bool> g_simFrozen{false};
void locoPlace(const std::unique_ptr<cSimThread>& sim, PlayerLocomotion& L, const Vec3& pos) {
  const uint64_t seq = ++g_plink.placeSeq;
  if (plinkSimOwned(sim)) {
    plink::Cmd c; c.kind = plink::Cmd::Place; c.pos[0] = pos.x; c.pos[1] = pos.y; c.pos[2] = pos.z; c.seq = seq;
    g_plink.push(c);
  } else {
    L.place(pos);
    plinkPublish(L, true, g_plink.snap.load().tick, g_plink.snap.load().dt, seq);
  }
}
void locoSetMode(const std::unique_ptr<cSimThread>& sim, PlayerLocomotion& L, litholattice::LocomotionMode m) {
  if (plinkSimOwned(sim)) { plink::Cmd c; c.kind = plink::Cmd::Mode; c.mode = m; g_plink.push(c); }
  else { L.setMode(m); const auto s = g_plink.snap.load(); plinkPublish(L, true, s.tick, s.dt, s.placeSeq); }
}
plink::Snap locoSnap() { return g_plink.snap.load(); }
}  // namespace
}  // namespace lithokraft
namespace lithokraft {
std::shared_ptr<MeshInput> farLodMeshInput(const MeshInput& in, int f);  // bench_stream.cpp
bool asyncChunkUploadOn();  // Cortana 2026-10-25 upload pass (defined above Game::uploadMeshes)

// render.ambient_mode (JD 2026-10-11): 0 = Fraction (cheap default), 1 = NormalizedSun.
// Clamped here too, so a hand-edited settings value can never reach the renderer raw.
static litholattice::AmbientMode ambientModeFromInt(int m) {
  return std::clamp(m, 0, 1) == 1 ? litholattice::AmbientMode::NormalizedSun
                                  : litholattice::AmbientMode::Fraction;
}

// 2026-10-16 (JD leaf pass): render.leaves / render.twinkle resolution (see settings.h).
// LL_LEAVES=fast|fancy (bench / A-B) wins over the setting.
static bool resolvedLeavesFancy(const GameSettings& s) {
  if (const char* e = std::getenv("LL_LEAVES")) return std::string_view(e) != "fast";
  if (s.leaves >= 0) return s.leaves == 1;
  return std::thread::hardware_concurrency() == 0 || std::thread::hardware_concurrency() > 4;
}
// 2026-10-18 (Crew Canopy, F-B): render.foliage_density resolution.
// LL_FOLIAGE_DENSITY=0|1 (bench / A-B) wins over the setting, like LL_LEAVES.
static int resolvedFoliageDensity(const GameSettings& s) {
  if (const char* e = std::getenv("LL_FOLIAGE_DENSITY")) return std::string_view(e) == "1" ? 1 : 0;
  return std::clamp(s.foliageDensity, 0, 1);
}
// Cortana 2026-10-16: render.bloom resolution (LL_BLOOM=0|1 wins, like LL_LEAVES).
static bool resolvedBloom(const GameSettings& s) {
  if (const char* e = std::getenv("LL_BLOOM")) return std::atoi(e) != 0;
  if (s.bloom >= 0) return s.bloom == 1;
  return std::thread::hardware_concurrency() == 0 || std::thread::hardware_concurrency() > 4;
}
static int resolvedTwinkle(const GameSettings& s) {
  if (const char* e = std::getenv("LL_TWINKLE")) return std::clamp(std::atoi(e), 0, 2);
  if (s.twinkle >= 0) return s.twinkle;
  return resolvedLeavesFancy(s) ? 2 : 1;
}

Game::Game(const GameConfig& cfg, IRenderer& renderer, cJobPool& pool, cResourceMgr& resources, cAudioEngine& audio,
           cInputSystem& input)
    : cfg_(cfg),
      ui_({[this](int slot) { hot_ = slot; }, [this](BlockId b, bool shift) { pickInventoryBlock(b, shift); },
           [this] { setInventoryOpen(false); }, [this] { if (menuState_ == MenuState::Playing) setPaused(false); },
           [this] { quit_ = true; }, [this] { if (cfg_.worldPicker) openWorldPicker(); else enterPlay(); }, [this] { enterMenu(); },
           [this] { ui_.openSettings(settings_); }, [this](const GameSettings& s) { onSettingsChanged(s); },
           [this](int i) { pickCarrySlot(i); },
           [this] { saveWorld("menu", false); },  // 2026-10-31 (JD): pause menu Save world
           [this] { openWorldPicker(); },          // 2026-10-31 (JD): pause menu Load world
           [this](int flat) { putBackSlot(flat); },              // next-build-2 Item 4: creative put-back (retired from the click path 2026-11-11; kept wired)
           [this](uint32_t id, bool shift) { pickInventoryItem(id, shift); },  // Item 4: Equipment give
           [this](int flat) { cursorClick(flat); },              // cursor stack (2026-11-11): carry-slot press
           [this] { paletteClickCursor(); },                     // cursor stack: palette press while carrying discards
           [this](int flat) { shiftTransfer(flat); }}),          // D9: Shift-click quick move hotbar <-> main
      uiScripts_(scripts_, ui_.root()),
      renderer_(renderer), resources_(resources), sound_(audio), pool_(pool),
      registry_(createDimensions(resources.worldgen(cfg.seed))),
      playerAdapter_(player_),  // IPlayer integration: engine view of player_
      input_(input), audio_(audio),
      camera_(player_, GameCameraHooks{[this] { return snapshotMcpPlayer(); },
                                       [this](Vec3 o, Vec3 d, float m, float& hd) {
                                         return cameraRaycast(o, d, m, hd);
                                       },
                                       [] { return locoSnap().eye; }}),
      mcpController_(input_, {"player"},
                     [this] { return mcpControllable_.load(std::memory_order_relaxed); }),
      mcpView_([this] { return snapshotMcpPlayer(); }),
      mcpSenses_(audio, [this] { return snapshotMcpPlayer(); },
                 [this] { return mcpWorldTick_.load(std::memory_order_relaxed); }, camera_.pilotRig()),  // Cortana 2026-10-15 1658: F-2 (MCP thread must not read the sim triple buffer)
      settings_(cfg.settings) {
  // Projectile seams (P2c, ENT-1): voxel queries ride the sim's
  // race-free getBlock (the same read the entity behaviors use) and the
  // block registry's solidity/density; chunk-ahead requests land on the
  // active dimension's cChunkStreamer. Both lambdas resolve sim_ /
  // streamer_ at call time, so a dimension switch needs no rewiring.
  // NOTE on density: block defs carry per-material-class densities since
  // the A5 wiring pass (blocks.cpp's density table â€” JD-DECISION balance
  // values, not yet approved), so this seam now feeds real densities into
  // the engine rule (pierce when penetration > density). Non-solid blocks
  // stay 0 and are never consulted (solid=false above).
  projectiles_.setVoxelQuery([this](int x, int y, int z) {
    litholattice::cProjectileMgr::VoxelInfo v;
    if (!sim_) return v;
    const BlockId b = sim_->getBlock(x, y, z);
    v.solid = blockRegistry().has(b, kSolid);
    v.density = blockRegistry().def(b).density;
    return v;
  });
  projectiles_.setChunkRequester([this](int cx, int cz) {
    if (streamer_) streamer_->requestColumn(cx, cz);
  });
  // Wafer ground pulse: cache the wafer generator's biome weights once (the
  // registry is built above). Mesh workers query it through MeshContext.
  if (const Dimension* wafer = registry_.find("wafer"); wafer && wafer->generator)
    pulseCache_ = std::make_unique<WaferPulseCache>(wafer->generator);
  // Debug console (tilde key): available in all builds, wired before anything
  // else so engine log lines from startup are already in the tap when it opens.
  litholattice::console::registerConsoleScriptBindings();
  debugConsole_ = std::make_unique<litholattice::console::cDebugConsole>(consoleRegistry_);
  debugConsole_->attach(ui_.root().gui());
  {  // 2026-10-31 (JD, Load World): world_load <world id | folder>
    std::string werr;
    consoleRegistry_.registerCommand(
        "world_load", "world_load <world id | folder> - load a catalog world (default, testworld, flatlab, ...) or any world folder, in-process",
        [this](litholattice::console::cConsoleReg& r, const litholattice::console::ConsoleArgs& a) {
          const auto& cat = cWorldCatalog::instance().entries();
          if (a.size() < 2) {
            std::string ids;
            for (const auto& e : cat) ids += " " + e.id;
            r.print("usage: world_load <world id | folder>; worlds:" + ids);
            return;
          }
          std::string arg = a[1];
          for (size_t i = 2; i < a.size(); ++i) arg += " " + a[i];
          r.print(requestWorldLoad(arg));
        },
        &werr);
    if (!werr.empty()) logError("world_load: command registration failed: %s", werr.c_str());
  }
  if (!cfg_.settingsPath.empty()) {
    // exec/saveArchived sandbox: the console may only touch .lth scripts (and
    // legacy .cfg files) next to the settings file (the player's config dir).
    const std::filesystem::path sp(cfg_.settingsPath);
    if (sp.has_parent_path()) consoleRegistry_.setConfigDir(sp.parent_path());
  }
  // ---- lithoscript first-party population (Decision 9; the language/
  // population split, JD-locked): the console module owns the complete
  // language; these are the first-party registrations for JD to review.
  // Local single-player context: cheats allowed (Decision 8 per-context flag).
  consoleRegistry_.setCheatsAllowed(true);
  // Settings cvars: live get/set against the actual engine state, archived to
  // config.lth. A console set flows through onSettingsChanged, exactly like
  // a settings-file load; UI edits flow back via syncFrom() below.
  settingsCvars_ = std::make_unique<SettingsCvars>(
      [this](const GameSettings& s) { onSettingsChanged(s); }, [this] { return settings_; });
  settingsCvars_->registerOn(consoleRegistry_);
  // Engine camera-system cvars (camera.*): registered before the MCP senses
  // knobs because camera.chunk_radius is shared â€” the camera module's
  // registration is the canonical one (same name/default/clamp intent);
  // the senses row for it then no-ops with a boot notice, and update()
  // keeps reading the single cvar by name exactly as before.
  if (std::string camErr; !litholattice::camera::registerCameraCvars(consoleRegistry_, &camErr))
    logError("camera: cvar registration incomplete: %s", camErr.c_str());
  // MCP pilot-senses knobs (mcp.vision_width/height, mcp.audio_tier,
  // camera.chunk_radius): archived host knobs, not player preferences.
  registerMcpSensesCvars(consoleRegistry_);
  // cAiWorld host knob (ai.enabled, default OFF; brief B6, 2026-10-13):
  // archived host cvar, read per frame in update() like the mcp.* knobs.
  registerAiCvars(consoleRegistry_);
  // config.lth: the archived-cvar file the dtor writes via saveArchived().
  // Executed here through the SANDBOXED executeFile path (config dir only,
  // .lth only, no path escapes), so the archive is live again instead of
  // write-only. Placement: after every archived cvar is registered (an
  // unregistered cvar would reject the assignment), before the boot-apply
  // reads below and before autoexec.lth (the player's own script still wins).
  // Silent when absent.
  if (!cfg_.settingsPath.empty()) {
    const std::filesystem::path ce =
        std::filesystem::path(cfg_.settingsPath).parent_path() / "config.lth";
    std::error_code ceEc;
    if (std::filesystem::exists(ce, ceEc)) {
      // Fix 2026-10-11 (regression, LOW): executeFile refuses symlinks even in
      // the config dir (deliberate security behavior). Warn loudly so a
      // dotfiles-style setup doesn't lose config.lth with only a debug line.
      std::error_code slEc;
      if (std::filesystem::is_symlink(ce, slEc))
        logWarn("game: '%s' is a symlink; console exec refuses symlinks, config.lth not loaded",
                ce.string().c_str());
      else {
        // Cortana 2026-10-15 1240 (settings reset bug): config.lth archives
        // the settings-bridge cvars too, and executing it overrode
        // settings.json with stale values (display_mode 0, model_dither 1,
        // tps 32); the next save then wrote them back into settings.json.
        // settings.json is the source of truth for GameSettings: restore it
        // after the exec and push it back into the cvars (non-settings
        // archived cvars from config.lth still apply). autoexec.lth, run
        // later, can still override on purpose.
        const GameSettings fromFile = settings_;
        consoleRegistry_.executeFile("config.lth");
        settings_ = fromFile;
        settingsCvars_->syncFrom(settings_);
        saveSettings();  // 0.0.1.52 [C52-5] (BUG-0013): the exec's cvar sets already saved settings.json; put the file back too
        litholattice::setShellSolid(settings_.bulbShellSolid);  // 1551: config.lth exec must not leave the bulb shell stale
        if (sim_) sim_->requestRemeshAll();  // 0.0.1.51 (BUG): meshes made while config.lth had the shell Solid stayed opaque
      }
    }
  }
  // Game commands: give/kill (cheat-gated), map/save/pause/quit.
  {
    GameCommandBindings b;
    b.give = [this](uint32_t id, uint16_t count) {
      const uint16_t leftover = player_.inventory.addItem(id, count);
      std::string msg = "give: added " + std::to_string(count - leftover) + " of item " +
                        std::to_string(id);
      if (leftover) msg += " (" + std::to_string(leftover) + " did not fit)";
      return msg;
    };
    b.teleport = [this](const std::vector<std::string>& args) { return teleportCmd(args); };
    b.kill = [this] {
      // Provisional until the health/damage system lands: kill = respawn.
      player_.pos = spawnPoint_;
      return std::string("kill: respawned at spawn");
    };
    b.etchChip = [this](const std::string& name) {
      // Cortana 2026-10-15 1900: with a lithography machine within 6 blocks, the etch runs
      // through it (mask-and-expose, game/lithography.h): the wafer comes from the back
      // buffer and the named chip exits the front (into the inventory this pass). With no
      // machine near, the B6 inventory flow is unchanged.
      std::string msg;
      bool viaMachine = false;
      if (sim_) {
        sim_->runBetweenTicks([&] {
          const LithoReader rd = lithoReader();
          int ex, ey, ez;
          const auto& p = player_.pos;
          if (!findNearestEtchor(rd, int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z)), 6, ex, ey, ez)) return;
          viaMachine = true;
          const LithoMachine m = scanLithoMachine(rd, ex, ey, ez);
          if (!m.canEtch()) {
            msg = "etch refused: " + m.describe() + (m.exposed() && m.wafers == 0 ? " | back buffer empty (etchload)" : "");
            return;
          }
          Inventory tmp;  // validate the name field and build the chip stack with the B6 rules
          tmp.addItem(etchInputItemId(), 1);
          std::string err;
          if (!lithokraft::etchChip(tmp, name, &err)) { msg = err; return; }
          ItemStack chip;
          for (const auto* s : tmp.allSlots())
            if (s->itemId == etchedChipItemId() && s->count) { chip = *s; break; }
          if (chip.empty() || !player_.inventory.canPlaceStack(chip)) { msg = "etch: no room for the etched chip"; return; }
          auto& ch = active_->chunks;
          ch.setState(m.bufX, m.bufY, m.bufZ, lithoBufferWithCount(ch.getState(m.bufX, m.bufY, m.bufZ), m.wafers - 1));
          player_.inventory.placeStack(chip);
          msg = "etch: exposed and etched Chipor '" + name + "' (code " + std::to_string(m.code()) + ", " +
                std::to_string(m.wafers - 1) + " wafers left) - out the front";
        });
      }
      if (viaMachine) return msg;
      std::string err;
      if (!lithokraft::etchChip(player_.inventory, name, &err)) return err;
      return "etch: etched a Chipor named '" + name + "' (no lithography machine within 6 blocks: inventory flow)";
    };
    b.etchScan = [this] {
      std::string msg = "etchscan: no Etchor within 6 blocks";
      if (!sim_) return msg;
      sim_->runBetweenTicks([&] {
        const LithoReader rd = lithoReader();
        int ex, ey, ez;
        const auto& p = player_.pos;
        if (findNearestEtchor(rd, int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z)), 6, ex, ey, ez))
          msg = scanLithoMachine(rd, ex, ey, ez).describe();
      });
      return msg;
    };
    b.etchLoad = [this](int n) {
      std::string msg = "etchload: no Etchor within 6 blocks";
      if (!sim_ || n <= 0) return n <= 0 ? std::string("usage: etchload [n>0]") : msg;
      sim_->runBetweenTicks([&] {
        const LithoReader rd = lithoReader();
        int ex, ey, ez;
        const auto& p = player_.pos;
        if (!findNearestEtchor(rd, int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z)), 6, ex, ey, ez)) return;
        const LithoMachine m = scanLithoMachine(rd, ex, ey, ez);
        if (rd.block(m.bufX, m.bufY, m.bufZ) != kInputBuffer) { msg = "etchload: " + m.describe(); return; }
        const int room = kLithoBufferMax - m.wafers;
        const int want = std::min({n, room, int(player_.inventory.countOf(etchInputItemId()))});
        if (want <= 0) { msg = room <= 0 ? "etchload: back buffer full" : "etchload: no Wafer Silicon in inventory"; return; }
        const int got = int(player_.inventory.removeItem(etchInputItemId(), uint16_t(want)));
        auto& ch = active_->chunks;
        ch.setState(m.bufX, m.bufY, m.bufZ, lithoBufferWithCount(ch.getState(m.bufX, m.bufY, m.bufZ), m.wafers + got));
        msg = "etchload: loaded " + std::to_string(got) + " wafers (" + std::to_string(m.wafers + got) + " in the back buffer)";
      });
      return msg;
    };
    b.signCertificate = [this](const std::string& certId) {
      // Certificate-table signing (B7): spend static levels from the
      // adapter's stat, apply to the selected hotbar stack (flat slot ==
      // hotbar index in the canonical order).
      std::string err;
      int32_t total = playerAdapter_.staticCharge();
      if (!signStack(player_.inventory, hot_, certId, total, &err)) return err;
      playerAdapter_.enginePlayer().setStat(playerAdapter_.staticStatId(), total);
      const CertificateDef* def = findCertificate(certId);
      return "sign: applied " + std::string(def ? def->name : certId) + " (static now " +
             std::to_string(total) + ", level " + std::to_string(staticLevelForTotal(total)) + ")";
    };
    b.listCertificates = [this] {
      const int32_t total = playerAdapter_.staticCharge();
      std::string out = "certificates (you: " + std::to_string(total) + " static, level " +
                        std::to_string(staticLevelForTotal(total)) + "):";
      for (const CertificateDef& d : certificateRegistry())
        out += "\n  " + std::string(d.id) + " - " + std::string(d.name) + ", " +
               std::to_string(d.levelCost) + " levels";
      return out;
    };
    b.spawnDeceitor = [this] { return spawnDeceitorCmd(); };
    b.deceitorInteract = [this] { return deceitorInteractCmd(); };
    b.deceitorAttack = [this] { return deceitorAttackCmd(); };
    b.flyToggle = [this](const std::string& arg) { return toggleFly(arg); };
    b.map = [this](const std::string& id) {
      Dimension* d = registry_.find(id);
      if (!d) return "map: unknown dimension '" + id + "'";
      if (d == active_) return "map: already in '" + id + "'";
      return switchDimension(id) ? "map: switched to '" + d->id + "'" : "map: switch failed";
    };
    b.dimensionIds = [this] {
      std::vector<std::string> ids;
      for (const auto& d : registry_.all()) ids.push_back(d->id);
      return ids;
    };
    b.save = [this] {
      // Honest reporting (audit LOW): saveWorld is a no-op without a save dir
      // (--no-save, --bench, headless); don't claim success then.
      return saveWorld("console", false) ? std::string("save: world saved")
                                         : std::string("save: no save directory");
    };
    b.pauseToggle = [this] {
      // The console is reachable in the menu; unpausing there breaks the
      // menu=>paused invariant (audit MEDIUM) - no-op instead.
      if (menuState_ == MenuState::cMenu) return std::string("pause: unavailable in the menu");
      setPaused(!paused_);
      return std::string(paused_ ? "pause: paused" : "pause: resumed");
    };
    // Tick debugger: freeze/step/unfreeze the sim thread's tick engine.
    // Creative-mode contraption debugging, tick by tick. Ships in alpha.
    // All three route through the pause state machine (P2b-2, CH-H7/G-M2):
    // no binding touches sim_->setPaused() directly any more.
    b.tickFreeze = [this] {
      if (!sim_) return std::string("tick: no sim thread");
      setPauseState(PauseState::TickFrozen);
      return std::string("tick: frozen");
    };
    b.tickUnfreeze = [this] {
      if (!sim_) return std::string("tick: no sim thread");
      // The pause menu (or launch menu) owns the pause while it is up:
      // unfreezing must NOT run the sim behind it (CH-H7). The freeze is
      // still in effect; resuming play is what unfreezes.
      if (menuState_ == MenuState::cMenu)
        return std::string("tick: still frozen (the launch menu is up; Play unfreezes)");
      if (pauseState_ == PauseState::MenuPaused)
        return std::string("tick: still frozen (the pause menu is up; resume unfreezes)");
      if (pauseState_ == PauseState::Playing) return std::string("tick: running");
      // TickFrozen / HsFrozen: entering Playing clears a latched handshake
      // freeze first (setPauseState), then unpauses â€” never the reverse.
      setPauseState(PauseState::Playing);
      return std::string("tick: running");
    };
    b.tickStep = [this](uint64_t n) {
      if (!sim_) return std::string("tick: no sim thread");
      // Stepping from live play freezes first (the machine owns it; the
      // sim's own stepTicks auto-pause agrees). Stepping while a menu is
      // up is the debugger's explicit act: the state stays MenuPaused.
      if (pauseState_ == PauseState::Playing) setPauseState(PauseState::TickFrozen);
      const uint64_t before = sim_->stats().worldTick;
      sim_->stepTicks(n);
      // Report the ticks ACTUALLY advanced (worldTick delta), not the
      // request: wait for the stepped iteration to publish, then measure.
      sim_->waitLoops(2, std::chrono::milliseconds(2000));
      const uint64_t advanced = sim_->stats().worldTick - before;
      std::string msg = "tick: stepped " + std::to_string(advanced) + (advanced == 1 ? " tick" : " ticks");
      if (advanced != n)
        msg += " (requested " + std::to_string(n) + " â€” the sim did not advance the full count)";
      return msg;
    };
    // Hot-swap refresh: apply staged block-definition updates between ticks.
    // The console command is debug-only (registered #ifndef NDEBUG); the
    // binding stays wired unconditionally but is unreachable in release.
    b.hotswapRefresh = [this] {
      if (!sim_) return std::string("hotswap: no sim thread");
      auto& reg = mutableBlockRegistry();
      if (reg.content().isSealed()) return std::string("hotswap: registry is sealed");
      size_t n = 0;
      sim_->runBetweenTicks([&] { n = reg.content().refreshStaged(); });
      if (n == 0) return std::string("hotswap: no staged updates");
      return std::string("hotswap: applied ") + std::to_string(n) +
             " staged block update(s), epoch " + std::to_string(reg.content().definitionEpoch());
    };
    b.quit = [this] { quit_ = true; };
    b.screenshot = [this](const ScreenshotRequest& req) { return requestScreenshot(req); };
    b.locateBiome = [this](const std::string& name) {
      if (!active_ || !active_->generator) return std::string("locatebiome: no active dimension");
      const auto& names = active_->biomes;
      uint8_t idx = 0;
      bool known = false;
      for (size_t i = 0; i < names.size() && i < 256; ++i)
        if (names[i] == name) {
          idx = uint8_t(i);
          known = true;
          break;
        }
      if (!known) {
        std::string msg = "locatebiome: unknown biome '" + name + "' (";
        for (size_t i = 0; i < names.size(); ++i) {
          if (i) msg += ", ";
          msg += names[i];
        }
        return msg + ")";
      }
      // The report names the biome for readability (scout.cpp reports the index).
      BiomeLocateResult r = locateBiome(*active_->generator, idx, double(player_.pos.x), double(player_.pos.z));
      if (!r.found) return std::string("locatebiome ") + name + ": " + r.message;
      char buf[192];
      std::snprintf(buf, sizeof buf, "locatebiome %s: nearest at (%d, %d), %.0f blocks away", name.c_str(),
                    r.x, r.z, r.distance);
      return std::string(buf);
    };
    b.genmap = [this](const GenMapRequest& req) {
      if (!active_ || !active_->generator) return std::string("genmap: no active dimension");
      int64_t cx = 0, cz = 0;
      if (req.center == "player") {
        cx = int64_t(std::floor(player_.pos.x));
        cz = int64_t(std::floor(player_.pos.z));
      } else {
        // "x,z": optional whitespace/signs, nothing else.
        size_t n = 0;
        try {
          const size_t comma = req.center.find(',');
          if (comma == std::string::npos) throw std::invalid_argument("no comma");
          cx = std::stoll(req.center.substr(0, comma), &n);
          if (n != comma) throw std::invalid_argument("bad x");
          std::string zs = req.center.substr(comma + 1);
          cz = std::stoll(zs, &n);
          if (n != zs.size()) throw std::invalid_argument("bad z");
        } catch (...) {
          return std::string("genmap: center must be \"player\" or \"x,z\"");
        }
      }
      OverviewMapSpec spec;
      spec.centerX = cx;
      spec.centerZ = cz;
      spec.zoom = req.zoom;
      spec.granularity = req.granularity;
      OverviewMapSource src;
      auto gen = active_->generator;
      src.sampleBiomes = [gen](const int32_t* xs, const int32_t* zs, size_t n, uint8_t* out) {
        gen->pipeline().sampleBiomes(xs, zs, n, out);
      };
      const uint64_t dimSeed = gen->dimensionSeed();
      src.structures = [gen, dimSeed](int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
        std::vector<MapMarker> markers;
        gen->pipeline().visitFeatures([&](std::string_view name, const procgen::IFeature& f) {
          const auto* s = dynamic_cast<const procgen::cStructureFeature*>(&f);
          if (!s) return;
          const auto rgb = structureMarkerColor(std::string(name));
          // B-L6 (P3-2): basesInRect returns CANDIDATE bases â€” place()'s
          // terrain gates (height band, slope, surface material, ...) can
          // still reject a candidate at generation time, so a marker means
          // "a structure may generate here", not "one did".
          for (const auto& b : s->basesInRect(dimSeed, x0, z0, x1, z1))
            markers.push_back({b[0], b[1], rgb[0], rgb[1], rgb[2]});
        });
        return markers;
      };
      src.biomeColors = mapPaletteForBiomes(active_->biomes);
      OverviewMap map;
      std::string warn;
      if (!renderOverviewMap(spec, src, map, warn)) return std::string("genmap: ") + warn;
      std::vector<uint8_t> png;
      if (!litholattice::mcp::encodePngRgb(map.width, map.height, map.rgb.data(), png))
        return std::string("genmap: PNG encode failed");
      std::string err;
      if (!writeFileBytes(req.path, png, &err)) return std::string("genmap: write failed: ") + err;
      std::string msg = "genmap: wrote " + req.path + " (" + std::to_string(map.width) + "x" +
                        std::to_string(map.height) + ", zoom " + std::to_string(req.zoom) + ", " +
                        std::to_string(req.granularity) + " blocks/px)";
      if (!warn.empty()) msg += " -- warning: " + warn;
      return msg;
    };
    registerGameCommands(consoleRegistry_, std::move(b));
  }
  // Deceitor hooks: the brain's window into live game state (player
  // pose, ground item entities, player health). All game-thread, read
  // at AI-advance time; empty when no Deceitor exists, so the default
  // game pays nothing for them.
  {
    DeceitorHooks h;
    h.playerPose = [this](double& x, double& y, double& z, double& fx, double& fz) {
      x = player_.pos.x;
      y = player_.pos.y;
      z = player_.pos.z;
      fx = std::cos(player_.yaw);  // lookDir XZ (player.cpp): (cos yaw, sin yaw)
      fz = std::sin(player_.yaw);
      return true;
    };
    h.nearestDrop = [this](double x, double y, double z, double radius) -> std::optional<DeceitorDrop> {
      DeceitorDrop best;
      double bestD = radius;
      bool found = false;
      for (litholattice::Entity* e : entities_.querySphere(x, y, z, radius, kEntityItem)) {
        if (e->dead) continue;  // consumed this frame, culled at the entity tick
        const double dx = e->x - x, dz = e->z - z;
        const double d = std::sqrt(dx * dx + dz * dz);
        if (!found || d < bestD) {
          found = true;
          bestD = d;
          best = DeceitorDrop{e->id, e->data, uint16_t(e->count), e->x, e->y, e->z};
        }
      }
      return found ? std::optional<DeceitorDrop>(best) : std::nullopt;
    };
    h.consumeDrop = [this](uint64_t id) { entities_.despawn(id); };
    h.tossReturn = [this](double x, double y, double z, uint32_t itemId, uint16_t count, double tx,
                          double ty, double tz) {
      const uint64_t id = entities_.spawn(kEntityItem, x, y, z);
      if (litholattice::Entity* e = entities_.get(id)) {
        e->data = itemId;
        e->count = count;
        const double dx = tx - x, dz = tz - z;
        const double d = std::sqrt(dx * dx + dz * dz);
        if (d > 1e-6) {
          e->vx = dx / d * 3.0;
          e->vz = dz / d * 3.0;
        }
        e->vy = 3.5;  // a friendly arc; cItemBehavior gravity lands it
        (void)ty;
      }
    };
    h.hurtPlayer = [this](int dmg) {
      playerAdapter_.enginePlayer().adjustVital(playerAdapter_.vitals().health, -dmg);
    };
    deceitorDirector_.setHooks(std::move(h));
  }
  // cCamera command (view-only, not a cheat â€” same standing as `map`):
  // `camera [first|third|follow|free]` drives the cGameCameraController.
  {
    CameraCommandBindings cb;
    cb.setModeByName = [this](std::string_view name) {
      const auto m = parseGameCameraMode(name);
      if (!m) return std::string("camera: unknown mode '") + std::string(name) + "' (first|third|follow|free)";
      camera_.setMode(*m);
      return std::string("camera: mode = ") + gameCameraModeName(*m);
    };
    cb.status = [this] { return std::string("camera: mode = ") + gameCameraModeName(camera_.mode()); };
    registerCameraCommands(consoleRegistry_, std::move(cb));
  }
  // Sprite system (presentation only, like the camera): starter defs from
  // every image source, and the `sprite` console command. Spawn/burst land
  // on the block the player is looking at (the sim's look target), or four
  // blocks ahead of the eye when nothing is in reach.
  spriteHost_.registerStarterSprites(resources_);
  {
    SpriteCommandBindings sb;
    auto spriteTarget = [this]() -> Vec3 {
      if (sim_) {
        const SimStats& st = sim_->stats();
        if (st.inReach) return Vec3{st.hx + 0.5f, st.hy + 1.6f, st.hz + 0.5f};
      }
      const Vec3 f = player_.forward();
      const Vec3 eye = playerEyePos(player_, locoSnap().eye);
      return Vec3{eye.x + f.x * 4.f, eye.y + f.y * 4.f, eye.z + f.z * 4.f};
    };
    sb.spawn = [this, spriteTarget](std::string_view name, int count) {
      if (!sim_) return std::string("sprite: no world loaded");
      return spriteHost_.spawnNamed(name, spriteTarget(), count);
    };
    sb.burst = [this, spriteTarget](std::string_view name) {
      if (!sim_) return std::string("sprite: no world loaded");
      return spriteHost_.burstNamed(name, spriteTarget());
    };
    sb.clear = [this] { return spriteHost_.clearAll(); };
    sb.list = [this] { return spriteHost_.listDefs(); };
    registerSpriteCommands(consoleRegistry_, std::move(sb));
  }
  // World queries ("SQL for voxels"): the pause-scan-restore backend.
  {
    cGameWorldQuery::Hooks h;
    h.chunks = [this]() -> litholattice::ChunkMap* { return active_ ? &active_->chunks : nullptr; };
    h.entities = [this]() -> litholattice::cEntityMgr* { return &entities_; };
    h.simRunning = [this] { return sim_ != nullptr; };
    h.setSimPaused = [this](bool p) {
      if (!sim_) return;
      // World-query scan pause (P2b-2): pausing for a scan is transient
      // and does not change the pause-machine state; the RESTORE routes
      // through applySimPause() so the sim returns to exactly the state
      // the machine owns â€” never to a stale snapshot of it.
      if (p) sim_->setPaused(true);
      else applySimPause();
    };
    h.gamePaused = [this] { return paused_; };
    h.waitSimSettled = [this](uint64_t n, std::chrono::milliseconds t) {
      return sim_ ? sim_->waitLoops(n, t) : true;
    };
    h.entityTypeName = [](uint32_t t) -> std::string {
      return t == kEntityItem ? "item" : t == kEntityDeceitor ? "deceitor" : "";
    };
    h.entityTypeId = [](const std::string& name) -> uint32_t {
      if (name == "item") return kEntityItem;
      if (name == "deceitor") return kEntityDeceitor;
      try {
        size_t n = 0;
        const unsigned long v = std::stoul(name, &n);
        if (n == name.size()) return static_cast<uint32_t>(v);
      } catch (...) {
      }
      return UINT32_MAX;
    };
    worldQuery_ = std::make_unique<cGameWorldQuery>(std::move(h));
    consoleRegistry_.setWorldQuery(worldQuery_.get());
  }
  // autoexec.lth: player startup script in the config dir; silent when absent.
  if (!cfg_.settingsPath.empty()) {
    const std::filesystem::path ae =
        std::filesystem::path(cfg_.settingsPath).parent_path() / "autoexec.lth";
    std::error_code ec;
    if (std::filesystem::exists(ae, ec)) {
      // Same symlink warning as config.lth above (deliberate exec refusal).
      std::error_code slEc;
      if (std::filesystem::is_symlink(ae, slEc))
        logWarn("game: '%s' is a symlink; console exec refuses symlinks, autoexec.lth not loaded",
                ae.string().c_str());
      else
        consoleRegistry_.executeFile("autoexec.lth");
    }
  }
  // Mesher option (PBR phase 2, Worker 3): boot-applied from the settings, honored at
  // world (re)load - switching mesh formats mid-session would need a full remesh plus
  // a GPU layout swap, so like tickRadius/vsync it takes effect on restart.
  useBinaryMesher_ = settings_.mesher == 1;
  if (useBinaryMesher_ && !renderer_.supportsPbrPath()) {
    logError("game: mesher=BinaryQuad selected but the renderer has no PBR path - falling back to PackedVertex");
    useBinaryMesher_ = false;
  }
  if (useBinaryMesher_) logInfo("game: mesher=BinaryQuad (chunk_pbr programs + classic fallback for non-cubes)");
  renderer_.setPbrTier(static_cast<litholattice::PbrTransparencyTier>(settings_.pbrTier));
  renderer_.setAmbientMode(ambientModeFromInt(settings_.ambientMode));  // flood-fill ambient, JD 2026-10-11
  renderer_.setFrustumMarginDeg(settings_.frustumMarginDeg);  // FOV culling margin, JD 2026-10-11
  litholattice::setLeavesFancy(resolvedLeavesFancy(settings_));
  litholattice::setFoliageDensity(resolvedFoliageDensity(settings_));  // 2026-10-18 (Crew Canopy, F-B)
  litholattice::setShellSolid(settings_.bulbShellSolid);  // 1551 (JD): Trace Bulb shell  // 2026-10-16 (JD leaf pass)
  renderer_.setModelDither(settings_.modelDither, settings_.modelDitherNear,
                            settings_.modelDitherFar);  // near-camera fade, JD 2026-10-13
  renderer_.setAaMode(settings_.aaMode.c_str());        // AA ladder, JD 2026-10-13
  renderer_.setTextureFiltering(settings_.texFilter, settings_.anisotropy);  // PROVISIONAL JD 2026-10-24
  renderer_.setBloom(resolvedBloom(settings_));          // Cortana 2026-10-16: render.bloom
  renderer_.setAo(settings_.ao);                         // Cortana 2026-10-16: render.ao
  {
    bool shim = settings_.shimmer;
    if (const char* e = std::getenv("LL_SHIMMER")) shim = std::atoi(e) != 0;
    renderer_.setShimmer(shim);  // Cortana 2026-10-16: render.shimmer; LL_SHIMMER=0|1 overrides
  }
  renderer_.setTexRes(std::min(settings_.texRes, 64));  // 2026-10-24: 128 = GPU pack; software caps at 64
 //                 // Cortana 2026-10-16: render.tex_res
  renderer_.setMipmaps(settings_.mipmaps);               // Cortana 2026-10-16: render.mipmaps
  renderer_.setEmissiveLight(settings_.emissiveLight);   // Cortana 2026-10-16: render.emissive_light
  renderer_.setAsciiMode(settings_.asciiMode, settings_.asciiCols);  // ASCII pipeline
  renderer_.setAsciiGlyphSet(settings_.asciiSet.c_str());            // ascii | systerian
  renderer_.setAsciiValueSpan(settings_.asciiValueSpan);  // JD amendment: full 0-255 span
  pushCullProtoConfig();  // culling-cache prototypes (JD 2026-10-11); all default off
  // PBR analytic lights: the provider scans chunk material bytes around the focus
  // column (cChunkEmissiveProvider, Worker 2); the game refreshes it per frame below.
  if (cfg.materials) emissiveProvider_ = std::make_unique<litholattice::cChunkEmissiveProvider>(cfg.materials);
  // Called on the main thread at load and on the sim thread afterwards; each job keeps the
  // materials it started with, so a hot reload never pulls them out from under a worker.
  swLodOn_ = renderer_.prefersGreedyMeshes() && !(std::getenv("LL_SW_LOD") && std::atoi(std::getenv("LL_SW_LOD")) == 0);
  if (const char* r = std::getenv("LL_SW_LOD_RINGS")) std::sscanf(r, "%d,%d,%d", &swLodRing_[0], &swLodRing_[1], &swLodRing_[2]);
  // PlayB (JD 2026-11-04, Elfie far-LOD brief): the ladder is hard-bounded to the view radius.
  // view <= ring 0: no LOD at all (no checks, no LOD meshing); else rings span ring 0 .. view.
  swLodRings_.setDetail(settings_.lodDetail);  // Archie Phase 1: the settings slider
  const bool lodLive = swLodRings_.configure(swLodRing_, effectiveStreamRadius(cfg_));
  if (swLodOn_) logInfo("software far LOD: rings %d/%d/%d chunks, view %d -> %s (effective %d/%d/%d; LL_SW_LOD=0 off)", swLodRing_[0], swLodRing_[1], swLodRing_[2],
                        swLodRings_.view(), lodLive ? "live" : "off (view inside the full-fidelity core)", swLodRings_.edge(0), swLodRings_.edge(1), swLodRings_.edge(2));
  submitMesh_ = [this](std::shared_ptr<MeshInput> in) {
    if (!viewGate_.inView(in->cx, in->cz)) {
      // A culled member still completes its publish batch. Dropping it silently stranded
      // every visible wire/lamp in that batch forever (batchSize included this member).
      if (in->batch && in->batchSize > 1 && in->batchSize <= 64) {  // matches kAtomicBatchMax at ingress
        ChunkMesh done; done.cx = in->cx; done.sy = in->sy; done.cz = in->cz;
        done.epoch = in->epoch; done.seq = 0;  // completion only, never upload geometry
        done.batch = in->batch; done.batchSize = in->batchSize;
        ++meshInFlight_; meshOut_.push(std::move(done));
      }
      return;  // still no mesh work beyond view distance
    }
    ++meshInFlight_;  // P8 quiescence
    litholattice::workTrace("submit", in->cx, in->sy, in->cz, in->seq);  // duplicate-work trace (LL_WORK_TRACE)
    submitMeshJob(pool_, [this, in, mats = resources_.materials(), binary = useBinaryMesher_] { meshOne(in, mats, binary); });
  };
  std::optional<PlayerMeta> savedPlayer;
  if (!cfg_.saveDir.empty()) {
    save_ = std::make_unique<cWorldSave>(cfg_.saveDir, cfg_.materials);
    save_->setSeed(cfg_.seed);  // SV-H2: cfg.seed already carries the persisted-seed override (main.cpp)
    save_->setWorldArea({simAreaShapeOf(cfg_), streamAreaShapeOf(cfg_), cfg_.tickRadius});  // Fix run 3: level.json v3 (main.cpp applies the saved area)
    {  // 2026-10-25 (JD): the world's named /tp spots (merged with the generated marks at lookup)
      std::string terr;
      userTpMarks_ = cWorldSave::readTpMarks(cfg_.saveDir, &terr);
      if (!terr.empty()) logError("save: %s (no saved tp spots)", terr.c_str());
      else if (!userTpMarks_.empty()) logInfo("save: %zu tp spot(s) restored", userTpMarks_.size());
      save_->setTpMarks(userTpMarks_);
    }
    std::string err;
    savedPlayer = save_->readPlayer(&err);
    if (!err.empty()) logError("save: %s (starting at spawn)", err.c_str());
    // Cortana 2026-10-16 [GAME] (JD): restore the persisted world clock (level.json v2). A world
    // saved before the clock existed migrates the v6 player play clock (ticks at 32 TPS), else
    // starts at tick 0 (00:00 of day 0).
    std::string cerr;
    if (const auto c = cWorldSave::readWorldClock(cfg_.saveDir, &cerr)) {
      worldClock_ = worldclock::Clock{c->tick, c->time};
    } else {
      if (!cerr.empty()) logError("save: %s (world clock starts at 0)", cerr.c_str());
      if (savedPlayer && savedPlayer->fileVersion == 6 && savedPlayer->playTicks > 0) {
        worldClock_ = worldclock::fromLegacyPlayTicks(savedPlayer->playTicks);
        logInfo("save: world clock migrated from the v6 play clock (%llu ticks)", (unsigned long long)savedPlayer->playTicks);
      }
    }
    // Review fix: arm level.json with the restored clock at once, so an eviction save
    // (saveColumns) before the first full save never rewrites level.json without it.
    save_->setWorldClock(worldClock_.tick, worldClock_.time);
    logInfo("world clock: tick %llu, day %u %s", (unsigned long long)worldClock_.tick,
            unsigned(worldclock::dayOf(worldClock_.time)), worldclock::hhmm(worldClock_.time).c_str());
  }
  active_ = registry_.find(savedPlayer && registry_.find(savedPlayer->dimension) ? savedPlayer->dimension : cfg.dimension);
  if (!active_) { logError("unknown dimension '%s' (wafer|organic)", cfg.dimension.c_str()); return; }
  loadActive();
  if (savedPlayer) {
    // Cortana 2026-10-15 1515 (GAME, Herominde H-05): validate the saved pose. A
    // non-finite value, or a position outside the world (y outside [0, kWorldHeight),
    // |x|/|z| > 2^24), keeps the dimension spawn that loadActive() set.
    {
      const auto& sp = *savedPlayer;
      constexpr float kXZLimit = 16777216.f;
      const bool finite = std::isfinite(sp.pos.x) && std::isfinite(sp.pos.y) && std::isfinite(sp.pos.z) &&
                          std::isfinite(sp.yaw) && std::isfinite(sp.pitch);
      const bool inWorld = finite && sp.pos.y >= 0.f && sp.pos.y < float(litholattice::kWorldHeight) &&
                           std::fabs(sp.pos.x) <= kXZLimit && std::fabs(sp.pos.z) <= kXZLimit;
      if (inWorld) {
        player_.pos = sp.pos; player_.yaw = sp.yaw; player_.pitch = std::clamp(sp.pitch, -1.5707963f, 1.5707963f);
      } else {
        logWarn("save: player.lat pose invalid (pos %g,%g,%g yaw %g pitch %g); starting at spawn",
                double(sp.pos.x), double(sp.pos.y), double(sp.pos.z), double(sp.yaw), double(sp.pitch));
      }
    }
    hot_ = std::clamp(savedPlayer->hot, 0, kHotbarSize - 1);
    if (savedPlayer->hasInventory) {
      // v4: the persisted inventory IS the player's inventory (P2b-1).
      player_.inventory = savedPlayer->inventory;
    } else if (!savedPlayer->hotbar.empty()) {
      // v1â€“v3 migration: the old raw-BlockId hotbar becomes real stacks in
      // the inventory hotbar. Full stacks: the old hotbar was an infinite
      // creative source, and a stack is the closest honest equivalent.
      for (size_t i = 0; i < size_t(kHotbarSize) && i < savedPlayer->hotbar.size(); ++i) {
        const BlockId b = savedPlayer->hotbar[i];
        if (b != kAir && b < blockRegistry().size())
          player_.inventory.hotbar()[i] = ItemStack{uint32_t(b), uint16_t(kMaxStack)};
      }
    }
    logInfo("save: player restored in '%s'", active_->id.c_str());
  }
  // IPlayer integration: restore the saved stat bases by name (v2 player.lat;
  // v1 saves carry none â€” all defaults), then mirror the restored player
  // into the engine IPlayer.
  if (savedPlayer) {
    playerAdapter_.applySavedStats(savedPlayer->stats);
    playerAdapter_.applySavedVitals(savedPlayer->vitals);
    // Cortana 2026-10-16: v7 mealtime prayer block. A v6 save's any-time prayers do not map to
    // the windows: it starts the current world day with no windows prayed.
    if (savedPlayer->fileVersion >= 7) {
      prayer_.prayerDay = savedPlayer->prayerDay;
      prayer_.windows = savedPlayer->prayerWindows;
      // 2026-10-16: a lost or older level.json clock must not sit days behind player.lat (the
      // windows would read as spent until the clock caught up). Resume at that day's 00:00.
      if (worldclock::dayOf(worldClock_.time) < prayer_.prayerDay) {
        logWarn("save: world clock (day %u) is behind player.lat (day %u); resuming at day %u 00:00",
                unsigned(worldclock::dayOf(worldClock_.time)), unsigned(prayer_.prayerDay), unsigned(prayer_.prayerDay));
        worldClock_.time = uint64_t(prayer_.prayerDay) * worldclock::kUnitsPerDay;
        if (save_) save_->setWorldClock(worldClock_.tick, worldClock_.time);
      }
    } else {
      prayer_.prayerDay = worldclock::dayOf(worldClock_.time);
      prayer_.windows = 0;
    }
  }
  playerAdapter_.syncFromGame(active_ ? mcpDimensionNumber(active_->id) : 0, 0.f);
  // Starting item (lore, LOCKED): the Fragment -- day one, first tick.
  // grantStartingItems is idempotent (grants only when not already held).
  // Since P2b-1 the inventory persists in player.lat (v4), so this is true
  // across restarts too: a restored player keeps their Fragment and is NOT
  // re-granted a duplicate (selftest-pinned), and a fresh spawn receives
  // it here, in hotbar slot 0.
  if (grantStartingItems(player_.inventory))
    logInfo("spawn: granted the starting item (the Fragment)");
  if (!savedPlayer) {
    // Fresh-spawn creative kit (P2b-1): the blocks the old hardcoded
    // hotbar array used to conjure from nothing, now as real stacks in the
    // real inventory UNDER the Fragment (slot 0). Full stacks (512): this
    // is the creative/testing kit, and placement consumes from the stack
    // like any other. The kit's 8th block (Switch) rides in main slot 0 â€”
    // the palette or a pickup can move it into the hotbar.
    const BlockId kit[] = {kWaferSilicon, kRedTrace, kBlueTrace, kRedTorch, kBlueTorch, kLamp, kRepeator};
    for (int i = 0; i < 7; ++i)
      player_.inventory.hotbar()[size_t(i + 1)] = ItemStack{uint32_t(kit[i]), uint16_t(kMaxStack)};
    player_.inventory.main()[0] = ItemStack{uint32_t(kSwitch), uint16_t(kMaxStack)};
  }
  if (testWorld()) {
    // Playtest 2026-10-16 (JD): every carry slot but the Fragment's (hotbar 1) holds a full stack,
    // components first (testWorldKit order); placing in the test world never consumes (infinite).
    // The E dialog shows all 42 slots (click one to swap it into the selected hotbar slot) and
    // still lists every registry block as a creative palette.
    auto slots = player_.inventory.allSlots();
    const auto kit = testWorldKit(kCarrySlots - 1);
    size_t k = 0;
    for (int i = 0; i < kCarrySlots && k < kit.size(); ++i) {
      if (i == 0 && !slots[0]->empty()) continue;  // the Fragment stays in hotbar slot 1
      *slots[size_t(i)] = ItemStack{uint32_t(kit[k++]), uint16_t(kMaxStack)};
    }
    hot_ = 1;
    logInfo("test world: %zu carry stacks seeded (of %d slots), infinite placement", k, kCarrySlots);
  }
  if (sim_) releaseTicksAfterIo("load");  // B9: world, components and player all restored - THEN ticks resume
  if (cfg_.capture) { showDebug_ = false; showControls_ = false; }  // playtest 2026-10-16: clean capture
  initScripts();
  // Boot-applied settings (tps / tick radius / EUV rode in on cfg_; the window ones were
  // applied by main.cpp before the window existed).
  audio_.post(AudioCommand::masterGain(settings_.masterVolume));
  audio_.setEnabled(!settings_.muted);
  ui_.setHudScalePref(settings_.hudScale);
}

// Game API exposed to scripts (the only bindings besides string + log).
static std::string scriptCurrentDimension(Game* g) { return g->dimensionId(); }

void Game::initScripts() {
  if (cfg_.scriptsDir.empty() || !scripts_.init()) return;
  // Script sandbox: bindings are gated by the binding allowlist and the mod's
  // permission bitmask (no raw engine access); refuse anything unlisted.
  scripts_.setModPermissions(kPermWorldRead | kPermUI | uint32_t(kPermConsole));
  consoleScriptCtx_ =
      litholattice::console::bindConsoleToScripts(scripts_, consoleRegistry_, "lithokraft");
  if (!consoleScriptCtx_) logError("game: console script bindings refused (allowlist/permissions)");
  scripts_.bindFunction("string currentDimension()", LL_SCRIPT_FN_OBJLAST(scriptCurrentDimension), this);  // fix 3c: generic on web
  if (uiScripts_.registerApi()) ui_.root().setScriptHost(&uiScripts_);
  if (scripts_.loadDirectory(cfg_.scriptsDir)) scripts_.callHook("void onLoad()");
  editor_ = std::make_unique<cScriptEditor>(ui_.root(), scripts_, cfg_.scriptsDir, [this] { scripts_.callHook("void onLoad()"); });
}

void Game::setBenchPath(const std::string& path) {
  benchPath_ = path;
  if ((path == "stream" || path == "soak" || path == "sprint" || path == "showcase") && active_ && active_->generator)
    stream_ = std::make_unique<cBenchStreamer>(renderer_, pool_, active_->generator, cfg_.worldChunks, active_->belowWorldFill, std::max(1, cfg_.streamRadius));
}

void Game::setBenchCamera(float t) {
  // Cortana 2026-10-26 (Archie item 5): in the test world the benches anchor at JD's v8 view
  // (62,266,46 looking down the rows, yaw 4.712 pitch -0.1) instead of the spawn, so still /
  // live / walk / orbit share one framing across builds. LL_BENCH_ANCHOR=legacy restores the
  // old spawn-relative poses. flyover stays screen-relative (already fixed in the test world).
  static const bool anchorV8 = !(std::getenv("LL_BENCH_ANCHOR") && std::string(std::getenv("LL_BENCH_ANCHOR")) == "legacy");
  const bool v8 = anchorV8 && testWorld();
  if (!benchSet_) {
    benchCenter_ = v8 ? Vec3{62.f, 266.f, 46.f} : player_.pos;
    benchSet_ = true;
    if (v8) logInfo("bench anchor: v8 62,266,46 (LL_BENCH_ANCHOR=legacy for the old spawn poses)");
  }
  // soak: alternate one minute of orbit and one minute of stream (streamed columns are dropped
  // again while orbiting: allocation churn for the drift numbers).
  if (benchPath_ == "flyover" && fb_.vertical) {  // 40 s loop: approach the wall, along the back wiring, stand view
    const float u = std::fmod(t, 40.f), midX = float(fb_.x0) + float(fb_.width) * 0.5f;
    const float midY = float(fb_.y + fb_.topY) * 0.5f, zw = float(fb_.wallZ);
    if (u < 14.f) {  // fly in toward the lamp wall from the front
      const float k = u / 14.f;
      player_.pos = {midX, midY + 6.f, zw - 70.f + 40.f * k};
      player_.yaw = 1.5708f; player_.pitch = -0.1f;
    } else if (u < 28.f) {  // behind the wall: climbing along the wire stairs, spine and links
      const float k = (u - 14.f) / 14.f;
      player_.pos = {float(fb_.x0) + 2.f + 30.f * k, float(fb_.y) + 4.f + (midY - float(fb_.y)) * k, zw + 9.f};
      player_.yaw = -1.5708f; player_.pitch = -0.15f;
    } else {  // the viewing stand
      player_.pos = {midX, midY + 2.f, zw - (10.f + 0.4f * float(fb_.width))};
      player_.yaw = 1.5708f; player_.pitch = 0.f;
    }
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "flyover" && fb_.size > 0) {  // 40 s loop: screen pass, cache / controller pass, stand view
    const float u = std::fmod(t, 40.f), W = float(std::max(fb_.size, 64 * fb_.columns));
    const float midX = float(fb_.x0) + W * 0.5f, top = float(fb_.y);
    if (u < 15.f) {  // high over the screen, flying north, looking down at it
      const float k = u / 15.f;
      player_.pos = {midX, top + 45.f - 15.f * k, float(fb_.screenZ1) + 30.f - (float(fb_.screenZ1 - fb_.screenZ0) + 30.f) * k};
      player_.yaw = -1.5708f; player_.pitch = -0.9f;
    } else if (u < 30.f) {  // low along the frame cache, Pulsors and controller ring
      const float k = (u - 15.f) / 15.f;
      player_.pos = {float(fb_.x0) + W * k, top + 7.f, float(fb_.cacheZ) + 12.f};
      player_.yaw = -1.2f; player_.pitch = -0.7f;
    } else {  // the viewing stand
      player_.pos = {midX, top + 16.f, float(fb_.screenZ1) + 12.f};
      player_.yaw = -1.5708f; player_.pitch = -0.55f;
    }
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "zigzag") {
    // 2026-10-30 (JD): deterministic boustrophedon over EVERY placed Examples pad, at a JD-like
    // sprint-fly speed (LL_ZIGZAG_SPEED blocks/s, default 20), 24 m above the floor, rows 24
    // blocks apart along x, alternating direction; loops. Same path every run (time-driven).
    static const float speed = [] { const char* e = std::getenv("LL_ZIGZAG_SPEED"); const float v = e ? float(std::atof(e)) : 20.f; return v > 0.f ? v : 20.f; }();
    if (benchExamples_.empty()) { benchPath_ = "live"; logError("bench zigzag: no Examples pads (use --test-world-examples); falling back to live"); return; }
    int x0 = benchExamples_[0].x0, x1 = benchExamples_[0].x1, z0 = benchExamples_[0].z0, z1 = benchExamples_[0].z1;
    for (const BenchBox& b : benchExamples_) { x0 = std::min(x0, b.x0); x1 = std::max(x1, b.x1); z0 = std::min(z0, b.z0); z1 = std::max(z1, b.z1); }
    const float ax = float(x0 - 8), bx = float(x1 + 8), az = float(z0 - 8), bz = float(z1 + 8), row = 24.f;
    const int rows = std::max(1, int(std::ceil((bz - az) / row)) + 1);
    const float len = bx - ax, legs = float(rows) * len + float(rows - 1) * row;
    static bool logged = false;
    if (!logged) { logged = true; logInfo("bench zigzag: %d pads, x %.0f..%.0f z %.0f..%.0f, %d rows, %.0f blocks/loop at %.1f b/s (%.1f s)", int(benchExamples_.size()), ax, bx, az, bz, rows, legs, speed, legs / speed); }
    float d = std::fmod(t * speed, 2.f * legs);
    if (d > legs) d = 2.f * legs - d;  // ping-pong so the loop stays continuous
    int r = 0;
    while (r < rows - 1 && d > len + row) { d -= len + row; ++r; }
    const bool fwd = (r % 2) == 0;
    float x, z = az + float(r) * row;
    if (d <= len) x = fwd ? ax + d : bx - d;
    else { x = fwd ? bx : ax; z += d - len; }
    player_.pos = {x, float(benchFloorY_) + 24.f, std::min(z, bz)};
    player_.yaw = d <= len ? (fwd ? 0.f : 3.14159f) : 1.5708f; player_.pitch = -0.6f;
    locoPlace(sim_, locomotion_, player_.pos);
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "still" || benchPath_ == "live") {
    // 2026-10-25 (Archie perf plan): "live" = a held, JD-like play view (testworld looking down
    // the rows to the horizon, ~350-600k tris) with the in-play HUD up, so bench numbers match
    // what JD sees. LL_BENCH_POSE still overrides the pose.
    if (v8) { player_.pos = {62.f, 266.f, 46.f}; player_.yaw = 4.712f; player_.pitch = -0.1f; }
    if (benchPath_ == "live") {
      if (!v8) { player_.pos = {58.5f, 270.f, 42.5f}; player_.yaw = 4.712f; player_.pitch = -0.1f; }
      menuState_ = MenuState::Playing; showHud_ = true;
      // 2026-10-25 (box): LL_BENCH_SHOT=N saves one screenshot on live-bench frame N (look checks).
      static int shotFrame = 0;
      if (const char* e = std::getenv("LL_BENCH_SHOT"); e && ++shotFrame == std::max(1, std::atoi(e))) logInfo("%s", requestScreenshot(ScreenshotRequest{}).c_str());
    }
    // Playtest 2026-10-16: hold the spawn pose (screenshots); LL_BENCH_POSE="x,y,z,yaw,pitch"
    // overrides it (e.g. a close view of the test world's Pattern ROM screen).
    // Cortana 2026-10-26 (item 5): the override is applied BEFORE the one-time pose log (the log
    // used to show the default pose), and the body is pinned there like the torture paths, so
    // the sim-thread locomotion (item 1) can't drift the held camera.
    bool poseOverride = false;
    if (const char* e = std::getenv("LL_BENCH_POSE")) {
      float v[5];
      if (std::sscanf(e, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5) {
        player_.pos = {v[0], v[1], v[2]}; player_.yaw = v[3]; player_.pitch = v[4];
        poseOverride = true;
      } else {
        static bool warned = false;
        if (!warned) { warned = true; logError("LL_BENCH_POSE: expected x,y,z,yaw,pitch, got '%s' (ignored)", e); }
      }
    }
    static bool loggedPose = false;  // JD Vulkan push: log the held pose once (screenshot framing)
    if (!loggedPose) { loggedPose = true; logInfo("bench %s pose: %.2f,%.2f,%.2f,%.4f,%.4f%s", benchPath_.c_str(), player_.pos.x, player_.pos.y, player_.pos.z, player_.yaw, player_.pitch, poseOverride ? " (LL_BENCH_POSE)" : ""); }
    locoPlace(sim_, locomotion_, player_.pos);
    benchCam_ = player_.pos;
    // 2026-11-01 (JD placement lag, measure only): LL_BENCH_PLACE=<block name> alternates place / break at the
    // look target every LL_BENCH_PLACE_EVERY frames (default 30) so LL_TICK_PROFILE can time each edit.
    if (const char* bp = std::getenv("LL_BENCH_PLACE"); bp && *bp) {
      static int pf = 0, np = 0; static BlockId pb = BlockId(-1);
      static const int every = [] { const char* e = std::getenv("LL_BENCH_PLACE_EVERY"); return e ? std::max(2, std::atoi(e)) : 30; }();
      if (pb == BlockId(-1)) {
        pb = kAir;
        if (const auto f = blockRegistry().findById(bp)) pb = *f;
        logInfo("bench place: '%s' -> block %d, every %d frames", bp, int(pb), every);
      }
      if (pb != kAir && ++pf % every == 0) {
        if ((np++ & 1) == 0) { EditCmd pc{EditCmd::Kind::Place, pb, 0}; sim_->edit(pc); }
        else sim_->edit({EditCmd::Kind::Break});
      }
    }
    return;
  }
  // Phase 4 torture paths (Direction JD, research Elfie, implementation Cortana). Camera only; sim untouched.
  if (benchPath_ == "whip") {  // T1: hold 20 up over the spawn; every 1.5 s a 180-degree pan in 0.12 s (max speed)
    const float u = std::fmod(t, 1.5f), k = std::min(1.f, u / 0.12f), s = k * k * (3.f - 2.f * k);
    const float base = float(int(t / 1.5f) & 1) * 3.14159265f;
    player_.pos = {benchCenter_.x, benchCenter_.y + 20.f, benchCenter_.z};
    player_.yaw = base + 3.14159265f * s; player_.pitch = -0.2f;
    locoPlace(sim_, locomotion_, player_.pos);  // pin the body too, or locomotion pulls the camera back each update
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "sprint") {  // T2/T3: from the spawn along +x at 60 blocks/s, 30 up (~4 chunk borders/s); the play streamer follows
    player_.pos = {benchCenter_.x + 60.f * t, benchCenter_.y + 30.f, benchCenter_.z};
    player_.yaw = 0.f; player_.pitch = -0.25f;
    locoPlace(sim_, locomotion_, player_.pos);
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "forward" || benchPath_ == "line") {
    // TheReefs 2026-11-14: straight-line play-streamer flight (JD fly-forward
    // hitch / 20-25 to 70 fps swing). Default 24 blocks/s, 20 up, +x from spawn.
    // LL_FORWARD_SPEED overrides. Does not use cBenchStreamer (sim/world unchanged).
    static const float speed = [] {
      const char* e = std::getenv("LL_FORWARD_SPEED");
      const float v = e ? float(std::atof(e)) : 24.f;
      return v > 0.f ? v : 24.f;
    }();
    static bool logged = false;
    if (!logged) { logged = true; logInfo("bench forward: +x at %.1f blocks/s, 20 up (play streamer)", double(speed)); }
    player_.pos = {benchCenter_.x + speed * t, benchCenter_.y + 20.f, benchCenter_.z};
    player_.yaw = 0.f; player_.pitch = -0.2f;
    locoPlace(sim_, locomotion_, player_.pos);
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "showcase") {  // --showcase wafer (JD 2026-10-30): side-on, slow +x pan, new columns stream in on the right
    static const float kSpeed = [] { const char* e = std::getenv("LL_SHOWCASE_SPEED"); return e ? float(std::atof(e)) : 10.f; }();
    const float startX = float(cfg_.worldChunks * 16), zc = float(cfg_.worldChunks * 8);
    player_.pos = {startX - 24.f + kSpeed * t, benchCenter_.y + 70.f, zc + 110.f};
    player_.yaw = -1.5708f; player_.pitch = -0.55f;  // look toward -z: +x is screen-right
    benchCam_ = player_.pos;
    return;
  }
  const bool streaming = benchPath_ == "stream" || (benchPath_ == "soak" && (int(t / 60.f) & 1));
  if (benchPath_ == "soak") t = std::fmod(t, 60.f);
  if (streaming) {  // start 2 chunks inside the fixed world's +x edge, fly out along +x
    const float startX = float(cfg_.worldChunks * 16 - 32), z = float(cfg_.worldChunks * 8);
    player_.pos = {startX + 32.f * t, benchCenter_.y + 30.f, z};
    player_.yaw = 0.f; player_.pitch = -0.3f;
    benchCam_ = player_.pos;
    return;
  }
  if (benchPath_ == "walk") {
    // Cortana 2026-10-15 1245: real-play bench. Eye height at the spawn
    // ground point, a slow 6-block circle looking along the horizon (the
    // view JD plays in: near ground geometry + the far wafer), one turn per
    // ~25 s. Same world/radius as play, so ~1,700 meshes like the playtest.
    const float a = t * 0.25f;
    player_.pos = {benchCenter_.x + 6.f * std::cos(a), benchCenter_.y + 0.2f, benchCenter_.z + 6.f * std::sin(a)};
    player_.yaw = a + 1.5708f; player_.pitch = -0.08f;
    benchCam_ = player_.pos;
    return;
  }
  // alu32: slow low orbit locked look-at tower centre. Others keep classic boom.
  const bool alu = (cfg_.benchScene == "alu32");
  const float a = t * (alu ? 0.12f : 0.25f);  // ~one turn / 52 s for alu32
  const float rad = alu ? 48.f : 24.f;
  const float elev = alu ? 3.f : 12.f;  // lower height for alu32
  player_.pos = {benchCenter_.x + rad * std::cos(a), benchCenter_.y + elev, benchCenter_.z + rad * std::sin(a)};
  if (alu) {
    // Look exactly at dead centre (footprint x/z mid, mid-height).
    const float dx = benchCenter_.x - player_.pos.x;
    const float dy = benchCenter_.y - player_.pos.y;
    const float dz = benchCenter_.z - player_.pos.z;
    const float horiz = std::sqrt(dx * dx + dz * dz);
    player_.yaw = std::atan2(dz, dx);  // forward XZ = (cos yaw, sin yaw) -> toward centre
    player_.pitch = std::atan2(dy, std::max(horiz, 1e-3f));
    // Once per second: forward vs direction-to-centre must be within 2 degrees.
    static int lastLookLog = -1;
    const int sec = int(t);
    if (sec != lastLookLog) {
      lastLookLog = sec;
      const Vec3 fwd = player_.forward();
      const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
      const float inv = (len > 1e-3f) ? (1.f / len) : 0.f;
      const float cx = dx * inv, cy = dy * inv, cz = dz * inv;
      const float dot = std::clamp(fwd.x * cx + fwd.y * cy + fwd.z * cz, -1.f, 1.f);
      const float errDeg = std::acos(dot) * 57.2957795f;
      logInfo("alu32 orbit look: pos=(%.1f,%.1f,%.1f) center=(%.1f,%.1f,%.1f) fwd=(%.3f,%.3f,%.3f) to=(%.3f,%.3f,%.3f) err=%.2f deg %s",
              player_.pos.x, player_.pos.y, player_.pos.z, benchCenter_.x, benchCenter_.y, benchCenter_.z,
              fwd.x, fwd.y, fwd.z, cx, cy, cz, errDeg, errDeg <= 2.f ? "OK" : "FAIL");
    }
  } else {
    player_.yaw = a + 1.5708f; player_.pitch = -0.25f;
  }
  benchCam_ = player_.pos;
}

Game::~Game() {
  logInfo("exit: game teardown (paused=%d)", int(paused_));
  cNemoViewWindow::instance().setWanted(false, nullptr);  // close the Nemo View window before the adapter goes
  holdTicksForIo("exit");  // B9: tick pause FIRST, then the exit save, then leave
  stream_.reset();
  streamer_.reset();  // waits out in-flight generation jobs (they may touch save_)
  logInfo("exit: streamer joined");
  saveWorld("exit", true);
  cExitWatchdog::noteSaved();  // 2026-10-31 close-hang: the watchdog (final exit only) counts from here
  // Persist archived cvars (the settings bridge) to config.lth.
  if (!cfg_.settingsPath.empty()) {
    const std::filesystem::path cfg(cfg_.settingsPath);
    if (cfg.has_parent_path())
      consoleRegistry_.saveArchived((cfg.parent_path() / "config.lth").string());
  }
  traceIo("leave:exit");  // B9: the sim is still held here; sim_.reset() below leaves without a tick
  aiHost_.unbindWorld();  // cWalkCache references active_->chunks; drop before the registry dies (B6)
  sim_.reset();
  pool_.waitIdle();  // mesh jobs reference meshOut_
  if (litholattice::cLfJobQueue::enabled()) litholattice::cLfJobQueue::shared().waitIdle();  // LL_LF_UPLOAD jobs capture this
}

// 2026-10-25 (JD J3): teleport <x y z> (absolute or ~relative) | <name>. Names: spawn, then the marks of
// this load (testarea, circuits, Examples pads); exact match first, then a unique prefix ("pixel").
// Writes player_.pos like kill does; the locomotion body re-seats on the external write (see update()).
std::string Game::teleportCmd(const std::vector<std::string>& args) {
  const std::string dim = active_ ? active_->id : std::string();
  auto names = [this, &dim] {
    std::string s = "spawn";
    for (const auto& m : userTpMarks_)
      if (m.dim == dim) s += ", " + m.name;
    for (const auto& [k, p] : tpMarks_) s += ", " + k;
    return s;
  };
  auto lower = [](const std::string& in) {
    std::string o;
    for (char ch : in) o += std::isalnum(static_cast<unsigned char>(ch)) ? char(std::tolower(static_cast<unsigned char>(ch))) : '-';
    return o;
  };
  // 2026-10-25 (JD): tp set <name> (here) / tp del <name>: this world's own spots, saved in level.json
  if (args.size() == 2 && (args[0] == "set" || args[0] == "del")) {
    const std::string n = lower(args[1]);
    if (n.empty() || n == "spawn" || n == "set" || n == "del") return "teleport: '" + args[1] + "' is a reserved name";
    auto it = std::find_if(userTpMarks_.begin(), userTpMarks_.end(), [&](const cWorldSave::TpMark& m) { return m.name == n && m.dim == dim; });
    std::string msg;
    if (args[0] == "set") {
      cWorldSave::TpMark m{n, dim, player_.pos.x, player_.pos.y, player_.pos.z};
      if (it != userTpMarks_.end()) *it = m; else userTpMarks_.push_back(m);
      char buf[160];
      std::snprintf(buf, sizeof buf, "teleport: spot '%s' set at (%.1f, %.1f, %.1f)", n.c_str(), m.x, m.y, m.z);
      msg = buf;
    } else {
      if (it == userTpMarks_.end()) return "teleport: no saved spot '" + n + "' here";
      userTpMarks_.erase(it);
      msg = "teleport: spot '" + n + "' deleted";
    }
    if (save_) save_->setTpMarks(userTpMarks_);
    else msg += " (this world is not saved: the spot lasts until exit)";
    return msg;
  }
  auto go = [this](const Vec3& p, const std::string& what) {
    player_.pos = p;
    char buf[160];
    std::snprintf(buf, sizeof buf, "teleport: %s (%.1f, %.1f, %.1f)", what.c_str(), p.x, p.y, p.z);
    return std::string(buf);
  };
  if (args.empty()) return "usage: tp <x y z> | <name>   (~ = relative)   names: " + names();
  if (args.size() == 3) {
    const float cur[3] = {player_.pos.x, player_.pos.y, player_.pos.z};
    float v[3];
    for (int i = 0; i < 3; ++i) {
      const std::string& s = args[size_t(i)];
      const bool rel = !s.empty() && s[0] == '~';
      const std::string num = rel ? s.substr(1) : s;
      char* end = nullptr;
      const float f = num.empty() ? 0.f : std::strtof(num.c_str(), &end);
      if (!num.empty() && (end == num.c_str() || *end != '\0' || !std::isfinite(f))) return "teleport: bad coordinate '" + s + "' (number or ~[number])";
      v[i] = rel ? cur[i] + f : f;
    }
    return go(Vec3{v[0], v[1], v[2]}, "to");
  }
  if (args.size() != 1) return "usage: tp <x y z> | <name>";
  std::string want;
  for (char ch : args[0]) want += char(std::tolower(static_cast<unsigned char>(ch)));
  if (want == "spawn") return go(spawnPoint_, "spawn");
  for (const auto& m : userTpMarks_)  // saved spots first: a /tp set name shadows a generated one
    if (m.dim == dim && m.name == want) return go(Vec3{m.x, m.y, m.z}, m.name);
  for (const auto& [k, p] : tpMarks_)
    if (k == want) return go(p, k);
  const std::pair<std::string, Vec3>* hit = nullptr;
  for (const auto& m : tpMarks_)
    if (m.first.rfind(want, 0) == 0) {
      if (hit) return "teleport: '" + args[0] + "' is ambiguous; names: " + names();
      hit = &m;
    }
  if (hit) return go(hit->second, hit->first);
  return "teleport: no target '" + args[0] + "' here; names: " + names();
}

void Game::loadActive() {
  const auto t0 = Clock::now();
  holdTicksForIo("load");  // B9: held from the first byte of the load (no sim exists yet; the new one inherits the hold)
  if (cfg_.benchScene != "default") {  // bench scenes: load and tick the whole scene
    cfg_.worldChunks = std::max(cfg_.worldChunks, cfg_.sceneSize / 16 + 12);
    if (cfg_.testWorldExamples) cfg_.worldChunks = std::max(cfg_.worldChunks, 36);  // 2026-10-24: room for the Examples grid
    if (cfg_.dimension == "flatlab") cfg_.worldChunks = std::max(cfg_.worldChunks, 56);  // 2026-10-30 (JD): Flat Lab fits every Examples slot (x/z up to ~800)
    if (cfg_.dimension == "componenttest") cfg_.worldChunks = std::max(cfg_.worldChunks, cComponentTestWorld::minWorldChunks());  // 2026-11-12 (JD): no pad clipped
    cfg_.tickRadius = std::max(cfg_.tickRadius, cfg_.sceneSize / 32 + 4);
  }
  // 2026-10-26 (JD streaming hitches): the in-play background cap (main.cpp) is lifted for the
  // blocking world load, which nothing renders over, so load time is unchanged.
  const int bgCap = pool_.maxActive();
  pool_.setDefaultMaxActive(0);
  // Archie PlayG: when streaming is on, only pre-bake a Chebyshev disk around the expected
  // spawn (<< worldChunks^2). Far columns come from the streamer; authored Examples call
  // generateMissing for their footprints below. Target: flatlab first rebuild well under 500 ms.
  const auto tWorldgen = Clock::now();
  Spawn sp;
  const int extent = cfg_.worldChunks;
  int prebakeCols = extent * extent;
  if (cfg_.streaming && active_->generator) {
    const int spawnBlock = (cfg_.dimension == "flatlab") ? 288 : std::max(0, extent * 16 / 2);
    int scx = spawnBlock >> 4, scz = spawnBlock >> 4;
    if (cfg_.dimension == "componenttest") {  // 2026-11-12 (JD): the sync disk sits on the Component Test spawn aisle
      float cxs = 0, czs = 0, cyaw = 0;
      cComponentTestWorld::spawn(cxs, czs, cyaw);
      scx = int(cxs) >> 4; scz = int(czs) >> 4;
    }
    // Cover the sim tick area plus a small pad; streamRadius can be 32 (huge) so clamp the sync disk.
    const int rad = std::clamp(std::max(cfg_.tickRadius + 2, 8), 6, 10);  // Archie PlayG: keep sync disk small
    sp = active_->loadRadius(pool_, extent, scx, scz, rad);
    prebakeCols = int(active_->chunks.columns().size());
    logInfo("world build: loadRadius extent=%d centre=%d,%d r=%d -> %d columns in %.1f ms",
            extent, scx, scz, rad, prebakeCols, msBetween(tWorldgen, Clock::now()));
  } else {
    sp = active_->load(pool_, extent);
    logInfo("world build: load full %dx%d -> %zu columns in %.1f ms", extent, extent,
            active_->chunks.columns().size(), msBetween(tWorldgen, Clock::now()));
  }
  pool_.setDefaultMaxActive(bgCap);
  // Archie PlayG: columns present after the sync disk â€” Examples added later stay StagedSim
  // so sim scan()/first mesh stay on the near disk only (<<500 ms flatlab rebuild).
  std::unordered_set<uint64_t> prebakeKeys;
  prebakeKeys.reserve(active_->chunks.columns().size());
  for (const auto& [k, col] : active_->chunks.columns()) { (void)col; prebakeKeys.insert(k); }
  tpMarks_.clear();  // 2026-10-25 (JD J3): rebuilt below for this load
  std::vector<PatternRomScreen> roms;  // playtest 2026-10-16: handed to the new sim below
  std::vector<cSignalWorldSim::GatedSource> torture;  // 2026-10-24 (JD): LEDs held off by the torture lever
  int tortureLever[3] = {0, -1, 0};                    // y < 0 = no lever
  std::vector<cSignalWorldSim::LeverPos> sceneLevers;  // 2026-10-24 9:15 PM (JD): one lever per scene
  int s16Box[4] = {0, -1, 0, -1};                      // wired screen 16 footprint (x0, x1, z0, z1)
  if (cfg_.benchScene == "testworld") {
    // Playtest 2026-10-16 (JD): flat pad, every registry block in labeled rows, Pattern ROM screen.
    const int sx = int(std::floor(sp.x)), sz = int(std::floor(sp.z));
    const int floorY = std::max(int(std::floor(sp.y)) - 1, active_->chunks.surfaceY(sx, sz));
    const TestWorldLayout tw = buildTestWorld(active_->chunks, sx, floorY, sz, cfg_.sceneSize >= 64 ? 64 : 32);
    sp.x = tw.spawnX; sp.z = tw.spawnZ; sp.y = float(tw.floorY + 1); sp.yaw = tw.spawnYaw; sp.pitch = -0.32f;
    tpMarks_.push_back({"testarea", Vec3{sp.x, sp.y, sp.z}});  // J3; the Examples grid centre replaces it below
    logInfo("testworld: origin sx=%d sz=%d floorY=%d (screenshot poses: LL_BENCH_POSE)", sx, sz, tw.floorY);
    size_t n = 0;
    for (const TestWorldRow& r : tw.rows) {
      std::string names;
      for (BlockId b : r.blocks) { names += names.empty() ? "" : ", "; names += blockRegistry().def(b).name; }
      logInfo("test world: row %-8s z=%d  %zu: %s", r.label.c_str(), r.z, r.blocks.size(), names.c_str());
      n += r.blocks.size();
    }
    logInfo("test world: %zu of %zu registry blocks placed, %zu missing; floor y=%d; Pattern ROM screen %dx%d at (%d..%d, %d..%d, z=%d) scale %d; %d blocks",
            n - tw.missing.size(), blockRegistry().size() - 1, tw.missing.size(), tw.floorY, 16 * tw.screen.scale,
            16 * tw.screen.scale, tw.screen.x0, tw.screen.x0 + 16 * tw.screen.scale - 1, tw.screen.y0,
            tw.screen.y0 + 16 * tw.screen.scale - 1, tw.screen.z, tw.screen.scale, tw.placed);
    tpMarks_.push_back({"grove", Vec3{float(tw.leafDemoX - 14) + 0.5f, float(tw.floorY + 1), float(tw.leafDemoZ - 3) + 0.5f}});  // 2026-10-25 (JD): /tp grove
    logInfo("test world: crystal leaf grove (8 trees, red/blue-dominant) x %d..%d, y %d..%d, z %d..%d", tw.leafDemoX - 28, tw.leafDemoX,
            tw.floorY + 1, tw.floorY + 7, tw.leafDemoZ - 1, tw.leafDemoZ + 10);  // 2026-10-16 JD leaf pass
    for (BlockId b : tw.missing) logError("test world: could not place '%s'", blockRegistry().def(b).name.data());
    if (cfg_.testWorldExamples) {  // 2026-10-24 (JD playtest note 4): 6x6-chunk flat test area, one floor block per chunk
      ChunkMap& cw = active_->chunks;
      const auto& reg = blockRegistry();
      std::vector<BlockId> floors{kWaferSilicon};
      // Smoke 15:44: the flag filter only found Wafer Silicon, so pick plain full blocks by name (first match wins).
      for (const char* want : {"Stone", "Copper Block", "Ceramic", "Aluminum Block", "Dense Silicon", "Dirt", "Flecked Silicon"}) {
        for (int id = 1; id < int(reg.size()) && floors.size() < 6; ++id)
          if (reg.def(BlockId(id)).name == std::string_view(want) && BlockId(id) != kWaferSilicon) { floors.push_back(BlockId(id)); break; }
      }
      const int ccx = std::max(0, (sx >> 4) - 3), ccz = std::max(0, (sz >> 4) - 3);
      if (!tpMarks_.empty()) tpMarks_[0].second = Vec3{float((ccx + 3) * 16) + 0.5f, float(floorY + 1), float((ccz + 3) * 16) + 0.5f};  // J3: grid centre  // chunks -3..+2 around spawn (Manhattan <= 6, inside the 8 diamond); clamped to generated columns (smoke 15:46: z<0 is not generated)
      auto busy = [&](int x, int z) {  // keep existing pads: anything standing within one block
        for (int dz = -1; dz <= 1; ++dz)
          for (int dx = -1; dx <= 1; ++dx)
            for (int y = floorY + 1; y <= floorY + 2; ++y)
              if (cw.get(x + dx, y, z + dz) != kAir) return true;
        return false;
      };
      int refloored = 0;
      for (int j = 0; j < 6; ++j)
        for (int i = 0; i < 6; ++i) {
          const BlockId fb = floors[size_t((i % 3) + 3 * (j % 2)) % floors.size()];  // no two edge-neighbours match
          for (int z = (ccz + j) * 16; z < (ccz + j + 1) * 16; ++z)
            for (int x = (ccx + i) * 16; x < (ccx + i + 1) * 16; ++x) {
              const BlockId top = cw.get(x, floorY, z);
              const bool pad = (top == kWaferSilicon || reg.has(top, kCutsWire)) && cw.get(x, floorY - 3, z) == kWaferSilicon;
              if (pad && busy(x, z)) continue;
              if (!pad) for (int y = floorY + 1; y <= floorY + 24; ++y) cw.set(x, y, z, kAir);
              for (int y = floorY - 3; y < floorY; ++y) cw.set(x, y, z, kWaferSilicon);
              if (!reg.has(top, kCutsWire) || !pad) { cw.set(x, floorY, z, fb); cw.setState(x, floorY, z, 0); ++refloored; }
            }
        }
      // Cross-chunk run: Switch -> 62 Red Trace -> Lamp along the south chunk row (clear of the pads), crossing 3+ chunk borders.
      int rz = ccz * 16 + 8;
      const int rx = (ccx + 1) * 16 + 4;
      bool runOk = false;
      int blockX = 0, blockZ = 0;
      for (int zt = ccz * 16 + 2; zt <= ccz * 16 + 14 && !runOk; zt += 2) {  // first free row in the south chunk row
        runOk = true;
        for (int k = 0; k <= 63 && runOk; ++k) if (busy(rx + k, zt)) { runOk = false; blockX = rx + k; blockZ = zt; }
        if (runOk) rz = zt;
      }
      if (!runOk) logInfo("test world chunk grid: cross-chunk run blocked, last blocker near (%d,%d)", blockX, blockZ);
      if (runOk) runOk = cw.set(rx, floorY + 1, rz, kSwitch);
      for (int k = 1; k <= 62 && runOk; ++k) runOk = cw.set(rx + k, floorY + 1, rz, kRedTrace);
      if (runOk) runOk = cw.set(rx + 63, floorY + 1, rz, kLamp);
      std::string names;
      for (BlockId b : floors) { names += names.empty() ? "" : ", "; names += reg.def(b).name; }
      logInfo("test world chunk grid: chunks x %d..%d z %d..%d (blocks x %d..%d z %d..%d), %d floor cells, floors: %s", ccx, ccx + 5, ccz,
              ccz + 5, ccx * 16, (ccx + 6) * 16 - 1, ccz * 16, (ccz + 6) * 16 - 1, refloored, names.c_str());
      logInfo("test world chunk grid: cross-chunk run %s: switch (%d,%d,%d) -> 62 red trace -> lamp (%d,%d,%d), crosses chunk borders at x %d, %d, %d, %d",
              runOk ? "placed" : "SKIPPED", rx, floorY + 1, rz, rx + 63, floorY + 1, rz, (ccx + 2) * 16, (ccx + 3) * 16, (ccx + 4) * 16, (ccx + 5) * 16);
      // 2026-10-24 (JD "Yes, add it"): a small working wired screen, 16 lamps wide x 4 stages (12 lamp rows),
      // the same Memor -> Pulsor -> trace -> lamp shift register + frame cache as the Examples FRAMEBUFFER, east of the grid.
      // 2026-10-24 5:00 PM (JD "Yes, move it in"): south of the grid, chunks x 2..4 z 6..7, fully inside sim distance 6 of spawn chunk (3,2).
      const int fx = (ccx + 2) * 16 + 8, fz = (ccz + 6) * 16 + 2;
      const FramebufferLayout fb16 = buildFramebufferScene(cw, fx, floorY + 1, fz, 32, 16);
      logInfo("test world wired screen 16: lamps x %d..%d z %d..%d y %d (frame cache z %d, viewing stand y %d); %d blocks", fx + 8, fx + 24,
              fb16.screenZ0, fb16.screenZ1, floorY + 2, fb16.cacheZ, floorY + 15, fb16.blocks);
      tpMarks_.push_back({"screen16", Vec3{float(fx + 16) + 0.5f, float(floorY + 1), float(fz - 2) + 0.5f}});  // 2026-10-25 (JD): /tp screen16
      s16Box[0] = fx - 4; s16Box[1] = fx + 40; s16Box[2] = fz - 4; s16Box[3] = fz + 48;
    }    if (cfg_.testWorldExamples) {  // 2026-10-24 (JD): playtest builds only
      ChunkMap& cw = active_->chunks;
      // 2026-10-24 5:11 PM (JD): every torture / stress circuit stays off until the torture lever is
      // switched on. Their LED sources are lifted out here (air) and handed to the sim, which puts them
      // back while the lever is on; the Pattern ROM screen stops with them. Shared builders unchanged.
      auto gateBox = [&](int x0, int x1, int z0, int z1, int group = 0) {
        int n = 0;
        for (int z = z0; z <= z1; ++z)
          for (int x = x0; x <= x1; ++x)
            for (int y = floorY - 8; y <= floorY + 72; ++y) {  // 2026-10-24: PATTERN / WALL PIXEL drivers sit under the floor (were ungated)
              const BlockId b = cw.get(x, y, z);
              // LEDs kick the clocks; Repeators / Pulsors are lifted too so self-running rings really stop.
              if (b != kRedTorch && b != kBlueTorch && b != kRepeator && b != kPulsor) continue;
              torture.push_back({x, y, z, b, cw.getState(x, y, z), group});
              cw.set(x, y, z, kAir); cw.setState(x, y, z, 0);
              ++n;
            }
        return n;
      };
      int fbX = 0, fbZ = 0; bool fbOk = false;
      {  // Archie PlayG: Examples slots may sit outside the sync loadRadius disk â€” generate just those columns.
        const auto tEx = Clock::now();
        // Grid origin matches buildTestWorldExamples (sx-40, screen.z+24); 6x2 slots of 80x104.
        const int gx0 = sx - 40, gz0 = tw.screen.z + 24;
        const int gx1 = gx0 + 6 * 80, gz1 = gz0 + 2 * 104;
        const int n = active_->generateMissing(pool_, gx0 >> 4, gz0 >> 4, (gx1 >> 4) + 1, (gz1 >> 4) + 1);
        logInfo("world build: Examples ensure (%d..%d,%d..%d) +%d columns in %.1f ms",
                gx0 >> 4, gx1 >> 4, gz0 >> 4, gz1 >> 4, n, msBetween(tEx, Clock::now()));
      }
      for (const TestWorldExample& e : buildTestWorldExamples(cw, tw, sx, sz, cfg_.worldChunks * 16)) {
        const int group = int(sceneLevers.size()) + 1;
        const int held = e.placed ? gateBox(e.x0, e.x1, e.z0, e.z1, group) : 0;
        // 2026-10-24 9:15 PM (JD): the scene's own lever in the gap NW of its corner (outside every
        // scene box): Switch -> 2 red Trace -> lamp, off by default. The master lever still drives all.
        if (held > 0) {
          const int lx = e.x0 - 5, ly = floorY + 1, lz = e.z0 - 4;
          cw.set(lx, ly, lz, kSwitch); cw.setState(lx, ly, lz, withAttach(0, 1));
          cw.set(lx + 1, ly, lz, kRedTrace); cw.set(lx + 2, ly, lz, kRedTrace); cw.set(lx + 3, ly, lz, kLamp);
          sceneLevers.push_back({lx, ly, lz});
          logInfo("test world examples: %s lever (%d,%d,%d) OFF, group %d", e.label.c_str(), lx, ly, lz, group);
        }
        if (e.placed) { benchExamples_.push_back({e.x0, e.z0, e.x1, e.z1}); benchFloorY_ = floorY; }  // 2026-10-30 zigzag bench
        if (e.placed) {  // J3: /tp <label>, standing just north of the pad, mid-width
          std::string key;
          for (char ch : e.label) key += std::isalnum(static_cast<unsigned char>(ch)) ? char(std::tolower(static_cast<unsigned char>(ch))) : '-';
          tpMarks_.push_back({key, Vec3{float(e.x0 + e.x1) * 0.5f + 0.5f, float(floorY + 1), float(e.z0 - 2) + 0.5f}});
        }
        logInfo("test world examples: %-12s x %d..%d z %d..%d  %s %d blocks%s%s | %d sources on the torture lever", e.label.c_str(), e.x0, e.x1, e.z0, e.z1,
                e.placed ? "placed" : "SKIPPED", e.blocks, e.note.empty() ? "" : " | ", e.note.c_str(), held);
        // FRAMEBUFFER viewing stand (buildFramebufferScene, size 64 at bx = x0 + 8, bz = z0 + 14): 9 x 9 at y + 14.
        if (e.placed && e.label == "FRAMEBUFFER") { fbX = e.x0 + 8 + 32; fbZ = e.z0 + 14 + 71; fbOk = true; }
      }
      if (s16Box[1] >= s16Box[0]) {
        const int group = int(sceneLevers.size()) + 1;
        const int held = gateBox(s16Box[0], s16Box[1], s16Box[2], s16Box[3], group);
        logInfo("test world wired screen 16: %d sources on the torture lever", held);
        if (held > 0) {  // 2026-10-24 9:15 PM (JD): its own lever, NW of its box
          const int lx = s16Box[0] - 5, ly = floorY + 1, lz = s16Box[2] - 4;
          cw.set(lx, ly, lz, kSwitch); cw.setState(lx, ly, lz, withAttach(0, 1));
          cw.set(lx + 1, ly, lz, kRedTrace); cw.set(lx + 2, ly, lz, kRedTrace); cw.set(lx + 3, ly, lz, kLamp);
          sceneLevers.push_back({lx, ly, lz});
          logInfo("test world wired screen 16: lever (%d,%d,%d) OFF, group %d", lx, ly, lz, group);
        }
      }
      if (fbOk) {
        // 2026-10-24 5:15 PM (JD): spawn on the centre of the FRAMEBUFFER viewing stand (top y = floorY + 15).
        const int standY = floorY + 15;
        sp.x = float(fbX) + 0.5f; sp.z = float(fbZ) + 0.5f; sp.y = float(standY + 1);
        // The torture lever on the stand, 2 west / 2 south of spawn, with a lamp beside it that lights while it is on.
        tpMarks_.push_back({"stand", Vec3{sp.x, sp.y, sp.z}});  // 2026-10-25 (JD): /tp stand (FRAMEBUFFER viewing stand, torture lever)
        tortureLever[0] = fbX - 2; tortureLever[1] = standY + 1; tortureLever[2] = fbZ - 2;
        cw.set(tortureLever[0], tortureLever[1], tortureLever[2], kSwitch);
        cw.setState(tortureLever[0], tortureLever[1], tortureLever[2], withAttach(0, 1));
        cw.set(tortureLever[0] - 1, tortureLever[1], tortureLever[2], kRedTrace);
        cw.set(tortureLever[0] - 2, tortureLever[1], tortureLever[2], kLamp);
        logInfo("test world: spawn on the FRAMEBUFFER stand (%.1f, %.1f, %.1f), chunk (%d, %d); torture lever (%d,%d,%d) OFF, %zu LEDs held",
                double(sp.x), double(sp.y), double(sp.z), fbX >> 4, fbZ >> 4, tortureLever[0], tortureLever[1], tortureLever[2], torture.size());
      } else {
        for (const auto& g : torture) { cw.set(g.x, g.y, g.z, g.id); cw.setState(g.x, g.y, g.z, g.st); }  // no stand: put the LEDs back
        torture.clear();
        logInfo("test world: FRAMEBUFFER slot not placed; no torture lever, spawn unchanged");
      }
    }
    roms.push_back(tw.screen);
  } else if (cfg_.benchScene == "boltlab") {
    // 2026-10-30 (JD): Bolt PCHIE Lab -- staged stations, one per test scenario, on the Flat Lab plain.
    const int sx = int(std::floor(sp.x)), sz = int(std::floor(sp.z));
    const int floorY = active_->chunks.surfaceY(sx, sz);
    const cBoltLabWorld::Layout bl = cBoltLabWorld().build(active_->chunks, sx, floorY, sz);
    sp.x = bl.spawnX; sp.z = bl.spawnZ; sp.y = float(floorY + 1); sp.yaw = 0.f; sp.pitch = -0.32f;
    tpMarks_.push_back({"lab", Vec3{sp.x, sp.y, sp.z}});
    for (const cBoltLabWorld::Station& s : bl.stations) {
      std::string key;
      for (char ch : s.id) key += char(std::tolower(static_cast<unsigned char>(ch)));
      tpMarks_.push_back({key, Vec3{s.spawnX, float(floorY + 1), s.spawnZ}});
      logInfo("bolt lab: station %-4s %-8s x %d..%d z %d..%d  (/tp %s)", s.id.c_str(), s.name.c_str(), s.x0, s.x1, s.z0, s.z1, key.c_str());
    }
    logInfo("bolt lab: %zu stations, %d blocks, floor y=%d, spawn (%.1f, %.1f)", bl.stations.size(), bl.placed, floorY, double(sp.x), double(sp.z));
  } else if (cfg_.benchScene == "componenttest") {
    // 2026-11-12 (JD t5295u / t5296u): Component Test -- one 3 x 3 chunk pad per Trace component on the
    // flat Wafer Silicon plain, component at the centre, its name spiraled in Ceramic (see component_test_world.h).
    int gx0 = 0, gz0 = 0, gx1 = 0, gz1 = 0;
    cComponentTestWorld::gridChunks(gx0, gz0, gx1, gz1);
    const auto tCt = Clock::now();
    const int gen = active_->generateMissing(pool_, gx0, gz0, gx1, gz1);  // pads outside the sync disk
    const int floorY = active_->chunks.surfaceY(int(std::floor(sp.x)), int(std::floor(sp.z)));
    const cComponentTestWorld::Layout cl = cComponentTestWorld().build(active_->chunks, floorY);
    sp.x = cl.spawnX; sp.z = cl.spawnZ; sp.y = float(floorY + 1); sp.yaw = cl.spawnYaw; sp.pitch = -0.32f;
    for (const cComponentTestWorld::Pad& p : cl.pads) {
      tpMarks_.push_back({p.id, Vec3{p.spawnX, float(floorY + 1), p.spawnZ}});
      logInfo("component test: pad %-20s %-28s x %d..%d z %d..%d  %s  (/tp %s)", p.label.c_str(), p.engineId.c_str(), p.x0,
              p.x0 + cComponentTestWorld::kPad - 1, p.z0, p.z0 + cComponentTestWorld::kPad - 1,
              p.block != kAir ? "placed" : (p.itemOnly ? "name-only (item)" : "name-only (not registered)"), p.id.c_str());
    }
    for (const std::string& u : cl.unfit) logError("component test: label '%s' does not fit its pad", u.c_str());
    logInfo("component test: %zu pads (%d components placed), %d label cells, floor y=%d, chunks %d..%d x %d..%d (+%d columns in %.1f ms), spawn (%.1f, %.1f)",
            cl.pads.size(), cl.components, cl.labelCells, floorY, gx0, gx1 - 1, gz0, gz1 - 1, gen, msBetween(tCt, Clock::now()),
            double(sp.x), double(sp.z));
  } else if (cfg_.benchScene == "pchiemaze") {
    // 2026-11-10 (JD t5033u, PCHIE P7): the Bolt PCHIE B1 grid from --pchie-maze FILE, as voxels on the
    // Flat Lab plain, Start cell on the spawn column. View-only example scene (see cPchieMazeWorld.h).
    const int sx = int(std::floor(sp.x)), sz = int(std::floor(sp.z));
    const int floorY = active_->chunks.surfaceY(sx, sz);
    std::string text;
    {
      std::ifstream f(cfg_.pchieMazeFile, std::ios::binary);
      std::ostringstream ss;
      if (f) ss << f.rdbuf();
      text = ss.str();
    }
    const cPchieMazeWorld::Layout ml = cPchieMazeWorld().build(active_->chunks, text, sx, floorY, sz);
    if (ml.ok) {
      sp.x = ml.spawnX; sp.z = ml.spawnZ; sp.y = float(floorY + 1); sp.yaw = 0.f; sp.pitch = -0.32f;
      tpMarks_.push_back({"maze", Vec3{sp.x, sp.y, sp.z}});
      logInfo("pchie maze: legend floor=%u wall=%u start=%u exit=%u key=%u door=%u origin=(%d,%d,%d) size=%dx%d start=(%d,%d) blocks=%d (/tp maze)",
              unsigned(kAluminumBlock), unsigned(kStone), unsigned(kCopperBlock), unsigned(kMazeExit), unsigned(kMazeKey),
              unsigned(kFrostedGlass), ml.originX, ml.floorY, ml.originZ, ml.width, ml.height, ml.startX, ml.startY, ml.placed);
    } else {
      logError("pchie maze: '%s' not built: %s", cfg_.pchieMazeFile.c_str(), ml.err.c_str());
    }
  } else if (cfg_.benchScene == "framebuffer" && !cfg_.screenVertical) {
    const int cx = int(sp.x), cz = int(sp.z) + 8, n = cfg_.sceneSize;
    const int sy = active_->chunks.surfaceY(cx, cz) + 1;
    fb_ = cfg_.screenVertical ? buildFramebufferWall(active_->chunks, cx - n / 2, sy, cz, n)
                              : buildFramebufferScene(active_->chunks, cx - n / 2, sy, cz, n);
    logInfo("bench scene: framebuffer, size %d, %d columns, %d blocks", n, fb_.columns, fb_.blocks);
  } else if (cfg_.benchScene == "wall-pixel" || (cfg_.benchScene == "framebuffer" && cfg_.screenVertical)) {
    const int cx = int(sp.x), cz = int(sp.z) + 20, n = cfg_.sceneSize <= 16 ? cfg_.sceneSize : 16;
    fb_ = buildWallPixelScene(active_->chunks, cx - n / 2, active_->chunks.surfaceY(cx, cz) + 1, cz, n, cfg_.sceneRows);
    logInfo("bench scene: wall-pixel, %d x %d pixels, %d blocks", fb_.width, std::clamp(cfg_.sceneRows, 1, 16), fb_.blocks);  // wall-pixel caps rows at 16
  } else if (cfg_.benchScene == "wire-torture") {
    const int cx = int(sp.x), cz = int(sp.z) + 8;
    fb_ = buildWireTorture(active_->chunks, cx - 25, active_->chunks.surfaceY(cx, cz) + 1, cz);
    logInfo("bench scene: wire-torture, %d blocks (fixtures + 24 x 48 every-tick stair field)", fb_.blocks);
  } else if (cfg_.benchScene == "sawtooth") {
    const int n = std::clamp(cfg_.sceneSize, 2, 64), rows = std::clamp(cfg_.sceneRows, 1, 64), cx = int(sp.x), cz = int(sp.z) + 20;
    fb_ = buildSawtoothWall(active_->chunks, cx - n / 2, active_->chunks.surfaceY(cx, cz) + 1, cz, n, rows, cfg_.pattern);
    logInfo("bench scene: sawtooth (%s), %d x %d pixels, %d blocks", cfg_.pattern.c_str(), n, rows, fb_.blocks);
  } else if (cfg_.benchScene == "pattern-wall") {
    const int n = std::clamp(cfg_.sceneSize & ~1, 16, 64), cx = int(sp.x), cz = int(sp.z) + 30;
    fb_ = buildPatternWall(active_->chunks, cx - n / 2, active_->chunks.surfaceY(cx, cz) + 1, cz, n, cfg_.pattern);
    logInfo("bench scene: pattern-wall (%s), %d x %d pixels, %d blocks", cfg_.pattern.c_str(), fb_.width, fb_.width, fb_.blocks);
  } else if (cfg_.benchScene == "facings") {  // 2026-10-31 (JD): readout overlay facing check
    const int cx = int(sp.x), cz = int(sp.z) + 6;
    const int placed = buildFacingsScene(active_->chunks, cx, active_->chunks.surfaceY(cx, cz) + 1, cz);
    logInfo("bench scene: facings at (%d, %d), %d blocks", cx, cz, placed);
  } else if (cfg_.benchScene == "redirector") {
    const int cx = int(sp.x), cz = int(sp.z) + 6, n = std::max(4, cfg_.sceneSize / 2);
    const int placed = buildRedirectorScene(active_->chunks, cx - n, active_->chunks.surfaceY(cx, cz) + 1, cz, n);
    logInfo("bench scene: redirector, %d redirectors, %d blocks", n, placed);
  } else if (cfg_.benchScene == "litho" || cfg_.benchScene == "litho-wired") {
    // Cortana 2026-10-15 1900: the hands-on lithography machine 10 blocks south of the spawn
    // (+Z), Etchor facing north toward the player. litho = unwired (JD wires it); litho-wired adds the wired
    // reference copy 16 blocks east.
    const int cx = int(sp.x), cz = int(sp.z) + 10;
    const int sy = active_->chunks.surfaceY(cx, cz) + 1;
    int placed = buildLithoMachine(active_->chunks, cx, sy, cz, false);
    if (cfg_.benchScene == "litho-wired") placed += buildLithoMachine(active_->chunks, cx + 16, sy, cz, true);
    logInfo("bench scene: %s, Etchor at (%d, %d, %d)%s, %d blocks", cfg_.benchScene.c_str(), cx, sy + 4, cz,
            cfg_.benchScene == "litho-wired" ? " + wired copy at x+16" : "", placed);
  } else if (cfg_.benchScene == "alu32") {
    const int cx = int(sp.x), cz = int(sp.z) + 8;
    const int sy = active_->chunks.surfaceY(cx, cz) + 1;
    const int n = std::max(1, cfg_.sceneCopies);
    // 4x4-chunk tower (64x64 footprint); keep tick radius covering the stack.
    constexpr int kFoot = 64;
    cfg_.worldChunks = std::max(cfg_.worldChunks, 16);
    cfg_.tickRadius = std::max(cfg_.tickRadius, 10);
    const Alu32Layout alu = buildAlu32Scene(active_->chunks, cx - kFoot / 2, sy, cz - kFoot / 2, n);
    // Orbit camera around tower mid-height (default --bench-path orbit).
    const float midY = float(sy) + float(std::max(1, alu.layers) * alu.pitch) * 0.5f + 1.f;
    benchCenter_ = Vec3{float(alu.x0) + float(kFoot) * 0.5f, midY, float(alu.z0) + float(kFoot) * 0.5f};
    logInfo("bench scene: alu32, copies %d (%dx%d x %d layers, pitch %d), seed 0x%llx, %d blocks; orbit center (%.1f,%.1f,%.1f)",
            alu.copies, alu.cols, alu.rows, alu.layers, alu.pitch, (unsigned long long)alu.seed, alu.blocks,
            benchCenter_.x, benchCenter_.y, benchCenter_.z);
    tpMarks_.push_back({"alu32", benchCenter_});
  } else if (cfg_.benchScene == "circuits" || cfg_.benchScene == "display") {
    const int cx = int(sp.x), cz = int(sp.z) + 8, n = cfg_.sceneSize;
    const int sy = active_->chunks.surfaceY(cx, cz) + 1;
    const int placed = cfg_.benchScene == "display" ? buildDisplayScene(active_->chunks, cx - n / 2, sy, cz, n)
                                                     : buildCircuitScene(active_->chunks, cx - n / 2, sy, cz, n);
    logInfo("bench scene: %s, size %d, %d blocks", cfg_.benchScene.c_str(), n, placed);
    tpMarks_.push_back({cfg_.benchScene, Vec3{float(cx) + 0.5f, float(sy), float(cz - 2) + 0.5f}});  // J3: /tp circuits
  }
  std::vector<uint8_t> signalState;
  int restored = 0;
  const auto tScenes = Clock::now();
  logInfo("world build: scenes+examples done in %.1f ms (since loadActive t0)", msBetween(t0, tScenes));
  {
    cStallTimer stall("world load (loadDimension)", lfIoTag());  // LL_STALL_TIMING
    restored = save_ ? save_->loadDimension(active_->id, active_->chunks, signalState) : 0;
  }
  traceIo("load-columns restored=" + std::to_string(restored));  // B9: no sim exists yet (old one reset / none)
  if (restored < 0)
    logError("save: '%s' load REFUSED (fail closed, see above); the world on disk is untouched and saves will refuse until the name table is repaired",
             active_->id.c_str());
  ++epoch_; totalSubs_ = 0; meshesReceived_ = 0;
  chunkColumns_ = active_->chunks.columns().size();  // read here: the sim thread owns the map later
  {  // fix 3b (JD t2279u): mesh the loaded columns nearest-first around the spawn chunk, not in
     // unordered_map order, so the world fills in from the player outwards (same rule as the streamer).
     // Archie PlayI (JD): sim ticking unchanged vs pre-PlayG. Every column within tickRadius is
     // Active from load so sim_->scan indexes it and Examples circuits tick on the first tick.
     // StagedSim is ONLY for columns OUTSIDE the sim area (d > tickRadius); non-sim mesh work for
     // in-sim non-prebake columns may still ride deferredMeshOrder_ (not render culling).
    const int scx = int(std::floor(sp.x / 16)), scz = int(std::floor(sp.z / 16));
    const int meshR = cfg_.streaming ? std::clamp(effectiveStreamRadius(cfg_), 6, 12) : 9999;
    const int tickR = cfg_.tickRadius;
    int meshQueued = 0, meshDeferred = 0, simActive = 0, stagedFar = 0;
    std::vector<std::pair<int, uint64_t>> order;
    order.reserve(size_t(2 * meshR + 1) * size_t(2 * meshR + 1));
    for (const auto& [k, col] : active_->chunks.columns()) {
      for (int sy = 0; sy < kSubChunks; ++sy)
        if (col->subs[sy]) ++totalSubs_;
      const int d = std::max(std::abs(col->cx - scx), std::abs(col->cz - scz));
      if (d <= tickR) {
        // Inside sim distance: Active from load (scan will index). Mesh now or defer.
        col->state.setStage(litholattice::ChunkStage::Active);
        ++simActive;
        if (d <= meshR && prebakeKeys.count(k)) {
          order.emplace_back(streamRequestPriority(col->cx - scx, col->cz - scz, cfg_.diamondStreamArea), k);
        } else {
          for (int sy = 0; sy < kSubChunks; ++sy) {
            if (!col->subs[sy]) continue;
            const int64_t mk = packPos(col->cx, sy, col->cz);
            if (deferredMeshKeys_.insert(mk).second) { deferredMeshOrder_.push_back(mk); ++meshDeferredTotal_; }
          }
          ++meshDeferred;
        }
      } else {
        // Outside sim area: StagedSim parks non-sim work until the player walks in.
        col->state.setStage(litholattice::ChunkStage::StagedSim);
        ++stagedFar;
        ++meshDeferred;
      }
    }
    std::sort(order.begin(), order.end());
    for (const auto& [pri, k] : order) {
      (void)pri;
      const auto& col = active_->chunks.columns().at(k);
      submitMeshColumn(col->cx, col->cz);
      ++meshQueued;
    }
    // PlayI verify: no in-sim column may remain StagedSim after load staging.
    int stagedInSim = 0;
    for (const auto& [k, col] : active_->chunks.columns()) {
      (void)k;
      const int d = std::max(std::abs(col->cx - scx), std::abs(col->cz - scz));
      if (d <= tickR && col->state.stage() == litholattice::ChunkStage::StagedSim) ++stagedInSim;
    }
    if (stagedInSim) logError("PlayI sim-tick restore FAIL: %d columns within tickR=%d still StagedSim", stagedInSim, tickR);
    else logInfo("PlayI sim-tick restore OK: simActive=%d stagedFar=%d (tickR=%d)", simActive, stagedFar, tickR);
    logInfo("world build: mesh submit queued=%d deferred=%d (r<=%d tickR=%d)", meshQueued, meshDeferred, meshR, tickR);
  }
  player_.pos = {sp.x, sp.y, sp.z}; player_.yaw = sp.yaw; player_.pitch = sp.pitch;
  {  // 2026-10-26 (JD spawn clip): the setups pick spawn from ONE column's surface (feet at the
     // column's top + 1), but the body is 2*halfWidth wide and a whole-number x/z spawn straddles
     // the neighbour columns, so a taller neighbour (a Capacitor Can cluster, a spire) left the
     // body inside solid cells. Lift the spawn until the full AABB is clear (sim not started yet,
     // so the map is ours to read), and log what was in the way.
    const double hw = locomotion_.controller().halfWidth(), ht = std::max(1.8, locomotion_.controller().currentHeight());
    BlockId hit = kAir;
    auto blocked = [&](double y) {
      const int x0 = int(std::floor(player_.pos.x - hw)), x1 = int(std::floor(player_.pos.x + hw - 1e-6));
      const int z0 = int(std::floor(player_.pos.z - hw)), z1 = int(std::floor(player_.pos.z + hw - 1e-6));
      const int y0 = int(std::floor(y)), y1 = int(std::floor(y + ht - 1e-6));
      for (int yy = y0; yy <= y1; ++yy)
        for (int zz = z0; zz <= z1; ++zz)
          for (int xx = x0; xx <= x1; ++xx) {
            const BlockId b = active_->chunks.get(xx, yy, zz);
            if (b != kAir && blockRegistry().has(b, kSolid)) { hit = b; return true; }
          }
      return false;
    };
    int lift = 0;
    BlockId first = kAir;
    while (lift < 64 && blocked(player_.pos.y)) {
      if (!lift) first = hit;
      player_.pos.y = std::floor(player_.pos.y) + 1.f;
      ++lift;
    }
    {  // Playtest 1 #1 (2026-10-26): a clear AABB can still be a sealed pocket (solid roof over the
       // head). Search upward for feet on a solid block, body-height air, and no solid cell in the
       // 24 cells above the head of the centre column; keep the old spot if none within 96.
      auto solidAt = [&](int x, int y, int z) { const BlockId b = active_->chunks.get(x, y, z); return b != kAir && blockRegistry().has(b, kSolid); };
      const int cx = int(std::floor(player_.pos.x)), cz = int(std::floor(player_.pos.z));
      auto roofed = [&](double y) { const int h = int(std::floor(y + ht)); for (int yy = h; yy < h + 24; ++yy) if (solidAt(cx, yy, cz)) return true; return false; };
      if (roofed(player_.pos.y)) {
        const float y0 = player_.pos.y;
        for (int dy = 1; dy <= 96; ++dy) {
          const double y = std::floor(y0) + dy;
          if (solidAt(cx, int(y) - 1, cz) && !blocked(y) && !roofed(y)) { player_.pos.y = float(y); break; }
        }
        if (player_.pos.y != y0) logInfo("spawn: sealed pocket at y=%.2f; moved to open air y=%.2f", double(y0), double(player_.pos.y));
        else logInfo("spawn: sealed pocket at y=%.2f; no open spot within 96 above, kept", double(y0));
      }
    }
    if (lift)
      logInfo("spawn: body overlapped '%s' at %.2f,%.2f,%.2f; lifted %d block(s) to y=%.2f",
              std::string(blockRegistry().def(first).name).c_str(), double(sp.x), double(sp.y), double(sp.z), lift,
              double(player_.pos.y));
  }
  spawnPoint_ = player_.pos;
  SimConfig sc;
  sc.tickRate = cfg_.tps; sc.radiusChunks = cfg_.tickRadius; sc.epoch = epoch_; sc.belowWorldFill = active_->belowWorldFill;
  sc.roundArea = cfg_.roundSimArea;  // F56-7 (Proposed by JD; co-implemented by JD and Cortana)
  sc.diamondArea = cfg_.diamondSimArea;  // diamond (JD 2026-10-23)
  {  // F56-6 (Cortana 2026-10-17): effective sim area (after scene clamps), logged at every sim start
    const int r = cfg_.tickRadius; long long n = 0;
    for (int dz = -r; dz <= r; ++dz) for (int dx = -r; dx <= r; ++dx)
      n += (cfg_.diamondSimArea ? (std::abs(dx) + std::abs(dz) <= r) : (!cfg_.roundSimArea || dx * dx + dz * dz <= r * r + r)) ? 1 : 0;
    logInfo("sim: tick area radius %d, %s, %lld chunk columns", r,
            cfg_.diamondSimArea ? "diamond (|dx|+|dz| <= r)" : cfg_.roundSimArea ? "round (dx^2+dz^2 <= r^2+r)" : "square", n);
  }
  sc.randomTicksPerSection = active_->randomTicksPerSection;
  sc.domains.push_back({"EUV", kMaxDomainHz, cfg_.euv});  // lithographed EUV-tier chips (stub)
  sc.timingHandshake = cfg_.timingHandshake;
  sc.handshakePhasesPerCycle = cfg_.handshakePhases;
  sc.handshakeWorkerThreads = cfg_.handshakeWorkers;  // --handshake-workers (0 = authority auto; G2 suspect-3 A/B knob, 2026-10-28)
#if defined(__wasm__)
  // Q1 trim (Archie exec summary, 2026-10-28, via JD): on the web/phone
  // layer the pull-worker pool defaults to 2 instead of the authority
  // auto (min(4, hw-1) = 4 on the target phone)... fewer futex/barrier
  // wakeups spread across the efficiency cores. An explicit
  // --handshake-workers still wins; desktop keeps the authority auto.
  if (sc.handshakeWorkerThreads == 0) sc.handshakeWorkerThreads = 2;
#endif
  if (sc.timingHandshake) logInfo("timing handshake: ON (phases/cycle %d; applies at this world load)", sc.handshakePhasesPerCycle);
  if (sc.timingHandshake && sc.handshakeWorkerThreads > 0)
    logInfo("timing handshake: pull-worker pool at %d workers", sc.handshakeWorkerThreads);
  placeRefunds_.clear();  // receipts belong to the sim that is replaced here
  clockSynced_ = false;   // 2026-10-16: a new sim's worldTick must not be diffed against the old one's
  lithokraft::randor::worldSeed().store(uint64_t(cfg_.seed), std::memory_order_relaxed);  // 2026-10-31 (JD): Randor rolls
  auto worldSim = std::make_unique<cSignalWorldSim>();
  for (PatternRomScreen& r : roms) worldSim->addPatternRom(std::move(r));  // before the sim thread owns it
  if (tortureLever[1] >= 0) {
    worldSim->setTortureGate(tortureLever[0], tortureLever[1], tortureLever[2], std::move(torture));
    worldSim->setSceneLevers(std::move(sceneLevers));
  }
  litholattice::setPickShapeFn(&pickShapeBoxes);  // 2026-10-25 (JD): model-shaped look / target ray
  sim_ = std::make_unique<cSimThread>(active_->chunks, blockRegistry(), std::move(worldSim), std::move(sc),
                                     submitMesh_);
  holdTicksForIo("load-sim");  // B9: the new sim is paused BEFORE components load and before it starts
  // PlayJ fix 3: null renderer has no mesh consumer â€” skip pinForMeshing on the sim remesh path.
  if (std::strcmp(renderer_.name(), "null") == 0) sim_->setMeshConsumer(false);
  // Cortana 2026-10-26 (Archie camera-jump decision): the player body steps here, on the sim
  // thread, at the end of every completed world tick (inside the tick's block-mutex window).
  sim_->setPlayerHook([this, sim = sim_.get(), lastStep = std::chrono::steady_clock::time_point{}](uint64_t wt) mutable {  // raw pointer: unique_ptr::reset nulls sim_ before the join
    // Cortana 2026-10-26 (item 5): pinned shots freeze the world at a fixed tick (LL_SW_SHOT set:
    // LL_BENCH_FREEZE_TICK, default 1024 = 2 s at 512 TPS; 0 = off) until the shot is written,
    // so animated blocks (clocks, Pulsors, screens) show the same state every run. Cap 30 s.
    // mutexfree pass 2a: the hook no longer sleeps here (that held blockMutex_ for up to 30 s). It requests a
    // park; the sim ends the pass at this tick and parks gate-free in the park hook below.
    if (g_shotFreezeTick && wt >= g_shotFreezeTick && !g_simFrozen.load(std::memory_order_relaxed) &&
        !litholattice::cSoftwareRenderer::shotTaken())
      sim->requestParkAfterPass();
    uint64_t placeSeq = locoSnap().placeSeq;
    bool reset = false;
    for (const plink::Cmd& c : g_plink.drain()) {
      if (c.kind == plink::Cmd::Place) { locomotion_.place({float(c.pos[0]), float(c.pos[1]), float(c.pos[2])}); placeSeq = c.seq; reset = true; }
      else locomotion_.setMode(c.mode);
    }
    const plink::InputMsg in = g_plink.input.load();
    const int tps = std::max(1, sim->simThreadWorldTps());
    // PlayI (JD, sluggish controls): step the body by wall-clock time since the last step (clamped),
    // so movement keeps real speed when ticks stall or TPS drops. LL_PLAYER_TICK_DT=1: old 1/TPS step.
    static const bool kTickDt = [] { const char* e = std::getenv("LL_PLAYER_TICK_DT"); return e && e[0] == '1'; }();
    const auto nowStep = std::chrono::steady_clock::now();
    double dtTick = 1.0 / double(tps);
    if (!kTickDt && lastStep.time_since_epoch().count() != 0)
      dtTick = std::clamp(std::chrono::duration<double>(nowStep - lastStep).count(), 0.0, 0.1);
    lastStep = nowStep;
    if (in.live && locomotion_.placed()) {
      const auto isSolid = [sim](int x, int y, int z) {
        const BlockId b = sim->getBlock(x, y, z);
        return b != kHighwayRamp && blockRegistry().has(b, kSolid);
      };
      locomotion_.tick(in.in, isSolid, dtTick);
      // Proposed by JD; co-implemented by JD and Cortana: the Highway Ramp is not a full cell for the player; its
      // 1-in-2 surface is applied after each tick (smooth walk-up, no step).
      namespace hw = litholattice::procgen::ramp::highway;
      litholattice::Entity& e = locomotion_.mutableBody();
      if (e.vy <= 0) {
        const int bx = int(std::floor(e.x)), bz = int(std::floor(e.z)), by = int(std::floor(e.y));
        for (int cy = by; cy >= by - 1; --cy) {
          const auto [b, st] = sim->getBlockAndState(bx, cy, bz);
          if (b != kHighwayRamp) continue;
          const double top = cy + hw::heightAt(uint32_t(st) & 7u, e.x - bx, e.z - bz);
          if (e.y < top + (locomotion_.flying() ? 0.0 : 0.2) && e.y > top - 0.75) { e.y = top; e.vy = 0; e.onGround = true; }
          break;
        }
      }
    } else {
      reset = true;  // not live: the body holds, display pins to it
    }
    plinkPublish(locomotion_, reset, wt, dtTick, placeSeq);
  });
  // mutexfree pass 2a: pinned-shot freeze PARK phase (sim thread, blockMutex_ NOT held; the post section has
  // already flushed this tick's remeshes). Same LL_BENCH_FREEZE_SEC cap; exits early on stop().
  sim_->setParkHook([sim = sim_.get()] {
    if (!g_shotFreezeTick) return;
    g_simFrozen.store(true, std::memory_order_release);
    const int freezeSec = [] { const char* e = std::getenv("LL_BENCH_FREEZE_SEC"); const int v = e ? std::atoi(e) : 30; return v > 0 ? v : 30; }();
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(freezeSec);
    while (sim->running() && !litholattice::cSoftwareRenderer::shotTaken() && std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    g_shotFreezeTick.store(0);
  });
  // Measure-only windowed determinism probe (LL_HASH_AT_TICK=<world tick>; Proposed by JD; co-implemented by JD and Cortana).
  // At exactly that world tick, on the sim thread inside its mutation window, hash every loaded column
  // (blocks + cell state, sorted by key) plus the signal state, and log it. Nothing is changed.
  if (const char* hashAt = std::getenv("LL_HASH_AT_TICK")) {
    // round 6 (Proposed by JD; co-implemented by JD and Cortana): FIXED column sets (squares of radius 4/8/16/24 chunks around one centre), each
    // logged with its loaded count, plus the signal state, so streaming distance cannot change what is hashed.
    const uint64_t at = std::strtoull(hashAt, nullptr, 10);
    ChunkMap* cm = &active_->chunks;
    cSignalSim* sig = &static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
    cSimThread* simp = sim_.get();
    int rcx = 3, rcz = 2;  // LL_HASH_REGION=cx,cz (default: the test world's spawn / circuit chunk)
    if (const char* e = std::getenv("LL_HASH_REGION")) std::sscanf(e, "%d,%d", &rcx, &rcz);
    sim_->setTickHook([at, cm, sig, simp, rcx, rcz](uint64_t tick) {
      if (tick != at) return;
      constexpr uint64_t kOff = 1469598103934665603ull;
      constexpr int kR[4] = {4, 8, 16, 24};
      auto mixTo = [](uint64_t& x, uint64_t v) { x ^= v; x *= 1099511628211ull; };
      uint64_t hr[4] = {kOff, kOff, kOff, kOff}, hs = kOff;
      size_t cnt[4] = {};
      std::vector<uint64_t> keys;
      keys.reserve(cm->columns().size());
      for (const auto& kv : cm->columns()) keys.push_back(kv.first);
      std::sort(keys.begin(), keys.end());
      for (uint64_t key : keys) {
        const auto* col = cm->columns().at(key).get();
        const int d = std::max(std::abs(col->cx - rcx), std::abs(col->cz - rcz));
        if (d > kR[3]) continue;
        uint64_t hc = kOff;
        mixTo(hc, key);
        for (int sy = 0; sy < kSubChunks; ++sy) {
          const auto* sc = col->subs[size_t(sy)].get();
          if (!sc) { mixTo(hc, 0xA1A1A1A1ull); continue; }
          for (int i = 0; i < kSubChunkVolume; ++i) { mixTo(hc, uint64_t(sc->blocks.at(size_t(i)))); mixTo(hc, uint64_t(sc->state[size_t(i)])); }
        }
        for (int r = 0; r < 4; ++r) if (d <= kR[r]) { mixTo(hr[r], hc); ++cnt[r]; }
      }
      for (const auto& [k, bytes] : sig->exportState()) { mixTo(hs, k); for (uint8_t b : bytes) mixTo(hs, b); }
      mixTo(hs, uint64_t(sig->componentCount())); mixTo(hs, tick);
      for (auto& x : hr) { mixTo(x, hs); }  // each fixed set also carries the signal state
      uint64_t hy = 0, hrd = 0, hin = 0;
      simp->handoffStats(hy, hrd, hin);
      logInfo("hash-at-tick: tick=%llu signal=%016llx | r4 %016llx (%zu/81) r8 %016llx (%zu/289) r16 %016llx (%zu/1089) r24 %016llx (%zu/2401) | columns=%zu components=%zu | handoff yields=%llu reads=%llu inserts=%llu",
              (unsigned long long)tick, (unsigned long long)hs, (unsigned long long)hr[0], cnt[0], (unsigned long long)hr[1], cnt[1],
              (unsigned long long)hr[2], cnt[2], (unsigned long long)hr[3], cnt[3], keys.size(), sig->componentCount(),
              (unsigned long long)hy, (unsigned long long)hrd, (unsigned long long)hin);
    });
    logInfo("hash-at-tick: armed for world tick %llu (LL_HASH_AT_TICK, measure only)", (unsigned long long)at);
  }
  traceIo("load-components restored=" + std::to_string(restored));  // B9: component state restored while held
  if (restored > 0) {
    if (!static_cast<cSignalWorldSim&>(sim_->simulation()).signal().importState(signalState))
      logError("save '%s': signal state unreadable (components restart idle)", active_->id.c_str());
    logInfo("save: '%s' restored %d columns", active_->id.c_str(), restored);
  }
  // Lamp afterglow (visual only): ~1/30 s at high TPS so 1-tick pulses reach a rendered frame.
  // The pattern wall lights each pixel once per colour sweep, so it holds a lamp for > one sweep.
  static_cast<cSignalWorldSim&>(sim_->simulation()).signal().setLampHoldTicks(
      cfg_.benchScene == "pattern-wall" ? 600 : cfg_.tps >= 256 ? cfg_.tps / 30 : 0);
  // cPlayer-centered chunk streaming: the pre-bake above only covers the fixed
  // [0, worldChunks)^2 square at the origin; the streamer generates/loads the
  // columns around the player from here on (see chunk_streamer.h).
  streamer_.reset();
  p5ResetChunkHandshake();            // P5: new streamer, new spawn spiral
  projectiles_.clear();               // projectiles belong to the old dimension
  projectiles_.clearRequestedChunks();  // the new streamer has seen no requests
  if (active_->generator && cfg_.streaming) {  // JD 2026-10-16: streaming toggle
    std::unordered_set<uint64_t> initial;
    initial.reserve(active_->chunks.columns().size());
    for (const auto& [k, col] : active_->chunks.columns()) initial.insert(k);
    streamer_ = std::make_unique<cChunkStreamer>(pool_, active_->generator, save_.get(), active_->id,
                                                effectiveStreamRadius(cfg_) /* fix3: sim area always inside stream */, initial);
    streamer_->setDiamond(cfg_.diamondStreamArea);  // diamond stream footprint (JD 2026-10-23)
    spawnSpiral_ = true;  // P5: ring-center bypass until the footprint first fills
  }
  // cAiWorld host (B6): bind to THIS dimension's live map + registry +
  // pool while the game thread still owns the map (before sim start).
  // Construction of the cAiWorld itself waits for the ai.enabled gate.
  aiHost_.bindWorld(active_->chunks, blockRegistry(), &pool_,
                    {int(std::floor(sp.x)), int(std::floor(sp.y)), int(std::floor(sp.z))});
  const auto tSim = Clock::now();
  logInfo("world build: pre-sim-start %.1f ms (scenes+io+mesh+sim-ctor)", msBetween(t0, tSim));
  sim_->start();  // from here the sim thread owns active_->chunks
  traceIo("sim-start");  // B9: started parked (held); the caller releases once the load is complete
  logInfo("dimension '%s' loaded: %zu columns, %d sub-chunks, %.1f ms (sim-start %.1f ms)", active_->id.c_str(),
          active_->chunks.columns().size(), totalSubs_, msBetween(t0, Clock::now()), msBetween(tSim, Clock::now()));
}

bool Game::switchDimension(const std::string& id) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  Dimension* next = registry_.find(id);
  if (!next || next == active_) return false;
  holdTicksForIo("exit-dimension");  // B9: pause, then save the old dimension, then leave it
  // 2026-10-31 (JD, Save World): async snapshot at the next tick boundary; wait (bounded) only
  // until the tick thread has copied the world out, then the old dimension may unload while the
  // pool worker writes. A boundary that never comes (frozen handshake) falls back to the sync save.
  if (saveWorld("dimension switch", false) && !saver_.settleSnapshot(sim_.get(), 2000))
    saveWorld("dimension switch", false, /*allowAsync=*/false);
  // P0a carry (P2b-2): a poisoned stop (handshake workers outstanding)
  // ABORTS the switch â€” destroying the cSimThread / unloading the old
  // dimension under a still-running worker is the C2 use-after-free.
  // The game stays in the current dimension; its sim thread is stopped
  // and the pause machine's state is untouched (still coherent).
  if (sim_ && !sim_->stop()) {
    logError("dimension switch ABORTED: sim stop failed (handshake workers outstanding); staying in '%s'",
             active_->id.c_str());
    releaseTicksAfterIo("exit-dimension aborted");
    return false;
  }
  streamer_.reset();            // its jobs reference the old dimension's generator/save id
  p5ResetChunkHandshake();
  sim_.reset();                 // stop ticking; main thread owns the old chunks again
  aiHost_.unbindWorld();        // its cWalkCache references the old dimension's chunks (B6)
  sound_.stopAll();
  // Design B Stage 2: the store clear is staged on the ring path (in ring
  // order, behind the old dimension's uploads it wipes) and immediate on
  // every other path. Spilled old-dimension uploads die here â€” the clear
  // covers the store, and nothing stale may be pushed behind it.
  ringSpillDroppedOnSwitch_ += ringSpill_.size();
  ringSpill_.clear();
  if (!renderer_.enqueueClearChunkMeshes()) spillRingUpload(2, {}, {});
  pendingOrder_.clear(); pendingMesh_.clear(); meshSeq_.clear(); uploadSortedAt_ = INT64_MIN; uploadSortedSize_ = 0;  // in-flight jobs are dropped by the epoch check
  deferredMeshOrder_.clear(); deferredMeshKeys_.clear();  // t3366u deferred keys belong to the old dimension
  mcpSenses_.clearMeshes();  // vision cache follows the GL renderer's clearChunkMeshes() above
  pbrPendingOrder_.clear(); pbrPendingMesh_.clear(); pbrMeshSeq_.clear();  // same for the PBR queue
  if (emissiveProvider_) emissiveProvider_->clear();  // new dimension: rescan
  logInfo("dimension switch: %s -> %s", active_->id.c_str(), next->id.c_str());
  active_->unload();
  active_ = next;
  loadActive();
  // Dimension switch must not silently unpause (audit MEDIUM): loadActive()
  // builds a fresh cSimThread (held by B9); releasing re-applies the pause machine.
  releaseTicksAfterIo("load");
  return true;
}

bool Game::saveWorld(const char* reason, bool wait, bool allowAsync) {
  if (!save_ || !sim_ || !active_) return false;
  cStallTimer stall(wait ? "world save (saveWorld, wait)" : "world save (saveWorld)", lfIoTag());  // LL_STALL_TIMING
  traceIo(std::string("save-begin:") + reason);  // B9 order trace
  // 2026-10-31 (JD, Save World): lock-free path. The tick thread snapshots at a tick boundary,
  // a pool worker writes; the sim is never stopped and this frame returns at once.
  if (!wait && allowAsync && cAsyncSaver::enabled() && sim_->running()) {
    if (saver_.busy()) {
      logInfo("save (%s): the previous save is still being written; skipped", reason);
      return false;
    }
    advanceWorldClock();
    save_->setWorldClock(worldClock_.tick, worldClock_.time);
    traceIo(std::string("save-write:") + reason);  // B9 order trace (async snapshot)
    if (saver_.begin(*sim_, *save_, active_->chunks, static_cast<cSignalWorldSim&>(sim_->simulation()).signal(), active_->id,
                     playerMetaForSave(), reason, [this](std::function<void()> job) { pool_.submit(std::move(job)); })) {
      lastSave_ = Clock::now();
      logInfo("save (%s): '%s' snapshot posted to the tick thread (sim keeps running; written on the pool)", reason, active_->id.c_str());
      return true;
    }
  }
  saver_.drain(sim_.get());  // a synchronous save follows (never overlaps) an async one
  const auto t0 = Clock::now();
  // P0a carry (P2b-2): cSimThread::stop() returns false when handshake pull
  // workers are still outstanding (poisoned). The snapshot would not be
  // consistent and start() will refuse until a retried stop succeeds, so
  // the save ABORTS here: nothing is written, nothing claims "on disk",
  // and the sim stays stopped under the pause machine's existing state.
  if (!sim_->stop()) {
    logError("save (%s) ABORTED: sim stop failed (handshake workers outstanding); nothing was written", reason);
    return false;
  }
  const std::unordered_set<uint64_t> dirty = sim_->takeChangedColumns();
  advanceWorldClock();  // Cortana 2026-10-16: bring the clock up to the stopped sim, then persist it
  save_->setWorldClock(worldClock_.tick, worldClock_.time);
  traceIo(std::string("save-write:") + reason);  // B9 order trace (sim stopped: every component captured here)
  const size_t n = save_->saveDimension(active_->id, active_->chunks, static_cast<cSignalWorldSim&>(sim_->simulation()).signal(),
                                        wait, &dirty);
  sim_->start();
  applySimPause();  // restore the machine's pause computation on the restarted sim
  const double pauseMs = msBetween(t0, Clock::now());
  // SV-H1 + C-4: a refused save (fail-closed name table) or a failed flush
  // is a FAILED save â€” report it as one, claim nothing is on disk, and do
  // not touch player.lat either (the world is not overwritten piecemeal).
  if (!save_->lastSaveOk()) {
    logError("save (%s) FAILED: '%s': %s", reason, active_->id.c_str(), save_->lastSaveError().c_str());
    return false;
  }
  PlayerMeta m = playerMetaForSave();
  std::string err;
  if (!save_->writePlayer(m, &err)) {
    logError("save (%s) FAILED: player: %s", reason, err.c_str());
    return false;
  }
  lastSave_ = Clock::now();
  logInfo("save (%s): '%s' %zu changed / component columns, sim paused %.2f ms%s", reason, active_->id.c_str(), n, pauseMs,
          wait ? ", on disk" : ", encoding + writing on the I/O thread");
  return true;
}

PlayerMeta Game::playerMetaForSave() {
  PlayerMeta m{player_.pos, player_.yaw, player_.pitch, active_->id, hot_};
  // P2b-1: the inventory is the store that persists (v4 inventory block).
  // The legacy hotbar field is DERIVED from its hotbar stacks â€” the block
  // each stack places (kAir for empty slots and non-block stacks) â€” and
  // writePlayer stores those as string ids (Appendix C M2).
  m.hotbar.reserve(kHotbarSize);
  for (const ItemStack& s : player_.inventory.hotbar()) m.hotbar.push_back(placedBlockForItem(s.itemId));
  m.inventory = player_.inventory;
  m.hasInventory = true;
  m.stats = playerAdapter_.statsForSave();    // IPlayer integration: stat bases by name
  m.vitals = playerAdapter_.vitalsForSave();  // IPlayer integration: vital values by name (v3 player.lat)
  m.prayerDay = prayer_.prayerDay;  // Cortana 2026-10-16: v7 mealtime prayer block
  m.prayerWindows = prayer_.windows;
  return m;
}

// 2026-10-25 (JD hitch, fix 1): the frame thread does not wait out a sim tick on the
// streaming paths. Try for blockMutex_; if the sim is mid-tick, skip this frame (work
// stays queued). Locomotion keeps a bounded blocking fallback (after kMaxBusySkips
// straight skips it blocks once, so a sim that re-locks back to back cannot starve
// the player). Stream pumps pass allowBlock=false: no forced wait, ever.
// 2026-10-26 (code review follow-up, JD ruling 3): every busy-deferral is counted and
// reported per second on the update-split line (fh defer/esc/blk/loco-unlocked).
// JD 2026-10-26 12:03 PM: NO forced escalation on stream pumps (allowBlock=false
// never blocks; columns stay queued until the sim is free); esc stays 0, kept in
// the line for log-format stability. Locomotion's unlocked fallback (handshake
// refused, not busy) is counted as g_fhLocoUnlocked. The frame thread waits only
// in locomotion's bounded fallback (counted as blk).
namespace {  // game thread only
// 2026-10-26 (JD F3 lag trace): LL_TRACE_F3=1 records one row per frame (movement step,
// player_.pos, HUD model, render camera, drawn XYZ text) into a ring and writes it to
// stderr once a second in a single write, so tracing I/O stays off the frame path.
// LL_TRACE_F3_WALK=1 additionally holds "forward" (2: + sprint, 3: + fly, climbing when blocked) for
// an unattended repro. Off by default: every hook is one cached bool test.
namespace f3t {
struct Row {
  uint64_t frame = 0, worldTick = 0, posTick = 0;
  char loco = '-';  // H held+stepped, U stepped unlocked, B skipped (sim busy), - body not live
  int ticks = -1;   // ticks consumed by the movement step this frame
  float pos[3] = {}, hud[3] = {}, cam[3] = {};
  std::string built, drawn;
  uint64_t drawnSeq = 0;
  size_t verts = 0;
};
static Row cur;
static std::vector<Row> ring;
static uint64_t frameNo = 0, lastPosTick = 0;
static std::chrono::steady_clock::time_point lastFlush = std::chrono::steady_clock::now();
static std::string xyzPart(const std::string& s) { const size_t a = s.find("XYZ "); return a == std::string::npos ? s : s.substr(a); }
static void endFrame() {  // called at the start of the next update(): the renderer has drawn frame `cur`
  if (cur.frame) {
    litholattice::f3trace::Drawn& d = litholattice::f3trace::drawn();
    { litholattice::cGateGuard l(d.m); cur.drawn = d.text; cur.drawnSeq = d.seq; cur.verts = d.verts; }
    ring.push_back(std::move(cur));
  }
  cur = Row{};
  cur.frame = ++frameNo;
  const auto now = std::chrono::steady_clock::now();
  if (now - lastFlush < std::chrono::seconds(1) && ring.size() < 2048) return;
  lastFlush = now;
  std::string out;
  out.reserve(ring.size() * 260);
  char b[512];
  for (const Row& r : ring) {
    const std::string bx = xyzPart(r.built), dx = xyzPart(r.drawn);
    std::snprintf(b, sizeof b,
                  "f3trace f=%llu wt=%llu posTick=%llu loco=%c/%d pos=%.2f,%.2f,%.2f hud=%.2f,%.2f,%.2f cam=%.2f,%.2f,%.2f "
                  "built=\"%s\" drawn#%llu=\"%s\" %s verts=%zu (no HUD mesh cache)\n",
                  (unsigned long long)r.frame, (unsigned long long)r.worldTick, (unsigned long long)r.posTick, r.loco, r.ticks,
                  r.pos[0], r.pos[1], r.pos[2], r.hud[0], r.hud[1], r.hud[2], r.cam[0], r.cam[1], r.cam[2], bx.c_str(),
                  (unsigned long long)r.drawnSeq, dx.c_str(), bx == dx ? "same" : "DIFF", r.verts);
    out += b;
  }
  ring.clear();
  std::fwrite(out.data(), 1, out.size(), stderr);
  std::fflush(stderr);
}
static int walk() { static const int w = [] { const char* e = std::getenv("LL_TRACE_F3_WALK"); return e ? std::atoi(e) : 0; }(); return w; }
}  // namespace f3t

uint64_t g_fhDeferrals = 0;     // busy-deferrals across all frameHold sites (per second; reset with the split line)
uint64_t g_fhEscalations = 0;   // always 0 since JD 2026-10-26 12:03 PM (escalation removed)
uint64_t g_fhBlocks = 0;        // locomotion bounded blocking holds taken
uint64_t g_fhLocoUnlocked = 0;  // locomotion steps run without the hold
}  // namespace
static bool frameHold(cSimThread& sim, ::litholattice::cGateLock<litholattice::cBlockGate>& hold, int& skips, bool& busy,
                      bool allowBlock = true) {
  constexpr int kMaxBusySkips = 8;
  if (allowBlock && skips >= kMaxBusySkips) { skips = 0; busy = false; ++g_fhBlocks; return sim.holdBetweenTicks(hold); }
  if (sim.tryHoldBetweenTicks(hold, &busy)) { skips = 0; return true; }
  if (busy) {
    ++skips;
    ++g_fhDeferrals;
  }
  return false;
}

// Chunk streaming (see chunk_streamer.h): request the columns in the load
// radius around the player, then insert the finished ones. The sim thread owns
// active_->chunks while it runs, so insertion takes the sim between ticks
// (holdBetweenTicks: the blockMutex_ hand-off); new columns are meshed through
// the same submitMesh_ path as the initial load. Build 80 (cycle-budget
// fairness): when the hold is refused (handshake frozen, or a worker still
// outstanding after a failed stop), the batch is DEFERRED â€” it stays queued
// and lands on a later pump. The pump never stop()/start()s the sim: that
// fallback parked the frame thread inside thread joins and worker respawns
// (the build-79 S3 starvation seesaw on device â€” present and sim starving
// each other by turns). Freeze recovery stays with the pause state machine
// (entering Playing clears the latch), exactly as before.
// 2026-10-25 stutter pass: stream churn counters for the once-a-second split line.
namespace { size_t g_colsAdded = 0, g_colsEvicted = 0, g_uploadsCapped = 0;
// TheReefs 2026-11-14 hitch profile (game thread only; no locks).
struct MeshSubmitSplit { double publishMs = 0, pinMs = 0; int columns = 0, slabs = 0; };
MeshSubmitSplit g_meshSubmitSplit;
inline double pumpMsSince(std::chrono::steady_clock::time_point t0) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}
}
int Game::swLodLevel(int d) const {
  return swLodRings_.level(d);  // PlayB: clamped to the view radius; 0 when LOD is not live
}
int Game::swLodFor(int cx, int cz) const {
  const int px = swLodPcx_.load(std::memory_order_relaxed), pz = swLodPcz_.load(std::memory_order_relaxed);
  return swLodLevel(std::max(std::abs(cx - px), std::abs(cz - pz)));
}

// Archie Phase 1 (JD 2026-11-04): bound meshing to the view. A centre / radius change retires
// every meshed column now beyond the view (tombstones -> the renderer drops its meshes) and meshes
// the deferred columns now inside it (bounded per frame, under the between-ticks sim hold).
void Game::pumpViewGate() {
  if (!sim_ || !active_ || !viewGate_.enabled()) return;
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  const bool moved = viewGate_.setCentre(pcx, pcz, effectiveStreamRadius(cfg_), cfg_.diamondStreamArea);
  static int sweepTick = 0;  // game thread only: a periodic sweep catches meshes the sim thread raced in
  if (!moved && !viewGateBacklog_ && ++sweepTick % 30 != 0) return;
  ::litholattice::cGateLock<litholattice::cBlockGate> simHold;
  static int gateSkips = 0;  // game thread only
  bool gateBusy = false;
  if (!frameHold(*sim_, simHold, gateSkips, gateBusy, /*allowBlock=*/false)) { viewGateBacklog_ = true; return; }
  // Every resident column is checked (a few hundred keys): meshes may also come from the sim
  // thread's remesh path, so the gate keeps only the set of columns it holds back.
  constexpr size_t kEnterPerPump = 12;
  size_t n = 0;
  viewGateBacklog_ = false;
  for (const auto& [key, col] : active_->chunks.columns()) {
    if (!col) continue;
    const int cx = col->cx, cz = col->cz;
    const bool in = viewGate_.inView(cx, cz), held = viewGate_.deferred(cx, cz);
    if (!in && !held) {  // left the view: its meshes go (tombstones), it re-meshes on return
      retireColumnMeshes(cx, cz, /*force=*/true);
      swLodMeshed_.erase(ChunkKey::make(cx, cz));
      viewGate_.defer(cx, cz);
      ++viewGateRetired_;
    } else if (in && held) {
      if (col->edgeShell) continue;  // Q12: promotes first (demoteFarColumns), then meshes
      if (n >= kEnterPerPump) { viewGateBacklog_ = true; continue; }
      ++n; ++viewGateEntered_;
      viewGate_.undefer(cx, cz);
      for (int sy = 0; sy < kSubChunks; ++sy) { meshSeq_.erase(packPos(cx, sy, cz)); pbrMeshSeq_.erase(packPos(cx, sy, cz)); }
      startFirstMesh(cx, cz);
    }
  }
  static int logEvery = 0;  // game thread only
  if (++logEvery % 120 == 1)
    logInfo("view gate: centre %d,%d radius %d | held %zu, retired %llu, entered %llu", pcx, pcz, viewGate_.radius(),
            viewGate_.deferredCount(), (unsigned long long)viewGateRetired_, (unsigned long long)viewGateEntered_);
}

// 2026-10-25 (box, triangle diet): re-mesh columns whose far-LOD ring changed (1-chunk hysteresis),
// a bounded batch per frame, under the same between-ticks sim hold the streaming insert uses.
void Game::pumpSwLod() {
  if (!swLodOn_ || !sim_ || !active_) return;
  // PlayB: a view-radius change re-clamps the rings; the loop below then reclassifies every column.
  if (const int v = effectiveStreamRadius(cfg_); v != swLodRings_.view()) swLodRings_.setView(v);
  if (!swLodRings_.live() && swLodMeshed_.empty()) return;  // LOD off: nothing still meshed at a LOD level
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  swLodPcx_.store(pcx, std::memory_order_relaxed); swLodPcz_.store(pcz, std::memory_order_relaxed);
  std::vector<std::pair<int, int>> todo;
  {
    for (auto it = swLodMeshed_.begin(); it != swLodMeshed_.end();) {
      const uint64_t key = it->first; const int have = it->second;
      const int cx = int(int32_t(uint32_t(key >> 32))), cz = int(int32_t(uint32_t(key)));
      if (!viewGate_.inView(cx, cz)) { it = swLodMeshed_.erase(it); continue; }  // Archie Phase 1: retired, not re-LODed
      ++it;
      const int d = std::max(std::abs(cx - pcx), std::abs(cz - pcz));
      const int want = swLodLevel(d);
      if (want == have) continue;
      if (want < have ? swLodLevel(d + 1) == have : swLodLevel(d - 1) == have) continue;  // hysteresis
      todo.emplace_back(cx, cz);
      if (todo.size() >= 24) break;
    }
  }
  if (todo.empty()) return;
  ::litholattice::cGateLock<litholattice::cBlockGate> simHold;
  static int lodSkips = 0;  // game thread only
  bool lodBusy = false;
  if (!frameHold(*sim_, simHold, lodSkips, lodBusy, /*allowBlock=*/false)) return;
  for (auto [cx, cz] : todo) {
    const ChunkColumn* c = active_->chunks.column(cx, cz);
    if (!c) { swLodMeshed_.erase(ChunkKey::make(cx, cz)); continue; }
    if (!swLodRings_.live()) swLodMeshed_.erase(ChunkKey::make(cx, cz));  // PlayB: back to full detail, no longer tracked
    submitMeshColumn(cx, cz);  // P6 locality (records the new ring)
  }
}

// ---- Thread Handshake P5: chunk handshake bookkeeping (game thread) ----
void Game::p5ResetChunkHandshake() {
  viewGate_.clear();  // Archie Phase 1: a new world / dimension starts ungated until its first pump
  viewGateBacklog_ = false;
  promoteBudget_ = {};
  spawnSpiral_ = false;
  borderRing_.clear();
  l0Hold_.clear();      // fix (b)
  meshLedger_.clear();  // P8
  meshHashLogged_ = 0;
  colMeshPending_.clear();
  stagedRenderCols_.clear();
  revealPending_ = true;  // P7: the next frame starts the wafer reveal around the spawn column
}

void Game::p5NoteMeshArrived(int cx, int cz) {
  const uint64_t key = ChunkKey::make(cx, cz);
  const auto it = colMeshPending_.find(key);
  if (it == colMeshPending_.end() || --it->second > 0) return;
  colMeshPending_.erase(it);
  // Only the game thread inserts/erases columns, so this lookup is safe while the sim runs.
  if (active_)
    if (const ChunkColumn* c = active_->chunks.column(cx, cz))
      if (c->state.advance(litholattice::ChunkStage::Mesh, litholattice::ChunkStage::StagedRender)) stagedRenderCols_.push_back(key);
}

void Game::p5AdvanceStagedRender() {
  if (stagedRenderCols_.empty() || !active_) return;
  std::erase_if(stagedRenderCols_, [&](uint64_t key) {
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    const ChunkColumn* c = active_->chunks.column(cx, cz);
    if (!c) return true;
    for (int sy = 0; sy < kSubChunks; ++sy)
      if (pendingMesh_.count(packPos(cx, sy, cz))) return false;  // still waiting for the render pop
    c->state.advance(litholattice::ChunkStage::StagedRender, litholattice::ChunkStage::Active);
    return true;
  });
}

// Thread Handshake P7: wafer reveal. Default on in every dimension (JD 2026-10-31); LL_WAFER_REVEAL=0
// disables it; LL_V_ETCH sets rings/s (inf/0 = instant).
// Backends without the reveal (GL / software) skip it entirely, sky included.
void Game::pumpWaferReveal(float dt) {
  if (revealPending_ && active_) {
    revealPending_ = false;
    reveal_.stop();
    revealSky_ = false;
    skyFade_ = 1.f;
    renderer_.setWaferReveal(litholattice::WaferRevealFrame{});
    const char* env = std::getenv("LL_WAFER_REVEAL");
    const int mode = env ? std::atoi(env) : -1;
    if (renderer_.supportsWaferReveal() && mode != 0) {  // JD 2026-10-31: default load-in in every dimension
      litholattice::cWaferRevealPacer::Config c;
      if (const char* v = std::getenv("LL_V_ETCH")) c.vEtch = std::strtof(v, nullptr);
      if (benchSet_) c.vEtch = std::numeric_limits<float>::infinity();  // --bench: v_etch infinite
      c.maxRing = std::clamp(int(cfg_.worldChunks), c.pedestalRings, litholattice::kRevealHalf);
      const float px = benchSet_ ? benchCam_.x : player_.pos.x, pz = benchSet_ ? benchCam_.z : player_.pos.z;
      reveal_.begin(int(std::floor(px / 16)), int(std::floor(pz / 16)), c);
      revealSky_ = true;
      skyFade_ = 0.f;
      revealLoggedOnline_ = false;
      logInfo("wafer reveal: origin column %d,%d, v_etch %g rings/s, %d rings", int(std::floor(px / 16)),
              int(std::floor(pz / 16)), double(c.vEtch), c.maxRing);
    }
  }
  if (!reveal_.active()) return;
  const float step = renderPaused() ? 0.f : dt;  // JD pause rule: the reveal clock is render pipeline
  reveal_.advance(step, [&](int cx, int cz) {
    if (colMeshPending_.count(ChunkKey::make(cx, cz))) return false;
    const ChunkColumn* c = active_->chunks.column(cx, cz);  // game thread: safe while the sim runs
    return c && c->state.stage() == litholattice::ChunkStage::Active;
  });
  if (reveal_.skyboxOnline()) {
    if (!revealLoggedOnline_) {
      revealLoggedOnline_ = true;
      logInfo("SKYBOX_ONLINE: wafer reveal complete at t=%.2fs", double(reveal_.now()));
    }
    skyFade_ = benchSet_ ? 1.f : std::min(1.f, skyFade_ + step);  // 1 s fade-in
    if (skyFade_ >= 1.f) { reveal_.stop(); revealSky_ = false; }
  }
  reveal_.fill(revealFrame_);
  renderer_.setWaferReveal(revealFrame_);
}

// ---- Thread Handshake P6: swarm and locality ----
void Game::meshOne(const std::shared_ptr<MeshInput>& in, const std::shared_ptr<const BlockMaterials>& mats, bool binary) {
  litholattice::workTrace("mesh", in->cx, in->sy, in->cz, in->seq);  // duplicate-work trace (LL_WORK_TRACE)
  const bool hc = litholattice::cHitchCapture::enabled();  // LL_HITCH_CAPTURE (default off)
  const uint64_t hc0 = hc ? litholattice::cHitchCapture::nowNs() : 0;
  if (in->pin) litholattice::gatherPinned(*in);
  const uint64_t hc1 = hc ? litholattice::cHitchCapture::nowNs() : 0;  // render pipeline M5: the worker builds its own 18^3 slab
  if (binary) {
    // BinaryQuad path (Worker 1's gap closed here): the per-voxel material bytes
    // ride the snapshot; extractMaterial16 de-pads them to the dense 16^3 layout
    // binaryMeshSubChunk's contract expects (MeshInput::mat is 18^3 padded).
    // The column palette rode the snapshot too, so bytes resolve to compact
    // registry IDs in the expansion below.
    std::vector<BinaryQuad> quads;
    uint8_t mat16[16 * 16 * 16];
    extractMaterial16(*in, mat16);
    binaryMeshSubChunk(*in, MeshContext{blockRegistry(), *mats, &blockAppearance()}, mat16, quads);
    // Transparent companion (fix 2026-10-11): non-opaque cubes (glass, water)
    // merge into the same quad stream; the PBR shader resolves alpha per
    // material (correct for the screen-door/refraction tiers).
    binaryMeshSubChunkTransparent(*in, MeshContext{blockRegistry(), *mats, &blockAppearance()}, mat16, quads);
    PbrChunkMesh pm;
    pm.cx = in->cx; pm.sy = in->sy; pm.cz = in->cz; pm.epoch = in->epoch; pm.seq = in->seq;
    // Coarse light levels: the flood fill is not wired yet (same gap as the
    // software path's unwired setPbrContext). sky = full like the classic path's
    // unlit brightness, so the PBR path doesn't render black; block = 0.
    expandBinaryQuadsToPbr(quads, in->matPalette, in->matPaletteSize, 0, litholattice::kMaxLight, pm.verts);
    pbrMeshOut_.push(std::move(pm));  // empty verts = remove (backend erases)
    // The binary mesher only covers opaque cubes; every other shape still needs
    // the classic mesher (drawn by the classic program) - coexistence, not cutover.
  }
  // Wafer ground pulse (JD 2026-10-11): the column's weight-blended pulse
  // params bake into the mesh at mesh time (biome weights are
  // worldgen-static, so this costs nothing per frame). Only the Wafer
  // pulses; other dimensions leave the mesh params zeroed.
  litholattice::MeshContext ctx{blockRegistry(), *mats, &blockAppearance()};
  ctx.greedy = renderer_.prefersGreedyMeshes();  // Cortana 2026-10-15 1705: greedy meshing [GAME] (software backend only)
  ctx.pulseParams = [this](int cx, int cz, float p0[4], float p1[4]) {
    if (active_ && active_->id == "wafer" && pulseCache_) pulseCache_->params(cx, cz, p0, p1);
  };
  // Wafer ground traces (JD 2026-10-11; landed 2026-10-13, P1-RENDER H1):
  // same biome weight vector as the pulse, blended by the same cache;
  // the mesher bakes the result into ChunkMesh::traceP0/traceP1.
  ctx.traceParams = [this](int cx, int cz, float t0[4], float t1[4]) {
    if (active_ && active_->id == "wafer" && pulseCache_) pulseCache_->traceParams(cx, cz, t0, t1);
  };
  // 2026-10-25 (box, triangle diet; software path only, Vulkan untouched): far sub-chunks mesh
  // from a 2x/4x downsampled snapshot (bench-stream lodInput) and drop near-detail quads.
  const bool lodLive = swLodOn_ && swLodRings_.live();  // PlayB: no LOD checks when view <= ring 0
  const int lod = lodLive ? swLodFor(in->cx, in->cz) : 0;
  const int lf = lod == 0 ? 1 : lod == 1 ? 1 : lod == 2 ? 2 : 4;
  const std::shared_ptr<MeshInput> src = lf > 1 ? farLodMeshInput(*in, lf) : in;
  ChunkMesh cm = meshSubChunk(*src, ctx);
  const uint64_t hc2 = hc ? litholattice::cHitchCapture::nowNs() : 0;
  cm.lodScale = uint8_t(lf);  // software renderer: >= 4x flat shade / thin-face cull
  if (lf > 1)
    for (PackedVertex& v : cm.verts) { v.x = uint16_t(v.x * lf); v.y = uint16_t(v.y * lf); v.z = uint16_t(v.z * lf); }
  if (lod >= 1) {
    auto& vs = cm.verts;
    size_t w = 0;
    for (size_t q = 0; q + 3 < vs.size(); q += 4)
      if (!(vs[q].mode & kShadeLodNear)) { if (w != q) for (int k = 0; k < 4; ++k) vs[w + size_t(k)] = vs[q + size_t(k)]; w += 4; }
    vs.resize(w);
  }
  // Cortana 2026-10-25 (box, upload pass; JD-approved): pack + copy into GPU memory here on the
  // worker (backends with a direct-upload arena: Vulkan ReBAR/host, or a worker-staged device-only
  // block); the main thread then only publishes the handle. Off unless LL_ASYNC_CHUNK_UPLOAD=1.
  if (asyncChunkUploadOn() && !cm.verts.empty()) {
    auto h = std::make_shared<PreparedChunkMeshHandle>();
    if (renderer_.prepareChunkMeshAsync(cm, h->mesh)) { h->owner = &renderer_; cm.prepared = std::move(h); }
  }
  if (hc) {  // gather = slab build, mesh = mesher (incl. binary path), vertex = LOD scale/cull + async pack/copy
    const uint64_t hc3 = litholattice::cHitchCapture::nowNs();
    litholattice::cHitchCapture::add(litholattice::cHitchCapture::kGather, hc1 - hc0);
    litholattice::cHitchCapture::add(litholattice::cHitchCapture::kMesh, hc2 - hc1);
    litholattice::cHitchCapture::add(litholattice::cHitchCapture::kVertex, hc3 - hc2);
    litholattice::cHitchCapture::addJob(hc3 - hc0);
  }
  meshOut_.push(std::move(cm));
}

std::shared_ptr<MeshInput> Game::snapshotSlab(int cx, int sy, int cz, bool ownFresh) {
  std::shared_ptr<MeshInput> in = meshSlab_.acquire();
  in->text.clear();
  // M5: pinned CoW columns (worker gathers). Republish this sub and its vertical neighbors, or
  // nothing when submitMeshColumn already republished the whole column.
  const uint64_t own = ownFresh ? ((uint64_t(7) << sy) >> 1) : 0;
  litholattice::pinForMeshingInto(active_->chunks, blockRegistry(), cx, sy, cz, epoch_, ++initSeq_, active_->belowWorldFill, *in,
                                  litholattice::MeshEdge::Clamp, own);  // P8 (Archie Q3): clamp-to-edge, not air
  in->batch = 0; in->batchSize = 0;  // recycled buffer: not part of a sim publish batch
  return in;
}

void Game::submitMeshColumn(int cx, int cz) {
  const ChunkColumn* c = active_->chunks.column(cx, cz);
  if (!c || c->edgeShell) return;  // Q12: a demoted column keeps its meshes
  if (!viewGate_.inView(cx, cz)) { viewGate_.defer(cx, cz); return; }  // Archie Phase 1: meshes when it enters the view
  if (swLodOn_ && swLodRings_.live()) swLodMeshed_[ChunkKey::make(cx, cz)] = swLodFor(cx, cz);  // Phase 3a: game thread records the ring
  std::vector<std::shared_ptr<MeshInput>> batch;
  batch.reserve(kSubChunks);
  const auto tPub = std::chrono::steady_clock::now();
  if (litholattice::cowColumnsEnabled()) active_->chunks.cowTable().publish(active_->chunks, blockRegistry(), cx, cz, ~uint64_t(0));  // M5
  g_meshSubmitSplit.publishMs += pumpMsSince(tPub);
  const auto tPin = std::chrono::steady_clock::now();
  for (int sy = 0; sy < kSubChunks; ++sy) {
    if (!c->subs[sy]) continue;
    if (meshBackpressured()) { submitMeshAt(cx, sy, cz); continue; }  // defers the key (t3366u)
    batch.push_back(snapshotSlab(cx, sy, cz, /*ownFresh=*/false));
  }
  g_meshSubmitSplit.pinMs += pumpMsSince(tPin);
  ++g_meshSubmitSplit.columns;
  g_meshSubmitSplit.slabs += int(batch.size());
  if (batch.empty()) return;
  ++p6ColumnJobs_;
  meshInFlight_ += int(batch.size());  // P8 quiescence
  if (meshHashOn_) meshLedger_.noteMeshedBorder(ChunkKey::make(cx, cz), litholattice::ChunkStateWord::borderGen(c->state.load()));
  submitMeshJob(pool_, [this, batch = std::move(batch), mats = resources_.materials(), binary = useBinaryMesher_] {
    for (const auto& in : batch) meshOne(in, mats, binary);
  });
}

void Game::prewarmSwarm() {
  if (swarmPrewarmed_) return;
  swarmPrewarmed_ = true;
  const auto t0 = std::chrono::steady_clock::now();
  meshSlab_.reserve(litholattice::kSlabRadius8Columns);
  const unsigned n = std::max(1u, std::thread::hardware_concurrency());
  for (unsigned i = 0; i < n; ++i)
    pool_.submitBackground([] {
      volatile uint8_t stack[128 * 1024];  // first-touch the worker stack
      for (size_t k = 0; k < sizeof stack; k += 4096) stack[k] = uint8_t(k);
      std::vector<uint8_t> warm(256 * 1024, 1);  // and its allocator arena
      (void)warm;
    });
  if (streamer_ && active_) {
    const int scx = int(std::floor(player_.pos.x / 16)), scz = int(std::floor(player_.pos.z / 16));
    streamer_->updateAround(scx, scz, 8);
  }
  const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  logInfo("swarm: title pre-warm - slab %zu snapshot buffers (radius-8 tier), %u worker wakes, radius-8 ring requested (%.2f ms)",
          meshSlab_.idle(), n, ms);
}

// Fix (b): a promoted column's first L0 mesh (immediately, or when its neighbour hold releases).
// Any neighbour BorderGen bumps taken while held are folded into this one mesh.
void Game::startFirstMesh(int cx, int cz) {
  const ChunkColumn* c = active_->chunks.column(cx, cz);
  if (!c) return;
  (void)c->state.takeDirty(litholattice::kChunkDirtyNeighbor);
  for (int sy = 0; sy < kSubChunks; ++sy)
    if (c->subs[sy]) c->state.bumpMeshGen();
  submitMeshColumn(cx, cz);  // P6 locality: one job per column
}

void Game::submitMeshAt(int cx, int sy, int cz) {
  if (const ChunkColumn* dc = active_->chunks.column(cx, cz); dc && dc->edgeShell) return;  // Q12
  if (!viewGate_.inView(cx, cz)) { viewGate_.defer(cx, cz); return; }  // Archie Phase 1
  if (swLodOn_ && swLodRings_.live()) swLodMeshed_[ChunkKey::make(cx, cz)] = swLodFor(cx, cz);  // Phase 3a
  if (meshBackpressured()) {  // t3366u: paused + Render Ingress at cap -> defer the key, never drop
    const int64_t k = packPos(cx, sy, cz);
    if (deferredMeshKeys_.insert(k).second) { deferredMeshOrder_.push_back(k); ++meshDeferredTotal_; }
    return;
  }
  auto in = snapshotSlab(cx, sy, cz);
  ++meshInFlight_;  // P8 quiescence
  submitMeshJob(pool_, [this, in, mats = resources_.materials(), binary = useBinaryMesher_] { meshOne(in, mats, binary); });
}

bool Game::streamSettled() const { return streamUnsettledReason() == nullptr; }

const char* Game::streamUnsettledReason() const {
  // Only the columns inside the stream footprint count: frontier columns beyond it (camera load
  // requests, eviction hysteresis) keep trickling through the L0 hold and the mesh queue forever.
  if (!streamer_ || !active_) return nullptr;
  if (!streamCentreSet_) return "footprint";  // no stream pump yet
  const int pcx = streamCx_, pcz = streamCz_;  // the last pump's centre (the bench camera moves the player mid-frame)
  const int r = effectiveStreamRadius(cfg_);
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      const int dist = cfg_.diamondStreamArea ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz));
      if (dist > r) continue;
      if (!active_->chunks.column(pcx + dx, pcz + dz)) return "footprint";
      const uint64_t key = ChunkKey::make(pcx + dx, pcz + dz);
      if (l0Hold_.isHeld(key)) return "l0-hold";
      if (colMeshPending_.count(key)) return "first-wave-pending";
    }
  return nullptr;
}

size_t Game::streamFootprintColumns() const {
  const int r = effectiveStreamRadius(cfg_);
  size_t n = 0;
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx)
      if ((cfg_.diamondStreamArea ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz))) <= r) ++n;
  return n;
}

bool Game::sampleStreamFill(cStreamFillTracker& t, std::chrono::steady_clock::time_point now) const {
  if (!streamer_ || !active_ || !streamCentreSet_) return false;
  const int pcx = streamCx_, pcz = streamCz_, r = effectiveStreamRadius(cfg_);
  bool all = true;
  for (int dz = -r; dz <= r; ++dz)
    for (int dx = -r; dx <= r; ++dx) {
      const int dist = cfg_.diamondStreamArea ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz));
      if (dist > r) continue;
      const uint64_t key = ChunkKey::make(pcx + dx, pcz + dz);
      t.noteWanted(key, now);
      const ChunkColumn* c = active_->chunks.column(pcx + dx, pcz + dz);
      const bool ready = c && c->state.stage() >= litholattice::ChunkStage::StagedRender && !l0Hold_.isHeld(key) && !colMeshPending_.count(key);
      if (ready) t.noteReady(key, now);
      else all = false;
    }
  return all;
}

void Game::pumpStreaming(float frameDtMs) {
  // P5 deferred free queue: evicted columns die here, two pumps after their
  // eviction and OUTSIDE the sim hold, a few per pump (never a free burst).
  ++streamPumps_;
  constexpr size_t kDeferredFreesPerPump = 4;
  const auto tPump = std::chrono::steady_clock::now();
  g_meshSubmitSplit = {};
  double msPre = 0, msStream = 0, msHold = 0, msStaged = 0, msPromote = 0, msScan = 0, msFirst = 0, msBorder = 0, msDefer = 0, msEvict = 0;
  int nScan = 0, nFirst = 0, nBorder = 0, nFromDisk = 0;
  auto logPumpSplit = [&](const char* why) {
    const double total = pumpMsSince(tPump);
    if (total < 20.0) return;
    logInfo("pumpStream split (%.1f ms, %s): pre=%.1f stream=%.1f hold=%.1f staged=%.1f promote=%.1f scan=%.1f (n=%d disk=%d) firstMesh=%.1f (n=%d) border=%.1f (n=%d) defer=%.1f evict=%.1f | cow publish=%.1f pin=%.1f meshCols=%d slabs=%d",
            total, why, msPre, msStream, msHold, msStaged, msPromote, msScan, nScan, nFromDisk, msFirst, nFirst, msBorder, nBorder, msDefer, msEvict,
            g_meshSubmitSplit.publishMs, g_meshSubmitSplit.pinMs, g_meshSubmitSplit.columns, g_meshSubmitSplit.slabs);
  };
  deferredFree_.collect(streamPumps_, kDeferredFreesPerPump);
  if (!streamer_ || !sim_ || !active_) return;
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  streamCx_ = pcx; streamCz_ = pcz; streamCentreSet_ = true;  // bench settle gate reads the pump's centre
  litholattice::setFoliageFocus(pcx, pcz);  // 2026-10-18 (Crew Canopy, F-B): sparse-leaf distance origin
  // Archie PlayI crash fix (PlayH AV 0xc0000005 in cSignalSim::edgeSplit): StagedSim promote
  // MUST run under the between-ticks hold. scanColumn mutates wires_/comps_/edgeCols_ that the
  // sim thread reads in edgeSplit/tick â€” unsynchronized promote raced the sim and faulted at
  // unordered_set::contains (WER read AV @ 0x10). Peek here; promote body is under hold below.
  const int stagedPromoteR = effectiveStreamRadius(cfg_);
  bool needStagedPromote = false;
  for (const auto& [key, col] : active_->chunks.columns()) {
    (void)key;
    if (!col || col->state.stage() != litholattice::ChunkStage::StagedSim) continue;
    if (std::max(std::abs(col->cx - pcx), std::abs(col->cz - pcz)) <= stagedPromoteR) {
      needStagedPromote = true;
      break;
    }
  }
  {  // 2026-10-25 stutter pass: above 90% mesh-pool fill, stop admitting far columns (shrink the
     // admission radius a step per 250 ms, floor 8) instead of evicting and re-streaming; grow back
     // below 85%. Resident columns are not touched; eviction keeps its own radius + hysteresis.
    static Clock::time_point lastAdmit = Clock::now();
    const int full = effectiveStreamRadius(cfg_);
    const float fill = renderer_.meshPoolFill();
    int ar = std::min(streamer_->admitRadius(), full);
    if (fill < 0.f) ar = full;
    else if (msBetween(lastAdmit, Clock::now()) >= 250.0) {
      lastAdmit = Clock::now();
      const int was = ar;
      if (fill > 0.90f) ar = std::max(std::min(8, full), ar - 1);
      else if (fill < 0.85f) ar = std::min(full, ar + 1);
      if (ar != was && (ar == full || fill > 0.90f) && (ar % 4 == 0 || ar == full))
        logInfo("stream: mesh pool %.1f%% full; admission radius %d (stream radius %d)", 100.0 * double(fill), ar, full);
    }
    streamer_->setAdmitRadius(ar);
  }
  {  // Frame-aware streaming budget (2026-10-28 â€” Archie: optimization
     // design; Elfie: co-implementation; JD: direction + debugging):
     // pace NEW column requests by frame health so the streaming fill
     // stops fighting the frame it is filling for. EMA of the frame
     // time; Archie's grant table: starving (>40 ms) 1, cruising
     // (30..40) 2, settling (25..30) 4, headroom (<25) 6 per frame.
    static float emaMs = 16.7f;
    emaMs += (frameDtMs - emaMs) * 0.1f;
    const int grant = emaMs > 40.0f ? 1 : emaMs >= 30.0f ? 2 : emaMs >= 25.0f ? 4 : 6;
    static int lastGrant = -1;
    if (grant != lastGrant) {
      lastGrant = grant;
      logInfo("stream: frame-aware budget: frame ema %.1f ms -> %d new columns/frame", double(emaMs), grant);
    }
    streamer_->setRequestBudget(grant);
  }
  {  // S4 emergency floor (2026-10-28, Archie ruling): a sprint-flying
     // player at a starving grant (1/frame) can outrun the world edge.
     // When any footprint column within 2 chunks of the player (same
     // metric as the stream footprint) is not resident, floor the DATA /
     // COLLISION generation admissions at 4/frame; render-mesh
     // compilation stays on its existing throttles (bounded insertion +
     // mesh submission are untouched by the floor â€” see chunk_streamer.h).
    bool edgeNear = false;
    for (int dz = -2; dz <= 2 && !edgeNear; ++dz)
      for (int dx = -2; dx <= 2; ++dx) {
        const int dist = cfg_.diamondStreamArea ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz));
        if (dist > 2) continue;
        if (!active_->chunks.column(pcx + dx, pcz + dz)) { edgeNear = true; break; }
      }
    static bool floorOn = false;  // game thread only
    if (edgeNear != floorOn) {
      floorOn = edgeNear;
      logInfo("stream: emergency floor %s (unloaded edge within 2 chunks of the player): data/collision admissions floored at 4/frame",
              edgeNear ? "ON" : "off");
    }
    streamer_->setRequestBudgetFloor(edgeNear ? 4 : -1);
  }
  { const auto tStm = std::chrono::steady_clock::now();
    streamer_->update(pcx, pcz);
    msStream += pumpMsSince(tStm);
  }
  // CAM-2 (P2c): non-player cameras (third/follow/free view camera, MCP
  // pilot cameras) request columns around their own centres within their
  // chunk-radius budget â€” AFTER the player's update, so they only spend
  // in-flight capacity the player did not use (player priority, same cap).
  for (const auto& req : camera_.chunkLoadRequests()) streamer_->updateAround(req.cx, req.cz, req.radiusChunks);
  if (spawnSpiral_) {  // P5: the spawn spiral ends the first time the stream footprint is fully resident
    const int r = effectiveStreamRadius(cfg_);
    bool full = true;
    for (int dz = -r; dz <= r && full; ++dz)
      for (int dx = -r; dx <= r; ++dx) {
        const int dist = cfg_.diamondStreamArea ? std::abs(dx) + std::abs(dz) : std::max(std::abs(dx), std::abs(dz));
        if (dist > r) continue;
        if (!active_->chunks.column(pcx + dx, pcz + dz)) { full = false; break; }
      }
    if (full) {
      spawnSpiral_ = false;
      logInfo("stream: spawn spiral complete (%llu promoted, %llu by ring-center bypass); bypass off",
              (unsigned long long)p5Promoted_, (unsigned long long)p5Bypassed_);
    }
  }
  const bool deferredMeshReady = !deferredMeshOrder_.empty() && !meshBackpressured();
  if (!streamer_->hasReady() && borderRing_.empty() && !deferredMeshReady && l0Hold_.empty() && !needStagedPromote) {
    const auto tEv = std::chrono::steady_clock::now();
    evictFarColumns();
    msEvict += pumpMsSince(tEv);
    msPre = pumpMsSince(tPump) - msEvict;
    logPumpSplit("early-evict");
    return;
  }
  msPre = pumpMsSince(tPump);
  // P0a carry (P2b-2): the ready queue is drained only under the
  // between-ticks hold â€” never under a still-running worker, and (build
  // 80) never under a stopped sim either: a refused hold defers the whole
  // batch with the columns still queued, exactly the outcome the old
  // poisoned-stop abort produced, minus the stop()/start() the frame
  // thread used to pay for it.
  // 2026-10-15: hold the sim between ticks (blockMutex_) instead of a full
  // stop()/start() (thread join + handshake worker join/respawn per pump).
  // Build 80: the stop()/start() fallback is REMOVED from this path (see
  // the header comment above); refusal now defers.
  ::litholattice::cGateLock<litholattice::cBlockGate> simHold;
  static int insertSkips = 0;  // game thread only
  bool insertBusy = false;
  if (!frameHold(*sim_, simHold, insertSkips, insertBusy, /*allowBlock=*/false)) {
    msHold = pumpMsSince(tPump) - msPre;
    if (insertBusy) { logPumpSplit("hold-busy"); return; }  // sim mid-tick: try next frame, batch stays queued
    static int deferLog = 0;  // game thread only; rate-limited report
    if ((deferLog++ % 256) == 0)
      logWarn("stream: insertion deferred (sim hold refused: %s); batch stays queued",
              sim_->handshakeFrozen() ? "timing handshake frozen" : "handshake workers outstanding");
    logPumpSplit("hold-refused");
    return;
  }
  msHold = pumpMsSince(tPump) - msPre;
  if (needStagedPromote) {
    // PlayH crash fix: scanColumn + stage flip only while holding blockMutex_ (same as insert path).
    auto& signal = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
    int promoted = 0;
    constexpr int kPromoteBudget = 4;
    const auto tSt = std::chrono::steady_clock::now();
    for (const auto& [key, col] : active_->chunks.columns()) {
      if (promoted >= kPromoteBudget) break;
      if (!col || col->state.stage() != litholattice::ChunkStage::StagedSim) continue;
      if (std::max(std::abs(col->cx - pcx), std::abs(col->cz - pcz)) > stagedPromoteR) continue;
      (void)key;
      const auto tSc = std::chrono::steady_clock::now();
      signal.scanColumn(active_->chunks, col->cx, col->cz);
      msScan += pumpMsSince(tSc); ++nScan;
      col->state.setStage(litholattice::ChunkStage::Active);
      const auto tM = std::chrono::steady_clock::now();
      submitMeshColumn(col->cx, col->cz);
      msFirst += pumpMsSince(tM); ++nFirst;
      ++promoted;
    }
    msStaged += pumpMsSince(tSt);
  }
  // B-M2 (P3-2): bounded insertion batch. Up to kMaxInFlight (32) columns
  // could finish between pumps; inserting all of them in this one
  // sim-stopped window (insert + scanColumn + up to 48 mesh snapshots
  // each) made the stop window â€” and the frame hitch â€” proportional to
  // the burst. Insert at most kStreamInsertBudget per pump instead; the
  // rest stay staged in the streamer and land over the next pumps, each
  // fully generated/loaded before handoff (snapshot-then-insert, as
  // before). 4/pump drains a full 32-column burst in 8 frames.
  //
  // Thread Handshake P5 (Archie ruling, R7): the bound is now the sim pop
  // budget - 4 promotions per sim tick stepped, spent here at the commit
  // barrier (this hold is taken between ticks, after the tick committed),
  // banked to 16. Ring-center columns bypass it during the initial spawn
  // spiral only. Counted in sim ticks, never wall clock.
  const int grant = promoteBudget_.open(sim_->stats().worldTick);  // world ticks: ticksStepped() only counts paused single-steps
  size_t bypassed = 0;
  const auto tProm = std::chrono::steady_clock::now();
  std::vector<StreamedColumn> fresh = streamer_->takeReadyPromote(size_t(grant), pcx, pcz, spawnSpiral_, &bypassed);
  msPromote += pumpMsSince(tProm);
  promoteBudget_.spend(int(fresh.size() - bypassed));
  p5Bypassed_ += bypassed;
  cSignalSim& signal = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
  size_t added = 0;
  for (auto& sc : fresh) {
    std::unique_ptr<ChunkColumn> col = std::move(sc.column);
    if (!col) continue;
    if (const ChunkColumn* rc = active_->chunks.column(col->cx, col->cz)) {
      if (!rc->edgeShell) continue;  // resident: keep it
      // Q12 promote: replace the demoted shell in place. Its meshes are NOT retired; the fresh
      // first wave swaps over them (old mesh stays visible until then).
      farPromoteAsked_.erase(ChunkKey::make(col->cx, col->cz));
      ++farPromoted_;
    }
    const int cx = col->cx, cz = col->cz;
    const bool fromDisk = sc.fromDisk;
    col->state.setSimTicket(uint16_t(sim_->ticksStepped()));  // P5: the commit barrier it was promoted at
    active_->chunks.insert(std::move(col));
    ++p5Promoted_;
    // P5 lazy border ring: the resident neighbors' edges changed. Bump their
    // BorderGen (raises NEIGHBOR_DIRTY) and queue them; they re-mesh lazily
    // below, a few per pump, under this same hold.
    // P8 (Archie Q4): all 8 Moore neighbors (4 edge + 4 corner); corner apron cells
    // were clamped to edge while this column was missing, so they re-mesh too.
    for (const auto& d : litholattice::kMoore8)
      if (const ChunkColumn* n = active_->chunks.column(cx + d[0], cz + d[1])) {
        n->state.bumpBorderGen();
        borderRing_.note(ChunkKey::make(cx + d[0], cz + d[1]));
      }
    // A2/B-M1: a re-streamed column may carry meshSeq tombstones from its
    // eviction (retireColumnMeshes). Clear them so the fresh snapshots
    // submitted below are not mistaken for stale meshes and dropped.
    for (int sy = 0; sy < kSubChunks; ++sy) {
      meshSeq_.erase(packPos(cx, sy, cz));
      pbrMeshSeq_.erase(packPos(cx, sy, cz));
    }
    ++added;
    if (fromDisk) {
      // SW-H1: a disk-loaded column was never seen by the sim's one scan() at
      // dimension load â€” index its cells and import its SGNL payload now,
      // while the sim is stopped, or its circuits come back dead and their
      // component memory is lost (and a later save would overwrite the
      // orphaned SGNL state with the sim's empty export). Fresh-generated
      // columns need no scan: worldgen places only SimRole::None blocks
      // (verified against the procgen/game worldgen paths), so there is
      // nothing to index and no SGNL payload exists for them.
      ++nFromDisk;
      const auto tSc = std::chrono::steady_clock::now();
      signal.scanColumn(active_->chunks, cx, cz);
      msScan += pumpMsSince(tSc); ++nScan;
      if (!sc.signal.empty() && !signal.importState(sc.signal))
        logError("save: streamed column %d,%d signal state unreadable (components restart idle)", cx, cz);
    }
    const ChunkColumn* c = active_->chunks.column(cx, cz);
    c->state.setStage(litholattice::ChunkStage::Mesh);  // P5: STAGED_SIM -> MESH
    int submitted = 0;
    for (int sy = 0; sy < kSubChunks; ++sy)
      if (c->subs[sy]) {
        ++totalSubs_;
        ++submitted;
      }
    if (submitted) {
      colMeshPending_[ChunkKey::make(cx, cz)] = submitted;
      // Fix (b): the first L0 mesh waits for the 8 Moore neighbours (or the hold's expiry).
      if (l0Hold_.enabled() && !cL0NeighbourHold::neighboursPresent(cChunkMapPresence(active_->chunks), cx, cz))
        l0Hold_.hold(ChunkKey::make(cx, cz), sim_->stats().worldTick);  // world ticks: ticksStepped() only counts paused single-steps
      else {
        const auto tM = std::chrono::steady_clock::now();
        startFirstMesh(cx, cz);
        msFirst += pumpMsSince(tM); ++nFirst;
      }
    } else {
      c->state.setStage(litholattice::ChunkStage::Active);  // all air: nothing to mesh
    }
  }
  if (!l0Hold_.empty()) {  // fix (b): release held columns whose neighbours arrived / hold expired
    std::vector<uint64_t> ready;
    l0Hold_.setMaxTicks(cL0NeighbourHold::maxTicksFor(sim_->stats().targetTps));  // 2 s of ticks at the live rate
    l0Hold_.collectReady(cChunkMapPresence(active_->chunks), sim_->stats().worldTick, ready);  // (under the between-ticks hold)
    const auto tL0 = std::chrono::steady_clock::now();
    for (uint64_t key : ready) { startFirstMesh(int(int32_t(key >> 32)), int(int32_t(key & 0xffffffffu))); ++nFirst; }
    msFirst += pumpMsSince(tL0);
  }
  {  // P5 lazy border ring: re-mesh at most kBorderColsPerPump neighbor columns whose
     // BorderGen moved (coalesced: a column queued by several new neighbors re-meshes once).
    constexpr size_t kBorderColsPerPump = 2;
    // P6 slack harvesting: spare frame time (vs a 60 Hz budget) buys extra border
    // columns, priced by the measured cost of a column re-mesh submit.
    const int extra = borderRing_.empty() ? 0 : slack_.grant(1000.0 / 60.0, double(frameDtMs));
    p6SlackSlices_ += uint64_t(extra);
    std::vector<uint64_t> border;
    borderRing_.popUpTo(kBorderColsPerPump + size_t(extra), border);
    const auto tB = std::chrono::steady_clock::now();
    for (uint64_t key : border) {
      const int bx = int(int32_t(key >> 32)), bz = int(int32_t(key & 0xffffffffu));
      const ChunkColumn* c = active_->chunks.column(bx, bz);
      if (!c || l0Hold_.isHeld(key)) continue;  // fix (b): a held column meshes once, at release
      if (!c->state.takeDirty(litholattice::kChunkDirtyNeighbor)) continue;
      const auto s0 = std::chrono::steady_clock::now();
      for (int sy = 0; sy < kSubChunks; ++sy)
        if (c->subs[sy]) c->state.bumpMeshGen();
      submitMeshColumn(bx, bz);
      slack_.noteSlice(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - s0).count());
      ++p5BorderRemeshed_;
      ++nBorder;
    }
    msBorder += pumpMsSince(tB);
  }
  {  // t3366u backpressure release: resubmit deferred sub-chunks (fresh snapshots), bounded per pump
    constexpr int kDeferredMeshPerPump = 64;
    const auto tD = std::chrono::steady_clock::now();
    for (int n = 0; n < kDeferredMeshPerPump && !deferredMeshOrder_.empty() && !meshBackpressured(); ++n) {
      const int64_t k = deferredMeshOrder_.front();
      deferredMeshOrder_.pop_front();
      deferredMeshKeys_.erase(k);
      int x, y, z;
      unpackPos(k, x, y, z);
      if (const ChunkColumn* c = active_->chunks.column(x, z); c && c->subs[y])
        submitMeshAt(x, y, z);
    }
    msDefer += pumpMsSince(tD);
  }
  if (fresh.empty()) {
    logPumpSplit("no-insert");
    return;  // hold released by the lock's destructor
  }
  chunkColumns_ = active_->chunks.columns().size();
  if (simHold.owns_lock()) simHold.unlock();  // release before evictFarColumns (it takes its own hold)
  g_colsAdded += added;
  if (added) logInfo("stream: +%zu columns around chunk (%d, %d); %zu loaded", added, pcx, pcz, chunkColumns_);
  const auto tEv = std::chrono::steady_clock::now();
  evictFarColumns();
  msEvict += pumpMsSince(tEv);
  logPumpSplit("insert");
}

// Column eviction policy (A2 / B-M1). JD-DECISION parameters (brief C6:
// the p3-2 design proposes, JD locks): the hysteresis band beyond the
// stream radius and the per-pump eviction budget (mirroring the B-M2
// insertion budget of 4). Named constants, not locks.
static constexpr int kEvictHysteresisChunks = 2;
static constexpr size_t kEvictBudgetPerPump = 4;

void Game::retireColumnMeshes(int cx, int cz, bool force) {
  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana).
  // Design B Stage 2: on the ring path the tombstones below are staged in
  // the upload ring like every other upload â€” this function then touches
  // no renderer state and takes no guard, so an eviction pump never waits
  // on the in-flight render. Other paths keep the guard exactly as before.
  std::unique_ptr<litholattice::threeway::RendererGuard> rendererGuard;
  if (!renderer_.meshUploadsQueued()) rendererGuard = std::make_unique<litholattice::threeway::RendererGuard>();
  for (int sy = 0; sy < kSubChunks; ++sy) {
    const int64_t key = packPos(cx, sy, cz);
    const bool hadClassic = force || meshSeq_.count(key) != 0 || pendingMesh_.count(key) != 0;  // force: the view gate retires sim-thread meshes the seq maps never saw
    const bool hadPbr = force || pbrMeshSeq_.count(key) != 0 || pbrPendingMesh_.count(key) != 0;
    pendingMesh_.erase(key);
    pbrPendingMesh_.erase(key);
    std::erase(pendingOrder_, key);
    std::erase(pbrPendingOrder_, key);
    if (!hadClassic && !hadPbr) continue;
    // Tombstone the upload sequence: a mesh job already in flight for
    // this sub-chunk must not resurrect it after the retire uploads
    // below. The tombstone is cleared if the column re-streams (see the
    // insertion loop in pumpStreaming).
    meshSeq_[key] = UINT64_MAX;
    pbrMeshSeq_[key] = UINT64_MAX;
    meshLedger_.erase(key);  // P8 MeshHash: retired with its column
    if (hadClassic) {
      ChunkMesh empty;
      empty.cx = cx; empty.sy = sy; empty.cz = cz; empty.epoch = epoch_;
      mcpSenses_.noteMesh(empty);      // vision cache erases on empty verts
      // Design B Stage 2: empty verts = remove (renderer contract); on the
      // ring path the tombstone is staged (must-push) or spilled â€” never dropped.
      if (!renderer_.enqueueChunkMesh(std::move(empty))) spillRingUpload(0, std::move(empty), {});
    }
    if (hadPbr) {
      litholattice::PbrChunkMesh empty;
      empty.cx = cx; empty.sy = sy; empty.cz = cz; empty.epoch = epoch_;
      // Stage 2: same staging/spill discipline as the classic tombstone.
      if (!renderer_.enqueuePbrChunkMesh(std::move(empty))) spillRingUpload(1, {}, std::move(empty));
    }
  }
}

void Game::evictFarColumns() {
  if (!streamer_ || !sim_ || !active_) return;
  demoteFarColumns();  // Q12 (LL_FAR_DEMOTE, default off)
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  std::vector<uint64_t> residentKeys;
  residentKeys.reserve(active_->chunks.columns().size());
  for (const auto& [k, col] : active_->chunks.columns()) residentKeys.push_back(k);
  std::vector<uint64_t> candidates = selectEvictionCandidates(residentKeys, pcx, pcz, effectiveStreamRadius(cfg_) /* fix3: sim area always inside stream */,
                                                              kEvictHysteresisChunks, SIZE_MAX, cfg_.diamondStreamArea);
  // 2026-10-25 stutter pass (hysteresis): a column a camera still streams (CAM-2 chunkLoadRequests)
  // is kept while within that camera's radius + the same hysteresis band, so unload distance stays
  // beyond load distance for every stream centre, not only the player's (no evict/re-stream loop).
  {
    const auto reqs = camera_.chunkLoadRequests();
    if (!reqs.empty())
      std::erase_if(candidates, [&](uint64_t key) {
        const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
        for (const auto& rq : reqs) {
          const int dx = std::abs(cx - rq.cx), dz = std::abs(cz - rq.cz);
          const int d = cfg_.diamondStreamArea ? dx + dz : std::max(dx, dz);
          if (d <= std::clamp(rq.radiusChunks, 0, effectiveStreamRadius(cfg_)) + kEvictHysteresisChunks) return true;
        }
        return false;
      });
  }
  if (candidates.size() > kEvictBudgetPerPump) candidates.resize(kEvictBudgetPerPump);
  if (candidates.empty()) return;
  // Same hold discipline as insertion (P0a + build 80): nothing is
  // evicted under a running sim, and a refused hold defers the whole
  // batch â€” columns stay resident and are reconsidered on a later pump.
  ::litholattice::cGateLock<litholattice::cBlockGate> simHold;  // 2026-10-15: see the insertion pump
  static int evictSkips = 0;  // game thread only
  bool evictBusy = false;
  if (!frameHold(*sim_, simHold, evictSkips, evictBusy, /*allowBlock=*/false)) {
    if (evictBusy) return;  // sim mid-tick: try next frame, columns stay resident
    static int evictDeferLog = 0;  // game thread only; rate-limited report
    if ((evictDeferLog++ % 256) == 0)
      logWarn("stream: eviction deferred (sim hold refused: %s); columns stay resident",
              sim_->handshakeFrozen() ? "timing handshake frozen" : "handshake workers outstanding");
    return;
  }
  cSignalSim& signal = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
  size_t evicted = 0;
  for (uint64_t key : candidates) {
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    ChunkColumn* col = active_->chunks.column(cx, cz);
    if (!col) continue;
    // WORLD PERMANENCE: a column is droppable without a save ONLY when
    // it is clean (no ChunkMap mutation since generation/load) AND the
    // sim indexes nothing in it. Anything else saves first; when the
    // save cannot complete (no save dir, refused/failed write), the
    // column stays resident â€” never evict on uncertain state.
    const bool needsSave = col->dirty || signal.hasIndexedCells(cx, cz);
    if (needsSave) {
      if (!save_) {
        // Cortana 2026-10-15 1455 (GAME): warn once per column. It used to
        // repeat every eviction pass (~1,700-1,950 lines in a 20 s
        // framebuffer bench). Game thread only.
        static std::unordered_set<uint64_t> warnedNoSave;
        if (warnedNoSave.insert(key).second)
          logWarn("stream: column %d,%d is dirty/signal-bearing but no save directory is configured; NOT evicted (logged once per column)", cx, cz);
        continue;
      }
      if (saver_.busy()) continue;  // 2026-10-31: an async save is committing (one committer); evict next pass
      // saveColumns exports the column's SGNL state from the live sim
      // inside the call â€” strictly BEFORE the unscan below.
      advanceWorldClock();  // Cortana 2026-10-16 review fix: level.json carries the current clock
      save_->setWorldClock(worldClock_.tick, worldClock_.time);
      const size_t saved = save_->saveColumns(active_->id, {col}, signal, /*wait=*/true);
      if (saved != 1 || !save_->lastSaveOk()) {
        logError("stream: column %d,%d NOT evicted: save-on-evict failed (%s)", cx, cz, save_->lastSaveError().c_str());
        continue;
      }
    }
    signal.unscanColumn(cx, cz);       // drop sim indexes (ghost-component guard)
    retireColumnMeshes(cx, cz);        // meshes die with their column
    viewGate_.forget(cx, cz);          // Archie Phase 1
    swLodMeshed_.erase(key);           // Phase 3a
    // P5 deferred free: unlink now, free the voxel data two pumps later outside the hold.
    deferredFree_.retire(active_->chunks.extract(cx, cz), streamPumps_ + 2);
    borderRing_.forget(key);
    l0Hold_.forget(key);            // fix (b)
    meshLedger_.forgetColumn(key);  // P8
    colMeshPending_.erase(key);
    std::erase(stagedRenderCols_, key);
    streamer_->forgetColumn(cx, cz);   // re-streamable later
    farPromoteAsked_.erase(key);       // Q12
    ++evicted;
  }
  chunkColumns_ = active_->chunks.columns().size();
  if (simHold.owns_lock()) simHold.unlock();
  g_colsEvicted += evicted;
  if (evicted) logInfo("stream: evicted %zu far columns around chunk (%d, %d); %zu loaded", evicted, pcx, pcz, chunkColumns_);
}

// Q12 (Gate 1, 2026-10-31): far-column demote. Clean, signal-free, fully meshed columns beyond
// R = max(LL_FAR_DEMOTE_RADIUS (9), tickRadius + 3) free their sub-chunks and keep a 1-cell edge
// shell (neighbour mesh aprons stay exact); their meshes stay up. At d <= R - 2 the column is
// re-requested (disk if saved, else worldgen) and the fresh one replaces the shell in place.
// Known limit (JD 12:14): light stops at the ring. LL_FAR_LOD=1 also builds the Voxy-#1 LOD.
void Game::demoteFarColumns() {
  static const bool on = [] { const char* e = std::getenv("LL_FAR_DEMOTE"); return !(e && e[0] == '0'); }();  // default on (JD 2026-10-31); =0 kill switch
  if (!on || !streamer_ || !sim_ || !active_ || !litholattice::cowColumnsEnabled()) return;
  static const bool withLod = [] { const char* e = std::getenv("LL_FAR_LOD"); return !(e && e[0] == '0'); }();  // default on (JD 2026-10-31); =0 kill switch
  static const int envR = [] { const char* e = std::getenv("LL_FAR_DEMOTE_RADIUS"); const int v = e ? std::atoi(e) : 9; return v > 0 ? v : 9; }();
  const int R = std::max(envR, cfg_.tickRadius + 3);
  static bool told = false;  // game thread only
  if (!told) {
    told = true;
    logInfo("far-demote: on (radius %d, lod %s); known limit: light stops at the 1-cell edge shell of demoted columns", R, withLod ? "on" : "off");
  }
  constexpr size_t kDemoteBudgetPerPump = 8;
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  auto dist = [&](int cx, int cz) {
    const int dx = std::abs(cx - pcx), dz = std::abs(cz - pcz);
    return cfg_.diamondStreamArea ? dx + dz : std::max(dx, dz);
  };
  auto eligible = [&](uint64_t key, const ChunkColumn& c) {
    if (c.edgeShell || c.dirty || !c.cellText.empty()) return false;
    const uint64_t w = c.state.load();
    if (litholattice::ChunkStateWord::stageOf(w) != litholattice::ChunkStage::Active || litholattice::ChunkStateWord::dirtyBits(w)) return false;
    if (colMeshPending_.count(key) || l0Hold_.isHeld(key) || std::find(stagedRenderCols_.begin(), stagedRenderCols_.end(), key) != stagedRenderCols_.end())
      return false;
    for (const auto& d : litholattice::kMoore8)
      if (!active_->chunks.column(c.cx + d[0], c.cz + d[1])) return false;
    return true;
  };
  std::vector<uint64_t> picks;
  for (const auto& [k, cp] : active_->chunks.columns()) {
    const ChunkColumn& c = *cp;
    const int d = dist(c.cx, c.cz);
    if (c.edgeShell) {
      if (d <= R - 2 && farPromoteAsked_.insert(k).second) streamer_->forgetColumn(c.cx, c.cz);  // promote
      continue;
    }
    if (d > R && picks.size() < kDemoteBudgetPerPump && eligible(k, c)) picks.push_back(k);
  }
  if (picks.empty()) return;
  ::litholattice::cGateLock<litholattice::cBlockGate> simHold;  // same between-ticks hold as eviction
  static int demoteSkips = 0;
  bool busy = false;
  if (!frameHold(*sim_, simHold, demoteSkips, busy, /*allowBlock=*/false)) return;  // retry next pump
  cSignalSim& signal = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
  size_t n = 0;
  for (uint64_t key : picks) {
    const int cx = int(int32_t(key >> 32)), cz = int(int32_t(key & 0xffffffffu));
    const ChunkColumn* c = active_->chunks.column(cx, cz);
    if (!c || !eligible(key, *c) || signal.hasIndexedCells(cx, cz)) continue;
    std::unique_ptr<ChunkColumn> body = active_->chunks.demote(cx, cz, withLod);
    if (!body) continue;
    deferredFree_.retire(std::move(body), streamPumps_ + 2);  // P5: freed outside the hold
    borderRing_.forget(key);
    ++n;
  }
  if (simHold.owns_lock()) simHold.unlock();
  farDemoted_ += n;
  static uint64_t lastLog = 0;
  if (n && farDemoted_ - lastLog >= 64) {
    lastLog = farDemoted_;
    size_t resident = 0, bytes = 0;
    for (const auto& [k, cp] : active_->chunks.columns())
      if (cp->edgeShell) { ++resident; bytes += cp->edgeShell->bytes(); }
    logInfo("far-demote: %llu demoted, %llu promoted, %zu resident shells (%.2f MB), %llu refused writes",
            (unsigned long long)farDemoted_, (unsigned long long)farPromoted_, resident, bytes / 1048576.0,
            (unsigned long long)litholattice::cColumnEdgeShell::refusedWrites());
  }
}

int Game::surfaceYAt(int x, int z) {
  if (!sim_ || !active_) return -1;
  int y = -1;
  sim_->runBetweenTicks([&] { y = active_->chunks.surfaceY(x, z); });
  return y;
}

// cCamera spring-arm collision (camera_controller.h's ICameraWorld hook).
// Runs at most once per frame, only while third person is live (the engine
// casts only for the orbit boom; the controller disengages otherwise).
// Cortana 2026-10-15 1900: world + signal reads for the lithography scan. Call only on the sim
// thread between ticks (inside cSimThread::runBetweenTicks): the sim owns the map while ticking.
LithoReader Game::lithoReader() {
  ChunkMap& ch = active_->chunks;
  const cSignalSim& sig = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
  return LithoReader{[&ch](int x, int y, int z) { return ch.get(x, y, z); },
                     [&ch](int x, int y, int z) { return ch.getState(x, y, z); },
                     [&ch, &sig](int x, int y, int z) { return sig.strongPowered(ch, x, y, z); }};
}

bool Game::cameraRaycast(Vec3 origin, Vec3 dir, float maxDist, float& hitDist) {
  if (!sim_ || !active_) return false;
  bool hit = false;
  float dist = maxDist;
  sim_->runBetweenTicks([&] {
    int h[3], prev[3];
    if (!litholattice::raycast(active_->chunks, origin.x, origin.y, origin.z, dir.x, dir.y, dir.z,
                               maxDist, h, prev, &blockRegistry()))
      return;
    hit = true;
    // Exact entry distance: the ray enters the hit cell through the face
    // shared with the previous (empty) cell â€” the axis where they differ.
    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    for (int a = 0; a < 3; ++a) {
      if (h[a] == prev[a] || d[a] == 0.f) continue;
      const float face = d[a] > 0.f ? float(h[a]) : float(h[a] + 1);
      dist = (face - o[a]) / d[a];
    }
    dist = std::clamp(dist, 0.f, maxDist);
  });
  hitDist = dist;
  return hit;
}

// Cortana 2026-10-16 [GAME] (JD): the world clock follows the sim's worldTick. Each tick adds
// unitsPerTick(target TPS) (world_clock.h), so a day is 32 real minutes at any rate. A sim
// whose worldTick went backwards (world reload) is resynced, never fast-forwarded.
void Game::advanceWorldClock() {
  if (!sim_) return;
  const uint64_t wt = sim_->stats().worldTick;
  if (!clockSynced_ || wt < lastClockTick_) { lastClockTick_ = wt; clockSynced_ = true; return; }
  const uint64_t d = wt - lastClockTick_;
  lastClockTick_ = wt;
  if (d > 0) worldclock::advance(worldClock_, d, sim_->stats().targetTps);
}

// Debug / HUD toggles (F-keys: the Debug context, live even while typing or in the inventory).
void Game::handleToggles() {
  if (input_.pressed(Action::CycleTPS)) {
    int i = 0;
    // Cortana 2026-10-15 1218 (JD): F7 steps the full power-of-two ladder
    // 32..512 (kSettingsTpsModes, same list as the settings dropdown); it
    // used to stop at 256 (kTickModes) so 512 needed the CLI. No EUV/flag gate.
    while (i < kSettingsTpsModeCount && kSettingsTpsModes[i] != cfg_.tps) ++i;
    const int next = kSettingsTpsModes[(i + 1) % kSettingsTpsModeCount];
    logInfo("tick rate change: %d -> %d TPS", cfg_.tps, next);
    cfg_.tps = next; sim_->setTickRate(cfg_.tps);
    settings_.tps = next; saveSettings();  // the settings file stays the source of truth
  }
  if (input_.pressed(Action::ToggleEUV)) {
    cfg_.euv = !cfg_.euv; sim_->toggleDomain(kEuvDomain); logInfo("EUV domain %s", cfg_.euv ? "on" : "off");
    settings_.euv = cfg_.euv; saveSettings();
  }
  if (input_.pressed(Action::SwitchDim)) switchDimension(active_->id == "wafer" ? "organic" : "wafer");
  if (input_.pressed(Action::ToggleControls)) showControls_ = !showControls_;
  if (input_.pressed(Action::ToggleHUD)) showHud_ = !showHud_;
  if (input_.pressed(Action::Screenshot)) logInfo("%s", requestScreenshot(ScreenshotRequest{}).c_str());  // 2026-10-25 (JD): F2
  if (input_.pressed(Action::ToggleDebug)) showDebug_ = !showDebug_ && !cfg_.capture;  // capture: F3 stays off
  if (input_.pressed(Action::Inventory)) setInventoryOpen(!inventoryOpen_);
  if (input_.pressed(Action::ReloadResources)) reloadResources();
  // PlayM4 (JD t4701u): numpad 7 compiles a Trace-in-time chip around the player, 8 stops it, 9 cycles its speed.
  if (sim_ && (input_.pressed(Action::LithoCompile) || input_.pressed(Action::LithoStop) || input_.pressed(Action::LithoSpeed))) {
    const int a = input_.pressed(Action::LithoCompile) ? 1 : input_.pressed(Action::LithoStop) ? 2 : 3;
    EditCmd c{EditCmd::Kind::Command};
    c.command = SimCommand{kCmdLitho, int(std::floor(double(player_.pos.x))), int(std::floor(double(player_.pos.y))),
                           int(std::floor(double(player_.pos.z))), a, 0};
    sim_->edit(c);
  }
  if (soak_.active() && !inMenu() && pauseState_ == PauseState::Playing && !worldLoad_) {  // 2026-10-31: LL_WORLD_LOAD_SOAK test hook
    const std::string step = soak_.tick();
    if (!step.empty()) {
      logInfo("soak: step '%s' | %s | dim=%s", step.c_str(), cWorldLoadSoak::processStats().c_str(), active_ ? active_->id.c_str() : "-");
      if (step == "save") saveWorld("soak", false);
      else if (step == "quit") quit_ = true;
      else logInfo("soak: %s", requestWorldLoad(step).c_str());
    }
  }
  if (auto r = saver_.poll()) {  // 2026-10-31 (JD, Save World): the async save landed
    if (r->ok)
      logInfo("save (%s): '%s' %zu columns on disk | tick-thread snapshot %.2f ms (%.1f MB copied; boundary after %.1f ms) | "
              "pool write %.1f ms | region files %.1f MB | total %.1f ms | sim never stopped",
              r->reason.c_str(), r->dim.c_str(), r->columns, r->snapshotMs, double(r->bytesCopied) / 1048576.0, r->queueMs,
              r->commitMs, double(r->bytesOnDisk) / 1048576.0, r->totalMs);
    else logError("save (%s) FAILED: '%s': %s", r->reason.c_str(), r->dim.c_str(), r->error.c_str());
    lastAsyncSave_ = std::move(r);
  }
  if (input_.pressed(Action::SaveWorld)) saveWorld("F5", false);
  // 2026-10-31: LL_AUTOSAVE_SECONDS=N overrides the autosave interval (profiling / testing; 0 = off).
  static const double kAutosaveEnv = [] { const char* e = std::getenv("LL_AUTOSAVE_SECONDS"); return e ? std::atof(e) : -1.0; }();
  const double autosaveS = kAutosaveEnv >= 0 ? kAutosaveEnv : double(cfg_.autosaveSeconds);
  if (save_ && autosaveS > 0 && !saver_.busy() && secondsBetween(lastSave_, Clock::now()) > autosaveS) saveWorld("autosave", false);
  if (input_.pressed(Action::ScriptEditor)) {
    if (editor_) editor_->toggle();
    else logInfo("script editor: no scripts folder");
  }
}

// F9: rebuild textures, models, materials and shaders from the packs, then remesh everything.
void Game::reloadResources() {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  const auto t0 = Clock::now();
  resources_.reload();
  resources_.apply(renderer_);
  sim_->requestRemeshAll();
  logInfo("hot reload done in %.1f ms (remeshing all loaded chunks)", msBetween(t0, Clock::now()));
}

// MCP wiring (P2c 2026-10-13, review F-M9): the backend bundle + the
// grant/revoke console commands the engine cMcpServer header assumes the
// game exposes. The server itself (and its stdio thread) is owned by
// main.cpp under --mcp-stdio; without the flag none of this runs and the
// `mcp` command does not exist.
litholattice::mcp::McpBackends Game::mcpBackends() {
  litholattice::mcp::McpBackends b;
  const auto noDelete = [](void*) {};
  b.controller = std::shared_ptr<litholattice::mcp::IPlayerController>(
      static_cast<litholattice::mcp::IPlayerController*>(&mcpController_), noDelete);
  b.player = std::shared_ptr<litholattice::mcp::IPlayerView>(
      static_cast<litholattice::mcp::IPlayerView*>(&mcpView_), noDelete);
  b.vision = std::shared_ptr<litholattice::mcp::IVisionCapture>(
      static_cast<litholattice::mcp::IVisionCapture*>(&mcpSenses_), noDelete);
  b.audio = std::shared_ptr<litholattice::mcp::IAudioCapture>(
      static_cast<litholattice::mcp::IAudioCapture*>(&mcpSenses_), noDelete);
  // 2026-11-10 (JD t5033u, PCHIE P7): world_get_block via the engine's cross-thread single-block read
  // (MCP-GAME-SEAM.md section 3). Still gated by the world_read grant; the agent starts at mask 0.
  b.world = std::make_shared<cMcpWorldReader>(
      [this](int x, int y, int z) -> uint16_t { return sim_ ? uint16_t(sim_->getBlock(x, y, z)) : uint16_t(0); });
  return b;
}

void Game::setMcpServer(litholattice::mcp::cMcpServer* server) {
  mcpServer_ = server;
  if (!server || mcpCommandsRegistered_) return;
  mcpCommandsRegistered_ = true;
  // `mcp grant <perm...>`, `mcp revoke <perm...>`, `mcp agent`.
  // Cheat-flagged ON PURPOSE: granting an agent permissions is an
  // authority change, and the console's cheat gate is exactly "full-
  // authority local caller only" â€” a restricted caller (an MCP agent via
  // a console seam, a mod) must never be able to grant ITSELF bits.
  // The local player (cheats allowed in this single-player context,
  // full authority) keeps the approval flow the engine header describes.
  litholattice::console::CommandFlags cheat;
  cheat.cheat = true;
  std::string err;
  consoleRegistry_.registerCommand(
      "mcp", "mcp grant|revoke <perm...> | mcp agent - MCP agent permission approval (cheat)",
      [this](litholattice::console::cConsoleReg& r, const litholattice::console::ConsoleArgs& a) {
        if (!mcpServer_) {
          r.print("mcp: no server running (start with --mcp-stdio)");
          return;
        }
        if (a.size() >= 2 && a[1] == "agent") {
          r.print("mcp agent '" + mcpServer_->agent().name + "': permissions = " +
                  litholattice::mcp::permMaskNames(mcpServer_->permissionMask()));
          return;
        }
        if (a.size() >= 3 && (a[1] == "grant" || a[1] == "revoke")) {
          uint32_t bits = 0;
          for (size_t i = 2; i < a.size(); ++i) {
            const uint32_t bit = litholattice::mods::permissionBitForName(a[i]);
            if (!bit) {
              r.print("mcp: unknown permission '" + a[i] + "' (see `mcp agent` for the granted set)");
              return;
            }
            bits |= bit;
          }
          if (a[1] == "grant") mcpServer_->grantPermissions(bits);
          else mcpServer_->revokePermissions(bits);
          r.print(std::string("mcp: ") + (a[1] == "grant" ? "granted " : "revoked ") +
                  litholattice::mcp::permMaskNames(bits) + "; agent mask now " +
                  litholattice::mcp::permMaskNames(mcpServer_->permissionMask()));
          return;
        }
        r.print("usage: mcp grant <perm...> | mcp revoke <perm...> | mcp agent");
      },
      cheat, &err);
  if (!err.empty()) logError("mcp: command registration failed: %s", err.c_str());
}

// Gameplay context only (contexts already hide these while typing or in the inventory).
cMcpPlayerView::GameState Game::snapshotMcpPlayer() const {
  // Archie PlayI: lock-free COW snapshot (atomic shared_ptr); no mcpStateMutex_.
  if (auto p = std::atomic_load_explicit(&mcpPlayerState_, std::memory_order_acquire)) return *p;
  return cMcpPlayerView::GameState{};
}

litholattice::LocomotionInput Game::sampleLocomotionInput() const {
  // Wish direction: the move actions in the player's facing frame, rotated
  // by yaw into world space (the ILocomotion contract is world-space,
  // magnitude <= 1; partial analog deflection = partial speed). The basis
  // is the one the pre-locomotion fly movement used: flat forward
  // (cos yaw, 0, sin yaw), right = forward x up.
  litholattice::LocomotionInput in;
  double fwd = double(input_.value(Action::MoveForward)) - double(input_.value(Action::MoveBack));
  double side = double(input_.value(Action::MoveRight)) - double(input_.value(Action::MoveLeft));
  if (const double len = std::sqrt(fwd * fwd + side * side); len > 1.0) {
    fwd /= len;
    side /= len;
  }
  const double cy = std::cos(double(player_.yaw)), sy = std::sin(double(player_.yaw));
  in.wishX = cy * fwd - sy * side;
  in.wishZ = sy * fwd + cy * side;
  in.jump = input_.held(Action::Jump);
  in.sneak = input_.held(Action::Sneak);
  in.sprint = input_.held(Action::Sprint);
  in.crawl = crawlToggle_;  // Cortana 2026-10-15 2215 (JD D8): C toggles crawl (forced crawl unchanged)
  return in;
}

std::string Game::toggleFly(const std::string& arg) {
  using litholattice::LocomotionMode;
  bool fly;
  if (arg.empty())
    fly = !locoSnap().flying;
  else if (arg == "on")
    fly = true;
  else if (arg == "off")
    fly = false;
  else
    return "fly: usage: fly [on|off]";
  locoSetMode(sim_, locomotion_, fly ? LocomotionMode::Fly : LocomotionMode::Walk);
  player_.flying = fly;  // keep the cPlayer flag a faithful mode mirror
  logInfo("fly: locomotion mode -> %s", fly ? "fly" : "walk");
  return std::string("fly: ") + (fly ? "on (locomotion mode: fly)" : "off (locomotion mode: walk)");
}

// 2026-10-31 (JD): Alt + wheel adjusts a component's 1..64 setting. LL_ALT_WHEEL=0 kill.
bool Game::altWheelOn() {
  static const bool on = [] { const char* e = std::getenv("LL_ALT_WHEEL"); return !(e && e[0] == '0'); }();
  return on;
}

void Game::handleHotbar() {
  for (int i = 0; i < kHotbarSize && i < kHotbarKeySlots; ++i)
    if (input_.pressed(hotbarAction(i))) hot_ = i;
  // In third/follow the wheel zooms the camera instead of cycling slots.
  const int steps = (camera_.consumesWheel() || (altWheelOn() && input_.held(Action::Secondary)))  // Alt + wheel = setting
                        ? 0
                        : int(input_.pressed(Action::HotbarNext)) - int(input_.pressed(Action::HotbarPrev));
  hot_ = ((hot_ + steps) % kHotbarSize + kHotbarSize) % kHotbarSize;
  // Q-drop (JD ruling 2026-11-10): in play, Q throws from the selected
  // hotbar slot into the world (one item; Shift+Q the whole stack).
  // While the inventory screen is open the screen owns Q (hovered
  // slot, routed in routeUiInput), so this path stands down there.
  if (!inventoryOpen_ && input_.pressed(Action::Drop)) {
    const bool whole = input_.held(Action::Sneak) || input_.devices().keyDown(Key::LeftShift) ||
                       input_.devices().keyDown(Key::RightShift);
    dropFromSlot(hot_, whole);
  }
  if (input_.pressed(Action::Jump)) {
    const double now = secondsBetween(Clock::time_point{}, Clock::now());
    if (lastJumpTap_ >= 0 && now - lastJumpTap_ < 0.30) {
      toggleFly("");  // double-tap Jump toggles flight (the `fly` command's twin)
      lastJumpTap_ = -1.0;
    } else {
      lastJumpTap_ = now;
    }
  }
}

// Esc / Start backs out one level: typing -> inventory -> mouse capture. On the launch menu
// (no modal open) Esc exits to the desktop.
void Game::handlePause() {
  if (!input_.pressed(Action::Pause)) return;
  if (menuState_ == MenuState::cMenu) { quit_ = true; return; }
  if (typing_) endTyping();
  else if (inventoryOpen_) setInventoryOpen(false);
  else setPaused(pauseState_ != PauseState::MenuPaused);  // Esc toggles: Esc while paused resumes
}

// ---------------------------------------------------------------------------
// Pause state machine (P2b-2, review CH-H7 + G-M2). ONE owner for both
// Game::paused_ and the sim's paused flag:
//
//   Playing     -- sim runs (subject to cfg_.pauseSim / bench below)
//   MenuPaused  -- Esc pause menu, or the launch menu (menuState_)
//   TickFrozen  -- tick_freeze / tick_step debugger freeze
//   HsFrozen    -- timing-handshake watchdog freeze, mirrored from the sim
//                  (update() watches the hsFrozen stats bit)
//
// setPaused(bool) is the Esc/console-pause face of the machine: pausing
// lands in MenuPaused (a debugger freeze is superseded by the menu while
// it is up), resuming lands in Playing â€” and entering Playing from ANY
// frozen state clears a latched handshake freeze first, so Esc-resume
// after tick_freeze can no longer leave the authority latch armed behind
// a game that believes it is running (the CH-H7 re-freeze loop).

/// The single sim_->setPaused() computation. Every other site that used
/// to write the sim flag now routes through here (or through
/// setPauseState, which ends here).
void Game::armFrameHashSteppedCapture() {
  frameHashStepped_ = true;
  setPauseState(PauseState::TickFrozen);
}

void Game::frameHashStepOne() {
  if (sim_) sim_->stepTicks(1);
}

uint64_t Game::worldTick() const {
  return sim_ ? sim_->stats().worldTick : 0;
}

void Game::applySimPause() {
  paused_ = pauseState_ != PauseState::Playing;
  if (sim_) {
    // --frame-hash stepped capture: force pause even under --bench (bench normally ignores pause).
    if (frameHashStepped_ || ioTickHold_) sim_->setPaused(true);  // B9: save/load hold wins (also over --bench)
    else sim_->setPaused(paused_ && cfg_.pauseSim && !benchSet_);
  }
}

// B9 save/load tick pause (JD 2026-11-11 10:00 PM ET): on exit, pause ticks THEN save every
// component THEN leave; on load, restore every component WHILE ticks are paused THEN unpause.
// Uses the existing sim pause flag (applySimPause) - no second pause mechanism.
namespace { std::vector<std::string>* g_ioOrderTrace = nullptr; }
void Game::setIoOrderTrace(std::vector<std::string>* sink) { g_ioOrderTrace = sink; }
void Game::traceIo(const std::string& event) const {
  if (!g_ioOrderTrace) return;
  g_ioOrderTrace->push_back(event + " tick=" + std::to_string(sim_ ? sim_->stats().worldTick : 0) +
                            " paused=" + (sim_ && sim_->paused() ? "1" : "0") + " running=" + (sim_ && sim_->running() ? "1" : "0") +
                            " hold=" + (ioTickHold_ ? "1" : "0"));
}
void Game::holdTicksForIo(const char* why) {
  ioTickHold_ = true;
  applySimPause();  // no new tick starts from here
  if (sim_ && sim_->running()) {
    // Wait out a tick already in flight (ticks hold the block gate across their whole window).
    // Bounded: a frozen / poisoned handshake refuses the hold without being busy, and the
    // save's own stop() path stays the authority there.
    ::litholattice::cGateLock<litholattice::cBlockGate> h;
    bool busy = false;
    const auto until = Clock::now() + std::chrono::milliseconds(2000);
    while (!sim_->tryHoldBetweenTicks(h, &busy) && busy && Clock::now() < until)
      std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  logInfo("tick pause: %s - ticks held (world tick %llu)", why, (unsigned long long)worldTick());
  traceIo(std::string("hold:") + why);
}
void Game::releaseTicksAfterIo(const char* why) {
  traceIo(std::string("release:") + why);
  ioTickHold_ = false;
  applySimPause();  // back to exactly what the pause machine owns
  logInfo("tick pause: %s - ticks released (world tick %llu, sim %s)", why, (unsigned long long)worldTick(),
          sim_ && sim_->paused() ? "still paused by the pause machine" : "ticking");
}

void Game::setPauseState(PauseState s) {
  if (s == pauseState_) { applySimPause(); return; }
  const PauseState old = pauseState_;
  pauseState_ = s;
  if (s == PauseState::Playing && sim_ && sim_->handshakeFrozen()) {
    // Handshake watchdog recovery (hs-design Â§4.4): clear the freeze
    // latch explicitly BEFORE unpausing â€” a plain unpause must not
    // silently resume past a frozen phase, and skipping this is what
    // re-froze the sim behind a "running" game in CH-H7.
    sim_->resetHandshakeAfterFreeze();
    hsFreezeLogged_ = false;
  }
  if (s == PauseState::MenuPaused && menuState_ == MenuState::Playing) {
    input_.push(InputContext::Inventory);  // an opaque menu context: gameplay actions gated
    ui_.openPause();
  } else if (old == PauseState::MenuPaused && menuState_ == MenuState::Playing) {
    input_.pop(InputContext::Inventory);
    ui_.closePause();
    captured_ = true;
  }
  applySimPause();
}

// Pause menu: frees the cursor and (single-player, unless configured off / bench) stops the sim.
void Game::setPaused(bool p) {
  // The launch menu owns the pause state (audit MEDIUM): a console `pause` in
  // the menu used to pop the inventory context and unpause the sim behind the
  // menu overlay. No-op here; b.pauseToggle reports it.
  if (menuState_ == MenuState::cMenu) return;
  setPauseState(p ? PauseState::MenuPaused : PauseState::Playing);
}

void Game::onFocusChanged(bool focused) {
  // 2026-10-25 (JD: alt-tab no longer pauses): log every edge and the gate so the next report shows why.
  logInfo("focus: %s (menu=%d bench=%d inventory=%d editor=%d pause=%d)", focused ? "gained" : "lost", int(menuState_ == MenuState::cMenu),
          int(benchSet_), int(inventoryOpen_), int(editorOpen()), int(pauseState_));
  // lkbed item 2 (SCRATCH 2026-11-11): tiny opt-in for UNATTENDED MCP runs only. LL_MCP_NO_FOCUS_PAUSE=1
  // keeps play running on focus loss; default (unset) behaviour is unchanged.
  static const bool kNoFocusPause = [] { const char* e = std::getenv("LL_MCP_NO_FOCUS_PAUSE"); return e && std::strcmp(e, "1") == 0; }();
  if (!focused && kNoFocusPause) logInfo("focus: lost - NOT pausing (LL_MCP_NO_FOCUS_PAUSE=1, unattended MCP opt-in)");
  if (!focused && !kNoFocusPause && menuState_ == MenuState::Playing && !benchSet_ && !inventoryOpen_ && !editorOpen() &&
      pauseState_ == PauseState::Playing) {
    setPaused(true);
    pausedByFocus_ = true;
    captured_ = false;  // Cortana 2026-10-15 1213: the OS freed the cursor; the resume click recaptures  // a click in the world (not on the dialog) resumes
  }
  // 2026-10-31 (JD: after alt-tab + maximize the mouse was not recaptured): a pause that focus
  // loss opened is undone when focus returns - back to play, cursor recaptured (main.cpp applies
  // the grab). A pause the player opened (Esc) stays. Also un-sticks the post-load focus blip.
  if (focused && pausedByFocus_) {
    pausedByFocus_ = false;
    if (menuState_ == MenuState::Playing && pauseState_ == PauseState::MenuPaused && !inventoryOpen_ && !editorOpen()) {
      ui_.closePause();
      setPaused(false);
      captured_ = true;
      logInfo("focus: back - resumed play, cursor recaptured");
    }
  }
}

// Launch menu: the world stays loaded, the sim pauses, the cursor is freed and the camera
// runs a slow side-scrolling dolly across the wafer behind the menu overlay.
void Game::enterMenu() {
  menuState_ = MenuState::cMenu;
  menuCameraT_ = 0.f;
  // Snapshot the player BEFORE the dolly starts overwriting it (audit MEDIUM):
  // enterPlay() restores this, so the menu never eats the saved position.
  menuReturnPos_ = player_.pos; menuReturnYaw_ = player_.yaw; menuReturnPitch_ = player_.pitch;
  menuSnapshotValid_ = true;
  menuCamX_ = player_.pos.x; menuCamY_ = player_.pos.y + 6.f; menuCamZ_ = player_.pos.z;
  ui_.closePause();
  ui_.closeSettings();
  // The launch menu is a MenuPaused state in the pause machine (P2b-2),
  // assigned directly: setPauseState would also open the Esc pause menu
  // UI, which is not this menu. The input-context push mirrors the old
  // behaviour exactly (only when coming from live play).
  if (pauseState_ == PauseState::Playing)
    input_.push(InputContext::Inventory);  // an opaque menu context: gameplay actions gated
  pauseState_ = PauseState::MenuPaused;
  applySimPause();
  captured_ = false;
  ui_.openMenu(cfg_.version);
  logInfo("menu: launch menu (dimension '%s' loaded, sim paused)", active_->id.c_str());
  prewarmSwarm();  // P6: the title screen is idle time - warm the swarm for Play
}

// Play: unpause and hand control to the player instantly (no reload).
void Game::enterPlay() {
  // PlayL JD WASD/stuck: enterMenu() assigns MenuPaused directly (bypassing setPauseState).
  // A second enterPlay() while already Playing used to return early and leave MenuPaused +
  // captured_=false - keys/shift register and fly double-tap still fire, but sampleLocomotionInput
  // is gated on playing=captured&&!paused so the body never steps (F3 wish overwrite hid this).
  ui_.closeMenu();
  menuState_ = MenuState::Playing;
  // S6 (2026-10-28): on this exact transition frame, zero the sim's
  // accumulated wall-clock time debt - RESOURCE_LOAD / menu time must
  // not land as a catch-up burst + dropped-tick spike in play.
  if (sim_) sim_->resetTimeDebt();
  // Restore the pre-dolly player transform (see enterMenu): the cinematic
  // overwrites player_.pos every menu frame, which used to defeat save/restore.
  if (menuSnapshotValid_) {
    player_.pos = menuReturnPos_; player_.yaw = menuReturnYaw_; player_.pitch = menuReturnPitch_;
  }
  if (pauseState_ == PauseState::MenuPaused) {
    input_.pop(InputContext::Inventory);  // enterMenu may have pushed; pause UI may be absent
    ui_.closePause();
  }
  pauseState_ = PauseState::Playing;
  pausedByFocus_ = false;
  captured_ = true;
  applySimPause();
  logInfo("menu: play - creative/testing");
}


// 2026-10-30 (JD): menu Play -> the world picker (worlds + named spawns).
void Game::openWorldPicker() {
  std::vector<std::string> spawns{"spawn"};
  for (const auto& m : tpMarks_) spawns.push_back(m.first);
  const int cur = cWorldCatalog::instance().indexFor(cfg_.benchScene, cfg_.dimension, cfg_.testWorldExamples);
  worldPicker_.open(ui_.root(), std::max(1.f, ui_.root().theme().scale()), cur, std::move(spawns),
                    [this](int w, const std::string& s) { pickWorld(w, s); }, [this] { openWorldFolderDialog(); });
  logInfo("menu: world picker open (current world %d)", cur);
}

void Game::pickWorld(int world, const std::string& spawn) {
  const auto& cat = cWorldCatalog::instance();
  if (world == cat.indexFor(cfg_.benchScene, cfg_.dimension, cfg_.testWorldExamples) && !cfg_.worldFromFolder) {
    if (spawn.empty()) enterPlay();
    else startAt(spawn);
    return;
  }
  // Archie Phase 2 (JD 2026-11-04): in-process load is the only path (no relaunch, no argv hand-off).
  std::vector<std::string> args = cat.entries()[size_t(world)].args;
  if (!spawn.empty() && spawn != "spawn") { args.push_back("--spawn"); args.push_back(spawn); }
  worldLoad_ = WorldLoadRequest{std::move(args), "", spawn, cat.entries()[size_t(world)].id};
  logInfo("menu: world picker -> %s (spawn '%s'): loading in-process after the exit save", cat.entries()[size_t(world)].id.c_str(), spawn.c_str());
}

void Game::openWorldFolderDialog() {
  std::string start;
  if (!cfg_.saveDir.empty()) start = std::filesystem::path(cfg_.saveDir).parent_path().string();
  // 2026-10-31 (JD: hang): the OS dialog runs on its own thread; update() polls the result.
  if (!cFolderPicker::start("Open a LithoKraft world folder", start)) logInfo("menu: world folder: dialog already open");
}

void Game::pollWorldFolderDialog() {
  bool chose = false;
  std::string folder;
  if (!cFolderPicker::poll(chose, folder)) return;
  if (!chose) { logInfo("menu: world folder: cancelled"); return; }
  logInfo("menu: %s", requestWorldLoadFolder(folder, "").c_str());
}

std::string Game::requestWorldLoad(const std::string& arg) {
  const auto& cat = cWorldCatalog::instance().entries();
  for (size_t i = 0; i < cat.size(); ++i)
    if (cat[i].id == arg) {
      pickWorld(int(i), "spawn");
      return worldLoad_ || quit_ ? "world_load: loading '" + arg + "'" : "world_load: '" + arg + "' is the loaded world";
    }
  return requestWorldLoadFolder(arg, "");
}

std::string Game::requestWorldLoadFolder(const std::string& folder, const std::string& spawn) {
  std::error_code ec;
  if (folder.empty() || !std::filesystem::is_directory(folder, ec)) return "world_load: not a folder: '" + folder + "'";
  WorldLoadRequest r;
  r.folder = folder;
  r.label = folder;
  if (!spawn.empty()) r.args = {"--spawn", spawn};
  worldLoad_ = std::move(r);
  return "world_load: loading '" + folder + "' (" +
         (cWorldConfigBuilder::looksLikeWorld(folder) ? std::string("saved world") : std::string("no world there yet: a new world is created")) + ")";
}

void Game::startAt(const std::string& spawn) {
  enterPlay();
  logInfo("%s", teleportCmd({spawn}).c_str());
}

void Game::applyMenuCamera() {
  // Side view, panning laterally: glide along +x while looking horizontally across the
  // silicon surface (-z), a slow sine so the dolly never jumps.
  player_.pos = {menuCamX_ + 42.f * std::sin(menuCameraT_ * 0.06f), menuCamY_, menuCamZ_};
  player_.yaw = -1.5708f;  // forward = -z
  player_.pitch = -0.10f;
}

static DisplayMode displayModeFromIndex(int i) {
  DisplayMode m = DisplayMode::Windowed;
  if (i >= 0 && i < kSettingsDisplayModeCount) litholattice::parseDisplayMode(kSettingsDisplayModes[i], m);
  return m;
}

// A settings control changed: apply the live ones now, persist everything (tick radius and
// vsync are boot-only and note it in the UI).
void Game::onSettingsChanged(const GameSettings& s) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  const GameSettings old = settings_;
  settings_ = s;
  if (s.masterVolume != old.masterVolume) audio_.post(AudioCommand::masterGain(s.masterVolume));
  if (s.muted != old.muted) audio_.setEnabled(!s.muted);
  if (s.hudScale != old.hudScale) ui_.setHudScalePref(s.hudScale);
  if (s.displayMode != old.displayMode && displayApplier_) displayApplier_(displayModeFromIndex(s.displayMode));
  if (s.tps != old.tps && sim_) {
    sim_->setTickRate(s.tps);
    logInfo("settings: tick rate -> %d TPS", s.tps);
  }
  if (s.euv != old.euv && sim_) {
    sim_->toggleDomain(kEuvDomain);
    logInfo("settings: EUV domain %s", s.euv ? "on" : "off");
  }
  if (s.timingHandshake != old.timingHandshake || s.handshakePhases != old.handshakePhases) {
    // Not live-swappable: registration happens at cSimThread construction.
    cfg_.timingHandshake = s.timingHandshake;
    cfg_.handshakePhases = s.handshakePhases;
    logInfo("settings: timing handshake -> %s, %d phases/cycle (applies on world load)",
            s.timingHandshake ? "on" : "off", s.handshakePhases);
  }
  if (s.lodDetail != old.lodDetail) {  // Archie Phase 1: far-LOD slider; re-mesh at the new ladder
    swLodRings_.setDetail(s.lodDetail);
    swLodRings_.setView(effectiveStreamRadius(cfg_));
    if (sim_) sim_->requestRemeshAll();
    logInfo("settings: render.lod_detail -> %d (edges %d/%d/%d)", s.lodDetail, swLodRings_.edge(0), swLodRings_.edge(1), swLodRings_.edge(2));
  }
  if (s.bulbShellSolid != old.bulbShellSolid) {  // 1551 (JD): re-mesh with the new bulb shell
    litholattice::setShellSolid(s.bulbShellSolid);
    if (sim_) sim_->requestRemeshAll();
    logInfo("settings: render.bulb_shell -> %s", s.bulbShellSolid ? "solid" : "ghostly");
  }
  if (resolvedLeavesFancy(s) != resolvedLeavesFancy(old)) {  // 2026-10-16: re-mesh with the new leaf look
    litholattice::setLeavesFancy(resolvedLeavesFancy(s));
    if (sim_) sim_->requestRemeshAll();
    logInfo("settings: render.leaves -> %s", resolvedLeavesFancy(s) ? "fancy" : "fast");
  }
  if (resolvedFoliageDensity(s) != resolvedFoliageDensity(old)) {  // 2026-10-18 (Canopy, F-B): re-mesh with the new foliage density
    litholattice::setFoliageDensity(resolvedFoliageDensity(s));
    if (sim_) sim_->requestRemeshAll();
    logInfo("settings: render.foliage_density -> %d", resolvedFoliageDensity(s));
  }
  if (s.pbrTier != old.pbrTier) {
    renderer_.setPbrTier(static_cast<litholattice::PbrTransparencyTier>(s.pbrTier));
    logInfo("settings: PBR transparency tier -> %d", s.pbrTier);
  }
  if (s.ambientMode != old.ambientMode) {
    renderer_.setAmbientMode(ambientModeFromInt(s.ambientMode));
    logInfo("settings: ambient mode -> %d", s.ambientMode);
  }
  if (s.frustumMarginDeg != old.frustumMarginDeg) {
    renderer_.setFrustumMarginDeg(s.frustumMarginDeg);
    logInfo("settings: frustum margin -> %.1f deg", s.frustumMarginDeg);
  }
  if (s.modelDither != old.modelDither || s.modelDitherNear != old.modelDitherNear ||
      s.modelDitherFar != old.modelDitherFar) {
    renderer_.setModelDither(s.modelDither, s.modelDitherNear, s.modelDitherFar);
    logInfo("settings: model dither -> %s, band %.2f..%.2f blocks",
            s.modelDither ? "on" : "off", s.modelDitherNear, s.modelDitherFar);
  }
  if (s.aaMode != old.aaMode) {
    renderer_.setAaMode(s.aaMode.c_str());
    logInfo("settings: render.aa -> %s", s.aaMode.c_str());
  }
  if (s.texFilter != old.texFilter || s.anisotropy != old.anisotropy) {  // PROVISIONAL JD 2026-10-24: live
    renderer_.setTextureFiltering(s.texFilter, s.anisotropy);
    logInfo("settings: render.tex_filter -> %d, render.anisotropy -> %dx (device max %dx)", s.texFilter, s.anisotropy,
            renderer_.maxAnisotropy());
  }
  if (s.emissiveLight != old.emissiveLight) { renderer_.setEmissiveLight(s.emissiveLight); logInfo("settings: render.emissive_light -> %s", s.emissiveLight ? "on" : "off"); }
  if (s.mipmaps != old.mipmaps) { renderer_.setMipmaps(s.mipmaps); logInfo("settings: render.mipmaps -> %s", s.mipmaps ? "on" : "off"); }
  if (s.ao != old.ao) { renderer_.setAo(s.ao); logInfo("settings: render.ao -> %s", s.ao ? "on" : "off"); }
  if (s.shimmer != old.shimmer) { renderer_.setShimmer(s.shimmer); logInfo("settings: render.shimmer -> %s", s.shimmer ? "on" : "off"); }
  if (s.texRes != old.texRes) { renderer_.setTexRes(std::min(s.texRes, 64)); logInfo("settings: render.tex_res -> %d (applies on the next texture load / F9)", s.texRes); }
  if (s.bloom != old.bloom) {  // Cortana 2026-10-16
    renderer_.setBloom(resolvedBloom(s));
    logInfo("settings: render.bloom -> %s", resolvedBloom(s) ? "on" : "off");
  }
  if (s.asciiMode != old.asciiMode || s.asciiCols != old.asciiCols) {
    renderer_.setAsciiMode(s.asciiMode, s.asciiCols);
    logInfo("settings: ascii -> %s, %d cols", s.asciiMode ? "on" : "off", s.asciiCols);
  }
  if (s.asciiSet != old.asciiSet) {
    renderer_.setAsciiGlyphSet(s.asciiSet.c_str());
    logInfo("settings: ascii set -> %s", s.asciiSet.c_str());
  }
  if (s.asciiValueSpan != old.asciiValueSpan) {
    renderer_.setAsciiValueSpan(s.asciiValueSpan);
    logInfo("settings: ascii value span -> %d (full 0-255 default, JD 2026-10-13)",
            s.asciiValueSpan);
  }
  if (s.cometTailFrames != old.cometTailFrames || s.velocityMargin != old.velocityMargin ||
      s.velocityMarginGain != old.velocityMarginGain || s.twoTier != old.twoTier ||
      s.twoTierOuterDeg != old.twoTierOuterDeg) {
    pushCullProtoConfig();
    logInfo("settings: cull prototypes pushed");
  }
  if (s.fpsCap != old.fpsCap) {
    // No Game-side action: the main loop owns the cFramePacer and reads
    // settings().fpsCap every frame (render.fps_cap wiring, 2026-10-13),
    // so the new cap applies on the next frame boundary.
    logInfo("settings: fps cap -> %d FPS (0 = off, vsync-only)", s.fpsCap);
  }
  if (s.renderer != old.renderer) {  // JD 2026-10-24: renderer switch = save + relaunch (no live GPU teardown)
    logInfo("settings: renderer -> %s; saving the world and relaunching", s.renderer.c_str());
    restart_ = true; quit_ = true;
  }
  // settings_.mesher is boot-only (world (re)load): no live action here.
  saveSettings();
  // Push UI-driven changes back into the cvars (cvar -> setting echo is
  // guarded inside SettingsCvars, so this cannot recurse).
  if (settingsCvars_) settingsCvars_->syncFrom(settings_);
}

void Game::saveSettings() {
  if (cfg_.settingsPath.empty()) return;
  if (!settings_.save(cfg_.settingsPath)) logError("settings: cannot write %s", cfg_.settingsPath.c_str());
}

void Game::pushCullProtoConfig() {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  litholattice::CullProtoConfig cfg;
  cfg.cometTailFrames = settings_.cometTailFrames;
  cfg.velocityMargin = settings_.velocityMargin;
  cfg.velocityMarginGain = settings_.velocityMarginGain;
  cfg.twoTier = settings_.twoTier;
  cfg.twoTierOuterDeg = settings_.twoTierOuterDeg;
  renderer_.setCullProtoConfig(cfg);
}

void Game::setInventoryOpen(bool open) {
  if (open == inventoryOpen_) return;
  inventoryOpen_ = open;
  if (open) {
    input_.push(InputContext::Inventory);
    ui_.openInventory(hot_, carrySnapshot());
    ui_.refreshCursor(cursorStack_);  // empty by the close chain's guarantee; the display starts in sync
  } else {
    // Cursor stack (2026-11-11): BOTH close paths funnel through here
    // (E / Escape self-close arrives via the inventoryClosed callback
    // bound to this function; a game-asked close calls it directly),
    // so the return chain runs here, before the screen goes away:
    // closing the screen can never destroy or duplicate the stack in
    // the mouse-hand.
    returnCursorStack();
    input_.pop(InputContext::Inventory);
    ui_.closeInventory();
    captured_ = true;  // closing returns to play
  }
}

// Block-palette click (creative give): a FULL stack of the block into the
// inventory hotbar â€” the selected slot (shift: the first free slot, else
// the selected slot). The palette writes the same inventory the HUD reads
// and player.lat persists (P2b-1); there is no second store to write to.
// Playtest 2026-10-16: the inventory dialog's carry grid; (2026-10-31 JD: now a one-way copy, was: swap) carry slot i (allSlots order,
// 0..41) with the selected hotbar slot, then refresh the grid.
void Game::pickCarrySlot(int i) {
  if (i < 0 || i >= kCarrySlots) return;
  player_.inventory.assignToHotbar(i, hot_);  // 2026-10-31 (JD): one-way copy into the hotbar, never a swap
  ui_.refreshCarry(carrySnapshot());
}

std::vector<ItemStack> Game::carrySnapshot() {
  // Item 4: the snapshot now covers ALL slots (carry 50 + gear 8 =
  // kTotalSlots), because the creative screen's inventory half shows the
  // worn gear too. Callers that only read the first kCarrySlots entries
  // (the test-world kit seeding) are unaffected.
  std::vector<ItemStack> out;
  auto slots = player_.inventory.allSlots();
  for (int i = 0; i < kTotalSlots; ++i) out.push_back(*slots[size_t(i)]);
  return out;
}

// B9 inventory drag (JD 2026-11-11 ~10:06-10:17 PM ET, critical): a
// plain palette press used to write straight into the SELECTED hotbar
// slot, so the only way to get an item into any other slot was to close
// the screen, select a hotbar slot outside it, reopen and click. Now a
// plain press puts a full stack in the mouse-hand (the cursor stack), and
// the next press on ANY inventory slot (hotbar, hands, main, gear; empty
// or filled) places / merges / swaps through cursorClick, the exact
// mirror of lift-then-palette-discard. Shift-click keeps the quick give.
void Game::pullPaletteToCursor(const ItemStack& stack) {
  if (stack.empty() || !cursorStack_.empty()) return;  // carrying: the UI routes the press to the discard
  cursorStack_ = stack;
  cursorOriginFlat_ = -1;  // no slot origin: the palette is the stack's home
  cursorFromPalette_ = true;
  ui_.refreshCursor(cursorStack_);
}

void Game::pickInventoryBlock(BlockId b, bool shift) {
  if (!shift) {
    pullPaletteToCursor(ItemStack{uint32_t(b), uint16_t(kMaxStack)});
    return;
  }
  int slot = hot_;
  if (shift)
    for (int i = 0; i < kHotbarSize; ++i)
      if (player_.inventory.hotbar()[size_t(i)].empty()) {
        slot = i;
        break;
      }
  player_.inventory.hotbar()[size_t(slot)] = ItemStack{uint32_t(b), uint16_t(kMaxStack)};
  ui_.refreshCarry(carrySnapshot());
}

// 2026-11-09 next-build-2 (Item 4): creative put-back. The palette is an
// infinite source, so "back" means the stack leaves the inventory (the
// mock-up's flow: click a filled slot to send it back). Inventory-UI
// state only: no sim, world, or save-format change (the inventory
// contents are ordinary player state, saved exactly as before).
// JD ruling 2026-11-10 (08:35 EST): this IS the palette-discard path
// (click/drag a stack onto the creative side discards it, returned to
// the infinite palette, explicitly NOT dropped into the world)... it
// must never call dropStackToWorld.
void Game::putBackSlot(int flatIndex) {
  if (flatIndex < 0 || flatIndex >= kTotalSlots) return;
  player_.inventory.slotAt(flatIndex).clear();
  ui_.refreshCarry(carrySnapshot());
}

// Q-drop (JD ruling 2026-11-10 08:35 EST; floating-items design
// 2026-11-10 section 2.4 contract, rulings 08:48 EST): spawns ONE item
// entity carrying the request's stack. Plain stacks ride the entity's
// data/count; a data-bearing stack (custom name or certificates)
// ALSO lands in the side payload table (design 2.9 option (a),
// approved), so its identity survives the drop and the pickup. Toss:
// the design's CLAY numbers (4.0 blocks/s along the look direction
// plus 1.5 up), deterministic, no RNG anywhere. Pickup delay per
// design 2.5: 256 world ticks (2 s) for player drops so the magnet
// cannot drag a just-thrown item straight back. Despawn stays
// LithoKraft's 480 seconds (JD ruling), enforced by cItemBehavior.
// Returns the entity id, or 0 on refusal (empty stack).
uint64_t Game::dropStackToWorld(const DropRequest& req) {
  if (req.stack.empty()) return 0;
  const uint64_t id = entities_.spawn(kEntityItem, req.origin.x, req.origin.y, req.origin.z);
  litholattice::Entity* e = entities_.get(id);
  if (!e) return 0;
  e->data = req.stack.itemId;
  e->count = req.stack.count;
  e->vx = double(req.direction.x) * 4.0;
  e->vy = double(req.direction.y) * 4.0 + 1.5;
  e->vz = double(req.direction.z) * 4.0;
  if (req.fromPlayer) entityPickupDelay_[id] = 256;  // 2 s at 128 TPS
  if (req.stack.hasData()) entityPayloads_[id] = req.stack;
  return id;
}

// The inventory side of Q (design 2.4: the GUI side owns the slot
// mutation, and the mutation happens only after the world accepted
// the drop, so a refused drop can never lose an item). `whole` is
// Shift+Q (the whole stack); otherwise exactly one item leaves.
bool Game::dropFromSlot(int flatIndex, bool whole) {
  if (flatIndex < 0 || flatIndex >= kTotalSlots) return false;
  ItemStack& slot = player_.inventory.slotAt(flatIndex);
  if (slot.empty()) return false;
  ItemStack leaving = slot;
  if (!whole) leaving.count = 1;
  DropRequest req;
  req.stack = leaving;
  const Vec3 fwd = player_.forward();
  req.origin = {float(player_.pos.x + fwd.x * 0.5), float(player_.pos.y + 1.62 - 0.3 + fwd.y * 0.5), float(player_.pos.z + fwd.z * 0.5)};  // MSVC C2397: explicit narrowing (same value GCC produced)
  req.direction = fwd;
  req.fromPlayer = true;
  if (dropStackToWorld(req) == 0) return false;
  if (whole || slot.count <= 1) slot.clear();
  else slot.count = uint16_t(slot.count - 1);
  ui_.refreshCarry(carrySnapshot());
  return true;
}

// ---------------------------------------------------------------------
// Cursor stack (2026-11-11; JD finding 07:02 EST: clicking a filled
// slot did nothing he could see... "it should be picked up like in
// Minecraft's GUI. And in the mouse hand... the mouse is considered
// the hand." Plan: Cursor-Stack-Plan-2026-11-11.md; gesture semantics
// follow the house Cinnabar inventory study, clean-room, JD's rulings
// supreme). One press on a carry slot, resolved here in exactly one
// place. Every branch moves by copy-then-clear at a single site, so
// the stack exists in exactly one place at every instant: the
// duplication firewall (plan 5.1).
void Game::cursorClick(int flatIndex) {
  if (flatIndex < 0 || flatIndex >= kTotalSlots) return;
  ItemStack& slot = player_.inventory.slotAt(flatIndex);
  if (cursorStack_.empty()) {
    // Lift (3.1): the slot's whole stack moves onto the cursor.
    if (slot.empty()) return;
    cursorStack_ = slot;
    cursorOriginFlat_ = flatIndex;
    cursorFromPalette_ = false;
    slot.clear();
  } else if (slot.empty()) {
    // Place (3.2): the whole carried stack moves into the slot.
    slot = cursorStack_;
    cursorStack_.clear();
    cursorOriginFlat_ = -1;
    cursorFromPalette_ = false;
  } else if (slot.sameIdentity(cursorStack_)) {
    // Merge (3.3): top the slot up to its cap; the remainder stays
    // on the cursor. At cap the click is inert (nothing moves).
    const int cap = int(stackCapFor(slot.itemId));
    const int room = cap - int(slot.count);
    if (room <= 0) return;
    const int move = std::min(room, int(cursorStack_.count));
    slot.count = uint16_t(slot.count + move);
    if (move >= int(cursorStack_.count)) {
      cursorStack_.clear();
      cursorOriginFlat_ = -1;
      cursorFromPalette_ = false;
    } else {
      cursorStack_.count = uint16_t(cursorStack_.count - move);
    }
  } else {
    // Swap (3.3): different identity (sameIdentity covers the data
    // law: a named stack never silently merges into a plain one).
    // The swapped-out stack's home becomes THIS slot.
    ItemStack tmp = slot;
    slot = cursorStack_;
    cursorStack_ = tmp;
    cursorOriginFlat_ = flatIndex;
    cursorFromPalette_ = false;  // the hand now holds a real inventory stack
  }
  ui_.refreshCarry(carrySnapshot());
  ui_.refreshCursor(cursorStack_);
}

// D9 2026-11-12 (JD): Shift-click quick move between the hotbar and the
// main grid (Minecraft-style). The merge step is cursorClick's 3.3 rule
// (sameIdentity, top up to stackCapFor, remainder kept); nothing new.
// Every unit is moved by add-here-then-subtract-there inside one call,
// so the stack is never eaten and never duplicated: a full destination
// moves nothing and the source keeps its stack.
bool Game::shiftTransfer(int flatIndex) {
  int lo = 0, hi = 0;
  if (flatIndex >= 0 && flatIndex < kInvHotbarSlots) {
    lo = kMainFlatBase;
    hi = kMainFlatBase + kMainSlots;
  } else if (flatIndex >= kMainFlatBase && flatIndex < kMainFlatBase + kMainSlots) {
    lo = 0;
    hi = kInvHotbarSlots;
  } else {
    return false;  // hands, gear: not quick-move slots
  }
  ItemStack& src = player_.inventory.slotAt(flatIndex);
  if (src.empty()) return false;
  int left = int(src.count);
  bool moved = false;
  for (int i = lo; i < hi && left > 0; ++i) {  // 1. top up same-identity stacks (merge 3.3)
    ItemStack& d = player_.inventory.slotAt(i);
    if (d.empty() || !d.sameIdentity(src)) continue;
    const int room = int(stackCapFor(d.itemId)) - int(d.count);
    if (room <= 0) continue;
    const int move = std::min(room, left);
    d.count = uint16_t(d.count + move);
    left -= move;
    moved = true;
  }
  for (int i = lo; i < hi && left > 0; ++i) {  // 2. the remainder, whole, into the first empty slot
    ItemStack& d = player_.inventory.slotAt(i);
    if (!d.empty()) continue;
    d = src;
    d.count = uint16_t(left);
    left = 0;
    moved = true;
  }
  if (!moved) return false;
  if (left <= 0) src.clear();
  else src.count = uint16_t(left);
  ui_.refreshCarry(carrySnapshot());
  return true;
}

// Palette discard (3.4; JD ruling 2026-11-10): a palette press while
// carrying returns the carried stack to the infinite creative source.
// It is gone from the inventory, deliberately, and it NEVER calls
// dropStackToWorld: discard is not a world drop. This is the only
// intentional destruction in the feature, and it takes an explicit
// palette press while carrying... never a close, a tab switch, or any
// automatic path.
void Game::paletteClickCursor() {
  if (cursorStack_.empty()) return;
  cursorStack_.clear();
  cursorOriginFlat_ = -1;
  cursorFromPalette_ = false;
  ui_.refreshCursor(cursorStack_);
}

// Q-from-the-hand (3.6, Cinnabar-confirmed parity in the plan): while
// carrying, Q acts on the held stack instead of the hovered slot.
// Same contract as dropFromSlot: compute what leaves, drop it through
// dropStackToWorld, and mutate the cursor ONLY after the world
// accepted, so a refused drop loses nothing.
bool Game::dropFromCursor(bool whole) {
  if (cursorStack_.empty()) return false;
  ItemStack leaving = cursorStack_;
  if (!whole) leaving.count = 1;
  DropRequest req;
  req.stack = leaving;
  const Vec3 fwd = player_.forward();
  req.origin = {float(player_.pos.x + fwd.x * 0.5), float(player_.pos.y + 1.62 - 0.3 + fwd.y * 0.5), float(player_.pos.z + fwd.z * 0.5)};  // MSVC C2397: explicit narrowing (same value GCC produced)
  req.direction = fwd;
  req.fromPlayer = true;
  if (dropStackToWorld(req) == 0) return false;
  if (whole || cursorStack_.count <= 1) {
    cursorStack_.clear();
    cursorOriginFlat_ = -1;
    cursorFromPalette_ = false;
  } else {
    cursorStack_.count = uint16_t(cursorStack_.count - 1);
  }
  ui_.refreshCursor(cursorStack_);
  return true;
}

// The close chain (plan 5): runs inside setInventoryOpen(false)
// before the screen closes. First success wins, and the chain cannot
// end without a home: closing never destroys, never duplicates.
//   1. The origin slot, if it is empty (the normal case; also gear
//      origins, which no fallback below may use as a home).
//   2. Inventory::placeStack: identity merge with room for the WHOLE
//      stack, else the first empty slot, both over the carry range
//      0..kCarrySlots-1 (gear is not a fallback home).
//   3. Never-destroy fallback (deviation note in the plan file: the
//      plan's literal step 4 merged into the origin even when the
//      origin held a different stack, which would have destroyed
//      THAT stack; this step preserves the invariant instead): any
//      empty slot in the full flat range (reachable only as the
//      emptied gear origin after a swap, by the piece invariant:
//      lifts / places / merges / swaps never increase the number of
//      stacks in play, so a carried stack always leaves at least one
//      slot empty somewhere), else an identity merge into any slot
//      past its cap (counts are uint16; 512 + 512 cannot overflow).
void Game::returnCursorStack() {
  if (cursorStack_.empty()) {
    cursorOriginFlat_ = -1;
    cursorFromPalette_ = false;
    return;
  }
  if (cursorFromPalette_) {
    // B9: a palette pull never placed goes back to the infinite palette
    // (it was never the player's; the same no-world-drop rule as the
    // palette discard). Nothing the player owned is lost.
    cursorStack_.clear();
    cursorOriginFlat_ = -1;
    cursorFromPalette_ = false;
    ui_.refreshCursor(cursorStack_);
    return;
  }
  const ItemStack held = cursorStack_;
  bool placed = false;
  if (cursorOriginFlat_ >= 0 && cursorOriginFlat_ < kTotalSlots &&
      player_.inventory.slotAt(cursorOriginFlat_).empty()) {
    player_.inventory.slotAt(cursorOriginFlat_) = held;
    placed = true;
  } else if (player_.inventory.placeStack(held)) {
    placed = true;
  } else {
    for (int i = 0; i < kTotalSlots && !placed; ++i) {
      ItemStack& s = player_.inventory.slotAt(i);
      if (s.empty()) {
        s = held;
        placed = true;
      }
    }
    for (int i = 0; i < kTotalSlots && !placed; ++i) {
      ItemStack& s = player_.inventory.slotAt(i);
      if (s.sameIdentity(held)) {
        s.count = uint16_t(s.count + held.count);
        placed = true;
      }
    }
  }
  if (placed) {
    cursorStack_.clear();
    cursorOriginFlat_ = -1;
    cursorFromPalette_ = false;
    ui_.refreshCarry(carrySnapshot());
    ui_.refreshCursor(cursorStack_);
  }
  // If `placed` is somehow false the cursor KEEPS the stack (the
  // screen closes around it and the next open retries the chain):
  // unreachable by the piece invariant above, and keeping beats
  // destroying under every circumstance.
}

// Item 4, Equipment tab give: one stack of the item at its own cap
// (tools stack to 1), into the selected hotbar slot (shift: first free).
void Game::pickInventoryItem(uint32_t itemId, bool shift) {
  if (!shift) {  // B9: plain press = full stack (the item's own cap) into the hand
    pullPaletteToCursor(ItemStack{itemId, stackCapFor(itemId)});
    return;
  }
  int slot = hot_;
  if (shift)
    for (int i = 0; i < kHotbarSize; ++i)
      if (player_.inventory.hotbar()[size_t(i)].empty()) {
        slot = i;
        break;
      }
  player_.inventory.hotbar()[size_t(slot)] = ItemStack{itemId, stackCapFor(itemId)};
  ui_.refreshCarry(carrySnapshot());
}

void Game::routeUiInput(bool& pointerUsed, bool& escapeUsed) {
  pointerUsed = escapeUsed = false;
  uiEvents_.clear();
  translateUiEvents(input_.frameEvents(), input_.devices(), uiEvents_);
  const int popupsAtStart = ui_.root().gui().context().popupCount();
  // With the mouse captured for play the cursor is hidden: only keys may reach the UI.
  // Cortana 2026-10-15 1213 (alt-tab click bug): paused_ added. A focus
  // pause left captured_ true, so every pointer event was skipped here: the
  // pause-menu buttons were dead and nothing saw the resume click.
  const bool pointerToUi = !captured_ || inventoryOpen_ || editorOpen() || paused_;
  freePointerDown_ = false;
  // Q-drop on the inventory screen (JD ruling 2026-11-10): the raw Q
  // key never reaches the widget tree (the UI key bridge carries no Q
  // and the facade Key enum has none), so the screen's drop gesture is
  // intercepted HERE, at the game's own routing layer, where the raw
  // input events still exist: Q down while the inventory is open and
  // no text entry owns the keyboard drops from the HOVERED carry slot
  // (one item; Shift+Q the whole stack)... or, since the cursor stack
  // (2026-11-11), from the stack in the mouse-hand while carrying.
  // Return means the event is consumed by the gesture and never falls
  // through to the tree.
  if (inventoryOpen_) {
    const bool shiftHeld = input_.devices().keyDown(Key::LeftShift) || input_.devices().keyDown(Key::RightShift);
    // A text field holding focus eats the Q as a character: stands down.
    const bool typing = dynamic_cast<const ui::cTextEditBase*>(ui_.root().focus()) != nullptr;
    for (const InputEvent& e : input_.frameEvents()) {
      if (typing) break;
      if (e.type != InputEvent::Type::KeyDown || Key(e.code) != Key::Q) continue;
      // Cursor stack (2026-11-11, plan 3.6): while a stack is in the
      // mouse-hand, Q acts on the HAND, not the hovered slot (the
      // Cinnabar drop-key rule). Not carrying, the hovered slot below
      // is the target, exactly as before.
      if (carryingStack()) {
        dropFromCursor(shiftHeld);
        break;
      }
      if (const ui::cWidget* hov = ui_.root().hover()) {
        if (const auto* slot = dynamic_cast<const ui::cSlot*>(hov)) {
          const int flat = slot->data() - (1 << 20);  // kCarryData (game_ui.cpp)
          if (flat >= 0 && flat < kTotalSlots) {
            dropFromSlot(flat, shiftHeld);
            break;
          }
          // The HUD hotbar inside the open screen: its slots carry the
          // plain hotbar index as data (0..kHotbarSlots-1)... those are
          // flat indices 0..8 in allSlots order, the hotbar's own home.
          if (slot->data() >= 0 && slot->data() < kHotbarSlots) {
            dropFromSlot(slot->data(), shiftHeld);
            break;
          }
        }
      }
    }
  }
  for (const ui::UiEvent& e : uiEvents_) {
    // Tilde toggles the debug console (never while a text field/modal owns the
    // keyboard); while open, the console pre-filters history/scroll/escape keys.
    // Litho GUI migration: the new cDebugConsole consumes lithogui events
    // and the ILithoGui, where the old one took the cUiRoot + UiEvent.
    if (debugConsole_ && debugConsole_->handleTildeKey(ui_.root().gui(), ui::toGuiKeyEvent(e))) continue;
    if (debugConsole_ && debugConsole_->isOpen() && debugConsole_->handleKeyPreFilter(ui::toGuiKeyEvent(e))) continue;
    const bool pointer = e.type != ui::UiEvent::Type::KeyDown && e.type != ui::UiEvent::Type::Text;
    if (pointer && !pointerToUi) continue;
    const bool consumed = ui_.root().handleEvent(e);
    if (!consumed && e.type == ui::UiEvent::Type::PointerDown) freePointerDown_ = true;  // click in the world
    if (consumed && e.type == ui::UiEvent::Type::PointerDown) pointerUsed = true;
    if (consumed && e.type == ui::UiEvent::Type::KeyDown && e.key == ui::Key::Escape) escapeUsed = true;
  }
  // 2026-11-07 (JD bug report): Escape that closed a popup inside the GUI
  // context (its dispatchKey closes the top popup without the key ever
  // reaching a widget handler the facade can see) still counts as USED.
  // handlePause() polls the raw Pause action, which stays "pressed" for
  // the whole frame, so without this the same Escape that closed the
  // world picker fell through to the launch menu's Esc-quits rule and
  // exited the game. A shrunk popup stack during this frame's routing is
  // the consumption proof.
  if (!escapeUsed && ui_.root().gui().context().popupCount() < popupsAtStart)
    for (const ui::UiEvent& e : uiEvents_)
      if (e.type == ui::UiEvent::Type::KeyDown && e.key == ui::Key::Escape) { escapeUsed = true; break; }
}

void Game::updateUi(float dt, int width, int height) {
  if (debugConsole_) debugConsole_->update();  // stream engine-log tap records into the console view
  if (resources_.themeRevision() != themeRevision_) {
    themeRevision_ = resources_.themeRevision();
    ui_.applyTheme(resources_.theme());
  }
  // Text entry (script editor, focused text field) gets every key: push the opaque UiText context.
  const bool wantText = editorOpen() || dynamic_cast<const ui::cTextEditBase*>(ui_.root().focus()) != nullptr;
  if (wantText != uiText_) {
    uiText_ = wantText;
    if (wantText) input_.push(InputContext::UiText);
    else input_.pop(InputContext::UiText);
  }
  if (editorOpen()) captured_ = false;
  else if (editorWasOpen_) captured_ = true;  // closing the editor returns to play
  editorWasOpen_ = editorOpen();
  ui_.update(hudModel(), {float(width), float(height)}, dt);
}

// Break / Place / Use / PickBlock become sim edits (applied at the sim's next tick boundary).
// Place and Use share right click: Use wins on a usable block unless sneaking. Right click on a
// plain wire with an empty hand is Use too (cycles that colour's front on an adjacent Redirector).
// 2026-10-16: the sim receipts every game Place edit in order; a rejected
// one gives back the item consumeOne took (same slot when it still holds
// that item, else the first carry slot that fits).
void Game::refundRejectedPlaces() {
  if (!sim_) { placeRefunds_.clear(); return; }
  for (uint8_t ok : sim_->drainPlaceReceipts()) {
    if (placeRefunds_.empty()) break;  // receipts from a replay or another producer
    ItemStack one = std::move(placeRefunds_.front());
    placeRefunds_.pop_front();
    if (ok || one.empty()) continue;
    ItemStack& s = player_.inventory.hotbar()[size_t(hot_)];
    if (!s.empty() && s.sameIdentity(one) && int(s.count) < int(stackCapFor(s.itemId))) ++s.count;
    else if (s.empty()) s = one;
    else if (!player_.inventory.placeStack(one)) logWarn("place refund: inventory full, item %u lost", unsigned(one.itemId));
  }
}

void Game::handleWorldActions() {
  refundRejectedPlaces();
  const SimStats& st = sim_->stats();
  // Cortana 2026-10-15 2215 [GAME] (docs/Controls-and-Trace-Spec, JD 22:07 decisions):
  //  D5 reach: survival 5, creative 16 (the sim's kReach is the 16 maximum; gated here).
  //  D3 hold-to-repeat: Break every 250 ms, Place every 200 ms onto a NEW target only.
  //  D1 Sneak + right click always places (sneaking never Uses).
  //  D2 Alt + right click = the secondary setting on ANY component (was Catchor-only via sneak,
  //     which left Thresholdor / radio / jammer settings unreachable).
  //  D9 walk-bridging: looking down with nothing in reach places at the edge (setting game.bridging).
  //  D4 Repeator: a top-face click selects that button's delay; a side click cycles.
  using SClock = std::chrono::steady_clock;
  const SClock::time_point nowT = SClock::now();
  const float reach = creativeMode() ? 16.f : 8.f;
  const bool inReach = st.hit && st.hitDist <= reach + 0.5f;
  if (input_.pressed(Action::Crawl)) crawlToggle_ = !crawlToggle_;  // D8: C toggles crawl
  const bool breakNow = input_.pressed(Action::Break) ||
                        (input_.held(Action::Break) && nowT - lastBreakT_ >= std::chrono::milliseconds(250));
  if (breakNow && inReach) {
    sim_->edit({EditCmd::Kind::Break});
    lastBreakT_ = nowT;
    noteBlockEdit();
  }
  const auto& reg = blockRegistry();
  // The selected inventory stack decides what the hand holds (P2b-1): a
  // block-backed stack places its block; a tool / the Fragment places
  // nothing (per-tool world actions route through the item-use branch
  // below - the B1a seam, content/item_use.h).
  const BlockId selBlock = selectedPlaceBlock();
  const bool sneaking = input_.held(Action::Sneak);
  const bool wireUse = inReach && isFlatWire(st.hitBlock) && selBlock == kAir;
  const bool usable = inReach && (playerUsable(reg, st.hitBlock) || wireUse) && !sneaking;  // 2026-11-04 shared gate (blocks.h)
  // 2026-10-24 (JD): sneak + RMB is the secondary setting too on a Catchor (match number).
  // 2026-10-25 (JD): ... and on EVERY block with a secondary setting (Thresholdor read mode, ...): never a place.
  const bool altUse = inReach && (input_.held(Action::Secondary) || (sneaking && hasSneakUse(st.hitBlock, st.hitState))) &&
                      roleOf(st.hitBlock) != SimRole::None;
  const int curTarget[3] = {st.hx, st.hy, st.hz};
  const bool newTarget = curTarget[0] != lastPlaceHit_[0] || curTarget[1] != lastPlaceHit_[1] ||
                         curTarget[2] != lastPlaceHit_[2];
  // 2026-10-31 (JD): held place fills each NEW target cell at once (strafing a Trace line skipped
  // cells: the 200 ms repeat was longer than the time to cross a cell). Same cell still never repeats.
  // Kill: LL_PLACE_NEWCELL=0 restores the 200 ms gate.
  static const bool kPlaceNewCell = [] { const char* e = std::getenv("LL_PLACE_NEWCELL"); return !(e && e[0] == '0'); }();
  // 2026-10-31 (JD): the new-cell path only while moving (horizontal movement input); standing still
  // it chained onto the block just placed. Still = one place per click + the 200 ms hold repeat.
  const bool placeMoving = input_.value(Action::MoveForward) != 0.f || input_.value(Action::MoveBack) != 0.f ||
                           input_.value(Action::MoveLeft) != 0.f || input_.value(Action::MoveRight) != 0.f;
  // 2026-10-31 (JD): a moving held-place run locks onto ONE line: the first placement captures the
  // dominant movement axis plus the perpendicular coordinate and Y of the target; later new-cell
  // placements only step along that axis. Released when Place is released or motion stops.
  if (!input_.held(Action::Place) || !placeMoving) placeLockAxis_ = -1;
  const bool placeNow = input_.pressed(Action::Place) ||
                        (input_.held(Action::Place) && ((kPlaceNewCell && newTarget && placeMoving) ||
                                                        nowT - lastPlaceT_ >= std::chrono::milliseconds(200)));
  auto consumeOne = [&] {
    if (testWorld()) return;  // playtest 2026-10-16: infinite placement in the test world
    ItemStack& s = player_.inventory.hotbar()[size_t(hot_)];
    if (s.count > 1) --s.count;
    else s.clear();
  };
  // C9 2026-11-12 (JD): creative items are infinite -- a PLACE never takes from the stack in creative
  // (survival keeps consumePlacedOne's one-per-place, unchanged). No consumption = no refund either.
  const bool placeConsumes = !testWorld() && !creativeMode();
  auto consumePlace = [&] {
    if (!placeConsumes) return;
    consumePlacedOne(player_.inventory.hotbar()[size_t(hot_)], false);
  };
  // 2026-10-25 (JD, Post viewer): the overlay follows the look target; it closes when the player
  // looks away from that post (either half) or out of reach.
  auto samePost = [&](int x, int y, int z) {
    return inReach && post::isPost(st.hitBlock) && st.hx == x && st.hz == z && std::abs(st.hy - y) <= 1;
  };
  if (postViewOpen_ && !samePost(postViewX_, postViewY_, postViewZ_)) postViewOpen_ = false;
  if (input_.pressed(Action::Use) && inReach && post::isPost(st.hitBlock) && input_.held(Action::Secondary)) {
    // Alt + right click on a post = write (the secondary action, as on every component)
    postViewOpen_ = true; postViewX_ = st.hx; postViewY_ = st.hy; postViewZ_ = st.hz;
    beginPostEdit(st.hx, st.hy, st.hz);
  } else if (altWheelOn() && altUse && input_.held(Action::Secondary) &&
             (input_.pressed(Action::HotbarNext) || input_.pressed(Action::HotbarPrev))) {
    // 2026-10-31 (JD): Alt + wheel steps the component's 1..64 setting (up = +1, down = -1 via
    // 63 wrapping steps); handleHotbar() leaves the hotbar alone while Alt is held.
    const int up = int(input_.pressed(Action::HotbarPrev)) - int(input_.pressed(Action::HotbarNext));  // wheel up = HotbarPrev
    if (up != 0) {
      EditCmd c{EditCmd::Kind::Command};
      c.command = SimCommand{kCmdAltUse, st.hx, st.hy, st.hz, up > 0 ? 1 : 63, 0};
      sim_->edit(c);
    }
  } else if (input_.pressed(Action::Use) && altUse) {  // Alt + right click: the component's secondary setting
    EditCmd c{EditCmd::Kind::Command};
    c.command = SimCommand{kCmdAltUse, st.hx, st.hy, st.hz, 0, 0};
    sim_->edit(c);
  } else if (input_.pressed(Action::Use) && usable) {
    const Vec3 eye = playerEyePos(player_, locoSnap().eye);
    const Vec3 fw = player_.forward();
    if (reg.def(st.hitBlock).role == SimRole::Keyboard) {
      beginTyping(st.hx, st.hy, st.hz);
    } else if (post::isPost(st.hitBlock)) {  // 2026-10-25 (JD): right click views the full text (toggle)
      const bool again = postViewOpen_ && samePost(postViewX_, postViewY_, postViewZ_);
      postViewOpen_ = !again; postViewX_ = st.hx; postViewY_ = st.hy; postViewZ_ = st.hz;
    } else if (false && roleOf(st.hitBlock) == SimRole::Repeator && eye.y > float(st.hy + 1) && fw.y < -1e-4f) {  // 2026-10-25 (JD): off; every right click cycles one step (Use)
      // Top-face click: intersect the look ray with the cell's top plane -> the button quadrant.
      const float t = (float(st.hy + 1) - eye.y) / fw.y;
      const double lx = double(eye.x + fw.x * t) - st.hx, lz = double(eye.z + fw.z * t) - st.hz;
      EditCmd c{EditCmd::Kind::Command};
      c.command = SimCommand{kCmdRepeatorSet, st.hx, st.hy, st.hz,
                             foliage::repeatorButtonAt(std::clamp(lx, 0.0, 0.999), std::clamp(lz, 0.0, 0.999),
                                                       stFacing(st.hitState)),
                             0};
      sim_->edit(c);
      noteBlockEdit();
    } else {
      EditCmd c{EditCmd::Kind::Use}; c.x = st.hx; c.y = st.hy; c.z = st.hz; sim_->edit(c); noteBlockEdit();
    }
  } else if (input_.pressed(Action::Use) && inReach && selBlock == kAir &&
             hasItemUseHandler(player_.inventory.hotbar()[hot_].itemId)) {
    // B1a/A3: the general per-item use seam - Use with an item that has a
    // registered handler and places no block (Seedor, silica seed, ...).
    // Dispatched on the sim thread via kCmdItemUse; the registry fails
    // closed (logged) for handlers that decline.
    EditCmd c{EditCmd::Kind::Command};
    c.command = SimCommand{kCmdItemUse, st.hx, st.hy, st.hz, int(player_.inventory.hotbar()[hot_].itemId), 0};
    sim_->edit(c);
    // Cortana 2026-10-15 1515 (GAME, Herominde H-02): a silica seed is used up when planted.
    if (player_.inventory.hotbar()[hot_].itemId == kItemSilicaSeed) consumeOne();
  } else if (placeNow && selBlock != kAir && !(input_.held(Action::Use) && usable && !input_.pressed(Action::Place))) {
    // Aim directional blocks along the player's horizontal look direction (front = away from you).
    const Vec3 fw = player_.forward();
    const int facing = foliage::facingFromLook(fw.x, fw.z);
    const bool repeat = !input_.pressed(Action::Place);
    bool onLine = true;
    if (kPlaceNewCell && placeMoving && inReach) {
      if (placeLockAxis_ < 0) {
        const double mf = double(input_.value(Action::MoveForward)) - double(input_.value(Action::MoveBack));
        const double ms = double(input_.value(Action::MoveRight)) - double(input_.value(Action::MoveLeft));
        const double dx = fw.x * mf - fw.z * ms, dz = fw.z * mf + fw.x * ms;
        placeLockAxis_ = std::fabs(dx) >= std::fabs(dz) ? 0 : 2;
        placeLockPerp_ = placeLockAxis_ == 0 ? curTarget[2] : curTarget[0];
        placeLockY_ = curTarget[1];
      } else if (repeat) {
        onLine = curTarget[1] == placeLockY_ && (placeLockAxis_ == 0 ? curTarget[2] : curTarget[0]) == placeLockPerp_;
      }
    }
    if (inReach && (!repeat || newTarget) && onLine) {
      // 2026-10-25 Posts: kFacing8 blocks take the yaw snapped to 45 degrees (octant 0..7).
      const int placeFacing = reg.has(selBlock, kFacing8) ? facing8FromLook(fw.x, fw.z) : facing;
      EditCmd pc{EditCmd::Kind::Place, selBlock, uint8_t(placeFacing)};
      pc.x = kPlaceReceipt;  // 2026-10-16: the sim reports the outcome; a rejection is refunded
      sim_->edit(pc);
      // Placement CONSUMES one from the selected stack in survival (P2b-1), gated on reach (a click into the
      // void must not eat a block). A reachable click the sim rejects is refunded next frame.
      // C9 (JD): creative never consumes, so nothing is queued for refund.
      {
        ItemStack one = player_.inventory.hotbar()[size_t(hot_)];
        one.count = 1;
        placeRefunds_.push_back(placeConsumes ? std::move(one) : ItemStack{});  // empty = no refund (receipt order kept)
        if (placeRefunds_.size() > 4096) placeRefunds_.pop_front();
      }
      consumePlace();
      lastPlaceT_ = nowT;
      lastPlaceHit_[0] = curTarget[0]; lastPlaceHit_[1] = curTarget[1]; lastPlaceHit_[2] = curTarget[2];
      noteBlockEdit();
    } else if (!inReach && settings_.bridging && !locoSnap().flying && fw.y < -0.45f) {
      // D9 walk-bridging: the cell beside the block under the feet, toward the horizontal look.
      const int fx = int(std::floor(player_.pos.x)), fy = int(std::floor(player_.pos.y)) - 1,
                fz = int(std::floor(player_.pos.z));
      const int bx = fx + (facing == 1 ? 1 : facing == 3 ? -1 : 0), bz = fz + (facing == 2 ? 1 : facing == 0 ? -1 : 0);
      if (bx != lastBridge_[0] || fy != lastBridge_[1] || bz != lastBridge_[2] || !repeat) {
        EditCmd c{EditCmd::Kind::Command};
        c.command = SimCommand{kCmdBridgePlace, bx, fy, bz, int(selBlock), facing};
        c.x = kPlaceReceipt;  // Review 2026-10-16: the sim receipts the bridge; a rejection is refunded
        sim_->edit(c);
        {
          ItemStack one = player_.inventory.hotbar()[size_t(hot_)];
          one.count = 1;
          placeRefunds_.push_back(placeConsumes ? std::move(one) : ItemStack{});  // C9: creative = no refund
          if (placeRefunds_.size() > 4096) placeRefunds_.pop_front();
        }
        consumePlace();
        lastPlaceT_ = nowT;
        lastBridge_[0] = bx; lastBridge_[1] = fy; lastBridge_[2] = bz;
        noteBlockEdit();
      }
    }
  }  // Cortana 2026-10-15 1515 (GAME, Herominde H-01): Pick Block needs a target within reach,
  // and only creative may create the block from nothing (survival only selects an existing stack).
  if (input_.pressed(Action::PickBlock))  // 2026-10-24 (JD playtest: MMB looked dead): one diagnostic line per press
    logInfo("pick block: press hit=%d dist=%.1f block=%u hot=%d", int(st.hit), double(st.hitDist), unsigned(st.hitBlock), hot_);
  if (input_.pressed(Action::PickBlock) && st.hit && st.hitDist <= (creativeMode() ? 16.f : 8.f) + 0.5f && st.hitBlock != kAir) {
    // PickBlock routes into the inventory (P2b-1), like every other block
    // collection path: if a hotbar stack already places this block, select
    // it; otherwise collect ONE of the block (the same addItem path broken
    // blocks take through item-entity pickup) and select the hotbar slot
    // it landed in, when it landed in the hotbar.
    // BUG-0002 (Discovered by J.D., Fixed by Cortana): the old path only
    // changed what you hold when the block was ALREADY on the hotbar, or
    // when the one collected block happened to land in the hotbar. With a
    // full hotbar (the creative / test-world kit) it went into the carry
    // inventory and the hand never changed, so middle click looked dead.
    // Now: hotbar hit -> select it; carry hit -> swap that stack into the
    // selected slot; creative miss -> the block goes INTO the selected slot
    // and the stack that was there moves to the carry inventory.
    auto& hb = player_.inventory.hotbar();
    int found = -1;
    for (int i = 0; i < kHotbarSize; ++i)
      if (placedBlockForItem(hb[size_t(i)].itemId) == viaBase(st.hitBlock)) { found = i; break; }
    ItemStack* carry = nullptr;
    if (found < 0)
      for (auto& s : player_.inventory.main())
        if (!s.empty() && placedBlockForItem(s.itemId) == viaBase(st.hitBlock)) { carry = &s; break; }
    logInfo("pick block: hotbar slot %d, carry %s", found, carry ? "yes" : "no");
    if (found >= 0) {
      hot_ = found;
    } else if (carry) {
      std::swap(hb[size_t(hot_)], *carry);
      ui_.refreshCarry(carrySnapshot());
    } else if (creativeMode()) {
      ItemStack old = std::move(hb[size_t(hot_)]);
      hb[size_t(hot_)] = ItemStack{uint32_t(viaBase(st.hitBlock)), uint16_t(1)};  // a side Via picks as a Via
      if (!old.empty() && !player_.inventory.placeStack(old))
        logWarn("pick block: inventory full, displaced stack %u lost", unsigned(old.itemId));
      ui_.refreshCarry(carrySnapshot());
    }
  }
}

// Screenshots (lithoscript `screenshot`): the command runs during update(),
// before the frame is drawn, so requestScreenshot() only arms the
// renderer's grab; capturePendingScreenshot() (called by the app after
// renderFrame(), before present) completes the capture. One pending shot
// at a time â€” a second `screenshot` before the frame lands is refused.
std::string Game::requestScreenshot(const ScreenshotRequest& req) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  if (pendingShot_) return "screenshot: a capture is already pending for this frame";
  if (req.toFragment) {
    renderer_.requestGrab();
    pendingShot_ = PendingShot{req, std::string()};
    return "screenshot: capturing into the Logbook gallery...";
  }
  // Path confinement (P1b, 2026-10-13, Appendix F M6): file screenshots
  // land ONLY in the screenshots directory â€” <config dir>/screenshots
  // when a config dir is set (settings file location), else the relative
  // "screenshots" (the pre-P1b auto-name behaviour). Absolute paths and
  // ".." escapes are refused and fall back to the confined auto-name;
  // the resolved path is always in the returned message (and in the
  // "saved" line at capture time) so the location is discoverable.
  const std::filesystem::path shotRoot =
      consoleRegistry_.configDir().empty()
          ? std::filesystem::path("screenshots")
          : consoleRegistry_.configDir() / "screenshots";
  const ScreenshotPathResolution res =
      resolveScreenshotPath(shotRoot, req.path, req.lowres);
  const std::string path = res.path.string();
  renderer_.requestGrab();
  pendingShot_ = PendingShot{req, path};
  if (res.fellBack)
    return "screenshot: path '" + req.path +
           "' escapes the screenshots directory â€” refused; capturing to '" +
           path + "' instead...";
  return "screenshot: capturing to '" + path + "'...";
}

void Game::debugShowUi(const std::string& view) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  if (view == "menu" || view == "settings" || view == "worlds") enterMenu();
  if (view == "settings") ui_.openSettings(settings_);
  if (view == "worlds") openWorldPicker();  // 2026-10-30: shot of the world picker
  if (view == "pause") ui_.openPause();
  // hud: the bench boots in MenuState::cMenu with no menu UI open, which hides the HUD; show the
  // in-play HUD (hotbar, crosshair) plus the F3 overlay for the shot. Bench-only, never saved.
  if (view == "hud" || view == "inventory") { menuState_ = MenuState::Playing; showHud_ = true; showDebug_ = !cfg_.capture; }
  if (view == "inventory") setInventoryOpen(true);  // playtest 2026-10-16: shot of the E dialog
}

void Game::requestUiShot(const std::string& path) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  if (pendingShot_) return;
  renderer_.requestGrab();
  pendingShot_ = PendingShot{ScreenshotRequest{}, path};
}

void Game::capturePendingScreenshot() {
  // --frame-dump (showcase video, JD 2026-10-30): with a sink installed, every frame is
  // grabbed (re-armed each frame) and handed to it as top-first RGB24.
  cFrameDumpHub& hub = cFrameDumpHub::instance();
  // SCRATCH vulkfb (2026-11-11): opt-in display-framebuffer vision for MCP pilots. Off unless
  // LL_MCP_DISPLAY_VISION=1; never shares a grab with --frame-hash/--frame-dump or a screenshot.
  {
    cMcpDisplayVision& dv = cMcpDisplayVision::instance();
    if (cMcpDisplayVision::enabled() && !hub.sink() && !pendingShot_) {
      if (dv.armed()) {
        std::vector<uint8_t> rgb;
        int w = 0, h = 0;
        if (renderer_.grabFramebuffer(rgb, w, h)) dv.publish(rgb, w, h, sim_ ? sim_->stats().worldTick : 0, renderer_.name());
        else dv.disarm();  // re-armed below while the request is still open
      }
      if (dv.wanted()) { renderer_.requestGrab(); dv.arm(); }
    }
  }
  if (IFrameSink* sink = hub.sink()) {
    if (hub.armed() && !pendingShot_) {
      static std::vector<uint8_t> rgb;
      int w = 0, h = 0;
      if (renderer_.grabFramebuffer(rgb, w, h)) {
        sink->onGrabTick(sim_ ? sim_->stats().worldTick : 0);
        sink->onFrame(rgb, w, h);
      }
    }
    if (!pendingShot_) { renderer_.requestGrab(); hub.setArmed(true); }
  }
  if (!pendingShot_) return;
  const PendingShot ps = std::move(*pendingShot_);
  pendingShot_.reset();
  // Agent-vision fidelity for the lowres variant (mcp.vision_width/height
  // cvars, refreshed once per frame by update()).
  const cMcpSensesAdapter::CvarState cs = mcpSenses_.cvars();
  const int vw = std::clamp(cs.visionWidth, 16, 1024);
  const int vh = std::clamp(cs.visionHeight, 16, 1024);
  CapturedShot shot;
  std::string err;
  if (!captureScreenshot(renderer_, ps.req.lowres, vw, vh, shot, &err)) {
    consoleRegistry_.print("screenshot: " + err);
    logError("screenshot: %s", err.c_str());
    return;
  }
  const std::string dims =
      std::to_string(shot.width) + "x" + std::to_string(shot.height);
  if (ps.req.toFragment) {
    FragmentImage img;
    img.caption = ps.req.caption;
    img.width = shot.width;
    img.height = shot.height;
    img.tick = sim_ ? sim_->stats().worldTick : 0;
    img.x = player_.pos.x;
    img.y = player_.pos.y;
    img.z = player_.pos.z;
    img.dimension = active_ ? active_->id : std::string();
    img.png = std::move(shot.png);
    const std::string id = fragmentGallery_.add(std::move(img));
    consoleRegistry_.print("screenshot: captured to Logbook gallery as '" + id +
                           "' (" + dims + ")");
    return;
  }
  if (!writeFileBytes(ps.path, shot.png, &err)) {
    consoleRegistry_.print("screenshot: " + err);
    return;
  }
  consoleRegistry_.print("screenshot: saved '" + ps.path + "' (" + dims + ")");
  logInfo("screenshot: saved '%s' (%s)", ps.path.c_str(), dims.c_str());
}

// 0.0.1.53 [C53-1] (Proposed by JD; co-implemented by JD and Cortana): per-section main-thread timing of update(),
// logged once a second during stream benches ("update-split"), to find render-side cost.
namespace {
struct UpdateSplit {
  double ms[10]{}, mx[10]{};  // 2026-10-25 stutter pass: + worst frame per section
  double lastMs[10]{};        // 2026-11-09 next-build-2: the most recent frame's own values (spike record)
  int frames = 0;
  Clock::time_point last = Clock::now(), mark;
  void lap(int i) { const auto t = Clock::now(); const double d = msBetween(mark, t); ms[i] += d; mx[i] = std::max(mx[i], d); lastMs[i] = d; mark = t; }
} g_split;
// The split line logs during stream benches, or always with LK_FRAME_SPLIT=1.
bool frameSplitEnv() { static const bool on = [] { const char* e = std::getenv("LK_FRAME_SPLIT"); return e && *e == '1'; }(); return on; }
const char* const kSplitNames[10] = {"head", "loco", "deceit", "cam", "pumpStream", "actions", "uploadMeshes", "benchStream", "sounds+ents", "ui"};
}  // namespace
void Game::update(float dt, int width, int height) {
  pollWorldFolderDialog();  // 2026-10-31 (JD): non-blocking Open world folder result
  if (litholattice::f3trace::on()) f3t::endFrame();  // LL_TRACE_F3
  g_split.mark = Clock::now();
  // lithoscript `wait` pump: script time follows the sim's world-tick
  // counter â€” deterministic, never wall-clock. A reloaded world resets the
  // counter, so resync instead of fast-forwarding. While the sim is paused
  // the counter does not advance, so waits freeze too (waits measure game
  // time, not real time).
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastScriptTick_) lastScriptTick_ = wt;
    // Per-frame catch-up cap (P2b-2, review F-L7): after a main-thread
    // stall the sim may be thousands of ticks ahead; pumping all of them
    // in one frame (each tick() also pumps the defrag service) is a
    // hitch of its own. Cap at 64/frame and carry the remainder â€” script
    // time still measures sim time, it just catches up over a few frames.
    uint64_t budget = 64;
    for (; lastScriptTick_ < wt && budget > 0; ++lastScriptTick_, --budget) consoleRegistry_.tick();
  }
  // Handshake freeze mirror (P2b-2, CH-H7): when the watchdog freezes the
  // sim, the GAME state follows â€” UI, `playing` and the world-query
  // gamePaused hook must agree with the sim, not keep reporting Playing.
  if (sim_ && pauseState_ == PauseState::Playing && sim_->stats().hsFrozen) {
    setPauseState(PauseState::HsFrozen);
    if (!hsFreezeLogged_) {
      hsFreezeLogged_ = true;
      logError("game: timing handshake watchdog froze the sim; game state -> HsFrozen (tick_unfreeze or resume recovers)");
    }
  }
  bool uiClick = false, uiEscape = false;
  const bool modalWasOpen = ui_.root().modalOpen();
  routeUiInput(uiClick, uiEscape);
  // Keybind dispatch (HL1-style): the console owns the binding table; the
  // input pump dispatches here, after UI routing. Suppressed while the debug
  // console (or any text field) holds keyboard focus â€” text entry wins.
  if (menuState_ == MenuState::Playing)
    consoleRegistry_.dispatchKeyBinds(input_.devices(), ui_.root().wantsKeyboard());
  // Esc on the launch menu quits; a modal open over the menu (the settings dialog) takes it.
  if (!uiEscape || (menuState_ == MenuState::cMenu && !modalWasOpen)) handlePause();
  if (typing_) handleTyping();
  if (menuState_ == MenuState::Playing) {
    handleToggles();
    handleHotbar();
  } else {
    menuCameraT_ += dt;
    applyMenuCamera();
  }
  // Alt-tab auto-pause: a left click in the 3D world that no UI widget took
  // resumes and recaptures (the Resume button and Esc still work too).
  if (pausedByFocus_ && pauseState_ != PauseState::MenuPaused) pausedByFocus_ = false;
  if (pausedByFocus_ && !uiClick && menuState_ == MenuState::Playing &&
      (freePointerDown_ || input_.devices().mousePressed(MouseButton::Left))) {
    setPaused(false);
    pausedByFocus_ = false;
    captured_ = true;
    uiClick = true;  // this click resumed: it must not also break a block
  }
  // PlayL JD WASD: launch-menu MenuPaused (enterMenu direct assign) is not pausedByFocus_, so
  // click-in-world never recovered; treat MenuPaused+Playing with no inventory as sticky pause.
  if (!uiClick && menuState_ == MenuState::Playing && pauseState_ == PauseState::MenuPaused &&
      !inventoryOpen_ && !editorOpen() &&
      (freePointerDown_ || input_.devices().mousePressed(MouseButton::Left))) {
    enterPlay();
    uiClick = true;
  }
  bool clickUsed = uiClick || inventoryOpen_ || editorOpen() || paused_;
  if (!clickUsed && !captured_ && input_.pressed(Action::Break)) { captured_ = true; clickUsed = true; }  // click to grab
  const bool playing = captured_ && !inventoryOpen_ && !paused_;
  // Look turning stays per render frame (mouse feel is a frame-rate
  // concern). In third/follow/free the active camera owns the Look axis,
  // so the player must not also turn with it.
  if (camera_.mode() != GameCameraMode::Free)
    player_.updateLook(input_, dt, playing && !camera_.consumesLook());
  // --- Locomotion (player_locomotion.h): movement joins the sim's FIXED
  // tick stream. The body steps once per elapsed world tick with dt = the
  // tick duration, consuming this frame's buffered input intent â€” the same
  // worldTick counter the vitals tick from below, so movement is
  // deterministic (fixed-dt steps of recorded intent) and freezes while
  // the sim is paused. Solidity is the game's entity world query â€” the
  // same getBlock/kSolid test the item entities sweep against, unloaded
  // cells reading as air (not solid).
  // Live only while Playing, out of the free camera (a camera, not the
  // player â€” it stays exactly as free as before), and out of bench (the
  // bench paths drive the player position directly). Whenever the body is
  // not live it SHADOWS player_.pos instead, so spawn / respawn /
  // teleports / dimension switches / menu restores / bench moves /
  // setPlayerPosition all seat the body rather than being overwritten by
  // it; the same divergence check catches external writes while live.
  const bool locoLive = menuState_ == MenuState::Playing &&
                        camera_.mode() != GameCameraMode::Free && !benchSet_;
  g_split.lap(0);  // [C53-1]
  advanceWorldClock();  // Cortana 2026-10-16: world clock follows the sim's worldTick
  if (locoLive) {
    pendingLocomotionInput_ =
        (playing && !typing_) ? sampleLocomotionInput() : litholattice::LocomotionInput{};
    if (litholattice::f3trace::on() && f3t::walk() > 0 && !typing_) {  // LL_TRACE_F3_WALK: real-path walk (needs playing)
      // PlayL JD WASD: do NOT bypass playing (captured && !paused). The old wish
      // overwrite moved the body while paused/uncaptured and false-passed headless F3.
      if (playing) {
        const double cy = std::cos(double(player_.yaw)), sy = std::sin(double(player_.yaw));
        pendingLocomotionInput_.wishX = cy; pendingLocomotionInput_.wishZ = sy;
        pendingLocomotionInput_.jump = true;  // Space held
        pendingLocomotionInput_.sprint = f3t::walk() >= 2;
        if (f3t::walk() >= 3) {  // 3: fly forward, climbing whenever the last step was blocked
          static bool flying = false;
          if (!flying) { flying = true; toggleFly("on"); }
          static Vec3 lastPos{};
          pendingLocomotionInput_.jump = std::fabs(player_.pos.x - lastPos.x) + std::fabs(player_.pos.z - lastPos.z) < 1e-3f;
          lastPos = player_.pos;
        }
      } else {
        static int skipLog = 0;
        if ((skipLog++ % 120) == 0)
          logInfo("f3walk: skipped (playing=0 captured=%d paused=%d pauseState=%d)", int(captured_), int(paused_),
                  int(pauseState_));
      }
    }
    // Cortana 2026-10-26 (Archie camera-jump decision): the body steps on the SIM thread (player
    // hook, once per completed world tick); this thread never waits on the sim lock for movement.
    // It publishes the input, then shows the pose lerped between the last two ticks (alpha by
    // wall time), extrapolating by velocity at most one tick when the sim runs late.
    static Vec3 shownPos{};  // what this thread last wrote to player_.pos
    static uint64_t prayerTick = 0;
    plink::Snap sn = locoSnap();
    if (!sn.placed || std::fabs(player_.pos.x - shownPos.x) > 0.05f || std::fabs(player_.pos.y - shownPos.y) > 0.05f ||
        std::fabs(player_.pos.z - shownPos.z) > 0.05f)
      locoPlace(sim_, locomotion_, player_.pos);  // an external write won: reseat (zeroes velocity)
    g_plink.input.store(plink::InputMsg{pendingLocomotionInput_, 1});
    sn = locoSnap();
    if (sn.placeSeq < g_plink.placeSeq) {
      shownPos = player_.pos;  // reseat not consumed yet: hold the requested position
    } else {
      double d[3];
      plink::displayPos(sn, plink::Link::nowNs(), d);
      shownPos = {float(d[0]), float(d[1]), float(d[2])};
    }
    player_.pos = shownPos;
    {  // Cortana 2026-10-26 (Archie item 4): adaptive background cap. Paused (faded screen):
       // uncapped. Still > 250 ms: cap 2 (3 on 8+ logical). Moving: cap 1 + yield to render.
       // <= 4 logical: always cap 1 + yield. World load lifts the cap in loadActive().
       // LL_BG_MAX_ACTIVE (any value) disables the policy; LL_BG_ADAPTIVE=0 too.
      static const bool adaptive = !litholattice::BgThrottle::get().maxActiveSet &&
                                   !(std::getenv("LL_BG_ADAPTIVE") && std::getenv("LL_BG_ADAPTIVE")[0] == '0');
      if (adaptive) {
        static const unsigned hw = std::thread::hardware_concurrency();
        static Vec3 lastP{}; static float lastYaw = 0, lastPitch = 0;
        static int64_t lastMoveNs = 0; static int curCap = -1; static bool curYield = false;
        static uint64_t nSwitch = 0;
        const int64_t now = plink::Link::nowNs();
        const bool moved = std::fabs(shownPos.x - lastP.x) + std::fabs(shownPos.y - lastP.y) + std::fabs(shownPos.z - lastP.z) > 1e-3f ||
                           std::fabs(player_.yaw - lastYaw) + std::fabs(player_.pitch - lastPitch) > 1e-4f;
        lastP = shownPos; lastYaw = player_.yaw; lastPitch = player_.pitch;
        if (moved) lastMoveNs = now;
        int cap; bool yieldR;
        if (hw != 0 && hw <= 4) { cap = 1; yieldR = true; }
        else if (paused_) { cap = 0; yieldR = false; }
        else if (now - lastMoveNs > 250'000'000) { cap = hw >= 8 ? 3 : 2; yieldR = false; }
        else { cap = 1; yieldR = true; }
        if (cap != curCap || yieldR != curYield) {
          pool_.setDefaultMaxActive(cap); pool_.setYieldRender(yieldR);
          curCap = cap; curYield = yieldR;
          if (++nSwitch <= 16 || nSwitch % 256 == 0) logInfo("bg policy: cap=%d yield=%d (switch %llu)", cap, int(yieldR), (unsigned long long)nSwitch);
        }
      }
    }
    if (litholattice::f3trace::on()) {
      f3t::cur.ticks = int(sn.tick > f3t::lastPosTick ? sn.tick - f3t::lastPosTick : 0);
      f3t::cur.loco = 'S';
      f3t::lastPosTick = sn.tick;
    }
    // Digital prayer / mealtime windows stay on this thread, per world tick the body consumed.
    if (sn.tick < prayerTick || prayerTick == 0) prayerTick = sn.tick;
    if (sn.tick > prayerTick && sim_) {
      const uint64_t ticks = std::min<uint64_t>(sn.tick - prayerTick, 64);
      prayerTick = sn.tick;
      const uint64_t perTick = worldclock::unitsPerTick(sim_->stats().targetTps);
      for (uint64_t i = 0; i < ticks; ++i) {
        const uint64_t back = (ticks - 1 - i) * perTick;
        const uint64_t tickTime = worldClock_.time >= back ? worldClock_.time - back : 0;
        if (const int32_t g = prayer::tick(prayer_, litholattice::LocomotionPose(sn.pose) == litholattice::LocomotionPose::Kneel,
                                           playerAdapter_.staticCharge(), tickTime);
            g > 0) {
          playerAdapter_.grantStatic(g);
          logInfo("prayer: +%d static (%u/%u today, day %u, %s)", g, unsigned(prayer_.prayersToday()),
                  unsigned(prayer::kPrayersPerDay), unsigned(prayer_.prayerDay), worldclock::hhmm(tickTime).c_str());
        }
      }
    }
  } else {
    g_plink.input.store(plink::InputMsg{});
    static Vec3 lastPinned{1e30f, 0, 0};
    if (std::fabs(player_.pos.x - lastPinned.x) > 1e-4f || std::fabs(player_.pos.y - lastPinned.y) > 1e-4f ||
        std::fabs(player_.pos.z - lastPinned.z) > 1e-4f) {
      locoPlace(sim_, locomotion_, player_.pos);
      lastPinned = player_.pos;
    }
    pendingLocomotionInput_ = litholattice::LocomotionInput{};
  }
  // --- IPlayer integration (player_adapter.h) ---
  // Mirror the game player into the engine IPlayer (position, the body's
  // TRUE velocity, look, dimension, health), then advance the vitals by the sim
  // world ticks elapsed since the last frame â€” the same counter the
  // lithoscript `wait` pump uses, so vitals freeze while the sim is paused
  // and never run on the sim thread. The head-in-liquid test is one
  // getBlock at the eye cell per frame (the sanctioned handful-of-queries
  // budget), applied to all elapsed ticks.
  playerAdapter_.syncFromGame(active_ ? mcpDimensionNumber(active_->id) : 0, dt,
                              Vec3{float(locoSnap().vel[0]), float(locoSnap().vel[1]), float(locoSnap().vel[2])});
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastVitalsTick_) lastVitalsTick_ = wt;  // world reloaded: resync, never fast-forward
    if (wt > lastVitalsTick_) {
      const uint64_t ticks = wt - lastVitalsTick_;
      lastVitalsTick_ = wt;
      const Vec3 eye = playerEyePos(player_, locoSnap().eye);
      static bool lastHeadInLiquid = false;  // 2026-10-25 (JD hitch): keep the last reading while the sim is mid-tick
      if (const auto eb = sim_->tryGetBlock(int(std::floor(eye.x)), int(std::floor(eye.y)), int(std::floor(eye.z))))
        lastHeadInLiquid = blockIsLiquid(*eb);
      const bool headInLiquid = lastHeadInLiquid;
      // PLY-1: tickVitals normalises the tick count to wall-clock seconds
      // at the sim's target TPS, so drain rates are TPS-invariant.
      playerAdapter_.tickVitals(ticks, headInLiquid, playing && input_.held(Action::Sprint),
                                sim_->stats().targetTps);
    }
  }
  // Sprite system: advance by the same sim-tick stream as the vitals, so
  // sprites freeze while the sim is paused (presentation only â€” sprite
  // state never feeds the sim and is not saved).
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastSpriteTick_) lastSpriteTick_ = wt;  // world reloaded: resync, never fast-forward
    if (wt > lastSpriteTick_) {
      spriteHost_.tickDelta(uint32_t(wt - lastSpriteTick_));
      lastSpriteTick_ = wt;
    }
  }
  // cAiWorld (B6 production wiring): the ai.enabled cvar is the gate
  // (default off = cAiHost constructs nothing, zero effect). When on,
  // advance by the same elapsed sim world ticks as vitals/sprites â€”
  // cAiWorld::advance(simTicks, worldTps), NEVER tick() per sim tick â€”
  // so AI thinks at its clamped 8..16 Hz and freezes with the sim
  // (code-review ENT-2). The observer feed is the live player cell.
  {
    const litholattice::console::CVar* aiCv = consoleRegistry_.findCVar("ai.enabled");
    aiHost_.setEnabled(aiCv && (aiCv->value == "1" || aiCv->value == "true"));
    if (sim_) {
      const uint64_t wt = sim_->stats().worldTick;
      if (wt < lastAiTick_) lastAiTick_ = wt;  // world reloaded: resync, never fast-forward
      if (wt > lastAiTick_) {
        uint64_t ticks = wt - lastAiTick_;
        lastAiTick_ = wt;
        if (ticks > 4096) ticks = 4096;  // cap the catch-up burst (long hitch / tick-debugger step)
        aiHost_.setObserver({int(std::floor(player_.pos.x)), int(std::floor(player_.pos.y)),
                             int(std::floor(player_.pos.z))});
        aiHost_.advance(int(ticks), sim_->stats().targetTps);
      }
    }
  }
  // Deceitor mirrors: sync the render entities to the cAiWorld mobs
  // the advance just moved (and reap mirrors when the world is gone).
  g_split.lap(1);  // [C53-1]
  updateDeceitors(dt);
  g_split.lap(2);
  // MCP player control: refresh the controllable flag and the thread-safe
  // player snapshot the MCP server thread reads (mutex-guarded copy; the
  // live player state is main-thread-owned). The snapshot is sourced from
  // the adapter's IPlayer state â€” transform, true velocity, health and the
  // named stats all come from the engine player system now.
  mcpControllable_.store(menuState_ == MenuState::Playing && playing && !typing_,
                         std::memory_order_relaxed);
  mcpWorldTick_.store(sim_ ? sim_->stats().worldTick : 0, std::memory_order_relaxed);  // Cortana 2026-10-15 1658: F-2
  {
    // Archie PlayI: publish a fresh shared GameState; readers take a snapshot via atomic_load.
    auto next = std::make_shared<cMcpPlayerView::GameState>();
    const litholattice::PlayerState& ps = playerAdapter_.enginePlayer().state();
    next->x = ps.x;
    next->y = ps.y;
    next->z = ps.z;
    next->vx = ps.vx;
    next->vy = ps.vy;
    next->vz = ps.vz;
    next->yaw = ps.yaw;
    next->pitch = ps.pitch;
    next->dimension = ps.dimension;
    next->health = uint8_t(std::clamp(playerAdapter_.health(), 0, 64));
    next->stats = playerAdapter_.namedStats();
    next->hotbar.clear();
    for (const ItemStack& s : player_.inventory.hotbar())
      next->hotbar.push_back(uint16_t(placedBlockForItem(s.itemId)));
    next->selectedSlot = hot_;
    std::atomic_store_explicit(&mcpPlayerState_, std::shared_ptr<cMcpPlayerView::GameState>(std::move(next)),
                               std::memory_order_release);
  }
  // MCP pilot senses: refresh the adapter's cvar knobs once per frame (the
  // console registry is main-thread-only; the MCP thread never reads it).
  {
    const auto cvarInt = [this](const char* name, int fallback) {
      const litholattice::console::CVar* cv = consoleRegistry_.findCVar(name);
      if (!cv) return fallback;
      try {
        return std::stoi(cv->value);
      } catch (...) {
        return fallback;
      }
    };
    const auto cvarFloat = [this](const char* name, float fallback) {
      const litholattice::console::CVar* cv = consoleRegistry_.findCVar(name);
      if (!cv) return fallback;
      try {
        return std::stof(cv->value);
      } catch (...) {
        return fallback;
      }
    };
    mcpSenses_.syncCvars({cvarInt("mcp.vision_width", 160), cvarInt("mcp.vision_height", 90),
                          cvarInt("mcp.audio_tier", 0), cvarInt("camera.chunk_radius", 4),
                          std::clamp(cvarFloat("render.frustum_margin_deg", 5.f), 0.f, 15.f),
                          std::clamp(cvarInt("render.comet_tail_frames", 0), 0, 64),
                          cvarInt("render.velocity_margin", 0) != 0,
                          std::clamp(cvarFloat("render.velocity_margin_gain", 1.f), 0.f, 10.f),
                          cvarInt("render.two_tier", 0) != 0,
                          std::clamp(cvarFloat("render.two_tier_outer_deg", 10.f), 0.f, 30.f)});
    // Nemo View: the object table follows the loaded block materials; the pop-up window
    // follows render.nemo_view. Both are read-only views; nothing here feeds the sim.
    if (const auto mats = resources_.materials(); mats && mats.get() != nemoObjectsFor_) {
      nemoObjectsFor_ = mats.get();
      mcpSenses_.setObjectTable(buildNemoObjectTable(*mats));
      mcpSenses_.setTextureColors(buildNemoTextureColors(resources_.textures()));
    }
    cNemoViewWindow& nemoView = cNemoViewWindow::instance();
    nemoView.setStyle(cvarInt("render.nemo_view_style", 4));
    if (const int wantCols = nemoView.takeGridRequest()) {
      litholattice::nemoview::Grid req;
      if (litholattice::nemoview::parseGrid(std::to_string(wantCols), req)) {
        consoleRegistry_.setCVar("render.ascii_grid", req.name);
        consoleRegistry_.setCVar("render.nemo_view_cols", std::to_string(req.cols));
      }
    }
    litholattice::nemoview::Grid asciiGrid = litholattice::nemoview::kDisplayGrid;
    if (const litholattice::console::CVar* cv = consoleRegistry_.findCVar("render.ascii_grid")) {
      litholattice::nemoview::Grid parsed;
      if (litholattice::nemoview::parseGrid(cv->value, parsed)) asciiGrid = parsed;
    } else {
      litholattice::nemoview::Grid parsed;
      if (litholattice::nemoview::parseGrid(std::to_string(cvarInt("render.nemo_view_cols", 160)), parsed))
        asciiGrid = parsed;
    }
    // The grid is a renderer setting, not something the pop-up invents, so a later
    // main-window ASCII play mode can read the same value and point the same builder
    // at the game view (that path is not wired yet).
    nemoView.setGrid(cvarInt("render.nemo_view_objects", 1) != 0, asciiGrid.cols, asciiGrid.rows,
                     asciiGrid.captureW, asciiGrid.captureH);
    nemoView.setDisplay(cvarInt("render.ascii_gamma", 62), cvarInt("render.ascii_luma_floor", 96),
                       cvarInt("render.ascii_texture_color", 1) != 0);
    const int nemoViewMode = cvarInt("render.nemo_view", 0);
    nemoView.setWanted(nemoViewMode != 0, &mcpSenses_, nemoViewMode == 2);
  }
  // cCamera system (presentation only): sync the camera.* cvars into the
  // camera configs, then advance the cameras â€” samples the player's true
  // velocity, feeds orbit/free input, resolves MCP pilot cameras. Placed
  // before the interaction ray below so third person's camera-facing sync
  // lands in the same frame's ray.
  camera_.syncCvars(litholattice::camera::readCameraCvars(consoleRegistry_));
  camera_.update(input_, dt, playing, menuState_ == MenuState::Playing, width, height);
  // Sprite frame: emit this frame's quads against the active camera pose
  // (frameParams hands them to the renderer), and feed the software form
  // to the MCP senses adapter so agent-vision captures show sprites too.
  {
    const CameraViewPose pose = camera_.viewPose();
    const Vec3 right = litholattice::normalize(litholattice::cross(pose.forward, Vec3{0.f, 1.f, 0.f}));
    const Vec3 up = litholattice::cross(right, pose.forward);
    spriteHost_.buildFrame(pose.position, pose.forward, up);
    mcpSenses_.setSpriteVerts(spriteHost_.softwareVerts(), spriteHost_.softwareOrigin());
  }
  const Vec3 f = player_.forward();
  sim_->setPlayerChunk(int(std::floor(player_.pos.x / 16)), int(std::floor(player_.pos.z / 16)));
  // The interaction ray starts at the EYE (player_.pos is the body's feet
  // since the locomotion wiring; the ray used to start at pos back when
  // pos doubled as the eye).
  const Vec3 eyePos = playerEyePos(player_, locoSnap().eye);
  sim_->setRay({eyePos.x, eyePos.y, eyePos.z, f.x, f.y, f.z});
  g_split.lap(3);  // [C53-1]
  pumpViewGate();  // Archie Phase 1
  pumpSwLod();
  pumpStreaming(dt * 1000.0f);  // generate/load columns around the player; insert finished ones
  g_split.lap(4);
  if (playing && !clickUsed) handleWorldActions();
  g_split.lap(5);
  // LL_THREEWAY: upload phase vs renderFrame (Proposed by JD; co-implemented by JD and Cortana).
  // Design B Stage 2: when the backend stages uploads in its lock-free ring
  // (meshUploadsQueued), this phase takes NO guard at all â€” uploads are
  // staged, the executor applies them at the frame seal, and nothing here
  // waits on the in-flight render. Every other path (the Vulkan split
  // included) keeps the guard exactly as before; the guard is not even
  // constructed on the ring path, so it cannot block on the render's behalf.
  std::unique_ptr<litholattice::threeway::RendererGuard> rgUpload;
  if (!renderer_.meshUploadsQueued()) rgUpload = std::make_unique<litholattice::threeway::RendererGuard>();
  const auto tUp = Clock::now();
  uploadBytes_ = 0;
  // PBR material table: the backend re-uploads only when the registry generation moved
  // (mods registering materials at runtime), so this per-frame call is nearly free.
  // (Stage 2: on the ring path the table is announced instead and lands at
  // the next seal, ahead of that seal's meshes â€” the same order as this call.)
  if (cfg_.materials) renderer_.enqueuePbrMaterials(*cfg_.materials);
  // PBR analytic lights: refresh the emissive cache when the focus column moved or a
  // block edit marked it dirty (game-chosen cadence: checked every frame, rescanned
  // only when needed - a 3x3-column refresh measured ~46 us in the engine tests).
  if (emissiveProvider_ && active_) {
    const int fcx = int(std::floor(player_.pos.x / 16)), fcz = int(std::floor(player_.pos.z / 16));
    if (emissiveProvider_->needsRefresh(fcx, fcz)) emissiveProvider_->refresh(active_->chunks, fcx, fcz);
  }
  uploadMeshes();
  pumpWaferReveal(dt);  // Thread Handshake P7
  g_split.lap(6);  // [C53-1] (includes the PBR/emissive lines just above)
  if (stream_) stream_->setFarLod(settings_.vkFarLod);  // [C54-1]
  // Phase 4 (JD-approved 6:30): follow the bench camera; player_.pos was just reset from the locomotion body.
  if (stream_) uploadBytes_ += stream_->update(benchSet_ ? benchCam_.x : player_.pos.x, benchSet_ ? benchCam_.z : player_.pos.z, resources_.materials());
  g_split.lap(7);
  uploadMs_ = msBetween(tUp, Clock::now());
  if (rgUpload) rgUpload->release();  // LL_THREEWAY (absent on the Stage 2 ring path by design)
  playSounds();
  updateEntities();
  g_split.lap(8);
  updateUi(dt, width, height);
  g_split.lap(9);
  lastUiSplitMs_ = g_split.lastMs[9];  // next-build-2: spike record reads this frame's ui bucket
  ++g_split.frames;
  // 2026-10-26 (Archie triage step 1): the split line (and its fh counters) was suppressed on
  // non-stream benches (still/live/walk/flyover leave stream_ null) unless LK_FRAME_SPLIT=1.
  // Every --bench run now emits it; play still needs LK_FRAME_SPLIT=1.
  if ((stream_ || benchSet_ || frameSplitEnv()) && msBetween(g_split.last, Clock::now()) >= 1000.0) {
    char buf[1024]; int n = 0;
    for (int i = 0; i < 10; ++i)
      n += std::snprintf(buf + n, sizeof buf - size_t(n), " %s=%.2f/%.1f", kSplitNames[i], g_split.ms[i] / std::max(1, g_split.frames), g_split.mx[i]);
    logInfo("update-split ms/frame (avg/max):%s | frames %d, cols +%zu -%zu, capped uploads %zu, mesh queue %zu, pool %.1f%%, admit r %d, fh defer/esc/blk/loco-unlocked %llu/%llu/%llu/%llu",
            buf, g_split.frames, g_colsAdded, g_colsEvicted, g_uploadsCapped, meshQueueDepth(), 100.0 * double(renderer_.meshPoolFill()),
            streamer_ ? std::min(streamer_->admitRadius(), effectiveStreamRadius(cfg_)) : 0,
            (unsigned long long)g_fhDeferrals, (unsigned long long)g_fhEscalations, (unsigned long long)g_fhBlocks,
            (unsigned long long)g_fhLocoUnlocked);
    g_split = UpdateSplit{};
    g_colsAdded = g_colsEvicted = g_uploadsCapped = 0;
    g_fhDeferrals = g_fhEscalations = g_fhBlocks = g_fhLocoUnlocked = 0;
  }
}

void Game::playSounds() {
  simEvents_.clear();
  sim_->drainEvents(simEvents_);
  soundBuf_.clear();
  for (const SimEvent& e : simEvents_) {
    if (e.kind == kEvItemDrop) {
      // Popped block -> item entity at the block's center with a small toss.
      const uint64_t id = entities_.spawn(kEntityItem, e.x + 0.5, e.y + 0.5, e.z + 0.5);
      if (litholattice::Entity* en = entities_.get(id)) {
        en->data = uint32_t(e.value);
        // Cortana review 2026-10-23 (R-DET-1): was rand() - global, unseeded,
        // non-reproducible. Toss now derives from the event itself (splitmix64),
        // so replays and lockstep peers see the same item motion.
        uint64_t h = (uint64_t(uint32_t(e.x)) * 0x9E3779B97F4A7C15ull) ^ (uint64_t(uint32_t(e.y)) << 21) ^
                     (uint64_t(uint32_t(e.z)) * 0xC2B2AE3D27D4EB4Full) ^ (uint64_t(uint32_t(e.value)) << 42);
        auto mix = [](uint64_t z) { z += 0x9E3779B97F4A7C15ull; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); };
        h = mix(h); const double rx = double(h >> 11) * (1.0 / 9007199254740992.0);
        h = mix(h); const double rz = double(h >> 11) * (1.0 / 9007199254740992.0);
        en->vx = (rx - 0.5) * 2.0;
        en->vy = 2.5;
        en->vz = (rz - 0.5) * 2.0;
      }
      continue;
    }
    soundBuf_.push_back(toSoundEvent(e));
  }
  sound_.play(soundBuf_, player_.pos);
}

void Game::updateEntities() {
  // ENT-3 (P2b-2): entities advance on WORLD time, not render-frame dt.
  // The elapsed world ticks since the last frame convert to seconds at
  // the sim's target TPS (the same worldTick stream vitals/locomotion
  // ride), so entities freeze exactly when the world freezes â€” pause
  // menu, tick_freeze, handshake freeze â€” and step at a fixed rate
  // instead of a frame-rate-shaped one. Catch-up is capped like
  // locomotion's (64 ticks): beyond the cap the excess is dropped, not
  // fast-forwarded. Magnet + physics + despawn all use this dt; when
  // the world is frozen dt is 0 and everything below is inert (the
  // pickup scan still runs, as before â€” collecting a resting item does
  // not move the world).
  double dt = 0.0;
  uint64_t entTicks = 0;
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastEntitiesTick_) lastEntitiesTick_ = wt;  // world reloaded: resync
    uint64_t ticks = wt - lastEntitiesTick_;
    lastEntitiesTick_ = wt;
    if (ticks > 64) ticks = 64;
    entTicks = ticks;
    dt = double(ticks) / double(std::max(1, sim_->stats().targetTps));
  }
  // Magnet: items within 4 blocks accelerate toward the player.
  // (Floating items design 2.5: an item still in its pickup delay is
  // skipped here too, or the magnet would drag a just-thrown drop
  // straight back into the pickup radius.)
  const Vec3& pp = player_.pos;
  for (litholattice::Entity* e : entities_.querySphere(pp.x, pp.y, pp.z, 4.0, kEntityItem)) {
    if (entityPickupDelay_.count(e->id)) continue;
    if (e->onGround) {
      const double dx = pp.x - e->x, dy = (pp.y + 1.0) - e->y, dz = pp.z - e->z;
      const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
      if (d > 0.8 && d < 4.0) {
        e->onGround = false;
        const double pull = 14.0 / (d * d + 1.0);
        e->vx += dx / d * pull * dt;
        e->vy += dy / d * pull * dt;
        e->vz += dz / d * pull * dt;
      }
    }
  }
  entities_.tick(dt);
  // Projectiles (P2c, ENT-1): one manager tick per elapsed world tick â€”
  // the locked "projectiles tick with the world" â€” so they freeze with
  // the world exactly like the entities above (entTicks is 0 while the
  // sim is paused/frozen) and step at the world's rate, never frame rate.
  for (uint64_t i = 0; i < entTicks; ++i) projectiles_.tick();
  // Pickup delay countdown (floating items design 2.5): the delay
  // ticks down on world ticks; magnet and pickup both skip entities
  // whose delay is still running, so a player can never re-grab (or
  // magnet-drag) the stack they just threw.
  for (auto it = entityPickupDelay_.begin(); it != entityPickupDelay_.end();) {
    if (entTicks >= it->second) it = entityPickupDelay_.erase(it);
    else { it->second -= uint32_t(entTicks); ++it; }
  }
  // Reap side state for entities that died (despawn, pickup, cull):
  // payload and delay entries live exactly as long as their entity.
  for (auto it = entityPayloads_.begin(); it != entityPayloads_.end();)
    if (!entities_.get(it->first)) it = entityPayloads_.erase(it); else ++it;
  for (auto it = entityPickupDelay_.begin(); it != entityPickupDelay_.end();)
    if (!entities_.get(it->first)) it = entityPickupDelay_.erase(it); else ++it;
  // Forced merging (JD ruling 2026-11-10 08:48 EST, design 2.6 + Part
  // 3 #8): the entity-count backstop. When item entities exceed the
  // cap, merge the nearest same-identity pairs (id order, larger
  // stack absorbs, tie to the lower id, survivor keeps the younger
  // age... forced merges never DELETE an item: counts are conserved
  // exactly, and pairs that would overflow the stack cap do not
  // merge). Runs at most once per 256 world ticks.
  {
    static constexpr size_t kItemEntityCap = 1024;
    static uint64_t lastMergeTick = 0;
    const uint64_t wtNow = sim_ ? sim_->stats().worldTick : 0;
    // Cheap gate first: the manager's total size bounds the item
    // count, so the (allocating) census only runs when a breach is
    // even possible, and then at most once per 256 world ticks.
    if (entities_.size() > kItemEntityCap && wtNow >= lastMergeTick + 256) {
      std::vector<litholattice::Entity*> items;
      for (litholattice::Entity* e : entities_.querySphere(pp.x, pp.y, pp.z, 1e9, kEntityItem)) items.push_back(e);
      if (items.size() > kItemEntityCap) {
        lastMergeTick = wtNow;
        std::sort(items.begin(), items.end(), [](const litholattice::Entity* a, const litholattice::Entity* b) { return a->id < b->id; });
        std::vector<char> gone(items.size(), 0);
        for (size_t i = 0; i < items.size(); ++i) {
          if (gone[i]) continue;
          litholattice::Entity* a = items[i];
          const ItemStack* pa = entityPayload(a->id);
          size_t bestJ = items.size();
          double bestD = 1e18;
          for (size_t j = 0; j < items.size(); ++j) {
            if (j == i || gone[j]) continue;
            litholattice::Entity* b = items[j];
            if (b->data != a->data) continue;
            const ItemStack* pb = entityPayload(b->id);
            if ((pa == nullptr) != (pb == nullptr)) continue;
            if (pa && pb && !pa->sameIdentity(*pb)) continue;
            if (uint32_t(a->count) + uint32_t(b->count) > uint32_t(stackCapFor(a->data))) continue;
            const double dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
            const double d = dx * dx + dy * dy + dz * dz;
            if (d < bestD) { bestD = d; bestJ = j; }
          }
          if (bestJ < items.size()) {
            litholattice::Entity* b = items[bestJ];
            litholattice::Entity* big = a->count >= b->count ? a : b;
            litholattice::Entity* small = big == a ? b : a;
            big->count = uint32_t(big->count) + uint32_t(small->count);
            if (small->age < big->age) big->age = small->age;  // survivor keeps the younger age (more life left)
            entities_.despawn(small->id);
            entityPayloads_.erase(small->id);
            entityPickupDelay_.erase(small->id);
            gone[small == a ? i : bestJ] = 1;
          }
        }
      }
    }
  }
  // Pickup: within 1.2 blocks the item goes into the inventory (42 slots, 512/stack).
  for (litholattice::Entity* e : entities_.querySphere(pp.x, pp.y, pp.z, 1.2, kEntityItem)) {
    if (entityPickupDelay_.count(e->id)) continue;  // still in its pickup delay
    if (const ItemStack* payload = entityPayload(e->id)) {
      // Data-bearing stack (design 2.9 (a)): identity pickup is
      // all-or-nothing through placeStack... a partial plain add
      // would strip the name/certificates, which is exactly the
      // identity loss JD's fork ruling forbids. If it does not fit
      // whole, it stays in the world whole.
      ItemStack whole = *payload;
      whole.count = uint16_t(e->count);
      if (player_.inventory.placeStack(whole)) {
        entities_.despawn(e->id);
        entityPayloads_.erase(e->id);
      }
      continue;
    }
    const uint16_t left = player_.inventory.addItem(e->data, uint16_t(e->count));
    if (left == 0) {
      entities_.despawn(e->id);  // fully collected
    } else {
      e->count = left;  // inventory full: stays in the world
    }
  }
}

// Design B Stage 2: hold an upload the renderer's ring refused (ring full)
// for an in-order retry at the next uploadMeshes(). Never a drop â€” this is
// the guarantee behind the ring's never-block producer: replaces, PBR
// meshes, tombstones and clears all ride the same queue.
void Game::spillRingUpload(uint8_t kind, ChunkMesh&& mesh, litholattice::PbrChunkMesh&& pbr) {
  ringSpill_.push_back(RingSpill{kind, std::move(mesh), std::move(pbr)});
  ++ringSpillEvents_;
  ringSpillMax_ = std::max(ringSpillMax_, ringSpill_.size());
}

// Cortana 2026-10-25 (box, upload pass): LL_ASYNC_CHUNK_UPLOAD=1 opts the game's mesh workers into
// prepareChunkMeshAsync (the bench_stream [C53-2] path). Default OFF until verified on Vulkan.
bool asyncChunkUploadOn() {
  static const bool on = [] { const char* e = std::getenv("LL_ASYNC_CHUNK_UPLOAD"); return e && e[0] == '1'; }();
  return on;
}
static std::atomic<uint64_t> g_uploadsPublished{0};
uint64_t asyncChunkUploadsPublished() { return g_uploadsPublished.load(std::memory_order_relaxed); }

void Game::uploadMeshes() {
  uploadItems_ = 0; uploadBatchMax_ = 0;  // next-build-2: spike-record fields for this frame
  // GUI lag round 2 (2026-11-10): drain/commit timing split, diagnostic
  // only (tickprof-gated clock reads, same discipline as buildHudVertices).
  const bool r2prof = litholattice::tickprof::enabled();
  const auto r2t0 = r2prof ? Clock::now() : Clock::time_point{};
  lastMeshDrainMs_ = 0; lastMeshCommitMs_ = 0; lastMeshDrained_ = 0;
  lastMeshDrainVerts_ = 0; lastMeshBatchLanded_ = 0; lastMeshBatchVerts_ = 0;
  // JD ruling t3366u: a paused sim stops the render pipeline - no Render Ingress pops,
  // no spill retries, no GPU uploads. Meshes keep queueing (lossless, see
  // kRenderIngressCapBytes); on unpause the normal N/K budget below drains them, so
  // the catch-up is spread over frames (no single-frame spike).
  if (renderPaused()) { ++renderPausedFrames_; if (r2prof) lastMeshCommitMs_ = msBetween(r2t0, Clock::now()); return; }
  // Design B Stage 2: retry spilled uploads first — they are the oldest
  // staged entries, and order across the spill/ring boundary is what keeps
  // a tombstone behind the uploads it precedes. Stops at the first refusal
  // (ring still full); the rest wait for the next frame, still in order.
  while (!ringSpill_.empty()) {
    RingSpill& e = ringSpill_.front();
    const bool ok = e.kind == 0   ? renderer_.enqueueChunkMesh(std::move(e.mesh))
                    : e.kind == 1 ? renderer_.enqueuePbrChunkMesh(std::move(e.pbr))
                                  : renderer_.enqueueClearChunkMeshes();
    if (!ok) break;
    ringSpill_.pop_front();
  }
  // Coalesce: a busy circuit remeshes the same sub-chunk ~120x/s; only its newest mesh is worth
  // uploading. Then upload in arrival order under a byte budget, well below the renderer's 8 MB
  // staging ring, so a burst never forces a synchronous copy flush (the old 49 ms hitches).
  // Cortana 2026-10-15 1350 (MIXED) - handshake-coupled meshing, publish at
  // the phase boundary: meshes of one sim flush batch (one settled world
  // state) are held until the whole batch is meshed, then land in the SAME
  // frame (outside the streaming budget), so the view never shows sub-chunks
  // from two different world states side by side. Batches above
  // kAtomicBatchMax (remesh-all, big loads) keep the budgeted path below.
  constexpr uint32_t kAtomicBatchMax = 64;
  auto land = [this](ChunkMesh& m) -> bool {
    if (m.seq == 0 || m.epoch != epoch_) return false;  // culled batch completion / unloaded dimension
    uint64_t& seq = meshSeq_[packPos(m.cx, m.sy, m.cz)];
    if (seq != 0 && seq >= m.seq) { litholattice::workTrace("drop", m.cx, m.sy, m.cz, m.seq); return false; }  // a newer mesh already landed
    litholattice::workTrace("land", m.cx, m.sy, m.cz, m.seq);  // duplicate-work trace (LL_WORK_TRACE)
    if (seq == 0) ++meshesReceived_;
    seq = m.seq;
    uploadBytes_ += m.verts.size() * sizeof(PackedVertex);
    if (meshHashOn_) meshLedger_.note(packPos(m.cx, m.sy, m.cz), litholattice::meshBytesHash(m.verts.data(), m.verts.size() * sizeof(PackedVertex)));
    mcpSenses_.noteMesh(m);  // vision cache copy, before the move into the GL renderer
    // Design B Stage 2: staged on the ring path (applied by the executor at
    // the frame seal), immediate on every other path. A full ring spills
    // the mesh â€” counted as landed here, because the game has accepted it
    // and the spill retries it in order next frame; it is never dropped.
    if (m.prepared && m.prepared->mesh.ready) {  // upload pass: already in GPU memory; publish only
      m.prepared->publish();
      ++g_uploadsPublished;
      return true;
    }
    if (!renderer_.enqueueChunkMesh(std::move(m))) spillRingUpload(0, std::move(m), {});
    return true;
  };
  { std::vector<ChunkMesh> fresh; meshOut_.drainInto(fresh);
    meshInFlight_ -= int(fresh.size());  // P8 quiescence: one ChunkMesh per submitted snapshot
    if (r2prof) {  // GUI lag round 2: what the drain actually carried
      lastMeshDrained_ = int(fresh.size());
      for (const auto& m : fresh) lastMeshDrainVerts_ += m.verts.size();
    }
    if (!colMeshPending_.empty())
      for (const auto& m : fresh) p5NoteMeshArrived(m.cx, m.cz);  // P5: MESH -> STAGED_RENDER bookkeeping
    for (auto& m : fresh) {
      if (m.batch != 0 && m.batchSize > 1 && m.batchSize <= kAtomicBatchMax) {
        const uint64_t id = m.batch;
        auto& b = meshBatches_[id];
        b.push_back(std::move(m));
        if (b.size() >= size_t(b.front().batchSize)) {
          uploadBatchMax_ = std::max(uploadBatchMax_, int(b.size()));  // next-build-2: spike record
          if (r2prof) {  // GUI lag round 2: atomic landings are inside the drain phase
            lastMeshBatchLanded_ += int(b.size());
            for (const auto& bm : b) lastMeshBatchVerts_ += bm.verts.size();
          }
          for (auto& bm : b) if (land(bm)) ++uploadItems_;
          meshBatches_.erase(id);
        }
        continue;
      }
      const int64_t k = packPos(m.cx, m.sy, m.cz);
      const auto it = pendingMesh_.find(k);
      if (it == pendingMesh_.end()) { pendingOrder_.push_back(k); pendingMesh_.emplace(k, std::move(m)); }
      else if (m.epoch != it->second.epoch || m.seq > it->second.seq) it->second = std::move(m);
    } }
  // 2026-10-25 stutter pass: at most 32 sub-chunks or ~1.5 ms of commits per frame, nearest first
  // (was 64, oldest first): a far burst no longer stalls the frame ahead of the ground underfoot.
  constexpr int kUploadBudget = 32;                 // sub-chunks per frame
  constexpr double kUploadMsBudget = 1.5;           // main-thread commit time per frame
  constexpr size_t kUploadBytesBudget = 3u << 20;   // bytes per frame
  constexpr int kUploadFloor = 16;                  // always commit this many (time cap applies past it)
  // P5 render pop N/K: at most N sub-chunks or K bytes per frame (the first always pops).
  litholattice::RenderPopBudget pop;
  pop.maxItems = kUploadBudget;
  pop.maxBytes = kUploadBytesBudget;
  int uploads = 0;
  {  // nearest-first: re-sort only when the player changes chunk or the queue grew past the last sort
    const int pcx = int(std::floor(player_.pos.x / 16)), pcy = int(std::floor(player_.pos.y / 16)), pcz = int(std::floor(player_.pos.z / 16));
    const int64_t here = packPos(pcx, std::clamp(pcy, 0, kSubChunks - 1), pcz);
    if (pendingOrder_.size() > size_t(kUploadBudget) && (here != uploadSortedAt_ || pendingOrder_.size() > uploadSortedSize_ + size_t(kUploadBudget))) {
      auto dist = [&](int64_t k) { int x, y, z; unpackPos(k, x, y, z); return (x - pcx) * (x - pcx) + (y - pcy) * (y - pcy) + (z - pcz) * (z - pcz); };
      std::stable_sort(pendingOrder_.begin(), pendingOrder_.end(), [&](int64_t a, int64_t b) { return dist(a) < dist(b); });
      uploadSortedAt_ = here;
    }
    uploadSortedSize_ = pendingOrder_.size();
  }
  if (r2prof) lastMeshDrainMs_ = msBetween(r2t0, Clock::now());  // GUI lag round 2: drain phase ends here
  const auto tUploads = Clock::now();  // the cap measures commits, not the sort
  for (; !pendingOrder_.empty() && uploads < pop.maxItems;) {
    if (uploads >= kUploadFloor && msBetween(tUploads, Clock::now()) > kUploadMsBudget) { g_uploadsCapped += pendingOrder_.size(); break; }
    const int64_t k = pendingOrder_.front();
    auto node = pendingMesh_.find(k);
    if (node == pendingMesh_.end()) { pendingOrder_.pop_front(); continue; }
    const size_t mb = node->second.verts.size() * sizeof(PackedVertex);
    if (!pop.admit(mb)) break;  // rest next frame
    ChunkMesh m = std::move(node->second);
    pendingMesh_.erase(node); pendingOrder_.pop_front();
    pop.bytes += mb;
    if (land(m)) { ++uploads; ++pop.items; ++uploadItems_; }
  }
  p5AdvanceStagedRender();  // P5: STAGED_RENDER -> ACTIVE once nothing of the column waits
  // Stale staged batches (dimension switch dropped their epoch) never complete.
  for (auto it = meshBatches_.begin(); it != meshBatches_.end();)
    it = (!it->second.empty() && it->second.front().epoch != epoch_) ? meshBatches_.erase(it) : std::next(it);
  // PBR (BinaryQuad) queue: same coalescing and budget, shared with the classic loop
  // above (one per-frame budget for both paths). Only non-empty on the BinaryQuad option.
  { std::vector<PbrChunkMesh> fresh; pbrMeshOut_.drainInto(fresh);
    for (auto& m : fresh) {
      const int64_t k = packPos(m.cx, m.sy, m.cz);
      const auto it = pbrPendingMesh_.find(k);
      if (it == pbrPendingMesh_.end()) { pbrPendingOrder_.push_back(k); pbrPendingMesh_.emplace(k, std::move(m)); }
      else if (m.epoch != it->second.epoch || m.seq > it->second.seq) it->second = std::move(m);
    } }
  for (; !pbrPendingOrder_.empty() && uploads < pop.maxItems;) {
    if (uploads >= kUploadFloor && msBetween(tUploads, Clock::now()) > kUploadMsBudget) break;
    const int64_t k = pbrPendingOrder_.front();
    auto node = pbrPendingMesh_.find(k);
    if (node == pbrPendingMesh_.end()) { pbrPendingOrder_.pop_front(); continue; }
    const size_t mb = node->second.verts.size() * sizeof(PbrQuadVertex);
    if (!pop.admit(mb)) break;  // rest next frame
    PbrChunkMesh m = std::move(node->second);
    pbrPendingMesh_.erase(node); pbrPendingOrder_.pop_front();
    pop.bytes += mb;
    if (m.epoch != epoch_) continue;  // from an unloaded dimension
    uint64_t& seq = pbrMeshSeq_[packPos(m.cx, m.sy, m.cz)];
    if (seq != 0 && seq >= m.seq) continue;  // a newer mesh already landed
    seq = m.seq;
    uploadBytes_ += m.verts.size() * sizeof(PbrQuadVertex);
    // Design B Stage 2: same staging/spill discipline as the classic path.
    if (!renderer_.enqueuePbrChunkMesh(std::move(m))) spillRingUpload(1, {}, std::move(m));
    ++uploads; ++pop.items; ++uploadItems_;
  }
  p8MeshHashCheck();
  if (r2prof) lastMeshCommitMs_ = msBetween(r2t0, Clock::now());  // GUI lag round 2: whole call, drain included
}

// P8 (Archie Q7): render-only MeshHash gate. Never part of the world/anchor hashes.
// Asserted only at quiescence: nothing in flight or pending, no border ring, and
// every meshed column was meshed against its current BorderGen (MeshGen == BorderGen).
void Game::p8MeshHashCheck() {
  if (!meshHashOn_ || !active_ || meshLedger_.size() == 0) return;
  if (!borderRing_.empty() || !deferredMeshKeys_.empty() || !pendingOrder_.empty() || !ringSpill_.empty()) return;
  const bool q = meshLedger_.quiescent(size_t(std::max(0, meshInFlight_.load())), [this](uint64_t key) -> int {
    const ChunkColumn* c = active_->chunks.column(int(int32_t(key >> 32)), int(int32_t(key & 0xffffffffu)));
    return c ? int(litholattice::ChunkStateWord::borderGen(c->state.load())) : -1;
  });
  if (!q) return;
  const uint64_t h = meshLedger_.combined();
  if (h == meshHashLogged_) return;
  meshHashLogged_ = h;
  logInfo("meshhash: %016llx subs=%zu (quiescent, MeshGen == BorderGen)", (unsigned long long)h, meshLedger_.size());
}

FrameParams Game::frameParams(int width, int height) const {
  FrameParams p;
  p.width = width; p.height = height;
  // cCamera system: the view pose comes from the active engine camera. In
  // first person the pose IS the player's (same values, same expressions
  // as the pre-camera-system path: perspective 1.2 rad, lookDir eye/forward).
  const CameraViewPose view = activeCameraPose();
  p.camPos = view.position;
  p.viewProj = mul(perspective(view.fovRad, float(width) / float(height), 0.05f, 1000.f),
                   lookDir(view.position, view.forward));
  p.yawRad = view.yaw;  // feeds the velocity-adaptive culling prototype
  p.pitchRad = view.pitch;
  p.sky = active_->id == "wafer" ? Vec3{0.58f, 0.70f, 0.86f} : Vec3{0.55f, 0.74f, 0.95f};
  // JD Vulkan push (Cortana 2026-10-16): LL_BENCH_SKY="r,g,b" overrides the sky for bench
  // screenshots only (e.g. the night-glow capture). Render-only; unset = unchanged.
  if (benchPath_ == "still") {
    if (const char* e = std::getenv("LL_BENCH_SKY")) {
      float c[3];
      if (std::sscanf(e, "%f,%f,%f", &c[0], &c[1], &c[2]) == 3) p.sky = Vec3{c[0], c[1], c[2]};
    }
  }
  if (revealSky_) p.sky = Vec3{p.sky.x * skyFade_, p.sky.y * skyFade_, p.sky.z * skyFade_};  // P7: dark until SKYBOX_ONLINE
  p.fogEnd = 16.f * float(cfg_.worldChunks) * 1.1f;
  const SimStats& st = sim_->stats();
  p.highlight = st.inReach; p.hx = st.hx; p.hy = st.hy; p.hz = st.hz;
  p.crosshair = showHud_ && !inventoryOpen_ && !editorOpen() && menuState_ == MenuState::Playing;
  p.debugOverlay = showDebug_;
  // Sprite pass inputs (quads built in update() against this same pose):
  // backends with a sprite path texture them from the registry atlas.
  p.spriteQuads = spriteHost_.drawQuads().data();
  p.spriteQuadCount = spriteHost_.drawQuads().size();
  p.spriteAtlasPages = spriteHost_.registry().atlasPages();
  p.spriteAtlasVersion = spriteHost_.registry().atlasVersion();
  // Wafer ground pulse (JD 2026-10-11): sim-time seconds drive the shader's
  // pulse phase. Pausing the sim freezes worldTick, so the pulse freezes too -
  // never wall-clock. render.ground_pulse toggles it live (no remesh).
  p.groundPulseOn = settings_.groundPulse && settings_.floorPulse > 0;  // 2026-10-24 (JD): Floor pulse Off/Slow/Normal
  p.leafTwinkle = resolvedTwinkle(settings_);  // 2026-10-16 crystal twinkle: render clock (p.timeSec)
  p.vkFx = cfg_.vkFxOverride >= 0 ? cfg_.vkFxOverride : settings_.vkFx;  // JD Vulkan push: Vulkan-only effects (render-only)
  p.vkBloomStrength = settings_.vkBloomStrength;
  p.vkHeartbeatGlow = settings_.vkHeartbeatGlow;  // 0.0.1.53 [C53-3] (Proposed by JD; co-implemented by JD and Cortana)
  p.groundPulseTimeSec = sim_ ? litholattice::groundPulseTimeSec(sim_->stats().worldTick,
                                                                uint32_t(settings_.tps))
                              : 0.f;
  // Wafer ground traces: same sim clock as the pulse (pausing freezes the
  // packets), independent live toggle (render.ground_traces, default on).
  p.groundTraceOn = settings_.groundTraces;
  p.groundTraceTimeSec = p.groundPulseTimeSec;
  if (settings_.floorPulse == 1) p.groundPulseTimeSec *= 0.35f;
  if (benchPath_ == "showcase") {  // --showcase: every visual effect on (render-only, settings untouched)
    p.groundPulseOn = true; p.groundTraceOn = true; p.leafTwinkle = 2; p.vkFx = 127;
    p.groundPulseTimeSec = p.groundTraceTimeSec;
  }  // Slow: about a third of Normal speed (traces keep their clock)
  {
    // GUI draw-cost program, cure 1 (2026-11-10): split delivery. The
    // composed layer still fills p.hud (every consumer that has not
    // split keeps working); the stream refs ride beside it so the
    // backends expand and upload static and dynamic separately.
    litholattice::HudLayerSplit hudSplit;
    ui_.paintSplit(p.hud, hudSplit, *resources_.materials(), [this](BlockId b) { return resources_.icon(b); },
                   [this](uint32_t itemId) { return resources_.itemIcon(itemId); });
    p.hudStatic = {hudSplit.staticLayer, hudSplit.staticGeneration};
    p.hudDynamic = {hudSplit.dynamicLayer, hudSplit.dynamicGeneration};
  }
  // PBR analytic lights for the chunk_pbr uniforms (phase 2, Worker 3): K-nearest
  // emissive voxels to the camera, reduced per frame from the cached provider scan.
  // Backends without a PBR path ignore the array.
  p.pbrLightCount = 0;
  if (emissiveProvider_) {
    // Optimization (audit App.A/game-1): reuse the member vector and skip the
    // whole emissive-cache walk when neither the camera column nor the
    // provider's cache changed since the last collection. The per-frame
    // refresh in update() already keeps the provider current; this only
    // re-reduces when something actually moved or was rebuilt.
    const int ccx = int(std::floor(p.camPos.x / 16)), ccz = int(std::floor(p.camPos.z / 16));
    if (!pbrLightsValid_ || ccx != pbrLightsCamCx_ || ccz != pbrLightsCamCz_ ||
        emissiveProvider_->needsRefresh(pbrLightsCamCx_, pbrLightsCamCz_)) {
      // Assignment reuses the member's capacity: no per-frame allocation.
      pbrLightsCache_ = lightCollector_.collect(
          p.camPos.x, p.camPos.y, p.camPos.z,
          [this](auto&& sink) { emissiveProvider_->forEachEmissive(sink); });
      pbrLightsCamCx_ = ccx; pbrLightsCamCz_ = ccz; pbrLightsValid_ = true;
    }
    const size_t n = std::min(pbrLightsCache_.size(), FrameParams::kMaxPbrFrameLights);
    for (size_t i = 0; i < n; ++i) p.pbrLights[i] = pbrLightsCache_[i];
    p.pbrLightCount = uint32_t(n);
  }
  if (litholattice::f3trace::on()) { f3t::cur.cam[0] = p.camPos.x; f3t::cur.cam[1] = p.camPos.y; f3t::cur.cam[2] = p.camPos.z; }  // LL_TRACE_F3
  return p;
}

void Game::beginTyping(int x, int y, int z) {
  typing_ = true; typeX_ = x; typeY_ = y; typeZ_ = z;
  input_.push(InputContext::KeyboardTyping);  // letters stop reaching movement / hotbar actions
  EditCmd c{EditCmd::Kind::Use}; c.x = x; c.y = y; c.z = z;
  sim_->edit(c);  // keyboard shows "capturing"
  logInfo("keyboard at %d %d %d: typing capture ON (Esc ends)", x, y, z);
}

void Game::beginPostEdit(int x, int y, int z) {
  typing_ = true; postTyping_ = true; typeX_ = x; typeY_ = y; typeZ_ = z;
  typed_.clear();
  input_.push(InputContext::KeyboardTyping);
  logInfo("post at %d %d %d: writing (Enter = new line, Backspace deletes, Esc ends)", x, y, z);
}

void Game::endTyping() {
  if (!typing_) return;
  typing_ = false;
  input_.pop(InputContext::KeyboardTyping);
  if (postTyping_) {  // a post has no capture state to toggle back
    postTyping_ = false;
    typed_.clear();
    logInfo("post writing OFF");
    return;
  }
  EditCmd c{EditCmd::Kind::Use}; c.x = typeX_; c.y = typeY_; c.z = typeZ_;
  sim_->edit(c);
  logInfo("keyboard typing capture OFF");
}

// While capturing: text-channel characters (A-Z, 0-9, symbols) -> SIXBIT code + 1 out the front;
// Enter / Backspace (including auto-repeat) -> right port.
void Game::handleTyping() {
  const TextInput& in = input_.text();
  if (postTyping_) {  // 2026-10-25 Posts: printable ASCII, Enter = new line, Backspace deletes
    auto edit = [&](int ch, int control) {
      EditCmd c{EditCmd::Kind::Command}; c.x = typeX_; c.y = typeY_; c.z = typeZ_;
      c.command = SimCommand{kCmdPostEdit, typeX_, typeY_, typeZ_, ch, control};
      sim_->edit(c);
    };
    for (char32_t ch : in.chars)
      if (ch >= 32 && ch <= 126) { edit(int(ch), post::kTypeChar); typed_ += char(ch); }
    for (int i = 0; i < in.enter; ++i) { edit(0, post::kNewline); typed_ += '/'; }
    for (int i = 0; i < in.backspace; ++i) { edit(0, post::kBackspace); if (!typed_.empty()) typed_.pop_back(); }
    if (typed_.size() > 60) typed_.erase(0, typed_.size() - 60);
    return;
  }
  auto pulse = [&](int front, int control) {
    EditCmd c{EditCmd::Kind::Command}; c.x = typeX_; c.y = typeY_; c.z = typeZ_;
    c.command = SimCommand{kCmdKeyPulse, typeX_, typeY_, typeZ_, uint8_t(front), uint8_t(control)};
    sim_->edit(c);
  };
  for (char32_t ch : in.chars) {
    const int code = sixbitCode(ch);
    if (code >= 0) { pulse(code + 1, 0); typed_ += sixbitChar(code); }
  }
  for (int i = 0; i < in.enter; ++i) { pulse(0, kKeyEnter); typed_.clear(); }
  for (int i = 0; i < in.backspace; ++i) { pulse(0, kKeyBackspace); if (!typed_.empty()) typed_.pop_back(); }
  if (typed_.size() > 40) typed_.erase(0, typed_.size() - 40);
}

std::string Game::targetText() const {
  const SimStats& st = sim_->stats();
  if (!st.hit) return "-";
  // Playtest 1 #3 (2026-10-26): one compact line, name id @x,y,z; the dense inspector line only
  // with LL_LOOK_VERBOSE=1. Falls back to the registry name when the catalog has no text.
  std::string text = cBehaviorCatalog::get().describe(st.hitBlock, st.hitState);
  if (const size_t nl = text.find('\n'); nl != std::string::npos) text.resize(nl);
  if (text.find_first_not_of(" \t") == std::string::npos) text = std::string(blockRegistry().def(st.hitBlock).name);
  text += "  " + std::string(blockRegistry().idOf(st.hitBlock));  // 2026-10-24 (JD): namespaced id on the look line
  text += "  @" + std::to_string(st.hx) + "," + std::to_string(st.hy) + "," + std::to_string(st.hz);
  static const bool verbose = [] { const char* e = std::getenv("LL_LOOK_VERBOSE"); return e && *e == '1'; }();
  if (verbose && st.hitInfo[0]) { text += "\n"; text += st.hitInfo; }  // inspector: ids, raw state, net, memory
  if (!st.inReach) text += "  (out of reach)";
  return text;
}

// 2026-10-16 (JD): active renderer + release info for the F3 overlay and the title bar.
std::string Game::rendererInfo() const {
  const std::string n = renderer_.name();
  if (n == "software")
    return "Software " + std::string(litholattice::swsimd::activePath()) + " x" +
           std::to_string(litholattice::cSoftwareRenderer::renderThreads()) + " threads";
  if (n.rfind("OpenGL", 0) == 0) return "GL33";
  return n;  // "Vulkan"
}
std::string Game::releaseInfo() {
  return buildInfo();  // build_version.h: "LithoKraft Alpha a.b.c.d (commit) Release|Debug built ..."
}

// HUD content; the widgets live in cGameUi.
HudModel Game::hudModel() const {
  HudModel m;
  // The HUD hotbar is a snapshot of the player inventory's hotbar slots â€”
  // the same store placement, pickups and player.lat use (P2b-1).
  for (int i = 0; i < kHotbarSize; ++i) m.hotbar[size_t(i)] = player_.inventory.hotbar()[size_t(i)];
  m.selected = hot_;
  // Item 4: the creative screen's Agent bars read the real vitals/stats.
  m.vitHealth = playerAdapter_.health();
  m.vitKharge = playerAdapter_.kharge();
  m.vitStatic = playerAdapter_.staticCharge();
  {  // 2026-10-16 (JD): looked-at block name over the hotbar (display-only; hidden with no target)
    const SimStats& lst = sim_->stats();
    // 1520: cached per target change (no per-frame string build); LL_P2_PERF_OFF bit 1 hides it (A/B)
    static std::decay_t<decltype(lst.hitBlock)> cachedId = kAir; static std::string cachedName;
    if (!(llP2PerfOff() & 1) && lst.hit && lst.hitDist <= (creativeMode() ? 16.f : 8.f) + 0.5f && lst.hitBlock != kAir) {
      if (lst.hitBlock != cachedId) { cachedId = lst.hitBlock; cachedName = std::string(blockRegistry().def(lst.hitBlock).name); }
      m.lookName = cachedName;
    }
  }
  m.visible = showHud_ && menuState_ == MenuState::Playing;  // F1; the menu has its own overlay
  // Cortana 2026-10-26 (item 5): pinned screenshots. With a bench path and LL_SW_SHOT (or
  // LL_BENCH_PIN=1) the live timing text (F3 block / FPS readout) is left out: run-to-run it was
  // the only difference between shots of identical code (1-2k px, rows 18-74 and 554-623).
  static const bool kShotPinned = std::getenv("LL_SW_SHOT") || (std::getenv("LL_BENCH_PIN") && std::getenv("LL_BENCH_PIN")[0] == '1');
  const bool pinned = kShotPinned && benchSet_;
  if (kShotPinned) {  // hold the shot frame count until the mesh queue has been empty for 30 frames (cap 1200 frames)
    static int settled = 0, held = 0;
    const bool frozenOk = !g_shotFreezeTick || g_simFrozen.load(std::memory_order_acquire);
    settled = (meshQueueDepth() == 0 && frozenOk) ? settled + 1 : 0;  // count only after the freeze (remeshes from the last ticks)
    const bool hold = settled < 60 && held < 1800;
    if (hold) ++held;
    static bool logged = false;
    if (!hold && !logged) { logged = true; logInfo("shot pin: released after %d held frames (mesh queue %s, sim %s)", held, settled >= 60 ? "drained" : "NOT drained", g_simFrozen.load() ? "frozen" : "NOT frozen"); }
    litholattice::cSoftwareRenderer::setShotHold(hold);
  }
  if (showDebug_ && !pinned) {      // F3
    const SimStats& st = sim_->stats();
    char l[9][160];
    std::snprintf(l[0], sizeof l[0], "FPS %.0f  frame %.2f ms (max %.1f)", fps_, frameAvgMs_, frameMaxMs_);
    std::snprintf(l[1], sizeof l[1], "TPS %.1f / %d  tick %.3f ms (max %.3f)  day %u %s", st.tps, st.targetTps, st.tickMs,
                  st.tickMaxMs, unsigned(worldclock::dayOf(worldClock_.time)), worldclock::hhmm(worldClock_.time).c_str());
    std::snprintf(l[2], sizeof l[2], "sim budget %.2f%%  dropped %llu  EUV %s", st.budgetPct,
                  (unsigned long long)st.droppedTicks, euvStats(st).enabled ? "on" : "off");
    // Live locomotion state (mode + pose from the body's controller):
    // e.g. "walk Â· sneak", "fly Â· stand". Replaces the old "flying (no
    // gravity yet)" confession â€” gravity exists now.
    const plink::Snap f3s = locoSnap();
    const char* modeName = f3s.flying ? "fly" : "walk";
    const char* poseName = litholattice::LocomotionPose(f3s.pose) == litholattice::LocomotionPose::Sneak   ? "sneak"
                           : litholattice::LocomotionPose(f3s.pose) == litholattice::LocomotionPose::Crawl ? "crawl"
                             : litholattice::LocomotionPose(f3s.pose) == litholattice::LocomotionPose::Kneel ? "kneel"  // Cortana 2026-10-15 2240
                                                                                        : "stand";
    std::snprintf(l[3], sizeof l[3], "dim %s  meshes %d/%d  %s \xc2\xb7 %s%s", active_->id.c_str(),
                  meshesReceived_, totalSubs_, modeName, poseName,
                  f3s.onGround ? "" : " (air)");
    std::snprintf(l[4], sizeof l[4], "look: %s", targetText().c_str());
    std::snprintf(l[5], sizeof l[5], "health %d  oxygen %d  stamina %d  kharge %d  |  XYZ %.2f %.2f %.2f  block %d %d %d",
                  playerAdapter_.health(), playerAdapter_.oxygen(), playerAdapter_.stamina(),
                  playerAdapter_.kharge(), double(player_.pos.x), double(player_.pos.y), double(player_.pos.z),
                  int(std::floor(double(player_.pos.x))), int(std::floor(double(player_.pos.y))), int(std::floor(double(player_.pos.z))));  // 2026-10-24 (JD): feet / origin
    if (litholattice::f3trace::on()) {  // LL_TRACE_F3: the pos this string was built from, and the string
      f3t::cur.hud[0] = player_.pos.x; f3t::cur.hud[1] = player_.pos.y; f3t::cur.hud[2] = player_.pos.z;
      f3t::cur.built = l[5];
      f3t::cur.worldTick = st.worldTick; f3t::cur.posTick = f3t::lastPosTick;
      f3t::cur.pos[0] = player_.pos.x; f3t::cur.pos[1] = player_.pos.y; f3t::cur.pos[2] = player_.pos.z;
    }
    std::snprintf(l[6], sizeof l[6], "renderer: %s", rendererInfo().c_str());  // 2026-10-16 (JD)
    std::snprintf(l[7], sizeof l[7], "%s", releaseInfo().c_str());
    std::snprintf(l[8], sizeof l[8], "%s", lithokraft::cChunkCoordText::line(int(std::floor(double(player_.pos.x))), int(std::floor(double(player_.pos.y))),
                  int(std::floor(double(player_.pos.z))), st.hit, st.hx, st.hy, st.hz).c_str());  // 2026-10-31 (JD): chunk seam aid
    {  // 2026-11-04 (Elfie B2): char rows are not assignable (std::rotate is MSVC-only here); same order, row by row
      char tmp[sizeof l[8]];
      std::copy(std::begin(l[8]), std::end(l[8]), tmp);
      for (int r = 8; r > 5; --r) std::copy(std::begin(l[r - 1]), std::end(l[r - 1]), l[r]);
      std::copy(std::begin(tmp), std::end(tmp), l[5]);
    }  // order: ..., look, chunk, health/XYZ, renderer, release
    m.debugLines.assign(std::begin(l), std::end(l));
  } else if (settings_.fpsCounter && !cfg_.capture && !pinned) {  // F56-4 (Proposed by JD; co-implemented by JD and Cortana): perf readout
    char l[200], gpu[24] = "n/a";
    if (gpuMs_ >= 0.f) std::snprintf(gpu, sizeof gpu, "%.2f ms", gpuMs_);
    std::snprintf(l, sizeof l, "FPS %.0f  avg %.2f ms  p99 %.2f ms  update %.2f ms  GPU %s", fps_, frameAvgMs_, p99Ms_, updateMs_, gpu);
    m.debugLines = {std::string(l)};
  }
  {  // 2026-10-31 (JD): tick stability readout (LL_HUD_TICKS=0 hides it)
    // 2026-11-05 (JD #6): when tick-frozen / paused, show "TPS paused" in neutral colour (not red 0).
    static const bool kHudTicks = [] { const char* e = std::getenv("LL_HUD_TICKS"); return !(e && e[0] == '0'); }();
    const bool tickFrozen = pauseState_ != PauseState::Playing;
    tickMeter_.setFrozen(tickFrozen);
    if (!tickFrozen) {
      const SimStats& ts = sim_->stats();
      tickMeter_.sample(ts.worldTick, ts.droppedTicks, ts.targetTps, std::chrono::steady_clock::now());
    }
    if (kHudTicks && !pinned && !cfg_.capture) { m.tickLine = tickMeter_.line(); m.tickLevel = int(tickMeter_.level()); }
    {  // PlayM4 (JD t4701u): Trace-in-time chip status on the tick line (messages other than "running" fade after 6 s)
      auto& lh = lithoHud();
      static uint32_t seenSeq = 0; static auto seenAt = std::chrono::steady_clock::now();
      if (lh.seq.load() != seenSeq) { seenSeq = lh.seq.load(); seenAt = std::chrono::steady_clock::now(); }
      const int ls = lh.state.load();
      const bool fresh = std::chrono::steady_clock::now() - seenAt < std::chrono::seconds(6);
      char lb[220] = "";
      if (ls == 1) {
        const float us = lh.runUs.load(); const unsigned stp = lh.steps.load();
        std::snprintf(lb, sizeof lb, "LITHO chip ON  %u nodes  compiled %.1f ms  x%u/tick  chip tick %llu  %.0f us/run (~%.0fk t/s cap)  [Num8 stop, Num9 speed]",
                      lh.nodes.load(), double(lh.compileMs.load()), stp, (unsigned long long)lh.ticks.load(), double(us),
                      us > 0.f ? double(stp) * 1e3 / double(us) : 0.0);
      } else if (fresh && ls == 2) std::snprintf(lb, sizeof lb, "LITHO chip dropped (block edit) - live sim");
      else if (fresh && ls == 3) std::snprintf(lb, sizeof lb, "LITHO nothing to compile within the box around you");
      else if (fresh && ls == 4) std::snprintf(lb, sizeof lb, "LITHO chip stopped - live sim");
      if (lb[0] && !pinned && !cfg_.capture) m.tickLine = m.tickLine.empty() ? std::string(lb) : m.tickLine + "   |   " + lb;
    }
  }
  if (postViewOpen_ || (typing_ && postTyping_)) {  // 2026-10-25 (JD): Post viewer overlay
    const SimStats& vs = sim_->stats();
    if (vs.hit && post::isPost(vs.hitBlock)) m.postViewLines = post::viewLines(vs.hitText, typing_ && postTyping_);
  }
  if (typing_ && postTyping_)
    m.bannerLines = {"POST - type up to 256 characters, Enter = new line, Backspace deletes, Esc ends", "> " + typed_ + "_"};
  else if (typing_)
    m.bannerLines = {"KEYBOARD CAPTURE - type A-Z 0-9 symbols (SIXBIT), Enter = 1, Backspace = 2, Esc ends", "> " + typed_ + "_"};
  m.controlLines = showControls_ ? controlsPanelLines(input_.map())
                                 : std::vector<std::string>{controlsToggleLabel(input_.map()) + "  show controls"};
  if (cfg_.capture && !showControls_) m.controlLines.clear();  // playtest 2026-10-16: no hint text
  return m;
}

std::string Game::perfLine(double fps, double frameAvgMs, double frameMaxMs) const {
  const SimStats& st = sim_->stats();
  char buf[320];
  std::snprintf(buf, sizeof buf,
                "perf fps=%.0f frame=%.2f/%.1fms tps=%.1f/%d tick=%.3f/%.3fms budget=%.2f%% dropped=%llu ticks/f=%u chunks=%zu meshes=%d/%d dim=%s",
                fps, frameAvgMs, frameMaxMs, st.tps, st.targetTps, st.tickMs, st.tickMaxMs, st.budgetPct,
                (unsigned long long)st.droppedTicks, st.ticksLastPass, chunkColumns_, meshesReceived_, totalSubs_,
                active_->id.c_str());
  return buf;
}

std::string Game::statusLine(double fps, double worstMs) const {
  const SimStats& st = sim_->stats();
  const std::string target = targetText();
  char buf[512];
  std::snprintf(buf, sizeof buf,
                "LithoKraft [%s] | [%d] %s | target: %s | FPS %.0f (worst %.1f ms) | TPS %.1f/%d tick %.3f ms, budget %.2f%% | EUV %.0f Hz%s | components %u | meshes %d/%d | %s | %s",
                active_->name.c_str(), hot_ + 1,
                itemDisplayName(player_.inventory.hotbar()[size_t(hot_)].itemId).c_str(), target.c_str(), fps, worstMs,
                st.tps, st.targetTps, st.tickMs, st.budgetPct, euvStats(st).achieved, euvStats(st).enabled ? "" : " (off)",
                st.active, meshesReceived_, totalSubs_, rendererInfo().c_str(), releaseInfo().c_str());
  return buf;
}

// ---------------------------------------------------------------- Deceitors
void Game::updateDeceitors(float dt) {
  ai::cAiWorld* w = aiHost_.world();
  if (!w) {
    // No cAiWorld (ai.enabled off / unbound): no population, no mirrors.
    for (auto& [mid, eid] : deceitorMirrors_) entities_.despawn(eid);
    deceitorMirrors_.clear();
    deceitorMirrorAnim_.clear();
    return;
  }
  // Reap mirrors whose mob left the registry (despawn / world rebuild).
  for (auto it = deceitorMirrors_.begin(); it != deceitorMirrors_.end();) {
    if (!deceitorDirector_.brain(it->first)) {
      entities_.despawn(it->second);
      deceitorMirrorAnim_.erase(it->first);
      it = deceitorMirrors_.erase(it);
    } else {
      ++it;
    }
  }
  // P2 (2026-10-14): the mirror GLIDES between cell centres over one
  // AI-tick period instead of teleporting each tick; legs that arrived via
  // a kJump / kGap step get a parabolic arc on top of the linear glide.
  // Presentation only: the cAiWorld cell remains the sim truth, mirror
  // state is never saved, and a leg longer than 5 blocks (respawn, surge
  // catch-up, world rebuild) snaps instead of gliding. Arc heights are
  // presentation constants (JD-review-pending): jump 0.25, gap 0.5.
  const double period = 1.0 / double(w->config().hz);
  for (const uint32_t mid : deceitorDirector_.mobIds()) {
    const ai::Mob* m = w->mob(mid);
    if (!m) continue;
    uint64_t eid = 0;
    const auto it = deceitorMirrors_.find(mid);
    if (it != deceitorMirrors_.end() && entities_.get(it->second)) eid = it->second;
    if (!eid) {
      eid = entities_.spawn(kEntityDeceitor, m->pos.x + 0.5, double(m->pos.y), m->pos.z + 0.5);
      if (litholattice::Entity* e = entities_.get(eid)) e->data = mid;
      deceitorMirrors_[mid] = eid;
      DeceitorMirrorAnim& a = deceitorMirrorAnim_[mid];
      a.cell = m->pos;
      a.fx = a.tx = m->pos.x + 0.5;
      a.fy = a.ty = double(m->pos.y);
      a.fz = a.tz = m->pos.z + 0.5;
      a.phase = 1.f;
      a.tag = ai::StepTag::kStepStart;
    }
    if (litholattice::Entity* e = entities_.get(eid)) {
      DeceitorMirrorAnim& a = deceitorMirrorAnim_[mid];
      if (!(m->pos == a.cell)) {
        // A new cell was reached since the last frame: start a leg from
        // wherever the mirror is currently drawn (never a visual pop).
        a.fx = e->x;
        a.fy = e->y;
        a.fz = e->z;
        a.tx = m->pos.x + 0.5;
        a.ty = double(m->pos.y);
        a.tz = m->pos.z + 0.5;
        a.tag = m->pathAt < m->pathSteps.size() ? m->pathSteps[m->pathAt] : ai::StepTag::kWalk;
        a.phase = 0.f;
        a.cell = m->pos;
        const double dx = a.tx - a.fx, dy = a.ty - a.fy, dz = a.tz - a.fz;
        if (dx * dx + dy * dy + dz * dz > 25.0) a.phase = 1.f;  // teleport: snap
      }
      if (a.phase < 1.f && period > 0.0)
        a.phase = std::min(1.f, a.phase + float(double(dt) / period));
      const double t = a.phase;
      double x = a.fx + (a.tx - a.fx) * t;
      double y = a.fy + (a.ty - a.fy) * t;
      double z = a.fz + (a.tz - a.fz) * t;
      if ((a.tag == ai::StepTag::kJump || a.tag == ai::StepTag::kGap) && t > 0.0 && t < 1.0)
        y += std::sin(3.141592653589793 * t) * (a.tag == ai::StepTag::kGap ? 0.5 : 0.25);
      e->x = x;
      e->y = y;
      e->z = z;
      e->vx = e->vy = e->vz = 0;
    }
  }
}

uint32_t Game::nearestDeceitorMob(double maxDist) {
  ai::cAiWorld* w = aiHost_.world();
  if (!w) return 0;
  uint32_t best = 0;
  double bestD = maxDist;
  for (const uint32_t mid : deceitorDirector_.mobIds()) {
    const ai::Mob* m = w->mob(mid);
    if (!m) continue;
    const double dx = m->pos.x + 0.5 - player_.pos.x, dz = m->pos.z + 0.5 - player_.pos.z;
    const double d = std::sqrt(dx * dx + dz * dz);
    if (d <= maxDist && (!best || d < bestD)) {
      best = mid;
      bestD = d;
    }
  }
  return best;
}

std::string Game::spawnDeceitorCmd() {
  ai::cAiWorld* w = aiHost_.world();
  if (!w) return "deceitor: no AiWorld - set ai.enabled 1 first (Deceitors live in the AiWorld)";
  const ai::Cell at{int(std::floor(player_.pos.x + std::cos(player_.yaw) * 3.0)),
                    int(std::floor(player_.pos.y)),
                    int(std::floor(player_.pos.z + std::sin(player_.yaw) * 3.0))};
  const auto id = deceitorDirector_.spawn(*w, at);
  if (!id) return "deceitor: AiWorld mob cap reached";
  return "deceitor: spawned mob " + std::to_string(*id) +
         " - an upright biped, smiling. Indistinguishable from a Receiptor; "
         "try 'deceitor interact', and mind your back";
}

std::string Game::deceitorInteractCmd() {
  ai::cAiWorld* w = aiHost_.world();
  if (!w) return "deceitor: no AiWorld - set ai.enabled 1 first";
  const uint32_t id = nearestDeceitorMob(16.0);
  if (!id) return "deceitor: none within reach";
  deceitorDirector_.playerInteract(id, w->stats().ticks);
  return "deceitor: you interact with mob " + std::to_string(id) +
         " - no trade interface opens. That silence is the tell. "
         "Throw it an item, quickly";
}

std::string Game::deceitorAttackCmd() {
  ai::cAiWorld* w = aiHost_.world();
  if (!w) return "deceitor: no AiWorld - set ai.enabled 1 first";
  const uint32_t id = nearestDeceitorMob(16.0);
  if (!id) return "deceitor: none within reach";
  deceitorDirector_.playerAttacked(*w, id);
  return "deceitor: you strike mob " + std::to_string(id) + " - the pack turns as one";
}

void Game::uploadEntities() {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  using namespace litholattice;
  std::vector<PackedVertex> verts;
  // Optimization (audit App.A/game-2): skip the 64-block querySphere scan and
  // the billboard build when there is nothing to draw. The (empty) upload
  // below still runs so a cleared scene never keeps stale billboards.
  if (entities_.size() == 0) { renderer_.uploadEntityMesh(verts); return; }
  // GAME-H1 (2026-10-15): every renderer decodes PackedVertex xyz as 1/16
  // block relative to a mesh origin (the chunk encoding). Entities were
  // written in whole blocks, absolute, so they drew at 1/16 of their
  // position and sub-block corners collapsed (negative coords were UB).
  // Encode (pos - origin) * 16 against a 16-aligned origin 128 blocks below
  // the player in each axis: the 64-block query sphere then spans 1/16-block
  // values in roughly [1024, 3100], well inside uint16, clamped for safety.
  const float eox = std::floor(player_.pos.x / 16.f) * 16.f - 128.f;
  const float eoy = std::floor(player_.pos.y / 16.f) * 16.f - 128.f;
  const float eoz = std::floor(player_.pos.z / 16.f) * 16.f - 128.f;
  renderer_.setEntityOrigin(eox, eoy, eoz);
  const auto enc = [](float v, float o) -> uint16_t {
    const float q = std::round((v - o) * 16.f);
    return uint16_t(q < 0.f ? 0.f : q > 65535.f ? 65535.f : q);
  };
  // Billboard basis from the camera yaw (items face the player).
  const float cy = std::cos(player_.yaw), sy = std::sin(player_.yaw);
  const float rx = cy, rz = -sy;  // camera right in XZ
  const float s = 0.15f;  // half-size (0.3 block item)
  for (litholattice::Entity* e : entities_.querySphere(player_.pos.x, player_.pos.y, player_.pos.z, 64.0, kEntityItem)) {
    const float cx = float(e->x), cyy = float(e->y), cz = float(e->z);
    // Two triangles (6 verts) forming a camera-facing quad.
    const float corners[4][2] = {{-1,-1}, {1,-1}, {1,1}, {-1,1}};
    PackedVertex q[4];
    for (int i = 0; i < 4; ++i) {
      const float ox = corners[i][0] * s, oy = corners[i][1] * s;
      q[i] = {};
      q[i].x = enc(cx + rx * ox, eox);  // GAME-H1: 1/16 block from the batch origin
      q[i].y = enc(cyy + oy, eoy);
      q[i].z = enc(cz + rz * ox, eoz);
      q[i].mode = kShadeTex;
      // TODO: set q[i].layer from the block's texture
    }
    // Triangles: 0,1,2 and 0,2,3
    verts.push_back(q[0]); verts.push_back(q[1]); verts.push_back(q[2]);
    verts.push_back(q[0]); verts.push_back(q[2]); verts.push_back(q[3]);
  }
  // Deceitor bodies: upright bipeds (1.8 blocks) from stacked billboards
  // through this same entity-mesh path â€” the placeholder presentation
  // until a real mob render path exists (cAiWorld mobs had NO renderer
  // before this). Presentation is IDENTICAL to a Receiptor's while
  // non-hostile: trace-trunk legs, ceramic torso and head. The head
  // material swaps with the face state the brain publishes â€” a lit
  // lamp for the Smile, a red torch for the Scowl â€” the only render
  // tell, and it appears only once hostility is already open.
  {
    const auto mats = resources_.materials();
    auto layerOf = [&](BlockId b, FaceSlot f, bool active) -> uint16_t {  // Cortana 2026-10-15 1628: CA-03 [GAME]
      return mats ? uint16_t(mats->face(b, f, active).layer) : uint16_t(0);
    };
    const uint16_t legsLayer = layerOf(kTraceTrunk, FaceSlot::Front, false);
    const uint16_t torsoLayer = layerOf(kCeramic, FaceSlot::Front, false);
    const uint16_t headNeutral = layerOf(kCeramic, FaceSlot::Top, false);
    const uint16_t headSmile = layerOf(kLamp, FaceSlot::Top, true);
    const uint16_t headScowl = layerOf(kRedTorch, FaceSlot::Top, true);
    struct Part {
      float y0, y1, halfW;
      uint16_t layer;
    };
    for (litholattice::Entity* e :
         entities_.querySphere(player_.pos.x, player_.pos.y, player_.pos.z, 64.0, kEntityDeceitor)) {
      const DeceitorFace face = deceitorDirector_.faceOf(e->data);
      const uint16_t headLayer = face == DeceitorFace::Smile    ? headSmile
                                : face == DeceitorFace::Scowl ? headScowl
                                                              : headNeutral;
      const Part parts[3] = {{0.f, 0.7f, 0.25f, legsLayer},
                             {0.7f, 1.35f, 0.31f, torsoLayer},
                             {1.35f, 1.8f, 0.25f, headLayer}};
      const float cx = float(e->x), cz = float(e->z), base = float(e->y);
      for (const Part& p : parts) {
        const float corners[4][2] = {{-1, 0}, {1, 0}, {1, 1}, {-1, 1}};
        PackedVertex q[4];
        for (int i = 0; i < 4; ++i) {
          const float ox = corners[i][0] * p.halfW;
          const float oy = p.y0 + corners[i][1] * (p.y1 - p.y0);
          q[i] = {};
          q[i].x = enc(cx + rx * ox, eox);  // GAME-H1: 1/16 block from the batch origin
          q[i].y = enc(base + oy, eoy);
          q[i].z = enc(cz + rz * ox, eoz);
          q[i].u = corners[i][0] < 0 ? 0 : 255;
          q[i].v = corners[i][1] < 1 ? 255 : 0;
          q[i].setLayer(p.layer);
          q[i].mode = kShadeTex;
        }
        verts.push_back(q[0]); verts.push_back(q[1]); verts.push_back(q[2]);
        verts.push_back(q[0]); verts.push_back(q[2]); verts.push_back(q[3]);
      }
    }
  }
  renderer_.uploadEntityMesh(verts);
}

}  // namespace lithokraft
