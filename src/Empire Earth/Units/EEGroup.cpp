// C:\EEX\Empire Earth\Units\EEGroup.cpp (file name inferred: no __FILE__). A group of units: 0x205eda..0x20834c.
// Order: the two key statics' `$E` chains, EEGroupPathRequest (ctor, ??_G, dtor,
// Cancel), EEGroup's members and vtable slots, MakeClusters, then the template COMDATs (EEGroupCluster's inline ctor,
// map<long, EEGroupPathRequest*>, map<unsigned short, vector<EEComplexUnit*> >, vector<vector<GE3DPoint> >).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>
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
#include "lle/GERectangle.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEFormation.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EETileCoordinate.h"
#include "ee/EEPathManager.h"
#include "ee/EEAutoPathRequest.h"
#include "ee/EEGroupPathRequest.h"
#include "ee/EEGroupCluster.h"

const UString EEGroup::sNumSelectedUnitsStringID("EEGroup-NumSelectedUnitsStringID");   // 0x963278 (provisional name)
// SYNTHETIC: EXE 0x605eda   (wrapper)
// SYNTHETIC: EXE 0x605ee4   (ctor)
// SYNTHETIC: EXE 0x605ef5   (registrar)
// SYNTHETIC: EXE 0x605f01   (dtor)
const UString EEGroup::sUnitIDStringID("EEGroup-UnitIDStringID");   // 0x963260 (provisional name)
// SYNTHETIC: EXE 0x605f1d   (wrapper)
// SYNTHETIC: EXE 0x605f27   (ctor)
// SYNTHETIC: EXE 0x605f38   (registrar)
// SYNTHETIC: EXE 0x605f44   (dtor)

// FUNCTION: EXE 0x605f60
// Creates a group path request holding the path (referenced) and the ids of the units that follow it
EEGroupPathRequest::EEGroupPathRequest(EEAutoPathRequest* inPath, const std::vector<long>& inUnitIDs, long inArg3)
    : mPath(NULL)
{
    if (inPath)
        inPath->Reference();
    this->mPath = inPath;
    this->mUnitIDs = inUnitIDs;
    this->mL30 = inArg3;
}

// SYNTHETIC: EXE 0x605fbb
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x605fd7
// Destroys the request, releasing its path
EEGroupPathRequest::~EEGroupPathRequest()
{
    if (this->mPath)
        this->mPath->Dereference();
}

// FUNCTION: EXE 0x605ff7
// Cancels the pending path with the path manager and releases it
void EEGroupPathRequest::Cancel()
{
    if (!this->mPath)
        return;
    gPathManager.CancelAutoPathRequest(this->mPath);
    this->mPath->Dereference();
    this->mPath = NULL;
}

// FUNCTION: EXE 0x60601b
// Creates an empty group for a player with the default formation
EEGroup::EEGroup(EEPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
    this->mFormation = EEFormation::Create(0);
    this->mKindFlags = 0;
}

// SYNTHETIC: EXE 0x606086
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x6060a2
// Destroys the group, cancelling and deleting its path requests and releasing its formation
EEGroup::~EEGroup()
{
    std::map<long, EEGroupPathRequest*>::iterator it;
    for (it = this->mPathRequests.begin(); it != this->mPathRequests.end(); it++) {
        gPathManager.CancelAutoPathRequest(it->second->mPath);
        delete it->second;
    }
    if (this->mFormation)
        this->mFormation->Dereference();
}

// FUNCTION: EXE 0x606121
// Copies another group's units, type counts and kind flags, moving the unit references over
EEGroup& EEGroup::CopyGroup(EEGroup* inGroup)
{
    std::vector<EEComplexUnit*>::iterator it;
    for (it = this->mUnits.begin(); it != this->mUnits.end(); it++)
        (*it)->Dereference();
    this->mUnitTypeCounts = inGroup->mUnitTypeCounts;
    this->mUnits = inGroup->mUnits;
    this->mKindFlags = inGroup->mKindFlags;
    for (it = this->mUnits.begin(); it != this->mUnits.end(); it++)
        (*it)->Reference();
    return *this;
}

