// C:\EEX\Empire Earth\Civilization Builder\EEAEManager.cpp (named by its "fix me in EEAEManager.cpp!" message; folder
// inferred). 0x27adb0..0x27b911: the area-effect manager (vtable 0x44bb40; the game's gAEManager): ctor, ??_G,
// dtor, the unit functor passes (vtables 0x44bb50.., 0x44bd28..), CollectEffects, Load, CreateAreaEffect with the inline
// area-effect kinds it emitted (EEAEMorale's slots and ??_G 0x27b2d7..0x27b335, the immunities' and economy's ??_G and
// slots 0x27b357..0x27b3d2; port: one class, ee/EEAreaEffect.h), the effect queries, the template loads, Tick, the functors' slots, then the template COMDATs
// (vector<EEAreaEffectTemplate*>::push_back 0x27b751, EEGetUnitsInRectangle<EEAEAliveUnit> 0x27b77d).
// EEAEMoraleFromBuildings.cpp starts at 0x27b911. Unit flags: the exe's module set.
#include <windows.h>
#include <string.h>
#include <vector>
#include <functional>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEAECapture.h"
#include "rust/game/screens.h"   // CollectEffects, FindEffect (area_effect_manager.rs)
#include <memory>
#include <stdint.h>

// The unit functors of the manager's passes (RTTI names; each > EEUUUnitFunctor, slot +0x04 only; their ??_G is
// ICF-shared with EEPSuggestRecomputeAllAreaEffects's 0x16b555)
class ApplyGeneratedEffectsToOthersFunctor : public EEUUUnitFunctor {   // vtable 0x44bb50
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->ApplyAreaEffects(); }
};

class SetAffectedByAreaEffectFunctor : public EEUUUnitFunctor {         // vtable 0x44bb60
public:
    SetAffectedByAreaEffectFunctor(EEAreaEffect* inEffect, unsigned long inPlayer, const GE2DPoint& inLocation)
        : mEffect(inEffect), mPlayer(inPlayer), mLocation(inLocation) {}
    virtual void Apply(EEComplexUnit* inUnit);

    EEAreaEffect* mEffect;                  // +0x04
    unsigned long mPlayer;                  // +0x08
    GE2DPoint mLocation;                    // +0x0c
};

class ClearAreaEffectsFunctor : public EEUUUnitFunctor {                // vtable 0x44bb70
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->RemoveAffectingAreaEffects(); }
};

class RebuildAreaEffectRegionListFunctor : public EEUUUnitFunctor {     // vtable 0x44bd28
public:
    RebuildAreaEffectRegionListFunctor(UReferenceContainer<EEAreaEffect*>* inEffects);
    virtual void Apply(EEComplexUnit* inUnit);

    UReferenceContainer<EEAreaEffect*>* mEffects;   // +0x04
};

class RemoveRedundantAreaEffectsFunctor : public EEUUUnitFunctor {      // vtable 0x44bd38
public:
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAffectingAreaEffects.KeepStrongest(); }
};

class UpdateGeneratedAreaEffectsFunctor : public EEUUUnitFunctor {      // vtable 0x44bd48
public:
    UpdateGeneratedAreaEffectsFunctor(float inElapsed) : mElapsed(inElapsed) {}
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->UpdateAreaEffects((unsigned long)this->mElapsed); }

    float mElapsed;                         // +0x04
};

class UpdateAffectedByAreaEffectsFunctor : public EEUUUnitFunctor {     // vtable 0x44bd58
public:
    UpdateAffectedByAreaEffectsFunctor(float inElapsed) : mElapsed(inElapsed) {}
    virtual void Apply(EEComplexUnit* inUnit) { inUnit->mAffectingAreaEffects.Tick(inUnit, this->mElapsed); }

    float mElapsed;                         // +0x04
};

// The living units (expanded into the collector 0x27b77d) (provisional name)
struct EEAEAliveUnit {
    bool operator()(EEComplexUnit* inUnit) const { return inUnit->mHitPoints > 0; }
};

// Creates the area-effect manager with no templates or effects
EEAEManager::EEAEManager()
{
}

