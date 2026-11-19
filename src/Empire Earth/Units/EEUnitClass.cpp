// C:\EEX\Empire Earth\Units\EEUnitClass.cpp (__FILE__ 0x2358b1's string). The unit class base: its `$E` globals, the
// ctor 0x22f317 (copies a dbunits record), dtor 0x2305ee and members, the class factory 0x2358b1 (one inline ctor per
// EEUC* subclass), the EEUC* inline dtors and slots emitted with their vtables (0x2362e3..0x23649e), more EEUnitClass
// members and the EEFOFindNewTarget* functors to 0x23a5bf (EEUnitClassImprovement.cpp follows).
// Range 0x22f272..0x23a5bf. Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <cstddef>
#include <string.h>
#include "ee/EEUnitUpdate.h"
#include "ee/EEUCUpgradeTable.h"
#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"   // the tag points' model chain (EECombatTagPointChain)
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/UResourceFile.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitVectorItem.h"
#include "lle/FSPersistent.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEUGRust.h"
#include "ee/EEAEManager.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEWheel.h"
#include "ee/EECalamityManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPASGFlags.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EECombatFormulas.h"
#include "ee/EECombatGoals.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEAnimal.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitClassRecord.h"
#include "ee/EEDbObject.h"
#include "ee/EEDatabase.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEFOFindNewTargetInMeleeRange.h"
#include "ee/EEMathInline.h"
#include <algorithm>
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kHeadingUnset = -10000.0f;
static const float kSoundDistanceFactor = 4.0f;  // 0x447ac4 (Virtual2C)
static const float kMaxLineOfSight = 32.0f;      // 0x447ac8 (the ctor: the largest line of sight)
static const float kBuildingZOffset = 0.01f;
static const float kCloakFadeTime = 750.0f;
static const float kCloakFadeMaxOpacity = 1.0f;
static const float kCloakFadeMinOpacity = 0.6f;
static const float k447ADC = 0.40824828f;        // 0x447adc (1 / sqrt(6))
static const float kFacingTolerance = 0.043633234f;
extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)
static const float kFleeDistanceScale = 1.5f;
static const float k447AF0 = 0.01f;
static const float kLargeUnitScale = 1.5f;
static const float kMinConstructionScale = 0.1f;
static const float k447AFC = 10.0f;
static const float kMinWaypointDistanceSquared = 36.0f;
static const float kMinDamageRoll = 0.05f;
static const float kMaxDamageRoll = 0.2f;
// the wall pieces' yaws (0x446dc0, one object shared with EEGhostUnit.cpp and EEUCBuilding.cpp; ReplantFarm)
static const float kWallRotations[4] = {0.0f, 3.1415927f, -1.5707964f, 1.5707964f};

[[maybe_unused]] static float sCloakFadeRate = 1.0f / kCloakFadeTime;  // 0x5636a8: the cloak fade per millisecond (provisional name; the fade is ee_units step.rs cloak_fade)

[[maybe_unused]] static float sCloakFadeOpacityRange = kCloakFadeMaxOpacity - kCloakFadeMinOpacity;  // 0x5636a4: the cloak fade's opacity range (provisional name)

static float sUnusedHalfInverseSqrt6 = k447ADC * 0.5f;

static float sUnusedFourInverseSqrt6 = k447ADC * 4.0f;

// the unit classes by type (0x5636ac in the original: 0x367 entries up to sUnitTypes at 0x564448; ee/EEUnit.h)
// (the original's fixed EEUnitClassCount() slots; now one per unit type, sized by CreateUnitClasses: EEUnitClassCount() empty slots until then)
std::vector<EEUnitClass*> gUnitClassTable(EEUnitClassCount());
// .data 0x4cc9fc/0x4cca00 (= 0.7, 1.5; this unit's data follows: ee/EERenderLists.h, EERenderObject.cpp reads them)
float gSpaceTurretHeight = 0.7f;
float gSpaceStationHeight = 1.5f;

// 0x564448: the unit types registered in gUnitClassTable (Register 0x236a9c push_backs; the vector right after the
// table, guarded dtor bit 1 of 0x563698: a class static).
std::vector<unsigned long> EEUnitClass::sUnitTypes;

// Creates a unit class from its dbunits record: stats, sounds and effects (referenced), terrain table, movement
// domains and kind flags. Port (R5b.4 round 27): the plain stores are ee_units unit_class_fields.rs's fill of the
// class's record base from the row, given the lookups it reads (the tech tree's epoch and calamity technology, the
// animal id, the AI script); the media, effects and the improvement table stay here, in the original's order.
EEUnitClass::EEUnitClass(long inIsComplex, EEUnitClassRecord* inRecord, EEPhysicsModel* inPhysicsModel)
{
    this->mPhysicsModel = inPhysicsModel;
    this->mResearchTechnologies.resize(inRecord->mNumAttributes);   // (the original's fill value was never set; all overwritten)
    for (unsigned long i = 0; i < inRecord->mNumAttributes; i++)
        this->mResearchTechnologies[i] = inRecord->mAttributes[i];
    this->mIsMediaLoaded = false;
    this->mButton = EEButtonObject::Get(inRecord->mButton);
    this->mModel = (EEModel*)EEMedia::Get(inRecord->mGraphic);
    this->mCreationSound = EESoundObject::Get(inRecord->mCreationSound);
    this->mAcknowledgeSounds[0] = EESoundObject::Get(inRecord->mAcknowledgeSound1);
    this->mAcknowledgeSounds[1] = EESoundObject::Get(inRecord->mAcknowledgeSound2);
    this->mSelectionSounds[0] = EESoundObject::Get(inRecord->mSelectionSound1);
    this->mSelectionSounds[1] = EESoundObject::Get(inRecord->mSelectionSound2);
    this->mDeathSound = EESoundObject::Get(inRecord->mDeathSound);
    if (inRecord->mLineOfSight > kMaxLineOfSight)
        ESDebugOut("EEUnitClass::EEUnitClass: unit %s has an LOS of %f.\n", inRecord->mName, inRecord->mLineOfSight);
    this->mAIScriptName.Copy(inRecord->mAIScriptName, 0, strlen(inRecord->mAIScriptName), 0, true);
    this->mName.Copy(inRecord->mName, 0, strlen(inRecord->mName), 0, true);
    EeUnitsUnitClassFillIn theIn = {};
    theIn.is_complex = inIsComplex;
    EEAnimal* theAnimal = EEAnimal::Find(inRecord->mUnitType);
    theIn.has_animal = theAnimal != NULL;
    theIn.animal_id = theAnimal != NULL ? theAnimal->mAnimalID : 0;
    EETechTreeNode* theNode = gTechTree.Get(inRecord->mTechnology);
    theIn.has_tech_node = theNode != NULL;
    theIn.tech_epoch = theNode != NULL ? theNode->mEpoch : 0;
    for (unsigned long j = 0; j < this->GetNumResearchTechnologies() && !theIn.has_calamity_powers; j++) {
        EETechTreeNode* theTechnology = gTechTree.Get(this->mResearchTechnologies[j]);
        theIn.has_calamity_powers = theTechnology != NULL && theTechnology->mData->mKind == 5;
    }
    // (the two layouts must agree on every target: the entries and the maximum level at the same offsets)
    static_assert(offsetof(EEUnitClassImprovementTable, mEntries) == offsetof(EEUCUpgradeTable, mFactors)
        && offsetof(EEUnitClassImprovementTable, mMaxLevel) == offsetof(EEUCUpgradeTable, mMaxLevel)
        && sizeof(EEUnitClassImprovementEntry) == sizeof(EEUCUpgradeFactor), "improvement table layout");
    this->mImprovements = reinterpret_cast<EEUnitClassImprovementTable*>(EEUCUpgradeTable::sTable[inRecord->mUpgradeTableIndex]);
    // each effect referenced as it is found, in the original's order
    auto theEffect = [](long inID) {
        EEGraphicEffect* theResult = EEGraphicEffect::Get(inID);
        if (theResult != NULL)
            theResult->Reference();
        return theResult;
    };
    this->mDeathEffect = theEffect(inRecord->mDeathEffect);
    this->mDamageEffect = theEffect(inRecord->mDamageEffect);
    this->mHealEffect = theEffect(inRecord->mHealEffect);
    this->mFireTrailEffect = theEffect(inRecord->mUnusedFireTrailEffect);
    this->mTrailEffect = theEffect(inRecord->mTrailEffect);
    this->mMuzzleFlashEffect = theEffect(inRecord->mMuzzleFlashEffect);
    this->mRippleEffect = theEffect(inRecord->mRippleEffect);
    this->mSplashEffect = theEffect(inRecord->mSplashEffect);
    this->mImpactSparksEffect = theEffect(inRecord->mImpactSparksEffect);
    this->mImpactBloodEffect = theEffect(inRecord->mImpactBloodEffect);
    this->mAttachedEffect = theEffect(inRecord->mAttachedEffect);
    this->mEffect090 = theEffect(inRecord->mUnusedEffect1);
    this->mCreationEffect = theEffect(inRecord->mCreationEffect);
    this->mEffect098 = theEffect(inRecord->mUnusedEffect2);
    this->mEffect09C = theEffect(inRecord->mUnusedEffect3);
    this->mEffect0A0 = theEffect(inRecord->mUnusedEffect4);
    this->mEffect0A4 = theEffect(inRecord->mUnusedEffect5);
    this->mEffect0A8 = theEffect(inRecord->mUnusedEffect6);
    this->mSpecialAttackEffect = theEffect(inRecord->mSpecialAttackEffect);
    this->mEffect0B0 = theEffect(inRecord->mUnusedEffect7);
    this->mShotEffect = theEffect(inRecord->mShotEffect);
    this->mLeprosyEffect = theEffect(inRecord->mLeprosyEffect);
    this->mPlagueEffect = theEffect(inRecord->mPlagueEffect);
    this->mPulseCannonEffect = theEffect(inRecord->mPulseCannonEffect);
    this->mMoraleEffect = theEffect(inRecord->mMoraleEffect);
    this->mBattleCryEffect = theEffect(inRecord->mBattleCryEffect);
    this->mShellHoleEffect = theEffect(inRecord->mShellHoleEffect);
    this->mEffect0D0 = theEffect(inRecord->mUnusedEffect8);
    this->mBloodStainEffect = theEffect(inRecord->mBloodStainEffect);
    this->mProjectileClass = NULL;
    this->mScaffoldingClass = NULL;
    this->mDeathClass = NULL;
    this->mDeathSpawnClass = NULL;
    this->mPackedClass = NULL;
    this->mRemainsClass = NULL;
    if (this->mAIScriptName.GetLength() > 0)
        this->mAIScript = EEUnitAction::GetScript(this->mAIScriptName);
    else
        this->mAIScript = NULL;
    theIn.ai_script = (uint64_t)(uintptr_t)this->mAIScript;
    ee_units_unit_class_fill(static_cast<EeUnitsUnitClassFields*>(this), inRecord, &theIn);
    this->mBehavior = EEUnitBehavior::sTable[inRecord->mBehavior];
}

// the class's effects as GetEffects' array (the members are contiguous: 29 pointers from mAttachedEffect)
static const long kUnitClassEffectCount = 29;
static_assert(offsetof(EEUnitClass, mBloodStainEffect) - offsetof(EEUnitClass, mAttachedEffect)
                  == (kUnitClassEffectCount - 1) * sizeof(EEGraphicEffect*), "the class's effects are one array");

// Destroys the unit class, releasing its graphic effects and its projectile, scaffolding, death and remains classes
EEUnitClass::~EEUnitClass()
{
    for (long i = 0; i < kUnitClassEffectCount; i++)   // the 29 effects, in their order (+0x64..+0xd4)
        if (this->GetEffects()[i] != NULL)
            this->GetEffects()[i]->Dereference();
    this->mAIScript = NULL;
    if (this->mProjectileClass != NULL) {
        this->mProjectileClass->Dereference();
        this->mProjectileClass = NULL;
    }
    if (this->mScaffoldingClass != NULL) {
        this->mScaffoldingClass->Dereference();
        this->mScaffoldingClass = NULL;
    }
    if (this->mDeathClass != NULL) {
        this->mDeathClass->Dereference();
        this->mDeathClass = NULL;
    }
    if (this->mDeathSpawnClass != NULL) {
        this->mDeathSpawnClass->Dereference();
        this->mDeathSpawnClass = NULL;
    }
    if (this->mPackedClass != NULL) {
        this->mPackedClass->Dereference();
        this->mPackedClass = NULL;
    }
    if (this->mRemainsClass != NULL) {
        this->mRemainsClass->Dereference();
        this->mRemainsClass = NULL;
    }
}

// Adds a waypoint at a map point, inZ above the tile's ground (on water at inZ); off the map it snaps or is dropped
void EEUnitClass::AddWaypointAt(EEComplexUnit* inUnit, float inX, float inY, float inZ, bool inSnapToMap)
{
    GE3DPoint theLocation;
    long theX = UMFloor(inX);
    long theY = UMFloor(inY);
    EEMapTile* theTile = ::gWorld.mMap->GetTile(theX, theY);
    if (theTile == NULL) {
        if (inSnapToMap != true)
            return;
        GE3DPoint thePoint(inX, inY, inZ);
        theTile = ::gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY);
        if (theTile == NULL)
            return;
    }
    theLocation.SetX(inX);
    theLocation.SetY(inY);
    float theHeight = theTile->mHeight;
    if (theHeight >= 0.0f)
        theLocation.SetZ(theHeight + inZ);
    else
        theLocation.SetZ(inZ);
    inUnit->AddWaypoint(theLocation, inSnapToMap);
}

// Returns a new building's height and flattens the ground under multi-tile land buildings to their lowest corner
// (granaries take the average; space turrets and orbital stations sit higher)
// NEAR: the original calls the map's GetTile out of line (0x2f85) at the top; loop counters in swapped slots
void EEUnitClass::LevelGround(GETerrainMesh* inMesh, unsigned long inX, unsigned long inY, float* outZ)
{
    static const float kWallHeight = 0.7f;      // 0x4cc9fc (unit type 0x302)
    static const float kTowerHeight = 1.5f;     // 0x4cca00 (unit type 0x31f)
    EEMap* theMap = ::gWorld.GetMap();
    float theMinimum = 3.4028235e38f;
    if (theMap == NULL || theMap->GetTile(inX, inY) == NULL)
        return;
    unsigned long y;
    unsigned long x;
    if (this->GetUnitType() == kUnitTypeGranaryImproved || this->GetUnitType() == kUnitTypeGranary) {
        float theSum = 0.0f;
        unsigned long theCount = 0;
        for (x = inX; x <= inX + this->GetTileSize(); x++) {
            for (y = inY; y <= inY + this->GetTileSize(); y++) {
                theSum += inMesh->GetZ(x, y);
                theCount++;
            }
        }
        *outZ = theSum / theCount + kBuildingZOffset;
    } else if (this->IsLandBuilding() && this->GetTileSize() > 1 && this->GetUnitType() != kUnitTypeFarm && this->GetUnitType() != kUnitTypeVolcano) {
        for (x = inX; x <= inX + this->GetTileSize(); x++) {
            for (y = inY; y <= inY + this->GetTileSize(); y++) {
                float theZ = inMesh->GetZ(x, y);
                if (theZ < theMinimum && theZ >= 0.0f)
                    theMinimum = theZ;
            }
        }
        if (theMinimum == 3.4028235e38f)
            theMinimum = 0.0f;
        for (x = inX; x <= inX + this->GetTileSize(); x++) {
            for (y = inY; y <= inY + this->GetTileSize(); y++) {
                if (inMesh->GetZ(x, y) >= 0.0f)
                    inMesh->SetZ(x, y, theMinimum);
            }
        }
        for (x = inX; x < inX + this->GetTileSize(); x++) {
            for (y = inY; y < inY + this->GetTileSize(); y++) {
                EEMapTile* theTile = theMap->GetTile(x, y);
                if (theTile != NULL && theTile->mHeight >= 0.0f)
                    theTile->mHeight = theMinimum;
            }
        }
        *outZ = kBuildingZOffset + theMinimum;
        gCameraFollowing = false;
    } else if (this->IsLandBuilding() && this->GetTileSize() == 1 && this->GetUnitType() != kUnitTypeFarm && this->GetUnitType() != kUnitTypeVolcano && this->GetUnitType() != kUnitTypeCameraObject) {
        *outZ = inMesh->GetZ(inX, inY);
    }
    if (this->GetUnitType() == kUnitTypeSpaceTurret)
        *outZ = kWallHeight + *outZ;
    else if (this->GetUnitType() == kUnitTypeOrbitalSpaceStation)
        *outZ = kTowerHeight + *outZ;
}

// (0x630c55 ApplyAreaDamage, 0x630fd2 ApplyHit and the damage of one hit: ee_combat damage.rs since the tenth
// round, over the world host in EECombatGoals.cpp)

// Fires one shot of inUnit's attack (a direct hit, a projectile or a special attack), then plays the muzzle effects,
// uses up ammunition and moves to the next weapon tag point
// The shot's target and drop point come from EEFindDropLocation; a direct shot checks the line for a blocker
// (EEIsLineToUnitBlocked: a friendly blocker only flashes, an enemy one takes the hit, open ground gets a puff); a
// projectile class launches by its turret parts; the capital ships' special attack (0x2da/0x33c) fires from the last
// tag point
void EEUnitClass::FireShot(EEComplexUnit* inUnit)
{
    // ee_combat world.rs fire_shot: the plan's operations on the world host (EECombatGoals.cpp: the drop point, the
    // line, the hit, the launch and the effects stay C++ behind them); CBXA records the outcome
    EECombatWorld(EE_COMBAT_WORLD_FIRE_SHOT, inUnit, NULL, this);
    EECombatCaptureXAction(EE_COMBAT_XA_FIRE_SHOT, inUnit, EECombatShotTarget(inUnit), 0);
}

// Returns whether units of this class may board a container of inClass (transport, fortress, carrier, airport ...)
bool EEUnitClass::CanEnter(EEUnitClass* inClass)
{
    return (this->mCanEnterAirTransports && inClass->mCategory == kUnitCategoryHelicopterTransport) || (this->mCanEnterLandTransports && inClass->mCategory == kUnitCategoryLandTransport) ||
           (this->mCanEnterSeaTransports && inClass->mCategory == kUnitCategoryNavalTransport) || (this->mCanEnterSpaceTransports && inClass->mCategory == kUnitCategorySpaceTransport) ||
           (this->mCanEnterFortresses && inClass->mCategory == kUnitCategoryFortress) || (this->mCanEnterCarriers && inClass->mAIKind == kAIKindAircraftCarrier) ||
           (this->mCanEnterAirports && inClass->mUnitType == kUnitTypeAirport) || (this->mCanEnterNavyYards && inClass->mUnitType == kUnitTypeNavyYard) ||
           (this->mUnitType == kUnitTypeSp15SpaceFighter && inClass->mUnitType == kUnitTypeSp15SpaceCarrier);
}

// Returns the high-ground damage factor of a land attacker (or a projectile's launcher) on a land unit or building
float EEUnitClass::GetElevationFactor(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    long theCategory = this->mCategory;
    long theTargetCategory = inTarget->mUnitClass->mCategory;
    bool theOwnHeight;
    const EECombatRules& theRules = EEGetRules().mCombat;
    if (theRules.IsElevationLauncher(theCategory)) {
        theCategory = inUnit->mLauncherClass->mCategory;
        theOwnHeight = false;
    } else
        theOwnHeight = true;
    // which attackers and targets the height difference scales: combat.yaml's elevation categories
    EeCombatElevationIn theIn = {};
    theIn.applies = theRules.IsElevationAttacker(theCategory) && theRules.IsElevationTarget(theTargetCategory) &&
                    inTarget != NULL;
    if (theIn.applies) {
        theIn.own_z = theOwnHeight ? inUnit->mPhysicalModel.GetLocation().GetZ() : inUnit->mHeight;
        theIn.target_z = inTarget->mPhysicalModel.GetLocation().GetZ();
        theIn.high_height = gWorldTable.GetFloatValue(kWorldHighGroundHeight);
        theIn.low_height = gWorldTable.GetFloatValue(kWorldLowGroundHeight);
        theIn.high_bonus = inUnit->mPlayer->mHighGroundBonus;
        theIn.high_factor = gWorldTable.GetFloatValue(kWorldHighGroundAttackFactor);
        theIn.low_factor = gWorldTable.GetFloatValue(kWorldLowGroundAttackFactor);
    }
    return EECombatElevation(theIn);
}

