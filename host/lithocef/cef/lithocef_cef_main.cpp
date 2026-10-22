// lithocef_cef_main.cpp — the minimal REAL LithoCEF host (Phase 2).
//
// A CEF-backed shell in JD's layering: this process plugs into the
// hardware layer (X11 window under the OS), boots its first payload —
// the shipped web build's GL page (wasm/full/gl.html) — and reports
// what the payload says through the console/load handlers. Modeled on
// CEF's own cefsimple sample structure (the project's canonical minimal
// host), with LithoCEF additions kept to logging + a timed clean exit
// so the run can be smoke-tested headlessly under Xvfb.
//
// Payload selection: LITHOCEF_PAYLOAD_URL env var, else the default
// file:// URL of the in-tree full-test GL page. (A file:// payload
// cannot grant SharedArrayBuffer — cross-origin isolation needs the
// package's serve.py headers — so a full game boot is NOT claimed from
// this run; see the run summary. What this proves: the CEF host builds,
// links, starts, creates its browser, and loads the payload page.)
#include <cstdlib>
#include <string>
#include <unistd.h> // getcwd

#include "include/cef_app.h"
#include "include/cef_version.h" // CEF_VERSION (not pulled in by cef_app.h)
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_helpers.h"

namespace {

std::string PayloadUrl() {
  if (const char* e = std::getenv("LITHOCEF_PAYLOAD_URL"); e && *e) return e;
  return "file:///home/hatch/workspace/torch-receive-2026-10-17/tree/wasm/full/gl.html";
}

class HostHandler : public CefClient,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefDisplayHandler {
 public:
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = browser;
    std::fprintf(stderr, "LITHOCEF CEF: browser created\n");
  }

  bool DoClose(CefRefPtr<CefBrowser> browser) override { return false; }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CEF_REQUIRE_UI_THREAD();
    browser_ = nullptr;
    CefQuitMessageLoop();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) return;
    std::fprintf(stderr, "LITHOCEF CEF: payload load ended (http %d) url=%s\n",
                 httpStatusCode, frame->GetURL().ToString().c_str());
    // Hold the shell open briefly so the payload's first frames and
    // console lines land, then close in order (close browser -> window
    // -> message loop), proving a clean full lifecycle under automation.
    ScheduleClose(4000);
  }

  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
                        const CefString& message, const CefString& source,
                        int line) override {
    std::fprintf(stderr, "LITHOCEF CEF: [page console] %s (%s:%d)\n",
                 message.ToString().c_str(), source.ToString().c_str(), line);
    return false; // do not suppress the default log
  }

  void CloseSoon() {
    if (browser_) browser_->GetHost()->CloseBrowser(true);
  }

 private:
  void ScheduleClose(int64_t delay_ms); // defined below, after CloseTask
  CefRefPtr<CefBrowser> browser_;
  IMPLEMENT_REFCOUNTING(HostHandler);
};

// Delayed-close task. (base::BindOnce to a member of a ref-counted
// handler is rejected by CEF 152's bind checks; an explicit CefTask is
// the supported shape.)
class CloseTask : public CefTask {
 public:
  explicit CloseTask(CefRefPtr<HostHandler> handler) : handler_(handler) {}
  void Execute() override { handler_->CloseSoon(); }

 private:
  CefRefPtr<HostHandler> handler_;
  IMPLEMENT_REFCOUNTING(CloseTask);
};

void HostHandler::ScheduleClose(int64_t delay_ms) {
  CefPostDelayedTask(TID_UI, new CloseTask(this), delay_ms);
}

class HostWindowDelegate : public CefWindowDelegate {
 public:
  explicit HostWindowDelegate(CefRefPtr<CefBrowserView> view) : view_(view) {}
  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(view_);
    window->Show();
    view_->RequestFocus();
  }
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override { view_ = nullptr; }
  bool CanClose(CefRefPtr<CefWindow> window) override {
    if (auto b = view_->GetBrowser()) {
      return b->GetHost()->TryCloseBrowser();
    }
    return true;
  }
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override { return CefSize(1280, 800); }

 private:
  CefRefPtr<CefBrowserView> view_;
  IMPLEMENT_REFCOUNTING(HostWindowDelegate);
};

class HostApp : public CefApp, public CefBrowserProcessHandler {
 public:
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }

  void OnContextInitialized() override {
    CEF_REQUIRE_UI_THREAD();
    CefBrowserSettings settings;
    auto client = CefRefPtr<HostHandler>(new HostHandler());
    auto view = CefBrowserView::CreateBrowserView(client, PayloadUrl(), settings,
                                                  nullptr, nullptr, nullptr);
    if (!view) {
      std::fprintf(stderr, "LITHOCEF CEF: CreateBrowserView FAILED\n");
      CefQuitMessageLoop();
      return;
    }
    CefWindow::CreateTopLevelWindow(new HostWindowDelegate(view));
    std::fprintf(stderr, "LITHOCEF CEF: window created, payload loading: %s\n",
                 PayloadUrl().c_str());
  }

 private:
  IMPLEMENT_REFCOUNTING(HostApp);
};

} // namespace

int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  CefRefPtr<HostApp> app(new HostApp());

  // Sub-processes (renderer, GPU, ...) re-enter here.
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) return exit_code;

  CefSettings settings;
  settings.no_sandbox = true; // VM bench host; the sandbox helper is not deployed here
  settings.log_severity = LOGSEVERITY_INFO;
  // Cache paths must be absolute (CEF rejects relative ones and falls
  // back to in-memory storage). Anchor them beside the executable's
  // working directory.
  {
    char cwd[4096];
    std::string cache;
    if (getcwd(cwd, sizeof(cwd))) {
      cache = std::string(cwd) + "/cef-cache";
    } else {
      cache = "/tmp/lithocef-cef-cache";
    }
    CefString(&settings.root_cache_path) = cache;
    CefString(&settings.cache_path) = cache;
  }

  if (!CefInitialize(main_args, settings, app, nullptr)) {
    std::fprintf(stderr, "LITHOCEF CEF: CefInitialize FAILED (%d)\n",
                 CefGetExitCode());
    return 1;
  }
  std::fprintf(stderr, "LITHOCEF CEF: initialized (CEF %s)\n", CEF_VERSION);
  CefRunMessageLoop();
  CefShutdown();
  std::fprintf(stderr, "LITHOCEF CEF: clean shutdown\n");
  return 0;
}
