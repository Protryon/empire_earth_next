// C:\EEX\Low-Level Engine\Input System\ISKeyboard.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10039ca1..0x1003b2ce: ISKeyboard's out-of-line members (the rest is header-inline, emitted by
// Input System.cpp). The ctor's key table names every key by its DIK_* name ("ESCAPE key", ...).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/ISKeyboard.h"
#include "lle/ISExtendedKey.h"
#include "lle/UResourceFile.h"

// FUNCTION: LLE 0x10039ca1
// Creates the keyboard device: one key per scan code, named from the resources or the built-in English table,
// plus combined Ctrl/Shift/Alt keys; leaves the device suspended
ISKeyboard::ISKeyboard(IDirectInputDevice* inDevice, const UString& inName, UResourceFile* inResources,
                       unsigned long inFirstStringID)
    : ISDevice(inName)
{
    UWideString theName;
    unsigned long theKeys[] = {
        DIK_ESCAPE, DIK_1, DIK_2, DIK_3, DIK_4, DIK_5, DIK_6, DIK_7, DIK_8, DIK_9, DIK_0, DIK_MINUS, DIK_EQUALS,
        DIK_BACK, DIK_TAB, DIK_Q, DIK_W, DIK_E, DIK_R, DIK_T, DIK_Y, DIK_U, DIK_I, DIK_O, DIK_P, DIK_LBRACKET,
        DIK_RBRACKET, DIK_RETURN, DIK_LCONTROL, DIK_A, DIK_S, DIK_D, DIK_F, DIK_G, DIK_H, DIK_J, DIK_K, DIK_L, DIK_SEMICOLON,
        DIK_APOSTROPHE, DIK_GRAVE, DIK_LSHIFT, DIK_BACKSLASH, DIK_Z, DIK_X, DIK_C, DIK_V, DIK_B, DIK_N, DIK_M, DIK_COMMA,
        DIK_PERIOD, DIK_SLASH, DIK_RSHIFT, DIK_MULTIPLY, DIK_LMENU, DIK_SPACE, DIK_CAPITAL, DIK_F1, DIK_F2, DIK_F3, DIK_F4,
        DIK_F5, DIK_F6, DIK_F7, DIK_F8, DIK_F9, DIK_F10, DIK_NUMLOCK, DIK_SCROLL, DIK_NUMPAD7, DIK_NUMPAD8,
        DIK_NUMPAD9, DIK_SUBTRACT, DIK_NUMPAD4, DIK_NUMPAD5, DIK_NUMPAD6, DIK_ADD, DIK_NUMPAD1, DIK_NUMPAD2,
        DIK_NUMPAD3, DIK_NUMPAD0, DIK_DECIMAL, DIK_F11, DIK_F12, DIK_F13, DIK_F14, DIK_F15, DIK_KANA, DIK_CONVERT,
        DIK_NOCONVERT, DIK_YEN, DIK_NUMPADEQUALS, DIK_CIRCUMFLEX, DIK_AT, DIK_COLON, DIK_UNDERLINE, DIK_KANJI,
        DIK_STOP, DIK_AX, DIK_UNLABELED, DIK_NUMPADENTER, DIK_RCONTROL, DIK_NUMPADCOMMA, DIK_DIVIDE, DIK_SYSRQ, DIK_RMENU, 0xc5,
        DIK_HOME, DIK_UP, DIK_PRIOR, DIK_LEFT, DIK_RIGHT, DIK_END, DIK_DOWN, DIK_NEXT, DIK_INSERT, DIK_DELETE,
        DIK_LWIN, DIK_RWIN, DIK_APPS
    };
    UWideString theNames[] = {
        "ESCAPE key", "1 key", "2 key", "3 key", "4 key", "5 key", "6 key", "7 key", "8 key", "9 key", "0 key",
        "MINUS key", "EQUALS key", "BACK key", "TAB key", "Q key", "W key", "E key", "R key", "T key", "Y key",
        "U key", "I key", "O key", "P key", "LBRACKET key", "RBRACKET key", "RETURN key", "CONTROL key (left)",
        "A key", "S key", "D key", "F key", "G key", "H key", "J key", "K key", "L key", "SEMICOLON key",
        "APOSTROPHE key", "GRAVE key", "SHIFT key (left)", "BACKSLASH key", "Z key", "X key", "C key", "V key",
        "B key", "N key", "M key", "COMMA key", "PERIOD key", "SLASH key", "SHIFT key (right)", "MULTIPLY key",
        "ALT key (left)", "SPACE key", "CAPITAL key", "F1 key", "F2 key", "F3 key", "F4 key", "F5 key", "F6 key",
        "F7 key", "F8 key", "F9 key", "F10 key", "NUMLOCK key", "SCROLL key", "NUMPAD7 key", "NUMPAD8 key",
        "NUMPAD9 key", "SUBTRACT key", "NUMPAD4 key", "NUMPAD5 key", "NUMPAD6 key", "ADD key", "NUMPAD1 key",
        "NUMPAD2 key", "NUMPAD3 key", "NUMPAD0 key", "DECIMAL key", "F11 key", "F12 key", "F13 key", "F14 key",
        "F15 key", "KANA key", "CONVERT key", "NOCONVERT key", "YEN key", "NUMPADEQUALS key", "CIRCUMFLEX key",
        "AT key", "COLON key", "UNDERLINE key", "KANJI key", "STOP key", "AX key", "UNLABELED key",
        "NUMPADENTER key", "CONTROL key (right)", "NUMPADCOMMA key", "DIVIDE key", "SYSRQ key", "ALT key (right)",
        "Pause key", "HOME key", "UP key", "PRIOR key", "LEFT key", "RIGHT key", "END key", "DOWN key", "NEXT key",
        "INSERT key", "DELETE key", "LWIN key", "RWIN key", "APPS key"
    };
    unsigned long i;
    this->mDevice = inDevice;
    this->mKeyCount = 256;
    this->mModifierState = 0;
    this->mNumberOfKeysDown = 0;
    this->mLastNumberOfKeysDown = 0;
    this->mCurrentStatus = this->mStatus;
    this->mLastStatus = this->mLast;
    memset(this->mStatus, 0, 256);
    memset(this->mLast, 0, 256);
    this->mKeys = new ISEntity*[256];
    memset(this->mKeys, 0, 256 * sizeof(ISEntity*));
    if (inResources) {
        for (i = 0; i < sizeof(theKeys) / sizeof(theKeys[0]); i++) {
            inResources->GetString(i + inFirstStringID, theName);
            this->mKeys[theKeys[i]] = new ISKey(this, theKeys[i], theName);
        }
    } else {
        for (i = 0; i < sizeof(theKeys) / sizeof(theKeys[0]); i++)
            this->mKeys[theKeys[i]] = new ISKey(this, theKeys[i], theNames[i]);
    }
    this->Resume();
    for (i = 0; i < 256; i++) {
        this->mStatus[i] = 0;
        this->mLast[i] = 0;
    }
    this->mKeys[0xe0] = new ISExtendedKey(dynamic_cast<ISKey*>(this->mKeys[DIK_LCONTROL]), dynamic_cast<ISKey*>(this->mKeys[DIK_RCONTROL]),
                                    0xe0);
    this->mKeys[0xe1] = new ISExtendedKey(dynamic_cast<ISKey*>(this->mKeys[DIK_LSHIFT]), dynamic_cast<ISKey*>(this->mKeys[DIK_RSHIFT]),
                                    0xe1);
    this->mKeys[0xe2] = new ISExtendedKey(dynamic_cast<ISKey*>(this->mKeys[DIK_LMENU]), dynamic_cast<ISKey*>(this->mKeys[DIK_RMENU]),
                                    0xe2);
    this->mFirstDigitalEntity = 0;
    this->mOverflowed = false;
    this->Suspend();
}

