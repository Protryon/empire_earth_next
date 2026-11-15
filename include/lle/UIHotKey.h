// C:\EEX\Low-Level Engine\User Interface\UIHotKey.h (RECONSTRUCTION). A key combination that fires up to
// four events. Vtable 0x1009997c (??_E, FireEvents); the original's sizeof was 0x38 (UIForm::AddHotKey news 0x38).
// Requires lle/UIEntity.h, lle/Input System.h (gISKeyboard).
//
// Whole in Rust (rust/crates/ee_ui/src/hotkeyobj.rs, R6 ui round 20): the members are EeUiHotKeyFields, the class's
// private base (UIForm.cpp, a friend, reads them in place); the constructors, the event setters, SetKey, SetModifier,
// the key count, FireEvents and the destructor forward to Rust, which calls out through UIHotKeyHost::Op; the shared
// table (sHotKey) is a Rust list. GetHotKeyName stays a C++ walk of the keys' names (round 9's Rust builds the name).
#ifndef LLE_UIHOTKEY_H
#define LLE_UIHOTKEY_H

#include "rust/ee_ui.h"
#include "lle/UIEntity.h"
#include "lle/TMPoll.h"

class FSFileSpec;
class ISEntity;
class UIEvent;
class UIForm;

// winspool.h's `#define SetForm SetFormA` would rename UIHotKey::SetForm (exported ?SetForm@UIHotKey@@QAEXPAVUIForm@@@Z)
#ifdef SetForm
#undef SetForm
#endif

class LLE_API UIHotKey : public UIEntity, private EeUiHotKeyFields {
    friend class UIForm;                    // UIForm.cpp reads and writes the fields directly
    friend struct UIHotKeyHost;             // UIHotKey.cpp: the Rust object's calls out
public:
    UIHotKey();
    UIHotKey(UIForm* inForm, const UIHotKey& inHotKey);
    virtual ~UIHotKey();
    static long CreateHotKeys(FSFileSpec& inFile);
    static void DeleteHotKeys();
    virtual bool FireEvents();              // slot 0x04
    bool GetAlwaysFire() const { return this->mAlwaysFire; }
    static UIHotKey* GetHotKey(unsigned long inIndex) { return static_cast<UIHotKey*>(ee_ui_hotkey_at(inIndex)); }
    static unsigned long GetHotKeyCount() { return ee_ui_hotkey_count(); }
    void GetHotKeyName(UWideString& outName);
    unsigned long GetKeyCount() const { return this->mKeyCount; }
    void SetAlwaysFire(bool inAlwaysFire) { this->mAlwaysFire = inAlwaysFire; }
    void SetDoublePressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 0, (intptr_t)inEvent); }
    void SetEnabled(bool inEnabled) { this->mEnabled = inEnabled; }
    void SetForm(UIForm* inForm) { this->mForm = inForm; }
    void SetJustPressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 1, (intptr_t)inEvent); }
    void SetJustUnpressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 3, (intptr_t)inEvent); }
    void SetKey(unsigned long inKey) { this->Call(EeUiHotKeyCall::SetKey, (intptr_t)inKey); }
    void SetModifier(unsigned long inKey, unsigned char inIndex) { this->Call(EeUiHotKeyCall::SetModifier, (intptr_t)inKey, inIndex); }
    void SetPressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 2, (intptr_t)inEvent); }
#ifdef LOWLEVELENGINE_EXPORTS
    static void UpdatePressedPollAmount() { UIHotKey::sPressedPollAmount = UIHotKey::sPressedPoll.Poll(); }
#endif
private:
    void CalculateKeyCount() { this->Call(EeUiHotKeyCall::CountKeys); }
    intptr_t Call(EeUiHotKeyCall inCall, intptr_t a = 0, intptr_t b = 0) { return ee_ui_hotkey_call(this, inCall, a, b); }

#ifdef LOWLEVELENGINE_EXPORTS
    static TMPoll sPressedPoll;
    static unsigned long sPressedPollAmount;
#endif
};

#endif
