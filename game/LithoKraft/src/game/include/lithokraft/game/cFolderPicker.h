// Folder picker (2026-10-31, JD: Load World). The OS's own folder dialog (Windows: the shell
// IFileOpenDialog in FOS_PICKFOLDERS mode; no external library). Modal on the calling (main)
// thread; the sim keeps ticking on its own thread while it is open.
#pragma once
#include <string>

namespace lithokraft {

class cFolderPicker final {
 public:
  /// True with `out` set when the user chose a folder; false when cancelled or unsupported.
  static bool pick(const std::string& title, const std::string& startDir, std::string& out);
  /// 2026-10-31 (JD: Open world folder hang): runs pick() on its own detached thread and returns at
  /// once (false when one is already open). The game loop never waits: poll() each frame.
  static bool start(const std::string& title, const std::string& startDir);
  /// Lock-free poll: true once the dialog closed; `chose` and `out` carry the result.
  static bool poll(bool& chose, std::string& out);
};

}  // namespace lithokraft
