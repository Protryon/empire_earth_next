// C:\EEX\Empire Earth\Calamities\EECalamityManager.cpp (__FILE__ 0x4b9850). The calamity classes and the active
// calamities: 0xadf97..0xaebb7. Unit flags: the exe's module set.
// The unit closes with its template COMDATs: vector<EECalamity*>::push_back (0xaea04), USingleton's ??_G (0xaea30),
// DeleteVectorContents<EECalamity> (0xaea54), GetCalamityClass<EEGlobalCalamityClass> (0xaea85),
// <EELocationCalamityClass> (0xaeaec), DeleteVectorContents<EECalamityClass> (0xaeb53). The three bases' units
// (EEGlobalCalamityClass.cpp, EELocationCalamityClass.cpp 0xb6f21, EEUnitCalamityClass.cpp 0xb70cc) are folded into
// EECalamityClass and the creations below (calamities lane, round 4).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityCapture.h"
// Creates the calamity manager with no classes or active calamities
EECalamityManager::EECalamityManager()
{
}

// Stops and deletes every active calamity (asserts that all were deleted)
void EECalamityManager::DestroyCalamities()
{
    unsigned long theNumActiveCalamities = this->mCalamities.size();
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        EECalamityCaptureStop((*theIterator)->mClass, *theIterator);   // CALT (TEST ONLY): Stop
    unsigned long theNumCalamitiesDeleted = DeleteVectorContents(this->mCalamities);
    ESVERIFY_AT(theNumCalamitiesDeleted == theNumActiveCalamities, 77);
}

namespace {

// New (port): a class for a record's kind when it has a base (ee_tech calamity_class.rs target_kind: none for kinds
// 1, 2, 5, 6, 11, 12 and unknown ones; LoadCalamityClasses's ESVERIFYs on `new`'s result, lines 128..235, could
// never fail)
EECalamityClass* NewCalamityClass(long inClassEnum)
{
    return ee_calamity_class_target_kind((int32_t)inClassEnum) != EE_CALAMITY_TARGET_NONE ? new EECalamityClass : NULL;
}

// the class of the enum if its base is inKind (the original's GetCalamityClass<T> dynamic_cast)
EECalamityClass* ClassOfKind(EECalamityManager* inManager, unsigned long inClassEnum, long inKind)
{
    EECalamityClass* theClass = inManager->GetCalamityClass(inClassEnum);
    return theClass != NULL && theClass->GetTargetKind() == inKind ? theClass : NULL;
}

// a new calamity of the class cast by the caster (referenced), at the location, Reset (the bases' CreateCalamity:
// 0xae578, 0xb6f21, 0xb70cc)
EECalamity* NewCalamity(EECalamityClass* inClass, EEUnit* inCasterUnit, const GE3DPoint* inLocation, EEUnit* inTargetUnit)
{
    EECalamity* theCalamity = new EECalamity(inClass, inCasterUnit);
    if (inTargetUnit != NULL) {
        theCalamity->SetTargetUnit(inTargetUnit);
        theCalamity->mTargetLocation = GE3DPoint((float)inTargetUnit->GetTileX(), (float)inTargetUnit->GetTileY(), 0.0f);
    } else if (inLocation != NULL)
        theCalamity->mTargetLocation = *inLocation;
    inClass->Reset(theCalamity);
    return theCalamity;
}

}   // namespace

// Reads the calamity class records from the file and creates and initializes the class each one names (NULL for
// unknown kinds); returns the record-count read result
long EECalamityManager::LoadCalamityClasses(FSFileSpec* inFile)
{
    unsigned long theNumRecords = 0;
    long theResult = 0;
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theNumRecords);
    if (theResult == 0) {
        this->mCalamityClasses.resize(theNumRecords, 0);
        for (unsigned long i = 0; i < theNumRecords; i++) {
            unsigned long theRead;
            EECalamityData theData;
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theData), sizeof(EECalamityData), theRead);
            // the class of the record's kind (none for kinds 1, 2, 5, 6, 11, 12, unknown ones or a negative id)
            EECalamityClass* theCalamity = theData.mID >= 0 ? NewCalamityClass(theData.mClassEnum) : NULL;
            if (theCalamity != NULL)
                theCalamity->Initialize(&theData);
            this->mCalamityClasses[i] = theCalamity;
        }
    }
    inFile->Close();
    return theResult;
}

