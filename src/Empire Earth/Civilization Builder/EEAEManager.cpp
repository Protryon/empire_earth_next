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
#include "ee/EESaveGameFiles.h"
#include "rust/game/screens.h"   // CollectEffects, FindEffect (area_effect_manager.rs)
#include <memory>
#include <stdint.h>
#include "ee/EEScreensShims.h"
#include <stdio.h>
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGameVariant.h"
#include "ee/EERules.h"
#include "ee/EEAIContinentManager.h"
#include "rust/sim/units.h"
#include "rust/game/screens.h"   // the host (area_effect_host.rs)
#include "lle/FSMemoryFile.h"
#include "rust/game/gamecpp.h"
#include "rust/render/gfx.h"

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

// CollectEffects' and FindEffect's logic: area_effect_manager.rs over the one-statement shims below (UI lane
// round 11)
extern "C" void ee_screenscpp_aem_units_clear() { EEScratch<std::vector<EEComplexUnit*>>(kScratchAEMUnits).clear(); }
extern "C" void ee_screenscpp_aem_rect_top(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAEMRect).mTop = inV; }
extern "C" void ee_screenscpp_aem_rect_left(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAEMRect).mLeft = inV; }
extern "C" void ee_screenscpp_aem_rect_bottom(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAEMRect).mBottom = inV; }
extern "C" void ee_screenscpp_aem_rect_right(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAEMRect).mRight = inV; }
extern "C" void ee_screenscpp_aem_collect_alive() { EEGetUnitsInRectangle(EEScratch<U2DRectangle<long>>(kScratchAEMRect), EEScratch<std::vector<EEComplexUnit*>>(kScratchAEMUnits), EEAEAliveUnit(), true); }
extern "C" uintptr_t ee_screenscpp_aem_units_count() { return EEScratch<std::vector<EEComplexUnit*>>(kScratchAEMUnits).size(); }
extern "C" uintptr_t ee_screenscpp_aem_unit_complex(uintptr_t inIndex)
{ return (uintptr_t)(EEComplexUnit*)EEScratch<std::vector<EEComplexUnit*>>(kScratchAEMUnits)[inIndex]->mComplexUnit; }
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

// port (save version 0.256, EEPWorldHelper): the area effects' running state. The original saved none of it: a
// loaded game made each unit's own effects anew (MakeAreaEffects: no location, morale from buildings to be counted)
// and every unit's affecting list and the active list were empty until the next area-effect tick (up to 2 s: no
// morale in the damage, no immunities, no cloaking). Saved: per unit, its own effects' location (and the
// morale-from-buildings count), then the effects acting on it, each as its owner unit and index; then the active list
// the same way (the record and its bytes: ee_tech area_effect_save.rs; the FSFile's reads and writes: ee_screens
// civ_whole_ffi.rs, ee_screens_aes_save and _load; the entry points are inline in ee/EEAEManager.h). Put back at the
// loaded game's first tick (EEAfterLoad).

