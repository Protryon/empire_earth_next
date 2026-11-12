// C:\EEX\Empire Earth\Persistent\EEPWorldHelper.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPWorldHelper-..."). The saved state of the world: 0x14bd64..0x14c6b1. Unit flags: the exe's module set.
// Data: the vtable 0x4422bc; the 15 class static keys (guard bytes 0x531c58, 0x531bd0).
#include <windows.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/TMPoll.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIFormManager.h"
#include "lle/UIManager.h"
#include "lle/FSBasicFile.h"
#include "lle/TSScheduler.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameState.h"
#include "ee/EETeam.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEUnit.h"
#include "ee/EESaveGameFiles.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIForest.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEPathManager.h"
#include "ee/EEAEManager.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EECPExperimentTimers.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGGather.h"
#include "ee/EEUGAttackUnit.h"
#include <algorithm>
#include <vector>

const UString EEPWorldHelper::sTheScenarioManager("EEWorldHelper-The Scenario Manager");
const UString EEPWorldHelper::sTheTeams("EEPWorldHelper-The Teams");
const UString EEPWorldHelper::sTheNumberOfWondersToWin("EEPWorldHelper-The Number Of Wonders To Win");
const UString EEPWorldHelper::sTheTimeOfDay("EEPWorldHelper-The Time Of Day");
const UString EEPWorldHelper::sTheWaterLevel("EEPWorldHelper-The Water Level");
const UString EEPWorldHelper::sTheZoom("EEPWorldHelper-The Zoom");
const UString EEPWorldHelper::sThePopulationLimit("EEPWorldHelper-The Population Limit");
const UString EEPWorldHelper::sTheGameSpeed("EEPWorldHelper-The Game Speed");
const UString EEPWorldHelper::sTheVictoryEnabled("EEPWorldHelper-The Victory Enabled");
const UString EEPWorldHelper::sTheUnitImprovementsAllowed("EEPWorldHelper-The Unit Improvements Allowed");
const UString EEPWorldHelper::sTheTeamsLocked("EEPWorldHelper-The Teams Locked");
const UString EEPWorldHelper::sTheCheatCodesAllowed("EEPWorldHelper-The Cheat Codes Allowed");
const UString EEPWorldHelper::sTownIDCounter("EEPWorldHelper-TownID Counter");
const UString EEPWorldHelper::sHerdIDCounter("EEPWorldHelper-HerdID Counter");
const UString EEPWorldHelper::sUsingCarbon("EEPWorldHelper-Using Carbon");

static long LoadTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      std::vector<EETeam>& outTeams);
static long SaveTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                      std::vector<EETeam>& inTeams);

// FUNCTION: EXE 0x54c151
// Creates an empty saved-world chunk (no teams, improvements, team lock and cheats allowed)
EEPWorldHelper::EEPWorldHelper()
    : FSPersistent(kPersistentWorld)
{
    this->mScenarioManager = NULL;
    this->mTimeOfDay = 0.0f;
    this->mZoom = 0.0f;
    this->mGameSpeed = 0.0f;
    this->mPopulationLimit = 0;
    this->mNumberOfWondersToWin = 0;
    this->mC36 = 0;
    this->mVictoryEnabled = false;
    this->mTownIDCounter = 0;
    this->mHerdIDCounter = 0;
    this->mUsingCarbon = false;
    this->mUnitImprovementsAllowed = true;
    this->mTeamsLocked = true;
    this->mCheatsEnabled = true;
}

// SYNTHETIC: EXE 0x54c1bc
// (the scalar deleting destructor, emitted with the vtable; the implicit dtor inline)

// FUNCTION: EXE 0x54c1e2
// Creates the saved-world chunk from the world's teams, time of day, zoom, speed, limits, victory and cheat settings
// and id counters
EEPWorldHelper::EEPWorldHelper(EEWorld& inWorld)
    : FSPersistent(kPersistentWorld)
{
    this->mScenarioManager = NULL;
    this->mTimeOfDay = inWorld.GetTimeOfDay();
    this->mZoom = gWorld.mCameraControl.GetZoom();
    this->mGameSpeed = inWorld.GetGameSpeed();
    this->mPopulationLimit = inWorld.mPopulationLimit;
    this->mNumberOfWondersToWin = inWorld.mNumberOfWondersToWin;
    this->mC36 = 0;

    this->mTeams = inWorld.mTeams;
    this->mVictoryEnabled = inWorld.mVictoryEnabled;
    this->mUnitImprovementsAllowed = inWorld.mUnitImprovementsAllowed;
    this->mTeamsLocked = inWorld.mTeamsLocked;
    this->mCheatsEnabled = inWorld.mCheatsEnabled;
    this->mTownIDCounter = inWorld.mTownIDCounter;
    this->mHerdIDCounter = inWorld.mHerdIDCounter;
    this->mUsingCarbon = inWorld.mUsingCarbon;
}

// FUNCTION: EXE 0x54c2a6
// Creates an empty saved-world chunk (persistent factory)
FSPersistent* EEPWorldHelper::CreateDefault() const
{
    return new EEPWorldHelper;
}

// port (save version 0.254): the order of the world's unit chain (x-sorted; units on one column in the order they
// came) and of the units stepped each tick, and the queued (de)activations, as unit ids. A load links the units in
// its own order and rebuilds the stepped list from the chain, so a loaded game stepped its units (and drew its random
// numbers) in another order than the game that was saved. Put back at the first tick (EEAfterLoad), when every unit
// of the load is in place; left as the load made it when the units are not the same.
static void SaveUnitOrder(FSFile& outFile)
{
    std::vector<unsigned long> theIDs;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext)
        theIDs.push_back(theUnit->mUnitID);
    long theResult = outFile.Write((unsigned long)theIDs.size());
    for (size_t i = 0; i < theIDs.size() && theResult == kErrorOK; i++)
        theResult = outFile.Write(theIDs[i]);
    const std::deque<EEUnitHandle<EEComplexUnit> >* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
    for (int l = 0; l < 3 && theResult == kErrorOK; l++) {
        theResult = outFile.Write((unsigned long)theLists[l]->size());
        for (size_t i = 0; i < theLists[l]->size() && theResult == kErrorOK; i++)
            theResult = outFile.Write((*theLists[l])[i]->mUnitID);
    }
}

static EEComplexUnit* LoadedComplexUnit(unsigned long inID)
{
    return dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(inID)));
}

