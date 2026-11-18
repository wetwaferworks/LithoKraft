// Cortana 2026-10-15 0935: ALT-TAB pausedByFocus_
// Game layer: wires world + sim + render together for one player. Owns the dimension
// registry, the active dimension's sim thread, the mesh pipeline, creative placement,
// hotbar and debug commands.
#pragma once
#include "litholattice/render/view_radius.h"  // draw-side view bound
#include "lithokraft/content/circuit_scene.h"
#include <atomic>
#include <chrono>  // Cortana 2026-10-15 2215
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <optional>
#include <unordered_set>
#include "lithokraft/game/engine.h"
#include "lithokraft/game/prayer.h"  // Cortana 2026-10-15 2255
#include "lithokraft/game/view_mesh_gate.h"  // Archie Phase 1
#include "lithokraft/game/frame_budget.h"    // R4
#include "lithokraft/game/stream_rank.h"     // R3
#include "lithokraft/game/world_clock.h"  // Cortana 2026-10-16
#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "litholattice/core/threading.h"
#include "litholattice/world/chunk_ingress.h"  // Thread Handshake P5
#include "litholattice/world/neighbor_mesh.h"  // Thread Handshake P8
#include "litholattice/world/chunk_swarm.h"    // Thread Handshake P6
#include "lithokraft/game/sound_director.h"
#include "lithokraft/game/player.h"
#include "lithokraft/game/player_adapter.h"
#include "lithokraft/game/player_locomotion.h"
#include "lithokraft/game/entities.h"
#include "lithokraft/game/resource_manager.h"
#include "litholattice/input/input_system.h"
#include "lithokraft/game/actions.h"
#include "lithokraft/game/world_save.h"
#include "lithokraft/game/cow_publish.h"
#include "lithokraft/game/cAsyncSaver.h"
#include "lithokraft/game/tick_signal_posts.h"  // t46u 6b-3b: cSignalInitTickets
#include "lithokraft/game/cWorldLoadSoak.h"
#include "lithokraft/game/cTickStabilityMeter.h"  // 2026-10-31 (JD): Save World, tick-boundary snapshot + pool write
#include "lithokraft/game/lithography.h"  // Cortana 2026-10-15 1900
#include "lithokraft/game/camera_controller.h"  // cGameCameraController (engine camera system)
#include "lithokraft/game/sprite_host.h"        // cSpriteHost (engine sprite system)
#include "lithokraft/game/ai_host.h"            // AiHost (engine AiWorld production owner, B6)
#include "lithokraft/game/deceitor.h"           // Deceitor mob director (brains live in the AiWorld)
#include "lithokraft/game/mcp_player_control.h"
#include "lithokraft/game/mcp_senses_adapter.h"
#include "lithokraft/game/settings_cvars.h"
#include "lithokraft/game/game_commands.h"
#include "lithokraft/game/fragment_gallery.h"
#include "lithokraft/game/fragment_markdown.h"
#include "lithokraft/game/fragment_state.h"
#include "lithokraft/game/world_query.h"
#include "lithokraft/game/bench_stream.h"
#include "lithokraft/game/chunk_streamer.h"
#include "lithokraft/game/L0NeighbourHold.h"  // fix (b) re-mesh churn
#include "lithokraft/game/StreamFillTracker.h"  // --stream-bench
#include "litholattice/core/time.h"
#include "litholattice/platform/window.h"
#include "litholattice/render/renderer.h"
#include "litholattice/render/binary_greedy_mesher.h"   // BinaryQuad path (mesher option)
#include "litholattice/render/pbr_quad_mesh.h"          // PbrChunkMesh + expansion
#include "litholattice/render/pbr_emissive_provider.h"  // cChunkEmissiveProvider (PBR lights)
#include "litholattice/script/script_engine.h"
#include "litholattice/console/concmd.h"
#include "litholattice/console/debug_console.h"
#include "litholattice/console/script_console.h"
#include "litholattice/mcp/server.h"  // cMcpServer / McpBackends (P2c MCP wiring)
#include "litholattice/projectile/projectile.h"  // cProjectileMgr (P2c, ENT-1)
#include "litholattice/sim/sim_thread.h"
#include "litholattice/world/dimension.h"
#include "lithokraft/game/game_ui.h"
#include "lithokraft/game/cWorldPicker.h"
#include "lithokraft/game/script_editor.h"
#include "lithokraft/game/settings.h"
#include "lithokraft/game/ui_script_bindings.h"

namespace litholattice { class cMaterialReg; }  // pbr_materials.h; main.cpp owns the instance

namespace lithokraft {

/// The game's clock domains (SimConfig::domains order): lithographed EUV-tier chips.
constexpr int kEuvDomain = 0;
inline const SimStats::Domain& euvStats(const SimStats& s) { return s.domains[kEuvDomain]; }

struct GameConfig {
  std::string dimension = "wafer";
  uint64_t seed = 0;        // world seed (--seed): worldgen noise, per-chunk features
  int worldChunks = 16, tps = kDefaultTps, tickRadius = 8;
  bool roundSimArea = false;  ///< F56-7 (Proposed by JD, Built by Cortana): disc dx^2+dz^2 <= r^2+r instead of the square
  bool diamondSimArea = true;  ///< diamond (Manhattan) sim area (JD 2026-10-23); wins over roundSimArea. DEFAULT since fix run 3 (JD t2262u)
  bool diamondStreamArea = true;  ///< diamond (Manhattan) stream/residency footprint (JD 2026-10-23). DEFAULT since fix run 3
  int vkFxOverride = -1;  // 0.0.1.51: --vkfx for this run only (never saved); -1 = settings
  int streamRadius = 32;  // JD 2026-10-16: render / procgen stream distance (chunks, max 64)
  /// R2 (2026-11-17): streaming budgets by count and bytes only, never the wall clock, so a bench
  /// or golden run admits, meshes and uploads the same columns on any host load.
  /// --det-budget or LL_DET_BUDGET=1. Play mode keeps the frame-aware budgets.
  bool deterministicBudget = false;
  bool streaming = true;  // false: no player-centred streaming (fixed pre-baked world only)
  bool euv = true;
  bool timingHandshake = true;   // SimConfig::timingHandshake at world load (standard; default ON since 2026-10-14)
  int handshakePhases = 4;       // phases per handshake cycle
  int handshakeWorkers = 0;      // --handshake-workers: SimConfig::handshakeWorkerThreads (0 = authority auto)
  std::string scriptsDir;   // empty = no scripts
  bool pauseSim = true;     // the pause menu stops the sim (single-player); bench ignores it
  std::string benchScene = "default";  // "circuits" / "display": stress scenes built next to the spawn
  int sceneSize = 120;
  int sceneRows = 16;
  int sceneCopies = 256;  ///< 2026-11-05 (JD): --copies for alu32 tiles
  bool worldPicker = false;  // 2026-10-30 (JD): menu Play opens the world picker (interactive launches; harness/tests keep direct Play)
  std::string startSpawn;    // 2026-10-30: --spawn <name>: skip the menu and start at this named spawn
  std::string pchieMazeFile;  // 2026-11-10 (JD t5033u, PCHIE P7): --pchie-maze FILE (B1 grid text from pchie_lk_pilot)
  bool testWorldExamples = false;  // 2026-10-24 (JD): playtest builds add the Examples section to the test world
  std::string pattern = "checkers";  // pattern-wall: checkers | circles  // wall-pixel: rows (1-16)
  bool capture = false;      // --capture (playtest 2026-10-16): F3 / controls / hint text off, for filming
  bool screenVertical = false;          // --screen-orient vertical: framebuffer as an upright wall                  // bench scene footprint (blocks per side)
  std::string saveDir;      // empty = no saving / loading (tests, --bench, --no-save)
  bool worldFromFolder = false;  // 2026-10-31: loaded via Load World > folder (not the catalog world's own dir)
  double autosaveSeconds = 300.0;
  std::string version = "dev";  // LITHOKRAFT_VERSION, plumbed from main.cpp for the menu footer
  GameSettings settings;        // player preferences (main.cpp loads config/settings.json)
  std::string settingsPath;     // where the settings persist (empty = do not save)
  const litholattice::cMaterialReg* materials = nullptr;  // PBR registry (main.cpp loads the
      // data file and owns it; must outlive the game). Null = chunk saves skip the MATS payload.
};

class Game {
 public:
  /// `resources` (block materials for meshing and HUD swatches; F9 reloads them), `audio` and
  /// `input` must outlive the game. The game pushes / pops input contexts (inventory, keyboard
  /// typing) on `input`.
  Game(const GameConfig& cfg, IRenderer& renderer, cJobPool& pool, cResourceMgr& resources, cAudioEngine& audio,
       cInputSystem& input);
  ~Game();
  /// Saves the active dimension and the player (F5, autosave, exit, dimension switch). `wait`
  /// blocks until it is on disk. No-op without a save directory.
  /// 2026-10-31 (JD): wait=false (F5, menu, autosave, console) goes through cAsyncSaver - snapshot
  /// on the tick thread at a tick boundary, write on a pool worker, nobody waits (LL_SAVE_ASYNC=0:
  /// the old stop-the-sim save). wait=true (exit, world load) stays synchronous and on disk.
  bool saveWorld(const char* reason, bool wait, bool allowAsync = true);  // true when a save was written / posted
  bool ok() const { return active_ != nullptr; }

