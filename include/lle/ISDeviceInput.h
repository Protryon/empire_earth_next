// The Input System's devices on the window layer's events (platform/PlatWindow.h; PORT.md step 4.4): what the
// keyboard and the mouse read from DirectInput 7 before, with its semantics. The window thread hands every key and
// mouse event to ISHandleEvent (Input System.cpp), which fills one ISDeviceInput per device; the game threads read
// them as they read DirectInput:
// - a buffer of up to 256 records (DirectInput's DIPROP_BUFFERSIZE 256) taken while the device is acquired, read
//   oldest first; records that did not fit are reported once as DI_BUFFEROVERFLOW was;
// - the device state: the keyboard's 256 key bytes (0x80 held, by DIK_* scan code), the mouse's motion since the last
//   state read (relative axes: X, Y and the wheel Z in WHEEL_DELTA units) and its four buttons.
// Records: dwOfs is the DIK_* code (keyboard) or the DIMOFS_* offset (mouse), dwData 0x80 / 0 for a press / release,
// the motion for an axis.
#ifndef LLE_ISDEVICEINPUT_H
#define LLE_ISDEVICEINPUT_H

#include <deque>

#include "platform/PlatThread.h"

// the engine's mouse state (DirectInput 7's DIMOUSESTATE: the axes are `long`, ISMouseAxis points at them)
struct ISMouseState {
    long lX;
    long lY;
    long lZ;
    unsigned char rgbButtons[4];
};

// a buffered record (DIDEVICEOBJECTDATA's fields the engine reads)
struct ISInputRecord {
    unsigned long dwOfs;
    unsigned long dwData;
};

// DirectInput's mouse data offsets (the DIMOUSESTATE field offsets)
enum {
    kISMouseOffsetX = 0,
    kISMouseOffsetY = 4,
    kISMouseOffsetZ = 8,
    kISMouseOffsetButton0 = 12,             // left
    kISMouseOffsetButton1 = 13,             // right
    kISMouseOffsetButton2 = 14,             // middle
    kISMouseOffsetButton3 = 15              // the first side button
};

// GetData's results (DI_OK, DI_BUFFEROVERFLOW, DIERR_NOTACQUIRED)
enum { kISInputOK = 0, kISInputOverflow = 1, kISInputNotAcquired = -1 };

class LLE_API ISDeviceInput {
public:
    ISDeviceInput();
    ~ISDeviceInput();
    void Acquire();
    void Unacquire();
    bool IsAcquired();
    // up to ioCount records, oldest first, into outRecords (NULL: discards them all) -> kISInput*; ioCount: how many
    long GetData(ISInputRecord* outRecords, unsigned long& ioCount);
    // the keyboard's 256 key bytes -> kISInputOK / kISInputNotAcquired
    long GetKeyState(unsigned char* outKeys);
    // the mouse's motion since the last call and its buttons -> kISInputOK / kISInputNotAcquired
    long GetMouseState(ISMouseState& outState);

    // the window thread's events (ISHandleEvent)
    void Key(unsigned long inScanCode, bool inDown);
    void MouseMove(long inDeltaX, long inDeltaY);
    void MouseWheel(long inDelta);
    void MouseButton(unsigned long inButton, bool inDown);
private:
    void Push(unsigned long inOffset, unsigned long inData);

    PlatMutex* mLock;
    bool mAcquired;
    bool mLost;                             // records were dropped since the last GetData (the buffer was full)
    std::deque<ISInputRecord> mRecords;
    unsigned char mKeys[256];
    long mMotion[3];
    unsigned char mButtons[4];
};

struct PlatInputEvent;
// Feeds a window event to the system keyboard and mouse (the window thread calls it for every event)
LLE_API void ISHandleEvent(const PlatInputEvent& inEvent);

#endif
