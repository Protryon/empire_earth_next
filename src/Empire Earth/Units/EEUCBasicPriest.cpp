// C:\EEX\Empire Earth\Units\EEUCBasicPriest.cpp (file name inferred: no __FILE__; the class name). The priests' common
// unit class (conversion): 0x20d571..0x20e2cb (after EEUCBasicCitizen.cpp; EEUCBomb.cpp follows). Unit flags: the
// exe's module set. Order: the conversion cost (static), UpdateConversion (command 0x7d4), ConvertTarget (the convert), Virtual74,
// Virtual78, the can-convert test (static), VirtualA8, VirtualA0, VirtualB4, VirtualB8, then EEComplexUnit's
// helpers that VirtualB8 calls (COMDATs after their first user: inline in the real header? written out of line
// here), ShowConversionEffects, HideConversionEffects.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

#include "ee/EEUnitKind.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEEffectManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EECombatFormulas.h"

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kBasicPriestNoHeight = -10000.0f;   // 0x446fe0 (provisional name)
static const float kBasicPriestSoundFactor = 4.0f;     // 0x446fe4 (the conversion sound's distance factor)


// FUNCTION: EXE 0x60d571
// Returns the faith a conversion by a unit costs: the world table's entry 46, at most the unit's own power (0 if none)
long EEGetConversionCost(EEComplexUnit* inUnit)
{
    EeCombatConversionCostIn theIn = {(int32_t)gWorldTable.GetLongValue(kWorldConversionFaithCost),
                                      (int32_t)inUnit->mStats->mPower};
    return EECombatConversionCost(theIn);
}

// FUNCTION: EXE 0x60d59f
// Works a conversion with enough faith: stops at an ally or own unit, shows the effects and warns the target's owner,
// then after the minimum time tries once a second (resistance, owner factor, rate) and converts on success
void EEUCBasicPriest::UpdateConversion(EEComplexUnit* inUnit)
{
    if (inUnit->GetPower() < EEGetConversionCost(inUnit))
        return;
    EEComplexUnit* theTarget = static_cast<EEComplexUnit*>(EEGoalCast<EEUGAttackUnit>(inUnit->GetGoal())->mTarget);
    if (theTarget->mPlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[theTarget->mPlayer->GetPlayerID()] == 0) {
        inUnit->ReleaseConversionEffect();
        inUnit->ClearGoals(0);
        this->HideConversionEffects(inUnit);
        if (inUnit->mGoalState == NULL)
            inUnit->StartAction(kActionIdle);
        this->ActionChanged(inUnit);
        return;
    }
    if (UIsNone32(inUnit->mConversionEffect.mValue)) {
        EEEffectManager* theManager = gWorld.GetEffectManager();
        EEEffectHandle theHandle;
        if (theManager->CreateUnitEffect(theTarget, EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator), &theHandle, 0, 0, 0) == 0 &&
            inUnit->mConversionEffect != theHandle)
            inUnit->mConversionEffect = theHandle;
        this->ShowConversionEffects(inUnit);
        EETileLocation theFrom(inUnit->mTileX, inUnit->mTileY);
        EETileLocation theTile(theTarget->mTileX, theTarget->mTileY);
        theTarget->mPlayer->mEvents.AddPlayerEvent(theTile, theFrom, 1, theTarget->GetPlayer(), -1, false);
    }
    if (::gWorld.mInCinematic)
        return;
    if (gPhysics->mGameTime - inUnit->mConversionStartTime < (unsigned long)this->mMinConversionTime)
        return;
    EeCombatConversionIn theIn;
    theIn.work_timer = (int32_t)inUnit->mWorkTimer;
    theIn.tick_seconds = gPhysics->mTickSeconds;
    theIn.resistance = theTarget->mStats->mConversionResistance;
    theIn.adaptation = theIn.resistance == 1.0f && gCivPowersManager.HasPower(inUnit->GetPlayer(), kCivPowerAdaptation) &&
                       !inUnit->mUnitClass->IsCrusader() && inUnit->mUnitClass->mCategory != kUnitCategoryBuildingPriest &&
                       theTarget->mUnitClass->mUnitType == kUnitTypeTownCenter;
    theIn.rate = inUnit->mStats->mConversionRate;
    theIn.factor = theTarget->mPlayer->mConversionFactor;
    EeCombatConversionOut theOut = EECombatConversion(theIn);
    long theTime = theOut.timer;
    if (theOut.due) {
        if (gPhysics->mRandom.Generate() <= theOut.threshold &&
            (theTarget->mTileNode == NULL || !theTarget->GetComplexUnit()->mIsDisabled || theTarget->GetComplexUnit()->IsProtected()))
            this->ConvertTarget(inUnit, theTarget);
        else
            inUnit->SetWorkTimer(theOut.miss_timer);
    } else
        inUnit->SetWorkTimer(theTime);
}

