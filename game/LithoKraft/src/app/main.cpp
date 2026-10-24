// Cortana 2026-10-15 0935/1045: ALT-TAB, LAUNCH-FOCUS focus->capture->topmost
// LithoKraft: application entry. Parses options and wires the modules together:
// platform (window + GLFW input backend) -> input (actions) -> render backend (software, the
// default: the CPU baseline every machine runs; gl33, or Vulkan with --renderer vulkan,
// falling back to gl33) -> game.
// Input runs on the main thread only (see docs/ARCHITECTURE.md, "Input").
#include "litholattice/core/lock_timing.h"
#include "litholattice/render/mesher.h"  // Cortana 2026-10-15 1345: --verify-mesh
#include "litholattice/procgen/silicon_cities.h"  // --cities on|off
#include <atomic>
#include <charconv>
#include <mutex>
#include <system_error>
#if !defined(_WIN32)
#include <cerrno>
#include <poll.h>
#include <unistd.h>
#endif
#if defined(_WIN32)
#include <io.h>  // _isatty (wheel-ink console check, 2026-10-18 JD)
#include <process.h>  // _spawnv: renderer-switch relaunch (JD 2026-10-24)
#include <cerrno>
#endif
#include <algorithm>
#include <cstdio>
#include "litholattice/core/process_memory.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <thread>
#include <chrono>  // 0.0.1.56 F56-11
#include "litholattice/core/timer_resolution.h"  // 0.0.1.57 F57-1
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "litholattice/assets/mod_manifest.h"
#include "litholattice/assets/mod_signature.h"
#include "litholattice/procgen/struct_registry.h"
#include "litholattice/assets/image.h"
#include "noise_preview.h"
#include "scripts_dir.h"
#include "selftest.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>  // build 50: emscripten_set_main_loop_arg for the interactive web present fix
#endif
#include "perf_headless.h"
#include "handshake_test.h"
#include "control_latency.h"
#include "control_stream_test.h"
#include "demo_record_test.h"
#include "litholattice/audio/audio_engine.h"
#include "litholattice/audio/audio_output.h"
#include "litholattice/core/log.h"
#include "litholattice/core/threading.h"
#include "litholattice/core/time.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/game/actions.h"
#include "lithokraft/game/game.h"
#include "lithokraft/game/build_version.h"  // 2026-10-16 (JD): Alpha build counter
#include "build_counter.h"  // 2026-10-18 (JD): rolling build counter (8 wheels, base 64; generated per build)
#include "lithokraft/game/modWarnings.h"  // 2026-10-18 (JD): boot-log warnings for unofficial mods (default-ON)
#include "lithokraft/game/world_save.h"  // SV-H2: cWorldSave::readWorldSeed (level.json) before Game construction
#include "lithokraft/game/resource_manager.h"
#include "lithokraft/game/settings.h"
#include "litholattice/input/input_system.h"
#include "litholattice/platform/window.h"
#include "litholattice/render/pbr_materials.h"
#include "litholattice/render/renderer.h"
#include "litholattice/mcp/server.h"  // cMcpServer + IStdioTransport (--mcp-stdio, P2c F-M9)
#include "litholattice/render/frame_pacer.h"  // cFramePacer (render.fps_cap drive, D-M3)
#include "litholattice/render/software_presenter.h"
#include "litholattice/render/software_renderer.h"
#include "litholattice/render_vk/vulkan_renderer.h"
#include "litholattice/core/threeway.h"  // LL_THREEWAY (Proposed by JD; co-implemented by JD and Cortana)
#include "litholattice/core/tick_profile.h"  // LL_TICK_PROFILE (measure only; Proposed by JD; co-implemented by JD and Cortana)
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <span>
#include <thread>
#include <type_traits>
#include <vector>

#ifndef LITHOKRAFT_SOURCE_DIR
#define LITHOKRAFT_SOURCE_DIR ""
#endif
#ifndef LITHOKRAFT_ENGINE_ASSETS_DIR
#define LITHOKRAFT_ENGINE_ASSETS_DIR ""
#endif
#ifndef LITHOKRAFT_VERSION
#define LITHOKRAFT_VERSION "dev"
#endif

using namespace lithokraft;

#if !defined(_WIN32)
// MCP stdio transport for --mcp-stdio (P2c, F-M9): raw-fd, poll-based.
// Two properties the engine's cFdStdioTransport does not have, both needed
// for a clean in-game wiring:
//  - it writes to a SAVED copy of the real stdout fd, so main() can point
//    fd 1 at stderr for the process's log output (logInfo writes to
//    stdout and would corrupt the JSON-RPC stream) while MCP responses
//    still reach the real stdout, byte-clean;
//  - readLine() polls with a timeout and honours requestStop(), so the
//    game can shut the server thread down deterministically on exit
//    (join, never detach) instead of stranding it in a blocking read.
class cMcpStdioTransport final : public litholattice::mcp::IStdioTransport {
 public:
  explicit cMcpStdioTransport(int outFd) : outFd_(outFd) {}
  void requestStop() { stop_.store(true, std::memory_order_relaxed); }

  bool readLine(std::string& out) override {
    for (;;) {
      if (const size_t nl = buffer_.find('\n'); nl != std::string::npos) {
        out = buffer_.substr(0, nl);
        buffer_.erase(0, nl + 1);
        if (!out.empty() && out.back() == '\r') out.pop_back();
        return true;
      }
      if (stop_.load(std::memory_order_relaxed)) return false;
      pollfd pfd{};
      pfd.fd = STDIN_FILENO;
      pfd.events = POLLIN;
      const int r = ::poll(&pfd, 1, 100);
      if (r < 0) {
        if (errno == EINTR) continue;
        return false;
      }
      if (r == 0) continue;  // timeout: re-check the stop flag
      char chunk[8192];
      const ssize_t n = ::read(STDIN_FILENO, chunk, sizeof chunk);
      if (n < 0) {
        if (errno == EINTR) continue;
        return false;
      }
      if (n == 0) {  // EOF: a trailing unterminated line still counts
        if (buffer_.empty()) return false;
        out = std::move(buffer_);
        buffer_.clear();
        if (!out.empty() && out.back() == '\r') out.pop_back();
        return true;
      }
      buffer_.append(chunk, size_t(n));
    }
  }

  void writeLine(std::string_view line) override {
    std::lock_guard<std::mutex> lk(writeMutex_);
    std::string out(line);
    out.push_back('\n');
    const char* p = out.data();
    size_t left = out.size();
    while (left > 0) {
      const ssize_t n = ::write(outFd_, p, left);
      if (n < 0) {
        if (errno == EINTR) continue;
        return;  // peer gone: drop the line, the read side will see EOF/stop
      }
      p += n;
      left -= size_t(n);
    }
  }

 private:
  int outFd_;
  std::atomic<bool> stop_{false};
  std::string buffer_;
  std::mutex writeMutex_;
};
#endif  // !_WIN32

namespace lithokraft {

/// The window's OS clipboard for the UI.
class cWindowClipboard final : public ui::IClipboard {
 public:
  explicit cWindowClipboard(Window& w) : window_(w) {}
  std::string text() const override { return window_.clipboardText(); }
  void setText(const std::string& text) override { window_.setClipboardText(text); }

 private:
  Window& window_;
};

// Pack directories: the engine base pack (assets/: pack.json + shaders/) and the game pack
// (LithoKraft/assets/: ui/, worldgen/, art packs in packs/). Looked up next to the exe (shipped
// layout: <exe dir>/assets and <exe dir>/LithoKraft/assets), else in the source tree. Empty if not found.
struct AssetDirs {
  std::string engine, game;
};

AssetDirs findAssetDirs(const char* argv0) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path exeDir = fs::absolute(fs::path(argv0), ec).parent_path();
  const auto first = [&ec](std::initializer_list<fs::path> candidates) -> std::string {
    for (const fs::path& a : candidates)
      if (fs::is_regular_file(a / "pack.json", ec)) return a.lexically_normal().string();
    return {};
  };
  return {first({exeDir / "assets", fs::path(LITHOKRAFT_ENGINE_ASSETS_DIR)}),
          first({exeDir / "LithoKraft" / "assets", fs::path(LITHOKRAFT_SOURCE_DIR) / "assets"})};
}

// mod.json: validate the game's own manifest and list <exe>/mods. Content mods are accepted
// silently (decision Q6) and returned for the pack stack; script / native mods wait for the
// permission prompt (not built yet).
std::vector<std::filesystem::path> scanMods(const char* argv0) {
  namespace fs = std::filesystem;
  std::error_code ec;
  const fs::path exeDir = fs::absolute(fs::path(argv0), ec).parent_path();
  const ModManifest engine = *ModManifest::parse(R"({"id":"litholattice","version":"0.1.0"})", nullptr);
  bool gameSealed = false;  // folded into the seal decision below
  for (const fs::path& p : {exeDir / "mod.json", fs::path(LITHOKRAFT_SOURCE_DIR) / "mod.json"}) {
    std::ifstream in(p, std::ios::binary);
    if (!in) continue;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string err;
    if (const auto game = ModManifest::parse(ss.str(), &err)) {
      logInfo("mods: game '%s' %s", game->id.c_str(), game->version.str().c_str());
      gameSealed = game->sealed;
    } else logError("mods: %s: %s", p.string().c_str(), err.c_str());
    break;
  }
  // Fail-closed key hygiene: the throwaway test key must never ship. In release
  // builds this is a hard boot refusal; in debug it's a loud warning (dev loop).
  // Runs before the mods/ early-return below so a stock install can't dodge it.
  if (litholattice::assets::bakedKeyIsTestKey()) {
#ifdef NDEBUG
    // Genuine boot refusal (not just "no mods"): returning an empty pack list
    // would let the game boot normally, defeating the fail-closed intent.
    logError("FATAL: baked-in public key is the retired TEST key — refusing to boot a release build. "
             "Install JD's real key (see mod_signature.h) and rebuild.");
    std::exit(1);
#else
    logError("WARNING: baked-in public key is the retired TEST key — mods will verify against it. "
             "Replace before beta (see mod_signature.h).");
#endif
  }
  // Trust anchor for structure signatures: JD's baked-in public key. Must be set before
  // any structure registers (mods load below). Set even when mods/ is missing so
  // JD-signed structures can verify Trusted on a stock install. Until set, no
  // structure can verify Trusted.
  litholattice::procgen::cStructureReg::instance().setTrustedPublicKeyHex(
      litholattice::assets::kBakedPublicKeyHex);
  // Note: no mods/ directory is NOT an early return — the seal decision below must
  // still run (release with zero mods seals, fail closed). An empty ModSet falls
  // through to the same decision logic.
  const bool haveModsDir = fs::is_directory(exeDir / "mods", ec);
  if (!haveModsDir) logInfo("mods: no mods/ directory — continuing with zero mods (seal decision still applies)");
  // Containment (P1b, Appendix E M1, layer 2): loadModDirectory already
  // skips a single hostile mod with an error entry, but the boot boundary
  // catches anything that still escapes — a crafted mod folder must never
  // abort boot; on a catastrophic scan failure we continue with zero mods
  // (and the seal decision below still applies, fail-closed).
  ModSet set;
  if (haveModsDir) {
    try {
      set = loadModDirectory(exeDir / "mods", {engine});
    } catch (const std::exception& ex) {
      logError("mods: mod scan threw (%s) — continuing with zero mods", ex.what());
    } catch (...) {
      logError("mods: mod scan threw an unknown exception — continuing with zero mods");
    }
  }
  for (const std::string& e : set.errors) logError("mods: %s", e.c_str());
  // Boot seal decision (certificate trust model, JD 2026-10-12):
  //   - sealed:true in any manifest -> seal (sealed wins, unchanged).
  //   - Release: open ONLY when every loaded mod is Verified and at least one
  //     verified mod opts into hot_swap (the live-ops case). Any unsigned mod
  //     (or no verified hot-swap mod) -> seal (fail closed).
  //   - Debug: open unless sealed (developer path, unchanged).
  // Sealing is a side effect on the global registry (per-mod scoping is future work).
  {
    const ModManifest* sealedBy = nullptr;
    bool allVerified = true;
    bool anyVerifiedHotSwap = false;
    for (const ModManifest& m : set.ordered) {
      if (m.sealed && !sealedBy) sealedBy = &m;
      if (m.signatureStatus != SignatureStatus::Verified) allVerified = false;
      if (m.hotSwap && m.signatureStatus == SignatureStatus::Verified) anyVerifiedHotSwap = true;
      // Trust-tier log line: unsigned mods run sandboxed, clearly labeled.
      if (m.signatureStatus == SignatureStatus::Unsigned)
        logInfo("mods: '%s' is unsigned - sandboxed", m.id.c_str());
      else if (m.signatureStatus == SignatureStatus::Verified)
        logInfo("mods: '%s' signature verified", m.id.c_str());
      // 2026-10-18 (JD, default-ON): a loaded unofficial mod also gets its
      // provenance line at WARN in the boot log — elevate + flag, never
      // block. A mod in this set is Verified or Unsigned (Invalid signatures
      // are refused upstream by loadModDirectory and never load). The
      // mod.json schema has no provenance field, so an unsigned mod is
      // labeled Community — the one label boot can truthfully give; the
      // SelfMade / AiAssisted lines wait in modWarnings.h for a schema field.
      if (m.signatureStatus == SignatureStatus::Unsigned)
        logWarn("mods: '%s' %s", m.id.c_str(),
                modWarnings::Warning(modWarnings::ModKind::Community).c_str());
    }
    bool seal = false;
    const char* reason = "";
    if (sealedBy != nullptr) {
      seal = true;
      reason = "sealed:true manifest";
    } else if (gameSealed) {
      seal = true;
      reason = "sealed:true in game manifest";
    }
#ifdef NDEBUG
    else if (allVerified && anyVerifiedHotSwap) {
      seal = false;
      reason = "release with verified hot-swap mod (live-ops)";
    } else {
      seal = true;
      reason = "release build (fail closed: no verified hot-swap mod)";
    }
#else
    else {
      seal = false;
      reason = "debug build (developer path)";
    }
#endif
    if (seal) {
      mutableBlockRegistry().content().seal();
      litholattice::procgen::cStructureReg::instance().seal();
      logInfo("mods: registry sealed (%s)", reason);
    } else {
      logInfo("mods: registry open for hot-swap (%s)", reason);
    }
  }
  for (const ModManifest& m : set.ordered)
    if (m.type != ModType::Content) logInfo("mods: '%s' is a %s mod; waiting for the permission prompt (not loaded yet)", m.id.c_str(), m.type == ModType::Script ? "script" : "native");
  return contentModPacks(set);
}

