// Golden driver for Input System.cpp's entry points (LLE lane round 7): ISInitialize (with and without the key
// names' resources), the keyboard's and mouse's Refresh/Resume/Suspend (the clock and the window's relative mode),
// ISHandleEvent, ISSetDoublePressTimeDelta and the shutdown through UShutdown, over several init/shutdown cycles.
// The window layer's PlatMouseSetRelative is a fake that prints its calls; TMGetTime is pinned (ee_time_pin).
//
//   python -I build/lle7/drvis.py orig|new;   build/lle7/is_<orig|new>.exe <cycles> > golden.txt
// Linked over the original Input System.cpp and Utilities.cpp (orig) and over the new units (new): both must print
// the same.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/UResourceFile.h"
#include "lle/GE2DPoint.h"
#include "GEWindow.h"
#include "lle/Input System.h"
#include "platform/PlatWindow.h"
#include "rust/base/time.h"

void ISHandleEvent(const PlatInputEvent& inEvent);   // Input System.cpp

static uint64_t gRand;
static uint32_t R(uint32_t n)
{
    gRand = gRand * 6364136223846793005ull + 1442695040888963407ull;
    return n ? (uint32_t)(gRand >> 33) % n : 0;
}

static PlatWindow* gWindow;

extern "C" int PlatMouseSetRelative(PlatWindow* inWindow, int inRelative)
{
    printf("rel %d %d\n", inWindow == gWindow ? 1 : inWindow == NULL ? 0 : 2, inRelative);
    return 0;
}

static void Dump()
{
    if (gISKeyboard == NULL || gISMouse == NULL) {
        printf("dump none %d %d\n", gISKeyboard == NULL, gISMouse == NULL);
        return;
    }
    printf("kb %lu %d |", gISKeyboard->GetModifierState(), (int)gISKeyboard->GetShouldUpdate());
    for (unsigned long i = 0; i < 0xe3; i++) {
        ISEntity* e = NULL;
        if (gISKeyboard->GetEntity(i, e) != kErrorOK || e == NULL)
            continue;
        if (e->GetValue() || e->JustPressed() || e->JustUnpressed() || e->JustDoublePressed())
            printf(" %lx:%ld,%d,%d,%d", i, e->GetValue(), (int)e->JustPressed(), (int)e->JustUnpressed(),
                   (int)e->JustDoublePressed());
    }
    printf("\nms %d |", (int)gISMouse->GetShouldUpdate());
    for (unsigned long i = 0; i < 7; i++) {
        ISEntity* e = NULL;
        gISMouse->GetEntity(i, e);
        printf(" %ld,%ld,%d,%d,%d", e->GetValue(), e->GetNormalizedValue(), (int)e->JustPressed(),
               (int)e->JustUnpressed(), (int)e->JustDoublePressed());
    }
    printf("\n");
}

static const unsigned char kKeys[] = {0x1e, 0x1f, 0x2a, 0x36, 0x1d, 0x9d, 0x38, 0xb8, 0x01, 0x39, 0xc8};

int main(int argc, char** argv)
{
    int theCycles = argc > 1 ? atoi(argv[1]) : 40;
    gRand = 11;
    gWindow = reinterpret_cast<PlatWindow*>(0x1000);
    GEWindow* theWindow = new GEWindow(gWindow, 640, 480, 32, false);   // never destroyed (its dtor is GEWindow.cpp's)
    GEWindow* theNoWindow = new GEWindow(NULL, 640, 480, 32, false);
    int64_t theTime = 1000;
    for (int c = 0; c < theCycles; c++) {
        ee_time_pin(theTime);
        unsigned long theDelay = 100 + R(400);
        bool theHasWindow = R(4) != 0;
        printf("cycle %d %lu %d\n", c, theDelay, (int)theHasWindow);
        long theResult = ISInitialize(NULL, theHasWindow ? theWindow : theNoWindow, NULL, 0, theDelay);
        printf("init %ld %d %d\n", theResult, gISKeyboard != NULL, gISMouse != NULL);
        Dump();
        int theOps = 20 + R(200);
        for (int op = 0; op < theOps; op++) {
            switch (R(14)) {
            case 0: case 1: case 2: case 3: {
                PlatInputEvent e;
                memset(&e, 0, sizeof e);
                e.mType = R(2) ? PLAT_EVENT_KEY_DOWN : PLAT_EVENT_KEY_UP;
                e.mKey.mScanCode = kKeys[R(sizeof kKeys)];
                printf("key %u %x\n", e.mType, e.mKey.mScanCode);
                ISHandleEvent(e);
                continue;
            }
            case 4: case 5: {
                PlatInputEvent e;
                memset(&e, 0, sizeof e);
                static const uint32_t kTypes[] = {PLAT_EVENT_MOUSE_MOVE, PLAT_EVENT_MOUSE_DOWN, PLAT_EVENT_MOUSE_UP,
                                                  PLAT_EVENT_MOUSE_WHEEL};
                e.mType = kTypes[R(4)];
                e.mMouse.mDeltaX = (int)R(41) - 20;
                e.mMouse.mDeltaY = (int)R(41) - 20;
                e.mMouse.mButton = 1 + R(5);
                e.mMouse.mWheel = (float)((int)R(5) - 2);
                printf("mouse %u %d %d %u %d\n", e.mType, e.mMouse.mDeltaX, e.mMouse.mDeltaY, e.mMouse.mButton,
                       (int)e.mMouse.mWheel);
                ISHandleEvent(e);
                continue;
            }
            case 6:
                theTime += R(R(2) ? 50 : 600);
                printf("time %lld\n", (long long)theTime);
                ee_time_pin(theTime);
                continue;
            case 7:
                printf("kref\n");
                gISKeyboard->Refresh();
                break;
            case 8:
                printf("mref\n");
                gISMouse->Refresh();
                break;
            case 9:
                printf(R(2) ? (gISKeyboard->Resume(), "kres\n") : (gISKeyboard->Suspend(), "ksus\n"));
                break;
            case 10:
                printf("mres\n");
                gISMouse->Resume();
                break;
            case 11:
                printf("msus\n");
                gISMouse->Suspend();
                break;
            case 12: {
                unsigned long d = 50 + R(500);
                printf("delay %lu\n", d);
                ISSetDoublePressTimeDelta(d);
                continue;
            }
            default:
                printf("mxy\n");
                gISMouse->RefreshXY();
                break;
            }
            Dump();
        }
        printf("progress %d\n", (int)UShutdownInProgress());
        UShutdown(R(2) != 0, false);
        printf("shutdown %d %d\n", gISKeyboard == NULL, gISMouse == NULL);
    }
    return 0;
}
