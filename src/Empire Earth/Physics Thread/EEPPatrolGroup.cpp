// C:\EEX\Empire Earth\Physics Thread\EEPPatrolGroup.cpp (name from RTTI EEPPatrolGroup; no __FILE__). 0x16066d..0x16100f:
// the patrol points a group of units walks. No key chains; ctor, ??_G, dtor, the members in address order, the
// ID counter reset (0x160cdc) and the point-matching functor. EEPMLand.cpp's last function ends at 0x16066d,
// RTSPhysics.cpp starts at 0x16100f. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/U2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EESaveGameFiles.h"

static unsigned long sPatrolGroupID = 0;    // 0x531d8c

// 0x160ff3 (find_if's functor 0x160fcc): the unit standing on a point's tile (GetTile() == mTile: U2DPoint's
// operator== loads both coordinates before the first branch; the nested inlines make find_if call it out of line)
struct EEPatrolTileIs {
    EEPatrolTileIs(U2DPoint<long> inTile) { this->mTile.mX = inTile.mX; this->mTile.mY = inTile.mY; }  // 0x160fba (by value: ret 8)
    bool operator()(EEComplexUnit* inUnit) const
    {
        if (inUnit->GetTile() == this->mTile)
            return true;
        return false;
    }
    U2DPoint<long> mTile;
};

// FUNCTION: EXE 0x56066d
// Creates an empty, open patrol group with the next patrol group id
EEPPatrolGroup::EEPPatrolGroup()
{
    this->mIsLoop = false;
    this->mID = sPatrolGroupID++;
    this->mIsTownPatrol = false;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x5606b0

// FUNCTION: EXE 0x5606cc
// Destroys the patrol group, detaching and killing every patrol point unit
EEPPatrolGroup::~EEPPatrolGroup()
{
    for (unsigned long i = 0; i < this->mPatrolPoints.size(); i++) {
        EEComplexUnit* theUnit = this->mPatrolPoints[i];
        theUnit->SetPatrolGroup(NULL);
        theUnit->GetPlayer()->KillUnit(theUnit, false);
    }
    this->mPatrolPoints.clear();
}

// FUNCTION: EXE 0x560727
// Adds a patrol point at the end at ground height; a point on the first point's tile closes the loop instead
void EEPPatrolGroup::AddPoint(const GE2DPoint& inPoint, unsigned long inPlayerID)
{
    GE3DPoint thePoint;
    EEMap* theMap = ::gWorld.mMap;
    EEMapTile* theTile = theMap->GetTile((long)inPoint.GetX(), (long)inPoint.GetY());
    if (this->mPatrolPoints.size() > 0 && theTile->HasUnit(this->mPatrolPoints[0])) {
        this->mIsLoop = true;
    } else {
        thePoint.SetX(inPoint.GetX());
        thePoint.SetY(inPoint.GetY());
        thePoint.SetZ(theTile->GetHeight());
        this->AppendPatrolPoint(thePoint, inPlayerID);
    }
}

// FUNCTION: EXE 0x5607c8
// Creates a patrol point at the given spot after the last one
void EEPPatrolGroup::AppendPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID)
{
    EEComplexUnit* theLast = NULL;
    if (this->mPatrolPoints.size() > 0)
        theLast = this->mPatrolPoints.back();
    this->InsertPatrolPoint(inPoint, inPlayerID, theLast);
}

