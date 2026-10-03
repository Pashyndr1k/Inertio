#pragma once
#include "spring.h"

struct Settings {
  SpringParams motion;
  double size = 1.0;   // multiplies the arrow's size, on top of Windows' own cursor size
  bool shadow = true;
};

// Reads inertia-cursor.ini next to the .exe. Missing file or keys keep the defaults.
Settings LoadSettings();
