// C:\EEX\Empire Earth\Calamities\EECCLogicBomb.cpp (file name inferred: no __FILE__; the class name). The logic bomb
// calamity class: 0xb2034..0xb2634. Unit flags: the exe's module set. Reset and Virtual2C are ICF-folded (0x157269),
// Virtual28/Virtual34 into the area effects' (0xaf5f2/0xaf632).
// Data (.rdata, in order): kNoEffectHandle 0x43cb18, kCalamitySoundFalloff 0x43cb20, the vtable 0x43cb28 (COL
// 0x43cb24).
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
#include "lle/GEModel.h"
#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCLogicBomb.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

static void AdjustLocation(EECalamity* inCalamity, GE3DPoint* ioLocation);

// FUNCTION: EXE 0x4b21c2
// Raises the location by the target model's first-part LOD-0 bounding radius (provisional name)
static void AdjustLocation(EECalamity* inCalamity, GE3DPoint* ioLocation)
{
    EEUnit* theTargetUnit;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEModel* theModel = theTargetUnit->GetModel();
    if (theModel == NULL)
        return;
    GEModel* thePart = theModel->mModels[0];
    if (thePart == NULL)
        return;
    // raise the point by the radius (as the disable effect in Step uses it as a height), not shift it sideways
    ioLocation->SetZ(thePart->GetBoundingRadius(0) + ioLocation->GetZ());
}

// New (port): EECCLogicBomb's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCLogicBomb::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCLogicBomb* theClass = static_cast<EECCLogicBomb*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::LogicBombMark: {
        EECalamityCaptureEvent(kCalamityOpLogicBombMark);   // CALW (TEST ONLY)
        GE3DPoint theLocation;
        theCalamity->GetTargetLocation(theLocation);
        AdjustLocation(theCalamity, &theLocation);
        theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()) + 0.05f);
        gEffectManager->CreatePlacedEffect(theLocation, theClass->mGraphicEffect2, NULL);
        return true;
    }
    case EeCalamityOp::LogicBombDisable: {
        EECalamityCaptureEvent(kCalamityOpLogicBombDisable);   // CALW (TEST ONLY)
        EEEffectHandle theEffectHandle;
        EEUnit* theTargetUnit = NULL;
        if (theCalamity->GetTargetUnit(theTargetUnit)) {
            EEComplexUnit* theUnit = theTargetUnit->GetComplexUnit();
            if (theUnit->GetHitPoints() > 0) {
                if (theClass->mGraphicEffect3 != NULL) {
                    float theHeight = theUnit->GetModel()->mModels[0]->GetBoundingRadius(0);
                    GE3DPoint thePosition;
                    theUnit->GetPosition(thePosition);
                    thePosition.SetZ(thePosition.GetZ() + theHeight * 1.5f);
                    gEffectManager->CreatePlacedEffect(thePosition, theClass->mGraphicEffect3, &theEffectHandle);
                    theCalamity->SetEffectHandle(theEffectHandle);
                }
                theUnit->SetDisabled(1, 0x1c);
                EETechTreeNode* theNode = theUnit->GetTechTreeNode();
                if (theNode != NULL && theNode->mData->mKind == 4)
                    theUnit->GetPlayer()->RemoveWonder(theUnit);
                if (theTargetUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate)
                    theUnit->Halt();
                else {
                    theUnit->StartAction(kActionOpen);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                    EEUnitClass* theClass = theTargetUnit->GetUnitClass();
                    theUnit->SetOpenFraction(1.0f);
                    theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(1.0f);
                    theClass->GetLastLinked(theUnit)->SetOpenFraction(1.0f);
                }
            }
        }
        return true;
    }
    default:
        return false;
    }
}

// New (port): EECCLogicBomb's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCLogicBomb::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCLogicBomb* theClass = static_cast<EECCLogicBomb*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        AdjustLocation(theCalamity, &theLocation);
        outReply.x = theLocation.mX;
        outReply.y = theLocation.mY;
        outReply.z = theLocation.mZ;
        return true;
    }
    case 2: {
        // the target enabled again; its node's kind (-1 without one)
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        theTargetUnit->mComplexUnit->SetDisabled(0, 0);
        theTargetUnit->mUnitClass->ActionChanged(theTargetUnit->mComplexUnit);
        EETechTreeNode* theNode = theTargetUnit->GetTechTreeNode();
        outReply.value = theNode != NULL ? (int32_t)theNode->mData->mKind : -1;
        return true;
    }
    case 3: {
        EEUnit* theTargetUnit = NULL;
        theCalamity->GetTargetUnit(theTargetUnit);
        theTargetUnit->mPlayer->AddWonder(theTargetUnit->mComplexUnit, false);
        return true;
    }
    case 4: {
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        return true;
    }
    default:
        return false;
    }
}
