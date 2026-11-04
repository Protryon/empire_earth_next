// C:\EEX\Empire Earth\Calamities\EEContagionHelper.cpp (file name inferred: no __FILE__; the class name). One
// spreading calamity's units: 0xb5b7d..0xb662f (the four save keys' init chains first). Unit flags: the exe's module
// set. SetHelperID is ICF-folded (0x1c1c).
#include <windows.h>
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
#include "ee/EEAreaUtilities.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

const UString EEContagionHelper::mNumUnitsStringID("ContagionHelperNumUnits-StringID");
const UString EEContagionHelper::mUnitIDStringID("ContagionHelperUnitID-StringID");
const UString EEContagionHelper::mHelperIDStringID("ContagionHelperHelperID-StringID");
const UString EEContagionHelper::mLastSpreadTimeIDStringID("ContagionHelperLastSpreadTimeID-StringID");

// FUNCTION: EXE 0x4b5c89
// Creates an empty contagion helper with no calamity or functor (for loading)
EEContagionHelper::EEContagionHelper()
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
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Contangion Helper id is %d\n", this->mHelperID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper time since last spread: %6.6f\n", this->mTimeSinceLastSpread);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper unit list size is %d\n", this->mUnits.size());
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        fprintf(inFile, "OUT OF SYNCH DUMP:  Contangion Helper Unit ID for index %d is #%08x\n", i, this->mUnits[i]->mUnitID);
    this->mCalamity->DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x4b5eb0
// Returns a sync checksum: unit ids, the calamity's checksum, the helper id and the spread timer's bit pattern
long EEContagionHelper::GetOutOfSynchChecksum() const
{
    long theChecksum = 0;
    for (unsigned long i = 0; i < this->mUnits.size(); i++)
        theChecksum += this->mUnits[i]->mUnitID;
    theChecksum += this->mCalamity->GetOutOfSynchChecksum();
    float theTime = this->mTimeSinceLastSpread;
    return this->mHelperID + *reinterpret_cast<long*>(&theTime) + theChecksum;
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

// FUNCTION: EXE 0x4b5feb
// Reads the helper from a saved game (unit ids, id, spread timer; the update timer from 0.231 on); returns the first
// read error
long EEContagionHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theNumUnits = 0;
    unsigned long theUnitID = 0;
    long theResult;
    if (inMajorVersion == 0 && inMinorVersion < 0xe7) {
        theResult = inFile.Read(theNumUnits);
        if (theResult == kErrorOK) {
            for (unsigned long i = 0; i < theNumUnits && theResult == kErrorOK; i++) {
                theResult = inFile.Read(theUnitID);
                this->mUnitIDs.push_back(theUnitID);
            }
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mHelperID);
                if (theResult == kErrorOK)
                    theResult = inFile.Read(this->mTimeSinceLastSpread);
            }
        }
    } else {
        theResult = inFile.Read(theNumUnits);
        if (theResult == kErrorOK) {
            for (unsigned long i = 0; i < theNumUnits && theResult == kErrorOK; i++) {
                theResult = inFile.Read(theUnitID);
                this->mUnitIDs.push_back(theUnitID);
            }
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mHelperID);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mTimeSinceLastSpread);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mTimeSinceLastUpdate);
                }
            }
        }
    }
    return theResult;
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
// Writes the helper's unit ids, id and both timers to the saved game; returns the first write error
long EEContagionHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theNumUnits = this->mUnits.size();
    long theResult = outFile.Write(theNumUnits);
    if (theResult != kErrorOK)
        return theResult;
    for (std::vector<EEUnit*>::iterator theIterator = this->mUnits.begin();
         theIterator != this->mUnits.end() && theResult == kErrorOK; ++theIterator) {
        unsigned long theUnitID = (*theIterator)->mUnitID;
        theResult = outFile.Write(theUnitID);
    }
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mHelperID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTimeSinceLastSpread);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTimeSinceLastUpdate);
    return theResult;
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

