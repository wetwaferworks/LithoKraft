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

void cNemoViewWindow::setWanted(bool wanted, cMcpSensesAdapter* adapter) {
#ifdef _WIN32
  if (wanted == wanted_) return;
  wanted_ = wanted;
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
};

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

COLORREF cellColour(const nv::AsciiFrame& f, size_t i, int style) {
  const char g = f.glyph[i];
  switch (style) {
    case 1: {  // true palette
      const uint32_t c = nv::paletteColour(f.fg[i]);
      return RGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    }
    case 2: {  // depth heat: near red .. far blue
      static const COLORREF kHeat[8] = {RGB(255, 40, 40), RGB(255, 120, 30), RGB(255, 200, 40), RGB(160, 230, 60),
                                        RGB(60, 200, 160), RGB(60, 140, 230), RGB(80, 80, 200), RGB(0, 0, 0)};
      return kHeat[f.band[i] & 7];
    }
    case 3: {  // object colours
      switch (nv::ObjectId(f.object[i])) {
        case nv::ObjectId::Wall: return RGB(170, 170, 180);
        case nv::ObjectId::Door: return RGB(80, 220, 255);
        case nv::ObjectId::Key: return RGB(255, 220, 60);
        case nv::ObjectId::Light: return RGB(255, 250, 200);
        case nv::ObjectId::Exit: return RGB(120, 255, 120);
        case nv::ObjectId::Pit: return RGB(220, 60, 220);
        case nv::ObjectId::Floor: return RGB(70, 140, 70);
        case nv::ObjectId::Translucent: return RGB(150, 220, 255);
        default: return RGB(120, 120, 120);
      }
    }
    default: {  // matrix green: brighter for denser glyphs, object characters stand out
      switch (g) {
        case '#': return RGB(0, 190, 70);
        case '|': return RGB(120, 255, 200);
        case 'k': case 'E': case '*': return RGB(210, 255, 210);
        case 'o': return RGB(0, 110, 50);
        default: break;
      }
      const char* p = std::strchr(nv::kRamp, g);
      const int idx = p ? int(p - nv::kRamp) : 0;
      return RGB(0, 60 + idx * 17, 20 + idx * 4);
    }
  }
}

void paintFrame(WindowState& st, HWND hwnd, HFONT font, int cellW, int cellH, const nv::AsciiFrame& f,
                const char* source, int style) {
  RECT rc;
  GetClientRect(hwnd, &rc);
  const int w = rc.right - rc.left, h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) return;
  if (!st.backDc || st.backW != w || st.backH != h) {
    HDC wdc = GetDC(hwnd);
    if (st.backDc) { DeleteDC(st.backDc); DeleteObject(st.back); }
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
  SelectObject(dc, font);
  SetBkMode(dc, TRANSPARENT);
  const int ox = std::max(0, (w - f.cols * cellW) / 2);
  for (int y = 0; y < f.rows; ++y) {
    int x = 0;
    while (x < f.cols) {  // one ExtTextOut per run of one colour
      const size_t i0 = size_t(y) * size_t(f.cols) + size_t(x);
      const COLORREF c = cellColour(f, i0, style);
      int x1 = x + 1;
      while (x1 < f.cols && cellColour(f, size_t(y) * size_t(f.cols) + size_t(x1), style) == c) ++x1;
      SetTextColor(dc, c);
      ExtTextOutA(dc, ox + x * cellW, y * cellH, 0, nullptr, f.glyph.data() + i0, UINT(x1 - x), nullptr);
      x = x1;
    }
  }
  char footer[200];
  static const char* kStyles[4] = {"matrix", "true colour", "depth", "objects"};
  std::snprintf(footer, sizeof footer, "%s   tick %llu   %s view   %dx%d   sha %.12s   style %s", source,
                static_cast<unsigned long long>(f.tick), nv::viewModeName(f.mode), f.cols, f.rows,
                nv::shaHex(f).c_str(), kStyles[style & 3]);
  SetTextColor(dc, RGB(169, 112, 255));  // JD violet
  TextOutA(dc, 6, h - kFooterPx + 3, footer, int(std::strlen(footer)));
  InvalidateRect(hwnd, nullptr, FALSE);
}

}  // namespace

void cNemoViewWindow::run(cMcpSensesAdapter* adapter) {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
  HINSTANCE inst = GetModuleHandleA(nullptr);
  WNDCLASSA wc{};
  wc.lpfnWndProc = nemoWndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.lpszClassName = "LithoKraftNemoView";
  RegisterClassA(&wc);  // fails harmlessly when already registered
  HFONT font = CreateFontA(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
  int cellW = 7, cellH = 14;
  {
    HDC sdc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(sdc, font);
    TEXTMETRICA tm;
    if (GetTextMetricsA(sdc, &tm)) {
      cellW = tm.tmAveCharWidth;
      cellH = tm.tmHeight;
    }
    SelectObject(sdc, old);
    ReleaseDC(nullptr, sdc);
  }
  RECT want{0, 0, 96 * cellW + 12, 54 * cellH + kFooterPx};
  AdjustWindowRect(&want, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd = CreateWindowExA(0, "LithoKraftNemoView", "Nemo View - what Little Nemo sees", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, want.right - want.left, want.bottom - want.top, nullptr,
                              nullptr, inst, nullptr);
  if (!hwnd) {
    DeleteObject(font);
    return;
  }
  WindowState st;
  SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
  ShowWindow(hwnd, SW_SHOWNOACTIVATE);  // never steal focus from the game
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
      if (msg.message == WM_QUIT) { running = false; break; }
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
        paintFrame(st, hwnd, font, cellW, cellH, e.frame, e.source.c_str(), style);
      } else if (adapter) {
        litholattice::mcp::AsciiRequest req;
        req.cols = cols_.load();
        req.objects = objects_.load();
        if (adapter->previewAsciiFrame(sw, cull, req, preview)) paintFrame(st, hwnd, font, cellW, cellH, preview, "preview", style);
      }
    }
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
  }
  if (IsWindow(hwnd)) DestroyWindow(hwnd);
  MSG msg;
  while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {}
  if (st.backDc) { DeleteDC(st.backDc); DeleteObject(st.back); }
  DeleteObject(font);
  open_.store(false);
}
#else
void cNemoViewWindow::run(cMcpSensesAdapter*) {}
#endif

}  // namespace lithokraft
