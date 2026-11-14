// C:\EEX\Empire Earth\Calamities\EECCAreaEffect.cpp (__FILE__ 0x4b9ad8, EECCAreaEffect::Stop's ESVERIFY).
// The base of the area-effect calamity classes: 0xaf1ce..0xaf8c0. Unit flags: the exe's module set.
// GetContagionFunctor, Reset and GetSourceLocation are ICF-folded into other units' copies (0x12c734, 0x157269,
// 0xb1a52).
// Data (.rdata, in order): kNoEffectHandle 0x43c7ec, the vtable 0x43c7f8 (COL 0x43c7f4). (.data): type descriptor,
// __FILE__ 0x4b9ad8; the assert text is EECCAEExtraDamage.cpp's (string pooling).
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

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCAreaEffect.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4af411
// Lists the map tiles within the radius of the target, skipping tiles immune to it when immunity applies
void EECCAreaEffect::GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles)
{
    GE3DPoint theLocation;
    std::vector<EETileCoordinate> theTiles;

    ::gWorld.GetContagionManager()->GetContagionHelper(inCalamity->GetIntPlaceHolder1());
    inCalamity->GetTargetLocation(theLocation);
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    EEGetTilesInRadius(theX, theY, this->mRadiusTiles, true, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        if (::gWorld.mMap->GetTile(it->mX, it->mY) != NULL) {
            if (!this->mIsBlockedByImmunity || !gAEManager.HasAreaEffect(3, it->mX, it->mY, inCalamity->GetPlayerNumber()))
                outTiles.push_back(*it);
        }
    }
}

// New (port): EECCAreaEffect's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCAreaEffect::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCAreaEffect* theClass = static_cast<EECCAreaEffect*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::CreateHelper: {
        long theCalamityHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(theClass->mFunctor, theCalamity, NULL);
        ::gWorld.GetContagionManager()->GetContagionHelper(theCalamityHelperID);
        EECalamityCaptureEvent(kCalamityOpCreateHelper, theCalamityHelperID);   // CALW (TEST ONLY)
        outReply.value = (int32_t)theCalamityHelperID;
        return true;
    }
    case EeCalamityOp::ApplyEffect: {
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        theClass->ApplyEffect(theCalamity);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4af726
// Hands the units in range whose type is in the unit set to the contagion helper: enemies of the caster
// (everyone when the class affects own units), or gaia's enemies without a caster
void EECCAreaEffect::ApplyEffect(EECalamity* inCalamity)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;
    GE2DPoint the2DLocation;

    inCalamity->GetTargetLocation(theLocation);
    this->GetAffectedTiles(inCalamity, theTiles);
    the2DLocation = theLocation.Get2DPoint();
    EEGetUnitsInRadius(the2DLocation, (float)this->mRadiusTiles, theUnits);
    EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inCalamity->GetIntPlaceHolder1());
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        EEUnit* theUnit = theUnits[i];
        if (!gUnitSetManager.Contains(theUnit->GetUnitClass()->GetUnitType(), this->GetUnitSet()))
            continue;
        if (inCalamity->GetCasterUnit() != NULL) {
            EEPlayer* theCasterPlayer = inCalamity->mCasterUnit->mPlayer;
            if ((theUnit->GetPlayer() != theCasterPlayer && theCasterPlayer->mEnemies[theUnit->GetPlayer()->mPlayerID] != 0)
                || this->AffectsOwnUnits())
                theHelper->AddUnit(theUnit, true);
        } else {
            EEPlayer* theGaia = ::gWorld.mPlayers[0];
            if (theUnit->GetPlayer() != theGaia && theGaia->mEnemies[theUnit->GetPlayer()->mPlayerID] != 0)
                theHelper->AddUnit(theUnit, true);
        }
    }
}

// ICF-FOLDED: EXE 0x4ac9f3
// (`return false`: folded into an earlier unit's identical function)
bool EECCAreaEffect::AffectsOwnUnits()
{
    return false;
}

// ICF-FOLDED: EXE 0x4b1a52
// (ICF-folded into EECCLeprosy.cpp's identical copy)
void EECCAreaEffect::GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation)
{
    if (inCalamity->mCasterUnit != NULL) {
        const GETransformation& theTransformation = inCalamity->mCasterUnit->mComplexUnit->mPhysicalModel.GetLocation();
        outLocation.SetX(theTransformation.GetX());
        outLocation.SetY(theTransformation.GetY());
        outLocation.SetZ(theTransformation.GetZ());
    } else
        outLocation = inCalamity->mTargetLocation;
}

// New (port): EECCAreaEffect's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCAreaEffect::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCAreaEffect* theClass = static_cast<EECCAreaEffect*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        // the first effect from the caster to the location (the handle: kept by the area effect only)
        GE3DPoint theLocation(inCall.x, inCall.y, inCall.z);
        EEEffectHandle theEffectHandle;
        EECalamityCaptureStartPoint(1, theLocation.mX, theLocation.mY, theLocation.mZ);   // CALT (TEST ONLY)
        gEffectManager->CreateEffect(theCalamity->mCasterUnit, theLocation, theClass->mGraphicEffect1, &theEffectHandle, 0);
        outReply.id = theEffectHandle.mValue;
        return true;
    }
    default:
        return false;
    }
}
