// Nemo View pop-up window (see NemoViewWindow.h).
#include "lithokraft/game/NemoViewWindow.h"

#include "lithokraft/game/NemoViewHub.h"
#include "lithokraft/game/mcp_senses_adapter.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace lithokraft {

cNemoViewWindow& cNemoViewWindow::instance() {
  static cNemoViewWindow w;
  return w;
}

cNemoViewWindow::~cNemoViewWindow() {
  quit_.store(true);
  if (thread_.joinable()) thread_.join();
}

void cNemoViewWindow::setWanted(bool wanted, cMcpSensesAdapter* adapter, bool maximize) {
#ifdef _WIN32
  maximize_.store(maximize);
  if (wanted == wanted_) return;
  wanted_ = wanted;
  maximizeWanted_ = maximize;
  if (wanted) {
    if (thread_.joinable()) thread_.join();  // a window the user closed earlier has already finished
    quit_.store(false);
    thread_ = std::thread([this, adapter] { run(adapter); });
  } else {
    quit_.store(true);
    if (thread_.joinable()) thread_.join();
  }
#else
  (void)wanted;
  (void)adapter;
  (void)maximize;
#endif
}

#ifdef _WIN32
namespace {

namespace nv = litholattice::nemoview;

constexpr int kFooterPx = 22;

struct WindowState {
  HBITMAP back = nullptr;
  HDC backDc = nullptr;
  int backW = 0, backH = 0;
  HFONT font = nullptr;
  int fontPx = 0;
};

void adoptThreadDpi() {
  // This thread only: never change the process, so the game window stays as it is.
  using Ctx = void*;
  using Fn = Ctx(WINAPI*)(Ctx);
  auto setThread = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext"));
  if (setThread) setThread(reinterpret_cast<Ctx>(static_cast<INT_PTR>(-4)));  // per-monitor v2
}

LRESULT CALLBACK nemoWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  auto* st = reinterpret_cast<WindowState*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
  switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      if (st && st->backDc) BitBlt(dc, 0, 0, st->backW, st->backH, st->backDc, 0, 0, SRCCOPY);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_DPICHANGED:
      if (lp) {
        const RECT* rec = reinterpret_cast<const RECT*>(lp);
        SetWindowPos(hwnd, nullptr, rec->left, rec->top, rec->right - rec->left, rec->bottom - rec->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcA(hwnd, msg, wp, lp);
  }
}

COLORREF cr(uint32_t rgba) { return RGB((rgba >> 16) & 0xFF, (rgba >> 8) & 0xFF, rgba & 0xFF); }

void ensureFont(WindowState& st, int px) {
  const int use = std::max(8, px);
  if (st.font && st.fontPx == use) return;
  if (st.font) DeleteObject(st.font);
  st.font = CreateFontA(-use, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
  st.fontPx = use;
}

void paintFrame(WindowState& st, HWND hwnd, const nv::AsciiFrame& f, const char* source, int style) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  const int w = rc.right - rc.left, h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0 || f.cols <= 0 || f.rows <= 0) return;
  if (!st.backDc || st.backW != w || st.backH != h) {
    HDC wdc = GetDC(hwnd);
    if (st.backDc) {
      DeleteDC(st.backDc);
      DeleteObject(st.back);
    }
    st.backDc = CreateCompatibleDC(wdc);
    st.back = CreateCompatibleBitmap(wdc, w, h);
    SelectObject(st.backDc, st.back);
    st.backW = w;
    st.backH = h;
    ReleaseDC(hwnd, wdc);
  }
  HDC dc = st.backDc;
  HBRUSH black = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  FillRect(dc, &rc, black);

  const int bodyH = std::max(1, h - kFooterPx);
  // Fit every column and row into the client: never clip the right edge at 125% scale.
  int cellW = std::max(1, w / f.cols);
  int cellH = std::max(1, bodyH / f.rows);
  if (cellH > cellW * 2) cellH = cellW * 2;
  if (cellH < (cellW * 3) / 2) {
    // Keep a readable monospace aspect (~1:2) when there is room; if not, keep the fit so nothing
    // is cropped.
    const int prefer = std::min(cellW * 2, bodyH / f.rows);
    if (prefer * f.rows <= bodyH) cellH = std::max(cellH, prefer);
  }
  ensureFont(st, cellH);
  SelectObject(dc, st.font);
  SetBkMode(dc, OPAQUE);

  const int gridW = f.cols * cellW;
  const int gridH = f.rows * cellH;
  const int ox = std::max(0, (w - gridW) / 2);
  const int oy = std::max(0, (bodyH - gridH) / 2);
  const nv::ViewStyle vs = nv::ViewStyle(style < 0 || style > 4 ? 4 : style);

