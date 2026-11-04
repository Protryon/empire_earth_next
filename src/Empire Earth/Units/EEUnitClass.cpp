// C:\EEX\Empire Earth\Units\EEUnitClass.cpp (__FILE__ 0x2358b1's string). The unit class base: its `$E` globals, the
// ctor 0x22f317 (copies a dbunits record), dtor 0x2305ee and members, the class factory 0x2358b1 (one inline ctor per
// EEUC* subclass), the EEUC* inline dtors and slots emitted with their vtables (0x2362e3..0x23649e), more EEUnitClass
// members and the EEFOFindNewTarget* functors to 0x23a5bf (EEUnitClassImprovement.cpp follows).
// Range 0x22f272..0x23a5bf. Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include "ee/EEUCUpgradeTable.h"
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UResourceFile.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/FSPersistent.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGLoadGarrison.h"
#include "ee/EEUGBuildRepair.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAirTrafficControl.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMiniMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEWheel.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EESoundObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGAttackGround.h"
#include "ee/EEUGLocation.h"
#include "ee/EEAIContinentManager.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIContinent.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEAnimal.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitClassRecord.h"
#include "ee/EEDbObject.h"
#include "ee/EEDatabase.h"
#include "ee/EEUCAirAnimal.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEUCAmbient.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCBomb.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCBuildingPriest.h"
#include "ee/EEUCFortress.h"
#include "ee/EEUCGate.h"
#include "ee/EEUCHelicopter.h"
#include "ee/EEUCHelicopterPriest.h"
#include "ee/EEUCHelicopterTransport.h"
#include "ee/EEUCLand.h"
#include "ee/EEUCLandCitizen.h"
#include "ee/EEUCLandMilitaryHero.h"
#include "ee/EEUCLandPriest.h"
#include "ee/EEUCLandTransport.h"
#include "ee/EEUCLandable.h"
#include "ee/EEUCMiningBuilding.h"
#include "ee/EEUCMissile.h"
#include "ee/EEUCNaval.h"
#include "ee/EEUCNavalBuilding.h"
#include "ee/EEUCNavalCitizen.h"
#include "ee/EEUCNavalPriest.h"
#include "ee/EEUCNavalTransport.h"
#include "ee/EEUCPlanetSpaceship.h"
#include "ee/EEUCProjectile.h"
#include "ee/EEUCResource.h"
#include "ee/EEUCSeaAnimal.h"
#include "ee/EEUCSpaceBuilding.h"
#include "ee/EEUCSpaceTransport.h"
#include "ee/EEUCSpaceship.h"
#include "ee/EEUCSubmergedNaval.h"
#include "ee/EEUCTorpedo.h"
#include "ee/EEUCTransport.h"
#include "ee/EEFOFindNewTargetInMeleeRange.h"
#include "ee/EEMathInline.h"
#include <algorithm>
#include "lle/GESurface.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GERasterizer.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kHeadingUnset = -10000.0f;    // 0x447ac0
static const float kSoundDistanceFactor = 4.0f;  // 0x447ac4 (Virtual2C)
static const float kMaxLineOfSight = 32.0f;      // 0x447ac8 (the ctor: the largest line of sight)
static const float kBuildingZOffset = 0.01f;     // 0x447acc
static const float kCloakFadeTime = 750.0f;      // 0x447ad0
static const float kCloakFadeMaxOpacity = 1.0f;  // 0x447ad4
static const float kCloakFadeMinOpacity = 0.6f;  // 0x447ad8
static const float k447ADC = 0.40824828f;        // 0x447adc (1 / sqrt(6))
static const float kFacingTolerance = 0.043633234f;  // 0x447ae0
extern EEToHitTable gToHitTable;           // 0x5318e8 (Empire Earth.cpp; as EECPAttackSubGroupUnit.cpp declares it)
static const float k447AE4 = 0.1f;               // 0x447ae4
static const float kFleeDistanceScale = 1.5f;    // 0x447ae8
static const float kCallForHelpRadius = 5.0f;    // 0x447aec
static const float k447AF0 = 0.01f;              // 0x447af0
static const float kLargeUnitScale = 1.5f;       // 0x447af4
static const float kMinConstructionScale = 0.1f;  // 0x447af8
static const float k447AFC = 10.0f;              // 0x447afc
static const float kMinWaypointDistanceSquared = 36.0f;  // 0x447b00
static const float kMinDamageRoll = 0.05f;       // 0x447b04
static const float kMaxDamageRoll = 0.2f;        // 0x447b08
// the wall pieces' yaws (0x446dc0, one object shared with EEGhostUnit.cpp and EEUCBuilding.cpp; ReplantFarm)
static const float kWallRotations[4] = {0.0f, 3.1415927f, -1.5707964f, 1.5707964f};

// SYNTHETIC: EXE 0x62f272
// SYNTHETIC: EXE 0x62f277
static float sCloakFadeRate = 1.0f / kCloakFadeTime;  // 0x5636a8: the cloak fade per millisecond (provisional name)

// SYNTHETIC: EXE 0x62f286
// SYNTHETIC: EXE 0x62f28b
static float gF5636A4 = kCloakFadeMaxOpacity - kCloakFadeMinOpacity;  // 0x5636a4: the cloak fade's opacity range (provisional name)

// SYNTHETIC: EXE 0x62f29e
// SYNTHETIC: EXE 0x62f2a3
static float gF5636A0 = k447ADC * 0.5f;          // 0x5636a0 (provisional name)

// SYNTHETIC: EXE 0x62f2b6
// SYNTHETIC: EXE 0x62f2bb
static float gF56369C = k447ADC * 4.0f;          // 0x56369c (provisional name)

// the unit classes by type (0x5636ac: 0x367 entries up to sUnitTypes at 0x564448; ee/EEUnit.h)
EEUnitClass* gUnitClassTable[871];
// .data 0x4cc9fc/0x4cca00 (= 0.7, 1.5; this unit's data follows: ee/EERenderLists.h, EERenderObject.cpp reads them)
float gSpaceTurretHeight = 0.7f;
float gSpaceStationHeight = 1.5f;

// 0x564448: the unit types registered in gUnitClassTable (Register 0x236a9c push_backs; the vector right after the
// table, guarded dtor bit 1 of 0x563698: a class static).
std::vector<unsigned long> EEUnitClass::sUnitTypes;
// SYNTHETIC: EXE 0x62f2ce   (wrapper)
// SYNTHETIC: EXE 0x62f2d8   (ctor)
// SYNTHETIC: EXE 0x62f2f0   (registrar)
// SYNTHETIC: EXE 0x62f2fc   (dtor)

