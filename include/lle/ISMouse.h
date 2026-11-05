// C:\EEX\Low-Level Engine\Input System\ISMouse.h (RECONSTRUCTION; ISMouse.cpp 0x3b2ce..0x3bab2). The system mouse
// (DirectInput's exclusive mouse in the original; the window layer's events in relative mode now: lle/ISDeviceInput.h). sizeof 0x60 (operator new in ISInitialize; ??_E stride 0x38f53). Inline members and the implicit copy
// members are emitted by Input System.cpp (0x38de1..0x38faf).
#ifndef LLE_ISMOUSE_H
#define LLE_ISMOUSE_H

#include "lle/ISDeviceInput.h"

struct PlatWindow;
#include "lle/ISDevice.h"

class LLE_API ISMouse : public ISDevice {
    friend class ISMouseButton;
    friend void ISShutdown(bool inForced);
    friend LLE_API long ISInitialize(HINSTANCE inInstance, class GEWindow* inWindow, class UResourceFile* inResources,
                             unsigned long inArg4, unsigned long inDoublePressTimeDelta);
public:
    virtual long GetEntity(unsigned long inIndex, ISEntity*& outEntity) const;
    virtual unsigned long GetEntityCount() const { return this->mEntityCount; }
    virtual unsigned long GetFirstDigitalEntity() const { return this->mFirstDigitalEntity; }
    bool GetShouldUpdate()
    {
        return this->mOverflowed || this->mButtons[0] != this->mLastButtons[0] || this->mButtons[1] != this->mLastButtons[1] ||
               this->mButtons[2] != this->mLastButtons[2] || this->mButtons[3] != this->mLastButtons[3] || this->mCurrentStatus->lX != 0 ||
               this->mCurrentStatus->lY != 0 || this->mCurrentStatus->lZ != 0;
    }
    virtual void Refresh();
    void RefreshXY();
    virtual void Resume();
    virtual long SetMode(unsigned long inMode);
    void SetUseBufferedMovement(bool inUse) { this->mUseBufferedMovement = inUse; }
    virtual void Suspend();
private:
    ISMouse(ISDeviceInput* inDevice, PlatWindow* inWindow, const UString& inName);
    virtual ~ISMouse();
    ISMouseState* GetCurrentStatus() { return this->mCurrentStatus; }
    ISMouseState* GetLastStatus() { return this->mLastStatus; }
    void RecomputeCurrentButtonState();

    unsigned char mButtons[4];              // +0x18 per button: transition count, bit 0 = down
    unsigned char mLastButtons[4];          // +0x1c
    ISMouseState mState;                    // +0x20 (DIMOUSESTATE)
    ISMouseState mLastState;                // +0x30
    ISEntity** mEntities;                   // +0x40 new[7]: X, Y, Z axes, 4 buttons
    ISDeviceInput* mDevice;                 // +0x44 (an IDirectInputDevice in the original)
    ISMouseState* mCurrentStatus;           // +0x48 -> mState
    ISMouseState* mLastStatus;              // +0x4c -> mLastState
    PlatWindow* mWindow;                    // +0x50 (relative mode while resumed; an HWND in the original)
    unsigned long mEntityCount;             // +0x54 7
    unsigned long mFirstDigitalEntity;      // +0x58 3
    bool mOverflowed;                       // +0x5c
    bool mUseBufferedMovement;              // +0x5d
};

// ISMouse entity index (GetEntity / mEntities): the ISMouse ctor creates each with the name quoted here
enum ISMouseEntity {
    kMouseXAxis = 0,       // ISMouse::ISMouse: "X Axis" (DIMOFS_X)
    kMouseYAxis = 1,       // ISMouse::ISMouse: "Y Axis" (DIMOFS_Y)
    kMouseZAxis = 2,       // ISMouse::ISMouse: "Z Axis" (DIMOFS_Z, the wheel)
    kMouseLeftButton = 3,  // ISMouse::ISMouse: "Left button"
    kMouseRightButton = 4,  // ISMouse::ISMouse: "Right button"
    kMouseMiddleButton = 5,  // ISMouse::ISMouse: "Middle button"
    kMouseOtherButton = 6,  // ISMouse::ISMouse: "Other button"
};

#endif
