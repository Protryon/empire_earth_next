// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffectTemplate.cpp (name from RTTI EEAreaEffectTemplate; folder
// inferred: no __FILE__). 0x27c0f3..0x27c8b9: the area-effect template (vtable 0x44be78; EEAEManager::Load 0x27b079
// makes one per record of the area-effect file): ctor, ??_G, dtor, the player-relation and range tests, Init (port:
// in EEAreaEffect.cpp, over its host), Load/Unload; then a unit's area-effect list (EEUnitAreaEffectList) members and the UReferenceContainer<EEAreaEffect*>
// template COMDATs they instantiate (Remove 0x27c7f2, Contains 0x27c820 (ICF-shared with <EEComplexUnit*>), 0x27c873
// (its end()), Erase 0x27c87f). EECBCivilizationCreator.cpp starts at 0x27c8b9. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"

#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"

// UReferenceContainer<T>::Contains (its COMDAT 0x27c820 closes this unit) is defined in ee/UReferenceContainer.h

// FUNCTION: EXE 0x67c5ab
// Loads the template's graphic effects and sound media
void EEAreaEffectTemplate::Load(bool inIsOnDemand)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Load(inIsOnDemand);
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Load(inIsOnDemand);
    if (this->mMedia != NULL)
        this->mMedia->LoadMedia();
}

// FUNCTION: EXE 0x67c5de
// Unloads the template's graphic effects and sound media
void EEAreaEffectTemplate::Unload()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Unload();
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Unload();
    if (this->mMedia != NULL)
        this->mMedia->UnloadMedia();
}

// FUNCTION: EXE 0x67c607
// Adds an area effect to the unit's list (taking a reference)
void EEUnitAreaEffectList::Add(EEAreaEffect* inEffect)
{
    UReferenceContainer<EEAreaEffect*>::Add(inEffect);
}

// FUNCTION: EXE 0x67c623
// Adds an area effect unless the unit already has a stronger or equal one of the same kind (all morale kinds
// count as one), replacing a weaker one
void EEUnitAreaEffectList::AddAreaEffect(EEAreaEffect* inEffect)
{
    long theKind = inEffect->mTemplate->mKind;
    std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin();
    for (; theIterator != this->mItems.end(); ++theIterator) {
        if (ee_area_effect_same_group(theKind, (*theIterator)->mTemplate->mKind))
            break;
    }
    if (theIterator != this->mItems.end()) {
        EEAreaEffect* theOld = *theIterator;
        if ((unsigned long)inEffect->GetStrength() <= (unsigned long)theOld->GetStrength())
            return;
        UReferenceContainer<EEAreaEffect*>::Remove(*theIterator);
    }
    if (inEffect != NULL)
        UReferenceContainer<EEAreaEffect*>::Add(inEffect);
}

// FUNCTION: EXE 0x67c6b9
// Lets every effect modify a value of the given kind, showing each effect that applied
void EEUnitAreaEffectList::ApplyModifiers(long inKind, const long& inDamage, long& outDamage, EEComplexUnit* inUnit)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        if ((*theIterator)->Modify(inKind, inDamage, outDamage))
            (*theIterator)->ShowOn(inUnit);
    }
}

// FUNCTION: EXE 0x67c6f3
// Removes the first effect of the given kind; returns whether one was found
bool EEUnitAreaEffectList::Remove(long inKind)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        if ((*theIterator)->mTemplate->mKind == inKind) {
            UReferenceContainer<EEAreaEffect*>::Remove(*theIterator);
            return true;
        }
    }
    return false;
}

// FUNCTION: EXE 0x67c721
// Rebuilds the list keeping only the strongest effect of each kind
void EEUnitAreaEffectList::KeepStrongest()
{
    EEUnitAreaEffectList theList;
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        theList.AddAreaEffect(*theIterator);
    *this = theList;
}

// FUNCTION: EXE 0x67c768
// Marks every effect in the list for a recount
void EEUnitAreaEffectList::MarkDirty()
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        (*theIterator)->MarkDirty();
}

// FUNCTION: EXE 0x67c783
// Moves every effect in the list to a new location and recounts it
void EEUnitAreaEffectList::MoveAndUpdate(const GE2DPoint& inPoint, float inValue)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator) {
        (*theIterator)->mLocation = inPoint;
        (*theIterator)->Recount((long)inValue);
    }
}

// FUNCTION: EXE 0x67c7c9
// Ticks every effect in the list on the unit
void EEUnitAreaEffectList::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    for (std::vector<EEAreaEffect*>::iterator theIterator = this->mItems.begin(); theIterator != this->mItems.end(); ++theIterator)
        (*theIterator)->Tick(inUnit, inElapsed);
}

// TEMPLATE: EXE 0x67c7f2
// TEMPLATE: EXE 0x67c820
// TEMPLATE: EXE 0x67c873
// TEMPLATE: EXE 0x67c87f
