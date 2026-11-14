// C:\EEX\Empire Earth\Calamities\EECCHurricane.cpp (file name inferred: no __FILE__; the class name). The hurricane
// calamity class: 0xb114a..0xb18e2. Unit flags: the exe's module set.
// Data (.rdata): the vtable 0x43ca1c (COL 0x43ca18).
#include <windows.h>

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
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCHurricane.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEEffectKindIDs.h"

// FUNCTION: EXE 0x4b1226
// Creates the storm unit centred on the target location at the calamity height, with the storm effect on it, and makes
// it the calamity's target; returns false if the unit could not be created
bool EECCHurricane::CreateStorm(EECalamity* inCalamity)
{
    GETransformation theTransformation;
    bool theResult = false;
    GE3DPoint theLocation;
    theLocation = inCalamity->mTargetLocation;
    float theHalfSize = gUnitClassTable[this->mUnitClassIndex]->mTileSize * 0.5f;
    theLocation.SetX(theLocation.GetX() - theHalfSize);
    theLocation.SetY(theLocation.GetY() - theHalfSize);
    theLocation.SetZ(inCalamity->mHeight);
    theTransformation.SetX(theLocation.GetX());
    theTransformation.SetY(theLocation.GetY());
    theTransformation.SetZ(theLocation.GetZ());
    EEUnit* theUnit = EECreateCalamityUnit(inCalamity, this->mUnitClassIndex, theTransformation, -1);
    if (theUnit != NULL) {
        gEffectManager->CreateUnitEffect(theUnit, this->mGraphicEffect2, NULL, 0, 0, 0);
        theUnit->mComplexUnit->SetBuilt(1, 0, 1);
        inCalamity->SetTargetUnit(theUnit);
        theResult = true;
    }
    return theResult;
}

// New (port): EECCHurricane's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCHurricane::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCHurricane* theClass = static_cast<EECCHurricane*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::CreateStorm: {
        bool theCreated = theClass->CreateStorm(theCalamity);
        EECalamityCaptureEvent(kCalamityOpCreateStorm, theCreated);   // CALW (TEST ONLY)
        return true;
    }
    case EeCalamityOp::StormPrepare: {
        EEUnit* theStorm = NULL;
        theCalamity->GetTargetUnit(theStorm);
        EEComplexUnit* theStormUnit = theStorm->mComplexUnit;
        theStormUnit->SetIsDeletable(false);
        const GETransformation& theStormLocation = theStormUnit->mPhysicalModel.GetLocation();
        outReply.x = theStormLocation.GetX();
        outReply.y = theStormLocation.GetY();
        outReply.z = theStormLocation.GetZ();
        return true;
    }
    case EeCalamityOp::Strike: {
        EEUnit* theStorm = NULL;
        theCalamity->GetTargetUnit(theStorm);
        EEComplexUnit* theStormUnit = theStorm->mComplexUnit;
        EEUnit* theUnit = ioHost.mUnits[inCall.index];
        EECalamityCaptureEvent(kCalamityOpStrike, theUnit->mUnitID);   // CALW (TEST ONLY)
        if (theClass->mGraphicEffect3->mKind == kEffect2DLightning)
            gEffectManager->CreateUnitEffect(theUnit, theClass->mGraphicEffect3, NULL, 0, 0, 0);
        else
            gEffectManager->CreateEffectBetweenUnits(theStormUnit, theUnit, theClass->mGraphicEffect3, NULL);
        return true;
    }
    default:
        return false;
    }
}

// New (port): EECCHurricane's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCHurricane::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCHurricane* theClass = static_cast<EECCHurricane*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, NULL, 0);
        return true;
    }
    default:
        return false;
    }
}