static void RestoreUnitOrder(const std::vector<unsigned long>& inChain, const std::vector<unsigned long> inLists[3])
{
    // the chain: the same units, each at the same tile column as its neighbours allow (x-sorted)
    std::vector<RTSUnit*> theUnits;
    size_t theCount = 0;
    for (RTSUnit* theUnit = gWorld.mUnits.mFirst; theUnit != NULL; theUnit = theUnit->mTileNode->mNext)
        theCount++;
    bool theSame = theCount == inChain.size();
    for (size_t i = 0; i < inChain.size() && theSame; i++) {
        RTSUnit* theUnit = gWorld.GetUnit(inChain[i]);
        theSame = theUnit != NULL && theUnit->mTileNode != NULL && (theUnit->mTileNode->mPrev != NULL || gWorld.mUnits.mFirst == theUnit)
            && (theUnits.empty() || theUnits.back()->mTileX <= theUnit->mTileX);
        theUnits.push_back(theUnit);
    }
    if (theSame) {
        RTSUnit* thePrev = reinterpret_cast<RTSUnit*>(&gWorld.mUnits);
        for (size_t i = 0; i < theUnits.size(); i++) {
            theUnits[i]->Link(thePrev);
            thePrev = theUnits[i];
        }
    } else
        ESDebugOut("EEPWorldHelper: the loaded units are not the saved ones: their order is the load's\n");
    // the stepped units and the queues: the saved units in the saved order. The load's RebuildActiveUnits steps
    // every placed unit, where the saved game had some still queued for activation (a unit made in the save's
    // last tick is stepped from the next tick on) or deactivated since, so the sets differ as well as the order;
    // the lists are made the saved ones, with the references UpdateActiveUnits keeps (two per stepped unit, in
    // mActiveUnits too; one per queued unit). A saved unit the load does not have is left out.
    std::deque<EEUnitHandle<EEComplexUnit> >* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
    for (int l = 0; l < 3; l++) {
        std::deque<EEUnitHandle<EEComplexUnit> > theOrder;
        for (size_t i = 0; i < inLists[l].size(); i++) {
            EEComplexUnit* theUnit = LoadedComplexUnit(inLists[l][i]);
            if (theUnit != NULL && std::find(theOrder.begin(), theOrder.end(), theUnit) == theOrder.end())
                theOrder.push_back(theUnit);
            else if (theUnit == NULL)
                ESDebugOut("EEPWorldHelper: saved stepped unit %lu is not in the loaded game\n", inLists[l][i]);
        }
        std::deque<EEUnitHandle<EEComplexUnit> >& theList = *theLists[l];
        for (size_t i = 0; i < theList.size(); i++) {   // the load's units the saved game did not have
            EEComplexUnit* theUnit = theList[i];
            if (std::find(theOrder.begin(), theOrder.end(), theUnit) != theOrder.end())
                continue;
            theUnit->Dereference();
            if (l == 0) {
                theUnit->Dereference();
                gWorld.mActiveUnits.erase(theUnit->mUnitID);
            }
        }
        for (size_t i = 0; i < theOrder.size(); i++) {   // the saved units the load did not have
            EEComplexUnit* theUnit = theOrder[i];
            if (std::find(theList.begin(), theList.end(), theUnit) != theList.end())
                continue;
            theUnit->Reference();
            if (l == 0) {
                theUnit->Reference();
                gWorld.mActiveUnits[theUnit->mUnitID] = theUnit;
            }
        }
        theList = theOrder;
    }
}

// port (save version 0.254): each forest's next renewal (EEUnit::RegrowForest), with its tiles. The continent manager
// is not saved: a load builds its forests again from the map with no renewal due date, so the first tree cut in a
// loaded game regrew its forest at once. Put back at the first tick to the rebuilt forest on each saved forest's tiles.
static void SaveForests(FSFile& outFile)
{
    unsigned long theCount = 0;
    for (size_t f = 0; f < gContinentManager.mForests.size(); f++)
        if (gContinentManager.mForests[f] != NULL && gContinentManager.mForests[f]->mNextRenewal != 0)
            theCount++;
    long theResult = outFile.Write(theCount);
    for (size_t f = 0; f < gContinentManager.mForests.size() && theResult == kErrorOK; f++) {
        EEAIForest* theForest = gContinentManager.mForests[f];
        if (theForest == NULL || theForest->mNextRenewal == 0)
            continue;
        std::vector<U2DPoint<long> > theTiles;
        theForest->mTiles.BuildPointList(theTiles, 1);
        theResult = outFile.Write(theForest->mNextRenewal);
        if (theResult == kErrorOK)
            theResult = outFile.Write((unsigned long)theTiles.size());
        for (size_t t = 0; t < theTiles.size() && theResult == kErrorOK; t++) {
            theResult = outFile.Write(theTiles[t].mX);
            if (theResult == kErrorOK)
                theResult = outFile.Write(theTiles[t].mY);
        }
    }
}

static long LoadForests(FSFile& inFile)
{
    std::vector<std::pair<unsigned long, std::vector<U2DPoint<long> > > > theForests;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long f = 0; f < theCount && theResult == kErrorOK; f++) {
        unsigned long theRenewal = 0;
        unsigned long theTiles = 0;
        theResult = inFile.Read(theRenewal);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theTiles);
        std::vector<U2DPoint<long> > theList;
        for (unsigned long t = 0; t < theTiles && theResult == kErrorOK; t++) {
            U2DPoint<long> theTile;
            theResult = inFile.Read(theTile.mX);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theTile.mY);
            theList.push_back(theTile);
        }
        theForests.push_back(std::make_pair(theRenewal, theList));
    }
    if (theResult == kErrorOK)
        EEAfterLoad([theForests]() {
            bool theSame = true;
            for (size_t f = 0; f < theForests.size(); f++) {
                std::vector<EEAIForest*> theDone;
                for (size_t t = 0; t < theForests[f].second.size(); t++) {
                    EEAIForest* theForest = gContinentManager.GetForestAt(theForests[f].second[t]);
                    if (theForest == NULL || std::find(theDone.begin(), theDone.end(), theForest) != theDone.end())
                        continue;
                    theDone.push_back(theForest);
                    theForest->mNextRenewal = theForests[f].first;
                    theSame = theSame && (unsigned long)theForest->mTiles.Size() == theForests[f].second.size();
                }
            }
            if (!theSame)
                ESDebugOut("EEPWorldHelper: a rebuilt forest is not the saved one: its renewal is the saved forest's\n");
        });
    return theResult;
}