// Fills the AI's per-tile-kind passability flags for the unit or class (an air unit passes everything)
void EEUnitClass::GetTerrainFlags(EEComplexUnit* inUnit, EECPASGFlags& outFlags)
{
    EEUnitSet* theSet = gUnitSetManager.GetSet(0x62);
    memset(outFlags.mFlags, 0, sizeof(outFlags.mFlags));
    if (!(this->mDomains & kMovementAir)) {
        bool theWater = this->CanMoveOnTerrain(inUnit, 0xa);
        outFlags.mFlags[kAITileLand] = this->CanMoveOnTerrain(inUnit, 0);
        outFlags.mFlags[kAITileShallowsShore] = theWater;
        outFlags.mFlags[kAITileShallowsNearShore] = theWater;
        outFlags.mFlags[kAITileShallows] = theWater;
        outFlags.mFlags[kAITileCliff] = this->CanMoveOnTerrain(inUnit, 9);
        outFlags.mFlags[kAITileWater] = this->CanMoveOnTerrain(inUnit, 1);
        if (this->CanMoveOnTerrain(inUnit, 1)) {
            outFlags.mFlags[kAITileShore] = this->mWaterKind != 1 && this->mWaterKind != 2;
            outFlags.mFlags[kAITileShallowsShore] = theWater && this->mWaterKind != 1 && this->mWaterKind != 2;
            outFlags.mFlags[kAITileNearShore] = this->mWaterKind != 2;
            outFlags.mFlags[kAITileShallowsNearShore] = theWater && this->mWaterKind != 2;
        }
        if (inUnit != NULL)
            outFlags.mFlags[kAITileForest] = (inUnit->mStats->mTerrainMask >> 14) & 1;
        else if (this->mCanCrossForest)
            outFlags.mFlags[kAITileForest] = true;
        if (theSet->Contains(this->mUnitType))
            outFlags.mFlags[kAITileGate] = true;
    } else
        memset(outFlags.mFlags, 1, sizeof(outFlags.mFlags));
    outFlags.mFlags[kAITileSpace] = this->CanMoveOnTerrain(inUnit, 0xc);
}

// Computes the volume, pan and pitch of a sound played at the unit's position
void EEUnitClass::BaseGetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                            float* outPitch)
{
    GE3DPoint theLocation;
    EEComplexUnit* theOwner = inUnit->GetComplexUnit();
    if (theOwner != NULL) {
        theLocation.SetX(theOwner->mPhysicalModel.GetLocation().GetX());
        theLocation.SetY(theOwner->mPhysicalModel.GetLocation().GetY());
        theLocation.SetZ(theOwner->mPhysicalModel.GetLocation().GetZ());
    } else {
        theLocation.SetX((float)inUnit->GetTileX());
        theLocation.SetY((float)inUnit->GetTileY());
        theLocation.SetZ(inUnit->mTile->mHeight);
    }
    EEComputeSoundParameters(inSound != NULL ? inSound->mVolume : 1.0f, theLocation, *outVolume, *outPan, *outPitch,
                             kSoundDistanceFactor);
}

// Produces a unit of inClass at the building inUnit: loads aircraft and missiles into it, else places the unit toward
// the rally point and hands it the queued goals; returns false at the population cap or with no room
bool EEUnitClass::ProduceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, bool inArg)
{
    GETransformation thePlacement;
    GETransformation theAdjusted;
    GEOrientation theOrientation;
    GE3DPoint theGoalPosition;
    GE3DPoint theGoalPoint;
//
    std::vector<EETileCoordinate> theLineTiles;
    EEPlayer* thePlayer = inUnit->GetPlayer();
    EEComplexUnit* theCreated = NULL;
    unsigned long theQueue = 1;
    bool theLoaded = false;
    bool theSuccess = true;
    bool theHaveGoalPoint = false;
    long theLimit = thePlayer->GetExtraPopulation(inClass->GetUnitType());
    theLimit += thePlayer->GetPopulationLimit(0);
    long theNeeded = thePlayer->GetTotalPopulation(true, false);
    theNeeded += inClass->GetPopulationCost();
    if (!(theNeeded > theLimit)) {
        if ((inUnit->GetUnitClass()->GetUnitType() == kUnitTypeCpMissileBase && inClass->GetCategory() == kUnitCategoryMissile) ||
            inClass->IsAircraft() || inClass->GetCategory() == kUnitCategoryProjectile) {
            EEAirTrafficControl* theTraffic = inClass->mAirTrafficType != 0 ? thePlayer->mAirTraffic[inClass->mAirTrafficType - 1] : 0;
            bool theCanLoad;
            if (theTraffic != NULL) {
                theNeeded = inClass->mWeight;
                theCanLoad = theTraffic->GetUsedCargo() + theNeeded <= theTraffic->GetMaxCargo();
                if (theCanLoad)
                    theCanLoad = theTraffic->GetFreeCargoWeight(inUnit) >= theNeeded;
            } else {
                long theCapacity = inUnit->GetUnitClass()->mTransportCapacity;
                theCanLoad = inClass->mWeight - inUnit->mCargoRoom + theCapacity <= theCapacity;
            }
            if (theCanLoad) {
                thePlacement.SetX(-10000.0f);
                thePlacement.SetY(-10000.0f);
                theCreated = inClass->CreateUnitAt(thePlacement, thePlayer->GetPlayerID(), false, inUnit)->mComplexUnit;
                inUnit->LoadCargo(theCreated, true, 0);
                theLoaded = true;
                if (theCreated->GetUnitClass()->mIsParatrooperBomber)
                    EECreateCarrierAircraft(theCreated, -1);
            } else {
                theSuccess = false;
            }
        } else {
            theQueue = inClass->mFamily == kUnitFamilyHelicopter && inUnit->GetUnitClass()->GetUnitType() != kUnitTypeNavyYard ? 2 : 1;
            long theRings = 1;
            if (!thePlayer->IsHuman())
                theRings = 5;
            if (inUnit->GetQueuedGoal(theQueue) != NULL) {
                if (this->mUnitType != kUnitTypeAirport) {
                    inUnit->GetQueuedGoal(theQueue)->GetGoalLocation(theGoalPoint);
                    theHaveGoalPoint = true;
                } else {
                    EEUnitGoal* theGoal = inUnit->GetQueuedGoal(theQueue);
                    if (theGoal->mKind != kGoalAttackUnit) {
                        // The original does not set the flag on this branch.
                        theGoal->GetGoalLocation(theGoalPoint);
                    } else {
                        EEUnit* theTarget = EEAttackUnitCast(theGoal)->GetAttackTarget();
                        if (theTarget == NULL) {
                            theHaveGoalPoint = false;
                        } else {
                            if (theTarget->GetComplexUnit() != NULL && theTarget->GetComplexUnit()->mContainer != NULL)
                                theTarget->GetComplexUnit()->mContainer->GetPosition(theGoalPoint);
                            else
                                theTarget->GetPosition(theGoalPoint);
                            theHaveGoalPoint = true;
                        }
                    }
                }
            }
            bool thePlaced;
            if ((inClass->mDomains & kMovementSea) != 0)
                thePlaced = EEGetDockShipFacing(&theGoalPoint, inClass, inUnit, theHaveGoalPoint, &thePlacement);
            else if ((inClass->mDomains & kMovementSpace) != 0)
                thePlaced = EEFindDockShipPlacement(&theGoalPoint, inClass, inUnit, theHaveGoalPoint, &thePlacement);
            else
                thePlaced = EEFindPlacement(&theGoalPoint, inClass, inUnit->GetTileX(), inUnit->GetTileY(),
                    this->GetTileSize(), theRings, !thePlayer->IsHuman(), theHaveGoalPoint, false, &thePlacement, inUnit);
            if (!thePlaced) {
                theSuccess = false;
            } else {
                theCreated = inClass->CreateUnitAt(thePlacement, thePlayer->GetPlayerID(), false, inUnit)->mComplexUnit;
                if ((inClass->mDomains & kMovementSea) != 0) {
                    theAdjusted = theCreated->mPhysicalModel.GetLocation();
                    thePlacement.ConvertToOrientation(theOrientation);
                    theAdjusted.SetOrientationYPR(theOrientation.GetYaw(), 0.0f, 0.0f);
                    theCreated->SetTransformation(theAdjusted, true);
                }
                if (inClass->FliesOver()) {
                    theAdjusted = theCreated->mPhysicalModel.GetLocation();
                    thePlacement.ConvertToOrientation(theOrientation);
                    theAdjusted.SetOrientationYPR(theOrientation.GetYaw(), 0.0f, 0.0f);
                    theCreated->SetTransformation(theAdjusted, true);
                }
                EEAreaEffect* theEffect = gAEManager.FindEffectAtTile(8, theCreated->GetTileX(), theCreated->GetTileY(),
                    theCreated->GetPlayer()->GetPlayerID());
                if (theEffect != NULL && theEffect->mUnit != NULL) {
                    EEComplexUnit* theOwner = theEffect->mUnit;
                    GE2DPoint thePoint;
                    thePoint = GE2DPoint(theOwner->mPhysicalModel.GetLocation().GetX(), theOwner->mPhysicalModel.GetLocation().GetY());
                    if (theEffect->Covers(theOwner->GetPlayer()->GetPlayerID(), theCreated, thePoint))
                        theCreated->SetCloakableChain(true);
                }
            }
        }
        if (theCreated == NULL) {
            theSuccess = false;
        } else {
            if (!theLoaded && inUnit->GetQueuedGoal(theQueue) != NULL) {
                theCreated->ClearGoalQueue(0);
                const EEGoalDeque& theGoals = inUnit->GetQueueGoals(theQueue);
                for (EEGoalDeque::const_iterator i = theGoals.begin(); i != theGoals.end(); ++i) {
                    EEUnitGoal* theGoal;
                    if ((*i)->Suits(theCreated)) {
                        if ((*i)->mKind == kGoalBuildRepair) {
                            theCreated->AddBuildTarget(EEBuildRepairGoalCast(*i)->GetBuildTarget());
                            continue;
                        } else if ((*i)->mKind == kGoalLoadGarrison) {
                            EEUGRust* theLoad = EELoadGarrisonGoalCast(*i);
                            if (theCreated->GetUnitClass()->mCanGarrison) {
                                theGoal = theLoad->Clone();
                                if (theGoal->IsLoadGoal())
                                    theGoal->AddCreatedUnit(theCreated);
                                if (theLoad->GetLoadContainer()->mActiveGoal != NULL)
                                    EELoadGarrisonGoalCast(theLoad->GetLoadContainer()->mActiveGoal)->AddLoadUnit(theCreated);
                            } else {
                                if (theLoad->GetLoadContainer() == NULL)
                                    continue;
                                theLoad->GetLoadContainer()->GetPosition(theGoalPosition);
                                theGoal = EEUGRust::NewLocation(&theGoalPosition, false, true, false, 0.0f, false, 0);
                            }
                        } else {
                            theGoal = (*i)->Clone();
                            if (theGoal->IsLoadGoal())
                                theGoal->AddCreatedUnit(theCreated);
                        }
                    } else {
                        (*i)->GetGoalLocation(theGoalPosition);
                        theGoal = EEUGRust::NewLocation(&theGoalPosition, false, true, false, 0.0f, false, 0);
                    }
                    if (theGoal == NULL)
                        continue;
                    if (theGoal->mKind == kGoalLocation) {
                        theGoal->GetGoalLocation(theGoalPosition);
                        EEGetTileLine(theCreated->GetTileX(), theCreated->GetTileY(), UMFloor(theGoalPosition.GetX()),
                                   UMFloor(theGoalPosition.GetY()), &theLineTiles, 1, 1);
                        long theIndex;
                        for (theIndex = (long)theLineTiles.size() - 1; theIndex >= 0; --theIndex)
                            if (EECanStandOn(theLineTiles[theIndex], theCreated))
                                break;
                        if (theIndex >= 0) {
                            long theRings = UMFloatToLong(theCreated->GetUnitClass()->mOuterCollisionSphere * k447AFC);
                            GE3DPoint theOrigin = inUnit->mPhysicalModel.GetLocation().GetPosition();
    //
    //
                            if (EEFindPlacement(&theOrigin, theCreated, NULL, theLineTiles[theIndex].mX,
                                theLineTiles[theIndex].GetY(), 1, theRings, true, true, true, false, &theAdjusted, NULL))
                                static_cast<EEUGRust*>(theGoal)->SetLocation(theAdjusted.GetPosition());
                        }
                    }
                    if (theGoal->mKind == kGoalAttackUnit && (theCreated->GetUnitClass()->GetCategory() == kUnitCategoryLandCitizen || theCreated->GetUnitClass()->GetCategory() == kUnitCategoryNavalCitizen)) {
                        EEUnit* theTarget = EEAttackUnitCast(theGoal)->GetAttackTarget();
                        if (theCreated->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), false, true)) {
                            EEUnitGoal* theGather = EEUGRust::NewGather(theTarget, 0, true);
                            delete theGoal;
                            theGoal = theGather;
                            theCreated->SetBehavior(EEUnitBehavior::sTable[5]);
                        }
                    }
                    theCreated->QueueGoal(theGoal, 0);
                }
                if (theCreated->mGoalState != NULL) {
                    theCreated->StartAction(theCreated->mGoalState->GetStartAction());
                    theCreated->GetUnitClass()->ActionChanged(theCreated);
                }
            }
            if (thePlayer == gWorld.mPlayers[gWorld.mCurrentPlayer]) {
                EESoundObject* theSound = theCreated->GetStats()->mCreationSound;
                if (theSound != NULL)
                    gSoundManager->Play(theSound->mSampleID, theSound->mVolume, 0.0f, 1.0f, false, NULL);
            }
            if (inArg)
                inUnit->PlayEffect(100000);
        }
    } else {
        theSuccess = false;
    }
    return theSuccess;
}

// Creates a unit of the class for a player at a tile's centre with a heading, grounded when a map is given, and
// queues it in the player's pending units
EEUnit* EEUnitClass::CreateUnitAtTile(EEPlayer* inPlayer, long inX, long inY, EEMap* inMap, float inHeading)
{
    GETransformation theTransformation;
    GE3DPoint thePoint((float)(unsigned long)inX + 0.5f, (float)(unsigned long)inY + 0.5f, 0.0f);
    GE3DPoint theGround;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theZ = 0.0f;
    EEUnit* theUnit;
    if (this->mIsComplex == 0 && this->mUnitType != kUnitTypePathPoint) {
        theUnit = EEUnit::Create(this, inX, inY, inPlayer);
        if (inMap != NULL)
            this->SimpleUnitCreated(theUnit);
    } else {
        theTransformation.SetX(thePoint.GetX());
        theTransformation.SetY(thePoint.GetY());
        theTransformation.SetZ(0.0f);
        if (inMap != NULL) {
            inMap->GetGroundPoint(thePoint, theGround);
            this->GroundLocation(theGround);
            theTransformation.SetX(theGround.GetX());
            theTransformation.SetY(theGround.GetY());
            theTransformation.SetZ(theGround.GetZ());
            if (this->mCategory != kUnitCategoryBuilding && this->mCategory != kUnitCategoryMiningBuilding && this->mCategory != kUnitCategoryGate && this->mCategory != kUnitCategoryFortress) {
                theTransformation.SetOrientationYPR(inHeading, 0.0f, 0.0f);
                if (this->mPitchesWithTerrain)
                    thePitch = inMap->GetTerrainPitch(theTransformation);
                if (this->mRollsWithTerrain)
                    theRoll = inMap->GetTerrainRoll(theTransformation);
                theTransformation.SetOrientationYPR(inHeading, thePitch, theRoll);
            }
        }
        unsigned long theGoalCount = this->GetNumGoalQueues();
        if (inMap != NULL) {
            theZ = theTransformation.GetZ();
            this->LevelGround(&inMap->mTerrainMesh, inX, inY, &theZ);
            theTransformation.SetZ(theZ);
        }
        EEComplexUnit* theComplexUnit = EEComplexUnit::Create(this, theTransformation, inPlayer, theGoalCount);
        theUnit = theComplexUnit;
        if (inMap != NULL)
            this->UnitCreated(theComplexUnit);
    }
    {
        RTSUnit* theEntry = theUnit;
        inPlayer->mPendingUnits.push_back(theEntry);
        theEntry->Reference();
    }
    ++this->mNumUnits;
    theUnit->Dereference();
    return theUnit;
}

// Creates a unit of the class for a player at a map point with a heading, grounded when a map is given, and queues it
// in the player's pending units
EEUnit* EEUnitClass::CreateUnitAtPoint(EEPlayer* inPlayer, float inX, float inY, EEMap* inMap, float inHeading)
{
    GETransformation theTransformation;
    GE3DPoint thePoint(inX, inY, 0.0f);
    GE3DPoint theGround;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theZ = 0.0f;
    unsigned long theX = UMFloorUnsigned(inX);
    unsigned long theY = UMFloorUnsigned(inY);
    EEUnit* theUnit;
    if (this->mIsComplex == 0 && this->mUnitType != kUnitTypePathPoint) {
        theUnit = EEUnit::Create(this, theX, theY, inPlayer);
        if (inMap != NULL)
            this->SimpleUnitCreated(theUnit);
    } else {
        theTransformation.SetX(thePoint.GetX());
        theTransformation.SetY(thePoint.GetY());
        theTransformation.SetZ(thePoint.GetZ());
        if (inMap != NULL) {
            inMap->GetGroundPoint(thePoint, theGround);
            this->GroundLocation(theGround);
            theTransformation.SetX(theGround.GetX());
            theTransformation.SetY(theGround.GetY());
            theTransformation.SetZ(theGround.GetZ());
            if (this->mCategory != kUnitCategoryBuilding && this->mCategory != kUnitCategoryMiningBuilding && this->mCategory != kUnitCategoryGate && this->mCategory != kUnitCategoryFortress) {
                theTransformation.SetOrientationYPR(inHeading, 0.0f, 0.0f);
                if (this->mPitchesWithTerrain)
                    thePitch = inMap->GetTerrainPitch(theTransformation);
                if (this->mRollsWithTerrain)
                    theRoll = inMap->GetTerrainRoll(theTransformation);
                theTransformation.SetOrientationYPR(inHeading, thePitch, theRoll);
            }
        }
        unsigned long theGoalCount = this->GetNumGoalQueues();
        if (inMap != NULL) {
            theZ = theTransformation.GetZ();
            this->LevelGround(&inMap->mTerrainMesh, theX, theY, &theZ);
            theTransformation.SetZ(theZ);
        }
        EEComplexUnit* theComplexUnit = EEComplexUnit::Create(this, theTransformation, inPlayer, theGoalCount);
        theUnit = theComplexUnit;
        if (inMap != NULL)
            this->UnitCreated(theComplexUnit);
    }
    {
        RTSUnit* theEntry = theUnit;
        inPlayer->mPendingUnits.push_back(theEntry);
        theEntry->Reference();
    }
    ++this->mNumUnits;
    theUnit->Dereference();
    return theUnit;
}