  /// Reads this frame's actions (the app updated the cInputSystem first), turns them into sim
  /// commands (applied by the sim thread at its next tick boundary), uploads meshes, plays sounds.
  void update(float dt, int width, int height);
  /// Pump a cooperative (no-thread) sim on the caller. No-op when the sim has a dedicated thread.
  void pumpSimCooperative();
  FrameParams frameParams(int width, int height) const;
  /// The view pose the render path uses: the camera system's active camera
  /// (first person resolves live from the player â€” the identical view the
  /// game rendered before the camera system).
  CameraViewPose activeCameraPose() const { return camera_.viewPose(); }
  void uploadEntities();  // builds the item billboard mesh
  std::string statusLine(double fps, double worstMs) const;
  /// 2026-10-16 (JD): "Software avx2 x8 threads" / "GL33" / "Vulkan", and version (commit) Release|Debug + build date.
  std::string rendererInfo() const;
  static std::string releaseInfo();
  // Frame timing measured by the app loop (fps, avg/max frame ms over the last ~0.5 s); shown on the HUD.
  void setFrameStats(float fps, float avgMs, float maxMs) { fps_ = fps; frameAvgMs_ = avgMs; frameMaxMs_ = maxMs; }
  void setPerfReadout(float p99Ms, float updateMs, float gpuMs) { p99Ms_ = p99Ms; updateMs_ = updateMs; gpuMs_ = gpuMs; }  // F56-4
  /// R4 (B5/B6): the pacer cap (0 = uncapped, priced at 30 FPS) and what it slept after the last frame.
  void setFramePacing(int capFps, double lastSleptMs) { frameTargetMs_ = cFrameBudget::targetMsFor(capFps); pacerSleptMs_ = lastSleptMs; }
  double frameTargetMs() const { return frameTargetMs_; }
  /// --bench: put the camera on a fixed, repeatable path (a slow circle over the spawn, looking
  /// across the world) at time `t` seconds. Same path for every backend and build.
  /// --bench: put the camera on a fixed, repeatable path at time `t` seconds. "orbit": a slow
  /// circle over the spawn. "stream": a straight line (+x, 32 blocks/s) out of the fixed world,
  /// with a cBenchStreamer generating / meshing / dropping columns around the camera.
  /// "forward" / "line": play-streamer +x flight at 24 blocks/s (LL_FORWARD_SPEED), 20 up.
  void setBenchPath(const std::string& path);
  /// Cortana 2026-10-16 (GUI overhaul tooling): open a UI view for a bench screenshot
  /// ("hud" = gameplay HUD + F3 panel, "menu", "settings", "pause") and capture the next frame
  /// to `path` (CLI-given, so not confined to the screenshots dir). Bench-only (--ui-shot).
  void debugShowUi(const std::string& view);
  void requestUiShot(const std::string& path);
  void setBenchCamera(float t);
  cBenchStreamer* benchStreamer() { return stream_.get(); }
  /// Latest sim stats snapshot. Returns nullopt before the sim thread exists.
  /// NOTE: the returned reference aliases the triple-buffer's read slot â€” copy
  /// what you need immediately; the next stats_.publish() may overwrite it.
  std::optional<std::reference_wrapper<const SimStats>> simStats() {
    if (!sim_) return std::nullopt;
    return std::cref(sim_->stats());
  }
  /// Main-thread cost of the last update(): mesh uploads (ms, bytes).
  double lastUploadMs() const { return uploadMs_; }
  size_t lastUploadBytes() const { return uploadBytes_; }
  int lastUploadItems() const { return uploadItems_; }
  int lastUploadBatchMax() const { return uploadBatchMax_; }
  /// next-build-2: the update-split "ui" bucket of the most recent update
  /// (whole GUI phase incl. hudModel), for the app loop's spike record.
  double lastUiSplitMs() const { return lastUiSplitMs_; }
  /// GUI lag round 2 (2026-11-10): last update's drain/commit split of
  /// uploadMeshes (drain+bookkeeping vs the commit loop proper).
  /// Diagnostic only; plain values read by the app loop's spike record.
  double lastMeshDrainMs() const { return lastMeshDrainMs_; }
  double lastMeshCommitMs() const { return lastMeshCommitMs_; }
  int lastMeshDrained() const { return lastMeshDrained_; }
  size_t lastMeshDrainVerts() const { return lastMeshDrainVerts_; }
  int lastMeshBatchLanded() const { return lastMeshBatchLanded_; }
  size_t lastMeshBatchVerts() const { return lastMeshBatchVerts_; }
  // One-line perf summary for the log (called once per second by the app).
  std::string perfLine(double fps, double frameAvgMs, double frameMaxMs) const;
  // Cortana 2026-10-15 1610 (MIXED): Debug Overlay gating inputs. There is
  // no survival mode yet; the game is creative-only, so this returns true.
  // When survival lands, this must return false for survival worlds; the
  // main loop already hides the full Debug Overlay in that case (except in
  // Debug builds and --bench runs).
  bool creativeMode() const { return true; }
  bool debugOverlayToggled() const { return showDebug_; }  ///< F3 state (ToggleDebug)
  size_t meshQueueDepth() const { return pendingOrder_.size() + meshBatches_.size(); }
  /// Bench steady-state gate (JD 2026-10-30): true when every column in the stream footprint around
  /// the player is resident, out of the L0 neighbour hold, and has its first-wave meshes back.
  /// True when there is no player streamer. Game thread; O(radius^2).
  bool streamSettled() const;
  /// First unmet settle condition ("footprint", "l0-hold", ...), or nullptr when settled.
  const char* streamUnsettledReason() const;
  /// Columns in the stream footprint around the player (the radius the settle check covers).
  size_t streamFootprintColumns() const;
  /// --stream-bench: feed every footprint column to the tracker; true once all are streamed in.
  bool sampleStreamFill(cStreamFillTracker& t, std::chrono::steady_clock::time_point now) const;
  /// Design B Stage 2: ring-spill accounting for the shutdown/bench log.
  uint64_t ringSpillEvents() const { return ringSpillEvents_; }
  size_t ringSpillMax() const { return ringSpillMax_; }
  uint64_t ringSpillDroppedOnSwitch() const { return ringSpillDroppedOnSwitch_; }
  size_t ringSpillDepth() const { return ringSpill_.size(); }
  bool wantsCursorCaptured() const { return captured_ && !inventoryOpen_ && !editorOpen() && !paused_; }
  /// Window focus changed (engine focus events): losing it opens the pause menu (not in bench).
  void onFocusChanged(bool focused);
  void setPaused(bool p);
  /// --frame-hash: freeze sim (even under --bench) and advance exactly one tick per present.
  void armFrameHashSteppedCapture();
  void frameHashStepOne();
  uint64_t worldTick() const;
  bool paused() const { return paused_; }
  /// B9 save/load tick pause (JD 2026-11-11 10:00 PM ET): test seam. When set, the exit / load
  /// sequence appends one line per step ("hold:exit", "save-write:exit", "leave:exit",
  /// "hold:load", "load-components", "sim-start", "release:load", ...) with the sim's world
  /// tick, paused and running flags at that moment. Game thread only; nullptr = off.
  static void setIoOrderTrace(std::vector<std::string>* sink);
  /// The pause state machine's current state (P2b-2): Playing, MenuPaused
  /// (Esc / launch menu), TickFrozen (tick_freeze / tick_step), HsFrozen
  /// (timing-handshake watchdog freeze, mirrored from the sim).
  enum class PauseState { Playing, MenuPaused, TickFrozen, HsFrozen };
  PauseState pauseState() const { return pauseState_; }
  bool quitRequested() const { return quit_; }
  /// Settings > Graphics > Renderer changed (JD 2026-10-24): quit (the exit save runs) and relaunch with settings().renderer.
  bool restartRequested() const { return restart_; }
  /// Launch menu state machine: boot opens the menu (world loaded, sim paused, cinematic
  /// camera); Play hands control to the player instantly, no reload.
  void enterMenu();
  void enterPlay();
  /// 2026-10-30 (JD): --spawn: enter play at a named spawn (tp mark), skipping the menu.
  void startAt(const std::string& spawn);
  /// 2026-10-31 (JD, Load World): an in-process world load. The picker / console sets it; the
  /// app loop (main.cpp) sees it, destroys this Game (exit save on disk, sim + streamer + jobs
  /// drained) and builds the next one on the same window, renderer, pool and resources.
  struct WorldLoadRequest { std::vector<std::string> args; std::string folder, spawn, label; };
  bool worldLoadRequested() const { return worldLoad_.has_value(); }
  WorldLoadRequest takeWorldLoadRequest() { WorldLoadRequest r = std::move(*worldLoad_); worldLoad_.reset(); return r; }
  /// Request a load of the world folder `folder` (an empty folder becomes a new world). Returns a status line.
  std::string requestWorldLoadFolder(const std::string& folder, const std::string& spawn);
  /// A catalog world id (default, testworld, flatlab, ...) or a world folder. Returns a status line.
  std::string requestWorldLoad(const std::string& idOrFolder);
  /// The most recent finished async save (profile: snapshot / write ms, bytes); nullopt before the first.
  const std::optional<cAsyncSaver::Result>& lastAsyncSave() const { return lastAsyncSave_; }
  bool inMenu() const { return menuState_ == MenuState::cMenu; }
  const GameSettings& settings() const { return settings_; }
  /// Applies a display-mode change from the settings UI (main.cpp wires the window).
  void setDisplayModeApplier(std::function<void(DisplayMode)> f) { displayApplier_ = std::move(f); }
  /// OS clipboard for UI text fields (optional; a process-local one otherwise). Must outlive the game.
  void setClipboard(ui::IClipboard* clipboard) { ui_.root().setClipboard(clipboard); }
  /// The game's UI (HUD, hotbar, inventory, script editor); for tools and tests.
  cGameUi& gameUi() { return ui_; }
  /// MCP player-control adapters (used by mcpBackends() below; also
  /// directly accessible for tests).
  cMcpPlayerController& mcpPlayerController() { return mcpController_; }
  cMcpPlayerView& mcpPlayerView() { return mcpView_; }
  /// MCP pilot-senses adapter (one object serves both the vision and
  /// audio seams).
  cMcpSensesAdapter& mcpSenses() { return mcpSenses_; }
  /// The production cAiWorld host (B6): bound to the active dimension in
  /// loadActive(), gated by the ai.enabled cvar (default off â€” nothing
  /// is constructed), advanced from elapsed sim world ticks in update().
  cAiHost& aiHost() { return aiHost_; }
  /// The Deceitor director: owns the mob brains' game-side registry +
  /// hooks (filled in the ctor); mobs themselves live in aiHost_'s
  /// cAiWorld. Spawn/interact/attack are today's explicit test paths
  /// (the `deceitor` console command, cheat) for the future
  /// click-picking / mob-combat hooks.
  cDeceitorDirector& deceitorDirector() { return deceitorDirector_; }
  std::string spawnDeceitorCmd();
  std::string deceitorInteractCmd();
  std::string deceitorAttackCmd();
  /// The MCP backend bundle for the engine cMcpServer (P2c 2026-10-13,
  /// review F-M9): the game's existing adapters â€” player control, player
  /// view, pilot vision, pilot audio. Every other seam stays null, so
  /// tools backed by an unwired seam answer "not wired" honestly (the
  /// engine server's documented null-seam behaviour). The shared_ptrs use
  /// no-op deleters: the adapters are Game members, and the wiring site
  /// (main.cpp, --mcp-stdio) stops the server BEFORE the Game is
  /// destroyed, so the pointers never outlive the Game.
  litholattice::mcp::McpBackends mcpBackends();
  /// Hand the running MCP server to the game so the `mcp grant|revoke|
  /// agent` console commands (registered on first call) can drive its
  /// permission flow â€” the commands the engine headers assume. Call on
  /// the game thread before the frame loop; pass nullptr on shutdown.
  /// Without a server (no --mcp-stdio) the commands do not exist.
  void setMcpServer(litholattice::mcp::cMcpServer* server);
  /// The engine projectile manager (P2c 2026-10-13, ENT-1): ticked once
  /// per world tick from updateEntities(), with its voxel seam over the
  /// sim's block reads and its chunk seam over the cChunkStreamer. No
  /// production code spawns projectiles yet (Ping's item-path projectiles
  /// are explicitly out of scope); spawners use this accessor.
  litholattice::cProjectileMgr& projectiles() { return projectiles_; }

