// C:\EEX\Empire Earth\Calamities\EEContagionHelper.cpp (file name inferred: no __FILE__; the class name). One
// spreading calamity's units: 0xb5b7d..0xb662f (the four save keys' init chains first). Unit flags: the exe's module
// set. SetHelperID is ICF-folded (0x1c1c).
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionHelper.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
#include "rust/ee_tech.h"


// FUNCTION: EXE 0x4b5c89
// Creates an empty contagion helper with no calamity or functor (for loading)
EEContagionHelper::EEContagionHelper() : EeContagionHelperFields()
{
    this->mCalamity = NULL;
    this->mTimeSinceLastUpdate = 0.0f;
    this->mFunctor = NULL;
}

// SYNTHETIC: EXE 0x4b5ccd
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b5ce9
// Creates a contagion helper for the calamity that spreads with the functor, with no units and no id
EEContagionHelper::EEContagionHelper(EEContagionFunctor* inFunctor, EECalamity* inCalamity)
{
    this->mTimeSinceLastSpread = 0.0f;
    this->mHelperID = -1;
    this->mFunctor = inFunctor;
    this->mTimeSinceLastUpdate = 0.0f;
    this->mCalamity = inCalamity;
}

// FUNCTION: EXE 0x4b5d3b
// Destroys the helper, curing and releasing every unit it holds
EEContagionHelper::~EEContagionHelper()
{
    std::vector<EEUnit*>::iterator theIterator = this->mUnits.begin();
    while (theIterator != this->mUnits.end())
        theIterator = this->RemoveUnit(theIterator, true);
    this->mCalamity = NULL;
}

