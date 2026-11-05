// C:\EEX\Low-Level Engine\Input System\Input System.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The devices read the window layer's events (lle/ISDeviceInput.h; DirectInput 7 before, PORT.md step 4.4).
// 0x10038abf..0x10039715: first Input System unit, so it emits every IS class's header-inline members, implicit copy
// members and vector deleting dtors (ISEntity, ISDevice, ISKeyboard, ISMouse, ISKey, ISExtendedKey, ISMouseAxis,
// ISMouseButton, in include order), then ISInitialize, ISSetDoublePressTimeDelta and the shutdown hook.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/TSScheduler.h"
#include "lle/FSMemoryFile.h"
#include "GEWindow.h"
#include "lle/Input System.h"
#include "lle/ISExtendedKey.h"
#include "lle/ISMouseAxis.h"
#include "lle/ISMouseButton.h"
#include "lle/ISDeviceInput.h"
#include "platform/PlatWindow.h"

void ISShutdown(bool inForced);

ISKeyboard* gISKeyboard;
ISMouse* gISMouse;
unsigned long gDoublePressTimeDelta = 250;
static ISDeviceInput* sKeyboardInput;       // the devices' event buffers (DirectInput's devices before)
static ISDeviceInput* sMouseInput;

// ---- ISDeviceInput (lle/ISDeviceInput.h)

ISDeviceInput::ISDeviceInput() : mLock(PlatMutexCreate()), mAcquired(false), mLost(false)
{
    memset(this->mKeys, 0, sizeof(this->mKeys));
    memset(this->mMotion, 0, sizeof(this->mMotion));
    memset(this->mButtons, 0, sizeof(this->mButtons));
}

ISDeviceInput::~ISDeviceInput()
{
    PlatMutexDestroy(this->mLock);
}

void ISDeviceInput::Acquire()
{
    PlatMutexLock(this->mLock);
    if (!this->mAcquired) {   // a newly acquired device starts with an empty buffer and no motion
        this->mRecords.clear();
        this->mLost = false;
        memset(this->mMotion, 0, sizeof(this->mMotion));
    }
    this->mAcquired = true;
    PlatMutexUnlock(this->mLock);
}

void ISDeviceInput::Unacquire()
{
    PlatMutexLock(this->mLock);
    this->mAcquired = false;
    PlatMutexUnlock(this->mLock);
}

bool ISDeviceInput::IsAcquired()
{
    return this->mAcquired;
}

long ISDeviceInput::GetData(ISInputRecord* outRecords, unsigned long& ioCount)
{
    PlatMutexLock(this->mLock);
    if (!this->mAcquired) {
        PlatMutexUnlock(this->mLock);
        ioCount = 0;
        return kISInputNotAcquired;
    }
    unsigned long n = 0;
    while (n < ioCount && !this->mRecords.empty()) {
        if (outRecords != NULL)
            outRecords[n] = this->mRecords.front();
        this->mRecords.pop_front();
        n++;
    }
    long theResult = this->mLost ? kISInputOverflow : kISInputOK;
    this->mLost = false;
    PlatMutexUnlock(this->mLock);
    ioCount = n;
    return theResult;
}

long ISDeviceInput::GetKeyState(unsigned char* outKeys)
{
    PlatMutexLock(this->mLock);
    long theResult = this->mAcquired ? kISInputOK : kISInputNotAcquired;
    if (this->mAcquired)
        memcpy(outKeys, this->mKeys, sizeof(this->mKeys));
    PlatMutexUnlock(this->mLock);
    return theResult;
}

long ISDeviceInput::GetMouseState(ISMouseState& outState)
{
    PlatMutexLock(this->mLock);
    long theResult = this->mAcquired ? kISInputOK : kISInputNotAcquired;
    if (this->mAcquired) {
        outState.lX = this->mMotion[0];
        outState.lY = this->mMotion[1];
        outState.lZ = this->mMotion[2];
        memcpy(outState.rgbButtons, this->mButtons, sizeof(this->mButtons));
        memset(this->mMotion, 0, sizeof(this->mMotion));   // relative axes: the motion since the last read
    }
    PlatMutexUnlock(this->mLock);
    return theResult;
}

void ISDeviceInput::Push(unsigned long inOffset, unsigned long inData)
{
    if (!this->mAcquired)
        return;
    if (this->mRecords.size() >= 256) {   // DIPROP_BUFFERSIZE
        this->mLost = true;
        return;
    }
    ISInputRecord theRecord = {inOffset, inData};
    this->mRecords.push_back(theRecord);
}

