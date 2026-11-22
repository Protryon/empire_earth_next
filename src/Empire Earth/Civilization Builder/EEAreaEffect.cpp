// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffect.cpp (name from RTTI EEAreaEffect; folder inferred: no
// __FILE__; the area-effect units sit between EEWonNetAvailableGameFormManager.cpp and the civilization builder's).
// 0x27bf4b..0x27c0f3: the base area effect (vtable 0x44be24): ctor, ??_G, dtor, the slots it implements and the
// scale helper. EEAreaEffectTemplate.cpp starts at 0x27c0f3. Unit flags: the exe's module set.
//
// port (calamities lane, round 3): the eight kinds' classes are this one class over ee_tech area_effect.rs (their
// units EEAECloaking.cpp 0x27aa0b.., EEAEEconomy.cpp 0x27ac2c.., EEAEHealing.cpp 0x27ace1..,
// EEAEMoraleFromBuildings.cpp 0x27b911.., EEAEMoraleFromMilitaryHeroes.cpp 0x27bf0a.. and the inline kinds of
// EEAEManager.cpp are gone); EEAEHost below answers their world calls, in the originals' order. The template's
// members that ask the host (its ctor, dtor, player and area tests, Init: 0x27c0f3..0x27c5ab) are here too.
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

extern bool& gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)

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
        long theX = inUnit->mTileX;
        long theY = inUnit->mTileY;
        long theSize = inUnit->mUnitClass->mTileSize;
        long theRight = theX + theSize - 1;
        long theBottom = theY + theSize - 1;
        return this->mRect.mLeft <= theRight && this->mRect.mRight >= theX && this->mRect.mTop <= theBottom && this->mRect.mBottom >= theY;
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
        U2DPoint<long> theTile(inUnit->mTileX, inUnit->mTileY);
        return this->mCenter.DistanceToSquaredF(theTile) <= this->mRangeSquared;
    }

    U2DPoint<long> mCenter;
    EEPlayer* mPlayer;
    float mRangeSquared;
};

// The world of an area effect's Rust slots (ee_tech area_effect.rs EeAreaEffectOp): the effect (its owner unit and
// player; NULL for a template's test) and the unit the slot was given (NULL when none)
struct EEAEHost {
    EEAEHost(EEAreaEffect* inEffect, EEComplexUnit* inTarget) : mEffect(inEffect), mTarget(inTarget)
    {
        this->mFfi.ctx = this;
        this->mFfi.call = &EEAEHost::Call;
    }
    static void Call(void* inContext, const EeAreaEffectCall* c, EeAreaEffectReply* r);

    EEAreaEffect* mEffect;
    EEComplexUnit* mTarget;
    EeAreaEffectHostFfi mFfi;
};

// the area effects' world calls: Rust's (area_effect_host.rs) over the one-statement shims below (UI lane round 11)
void EEAEHost::Call(void* inContext, const EeAreaEffectCall* c, EeAreaEffectReply* r)
{
    ee_screens_ae_host(static_cast<EEAEHost*>(inContext)->mEffect, static_cast<EEAEHost*>(inContext)->mTarget, c, r);
}

}   // namespace

#define AE_T(t) ((EEComplexUnit*)(t))
#define AE_P(p) ((EEPlayer*)(p))
#define AE_E(e) ((EEAreaEffect*)(e))
static EETileCoordinate sAETile;                // the continent's tile
static U2DRectangle<long> sAERect;              // the morale buildings' rectangle
static std::vector<EEComplexUnit*> sAEUnits;    // and the units collected in it
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
extern "C" void ee_screenscpp_aeh_tile_x(int32_t inV) { sAETile.mX = inV; }
extern "C" void ee_screenscpp_aeh_tile_y(int32_t inV) { sAETile.mY = inV; }
extern "C" int32_t ee_screenscpp_aeh_continent() { return gContinentManager.GetContinent(sAETile, kAIPathNonSpace); }
extern "C" uintptr_t ee_screenscpp_aeh_carrier(uintptr_t inT)
{ return (uintptr_t)(EEComplexUnit*)AE_T(inT)->mComplexUnit; }
extern "C" uint64_t ee_screenscpp_aeh_unit_type(uintptr_t inT)
{ return (uint64_t)AE_T(inT)->mUnitClass->GetUnitType(); }
extern "C" int32_t ee_screenscpp_aeh_planet(uintptr_t inCarrier) { return (int)AE_T(inCarrier)->mPlanetID; }
extern "C" int32_t ee_screenscpp_aeh_hit_points(uintptr_t inT) { return AE_T(inT)->mHitPoints; }
extern "C" int32_t ee_screenscpp_aeh_max_hit_points(uintptr_t inT) { return AE_T(inT)->mStats->mMaxHitPoints; }
extern "C" bool ee_screenscpp_aeh_in_cinematic() { return gInCinematic; }
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
extern "C" void ee_screenscpp_aeh_rect_top(int32_t inV) { sAERect.mTop = inV; }
extern "C" void ee_screenscpp_aeh_rect_left(int32_t inV) { sAERect.mLeft = inV; }
extern "C" void ee_screenscpp_aeh_rect_right(int32_t inV) { sAERect.mRight = inV; }
extern "C" void ee_screenscpp_aeh_rect_bottom(int32_t inV) { sAERect.mBottom = inV; }
extern "C" void ee_screenscpp_aeh_units_clear() { sAEUnits.clear(); }
extern "C" int32_t ee_screenscpp_aeh_count_in_rect(uintptr_t inEffect)
{ return EEGetUnitsInRectangle(sAERect, sAEUnits, EEAEMFBBuildingInRectangle(sAERect, gWorld.mPlayers[AE_E(inEffect)->mPlayer]), true); }
extern "C" int32_t ee_screenscpp_aeh_count_in_range(uintptr_t inEffect, int32_t inX, int32_t inY, float inRange)
{ return EEGetUnitsInRectangle(sAERect, sAEUnits, EEAEMFBBuildingInRange(U2DPoint<long>(inX, inY), gWorld.mPlayers[AE_E(inEffect)->mPlayer], inRange), true); }