// Load's, the save's and the load's fix-up's logic: area_effect_running.rs over the one-statement shims below (UI
// lane round 11)
#define AES_U(u) ((EEComplexUnit*)(u))
#define AES_E(e) ((EEAreaEffect*)(e))
extern "C" uintptr_t ee_screenscpp_aes_own_count(uintptr_t inUnit) { return AES_U(inUnit)->mAreaEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aes_own_at(uintptr_t inUnit, uintptr_t inIndex)
{ return (uintptr_t)AES_U(inUnit)->mAreaEffects.mItems[inIndex]; }
extern "C" uintptr_t ee_screenscpp_aes_affecting_count(uintptr_t inUnit)
{ return AES_U(inUnit)->mAffectingAreaEffects.mItems.size(); }
extern "C" uintptr_t ee_screenscpp_aes_affecting_at(uintptr_t inUnit, uintptr_t inIndex)
{ return (uintptr_t)AES_U(inUnit)->mAffectingAreaEffects.mItems[inIndex]; }
extern "C" float ee_screenscpp_aes_effect_x(uintptr_t inEffect) { return AES_E(inEffect)->mLocation.mX; }
extern "C" float ee_screenscpp_aes_effect_y(uintptr_t inEffect) { return AES_E(inEffect)->mLocation.mY; }
extern "C" void ee_screenscpp_aes_set_x(uintptr_t inEffect, float inV) { AES_E(inEffect)->mLocation.mX = inV; }
extern "C" void ee_screenscpp_aes_set_y(uintptr_t inEffect, float inV) { AES_E(inEffect)->mLocation.mY = inV; }
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
extern "C" void ee_screenscpp_aes_data_clear() { memset(&EEScratch<EEAreaEffectData>(kScratchAESData), 0, sizeof(EEScratch<EEAreaEffectData>(kScratchAESData))); }
extern "C" int32_t ee_screenscpp_aes_data_read(uintptr_t inFile)
{ return ((FSFile*)inFile)->Read(reinterpret_cast<unsigned char*>(&EEScratch<EEAreaEffectData>(kScratchAESData)), sizeof(EEScratch<EEAreaEffectData>(kScratchAESData)), EEScratch<unsigned long>(kScratchAESRead)); }
extern "C" int64_t ee_screenscpp_aes_data_id() { return (int64_t)EEScratch<EEAreaEffectData>(kScratchAESData).mID; }
extern "C" uintptr_t ee_screenscpp_aes_template_new() { return (uintptr_t) new EEAreaEffectTemplate; }
extern "C" bool ee_screenscpp_aes_template_init(uintptr_t inT) { return ((EEAreaEffectTemplate*)inT)->Init(EEScratch<EEAreaEffectData>(kScratchAESData)); }
extern "C" void ee_screenscpp_aes_template_error()
{ ESDebugOut("Error reading area effect template %s...fix me in EEAEManager.cpp!\n", EEScratch<EEAreaEffectData>(kScratchAESData).mName); }
extern "C" void ee_screenscpp_aes_templates_push(uintptr_t inManager, uintptr_t inT)
{ ((EEAEManager*)inManager)->mTemplates.push_back((EEAreaEffectTemplate*)inT); }
extern "C" void ee_screenscpp_aes_file_close(uintptr_t inFile) { ((FSFile*)inFile)->Close(); }
extern "C" uintptr_t ee_screenscpp_aem_manager() { return (uintptr_t)&gAEManager; }

// the manager's passes, Tick, Clear and CreateAreaEffect: area_effect_obj.rs over these (UI lane round 12)
#define AEM_U(u) ((EEComplexUnit*)(u))
#define AEM_M(m) ((EEAEManager*)(m))
extern "C" void ee_screenscpp_aem_for_each(uint32_t inPass, uintptr_t inArg, uint64_t inPlayer, float inElapsed, float inX, float inY)
{ EEForEachPlayersUnit(EEAETemp(EEAEPassFunctor(inPass, inArg, inPlayer, inElapsed, inX, inY))); }
extern "C" void ee_screenscpp_aem_apply_area_effects(uintptr_t inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_APPLY_AREA_EFFECTS, AEM_U(inUnit), 0, 0, 0); }
extern "C" void ee_screenscpp_aem_remove_affecting(uintptr_t inUnit) { ee_units_cu_body(EE_UNITS_CU_DO_REMOVE_AFFECTING, AEM_U(inUnit), 0, 0, 0); }
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
extern "C" uintptr_t ee_screenscpp_aem_effect_new(uintptr_t inT)
{ return (uintptr_t)new EEAreaEffect((EEAreaEffectTemplate*)inT); }
extern "C" void ee_screenscpp_aem_set_effect_player(uintptr_t inEffect, uintptr_t inUnit)
{ ((EEAreaEffect*)inEffect)->mPlayer = AEM_U(inUnit)->mPlayer->mPlayerID; }

// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffect.cpp (name from RTTI EEAreaEffect; folder inferred: no
// __FILE__; the area-effect units sit between EEWonNetAvailableGameFormManager.cpp and the civilization builder's).
// 0x27bf4b..0x27c0f3: the base area effect (vtable 0x44be24): ctor, ??_G, dtor, the slots it implements and the
// scale helper. EEAreaEffectTemplate.cpp starts at 0x27c0f3. Unit flags: the exe's module set.
//
// port (calamities lane, round 3): the eight kinds' classes are this one class over ee_tech area_effect.rs (their
// units EEAECloaking.cpp 0x27aa0b.., EEAEEconomy.cpp 0x27ac2c.., EEAEHealing.cpp 0x27ace1..,
// EEAEMoraleFromBuildings.cpp 0x27b911.., EEAEMoraleFromMilitaryHeroes.cpp 0x27bf0a.. and the inline kinds of
// EEAEManager.cpp are gone). UI lane round 12: the members are Rust's (ee_game screens area_effect_obj.rs), each a
// forward; the world calls of the simulation are area_effect_host.rs' over the shims below, in the originals' order.
// The template's members that ask the host (its ctor, dtor, player and area tests, Init: 0x27c0f3..0x27c5ab) too.
namespace {

// The owner's living, built morale buildings (combat.yaml morale buildings: the house, 0x25, in the original) whose
// footprint meets mRect (0x27bc86, the overlap 0x27bcc1) (provisional name)
struct EEAEMFBBuildingInRectangle {
    EEAEMFBBuildingInRectangle(const U2DRectangle<long>& inRect, EEPlayer* inPlayer) : mRect(inRect), mPlayer(inPlayer) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && EEGetRules().mCombat.IsMoraleBuilding(inUnit->mUnitClass->mUnitType) && inUnit->mHitPoints > 0 &&
               inUnit->mIsBuilt && this->Overlaps(inUnit);
    }
    bool Overlaps(EEComplexUnit* inUnit) const
    {
        return this->mRect.mLeft <= inUnit->mTileX + inUnit->mUnitClass->mTileSize - 1 && this->mRect.mRight >= inUnit->mTileX &&
               this->mRect.mTop <= inUnit->mTileY + inUnit->mUnitClass->mTileSize - 1 && this->mRect.mBottom >= inUnit->mTileY;
    }