// FUNCTION: EXE 0x606174
// Adds a unit (referenced) and counts its type and kind flags
void EEGroup::Add(EEComplexUnit* inUnit)
{
    long theType = inUnit->mUnitClass->mUnitType;
    this->mUnits.push_back(inUnit);
    inUnit->Reference();
    this->mKindFlags |= inUnit->mUnitClass->mKindFlags;
    std::map<long, long>::iterator it = this->mUnitTypeCounts.find(theType);
    if (it != this->mUnitTypeCounts.end())
        it->second++;
    else
        this->mUnitTypeCounts.insert(std::map<long, long>::value_type(theType, 1));
}

// FUNCTION: EXE 0x6061ee
// Returns the rectangle around the group's units that are on the map
void EEGroup::GetBounds(GERectangle& outBounds)
{
    bool theFirst = false;
    GE2DPoint thePoint;
    for (unsigned long i = 0; i < this->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = this->GetUnit(i);
        float theX = theUnit->mPhysicalModel.GetLocation().GetX();
        thePoint.SetX(theX);
        thePoint.SetY(theUnit->mPhysicalModel.GetLocation().GetY());
        if (theUnit->mTile == NULL || theUnit->mTileX == -10000)
            continue;
        if (theFirst) {
            outBounds.Extend(thePoint);
        } else {
            theFirst = true;
            outBounds.SetBottom(thePoint.GetY());
            outBounds.SetRight(theX);
            outBounds.SetTop(thePoint.GetY());
            outBounds.SetLeft(theX);
        }
    }
}

// FUNCTION: EXE 0x606280
// Cancels and deletes the path request with the given id
void EEGroup::Remove(long inPathID)
{
    std::map<long, EEGroupPathRequest*>::iterator it = this->mPathRequests.find(inPathID);
    if (it == this->mPathRequests.end())
        return;
    EEGroupPathRequest* theRequest = it->second;
    theRequest->Cancel();
    this->mPathRequests.erase(it);
    delete theRequest;
}

// FUNCTION: EXE 0x6062cb
// Returns the terrain flags every unit in the group shares
EECPASGFlags EEGroup::GetFlags()
{
    EECPASGFlags theResult;
    EECPASGFlags theUnitFlags;
    memset(theResult.mFlags, 1, sizeof(theResult.mFlags));
    for (long i = this->mUnits.size() - 1; i >= 0; i--) {
        memset(theUnitFlags.mFlags, 0, sizeof(theUnitFlags.mFlags));
        theUnitFlags.mPlayerID = 0;
        this->GetUnitAt(i)->mUnitClass->GetTerrainFlags(this->GetUnitAt(i), theUnitFlags);
        theResult.And(theUnitFlags);
    }
    return theResult;
}

// FUNCTION: EXE 0x60637e
// Returns whether the unit is in the group
bool EEGroup::Contains(EEComplexUnit* inUnit)
{
    std::vector<EEComplexUnit*>::iterator theEnd = this->mUnits.end();
    std::vector<EEComplexUnit*>::iterator it;
    for (it = this->mUnits.begin(); it != theEnd; it++) {
        if (*it == inUnit)
            break;
    }
    return it != theEnd;
}

// FUNCTION: EXE 0x6063a1
// Replaces the group's units and formation with another group's, keeping only valid units on the map
void EEGroup::CopyUnits(EEGroup* inGroup)
{
    unsigned long theCount = inGroup->GetNumUnits();
    this->Clear();
    this->SetFormation(inGroup->mFormation);
    this->mUnits.reserve(theCount);
    for (unsigned long i = 0; i < theCount; i++) {
        EEComplexUnit* theUnit = inGroup->GetUnit(i);
        if (theUnit->mIsValid && theUnit->mTileX != -10000 && theUnit->mTileY != -10000)
            this->Add(theUnit);
    }
}

