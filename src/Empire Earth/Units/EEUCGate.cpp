// C:\EEX\Empire Earth\Units\EEUCGate.cpp (file name inferred: no __FILE__; the class name).
// The wall gate's unit class:
// 0x21048f..0x212a6f (after EEUCFortress.cpp; EEUCHelicopter.cpp follows). One unit per EEUC* class in alphabetical
// order. Unit flags: the exe's module set. Its ctor is inline (EEUnitClass.cpp's factory). The unit opens with the
// `$E` chains of its two file-static colours, then Virtual20, EEBuildGate (EEMBuildGate's work), the slots and the
// gate helpers in address order; EEMap's corner-height inlines and the sort/unique COMDATs over the tile vector
// close it. A gate is five linked pieces (+0x1cc/+0x1d0): piece 2 is the gate, 0/4 the posts, 1/3 the doors.
#include <windows.h>
#include <stdio.h>
#include <typeinfo>
#include <algorithm>
#include <functional>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"

// The highlight of a wall that can take a gate, and the normal colour (`$E` chains 0x21048f / 0x2104bf; within one
// unit later globals get lower addresses: 0x5634f0 / 0x5634e0) (provisional names)
static GEColor sGateHighlight(0.0f, 1.0f, 0.0f);
// SYNTHETIC: EXE 0x61048f   (wrapper)
// SYNTHETIC: EXE 0x610499   (ctor)
// SYNTHETIC: EXE 0x6104b2   (registrar)
// SYNTHETIC: EXE 0x6104be   (dtor)
static GEColor sGateNormal(1.0f, 1.0f, 1.0f);
// SYNTHETIC: EXE 0x6104bf   (wrapper)
// SYNTHETIC: EXE 0x6104c9   (ctor)
// SYNTHETIC: EXE 0x6104e2   (registrar)
// SYNTHETIC: EXE 0x6104ee   (dtor)

// 0x447004..0x447018: the unit's named float constants (one .rdata block per unit) (provisional names)
static const float kGateOpen = 1.0f;            // 0x447004 (+0x228: fully open)
static const float kGateClosed = 0.0f;          // 0x447008
static const float kGateStep = 0.2f;            // 0x44700c (per update)
static const float kGateRangeBase = 1.0f;       // 0x447010 (added to half the class's +0x178)
static const float kGateMaximumSlope = 0.6f;    // 0x447014 (the gate's tiles' corner heights)
static const float kGateSearchRadius = 2.5f;    // 0x447018 (the units near the gate)

long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex);
void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles);
bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits);
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles);
// 0x24c627 (inTiles' units of inUnitType): ee/EEUnitUtilities.h
// 0x24c433: the units on inTiles (EECPWallManager.cpp declares it the same way)
long EEGetUnitsOn(std::vector<U2DPoint<long> >& inTiles, std::vector<EEUnit*>& outUnits);
// 0x24f8f9: after a gate is built for the current player (the new gate's piece 2) (provisional name)
void EECreateSpaceGateEnergyWall(EEComplexUnit* inGate);

// FUNCTION: EXE 0x6104ef
// Replaces the other four pieces of a gate whose gate piece was replaced (new owner and class), turning and
// relinking them; the posts go idle and the doors take the new gate piece's action
void EEUCGate::GateReplaced(EEUnit* inGate, EEUnit* inUnit)
{
    std::vector<EEUnit*> thePieces;
    EEUnitClass* theClass = inUnit->GetUnitClass();
    unsigned long theDirection = inUnit->GetComplexUnit()->mGateDirection;
    thePieces.resize(5);
    thePieces[2] = inUnit;
    for (long i = 0; i <= 4; i++) {
        if (i == 2)
            continue;
        EEComplexUnit* theOld = this->GetPart(inGate->GetComplexUnit(), i);
        EEComplexUnit* thePiece = EEReplaceUnit(theOld, inUnit->GetPlayer()->GetPlayerID(), theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, theDirection, i);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(theDirection);
        thePieces[i] = thePiece;
    }
    thePieces[4]->GetComplexUnit()->LinkPart(thePieces[0]->GetComplexUnit());
    thePieces[0]->GetComplexUnit()->LinkPart(thePieces[3]->GetComplexUnit());
    thePieces[3]->GetComplexUnit()->LinkPart(thePieces[1]->GetComplexUnit());
    thePieces[1]->GetComplexUnit()->LinkPart(thePieces[2]->GetComplexUnit());
    thePieces[4]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[0]->GetComplexUnit()->StartAction(kActionIdle);
    thePieces[3]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());
    thePieces[1]->GetComplexUnit()->StartAction(thePieces[2]->GetComplexUnit()->GetCommandType());

}