// FUNCTION: EXE 0x62f317
// Creates a unit class from its dbunits record: stats, sounds and effects (referenced), terrain table, movement
// domains and kind flags (terrain slot 5 copies slot 0's record byte; the animal id is looked up twice)
EEUnitClass::EEUnitClass(long inIsComplex, EEUnitClassRecord* inRecord, long inArg3)
{
    this->mResearchTechnologies.resize(inRecord->mNumAttributes, inArg3);
    this->mPhysicsModel = reinterpret_cast<EEPhysicsModel*>(inArg3);
    for (unsigned long i = 0; i < inRecord->mNumAttributes; i++)
        this->mResearchTechnologies[i] = inRecord->mAttributes[i];
    for (unsigned long theIndex = 0; theIndex < 7; theIndex++)
        this->mGatherTimes[theIndex] = 0;
    this->mIsMediaLoaded = false;
    this->mIsCyber = false;
    this->mIsHorse = false;
    this->mNumUnits = 0;
    this->mLastUnitGoneTime = 0;
    this->mIsComplex = inIsComplex;
    this->mCategory = inRecord->mCategory;
    this->mUnitType = inRecord->mUnitType;
    this->mResourceType = inRecord->mResourceType;
    this->mFamily = inRecord->mFamily;
    this->mL208 = inRecord->mL06C;
    this->mL294 = inRecord->mL32C;
    this->mTechnology = inRecord->mTechnology;
    this->mButton = EEButtonObject::Get(inRecord->mButton);
    this->mModel = (EEModel*)EEMedia::Get(inRecord->mGraphic);
    this->mCreationSound = EESoundObject::Get(inRecord->mL150);
    this->mAcknowledgeSounds[0] = EESoundObject::Get(inRecord->mAcknowledgeSound1);
    this->mAcknowledgeSounds[1] = EESoundObject::Get(inRecord->mAcknowledgeSound2);
    this->mSelectionSounds[0] = EESoundObject::Get(inRecord->mSelectionSound1);
    this->mSelectionSounds[1] = EESoundObject::Get(inRecord->mSelectionSound2);
    this->mDeathSound = EESoundObject::Get(inRecord->mDeathSound);
    this->mMinRange = inRecord->mMinRange;
    this->mRange = inRecord->mRange;
    this->mLineOfSight = inRecord->mLineOfSight;
    if (this->mLineOfSight > kMaxLineOfSight) {
        this->mLineOfSight = kMaxLineOfSight;
        ESDebugOut("EEUnitClass::EEUnitClass: unit %s has an LOS of %f.\n", inRecord->mName, inRecord->mLineOfSight);
    }
    this->mAreaEffect = inRecord->mAreaEffect;
    this->mScale = inRecord->mScale > 0.0001f ? inRecord->mScale : 0.0f;
    this->mAcceleration = inRecord->mAcceleration;
    this->mMaxVelocity = inRecord->mMaxVelocity;
    this->mF138 = 0.0f;
    this->mMaxAngularVelocity = inRecord->mMaxAngularVelocity;
    this->mAngularAcceleration = inRecord->mAngularAcceleration;
    this->mTurretMaxAngularVelocity = inRecord->mTurretMaxAngularVelocity;
    this->mResourceAmount = (long)inRecord->mResourceAmount;
    this->mCarryAmount = (long)inRecord->mCarryAmount;
    this->mWorkRate = (long)inRecord->mWorkRate;
    this->mGatherTimes[0] = (long)inRecord->mWoodGatherTime;
    this->mGatherTimes[1] = (long)inRecord->mStoneGatherTime;
    this->mGatherTimes[2] = (long)inRecord->mGoldGatherTime;
    this->mGatherTimes[3] = (long)inRecord->mIronGatherTime;
    this->mGatherTimes[4] = (long)inRecord->mHuntGatherTime;
    this->mGatherTimes[6] = (long)inRecord->mFarmGatherTime;
    this->mGatherTimes[5] = (long)inRecord->mForageGatherTime;
    this->mReloadTime = (long)(inRecord->mReloadTime * 1000.0f);
    this->mFacingOffset = inRecord->mFacingOffset;
    this->mFiringArc = inRecord->mFiringArc;
    if (inRecord->mFiringArc == 0.0f)
        this->mFiringArc = 0.34906587f;
    this->mWeapon = inRecord->mWeapon;
    this->mAttack = inRecord->mAttack;
    this->mArmor[0] = inRecord->mShockArmor;
    this->mArmor[1] = inRecord->mArrowArmor;
    this->mArmor[2] = inRecord->mPierceArmor;
    this->mArmor[3] = inRecord->mGunArmor;
    this->mArmor[4] = inRecord->mLaserArmor;
    this->mArmor[5] = inRecord->mMissileArmor;
    this->mFlightTime = inRecord->mFlightTime * 1000;
    this->mLeadership = inRecord->mLeadership;
    this->mMaxObjectCarry = inRecord->mMaxObjectCarry;
    this->mRottingTime = (long)(inRecord->mRottingTime * 1000.0f);
    this->mBuildTimeScale = 1.0f;
    this->mCostScale = 1.0f;
    this->mAreaEffectScale = 1.0f;
    this->mAmmoType = inRecord->mAmmoType;
    this->mRepairRate = inRecord->mRepairRate;
    this->mHitPoints = inRecord->mHitPoints;
    this->mCombatReaction = 0;
    this->mIsPlayerPlaceable = inRecord->mIsPlayerPlaceable;
    this->mIsVital = inRecord->mIsVital;
    this->mTileSize = inRecord->mTileSize;
    this->mWaterKind = inRecord->mWaterKind;
    this->mIsWall = false;
    this->mIsHuman = false;
    this->mLoadRange = inRecord->mLoadRange;
    this->mUnloadRange = inRecord->mUnloadRange;
    memset(this->mTerrainTypes, 0, sizeof(this->mTerrainTypes));
    this->mB315 = inRecord->mB3AC;
    this->mTerrainTypes[0] = inRecord->mB2DC;
    this->mTerrainTypes[kTerrainDeepWater] = inRecord->mB2DD;
    this->mTerrainTypes[2] = inRecord->mB2DE;
    this->mTerrainTypes[3] = inRecord->mB2DF;
    this->mTerrainTypes[4] = inRecord->mB2E2;
    this->mTerrainTypes[kTerrainSnow] = inRecord->mB2DC;
    this->mTerrainTypes[6] = inRecord->mB2E4;
    this->mTerrainTypes[7] = inRecord->mB2E5;
    this->mTerrainTypes[8] = inRecord->mB2E6;
    this->mTerrainTypes[kTerrainCliff] = inRecord->mB2E7;
    this->mTerrainTypes[kTerrainShallowWater] = inRecord->mB2E8;
    this->mTerrainTypes[11] = inRecord->mB2EA;
    this->mTerrainTypes[kTerrainSpace] = inRecord->mB2EB;
    this->mTerrainTypes[14] = inRecord->mB3AC;
    this->mIsDecoration = inRecord->mIsDecoration;
    this->mFiresBroadside = inRecord->mFiresBroadside;
    this->mCanFireOverWalls = inRecord->mCanFireOverWalls;
    this->mCanAttackGround = inRecord->mCanAttackGround;
    this->mIsMelee = inRecord->mIsMelee;
    this->mCanDisband = inRecord->mCanDisband;
    this->mIsScenarioSelectable = inRecord->mIsScenarioSelectable;
    this->mIsEnabled = inRecord->mIsEnabled;
    this->mB2F6 = inRecord->mB1A0;
    this->mFiresWhileMoving = inRecord->mFiresWhileMoving;
    this->mCanFly = inRecord->mCanFly;
    this->mB2FA = inRecord->mB319;
    this->mBuildsOnElevation = inRecord->mBuildsOnElevation;
    this->mTransportCapacity = inRecord->mTransportCapacity;
    this->mWeight = inRecord->mTransportSize;
    this->mPitchesWithTerrain = inRecord->mPitchesWithTerrain;
    this->mRollsWithTerrain = inRecord->mRollsWithTerrain;
    this->mUpgradesAreaEffect = inRecord->mCanUpgradeAreaEffect;
    this->mUpgradesArrowArmor = inRecord->mCanUpgradeArrowArmor;
    this->mUpgradesGunArmor = inRecord->mCanUpgradeGunArmor;
    this->mUpgradesMissileArmor = inRecord->mCanUpgradeMissileArmor;
    this->mUpgradesRange = inRecord->mCanUpgradeRange;
    this->mUpgradesLaserArmor = inRecord->mCanUpgradeLaserArmor;
    this->mUpgradesPierceArmor = inRecord->mCanUpgradePierceArmor;
    this->mUpgradesRepair = inRecord->mCanUpgradeRepair;
    this->mUpgradesShockArmor = inRecord->mCanUpgradeShockArmor;
    this->mAIScriptName.Copy(inRecord->mAIScriptName, 0, strlen(inRecord->mAIScriptName), 0, true);
    this->mF19C = inRecord->mF368;
    if (this->mF19C == 0.0f)
        this->mF19C = -20.0f;
    this->mMaxRoll = inRecord->mMaxRoll;
    this->mCruiseAltitude = inRecord->mCruiseAltitude;
    this->mTargetSearchInterval = inRecord->mTargetSearchInterval;
    this->mNameStringID = inRecord->mNameStringID;
    this->mHealingPoints = inRecord->mHealingPoints;
    this->mRegeneration = inRecord->mRegeneration;
    this->mCanEnterAirTransports = inRecord->mCanEnterAirTransports;
    this->mCanEnterLandTransports = inRecord->mCanEnterLandTransports;
    this->mCanEnterSpaceTransports = this->mCanEnterSeaTransports = inRecord->mCanEnterSeaTransports;
    this->mCanEnterFortresses = inRecord->mCanEnterFortresses;
    this->mSelectionSize = inRecord->mSelectionSize;
    this->mL170 = inRecord->mL1FC;
    this->mShadowScale = inRecord->mShadowScale;
    this->mOuterCollisionSphere = inRecord->mOuterCollisionSphere;
    this->mInnerCollisionSphere = inRecord->mInnerCollisionSphere;
    this->mIsCollidable = inRecord->mIsCollidable;
    this->mAmmunition = inRecord->mAmmunition;
    this->mUsesAmmunition = inRecord->mUsesAmmunition;
    this->mIsAffectedByGravity = inRecord->mIsAffectedByGravity;
    this->mShootsHigh = inRecord->mShootsHigh;
    this->mL284 = inRecord->mL21C;
    this->mL180 = inRecord->mL220;
    this->mMissileDelayTime = (long)(inRecord->mMissileDelayTime * 1000.0f);
    this->mLaunchPitch = inRecord->mLaunchPitch * 0.017453292f;  // NEAR: the original multiplies by DAT_446094 (a named const)
    this->mCanSetRallyPoint = inRecord->mCanSetRallyPoint;
    this->mAIKind = inRecord->mAIKind;
    this->mCounterKind1 = inRecord->mCounterKind1;
    this->mCounterKind2 = inRecord->mCounterKind2;
    this->mCounterKind3 = inRecord->mCounterKind3;
    this->mB305 = inRecord->mB209;
    this->mIsGaiaPlaceable = inRecord->mIsGaiaPlaceable;
    this->mConvertsBuildings = false;
    this->mConvertsPriests = false;
    this->mName.Copy(inRecord->mName, 0, strlen(inRecord->mName), 0, true);
    this->mConversionResistance = inRecord->mConversionResistance;
    this->mConversionRate = (float)inRecord->mConversionRate * 0.01f;
    this->mF18C = inRecord->mF354;
    this->mBobHeight = inRecord->mBobHeight;
    this->mBobPitch = inRecord->mBobPitch * 0.017453292f;
    this->mIsCloakable = inRecord->mIsCloakable;
    this->mCloakRadius = inRecord->mCloakRadius;
    this->mB30C = inRecord->mB37E;
    this->mCanEnterCarriers = inRecord->mCanEnterCarriers;
    this->mCanEnterAirports = inRecord->mCanEnterAirports;
    if (this->mUnitType == kUnitTypeCatalina)
        this->mCanEnterNavyYards = true;
    else
        this->mCanEnterNavyYards = false;
    this->mMiniMapColor = inRecord->mMiniMapColor;
    this->mAirTrafficType = inRecord->mAirTrafficType;
    this->mIsDeletable = inRecord->mIsDeletable;
    this->mWonderBonus = inRecord->mWonderBonus;
    this->mWonderUnitSet = inRecord->mWonderUnitSet;
    this->mIsInactive = inRecord->mIsInactive;
    this->mIsDefenseUnit = inRecord->mIsDefenseUnit;
    this->mIsAttackUnit = inRecord->mIsAttackUnit;
    this->mWaterKind = inRecord->mWaterKind;
    this->mPowerRegeneration = inRecord->mPowerRegeneration;
    this->mPower = inRecord->mPower;
    this->mHealThreshold = inRecord->mHealThreshold;
    this->mHealRange = inRecord->mHealRange;
    this->mHealDelay = inRecord->mHealDelay;
    this->mMinConversionTime = inRecord->mMinConversionTime;
    this->mSkipsLineOfFireCheck = false;
    this->mHasSpecialAttack = inRecord->mHasSpecialAttack;
    if (this->mUnitType == kUnitTypeGuardTowerPaleo || (this->mUnitType > kUnitTypeAmbStarfish3 && (this->mUnitType <= kUnitTypeGuardTowerDigital || this->mUnitType == kUnitTypeGuardTowerSpace || this->mUnitType == kUnitTypeSpaceTurret
        || this->mUnitType == kUnitTypeGuardTowerPalisades || this->mUnitType == kUnitTypeGuardTowerBamboo)))
        this->mIsTower = true;
    else
        this->mIsTower = false;
    this->mDomains = 0;
    if (this->mCategory == kUnitCategoryLand || this->mCategory == kUnitCategoryLandCitizen || this->mCategory == kUnitCategoryLandPriest || this->mCategory == kUnitCategoryLandTransport || this->mCategory == kUnitCategoryLandMilitaryHero
        || (this->mCategory == kUnitCategoryBuilding && (this->mIsTower || this->mUnitType == kUnitTypeMissileBase || this->mUnitType == kUnitTypeCpMissileBase)))
        this->mDomains = kMovementLand;
    if (this->mCategory == kUnitCategoryAirplane || this->mCategory == kUnitCategoryHelicopter || this->mCategory == kUnitCategoryHelicopterTransport || this->mCategory == kUnitCategoryHelicopterPriest || this->mCategory == kUnitCategoryAirAnimal)
        this->mDomains |= kMovementAir;
    if (this->mCategory == kUnitCategoryNaval || this->mCategory == kUnitCategoryNavalCitizen || this->mCategory == kUnitCategoryLand8 || this->mCategory == kUnitCategorySubmergedNaval || this->mCategory == kUnitCategoryNavalPriest
        || this->mCategory == kUnitCategoryNavalTransport || this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySeaAnimal)
        this->mDomains |= kMovementSea;
    if (this->mCategory == kUnitCategorySpaceship || this->mCategory == kUnitCategorySpaceTransport || this->mCategory == kUnitCategorySpaceBuilding || this->mCategory == kUnitCategoryPlanetSpaceship)
        this->mDomains |= kMovementSpace;
    if (this->mUnitType == kUnitTypeResShark || this->mUnitType == kUnitTypeResDolphin)
        this->mIsCollidable = false;
    EEAnimal* theAnimal = EEAnimal::Find(this->mUnitType);
    if (theAnimal != NULL)
        this->mAnimalID = theAnimal->mAnimalID;
    else
        this->mAnimalID = 1000;
    theAnimal = EEAnimal::Find(this->mUnitType);
    if (theAnimal != NULL)
        this->mAnimalID = theAnimal->mAnimalID;
    else
        this->mAnimalID = 1000;
    this->mInfluence = inRecord->mInfluence;
    if (this->mInfluence == 0 && inRecord->mAttack > 0)
        this->mInfluence = 1;
    this->mCanGarrison = inRecord->mCanGarrison;
    this->mGarrisonCapacity = inRecord->mGarrisonCapacity;
    this->mInitialGarrisonCount = inRecord->mInitialGarrisonCount;
    this->mGarrisonTechnology = inRecord->mGarrisonTechnology;
    this->mGarrisonBonus = inRecord->mGarrisonBonus;
    EETechTreeNode* theNode = gTechTree.Get(this->mTechnology);
    if (theNode != NULL)
        this->mEpoch = theNode->mEpoch;
    else
    this->mEpoch = inRecord->mEpoch;
    this->mHasCalamityPowers = false;
    if (this->GetNumResearchTechnologies() != 0) {
        for (unsigned long j = 0; j < this->GetNumResearchTechnologies() && !this->mHasCalamityPowers; j++) {
            long& theID = this->mResearchTechnologies[j];
            EETechTreeNode* theTechnology = gTechTree.Get(theID);
            this->mHasCalamityPowers = theTechnology != NULL && theTechnology->mData->mKind == 5;
        }
    }
    this->mIsProjectile = this->mCategory == kUnitCategoryProjectile || this->mCategory == kUnitCategoryBomb || this->mCategory == kUnitCategoryMissile || this->mCategory == kUnitCategoryTorpedo;
    this->mAreaEffectTemplates[0] = inRecord->mAreaEffectTemplate0;
    this->mAreaEffectTemplates[1] = inRecord->mAreaEffectTemplate1;
    this->mAreaEffectTemplates[2] = inRecord->mAreaEffectTemplate2;
    this->mAreaEffectTemplates[3] = inRecord->mAreaEffectTemplate3;
    EEUnitClassImprovementTable* theTable = reinterpret_cast<EEUnitClassImprovementTable*>(EEUCUpgradeTable::sTable[inRecord->mUpgradeTableIndex]);
    if (theTable != NULL)
        this->mImprovements = theTable;
    else
        this->mImprovements = NULL;
    this->mOverlapUnitSet = inRecord->mOverlapUnitSet;
    switch (this->mCategory) {
    case kUnitCategoryAmbient:
        if (this->mUnitType == kUnitTypePathPoint)
            this->mKindFlags = kUnitKindPathPoint;
        else
            this->mKindFlags = 0;
        break;
    case kUnitCategoryLandCitizen:
        this->mKindFlags = kUnitKindCitizen;
        break;
    case kUnitCategoryNavalCitizen:
        this->mKindFlags = kUnitKindNavalCitizen;
        break;
    case kUnitCategoryBuilding:
    case kUnitCategoryBuilding19:
    case kUnitCategoryNavalBuilding:
    case kUnitCategoryMiningBuilding:
    case kUnitCategoryGate:
    case kUnitCategorySpaceBuilding:
        if (this->mIsTower || this->mUnitType == kUnitTypeMissileBase || this->mUnitType == kUnitTypeCpMissileBase
            || ((this->mCategory == kUnitCategoryNavalBuilding || this->mCategory == kUnitCategorySpaceBuilding) && this->mAirTrafficType != 0) || this->mUnitType == kUnitTypeAirport || this->mUnitType == kUnitTypePillbox)
            this->mKindFlags = kUnitKindMilitary;
        else
            this->mKindFlags = 0x10;
        break;
    case kUnitCategoryAirplane:
    case kUnitCategoryNaval:
    case kUnitCategoryLand:
    case kUnitCategoryHelicopter:
    case kUnitCategoryLand8:
    case kUnitCategorySubmergedNaval:
    case kUnitCategoryLand15:
    case kUnitCategoryLandMilitaryHero:
    case kUnitCategorySpaceship:
    case kUnitCategoryPlanetSpaceship:
        this->mKindFlags = kUnitKindMilitary;
        if (this->mRange > this->mLineOfSight)
            this->mKindFlags = kUnitKindLongRangeMilitary;
        break;
    case kUnitCategoryLandPriest:
    case kUnitCategoryNavalPriest:
    case kUnitCategoryHelicopterPriest:
        if (this->mUnitType == kUnitTypeProphet)
            this->mKindFlags = kUnitKindMilitary;
        else
            this->mKindFlags = 4;
        break;
    case kUnitCategoryBuildingPriest:
        this->mKindFlags = kUnitKindBuildingPriest;
        break;
    case kUnitCategoryLandTransport:
        this->mKindFlags = kUnitKindLandTransport;
        break;
    case kUnitCategoryFortress:
        this->mKindFlags = kUnitKindFortress;
        break;
    case kUnitCategoryHelicopterTransport:
        this->mKindFlags = kUnitKindHelicopterTransport;
        break;
    case kUnitCategoryNavalTransport:
        this->mKindFlags = kUnitKindNavalTransport;
        break;
    case kUnitCategorySpaceTransport:
        this->mKindFlags = kUnitKindSpaceTransport;
        break;
    case kUnitCategoryResource:
        this->mKindFlags = kUnitKindResource;
        break;
    default:
        this->mKindFlags = 0;
        break;
    }
    if (this->mCanEnterAirTransports)
        this->mKindFlags |= kUnitKindCanEnterAirTransports;
    if (this->mCanEnterLandTransports)
        this->mKindFlags |= kUnitKindCanEnterLandTransports;
    if (this->mCanEnterSeaTransports)
        this->mKindFlags |= kUnitKindCanEnterSeaTransports;
    if (this->mCanEnterFortresses)
        this->mKindFlags |= kUnitKindCanEnterFortresses;
    if (this->mCanEnterSpaceTransports)
        this->mKindFlags |= kUnitKindCanEnterSpaceTransports;
    this->mDeathEffect = EEGraphicEffect::Get(inRecord->mL24C);
    if (this->mDeathEffect != NULL)
        this->mDeathEffect->Reference();
    this->mDamageEffect = EEGraphicEffect::Get(inRecord->mL250);
    if (this->mDamageEffect != NULL)
        this->mDamageEffect->Reference();
    this->mHealEffect = EEGraphicEffect::Get(inRecord->mL264);
    if (this->mHealEffect != NULL)
        this->mHealEffect->Reference();
    this->mFireTrailEffect = EEGraphicEffect::Get(inRecord->mL268);
    if (this->mFireTrailEffect != NULL)
        this->mFireTrailEffect->Reference();
    this->mTrailEffect = EEGraphicEffect::Get(inRecord->mL258);
    if (this->mTrailEffect != NULL)
        this->mTrailEffect->Reference();
    this->mMuzzleFlashEffect = EEGraphicEffect::Get(inRecord->mL254);
    if (this->mMuzzleFlashEffect != NULL)
        this->mMuzzleFlashEffect->Reference();
    this->mRippleEffect = EEGraphicEffect::Get(inRecord->mL25C);
    if (this->mRippleEffect != NULL)
        this->mRippleEffect->Reference();
    this->mSplashEffect = EEGraphicEffect::Get(inRecord->mL260);
    if (this->mSplashEffect != NULL)
        this->mSplashEffect->Reference();
    this->mImpactSparksEffect = EEGraphicEffect::Get(inRecord->mL26C);
    if (this->mImpactSparksEffect != NULL)
        this->mImpactSparksEffect->Reference();
    this->mImpactBloodEffect = EEGraphicEffect::Get(inRecord->mL270);
    if (this->mImpactBloodEffect != NULL)
        this->mImpactBloodEffect->Reference();
    this->mAttachedEffect = EEGraphicEffect::Get(inRecord->mL274);
    if (this->mAttachedEffect != NULL)
        this->mAttachedEffect->Reference();
    this->mEffect090 = EEGraphicEffect::Get(inRecord->mL278);
    if (this->mEffect090 != NULL)
        this->mEffect090->Reference();
    this->mCreationEffect = EEGraphicEffect::Get(inRecord->mCreationEffect);
    if (this->mCreationEffect != NULL)
        this->mCreationEffect->Reference();
    this->mEffect098 = EEGraphicEffect::Get(inRecord->mL280);
    if (this->mEffect098 != NULL)
        this->mEffect098->Reference();
    this->mEffect09C = EEGraphicEffect::Get(inRecord->mL284);
    if (this->mEffect09C != NULL)
        this->mEffect09C->Reference();
    this->mEffect0A0 = EEGraphicEffect::Get(inRecord->mL288);
    if (this->mEffect0A0 != NULL)
        this->mEffect0A0->Reference();
    this->mEffect0A4 = EEGraphicEffect::Get(inRecord->mL28C);
    if (this->mEffect0A4 != NULL)
        this->mEffect0A4->Reference();
    this->mEffect0A8 = EEGraphicEffect::Get(inRecord->mL290);
    if (this->mEffect0A8 != NULL)
        this->mEffect0A8->Reference();
    this->mSpecialAttackEffect = EEGraphicEffect::Get(inRecord->mL294);
    if (this->mSpecialAttackEffect != NULL)
        this->mSpecialAttackEffect->Reference();
    this->mEffect0B0 = EEGraphicEffect::Get(inRecord->mL298);
    if (this->mEffect0B0 != NULL)
        this->mEffect0B0->Reference();
    this->mShotEffect = EEGraphicEffect::Get(inRecord->mL29C);
    if (this->mShotEffect != NULL)
        this->mShotEffect->Reference();
    this->mLeprosyEffect = EEGraphicEffect::Get(inRecord->mLeprosyEffect);
    if (this->mLeprosyEffect != NULL)
        this->mLeprosyEffect->Reference();
    this->mPlagueEffect = EEGraphicEffect::Get(inRecord->mPlagueEffect);
    if (this->mPlagueEffect != NULL)
        this->mPlagueEffect->Reference();
    this->mPulseCannonEffect = EEGraphicEffect::Get(inRecord->mL2AC);
    if (this->mPulseCannonEffect != NULL)
        this->mPulseCannonEffect->Reference();
    this->mMoraleEffect = EEGraphicEffect::Get(inRecord->mMoraleEffect);
    if (this->mMoraleEffect != NULL)
        this->mMoraleEffect->Reference();
    this->mBattleCryEffect = EEGraphicEffect::Get(inRecord->mL2B0);
    if (this->mBattleCryEffect != NULL)
        this->mBattleCryEffect->Reference();
    this->mShellHoleEffect = EEGraphicEffect::Get(inRecord->mL2B4);
    if (this->mShellHoleEffect != NULL)
        this->mShellHoleEffect->Reference();
    this->mEffect0D0 = EEGraphicEffect::Get(inRecord->mL2B8);
    if (this->mEffect0D0 != NULL)
        this->mEffect0D0->Reference();
    this->mBloodStainEffect = EEGraphicEffect::Get(inRecord->mL2BC);
    if (this->mBloodStainEffect != NULL)
        this->mBloodStainEffect->Reference();
    this->mFriendlyDamageFactor = inRecord->mFriendlyDamageFactor;
    this->mB2D1 = true;
    this->mProjectileClass = NULL;
    this->mScaffoldingClass = NULL;
    this->mDeathClass = NULL;
    this->mDeathSpawnClass = NULL;
    this->mPackedClass = NULL;
    this->mRemainsClass = NULL;
    this->mBlocksTile = inRecord->mBlocksTile;
    if (this->mAIScriptName.GetLength() > 0)
        this->mAIScript = EEUnitAction::GetScript(this->mAIScriptName);
    else
        this->mAIScript = NULL;
    this->mCommands[0] = inRecord->mCommand0;
    this->mCommands[1] = inRecord->mCommand1;
    this->mCommands[2] = inRecord->mCommand2;
    this->mCommands[3] = inRecord->mCommand3;
    this->mCommands[4] = inRecord->mCommand4;
    this->mCommands[5] = inRecord->mCommand5;
    this->mCommands[6] = inRecord->mCommand6;
    this->mCommands[7] = inRecord->mCommand7;
    this->mCommands[8] = inRecord->mCommand8;
    this->mCanChangeBehavior = false;
    for (unsigned long k = 0; k < 8; k++) {
        if (this->mCommands[k] == 0x1d) {
            this->mCanChangeBehavior = true;
            break;
        }
    }
    this->mCanPatrol = false;
    for (unsigned long m = 0; m < 8; m++) {
        if (this->mCommands[m] == 0x12 && this->FindAction(kActionGetNextPatrolPoint) != NULL) {
            this->mCanPatrol = true;
            break;
        }
    }
    this->mIsObstacle = inRecord->mIsObstacle;
    this->mIsTree = this->mResourceType == kResourceWood;
    this->mBehavior = EEUnitBehavior::sTable[inRecord->mBehavior];
}

// SYNTHETIC: EXE 0x6305d2
// (the scalar deleting destructor, emitted with the vtable; slot EEUnitClass+0x0)

