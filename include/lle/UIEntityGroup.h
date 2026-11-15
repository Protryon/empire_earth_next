// C:\EEX\Low-Level Engine\User Interface\UIEntityGroup.h (RECONSTRUCTION). A form's group of entities that are
// enabled and disabled together. The original's sizeof was 0x14 (not polymorphic).
//
// Whole in Rust (rust/crates/ee_ui/src/groupobj.rs, R6 ui round 21): the entities and the disable count are the Rust
// object mRust; every method forwards there (UIForm.cpp holds HostOp, the calls out on an entity).
#ifndef LLE_UIENTITYGROUP_H
#define LLE_UIENTITYGROUP_H

#include "rust/ee_ui.h"

class UIEntity;

class LLE_API UIEntityGroup {
    friend class UIForm;                    // AddControl/RemoveControl call AddEntity/RemoveEntity (private)
public:
    UIEntityGroup() : mRust(ee_ui_entity_group_new(this, UIEntityGroup::HostOp)) {}
    UIEntityGroup(const UIEntityGroup&) = delete;
    UIEntityGroup& operator=(const UIEntityGroup&) = delete;
    ~UIEntityGroup() { ee_ui_entity_group_free(this->mRust); }
    void Disable() { this->Call(EeUiEntityGroupCall::Disable); }
    void Enable() { this->Call(EeUiEntityGroupCall::Enable); }
    UIEntity* GetEntity(unsigned long inIndex) { return (UIEntity*)this->Call(EeUiEntityGroupCall::Entity, inIndex); }
    unsigned long GetEntityCount() { return (unsigned long)this->Call(EeUiEntityGroupCall::Count); }
    bool GetIsEnabled() { return this->Call(EeUiEntityGroupCall::IsEnabled) != 0; }
private:
    long AddEntity(UIEntity* inEntity) { return (long)this->Call(EeUiEntityGroupCall::Add, (intptr_t)inEntity); }
    long RemoveEntity(UIEntity* inEntity) { return (long)this->Call(EeUiEntityGroupCall::Remove, (intptr_t)inEntity); }
    intptr_t Call(EeUiEntityGroupCall inCall, intptr_t a = 0) { return ee_ui_entity_group_call(this->mRust, inCall, a); }
    static intptr_t HostOp(void* inGroup, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f);

    EeUiEntityGroup* mRust;                 // the entities and the disable count (groupobj.rs)
};

#endif