// FUNCTION: EXE 0x60d7f2
// Converts the target to the priest's player (with area conversion, also every convertible visible unit nearby),
// spends the faith and plays the converted unit's creation sound for the two players involved
void EEUCBasicPriest::ConvertTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    bool theFlag;
    std::vector<EEUnit*> theUnits;
    GE3DPoint theLocation;
    EEPlayer* theOldPlayer = inTarget->mPlayer;
    inUnit->ReleaseConversionEffect();
    this->HideConversionEffects(inUnit);
    EEComplexUnit* theConverted = EEConvertUnit(inTarget, inUnit->mPlayer->GetPlayerID(), 1);
    EEUnitClassUpgrade* theStats = theConverted->mStats;
    if (inUnit->mPlayer->mAreaConversion == true) {
        const GETransformation& theConvertedLocation = theConverted->mPhysicalModel.GetLocation();
        theLocation.SetX(theConvertedLocation.GetX());
        theLocation.SetY(theConvertedLocation.GetY());
        theLocation.SetZ(theConvertedLocation.GetZ());
        EEGetUnitsNear(theLocation, gWorldTable.GetFloatValue(kWorldAreaConversionRadius), theUnits);
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = theUnits[i];
            if (theUnit->GetComplexUnit() != NULL && ((EEMapTile*)theUnit->mTile)->IsSeenBy(inUnit->GetPlayer()) == true) {
                if (EECanConvert(inUnit, theUnit->GetComplexUnit(), theFlag))
                    EEConvertUnit(static_cast<EEComplexUnit*>(theUnit), inUnit->mPlayer->GetPlayerID(), 1);
            }
        }
    }
    inUnit->SetPower(inUnit->mPower - EEGetConversionCost(inUnit));
    if (inUnit->mConversionStartTime != 0)
        inUnit->mConversionStartTime = 0;
    EESoundObject* theSound = theStats->mCreationSound;
    if (theSound == NULL)
        return;
    EEPlayer* theCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
    if (inUnit->mPlayer == theCurrent || theOldPlayer == theCurrent)
        gSoundManager->Play(theSound->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL);
}

