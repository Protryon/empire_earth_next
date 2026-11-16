// C:\EEX\Empire Earth\Calamities\EEContagionManager.cpp (file name inferred: no __FILE__; the class name). The world's
// contagion helpers: 0xb662f..0xb6bd1 (the three save keys' init chains first, then EEContagionFunctor::CanInfect);
// with the six contagion functors as one (ee/EEContagionFunctor.h; their units EE*Functor.cpp are gone).
// Unit flags: the exe's module set. The unit closes with its template COMDATs: vector<EEContagionHelper*>::push_back
// (0xb6b74) and DeleteVectorContents<vector<EEContagionHelper*> > (0xb6ba0).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEEffect.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
#include "rust/ee_tech.h"
#include "ee/EESaveGameFiles.h"


// New (port): the six functor classes' constructors (0xb6bd1, 0xb6e58, 0xb6fb1, 0xac991, 0xac7b9, 0xac891) as one:
// the numbers from the class's record (ee_tech contagion_functor.rs init), the effects each kind kept
EEContagionFunctor::EEContagionFunctor(EECalamityClass* inClass, uint32_t inKind)
{
    ee_contagion_functor_init(this, inClass, inKind);
    this->mEffect = NULL;
    this->mEffect2 = NULL;
    this->mEffect3 = NULL;
    if (inKind == EE_CALAMITY_FUNCTOR_FIRE) {
        this->mEffect = inClass->mGraphicEffect2;
        this->mEffect->Reference();
    } else if (inKind == EE_CALAMITY_FUNCTOR_MIND_CONTROL)
        this->mEffect = inClass->mGraphicEffect3;
    else if (inKind == EE_CALAMITY_FUNCTOR_AREA) {
        this->mEffect2 = inClass->mGraphicEffect2;
        this->mEffect3 = inClass->mGraphicEffect3;
    }
}

// Destroys the functor, releasing the fire's burning effect
EEContagionFunctor::~EEContagionFunctor()
{
    if (this->mKind == EE_CALAMITY_FUNCTOR_FIRE)
        this->mEffect->Dereference();
}

// Accepts a unit type in the functor's unit set (every functor's CanInfectType, by ICF)
bool EEContagionFunctor::CanInfectType(long inCategory, long inUnitType)
{
    return gUnitSetManager.Contains(inUnitType, this->mUnitSet);
}

// Tells whether the functor can infect the unit's category and type
bool EEContagionFunctor::CanInfect(EEUnit* inUnit)
{
    return this->CanInfectType(inUnit->mUnitClass->GetCategory(), inUnit->mUnitClass->GetUnitType());
}

// Counts the whole discrete update periods elapsed on the calamity for this update's damage
void EEContagionFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    inCalamity->mDiscreteUpdateTime = ee_contagion_functor_discrete_time(this);
    inCalamity->mTimeElapsedSinceLastUpdate += inElapsedTime;
    this->mUpdates = inCalamity->GetDiscreteUpdates();
}

void EEContagionFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    EECalamityCaptureContagionEvent(kContagionInfect, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    switch (this->mKind) {
    case EE_CALAMITY_FUNCTOR_FIRE: {
        // alight: the burning effect once, then burn percent of its maximum hit points as damage unless carried
        long theDamage = ee_contagion_fire_burn((int32_t)this->mBurn, (int32_t)inUnit->mStats->mMaxHitPoints);
        if (theComplexUnit != NULL)
            theComplexUnit->SetIsDeletable(false);
        if (theComplexUnit->mFireEffect == kNoEffectHandle) {
            EEEffectHandle theEffectHandle;
            gEffectManager->CreateUnitEffect(theComplexUnit, this->mEffect, &theEffectHandle, 0, 0, 0);
            theComplexUnit->mFireEffect = theEffectHandle;
        }
        if (theComplexUnit == NULL || theComplexUnit->GetOutermostCarrier() == theComplexUnit) {
            EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, theDamage);   // CONT (TEST ONLY)
            inUnit->TakeHit(inCalamity->mCasterUnit, theDamage, inCalamity->mPlayerNumber, this->mMode);
        }
        return;
    }
    case EE_CALAMITY_FUNCTOR_LEPROSY:
    case EE_CALAMITY_FUNCTOR_PLAGUE: {
        // the unit class's leprosy or plague effect; the plague keeps the unit from being deleted
        EEGraphicEffect* theEffect = this->mKind == EE_CALAMITY_FUNCTOR_LEPROSY ? inUnit->mUnitClass->mLeprosyEffect
                                                                                : inUnit->mUnitClass->mPlagueEffect;
        if (theEffect != NULL)
            gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, 0, 0, 0);
        if (this->mKind == EE_CALAMITY_FUNCTOR_PLAGUE && theComplexUnit != NULL)
            theComplexUnit->SetIsDeletable(false);
        return;
    }
    case EE_CALAMITY_FUNCTOR_AREA: {
        // the target effect at the calamity's target on the ground (at height 0 on water terrain types 1 and 10)
        GE3DPoint theLocation;
        inCalamity->GetTargetLocation(theLocation);
        long theTerrainType = ::gWorld.mMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()))->mTerrainType;
        if (theTerrainType != 1 && theTerrainType != 10)
            theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
        else
            theLocation.SetZ(0.0f);
        if (this->mEffect2 != NULL)
            gEffectManager->CreateEffect(inUnit, theLocation, this->mEffect2, NULL, 0);
        return;
    }
    case EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE: {
        // an enemy of the caster (any unit without a caster): its class's pulse cannon or battle cry effect, one
        // more negative morale
        EEUnit* theCaster = inCalamity->mCasterUnit;
        if (theCaster != NULL &&
            (inUnit->mPlayer == theCaster->mPlayer || theCaster->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0))
            return;
        EEGraphicEffect* theEffect = this->mFlag ? inUnit->mUnitClass->mPulseCannonEffect : inUnit->mUnitClass->mBattleCryEffect;
        if (theEffect != NULL)
            gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, false, 0, 0);
        theComplexUnit->mNegativeMorale++;
        return;
    }
    case EE_CALAMITY_FUNCTOR_MIND_CONTROL:
        // the effect on a caught unit, disabled (reason 0x1a) and halted
        if (this->mEffect != NULL)
            gEffectManager->CreateUnitEffect(inUnit, this->mEffect, NULL, false, 0, 0);
        theComplexUnit->SetDisabled(true, 0x1a);
        theComplexUnit->Halt();
        return;
    }
}

void EEContagionFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    if (this->mKind == EE_CALAMITY_FUNCTOR_LEPROSY || this->mKind == EE_CALAMITY_FUNCTOR_AREA)
        return;
    EECalamityCaptureContagionEvent(kContagionCure, inUnit->mUnitID);   // CONT (TEST ONLY)
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    switch (this->mKind) {
    case EE_CALAMITY_FUNCTOR_FIRE: {
        // the fire put out: the deletable flag restored, the burning effect finished or removed
        if (theUnit == NULL)
            return;
        theUnit->SetIsDeletable(inUnit->mUnitClass->mIsDeletable);
        EEEffectHandle theEffectHandle = theUnit->mFireEffect;
        if (theEffectHandle == kNoEffectHandle)
            return;
        EEEffect* theEffect = gEffectManager->GetEffect(theEffectHandle);
        if (theEffect != NULL) {
            if (theEffect->mKind == kEffectDamaged)
                theEffect->mRecord->damaged.finished = true;   // the damage controller (0x3e, on EEE2DRust)
            else
                gEffectManager->DestroyEffect(theEffectHandle);
        }
        theUnit->mFireEffect = kNoEffectHandle;
        return;
    }
    case EE_CALAMITY_FUNCTOR_PLAGUE:
        if (theUnit != NULL)
            theUnit->SetIsDeletable(inUnit->mUnitClass->mIsDeletable);
        return;
    case EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE:
        theUnit->mNegativeMorale--;
        return;
    case EE_CALAMITY_FUNCTOR_MIND_CONTROL:
        // re-enabled; a living one first loses its build targets, goals and orders and goes idle (handing the unit
        // back to its owner is not done here)
        if (theUnit == NULL)
            return;
        if (theUnit->mHitPoints > 0) {
            theUnit->ClearBuildTargets();
            for (unsigned long i = 0; i < theUnit->GetNumGoalQueues(); i++)
                theUnit->ClearGoalQueue(i);
            theUnit->ClearGoals(0);
            theUnit->SetDisabled(false, 0);
            theUnit->StartAction(kActionIdle);
            inUnit->mUnitClass->ActionChanged(theUnit);
        } else
            theUnit->SetDisabled(false, 0);
        return;
    }
}

