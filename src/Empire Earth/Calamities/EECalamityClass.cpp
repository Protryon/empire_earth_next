// C:\EEX\Empire Earth\Calamities\EECalamityClass.cpp (file name inferred: no __FILE__; the class's members in vtable
// order). The base of every calamity class: 0xadcd9..0xadf97. Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c6bc (not referenced by the unit's code), the vtable 0x43c6c4 (COL 0x43c6c0).
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
#include "ee/EEAEManager.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4adcd9
// Creates a calamity class with no lifetime or class flags
EECalamityClass::EECalamityClass() : mLifeTime(0.0f), mClassFlags(0)
{
}

// FUNCTION: EXE 0x4adceb
// Returns the power cost of casting the calamity
long EECalamityClass::GetCost()
{
    return this->mCost;
}

// FUNCTION: EXE 0x4adcef
// Returns -1: no default target
long EECalamityClass::GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool)
{
    return -1;
}

// FUNCTION: EXE 0x4adcf5
// Returns the unit set the calamity affects
unsigned long EECalamityClass::GetUnitSet()
{
    return this->mUnitSet;
}

// SYNTHETIC: EXE 0x4adcf9
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4add15
// Destroys the calamity class
EECalamityClass::~EECalamityClass()
{
}

// FUNCTION: EXE 0x4add1c
// Reads the common calamity data: class, lifetime, unit set, cost, range, immunity and cast-by-all-selected flags,
// name and id
void EECalamityClass::Initialize(EECalamityData* inData)
{
    this->mClassEnum = inData->mClassEnum;
    this->mLifeTime = (float)inData->mLifeTime;
    this->mUnitSet = inData->mUnitSet;
    this->mCost = inData->mCost;
    this->mRange = UMFloatToLong(inData->mRange);
    this->mIsBlockedByImmunity = inData->mIsBlockedByImmunity;
    this->mIsCastByAllSelected = inData->mIsCastByAllSelected;
    this->mB24 = inData->mUnusedClassFlag;
    this->mNameID = inData->mNameID;
    this->mID = inData->mID;
}

// FUNCTION: EXE 0x4add8a
// Ages the calamity (and its contagion helper, if given); returns 7 once its lifetime is over, else 0
long EECalamityClass::UpdateTimeAlive(EECalamity* inCalamity, float inElapsedTime, long inHelperID)
{
    long theResult = 0;
    float theTimeAlive = inCalamity->GetTimeAlive() + inElapsedTime;
    if (inHelperID != -1) {
        EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inHelperID);
        if (theHelper != NULL)
            theHelper->Update(inElapsedTime);
    }
    inCalamity->mTimeAlive = theTimeAlive;
    if (theTimeAlive >= this->mLifeTime)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4adde9
// Updates the calamity's main or secondary sound for the target location with the given falloff; a volume other than
// -1 replaces the computed one (clamped to 0..1)
void EECalamityClass::UpdateSound(EECalamity* inCalamity, EESoundObject* inSound, float inVolume, float inFalloff,
                                  bool inPrimary)
{
    GE3DPoint theLocation;
    theLocation = inCalamity->mTargetLocation;
    SESMInstanceNode* theNode;
    if (inPrimary)
        theNode = inCalamity->mSound;
    else
        theNode = inCalamity->mSecondarySound;
    if (theNode == NULL)
        return;
    float theVolume;
    float thePan;
    float thePitch;
    EEComputeSoundParameters(inSound->GetVolume(), theLocation, theVolume, thePan, thePitch, inFalloff);
    if (inVolume != -1.0f)
        theVolume = (1.0f < inVolume) ? 1.0f : max(0.0f, inVolume);
    gSoundManager->SetSoundParameters(theNode, theVolume, thePan, thePitch);
}

// FUNCTION: EXE 0x4ade97
// Checks whether the calamity can be cast at the tile with no caster
long EECalamityClass::CanCast(EEUnit* inTargetUnit, unsigned long inX, unsigned long inY, bool inCheck)
{
    GE3DPoint theLocation;
    return this->CanCastAt(NULL, theLocation, inX, inY, inCheck, inTargetUnit);
}

// FUNCTION: EXE 0x4adec7
// Returns 0 if the caster (or nature) is alive with enough power and sees the tile, 2 if immunity blocks it there,
// else 1; always 1 without the check flag
long EECalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                bool inCheck, EEUnit* inTargetUnit)
{
    long thePlayerIndex = 0;
    long theResult = 1;
    if (inUnit != NULL)
        thePlayerIndex = ::gWorld.GetPlayerIndex(inUnit->mPlayer);
    if (inCheck) {
        if (thePlayerIndex == 0 || (inUnit != NULL && inUnit->mHitPoints > 0 && inUnit->GetPower() >= this->GetCost())) {
            EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
            if (theTile != NULL && (inUnit == NULL || theTile->IsVisibleTo(inUnit->mPlayer))) {
                theResult = this->mIsBlockedByImmunity && gAEManager.HasAreaEffect2(3, inX, inY, thePlayerIndex) ? 2 : 0;
            }
        }
    }
    return theResult;
}
