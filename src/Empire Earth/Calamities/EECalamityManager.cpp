// C:\EEX\Empire Earth\Calamities\EECalamityManager.cpp (__FILE__ 0x4b9850). The calamity classes and the active
// calamities: 0xadf97..0xaebb7. Unit flags: the exe's module set.
// The unit closes with its template COMDATs: vector<EECalamity*>::push_back (0xaea04), USingleton's ??_G (0xaea30),
// DeleteVectorContents<EECalamity> (0xaea54), GetCalamityClass<EEGlobalCalamityClass> (0xaea85),
// <EELocationCalamityClass> (0xaeaec), DeleteVectorContents<EECalamityClass> (0xaeb53).
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
#include "lle/FSFileSpec.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCAreaEffect.h"
#include "ee/EECCAEExtraDamage.h"
#include "ee/EECCAEMindControl.h"
#include "ee/EECCCloaking.h"
#include "ee/EECCConscription.h"
#include "ee/EECCDropObject.h"
#include "ee/EECCEarthquake.h"
#include "ee/EECCFire.h"
#include "ee/EECCHurricane.h"
#include "ee/EECCLeprosy.h"
#include "ee/EECCLineOfSight.h"
#include "ee/EECCLogicBomb.h"
#include "ee/EECCMeteorStorm.h"
#include "ee/EECCMorale.h"
#include "ee/EECCPlague.h"
#include "ee/EECCSpecialAttack.h"
#include "ee/EECCSpecialLocation.h"
#include "ee/EECCTeleport.h"
#include "ee/EECCTimeWarp.h"
#include "ee/EECCVolcano.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_CASE)
// FUNCTION: EXE 0x4adf97
// Creates the calamity manager with no classes or active calamities
EECalamityManager::EECalamityManager()
{
}

// SYNTHETIC: EXE 0x4adfcd
// (the scalar deleting destructor, emitted with the vtable)

// SYNTHETIC: EXE 0x4adfe9
// (the implicit destructor: no vptr store of its own; emitted after its first user, the ??_G)

// FUNCTION: EXE 0x4ae00b
// Stops and deletes every active calamity (asserts that all were deleted)
void EECalamityManager::DestroyCalamities()
{
    unsigned long theNumActiveCalamities = this->mCalamities.size();
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        (*theIterator)->mClass->Stop(*theIterator);
    unsigned long theNumCalamitiesDeleted = DeleteVectorContents(this->mCalamities);
    ESVERIFY_AT(theNumCalamitiesDeleted == theNumActiveCalamities, 77);
}

// FUNCTION: EXE 0x4ae070
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
            EECalamityClass* theCalamity = NULL;
            if (theData.mID >= 0) {
                switch (theData.mClassEnum) {
                case 1:   // LOST-DEBUG: the jump table keeps cases 1/2 (entries -> the store), so their bodies compiled away; EE_LOST_CASE stands in
                    EE_LOST_CASE(theCalamity); break;

                case 2:
                    EE_LOST_CASE(theCalamity); break;

                case kCalamityEarthquake:
                    theCalamity = new EECCEarthquake;
                    ESVERIFY_AT(theCalamity, 128);
                    break;

                case kCalamityHurricane:
                    theCalamity = new EECCHurricane;
                    ESVERIFY_AT(theCalamity, 133);
                    break;

                case 5:
                    break;

                case 6:
                    break;

                case kCalamityPlague:
                    theCalamity = new EECCPlague;
                    ESVERIFY_AT(theCalamity, 144);
                    break;

                case kCalamityLeprosy:
                    theCalamity = new EECCLeprosy;
                    ESVERIFY_AT(theCalamity, 149);
                    break;

                case kCalamityVolcano:
                    theCalamity = new EECCVolcano;
                    ESVERIFY_AT(theCalamity, 154);
                    break;

                case kCalamityFire:
                    theCalamity = new EECCFire;
                    ESVERIFY_AT(theCalamity, 159);
                    break;

                case 11:
                    break;

                case 12:
                    break;

                case kCalamityMorale:
                    theCalamity = new EECCMorale;
                    ESVERIFY_AT(theCalamity, 170);
                    break;

                case kCalamityTimeWarp:
                    theCalamity = new EECCTimeWarp;
                    ESVERIFY_AT(theCalamity, 175);
                    break;

                case kCalamityTeleport:
                    theCalamity = new EECCTeleport;
                    ESVERIFY_AT(theCalamity, 180);
                    break;

                case kCalamityAreaEffect:
                    theCalamity = new EECCAreaEffect;
                    ESVERIFY_AT(theCalamity, 185);
                    break;

                case kCalamityAEExtraDamage:
                    theCalamity = new EECCAEExtraDamage;
                    ESVERIFY_AT(theCalamity, 190);
                    break;

                case kCalamitySpecialAttack:
                    theCalamity = new EECCSpecialAttack;
                    ESVERIFY_AT(theCalamity, 195);
                    break;

                case kCalamityAEMindControl:
                    theCalamity = new EECCAEMindControl;
                    ESVERIFY_AT(theCalamity, 200);
                    break;

                case kCalamityConscription:
                    theCalamity = new EECCConscription;
                    ESVERIFY_AT(theCalamity, 205);
                    break;

                case kCalamityLogicBomb:
                    theCalamity = new EECCLogicBomb;
                    ESVERIFY_AT(theCalamity, 210);
                    break;

                case kCalamityDropObject:
                    theCalamity = new EECCDropObject;
                    ESVERIFY_AT(theCalamity, 215);
                    break;

                case kCalamityCloaking:
                    theCalamity = new EECCCloaking;
                    ESVERIFY_AT(theCalamity, 220);
                    break;

                case kCalamityMeteorStorm:
                    theCalamity = new EECCMeteorStorm;
                    ESVERIFY_AT(theCalamity, 225);
                    break;

                case kCalamityLineOfSight:
                    theCalamity = new EECCLineOfSight;
                    ESVERIFY_AT(theCalamity, 230);
                    break;

                case kCalamitySpecialLocation:
                    theCalamity = new EECCSpecialLocation;
                    ESVERIFY_AT(theCalamity, 235);
                    break;
                }
                if (theCalamity != NULL)
                    theCalamity->Initialize(&theData);
            }
            this->mCalamityClasses[i] = theCalamity;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4ae526
