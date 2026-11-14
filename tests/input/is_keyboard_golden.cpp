// Golden driver for the Input System's keyboard and mouse with their entities (ISKeyboard, ISMouse, ISKey,
// ISExtendedKey, ISMouseButton, ISMouseAxis): random call sequences over the original C++ classes
// (is_keyboard_orig.h) on the original device (is_device_orig.h), one trace line per call and a dump of every
// observable state after it. The Rust replay (rust/crates/ee_input/src/golden_keyboard.rs) makes the same calls on
// the Rust keyboard and mouse and compares every dump.
//
//   python -I build/lle1/cc.py build/lle2/iskb.exe tests/input/is_keyboard_golden.cpp /I tests/input
//   build/lle2/iskb.exe <sequences> [first seed] > golden.txt
//
// Lines (fields separated by one space):
//   name k|m <index> <name...>       once at the start: every entity's name (keyboard by scan code, mouse 0..6)
//   seq <n> <double-press delta>     a new sequence: fresh devices
//   pre <scan> <button>              a key and a mouse button held on the fresh devices
//   new                              the keyboard and the mouse created (at time 0)
//   time <t>                         TMGetTime from now on (32-bit)
//   ev <type> <scan> <dx> <dy> <button> <wheel bits hex>  ISHandleEvent
//   key <scan> <down> | burst <n>    direct keyboard device calls (burst: n presses and releases of 0x1e)
//   kref | kres | ksus               the keyboard's Refresh, Resume, Suspend
//   mref | mxy | mres | msus | mbuf <0/1>  the mouse's Refresh, RefreshXY, Resume, Suspend, SetUseBufferedMovement
//   kent <i> = <result> <null 0/1> | ment <i> = <result> <null 0/1>   GetEntity
//   kb = <modifiers> <down> <last down> <should update> <overflowed> | <i>:<status>,<last>,<value>,<pressed amount>,
//        <unpressed amount>,<double>,<last press time> ... | e<i>:<value>,<normalized>,<pressed>,<unpressed>,<double>,
//        <pressed amount>,<unpressed amount> ...           (keys listed when any field is nonzero)
//   ms = <should update> <overflowed> <x> <y> <z> <b0..b3> | <last x> <y> <z> <b0..b3> | <axis normalized x3> |
//        b<i>:<count>,<last count>,<value>,<pressed amount>,<unpressed amount>,<double>,<last press time> ...
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <deque>
#include <vector>
#include <string>

#define LLE_API
#include "platform/PlatWindow.h"
#include "platform/PlatKeys.h"

struct PlatMutex { int mUnused; };
static PlatMutex sMutexes[4];
static int sNextMutex;
static PlatMutex* PlatMutexCreate() { return &sMutexes[sNextMutex++ & 3]; }
static void PlatMutexDestroy(PlatMutex*) {}
static void PlatMutexLock(PlatMutex*) {}
static void PlatMutexUnlock(PlatMutex*) {}

#include "is_device_orig.h"

// what the original classes use from the engine
enum { kErrorOK = 0, kErrorNotFound = 3, kErrorFailed = 9, kErrorBeyondEnd = 0xa };
struct UWideString {
    UWideString(const UWideString&) = default;
    std::string s;
    UWideString() {}
    UWideString(const char* inText) : s(inText) {}
    UWideString(const std::string& inText) : s(inText) {}
    operator const char*() const { return s.c_str(); }
};
static UWideString operator+(const UWideString& a, const char* b) { return UWideString(a.s + b); }
static UWideString operator+(const UWideString& a, const UWideString& b) { return UWideString(a.s + b.s); }
struct UString : UWideString {
    UString(const char* inText) : UWideString(inText) {}
};
#include "lle/ISModifierState.h"
struct UResourceFile {
    void GetString(unsigned long, UWideString&) {}
};
struct ISCalibration;
static uint32_t sTime;
static unsigned long TMGetTime() { return sTime; }
static unsigned long gDoublePressTimeDelta = 250;
static int PlatMouseSetRelative(PlatWindow*, int) { return 0; }
static long ee_input_get_data(ISDeviceInput* d, ISInputRecord* r, unsigned long* c) { return d->GetData(r, *c); }
static long ee_input_get_key_state(ISDeviceInput* d, unsigned char* k) { return d->GetKeyState(k); }
static long ee_input_get_mouse_state(ISDeviceInput* d, ISMouseState* s) { return d->GetMouseState(*s); }
static void ee_input_acquire(ISDeviceInput* d) { d->Acquire(); }
static void ee_input_unacquire(ISDeviceInput* d) { d->Unacquire(); }

#define private public
#define protected public
#include "is_keyboard_orig.h"
#undef private
#undef protected

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static const uint32_t kScans[] = {DIK_LSHIFT, DIK_RSHIFT, DIK_LCONTROL, DIK_RCONTROL, DIK_LMENU, DIK_RMENU, DIK_A,
                                  DIK_SPACE,  DIK_F3,     0xe0,         0xe1,         0xe2,      0xdd,  0xff};
