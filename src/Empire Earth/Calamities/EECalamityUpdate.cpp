// New (port): the host of the calamity classes' Rust updates (ee/EECalamityUpdate.h; ee_tech calamity_update.rs;
// docs/RUST_PLAN.md R5b.2). The calls every class shares: the contagion helper, the sounds, the units near the
// target and what the damage loops read of them, damage, the physics random draws, the target's and the caster's
// state, the terrain height, the effect manager's DestroyEffect. The rest goes to the class's UpdateStep.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIKindIDs.h"

namespace {

void StateFrom(EECalamity* inCalamity, EeCalamityState& outState)
{
    outState.time_alive = inCalamity->mTimeAlive;
    outState.float1 = inCalamity->mFloatPlaceHolder1;
    outState.float2 = inCalamity->mFloatPlaceHolder2;
    outState.int1 = (int32_t)inCalamity->mIntPlaceHolder1;
    outState.int2 = (int32_t)inCalamity->mIntPlaceHolder2;
    outState.target[0] = inCalamity->mTargetLocation.mX;
    outState.target[1] = inCalamity->mTargetLocation.mY;
    outState.target[2] = inCalamity->mTargetLocation.mZ;
    outState.epicenter_factor = inCalamity->mEpicenterFactor;
    outState.effect_handle = (uint32_t)inCalamity->mEffectHandle.mValue;
    outState.bool1 = inCalamity->mBoolPlaceHolder1;
    outState.bool2 = inCalamity->mBoolPlaceHolder2;
}

void StateTo(const EeCalamityState& inState, EECalamity* outCalamity)
{
    outCalamity->mTimeAlive = inState.time_alive;
    outCalamity->mFloatPlaceHolder1 = inState.float1;
    outCalamity->mFloatPlaceHolder2 = inState.float2;
    outCalamity->mIntPlaceHolder1 = inState.int1;
    outCalamity->mIntPlaceHolder2 = inState.int2;
    outCalamity->mTargetLocation.mX = inState.target[0];
    outCalamity->mTargetLocation.mY = inState.target[1];
    outCalamity->mTargetLocation.mZ = inState.target[2];
    outCalamity->mEpicenterFactor = inState.epicenter_factor;
    outCalamity->mEffectHandle.mValue = inState.effect_handle;
    outCalamity->mBoolPlaceHolder1 = inState.bool1;
    outCalamity->mBoolPlaceHolder2 = inState.bool2;
}

EEUnit* TargetUnit(EECalamity* inCalamity)
{
    EEUnit* theUnit = NULL;
    inCalamity->GetTargetUnit(theUnit);
    return theUnit;
}

// the calls every class shares; false for a class's own
bool CommonCall(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECalamity* theCalamity = ioHost.mCalamity;
    EECalamityClass* theClass = ioHost.mClass;
    switch (inCall.op) {
    case EeCalamityOp::HelperUpdate: {
        EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inCall.index);
        EECalamityCaptureEvent(kCalamityOpHelperUpdate, inCall.index, theHelper != NULL);   // CALW (TEST ONLY)
        if (theHelper != NULL)
            EECalamityCaptureContagion(theHelper, inCall.x);   // CONT (TEST ONLY): theHelper->Update
        return true;
    }
    case EeCalamityOp::Sound:
        theClass->UpdateSound(theCalamity, ioHost.mSounds[inCall.index != 0 ? 1 : 0], inCall.x, inCall.y, inCall.value != 0);
        return true;
    case EeCalamityOp::UnitsNear: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        long theNear = EEGetUnitsNear(theLocation, inCall.w, ioHost.mUnits);
        EECalamityCaptureEvent(kCalamityOpUnitsNear, theNear);   // CALW (TEST ONLY)
        outReply.value = (int32_t)theNear;
        outReply.id = (uint32_t)ioHost.mUnits.size();
        return true;
    }
    case EeCalamityOp::CasterContinent:
        outReply.value = gContinentManager.GetContinent(theCalamity->mCasterUnit, kAIPathNonSpace);
        return true;
    case EeCalamityOp::QuakeUnit: {
        // the earthquake's tests in its order: a land building, alive, then the continent
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        outReply.id = theUnit->mUnitID;
        if (theUnit->mUnitClass->IsLandBuilding()) {
            outReply.flags |= 1;
            if (theUnit->mHitPoints > 0) {
                outReply.flags |= 2;
                outReply.value = gContinentManager.GetContinent(theUnit, kAIPathNonSpace);
            }
        }
        return true;
    }
    case EeCalamityOp::Immune: {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        EEGraphicEffect* theEffect = NULL;
        if (theUnit->GetEffect(3, theEffect)) {
            EECalamityCaptureEvent(kCalamityOpImmune, theUnit->mUnitID);   // CALW (TEST ONLY)
            if (theEffect != NULL)
                gWorld.GetEffectManager()->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
            outReply.ok = true;
        }
        return true;
    }
    case EeCalamityOp::Carried: {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(ioHost.mUnits[inCall.index]);
        if (theUnit->IsCarried()) {
            outReply.ok = true;
            outReply.key = (uint64_t)(uintptr_t)theUnit->GetOutermostCarrier();
        }
        return true;
    }
    case EeCalamityOp::TakeHit: {
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        EEUnit* theAttacker = inCall.x != 0.0f ? TargetUnit(theCalamity) : theCalamity->mCasterUnit;
        long theMode = theClass->mIsBlockedByImmunity ? 2 : 3;
        EECalamityCaptureEvent(kCalamityOpTakeHit, theUnit->mUnitID, inCall.value, (float)theCalamity->mPlayerNumber,
                               (float)theMode);   // CALW (TEST ONLY)
        theUnit->TakeHit(theAttacker, inCall.value, theCalamity->mPlayerNumber, theMode);
        return true;
    }
    case EeCalamityOp::StormUnit: {
        // the hurricane's tests in its order, stopping at the first that skips the unit
        EEUnit* theStorm = TargetUnit(theCalamity);
        EEComplexUnit* theStormUnit = theStorm->mComplexUnit;
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        outReply.id = theUnit->mUnitID;
        if (theUnit->mUnitID == theStorm->mUnitID)
            outReply.flags |= 1;
        else if (theUnit->mPlayer == theStormUnit->mPlayer)
            outReply.flags |= 2;
        else if (theStormUnit->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] != 0) {
            outReply.flags |= 4;
            if (gUnitSetManager.Contains(theUnit->mUnitClass->GetUnitType(), theClass->GetUnitSet())) {
                outReply.flags |= 8;
                if (theUnit->mHitPoints > 0) {
                    outReply.flags |= 16;
                    outReply.x = EEGetDistanceSquared(static_cast<EEComplexUnit*>(theUnit), theStormUnit);
                    outReply.y = EEGetUnitRadius(static_cast<EEComplexUnit*>(theUnit), theStormUnit);
                    if (theUnit->mPlayer == ::gWorld.mPlayers[0])
                        outReply.flags |= 32;
                }
            }
        }
        return true;
    }
    case EeCalamityOp::Random: {
        float theRandom = gPhysics->GetRandom();
        EECalamityCaptureEvent(kCalamityOpRandom, 0, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::RandomGenerate: {
        float theRandom = gPhysics->mRandom.Generate();
        EECalamityCaptureEvent(kCalamityOpRandom, 0, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::RandomRange: {
        float theRandom = gPhysics->GetRandom(inCall.x, inCall.y);
        EECalamityCaptureEvent(kCalamityOpRandom, 1, 0, theRandom);   // CALW (TEST ONLY)
        outReply.x = theRandom;
        return true;
    }
    case EeCalamityOp::TargetExists:
        outReply.ok = TargetUnit(theCalamity) != NULL;
        return true;
    case EeCalamityOp::PlayClassSound:
        theClass->PlaySound(theCalamity);
        return true;
    case EeCalamityOp::TargetHitPoints:
        outReply.value = (int32_t)TargetUnit(theCalamity)->mComplexUnit->mHitPoints;
        return true;
    case EeCalamityOp::DestroyEffect: {
        EEEffectHandle theHandle;
        theHandle.mValue = (uint32_t)inCall.value;
        EECalamityCaptureEvent(kCalamityOpDestroyEffect, theHandle.mValue);   // CALW (TEST ONLY)
        gEffectManager->DestroyEffect(theHandle);
        return true;
    }
    case EeCalamityOp::TargetTinted:
        outReply.ok = TargetUnit(theCalamity)->mComplexUnit->mIsTinted;
        return true;
    case EeCalamityOp::CasterHitPoints:
        outReply.value = (int32_t)theCalamity->mCasterUnit->mHitPoints;
        return true;
    case EeCalamityOp::CasterExists:
        outReply.ok = theCalamity->mCasterUnit != NULL;
        return true;
    case EeCalamityOp::TargetDead: {
        EEUnit* theTarget = NULL;
        outReply.ok = theCalamity->GetTargetUnit(theTarget) && theTarget->GetComplexUnit() != NULL &&
                      theTarget->GetComplexUnit()->GetHitPoints() <= 0;
        return true;
    }
    case EeCalamityOp::MeshZ:
        outReply.x = ::gWorld.mMap->mTerrainMesh.GetMeshZ(inCall.x, inCall.y);
        return true;
    case EeCalamityOp::TargetAlive: {
        EEUnit* theTarget = NULL;
        outReply.ok = theCalamity->GetTargetUnit(theTarget) && theTarget->mHitPoints > 0;
        return true;
    }
    // the starts' and stops' (calamity_start.rs)
    case EeCalamityOp::StopSound:
        if (inCall.index == 0)
            theCalamity->StopSound();
        else
            theCalamity->StopSecondarySound();
        return true;
    case EeCalamityOp::DestroyHelper:
        ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(inCall.index), inCall.value);
        return true;
    case EeCalamityOp::StartHelper: {
        long theHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(theClass->GetContagionFunctor(), theCalamity, NULL);
        outReply.ok = ::gWorld.GetContagionManager()->GetContagionHelper(theHelperID) != NULL;
        outReply.value = (int32_t)theHelperID;
        return true;
    }
    case EeCalamityOp::StartSound: {
        EESoundObject* theSound = ioHost.mSounds[0];
        float theVolume = theSound->GetVolume();
        float thePan = 0.0f;
        float thePitch = 1.0f;
        if (inCall.value != 0) {
            GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
            EEComputeSoundParameters(theSound->GetVolume(), theLocation, theVolume, thePan, thePitch, kCalamitySoundFalloff);
            EECalamityCaptureStartEvent(kCalamityStartSound, 0, 0, theVolume, thePan);   // CALT (TEST ONLY)
        }
        theCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(theSound->GetSampleID(), theVolume, thePan, thePitch,
                                                                         theSound->GetLooping(), NULL));
        if (theCalamity->mSound != NULL)
            theCalamity->mSound->Dereference();
        return true;
    }
    case EeCalamityOp::SetHeight:
        theCalamity->mHeight = inCall.x;
        return true;
    case EeCalamityOp::KillTarget: {
        EEUnit* theUnit = TargetUnit(theCalamity);
        theUnit->Kill(0, 0, 0);
        theUnit->mPlayer->DeselectUnit(theUnit);
        theCalamity->SetTargetUnit(NULL);
        return true;
    }
    case EeCalamityOp::CasterSightOff:
        theCalamity->mCasterUnit->mComplexUnit->SightOffHere();
        return true;
    case EeCalamityOp::CasterSight:
        outReply.x = theCalamity->mCasterUnit->mComplexUnit->GetSight();
        return true;
    case EeCalamityOp::CasterSightOn: {
        EEComplexUnit* theUnit = theCalamity->mCasterUnit->mComplexUnit;
        theUnit->SetSight(inCall.x);
        theUnit->SightOnHere();
        return true;
    }
    case EeCalamityOp::TargetProtected:
        outReply.ok = TargetUnit(theCalamity)->mComplexUnit->IsProtected();
        return true;
    case EeCalamityOp::TileTerrain: {
        EEMapTile* theTile = ::gWorld.mMap->GetTile(inCall.index, inCall.index2);
        if (theTile != NULL) {
            outReply.ok = true;
            outReply.value = (int32_t)theTile->mTerrainType;
        }
        return true;
    }
    case EeCalamityOp::TargetComplexAlive: {
        EEUnit* theTarget = NULL;
        EEComplexUnit* theUnit;
        outReply.ok = theCalamity->GetTargetUnit(theTarget) && (theUnit = theTarget->mComplexUnit) != 0 && theUnit->mHitPoints > 0;
        return true;
    }
    default:
        return false;
    }
}

