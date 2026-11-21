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
#include "lle/LowLevelEngine.h"
#include "ee/EEUCUpgradeTable.h"
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
#include "ee/EEGlobalStorage.h"
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
// (constructed here, its storage EEGlobals.cpp's: ee/EEGlobalStorage.h)
EE_CONSTRUCT_GLOBAL_AS(std::vector<EEUnitClass*>, gUnitClassTable, gUnitClassTable, (EEUnitClassCount()));
// (gSpaceTurretHeight, gSpaceStationHeight: EEGlobals.cpp's)

// 0x564448: the unit types registered in gUnitClassTable (Register 0x236a9c push_backs; the vector right after the
// table, guarded dtor bit 1 of 0x563698: a class static).
EE_CONSTRUCT_GLOBAL_AS(std::vector<unsigned long>, EEUnitClassUnitTypes, EEUnitClass::sUnitTypes, );

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
    return ee_units_uc_produce(this, inUnit, inClass, inArg);   // uc_produce.rs
}

// CreateUnitAtTile's and CreateUnitAtPoint's body: a unit of the class for a player at a map point (its tile the
// simple unit's and the levelled ground's) with a heading, grounded when a map is given, queued in the player's
// pending units
static EEUnit* EECreateUnitAt(EEUnitClass* inClass, EEPlayer* inPlayer, const GE3DPoint& thePoint, unsigned long theX,
                              unsigned long theY, EEMap* inMap, float inHeading)
{
    GETransformation theTransformation;
    GE3DPoint theGround;
    float thePitch = 0.0f;
    float theRoll = 0.0f;
    float theZ = 0.0f;
    EEUnit* theUnit;
    if (inClass->mIsComplex == 0 && inClass->mUnitType != kUnitTypePathPoint) {
        theUnit = EEUnit::Create(inClass, theX, theY, inPlayer);
        if (inMap != NULL)
            inClass->SimpleUnitCreated(theUnit);
    } else {
        theTransformation.SetX(thePoint.GetX());
        theTransformation.SetY(thePoint.GetY());
        theTransformation.SetZ(thePoint.GetZ());
        if (inMap != NULL) {
            inMap->GetGroundPoint(thePoint, theGround);
            inClass->GroundLocation(theGround);
            theTransformation.SetX(theGround.GetX());
            theTransformation.SetY(theGround.GetY());
            theTransformation.SetZ(theGround.GetZ());
            if (inClass->mCategory != kUnitCategoryBuilding && inClass->mCategory != kUnitCategoryMiningBuilding && inClass->mCategory != kUnitCategoryGate && inClass->mCategory != kUnitCategoryFortress) {
                theTransformation.SetOrientationYPR(inHeading, 0.0f, 0.0f);
                if (inClass->mPitchesWithTerrain)
                    thePitch = inMap->GetTerrainPitch(theTransformation);
                if (inClass->mRollsWithTerrain)
                    theRoll = inMap->GetTerrainRoll(theTransformation);
                theTransformation.SetOrientationYPR(inHeading, thePitch, theRoll);
            }
        }
        unsigned long theGoalCount = inClass->GetNumGoalQueues();
        if (inMap != NULL) {
            theZ = theTransformation.GetZ();
            inClass->LevelGround(&inMap->mTerrainMesh, theX, theY, &theZ);
            theTransformation.SetZ(theZ);
        }
        EEComplexUnit* theComplexUnit = EEComplexUnit::Create(inClass, theTransformation, inPlayer, theGoalCount);
        theUnit = theComplexUnit;
        if (inMap != NULL)
            inClass->UnitCreated(theComplexUnit);
    }
    {
        RTSUnit* theEntry = theUnit;
        inPlayer->mPendingUnits.push_back(theEntry);
        theEntry->Reference();
    }
    ++inClass->mNumUnits;
    theUnit->Dereference();
    return theUnit;
}

// Creates a unit of the class for a player at a tile's centre with a heading, grounded when a map is given, and
// queues it in the player's pending units
EEUnit* EEUnitClass::CreateUnitAtTile(EEPlayer* inPlayer, long inX, long inY, EEMap* inMap, float inHeading)
{
    return EECreateUnitAt(this, inPlayer, GE3DPoint((float)(unsigned long)inX + 0.5f, (float)(unsigned long)inY + 0.5f, 0.0f),
                          inX, inY, inMap, inHeading);
}

