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
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitCollector.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGameVariant.h"
#include "ee/EERules.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "rust/sim/units.h"
#include "rust/game/screens.h"   // the host (area_effect_host.rs)
#include "ee/EEScreensShims.h"
#include <algorithm>
#include <string.h>
#include "ee/EEAEManager.h"
#include "lle/FSMemoryFile.h"


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
extern "C" bool ee_screenscpp_aeh_set_contains(uintptr_t inT, int32_t inSet)
{ return gUnitSetManager.Contains(AE_T(inT)->GetUnitClass()->GetUnitType(), inSet); }
extern "C" uintptr_t ee_screenscpp_aeh_tech_node(int32_t inPlayer, uintptr_t inT)
{ return (uintptr_t)gWorld.mPlayers[inPlayer]->GetTechTree()->Get(AE_T(inT)->GetUnitClass()->mTechnology); }
extern "C" int64_t ee_screenscpp_aeh_node_kind(uintptr_t inNode)
{ return (int64_t)((EETechTreeNode*)inNode)->mData->mKind; }
extern "C" uintptr_t ee_screenscpp_aeh_player(int32_t inIndex) { return (uintptr_t)gWorld.mPlayers[inIndex]; }
extern "C" bool ee_screenscpp_aeh_allied(uintptr_t inPlayer, uintptr_t inOther)
{ return AE_P(inPlayer)->IsAlliedWith(AE_P(inOther)); }
extern "C" bool ee_screenscpp_aeh_enemy(uintptr_t inPlayer, uintptr_t inOther)
{ return AE_P(inPlayer)->mEnemies[AE_P(inOther)->mPlayerID] != 0; }
extern "C" bool ee_screenscpp_aeh_built(uintptr_t inT) { return AE_T(inT)->mIsBuilt; }
extern "C" bool ee_screenscpp_aeh_repairable(uintptr_t inT) { return AE_T(inT)->IsRepairable(); }
extern "C" float ee_screenscpp_aeh_position_x(uintptr_t inT)
{ return AE_T(inT)->mPhysicalModel.GetLocation().GetPosition().GetX(); }
extern "C" float ee_screenscpp_aeh_position_y(uintptr_t inT)
{ return AE_T(inT)->mPhysicalModel.GetLocation().GetPosition().GetY(); }
extern "C" void ee_screenscpp_aeh_tile_x(int32_t inV) { EEScratch<EETileCoordinate>(kScratchAETile).mX = inV; }
extern "C" void ee_screenscpp_aeh_tile_y(int32_t inV) { EEScratch<EETileCoordinate>(kScratchAETile).mY = inV; }
extern "C" int32_t ee_screenscpp_aeh_continent() { return gContinentManager.GetContinent(EEScratch<EETileCoordinate>(kScratchAETile), kAIPathNonSpace); }
extern "C" uintptr_t ee_screenscpp_aeh_carrier(uintptr_t inT)
{ return (uintptr_t)(EEComplexUnit*)AE_T(inT)->mComplexUnit; }
extern "C" uint64_t ee_screenscpp_aeh_unit_type(uintptr_t inT)
{ return (uint64_t)AE_T(inT)->mUnitClass->GetUnitType(); }
extern "C" int32_t ee_screenscpp_aeh_planet(uintptr_t inCarrier) { return (int)AE_T(inCarrier)->mPlanetID; }
extern "C" int32_t ee_screenscpp_aeh_hit_points(uintptr_t inT) { return AE_T(inT)->mHitPoints; }
extern "C" int32_t ee_screenscpp_aeh_max_hit_points(uintptr_t inT) { return AE_T(inT)->mStats->mMaxHitPoints; }
extern "C" bool ee_screenscpp_aeh_in_cinematic() { return gWorld.mInCinematic; }
extern "C" void ee_screenscpp_aeh_set_hit_points(uintptr_t inT, int32_t inV) { AE_T(inT)->SetHitPoints(inV); }
extern "C" void ee_screenscpp_aeh_cloak(uintptr_t inT) { AE_T(inT)->SetCloakableChain(true); }
extern "C" bool ee_screenscpp_aeh_has_camouflage(uintptr_t inT)
{ return gCivPowersManager.HasPower(AE_T(inT)->GetPlayer(), kCivPowerCamouflage); }
extern "C" bool ee_screenscpp_aeh_camouflage_set_exists()
{ return gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) != NULL; }
extern "C" bool ee_screenscpp_aeh_camouflage_set_contains(uintptr_t inT)
{ return gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)->Contains(AE_T(inT)->mUnitClass->mUnitType); }
extern "C" bool ee_screenscpp_aeh_idle(uintptr_t inT) { return AE_T(inT)->GetCommandType() == kActionIdle; }
extern "C" uint64_t ee_screenscpp_aeh_camouflage_timer(uintptr_t inT) { return (uint64_t)AE_T(inT)->mCamouflageTimer; }
extern "C" int64_t ee_screenscpp_aeh_u32c(uintptr_t inT) { return (int64_t)AE_T(inT)->mU32C; }
extern "C" uintptr_t ee_screenscpp_aeh_current_player() { return (uintptr_t)(gWorld.mPlayers[gWorld.mCurrentPlayer]); }
extern "C" void ee_screenscpp_aeh_set_cloakable_own(uintptr_t inT)
{ AE_T(inT)->SetCloakableChain(AE_T(inT)->mUnitClass->mIsCloakable); }
extern "C" void ee_screenscpp_aeh_minimap_hide(uintptr_t inT)
{ gMiniMap->MoveUnit(AE_T(inT), -10000, -10000, AE_T(inT)->mTileX, AE_T(inT)->mTileY); }
extern "C" uintptr_t ee_screenscpp_aeh_owner(uintptr_t inEffect) { return (uintptr_t)AE_E(inEffect)->mUnit; }
extern "C" float ee_screenscpp_aeh_owner_scale(uintptr_t inOwner) { return AE_T(inOwner)->mStats->mAreaEffectScale; }
extern "C" float ee_screenscpp_aeh_leadership(uintptr_t inOwner) { return AE_T(inOwner)->mStats->mLeadership; }
extern "C" float ee_screenscpp_aeh_garrison_bonus(uintptr_t inOwner)
{ return AE_T(inOwner)->mUnitClass->mGarrisonBonus; }
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
extern "C" int32_t ee_screenscpp_ae_unit_player_id(uintptr_t inUnit)
{ return (int32_t)AE_T(inUnit)->GetPlayer()->mPlayerID; }
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
extern "C" uint64_t ee_screenscpp_aec_num_players() { return (uint64_t)::gWorld.mNumPlayers; }
extern "C" uintptr_t ee_screenscpp_aec_player(uint64_t inIndex) { return (uintptr_t)::gWorld.mPlayers[inIndex]; }
extern "C" uintptr_t ee_screenscpp_aec_first_unit(uintptr_t inPlayer)
{ return (uintptr_t)static_cast<EEUnit*>(((EEPlayer*)inPlayer)->mFirstUnit); }
extern "C" uintptr_t ee_screenscpp_aec_next_unit(uintptr_t inUnit)
{ return (uintptr_t)static_cast<EEUnit*>(((EEUnit*)inUnit)->mNext); }
extern "C" bool ee_screenscpp_aec_cloakable(uintptr_t inUnit) { return ((EEComplexUnit*)inUnit)->mIsCloakable; }
extern "C" bool ee_screenscpp_aec_cloaked(uintptr_t inUnit) { return ((EEComplexUnit*)inUnit)->mIsCloaked; }
extern "C" int64_t ee_screenscpp_aec_checksum(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->Checksum(); }
extern "C" uintptr_t ee_screenscpp_aec_template_count() { return gAEManager.mTemplates.size(); }
extern "C" uintptr_t ee_screenscpp_aec_template_at(uintptr_t inIndex)
{ return (uintptr_t)gAEManager.mTemplates[inIndex]; }
extern "C" int32_t ee_screenscpp_aec_effect_index(uintptr_t inEffect)
{ return (int32_t)EEGraphicEffect::IndexOf((EEGraphicEffect*)inEffect); }
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
