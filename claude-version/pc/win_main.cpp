// TamaPoke para PC (Windows): la ventana, el raton, el dialogo de abrir ficheros y la bateria del portatil.
// El juego esta en host_win.cpp (sin windows.h); aqui solo Win32.
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#include <algorithm>
#include <string>
#include <vector>

// host_win.cpp
void pcInit();
bool pcStep();
void pcSave();
const uint32_t *pcFrame(int *w, int *h);
void pcLogicalSize(int *w, int *h);
void pcMouse(int x, int y, int act);
void pcToggleMenu();
void pcSetPakFds(const int *fds, int n);
void pcBattery(int pct, bool chg);
void pcRequestExit();
void pcRepaintLast();

static HWND gWnd = nullptr;

static void calcRect(int cw, int ch, int fw, int fh, int &x0, int &y0, int &w, int &h) {
  w = cw; h = (int)((int64_t)cw * fh / fw);
  if (h > ch) { h = ch; w = (int)((int64_t)ch * fw / fh); }
  x0 = (cw - w) / 2; y0 = (ch - h) / 2;
}

void winRepaint() {
  if (!gWnd) return;
  int fw, fh;
  const uint32_t *px = pcFrame(&fw, &fh);
  if (!px) return;
  RECT rc;
  GetClientRect(gWnd, &rc);
  int cw = rc.right, ch = rc.bottom;
  if (cw <= 0 || ch <= 0) return;
  int x0, y0, w, h;
  calcRect(cw, ch, fw, fh, x0, y0, w, h);
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = fw;
  bi.bmiHeader.biHeight = -fh;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  HDC dc = GetDC(gWnd);
  HBRUSH blk = (HBRUSH)GetStockObject(BLACK_BRUSH);
  RECT r1 = { 0, 0, cw, y0 }, r2 = { 0, y0 + h, cw, ch }, r3 = { 0, 0, x0, ch }, r4 = { x0 + w, 0, cw, ch };
  FillRect(dc, &r1, blk); FillRect(dc, &r2, blk); FillRect(dc, &r3, blk); FillRect(dc, &r4, blk);
  SetStretchBltMode(dc, HALFTONE);  // reducir con buena calidad (el lienzo es 1398 de ancho)
  SetBrushOrgEx(dc, 0, 0, nullptr);
  StretchDIBits(dc, x0, y0, w, h, 0, 0, fw, fh, px, &bi, DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(gWnd, dc);
}

static void mouse(int mx, int my, int act) {
  RECT rc;
  GetClientRect(gWnd, &rc);
  int lw, lh;
  pcLogicalSize(&lw, &lh);
  int x0, y0, w, h;
  calcRect(rc.right, rc.bottom, lw, lh, x0, y0, w, h);
  if (w <= 0 || h <= 0) return;
  pcMouse((int)((int64_t)(mx - x0) * lw / w), (int)((int64_t)(my - y0) * lh / h), act);
}

void winPickPak() {
  std::vector<wchar_t> buf(65536, 0);
  OPENFILENAMEW of = {};
  of.lStructSize = sizeof(of);
  of.hwndOwner = gWnd;
  of.lpstrFilter = L"TamaPoke pak (mons.pak, ani.pak, 조각)\0*.pak;*.pak.*\0모든 파일\0*.*\0";
  of.lpstrFile = buf.data();
  of.nMaxFile = (DWORD)buf.size();
  of.lpstrTitle = L"mons.pak / ani.pak 고르기 (조각이면 모두 선택)";
  of.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameW(&of)) return;
  std::wstring dir = buf.data();
  std::vector<std::wstring> names;
  const wchar_t *p = buf.data() + dir.size() + 1;
  while (*p) { names.push_back(p); p += names.back().size() + 1; }
  std::vector<std::wstring> paths;
  if (names.empty()) paths.push_back(dir);
  else {
    std::sort(names.begin(), names.end());  // .001, .002 ... en orden
    for (auto &n : names) paths.push_back(dir + L"\\" + n);
  }
  std::vector<int> fds;
  for (auto &w : paths) {
    int fd = _wopen(w.c_str(), _O_RDONLY | _O_BINARY);
    if (fd >= 0) fds.push_back(fd);
  }
  pcSetPakFds(fds.data(), (int)fds.size());
}

void winAskFirstPak() {
  MessageBoxW(gWnd,
              L"처음 한 번: mons.pak을 골라 주세요 (조각 mons.pak.001 ~ 이면 모두 함께).\n"
              L"3D 움직이는 그림은 ani.pak.001 ~ .002를 [메뉴] → [pak 파일 넣기]로 추가해요.",
              L"TamaPoke", MB_OK | MB_ICONINFORMATION);
  winPickPak();
}

static bool gRunning = true;
static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
  switch (m) {
    case WM_LBUTTONDOWN: SetCapture(h); mouse(GET_X_LPARAM(l), GET_Y_LPARAM(l), 0); return 0;
    case WM_MOUSEMOVE: if (w & MK_LBUTTON) mouse(GET_X_LPARAM(l), GET_Y_LPARAM(l), 1); return 0;
    case WM_LBUTTONUP: ReleaseCapture(); mouse(GET_X_LPARAM(l), GET_Y_LPARAM(l), 2); return 0;
    case WM_KEYDOWN: if (w == VK_ESCAPE) pcToggleMenu(); return 0;
    case WM_PAINT: { PAINTSTRUCT ps; BeginPaint(h, &ps); EndPaint(h, &ps); pcRepaintLast(); return 0; }
    case WM_SIZE: pcRepaintLast(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_CLOSE: pcRequestExit(); return 0;  // guardar y salir (lo hace el bucle)
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, m, w, l);
}

static bool pump() {
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT) return false;
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return true;
}
void hostPumpEvents() { pump(); }  // mientras se descomprime un .pak

static void batteryPoll() {
  SYSTEM_POWER_STATUS ps;
  if (GetSystemPowerStatus(&ps)) pcBattery(ps.BatteryLifePercent <= 100 ? ps.BatteryLifePercent : 100, ps.ACLineStatus == 1);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int show) {
  SetProcessDPIAware();
  timeBeginPeriod(1);
  // los datos junto al .exe: TamaPoke_data\ (el juego usa rutas relativas)
  wchar_t exe[MAX_PATH];
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring dir = exe;
  dir = dir.substr(0, dir.find_last_of(L"\\/"));
  std::wstring data = dir + L"\\TamaPoke_data";
  CreateDirectoryW(data.c_str(), nullptr);
  SetCurrentDirectoryW(data.c_str());
  pcInit();

  WNDCLASSW wc = {};
  wc.lpfnWndProc = wndProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
  wc.lpszClassName = L"TamaPokePC";
  RegisterClassW(&wc);
  int lw, lh;
  pcLogicalSize(&lw, &lh);
  int ch = GetSystemMetrics(SM_CYSCREEN) * 85 / 100, cw = ch * lw / lh;
  RECT r = { 0, 0, cw, ch };
  AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  gWnd = CreateWindowW(L"TamaPokePC", L"TamaPoke", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                       r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
  ShowWindow(gWnd, show);
  batteryPoll();
  DWORD lastBat = GetTickCount();
  while (pump()) {
    DWORD t0 = GetTickCount();
    if (!pcStep()) break;
    if (GetTickCount() - lastBat > 30000) { lastBat = GetTickCount(); batteryPoll(); }
    DWORD spent = GetTickCount() - t0;
    if (spent < 16) Sleep(16 - spent);  // ~60 fps como mucho
  }
  pcSave();
  if (gWnd) DestroyWindow(gWnd);
  return 0;
}
