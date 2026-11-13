// C:\EEX\Empire Earth\Scenario Editor\EESETSConditionType.h (RECONSTRUCTION; file name inferred). The condition
// screen's per-type control logic: RTTI EESETSConditionType (a root class; vtable 0x443a64: the ??_G 0x17eef3 of a
// user-declared inline empty dtor, four pure slots) and its seven
// types, created by EESETSCondition's ctor 0x17e49e (EESETSCondition::mTypes): 0 AlwaysTrue, 2 PlayerResource,
// 3 PlayerResearch, 4 TriggerAttribute, 5 Diplomacy, 6 Game, 7 ChatMessage. Their slots are EESETSConditionType.cpp
// (0x17ef10..0x182445). The ctors are inline (vptr stores at the `new` sites; PlayerResource's is the out-of-line COMDAT
// 0x17ee40, emitted by its first user EESETSCondition.cpp, like the ??_G 0x17ee9c/0x17eec8/0x17eef3).
// Slot use (EESETSCondition): +0x04 (condition, controls) writes the controls into the condition (Virtual24 0x17e9fb),
// +0x08 (controls) hides the type's controls, +0x0c (condition, controls) shows the condition's values (Virtual2C
// 0x17eae3), +0x10 (controls) shows the type's controls. Slot names by offset (provisional name).
// Requires ee/EESETSConditionControls.h, <vector>, lle/UIEvent.h.
#ifndef EE_EESETSCONDITIONTYPE_H
#define EE_EESETSCONDITIONTYPE_H

class EESMCondition;

class EESETSConditionType {
public:
    EESETSConditionType() {}
    virtual ~EESETSConditionType() {}       // +0x00 (??_G 0x17eef3, shared by the vptr-only types)
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls) = 0;       // +0x04
    virtual void Hide(EESETSConditionControls* inControls) = 0;                                   // +0x08
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls) = 0;        // +0x0c
    virtual void Show(EESETSConditionControls* inControls) = 0;                                   // +0x10
};

// vtable 0x443a34; sizeof 4. +0x04/+0x0c are the shared `ret 8` slot 0x163d6f.
class EESETSCTAlwaysTrue : public EESETSConditionType {
public:
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls) {}
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x17ef10
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls) {}
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x17ef92
};

// vtable 0x443a94; sizeof 4
class EESETSCTChatMessage : public EESETSConditionType {
public:
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x17f013
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x17f04e
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x17f06a
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x17f082
};

// vtable 0x443a1c; sizeof 4
class EESETSCTDiplomacy : public EESETSConditionType {
public:
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x17f18f
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x17f1c0
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x17f21d
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x17f24e
};

// vtable 0x443a04; sizeof 4
class EESETSCTGame : public EESETSConditionType {
public:
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x17f558
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x17f5d8
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x17f6a9
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x17f71a
};

// vtable 0x443a4c; sizeof 0x14 (the base's vptr store survives the vector's _Buy; ??_G 0x17eec8 inlines the dtor)
class EESETSCTPlayerResearch : public EESETSConditionType {
public:
    EESETSCTPlayerResearch() {}
    virtual ~EESETSCTPlayerResearch() {}   // (the ??_G stores the own vptr first: user-declared)
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x1801c8
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x1801f6
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x180226
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x1802a4

    std::vector<long> mTechnologies;        // +0x04 (element type unproven: 4 bytes)
};

// vtable 0x443b20; sizeof 8 (ctor: COMDAT 0x17ee40; ??_G 0x17ee9c inlines the dtor)
class EESETSCTPlayerResource : public EESETSConditionType {
public:
    EESETSCTPlayerResource();               // EESETSConditionType.cpp (its event an EEScreenEvent)
    virtual ~EESETSCTPlayerResource() { this->mEvent->Dereference(); }
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x180cb0
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x180d16
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x180d99
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x180e23

    UIEvent* mEvent;                        // +0x04 (the resource/amount controls' trigger event)
};

// vtable 0x443a7c; sizeof 4
class EESETSCTTriggerAttribute : public EESETSConditionType {
public:
    virtual void Store(EESMCondition* outCondition, EESETSConditionControls* inControls);         // 0x181450
    virtual void Hide(EESETSConditionControls* inControls);                                       // 0x18148a
    virtual void Load(EESMCondition* inCondition, EESETSConditionControls* inControls);           // 0x1814c3
    virtual void Show(EESETSConditionControls* inControls);                                       // 0x181502
};

#endif