// port (save version 0.256): the computer players' round-robin schedule (EEArtificialIntelligence::Update: which
// computer player updates next and when). A load's EEWorld::Restore calls CollectComputerPlayers, which starts the
// round again at the first player at time 0, so a loaded game ran a computer player's Update at its first tick where
// the saved game had it a few ticks later (the first divergent draw of every save: EEComputerPlayer::Update's). Put
// back at the first tick; the interval (1000 ms / players) is the same on both sides.
static void SaveAISchedule(FSFile& outFile)
{
    outFile.Write(gArtificialIntelligence->mNextPlayer);
    outFile.Write(gArtificialIntelligence->mNextUpdateTime);
}

static long LoadAISchedule(FSFile& inFile)
{
    unsigned long theNextPlayer = 0;
    unsigned long theNextUpdateTime = 0;
    long theResult = inFile.Read(theNextPlayer);
    if (theResult == kErrorOK)
        theResult = inFile.Read(theNextUpdateTime);
    if (theResult == kErrorOK)
        EEAfterLoad([theNextPlayer, theNextUpdateTime]() {
            if (theNextPlayer < gArtificialIntelligence->mComputerPlayers.size()) {
                gArtificialIntelligence->mNextPlayer = theNextPlayer;
                gArtificialIntelligence->mNextUpdateTime = theNextUpdateTime;
            } else
                ESDebugOut("EEPWorldHelper: the loaded computer players are not the saved ones: their schedule is the load's\n");
        });
    return theResult;
}

// port (save version 0.256): the order of the units on each tile (RTSTile::mUnits, newest first). A load places the
// units in its own order, so a tile with several units listed them otherwise, and EEComplexUnit::FindContacts, which
// walks the tiles' lists, built the contact lists in another order (collisions resolved in another order). Saved:
// every tile with two units or more, by index, with its units' ids; put back at the first tick by moving the units
// between the tile's own nodes (each node keeps its reference) when the tile holds the same units.
static void SaveTileOrder(FSFile& outFile)
{
    EEMap* theMap = gWorld.mMap;
    std::vector<std::pair<unsigned long, std::vector<unsigned long> > > theTiles;
    long theSize = theMap != NULL ? theMap->mSize : 0;
    for (long i = 0; i < theSize * theSize; i++) {
        EEMapTile* theTile = theMap->mTiles[i];
        if (theTile == NULL || theTile->mUnits == NULL || theTile->mUnits->mNext == NULL)
            continue;
        std::vector<unsigned long> theIDs;
        for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext)
            theIDs.push_back(theNode->mUnit != NULL ? (unsigned long)theNode->mUnit->mUnitID : (unsigned long)-1);
        theTiles.push_back(std::make_pair((unsigned long)i, theIDs));
    }
    outFile.Write((unsigned long)theTiles.size());
    for (size_t t = 0; t < theTiles.size(); t++) {
        outFile.Write(theTiles[t].first);
        outFile.Write((unsigned long)theTiles[t].second.size());
        for (size_t k = 0; k < theTiles[t].second.size(); k++)
            outFile.Write(theTiles[t].second[k]);
    }
}

static void RestoreTileOrder(const std::vector<std::pair<unsigned long, std::vector<unsigned long> > >& inTiles)
{
    EEMap* theMap = gWorld.mMap;
    unsigned long theOther = 0;
    for (size_t t = 0; t < inTiles.size(); t++) {
        if (theMap == NULL || inTiles[t].first >= (unsigned long)(theMap->mSize * theMap->mSize))
            break;
        EEMapTile* theTile = theMap->mTiles[inTiles[t].first];
        const std::vector<unsigned long>& theIDs = inTiles[t].second;
        std::vector<RTSTileUnitNode*> theNodes;
        for (RTSTileUnitNode* theNode = theTile != NULL ? theTile->mUnits : NULL; theNode != NULL; theNode = theNode->mNext)
            theNodes.push_back(theNode);
        std::vector<EEUnit*> theUnits;
        for (size_t k = 0; k < theIDs.size() && theNodes.size() == theIDs.size(); k++) {
            EEUnit* theUnit = NULL;
            for (size_t n = 0; n < theNodes.size() && theUnit == NULL; n++)
                if (theNodes[n]->mUnit != NULL && (unsigned long)theNodes[n]->mUnit->mUnitID == theIDs[k]
                    && std::find(theUnits.begin(), theUnits.end(), theNodes[n]->mUnit) == theUnits.end())
                    theUnit = theNodes[n]->mUnit;
            if (theUnit == NULL)
                break;
            theUnits.push_back(theUnit);
        }
        if (theUnits.size() != theNodes.size() || theUnits.size() != theIDs.size()) {
            theOther++;
            continue;
        }
        for (size_t n = 0; n < theNodes.size(); n++)
            theNodes[n]->mUnit = theUnits[n];
    }
    if (theOther != 0)
        ESDebugOut("EEPWorldHelper: %lu tiles hold other units than the saved ones: their order is the load's\n", theOther);
}

static long LoadTileOrder(FSFile& inFile)
{
    std::vector<std::pair<unsigned long, std::vector<unsigned long> > > theTiles;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long t = 0; t < theCount && theResult == kErrorOK; t++) {
        unsigned long theIndex = 0;
        unsigned long theUnits = 0;
        theResult = inFile.Read(theIndex);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theUnits);
        std::vector<unsigned long> theIDs;
        for (unsigned long k = 0; k < theUnits && theResult == kErrorOK; k++) {
            unsigned long theID = 0;
            theResult = inFile.Read(theID);
            theIDs.push_back(theID);
        }
        theTiles.push_back(std::make_pair(theIndex, theIDs));
    }
    if (theResult == kErrorOK)
        EEAfterLoad([theTiles]() { RestoreTileOrder(theTiles); });
    return theResult;
}

