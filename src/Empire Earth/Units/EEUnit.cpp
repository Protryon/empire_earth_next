// C:\EEX\Empire Earth\Units\EEUnit.cpp (file name inferred: no __FILE__; the keys "EEUnit-HitPoints"/"EEUnit-Data").
// The unit base: 0x228c4c..0x22a509. Unit flags: the exe's module set. Data: the keys 0x963638/0x963620 (one guard
// byte 0x963630), the vtable 0x447a20.

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEUnitStore.h"
#include "ee/EEUnitKind.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECombatFormulas.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEEffectManager.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "ee/EECameraControl.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EESoundObject.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnitRegions.h"
#include "ee/EEInfluenceTable.h"
#include <math.h>
#include <algorithm>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIForest.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEUnitHandle.h"
#include "rust/sim/units.h"

// New (R5a.4/R5a.6): the typed halves of EEUnitHandle (ee/EEUnitHandle.h), which need the complete classes; the
// registry and the RTSUnit halves are in RTSUnit.cpp
uint32_t EEUnitHandleID(const EEUnit* inUnit) { return EEUnitHandleID(static_cast<const RTSUnit*>(inUnit)); }
uint32_t EEUnitHandleID(const EEComplexUnit* inUnit) { return EEUnitHandleID(static_cast<const RTSUnit*>(inUnit)); }
void EEUnitHandleResolve(uint32_t inUnitID, EEUnit*& outUnit)
{
    outUnit = static_cast<EEUnit*>(RTSResolveUnitID(inUnitID));
}
void EEUnitHandleResolve(uint32_t inUnitID, EEComplexUnit*& outUnit)
{
    outUnit = static_cast<EEComplexUnit*>(static_cast<EEUnit*>(RTSResolveUnitID(inUnitID)));
}

const UString EEUnit::sHitPoints("EEUnit-HitPoints");

const UString EEUnit::sData("EEUnit-Data");

// Creates an empty unit with no class, player, resources or hit points and no references
EEUnit::EEUnit()
{
    this->mObjectKind.mValue = 1;   // New (port): an EEUnit (ee/EEUnitKind.h)
    this->Reset();
    this->ResetEEUnit();
    this->mReferenceCount = 0;
}

// Removes the unit from the game, unloading its class media at once
void EEUnit::Invalidate()
{
    EEUnit::Remove(false);
}

// Returns the unit's 3D model (from its player's stats for the class)
EEModel* EEUnit::GetModel()
{
    return this->mStats->mModel;
}

// Resets the unit to an empty, invalid unit
void EEUnit::Clear()
{
    this->ResetEEUnit();
    this->Reset();
    this->mIsValid = false;
}

// Destroys the unit, freeing its tile node
EEUnit::~EEUnit()
{
    this->mObjectKind.mValue = 1;   // New (port): an EEUnit again (ee/EEUnitKind.h)
    this->FreeNode();
}

// Adds inValue (negative: takes it) to the resource amount the unit holds
void EEUnit::AddResourceAmount(long inValue)
{
    this->mResourceAmount += inValue;
}

// FindAreaTile's tile from eu_body.rs's answer (x in the low 32 bits, y in the high)
static EETileCoordinate EEEuTile(int64_t inPacked)
{
    return EETileCoordinate((long)(int32_t)(uint32_t)inPacked, (long)(int32_t)(uint32_t)((uint64_t)inPacked >> 32));
}

// Returns whether the unit may gather from inUnit (visible tile, resource left unless inIgnoreAmount): a land citizen
// takes its own fully repaired farm or a dead or enemy non-citizen off deep water; a naval citizen enemy naval units
// Reads through the inline getters: the original refuses RTSMap::GetTile under EEMap::GetTile (inline budget: the
// getter sites after it leave the wrapper too small a share).
bool EEUnit::CanGather(EEUnit* inUnit, long inX, long inY, bool inIsVisible, bool inIgnoreAmount)
{
    return ee_units_eu_can_gather(this, inUnit, inX, inY, inIsVisible, inIgnoreAmount) != 0;   // eu_body.rs
}

// Removes the unit from the game: unloads its class media (queued to the preloader if asked), zeroes its hit points
// and deactivates it
void EEUnit::Remove(bool inUnloadInBackground)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_REMOVE, this, NULL, inUnloadInBackground, 0);   // eu_more.rs
}

// Copies another unit's resource type and amount and its health fraction (scaled to this unit's maximum hit points)
void EEUnit::CopyFrom(RTSUnit* inUnit)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_COPY_FROM, this, inUnit, 0, 0);   // eu_more.rs
}

// Rechecks, staggered by game-time phase, whether a unit or moving object stands on or beside this obstacle (a
// complex unit: hides the obstacles behind it when it moved) and sets the hidden flags; returns whether it is hidden.
// Port (the units' round 1): both classes' bodies are Rust over the world views (ee_units hidden.rs)
bool EEUnit::UpdateHidden()
{
    return ee_units_update_hidden(EEObj(this));
}

// Creates a simple unit of inClass for inPlayer at tile (inX, inY)
EEUnit* EEUnit::Create(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer)
{
    EEUnit* theUnit = new EEUnit;
    theUnit->Init(inClass, inX, inY, inPlayer);
    return theUnit;
}

// Creates an empty simple unit
EEUnit* EEUnit::Create()
{
    return new EEUnit;
}

