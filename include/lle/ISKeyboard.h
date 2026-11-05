// C:\EEX\Low-Level Engine\Input System\ISKeyboard.h (RECONSTRUCTION; ISKeyboard.cpp 0x39ca1..0x3b2ce). The system
// keyboard (DirectInput's in the original; the window layer's events now: lle/ISDeviceInput.h). sizeof 0x240 (operator new in ISInitialize; ??_E stride 0x38d82). Inline members and the implicit
// copy members are emitted by Input System.cpp (0x38be3..0x38de1).
#ifndef LLE_ISKEYBOARD_H
#define LLE_ISKEYBOARD_H

#include "platform/PlatKeys.h"              // DIK_* scan codes
#include "lle/ISDeviceInput.h"
#include "lle/ISDevice.h"
#include "lle/ISKeyCode.h"
#include "lle/ISModifierState.h"

class ISKey;
class UResourceFile;

class LLE_API ISKeyboard : public ISDevice {
    friend class ISKey;
    friend void ISShutdown(bool inForced);
    friend LLE_API long ISInitialize(HINSTANCE inInstance, class GEWindow* inWindow, UResourceFile* inResources,
                             unsigned long inArg4, unsigned long inDoublePressTimeDelta);
public:
    // modifier bit (1 Shift, 2 Control, 4 Alt) of a DIK_* code; 0xe0..0xe2 are the extended (either-side) keys
    static unsigned long ComputeModifierState(unsigned long inKey)
    {
        unsigned long theState = 0;
        if (inKey == DIK_LSHIFT || inKey == DIK_RSHIFT || inKey == 0xe1)
            theState = 1;
        if (inKey == DIK_LMENU || inKey == DIK_RMENU || inKey == 0xe2)
            theState |= kModifierAlt;
        if (inKey == DIK_LCONTROL || inKey == DIK_RCONTROL || inKey == 0xe0)
            theState |= kModifierControl;
        return theState;
    }
    virtual long GetEntity(unsigned long inIndex, ISEntity*& outEntity) const;
    virtual unsigned long GetEntityCount() const { return this->mKeyCount; }
    virtual unsigned long GetFirstDigitalEntity() const { return this->mFirstDigitalEntity; }
    unsigned long GetModifierState() { return this->mModifierState; }
    long GetNumberOfKeysDown() { return this->mNumberOfKeysDown; }
    bool GetShouldUpdate() { return this->mOverflowed || this->GetNumberOfKeysDown() > 0 || this->mLastNumberOfKeysDown > 0; }
    virtual void Refresh();
    virtual void Resume();
    virtual long SetMode(unsigned long inMode) { return kErrorFailed; }
    virtual void Suspend();
private:
    ISKeyboard(ISDeviceInput* inDevice, const UString& inName, UResourceFile* inResources,
               unsigned long inArg4);
    virtual ~ISKeyboard();
    unsigned char* GetCurrentStatus() const { return this->mCurrentStatus; }
    unsigned char* GetLastStatus() const { return this->mLastStatus; }
    void RecomputeCurrentKeyState();

    ISDeviceInput* mDevice;                 // +0x18 (an IDirectInputDevice in the original)
    ISEntity** mKeys;                       // +0x1c new[256] by DIK_* code (ISKey; 0xe0..0xe2 ISExtendedKey)
    unsigned char* mCurrentStatus;          // +0x20 -> mStatus
    unsigned char* mLastStatus;             // +0x24 -> mLast
    unsigned long mModifierState;           // +0x28
    unsigned long mKeyCount;                // +0x2c
    unsigned long mFirstDigitalEntity;      // +0x30
    long mNumberOfKeysDown;                 // +0x34
    long mLastNumberOfKeysDown;             // +0x38
    unsigned char mStatus[256];             // +0x3c per key: transition count, bit 0 = down
    unsigned char mLast[256];               // +0x13c
    bool mOverflowed;                       // +0x23c DI_BUFFEROVERFLOW on the last read
};

#endif
