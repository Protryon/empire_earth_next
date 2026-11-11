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

// FUNCTION: EXE 0x4b18e2
// Creates the leprosy calamity class with no effect, functor, damage or radius
EECCLeprosy::EECCLeprosy()
{
    this->mGraphicEffect1 = NULL;
    this->mFunctor = NULL;
    this->mDamage = 0;
    this->mEffect1Duration = 0;
    this->mRadius = 0;
    this->mClassFlags = 2;
}

// SYNTHETIC: EXE 0x4b190c
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b1928
// Destroys the calamity class, releasing its effect and deleting its leprosy functor
EECCLeprosy::~EECCLeprosy()
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    delete this->mFunctor;
    this->mFunctor = NULL;
}

// FUNCTION: EXE 0x4b1958
// Starts leprosy: registers a contagion helper, shows the effects over the area and plays the sound; returns false if
// the helper could not be created
bool EECCLeprosy::Start(EECalamity* inCalamity)
{
    bool theResult = false;
    long theCalamityHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity, NULL);
    if (::gWorld.GetContagionManager()->GetContagionHelper(theCalamityHelperID) != NULL) {
        inCalamity->mIntPlaceHolder1 = theCalamityHelperID;
        this->ShowEffects(inCalamity);
        inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(),
                                                                        0.0f, 1.0f, this->mSound->GetLooping(), NULL));
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b1a07
// Stops the leprosy's sounds and removes its contagion helper (asserts that it existed)
void EECCLeprosy::Stop(EECalamity* inCalamity)
{
    long theCalamityHelperID = inCalamity->mIntPlaceHolder1;
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID), 112);
}

// FUNCTION: EXE 0x4b1a4e
// Returns the leprosy functor that spreads the disease
EEContagionFunctor* EECCLeprosy::GetContagionFunctor()
{
    return this->mFunctor;
}

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
    EEGetTilesInRadius(theX, theY, this->mRadius, true, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        if (::gWorld.mMap->GetTile(it->mX, it->mY) != NULL)
            outTiles.push_back(*it);
    }
}

// FUNCTION: EXE 0x4b1b7c
// Reads the calamity data: sound, radius, damage, spread time and effect (assumed present), and builds the leprosy
// functor
void EECCLeprosy::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mRadius = inData->mIntParam1;
    this->mDamage = inData->mIntParam2;
    long theSpreadTime = inData->mIntParam3;
    this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
    this->mGraphicEffect1->Reference();
    this->mEffect1Duration = this->mGraphicEffect1->mDuration;
    this->mFunctor = new EELeprosyFunctor(this->mDamage, theSpreadTime, this->GetUnitSet(), this->mIsBlockedByImmunity ? 2 : 3);
}

// FUNCTION: EXE 0x4b1c06
// Clears the calamity's applied flag (also the morale calamity's Reset, by ICF)
void EECCLeprosy::Reset(EECalamity* inCalamity)
{
    inCalamity->mBoolPlaceHolder1 = false;
}

// FUNCTION: EXE 0x4b1c14
// Loads the leprosy effect and sound (both assumed present)
void EECCLeprosy::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    this->mGraphicEffect1->Load(inIsOnDemand);
    this->mSound->LoadMedia();
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCLeprosy::PlaySound(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4b1c2f
// Unloads the leprosy effect and sound
void EECCLeprosy::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4b1c4e
// Ages the calamity with its contagion helper, infects the area once the effect has played and updates the sound
long EECCLeprosy::Update(EECalamity* inCalamity, float inElapsedTime)
{
    long theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, inCalamity->mIntPlaceHolder1);
    if (inCalamity->GetTimeAlive() >= this->mEffect1Duration && !inCalamity->mBoolPlaceHolder1) {
        EECalamityCaptureEvent(kCalamityOpApplyEffect);   // CALW (TEST ONLY)
        this->ApplyEffect(inCalamity);
        inCalamity->mBoolPlaceHolder1 = true;
    }
    this->UpdateSound(inCalamity, this->mSound, -1.0f, 4.0f, true);
    return theResult;
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
    EEGetUnitsInRadius(the2DLocation, (float)this->mRadius, theUnits);
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

// FUNCTION: EXE 0x4b1f32
// Accepts a tile whose surrounding area (half the radius, rounded up) is on the map
bool EECCLeprosy::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    return ::gWorld.mMap->IsAreaOnMap(inX, inY, (this->mRadius + 1) / 2);
}
