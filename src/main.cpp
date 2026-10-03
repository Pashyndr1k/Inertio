/*
  Inertia Cursor: replaces the Windows arrow with one that leans into the motion and springs
  back to the classic pose when the mouse stops.

  How it works:
    1. The normal arrow is swapped for an invisible cursor (SetSystemCursor). Nothing else is.
    2. Every display frame we read the cursor. If the arrow is what's showing, an overlay
       window draws our own arrow at the hotspot, rotated by the spring model in spring.h.
       Any other cursor (text beam, hand, resize...) hides the overlay and shows as normal.
    3. The user's cursors come back on Quit, on sign-out or shutdown, on a crash, and even if
       the process is killed: a small watchdog copy of this .exe waits for it to exit and
       restores them. Each launch also restores first, in case anything was left behind.
*/

#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <timeapi.h>

#include <cstdlib>
#include <cwchar>
#include <exception>
#include <string>

#include "overlay.h"
#include "presets.h"
#include "settings.h"
#include "spring.h"
#include "system_cursor.h"

namespace {

const wchar_t kAppName[] = L"Inertia Cursor";
const wchar_t kInstanceMutex[] = L"Local\\InertiaCursor.Instance";
const wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t kRunValue[] = L"InertiaCursor";

constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT kTrayId = 1;
constexpr UINT_PTR kMenuTimer = 1;
enum MenuId : UINT { kMenuEnabled = 100, kMenuAutostart, kMenuQuit, kMenuResetMotion };
// Preset j of motion setting i is kMenuPresetBase + i * kMenuPresetStride + j.
constexpr UINT kMenuPresetBase = 200;
constexpr UINT kMenuPresetStride = 20;

HINSTANCE g_instance;
HWND g_window;  // hidden; owns the tray icon and gets shutdown messages
UINT g_taskbarCreated;
HICON g_icon;
Overlay g_overlay;
ArrowSpring g_spring;
SpringParams g_motion;  // what g_spring runs with; the tray presets change it
bool g_enabled = true;
bool g_running = true;

std::wstring ExePath() {
  wchar_t buf[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return std::wstring(buf, n);
}

// ---- Putting the cursor back, whatever happens ----

LONG WINAPI OnCrash(EXCEPTION_POINTERS*) {
  system_cursor::Restore();
  return EXCEPTION_CONTINUE_SEARCH;
}

void OnTerminate() {
  system_cursor::Restore();
  std::abort();
}

// `InertiaCursor.exe --watchdog <pid>`: waits for that process to end, then restores the
// cursors, unless a new instance has already started and hidden the arrow again.
int RunWatchdog(DWORD pid) {
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
  if (!process) return 1;
  WaitForSingleObject(process, INFINITE);
  CloseHandle(process);
  HANDLE other = OpenMutexW(SYNCHRONIZE, FALSE, kInstanceMutex);
  if (other) {
    CloseHandle(other);
    return 0;
  }
  system_cursor::Restore();
  return 0;
}

void StartWatchdog() {
  const std::wstring exe = ExePath();
  std::wstring cmd = L"\"" + exe + L"\" --watchdog " + std::to_wstring(GetCurrentProcessId());
  STARTUPINFOW si = {sizeof(si)};
  PROCESS_INFORMATION pi = {};
  if (CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE,
                     CREATE_NO_WINDOW | CREATE_BREAKAWAY_FROM_JOB, nullptr, nullptr, &si, &pi) ||
      CreateProcessW(exe.c_str(), &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                     nullptr, &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
}

// ---- Enabling and disabling ----

void SetEnabled(bool on) {
  g_enabled = on;
  if (on) {
    system_cursor::HideArrow();
  } else {
    g_overlay.Hide();
    system_cursor::Restore();
  }
}

// ---- Start with Windows ----

bool AutostartIsOn() {
  wchar_t buf[MAX_PATH * 2];
  DWORD size = sizeof(buf);
  if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, buf, &size) !=
      ERROR_SUCCESS) {
    return false;
  }
  return lstrcmpiW(buf, (L"\"" + ExePath() + L"\"").c_str()) == 0;
}

void SetAutostart(bool on) {
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
  if (on) {
    const std::wstring value = L"\"" + ExePath() + L"\"";
    RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
  } else {
    RegDeleteValueW(key, kRunValue);
  }
  RegCloseKey(key);
}

// ---- Motion presets ----

// Applies a motion setting at once and saves it to the ini, so it survives a restart.
// Returns false if it couldn't be saved.
bool SetMotion(size_t setting, double value) {
  presets::Set(g_motion, setting, value);
  g_spring.setParams(g_motion);
  return SaveMotionValue(presets::kSettings[setting].iniKey, value);
}

void WarnNotSaved() {
  MessageBoxW(nullptr,
              L"Couldn't save to inertia-cursor.ini, so this change lasts until you quit.\n"
              L"Move InertiaCursor.exe to a folder you can write to, such as "
              L"%LOCALAPPDATA%\\InertiaCursor.",
              kAppName, MB_OK | MB_ICONWARNING);
}

// Puts the four tray settings back to the default presets (the same values the ini ships with).
void ResetMotion() {
  bool saved = true;
  for (size_t i = 0; i < presets::kCount; ++i) {
    const MotionSetting& s = presets::kSettings[i];
    saved &= SetMotion(i, s.presets[s.defaultIndex].value);
  }
  if (!saved) WarnNotSaved();
}

HMENU MotionSubmenu(size_t setting) {
  const MotionSetting& s = presets::kSettings[setting];
  const int current = presets::Match(g_motion, setting);
  HMENU sub = CreatePopupMenu();
  for (size_t j = 0; j < s.count; ++j) {
    const UINT id = kMenuPresetBase + static_cast<UINT>(setting) * kMenuPresetStride +
                    static_cast<UINT>(j);
    AppendMenuW(sub, MF_STRING, id, s.presets[j].label);
    if (current == static_cast<int>(j)) {
      CheckMenuRadioItem(sub, id, id, id, MF_BYCOMMAND);
    }
  }
  if (current < 0) {
    // A value typed into the ini by hand: show it, checked, but it can't be picked again.
    wchar_t label[64];
    std::swprintf(label, 64, L"Custom (%g)", presets::Get(g_motion, setting));
    AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(sub, MF_STRING | MF_GRAYED | MF_CHECKED, 0, label);
  }
  return sub;
}

// ---- Tray ----

void AddTrayIcon() {
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = g_window;
  nid.uID = kTrayId;
  nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  nid.uCallbackMessage = WM_TRAY;
  nid.hIcon = g_icon;
  lstrcpynW(nid.szTip, kAppName, ARRAYSIZE(nid.szTip));
  Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon() {
  NOTIFYICONDATAW nid = {sizeof(nid)};
  nid.hWnd = g_window;
  nid.uID = kTrayId;
  Shell_NotifyIconW(NIM_DELETE, &nid);
}

void ShowTrayMenu() {
  HMENU menu = CreatePopupMenu();
  AppendMenuW(menu, MF_STRING | (g_enabled ? MF_CHECKED : 0), kMenuEnabled, L"&Animated arrow");
  AppendMenuW(menu, MF_STRING | (AutostartIsOn() ? MF_CHECKED : 0), kMenuAutostart,
              L"&Start with Windows");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  for (size_t i = 0; i < presets::kCount; ++i) {
    // Destroying the menu also destroys its submenus.
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(MotionSubmenu(i)),
                presets::kSettings[i].menuLabel);
  }
  AppendMenuW(menu, MF_STRING, kMenuResetMotion, L"Reset &motion to defaults");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kMenuQuit, L"&Quit");
  POINT pt;
  GetCursorPos(&pt);
  SetForegroundWindow(g_window);  // so the menu closes when you click elsewhere
  const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y,
                                  0, g_window, nullptr);
  DestroyMenu(menu);
  PostMessageW(g_window, WM_NULL, 0, 0);
  switch (cmd) {
    case kMenuEnabled: SetEnabled(!g_enabled); break;
    case kMenuAutostart: SetAutostart(!AutostartIsOn()); break;
    case kMenuQuit: DestroyWindow(g_window); break;
    case kMenuResetMotion: ResetMotion(); break;
    default:
      if (cmd >= kMenuPresetBase) {
        const size_t setting = (cmd - kMenuPresetBase) / kMenuPresetStride;
        const size_t j = (cmd - kMenuPresetBase) % kMenuPresetStride;
        if (setting < presets::kCount && j < presets::kSettings[setting].count) {
          if (!SetMotion(setting, presets::kSettings[setting].presets[j].value)) WarnNotSaved();
        }
      }
      break;
  }
}