// Creates an area effect of its template's kind that holds a reference to the template, with no unit or player yet
// (the morale from buildings' cap: the template's maximum level plus the game variant's morale bonus), and loads its
// media
EEAreaEffect::EEAreaEffect(EEAreaEffectTemplate* inTemplate)
{
    this->mTemplate = inTemplate;
    this->mTemplate->Reference();
    this->mUnit = NULL;
    long theKind = inTemplate->mKind;
    long theBonus = theKind == kAreaEffectMoraleFromBuildings ? gWorld.GetGameVariant()->mMoraleBonus : 0;
    ee_area_effect_init(this, theKind, inTemplate->mMaxLevel, theBonus);
    this->LoadMedia(false);
}

// Destroys the area effect, releasing its template and unit and unloading its media
EEAreaEffect::~EEAreaEffect()
{
    this->mTemplate->Dereference();
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->UnloadMedia();
}

// Applies the effect to a unit (the cloaking makes it cloakable)
void EEAreaEffect::AppliedTo(EEComplexUnit* inUnit)
{
    EEAEHost theHost(this, inUnit);
    ee_area_effect_applied(this, &theHost.mFfi, false);
}

// Tells whether the effect, by its kind, covers the unit from a point for a player
bool EEAreaEffect::Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint)
{
    EEAEHost theHost(this, inUnit);
    return ee_area_effect_covers(this, this->mTemplate, &theHost.mFfi, (int32_t)inPlayer,
                                 (int32_t)inUnit->GetPlayer()->mPlayerID, inPoint.mX, inPoint.mY);
}

// Plays the template's graphic effect on the unit, if it has one
void EEAreaEffect::ShowOn(EEUnit* inUnit)
{
    EEGraphicEffect* theEffect = this->mTemplate->mGraphicEffect;
    if (theEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, false, 0, 0);
}

// Writes the effect's location, owning unit id and player to the out-of-sync dump
void EEAreaEffect::Dump(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location X: %6.6f\n", (double)this->mLocation.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location Y: %6.6f\n", (double)this->mLocation.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Owning Unit ID #%08lx\n", this->mUnit != NULL ? this->mUnit->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Player Number %ld\n", this->mPlayer);
}

// Returns a sync checksum of the location's float bits, the player and the owning unit id
long EEAreaEffect::Checksum()
{
    return ee_area_effect_checksum(this, this->mLocation.mX, this->mLocation.mY,
                                   this->mUnit != NULL ? (long)this->mUnit->mUnitID : 0);
}

// Returns the effect's strength by its kind
long EEAreaEffect::GetStrength()
{
    EEAEHost theHost(this, NULL);
    return ee_area_effect_strength(this, this->mTemplate, &theHost.mFfi);
}

// Returns a morale kind's level
long EEAreaEffect::GetLevel()
{
    EEAEHost theHost(this, NULL);
    return ee_area_effect_level(this, this->mTemplate, &theHost.mFfi);
}

// Returns the template's radius times the effect's scale
float EEAreaEffect::GetRadius()
{
    EEAEHost theHost(this, NULL);
    return ee_area_effect_radius(this->mTemplate, &theHost.mFfi);
}

// Tells whether the effect is an immunity
bool EEAreaEffect::IsImmunity()
{
    return ee_area_effect_is_immunity(this);
}