// Creates a unit of the class at a transformation for a player and adds it to the world (an airport gets its runways)
EEUnit* EEUnitClass::BaseCreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    float theZ = 0.0f;
    EEComplexUnit* theComplexUnit = NULL;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    long theY;
    long theX;
    EEUnit* theUnit;
    if (!this->IsLandBuilding() && !gFreeCamera)
        thePlayer->CheckButtonsPreloaded(this->mUnitType);
    if (this->mIsComplex != 1 && this->mUnitType != kUnitTypePathPoint) {
        theX = UMFloor(theTransformation.GetX());
        theY = UMFloor(theTransformation.GetY());
        theUnit = EEUnit::Create(this, theX, theY, thePlayer);
        this->SimpleUnitCreated(theUnit);
    } else {
        unsigned long theID = this->GetNumGoalQueues();
        theX = UMFloor(theTransformation.GetX());
        theY = UMFloor(theTransformation.GetY());
        theZ = theTransformation.GetZ();
        this->LevelGround(&::gWorld.mMap->mTerrainMesh, theX, theY, &theZ);
        theTransformation.SetZ(theZ);
        theUnit = theComplexUnit = EEComplexUnit::Create(this, theTransformation, thePlayer, theID);
        this->UnitCreated(theComplexUnit);
        if ((this->mDomains & kMovementAir) || this->mUnitType == kUnitTypeSpaceDock || this->mUnitType == kUnitTypeSpaceTurret || this->mUnitType == kUnitTypeOrbitalSpaceStation)
            theComplexUnit->mPlanetID = EEGetLandContinentOf(theComplexUnit);
        theComplexUnit->FnE123D();
    }
    if (!inIsFoundation)
        thePlayer->LoadUnitClassMedia(this->mUnitType);
    theUnit->LoadMedia(inIsFoundation);
    {
        RTSUnit* theEntry = theUnit;
        thePlayer->mPendingUnits.push_back(theEntry);
        theEntry->Reference();
    }
    ::gWorld.AddUnitEntry(theUnit, inCreator);
    if (this->mUnitType == kUnitTypeAirport)
        this->CreateRunways(theComplexUnit, thePlayer, inIsFoundation);
    if (this->mAttachedEffect != NULL)
        gEffectManager->CreateUnitEffect(theUnit, this->mAttachedEffect, NULL, 1, 0, 0);
    this->mNumUnits++;
    theUnit->Dereference();
    return theUnit;
}

// Creates a unit of the class at a tile for a player (yaw pi) and adds it to the world (an airport gets its runways)
EEUnit* EEUnitClass::BaseCreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    EEMap* theMap = ::gWorld.mMap;
    GE3DPoint thePoint((float)inX, (float)inY, 0.0f);
    GE3DPoint theGround;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theZ = 0.0f;
    EEComplexUnit* theComplexUnit = NULL;
    if (inX != -10000 && inY != -10000) {
        thePoint.SetX(thePoint.GetX() + 0.5f);
        thePoint.SetY(thePoint.GetY() + 0.5f);
    }
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    if (!this->IsLandBuilding() && !gFreeCamera)
        thePlayer->CheckButtonsPreloaded(this->mUnitType);
    EEUnit* theUnit;
    if (this->mIsComplex == 0 && this->mUnitType != kUnitTypePathPoint) {
        theUnit = EEUnit::Create(this, inX, inY, thePlayer);
        this->SimpleUnitCreated(theUnit);
    } else {
        ::gWorld.mMap->GetGroundPoint(thePoint, theGround);
        theTransformation.SetX(theGround.GetX());
        theTransformation.SetY(theGround.GetY());
        theTransformation.SetZ(theGround.GetZ());
        if (this->mCategory != kUnitCategoryBuilding && this->mCategory != kUnitCategoryMiningBuilding && this->mCategory != kUnitCategoryGate && this->mCategory != kUnitCategoryFortress) {
            theTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
            if (this->mPitchesWithTerrain)
                thePitch = theMap->GetTerrainPitch(theTransformation);
            if (this->mRollsWithTerrain)
                theRoll = theMap->GetTerrainRoll(theTransformation);
            theTransformation.SetOrientationYPR(3.1415927f, thePitch, theRoll);
        }
        unsigned long theGoalCount = this->GetNumGoalQueues();
        theZ = theTransformation.GetZ();
        this->LevelGround(&::gWorld.mMap->mTerrainMesh, inX, inY, &theZ);
        theTransformation.SetZ(theZ);
        theUnit = theComplexUnit = EEComplexUnit::Create(this, theTransformation, thePlayer, theGoalCount);
        this->UnitCreated(theComplexUnit);
        if ((this->mDomains & kMovementAir) || this->mUnitType == kUnitTypeSpaceDock || this->mUnitType == kUnitTypeSpaceTurret || this->mUnitType == kUnitTypeOrbitalSpaceStation)
            theComplexUnit->mPlanetID = EEGetLandContinentOf(theComplexUnit);
        theComplexUnit->FnE123D();
        theComplexUnit->mOrientation = GEOrientation(3.1415927f, thePitch, theRoll);
    }
    if (!inIsFoundation)
        thePlayer->LoadUnitClassMedia(this->mUnitType);
    theUnit->LoadMedia(inIsFoundation);
    {
        RTSUnit* theEntry = theUnit;
        thePlayer->mPendingUnits.push_back(theEntry);
        theEntry->Reference();
    }
    ::gWorld.AddUnitEntry(theUnit, inCreator);
    if (this->mUnitType == kUnitTypeAirport)
        this->CreateRunways(theComplexUnit, thePlayer, inIsFoundation);
    if (this->mAttachedEffect != NULL)
        gEffectManager->CreateUnitEffect(theUnit, this->mAttachedEffect, NULL, 0, 0, 0);
    ++this->mNumUnits;
    EEUnitStoreInsertUnlisted(theUnit);   // New (R5b.4, the fourteenth round): a shot's record by object
    theUnit->Dereference();
    return theUnit;
}

// Plants the player's farms on the free plots of the 3x3 grid around the unit, each with one hit point
void EEUnitClass::ReplantFarm(EEComplexUnit* inUnit, EEPlayer* inPlayer)
{
    long theType = EEGetFarmType(inPlayer);
    EEUnitClass* theClass = gUnitClassTable[theType];
    long theCenterX = inUnit->GetTileX();
    long theCenterY = inUnit->GetTileY();
    long theSize = theClass->mTileSize;
    long thePlayerID = inPlayer->GetPlayerID();
    if (!inPlayer->CanProduce(theClass->GetTechnology(), 0x20, 1, false, 3, NULL))
        return;
    long theOffset = 0;
    switch (theSize) {
    case 1: theOffset = 1; break;
    case 2: theOffset = 2; break;
    case 3: theOffset = 3; break;
    case 4: theOffset = 4; break;
    case 5: theOffset = 10; break;
    }
    long theLeft = theCenterX - theOffset;
    long theX = theLeft;
    long theY = theOffset + theCenterY;
    for (long theRow = 0; theRow < 9; theRow += 3) {
        for (long theColumn = 0; theColumn < 3; theColumn++) {
            if ((theX != theCenterX || theY != theCenterY) &&
                EEPlayerBlockTakes(inPlayer, theType, theX, theY, theSize, theSize, 0)) {
                EEUnit* theWall = theClass->CreateUnit(theX, theY, thePlayerID, false, NULL);
                theWall->SetHitPoints(1);
                theWall->MoveTo(theX, theY, true);
                if (theType == 0x33a) {
                    bool theFlag = false;
                    long thePiece = EEGetWallPiecePart(theRow + theColumn, &theFlag);
                    if (theFlag)
                        theWall->GetComplexUnit()->SetAnimation(0xb, -1, 0);
                    GETransformation theTransformation(theWall->GetComplexUnit()->mPhysicalModel.GetLocation());
                    theTransformation.SetOrientationYPR(kWallRotations[thePiece], 0.0f, 0.0f);
                    theWall->GetComplexUnit()->mPhysicalModel.SetLocation(theTransformation);
                }
            }
            theX += theSize;
        }
        theY -= theSize;
        theX = theLeft;
    }
}

// Builds the airport's three runway pieces in its facing direction on levelled ground and links them to it
void EEUnitClass::CreateRunways(EEComplexUnit* inUnit, EEPlayer* inPlayer, bool inIsFoundation)
{
    // Original 0x563a6c is entry 0xf0 of the genuine complete unit-class table.
    long theHeading = inUnit->mFacing;
    long theX = inUnit->GetTileX();
    long theY = inUnit->GetTileY();
    long theSize = this->GetTileSize();
    EEUnitClass* theClass = gUnitClassTable[kUnitTypeAirportRunway];
    long thePlayer = inPlayer->GetPlayerID();
    EEComplexUnit* theFirst;
    EEComplexUnit* theSecond;
    EEComplexUnit* theThird;
    switch (theHeading) {
    case 0:
        EEUnitClass::LevelRunwayPosY(inUnit);
        theY += theSize;
        theFirst = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, inUnit)->mComplexUnit;
        theY += theSize;
        theSecond = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, theFirst)->mComplexUnit;
        theThird = theClass->CreateUnit(theX, theY + theSize, thePlayer, inIsFoundation, theSecond)->mComplexUnit;
        break;
    case 1:
        EEUnitClass::LevelRunwayNegY(inUnit);
        theY -= theSize;
        theFirst = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, inUnit)->mComplexUnit;
        theY -= theSize;
        theSecond = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, theFirst)->mComplexUnit;
        theThird = theClass->CreateUnit(theX, theY - theSize, thePlayer, inIsFoundation, theSecond)->mComplexUnit;
        break;
    case 2:
        EEUnitClass::LevelRunwayPosX(inUnit);
        theX += theSize;
        theFirst = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, inUnit)->mComplexUnit;
        theX += theSize;
        theSecond = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, theFirst)->mComplexUnit;
        theThird = theClass->CreateUnit(theX + theSize, theY, thePlayer, inIsFoundation, theSecond)->mComplexUnit;
        break;
    default:
        EEUnitClass::LevelRunwayNegX(inUnit);
        theX -= theSize;
        theFirst = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, inUnit)->mComplexUnit;
        theX -= theSize;
        theSecond = theClass->CreateUnit(theX, theY, thePlayer, inIsFoundation, theFirst)->mComplexUnit;
        theThird = theClass->CreateUnit(theX - theSize, theY, thePlayer, inIsFoundation, theSecond)->mComplexUnit;
        break;
    }
    theThird->LinkPart(theSecond);
    theSecond->LinkPart(theFirst);
    theFirst->LinkPart(inUnit);
    theFirst->SetFacing(theHeading);
    theSecond->SetFacing(theHeading);
    theThird->SetFacing(theHeading);
    gCameraFollowing = false;
}

// Flattens the ground on the +X side of the airport (four airport lengths) to the airport tile's height
void EEUnitClass::LevelRunwayPosX(EEComplexUnit* inUnit)
{
    long theSize = inUnit->mUnitClass->mTileSize;
    long theY = inUnit->GetTileY();
    EEMap* theMap = ::gWorld.mMap;
    long theLength = theSize * 4;
    long theX = inUnit->GetTileX();
    if (theMap == NULL)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile == NULL)
        return;
    float theHeight = theTile->mHeight;
    long y;
    for (y = theY; y <= theY + theSize; y++)
        for (long x = theX; x <= theX + theLength; x++)
            theMap->mTerrainMesh.SetZ(x, y, theHeight);
    for (y = theY; y < theY + theSize; y++)
        for (long x = theX; x < theX + theLength; x++)
            theMap->GetTile(x, y)->mHeight = theHeight;
}

// Flattens the ground on the +Y side of the airport (four airport lengths) to the airport tile's height
void EEUnitClass::LevelRunwayPosY(EEComplexUnit* inUnit)
{
    long theSize = inUnit->mUnitClass->mTileSize;
    long theX = inUnit->GetTileX();
    EEMap* theMap = ::gWorld.mMap;
    long theY = inUnit->GetTileY();
    if (theMap == NULL)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile == NULL)
        return;
    float theHeight = theTile->mHeight;
    long y;
    for (y = theY; y <= theY + theSize * 4; y++)
        for (long x = theX; x <= theX + theSize; x++)
            theMap->mTerrainMesh.SetZ(x, y, theHeight);
    for (y = theY; y < theY + theSize * 4; y++)
        for (long x = theX; x < theX + theSize; x++)
            theMap->GetTile(x, y)->mHeight = theHeight;
}

// Flattens the ground on the -Y side of the airport (three lengths, to its first row) to the airport's height
void EEUnitClass::LevelRunwayNegY(EEComplexUnit* inUnit)
{
    long theX = inUnit->GetTileX();
    EEMap* theMap = ::gWorld.mMap;
    long theSize = inUnit->mUnitClass->mTileSize;
    long theY = inUnit->GetTileY();
    if (theMap == NULL)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile == NULL)
        return;
    float theHeight = theTile->mHeight;
    long theStart = theY - theSize * 3;
    long y;
    for (y = theStart; y <= theY + 1; y++)
        for (long x = theX; x <= theX + theSize; x++)
            theMap->mTerrainMesh.SetZ(x, y, theHeight);
    for (y = theStart; y < theY + 1; y++)
        for (long x = theX; x < theX + theSize; x++)
            theMap->GetTile(x, y)->mHeight = theHeight;
}

// Flattens the ground on the -X side of the airport (three lengths, to its first column) to the airport's height
void EEUnitClass::LevelRunwayNegX(EEComplexUnit* inUnit)
{
    long theSize = inUnit->mUnitClass->mTileSize;
    long theY = inUnit->GetTileY();
    EEMap* theMap = ::gWorld.mMap;
    long theLength = theSize * 3;
    long theX = inUnit->GetTileX();
    if (theMap == NULL)
        return;
    EEMapTile* theTile = theMap->GetTile(theX, theY);
    if (theTile == NULL)
        return;
    float theHeight = theTile->mHeight;
    long y;
    for (y = theY; y <= theY + theSize; y++)
        for (long x = theX - theLength; x <= theX + 1; x++)
            theMap->mTerrainMesh.SetZ(x, y, theHeight);
    for (y = theY; y < theY + theSize; y++)
        for (long x = theX - theLength; x < theX + 1; x++)
            theMap->GetTile(x, y)->mHeight = theHeight;
}

// Releases every unit class in the class table
void EEUnitClass::ReleaseUnitClasses()
{
    for (unsigned long i = 0; i < EEUnitClassCount(); i++)
        EEUnitClass::ReleaseUnitClass(i);
}

// Returns the unit a projectile hits: its attack target first, then the units of its region in storage order (walls,
// enemies); a hit on a non-target wall makes the launcher fire high
EEUnit* EEUnitClass::FindImpactUnit(EEComplexUnit* inUnit)
{
    EECombatHostReadScope theScope;
    // ee_combat reads.rs impact_unit over the C++ host (EECombatGoals.cpp); CBQR records the outcome
    EEUnit* theFound = EECombatFindImpactUnit(inUnit);
    if (theScope.mOuter) {
        EEUGRust* theGoal = EEAttackUnitCast(inUnit->mGoalState);
        EECombatCaptureHostRead(EE_COMBAT_HR_IMPACT_UNIT, inUnit, theGoal != NULL ? theGoal->GetAttackTarget() : NULL, theFound != NULL,
                                theFound != NULL ? (uint32_t)theFound->mUnitID : 0,
                                inUnit->mOwner != NULL ? (uint32_t)inUnit->mOwner->mIsFiringHigh : 0xffffffffu);
    }
    return theFound;
}

// Returns whether the unit faces the target unit (or its tile's centre)
bool EEUnitClass::FacesTarget(EEComplexUnit* inUnit, EEUnit* inTarget, bool inUseTolerance)
{
    EECombatHostReadScope theScope;   // CBQR records the outcome (the original body in a lambda)
    bool theFaces = [&]() -> bool {
    GE3DPoint theLocation;
    EEComplexUnit* theOwner = inTarget->GetComplexUnit();
    if (theOwner != NULL) {
        theLocation.SetX(theOwner->mPhysicalModel.GetLocation().GetX());
        theLocation.SetY(theOwner->mPhysicalModel.GetLocation().GetY());
    } else {
        theLocation.SetX(inTarget->GetTileX() + 0.5f);
        theLocation.SetY(inTarget->GetTileY() + 0.5f);
    }
    return this->FacesPoint(inUnit, theLocation, inUseTolerance);
    }();
    if (theScope.mOuter)
        EECombatCaptureHostRead(EE_COMBAT_HR_FACES_TARGET, inUnit, inTarget, theFaces, 0, 0, inUseTolerance);
    return theFaces;
}

// Returns whether the unit (or its turrets) faces a point within its firing arc or the fixed tolerance; broadside
// ships also count the opposite side
bool EEUnitClass::FacesPoint(EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance)
{
    EECombatHostReadScope theScope;
    // ee_combat reads.rs faces_point (with TurretsFacePoint) over the plain reads; CBQR records the outcome
    bool theFaces = EECombatFacesPoint(this, inUnit, inLocation, inUseTolerance);
    if (theScope.mOuter) {
        float theX = inLocation.GetX();
        float theY = inLocation.GetY();
        uint32_t theBits[2];
        memcpy(&theBits[0], &theX, 4);
        memcpy(&theBits[1], &theY, 4);
        EECombatCaptureHostRead(EE_COMBAT_HR_FACES_POINT, inUnit, NULL, theFaces, theBits[0], theBits[1], inUseTolerance);
    }
    return theFaces;
}

// Starts a unit's death: spawns the death class, plays the death effect and sound (not for ICBMs), the dying animation,
// and makes it uncommandable
void EEUnitClass::UnitDied(EEComplexUnit* inUnit)
{
    if (this->mDeathClass != NULL && inUnit->mTile != NULL)
        this->mDeathClass->CreateUnit(inUnit->GetTileX(), inUnit->GetTileY(), ::gWorld.mPlayers[0]->GetPlayerID(), 0, NULL);
    if (inUnit->mUnitClass->GetUnitType() != kUnitTypeCpICBM) {
        if (this->mDeathEffect != NULL)
            gEffectManager->CreateUnitEffect(inUnit, this->mDeathEffect, NULL, 0, 0, 0);
        EESoundObject* theSound = inUnit->mStats->mDeathSound;
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, theSound->mLooping, NULL);
        }
    }
    inUnit->SetCurrentPhysicsAction(2);
    if (!inUnit->mUnitClass->mIsWall)
        inUnit->SetAnimation(4, -1, 0);
    inUnit->SetIsCommandable(false);
    inUnit->DropPath();
}