// FUNCTION: EXE 0x60d998
// Drops the conversion and heal effects, then starts a conversion (stands, clock started once), waits for reload,
// or uses the common handling
void EEUCBasicPriest::ActionChanged(EEComplexUnit* inUnit)
{
    inUnit->ReleaseConversionEffect();
    inUnit->ReleaseHealEffect();
    this->HideConversionEffects(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionConvertEnemyUnit:
        inUnit->SetCurrentPhysicsAction(1);
        inUnit->SetTargetSearchTimer(0);
        inUnit->SetAnimation(9, -1, 0);
        if (inUnit->mConversionStartTime == 0) {
            if (inUnit->mConversionStartTime != gPhysics->mGameTime)
                inUnit->mConversionStartTime = gPhysics->mGameTime;
            inUnit->SetWorkTimer(0);
            EEGoalCast<EEUGAttackUnit>(inUnit->GetGoal());
        }
        inUnit->DropPath();
        break;
    case kActionWaitForReload:
        inUnit->SetCurrentPhysicsAction(3);
        break;
    default:
        this->HandleActionChanged(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x60da7a
// Sets up a new priest like any unit
void EEUCBasicPriest::UnitCreated(EEComplexUnit* inUnit)
{
    EEUnitClass::UnitCreated(inUnit);
}

// FUNCTION: EXE 0x60da86
// Returns whether a priest can convert a target now (enemy, resistance, faith, priests and buildings rules, the
// temple's range); outFlag is set when the target is shielded
// (new: the decision is ee_combat's can_convert over a record of pure reads, gathered here; the damage table is
// read only for a valid ammo column, the shield and the building priest's distance always: none has side effects)
bool EECanConvert(EEComplexUnit* inPriest, EEComplexUnit* inTarget, bool& outFlag)
{
    long theCost = EEGetConversionCost(inPriest);
    EEUnitClass* theClass = inTarget->mUnitClass;
    EEUnitClass* thePriestClass = inPriest->mUnitClass;
    EeCombatCanConvertIn in = {};
    in.crusader = thePriestClass->IsCrusader();
    in.building_priest = thePriestClass->mCategory == kUnitCategoryBuildingPriest;
    in.adaptation = gCivPowersManager.HasPower(inPriest->GetPlayer(), kCivPowerAdaptation);
    in.same_player = inTarget->mPlayer == inPriest->mPlayer;
    in.enemy = inPriest->mPlayer->mEnemies[inTarget->mPlayer->GetPlayerID()] != 0;
    in.conversion_rate = inPriest->mStats->mConversionRate;
    in.target_disabled = inTarget->mIsDisabled;
    in.target_protected = inTarget->IsProtected();
    in.resistance = inTarget->mStats->mConversionResistance;
    in.target_town_center = theClass->mUnitType == kUnitTypeTownCenter;
    in.target_settlement = theClass->mUnitType == kUnitTypeSettlement;
    in.target_hit_points = (int32_t)inTarget->mHitPoints;
    in.attackable = theClass->CanBeAttacked();
    in.damage = thePriestClass->mAmmoType >= 0 ? gDamageTable[theClass->mFamily][thePriestClass->mAmmoType] : 0.0f;
    in.same_category = theClass->mCategory == thePriestClass->mCategory;
    in.converts_priests = inPriest->mStats->mConvertsPriests;
    in.power = (int32_t)inPriest->GetPower();
    in.cost = (int32_t)theCost;
    in.target_land_priest = theClass->mCategory == kUnitCategoryLandPriest;
    in.converts_buildings = inPriest->mStats->mConvertsBuildings;
    in.target_land_building = theClass->IsLandBuilding();
    in.at_copper = inPriest->mPlayer->mTechnologies->mCurrentEpoch >= kTechEpochCopper;
    in.priest_x = inPriest->mPhysicalModel.GetLocation().GetX();
    in.priest_y = inPriest->mPhysicalModel.GetLocation().GetY();
    in.priest_z = inPriest->mPhysicalModel.GetLocation().GetZ();
    in.target_x = inTarget->mPhysicalModel.GetLocation().GetX();
    in.target_y = inTarget->mPhysicalModel.GetLocation().GetY();
    in.target_z = inTarget->mPhysicalModel.GetLocation().GetZ();
    in.range = inPriest->mStats->mRange;
    in.priest_cyber = thePriestClass->mIsCyber;
    in.target_cyber = theClass->mIsCyber;
    in.shielded = inTarget->HasEffect(4);
    EeCombatCanConvertOut theOut = ee_combat_can_convert(&in);
    if (theOut.flag_written)
        outFlag = theOut.flag != 0;
    EECombatCapture(EE_COMBAT_TAG('C', 'B', 'C', 'N'), in, theOut);
    return theOut.result != 0;
}

// FUNCTION: EXE 0x60dd0a
// Loads the conversion indicator effect and the class's media
void EEUCBasicPriest::LoadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false);
    EEUnitClass::LoadMedia(inPlayer);
}

// FUNCTION: EXE 0x60dd2d
// Answers the conversion triggers (attacker convertible, enough faith, goal still valid, goal is or is not a
// conversion, target converted); the faith check uses the attacker's conversion cost
long EEUCBasicPriest::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    case kTriggerAttackerIsLegalConversionTarget: {
        bool theFlag;
        theResult = inUnit->mLastAttacker != NULL && EECanConvert(inUnit, static_cast<EEComplexUnit*>(inUnit->mLastAttacker), theFlag) ? 1 : 0;
        break;
    }
    case kTriggerHasEnoughManaForConversion:
        theResult = inUnit->mLastAttacker != NULL && inUnit->GetPower() >= EEGetConversionCost(inUnit) ? 1 : 0;
        break;
    case kTriggerConvertGoalStillValid:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(inUnit->GetGoal());
            if (theGoal != NULL && theGoal->mTarget != NULL &&
                (theGoal->mTarget->mTileNode == NULL || !theGoal->mTarget->GetComplexUnit()->mIsDisabled ||
                 theGoal->mTarget->GetComplexUnit()->IsProtected()))
                theResult = 1;
            else
                theResult = 0;
        }
        break;
    case kTriggerGoalIsNotConversion: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if (theGoal == NULL || theGoal->mKind != kGoalAttackUnit ||
            (inUnit->GetUnitClass()->IsCrusader() && !static_cast<EEUGAttackUnit*>(theGoal)->mIsConvert))
            theResult = 1;
        break;
    }
    case kTriggerGoalIsConversion:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(inUnit->GetGoal());
            if (theGoal != NULL && (!inUnit->GetUnitClass()->IsCrusader() || theGoal->mIsConvert))
                theResult = 1;
        }
        break;
    case kTriggerEnemyUnitConverted:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = EEGoalCast<EEUGAttackUnit>(inUnit->GetGoal());
            if (theGoal != NULL) {
                EEPlayer* thePlayer = theGoal->mTarget->GetPlayer();
                if (thePlayer == inUnit->GetPlayer() || inUnit->GetPlayer()->mEnemies[thePlayer->GetPlayerID()] == 0)
                    theResult = 1;
            }
        }
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x60dee4
// Unloads the conversion indicator effect and the class's media
void EEUCBasicPriest::UnloadMedia(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload();
    EEUnitClass::UnloadMedia(inPlayer);
}