// FUNCTION: EXE 0x61080d
// Turns the five wall pieces across a tile into a closed gate of the technology's class for a player (EEMBuildGate's
// work): weakest wall's health, posts on the lower ground; adds the energy wall once space gates are available
void EEBuildGate(EETileCoordinate& inTile, long inPlayer, long inTechnology, long inDirection)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theWalls;
    std::vector<EEUnit*> theGate;
    GE3DPoint thePost4;
    U2DPoint<long> theTile;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    GE3DPoint thePost0;
    float theHealth = 1.0f;
    EEUnitClass* theClass = gUnitClassTable[thePlayer->mTechnologies->Get(inTechnology)->mData->mUnitClass];
    theTile.mX = inTile.mX;
    theTile.mY = inTile.mY;
    EEGetWallGateTiles(theTile, inDirection, theTiles);
    EEGetUnitsOn(theTiles, theUnits);
    for (unsigned long i = 0; i < theUnits.size(); i++) {
        if (theUnits[i]->GetUnitClass()->mIsWall)
            theWalls.push_back(theUnits[i]);
    }
    std::vector<EEUnit*> theSide;
    if (theWalls.size() == 0 || !EEDoesWallGateFit(inTile, inDirection, theWalls[0]->GetUnitClass()->GetUnitType(), theSide))
        return;
    long j;
    std::vector<EEUnit*>::iterator it;
    for (j = 0, it = theWalls.begin(); j <= 4; j++, it++) {
        if (it == theWalls.end())
            break;
        float theRatio = (float)(*it)->mHitPoints / (*it)->mStats->mMaxHitPoints;
        if (theHealth >= theRatio)
            theHealth = theRatio;
        EEComplexUnit* thePiece = EEReplaceUnit((EEComplexUnit*)*it, thePlayer->GetPlayerID(), theClass);
        GETransformation theTransformation(false);
        theTransformation = thePiece->mPhysicalModel.GetLocation();
        long theAction = EETurnGatePart(theTransformation, inDirection, j);
        thePiece->SetTransformation(theTransformation, true);
        thePiece->SetAnimation(theAction, -1, 0);
        thePiece->SetGateDirection(inDirection);
        theGate.push_back(thePiece);
    }
    theGate[4]->GetComplexUnit()->GetPosition(thePost4);
    theGate[0]->GetComplexUnit()->GetPosition(thePost0);
    float theZ = min(::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost4.GetX(), thePost4.GetY()),
                     ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(thePost0.GetX(), thePost0.GetY()));
    thePost4.SetZ(theZ);
    thePost0.SetZ(theZ);
    theGate[4]->GetComplexUnit()->SetLocation(thePost4, true);
    theGate[0]->GetComplexUnit()->SetLocation(thePost0, true);
    theGate[4]->GetComplexUnit()->LinkPart(theGate[0]->GetComplexUnit());
    theGate[0]->GetComplexUnit()->LinkPart(theGate[3]->GetComplexUnit());
    theGate[3]->GetComplexUnit()->LinkPart(theGate[1]->GetComplexUnit());
    theGate[1]->GetComplexUnit()->LinkPart(theGate[2]->GetComplexUnit());
    theGate[4]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[0]->GetComplexUnit()->StartAction(kActionIdle);
    theGate[3]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[1]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[2]->GetComplexUnit()->StartAction(kActionClosed);
    theGate[2]->SetHitPoints(UMFloatToLong(theGate[2]->GetStats()->mMaxHitPoints * theHealth));
    if (gUIManager->mInScenarioEditor && !gGameScreenActive) {
        ::gWorld.FlushQueuedPlacements();
        thePlayer->AddPendingUnits(0, true);
        thePlayer->RemoveKilledUnits();
    }
    if (thePlayer->mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && theClass->GetUnitType() != kUnitTypeGatePalisades)
        EECreateSpaceGateEnergyWall(dynamic_cast<EEComplexUnit*>(theGate[2]));
}

