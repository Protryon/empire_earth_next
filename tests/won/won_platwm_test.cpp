// PlatWindowManager as a whole Rust object (R6 WON round 40): PlatWindowManager.cpp's statements copied verbatim
// before the port (won_platwm_orig.h, with its class and PlatGUIWindow::GetClientRect) against the port's shell and
// host over ee_won's platwm.rs (won_platwm_port.h, copied from the working tree), both over the same stand-in world
// (won_platwm_world.h: windows behaving as PlatGUIWindow's methods do, the WONAPI classes the manager reads, the
// platform's window system; won_platwm_cases.h: the windows' methods and the runner). The caption font is the
// platform layer's (ee_plat_ui) with the test face of its goldens registered.
//
// 2,000 worlds from seeds: 1..6 windows (parents, frames, popups, topmost, visible, modal or not, titles, offscreen
// images of any size), a game window's client size, an event forward or none; then 60 random operations each: windows
// added, removed, raised, focused, captured, hit-tested; platform events of every type (mouse moves, presses with
// every button and click count, releases, wheels; keys with every modifier, Alt+Tab, F10, control characters; text
// of 0..64 bytes, invalid UTF-8, a character past U+FFFF; focus, close, quit, window events); the surface's size and
// pixel scale from the UI thread and another (pending until the next pass); the present rectangle; passes of the
// event loop with and without an event; paints and composites; the colour scheme; the screen; wakes; windows shown,
// hidden, closed and retitled. After each, the trace (every call on a window, its events, the platform calls, the
// forwards) and a snapshot (focus, capture, surface, scale, the pointer, every window's place and flags, the frame's
// size, pitch, serial and pixel hash) must be the same. Exit code: the number of mismatching worlds.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <iterator>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONSurface.h"
#include "platform/PlatWindow.h"
#include "platform/PlatThread.h"
#include "platform/PlatFont.h"
#include "platform/PlatString.h"
#include "platform/PlatTime.h"
#include "rust/ee_won.h"

// the world's window system in place of the platform's (renamed: argument-dependent lookup on PlatWindow* and
// PlatInputEvent* would find the real ones too), and the manager's protected members reachable from the runner
#define PlatWindowWaitEvent WorldWaitEvent
#define PlatWindowGetClientSize WorldGetClientSize
#define PlatWindowMinimize WorldMinimize
#define protected public

#define EE_PLATWM_COMMON \
inline int PlatGUIPixels(int inLogical, float inScale) \
{ return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f); } \
static std::string PlatGUIToUtf8(const wchar_t* inText, unsigned short inLength) \
{ \
    std::string theText; \
    if (inText == NULL || inLength == 0) return theText; \
    uint32_t theCount = PlatWideToUtf8(inText, inLength, NULL, 0); \
    theText.resize(theCount); \
    if (theCount != 0) PlatWideToUtf8(inText, inLength, &theText[0], theCount); \
    return theText; \
}

namespace Orig {
#include "won_platwm_world.h"
EE_PLATWM_COMMON
#include "won_platwm_orig.h"
#include "won_platwm_cases.h"
}
namespace Port {
#include "won_platwm_world.h"
EE_PLATWM_COMMON
#include "won_platwm_port.h"
#include "won_platwm_cases.h"
}

int main(int argc, char** argv)
{
    std::string ttf = std::string(__FILE__);
    ttf = ttf.substr(0, ttf.find_last_of("/\\")) + "/../../rust/crates/ee_base/src/plat_ui/golden_font.ttf";
    std::ifstream in(ttf.c_str(), std::ios::binary);
    static std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.empty() || !PlatFontRegister("Synth", 0, 0, data.data(), (uint32_t)data.size())) { printf("no test font at %s\n", ttf.c_str()); return 1; }
    PlatFontRegister("Synth", 1, 0, data.data(), (uint32_t)data.size());
    int worlds = argc > 1 ? atoi(argv[1]) : 2000, failures = 0;
    long ops = 0, lines = 0;
    for (int n = 0; n < worlds; n++) {
        unsigned long long seed = 0x51ED270B27F3A1ull * (n + 1);
        int k = 0;
        if (getenv("PLATWM_DEBUG")) fprintf(stderr, "world %d orig\n", n);
        std::string a = Orig::Run(seed, k);
        if (getenv("PLATWM_DEBUG")) fprintf(stderr, "world %d port\n", n);
        std::string b = Port::Run(seed, k);
        ops += k;
        lines += (long)std::count(a.begin(), a.end(), '\n');
        if (a != b) {
            if (++failures < 4) {
                size_t i = 0;
                while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
                size_t s = a.rfind("\nop ", i);
                s = s == std::string::npos ? 0 : s + 1;
                printf("MISMATCH world %d at byte %zu\n--- original\n%s\n--- port\n%s\n", n, i, a.substr(s, i - s + 300).c_str(),
                       b.substr(s, i - s + 300).c_str());
            }
        }
    }
    printf("%d worlds, %ld operations, %ld trace lines, %d mismatching worlds\n", worlds, ops, lines, failures);
    return failures;
}