// Starts a global calamity of the class for the caster unit and player; false if the class or caster does not exist
bool EECalamityManager::CreateGlobalCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                             unsigned long inPlayerNumber)
{
    bool theResult = false;
    EECalamityClass* theClass = ClassOfKind(this, inClassEnum, EE_CALAMITY_TARGET_GLOBAL);
    if (theClass != NULL) {
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theCasterUnit != NULL) {
            EECalamity* theCalamity = NewCalamity(theClass, theCasterUnit, NULL, NULL);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EECalamityCapture(0, inClassEnum, inCasterUnitID, 0, 0, 0, inPlayerNumber, theResult);
    return theResult;
}

// Starts a location calamity at the centre of the tile for the caster (or none, id -1) and player; false if the class,
// tile or caster does not exist
bool EECalamityManager::CreateLocationCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                               long inX, long inY, unsigned long inPlayerNumber)
{
    bool theResult = false;
    GE3DPoint theLocation;
    EECalamityClass* theClass = ClassOfKind(this, inClassEnum, EE_CALAMITY_TARGET_LOCATION);
    if (theClass != NULL) {
        EEMapTile* theTile = gWorld.mMap->GetTile(inX, inY);
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theTile != NULL && (theCasterUnit != NULL || UIsNone32(inCasterUnitID))) {
            theLocation.SetX(inX + 0.5f);
            theLocation.SetY(inY + 0.5f);
            theLocation.SetZ(theTile->mHeight);
            EECalamity* theCalamity = NewCalamity(theClass, theCasterUnit, &theLocation, NULL);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EECalamityCapture(1, inClassEnum, inCasterUnitID, 0, inX, inY, inPlayerNumber, theResult);
    return theResult;
}

// Starts a unit calamity on the target unit for the caster (or none, id -1) and player; false if the class, target or
// caster does not exist
bool EECalamityManager::CreateUnitCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                           unsigned long inTargetUnitID, unsigned long inPlayerNumber)
{
    EECalamityClass* theClass = ClassOfKind(this, inClassEnum, EE_CALAMITY_TARGET_UNIT);
    bool theResult = false;
    if (theClass != NULL) {
        EEUnit* theTargetUnit = gWorld.GetUnit(inTargetUnitID);
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theTargetUnit != NULL && (theCasterUnit != NULL || UIsNone32(inCasterUnitID))) {
            EECalamity* theCalamity = NewCalamity(theClass, theCasterUnit, NULL, theTargetUnit);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    if (EESimCaptureRecord::Active())   // TEST ONLY (ee/EETechCapture.h)
        EECalamityCapture(2, inClassEnum, inCasterUnitID, inTargetUnitID, 0, 0, inPlayerNumber, theResult);
    return theResult;
}

// Writes every active calamity to the out-of-sync dump
void EECalamityManager::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  ************************** CALAMITY MANAGER DUMP **************************\n\n");
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        (*theIterator)->DumpOutOfSynch(inFile);
}

// Returns the list of active calamities
void EECalamityManager::GetCalamities(std::vector<EECalamity*>*& outCalamities)
{
    outCalamities = &this->mCalamities;
}

// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(const EECalamityEnum& inClassEnum) const
{
    EECalamityClass* theClass = NULL;
    if (inClassEnum < this->mCalamityClasses.size())
        theClass = this->mCalamityClasses[inClassEnum];
    return theClass;
}

// Finds the calamity's index among the active calamities; false if it is not active
bool EECalamityManager::GetCalamityIndex(EECalamity* inCalamity, long& outIndex)
{
    long theIndex = 0;
    bool theResult = false;
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator, theIndex++)
        if (*theIterator == inCalamity) {
            theResult = true;
            outIndex = theIndex;
            break;
        }
    return theResult;
}

// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(unsigned long inClassEnum) const
{
    EECalamityClass* theClass = NULL;
    if (inClassEnum < this->mCalamityClasses.size())
        theClass = this->mCalamityClasses[inClassEnum];
    return theClass;
}

// Finds the class's index among the calamity classes; false if it is not there
bool EECalamityManager::GetCalamityClassIndex(EECalamityClass* inClass, long& outIndex)
{
    long theIndex = 0;
    bool theResult = false;
    for (std::vector<EECalamityClass*>::iterator theIterator = this->mCalamityClasses.begin();
         theIterator != this->mCalamityClasses.end(); ++theIterator, theIndex++)
        if (*theIterator == inClass) {
            outIndex = theIndex;
            theResult = true;
            break;
        }
    return theResult;
}

// Returns the class's target kind (global, location or unit); false if the class does not exist
bool EECalamityManager::GetCalamityType(EECalamityEnum inClassEnum, long& outType)
{
    bool theResult = false;
    EECalamityClass* theClass = this->GetCalamityClass(inClassEnum);
    if (theClass != NULL) {
        outType = theClass->GetTargetKind();
        theResult = true;
    }
    return theResult;
}

// Returns the sum of the active calamities' sync checksums
long EECalamityManager::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        theChecksum += (*theIterator)->GetOutOfSynchChecksum();
    return theChecksum;
}

// Tells whether a calamity of the given kind is active
bool EECalamityManager::IsCalamityActive(long inClassEnum)
{
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        if ((*theIterator)->mClass->mClassEnum == inClassEnum)
            return true;
    return false;
}

// Loads the media of the calamity class of the given kind for the player
void EECalamityManager::LoadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    if (!(inClassEnum < this->mCalamityClasses.size()))
        return;
    EECalamityClass* theClass = this->mCalamityClasses[inClassEnum];
    if (theClass != NULL)
        theClass->LoadMedia(inPlayer, false);
}