static uint32_t Scan()
{
    switch (Below(5)) {
    case 0: return Below(260);
    default: return kScans[Below(14)];
    }
}

static void DumpKeyboard(ISKeyboard* k)
{
    printf("kb = %lu %ld %ld %d %d |", k->mModifierState, k->mNumberOfKeysDown, k->mLastNumberOfKeysDown,
           k->GetShouldUpdate() ? 1 : 0, k->mOverflowed ? 1 : 0);
    for (int i = 0; i < 256; i++) {
        ISKey* theKey = dynamic_cast<ISKey*>(k->mKeys[i]);
        unsigned char s = k->mStatus[i], l = k->mLast[i];
        if (theKey == NULL) {
            if (s || l)
                printf(" %d:%u,%u", i, s, l);
            continue;
        }
        long v = theKey->GetValue();
        unsigned long pa = theKey->JustPressedAmount(), ua = theKey->JustUnpressedAmount();
        if (s || l || v || pa || ua || theKey->mDoublePressed || theKey->mLastPressTime)
            printf(" %d:%u,%u,%ld,%lu,%lu,%d,%lu", i, s, l, v, pa, ua, theKey->mDoublePressed ? 1 : 0,
                   theKey->mLastPressTime);
    }
    printf(" |");
    for (int i = 0xe0; i <= 0xe2; i++) {
        ISEntity* e = k->mKeys[i];
        printf(" e%d:%ld,%ld,%d,%d,%d,%lu,%lu", i, e->GetValue(), e->GetNormalizedValue(), e->JustPressed() ? 1 : 0,
               e->JustUnpressed() ? 1 : 0, e->JustDoublePressed() ? 1 : 0, e->JustPressedAmount(),
               e->JustUnpressedAmount());
    }
    printf("\n");
}

static void DumpMouse(ISMouse* m)
{
    ISMouseState& c = *m->mCurrentStatus;
    ISMouseState& l = *m->mLastStatus;
    printf("ms = %d %d %ld %ld %ld %u %u %u %u | %ld %ld %ld %u %u %u %u | %ld %ld %ld |", m->GetShouldUpdate() ? 1 : 0,
           m->mOverflowed ? 1 : 0, c.lX, c.lY, c.lZ, c.rgbButtons[0], c.rgbButtons[1], c.rgbButtons[2],
           c.rgbButtons[3], l.lX, l.lY, l.lZ, l.rgbButtons[0], l.rgbButtons[1], l.rgbButtons[2], l.rgbButtons[3],
           m->mEntities[0]->GetNormalizedValue(), m->mEntities[1]->GetNormalizedValue(),
           m->mEntities[2]->GetNormalizedValue());
    for (int i = 0; i < 4; i++) {
        ISMouseButton* b = static_cast<ISMouseButton*>(m->mEntities[3 + i]);
        printf(" b%d:%u,%u,%ld,%lu,%lu,%d,%lu", i, m->mButtons[i], m->mLastButtons[i], b->GetValue(),
               b->JustPressedAmount(), b->JustUnpressedAmount(), b->JustDoublePressed() ? 1 : 0, b->mLastPressTime);
    }
    printf("\n");
}

