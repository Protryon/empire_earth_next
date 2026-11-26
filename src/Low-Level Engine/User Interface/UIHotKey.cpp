// C:\EEX\Low-Level Engine\User Interface\UIHotKey.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10076f27..0x100774b9: the sPressedPoll/sHotKey init chains, then the out-of-line members (CreateHotKeys,
// DeleteHotKeys, the private CalculateKeyCount, GetHotKeyName, the Set*Event setters, SetModifier, FireEvents) and the
// vector-delete helper COMDAT last. The inline members are emitted by UIEMouseWheelScroll.cpp (0x1006e075..).
// UIIMECandidate.cpp's key chains start at 0x100774b9.
//
// UIHotKey is whole in Rust (rust/crates/ee_ui/src/hotkeyobj.rs, R6 ui round 20): the members forward to the Rust
// record, which calls out through ee_ui_hotkey_host (the keyboard's entities, the events, new and delete); the
// database records are read in Rust (hotkey.rs DbHotKey, 124 bytes).
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/Input System.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"


// The UI database files' calls (FSFileSpec&): the hot keys' records (hotkeyobj.rs), the events' and the background
// table's (UIManager.cpp, UIBackgroundTable.cpp)
extern "C" void ee_uicpp_file_open(void* inFile) { static_cast<FSFileSpec*>(inFile)->Open(kFileAccessRead); }
extern "C" long ee_uicpp_file_read_count(void* inFile, unsigned long* outCount)
{
    return static_cast<FSFileSpec*>(inFile)->Read(*outCount);
}
static unsigned long sUIFileRead;   // the reads' byte counts (unread)
extern "C" long ee_uicpp_file_read(void* inFile, unsigned char* outData, unsigned long inLength)
{ return static_cast<FSFileSpec*>(inFile)->Read(outData, inLength, sUIFileRead); }
extern "C" void ee_uicpp_file_close(void* inFile) { static_cast<FSFileSpec*>(inFile)->Close(); }

// The Rust hot keys' calls out (EeUiOp 570..581): the object is the record (NULL for the table's own calls)
#define r static_cast<EeUiHotKeyFields*>(inObj)
#define theEntity reinterpret_cast<ISEntity*>(a)
#define theEvent reinterpret_cast<UIEvent*>(a)
EE_UI_CASE(hk_value) { return theEntity->GetValue(); }
EE_UI_CASE(hk_double_pressed) { return theEntity->JustDoublePressed(); }
EE_UI_CASE(hk_just_pressed) { return theEntity->JustPressed(); }
EE_UI_CASE(hk_just_unpressed) { return theEntity->JustUnpressed(); }
EE_UI_CASE(hk_keyboard_state) { return gISKeyboard->GetModifierState(); }
EE_UI_CASE(hk_modifier_state) { return ISKeyboard::ComputeModifierState((unsigned long)a); }
EE_UI_CASE(hk_key_index) { return theEntity->GetIndex(); }
EE_UI_VOID(hk_process) { theEvent->ProcessEvent(static_cast<UIForm*>(r->mForm), static_cast<UIHotKey*>(r)); }
EE_UI_CASE(hk_new) { return (intptr_t) static_cast<EeUiHotKeyFields*>(new UIHotKey); }
EE_UI_VOID(hk_delete) { delete static_cast<UIHotKey*>(reinterpret_cast<EeUiHotKeyFields*>(a)); }
extern "C" intptr_t ee_uicpp_hk_get_entity(intptr_t inKey, ISEntity** ioEntity) { return gISKeyboard->GetEntity((unsigned long)inKey, *ioEntity); }
#undef theEvent
#undef theEntity
#undef r



// the entities' names (ee_ui hotkey_ffi.rs: the hot key's display name)
extern "C" const char* ee_uicpp_entity_name(void* inEntity) { return static_cast<ISEntity*>(inEntity)->GetName(); }

