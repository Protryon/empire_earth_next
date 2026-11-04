// C:\EEX\Empire Earth\Units\EEUCBuildingPriest.cpp (file name inferred: no __FILE__; the class name). A building that
// converts (the temple): 0x21005e..0x21013b (after EEUCBuilding.cpp; EEUCFortress.cpp follows). Unit flags: the exe's
// module set. Its ctor is inline (EEUnitClass.cpp's factory); the slots pick the building's or the priest's handling
// by command/event. RTTI gives EEUCBuildingPriest no EEUCBasicPriest base, so the priest half is the class's own copy
// of EEUCBasicPriest's bodies (below the slots, no markers): identical source compiles to identical code, which LINK's
// ICF folds into EEUCBasicPriest's (0x20d59f, 0x20d7f2, 0x20d998, 0x20dd0a, 0x20dd2d, 0x20dee4, 0x20df05, 0x20e16a, 0x20e274; names.txt
// ICF aliases). Owner decision: no cast `this`.
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
#include "ee/EEUCBuilding.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCBuildingPriest.h"

void EEComputeSoundParameters(float inVolume, const GE3DPoint& inLocation, float& outVolume, float& outPan,
                              float& outPitch, float inFactor);

static const float kBuildingPriestSoundFactor = 4.0f;   // EEUCBasicPriest.cpp's kBasicPriestSoundFactor (0x446fe4)

// FUNCTION: EXE 0x61005e
// Handles the temple's new action: building actions (targets, dying, airport) as a building, the rest as a priest
void EEUCBuildingPriest::ActionChanged(EEComplexUnit* inUnit)
{
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionSearchForSecondaryTarget:
    case kActionTrackEnemyUnit:
    case kActionDying:
    case kActionResourcesRotting:
    case kActionPrepareToMove:
    case kActionAirportOpen:
    case kActionWaitForTakeoffClearance:
    case kActionLaunchNextAirplane:
        EEUCBuilding::ActionChanged(inUnit);
        break;
    default:
        this->PriestVirtual74(inUnit);
        break;
    }
}

// FUNCTION: EXE 0x6100b5
// Sets up a new temple as a building
void EEUCBuildingPriest::UnitCreated(EEComplexUnit* inUnit)
{
    EEUCBuilding::UnitCreated(inUnit);
}

