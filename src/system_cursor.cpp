#include "system_cursor.h"

#include <vector>

#ifndef OCR_NORMAL
#define OCR_NORMAL 32512
#endif

namespace system_cursor {

HCURSOR ArrowHandle() {
  static const HCURSOR arrow = LoadCursorW(nullptr, IDC_ARROW);
  return arrow;
}

bool HideArrow() {
  ArrowHandle();  // cache the shared handle before the swap
  const int w = GetSystemMetrics(SM_CXCURSOR), h = GetSystemMetrics(SM_CYCURSOR);
  const size_t bytes = static_cast<size_t>((w + 15) / 16 * 2) * h;
  std::vector<BYTE> andMask(bytes, 0xFF);  // AND 1, XOR 0: leave the screen untouched
  std::vector<BYTE> xorMask(bytes, 0x00);
  HCURSOR blank = CreateCursor(GetModuleHandleW(nullptr), 0, 0, w, h, andMask.data(), xorMask.data());
  if (!blank) return false;
  // SetSystemCursor takes ownership of `blank` and destroys it.
  if (!SetSystemCursor(blank, OCR_NORMAL)) {
    DestroyCursor(blank);
    return false;
  }
  return true;
}

void Restore() { SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, 0); }

bool ArrowIsShowing() {
  CURSORINFO ci = {sizeof(ci)};
  if (!GetCursorInfo(&ci)) return false;
  return (ci.flags & CURSOR_SHOWING) && ci.hCursor == ArrowHandle();
}

}  // namespace system_cursor
