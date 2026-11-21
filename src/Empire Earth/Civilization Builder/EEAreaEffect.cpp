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
#include "ee/EEAECapture.h"
#include "lle/FSMemoryFile.h"
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
// ee_gamecpp_ae_host over the cases at the end of this file): the first two members of gamecpp ae_effect.rs's AeHost,
// the effect (its owner unit and player; NULL for a template's test) and the unit the slot was given (NULL when none)
struct EEAEHost {
    EEAreaEffect* mEffect;
    EEComplexUnit* mTarget;
};

// a temporary's address for the full expression (the building counts' vector)
template <class T> static T* EETemporary(T&& inObject)
{
    return &inObject;
}

}   // namespace

// port (round 10, lane C): the effect's, the template's and the unit list's bodies are gamecpp ae_effect.rs's

// Creates an area effect of its template's kind that holds a reference to the template, with no unit or player yet,
// and loads its media
EEAreaEffect::EEAreaEffect(EEAreaEffectTemplate* inTemplate)
{
    ee_gamecpp_aefx_construct(this, inTemplate);
}

// Destroys the area effect, releasing its template and unit and unloading its media
EEAreaEffect::~EEAreaEffect()
{
    ee_gamecpp_aefx_destruct(this);
}

// Applies the effect to a unit (the cloaking makes it cloakable)
void EEAreaEffect::AppliedTo(EEComplexUnit* inUnit)
{
    ee_gamecpp_aefx_applied(this, inUnit, false);
}

// Tells whether the effect, by its kind, covers the unit from a point for a player
bool EEAreaEffect::Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint)
{
    return ee_gamecpp_aefx_covers(this, inPlayer, inUnit, &inPoint);
}

// Plays the template's graphic effect on the unit, if it has one
void EEAreaEffect::ShowOn(EEUnit* inUnit)
{
    ee_gamecpp_aefx_show_on(this, inUnit);
}

// Writes the effect's location, owning unit id and player to the out-of-sync dump
void EEAreaEffect::Dump(FILE* inFile)
{
    ee_gamecpp_aefx_dump(this, inFile);
}

// Returns a sync checksum of the location's float bits, the player and the owning unit id
long EEAreaEffect::Checksum()
{
    return ee_gamecpp_aefx_checksum(this);
}

// Returns the effect's strength by its kind
long EEAreaEffect::GetStrength()
{
    return ee_gamecpp_aefx_strength(this);
}

// Returns a morale kind's level
long EEAreaEffect::GetLevel()
{
    return ee_gamecpp_aefx_level(this);
}

// Returns the template's radius times the effect's scale
float EEAreaEffect::GetRadius()
{
    return ee_gamecpp_aefx_radius(this);
}

// Tells whether the effect is an immunity
bool EEAreaEffect::IsImmunity()
{
    return ee_area_effect_is_immunity(this);
}

// Returns the owning unit's area-effect scale, or 1 without a unit
float EEAreaEffect::GetScale()
{
    return ee_gamecpp_aefx_scale(this);
}

// Sets the owning unit, taking a reference to it
void EEAreaEffect::SetUnit(EEComplexUnit* inUnit)
{
    ee_gamecpp_aefx_set_unit(this, inUnit);
}

// Loads the template's media
void EEAreaEffect::LoadMedia(bool inIsOnDemand)
{
    ee_gamecpp_aetpl_media(this->mTemplate, false, inIsOnDemand);
}

// Modifies a value of a kind by the effect's kind; false when it leaves it
bool EEAreaEffect::Modify(long inKind, long inValue, long& outValue)
{
    return ee_gamecpp_aefx_modify(this, inKind, inValue, &outValue);
}

// Takes the effect off a unit (the cloaking gives back the unit's own cloakability)
void EEAreaEffect::RemovedFrom(EEComplexUnit* inUnit)
{
    ee_gamecpp_aefx_applied(this, inUnit, true);
}

// Marks the morale from buildings for a recount
void EEAreaEffect::MarkDirty()
{
    ee_area_effect_mark_dirty(this);
}

// Unloads the template's media
void EEAreaEffect::UnloadMedia()
{
    ee_gamecpp_aetpl_media(this->mTemplate, true, false);
}

// Recounts the morale from buildings' buildings when marked (inArg unread)
void EEAreaEffect::Recount(long inArg)
{
    ee_gamecpp_aefx_recount(this);
}

// Ticks the effect on a unit it affects (the healing heals it)
void EEAreaEffect::Tick(EEComplexUnit* inUnit, float inElapsed)
{
    ee_gamecpp_aefx_tick(this, inUnit, inElapsed);
}

// Creates an empty template: square shape, radius 1, allied players, no unit set or graphics, white tint
EEAreaEffectTemplate::EEAreaEffectTemplate()
{
    ee_gamecpp_aetpl_construct(this);
}

// Destroys the template, releasing its two graphic effects
EEAreaEffectTemplate::~EEAreaEffectTemplate()
{
    ee_gamecpp_aetpl_destruct(this);
}

// Tells whether the template's player mode (1 self, 2 allies, 3 enemies, 4 everyone) affects the other player
bool EEAreaEffectTemplate::AffectsPlayer(long inPlayer, long inOther)
{
    return ee_gamecpp_aetpl_affects(this, inPlayer, inOther, false);
}

// Tells whether the other player falls outside the template's mode (1 others, 2 enemies, 3 friends)
bool EEAreaEffectTemplate::AffectsPlayerReversed(long inPlayer, long inOther)
{
    return ee_gamecpp_aetpl_affects(this, inPlayer, inOther, true);
}