  /// Completes a pending `screenshot` capture: grabs the armed frame from the
  /// renderer, PNG-encodes it (downscaled for lowres), and writes the file or
  /// files it in the Fragment gallery. The app calls this after
  /// renderer->renderFrame(), before present. No-op without a pending shot.
  void capturePendingScreenshot();
  /// The Fragment's screenshots gallery (Fragment lore page 5). The future
  /// Fragment UI binds to this; `screenshot fragment` captures into it.
  cFragmentGallery& fragmentGallery() { return fragmentGallery_; }
  /// The Fragment book's upgrade state (A3 wiring, 2026-10-13, no-decision
  /// subset of brief B1): Game owns the ONE production instance, next to
  /// the gallery it conceptually belongs to. Game thread only (see
  /// fragment_state.h). NOT persisted yet and with NO upgrade trigger â€”
  /// both are JD decision C3, deliberately not chosen here.
  FragmentState& fragmentState() { return fragmentState_; }
  const FragmentState& fragmentState() const { return fragmentState_; }
  /// The production Fragment text-render path (A3/B1 subset): the markdown
  /// renderer's enable flag is fed from the owned FragmentState's
  /// markdown_unlocked() â€” never a literal â€” so when a future JD-approved
  /// trigger (C3) upgrades the Fragment, every render through here follows
  /// with no further wiring. No dialog binds to it yet (also C3).
  FragmentRenderResult renderFragmentText(const std::string& text, FragmentView view) const {
    return renderFragment(text, fragmentState_.markdown_unlocked(), view);
  }

  // Unload the current dimension (sim, chunks, meshes) and load another. Debug: F6.
  bool switchDimension(const std::string& id);
  const std::string& dimensionId() const { return active_->id; }

