// The original ISDeviceInput and ISHandleEvent (src/Low-Level Engine/Input System/Input System.cpp and
// include/lle/ISDeviceInput.h at 5d3ce6a9, before the class moved to rust/crates/ee_input), unchanged, for the golden
// driver is_device_golden.cpp. Needs <deque>, <string.h>, <math.h>, platform/PlatWindow.h and the PlatMutex calls.
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

static ISDeviceInput* sKeyboardInput;
static ISDeviceInput* sMouseInput;

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
