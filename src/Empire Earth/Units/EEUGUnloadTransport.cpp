// C:\EEX\Empire Earth\Units\EEUGUnloadTransport.cpp (file name inferred: no __FILE__; the key "EEUGUnloadTransport-Data").
// Unloading a transport: 0x2283bc..0x228c4c. Unit flags: the exe's module set. Data: the key 0x563610 (guard byte
// 0x563608); vtable 0x4479a0. The (bool) ctor is inline (COMDAT 0x23c685 in EEUnitGoal.cpp).

#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUCTransport.h"
#include "ee/EEUCAirplane.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGUnloadTransport.h"
long gSpaceUnloadSoundID = 498;             // 0x4c8fa8 (.data = 0x1f2; ee/EEUGUnloadTransport.h)

const UString EEUGUnloadTransport::sData("EEUGUnloadTransport-Data");
// SYNTHETIC: EXE 0x6283bc   (wrapper)
// SYNTHETIC: EXE 0x6283c6   (ctor)
// SYNTHETIC: EXE 0x6283d7   (registrar)
// SYNTHETIC: EXE 0x6283e3   (dtor)

// FUNCTION: EXE 0x6283ff
// Creates a goal for a transport to unload at a tile, referencing it; finds the unload tiles when it carries cargo
EEUGUnloadTransport::EEUGUnloadTransport(EEComplexUnit* inTransport, long inX, long inY, bool inArg)
    : EEUnitGoal(kGoalUnloadTransport, inArg), mTransport(inTransport), mDestination(inX, inY)
{
    this->mTransport->Reference();
    this->mUnloadRangeSquared = UMSquare(this->mTransport->mUnitClass->mUnloadRange);
    if (inTransport->GetNumCargoUnits() > 0)
        this->FindUnloadTiles();
}

// SYNTHETIC: EXE 0x628467
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x628483
// Destroys the goal, releasing the transport
EEUGUnloadTransport::~EEUGUnloadTransport()
{
    if (this->mTransport != NULL)
        this->mTransport->Dereference();
}

// FUNCTION: EXE 0x6284a0
// Returns the position of the tile where the transport stops to unload
void EEUGUnloadTransport::GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation)
{
    ::gWorld.mMap->GetTileLocation(this->mUnloadTile.mX, this->mUnloadTile.mY, outLocation);
}

// FUNCTION: EXE 0x6284b8
// Returns a copy of the goal for the same transport and unload tile
EEUnitGoal* EEUGUnloadTransport::Clone()
{
    return new EEUGUnloadTransport(this->mTransport, this->mDestination.GetX(), this->mDestination.GetY(), this->GetB1C());
}

// FUNCTION: EXE 0x6284ef
// Writes the transport id, its stopping tile, first cargo tile, unload tile and squared range to the out-of-synch
// dump, then the base goal's
void EEUGUnloadTransport::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport ID #%08x\n",
            this->mTransport ? this->mTransport->GetUnitID() : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Transport Destination (%d, %d)\n", this->mUnloadTile.mX,
            this->mUnloadTile.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGUnloadTransport - First Cargo Tile (%d, %d)\n", this->mFirstCargoTile.mX,
            this->mFirstCargoTile.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Tile (%d, %d)\n", this->mDestination.mX,
            this->mDestination.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  EEUGUnloadTransport - Unload Range Squared %6.6f\n", this->mUnloadRangeSquared);
    EEUnitGoal::DumpOutOfSynch(inFile);
}

// FUNCTION: EXE 0x628562
// Returns the base goal's checksum plus the transport id, the three tiles and the raw bits of the squared range
long EEUGUnloadTransport::GetOutOfSynchChecksum()
{
    long theID = this->mTransport ? this->mTransport->GetUnitID() : 0;
    long theUnloadX = this->mUnloadTile.mX;
    long theUnloadY = this->mUnloadTile.mY;
    long theFirstX = this->mFirstCargoTile.mX;
    long theFirstY = this->mFirstCargoTile.mY;
    long theDestinationX = this->mDestination.mX;
    long theDestinationY = this->mDestination.mY;
    float theRange = this->mUnloadRangeSquared;
    return EEUnitGoal::GetOutOfSynchChecksum() + theID + theDestinationY + theDestinationX + theFirstY + theFirstX
           + theUnloadY + theUnloadX + *reinterpret_cast<long*>(&theRange);
}

