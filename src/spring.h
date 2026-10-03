#pragma once
/*
  The arrow is a stiff pendulum pinned at its tip. Its tail sits down and to the right of the
  tip, the classic pose. When the tip moves, two things push on the tail:

    - drag: the tail trails behind the motion, so the tip turns to lead it;
    - inertia: when the tip speeds up or brakes, the tail is left behind or carried past.

  A torsion spring pulls the arrow back to the classic pose and a damper takes the energy out,
  so when the mouse stops the arrow swings back with a small overshoot and settles.

  Screen coordinates, y down. The angle is positive clockwise on screen. Positions are in
  96-DPI pixels so the feel is the same on every monitor. Pure C++, no Windows headers, so it
  can be tested anywhere.
*/

#include <algorithm>
#include <cmath>

struct SpringParams {
  double frequency = 3.0;          // Hz: how quickly the arrow swings back to the classic pose
  double damping = 0.32;           // ratio: below 1 it overshoots a little before settling
  double drag = 520.0;             // rad/s^2 at full speed: how far motion turns the arrow
  double dragHalfSpeed = 900.0;    // px/s at which drag reaches half its strength
  double kick = 140.0;             // rad/s^2 at full acceleration: the swing on start and stop
  double kickHalfAccel = 20000.0;  // px/s^2 at which the kick reaches half its strength
  double maxAngle = 2.6;           // rad: hard limit, so it never spins all the way round
  double smoothing = 0.02;         // s: low-pass on the measured velocity, mice are noisy
};

class ArrowSpring {
 public:
  explicit ArrowSpring(const SpringParams& p = SpringParams()) : p_(p) {}

  void setParams(const SpringParams& p) { p_ = p; }
  const SpringParams& params() const { return p_; }

  // Forget the motion history, e.g. after the arrow was hidden or the cursor jumped.
  void reset(double x, double y) {
    hasPos_ = true;
    px_ = x;
    py_ = y;
    vx_ = vy_ = ax_ = ay_ = 0;
    angle_ = omega_ = 0;
  }

  // Feed the cursor position (96-DPI px) after dt seconds. Returns the new angle in radians.
  double update(double dt, double x, double y) {
    if (!hasPos_) {
      reset(x, y);
      return angle_;
    }
    if (dt <= 0) return angle_;
    if (dt > 0.1) {
      // A long stall (sleep, lock screen, a hitch): treat the gap as a fresh start rather than
      // as one enormous jerk.
      double a = angle_, w = omega_;
      reset(x, y);
      angle_ = a;
      omega_ = w;
      dt = 0.1;
    }

    const double rawVx = (x - px_) / dt, rawVy = (y - py_) / dt;
    px_ = x;
    py_ = y;
    const double k = p_.smoothing > 0 ? 1.0 - std::exp(-dt / p_.smoothing) : 1.0;
    const double nvx = vx_ + (rawVx - vx_) * k, nvy = vy_ + (rawVy - vy_) * k;
    const double rawAx = (nvx - vx_) / dt, rawAy = (nvy - vy_) / dt;
    vx_ = nvx;
    vy_ = nvy;
    ax_ += (rawAx - ax_) * k;
    ay_ += (rawAy - ay_) * k;

    // The push on the tail, as an acceleration in the tip's frame: opposite to the velocity
    // (drag) and opposite to the acceleration (inertia). Each saturates, so a flick turns the
    // arrow decisively but never sends it spinning.
    double fx = 0, fy = 0;
    addSaturated(-vx_, -vy_, p_.drag, p_.dragHalfSpeed, fx, fy);
    addSaturated(-ax_, -ay_, p_.kick, p_.kickHalfAccel, fx, fy);

    // Fixed 240 Hz substeps, semi-implicit Euler: stable at any frame rate.
    const double w0 = 2 * kPi * p_.frequency;
    const double kSpring = w0 * w0, cDamp = 2 * p_.damping * w0;
    acc_ += dt;
    while (acc_ >= kStep) {
      acc_ -= kStep;
      const double c = std::cos(angle_), s = std::sin(angle_);
      // Tail direction: the rest direction rotated by the current angle.
      const double rx = kTailX * c - kTailY * s, ry = kTailX * s + kTailY * c;
      const double torque = rx * fy - ry * fx;
      omega_ += (torque - kSpring * angle_ - cDamp * omega_) * kStep;
      angle_ += omega_ * kStep;
      if (angle_ > p_.maxAngle) {
        angle_ = p_.maxAngle;
        omega_ = (std::min)(omega_, 0.0);
      } else if (angle_ < -p_.maxAngle) {
        angle_ = -p_.maxAngle;
        omega_ = (std::max)(omega_, 0.0);
      }
    }

    // Snap to exactly upright once it is visually still, so the overlay can stop redrawing.
    if (std::abs(angle_) < 1e-4 && std::abs(omega_) < 1e-3 && speed() < 1) {
      angle_ = omega_ = 0;
    }
    return angle_;
  }

  double angle() const { return angle_; }
  double angularVelocity() const { return omega_; }
  double speed() const { return std::hypot(vx_, vy_); }
  bool settled() const { return angle_ == 0 && omega_ == 0 && speed() < 1; }

  static constexpr double kPi = 3.14159265358979323846;
  // From the tip to the middle of the tail at rest: about 23 degrees right of straight down.
  static constexpr double kTailX = 0.3907311284892737;  // sin 23 deg
  static constexpr double kTailY = 0.9205048534524404;  // cos 23 deg
  static constexpr double kStep = 1.0 / 240.0;

 private:
  static void addSaturated(double x, double y, double gain, double half, double& fx, double& fy) {
    const double m = std::hypot(x, y);
    if (m < 1e-9) return;
    const double scale = gain / (m + half);  // gain * (m / (m + half)) / m
    fx += x * scale;
    fy += y * scale;
  }

  SpringParams p_;
  bool hasPos_ = false;
  double px_ = 0, py_ = 0;
  double vx_ = 0, vy_ = 0;
  double ax_ = 0, ay_ = 0;
  double angle_ = 0, omega_ = 0;
  double acc_ = 0;
};