    U2DRectangle<long> mRect;
    EEPlayer* mPlayer;
};

// The same buildings within mRangeSquared of mCenter (0x27be9a, the distance test 0x27bed5) (provisional name)
struct EEAEMFBBuildingInRange {
    EEAEMFBBuildingInRange(const U2DPoint<long>& inCenter, EEPlayer* inPlayer, float inRangeSquared)
        : mCenter(inCenter), mPlayer(inPlayer), mRangeSquared(inRangeSquared) {}
    bool operator()(EEComplexUnit* inUnit) const
    {
        return inUnit->mPlayer == this->mPlayer && EEGetRules().mCombat.IsMoraleBuilding(inUnit->mUnitClass->mUnitType) && inUnit->mHitPoints > 0 &&
               inUnit->mIsBuilt && this->InRange(inUnit);
    }
    bool InRange(EEComplexUnit* inUnit) const
    {
        return this->mCenter.DistanceToSquaredF(U2DPoint<long>(inUnit->mTileX, inUnit->mTileY)) <= this->mRangeSquared;
    }

    U2DPoint<long> mCenter;
    EEPlayer* mPlayer;
    float mRangeSquared;
};

}   // namespace

#define AE_T(t) ((EEComplexUnit*)(t))
#define AE_P(p) ((EEPlayer*)(p))
#define AE_E(e) ((EEAreaEffect*)(e))
extern "C" uintptr_t ee_screenscpp_aeh_tech_node(int32_t inPlayer, uintptr_t inT)
{ return (uintptr_t)gWorld.mPlayers[inPlayer]->GetTechTree()->Get(AE_T(inT)->GetUnitClass()->mTechnology); }
extern "C" bool ee_screenscpp_aeh_allied(uintptr_t inPlayer, uintptr_t inOther)
{ return AE_P(inPlayer)->IsAlliedWith(AE_P(inOther)); }
extern "C" bool ee_screenscpp_aeh_repairable(uintptr_t inT) { return AE_T(inT)->IsRepairable(); }
extern "C" void ee_screenscpp_aeh_tile_x(int32_t inV) { EEScratch<EETileCoordinate>(kScratchAETile).mX = inV; }
extern "C" void ee_screenscpp_aeh_tile_y(int32_t inV) { EEScratch<EETileCoordinate>(kScratchAETile).mY = inV; }
extern "C" int32_t ee_screenscpp_aeh_continent() { return gContinentManager.GetContinent(EEScratch<EETileCoordinate>(kScratchAETile), kAIPathNonSpace); }
extern "C" void ee_screenscpp_aeh_set_hit_points(uintptr_t inT, int32_t inV) { AE_T(inT)->SetHitPoints(inV); }
extern "C" void ee_screenscpp_aeh_cloak(uintptr_t inT) { AE_T(inT)->SetCloakableChain(true); }
extern "C" bool ee_screenscpp_aeh_idle(uintptr_t inT) { return AE_T(inT)->GetCommandType() == kActionIdle; }
extern "C" void ee_screenscpp_aeh_set_cloakable_own(uintptr_t inT)
{ AE_T(inT)->SetCloakableChain(AE_T(inT)->mUnitClass->mIsCloakable); }
extern "C" void ee_screenscpp_aeh_minimap_hide(uintptr_t inT)
{ ee_gamecpp_minimap_move_unit(gMiniMap, AE_T(inT), -10000, (long)(-10000), AE_T(inT)->mTileX, AE_T(inT)->mTileY); }
extern "C" uintptr_t ee_screenscpp_aeh_owner(uintptr_t inEffect) { return (uintptr_t)AE_E(inEffect)->mUnit; }
extern "C" void ee_screenscpp_aeh_rect_top(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAERect).mTop = inV; }
extern "C" void ee_screenscpp_aeh_rect_left(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAERect).mLeft = inV; }
extern "C" void ee_screenscpp_aeh_rect_right(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAERect).mRight = inV; }
extern "C" void ee_screenscpp_aeh_rect_bottom(int32_t inV) { EEScratch<U2DRectangle<long>>(kScratchAERect).mBottom = inV; }
extern "C" void ee_screenscpp_aeh_units_clear() { EEScratch<std::vector<EEComplexUnit*>>(kScratchAEUnits).clear(); }
extern "C" int32_t ee_screenscpp_aeh_count_in_rect(uintptr_t inEffect)
{ return EEGetUnitsInRectangle(EEScratch<U2DRectangle<long>>(kScratchAERect), EEScratch<std::vector<EEComplexUnit*>>(kScratchAEUnits), EEAEMFBBuildingInRectangle(EEScratch<U2DRectangle<long>>(kScratchAERect), gWorld.mPlayers[AE_E(inEffect)->mPlayer]), true); }
extern "C" int32_t ee_screenscpp_aeh_count_in_range(uintptr_t inEffect, int32_t inX, int32_t inY, float inRange)
{ return EEGetUnitsInRectangle(EEScratch<U2DRectangle<long>>(kScratchAERect), EEScratch<std::vector<EEComplexUnit*>>(kScratchAEUnits), EEAEMFBBuildingInRange(U2DPoint<long>(inX, inY), gWorld.mPlayers[AE_E(inEffect)->mPlayer], inRange), true); }