// Window icon from LithoKraft/assets/icons (the exe's own icon comes from lithokraft.rc).
void setWindowIcon(Window& window, const AssetDirs& assets) {
  if (assets.game.empty()) return;
  std::vector<Image> images;
  for (const int size : {16, 24, 32, 48, 64, 256}) {
    std::string err;
    const auto file = std::filesystem::path(assets.game) / "icons" / ("lithokraft_icon_dark_" + std::to_string(size) + ".png");
    if (auto img = Image::load(file, &err)) images.push_back(std::move(*img));
  }
  std::vector<IconPixels> px;
  for (const Image& i : images) px.push_back({i.width(), i.height(), i.pixels().data()});
  window.setIcon(px.data(), int(px.size()));
  if (px.empty()) logError("window icon: no LithoKraft/assets/icons/lithokraft_icon_dark_*.png found");
}

// Pack stack, lowest first: engine base, game pack, the art pack, content mods (dependency
// order), then --resource-pack folders in order.
std::vector<std::filesystem::path> packStack(const AssetDirs& assets, const std::string& pack,
                                             const std::vector<std::string>& extra,
                                             const std::vector<std::filesystem::path>& mods = {}) {
  std::vector<std::filesystem::path> dirs;
  if (!assets.engine.empty()) dirs.emplace_back(assets.engine);
  if (!assets.game.empty()) {
    dirs.emplace_back(assets.game);
    dirs.emplace_back(std::filesystem::path(assets.game) / "packs" / pack);
  }
  for (const auto& m : mods) dirs.push_back(m);
  for (const auto& e : extra) dirs.emplace_back(e);
  return dirs;
}

// Writable data root: the source tree when it exists (logs/, config/), else the exe's folder.
std::filesystem::path dataRoot(const char* argv0) {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path root = LITHOKRAFT_SOURCE_DIR;
  if (root.empty() || !fs::is_directory(root / "src", ec)) root = fs::absolute(fs::path(argv0), ec).parent_path();
  return root;
}

// Live log: <root>/logs/lithokraft.log.
std::string defaultLogPath(const char* argv0) { return (dataRoot(argv0) / "logs" / "lithokraft.log").lexically_normal().string(); }

// Bindings from <root>/config/input.json (created from the defaults on first run).
ActionMap loadBindings(const char* argv0) {
  const std::filesystem::path path = (dataRoot(argv0) / "config" / "input.json").lexically_normal();
  BindingsFile f = loadOrCreateBindings(path, defaultActionMap());
  switch (f.status) {
    case BindingsFile::Status::Created: logInfo("input: wrote default bindings to %s", path.string().c_str()); break;
    case BindingsFile::Status::Loaded: logInfo("input: bindings from %s", path.string().c_str()); break;
    case BindingsFile::Status::Invalid:
      logError("input: %s (%s); using defaults, file left untouched", path.string().c_str(), f.message.c_str());
      break;
  }
  for (const auto& w : f.warnings) logInfo("  input.json: %s", w.c_str());
  for (const auto& [a, b] : f.map.conflicts())
    logInfo("  input.json: %s and %s share an input", std::string(f.map.actions().name(a)).c_str(),
            std::string(f.map.actions().name(b)).c_str());
  return std::move(f.map);
}

/// The window and the renderer drawing into it (destroyed renderer first).
struct Display {
  std::unique_ptr<Window> window;
  std::unique_ptr<IRenderer> renderer;
  /// Software backend only: blits the CPU framebuffer into the window's back
  /// buffer before swap(); `software` is the same object as `renderer` then.
  std::unique_ptr<ISoftwarePresenter> presenter;
  cSoftwareRenderer* software = nullptr;
};

// Window + renderer for `backend` ("software", "gl33" or "vulkan"). Vulkan falls back to
// OpenGL 3.3 with a log line when the loader, a device or any init step is missing (its window
// is recreated for GL). Software renders on the CPU into its framebuffer; the window is still
// created for OpenGL, whose context the presenter borrows purely as the blit mechanism — if
// that present path cannot be built, software falls back to gl33 the same loud way.
Display createDisplay(const std::string& backend, bool vsync, bool validation, const std::string& present, const DisplayDesc& disp,
                      const std::string& vkArena, bool glArena) {
  Display d;
  if (backend == "software") {
    d.window = Window::create({1280, 720, "LithoKraft", vsync, GraphicsApi::OpenGL33, disp});
    if (d.window) {
      auto sw = std::make_unique<cSoftwareRenderer>();
      // The textured, diffuse-lit path (biome_capture does the same on its
      // instance); without it the renderer stays in the flat faceColor mode
      // the MCP vision path deliberately uses. Per-instance state: the MCP
      // adapter's own renderer is unaffected.
      sw->setPerPixelShading(true);
      d.software = sw.get();
      d.renderer = std::move(sw);
      d.presenter = createSoftwarePresenter(&Window::glProcAddress);
      if (!d.presenter) {
        logError("renderer: software present path unavailable; falling back to OpenGL 3.3");
        d.software = nullptr;
        d.renderer = createGL33Renderer(&Window::glProcAddress, glArena);
      }
    }
    return d;
  }
  if (backend == "vulkan") {
    d.window = Window::create({1280, 720, "LithoKraft", vsync, GraphicsApi::Vulkan, disp});
    std::string err = "no Vulkan window";
    if (d.window) {
      Window& w = *d.window;
      VulkanRendererDesc desc;
      desc.surface.instanceExtensions = w.vulkanInstanceExtensions();
      desc.surface.createSurface = [&w](void* instance) { return w.createVulkanSurface(instance); };
      desc.vsync = vsync;
      desc.presentMode = present;
      desc.vertexArena = vkArena;
      desc.validation = validation;
      d.renderer = createVulkanRenderer(desc, &err);
    }
    if (d.renderer) return d;
    logError("renderer: Vulkan unavailable (%s); falling back to OpenGL 3.3", err.c_str());
    d.window.reset();
  }
  d.window = Window::create({1280, 720, "LithoKraft", vsync, GraphicsApi::OpenGL33, disp});
  if (d.window) d.renderer = createGL33Renderer(&Window::glProcAddress, glArena);
  return d;
}
}  // namespace lithokraft

/// fopen for writing without MSVC's C4996 (fopen_s there).
inline std::FILE* openForWrite(const char* path) {
#if defined(_MSC_VER)
  std::FILE* f = nullptr;
  return fopen_s(&f, path, "w") == 0 ? f : nullptr;
#else
  return std::fopen(path, "w");
#endif
}

/// Bench recorder: frame times, the main-thread breakdown, sim tick ms, streaming and memory,
/// per second (log lines) and for the whole run (BENCH line + optional JSON).
struct BenchRun {
  std::vector<double> frame, input, update, upload, render, present, tick, uploadBytes;
  std::vector<double> secFrame;
  uint64_t memStart = 0, memPeak = 0, memEnd = 0;
  std::vector<double> minuteFrame, minuteRam;  // soak drift
  double minSum = 0; int minFrames = 0, minuteNo = 0;
  double secInput = 0, secUpdate = 0, secUpload = 0, secRender = 0, secPresent = 0, secBytes = 0; int secFrames = 0;
};

double avgOf(const std::vector<double>& v) { double s = 0; for (double x : v) s += x; return v.empty() ? 0.0 : s / double(v.size()); }

/// q-quantile (0..1) of `v` (sorted in place); 0 when empty.
double percentile(std::vector<double>& v, double q) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  return v[std::min(v.size() - 1, size_t(q * double(v.size() - 1) + 0.5))];
}

// 2026-10-18 (JD): wheel-ink rendering for the rolling build counter's startup
// line. Even-index wheels ink muted lavender, odd-index wheels warm gray, and
// any wheel that wrapped on the build that produced this binary inks brand
// orange (SelectOR bus orange) — the per-wheel ink is computed by
// tools/build_counter.py at roll time and carried in the generated header
// (kJdBuildCounterWheelInk). Ink is a console garnish only: the startup line
// carries ANSI truecolor when stdout is a real terminal (NO_COLOR honoured);
// pipes and captures — every bench and gate — keep the exact plain decimal
// string. (Where a log file is open in an interactive run it receives the same
// inked line; the plain string remains the canonical value everywhere else.)
static bool jdConsoleColor() {
  if (std::getenv("NO_COLOR")) return false;
#if defined(_WIN32)
  return _isatty(_fileno(stdout)) != 0;
#else
  return ::isatty(STDOUT_FILENO) != 0;
#endif
}

static std::string jdCounterDisplay() {
  if (!jdConsoleColor()) return kJdBuildCounterDisplay;
  std::string out;
  for (int i = 0; i < 8; ++i) {
    if (i) out += '.';
    const std::uint32_t ink = kJdBuildCounterWheelInk[i];
    char buf[64];
    std::snprintf(buf, sizeof buf, "\x1b[38;2;%u;%u;%um%d\x1b[0m",
                  unsigned((ink >> 16) & 0xFFu), unsigned((ink >> 8) & 0xFFu),
                  unsigned(ink & 0xFFu), kJdBuildCounterWheels[i]);
    out += buf;
  }
  return out;
}

struct RenderWorker {
      using Quad = std::remove_const_t<std::remove_pointer_t<decltype(FrameParams::spriteQuads)>>;
      using Page = std::remove_const_t<decltype(FrameParams::spriteAtlasPages)::element_type>;
      IRenderer* r = nullptr;
      std::function<void()> post;
      std::function<bool()> pending;  // B61-1
      bool shotArmed = false;          // B61-1: a capture was armed for the frame submitted last
      std::thread th;
      std::mutex m;
      std::condition_variable cv;
      bool has = false, busy = false, quit = false;
      FrameParams job{};
      std::vector<Quad> quads;
      std::vector<Page> pages;
      uint64_t pagesVer = ~0ull;
      std::atomic<bool> lost{false};
      void loop() {
        std::unique_lock<std::mutex> l(m);
        for (;;) {
          cv.wait(l, [&] { return has || quit; });
          if (!has) return;
          has = false; busy = true;
          l.unlock();
          {
            const int64_t pw0 = ::litholattice::tickprof::enabled() ? ::litholattice::tickprof::nowNs() : 0;  // LL_TICK_PROFILE (measure only; Proposed by JD; co-implemented by JD and Cortana)
            litholattice::threeway::RendererGuard rg;
            const int64_t pw1 = pw0 ? ::litholattice::tickprof::nowNs() : 0;
            r->renderFrame(job);
            const int64_t pw2 = pw0 ? ::litholattice::tickprof::nowNs() : 0;
            if (r->deviceLost()) lost.store(true);
            if (pw0) ::litholattice::tickprof::frames().add(pw1 - pw0, pw2 - pw1, ::litholattice::tickprof::nowNs() - pw2);
          }
          l.lock();
          busy = false;
          cv.notify_all();
        }
      }
      void submit(const FrameParams& fp) {  // main thread: the render handshake
        std::unique_lock<std::mutex> l(m);
        const int64_t ph0 = ::litholattice::tickprof::enabled() ? ::litholattice::tickprof::nowNs() : 0;  // LL_TICK_PROFILE (measure only)
        cv.wait(l, [&] { return !busy && !has; });
        if (ph0) { ::litholattice::tickprof::frames().handoffNs.fetch_add(::litholattice::tickprof::nowNs() - ph0); ::litholattice::tickprof::frames().handoffs.fetch_add(1); }
        // B61-1 (Proposed by JD; co-implemented by JD and Cortana): the screenshot capture touches main-thread game state (console, Logbook gallery, player
        // pose), so it runs here on the main thread while the render thread is idle, right after the armed frame.
        // A capture armed during this frame's update rides on the job submitted now, so it is taken at the next submit.
        if (shotArmed) { litholattice::threeway::RendererGuard rg; post(); shotArmed = false; }
        else if (pending && pending()) shotArmed = true;
        job = fp;
        const int64_t pc0 = ph0 ? ::litholattice::tickprof::nowNs() : 0;
        quads.assign(fp.spriteQuads, fp.spriteQuads + (fp.spriteQuads ? fp.spriteQuadCount : 0));
        job.spriteQuads = quads.empty() ? nullptr : quads.data();
        job.spriteQuadCount = quads.size();
        if (fp.spriteAtlasVersion != pagesVer) { pages.assign(fp.spriteAtlasPages.begin(), fp.spriteAtlasPages.end()); pagesVer = fp.spriteAtlasVersion; }
        job.spriteAtlasPages = std::span<const Page>(pages.data(), pages.size());
        if (pc0) { ::litholattice::tickprof::frames().copyNs.fetch_add(::litholattice::tickprof::nowNs() - pc0); ::litholattice::tickprof::frames().copies.fetch_add(1); }
        has = true;
        cv.notify_all();
      }
      void stop() {
        if (!th.joinable()) return;
        { std::lock_guard<std::mutex> l(m); quit = true; }
        cv.notify_all();
        th.join();
        if (shotArmed && post) post();  // B61-1: a capture armed for the last rendered frame still lands (render thread joined)
      }
      ~RenderWorker() { stop(); }
    };

struct SwRenderJob {
      using Quad = std::remove_const_t<std::remove_pointer_t<decltype(FrameParams::spriteQuads)>>;
      using Page = std::remove_const_t<decltype(FrameParams::spriteAtlasPages)::element_type>;
      FrameParams fp{};
      std::vector<Quad> quads;  // the job's own sprite quads (fp.spriteQuads repointed here)
      std::vector<Page> pages;  // the job's own atlas pages (fp.spriteAtlasPages repointed here)
      uint64_t drainTarget = 0;  // Stage 2 seal: ring entries published at submit; the executor drains up to it, so the frame renders the store state after exactly those uploads
    };
struct SwFrameSlot {
      SoftwareFramebuffer fb;  // pixels only (Argon amendment): the presentable image
      uint64_t seq = 0;        // job sequence that produced these pixels
      uint64_t storeGen = 0;   // that job's mesh-store seal
      enum State : uint8_t { Free, Ready, Presenting, Shown } state = Free;
    };
