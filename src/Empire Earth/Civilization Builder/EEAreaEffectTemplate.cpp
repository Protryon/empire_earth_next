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

#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaEffectTemplate.h"
#include "rust/game/gamecpp.h"

// UReferenceContainer<T>::Contains (its COMDAT 0x27c820 closes this unit) is defined in ee/UReferenceContainer.h

// port (round 10, lane C): the bodies are gamecpp ae_effect.rs's; each list passes itself as its container
#define EELIST static_cast<UReferenceContainer<EEAreaEffect*>*>(this)

// Loads the template's graphic effects and sound media
void EEAreaEffectTemplate::Load(bool inIsOnDemand)
{
    ee_gamecpp_aetpl_media(this, false, inIsOnDemand);
}

// Unloads the template's graphic effects and sound media
void EEAreaEffectTemplate::Unload()
{
    ee_gamecpp_aetpl_media(this, true, false);
}

// Adds an area effect to the unit's list (taking a reference)
void EEUnitAreaEffectList::Add(EEAreaEffect* inEffect)
{
    UReferenceContainer<EEAreaEffect*>::Add(inEffect);
}

// Adds an area effect unless the unit already has a stronger or equal one of the same kind (all morale kinds
// count as one), replacing a weaker one
void EEUnitAreaEffectList::AddAreaEffect(EEAreaEffect* inEffect)
{
    ee_gamecpp_aelist_add(EELIST, inEffect);
}

// Lets every effect modify a value of the given kind, showing each effect that applied
void EEUnitAreaEffectList::ApplyModifiers(long inKind, const long& inDamage, long& outDamage, EEComplexUnit* inUnit)
{
    ee_gamecpp_aelist_modifiers(EELIST, inKind, &inDamage, &outDamage, inUnit);
}

// Removes the first effect of the given kind; returns whether one was found
bool EEUnitAreaEffectList::Remove(long inKind)
{
    return ee_gamecpp_aelist_remove(EELIST, inKind);
}

// Rebuilds the list keeping only the strongest effect of each kind
void EEUnitAreaEffectList::KeepStrongest()
{
    ee_gamecpp_aelist_keep_strongest(EELIST);
}

// Marks every effect in the list for a recount
void EEUnitAreaEffectList::MarkDirty()
{
    ee_gamecpp_aelist_mark_dirty(EELIST);
}

// Moves every effect in the list to a new location and recounts it (inValue unread)
void EEUnitAreaEffectList::MoveAndUpdate(const GE2DPoint& inPoint, float inValue)
{
    ee_gamecpp_aelist_move(EELIST, &inPoint);
}

// Ticks every effect in the list on the unit
void EEUnitAreaEffectList::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    ee_gamecpp_aelist_tick(EELIST, inUnit, inElapsed);
}