// the effect's and the template's C++ objects for area_effect_obj.rs (UI lane round 12)
extern "C" uintptr_t ee_screenscpp_ae_fields(uintptr_t inEffect)
{ return (uintptr_t)static_cast<EeAreaEffectFields*>(AE_E(inEffect)); }
extern "C" uintptr_t ee_screenscpp_ae_template(uintptr_t inEffect) { return (uintptr_t)AE_E(inEffect)->mTemplate; }
extern "C" uintptr_t ee_screenscpp_ae_template_fields(uintptr_t inT)
{ return (uintptr_t)static_cast<EeAreaEffectTemplateFields*>((EEAreaEffectTemplate*)inT); }
extern "C" void ee_screenscpp_ae_set_template(uintptr_t inEffect, uintptr_t inT)
{ AE_E(inEffect)->mTemplate = (EEAreaEffectTemplate*)inT; }
extern "C" void ee_screenscpp_ae_template_reference(uintptr_t inT) { ((EEAreaEffectTemplate*)inT)->Reference(); }
extern "C" void ee_screenscpp_ae_template_dereference(uintptr_t inT) { ((EEAreaEffectTemplate*)inT)->Dereference(); }
extern "C" void ee_screenscpp_ae_set_unit(uintptr_t inEffect, uintptr_t inUnit)
{ AE_E(inEffect)->mUnit = AE_T(inUnit); }
extern "C" void ee_screenscpp_ae_unit_reference(uintptr_t inUnit) { AE_T(inUnit)->Reference(); }
extern "C" void ee_screenscpp_ae_unit_dereference(uintptr_t inUnit) { AE_T(inUnit)->Dereference(); }
extern "C" int64_t ee_screenscpp_ae_morale_bonus() { return (int64_t)gWorld.GetGameVariant()->mMoraleBonus; }
extern "C" void ee_screenscpp_ae_create_unit_effect(uintptr_t inUnit, uintptr_t inEffect)
{ gWorld.mEffectManager->CreateUnitEffect((EEUnit*)inUnit, (EEGraphicEffect*)inEffect, NULL, false, 0, 0); }
extern "C" void ee_screenscpp_aet_set_effect(uintptr_t inT, uintptr_t inEffect)
{ ((EEAreaEffectTemplate*)inT)->mGraphicEffect = (EEGraphicEffect*)inEffect; }
extern "C" void ee_screenscpp_aet_set_effect18(uintptr_t inT, uintptr_t inEffect)
{ ((EEAreaEffectTemplate*)inT)->mGraphicEffect18 = (EEGraphicEffect*)inEffect; }
extern "C" void ee_screenscpp_aet_set_media(uintptr_t inT, uintptr_t inMedia)
{ ((EEAreaEffectTemplate*)inT)->mMedia = (EEMedia*)inMedia; }
extern "C" uintptr_t ee_screenscpp_ae_graphic_effect_get(int32_t inId) { return (uintptr_t)EEGraphicEffect::Get(inId); }
extern "C" void ee_screenscpp_ae_graphic_effect_reference(uintptr_t inEffect)
{ ((EEGraphicEffect*)inEffect)->Reference(); }
extern "C" void ee_screenscpp_ae_graphic_effect_dereference(uintptr_t inEffect)
{ ((EEGraphicEffect*)inEffect)->Dereference(); }
extern "C" uintptr_t ee_screenscpp_ae_media_get(int32_t inId) { return (uintptr_t)EEMedia::Get(inId); }

