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
    const std::deque<EEComplexUnit*>* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
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
    // the stepped units and the queues: the same units, in the saved order
    std::deque<EEComplexUnit*>* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
    for (int l = 0; l < 3; l++) {
        std::deque<EEComplexUnit*> theOrder;
        bool theFound = inLists[l].size() == theLists[l]->size();
        for (size_t i = 0; i < inLists[l].size() && theFound; i++) {
            EEComplexUnit* theUnit = LoadedComplexUnit(inLists[l][i]);
            theFound = theUnit != NULL && std::find(theLists[l]->begin(), theLists[l]->end(), theUnit) != theLists[l]->end();
            theOrder.push_back(theUnit);
        }
        if (theFound)
            *theLists[l] = theOrder;
        else if (l == 0)
            ESDebugOut("EEPWorldHelper: the loaded stepped units are not the saved ones: their order is the load's\n");
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
        if (LoadUnitOrder(inFile) == kErrorOK)
            LoadForests(inFile);
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
