#pragma once
#include <windows.h>

#include <climits>

// A topmost, click-through, per-pixel-alpha window that draws the arrow with its tip on the
// cursor's hotspot. It never takes focus, never appears in Alt+Tab, and mouse input passes
// straight through it to whatever is underneath.
class Overlay {
 public:
  bool Create(HINSTANCE instance);
  void Destroy();

  // Draws the arrow with its tip at (x, y) physical pixels, rotated by `angle` radians
  // (clockwise), sized for `dpi`. Cheap to call every frame; skips work when nothing changed.
  void Show(int x, int y, double angle, UINT dpi);
  void Hide();
  bool visible() const { return visible_; }

  // Keeps the window above other topmost windows: the taskbar, menus, tooltips.
  void RaiseToTop();

  void SetLook(double size, bool shadow);

  // The classic arrow, drawn upright, for the tray.
  static HICON MakeIcon(int size);

 private:
  void Resize(UINT dpi);

  HWND hwnd_ = nullptr;
  HDC memDC_ = nullptr;
  HBITMAP dib_ = nullptr;
  HGDIOBJ oldBitmap_ = nullptr;
  void* bits_ = nullptr;
  int side_ = 0;
  UINT dpi_ = 0;
  double size_ = 1.0;
  bool shadow_ = true;
  bool visible_ = false;
  int lastX_ = INT_MIN, lastY_ = INT_MIN;
  double lastAngle_ = 1e9;
};
