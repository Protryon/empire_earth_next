// Platform layer, Win32: the key-state queries of include/platform/PlatWindow.h for the current game, whose window
// and messages are still Win32 (GetKeyState, as the callers did themselves). The rest of PlatWindow.h is
// src/Platform/SDL/PlatWindow.cpp, which the game switches to (PORT.md step 4).
#include <windows.h>

#include "platform/PlatWindow.h"

extern "C" int PlatKeyIsDown(uint32_t inVirtualKey)
{
    return GetKeyState(static_cast<int>(inVirtualKey)) < 0 ? 1 : 0;
}

extern "C" int PlatKeyIsToggled(uint32_t inVirtualKey)
{
    return (GetKeyState(static_cast<int>(inVirtualKey)) & 1) ? 1 : 0;
}