// port (save version 0.257): the order of the units in each map region (EEMap::mUnitRegions, RTSRegionTable: a unit
// is appended when it enters a region and erased when it leaves). A load adds the units in its own order, and
// EEUnitClass::FindImpactUnit and the target searches walk a region's units in this order, taking the first match
// (seed 3's 600 s save: a projectile hit another unit in the loaded game). Saved: every region with two units or more,
// by index, with its units' ids; put back at the first tick when the region holds the same units.
static void SaveRegionOrder(FSFile& outFile)
{
    RTSRegionTable<EEComplexUnit*>* theTable = gWorld.mMap != NULL ? gWorld.mMap->mUnitRegions : NULL;
    unsigned long theRegions = theTable != NULL ? theTable->mRegionsX * theTable->mRegionsY : 0;
    unsigned long theCount = 0;
    for (unsigned long r = 0; r < theRegions; r++)
        theCount += theTable->mRegions[r].size() >= 2 ? 1 : 0;
    outFile.Write(theCount);
    for (unsigned long r = 0; r < theRegions; r++) {
        RTSRegion<EEComplexUnit*>& theRegion = theTable->mRegions[r];
        if (theRegion.size() < 2)
            continue;
        outFile.Write(r);
        outFile.Write((unsigned long)theRegion.size());
        for (size_t k = 0; k < theRegion.size(); k++)
            outFile.Write(theRegion[k] != NULL ? (unsigned long)theRegion[k]->mUnitID : (unsigned long)-1);
    }
}

static void RestoreRegionOrder(const std::vector<std::pair<unsigned long, std::vector<unsigned long> > >& inRegions)
{
    RTSRegionTable<EEComplexUnit*>* theTable = gWorld.mMap != NULL ? gWorld.mMap->mUnitRegions : NULL;
    unsigned long theOther = 0;
    for (size_t t = 0; t < inRegions.size(); t++) {
        if (theTable == NULL || inRegions[t].first >= theTable->mRegionsX * theTable->mRegionsY)
            break;
        RTSRegion<EEComplexUnit*>& theRegion = theTable->mRegions[inRegions[t].first];
        const std::vector<unsigned long>& theIDs = inRegions[t].second;
        std::vector<EEComplexUnit*> theUnits;
        std::vector<bool> theTaken(theRegion.size(), false);
        for (size_t k = 0; k < theIDs.size() && theRegion.size() == theIDs.size(); k++) {
            size_t n = 0;
            while (n < theRegion.size() && (theTaken[n] || theRegion[n] == NULL || (unsigned long)theRegion[n]->mUnitID != theIDs[k]))
                n++;
            if (n == theRegion.size())
                break;
            theTaken[n] = true;
            theUnits.push_back(theRegion[n]);
        }
        if (theUnits.size() != theRegion.size() || theUnits.size() != theIDs.size()) {
            theOther++;
            continue;
        }
        for (size_t n = 0; n < theUnits.size(); n++)
            theRegion[n] = theUnits[n];
    }
    if (theOther != 0)
        ESDebugOut("EEPWorldHelper: %lu map regions hold other units than the saved ones: their order is the load's\n", theOther);
}

static long LoadRegionOrder(FSFile& inFile)
{
    std::vector<std::pair<unsigned long, std::vector<unsigned long> > > theRegions;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long t = 0; t < theCount && theResult == kErrorOK; t++) {
        unsigned long theIndex = 0;
        unsigned long theUnits = 0;
        theResult = inFile.Read(theIndex);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theUnits);
        std::vector<unsigned long> theIDs;
        for (unsigned long k = 0; k < theUnits && theResult == kErrorOK; k++) {
            unsigned long theID = 0;
            theResult = inFile.Read(theID);
            theIDs.push_back(theID);
        }
        theRegions.push_back(std::make_pair(theIndex, theIDs));
    }
    if (theResult == kErrorOK)
        EEAfterLoad([theRegions]() { RestoreRegionOrder(theRegions); });
    return theResult;
}

// port (save version 0.262): the dead attackers. A unit's mLastAttacker holds a reference, so the running game keeps
// an attacker that died since as a unit out of the world, and reads it: the triggers (AttackerIsNotBuilding,
// AttackerIsReachable, CanDamageAttacker read its class and tile), the AI (a raider's tile, a transport's alarm), a
// predator's strike back. The save wrote its id and the load found no unit, so the loaded game read NULL (seed 4's
// 2100 s save: a gatherer of player 3 kept its iron in the loaded game only). Saved: each such unit once, with what
// those readers read (class, player, hit points, tile, built flag, placement); a load makes it again as a unit out of
// the world (EEFindLoadedGhost, for EEComplexUnit::RepairPointers), whose references then keep it as before.
struct EESavedGhost {
    unsigned long mID;
    unsigned long mType;
    unsigned long mPlayer;
    long mHitPoints;
    long mTileX;
    long mTileY;
    bool mIsBuilt;
    float mPlacement[4][4];
    unsigned long mResourceType;   // 0.272 (EESaveHasGhostResources)
    long mResourceAmount;
    bool mIsComplex;               // 0.275 (EESaveHasGoalGhosts): false for a simple unit (a tree, a mine)
};
static std::vector<EESavedGhost> sGhosts;            // the loaded records, until the first tick
static std::vector<EEUnit*> sGhostUnits;             // made from them, one reference each until the first tick

// 0.275: a unit a goal holds out of the world, once (simple units too: a gathered tree or mine gone)
static void AddHeldGhost(EEUnit* inUnit, std::vector<EEComplexUnit*>& ioComplex, std::vector<EEUnit*>& ioSimple)
{
    if (inUnit == NULL || gWorld.GetUnit(inUnit->mUnitID) == inUnit)
        return;
    EEComplexUnit* theComplex = dynamic_cast<EEComplexUnit*>(inUnit);   // (a removed unit's store fields read 0)
    if (theComplex != NULL) {
        if (std::find(ioComplex.begin(), ioComplex.end(), theComplex) == ioComplex.end())
            ioComplex.push_back(theComplex);
    } else if (std::find(ioSimple.begin(), ioSimple.end(), inUnit) == ioSimple.end())
        ioSimple.push_back(inUnit);
}

