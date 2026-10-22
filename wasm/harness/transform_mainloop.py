#!/usr/bin/env python3
"""Build-50 main-loop extraction for game/LithoKraft/src/app/main.cpp.

Moves the frame-loop body and shutdown tail out of main() into
lkStepFrame()/lkShutdownTail() over a heap FrameCtx, so the interactive
web build can drive the loop through emscripten_set_main_loop (the only
way the browser gets a task boundary — and therefore presents the
canvas — between frames). Native and bench paths call lkStepFrame in a
plain while loop: identical semantics to the original code.
Every replacement is exact-match asserted.
"""
import sys

path = sys.argv[1]
src = open(path).read()

def rep(old, new, count=1):
    global src
    n = src.count(old)
    assert n == count, f"anchor count {n} != {count} for: {old[:80]!r}"
    src = src.replace(old, new)

# ---------------------------------------------------------------- structs cut
def cut_block(start_marker, end_marker):
    """Remove and return the text from start_marker line through end_marker line."""
    global src
    i = src.index(start_marker)
    j = src.index(end_marker, i) + len(end_marker)
    block = src[i:j]
    src = src[:i] + src[j:]
    return block

rw_block = cut_block("    struct RenderWorker {", "    } rw;")
sw_block = cut_block("    struct SwRenderJob {", "    } sw;")

# file-scope versions: unindent the struct keyword lines, close with };
def file_scope(block, first_old, first_new, last_old):
    assert block.startswith(first_old), block[:60]
    block = first_new + block[len(first_old):]
    assert block.rstrip().endswith(last_old)
    block = block.rstrip()[: -len(last_old)] + "};"
    return block

rw_fs = file_scope(rw_block, "    struct RenderWorker {", "struct RenderWorker {", "} rw;")
sw_fs = file_scope(sw_block, "    struct SwRenderJob {", "struct SwRenderJob {", "} sw;")
# Sw block contains three structs; fix the other two keyword lines' indent too.
sw_fs = sw_fs.replace("\n    struct SwFrameSlot {", "\nstruct SwFrameSlot {")
sw_fs = sw_fs.replace("\n    struct SwRenderWorker {", "\nstruct SwRenderWorker {")

# ------------------------------------------------------- loop body + tail cut
while_line = "    while (!window->shouldClose()) {\n"
i = src.index(while_line)
body_start = i + len(while_line)
tail_start_marker = "    rw.stop();  // LL_THREEWAY: join the render thread"
tail_start = src.index(tail_start_marker, body_start)
body = src[body_start:tail_start]
# body must end with the while loop's closing brace line
assert body.rstrip().endswith("    }"), body[-200:]
body = body.rstrip()[: -len("    }")].rstrip() + "\n"

tail_end_marker = "  }  // game (sim thread, mesh jobs) shuts down before the renderer and window"
tail_end = src.index(tail_end_marker, tail_start)
tail = src[tail_start:tail_end]

# body transforms: break -> return false, the one bench continue -> return true
assert body.count("if (runSeconds > 0 && secondsBetween(tStart, now) > runSeconds) break;") == 1
body = body.replace("if (runSeconds > 0 && secondsBetween(tStart, now) > runSeconds) break;",
                    "if (runSeconds > 0 && secondsBetween(tStart, now) > runSeconds) return false;")
assert body.count("if (!ssOpt) continue;") == 1
body = body.replace("if (!ssOpt) continue;", "if (!ssOpt) return true;  // (was: continue — skip the rest of this bench second)")

# remove the whole loop+tail region from main, substitute the dispatch
dispatch = """    // Build 50 (2026-10-19, web present fix): the frame-loop state lives
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
    if (benchSeconds <= 0 && EM_ASM_INT({ return (typeof window !== 'undefined') ? 1 : 0; }))
      emscripten_set_main_loop_arg(&lkFrameThunk, &F, 0, true);
#endif
    while (lkStepFrame(F)) {}
    lkShutdownTail(F);
    F.game.reset();   // game dies before the renderer and window, as at the old block-scope exit
    F.input.reset();
"""
src = src[:i] + dispatch + src[tail_end:]

