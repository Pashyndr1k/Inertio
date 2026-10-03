#pragma once
#include <windows.h>

// Swaps the system's normal arrow for an invisible one, and puts the user's cursors back.
// Only the arrow (OCR_NORMAL) is touched: the text beam, hand, resize arrows, busy cursors and
// any cursor an app draws itself stay exactly as they were.
namespace system_cursor {

// The shared arrow handle the system hands to every app. Its image changes when we swap it,
// the handle does not, so this is how we know the arrow is the cursor on screen right now.
HCURSOR ArrowHandle();

bool HideArrow();

// Reloads the user's whole cursor scheme from the registry, undoing HideArrow.
void Restore();

// True while the cursor on screen is the normal arrow (and is not hidden).
bool ArrowIsShowing();

}  // namespace system_cursor