struct SwRenderWorker {
      cSoftwareRenderer* r = nullptr;
      std::function<void()> post;     // B61-1 mirror: capture of a finished frame (game state lives on main)
      std::function<bool()> pending;  // B61-1 mirror: a capture is armed (game.screenshotPending())
      bool shotArmed = false;         // B61-1: a capture was armed for the job submitted last
      std::mutex m;
      std::condition_variable cv;
      SwRenderJob job;                // the one mailbox slot (producer side)
      uint64_t pagesVer = ~0ull;      // atlas version the job's page copy belongs to
      bool jobActive = false;         // a job is submitted and not yet completed (m)
      std::exception_ptr jobErr;      // first failure of a completed job, surfaced on main (m)
      uint64_t submitted = 0, completed = 0;  // (m)
      SwFrameSlot slots[2];           // pixels-only handoff slots (m)
      int presentIdx = -1;            // slot main is presenting from, else -1 (main thread only)
      std::atomic<bool> lost{false};
      std::atomic<size_t> lastDrawn{0};  // meshes drawn by the last completed frame (overlay readout)
      void submit(const FrameParams& fp) {  // main thread: copy the job in, hand it to the executor, return
        std::exception_ptr err;
        { std::unique_lock<std::mutex> l(m); cv.wait(l, [&] { return !jobActive; }); err = jobErr; jobErr = nullptr; }
        if (err) std::rethrow_exception(err);  // a failed render surfaces on main one frame late (the sync path throws out of renderFrame itself)
        // B61-1 (mirrors RenderWorker::submit): the screenshot capture touches main-thread
        // game state, so it runs here on main while no job is in flight — the armed job has
        // completed, so its grab snapshot is final. A capture armed during this frame's
        // update rides on the job submitted now and is taken at the next submit (or stop()).
        if (shotArmed) { post(); shotArmed = false; }
        else if (pending && pending()) shotArmed = true;
        job.fp = fp;
        job.quads.assign(fp.spriteQuads, fp.spriteQuads + (fp.spriteQuads ? fp.spriteQuadCount : 0));
        job.fp.spriteQuads = job.quads.empty() ? nullptr : job.quads.data();
        job.fp.spriteQuadCount = job.quads.size();
        if (fp.spriteAtlasVersion != pagesVer) { job.pages.assign(fp.spriteAtlasPages.begin(), fp.spriteAtlasPages.end()); pagesVer = fp.spriteAtlasVersion; }
        job.fp.spriteAtlasPages = std::span<const SwRenderJob::Page>(job.pages.data(), job.pages.size());
        job.drainTarget = r->uploadRingPublished();  // the seal: every upload staged by now belongs to this frame
        uint64_t seq;
        { std::lock_guard<std::mutex> l(m); jobActive = true; seq = ++submitted; }
        // The job's copies stay untouched until completeJob(seq) has run: the next submit()
        // waits for exactly that, so the one-slot discipline holds.
        r->renderFrameDetached(job.fp, job.drainTarget, [this, seq](std::exception_ptr e) { completeJob(seq, e); });
      }
      // Runs on the pool executor the moment the frame is finished (or on the caller when
      // the pool's detached mode is off). Swaps the finished pixels into the freest slot:
      // a Free slot, else the oldest Shown (already presented), else the older Ready
      // (superseded unpresented). A Presenting slot is never a target — main presents one
      // slot at a time, so a target always exists.
      void completeJob(uint64_t seq, std::exception_ptr err) {
        std::lock_guard<std::mutex> l(m);
        if (!err) {
          SoftwareFramebuffer& fb = r->framebuffer();
          SwFrameSlot* tgt = nullptr;
          for (auto& s : slots) if (s.state == SwFrameSlot::Free) { tgt = &s; break; }
          if (!tgt) for (auto& s : slots) if (s.state == SwFrameSlot::Shown && (!tgt || s.seq < tgt->seq)) tgt = &s;
          if (!tgt) for (auto& s : slots) if (s.state == SwFrameSlot::Ready && (!tgt || s.seq < tgt->seq)) tgt = &s;
          if (tgt) {
            // The swap must preserve the renderer's invariant pixels.size() == width*height:
            // size the slot's outgoing buffer first (first use / resolution change only).
            if (tgt->fb.pixels.size() != fb.pixels.size()) tgt->fb.pixels.assign(fb.pixels.size(), 0u);
            std::swap(tgt->fb.pixels, fb.pixels);
            tgt->fb.width = fb.width;
            tgt->fb.height = fb.height;
            tgt->seq = seq;
            tgt->storeGen = r->meshStoreGeneration();  // Stage 2: the post-drain seal, read here on the executor — the thread that applied the drain, after it
            tgt->state = SwFrameSlot::Ready;
          }
          if (r->deviceLost()) lost.store(true);
          lastDrawn.store(r->meshesDrawnLastFrame());
        } else if (!jobErr) {
          jobErr = err;  // surfaced by the next submit() / stop()
        }
        jobActive = false;
        ++completed;
        cv.notify_all();
      }
      // Main thread, present path: the newest FINISHED frame (F-B4). The presenter never
      // waits and never sees an in-flight buffer: with no new frame finished it re-presents
      // the last shown slot, and before the first completion it returns an empty
      // framebuffer — the caller's zero-size check skips the blit, as it does today.
      const SoftwareFramebuffer& acquirePresent() {
        std::lock_guard<std::mutex> l(m);
        presentIdx = -1;
        SwFrameSlot* best = nullptr;
        for (int i = 0; i < 2; ++i)
          if (slots[i].state == SwFrameSlot::Ready && (!best || slots[i].seq > best->seq)) { best = &slots[i]; presentIdx = i; }
        if (!best)
          for (int i = 0; i < 2; ++i)
            if (slots[i].state == SwFrameSlot::Shown && (!best || slots[i].seq > best->seq)) { best = &slots[i]; presentIdx = i; }
        if (!best) { static const SoftwareFramebuffer kEmpty; return kEmpty; }
        best->state = SwFrameSlot::Presenting;
        return best->fb;
      }
      void releasePresent() {
        std::lock_guard<std::mutex> l(m);
        if (presentIdx >= 0 && slots[presentIdx].state == SwFrameSlot::Presenting) slots[presentIdx].state = SwFrameSlot::Shown;
        presentIdx = -1;
      }
      void stop() {  // main thread, shutdown: drain the in-flight job, land a trailing capture (B61-1)
        { std::unique_lock<std::mutex> l(m); cv.wait(l, [&] { return !jobActive; }); }
        if (jobErr) {  // nowhere left to surface it: log and drop
          try { std::rethrow_exception(jobErr); }
          catch (const std::exception& e) { logError("threeway software: render job failed at shutdown: %s", e.what()); }
          catch (...) { logError("threeway software: render job failed at shutdown (unknown exception)"); }
          jobErr = nullptr;
        }
        if (shotArmed && post) { post(); shotArmed = false; }  // a capture armed for the last rendered frame still lands
      }
      ~SwRenderWorker() { stop(); }
    };

// ---------------------------------------------------------------------------
// Build 50 (2026-10-19): frame-loop state, heap-held.
//
// The frame loop and its shutdown tail used to live entirely inside main().
// On the web build the game runs on a proxied pthread whose main() is one
// never-ending task: the browser presents an OffscreenCanvas only at task
// boundaries, so a hand-rolled while loop rendered forever into a buffer
// nobody ever saw (interactive boot = black canvas; bench mode, which
// exits, presented its final frame — the tell). The fix drives the loop
// through emscripten_set_main_loop_arg, whose registration unwinds main();
// everything the loop touches therefore lives in this heap struct, never
// in main()'s abandoned stack frame. Native and bench runs iterate the
// same step function in a plain while loop — behaviour unchanged.
// ---------------------------------------------------------------------------
struct FrameCtx {
  // Borrowed: owned by main()'s frame, assigned just before the loop runs.
  Window* window = nullptr;
  IRenderer* renderer = nullptr;
  Display* display = nullptr;
  double benchSeconds = 0, runSeconds = 0;
  std::string benchPath, benchJson, uiShot;
  bool noOverlay = false;
  std::string rname, present;
  // Owned by the context: constructed during main()'s setup (through the
  // aliases there), destroyed after the shutdown tail runs.
  std::unique_ptr<Game> game;
  std::unique_ptr<cInputSystem> input;
  std::unique_ptr<litholattice::mcp::cMcpServer> mcpServer;
  std::thread mcpThread;
  std::shared_ptr<std::atomic<bool>> mcpDone;  // set by the MCP thread as it exits (R-RACE-1)
#if !defined(_WIN32)
  std::unique_ptr<cMcpStdioTransport> mcpTransport;
  int mcpSavedStdoutFd = -1;
#endif
  RenderWorker rw;
  SwRenderWorker sw;
  bool threewayOn = false, swThreewayOn = false;
  // Cross-frame loop state (was: main() locals beside the loop).
  std::chrono::steady_clock::time_point tStart{}, tLast{}, tTitle{}, tPerf{};
  bool uiShotDone = false;
  BenchRun br;
  double msInput = 0, msUpdate = 0, msRender = 0, msPresent = 0;
  std::vector<double> perfMs, benchMs;
  int frames = 0, perfFrames = 0, hitches = 0;
  double worstMs = 0, perfSumMs = 0, perfMaxMs = 0, avgFrameMs = 0;
  bool wasFocused = false;
  litholattice::cFramePacer pacer;
  int appliedFpsCap = 0;
  int autoFrames = 0; double autoRenderSum = 0;
  std::vector<double> ovMs;
  size_t ovHead = 0;
  std::chrono::steady_clock::time_point ovPrev{}, ovT{};
  double ovFps = 0, ovFrameMs = 0; int ovFrames = 0;
  std::vector<uint32_t> ovSaved;
  double ovUpdSum = 0; int ovUpdN = 0;
};

#if !defined(__EMSCRIPTEN__)
// JD 2026-10-24: Settings > Graphics > Renderer. After the normal exit (world saved), start the
// same exe again with the same arguments and --renderer <new>, then this process returns.
static std::string g_relaunchBackend;
static void relaunchWithRenderer(int argc, char** argv, const std::string& backend) {
  std::vector<std::string> args{argv[0]};
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--renderer") && i + 1 < argc) { ++i; continue; }
    args.emplace_back(argv[i]);
  }
  args.emplace_back("--renderer"); args.push_back(backend);
#if defined(_WIN32)
  std::vector<std::string> quoted;
  for (const auto& a : args) {
    std::string q = "\"";
    for (char c : a) { if (c == '"') q += '\\'; q += c; }
    if (!q.empty() && q.back() == '\\') q += '\\';
    q += '"'; quoted.push_back(std::move(q));
  }
  std::vector<const char*> av;
  for (const auto& q : quoted) av.push_back(q.c_str());
  av.push_back(nullptr);
  if (_spawnv(_P_NOWAIT, argv[0], av.data()) == -1) std::fprintf(stderr, "renderer switch: relaunch failed (errno %d)\n", errno);
#else
  std::vector<char*> av;
  for (auto& a : args) av.push_back(a.data());
  av.push_back(nullptr);
  execv(argv[0], av.data());
  std::fprintf(stderr, "renderer switch: relaunch failed (errno %d)\n", errno);
#endif
}
#endif
static bool lkStepFrame(FrameCtx& F);
static void lkShutdownTail(FrameCtx& F);
#if defined(__EMSCRIPTEN__)
static void lkFrameThunk(void* p);
#endif