  // --- Tools / tests surface (headless harness, MCP): the player transform and
  // streaming state. Gameplay code uses player_ / streamer_ directly. ---
  Vec3 playerPosition() const { return player_.pos; }
  void setPlayerPosition(const Vec3& p) { player_.pos = p; }
  /// Locomotion state of the local player body (steps on the sim tick stream).
  litholattice::LocomotionPose playerPose() const { return locomotion_.pose(); }
  bool playerOnGround() const { return locomotion_.onGround(); }
  Vec3 playerVelocity() const { return locomotion_.velocity(); }
  bool playerFlying() const { return locomotion_.flying(); }
  /// The `fly [on|off]` command, public for the headless harness (BUG-0002 selftest).
  std::string setFlyMode(const std::string& arg) { return toggleFly(arg); }
  /// Headless harness hooks (BUG-0007 selftest): aim the player and fill a hotbar slot.
  void setPlayerLook(float yaw, float pitch) { player_.yaw = yaw; player_.pitch = pitch; }
  void setHotbarStack(int slot, uint32_t itemId, uint16_t count) { player_.inventory.hotbar()[size_t(slot)] = ItemStack{itemId, count}; hot_ = slot; }
  /// Q-drop (JD ruling 2026-11-10 08:35 EST, design contract in
  /// Floating-Items-Design-2026-11-10.md section 2.4): the inventory
  /// gestures call dropFromSlot; the GUI side owns the slot mutation
  /// decision, the game side owns the world. dropFromSlot computes
  /// what leaves (one item, or the whole stack when `whole`), refuses
  /// on an empty slot (no mutation, no spawn), calls dropStackToWorld
  /// for exactly the leaving part, and only then mutates the slot...
  /// a refused world drop never loses an item. The creative palette
  /// discard path NEVER calls either function (putBackSlot above is
  /// that path: deletion by the creative source).
  struct DropRequest {
    ItemStack stack;    ///< full stack leaving the inventory (id, count, per-stack data)
    Vec3 origin;        ///< spawn point: player eye, nudged 0.3 down (MC parity)
    Vec3 direction;     ///< player forward() (unit, includes pitch)
    bool fromPlayer = true;  ///< Q drop: long pickup delay (design 2.5)
  };
  uint64_t dropStackToWorld(const DropRequest& req);  ///< entity id, 0 on refusal
  bool dropFromSlot(int flatIndex, bool whole);       ///< false: empty slot or refused drop
  /// Cursor stack (2026-11-11, JD finding 07:02 EST: "the mouse is
  /// considered the hand"; Cursor-Stack-Plan-2026-11-11.md). The game
  /// owns the carried stack: cursorStack_ is a real ItemStack held off
  /// the inventory while the creative screen is open, and
  /// cursorOriginFlat_ is the flat slot it returns to on close.
  /// cursorClick resolves one press on a carry slot (lift / place /
  /// merge / swap, plan 3.1..3.3); paletteClickCursor is the palette
  /// discard (3.4: back to the infinite source, NEVER a world drop);
  /// dropFromCursor is Q-from-the-hand (3.6: same contract as
  /// dropFromSlot, the cursor mutates only after the world accepts);
  /// returnCursorStack is the close chain (plan 5: origin, then
  /// Inventory::placeStack's merge-then-first-empty over the carry
  /// range, then a never-destroy fallback) and runs inside
  /// setInventoryOpen(false) before the screen closes, so closing can
  /// never destroy or duplicate the carried stack. Public beside
  /// dropFromSlot: the headless harness drives these directly.
  const ItemStack& cursorStack() const { return cursorStack_; }
  int cursorOriginFlat() const { return cursorOriginFlat_; }
  bool carryingStack() const { return !cursorStack_.empty(); }
  /// B9 inventory drag (JD 2026-11-11 ~10:06 PM ET): true while the hand
  /// holds a stack pulled from the infinite creative palette (it has no
  /// inventory origin; on close it returns to the palette, never into a slot).
  bool cursorFromPalette() const { return cursorFromPalette_; }
  /// Headless harness hooks (B9): the palette slot presses exactly as the
  /// creative screen dispatches them while NOT carrying (pickBlock / pickItem).
  void palettePressBlock(BlockId b, bool shift) { pickInventoryBlock(b, shift); }
  void palettePressItem(uint32_t itemId, bool shift) { pickInventoryItem(itemId, shift); }
  void cursorClick(int flatIndex);
  void paletteClickCursor();
  /// D9 2026-11-12 (JD): Shift-click quick move. Hotbar (0..15) -> main
  /// (18..49), main -> hotbar. Same-identity stacks in the destination
  /// are topped up to their cap first, in slot order (cursorClick's
  /// merge rule 3.3); the remainder goes whole into the first empty
  /// destination slot; whatever finds no room stays in the source slot.
  /// False when nothing moved (empty source, other section full, or a
  /// slot outside hotbar / main).
  bool shiftTransfer(int flatIndex);
  bool dropFromCursor(bool whole);  ///< false: not carrying or refused drop
  void returnCursorStack();
  /// The full stack a dropped entity carries, when it is data-bearing
  /// (design 2.9 option (a), approved by JD 2026-11-10): the side
  /// payload table. Null for plain drops (id + count ride the entity).
  const ItemStack* entityPayload(uint64_t entityId) const {
    const auto it = entityPayloads_.find(entityId);
    return it == entityPayloads_.end() ? nullptr : &it->second;
  }
  /// Live item-entity census (headless harness + diagnostics): counts
  /// kEntityItem entities in a wide sphere around the player.
  size_t itemEntityCount() const {
    return const_cast<litholattice::cEntityMgr&>(entities_)
        .querySphere(player_.pos.x, player_.pos.y, player_.pos.z, 1e9, kEntityItem)
        .size();
  }
  /// The newest live item entity near the player (harness/diagnostics).
  const litholattice::Entity* newestItemEntity() const {
    const auto v = const_cast<litholattice::cEntityMgr&>(entities_)
                       .querySphere(player_.pos.x, player_.pos.y, player_.pos.z, 1e9, kEntityItem);
    return v.empty() ? nullptr : v.back();  // querySphere returns id order: back = newest
  }
  /// Playtest 2026-10-16: carry-grid click (allSlots index 0..41) swaps into the selected hotbar slot.
  /// The player's canonical inventory (P2b-1): the HUD hotbar, placement,
  /// pickups and player.lat all read/write THIS store â€” there is no second
  /// hotbar array any more.
  const Inventory& playerInventory() const { return player_.inventory; }
  /// The selected hotbar slot index (0..7) into playerInventory().hotbar().
  int selectedHotbarSlot() const { return hot_; }
  /// Columns in the active dimension's map right now (pre-bake + streamed).
  size_t loadedColumnCount() const { return active_ ? active_->chunks.columns().size() : 0; }
  /// Columns chunk streaming has brought in beyond the pre-bake (0 = none yet).
  size_t streamedColumnCount() const { return streamer_ ? streamer_->streamedCount() : 0; }
  /// Highest non-air y at world column (x, z), or -1 when that column is not
  /// loaded. Race-free: read between ticks through the sim thread.
  int surfaceYAt(int x, int z);

 private:
  void loadActive();
  void handleToggles();
  void advanceWorldClock();  ///< Cortana 2026-10-16: world clock += sim ticks since the last call
  void handleHotbar();
  static bool altWheelOn();  ///< 2026-10-31 (JD): Alt + wheel steps a 1..64 setting (LL_ALT_WHEEL=0 kill)
  /// This frame's movement intent as a locomotion input: move actions
  /// rotated by the player's yaw into world space (the ILocomotion
  /// contract), jump/sneak/sprint/crawl as held states.
  litholattice::LocomotionInput sampleLocomotionInput() const;
  /// `fly` console command + double-tap Jump: toggle (or set, arg on/off)
  /// the locomotion Fly mode. Returns the console message.
  std::string toggleFly(const std::string& arg);
  void handlePause();
  /// Pause state machine (P2b-2, review CH-H7 + G-M2): the ONE owner of
  /// both Game::paused_ and the sim's paused flag. Every pause source â€”
  /// Esc pause menu, launch menu, tick_freeze/tick_step, handshake
  /// watchdog freeze â€” transitions through setPauseState(); the sim flag
  /// itself is written ONLY by applySimPause() (single computation:
  /// paused_ && cfg_.pauseSim && !benchSet_), plus the transient
  /// world-query scan pause, whose restore also routes through
  /// applySimPause(). Entering Playing from any frozen state clears a
  /// latched handshake freeze (resetHandshakeAfterFreeze) BEFORE the sim
  /// unpauses, so the authority latch can never re-freeze a sim the game
  /// believes is running.
  void setPauseState(PauseState s);
  void applySimPause();
  /// B9 save/load tick pause: hold ticks through the EXISTING sim pause flag (applySimPause
  /// honours ioTickHold_ over every other pause source), then wait out a tick already in
  /// flight, so nothing ticks mid-save / mid-load. Release re-applies the pause machine.
  void holdTicksForIo(const char* why);
  void releaseTicksAfterIo(const char* why);
  void traceIo(const std::string& event) const;
  void handleWorldActions();
  void setInventoryOpen(bool open);
  /// Block-palette click (creative give): writes a full stack of `b` into
  /// the player inventory's hotbar (selected slot; shift = first free slot).
  void pickInventoryBlock(BlockId b, bool shift);
  /// 2026-11-09 next-build-2 (Item 4): creative-screen put-back (a filled
  /// inventory-half slot's stack returns to the infinite palette, i.e.
  /// the slot clears) and the Equipment tab's item give.
  void putBackSlot(int flatIndex);
  void pickInventoryItem(uint32_t itemId, bool shift);
  void pickCarrySlot(int carryIndex);
  std::vector<ItemStack> carrySnapshot();
  /// Cursor stack state (2026-11-11): GUI-session state, deliberately
  /// NOT saved (plan 5.3: the origin slot it left is already empty in
  /// the saved inventory; process exit mid-carry loses the stack with
  /// the unsaved session, the same exposure as any unsaved change).
  ItemStack cursorStack_;        ///< the stack in the mouse-hand (empty = not carrying)
  int cursorOriginFlat_ = -1;    ///< flat slot the carried stack returns to on close
  bool cursorFromPalette_ = false;  ///< B9: the hand holds a palette pull (no slot origin)
  /// B9: a plain palette press while empty-handed puts a full stack in the hand.
  void pullPaletteToCursor(const ItemStack& stack);
  /// --test-world / --bench-scene testworld (playtest 2026-10-16).
  bool testWorld() const { return cfg_.benchScene == "testworld"; }
  /// The block the selected hotbar stack places (kAir when the selected
  /// stack is empty or places nothing â€” a tool, the Fragment).
  BlockId selectedPlaceBlock() const {
    return placedBlockForItem(player_.inventory.hotbar()[size_t(hot_)].itemId);
  }
  /// Slow 2D side-scrolling dolly across the wafer behind the launch menu (side view,
  /// panning laterally); reuses the player camera the renderer already reads.
  void applyMenuCamera();
  /// A settings control changed: apply the live ones, persist everything.
  void onSettingsChanged(const GameSettings& s);
  void saveSettings();
  /// Push the culling-cache prototype toggles to the renderer (JD 2026-10-11).
  void pushCullProtoConfig();
  /// Feeds this frame's input to the UI. Sets `pointerUsed` if the UI took a click and
  /// `escapeUsed` if it took Escape (so gameplay ignores them).
  void routeUiInput(bool& pointerUsed, bool& escapeUsed, int width, int height);
  /// Input context for UI text entry, theme reloads, HUD model -> widgets, layout.
  void updateUi(float dt, int width, int height);
  HudModel hudModel() const;
  bool editorOpen() const { return editor_ && editor_->isOpen(); }
  void uploadMeshes();
  /// PBR analytic lights (phase 2, Worker 3): marks the emissive cache dirty after a
  /// block edit so the next per-frame refresh picks it up. Called at the game's
  /// block-edit sites (Break/Use/Place); the sim applies edits asynchronously, so
  /// worst-case staleness is one refresh interval - explicit, never silent drift.
  void noteBlockEdit() { if (emissiveProvider_) emissiveProvider_->markDirty(); }
  void playSounds();
  void updateEntities();  ///< spawn from sim events, magnet, tick, pickup (worldTick-driven, ENT-3)
  void updateDeceitors(float dt);  ///< mirror cAiWorld Deceitor mobs into render entities (after the AI advance); P2: interpolated legs
  uint32_t nearestDeceitorMob(double maxDist);  ///< nearest director mob to the player (0 = none)
  void reloadResources();
  /// Spring-arm collision query for the camera controller (its ICameraWorld
  /// hook): engine voxel raycast over the active dimension's ChunkMap, read
  /// race-free between sim ticks; on a hit, hitDist gets the exact entry
  /// distance along the (normalized) ray. False = no solid cell in range.
  bool cameraRaycast(Vec3 origin, Vec3 dir, float maxDist, float& hitDist);
  LithoReader lithoReader();  // Cortana 2026-10-15 1900: sim thread, between ticks only