bool Frame();

LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == g_taskbarCreated && msg != 0) {
    AddTrayIcon();  // Explorer restarted
    return 0;
  }
  switch (msg) {
    case WM_TRAY:
      if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) ShowTrayMenu();
      if (lp == WM_LBUTTONDBLCLK) SetEnabled(!g_enabled);
      return 0;
    case WM_ENTERMENULOOP:
      // The tray menu runs its own message loop; keep the arrow moving while it is open.
      SetTimer(hwnd, kMenuTimer, 8, nullptr);
      return 0;
    case WM_EXITMENULOOP:
      KillTimer(hwnd, kMenuTimer);
      return 0;
    case WM_TIMER:
      if (wp == kMenuTimer) Frame();
      return 0;
    case WM_SETTINGCHANGE:
      // Someone changed the pointer scheme in Settings: take the arrow again on top of it.
      if (wp == SPI_SETCURSORS && g_enabled) system_cursor::HideArrow();
      return 0;
    case WM_QUERYENDSESSION:
      return TRUE;
    case WM_ENDSESSION:
      if (wp) system_cursor::Restore();
      return 0;
    case WM_DESTROY:
      RemoveTrayIcon();
      g_running = false;
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- The frame loop ----

UINT DpiAt(POINT pt) {
  static HMONITOR lastMonitor;
  static UINT lastDpi = 96;
  HMONITOR m = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
  if (m != lastMonitor) {
    UINT dx = 96, dy = 96;
    if (FAILED(GetDpiForMonitor(m, MDT_EFFECTIVE_DPI, &dx, &dy))) dx = 96;
    lastMonitor = m;
    lastDpi = dx;
  }
  return lastDpi;
}