// Returns the owning unit's area-effect scale, or 1 without a unit
float EEAreaEffect::GetScale()
{
    EEAEHost theHost(this, NULL);
    return ee_area_effect_scale(&theHost.mFfi);
}

// Sets the owning unit, taking a reference to it
void EEAreaEffect::SetUnit(EEComplexUnit* inUnit)
{
    this->mUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// Loads the template's media
void EEAreaEffect::LoadMedia(bool inIsOnDemand)
{
    this->mTemplate->Load(inIsOnDemand);
}

// Modifies a value of a kind by the effect's kind; false when it leaves it
bool EEAreaEffect::Modify(long inKind, long inValue, long& outValue)
{
    EEAEHost theHost(this, NULL);
    return ee_area_effect_modify(this, this->mTemplate, &theHost.mFfi, inKind, inValue, &outValue);
}

// Takes the effect off a unit (the cloaking gives back the unit's own cloakability)
void EEAreaEffect::RemovedFrom(EEComplexUnit* inUnit)
{
    EEAEHost theHost(this, inUnit);
    ee_area_effect_applied(this, &theHost.mFfi, true);
}

// Marks the morale from buildings for a recount
void EEAreaEffect::MarkDirty()
{
    ee_area_effect_mark_dirty(this);
}

// Unloads the template's media
void EEAreaEffect::UnloadMedia()
{
    this->mTemplate->Unload();
}

// Recounts the morale from buildings' buildings when marked
void EEAreaEffect::Recount(long inArg)
{
    EEAEHost theHost(this, NULL);
    ee_area_effect_recount(this, this->mTemplate, &theHost.mFfi, this->mLocation.mX, this->mLocation.mY);
}

// Ticks the effect on a unit it affects (the healing heals it)
void EEAreaEffect::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    EEAEHost theHost(this, inUnit);
    ee_area_effect_tick(this, this->mTemplate, &theHost.mFfi, inElapsed);
}

// Creates an empty template: square shape, radius 1, allied players, no unit set or graphics, white tint
EEAreaEffectTemplate::EEAreaEffectTemplate()
{
    ee_area_effect_template_reset(this);
    this->mGraphicEffect = NULL;
    this->mGraphicEffect18 = NULL;
    this->mMedia = NULL;
}

// Destroys the template, releasing its two graphic effects
EEAreaEffectTemplate::~EEAreaEffectTemplate()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Dereference();
    if (this->mGraphicEffect18 != NULL)
        this->mGraphicEffect18->Dereference();
}

// Tells whether the template's player mode (1 self, 2 allies, 3 enemies, 4 everyone) affects the other player
bool EEAreaEffectTemplate::AffectsPlayer(long inPlayer, long inOther)
{
    EEAEHost theHost(NULL, NULL);
    return ee_area_effect_affects_player(this, &theHost.mFfi, inPlayer, inOther, false);
}

// Tells whether the other player falls outside the template's mode (1 others, 2 enemies, 3 friends)
bool EEAreaEffectTemplate::AffectsPlayerReversed(long inPlayer, long inOther)
{
    EEAEHost theHost(NULL, NULL);
    return ee_area_effect_affects_player(this, &theHost.mFfi, inPlayer, inOther, true);
}

// Tells whether a point lies in the square or circular area around a center on the same continent; a space
// dock or turret carried by another unit also passes when the point is on its owner's planet
bool EEAreaEffectTemplate::IsWithin(const GE2DPoint& inCenter, const GE2DPoint& inPoint, float inScale,
                                    EEComplexUnit* inUnit)
{
    EEAEHost theHost(NULL, inUnit);
    return ee_area_effect_is_within(this, &theHost.mFfi, inCenter.mX, inCenter.mY, inPoint.mX, inPoint.mY, inScale,
                                    inUnit != NULL);
}

// Fills the template from an area-effect file record, resolving its graphic effects and media
bool EEAreaEffectTemplate::Init(const EEAreaEffectData& inData)
{
    ee_area_effect_template_init(this, reinterpret_cast<const EeAreaEffectData*>(&inData));
    if (inData.mGraphicEffect > 0) {
        this->mGraphicEffect = EEGraphicEffect::Get(inData.mGraphicEffect);
        if (this->mGraphicEffect != NULL)
            this->mGraphicEffect->Reference();
    }
    if (inData.mGraphicEffect18 > 0) {
        this->mGraphicEffect18 = EEGraphicEffect::Get(inData.mGraphicEffect18);
        if (this->mGraphicEffect18 != NULL)
            this->mGraphicEffect18->Reference();
    }
    this->mMedia = inData.mMedia > 0 ? EEMedia::Get(inData.mMedia) : NULL;
    return true;
}