// Creates a unit of the class for a player at a map point with a heading, grounded when a map is given, and queues it
// in the player's pending units
EEUnit* EEUnitClass::CreateUnitAtPoint(EEPlayer* inPlayer, float inX, float inY, EEMap* inMap, float inHeading)
{
    return EECreateUnitAt(this, inPlayer, GE3DPoint(inX, inY, 0.0f), UMFloorUnsigned(inX), UMFloorUnsigned(inY), inMap, inHeading);
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
                ee_units_uu_block_takes((uint64_t)(uintptr_t)inPlayer, theType, theX, theY, theSize, theSize, 0)) {
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
    ee_units_lv_create_runways(EEObj(this), EEObj(inUnit), inPlayer->GetPlayerID(), inIsFoundation);   // cls_level.rs
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
    if (inUnit->mPlayer->mComputerPlayer == NULL || !ee_units_uu_escape_point((uint64_t)(uintptr_t)inUnit, (float(*)[2])&theLocation.mX))
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

// Plays the class's ripple effect at the model's ripple tag points that are under water, when the player sees the unit
void EEUnitClass::CreateRipples(EEComplexUnit* inUnit)
{
    GE3DPoint theLocation;
    GETransformation theEffect;
    GETransformation theModel(false);
    EETagPointList* theTags = inUnit->GetModel()->GetTagPoints(inUnit->GetAnimation(), 0x22);
    if (theTags == NULL || theTags->mPoints.size() == 0 ||
        !EECombatIsSeenBy(inUnit, ::gWorld.GetCurrentPlayer(), true))
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
    ee_units_uu_block_ring(inCorners, inUnit->GetTileX(), inUnit->GetTileY(), this->GetTileSize(), this->GetTileSize(), &outTiles, inAppend, 1);
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
    if ((unsigned long)ee_units_uu_units_near((const float(*)[3])&thePosition.mX, theRange, &theUnits) > 0) {
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

// IsImportantUnit reads the heal goal (included here, after the unit's init chains: its class static would renumber them)

// Heals the heal goal's target when in range: plays the healing animation and the class's heal effect
// The heal effect is drawn between the two units or on the target, by the graphic effect's kind
void EEUnitClass::Heal(EEComplexUnit* inUnit)
{
    ee_units_uc_body(EE_UNITS_UC_DO_HEAL, this, inUnit);   // uc_body.rs
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
    ee_units_mv_path_toward(EEObj(inUnit), inFrom.mX, inFrom.mY, inFrom.mZ, inTo.mX, inTo.mY, inTo.mZ,
                            inSnapToMap);   // ee_units cls_move.rs
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
#include "rust/sim/astar.h"   // the path goals' modifiers (modifier_obj.rs)
#include "rust/sim/units.h"
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

// The movement bodies' world (ee_units cls_move_host.rs EE_UNITS_CLS_MV_*: the class bodies' host switch passes them
// here, where the path manager's and the screens' headers are)
int64_t EEUnitClassMoveOp(int32_t inOp, EEComplexUnit* inUnit, uint64_t inB, int64_t inC, int64_t inD, EeUnitsClsIo* io)
{
    EEUnitGoal* theGoal = (EEUnitGoal*)(uintptr_t)inB;
    EEComplexUnit* theOther = (EEComplexUnit*)(uintptr_t)inB;
    GE3DPoint thePoint(io->p[0], io->p[1], io->p[2]);
    switch (inOp) {
    case EE_UNITS_CLS_MV_CLOSEST_TILE_HEIGHT: {
        long theX, theY;
        EEMapTile* theTile = ::gWorld.mMap->GetClosestTile(&thePoint, &theX, &theY);
        if (theTile != NULL)
            io->f = theTile->mHeight;
        return theTile != NULL;
    }
    case EE_UNITS_CLS_MV_PURSUER: return EEObj(EEGetPursuerOf(inUnit));
    case EE_UNITS_CLS_MV_DIRECTION_TO:
        return ee_units_uu_tile_direction(inUnit->GetTileX(), inUnit->GetTileY(), theOther->GetTileX(), theOther->GetTileY());
    case EE_UNITS_CLS_MV_MAX_VELOCITY: io->p[0] = inC != 0 ? inUnit->GetMaxAngularVelocity() : inUnit->GetMaxVelocity(); return 0;
    case EE_UNITS_CLS_MV_GOAL: {
        const void* theMembers[4] = {inUnit->mGoalState, inUnit->mActiveGoal, inUnit->mBlockingUnit, inUnit->mLastAttacker};
        return EEObj(theMembers[inC]);
    }
    case EE_UNITS_CLS_MV_CLEAR_LAST_ATTACKER:
        if (inUnit->mLastAttacker != NULL)
            inUnit->mLastAttacker->Dereference();
        inUnit->mLastAttacker = NULL;
        return 0;
    case EE_UNITS_CLS_MV_GOAL_KIND: return theGoal->mKind;
    case EE_UNITS_CLS_MV_GOAL_LOCATION:
        if (inC != 0)
            theGoal->GetDestination(inUnit, thePoint);
        else
            theGoal->GetGoalLocation(thePoint);
        io->p[0] = thePoint.mX; io->p[1] = thePoint.mY; io->p[2] = thePoint.mZ;
        return 0;
    case EE_UNITS_CLS_MV_GOAL_FLAG: {
        if (inC < 4)
            return inC == 0 ? theGoal->mB1C : inC == 1 ? theGoal->mB1D : inC == 2 ? theGoal->GetIsAutoWaypoint() : theGoal->GetGroupPathState();
        EEUGRust* theGather = EEGatherGoalCast(theGoal);
        return theGather != NULL && theGather->GetGatherResource() != NULL && theGather->GetGatherResource()->mResourceType == kResourceWood;
    }
    case EE_UNITS_CLS_MV_PATH_ATTEMPTS: theGoal->mPathAttempts += (long)inC; return 0;
    case EE_UNITS_CLS_MV_SIDESTEP:
        if (inC < 0)
            return theGoal->GetSidestepCount();
        theGoal->SetSidestepCount((unsigned long)inC);
        return 0;
    case EE_UNITS_CLS_MV_SET_GROUP_PATH_STATE: theGoal->SetGroupPathState((unsigned char)inC); return 0;
    case EE_UNITS_CLS_MV_ATTACK_TARGET: return EEObj(theGoal->GetAttackTarget()->GetComplexUnit());
    case EE_UNITS_CLS_MV_IGNORE_TARGET: inUnit->IgnoreTarget(theOther); return 0;
    case EE_UNITS_CLS_MV_MELEE_RETARGET: {
        EETileCoordinate theTile = inUnit->GetTile();
        EEFOFindNewTargetInMeleeRange theScreen(inUnit, theOther);
        return EEFindNewTarget(&theScreen, (EEUnitGoal*)(uintptr_t)inC, inUnit, &theTile, 1,
                               theOther != NULL ? theOther->mUnitClass->mIsWall : false, 1);
    }
    case EE_UNITS_CLS_MV_SET_BLOCKING: inUnit->SetBlockingUnit((RTSUnit*)(uintptr_t)inB); return 0;
    case EE_UNITS_CLS_MV_ADD_WAYPOINT: inUnit->AddWaypoint(thePoint, inC != 0); return 0;
    case EE_UNITS_CLS_MV_WAYPOINT:
        thePoint = inUnit->mWaypoints[(size_t)inC].mLocation;
        io->p[0] = thePoint.mX; io->p[1] = thePoint.mY; io->p[2] = thePoint.mZ;
        return 0;
    case EE_UNITS_CLS_MV_SECOND_WAYPOINT: {
        long theAnswer = inUnit->GetSecondWaypoint(thePoint);
        io->p[0] = thePoint.mX; io->p[1] = thePoint.mY; io->p[2] = thePoint.mZ;
        return theAnswer;
    }
    case EE_UNITS_CLS_MV_NEXT_WAYPOINT: inUnit->NextWaypoint(); return 0;
    case EE_UNITS_CLS_MV_DISTANCE_3D: io->f = inUnit->DistanceSquared3D(thePoint); return 0;
    case EE_UNITS_CLS_MV_BOUNDED:
        if (inC == 0)
            inUnit->mBoundedPathTiles.clear();
        else
            inUnit->mBoundedPathTiles.push_back(U2DPoint<long>((long)io->a, (long)io->b));
        return 0;
    case EE_UNITS_CLS_MV_FIND_BOUNDED_PATH:
    case EE_UNITS_CLS_MV_MERGE_WAYPOINTS: {
        EeUnitsClsListHandle* theList = (EeUnitsClsListHandle*)io->list;
        std::vector<GE3DPoint> thePath;
        bool theReached = false;
        if (inOp == EE_UNITS_CLS_MV_FIND_BOUNDED_PATH) {
            gPathManager.FindBoundedPath(inUnit, gWorld.mMap, (long)inC, inUnit->GetTile(),
                                         EETileCoordinate((long)io->a, (long)io->b), thePath, 50, true, &theReached);
        } else {
            size_t theCount = 0;
            const float* theXYZ = ee_units_cls_points(theList, &theCount);
            for (size_t i = 0; i < theCount; i++)
                thePath.push_back(GE3DPoint(theXYZ[3 * i], theXYZ[3 * i + 1], theXYZ[3 * i + 2]));
            EEPathManager::MergeWaypoints(thePath, inUnit->mWaypoints, 0);
            ee_units_cls_clear_points(theList);
        }
        for (size_t i = 0; i < thePath.size(); i++)
            ee_units_cls_push_point(theList, thePath[i].mX, thePath[i].mY, thePath[i].mZ);
        return 0;
    }
    case EE_UNITS_CLS_MV_SET_VELOCITY: inUnit->SetVelocity(io->f); return 0;
    case EE_UNITS_CLS_MV_START_ACTION: inUnit->StartAction((long)inC); return 0;
    case EE_UNITS_CLS_MV_ACTION_CHANGED: ((EEUnitClass*)(uintptr_t)inB)->ActionChanged(inUnit); return 0;
    case EE_UNITS_CLS_MV_UPDATE_WATER_STATE: inUnit->UpdateWaterState(true); return 0;
    case EE_UNITS_CLS_MV_CANCEL_PATHS: gPathManager.CancelPaths(inUnit, true); return 0;
    case EE_UNITS_CLS_MV_INITIAL_CONTACT:
        if (inUnit->mInitialContactLocation != thePoint)
            inUnit->mInitialContactLocation = thePoint;
        return 0;
    case EE_UNITS_CLS_MV_REQUEST_PATH: {
        EeAStarModifier* theModifier = NULL;
        if (inUnit->mPlayer->AvoidsEnemies(inUnit))
            theModifier = ee_astar_modifier_new_enemy_avoidance((uint32_t)inUnit->mPlayer->GetPlayerID());
        theGoal->mPathAttempts++;
        gPathManager.RequestUnitPath(inUnit, inUnit->GetTile(), EETileCoordinate((long)io->a, (long)io->b), false,
                                     (long)inC, (inD & 1) != 0, theModifier, (inD & 2) != 0);
        ee_astar_modifier_release(theModifier);
        return 0;
    }
    case EE_UNITS_CLS_CR_TAG: {   // the carrier inUnit's tag point inC at the plane's frame (cls_carrier.rs)
        EETagPointList* theTags = inUnit->GetModel()->mTagPoints[inUnit->mAnimation];
        if (theTags == NULL || theTags->mPoints.size() == 0)
            return 0;
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        EETagPointList::Scale(theTransformation, inUnit->GetStats()->GetScale());
        theTags->mPoints[(size_t)inC]->GetPoint(theOther->mPhysicalModel.GetFrame(), theTransformation, thePoint);
        io->p[0] = thePoint.mX; io->p[1] = thePoint.mY; io->p[2] = thePoint.mZ;
        return 1;
    }
    case EE_UNITS_CLS_CR_SET_ORIENTATION: inUnit->mOrientation = GEOrientation(io->p[0], io->p[1], io->p[2]); return 0;
    case EE_UNITS_CLS_CR_SET_POSITION: {
        GETransformation theTransformation(inUnit->mPhysicalModel.GetLocation());
        theTransformation.SetPosition(thePoint);
        inUnit->SetTransformation(theTransformation, true);
        return 0;
    }
    case EE_UNITS_CLS_CR_SET_SCALE: inUnit->mPhysicalModel.SetScale(io->f); return 0;
    case EE_UNITS_CLS_CR_ROUTE_DOCK: ((EEUnitClass*)(uintptr_t)inB)->RouteDockWaypoint(inUnit); return 0;
    case EE_UNITS_CLS_CR_HAS_AIR_TRAFFIC: return inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1] != 0;
    case EE_UNITS_CLS_CR_DECK_GOAL: {
        EEUnitGoal* theLocation = NULL;   // a location goal or a heal goal
        if (inUnit->mGoalState != NULL && EEGoalHasLocation(inUnit->GetGoal()))
            theLocation = inUnit->GetGoal();
        if (theLocation == NULL) {
            EEUGRust* theMove = EEUGRust::NewLocation(&thePoint, false, true, false, 0.0f, false, false);
            inUnit->ClearGoalQueue(0);
            inUnit->SetGoal(theMove, 0, 0);
        } else
            EESetLocationGoalPoint(theLocation, thePoint);
        return 0;
    }
    case EE_UNITS_CLS_CR_LOCATION_GOAL_POINT:
        thePoint = EELocationGoalPoint(inUnit->GetGoal());
        io->p[0] = thePoint.mX; io->p[1] = thePoint.mY; io->p[2] = thePoint.mZ;
        return 0;
    case EE_UNITS_CLS_CR_HOME_FIRST_SLOT: {
        EeUnitsAtc* theControl = inUnit->mPlayer->mAirTraffic[inUnit->mUnitClass->mAirTrafficType - 1];
        if (ee_units_atc_slot_count(theControl) > 0)
            inUnit->SetHomeBase(static_cast<EEComplexUnit*>(ee_units_atc_slot_airport(theControl, 0)));
        else
            inUnit->Kill(true, false, false);
        return EEObj(inUnit->mHomeBase);
    }
    case EE_UNITS_CLS_CR_RANDOM: io->f = gPhysics->mRandom.Generate(); return 0;
    case EE_UNITS_CLS_CR_HEIGHT_AT: io->f = ((EEUnitClass*)(uintptr_t)inB)->GetHeightAt(inUnit, thePoint); return 0;
    case EE_UNITS_CLS_CR_CAN_GET_TO: return gContinentManager.CanGetTo(inUnit, (long)inC, (long)inD);
    // the ground levelling's (cls_level.rs): the mesh and the tile are the handle a
    case EE_UNITS_CLS_LV_MESH_Z: io->f = ((GETerrainMesh*)(uintptr_t)inUnit)->GetZ((unsigned long)inC, (unsigned long)inD); return 0;
    case EE_UNITS_CLS_LV_SET_MESH_Z: ((GETerrainMesh*)(uintptr_t)inUnit)->SetZ((unsigned long)inC, (unsigned long)inD, io->f); return 0;
    case EE_UNITS_CLS_LV_SET_TILE_HEIGHT:
        if (inUnit != NULL)
            ((EEMapTile*)(uintptr_t)inUnit)->mHeight = io->f;
        return 0;
    case EE_UNITS_CLS_LV_MAP_MESH: return EEObj(&::gWorld.mMap->mTerrainMesh);
    case EE_UNITS_CLS_LV_CREATE_UNIT:
        return EEObj(((EEUnitClass*)(uintptr_t)inB)->CreateUnit((long)inC, (long)inD, (long)io->a, io->b != 0, inUnit)->mComplexUnit);
    case EE_UNITS_CLS_LV_LINK_PART: inUnit->LinkPart(theOther); return 0;
    case EE_UNITS_CLS_LV_SET_FACING: inUnit->SetFacing((unsigned long)inC); return 0;
    case EE_UNITS_CLS_LV_CAMERA_OFF: gCameraFollowing = false; return 0;
    // the status ticks' (cls_status.rs)
    case EE_UNITS_CLS_ST_CINEMATIC_INTERVAL:
        io->a = ::gWorld.mInCinematic ? 1 : 0;
        io->b = EEGetRules().mCombat.mPowerRegenerationInterval;
        return 0;
    case EE_UNITS_CLS_ST_CLOAKED_ENEMY: return ee_units_uu_cloaked_enemy((uint64_t)(uintptr_t)inUnit, (uint64_t)(uintptr_t)(::gWorld.GetCurrentPlayer()));
    case EE_UNITS_CLS_ST_MINI_MAP:
        if (inC != 0)
            gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
        else
            gMiniMap->MoveUnit(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), -10000, -10000);
        return 0;
    case EE_UNITS_CLS_ST_CHAIN:
        if (inC == 0)
            inUnit->SetCloakableChain(inD != 0);
        else if (inC == 1)
            inUnit->SetCloakedChain(inD != 0);
        else
            inUnit->SetOpacityChain(io->f);
        return 0;
    case EE_UNITS_CLS_ST_CLOAK_DETECTION: ((EEUnitClass*)(uintptr_t)inB)->UpdateCloakDetection(inUnit); return 0;
    case EE_UNITS_CLS_ST_AREA_CLOAK: {
        if (!inUnit->HasEffect(8))
            return 0;
        long theInput = 0;
        long theRadius = 0;
        inUnit->mAffectingAreaEffects.ApplyModifiers(3, theInput, theRadius, inUnit);
        io->a = theRadius;
        return 1;
    }
    case EE_UNITS_CLS_ST_CAMOUFLAGE_TIMING: io->f = gWorldTable.GetFloatValue(kWorldCamouflageTiming); return 0;
    case EE_UNITS_CLS_MV_FLEE_GOAL:
        inUnit->SetGoal(EEUGRust::NewLocation(GE2DPoint(io->p[0], io->p[1]), true, true, false, 0.0f, false, 0), inC != 0, 0);
        return 0;
    }
    return 0;
}

// Creates a target screen that accepts units closer to the attacker than its current target
EEFOFindNewTargetInMeleeRange::EEFOFindNewTargetInMeleeRange(EEComplexUnit* inUnit, EEUnit* inTarget)
    : mUnit(inUnit), mTarget(inTarget)
{
    this->mRange = EECombatHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
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
    return EECombatHitDistanceSquared(theContext->mUnit, theContext->mCandidate, -10000.0f, -10000.0f);
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

// Creates the class's remains unit for the dead unit's player at a free spot near where it died
void EEUnitClass::CreateRemainsUnit(EEComplexUnit* inUnit)
{
    GETransformation theTransformation(false);
    theTransformation = inUnit->mPhysicalModel.GetLocation();
    EEUnitClass* theClass = this->mRemainsClass;
    if (theClass == NULL)
        return;
    GE3DPoint thePoint(theTransformation.GetX(), theTransformation.GetY(), theTransformation.GetZ());
    if (!ee_units_uu_find_placement((const float(*)[3])&thePoint.mX, 0, (uint64_t)(uintptr_t)theClass, inUnit->GetTileX(), inUnit->GetTileY(), this->GetTileSize(), 1, !inUnit->mPlayer->IsHuman(), false, false, false, (float(*)[4][4])&theTransformation.mMatrix[0][0], (uint64_t)(uintptr_t)inUnit))
        return;
    if (!(theTransformation.GetZ() > 0.0f))
        theTransformation.SetZ(0.3f);
    theClass->CreateUnitAt(theTransformation, inUnit->mPlayer->GetPlayerID(), false, NULL);
}

// Spawns the class's death unit where the unit (or its container) was and launches it at that ground point
void EEUnitClass::CreateDeathUnit(EEComplexUnit* inUnit)
{
    ee_units_uc_body(EE_UNITS_UC_DO_CREATE_DEATH_UNIT, this, inUnit);   // uc_body.rs
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
    ee_units_uc_body(EE_UNITS_UC_DO_UPDATE_BUILDING, this, inUnit);   // uc_body.rs
}

// Ticks an animal: physics, effects, timers and animation; fires on attack frames and lets its carcass rot
void EEUnitClass::UpdateAnimal(EEComplexUnit* inUnit)
{
    ee_units_uc_body(EE_UNITS_UC_DO_UPDATE_ANIMAL, this, inUnit);   // uc_body.rs
}

// Sets or clears the unit's camouflaged flag (the top bit of its camouflage timer)
void EEComplexUnit::SetCamouflaged(bool inValue)
{
    if (inValue == true)
        this->mCamouflageTimer |= 0x80000000;
    else
        this->mCamouflageTimer &= 0x7fffffff;
}

extern GEViewport gViewport;
extern bool& gOrthographicView;

// Updates the volume, pan and pitch of a unit's animation sound every third tick (off-screen every 21st)
void EEUnitClass::RefreshSound(EEComplexUnit* inUnit)
{
    ee_units_uc_body(EE_UNITS_UC_DO_REFRESH_SOUND, this, inUnit);   // uc_body.rs
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

// a location's X, Y and Z into a body's floats
static void EEUcLocationOut(const GETransformation& inLocation, float* outFloats)
{
    outFloats[0] = inLocation.GetX();
    outFloats[1] = inLocation.GetY();
    outFloats[2] = inLocation.GetZ();
}

// ProduceUnit's tile line (EE_UNITS_UC_LINE_RESET, EE_UNITS_UC_TILE_LINE and its reads)
static std::vector<EETileCoordinate> sUcLine;

// a float's bits as a uc_produce.rs answer
static int64_t EEUcBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, sizeof(theBits));
    return theBits;
}

// ProduceUnit's ship and flier: outAdjusted = the unit's location at the placement's yaw (no pitch or roll), set
static void EEUcAlignYaw(EEComplexUnit* inUnit, GETransformation& inPlacement, GETransformation& outAdjusted)
{
    GEOrientation theOrientation;
    outAdjusted = inUnit->mPhysicalModel.GetLocation();
    inPlacement.ConvertToOrientation(theOrientation);
    outAdjusted.SetOrientationYPR(theOrientation.GetYaw(), 0.0f, 0.0f);
    inUnit->SetTransformation(outAdjusted, true);
}

// the goal points uc_produce.rs hands in place (three floats)
static_assert(sizeof(GE3DPoint) == 3 * sizeof(float), "a GE3DPoint is three floats");

// Heal's local effect handle (EE_UNITS_UC_RESET_HANDLE, the creations, EE_UNITS_UC_HANDLE)
static EEEffectHandle sUcHandle;

template <class T> static T* EEUcObject(uint64_t inHandle)
{
    return reinterpret_cast<T*>(static_cast<uintptr_t>(inHandle));
}

// The C++ of EEUnitClass's Rust bodies (ee_units uc_body.rs; EE_UNITS_UC_*): one statement of the original bodies
// each, in their order; inClass the class the body runs on, inUnit its unit, inA an object, inB and inC values, ioF
// floats in and out
extern "C" int64_t ee_units_uc_host(int32_t inOp, void* inClass, void* inUnit, uint64_t inA, int64_t inB, int64_t inC,
                                    float (*ioF)[4])
{
    EEUnitClass* theClass = static_cast<EEUnitClass*>(inClass);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    EEComplexUnit* theOther = EEUcObject<EEComplexUnit>(inA);
    float* f = *ioF;
    switch (inOp) {
    // Heal's
    case EE_UNITS_UC_HEAL_GOAL: return (uint64_t)(uintptr_t)EEHealGoalCast(theUnit->GetGoal());
    case EE_UNITS_UC_DROP_PATH: theUnit->DropPath(); return 0;
    case EE_UNITS_UC_HEAL_IN_RANGE: return EECombatHealInRange(theUnit, EEUcObject<EEUGRust>(inA)->GetHealTarget());
    case EE_UNITS_UC_SET_ANIMATION: theUnit->SetAnimation((long)inB, -1, 0); return 0;
    case EE_UNITS_UC_HEAL_EFFECT: return (uint64_t)(uintptr_t)theUnit->mUnitClass->mHealEffect;
    case EE_UNITS_UC_EFFECT_KIND: return static_cast<unsigned long>(EEUcObject<EEGraphicEffect>(inA)->mKind);
    case EE_UNITS_UC_HEAL_BETWEEN:
        return ::gWorld.GetEffectManager()->CreateEffectBetweenUnits(theUnit, EEUcObject<EEUGRust>((uint64_t)inB)->GetHealTarget(),
                                                                     EEUcObject<EEGraphicEffect>(inA), &sUcHandle) != 0;
    case EE_UNITS_UC_HEAL_ON_TARGET:
        return ::gWorld.GetEffectManager()->CreateUnitEffect(EEUcObject<EEUGRust>((uint64_t)inB)->GetHealTarget(),
                                                             EEUcObject<EEGraphicEffect>(inA), &sUcHandle, false, 0, 0) != 0;
    case EE_UNITS_UC_HEAL_HANDLE: return theUnit->mHealEffect.mValue;
    case EE_UNITS_UC_SET_HEAL_HANDLE: theUnit->mHealEffect.mValue = (uint32_t)inB; return 0;
    case EE_UNITS_UC_RESET_HANDLE: sUcHandle.mValue = 0; return 0;
    case EE_UNITS_UC_HANDLE: return sUcHandle.mValue;
    // ProduceUnit's (uc_produce.rs; inA a class, a goal or a unit)
    case EE_UNITS_UC_EXTRA_POPULATION: return theUnit->GetPlayer()->GetExtraPopulation((long)inB);
    case EE_UNITS_UC_POPULATION_LIMIT: return theUnit->GetPlayer()->GetPopulationLimit(0);
    case EE_UNITS_UC_TOTAL_POPULATION: return theUnit->GetPlayer()->GetTotalPopulation(true, false);
    case EE_UNITS_UC_CLASS_UNIT_TYPE: return EEUcObject<EEUnitClass>(inA)->GetUnitType();
    case EE_UNITS_UC_CLASS_POPULATION_COST: return EEUcObject<EEUnitClass>(inA)->GetPopulationCost();
    case EE_UNITS_UC_PRODUCER_TYPE: return theUnit->GetUnitClass()->GetUnitType();
    case EE_UNITS_UC_CLASS_CATEGORY: return EEUcObject<EEUnitClass>(inA)->GetCategory();
    case EE_UNITS_UC_CLASS_IS_AIRCRAFT: return EEUcObject<EEUnitClass>(inA)->IsAircraft();
    case EE_UNITS_UC_CLASS_AIR_TRAFFIC: return EEUcObject<EEUnitClass>(inA)->mAirTrafficType;
    case EE_UNITS_UC_ATC_CARGO: return ee_units_atc_cargo(theUnit->GetPlayer()->mAirTraffic[inB - 1], (int32_t)inC);
    case EE_UNITS_UC_ATC_FREE_WEIGHT: return ee_units_atc_free_cargo_weight(theUnit->GetPlayer()->mAirTraffic[inB - 1], theUnit);
    case EE_UNITS_UC_CLASS_WEIGHT: return EEUcObject<EEUnitClass>(inA)->mWeight;
    case EE_UNITS_UC_TRANSPORT_CAPACITY: return theUnit->GetUnitClass()->mTransportCapacity;
    case EE_UNITS_UC_CARGO_ROOM: return theUnit->mCargoRoom;
    case EE_UNITS_UC_CREATE_AT:
        return (uint64_t)(uintptr_t)EEUcObject<EEUnitClass>(inA)
            ->CreateUnitAt(*EEUcObject<GETransformation>((uint64_t)inB), theUnit->GetPlayer()->GetPlayerID(), false, theUnit)->mComplexUnit;
    case EE_UNITS_UC_LOAD_CARGO: theUnit->LoadCargo(theOther, true, 0); return 0;
    case EE_UNITS_UC_PARATROOPER_BOMBER: return theOther->GetUnitClass()->mIsParatrooperBomber;
    case EE_UNITS_UC_CARRIER_AIRCRAFT: EECreateCarrierAircraft(theOther, -1); return 0;
    case EE_UNITS_UC_CLASS_FAMILY: return EEUcObject<EEUnitClass>(inA)->mFamily;
    case EE_UNITS_UC_IS_HUMAN: return theUnit->GetPlayer()->IsHuman();
    case EE_UNITS_UC_QUEUED_GOAL: return (uint64_t)(uintptr_t)theUnit->GetQueuedGoal((unsigned long)inB);
    case EE_UNITS_UC_THIS_UNIT_TYPE: return theClass->mUnitType;
    case EE_UNITS_UC_GOAL_LOCATION: EEUcObject<EEUnitGoal>(inA)->GetGoalLocation(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_GOAL_KIND: return EEUcObject<EEUnitGoal>(inA)->mKind;
    case EE_UNITS_UC_ATTACK_TARGET: return (uint64_t)(uintptr_t)EEAttackUnitCast(EEUcObject<EEUnitGoal>(inA))->GetAttackTarget();
    case EE_UNITS_UC_COMPLEX_OF: return (uint64_t)(uintptr_t)EEUcObject<EEUnit>(inA)->GetComplexUnit();
    case EE_UNITS_UC_CONTAINER_OF: return (uint64_t)(uintptr_t)theOther->mContainer;
    case EE_UNITS_UC_POSITION: EEUcObject<EEUnit>(inA)->GetPosition(*reinterpret_cast<GE3DPoint*>(f)); return 0;
    case EE_UNITS_UC_CLASS_DOMAINS: return EEUcObject<EEUnitClass>(inA)->mDomains;
    case EE_UNITS_UC_TILE_SIZE: return theClass->GetTileSize();
    case EE_UNITS_UC_ALIGN_YAW:
        EEUcAlignYaw(theOther, *EEUcObject<GETransformation>((uint64_t)inB), *EEUcObject<GETransformation>((uint64_t)inC));
        return 0;
    case EE_UNITS_UC_CLASS_FLIES_OVER: return EEUcObject<EEUnitClass>(inA)->FliesOver();
    case EE_UNITS_UC_AREA_EFFECT_AT:
        return (uint64_t)(uintptr_t)gAEManager.FindEffectAtTile(8, theOther->GetTileX(), theOther->GetTileY(), theOther->GetPlayer()->GetPlayerID());
    case EE_UNITS_UC_EFFECT_OWNER: return (uint64_t)(uintptr_t)EEUcObject<EEAreaEffect>(inA)->mUnit;
    case EE_UNITS_UC_EFFECT_COVERS:
        return EEUcObject<EEAreaEffect>(inA)->Covers(EEUcObject<EEAreaEffect>(inA)->mUnit->GetPlayer()->GetPlayerID(),
                                                     EEUcObject<EEComplexUnit>((uint64_t)inB),
                                                     GE2DPoint(EEUcObject<EEAreaEffect>(inA)->mUnit->mPhysicalModel.GetLocation().GetX(),
                                                               EEUcObject<EEAreaEffect>(inA)->mUnit->mPhysicalModel.GetLocation().GetY()));
    case EE_UNITS_UC_CLOAKABLE_CHAIN: theOther->SetCloakableChain(true); return 0;
    case EE_UNITS_UC_CLEAR_GOAL_QUEUE: theOther->ClearGoalQueue(0); return 0;
    case EE_UNITS_UC_QUEUE_LEN: return theUnit->GetQueueGoals((unsigned long)inB).size();
    case EE_UNITS_UC_QUEUE_GOAL: return (uint64_t)(uintptr_t)theUnit->GetQueueGoals((unsigned long)inB)[(size_t)inC];
    case EE_UNITS_UC_GOAL_SUITS: return EEUcObject<EEUnitGoal>(inA)->Suits(EEUcObject<EEComplexUnit>((uint64_t)inB));
    case EE_UNITS_UC_ADD_BUILD_TARGET:
        theOther->AddBuildTarget(EEBuildRepairGoalCast(EEUcObject<EEUnitGoal>((uint64_t)inB))->GetBuildTarget());
        return 0;
    case EE_UNITS_UC_CAN_GARRISON: return theOther->GetUnitClass()->mCanGarrison;
    case EE_UNITS_UC_GOAL_CLONE: return (uint64_t)(uintptr_t)EEUcObject<EEUnitGoal>(inA)->Clone();
    case EE_UNITS_UC_GOAL_IS_LOAD: return EEUcObject<EEUnitGoal>(inA)->IsLoadGoal();
    case EE_UNITS_UC_GOAL_ADD_CREATED: EEUcObject<EEUnitGoal>(inA)->AddCreatedUnit(EEUcObject<EEComplexUnit>((uint64_t)inB)); return 0;
    case EE_UNITS_UC_LOAD_CONTAINER: return (uint64_t)(uintptr_t)EELoadGarrisonGoalCast(EEUcObject<EEUnitGoal>(inA))->GetLoadContainer();
    case EE_UNITS_UC_ACTIVE_GOAL: return (uint64_t)(uintptr_t)theOther->mActiveGoal;
    case EE_UNITS_UC_ADD_LOAD_UNIT:
        EELoadGarrisonGoalCast(EEUcObject<EEUnitGoal>(inA))->AddLoadUnit(EEUcObject<EEComplexUnit>((uint64_t)inB));
        return 0;
    case EE_UNITS_UC_NEW_LOCATION:
        return (uint64_t)(uintptr_t)EEUGRust::NewLocation(reinterpret_cast<GE3DPoint*>(f), false, true, false, 0.0f, false, 0);
    case EE_UNITS_UC_GOAL_SET_LOCATION: static_cast<EEUGRust*>(EEUcObject<EEUnitGoal>(inA))->SetLocation(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UC_LINE_RESET: sUcLine.clear(); return 0;
    case EE_UNITS_UC_TILE_LINE: EEGetTileLine(theOther->GetTileX(), theOther->GetTileY(), (long)inB, (long)inC, &sUcLine, 1, 1); return 0;
    case EE_UNITS_UC_LINE_LEN: return (long)sUcLine.size();
    case EE_UNITS_UC_LINE_X: return sUcLine[(size_t)inB].mX;
    case EE_UNITS_UC_LINE_Y: return sUcLine[(size_t)inB].mY;
    case EE_UNITS_UC_OUTER_SPHERE: return EEUcBits(theOther->GetUnitClass()->mOuterCollisionSphere);
    case EE_UNITS_UC_UNIT_CATEGORY: return theOther->GetUnitClass()->GetCategory();
    case EE_UNITS_UC_CAN_GATHER:
        return theOther->CanGather(EEUcObject<EEUnit>((uint64_t)inB), EEUcObject<EEUnit>((uint64_t)inB)->GetTileX(),
                                   EEUcObject<EEUnit>((uint64_t)inB)->GetTileY(), false, true);
    case EE_UNITS_UC_NEW_GATHER: return (uint64_t)(uintptr_t)EEUGRust::NewGather(EEUcObject<EEUnit>(inA), 0, true);
    case EE_UNITS_UC_DELETE_GOAL: delete EEUcObject<EEUnitGoal>(inA); return 0;
    case EE_UNITS_UC_GATHER_BEHAVIOR: theOther->SetBehavior(EEUnitBehavior::sTable[5]); return 0;
    case EE_UNITS_UC_QUEUE_GOAL_ON: theOther->QueueGoal(EEUcObject<EEUnitGoal>((uint64_t)inB), 0); return 0;
    case EE_UNITS_UC_GOAL_STATE: return (uint64_t)(uintptr_t)theOther->mGoalState;
    case EE_UNITS_UC_START_GOAL_ACTION: theOther->StartAction(theOther->mGoalState->GetStartAction()); return 0;
    case EE_UNITS_UC_PRODUCER_IS_CURRENT: return theUnit->GetPlayer() == gWorld.mPlayers[gWorld.mCurrentPlayer];
    case EE_UNITS_UC_CREATION_SOUND: return (uint64_t)(uintptr_t)theOther->GetStats()->mCreationSound;
    case EE_UNITS_UC_PLAY_SAMPLE:
        gSoundManager->Play(EEUcObject<EESoundObject>(inA)->mSampleID, EEUcObject<EESoundObject>(inA)->mVolume, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_UNITS_UC_PRODUCER_TILE_X: return theUnit->GetTileX();
    case EE_UNITS_UC_PRODUCER_TILE_Y: return theUnit->GetTileY();
    // UpdateBuilding's
    case EE_UNITS_UC_DISABLED: return theUnit->mIsDisabled;
    case EE_UNITS_UC_HIT_POINTS: return theUnit->mHitPoints;
    case EE_UNITS_UC_PRODUCTION_TIME: return theUnit->mProductionTime;
    case EE_UNITS_UC_IN_CINEMATIC: return gInCinematic;
    case EE_UNITS_UC_TICK_MS: return gPhysics->mTickTimeMs;
    case EE_UNITS_UC_INSTANT_BUILD: return theUnit->mPlayer->mInstantBuild;
    case EE_UNITS_UC_RESEARCH_MAKES_UNIT: return theUnit->mPlayer->mTechnologies->Get(theUnit->mCurrentResearch)->mData->mMakesUnit;
    case EE_UNITS_UC_RESEARCH_CLASS:
        return (uint64_t)(uintptr_t)gUnitClassTable[theUnit->mPlayer->mTechnologies->Get(theUnit->mCurrentResearch)->mData->mUnitClass];
    case EE_UNITS_UC_IS_LAND_BUILDING: return EEUcObject<EEUnitClass>(inA)->IsLandBuilding();
    case EE_UNITS_UC_PRODUCE_UNIT: return theClass->ProduceUnit(theUnit, EEUcObject<EEUnitClass>(inA), true);
    case EE_UNITS_UC_RESEARCH: theUnit->mPlayer->mTechnologies->Research(theUnit->mCurrentResearch, theUnit); return 0;
    case EE_UNITS_UC_PLAY_NO_EFFECT: theUnit->PlayEffect(100000); return 0;
    case EE_UNITS_UC_SET_PRODUCTION_TIME: theUnit->SetProductionTime((long)inB); return 0;
    case EE_UNITS_UC_UPDATE_ORBITALS: theUnit->UpdateOrbitals(); return 0;
    case EE_UNITS_UC_UPDATE_CAMOUFLAGE_TIMER: theClass->UpdateCamouflageTimer(theUnit); return 0;
    case EE_UNITS_UC_UPDATE_EFFECTS: theClass->UpdateEffects(theUnit); return 0;
    case EE_UNITS_UC_COMMAND_KIND: return theUnit->mCommand != NULL ? theUnit->mCommand->mType : 0;
    case EE_UNITS_UC_UPDATE_ROTTING: return (long)theUnit->UpdateRottingAwayTime();
    case EE_UNITS_UC_NEXT_PART: return (uint64_t)(uintptr_t)(inA != 0 ? theOther : theUnit)->mNextPart;
    case EE_UNITS_UC_SET_ROTTING: theOther->SetRottingAwayTime((long)inB); return 0;
    case EE_UNITS_UC_CONTINUE_PRODUCTION: theUnit->ContinueProduction(); return 0;
    case EE_UNITS_UC_HEAL_GARRISON: theClass->HealGarrison(theUnit); return 0;
    // UpdateAnimal's
    case EE_UNITS_UC_FRAME: return theUnit->mPhysicalModel.GetFrame();
    case EE_UNITS_UC_KEEP_LAST_HIT_POINTS: theUnit->SetLastHitPoints(theUnit->mHitPoints); return 0;
    case EE_UNITS_UC_HAS_PHYSICS_MODEL: return theClass->mPhysicsModel != NULL;
    case EE_UNITS_UC_TILE_X: return theUnit->GetTileX();
    case EE_UNITS_UC_PHYSICS_STEP: theClass->mPhysicsModel->Step(theUnit); return 0;
    case EE_UNITS_UC_UPDATE_RELOAD_TIMER: theUnit->UpdateReloadTimer(); return 0;
    case EE_UNITS_UC_UPDATE_TARGET_SEARCH_TIMER: theUnit->UpdateTargetSearchTimer(); return 0;
    case EE_UNITS_UC_ANIMATE: return theClass->Animate(theUnit);
    case EE_UNITS_UC_REFRESH_SOUND: theClass->RefreshSound(theUnit); return 0;
    case EE_UNITS_UC_COMMAND_TYPE: return theUnit->GetCommandType();
    case EE_UNITS_UC_FIRE_ON_ATTACK_FRAME: theClass->FireOnAttackFrame(theUnit, (long)inB, (long)inC); return 0;
    case EE_UNITS_UC_UPDATE_RESOURCES_ROTTING: theUnit->UpdateResourcesRotting(); return 0;
    case EE_UNITS_UC_UPDATE_DONE: theClass->UpdateDone(theUnit); return 0;
    // RefreshSound's
    case EE_UNITS_UC_HAS_SOUND: return theUnit->mSound != NULL;
    case EE_UNITS_UC_SOUND_NONE: return UIsNone32(gSoundManager->GetInstanceID(theUnit->mSound));
    case EE_UNITS_UC_TICKS_DONE: return gPhysics->mNumTicksDone;
    case EE_UNITS_UC_UNIT_ID: return theUnit->mUnitID;
    case EE_UNITS_UC_IN_VIEW: return theUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView);
    case EE_UNITS_UC_SOUND_SAMPLE: return gSoundManager->GetSampleID(theUnit->mSound);
    case EE_UNITS_UC_MODEL_SOUND1: return (uint64_t)(uintptr_t)theUnit->GetModel()->mSounds1[theUnit->GetAnimation()];
    case EE_UNITS_UC_MODEL_SOUND2: return (uint64_t)(uintptr_t)theUnit->GetModel()->mSounds2[theUnit->mAnimation];
    case EE_UNITS_UC_SAMPLE_ID: return EEUcObject<EESoundObject>(inA)->mSampleID;
    case EE_UNITS_UC_SOUND_PARAMETERS:
        theClass->GetSoundParameters(theUnit, EEUcObject<EESoundObject>(inA), &f[0], &f[1], &f[2]);
        return 0;
    case EE_UNITS_UC_SET_SOUND_PARAMETERS: gSoundManager->SetSoundParameters(theUnit->mSound, f[0], f[1], f[2]); return 0;
    // CreateDeathUnit's
    case EE_UNITS_UC_DEATH_SPAWN_CLASS: return (uint64_t)(uintptr_t)theClass->mDeathSpawnClass;
    case EE_UNITS_UC_CONTAINER: return (uint64_t)(uintptr_t)theUnit->mContainer;
    case EE_UNITS_UC_LOCATION:
        EEUcLocationOut(theOther->mPhysicalModel.GetLocation(), f);
        return 0;
    case EE_UNITS_UC_UNIT_TILE_X: return theOther->GetTileX();
    case EE_UNITS_UC_UNIT_TILE_Y: return theOther->GetTileY();
    case EE_UNITS_UC_CREATE_UNIT:
        return (uint64_t)(uintptr_t)EEUcObject<EEUnitClass>(inA)->CreateUnit((long)inB, (long)inC, theUnit->mPlayer->GetPlayerID(), false, NULL)->GetComplexUnit();
    case EE_UNITS_UC_NEW_ATTACK_GROUND: return (uint64_t)(uintptr_t)EEUGRust::NewAttackGround(GE3DPoint(f[0], f[1], f[2]), false);
    case EE_UNITS_UC_STOP: theOther->SetVelocity(0.0f); return 0;
    case EE_UNITS_UC_OWN_LAUNCHER_CLASS: theOther->SetLauncherClass(theOther->GetUnitClass()); return 0;
    case EE_UNITS_UC_SET_OWNER: theOther->SetOwner(theUnit); return 0;
    case EE_UNITS_UC_SET_PROJECTILE_START: theOther->SetProjectileStart(GE3DPoint(f[0], f[1], f[2])); return 0;
    case EE_UNITS_UC_SET_GOAL: theOther->SetGoal(EEUcObject<EEUnitGoal>((uint64_t)inB), 0, 0); return 0;
    case EE_UNITS_UC_START_ACTION: theOther->StartAction((long)inB); return 0;
    case EE_UNITS_UC_ACTION_CHANGED: theOther->mUnitClass->ActionChanged(theOther); return 0;
    case EE_UNITS_UC_MUZZLE_FLASH: return (uint64_t)(uintptr_t)theOther->mUnitClass->mMuzzleFlashEffect;
    case EE_UNITS_UC_PLACED_EFFECT:
        gEffectManager->CreatePlacedEffect(GE3DPoint(f[0], f[1], f[2]), EEUcObject<EEGraphicEffect>(inA), NULL);
        return 0;
    }
    return 0;
}