// FUNCTION: LLE 0x1003b035
// Destroys the keyboard and every key it created
ISKeyboard::~ISKeyboard()
{
    for (unsigned long i = 0; i < this->mKeyCount; i++)
        delete this->mKeys[i];
    delete[] this->mKeys;
}

// FUNCTION: LLE 0x1003b070
// Returns the key for a scan code (NULL and a beyond-end error past the table)
long ISKeyboard::GetEntity(unsigned long inIndex, ISEntity*& outEntity) const
{
    if (inIndex <= this->mKeyCount - 1) {
        outEntity = this->mKeys[inIndex];
        return kErrorOK;
    }
    outEntity = NULL;
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1003b099
// Reads the buffered DirectInput key events: updates key states, the pressed count, modifiers and double presses;
// rereads the whole state after a buffer overflow
void ISKeyboard::Refresh()
{
    DIDEVICEOBJECTDATA theData[256];
    DWORD theCount = 256;
    // Fixed: the event counter was an unsigned char, so exactly 256 buffered events looped forever
    unsigned long i;
    this->mLastNumberOfKeysDown = this->mNumberOfKeysDown;
    memcpy(this->mLast, this->mStatus, this->mKeyCount);
    if (this->mOverflowed) {
        for (unsigned long j = 0; j < this->mKeyCount; j++)
            if (this->mKeys[j] != NULL && static_cast<ISKey*>(this->mKeys[j])->mDoublePressed) {
                static_cast<ISKey*>(this->mKeys[j])->mDoublePressed = false;
                static_cast<ISKey*>(this->mKeys[j])->mLastPressTime = 0;
            }
    }
    this->mOverflowed = false;
    HRESULT theResult = this->mDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), theData, &theCount, 0);
    if (!(theResult >= DI_OK))
        return;
    if (theResult == DI_BUFFEROVERFLOW) {
        this->mOverflowed = true;
        this->RecomputeCurrentKeyState();
    } else if (theCount > 0) {
        this->mOverflowed = true;
        for (i = 0; i < theCount; i++) {
            unsigned long theModifier = ISKeyboard::ComputeModifierState(theData[i].dwOfs);
            if (theData[i].dwData & 0x80) {
                if (!(this->mStatus[theData[i].dwOfs] & 1)) {
                    this->mNumberOfKeysDown++;
                    this->mStatus[theData[i].dwOfs]++;
                }
                if (theData[i].dwOfs < 0xdd)
                    this->mModifierState |= theModifier;
            } else {
                if (this->mStatus[theData[i].dwOfs] & 1) {
                    this->mNumberOfKeysDown--;
                    this->mStatus[theData[i].dwOfs]++;
                }
                if (theData[i].dwOfs < 0xdd)
                    this->mModifierState ^= theModifier;
            }
            if (this->mKeys[theData[i].dwOfs])
                this->mKeys[theData[i].dwOfs]->CheckForDoublePress();
        }
    }
}

