// C:\EEX\Low-Level Engine\User Interface\UIEvent.h (RECONSTRUCTION). Base of the UI's triggered events (reference
// counted). Vtable 0x10098b5c: ProcessEvent (pure, named from the UIE* overrides), InitializeEvent, UninitializeEvent
// (no event overrode those two: Rust answers kErrorOK).
// The original's sizeof was 0xc.
//
// Whole in Rust (rust/crates/ee_ui/src/eventobj.rs, R6 ui round 21): the member is EeUiEventFields, the class's private
// base (the inline constructor reads it in place); Initialize and Uninitialize forward there, and the two loaders are
// Rust's (ee_ui lle_chains.rs db_*), calling out through HostOp: the virtuals, the events file and the owners.
#ifndef LLE_UIEVENT_H
#define LLE_UIEVENT_H

#include "rust/ui/eventobj.h"
#include "lle/UIShims.h"
#include "rust/ui/records.h"
#include "lle/UIObjHost.h"


class LLE_API UIEvent : public UReferenceCounter, public EeUiEventFields {
public:
    UIEvent() { this->mInitializeCount = 0; }  // body assignment: the store follows the vptr load (exe 0x1aa9ef)
    virtual ~UIEvent() {}
    long Initialize() { return ee_ui_event_call(this, ee_ui_event_host, EeUiEventCall::Initialize); }
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity) = 0;   // slot 0x04
    long Uninitialize() { return ee_ui_event_call(this, ee_ui_event_host, EeUiEventCall::Uninitialize); }
    // the Rust record's and loaders' calls out that are not Rust's (UIManager.cpp; ee_ui lle_manager.rs:
    // ee_ui_event_host comes first): the object is the record or the file
};

#endif