// Sends a computer player's unit to an escape point, when one is found
void EEUnitClass::FindSafeLocation(EEComplexUnit* inUnit)
{
    GE2DPoint theLocation;
    if (inUnit->mPlayer->mComputerPlayer == NULL || !EEGetEscapePoint(inUnit, theLocation))
        return;
    inUnit->DropPath();
    inUnit->SetGoal(EEUGRust::NewLocation(theLocation, false, true, false, 0.0f, false, false), 0, 0);
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// Plays the class's creation effect on the unit
void EEUnitClass::PlayCreationEffect(EEUnit* inUnit)
{
    if (this->mCreationEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, this->mCreationEffect, NULL, 0, 0, 0);
}

// Launches a projectile (or carried cargo) from the unit's weapon tag point toward the target or ground point (new:
// the launch transform, its tile and the special attack's choices are ee_combat's launch_shot)
void EEUnitClass::LaunchShot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation, EEComplexUnit* inCargo)
{
    GE3DPoint thePosition;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EETagPointList* thePoints = inUnit->GetModel()->mTagPoints[inUnit->mAnimation];
    EEUnitClass* theClass = this->mProjectileClass;
    EEUnit* theCreated = NULL;
    // the launch's geometry and choices are ee_combat's (launch.rs): the model data at the frame gathered here
    long theType = inUnit->mUnitClass->GetUnitType();
    EeCombatLaunchIn in = {};
    in.kind = EE_COMBAT_LAUNCH_KIND_SHOT;
    in.special = inUnit->mIsSpecialAttack;
    in.has_special = this->mHasSpecialAttack;
    in.capital = theType == kUnitTypeSp15CapitalShip || theType == kUnitTypeCapitalShipYamato;
    in.legionnaire = theType == kUnitTypeX04RomanLegionnaire;
    in.point_count = thePoints != NULL ? (int32_t)thePoints->GetNumTagPoints() : 0;
    in.weapon_index = (int32_t)inUnit->mWeaponTagPoint;
    in.building = this->mCategory == kUnitCategoryBuilding;
    in.turn = this->mProjectileClass == NULL || this->mProjectileClass->mCategory != kUnitCategoryBomb;
    memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
    in.scale = theStats->GetScale();
    in.target[0] = inLocation.GetX();
    in.target[1] = inLocation.GetY();
    in.target[2] = inLocation.GetZ();
    std::vector<float> theChain;
    int32_t theIndex = ee_combat_launch_index(&in);
    if (theIndex >= 0)
        EECombatTagPointChain(thePoints->GetTagPoint((unsigned long)theIndex), inUnit->mPhysicalModel.GetFrame(), in.point,
                              theChain, in.chain);
    EeCombatLaunchOut theLaunch = ee_combat_launch(&in, theChain.empty() ? NULL : (const float(*)[16])&theChain[0]);
    EECombatChainCapture(EE_COMBAT_TAG('C', 'B', 'L', 'S'), in, theChain, theLaunch);
    GETransformation theTransform(false);
    memcpy(theTransform.mMatrix, theLaunch.launch, sizeof(theLaunch.launch));
    long theX = theLaunch.tile[0];
    long theY = theLaunch.tile[1];
    if (::gWorld.mMap->GetTile(theX, theY) == NULL) {
        thePosition.SetX(theTransform.GetX());
        thePosition.SetY(theTransform.GetY());
        thePosition.SetZ(theTransform.GetZ());
        ::gWorld.mMap->GetClosestTile(&thePosition, &theX, &theY);
    }
    if (inUnit->mIsSpecialAttack) {
        if (theLaunch.flaming) {
            theClass = gUnitClassTable[gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mUnitType];
            theCreated = theClass->CreateUnit(theX, theY, inUnit->GetPlayer()->GetPlayerID(), false, inUnit);
        }
        if (!inUnit->CanAttackClass(inTarget->GetUnitClass()))
            inUnit->StartAction(kActionIdle);
        inUnit->mIsSpecialAttack = false;
    }
    if (inCargo != NULL) {
        theCreated = inCargo;
        theClass = inCargo->GetUnitClass();
        inCargo->MoveTo(theX, theY, true);
    }
    if (theCreated == NULL) {
        theCreated = this->mProjectileClass->CreateUnit(theX, theY, inUnit->GetPlayer()->GetPlayerID(), false, inUnit);
        theClass = this->mProjectileClass;
    }
    if (inUnit->GetUnitClass()->mIsParatrooperBomber)
        EEMovePassengers(theCreated->GetComplexUnit(), inUnit, false);
    theCreated->GetComplexUnit()->mPhysicalModel.SetLocation(theTransform);
    if (this->mCreationEffect != NULL)
        ::gWorld.GetEffectManager()->CreateUnitEffect(theCreated, this->mCreationEffect, NULL, false, 0, 0);
    theCreated->GetComplexUnit()->SetLauncherClass(this);
    theCreated->GetComplexUnit()->SetOwner(inUnit);
    EEUnitGoal* theGoal;
    if (inTarget != NULL) theGoal = EEUGRust::NewAttackUnit(theCreated->GetComplexUnit(), inTarget, false, false);
    else theGoal = EEUGRust::NewAttackGround(inLocation, false);
    theCreated->GetComplexUnit()->SetGoal(theGoal, false, false);
    theCreated->GetComplexUnit()->SetVelocity(inUnit->GetVelocity());
    theCreated->GetComplexUnit()->SetHeight(inUnit->mPhysicalModel.GetLocation().GetZ());
    theClass->LaunchProjectile(theCreated->GetComplexUnit(), inLocation);
    // The original ended with "if the class's category is 0x16e (the Volcano unit type), idle": no category is that
    // large, so it never passed. Testing the unit type instead (8fdb8c62) idled the volcano after every launch, and its
    // idle state (volcano.tai: ChooseRandomVolcanoTarget) fired again at once, skipping AttackLocation's
    // ReadyToFireAgain wait: a boulder every physics tick instead of one per reload (2 s). The dead test is left out:
    // volcano.tai returns to Idle once the reload is done.
}

// Moves the unit away from its pursuer to the first standable spot of eight directions (Gaia animals only 1-2 tiles);
// else idles
void EEUnitClass::Flee(EEComplexUnit* inUnit)
{
    GE2DPoint theDestination;
    EEUnitGoal* theOldGoal = inUnit->mGoalState;
    EEComplexUnit* theEnemy = EEGetPursuerOf(inUnit);
    if (theEnemy != NULL) {
        long theDistance;
        long theDirection;
        if (inUnit->mPlayer->GetPlayerID() != 0) {
            theDistance = UMFloatToLong(inUnit->GetLineOfSight() * kFleeDistanceScale);
            theDirection = EEGetDirectionTo(inUnit, theEnemy);
            theDirection = EETurnAway(theDirection, true);
        } else {
            theDistance = gPhysics->GetRandom(1L, 2L);
            theDirection = EEGetDirectionTo(inUnit, theEnemy);
            theDirection = EETurnAway(theDirection, false);
        }
        long theCount = 0;
        do {
            GE2DPoint theOrigin(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY());
//
            EEMoveInDirection(theOrigin, theDestination, theDirection, theDistance);
            theDirection = (theDirection + 1) % 8;
            ++theCount;
            if (theCount > 7)
                break;
            EETileCoordinate theTile(UMFloor(theDestination.GetX()), UMFloor(theDestination.GetY()));
            if (EECanStandOn(theTile, inUnit))
                break;
        } while (true);
        if (theCount <= 7) {
            EEUnitGoal* theGoal = EEUGRust::NewLocation(theDestination, true, true, false, 0.0f, false, 0);
            inUnit->SetGoal(theGoal, theOldGoal != NULL && theOldGoal->mKind == kGoalExplore, 0);
            inUnit->StartAction(kActionPrepareToMove);
            this->ActionChanged(inUnit);
            return;
        }
    }
    inUnit->StartAction(kActionIdle);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// Plays the class's ripple effect at the model's ripple tag points that are under water, when the player sees the unit
void EEUnitClass::CreateRipples(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    GETransformation theEffect;
    GETransformation theModel(false);
    EETagPointList* theTags = inUnit->GetModel()->GetTagPoints(inUnit->GetAnimation(), 0x22);
    if (theTags == NULL || theTags->mPoints.size() == 0 ||
        !EEIsSeenBy(inUnit, ::gWorld.GetCurrentPlayer(), true))
        return;
    theModel = inUnit->mPhysicalModel.GetLocation();
    theTags->Scale(theModel, inUnit->mPhysicalModel.GetScale());
    for (unsigned long i = 0; i < theTags->mPoints.size(); i++) {
        unsigned long theFrame = inUnit->mPhysicalModel.GetFrame();
        theTags->GetTagPoint(i)->GetPoint(theFrame, theModel, theLocation);
        long theY = UMFloor(theLocation.GetY());
        long theX = UMFloor(theLocation.GetX());
        EEMapTile* theTile = ::gWorld.mMap->GetTile(theX, theY);
        if (theTile == NULL || !(theTile->mHeight < 0.0f) || !(theLocation.GetZ() < 0.0f))
            continue;
        theEffect.SetOrientationYPR(UMArcTan2(theModel.GetElement(1, 0), theModel.GetElement(0, 0)), 0.0f, 0.0f);
        theEffect.SetPosition(theLocation);
        gEffectManager->CreateTransformationEffect(theEffect, this->mRippleEffect, NULL);
    }
}

// Lists the ring of tiles around the unit's footprint (optionally with corners, appending or replacing)
void EEUnitClass::BaseGetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    if (!inAppend && outTiles.size() > 0)
        outTiles.clear();
    EEGetSquareBlockRing(inCorners, inUnit->GetTileX(), inUnit->GetTileY(), this->GetTileSize(), outTiles, inAppend);
}

// Returns the unit's passable terrain as bits: cliff 1, water 2, land 4, space 8
long EEUnitClass::GetPassableFlags(EEUnit* inUnit)
{
    long theResult = 0;
    if (this->CanMoveOnTerrain(inUnit, 0))
        theResult = 4;
    if (this->CanMoveOnTerrain(inUnit, 9))
        theResult |= 1;
    if (this->CanMoveOnTerrain(inUnit, 1))
        theResult |= 2;
    if (this->CanMoveOnTerrain(inUnit, 0xc))
        theResult |= 8;
    return theResult;
}

// Finds a unit within range (default: line of sight) that the unit can heal and reach; returns whether one was found
bool EEUnitClass::FindTargetNear(EEComplexUnit* inUnit, float inRange, EEUnit** outTarget)
{
    std::vector<EEUnit*> theUnits;
    bool theResult = false;
    GE3DPoint thePosition;
    float theRange = inRange == 0.0f ? inUnit->GetLineOfSight() : inRange;
    inUnit->GetPosition(thePosition);
    *outTarget = NULL;
    if ((unsigned long)EEGetUnitsNear(thePosition, theRange, theUnits) > 0) {
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = theUnits[i];
            if (!EECanHeal(inUnit, static_cast<EEComplexUnit*>(theUnit)) || !inUnit->CanReachUnit(theUnit))
                continue;
            *outTarget = theUnit;
            theResult = true;
            break;
        }
    }
    return theResult;
}

// Returns whether the unit (by its stats) or the class may move on a terrain type
bool EEUnitClass::CanMoveOnTerrain(EEUnit* inUnit, long inTerrainType)
{
    bool theResult;
    if (inUnit != NULL)
        theResult = (inUnit->mStats->mTerrainMask & (1 << inTerrainType)) != 0;
    else
        theResult = this->mTerrainTypes[inTerrainType];
    return theResult;
}

// Returns a land continent beside the unit (the target's, when given); with no match, the last continent examined
EEAIContinent* EEUnitClass::GetContainerContinent(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EETileCoordinate> theTiles;
    bool theFound = false;
    EEAIContinent* theContinent = NULL;
    EEAIContinent* theTargetContinent = NULL;
    if (inTarget != NULL)
        theTargetContinent = gContinentManager.GetContinentAt(inTarget);
    this->GetSurroundingTiles(inUnit, theTiles, 1, false);
    if (theTiles.size() > 0) {
        for (long i = theTiles.size() - 1; i >= 0 && !theFound; i--) {
            theContinent = gContinentManager.GetContinentAt(theTiles[i].GetX(), theTiles[i].GetY());
            EETile* theTile = gWorld.mMap->GetTileAtLocation(theTiles[i]);
            if (theContinent != NULL && theTile != NULL && theContinent->GetCategory() == kContinentCategoryLand && theTile->mTerrainType != kTerrainCliff &&
                (theTargetContinent == NULL || theContinent == theTargetContinent))
                theFound = true;
            else
                theFound = false;
        }
    }
    return theContinent;
}

// Advances the unit to its next waypoint, first marking a land unit blocked when the next tile (or both corners of a
// diagonal step) holds an impassable unit
void EEUnitClass::NextWaypoint(EEComplexUnit* inUnit)
{
    bool theFirstCorner = false;
    bool theSecondCorner = false;
    GE3DPoint theNextPosition;
    EEUnit* theBlocker = NULL;
    bool theBlocked = false;
    if ((this->mDomains & kMovementLand) != 0 && inUnit->GetSecondWaypoint(theNextPosition) == 0) {
        long theY = UMFloor(theNextPosition.GetY());
        long theX = UMFloor(theNextPosition.GetX());
        EEMapTile* theTile = ::gWorld.mMap->GetTile(theX, theY);
        if (theTile != NULL) {
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theBlocked; theNode = theNode->mNext) {
                theBlocker = theNode->mUnit;
                if (theBlocker != NULL && !theBlocker->IsPassableBy(inUnit))
                    theBlocked = true;
            }
        }
        if (!theBlocked) {
            long theDX = UMFloor(inUnit->mPhysicalModel.GetLocation().GetX()) - UMFloor(theNextPosition.GetX());
            if (theDX < 0)
                theDX = -theDX;
            if (theDX == 1) {
                long theDY = UMFloor(inUnit->mPhysicalModel.GetLocation().GetY()) - UMFloor(theNextPosition.GetY());
                if (theDY < 0)
                    theDY = -theDY;
                if (theDY == 1) {
                    theY = UMFloor(theNextPosition.GetY());
                    theX = UMFloor(inUnit->mPhysicalModel.GetLocation().GetX());
                    theTile = ::gWorld.mMap->GetTile(theX, theY);
                    if (theTile != NULL) {
                        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFirstCorner; theNode = theNode->mNext) {
                            theBlocker = theNode->mUnit;
                            if (theBlocker != NULL && !theBlocker->IsPassableBy(inUnit))
                                theFirstCorner = true;
                        }
                    } else
                        theFirstCorner = true;
                    if (theFirstCorner) {
                        theY = UMFloor(inUnit->mPhysicalModel.GetLocation().GetY());
                        theX = UMFloor(theNextPosition.GetX());
                        theTile = ::gWorld.mMap->GetTile(theX, theY);
                        if (theTile != NULL) {
                            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theSecondCorner; theNode = theNode->mNext) {
                                theBlocker = theNode->mUnit;
                                if (theBlocker != NULL && !theBlocker->IsPassableBy(inUnit))
                                    theSecondCorner = true;
                            }
                        } else
                            theSecondCorner = true;
                    }
                }
            }
        }
        if (theBlocked || theFirstCorner && theSecondCorner) {
            inUnit->SetBlockingUnit(theBlocker);
            if (inUnit->mCollisionState != 2)
                inUnit->mCollisionState = 2;
        }
    }
    inUnit->NextWaypoint();
}

// Returns the number of goal queues: airport 5, navy yard 3, buildings, airfields, airplanes and spaceships 2, else 1
long EEUnitClass::GetNumGoalQueues()
{
    if (this->mUnitType == kUnitTypeAirport)
        return 5;
    if (this->mUnitType == kUnitTypeNavyYard)
        return 3;
    if (((this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySpaceBuilding) && this->mAirTrafficType != 0) || this->mCategory == kUnitCategoryBuilding || this->mCategory == kUnitCategoryAirplane ||
        this->mCategory == kUnitCategoryPlanetSpaceship)
        return 2;
    return 1;
}

// Lists every unit class whose unit type is in a unit set
void EEUnitClass::GetClassesInSet(unsigned long inSet, std::vector<EEUnitClass*>& outClasses)
{
    outClasses.clear();
    for (long i = 0; i < EEUnitClassCount(); i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass != NULL && gUnitSetManager.Contains(theClass->GetUnitType(), inSet) == true)
            outClasses.push_back(theClass);
    }
}

// Returns whether a building of another unit type shares this one's rally point (military, town centre, dock)
bool EEUnitClass::SharesRallyPoint(unsigned long inUnitType)
{
    bool theResult = false;
    switch (this->mUnitType) {
    case kUnitTypeBarracks:
    case kUnitTypeArcheryRange:
    case kUnitTypeSiegeFactory:
    case kUnitTypeStable:
    case kUnitTypeTankFactory:
    case kUnitTypeCyberFactory:
    case kUnitTypeCyberLaboratory:
        switch (inUnitType) {
        case kUnitTypeBarracks:
        case kUnitTypeArcheryRange:
        case kUnitTypeSiegeFactory:
        case kUnitTypeStable:
        case kUnitTypeTankFactory:
        case kUnitTypeCyberFactory:
        case kUnitTypeCyberLaboratory:
            theResult = true;
            break;
        }
        break;
    case kUnitTypeTownCenter:
    case kUnitTypeCapitol:
        if (inUnitType == kUnitTypeTownCenter || inUnitType == kUnitTypeCapitol)
            theResult = true;
        break;
    case kUnitTypeDock:
    case kUnitTypeNavyYard:
        if (inUnitType == kUnitTypeDock || inUnitType == kUnitTypeNavyYard)
            theResult = true;
        break;
    }
    return theResult;
}

// Returns whether another unit type shares this one's research (town centre and capitol, male and female citizens)
bool EEUnitClass::SharesResearch(long inUnitType)
{
    bool theResult = false;
    switch (this->mUnitType) {
    case kUnitTypeTownCenter:
    case kUnitTypeCapitol:
        if (inUnitType == kUnitTypeTownCenter || inUnitType == kUnitTypeCapitol)
            theResult = true;
        break;
    case kUnitTypeCitizen:
    case kUnitTypeFemaleCitizen:
        if (inUnitType == kUnitTypeCitizen || inUnitType == kUnitTypeFemaleCitizen)
            theResult = true;
        break;
    }
    return theResult;
}

// IsImportantUnit reads the heal goal (included here, after the unit's init chains: its class static would renumber them)

// Heals the heal goal's target when in range: plays the healing animation and the class's heal effect
// The heal effect is drawn between the two units or on the target, by the graphic effect's kind
void EEUnitClass::Heal(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    theHandle.mValue = 0;
    EEUGRust* theGoal = EEHealGoalCast(inUnit->GetGoal());   // (the original cast any goal)
    inUnit->DropPath();
    if (theGoal == NULL || !EECombatHealInRange(inUnit, theGoal->GetHealTarget()))
        return;
    inUnit->SetAnimation(3, -1, 0);
    EEGraphicEffect* theEffect = inUnit->mUnitClass->mHealEffect;
    if (theEffect == NULL)
        return;
    switch (static_cast<unsigned long>(theEffect->mKind)) {
    case 0x14:
    case 0x16:
    case 0x21:
    case 0x26:
    case 0x40:
        if (::gWorld.GetEffectManager()->CreateEffectBetweenUnits(inUnit, theGoal->GetHealTarget(), theEffect, &theHandle) != 0)
            return;
        break;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 0xa:
    case 0xd:
    case 0xe:
    case 0xf:
    case 0x10:
    case 0x12:
    case 0x1e:
    case 0x23:
    case 0x24:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2b:
    case 0x2c:
    case 0x3a:
    case 0x3b:
        if (::gWorld.GetEffectManager()->CreateUnitEffect(theGoal->GetHealTarget(), theEffect, &theHandle, false, 0, 0) != 0)
            return;
        break;
    }
    if (inUnit->mHealEffect != theHandle)
        inUnit->mHealEffect = theHandle;
}

// Sets up a new unit: full hit points, ammunition, behaviour, heal timer, standing still and idle
void EEUnitClass::BaseUnitCreated(EEComplexUnit* inUnit)
{
    inUnit->SetHealSearchTimer(this->mHealDelay);
    inUnit->SetHitPoints(inUnit->mStats->mMaxHitPoints);
    inUnit->SetLastHitPoints(inUnit->mStats->mMaxHitPoints);
    inUnit->SetVelocity(0.0f);
    inUnit->SetAmmunition(this->mAmmunition);
    inUnit->SetBehavior(this->mBehavior);
    inUnit->StartAction(kActionIdle);
    this->ActionChanged(inUnit);
}

// Gives a new simple unit its full hit points
void EEUnitClass::BaseSimpleUnitCreated(EEUnit* inUnit)
{
    inUnit->SetHitPoints(inUnit->mStats->mMaxHitPoints);
}

// Adds a placed unit (not a path marker) to the map's unit region of its tile
void EEUnitClass::BaseAddToRegion(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    U2DPoint<long> theRegion(0, 0);
    EeCombatUnitRegions* theTable = theMap->mUnitRegions;
    if (inUnit->mTileNode == NULL || inUnit->IsPathMarker())
        return;
    long theSize = ee_combat_regions_size(theTable);
    theRegion = U2DPoint<long>(inUnit->GetTileX() / theSize, inUnit->GetTileY() / theSize);
    ee_combat_regions_add(theTable, (uint32_t)theRegion.mX, (uint32_t)theRegion.mY, (uint64_t)(uintptr_t)inUnit->GetComplexUnit());
}

// Adds the unit to its tile's unit list
void EEUnitClass::BaseAddToTile(EEUnit* inUnit)
{
    RTSTile* theTile = inUnit->mTile;
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
}

