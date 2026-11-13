// Golden driver for the Input System's device buffers (ISDeviceInput, ISHandleEvent): random call sequences over the
// original C++ class (is_device_orig.h), one trace line per call with its results. The Rust replay
// (rust/crates/ee_input/src/golden.rs) makes the same calls on the Rust devices and compares every result.
//
//   clang-cl /nologo /EHsc /O1 -I include tests\input\is_device_golden.cpp /Fe:build\lle1\is_device_golden.exe
//   build\lle1\is_device_golden.exe <sequences> [first seed] > golden.txt
//
// Lines (fields separated by one space):
//   seq <n> <keyboard present 0/1> <mouse present 0/1>   a new sequence: fresh devices, the system pointers set or NULL
//   acq <d> | unacq <d>                                   d: 0 keyboard device, 1 mouse device
//   isacq <d> = <0/1>
//   data <d> <count> <null 0/1> = <result> <n> [<ofs>:<data> ...]   (ofs and data as 32-bit hex)
//   keys <d> = <result> [<index> ...]                    (the key bytes that are 0x80; every other byte is 0)
//   mouse <d> = <result> <x> <y> <z> <b0> <b1> <b2> <b3>
//   key <d> <scan> <down> | move <d> <dx> <dy> | wheel <d> <delta> | btn <d> <button> <down>   (direct calls)
//   burst <d> <n>                                          n calls of MouseMove(1, -1)
//   ev <type> <scan> <dx> <dy> <button> <wheel bits hex>  ISHandleEvent
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <deque>

#define LLE_API
#include "platform/PlatWindow.h"

struct PlatMutex { int mUnused; };
static PlatMutex sMutexes[4];
static int sNextMutex;
static PlatMutex* PlatMutexCreate() { return &sMutexes[sNextMutex++ & 3]; }
static void PlatMutexDestroy(PlatMutex*) {}
static void PlatMutexLock(PlatMutex*) {}
static void PlatMutexUnlock(PlatMutex*) {}

#include "is_device_orig.h"

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static const uint32_t kScans[] = {1, 0x1d, 0x2a, 0x38, 0x9d, 0xc8, 0xff, 0x1c};
static uint32_t Scan()
{
    switch (Below(6)) {
    case 0: return Below(300);
    case 1: return Below(2) ? 0 : 256 + Below(4);
    default: return kScans[Below(8)];
    }
}
static int32_t Delta() { return Below(3) == 0 ? 0 : (int32_t)Below(15) - 7; }
static float Wheel()
{
    static const float kWheels[] = {0.0f, 1.0f, -1.0f, 0.5f, 1.5f, 2.5f, -0.5f, 0.004f, -0.0125f, 999.5f, -999.75f,
                                    0.3333f};
    if (Below(4) == 0) {
        uint32_t theBits = Next();
        float theValue;
        memcpy(&theValue, &theBits, 4);
        // a NaN or a value past 2^31 / 120 converts to the indefinite value, 32-bit here and 64-bit on LP64 systems,
        // and big wheels overflow a 32-bit sum: the golden keeps to what every system computes alike (the Rust unit
        // tests pin the indefinite value)
        if (theValue != theValue || fabsf(theValue) >= 1000.0f)
            theValue = 1.25f;
        return theValue;
    }
    return kWheels[Below(12)];
}

static void Data(ISDeviceInput* inDevice, int inD)
{
    static ISInputRecord sRecords[400];
    unsigned long theCount = Below(4) == 0 ? Below(300) : Below(20);
    int theNull = Below(10) == 0;
    unsigned long theAsked = theCount;
    long theResult = inDevice->GetData(theNull ? NULL : sRecords, theCount);
    printf("data %d %lu %d = %ld %lu", inD, theAsked, theNull, theResult, theCount);
    if (!theNull)
        for (unsigned long i = 0; i < theCount; i++)
            printf(" %x:%x", (uint32_t)sRecords[i].dwOfs, (uint32_t)sRecords[i].dwData);
    printf("\n");
}