// Frees the unit's tile node (the RTSUnit part)
void EEUnit::FreeNode()
{
    RTSUnit::FreeNode();
}

// Returns whether inUnit may pass the unit: non-blocking units, dead non-resources, and wood for forest-crossing units
bool EEUnit::IsPassableBy(EEComplexUnit* inUnit)
{
    return !this->mUnitClass->mBlocksTile || (this->mHitPoints == 0 && this->mUnitClass->mCategory != kUnitCategoryResource) || (this->mUnitClass->mResourceType == kResourceWood && inUnit != NULL && (bool)((inUnit->mStats->mTerrainMask >> 14) & 1));
}

// Returns 0 (a simple unit adds nothing to the sync checksum)
long EEUnit::GetChecksum(unsigned long)
{
    return 0;
}

// Returns the squared tile distance to the nearest edge tile of inUnit's footprint and that tile's coordinates
// NEAR: register choice only (edi/ebx and where x is spilled)
float EEUnit::GetNearestFootprintTile(EEUnit* inUnit, long* outX, long* outY)
{
    return ee_units_eu_nearest_footprint_tile(this, inUnit, outX, outY);   // eu_body.rs
}

// Appends the unit's one-line debug text: base text, resource type and amount, class type, hit points
void EEUnit::Describe(UWideString& ioString)
{
    UWideString theString;
    RTSUnit::Describe(theString);
    ioString += theString;
    sprintf(theString, "res %ld / %ld:", this->mResourceType, this->mResourceAmount);
    ioString += theString;
    sprintf(theString, "obj %ld:", this->mUnitClass->mUnitType);
    ioString += theString;
    sprintf(theString, "hp %ld:", this->mHitPoints);
    ioString += theString;
}

// Appends the unit's long debug text: base text, class (simple units), resource and health
void EEUnit::DescribeLong(UWideString& ioString)
{
    UWideString theString;
    if (this->mComplexUnit == NULL) {
        sprintf(theString, "\n======] SIMPLE UNIT [======\n");
        ioString += theString;
    }
    RTSUnit::DescribeLong(theString);
    ioString += theString;
    if (this->mComplexUnit == NULL) {
        sprintf(theString, "Object Type and Name: %ld, %s\n", this->mUnitClass->mUnitType, (const char*)this->mUnitClass->mName);
        ioString += theString;
    }
    if (this->mResourceType != kResourceNone) {
        sprintf(theString, "Resource Type/Amount:%ld / %ld\n", this->mResourceType, this->mResourceAmount);
        ioString += theString;
    }
    sprintf(theString, "Health: %ld / %ld (%.2f%%)\n", this->mHitPoints, this->mStats->mMaxHitPoints,
            (float)this->mHitPoints / (float)this->mStats->mMaxHitPoints);
    ioString += theString;
}

// Returns the unit's tile, or for a multi-tile unit off a continent of kind inFlag, the first tile within inRange
// of its footprint that lies on such a continent
EETileCoordinate EEUnit::FindAreaTile(bool inFlag, long inRange)
{
    return EEEuTile(ee_units_eu_body(EE_UNITS_EU_DO_FIND_AREA_TILE, this, inFlag, inRange, 0));   // eu_body.rs
}

// Returns the centre of the unit's tile at ground height
void EEUnit::GetPosition(GE3DPoint& outPosition)
{
    outPosition.SetX(this->mTileX + 0.5f);
    outPosition.SetY(this->mTileY + 0.5f);
    outPosition.SetZ(this->mTile->mHeight);
}

// Initialises the unit as a simple unit of inClass for inPlayer and places it at tile (inX, inY)
void EEUnit::Init(EEUnitClass* inClass, long inX, long inY, EEPlayer* inPlayer)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_INIT_AT, this, inClass, (int64_t)(uint32_t)inX | (int64_t)((uint64_t)(uint32_t)inY << 32), (int64_t)(uintptr_t)inPlayer);   // eu_more.rs
}

// Initialises the unit part of a complex unit of inClass for inPlayer, with no resources or hit points yet
void EEUnit::Init(EEUnitClass* inClass, EEPlayer* inPlayer)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_INIT_COMPLEX, this, inClass, 0, (int64_t)(uintptr_t)inPlayer);   // eu_more.rs
}

// Clears the unit's class, stats, resources, hit points and hidden flags
void EEUnit::ResetEEUnit()
{
    ee_units_eu_more(EE_UNITS_EUM_DO_RESET, this, NULL, 0, 0);   // eu_more.rs
}

// Loads the unit class's media for the owner: at once, or for a foundation queued to the preloader
void EEUnit::LoadMedia(bool inIsFoundation)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_LOAD_MEDIA, this, NULL, inIsFoundation, 0);   // eu_more.rs
}

// Kills the unit: updates sight and doppelgangers, plays a simple unit's death effect and sound, empties farms and
// citizens, takes it off the minimap and camera and reports a simple unit lost
void EEUnit::Kill(bool inKilled, bool inKeepResearch, bool inOther)
{
    ee_units_eu_body(EE_UNITS_EU_DO_KILL, this, inKilled | inKeepResearch << 1 | inOther << 2, 0, 0);   // eu_body.rs
}

// Frees the unit's tile node and deletes the unit once its last reference goes
void EEUnit::LastDereference()
{
    this->FreeNode();
    this->mReferenceCount = 0;
    delete this;
}