// FUNCTION: EXE 0x6285be
// Appends "unloadtransport", the transport id and the unload tile to the one-line debug text
void EEUGUnloadTransport::GetDebugLine(UWideString& ioText)
{
    UWideString theText;
    ioText += "unloadtransport";
    sprintf(theText, " (transport id %d, pointx %d pointy %d):", this->mTransport->GetUnitID() & 0xffffff, this->mDestination.mX,
            this->mDestination.mY);
    ioText += theText;
}

// FUNCTION: EXE 0x62861d
// Appends "UnLoad Transport", the transport's class name and the unload tile to the debug text
void EEUGUnloadTransport::GetDebugText(UWideString& ioText)
{
    UWideString theText;
    sprintf(theText, "UnLoad Transport\n Transport:%s  at(%d,%d):", (const char*)this->mTransport->mUnitClass->mName,
            this->mDestination.mX, this->mDestination.mY);
    ioText += theText;
}

// FUNCTION: EXE 0x62866b
// Returns the position of the unload tile
void EEUGUnloadTransport::GetGoalLocation(GE3DPoint& outLocation)
{
    ::gWorld.mMap->GetTileLocation(this->mDestination.mX, this->mDestination.mY, outLocation);
}

// FUNCTION: EXE 0x628683
// Returns whether the transport is within unload range of the unload tile (a teleporter always is)
bool EEUGUnloadTransport::IsInUnloadRange()
{
    bool theResult;
    if (this->mTransport != NULL && this->mTransport->mUnitClass->mUnitType == kUnitTypeTeleporter)
        theResult = true;
    else if (this->mTransport == NULL)
        // a transport not found on load: not in range (its tile used to be read through NULL)
        theResult = false;
    else {
        U2DPoint<long> theTile(this->mTransport->mTileX, this->mTransport->mTileY);
        theResult = theTile.DistanceToSquaredF(this->mDestination) <= this->mUnloadRangeSquared;
    }
    return theResult;
}

// FUNCTION: EXE 0x6286cf
// Returns whether the transport can unload at the unload tile (its transport class decides, else the tile must be
// free)
bool EEUGUnloadTransport::CanUnloadAtDestination()
{
    EEUCTransport* theTransportClass = dynamic_cast<EEUCTransport*>(this->mTransport->GetUnitClass());
    EEUCAirplane* theAirplaneClass = dynamic_cast<EEUCAirplane*>(this->mTransport->GetUnitClass());
    if (theTransportClass != NULL)
        return theTransportClass->CanUnloadAt(this->mTransport, this->mDestination.GetX(), this->mDestination.GetY());
    return EEIsTileFree(this->mTransport, this->mDestination.GetX(), this->mDestination.GetY());
}

// FUNCTION: EXE 0x628735
// Returns 9 path attempts
long EEUGUnloadTransport::GetMaxPathAttempts()
{
    return 9;
}

// FUNCTION: EXE 0x628739
// When the transport can unload there, has its class find the stopping tile, first cargo tile and final unload
// tile
void EEUGUnloadTransport::FindUnloadTiles()
{
    U2DPoint<long> theTile;
    if (!this->CanUnloadAtDestination())
        return;
    EEUCTransport* theTransportClass = dynamic_cast<EEUCTransport*>(this->mTransport->GetUnitClass());
    EEUCAirplane* theAirplaneClass = dynamic_cast<EEUCAirplane*>(this->mTransport->GetUnitClass());
    if (theTransportClass != NULL)
        theTransportClass->FindUnloadTiles(this->mTransport, &this->mDestination, &this->mUnloadTile, &this->mFirstCargoTile, &theTile);
    else
        theAirplaneClass->FindUnloadTiles(this->mTransport, &this->mDestination, &this->mUnloadTile, &this->mFirstCargoTile, &theTile);
    this->mDestination = theTile;
}

