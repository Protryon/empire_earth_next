// C:\EEX\Empire Earth\Scenario Manager\EESMObject.h (RECONSTRUCTION; file name inferred). One scenario object (RTTI
// EESMObject, a root class; vtable 0x4460a4, eight slots; ctor 0x1c5f73, dtor body 0x1c6035 behind the scalar
// deleting destructor 0x1c6019). Layout from the ctor: vptr, a
// UWideString at +0x04, a vector<long> at +0x18 (`_Buy` 0xe7d2e, `_Tidy` 0x2002c7), then longs to +0x7c and three
// bytes (sizeof 0x84). Slot signatures beyond Virtual0C are unproven. Member names by offset (provisional name).
// Requires lle/UWideString.h, <vector>. The fields +0x28..+0x7c are unsigned long (Load 0x1c8284 reads every one with
// FSFile::Read(unsigned long&), slot +0x5c; AcceptsClass compares mL2C/mL30 unsigned).
#ifndef EE_EESMOBJECT_H
#define EE_EESMOBJECT_H

#include <vector>
#include "rust/sim/scenario.h"
class EEComplexUnit;
class EEUnit;

// the numbers (mOwner .. mIsAttributePercent) are EeScenarioObjectFields (ee_scenario fields.rs)
class EESMObject : public EeScenarioObjectFields {
public:
    // Finds a free spot for a new unit of the type that meets the object's area and nearness filters (random tries for
    // 'not in/near' filters, then one retry without them); returns whether one was found (ee_scenario placement.rs over
    // the area's record; entry.rs adds the simulation capture's SMFP record)
    // 0x1c6d4f, actual ret12/AL/float output
    bool FindPlacement(unsigned long inUnitType, GE2DPoint& outLocation, bool inTryRandom) { return ee_scenario_en_find_placement(this, inUnitType, &outLocation.mX, &outLocation.mY, inTryRandom); }
    EESMObject();
    // Destroys the object (members only)
    // +0x00 (??_G 0x1c6019; the body 0x1c6035)
    virtual ~EESMObject() {  }
    // slot signatures from `ret N` and EEScenarioManager's broadcast hooks
    // Reads the object's units (appended) and filter settings (files before 0.0xcd: attribute ids above Construction
    // Complete move up by 3); returns the first error (ee_scenario save.rs object_load)
    // +0x04 0x1c8284
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_object_io(this, &inFile, inMajorVersion, inMinorVersion, false); }
    // Replaces every occurrence of a unit id in the object's unit list by a new id
    // +0x18 0x1c85a4 (replaces a unit id in mUnits)
    virtual void ReplaceUnit(long inOldUnit, long inNewUnit) { ee_scenario_en_object_replace_unit(this, inOldUnit, inNewUnit); }
    // Writes the object's units and filter settings (the parent only for minor version >= 0x2a); returns the first error
    // (ee_scenario save.rs object_save)
    // +0x1c 0x1c85de
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_object_io(this, &outFile, inMajorVersion, inMinorVersion, true); }
    // Copies another object's name and filter settings, and its unit list when it picks units on the map (ee_scenario
    // entry.rs)
    void Copy(const EESMObject& inObject) { ee_scenario_en_object_copy(this, (void*)&inObject); }
    // Returns a unit's attribute for an object filter: current values, upgrade bonuses over the class, or totals,
    // optionally as a percentage of the class or upgrade value; 0 for a NULL unit
    // 0x1c7749; last two slots unused/provisional
    long GetUnitAttribute(unsigned long inAttribute, bool inPercent, EEUnit* inUnit, long inArg4, long inArg5) { return ee_scenario_en_unit_attribute(inAttribute, inPercent, inUnit); }
    // Picks the unit type to create for the object: the parent's first unit's type, the object's class, or a random
    // class matching its epoch/family/AI kind/unit set filter; -1 if none
    // 0x1c6b14: chooses a matching unit class (provisional name)
    long PickUnitType() { return ee_scenario_en_object_pick_unit_type(this); }
    // Rebuilds a dynamic object's units (parent or owners), keeps those passing the state/attribute/selected/visible/area/
    // near filters, shuffles and caps them; returns whether any pass (visibility uses the selected-by player)
    // (ee_scenario objeval.rs over the object's list and the world: EESMObjectRead / EESMObjectWrite below; entry.rs adds
    // the simulation capture's SMOB record)
    // 0x1c6458 (the object condition: its units exist/are in the area; provisional name)
    bool Evaluate(bool inIncludeDead) { return ee_scenario_en_object_evaluate(this, inIncludeDead); }
    // Copies the object's unit ids out
    void GetUnits(std::vector<unsigned long>& outUnits) const { outUnits = this->mUnits; }
    // inline const accessor (Save 0x1c85de reads mUnits[i] through the const operator[]: lea; mov [eax]) (provisional
    // name)
    unsigned long GetUnitAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }  // 0x1c8274 (`outUnits = mUnits`)
    unsigned long GetUnitCount() const { return this->mUnits.size(); }  // inline (EESMEffect 0x5bd5eb, EESMObject 0x5c6458)
    // inline value getters (EESMEffect 0x5c2b3e / 0x5bc942 read the object test through them)
    bool GetIsSelectOnMap() const { return this->mIsSelectOnMap; }
    unsigned long GetInAreaCheck() const { return this->mInAreaCheck; }
    unsigned long GetNearCheck() const { return this->mNearCheck; }
    unsigned long GetAttribute() const { return this->mAttribute; }
    bool GetIsAttributePercent() const { return this->mIsAttributePercent; }
    unsigned long GetArea() const { return this->mArea; }
    // Replaces the object's unit list
    void SetUnits(const std::vector<unsigned long>& inUnits) { this->mUnits = inUnits; }
    // Sets the parent object; a parent chain that loops back to this object resets the parent to 0 (ee_scenario
    // lists.rs set_parent)
    // 0x1c96b7 (0 when it would close a cycle; provisional name)
    void SetParent(long inParent) { ee_scenario_en_object_set_parent(this, inParent); }
    // Renames the object and renames its object conditions to "<name> Exists" (ee_scenario lists.rs)
    // 0x1c9628 (also renames the conditions that use the object)
    void SetName(const UWideString& inName) { ee_scenario_en_object_set_name(this, &inName); }

    UWideString mName;                      // +0x04 (ctor: gResource string 0x32c9)
    std::vector<unsigned long> mUnits;      // +0x18 (unit ids; Load reads them as unsigned long)
};

#endif
