#pragma once
#include "spring.h"

struct Settings {
  SpringParams motion;
  double size = 1.0;   // multiplies the arrow's size, on top of Windows' own cursor size
  bool shadow = true;
};

// Reads inertia-cursor.ini next to the .exe. Missing file or keys keep the defaults.
Settings LoadSettings();

// Writes one [Motion] key to inertia-cursor.ini, creating the file if needed. Comments and
// other keys in the file are left as they are. Returns false if the file can't be written.
bool SaveMotionValue(const wchar_t* key, double value);
