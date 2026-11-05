// C:\EEX\Low-Level Engine\User Interface\UIHotKey.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10076f27..0x100774b9: the sPressedPoll/sHotKey init chains, then the out-of-line members (CreateHotKeys,
// DeleteHotKeys, the private CalculateKeyCount, GetHotKeyName, the Set*Event setters, SetModifier, FireEvents) and the
// vector-delete helper COMDAT last. The inline members are emitted by UIEMouseWheelScroll.cpp (0x1006e075..).
// UIIMECandidate.cpp's key chains start at 0x100774b9.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/ISKey.h"
#include "lle/ISKeyboard.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"

// One hot key record of the UI database (0x7c bytes) (provisional names)
struct UIDbDataHotKeys {
    uint32_t mUnused;                       // +0x00
    int32_t mID;                            // +0x04 negative: no hot key
    char mLabel[0x64];                      // +0x08
    uint32_t mModifiers[3];                 // +0x6c DIK_* codes, 0x7fff: none
    uint32_t mKey;                          // +0x78
};
static_assert(sizeof(UIDbDataHotKeys) == 124, "on-disk record: 124 bytes on every target");

// Deletes every element of a vector of pointers, then clears it; returns the element count (provisional name)
template <class T> unsigned long UIDeleteAll(std::vector<T>& ioVector)
{
    unsigned long theCount = 0;
    for (std::vector<T>::iterator i = ioVector.begin(); i != ioVector.end(); i++) {
        if (*i)
            delete *i;
        theCount++;
    }
    ioVector.clear();
    return theCount;
}

TMPoll UIHotKey::sPressedPoll(50, 0, true);
std::vector<UIHotKey*> UIHotKey::sHotKey;
unsigned long UIHotKey::sPressedPollAmount;

// FUNCTION: LLE 0x10076fa8
// Reads every hot key record from the UI database file into the shared table (NULL for unused records)
long UIHotKey::CreateHotKeys(FSFileSpec& inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult;
    inFile.Open(kFileAccessRead);
    theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            UIDbDataHotKeys theData;
            UIHotKey* theHotKey;
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theData), sizeof(theData), theRead);
            if (theData.mID >= 0) {
                theHotKey = new UIHotKey;
                if (theData.mModifiers[0] != kKeyNone) {
                    gISKeyboard->GetEntity(theData.mModifiers[0], theHotKey->mModifiers[0]);
                    theHotKey->mModifierState |= ISKeyboard::ComputeModifierState(theData.mModifiers[0]);
                } else
                    theHotKey->mModifiers[0] = NULL;
                if (theData.mModifiers[1] != kKeyNone) {
                    gISKeyboard->GetEntity(theData.mModifiers[1], theHotKey->mModifiers[1]);
                    theHotKey->mModifierState |= ISKeyboard::ComputeModifierState(theData.mModifiers[1]);
                } else
                    theHotKey->mModifiers[1] = NULL;
                if (theData.mModifiers[2] != kKeyNone) {
                    gISKeyboard->GetEntity(theData.mModifiers[2], theHotKey->mModifiers[2]);
                    theHotKey->mModifierState |= ISKeyboard::ComputeModifierState(theData.mModifiers[2]);
                } else
                    theHotKey->mModifiers[2] = NULL;
                theResult = gISKeyboard->GetEntity(theData.mKey, theHotKey->mKey);
                theHotKey->CalculateKeyCount();
            } else
                theHotKey = NULL;
            UIHotKey::sHotKey.push_back(theHotKey);
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: LLE 0x1007710d
// Deletes every hot key in the shared table
void UIHotKey::DeleteHotKeys()
{
    UIDeleteAll(UIHotKey::sHotKey);
}

// FUNCTION: LLE 0x10077119
// Counts the key and modifiers the hot key uses
void UIHotKey::CalculateKeyCount()
{
    this->mKeyCount = 0;
    if (this->mKey)
        this->mKeyCount = 1;
    for (long i = 0; i < 3; i++) {
        if (this->mModifiers[i])
            this->mKeyCount++;
    }
}

// FUNCTION: LLE 0x1007713f
// Builds the hot key's display name, e.g. "Ctrl + Shift + A" (key names without " key")
void UIHotKey::GetHotKeyName(UWideString& outName)
{
    UString theModifierNames[3];
    UString theKeyName;
    UString theName;
    for (long i = 0; i < 3; i++) {
        if (this->mModifiers[i]) {
            theModifierNames[i] = this->mModifiers[i]->GetName();
            theModifierNames[i] = theModifierNames[i].GetSubstring(0, theModifierNames[i].Find(" key", 0));
            theModifierNames[i] += " + ";
        } else
            theModifierNames[i] = "";
    }
    theKeyName = this->mKey->GetName();
    theKeyName = theKeyName.GetSubstring(0, theKeyName.Find(" key", 0));
    theName = theModifierNames[0] + theModifierNames[1] + theModifierNames[2] + theKeyName;
    outName = theName;
}