// Reads the unit from a saved game (resource type, health fraction held in mHitPoints until FinishLoad, amount);
// returns the first read error. port (R5b.4, round 26): the body is Rust (ee_units unit_load.rs unit_load)
long EEUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEUnitSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion, 0, 0};
    return ee_units_unit_load(&theContext, &EEUnitSaveOperation, this, this, NULL, &theIn);
}

// Turns the loaded health fraction into hit points (at least 1 if the fraction was not 0; port, R5b.4 round 27:
// ee_units unit_load.rs finish_hit_points over the record); returns 0
long EEUnit::FinishLoad(long, long)
{
    if (this->mUnitClass != NULL)
        ee_units_unit_finish_hit_points(this, this->mStats->mMaxHitPoints);
    return 0;
}

// Repairs the unit's pointers after loading (the RTSUnit part only)
long EEUnit::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return RTSUnit::RepairPointers(inMajorVersion, inMinorVersion);
}

// Writes the unit to the saved game (base unit part, resource type, health fraction, amount, a zero); returns the
// first write error. port (R5b.4, round 26): the body is Rust (ee_units unit_save.rs unit_save)
long EEUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEUnitSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion, NULL};
    EeUnitsUnitSaveIn theIn = {(uint32_t)inMajorVersion, (uint32_t)inMinorVersion, 0, 0};
    return ee_units_unit_save(&theContext, &EEUnitSaveOperation, this, this, NULL, &theIn);
}

// Sets the unit's hit points, activates a dead unit of an inactive class and refreshes the icon bar if it is selected
void EEUnit::SetHitPoints(long inHitPoints)
{
    ee_units_eu_body(EE_UNITS_EU_DO_SET_HIT_POINTS, this, inHitPoints, 0, 0);   // eu_body.rs
}

// Moves the unit to tile (inX, inY); with inUpdateTiles also updates the off-map list, minimap, sight, tile, region
// and influence tables, activation and the world's sort order
// NEAR: the original calls the first GetTile out of line (0x2f85) and expands the second (inline budget)
// and divides into reused slots; the four (EEComplexUnit*)this casts follow the EEUnitClass slot types (likely EEUnit*)
void EEUnit::MoveTo(long inX, long inY, bool inUpdateTiles)
{
    ee_units_eu_body(EE_UNITS_EU_DO_MOVE_TO, this, inX, inY, inUpdateTiles);   // eu_body.rs
}

// Takes inAmount from the resource; an exhausted resource unit leaves the minimap and is removed from the game
void EEUnit::DepleteResource(long inAmount)
{
    ee_units_eu_body(EE_UNITS_EU_DO_DEPLETE_RESOURCE, this, inAmount, 0, 0);   // eu_body.rs
}

// Applies inDamage from inAttacker: area effects and morale adjust it and the attacker is remembered, then the unit
// loses hit points with damage and impact effects, an attack event and minimap ping, or dies. port (the units' lane,
// round 7): the body is Rust (ee_units eu_hit.rs) over ee_units_eu_host's statements
void EEUnit::TakeHit(EEUnit* inAttacker, long inDamage, unsigned long inPlayerNumber, long inMode)
{
    ee_units_eu_take_hit(this, inAttacker, inDamage, (uint32_t)inPlayerNumber, inMode);
}

// TakeHit's locals (the modifiers' input, the area damage, the damage) and the hit formula's answer
static long sHitInput;
static long sHitAreaDamage;
static long sHitDamage;
static EeCombatHitPointsOut sHitOut;
// Kill's death sound parameters (its locals)
static float sDeathVolume;
static float sDeathPan;
static float sDeathPitch;

template <class T> static T* EEEuObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

static int64_t EEEuBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof theBits);
    return theBits;
}