void HostCall(void* inContext, EeCalamityState* ioState, const EeCalamityCall* inCall, EeCalamityReply* outReply)
{
    EECalamityUpdateHost& theHost = *static_cast<EECalamityUpdateHost*>(inContext);
    // the calamity as the Rust update has it, for the world step; and back after it
    StateTo(*ioState, theHost.mCalamity);
    *outReply = EeCalamityReply();
    if (!CommonCall(theHost, *inCall, *outReply)) {
        bool theAnswered = theHost.mStep != NULL && theHost.mStep(theHost, *inCall, *outReply);
        ESVERIFY_AT(theAnswered, 1);
    }
    StateFrom(theHost.mCalamity, *ioState);
}

}   // namespace

EeCalamityParams EECalamityBaseParams(EECalamityClass* inClass, EESoundObject* inSound, EESoundObject* inSecondarySound)
{
    EeCalamityParams theParams = EeCalamityParams();
    theParams.class_enum = (int32_t)inClass->mClassEnum;
    theParams.lifetime = inClass->mLifeTime;
    theParams.flags = (inClass->mIsBlockedByImmunity ? EE_CALAMITY_BLOCKED_BY_IMMUNITY : 0) |
                      (inSound != NULL ? EE_CALAMITY_SOUND : 0) | (inSecondarySound != NULL ? EE_CALAMITY_SECONDARY_SOUND : 0);
    return theParams;
}

