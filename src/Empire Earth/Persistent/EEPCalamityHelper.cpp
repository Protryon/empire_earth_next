// C:\EEX\Empire Earth\Persistent\EEPCalamityHelper.cpp (__FILE__ 0x4c14d4, Save's ESVERIFYs). The saved calamities
// and contagion manager: 0x1497f4..0x149c3c. Unit flags: the exe module set. Data: the vtable
// 0x44220c.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/USingleton.h"
#include "lle/GEColor.h"
#include "lle/GE3DPoint.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEPersistentTag.h"

// FUNCTION: EXE 0x5497f4
// Creates the saved-calamities chunk with no calamity list and a fresh contagion manager
EEPCalamityHelper::EEPCalamityHelper()
    : FSPersistent(kPersistentCalamities)
{
    this->mCalamities = NULL;
    this->mContagionManager = new EEContagionManager;
}

// SYNTHETIC: EXE 0x549826
// (the scalar deleting destructor)

// FUNCTION: EXE 0x549842
// Destroys the chunk, deleting its calamity list and contagion manager
EEPCalamityHelper::~EEPCalamityHelper()
{
    this->ClearCalamities();
    this->ClearContagionManager();
}

// FUNCTION: EXE 0x549860
// Deletes the calamity list
void EEPCalamityHelper::ClearCalamities()
{
    delete this->mCalamities;
    this->mCalamities = NULL;
}

// FUNCTION: EXE 0x549880
// Deletes the contagion manager
void EEPCalamityHelper::ClearContagionManager()
{
    delete this->mContagionManager;
    this->mContagionManager = NULL;
}

// FUNCTION: EXE 0x549897
// Empties the chunk: no calamity list and a fresh contagion manager
void EEPCalamityHelper::Reset()
{
    this->ClearCalamities();
    this->ClearContagionManager();
    this->mContagionManager = new EEContagionManager;
}

// FUNCTION: EXE 0x5498c2
// Copies another chunk's calamity list and contagion manager into fresh copies of its own
EEPCalamityHelper& EEPCalamityHelper::operator=(const EEPCalamityHelper& inOther)
{
    if (this->mCalamities != inOther.mCalamities) {
        this->ClearCalamities();
        this->mCalamities = new std::vector<EECalamity*>;
        if (inOther.mCalamities != NULL)
            *this->mCalamities = *inOther.mCalamities;
    }
    if (this->mContagionManager != inOther.mContagionManager) {
        this->ClearContagionManager();
        this->mContagionManager = new EEContagionManager;
        if (inOther.mContagionManager != NULL)
            *this->mContagionManager = *inOther.mContagionManager;
    }
    return *this;
}

// TEMPLATE: EXE 0x54994a
// (EEContagionManager's implicit operator=, emitted after its first user)

// FUNCTION: EXE 0x549977
// Creates an empty saved-calamities chunk (persistent factory)
FSPersistent* EEPCalamityHelper::CreateDefault() const
{
    return new EEPCalamityHelper;
}

// FUNCTION: EXE 0x54998e
// Reads the active calamities and then the contagion manager from a saved game; returns the contagion manager's error
long EEPCalamityHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECalamity* theCalamity = NULL;
    long theCount = 0;
    this->ClearCalamities();
    if (this->mCalamities == NULL)
        this->mCalamities = new std::vector<EECalamity*>;
    inFile.Read(theCount);
    for (long i = 0; i < theCount; i++) {
        theCalamity = new EECalamity;
        theCalamity->Load(inMajorVersion, inMinorVersion, inFile);
        this->mCalamities->push_back(theCalamity);
    }
    return this->mContagionManager->Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x549a33
// Reconnects the loaded calamities to their caster and target units and loads their media, deleting those whose units
// are gone; then reconnects the contagions
long EEPCalamityHelper::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    long theResult = 0;
    if (this->mCalamities != NULL) {
        for (std::vector<EECalamity*>::iterator theIterator = this->mCalamities->begin(); theIterator != this->mCalamities->end();
             theIterator++) {
            EECalamity* theCalamity = *theIterator;
            long theError = 0;
            if (theCalamity->mClass == NULL)
                theError = 3;
            unsigned long theCasterID = theCalamity->mOriginatingUnitID;
            EEUnit* theCaster = gWorld.GetUnit(theCasterID);
            theCalamity->SetCasterUnit(theCaster);
            if (theCaster == NULL) {
                if (UIsNone32(theCasterID))
                    theCalamity->mClass->LoadMedia(gWorld.mPlayers[0], false);
                else
                    theError = 9;
            } else
                theCalamity->mClass->LoadMedia(theCaster->mPlayer, false);
            if (theCalamity->mHasTargetUnit) {
                EEUnit* theTarget = gWorld.GetUnit(theCalamity->mTargetUnitID);
                theCalamity->SetTargetUnit(theTarget);
                if (theTarget == NULL)
                    theError = 3;
            }
            if (theError == 0)
                theCalamity->PlaySound();
            else {
                delete theCalamity;
                *theIterator = NULL;
            }
        }
        theResult = this->mContagionManager->RestorePointers(*this->mCalamities);
    }
    return theResult;
}

// FUNCTION: EXE 0x549b30
// Writes the calamity manager's active calamities (with their current target ids) and the world's contagions to a save
long EEPCalamityHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = 0;
    EEUnit* theTarget = NULL;
    long theCurrentClassOffset = 0, theCount = 0;
    std::vector<EECalamity*>* theTempActiveCalamities;
    USingleton<EECalamityManager>::GetInstance()->GetCalamities(theTempActiveCalamities);
    ESVERIFY_AT(theTempActiveCalamities, 334);
    if (theTempActiveCalamities != NULL) {
        theCount = theTempActiveCalamities->size();
        outFile.Write(theCount);
        std::vector<EECalamity*>::iterator theIterator = theTempActiveCalamities->begin();
        while (true) {
            if (theIterator == theTempActiveCalamities->end())
                break;
            EECalamity* theCalamity = *theIterator;

            EECalamityClass* theCurrentCalamityClass = theCalamity->mClass;
            if (!USingleton<EECalamityManager>::GetInstance()->GetCalamityClassIndex(theCurrentCalamityClass, theCurrentClassOffset))
                ESVerify(__FILE__, 349, "EECalamityManager::Instance()->GetCalamityClassOffsetToSave(theCurrentCalamityClass, theCurrentClassOffset)"); // the original assert text (provisional names here)
            theCalamity->mTargetUnitID = theCalamity->GetTargetUnit(theTarget) ? theTarget->GetUnitID() : -1;
            (*theIterator)->Save(inMajorVersion, inMinorVersion, outFile);
            theIterator++;
        }
        theResult = gWorld.GetContagionManager()->Save(inMajorVersion, inMinorVersion, outFile);
    }
    return theResult;
}