// Damages the unit per elapsed period (ee_tech contagion_functor.rs apply_damage): the fire and the plague keep it
// from being deleted (the fire before, unless it is carried; the plague after), the area effect shows its hit effect
void EEContagionFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    if (this->mKind == EE_CALAMITY_FUNCTOR_FIRE && theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
    EeContagionFunctorHit theHit = ee_contagion_functor_apply_damage(this, (int32_t)inUnit->mHitPoints,
                                                                     this->mKind == EE_CALAMITY_FUNCTOR_AREA ? inUnit->mStats->GetAttack() : 0.0f);
    if (theHit.hits && (this->mKind != EE_CALAMITY_FUNCTOR_FIRE || theComplexUnit == NULL ||
                        theComplexUnit->GetOutermostCarrier() == theComplexUnit)) {
        if (this->mKind == EE_CALAMITY_FUNCTOR_AREA && this->mEffect3 != NULL)
            gEffectManager->CreateUnitEffect(inUnit, this->mEffect3, NULL, false, 0, 0);
        EECalamityCaptureContagionEvent(kContagionDamage, inUnit->mUnitID, theHit.damage);   // CONT (TEST ONLY)
        inUnit->TakeHit(inCalamity->mCasterUnit, theHit.damage, inCalamity->mPlayerNumber, this->mMode);
    }
    if (this->mKind == EE_CALAMITY_FUNCTOR_PLAGUE && theComplexUnit != NULL)
        theComplexUnit->SetIsDeletable(false);
}

// Returns whether a unit type is in the given unit set (false for an unused set)
// (declared out of line in ee/EEUnitSetManager.h; the original's COMDAT sat in EEAreaEffectFunctor.cpp, after its
// first user)
bool EEUnitSetManager::Contains(long inUnitType, unsigned long inSet)
{
    if (this->mSets[inSet] != NULL)
        return this->mSets[inSet]->Contains(inUnitType);
    return false;
}

// Creates the contagion manager with no helpers
EEContagionManager::EEContagionManager()
{
    this->mNextHelperID = 0;
}

// Destroys the contagion manager (members only; the helpers are not deleted)
EEContagionManager::~EEContagionManager()
{
}

// Creates a helper for the calamity with the next id, infecting the target unit if given; returns the id
long EEContagionManager::CreateContagionHelper(EEContagionFunctor* inFunctor, EECalamity* inCalamity,
                                               EEUnit* inTargetUnit)
{
    EEContagionHelper* theHelper = NULL;
    theHelper = new EEContagionHelper(inFunctor, inCalamity);
    this->mNextHelperID++;
    long theHelperID = this->mNextHelperID;
    theHelper->SetHelperID(theHelperID);
    if (inTargetUnit != NULL)
        theHelper->AddUnit(inTargetUnit, true);
    this->mHelpers.push_back(theHelper);
    return theHelperID;
}

// Deletes the helper with the id; does nothing while a loaded game awaits its pointers; always returns true
bool EEContagionManager::DestroyContagionHelper(long inHelperID)
{
    if (this->mCalamityIndexes.size() != 0)
        return true;
    for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin();
         theIterator != this->mHelpers.end(); ++theIterator) {
        EEContagionHelper* theHelper = *theIterator;
        if (theHelper->mHelperID != inHelperID)
            continue;
        delete theHelper;
        this->mHelpers.erase(theIterator);
        break;
    }
    return true;
}

// Writes every helper and the next helper id to the out-of-sync dump
void EEContagionManager::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Contangion Manager Dump\n");
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Contangion Manager Help list size %zu\n", this->mHelpers.size());
    for (unsigned long i = 0; i < this->mHelpers.size(); i++) {
        EEContagionHelper*& theHelper = this->mHelpers[i];
        theHelper->DumpOutOfSynch(inFile);
    }
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEContagionManager-Unique Number is %ld\n", this->mNextHelperID);
}

