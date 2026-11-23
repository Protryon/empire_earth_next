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

// The unit functors of the manager's passes (the original's ApplyGeneratedEffectsToOthersFunctor,
// SetAffectedByAreaEffectFunctor, ClearAreaEffectsFunctor, RebuildAreaEffectRegionListFunctor,
// RemoveRedundantAreaEffectsFunctor, UpdateGeneratedAreaEffectsFunctor, UpdateAffectedByAreaEffectsFunctor, each
// > EEUUUnitFunctor, slot +0x04 only): one functor whose Apply is the pass's, Rust's (area_effect_obj.rs pass; UI lane
// round 12)
class EEAEPassFunctor : public EEUUUnitFunctor {
public:
    EEAEPassFunctor(uint32_t inPass, uintptr_t inArg, uint64_t inPlayer, float inElapsed, float inX, float inY)
        : mPass(inPass), mArg(inArg), mPlayer(inPlayer), mElapsed(inElapsed), mX(inX), mY(inY) {}
    virtual void Apply(EEComplexUnit* inUnit)
    {
        ee_screens_aem_pass(this->mPass, this->mArg, this->mPlayer, this->mElapsed, this->mX, this->mY, (uintptr_t)inUnit);
    }
    uint32_t mPass;
    uintptr_t mArg;
    uint64_t mPlayer;
    float mElapsed, mX, mY;
};
// a temporary's address for the full expression (the walker's functor argument)
template <class T> static T* EEAETemp(T&& inValue) { return &inValue; }

// The living units (expanded into the collector 0x27b77d) (provisional name)
struct EEAEAliveUnit {
    bool operator()(EEComplexUnit* inUnit) const { return inUnit->mHitPoints > 0; }
};

// Creates the area-effect manager with no templates or effects
EEAEManager::EEAEManager()
{
}

// CollectEffects' and FindEffect's logic: area_effect_manager.rs over the one-statement shims below (UI lane
// round 11)
static std::vector<EEComplexUnit*> sAEMUnits;   // the living units around the location
static U2DRectangle<long> sAEMRect;
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
extern "C" void ee_screenscpp_aem_affecting_add(uintptr_t inUnit, uintptr_t inEffect)
{ ((EEComplexUnit*)inUnit)->mAffectingAreaEffects.Add((EEAreaEffect*)inEffect); }
extern "C" uintptr_t ee_screenscpp_aem_effects_count(uintptr_t inManager)
{ return ((EEAEManager*)inManager)->mEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aem_effects_at(uintptr_t inManager, uintptr_t inIndex)
{ return (uintptr_t)((EEAEManager*)inManager)->mEffects.mItems[inIndex]; }
extern "C" int64_t ee_screenscpp_aem_effect_player(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->mPlayer; }

// Tells whether a point lies in the effect's scaled area around its location
bool EEAreaEffect::IsInRange(const GE2DPoint& inPoint)
{
    return this->mTemplate->IsWithin(inPoint, this->mLocation, this->GetScale(), NULL);
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

// the manager's passes, Tick, Clear and CreateAreaEffect: area_effect_obj.rs over these (UI lane round 12)
#define AEM_U(u) ((EEComplexUnit*)(u))
#define AEM_M(m) ((EEAEManager*)(m))
extern "C" void ee_screenscpp_aem_for_each(uint32_t inPass, uintptr_t inArg, uint64_t inPlayer, float inElapsed, float inX, float inY)
{ EEForEachPlayersUnit(EEAETemp(EEAEPassFunctor(inPass, inArg, inPlayer, inElapsed, inX, inY))); }
extern "C" void ee_screenscpp_aem_apply_area_effects(uintptr_t inUnit) { AEM_U(inUnit)->ApplyAreaEffects(); }
extern "C" void ee_screenscpp_aem_remove_affecting(uintptr_t inUnit) { AEM_U(inUnit)->RemoveAffectingAreaEffects(); }
extern "C" uintptr_t ee_screenscpp_aem_affecting_list(uintptr_t inUnit)
{ return (uintptr_t)&AEM_U(inUnit)->mAffectingAreaEffects; }
extern "C" void ee_screenscpp_aem_update_area_effects(uintptr_t inUnit, float inElapsed)
{ AEM_U(inUnit)->UpdateAreaEffects((unsigned long)inElapsed); }
extern "C" void ee_screenscpp_aem_effects_release(uintptr_t inManager) { AEM_M(inManager)->mEffects.Clear(); }
extern "C" void ee_screenscpp_aem_effects_erase(uintptr_t inManager) { AEM_M(inManager)->mEffects.mItems.clear(); }
extern "C" void ee_screenscpp_aem_effects_add(uintptr_t inManager, uintptr_t inEffect)
{ AEM_M(inManager)->mEffects.Add((EEAreaEffect*)inEffect); }
extern "C" uintptr_t ee_screenscpp_aem_templates_count(uintptr_t inManager)
{ return AEM_M(inManager)->mTemplates.size(); }
extern "C" uintptr_t ee_screenscpp_aem_template_at(uintptr_t inManager, uintptr_t inIndex)
{ return (uintptr_t)AEM_M(inManager)->mTemplates[inIndex]; }
extern "C" void ee_screenscpp_aem_templates_clear(uintptr_t inManager) { AEM_M(inManager)->mTemplates.clear(); }
extern "C" void ee_screenscpp_aem_capture_tick(float inElapsed) { EEAECaptureTick(inElapsed); }
extern "C" uintptr_t ee_screenscpp_aem_effect_new(uintptr_t inT)
{ return (uintptr_t)new EEAreaEffect((EEAreaEffectTemplate*)inT); }
extern "C" void ee_screenscpp_aem_set_effect_player(uintptr_t inEffect, uintptr_t inUnit)
{ ((EEAreaEffect*)inEffect)->mPlayer = AEM_U(inUnit)->mPlayer->mPlayerID; }
