// C:\EEX\Empire Earth\Empire Earth\EEUnitSet.cpp (name from RTTI EEUnitSet; no __FILE__). 0x136d66..0x136f4c: one
// unit set of the database (ctor, ??_G, dtor, the adders, ForEach, Init, Contains). EEVoting.cpp's range is after
// EEUnitSetManager.cpp (0x136f4c). Unit flags: the exe's module set.
#include <windows.h>
#include <set>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// FUNCTION: EXE 0x536d66
// Creates an empty unit set (it allows every unit type)
EEUnitSet::EEUnitSet()
{
    this->Init();
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x536dae

// FUNCTION: EXE 0x536dca
// Destroys the unit set (members only)
EEUnitSet::~EEUnitSet()
{
}

// FUNCTION: EXE 0x536dee
// Adds a unit family to the set (valid families only) and marks the set as family based
void EEUnitSet::AddFamily(unsigned long inUnitClass)
{
    if (inUnitClass > 0 && inUnitClass < 0x3c) {
        this->mFamilies.insert(inUnitClass);
        this->mHasFamilies = true;
    }
}

// FUNCTION: EXE 0x536e1b
// Adds a unit type to the set (0 is ignored)
void EEUnitSet::AddUnitType(unsigned long inUnitClass)
{
    if (inUnitClass != 0)
        this->mUnitTypes.insert(inUnitClass);
}

// NEAR: the original tests bl at the loop top and returns without `mov al, bl` (the result of the last call)
// FUNCTION: EXE 0x536e3a
// Calls a functor for each unit type in the set until it returns false
void EEUnitSet::ForEach(EEUnitSetFunctor* inFunctor)
{
    bool theResult = true;
    for (unsigned long i = 1; i < EEUnitClassCount() && theResult; i++) {
        if (this->Contains(i))
            theResult = inFunctor->Execute(i);
    }

}

// FUNCTION: EXE 0x536e72
// Empties the unit set: no families, unit types, name or linked sets, hidden in the editor
void EEUnitSet::Init()
{
    this->mFamilies.clear();
    this->mUnitTypes.clear();
    this->mHasFamilies = false;
    this->mName = "";
    this->mIncludedSet = NULL;
    this->mExcludedSet = NULL;
    this->mShowInEditor = false;
}

// NEAR: branch layout/register use of the include/exclude tests (the original keeps both find results in bl/al
// and threads the mB38 test); behaviour is the reading above
// FUNCTION: EXE 0x536ea5
// Returns whether a unit type is in the set: in a family and not listed (family sets) or listed, or in the
// included set; never when in the excluded set (an empty set allows every type)
bool EEUnitSet::Contains(unsigned long inUnitType)
{
    bool theAllowed = false;
    if (this->mFamilies.size() == 0 && this->mUnitTypes.size() == 0 && this->mIncludedSet == NULL) {
        theAllowed = true;
    } else {
        EEUnitClass* theClass = gUnitClassTable[inUnitType];
        if (theClass != NULL) {
            bool theInIncluded = this->mFamilies.find(theClass->mFamily) != this->mFamilies.end();
            bool theInExcluded = this->mUnitTypes.find(inUnitType) != this->mUnitTypes.end();
            if (this->mHasFamilies)
                theAllowed = theInIncluded && !theInExcluded;
            else
                theAllowed = theInExcluded;
        }
        if (!theAllowed && this->mIncludedSet != NULL && this->mIncludedSet->Contains(inUnitType))
            theAllowed = true;
    }
    if (theAllowed) {
        if (this->mExcludedSet != NULL && this->mExcludedSet->Contains(inUnitType))
            return false;
        return true;
    }
    return false;
}
