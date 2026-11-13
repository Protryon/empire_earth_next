// C:\EEX\Low-Level Engine\Input System\ISMouse.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1003b2ce..0x1003bab2: ISMouse's out-of-line members (the rest is header-inline, emitted by Input System.cpp).
// The device is the window layer's mouse (lle/ISDeviceInput.h) where the original read DirectInput 7's exclusive
// mouse: resumed, the window's pointer is in relative mode (hidden and held in the window, raw motion), as an
// exclusive DirectInput mouse was.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/ISMouse.h"
#include "lle/ISMouseAxis.h"
#include "lle/ISMouseButton.h"
#include "platform/PlatWindow.h"

// FUNCTION: LLE 0x1003b2ce
// Creates the mouse with X/Y/Z axes and four buttons, left suspended (the original created the buttons the
// DirectInput device reported; the axes' granularity was DirectInput's: 1 for X and Y, WHEEL_DELTA for the wheel)
ISMouse::ISMouse(ISDeviceInput* inDevice, PlatWindow* inWindow, const UString& inName)
    : ISDevice(inName)
{
    long theGranularity;
    this->mCurrentStatus = &this->mState;
    this->mLastStatus = &this->mLastState;
    memset(&this->mState, 0, sizeof(this->mState));
    memset(&this->mLastState, 0, sizeof(this->mLastState));
    this->mDevice = inDevice;
    this->mWindow = inWindow;
    this->mUseBufferedMovement = true;
    this->Resume();
    this->mEntities = new ISEntity*[7];
    memset(this->mEntities, 0, 7 * sizeof(ISEntity*));
    memset(this->mButtons, 0, sizeof(this->mButtons));
    memset(this->mLastButtons, 0, sizeof(this->mLastButtons));
    this->mOverflowed = false;
    theGranularity = 1;
    this->mEntities[kMouseXAxis] = new ISMouseAxis("X Axis", &this->mState.lX, theGranularity);
    this->mEntities[kMouseYAxis] = new ISMouseAxis("Y Axis", &this->mState.lY, theGranularity);
    theGranularity = 120;
    this->mEntities[kMouseZAxis] = new ISMouseAxis("Z Axis", &this->mState.lZ, theGranularity);
    this->mEntities[kMouseLeftButton] = new ISMouseButton(this, 0, "Left button");
    this->mEntities[kMouseRightButton] = new ISMouseButton(this, 1, "Right button");
    this->mEntities[kMouseMiddleButton] = new ISMouseButton(this, 2, "Middle button");
    this->mEntities[kMouseOtherButton] = new ISMouseButton(this, 3, "Other button");
    this->mEntityCount = 7;
    this->mFirstDigitalEntity = 3;
    this->Suspend();
}

// FUNCTION: LLE 0x1003b689
// Destroys the mouse and every axis and button entity it owns
ISMouse::~ISMouse()
{
    for (unsigned long i = 0; i < this->mEntityCount; i++)
        delete this->mEntities[i];
    delete[] this->mEntities;
}

