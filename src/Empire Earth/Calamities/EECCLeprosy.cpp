// C:\EEX\Empire Earth\Calamities\EECCLeprosy.cpp (__FILE__ 0x4b9c14, EECCLeprosy::Stop's ESVERIFY). The leprosy
// calamity class: 0xb18e2..0xb1f4f. Unit flags: the exe's module set. Virtual2C is ICF-folded (0x157269); this unit
// holds the kept copies of GetSourceLocation (EECCAreaEffect's), Reset (EECCMorale's) and Virtual34 (EECCCloaking's).
// Data (.rdata): the vtable 0x43ca74 (COL 0x43ca70). (.data): type descriptor, __FILE__; the assert text is
// EECCAEExtraDamage.cpp's (string pooling).
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
#include "ee/EECCLeprosy.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEContagionFunctor.h"
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

// FUNCTION: EXE 0x4b1a52
// Returns the caster's position, or the target location without a caster
void EECCLeprosy::GetSourceLocation(EECalamity* inCalamity, GE3DPoint& outLocation)
{
    if (inCalamity->mCasterUnit != NULL) {
        const GETransformation& theTransformation = inCalamity->mCasterUnit->mComplexUnit->mPhysicalModel.GetLocation();
        outLocation.SetX(theTransformation.GetX());
        outLocation.SetY(theTransformation.GetY());
        outLocation.SetZ(theTransformation.GetZ());
    } else
        outLocation = inCalamity->mTargetLocation;
}

// FUNCTION: EXE 0x4b1a97
// Appends the map tiles within the leprosy radius around the target location
void EECCLeprosy::GetAffectedTiles(EECalamity* inCalamity, std::vector<EETileCoordinate>& outTiles)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;

    ::gWorld.GetContagionManager()->GetContagionHelper(inCalamity->mIntPlaceHolder1);
    inCalamity->GetTargetLocation(theLocation);
    long theX = UMFloor(theLocation.GetX());
    long theY = UMFloor(theLocation.GetY());
    EEGetTilesInRadius(theX, theY, this->mRadiusTiles, true, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        if (::gWorld.mMap->GetTile(it->mX, it->mY) != NULL)
            outTiles.push_back(*it);
    }
}

// New (port): EECCLeprosy's world steps for its Rust update (ee/EECalamityUpdate.h)
bool EECCLeprosy::UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCLeprosy* theClass = static_cast<EECCLeprosy*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    switch (inCall.op) {
    case EeCalamityOp::ApplyEffect: {
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        theClass->ApplyEffect(theCalamity);
        return true;
    }
    default:
        return false;
    }
}

// FUNCTION: EXE 0x4b1cca
// Sends the leprosy effect from the source to a random point on every affected tile
void EECCLeprosy::ShowEffects(EECalamity* inCalamity)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theSourceLocation;
    GE3DPoint theLocation;
    EEMap* theMap = ::gWorld.mMap;

    this->GetAffectedTiles(inCalamity, theTiles);
    this->GetSourceLocation(inCalamity, theSourceLocation);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        long theX = it->mX;
        theLocation.SetX(theX + (gPhysics->GetRandom() - 0.5f) + 0.5f);
        long theY = it->mY;
        theLocation.SetY(theY + (gPhysics->mRandom.Generate() - 0.5f) + 0.5f);
        theLocation.SetZ(theMap->GetTile(it->mX, it->mY)->mHeight);
        gEffectManager->CreateEffectBetween(theSourceLocation, theLocation, this->mGraphicEffect1, NULL);
    }
}

// FUNCTION: EXE 0x4b1dd3
// Adds every unit of the class's unit set within the radius to the contagion helper; an immune unit only shows its
// immunity effect when immunity blocks the calamity
void EECCLeprosy::ApplyEffect(EECalamity* inCalamity)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint theLocation;
    GE2DPoint the2DLocation;
    EEGraphicEffect* theEffect = NULL;

    theLocation = inCalamity->mTargetLocation;
    this->GetAffectedTiles(inCalamity, theTiles);
    the2DLocation = theLocation.Get2DPoint();
    EEGetUnitsInRadius(the2DLocation, (float)this->mRadiusTiles, theUnits);
    EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inCalamity->GetIntPlaceHolder1());
    for (long i = theUnits.size() - 1; i >= 0; i--) {
        EEUnit* theUnit = theUnits[i];
        if (!gUnitSetManager.Contains(theUnit->mUnitClass->GetUnitType(), this->GetUnitSet()))
            continue;
        if (this->mIsBlockedByImmunity && static_cast<EEComplexUnit*>(theUnit)->GetEffect(3, theEffect)) {
            if (theEffect != NULL)
                gEffectManager->CreateUnitEffect(theUnit, theEffect, NULL, 0, 0, 0);
        } else
            theHelper->AddUnit(theUnit, true);
    }
}

// New (port): EECCLeprosy's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCLeprosy::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCLeprosy* theClass = static_cast<EECCLeprosy*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1:
        theClass->ShowEffects(theCalamity);
        return true;
    default:
        return false;
    }
}
