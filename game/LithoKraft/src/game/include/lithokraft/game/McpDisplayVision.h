#pragma once
// SCRATCH RESEARCH (vulkfb, 2026-11-11, JD t5132u/t5136u): opt-in display-framebuffer vision for
// MCP pilots. With LL_MCP_DISPLAY_VISION=1 the MCP tool vision.capture_display asks the game for the
// NEXT frame the ACTIVE display renderer draws (Vulkan: the image-to-host copy of the swapchain image
// recorded before present; gl33: glReadPixels of the back buffer; software: its framebuffer). This is
// the player's actual on-screen view, not the senses adapter's own 160x90 software cone.
// Default (env unset): enabled() == false, no grab is ever armed, behaviour is unchanged.
//
// Threading: request()/waitFrame() run on the MCP server thread; pending()/arm()/publish() run on the
// thread that calls Game::capturePendingScreenshot (main).
// MUTEX-FREE (scratch, 2026-11-11, JD t5192u): the std::mutex is replaced by a triple-buffer ownership
// handoff. The producer (main, single owner) fills its private back slot, then swaps it into the shared
// middle slot with one atomic exchange (publish edge). The consumer (MCP thread, single owner) swaps the
// middle slot into its private front slot only when the FRESH bit is set (acquire edge). No slot is ever
// touched by two threads at once, and neither side waits on the other.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace lithokraft {

class cMcpDisplayVision {
 public:
  static cMcpDisplayVision& instance() {
    static cMcpDisplayVision v;
    return v;
  }
  static bool enabled() {
    static const bool on = [] {
      const char* e = std::getenv("LL_MCP_DISPLAY_VISION");
      return e && std::strcmp(e, "1") == 0;
    }();
    return on;
  }
  struct Frame {
    std::vector<uint8_t> rgb;  ///< top-row-first RGB24 at the display resolution
    int width = 0, height = 0;
    uint64_t tick = 0;
    uint64_t seq = 0;          ///< request sequence this frame answers
    std::string renderer;      ///< "vulkan" | "gl33" | "software" (the active display renderer)
  };
  // ---- MCP thread ----
  uint64_t request() { return ++requested_; }
  /// Waits until a frame drawn after request `seq` was published (or timeout).
  bool waitFrame(uint64_t seq, int timeoutMs, Frame& out) {
    const auto t0 = std::chrono::steady_clock::now();
    for (;;) {
      if (mid_.load(std::memory_order_acquire) & kFresh) {
        const unsigned prev = mid_.exchange(front_, std::memory_order_acq_rel);  // take ownership
        front_ = prev & kIdx;
      }
      const Frame& f = slots_[front_];
      if (f.seq >= seq && f.width > 0) { out = f; return true; }
      if (std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(timeoutMs)) return false;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }
  // ---- game (capture) thread ----
  bool wanted() const { return enabled() && requested_.load() > armedFor_.load() && !armed_.load(); }
  bool armed() const { return armed_.load(); }
  bool pending() const { return enabled() && (armed() || requested_.load() > armedFor_.load()); }
  void arm() { armedFor_.store(requested_.load()); armed_.store(true); }
  void disarm() { armed_.store(false); }
  void publish(const std::vector<uint8_t>& rgb, int w, int h, uint64_t tick, const char* rendererName) {
    Frame& f = slots_[back_];  // producer-owned slot: no other thread can see it
    f.rgb = rgb;
    f.width = w;
    f.height = h;
    f.tick = tick;
    f.seq = armedFor_.load();
    const std::string rn = rendererName ? rendererName : "";
    f.renderer = rn == "Vulkan" ? "vulkan" : (rn.rfind("OpenGL", 0) == 0 ? "gl33" : (rn == "software" ? "software" : rn));
    const unsigned prev = mid_.exchange(back_ | kFresh, std::memory_order_acq_rel);  // publish edge
    back_ = prev & kIdx;
    armed_.store(false);
  }

 private:
  std::atomic<uint64_t> requested_{0};
  std::atomic<uint64_t> armedFor_{0};
  std::atomic<bool> armed_{false};
  static constexpr unsigned kIdx = 3u, kFresh = 4u;
  Frame slots_[3];
  unsigned back_ = 0;                 ///< producer-owned slot index (main thread only)
  std::atomic<unsigned> mid_{1u};     ///< shared handoff slot index | kFresh
  unsigned front_ = 2;                ///< consumer-owned slot index (MCP thread only)
};

}  // namespace lithokraft
