// C:\EEX\Empire Earth\Calamities\EEContagionManager.cpp (file name inferred: no __FILE__; the class name). The world's
// contagion helpers: 0xb662f..0xb6bd1 (the three save keys' init chains first, then EEContagionFunctor::CanInfect).
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

const UString EEContagionManager::mNumHelpersStringID("NumHelpers-StringID");
const UString EEContagionManager::mCalamityHelperOffsetStringID("CalamityHelperOffset-StringID");
const UString EEContagionManager::mHelperFunctorIDStringID("HelperFunctorID-StringID");

// FUNCTION: EXE 0x4b66f8
// Tells whether the functor can infect the unit's category and type
bool EEContagionFunctor::CanInfect(EEUnit* inUnit)
{
    bool theResult = false;
    long theCategory = inUnit->mUnitClass->GetCategory();
    long theUnitType = inUnit->mUnitClass->GetUnitType();
    if (this->CanInfectType(theCategory, theUnitType))
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x4b6723
// Creates the contagion manager with no helpers
EEContagionManager::EEContagionManager()
{
    this->mNextHelperID = 0;
}

// SYNTHETIC: EXE 0x4b675e
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b677a
// Destroys the contagion manager (members only; the helpers are not deleted)
EEContagionManager::~EEContagionManager()
{
}

// FUNCTION: EXE 0x4b6795
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

// FUNCTION: EXE 0x4b67f9
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

// FUNCTION: EXE 0x4b6851
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

// FUNCTION: EXE 0x4b68c3
// Returns the calamity at the index in the list, NULL if out of range
EECalamity* EEContagionManager::GetCalamity(std::vector<EECalamity*>& inCalamities, unsigned long inIndex)
{
    EECalamity* theCalamity = NULL;
    if (inIndex < inCalamities.size())
        theCalamity = inCalamities[inIndex];
    return theCalamity;
}

// FUNCTION: EXE 0x4b68e8
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

// FUNCTION: EXE 0x4b691d
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

// FUNCTION: EXE 0x4b6940
// Deletes the helpers and reads them from a saved game with their calamity indexes; returns the first read error
// (a helper's own load result is ignored)
long EEContagionManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theNumHelpers = 0;
    EEContagionHelper* theHelper = NULL;
    DeleteVectorContents(this->mHelpers);
    this->mCalamityIndexes.clear();
    long theResult = inFile.Read(theNumHelpers);
    if (theResult == kErrorOK)
        for (unsigned long i = 0; i < theNumHelpers && theResult == kErrorOK; i++) {
            unsigned long theCalamityIndex;
            theResult = inFile.Read(theCalamityIndex);
            if (theResult != kErrorOK)
                continue;
            theHelper = new EEContagionHelper;
            theHelper->Load(inMajorVersion, inMinorVersion, inFile);
            this->mHelpers.push_back(theHelper);
            this->mCalamityIndexes.push_back(theCalamityIndex);
        }
    return theResult;
}

// FUNCTION: EXE 0x4b69ef
// Reconnects each loaded helper to its calamity and functor and its units, deletes helpers without a calamity, and
// resumes ids after the highest
long EEContagionManager::RestorePointers(std::vector<EECalamity*>& inCalamities)
{
    std::vector<long> theDeadHelpers;
    this->mNextHelperID = 0;
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

// FUNCTION: EXE 0x4b6abe
// Writes each helper with its calamity's index to the saved game; returns 9 if a calamity is not active, else the
// first write error
long EEContagionManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theNumHelpers = this->mHelpers.size();
    unsigned long theCalamityIndex = 0;
    long theResult = outFile.Write(theNumHelpers);
    if (theResult == kErrorOK)
        for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin();
             theIterator != this->mHelpers.end() && theResult == kErrorOK; ++theIterator)
            if (EECalamityManager::GetInstance()->GetCalamityIndex((*theIterator)->GetCalamity(),
                                                                    (long&)theCalamityIndex)) {
                theResult = outFile.Write(theCalamityIndex);
                if (theResult == kErrorOK)
                    theResult = (*theIterator)->Save(inMajorVersion, inMinorVersion, outFile);
            } else
                theResult = 9;
    return theResult;
}

// FUNCTION: EXE 0x4b6b4f
// Replaces the old unit by the new one in every helper
void EEContagionManager::SwapUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    for (std::vector<EEContagionHelper*>::iterator theIterator = this->mHelpers.begin(); theIterator != this->mHelpers.end();
         ++theIterator)
        (*theIterator)->SwapUnit(inOldUnit, inNewUnit);
}

// Compiler-generated for this unit (template COMDATs, in the original's order):
// TEMPLATE: EXE 0x4b6b74
// TEMPLATE: EXE 0x4b6ba0

// SYNTHETIC: EXE 0x4b662f
// SYNTHETIC: EXE 0x4b6639
// SYNTHETIC: EXE 0x4b664a
// SYNTHETIC: EXE 0x4b6656
// SYNTHETIC: EXE 0x4b6672
// SYNTHETIC: EXE 0x4b667c
// SYNTHETIC: EXE 0x4b668d
// SYNTHETIC: EXE 0x4b6699
// SYNTHETIC: EXE 0x4b66b5
// SYNTHETIC: EXE 0x4b66bf
// SYNTHETIC: EXE 0x4b66d0
// SYNTHETIC: EXE 0x4b66dc
