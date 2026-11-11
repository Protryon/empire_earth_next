// C:\EEX\Empire Earth\AI Thread\EECPILocation.cpp (file name inferred: no __FILE__; RTTI class EECPILocation,
// vtable 0x43a6b4). A location in the computer player's intelligence: 0x80816..0x80c69 (the file-scope key's
// unguarded init chain first; EECPIArmy.cpp's code ends at 0x80816, EECPIntelligence.cpp's EECPIntelligencePlayerFile
// ctor starts at 0x80c69). Clear (0x46527) and GetPathSubSector (0x4e0b1) are defined here but ICF kept twins in
// earlier units.
// Update (0x80a57) is not exact: the original refuses both GetContinentAt(x, y) inlines and both mTowns[i]
// operator+ expansions, keeps &mTile in edi and tail-merges `mSerial = theArea->mSerial`.
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEAreaUtilities.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EEAIForest.h"
#include "ee/EEAIHill.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EETown.h"
#include "ee/EECPILocation.h"

static const UString kData("EECPILocation-Data");

// port (R5a.2, "load equals continue"): set while a game saved from version 0.263 on is loaded, until its first tick
// (EEWorld::StartGame). The running game calls Update on a location only at a group refresh (RefreshTaskLocation),
// which only a load runs (RefreshResourceCenters), and at a load (Restore). The original re-found the areas then
// because a load renumbered them; a 0.263 save keeps the continent manager and its serials (EEAIContinentSave.cpp),
// so the saved area stands. Update re-found a choke point task as a continent (the load had made no choke points
// yet when it restored the tasks) and dropped a town task whose location names player 0 (two of the original's
// tasks leave mPlayer 0: EECPAttackGroup's continent task, EECPDefenseCommander's aid task)
bool gLoadKeepsLocations = false;
// SYNTHETIC: EXE 0x480816   (wrapper)
// SYNTHETIC: EXE 0x480820   (ctor)
// SYNTHETIC: EXE 0x480831   (registrar)
// SYNTHETIC: EXE 0x48083d   (dtor)

// FUNCTION: EXE 0x480848
// Creates a location at the given tile, with no area, type or player
EECPILocation::EECPILocation(const EETileLocation& inTile)
{
    this->Clear();
    this->mTile = inTile;
}

// FUNCTION: EXE 0x480873
// Creates an empty location at tile (0, 0)
EECPILocation::EECPILocation()
{
    this->Clear();
}

// FUNCTION: EXE 0x48088d
// Creates a copy of a location (area serial, type, tile and player)
EECPILocation::EECPILocation(const EECPILocation& inLocation)
{
    this->mSerial = inLocation.mSerial;
    this->mType = inLocation.mType;
    this->mTile = inLocation.mTile;
    this->mPlayer = inLocation.mPlayer;
}

// FUNCTION: EXE 0x4808c2
// Returns true if the location's tile is on the map; otherwise moves it to a random point of its area (if any)
// and returns whether that point is on the map
bool EECPILocation::IsValid()
{
    bool theResult = gWorld.mMap->GetTileAtLocation(this->mTile) != NULL;
    if (theResult || this->mSerial == 0)
        return theResult;
    EEAIArea* theArea = gContinentManager.GetArea(this->mSerial);
    if (theArea != NULL) {
        this->mTile = theArea->GetRandomPoint();
        theResult = gWorld.mMap->GetTileAtLocation(this->mTile) != NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x480926
// Returns true if the two locations' tiles are the same or touch (8-neighbourhood)
bool EECPILocation::IsAdjacent(const EECPILocation& inLocation) const
{
    long theDeltaX = this->mTile.mX - inLocation.mTile.mX;
    if (theDeltaX < 0)
        theDeltaX = -theDeltaX;
    if (theDeltaX <= 1) {
        long theDeltaY = this->mTile.mY - inLocation.mTile.mY;
        if (theDeltaY < 0)
            theDeltaY = -theDeltaY;
        if (theDeltaY <= 1)
            return true;
    }
    return false;
}

// FUNCTION: EXE 0x480951
// Returns true if both locations lie in path sub-sectors and these are the same or neighbours
bool EECPILocation::IsConnected(const EECPILocation& inLocation) const
{
    EEAIPathSubSector* theSubSector = this->GetPathSubSector();
    EEAIPathSubSector* theOther = inLocation.GetPathSubSector();
    if (theSubSector != NULL && theOther != NULL && (theSubSector == theOther || theSubSector->IsNeighbor(theOther)))
        return true;
    return false;
}

// FUNCTION: EXE 0x480984
// Reads the tile, area serial, type and player from a saved game; returns the first read error
long EECPILocation::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mTile.Load(inFile);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSerial);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mType);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPlayer);
    return theResult;
}