namespace {

void RunHost(EECalamityUpdateHost& outHost, EeCalamityHostFfi& outFfi, EECalamityClass* inClass, EECalamity* inCalamity,
             EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep)
{
    outHost.mClass = inClass;
    outHost.mCalamity = inCalamity;
    outHost.mSounds[0] = inSound;
    outHost.mSounds[1] = inSecondarySound;
    outHost.mStep = inStep;
    outHost.mPending = NULL;
    outFfi.ctx = &outHost;
    outFfi.call = &HostCall;
}

}   // namespace

bool EECalamityRunStart(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                        EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep)
{
    EECalamityUpdateHost theHost;
    EeCalamityHostFfi theFfi;
    RunHost(theHost, theFfi, inClass, inCalamity, inSound, inSecondarySound, inStep);
    EeCalamityState theState;
    StateFrom(inCalamity, theState);
    bool theResult = ee_calamity_start(&inParams, &theState, &theFfi);
    StateTo(theState, inCalamity);
    return theResult;
}

void EECalamityRunStop(EECalamityClass* inClass, EECalamity* inCalamity, const EeCalamityParams& inParams,
                       EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep)
{
    EECalamityUpdateHost theHost;
    EeCalamityHostFfi theFfi;
    RunHost(theHost, theFfi, inClass, inCalamity, inSound, inSecondarySound, inStep);
    EeCalamityState theState;
    StateFrom(inCalamity, theState);
    ee_calamity_stop(&inParams, &theState, &theFfi);
    StateTo(theState, inCalamity);
}

long EECalamityRunUpdate(EECalamityClass* inClass, EECalamity* inCalamity, float inElapsedTime, const EeCalamityParams& inParams,
                         EESoundObject* inSound, EESoundObject* inSecondarySound, EECalamityStepFn inStep)
{
    EECalamityUpdateHost theHost;
    theHost.mClass = inClass;
    theHost.mCalamity = inCalamity;
    theHost.mSounds[0] = inSound;
    theHost.mSounds[1] = inSecondarySound;
    theHost.mStep = inStep;
    theHost.mPending = NULL;
    EeCalamityHostFfi theFfi;
    theFfi.ctx = &theHost;
    theFfi.call = &HostCall;
    EeCalamityState theState;
    StateFrom(inCalamity, theState);
    int32_t theResult = ee_calamity_update(&inParams, &theState, inElapsedTime, &theFfi);
    StateTo(theState, inCalamity);
    return theResult;
}
