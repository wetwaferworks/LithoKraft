#include <string_view>
#include "litholattice/script/script_generic.h"
#include <thread>
#include <unordered_set>  // Cortana 2026-10-15 1455
// Cortana 2026-10-15 0825/0935/1020: CHUNK-STALL, ALT-TAB, GAME-H1
#include "lithokraft/game/game.h"
#include "litholattice/core/threeway.h"  // LL_THREEWAY (Proposed by JD; co-implemented by JD and Cortana)
#include "litholattice/mods/mod_interface.h"  // kPerm* for the script sandbox
#include "litholattice/mods/mod_permissions.h"  // permissionBitForName (mcp grant/revoke)
#include <algorithm>
#include <chrono>  // Cortana 2026-10-15 2215: hold-to-repeat timers
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include "controls_panel.h"
#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
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
#include "lithokraft/game/dimensions.h"
#include "ui_input_bridge.h"
#include "litholattice/ui/text_edit.h"
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
static int llP2PerfOff() { static const int v = [] { const char* e = std::getenv("LL_P2_PERF_OFF"); return e ? int(std::strtol(e, nullptr, 0)) : 0; }(); return v; }  // Cortana 2026-10-16 1520: A/B perf switch (render/display only)

namespace lithokraft {

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
           [this] { quit_ = true; }, [this] { enterPlay(); }, [this] { enterMenu(); },
           [this] { ui_.openSettings(settings_); }, [this](const GameSettings& s) { onSettingsChanged(s); },
           [this](int i) { pickCarrySlot(i); }}),
      uiScripts_(scripts_, ui_.root()),
      renderer_(renderer), resources_(resources), sound_(audio), pool_(pool),
      registry_(createDimensions(resources.worldgen(cfg.seed))),
      playerAdapter_(player_),  // IPlayer integration: engine view of player_
      input_(input), audio_(audio),
      camera_(player_, GameCameraHooks{[this] { return snapshotMcpPlayer(); },
                                       [this](Vec3 o, Vec3 d, float m, float& hd) {
                                         return cameraRaycast(o, d, m, hd);
                                       },
                                       [this] { return locomotion_.eyeHeight(); }}),
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
  // the A5 wiring pass (blocks.cpp's density table — JD-DECISION balance
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
  debugConsole_->attach(ui_.root());
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
  // knobs because camera.chunk_radius is shared — the camera module's
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
      // freeze first (setPauseState), then unpauses — never the reverse.
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
        msg += " (requested " + std::to_string(n) + " — the sim did not advance the full count)";
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
          // B-L6 (P3-2): basesInRect returns CANDIDATE bases — place()'s
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
  // cCamera command (view-only, not a cheat — same standing as `map`):
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
      const Vec3 eye = playerEyePos(player_, locomotion_.eyeHeight());
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
      // the machine owns — never to a stale snapshot of it.
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
  renderer_.setBloom(resolvedBloom(settings_));          // Cortana 2026-10-16: render.bloom
  renderer_.setAo(settings_.ao);                         // Cortana 2026-10-16: render.ao
  renderer_.setShimmer(settings_.shimmer);               // Cortana 2026-10-16: render.shimmer
  renderer_.setTexRes(settings_.texRes);                 // Cortana 2026-10-16: render.tex_res
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
  submitMesh_ = [this](std::shared_ptr<MeshInput> in) {
    pool_.submit([this, in, mats = resources_.materials(), binary = useBinaryMesher_] {
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
      meshOut_.push(meshSubChunk(*in, ctx));
    });
  };
  std::optional<PlayerMeta> savedPlayer;
  if (!cfg_.saveDir.empty()) {
    save_ = std::make_unique<cWorldSave>(cfg_.saveDir, cfg_.materials);
    save_->setSeed(cfg_.seed);  // SV-H2: cfg.seed already carries the persisted-seed override (main.cpp)
    save_->setWorldArea({simAreaShapeOf(cfg_), streamAreaShapeOf(cfg_), cfg_.tickRadius});  // Fix run 3: level.json v3 (main.cpp applies the saved area)
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
      // v1–v3 migration: the old raw-BlockId hotbar becomes real stacks in
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
  // v1 saves carry none — all defaults), then mirror the restored player
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
    // like any other. The kit's 8th block (Switch) rides in main slot 0 —
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
  if ((path == "stream" || path == "soak") && active_ && active_->generator)
    stream_ = std::make_unique<cBenchStreamer>(renderer_, pool_, active_->generator, cfg_.worldChunks, active_->belowWorldFill, std::max(1, cfg_.streamRadius));
}

void Game::setBenchCamera(float t) {
  if (!benchSet_) { benchCenter_ = player_.pos; benchSet_ = true; }
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
    return;
  }
  if (benchPath_ == "still") {
    static bool loggedPose = false;  // JD Vulkan push: log the held pose once (screenshot framing)
    if (!loggedPose) { loggedPose = true; logInfo("bench still pose: %.1f,%.1f,%.1f,%.3f,%.3f", player_.pos.x, player_.pos.y, player_.pos.z, player_.yaw, player_.pitch); }
    // Playtest 2026-10-16: hold the spawn pose (screenshots); LL_BENCH_POSE="x,y,z,yaw,pitch"
    // overrides it (e.g. a close view of the test world's Pattern ROM screen).
    if (const char* e = std::getenv("LL_BENCH_POSE")) {
      float v[5];
      if (std::sscanf(e, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5) {
        player_.pos = {v[0], v[1], v[2]}; player_.yaw = v[3]; player_.pitch = v[4];
      }
    }
    return;
  }
  const bool streaming = benchPath_ == "stream" || (benchPath_ == "soak" && (int(t / 60.f) & 1));
  if (benchPath_ == "soak") t = std::fmod(t, 60.f);
  if (streaming) {  // start 2 chunks inside the fixed world's +x edge, fly out along +x
    const float startX = float(cfg_.worldChunks * 16 - 32), z = float(cfg_.worldChunks * 8);
    player_.pos = {startX + 32.f * t, benchCenter_.y + 30.f, z};
    player_.yaw = 0.f; player_.pitch = -0.3f;
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
    return;
  }
  const float a = t * 0.25f;
  player_.pos = {benchCenter_.x + 24.f * std::cos(a), benchCenter_.y + 12.f, benchCenter_.z + 24.f * std::sin(a)};
  player_.yaw = a + 1.5708f; player_.pitch = -0.25f;
}

Game::~Game() {
  stream_.reset();
  streamer_.reset();  // waits out in-flight generation jobs (they may touch save_)
  saveWorld("exit", true);
  // Persist archived cvars (the settings bridge) to config.lth.
  if (!cfg_.settingsPath.empty()) {
    const std::filesystem::path cfg(cfg_.settingsPath);
    if (cfg.has_parent_path())
      consoleRegistry_.saveArchived((cfg.parent_path() / "config.lth").string());
  }
  aiHost_.unbindWorld();  // cWalkCache references active_->chunks; drop before the registry dies (B6)
  sim_.reset();
  pool_.waitIdle();  // mesh jobs reference meshOut_
}