// FUNCTION: LLE 0x1003b6c4
// Returns the axis or button entity at an index; kErrorNotFound for an absent one, kErrorBeyondEnd past the end
long ISMouse::GetEntity(unsigned long inIndex, ISEntity*& outEntity) const
{
    if (inIndex <= this->mEntityCount - 1) {
        outEntity = this->mEntities[inIndex];
        return outEntity == NULL ? kErrorNotFound : kErrorOK;
    }
    outEntity = NULL;
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1003b6f5
// Clears the mouse state, then reads the device's current buttons and motion directly and flushes its buffer
void ISMouse::RecomputeCurrentButtonState()
{
    unsigned long theCount = 0xffffffffu;
    this->mCurrentStatus = &this->mState;
    this->mLastStatus = &this->mLastState;
    memset(&this->mState, 0, sizeof(this->mState));
    memset(&this->mLastState, 0, sizeof(this->mLastState));
    memset(this->mButtons, 0, sizeof(this->mButtons));
    memset(this->mLastButtons, 0, sizeof(this->mLastButtons));
    if (ee_input_get_mouse_state(this->mDevice, this->mCurrentStatus) != kISInputOK)
        return;
    ee_input_get_data(this->mDevice, NULL, &theCount);
    this->mButtons[0] = (this->mCurrentStatus->rgbButtons[0] & 0x80) == 0x80;
    this->mButtons[1] = (this->mCurrentStatus->rgbButtons[1] & 0x80) == 0x80;
    this->mButtons[2] = (this->mCurrentStatus->rgbButtons[2] & 0x80) == 0x80;
    this->mButtons[3] = (this->mCurrentStatus->rgbButtons[3] & 0x80) == 0x80;
    if (!this->mUseBufferedMovement) {
        this->mCurrentStatus->lX = 0;
        this->mCurrentStatus->lY = 0;
        this->mCurrentStatus->lZ = 0;
    }
    memcpy(this->mLastButtons, this->mButtons, sizeof(this->mButtons));
    memcpy(this->mLastStatus, this->mCurrentStatus, sizeof(ISMouseState));
}

// FUNCTION: LLE 0x1003b7cc
// Reads the buffered mouse events: position, wheel and button press/release counts, then updates double presses
void ISMouse::Refresh()
{
    ISInputRecord theData[256];
    unsigned long theCount = 256;
    memcpy(this->mLastStatus, this->mCurrentStatus, sizeof(ISMouseState));
    memcpy(this->mLastButtons, this->mButtons, sizeof(this->mButtons));
    long theResult = ee_input_get_data(this->mDevice, theData, &theCount);
    this->mCurrentStatus->lX = 0;
    this->mCurrentStatus->lY = 0;
    this->mCurrentStatus->lZ = 0;
    this->mOverflowed = false;
    if (theResult != kISInputNotAcquired && theCount > 0) {
        this->mOverflowed = true;
        if (theResult == kISInputOverflow)
            this->RecomputeCurrentButtonState();
        else {
            ISMouseState theState;
            if (ee_input_get_mouse_state(this->mDevice, &theState) == kISInputOK) {
                this->mCurrentStatus->lX = theState.lX;
                this->mCurrentStatus->lY = theState.lY;
            }
            for (unsigned long i = 0; i < theCount; i++) {
                unsigned long theValue = theData[i].dwData;
                switch (theData[i].dwOfs) {
                case kISMouseOffsetButton3:
                    this->mCurrentStatus->rgbButtons[3] = (unsigned char)theValue;
                    if (((theValue & 0x80) == 0x80 && !(this->mButtons[3] & 1)) || (theValue == 0 && (this->mButtons[3] & 1)))
                        this->mButtons[3]++;
                    break;
                case kISMouseOffsetButton2:
                    this->mCurrentStatus->rgbButtons[2] = (unsigned char)theValue;
                    if (((theValue & 0x80) == 0x80 && !(this->mButtons[2] & 1)) || (theValue == 0 && (this->mButtons[2] & 1)))
                        this->mButtons[2]++;
                    break;
                case kISMouseOffsetButton1:
                    this->mCurrentStatus->rgbButtons[1] = (unsigned char)theValue;
                    if (((theValue & 0x80) == 0x80 && !(this->mButtons[1] & 1)) || (theValue == 0 && (this->mButtons[1] & 1)))
                        this->mButtons[1]++;
                    break;
                case kISMouseOffsetButton0:
                    this->mCurrentStatus->rgbButtons[0] = (unsigned char)theValue;
                    if (((theValue & 0x80) == 0x80 && !(this->mButtons[0] & 1)) || (theValue == 0 && (this->mButtons[0] & 1)))
                        this->mButtons[0]++;
                    break;
                case kISMouseOffsetZ:
                    this->mCurrentStatus->lZ += (long)theValue;
                    break;
                }
            }
            if (!this->mUseBufferedMovement) {
                this->mCurrentStatus->lX = 0;
                this->mCurrentStatus->lY = 0;
                this->mCurrentStatus->lZ = 0;
            }
        }
    }
    for (int j = 0; j < 7; j++)
        if (this->mEntities[j])
            this->mEntities[j]->CheckForDoublePress();
}

// FUNCTION: LLE 0x1003b9ba
// Reads the current cursor movement from the device (zeroed when buffered movement is off)
void ISMouse::RefreshXY()
{
    ISMouseState theState;
    if (ee_input_get_mouse_state(this->mDevice, &theState) == kISInputOK) {
        this->mCurrentStatus->lX = theState.lX;
        this->mCurrentStatus->lY = theState.lY;
    }
    if (!this->mUseBufferedMovement) {
        this->mCurrentStatus->lX = 0;
        this->mCurrentStatus->lY = 0;
    }
}

// FUNCTION: LLE 0x1003b9fc
// Takes the mouse for the game (the pointer in relative mode: DirectInput's exclusive foreground mouse) and
// re-reads its state
void ISMouse::Resume()
{
    if (this->mWindow != NULL)
        PlatMouseSetRelative(this->mWindow, 1);
    ee_input_acquire(this->mDevice);
    this->mSuspended = false;
    this->RecomputeCurrentButtonState();
}

// FUNCTION: LLE 0x1003ba39
// Sets the mouse axis mode (0 relative, 1 absolute); kErrorFailed for any other mode. The axes stay relative (the
// game only ever asks for relative axes)
long ISMouse::SetMode(unsigned long inMode)
{
    long theResult = kErrorOK;
    if (inMode != 0 && inMode != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x1003ba87
// Releases the mouse (its events are no longer buffered; the pointer leaves relative mode) and marks it suspended
void ISMouse::Suspend()
{
    ee_input_unacquire(this->mDevice);
    this->mSuspended = true;
    if (this->mWindow != NULL)
        PlatMouseSetRelative(this->mWindow, 0);
}