static void SaveGhosts(FSFile& outFile)
{
    std::vector<EEComplexUnit*> theDead;
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        for (EEUnit* theUnit = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theAttacker = theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->mLastAttacker : NULL;
            if (theAttacker == NULL || gWorld.GetUnit(theAttacker->mUnitID) == theAttacker
                || std::find(theDead.begin(), theDead.end(), theAttacker) != theDead.end())
                continue;
            theDead.push_back(theAttacker);
        }
    }
    EECPOutOfWorldGatherers(theDead);   // the dead units the gatherer lists still hold
    // 0.272: the resources and drop sites the gather goals hold out of the world (EEUGGather::RepairPointers); 0.275
    // also the simple ones (a tree or mine used up) and the attack goals' targets (EEUGAttackUnit::RepairPointers)
    std::vector<EEUnit*> theSimple;
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        for (EEUnit* theUnit = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex == NULL)
                continue;
            for (size_t q = 0; q < theComplex->mGoalQueues.size(); q++) {
                std::deque<EEUnitGoal*>& theGoals = theComplex->mGoalQueues[q].mGoals;
                for (size_t g = 0; g < theGoals.size(); g++) {
                    if (EEUGAttackUnit* theAttack = dynamic_cast<EEUGAttackUnit*>(theGoals[g])) {
                        AddHeldGhost(theAttack->mTarget, theDead, theSimple);
                        continue;
                    }
                    EEUGGather* theGather = dynamic_cast<EEUGGather*>(theGoals[g]);
                    if (theGather == NULL)
                        continue;
                    AddHeldGhost(theGather->mResource, theDead, theSimple);
                    AddHeldGhost(theGather->mStorage, theDead, theSimple);
                }
            }
        }
    }
    std::vector<EEUnit*> theAll(theDead.begin(), theDead.end());
    theAll.insert(theAll.end(), theSimple.begin(), theSimple.end());
    outFile.Write((unsigned long)theAll.size());
    for (size_t i = 0; i < theAll.size(); i++) {
        EEUnit* theUnit = theAll[i];
        outFile.Write((unsigned long)theUnit->mUnitID);
        outFile.Write(theUnit->mUnitClass != NULL ? (unsigned long)theUnit->mUnitClass->mUnitType : (unsigned long)-1);
        outFile.Write(theUnit->mPlayer != NULL ? (unsigned long)theUnit->mPlayer->mPlayerID : (unsigned long)-1);
        outFile.Write(theUnit->mHitPoints);
        outFile.Write(theUnit->mTileX);
        outFile.Write(theUnit->mTileY);
        EEComplexUnit* theComplex = i < theDead.size() ? theDead[i] : NULL;   // (a simple unit: no placement, not built)
        outFile.Write(theComplex != NULL && theComplex->mIsBuilt);
        GETransformation thePlacement;
        memset(thePlacement.mMatrix, 0, sizeof(thePlacement.mMatrix));
        if (theComplex != NULL)
            thePlacement = theComplex->mPhysicalModel.GetLocation();
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                outFile.Write(thePlacement.mMatrix[r][c]);
        outFile.Write((unsigned long)theUnit->mResourceType);   // 0.272
        outFile.Write((long)theUnit->mResourceAmount);
        outFile.Write(i < theDead.size());   // 0.275: complex
    }
}

// inHasResources: the save has the 0.272 fields (EESaveHasGhostResources); inHasKinds the 0.275 one (EESaveHasGoalGhosts)
static long LoadGhosts(FSFile& inFile, bool inHasResources, bool inHasKinds)
{
    std::vector<EESavedGhost> theGhosts;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        EESavedGhost theGhost;
        memset(&theGhost, 0, sizeof(theGhost));
        if ((theResult = inFile.Read(theGhost.mID)) == kErrorOK && (theResult = inFile.Read(theGhost.mType)) == kErrorOK
            && (theResult = inFile.Read(theGhost.mPlayer)) == kErrorOK && (theResult = inFile.Read(theGhost.mHitPoints)) == kErrorOK
            && (theResult = inFile.Read(theGhost.mTileX)) == kErrorOK && (theResult = inFile.Read(theGhost.mTileY)) == kErrorOK)
            theResult = inFile.Read(theGhost.mIsBuilt);
        for (int r = 0; r < 4 && theResult == kErrorOK; r++)
            for (int c = 0; c < 4 && theResult == kErrorOK; c++)
                theResult = inFile.Read(theGhost.mPlacement[r][c]);
        theGhost.mResourceType = (unsigned long)-1;   // before 0.272: the class's own
        if (inHasResources && theResult == kErrorOK && (theResult = inFile.Read(theGhost.mResourceType)) == kErrorOK)
            theResult = inFile.Read(theGhost.mResourceAmount);
        theGhost.mIsComplex = true;   // before 0.275: complex units only
        if (inHasKinds && theResult == kErrorOK)
            theResult = inFile.Read(theGhost.mIsComplex);
        theGhosts.push_back(theGhost);
    }
    if (theResult == kErrorOK) {
        sGhosts = theGhosts;
        EEAfterLoad([]() {   // the references the loaded units took keep the ghosts from here on
            std::vector<EEUnit*> theUnits;
            theUnits.swap(sGhostUnits);
            for (size_t i = 0; i < theUnits.size(); i++)
                theUnits[i]->Dereference();
            sGhosts.clear();
        });
    }
    return theResult;
}

EEComplexUnit* EEFindLoadedGhost(unsigned long inID)
{
    return dynamic_cast<EEComplexUnit*>(EEFindLoadedGhostUnit(inID));
}

EEUnit* EEFindLoadedGhostUnit(unsigned long inID)
{
    for (size_t i = 0; i < sGhostUnits.size(); i++)
        if (sGhostUnits[i]->mUnitID == inID)
            return sGhostUnits[i];
    for (size_t i = 0; i < sGhosts.size(); i++) {
        const EESavedGhost& theGhost = sGhosts[i];
        if (theGhost.mID != inID)
            continue;
        EEUnitClass* theClass = theGhost.mType < EEUnitClassCount() ? gUnitClassTable[theGhost.mType] : NULL;
        EEPlayer* thePlayer = theGhost.mPlayer < gWorld.mNumPlayers ? gWorld.mPlayers[theGhost.mPlayer] : NULL;
        if (theClass == NULL || thePlayer == NULL)
            return NULL;
        EEUnit* theUnit;   // one reference, ours until the first tick
        if (theGhost.mIsComplex) {
            EEComplexUnit* theComplex = EEComplexUnit::Create();
            theComplex->EEUnit::Init(theClass, thePlayer);
            theComplex->mIsBuilt = theGhost.mIsBuilt;
            GETransformation thePlacement = theComplex->mPhysicalModel.GetLocation();
            memcpy(thePlacement.mMatrix, theGhost.mPlacement, sizeof(thePlacement.mMatrix));
            theComplex->mPhysicalModel.SetLocation(thePlacement);
            theUnit = theComplex;
        } else {   // 0.275: a simple unit, off the map as the load makes one (EEPlayer's Load: tile -10000)
            theUnit = EEUnit::Create();
            theUnit->Init(theClass, -10000, -10000, thePlayer);
        }
        theUnit->mUnitID = theGhost.mID;
        theUnit->mHitPoints = theGhost.mHitPoints;
        theUnit->mTileX = theGhost.mTileX;
        theUnit->mTileY = theGhost.mTileY;
        // a used-up simple resource keeps its tile pointer, off the tile's lists (the gather goal's ResourceAccessible
        // reads its terrain), as MoveTo sets it without adding the unit to the tile
        if (!theGhost.mIsComplex && gWorld.mMap != NULL)
            theUnit->mTile = gWorld.mMap->GetTile(theGhost.mTileX, theGhost.mTileY);
        if (theGhost.mResourceType != (unsigned long)-1) {   // 0.272
            theUnit->mResourceType = theGhost.mResourceType;
            theUnit->mResourceAmount = theGhost.mResourceAmount;
        }
        theUnit->mIsValid = false;     // out of the world, as the dead unit was (FreeNode asserts it)
        RTSRegisterUnit(theUnit);      // the running game's handles still resolve a dead unit's id until its last reference
        sGhostUnits.push_back(theUnit);
        return theUnit;
    }
    return NULL;
}

