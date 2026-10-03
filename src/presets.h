#pragma once
/*
  The motion presets offered in the tray menu. Each setting gets a short list of named
  choices, one of which is the default from SpringParams. Plain C++, so the tests can check
  that the defaults here and in spring.h never drift apart.
*/

#include <cmath>
#include <cstddef>

#include "spring.h"

struct Preset {
  const wchar_t* label;
  double value;  // in the units the ini file uses (MaxAngle in degrees)
};

struct MotionSetting {
  const wchar_t* menuLabel;
  const wchar_t* iniKey;
  const Preset* presets;
  size_t count;
  size_t defaultIndex;
};

namespace presets {

constexpr double kRadToDeg = 180 / ArrowSpring::kPi;

constexpr Preset kReturnSpeed[] = {
    {L"Slow", 1.5}, {L"Relaxed", 2.2}, {L"Default", 3.0}, {L"Quick", 4.5}, {L"Snappy", 6.5},
};
constexpr Preset kWobble[] = {
    {L"Bouncy", 0.15}, {L"Lively", 0.3}, {L"Default", 0.45}, {L"Gentle", 0.7}, {L"None", 1.0},
};
constexpr Preset kTiltStrength[] = {
    {L"Subtle", 1500}, {L"Light", 3000}, {L"Default", 6000}, {L"Strong", 12000}, {L"Extreme", 25000},
};
constexpr Preset kMaxTilt[] = {
    {L"45°", 45}, {L"90°", 90}, {L"135°", 135}, {L"180° (default)", 180},
};

constexpr MotionSetting kSettings[] = {
    {L"&Return speed", L"Frequency", kReturnSpeed, 5, 2},
    {L"&Wobble", L"Damping", kWobble, 5, 2},
    {L"&Tilt strength", L"Drag", kTiltStrength, 5, 2},
    {L"&Max tilt", L"MaxAngle", kMaxTilt, 4, 3},
};
constexpr size_t kCount = sizeof(kSettings) / sizeof(kSettings[0]);

// The field of SpringParams each setting drives, in ini units.
inline double Get(const SpringParams& p, size_t setting) {
  switch (setting) {
    case 0: return p.frequency;
    case 1: return p.damping;
    case 2: return p.drag;
    default: return p.maxAngle * kRadToDeg;
  }
}

inline void Set(SpringParams& p, size_t setting, double value) {
  switch (setting) {
    case 0: p.frequency = value; break;
    case 1: p.damping = value; break;
    case 2: p.drag = value; break;
    default: p.maxAngle = value / kRadToDeg; break;
  }
}

// Which preset the current value is, or -1 when the ini holds something else.
inline int Match(const SpringParams& p, size_t setting) {
  const MotionSetting& s = kSettings[setting];
  const double v = Get(p, setting);
  for (size_t i = 0; i < s.count; ++i) {
    if (std::fabs(s.presets[i].value - v) <= 1e-3 * (1 + std::fabs(v))) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace presets
