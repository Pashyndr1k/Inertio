#include "settings.h"

#include <windows.h>

#include <cwchar>
#include <string>

namespace {

std::wstring IniPath() {
  wchar_t exe[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::wstring path(exe, n);
  const size_t slash = path.find_last_of(L"\\/");
  return path.substr(0, slash + 1) + L"inertia-cursor.ini";
}

double ReadDouble(const std::wstring& ini, const wchar_t* section, const wchar_t* key, double fallback,
                  double lo, double hi) {
  wchar_t buf[64];
  GetPrivateProfileStringW(section, key, L"", buf, 64, ini.c_str());
  if (!buf[0]) return fallback;
  wchar_t* end = nullptr;
  const double v = std::wcstod(buf, &end);
  if (end == buf) return fallback;
  return v < lo ? lo : v > hi ? hi : v;
}

}  // namespace

Settings LoadSettings() {
  Settings s;
  const std::wstring ini = IniPath();
  SpringParams& m = s.motion;
  m.frequency = ReadDouble(ini, L"Motion", L"Frequency", m.frequency, 0.5, 20);
  m.damping = ReadDouble(ini, L"Motion", L"Damping", m.damping, 0.05, 3);
  m.drag = ReadDouble(ini, L"Motion", L"Drag", m.drag, 0, 50000);
  m.kick = ReadDouble(ini, L"Motion", L"Kick", m.kick, 0, 5000);
  m.maxAngle = ReadDouble(ini, L"Motion", L"MaxAngle", m.maxAngle * 180 / ArrowSpring::kPi, 0, 180) *
               ArrowSpring::kPi / 180;
  s.size = ReadDouble(ini, L"Look", L"Size", s.size, 0.5, 4);
  s.shadow = ReadDouble(ini, L"Look", L"Shadow", 1, 0, 1) >= 0.5;
  return s;
}

bool SaveMotionValue(const wchar_t* key, double value) {
  wchar_t buf[32];
  std::swprintf(buf, 32, L"%g", value);
  return WritePrivateProfileStringW(L"Motion", key, buf, IniPath().c_str()) != 0;
}
