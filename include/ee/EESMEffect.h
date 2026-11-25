// C:\EEX\Empire Earth\Scenario Manager\EESMEffect.h (RECONSTRUCTION; file name inferred). One scenario trigger effect
// (RTTI EESMEffect, a root class; vtable 0x446068, nine slots +0x00..+0x20 at
// 0x1bfa67/0x1bef17/0x1bf508/0x1bf2b3/0x1bf54c/0x1bf384/0x1bf5c3/ 0x1bf4c9/0x1c0134; sizeof 0x194 from the `new`
// sites). Layout from the ctor 0x1bb5a5 (vector<long> at +0x04, nine UWideStrings +0x14..+0xc8, then Reset 0x1bb65a)
// and the dtor 0x1bb609 (not virtual; stores the vptr). The slots'
// signatures are unproven (declared without arguments) except Load/Save/remaps. Member types from Load 0x1bfa67 / Save
// 0x1c0134 (FSFile Read/Write slots). Member names by offset (provisional name). Requires lle/UWideString.h.
#ifndef EE_EESMEFFECT_H
#define EE_EESMEFFECT_H

#include <vector>
#include "lle/UWideString.h"
#include "rust/sim/scenario.h"

// the numbers (mPlayerAttribute .. mB193, their order and types) are EeScenarioEffectFields (ee_scenario fields.rs):
// their defaults, saves and index upkeep are Rust's
class EESMEffect : public EeScenarioEffectFields {
public:
    // Creates a scenario effect with the default settings (see Reset)
    EESMEffect() { this->Reset(); }
    // Destroys the effect (members only)
    ~EESMEffect() {  }
    // Copies another effect's settings, texts and object list
    // 0x1bc60f (field-wise; EEScenarioManager 0x1b000d; provisional name)
    void Copy(const EESMEffect& inEffect) { ee_scenario_effect_copy(this, &inEffect); }
    // Sets, raises or lowers a player value: a resource, the population cap (also queued as a pending effect), difficulty,
    // a scenario variable, a tribute or other statistic or the mining limit; any/all resources recurse per resource
    // (ee_scenario player.rs: the plan of writes, EEScenarioHost.cpp's write callback; the any/all resources come back
    // here; entry.rs adds the simulation capture's SMPA record)
    void SetPlayerAttribute(long inPlayer, unsigned long inAttribute, long inMode, long inAmount) { ee_scenario_en_effect_set_player_attribute(this, inPlayer, inAttribute, inMode, inAmount); }
    // 0x1bd5eb (6.4 KB: the effect's execution, its 17 helpers' bodies with it: ee_scenario effect.rs, effect_ai.rs,
    // task.rs over EESMEffectWorld; provisional name)
    // Runs the effect: an object action, a player action (victory, diplomacy, tech tree, camera, flare...), trigger state,
    // media (sound, text, dialogue, chat), a game action (time of day, calamity, graphic effect, cinematic) or an AI action
    // (entry.rs: ExecuteUncaptured with the simulation capture's SMEX record)
    void Execute(bool inIsCinematic) { ee_scenario_en_effect_execute(this, inIsCinematic); }
    // Resets every effect setting to its default: an object Create effect, 20 s duration, empty texts and object list
    // (the numbers: ee_scenario fields.rs effect_defaults)
    // 0x1bb65a (the ctor's tail; assigns "" to the strings, the numbers below)
    void Reset() { ee_scenario_effect_reset(this); }
    // Reads the effect from a scenario file, renumbering player actions and unit attributes of older files; returns the
    // first read error (ee_scenario save.rs effect_load)
    // +0x00 0x1bfa67
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_effect_io(this, &inFile, inMajorVersion, inMinorVersion, false); }
    // Copies a message text, replacing each /#OA, /#PA or /#OC tag and its 4-digit id with an object's attribute value,
    // a player condition's current value or an object's unit count
    // (ee_scenario tokens.rs expand_tokens; entry.rs adds the simulation capture's SMTK record)
    // +0x04 0x1bef17
    virtual void ExpandTokens(const UWideString& inText, UWideString& outText) { ee_scenario_en_effect_expand_tokens(this, (void*)&inText, &outText); }
    // Writes the effect to a scenario file (civ points from minor version 0x2a, mB193 from 0x14); returns the first write
    // error (ee_scenario save.rs effect_save)
    // +0x20 0x1c0134
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_effect_io(this, &outFile, inMajorVersion, inMinorVersion, true); }
    // inline accessors (EESETSEffect's Virtual24/Virtual2C: EESMCondition's shape; provisional names)
    const UWideString& GetName() const { return this->mName; }
    const UWideString& GetText() const { return this->mText; }
    // inline (out-of-line COMDAT 0x1c577c `lea eax, [ecx + 0x8c]`: SetClassName's two SetName copies; provisional name)
    const UWideString& GetNewName() const { return this->mNewName; }
    unsigned long GetType() const { return this->mType; }
    unsigned long GetID() const { return this->mID; }
    float GetFloatValue() const { return this->mFloatValue; }  // inline (Copy 0x5bc60f copies it with fld/fstp; provisional name)
    void SetName(const UWideString& inName) { this->mName = inName; }
    void SetText(const UWideString& inText) { this->mText = inText; }
    void SetType(unsigned long inType) { this->mType = inType; }

    std::vector<unsigned long> mObjects;    // +0x04 (`_Buy` 0xe7d2e, `_Tidy` 0x2002c7; Load/Save: unsigned long elements)
    UWideString mName;                      // +0x14
    UWideString mText;                      // +0x28
    UWideString mS3C;                       // +0x3c (not assigned by Reset)
    UWideString mMessage;                   // +0x50
    UWideString mS64;                       // +0x64
    UWideString mS78;                       // +0x78
    UWideString mNewName;                   // +0x8c
    UWideString mFileName;                  // +0xa0
    UWideString mDescription;               // +0xb4
};

#endif
