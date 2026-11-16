// Folder picker: see cFolderPicker.h. 2026-10-31 (JD: Load World).
#include "lithokraft/game/cFolderPicker.h"
#include "litholattice/core/thread_name.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shobjidl.h>
#include <filesystem>
#endif
#include <atomic>
#include <thread>

namespace lithokraft {

namespace {
// 0 idle, 1 open (worker owns g_out), 2 closed chose, 3 closed cancelled (main owns g_out).
std::atomic<int> g_state{0};
std::string g_out;
}  // namespace

bool cFolderPicker::start(const std::string& title, const std::string& startDir) {
  int idle = 0;
  if (!g_state.compare_exchange_strong(idle, 1, std::memory_order_acq_rel)) return false;
  std::thread([title, startDir] { ::litholattice::setCurrentThreadName("lk-folder-pick");
    std::string o;
    const bool ok = pick(title, startDir, o);
    g_out = std::move(o);
    g_state.store(ok ? 2 : 3, std::memory_order_release);
  }).detach();
  return true;
}

bool cFolderPicker::poll(bool& chose, std::string& out) {
  const int s = g_state.load(std::memory_order_acquire);
  if (s < 2) return false;
  chose = s == 2;
  out = std::move(g_out);
  g_out.clear();
  g_state.store(0, std::memory_order_release);
  return true;
}

bool cFolderPicker::pick(const std::string& title, const std::string& startDir, std::string& out) {
#if defined(_WIN32)
  const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  bool ok = false;
  IFileOpenDialog* dlg = nullptr;
  if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    const std::wstring wt = std::filesystem::path(title).wstring();
    dlg->SetTitle(wt.c_str());
    if (!startDir.empty()) {
      IShellItem* start = nullptr;
      const std::wstring ws = std::filesystem::path(startDir).wstring();
      if (SUCCEEDED(SHCreateItemFromParsingName(ws.c_str(), nullptr, IID_PPV_ARGS(&start)))) { dlg->SetFolder(start); start->Release(); }
    }
    if (SUCCEEDED(dlg->Show(nullptr))) {
      IShellItem* item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR path = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
          out = std::filesystem::path(path).string();
          ok = true;
          CoTaskMemFree(path);
        }
        item->Release();
      }
    }
    dlg->Release();
  }
  if (SUCCEEDED(init)) CoUninitialize();
  return ok;
#else
  (void)title; (void)startDir; (void)out;
  return false;
#endif
}

}  // namespace lithokraft