# ------------------------------------------------------------- in-main edits
rep('#include "selftest.h"',
    '#include "selftest.h"\n#if defined(__EMSCRIPTEN__)\n#include <emscripten/emscripten.h>  // build 50: emscripten_set_main_loop_arg for the interactive web present fix\n#endif')

rep("    JobPool pool(std::max(1u, std::max(2u, std::thread::hardware_concurrency()) - 2));",
    "    auto lkFOwner = std::make_unique<FrameCtx>(); FrameCtx& F = *lkFOwner;  // build 50: frame-loop state on the heap (survives the Emscripten main-loop unwind; see FrameCtx)\n"
    "    JobPool pool(std::max(1u, std::max(2u, std::thread::hardware_concurrency()) - 2));")

rep("    InputSystem input(loadBindings(argv[0]), inputContexts());",
    "    F.input = std::make_unique<InputSystem>(loadBindings(argv[0]), inputContexts()); InputSystem& input = *F.input;")

rep("    Game game(cfg, *renderer, pool, resources, audio, input);",
    "    F.game = std::make_unique<Game>(cfg, *renderer, pool, resources, audio, input); Game& game = *F.game;")

rep("""    std::unique_ptr<litholattice::mcp::McpServer> mcpServer;
    std::thread mcpThread;
#if !defined(_WIN32)
    std::unique_ptr<McpStdioTransport> mcpTransport;
    int mcpSavedStdoutFd = -1;
#endif""",
    """    auto& mcpServer = F.mcpServer;
    auto& mcpThread = F.mcpThread;
#if !defined(_WIN32)
    auto& mcpTransport = F.mcpTransport;
    auto& mcpSavedStdoutFd = F.mcpSavedStdoutFd;
#endif""")

rep("""    const auto tStart = Clock::now();
    auto tLast = tStart, tTitle = tStart, tPerf = tStart;""",
    """    F.tStart = Clock::now(); F.tLast = F.tStart; F.tTitle = F.tStart; F.tPerf = F.tStart;
    auto& tStart = F.tStart; auto& tLast = F.tLast; auto& tTitle = F.tTitle; auto& tPerf = F.tPerf;""")

rep("    bool uiShotDone = uiShot.empty() || benchSeconds <= 0;",
    "    F.uiShotDone = uiShot.empty() || benchSeconds <= 0; auto& uiShotDone = F.uiShotDone;")

rep("    BenchRun br;", "    BenchRun& br = F.br;")

rep("    double msInput = 0, msUpdate = 0, msRender = 0, msPresent = 0;",
    "    double &msInput = F.msInput, &msUpdate = F.msUpdate, &msRender = F.msRender, &msPresent = F.msPresent;")

rep("    std::vector<double> perfMs, benchMs;  // this second's frame times (p99), the bench's",
    "    auto& perfMs = F.perfMs; auto& benchMs = F.benchMs;  // this second's frame times (p99), the bench's")

rep("    int frames = 0, perfFrames = 0, hitches = 0; double worstMs = 0, perfSumMs = 0, perfMaxMs = 0, avgFrameMs = 0;",
    "    int &frames = F.frames, &perfFrames = F.perfFrames, &hitches = F.hitches; double &worstMs = F.worstMs, &perfSumMs = F.perfSumMs, &perfMaxMs = F.perfMaxMs, &avgFrameMs = F.avgFrameMs;")

rep("    bool wasFocused = window->focused();",
    "    F.wasFocused = window->focused(); auto& wasFocused = F.wasFocused;")

rep("    litholattice::FramePacer pacer;", "    auto& pacer = F.pacer;")

rep("    int appliedFpsCap = initialCap;",
    "    F.appliedFpsCap = initialCap; auto& appliedFpsCap = F.appliedFpsCap;")

rep("    int autoFrames = 0; double autoRenderSum = 0;",
    "    auto& autoFrames = F.autoFrames; auto& autoRenderSum = F.autoRenderSum;")

# kDebugBuild leaves main; lkStepFrame defines its own identical copy.
rep("""#ifndef NDEBUG
    constexpr bool kDebugBuild = true;
#else
    constexpr bool kDebugBuild = false;
#endif
""", "")

rep("    std::vector<double> ovMs;  ovMs.reserve(240);  // ring for the 1% low",
    "    auto& ovMs = F.ovMs;  ovMs.reserve(240);  // ring for the 1% low")