// Starts a global calamity of the class for the caster unit and player; false if the class or caster does not exist
bool EECalamityManager::CreateGlobalCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                             unsigned long inPlayerNumber)
{
    bool theResult = false;
    EEGlobalCalamityClass* theClass = NULL;
    if (this->GetCalamityClass(inClassEnum, theClass)) {
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theCasterUnit != NULL) {
            EECalamity* theCalamity = theClass->CreateCalamity(theCasterUnit);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    return theResult;
}

// EEGlobalCalamityClass::CreateCalamity (inline, refused here; first emitted after its only caller):
// TEMPLATE: EXE 0x4ae578

// FUNCTION: EXE 0x4ae5ac
// Starts a location calamity at the centre of the tile for the caster (or none, id -1) and player; false if the class,
// tile or caster does not exist
bool EECalamityManager::CreateLocationCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                               long inX, long inY, unsigned long inPlayerNumber)
{
    bool theResult = false;
    GE3DPoint theLocation;
    EELocationCalamityClass* theClass = NULL;
    if (this->GetCalamityClass(inClassEnum, theClass)) {
        EEMapTile* theTile = gWorld.mMap->GetTile(inX, inY);
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theTile != NULL && (theCasterUnit != NULL || inCasterUnitID == -1)) {
            theLocation.SetX(inX + 0.5f);
            theLocation.SetY(inY + 0.5f);
            theLocation.SetZ(theTile->mHeight);
            EECalamity* theCalamity = theClass->CreateCalamity(theLocation, theCasterUnit);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4ae66b
// Starts a unit calamity on the target unit for the caster (or none, id -1) and player; false if the class, target or
// caster does not exist
bool EECalamityManager::CreateUnitCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                           unsigned long inTargetUnitID, unsigned long inPlayerNumber)
{
    EEUnitCalamityClass* theClass = NULL;
    bool theResult = false;
    if (this->GetCalamityClass(inClassEnum, theClass)) {
        EEUnit* theTargetUnit = gWorld.GetUnit(inTargetUnitID);
        EEUnit* theCasterUnit = gWorld.GetUnit(inCasterUnitID);
        if (theTargetUnit != NULL && (theCasterUnit != NULL || inCasterUnitID == -1)) {
            EECalamity* theCalamity = theClass->CreateCalamity(theTargetUnit, theCasterUnit);
            theCalamity->mPlayerNumber = inPlayerNumber;
            this->AddCalamity(theCalamity);
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4ae6df
// Writes every active calamity to the out-of-sync dump
void EECalamityManager::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  ************************** CALAMITY MANAGER DUMP **************************\n\n");
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        (*theIterator)->DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x4ae711
// Returns the list of active calamities
void EECalamityManager::GetCalamities(std::vector<EECalamity*>*& outCalamities)
{
    outCalamities = &this->mCalamities;
}

// FUNCTION: EXE 0x4ae71d
// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(const EECalamityEnum& inClassEnum) const
{
    EECalamityClass* theClass = NULL;
    if (inClassEnum < this->mCalamityClasses.size())
        theClass = this->mCalamityClasses[inClassEnum];
    return theClass;
}

// FUNCTION: EXE 0x4ae749
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

// FUNCTION: EXE 0x4ae774
// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(unsigned long inClassEnum) const
{
    EECalamityClass* theClass = NULL;
    if (inClassEnum < this->mCalamityClasses.size())
        theClass = this->mCalamityClasses[inClassEnum];
    return theClass;
}

// FUNCTION: EXE 0x4ae79a
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

// FUNCTION: EXE 0x4ae7c5
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

// FUNCTION: EXE 0x4ae7eb
// Returns the sum of the active calamities' sync checksums
long EECalamityManager::GetOutOfSynchChecksum()
{
    long theChecksum = 0;
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        theChecksum += (*theIterator)->GetOutOfSynchChecksum();
    return theChecksum;
}

// FUNCTION: EXE 0x4ae80e
// Tells whether a calamity of the given kind is active
bool EECalamityManager::IsCalamityActive(long inClassEnum)
{
    for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin(); theIterator != this->mCalamities.end();
         ++theIterator)
        if ((*theIterator)->mClass->mClassEnum == inClassEnum)
            return true;
    return false;
}

// FUNCTION: EXE 0x4ae834
// Loads the media of the calamity class of the given kind for the player
void EECalamityManager::LoadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    if (!(inClassEnum < this->mCalamityClasses.size()))
        return;
    EECalamityClass* theClass = this->mCalamityClasses[inClassEnum];
    if (theClass != NULL)
        theClass->LoadMedia(inPlayer, false);
}

// FUNCTION: EXE 0x4ae869
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

// FUNCTION: EXE 0x4ae8bc
// Starts the calamity and keeps it active if it started; one that fails to start is deleted
void EECalamityManager::AddCalamity(EECalamity* inCalamity)
{
    if (inCalamity->mClass->Start(inCalamity))
        this->mCalamities.push_back(inCalamity);
    else
        delete inCalamity;  // releases its caster/target references and the media its ctor loaded
}

// FUNCTION: EXE 0x4ae8e1
// Stops and deletes the active calamity; returns the iterator to the next one
std::vector<EECalamity*>::iterator EECalamityManager::DestroyCalamity(std::vector<EECalamity*>::iterator inCalamity)
{
    (*inCalamity)->mClass->Stop(*inCalamity);
    delete *inCalamity;
    return this->mCalamities.erase(inCalamity);
}

// FUNCTION: EXE 0x4ae921
// Destroys the active calamities and deletes every calamity class (asserts that all were deleted)
void EECalamityManager::DestroyCalamityClasses(bool inShutdown)
{
    unsigned long theNumCalamityClasses = this->mCalamityClasses.size();
    this->DestroyCalamities();
    unsigned long theNumCalamityClassesDeleted = DeleteVectorContents(this->mCalamityClasses);
    ESVERIFY_AT(theNumCalamityClassesDeleted == theNumCalamityClasses, 690);
}

// FUNCTION: EXE 0x4ae966
// Unloads the media of the calamity class of the given kind for the player
void EECalamityManager::UnloadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    if (!(inClassEnum < this->mCalamityClasses.size()))
        return;
    EECalamityClass* theClass = this->mCalamityClasses[inClassEnum];
    if (theClass != NULL)
        theClass->UnloadMedia(inPlayer);
}

// FUNCTION: EXE 0x4ae999
// Clears every player's earthquake-shaken flag, updates each active calamity and destroys those that report 7 (done)
void EECalamityManager::Update(float inElapsedTime)
{
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++)
        gWorld.mPlayers[i]->mEarthquakeShaken = false;
    std::vector<EECalamity*>::iterator theIterator = this->mCalamities.begin();
    while (theIterator != this->mCalamities.end())
        if ((*theIterator)->mClass->Update(*theIterator, inElapsedTime) == 7)
            theIterator = this->DestroyCalamity(theIterator);
        else
            ++theIterator;
}

// FUNCTION: EXE 0x4aeb83
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

// Compiler-generated for this unit (template COMDATs, in the original's order):
// TEMPLATE: EXE 0x4aea04
// TEMPLATE: EXE 0x4aea30
// TEMPLATE: EXE 0x4aea54
// TEMPLATE: EXE 0x4aea85
// TEMPLATE: EXE 0x4aeaec
// TEMPLATE: EXE 0x4aeb53