// FUNCTION: EXE 0x610e53
// Creates a gate unit at a transformation, the building's way
EEUnit* EEUCGate::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    return EEUCBuilding::CreateUnitAt(inTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x610e6b
// Creates a gate unit on a tile, the building's way
EEUnit* EEUCGate::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    return EEUCBuilding::CreateUnit(inX, inY, inPlayer, inIsFoundation, inCreator);
}

// The sort-and-unique of a vector (cdecl template 0x211ebc and its range half 0x211f07) (provisional names)
template <class V, class L, class E> typename V::iterator EESortUniqueRange(V& ioValues, L inLess, E inEqual)
{
    std::sort(ioValues.begin(), ioValues.end(), inLess);
    typename V::iterator theResult;
    theResult = std::unique(ioValues.begin(), ioValues.end(), inEqual);
    return theResult;
}
template <class V, class L, class E> void EESortUnique(V& ioValues, L inLess, E inEqual)
{
    typename V::iterator theEnd = EESortUniqueRange(ioValues, inLess, inEqual);
    ioValues.erase(theEnd, ioValues.end());
}

// FUNCTION: EXE 0x610e86
// Collects the footprint tiles of every gate piece not left out (doors and gate piece while open), sorted and unique
void EEUCGate::GetSurroundingTiles(EEComplexUnit* inUnit, std::vector<EETileCoordinate>& outTiles, long inCorners, bool inAppend)
{
    if (!inAppend)
        outTiles.clear();
    EEComplexUnit* thePiece = inUnit->GetComplexUnit();
    while (thePiece->mPreviousPart != NULL)
        thePiece = thePiece->mPreviousPart;
    for (; thePiece != NULL; thePiece = thePiece->mNextPart) {
        if (!this->IsPartLeftOut(thePiece))
            EEUnitClass::GetSurroundingTiles(thePiece, outTiles, inCorners, true);
    }
    // the COMDATs sort U2DPoint<long>s (operator< 0x13710, vector<U2DPoint<long> >::erase 0x288efb); EETileCoordinate
    // is U2DPoint<long>, so the tiles need no vector cast
    EESortUnique(outTiles, std::less<U2DPoint<long> >(), std::equal_to<U2DPoint<long> >());
}

// FUNCTION: EXE 0x610eee
// Returns whether a gate fits across a wall unit's tile in a direction (1..4), giving the five walls of the type
// (EECPWallManager.cpp declares it the same way)
bool EEDoesWallGateFitOnUnit(EEComplexUnit* inUnit, long inSide, long inUnitType, std::vector<EEUnit*>& outUnits)
{
    EETileCoordinate theTile;
    theTile.mX = inUnit->mTileX;
    theTile.mY = inUnit->mTileY;
    return EEDoesWallGateFit(theTile, inSide, inUnitType, outUnits);
}

