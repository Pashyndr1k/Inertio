#include "overlay.h"

#include <gdiplus.h>

#include <cmath>

namespace {

const wchar_t kClassName[] = L"InertiaCursorOverlay";

// The classic Windows arrow, 12 x 19 units, tip at the origin.
const Gdiplus::PointF kArrow[] = {
    {0.0f, 0.0f},   {0.0f, 16.6f}, {4.0f, 12.9f}, {6.8f, 19.0f},
    {9.3f, 17.9f},  {6.6f, 11.9f}, {11.6f, 11.9f},
};
constexpr float kArrowHeight = 19.0f;

// The user's cursor size from Settings > Accessibility > Mouse pointer, in 96-DPI pixels.
int CursorBaseSize() {
  DWORD value = 32, size = sizeof(value);
  if (RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", L"CursorBaseSize", RRF_RT_REG_DWORD,
                   nullptr, &value, &size) != ERROR_SUCCESS ||
      value < 16 || value > 512) {
    value = 32;
  }
  return static_cast<int>(value);
}

void DrawArrow(Gdiplus::Graphics& g, float tipX, float tipY, float height, float degrees, bool shadow) {
  const float unit = height / kArrowHeight;
  Gdiplus::GraphicsPath path;
  path.AddPolygon(kArrow, sizeof(kArrow) / sizeof(kArrow[0]));

  if (shadow) {
    // The shadow keeps falling down and to the right, whichever way the arrow turns.
    Gdiplus::SolidBrush shade(Gdiplus::Color(46, 0, 0, 0));
    for (int i = 1; i <= 2; i++) {
      g.ResetTransform();
      g.TranslateTransform(tipX + unit * 0.7f * i, tipY + unit * 0.9f * i);
      g.RotateTransform(degrees);
      g.ScaleTransform(unit, unit);
      g.FillPath(&shade, &path);
    }
  }

  g.ResetTransform();
  g.TranslateTransform(tipX, tipY);
  g.RotateTransform(degrees);
  g.ScaleTransform(unit, unit);
  Gdiplus::SolidBrush fill(Gdiplus::Color(255, 255, 255, 255));
  g.FillPath(&fill, &path);
  Gdiplus::Pen outline(Gdiplus::Color(255, 0, 0, 0), 1.0f);  // one unit, scaled with the arrow
  outline.SetLineJoin(Gdiplus::LineJoinMiter);
  g.DrawPath(&outline, &path);
  g.ResetTransform();
}

void Prepare(Gdiplus::Graphics& g) {
  g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
  g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
  g.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_NCHITTEST) return HTTRANSPARENT;
  if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool Overlay::Create(HINSTANCE instance) {
  WNDCLASSEXW wc = {sizeof(wc)};
  wc.lpfnWndProc = OverlayProc;
  wc.hInstance = instance;
  wc.lpszClassName = kClassName;
  RegisterClassExW(&wc);
  hwnd_ = CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      kClassName, L"Inertia Cursor", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
  if (!hwnd_) return false;
  memDC_ = CreateCompatibleDC(nullptr);
  return memDC_ != nullptr;
}

void Overlay::Destroy() {
  if (dib_) {
    SelectObject(memDC_, oldBitmap_);
    DeleteObject(dib_);
    dib_ = nullptr;
  }
  if (memDC_) DeleteDC(memDC_), memDC_ = nullptr;
  if (hwnd_) DestroyWindow(hwnd_), hwnd_ = nullptr;
  visible_ = false;
}

void Overlay::SetLook(double size, bool shadow) {
  size_ = size;
  shadow_ = shadow;
  dpi_ = 0;  // rebuild at the next Show
}

void Overlay::Resize(UINT dpi) {
  dpi_ = dpi;
  const double height = CursorBaseSize() * (kArrowHeight / 32.0) * dpi / 96.0 * size_;
  // A square centred on the tip, big enough for the arrow at any angle plus its shadow.
  side_ = static_cast<int>(std::ceil(height * 1.25)) * 2 + 8;
  if (dib_) {
    SelectObject(memDC_, oldBitmap_);
    DeleteObject(dib_);
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = side_;
  bi.bmiHeader.biHeight = -side_;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  dib_ = CreateDIBSection(memDC_, &bi, DIB_RGB_COLORS, &bits_, nullptr, 0);
  oldBitmap_ = SelectObject(memDC_, dib_);
  lastAngle_ = 1e9;  // force a redraw
}

void Overlay::Show(int x, int y, double angle, UINT dpi) {
  if (!hwnd_) return;
  if (dpi != dpi_) Resize(dpi);
  if (!dib_) return;

  const bool redraw = std::fabs(angle - lastAngle_) > 1e-5;
  const bool moved = x != lastX_ || y != lastY_;
  if (visible_ && !redraw && !moved) return;

  POINT dst = {x - side_ / 2, y - side_ / 2};
  SIZE size = {side_, side_};
  POINT src = {0, 0};
  BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};

  if (redraw || !visible_) {
    {
      Gdiplus::Bitmap canvas(side_, side_, side_ * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits_));
      Gdiplus::Graphics g(&canvas);
      Prepare(g);
      g.Clear(Gdiplus::Color(0, 0, 0, 0));
      const float height =
          static_cast<float>(CursorBaseSize() * (kArrowHeight / 32.0) * dpi_ / 96.0 * size_);
      DrawArrow(g, side_ / 2.0f, side_ / 2.0f, height, static_cast<float>(angle * 180.0 / 3.14159265358979),
                shadow_);
    }
    UpdateLayeredWindow(hwnd_, nullptr, &dst, &size, memDC_, &src, 0, &blend, ULW_ALPHA);
    lastAngle_ = angle;
  } else {
    // Same picture, new place: just move it.
    UpdateLayeredWindow(hwnd_, nullptr, &dst, &size, nullptr, nullptr, 0, nullptr, 0);
  }
  lastX_ = x;
  lastY_ = y;

  if (!visible_) {
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    visible_ = true;
  }
}

void Overlay::Hide() {
  if (!hwnd_ || !visible_) return;
  ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

void Overlay::RaiseToTop() {
  // Menus and tooltips open as topmost windows above ours; only re-stack when something is.
  if (hwnd_ && visible_ && GetWindow(hwnd_, GW_HWNDPREV)) {
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
}

HICON Overlay::MakeIcon(int size) {
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biWidth = size;
  bi.bmiHeader.biHeight = -size;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP color = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!color) return nullptr;
  {
    Gdiplus::Bitmap canvas(size, size, size * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(bits));
    Gdiplus::Graphics g(&canvas);
    Prepare(g);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    // Tilted a little, so it reads as this app rather than the plain system arrow.
    DrawArrow(g, size * 0.36f, size * 0.06f, size * 0.86f, 18.0f, false);
  }
  HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
  ICONINFO ii = {TRUE, 0, 0, mask, color};
  HICON icon = CreateIconIndirect(&ii);
  DeleteObject(mask);
  DeleteObject(color);
  return icon;
}