void Game::loadActive() {
  const auto t0 = Clock::now();
  if (cfg_.benchScene != "default") {  // bench scenes: load and tick the whole scene
    cfg_.worldChunks = std::max(cfg_.worldChunks, cfg_.sceneSize / 16 + 12);
    cfg_.tickRadius = std::max(cfg_.tickRadius, cfg_.sceneSize / 32 + 4);
  }
  Spawn sp = active_->load(pool_, cfg_.worldChunks);
  std::vector<PatternRomScreen> roms;  // playtest 2026-10-16: handed to the new sim below
  if (cfg_.benchScene == "testworld") {
    // Playtest 2026-10-16 (JD): flat pad, every registry block in labeled rows, Pattern ROM screen.
    const int sx = int(std::floor(sp.x)), sz = int(std::floor(sp.z));
    const int floorY = std::max(int(std::floor(sp.y)) - 1, active_->chunks.surfaceY(sx, sz));
    const TestWorldLayout tw = buildTestWorld(active_->chunks, sx, floorY, sz, cfg_.sceneSize >= 64 ? 64 : 32);
    sp.x = tw.spawnX; sp.z = tw.spawnZ; sp.y = float(tw.floorY + 1); sp.yaw = tw.spawnYaw; sp.pitch = -0.32f;
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
    logInfo("test world: crystal leaf grove (8 trees, red/blue-dominant) x %d..%d, y %d..%d, z %d..%d", tw.leafDemoX - 28, tw.leafDemoX,
            tw.floorY + 1, tw.floorY + 7, tw.leafDemoZ - 1, tw.leafDemoZ + 10);  // 2026-10-16 JD leaf pass
    for (BlockId b : tw.missing) logError("test world: could not place '%s'", blockRegistry().def(b).name.data());
    roms.push_back(tw.screen);
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
  } else if (cfg_.benchScene == "circuits" || cfg_.benchScene == "display") {
    const int cx = int(sp.x), cz = int(sp.z) + 8, n = cfg_.sceneSize;
    const int sy = active_->chunks.surfaceY(cx, cz) + 1;
    const int placed = cfg_.benchScene == "display" ? buildDisplayScene(active_->chunks, cx - n / 2, sy, cz, n)
                                                     : buildCircuitScene(active_->chunks, cx - n / 2, sy, cz, n);
    logInfo("bench scene: %s, size %d, %d blocks", cfg_.benchScene.c_str(), n, placed);
  }
  std::vector<uint8_t> signalState;
  const int restored = save_ ? save_->loadDimension(active_->id, active_->chunks, signalState) : 0;
  if (restored < 0)
    logError("save: '%s' load REFUSED (fail closed, see above); the world on disk is untouched and saves will refuse until the name table is repaired",
             active_->id.c_str());
  ++epoch_; totalSubs_ = 0; meshesReceived_ = 0;
  chunkColumns_ = active_->chunks.columns().size();  // read here: the sim thread owns the map later
  {  // fix 3b (JD t2279u): mesh the loaded columns nearest-first around the spawn chunk, not in
     // unordered_map order, so the world fills in from the player outwards (same rule as the streamer).
    const int scx = int(std::floor(sp.x / 16)), scz = int(std::floor(sp.z / 16));
    std::vector<std::pair<int, uint64_t>> order;
    order.reserve(active_->chunks.columns().size());
    for (const auto& [k, col] : active_->chunks.columns())
      order.emplace_back(streamRequestPriority(col->cx - scx, col->cz - scz, cfg_.diamondStreamArea), k);
    std::sort(order.begin(), order.end());
    for (const auto& [pri, k] : order) {
      (void)pri;
      const auto& col = active_->chunks.columns().at(k);
      for (int sy = 0; sy < kSubChunks; ++sy)
        if (col->subs[sy]) { ++totalSubs_; submitMesh_(snapshotForMeshing(active_->chunks, blockRegistry(), col->cx, sy, col->cz, epoch_, ++initSeq_, active_->belowWorldFill)); }
    }
  }
  player_.pos = {sp.x, sp.y, sp.z}; player_.yaw = sp.yaw; player_.pitch = sp.pitch;
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
  if (sc.timingHandshake) logInfo("timing handshake: ON (phases/cycle %d; applies at this world load)", sc.handshakePhasesPerCycle);
  placeRefunds_.clear();  // receipts belong to the sim that is replaced here
  clockSynced_ = false;   // 2026-10-16: a new sim's worldTick must not be diffed against the old one's
  auto worldSim = std::make_unique<cSignalWorldSim>();
  for (PatternRomScreen& r : roms) worldSim->addPatternRom(std::move(r));  // before the sim thread owns it
  sim_ = std::make_unique<cSimThread>(active_->chunks, blockRegistry(), std::move(worldSim), std::move(sc),
                                     submitMesh_);
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
  projectiles_.clear();               // projectiles belong to the old dimension
  projectiles_.clearRequestedChunks();  // the new streamer has seen no requests
  if (active_->generator && cfg_.streaming) {  // JD 2026-10-16: streaming toggle
    std::unordered_set<uint64_t> initial;
    initial.reserve(active_->chunks.columns().size());
    for (const auto& [k, col] : active_->chunks.columns()) initial.insert(k);
    streamer_ = std::make_unique<cChunkStreamer>(pool_, active_->generator, save_.get(), active_->id,
                                                effectiveStreamRadius(cfg_) /* fix3: sim area always inside stream */, initial);
    streamer_->setDiamond(cfg_.diamondStreamArea);  // diamond stream footprint (JD 2026-10-23)
  }
  // cAiWorld host (B6): bind to THIS dimension's live map + registry +
  // pool while the game thread still owns the map (before sim start).
  // Construction of the cAiWorld itself waits for the ai.enabled gate.
  aiHost_.bindWorld(active_->chunks, blockRegistry(), &pool_,
                    {int(std::floor(sp.x)), int(std::floor(sp.y)), int(std::floor(sp.z))});
  sim_->start();  // from here the sim thread owns active_->chunks
  logInfo("dimension '%s' loaded: %zu columns, %d sub-chunks, %.1f ms", active_->id.c_str(),
          active_->chunks.columns().size(), totalSubs_, msBetween(t0, Clock::now()));
}

bool Game::switchDimension(const std::string& id) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  Dimension* next = registry_.find(id);
  if (!next || next == active_) return false;
  saveWorld("dimension switch", false);
  // P0a carry (P2b-2): a poisoned stop (handshake workers outstanding)
  // ABORTS the switch — destroying the cSimThread / unloading the old
  // dimension under a still-running worker is the C2 use-after-free.
  // The game stays in the current dimension; its sim thread is stopped
  // and the pause machine's state is untouched (still coherent).
  if (sim_ && !sim_->stop()) {
    logError("dimension switch ABORTED: sim stop failed (handshake workers outstanding); staying in '%s'",
             active_->id.c_str());
    return false;
  }
  streamer_.reset();            // its jobs reference the old dimension's generator/save id
  sim_.reset();                 // stop ticking; main thread owns the old chunks again
  aiHost_.unbindWorld();        // its cWalkCache references the old dimension's chunks (B6)
  sound_.stopAll();
  // Design B Stage 2: the store clear is staged on the ring path (in ring
  // order, behind the old dimension's uploads it wipes) and immediate on
  // every other path. Spilled old-dimension uploads die here — the clear
  // covers the store, and nothing stale may be pushed behind it.
  ringSpillDroppedOnSwitch_ += ringSpill_.size();
  ringSpill_.clear();
  if (!renderer_.enqueueClearChunkMeshes()) spillRingUpload(2, {}, {});
  pendingOrder_.clear(); pendingMesh_.clear(); meshSeq_.clear();  // in-flight jobs are dropped by the epoch check
  mcpSenses_.clearMeshes();  // vision cache follows the GL renderer's clearChunkMeshes() above
  pbrPendingOrder_.clear(); pbrPendingMesh_.clear(); pbrMeshSeq_.clear();  // same for the PBR queue
  if (emissiveProvider_) emissiveProvider_->clear();  // new dimension: rescan
  logInfo("dimension switch: %s -> %s", active_->id.c_str(), next->id.c_str());
  active_->unload();
  active_ = next;
  loadActive();
  // Dimension switch must not silently unpause (audit MEDIUM): loadActive()
  // builds a fresh, unpaused cSimThread; the pause machine re-applies.
  applySimPause();
  return true;
}