// FUNCTION: EXE 0x610f19
// Returns whether a gate fits across a tile in a direction: five built, living walls of the type on level tiles
// (else outUnits is emptied)
bool EEDoesWallGateFit(const EETileCoordinate& inTile, long inDirection, unsigned long inUnitType,
                std::vector<EEUnit*>& outUnits)
{
    std::vector<U2DPoint<long> > theTiles;
    std::vector<EEUnit*> theUnused;
    bool theResult = true;
    U2DPoint<long> theTile(inTile.mX, inTile.mY);
    EEGetWallGateTiles(theTile, inDirection, theTiles);
    EEGetWallUnitsOfTypeOnTiles(theTiles, inUnitType, outUnits);
    std::vector<EEUnit*>::iterator it = outUnits.begin();
    while (true) {
        if (it == outUnits.end())
            break;
        if ((*it)->mComplexUnit->mIsBuilt && (*it)->mHitPoints > 0)
            theResult = true;
        else
            theResult = false;
        it++;
        if (!theResult)
            break;
    }
    if (theResult && outUnits.size() == 5)
        theResult = EEAreTilesLevel(theTiles);
    else {
        outUnits.clear();
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x610fe2
// Returns the first piece of a unit's linked chain (+0x1cc)
static EEComplexUnit* EEGetFirstPart(EEComplexUnit* inUnit)
{
    while (inUnit->mPreviousPart != NULL)
        inUnit = inUnit->mPreviousPart;
    return inUnit;
}

// FUNCTION: EXE 0x610ff5
// Returns the middle unit of a list (the lower of the two middles for an even count)
static EEUnit* EEGetMiddleOf(std::vector<EEUnit*>& inUnits)
{
    unsigned long theIndex = inUnits.size() / 2;
    if (inUnits.size() % 2 == 0)
        theIndex--;
    return inUnits[theIndex];
}

// FUNCTION: EXE 0x611028
// Turns gate piece inIndex for a gate in direction 1..4; returns its animation: 9 a post, 10 a diagonal post, 11 else
long EETurnGatePart(GETransformation& ioTransformation, long inDirection, long inIndex)
{
    long theResult = 11;
    switch (inIndex) {
    case 4:
        switch (inDirection) {
        case 4:
            ioTransformation.SetOrientationYPR(2.3561945f, 0.0f, 0.0f);
            theResult = 10;
            break;
        case 3:
            ioTransformation.SetOrientationYPR(-2.3561945f, 0.0f, 0.0f);
            theResult = 10;
            break;
        case 2:
            ioTransformation.SetOrientationYPR(3.1415927f, 0.0f, 0.0f);
            theResult = 9;
            break;
        case 1:
            ioTransformation.SetOrientationYPR(1.5707964f, 0.0f, 0.0f);
            theResult = 9;
            break;
        }
        break;
    case 0:
        switch (inDirection) {
        case 4:
            ioTransformation.SetOrientationYPR(-0.78539819f, 0.0f, 0.0f);
            theResult = 10;
            break;
        case 3:
            ioTransformation.SetOrientationYPR(0.78539819f, 0.0f, 0.0f);
            theResult = 10;
            break;
        case 2:
            ioTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f);
            theResult = 9;
            break;
        case 1:
            ioTransformation.SetOrientationYPR(-1.5707964f, 0.0f, 0.0f);
            theResult = 9;
            break;
        }
        break;
    case 1:
    case 2:
    case 3:
        theResult = 11;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x61112a
// Returns piece inIndex (0..4) of a gate, found along its linked chain (2 is the gate piece itself)
EEComplexUnit* EEUCGate::GetPart(EEComplexUnit* inGate, long inIndex)
{
    EEComplexUnit* theResult = NULL;
    switch (inIndex) {
    case 4:
        theResult = this->GetLastLinked(inGate);
        break;
    case 3:
        theResult = this->GetFourthPart(inGate);
        break;
    case 2:
        theResult = inGate;
        break;
    case 1:
        theResult = this->GetSecondPart(inGate);
        break;
    case 0:
        theResult = this->GetLastLinkedCarrier(inGate);
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x611171
// Lists the five tiles of a gate line through a tile: direction 1 along x, 2 along y, 3 and 4 the diagonals
void EEGetWallGateTiles(const U2DPoint<long>& inTile, long inDirection, std::vector<U2DPoint<long> >& outTiles)
{
    long x = inTile.mX;
    long y = inTile.mY;
    switch (inDirection) {
    case 1:
        outTiles.push_back(U2DPoint<long>(x - 2, y));
        outTiles.push_back(U2DPoint<long>(x - 1, y));
        outTiles.push_back(U2DPoint<long>(x, y));
        outTiles.push_back(U2DPoint<long>(x + 1, y));
        outTiles.push_back(U2DPoint<long>(x + 2, y));
        break;
    case 2:
        outTiles.push_back(U2DPoint<long>(x, y - 2));
        outTiles.push_back(U2DPoint<long>(x, y - 1));
        outTiles.push_back(U2DPoint<long>(x, y));
        outTiles.push_back(U2DPoint<long>(x, y + 1));
        outTiles.push_back(U2DPoint<long>(x, y + 2));
        break;
    case 3:
        outTiles.push_back(U2DPoint<long>(x + 2, y - 2));
        outTiles.push_back(U2DPoint<long>(x + 1, y - 1));
        outTiles.push_back(U2DPoint<long>(x, y));
        outTiles.push_back(U2DPoint<long>(x - 1, y + 1));
        outTiles.push_back(U2DPoint<long>(x - 2, y + 2));
        break;
    case 4:
        outTiles.push_back(U2DPoint<long>(x - 2, y - 2));
        outTiles.push_back(U2DPoint<long>(x - 1, y - 1));
        outTiles.push_back(U2DPoint<long>(x, y));
        outTiles.push_back(U2DPoint<long>(x + 1, y + 1));
        outTiles.push_back(U2DPoint<long>(x + 2, y + 2));
        break;
    }
}

// FUNCTION: EXE 0x611370
// Returns whether a gate piece is left out of the footprint: any piece but the posts while the gate is not closed
bool EEUCGate::IsPartLeftOut(EEComplexUnit* inUnit)
{
    return !this->IsSidePart(inUnit) && EEGetFirstPart(inUnit)->GetCommandType() != kActionClosed;
}

// FUNCTION: EXE 0x6113a9
// Returns whether a living enemy unit of unit set 0x62 is within reach of the gate or its next two pieces
// (the squared distance is compared with the unsquared reach)
bool EEUCGate::AreEnemiesNear(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    GE3DPoint thePosition;
    EEUnitSet* theSet = gUnitSetManager.GetSet(0x62);
    bool theFound = false;
    EEPlayer* thePlayer = inUnit->mPlayer;
    inUnit->GetPosition(thePosition);
    EEComplexUnit* theNext = inUnit->mNextPart;
    EEComplexUnit* theLast = theNext->mNextPart;
    EEGetUnitsNear(thePosition, kGateSearchRadius, theUnits);
    for (long i = theUnits.size() - 1; i >= 0 && !theFound; i--) {
        EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(theUnits[i]);
        if (!theSet->Contains(theUnit->mUnitClass->GetUnitType()) || !(theUnit->mHitPoints > 0) ||
            thePlayer == theUnit->mPlayer || theUnit->mPlayer->mEnemies[thePlayer->mPlayerID] == 0)
            continue;
        float theRange = theUnit->mUnitClass->mInnerCollisionSphere * 0.5f + kGateRangeBase;
        if (inUnit->DistanceSquaredTo(theUnit) <= theRange ||
            theNext->DistanceSquaredTo(theUnit) <= theRange ||
            theLast->DistanceSquaredTo(theUnit) <= theRange)
            theFound = true;
    }
    return theFound;
}

// FUNCTION: EXE 0x611506
// Returns whether a unit that may pass is within reach of the gate or its next two pieces: a mobile land unit
// the owner is not hostile to, or an enemy of unit set 0x62 (squared distance against the unsquared reach)
bool EEUCGate::IsPasserNear(EEComplexUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    GE3DPoint thePosition;
    EEUnitSet* theSet;
    theSet = gUnitSetManager.mSets.size() < 99 ? NULL : gUnitSetManager.mSets[0x62];
    EEPlayer* thePlayer = inUnit->GetPlayer();
    bool theFound = false;
    inUnit->GetPosition(thePosition);
    EEComplexUnit* theNext = inUnit->mNextPart;
    EEComplexUnit* theLast = theNext->mNextPart;
    EEGetUnitsNear(thePosition, kGateSearchRadius, theUnits);
    for (long i = theUnits.size() - 1; i >= 0 && !theFound; i--) {
        EEUnit* theUnit = theUnits[i];
        bool theFriend;
        if (theUnit->GetPlayer() == thePlayer || thePlayer->mEnemies[theUnit->GetPlayer()->mPlayerID] == 0)
            theFriend = true;
        else
            theFriend = false;
        if (theUnit->GetHitPoints() > 0) {
            bool thePasses;
            if (theFriend) {
                if (theUnit->GetStats()->mMaxVelocity <= 0.0f) {
                    theFound = false;
                    continue;
                }
                thePasses = (theUnit->GetUnitClass()->mDomains & kMovementLand) != 0;
            } else
                thePasses = theSet->Contains(theUnit->GetUnitClass()->GetUnitType());
            if (thePasses) {
                float theRange = theUnit->GetUnitClass()->mInnerCollisionSphere * 0.5f + kGateRangeBase;
                if (inUnit->DistanceSquaredTo(theUnit->GetComplexUnit()) <= theRange ||
                    theNext->DistanceSquaredTo(theUnit->GetComplexUnit()) <= theRange ||
                    theLast->DistanceSquaredTo(theUnit->GetComplexUnit()) <= theRange) {
                    theFound = true;
                    continue;
                }
            }
        }
        theFound = false;
    }
    return theFound;
}

// FUNCTION: EXE 0x6116a2
// Returns whether a tile holds a highlighted living wall that can take a gate, giving its gate direction
bool EEFindGateWall(const EETileCoordinate& inTile, long& ioDirection)
{
    bool theFound = false;
    EEMapTile* theTile = ::gWorld.GetMap()->GetTile(inTile.mX, inTile.mY);
    if (theTile != NULL) {
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL && !theFound; theNode = theNode->mNext) {
            EEUnit* theUnit = theNode->mUnit;
            if (theUnit != NULL && theUnit->mUnitClass->mIsWall && theUnit->mHitPoints > 0 && theUnit->mTileNode != NULL &&
                theUnit->GetComplexUnit()->mIsTinted) {
                theFound = true;
                ioDirection = theUnit->GetComplexUnit()->mGateDirection;
            }
        }
    }
    return theFound;
}

// FUNCTION: EXE 0x611723
// Returns the piece before the last one of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinkedCarrier(EEComplexUnit* inUnit)
{
    return this->GetLastLinked(inUnit)->mPreviousPart;
}

// FUNCTION: EXE 0x611735
// Returns gate piece 1 (the piece before piece 3); NULL when the chain is too short (a gate created as a single
// piece, e.g. by a scenario effect)
EEComplexUnit* EEUCGate::GetSecondPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theFourth = this->GetFourthPart(inGate);
    return theFourth != NULL ? theFourth->mPreviousPart : NULL;
}

// FUNCTION: EXE 0x611747
// Returns the last piece of a unit's linked chain
EEComplexUnit* EEUnitClass::GetLastLinked(EEComplexUnit* inUnit)
{
    while (inUnit->mNextPart != NULL)
        inUnit = inUnit->mNextPart;
    return inUnit;
}

// FUNCTION: EXE 0x61175c
// Returns gate piece 3 (two pieces before the end of the chain); NULL when the chain is too short (a gate created
// as a single piece, e.g. by a scenario effect)
EEComplexUnit* EEUCGate::GetFourthPart(EEComplexUnit* inGate)
{
    EEComplexUnit* theCarrier = this->GetLastLinkedCarrier(inGate);
    return theCarrier != NULL ? theCarrier->mPreviousPart : NULL;
}

// FUNCTION: EXE 0x61176e
// Restores the normal colour of a player's walls of a type and clears their gate direction (after gate placement)
void EEClearGateHighlights(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    EEGetPlayerUnitsOfType(inUnitType, inPlayer->mPlayerID, theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        EEComplexUnit* theUnit = (*it)->mComplexUnit;
        if (theUnit == NULL)
            continue;
        theUnit->SetColor(sGateNormal);
        theUnit->SetIsTinted(false);
        theUnit->SetGateDirection(0);
    }
}

// FUNCTION: EXE 0x6117f2
// Starts the next two pieces' open, closed, opening or closing animation (with the gate sound when moving);
// other actions go the building's way
void EEUCGate::ActionChanged(EEComplexUnit* inUnit)
{
    long theAction;
    switch (inUnit->mCommand ? inUnit->mCommand->mType : 0) {
    case kActionOpen:
        theAction = 15000;
        break;
    case kActionClosed:
        theAction = 15001;
        break;
    case kActionOpening: {
        EESoundObject* theSound = EESoundObject::Get(kSoundGate);
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            theSound->Play(theVolume, thePan, thePitch);
        }
        theAction = 15002;
        break;
    }
    case kActionClosing: {
        EESoundObject* theSound = EESoundObject::Get(kSoundGate);
        if (theSound != NULL) {
            float theVolume;
            float thePan;
            float thePitch;
            this->GetSoundParameters(inUnit, theSound, &theVolume, &thePan, &thePitch);
            theSound->Play(theVolume, thePan, thePitch);
        }
        theAction = 15003;
        break;
    }
    default:
        EEUCBuilding::ActionChanged(inUnit);
        return;
    }
    inUnit->mNextPart->StartAction(theAction);
    inUnit->mNextPart->mNextPart->StartAction(theAction);
}

// FUNCTION: EXE 0x611905
// Sets up a new gate piece the building's way; the chain's first piece starts closed
void EEUCGate::UnitCreated(EEComplexUnit* inUnit)
{
    EEUCBuilding::UnitCreated(inUnit);
    EEUCGate* theGate = dynamic_cast<EEUCGate*>(inUnit->GetUnitClass());
    if (inUnit->mPreviousPart == NULL && inUnit->mNextPart != NULL) {
        inUnit->StartAction(kActionClosed);
        this->ActionChanged(inUnit);
    }
}

// FUNCTION: EXE 0x611957
// Highlights the middle wall of a gate line through each of a player's walls of a type when one or three of the
// four directions fit (an XOR of the four tests); the first fitting direction is used
void EEHighlightGateWalls(EEPlayer* inPlayer, long inUnitType)
{
    std::vector<EEUnit*> theUnits;
    std::vector<EEUnit*> theSide1;
    std::vector<EEUnit*> theSide2;
    std::vector<EEUnit*> theSide3;
    std::vector<EEUnit*> theSide4;
    EEClearGateHighlights(inPlayer, inUnitType);
    EEGetPlayerUnitsOfType(inUnitType, inPlayer->mPlayerID, theUnits);
    for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end(); it++) {
        bool the1 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 1, inUnitType, theSide1);
        bool the2 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 2, inUnitType, theSide2);
        bool the3 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 3, inUnitType, theSide3);
        bool the4 = EEDoesWallGateFitOnUnit((EEComplexUnit*)*it, 4, inUnitType, theSide4);
        if (the1 ^ the2 ^ the3 ^ the4) {
            if (the1) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide1)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(1);
                }
            } else if (the2) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide2)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(2);
                }
            } else if (the3) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide3)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(3);
                }
            } else if (the4) {
                EEComplexUnit* theWall = EEGetMiddleOf(theSide4)->mComplexUnit;
                if (!theWall->mIsTinted) {
                    theWall->SetColor(sGateHighlight);
                    theWall->SetIsTinted(true);
                    theWall->SetGateDirection(4);
                }
            }
        }
        theSide1.clear();
        theSide2.clear();
        theSide3.clear();
        theSide4.clear();
    }
}