// FUNCTION: EXE 0x6305ee
// Destroys the unit class, releasing its graphic effects and its projectile, scaffolding, death and remains classes
EEUnitClass::~EEUnitClass()
{
    if (this->mAttachedEffect != NULL)
        this->mAttachedEffect->Dereference();
    if (this->mDeathEffect != NULL)
        this->mDeathEffect->Dereference();
    if (this->mDamageEffect != NULL)
        this->mDamageEffect->Dereference();
    if (this->mHealEffect != NULL)
        this->mHealEffect->Dereference();
    if (this->mFireTrailEffect != NULL)
        this->mFireTrailEffect->Dereference();
    if (this->mTrailEffect != NULL)
        this->mTrailEffect->Dereference();
    if (this->mMuzzleFlashEffect != NULL)
        this->mMuzzleFlashEffect->Dereference();
    if (this->mRippleEffect != NULL)
        this->mRippleEffect->Dereference();
    if (this->mSplashEffect != NULL)
        this->mSplashEffect->Dereference();
    if (this->mImpactSparksEffect != NULL)
        this->mImpactSparksEffect->Dereference();
    if (this->mImpactBloodEffect != NULL)
        this->mImpactBloodEffect->Dereference();
    if (this->mEffect090 != NULL)
        this->mEffect090->Dereference();
    if (this->mCreationEffect != NULL)
        this->mCreationEffect->Dereference();
    if (this->mEffect098 != NULL)
        this->mEffect098->Dereference();
    if (this->mEffect09C != NULL)
        this->mEffect09C->Dereference();
    if (this->mEffect0A0 != NULL)
        this->mEffect0A0->Dereference();
    if (this->mEffect0A4 != NULL)
        this->mEffect0A4->Dereference();
    if (this->mEffect0A8 != NULL)
        this->mEffect0A8->Dereference();
    if (this->mSpecialAttackEffect != NULL)
        this->mSpecialAttackEffect->Dereference();
    if (this->mEffect0B0 != NULL)
        this->mEffect0B0->Dereference();
    if (this->mShotEffect != NULL)
        this->mShotEffect->Dereference();
    if (this->mLeprosyEffect != NULL)
        this->mLeprosyEffect->Dereference();
    if (this->mPlagueEffect != NULL)
        this->mPlagueEffect->Dereference();
    if (this->mPulseCannonEffect != NULL)
        this->mPulseCannonEffect->Dereference();
    if (this->mMoraleEffect != NULL)
        this->mMoraleEffect->Dereference();
    if (this->mBattleCryEffect != NULL)
        this->mBattleCryEffect->Dereference();
    if (this->mShellHoleEffect != NULL)
        this->mShellHoleEffect->Dereference();
    if (this->mEffect0D0 != NULL)
        this->mEffect0D0->Dereference();
    if (this->mBloodStainEffect != NULL)
        this->mBloodStainEffect->Dereference();
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

// FUNCTION: EXE 0x63086c
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

// FUNCTION: EXE 0x63094b
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

// FUNCTION: EXE 0x630c55
// Deals splash damage to every unit within inRadius of the attacker that it may target, counting kills and
// cutting the food of a hunted animal it kills
// inTarget's unit is counted first, so it is not hit twice. Each unit takes the to-hit factor times inDamage less its
// armour, scaled by the height factor (at least one point more or less) and, for the own player's units, by the
// class's friendly damage factor at
// +0x198; the 0x16f class only plays its effect on them
void EEUnitClass::ApplyAreaDamage(EEComplexUnit* inUnit, float inRadius, float inDamage, long inKind, long inColumn,
                           EEUnit* inTarget)
{
    std::vector<EEComplexUnit*> theHit;
    float theRadiusSquared = inRadius * inRadius;
    EEGraphicEffect* theEffect = NULL;
    long theX = inUnit->GetTileX();
    long theY = inUnit->GetTileY();
    EEMap* theMap = ::gWorld.mMap;
    EEUnitClass* theClass = inUnit->mLauncherClass != NULL ? inUnit->mLauncherClass : this;
    if (inTarget != NULL && inTarget->GetComplexUnit() != NULL)
        theHit.push_back(inTarget->GetComplexUnit());
    long theRange = UMCeil(inRadius);
    for (long x = theX - theRange; x <= theX + theRange; x++) {
        for (long y = theY - theRange; y <= theY + theRange; y++) {
            EEMapTile* theTile = theMap->GetTile(x, y);
            if (theTile == NULL)
                continue;
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEComplexUnit* theUnit = theNode->mUnit->GetComplexUnit();
                if (theUnit == NULL)
                    continue;
                if (std::find(theHit.begin(), theHit.end(), theUnit) != theHit.end())
                    continue;
                if (inUnit->DistanceSquaredTo(theUnit) > theRadiusSquared)
                    continue;
                if (!EEUUGetIsUnitClassTargetable(inUnit, theUnit->GetUnitClass()))
                    continue;
                if (theUnit->GetPlayer() == inUnit->GetPlayer() && !(theClass->mFriendlyDamageFactor > 0.0f))
                    continue;
                theHit.push_back(theUnit);
                if (inUnit->GetUnitClass()->GetUnitType() == kUnitTypeVolcanoProjectile && theUnit->GetEffect(3, theEffect)) {
                    if (theEffect != NULL)
                        gEffectManager->CreateUnitEffect(theUnit, theEffect, NULL, false, 0, 0);
                    continue;
                }
                float theFactor = gWeaponToHitFactors[theUnit->GetUnitClass()->mWeapon][inColumn];
                float theDamage = gToHitTable.GetAttackFactor(inKind, theUnit->GetUnitClass(), inUnit->GetPlayer()->GetPlayerID(),
                                                       theUnit->GetPlayer()->GetPlayerID()) * theFactor;
                if (theDamage <= 0.0f)
                    continue;
                theDamage = theDamage * inDamage - theUnit->GetStats()->mArmor[inColumn];
                float theModifier = this->GetElevationFactor(inUnit, theUnit);
                if (theModifier != 1.0f) {
                    if (theModifier > 1.0f) {
                        theModifier = theModifier * theDamage;
                        if (theModifier < theDamage + 1.0f)
                            theDamage = theDamage + 1.0f;
                        else
                            theDamage = theModifier;
                    } else if (theModifier < 1.0f) {
                        theModifier = theModifier * theDamage;
                        if (theModifier > theDamage - 1.0f)
                            theDamage = theDamage - 1.0f;
                        else
                            theDamage = theModifier;
                    }
                }
                if (theUnit->GetPlayer() == inUnit->GetPlayer())
                    theDamage = theClass->mFriendlyDamageFactor * theDamage;
                long theAmount = UMFloatToLong(theDamage);
                if (theAmount < 1)
                    theAmount = 1;
                long theHitPoints = theUnit->GetHitPoints();
                theUnit->TakeHit(inUnit, theAmount, inUnit->GetPlayer()->GetPlayerID(), 1);
                if (theUnit->GetHitPoints() <= 0)
                    inUnit->mExperience++;
                if (theClass->GetCategory() == kUnitCategoryLandCitizen || theHitPoints == 0 || theUnit->GetHitPoints() != 0
                    || theUnit->GetUnitClass()->GetCategory() != kUnitCategoryLand || theUnit->mResourceType != kResourceFood)
                    continue;
                long theRemaining = UMCeil(theUnit->mResourceAmount * 0.25f);
                if (theUnit->mResourceAmount != theRemaining)
                    theUnit->mResourceAmount = theRemaining;
            }
        }
    }
}

// FUNCTION: EXE 0x630fd2
// Applies inUnit's hit on inTarget: splash damage, then the direct damage, the target's reaction and the kill count
// (the volcano projectile only plays its effect)
// Damage comes from the player's upgrade of the weapon class (or the class's own values): the to-hit factor times the
// damage (scaled when inUnit or its owner is cloaked) less the armour, the height factor's at least one point more or
// less; against an own or allied unit nothing, or a tenth (rounded up) for herd animals
void EEUnitClass::ApplyHit(EEComplexUnit* inUnit, EEUnit* inTarget)
{
    EEComplexUnit* theTarget = NULL;
    EEGraphicEffect* theEffect = NULL;
    if (inTarget != NULL)
        theTarget = inTarget->GetComplexUnit();
    if (inUnit->mUnitClass->GetUnitType() == kUnitTypeVolcanoProjectile && theTarget != NULL && theTarget->GetEffect(3, theEffect)) {
        if (theEffect != NULL)
            gEffectManager->CreateUnitEffect(theTarget, theEffect, NULL, false, 0, 0);
        return;
    }
    EEUnitClass* theClass;
    if (inUnit->mUnitClass->mCategory == kUnitCategorySpaceship && inUnit->mIsSpecialAttack)
        theClass = gUnitClassTable[kUnitTypeCapitalShipSpecialWeapon];
    else
        theClass = inUnit->mLauncherClass == NULL ? this : inUnit->mLauncherClass;
    EEUnitClassUpgrade* theUpgrade = inUnit->mPlayer->GetUpgrade(theClass->mUnitType);
    float theDamage = theUpgrade != NULL ? theUpgrade->GetAttack() : theClass->mAttack;
    float theRadius = theUpgrade != NULL ? theUpgrade->mAreaEffect : theClass->mAreaEffect;
    if (theRadius != 0.0f)
        this->ApplyAreaDamage(inUnit, theRadius, theDamage, theClass->mAmmoType, theClass->mWeapon, inTarget);
    if (inTarget == NULL)
        return;
    long theHitPoints = inTarget->mHitPoints;
    float theHit;
    if (theDamage > 0.0f) {
        long theTargetPlayer = inTarget->mPlayer->GetPlayerID();
        long theUnitPlayer = inUnit->mPlayer->GetPlayerID();
        float theFactor = gWeaponToHitFactors[inTarget->mUnitClass->mWeapon][theClass->mWeapon];
        theHit = gToHitTable.GetAttackFactor(theClass->mAmmoType, inTarget->GetUnitClass(), theUnitPlayer, theTargetPlayer)
                 * theFactor;
    } else {
        theHit = 0.0f;
    }
    if (theHit <= 0.0f)
        return;
    theDamage = theHit * theDamage;
    EEComplexUnit* theCloaked = inUnit->mOwner != NULL ? inUnit->mOwner : inUnit;
    float theScaled = theCloaked->IsCloaked() ? gWorldTable.GetFloatValue2(0x58) * theDamage : theDamage;
    theDamage = theScaled - inTarget->mStats->mArmor[theClass->mWeapon];
    float theModifier = this->GetElevationFactor(inUnit, inTarget->GetComplexUnit());
    if (theModifier != 1.0f) {
        if (theModifier > 1.0f) {
            theModifier = theModifier * theDamage;
            if (theModifier >= theDamage + 1.0f)
                theDamage = theModifier;
            else
                theDamage = theDamage + 1.0f;
        } else if (theModifier < 1.0f) {
            theModifier = theModifier * theDamage;
            if (theModifier <= theDamage - 1.0f)
                theDamage = theModifier;
            else
                theDamage = theDamage - 1.0f;
        }
    }
    long theAmount = UMFloatToLong(theDamage);
    if (theAmount < 1)
        theAmount = 1;
    if (inTarget->mPlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] == 0) {
        if (inUnit->mHerd != NULL)
            theAmount = UMCeil(theAmount * k447AE4);
        else
            theAmount = 0;
    }
    inTarget->TakeHit(inUnit, theAmount, inUnit->mPlayer->GetPlayerID(), 1);
    if (inTarget->GetComplexUnit() != NULL && this->mShotEffect == NULL)
        inTarget->mUnitClass->UnitDamaged(inTarget->GetComplexUnit(), inUnit, theAmount);
    if (inTarget->mHitPoints <= 0)
        inUnit->mExperience++;
    if (theAmount <= 0 || theClass->mCategory == kUnitCategoryLandCitizen || theTarget == NULL || theHitPoints == 0 || theTarget->mHitPoints != 0
        || theTarget->mUnitClass->mCategory != kUnitCategoryLand || theTarget->mResourceType != kResourceFood)
        return;
    long theRemaining = UMFloatToLong(theTarget->mResourceAmount * 0.25f);
    if (theTarget->mResourceAmount != theRemaining)
        theTarget->mResourceAmount = theRemaining;
}

// FUNCTION: EXE 0x631312
// Fires one shot of inUnit's attack (a direct hit, a projectile or a special attack), then plays the muzzle effects,
// uses up ammunition and moves to the next weapon tag point
// The shot's target and drop point come from EEFindDropLocation; a direct shot checks the line for a blocker
// (EEIsLineToUnitBlocked: a friendly blocker only flashes, an enemy one takes the hit, open ground gets a puff); a
// projectile class launches by its turret parts; the capital ships' special attack (0x2da/0x33c) fires from the last
// tag point
void EEUnitClass::FireShot(EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    EEUnit* theBlocker = NULL;
    EEModel* theModel = inUnit->GetModel();
    long theBlockX = 0;
    long theBlockY = 0;
    unsigned long theCount;
    theCount = theModel->mTagPoints[inUnit->GetAnimation()] != NULL ? theModel->mTagPoints[inUnit->GetAnimation()]->GetNumTagPoints() : 0;
    unsigned long theNext = 0;
    bool theDone = false;
    if (inUnit->mUnitClass->GetUnitType() == kUnitTypeCpMissileBase) {
        this->LaunchStoredMissile(inUnit);
        return;
    }
    if (inUnit->mReloadTimer == 0)
        inUnit->Fn1FEA22(inUnit->mStats->mReloadTime);
    if (inUnit->mStats->mCanFireOverWalls && inUnit->mIsFiringHigh)
        inUnit->Fn1FEA22(UMFloatToLong(inUnit->mReloadTimer * 1.5f));
    EEUnit* theTarget;
    EEFindDropLocation(inUnit, &theTarget, &thePoint, NULL);
    if (this->mProjectileClass == NULL) {
        if (inUnit->mStats->mRange > 1.0f && theTarget != NULL && !inUnit->mStats->mCanFireOverWalls &&
            EEIsLineToUnitBlocked(inUnit, theTarget, &theBlocker, &theBlockX, &theBlockY)) {
            if (theBlocker != NULL) {
                theTarget = theBlocker;
                if (theBlocker->mPlayer == inUnit->mPlayer ||
                    inUnit->mPlayer->mEnemies[theBlocker->mPlayer->GetPlayerID()] == 0) {
                    if (theBlocker->mUnitClass->mImpactSparksEffect != NULL)
                        gWorld.GetEffectManager()->CreateUnitEffect(theBlocker, theBlocker->mUnitClass->mImpactSparksEffect, NULL, 0, 0, 0);
                } else {
                    this->ApplyHit(inUnit, theTarget);
                }
            } else {
                GE3DPoint theSpot((float)theBlockX + 0.5f, (float)theBlockY + 0.5f,
                                  gWorld.mMap->GetTile(theBlockX, theBlockY)->mHeight);
                gWorld.GetEffectManager()->CreatePlacedEffect(theSpot, EEGraphicEffect::Get(kGraphicEffectImpactSparks), NULL);
            }
        } else {
            this->ApplyHit(inUnit, theTarget);
        }
    } else {
        unsigned long theType = inUnit->mUnitClass->GetUnitType();
        if (theType != kUnitTypeX04RomanLegionnaire && theType != kUnitTypeSp15CapitalShip && theType != kUnitTypeCapitalShipYamato) {
            if (theModel->mTurretParts.size() == 0)
                this->LaunchShot(inUnit, theTarget, thePoint, NULL);
            else
                this->LaunchFromTurret(inUnit, theTarget, thePoint);
        } else if (!inUnit->mIsSpecialAttack) {
            this->ApplyHit(inUnit, theTarget);
        } else {
            if (theType == kUnitTypeSp15CapitalShip || theType == kUnitTypeCapitalShipYamato) {
                unsigned long thePart = inUnit->GetAnimation();
                unsigned long theLast = inUnit->GetModel()->mTagPoints[thePart]->mPoints.size() - 1;
                if (inUnit->mWeaponTagPoint != theLast)
                    inUnit->mWeaponTagPoint = theLast;
                gWorld.GetEffectManager()->CreateEffectBetweenUnits(inUnit, theTarget, this->mSpecialAttackEffect, NULL);
                this->ApplyHit(inUnit, theTarget);
                if (!inUnit->CanAttackClass(theTarget->GetUnitClass()))
                    inUnit->StartAction(kActionIdle);
            } else {
                this->LaunchShot(inUnit, theTarget, thePoint, NULL);
            }
            theDone = true;
        }
    }
    if (theCount != 0) {
        if (this->mMuzzleFlashEffect != NULL)
            gWorld.GetEffectManager()->CreateUnitEffect(inUnit, this->mMuzzleFlashEffect, NULL, 0, 0, 0);
        if (inUnit->mUnitClass->mCategory == kUnitCategoryLandCitizen && inUnit->mPlayer->mTechnologies->mCurrentEpoch < kTechEpochNano)
            theDone = true;
        if (this->mShotEffect != NULL && !theDone) {
            if (theTarget != NULL)
                gWorld.GetEffectManager()->CreateEffectBetweenUnits(inUnit, theTarget, this->mShotEffect, NULL);
            else
                gWorld.GetEffectManager()->CreateEffect(inUnit, thePoint, this->mShotEffect, NULL, 0);
        }
    }
    if (this->mUsesAmmunition) {
        if (inUnit->mAmmunition > 1) {
            inUnit->SetAmmunition(inUnit->mAmmunition - 1);
        } else {
            inUnit->SetAmmunition(0);
            inUnit->SetFlightTime(0);
        }
    }
    if (inUnit->mIsSpecialAttack && (inUnit->mUnitClass->GetUnitType() == kUnitTypeSp15CapitalShip || inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitalShipYamato))
        inUnit->mIsSpecialAttack = false;
    if (theCount != 0) {
        theNext = (inUnit->mWeaponTagPoint + 1) % theCount;
        if ((inUnit->mUnitClass->GetUnitType() == kUnitTypeSp15CapitalShip || inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitalShipYamato) &&
            theNext == theCount - 1)
            theNext = 0;
    }
    if (inUnit->mWeaponTagPoint != theNext)
        inUnit->mWeaponTagPoint = theNext;
}

// FUNCTION: EXE 0x631723
// Returns whether units of this class may board a container of inClass (transport, fortress, carrier, airport ...)
bool EEUnitClass::CanEnter(EEUnitClass* inClass)
{
    return (this->mCanEnterAirTransports && inClass->mCategory == kUnitCategoryHelicopterTransport) || (this->mCanEnterLandTransports && inClass->mCategory == kUnitCategoryLandTransport) ||
           (this->mCanEnterSeaTransports && inClass->mCategory == kUnitCategoryNavalTransport) || (this->mCanEnterSpaceTransports && inClass->mCategory == kUnitCategorySpaceTransport) ||
           (this->mCanEnterFortresses && inClass->mCategory == kUnitCategoryFortress) || (this->mCanEnterCarriers && inClass->mAIKind == kAIKindAircraftCarrier) ||
           (this->mCanEnterAirports && inClass->mUnitType == kUnitTypeAirport) || (this->mCanEnterNavyYards && inClass->mUnitType == kUnitTypeNavyYard) ||
           (this->mUnitType == kUnitTypeSp15SpaceFighter && inClass->mUnitType == kUnitTypeSp15SpaceCarrier);
}