static void Sequence(int inIndex)
{
    static const unsigned long kDeltas[] = {250, 0, 500, 1};
    gDoublePressTimeDelta = Below(5) == 0 ? Below(2000) : kDeltas[Below(4)];
    printf("seq %d %lu\n", inIndex, gDoublePressTimeDelta);
    sTime = 0;
    ISDeviceInput* theDevices[2] = {new ISDeviceInput, new ISDeviceInput};
    sKeyboardInput = theDevices[0];
    sMouseInput = theDevices[1];
    for (uint32_t j = Below(4) == 0 ? Below(4) : 0; j > 0; j--) {   // keys and buttons already held
        uint32_t theScan = Scan();
        uint32_t theButton = Below(4);
        printf("pre %u %u\n", theScan, theButton);
        theDevices[0]->Key(theScan, true);
        theDevices[1]->MouseButton(theButton, true);
    }
    printf("new\n");
    ISKeyboard* k = new ISKeyboard(theDevices[0], "System Keyboard", NULL, 100);
    ISMouse* m = new ISMouse(theDevices[1], NULL, "System Mouse");
    DumpKeyboard(k);
    DumpMouse(m);
    int theOps = 40 + Below(80);
    for (int i = 0; i < theOps; i++) {
        uint32_t theOp = Below(100);
        if (theOp < 30) {
            static const uint32_t kTypes[] = {PLAT_EVENT_KEY_DOWN, PLAT_EVENT_KEY_UP, PLAT_EVENT_MOUSE_MOVE,
                                              PLAT_EVENT_MOUSE_DOWN, PLAT_EVENT_MOUSE_UP, PLAT_EVENT_MOUSE_WHEEL,
                                              PLAT_EVENT_KEY_DOWN, PLAT_EVENT_KEY_UP, PLAT_EVENT_FOCUS_LOST};
            PlatInputEvent theEvent;
            memset(&theEvent, 0, sizeof(theEvent));
            theEvent.mType = kTypes[Below(Below(10) == 0 ? 9 : 8)];
            uint32_t theScan = Scan();
            int32_t theDX = (int32_t)Below(21) - 10, theDY = (int32_t)Below(21) - 10;
            uint32_t theButton = Below(6);
            static const float kWheels[] = {1.0f, -1.0f, 0.5f, 2.0f, -3.0f, 0.25f};
            float theWheel = kWheels[Below(6)];
            uint32_t theBits;
            memcpy(&theBits, &theWheel, 4);
            if (theEvent.mType == PLAT_EVENT_KEY_DOWN || theEvent.mType == PLAT_EVENT_KEY_UP) {
                theEvent.mKey.mScanCode = theScan;
                printf("ev %u %u 0 0 0 0\n", theEvent.mType, theScan);
            } else {
                theEvent.mMouse.mDeltaX = theDX;
                theEvent.mMouse.mDeltaY = theDY;
                theEvent.mMouse.mButton = theButton;
                theEvent.mMouse.mWheel = theWheel;
                printf("ev %u 0 %d %d %u %x\n", theEvent.mType, theDX, theDY, theButton, theBits);
            }
            ISHandleEvent(theEvent);
        } else if (theOp < 36) {
            uint32_t theScan = Scan();
            int theDown = Below(2);
            printf("key %u %d\n", theScan, theDown);
            theDevices[0]->Key(theScan, theDown != 0);
        } else if (theOp < 46) {
            uint32_t theStep = Below(4);
            if (theStep == 0)
                sTime += Below(100);
            else if (theStep == 1)
                sTime += Below(600);
            else if (theStep == 2)
                sTime = Next();
            else
                sTime += 0xffffff00u + Below(400);   // near and across the 32-bit wrap
            printf("time %u\n", sTime);
        } else if (theOp < 58) {
            printf("kref\n");
            k->Refresh();
            DumpKeyboard(k);
        } else if (theOp < 61) {
            printf("kres\n");
            k->Resume();
            DumpKeyboard(k);
        } else if (theOp < 63) {
            printf("ksus\n");
            k->Suspend();
            DumpKeyboard(k);
        } else if (theOp < 75) {
            printf("mref\n");
            m->Refresh();
            DumpMouse(m);
        } else if (theOp < 78) {
            printf("mxy\n");
            m->RefreshXY();
            DumpMouse(m);
        } else if (theOp < 81) {
            printf("mres\n");
            m->Resume();
            DumpMouse(m);
        } else if (theOp < 83) {
            printf("msus\n");
            m->Suspend();
            DumpMouse(m);
        } else if (theOp < 86) {
            int theUse = Below(3) != 0;
            printf("mbuf %d\n", theUse);
            m->SetUseBufferedMovement(theUse != 0);
        } else if (theOp < 90) {
            uint32_t theIndex = Below(3) == 0 ? Below(300) : kScans[Below(14)];
            ISEntity* theEntity = (ISEntity*)1;
            long theResult = k->GetEntity(theIndex, theEntity);
            printf("kent %u = %ld %d\n", theIndex, theResult, theEntity == NULL ? 1 : 0);
        } else if (theOp < 93) {
            uint32_t theIndex = Below(10);
            ISEntity* theEntity = (ISEntity*)1;
            long theResult = m->GetEntity(theIndex, theEntity);
            printf("ment %u = %ld %d\n", theIndex, theResult, theEntity == NULL ? 1 : 0);
        } else if (theOp < 95) {
            uint32_t theCount = 100 + Below(100);
            printf("burst %u\n", theCount);
            for (uint32_t j = 0; j < theCount; j++) {
                theDevices[0]->Key(DIK_A, true);
                theDevices[0]->Key(DIK_A, false);
            }
        } else {
            printf("kb\n");
            DumpKeyboard(k);
            printf("ms\n");
            DumpMouse(m);
        }
    }
    sKeyboardInput = NULL;
    sMouseInput = NULL;
    m->Suspend();
    delete m;
    k->Suspend();
    delete k;
    delete theDevices[0];
    delete theDevices[1];
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    {
        ISDeviceInput theDevices[2];
        ISKeyboard* k = new ISKeyboard(&theDevices[0], "System Keyboard", NULL, 100);
        ISMouse* m = new ISMouse(&theDevices[1], NULL, "System Mouse");
        for (int i = 0; i < 256; i++)
            if (k->mKeys[i])
                printf("name k %d %s\n", i, (const char*)k->mKeys[i]->GetName());
        for (int i = 0; i < 7; i++)
            printf("name m %d %s\n", i, (const char*)m->mEntities[i]->GetName());
        delete m;
        delete k;
    }
    for (int i = 0; i < theCount; i++) {
        sRandom = (uint32_t)(theFirst + i) * 2654435761u + 1;
        Next();
        Sequence(theFirst + i);
    }
    return 0;
}