bool Game::saveWorld(const char* reason, bool wait) {
  if (!save_ || !sim_ || !active_) return false;
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
  const size_t n = save_->saveDimension(active_->id, active_->chunks, static_cast<cSignalWorldSim&>(sim_->simulation()).signal(),
                                        wait, &dirty);
  sim_->start();
  applySimPause();  // restore the machine's pause computation on the restarted sim
  const double pauseMs = msBetween(t0, Clock::now());
  // SV-H1 + C-4: a refused save (fail-closed name table) or a failed flush
  // is a FAILED save — report it as one, claim nothing is on disk, and do
  // not touch player.lat either (the world is not overwritten piecemeal).
  if (!save_->lastSaveOk()) {
    logError("save (%s) FAILED: '%s': %s", reason, active_->id.c_str(), save_->lastSaveError().c_str());
    return false;
  }
  PlayerMeta m{player_.pos, player_.yaw, player_.pitch, active_->id, hot_};
  // P2b-1: the inventory is the store that persists (v4 inventory block).
  // The legacy hotbar field is DERIVED from its hotbar stacks — the block
  // each stack places (kAir for empty slots and non-block stacks) — and
  // writePlayer stores those as string ids (Appendix C M2).
  m.hotbar.reserve(kHotbarSize);
  for (const ItemStack& s : player_.inventory.hotbar()) m.hotbar.push_back(placedBlockForItem(s.itemId));
  m.inventory = player_.inventory;
  m.hasInventory = true;
  m.stats = playerAdapter_.statsForSave();    // IPlayer integration: stat bases by name
  m.vitals = playerAdapter_.vitalsForSave();  // IPlayer integration: vital values by name (v3 player.lat)
  m.prayerDay = prayer_.prayerDay;  // Cortana 2026-10-16: v7 mealtime prayer block
  m.prayerWindows = prayer_.windows;
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

// Chunk streaming (see chunk_streamer.h): request the columns in the load
// radius around the player, then insert the finished ones. The sim thread owns
// active_->chunks while it runs, so insertion takes the sim between ticks
// (holdBetweenTicks: the blockMutex_ hand-off); new columns are meshed through
// the same submitMesh_ path as the initial load. Build 80 (cycle-budget
// fairness): when the hold is refused (handshake frozen, or a worker still
// outstanding after a failed stop), the batch is DEFERRED — it stays queued
// and lands on a later pump. The pump never stop()/start()s the sim: that
// fallback parked the frame thread inside thread joins and worker respawns
// (the build-79 S3 starvation seesaw on device — present and sim starving
// each other by turns). Freeze recovery stays with the pause state machine
// (entering Playing clears the latch), exactly as before.
void Game::pumpStreaming() {
  if (!streamer_ || !sim_ || !active_) return;
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  litholattice::setFoliageFocus(pcx, pcz);  // 2026-10-18 (Crew Canopy, F-B): sparse-leaf distance origin
  streamer_->update(pcx, pcz);
  // CAM-2 (P2c): non-player cameras (third/follow/free view camera, MCP
  // pilot cameras) request columns around their own centres within their
  // chunk-radius budget — AFTER the player's update, so they only spend
  // in-flight capacity the player did not use (player priority, same cap).
  for (const auto& req : camera_.chunkLoadRequests()) streamer_->updateAround(req.cx, req.cz, req.radiusChunks);
  if (!streamer_->hasReady()) { evictFarColumns(); return; }
  // P0a carry (P2b-2): the ready queue is drained only under the
  // between-ticks hold — never under a still-running worker, and (build
  // 80) never under a stopped sim either: a refused hold defers the whole
  // batch with the columns still queued, exactly the outcome the old
  // poisoned-stop abort produced, minus the stop()/start() the frame
  // thread used to pay for it.
  // 2026-10-15: hold the sim between ticks (blockMutex_) instead of a full
  // stop()/start() (thread join + handshake worker join/respawn per pump).
  // Build 80: the stop()/start() fallback is REMOVED from this path (see
  // the header comment above); refusal now defers.
  std::unique_lock<std::recursive_mutex> simHold;
  if (!sim_->holdBetweenTicks(simHold)) {
    static int deferLog = 0;  // game thread only; rate-limited report
    if ((deferLog++ % 256) == 0)
      logWarn("stream: insertion deferred (sim hold refused: %s); batch stays queued",
              sim_->handshakeFrozen() ? "timing handshake frozen" : "handshake workers outstanding");
    return;
  }
  // B-M2 (P3-2): bounded insertion batch. Up to kMaxInFlight (32) columns
  // could finish between pumps; inserting all of them in this one
  // sim-stopped window (insert + scanColumn + up to 48 mesh snapshots
  // each) made the stop window — and the frame hitch — proportional to
  // the burst. Insert at most kStreamInsertBudget per pump instead; the
  // rest stay staged in the streamer and land over the next pumps, each
  // fully generated/loaded before handoff (snapshot-then-insert, as
  // before). 4/pump drains a full 32-column burst in 8 frames.
  constexpr size_t kStreamInsertBudget = 4;
  std::vector<StreamedColumn> fresh = streamer_->takeReadyUpTo(kStreamInsertBudget);
  if (fresh.empty()) return;  // hold released by the lock's destructor
  cSignalSim& signal = static_cast<cSignalWorldSim&>(sim_->simulation()).signal();
  size_t added = 0;
  for (auto& sc : fresh) {
    std::unique_ptr<ChunkColumn> col = std::move(sc.column);
    if (!col || active_->chunks.column(col->cx, col->cz)) continue;
    const int cx = col->cx, cz = col->cz;
    const bool fromDisk = sc.fromDisk;
    active_->chunks.insert(std::move(col));
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
      // dimension load — index its cells and import its SGNL payload now,
      // while the sim is stopped, or its circuits come back dead and their
      // component memory is lost (and a later save would overwrite the
      // orphaned SGNL state with the sim's empty export). Fresh-generated
      // columns need no scan: worldgen places only SimRole::None blocks
      // (verified against the procgen/game worldgen paths), so there is
      // nothing to index and no SGNL payload exists for them.
      signal.scanColumn(active_->chunks, cx, cz);
      if (!sc.signal.empty() && !signal.importState(sc.signal))
        logError("save: streamed column %d,%d signal state unreadable (components restart idle)", cx, cz);
    }
    const ChunkColumn* c = active_->chunks.column(cx, cz);
    for (int sy = 0; sy < kSubChunks; ++sy)
      if (c->subs[sy]) {
        ++totalSubs_;
        submitMesh_(snapshotForMeshing(active_->chunks, blockRegistry(), cx, sy, cz, epoch_, ++initSeq_,
                                       active_->belowWorldFill));
      }
  }
  chunkColumns_ = active_->chunks.columns().size();
  if (simHold.owns_lock()) simHold.unlock();  // release before evictFarColumns (it takes its own hold)
  if (added) logInfo("stream: +%zu columns around chunk (%d, %d); %zu loaded", added, pcx, pcz, chunkColumns_);
  evictFarColumns();
}

// Column eviction policy (A2 / B-M1). JD-DECISION parameters (brief C6:
// the p3-2 design proposes, JD locks): the hysteresis band beyond the
// stream radius and the per-pump eviction budget (mirroring the B-M2
// insertion budget of 4). Named constants, not locks.
static constexpr int kEvictHysteresisChunks = 2;
static constexpr size_t kEvictBudgetPerPump = 4;