// FUNCTION: EXE 0x6287d6
// Unloads the cargo on the unload-transport action
void EEUGUnloadTransport::Step(EEComplexUnit* inUnit)
{
    long theType = inUnit->mCommand != NULL ? inUnit->mCommand->mType : 0;
    if (theType == kActionUnloadTransport)
        this->Unload();
}

// FUNCTION: EXE 0x6287fa
// Reads the base goal, transport id (resolved by RepairPointers), unload tile and (after version 0.218) the first
// cargo and stopping tiles; returns the first read error
long EEUGUnloadTransport::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theTransportID = -1;
    long theResult = EEUnitGoal::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(theTransportID);
    this->mTransport = reinterpret_cast<EEComplexUnit*>(theTransportID);
    if (theResult == 0) {
        long theValue;
        theResult = inFile.Read(theValue);
        this->mDestination.mX = theValue;
        if (theResult == 0) {
            theResult = inFile.Read(theValue);
            this->mDestination.mY = theValue;
        }
    }
    if (inMajorVersion <= 0 && inMinorVersion <= 0xda)
        return theResult;
    if (theResult != 0)
        return theResult;
    theResult = this->mFirstCargoTile.Load(inFile);
    if (theResult == 0)
        theResult = this->mUnloadTile.Load(inFile);
    return theResult;
}

// FUNCTION: EXE 0x628883
// Turns the loaded id back into the transport (NULL when gone), referencing it and resetting the unload range;
// saves up to 0.218 use the unload tile for all three tiles; returns 0
long EEUGUnloadTransport::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    RTSUnit* theUnit = ::gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTransport));
    if (theUnit != NULL) {
        this->mTransport = static_cast<EEUnit*>(theUnit)->mComplexUnit;
        this->mTransport->Reference();
        this->mUnloadRangeSquared = UMSquare(this->mTransport->mUnitClass->mUnloadRange);
        if (inMajorVersion == 0 && inMinorVersion <= 0xda) {
            this->mUnloadTile = this->mFirstCargoTile = this->mDestination;
        }
    } else
        this->mTransport = NULL;
    return 0;
}

// FUNCTION: EXE 0x6288ea
// Starts a transport with cargo and a set unload tile moving; a computer player's transport carrying citizens
// first finds its unload tiles again
void EEUGUnloadTransport::Start(EEComplexUnit* inUnit)
{
    if (this->mDestination.mX == -10000 || this->mDestination.mY == -10000 || this->mTransport != inUnit
        || !(inUnit->GetNumCargoUnits() > 0))
        return;
    if (!this->mTransport->mPlayer->IsHuman()) {
        long theCategory = this->mTransport->mCargo[0]->mUnitClass->mCategory;
        if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen) {
            this->FindUnloadTiles();
            this->mUnloadRangeSquared = UMSquare(this->mTransport->mUnitClass->mUnloadRange);
        }
    }
    inUnit->StartAction(kActionPrepareToMove);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// FUNCTION: EXE 0x628990