// Adds waypoints along the line to a point at turning-radius steps and cruise altitude (a near point: one waypoint)
void EEUnitClass::PathToward(EEComplexUnit* inUnit, GE3DPoint& inFrom, GE3DPoint& inTo, bool inSnapToMap)
{
    GE3DPoint theFrom(inFrom.GetX(), inFrom.GetY(), inFrom.GetZ());
    GE3DPoint theTo(inTo.GetX(), inTo.GetY(), inTo.GetZ());
    GEVector theDirection;
    std::vector<GE3DPoint> thePoints;
    float theStep = inUnit->GetMaxVelocity() * 2.0f / inUnit->GetMaxAngularVelocity();
    float theHeight = inUnit->mUnitClass->GetCruiseAltitude();
    theDirection.SetX(theTo.GetX() - theFrom.GetX());
    theDirection.SetY(theTo.GetY() - theFrom.GetY());
    theDirection.SetZ(0.0f);
    if (theDirection.GetMagnitudeSquared() > k447AF0 * k447AF0) {
        inFrom.InterpolateTo(inTo, theStep, thePoints);
        unsigned long theCount = thePoints.size();
        for (unsigned long i = 1; i < theCount; i++)
            this->AddWaypointAt(inUnit, (thePoints[i]).GetX(),
                     (thePoints[i]).GetY(), theHeight, inSnapToMap);
    } else
        this->AddWaypointAt(inUnit, inTo.GetX(), inTo.GetY(), inTo.GetZ(), inSnapToMap);
}

// Returns whether the unit has reached its current waypoint, given the distance left
bool EEUnitClass::BaseHasArrived(EEComplexUnit* inUnit, float inValue)
{
    GE3DPoint theLocation;
    bool theResult = false;
    if (inUnit->mCollisionState == 8)
        theResult = true;
    else {
        unsigned long theCount = inUnit->mWaypoints.size();
        inUnit->GetNextWaypointPoint(theLocation);
        theLocation.SetZ(0.0f);
        if (theCount > 1 || (inUnit->mGoalState != NULL &&
                             ((inUnit->GetQueueGoals(0).size() > 1 && inUnit->mGoalState->mKind == kGoalLocation) ||
                              inUnit->mGoalState->mKind == kGoalExplore))) {
            if (inValue <= this->mInnerCollisionSphere) {
                if (inUnit->GetTileX() == UMFloor(theLocation.GetX()) && inUnit->GetTileY() == UMFloor(theLocation.GetY()))
                    theResult = true;
                else if (inValue < 0.05f)
                    theResult = true;
                else
                    theResult = false;
            } else
                theResult = false;
        } else if (theCount == 1) {
            if (!(inValue < 0.05f))
                theResult = false;
            else
                theResult = true;
        }
    }
    return theResult;
}

// Loads the class's sounds and model and acquires its button
void EEUnitClass::LoadClassMedia()
{
    long i;
    for (i = 0; i < 2; i++)
        if (this->mAcknowledgeSounds[i] != NULL)
            this->mAcknowledgeSounds[i]->LoadMedia();
    for (i = 0; i < 2; i++)
        if (this->mSelectionSounds[i] != NULL)
            this->mSelectionSounds[i]->LoadMedia();
    if (this->mDeathSound != NULL)
        this->mDeathSound->LoadMedia();
    if (this->mCreationSound != NULL)
        this->mCreationSound->LoadMedia();
    if (this->mModel != NULL)
        this->mModel->LoadMedia();
    if (this->mButton != NULL)
        this->mButton->Acquire();
}

// Loads the buttons of the class's research and commands for a human player, and its calamities' media
void EEUnitClass::LoadButtons(EEPlayer* inPlayer)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    if (theTree != NULL) {
        for (long i = 0; i < (long)this->mResearchTechnologies.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(this->mResearchTechnologies[i]);
            if (theNode == NULL)
                continue;
            if (inPlayer->IsHuman() == true)
                theNode->mButton->Acquire();
            RTSTechTreeNodeData* theData = theNode->mData;
            if (theData->mKind == 5) {
                EECalamityEnum theEnum = (EECalamityEnum)theData->mCalamity;
                USingleton<EECalamityManager>::GetInstance()->LoadMedia(inPlayer, theEnum);
            }
        }
    }
    for (long j = 0; j < 9; j++)
        if (this->mCommands[j] >= 2 && this->mCommands[j] < 0x32 && inPlayer->IsHuman() == true)
            EEButtonObject::Get(this->mCommands[j])->Acquire();
}

// Loads the class's graphic effects and marks its related classes (projectile, death, farms, runways ...) for loading
void EEUnitClass::BaseLoadMedia(EEPlayer* inPlayer)
{
    if (this->mUnitType == kUnitTypeAirport) {
        inPlayer->SetUnitClassMedia(0xf0, 0x7f);
        EEGraphicEffect::Get(kGraphicEffectRallyFlagFighter)->Load(false);
        EEGraphicEffect::Get(kGraphicEffectRallyFlagBomber)->Load(false);
        EEGraphicEffect::Get(kGraphicEffectRallyFlagAtomicBomber)->Load(false);
        EEGraphicEffect::Get(kGraphicEffectRallyFlagParatrooper)->Load(false);
    }
    if (this->mUnitType == kUnitTypeGranaryImproved || this->mUnitType == kUnitTypeGranary)
        inPlayer->SetUnitClassMedia(0x2a, 0x7f);
    if (this->mProjectileClass != NULL)
        inPlayer->SetUnitClassMedia(this->mProjectileClass->mUnitType, 0x7f);
    if (this->mScaffoldingClass != NULL)
        inPlayer->SetUnitClassMedia(this->mScaffoldingClass->mUnitType, 0x7f);
    if (this->mDeathClass != NULL)
        inPlayer->SetUnitClassMedia(this->mDeathClass->mUnitType, 0x7f);
    if (this->mDeathSpawnClass != NULL)
        inPlayer->SetUnitClassMedia(this->mDeathSpawnClass->mUnitType, 0x7f);
    if (this->mRemainsClass != NULL)
        inPlayer->SetUnitClassMedia(this->mRemainsClass->mUnitType, 0x7f);
    for (long i = 0; i < kUnitClassEffectCount; i++)   // the 29 effects, in their order (+0x64..+0xd4)
        if (this->GetEffects()[i] != NULL)
            this->GetEffects()[i]->Load(false);
    this->mIsMediaLoaded = true;
}

// The order sUnitTypes is sorted in: the unit names (gResource strings of the classes' +0x260); its operator() is
// emitted out of line among the std::sort COMDATs (0x63a19c) (provisional name)
struct EEUnitClassNameLess {
    bool operator()(long inType1, long inType2) const
    {
        UWideString theName1;
        UWideString theName2;
        gResource->GetString(gUnitClassTable[inType1]->mNameStringID, theName1);
        gResource->GetString(gUnitClassTable[inType2]->mNameStringID, theName2);
        return theName1 < theName2;
    }
};

// Each category's physics model (an index into gPhysicsModels; -1: an ambient or resource class, which is not complex
// and has none): the EEUC* constructors' arguments (the land 0, airplane 1, helicopter 2, naval 4, submerged naval 5,
// projectile 6, missile 7, building 8, sea animal 9, air animal 10 and space 11 models)
static const signed char kCategoryPhysicsModels[0x23] = {
    -1, -1, 8, 1, 4, 0, 0, 2, 0, 6, 5, 6, 7, 0, 4, 0,     // ambient .. land 15
    0, 2, 4, 8, 7, 4, 4, 0, 0, 8, 2, 9, 10, 8, 8, 11,     // land transport .. spaceship
    11, 11, 1};                                         // space building, space transport, planet spaceship

// Builds every unit class from dbobjects by category, links the related classes and set flags, sorts sUnitTypes by
// name (the class table has a slot per unit type, sized from the records)
// The cases are in the order the original lays their bodies out
void EEUnitClass::CreateUnitClasses()
{
    FSFileSpec* theDbFile = NULL;
    long theResult = EEDbFindFile(gDataPath, kDbObjectsFile, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 5710);
    ESVERIFY_AT(theDbFile != NULL, 5711);
    theResult = gDatabaseObjects.Load(theDbFile);
    ESVERIFY_AT(theResult == keseOK, 5713);
    if (theDbFile != NULL)
        delete theDbFile;
    // one class slot per unit type: as many as dbobjects has records, or the highest unit type + 1
    unsigned long theSlots = gDatabaseObjects.GetCount();
    for (unsigned long r = 0; r < gDatabaseObjects.GetCount(); r++) {
        EEUnitClassRecord* theRecord = (EEUnitClassRecord*)gDatabaseObjects.GetEntry(r)->mRecord;
        if (theRecord->mIsEnabled && theRecord->mUnitType >= 0 && (unsigned long)theRecord->mUnitType >= theSlots)
            theSlots = (unsigned long)theRecord->mUnitType + 1;
    }
    gUnitClassTable.assign(theSlots, NULL);
    unsigned long theCounts[0x23];
    for (unsigned long theCategory = 0; theCategory < 0x23; theCategory++)
        theCounts[theCategory] = 0;
    unsigned long theCount = gDatabaseObjects.GetCount();
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        EEUnitClassRecord* theRecord = (EEUnitClassRecord*)gDatabaseObjects.GetEntry(i)->mRecord;
        if (!theRecord->mIsEnabled || theRecord->mUnitType == -1)
            continue;
        // one EEUnitClass per record, of its category's kind (the EEUC* subclass the factory made until R5b.4 round
        // 32), with that kind's physics model (its constructors' argument)
        EEUnitClass* theClass = NULL;
        unsigned long theCategory = theRecord->mCategory;
        if (theCategory < 0x23 && EE_UNIT_CLASS_KINDS[theCategory] != 0) {
            long theModel = kCategoryPhysicsModels[theCategory];
            theClass = new EEUnitClass(theModel >= 0, theRecord, theModel >= 0 ? gPhysicsModels[theModel] : NULL);
            theCounts[theCategory]++;
        }
        EEUnitClass::RegisterUnitClass(theClass);
        if (theClass != NULL)
            theClass->Dereference();
    }
    // links each class to its related classes, from its own record (the original paired class table slot i with
    // record i, right only while every unit type equals its record's row)
    for (i = 0; i < theCount; i++) {
        EEDbObject* theObject = gDatabaseObjects.GetEntry(i);
        EEUnitClassRecord* theRecord = (EEUnitClassRecord*)theObject->mRecord;
        if (!theRecord->mIsEnabled || theRecord->mUnitType < 0)
            continue;
        EEUnitClass* theClass = gUnitClassTable[theRecord->mUnitType];
        if (theClass == NULL)
            continue;
        if (theRecord->mProjectileType > 0) {
            theClass->mProjectileClass = gUnitClassTable[theRecord->mProjectileType];
            theClass->mProjectileClass->Reference();
        }
        if (theRecord->mScaffoldingType > 0) {
            theClass->mScaffoldingClass = gUnitClassTable[theRecord->mScaffoldingType];
            theClass->mScaffoldingClass->Reference();
        }
        if (theRecord->mDeathType > 0) {
            theClass->mDeathClass = gUnitClassTable[theRecord->mDeathType];
            theClass->mDeathClass->Reference();
        }
        if (theRecord->mDeathSpawnType > 0) {
            theClass->mDeathSpawnClass = gUnitClassTable[theRecord->mDeathSpawnType];
            theClass->mDeathSpawnClass->Reference();
        }
        if (theRecord->mPackedType > 0) {
            theClass->mPackedClass = gUnitClassTable[theRecord->mPackedType];
            theClass->mPackedClass->Reference();
        }
        if (theRecord->mRemainsType > 0) {
            theClass->mRemainsClass = gUnitClassTable[theRecord->mRemainsType];
            theClass->mRemainsClass->Reference();
        }
        theClass->mIsWall = gUnitSetManager.Contains(theClass->mUnitType, 9);
        theClass->mIsCyber = gUnitSetManager.Contains(theClass->mUnitType, 0x38);
        theClass->mIsHorse = gUnitSetManager.Contains(theClass->mUnitType, 0x6e);
        theClass->mIsHuman = gUnitSetManager.Contains(theClass->mUnitType, 0x71);
        theClass->mIsMedic = gUnitSetManager.Contains(theClass->mUnitType, 0x81);
        theClass->mIsFlare = gUnitSetManager.Contains(theClass->mUnitType, 0x8a);
        theClass->mIsBoobyTrap = gUnitSetManager.Contains(theClass->mUnitType, 0x97);
        theClass->mIsParatrooperBomber = gUnitSetManager.Contains(theClass->mUnitType, 0x96);
        theClass->mSkipsLineOfFireCheck = gUnitSetManager.Contains(theClass->mUnitType, 0x86);
        theClass->mIsLandBuildingClass = theClass->IsLandBuilding();   // port: the unit store reads it (EEUnitStore.h)
    }
    unsigned long theTotal = 0;
    for (unsigned long theKind = 0; theKind < 0x23; theKind++)
        theTotal += theCounts[theKind];
    gDatabaseObjects.Clear();
    std::sort(EEUnitClass::sUnitTypes.begin(), EEUnitClass::sUnitTypes.end(), EEUnitClassNameLess());
}

// Returns the class's AI script action for a key, or NULL when the class has no script or no such action
EEUnitAction* EEUnitClass::FindAction(long inKey)
{
    // port (R5b.4 round 20): the script is Rust's (ee_units script.rs)
    return reinterpret_cast<EEUnitAction*>(static_cast<uintptr_t>(
        ee_units_script_find(reinterpret_cast<uintptr_t>(this->mAIScript), (int32_t)inKey)));
}

// The unit's turrets for one tick through ee_units turret.rs (TurnTurrets' aim, TurnTurretsBack's turn back): the
// parts' positions, transformations and angles in, each angle stored back where it differs
static void EETurnTurretsRust(EEComplexUnit* inUnit, bool inAim, float inX, float inY)
{
    EeUnitsTurretsIn theIn;
    theIn.aim = inAim;
    theIn.alive = inUnit->mHitPoints > 0;
    theIn.turn_rate = inUnit->mStats->mTurretMaxAngularVelocity;
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.scale = inUnit->mStats->GetScale();
    memcpy(theIn.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(theIn.location));
    theIn.target_x = inX;
    theIn.target_y = inY;
    EEModel* theModel = inUnit->GetModel();
    unsigned long theCount = theModel->mTurretParts.size();
    if (theCount == 0 || (!inAim && !theIn.alive))
        return;
    EeUnitsTurretPart theLocal[8];
    std::vector<EeUnitsTurretPart> theMore;
    EeUnitsTurretPart* theParts = theLocal;
    if (theCount > 8) {
        theMore.resize(theCount);
        theParts = theMore.data();
    }
    for (unsigned long i = 0; i < theCount; ++i) {
        const EETurretPart* thePart = theModel->mTurretParts[i];
        theParts[i].position[0] = thePart->mPosition.GetX();
        theParts[i].position[1] = thePart->mPosition.GetY();
        theParts[i].position[2] = thePart->mPosition.GetZ();
        memcpy(theParts[i].transform, thePart->mTransformation.mMatrix, sizeof(theParts[i].transform));
        theParts[i].angle = inUnit->mTurretAngles[i];
    }
    ee_units_turn_turrets(&theIn, theParts, (uint32_t)theCount);
    for (unsigned long i = 0; i < theCount; ++i)
        if (inUnit->mTurretAngles[i] != theParts[i].angle)
            inUnit->mTurretAngles[i] = theParts[i].angle;
}

// Turns each turret of a living unit back toward straight ahead by at most its turn rate for this tick
// port (R5b.4): the turn is Rust (ee_units turret.rs), as TurnTurrets'
void EEUnitClass::TurnTurretsBack(EEComplexUnit* inUnit)
{
    EETurnTurretsRust(inUnit, false, 0.0f, 0.0f);
}

// Sends a goalless unit to the centre of its tile; with a goal, faces the unit to the centre and places it there
void EEUnitClass::MoveToTileCentre(EEComplexUnit* inUnit)
{
    GETransformation theTransformation;
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    if (inUnit->mGoalState == NULL) {
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->DropPath();
        theLocation.SetX(inUnit->mTileX + 0.5f);
        theLocation.SetY(inUnit->GetTileY() + 0.5f);
        theLocation.SetZ(inUnit->mUnitClass->mCanFly == true ? inUnit->mHeight : inUnit->mTile->mHeight);
        inUnit->SetGoal(EEUGRust::NewLocation(&theLocation, false, true, false, 0.0f, false, false), 0, 0);
    } else {
        GE3DPoint theTarget(inUnit->mTileX + 0.5f, inUnit->GetTileY() + 0.5f,
                            inUnit->mPhysicalModel.GetLocation().GetZ());
        const GETransformation& theCurrent = inUnit->mPhysicalModel.GetLocation();
        theLocation.SetX(theCurrent.GetX());
        theLocation.SetY(theCurrent.GetY());
        theLocation.SetZ(theCurrent.GetZ());
        float theDX = theTarget.GetX() - theLocation.GetX();
        float theHeading = UMArcTan2(-theDX, theTarget.GetY() - theLocation.GetY());
        theTransformation.SetPosition(theTarget);
        theTransformation.SetOrientationYPR(theHeading, 0.0f, 0.0f);
        inUnit->mOrientation = GEOrientation(theHeading, 0.0f, 0.0f);
        float theDepth = 1.0f - gWorldTable.GetFloatValue(kWorldTileStrayDistance);
        theTarget.SetX(0.0f);
        theTarget.SetY(theDepth);
        theTarget.SetZ(0.0f);
        theTransformation.Apply(theTarget, theLocation);
        theTransformation.SetX(theLocation.GetX());
        theTransformation.SetY(theLocation.GetY());
        theTransformation.SetZ(theLocation.GetZ());
        long theX;
        long theY;
        EEUpdateUnitTile(&theTransformation, inUnit, &theX, &theY);
    }
}

#include "ee/EEPathManager.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
// Requests a path from the unit's tile to its goal (cancelling its paths when it already stands there), setting
// its start heading when unset and an enemy-avoidance modifier for players that avoid enemies; idles when pathless
// A location goal's group path state and auto-waypoint flag pick the revisions and the graph; a unit without
// waypoints afterwards goes to physics action 4
void EEUnitClass::PathToGoal(EEComplexUnit* inUnit)
{
    EEAIGPModifierEnemyAvoidance* theModifier = NULL;
    bool theArg8 = false;
    GE3DPoint theGoalPoint;
    EEUnitGoal* theGoal = inUnit->mGoalState;
    bool theUseGraph;
    inUnit->DropPath();
    theGoal->GetDestination(inUnit, theGoalPoint);
    if (this->mDomains & kMovementSeaOrSpace)
        inUnit->UpdateWaterState(true);
    if (inUnit->mStats->mMaxVelocity > 0.0f) {
        GE3DPoint thePosition;
        GEVector theForward(0.0f, 1.0f, 0.0f);
        GEVector theDirection;
        const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
        if (theLocation.GetX() == theGoalPoint.mX && theLocation.GetY() == theGoalPoint.GetY() &&
            theLocation.GetZ() == theGoalPoint.GetZ()) {
            gPathManager.CancelPaths(inUnit, true);
        } else {
            thePosition.mX = theLocation.GetX();
            thePosition.mY = theLocation.GetY();
            thePosition.mZ = 0.0f;
            if (inUnit->mInitialContactHeading == kHeadingUnset) {
                theLocation.Apply(theForward, theDirection);
                float theHeading = UMArcTan2(-theDirection.mX, theDirection.GetY());
                if (inUnit->mInitialContactLocation != thePosition)
                    inUnit->mInitialContactLocation = thePosition;
                if (inUnit->mInitialContactHeading != theHeading)
                    inUnit->mInitialContactHeading = theHeading;
            }
            long theRevs = -1;
            EEUGRust* theLocationGoal = theGoal->mKind == kGoalLocation ? static_cast<EEUGRust*>(theGoal) : 0;
            if (theLocationGoal != NULL) {
                theUseGraph = !theLocationGoal->GetIsAutoWaypoint() && !theGoal->mB1D;
                if (theLocationGoal->GetGroupPathState() == 1) {
                    theRevs = 15;
                    theLocationGoal->SetGroupPathState(2);
                } else {
                    theArg8 = !theUseGraph;
                }
            } else {
                theUseGraph = !theGoal->mB1D;
            }
            if (inUnit->mPlayer->AvoidsEnemies(inUnit))
                theModifier = new EEAIGPModifierEnemyAvoidance(inUnit->mPlayer->GetPlayerID());
            theGoal->mPathAttempts++;
            EETileCoordinate theTo(UMFloor(theGoalPoint.mX), UMFloor(theGoalPoint.GetY()));
            gPathManager.RequestUnitPath(inUnit, inUnit->GetTile(), theTo, false, theRevs, theUseGraph, theModifier,
                                         theArg8);
            if (theModifier != NULL)
                theModifier->Dereference();
        }
    }
    if (inUnit->mWaypoints.size() == 0)
        inUnit->SetCurrentPhysicsAction(4);
}