// FUNCTION: EXE 0x6317d7
// Returns whether the class is in the unit set of the calamity flag (plague, leprosy, fire, AE damage, mind control)
bool EEUnitClass::IsAffectedByCalamity(unsigned long inFlag)
{
    bool theResult = false;
    EECalamityClass* theClass = NULL;
    switch (inFlag) {
    case 1:
        theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(kCalamityPlague);
        break;
    case 2:
        theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(kCalamityLeprosy);
        break;
    case 4:
        theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(kCalamityFire);
        break;
    case 8:
        theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(kCalamityAEExtraDamage);
        break;
    case 0x10:
        theClass = USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(kCalamityAEMindControl);
        break;
    }
    if (theClass != NULL) {
        EEUnitSet* theSet = gUnitSetManager.GetSet(theClass->GetUnitSet());
        if (theSet != NULL && theSet->Contains(this->mUnitType))
            theResult = true;
        else
            theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x63187a
// Returns the high-ground damage factor of a land attacker (or a projectile's launcher) on a land unit or building
float EEUnitClass::GetElevationFactor(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    long theCategory = this->mCategory;
    long theTargetCategory = inTarget->mUnitClass->mCategory;
    bool theOwnHeight;
    if (theCategory == kUnitCategoryProjectile || theCategory == kUnitCategoryBomb || theCategory == kUnitCategoryMissile || theCategory == kUnitCategoryTorpedo) {
        theCategory = inUnit->mLauncherClass->mCategory;
        theOwnHeight = false;
    } else
        theOwnHeight = true;
    bool theIsLand = theCategory == kUnitCategoryLand || theCategory == kUnitCategoryLandCitizen;
    bool theIsTarget = theTargetCategory == kUnitCategoryLand || theTargetCategory == kUnitCategoryBuilding || theTargetCategory == kUnitCategoryLandCitizen ||
                       theTargetCategory == kUnitCategoryLandPriest || theTargetCategory == kUnitCategoryBuilding19 || theTargetCategory == kUnitCategoryLand15 ||
                       theTargetCategory == kUnitCategoryLandMilitaryHero || theTargetCategory == kUnitCategoryLandTransport || theTargetCategory == kUnitCategoryFortress ||
                       theTargetCategory == kUnitCategoryMiningBuilding || theTargetCategory == kUnitCategoryBuildingPriest || theTargetCategory == kUnitCategoryGate;
    float theResult;
    if (theIsLand && theIsTarget && inTarget != NULL) {
        float theDifference = (theOwnHeight ? inUnit->mPhysicalModel.GetLocation().GetZ() : inUnit->mHeight) -
                              inTarget->mPhysicalModel.GetLocation().GetZ();
        if (theDifference >= gWorldTable.GetFloatValue(53) * k447ADC) {
            float theBonus = inUnit->mPlayer->mHighGroundBonus;
            theResult = theBonus + gWorldTable.GetFloatValue(55);
        } else if (theDifference <= -(gWorldTable.GetFloatValue(54) * k447ADC))
            theResult = gWorldTable.GetFloatValue(56);
        else
            theResult = 1.0f;
    } else
        theResult = 1.0f;
    return theResult;
}

// FUNCTION: EXE 0x63198b
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
        else if (this->mB315)
            outFlags.mFlags[kAITileForest] = true;
        if (theSet->Contains(this->mUnitType))
            outFlags.mFlags[kAITileGate] = true;
    } else
        memset(outFlags.mFlags, 1, sizeof(outFlags.mFlags));
    outFlags.mFlags[kAITileSpace] = this->CanMoveOnTerrain(inUnit, 0xc);
}

// FUNCTION: EXE 0x631ad2
// Computes the volume, pan and pitch of a sound played at the unit's position
void EEUnitClass::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
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

// FUNCTION: EXE 0x631b55
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
    if (theNeeded > theLimit) {
        theSuccess = false;
    } else {
        if ((inUnit->GetUnitClass()->GetUnitType() == kUnitTypeCpMissileBase && inClass->GetCategory() == kUnitCategoryMissile) ||
            inClass->IsAircraft() || inClass->GetCategory() == kUnitCategoryProjectile) {
            EEAirTrafficControl* theTraffic = inClass->mAirTrafficType != 0 ? thePlayer->mAirTraffic[inClass->mAirTrafficType - 1] : 0;
            bool theCanLoad;
            if (theTraffic != NULL) {
                theNeeded = inClass->mWeight;
                theCanLoad = theTraffic->mUsedCargo + theNeeded <= theTraffic->mMaxCargo;
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
                        EEUnit* theTarget = dynamic_cast<EEUGAttackUnit*>(theGoal)->mTarget;
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
                std::deque<EEUnitGoal*>& theGoals = inUnit->GetQueueGoals(theQueue);
                for (std::deque<EEUnitGoal*>::iterator i = theGoals.begin(); i != theGoals.end(); ++i) {
                    EEUnitGoal* theGoal;
                    if ((*i)->Suits(theCreated)) {
                        if ((*i)->mKind == kGoalBuildRepair) {
                            theCreated->AddBuildTarget(dynamic_cast<EEUGBuildRepair*>(*i)->mTarget);
                            continue;
                        } else if ((*i)->mKind == kGoalLoadGarrison) {
                            EEUGLoadGarrison* theLoad = dynamic_cast<EEUGLoadGarrison*>(*i);
                            if (theCreated->GetUnitClass()->mCanGarrison) {
                                theGoal = theLoad->Clone();
                                if (theGoal->IsLoadGoal())
                                    theGoal->AddCreatedUnit(theCreated);
                                if (theLoad->mContainer->mActiveGoal != NULL)
                                    dynamic_cast<EEUGLoadGarrison*>(theLoad->mContainer->mActiveGoal)->AddUnit(theCreated);
                            } else {
                                if (theLoad->mContainer == NULL)
                                    continue;
                                theLoad->mContainer->GetPosition(theGoalPosition);
                                theGoal = new EEUGLocation(&theGoalPosition, false, true, false, 0.0f, false, 0);
                            }
                        } else {
                            theGoal = (*i)->Clone();
                            if (theGoal->IsLoadGoal())
                                theGoal->AddCreatedUnit(theCreated);
                        }
                    } else {
                        (*i)->GetGoalLocation(theGoalPosition);
                        theGoal = new EEUGLocation(&theGoalPosition, false, true, false, 0.0f, false, 0);
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
                                static_cast<EEUGLocation*>(theGoal)->mLocation = theAdjusted.GetPosition();
                        }
                    }
                    if (theGoal->mKind == kGoalAttackUnit && (theCreated->GetUnitClass()->GetCategory() == kUnitCategoryLandCitizen || theCreated->GetUnitClass()->GetCategory() == kUnitCategoryNavalCitizen)) {
                        EEUnit* theTarget = dynamic_cast<EEUGAttackUnit*>(theGoal)->mTarget;
                        if (theCreated->CanGather(theTarget, theTarget->GetTileX(), theTarget->GetTileY(), false, true)) {
                            EEUnitGoal* theGather = new EEUGGather(theTarget, 0, true);
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
    }
    return theSuccess;
}

// FUNCTION: EXE 0x63241c
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

// FUNCTION: EXE 0x6325fd
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

// FUNCTION: EXE 0x6327ff
// Creates a unit of the class at a transformation for a player and adds it to the world (an airport gets its runways)
EEUnit* EEUnitClass::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
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

// FUNCTION: EXE 0x6329f7
// Creates a unit of the class at a tile for a player (yaw pi) and adds it to the world (an airport gets its runways)
EEUnit* EEUnitClass::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
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
    theUnit->Dereference();
    return theUnit;
}

// FUNCTION: EXE 0x632c9f
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

// FUNCTION: EXE 0x632e4a
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

// FUNCTION: EXE 0x63302d
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

// FUNCTION: EXE 0x633122
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

// FUNCTION: EXE 0x633210
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

// FUNCTION: EXE 0x633306
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

// FUNCTION: EXE 0x6333fb
// Releases every unit class in the class table
void EEUnitClass::ReleaseUnitClasses()
{
    for (unsigned long i = 0; i < 0x367; i++)
        EEUnitClass::ReleaseUnitClass(i);
}

// FUNCTION: EXE 0x633410
// Returns the unit a projectile hits: its attack target first, then the units of its region in storage order (walls,
// enemies); a hit on a non-target wall makes the launcher fire high
EEUnit* EEUnitClass::FindImpactUnit(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    GE3DLine theLine;
    GEVector theDirection;
    EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
    EEComplexUnit* theResult = NULL;
    const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
    thePosition.SetX(theLocation.GetX());
    thePosition.SetY(theLocation.GetY());
    thePosition.SetZ(theLocation.GetZ());
    const GE3DPoint& theGoal = inUnit->mInitialContactLocation;
    theDirection.SetX(theGoal.GetX() - thePosition.GetX());
    theDirection.SetY(theGoal.GetY() - thePosition.GetY());
    theDirection.SetZ(theGoal.GetZ() - thePosition.GetZ());
    bool theSegment = theDirection.GetMagnitudeSquared() >
        inUnit->mPhysicalModel.GetScale() * inUnit->mPhysicalModel.GetScale();
    if (theSegment) {
        theLine.SetStart(thePosition);
        theLine.SetDirection(theDirection);
    }
    if (theAttack != NULL && theAttack->mTarget != NULL) {
        EEComplexUnit* theTarget = theAttack->mTarget->GetComplexUnit();
        if (theTarget != NULL && theTarget->mPhysicalModel.GetModel() != NULL) {
            if (inUnit->mPhysicalModel.GetLocation().DistanceSquaredTo(theTarget->mPhysicalModel.GetLocation()) <=
                (inUnit->GetStats()->GetScale() + theTarget->GetStats()->GetScale()) *
                (inUnit->GetStats()->GetScale() + theTarget->GetStats()->GetScale())) {
                unsigned long theLOD = theTarget->GetStats()->GetScale() < kLargeUnitScale ? 1 : 2;
                if (theTarget->mIsBuilt) {
                    if (inUnit->mPhysicalModel.Intersects(&theTarget->mPhysicalModel, theLine,
                        theSegment, theLOD, 1.0f, 1.0f, 1.0f))
                        theResult = theTarget;
                } else {
                    float theHealth = (float)theTarget->GetHitPoints() / (float)theTarget->GetStats()->mMaxHitPoints;
                    if (theHealth != 0.0f) {
                        if (theHealth < kMinConstructionScale)
                            theHealth = kMinConstructionScale;
                        if (inUnit->mPhysicalModel.Intersects(&theTarget->mPhysicalModel, theLine,
                            theSegment, theLOD, 1.0f, 1.0f, theHealth))
                            theResult = theTarget;
                    }
                }
            }
        }
    }
    if (theResult == NULL) {
        if (inUnit->GetUnitClass()->GetUnitType() == kUnitTypeFlamingArrow || inUnit->GetUnitClass()->GetUnitType() == kUnitTypePilum ||
            inUnit->GetUnitClass()->GetUnitType() == kUnitTypeCpICBM)
            return theResult;
        RTSRegionTable<EEComplexUnit*>* theTable = ::gWorld.mMap->mUnitRegions;
        long theRegionX = inUnit->GetTileX() / theTable->mRegionSize;
        long theRegionY = inUnit->GetTileY() / theTable->mRegionSize;
        std::vector<EEComplexUnit*>* theRegion = theTable->GetRegion(theRegionX, theRegionY);
        if (theRegion == NULL)
            return theResult;
        unsigned long theCount = theRegion->size();
        for (unsigned long i = 0; i < theCount && theResult == NULL; ++i) {
            EEComplexUnit* theCandidate = (*theRegion)[i];
            if (!EEUUGetIsUnitClassTargetable(NULL, theCandidate->GetUnitClass()) || theCandidate == inUnit->mOwner)
                continue;
            EEUnitClass* theClass = theCandidate->GetUnitClass();
            if ((theClass->mIsParatrooperBomber && inUnit->mOwner->GetUnitClass()->mIsParatrooperBomber) ||
                theCandidate->mPhysicalModel.GetModel() == NULL)
                continue;
            unsigned long theLOD = 1;
            if (theCandidate->GetStats()->GetScale() >= kLargeUnitScale)
                theLOD = 2;
            if (theClass->mIsWall) {
                if (theCandidate->IsPassableBy(inUnit))
                    continue;
                EEComplexUnit* theGoalCarrier;
                if (theAttack != NULL && (theGoalCarrier = theAttack->mTarget->GetComplexUnit()) != 0) {
                    EEComplexUnit* theNext;
                    while ((theNext = theGoalCarrier->mPreviousPart) != 0)
                        theGoalCarrier = theNext;
                } else
                    theGoalCarrier = NULL;
                EEComplexUnit* theCarrier = theCandidate;
                while (theCarrier->mPreviousPart != NULL)
                    theCarrier = theCarrier->mPreviousPart;
                if (theGoalCarrier != theCarrier && inUnit->mOwner->GetStats()->mCanFireOverWalls && inUnit->mOwner->mIsFiringHigh)
                    continue;
                if (theCandidate->mIsBuilt) {
                    if (inUnit->mPhysicalModel.Intersects(&theCandidate->mPhysicalModel, theLine,
                        theSegment, theLOD, 1.0f, 1.0f, 1.0f))
                        theResult = theCandidate;
                } else {
                    float theHealth = (float)theCandidate->GetHitPoints() / (float)theCandidate->GetStats()->mMaxHitPoints;
                    if (theHealth == 0.0f)
                        continue;
                    if (theHealth < kMinConstructionScale)
                        theHealth = kMinConstructionScale;
                    if (inUnit->mPhysicalModel.Intersects(&theCandidate->mPhysicalModel, theLine,
                        theSegment, theLOD, 1.0f, 1.0f, theHealth))
                        theResult = theCandidate;
                }
            } else {
                if (theCandidate->GetPlayer() == inUnit->GetPlayer() ||
                    inUnit->GetPlayer()->mEnemies[theCandidate->GetPlayer()->GetPlayerID()] == 0 ||
                    theCandidate->GetHitPoints() <= 0 || theClass->mIsProjectile)
                    continue;
                unsigned long theCategory = theClass->GetCategory();
                if ((inUnit->GetUnitClass()->GetCategory() != kUnitCategoryBomb && inUnit->GetUnitClass()->GetUnitType() != kUnitTypeMeteor &&
                    (theCategory == kUnitCategoryBuilding || theCategory == kUnitCategoryBuilding19 || theCategory == kUnitCategoryMiningBuilding || theCategory == kUnitCategoryFortress)) ||
                    theClass->GetUnitType() == kUnitTypeResFish)
                    continue;
                if (theCandidate->mIsBuilt) {
                    if (inUnit->mPhysicalModel.Intersects(&theCandidate->mPhysicalModel, theLine,
                        theSegment, theLOD, 1.0f, 1.0f, 1.0f))
                        theResult = theCandidate;
                } else {
                    float theHealth = (float)theCandidate->GetHitPoints() / (float)theCandidate->GetStats()->mMaxHitPoints;
                    if (theHealth == 0.0f)
                        continue;
                    if (theHealth < kMinConstructionScale)
                        theHealth = kMinConstructionScale;
                    if (inUnit->mPhysicalModel.Intersects(&theCandidate->mPhysicalModel, theLine,
                        theSegment, theLOD, 1.0f, 1.0f, theHealth))
                        theResult = theCandidate;
                }
            }
        }
    }
    if (theResult != NULL && theAttack != NULL && inUnit->mOwner != NULL && theResult != theAttack->mTarget &&
        gUnitSetManager.Contains(theResult->GetUnitClass()->GetUnitType(), 0x2a) && inUnit->mOwner->mIsFiringHigh != true)
        inUnit->mOwner->mIsFiringHigh = true;
    return theResult;
}

// FUNCTION: EXE 0x633a14
// Returns whether the unit faces the target unit (or its tile's centre)
bool EEUnitClass::FacesTarget(EEComplexUnit* inUnit, EEUnit* inTarget, bool inUseTolerance)
{
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
}

// FUNCTION: EXE 0x633a72
// Returns whether the unit (or its turrets) faces a point within its firing arc or the fixed tolerance; broadside
// ships also count the opposite side
bool EEUnitClass::FacesPoint(EEComplexUnit* inUnit, const GE3DPoint& inLocation, bool inUseTolerance)
{
    GETransformation theLocation(inUnit->mPhysicalModel.GetLocation());
    GE3DPoint thePosition(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    GE3DPoint theTarget(inLocation);
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theRotated;
    GEVector theForward;
    GEVector theDirection;
    EEGraphicObject* theModel = inUnit->GetModel();
    float theTolerance = this->mFiringArc;
    if ((inUnit->mUnitClass->GetUnitType() == kUnitTypeSp15CapitalShip || inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitalShipYamato) && inUnit->mIsSpecialAttack)
        theTolerance = 0.5235988f;
    if (theModel->GetNumParts() > 0)
        return this->TurretsFacePoint(inUnit, theTarget, inUseTolerance);
    if ((theTarget.GetX() == thePosition.GetX() && theTarget.GetY() == thePosition.GetY()) ||
        theTolerance == 6.2831855f)
        return true;
    GETransformation theRotation;
    theDirection.SetX(theTarget.GetX() - thePosition.GetX());
    theDirection.SetY(theTarget.GetY() - thePosition.GetY());
    theDirection.Normalize();
    theRotation.SetOrientationYPR(this->mFacingOffset, 0.0f, 0.0f);
    theRotation.Apply(theUp, theRotated);
    theLocation.Apply(theRotated, theForward);
    theForward.SetZ(0.0f);
    theForward.Normalize();
    float theDot = theForward.DotProduct(theDirection);
    if (theDot > 1.0f)
        theDot = 1.0f;
    else if (theDot < -1.0f)
        theDot = -1.0f;
    float theAngle = UMAbs(UMArcCos(theDot));
    if (!this->mFiresBroadside) {
        if (inUseTolerance)
            return theAngle <= theTolerance * 0.5f;
        return theAngle <= kFacingTolerance;
    }
    float theOppositeAngle = UMAbs(theAngle - 3.1415927f);
    if (inUseTolerance) {
        float theHalfTolerance = theTolerance * 0.5f;
        return theAngle <= theHalfTolerance || theOppositeAngle <= theHalfTolerance;
    }
    return theAngle <= kFacingTolerance || theOppositeAngle <= kFacingTolerance;
}

// FUNCTION: EXE 0x633ccc
// Returns whether every turret of the unit faces a point within the firing arc or the fixed tolerance
bool EEUnitClass::TurretsFacePoint(EEComplexUnit* inUnit, const GE3DPoint& inPoint, bool inFlag)
{
    GE3DPoint theOffset;
    GE3DPoint theWorldPosition;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theRotated, thePartFacing, theWorldFacing, theDirection;
    GETransformation theRotation;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEModel* theModel = inUnit->GetModel();
    unsigned long theCount = theModel->mTurretParts.size();
    bool theResult = true;
    for (unsigned long i = 0; i < theCount; ++i) {
        if (!theResult) return false;
        EETurretPart* thePart = *(theModel->mTurretParts.begin() + i);
        theOffset = thePart->mPosition;
        theOffset.SetX(theStats->GetScale() * theOffset.GetX());
        theOffset.SetY(theStats->GetScale() * theOffset.GetY());
        theOffset.SetZ(theStats->GetScale() * theOffset.GetZ());
        inUnit->mPhysicalModel.GetLocation().Apply(theOffset, theWorldPosition);
        theDirection.SetX(inPoint.GetX() - theWorldPosition.GetX());
        theDirection.SetY(inPoint.GetY() - theWorldPosition.GetY());
        theDirection.SetZ(0.0f);
        theDirection.Normalize();
        GEModel* thePartModel = theModel->GetTurretPartModel(i);   // unused: the original makes the call (0x233e02) and drops the result
        theRotation.SetOrientationYPR(inUnit->mTurretAngles[i], 0.0f, 0.0f);
        theRotation.Apply(theForward, theRotated);
        theModel->GetTurretPartTransformation(i).Apply(theRotated, thePartFacing);
        inUnit->mPhysicalModel.GetLocation().Apply(thePartFacing, theWorldFacing);
        theWorldFacing.SetZ(0.0f);
        theWorldFacing.Normalize();
        float theDot = theWorldFacing.GetZ() * theDirection.GetZ() +
            theWorldFacing.GetY() * theDirection.GetY() + theDirection.GetX() * theWorldFacing.GetX();
        if (theDot > 1.0f) theDot = 1.0f;
        else if (!(theDot >= -1.0f)) theDot = -1.0f;
        float theAngle = UMArcCos(theDot);
        if (!(theAngle >= 0.0f)) theAngle = -theAngle;
        if (inFlag) theResult = !(theAngle > this->mFiringArc * 0.5f);
        else theResult = !(theAngle > kFacingTolerance);
    }
    return theResult;
}

// FUNCTION: EXE 0x633f0e
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

// FUNCTION: EXE 0x634000
// Sends a computer player's unit to an escape point, when one is found
void EEUnitClass::FindSafeLocation(EEComplexUnit* inUnit)
{
    GE2DPoint theLocation;
    if (inUnit->mPlayer->mComputerPlayer == NULL || !EEGetEscapePoint(inUnit, theLocation))
        return;
    inUnit->DropPath();
    inUnit->SetGoal(new EEUGLocation(theLocation, false, true, false, 0.0f, false, false), 0, 0);
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x634084
// Plays the class's creation effect on the unit
void EEUnitClass::PlayCreationEffect(EEUnit* inUnit)
{
    if (this->mCreationEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, this->mCreationEffect, NULL, 0, 0, 0);
}

// FUNCTION: EXE 0x6340a7
// Launches a projectile (or carried cargo) from the unit's weapon tag point toward the target or ground point; the
// volcano idles after each launch
void EEUnitClass::LaunchShot(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation, EEComplexUnit* inCargo)
{
    GE3DPoint theOffset;
    GE3DPoint thePosition;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theFacing;
    GEVector theDirection;
    GETransformation theTransform(inUnit->mPhysicalModel.GetLocation());
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EETagPointList* thePoints = inUnit->GetModel()->mTagPoints[inUnit->mAnimation];
    EEUnitClass* theClass = this->mProjectileClass;
    EEUnit* theCreated = NULL;
    if (thePoints != NULL && thePoints->GetNumTagPoints() != 0) {
        unsigned long theIndex = inUnit->mWeaponTagPoint;
        if ((inUnit->mUnitClass->GetUnitType() == kUnitTypeSp15CapitalShip || inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitalShipYamato) && inUnit->mIsSpecialAttack)
            theIndex = inUnit->GetModel()->mTagPoints[inUnit->GetAnimation()]->GetNumTagPoints() - 1;
        thePoints->GetTagPoint(theIndex)->GetOffset(inUnit->mPhysicalModel.GetFrame(), theOffset);
    } else {
        theOffset.SetX(0.0f);
        theOffset.SetY(0.0f);
        theOffset.SetZ(0.5f);
    }
    theOffset.SetX(theStats->GetScale() * theOffset.GetX());
    theOffset.SetY(theStats->GetScale() * theOffset.GetY());
    theOffset.SetZ(theStats->GetScale() * theOffset.GetZ());
    theTransform.Apply(theOffset, thePosition);
    theTransform.SetPosition(thePosition);
    if (this->mCategory == kUnitCategoryBuilding) {
        theDirection.SetX(inLocation.GetX() - theTransform.GetX());
        theDirection.SetY(inLocation.GetY() - theTransform.GetY());
        theDirection.SetZ(0.0f);
        theDirection.Normalize();
        float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
        theTransform.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    } else if (this->mProjectileClass == NULL || this->mProjectileClass->mCategory != kUnitCategoryBomb) {
        theDirection.SetX(inLocation.GetX() - theTransform.GetX());
        theDirection.SetY(inLocation.GetY() - theTransform.GetY());
        theDirection.SetZ(0.0f);
        theDirection.Normalize();
        float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
        theTransform.Apply(theForward, theFacing);
        float theDifference = theYaw - UMArcTan2(-theFacing.GetX(), theFacing.GetY());
        if (theDifference < -3.1415927f) theDifference += 6.2831855f;
        else if (theDifference > 3.1415927f) theDifference -= 6.2831855f;
        theTransform.Revolve(theDifference, 0.0f, 0.0f);
    }
    long theX = UMFloor(theTransform.GetX());
    long theY = UMFloor(theTransform.GetY());
    if (::gWorld.mMap->RTSMap::GetTile(theX, theY) == NULL) {
        thePosition.SetX(theTransform.GetX());
        thePosition.SetY(theTransform.GetY());
        thePosition.SetZ(theTransform.GetZ());
        ::gWorld.mMap->GetClosestTile(&thePosition, &theX, &theY);
    }
    if (inUnit->mIsSpecialAttack) {
        if (this->mHasSpecialAttack && inUnit->mUnitClass->GetUnitType() != kUnitTypeX04RomanLegionnaire && inUnit->mUnitClass->GetUnitType() != kUnitTypeSp15CapitalShip &&
            inUnit->mUnitClass->GetUnitType() != kUnitTypeCapitalShipYamato) {
            theClass = gUnitClassTable[gCivPowersManager.GetPowerRecord(kCivPowerFlamingArrows)->mUnitType];
            theCreated = theClass->CreateUnit(theX, theY, inUnit->mPlayer->GetPlayerID(), false, inUnit);
        }
        if (!inUnit->CanAttackClass(inTarget->GetUnitClass()))
            inUnit->StartAction(kActionIdle);
        inUnit->mIsSpecialAttack = false;
    }
    if (inCargo != NULL) {
        theCreated = inCargo;
        theClass = inCargo->mUnitClass;
        inCargo->MoveTo(theX, theY, true);
    }
    if (theCreated == NULL) {
        theCreated = this->mProjectileClass->CreateUnit(theX, theY, inUnit->mPlayer->GetPlayerID(), false, inUnit);
        theClass = this->mProjectileClass;
    }
    if (inUnit->mUnitClass->mIsParatrooperBomber)
        EEMovePassengers(theCreated->GetComplexUnit(), inUnit, false);
    theCreated->GetComplexUnit()->mPhysicalModel.SetLocation(theTransform);
    if (this->mCreationEffect != NULL)
        gEffectManager->CreateUnitEffect(theCreated, this->mCreationEffect, NULL, false, 0, 0);
    theCreated->GetComplexUnit()->SetLauncherClass(this);
    theCreated->GetComplexUnit()->SetOwner(inUnit);
    EEUnitGoal* theGoal;
    if (inTarget != NULL) theGoal = new EEUGAttackUnit(theCreated->GetComplexUnit(), inTarget, false, false);
    else theGoal = new EEUGAttackGround(inLocation, false);
    theCreated->GetComplexUnit()->SetGoal(theGoal, false, false);
    theCreated->GetComplexUnit()->SetVelocity(inUnit->mVelocity);
    theCreated->GetComplexUnit()->SetHeight(inUnit->mPhysicalModel.GetLocation().GetZ());
    theClass->LaunchProjectile(theCreated->GetComplexUnit(), inLocation);
    if (inUnit->mUnitClass->mUnitType == kUnitTypeVolcano)   // the unit type (was mCategory, which never is 0x16e)
        inUnit->StartAction(kActionIdle);
}

// FUNCTION: EXE 0x6345d2
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
            EEUnitGoal* theGoal = new EEUGLocation(theDestination, true, true, false, 0.0f, false, 0);
            inUnit->SetGoal(theGoal, theOldGoal != NULL && theOldGoal->mKind == kGoalExplore, 0);
            inUnit->StartAction(kActionPrepareToMove);
            this->ActionChanged(inUnit);
            return;
        }
    }
    inUnit->StartAction(kActionIdle);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x634779
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

// FUNCTION: EXE 0x634965
// Lists the ring of tiles around the unit's footprint (optionally with corners, appending or replacing)
void EEUnitClass::GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    if (!inAppend && outTiles.size() > 0)
        outTiles.clear();
    EEGetSquareBlockRing(inCorners, inUnit->GetTileX(), inUnit->GetTileY(), this->GetTileSize(), outTiles, inAppend);
}

// FUNCTION: EXE 0x6349b1
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

// FUNCTION: EXE 0x634a05
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

// FUNCTION: EXE 0x634ad8
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

// FUNCTION: EXE 0x634b05
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
            if (theContinent != NULL && theTile != NULL && theContinent->mCategory == kContinentCategoryLand && theTile->mTerrainType != kTerrainCliff &&
                (theTargetContinent == NULL || theContinent == theTargetContinent))
                theFound = true;
            else
                theFound = false;
        }
    }
    return theContinent;
}