// FUNCTION: EXE 0x4809c7
// Moves the location to a random point of its area, if it has one
void EECPILocation::MoveIntoArea()
{
    EEAIArea* theArea = this->mSerial != 0 ? gContinentManager.GetArea(this->mSerial) : 0;
    if (theArea != NULL)
        this->mTile = theArea->GetRandomPoint();
}

// FUNCTION: EXE 0x480a01
// Re-derives the location's area after a load; always succeeds
long EECPILocation::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->Update();
    return kErrorOK;
}

// FUNCTION: EXE 0x480a0b
// Writes the tile, area serial, type and player to the saved game; returns the first write error
long EECPILocation::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mTile.Save(outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSerial);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPlayer);
    return theResult;
}

// FUNCTION: EXE 0x480a57
// Re-finds the area of the location's type at its tile (continent, choke point, forest, hill, rectangle, the
// player's town) and stores its serial; armies are left alone; with no area found the type becomes none
void EECPILocation::Update()
{
    if (gLoadKeepsLocations)   // port: a load of a 0.263 save keeps the saved area (ee/EECPILocation.h)
        return;
    EEAIArea* theArea = NULL;
    if (this->GetTile().GetX() < gMapWidth && this->GetTile().GetY() < gMapHeight) {
        switch (this->mType) {
        case kAIAreaNone:
        case kAIAreaContinent:
            theArea = gContinentManager.GetContinentAt(this->GetTile());
            if (theArea != NULL)
                this->mType = kAIAreaContinent;
            break;
        case kAIAreaChokePoint:
            theArea = gContinentManager.GetChokePointAt(this->GetTile());
            break;
        case kAIAreaForest:
            theArea = gContinentManager.GetForestAt(this->GetTile());
            break;
        case kAIAreaHill:
            theArea = gContinentManager.GetHillAt(this->GetTile());
            break;
        case kAIAreaRectangleArea:
            theArea = gContinentManager.GetRectangleAreaAt(this->GetTile());
            break;
        case kAIAreaTown: {
            this->mType = kAIAreaNone;
            this->mSerial = 0;
            EEPlayer* thePlayer = gWorld.mPlayers[this->mPlayer];
            if (thePlayer == NULL)
                return;
            for (long i = thePlayer->mTowns.size() - 1; i >= 0; i--) {
                if (!thePlayer->mTowns[i]->mHull.BoundedContains(this->GetTile()))
                    continue;
                this->mType = kAIAreaTown;
                theArea = thePlayer->mTowns[i];
                break;
            }
            if (theArea == NULL)
                return;
            break;
        }
        case kAIAreaArmy:
            return;
        case 10:
            theArea = gContinentManager.GetContinentAt(this->GetTile());
            if (theArea != NULL)
                this->mType = 10;
            break;
        }
    }
    if (theArea != NULL)
        this->mSerial = theArea->mSerial;
    else
        this->mType = 0;
}

// FUNCTION: EXE 0x480be4
// Clears the location and puts it at the unit's tile (if a unit is given)
void EECPILocation::SetUnit(EEUnit* inUnit)
{
    this->Clear();
    if (inUnit != NULL)
        this->mTile.SetXY(inUnit->GetTileX(), inUnit->GetTileY());
}

// FUNCTION: EXE 0x480c04
// Moves the location to tile (0, 0), then to a random point of its area if it has one
void EECPILocation::ResetToArea()
{
    this->mTile.SetXY(0, 0);
    if (this->mSerial == 0)
        return;
    EEAIArea* theArea = gContinentManager.GetArea(this->mSerial);
    if (theArea != NULL)
        this->mTile = theArea->GetRandomPoint();
}

// FUNCTION: EXE 0x480c42
// Copies another location (area serial, type, tile and player)
EECPILocation& EECPILocation::operator=(const EECPILocation& inLocation)
{
    this->mSerial = inLocation.mSerial;
    this->mType = inLocation.mType;
    this->mTile = inLocation.mTile;
    this->mPlayer = inLocation.mPlayer;
    return *this;
}

// The out-of-line members ICF folded into twins of earlier units (no address of their own).
// void EECPILocation::Clear()   // (defined at its address in another unit; this copy duplicated the definition in the full link)
// {
//     mSerial = 0;
//     mType = 0;
//     mPlayer = 0;
//     mTile.mX = 0;
//     mTile.mY = 0;
// }

// EEAIPathSubSector* EECPILocation::GetPathSubSector() const   // (defined at its address in another unit; this copy duplicated the definition in the full link)
// {
//     return gContinentManager.GetPathSubSectorAt(mTile.GetX(), mTile.GetY());
// }