// FUNCTION: EXE 0x4b62dd
// Spreads the contagion: each held unit may infect nearby units the functor accepts (allies only if it says so) by a
// chance scaled by the elapsed time; immune units only show their immunity effect
void EEContagionHelper::Spread(float inElapsedTime)
{
    std::vector<EEUnit*> theNewUnits;
    EEComplexUnit* theComplexUnit = NULL;
    EEGraphicEffect* theEffect = NULL;
    std::vector<EEUnit*>::iterator theFound;
    float theChance = this->mFunctor->GetChance() * (inElapsedTime * 0.001f);
    std::vector<EEComplexUnit*> theNearUnits;
    float theRadius = this->mFunctor->GetRadius();
    float theRadiusSquared = theRadius * theRadius;
    for (std::vector<EEUnit*>::iterator theIterator = this->mUnits.begin(); theIterator != this->mUnits.end(); ++theIterator) {
        EEComplexUnit* theUnit = (*theIterator)->mComplexUnit;
        if (theUnit == NULL)
            continue;
        this->GetUnitsInArea(theRadius, theUnit, theNearUnits);
        for (std::vector<EEComplexUnit*>::iterator theNear = theNearUnits.begin(); theNear != theNearUnits.end();
             ++theNear) {
            EEComplexUnit* theNearUnit = *theNear;
            if (theNearUnit == NULL)
                continue;
            long theCategory = theNearUnit->mUnitClass->GetCategory();
            long theUnitType = theNearUnit->mUnitClass->GetUnitType();
            if (this->mFunctor->CanInfectType(theCategory, theUnitType) &&
                (!this->mFunctor->InfectsAlliesOnly() || theUnit->mPlayer == theNearUnit->mPlayer ||
                 theNearUnit->mPlayer->mEnemies[theUnit->mPlayer->mPlayerID] == 0) &&
                gPhysics->mRandom.Generate() <= theChance &&
                theUnit->mPhysicalModel.GetLocation().DistanceSquaredTo(theNearUnit->mPhysicalModel.GetLocation()) <= theRadiusSquared)
                theNewUnits.push_back(theNearUnit);
        }
        theNearUnits.clear();
    }
    for (std::vector<EEUnit*>::iterator theNew = theNewUnits.begin(); theNew != theNewUnits.end(); ++theNew) {
        EEUnit* theNewUnit = *theNew;
        theComplexUnit = theNewUnit->mComplexUnit;
        theFound = std::find(this->mUnits.begin(), this->mUnits.end(), theComplexUnit);
        if (theComplexUnit == NULL || theFound != this->mUnits.end())
            continue;
        theEffect = NULL;
        if (this->mCalamity->mClass->mIsBlockedByImmunity && theComplexUnit->GetEffect(3, theEffect)) {
            if (theEffect != NULL)
                gEffectManager->CreateUnitEffect(theComplexUnit, theEffect, NULL, 0, 0, 0);
        } else
            this->AddUnit(theNewUnit, true);
    }
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
// removes and cures the dead
void EEContagionHelper::Update(float inElapsedTime)
{
    unsigned long theSpreadTime = this->mFunctor->GetSpreadTime();
    this->mTimeSinceLastSpread += inElapsedTime;
    bool theSpread = theSpreadTime == 0 || this->mTimeSinceLastSpread > theSpreadTime;
    if (theSpread) {
        this->mTimeSinceLastSpread -= theSpreadTime;
        if (!this->mFunctor->SkipsSpread())
            this->Spread(this->mTimeSinceLastUpdate);
        else
            this->mFunctor->UpdateUnits(&this->mUnits);
        this->mTimeSinceLastUpdate = 0.0f;
    } else
        this->mTimeSinceLastUpdate += inElapsedTime;
    this->mFunctor->Update(inElapsedTime, this->mCalamity);
    std::vector<EEUnit*>::iterator theIterator = this->mUnits.begin();
    while (theIterator != this->mUnits.end())
        if ((*theIterator)->mHitPoints > 0) {
            this->mFunctor->Apply(*theIterator, inElapsedTime, this->mCalamity);
            ++theIterator;
        } else
            theIterator = this->RemoveUnit(theIterator, true);
}

// SYNTHETIC: EXE 0x4b5b7d
// SYNTHETIC: EXE 0x4b5b87
// SYNTHETIC: EXE 0x4b5b98
// SYNTHETIC: EXE 0x4b5ba4
// SYNTHETIC: EXE 0x4b5bc0
// SYNTHETIC: EXE 0x4b5bca
// SYNTHETIC: EXE 0x4b5bdb
// SYNTHETIC: EXE 0x4b5be7
// SYNTHETIC: EXE 0x4b5c03
// SYNTHETIC: EXE 0x4b5c0d
// SYNTHETIC: EXE 0x4b5c1e
// SYNTHETIC: EXE 0x4b5c2a
// SYNTHETIC: EXE 0x4b5c46
// SYNTHETIC: EXE 0x4b5c50
// SYNTHETIC: EXE 0x4b5c61
// SYNTHETIC: EXE 0x4b5c6d
