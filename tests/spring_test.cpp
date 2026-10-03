// Checks the arrow's motion model without Windows: g++ -std=c++17 -I../src spring_test.cpp
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "spring.h"

static int failures = 0;

static void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) failures++;
}

constexpr double kFrame = 1.0 / 60.0;

// Moves the cursor at (vx, vy) px/s for `seconds`; returns the largest |angle| seen.
static double move(ArrowSpring& s, double& x, double& y, double vx, double vy, double seconds) {
  double peak = 0;
  for (double t = 0; t < seconds; t += kFrame) {
    x += vx * kFrame;
    y += vy * kFrame;
    peak = std::fmax(peak, std::fabs(s.update(kFrame, x, y)));
  }
  return peak;
}

int main() {
  const double maxAngle = SpringParams().maxAngle;

  {
    ArrowSpring s;
    double x = 500, y = 500;
    move(s, x, y, 0, 0, 2);
    check(s.angle() == 0 && s.settled(), "a still cursor stays upright");
  }
  {
    ArrowSpring s;
    double x = 500, y = 500;
    move(s, x, y, 1500, 0, 0.6);
    check(s.angle() > 0.3, "moving right turns the arrow clockwise, toward the motion");
  }
  {
    ArrowSpring s;
    double x = 500, y = 500;
    move(s, x, y, -1500, 0, 0.6);
    check(s.angle() < -0.3, "moving left turns the arrow counter-clockwise, toward the motion");
  }
  {
    ArrowSpring s;
    double x = 500, y = 500;
    move(s, x, y, 1500, 0, 0.6);
    const double moving = s.angle();
    // Stop and watch it swing back past upright, then settle.
    double minAfter = moving;
    for (double t = 0; t < 2.0; t += kFrame) minAfter = std::fmin(minAfter, s.update(kFrame, x, y));
    check(minAfter < -0.01, "after stopping it overshoots past upright, like a spring");
    check(std::fabs(s.angle()) < 0.01, "and settles back to the classic pose within two seconds");
    for (double t = 0; t < 2.0; t += kFrame) s.update(kFrame, x, y);
    check(s.settled() && s.angle() == 0, "then snaps exactly upright so redrawing can stop");
  }
  {
    // Moving the way the arrow already points (up and a little left) should barely turn it.
    ArrowSpring s;
    double x = 500, y = 500;
    const double a = 23 * ArrowSpring::kPi / 180;
    const double peak = move(s, x, y, -1500 * std::sin(a), -1500 * std::cos(a), 0.6);
    check(peak < 0.1, "moving along the arrow's own direction keeps it nearly upright");
  }
  {
    // Wild shaking must never exceed the limit or blow up.
    ArrowSpring s;
    double x = 500, y = 500, peak = 0;
    std::srand(7);
    for (int i = 0; i < 6000; i++) {
      x += (std::rand() % 2001 - 1000) * 0.2;
      y += (std::rand() % 2001 - 1000) * 0.2;
      const double a = s.update(kFrame, x, y);
      peak = std::fmax(peak, std::fabs(a));
      if (!std::isfinite(a)) peak = 1e9;
    }
    check(peak <= maxAngle + 1e-9, "violent shaking stays within the angle limit and finite");
  }
  {
    // Frame rate should not change where it ends up.
    ArrowSpring a, b;
    double ax = 0, ay = 0, bx = 0, by = 0;
    for (double t = 0; t < 0.5; t += 1.0 / 60) {
      ax += 1200.0 / 60;
      a.update(1.0 / 60, ax, ay);
    }
    for (double t = 0; t < 0.5; t += 1.0 / 144) {
      bx += 1200.0 / 144;
      b.update(1.0 / 144, bx, by);
    }
    check(std::fabs(a.angle() - b.angle()) < 0.08, "60 Hz and 144 Hz give nearly the same tilt");
  }
  {
    // A long hitch should not register as a huge jerk.
    ArrowSpring s;
    s.update(kFrame, 0, 0);
    s.update(kFrame, 0, 0);
    const double a = s.update(5.0, 3000, 0);
    check(std::fabs(a) < 0.5, "a five-second stall followed by a jump does not fling the arrow");
  }

  std::printf("%s\n", failures ? "FAILED" : "all passed");
  return failures ? 1 : 0;
}