void ISDeviceInput::Key(unsigned long inScanCode, bool inDown)
{
    if (inScanCode == 0 || inScanCode > 0xff)
        return;
    PlatMutexLock(this->mLock);
    unsigned char theState = inDown ? 0x80 : 0;
    if (this->mKeys[inScanCode] != theState) {   // DirectInput reports changes only (no auto-repeat)
        this->mKeys[inScanCode] = theState;
        this->Push(inScanCode, theState);
    }
    PlatMutexUnlock(this->mLock);
}

void ISDeviceInput::MouseMove(long inDeltaX, long inDeltaY)
{
    PlatMutexLock(this->mLock);
    if (this->mAcquired) {
        this->mMotion[0] += inDeltaX;
        this->mMotion[1] += inDeltaY;
        if (inDeltaX)
            this->Push(kISMouseOffsetX, (unsigned long)inDeltaX);
        if (inDeltaY)
            this->Push(kISMouseOffsetY, (unsigned long)inDeltaY);
    }
    PlatMutexUnlock(this->mLock);
}

void ISDeviceInput::MouseWheel(long inDelta)
{
    PlatMutexLock(this->mLock);
    if (this->mAcquired && inDelta) {
        this->mMotion[2] += inDelta;
        this->Push(kISMouseOffsetZ, (unsigned long)inDelta);
    }
    PlatMutexUnlock(this->mLock);
}

void ISDeviceInput::MouseButton(unsigned long inButton, bool inDown)
{
    if (inButton > 3)
        return;
    PlatMutexLock(this->mLock);
    unsigned char theState = inDown ? 0x80 : 0;
    if (this->mButtons[inButton] != theState) {
        this->mButtons[inButton] = theState;
        this->Push(kISMouseOffsetButton0 + inButton, theState);
    }
    PlatMutexUnlock(this->mLock);
}

// Feeds a window event to the system keyboard and mouse: keys by their DIK_* scan code (auto-repeat dropped), the
// mouse's motion (raw deltas in relative mode, the exclusive mouse), buttons (DirectInput's order: left, right,
// middle, side) and the wheel (WHEEL_DELTA 120 a notch); a focus loss lets go of what is held
void ISHandleEvent(const PlatInputEvent& inEvent)
{
    switch (inEvent.mType) {
    case PLAT_EVENT_KEY_DOWN:
    case PLAT_EVENT_KEY_UP:
        if (sKeyboardInput != NULL)
            sKeyboardInput->Key(inEvent.mKey.mScanCode, inEvent.mType == PLAT_EVENT_KEY_DOWN);
        break;
    case PLAT_EVENT_MOUSE_MOVE:
        if (sMouseInput != NULL)
            sMouseInput->MouseMove(inEvent.mMouse.mDeltaX, inEvent.mMouse.mDeltaY);
        break;
    case PLAT_EVENT_MOUSE_DOWN:
    case PLAT_EVENT_MOUSE_UP: {
        // PlatWindow.h's buttons 1 left, 2 middle, 3 right, 4 side -> DirectInput's 0 left, 1 right, 2 middle, 3 side
        static const unsigned long kButtons[6] = {4, 0, 2, 1, 3, 4};
        unsigned long theButton = inEvent.mMouse.mButton < 6 ? kButtons[inEvent.mMouse.mButton] : 4;
        if (sMouseInput != NULL)
            sMouseInput->MouseButton(theButton, inEvent.mType == PLAT_EVENT_MOUSE_DOWN);
        break;
    }
    case PLAT_EVENT_MOUSE_WHEEL:
        if (sMouseInput != NULL)
            sMouseInput->MouseWheel((long)lrintf(inEvent.mMouse.mWheel * 120.0f));
        break;
    case PLAT_EVENT_FOCUS_LOST:
        if (sKeyboardInput != NULL)
            for (unsigned long i = 1; i < 256; i++)
                sKeyboardInput->Key(i, false);
        if (sMouseInput != NULL)
            for (unsigned long i = 0; i < 4; i++)
                sMouseInput->MouseButton(i, false);
        break;
    default:
        break;
    }
}