// FUNCTION: EXE 0x634be6
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

// FUNCTION: EXE 0x634ea2
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

// FUNCTION: EXE 0x634ee7
// Lists every unit class whose unit type is in a unit set
void EEUnitClass::GetClassesInSet(unsigned long inSet, std::vector<EEUnitClass*>& outClasses)
{
    outClasses.clear();
    for (long i = 0; i < 0x367; i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass != NULL && gUnitSetManager.Contains(theClass->GetUnitType(), inSet) == true)
            outClasses.push_back(theClass);
    }
}

// FUNCTION: EXE 0x634f34
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

// FUNCTION: EXE 0x634fbd
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
#include "ee/EEUGHealUnit.h"

// FUNCTION: EXE 0x634fff
// Heals the heal goal's target when in range: plays the healing animation and the class's heal effect
// The heal effect is drawn between the two units or on the target, by the graphic effect's kind
void EEUnitClass::Heal(EEComplexUnit* inUnit)
{
    EEEffectHandle theHandle;
    theHandle.mValue = 0;
    EEUGHealUnit* theGoal = static_cast<EEUGHealUnit*>(inUnit->GetGoal());
    inUnit->DropPath();
    if (theGoal == NULL || !theGoal->IsInRange(inUnit, theGoal->GetTarget()))
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
        if (gEffectManager->CreateEffectBetweenUnits(inUnit, theGoal->GetTarget(), theEffect, &theHandle) != 0)
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
        if (gEffectManager->CreateUnitEffect(theGoal->GetTarget(), theEffect, &theHandle, false, 0, 0) != 0)
            return;
        break;
    }
    if (inUnit->mHealEffect != theHandle)
        inUnit->mHealEffect = theHandle;
}

// FUNCTION: EXE 0x63511e
// Sets up a new unit: full hit points, ammunition, behaviour, heal timer, standing still and idle
void EEUnitClass::UnitCreated(EEComplexUnit* inUnit)
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

// FUNCTION: EXE 0x6351ba
// Gives a new simple unit its full hit points
void EEUnitClass::SimpleUnitCreated(EEUnit* inUnit)
{
    inUnit->SetHitPoints(inUnit->mStats->mMaxHitPoints);
}

// FUNCTION: EXE 0x6351cf
// Adds a placed unit (not a path marker) to the map's unit region of its tile
void EEUnitClass::AddToRegion(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    U2DPoint<long> theRegion(0, 0);
    RTSRegionTable<EEComplexUnit*>* theTable = theMap->mUnitRegions;
    if (inUnit->mTileNode == NULL || inUnit->IsPathMarker())
        return;
    theRegion = U2DPoint<long>(inUnit->GetTileX() / theTable->mRegionSize, inUnit->GetTileY() / theTable->mRegionSize);
    EEComplexUnit* theUnit = inUnit->GetComplexUnit();
    std::vector<EEComplexUnit*>* theList = theTable->GetRegion(theRegion);
    if (theList != NULL)
        theList->push_back(theUnit);
}

// FUNCTION: EXE 0x63523c
// Adds the unit to its tile's unit list
void EEUnitClass::AddToTile(EEUnit* inUnit)
{
    RTSTile* theTile = inUnit->mTile;
    if (theTile != NULL)
        theTile->mUnits = theTile->AddUnit(inUnit, theTile->mUnits);
}

// FUNCTION: EXE 0x63525a
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

// FUNCTION: EXE 0x6353bb
// Returns whether the unit has reached its current waypoint, given the distance left
bool EEUnitClass::HasArrived(EEComplexUnit* inUnit, float inValue)
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
            if (inValue < 0.05f)
                theResult = true;
            else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x6354ae
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

// FUNCTION: EXE 0x635527
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

// FUNCTION: EXE 0x6355e3
// Loads the class's graphic effects and marks its related classes (projectile, death, farms, runways ...) for loading
void EEUnitClass::LoadMedia(EEPlayer* inPlayer)
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
    if (this->mAttachedEffect != NULL)
        this->mAttachedEffect->Load(false);
    if (this->mDeathEffect != NULL)
        this->mDeathEffect->Load(false);
    if (this->mDamageEffect != NULL)
        this->mDamageEffect->Load(false);
    if (this->mHealEffect != NULL)
        this->mHealEffect->Load(false);
    if (this->mFireTrailEffect != NULL)
        this->mFireTrailEffect->Load(false);
    if (this->mTrailEffect != NULL)
        this->mTrailEffect->Load(false);
    if (this->mMuzzleFlashEffect != NULL)
        this->mMuzzleFlashEffect->Load(false);
    if (this->mRippleEffect != NULL)
        this->mRippleEffect->Load(false);
    if (this->mSplashEffect != NULL)
        this->mSplashEffect->Load(false);
    if (this->mImpactSparksEffect != NULL)
        this->mImpactSparksEffect->Load(false);
    if (this->mImpactBloodEffect != NULL)
        this->mImpactBloodEffect->Load(false);
    if (this->mEffect090 != NULL)
        this->mEffect090->Load(false);
    if (this->mCreationEffect != NULL)
        this->mCreationEffect->Load(false);
    if (this->mEffect098 != NULL)
        this->mEffect098->Load(false);
    if (this->mEffect09C != NULL)
        this->mEffect09C->Load(false);
    if (this->mEffect0A0 != NULL)
        this->mEffect0A0->Load(false);
    if (this->mEffect0A4 != NULL)
        this->mEffect0A4->Load(false);
    if (this->mEffect0A8 != NULL)
        this->mEffect0A8->Load(false);
    if (this->mSpecialAttackEffect != NULL)
        this->mSpecialAttackEffect->Load(false);
    if (this->mEffect0B0 != NULL)
        this->mEffect0B0->Load(false);
    if (this->mShotEffect != NULL)
        this->mShotEffect->Load(false);
    if (this->mLeprosyEffect != NULL)
        this->mLeprosyEffect->Load(false);
    if (this->mPlagueEffect != NULL)
        this->mPlagueEffect->Load(false);
    if (this->mPulseCannonEffect != NULL)
        this->mPulseCannonEffect->Load(false);
    if (this->mMoraleEffect != NULL)
        this->mMoraleEffect->Load(false);
    if (this->mBattleCryEffect != NULL)
        this->mBattleCryEffect->Load(false);
    if (this->mShellHoleEffect != NULL)
        this->mShellHoleEffect->Load(false);
    if (this->mEffect0D0 != NULL)
        this->mEffect0D0->Load(false);
    if (this->mBloodStainEffect != NULL)
        this->mBloodStainEffect->Load(false);
    this->mIsMediaLoaded = true;
}

// The EEUC* ctors the class factory builds (inline, declared in the EEUC*.h headers; defined here, where their only
// user is: an inline body in the headers would renumber every includer's `$E`, e.g. EEUGGather.cpp's chains).
// Where the original's budget refused them, their out-of-line copies follow the factory (0x6361e3..0x6363ce).
inline EEUCAirAnimal::EEUCAirAnimal(EEUnitClassRecord* inRecord) : EEUCAirplane(inRecord, gPhysicsModels[10]) {}
inline EEUCAirplane::EEUCAirplane(EEUnitClassRecord* inRecord) : EEUCLandable(inRecord, 1) {}
inline EEUCAirplane::EEUCAirplane(EEUnitClassRecord* inRecord, long inModel) : EEUCLandable(inRecord, inModel) {}
inline EEUCAmbient::EEUCAmbient(EEUnitClassRecord* inRecord) : EEUnitClass(0, inRecord, 0) {}
inline EEUCBasicCitizen::EEUCBasicCitizen(EEUnitClassRecord* inRecord, int inIndex) : EEUnitClass(1, inRecord, gPhysicsModels[inIndex]) {}
inline EEUCBomb::EEUCBomb(EEUnitClassRecord* inRecord) : EEUCProjectile(inRecord) {}
inline EEUCBuilding::EEUCBuilding(EEUnitClassRecord* inRecord, long inModel) : EEUnitClass(1, inRecord, inModel) {}
inline EEUCBuildingPriest::EEUCBuildingPriest(EEUnitClassRecord* inRecord) : EEUCBuilding(inRecord, gPhysicsModels[8]) {}
inline EEUCFortress::EEUCFortress(EEUnitClassRecord* inRecord) : EEUCLandTransport(inRecord, 8) {}
inline EEUCGate::EEUCGate(EEUnitClassRecord* inRecord) : EEUCBuilding(inRecord, gPhysicsModels[8]) {}
inline EEUCHelicopter::EEUCHelicopter(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[2]) {}
inline EEUCHelicopterPriest::EEUCHelicopterPriest(EEUnitClassRecord* inRecord) : EEUCBasicPriest(inRecord, gPhysicsModels[2]) {}
inline EEUCHelicopterTransport::EEUCHelicopterTransport(EEUnitClassRecord* inRecord) : EEUCTransport(inRecord, 2) {}
inline EEUCLand::EEUCLand(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[0]) {}
inline EEUCLandCitizen::EEUCLandCitizen(EEUnitClassRecord* inRecord) : EEUCBasicCitizen(inRecord, 0) {}
inline EEUCLandMilitaryHero::EEUCLandMilitaryHero(EEUnitClassRecord* inRecord) : EEUCLand(inRecord) {}
inline EEUCLandTransport::EEUCLandTransport(EEUnitClassRecord* inRecord) : EEUCTransport(inRecord, 0) {}
inline EEUCLandTransport::EEUCLandTransport(EEUnitClassRecord* inRecord, int inIndex) : EEUCTransport(inRecord, inIndex) {}
inline EEUCLandable::EEUCLandable(EEUnitClassRecord* inRecord, int inIndex) : EEUnitClass(1, inRecord, gPhysicsModels[inIndex]) {}
inline EEUCLandable::EEUCLandable(EEUnitClassRecord* inRecord, long inModel) : EEUnitClass(1, inRecord, inModel) {}
inline EEUCMiningBuilding::EEUCMiningBuilding(EEUnitClassRecord* inRecord) : EEUCBuilding(inRecord, gPhysicsModels[0]) {}
inline EEUCMissile::EEUCMissile(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[7]) {}
inline EEUCNaval::EEUCNaval(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[4]) {}
inline EEUCNavalBuilding::EEUCNavalBuilding(EEUnitClassRecord* inRecord) : EEUCBuilding(inRecord, gPhysicsModels[4]) {}
inline EEUCNavalCitizen::EEUCNavalCitizen(EEUnitClassRecord* inRecord) : EEUCBasicCitizen(inRecord, 4) {}
inline EEUCNavalTransport::EEUCNavalTransport(EEUnitClassRecord* inRecord) : EEUCTransport(inRecord, 4) {}
inline EEUCPlanetSpaceship::EEUCPlanetSpaceship(EEUnitClassRecord* inRecord) : EEUCLandable(inRecord, 1) {}
inline EEUCProjectile::EEUCProjectile(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[6]) {}
inline EEUCResource::EEUCResource(EEUnitClassRecord* inRecord) : EEUnitClass(0, inRecord, 0) {}
inline EEUCSeaAnimal::EEUCSeaAnimal(EEUnitClassRecord* inRecord) : EEUCSubmergedNaval(inRecord, gPhysicsModels[9]) {}
inline EEUCSpaceBuilding::EEUCSpaceBuilding(EEUnitClassRecord* inRecord) : EEUCBuilding(inRecord, gPhysicsModels[11]) {}
inline EEUCSpaceTransport::EEUCSpaceTransport(EEUnitClassRecord* inRecord) : EEUCTransport(inRecord, 0xb) {}
inline EEUCSpaceship::EEUCSpaceship(EEUnitClassRecord* inRecord) : EEUnitClass(1, inRecord, gPhysicsModels[11]) {}
inline EEUCSubmergedNaval::EEUCSubmergedNaval(EEUnitClassRecord* inRecord, long inModel) : EEUnitClass(1, inRecord, inModel) {}
inline EEUCTorpedo::EEUCTorpedo(EEUnitClassRecord* inRecord) : EEUCMissile(inRecord) {}
inline EEUCTransport::EEUCTransport(EEUnitClassRecord* inRecord, int inIndex) : EEUnitClass(1, inRecord, gPhysicsModels[inIndex]) {}

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