// Replaces the active calamities with the loaded ones; saves before 0.30 drop kinds 7, 8 and 10 (not deleted)
void EECalamityManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion,
                                std::vector<EECalamity*>& inCalamities)
{
    this->DestroyCalamities();
    for (std::vector<EECalamity*>::iterator theIterator = inCalamities.begin(); theIterator != inCalamities.end();
         ++theIterator)
        if (*theIterator != 0) {
            long theClassEnum = (*theIterator)->mClass->mClassEnum;
            if ((theClassEnum != 7 && theClassEnum != 8 && theClassEnum != 10) || inMajorVersion != 0 ||
                inMinorVersion >= 30)
                this->mCalamities.push_back(*theIterator);
        }
}

// Starts the calamity and keeps it active if it started; one that fails to start is deleted
void EECalamityManager::AddCalamity(EECalamity* inCalamity)
{
    bool theStarted = EECalamityCaptureStart(inCalamity->mClass, inCalamity);   // CALT (TEST ONLY): Start
    if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
        EESimCaptureRecord theRecord(EECaptureTag("CALS"));
        theRecord.I32(inCalamity->mClass->mClassEnum);
        theRecord.U32(inCalamity->mPlayerNumber);
        theRecord.F32(inCalamity->mTimeAlive);
        theRecord.U32(theStarted ? 1 : 0);
        theRecord.Commit();
    }
    if (theStarted) {
        this->mCalamities.push_back(inCalamity);
        EECalamityCaptureSave(inCalamity, this->mCalamities.size() - 1);   // CALK (TEST ONLY): each calamity at its start
    } else
        delete inCalamity;  // releases its caster/target references and the media its ctor loaded
}

// Stops and deletes the active calamity; returns the iterator to the next one
std::vector<EECalamity*>::iterator EECalamityManager::DestroyCalamity(std::vector<EECalamity*>::iterator inCalamity)
{
    EECalamityCaptureStop((*inCalamity)->mClass, *inCalamity);   // CALT (TEST ONLY): Stop
    delete *inCalamity;
    return this->mCalamities.erase(inCalamity);
}

// Destroys the active calamities and deletes every calamity class (asserts that all were deleted)
void EECalamityManager::DestroyCalamityClasses(bool inShutdown)
{
    unsigned long theNumCalamityClasses = this->mCalamityClasses.size();
    this->DestroyCalamities();
    unsigned long theNumCalamityClassesDeleted = DeleteVectorContents(this->mCalamityClasses);
    ESVERIFY_AT(theNumCalamityClassesDeleted == theNumCalamityClasses, 690);
}

// Unloads the media of the calamity class of the given kind for the player
void EECalamityManager::UnloadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    if (!(inClassEnum < this->mCalamityClasses.size()))
        return;
    EECalamityClass* theClass = this->mCalamityClasses[inClassEnum];
    if (theClass != NULL)
        theClass->UnloadMedia(inPlayer);
}

// Clears every player's earthquake-shaken flag, updates each active calamity and destroys those that report 7 (done)
void EECalamityManager::Update(float inElapsedTime)
{
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++)
        gWorld.mPlayers[i]->mEarthquakeShaken = false;
    std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin();
    while (theIterator != this->mCalamities.end()) {
        float theBefore = (*theIterator)->mTimeAlive;
        long theUpdate = EECalamityCaptureUpdate((*theIterator)->mClass, *theIterator, inElapsedTime);   // CALW (TEST ONLY)
        if (EESimCaptureRecord::Active()) {   // TEST ONLY (ee/EETechCapture.h)
            EESimCaptureRecord theRecord(EECaptureTag("CALU"));
            theRecord.I32((*theIterator)->mClass->mClassEnum);
            theRecord.U32((*theIterator)->mPlayerNumber);
            theRecord.F32(theBefore);
            theRecord.F32(inElapsedTime);
            theRecord.F32((*theIterator)->mTimeAlive);
            theRecord.F32((*theIterator)->mClass->mLifeTime);
            theRecord.I32(theUpdate);
            theRecord.Commit();
        }
        EECalamityCaptureAged(*theIterator, (unsigned long)(theIterator - this->mCalamities.begin()), theBefore);   // CALK (TEST ONLY)
        if (theUpdate == 7)
            theIterator = this->DestroyCalamity(theIterator);
        else
            ++theIterator;
    }
}

// Creates a unit of the class at the transformation for the player (-1: the calamity's player); NULL if the class
// does not exist
// cdecl; the hurricane and volcano calamities call it (provisional name). It sits after the unit's template COMDATs
// in the original.
EEUnit* EECreateCalamityUnit(EECalamity* inCalamity, long inUnitClass, GETransformation& inTransformation,
                             long inPlayerNumber)
{
    EEUnit* theUnit = NULL;
    long thePlayerNumber = inCalamity->mPlayerNumber;
    if (inPlayerNumber != -1)
        thePlayerNumber = inPlayerNumber;
    EEUnitClass* theClass = gUnitClassTable[inUnitClass];
    if (theClass != NULL)
        theUnit = theClass->CreateUnitAt(inTransformation, thePlayerNumber, 0, NULL);
    return theUnit;
}
