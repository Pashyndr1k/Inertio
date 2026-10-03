# Inertia Cursor

A small Windows background app that replaces the arrow pointer with an animated one. The tip
stays exactly where the pointer is and the whole arrow swings around it: move the mouse and
the arrow turns to point the way you are going, up to a full half turn when you move straight
back against it. Stop, and it swings back to the classic upright pose with a little springy
overshoot.

Only the normal arrow changes. The text beam, the hand over links, the resize arrows, the
busy cursors and any pointer an app draws itself all stay exactly as they were.

## Run it

1. Get `InertiaCursor.exe`: download the **InertiaCursor** artifact from the latest
   [Inertia Cursor workflow run](https://github.com/Pashyndr1k/Inertio/actions/workflows/build.yml), or build it
   (below).
2. Put `InertiaCursor.exe` (and, if you want to tweak it, `inertia-cursor.ini`) in a folder
   that will stay put, such as `%LOCALAPPDATA%\InertiaCursor`.
3. Double-click it. There's no window; a tilted arrow appears in the tray.

Right-click the tray arrow for:

- **Animated arrow**: turn the effect off and on (double-clicking the icon does the same).
- **Start with Windows**: run it at sign-in. This adds a per-user entry, no admin needed.
- **Quit**: put the normal arrow back and exit.

It needs Windows 10 (1703 or later) or Windows 11. One .exe, nothing to install.

### Getting your normal cursor back

The app restores your pointer scheme when you quit, sign out or shut down, if it crashes,
and even if it is killed from Task Manager (a tiny watchdog copy of the app waits for it to
exit and puts the cursors back). Starting the app also restores first, in case anything was
left behind. If the arrow is ever missing anyway, any of these brings it back:

- run `InertiaCursor.exe --restore`,
- or press Win+R, run `main.cpl`, open the **Pointers** tab and press **OK**.

## Tune it

Edit `inertia-cursor.ini` next to the .exe, then quit and restart the app:

| Setting | Default | What it does |
| --- | --- | --- |
| `Frequency` | 3.0 | How fast it swings back, in swings per second |
| `Damping` | 0.45 | Below 1 overshoots before settling; 1 or more returns smoothly |
| `Drag` | 6000 | How hard moving swings the arrow round to point along the motion |
| `Kick` | 140 | How much it swings when you start or stop |
| `MaxAngle` | 180 | Furthest turn either way, in degrees (180 lets it face straight back) |
| `Size` | 1.0 | Arrow size, on top of Windows' own pointer size |
| `Shadow` | 1 | Soft drop shadow, 1 on or 0 off |

At the defaults, a slow move turns the arrow about 80% of the way to pointing along it and a
brisk one about 95%: moving right turns it roughly 100 degrees, moving straight back against
it about 165. When you stop it swings back with an overshoot of about 15 to 25 degrees.

## Build it

With Visual Studio 2022 ("Desktop development with C++", which includes CMake), open a
**Developer Command Prompt for VS** in this folder and run `build.bat`, or:

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

The .exe lands in `build\Release\`. MinGW-w64 also works (`cmake -G "MinGW Makefiles"`),
including cross-compiling from Linux with a toolchain file pointing at
`x86_64-w64-mingw32-g++`.

On Linux or macOS, `cmake -S . -B build && cmake --build build && ctest --test-dir build`
builds and runs just the motion tests.

## How it works

- `src/system_cursor.cpp` swaps the system arrow (`OCR_NORMAL`) for a fully transparent
  cursor with `SetSystemCursor`, and restores the user's scheme with
  `SystemParametersInfo(SPI_SETCURSORS)`. Because the system keeps the same arrow handle
  when its image is swapped, comparing it with `GetCursorInfo` tells us whenever the arrow,
  and not some other pointer, is on screen.
- `src/overlay.cpp` draws the arrow with GDI+ into a per-pixel-alpha layered window that is
  topmost, click-through, never activated and kept out of Alt+Tab. Its tip sits exactly on
  the cursor's hotspot, so clicks land where they always did. It follows Windows' pointer
  size setting and each monitor's DPI.
- `src/spring.h` is the motion: the arrow is pinned at its tip like a weathervane. Moving
  steers it to point along the motion, inertia from speeding up or braking swings the tail,
  and a torsion spring and damper pull it back upright. It steps at a fixed 240 Hz, so it feels the same at 60 or 144 Hz, and it is
  plain C++ covered by `tests/spring_test.cpp`.
- `src/main.cpp` runs the tray icon, the autostart entry, the crash and watchdog restore,
  and a frame loop paced to the display with `DwmFlush`. While no arrow is on screen it
  only checks back a few times a second.

## Known limits

- The drawn arrow follows the real cursor one display frame behind, as any software cursor
  does. On a 60 Hz screen during fast moves it can trail slightly.
- Exclusive-fullscreen games draw over every window, including this one. Most games use
  their own cursor, but if one shows the standard arrow it will look invisible: turn the
  effect off from the tray while you play.
- The overlay is an ordinary window, so screenshots and screen recordings show it.
- On the sign-in and UAC screens Windows shows its own cursor.