// Returns the calamity at the index in the list, NULL if out of range
EECalamity* EEContagionManager::GetCalamity(std::vector<EECalamity*>& inCalamities, unsigned long inIndex)
{
    EECalamity* theCalamity = NULL;
    if (inIndex < inCalamities.size())
        theCalamity = inCalamities[inIndex];
    return theCalamity;
}

// Returns the sum of the helpers' sync checksums plus the last helper id
long EEContagionManager::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    for (unsigned long i = 0; i < this->mHelpers.size(); i++) {
        EEContagionHelper*& theHelper = this->mHelpers[i];
        theChecksum += theHelper->GetOutOfSynchChecksum();
    }
    return this->mNextHelperID + theChecksum;
}

// Returns the helper with the id, NULL if none
EEContagionHelper* EEContagionManager::GetContagionHelper(long inHelperID)
{
    EEContagionHelper* theHelper = NULL;
    for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin(); theIterator != this->mHelpers.end();
         ++theIterator)
        if ((*theIterator)->mHelperID == inHelperID) {
            theHelper = *theIterator;
            break;
        }
    return theHelper;
}

// Deletes the helpers and reads them from a saved game with their calamity indexes; returns the first read error
// (a helper's own load result is ignored). Port (0.273, ee/EESaveGameFiles.h EESaveHasContagionCounter): the running
// game's last helper id, which RestorePointers goes on from; an older save goes on from its highest helper's id as
// the original did (ee_tech contagion_save.rs manager_load)
long EEContagionManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEContagionIo theIo = {&inFile, NULL, this, inMajorVersion, inMinorVersion};
    EeContagionIoFfi theFfi = {&theIo, &EEContagionIoCall};
    return ee_contagion_manager_load(&this->mNextHelperID, EESaveHasContagionCounter(inMajorVersion, inMinorVersion),
                                     theFfi);
}

// Reconnects each loaded helper to its calamity and functor and its units, deletes helpers without a calamity, and
// resumes ids after the highest
long EEContagionManager::RestorePointers(std::vector<EECalamity*>& inCalamities)
{
    std::vector<long> theDeadHelpers;
    // port (0.273): from the saved last helper id (Load: 0 for an older save, where the original started from 0)
    std::vector<unsigned long>::iterator theIndex = this->mCalamityIndexes.begin();
    for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin(); theIterator != this->mHelpers.end();
         ++theIterator, ++theIndex) {
        EECalamity* theCalamity = this->GetCalamity(inCalamities, *theIndex);
        if (theCalamity != NULL) {
            (*theIterator)->SetCalamity(theCalamity);
            (*theIterator)->SetFunctor(theCalamity->mClass->GetContagionFunctor());
            (*theIterator)->RestorePointers();
            long theHelperID = (*theIterator)->mHelperID;
            this->mNextHelperID = max(this->mNextHelperID, theHelperID);
        } else {
            long theHelperID = (*theIterator)->mHelperID;
            theDeadHelpers.push_back(theHelperID);
        }
    }
    this->mCalamityIndexes.clear();
    for (unsigned long i = 0; i < theDeadHelpers.size(); i++)
        this->DestroyContagionHelper(theDeadHelpers[i]);
    return kErrorOK;
}

// Writes each helper with its calamity's index to the saved game; returns 9 if a calamity is not active, else the
// first write error; the last helper id from 0.273 on (ee_tech contagion_save.rs manager_save)
long EEContagionManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEContagionIo theIo = {&outFile, NULL, this, inMajorVersion, inMinorVersion};
    EeContagionIoFfi theFfi = {&theIo, &EEContagionIoCall};
    return ee_contagion_manager_save(this->mNextHelperID, EESaveHasContagionCounter(inMajorVersion, inMinorVersion),
                                     theFfi);
}

// Replaces the old unit by the new one in every helper
void EEContagionManager::SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin(); theIterator != this->mHelpers.end();
         ++theIterator)
        (*theIterator)->SwapUnit(inOldUnit, inNewUnit);
}