// Writes the base goal, transport id, unload tile, first cargo and stopping tiles; returns the first write error
long EEUGUnloadTransport::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEUnitGoal::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0) {
        // RepairPointers clears the transport when it no longer exists: save -1, which it reads back as no
        // transport, instead of reading the id through NULL
        unsigned long theTransportID = this->mTransport != NULL ? this->mTransport->GetUnitID() : (unsigned long)-1;
        theResult = outFile.Write(theTransportID);
        if (theResult == 0) {
            theResult = outFile.Write(this->mDestination.GetX());
            if (theResult == 0) {
                theResult = outFile.Write(this->mDestination.GetY());
                // the two tile writes are checked too (their results used to be ignored)
                if (theResult == 0)
                    theResult = this->mFirstCargoTile.Save(outFile);
                if (theResult == 0)
                    theResult = this->mUnloadTile.Save(outFile);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x628a19
// Switches the goal to another transport, moving the reference and taking its unload range
void EEUGUnloadTransport::SetTransport(EEComplexUnit* inTransport)
{
    this->mTransport->Dereference();
    this->mTransport = inTransport;
    this->mUnloadRangeSquared = UMSquare(this->mTransport->mUnitClass->mUnloadRange);
    this->mTransport->Reference();
}

// FUNCTION: EXE 0x628a44
// Answers the unload triggers (goal is an unload; in range with cargo, and for a non-teleporter transport a place
// to unload near the tile); others go to the common goal
bool EEUGUnloadTransport::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    EEUCTransport* theTransportClass = dynamic_cast<EEUCTransport*>(this->mTransport->GetUnitClass());
    EEUCAirplane* theAirplaneClass = dynamic_cast<EEUCAirplane*>(this->mTransport->GetUnitClass());
    switch (inEvent) {
    case kTriggerGoalIsUnloadTransport:
        return this->mKind == kGoalUnloadTransport ? true : false;
    case kTriggerContainerInRange: {
            bool theInRange = this->IsInUnloadRange() && this->mTransport->GetNumCargoUnits() > 0 ? true : false;
            if (theTransportClass != NULL && theTransportClass->mUnitType != kUnitTypeTeleporter) {
                if (theInRange && theTransportClass->CanUnloadNear(this->mTransport, this->mTransport->GetCargo(0), &this->mUnloadTile,
                                                              &this->mFirstCargoTile, &this->mDestination))
                    return true;
                return false;
            }
            return theInRange;
        }
    }
    return EEUnitGoal::EvaluateCondition(inUnit, inEvent);
}

// FUNCTION: EXE 0x628b08
// Follows a replaced transport of the same player (moving the reference); one that changed player is idled
void EEUGUnloadTransport::ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*)
{
    if (this->mTransport != inOld)
        return;
    if (inNew->mPlayer == this->mTransport->mPlayer) {
        this->mTransport->Dereference();
        this->mTransport = inNew;
        this->mUnloadRangeSquared = UMSquare(inNew->mUnitClass->mUnloadRange);
        inNew->Reference();
    } else if (this->mTransport->mHitPoints > 0)
        this->mTransport->StartAction(kActionIdle);
}

// FUNCTION: EXE 0x628b55
// Returns whether the goal has a transport and a unit is given
bool EEUGUnloadTransport::Suits(EEUnit* inUnit)
{
    return this->mTransport != NULL && inUnit != NULL;
}

// FUNCTION: EXE 0x628b68
// Drops the cargo (an aircraft fires it like a shot, others unload at the tile); plays the unload sound (the
// space sound for a teleporter) when any cargo left
void EEUGUnloadTransport::Unload()
{
    unsigned long theCount = this->mTransport->GetNumCargoUnits();
    EEUCTransport* theTransportClass = dynamic_cast<EEUCTransport*>(this->mTransport->GetUnitClass());
    float theVolume;
    float thePan;
    float thePitch;
    if (dynamic_cast<EEUCAirplane*>(this->mTransport->GetUnitClass()) != 0)
        this->mTransport->mUnitClass->FireShot(this->mTransport);
    else
        theTransportClass->UnloadAt(this->mTransport, this->mDestination.mX, this->mDestination.mY);
    if (theCount == this->mTransport->GetNumCargoUnits())
        return;
    EESoundObject* theSound = EESoundObject::Get(theTransportClass != NULL && theTransportClass->mUnitType == kUnitTypeTeleporter
                                                 ? gSpaceUnloadSoundID : 0x184);
    if (theSound != NULL) {
        this->mTransport->mUnitClass->GetSoundParameters(this->mTransport, theSound, &theVolume, &thePan, &thePitch);
        theSound->Play(theVolume, thePan, thePitch);
    }
}

// ICF-FOLDED: EXE 0x627264
// Returns prepare-to-move as the first action
long EEUGUnloadTransport::GetStartAction()
{
    return kActionPrepareToMove;
}

// ICF-FOLDED: EXE 0x557269
// Does nothing: the goal keeps its transport
void EEUGUnloadTransport::SetOwner(EEComplexUnit* inUnit)
{
}