// The C++ of EEUnit::TakeHit's Rust body (ee_units eu_hit.rs; EE_UNITS_EU_*): one statement of the original each, in
// its order; inA an object, inB and inC values
extern "C" int64_t ee_units_eu_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    EEUnit* theOther = inA != 0 ? EEEuObject<EEUnit>(inA) : theUnit;
    EEComplexUnit* theComplex = theUnit->mComplexUnit;
    EEPlayer* thePlayer = dynamic_cast<EEPlayer*>(theUnit->mPlayer);
    const int64_t* theArgs = EEEuObject<const int64_t>((uint64_t)inB);
    switch (inOp) {
    case EE_UNITS_EU_COMPLEX: return (uint64_t)(uintptr_t)theComplex;
    case EE_UNITS_EU_PREVIOUS_PART: return (uint64_t)(uintptr_t)theComplex->mPreviousPart;
    case EE_UNITS_EU_PART_TAKE_HIT:
        EEEuObject<EEComplexUnit>(inA)->TakeHit(EEEuObject<EEUnit>((uint64_t)theArgs[0]), (long)theArgs[1], (unsigned long)theArgs[2], (long)theArgs[3]);
        return 0;
    case EE_UNITS_EU_IN_CINEMATIC: return ::gWorld.mInCinematic;
    case EE_UNITS_EU_DISABLED_UNPROTECTED: return theComplex->mIsDisabled && !theComplex->IsProtected();
    case EE_UNITS_EU_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_EU_HAS_SPARKS: return theUnit->mUnitClass->mImpactSparksEffect != NULL;
    case EE_UNITS_EU_SPARKS: gEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mImpactSparksEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_HAS_BLOOD: return theUnit->mUnitClass->mImpactBloodEffect != NULL;
    case EE_UNITS_EU_BLOOD: gEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mImpactBloodEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_SET_INPUT: sHitInput = (long)inB; return 0;
    case EE_UNITS_EU_SET_AREA_DAMAGE: sHitAreaDamage = (long)inB; return 0;
    case EE_UNITS_EU_APPLY_MODIFIERS: theComplex->mAffectingAreaEffects.ApplyModifiers(1, sHitInput, sHitAreaDamage, theComplex); return 0;
    case EE_UNITS_EU_AREA_DAMAGE: return sHitAreaDamage;
    case EE_UNITS_EU_SET_DAMAGE: sHitDamage = (long)inB; return 0;
    case EE_UNITS_EU_APPLY_MORALE: theComplex->ApplyMorale(sHitAreaDamage, sHitDamage); return 0;
    case EE_UNITS_EU_DAMAGE: return sHitDamage;
    case EE_UNITS_EU_ON_MAP: return theOther->mTileNode != NULL;
    case EE_UNITS_EU_IS_ENEMY: return theOther->mPlayer != theUnit->mPlayer && theUnit->mPlayer->mEnemies[theOther->mPlayer->mPlayerID] != 0;
    case EE_UNITS_EU_OWNER: return (uint64_t)(uintptr_t)theOther->mComplexUnit->mOwner;
    case EE_UNITS_EU_TARGETABLE: return ee_units_uu_targetable((uint64_t)(uintptr_t)theUnit, (uint64_t)(uintptr_t)theOther->GetUnitClass(), 0);
    case EE_UNITS_EU_COMPLEX_OF: return (uint64_t)(uintptr_t)theOther->mComplexUnit;
    case EE_UNITS_EU_SET_LAST_ATTACKER: theComplex->SetLastAttacker(EEEuObject<EEComplexUnit>(inA)); return 0;
    case EE_UNITS_EU_IS_IDLE: return theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionIdle;
    case EE_UNITS_EU_HAS_UNDER_ATTACK: return theComplex->mUnitClass->FindAction(kActionUnderAttack) != NULL;
    case EE_UNITS_EU_START_UNDER_ATTACK: theComplex->StartAction(kActionUnderAttack); return 0;
    case EE_UNITS_EU_ACTION_CHANGED: theComplex->mUnitClass->ActionChanged(theComplex); return 0;
    case EE_UNITS_EU_SHOOTS_TRACERS:
        return theOther->mUnitClass->mShotEffect != NULL && theOther->mUnitClass->mShotEffect->mKind == kEffect3DTracers;
    case EE_UNITS_EU_HIT_FORMULA:
        sHitOut = EECombatHitPoints(EeCombatHitPointsIn{(int32_t)inB, (int32_t)inC, (int32_t)theUnit->mStats->mMaxHitPoints});
        return 0;
    case EE_UNITS_EU_HIT_BADLY_HURT: return sHitOut.badly_hurt;
    case EE_UNITS_EU_HIT_NEW_POINTS: return sHitOut.hit_points;
    case EE_UNITS_EU_BADLY_HURT: theUnit->mUnitClass->BadlyHurt(theUnit); return 0;
    case EE_UNITS_EU_DAMAGE_TAKEN: thePlayer->DamageTaken((unsigned long)inB, (long)inC, theUnit); return 0;
    case EE_UNITS_EU_NUM_PLAYERS: return gWorld.mNumPlayers;
    case EE_UNITS_EU_HAS_UNITS_STATE: return gWorld.mPlayers[inB] != NULL && gWorld.mPlayers[inB]->mComputerPlayer != NULL && gWorld.mPlayers[inB]->mComputerPlayer->mUnitsState != NULL;
    case EE_UNITS_EU_ON_DEALT: EECPUnitsOnDealt(gWorld.mPlayers[inB]->mComputerPlayer, (long)inC, theUnit); return 0;
    case EE_UNITS_EU_TILE_X: return theOther->mTileX;
    case EE_UNITS_EU_TILE_Y: return theOther->mTileY;
    case EE_UNITS_EU_CONTAINER: return (uint64_t)(uintptr_t)EEEuObject<EEComplexUnit>(inA)->mContainer;
    case EE_UNITS_EU_ADD_ATTACK_EVENT:
        thePlayer->mEvents.AddPlayerEvent(EETileLocation(theUnit->mTileX, theUnit->mTileY), EETileLocation((long)inB, (long)inC), 1, thePlayer, -1, false);
        return 0;
    case EE_UNITS_EU_DAMAGE_EFFECT: return (uint64_t)(uintptr_t)theUnit->mUnitClass->mDamageEffect;
    case EE_UNITS_EU_SHOWS_DAMAGE: return theUnit->mTileNode == NULL || theUnit->mComplexUnit->mIsBuilt;
    case EE_UNITS_EU_DAMAGE_EFFECT_FORMULA: return EECombatDamageEffect(EeCombatDamageEffectIn{(int32_t)theUnit->mHitPoints, (int32_t)inB, (int32_t)theUnit->mStats->mMaxHitPoints, EEEuObject<EEGraphicEffect>(inA)->GetParam2()});
    case EE_UNITS_EU_CATEGORY: return theUnit->mUnitClass->mCategory;
    case EE_UNITS_EU_EFFECT_ON_SELF: gEffectManager->CreateUnitEffect(theUnit, EEEuObject<EEGraphicEffect>(inA), NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_GATE_EFFECT_LINKED: gEffectManager->CreateUnitEffect(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinked(theUnit->mComplexUnit), theUnit->mUnitClass->mDamageEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_GATE_EFFECT_CARRIER: gEffectManager->CreateUnitEffect(EEUnitClassOfKind<EE_CLASS_KIND_GATE>(theUnit->mUnitClass)->GetLastLinkedCarrier(theUnit->mComplexUnit), theUnit->mUnitClass->mDamageEffect, NULL, false, 0, 0); return 0;
    case EE_UNITS_EU_SET_HIT_POINTS: theUnit->mHitPoints = (long)inB; return 0;
    case EE_UNITS_EU_TARGET_SEARCH_TIMER: return theComplex->mTargetSearchTimer;
    case EE_UNITS_EU_CLEAR_TARGET_SEARCH_TIMER: theComplex->mTargetSearchTimer = 0; return 0;
    case EE_UNITS_EU_COMPLEX_CATEGORY: return theComplex->mUnitClass->mCategory;
    case EE_UNITS_EU_AMMUNITION: return theComplex->mAmmunition;
    case EE_UNITS_EU_FLIGHT_TIME: return theComplex->mFlightTime;
    case EE_UNITS_EU_CLEAR_FLIGHT_TIME: theComplex->mFlightTime = 0; return 0;
    case EE_UNITS_EU_PLAYER_ID: return theUnit->mPlayer->mPlayerID;
    case EE_UNITS_EU_FIRST_PLAYER_ID: return ::gWorld.mPlayers[0]->mPlayerID;
    case EE_UNITS_EU_STATS_RESET: ::gWorld.mPlayers[inB]->mSavedGroups.Reset((long)inC, 1); return 0;
    case EE_UNITS_EU_KILL: theUnit->Kill(true, false, false); return 0;
    case EE_UNITS_EU_UNIT_TYPE: return theUnit->mUnitClass->mUnitType;
    case EE_UNITS_EU_ICBM_EXPLOSION:
        gEffectManager->CreateUnitEffect(theUnit, EEGraphicEffect::Get(kGraphicEffectAircraftExplosion), NULL, false, 0, 0);
        return 0;
    case EE_UNITS_EU_IS_CURRENT_PLAYER: return theUnit->mPlayer == ::gWorld.GetCurrentPlayer();
    case EE_UNITS_EU_MINIMAP_PING: gMiniMap->Ping(GE2DPoint((float)theUnit->mTileX, (float)theUnit->mTileY)); return 0;
    case EE_UNITS_EU_SELECTED: return theUnit->mPlayer->mSelectedGroup->Contains(theComplex);
    case EE_UNITS_EU_POST_424: gUserInterface->PostMessage424Once(); return 0;
    // eu_body.rs's
    case EE_UNITS_EU_REGIONS: return (uint64_t)(uintptr_t)(gWorld.mMap ? gWorld.mMap->mUnitRegions : 0);
    case EE_UNITS_EU_TILE: return (uint64_t)(uintptr_t)theUnit->mTile;
    case EE_UNITS_EU_ADD_OFF_MAP: theUnit->mPlayer->AddOffMapUnit(theUnit); return 0;
    case EE_UNITS_EU_REMOVE_OFF_MAP: theUnit->mPlayer->RemoveOffMapUnit(theUnit); return 0;
    case EE_UNITS_EU_KEEP_COLLISION_COUNT: theComplex->mF24C = theComplex->mCollisionResolutionCount; return 0;
    case EE_UNITS_EU_CLEAR_COLLISION_COUNT: theComplex->mCollisionResolutionCount = 0.0f; return 0;
    case EE_UNITS_EU_CLOAKED_ENEMY: return ee_units_uu_cloaked_enemy((uint64_t)(uintptr_t)theUnit, (uint64_t)(uintptr_t)gWorld.GetCurrentPlayer());
    case EE_UNITS_EU_IS_FOUNDATION: return theUnit->IsFoundation();
    case EE_UNITS_EU_MINIMAP_MOVE: gMiniMap->MoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_SIGHT_MOVED: theUnit->SightMoved(theUnit->mTileX, theUnit->mTileY, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_CLASS_TILE_SIZE: return theUnit->mUnitClass->mTileSize;
    case EE_UNITS_EU_REGIONS_SIZE: return ee_combat_regions_size(EEEuObject<EeCombatUnitRegions>(inA));
    case EE_UNITS_EU_HAS_MAP: return gWorld.mMap != NULL;
    case EE_UNITS_EU_MAP_TILE_EXISTS: return gWorld.mMap->GetTile((long)inB, (long)inC) != NULL;
    case EE_UNITS_EU_REMOVE_FROM_REGION: theUnit->mUnitClass->RemoveFromRegion(theUnit); return 0;
    case EE_UNITS_EU_HAS_INFLUENCE: return theUnit->mUnitClass->mInfluence != 0;
    case EE_UNITS_EU_INFLUENCE_REMOVE: gWorld.mInfluenceTable->RemoveUnit(theUnit, theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_REMOVE_FROM_TILE: theUnit->mUnitClass->RemoveFromTile(theUnit); return 0;
    case EE_UNITS_EU_MARK_MOVED: theUnit->mHiddenFlags |= kHiddenFlagMoved; return 0;
    case EE_UNITS_EU_SET_TILE_Y: theUnit->mTileY = (long)inB; return 0;
    case EE_UNITS_EU_SET_TILE_X: theUnit->mTileX = (long)inB; return 0;
    case EE_UNITS_EU_CLASS_HIDES: return theUnit->mUnitClass->mIsObstacle && !(theUnit->mUnitClass->mDomains & kMovementAir) && !theUnit->mUnitClass->mIsProjectile && theUnit->mUnitClass->mMaxVelocity == 0.0f;
    case EE_UNITS_EU_UPDATE_HIDDEN: theUnit->UpdateHidden(); return 0;
    case EE_UNITS_EU_SET_TILE: theUnit->mTile = gWorld.mMap->GetTile((long)inB, (long)inC); return 0;
    case EE_UNITS_EU_ADD_TO_TILE: theUnit->mUnitClass->AddToTile(theUnit); return 0;
    case EE_UNITS_EU_ADD_TO_REGION: theUnit->mUnitClass->AddToRegion(theUnit); return 0;
    case EE_UNITS_EU_INFLUENCE_ADD: gWorld.mInfluenceTable->AddUnit(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_EU_CLASS_INACTIVE: return theUnit->mUnitClass->mIsInactive;
    case EE_UNITS_EU_ACTIVATE_COMPLEX: gWorld.ActivateUnit(theComplex); return 0;
    case EE_UNITS_EU_RESORT: gWorld.ResortUnit(theUnit); return 0;
    case EE_UNITS_EU_SIGHT_OFF_HERE: theUnit->SightOff(theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_HAS_DOPPELGANGER:
        return gDoppelgangerManager.GetDoppelganger(EEEuObject<EETile>(inA), theUnit, theUnit->mPlayer->mPlayerID) != NULL;
    case EE_UNITS_EU_DOPPELGANGER_TILE_SHOWN:
        gDoppelgangerManager.TileShown(EEEuObject<EETile>(inA), theUnit->mTileX, theUnit->mTileY, theUnit->mPlayer);
        return 0;
    case EE_UNITS_EU_HAS_DEATH_EFFECT: return theUnit->mUnitClass->mDeathEffect != NULL;
    case EE_UNITS_EU_DEATH_EFFECT: gEffectManager->CreateUnitEffect(theUnit, theUnit->mUnitClass->mDeathEffect, NULL, 0, 0, 0); return 0;
    case EE_UNITS_EU_HAS_DEATH_SOUND: return theUnit->mStats->mDeathSound != NULL;
    case EE_UNITS_EU_DEATH_SOUND_PARAMETERS:
        theUnit->mUnitClass->GetSoundParameters(theUnit, theUnit->mStats->mDeathSound, &sDeathVolume, &sDeathPan, &sDeathPitch);
        return 0;
    case EE_UNITS_EU_DEATH_SOUND_PLAY:
        gSoundManager->Play(theUnit->mStats->mDeathSound->mSampleID, sDeathVolume, sDeathPan, sDeathPitch, theUnit->mStats->mDeathSound->mLooping, NULL);
        return 0;
    case EE_UNITS_EU_SET_RESOURCE_AMOUNT: theUnit->mResourceAmount = (long)inB; return 0;
    case EE_UNITS_EU_SIGHT_ON_HERE: theUnit->SightOn(theUnit->mTileX, theUnit->mTileY); return 0;
    case EE_UNITS_EU_CAMERA_UNIT_IS_THIS: return (inB == 0 ? gWorld.mCameraControl.mUnitA : gWorld.mCameraControl.mUnitB) == theUnit;
    case EE_UNITS_EU_CLEAR_CAMERA_UNIT: gWorld.mCameraControl.ClearUnitA(); return 0;
    case EE_UNITS_EU_CLEAR_CAMERA_UNIT_B: gWorld.mCameraControl.ClearUnitB(); return 0;
    case EE_UNITS_EU_UNIT_TAKEN: theUnit->mPlayer->UnitTaken(theUnit, inB != 0); return 0;
    case EE_UNITS_EU_TILE_VISIBLE: return gWorld.mMap->GetTile((long)inB, (long)inC)->IsVisibleTo(theUnit->GetPlayer());
    case EE_UNITS_EU_CARRIES: return theUnit->GetStats()->mCarryAmount > 0;
    case EE_UNITS_EU_RESOURCE_AMOUNT_OF: return theOther->mResourceAmount;
    case EE_UNITS_EU_RESOURCE_TYPE_OF: return theOther->mResourceType;
    case EE_UNITS_EU_CATEGORY_OF: return theOther->GetUnitClass()->GetCategory();
    case EE_UNITS_EU_UNIT_TYPE_OF: return theOther->GetUnitClass()->GetUnitType();
    case EE_UNITS_EU_SAME_PLAYER: return theOther->GetPlayer() == theUnit->GetPlayer();
    case EE_UNITS_EU_IS_BUILT_OF: return theOther->GetComplexUnit()->mIsBuilt;
    case EE_UNITS_EU_FULL_HEALTH_OF: return theOther->GetHitPoints() == theOther->GetStats()->mMaxHitPoints;
    case EE_UNITS_EU_TERRAIN_OF: return theOther->mTile->mTerrainType;
    case EE_UNITS_EU_ENEMY_OF: return theUnit->GetPlayer()->GetEnemy(theOther->GetPlayer()->mPlayerID) != 0;
    case EE_UNITS_EU_HIT_POINTS_OF: return theOther->GetHitPoints();
    case EE_UNITS_EU_CONTINENT_AT: return (uint64_t)(uintptr_t)gContinentManager.GetContinentAt(theUnit);
    case EE_UNITS_EU_CONTINENT_CATEGORY: return EEEuObject<EEAIContinent>(inA)->GetCategory();
    case EE_UNITS_EU_MAP_SIZE: return inB == 0 ? (int64_t)gMapWidth : (int64_t)gMapHeight;
    case EE_UNITS_EU_CONTINENT_ID: return gContinentManager.GetContinentID((long)inB, (long)inC);
    case EE_UNITS_EU_CONTINENT_BY_ID: return (uint64_t)(uintptr_t)gContinentManager.GetContinentByID((unsigned short)inB);
    case EE_UNITS_EU_TILE_DISTANCE_SQUARED: return EEEuBits(theUnit->GetTileDistanceSquared(theOther));
    case EE_UNITS_EU_TILE_SIZE_OF: return theOther->mUnitClass->mTileSize;
    case EE_UNITS_EU_HAS_TILE_NODE: return theUnit->mTileNode != NULL;
    case EE_UNITS_EU_KILL_UNIT: theUnit->mPlayer->KillUnit(theUnit, false); return 0;
    case EE_UNITS_EU_TAKE_RESOURCE: theUnit->mResourceAmount -= (long)inB; return 0;
    case EE_UNITS_EU_RESOURCE_AMOUNT: return theUnit->mResourceAmount;
    case EE_UNITS_EU_NO_PREVIOUS_PART: return theComplex->mPreviousPart == NULL;
    default: return 0;
    }
}

// The statements of eu_more.rs's bodies (EE_UNITS_EUM_*; no preamble: Init and ResetEEUnit run on a unit whose
// members are not set yet)
extern "C" int64_t ee_units_eum_host(int32_t inOp, void* inUnit, uint64_t inA, int64_t inB, int64_t inC)
{
    EEUnit* theUnit = static_cast<EEUnit*>(inUnit);
    switch (inOp) {
    case EE_UNITS_EUM_UNIT_REMOVED: theUnit->mUnitClass->UnitRemoved(); return 0;
    case EE_UNITS_EUM_SHUTDOWN: return UShutdownInProgress();
    case EE_UNITS_EUM_CLEARING: return gWorld.mClearing;
    case EE_UNITS_EUM_POST_UNLOAD: gDataPreLoading->PostUnloadUnitClass(theUnit->mPlayer->mPlayerID, theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_UNLOAD_MEDIA: theUnit->mPlayer->UnloadUnitClassMedia(theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_ZERO_HIT_POINTS: theUnit->mHitPoints = 0; return 0;
    case EE_UNITS_EUM_HAS_COMPLEX: return theUnit->mComplexUnit != NULL;
    case EE_UNITS_EUM_DEACTIVATE: gWorld.DeactivateUnit(theUnit->mComplexUnit); return 0;
    case EE_UNITS_EUM_RTS_INVALIDATE: theUnit->RTSUnit::Invalidate(); return 0;
    case EE_UNITS_EUM_AS_UNIT: return EEObj(dynamic_cast<EEUnit*>(EEEuObject<RTSUnit>(inA)));
    case EE_UNITS_EUM_COPY_RESOURCE_TYPE: theUnit->mResourceType = EEEuObject<EEUnit>(inA)->mResourceType; return 0;
    case EE_UNITS_EUM_UPGRADE_MAX_HIT_POINTS: return EEEuObject<EEUnit>(inA)->GetPlayer()->mUnitClassUpgrades[EEEuObject<EEUnit>(inA)->GetUnitClass()->mUnitType]->mMaxHitPoints;
    case EE_UNITS_EUM_HIT_POINTS_OF: return EEEuObject<EEUnit>(inA)->GetHitPoints();
    case EE_UNITS_EUM_SET_HIT_POINTS_FIELD: theUnit->mHitPoints = (long)inB; return 0;
    case EE_UNITS_EUM_COPY_RESOURCE_AMOUNT: theUnit->mResourceAmount = EEEuObject<EEUnit>(inA)->mResourceAmount; return 0;
    case EE_UNITS_EUM_RTS_COPY_FROM: theUnit->RTSUnit::CopyFrom(EEEuObject<RTSUnit>(inA)); return 0;
    case EE_UNITS_EUM_INITIALIZE: theUnit->Initialize((long)inB); return 0;
    case EE_UNITS_EUM_REFERENCE_COUNT: return theUnit->mReferenceCount;
    case EE_UNITS_EUM_REFERENCE_ONE: theUnit->mReferenceCount = 1; return 0;
    case EE_UNITS_EUM_SET_PLAYER: theUnit->mPlayer = EEEuObject<EEPlayer>(inA); return 0;
    case EE_UNITS_EUM_SET_CLASS: theUnit->mUnitClass = EEEuObject<EEUnitClass>(inA); return 0;
    case EE_UNITS_EUM_SET_STATS: theUnit->mStats = theUnit->mPlayer->mUnitClassUpgrades[theUnit->mUnitClass->mUnitType]; return 0;
    case EE_UNITS_EUM_MOVE_TO_TILE: theUnit->MoveTo((long)inB, (long)inC, 0); return 0;
    case EE_UNITS_EUM_SET_COMPLEX_SELF: theUnit->mComplexUnit = static_cast<EEComplexUnit*>(theUnit); return 0;
    case EE_UNITS_EUM_ZERO_RESOURCE_AMOUNT: theUnit->mResourceAmount = 0; return 0;
    case EE_UNITS_EUM_RESOURCE_TYPE_NONE: theUnit->mResourceType = kResourceNone; return 0;
    case EE_UNITS_EUM_CLEAR_CLASS: theUnit->mUnitClass = NULL; return 0;
    case EE_UNITS_EUM_CLEAR_COMPLEX: theUnit->mComplexUnit = NULL; return 0;
    case EE_UNITS_EUM_CLEAR_HIDDEN: theUnit->mHiddenFlags = 0; return 0;
    case EE_UNITS_EUM_CLEAR_STATS: theUnit->mStats = NULL; return 0;
    case EE_UNITS_EUM_CARBON_ROCK: return EEUnit::IsCarbonRock(theUnit->mUnitClass->mUnitType);
    case EE_UNITS_EUM_FOREST: return EEObj(gContinentManager.GetForestAt(theUnit));
    case EE_UNITS_EUM_GAME_TIME: return gPhysics->mGameTime;
    case EE_UNITS_EUM_FOREST_DUE: return EEEuObject<EEAIForest>(inA)->GetNextRenewal() <= gPhysics->mGameTime;
    case EE_UNITS_EUM_FOREST_TILES: return EEEuObject<EEAIForest>(inA)->GetNumTiles();
    case EE_UNITS_EUM_CATEGORY_OF: return EEEuObject<EEComplexUnit>(inA)->mUnitClass->mCategory;
    case EE_UNITS_EUM_RANDOM: return gPhysics->GetRandom((long)inB, (long)inC);
    case EE_UNITS_EUM_REGROW: EEEuObject<EEAIForest>(inA)->RegrowTrees(U2DPoint<long>(theUnit->mTileX, theUnit->mTileY), (long)inB, (long)inC); return 0;
    case EE_UNITS_EUM_SET_NEXT_RENEWAL: EEEuObject<EEAIForest>(inA)->SetNextRenewal((long)inB); return 0;
    case EE_UNITS_EUM_SET_CLASS_MEDIA: theUnit->mPlayer->SetUnitClassMedia(theUnit->mUnitClass->mUnitType, 0x7f); return 0;
    case EE_UNITS_EUM_POST_LOAD: gDataPreLoading->PostLoadUnitClass(theUnit->mPlayer->mPlayerID, theUnit->mUnitClass->mUnitType); return 0;
    case EE_UNITS_EUM_LAST_ATTACKER: return EEObj(static_cast<EEComplexUnit*>(theUnit)->mLastAttacker);
    case EE_UNITS_EUM_DEREFERENCE_LAST_ATTACKER: static_cast<EEComplexUnit*>(theUnit)->mLastAttacker->Dereference(); return 0;
    case EE_UNITS_EUM_SET_LAST_ATTACKER: static_cast<EEComplexUnit*>(theUnit)->mLastAttacker = EEEuObject<EEComplexUnit>(inA); return 0;
    case EE_UNITS_EUM_REFERENCE_OF: EEEuObject<EEComplexUnit>(inA)->Reference(); return 0;
    default: return 0;
    }
}
// Remembers the unit's last attacker, moving the reference from the previous one (eu_more.rs)
void EEComplexUnit::SetLastAttacker(EEComplexUnit* inUnit)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_SET_LAST_ATTACKER, static_cast<EEUnit*>(this), inUnit, 0, 0);
}

// Regrows trees in the forest of a cut tree when its renewal is due (fewer, nearer ones when a citizen cut it), then
// schedules the next renewal 125-275 s ahead; never for carbon rock (eu_more.rs)
void EEUnit::RegrowForest(EEComplexUnit* inUnit)
{
    ee_units_eu_more(EE_UNITS_EUM_DO_REGROW_FOREST, this, inUnit, 0, 0);
}

// Returns whether inUnitType is one of the carbon rock unit types
bool EEUnit::IsCarbonRock(unsigned long inUnitType)
{
    return inUnitType == 0x2fb || inUnitType == 0x306 || inUnitType == 0x307
        || (inUnitType >= 0x340 && inUnitType <= 0x345) || inUnitType == 0x348 || inUnitType == 0x24b
        || inUnitType == 0x24c;
}

// Does nothing (a simple unit has no sight to move)
void EEUnit::SightMoved(unsigned long, unsigned long, unsigned long, unsigned long)
{
}

// The empty slots EEUnit overrides; ICF folded each into a shared body elsewhere (no address of their own):
void EEUnit::SightOnHere()
{
}

void EEUnit::SightOffHere()
{
}

void EEUnit::SightOn(unsigned long, unsigned long)
{
}

void EEUnit::OutOfSynchDump(FILE*, unsigned long)
{
}

bool EEUnit::IsFoundation()
{
    return false;
}

void EEUnit::SightOff(unsigned long, unsigned long)
{
}