// FUNCTION: EXE 0x5607f4
// Returns the patrol point nearest to the tile (the first of equals), or NULL when there are none
EEComplexUnit* EEPPatrolGroup::GetNearest(long inTileX, long inTileY)
{
    EEComplexUnit* theResult = NULL;
    if (this->mPatrolPoints.size() > 0) {
        theResult = this->mPatrolPoints[0];
        std::vector<EEComplexUnit*>::iterator theIterator = this->mPatrolPoints.begin() + 1;
        while (theIterator != this->mPatrolPoints.end()) {
            EEComplexUnit* theUnit = *theIterator;
            float theDistance = theUnit->DistanceSquaredToTileXY(inTileX, inTileY);
            if (theResult->DistanceSquaredToTileXY(inTileX, inTileY) > theDistance)
                theResult = theUnit;
            ++theIterator;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x56085d
// Returns the patrol point after the given one (the first for NULL); wraps to the first in a loop or a
// one-point patrol, else NULL at the end
EEComplexUnit* EEPPatrolGroup::GetNext(EEComplexUnit* inUnit)
{
    EEComplexUnit* theResult = NULL;
    if (inUnit == NULL) {
        if (this->mPatrolPoints.size() > 0)
            theResult = this->mPatrolPoints[0];
    } else if (this->mPatrolPoints.size() > 0) {
        unsigned long theIndex = this->IndexOf(inUnit);
        if (theIndex + 1 < this->mPatrolPoints.size())
            theResult = this->mPatrolPoints[theIndex + 1];
        else if (this->mIsLoop || this->mPatrolPoints.size() == 1)
            theResult = this->mPatrolPoints[0];
    }
    return theResult;
}

// FUNCTION: EXE 0x5608db
// Returns the index of the patrol point, or of the point nearest to the unit's tile when it is not one
unsigned long EEPPatrolGroup::IndexOf(EEComplexUnit* inUnit)
{
    EEComplexUnit* theNearest;
    std::vector<EEComplexUnit*>::iterator theIterator = std::find(this->mPatrolPoints.begin(), this->mPatrolPoints.end(), inUnit);
    if (theIterator == this->mPatrolPoints.end()) {
        theNearest = this->GetNearest(inUnit->GetTileX(), inUnit->GetTileY());
        if (theNearest != NULL)
            theIterator = std::find(this->mPatrolPoints.begin(), this->mPatrolPoints.end(), theNearest);
    }
    return theIterator - this->mPatrolPoints.begin();
}

// FUNCTION: EXE 0x56098a
// Returns the patrol point before the given one (the first for NULL); wraps to the last in a loop, else
// NULL at the start
EEComplexUnit* EEPPatrolGroup::GetPrevious(EEComplexUnit* inUnit)
{
    EEComplexUnit* theResult = NULL;
    if (inUnit == NULL) {
        if (this->mPatrolPoints.size() > 0)
            theResult = this->mPatrolPoints[0];
    } else if (this->mPatrolPoints.size() > 0) {
        unsigned long theIndex = this->IndexOf(inUnit);
        if (theIndex > 0)
            theResult = this->mPatrolPoints[theIndex - 1];
        else if (this->mIsLoop)
            theResult = this->mPatrolPoints.back();
    }
    return theResult;
}

// FUNCTION: EXE 0x5609e4
// Creates a patrol point at a map position (at ground height) after the given point
void EEPPatrolGroup::InsertPatrolPointAt(const GE2DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    thePoint.SetX(inPoint.GetX());
    thePoint.SetY(inPoint.GetY());
    EEMap* theMap = ::gWorld.mMap;
    EEMapTile* theTile = theMap->GetTile((long)thePoint.GetX(), (long)thePoint.GetY());
    thePoint.SetZ(theTile->GetHeight());
    this->InsertPatrolPoint(thePoint, inPlayerID, inUnit);
}

// FUNCTION: EXE 0x560a58
// Creates a path point unit for the player at the spot and inserts it after the given point (at the end
// for NULL)
void EEPPatrolGroup::InsertPatrolPoint(const GE3DPoint& inPoint, unsigned long inPlayerID, EEComplexUnit* inAfter)
{
    EEUnit* theCreated = gUnitClassTable[kUnitTypePathPoint]->CreateUnit((long)inPoint.GetX(), (long)inPoint.GetY(), inPlayerID, 0, NULL);
    EEComplexUnit* theUnit = theCreated->GetComplexUnit();
    theUnit->SetPatrolGroup(this);
    std::vector<EEComplexUnit*>::iterator theWhere;
    if (inAfter != NULL)
        theWhere = this->mPatrolPoints.begin() + this->IndexOf(inAfter) + 1;
    else
        theWhere = this->mPatrolPoints.end();
    this->mPatrolPoints.insert(theWhere, theUnit);
}

// FUNCTION: EXE 0x560ade
// Reads the group's id, its patrol point unit ids (as placeholders for RepairPointers) and the loop flag
// from the saved game; returns the first read error
long EEPPatrolGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    unsigned long theCount = 0;
    unsigned long theUnitID = (unsigned long)-1;
    long theResult = inFile->Read(this->mID);
    if (theResult == kErrorOK) {
        theResult = inFile->Read(theCount);
        if (theResult == kErrorOK) {
            this->mPatrolPoints.clear();
            for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
                theResult = inFile->Read(theUnitID);
                if (theResult == kErrorOK)
                    this->mPatrolPoints.push_back(reinterpret_cast<EEComplexUnit*>(theUnitID));
            }
            if (theResult == kErrorOK)
                theResult = inFile->Read(this->mIsLoop);
            // port (0.253): the town patrol flag (town patrols are saved from then on) and the id counter as saved
            if (theResult == kErrorOK && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
                theResult = inFile->Read(this->mIsTownPatrol);
                unsigned long theNextID = 0;
                if (theResult == kErrorOK)
                    theResult = inFile->Read(theNextID);
                if (theResult == kErrorOK && theNextID > sPatrolGroupID)
                    sPatrolGroupID = theNextID;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x560b72
// Removes a patrol point; the player drops the group once it has none left
void EEPPatrolGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    EEPlayer* thePlayer = inUnit->GetPlayer();
    std::vector<EEComplexUnit*>::iterator theIterator = std::find(this->mPatrolPoints.begin(), this->mPatrolPoints.end(), inUnit);
    this->mPatrolPoints.erase(theIterator);
    if (this->mPatrolPoints.size() == 0)
        thePlayer->RemovePatrolGroup(this);
}

// NEAR: the first v[i] of the function is expanded here, the original calls _Ptrit::operator+ out of line;
// frame 0x1c vs 0x24
// FUNCTION: EXE 0x560bf2
// Turns the loaded unit ids back into patrol point units, claiming those that point at another group;
// returns 3 at the first id with no unit
long EEPPatrolGroup::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<long> theIDs;
    EEComplexUnit* theUnit;
    long theResult = 0;
    for (unsigned long i = 0; i < this->mPatrolPoints.size(); i++) {
        theUnit = this->mPatrolPoints[i];
        theIDs.push_back((int32_t)USavedID(theUnit));
    }
    this->mPatrolPoints.clear();
    for (unsigned long j = 0; j < theIDs.size() && theResult == 0; j++) {
        long theID = theIDs[j];
        theUnit = dynamic_cast<EEComplexUnit*>(::gWorld.GetUnit(theID));
        if (theUnit != NULL) {
            // port: a point without a group is this group's too (path points are saved from 0.253 on)
            if (theUnit->GetPatrolGroup() != this)
                theUnit->SetPatrolGroup(this);
            this->mPatrolPoints.push_back(theUnit);
        } else {
            theResult = 3;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x560cdc
// Restarts patrol group ids at 0
void EEPPatrolGroup::ResetIDs()
{
    sPatrolGroupID = 0;
}

// FUNCTION: EXE 0x560ce4
// Writes the group's id, its patrol point unit ids and the loop flag to the saved game; returns the first
// write error
long EEPPatrolGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile* inFile)
{
    unsigned long theCount = 0;
    unsigned long theUnitID;
    long theResult = inFile->Write(this->mID);
    if (theResult != 0)
        return theResult;
    theCount = this->mPatrolPoints.size();
    theResult = inFile->Write(theCount);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        theUnitID = this->mPatrolPoints[i]->mUnitID;
        theResult = inFile->Write(theUnitID);
    }
    if (theResult == 0)
        theResult = inFile->Write(this->mIsLoop);
    // port (0.253): the town patrol flag and the id counter (Load)
    if (theResult == 0 && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        theResult = inFile->Write(this->mIsTownPatrol);
        if (theResult == 0)
            theResult = inFile->Write(sPatrolGroupID);
    }
    return theResult;
}

// FUNCTION: EXE 0x560d70
// Selects the patrol points for the player, stopping at the first that cannot be selected
void EEPPatrolGroup::Select(EEPlayer* inPlayer, bool inFlag)
{
    bool theOK = true;
    for (unsigned long i = 0; i < this->mPatrolPoints.size() && theOK; i++)
        theOK = inPlayer->TrySelectUnit(this->mPatrolPoints[i], inFlag);
}

// NEAR: the locals' order/slots and the three loops' iterator temporaries differ (the original keeps theTile and
// theOther at [ebp-0x24]/[ebp-0x1c], the iterators in registers); behaviour is the same by reading
// FUNCTION: EXE 0x560da9
// Replaces the patrol route with the given tiles: kills points on no listed tile, adds missing ones and
// orders the points as listed, as a closed loop
void EEPPatrolGroup::SetPatrolPoints(std::vector<U2DPoint<long> > inPoints, unsigned long inPlayerID)
{
    std::vector<EEComplexUnit*> theKept;
    U2DPoint<long> theTile;
    U2DPoint<long> theOther;
    std::vector<EEComplexUnit*>::iterator theIterator = this->mPatrolPoints.begin();
    while (theIterator != this->mPatrolPoints.end()) {
        EEComplexUnit* theUnit = *theIterator;
        theTile.mX = theUnit->GetTileX();
        theTile.mY = theUnit->GetTileY();
        if (std::find(inPoints.begin(), inPoints.end(), theTile) == inPoints.end()) {
            theUnit->SetPatrolGroup(NULL);
            theUnit->Kill(1, 0, 0);
            theUnit->GetPlayer()->KillUnit(theUnit, false);
            theIterator = this->mPatrolPoints.erase(theIterator);
        } else {
            ++theIterator;
        }
    }
    std::vector<U2DPoint<long> >::iterator thePoint;
    for (thePoint = inPoints.begin(); thePoint != inPoints.end(); ++thePoint) {
        theOther.mX = thePoint->mX;
        theOther.mY = thePoint->mY;
        if (std::find_if(this->mPatrolPoints.begin(), this->mPatrolPoints.end(), EEPatrolTileIs(theOther)) == this->mPatrolPoints.end()) {
            GE2DPoint theNew((float)theOther.mX, (float)theOther.mY);
            this->AddPoint(theNew, inPlayerID);
        }
    }
    for (thePoint = inPoints.begin(); thePoint != inPoints.end(); ++thePoint) {
        theOther.mX = thePoint->mX;
        theOther.mY = thePoint->mY;
        theKept.push_back(*std::find_if(this->mPatrolPoints.begin(), this->mPatrolPoints.end(), EEPatrolTileIs(theOther)));
    }
    this->mPatrolPoints.clear();
    this->mPatrolPoints = theKept;
    this->mIsLoop = true;
}

// TEMPLATE: EXE 0x560fba
// (EEPatrolTileIs's constructor, emitted by this unit)

// TEMPLATE: EXE 0x560fcc
// (std::find_if<_Ptrit<EEComplexUnit*>, EEPatrolTileIs>, SetPatrolPoints's two searches)

// TEMPLATE: EXE 0x560ff3
// (EEPatrolTileIs::operator(), find_if's predicate)
