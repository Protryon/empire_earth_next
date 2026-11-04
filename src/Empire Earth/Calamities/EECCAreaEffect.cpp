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
#include "LostDebug.h"   // NOT original source: EE_LOST_DEBUG stand-ins (docs/LOST_DEBUG.md)

// FUNCTION: EXE 0x4af1ce
// Creates an area-effect calamity class with no effects, sound, functor or radius
EECCAreaEffect::EECCAreaEffect()
{
    this->mGraphicEffect2 = NULL;
    this->mGraphicEffect1 = NULL;
    this->mGraphicEffect3 = NULL;
    this->mFunctor = NULL;
    this->mDamageFactor = 0;
    this->mEffect1Duration = 0;
    this->mRadius = 0;
    this->mSound = NULL;
}

// SYNTHETIC: EXE 0x4af1fa
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4af216
// Destroys the calamity class, releasing its three effects and deleting its functor
EECCAreaEffect::~EECCAreaEffect()
{
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Dereference();
    delete this->mFunctor;
    this->mFunctor = NULL;
}

// FUNCTION: EXE 0x4af260
// Starts the calamity when it has a launch effect and a caster: plays it at the target (height from the
// target's tile) with the sound; returns whether it started
bool EECCAreaEffect::Start(EECalamity* inCalamity)
{
    EE_LOST_DEBUG(3);   // LOST-DEBUG: none: exact only (tools lostdebug_search)
    bool theResult = false;
    GE3DPoint theSourceLocation;
    GE3DPoint theLocation;

    if (this->mGraphicEffect1 != NULL && inCalamity->GetCasterUnit() != NULL) {
        this->GetSourceLocation(inCalamity, theSourceLocation);
        inCalamity->GetTargetLocation(theLocation);
        // the effect's height at the target comes from the target's tile (water: sea level), as Infect tests
        long theTerrainType = 0;
        if (::gWorld.mMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY())) != NULL)
            theTerrainType = ::gWorld.mMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()))->mTerrainType;
        if (theTerrainType != 1 && theTerrainType != 10)
            theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        else
            theLocation.SetZ(0.0f);
        EEEffectHandle theEffectHandle;
        gEffectManager->CreateEffect(inCalamity->GetCasterUnit(), theLocation, this->mGraphicEffect1, &theEffectHandle, 0);
        inCalamity->SetEffectHandle(theEffectHandle);
        if (this->mSound != NULL) {
            inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(),
                                                                            0.0f, 1.0f, this->mSound->GetLooping(), NULL));
            if (inCalamity->mSound != NULL)
                inCalamity->mSound->Dereference();
        }
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4af3a8
// Stops the calamity: destroys its effect, stops both sounds and removes its contagion helper
void EECCAreaEffect::Stop(EECalamity* inCalamity)
{
    long theCalamityHelperID = inCalamity->mIntPlaceHolder1;
    if (inCalamity->mEffectHandle != kNoEffectHandle) {
        gEffectManager->DestroyEffect(inCalamity->mEffectHandle);
        inCalamity->mEffectHandle = kNoEffectHandle;
    }
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
    ESVERIFY_AT(::gWorld.GetContagionManager()->DestroyContagionHelper(theCalamityHelperID), 150);
}

// ICF-FOLDED: EXE 0x52c734
// (ICF-folded into another class's identical accessor)
EEContagionFunctor* EECCAreaEffect::GetContagionFunctor()
{
    return this->mFunctor;
}

// ICF-FOLDED: EXE 0x557269
// (ICF-folded into a shared empty function)
void EECCAreaEffect::Reset(EECalamity* inCalamity)
{
}

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
    EEGetTilesInRadius(theX, theY, this->mRadius, true, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); it++) {
        if (::gWorld.mMap->GetTile(it->mX, it->mY) != NULL) {
            if (!this->mIsBlockedByImmunity || !gAEManager.HasAreaEffect(3, it->mX, it->mY, inCalamity->GetPlayerNumber()))
                outTiles.push_back(*it);
        }
    }
}

// FUNCTION: EXE 0x4af521
// Reads the calamity data: sound, radius, damage, update time and three effects (no launch effect:
// 600 delay), and builds the area-effect functor
void EECCAreaEffect::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    if (inData->mSoundID != 0)
        this->mSound = EESoundObject::Get(inData->mSoundID);
    this->mRadius = inData->mIntParam1;
    this->mDamageFactor = inData->mIntParam2;
    long theUpdateTime = inData->mIntParam3;
    if (inData->mGraphicEffectID1 != 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
        this->mEffect1Duration = this->mGraphicEffect1->mDuration;
    } else
        this->mEffect1Duration = 600;
    if (inData->mGraphicEffectID2 != 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
    }
    if (inData->mGraphicEffectID3 != 0) {
        this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
        this->mGraphicEffect3->Reference();
    }
    this->mFunctor = new EEAreaEffectFunctor(this->mGraphicEffect2, this->mGraphicEffect3, this->mDamageFactor, theUpdateTime, this->GetUnitSet(), this->mIsBlockedByImmunity ? 2 : 3);
}

// FUNCTION: EXE 0x4af5f2
// Loads the calamity's effects and sound
void EECCAreaEffect::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Load(inIsOnDemand);
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// FUNCTION: EXE 0x4af632
// Unloads the calamity's effects and sound
void EECCAreaEffect::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4af669
// Ages the calamity and updates its sound; once the launch effect is over, registers the contagion
// helper and applies the effect to the units in range, once. Returns the time-alive result
long EECCAreaEffect::Update(EECalamity* inCalamity, float inElapsedTime)
{
    long theResult;
    if (inCalamity->mBoolPlaceHolder1)
        theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, inCalamity->mIntPlaceHolder1);
    else
        theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
    if (this->mSound != NULL)
        this->UpdateSound(inCalamity, this->mSound, -1.0f, 15.0f, true);
    if (!(inCalamity->GetTimeAlive() >= this->mEffect1Duration) || inCalamity->mBoolPlaceHolder1)
        return theResult;
    long theCalamityHelperID = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity, NULL);
    ::gWorld.GetContagionManager()->GetContagionHelper(theCalamityHelperID);
    inCalamity->mIntPlaceHolder1 = theCalamityHelperID;
    this->ApplyEffect(inCalamity);
    inCalamity->mBoolPlaceHolder1 = true;
    return theResult;
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
    EEGetUnitsInRadius(the2DLocation, (float)this->mRadius, theUnits);
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

// FUNCTION: EXE 0x4af8a3
// Returns whether the calamity's area around the tile fits on the map
bool EECCAreaEffect::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    return ::gWorld.mMap->IsAreaOnMap(inX, inY, (this->mRadius + 1) / 2);
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