static void Sequence(int inIndex)
{
    ISDeviceInput* theDevices[2] = {new ISDeviceInput, new ISDeviceInput};
    int theKeyboard = Below(12) != 0;
    int theMouse = Below(12) != 0;
    sKeyboardInput = theKeyboard ? theDevices[0] : NULL;
    sMouseInput = theMouse ? theDevices[1] : NULL;
    printf("seq %d %d %d\n", inIndex, theKeyboard, theMouse);
    int theOps = 30 + Below(60);
    for (int i = 0; i < theOps; i++) {
        uint32_t theOp = Below(100);
        int d = Below(2);
        ISDeviceInput* theDevice = theDevices[d];
        if (theOp < 40) {
            static const uint32_t kTypes[] = {PLAT_EVENT_KEY_DOWN, PLAT_EVENT_KEY_UP, PLAT_EVENT_MOUSE_MOVE,
                                              PLAT_EVENT_MOUSE_DOWN, PLAT_EVENT_MOUSE_UP, PLAT_EVENT_MOUSE_WHEEL,
                                              PLAT_EVENT_FOCUS_LOST, PLAT_EVENT_KEY_DOWN, PLAT_EVENT_MOUSE_DOWN,
                                              PLAT_EVENT_TEXT, PLAT_EVENT_NONE, PLAT_EVENT_FOCUS_GAINED};
            PlatInputEvent theEvent;
            memset(&theEvent, 0, sizeof(theEvent));
            theEvent.mType = kTypes[Below(Below(8) == 0 ? 12 : 9)];
            uint32_t theScan = Scan();
            int32_t theDX = Delta(), theDY = Delta();
            uint32_t theButton = Below(8);
            float theWheel = Wheel();
            if (theEvent.mType == PLAT_EVENT_KEY_DOWN || theEvent.mType == PLAT_EVENT_KEY_UP)
                theEvent.mKey.mScanCode = theScan;
            else {
                theEvent.mMouse.mDeltaX = theDX;
                theEvent.mMouse.mDeltaY = theDY;
                theEvent.mMouse.mButton = theButton;
                theEvent.mMouse.mWheel = theWheel;
            }
            // the trace holds what the event carries for its type (the union's other view is 0)
            uint32_t theBits;
            memcpy(&theBits, &theEvent.mMouse.mWheel, 4);
            if (theEvent.mType == PLAT_EVENT_KEY_DOWN || theEvent.mType == PLAT_EVENT_KEY_UP)
                printf("ev %u %u 0 0 0 0\n", theEvent.mType, theScan);
            else
                printf("ev %u 0 %d %d %u %x\n", theEvent.mType, theDX, theDY, theButton, theBits);
            ISHandleEvent(theEvent);
        } else if (theOp < 50) {
            printf("acq %d\n", d);
            theDevice->Acquire();
        } else if (theOp < 55) {
            printf("unacq %d\n", d);
            theDevice->Unacquire();
        } else if (theOp < 57) {
            printf("isacq %d = %d\n", d, theDevice->IsAcquired() ? 1 : 0);
        } else if (theOp < 70) {
            Data(theDevice, d);
        } else if (theOp < 75) {
            unsigned char theKeys[256];
            memset(theKeys, 0x55, sizeof(theKeys));
            long theResult = theDevice->GetKeyState(theKeys);
            printf("keys %d = %ld", d, theResult);
            if (theResult == kISInputOK)
                for (int k = 0; k < 256; k++) {
                    if (theKeys[k] == 0x80)
                        printf(" %d", k);
                    else if (theKeys[k] != 0)
                        printf(" !%d=%d", k, theKeys[k]);
                }
            printf("\n");
        } else if (theOp < 81) {
            ISMouseState theState;
            memset(&theState, 0x55, sizeof(theState));
            long theResult = theDevice->GetMouseState(theState);
            if (theResult == kISInputOK)
                printf("mouse %d = %ld %ld %ld %ld %u %u %u %u\n", d, theResult, theState.lX, theState.lY, theState.lZ,
                       theState.rgbButtons[0], theState.rgbButtons[1], theState.rgbButtons[2], theState.rgbButtons[3]);
            else
                printf("mouse %d = %ld\n", d, theResult);
        } else if (theOp < 86) {
            uint32_t theScan = Scan();
            int theDown = Below(2);
            printf("key %d %u %d\n", d, theScan, theDown);
            theDevice->Key(theScan, theDown != 0);
        } else if (theOp < 90) {
            int32_t theDX = Delta(), theDY = Delta();
            printf("move %d %d %d\n", d, theDX, theDY);
            theDevice->MouseMove(theDX, theDY);
        } else if (theOp < 93) {
            int32_t theDelta = Below(3) == 0 ? 0 : (int32_t)(Next() % 481) - 240;
            printf("wheel %d %d\n", d, theDelta);
            theDevice->MouseWheel(theDelta);
        } else if (theOp < 97) {
            uint32_t theButton = Below(7);
            int theDown = Below(2);
            printf("btn %d %u %d\n", d, theButton, theDown);
            theDevice->MouseButton(theButton, theDown != 0);
        } else {
            uint32_t theCount = 100 + Below(250);
            printf("burst %d %u\n", d, theCount);
            for (uint32_t k = 0; k < theCount; k++)
                theDevice->MouseMove(1, -1);
        }
    }
    sKeyboardInput = NULL;
    sMouseInput = NULL;
    delete theDevices[0];
    delete theDevices[1];
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    for (int i = 0; i < theCount; i++) {
        sRandom = (uint32_t)(theFirst + i) * 2654435761u + 1;
        Next();
        Sequence(theFirst + i);
    }
    return 0;
}
