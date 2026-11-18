// C:\EEX\Empire Earth\Calamities\EEContagionManager.cpp (file name inferred: no __FILE__; the class name). The world's
// contagion helpers: 0xb662f..0xb6bd1 (the three save keys' init chains first, then EEContagionFunctor::CanInfect);
// with the six contagion functors as one (ee/EEContagionFunctor.h; their units EE*Functor.cpp are gone).
// Unit flags: the exe's module set. The manager and its helpers are Rust's (ee/EEContagionManager.h); here are the
// functors and the world under the Rust (ee_contagion_world).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
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

// Reads the helpers and their calamity indexes from a saved game (the last helper id from 0.273 on: ee/
// EESaveGameFiles.h EESaveHasContagionCounter); the first read error (contagion_store.rs EeContagionManager::load)
long EEContagionManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_contagion_manager_load(this->mRust, (uint64_t)(uintptr_t)&inFile, inMajorVersion, inMinorVersion,
                                     EESaveHasContagionCounter(inMajorVersion, inMinorVersion));
}

// Writes each helper with its calamity's index; 9 if a calamity is not active, else the first write error
long EEContagionManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_contagion_manager_save(this->mRust, (uint64_t)(uintptr_t)&outFile,
                                     EESaveHasContagionCounter(inMajorVersion, inMinorVersion));
}

long EEContagionManager::RestorePointers(std::vector<EECalamity*>& inCalamities)
{
    std::vector<uint64_t> theCalamities;
    for (EECalamity* theCalamity : inCalamities)
        theCalamities.push_back((uint64_t)(uintptr_t)theCalamity);
    return ee_contagion_manager_restore(this->mRust, theCalamities.data(), theCalamities.size());
}

bool EEContagionHelperAddUnit(EEContagionHelper* inHelper, EEUnit* inUnit, bool inNotify)
{
    return ee_contagion_helper_add_unit(inHelper, (uint64_t)(uintptr_t)inUnit, inNotify);
}

void EEContagionHelperUpdate(EEContagionHelper* inHelper, float inElapsedTime)
{
    EECalamity* theCalamity = reinterpret_cast<EECalamity*>((uintptr_t)ee_contagion_helper_calamity(inHelper));
    ee_contagion_helper_update(inHelper, theCalamity->mClass->mIsBlockedByImmunity, inElapsedTime);
}

namespace {

template<class T> T* Object(uint64_t inHandle) { return reinterpret_cast<T*>((uintptr_t)inHandle); }

uint32_t FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

}   // namespace