// Tells whether a point lies in the square or circular area around a center on the same continent; a space
// dock or turret carried by another unit also passes when the point is on its owner's planet
bool EEAreaEffectTemplate::IsWithin(const GE2DPoint& inCenter, const GE2DPoint& inPoint, float inScale,
                                    EEComplexUnit* inUnit)
{
    return ee_gamecpp_aetpl_is_within(this, &inCenter, &inPoint, inScale, inUnit);
}

// Fills the template from an area-effect file record, resolving its graphic effects and media
bool EEAreaEffectTemplate::Init(const EEAreaEffectData& inData)
{
    return ee_gamecpp_aetpl_init(this, &inData);
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

// ------------------------------------------------------------------------- round 10, lane C: Communications, the Civilization Builder's area effects and captures, Calamities, Map

int64_t EECore10OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore10OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define LCAE ((EEAreaEffect*)p)
#define LCTPL ((EEAreaEffectTemplate*)p)
#define LCGFX ((EEGraphicEffect*)p)
#define LCCX ((EEComplexUnit*)p)
#define LCLIST ((UReferenceContainer<EEAreaEffect*>*)p)
#define LCMEM (static_cast<FSMemoryFile*>((FSFile*)p))

// the C++ calls and members of lane C's bodies in Rust (gamecpp; EeGameCpp's round 10 lane C section): one
// statement each
int64_t EECore10OpC(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the area effects (gamecpp ae_effect.rs; p an EEAreaEffect*)
    case O::LcAeTemplate: return P64(LCAE->mTemplate);
    case O::LcAeSetTemplate: LCAE->mTemplate = (EEAreaEffectTemplate*)a[0];
        break;
    case O::LcAeSetOwner: LCAE->mUnit = (EEComplexUnit*)a[0];
        break;
    case O::LcMoraleBonus: return gWorld.GetGameVariant()->mMoraleBonus;
    // the templates (p an EEAreaEffectTemplate*), their graphic effects (p an EEGraphicEffect*) and media
    case O::LcTplReference: LCTPL->Reference();
        break;
    case O::LcTplDereference: LCTPL->Dereference();
        break;
    case O::LcTplGfx: return P64(LCTPL->mGraphicEffect);
    case O::LcTplGfx18: return P64(LCTPL->mGraphicEffect18);
    case O::LcTplMedia: return P64(LCTPL->mMedia);
    case O::LcTplSetGfx: LCTPL->mGraphicEffect = (EEGraphicEffect*)a[0];
        break;
    case O::LcTplSetGfx18: LCTPL->mGraphicEffect18 = (EEGraphicEffect*)a[0];
        break;
    case O::LcTplSetMedia: LCTPL->mMedia = (EEMedia*)a[0];
        break;
    case O::LcGfxGet: return P64(EEGraphicEffect::Get((unsigned long)a[0]));
    case O::LcGfxReference: LCGFX->Reference();
        break;
    case O::LcGfxDereference: LCGFX->Dereference();
        break;
    case O::LcGfxLoad: LCGFX->Load(a[0] != 0);
        break;
    case O::LcGfxUnload: LCGFX->Unload();
        break;
    case O::LcGfxCreateUnitEffect: gEffectManager->CreateUnitEffect((EEUnit*)a[0], LCGFX, NULL, false, 0, 0);
        break;
    case O::LcMediaGet: return P64(EEMedia::Get((unsigned long)a[0]));
    case O::LcMediaCount: return (int64_t)EEMedia::GetCount();
    case O::LcMediaLoad: ((EEMedia*)p)->LoadMedia();
        break;
    case O::LcMediaUnload: ((EEMedia*)p)->UnloadMedia();
        break;
    // the units (p an EEComplexUnit*) and the area-effect lists (p a UReferenceContainer<EEAreaEffect*>*)
    case O::LcCxReference: LCCX->Reference();
        break;
    case O::LcCxDereference: LCCX->Dereference();
        break;
    case O::LcCxHitPoints: return LCCX->mHitPoints;
    case O::LcCxApplyAreaEffects: LCCX->ApplyAreaEffects();
        break;
    case O::LcCxRemoveAffecting: LCCX->RemoveAffectingAreaEffects();
        break;
    case O::LcCxUpdateAreaEffects: LCCX->UpdateAreaEffects((unsigned long)f[0]);
        break;
    case O::LcListRemove: return LCLIST->Remove((EEAreaEffect*)a[0]);
    case O::LcListNew: return P64(static_cast<UReferenceContainer<EEAreaEffect*>*>(new EEUnitAreaEffectList));
    case O::LcListDelete: delete static_cast<EEUnitAreaEffectList*>(LCLIST);
        break;
    case O::LcListAssign: *static_cast<EEUnitAreaEffectList*>(LCLIST) = *static_cast<EEUnitAreaEffectList*>((UReferenceContainer<EEAreaEffect*>*)a[0]);
        break;
    // the capture's AEFX (TEST ONLY: gamecpp ae_capture.rs; p a memory file as an FSFile*)
    case O::LcAeCaptureTick: EEAECaptureTick(f[0]);
        break;
    case O::LcMemFileNew: return P64(static_cast<FSFile*>(new FSMemoryFile));
    case O::LcMemFileDelete: delete LCMEM;
        break;
    case O::LcMemFilePosition: return LCMEM->GetPosition(*(long*)a[0]);
    case O::LcMemFileData: return P64(LCMEM->GetData());
    default: return EECore10OpD(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpCF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpDF(inOp, p, a, f);
}
