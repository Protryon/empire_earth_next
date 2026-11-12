// C:\EEX\Low-Level Engine\User Interface\UIHotKey.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10076f27..0x100774b9: the sPressedPoll/sHotKey init chains, then the out-of-line members (CreateHotKeys,
// DeleteHotKeys, the private CalculateKeyCount, GetHotKeyName, the Set*Event setters, SetModifier, FireEvents) and the
// vector-delete helper COMDAT last. The inline members are emitted by UIEMouseWheelScroll.cpp (0x1006e075..).
// UIIMECandidate.cpp's key chains start at 0x100774b9.
#include <windows.h>
#include <string.h>

#include "rust/ee_ui.h"
#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"
#include "lle/Input System.h"
#include "lle/ISKey.h"
#include "lle/ISKeyboard.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"

// One hot key record of the UI database (0x7c bytes) (provisional names; read in Rust: ee_ui hotkey.rs DbHotKey)
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
    return ee_ui_hotkey_create_hotkeys(&inFile);
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
    this->mKeyCount = ee_ui_hotkey_key_count(this->mKey != NULL, this->mModifiers[0] != NULL, this->mModifiers[1] != NULL,
                                             this->mModifiers[2] != NULL);
}

// FUNCTION: LLE 0x1007713f
// Builds the hot key's display name, e.g. "Ctrl + Shift + A" (key names without " key")
void UIHotKey::GetHotKeyName(UWideString& outName)
{
    const char* theNames[3];
    unsigned long theLength = 1;
    for (long i = 0; i < 3; i++) {
        theNames[i] = this->mModifiers[i] ? (const char*)this->mModifiers[i]->GetName() : NULL;
        if (theNames[i])
            theLength += strlen(theNames[i]) + 3;
    }
    const char* theKey = this->mKey->GetName();
    theLength += strlen(theKey);
    std::vector<unsigned char> theName(theLength);
    ee_ui_hotkey_name(theNames[0], theNames[1], theNames[2], theKey, &theName[0], theName.size());
    outName = (const char*)&theName[0];
}

// FUNCTION: LLE 0x100772bf
// Replaces the double-press event, releasing the old one and initializing the new one
void UIHotKey::SetDoublePressedEvent(UIEvent* inEvent)
{
    ee_ui_hotkey_set_event(reinterpret_cast<void**>(&this->mDoublePressedEvent), inEvent);
}

// FUNCTION: LLE 0x100772f0
// Replaces the just-pressed event, releasing the old one and initializing the new one
void UIHotKey::SetJustPressedEvent(UIEvent* inEvent)
{
    ee_ui_hotkey_set_event(reinterpret_cast<void**>(&this->mJustPressedEvent), inEvent);
}

// FUNCTION: LLE 0x10077321
// Replaces the just-released event, releasing the old one and initializing the new one
void UIHotKey::SetJustUnpressedEvent(UIEvent* inEvent)
{
    ee_ui_hotkey_set_event(reinterpret_cast<void**>(&this->mJustUnpressedEvent), inEvent);
}

// FUNCTION: LLE 0x10077352
// Replaces the held-down event, releasing the old one and initializing the new one
void UIHotKey::SetPressedEvent(UIEvent* inEvent)
{
    ee_ui_hotkey_set_event(reinterpret_cast<void**>(&this->mPressedEvent), inEvent);
}

// FUNCTION: LLE 0x10077383
// Sets one modifier slot to a key and adds it to the required modifier state
void UIHotKey::SetModifier(unsigned long inKey, unsigned char inIndex)
{
    ee_ui_hotkey_set_modifier(this, inKey, inIndex);
}

