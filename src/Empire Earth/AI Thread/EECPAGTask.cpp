// C:\EEX\Empire Earth\AI Thread\EECPAGTask.cpp (file name inferred: no __FILE__; RTTI class EECPAGTask,
// vtable 0x43a6c0). An attack group task: 0x4162f..0x4192c (EECPAGOrder.cpp ends at 0x4162f, EECPASGAir's code starts
// at 0x4192c). The members in definition order; ??_G (the inline dtor) after the default ctor.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"

// FUNCTION: EXE 0x44162f
// Creates an inactive attack group task of no type or mode, with no target unit or player
EECPAGTask::EECPAGTask()
    : mUnit(NULL), mPlayer(NULL), mAggression(0.0f), mType(kAGTaskNone), mMode(kAGModeNone), mIsActive(false), mB31(false)
{
}

// SYNTHETIC: EXE 0x44165d
// (the scalar deleting destructor; the inline dtor expanded)

// FUNCTION: EXE 0x441686
// Creates a copy of a task, taking its own reference on the target unit
EECPAGTask::EECPAGTask(const EECPAGTask& inTask)
{
    this->mLocation = inTask.mLocation;
    this->mAggression = inTask.mAggression;
    this->mType = inTask.mType;
    this->mB31 = inTask.mB31;
    this->mMode = inTask.mMode;
    this->mIsActive = inTask.mIsActive;
    this->mPlayer = inTask.mPlayer;
    this->mUnit = inTask.mUnit;
    if (this->mUnit)
        this->mUnit->Reference();
}

// FUNCTION: EXE 0x4416e3
// Returns the target player's id, or -1 when the task has no player
long EECPAGTask::GetPlayerID()
{
    if (this->mPlayer)
        return this->mPlayer->GetPlayerID();
    return -1;
}

// FUNCTION: EXE 0x4416f5
// Reads the task from a saved game (unit and player as ids until Restore); returns the first read error
long EECPAGTask::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mLocation.Load(inMajorVersion, inMinorVersion, inFile);
    unsigned long theID = 0;
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mUnit = reinterpret_cast<EEComplexUnit*>(theID);
    }
    theID = 0;
    if (theResult == kErrorOK) {
        theResult = inFile.Read(theID);
        this->mPlayer = reinterpret_cast<EEPlayer*>(theID);
    }
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mAggression);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mMode);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mIsActive);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mB31);
    return theResult;
}

// FUNCTION: EXE 0x441791
// Turns the loaded player and unit ids back into pointers (-1: none) and references the unit; returns
// the location's restore result
long EECPAGTask::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = this->mLocation.Restore(inMajorVersion, inMinorVersion);
    if (reinterpret_cast<unsigned long>(this->mPlayer) != -1)   // -1: no player (older saves wrote 0)
        this->mPlayer = ::gWorld.mPlayers[reinterpret_cast<unsigned long>(this->mPlayer)];
    else
        this->mPlayer = NULL;
    if (reinterpret_cast<unsigned long>(this->mUnit) != -1) {
        this->mUnit = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mUnit)));
        if (this->mUnit)
            this->mUnit->Reference();
    } else {
        this->mUnit = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x4417f5
// Releases the reference on the target unit and clears it
void EECPAGTask::ReleaseUnit()
{
    if (this->mUnit)
        this->mUnit->Dereference();
    this->mUnit = NULL;
}

// FUNCTION: EXE 0x44180a
// Writes the task to the saved game (unit id or -1, player id or -1); returns the first write error
long EECPAGTask::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theID = -1;
    long theResult = this->mLocation.Save(inMajorVersion, inMinorVersion, outFile);
    theID = this->mUnit ? this->mUnit->GetUnitID() : -1;
    if (theResult == kErrorOK)
        theResult = outFile.Write(theID);
    theID = this->mPlayer ? this->mPlayer->GetPlayerID() : -1;   // -1: no player (was 0, gaia)
    if (theResult == kErrorOK)
        theResult = outFile.Write(theID);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAggression);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMode);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsActive);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mB31);
    return theResult;
}

// FUNCTION: EXE 0x4418c9
// Copies another task, moving the unit reference to its target unit
EECPAGTask& EECPAGTask::operator=(const EECPAGTask& inTask)
{
    this->mLocation = inTask.mLocation;
    this->mAggression = inTask.mAggression;
    this->mType = inTask.mType;
    this->mB31 = inTask.mB31;
    this->mMode = inTask.mMode;
    this->mIsActive = inTask.mIsActive;
    this->mPlayer = inTask.mPlayer;
    this->SetUnit(inTask.mUnit);
    return *this;
}

// FUNCTION: EXE 0x441912
// Sets the target unit: releases the old reference first (even for the same unit), then references the new
void EECPAGTask::SetUnit(EEComplexUnit* inUnit)
{
    this->ReleaseUnit();
    this->mUnit = inUnit;
    if (this->mUnit)
        this->mUnit->Reference();
}