void Game::retireColumnMeshes(int cx, int cz) {
  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana).
  // Design B Stage 2: on the ring path the tombstones below are staged in
  // the upload ring like every other upload — this function then touches
  // no renderer state and takes no guard, so an eviction pump never waits
  // on the in-flight render. Other paths keep the guard exactly as before.
  std::unique_ptr<litholattice::threeway::RendererGuard> rendererGuard;
  if (!renderer_.meshUploadsQueued()) rendererGuard = std::make_unique<litholattice::threeway::RendererGuard>();
  for (int sy = 0; sy < kSubChunks; ++sy) {
    const int64_t key = packPos(cx, sy, cz);
    const bool hadClassic = meshSeq_.count(key) != 0 || pendingMesh_.count(key) != 0;
    const bool hadPbr = pbrMeshSeq_.count(key) != 0 || pbrPendingMesh_.count(key) != 0;
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
    if (hadClassic) {
      ChunkMesh empty;
      empty.cx = cx; empty.sy = sy; empty.cz = cz; empty.epoch = epoch_;
      mcpSenses_.noteMesh(empty);      // vision cache erases on empty verts
      // Design B Stage 2: empty verts = remove (renderer contract); on the
      // ring path the tombstone is staged (must-push) or spilled — never dropped.
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
  const int pcx = int(std::floor(player_.pos.x / 16)), pcz = int(std::floor(player_.pos.z / 16));
  std::vector<uint64_t> residentKeys;
  residentKeys.reserve(active_->chunks.columns().size());
  for (const auto& [k, col] : active_->chunks.columns()) residentKeys.push_back(k);
  const std::vector<uint64_t> candidates = selectEvictionCandidates(residentKeys, pcx, pcz, effectiveStreamRadius(cfg_) /* fix3: sim area always inside stream */,
                                                                    kEvictHysteresisChunks, kEvictBudgetPerPump, cfg_.diamondStreamArea);
  if (candidates.empty()) return;
  // Same hold discipline as insertion (P0a + build 80): nothing is
  // evicted under a running sim, and a refused hold defers the whole
  // batch — columns stay resident and are reconsidered on a later pump.
  std::unique_lock<std::recursive_mutex> simHold;  // 2026-10-15: see the insertion pump
  if (!sim_->holdBetweenTicks(simHold)) {
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
    // column stays resident — never evict on uncertain state.
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
      // saveColumns exports the column's SGNL state from the live sim
      // inside the call — strictly BEFORE the unscan below.
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
    active_->chunks.erase(cx, cz);     // free the voxel data
    streamer_->forgetColumn(cx, cz);   // re-streamable later
    ++evicted;
  }
  chunkColumns_ = active_->chunks.columns().size();
  if (simHold.owns_lock()) simHold.unlock();
  if (evicted) logInfo("stream: evicted %zu far columns around chunk (%d, %d); %zu loaded", evicted, pcx, pcz, chunkColumns_);
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
    // shared with the previous (empty) cell — the axis where they differ.
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
  if (input_.pressed(Action::ToggleDebug)) showDebug_ = !showDebug_ && !cfg_.capture;  // capture: F3 stays off
  if (input_.pressed(Action::Inventory)) setInventoryOpen(!inventoryOpen_);
  if (input_.pressed(Action::ReloadResources)) reloadResources();
  if (input_.pressed(Action::SaveWorld)) saveWorld("F5", false);
  if (save_ && cfg_.autosaveSeconds > 0 && secondsBetween(lastSave_, Clock::now()) > cfg_.autosaveSeconds) saveWorld("autosave", false);
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
  return b;
}

void Game::setMcpServer(litholattice::mcp::cMcpServer* server) {
  mcpServer_ = server;
  if (!server || mcpCommandsRegistered_) return;
  mcpCommandsRegistered_ = true;
  // `mcp grant <perm...>`, `mcp revoke <perm...>`, `mcp agent`.
  // Cheat-flagged ON PURPOSE: granting an agent permissions is an
  // authority change, and the console's cheat gate is exactly "full-
  // authority local caller only" — a restricted caller (an MCP agent via
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
  std::lock_guard<std::mutex> lk(mcpStateMutex_);
  return mcpPlayerState_;
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
    fly = !locomotion_.flying();
  else if (arg == "on")
    fly = true;
  else if (arg == "off")
    fly = false;
  else
    return "fly: usage: fly [on|off]";
  locomotion_.setMode(fly ? LocomotionMode::Fly : LocomotionMode::Walk);
  player_.flying = fly;  // keep the cPlayer flag a faithful mode mirror
  logInfo("fly: locomotion mode -> %s", fly ? "fly" : "walk");
  return std::string("fly: ") + (fly ? "on (locomotion mode: fly)" : "off (locomotion mode: walk)");
}

void Game::handleHotbar() {
  for (int i = 0; i < kHotbarSize; ++i)
    if (input_.pressed(hotbarAction(i))) hot_ = i;
  // In third/follow the wheel zooms the camera instead of cycling slots.
  const int steps = camera_.consumesWheel()
                        ? 0
                        : int(input_.pressed(Action::HotbarNext)) - int(input_.pressed(Action::HotbarPrev));
  hot_ = ((hot_ + steps) % kHotbarSize + kHotbarSize) % kHotbarSize;
  if (input_.pressed(Action::Drop))
    player_.inventory.hotbar()[size_t(hot_)].clear();  // no item entities for held stacks yet: dropping discards the stack
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
// it is up), resuming lands in Playing — and entering Playing from ANY
// frozen state clears a latched handshake freeze first, so Esc-resume
// after tick_freeze can no longer leave the authority latch armed behind
// a game that believes it is running (the CH-H7 re-freeze loop).

/// The single sim_->setPaused() computation. Every other site that used
/// to write the sim flag now routes through here (or through
/// setPauseState, which ends here).
void Game::applySimPause() {
  paused_ = pauseState_ != PauseState::Playing;
  if (sim_) sim_->setPaused(paused_ && cfg_.pauseSim && !benchSet_);
}

void Game::setPauseState(PauseState s) {
  if (s == pauseState_) { applySimPause(); return; }
  const PauseState old = pauseState_;
  pauseState_ = s;
  if (s == PauseState::Playing && sim_ && sim_->handshakeFrozen()) {
    // Handshake watchdog recovery (hs-design §4.4): clear the freeze
    // latch explicitly BEFORE unpausing — a plain unpause must not
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
  if (!focused && menuState_ == MenuState::Playing && !benchSet_ && !inventoryOpen_ && !editorOpen() &&
      pauseState_ == PauseState::Playing) {
    setPaused(true);
    pausedByFocus_ = true;
    captured_ = false;  // Cortana 2026-10-15 1213: the OS freed the cursor; the resume click recaptures  // a click in the world (not on the dialog) resumes
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
}

// Play: unpause and hand control to the player instantly (no reload).
void Game::enterPlay() {
  if (menuState_ == MenuState::Playing) return;
  menuState_ = MenuState::Playing;
  ui_.closeMenu();
  // Restore the pre-dolly player transform (see enterMenu): the cinematic
  // overwrites player_.pos every menu frame, which used to defeat save/restore.
  if (menuSnapshotValid_) {
    player_.pos = menuReturnPos_; player_.yaw = menuReturnYaw_; player_.pitch = menuReturnPitch_;
  }
  setPaused(false);
  logInfo("menu: play - creative/testing");
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
  if (s.emissiveLight != old.emissiveLight) { renderer_.setEmissiveLight(s.emissiveLight); logInfo("settings: render.emissive_light -> %s", s.emissiveLight ? "on" : "off"); }
  if (s.mipmaps != old.mipmaps) { renderer_.setMipmaps(s.mipmaps); logInfo("settings: render.mipmaps -> %s", s.mipmaps ? "on" : "off"); }
  if (s.ao != old.ao) { renderer_.setAo(s.ao); logInfo("settings: render.ao -> %s", s.ao ? "on" : "off"); }
  if (s.shimmer != old.shimmer) { renderer_.setShimmer(s.shimmer); logInfo("settings: render.shimmer -> %s", s.shimmer ? "on" : "off"); }
  if (s.texRes != old.texRes) { renderer_.setTexRes(s.texRes); logInfo("settings: render.tex_res -> %d (applies on the next texture load / F9)", s.texRes); }
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
  } else {
    input_.pop(InputContext::Inventory);
    ui_.closeInventory();
    captured_ = true;  // closing returns to play
  }
}

// Block-palette click (creative give): a FULL stack of the block into the
// inventory hotbar — the selected slot (shift: the first free slot, else
// the selected slot). The palette writes the same inventory the HUD reads
// and player.lat persists (P2b-1); there is no second store to write to.
// Playtest 2026-10-16: the inventory dialog's carry grid; swap carry slot i (allSlots order,
// 0..41) with the selected hotbar slot, then refresh the grid.
void Game::pickCarrySlot(int i) {
  if (i < 0 || i >= kCarrySlots) return;
  auto slots = player_.inventory.allSlots();
  if (i != hot_) std::swap(*slots[size_t(i)], *slots[size_t(hot_)]);
  ui_.refreshCarry(carrySnapshot());
}

std::vector<ItemStack> Game::carrySnapshot() {
  std::vector<ItemStack> out;
  auto slots = player_.inventory.allSlots();
  for (int i = 0; i < kCarrySlots; ++i) out.push_back(*slots[size_t(i)]);
  return out;
}

void Game::pickInventoryBlock(BlockId b, bool shift) {
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

void Game::routeUiInput(bool& pointerUsed, bool& escapeUsed) {
  pointerUsed = escapeUsed = false;
  uiEvents_.clear();
  translateUiEvents(input_.frameEvents(), input_.devices(), uiEvents_);
  // With the mouse captured for play the cursor is hidden: only keys may reach the UI.
  // Cortana 2026-10-15 1213 (alt-tab click bug): paused_ added. A focus
  // pause left captured_ true, so every pointer event was skipped here: the
  // pause-menu buttons were dead and nothing saw the resume click.
  const bool pointerToUi = !captured_ || inventoryOpen_ || editorOpen() || paused_;
  freePointerDown_ = false;
  for (const ui::UiEvent& e : uiEvents_) {
    // Tilde toggles the debug console (never while a text field/modal owns the
    // keyboard); while open, the console pre-filters history/scroll/escape keys.
    if (debugConsole_ && debugConsole_->handleTildeKey(ui_.root(), e)) continue;
    if (debugConsole_ && debugConsole_->isOpen() && debugConsole_->handleKeyPreFilter(e)) continue;
    const bool pointer = e.type != ui::UiEvent::Type::KeyDown && e.type != ui::UiEvent::Type::Text;
    if (pointer && !pointerToUi) continue;
    const bool consumed = ui_.root().handleEvent(e);
    if (!consumed && e.type == ui::UiEvent::Type::PointerDown) freePointerDown_ = true;  // click in the world
    if (consumed && e.type == ui::UiEvent::Type::PointerDown) pointerUsed = true;
    if (consumed && e.type == ui::UiEvent::Type::KeyDown && e.key == ui::Key::Escape) escapeUsed = true;
  }
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
  const bool usable = inReach && (reg.has(st.hitBlock, kInteractive) || wireUse) && !sneaking;
  const bool altUse = inReach && input_.held(Action::Secondary) && roleOf(st.hitBlock) != SimRole::None;
  const int curTarget[3] = {st.hx, st.hy, st.hz};
  const bool newTarget = curTarget[0] != lastPlaceHit_[0] || curTarget[1] != lastPlaceHit_[1] ||
                         curTarget[2] != lastPlaceHit_[2];
  const bool placeNow = input_.pressed(Action::Place) ||
                        (input_.held(Action::Place) && nowT - lastPlaceT_ >= std::chrono::milliseconds(200));
  auto consumeOne = [&] {
    if (testWorld()) return;  // playtest 2026-10-16: infinite placement in the test world
    ItemStack& s = player_.inventory.hotbar()[size_t(hot_)];
    if (s.count > 1) --s.count;
    else s.clear();
  };
  if (input_.pressed(Action::Use) && altUse) {  // Alt + right click: the component's secondary setting
    EditCmd c{EditCmd::Kind::Command};
    c.command = SimCommand{kCmdAltUse, st.hx, st.hy, st.hz, 0, 0};
    sim_->edit(c);
  } else if (input_.pressed(Action::Use) && usable) {
    const Vec3 eye = playerEyePos(player_, locomotion_.eyeHeight());
    const Vec3 fw = player_.forward();
    if (reg.def(st.hitBlock).role == SimRole::Keyboard) {
      beginTyping(st.hx, st.hy, st.hz);
    } else if (roleOf(st.hitBlock) == SimRole::Repeator && eye.y > float(st.hy + 1) && fw.y < -1e-4f) {
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
    const int facing = std::abs(fw.x) > std::abs(fw.z) ? (fw.x > 0 ? 1 : 3) : (fw.z > 0 ? 2 : 0);
    const bool repeat = !input_.pressed(Action::Place);
    if (inReach && (!repeat || newTarget)) {
      EditCmd pc{EditCmd::Kind::Place, selBlock, uint8_t(facing)};
      pc.x = kPlaceReceipt;  // 2026-10-16: the sim reports the outcome; a rejection is refunded
      sim_->edit(pc);
      // Placement CONSUMES one from the selected stack (P2b-1), gated on reach (a click into the
      // void must not eat a block). A reachable click the sim rejects is refunded next frame.
      {
        ItemStack one = player_.inventory.hotbar()[size_t(hot_)];
        one.count = 1;
        placeRefunds_.push_back(std::move(one));
        if (placeRefunds_.size() > 4096) placeRefunds_.pop_front();
      }
      consumeOne();
      lastPlaceT_ = nowT;
      lastPlaceHit_[0] = curTarget[0]; lastPlaceHit_[1] = curTarget[1]; lastPlaceHit_[2] = curTarget[2];
      noteBlockEdit();
    } else if (!inReach && settings_.bridging && !locomotion_.flying() && fw.y < -0.45f) {
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
          placeRefunds_.push_back(std::move(one));
          if (placeRefunds_.size() > 4096) placeRefunds_.pop_front();
        }
        consumeOne();
        lastPlaceT_ = nowT;
        lastBridge_[0] = bx; lastBridge_[1] = fy; lastBridge_[2] = bz;
        noteBlockEdit();
      }
    }
  }  // Cortana 2026-10-15 1515 (GAME, Herominde H-01): Pick Block needs a target within reach,
  // and only creative may create the block from nothing (survival only selects an existing stack).
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
      if (placedBlockForItem(hb[size_t(i)].itemId) == st.hitBlock) { found = i; break; }
    ItemStack* carry = nullptr;
    if (found < 0)
      for (auto& s : player_.inventory.main())
        if (!s.empty() && placedBlockForItem(s.itemId) == st.hitBlock) { carry = &s; break; }
    if (found >= 0) {
      hot_ = found;
    } else if (carry) {
      std::swap(hb[size_t(hot_)], *carry);
      ui_.refreshCarry(carrySnapshot());
    } else if (creativeMode()) {
      ItemStack old = std::move(hb[size_t(hot_)]);
      hb[size_t(hot_)] = ItemStack{uint32_t(st.hitBlock), uint16_t(1)};
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
// at a time — a second `screenshot` before the frame lands is refused.
std::string Game::requestScreenshot(const ScreenshotRequest& req) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  if (pendingShot_) return "screenshot: a capture is already pending for this frame";
  if (req.toFragment) {
    renderer_.requestGrab();
    pendingShot_ = PendingShot{req, std::string()};
    return "screenshot: capturing into the Logbook gallery...";
  }
  // Path confinement (P1b, 2026-10-13, Appendix F M6): file screenshots
  // land ONLY in the screenshots directory — <config dir>/screenshots
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
           "' escapes the screenshots directory — refused; capturing to '" +
           path + "' instead...";
  return "screenshot: capturing to '" + path + "'...";
}

void Game::debugShowUi(const std::string& view) {
  litholattice::threeway::RendererGuard rendererGuard;  // LL_THREEWAY: renderer calls vs the render thread (Proposed by JD; co-implemented by JD and Cortana)
  if (view == "menu" || view == "settings") enterMenu();
  if (view == "settings") ui_.openSettings(settings_);
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
}

// 0.0.1.53 [C53-1] (Proposed by JD; co-implemented by JD and Cortana): per-section main-thread timing of update(),
// logged once a second during stream benches ("update-split"), to find render-side cost.
namespace {
struct UpdateSplit {
  double ms[10]{};
  int frames = 0;
  Clock::time_point last = Clock::now(), mark;
  void lap(int i) { const auto t = Clock::now(); ms[i] += msBetween(mark, t); mark = t; }
} g_split;
const char* const kSplitNames[10] = {"head", "loco", "deceit", "cam", "pumpStream", "actions", "uploadMeshes", "benchStream", "sounds+ents", "ui"};
}  // namespace
void Game::update(float dt, int width, int height) {
  g_split.mark = Clock::now();
  // lithoscript `wait` pump: script time follows the sim's world-tick
  // counter — deterministic, never wall-clock. A reloaded world resets the
  // counter, so resync instead of fast-forwarding. While the sim is paused
  // the counter does not advance, so waits freeze too (waits measure game
  // time, not real time).
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastScriptTick_) lastScriptTick_ = wt;
    // Per-frame catch-up cap (P2b-2, review F-L7): after a main-thread
    // stall the sim may be thousands of ticks ahead; pumping all of them
    // in one frame (each tick() also pumps the defrag service) is a
    // hitch of its own. Cap at 64/frame and carry the remainder — script
    // time still measures sim time, it just catches up over a few frames.
    uint64_t budget = 64;
    for (; lastScriptTick_ < wt && budget > 0; ++lastScriptTick_, --budget) consoleRegistry_.tick();
  }
  // Handshake freeze mirror (P2b-2, CH-H7): when the watchdog freezes the
  // sim, the GAME state follows — UI, `playing` and the world-query
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
  // console (or any text field) holds keyboard focus — text entry wins.
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
  // tick duration, consuming this frame's buffered input intent — the same
  // worldTick counter the vitals tick from below, so movement is
  // deterministic (fixed-dt steps of recorded intent) and freezes while
  // the sim is paused. Solidity is the game's entity world query — the
  // same getBlock/kSolid test the item entities sweep against, unloaded
  // cells reading as air (not solid).
  // Live only while Playing, out of the free camera (a camera, not the
  // player — it stays exactly as free as before), and out of bench (the
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
    const litholattice::Entity& b = locomotion_.body();
    // 0.05 blocks: above float-rounding of the body's double position at
    // any dimension bound, below any real step (max ~0.35 blocks/tick).
    if (!locomotion_.placed() || std::fabs(double(player_.pos.x) - b.x) > 0.05 ||
        std::fabs(double(player_.pos.y) - b.y) > 0.05 ||
        std::fabs(double(player_.pos.z) - b.z) > 0.05)
      locomotion_.place(player_.pos);  // an external write won: reseat (zeroes velocity)
    if (sim_) {
      const uint64_t wt = sim_->stats().worldTick;
      if (wt < lastLocomotionTick_) lastLocomotionTick_ = wt;  // world reloaded: resync, never fast-forward
      uint64_t ticks = wt - lastLocomotionTick_;
      lastLocomotionTick_ = wt;
      // Cap the catch-up (tick-debugger steps, long hitches): beyond the
      // cap the body simply does not consume the dropped ticks.
      if (ticks > 64) ticks = 64;
      if (ticks > 0) {
        const double dtTick = 1.0 / double(std::max(1, sim_->stats().targetTps));
        // Proposed by JD; co-implemented by JD and Cortana: the Highway Ramp is not a full cell for the player; its
        // 1-in-2 surface is applied after each tick by rampSupport (smooth walk-up, no step).
        const auto isSolid = [this](int x, int y, int z) {
          if (!sim_) return false;
          const BlockId b = sim_->getBlock(x, y, z);
          return b != kHighwayRamp && blockRegistry().has(b, kSolid);
        };
        const auto rampSupport = [this] {
          namespace hw = litholattice::procgen::ramp::highway;
          litholattice::Entity& e = locomotion_.mutableBody();
          if (e.vy > 0) return;
          const int bx = int(std::floor(e.x)), bz = int(std::floor(e.z)), by = int(std::floor(e.y));
          for (int cy = by; cy >= by - 1; --cy) {
            const auto [b, st] = sim_->getBlockAndState(bx, cy, bz);
            if (b != kHighwayRamp) continue;
            const double top = cy + hw::heightAt(uint32_t(st) & 7u, e.x - bx, e.z - bz);
            // Lift onto the slope when inside it; stick to it when walking down (small gap).
            if (e.y < top + (locomotion_.flying() ? 0.0 : 0.2) && e.y > top - 0.75) {
              e.y = top; e.vy = 0; e.onGround = true;
            }
            return;
          }
        };
        const uint64_t perTick = worldclock::unitsPerTick(sim_->stats().targetTps);
        for (uint64_t i = 0; i < ticks; ++i) {
          locomotion_.tick(pendingLocomotionInput_, isSolid, dtTick);
          if (sim_) rampSupport();  // Highway Ramp walk surface
          // Cortana 2026-10-15 2255 [GAME] (JD 22:50): digital prayer, advanced per sim tick from
          // the body's derived Kneel pose (sneak-bit triple tap, no control-format change).
          // Cortana 2026-10-16: mealtime windows on the world clock (the clock at tick i).
          const uint64_t back = (ticks - 1 - i) * perTick;
          const uint64_t tickTime = worldClock_.time >= back ? worldClock_.time - back : 0;
          if (const int32_t g = prayer::tick(prayer_, locomotion_.pose() == litholattice::LocomotionPose::Kneel,
                                             playerAdapter_.staticCharge(), tickTime);
              g > 0) {
            playerAdapter_.grantStatic(g);
            logInfo("prayer: +%d static (%u/%u today, day %u, %s)", g, unsigned(prayer_.prayersToday()),
                    unsigned(prayer::kPrayersPerDay), unsigned(prayer_.prayerDay), worldclock::hhmm(tickTime).c_str());
          }
        }
      }
    }
    player_.pos = locomotion_.position();
  } else {
    locomotion_.place(player_.pos);
    if (sim_) lastLocomotionTick_ = sim_->stats().worldTick;  // no catch-up burst on re-entry
    pendingLocomotionInput_ = litholattice::LocomotionInput{};
  }
  // --- IPlayer integration (player_adapter.h) ---
  // Mirror the game player into the engine IPlayer (position, the body's
  // TRUE velocity, look, dimension, health), then advance the vitals by the sim
  // world ticks elapsed since the last frame — the same counter the
  // lithoscript `wait` pump uses, so vitals freeze while the sim is paused
  // and never run on the sim thread. The head-in-liquid test is one
  // getBlock at the eye cell per frame (the sanctioned handful-of-queries
  // budget), applied to all elapsed ticks.
  playerAdapter_.syncFromGame(active_ ? mcpDimensionNumber(active_->id) : 0, dt,
                              locomotion_.velocity());
  if (sim_) {
    const uint64_t wt = sim_->stats().worldTick;
    if (wt < lastVitalsTick_) lastVitalsTick_ = wt;  // world reloaded: resync, never fast-forward
    if (wt > lastVitalsTick_) {
      const uint64_t ticks = wt - lastVitalsTick_;
      lastVitalsTick_ = wt;
      const Vec3 eye = playerEyePos(player_, locomotion_.eyeHeight());
      const bool headInLiquid = blockIsLiquid(sim_->getBlock(int(std::floor(eye.x)), int(std::floor(eye.y)),
                                                             int(std::floor(eye.z))));
      // PLY-1: tickVitals normalises the tick count to wall-clock seconds
      // at the sim's target TPS, so drain rates are TPS-invariant.
      playerAdapter_.tickVitals(ticks, headInLiquid, playing && input_.held(Action::Sprint),
                                sim_->stats().targetTps);
    }
  }
  // Sprite system: advance by the same sim-tick stream as the vitals, so
  // sprites freeze while the sim is paused (presentation only — sprite
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
  // advance by the same elapsed sim world ticks as vitals/sprites —
  // cAiWorld::advance(simTicks, worldTps), NEVER tick() per sim tick —
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
  // the adapter's IPlayer state — transform, true velocity, health and the
  // named stats all come from the engine player system now.
  mcpControllable_.store(menuState_ == MenuState::Playing && playing && !typing_,
                         std::memory_order_relaxed);
  mcpWorldTick_.store(sim_ ? sim_->stats().worldTick : 0, std::memory_order_relaxed);  // Cortana 2026-10-15 1658: F-2
  {
    std::lock_guard<std::mutex> lk(mcpStateMutex_);
    const litholattice::PlayerState& ps = playerAdapter_.enginePlayer().state();
    mcpPlayerState_.x = ps.x;
    mcpPlayerState_.y = ps.y;
    mcpPlayerState_.z = ps.z;
    mcpPlayerState_.vx = ps.vx;
    mcpPlayerState_.vy = ps.vy;
    mcpPlayerState_.vz = ps.vz;
    mcpPlayerState_.yaw = ps.yaw;
    mcpPlayerState_.pitch = ps.pitch;
    mcpPlayerState_.dimension = ps.dimension;
    mcpPlayerState_.health = uint8_t(std::clamp(playerAdapter_.health(), 0, 64));
    mcpPlayerState_.stats = playerAdapter_.namedStats();
    // MCP view of the hotbar: the block each inventory hotbar stack places
    // (0 = the stack places nothing / is empty), derived from the one
    // inventory store (P2b-1).
    mcpPlayerState_.hotbar.clear();
    for (const ItemStack& s : player_.inventory.hotbar())
      mcpPlayerState_.hotbar.push_back(uint16_t(placedBlockForItem(s.itemId)));
    mcpPlayerState_.selectedSlot = hot_;
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
  }
  // cCamera system (presentation only): sync the camera.* cvars into the
  // camera configs, then advance the cameras — samples the player's true
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
  const Vec3 eyePos = playerEyePos(player_, locomotion_.eyeHeight());
  sim_->setRay({eyePos.x, eyePos.y, eyePos.z, f.x, f.y, f.z});
  g_split.lap(3);  // [C53-1]
  pumpStreaming();  // generate/load columns around the player; insert finished ones
  g_split.lap(4);
  if (playing && !clickUsed) handleWorldActions();
  g_split.lap(5);
  // LL_THREEWAY: upload phase vs renderFrame (Proposed by JD; co-implemented by JD and Cortana).
  // Design B Stage 2: when the backend stages uploads in its lock-free ring
  // (meshUploadsQueued), this phase takes NO guard at all — uploads are
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
  // the next seal, ahead of that seal's meshes — the same order as this call.)
  if (cfg_.materials) renderer_.enqueuePbrMaterials(*cfg_.materials);
  // PBR analytic lights: refresh the emissive cache when the focus column moved or a
  // block edit marked it dirty (game-chosen cadence: checked every frame, rescanned
  // only when needed - a 3x3-column refresh measured ~46 us in the engine tests).
  if (emissiveProvider_ && active_) {
    const int fcx = int(std::floor(player_.pos.x / 16)), fcz = int(std::floor(player_.pos.z / 16));
    if (emissiveProvider_->needsRefresh(fcx, fcz)) emissiveProvider_->refresh(active_->chunks, fcx, fcz);
  }
  uploadMeshes();
  g_split.lap(6);  // [C53-1] (includes the PBR/emissive lines just above)
  if (stream_) stream_->setFarLod(settings_.vkFarLod);  // [C54-1]
  if (stream_) uploadBytes_ += stream_->update(player_.pos.x, player_.pos.z, resources_.materials());
  g_split.lap(7);
  uploadMs_ = msBetween(tUp, Clock::now());
  if (rgUpload) rgUpload->release();  // LL_THREEWAY (absent on the Stage 2 ring path by design)
  playSounds();
  updateEntities();
  g_split.lap(8);
  updateUi(dt, width, height);
  g_split.lap(9);
  ++g_split.frames;
  if (stream_ && msBetween(g_split.last, Clock::now()) >= 1000.0) {
    char buf[512]; int n = 0;
    for (int i = 0; i < 10; ++i)
      n += std::snprintf(buf + n, sizeof buf - size_t(n), " %s=%.2f", kSplitNames[i], g_split.ms[i] / std::max(1, g_split.frames));
    logInfo("update-split ms/frame:%s", buf);
    g_split = UpdateSplit{};
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
  // ride), so entities freeze exactly when the world freezes — pause
  // menu, tick_freeze, handshake freeze — and step at a fixed rate
  // instead of a frame-rate-shaped one. Catch-up is capped like
  // locomotion's (64 ticks): beyond the cap the excess is dropped, not
  // fast-forwarded. Magnet + physics + despawn all use this dt; when
  // the world is frozen dt is 0 and everything below is inert (the
  // pickup scan still runs, as before — collecting a resting item does
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
  const Vec3& pp = player_.pos;
  for (litholattice::Entity* e : entities_.querySphere(pp.x, pp.y, pp.z, 4.0, kEntityItem)) {
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
  // Projectiles (P2c, ENT-1): one manager tick per elapsed world tick —
  // the locked "projectiles tick with the world" — so they freeze with
  // the world exactly like the entities above (entTicks is 0 while the
  // sim is paused/frozen) and step at the world's rate, never frame rate.
  for (uint64_t i = 0; i < entTicks; ++i) projectiles_.tick();
  // Pickup: within 1.2 blocks the item goes into the inventory (42 slots, 512/stack).
  for (litholattice::Entity* e : entities_.querySphere(pp.x, pp.y, pp.z, 1.2, kEntityItem)) {
    const uint16_t left = player_.inventory.addItem(e->data, uint16_t(e->count));
    if (left == 0) {
      entities_.despawn(e->id);  // fully collected
    } else {
      e->count = left;  // inventory full: stays in the world
    }
  }
}

// Design B Stage 2: hold an upload the renderer's ring refused (ring full)
// for an in-order retry at the next uploadMeshes(). Never a drop — this is
// the guarantee behind the ring's never-block producer: replaces, PBR
// meshes, tombstones and clears all ride the same queue.
void Game::spillRingUpload(uint8_t kind, ChunkMesh&& mesh, litholattice::PbrChunkMesh&& pbr) {
  ringSpill_.push_back(RingSpill{kind, std::move(mesh), std::move(pbr)});
  ++ringSpillEvents_;
  ringSpillMax_ = std::max(ringSpillMax_, ringSpill_.size());
}

void Game::uploadMeshes() {
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
    if (m.epoch != epoch_) return false;  // from an unloaded dimension
    uint64_t& seq = meshSeq_[packPos(m.cx, m.sy, m.cz)];
    if (seq != 0 && seq >= m.seq) return false;  // a newer mesh already landed
    if (seq == 0) ++meshesReceived_;
    seq = m.seq;
    uploadBytes_ += m.verts.size() * sizeof(PackedVertex);
    mcpSenses_.noteMesh(m);  // vision cache copy, before the move into the GL renderer
    // Design B Stage 2: staged on the ring path (applied by the executor at
    // the frame seal), immediate on every other path. A full ring spills
    // the mesh — counted as landed here, because the game has accepted it
    // and the spill retries it in order next frame; it is never dropped.
    if (!renderer_.enqueueChunkMesh(std::move(m))) spillRingUpload(0, std::move(m), {});
    return true;
  };
  { std::vector<ChunkMesh> fresh; meshOut_.drainInto(fresh);
    for (auto& m : fresh) {
      if (m.batch != 0 && m.batchSize > 1 && m.batchSize <= kAtomicBatchMax) {
        const uint64_t id = m.batch;
        auto& b = meshBatches_[id];
        b.push_back(std::move(m));
        if (b.size() >= size_t(b.front().batchSize)) {
          for (auto& bm : b) land(bm);
          meshBatches_.erase(id);
        }
        continue;
      }
      const int64_t k = packPos(m.cx, m.sy, m.cz);
      const auto it = pendingMesh_.find(k);
      if (it == pendingMesh_.end()) { pendingOrder_.push_back(k); pendingMesh_.emplace(k, std::move(m)); }
      else if (m.epoch != it->second.epoch || m.seq > it->second.seq) it->second = std::move(m);
    } }
  constexpr int kUploadBudget = 64;                 // sub-chunks per frame
  constexpr size_t kUploadBytesBudget = 3u << 20;   // bytes per frame
  size_t bytes = 0;
  int uploads = 0;
  for (; !pendingOrder_.empty() && uploads < kUploadBudget;) {
    const int64_t k = pendingOrder_.front();
    auto node = pendingMesh_.find(k);
    if (node == pendingMesh_.end()) { pendingOrder_.pop_front(); continue; }
    const size_t mb = node->second.verts.size() * sizeof(PackedVertex);
    if (uploads > 0 && bytes + mb > kUploadBytesBudget) break;  // rest next frame
    ChunkMesh m = std::move(node->second);
    pendingMesh_.erase(node); pendingOrder_.pop_front();
    bytes += mb;
    if (land(m)) ++uploads;
  }
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
  for (; !pbrPendingOrder_.empty() && uploads < kUploadBudget;) {
    const int64_t k = pbrPendingOrder_.front();
    auto node = pbrPendingMesh_.find(k);
    if (node == pbrPendingMesh_.end()) { pbrPendingOrder_.pop_front(); continue; }
    const size_t mb = node->second.verts.size() * sizeof(PbrQuadVertex);
    if (uploads > 0 && bytes + mb > kUploadBytesBudget) break;  // rest next frame
    PbrChunkMesh m = std::move(node->second);
    pbrPendingMesh_.erase(node); pbrPendingOrder_.pop_front();
    bytes += mb;
    if (m.epoch != epoch_) continue;  // from an unloaded dimension
    uint64_t& seq = pbrMeshSeq_[packPos(m.cx, m.sy, m.cz)];
    if (seq != 0 && seq >= m.seq) continue;  // a newer mesh already landed
    seq = m.seq;
    uploadBytes_ += m.verts.size() * sizeof(PbrQuadVertex);
    // Design B Stage 2: same staging/spill discipline as the classic path.
    if (!renderer_.enqueuePbrChunkMesh(std::move(m))) spillRingUpload(1, {}, std::move(m));
    ++uploads;
  }
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
  p.groundPulseOn = settings_.groundPulse;
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
  ui_.paint(p.hud, *resources_.materials(), [this](BlockId b) { return resources_.icon(b); },
            [this](uint32_t itemId) { return resources_.itemIcon(itemId); });
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
  return p;
}

void Game::beginTyping(int x, int y, int z) {
  typing_ = true; typeX_ = x; typeY_ = y; typeZ_ = z;
  input_.push(InputContext::KeyboardTyping);  // letters stop reaching movement / hotbar actions
  EditCmd c{EditCmd::Kind::Use}; c.x = x; c.y = y; c.z = z;
  sim_->edit(c);  // keyboard shows "capturing"
  logInfo("keyboard at %d %d %d: typing capture ON (Esc ends)", x, y, z);
}

void Game::endTyping() {
  if (!typing_) return;
  typing_ = false;
  input_.pop(InputContext::KeyboardTyping);
  EditCmd c{EditCmd::Kind::Use}; c.x = typeX_; c.y = typeY_; c.z = typeZ_;
  sim_->edit(c);
  logInfo("keyboard typing capture OFF");
}

// While capturing: text-channel characters (A-Z, 0-9, symbols) -> SIXBIT code + 1 out the front;
// Enter / Backspace (including auto-repeat) -> right port.
void Game::handleTyping() {
  const TextInput& in = input_.text();
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
  std::string text = cBehaviorCatalog::get().describe(st.hitBlock, st.hitState);
  if (st.hitInfo[0]) { text += "\n"; text += st.hitInfo; }  // inspector: ids, raw state, net, memory
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
  // The HUD hotbar is a snapshot of the player inventory's hotbar slots —
  // the same store placement, pickups and player.lat use (P2b-1).
  for (int i = 0; i < kHotbarSize; ++i) m.hotbar[size_t(i)] = player_.inventory.hotbar()[size_t(i)];
  m.selected = hot_;
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
  if (showDebug_) {      // F3
    const SimStats& st = sim_->stats();
    char l[8][160];
    std::snprintf(l[0], sizeof l[0], "FPS %.0f  frame %.2f ms (max %.1f)", fps_, frameAvgMs_, frameMaxMs_);
    std::snprintf(l[1], sizeof l[1], "TPS %.1f / %d  tick %.3f ms (max %.3f)  day %u %s", st.tps, st.targetTps, st.tickMs,
                  st.tickMaxMs, unsigned(worldclock::dayOf(worldClock_.time)), worldclock::hhmm(worldClock_.time).c_str());
    std::snprintf(l[2], sizeof l[2], "sim budget %.2f%%  dropped %llu  EUV %s", st.budgetPct,
                  (unsigned long long)st.droppedTicks, euvStats(st).enabled ? "on" : "off");
    // Live locomotion state (mode + pose from the body's controller):
    // e.g. "walk · sneak", "fly · stand". Replaces the old "flying (no
    // gravity yet)" confession — gravity exists now.
    const char* modeName = locomotion_.flying() ? "fly" : "walk";
    const char* poseName = locomotion_.pose() == litholattice::LocomotionPose::Sneak   ? "sneak"
                           : locomotion_.pose() == litholattice::LocomotionPose::Crawl ? "crawl"
                             : locomotion_.pose() == litholattice::LocomotionPose::Kneel ? "kneel"  // Cortana 2026-10-15 2240
                                                                                        : "stand";
    std::snprintf(l[3], sizeof l[3], "dim %s  meshes %d/%d  %s \xc2\xb7 %s%s", active_->id.c_str(),
                  meshesReceived_, totalSubs_, modeName, poseName,
                  locomotion_.onGround() ? "" : " (air)");
    std::snprintf(l[4], sizeof l[4], "look: %s", targetText().c_str());
    std::snprintf(l[5], sizeof l[5], "health %d  oxygen %d  stamina %d  kharge %d",
                  playerAdapter_.health(), playerAdapter_.oxygen(), playerAdapter_.stamina(),
                  playerAdapter_.kharge());
    std::snprintf(l[6], sizeof l[6], "renderer: %s", rendererInfo().c_str());  // 2026-10-16 (JD)
    std::snprintf(l[7], sizeof l[7], "%s", releaseInfo().c_str());
    m.debugLines.assign(std::begin(l), std::end(l));
  } else if (settings_.fpsCounter && !cfg_.capture) {  // F56-4 (Proposed by JD; co-implemented by JD and Cortana): perf readout
    char l[200], gpu[24] = "n/a";
    if (gpuMs_ >= 0.f) std::snprintf(gpu, sizeof gpu, "%.2f ms", gpuMs_);
    std::snprintf(l, sizeof l, "FPS %.0f  avg %.2f ms  p99 %.2f ms  update %.2f ms  GPU %s", fps_, frameAvgMs_, p99Ms_, updateMs_, gpu);
    m.debugLines = {std::string(l)};
  }
  if (typing_)
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
                "perf fps=%.0f frame=%.2f/%.1fms tps=%.1f/%d tick=%.3f/%.3fms budget=%.2f%% dropped=%llu chunks=%zu meshes=%d/%d dim=%s",
                fps, frameAvgMs, frameMaxMs, st.tps, st.targetTps, st.tickMs, st.tickMaxMs, st.budgetPct,
                (unsigned long long)st.droppedTicks, chunkColumns_, meshesReceived_, totalSubs_,
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
  // through this same entity-mesh path — the placeholder presentation
  // until a real mob render path exists (cAiWorld mobs had NO renderer
  // before this). Presentation is IDENTICAL to a Receiptor's while
  // non-hostile: trace-trunk legs, ceramic torso and head. The head
  // material swaps with the face state the brain publishes — a lit
  // lamp for the Smile, a red torch for the Scowl — the only render
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