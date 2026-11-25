// EEUnitSetManager.h (RECONSTRUCTION; file name inferred). The unit sets of the database (RTTI EEUnitSetManager;
// vtable 0x441fd4). The game's instance is a global object at 0x52f660 (provisional name). Since the game's leaves
// round 2 the sets are ee_sim's (app/unit_sets.rs: the records, the table, the links, Contains); the classes here are
// their inline readers and an EEUnitSet* is a Rust set (EeUnitSet; never made or freed by the C++).
#ifndef EE_EEUNITSETMANAGER_H
#define EE_EEUNITSETMANAGER_H

#include <stdint.h>

#include "rust/sim/app.h"

// A callback over unit types (EEUnitSet::ForEach calls slot +0x00; provisional name)
class EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType) = 0;
};

// The Rust walks' visit (app/unit_sets.rs ee_unit_set_for_each): the functor's Execute
inline bool EEUnitSetVisit(void* inFunctor, uint32_t inUnitType) { return static_cast<EEUnitSetFunctor*>(inFunctor)->Execute((long)inUnitType); }

// One unit set (provisional name; the original: RTTI, vtable 0x441fcc, sizeof 0x3c)
class EEUnitSet {
public:
    // 0x136ea5: whether it holds unit type inUnitType (a family set: its families' types but the listed ones; a type
    // set: the listed types; then the included set's; never the excluded set's; an empty set holds every type)
    bool Contains(unsigned long inUnitType) const { return ee_unit_set_contains(this->Rust(), (uint32_t)inUnitType); }
    // 0x136e3a: every unit type 1..EEUnitClassCount() - 1 in the set, until the functor returns false
    void ForEach(EEUnitSetFunctor* inFunctor) const { ee_unit_set_for_each(this->Rust(), inFunctor, EEUnitSetVisit); }
    bool IsEmpty() const { return ee_unit_set_is_empty(this->Rust()); }   // inline (COMDAT 0x2bae4)
    const char* GetName() const { return ee_unit_set_name(this->Rust()); }   // the record's name (provisional name)
    bool ShowInEditor() const { return ee_unit_set_show_in_editor(this->Rust()); }

private:
    const EeUnitSet* Rust() const { return reinterpret_cast<const EeUnitSet*>(this); }
    EEUnitSet();
    ~EEUnitSet();
};

class EEUnitSetManager {
public:
    // original-backed 0x136fa9: the records (an empty slot for each unused one), then each set's included and
    // excluded sets; the last read's result (requires ee/EEDbRecords.h)
    template <class F> long Load(F* inFile)
    {
        ee_unit_sets_clear();
        long theResult = EEDbReadRecords(inFile, ee_unit_sets_record_size(), [](unsigned long) { return true; },
            [](unsigned long, const unsigned char* inRecord) { ee_unit_sets_load(inRecord); });
        ee_unit_sets_link();
        return theResult;
    }
    EEUnitSetManager() {}
    ~EEUnitSetManager() { ee_unit_sets_clear(); }
    // 0xd150: set inSet (NULL past the end or for an unused record)
    EEUnitSet* GetSet(unsigned long inSet) const
        { return reinterpret_cast<EEUnitSet*>(const_cast<EeUnitSet*>(ee_unit_sets_get((size_t)inSet))); }
    unsigned long GetSize() const { return (unsigned long)ee_unit_sets_count(); }
    // 0xacb52: whether set inSet (false if there is none) holds unit type inUnitType
    bool Contains(long inUnitType, unsigned long inSet) const
        { return ee_unit_sets_contains((size_t)inSet, (uint32_t)inUnitType); }
    // inline (COMDAT 0x2830ae): set inSet's ForEach when it exists; true
    bool ForEach(unsigned long inSet, EEUnitSetFunctor* inFunctor) const { return ee_unit_sets_for_each((size_t)inSet, inFunctor, EEUnitSetVisit); }
};

extern EEUnitSetManager gUnitSetManager;

#endif