// Writes the effect's location, owning unit and player to the out-of-synch dump
void EEAreaEffect::Dump(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location X: %6.6f\n", (double)this->mLocation.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location Y: %6.6f\n", (double)this->mLocation.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Owning Unit ID #%08lx\n", this->mUnit != NULL ? this->mUnit->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Player Number %ld\n", this->mPlayer);
}

long EEAreaEffect::Checksum()
{
    return ee_area_effect_checksum(this, this->mLocation.mX, this->mLocation.mY, this->mUnit != NULL ? (long)this->mUnit->mUnitID : 0);
}

// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffectTemplate.cpp (name from RTTI EEAreaEffectTemplate; folder
// inferred: no __FILE__). 0x27c0f3..0x27c8b9: the area-effect template (vtable 0x44be78; EEAEManager::Load 0x27b079
// makes one per record of the area-effect file): ctor, ??_G, dtor, the player-relation and range tests, Init (port:
// in EEAreaEffect.cpp, over its host), Load/Unload; then a unit's area-effect list (EEUnitAreaEffectList) members and the UReferenceContainer<EEAreaEffect*>
// template COMDATs they instantiate (Remove 0x27c7f2, Contains 0x27c820 (ICF-shared with <EEComplexUnit*>), 0x27c873
// (its end()), Erase 0x27c87f). EECBCivilizationCreator.cpp starts at 0x27c8b9. Unit flags: the exe's module set.
// UReferenceContainer<T>::Contains (its COMDAT 0x27c820 closes this unit) is defined in ee/UReferenceContainer.h

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

// New (port), TEST ONLY: the area effects' records of the simulation capture (tools/sim_capture.py,
// every profile). Reads the world; writes nothing to it. UI lane round 12: the records are Rust's (ee_game screens
// area_effect_capture.rs) over the one-statement shims below; EEAEManager's tick calls ee_screens_ae_capture_tick.
//   AETP  every area-effect template as Init left it (once, at the first area-effect tick): index, the numbers, the
//         graphic effects and media by table index
//   AEFX  the area effects after each EEAEManager::Tick: the elapsed time; per unit (players in order, each list in
//         order) that owns or is affected by an effect: its id, hit points and cloakable flag, each own effect's kind,
//         player, location, checksum, strength, radius, scale, immunity, the three modifiers of a probe value, the
//         morale count, and its Covers of its own unit; each affecting effect as its owner and index; then the active
//         list the same way and the running state's save bytes (EEAESaveRunningState)
extern "C" int64_t ee_screenscpp_aec_checksum(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->Checksum(); }
extern "C" uintptr_t ee_screenscpp_aec_template_count() { return gAEManager.mTemplates.size(); }
extern "C" uintptr_t ee_screenscpp_aec_template_at(uintptr_t inIndex)
{ return (uintptr_t)gAEManager.mTemplates[inIndex]; }
extern "C" int32_t ee_screenscpp_aec_effect_index(uintptr_t inEffect)
{ return (int32_t)ee_gfx_geffect_index_of(static_cast<const EEGraphicEffect*>((EEGraphicEffect*)inEffect)); }
extern "C" uint64_t ee_screenscpp_aec_media_count() { return (uint64_t)EEMedia::GetCount(); }
extern "C" uintptr_t ee_screenscpp_aec_media_at(uint64_t inIndex)
{ return (uintptr_t)EEMedia::Get((unsigned long)inIndex); }
extern "C" uintptr_t ee_screenscpp_aec_mem_new() { return (uintptr_t)new FSMemoryFile; }
extern "C" void ee_screenscpp_aec_mem_save(uintptr_t inFile) { EEAESaveRunningState(*(FSMemoryFile*)inFile); }
extern "C" void ee_screenscpp_aec_mem_position(uintptr_t inFile, long* inOut)
{ ((FSMemoryFile*)inFile)->GetPosition(*inOut); }
extern "C" void ee_screenscpp_aec_mem_bytes(uintptr_t inFile, void* inOut, uintptr_t inLength)
{ ee_screens_shim_out(inOut, ((FSMemoryFile*)inFile)->GetData(), inLength); }
extern "C" void ee_screenscpp_aec_mem_free(uintptr_t inFile) { delete (FSMemoryFile*)inFile; }