// The Rust FireEvents' questions about the hot key, its keys and events (ee_ui hotkey.rs, docs/RUST_PLAN.md R6)
struct UIHotKeyRust {
    static UIHotKey* Key(void* inKey) { return static_cast<UIHotKey*>(inKey); }
    static UIEvent* Event(void* inKey, int inEvent)
    {
        UIHotKey* theKey = Key(inKey);
        switch (inEvent) {
        case 0:
            return theKey->mDoublePressedEvent;
        case 1:
            return theKey->mJustPressedEvent;
        case 2:
            return theKey->mPressedEvent;
        default:
            return theKey->mJustUnpressedEvent;
        }
    }
    static unsigned long Value(void* inKey, int inWhich)
    {
        UIHotKey* theKey = Key(inKey);
        switch (inWhich) {
        case 0:
            return theKey->mEnabled;
        case 1:
            return theKey->mModifierState;
        case 2:
            return gISKeyboard->GetModifierState();
        case 3:
            return ISKeyboard::ComputeModifierState(static_cast<ISKey*>(theKey->mKey)->GetIndex());
        default:
            return theKey->mModifiers[inWhich - 4] != NULL;
        }
    }
    static long ModifierValue(void* inKey, int inIndex) { return Key(inKey)->mModifiers[inIndex]->GetValue(); }
    static bool KeyTest(void* inKey, int inEvent)
    {
        ISEntity* theKey = Key(inKey)->mKey;
        switch (inEvent) {
        case 0:
            return theKey->JustDoublePressed();
        case 1:
            return theKey->JustPressed();
        case 2:
            return theKey->GetValue() > 0;
        default:
            return theKey->JustUnpressed();
        }
    }
    static void Process(void* inKey, int inEvent) { Event(inKey, inEvent)->ProcessEvent(Key(inKey)->mForm, Key(inKey)); }
    static void CountKeys(void* inKey) { Key(inKey)->CalculateKeyCount(); }
    static long GetEntity(void* inKey, unsigned long inCode, int inMember)
    {
        UIHotKey* theKey = Key(inKey);
        return gISKeyboard->GetEntity(inCode, inMember == 3 ? theKey->mKey : theKey->mModifiers[inMember]);
    }
    static unsigned long* State(void* inKey) { return &Key(inKey)->mModifierState; }
    static void ClearModifier(void* inKey, int inSlot) { Key(inKey)->mModifiers[inSlot] = NULL; }
    static std::vector<UIHotKey*>& sHotKeys() { return UIHotKey::sHotKey; }
};

extern "C" unsigned long ee_uicpp_hotkey_value(void* inKey, int inWhich) { return UIHotKeyRust::Value(inKey, inWhich); }
extern "C" long ee_uicpp_hotkey_modifier_value(void* inKey, int inIndex) { return UIHotKeyRust::ModifierValue(inKey, inIndex); }
extern "C" bool ee_uicpp_hotkey_has_event(void* inKey, int inEvent) { return UIHotKeyRust::Event(inKey, inEvent) != NULL; }
extern "C" bool ee_uicpp_hotkey_key_test(void* inKey, int inEvent) { return UIHotKeyRust::KeyTest(inKey, inEvent); }
extern "C" void ee_uicpp_hotkey_process(void* inKey, int inEvent) { UIHotKeyRust::Process(inKey, inEvent); }

// The Rust setters' and CreateHotKeys' calls (ee_ui hotkey.rs): the events, the keyboard's entities, the members
extern "C" void ee_uicpp_hotkey_event_call(void* inEvent, int inCall)
{
    UIEvent* theEvent = static_cast<UIEvent*>(inEvent);
    switch (inCall) {
    case 0:
        theEvent->Uninitialize();
        break;
    case 1:
        theEvent->Dereference();
        break;
    case 2:
        theEvent->Reference();
        break;
    default:
        theEvent->Initialize();
        break;
    }
}
extern "C" long ee_uicpp_hotkey_get_entity(void* inKey, unsigned long inCode, int inMember)
{
    return UIHotKeyRust::GetEntity(inKey, inCode, inMember);
}
extern "C" unsigned long ee_uicpp_hotkey_modifier_state(unsigned long inCode) { return ISKeyboard::ComputeModifierState(inCode); }
extern "C" unsigned long ee_uicpp_hotkey_state(void* inKey) { return *UIHotKeyRust::State(inKey); }
extern "C" void ee_uicpp_hotkey_set_state(void* inKey, unsigned long inState) { *UIHotKeyRust::State(inKey) = inState; }
extern "C" void ee_uicpp_hotkey_clear_modifier(void* inKey, int inSlot) { UIHotKeyRust::ClearModifier(inKey, inSlot); }
extern "C" void ee_uicpp_hotkey_count_keys(void* inKey) { UIHotKeyRust::CountKeys(inKey); }
extern "C" void* ee_uicpp_hotkey_create() { return new UIHotKey; }
extern "C" void ee_uicpp_hotkey_push(void* inKey) { UIHotKeyRust::sHotKeys().push_back(static_cast<UIHotKey*>(inKey)); }

// FUNCTION: LLE 0x100773b6
// Fires the first matching event (double press, press, held, release) when enabled and the modifiers are down;
// true if one fired
bool UIHotKey::FireEvents()
{
    return ee_ui_hotkey_fire_events(this);
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