  /// Per-frame chunk streaming: request columns around the player and insert
  /// finished ones into the active dimension (sim stopped for the insert).
  void pumpStreaming(float frameDtMs);
  void pumpViewGate();  ///< Archie Phase 1: retire columns leaving the view, mesh columns entering it
  /// Column eviction (A2 / B-M1): drops resident columns beyond the stream
  /// radius + hysteresis, bounded per call, in its own sim-stopped window
  /// at the end of pumpStreaming(). Dirty or signal-bearing columns are
  /// saved first (cWorldSave::saveColumns, SGNL exported before unscan);
  /// a column whose save cannot complete is NOT evicted. Clean,
  /// signal-free columns evict with no save (regenerable from the seed).
  void evictFarColumns();
  /// Q12 (Gate 1): LL_FAR_DEMOTE=1 demotes clean far columns to an edge shell; promotes on approach.
  void demoteFarColumns();
  /// Retires every sub-chunk mesh of a column being evicted: purges the
  /// pending classic/PBR upload queues, tombstones the upload sequence
  /// entries (cleared again if the column re-streams and re-inserts),
  /// and uploads empty meshes so the renderer and the MCP vision cache
  /// drop theirs. Game thread, sim stopped (eviction window).
  void retireColumnMeshes(int cx, int cz, bool force = false);
  void initScripts();
  std::string targetText() const;
  void beginTyping(int x, int y, int z);
  void endTyping();
  void beginPostEdit(int x, int y, int z);  ///< 2026-10-25 Posts: typing goes to the post's text
  void handleTyping();

  enum class MenuState { cMenu, Playing };