static long LoadUnitOrder(FSFile& inFile)
{
    std::vector<unsigned long> theChain;
    std::vector<unsigned long> theLists[3];
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theID = 0;
        theResult = inFile.Read(theID);
        theChain.push_back(theID);
    }
    for (int l = 0; l < 3 && theResult == kErrorOK; l++) {
        theCount = 0;
        theResult = inFile.Read(theCount);
        for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
            unsigned long theID = 0;
            theResult = inFile.Read(theID);
            theLists[l].push_back(theID);
        }
    }
    if (theResult == kErrorOK)
        EEAfterLoad([theChain, theLists]() { RestoreUnitOrder(theChain, theLists); });
    return theResult;
}

// port (save version 0.270): the order of each player's unit list (RTSPlayer::mFirstUnit, mNext). InsertUnit puts a
// unit after the first of its id's hash bucket, or at the end when the bucket is empty, so the running list holds the
// buckets in the order they last filled; SaveUnits writes them in bucket index order and the load inserts them in
// that order. Every walk of a player's units (the militia's raiders, the AI's groupings) then saw another order.
// Put back at the first tick.
static void SavePlayerUnitOrder(FSFile& outFile)
{
    long theResult = outFile.Write((unsigned long)gWorld.mNumPlayers);
    for (unsigned long p = 0; p < gWorld.mNumPlayers && theResult == kErrorOK; p++) {
        std::vector<unsigned long> theIDs;
        for (EEUnit* theUnit = gWorld.mPlayers[p] != NULL ? gWorld.mPlayers[p]->mFirstUnit : NULL; theUnit != NULL;
             theUnit = static_cast<EEUnit*>(theUnit->mNext))
            theIDs.push_back(theUnit->mUnitID);
        theResult = outFile.Write((unsigned long)theIDs.size());
        for (size_t i = 0; i < theIDs.size() && theResult == kErrorOK; i++)
            theResult = outFile.Write(theIDs[i]);
    }
}

// The player's units in the saved order, then any unit the load has that the save did not (after the last unit of
// its bucket, which keeps every bucket's units together as GetUnit walks them); each bucket's first unit follows
static void RestorePlayerUnitOrder(unsigned long inPlayer, const std::vector<unsigned long>& inIDs)
{
    EEPlayer* thePlayer = inPlayer < gWorld.mNumPlayers ? gWorld.mPlayers[inPlayer] : NULL;
    if (thePlayer == NULL)
        return;
    std::vector<RTSUnit*> theCurrent;
    for (RTSUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = theUnit->mNext)
        theCurrent.push_back(theUnit);
    std::vector<RTSUnit*> theOrder;
    for (size_t i = 0; i < inIDs.size(); i++) {
        RTSUnit* theUnit = thePlayer->GetUnit(inIDs[i]);
        if (theUnit != NULL && std::find(theOrder.begin(), theOrder.end(), theUnit) == theOrder.end())
            theOrder.push_back(theUnit);
    }
    for (size_t c = 0; c < theCurrent.size(); c++) {
        RTSUnit* theUnit = theCurrent[c];
        if (std::find(theOrder.begin(), theOrder.end(), theUnit) != theOrder.end())
            continue;
        size_t theAt = theOrder.size();
        for (size_t k = theOrder.size(); k > 0; k--)
            if ((theOrder[k - 1]->mUnitID & 0x7f) == (theUnit->mUnitID & 0x7f)) {
                theAt = k;
                break;
            }
        theOrder.insert(theOrder.begin() + theAt, theUnit);
    }
    if (theOrder.size() != theCurrent.size()) {   // a saved unit in another player's list: left as the load made it
        ESDebugOut("EEPWorldHelper: player %lu's saved units are not its list's: their order is the load's\n", inPlayer);
        return;
    }
    if (theOrder == theCurrent)
        return;
    for (size_t i = 0; i < theOrder.size(); i++) {
        theOrder[i]->mPrevious = i > 0 ? theOrder[i - 1] : NULL;
        theOrder[i]->mNext = i + 1 < theOrder.size() ? theOrder[i + 1] : NULL;
    }
    thePlayer->mFirstUnit = theOrder.empty() ? NULL : static_cast<EEUnit*>(theOrder.front());
    thePlayer->mLastUnit = theOrder.empty() ? NULL : static_cast<EEUnit*>(theOrder.back());
    bool theSeen[0x80] = {};
    for (size_t i = 0; i < theOrder.size(); i++) {
        unsigned long theBucket = theOrder[i]->mUnitID & 0x7f;
        if (!theSeen[theBucket])
            thePlayer->mUnitBuckets[theBucket].mFirst = static_cast<EEUnit*>(theOrder[i]);
        theSeen[theBucket] = true;
    }
}

static long LoadPlayerUnitOrder(FSFile& inFile)
{
    unsigned long thePlayers = 0;
    std::vector<std::vector<unsigned long> > theOrders;
    long theResult = inFile.Read(thePlayers);
    for (unsigned long p = 0; p < thePlayers && theResult == kErrorOK; p++) {
        unsigned long theCount = 0;
        theOrders.push_back(std::vector<unsigned long>());
        theResult = inFile.Read(theCount);
        for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
            unsigned long theID = 0;
            theResult = inFile.Read(theID);
            theOrders.back().push_back(theID);
        }
    }
    if (theResult == kErrorOK)
        EEAfterLoad([theOrders]() {
            for (size_t p = 0; p < theOrders.size(); p++)
                RestorePlayerUnitOrder((unsigned long)p, theOrders[p]);
        });
    return theResult;
}

