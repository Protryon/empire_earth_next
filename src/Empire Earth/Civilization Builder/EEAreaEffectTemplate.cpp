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
#include "rust/game/screens.h"   // the template media and list members (area_effect_lists.rs)

// UReferenceContainer<T>::Contains (its COMDAT 0x27c820 closes this unit) is defined in ee/UReferenceContainer.h

// Loads the template's graphic effects and sound media
void EEAreaEffectTemplate::Load(bool inIsOnDemand)
{
    ee_screens_ael_template_load(this, inIsOnDemand);
}

// Unloads the template's graphic effects and sound media
void EEAreaEffectTemplate::Unload()
{
    ee_screens_ael_template_unload(this);
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
    ee_screens_ael_add(this, inEffect);
}

// Lets every effect modify a value of the given kind, showing each effect that applied
void EEUnitAreaEffectList::ApplyModifiers(long inKind, const long& inDamage, long& outDamage, EEComplexUnit* inUnit)
{
    ee_screens_ael_apply_modifiers(this, inKind, &inDamage, &outDamage, inUnit);
}

// Removes the first effect of the given kind; returns whether one was found
bool EEUnitAreaEffectList::Remove(long inKind)
{
    return ee_screens_ael_remove(this, inKind);
}

// Rebuilds the list keeping only the strongest effect of each kind
void EEUnitAreaEffectList::KeepStrongest() { ee_screens_ael_keep_strongest((uintptr_t)this); }

// Marks every effect in the list for a recount
void EEUnitAreaEffectList::MarkDirty()
{
    ee_screens_ael_mark_dirty(this);
}

// Moves every effect in the list to a new location and recounts it
void EEUnitAreaEffectList::MoveAndUpdate(const GE2DPoint& inPoint, float inValue)
{
    ee_screens_ael_move_and_update(this, inPoint.mX, inPoint.mY, inValue);
}

// Ticks every effect in the list on the unit
void EEUnitAreaEffectList::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    ee_screens_ael_tick(this, inUnit, inElapsed);
}

// The template's media and the list members' logic: area_effect_lists.rs over the one-statement shims below (UI lane
// round 11)
#define AEL_L(l) ((EEUnitAreaEffectList*)(l))
#define AEL_E(e) ((EEAreaEffect*)(e))
#define AEL_T(t) ((EEAreaEffectTemplate*)(t))
extern "C" uintptr_t ee_screenscpp_ael_count(uintptr_t inList) { return AEL_L(inList)->mItems.size(); }
extern "C" uintptr_t ee_screenscpp_ael_at(uintptr_t inList, uintptr_t inIndex)
{ return (uintptr_t)AEL_L(inList)->mItems[inIndex]; }
extern "C" void ee_screenscpp_ael_remove(uintptr_t inList, uintptr_t inEffect)
{ AEL_L(inList)->UReferenceContainer<EEAreaEffect*>::Remove(AEL_E(inEffect)); }
extern "C" void ee_screenscpp_ael_add(uintptr_t inList, uintptr_t inEffect)
{ AEL_L(inList)->UReferenceContainer<EEAreaEffect*>::Add(AEL_E(inEffect)); }
extern "C" void ee_screenscpp_ael_set_location(uintptr_t inEffect, float inX, float inY)
{ AEL_E(inEffect)->mLocation = GE2DPoint(inX, inY); }
extern "C" uintptr_t ee_screenscpp_ael_t_effect(uintptr_t inT) { return (uintptr_t)AEL_T(inT)->mGraphicEffect; }
extern "C" uintptr_t ee_screenscpp_ael_t_effect18(uintptr_t inT) { return (uintptr_t)AEL_T(inT)->mGraphicEffect18; }
extern "C" uintptr_t ee_screenscpp_ael_t_media(uintptr_t inT) { return (uintptr_t)AEL_T(inT)->mMedia; }
extern "C" void ee_screenscpp_ael_effect_load(uintptr_t inEffect, bool inOnDemand)
{ ((EEGraphicEffect*)inEffect)->Load(inOnDemand); }
extern "C" void ee_screenscpp_ael_effect_unload(uintptr_t inEffect) { ((EEGraphicEffect*)inEffect)->Unload(); }
extern "C" void ee_screenscpp_ael_media_load(uintptr_t inMedia) { ((EEMedia*)inMedia)->LoadMedia(); }
extern "C" void ee_screenscpp_ael_media_unload(uintptr_t inMedia) { ((EEMedia*)inMedia)->UnloadMedia(); }
extern "C" uintptr_t ee_screenscpp_ael_list_new() { return (uintptr_t)new EEUnitAreaEffectList; }
extern "C" void ee_screenscpp_ael_list_assign(uintptr_t inList, uintptr_t inOther) { *AEL_L(inList) = *AEL_L(inOther); }
extern "C" void ee_screenscpp_ael_list_free(uintptr_t inList) { delete AEL_L(inList); }