  GameConfig cfg_;
  cScriptEngine scripts_;
  cGameUi ui_;                          // before uiScripts_ / editor_, which reference it
  cUiScriptBindings uiScripts_;
  std::unique_ptr<cScriptEditor> editor_;  // F10; only with a scripts folder
  litholattice::console::cConsoleReg consoleRegistry_;  // before debugConsole_
  uint64_t lastScriptTick_ = 0;  // sim world-tick counter for the lithoscript `wait` pump
  cFragmentGallery fragmentGallery_;  // the Fragment's screenshots (lore page 5)
  FragmentState fragmentState_;      // the Fragment's upgrade state (A3: production-owned, C3-gated trigger/persistence)
  // A `screenshot` command armed during update(); completed by
  // capturePendingScreenshot() after the next renderFrame(), before present.
  struct PendingShot {
    ScreenshotRequest req;
    std::string path;  // resolved at request time (auto-name or the given path)
  };
  std::optional<PendingShot> pendingShot_;
 public:
  bool screenshotPending() const { return pendingShot_.has_value(); }  // B61-1 LL_THREEWAY capture timing (Proposed by JD; co-implemented by JD and Cortana)
 private:
  std::string requestScreenshot(const ScreenshotRequest& req);
  // First-party lithoscript population (language/population split, JD-locked):
  // settings cvars (live, archived), Decision-9 game commands, world queries.
  std::unique_ptr<SettingsCvars> settingsCvars_;
  std::unique_ptr<cGameWorldQuery> worldQuery_;
  std::unique_ptr<litholattice::console::cDebugConsole> debugConsole_;
  std::shared_ptr<litholattice::console::ConsoleScriptContext> consoleScriptCtx_;
  std::vector<ui::UiEvent> uiEvents_;
  int themeRevision_ = -1;
  bool uiText_ = false;                // UiText input context pushed
  bool editorWasOpen_ = false;
  // Cortana 2026-10-15 2215 [GAME] (Controls spec): crawl toggle + hold-to-repeat state.
  bool crawlToggle_ = false;
  prayer::State prayer_;  // Cortana 2026-10-15 2255: digital prayer (prayer.h)
  worldclock::Clock worldClock_;  // Cortana 2026-10-16: persisted world clock (level.json v2)
  uint64_t lastClockTick_ = 0;    ///< sim worldTick the world clock last advanced to
  bool clockSynced_ = false;      ///< lastClockTick_ seeded from the live sim
  std::chrono::steady_clock::time_point lastBreakT_{}, lastPlaceT_{};
  // 2026-10-16: one-item stacks consumed by receipted Place edits, oldest first;
  // a rejected receipt refunds the front (refundRejectedPlaces).
  std::deque<ItemStack> placeRefunds_;
  void refundRejectedPlaces();
  int lastPlaceHit_[3] = {INT32_MIN, INT32_MIN, INT32_MIN};
  int placeLockAxis_ = -1, placeLockPerp_ = 0, placeLockY_ = 0;  // 2026-10-31 (JD): held-place line lock
  int lastBridge_[3] = {INT32_MIN, INT32_MIN, INT32_MIN};
  IRenderer& renderer_;
  cResourceMgr& resources_;
  cSoundDirector sound_;
  std::vector<SimEvent> simEvents_;
  Vec3 benchCenter_{};
  Vec3 benchCam_{};  ///< Phase 4 (JD 6:30): last bench camera pose; the bench streamer follows this, not the locomotion body
  std::string benchPath_ = "orbit";
  FramebufferLayout fb_;  // --bench-scene framebuffer placement (flyover path); size 0 = none
  std::unique_ptr<cBenchStreamer> stream_;
  std::unique_ptr<cChunkStreamer> streamer_;  // player-centered streaming (real game path)
  double uploadMs_ = 0;
  size_t uploadBytes_ = 0;
  // 2026-11-09 next-build-2 (GUI diagnosis Section 6, spike record): the
  // last update's upload commit shape... items landed, and the largest
  // atomic sim-flush batch that landed whole in that frame (0 = none).
  // Read by the app loop's spike line; plain values, no behavior.
  int uploadItems_ = 0;
  int uploadBatchMax_ = 0;
  double lastUiSplitMs_ = 0;
  // GUI lag round 2 (2026-11-10): drain/commit instrumentation for
  // uploadMeshes (see the accessors near lastUiSplitMs). Written in
  // uploadMeshes, read by main's spike record. No behavior.
  double lastMeshDrainMs_ = 0, lastMeshCommitMs_ = 0;
  int lastMeshDrained_ = 0, lastMeshBatchLanded_ = 0;
  size_t lastMeshDrainVerts_ = 0, lastMeshBatchVerts_ = 0;
  std::unique_ptr<cWorldSave> save_;
  Clock::time_point lastSave_ = Clock::now();
  bool benchSet_ = false;
  std::vector<SoundEvent> soundBuf_;
  cJobPool& pool_;
  cDimensionReg registry_;
  Dimension* active_ = nullptr;
  cPlayer player_;
  // --- IPlayer integration (player_adapter.h) ---
  cGamePlayerAdapter playerAdapter_;  ///< engine IPlayer view of player_: vitals + stats
  uint64_t lastVitalsTick_ = 0;      ///< sim worldTick the vitals last advanced to
  // --- Locomotion (player_locomotion.h): the local player's Entity body +
  // the engine cLocomotionController stepping it, once per sim tick. ---
  PlayerLocomotion locomotion_;
  uint64_t lastLocomotionTick_ = 0;  ///< sim worldTick the body last advanced to
  litholattice::LocomotionInput pendingLocomotionInput_{};  ///< frame's intent, consumed per tick
  // --- Sprite system integration (sprite_host.h): presentation only ---
  cSpriteHost spriteHost_;            ///< sprite registry + live instances (game thread)
  uint64_t lastSpriteTick_ = 0;      ///< sim worldTick the sprites last advanced to
  // --- Deceitor director: declared BEFORE aiHost_ so the cAiWorld (and
  // the brains it owns) destructs first â€” brains unregister from the
  // director on destruction, so the director must still be alive then.
  cDeceitorDirector deceitorDirector_;
  std::map<uint32_t, uint64_t> deceitorMirrors_;  ///< cAiWorld mob id -> mirror entity id
  /// P2 mirror interpolation (presentation only -- the cAiWorld cell stays
  /// authoritative; this state is never saved and never feeds the sim).
  /// One leg = the glide from the previous cell centre to the current
  /// one over one AI-tick period; kJump/kGap legs add a parabolic arc.
  struct DeceitorMirrorAnim {
    litholattice::ai::Cell cell{};  ///< cell the mirror is synced to
    double fx = 0, fy = 0, fz = 0;   ///< leg start (world coords)
    double tx = 0, ty = 0, tz = 0;   ///< leg end (cell centre)
    float phase = 1.f;               ///< 0..1 along the leg (1 = arrived)
    litholattice::ai::StepTag tag = litholattice::ai::StepTag::kStepStart;  ///< arriving step's tag
  };
  std::map<uint32_t, DeceitorMirrorAnim> deceitorMirrorAnim_;
  // --- cAiWorld integration (ai_host.h): production owner, gated by the
  // ai.enabled cvar (default off). Bound per dimension in loadActive();
  // advanced by elapsed sim world ticks in update() (ENT-2 discipline).
  cAiHost aiHost_;
  uint64_t lastAiTick_ = 0;          ///< sim worldTick the cAiWorld last advanced to
  Vec3 spawnPoint_{20.f, 92.f, 20.f};  ///< dimension spawn; `kill` respawns here
  /// 2026-10-25 (JD J3): named teleport targets for this dimension load (testarea, circuits, Examples pads).
  std::vector<std::pair<std::string, Vec3>> tpMarks_;
  struct BenchBox { int x0, z0, x1, z1; };  // 2026-10-30 (JD): zigzag bench, placed Examples pads
  std::vector<BenchBox> benchExamples_;
  int benchFloorY_ = 0;
  cWorldPicker worldPicker_;                  ///< 2026-10-30 (JD): menu Play -> world + spawn
  std::optional<WorldLoadRequest> worldLoad_;  ///< 2026-10-31: in-process Load World request (main.cpp acts on it)
  cAsyncSaver saver_;                          ///< 2026-10-31: Save World (lock-free async save)
  cWorldLoadSoak soak_;                        ///< 2026-10-31: LL_WORLD_LOAD_SOAK test hook (default off)
  std::optional<cAsyncSaver::Result> lastAsyncSave_;
  void openWorldPicker();
  void pickWorld(int world, const std::string& spawn);
  void openWorldFolderDialog();
  void pollWorldFolderDialog();  ///< 2026-10-31: lock-free poll of the off-thread folder dialog     ///< 2026-10-31 (JD): picker "Open world folder..."
  PlayerMeta playerMetaForSave();   ///< 2026-10-31: the player.lat record of the current player
  std::vector<cWorldSave::TpMark> userTpMarks_;  ///< 2026-10-25 (JD): /tp set spots, every dimension; persisted in level.json
  std::string teleportCmd(const std::vector<std::string>& args);
  litholattice::cEntityMgr entities_{[this](uint32_t type) {
    return makeEntityBehavior(
        type,
        [this](int x, int y, int z) { return sim_ && blockRegistry().has(sim_->getBlock(x, y, z), kSolid); },
        [this](int x, int y, int z) {
          return sim_ ? blockRegistry().def(sim_->getBlock(x, y, z)).friction : 0.8f;
        },
        {}, [this](int x, int y, int z) { return sim_ && blockIsLiquid(sim_->getBlock(x, y, z)); });  // P-2: items float
  }};  ///< item drops etc. (game thread)

