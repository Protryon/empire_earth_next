// C:\EEX\Low-Level Engine\User Interface\UIHotKey.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10076f27..0x100774b9: the sPressedPoll/sHotKey init chains, then the out-of-line members (CreateHotKeys,
// DeleteHotKeys, the private CalculateKeyCount, GetHotKeyName, the Set*Event setters, SetModifier, FireEvents) and the
// vector-delete helper COMDAT last. The inline members are emitted by UIEMouseWheelScroll.cpp (0x1006e075..).
// UIIMECandidate.cpp's key chains start at 0x100774b9.
//
// UIHotKey is whole in Rust (rust/crates/ee_ui/src/hotkeyobj.rs, R6 ui round 20): the members forward to the Rust
// record, which calls out through UIHotKeyHost::Op (the keyboard's entities, the events, new and delete); the
// database records are read in Rust (hotkey.rs DbHotKey, 124 bytes).
#include <windows.h>
#include <string.h>

#include "rust/ui/hotkey_ffi.h"
#include "rust/ui/hotkeyobj.h"
#include "rust/ui/obj.h"
#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"
#include "lle/FSFileSpec.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"

TMPoll UIHotKey::sPressedPoll(50, 0, true);
unsigned long UIHotKey::sPressedPollAmount;

// The UI database files' calls (FSFileSpec&): the hot keys' records (hotkeyobj.rs), the events' and the background
// table's (UIManager.cpp, UIBackgroundTable.cpp)
extern "C" void ee_uicpp_file_open(void* inFile) { static_cast<FSFileSpec*>(inFile)->Open(kFileAccessRead); }
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount)
{
    return static_cast<FSFileSpec*>(inFile)->Read(*outCount);
}
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength)
{
    unsigned long theRead = 0;
    return static_cast<FSFileSpec*>(inFile)->Read(outData, inLength, theRead);
}
extern "C" void ee_uicpp_file_close(void* inFile) { static_cast<FSFileSpec*>(inFile)->Close(); }

// The Rust hot keys' calls out (EeUiOp 570..581): the object is the record (NULL for the table's own calls)
struct UIHotKeyHost {
    static intptr_t Op(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float*)
    {
        EeUiHotKeyFields* r = static_cast<EeUiHotKeyFields*>(inObj);
        ISEntity* theEntity = reinterpret_cast<ISEntity*>(a);
        UIEvent* theEvent = reinterpret_cast<UIEvent*>(a);
        switch (inOp) {
        case EeUiOp::HkValue: return theEntity->GetValue();
        case EeUiOp::HkDoublePressed: return theEntity->JustDoublePressed();
        case EeUiOp::HkJustPressed: return theEntity->JustPressed();
        case EeUiOp::HkJustUnpressed: return theEntity->JustUnpressed();
        case EeUiOp::HkKeyboardState: return gISKeyboard->GetModifierState();
        case EeUiOp::HkModifierState: return ISKeyboard::ComputeModifierState((unsigned long)a);
        case EeUiOp::HkKeyIndex: return theEntity->GetIndex();
        case EeUiOp::HkEntity: gISKeyboard->GetEntity((unsigned long)a, theEntity); return (intptr_t)theEntity;
        case EeUiOp::HkEntityResult: return gISKeyboard->GetEntity((unsigned long)a, theEntity);
        case EeUiOp::HkEventCall:
            switch (b) {
            case 0: theEvent->Uninitialize(); break;
            case 1: theEvent->Dereference(); break;
            case 2: theEvent->Reference(); break;
            default: theEvent->Initialize(); break;
            }
            break;
        case EeUiOp::HkProcess: theEvent->ProcessEvent(static_cast<UIForm*>(r->mForm), static_cast<UIHotKey*>(r)); break;
        case EeUiOp::HkNew: return (intptr_t) static_cast<EeUiHotKeyFields*>(new UIHotKey);
        case EeUiOp::HkDelete: delete static_cast<UIHotKey*>(reinterpret_cast<EeUiHotKeyFields*>(a)); break;
        default: break;
        }
        return 0;
    }
};

// Creates a hot key with no keys or events, enabled
UIHotKey::UIHotKey()
    : UIEntity(0)
{
    ee_ui_hotkey_init(this, UIHotKeyHost::Op);
}

// Copies a hot key onto a form, referencing its events
UIHotKey::UIHotKey(UIForm* inForm, const UIHotKey& inHotKey)
    : UIEntity(0)
{
    ee_ui_hotkey_copy(this, UIHotKeyHost::Op, const_cast<UIHotKey*>(&inHotKey), inForm);
}

UIHotKey::~UIHotKey() { this->Call(EeUiHotKeyCall::Release); }
long UIHotKey::CreateHotKeys(FSFileSpec& inFile) { return ee_ui_hotkey_create_all(&inFile, UIHotKeyHost::Op); }
void UIHotKey::DeleteHotKeys() { ee_ui_hotkey_delete_all(UIHotKeyHost::Op); }
bool UIHotKey::FireEvents() { return this->Call(EeUiHotKeyCall::FireEvents) != 0; }

// Builds the hot key's display name, e.g. "Ctrl + Shift + A" (key names without " key")
void UIHotKey::GetHotKeyName(UWideString& outName)
{
    const char* theNames[3];
    unsigned long theLength = 1;
    for (long i = 0; i < 3; i++) {
        theNames[i] = this->mModifiers[i] ? (const char*)static_cast<ISEntity*>(this->mModifiers[i])->GetName() : NULL;
        if (theNames[i])
            theLength += strlen(theNames[i]) + 3;
    }
    const char* theKey = static_cast<ISEntity*>(this->mKey)->GetName();
    theLength += strlen(theKey);
    std::vector<unsigned char> theName(theLength);
    ee_ui_hotkey_name(theNames[0], theNames[1], theNames[2], theKey, &theName[0], theName.size());
    outName = (const char*)&theName[0];
}