// Returns true while something is moving, so the loop knows to keep the frame rate up.
bool Tick(double dt) {
  if (!g_enabled || !system_cursor::ArrowIsShowing()) {
    g_overlay.Hide();
    return false;
  }
  POINT pt;
  if (!GetCursorPos(&pt)) {
    g_overlay.Hide();
    return false;
  }
  const UINT dpi = DpiAt(pt);
  const double scale = 96.0 / dpi;
  if (!g_overlay.visible()) g_spring.reset(pt.x * scale, pt.y * scale);
  const double angle = g_spring.update(dt, pt.x * scale, pt.y * scale);
  g_overlay.Show(pt.x, pt.y, angle, dpi);
  return !g_spring.settled();
}

double Now() {
  static LARGE_INTEGER freq = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f;
  }();
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return static_cast<double>(t.QuadPart) / freq.QuadPart;
}

// One frame: reads the cursor, steps the spring, draws. True while anything is moving.
double g_lastFrame;

bool Frame() {
  const double now = Now();
  const bool animating = Tick(now - g_lastFrame);
  g_lastFrame = now;
  g_overlay.RaiseToTop();
  return animating;
}

void RunLoop() {
  g_lastFrame = Now();
  while (g_running) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        g_running = false;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    if (!g_running) break;

    if (Frame() || g_overlay.visible()) {
      // Pace to the display: DwmFlush returns after the next composition. If it comes back
      // instantly (nothing to compose, or no DWM), don't spin; wait a few ms instead.
      const double before = Now();
      if (FAILED(DwmFlush()) || Now() - before < 0.002) {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 4, QS_ALLINPUT);
      }
    } else {
      // The arrow isn't on screen: check back a few times a second, or sooner on a message.
      MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
    }
  }
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argv && argc >= 3 && lstrcmpW(argv[1], L"--watchdog") == 0) {
    return RunWatchdog(static_cast<DWORD>(_wtoi(argv[2])));
  }
  const bool restoreOnly = argv && argc >= 2 && lstrcmpW(argv[1], L"--restore") == 0;
  LocalFree(argv);

  // Always start by putting the user's cursors back, in case an earlier run left them hidden.
  system_cursor::Restore();
  if (restoreOnly) return 0;

  HANDLE mutex = CreateMutexW(nullptr, FALSE, kInstanceMutex);
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    MessageBoxW(nullptr, L"Inertia Cursor is already running. Look for its arrow in the tray.",
                kAppName, MB_OK | MB_ICONINFORMATION);
    return 0;
  }

  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  SetUnhandledExceptionFilter(OnCrash);
  std::set_terminate(OnTerminate);
  StartWatchdog();

  Gdiplus::GdiplusStartupInput gdiInput;
  ULONG_PTR gdiToken;
  Gdiplus::GdiplusStartup(&gdiToken, &gdiInput, nullptr);

  g_instance = instance;
  const Settings settings = LoadSettings();
  g_motion = settings.motion;
  g_spring.setParams(g_motion);

  WNDCLASSEXW wc = {sizeof(wc)};
  wc.lpfnWndProc = MainProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"InertiaCursorMain";
  RegisterClassExW(&wc);
  g_window = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, kAppName, WS_POPUP, 0, 0, 0, 0,
                             nullptr, nullptr, instance, nullptr);
  g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
  g_icon = Overlay::MakeIcon(GetSystemMetrics(SM_CXSMICON));

  if (!g_window || !g_overlay.Create(instance)) {
    MessageBoxW(nullptr, L"Couldn't create the cursor window.", kAppName, MB_OK | MB_ICONERROR);
    Gdiplus::GdiplusShutdown(gdiToken);
    return 1;
  }
  g_overlay.SetLook(settings.size, settings.shadow);
  AddTrayIcon();
  SetEnabled(true);

  timeBeginPeriod(1);
  RunLoop();
  timeEndPeriod(1);

  system_cursor::Restore();
  g_overlay.Destroy();
  if (g_icon) DestroyIcon(g_icon);
  Gdiplus::GdiplusShutdown(gdiToken);
  CloseHandle(mutex);
  return 0;
}