// FUNCTION: EXE 0x54c2bd
// Reads the teams, world settings and id counters (Carbon flag from version 0.239 on), then the chat log and
// the main game form; returns only the main game form's error
long EEPWorldHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    LoadTeams(inMajorVersion, inMinorVersion, inFile, this->mTeams);
    inFile.Read(this->mNumberOfWondersToWin);
    inFile.Read(this->mTimeOfDay);
    inFile.Read(this->mC36);
    inFile.Read(this->mZoom);
    inFile.Read(this->mPopulationLimit);
    inFile.Read(this->mGameSpeed);
    inFile.Read(this->mVictoryEnabled);
    inFile.Read(this->mUnitImprovementsAllowed);
    inFile.Read(this->mTeamsLocked);
    inFile.Read(this->mCheatsEnabled);
    inFile.Read(this->mTownIDCounter);
    inFile.Read(this->mHerdIDCounter);
    if (inMajorVersion <= 0 && inMinorVersion < 0xef)
        this->mUsingCarbon = false;
    else
        inFile.Read(this->mUsingCarbon);
    if (EESaveHasLiveState(inMajorVersion, inMinorVersion))   // port (0.254): the units' order
        if (LoadUnitOrder(inFile) == kErrorOK && LoadForests(inFile) == kErrorOK
            && EESaveHasRunningState(inMajorVersion, inMinorVersion)   // port (0.256): the computer players' schedule
            && LoadAISchedule(inFile) == kErrorOK
            && gPathManager.LoadRunningState(inFile) == kErrorOK   // port (0.256): the path manager's history, regions, timers
            && EEAELoadRunningState(inFile) == kErrorOK   // port (0.256): the area effects' locations, the lists acting
            && LoadTileOrder(inFile) == kErrorOK   // port (0.256): the order of the units on each tile
            && EESaveHasRegionOrder(inMajorVersion, inMinorVersion)
            && LoadRegionOrder(inFile) == kErrorOK   // port (0.257): the order of the units in each map region
            && EESaveHasExperimentTimers(inMajorVersion, inMinorVersion)
            && EECPLoadExperimentTimers(inFile) == kErrorOK   // port (0.258): the computer players' experiment timers
            && EESaveHasTowns(inMajorVersion, inMinorVersion)
            && EELoadTowns(inFile, EESaveHasTownAreas(inMajorVersion, inMinorVersion)) == kErrorOK   // port (0.260): the towns' ids and order
            && EESaveHasGhosts(inMajorVersion, inMinorVersion)
            && LoadGhosts(inFile, EESaveHasGhostResources(inMajorVersion, inMinorVersion),
                          EESaveHasGoalGhosts(inMajorVersion, inMinorVersion)) == kErrorOK   // port (0.262): the dead attackers the units still hold
            && EESaveHasContinentRegions(inMajorVersion, inMinorVersion)
            && EELoadContinentRegions(inFile) == kErrorOK   // port (0.263): the continent manager as it was
            && EESaveHasResourceState(inMajorVersion, inMinorVersion)
            && EECPLoadResourceState(inFile) == kErrorOK   // port (0.265): the claim ranges and danger zones
            && EESaveHasPlayerUnitOrder(inMajorVersion, inMinorVersion)
            && LoadPlayerUnitOrder(inFile) == kErrorOK   // port (0.270): the order of each player's unit list
            && EESaveHasPathSearches(inMajorVersion, inMinorVersion))
            gPathManager.LoadSearches(inFile);   // port (0.276): the units' tile searches in progress
    EEUIECLEFormManager::Load(inMajorVersion, inMinorVersion, inFile);
    return gUIManager->mMainGameForm->Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x54c3a7
// Copies the loaded teams, world settings and id counters into the game state (and its scenario manager, if any)
void EEPWorldHelper::Restore(EEGameState& ioState)
{
    if (this->mScenarioManager != NULL)
        ioState.SetScenarioManager(this->mScenarioManager);
    ioState.mTeams = this->mTeams;
    ioState.mGameOptions.mWondersForVictory = this->mNumberOfWondersToWin;
    ioState.mTimeOfDay = this->GetTimeOfDay();
    ioState.mC4FA = this->mC36;
    ioState.mZoom = this->GetZoom();
    ioState.mGameOptions.mUnitLimit = this->mPopulationLimit;
    ioState.mGameSpeed = this->GetGameSpeed();
    ioState.mGameOptions.mVictory = this->mVictoryEnabled;
    ioState.mUnitImprovementsAllowed = this->mUnitImprovementsAllowed;
    ioState.mGameOptions.mCheatCodes = this->mCheatsEnabled;
    ioState.mTeamsLocked = this->mTeamsLocked;
    ioState.mTownIDCounter = this->mTownIDCounter;
    ioState.mHerdIDCounter = this->mHerdIDCounter;
    ioState.mUsingCarbon = this->mUsingCarbon;
}