// FUNCTION: LLE 0x100772bf
// Replaces the double-press event, releasing the old one and initializing the new one
void UIHotKey::SetDoublePressedEvent(UIEvent* inEvent)
{
    if (this->mDoublePressedEvent) {
        this->mDoublePressedEvent->Uninitialize();
        this->mDoublePressedEvent->Dereference();
    }
    this->mDoublePressedEvent = inEvent;
    if (this->mDoublePressedEvent) {
        this->mDoublePressedEvent->Reference();
        this->mDoublePressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x100772f0
// Replaces the just-pressed event, releasing the old one and initializing the new one
void UIHotKey::SetJustPressedEvent(UIEvent* inEvent)
{
    if (this->mJustPressedEvent) {
        this->mJustPressedEvent->Uninitialize();
        this->mJustPressedEvent->Dereference();
    }
    this->mJustPressedEvent = inEvent;
    if (this->mJustPressedEvent) {
        this->mJustPressedEvent->Reference();
        this->mJustPressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10077321
// Replaces the just-released event, releasing the old one and initializing the new one
void UIHotKey::SetJustUnpressedEvent(UIEvent* inEvent)
{
    if (this->mJustUnpressedEvent) {
        this->mJustUnpressedEvent->Uninitialize();
        this->mJustUnpressedEvent->Dereference();
    }
    this->mJustUnpressedEvent = inEvent;
    if (this->mJustUnpressedEvent) {
        this->mJustUnpressedEvent->Reference();
        this->mJustUnpressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10077352
// Replaces the held-down event, releasing the old one and initializing the new one
void UIHotKey::SetPressedEvent(UIEvent* inEvent)
{
    if (this->mPressedEvent) {
        this->mPressedEvent->Uninitialize();
        this->mPressedEvent->Dereference();
    }
    this->mPressedEvent = inEvent;
    if (this->mPressedEvent) {
        this->mPressedEvent->Reference();
        this->mPressedEvent->Initialize();
    }
}

// FUNCTION: LLE 0x10077383
// Sets one modifier slot to a key and adds it to the required modifier state
void UIHotKey::SetModifier(unsigned long inKey, unsigned char inIndex)
{
    this->mModifierState |= ISKeyboard::ComputeModifierState(inKey);
    gISKeyboard->GetEntity(inKey, this->mModifiers[inIndex]);
    this->CalculateKeyCount();
}

// FUNCTION: LLE 0x100773b6
// Fires the first matching event (double press, press, held, release) when enabled and the modifiers are down;
// true if one fired
bool UIHotKey::FireEvents()
{
    bool theFired = false;
    if (this->mEnabled && (!this->mModifiers[0] || this->mModifiers[0]->GetValue() != 0)
        && (!this->mModifiers[1] || this->mModifiers[1]->GetValue() != 0) && (!this->mModifiers[2] || this->mModifiers[2]->GetValue() != 0)) {
        unsigned long theState = gISKeyboard->GetModifierState();
        if (theState == this->mModifierState || theState == ISKeyboard::ComputeModifierState(static_cast<ISKey*>(this->mKey)->GetIndex())) {
            if (this->mDoublePressedEvent && this->mKey->JustDoublePressed()) {
                this->mDoublePressedEvent->ProcessEvent(this->mForm, this);
                theFired = true;
            } else if (this->mJustPressedEvent && this->mKey->JustPressed()) {
                this->mJustPressedEvent->ProcessEvent(this->mForm, this);
                theFired = true;
            } else if (this->mPressedEvent && this->mKey->GetValue() > 0) {
                this->mPressedEvent->ProcessEvent(this->mForm, this);
                theFired = true;
            } else if (this->mJustUnpressedEvent && this->mKey->JustUnpressed()) {
                this->mJustUnpressedEvent->ProcessEvent(this->mForm, this);
                theFired = true;
            }
        }
    }
    return theFired;
}

// Compiler-generated: the static init chains, and the vector-delete COMDAT (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10076f27
// SYNTHETIC: LLE 0x10076f31
// SYNTHETIC: LLE 0x10076f42
// SYNTHETIC: LLE 0x10076f4e
// SYNTHETIC: LLE 0x10076f5f
// SYNTHETIC: LLE 0x10076f69
// SYNTHETIC: LLE 0x10076f81
// SYNTHETIC: LLE 0x10076f8d
// SYNTHETIC: LLE 0x10077489
