#pragma once
// Nemo View window: a separate pop-up window, beside the main game window, that shows the ASCII
// picture Little Nemo reads. When Nemo has just looked (vision.capture_ascii), it shows exactly
// those bytes; otherwise it builds a live preview of the player's view the same way, on its own
// renderer. Its own thread, below-normal priority; it never touches the game's renderer or sim.
// The game shares data with it only through the senses adapter's snapshot and cNemoViewHub.
// Open with the console cvar render.nemo_view 1 (or LL_NEMO_VIEW=1 at launch); 0 closes it.
// render.nemo_view_style: 0 matrix green, 1 true palette, 2 depth heat, 3 object colours.
// Windows only; elsewhere setWanted() is a no-op.
#include <atomic>
#include <thread>

namespace lithokraft {

class cMcpSensesAdapter;

class cNemoViewWindow {
 public:
  static cNemoViewWindow& instance();
  ~cNemoViewWindow();
  /// Opens (true) or closes (false) the window. Idempotent; game thread.
  void setWanted(bool wanted, cMcpSensesAdapter* adapter);
  void setStyle(int style) { style_.store(style < 0 || style > 3 ? 0 : style); }
  void setView(bool objects, int cols) {
    objects_.store(objects);
    cols_.store(cols == 40 || cols == 96 ? cols : 80);
  }
  bool open() const { return open_.load(); }

 private:
  cNemoViewWindow() = default;
  void run(cMcpSensesAdapter* adapter);
  std::thread thread_;
  bool wanted_ = false;                 ///< game thread only
  std::atomic<bool> quit_{false};
  std::atomic<bool> open_{false};
  std::atomic<int> style_{0};
  std::atomic<bool> objects_{true};
  std::atomic<int> cols_{80};
};

}  // namespace lithokraft
