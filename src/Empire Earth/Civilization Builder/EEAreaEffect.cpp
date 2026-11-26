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
#include "ee/EEAIKindIDs.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "rust/sim/units.h"
#include "rust/game/screens.h"   // the host (area_effect_host.rs)
#include "ee/EEScreensShims.h"


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