// New (port): the world under the Rust contagion manager and helpers (contagion_store.rs EE_CONTAGION_WORLD_*): each
// case the original helper's or manager's statement on the units, functors and calamities by address
extern "C" int64_t ee_contagion_world(uint32_t inOp, uint64_t inA, uint64_t inB, uint64_t inC, float inX, void* ioValue)
{
    switch (inOp) {
    case EE_CONTAGION_WORLD_REFERENCE:
        if (inB != 0)
            Object<EEUnit>(inA)->Reference();
        else
            Object<EEUnit>(inA)->Dereference();
        return 0;
    case EE_CONTAGION_WORLD_MARK: {
        EEComplexUnit* theUnit = Object<EEUnit>(inA)->mComplexUnit;
        unsigned long theFlags = Object<EECalamity>(inB)->mClass->mClassFlags;
        if (theUnit != NULL)
            theUnit->mCalamityFlags = inC != 0 ? theUnit->mCalamityFlags | theFlags : theUnit->mCalamityFlags & ~theFlags;
        return 0;
    }
    case EE_CONTAGION_WORLD_INFECT: Object<EEContagionFunctor>(inA)->Infect(Object<EEUnit>(inB), Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_CURE: Object<EEContagionFunctor>(inA)->Cure(Object<EEUnit>(inB), Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_APPLY:
        Object<EEContagionFunctor>(inA)->Apply(Object<EEUnit>(inB), inX, Object<EECalamity>(inC));
        return 0;
    case EE_CONTAGION_WORLD_FUNCTOR_UPDATE: Object<EEContagionFunctor>(inA)->Update(inX, Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_NEAR: {
        // GetUnitsInArea: the square of the radius around the unit, unless it is inside a carrier
        EEComplexUnit* theUnit = Object<EEComplexUnit>(inA);
        GE3DPoint thePosition;
        theUnit->GetPosition(thePosition);
        if (theUnit->mContainer != NULL)
            return 0;
        EETileArea theArea;
        theArea.mLeft = UMFloor(thePosition.GetX() - inX);
        theArea.mTop = UMFloor(thePosition.GetY() - inX);
        theArea.mRight = UMFloor(thePosition.GetX() + inX);
        theArea.mBottom = UMFloor(thePosition.GetY() + inX);
        std::vector<EEComplexUnit*> theUnits;
        EEAnyUnit theFilter;
        EEGetUnitsInRectangle(theArea, theUnits, theFilter, true);
        for (EEComplexUnit* theNear : theUnits)
            ee_contagion_unit_list_push(static_cast<EeContagionUnitList*>(ioValue), (uint64_t)(uintptr_t)theNear);
        return 0;
    }
    case EE_CONTAGION_WORLD_CAN_INFECT: {
        EEUnit* theUnit = Object<EEUnit>(inB);
        return Object<EEContagionFunctor>(inA)->CanInfectType(theUnit->mUnitClass->GetCategory(), theUnit->mUnitClass->GetUnitType());
    }
    case EE_CONTAGION_WORLD_IMMUNE: {
        EEUnit* theUnit = Object<EEUnit>(inA);
        EEGraphicEffect* theEffect = NULL;
        if (!theUnit->mComplexUnit->GetEffect(3, theEffect))
            return 0;
        EECalamityCaptureContagionEvent(kContagionNew, theUnit->mUnitID, 2);   // CONT (TEST ONLY)
        if (theEffect != NULL)
            gEffectManager->CreateUnitEffect(theUnit->mComplexUnit, theEffect, NULL, 0, 0, 0);
        return 1;
    }
    case EE_CONTAGION_WORLD_RANDOM: return FloatBits(gPhysics->mRandom.Generate());
    case EE_CONTAGION_WORLD_DISTANCE:
        return FloatBits(Object<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().DistanceSquaredTo(
            Object<EEComplexUnit>(inB)->mPhysicalModel.GetLocation()));
    case EE_CONTAGION_WORLD_UNIT: return (int64_t)(uintptr_t)gWorld.GetUnit((unsigned long)inA);
    case EE_CONTAGION_WORLD_CALAMITY_CHECKSUM: return Object<EECalamity>(inA)->GetOutOfSynchChecksum();
    case EE_CONTAGION_WORLD_CALAMITY_INDEX: {
        long theIndex = 0;
        return EECalamityManager::GetInstance()->GetCalamityIndex(Object<EECalamity>(inA), theIndex) ? theIndex : -1;
    }
    case EE_CONTAGION_WORLD_CALAMITY_FUNCTOR:
        return (int64_t)(uintptr_t)Object<EECalamity>(inA)->mClass->GetContagionFunctor();
    case EE_CONTAGION_WORLD_CALAMITY_DUMP: Object<EECalamity>(inA)->DumpOutOfSynch(Object<FILE>(inB)); return 0;
    case EE_CONTAGION_WORLD_TEXT: fputs(static_cast<const char*>(ioValue), Object<FILE>(inB)); return 0;
    case EE_CONTAGION_WORLD_FILE: {
        FSFile& theFile = *Object<FSFile>(inB);
        switch (inA) {
        case EE_CONTAGION_IO_READ_ULONG: return theFile.Read(*static_cast<unsigned long*>(ioValue));
        case EE_CONTAGION_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
        case EE_CONTAGION_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
        case EE_CONTAGION_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
        case EE_CONTAGION_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
        case EE_CONTAGION_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
        }
        return kErrorFailed;
    }
    case EE_CONTAGION_WORLD_CAPTURE:   // CONT (TEST ONLY)
        EECalamityCaptureContagionEvent((int32_t)inA, (int32_t)inB, (int32_t)inC, inX, *static_cast<float*>(ioValue));
        return 0;
    }
    ESVERIFY_AT(false, 1);
    return 0;
}
