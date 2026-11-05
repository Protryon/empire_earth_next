// EEUnitSetManager.h (RECONSTRUCTION; file name inferred). The unit sets of the database (RTTI EEUnitSetManager;
// vtable 0x441fd4). The game's instance is a global object at 0x52f660 (provisional name). Only
// what the decompiled units call; (provisional names)
#ifndef EE_EEUNITSETMANAGER_H
#define EE_EEUNITSETMANAGER_H
class FSFile;

#include <set>
#include <vector>

// A callback over unit types (EEUnitSet::ForEach calls slot +0x00; provisional name)
class EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType) = 0;
};

// One unit set (provisional name): 0x136ea5 tests whether it holds unit type inUnitType
class EEUnitSet {                           // RTTI, vtable 0x441fcc (one slot), sizeof 0x3c, ctor 0x136d66
public:
    virtual ~EEUnitSet();                   // +0x00 (??_G 0x136dae)
    bool Contains(unsigned long inUnitType);   // 0x136ea5 (unsigned: the sets hold unsigned long, find/insert take the parameter's address)
    void Init();                            // 0x136e72 (clears both sets, mName = "", the rest 0; provisional name)
    EEUnitSet();                            // 0x136d66
    void AddFamily(unsigned long inUnitClass);     // 0x136dee (a class in 1..0x3b; sets mB38; provisional name)
    void AddUnitType(unsigned long inUnitClass);   // 0x136e1b (provisional name)
    // 0x136e3a (every unit type 1..EEUnitClassCount() - 1 in the set; stops at the first false). void: the original keeps the loop's
    // result in bl and never loads al; both callers discard it
    void ForEach(EEUnitSetFunctor* inFunctor);
    // inline (COMDAT 0x2bae4, `push 1; pop eax`; EECPAttackSubGroup::GetGateWeight calls it) (provisional name)
    bool IsEmpty() { return this->mFamilies.size() == 0 && this->mUnitTypes.size() == 0; }

    // Layout gap-free from the ctor 0x136d66 / dtor 0x136dca / Init: two trees with 0x14-byte
    // nodes (a 4-byte key: set<long>; the inline _Init 0x2921cd), a UWideString, two longs, two flags. Contains
    // looks inUnitType's class +0x1c8 up in mFamilies. Names are provisional
    std::set<unsigned long> mFamilies;               // +0x04
    std::set<unsigned long> mUnitTypes;              // +0x10
    UWideString mName;                      // +0x1c
    EEUnitSet* mIncludedSet;                // +0x30 (Contains: all three empty -> no unit is in the set; a second set tried when this one fails)
    EEUnitSet* mExcludedSet;                // +0x34 (a unit in it is excluded)
    bool mHasFamilies;                      // +0x38
    bool mShowInEditor;                     // +0x39
};

class EEUnitSetManager {
public:
    long Load(FSFile* inFile); // original-backed 0x136fa9; caller1414a3, FSFile IO proof
    // inline ctor (Empire Earth.cpp's chain stub 0x13edee builds the vector, then stores the vptr 0x441fd4); the dtor
    // 0x136f4c is EEUnitSetManager.cpp's
    EEUnitSetManager() {}
    virtual ~EEUnitSetManager();
    EEUnitSet* GetSet(unsigned long inSet);   // 0xd150 (0 past the end; EEMGoFanatic: set 0x2f)
    unsigned long GetSize() const { return this->mSets.size(); }  // inline; ICF-folded into UReferenceContainer<EEComplexUnit*>::GetSize 0x91f11 (EESETSObject's ctor; provisional name)
    // 0xacb52: whether set inSet (index into the table at +0x08; false if there is none) holds unit type inUnitType
    bool Contains(long inUnitType, unsigned long inSet);
    // inline (COMDAT 0x2830ae after its only user, EECivilizationManager 0x281e4d): set inSet's ForEach when it exists
    bool ForEach(unsigned long inSet, EEUnitSetFunctor* inFunctor)
        { if (this->mSets[inSet] != NULL) this->mSets[inSet]->ForEach(inFunctor); return true; }

    std::vector<EEUnitSet*> mSets;          // +0x04 (`_Buy` 0x14541f)
};

extern EEUnitSetManager gUnitSetManager;    // 0x52f660

#endif