  /// P5 Render Ingress byte weight: vertex payload + a fixed per-mesh overhead.
  struct MeshIngressBytes {
    template <class M>
    size_t operator()(const M& m) const { return m.verts.size() * sizeof(m.verts[0]) + 256; }
  };
  litholattice::cIngress<ChunkMesh, MeshIngressBytes> meshOut_{1024};  // P5 Render Ingress: mesh workers -> main (MPSC ring, lossless overflow)
  std::unique_ptr<class WaferPulseCache> pulseCache_;  // wafer ground-pulse biome weights (null if no wafer)
  std::unordered_map<uint64_t, std::vector<ChunkMesh>> meshBatches_;  // Cortana 2026-10-15 1350: phase-boundary publish staging
  std::deque<int64_t> pendingOrder_;                    // sub-chunks waiting, oldest first
  std::unordered_map<int64_t, ChunkMesh> pendingMesh_;  // newest waiting mesh per sub-chunk (older ones dropped)
  std::unordered_map<int64_t, uint64_t> meshSeq_;  // latest uploaded seq per sub-chunk
  int64_t uploadSortedAt_ = INT64_MIN;  // player-chunk key at the last nearest-first sort of pendingOrder_
  size_t uploadSortedSize_ = 0;         // pendingOrder_ size at that sort (reset with the queue on world switch)
  int uploadSortedSector_ = cStreamRank::kNoDirection;  // R3: view sector at that sort
  /// R3 (B7): the rank around the player's column and view sector (game thread).
  cStreamRank playerRank() const;
  // PBR (BinaryQuad) path queues (phase 2, Worker 3): only used when the mesher option
  // selects BinaryQuad. The classic path above keeps serving every non-cube shape even
  // then (the binary mesher's scope is opaque cubes), so both queues drain each frame.
  litholattice::cIngress<litholattice::PbrChunkMesh, MeshIngressBytes> pbrMeshOut_{1024};  // P5 Render Ingress (PBR): workers -> main
  std::deque<int64_t> pbrPendingOrder_;                             // sub-chunks waiting, oldest first
  std::unordered_map<int64_t, litholattice::PbrChunkMesh> pbrPendingMesh_;  // newest per sub-chunk
  std::unordered_map<int64_t, uint64_t> pbrMeshSeq_;  // latest uploaded PBR seq per sub-chunk
  cViewMeshGate viewGate_;          ///< Archie Phase 1: nothing beyond the view radius meshes
  bool viewGateBacklog_ = false;    ///< entering columns left over from a bounded sweep
  uint64_t viewGateRetired_ = 0, viewGateEntered_ = 0;  ///< Phase 1 counters (log)
  uint64_t viewGateKept_ = 0, viewGateHandovers_ = 0;   ///< R4 (B8) counters (log)
  bool useBinaryMesher_ = false;  // mesher option, boot-applied from settings_.mesher
  // Design B Stage 2: ring spill. When the renderer's lock-free upload ring
  // is momentarily full, an upload the ring refused waits here â€” in order,
  // retried first at the next uploadMeshes() â€” and is never dropped:
  // tombstones (retireColumnMeshes) ride the same queue, and a dimension
  // switch discards the queue only together with the ring clear that wipes
  // the store anyway (counted in ringSpillDroppedOnSwitch_).
  struct RingSpill {
    uint8_t kind = 0;  // 0 = classic chunk mesh, 1 = PBR chunk mesh, 2 = clear-all
    ChunkMesh mesh;
    litholattice::PbrChunkMesh pbr;
  };
  std::deque<RingSpill> ringSpill_;
  uint64_t ringSpillEvents_ = 0;         // uploads that had to spill (ring full at first try)
  size_t ringSpillMax_ = 0;              // deepest the spill ever got
  uint64_t ringSpillDroppedOnSwitch_ = 0;  // spilled entries discarded by a dimension switch
  void spillRingUpload(uint8_t kind, ChunkMesh&& mesh, litholattice::PbrChunkMesh&& pbr);
  // PBR analytic lights (phase 2, Worker 3): the provider scans chunk material bytes
  // around the focus column; the collector reduces to K-nearest per frame for the
  // chunk_pbr uniforms (FrameParams::pbrLights). Null when no material registry.
  std::unique_ptr<litholattice::cChunkEmissiveProvider> emissiveProvider_;
  litholattice::LightCollector lightCollector_;  // default K = 8 (JD's locked decision)
  // PBR light reduction cache (audit App.A/game-1): frameParams() reuses this
  // vector and skips the emissive-cache walk unless the camera column moved or
  // the provider's cache was rebuilt. Mutable: frameParams() is const.
  mutable std::vector<litholattice::AnalyticLight> pbrLightsCache_;
  mutable int pbrLightsCamCx_ = 0, pbrLightsCamCz_ = 0;
  mutable bool pbrLightsValid_ = false;
  cSimThread::SubmitMesh submitMesh_;
  std::unique_ptr<cSimThread> sim_;               // owns active_->chunks while alive
  uint32_t epoch_ = 0;
  uint64_t initSeq_ = 0;
  // ---- Thread Handshake P5: chunk handshake (see chunk_ingress.h) ----
  litholattice::cSimPromotionBudget promoteBudget_;   ///< 4 promotions per sim tick, at the commit barrier
  bool spawnSpiral_ = false;                          ///< initial spawn spiral still filling (ring-center bypass on)
  litholattice::cBorderRing borderRing_;              ///< lazy border ring: neighbor columns to re-mesh
  // P8 (Archie Q7): render-only MeshHash, logged at quiescence when LL_MESH_HASH=1.
  litholattice::cMeshHashLedger meshLedger_;
  bool meshHashOn_ = [] { const char* e = std::getenv("LL_MESH_HASH"); return e && *e == '1'; }();
  std::atomic<int> meshInFlight_{0};                  ///< mesh jobs submitted, not yet pushed to Render Ingress
  uint64_t meshHashLogged_ = 0;
  void p8MeshHashCheck();
  litholattice::cDeferredFreeQueue<litholattice::ChunkColumn> deferredFree_;  ///< evicted columns, freed outside the sim hold
  cSignalInitTickets signalInit_;  ///< t46u 6b-3b: disk columns whose tick-side signal init is still queued
  cSignalBearingQuery evictSignalQuery_{/*withExport=*/true};   ///< t46u 6b-4a': non-blocking signal-bearing query (evict)
  cSignalBearingQuery demoteSignalQuery_{/*withExport=*/false};  ///< t46u 6b-4a': same, for far-demote picks
  int streamCx_ = 0, streamCz_ = 0;                    ///< centre of the last pumpStreaming request pass
  bool streamCentreSet_ = false;
  cL0NeighbourHold l0Hold_;                           ///< fix (b): first L0 mesh waits for edge neighbours
  uint64_t streamPumps_ = 0;                          ///< deferred-free fence clock (pumpStreaming calls)
  std::unordered_map<uint64_t, int> colMeshPending_;  ///< promoted column -> first-wave meshes not yet returned (MESH)
  std::unordered_set<uint64_t> farPromoteAsked_;      ///< Q12: demoted columns already re-requested
  uint64_t farDemoted_ = 0, farPromoted_ = 0;         ///< Q12 diagnostics
  std::vector<uint64_t> stagedRenderCols_;            ///< columns whose first wave is back, awaiting upload (STAGED_RENDER)
  uint64_t p5Promoted_ = 0, p5Bypassed_ = 0, p5BorderRemeshed_ = 0;  ///< diagnostics
  void p5ResetChunkHandshake();
  void p5NoteMeshArrived(int cx, int cz);
  void p5AdvanceStagedRender();
  // JD ruling t3366u: while the sim is paused the RENDER pipeline stops (no Render
  // Ingress pops, no GPU uploads); generation, CPU meshing, Sim Ingress admission and
  // residency continue. The ingress queues without dropping; past the cap, NEW mesh
  // submissions are deferred as sub-chunk keys (backpressure) and resubmitted later.
  static constexpr size_t kRenderIngressCapBytes = size_t(256) << 20;  ///< 256 MB queued while paused
  // In-game pauses only: the launch menu (MenuPaused before Play) keeps rendering so its
  // world backdrop still fills in behind the menu.
  bool renderPaused() const {
    return sim_ && paused_ && cfg_.pauseSim && !benchSet_ && menuState_ == MenuState::Playing;
  }
  size_t renderIngressBytes() const { return meshOut_.queuedBytes() + pbrMeshOut_.queuedBytes(); }
  bool meshBackpressured() const { return renderPaused() && renderIngressBytes() >= kRenderIngressCapBytes; }
  /// Game thread, under the sim hold: snapshot + submit, or defer the key under backpressure.
  void submitMeshAt(int cx, int sy, int cz);
  // ---- Thread Handshake P6: swarm and locality (game thread unless noted) ----
  /// Snapshot one sub-chunk into a slab buffer (game thread; the map must be held).
  std::shared_ptr<MeshInput> snapshotSlab(int cx, int sy, int cz, bool ownFresh = true);
  /// Locality: snapshot every non-empty sub-chunk of a column (deferring keys under
  /// backpressure) and mesh them all in ONE pool job, on one worker, in order.
  void submitMeshColumn(int cx, int cz);
  void startFirstMesh(int cx, int cz);  ///< fix (b): first L0 mesh of a promoted column
  /// R2 (B3): everything a mesh job reads besides its MeshInput, captured by value when the job is
  /// submitted. The worker never looks at live game state (active_, the renderer) while it runs, so
  /// a dimension switch or a settings change can't reach into a job that is already queued.
  struct MeshJobParams {
    std::shared_ptr<const BlockMaterials> mats;
    class WaferPulseCache* pulse = nullptr;       // set only while the Wafer is the active dimension
    bool greedy = false;                           // renderer_.prefersGreedyMeshes() at submit
    bool binary = false;                           // BinaryQuad mesher option
  };
  /// Owner-thread side: the parameters for a job submitted now (game thread, or the sim thread
  /// while it owns the map; both only run while active_ is stable).
  MeshJobParams meshJobParams() const;
  /// The mesh job body (any pool worker). Reads only `in` and `p`.
  void meshOne(const std::shared_ptr<MeshInput>& in, const MeshJobParams& p);
  /// Title-screen pre-warm: wake every pool worker once (first-touch stacks and allocator
  /// arenas), grow the snapshot slab to the radius-8 tier, request the radius-8 ring.
  void prewarmSwarm();
  litholattice::cSlab<MeshInput> meshSlab_{litholattice::kSlabRadius4Columns};  // radius-4 tier at boot
  litholattice::cSlackHarvester slack_{0.25, 6, 2.0};
  // R2: the frame-aware request grant's state. Members (were function statics), reset per world load.
  float streamEmaMs_ = 16.7f;  ///< R4: EMA of the busy part of the frame (wall time minus the pacer's sleep)
  int streamLastGrant_ = -1;
  uint64_t streamGrantLogPump_ = 0;  ///< R4: the grant log is rate-limited (10 levels now)
  double frameTargetMs_ = cFrameBudget::kDefaultTargetMs;  ///< R4: 1000 / cap, or 1000 / 30 uncapped
  double pacerSleptMs_ = 0;
  uint64_t admitStepFrame_ = 0;  ///< R2 deterministic mode: pump count at the last admission-radius step
  bool swarmPrewarmed_ = false;
  uint64_t p6ColumnJobs_ = 0, p6SlackSlices_ = 0;
  // Thread Handshake P7: the wafer reveal (render-only). Started on world load, paced by
  // column readiness, clock frozen by renderPaused(), instant under --bench.
  void pumpWaferReveal(float dt);
  litholattice::cWaferRevealPacer reveal_;
  litholattice::WaferRevealFrame revealFrame_;
  bool revealPending_ = false, revealLoggedOnline_ = false, revealSky_ = false;
  float skyFade_ = 1.f;  ///< sky brightness while the reveal runs: dark until SKYBOX_ONLINE, then fades in
  std::deque<int64_t> deferredMeshOrder_;
  std::unordered_set<int64_t> deferredMeshKeys_;
  uint64_t renderPausedFrames_ = 0, meshDeferredTotal_ = 0;
  int totalSubs_ = 0, meshesReceived_ = 0;
  size_t chunkColumns_ = 0;