// Rocks a moving, living space unit that takes damage by a random roll to either side, clamped to the maximum roll
void EEUnitClass::UnitDamaged(EEComplexUnit* inUnit, EEComplexUnit* inAttacker, long inDamage)
{
    EEComplexUnit* theUnit = inUnit;
    if ((!(this->mDomains & kMovementSpace)) || theUnit == NULL || !(theUnit->GetMaxVelocity() > 0.0f) || !(theUnit->mHitPoints > 0))
        return;
    float theDelta;
    if (gPhysics->mRandom.Generate() < 0.5f)
        theDelta = gPhysics->GetRandom(kMinDamageRoll, kMaxDamageRoll);
    else
        theDelta = gPhysics->GetRandom(-kMaxDamageRoll, -kMinDamageRoll);
    float theValue = theUnit->mRoll; theValue += theDelta;
    if (theValue > kMaxDamageRoll)
        theValue = kMaxDamageRoll;
    else if (theValue < -kMaxDamageRoll)
        theValue = -kMaxDamageRoll;
    theUnit->mRoll = theValue;
}

// Counts one unit of the class gone and records the time when the last one goes
void EEUnitClass::UnitRemoved()
{
    if (--this->mNumUnits == 0)
        this->mLastUnitGoneTime = TMGetTime();
}

// Adds a unit class to the class table under its unit type and to the list of unit types, keeping a reference
void EEUnitClass::RegisterUnitClass(EEUnitClass* inClass)
{
    if (inClass == NULL)
        return;
    inClass->Reference();
    gUnitClassTable[inClass->mUnitType] = inClass;
    EEUnitClass::sUnitTypes.push_back(inClass->mUnitType);
}

// Removes a placed unit (not a path marker) from the map's unit region that holds its tile
// NEAR: the original calls a vector remove helper 0x239ee2 (find + erase; a COMDAT of this unit)
void EEUnitClass::BaseRemoveFromRegion(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    U2DPoint<long> theRegion(0, 0);
    EeCombatUnitRegions* theTable = theMap->mUnitRegions;
    if (inUnit->mTileNode == NULL || inUnit->IsPathMarker())
        return;
    long theSize = ee_combat_regions_size(theTable);
    theRegion = U2DPoint<long>(inUnit->GetTileX() / theSize, inUnit->GetTileY() / theSize);
    ee_combat_regions_remove(theTable, (uint32_t)theRegion.mX, (uint32_t)theRegion.mY, (uint64_t)(uintptr_t)inUnit->GetComplexUnit());
}

// Releases the class table's entry for a unit type and clears it
void EEUnitClass::ReleaseUnitClass(unsigned long inUnitType)
{
    EEUnitClass*& theEntry = gUnitClassTable[inUnitType];
    if (theEntry != NULL)
        theEntry->Dereference();
    theEntry = NULL;
}

// Removes a unit from the tile it stands on, if any
void EEUnitClass::BaseRemoveFromTile(EEUnit* inUnit)
{
    RTSTile* theTile = inUnit->mTile;
    if (theTile != NULL)
        theTile->RemoveUnit(inUnit);
}

// Moves a unit on along its path: a melee attacker closing in retargets or sidesteps to the less crowded side;
// otherwise merges a short bounded path toward the goal into its waypoints, or stops when it is already there
// The sidestep counts the units on the side tiles of the unit and of the goal; a diagonal squeeze counts as blocked.
// The bounded path runs to the goal or a waypoint further on, avoiding mBoundedPathTiles
void EEUnitClass::FindPath(EEComplexUnit* inUnit)
{
    std::vector<GE3DPoint> thePath;
    std::vector<U2DPoint<long> > theLine;
    std::vector<U2DPoint<long> > theTiles;
    GE3DPoint theGoalPoint;
    EEUnitGoal* theGoal = inUnit->GetGoal();
    EEUnitGoal* theAttack = inUnit->mActiveGoal;
    bool theReached = false;
    GE3DPoint thePosition;
    U2DPoint<long> theTile;
    U2DPoint<long> theSideTile;
    EEUnit* theDummy;
    inUnit->GetPosition(thePosition);
    if (theGoal != NULL) {
        theGoal->GetGoalLocation(theGoalPoint);
        theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
        if (inUnit->mUnitClass->mIsMelee && inUnit->mVelocity > 0.0f && theAttack != NULL && theAttack->mKind == kGoalAttackUnit) {
            float theRange = inUnit->mUnitClass->mInnerCollisionSphere + inUnit->mUnitClass->mInnerCollisionSphere + 5.0f;
            bool theFirst;
            if (theGoalPoint.GetX() > 0.1f && thePosition.DistanceToSquared(theGoalPoint) < theRange * theRange) {
                theFirst = static_cast<EEUGRust*>(theAttack)->GetSidestepCount() < 1;
                inUnit->DropPath();
            } else {
                theFirst = false;
            }
            if (theFirst) {
                EEComplexUnit* theTarget = static_cast<EEUGRust*>(theAttack)->GetAttackTarget()->GetComplexUnit();
                if (theTarget->mUnitClass->mIsWall || !theGoal->mB1C) {
                    inUnit->IgnoreTarget(theTarget);
                    EETileCoordinate theUnitTile = inUnit->GetTile();
                    EEFOFindNewTargetInMeleeRange theScreen(inUnit, theTarget);
                    if (EEFindNewTarget(&theScreen, theAttack, inUnit, &theUnitTile, 1,
                                   theTarget != NULL ? theTarget->mUnitClass->mIsWall : false, 1)) {
                        static_cast<EEUGRust*>(theAttack)->SetSidestepCount(0);
                        return;
                    }
                }
                inUnit->mCollisionResolutionCount = 0.0f;
                inUnit->SetBlockingUnit(NULL);
                inUnit->GetPosition(thePosition);
                float theRadius = inUnit->mUnitClass->mInnerCollisionSphere;
                GEVector theSide(-(theGoalPoint.GetY() - thePosition.GetY()), theGoalPoint.GetX() - thePosition.GetX(), 0.0f);
                long theLeft = 0;
                long theRight = 0;
                theSide.Normalize();
                float theScale = theRadius + theRadius;
                theSide.mX *= theScale;
                theSide.mY *= theScale;
                theSide.mZ *= theScale;
                // the left side: beside the unit, then beside the goal
                thePosition.mY += theSide.GetY();
                thePosition.mX += theSide.GetX();
                theSideTile = U2DPoint<long>(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY()));
                bool theLeftSqueezed = false;  // a squeeze beside the unit settles the left side (as the right)
                if (EECanStandOn(theSideTile, inUnit)) {
                    if (UMAbs(theSideTile.GetX() - inUnit->GetTileX()) == 1 && UMAbs(theSideTile.GetY() - inUnit->GetTileY()) == 1 &&
                        EEIsTileTopAbove(theSideTile.GetX(), inUnit->GetTileY(), 0.0f, &theDummy, true) &&
                        EEIsTileTopAbove(inUnit->GetTileX(), theSideTile.GetY(), 0.0f, &theDummy, true)) {
                        theLeft = 0x7fffffff;
                        theLeftSqueezed = true;
                    } else {
                        for (RTSTileUnitNode* theNode = gWorld.mMap->GetTile(theSideTile.GetX(), theSideTile.GetY())->mUnits;
                             theNode != NULL; theNode = theNode->mNext)
                            theLeft++;
                    }
                }
                theGoalPoint.mY += theSide.GetY();
                theGoalPoint.mX += theSide.GetX();
                theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
                if (!theLeftSqueezed && EECanStandOn(theTile, inUnit)) {
                    if (UMAbs(theTile.GetX() - inUnit->GetTileX()) == 1 && UMAbs(theTile.GetY() - inUnit->GetTileY()) == 1 &&
                        EEIsTileTopAbove(theTile.GetX(), inUnit->GetTileY(), 0.0f, &theDummy, true) &&
                        EEIsTileTopAbove(inUnit->GetTileX(), theTile.GetY(), 0.0f, &theDummy, true)) {
                        theLeft = 0x7fffffff;
                    } else {
                        for (RTSTileUnitNode* theNode = gWorld.mMap->GetTile(theTile.GetX(), theTile.GetY())->mUnits;
                             theNode != NULL; theNode = theNode->mNext)
                            theLeft++;
                    }
                }
                // the right side (a squeeze beside the unit settles it)
                thePosition.mY -= theSide.GetY() + theSide.GetY();
                thePosition.mX -= theSide.GetX() + theSide.GetX();
                theSideTile = U2DPoint<long>(UMFloor(thePosition.GetX()), UMFloor(thePosition.GetY()));
                bool theSqueezed = false;
                if (EECanStandOn(theSideTile, inUnit)) {
                    if (UMAbs(theSideTile.GetX() - inUnit->GetTileX()) == 1 && UMAbs(theSideTile.GetY() - inUnit->GetTileY()) == 1 &&
                        EEIsTileTopAbove(theSideTile.GetX(), inUnit->GetTileY(), 0.0f, &theDummy, true) &&
                        EEIsTileTopAbove(inUnit->GetTileX(), theSideTile.GetY(), 0.0f, &theDummy, true)) {
                        theRight = 0x7fffffff;
                        theSqueezed = true;
                    } else {
                        for (RTSTileUnitNode* theNode = gWorld.mMap->GetTile(theSideTile.GetX(), theSideTile.GetY())->mUnits;
                             theNode != NULL; theNode = theNode->mNext)
                            theRight++;
                    }
                }
                if (!theSqueezed) {
                    theGoalPoint.mY -= theSide.GetY() + theSide.GetY();
                    theGoalPoint.mX -= theSide.GetX() + theSide.GetX();
                    theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
                    if (EECanStandOn(theTile, inUnit)) {
                        if (UMAbs(theTile.GetX() - inUnit->GetTileX()) == 1 && UMAbs(theTile.GetY() - inUnit->GetTileY()) == 1 &&
                            EEIsTileTopAbove(theTile.GetX(), inUnit->GetTileY(), 0.0f, &theDummy, true) &&
                            EEIsTileTopAbove(inUnit->GetTileX(), theTile.GetY(), 0.0f, &theDummy, true)) {
                            theRight = 0x7fffffff;
                        } else {
                            for (RTSTileUnitNode* theNode = gWorld.mMap->GetTile(theTile.GetX(), theTile.GetY())->mUnits;
                                 theNode != NULL; theNode = theNode->mNext)
                                theRight++;
                        }
                    }
                }
                if (theRight < theLeft && theRight != 0x7fffffff) {
                    inUnit->AddWaypoint(thePosition, false);
                } else {
                    if (theLeft != 0x7fffffff) {
                        thePosition.mY += theSide.GetY() + theSide.GetY();
                        thePosition.mX += theSide.GetX() + theSide.GetX();
                        inUnit->AddWaypoint(thePosition, false);
                        theGoalPoint.mY += theSide.GetY() + theSide.GetY();
                        theGoalPoint.mX += theSide.GetX() + theSide.GetX();
                    } else {
                        static_cast<EEUGRust*>(theAttack)->SetSidestepCount(1);
                        this->FindPath(inUnit);
                        return;
                    }
                }
                inUnit->AddWaypoint(theGoalPoint, false);
                theGoal->mPathAttempts += 8;
                static_cast<EEUGRust*>(theAttack)->SetSidestepCount(static_cast<EEUGRust*>(theAttack)->GetSidestepCount() + 1);
                return;
            }
        }
    }
    if (theAttack != NULL && theAttack->mKind == kGoalAttackUnit)
        static_cast<EEUGRust*>(theAttack)->SetSidestepCount(0);
    if (inUnit->mWaypoints.size() != 0 && (!(inUnit->mUnitClass->mDomains & kMovementSea) || (long)inUnit->mWaypoints.size() >= 3)) {
        theGoalPoint = inUnit->mWaypoints[0].mLocation;
    } else {
        if (theGoal != NULL)
            theGoal->GetDestination(inUnit, theGoalPoint);
        else
            inUnit->GetPosition(theGoalPoint);
    }
    theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
    if (theTile.GetX() == inUnit->GetTileX() && theTile.GetY() == inUnit->GetTileY()) {
        inUnit->SetHasArrived(true);
        inUnit->mCollisionResolutionCount = 0.0f;
        inUnit->SetBlockingUnit(NULL);
        if (inUnit->mCollisionState != 8)
            inUnit->mCollisionState = 8;
        return;
    }
    inUnit->mBoundedPathTiles.clear();
    long theIndex;
    if (!(this->mDomains & kMovementAirOrSea)) {
        if ((long)inUnit->mWaypoints.size() > 7)
            theIndex = 7;
        else
            theIndex = inUnit->mWaypoints.size() - 1;
    } else {
        theIndex = inUnit->mWaypoints.size() - 1;
        for (long i = 0; i < (long)inUnit->mWaypoints.size(); i++) {
            if (inUnit->DistanceSquared3D(inUnit->mWaypoints[i].mLocation) >= kMinWaypointDistanceSquared) {
                theIndex = i;
                break;
            }
        }
    }
    EEUnit* theCarrier = static_cast<EEUnit*>(inUnit->mBlockingUnit);
    if (theCarrier != NULL) {
        if (theCarrier->mUnitClass->mInnerCollisionSphere >= 2.0f) {
            EEGetUnitTiles(theCarrier, theTiles, 0.0f);
            for (unsigned long i = 0; i < theTiles.size(); i++)
                inUnit->mBoundedPathTiles.push_back(theTiles[i]);
        } else {
            inUnit->mBoundedPathTiles.push_back(theCarrier->GetTile());
        }
    }
    inUnit->mCollisionResolutionCount = 0.0f;
    inUnit->SetBlockingUnit(NULL);
    long theFromX = UMFloorUnsigned(thePosition.GetX());
    long theFromY = UMFloorUnsigned(thePosition.GetY());
    long theToX = UMFloorUnsigned(theGoalPoint.GetX());
    long theToY = UMFloorUnsigned(theGoalPoint.GetY());
    EEGetTileLine(theFromX, theFromY, theToX, theToY, &theLine, 0, 1);
    if (abs(theFromX - theToX) == 1 && abs(theFromY - theToY) == 1) {
        theLine.push_back(U2DPoint<long>(theFromX, theToY));
        theLine.push_back(U2DPoint<long>(theToX, theFromY));
    }
    if ((long)inUnit->mWaypoints.size() > 1)
        theLine.push_back(U2DPoint<long>(UMFloor(inUnit->mWaypoints[0].mLocation.GetX()), UMFloor(inUnit->mWaypoints[0].mLocation.GetY())));
    if (theIndex >= 0)
        theGoalPoint = inUnit->mWaypoints[theIndex].mLocation;
    else
        inUnit->GetPosition(theGoalPoint);
    theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
    for (unsigned long i = 0; i < theLine.size(); i++) {
        if ((theLine[i]).GetX() != theTile.GetX() || (theLine[i]).GetY() != theTile.GetY())
            inUnit->mBoundedPathTiles.push_back(theLine[i]);
    }
    long theType;
    if (!(inUnit->GetUnitClass()->mDomains & kMovementSea))
        theType = (inUnit->GetUnitClass()->mDomains & kMovementSpace) ? 16 : 1;
    else
        theType = 2;
    gPathManager.FindBoundedPath(inUnit, gWorld.mMap, theType, inUnit->GetTile(), theTile, thePath, 50, true, &theReached);
    if (thePath.size() > 1) {
        if (UMFloor(thePath.back().GetX()) == theTile.GetX() && UMFloor(thePath.back().GetY()) == theTile.GetY()) {
            thePath.pop_back();
            thePath.push_back(theGoalPoint);
            EEPathManager::MergeWaypoints(thePath, inUnit->mWaypoints, 0);
            inUnit->DropPath();
            for (unsigned long i = UMFloor(thePath[0].GetX()) == inUnit->GetTileX() && UMFloor(thePath[0].GetY()) == inUnit->GetTileY();
                 i < thePath.size() - 1; i++)
                inUnit->AddWaypoint(thePath[i], false);
            inUnit->SetHasArrived(false);
            if (inUnit->mCollisionState != 1)
                inUnit->mCollisionState = 1;
            return;
        }
        if ((long)inUnit->mWaypoints.size() > 1 &&
            UMFloor((*(inUnit->mWaypoints.end() - 2)).mLocation.GetX()) == theTile.GetX() &&
            UMFloor((*(inUnit->mWaypoints.end() - 2)).mLocation.GetY()) == theTile.GetY()) {
            EEPathManager::MergeWaypoints(thePath, inUnit->mWaypoints, 0);
            inUnit->DropPath();
            for (unsigned long i = 0; i < thePath.size(); i++)
                inUnit->AddWaypoint(thePath[i], false);
            inUnit->SetHasArrived(false);
            if (inUnit->mCollisionState != 1)
                inUnit->mCollisionState = 1;
            return;
        }
    }
    if (inUnit->mCollisionState == 2) {
        inUnit->SetBlockingUnit(NULL);
        inUnit->mCollisionResolutionCount = 0.0f;
        inUnit->SetVelocity(0.0f);
        inUnit->SetCurrentPhysicsAction(6);
        EEUGRust* theGather = EEGatherGoalCast(theGoal);
        if (theGather != NULL && theGather->GetGatherResource() != NULL && theGather->GetGatherResource()->mResourceType == kResourceWood)
            inUnit->StartAction(kActionFindNewResource);
        else
            inUnit->StartAction(kActionIdle);
        inUnit->mUnitClass->ActionChanged(inUnit);
        if (inUnit->mCollisionState != 0)
            inUnit->mCollisionState = 0;
    } else {
        inUnit->mCollisionState = 2;
    }
}

// Creates a target screen that accepts units closer to the attacker than its current target
EEFOFindNewTargetInMeleeRange::EEFOFindNewTargetInMeleeRange(EEComplexUnit* inUnit, EEUnit* inTarget)
    : mUnit(inUnit), mTarget(inTarget)
{
    this->mRange = EEGetHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
}

// Returns whether a candidate is alive and closer to the attacker than its current target (new: ee_combat's screen)
// the melee screen's candidate distance (asked by ee_combat's screen for a living candidate only)
struct EEMeleeScreenContext {
    EEComplexUnit* mUnit;
    EEComplexUnit* mCandidate;
};

static float EEMeleeScreenDistance(void* inContext)
{
    EEMeleeScreenContext* theContext = (EEMeleeScreenContext*)inContext;
    return EEGetHitDistanceSquared(theContext->mUnit, theContext->mCandidate, -10000.0f, -10000.0f);
}

static bool EEMeleeScreen(EEFOFindNewTargetInMeleeRange* inScreen, EEComplexUnit* inUnit)
{
    EEMeleeScreenContext theContext = {inScreen->mUnit, inUnit};
    EeCombatScreenIn in = {EE_COMBAT_SCREEN_KIND_MELEE, (int32_t)inUnit->mHitPoints, inScreen->mRange, 0, 0.0f};
    int32_t theResult = ee_combat_screen(&in, &theContext, EEMeleeScreenDistance);
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'F', 'S'), in, theResult);
    return theResult != 0;
}

bool EEFOFindNewTargetInMeleeRange::Virtual04(EEComplexUnit* inUnit)
{
    return EEMeleeScreen(this, inUnit);
}