  for (int y = 0; y < f.rows; ++y) {
    int x = 0;
    while (x < f.cols) {
      const size_t i0 = size_t(y) * size_t(f.cols) + size_t(x);
      const nv::CellLook look = nv::decodeCell(f, i0, vs);
      int x1 = x + 1;
      while (x1 < f.cols) {
        const nv::CellLook n = nv::decodeCell(f, size_t(y) * size_t(f.cols) + size_t(x1), vs);
        if (n.ink != look.ink || n.fill != look.fill) break;
        ++x1;
      }
      SetBkColor(dc, cr(look.fill));
      SetTextColor(dc, cr(look.ink));
      RECT cell{ox + x * cellW, oy + y * cellH, ox + x1 * cellW, oy + (y + 1) * cellH};
      ExtTextOutA(dc, cell.left, cell.top, ETO_OPAQUE | ETO_CLIPPED, &cell, f.glyph.data() + i0, UINT(x1 - x),
                  nullptr);
      x = x1;
    }
  }

  char footer[240];
  std::snprintf(footer, sizeof footer, "%s   tick %llu   %s view   %dx%d   sha %.12s   style %s", source,
                static_cast<unsigned long long>(f.tick), nv::viewModeName(f.mode), f.cols, f.rows,
                nv::shaHex(f).c_str(), nv::viewStyleName(vs));
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(169, 112, 255));  // JD violet
  TextOutA(dc, 6, h - kFooterPx + 3, footer, int(std::strlen(footer)));
  InvalidateRect(hwnd, nullptr, FALSE);
}

}  // namespace

void cNemoViewWindow::run(cMcpSensesAdapter* adapter) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  adoptThreadDpi();
  HINSTANCE inst = GetModuleHandleA(nullptr);
  WNDCLASSA wc{};
  wc.lpfnWndProc = nemoWndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  wc.lpszClassName = "LithoKraftNemoView";
  RegisterClassA(&wc);  // fails harmlessly when already registered

  WindowState st;
  ensureFont(st, 16);
  int cellW = 8, cellH = 16;
  {
    HDC sdc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(sdc, st.font);
    TEXTMETRICA tm;
    if (GetTextMetricsA(sdc, &tm)) {
      cellW = std::max(1, int(tm.tmAveCharWidth));
      cellH = std::max(1, int(tm.tmHeight));
    }
    SelectObject(sdc, old);
    ReleaseDC(nullptr, sdc);
  }
  const int cols = cols_.load();
  const int rows = cols == 40 ? 22 : cols == 96 ? 54 : 45;
  RECT want{0, 0, cols * cellW + 16, rows * cellH + kFooterPx + 8};
  AdjustWindowRect(&want, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd = CreateWindowExA(0, "LithoKraftNemoView", "Nemo View - what Little Nemo sees", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, want.right - want.left, want.bottom - want.top, nullptr,
                              nullptr, inst, nullptr);
  if (!hwnd) {
    if (st.font) DeleteObject(st.font);
    return;
  }
  SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);  // never steal focus from the game
  if (maximize_.load()) {
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    if (GetMonitorInfoA(mon, &mi)) {
      SetWindowPos(hwnd, nullptr, mi.rcWork.left, mi.rcWork.top, mi.rcWork.right - mi.rcWork.left,
                   mi.rcWork.bottom - mi.rcWork.top, SWP_NOACTIVATE | SWP_NOZORDER);
    }
  }
  open_.store(true);
  litholattice::cSoftwareRenderer sw;  // this thread's own renderer
  litholattice::CullProtoState cull;
  nv::AsciiFrame preview;
  cNemoViewHub& hub = cNemoViewHub::instance();
  ULONGLONG lastFrame = 0;
  bool running = true;
  while (running && !quit_.load()) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        running = false;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    if (!running) break;
    const ULONGLONG now = GetTickCount64();
    if (now - lastFrame >= 100) {  // about ten frames a second
      lastFrame = now;
      const int style = style_.load();
      const cNemoViewHub::Entry& e = hub.latest();
      if (e.seq != 0 && cNemoViewHub::nowMs() - e.steadyMs < 3000) {
        paintFrame(st, hwnd, e.frame, e.source.c_str(), style);
      } else if (adapter) {
        litholattice::mcp::AsciiRequest req;
        req.cols = cols_.load();
        req.objects = objects_.load();
        if (adapter->previewAsciiFrame(sw, cull, req, preview))
          paintFrame(st, hwnd, preview, "preview", style);
      }
    }
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
  }
  if (IsWindow(hwnd)) DestroyWindow(hwnd);
  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
  }
  if (st.backDc) {
    DeleteDC(st.backDc);
    DeleteObject(st.back);
  }
  if (st.font) DeleteObject(st.font);
  open_.store(false);
}
#else
void cNemoViewWindow::run(cMcpSensesAdapter*) {}
#endif

}  // namespace lithokraft