// FUNCTION: EXE 0x611c35
// Answers the gate's triggers: open, closed, locked, special (enemy) unit or any passer in opening range;
// others the building's way
long EEUCGate::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    long theResult = 0;
    bool theFlag = false;
    switch (inEvent) {
    case kTriggerIsOpen:
        theFlag = inUnit->mOpenFraction == kGateOpen;
        break;
    case kTriggerIsClosed:
        theFlag = inUnit->mOpenFraction == kGateClosed;
        break;
    case kTriggerLocked:
        theFlag = inUnit->mIsLocked;
        break;
    case kTriggerNotLocked:
        theFlag = !inUnit->mIsLocked;
        break;
    case kTriggerSpecialUnitInOpeningRange:
        theFlag = this->AreEnemiesNear(inUnit);
        break;
    case kTriggerSpecialUnitNotInOpeningRange:
        theFlag = !this->AreEnemiesNear(inUnit);
        break;
    case kTriggerUnitInOpeningRange:
        theFlag = this->IsPasserNear(inUnit);
        break;
    case kTriggerUnitNotInOpeningRange:
        theFlag = !this->IsPasserNear(inUnit);
        break;
    default:
        theResult = EEUCBuilding::EvaluateCondition(inUnit, inEvent);
    }
    if (theFlag)
        theResult = 1;
    return theResult;
}

