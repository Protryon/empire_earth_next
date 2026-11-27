// C:\EEX\Low-Level Engine\User Interface\UIHotKey.h (RECONSTRUCTION). A key combination that fires up to
// four events. Vtable 0x1009997c (??_E, FireEvents); the original's sizeof was 0x38 (UIForm::AddHotKey news 0x38).
// Requires lle/UIObjHost.h (UIEntity).
//
// Whole in Rust (rust/crates/ee_ui/src/hotkeyobj.rs, R6 ui round 20): the members are EeUiHotKeyFields, the class's
// private base (UIForm.cpp, a friend, reads them in place); the constructors, the event setters, SetKey, SetModifier,
// the key count, FireEvents and the destructor forward to Rust, which calls out through UIHotKeyHost::Op; the shared
// table (sHotKey) is a Rust list. GetHotKeyName stays a C++ walk of the keys' names (round 9's Rust builds the name).
#ifndef LLE_UIHOTKEY_H
#define LLE_UIHOTKEY_H

#include "rust/ui/records.h"
#include "rust/ui/hotkeyobj.h"
#include "rust/ui/hotkey_ffi.h"
#include "lle/TMPoll.h"
#include "rust/ui/lle_hosts.h"
#include "lle/UIObjHost.h"


// winspool.h's `#define SetForm SetFormA` would rename UIHotKey::SetForm (exported ?SetForm@UIHotKey@@QAEXPAVUIForm@@@Z)
#ifdef SetForm
#undef SetForm
#endif

class LLE_API UIHotKey : public UIEntity, public EeUiHotKeyFields {
public:
    // a hot key with no keys or events, enabled; a copy onto a form, referencing its events
    UIHotKey() : UIEntity(0) { ee_ui_hotkey_init(this, ee_ui_hotkey_host, this); }
    UIHotKey(UIForm* inForm, const UIHotKey& inHotKey) : UIEntity(0) { ee_ui_hotkey_copy(this, ee_ui_hotkey_host, const_cast<UIHotKey*>(&inHotKey), inForm, this); }
    virtual ~UIHotKey() { this->Call(EeUiHotKeyCall::Release); }
    static UIHotKey* GetHotKey(unsigned long inIndex) { return static_cast<UIHotKey*>(ee_ui_hotkey_at(inIndex)); }
    void GetHotKeyName(UWideString& outName) { outName = ee_ui_hotkey_display_name(this); }
    void SetAlwaysFire(bool inAlwaysFire) { this->mAlwaysFire = inAlwaysFire; }
    void SetDoublePressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 0, (intptr_t)inEvent); }
    void SetEnabled(bool inEnabled) { this->mEnabled = inEnabled; }
    void SetJustPressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 1, (intptr_t)inEvent); }
    void SetJustUnpressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 3, (intptr_t)inEvent); }
    void SetKey(unsigned long inKey) { this->Call(EeUiHotKeyCall::SetKey, (intptr_t)inKey); }
    void SetModifier(unsigned long inKey, unsigned char inIndex) { this->Call(EeUiHotKeyCall::SetModifier, (intptr_t)inKey, inIndex); }
    void SetPressedEvent(UIEvent* inEvent) { this->Call(EeUiHotKeyCall::SetEvent, 2, (intptr_t)inEvent); }
    intptr_t Call(EeUiHotKeyCall inCall, intptr_t a = 0, intptr_t b = 0) { return ee_ui_hotkey_call(this, inCall, a, b); }
    // (the original's sPressedPoll is UIManager.cpp's hot key poll; its sPressedPollAmount, written, read by nothing,
    // went in UI round 16)
};

#endif