// FUNCTION: EXE 0x60df05
// Runs the priest's tick (physics, timers, animation, healing, regeneration, faith), then attacks or converts; when
// not converting, ends any conversion and its effects
void EEUCBasicPriest::Update(EEComplexUnit* inUnit)
{
    long theFrame = inUnit->mPhysicalModel.GetFrame();
    inUnit->SetLastHitPoints(inUnit->mHitPoints);
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000) {
        this->mPhysicsModel->Step(inUnit);
        inUnit->UpdateTurrets();
        inUnit->UpdateOrbitals();
    }
    this->UpdateEffects(inUnit);
    inUnit->UpdateReloadTimer();
    inUnit->UpdateTargetSearchTimer();
    this->UpdateCamouflageTimer(inUnit);
    long theAttack = this->Animate(inUnit);
    this->RefreshSound(inUnit);
    if (this->mIsMedic) {
        inUnit->UpdateHealSearchTimer();
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalHealUnit)
            inUnit->mGoalState->Work(inUnit);
    }
    if (inUnit->mStats->mRegeneration > 0)
        this->Regenerate(inUnit);
    this->UpdateDone(inUnit);
    this->RegeneratePower(inUnit);
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionAttackEnemyUnit:
    case kActionAttackLocation:
    case kActionWaitForReload:
        this->FireOnAttackFrame(inUnit, theAttack, theFrame);
        break;
    case kActionConvertEnemyUnit:
        this->UpdateConversion(inUnit);
        return;
    case kActionResourcesRotting:
        return;
    case kActionUnitRotting: {
        long theTime = inUnit->mRottingAwayTime - gPhysics->mTickTimeMs;
        if (theTime < 0)
            theTime = 0;
        inUnit->SetRottingAwayTime(theTime);
        break;
    }
    }
    if (inUnit->mConversionStartTime != 0)
        inUnit->mConversionStartTime = 0;
    inUnit->ReleaseConversionEffect();
    this->HideConversionEffects(inUnit);
}

