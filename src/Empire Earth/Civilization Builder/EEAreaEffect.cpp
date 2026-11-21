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
#include "rust/game/gamecpp.h"

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

// The world of an area effect's Rust slots (ee_tech area_effect.rs EeAreaEffectOp; answered by gamecpp civ.rs's
// ee_gamecpp_ae_host over the cases at the end of this file): the effect (its owner unit and player; NULL for a
// template's test) and the unit the slot was given (NULL when none)
struct EEAEHost {
    EEAEHost(EEAreaEffect* inEffect, EEComplexUnit* inTarget) : mEffect(inEffect), mTarget(inTarget)
    {
        this->mFfi.ctx = this;
        this->mFfi.call = (void (*)(void*, const EeAreaEffectCall*, EeAreaEffectReply*))&ee_gamecpp_ae_host;
    }

    EEAreaEffect* mEffect;
    EEComplexUnit* mTarget;
    EeAreaEffectHostFfi mFfi;
};

// a temporary's address for the full expression (the building counts' vector)
template <class T> static T* EETemporary(T&& inObject)
{
    return &inObject;
}

}   // namespace

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

// ------------------------------------------------------------------------- round 9, lane D: the area effects' host

int64_t EECore9OpD6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTree.cpp
double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTreeNode.cpp
double EECore9OpEF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kUnitTypeSpaceDock == 0x2d8 && kUnitTypeSpaceTurret == 0x302, "gamecpp civ.rs AH_SPACE_DOCK, AH_SPACE_TURRET");
static_assert(sizeof(EETileCoordinate) == 2 * sizeof(long) && sizeof(U2DRectangle<long>) == 4 * sizeof(long),
              "gamecpp civ.rs: a tile's and a rectangle's longs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define AH ((EEAEHost*)p)
#define AT (AH->mTarget)
#define AOWNER ((EEComplexUnit*)a[0])
#define ARECT (*(U2DRectangle<long>*)a[0])

// the C++ calls and members of the area effects' host in Rust (gamecpp civ.rs ee_gamecpp_ae_host; p the EEAEHost):
// one statement each
int64_t EECore9OpD5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::AhTargetInSet: return gUnitSetManager.Contains(AT->GetUnitClass()->GetUnitType(), (long)a[0]);
    case O::AhTargetTechNode: return P64(gWorld.mPlayers[a[0]]->GetTechTree()->Get(AT->GetUnitClass()->mTechnology));
    case O::AhNodeKind: return ((EETechTreeNode*)a[0])->mData->mKind;
    case O::AhPlayer: return P64(gWorld.mPlayers[a[0]]);
    case O::AhAllied: return ((EEPlayer*)a[0])->IsAlliedWith((EEPlayer*)a[1]);
    case O::AhAtWar: return ((EEPlayer*)a[0])->mEnemies[((EEPlayer*)a[1])->mPlayerID] != 0;
    case O::AhTargetBuilt: return AT->mIsBuilt;
    case O::AhTargetRepairable: return AT->IsRepairable();
    case O::AhTargetPosition: *(GE3DPoint*)a[0] = AT->mPhysicalModel.GetLocation().GetPosition();
        break;
    case O::AhContinent: return gContinentManager.GetContinent(*(EETileCoordinate*)a[0], kAIPathNonSpace);
    case O::AhTargetCarrier: return P64(AT->mComplexUnit);
    case O::AhTargetType: return (int64_t)AT->mUnitClass->GetUnitType();
    case O::AhPlanet: return (int)((EEComplexUnit*)a[0])->mPlanetID;
    case O::AhTargetHitPoints: return AT->mHitPoints;
    case O::AhTargetMaxHitPoints: return AT->mStats->mMaxHitPoints;
    case O::AhInCinematic: return gInCinematic;
    case O::AhTargetSetHitPoints: AT->SetHitPoints((long)a[0]);
        break;
    case O::AhTargetCloak: AT->SetCloakableChain(true);
        break;
    case O::AhCamouflaged: return gCivPowersManager.HasPower(AT->GetPlayer(), kCivPowerCamouflage) && gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet) != NULL && gUnitSetManager.GetSet(gCivPowersManager.GetPowerRecord(kCivPowerCamouflage)->mUnitSet)->Contains(AT->mUnitClass->mUnitType);
    case O::AhStaysHidden: return AT->GetCommandType() == kActionIdle && (AT->mCamouflageTimer & 0x7fffffff) == 0 || AT->mU32C > 0;
    case O::AhCloakedEnemy: return ee_units_uu_cloaked_enemy((uint64_t)(uintptr_t)AT, (uint64_t)(uintptr_t)(gWorld.mPlayers[gWorld.mCurrentPlayer]));
    case O::AhTargetCloakOwn: AT->SetCloakableChain(AT->mUnitClass->mIsCloakable);
        break;
    case O::AhMiniMapHide: gMiniMap->MoveUnit(AT, -10000, -10000, AT->mTileX, AT->mTileY);
        break;
    case O::AhOwner: return P64(AH->mEffect != NULL ? AH->mEffect->mUnit : NULL);
    case O::AhOwnerHitPoints: return AOWNER->mHitPoints;
    case O::AhCountInRect: return EEGetUnitsInRectangle(ARECT, *EETemporary(std::vector<EEComplexUnit*>()), EEAEMFBBuildingInRectangle(ARECT, gWorld.mPlayers[AH->mEffect->mPlayer]), true);
    case O::AhCountInRange: return EEGetUnitsInRectangle(ARECT, *EETemporary(std::vector<EEComplexUnit*>()), EEAEMFBBuildingInRange(U2DPoint<long>((long)a[1], (long)a[2]), gWorld.mPlayers[AH->mEffect->mPlayer], f[0]), true);
    default: return EECore9OpD6(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpD5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::AhOwnerScale: return AOWNER->mStats->mAreaEffectScale;
    case O::AhOwnerLeadership: return AOWNER->mStats->mLeadership;
    case O::AhOwnerGarrisonBonus: return AOWNER->mUnitClass->mGarrisonBonus;
    default: return EECore9OpD7F(inOp, p, a, f);
    }
}