// FUNCTION: EXE 0x6100c1
// Answers a trigger as a building, except the conversion triggers, which the priest half answers
long EEUCBuildingPriest::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = EEUCBuilding::EvaluateCondition(inUnit, inEvent);
    switch (inEvent) {
    case kTriggerEnemyUnitConverted:
    case kTriggerGoalIsConversion:
    case kTriggerConvertGoalStillValid:
    case kTriggerHasEnoughManaForConversion:
    case kTriggerAttackerIsLegalConversionTarget:
        theResult = this->PriestVirtualA0(inUnit, inEvent);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x61010c
// Runs the priest tick for a finished temple (a foundation only when dead or falling)
void EEUCBuildingPriest::Update(EEComplexUnit* inUnit)
{
    if (!inUnit->IsFoundation() || inUnit->mHitPoints <= 0 || inUnit->mCurrentPhysicsAction == 2)
        this->PriestVirtualB8(inUnit);
}

// The priest half: copies of EEUCBasicPriest's conversion, action, trigger, media, update and tower-effect bodies
// (no markers; see the header comment)

void EEUCBuildingPriest::UpdateConversion(EEComplexUnit* inUnit)
{
    if (inUnit->mPower < EEGetConversionCost(inUnit))
        return;
    EEComplexUnit* theTarget = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal())->mTarget->mComplexUnit;
    if (theTarget->mPlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[theTarget->mPlayer->mPlayerID] == 0) {
        inUnit->ReleaseConversionEffect();
        inUnit->ClearGoals(0);
        this->HideConversionEffects(inUnit);
        if (inUnit->mGoalState == NULL)
            inUnit->StartAction(kActionIdle);
        this->ActionChanged(inUnit);
        return;
    }
    if (inUnit->mConversionEffect.mValue == -1) {
        EEEffectManager* theManager = gEffectManager;
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
    long theTime = inUnit->mWorkTimer - (long)(gPhysics->mTickSeconds * -1000.0f);
    if ((unsigned long)theTime >= 1000) {
        float theResistance = theTarget->mStats->mConversionResistance;
        if (theResistance == 1.0f && gCivPowersManager.HasPower(inUnit->GetPlayer(), kCivPowerAdaptation) &&
            !inUnit->mUnitClass->IsCrusader() && inUnit->mUnitClass->mCategory != kUnitCategoryBuildingPriest &&
            theTarget->mUnitClass->mUnitType == kUnitTypeTownCenter)
            theResistance = 0.99f;
        float theChance = inUnit->mStats->mConversionRate;
        float theFactor = theTarget->mPlayer->mConversionFactor;
        if (gPhysics->mRandom.Generate() <= (1.0f - theResistance) * theFactor * theChance &&
            (theTarget->mTileNode == NULL || !theTarget->mComplexUnit->mIsDisabled || theTarget->mComplexUnit->IsProtected()))
            this->ConvertTarget(inUnit, theTarget);
        else
            inUnit->SetWorkTimer(theTime - 1000);
    } else
        inUnit->SetWorkTimer(theTime);
}

void EEUCBuildingPriest::ConvertTarget(EEComplexUnit* inUnit, EEComplexUnit* inTarget)
{
    std::vector<EEUnit*> theUnits;
    GE3DPoint theLocation;
    EEPlayer* theOldPlayer = inTarget->mPlayer;
    inUnit->ReleaseConversionEffect();
    this->HideConversionEffects(inUnit);
    EEComplexUnit* theConverted = EEConvertUnit(inTarget, inUnit->mPlayer->mPlayerID, 1);
    EEUnitClassUpgrade* theStats = theConverted->mStats;
    if (inUnit->mPlayer->mAreaConversion == true) {
        theLocation.SetX(theConverted->mPhysicalModel.GetLocation().GetX());
        theLocation.SetY(theConverted->mPhysicalModel.GetLocation().GetY());
        theLocation.SetZ(theConverted->mPhysicalModel.GetLocation().GetZ());
        EEGetUnitsNear(theLocation, gWorldTable.mEntries[0x23].mFloatValue, theUnits);
        for (long i = theUnits.size() - 1; i >= 0; i--) {
            EEUnit* theUnit = theUnits[i];
            if (theUnit->mComplexUnit == NULL || ((EEMapTile*)theUnit->mTile)->IsSeenBy(inUnit->GetPlayer()) != true)
                continue;
            bool theFlag;
            if (EECanConvert(inUnit, theUnit->mComplexUnit, theFlag))
                EEConvertUnit(static_cast<EEComplexUnit*>(theUnit), inUnit->mPlayer->mPlayerID, 1);
        }
    }
    inUnit->SetPower(inUnit->mPower - EEGetConversionCost(inUnit));
    if (inUnit->mConversionStartTime != 0)
        inUnit->mConversionStartTime = 0;
    EESoundObject* theSound = theStats->mCreationSound;
    if (theSound != NULL) {
        EEPlayer* theCurrent = ::gWorld.mPlayers[::gWorld.mCurrentPlayer];
        if (inUnit->mPlayer == theCurrent || theOldPlayer == theCurrent)
            gSoundManager->Play(theSound->mSampleID, 1.0f, 0.0f, 1.0f, false, NULL);
    }
}

void EEUCBuildingPriest::PriestVirtual74(EEComplexUnit* inUnit)
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
            dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
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

void EEUCBuildingPriest::PriestVirtualA8(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Load(false);
    EEUnitClass::LoadMedia(inPlayer);
}

long EEUCBuildingPriest::PriestVirtualA0(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    switch (inEvent) {
    case kTriggerEnemyUnitConverted:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
            if (theGoal != NULL) {
                EEPlayer* thePlayer = theGoal->mTarget->mPlayer;
                if (thePlayer == inUnit->mPlayer || inUnit->mPlayer->mEnemies[thePlayer->mPlayerID] == 0)
                    theResult = 1;
            }
        }
        break;
    case kTriggerGoalIsConversion:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
            if (theGoal != NULL && (!inUnit->mUnitClass->IsCrusader() || theGoal->mIsConvert))
                theResult = 1;
        }
        break;
    case kTriggerGoalIsNotConversion: {
        EEUnitGoal* theGoal = inUnit->mGoalState;
        if (theGoal == NULL || theGoal->mKind != kGoalAttackUnit ||
            (inUnit->mUnitClass->IsCrusader() && !static_cast<EEUGAttackUnit*>(theGoal)->mIsConvert))
            theResult = 1;
        break;
    }
    case kTriggerConvertGoalStillValid:
        if (inUnit->mGoalState != NULL) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetGoal());
            if (theGoal != NULL && theGoal->mTarget != NULL &&
                (theGoal->mTarget->mTileNode == NULL || !theGoal->mTarget->mComplexUnit->mIsDisabled ||
                 theGoal->mTarget->mComplexUnit->IsProtected()))
                theResult = 1;
        }
        break;
    case kTriggerHasEnoughManaForConversion:
        if (inUnit->mLastAttacker != NULL && inUnit->mPower >= EEGetConversionCost(inUnit))
            theResult = 1;
        break;
    case kTriggerAttackerIsLegalConversionTarget: {
        bool theFlag;
        if (inUnit->mLastAttacker != NULL && EECanConvert(inUnit, static_cast<EEComplexUnit*>(inUnit->mLastAttacker), theFlag))
            theResult = 1;
        break;
    }
    default:
        theResult = EEUnitClass::EvaluateCondition(inUnit, inEvent);
        break;
    }
    return theResult;
}