// FUNCTION: EXE 0x60e086
// Updates each of the unit's orbitals
void EEComplexUnit::UpdateOrbitals()
{
    unsigned long theCount = this->mNumOrbitals;
    if (theCount != 0) {
        for (unsigned long i = 0; i < theCount; i++)
            this->mOrbitals[i]->Update();
    }
}

// FUNCTION: EXE 0x60e0b2
// Counts the reload timer down by the tick, not below 0
void EEComplexUnit::UpdateReloadTimer()
{
    if (!(this->mReloadTimer > 0))
        return;
    long theTime = this->mReloadTimer - gPhysics->mTickTimeMs;
    if (theTime < 0)
        theTime = 0;
    this->SetReloadTimer(theTime);
}

// FUNCTION: EXE 0x60e0db
// Counts the target search timer down by the tick, not below 0
void EEComplexUnit::UpdateTargetSearchTimer()
{
    if (!(this->mTargetSearchTimer > 0))
        return;
    long theTime = this->mTargetSearchTimer - gPhysics->mTickTimeMs;
    if (theTime < 0)
        theTime = 0;
    this->SetTargetSearchTimer(theTime);
}

// FUNCTION: EXE 0x60e104
// Counts the heal search timer (seconds) down by the tick, not below 0
void EEComplexUnit::UpdateHealSearchTimer()
{
    float theValue = this->mHealSearchTimer;
    if (!(theValue > 0.0f))
        return;
    theValue -= gPhysics->mTickTime / 1000.0f;
    if (theValue < 0.0f)
        theValue = 0.0f;
    this->SetHealSearchTimer(theValue);
}

// FUNCTION: EXE 0x60e14c
// Turns the unit's turrets when its model has parts
void EEComplexUnit::UpdateTurrets()
{
    if (this->GetModel()->GetNumParts() != 0)
        this->mUnitClass->TurnTurrets(this);
}

// FUNCTION: EXE 0x60e16a
// For a temple not already showing them, starts the tower swirl and smoke effects and plays the tower convert sound
void EEUCBasicPriest::ShowConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest || inUnit->mCloaked != 0)
        return;
    EEEffectHandle theHandle;
    if (gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSwirlEffect), &theHandle, 0, 0, 0) == 0)
        inUnit->mCloaked = theHandle.mValue;
    if (gWorld.GetEffectManager()->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSmokeGenerator), &theHandle, 0, 0, 0) == 0)
        inUnit->SetGrowTime(theHandle.mValue);
    EESoundObject* theSound = EESoundObject::Get(kSoundPriestTowerConvert);
    GE3DPoint thePosition;
    inUnit->GetPosition(thePosition);
    float theVolume;
    float thePan;
    float thePitch;
    EEComputeSoundParameters(theSound->mVolume, thePosition, theVolume, thePan, thePitch, kBasicPriestSoundFactor);
    gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, false, NULL);
}

// FUNCTION: EXE 0x60e274
// For a temple, destroys the tower swirl and smoke effects
void EEUCBasicPriest::HideConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest)
        return;
    EEEffectHandle theHandle;
    if ((long)inUnit->mCloaked > 0) {
        theHandle.mValue = inUnit->mCloaked;
        gWorld.GetEffectManager()->DestroyEffect(theHandle);
        inUnit->mCloaked = 0;
    }
    if (!(inUnit->mGrowTime > 0))
        return;
    theHandle.mValue = inUnit->mGrowTime;
    gWorld.GetEffectManager()->DestroyEffect(theHandle);
    inUnit->SetGrowTime(0);
}

// ICF-FOLDED: EXE 0x418aa9
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EEUCBasicPriest::CanAttackMove()
{
    return true;
}

// ICF-FOLDED: EXE 0x60d336
// Updates a living citizen's water ripples when the effect detail allows them
// (no address of its own: the same body as EEUCBasicCitizen::UpdateEffects, which ICF kept)
void EEUCBasicPriest::UpdateEffects(EEComplexUnit* inUnit)
{
    if (this->mRippleEffect != NULL && inUnit->mHitPoints != 0 && (unsigned long)this->mRippleEffect->mDetailLevel <= gEffectDetail)
        this->UpdateRipples(inUnit);
}