  cInputSystem& input_;
  cAudioEngine& audio_;
  // cCamera system: owns the engine cCameraMgr (player/orbit/follow/free
  // view cameras + MCP pilot cameras). Presentation only â€” declared before
  // the MCP members because mcpSenses_ borrows its cPilotCameraRig.
  cGameCameraController camera_;
  // MCP player control (model pilots the local player through the real input
  // path). Wired into the cMcpServer by Game::mcpBackends() under
  // main.cpp's --mcp-stdio flag (P2c, F-M9).
  cMcpPlayerController mcpController_;
  cMcpPlayerView mcpView_;
  // MCP pilot senses (agent vision + agent audio). Fed the mesh cache from
  // uploadMeshes() and cvar values from update(); wired the same way
  // (McpBackends{.vision, .audio}).
  cMcpSensesAdapter mcpSenses_;
  const void* nemoObjectsFor_ = nullptr;  ///< block materials the Nemo View object table was built from
  // MCP server wiring (P2c, F-M9): non-owning; main.cpp owns the server
  // and its stdio thread and clears this (setMcpServer(nullptr)) before
  // destroying either. Only the `mcp` console commands read it, on the
  // game thread; the server's grant/revoke methods are internally
  // synchronized for exactly this cross-thread approval flow.
  litholattice::mcp::cMcpServer* mcpServer_ = nullptr;
  bool mcpCommandsRegistered_ = false;
  // Projectile system (P2c, ENT-1): engine manager, game-thread owned,
  // ticked per world tick in updateEntities(). Seams are installed in
  // the constructor (voxel over sim block reads, chunk requests over the
  // active dimension's streamer â€” both resolve the CURRENT sim/streamer
  // at call time, so dimension switches need no rewiring).
  litholattice::cProjectileMgr projectiles_;
  std::atomic<bool> mcpControllable_{false};  ///< set per frame in update()
  std::atomic<uint64_t> mcpWorldTick_{0};  ///< Cortana 2026-10-15 1658: F-2 [GAME] sim tick copy for MCP threads, set per frame in update()
  // Archie PlayI: COW MCP player snapshot; no mutex. 2026-11-16 (JD): CowPublished, since
  // atomic<shared_ptr> hid a lock. Starts as a default GameState (what the null case returned).
  mutable CowPublished<cMcpPlayerView::GameState> mcpPlayerState_{new cMcpPlayerView::GameState()};
  cMcpPlayerView::GameState snapshotMcpPlayer() const;
  MenuState menuState_ = MenuState::cMenu;
  GameSettings settings_;
  std::function<void(DisplayMode)> displayApplier_;
  float menuCameraT_ = 0.f;                       // cinematic dolly clock
  float menuCamX_ = 0.f, menuCamY_ = 0.f, menuCamZ_ = 0.f;  // dolly anchor (the spawn)
  // cMenu entry snapshot (audit MEDIUM): the dolly overwrites player_.pos every
  // menu frame; enterPlay() restores this so save/restore survives the menu.
  Vec3 menuReturnPos_{};
  float menuReturnYaw_ = 0.f, menuReturnPitch_ = 0.f;
  bool menuSnapshotValid_ = false;
  static constexpr int kHotbarSize = kHotbarSlots;   // == kInvHotbarSlots (16)
  // P2b-1: NO separate hotbar array. The hotbar IS player_.inventory's
  // hotbar slots; hot_ is only the selection index into them. (The old
  // array's default contents survive as the fresh-spawn creative kit,
  // seeded as real stacks in the Game constructor.)
  int hot_ = 1;
  bool captured_ = true;
  bool pausedByFocus_ = false;  // MenuPaused was entered by an alt-tab focus loss (click-in-world resumes)
  bool freePointerDown_ = false;  ///< Cortana 2026-10-15 1213: a PointerDown no widget consumed this frame
  bool paused_ = false, quit_ = false;  // paused_ mirrors pauseState_ != Playing (owned by the pause machine)
  bool restart_ = false;  // renderer switch: relaunch after the exit save (JD 2026-10-24)
  PauseState pauseState_ = PauseState::Playing;
  bool frameHashStepped_ = false;  ///< --frame-hash: force sim paused; stepTicks drives capture
  bool ioTickHold_ = false;        ///< B9: exit save / world load in progress: sim ticks held
  bool hsFreezeLogged_ = false;  ///< the current HsFrozen episode was reported once
  uint64_t lastEntitiesTick_ = 0;  ///< sim worldTick the live entities last advanced to (ENT-3)
  /// Floating items (2026-11-10): per-entity pickup delay in world
  /// ticks (design 2.5) and the data-bearing payload table (design
  /// 2.9 (a)). Both keyed by entity id, game thread only, entries
  /// reaped when their entity dies (updateEntities sweep).
  std::unordered_map<uint64_t, uint32_t> entityPickupDelay_;
  std::unordered_map<uint64_t, ItemStack> entityPayloads_;
  // Dropped items P-1..P-6 (2026-11-17). Game thread only.
  /// One world tick of item rules, in entity-id order: magnet, physics (every entity), pickup
  /// delays, merge, pickup.
  void stepItemsTick(uint64_t worldTick, double dt, int tps);
  void pickupItems(uint64_t worldTick, int tps);  ///< P-4 (also runs once per frame while paused)
  void ensureItemModels();
  uint32_t itemDelayTicks(bool fromPlayer) const;
  int itemTps() const;
  std::filesystem::path droppedItemsPath(const std::string& dimension) const;
  bool saveDroppedItems(std::string* err);  ///< P-6: <save>/items.<dimension>.lat, items in id order
  void loadDroppedItems();
  void spawnItemShowcase();  ///< LL_ITEM_SHOWCASE=1: every modelled item in rows in front of the player
  itemdrop::cModelIndex itemModels_;
  bool itemModelsBuilt_ = false;
  bool itemShowcasePending_ = false;
  int itemShowcaseWait_ = 0;  ///< frames after load before the showcase spawns (the bench pose settles first)
  /// P-4 pickup flight: a collected item keeps drawing for kPickupFlightSeconds while it flies
  /// to the player. Presentation only (not saved, not part of any hash).
  struct PickupFlight {
    uint64_t id = 0;
    uint32_t item = 0, count = 0, ageTicks = 0;
    double x = 0, y = 0, z = 0;
    uint64_t startTick = 0;
  };
  std::vector<PickupFlight> pickupFlights_;
  uint64_t itemWorldTick_ = 0;  ///< world tick the item rules last ran at (draw time base)
  /// P-5 interpolation: each item's position before the last item tick (id order), and when that
  /// tick ran on the frame clock. Presentation only.
  struct ItemPrevPos { uint64_t id = 0; double x = 0, y = 0, z = 0; };
  std::vector<ItemPrevPos> itemPrev_;
  std::chrono::steady_clock::time_point itemTickAt_{};
  bool showControls_ = true;   // F4: controls panel (ToggleControls)
  bool showHud_ = true;        // F1: hide all HUD (like Minecraft)
  bool showDebug_ = true;      // F3: stats overlay
  bool inventoryOpen_ = false; // E: creative block palette
  bool typing_ = false;        // keyboard block typing capture
  int typeX_ = 0, typeY_ = 0, typeZ_ = 0;
  std::string typed_;
  bool postViewOpen_ = false;  // 2026-10-25 Post viewer overlay (plain right click on a post)
  int postViewX_ = 0, postViewY_ = 0, postViewZ_ = 0;
  bool postTyping_ = false;    // 2026-10-25 Posts: the typing capture edits a post (not a Keyboard)
  double lastJumpTap_ = -1.0;  // double-tap Jump detection
  float fps_ = 0, frameAvgMs_ = 0, frameMaxMs_ = 0;
  mutable cTickStabilityMeter tickMeter_;  ///< 2026-10-31 (JD): HUD tick stability readout
  float p99Ms_ = 0, updateMs_ = 0, gpuMs_ = -1.f;  ///< F56-4 perf readout
};

}  // namespace lithokraft