// FUNCTION: EXE 0x611d00
// Returns whether all tiles are on the map, flat or ramps (0 or 2 raised corners), and differ in height by at most
// kGateMaximumSlope
// (EECPWallManager.cpp declares it the same way)
bool EEAreTilesLevel(std::vector<U2DPoint<long> >& inTiles)
{
    bool theResult = false;
    float theLowest = 100.0f;
    float theHighest = -100.0f;
    bool theOk = true;
    EEMap* theMap = ::gWorld.GetMap();
    for (std::vector<U2DPoint<long> >::iterator it = inTiles.begin(); theOk && it != inTiles.end(); it++) {
        long theRaised = theMap->GetNumRaisedCorners(it->GetX(), it->GetY());
        if (theMap->GetTileAtLocation(*it) != NULL) {
            float theZ = theMap->GetMinimumCornerZ(it->GetX(), it->GetY());
            if (theLowest >= theZ)
                theLowest = theZ;
            theZ = theMap->GetMaximumCornerZ(it->GetX(), it->GetY());
            if (theHighest <= theZ)
                theHighest = theZ;
            if (theRaised == 0 || theRaised == 2)
                theOk = true;
            else
                theOk = false;
        } else
            theOk = false;
    }
    if (theOk)
        theResult = theHighest - theLowest <= kGateMaximumSlope;
    return theResult;
}