// FUNCTION: LLE 0x1003b1d8
// Rereads the whole keyboard state from DirectInput, discarding buffered events and resetting the modifiers
void ISKeyboard::RecomputeCurrentKeyState()
{
    unsigned char theState[256];
    DWORD theCount = INFINITE;
    memset(theState, 0, 256);
    memset(this->mStatus, 0, 256);
    memset(this->mLast, 0, 256);
    this->mModifierState = 0;
    if (!(this->mDevice->GetDeviceState(256, theState) >= DI_OK))
        return;
    this->mDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), NULL, &theCount, 0);
    this->mNumberOfKeysDown = 0;
    for (int i = 0; i < this->mKeyCount; i++) {
        if ((theState[i] & 0x80) == 0x80) {
            unsigned long theModifier = ISKeyboard::ComputeModifierState(i);
            this->mStatus[i] = 1;
            this->mNumberOfKeysDown++;
            if (i < 0xdd)
                this->mModifierState |= theModifier;
        } else
            this->mStatus[i] = 0;
        this->mLast[i] = this->mStatus[i];
        if (this->mKeys[i])
            this->mKeys[i]->CheckForDoublePress();
    }
}

// FUNCTION: LLE 0x1003b2a0
// Reacquires the keyboard and rereads its state
void ISKeyboard::Resume()
{
    if (this->mDevice->Acquire() == DI_OK) {
        this->mSuspended = false;
        this->RecomputeCurrentKeyState();
    }
}

// FUNCTION: LLE 0x1003b2bc
// Releases the keyboard device and marks it suspended
void ISKeyboard::Suspend()
{
    this->mDevice->Unacquire();
    this->mSuspended = true;
}