// Destroys the manager, releasing every template and effect
EEAEManager::~EEAEManager()
{
    this->Clear();
}

// Lets every unit apply the area effects it generates to the units around it
void EEAEManager::ApplyGeneratedAreaEffects()
{
    ApplyGeneratedEffectsToOthersFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// Adds the list's effects to every living unit they cover around a location (a radius-0 effect: to every
// unit of every player); a healing effect skips its own unit and shows on the units it covers
void EEAEManager::CollectEffects(float inRadius, GE2DPoint& inLocation, unsigned long inPlayerID,
                           EEUnitAreaEffectList* inList, EEComplexUnit* inUnit)
{
    ee_screens_aem_collect(inRadius, inLocation.mX, inLocation.mY, inPlayerID, inList, inUnit);
}

// Adds the effect to a covered unit's affecting effects and applies it
void SetAffectedByAreaEffectFunctor::Apply(EEComplexUnit* inUnit)
{
    if (this->mEffect->Covers(this->mPlayer, inUnit, this->mLocation)) {
        inUnit->mAffectingAreaEffects.Add(this->mEffect);
        this->mEffect->AppliedTo(inUnit);
    }
}

// CollectEffects' and FindEffect's logic: area_effect_manager.rs over the one-statement shims below (UI lane
// round 11)
static std::vector<EEComplexUnit*> sAEMUnits;   // the living units around the location
static U2DRectangle<long> sAEMRect;
static SetAffectedByAreaEffectFunctor sAEMSetAffected(NULL, 0, GE2DPoint(0.0f, 0.0f));   // a radius-0 effect's pass
extern "C" int32_t ee_screenscpp_aem_floor(float inV) { return UMFloor(inV); }
extern "C" void ee_screenscpp_aem_units_clear() { sAEMUnits.clear(); }
extern "C" void ee_screenscpp_aem_rect_top(int32_t inV) { sAEMRect.mTop = inV; }
extern "C" void ee_screenscpp_aem_rect_left(int32_t inV) { sAEMRect.mLeft = inV; }
extern "C" void ee_screenscpp_aem_rect_bottom(int32_t inV) { sAEMRect.mBottom = inV; }
extern "C" void ee_screenscpp_aem_rect_right(int32_t inV) { sAEMRect.mRight = inV; }
extern "C" void ee_screenscpp_aem_collect_alive() { EEGetUnitsInRectangle(sAEMRect, sAEMUnits, EEAEAliveUnit(), true); }
extern "C" uintptr_t ee_screenscpp_aem_units_count() { return sAEMUnits.size(); }
extern "C" uintptr_t ee_screenscpp_aem_unit_complex(uintptr_t inIndex)
{ return (uintptr_t)(EEComplexUnit*)sAEMUnits[inIndex]->mComplexUnit; }
extern "C" uintptr_t ee_screenscpp_aem_list_count(uintptr_t inList)
{ return ((EEUnitAreaEffectList*)inList)->mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aem_list_at(uintptr_t inList, uintptr_t inIndex)
{ return (uintptr_t)((EEUnitAreaEffectList*)inList)->mItems[inIndex]; }
extern "C" bool ee_screenscpp_aem_unit_protected(uintptr_t inUnit) { return ((EEComplexUnit*)inUnit)->IsProtected(); }
extern "C" float ee_screenscpp_aem_effect_radius(uintptr_t inEffect)
{ return ((EEAreaEffect*)inEffect)->mTemplate->mRadius; }
extern "C" int64_t ee_screenscpp_aem_effect_kind(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->mTemplate->mKind; }
extern "C" void ee_screenscpp_aem_set_affected_functor(uintptr_t inEffect, uint64_t inPlayer, float inX, float inY)
{ sAEMSetAffected = SetAffectedByAreaEffectFunctor((EEAreaEffect*)inEffect, (unsigned long)inPlayer, GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_aem_for_each_set_affected() { EEForEachPlayersUnit(&sAEMSetAffected); }
extern "C" bool ee_screenscpp_aem_covers(uintptr_t inEffect, uint64_t inPlayer, uintptr_t inUnit, float inX, float inY)
{ return ((EEAreaEffect*)inEffect)->Covers((unsigned long)inPlayer, (EEComplexUnit*)inUnit, GE2DPoint(inX, inY)); }
extern "C" void ee_screenscpp_aem_affecting_add(uintptr_t inUnit, uintptr_t inEffect)
{ ((EEComplexUnit*)inUnit)->mAffectingAreaEffects.Add((EEAreaEffect*)inEffect); }
extern "C" void ee_screenscpp_aem_applied_to(uintptr_t inEffect, uintptr_t inUnit)
{ ((EEAreaEffect*)inEffect)->AppliedTo((EEComplexUnit*)inUnit); }
extern "C" void ee_screenscpp_aem_show_on(uintptr_t inEffect, uintptr_t inUnit)
{ ((EEAreaEffect*)inEffect)->ShowOn((EEComplexUnit*)inUnit); }
extern "C" uintptr_t ee_screenscpp_aem_effects_count(uintptr_t inManager)
{ return ((EEAEManager*)inManager)->mEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aem_effects_at(uintptr_t inManager, uintptr_t inIndex)
{ return (uintptr_t)((EEAEManager*)inManager)->mEffects.mItems[inIndex]; }
extern "C" bool ee_screenscpp_aem_is_immunity(uintptr_t inEffect) { return ((EEAreaEffect*)inEffect)->IsImmunity(); }
extern "C" int64_t ee_screenscpp_aem_effect_player(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->mPlayer; }
extern "C" bool ee_screenscpp_aem_affects_player(uintptr_t inEffect, int64_t inPlayer, uint64_t inOther)
{ return ((EEAreaEffect*)inEffect)->mTemplate->AffectsPlayer((long)inPlayer, (long)(unsigned long)inOther); }
extern "C" bool ee_screenscpp_aem_affects_player_reversed(uintptr_t inEffect, int64_t inPlayer, uint64_t inOther)
{ return ((EEAreaEffect*)inEffect)->mTemplate->AffectsPlayerReversed((long)inPlayer, (long)(unsigned long)inOther); }
extern "C" bool ee_screenscpp_aem_in_range(uintptr_t inEffect, float inX, float inY)
{ return ((EEAreaEffect*)inEffect)->IsInRange(GE2DPoint(inX, inY)); }

// Clears the affecting area effects of every unit
void EEAEManager::ClearAreaEffects()
{
    ClearAreaEffectsFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// Empties the list of active area effects
void EEAEManager::ClearEffects()
{
    UReferenceContainer<EEAreaEffect*>& theEffects = this->mEffects;
    theEffects.Clear();
    theEffects.mItems.clear();
}

// Reads the area-effect templates from the area-effect file (an empty slot for records with id 0); returns 3
// when a template fails to initialize
long EEAEManager::Load(FSFile* inFile)
{
    return ee_screens_aes_load_templates(this, inFile);
}

// Creates the area effect of the template at an index (its kind's: healing, morale, calamity and conversion immunity,
// morale from buildings, from military heroes, economy from units, cloaking) for a unit; NULL for an unknown kind
EEAreaEffect* EEAEManager::CreateAreaEffect(unsigned long inIndex, EEComplexUnit* inUnit)
{
    EEAreaEffect* theEffect = NULL;
    if (inIndex < this->mTemplates.size()) {
        EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
        if (ee_area_effect_known_kind(theTemplate->mKind))
            theEffect = new EEAreaEffect(theTemplate);
        if (theEffect != NULL) {
            theEffect->SetUnit(inUnit);
            theEffect->mPlayer = inUnit->mPlayer->mPlayerID;
        }
    }
    return theEffect;
}

// Finds an active effect of a kind covering a point that affects the player (immunities only, by the
// reversed player test, in immunity mode), skipping protected units' effects and those in the exclusion list
EEAreaEffect* EEAEManager::FindEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                                    EEUnitAreaEffectList* inExclude)
{
    return static_cast<EEAreaEffect*>(ee_screens_aem_find(this, inKind, inX, inY, inPlayer, inMode, inExclude));
}

// Tells whether a point lies in the effect's scaled area around its location
bool EEAreaEffect::IsInRange(const GE2DPoint& inPoint)
{
    return this->mTemplate->IsWithin(inPoint, this->mLocation, this->GetScale(), NULL);
}

// Tells whether any active effect of the kind covers the point for the player
bool EEAEManager::HasEffect(long inKind, float inX, float inY, unsigned long inPlayer, bool inMode,
                           EEUnitAreaEffectList* inList)
{
    return this->FindEffect(inKind, inX, inY, inPlayer, inMode, inList) != NULL;
}

// Tells whether an effect of the kind covers a tile for the player
bool EEAEManager::EffectAtTile(long inKind, long inX, long inY, long inPlayer, bool inMode)
{
    return this->HasEffect(inKind, (float)inX, (float)inY, inPlayer, inMode, NULL);
}

// Tells whether an effect of the kind covers a tile and affects the player
bool EEAEManager::HasAreaEffect(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, false);
}

// Tells whether an immunity effect of the kind covers a tile against the player
bool EEAEManager::HasAreaEffect2(long inKind, long inX, long inY, long inPlayer)
{
    return this->EffectAtTile(inKind, inX, inY, inPlayer, true);
}

// Returns the effect of the kind that covers a tile and affects the player, or NULL
EEAreaEffect* EEAEManager::FindEffectAtTile(long inKind, long inX, long inY, unsigned long inPlayer)
{
    return this->FindEffect(inKind, (float)inX, (float)inY, inPlayer, false, NULL);
}

// Loads the media of the template at an index, if any
void EEAEManager::LoadTemplateMedia(unsigned long inIndex)
{
    if (inIndex >= this->mTemplates.size())
        return;
    EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
    if (theTemplate != NULL)
        theTemplate->Load(false);
}

// Rebuilds the list of active area effects from every built unit's own effects
void EEAEManager::RebuildAreaEffectRegionList()
{
    RebuildAreaEffectRegionListFunctor theFunctor(&this->mEffects);
    EEForEachPlayersUnit(&theFunctor);
}

// Starts a rebuild of the active effect list by emptying it
RebuildAreaEffectRegionListFunctor::RebuildAreaEffectRegionListFunctor(UReferenceContainer<EEAreaEffect*>* inEffects)
{
    this->mEffects = inEffects;
    inEffects->Clear();
    inEffects->mItems.clear();
}

// Keeps only each unit's strongest affecting effect of each kind
void EEAEManager::RemoveRedundantAreaEffects()
{
    RemoveRedundantAreaEffectsFunctor theFunctor;
    EEForEachPlayersUnit(&theFunctor);
}

// Releases every template and empties the active effect list
void EEAEManager::Clear()
{
    for (std::vector<EEAreaEffectTemplate*>::iterator theIterator = this->mTemplates.begin();
         theIterator != this->mTemplates.end(); ++theIterator) {
        if (*theIterator != NULL)
            (*theIterator)->Dereference();
    }
    this->mTemplates.clear();
    UReferenceContainer<EEAreaEffect*>& theEffects = this->mEffects;
    theEffects.Clear();
    theEffects.mItems.clear();
}

// Unloads the media of the template at an index, if any
void EEAEManager::UnloadTemplateMedia(unsigned long inIndex)
{
    if (inIndex >= this->mTemplates.size())
        return;
    EEAreaEffectTemplate* theTemplate = this->mTemplates[inIndex];
    if (theTemplate != NULL)
        theTemplate->Unload();
}

// Runs one area-effect tick: clear, apply, keep the strongest, update generators, rebuild the list, tick effects
void EEAEManager::Tick(float inElapsed)
{
    this->ClearAreaEffects();
    this->ApplyGeneratedAreaEffects();
    this->RemoveRedundantAreaEffects();
    this->UpdateGeneratedAreaEffects(inElapsed);
    this->RebuildAreaEffectRegionList();
    this->UpdateAffectedByAreaEffects(inElapsed);
    EEAECaptureTick(inElapsed);   // TEST ONLY: AEFX (ee/EEAECapture.h)
}

// Updates the area effects every unit generates for the elapsed time
void EEAEManager::UpdateGeneratedAreaEffects(float inElapsed)
{
    UpdateGeneratedAreaEffectsFunctor theFunctor(inElapsed);
    EEForEachPlayersUnit(&theFunctor);
}

// Ticks every unit's affecting area effects for the elapsed time
void EEAEManager::UpdateAffectedByAreaEffects(float inElapsed)
{
    UpdateAffectedByAreaEffectsFunctor theFunctor(inElapsed);
    EEForEachPlayersUnit(&theFunctor);
}

// Adds a built unit's own area effects to the active effect list
void RebuildAreaEffectRegionListFunctor::Apply(EEComplexUnit* inUnit)
{
    if (!inUnit->mIsBuilt)
        return;
    for (std::vector<EEAreaEffect*>::iterator theIterator = inUnit->mAreaEffects.mItems.begin();
         theIterator != inUnit->mAreaEffects.mItems.end(); ++theIterator)
        this->mEffects->Add(*theIterator);
}

// port (save version 0.256, EEPWorldHelper): the area effects' running state. The original saved none of it: a
// loaded game made each unit's own effects anew (MakeAreaEffects: no location, morale from buildings to be counted)
// and every unit's affecting list and the active list were empty until the next area-effect tick (up to 2 s: no
// morale in the damage, no immunities, no cloaking). Saved: per unit, its own effects' location (and the
// morale-from-buildings count), then the effects acting on it, each as its owner unit and index; then the active list
// the same way (the record and its bytes: ee_tech area_effect_save.rs). Put back at the loaded game's first tick
// (EEAfterLoad).
namespace {
// FSFile's typed reads and writes for the Rust record (the calamities' I/O ops)
long EEAEIoCall(void* inContext, uint32_t inOp, void* ioValue)
{
    FSFile& theFile = *static_cast<FSFile*>(inContext);
    switch (inOp) {
    case EE_CALAMITY_IO_READ_ULONG: return theFile.Read(*static_cast<unsigned long*>(ioValue));
    case EE_CALAMITY_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
    case EE_CALAMITY_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
    case EE_CALAMITY_IO_READ_BOOL: return theFile.Read(*static_cast<bool*>(ioValue));
    case EE_CALAMITY_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
    case EE_CALAMITY_IO_WRITE_BOOL: return theFile.Write(*static_cast<const bool*>(ioValue));
    }
    return 1;
}

}

void EEAESaveRunningState(FSFile& outFile)
{
    ee_screens_aes_save(&outFile, &EEAEIoCall);
}

long EEAELoadRunningState(FSFile& inFile)
{
    return ee_screens_aes_load(&inFile, &EEAEIoCall);
}

// Load's, the save's and the load's fix-up's logic: area_effect_running.rs over the one-statement shims below (UI
// lane round 11)
#define AES_U(u) ((EEComplexUnit*)(u))
#define AES_E(e) ((EEAreaEffect*)(e))
static EEAreaEffectData sAESData;       // the record being read
static unsigned long sAESRead = 0;
extern "C" uintptr_t ee_screenscpp_aes_unit_complex(uintptr_t inUnit)
{ return (uintptr_t)(EEComplexUnit*)((EEUnit*)inUnit)->mComplexUnit; }
extern "C" uintptr_t ee_screenscpp_aes_own_count(uintptr_t inUnit) { return AES_U(inUnit)->mAreaEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aes_own_at(uintptr_t inUnit, uintptr_t inIndex)
{ return (uintptr_t)AES_U(inUnit)->mAreaEffects.mItems[inIndex]; }
extern "C" uintptr_t ee_screenscpp_aes_affecting_count(uintptr_t inUnit)
{ return AES_U(inUnit)->mAffectingAreaEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aes_affecting_at(uintptr_t inUnit, uintptr_t inIndex)
{ return (uintptr_t)AES_U(inUnit)->mAffectingAreaEffects.mItems[inIndex]; }
extern "C" uint64_t ee_screenscpp_aes_unit_id(uintptr_t inUnit) { return (uint64_t)AES_U(inUnit)->GetUnitID(); }
extern "C" int64_t ee_screenscpp_aes_effect_kind(uintptr_t inEffect) { return (int64_t)AES_E(inEffect)->mKind; }
extern "C" float ee_screenscpp_aes_effect_x(uintptr_t inEffect) { return AES_E(inEffect)->mLocation.mX; }
extern "C" float ee_screenscpp_aes_effect_y(uintptr_t inEffect) { return AES_E(inEffect)->mLocation.mY; }
extern "C" bool ee_screenscpp_aes_effect_dirty(uintptr_t inEffect) { return AES_E(inEffect)->mDirty; }
extern "C" int64_t ee_screenscpp_aes_effect_count(uintptr_t inEffect) { return (int64_t)AES_E(inEffect)->mCount; }
extern "C" void ee_screenscpp_aes_set_x(uintptr_t inEffect, float inV) { AES_E(inEffect)->mLocation.mX = inV; }
extern "C" void ee_screenscpp_aes_set_y(uintptr_t inEffect, float inV) { AES_E(inEffect)->mLocation.mY = inV; }
extern "C" void ee_screenscpp_aes_set_dirty(uintptr_t inEffect, bool inV) { AES_E(inEffect)->mDirty = inV; }
extern "C" void ee_screenscpp_aes_set_count(uintptr_t inEffect, int64_t inV) { AES_E(inEffect)->mCount = (long)inV; }
extern "C" uintptr_t ee_screenscpp_aes_world_unit(uint64_t inId)
{ return (uintptr_t)::gWorld.GetUnit((unsigned long)inId); }
extern "C" void ee_screenscpp_aes_affecting_clear(uintptr_t inUnit) { AES_U(inUnit)->mAffectingAreaEffects.Clear(); }
extern "C" void ee_screenscpp_aes_clear_effects() { gAEManager.ClearEffects(); }
extern "C" void ee_screenscpp_aes_add_active(uintptr_t inEffect) { gAEManager.mEffects.Add(AES_E(inEffect)); }
extern "C" void ee_screenscpp_aes_debug_lost(uint64_t inCount)
{ ESDebugOut("EEAEManager: %lu saved area effects are not the loaded ones\n", (unsigned long)inCount); }
extern "C" void ee_screenscpp_aes_after_load(uintptr_t inState)
{ EEAfterLoad([theState = std::shared_ptr<EeAreaEffectRunning>((EeAreaEffectRunning*)inState, ee_area_effect_running_free)]() { ee_screens_aes_restore(theState.get()); }); }
extern "C" void ee_screenscpp_aes_file_open(uintptr_t inFile) { ((FSFile*)inFile)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_aes_file_read_count(uintptr_t inFile, unsigned long* inCount)
{ return ((FSFile*)inFile)->Read(*inCount); }
extern "C" void ee_screenscpp_aes_data_clear() { memset(&sAESData, 0, sizeof(sAESData)); }
extern "C" int32_t ee_screenscpp_aes_data_read(uintptr_t inFile)
{ return ((FSFile*)inFile)->Read(reinterpret_cast<unsigned char*>(&sAESData), sizeof(sAESData), sAESRead); }
extern "C" int64_t ee_screenscpp_aes_data_id() { return (int64_t)sAESData.mID; }
extern "C" uintptr_t ee_screenscpp_aes_template_new() { return (uintptr_t) new EEAreaEffectTemplate; }
extern "C" bool ee_screenscpp_aes_template_init(uintptr_t inT) { return ((EEAreaEffectTemplate*)inT)->Init(sAESData); }
extern "C" void ee_screenscpp_aes_template_error()
{ ESDebugOut("Error reading area effect template %s...fix me in EEAEManager.cpp!\n", sAESData.mName); }
extern "C" void ee_screenscpp_aes_templates_push(uintptr_t inManager, uintptr_t inT)
{ ((EEAEManager*)inManager)->mTemplates.push_back((EEAreaEffectTemplate*)inT); }
extern "C" void ee_screenscpp_aes_file_close(uintptr_t inFile) { ((FSFile*)inFile)->Close(); }
extern "C" uintptr_t ee_screenscpp_aem_manager() { return (uintptr_t)&gAEManager; }