rep("    size_t ovHead = 0; auto ovPrev = Clock::now(); double ovFps = 0, ovFrameMs = 0; int ovFrames = 0; auto ovT = ovPrev;",
    "    auto& ovHead = F.ovHead; F.ovPrev = Clock::now(); auto& ovPrev = F.ovPrev; auto& ovFps = F.ovFps; auto& ovFrameMs = F.ovFrameMs; auto& ovFrames = F.ovFrames; F.ovT = F.ovPrev; auto& ovT = F.ovT;")

rep("    std::vector<uint32_t> ovSaved;", "    auto& ovSaved = F.ovSaved;")

rep("    double ovUpdSum = 0; int ovUpdN = 0;  // F56-4 (Cortana 2026-10-17): main-thread update ms for the perf readout",
    "    auto& ovUpdSum = F.ovUpdSum; auto& ovUpdN = F.ovUpdN;  // F56-4 (Cortana 2026-10-17): main-thread update ms for the perf readout")

# rw / sw alias insertions at the cut sites (the cuts left blank gaps)
rep("    } rw;\n", "    RenderWorker& rw = F.rw;\n") if False else None
# After cutting, the text where the structs were is just surrounding code;
# insert aliases right before the threeway flag definitions instead:
rep('    const bool threewayOn = litholattice::threeway::enabled() && !display.software && std::string(renderer->name()) == "Vulkan";',
    "    RenderWorker& rw = F.rw;\n"
    "    SwRenderWorker& sw = F.sw;\n"
    "    F.threewayOn = litholattice::threeway::enabled() && !display.software && std::string(renderer->name()) == \"Vulkan\";\n"
    "    const bool& threewayOn = F.threewayOn;")
rep('    const bool swThreewayOn = litholattice::threeway::enabled() && display.software && std::string(renderer->name()) == "software";',
    '    F.swThreewayOn = litholattice::threeway::enabled() && display.software && std::string(renderer->name()) == "software";\n'
    '    const bool& swThreewayOn = F.swThreewayOn;')

# ------------------------------------------------------- file-scope additions
frame_ctx = '''
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
  std::unique_ptr<InputSystem> input;
  std::unique_ptr<litholattice::mcp::McpServer> mcpServer;
  std::thread mcpThread;
#if !defined(_WIN32)
  std::unique_ptr<McpStdioTransport> mcpTransport;
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
  litholattice::FramePacer pacer;
  int appliedFpsCap = 0;
  int autoFrames = 0; double autoRenderSum = 0;
  std::vector<double> ovMs;
  size_t ovHead = 0;
  std::chrono::steady_clock::time_point ovPrev{}, ovT{};
  double ovFps = 0, ovFrameMs = 0; int ovFrames = 0;
  std::vector<uint32_t> ovSaved;
  double ovUpdSum = 0; int ovUpdN = 0;
};

static bool lkStepFrame(FrameCtx& F);
static void lkShutdownTail(FrameCtx& F);
#if defined(__EMSCRIPTEN__)
static void lkFrameThunk(void* p);
#endif

'''

step_aliases = '''  Window* window = F.window;
  IRenderer* renderer = F.renderer;
  Display& display = *F.display;
  Game& game = *F.game;
  InputSystem& input = *F.input;
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
  litholattice::FramePacer& pacer = F.pacer;
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
'''

step_fn = ("\nstatic bool lkStepFrame(FrameCtx& F) {\n" + step_aliases + body + "  return true;\n}\n")

tail_aliases = '''  Window* window = F.window;
  IRenderer* renderer = F.renderer;
  Display& display = *F.display;
  Game& game = *F.game;
  RenderWorker& rw = F.rw;
  SwRenderWorker& sw = F.sw;
  const bool& threewayOn = F.threewayOn;
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
'''

tail_fn = ("\nstatic void lkShutdownTail(FrameCtx& F) {\n" + tail_aliases + tail + "}\n")

thunk = '''
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
'''

insert_at = src.index("int main(int argc, char** argv) {")
src = src[:insert_at] + rw_fs + "\n\n" + sw_fs + "\n" + frame_ctx + src[insert_at:]
src = src.rstrip() + "\n" + step_fn + tail_fn + thunk

open(path, "w").write(src)
print("transform OK; new size", len(src))