// FUNCTION: LLE 0x100394a7
// Creates the system keyboard and mouse on the window's events (DirectInput's devices in the original: a
// non-exclusive keyboard and an exclusive mouse, buffers of 256)
long ISInitialize(HINSTANCE inInstance, GEWindow* inWindow, UResourceFile* inResources, unsigned long inFirstStringID,
                  unsigned long inDoublePressTimeDelta)
{
    gDoublePressTimeDelta = inDoublePressTimeDelta;
    URegisterShutdown(ISShutdown);
    sKeyboardInput = new ISDeviceInput;
    sMouseInput = new ISDeviceInput;
    gISKeyboard = new ISKeyboard(sKeyboardInput, "System Keyboard", inResources, inFirstStringID);
    gISMouse = new ISMouse(sMouseInput, inWindow->GetOSRepresentation(), "System Mouse");
    return kErrorOK;
}

// FUNCTION: LLE 0x10039691
// Sets the maximum time between two presses that counts as a double press
void ISSetDoublePressTimeDelta(unsigned long inDelta)
{
    gDoublePressTimeDelta = inDelta;
}

// FUNCTION: LLE 0x1003969b
// Suspends and destroys the system mouse and keyboard and their event buffers
void ISShutdown(bool inForced)
{
    if (gISMouse) {
        gISMouse->Suspend();
        delete gISMouse;
        gISMouse = NULL;
    }
    if (gISKeyboard) {
        gISKeyboard->Suspend();
        delete gISKeyboard;
        gISKeyboard = NULL;
    }
    ISDeviceInput* theKeyboard = sKeyboardInput;
    ISDeviceInput* theMouse = sMouseInput;
    sKeyboardInput = NULL;
    sMouseInput = NULL;
    delete theKeyboard;
    delete theMouse;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10038ada
// SYNTHETIC: LLE 0x10038b89
// SYNTHETIC: LLE 0x10038d82
// SYNTHETIC: LLE 0x10038f53
// SYNTHETIC: LLE 0x100390b6
// SYNTHETIC: LLE 0x1003925a
// SYNTHETIC: LLE 0x1003931f
// SYNTHETIC: LLE 0x1003944d

// Header-inline members emitted here (markers at the end):
// SYNTHETIC: LLE 0x10038abf
// SYNTHETIC: LLE 0x10038ac8
// SYNTHETIC: LLE 0x10038acf
// SYNTHETIC: LLE 0x10038b29
// SYNTHETIC: LLE 0x10038b41
// SYNTHETIC: LLE 0x10038b45
// SYNTHETIC: LLE 0x10038b6a
// SYNTHETIC: LLE 0x10038be3
// SYNTHETIC: LLE 0x10038c2f
// SYNTHETIC: LLE 0x10038c47
// SYNTHETIC: LLE 0x10038cd6
// SYNTHETIC: LLE 0x10038de1
// SYNTHETIC: LLE 0x10038e1d
// SYNTHETIC: LLE 0x10038e27
// SYNTHETIC: LLE 0x10038e2b
// SYNTHETIC: LLE 0x10038eb2
// SYNTHETIC: LLE 0x10038faf
// SYNTHETIC: LLE 0x10038fbe
// SYNTHETIC: LLE 0x10038fd0
// SYNTHETIC: LLE 0x10039007
// SYNTHETIC: LLE 0x1003903e
// SYNTHETIC: LLE 0x10039044
// SYNTHETIC: LLE 0x10039083
// SYNTHETIC: LLE 0x10039110
// SYNTHETIC: LLE 0x10039116
// SYNTHETIC: LLE 0x1003911a
// SYNTHETIC: LLE 0x10039136
// SYNTHETIC: LLE 0x10039152
// SYNTHETIC: LLE 0x10039178
// SYNTHETIC: LLE 0x1003919e
// SYNTHETIC: LLE 0x100391c4
// SYNTHETIC: LLE 0x100391d0
// SYNTHETIC: LLE 0x100391dc
// SYNTHETIC: LLE 0x10039221
// SYNTHETIC: LLE 0x100392b5
// SYNTHETIC: LLE 0x100392bf
// SYNTHETIC: LLE 0x100392c5
// SYNTHETIC: LLE 0x100392f8
// SYNTHETIC: LLE 0x1003937a
// SYNTHETIC: LLE 0x1003938a
// SYNTHETIC: LLE 0x1003938e
// SYNTHETIC: LLE 0x1003939c
// SYNTHETIC: LLE 0x100393aa
// SYNTHETIC: LLE 0x100393dc
// SYNTHETIC: LLE 0x1003940e
