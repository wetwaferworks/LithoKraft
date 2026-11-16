#pragma once
// Nemo View window: a separate pop-up window, beside the main game window, that shows the ASCII
// picture Little Nemo reads. When Nemo has just looked (vision.capture_ascii), it shows exactly
// those bytes; otherwise it builds a live preview of the player's view the same way, on its own
// renderer. Its own thread, below-normal priority; it never touches the game's renderer or sim.
// The game shares data with it only through the senses adapter's snapshot and cNemoViewHub.
// Open with the console cvar render.nemo_view 1 (or LL_NEMO_VIEW=1 at launch); 0 closes it.
// 2 opens the same window maximised, for a playable full-window look.
// render.nemo_view_style: 0 matrix green, 1 true palette, 2 depth heat, 3 object colours,
// 4 solid (material fill + shaded ink, the playable default).
// Grid size is a renderer / config option (render.ascii_grid, LL_ASCII_GRID): 80x45,
// 96x54, 160x90 (display default), 180x80, 240x135. The hashing / record default stays
// 80x45. This window only consumes the finished frame; a later main-window ASCII play
// mode can point the same renderer at the game view without changing this class.
// Keys 1-5 in the pop-up pick those five sizes; [ and ] cycle them.
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
  /// If maximize is true the window opens maximised (render.nemo_view 2).
  void setWanted(bool wanted, cMcpSensesAdapter* adapter, bool maximize = false);
  void setStyle(int style) { style_.store(style < 0 || style > 4 ? 4 : style); }
  /// Preview grid. Size is supplied by the renderer config, not invented here.
  void setGrid(bool objects, int cols, int rows, int captureW, int captureH) {
    objects_.store(objects);
    cols_.store(cols);
    rows_.store(rows);
    captureW_.store(captureW);
    captureH_.store(captureH);
  }
  void setView(bool objects, int cols);
  /// Pop-up key request (1-5, [ ]): column count, or 0 if none. Game thread applies it to the cvar.
  void requestGrid(int cols) { gridRequest_.store(cols); }
  int takeGridRequest() { return gridRequest_.exchange(0); }
  bool open() const { return open_.load(); }

 private:
  cNemoViewWindow() = default;
  void run(cMcpSensesAdapter* adapter);
  std::thread thread_;
  bool wanted_ = false;                 ///< game thread only
  bool maximizeWanted_ = false;         ///< game thread only
  std::atomic<bool> quit_{false};
  std::atomic<bool> open_{false};
  std::atomic<bool> maximize_{false};
  std::atomic<int> style_{4};
  std::atomic<bool> objects_{true};
  std::atomic<int> cols_{160};       ///< display default 160x90; hashing default stays 80x45
  std::atomic<int> rows_{90};
  std::atomic<int> captureW_{320};
  std::atomic<int> captureH_{180};
  std::atomic<int> gridRequest_{0};
};

}  // namespace lithokraft