// FUNCTION: EXE 0x606405
// Returns the first unit of a category, or NULL
EEComplexUnit* EEGroup::FindUnitOfCategory(long inCategory)
{
    EEComplexUnit* theResult = NULL;
    for (std::vector<EEComplexUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
        if ((*it)->mUnitClass->mCategory == inCategory) {
            theResult = *it;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x60642e
// Returns the first unit whose class has any of the kind flags, or NULL
EEComplexUnit* EEGroup::FindUnitWithFlags(unsigned long inFlags)
{
    EEComplexUnit* theResult = NULL;
    if ((this->mKindFlags & inFlags) > 0) {
        for (std::vector<EEComplexUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
            if (((*it)->mUnitClass->mKindFlags & inFlags) > 0) {
                theResult = *it;
                break;
            }
        }
    } else {
        theResult = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x606462
// Counts the group's units of a category
long EEGroup::CountUnitsOfKind(long inKind)
{
    long theCount = 0;
    for (std::vector<EEComplexUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
        if ((*it)->mUnitClass->mCategory == inKind)
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x60648c
// Returns whether a pending path request carries the unit
bool EEGroup::HasPathRequest(EEComplexUnit* inUnit)
{
    return this->FindPathRequest(inUnit) != NULL;
}

// FUNCTION: EXE 0x60649e
// Returns the pending path request that carries the unit, or NULL
EEGroupPathRequest* EEGroup::FindPathRequest(EEComplexUnit* inUnit)
{
    EEGroupPathRequest* theResult = NULL;
    std::map<long, EEGroupPathRequest*>::iterator it;
    for (it = this->mPathRequests.begin(); it != this->mPathRequests.end(); it++) {
        EEGroupPathRequest* theRequest = it->second;
        std::vector<long>::iterator theID;
        for (theID = theRequest->mUnitIDs.begin(); theID != theRequest->mUnitIDs.end(); theID++) {
            if (*theID == (long)inUnit->mUnitID) {
                theResult = it->second;
                break;
            }
        }
        if (theResult != NULL)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x6064f4
// Reads the formation kind (8 = keep the current one) and appends the unit ids from the saved game;
// returns the first count or id read error
long EEGroup::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theKind;
    inFile.Read(theKind);
    if (theKind != 8) {
        EEFormation* theFormation = EEFormation::Create(theKind);
        if (theFormation) {
            if (this->mFormation)
                this->mFormation->Dereference();
            this->mFormation = theFormation;
        }
    }
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
            unsigned long theID;
            theResult = inFile.Read(theID);
            this->mUnits.push_back(reinterpret_cast<EEComplexUnit*>(theID));
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x606581
// Requests one path for a cluster's units; follows it at once if it is already found, else keeps the request
void EEGroup::RequestPath(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, EEGroupCluster* inCluster)
{
    std::vector<long> theIDs;
    if (inCluster->mUnits.size() != 0) {
        for (long i = inCluster->mUnits.size() - 1; i >= 0; i--)
            theIDs.push_back(inCluster->mUnits[i]->mUnitID);
        EEAutoPathRequest* thePath = gPathManager.RequestPath(this->mPlayer->GetPlayerID(), inFrom, inTo, &inCluster->mFlags, 1, -1, NULL, 0);
        EEGroupPathRequest* theRequest = new EEGroupPathRequest(thePath, theIDs, 0);
        thePath->Dereference();
        if (thePath && thePath->mIsDone) {
            this->MoveAlong(theRequest);
            delete theRequest;
        } else {
            this->mPathRequests.insert(std::map<long, EEGroupPathRequest*>::value_type(thePath->mRequestID, theRequest));
        }
    }
}

#include <math.h>
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EEMapDimensions.h"

// EEAIContinentManager::GetPathAreaIDAt (inline, its out-of-line COMDAT 0x208334 follows this unit's template
// block; MoveAlong's first site calls it, the second expands it): the path area's id at inTile, 0 if none.
// The ternary is unsigned short on both arms (mov ax, not movzx: 0x208341).
// not inline: other units call it (VC6 left an out-of-line copy of the inline for them; clang does not)
unsigned short EEAIContinentManager::GetPathAreaIDAt(const EETileLocation& inTile)
{
    EEAIPathArea* theArea = this->GetPathAreaAt(inTile);
    return theArea != NULL ? theArea->mID : (unsigned short)0;
}

// The (x, y) overload: MoveAlong's first site passes UMFloor values (copied into the parameter slots
// [ebp-0x80]/[ebp-0x1c], 0x2069e1); nested under it, the U2DPoint ctor (0x2069fb) and the tile overload (0x206a03)
// are refused by the depth-2 budget share, which emits the 0x208334 COMDAT.
inline unsigned short EEAIContinentManager::GetPathAreaIDAt(long inX, long inY)
{
    return this->GetPathAreaIDAt(EETileLocation(inX, inY));
}

// FUNCTION: EXE 0x606680
// Gives each living group unit carried by a found path its waypoints: its formation slot at each step, or
// the step's centre when the slot lies in another path area; an empty path when none was found
void EEGroup::MoveAlong(EEGroupPathRequest* inRequest)
{
    EEComplexUnit* theUnit;
    EEAIPathSubSector* theSubSector;
    unsigned short theAreaID;
    unsigned short theSlotAreaID;
    std::vector<EEFormationSlot> theSlots;
    GE3DPoint thePoint;
    GE3DPoint theOriginalCenter;
    std::vector<GE3DPoint> thePath;
    std::vector<EEComplexUnit*> theUnits;
    std::vector<std::vector<GE3DPoint> > theUnitPaths;
    std::vector<GE3DPoint> theEmptyPath;
    EETileLocation theLocation;

    for (long i = (long)inRequest->mUnitIDs.size() - 1; i >= 0; i--) {
        theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(inRequest->mUnitIDs[i]));
        if (theUnit == NULL || !(theUnit->mHitPoints > 0) || theUnit->mIsDisabled != false
            || theUnit->mTile == NULL || !this->Contains(theUnit))
            continue;
        theUnits.push_back(theUnit);
        std::vector<GE3DPoint> theEmpty;
        theUnitPaths.push_back(theEmpty);
    }
    if (this->mFormation != NULL) {
        theOriginalCenter.mX = this->mFormation->mCenter.GetX();
        theOriginalCenter.mY = this->mFormation->mCenter.GetY();
        theOriginalCenter.mZ = this->mFormation->mCenter.mZ;
    }
    if (inRequest->mPath != NULL && inRequest->mPath->mPath.size() > 1) {
        if (theUnits.size() > 0) {
            for (long i = 1; i < (long)inRequest->mPath->mPath.size() - 1; i++) {
                const EETileCoordinate& theTile = inRequest->mPath->mPath[i];
                theSubSector = gContinentManager.GetPathSubSectorAt(theTile.GetX(), theTile.GetY());
                GE3DPoint theCenter((float)theSubSector->mLocation.GetX() + 0.5f,
                                    (float)theSubSector->mLocation.GetY() + 0.5f, 0.0f);
                thePath.push_back(theCenter);
            }
            for (long p = 0; p < (long)thePath.size(); p++) {
                thePoint = thePath[p];
                theSlots.clear();
                if (this->mFormation != NULL && p < (long)thePath.size()) {
                    // The original stores a GE3DPoint at +48. These genuine existing scalar fields cover that point.
                    this->mFormation->mCenter.mX = thePoint.GetX();
                    this->mFormation->mCenter.mY = thePoint.GetY();
                    this->mFormation->mCenter.mZ = thePoint.mZ;
                    this->mFormation->Place(theUnits, theSlots);
                }
                if (theSlots.size() == 0) {
                    EEFormationSlot theSlot;
                    theSlot.mPosition = thePoint;
                    theSlot.mUnit = NULL;
                    theSlot.mHeading = 0.0f;
                    theSlots.push_back(theSlot);
                }
                theAreaID = gContinentManager.GetPathAreaIDAt(UMFloor(thePoint.GetX()), UMFloor(thePoint.GetY()));
                for (long u = (long)theUnitPaths.size() - 1; u >= 0; u--) {
                    EETileLocation theSlotTile(UMFloor(theSlots.back().mPosition.GetX()),
                                              UMFloor(theSlots.back().mPosition.GetY()));
                    theLocation = theSlotTile;
                    if ((unsigned long)theSlotTile.GetX() < gMapWidth && (unsigned long)theSlotTile.GetY() < gMapHeight) {
                        theSlotAreaID = gContinentManager.GetPathAreaIDAt(theLocation);
                        if (theSlotAreaID == theAreaID)
                            (theUnitPaths[u]).push_back(theSlots.back().mPosition);
                        else
                            (theUnitPaths[u]).push_back(thePoint);
                    } else
                        (theUnitPaths[u]).push_back(thePoint);
                    if (theSlots.size() > 1)
                        theSlots.pop_back();
                }
            }
            for (long u = (long)theUnits.size() - 1; u >= 0; u--)
                (theUnits[u])->SetGroupPathGoals(theUnitPaths[u]);
        }
    } else {
        for (long u = (long)theUnits.size() - 1; u >= 0; u--)
            (theUnits[u])->SetGroupPathGoals(theEmptyPath);
    }
    if (this->mFormation != NULL) {
        this->mFormation->mCenter.mX = theOriginalCenter.GetX();
        this->mFormation->mCenter.mY = theOriginalCenter.GetY();
        this->mFormation->mCenter.mZ = theOriginalCenter.mZ;
    }
}

// FUNCTION: EXE 0x606c41
// Recomputes the group's kind flags from its units
void EEGroup::UpdateKindFlags()
{
    this->mKindFlags = 0;
    unsigned long theCount = this->GetNumUnits();
    for (unsigned long i = 0; i < theCount; i++)
        this->mKindFlags |= this->GetUnit(i)->mUnitClass->mKindFlags;
}

// FUNCTION: EXE 0x606c73
// Removes and releases every unit and resets the type counts and kind flags
void EEGroup::Clear()
{
    while (this->mUnits.size() > 0) {
        this->mUnits[0]->Dereference();
        this->mUnits.erase(this->mUnits.begin());
    }
    this->mKindFlags = 0;
    this->mUnitTypeCounts.clear();
}

// FUNCTION: EXE 0x606cc9
// Cancels and deletes the path request with the given id (as Remove)
void EEGroup::CancelPathRequest(long inPathID)
{
    std::map<long, EEGroupPathRequest*>::iterator it = this->mPathRequests.find(inPathID);
    if (it == this->mPathRequests.end())
        return;
    it->second->Cancel();
    EEGroupPathRequest* theRequest = it->second;
    this->mPathRequests.erase(it);
    delete theRequest;
}

// FUNCTION: EXE 0x606d15
// Removes the unit from the group if it is in it
void EEGroup::RemoveUnit(EEComplexUnit* inUnit)
{
    for (std::vector<EEComplexUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++) {
        if (*it == inUnit) {
            this->RemoveAt(it - this->mUnits.begin());
            break;
        }
    }
}

// FUNCTION: EXE 0x606d40
// Removes and releases the unit at an index; recomputes the kind flags when its type's last unit leaves
void EEGroup::RemoveAt(long inIndex)
{
    long theType = this->mUnits[inIndex]->mUnitClass->mUnitType;
    std::map<long, long>::iterator it = this->mUnitTypeCounts.find(theType);
    this->mUnits[inIndex]->Dereference();
    this->mUnits.erase(this->mUnits.begin() + inIndex);
    if (--it->second == 0) {
        this->mUnitTypeCounts.erase(it);
        this->UpdateKindFlags();
    }
}

// FUNCTION: EXE 0x606dd9
// Turns the loaded unit ids back into units, dropping any that no longer exist; returns 0
long EEGroup::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<long> theIDs;
    for (std::vector<EEComplexUnit*>::iterator it = this->mUnits.begin(); it != this->mUnits.end(); it++)
        theIDs.push_back((int32_t)USavedID(*it));
    this->mUnits.clear();
    for (unsigned long i = 0; i < theIDs.size(); i++) {
        EEUnit* theUnit = static_cast<EEUnit*>(gWorld.GetUnit(*(theIDs.begin() + i)));
        if (theUnit && theUnit->mComplexUnit)
            this->Add(theUnit->mComplexUnit);
    }
    return 0;
}

// FUNCTION: EXE 0x606e61
// Writes the formation kind (8 = none), unit count and unit ids to the saved game; returns the first error
long EEGroup::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theKind = 8;
    unsigned long theCount = this->mUnits.size();
    if (this->mFormation)
        theKind = this->mFormation->mType;
    long theResult = outFile.Write(theKind);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
        unsigned long theID = this->GetUnitAt(i)->mUnitID;
        theResult = outFile.Write(theID);
    }
    return theResult;
}

// FUNCTION: EXE 0x606ef2
// Sets the group's formation, moving the reference from the old one
void EEGroup::SetFormation(EEFormation* inFormation)
{
    if (inFormation == this->mFormation)
        return;
    if (this->mFormation)
        this->mFormation->Dereference();
    this->mFormation = inFormation;
    if (inFormation)
        inFormation->Reference();
}

// U2DRectangle<long>::GetClosestCorner(point) (inline in lle/U2DRectangle.h, exe view; its out-of-line COMDAT
// 0x208164 follows MakeClusters, the only user)

// FUNCTION: EXE 0x606f1a
// Splits units into path clusters: land buildings and flyers by continent, the rest by shared terrain
// flags and distance to a cluster's bounds (squared when asked)
// NEAR: ours expands the bounds' GetClosestCorner inline; the original calls its COMDAT 0x208164.
// The bounds are the declared four longs (no layout alias). theBoats holds the flyers (domain bit 1).
void EEGroup::MakeClusters(std::vector<EEComplexUnit*>& inUnits, std::vector<EEGroupCluster*>& outClusters,
                           long inDistance, bool inSquare)
{
    EEComplexUnit* theUnit;
    unsigned short theContinent;
    bool theNewCluster;
    bool theNear;
    typedef std::map<unsigned short, std::vector<EEComplexUnit*> > UnitMap;
    UnitMap theContinents;
    UnitMap theBoats;
    EECPASGFlags theFlags;
    EEGroupCluster* theCluster = NULL;
    U2DPoint<long> theCenter;
    if (inSquare)
        inDistance *= inDistance;
    for (long i = inUnits.size() - 1; i >= 0; i--) {
        theUnit = inUnits[i];
        theContinent = gContinentManager.GetContinentID(theUnit);
        if (theUnit->mUnitClass->IsLandBuilding()) {
            UnitMap::iterator theIt = theContinents.find(theContinent);
            if (theIt == theContinents.end())
                theIt = theContinents.insert(std::pair<unsigned short, std::vector<EEComplexUnit*> >(theContinent,
                                       std::vector<EEComplexUnit*>())).first;
            if (theIt != theContinents.end())
                theIt->second.push_back(theUnit);
        } else if (theUnit->mUnitClass->mDomains & kMovementAir) {
            UnitMap::iterator theIt = theBoats.find(theContinent);
            if (theIt == theBoats.end())
                theIt = theBoats.insert(std::pair<unsigned short, std::vector<EEComplexUnit*> >(theContinent,
                                       std::vector<EEComplexUnit*>())).first;
            if (theIt != theBoats.end())
                theIt->second.push_back(theUnit);
        } else {
            theUnit->mUnitClass->GetTerrainFlags(theUnit, theFlags);
            theNewCluster = true;
            U2DPoint<long> theTile(theUnit->mTileX, theUnit->mTileY);
            for (long j = outClusters.size() - 1; j >= 0; j--) {
                theCluster = outClusters[j];
                if (memcmp(theCluster->mFlags.mFlags, theFlags.mFlags, 17) != 0)
                    continue;
                theNear = !inSquare || inDistance < 1;
                if (!theNear) {
                    EETileCoordinate theCenterTile;
                    theCluster->mBounds.GetCenter(theCenterTile);
                    theCenter.SetXY(theCenterTile.GetX(), theCenterTile.GetY());
                    if (theCluster->mBounds.Contains(theTile.GetX(), theTile.GetY())
                        || theCenter.DistanceToSquared(theTile) <= inDistance) {
                        theNear = true;
                    } else {
                        U2DPoint<long> theClosest = theCluster->mBounds.GetClosestCorner(theTile);
                        theNear = theTile.DistanceToSquared(theClosest) <= inDistance;
                    }
                }
                if (!theNear)
                    continue;
                theNewCluster = false;
                theCluster->mUnits.push_back(theUnit);
                theCluster->mBounds.Extend(theTile);
                break;
            }
            if (theNewCluster) {
                theCluster = new EEGroupCluster;
                theCluster->mUnits.push_back(theUnit);
                theCluster->mFlags = theFlags;
                theCluster->mKind = 1;
                theCluster->mBounds.Set(theTile);
                outClusters.push_back(theCluster);
            }
        }
    }
    for (UnitMap::iterator c = theContinents.begin(); c != theContinents.end(); c++) {
        theCluster = new EEGroupCluster;
        theCluster->mUnits = c->second;
        theCluster->mKind = 3;
        theCluster->mContinentID = c->first;
        outClusters.push_back(theCluster);
    }
    for (UnitMap::iterator b = theBoats.begin(); b != theBoats.end(); b++) {
        theCluster = new EEGroupCluster;
        theCluster->mUnits = b->second;
        theCluster->mKind = 2;
        theCluster->mContinentID = b->first;
        outClusters.push_back(theCluster);
    }
}

// FUNCTION: EXE 0x60754e
// Moves the units of every path request whose path is found and deletes those requests
void EEGroup::MoveAlongFoundPaths()
{
    std::map<long, EEGroupPathRequest*>::iterator it;
    for (it = this->mPathRequests.begin(); it != this->mPathRequests.end();) {
        EEGroupPathRequest* theRequest = it->second;
        if (!theRequest->mPath->mIsDone) {
            it++;
        } else {
            it = this->mPathRequests.erase(it);
            this->MoveAlong(theRequest);
            delete theRequest;
        }
    }
}

// TEMPLATE: EXE 0x607510
// EEGroupCluster::EEGroupCluster (the cluster's inline ctor), emitted by MakeClusters

// TEMPLATE: EXE 0x6075b4
// map<long, long>::operator= (the type counts; CopyGroup)

// TEMPLATE: EXE 0x6075e8
// map<long, EEGroupPathRequest*>::~map

// TEMPLATE: EXE 0x607613
// vector<vector<GE3DPoint> >::push_back (MoveAlong)

// TEMPLATE: EXE 0x60763f
// map<unsigned short, vector<EEComplexUnit*> >::~map (MakeClusters)

// TEMPLATE: EXE 0x60766a
// map<unsigned short, vector<EEComplexUnit*> >::insert (MakeClusters)

// TEMPLATE: EXE 0x607724
// map<unsigned short, vector<...> >::find (MakeClusters; one ICF body for several value types)

// TEMPLATE: EXE 0x607760
// vector<EEGroupCluster*>::push_back (MakeClusters)

// TEMPLATE: EXE 0x60778c
// map<long, long>::_Copy (the type counts)

// TEMPLATE: EXE 0x6077ec
// map<long, EEGroupPathRequest*>::erase(iterator)

// TEMPLATE: EXE 0x60785d
// vector<vector<GE3DPoint> >::insert(iterator, const T&) (MoveAlong)

// TEMPLATE: EXE 0x6078ad
// _Ptrit<long>'s copy ctor

// TEMPLATE: EXE 0x6078ba
// map<unsigned short, ...>'s ctor (MakeClusters; one ICF body for several value types)

// TEMPLATE: EXE 0x6078db
// map<unsigned short, vector<EEComplexUnit*> >::erase(first, last) (MakeClusters)

// TEMPLATE: EXE 0x60794c
// map<unsigned short, vector<EEComplexUnit*> >::_Insert (MakeClusters)

// TEMPLATE: EXE 0x607ae0
// map<long, long>::_Copy(node, parent) (the type counts)

// TEMPLATE: EXE 0x607b65
// map<long, EEGroupPathRequest*>::_Erase(node)

// TEMPLATE: EXE 0x607b97
// map<long, ...*>::_Lbound (one ICF body for several pointer value types)

// TEMPLATE: EXE 0x607bc0
// vector<vector<GE3DPoint> >::insert(iterator, n, const T&) (MoveAlong)

// TEMPLATE: EXE 0x607e74
// _Tree::erase(first, last) (one ICF body for several map types)

// TEMPLATE: EXE 0x60812a
// map<unsigned short, vector<EEComplexUnit*> >::_Erase(node) (MakeClusters)

// FUNCTION: EXE 0x608164
// Returns the rectangle corner closest to a point (U2DRectangle<long>::GetClosestCorner, see above)

// TEMPLATE: EXE 0x608295
// fill over vector<GE3DPoint> elements (MoveAlong's vector<vector<GE3DPoint> >)

// TEMPLATE: EXE 0x6082b2
// copy_backward over vector<GE3DPoint> elements (MoveAlong's vector<vector<GE3DPoint> >)

// TEMPLATE: EXE 0x6082de
// _Construct(pair<const unsigned short, vector<EEComplexUnit*> >) (MakeClusters)

// TEMPLATE: EXE 0x6082fd
// fill over long elements (vector<long>)

// TEMPLATE: EXE 0x608315
// pair<unsigned short, vector<EEComplexUnit*> >(const&, const&): MakeClusters builds it; the original refuses it
// inline (no /Og), ours expands it (inline budget, depth)

// FUNCTION: EXE 0x608334
// Returns the id of the path area at a tile, 0 if none (EEAIContinentManager::GetPathAreaIDAt, written above)
// (out of line because MoveAlong's first site goes through the inline (x, y) overload, whose two nested calls
// (the U2DPoint ctor and this) the inline budget refuses at depth 2, 0x206a03)