int main(int argc, char** argv) {
  GameConfig cfg;
  std::string uiShot, uiView = "hud";  // Cortana 2026-10-16: bench UI screenshot (--ui-shot FILE --ui-view hud|menu|settings|pause)
  double runSeconds = 0, benchSeconds = 0; bool vsync = false, noSave = false; int romScreen = 32;
  // Cortana 2026-10-15 1345: verify-mode summary at exit (when on).
  std::atexit([] { if (litholattice::meshVerifyEnabled()) { const auto s = litholattice::meshVerifyStats(); std::fprintf(stderr, "mesh-verify: checked %llu mismatches %llu skipped %llu\n", (unsigned long long)s.checked, (unsigned long long)s.mismatches, (unsigned long long)s.skipped); } });
  int renderThreadsArg = 0;  // Cortana 2026-10-15 1156: --render-threads N
  int fpsCapArg = -2; bool noVsync = false; bool noOverlay = false;  // Cortana 2026-10-15 1440: --no-overlay (A/B)  // Cortana 2026-10-15 1610: --fps-cap 30|60|off|auto (-2 = not given), --no-vsync
  bool tpsGiven = false, radiusGiven = false, displayGiven = false, streamGiven = false; int streamingArg = -1;  // CLI wins over settings.json
  bool simAreaGiven = false, streamAreaGiven = false;  // diamond (JD 2026-10-23)
  int vkFxArg = -1;  // JD Vulkan push
  DisplayDesc display0;
  std::string vkArena = "device";
  bool glArena = true;
  std::string saveDir, presentMode, benchPath = "orbit", benchJson;
  std::string pack = "pack_64";
  std::vector<std::string> extraPacks;
  bool mute = false;
  std::string backend = "software";  // JD's standard: the CPU path is the baseline default;
                                     // gl33/vulkan are the GPU offload, selected explicitly
  bool rendererGiven = false;  // --renderer wins over settings.json "renderer" (JD 2026-10-24)
#ifdef NDEBUG
  bool vkValidation = false;
#else
  bool vkValidation = true;  // debug builds: Khronos validation layer when installed
#endif
  std::string logPath = defaultLogPath(argv[0]);
  std::string scriptsMode;  // --scripts source|exe|<dir>; empty = this build's default
  NoisePreviewOptions preview;
  bool runPreview = false, runSelftest = false, runPerf = false;
  bool runHandshakeTest = false, hsGiven = false, seedGiven = false;
  bool mcpStdio = false;  // --mcp-stdio: engine MCP server on stdin/stdout (P2c, F-M9)
  bool runScenario = false; int scenarioTicks = 0, scenarioWorkers = 4, scenarioPhases = 4;
  bool wantControlLatency = false, wantControlTest = false, wantControlScenario = false;
  bool wantDemoTest = false, wantDemoDump = false, wantDemoVerify = false;
  std::string demoFile; bool demoTicksGiven = false; uint64_t demoTicksLo = 0, demoTicksHi = 0;
  int controlScenarioTicks = 0; std::string controlScenarioMode = "legacy";
  std::string scenarioMode = "legacy";
  std::string perfJson;
  bool badArg = false;
  bool tpsUnlock = false;  // debug: lifts the 512 TPS cap for benches
  for (int i = 1; i < argc; ++i) {
    // Numeric option value: the whole next argument must parse and lie in [lo, hi], else the
    // run stops with a message (a typo used to become 0 silently, and sizes were unbounded).
    auto num = [&](double lo, double hi) -> double {
      const char* flag = argv[i];
      if (i + 1 >= argc) { std::fprintf(stderr, "%s: missing value\n", flag); badArg = true; return lo; }
      const char* s = argv[++i];
      double v = 0;
      const auto [end, ec] = std::from_chars(s, s + std::strlen(s), v);
      if (ec != std::errc{} || *end != '\0' || !(v >= lo && v <= hi)) {
        std::fprintf(stderr, "%s: '%s' is not a number in [%g, %g]\n", flag, s, lo, hi);
        badArg = true;
        return lo;
      }
      return v;
    };
    if (!std::strcmp(argv[i], "--size")) cfg.worldChunks = std::max(4, int(num(0, 1024)));
    else if (!std::strcmp(argv[i], "--tps")) { cfg.tps = int(num(1, 8192)); tpsGiven = true; }
    else if (!std::strcmp(argv[i], "--tps-unlock")) tpsUnlock = true;
    else if (!std::strcmp(argv[i], "--scene-size")) cfg.sceneSize = int(num(16, 1024));
    else if (!std::strcmp(argv[i], "--scene-rows")) cfg.sceneRows = int(num(1, 64));
    else if (!std::strcmp(argv[i], "--pattern") && i + 1 < argc) { cfg.pattern = argv[++i]; if (cfg.pattern != "checkers" && cfg.pattern != "circles") { std::fprintf(stderr, "--pattern: checkers|circles\n"); badArg = true; } }
    else if (!std::strcmp(argv[i], "--screen-orient") && i + 1 < argc) {
      const std::string o = argv[++i];
      if (o != "flat" && o != "vertical") { std::fprintf(stderr, "--screen-orient: flat|vertical\n"); badArg = true; }
      cfg.screenVertical = o == "vertical";
    }
    else if (!std::strcmp(argv[i], "--render-threads")) { renderThreadsArg = int(num(1, 64)); }  // Cortana 2026-10-15 1156
    else if (!std::strcmp(argv[i], "--radius")) { cfg.tickRadius = int(num(0, 512)); radiusGiven = true; }
    else if (!std::strcmp(argv[i], "--vkfx")) { vkFxArg = int(num(0, 127)); }  // JD Vulkan push: effect bits for this run
    else if (!std::strcmp(argv[i], "--stream-radius")) { cfg.streamRadius = int(num(1, 64)); streamGiven = true; }  // JD 2026-10-16
    else if (!std::strcmp(argv[i], "--sim-area") && i + 1 < argc) {
      const std::string a = argv[++i];
      bool ok = true;  // Fix run 3: case-insensitive; unknown -> WARN + diamond (never square)
      const litholattice::AreaShape sh = litholattice::parseAreaShape(a, &ok);
      if (!ok) std::fprintf(stderr, "WARN --sim-area '%s' unknown (square|round|diamond); using diamond\n", a.c_str());
      cfg.diamondSimArea = sh == litholattice::AreaShape::Diamond; cfg.roundSimArea = sh == litholattice::AreaShape::Round; simAreaGiven = true;
    }  // diamond (JD 2026-10-23)
    else if (!std::strcmp(argv[i], "--stream-area") && i + 1 < argc) {
      const std::string a = argv[++i];
      bool ok = true;  // Fix run 3: case-insensitive; unknown -> WARN + diamond
      const litholattice::AreaShape sh = litholattice::parseAreaShape(a, &ok, /*allowRound=*/false);
      if (!ok) std::fprintf(stderr, "WARN --stream-area '%s' unknown (square|diamond); using diamond\n", a.c_str());
      cfg.diamondStreamArea = sh == litholattice::AreaShape::Diamond; streamAreaGiven = true;
    }  // diamond (JD 2026-10-23)
    else if (!std::strcmp(argv[i], "--streaming") && i + 1 < argc) {
      const char* v = argv[++i];
      if (!std::strcmp(v, "on")) streamingArg = 1;
      else if (!std::strcmp(v, "off")) streamingArg = 0;
      else { std::fprintf(stderr, "--streaming: must be on|off\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--seconds")) runSeconds = num(0, 1e9);
    else if (!std::strcmp(argv[i], "--bench")) benchSeconds = num(1, 3600);
    else if (!std::strcmp(argv[i], "--vsync")) vsync = true;
    else if (!std::strcmp(argv[i], "--no-vsync")) noVsync = true;  // Cortana 2026-10-15 1610
    else if (!std::strcmp(argv[i], "--no-overlay")) noOverlay = true;  // Cortana 2026-10-15 1440: Debug Overlay off (bench A/B)
    else if (!std::strcmp(argv[i], "--fps-cap") && i + 1 < argc) {  // Cortana 2026-10-15 1610 (ENGINE)
      const std::string v = argv[++i];
      if (v == "off" || v == "0") fpsCapArg = 0; else if (v == "auto") fpsCapArg = -1;
      else if (v == "30") fpsCapArg = 30; else if (v == "60") fpsCapArg = 60;
      else { std::fprintf(stderr, "--fps-cap: 30|60|off|auto\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--vk-arena") && i + 1 < argc) {
      vkArena = argv[++i];
      if (vkArena != "host" && vkArena != "device" && vkArena != "rebar") { std::fprintf(stderr, "--vk-arena: host|device|rebar\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--gl-arena") && i + 1 < argc) {
      const std::string v = argv[++i];
      if (v != "on" && v != "off") { std::fprintf(stderr, "--gl-arena: on|off\n"); badArg = true; }
      glArena = v == "on";
    }
    else if (!std::strcmp(argv[i], "--display") && i + 1 < argc) {
      displayGiven = true;
      if (!parseDisplayMode(argv[++i], display0.mode)) { std::fprintf(stderr, "--display: windowed|maximized|borderless|fullscreen\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--monitor")) display0.monitor = int(num(0, 15));
    else if (!std::strcmp(argv[i], "--refresh")) display0.refreshHz = int(num(1, 1000));
    else if (!std::strcmp(argv[i], "--resolution") && i + 1 < argc) {
      char* end = nullptr;
      const char* res = argv[++i];
      display0.width = int(std::strtol(res, &end, 10));
      display0.height = (end && (*end == 'x' || *end == 'X')) ? int(std::strtol(end + 1, &end, 10)) : 0;
      if ((end && *end) || display0.width < 320 || display0.height < 200 ||
          display0.width > 16384 || display0.height > 16384) {
        std::fprintf(stderr, "--resolution: WxH, e.g. 1920x1080\n"); badArg = true;
      }
    }
    else if (!std::strcmp(argv[i], "--present") && i + 1 < argc) {
      presentMode = argv[++i];
      if (presentMode != "fifo" && presentMode != "mailbox" && presentMode != "immediate") { std::fprintf(stderr, "--present: fifo|mailbox|immediate\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--bench-path") && i + 1 < argc) {
      benchPath = argv[++i];
      if (benchPath != "orbit" && benchPath != "stream" && benchPath != "flyover" && benchPath != "walk" && benchPath != "still") { std::fprintf(stderr, "--bench-path: orbit|stream|flyover|walk|still\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--soak")) {
      benchSeconds = 60.0 * num(1, 1440);
      benchPath = "soak";
    }
    else if (!std::strcmp(argv[i], "--bench-json") && i + 1 < argc) benchJson = argv[++i];
    else if (!std::strcmp(argv[i], "--no-save")) noSave = true;
    // Playtest 2026-10-16 (JD): --test-world = --bench-scene testworld (flat pad, every registry
    // block in labeled rows, 42 stocked carry slots, infinite placement, Pattern ROM screen);
    // --rom-screen 32|64 picks the demo screen size; --capture hides F3 / controls / hint text.
    else if (!std::strcmp(argv[i], "--test-world")) cfg.benchScene = "testworld";
    else if (!std::strcmp(argv[i], "--rom-screen")) romScreen = int(num(32, 64));
    else if (!std::strcmp(argv[i], "--capture")) cfg.capture = true;
    else if (!std::strcmp(argv[i], "--ui-shot") && i + 1 < argc) uiShot = argv[++i];  // Cortana 2026-10-16
    else if (!std::strcmp(argv[i], "--ui-view") && i + 1 < argc) uiView = argv[++i];
    else if (!std::strcmp(argv[i], "--verify-mesh")) litholattice::setMeshVerify(true);  // Cortana 2026-10-15 1345
    else if (!std::strcmp(argv[i], "--save-dir") && i + 1 < argc) saveDir = argv[++i];
    else if (!std::strcmp(argv[i], "--no-euv")) cfg.euv = false;
    else if (!std::strcmp(argv[i], "--timing-handshake")) { cfg.timingHandshake = true; hsGiven = true; }
    else if (!std::strcmp(argv[i], "--no-timing-handshake")) { cfg.timingHandshake = false; hsGiven = true; }
    else if (!std::strcmp(argv[i], "--handshake-phases")) cfg.handshakePhases = int(num(1, 16));
    else if (!std::strcmp(argv[i], "--handshake-test")) runHandshakeTest = true;
    else if (!std::strcmp(argv[i], "--control-latency")) wantControlLatency = true;
    else if (!std::strcmp(argv[i], "--control-test")) wantControlTest = true;
    else if (!std::strcmp(argv[i], "--demo-test")) wantDemoTest = true;
    else if (!std::strcmp(argv[i], "--demo-dump")) {
      if (i + 1 >= argc) { std::fprintf(stderr, "--demo-dump: needs FILE\n"); badArg = true; }
      else { wantDemoDump = true; demoFile = argv[++i]; }
    }
    else if (!std::strcmp(argv[i], "--demo-verify")) {
      if (i + 1 >= argc) { std::fprintf(stderr, "--demo-verify: needs FILE\n"); badArg = true; }
      else { wantDemoVerify = true; demoFile = argv[++i]; }
    }
    else if (!std::strcmp(argv[i], "--ticks")) {
      if (i + 2 >= argc) { std::fprintf(stderr, "--ticks: needs LO HI\n"); badArg = true; }
      else {
        demoTicksGiven = true;
        const char* loS = argv[++i];
        const char* hiS = argv[++i];
        const auto [loEnd, loEc] = std::from_chars(loS, loS + std::strlen(loS), demoTicksLo);
        const auto [hiEnd, hiEc] = std::from_chars(hiS, hiS + std::strlen(hiS), demoTicksHi);
        if (loEc != std::errc{} || *loEnd != '\0' || hiEc != std::errc{} || *hiEnd != '\0') {
          std::fprintf(stderr, "--ticks: LO and HI must be unsigned 64-bit numbers\n"); badArg = true;
        }
      }
    }
    else if (!std::strcmp(argv[i], "--control-scenario")) {
      wantControlScenario = true;
      if (i + 2 >= argc) { std::fprintf(stderr, "--control-scenario: needs TICKS legacy|handshake\n"); badArg = true; }
      else {
        controlScenarioTicks = int(num(1, 1000000));
        controlScenarioMode = argv[++i];
        if (controlScenarioMode != "legacy" && controlScenarioMode != "handshake" && controlScenarioMode != "baseline") { std::fprintf(stderr, "--control-scenario: mode must be legacy|handshake|baseline\n"); badArg = true; }
      }
    }
    else if (!std::strcmp(argv[i], "--mcp-stdio")) mcpStdio = true;
    else if (!std::strcmp(argv[i], "--handshake-scenario")) {
      runScenario = true;
      if (i + 4 >= argc) { std::fprintf(stderr, "--handshake-scenario: needs TICKS legacy|handshake WORKERS PHASES\n"); badArg = true; }
      else {
        scenarioTicks = int(num(1, 1000000));
        scenarioMode = argv[++i];
        scenarioWorkers = int(num(1, 64));
        scenarioPhases = int(num(1, 16));
        if (scenarioMode != "legacy" && scenarioMode != "handshake") { std::fprintf(stderr, "--handshake-scenario: mode must be legacy|handshake\n"); badArg = true; }
      }
    }
    else if (!std::strcmp(argv[i], "--dim") && i + 1 < argc) cfg.dimension = argv[++i];
    else if (!std::strcmp(argv[i], "--log") && i + 1 < argc) logPath = argv[++i];
    else if (!std::strcmp(argv[i], "--scripts") && i + 1 < argc) scriptsMode = argv[++i];
    else if (!std::strcmp(argv[i], "--pack") && i + 1 < argc) pack = argv[++i];
    else if (!std::strcmp(argv[i], "--resource-pack") && i + 1 < argc) extraPacks.emplace_back(argv[++i]);
    else if (!std::strcmp(argv[i], "--mute")) mute = true;
    else if (!std::strcmp(argv[i], "--renderer") && i + 1 < argc) { backend = argv[++i]; rendererGiven = true; }
    else if (!std::strcmp(argv[i], "--vk-validation")) vkValidation = true;
    // Silicon Cities (Cortana 2026-10-16): --cities on|off overrides wafer.json's "enabled".
    else if (!std::strcmp(argv[i], "--cities") && i + 1 < argc) {
      const char* v = argv[++i];
      if (!std::strcmp(v, "on")) litholattice::procgen::setSiliconCitiesOverride(1);
      else if (!std::strcmp(v, "off")) litholattice::procgen::setSiliconCitiesOverride(0);
      else { std::fprintf(stderr, "--cities: must be on|off\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) {
      seedGiven = true;
      const char* s = argv[++i];
      const auto [end, ec] = std::from_chars(s, s + std::strlen(s), cfg.seed);
      if (ec != std::errc{} || *end != '\0') { std::fprintf(stderr, "--seed: '%s' is not an unsigned 64-bit number\n", s); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--noise-preview") && i + 1 < argc) { preview.graphFile = argv[++i]; runPreview = true; }
    else if (!std::strcmp(argv[i], "--preview-out") && i + 1 < argc) preview.outFile = argv[++i];
    else if (!std::strcmp(argv[i], "--preview-output") && i + 1 < argc) preview.output = argv[++i];
    else if (!std::strcmp(argv[i], "--preview-size")) preview.size = int(num(1, 16384));
    else if (!std::strcmp(argv[i], "--preview-scale")) preview.scale = float(num(1e-6, 1e6));
    else if (!std::strcmp(argv[i], "--preview-y")) preview.y = float(num(-1e6, 1e6));
    else if (!std::strcmp(argv[i], "--selftest")) runSelftest = true;
    else if (!std::strcmp(argv[i], "--perf-headless")) { runPerf = true; if (i + 1 < argc && argv[i + 1][0] != '-') perfJson = argv[++i]; }
    else if (!std::strcmp(argv[i], "--bench-scene") && i + 1 < argc) {
      cfg.benchScene = argv[++i];
      if (cfg.benchScene != "default" && cfg.benchScene != "circuits" && cfg.benchScene != "display" && cfg.benchScene != "framebuffer" && cfg.benchScene != "redirector" && cfg.benchScene != "wall-pixel" && cfg.benchScene != "pattern-wall" && cfg.benchScene != "sawtooth" && cfg.benchScene != "wire-torture" && cfg.benchScene != "litho" && cfg.benchScene != "litho-wired" && cfg.benchScene != "testworld") { std::fprintf(stderr, "--bench-scene: default|circuits|display|framebuffer|redirector|wall-pixel|pattern-wall|sawtooth|wire-torture|litho|litho-wired|testworld\n"); badArg = true; }
    }
    else if (!std::strcmp(argv[i], "--version")) { std::printf("LithoKraft %s (LithoLattice engine)\n", LITHOKRAFT_VERSION); return 0; }
    else {
      std::printf("usage: LithoKraft [--version] [--selftest] [--dim wafer|organic] [--size chunks] "
                  "[--tps 32|64|128|256|512] [--tps-unlock (debug: --tps up to 8192, powers of 2)] [--no-euv] [--radius chunks] [--render-threads 1|2|4|8|16|32|64] [--seconds s] [--vsync] [--display windowed|maximized|borderless|fullscreen] [--monitor N] [--resolution WxH] [--refresh Hz] [--log file] "
                  "[--pack pack_64|pack_32] [--resource-pack dir]... [--scripts source|exe|dir] [--mute] [--renderer software|gl33|vulkan] [--bench seconds] [--bench-path orbit|stream|flyover|walk|still] [--bench-scene default|circuits|display|framebuffer|redirector|wall-pixel|pattern-wall|sawtooth|wire-torture|litho|litho-wired|testworld] [--test-world [--rom-screen 32|64]] [--capture] [--scene-size n] [--scene-rows 1-64 (wall-pixel <= 16, sawtooth)] [--pattern checkers|circles (pattern-wall)] [--screen-orient flat|vertical] [--perf-headless [json]] [--soak minutes] [--bench-json file] [--ui-shot file.png [--ui-view hud|menu|settings|pause|inventory]] [--present fifo|mailbox|immediate] [--save-dir dir] [--no-save] [--verify-mesh] [--mcp-stdio] "
                  "[--vk-validation] [--vk-arena host|device|rebar] [--gl-arena on|off] [--seed n] [--cities on|off] [--noise-preview graph.json [--preview-out f.png] "
                  "[--preview-output name] [--preview-size px] [--preview-scale blocks/px] [--preview-y y]]\n");
      return 0;
    }
  }
  if (badArg) return 2;
  // cPlayer settings (config/settings.json next to config/input.json): CLI flags win.
  const std::string settingsPath = (dataRoot(argv[0]) / "config" / "settings.json").lexically_normal().string();
  GameSettings settings = GameSettings::load(settingsPath);
  if (!rendererGiven && benchSeconds <= 0) backend = settings.renderer;  // JD 2026-10-24: saved renderer is the startup default
  if (!hsGiven) { cfg.timingHandshake = settings.timingHandshake; cfg.handshakePhases = settings.handshakePhases; }
  if (!tpsGiven) cfg.tps = settings.tps;  // load() only yields valid modes
  if (!radiusGiven) cfg.tickRadius = settings.tickRadius;
  const char* radiusSource = radiusGiven ? "--radius" : "settings.json";  // F56-6 (logged once the log file is open)
  if (!simAreaGiven) cfg.roundSimArea = settings.roundSimArea;  // F56-7 (Proposed by JD; co-implemented by JD and Cortana): round sim area (CLI --sim-area wins)
  if (!simAreaGiven) cfg.diamondSimArea = settings.diamondSimArea;  // diamond (JD 2026-10-23)
  if (!streamAreaGiven) cfg.diamondStreamArea = settings.diamondStreamArea;  // diamond (JD 2026-10-23)
  // fix 3b: report where each area value came from; the CLI wins over the env vars (like --radius).
  const char* roundSource = simAreaGiven ? "--sim-area" : "settings.json";
  const char* streamSource = streamAreaGiven ? "--stream-area" : "settings.json";
  if (const char* e = std::getenv("LL_SIM_ROUND_AREA"); e && *e && !simAreaGiven) { cfg.roundSimArea = e[0] != '0'; roundSource = "LL_SIM_ROUND_AREA"; }
  if (const char* e = std::getenv("LL_SIM_AREA"); e && *e && !simAreaGiven) {  // diamond (JD 2026-10-23); fix run 3: shared parser
    bool ok = true;
    const litholattice::AreaShape sh = litholattice::parseAreaShape(e, &ok);
    if (!ok) std::fprintf(stderr, "WARN LL_SIM_AREA '%s' unknown (square|round|diamond); using diamond\n", e);
    cfg.diamondSimArea = sh == litholattice::AreaShape::Diamond; cfg.roundSimArea = sh == litholattice::AreaShape::Round; roundSource = "LL_SIM_AREA";
  }
  if (const char* e = std::getenv("LL_STREAM_AREA"); e && *e && !streamAreaGiven) {  // diamond (JD 2026-10-23); fix run 3: shared parser
    bool ok = true;
    const litholattice::AreaShape sh = litholattice::parseAreaShape(e, &ok, /*allowRound=*/false);
    if (!ok) std::fprintf(stderr, "WARN LL_STREAM_AREA '%s' unknown (square|diamond); using diamond\n", e);
    cfg.diamondStreamArea = sh == litholattice::AreaShape::Diamond; streamSource = "LL_STREAM_AREA";
  }
  // Cortana 2026-10-16 2335 F55-5 (JD): LL_SIM_TICK_RADIUS overrides settings.json (CLI --radius still wins).
  if (const char* e = std::getenv("LL_SIM_TICK_RADIUS"); e && *e && !radiusGiven) {
    cfg.tickRadius = std::clamp(std::atoi(e), 0, 512);
    radiusSource = "LL_SIM_TICK_RADIUS";
  }
  // JD 2026-10-16: render / stream distance (auto: Vulkan 64, software and GL 3.3 32; hard cap 64).
  if (!streamGiven) cfg.streamRadius = settings.streamRadius > 0 ? settings.streamRadius : (backend == "vulkan" ? 64 : 32);
  cfg.streamRadius = std::clamp(cfg.streamRadius, 1, 64);
  cfg.streaming = streamingArg >= 0 ? streamingArg == 1 : settings.streaming;
  vsync = vsync || settings.vsync;
  // Cortana 2026-10-15 1610 (ENGINE): tear-free software presentation. The
  // software frame is blitted and swapped through GL, so swap interval 1
  // presents on vblank. On by default for software play; off for --bench
  // (benches measure uncapped) and with --no-vsync.
  if (backend == "software" && benchSeconds <= 0) vsync = true;
  if (noVsync || benchSeconds > 0) vsync = false;
  if (fpsCapArg != -2) settings.fpsCap = fpsCapArg;  // CLI wins for this run
  if (!displayGiven) {
    DisplayMode mode = display0.mode;
    if (parseDisplayMode(kSettingsDisplayModes[settings.displayMode], mode)) display0.mode = mode;
  }
  // Cortana 2026-10-15 1156: software raster threads: CLI wins over the
  // settings key; 0 = engine default (4). Engine rounds to a power of two
  // and clamps to hardware threads.
  cSoftwareRenderer::setRenderThreads(unsigned(renderThreadsArg > 0 ? renderThreadsArg : settings.renderThreads));
  if (mute) settings.muted = true;
  cfg.vkFxOverride = vkFxArg;  // JD Vulkan push: CLI wins for this run only (0.0.1.51: no longer saved)
  cfg.version = LITHOKRAFT_VERSION;
  cfg.settings = settings;
  cfg.settingsPath = settingsPath;
  const ScriptsDirChoice scripts = findScriptsDir(argv[0], scriptsMode);
  cfg.scriptsDir = scripts.dir;
  // Saves: <data root>/saves/world by default; --bench never loads or saves (repeatable runs).
#ifdef LK_PLAYTEST_SPAWN
  // 2026-10-24 (JD): playtest builds (-DLK_PLAYTEST_SPAWN=ON) start in the test area when no
  // scene / bench / test mode was asked for, with their own save dir (saves/playtest) so the
  // normal world (saves/world) and the scenario hashes are never touched.
  if (cfg.benchScene == "default" && benchSeconds <= 0 && !runHandshakeTest) {
    cfg.benchScene = "testworld";
    if (saveDir.empty() && !noSave) saveDir = (dataRoot(argv[0]) / "saves" / "playtest").lexically_normal().string();
  }
#endif
  if (cfg.benchScene == "testworld") {
    cfg.sceneSize = romScreen >= 64 ? 64 : 32;
    // The test world is rebuilt every launch; a save would restore its columns over the fresh
    // build (and never touch JD's real world). Fresh every time unless --save-dir is given.
    if (saveDir.empty()) noSave = true;
  }
  if (!noSave && benchSeconds <= 0) cfg.saveDir = saveDir.empty() ? (dataRoot(argv[0]) / "saves" / "world").lexically_normal().string() : saveDir;
  if (runHandshakeTest) return runHandshakeTests();
  if (runScenario) return runHandshakeScenario(scenarioTicks, scenarioMode, scenarioWorkers, scenarioPhases);
  if (wantControlLatency) return runControlLatency();
  if (wantControlTest) return runControlTests();
  if (wantDemoTest) return runDemoTests();
  if (wantDemoDump) return runDemoDump(demoFile, demoTicksGiven, demoTicksLo, demoTicksHi);
  if (wantDemoVerify) return runDemoVerify(demoFile);
  if (wantControlScenario) return runControlScenario(controlScenarioTicks, controlScenarioMode);
  if (runPerf) {
    BenchSceneConfig bench;
    bench.scene = cfg.benchScene;
    bench.size = cfg.sceneSize;
    bench.rows = cfg.sceneRows;
    bench.pattern = cfg.pattern;
    bench.timingHandshake = cfg.timingHandshake;
    bench.handshakePhases = cfg.handshakePhases;
    return runPerfHeadless(perfJson, bench);
  }
  if (runSelftest) return runSelfTests(cfg.scriptsDir, packStack(findAssetDirs(argv[0]), "pack_64", {}), cfg.timingHandshake);
  if (runPreview) {  // headless tool: no window, no log file
    preview.seed = cfg.seed;
    return runNoisePreview(preview);
  }
  // F7 cycles kTickModes (32..256); the CLI also takes 512 (kMaxDomainHz) for stress benches.
  // --tps-unlock (debug / bench only): any power of two up to 8192 TPS, the enthusiast-tier probe.
  const bool tpsUnlocked = tpsUnlock && cfg.tps >= 32 && cfg.tps <= 8192 && (cfg.tps & (cfg.tps - 1)) == 0;
  if (!tpsUnlocked && cfg.tps != kMaxDomainHz && std::find(std::begin(kTickModes), std::end(kTickModes), cfg.tps) == std::end(kTickModes)) {
    std::fprintf(stderr, "--tps must be 32, 64, 128, 256 or 512\n");  // stderr: the log file is not open yet
    logError("--tps must be 32, 64, 128, 256 or 512"); return 1;
  }
  if (backend != "software" && backend != "gl33" && backend != "vulkan") { logError("--renderer must be software, gl33 or vulkan"); return 1; }

  if (logOpenFile(logPath)) logInfo("LithoKraft %s (LithoLattice engine), log file %s (previous run: .prev.log)", LITHOKRAFT_VERSION, logPath.c_str());
  else {
    // 0.0.1.56 F56-11 (Discovered by Cortana, Fixed by Cortana): the log is held deny-write by another running
    // LithoKraft (e.g. JD playing while a test runs). Log to a run-unique file next to it instead of console only.
    std::filesystem::path alt(logPath);
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    alt.replace_filename("lithokraft.second-" + std::to_string(ms) + ".log");
    if (logOpenFile(alt.string()))
      logWarn("log file %s is in use (another LithoKraft running?); this run logs to %s", logPath.c_str(), alt.string().c_str());
    else logError("could not open log file %s; console only", logPath.c_str());
  }
  optOutOfBackgroundThrottling();  // 0.0.1.57 F57-1 (R21; Discovered by Cortana, Fixed by Cortana)
  logInfo("%s", lithokraft::buildInfo().c_str());  // 2026-10-16 (JD): Alpha build counter + Release/Debug + date (renderer logged at init)
  // fix 3b (Elfie P3): the log line is always the plain counter (no raw ANSI escapes in log files /
  // web consoles); the wheel ink is printed to a real terminal only.
  logInfo("rolling build counter: %s", kJdBuildCounterDisplay);
  logInfo("game counter: LithoKraft %s (%s)", lithokraft::gameCounter(), lithokraft::buildVersion());  // 2026-10-24 (JD)
  if (jdConsoleColor()) std::printf("rolling build counter (ink): %s\n", jdCounterDisplay().c_str());  // 2026-10-18 (JD): 8 wheels, base 64, every build rolls it; wheel-inked on terminals (jdCounterDisplay)
  logInfo("sim tick radius %d chunks (from %s), %s area (from %s), %s stream area (from %s)", cfg.tickRadius, radiusSource,
          cfg.diamondSimArea ? "diamond" : cfg.roundSimArea ? "round" : "square", roundSource,
          cfg.diamondStreamArea ? "diamond" : "square", streamSource);  // F56-6 (Cortana 2026-10-17) + diamond
  // SV-H2 seed precedence: an existing world's persisted seed (level.json in
  // the save dir) WINS over --seed and over the default. This must run here —
  // before Game construction — because Game's constructor builds the
  // dimensions from cfg.seed in its initializer list, before any save is
  // read; a late override would regenerate untouched (dirty-only-saved)
  // columns from the wrong seed under the saved ones. The Wafer is pinned
  // to seed 0 inside worldgen and is unaffected either way.
  if (!cfg.saveDir.empty()) {
    std::string seedErr;
    if (const auto persisted = cWorldSave::readWorldSeed(cfg.saveDir, &seedErr)) {
      if (seedGiven && *persisted != cfg.seed)
        logError("save: --seed %llu DISAGREES with the saved world's seed %llu in '%s'; using the SAVED seed (an existing world must not regenerate under a new seed)",
                 (unsigned long long)cfg.seed, (unsigned long long)*persisted, cfg.saveDir.c_str());
      cfg.seed = *persisted;
    } else if (!seedErr.empty()) {
      // Present but unreadable after recovery: warn loudly and fall back to
      // the CLI/default seed. (level.json alone gates no region data — the
      // blocks.names fail-closed path in cWorldSave is the data gate.)
      logError("save: world metadata in '%s' is unreadable (%s); falling back to %s seed %llu", cfg.saveDir.c_str(), seedErr.c_str(),
               seedGiven ? "the --seed" : "the default", (unsigned long long)cfg.seed);
    }
  }
  // Fix run 3 (2026-10-23): a saved world's area (level.json v3) WINS, like
  // its seed: the world keeps the sim/stream footprint it was played with.
  // v1/v2 saves carry none and keep the configured area (old behaviour).
  if (!cfg.saveDir.empty()) {
    std::string areaErr;
    if (const auto wa = cWorldSave::readWorldArea(cfg.saveDir, &areaErr)) {
      const bool simD = wa->sim == litholattice::AreaShape::Diamond, simR = wa->sim == litholattice::AreaShape::Round;
      const bool strD = wa->stream == litholattice::AreaShape::Diamond;
      if (simD != cfg.diamondSimArea || (!simD && simR != cfg.roundSimArea) || strD != cfg.diamondStreamArea || wa->tickRadius != cfg.tickRadius)
        logWarn("save: configured area (sim %s r%d, stream %s) differs from the saved world's (sim %s r%d, stream %s) in '%s'; using the SAVED area",
                cfg.diamondSimArea ? "diamond" : cfg.roundSimArea ? "round" : "square", cfg.tickRadius, cfg.diamondStreamArea ? "diamond" : "square",
                litholattice::areaShapeName(wa->sim), wa->tickRadius, litholattice::areaShapeName(wa->stream), cfg.saveDir.c_str());
      cfg.diamondSimArea = simD; cfg.roundSimArea = simR; cfg.diamondStreamArea = strD; cfg.tickRadius = wa->tickRadius;
      logInfo("sim tick radius %d chunks, %s area, %s stream area (from the saved world's level.json)", cfg.tickRadius,
              litholattice::areaShapeName(wa->sim), litholattice::areaShapeName(wa->stream));  // fix 3b
    }
    if (!areaErr.empty()) logWarn("save: world area: %s", areaErr.c_str());
  }
  if (scripts.dir.empty()) logError("scripts: no scripts/ folder (%s)", scripts.reason.c_str());
  else logInfo("scripts: %s (%s; the F10 editor saves here)", scripts.dir.c_str(), scripts.reason.c_str());

  Display display = createDisplay(backend, vsync, vkValidation, presentMode, display0, vkArena, glArena);
  if (!display.window || !display.renderer) return 1;
  Window* const window = display.window.get();
  IRenderer* const renderer = display.renderer.get();
  logInfo("renderer: %s | %s", renderer->name(), renderer->deviceInfo().c_str());
  const AssetDirs assets = findAssetDirs(argv[0]);
  setWindowIcon(*window, assets);
  if (assets.engine.empty()) logError("engine assets/ not found; built-in shaders");
  if (assets.game.empty()) logError("LithoKraft/assets/ not found; every block uses the magenta missing texture, built-in theme and generators");
  // PBR material registry (phase 2): the game resolves the data-file path, the engine loads it
  // (initMaterialRegistry). A missing file is graceful: log + the built-in default, the same
  // degrade-loudly convention as audio's "no output device; continuing muted". The registry
  // outlives the game (chunk saves resolve string IDs through it on the I/O thread).
  litholattice::cMaterialReg materials;
  {
    std::string matErr;
    const std::string matPath = assets.engine.empty()
                                    ? ""
                                    : (std::filesystem::path(assets.engine) / "materials" / "lithokraft.materials").string();
    if (litholattice::initMaterialRegistry(materials, matPath, &matErr))
      logInfo("materials: %zu materials from %s", materials.size(), matPath.c_str());
    else
      logError("materials: %s; continuing with the built-in default", matErr.c_str());
  }
  cfg.materials = &materials;
  const std::vector<std::filesystem::path> modPacks = scanMods(argv[0]);
  cResourceMgr resources(packStack(assets, pack, extraPacks, modPacks));  // outlives the game (mesh jobs)
  resources.reload();
  resources.apply(*renderer);
  cAudioEngine audio;  // outlives the output device and the game
  std::unique_ptr<IAudioOutput> audioOut;
  if (mute) {
    audio.setEnabled(false);
    logInfo("audio: muted (--mute)");
  } else {
    std::string err;
    audioOut = openAudioOutput(audio, &err);
    if (audioOut) logInfo("audio: %s", audioOut->description().c_str());
    else { audio.setEnabled(false); logError("audio: no output device (%s); continuing muted", err.c_str()); }
  }
  {
    auto lkFOwner = std::make_unique<FrameCtx>(); FrameCtx& F = *lkFOwner;  // build 50: frame-loop state on the heap (survives the Emscripten main-loop unwind; see FrameCtx)
    cJobPool pool(std::max(1u, std::max(2u, std::thread::hardware_concurrency()) - 2));
    F.input = std::make_unique<cInputSystem>(loadBindings(argv[0]), inputContexts()); cInputSystem& input = *F.input;
    window->loadGamepadMappings((dataRoot(argv[0]) / "config" / "gamecontrollerdb.txt").string());  // optional
    cWindowClipboard clipboard(*window);  // UI text fields copy / paste through the OS; outlives the game
    F.game = std::make_unique<Game>(cfg, *renderer, pool, resources, audio, input); Game& game = *F.game;
    logInfo("%s | renderer: %s", lithokraft::buildInfo().c_str(), game.rendererInfo().c_str());  // 2026-10-16 (JD)
    if (!game.ok()) return 1;
    // --mcp-stdio (P2c, F-M9): the engine MCP server over stdin/stdout on
    // its own thread, for the process lifetime. Default OFF: no flag, no
    // server, no thread, no behaviour change. The backends are the game's
    // existing adapters (Game::mcpBackends); the `mcp grant|revoke|agent`
    // console commands are registered by setMcpServer. Shutdown is
    // sequenced after the frame loop below: stop + join BEFORE the Game
    // is destroyed, because the backends point into it.
    auto& mcpServer = F.mcpServer;
    auto& mcpThread = F.mcpThread;
#if !defined(_WIN32)
    auto& mcpTransport = F.mcpTransport;
    auto& mcpSavedStdoutFd = F.mcpSavedStdoutFd;
#endif
    if (mcpStdio) {
      mcpServer = std::make_unique<litholattice::mcp::cMcpServer>(game.mcpBackends());
      mcpServer->setLogSink([](std::string_view line) {
        std::fprintf(stderr, "[mcp] %.*s\n", int(line.size()), line.data());
      });
      game.setMcpServer(mcpServer.get());
#if !defined(_WIN32)
      // stdout belongs to JSON-RPC now: save the real stdout for the
      // transport, then point fd 1 at stderr so logInfo (which writes to
      // stdout) and any stray printf cannot corrupt the protocol stream.
      std::fflush(stdout);
      mcpSavedStdoutFd = ::dup(STDOUT_FILENO);
      if (mcpSavedStdoutFd >= 0) ::dup2(STDERR_FILENO, STDOUT_FILENO);
      const int outFd = mcpSavedStdoutFd >= 0 ? mcpSavedStdoutFd : STDOUT_FILENO;
      mcpTransport = std::make_unique<cMcpStdioTransport>(outFd);
      litholattice::mcp::cMcpServer* srv = mcpServer.get();
      cMcpStdioTransport* tr = mcpTransport.get();
      mcpThread = std::thread([srv, tr] { srv->runStdio(*tr); });
#else
      // Windows: the engine's own stdio transport (blocking std::cin);
      // the thread is detached at shutdown — process exit ends it. The
      // poll-based clean join above is the POSIX path this build targets.
      litholattice::mcp::cMcpServer* srv = mcpServer.get();
      // Fix run 2 (R-RACE-1): the thread flags its exit so shutdown can cancel
      // the blocking stdin read and JOIN it before the server is freed.
      F.mcpDone = std::make_shared<std::atomic<bool>>(false);
      auto done = F.mcpDone;
      mcpThread = std::thread([srv, done] { srv->runStdio(); done->store(true, std::memory_order_release); });
#endif
      logWarn("mcp: stdio server running (agent starts with NO permissions; approve with `mcp grant <perm...>` in the console)");
    }
    game.setClipboard(&clipboard);
    game.setDisplayModeApplier([window](DisplayMode mode) {
      DisplayDesc d = window->display();
      d.mode = mode;
      window->setDisplay(d);
    });
    // The launch menu opens first (world loaded, sim paused, cinematic camera); the bench
    // harness drives the camera itself and skips it. The loop syncs cursor capture from
    // game.wantsCursorCaptured() every frame.
    if (benchSeconds <= 0) game.enterMenu();

    F.tStart = Clock::now(); F.tLast = F.tStart; F.tTitle = F.tStart; F.tPerf = F.tStart;
    constexpr double kBenchWarmup = 3.0;  // world load + first meshes settle before measuring
    // Cortana 2026-10-16 (BUG-0004): --ui-shot only ran inside a bench, so a plain
    // --ui-shot launch never wrote its PNG and never exited. It now implies a short
    // self-closing bench (2 s after warm-up) when no --bench was given.
    if (benchSeconds <= 0 && !uiShot.empty()) {
      benchSeconds = 2;
      logInfo("ui-shot: no --bench given, running a 2 s self-closing bench for the capture");
    }
    if (benchSeconds > 0) runSeconds = kBenchWarmup + benchSeconds;
    if (benchSeconds > 0) game.setBenchPath(benchPath);
    if (benchSeconds > 0 && !uiShot.empty()) game.debugShowUi(uiView);
    F.uiShotDone = uiShot.empty() || benchSeconds <= 0;
    const std::string rname = renderer->name();
    const std::string present = rname == "Vulkan"    ? renderer->presentMode()
                                : rname == "software" ? renderer->presentMode() + (vsync ? " (swap-interval-1)" : " (swap-interval-0)")
                                                      : (vsync ? "swap-interval-1" : "swap-interval-0");
    logInfo("present: %s (%s)", present.c_str(), renderer->name());
    BenchRun& br = F.br;
    br.memStart = processWorkingSetBytes();
    // Cortana 2026-10-15 1925 (bug: test windows steal focus): benches are unattended -- never
  // force the foreground (raiseAndFocus toggles topmost to beat the Windows foreground lock).
  if (benchSeconds <= 0) window->raiseAndFocus();  // launch-focus fix: come to the front on first show
    F.wasFocused = window->focused();
    // Frame pacer (render.fps_cap, D-M3 game-side wiring 2026-10-13):
    // configured from Settings at init (0 = off, vsync-only — the default)
    // and reconfigured live whenever the settings value changes (the
    // cvar/UI flow lands in Game::settings() via onSettingsChanged).
    // beginFrame() at the loop top, endFrameAndWait() right after swap.
    // Presentation only: the pacer never feeds the sim.
    auto& pacer = F.pacer;
    // Cortana 2026-10-15 1610 (ENGINE): -1 = auto: measure render time over
    // frames 61..180 uncapped, then cap at 60 if avg render <= 14 ms, else 30.
    // --bench always runs uncapped.
    const int initialCap = benchSeconds > 0 ? 0 : settings.fpsCap;
    pacer.setCapFps(initialCap > 0 ? initialCap : 0);
    F.appliedFpsCap = initialCap; auto& appliedFpsCap = F.appliedFpsCap;
    // Debug Overlay (Cortana 2026-10-15 1610, ENGINE): the full strip only in
    // Debug builds, --bench runs, or creative mode. Survival may show only
    // the minimal FPS/TPS counter (settings fpsCounter, off by default).
    auto& ovMs = F.ovMs;  ovMs.reserve(240);  // ring for the 1% low
    F.ovPrev = Clock::now(); F.ovT = F.ovPrev;
    if (appliedFpsCap > 0) logInfo("fps cap: %d FPS (render.fps_cap)", appliedFpsCap);
    litholattice::locktiming::markMainThread();  // 0.0.1.60 diagnostic lock timing (LL_LOCK_TIMING=1; Proposed by JD; co-implemented by JD and Cortana)
    // ---- Three-way split timing handshake (LL_THREEWAY=1, default OFF; Vulkan only). Proposed by JD; co-implemented by JD and Cortana.
    // Main thread: window, OS events, input and game logic (the control & event handshake). Render thread: GPU and
    // memory work only (command recording, uploads, present) from its own copy of a finished FrameParams. Sim
    // thread: unchanged. No blending: the render thread always draws the newest finished frame. Handoff is a
    // one-slot swap; main waits only while the previous frame is still rendering. The game's renderer calls take
    // threeway::rendererMutex(), so they never overlap renderFrame. The render side never writes sim state.

    // ---- Three-way split, software path (Design B, Stage 1: asynchronous mailbox; same
    // LL_THREEWAY=1 toggle, default OFF). The software counterpart of RenderWorker: main submits
    // a COMPLETE frame job — FrameParams by value, sprite quads deep-copied, atlas pages
    // deep-copied on version change, borrowed pointers repointed at the job's own copies (the
    // exact copy set RenderWorker::submit uses) — plus the mesh-store seal, the renderer's
    // store generation at submit time. Stage 1 executes the job OFF the main thread, on the
    // band pool's detached executor worker (cSoftwareRenderer::renderFrameDetached; JD ruling 2:
    // the render side rides the pool threads, band count unchanged, no new always-on thread).
    // One job in flight, RenderWorker's own rule: submit() waits only while the previous job
    // is still rendering. The finished frame returns through two pixels-only handoff slots
    // (the Argon amendment: the renderer's fb_.pixels is SWAPPED with a slot buffer, never
    // copied; depth_ and hizMax_ stay private to the renderer — renderFrame's post stages
    // have already consumed them by completion). The presenter takes the newest FINISHED
    // slot (F-B4, accepted by JD: ~1 frame of display latency; sim/input latency untouched).
    // Renderer discipline mirrors the Vulkan worker exactly: the detached renderFrame runs
    // under threeway::RendererGuard, so the game's remaining guarded renderer calls
    // serialise with an in-flight render. Stage 2 removed the upload phase from that
    // discipline: uploads stage in the renderer's lock-free SPSC ring and the executor
    // applies them at the frame seal (drain target recorded per job below), so the game's
    // upload phase takes no guard and never waits on the render. Sim code is not touched
    // by any of this.

    RenderWorker& rw = F.rw;
    SwRenderWorker& sw = F.sw;
    F.threewayOn = litholattice::threeway::enabled() && !display.software && std::string(renderer->name()) == "Vulkan";
    const bool& threewayOn = F.threewayOn;
    F.swThreewayOn = litholattice::threeway::enabled() && display.software && std::string(renderer->name()) == "software";
    const bool& swThreewayOn = F.swThreewayOn;
    if (threewayOn) {
      rw.r = renderer;
      rw.post = [&game] { game.capturePendingScreenshot(); };
      rw.pending = [&game] { return game.screenshotPending(); };
      rw.th = std::thread([&rw] { rw.loop(); });
      logInfo("threeway: ON (LL_THREEWAY=1): main = window/input/game, render thread = GPU/memory + present, sim unchanged (Proposed by JD; co-implemented by JD and Cortana)");
    } else if (swThreewayOn) {
      sw.r = display.software;
      sw.post = [&game] { game.capturePendingScreenshot(); };
      sw.pending = [&game] { return game.screenshotPending(); };
      cSoftwareRenderer::setDetachedPoolMode(true);  // Design B Stage 1: the pool spawns its executor worker (band count unchanged)
      display.software->setUploadRingEnabled(true);  // Design B Stage 2: game uploads stage in the lock-free ring; the executor applies them at the frame seal
      logInfo("threeway: ON (LL_THREEWAY=1): software path, stage 2 async mailbox + upload ring: main = window/input/game + present (newest finished frame), frame job copied and rendered on the band-pool executor, pixels-only slots, uploads lock-free via the ring (no upload guard), sim unchanged (Proposed by JD; co-implemented by JD and Cortana)");
    } else if (litholattice::threeway::enabled()) {
      logInfo("threeway: requested but renderer is %s; no split path for this backend, staying on the single-thread path", renderer->name());
    }
    // Build 50 (2026-10-19, web present fix): the frame-loop state lives
    // in the heap FrameCtx F (aliases above bind the setup code to it), and
    // the loop body / shutdown tail are lkStepFrame()/lkShutdownTail().
    // Native and bench runs iterate lkStepFrame directly — the exact
    // semantics of the original while loop. The interactive WEB run
    // instead registers lkStepFrame as the Emscripten main loop: with a
    // hand-rolled while loop the proxied pthread's task never ends, the
    // browser never gets the task boundary it uses to present the
    // OffscreenCanvas, and the canvas stays black forever even though the
    // game renders, simulates and answers input (JD's itch playtest of
    // build 49, 2026-10-19 12:43 EDT). Bench mode always presented
    // because exiting ends the task and flushes the final frame.
    F.window = window; F.renderer = renderer; F.display = &display;
    F.benchSeconds = benchSeconds; F.runSeconds = runSeconds;
    F.benchPath = benchPath; F.benchJson = benchJson; F.uiShot = uiShot;
    F.noOverlay = noOverlay; F.rname = rname; F.present = present;
#if defined(__EMSCRIPTEN__)
    if (benchSeconds <= 0 &&
        !EM_ASM_INT({ return (typeof process === 'object' && typeof process.versions === 'object' &&
                              typeof process.versions.node === 'string') ? 1 : 0; })) {
      logInfo("web present: interactive loop via emscripten_set_main_loop (task boundary per frame)");
      emscripten_set_main_loop_arg(&lkFrameThunk, &F, 0, true);
    } else {
      logInfo("web present: direct frame loop (bench or Node)");
    }
#endif
    while (lkStepFrame(F)) {}
    lkShutdownTail(F);
#if !defined(__EMSCRIPTEN__)
    if (F.game && F.game->restartRequested()) g_relaunchBackend = F.game->settings().renderer;
#endif
    F.game.reset();   // game dies before the renderer and window, as at the old block-scope exit
    F.input.reset();
  }  // game (sim thread, mesh jobs) shuts down before the renderer and window
  display.renderer.reset();
  logInfo("exit");
  logCloseFile();
#if !defined(__EMSCRIPTEN__)
  if (!g_relaunchBackend.empty()) relaunchWithRenderer(argc, argv, g_relaunchBackend);
#endif
  return 0;
}

static bool lkStepFrame(FrameCtx& F) {
  Window* window = F.window;
  IRenderer* renderer = F.renderer;
  Display& display = *F.display;
  Game& game = *F.game;
  cInputSystem& input = *F.input;
  RenderWorker& rw = F.rw;
  SwRenderWorker& sw = F.sw;
  const bool& threewayOn = F.threewayOn;
  const bool& swThreewayOn = F.swThreewayOn;
  double& benchSeconds = F.benchSeconds;
  double& runSeconds = F.runSeconds;
  const std::string& benchPath = F.benchPath;
  const std::string& uiShot = F.uiShot;
  bool& uiShotDone = F.uiShotDone;
  bool noOverlay = F.noOverlay;
  auto& tStart = F.tStart; auto& tLast = F.tLast; auto& tTitle = F.tTitle; auto& tPerf = F.tPerf;
  BenchRun& br = F.br;
  double &msInput = F.msInput, &msUpdate = F.msUpdate, &msRender = F.msRender, &msPresent = F.msPresent;
  std::vector<double>& perfMs = F.perfMs; std::vector<double>& benchMs = F.benchMs;
  int &frames = F.frames, &perfFrames = F.perfFrames, &hitches = F.hitches;
  double &worstMs = F.worstMs, &perfSumMs = F.perfSumMs, &perfMaxMs = F.perfMaxMs, &avgFrameMs = F.avgFrameMs;
  bool& wasFocused = F.wasFocused;
  litholattice::cFramePacer& pacer = F.pacer;
  int& appliedFpsCap = F.appliedFpsCap;
  int& autoFrames = F.autoFrames; double& autoRenderSum = F.autoRenderSum;
  std::vector<double>& ovMs = F.ovMs;
  size_t& ovHead = F.ovHead;
  auto& ovPrev = F.ovPrev; auto& ovT = F.ovT;
  double& ovFps = F.ovFps; double& ovFrameMs = F.ovFrameMs; int& ovFrames = F.ovFrames;
  std::vector<uint32_t>& ovSaved = F.ovSaved;
  double& ovUpdSum = F.ovUpdSum; int& ovUpdN = F.ovUpdN;
#ifndef NDEBUG
  constexpr bool kDebugBuild = true;
#else
  constexpr bool kDebugBuild = false;
#endif
  constexpr double kBenchWarmup = 3.0;  // mirrors main()'s setup constant of the same name/value
  if (window->shouldClose()) return false;
      if (benchSeconds <= 0 && appliedFpsCap == -1 && game.settings().fpsCap == -1) {  // auto resolve
        if (++autoFrames > 60) autoRenderSum += msRender;
        if (autoFrames == 180) {
          const double avg = autoRenderSum / 120.0;
          const int cap = avg <= 14.0 ? 60 : 30;
          pacer.setCapFps(cap);
          appliedFpsCap = -3;  // resolved-auto marker (live setting changes are still honoured below)
          logInfo("fps cap: auto -> %d FPS (avg render %.2f ms over 120 frames)", cap, avg);
        }
      } else if (const int cap = game.settings().fpsCap; benchSeconds <= 0 && cap != appliedFpsCap && !(appliedFpsCap == -3 && cap == -1)) {
        pacer.setCapFps(cap);
        appliedFpsCap = cap;
        logInfo("fps cap: %d FPS (render.fps_cap, live)", cap);
      }
      pacer.beginFrame();
      litholattice::locktiming::frameEnd();  // 0.0.1.60: per-frame lock-wait totals (no-op unless LL_LOCK_TIMING=1)
      window->pollDisplayToggle();  // Alt+Enter
      const auto tIn = Clock::now();
      input.update(window->input());  // pumps OS events; actions for this frame
      const auto now = Clock::now();
      msInput = msBetween(tIn, now);
      const float dt = float(std::min(0.1, secondsBetween(tLast, now)));
      const double frameMs = msBetween(tLast, now);
      worstMs = std::max(worstMs, frameMs);
      tLast = now;
      if (frames + perfFrames > 0) {  // hitch: a frame over 2x the running average (and over 10 ms)
        if (avgFrameMs > 0 && frameMs > 2.0 * avgFrameMs && frameMs > 10.0 && ++hitches <= 3)  // max 3 lines/s
          logInfo("hitch: frame %.1f ms (avg %.2f ms)", frameMs, avgFrameMs);
        avgFrameMs = avgFrameMs > 0 ? avgFrameMs * 0.95 + frameMs * 0.05 : frameMs;
        perfSumMs += frameMs; perfMaxMs = std::max(perfMaxMs, frameMs); ++perfFrames; perfMs.push_back(frameMs);
        if (benchSeconds > 0 && secondsBetween(tStart, now) > kBenchWarmup) {
          // msUpdate / msRender still hold the previous frame's split (this frameMs ends here).
          benchMs.push_back(frameMs);
          br.minSum += frameMs; ++br.minFrames;
          br.input.push_back(msInput); br.update.push_back(msUpdate - game.lastUploadMs()); br.upload.push_back(game.lastUploadMs());
          br.render.push_back(msRender); br.present.push_back(msPresent); br.uploadBytes.push_back(double(game.lastUploadBytes()));
          br.secInput += msInput; br.secUpdate += msUpdate - game.lastUploadMs(); br.secUpload += game.lastUploadMs();
          br.secRender += msRender; br.secPresent += msPresent; br.secBytes += double(game.lastUploadBytes()); ++br.secFrames;
        }
      }
      if (runSeconds > 0 && secondsBetween(tStart, now) > runSeconds) return false;

      int w, h; window->framebufferSize(w, h);
      // 0.0.1.54 [C54-2] (Proposed by JD; co-implemented by JD and Cortana): LL_BENCH_SHOT_AT=<s> delays the --ui-shot (far-view LOD shots).
      static const double kShotAt = [] { const char* e = std::getenv("LL_BENCH_SHOT_AT"); return e ? std::atof(e) : 0.0; }();
      if (!uiShotDone && secondsBetween(tStart, now) > std::max(kBenchWarmup, kShotAt)) { game.requestUiShot(uiShot); uiShotDone = true; }
      if (benchSeconds > 0) game.setBenchCamera(float(secondsBetween(tStart, now)));
      const auto tUp = Clock::now();
      game.update(dt, w, h);
      if (benchSeconds > 0) game.setBenchCamera(float(secondsBetween(tStart, now)));
      const auto tRender = Clock::now();
      msUpdate = msBetween(tUp, tRender);
      ovUpdSum += msUpdate; ++ovUpdN;  // F56-4
      {
        // 2026-10-15 alt-tab fix: focus edges come from GLFW's focus callback
        // (counted, so a quick away-and-back between two frames is not lost);
        // the attribute poll stays as a fallback.
        int lostN = 0, gainedN = 0;
        window->takeFocusEdges(lostN, gainedN);
        const bool foc = window->focused();
        if (lostN > 0 || (wasFocused && !foc)) game.onFocusChanged(false);
        if (foc && (gainedN > 0 || !wasFocused)) {
          game.onFocusChanged(true);
          window->setCursorCaptured(benchSeconds <= 0 && game.wantsCursorCaptured());  // re-apply: the OS dropped it (1925: never in bench)
        }
        wasFocused = foc;
      }
      if (game.quitRequested()) window->requestClose();
      // Compare against GLFW's REAL cursor mode, not the cached flag.
      {
        // Launch-focus fix (2026-10-15): capture only once the window really
        // has focus (no clipping the cursor from behind another program).
        // Order (JD): focus -> capture -> always-on-top. Topmost is dropped
        // whenever capture is not wanted or focus is lost (alt-tab stays
        // usable) and restored when play recaptures.
        const bool wantCap = benchSeconds <= 0 && game.wantsCursorCaptured() && window->focused();  // Cortana 2026-10-15 1925: no cursor grab / topmost in bench
        if (wantCap != window->cursorReallyCaptured()) window->setCursorCaptured(wantCap);
        window->setTopmost(wantCap && window->cursorReallyCaptured());
      }
      if (w > 0 && h > 0) {
        game.uploadEntities();
        FrameParams fp = game.frameParams(w, h);
        fp.timeSec = float(secondsBetween(tStart, now));
        // 2026-10-16 (capture aid): LL_RENDER_TIME=<sec> pins the render/animation clock so a
        // shot shows an exact twinkle phase. Render-only; the sim clock is untouched.
        static const char* pinT = std::getenv("LL_RENDER_TIME");
        if (pinT) fp.timeSec = float(std::atof(pinT));
        bool lostNow = false;
        if (threewayOn) { rw.submit(fp); lostNow = rw.lost.load(); }  // LL_THREEWAY handshake (Proposed by JD; co-implemented by JD and Cortana)
        else if (swThreewayOn) { sw.submit(fp); lostNow = sw.lost.load(); }  // LL_THREEWAY, software path (Design B stage 1: async mailbox)
        else {
          renderer->renderFrame(fp);
          game.capturePendingScreenshot();  // `screenshot` lands here, pre-present
          lostNow = renderer->deviceLost();
        }
        if (lostNow) {  // F56-1 (Cortana 2026-10-17): clean close instead of a hang / error spam
          static bool closing = false;
          if (!closing) { closing = true; logError("renderer: %s device lost; closing LithoKraft (normal shutdown)", renderer->name());
                          std::fprintf(stderr, "LithoKraft: the GPU device was lost; closing. See logs/lithokraft.log.\n"); }
          window->requestClose();
        }
      }
      const auto tPresent = Clock::now();
      if (display.presenter && display.software) {  // software backend: blit the CPU
        // Design B Stage 1: on the split path the presenter reads the newest FINISHED
        // handoff slot, never the renderer's live framebuffer (a frame may be in flight
        // on the executor). Off the split path this is the framebuffer, exactly as before.
        const SoftwareFramebuffer& fb = swThreewayOn ? sw.acquirePresent() : display.software->framebuffer();  // frame, then swap
        if (fb.width > 0 && fb.height > 0) {
          // Cortana 2026-10-15 1610 (ENGINE): Debug Overlay strip. Drawn
          // after renderFrame and after the screenshot grab, for this present
          // only. The covered rows are saved and restored straight after, so
          // screenshots, the MCP capture (its own renderer) and the next frame
          // never see it.
          const auto tNow = Clock::now();
          const double fms = msBetween(ovPrev, tNow); ovPrev = tNow;
          if (ovMs.size() < 240) ovMs.push_back(fms); else { ovMs[ovHead] = fms; ovHead = (ovHead + 1) % 240; }
          ++ovFrames; if (secondsBetween(ovT, tNow) >= 0.5) { ovFps = ovFrames / secondsBetween(ovT, tNow); ovFrames = 0; ovT = tNow; }
          ovFrameMs = fms;
          const bool full = !noOverlay && (kDebugBuild || benchSeconds > 0 || game.creativeMode()) &&
                            (benchSeconds > 0 || game.debugOverlayToggled());
          const bool mini = false;  // F56-4 (Cortana 2026-10-17): the HUD perf readout (every backend) replaces the software-only mini strip
          const int W = int(fb.width), H = int(fb.height), rows = cSoftwareRenderer::stripRows();
          if ((full || mini) && H > rows) {
            char txt[400];
            const auto ssOpt = game.simStats();
            const double tps = ssOpt ? ssOpt->get().tps : 0.0;
            const int ttps = ssOpt ? ssOpt->get().targetTps : 0;
            if (full) {
              // Stage 1: on the split path the mesh count comes from the last COMPLETED
              // frame (the worker caches it at completion) — reading the renderer's live
              // drawOrder_ here would race an in-flight render on the executor.
              std::vector<double> sorted(ovMs);
              const double low1 = sorted.empty() ? 0.0 : percentile(sorted, 0.99);
              char benchPart[96] = "";
              if (benchSeconds > 0) std::snprintf(benchPart, sizeof benchPart, "  bench %s %.1fs", benchPath.c_str(), secondsBetween(tStart, tNow));
              std::snprintf(txt, sizeof txt, "DEBUG  FPS %.1f  frame %.1f ms (1%% low %.1f)  TPS %.0f/%d  dropped %llu  rthreads %u  meshes %zu  meshQ %zu%s",
                            ovFps, ovFrameMs, low1, tps, ttps, ssOpt ? (unsigned long long)ssOpt->get().droppedTicks : 0ull,
                            cSoftwareRenderer::renderThreads(), swThreewayOn ? sw.lastDrawn.load() : display.software->meshesDrawnLastFrame(), game.meshQueueDepth(), benchPart);
            } else {
              std::snprintf(txt, sizeof txt, "FPS %.0f  TPS %.0f", ovFps, tps);
            }
            auto& px = const_cast<std::vector<uint32_t>&>(fb.pixels);
            ovSaved.assign(px.begin(), px.begin() + size_t(W) * size_t(rows));
            cSoftwareRenderer::drawStatusStrip(px.data(), W, H, txt);
            display.presenter->present(fb.pixels.data(), W, H, w, h);
            std::copy(ovSaved.begin(), ovSaved.end(), px.begin());
          } else {
            display.presenter->present(fb.pixels.data(), W, H, w, h);
          }
        }
        if (swThreewayOn) sw.releasePresent();  // slot: Presenting -> Shown (re-presentable, recyclable)
      }
      window->swap();
      pacer.endFrameAndWait();  // render.fps_cap: sleep to the frame deadline (no-op when uncapped)
      msPresent = msBetween(tPresent, Clock::now());  // present = blit + swap (swap only on GPU backends)
      msRender = msBetween(tRender, tPresent);
      ++frames;

      const double since = secondsBetween(tTitle, now);
      if (since >= 0.5) {
        // Proposed by JD; co-implemented by JD and Cortana (2026-10-16): the window title shows only the game name,
        // build config and version; fps / renderer stats stay in the in-game overlay (F3).
        static const std::string kTitle = [] {
          std::string v = lithokraft::buildVersion();
          if (v.rfind("Alpha ", 0) == 0) v = v.substr(6);
#ifdef NDEBUG
          return std::string("LithoKraft - Release ") + v;
#else
          return std::string("LithoKraft - Debug ") + v;
#endif
        }();
        window->setTitle(kTitle);
        game.setFrameStats(float(frames / since), float(since * 1000.0 / std::max(1, frames)), float(worstMs));
        frames = 0; worstMs = 0; tTitle = now;
      }
      const double perfSince = secondsBetween(tPerf, now);
      if (perfSince >= 1.0) {
        // F56-4 (Proposed by JD; co-implemented by JD and Cortana): FPS / frame-time readout (Graphics setting, HUD text path).
        float gpuMsNow; { litholattice::threeway::RendererGuard rg; gpuMsNow = float(renderer->gpuFrameMs()); }  // LL_THREEWAY
        game.setPerfReadout(float(percentile(perfMs, 0.99)), float(ovUpdN ? ovUpdSum / ovUpdN : 0.0), gpuMsNow);
        ovUpdSum = 0; ovUpdN = 0;
        logInfo("%s p99=%.2fms hitches=%d", game.perfLine(perfFrames / perfSince, perfFrames ? perfSumMs / perfFrames : 0.0, perfMaxMs).c_str(),
                percentile(perfMs, 0.99), hitches);
        if (benchSeconds > 0) {
          const uint64_t mem = processWorkingSetBytes();
          br.memPeak = std::max(br.memPeak, mem);
          const auto ssOpt = game.simStats();
          if (!ssOpt) return true;  // (was: continue — skip the rest of this bench second)
          const SimStats& ss = ssOpt->get();
          if (secondsBetween(tStart, now) > kBenchWarmup) br.tick.push_back(ss.tickMs);
          const double f = std::max(1, br.secFrames);
          std::string streamPart;
          if (cBenchStreamer* bs = game.benchStreamer()) {
            const double genAvg = MsSamples::avg(bs->genMs.second), genP99 = MsSamples::pct(bs->genMs.second, 0.99);
            const double meshAvg = MsSamples::avg(bs->meshMs.second);
            const cBenchStreamer::Second s = bs->takeSecond();
            char buf[256];
            std::snprintf(buf, sizeof buf, " stream gen=%d cols (%.2f/%.2fms) meshed=%d subs (%.3fms) dropped=%d loaded=%d", s.generated,
                          genAvg, genP99, s.meshed, meshAvg, s.dropped, s.loaded);
            streamPart = buf;
          }
          logInfo("bench-sec frame: input=%.3f update=%.3f upload=%.3f render=%.3f present=%.3f ms, upload=%.0f B/frame, tick=%.3f ms, ram=%.1f MB%s",
                  br.secInput / f, br.secUpdate / f, br.secUpload / f, br.secRender / f, br.secPresent / f, br.secBytes / f, ss.tickMs, double(mem) / 1048576.0,
                  streamPart.c_str());
          if (benchPath == "soak" && br.minFrames > 0 && secondsBetween(tStart, now) - kBenchWarmup >= 60.0 * (br.minuteNo + 1)) {
            br.minuteFrame.push_back(br.minSum / br.minFrames); br.minuteRam.push_back(double(mem) / 1048576.0);
            logInfo("soak minute=%d frame_avg=%.3fms ram=%.1fMB (drift vs minute 1: %+.3f ms, %+.1f MB)", br.minuteNo + 1, br.minuteFrame.back(),
                    br.minuteRam.back(), br.minuteFrame.back() - br.minuteFrame.front(), br.minuteRam.back() - br.minuteRam.front());
            br.minSum = 0; br.minFrames = 0; ++br.minuteNo;
          }
          br.secInput = br.secUpdate = br.secUpload = br.secRender = br.secPresent = br.secBytes = 0; br.secFrames = 0;
        }
        perfMs.clear(); perfFrames = 0; hitches = 0; perfSumMs = perfMaxMs = 0; tPerf = now;
      }
  return true;
}

static void lkShutdownTail(FrameCtx& F) {
  Window* window = F.window;
  IRenderer* renderer = F.renderer;
  Display& display = *F.display;
  Game& game = *F.game;
  RenderWorker& rw = F.rw;
  SwRenderWorker& sw = F.sw;
  const bool& swThreewayOn = F.swThreewayOn;
  double& benchSeconds = F.benchSeconds;
  const std::string& benchPath = F.benchPath;
  const std::string& benchJson = F.benchJson;
  const std::string& present = F.present;
  BenchRun& br = F.br;
  std::vector<double>& benchMs = F.benchMs;
  auto& mcpServer = F.mcpServer;
  auto& mcpThread = F.mcpThread;
#if !defined(_WIN32)
  auto& mcpTransport = F.mcpTransport;
  auto& mcpSavedStdoutFd = F.mcpSavedStdoutFd;
#endif
    rw.stop();  // LL_THREEWAY: join the render thread before shutdown (Proposed by JD; co-implemented by JD and Cortana)
    sw.stop();  // LL_THREEWAY software path (stage 1): drain the in-flight job + land a trailing capture
    if (swThreewayOn) {  // Design B Stage 2: ring accounting, then the ring closes (later teardown uploads apply directly again)
      const auto rs = display.software->uploadRingStats();
      logInfo("threeway upload ring: pushed=%llu refused=%llu max-occupancy=%llu drained=%llu applied=%llu coalesced=%llu clears=%llu (game spill: events=%llu max=%zu discarded-on-switch=%llu, depth-at-stop=%zu)",
              (unsigned long long)rs.pushed, (unsigned long long)rs.pushFull, (unsigned long long)rs.maxOccupancy,
              (unsigned long long)rs.drained, (unsigned long long)rs.applied, (unsigned long long)rs.coalesced,
              (unsigned long long)rs.clears, (unsigned long long)game.ringSpillEvents(), game.ringSpillMax(),
              (unsigned long long)game.ringSpillDroppedOnSwitch(), game.ringSpillDepth());
      display.software->setUploadRingEnabled(false);
    }
    if (swThreewayOn) cSoftwareRenderer::setDetachedPoolMode(false);  // pool returns to its pre-split worker shape before teardown
    // MCP shutdown (P2c, F-M9): stop + join the stdio thread and drop the
    // server BEFORE the Game (and its adapters) is destroyed at scope
    // exit; restore the real stdout for the bench summary / exit path.
    if (mcpServer) {
      game.setMcpServer(nullptr);
#if !defined(_WIN32)
      if (mcpTransport) mcpTransport->requestStop();
      if (mcpThread.joinable()) mcpThread.join();
#else
      // Fix run 2 (2026-10-23, R-RACE-1): was detach() + reset(), a use-after-free
      // while the thread sat in the blocking stdin read. Now: cancel the read,
      // join, THEN free. If the read cannot be cancelled in time the server is
      // leaked on purpose (the stop flag guarantees no further dispatch).
      if (mcpThread.joinable() && F.mcpDone &&
          !litholattice::mcp::stopStdioThread(mcpThread, *F.mcpDone, std::chrono::milliseconds(2000))) {
        logError("mcp: stdin reader did not stop within 2 s; leaking the server instead of freeing it under a live thread");
        mcpThread.detach();
        (void)mcpServer.release();
      }
#endif
      mcpServer.reset();
#if !defined(_WIN32)
      mcpTransport.reset();
      if (mcpSavedStdoutFd >= 0) {
        std::fflush(stdout);
        ::dup2(mcpSavedStdoutFd, STDOUT_FILENO);
        ::close(mcpSavedStdoutFd);
        mcpSavedStdoutFd = -1;
      }
#endif
    }
    if (benchSeconds > 0) {
      br.memEnd = processWorkingSetBytes();
      const double avg = avgOf(benchMs), p50 = percentile(benchMs, 0.5), p99 = percentile(benchMs, 0.99), mx = percentile(benchMs, 1.0);
      cBenchStreamer* bs = game.benchStreamer();
      const double genAvg = bs ? MsSamples::avg(bs->genMs.run) : 0, genP99 = bs ? MsSamples::pct(bs->genMs.run, 0.99) : 0;
      const double meshAvg = bs ? MsSamples::avg(bs->meshMs.run) : 0, meshP99 = bs ? MsSamples::pct(bs->meshMs.run, 0.99) : 0;
      const double genRate = bs ? double(bs->totalGenerated) / benchSeconds : 0, meshRate = bs ? double(bs->totalMeshed) / benchSeconds : 0;
      logInfo("bench renderer=%s present=%s path=%s seconds=%.0f frames=%zu rthreads=%u avg=%.3fms p50=%.3fms p99=%.3fms max=%.3fms "
              "input=%.3f update=%.3f upload=%.3f render=%.3f present=%.3f ms upload=%.0f B/frame tick=%.3f/%.3fms gen=%.1f cols/s (%.2f/%.2fms) "
              "mesh=%.1f subs/s (%.3f/%.3fms) ram=%.1f->%.1f MB (peak %.1f)",
              renderer->name(), present.c_str(), benchPath.c_str(), benchSeconds, benchMs.size(), cSoftwareRenderer::renderThreads() /* Cortana 2026-10-15 1450 */, avg, p50, p99, mx, avgOf(br.input),
              avgOf(br.update), avgOf(br.upload), avgOf(br.render), avgOf(br.present), avgOf(br.uploadBytes), avgOf(br.tick), percentile(br.tick, 0.99), genRate,
              genAvg, genP99, meshRate, meshAvg, meshP99, double(br.memStart) / 1048576.0, double(br.memEnd) / 1048576.0,
              double(br.memPeak) / 1048576.0);
      if (!br.minuteFrame.empty())
        logInfo("soak drift: frame %+.3f ms, ram %+.1f MB over %zu minutes (same-path minutes: %+.3f ms)", br.minuteFrame.back() - br.minuteFrame.front(),
                br.minuteRam.back() - br.minuteRam.front(), br.minuteFrame.size(),
                br.minuteFrame.size() >= 3 ? br.minuteFrame.back() - br.minuteFrame[br.minuteFrame.size() % 2 ? 0 : 1] : 0.0);
      std::printf("BENCH %s present=%s display=%s path=%s frames=%zu avg=%.3f ms p99=%.3f ms\n", renderer->name(), present.c_str(),
                  displayModeName(window->display().mode), benchPath.c_str(), benchMs.size(), avg, p99);
      if (!benchJson.empty()) {
        if (std::FILE* f = openForWrite(benchJson.c_str())) {
          std::fprintf(f,
                       "{\n  \"renderer\": \"%s\", \"present\": \"%s\", \"display\": \"%s\", \"path\": \"%s\", \"seconds\": %.0f, \"frames\": %zu, \"render_threads\": %u,\n"
                       "  \"frame_ms\": {\"avg\": %.4f, \"p50\": %.4f, \"p99\": %.4f, \"max\": %.4f},\n"
                       "  \"main_thread_ms\": {\"input\": %.4f, \"update\": %.4f, \"upload\": %.4f, \"render\": %.4f, \"present\": %.4f},\n"
                       "  \"upload_bytes_per_frame\": %.0f, \"sim_tick_ms\": {\"avg\": %.4f, \"p99\": %.4f},\n"
                       "  \"stream\": {\"columns_per_s\": %.2f, \"gen_ms_avg\": %.4f, \"gen_ms_p99\": %.4f, \"subchunks_per_s\": %.2f, "
                       "\"mesh_ms_avg\": %.4f, \"mesh_ms_p99\": %.4f},\n"
                       "  \"ram_mb\": {\"start\": %.1f, \"end\": %.1f, \"peak\": %.1f},\n  \"soak_drift\": {\"frame_ms\": %.4f, \"ram_mb\": %.1f, \"minutes\": %zu}\n}\n",
                       renderer->name(), present.c_str(), displayModeName(window->display().mode), benchPath.c_str(), benchSeconds, benchMs.size(), cSoftwareRenderer::renderThreads() /* Cortana 2026-10-15 1450 */, avg, p50, p99, mx, avgOf(br.input),
                       avgOf(br.update), avgOf(br.upload), avgOf(br.render), avgOf(br.present), avgOf(br.uploadBytes), avgOf(br.tick), percentile(br.tick, 0.99),
                       genRate, genAvg, genP99, meshRate, meshAvg, meshP99, double(br.memStart) / 1048576.0, double(br.memEnd) / 1048576.0,
                       double(br.memPeak) / 1048576.0,
                       br.minuteFrame.empty() ? 0.0 : br.minuteFrame.back() - br.minuteFrame.front(),
                       br.minuteRam.empty() ? 0.0 : br.minuteRam.back() - br.minuteRam.front(), br.minuteFrame.size());
          std::fclose(f);
        } else {
          logError("bench: cannot write %s", benchJson.c_str());
        }
      }
    }
}

#if defined(__EMSCRIPTEN__)
static void lkFrameThunk(void* p) {
  FrameCtx& F = *static_cast<FrameCtx*>(p);
  if (!lkStepFrame(F)) {
    emscripten_cancel_main_loop();
    lkShutdownTail(F);
    F.game.reset();
    F.input.reset();
    std::exit(0);
  }
}
#endif