// FUNCTION: EXE 0x611dc9
// Moves an opening or closing gate's doors 0.2 a tick (from the chain's first piece), then updates it as a building
void EEUCGate::Update(EEComplexUnit* inUnit)
{
    long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    if (inUnit->mPreviousPart == NULL && inUnit->mNextPart != NULL && (theCommand == kActionOpening || theCommand == kActionClosing)) {
        float theOpen = inUnit->mOpenFraction;
        if (theCommand == kActionOpening) {
            theOpen = theOpen + kGateStep;
            if (kGateOpen < theOpen)
                theOpen = kGateOpen;
        } else {
            theOpen = theOpen - kGateStep;
            if (kGateClosed > theOpen)
                theOpen = kGateClosed;
        }
        inUnit->SetOpenFraction(theOpen);
        this->GetLastLinkedCarrier(inUnit)->SetOpenFraction(theOpen);
        this->GetLastLinked(inUnit)->SetOpenFraction(theOpen);
    }
    EEUCBuilding::Update(inUnit);
}

// The template and inline COMDATs that close the unit (names paired by address range)
// TEMPLATE: EXE 0x611ebc
// TEMPLATE: EXE 0x611f07
// TEMPLATE: EXE 0x611f84
// TEMPLATE: EXE 0x612044
// TEMPLATE: EXE 0x6120b0
// TEMPLATE: EXE 0x6120d9
// TEMPLATE: EXE 0x6121b1
// TEMPLATE: EXE 0x6121ef
// TEMPLATE: EXE 0x61223e
// TEMPLATE: EXE 0x6122f0
// TEMPLATE: EXE 0x61232a
// TEMPLATE: EXE 0x61236b
// TEMPLATE: EXE 0x612473
// TEMPLATE: EXE 0x6124ab
// TEMPLATE: EXE 0x6124ed
// TEMPLATE: EXE 0x612546
// TEMPLATE: EXE 0x61256f
// TEMPLATE: EXE 0x6125b3
// TEMPLATE: EXE 0x61263d
// TEMPLATE: EXE 0x6126aa
// TEMPLATE: EXE 0x61270b
// TEMPLATE: EXE 0x612753
// TEMPLATE: EXE 0x6128e1
