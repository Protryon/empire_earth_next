// C:\EEX\Low-Level Engine\User Interface\UIEntityGroup.h (RECONSTRUCTION). A form's group of entities that are
// enabled and disabled together. The original's sizeof was 0x14 (not polymorphic).
//
// Whole in Rust (rust/crates/ee_ui/src/groupobj.rs, R6 ui round 21): the entities and the disable count are the Rust
// object mRust; every method forwards there (UIForm.cpp holds HostOp, the calls out on an entity).
#ifndef LLE_UIENTITYGROUP_H
#define LLE_UIENTITYGROUP_H
#include "lle/LLEForward.h"

#include "rust/ui/groupobj.h"
#include "rust/ui/lle_hosts.h"
#include "rust/ui/obj.h"
#include "lle/UIObjHost.h"


class LLE_API UIEntityGroup {
public:
    UIEntityGroup() : mRust(ee_ui_entity_group_new(this, ee_ui_group_host)) {}
    ~UIEntityGroup() { ee_ui_entity_group_free(this->mRust); }
    void Disable() { this->Call(EeUiEntityGroupCall::Disable); }
    void Enable() { this->Call(EeUiEntityGroupCall::Enable); }
    UIEntity* GetEntity(unsigned long inIndex) { return (UIEntity*)this->Call(EeUiEntityGroupCall::Entity, inIndex); }
    unsigned long GetEntityCount() { return (unsigned long)this->Call(EeUiEntityGroupCall::Count); }
    bool GetIsEnabled() { return this->Call(EeUiEntityGroupCall::IsEnabled) != 0; }
    intptr_t Call(EeUiEntityGroupCall inCall, intptr_t a = 0) { return ee_ui_entity_group_call(this->mRust, inCall, a); }

    URust<EeUiEntityGroup> mRust;                 // the entities and the disable count (groupobj.rs)
};

#endif