// FUNCTION: EXE 0x4b5d88
// Adds a live unit not yet held: references it, marks it with the class's calamity flags and infects it if asked;
// returns whether it was added
bool EEContagionHelper::AddUnit(EEUnit* inUnit, bool inNotify)
{
    bool theResult = false;
    std::vector<EEUnit*>::iterator theFound = std::find(this->mUnits.begin(), this->mUnits.end(), inUnit);
    if (theFound == this->mUnits.end() && inUnit->mHitPoints > 0) {
        inUnit->Reference();
        if (inUnit->mComplexUnit != NULL)
            inUnit->mComplexUnit->mCalamityFlags |= this->mCalamity->mClass->mClassFlags;
        this->mUnits.push_back(inUnit);
        if (inNotify)
            this->mFunctor->Infect(inUnit, this->mCalamity);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b5e26
// Writes the helper's id, spread timer, unit ids and calamity to the out-of-sync dump
void EEContagionHelper::DumpOutOfSynch(FILE* inFile) const
{
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Contangion Helper id is %ld\n", this->mHelperID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper time since last spread: %6.6f\n", this->mTimeSinceLastSpread);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper unit list size is %zu\n", this->mUnits.size());
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper Unit ID for index %ld is #%08lx\n", i, this->mUnits[i]->mUnitID);
    this->mCalamity->DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x4b5eb0
// Returns a sync checksum: unit ids, the calamity's checksum, the helper id and the spread timer's bit pattern (ee_tech
// contagion_save.rs helper_checksum)
long EEContagionHelper::GetOutOfSynchChecksum() const
{
    long theUnitIDs = 0;
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        theUnitIDs += this->mUnits[i]->mUnitID;
    return ee_contagion_helper_checksum(this, theUnitIDs, this->mCalamity->GetOutOfSynchChecksum());
}

// FUNCTION: EXE 0x4b5ef9
// Collects every unit in the square of the radius around the unit, unless the unit is inside a carrier
void EEContagionHelper::GetUnitsInArea(float inRadius, EEComplexUnit* inUnit, std::vector<EEComplexUnit*>& outUnits)
{
    GE3DPoint thePosition;
    EETileArea theArea;
    inUnit->GetPosition(thePosition);
    float theX = thePosition.GetX();
    float theY = thePosition.GetY();
    if (inUnit->mContainer != NULL)
        return;
    theArea.mLeft = UMFloor(theX - inRadius);
    theArea.mTop = UMFloor(theY - inRadius);
    theArea.mRight = UMFloor(theX + inRadius);
    theArea.mBottom = UMFloor(theY + inRadius);
    EEAnyUnit theFilter;
    EEGetUnitsInRectangle(theArea, outUnits, theFilter, true);
}

// New (port): a helper's or the manager's file and C++ lists for their Rust Load and Save (ee_tech
// contagion_save.rs EE_CONTAGION_IO_*)
long EEContagionIoCall(void* inContext, uint32_t inOp, void* ioValue)
{
    EEContagionIo& theIo = *static_cast<EEContagionIo*>(inContext);
    FSFile& theFile = *theIo.mFile;
    unsigned long* theULong = static_cast<unsigned long*>(ioValue);
    switch (inOp) {
    case EE_CONTAGION_IO_READ_ULONG: return theFile.Read(*theULong);
    case EE_CONTAGION_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
    case EE_CONTAGION_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
    case EE_CONTAGION_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
    case EE_CONTAGION_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
    case EE_CONTAGION_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
    case EE_CONTAGION_IO_PUSH_UNIT_ID: theIo.mHelper->mUnitIDs.push_back(*theULong); return kErrorOK;
    case EE_CONTAGION_IO_UNIT_COUNT: *theULong = theIo.mHelper->mUnits.size(); return kErrorOK;
    case EE_CONTAGION_IO_UNIT_ID: *theULong = theIo.mHelper->mUnits[*theULong]->mUnitID; return kErrorOK;
    case EE_CONTAGION_IO_CLEAR:
        DeleteVectorContents(theIo.mManager->mHelpers);
        theIo.mManager->mCalamityIndexes.clear();
        return kErrorOK;
    case EE_CONTAGION_IO_LOAD_HELPER: {
        EEContagionHelper* theHelper = new EEContagionHelper;
        theHelper->Load(theIo.mMajorVersion, theIo.mMinorVersion, theFile);
        theIo.mManager->mHelpers.push_back(theHelper);
        theIo.mManager->mCalamityIndexes.push_back(*theULong);
        return kErrorOK;
    }
    case EE_CONTAGION_IO_HELPER_COUNT: *theULong = theIo.mManager->mHelpers.size(); return kErrorOK;
    case EE_CONTAGION_IO_HELPER_CALAMITY: {
        long theIndex = 0;
        if (!EECalamityManager::GetInstance()->GetCalamityIndex(theIo.mManager->mHelpers[*theULong]->GetCalamity(), theIndex))
            return kErrorFailed;
        *theULong = theIndex;
        return kErrorOK;
    }
    case EE_CONTAGION_IO_SAVE_HELPER:
        return theIo.mManager->mHelpers[*theULong]->Save(theIo.mMajorVersion, theIo.mMinorVersion, theFile);
    }
    return kErrorFailed;
}

// FUNCTION: EXE 0x4b5feb
// Reads the helper from a saved game (unit ids, id, spread timer; the update timer from 0.231 on); returns the first
// read error (ee_tech contagion_save.rs helper_load)
long EEContagionHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEContagionIo theIo = {&inFile, this, NULL, inMajorVersion, inMinorVersion};
    EeContagionIoFfi theFfi = {&theIo, &EEContagionIoCall};
    return ee_contagion_helper_load(this, inMajorVersion, inMinorVersion, theFfi);
}

// FUNCTION: EXE 0x4b60f7
// Removes the unit if held (curing it if asked); returns whether it was held
bool EEContagionHelper::RemoveUnit(EEUnit* inUnit, bool inNotify)
{
    bool theResult = false;
    std::vector<EEUnit*>::iterator theIterator = std::find(this->mUnits.begin(), this->mUnits.end(), inUnit);
    if (theIterator != this->mUnits.end()) {
        this->RemoveUnit(theIterator, inNotify);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x4b616e
// Clears the unit's calamity flags, cures it if asked, releases it and erases it; returns the next position
std::vector<EEUnit*>::iterator EEContagionHelper::RemoveUnit(std::vector<EEUnit*>::iterator inUnit, bool inNotify)
{
    if ((*inUnit)->mComplexUnit != 0)
        (*inUnit)->mComplexUnit->mCalamityFlags &= ~mCalamity->mClass->mClassFlags;
    if (inNotify)
        this->mFunctor->Cure(*inUnit, this->mCalamity);
    (*inUnit)->Dereference();
    return this->mUnits.erase(inUnit);
}

// FUNCTION: EXE 0x4b61ca
// Turns the loaded unit ids back into referenced units with the calamity flags (an id that no longer resolves is
// dropped)
long EEContagionHelper::RestorePointers()
{
    EEUnit* theUnit = NULL;
    for (std::vector<unsigned long>::iterator theIterator = this->mUnitIDs.begin(); theIterator != this->mUnitIDs.end();
         ++theIterator) {
        theUnit = gWorld.GetUnit(*theIterator);
        // a unit that is gone cannot be referenced (it used to be dereferenced through NULL on load)
        if (theUnit == NULL)
            continue;
        if (theUnit->mComplexUnit != NULL)
            theUnit->mComplexUnit->mCalamityFlags |= this->mCalamity->mClass->mClassFlags;
        theUnit->Reference();
        this->mUnits.push_back(theUnit);
    }
    this->mUnitIDs.clear();
    return kErrorOK;
}

// FUNCTION: EXE 0x4b6233
// Writes the helper's unit ids, id and both timers to the saved game; returns the first write error (ee_tech
// contagion_save.rs helper_save)
long EEContagionHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEContagionIo theIo = {&outFile, this, NULL, inMajorVersion, inMinorVersion};
    EeContagionIoFfi theFfi = {&theIo, &EEContagionIoCall};
    return ee_contagion_helper_save(this, theFfi);
}

// FUNCTION: EXE 0x4b62c9
// Sets the calamity the helper spreads
void EEContagionHelper::SetCalamity(EECalamity* inCalamity)
{
    this->mCalamity = inCalamity;
}

// FUNCTION: EXE 0x4b62d3
// Sets the functor that infects, damages and cures the units
void EEContagionHelper::SetFunctor(EEContagionFunctor* inFunctor)
{
    this->mFunctor = inFunctor;
}

// ICF-FOLDED: EXE 0x401c1c
void EEContagionHelper::SetHelperID(long inHelperID)
{
    this->mHelperID = inHelperID;
}

// FUNCTION: EXE 0x4b64e8
// Replaces a held unit by its new unit without curing or infecting
void EEContagionHelper::SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    std::vector<EEUnit*>::iterator theFound = std::find(this->mUnits.begin(), this->mUnits.end(), inOldUnit);
    if (theFound != this->mUnits.end()) {
        this->RemoveUnit(inOldUnit, false);
        this->AddUnit(inNewUnit, false);
    }
}

// FUNCTION: EXE 0x4b6552
// Spreads (or lets the functor update its units) every spread time, updates the functor, damages each live unit and
// removes and cures the dead. The decisions are Rust (ee_tech contagion.rs: the timers, which near units a spread
// infects, which held units are damaged or dropped); the helper, its units and the functor's calls stay here
// (Spread, 0x4b62dd, is that Rust update's spread)
void EEContagionHelper::Update(float inElapsedTime)
{
    // the functor's getters (ee_tech contagion_functor.rs params)
    EeContagionParams theParams = ee_contagion_functor_params(this->mFunctor, this->mCalamity->mClass->mIsBlockedByImmunity);
    EEContagionHelperHost theHost;
    theHost.mHelper = this;
    EeContagionHostFfi theFfi;
    theFfi.ctx = &theHost;
    theFfi.call = &EEContagionHelperHost::Call;
    EeContagionState theState;
    theState.since_spread = this->mTimeSinceLastSpread;
    theState.since_update = this->mTimeSinceLastUpdate;
    ee_contagion_update(&theParams, &theState, inElapsedTime, &theFfi);
    this->mTimeSinceLastSpread = theState.since_spread;
    this->mTimeSinceLastUpdate = theState.since_update;
}

// New (port): the helper's answers to its Rust update (ee_tech contagion.rs EeContagionOp)
void EEContagionHelperHost::Call(void* inContext, const EeContagionCall* inCall, EeContagionReply* outReply)
{
    EEContagionHelperHost& theHost = *static_cast<EEContagionHelperHost*>(inContext);
    EEContagionHelper* theHelper = theHost.mHelper;
    const EeContagionCall& theCall = *inCall;
    EeContagionReply& theReply = *outReply;
    theReply = EeContagionReply();
    switch (theCall.op) {
    case EeContagionOp::UpdateUnits:
        // (every functor's UpdateUnits was empty)
        EECalamityCaptureContagionEvent(kContagionUpdateUnits);   // CONT (TEST ONLY)
        break;
    case EeContagionOp::FunctorUpdate:
        EECalamityCaptureContagionEvent(kContagionFunctorUpdate);   // CONT (TEST ONLY)
        theHelper->mFunctor->Update(theCall.x, theHelper->mCalamity);
        break;
    case EeContagionOp::UnitCount:
        theReply.value = (int32_t)theHelper->mUnits.size();
        break;
    case EeContagionOp::UnitAlive:
        theReply.ok = theHelper->mUnits[theCall.index]->mHitPoints > 0;
        break;
    case EeContagionOp::Apply: {
        EEUnit* theUnit = theHelper->mUnits[theCall.index];
        EECalamityCaptureContagionEvent(kContagionApply, theUnit->mUnitID);   // CONT (TEST ONLY)
        theHelper->mFunctor->Apply(theUnit, theCall.x, theHelper->mCalamity);
        break;
    }
    case EeContagionOp::Remove:
        EECalamityCaptureContagionEvent(kContagionRemove, theHelper->mUnits[theCall.index]->mUnitID);   // CONT (TEST ONLY)
        theHelper->RemoveUnit(theHelper->mUnits.begin() + theCall.index, true);
        break;
    case EeContagionOp::HeldComplex:
        theReply.ok = theHelper->mUnits[theCall.index]->mComplexUnit != NULL;
        break;
    case EeContagionOp::NearUnits:
        theHost.mNear.clear();
        theHelper->GetUnitsInArea(theCall.x, theHelper->mUnits[theCall.index]->mComplexUnit, theHost.mNear);
        theReply.value = (int32_t)theHost.mNear.size();
        break;
    case EeContagionOp::NearView: {
        EEComplexUnit* theNearUnit = theHost.mNear[theCall.index];
        if (theNearUnit == NULL)
            break;
        EEComplexUnit* theUnit = theHelper->mUnits[theCall.index2]->mComplexUnit;
        theReply.id = theNearUnit->mUnitID;
        theReply.flags = EE_CONTAGION_NEAR_UNIT;
        if (theHelper->mFunctor->CanInfectType(theNearUnit->mUnitClass->GetCategory(), theNearUnit->mUnitClass->GetUnitType()))
            theReply.flags |= EE_CONTAGION_NEAR_TYPE;
        if (theUnit->mPlayer == theNearUnit->mPlayer)
            theReply.flags |= EE_CONTAGION_NEAR_SAME_PLAYER;
        if (theNearUnit->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] == 0)
            theReply.flags |= EE_CONTAGION_NEAR_NOT_ENEMY;
        break;
    }
    case EeContagionOp::Random:
        theReply.x = gPhysics->mRandom.Generate();
        break;
    case EeContagionOp::DistanceSquared: {
        EEComplexUnit* theUnit = theHelper->mUnits[theCall.index2]->mComplexUnit;
        theReply.x = theUnit->mPhysicalModel.GetLocation().DistanceSquaredTo(theHost.mNear[theCall.index]->mPhysicalModel.GetLocation());
        break;
    }
    case EeContagionOp::AddNew:
        theHost.mNew.push_back(theHost.mNear[theCall.index]);
        break;
    case EeContagionOp::NewView: {
        EEUnit* theNewUnit = theHost.mNew[theCall.index];
        EEComplexUnit* theComplexUnit = theNewUnit->mComplexUnit;
        theReply.id = theNewUnit->mUnitID;
        if (theComplexUnit != NULL)
            theReply.flags |= EE_CONTAGION_NEW_COMPLEX;
        if (std::find(theHelper->mUnits.begin(), theHelper->mUnits.end(), theComplexUnit) != theHelper->mUnits.end())
            theReply.flags |= EE_CONTAGION_NEW_HELD;
        break;
    }
    case EeContagionOp::Immune: {
        EEUnit* theNewUnit = theHost.mNew[theCall.index];
        EEComplexUnit* theComplexUnit = theNewUnit->mComplexUnit;
        EEGraphicEffect* theEffect = NULL;
        if (theComplexUnit->GetEffect(3, theEffect)) {
            EECalamityCaptureContagionEvent(kContagionNew, theNewUnit->mUnitID, 2);   // CONT (TEST ONLY)
            if (theEffect != NULL)
                gEffectManager->CreateUnitEffect(theComplexUnit, theEffect, NULL, 0, 0, 0);
            theReply.ok = true;
        }
        break;
    }
    case EeContagionOp::AddUnit: {
        EEUnit* theNewUnit = theHost.mNew[theCall.index];
        EECalamityCaptureContagionEvent(kContagionNew, theNewUnit->mUnitID, 1);   // CONT (TEST ONLY)
        theHelper->AddUnit(theNewUnit, true);
        break;
    }
    case EeContagionOp::Capture:
        EECalamityCaptureContagionEvent(theCall.value, theCall.index, theCall.index2, theCall.x, theCall.y);   // CONT (TEST ONLY)
        break;
    default:
        ESVERIFY_AT(false, 1);
        break;
    }
}

// SYNTHETIC: EXE 0x4b5c03
// SYNTHETIC: EXE 0x4b5c0d
// SYNTHETIC: EXE 0x4b5c1e
// SYNTHETIC: EXE 0x4b5c2a
// SYNTHETIC: EXE 0x4b5c46
// SYNTHETIC: EXE 0x4b5c50
// SYNTHETIC: EXE 0x4b5c61
// SYNTHETIC: EXE 0x4b5c6d