bool EEFOFindNewTargetInMeleeRange::Virtual08(EEComplexUnit* inUnit)
{
    return EEMeleeScreen(this, inUnit);
}

// Turns an attacked unit on its attacker (or to its location when unseen) and calls idle nearby soldiers to help
void EEUnitClass::Attacked(EEComplexUnit* inUnit)
{
    uint32_t theBefore = inUnit->mLastAttacker != NULL ? (uint32_t)inUnit->mLastAttacker->mUnitID : 0;   // TEST ONLY
    // ee_combat world.rs attacked: the plan's operations on the world host (EECombatGoals.cpp); CBXA records the outcome
    EECombatWorld(EE_COMBAT_WORLD_ATTACKED, inUnit, NULL, this);
    EECombatCaptureXAction(EE_COMBAT_XA_ATTACKED, inUnit, NULL, theBefore);
}

// Sends a unit running away from its attacker or pursuer to the first standable spot of up to 8 directions
void EEUnitClass::FleeAttacker(EEComplexUnit* inUnit)
{
    GE2DPoint theDestination;
    EEUnitGoal* theOldGoal = inUnit->mGoalState;
    EEComplexUnit* theEnemy = inUnit->mLastAttacker;
    if (theEnemy == NULL)
        theEnemy = EEGetPursuerOf(inUnit);
    if (theEnemy != NULL) {
        long theDistance;
        long theDirection;
        if (inUnit->mPlayer->GetPlayerID() != 0) {
            theDistance = UMFloatToLong(inUnit->GetLineOfSight() * kFleeDistanceScale);
            theDirection = EEGetDirectionTo(inUnit, theEnemy);
            theDirection = EETurnAway(theDirection, true);
        } else {
            theDistance = gPhysics->GetRandom(1L, 2L);
            theDirection = EEGetDirectionTo(inUnit, theEnemy);
            theDirection = EETurnAway(theDirection, false);
        }
        long theCount = 0;
        do {
            GE2DPoint theOrigin(inUnit->mPhysicalModel.GetLocation().GetX(), inUnit->mPhysicalModel.GetLocation().GetY());
            EEMoveInDirection(theOrigin, theDestination, theDirection, theDistance);
            theDirection = (theDirection + 1) % 8;
            ++theCount;
            if (theCount > 7)
                break;
            EETileCoordinate theTile(UMFloor(theDestination.GetX()), UMFloor(theDestination.GetY()));
            if (EECanStandOn(theTile, inUnit))
                break;
        } while (true);
        if (theCount <= 7) {
            EEUnitGoal* theGoal = EEUGRust::NewLocation(theDestination, true, true, false, 0.0f, false, 0);
            inUnit->SetGoal(theGoal, theOldGoal != NULL && theOldGoal->mKind == kGoalExplore, 0);
            inUnit->StartAction(kActionPrepareToMove);
            this->ActionChanged(inUnit);
        } else {
            inUnit->StartAction(kActionIdle);
            inUnit->mUnitClass->ActionChanged(inUnit);
        }
    } else {
        this->Flee(inUnit);
    }
    if (inUnit->mLastAttacker != NULL)
        inUnit->mLastAttacker->Dereference();
    inUnit->mLastAttacker = NULL;
}

// Creates the class's remains unit for the dead unit's player at a free spot near where it died
void EEUnitClass::CreateRemainsUnit(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(false);
    theTransformation = inUnit->mPhysicalModel.GetLocation();
    EEUnitClass* theClass = this->mRemainsClass;
    if (theClass == NULL)
        return;
    GE3DPoint thePoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    if (!EEFindPlacement(&thePoint, theClass, inUnit->GetTileX(), inUnit->GetTileY(), this->GetTileSize(), 1,
        !inUnit->mPlayer->IsHuman(), false, false, &theTransformation, inUnit))
        return;
    if (!(theTransformation.GetZ() > 0.0f))
        theTransformation.SetZ(0.3f);
    theClass->CreateUnitAt(theTransformation, inUnit->mPlayer->GetPlayerID(), false, NULL);
}

// Spawns the class's death unit where the unit (or its container) was and launches it at that ground point
void EEUnitClass::CreateDeathUnit(EEComplexUnit* inUnit)
{
    EEUnitClass* theClass = this->mDeathSpawnClass;
    if (theClass == NULL)
        return;
    GETransformation theTransformation(false);
    long theX, theY;
    if (inUnit->mContainer == NULL) {
        theTransformation = inUnit->mPhysicalModel.GetLocation();
        theX = inUnit->GetTileX();
        theY = inUnit->GetTileY();
    } else {
        theTransformation = inUnit->mContainer->mPhysicalModel.GetLocation();
        theX = inUnit->mContainer->GetTileX();
        theY = inUnit->mContainer->GetTileY();
    }
    if (!(theTransformation.GetZ() > 0.0f))
        theTransformation.SetZ(0.3f);
    EEUnit* theCreated = theClass->CreateUnit(theX, theY, inUnit->mPlayer->GetPlayerID(), false, NULL);
    EEComplexUnit* theComplex = theCreated->GetComplexUnit();
    EEUnitGoal* theGoal = EEUGRust::NewAttackGround(GE3DPoint(theTransformation.GetX(), theTransformation.GetY(),
                                                         theTransformation.GetZ()), false);
    theComplex->SetVelocity(0.0f);
    theComplex->SetLauncherClass(theCreated->GetUnitClass());
    // the dying unit owns its death unit, as a shooter owns its shot (FireProjectile)
    theComplex->SetOwner(inUnit);
    theComplex->SetProjectileStart(GE3DPoint(theTransformation.GetX(), theTransformation.GetY(),
                                      theTransformation.GetZ()));
    theComplex->SetGoal(theGoal, 0, 0);
    theComplex->StartAction(kActionAdvance);
    theComplex->mUnitClass->ActionChanged(theComplex);
    if (theComplex->mUnitClass->mMuzzleFlashEffect != NULL) {
        gEffectManager->CreatePlacedEffect(GE3DPoint(theTransformation.GetX(), theTransformation.GetY(),
                                                     theTransformation.GetZ()), theComplex->mUnitClass->mMuzzleFlashEffect, NULL);
    }
}

// Fires the class's projectile from the unit's turret weapon point at a target or ground location;
// without a turret part, fires a plain shot
// The launch point is the weapon tag point mWeaponTagPoint scaled by the stats' scale and turned by the turret's
// yaw, under the unit's transformation; the shot is owned by inUnit (new: the launch is ee_combat's launch_from_turret)
void EEUnitClass::LaunchFromTurret(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEModel* theModel = inUnit->GetModel();
    EETagPointList* theList = theModel->mTagPoints[inUnit->GetAnimation()];
    unsigned long thePart = 0;
    unsigned long theIndex = inUnit->mWeaponTagPoint;
    if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, thePart) == 0) {
        // the launch is ee_combat's launch_from_turret (launch.rs) over the tag point's model chain at the frame
        EeCombatLaunchIn in = {};
        std::vector<float> theChain;
        in.kind = EE_COMBAT_LAUNCH_KIND_TURRET;
        memcpy(in.location, inUnit->mPhysicalModel.GetLocation().mMatrix, sizeof(in.location));
        in.scale = theStats->mScale;
        in.yaw = inUnit->mTurretAngles[thePart];
        EECombatTagPointChain(theList->GetTagPoint(theIndex), inUnit->mPhysicalModel.GetFrame(), in.point, theChain,
                              in.chain);
        EeCombatLaunchOut theOut = ee_combat_launch(&in, theChain.empty() ? NULL : (const float(*)[16])&theChain[0]);
        EECombatChainCapture(EE_COMBAT_TAG('C', 'B', 'L', 'T'), in, theChain, theOut);
        GETransformation theLaunch(false);
        memcpy(theLaunch.mMatrix, theOut.launch, sizeof(theOut.launch));
        EEUnit* theShot = this->mProjectileClass->CreateUnitAt(theLaunch, inUnit->mPlayer->GetPlayerID(), false, inUnit);
        if (this->mCreationEffect != NULL)
            gEffectManager->CreateUnitEffect(theShot, this->mCreationEffect, NULL, false, 0, 0);
        theShot->GetComplexUnit()->SetLauncherClass(this);
        theShot->GetComplexUnit()->SetOwner(inUnit);
        EEUnitGoal* theGoal;
        if (inTarget != NULL)
            theGoal = EEUGRust::NewAttackUnit(theShot->GetComplexUnit(), inTarget, false, false);
        else
            theGoal = EEUGRust::NewAttackGround(inLocation, false);
        theShot->GetComplexUnit()->SetGoal(theGoal, 0, 0);
        theShot->GetComplexUnit()->SetVelocity(inUnit->mVelocity);
        theShot->GetComplexUnit()->SetHeight(inUnit->mPhysicalModel.GetLocation().GetZ());
        this->mProjectileClass->LaunchProjectile(theShot->GetComplexUnit(), inLocation);
    } else
        this->LaunchShot(inUnit, inTarget, inLocation, NULL);
}

// Unloads the class's acknowledge, selection, death and creation sounds, its model and its button
void EEUnitClass::UnloadClassMedia()
{
    long i;
    for (i = 0; i < 2; i++)
        if (this->mAcknowledgeSounds[i] != NULL)
            this->mAcknowledgeSounds[i]->UnloadMedia();
    for (i = 0; i < 2; i++)
        if (this->mSelectionSounds[i] != NULL)
            this->mSelectionSounds[i]->UnloadMedia();
    if (this->mDeathSound != NULL)
        this->mDeathSound->UnloadMedia();
    if (this->mCreationSound != NULL)
        this->mCreationSound->UnloadMedia();
    if (this->mModel != NULL)
        this->mModel->UnloadMedia();
    if (this->mButton != NULL)
        this->mButton->Release();
}

// Releases a human player's buttons for the class's research technologies and commands, and calamity media
void EEUnitClass::UnloadButtons(EEPlayer* inPlayer)
{
    EETechTree* theTree = inPlayer->mTechnologies;
    if (theTree != NULL) {
        for (long i = 0; i < (long)this->mResearchTechnologies.size(); i++) {
            EETechTreeNode* theNode = theTree->Get(this->mResearchTechnologies[i]);
            // a technology missing from the player's tree has nothing to release (its data used to be read
            // through NULL)
            if (theNode == NULL)
                continue;
            if (inPlayer->IsHuman() == true)
                theNode->mButton->Release();
            RTSTechTreeNodeData* theData = theNode->mData;
            if (theData->mKind == 5) {
                EECalamityEnum theEnum = (EECalamityEnum)theData->mCalamity;
                USingleton<EECalamityManager>::GetInstance()->UnloadMedia(inPlayer, theEnum);
            }
        }
    }
    for (long j = 0; j < 9; j++)
        if (this->mCommands[j] >= 2 && this->mCommands[j] < 0x32 && inPlayer->IsHuman() == true)
            EEButtonObject::Get(this->mCommands[j])->Release();
}

// Unloads a player's media for the classes this class spawns and for all its effects, and marks it unloaded
void EEUnitClass::BaseUnloadMedia(EEPlayer* inPlayer)
{
    if (this->mUnitType == kUnitTypeAirport) {
        inPlayer->UnloadUnitClassMedia(0xf0);
        EEGraphicEffect::Get(kGraphicEffectRallyFlagFighter)->Unload();
        EEGraphicEffect::Get(kGraphicEffectRallyFlagBomber)->Unload();
        EEGraphicEffect::Get(kGraphicEffectRallyFlagAtomicBomber)->Unload();
        EEGraphicEffect::Get(kGraphicEffectRallyFlagParatrooper)->Unload();
    }
    if (this->mUnitType == kUnitTypeGranaryImproved || this->mUnitType == kUnitTypeGranary)
        inPlayer->UnloadUnitClassMedia(0x2a);
    if (this->mProjectileClass != NULL)
        inPlayer->UnloadUnitClassMedia(this->mProjectileClass->mUnitType);
    if (this->mScaffoldingClass != NULL)
        inPlayer->UnloadUnitClassMedia(this->mScaffoldingClass->mUnitType);
    if (this->mDeathClass != NULL)
        inPlayer->UnloadUnitClassMedia(this->mDeathClass->mUnitType);
    if (this->mDeathSpawnClass != NULL)
        inPlayer->UnloadUnitClassMedia(this->mDeathSpawnClass->mUnitType);
    if (this->mRemainsClass != NULL)
        inPlayer->UnloadUnitClassMedia(this->mRemainsClass->mUnitType);
    for (long i = 0; i < kUnitClassEffectCount; i++)   // the 29 effects, in their order (+0x64..+0xd4)
        if (this->GetEffects()[i] != NULL)
            this->GetEffects()[i]->Unload();
    this->mIsMediaLoaded = false;
}

// Fires a shot when an attacking unit's animation steps across one of its attack set's sound frames
void EEUnitClass::FireOnAttackFrame(EEComplexUnit* inUnit, long inArg, long inFrame)
{
    long theIndex = EEGetAttackFrameSet(inUnit);
    unsigned long theFirst = inUnit->GetModel()->mSound1Frames[theIndex];
    unsigned long theLast = inUnit->GetModel()->mSound2Frames[theIndex];
    unsigned long theOld = inUnit->mPhysicalModel.GetFrame();
    unsigned long theFrame = inFrame;
    if (inUnit->GetAnimation() == theIndex && inArg != 0 && !inUnit->mIsAnimationPaused &&
        ((theFirst > theFrame && (theOld >= theFirst || theOld <= theFrame)) ||
         (theFrame > theFirst && (theOld <= theFrame && theOld >= theFirst)) ||
         (theLast > theFrame && (theOld >= theLast || theOld <= theFrame)) ||
         (theFrame > theLast && (theOld <= theFrame && theOld >= theLast))))
        this->FireShot(inUnit);
}

// Uncloaks a unit while an enemy is within its cloak radius and moves it on the minimap when that changes
void EEUnitClass::UpdateCloakDetection(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    GE3DPoint thePosition;
    EEPlayer* thePlayer = inUnit->mPlayer;
    long theRadius = 0;
    bool theOldConcealed = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
    bool theConcealed = true;
    if (!(inUnit->mHitPoints > 0))
        return;
    inUnit->GetPosition(thePosition);
    bool theOldVisible = EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer());
    inUnit->SetCloakedChain(true);
    if (inUnit->HasEffect(8)) {
        long theInput = theRadius;
        inUnit->mAffectingAreaEffects.ApplyModifiers(3, theInput, theRadius, inUnit);
        theRadius = max(theRadius, this->mCloakRadius);
    } else if (inUnit->mU32C != 0)
        theRadius = UMFloatToLong(gWorldTable.GetFloatValue(kWorldCamouflageTiming) * inUnit->mStats->mRange);
    else
        theRadius = this->mCloakRadius;
    if ((float)theRadius > 0.0f && EEGetUnitsNear(thePosition, (float)theRadius + 0.5f, theUnits) > 0) {
        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); ++it) {
            EEUnit* theOther = *it;
            if (theOther == inUnit || !(theOther->mHitPoints > 0) || theOther->mPlayer->GetPlayerID() == 0 ||
                theOther->mUnitClass->mIsProjectile || theOther->mPlayer == thePlayer ||
                thePlayer->mEnemies[theOther->mPlayer->GetPlayerID()] == 0)
                continue;
            inUnit->SetCloakedChain(false);
            theConcealed = false;
            break;
        }
    }
    bool theVisible = EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer());
    if (theOldConcealed == theConcealed)
        return;
    if (theOldConcealed && theOldVisible && !theVisible)
        gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
    else if (theConcealed && !theOldVisible && theVisible)
        gMiniMap->MoveUnit(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), -10000, -10000);
}

// Turns each turret toward the unit's attack, heal or calamity target by its turn rate; else turns them back
// port (R5b.4): the turn is Rust (ee_units turret.rs) over the parts' numbers; the C++ finds the goal's point
void EEUnitClass::TurnTurrets(EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mGoalState;
    if (theGoal == NULL || (theGoal->mKind != kGoalAttackUnit && theGoal->mKind != kGoalAttackGround && theGoal->mKind != kGoalCastCalamity && theGoal->mKind != kGoalHealUnit)) {
        this->TurnTurretsBack(inUnit);
        return;
    }
    GE2DPoint theTarget;
    if (theGoal->mKind == kGoalAttackUnit) {
        EEUnit* theUnit = static_cast<EEUGRust*>(theGoal)->GetAttackTarget();
        if (theUnit->GetComplexUnit() != NULL) {
            theTarget.SetX(theUnit->GetComplexUnit()->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theUnit->GetComplexUnit()->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float)theUnit->GetTileX());
            theTarget.SetY((float)theUnit->GetTileY());
        }
    } else {
        GE3DPoint theGoalPosition;
        theGoal->GetGoalLocation(theGoalPosition);
        theTarget.SetX(theGoalPosition.GetX());
        theTarget.SetY(theGoalPosition.GetY());
    }
    EETurnTurretsRust(inUnit, true, theTarget.GetX(), theTarget.GetY());
}

// Advances a unit's animation by this tick, plays the frame sounds and returns the number of frames stepped
long EEUnitClass::Animate(EEComplexUnit* inUnit)
{
    // port (R5b.4): the clock's decisions are Rust (ee_units animate.rs) over the unit's and its model's numbers
    // for the current state; the C++ applies them in the original's order and keeps the idle pick's random draw,
    // the horse's breath and the sound
    EESoundObject* theSound = NULL;
    EEGraphicObject* theObject = inUnit->GetModel();
    unsigned long theState = inUnit->GetAnimation();
    EeUnitsAnimateIn theIn;
    theIn.paused = inUnit->mIsAnimationPaused;
    theIn.loops = theObject->mLoops[theState];
    theIn.speed_scaled = theObject->mIsSpeedScaled[theState];
    theIn.shares_frames = theObject->mSharesFrames[theState];
    // read where the original read it: a paused idle loop's state 8 test
    theIn.can_fire_over_walls = theIn.paused && theIn.loops && theState == 8 ? inUnit->mStats->mCanFireOverWalls : false;
    theIn.foundation = !theIn.paused && !theIn.loops ? inUnit->IsFoundation() : false;
    theIn.state = (uint32_t)theState;
    theIn.tick_ms = (uint32_t)gPhysics->mTickTimeMs;
    theIn.time = (uint32_t)inUnit->mAnimationTime;
    theIn.frame = (uint32_t)inUnit->mPhysicalModel.GetFrame();
    theIn.hold_time = (uint32_t)theObject->mHoldTimes[theState];
    theIn.start_frame = (uint32_t)theObject->mStartFrames[theState];
    theIn.last_frame = (uint32_t)theObject->mLastFrames[theState];
    theIn.frame_count = (uint32_t)theObject->mFrameCounts[theState];
    theIn.frame_time = (uint32_t)theObject->mFrameTimes[theState];
    theIn.sound1_frame = (uint32_t)theObject->mSound1Frames[theState];
    theIn.sound2_frame = (uint32_t)theObject->mSound2Frames[theState];
    theIn.velocity = 0.0f;
    theIn.max_velocity = 0.0f;
    theIn.yaw_velocity = 0.0f;
    theIn.max_angular_velocity = 0.0f;
    if (!theIn.paused && theIn.speed_scaled) {
        theIn.velocity = inUnit->GetVelocity();
        theIn.max_velocity = this->GetMaxVelocity();
        theIn.yaw_velocity = inUnit->mAngularVelocity.GetYaw();
        theIn.max_angular_velocity = this->mMaxAngularVelocity;
    }
    EeUnitsAnimateOut theOut;
    ee_units_animate(&theIn, &theOut);
    if (theOut.set_paused)
        inUnit->SetIsAnimationPaused(theOut.paused);
    if (theOut.set_frame)
        inUnit->SetAnimationFrame(theOut.frame);
    if (theOut.set_time)
        inUnit->SetAnimationTime(theOut.time);
    if (theOut.stop_sound) {
        SESMInstanceNode* theInstance = inUnit->mSound;
        if (theInstance != NULL) {
            gSoundManager->Stop(theInstance, !gSoundManager->GetIsLooping(theInstance));
            inUnit->ReleaseSound();
        }
    }
    if (theOut.idle_pick) {
        if (!(gPhysics->GetRandom() >= 0.5f) || inUnit->mStats->mCanFireOverWalls)
            inUnit->SetAnimation(7, -1, 0);
        else
            inUnit->SetAnimation(8, -1, 0);
        if (this->mIsHorse && theObject->GetTagPoints(theState, 0x16) != NULL) {
            bool theAllowed = true;
            RTSTile* theTile = inUnit->mTile;
            if (gWorld.mTimeOfDay < 0.45f && gWorld.mTimeOfDay > 0.075f &&
                (theTile == NULL || theTile->mTerrainType != kTerrainSnow)) {
                U2DPoint<long> theTile(inUnit->GetTileX(), inUnit->GetTileY());
                theAllowed = gWorld.mEnvironmentManager->IsInStorm(theTile, false, true);
            }
            if (theAllowed && EEIsSeen(inUnit)) {
                EEEffectManager* theManager = gEffectManager;
                theManager->CreateEffectBetweenUnits(inUnit, inUnit, EEGraphicEffect::Get(kGraphicEffectHorseBreathUnits), NULL);
            }
        }
    }
    if (theOut.sound == 1)
        theSound = theObject->mSounds1[theState];
    else if (theOut.sound == 2)
        theSound = theObject->mSounds2[theState];
    if (!theIn.paused) {
        if (inUnit->mAnimationFraction != theOut.fraction)
            inUnit->mAnimationFraction = theOut.fraction;
        inUnit->SetNextFrame(theOut.next_frame);
    }
    unsigned long theSteps = theOut.steps;
    if (theSound == NULL)
        return theSteps;
    SESMInstanceNode* theInstance = inUnit->mSound;
    unsigned long theSample = theSound->GetSampleID();
    float theVolume, thePan, thePitch;
    if (theSample == gSoundManager->GetSampleID(theInstance)) {
        this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
        if (gSoundManager->GetIsLooping(theInstance)) {
            gSoundManager->SetSoundParameters(theInstance, theVolume, thePan, thePitch);
            return theSteps;
        }
    } else {
        if (theInstance != NULL) {
            gSoundManager->Stop(theInstance, !gSoundManager->GetIsLooping(theInstance));
            inUnit->ReleaseSound();
        }
        this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
    }
    theInstance = gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theVolume, thePan, thePitch,
        theSound->GetLooping(), const_cast<char*>((const char*)this->mName));
    inUnit->SetSound(theInstance);
    if (theInstance != NULL)
        theInstance->Dereference();
    return theSteps;
}