void EEUCBuildingPriest::PriestVirtualB4(EEPlayer* inPlayer)
{
    EEGraphicEffect::Get(kGraphicEffectPriestConversionIndiator)->Unload();
    EEUnitClass::UnloadMedia(inPlayer);
}

void EEUCBuildingPriest::PriestVirtualB8(EEComplexUnit* inUnit)
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
    this->Fn157269(inUnit);
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

void EEUCBuildingPriest::ShowConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest || inUnit->mCloaked != 0)
        return;
    EEEffectHandle theHandle;
    if (gEffectManager->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSwirlEffect), &theHandle, 0, 0, 0) == 0)
        inUnit->mCloaked = theHandle.mValue;
    if (gEffectManager->CreateUnitEffect(inUnit, EEGraphicEffect::Get(kGraphicEffectPriestTowerSmokeGenerator), &theHandle, 0, 0, 0) == 0)
        inUnit->SetGrowTime(theHandle.mValue);
    EESoundObject* theSound = EESoundObject::Get(kSoundPriestTowerConvert);
    GE3DPoint thePosition;
    inUnit->GetPosition(thePosition);
    float theVolume;
    float thePan;
    float thePitch;
    EEComputeSoundParameters(theSound->mVolume, thePosition, theVolume, thePan, thePitch, kBuildingPriestSoundFactor);
    gSoundManager->Play(theSound->mSampleID, theVolume, thePan, thePitch, false, NULL);
}

void EEUCBuildingPriest::HideConversionEffects(EEComplexUnit* inUnit)
{
    if (this->mCategory != kUnitCategoryBuildingPriest)
        return;
    EEEffectHandle theHandle;
    if ((long)inUnit->mCloaked > 0) {
        theHandle.mValue = inUnit->mCloaked;
        gEffectManager->DestroyEffect(theHandle);
        inUnit->mCloaked = 0;
    }
    if (!(inUnit->mGrowTime > 0))
        return;
    theHandle.mValue = inUnit->mGrowTime;
    gEffectManager->DestroyEffect(theHandle);
    inUnit->SetGrowTime(0);
}