// FUNCTION: EXE 0x54c449
// Writes the teams, world settings, id counters and Carbon flag, then the chat log and the main game form;
// returns only the main game form's error
long EEPWorldHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    SaveTeams(inMajorVersion, inMinorVersion, outFile, this->mTeams);
    outFile.Write(this->mNumberOfWondersToWin);
    outFile.Write(this->mTimeOfDay);
    outFile.Write(this->mC36);
    outFile.Write(this->mZoom);
    outFile.Write(this->mPopulationLimit);
    outFile.Write(this->mGameSpeed);
    outFile.Write(this->mVictoryEnabled);
    outFile.Write(this->mUnitImprovementsAllowed);
    outFile.Write(this->mTeamsLocked);
    outFile.Write(this->mCheatsEnabled);
    outFile.Write(this->mTownIDCounter);
    outFile.Write(this->mHerdIDCounter);
    outFile.Write(this->mUsingCarbon);
    if (EESaveHasLiveState(inMajorVersion, inMinorVersion))   // port (0.254): the units' order
    {
        SaveUnitOrder(outFile);
        SaveForests(outFile);
        if (EESaveHasRunningState(inMajorVersion, inMinorVersion)) {   // port (0.256): the computer players' schedule
            SaveAISchedule(outFile);
            gPathManager.SaveRunningState(outFile);   // port (0.256): the path manager's history, regions and timers
            EEAESaveRunningState(outFile);   // port (0.256): the area effects' locations and the lists of those acting
            SaveTileOrder(outFile);   // port (0.256): the order of the units on each tile
            if (EESaveHasRegionOrder(inMajorVersion, inMinorVersion))
                SaveRegionOrder(outFile);   // port (0.257): the order of the units in each map region
            if (EESaveHasExperimentTimers(inMajorVersion, inMinorVersion))
                EECPSaveExperimentTimers(outFile);   // port (0.258): the computer players' experiment timers
            if (EESaveHasTowns(inMajorVersion, inMinorVersion))
                EESaveTowns(outFile);   // port (0.260): the towns' ids and order
            if (EESaveHasGhosts(inMajorVersion, inMinorVersion))
                SaveGhosts(outFile);   // port (0.262): the dead attackers the units still hold
            if (EESaveHasContinentRegions(inMajorVersion, inMinorVersion))
                EESaveContinentRegions(outFile);   // port (0.263): the continent manager as it is
            if (EESaveHasResourceState(inMajorVersion, inMinorVersion))
                EECPSaveResourceState(outFile);   // port (0.265): the claim ranges and danger zones
            if (EESaveHasPlayerUnitOrder(inMajorVersion, inMinorVersion))
                SavePlayerUnitOrder(outFile);   // port (0.270): the order of each player's unit list
            if (EESaveHasPathSearches(inMajorVersion, inMinorVersion))
                gPathManager.SaveSearches(outFile);   // port (0.276): the units' tile searches in progress
        }
    }
    EEUIECLEFormManager::Save(inMajorVersion, inMinorVersion, outFile);
    return gUIManager->mMainGameForm->Save(inMajorVersion, inMinorVersion, outFile);
}

// FUNCTION: EXE 0x54c530
// Reads the team count and that many teams; returns the first read error
static long LoadTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      std::vector<EETeam>& outTeams)
{
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    // A failed count read leaves theCount unset: do not resize to it.
    if (theResult != kErrorOK)
        return theResult;
    outTeams.resize(theCount);
    for (std::vector<EETeam>::iterator it = outTeams.begin(); it != outTeams.end() && theResult == kErrorOK; ++it)
        theResult = it->Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// FUNCTION: EXE 0x54c581
// Writes the team count and each team; returns the first write error
static long SaveTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                      std::vector<EETeam>& inTeams)
{
    unsigned long theCount = inTeams.size();
    long theResult = outFile.Write(theCount);
    for (std::vector<EETeam>::iterator it = inTeams.begin(); it != inTeams.end() && theResult == kErrorOK; ++it)
        theResult = it->Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// TEMPLATE: EXE 0x54c5d9
// std::vector<EETeam>::resize(size_type) (first used by LoadTeams)

// TEMPLATE: EXE 0x54c5f5
// std::vector<EETeam>::resize(size_type, EETeam)

// ICF-FOLDED: EXE 0x557269
// Adds nothing to the checksum (empty: folded into a shared empty function)
void EEPWorldHelper::GetChecksum(ESChecksum& ioChecksum) const
{
}

// SYNTHETIC: EXE 0x54bd64
// SYNTHETIC: EXE 0x54bd6e
// SYNTHETIC: EXE 0x54bd7f
// SYNTHETIC: EXE 0x54bd8b
// SYNTHETIC: EXE 0x54bda7
// SYNTHETIC: EXE 0x54bdb1
// SYNTHETIC: EXE 0x54bdc2
// SYNTHETIC: EXE 0x54bdce
// SYNTHETIC: EXE 0x54bdea
// SYNTHETIC: EXE 0x54bdf4
// SYNTHETIC: EXE 0x54be05
// SYNTHETIC: EXE 0x54be11
// SYNTHETIC: EXE 0x54be2d
// SYNTHETIC: EXE 0x54be37
// SYNTHETIC: EXE 0x54be48
// SYNTHETIC: EXE 0x54be54
// SYNTHETIC: EXE 0x54be70
// SYNTHETIC: EXE 0x54be7a
// SYNTHETIC: EXE 0x54be8b
// SYNTHETIC: EXE 0x54be97
// SYNTHETIC: EXE 0x54beb3
// SYNTHETIC: EXE 0x54bebd
// SYNTHETIC: EXE 0x54bece
// SYNTHETIC: EXE 0x54beda
// SYNTHETIC: EXE 0x54bef6
// SYNTHETIC: EXE 0x54bf00
// SYNTHETIC: EXE 0x54bf11
// SYNTHETIC: EXE 0x54bf1d
// SYNTHETIC: EXE 0x54bf39
// SYNTHETIC: EXE 0x54bf43
// SYNTHETIC: EXE 0x54bf54
// SYNTHETIC: EXE 0x54bf60
// SYNTHETIC: EXE 0x54bf7c
// SYNTHETIC: EXE 0x54bf86
// SYNTHETIC: EXE 0x54bf97
// SYNTHETIC: EXE 0x54bfa3
// SYNTHETIC: EXE 0x54bfbf
// SYNTHETIC: EXE 0x54bfc9
// SYNTHETIC: EXE 0x54bfda
// SYNTHETIC: EXE 0x54bfe6
// SYNTHETIC: EXE 0x54c002
// SYNTHETIC: EXE 0x54c00c
// SYNTHETIC: EXE 0x54c01d
// SYNTHETIC: EXE 0x54c029
// SYNTHETIC: EXE 0x54c045
// SYNTHETIC: EXE 0x54c04f
// SYNTHETIC: EXE 0x54c060
// SYNTHETIC: EXE 0x54c06c
// SYNTHETIC: EXE 0x54c088
// SYNTHETIC: EXE 0x54c092
// SYNTHETIC: EXE 0x54c0a3
// SYNTHETIC: EXE 0x54c0af
// SYNTHETIC: EXE 0x54c0cb
// SYNTHETIC: EXE 0x54c0d5
// SYNTHETIC: EXE 0x54c0e6
// SYNTHETIC: EXE 0x54c0f2
// SYNTHETIC: EXE 0x54c10e
// SYNTHETIC: EXE 0x54c118
// SYNTHETIC: EXE 0x54c129
// SYNTHETIC: EXE 0x54c135