// Counts down a unit's trail timer and creates the class's trail effect each time it runs out
void EEUnitClass::UpdateTrail(EEComplexUnit* inUnit)
{
    long theTime = inUnit->mTrailEffectTimer;
    if (theTime > 0) {
        long theLeft = theTime - gPhysics->mTickTimeMs;
        if (theLeft < 0)
            theLeft = 0;
        inUnit->SetTrailEffectTimer(theLeft);
    } else {
        gEffectManager->CreateUnitEffect(inUnit, this->mTrailEffect, NULL, 0, 0, 0);
        inUnit->SetTrailEffectTimer(this->mTrailEffect->mTime1);
    }
}

// Creates ripples behind a unit on water at a rate that grows with its speed
void EEUnitClass::UpdateRipples(EEComplexUnit* inUnit)
{
    if (inUnit->mTile == NULL || (inUnit->mTile->mTerrainType != kTerrainDeepWater && inUnit->mTile->mTerrainType != kTerrainShallowWater))
        return;
    long theTime = inUnit->mWakeTimer;
    float theProgress = inUnit->GetVelocity();
    float theRate = inUnit->GetInverseMaxVelocity() * theProgress;
    unsigned long theMaximum = this->mRippleEffect->mTime2;
    if (theTime < theMaximum - (theMaximum - this->mRippleEffect->mTime1) * theRate) {
        long theNewTime = theTime + gPhysics->mTickTimeMs;
        if (inUnit->mWakeTimer != theNewTime)
            inUnit->mWakeTimer = theNewTime;
    } else {
        this->CreateRipples(inUnit);
        if (inUnit->mWakeTimer != 0)
            inUnit->mWakeTimer = 0;
    }
}

// Returns one over the unit's top speed, its active group's when it moves in one
float EEComplexUnit::GetInverseMaxVelocity()
{
    if (this->mActiveGroup != NULL)
        return this->mActiveGroup->GetInverseMaxVelocity(this);
    return this->mStats->mInverseMaxVelocity;
}

// Ticks a building: finishes its production countdown (a unit, retried in 5 seconds when it cannot be placed,
// or a technology), then its orbitals, camouflage, effects, rotting, production queue and garrison healing
// EEUCBuilding::Update; production is instant under the player's mInstantBuild
extern bool& gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)

void EEUnitClass::UpdateBuilding(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDisabled && inUnit->mHitPoints > 0)
        return;
    if (inUnit->mProductionTime > 0 && inUnit->mHitPoints > 0 && !gInCinematic) {
        long theStep = gPhysics->mTickTimeMs;
        if (theStep == 0)
            theStep = 1;
        EEPlayer* thePlayer = inUnit->mPlayer;
        long theLeft = inUnit->mProductionTime - theStep;
        if (thePlayer->mInstantBuild && theLeft >= 0)
            theLeft = -1;
        if (theLeft <= 0) {
            theLeft = 0;
            RTSTechTreeNodeData* theData = thePlayer->mTechnologies->Get(inUnit->mCurrentResearch)->mData;
            if (theData->mMakesUnit) {
                EEUnitClass* theClass = gUnitClassTable[theData->mUnitClass];
                if (!theClass->IsLandBuilding() && !this->ProduceUnit(inUnit, theClass, true))
                    theLeft = 5000;
            } else {
                thePlayer->mTechnologies->Research(inUnit->mCurrentResearch, inUnit);
                inUnit->PlayEffect(100000);
            }
        }
        inUnit->SetProductionTime(theLeft);
    }
    inUnit->UpdateOrbitals();
    this->UpdateCamouflageTimer(inUnit);
    this->UpdateEffects(inUnit);
    if ((inUnit->mCommand != NULL ? inUnit->mCommand->mType : 0) == kActionUnitRotting) {
        long theTime = inUnit->UpdateRottingAwayTime();
        for (EEComplexUnit* theLinked = inUnit->mNextPart; theLinked != NULL; theLinked = theLinked->mNextPart)
            theLinked->SetRottingAwayTime(theTime);
    }
    inUnit->ContinueProduction();
    this->HealGarrison(inUnit);
}

// Ticks an animal: physics, effects, timers and animation; fires on attack frames and lets its carcass rot
void EEUnitClass::UpdateAnimal(EEComplexUnit* inUnit)
{
    unsigned long theFrame = inUnit->mPhysicalModel.GetFrame();
    inUnit->SetLastHitPoints(inUnit->mHitPoints);
    if (this->mPhysicsModel != NULL && inUnit->GetTileX() != -10000)
        this->mPhysicsModel->Step(inUnit);
    this->UpdateEffects(inUnit);
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    long theResult = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    switch (inUnit->GetCommandType()) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theResult, theFrame);
        break;
    case kActionResourcesRotting:
        inUnit->UpdateResourcesRotting();
        break;
    case kActionUnitRotting: {
        inUnit->UpdateRottingAwayTime();
        break;
    }
    }
    this->UpdateDone(inUnit);
}

// TEST ONLY (USTM): the camouflage timers' call as words (the inputs, each event in order, the outputs)
struct EECamouflageNote {
    uint32_t mWords[48];
    unsigned int mCount;
    void Add(uint32_t inWord)
    {
        if (this->mCount < 48)
            this->mWords[this->mCount++] = inWord;
    }
};

// TEST ONLY (USTM): a float's bits
static uint32_t CloakBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

// Counts down a unit's camouflage timers (dropping them without vital buildings) and fades its cloak opacity
void EEUnitClass::UpdateCamouflageTimer(EEComplexUnit* inUnit)
{
    // TEST ONLY (USTM): the events, in the codes of ee_units step.rs (EE_UNITS_CAMO_*)
    EECamouflageNote theNote;
    theNote.mCount = 0;
    bool theNoting = EESimCaptureRecord::Active()
                     && ((inUnit->mCamouflageTimer & 0x7fffffff) > 0 || inUnit->mU32C > 0);
    theNote.Add((uint32_t)inUnit->mCamouflageTimer);
    theNote.Add((uint32_t)inUnit->mU32C);
    theNote.Add(inUnit->mPlayer->mNumVitalBuildings < 1 ? 1u : 0u);
    theNote.Add((uint32_t)gPhysics->mTickTimeMs);
    // New (R5b.4 leaf 6): the timers' decision is Rust over the unit store (ee_units step.rs camouflage); its events are
    // applied here in order (a mini map move only when the unit is then a cloaked enemy of the local player, the
    // original's test). TEST ONLY: while the capture runs, USTM records them
    EeUnitsCamoEvent theEvents[EE_UNITS_CAMO_MAX_EVENTS];
    bool theNoVital = inUnit->mPlayer->mNumVitalBuildings < 1;
    unsigned long theTimerBefore = inUnit->mCamouflageTimer, theSecondBefore = inUnit->mU32C;
    uint32_t theCount = ee_units_step_camouflage(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit),
                                                 (uint32_t)theTimerBefore, (uint32_t)theSecondBefore, theNoVital ? 1 : 0,
                                                 gPhysics->mTickTimeMs, theEvents);
    for (uint32_t i = 0; i < theCount; i++) {
        const EeUnitsCamoEvent& theEvent = theEvents[i];
        switch (theEvent.code) {
        case EE_UNITS_CAMO_SHOW:
        case EE_UNITS_CAMO_HIDE: {
            bool theMoved = EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer());
            if (theMoved && theEvent.code == EE_UNITS_CAMO_SHOW)
                gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
            else if (theMoved)
                gMiniMap->MoveUnit(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), -10000, -10000);
            theNote.Add(theEvent.code | (theMoved ? 0x100u : 0u));
            break;
        }
        case EE_UNITS_CAMO_CLOAKABLE_OFF:
        case EE_UNITS_CAMO_CLOAKABLE_ON:
            inUnit->SetCloakableChain(theEvent.code == EE_UNITS_CAMO_CLOAKABLE_ON);
            theNote.Add(theEvent.code);
            break;
        case EE_UNITS_CAMO_CLOAKED_OFF:
        case EE_UNITS_CAMO_CLOAKED_ON:
            inUnit->SetCloakedChain(theEvent.code == EE_UNITS_CAMO_CLOAKED_ON);
            theNote.Add(theEvent.code);
            break;
        case EE_UNITS_CAMO_CAMOUFLAGED_OFF:
        case EE_UNITS_CAMO_CAMOUFLAGED_ON:
            inUnit->SetCamouflaged(theEvent.code == EE_UNITS_CAMO_CAMOUFLAGED_ON);
            theNote.Add(theEvent.code);
            break;
        case EE_UNITS_CAMO_SET_TIMER:
            inUnit->mCamouflageTimer = theEvent.value;
            theNote.Add(theEvent.code);
            theNote.Add(theEvent.value);
            break;
        case EE_UNITS_CAMO_SET_SECOND:
            inUnit->mU32C = theEvent.value;
            theNote.Add(theEvent.code);
            theNote.Add(theEvent.value);
            break;
        }
    }
    if (theNoting) {   // TEST ONLY (USTM)
        theNote.Add((uint32_t)inUnit->mCamouflageTimer);
        theNote.Add((uint32_t)inUnit->mU32C);
        EEUnitStepNote(kUnitStepCamouflage, inUnit->mUnitID, theNote.mWords, theNote.mCount);
    }
    if (!inUnit->mIsCloakable)
        return;
    this->UpdateCloakDetection(inUnit);
    float theOpacity = inUnit->mOpacity;
    float theOpacityBefore = theOpacity;   // TEST ONLY (USTM)
    bool theCloaked = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
    bool thePulse = inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitol && inUnit->mStats->mPower > 0 &&
                    !EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()) && inUnit->mCloaked != 0;
    // New (R5b.4 leaf 6): the fade is Rust (ee_units step.rs cloak_fade: the same f32 operations in order)
    theOpacity = ee_units_step_cloak_fade(theOpacityBefore, theCloaked ? 1 : 0, gPhysics->GetTickTime(), thePulse ? 1 : 0,
                                          gPhysics->mGameTime);
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        uint32_t theWords[6] = {CloakBits(theOpacityBefore), theCloaked ? 1u : 0u, CloakBits(gPhysics->GetTickTime()),
                                thePulse ? 1u : 0u, (uint32_t)gPhysics->mGameTime, CloakBits(theOpacity)};
        EEUnitStepNote(kUnitStepCloakFade, inUnit->mUnitID, theWords, 6);
    }
    inUnit->SetOpacityChain(theOpacity);
}

// Sets or clears the unit's camouflaged flag (the top bit of its camouflage timer)
void EEComplexUnit::SetCamouflaged(bool inValue)
{
    if (inValue == true)
        this->mCamouflageTimer |= 0x80000000;
    else
        this->mCamouflageTimer &= 0x7fffffff;
}

// Restores a unit's power by its regeneration rate every 5 seconds, up to its maximum (not during cinematics)
void EEUnitClass::RegeneratePower(EEComplexUnit* inUnit)
{
    uint32_t theWords[13] = {(uint32_t)inUnit->mStats->mPowerRegeneration, (uint32_t)inUnit->mStats->mPower,
                             ::gWorld.mInCinematic ? 1u : 0u, this->IsLandBuilding() ? 1u : 0u, inUnit->mIsBuilt ? 1u : 0u,
                             (uint32_t)inUnit->mPower, (uint32_t)inUnit->mPowerRegenTimer,
                             (uint32_t)gPhysics->mTickTimeMs, (uint32_t)EEGetRules().mCombat.mPowerRegenerationInterval,
                             0, 0, 0, 0};
    // New (R5b.4 leaf 6): the regeneration is Rust over the unit store (ee_units step.rs regenerate_power); the
    // writes are the holders'. TEST ONLY: while the capture runs, USTM records it
    EeUnitsPowerIn theIn;
    theIn.regeneration = inUnit->mStats->mPowerRegeneration;
    theIn.max_power = inUnit->mStats->mPower;
    theIn.in_cinematic = ::gWorld.mInCinematic ? 1 : 0;
    theIn.interval = EEGetRules().mCombat.mPowerRegenerationInterval;
    theIn.tick_ms = gPhysics->mTickTimeMs;
    EeUnitsPowerOut theOut;
    ee_units_step_power(static_cast<uint32_t>(inUnit->mUnitID), reinterpret_cast<uintptr_t>(inUnit), inUnit->mPower,
                        inUnit->mPowerRegenTimer, inUnit->mIsBuilt ? 1 : 0, this->IsLandBuilding() ? 1 : 0, &theIn,
                        &theOut);
    if (theOut.write) {
        if (theOut.set_power)
            inUnit->SetPower((long)theOut.power);
        inUnit->SetPowerRegenTimer((long)theOut.regen_timer);
    }
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (USTM)
        theWords[9] = theOut.write;
        theWords[10] = theOut.set_power;
        theWords[11] = theWords[10] ? (uint32_t)inUnit->mPower : 0u;
        theWords[12] = theWords[9] ? (uint32_t)inUnit->mPowerRegenTimer : 0u;
        EEUnitStepNote(kUnitStepPower, inUnit->mUnitID, theWords, 13);
    }
}

extern GEViewport gViewport;
extern bool& gOrthographicView;

// Updates the volume, pan and pitch of a unit's animation sound every third tick (off-screen every 21st)
void EEUnitClass::RefreshSound(EEComplexUnit* inUnit)
{
    SESMInstanceNode* theInstance = inUnit->mSound;
    if (theInstance == NULL || UIsNone32(gSoundManager->GetInstanceID(theInstance)))
        return;
    unsigned long theTick = gPhysics->mNumTicksDone;
    if (inUnit->mUnitID % 3 != theTick % 3)
        return;
    if (inUnit->mUnitID % 0x15 != theTick % 0x15 &&
        !inUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView))
        return;
    unsigned long theSample = gSoundManager->GetSampleID(theInstance);
    EESoundObject* theSound = inUnit->GetModel()->mSounds1[inUnit->GetAnimation()];
    if (theSound == NULL || theSample != theSound->mSampleID) {
        theSound = inUnit->GetModel()->mSounds2[inUnit->mAnimation];
        if (theSound == NULL || theSample != theSound->mSampleID)
            theSound = NULL;
    }
    if (theSound == NULL)
        return;
    float theVolume, thePan, thePitch;
    this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
    gSoundManager->SetSoundParameters(theInstance, theVolume, thePan, thePitch);
}

// Heals the living units garrisoned in a living unit by its healing points each second
void EEUnitClass::HealGarrison(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    if (theStats->mHealingPoints == 0 || inUnit->GetNumCargoUnits() == 0 || ::gWorld.mInCinematic)
        return;
    EeCombatTimerIn theTimer = {(int32_t)inUnit->mRegenerationTimer, (int32_t)gPhysics->mTickTimeMs,
                                (int32_t)EEGetRules().mCombat.mRegenerationInterval};
    EeCombatTimerOut theTimerOut = EECombatTimer(theTimer);
    bool theHealed = false;
    if (theTimerOut.due) {
        if (inUnit->GetHitPoints() > 0) {
            unsigned long theCount = inUnit->GetNumCargoUnits();
            for (unsigned long i = 0; i < theCount; i++) {
                EEComplexUnit* theUnit = inUnit->GetCargo(i);
                EeCombatHealIn theHeal = {(int32_t)theUnit->GetHitPoints(), (int32_t)theStats->mHealingPoints,
                                          (int32_t)theUnit->GetStats()->mMaxHitPoints};
                EeCombatHealOut theHealOut = EECombatHeal(theHeal);
                if (theHealOut.set) {
                    theUnit->SetHitPoints(theHealOut.hit_points);
                    theHealed = true;
                }
            }
        }
        if (theHealed && inUnit->GetPlayer() == ::gWorld.GetCurrentPlayer() &&
            inUnit->GetPlayer()->mActiveGroup->Contains(inUnit))
            gUIManager->GetMainGameForm()->mGroups[0]->Reset();
    }
    inUnit->SetRegenerationTimer(theTimerOut.timer);
}

// Regenerates a living unit's hit points each second up to its maximum (not during cinematics)
void EEUnitClass::Regenerate(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    if (::gWorld.mInCinematic)
        return;
    EeCombatRegenerateIn theIn = {(int32_t)inUnit->mRegenerationTimer, (int32_t)gPhysics->mTickTimeMs,
                                  (int32_t)inUnit->mHitPoints, (int32_t)theStats->mRegeneration,
                                  (int32_t)theStats->mMaxHitPoints, (int32_t)EEGetRules().mCombat.mRegenerationInterval};
    EeCombatRegenerateOut theOut = EECombatRegenerate(theIn);
    if (theOut.set) {
        inUnit->SetHitPoints(theOut.hit_points);
        if (inUnit->mPlayer == ::gWorld.GetCurrentPlayer() &&
            inUnit->mPlayer->mActiveGroup->Contains(inUnit))
            gUIManager->GetMainGameForm()->mGroups[0]->Reset();
    }
    inUnit->SetRegenerationTimer(theOut.timer);
}

void EEUnitClass::UpdateDone(class EEComplexUnit *)
{
}

// Runs the default handling of the unit's new AI-script action (animation, goals, herd, flight, healing, death,
// rotting), then restarts the current goal's timer
void EEUnitClass::HandleActionChanged(EEComplexUnit* inUnit)
{
    EEUnitClassActionChanged(EE_UNITS_ACTION_BASE, this, inUnit);
}

bool EEUnitClass::Virtual4C(unsigned long)
{
    return false;
}

void EEUnitClass::Virtual50(unsigned long)
{
}