// FUNCTION: EXE 0x6358b1
// Builds every unit class from dbobjects by category, links the related classes and set flags, sorts sUnitTypes by
// name (the linking pass pairs class table slot i with database record i)
// The cases are in the order the original lays their bodies out
void EEUnitClass::CreateUnitClasses()
{
    FSFileSpec* theDbFile = NULL;
    long theResult = gDataPath->FindFile(kDbObjectsFile, theDbFile);
    ESVERIFY_AT(theResult == keseOK, 5710);
    ESVERIFY_AT(theDbFile != NULL, 5711);
    theResult = gDatabaseObjects.Load(theDbFile);
    ESVERIFY_AT(theResult == keseOK, 5713);
    if (theDbFile != NULL)
        delete theDbFile;
    for (unsigned long theIndex = 0; theIndex < 0x367; theIndex++)
        gUnitClassTable[theIndex] = NULL;
    unsigned long theCounts[0x23];
    for (unsigned long theCategory = 0; theCategory < 0x23; theCategory++)
        theCounts[theCategory] = 0;
    unsigned long theCount = gDatabaseObjects.GetCount();
    unsigned long i;
    for (i = 0; i < theCount; i++) {
        EEUnitClassRecord* theRecord = (EEUnitClassRecord*)gDatabaseObjects.GetEntry(i)->mRecord;
        if (!theRecord->mIsEnabled || theRecord->mUnitType == -1)
            continue;
        EEUnitClass* theClass;
        switch (theRecord->mCategory) {
        case kUnitCategoryAmbient:
            theClass = new EEUCAmbient(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryResource:
            theClass = new EEUCResource(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryBuilding:
        case kUnitCategoryBuilding19:
            theClass = new EEUCBuilding(theRecord, gPhysicsModels[8]);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryAirplane:
            theClass = new EEUCAirplane(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryNaval:
            theClass = new EEUCNaval(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryLand:
        case kUnitCategoryLand8:
        case kUnitCategoryLand15:
            theClass = new EEUCLand(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryLandCitizen:
            theClass = new EEUCLandCitizen(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryNavalCitizen:
            theClass = new EEUCNavalCitizen(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryHelicopter:
            theClass = new EEUCHelicopter(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryProjectile:
            theClass = new EEUCProjectile(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryBomb:
            theClass = new EEUCBomb(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryMissile:
            theClass = new EEUCMissile(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryLandPriest:
            theClass = new EEUCLandPriest(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryNavalPriest:
            theClass = new EEUCNavalPriest(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryNavalBuilding:
            theClass = new EEUCNavalBuilding(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryTorpedo:
            theClass = new EEUCTorpedo(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategorySubmergedNaval:
            theClass = new EEUCSubmergedNaval(theRecord, gPhysicsModels[5]);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryLandTransport:
            theClass = new EEUCLandTransport(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryHelicopterTransport:
            theClass = new EEUCHelicopterTransport(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryNavalTransport:
            theClass = new EEUCNavalTransport(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryFortress:
            theClass = new EEUCFortress(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryLandMilitaryHero:
            theClass = new EEUCLandMilitaryHero(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryMiningBuilding:
            theClass = new EEUCMiningBuilding(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryGate:
            theClass = new EEUCGate(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryHelicopterPriest:
            theClass = new EEUCHelicopterPriest(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategorySeaAnimal:
            theClass = new EEUCSeaAnimal(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryAirAnimal:
            theClass = new EEUCAirAnimal(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryBuildingPriest:
            theClass = new EEUCBuildingPriest(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategorySpaceship:
            theClass = new EEUCSpaceship(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategorySpaceTransport:
            theClass = new EEUCSpaceTransport(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategorySpaceBuilding:
            theClass = new EEUCSpaceBuilding(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        case kUnitCategoryPlanetSpaceship:
            theClass = new EEUCPlanetSpaceship(theRecord);
            theCounts[theRecord->mCategory]++;
            break;
        default:
            theClass = NULL;
            break;
        }
        EEUnitClass::RegisterUnitClass(theClass);
        if (theClass != NULL)
            theClass->Dereference();
    }
    EEUnitClass** theEntry = gUnitClassTable;
    for (i = 0; i < theCount; i++, theEntry++) {
        EEUnitClass* theClass = *theEntry;
        if (theClass == NULL)
            continue;
        EEDbObject* theObject = gDatabaseObjects.GetEntry(i);
        EEUnitClassRecord* theRecord = (EEUnitClassRecord*)theObject->mRecord;
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
        if (theRecord->mL480 > 0) {
            theClass->mRemainsClass = gUnitClassTable[theRecord->mL480];
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
    }
    unsigned long theTotal = 0;
    for (unsigned long theKind = 0; theKind < 0x23; theKind++)
        theTotal += theCounts[theKind];
    gDatabaseObjects.Clear();
    std::sort(EEUnitClass::sUnitTypes.begin(), EEUnitClass::sUnitTypes.end(), EEUnitClassNameLess());
}

// SYNTHETIC: EXE 0x636165
// (the scalar deleting destructor, emitted with the vtable; slot EEUCAmbient+0x0, stores EEUCAmbient vtable)

// FUNCTION: EXE 0x636187
// Returns 2 for a resource class (EEUCResource::Virtual70)
// (inline in ee/EEUCResource.h, emitted with the vtable; slot EEUCResource+0x70)

// SYNTHETIC: EXE 0x63618b
// (the scalar deleting destructor, emitted with the vtable; slot EEUCResource+0x0, stores EEUCResource vtable)

// FUNCTION: EXE 0x6361ad
// Returns whether a building can be picked: not a flare and not the volcano (EEUCBuilding::IsPickable)
// (inline in ee/EEUCBuilding.h, emitted with the vtable; slot +0x60 of EEUCBuilding and its subclasses)

// FUNCTION: EXE 0x6361c7
// Returns whether a player may select the building: not a flare and not the volcano (EEUCBuilding::IsSelectable)
// (inline in ee/EEUCBuilding.h, emitted with the vtable; slot +0x64 of EEUCBuilding and its subclasses)

// FUNCTION: EXE 0x6361e3
// Creates a building class from its database record and physics model (EEUCBuilding ctor, out of line)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// FUNCTION: EXE 0x636201
// Returns 1 for a building class (EEUCBuilding::Virtual70)
// (inline in ee/EEUCBuilding.h, emitted with the vtable; slot +0x70 of EEUCBuilding and its subclasses; folded
// by ICF with EEGlobalCalamityClass::GetTargetKind, slot +0x14 of EECCCloaking and EECCLineOfSight)

// FUNCTION: EXE 0x636205
// Creates a landable (aircraft) class from its record with the physics model of the given index (EEUCLandable ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// FUNCTION: EXE 0x63622a
// Creates a landable (aircraft) class from its record and a given physics model (EEUCLandable ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// SYNTHETIC: EXE 0x636248
// (the scalar deleting destructor, emitted with the vtable; slot EEUCNaval+0x0, stores EEUCNaval vtable)

// SYNTHETIC: EXE 0x63626a
// (the scalar deleting destructor, emitted with the vtable; slot EEUCSpaceBuilding+0x0, slot EEUCBuildingPriest+0x0, slot EEUCGate+0x0, slot EEUCMiningBuilding+0x0, slot EEUCNavalBuilding+0x0, slot EEUCBuilding+0x0, stores EEUCBuilding vtable)

// FUNCTION: EXE 0x63628c
// Returns the string id 0x7c38 of the transport's unload-failed message
// (inline in ee/EEUCTransport.h, emitted with the vtable; slot +0xec of EEUCTransport and its subclasses)

// FUNCTION: EXE 0x636292
// Returns whether the transport flies (its class has the air movement domain)
// (inline in ee/EEUCTransport.h, emitted with the vtable; slot +0x5c of EEUCTransport, its subclasses and EEUCFortress)

// FUNCTION: EXE 0x63629e
// Creates a transport class from its record with the physics model of the given index (EEUCTransport ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// FUNCTION: EXE 0x6362c3
// Creates a land unit class from its record with the land physics model (EEUCLand ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// FUNCTION: EXE 0x6362e3
// Creates a citizen class from its record with the physics model of the given index (EEUCBasicCitizen ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// SYNTHETIC: EXE 0x636308
// (the scalar deleting destructor, emitted with the vtable; slot EEUCNavalCitizen+0x0, slot EEUCLandCitizen+0x0, slot EEUCBasicCitizen+0x0, stores EEUCBasicCitizen vtable)

// FUNCTION: EXE 0x63632a
// Creates a priest class from its record and a given physics model (EEUCBasicPriest ctor)
// (out-of-line copy of the inline ctor in ee/EEUCBasicPriest.h)

// SYNTHETIC: EXE 0x636348
// (the scalar deleting destructor, emitted with the vtable; slot EEUCBomb+0x0, slot EEUCProjectile+0x0, stores EEUCProjectile vtable)

// FUNCTION: EXE 0x63636a
// Creates a missile class from its record with the missile physics model (EEUCMissile ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// SYNTHETIC: EXE 0x63638a
// (the scalar deleting destructor, emitted with the vtable; slot EEUCHelicopter+0x0, stores EEUCHelicopter vtable)

// SYNTHETIC: EXE 0x6363ac
// (the scalar deleting destructor, emitted with the vtable; slot EEUCTorpedo+0x0, slot EEUCMissile+0x0, stores EEUCMissile vtable)

// FUNCTION: EXE 0x6363ce
// Creates a submerged naval class from its record and a given physics model (EEUCSubmergedNaval ctor)
// (out-of-line copy of the inline ctor defined above CreateUnitClasses)

// SYNTHETIC: EXE 0x6363ec
// (the scalar deleting destructor, emitted with the vtable; slot EEUCSeaAnimal+0x0, slot EEUCSubmergedNaval+0x0, stores EEUCSubmergedNaval vtable)

// FUNCTION: EXE 0x63640e
// Returns the string id 0x7c39 of the fortress's unload-failed message
// (inline in ee/EEUCFortress.h, emitted with the vtable; slot EEUCFortress+0xec)

// SYNTHETIC: EXE 0x636414
// (the scalar deleting destructor, emitted with the vtable; slot EEUCLandMilitaryHero+0x0, slot EEUCLand+0x0, stores EEUCLand vtable)

// FUNCTION: EXE 0x636436
// Loads a building priest's media for a player as a priest's (EEUCBasicPriest::LoadMedia)
// (EEUCBuildingPriest::LoadMedia: inline in ee/EEUCBuildingPriest.h, emitted with the vtable)

// FUNCTION: EXE 0x636442
// Unloads a building priest's media for a player as a priest's (EEUCBasicPriest::UnloadMedia)
// (EEUCBuildingPriest::UnloadMedia: inline in ee/EEUCBuildingPriest.h, emitted with the vtable)

// FUNCTION: EXE 0x63644e
// Updates a building priest's effects as a building's (EEUCBuilding::UpdateEffects)
// (EEUCBuildingPriest::UpdateEffects: inline in ee/EEUCBuildingPriest.h, emitted with the vtable)

// SYNTHETIC: EXE 0x63645a
// (the scalar deleting destructor, emitted with the vtable; slot EEUCSpaceship+0x0, stores EEUCSpaceship vtable)

// SYNTHETIC: EXE 0x63647c
// (the scalar deleting destructor, emitted with the vtable; slot EEUCSpaceTransport+0x0, slot EEUCFortress+0x0, slot EEUCNavalTransport+0x0, slot EEUCHelicopterTransport+0x0, slot EEUCLandTransport+0x0, slot EEUCTransport+0x0, stores EEUCTransport vtable)

// SYNTHETIC: EXE 0x63649e
// (the scalar deleting destructor, emitted with the vtable; slot EEUCPlanetSpaceship+0x0, slot EEUCAirAnimal+0x0, slot EEUCAirplane+0x0, slot EEUCLandable+0x0, stores EEUCLandable vtable)

// FUNCTION: EXE 0x6364c0
// Returns the class's AI script action for a key, or NULL when the class has no script or no such action
EEUnitAction* EEUnitClass::FindAction(long inKey)
{
    EEUnitAction* theResult = NULL;
    if (this->mAIScript != NULL) {
        std::map<long, EEUnitAction*>::iterator theEntry = this->mAIScript->find(inKey);
        if (theEntry != this->mAIScript->end())
            theResult = theEntry->second;
    }
    return theResult;
}

// FUNCTION: EXE 0x6364f3
// Turns each turret of a living unit back toward straight ahead by at most its turn rate for this tick
void EEUnitClass::TurnTurretsBack(EEComplexUnit* inUnit)
{
    float theStep = inUnit->mStats->mTurretMaxAngularVelocity;
    theStep *= gPhysics->GetTickSeconds();
    unsigned long theCount = inUnit->GetModel()->GetNumParts();
    if (!(inUnit->mHitPoints > 0))
        return;
    for (unsigned long i = 0; i < theCount; i++) {
        float theValue = inUnit->mTurretAngles[i];
        if (theValue > 0.0f) {
            theValue -= theStep;
            if (theValue < 0.0f)
                theValue = 0.0f;
        } else if (theValue < 0.0f) {
            theValue += theStep;
            if (theValue > 0.0f)
                theValue = 0.0f;
        }
        if (inUnit->mTurretAngles[i] != theValue)
            inUnit->mTurretAngles[i] = theValue;
    }
}

// FUNCTION: EXE 0x63658b
// Sends a goalless unit to the centre of its tile; with a goal, faces the unit to the centre and places it there
void EEUnitClass::MoveToTileCentre(EEComplexUnit* inUnit)
{
    GETransformation theTransformation;
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    if (inUnit->mGoalState == NULL) {
        inUnit->SetCurrentPhysicsAction(0);
        inUnit->DropPath();
        theLocation.SetX(inUnit->GetTileX() + 0.5f);
        theLocation.SetY(inUnit->GetTileY() + 0.5f);
        theLocation.SetZ(inUnit->mUnitClass->mCanFly == true ? inUnit->mHeight : inUnit->mTile->mHeight);
        inUnit->SetGoal(new EEUGLocation(&theLocation, false, true, false, 0.0f, false, false), 0, 0);
    } else {
        GE3DPoint theTarget(inUnit->GetTileX() + 0.5f, inUnit->GetTileY() + 0.5f,
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
        float theDepth = 1.0f - gWorldTable.GetFloatValue(0x4e);
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
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIPathGoalModifierBase.h"
#include "ee/EEAIBoundedPathGoalModifierBase.h"
#include "ee/EEAIPathGoalModifiers.h"
// FUNCTION: EXE 0x63674b
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
            EEUGLocation* theLocationGoal = theGoal->mKind == kGoalLocation ? static_cast<EEUGLocation*>(theGoal) : 0;
            if (theLocationGoal != NULL) {
                theUseGraph = !theLocationGoal->mIsAutoWaypoint && !theGoal->mB1D;
                if (theLocationGoal->mGroupPathState == 1) {
                    theRevs = 15;
                    theLocationGoal->mGroupPathState = 2;
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

// FUNCTION: EXE 0x6369b9
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

// FUNCTION: EXE 0x636a83
// Counts one unit of the class gone and records the time when the last one goes
void EEUnitClass::UnitRemoved()
{
    if (--this->mNumUnits == 0)
        this->mLastUnitGoneTime = TMGetTime();
}

// FUNCTION: EXE 0x636a9c
// Adds a unit class to the class table under its unit type and to the list of unit types, keeping a reference
void EEUnitClass::RegisterUnitClass(EEUnitClass* inClass)
{
    if (inClass == NULL)
        return;
    inClass->Reference();
    gUnitClassTable[inClass->mUnitType] = inClass;
    EEUnitClass::sUnitTypes.push_back(inClass->mUnitType);
}

// FUNCTION: EXE 0x636ac6
// Removes a placed unit (not a path marker) from the map's unit region that holds its tile
// NEAR: the original calls a vector remove helper 0x239ee2 (find + erase; a COMDAT of this unit)
void EEUnitClass::RemoveFromRegion(EEUnit* inUnit)
{
    EEMap* theMap = ::gWorld.mMap;
    U2DPoint<long> theRegion(0, 0);
    RTSRegionTable<EEComplexUnit*>* theTable = theMap->mUnitRegions;
    if (inUnit->mTileNode == NULL || inUnit->IsPathMarker())
        return;
    theRegion = U2DPoint<long>(inUnit->GetTileX() / theTable->mRegionSize, inUnit->GetTileY() / theTable->mRegionSize);
    EEComplexUnit* theUnit = inUnit->GetComplexUnit();
    RTSRegion<EEComplexUnit*>* theList = theTable->GetRegion(theRegion);
    if (theList != NULL)
        theList->Remove(theUnit);
}

// FUNCTION: EXE 0x636b33
// Releases the class table's entry for a unit type and clears it
void EEUnitClass::ReleaseUnitClass(unsigned long inUnitType)
{
    EEUnitClass*& theEntry = gUnitClassTable[inUnitType];
    if (theEntry != NULL)
        theEntry->Dereference();
    theEntry = NULL;
}

// FUNCTION: EXE 0x636b55
// Removes a unit from the tile it stands on, if any
void EEUnitClass::RemoveFromTile(EEUnit* inUnit)
{
    RTSTile* theTile = inUnit->mTile;
    if (theTile != NULL)
        theTile->RemoveUnit(inUnit);
}

// FUNCTION: EXE 0x636b69
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
                theFirst = static_cast<EEUGAttackUnit*>(theAttack)->mSidestepCount < 1;
                inUnit->DropPath();
            } else {
                theFirst = false;
            }
            if (theFirst) {
                EEComplexUnit* theTarget = static_cast<EEUGAttackUnit*>(theAttack)->mTarget->GetComplexUnit();
                if (theTarget->mUnitClass->mIsWall || !theGoal->mB1C) {
                    inUnit->IgnoreTarget(theTarget);
                    EETileCoordinate theUnitTile = inUnit->GetTile();
                    EEFOFindNewTargetInMeleeRange theScreen(inUnit, theTarget);
                    if (EEFindNewTarget(&theScreen, theAttack, inUnit, &theUnitTile, 1,
                                   theTarget != NULL ? theTarget->mUnitClass->mIsWall : false, 1)) {
                        static_cast<EEUGAttackUnit*>(theAttack)->mSidestepCount = 0;
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
                if (EECanStandOn(theSideTile, inUnit)) {
                    if (UMAbs(theSideTile.GetX() - inUnit->GetTileX()) == 1 && UMAbs(theSideTile.GetY() - inUnit->GetTileY()) == 1 &&
                        EEIsTileTopAbove(theSideTile.GetX(), inUnit->GetTileY(), 0.0f, &theDummy, true) &&
                        EEIsTileTopAbove(inUnit->GetTileX(), theSideTile.GetY(), 0.0f, &theDummy, true)) {
                        theLeft = 0x7fffffff;
                    } else {
                        for (RTSTileUnitNode* theNode = gWorld.mMap->GetTile(theSideTile.GetX(), theSideTile.GetY())->mUnits;
                             theNode != NULL; theNode = theNode->mNext)
                            theLeft++;
                    }
                }
                theGoalPoint.mY += theSide.GetY();
                theGoalPoint.mX += theSide.GetX();
                theTile = U2DPoint<long>(UMFloor(theGoalPoint.GetX()), UMFloor(theGoalPoint.GetY()));
                if (EECanStandOn(theTile, inUnit)) {
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
                        static_cast<EEUGAttackUnit*>(theAttack)->mSidestepCount = 1;
                        this->FindPath(inUnit);
                        return;
                    }
                }
                inUnit->AddWaypoint(theGoalPoint, false);
                theGoal->mPathAttempts += 8;
                static_cast<EEUGAttackUnit*>(theAttack)->mSidestepCount++;
                return;
            }
        }
    }
    if (theAttack != NULL && theAttack->mKind == kGoalAttackUnit)
        static_cast<EEUGAttackUnit*>(theAttack)->mSidestepCount = 0;
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
            Fn248FE4(theCarrier, theTiles, 0.0f);
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
        EEUGGather* theGather = dynamic_cast<EEUGGather*>(theGoal);
        if (theGather != NULL && theGather->mResource != NULL && theGather->mResource->mResourceType == kResourceWood)
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

// FUNCTION: EXE 0x63795f
// Creates a target screen that accepts units closer to the attacker than its current target
EEFOFindNewTargetInMeleeRange::EEFOFindNewTargetInMeleeRange(EEComplexUnit* inUnit, EEUnit* inTarget)
    : mUnit(inUnit), mTarget(inTarget)
{
    this->mRange = EEGetHitDistanceSquared(inUnit, inTarget, -10000.0f, -10000.0f);
}

// FUNCTION: EXE 0x63799d
// Returns whether a candidate is alive and closer to the attacker than its current target
bool EEFOFindNewTargetInMeleeRange::Virtual04(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && EEGetHitDistanceSquared(this->mUnit, inUnit, -10000.0f, -10000.0f) < this->mRange;
}

bool EEFOFindNewTargetInMeleeRange::Virtual08(EEComplexUnit* inUnit)
{
    return inUnit->mHitPoints > 0 && EEGetHitDistanceSquared(this->mUnit, inUnit, -10000.0f, -10000.0f) < this->mRange;
}

// FUNCTION: EXE 0x6379dd
// Turns an attacked unit on its attacker (or to its location when unseen) and calls idle nearby soldiers to help
void EEUnitClass::Attacked(EEComplexUnit* inUnit)
{
    GE3DPoint thePosition;
    std::vector<EEUnit*> theUnits;
    EEComplexUnit* theTarget = inUnit->mLastAttacker;
    U2DPoint<long> theTile;
    EEUnitGoal* theGoal;
    if (!EEIsSeenByOwnerOf(theTarget, inUnit) && inUnit->GetStats()->mMaxVelocity != 0.0f) {
        theGoal = new EEUGLocation(theTarget->mPhysicalModel.GetLocation().GetX(),
            theTarget->mPhysicalModel.GetLocation().GetY(), 0.0f, false, false, false, 0.0f, false, 0);
    } else {
        theGoal = new EEUGAttackUnit(inUnit, theTarget, false, false);
    }
    inUnit->SetGoal(theGoal, 1, 0);
    if (inUnit->GetPlayer()->GetPlayerID() != 0 &&
        ((inUnit->GetUnitClass()->GetCategory() != kUnitCategoryLandCitizen && inUnit->GetUnitClass()->GetCategory() != kUnitCategoryNavalCitizen) || theTarget->GetPlayer()->GetPlayerID() != 0)) {
        const GETransformation& theLocation = inUnit->mPhysicalModel.GetLocation();
        thePosition.SetX(theLocation.GetX());
        thePosition.SetY(theLocation.GetY());
        thePosition.SetZ(theLocation.GetZ());
        EEGetUnitsNear(thePosition, kCallForHelpRadius, theUnits);
        for (long i = (long)theUnits.size() - 1; i >= 0; --i) {
            // The original uses the returned object as an EEComplexUnit directly.
            EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(theUnits[i]);
            if (!(theUnit->GetHitPoints() > 0) || theUnit->GetPlayer() != inUnit->GetPlayer() ||
                !theUnit->mBehavior->mCanTargetEnemies || !theUnit->mBehavior->CanMoveFreely(theUnit) || theUnit->GetUnitClass()->mIsProjectile ||
                theUnit->GetUnitClass()->GetCategory() == kUnitCategoryLandCitizen || !theUnit->CanAttackClass(theTarget->GetUnitClass()) ||
                theUnit->IsTargetIgnored(theTarget, true))
                continue;
            EEUnitGoal* theCurrent = theUnit->mGoalState;
            if (theCurrent != NULL) {
                if (theCurrent->GetB1C() || theCurrent->mKind != kGoalAttackUnit)
                    continue;
                EEUnit* theOldTarget = static_cast<EEUGAttackUnit*>(theCurrent)->mTarget;
                if (!theOldTarget->GetUnitClass()->IsLandBuilding() || theOldTarget->GetStats()->mAttack != 0.0f)
                    continue;
            }
            if (!EEIsAttackReachable(theUnit, theTarget, &theTile)) {
                theUnit->IgnoreTarget(theTarget);
            } else {
                EEUnitGoal* theCopy = theGoal->Clone();
                if (theUnit->GetClass()->FindAction(theCopy->GetStartAction()) != NULL) {
                    theUnit->SetGoal(theCopy, 1, 0);
                    theUnit->StartAction(theCopy->GetStartAction());
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                }
            }
        }
        theUnits.clear();
    }
    if (inUnit->mLastAttacker != NULL)
        inUnit->mLastAttacker->Dereference();
    inUnit->mLastAttacker = NULL;
}

// FUNCTION: EXE 0x637ca6
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
            EEUnitGoal* theGoal = new EEUGLocation(theDestination, true, true, false, 0.0f, false, 0);
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

// FUNCTION: EXE 0x637e79
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

// FUNCTION: EXE 0x637f2f
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
    EEUnitGoal* theGoal = new EEUGAttackGround(GE3DPoint(theTransformation.GetX(), theTransformation.GetY(),
                                                         theTransformation.GetZ()), false);
    theComplex->SetVelocity(0.0f);
    theComplex->SetLauncherClass(theCreated->GetUnitClass());
    // The shipping routine gives the newly created unit itself as owner.
    theComplex->SetOwner(theComplex);
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

// FUNCTION: EXE 0x638087
// Fires the class's projectile from the unit's turret weapon point at a target or ground location;
// without a turret part, fires a plain shot
// The launch point is the weapon tag point mWeaponTagPoint scaled by the stats' scale and turned by the turret's
// yaw, under the unit's transformation; the shot is owned by inUnit
void EEUnitClass::LaunchFromTurret(EEComplexUnit* inUnit, EEUnit* inTarget, GE3DPoint& inLocation)
{
    GE3DPoint thePoint;
    GE3DPoint theWorld;
    GETransformation theLocation(inUnit->mPhysicalModel.GetLocation());
    GETransformation theOrientation;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEModel* theModel = inUnit->GetModel();
    EETagPointList* theList = theModel->mTagPoints[inUnit->GetAnimation()];
    unsigned long thePart = 0;
    unsigned long theIndex = inUnit->mWeaponTagPoint;
    if (theModel->FindTurretPart(theList->GetTagPoint(theIndex)->mModel, thePart) == 0) {
        theOrientation.SetOrientationYPR(inUnit->mTurretAngles[thePart], 0.0f, 0.0f);
        unsigned long theFrame = inUnit->mPhysicalModel.GetFrame();
        theList->GetTagPoint(theIndex)->GetOffset(theFrame, theOrientation, thePoint);
        thePoint.mX = theStats->GetScale() * thePoint.mX;
        thePoint.mY = theStats->GetScale() * thePoint.GetY();
        thePoint.mZ = theStats->GetScale() * thePoint.mZ;
        theLocation.Apply(thePoint, theWorld);
        GETransformation theLaunch(false);
        (theList->mPoints[theIndex])->GetTransformation(inUnit->mPhysicalModel.GetFrame(), theLocation,
                                                                  theOrientation, theLaunch);
        theLaunch.mMatrix[0][3] = theWorld.mX;
        theLaunch.mMatrix[1][3] = theWorld.GetY();
        theLaunch.mMatrix[2][3] = theWorld.mZ;
        EEUnit* theShot = this->mProjectileClass->CreateUnitAt(theLaunch, inUnit->mPlayer->GetPlayerID(), false, inUnit);
        if (this->mCreationEffect != NULL)
            gEffectManager->CreateUnitEffect(theShot, this->mCreationEffect, NULL, false, 0, 0);
        theShot->GetComplexUnit()->SetLauncherClass(this);
        theShot->GetComplexUnit()->SetOwner(inUnit);
        EEUnitGoal* theGoal;
        if (inTarget != NULL)
            theGoal = new EEUGAttackUnit(theShot->GetComplexUnit(), inTarget, false, false);
        else
            theGoal = new EEUGAttackGround(inLocation, false);
        theShot->GetComplexUnit()->SetGoal(theGoal, 0, 0);
        theShot->GetComplexUnit()->SetVelocity(inUnit->mVelocity);
        theShot->GetComplexUnit()->SetHeight(inUnit->mPhysicalModel.GetLocation().GetZ());
        this->mProjectileClass->LaunchProjectile(theShot->GetComplexUnit(), inLocation);
    } else
        this->LaunchShot(inUnit, inTarget, inLocation, NULL);
}

// FUNCTION: EXE 0x6382fd
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

// FUNCTION: EXE 0x638376
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

// FUNCTION: EXE 0x638432
// Unloads a player's media for the classes this class spawns and for all its effects, and marks it unloaded
void EEUnitClass::UnloadMedia(EEPlayer* inPlayer)
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
    if (this->mAttachedEffect != NULL)
        this->mAttachedEffect->Unload();
    if (this->mDeathEffect != NULL)
        this->mDeathEffect->Unload();
    if (this->mDamageEffect != NULL)
        this->mDamageEffect->Unload();
    if (this->mHealEffect != NULL)
        this->mHealEffect->Unload();
    if (this->mFireTrailEffect != NULL)
        this->mFireTrailEffect->Unload();
    if (this->mTrailEffect != NULL)
        this->mTrailEffect->Unload();
    if (this->mMuzzleFlashEffect != NULL)
        this->mMuzzleFlashEffect->Unload();
    if (this->mRippleEffect != NULL)
        this->mRippleEffect->Unload();
    if (this->mSplashEffect != NULL)
        this->mSplashEffect->Unload();
    if (this->mImpactSparksEffect != NULL)
        this->mImpactSparksEffect->Unload();
    if (this->mImpactBloodEffect != NULL)
        this->mImpactBloodEffect->Unload();
    if (this->mEffect090 != NULL)
        this->mEffect090->Unload();
    if (this->mCreationEffect != NULL)
        this->mCreationEffect->Unload();
    if (this->mEffect098 != NULL)
        this->mEffect098->Unload();
    if (this->mEffect09C != NULL)
        this->mEffect09C->Unload();
    if (this->mEffect0A0 != NULL)
        this->mEffect0A0->Unload();
    if (this->mEffect0A4 != NULL)
        this->mEffect0A4->Unload();
    if (this->mEffect0A8 != NULL)
        this->mEffect0A8->Unload();
    if (this->mSpecialAttackEffect != NULL)
        this->mSpecialAttackEffect->Unload();
    if (this->mEffect0B0 != NULL)
        this->mEffect0B0->Unload();
    if (this->mShotEffect != NULL)
        this->mShotEffect->Unload();
    if (this->mLeprosyEffect != NULL)
        this->mLeprosyEffect->Unload();
    if (this->mPlagueEffect != NULL)
        this->mPlagueEffect->Unload();
    if (this->mPulseCannonEffect != NULL)
        this->mPulseCannonEffect->Unload();
    if (this->mMoraleEffect != NULL)
        this->mMoraleEffect->Unload();
    if (this->mBattleCryEffect != NULL)
        this->mBattleCryEffect->Unload();
    if (this->mShellHoleEffect != NULL)
        this->mShellHoleEffect->Unload();
    if (this->mEffect0D0 != NULL)
        this->mEffect0D0->Unload();
    if (this->mBloodStainEffect != NULL)
        this->mBloodStainEffect->Unload();
    this->mIsMediaLoaded = false;
}

// FUNCTION: EXE 0x6386cf
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

// FUNCTION: EXE 0x638761
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
        theRadius = UMFloatToLong(gWorldTable.GetFloatValue(0x55) * inUnit->mStats->mRange);
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

// FUNCTION: EXE 0x638948
// Turns each turret toward the unit's attack, heal or calamity target by its turn rate; else turns them back
void EEUnitClass::TurnTurrets(EEComplexUnit* inUnit)
{
    GE3DPoint theOffset;
    GE3DPoint theWorldPosition;
    GE2DPoint theTarget;
    GE3DPoint theGoalPosition;
    GETransformation theTransform(inUnit->mPhysicalModel.GetLocation());
    GETransformation theRotation;
    GEVector theDirection;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theRotated;
    GEVector thePartFacing;
    GEVector theWorldFacing;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEModel* theModel = inUnit->GetModel();
    float theStep = theStats->mTurretMaxAngularVelocity * gPhysics->mTickSeconds;
    unsigned long theCount = theModel->mTurretParts.size();
    EEUnitGoal* theGoal = inUnit->mGoalState;
    if (theGoal == NULL || (theGoal->mKind != kGoalAttackUnit && theGoal->mKind != kGoalAttackGround && theGoal->mKind != kGoalCastCalamity && theGoal->mKind != kGoalHealUnit)) {
        this->TurnTurretsBack(inUnit);
        return;
    }
    if (theGoal->mKind == kGoalAttackUnit) {
        EEUnit* theUnit = static_cast<EEUGAttackUnit*>(theGoal)->mTarget;
        if (theUnit->GetComplexUnit() != NULL) {
            theTarget.SetX(theUnit->GetComplexUnit()->mPhysicalModel.GetLocation().GetX());
            theTarget.SetY(theUnit->GetComplexUnit()->mPhysicalModel.GetLocation().GetY());
        } else {
            theTarget.SetX((float)theUnit->GetTileX());
            theTarget.SetY((float)theUnit->GetTileY());
        }
    } else if (theGoal->mKind == kGoalHealUnit || theGoal->mKind == kGoalAttackGround || theGoal->mKind == kGoalCastCalamity) {
        theGoal->GetGoalLocation(theGoalPosition);
        theTarget.SetX(theGoalPosition.GetX());
        theTarget.SetY(theGoalPosition.GetY());
    }
    for (unsigned long i = 0; i < theCount; ++i) {
        EETurretPart* thePart = theModel->mTurretParts[i];
        theOffset = thePart->mPosition;
        theOffset.SetX(theStats->GetScale() * theOffset.GetX());
        theOffset.SetY(theStats->GetScale() * theOffset.GetY());
        theOffset.SetZ(theStats->GetScale() * theOffset.GetZ());
        theTransform.Apply(theOffset, theWorldPosition);
        theDirection.SetX(theTarget.GetX() - theWorldPosition.GetX());
        theDirection.SetY(theTarget.GetY() - theWorldPosition.GetY());
        theDirection.SetZ(0.0f);
        theDirection.Normalize();
        float theHeading = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
        float theOld = inUnit->mTurretAngles[i];
        theRotation.SetOrientationYPR(theOld, 0.0f, 0.0f);
        theRotation.Apply(theForward, theRotated);
        (theModel->mTurretParts[i])->mTransformation.Apply(theRotated, thePartFacing);
        theTransform.Apply(thePartFacing, theWorldFacing);
        theWorldFacing.SetZ(0.0f);
        theWorldFacing.Normalize();
        float theAngle = theHeading - UMArcTan2(-theWorldFacing.GetX(), theWorldFacing.GetY());
        if (theAngle < -3.1415927f) theAngle += 6.2831855f;
        else if (theAngle > 3.1415927f) theAngle -= 6.2831855f;
        if (theAngle > 0.0f) {
            if (theAngle > theStep) theAngle = theOld + theStep;
            else theAngle += theOld;
        } else if (theAngle < 0.0f) {
            theAngle = -theAngle;
            if (theAngle > theStep) theAngle = theOld - theStep;
            else theAngle = theOld - theAngle;
        } else theAngle = theOld;
        if (theAngle < -3.1415927f) theAngle += 6.2831855f;
        else if (theAngle > 3.1415927f) theAngle -= 6.2831855f;
        if (theAngle != inUnit->mTurretAngles[i]) inUnit->mTurretAngles[i] = theAngle;
    }
}

// FUNCTION: EXE 0x638ce7
// Advances a unit's animation by this tick, plays the frame sounds and returns the number of frames stepped
long EEUnitClass::Animate(EEComplexUnit* inUnit)
{
    EESoundObject* theSound = NULL;
    unsigned long theSteps = 0;
    unsigned long theFrame;
    if (inUnit->mIsAnimationPaused) {
        unsigned long theTime = gPhysics->mTickTimeMs + inUnit->mAnimationTime;
        unsigned long theState = inUnit->GetAnimation();
        if (theTime < inUnit->GetModel()->mHoldTimes[theState]) {
            inUnit->SetAnimationTime(theTime);
        } else {
            EEGraphicObject* theObject = inUnit->GetModel();
            theState = inUnit->GetAnimation();
            if (theObject->mLoops[theState]) {
                if (theState != 7 && (inUnit->mStats->mCanFireOverWalls || theState != 8)) {
                    theFrame = theObject->mStartFrames[theState];
                    inUnit->SetIsAnimationPaused(false);
                    inUnit->SetAnimationFrame(theFrame);
                    inUnit->SetAnimationTime(0);
                    if (theFrame == theObject->mSound1Frames[theState])
                        theSound = theObject->mSounds1[theState];
                    else if (theFrame == theObject->mSound2Frames[theState])
                        theSound = theObject->mSounds2[theState];
                } else {
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
            } else {
                SESMInstanceNode* theInstance = inUnit->mSound;
                if (theInstance != NULL) {
                    gSoundManager->Stop(theInstance, !gSoundManager->GetIsLooping(theInstance));
                    inUnit->ReleaseSound();
                }
            }
        }
    } else {
        EEGraphicObject* theObject = inUnit->GetModel();
        unsigned long theState = inUnit->GetAnimation();
        float theFraction = 0.0f;
        unsigned long theCount = theObject->mFrameCounts[theState];
        unsigned long theTime = gPhysics->mTickTimeMs + inUnit->mAnimationTime;
        bool theLoop = theObject->mLoops[theState];
        unsigned long theStart = theObject->mStartFrames[theState];
        unsigned long theLast = theObject->mLastFrames[theState];
        unsigned long theOldFrame = inUnit->mPhysicalModel.GetFrame();
        theFrame = theOldFrame;
        unsigned long theFrameTime;
        if (!theObject->mIsSpeedScaled[theState]) {
            theFrameTime = theObject->mFrameTimes[theState];
        } else {
            float theSpeed = inUnit->GetVelocity() / this->GetMaxVelocity();
            if (!(theSpeed >= 0.5f))
                theSpeed = 0.5f;
            float theTurn = inUnit->mAngularVelocity.GetYaw();
            theTurn = (theTurn >= 0.0f ? theTurn : -theTurn) / this->mMaxAngularVelocity;
            if (!(theSpeed >= 0.5f))
                theSpeed = 0.5f;
            if (!(theTurn >= 0.5f))
                theTurn = 0.5f;
            if (theSpeed > theTurn)
                theFrameTime = UMFloatToLong((float)theObject->mFrameTimes[theState] / theSpeed);
            else
                theFrameTime = UMFloatToLong((float)theObject->mFrameTimes[theState] / theTurn);
        }
        unsigned long theRemainder;
        if (theTime < theFrameTime) {
            inUnit->SetAnimationTime(theTime);
            theRemainder = theTime;
        } else {
            if (theFrameTime > 0) {
                theSteps = theTime / theFrameTime;
                theRemainder = theTime - theFrameTime * theSteps;
            } else {
                theRemainder = 0;
                theSteps = 1;
            }
            if (theLoop) {
                theFrame = (theOldFrame - theStart + theSteps) % theCount + theStart;
                if (theFrame > theOldFrame || theObject->mHoldTimes[theState] == 0) {
                    inUnit->SetAnimationFrame(theFrame);
                    inUnit->SetAnimationTime(theRemainder);
                    if (theFrame == theObject->mSound1Frames[theState])
                        theSound = theObject->mSounds1[theState];
                    else if (theFrame == theObject->mSound2Frames[theState])
                        theSound = theObject->mSounds2[theState];
                } else {
                    inUnit->SetIsAnimationPaused(true);
                    inUnit->SetAnimationFrame(theStart);
                    inUnit->SetAnimationTime(0);
                    SESMInstanceNode* theInstance = inUnit->mSound;
                    if (theInstance != NULL) {
                        gSoundManager->Stop(theInstance, !gSoundManager->GetIsLooping(theInstance));
                        inUnit->ReleaseSound();
                    }
                }
            } else if (!inUnit->IsFoundation()) {
                unsigned long theOffset = theOldFrame - theStart + theSteps;
                theFrame = theStart + theOffset;
                if (theOffset < theCount) {
                    inUnit->SetAnimationFrame(theFrame);
                    inUnit->SetAnimationTime(theRemainder);
                    if (theFrame == theObject->mSound1Frames[theState])
                        theSound = theObject->mSounds1[theState];
                    else if (theFrame == theObject->mSound2Frames[theState])
                        theSound = theObject->mSounds2[theState];
                } else {
                    inUnit->SetIsAnimationPaused(true);
                    inUnit->SetAnimationFrame(theLast);
                    inUnit->SetAnimationTime(0);
                    theFrame = theLast;
                    SESMInstanceNode* theInstance = inUnit->mSound;
                    if (theInstance != NULL) {
                        gSoundManager->Stop(theInstance, !gSoundManager->GetIsLooping(theInstance));
                        inUnit->ReleaseSound();
                    }
                }
            }
        }
        if (theFrameTime != 0)
            theFraction = (float)theRemainder / (float)(long)theFrameTime;
        unsigned long theNext;
        if (theStart == theLast)
            theNext = theStart;
        else if (theFrame == theLast)
            theNext = theLoop ? theStart : theFrame;
        else
            theNext = theFrame + 1;
        if (theCount > 4 && theObject->mSharesFrames[theState]) {
            bool theEven = (~theCount & 1) != 0;
            if (theFrame == theLast) {
                theNext = theLoop ? theStart : theFrame;
            } else {
                theNext = theFrame + 1;
                if (theNext == theLast) {
                    if (theEven && theLoop)
                        theNext = theStart;
                } else {
                    theNext = theFrame + 2;
                }
            }
            if (theFrame != theLast || theEven)
                theFraction *= 0.5f;
            if (((theFrame - theStart) & 1) == 1)
                theFraction += 0.5f;
        }
        if (inUnit->mAnimationFraction != theFraction)
            inUnit->mAnimationFraction = theFraction;
        inUnit->SetNextFrame(theNext);
    }
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

// FUNCTION: EXE 0x6393a9
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

// FUNCTION: EXE 0x6393ff
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

// FUNCTION: EXE 0x6394b0
// Returns one over the unit's top speed, its active group's when it moves in one
float EEComplexUnit::GetInverseMaxVelocity()
{
    if (this->mActiveGroup != NULL)
        return this->mActiveGroup->GetInverseMaxVelocity(this);
    return this->mStats->mInverseMaxVelocity;
}

// FUNCTION: EXE 0x6394cd
// Ticks a building: finishes its production countdown (a unit, retried in 5 seconds when it cannot be placed,
// or a technology), then its orbitals, camouflage, effects, rotting, production queue and garrison healing
// EEUCBuilding::Update; production is instant under the player's mInstantBuild
extern bool gInCinematic;                   // 0x5318e0 (EECameraControl.cpp's free-camera flag; provisional name)

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
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
        for (EEComplexUnit* theLinked = inUnit->mNextPart; theLinked != NULL; theLinked = theLinked->mNextPart)
            theLinked->SetRottingAwayTime(theTime);
    }
    inUnit->ContinueProduction();
    this->HealGarrison(inUnit);
}

// FUNCTION: EXE 0x639644
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
    case kActionResourcesRotting: {
        long theTime = inUnit->mRottingAwayTime + gPhysics->mTickTimeMs;
        if (theTime > 4000) {
            long theAmount = inUnit->mResourceAmount - theTime / 4000;
            if (theAmount < 0)
                theAmount = 0;
            inUnit->SetResourceAmount(theAmount);
            theTime %= 4000;
        }
        inUnit->SetRottingAwayTime(theTime);
        break;
    }
    case kActionUnitRotting: {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
        break;
    }
    }
    this->Fn157269(inUnit);
}

// FUNCTION: EXE 0x63978b
// Counts down a unit's camouflage timers (dropping them without vital buildings) and fades its cloak opacity
void EEUnitClass::UpdateCamouflageTimer(EEComplexUnit* inUnit)
{
    unsigned long theRemaining = inUnit->mCamouflageTimer & 0x7fffffff;
    if (theRemaining > 0) {
        if (inUnit->mPlayer->mNumVitalBuildings < 1) {
            bool theCloaked = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
            if (theCloaked && EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()))
                gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
            inUnit->SetCloakableChain(false);
            inUnit->SetCloakedChain(false);
            inUnit->mCamouflageTimer = 0;
            inUnit->mU32C = 0;
        } else {
            long theTime = theRemaining - gPhysics->mTickTimeMs;
            if (theTime > 0) {
                inUnit->mCamouflageTimer = theTime;
                inUnit->SetCamouflaged(false);
            } else {
                inUnit->mCamouflageTimer = 0;
                inUnit->SetCamouflaged(true);
                inUnit->SetCloakableChain(true);
                inUnit->SetCloakedChain(true);
                inUnit->mU32C = 0;
                if (EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()))
                    gMiniMap->MoveUnit(inUnit, inUnit->GetTileX(), inUnit->GetTileY(), -10000, -10000);
            }
        }
    }
    if (inUnit->mU32C > 0) {
        if (inUnit->mPlayer->mNumVitalBuildings < 1) {
            bool theCloaked = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
            if (theCloaked && EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()))
                gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
            inUnit->SetCloakableChain(false);
            inUnit->SetCloakedChain(false);
            inUnit->mCamouflageTimer = 0;
            inUnit->mU32C = 0;
        } else {
            long theTime = inUnit->mU32C - gPhysics->mTickTimeMs;
            if (theTime > 0)
                inUnit->mU32C = theTime;
            else {
                bool theCloaked = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
                if (theCloaked && EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()))
                    gMiniMap->MoveUnit(inUnit, -10000, -10000, inUnit->GetTileX(), inUnit->GetTileY());
                inUnit->mU32C = 0;
                inUnit->SetCloakableChain(false);
                inUnit->SetCloakedChain(false);
            }
        }
    }
    if (!inUnit->mIsCloakable)
        return;
    this->UpdateCloakDetection(inUnit);
    float theOpacity = inUnit->mOpacity;
    bool theCloaked = inUnit->mIsCloakable ? inUnit->mIsCloaked : false;
    if (!theCloaked) {
        if (theOpacity < kCloakFadeMaxOpacity)
            theOpacity = gPhysics->GetTickTime() * gF5636A4 * sCloakFadeRate + theOpacity;
        if (theOpacity > kCloakFadeMaxOpacity)
            theOpacity = kCloakFadeMaxOpacity;
    } else {
        if (theOpacity > kCloakFadeMinOpacity)
            theOpacity = theOpacity - gPhysics->GetTickTime() * gF5636A4 * sCloakFadeRate;
        if (theOpacity < kCloakFadeMinOpacity)
            theOpacity = kCloakFadeMinOpacity;
    }
    if (inUnit->mUnitClass->GetUnitType() == kUnitTypeCapitol && inUnit->mStats->mPower > 0 &&
        !EEIsCloakedEnemyOf(inUnit, ::gWorld.GetCurrentPlayer()) && inUnit->mCloaked != 0) {
        long theTime = gPhysics->mGameTime % 2000;
        if (theTime >= 1000)
            theTime = 1999 - theTime;
        theOpacity = theTime * 0.00033333333f + 0.2f;
    }
    inUnit->SetOpacityChain(theOpacity);
}

// FUNCTION: EXE 0x639aad
// Sets or clears the unit's camouflaged flag (the top bit of its camouflage timer)
void EEComplexUnit::SetCamouflaged(bool inValue)
{
    if (inValue == true)
        this->mCamouflageTimer |= 0x80000000;
    else
        this->mCamouflageTimer &= 0x7fffffff;
}

// FUNCTION: EXE 0x639ac7
// Restores a unit's power by its regeneration rate every 5 seconds, up to its maximum (not during cinematics)
void EEUnitClass::RegeneratePower(EEComplexUnit* inUnit)
{
    if (!(inUnit->mStats->mPowerRegeneration > 0) || ::gWorld.mInCinematic || (this->IsLandBuilding() && !inUnit->mIsBuilt))
        return;
    long theRemaining = inUnit->mPowerRegenTimer - gPhysics->mTickTimeMs;
    if (theRemaining <= 0) {
        long theValue = inUnit->mStats->mPowerRegeneration + inUnit->mPower;
        if (theValue > inUnit->mStats->mPower)
            theValue = inUnit->mStats->mPower;
        inUnit->SetPower(theValue);
        theRemaining = 5000;
    }
    inUnit->SetPowerRegenTimer(theRemaining);
}

extern GEViewport gViewport;
extern bool gOrthographicView;

// FUNCTION: EXE 0x639b47
// Updates the volume, pan and pitch of a unit's animation sound every third tick (off-screen every 21st)
void EEUnitClass::RefreshSound(EEComplexUnit* inUnit)
{
    SESMInstanceNode* theInstance = inUnit->mSound;
    if (theInstance == NULL || gSoundManager->GetInstanceID(theInstance) == 0xffffffff)
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

// FUNCTION: EXE 0x639c74
// Heals the living units garrisoned in a living unit by its healing points each second
void EEUnitClass::HealGarrison(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    if (theStats->mHealingPoints == 0 || inUnit->GetNumCargoUnits() == 0 || ::gWorld.mInCinematic)
        return;
    long theTime = inUnit->mRegenerationTimer - gPhysics->mTickTimeMs;
    bool theHealed = false;
    if (theTime <= 0) {
        if (inUnit->GetHitPoints() > 0) {
            unsigned long theCount = inUnit->GetNumCargoUnits();
            for (unsigned long i = 0; i < theCount; i++) {
                EEComplexUnit* theUnit = inUnit->GetCargo(i);
                long theOldHitPoints = theUnit->GetHitPoints();
                long theHitPoints = theStats->mHealingPoints + theOldHitPoints;
                if (theHitPoints > theUnit->GetStats()->mMaxHitPoints)
                    theHitPoints = theUnit->GetStats()->mMaxHitPoints;
                if (theOldHitPoints > 0) {
                    theUnit->SetHitPoints(theHitPoints);
                    theHealed = true;
                }
            }
        }
        theTime = 1000;
        if (theHealed && inUnit->GetPlayer() == ::gWorld.GetCurrentPlayer() &&
            inUnit->GetPlayer()->mActiveGroup->Contains(inUnit))
            gUIManager->GetMainGameForm()->mGroups[0]->Reset();
    }
    inUnit->SetRegenerationTimer(theTime);
}

// FUNCTION: EXE 0x639d8a
// Regenerates a living unit's hit points each second up to its maximum (not during cinematics)
void EEUnitClass::Regenerate(EEComplexUnit* inUnit)
{
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    if (::gWorld.mInCinematic)
        return;
    long theTime = inUnit->mRegenerationTimer - gPhysics->mTickTimeMs;
    if (theTime <= 0) {
        long theHitPoints = theStats->mRegeneration + inUnit->mHitPoints;
        if (theHitPoints > theStats->mMaxHitPoints)
            theHitPoints = theStats->mMaxHitPoints;
        if (inUnit->mHitPoints > 0 && theHitPoints > 0) {
            inUnit->SetHitPoints(theHitPoints);
            if (inUnit->mPlayer == ::gWorld.GetCurrentPlayer() &&
                inUnit->mPlayer->mActiveGroup->Contains(inUnit))
                gUIManager->GetMainGameForm()->mGroups[0]->Reset();
        }
        theTime = 1000;
    }
    inUnit->SetRegenerationTimer(theTime);
}

// TEMPLATE: EXE 0x639e2d
// std::vector<GE3DPoint>::pop_back (FindPath)

// TEMPLATE: EXE 0x639e61
// std::vector<GE3DPoint>::erase(iterator) (pop_back)

// FUNCTION: EXE 0x639ec5
// Destroys a melee-range target screen and frees it when asked (scalar deleting destructor)
// (emitted with the vtable; folded by ICF with the other target screens' scalar deleting destructors)

// TEMPLATE: EXE 0x639ee2
// ?Remove@?$RTSRegion@PAVEEComplexUnit@@@@QAEXABQAVEEComplexUnit@@@Z

// TEMPLATE: EXE 0x639f15

// TEMPLATE: EXE 0x639fd2

// TEMPLATE: EXE 0x639ffb

// TEMPLATE: EXE 0x63a0ce

// TEMPLATE: EXE 0x63a0ff

// FUNCTION: EXE 0x63a19c
// Returns whether the first unit type's name sorts before the second's (EEUnitClassNameLess, for std::sort)

// TEMPLATE: EXE 0x63a21c

// TEMPLATE: EXE 0x63a256

// TEMPLATE: EXE 0x63a297

// TEMPLATE: EXE 0x63a3b7

// TEMPLATE: EXE 0x63a3ea

// TEMPLATE: EXE 0x63a42e

// TEMPLATE: EXE 0x63a457

// TEMPLATE: EXE 0x63a4c5

// TEMPLATE: EXE 0x63a518

// TEMPLATE: EXE 0x63a567

// FUNCTION: EXE 0x63a59b
// Returns the model of a turret part
// (EEGraphicObject::GetTurretPartModel: inline in ee/EEGraphicObject.h)

// FUNCTION: EXE 0x63a5ad
// Returns the transformation of a turret part
// (EEGraphicObject::GetTurretPartTransformation: inline in ee/EEGraphicObject.h)
