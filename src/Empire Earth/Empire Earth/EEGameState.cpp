// C:\EEX\Empire Earth\Empire Earth\EEGameState.cpp (__FILE__ 0x4bfa30 family, Create's asserts). 0x1304e4..0x131b7c
// Clear, RepairRestorables, Init, Create, Reset, the manager setters, EEGetPlayerColor and the
// vector<EETeam>/ESChecksum COMDATs. EEMusicManager.cpp's chains start at 0x131b7c. Unit flags: the exe module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/U2DPoint.h"
#include "lle/UIEvent.h"
#include "lle/UResourceFile.h"
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
#include "lle/GEModelManager.h"
#include "lle/FSFileSpec.h"
#include "lle/FSFile.h"
#include "lle/FSMemoryFile.h"
#include "lle/ESChecksum.h"
#include "lle/TSScheduler.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DRectangle.h"
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFormManager.h"
#include "lle/GELine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EETeam.h"
#include "ee/EEGameState.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPEngine.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EEColorTable.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEEffectManager.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/URandom.h"
#include "lle/TSReadWriteLock.h"
#include "ee/EEPhysics.h"
#include "ee/EEUserInterface.h"
#include "ee/EESaveLoad.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameStateLoader.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEStartingResources.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x5304e4
// Deletes the map, the scenario, effect, calamity and weather managers and every player, then resets the state
void EEGameState::Clear()
{
    delete this->mMap;
    this->mMap = NULL;
    delete this->mScenarioManager;
    this->mScenarioManager = NULL;
    delete this->mEffectManager;
    this->mEffectManager = NULL;
    delete this->mCalamityHelper;
    this->mCalamityHelper = NULL;
    delete this->mEnvironmentManager;
    this->mEnvironmentManager = NULL;
    this->mRestorables.clear();
    for (unsigned long i = 0; i < this->mPlayers.size(); i++)
        delete this->mPlayers[i];
    this->mPlayers.clear();
    this->Init();
    this->Reset();
}

// FUNCTION: EXE 0x5305bf
// Lets every restorable object repair its pointers with the loaded save's version
void EEGameState::RepairRestorables()
{
    for (unsigned long i = 0; i < this->mRestorables.size(); i++) {
        FSRestorable*& theRestorable = this->mRestorables[i]; theRestorable->RepairPointers(this->mMajorVersion, this->mMinorVersion); }
}

// FUNCTION: EXE 0x5305fb
// Resets the game state's settings to their defaults (normal speed and zoom, no map, managers, teams or
// loaded players, a fresh checksum)
void EEGameState::Init()
{
    this->mGameSpeed = 1.0f;
    this->mMap = NULL;
    this->mZoom = 1.0f;
    this->mScenarioManager = NULL;
    this->mEnvironmentManager = NULL;
    this->mEffectManager = NULL;
    this->mCalamityHelper = NULL;
    this->mLocalPlayer = 0;
    this->mRandomSeed = 0;
    this->mMajorVersion = 0;
    this->mMinorVersion = 0;
    this->mTimeOfDay = 0.35f;
    this->mNumPlayersMade = 3;
    this->mC4FA = 0;
    this->mB4FB = false;
    this->mIsLoadedGame = false;
    this->mInScenarioEditor = false;
    this->mTeamsLocked = true;
    this->mUnitImprovementsAllowed = true;
    this->mUsingCarbon = false;
    this->mRevealMap = false;
    this->mB501 = false;
    this->mTownIDCounter = 0;
    this->mHerdIDCounter = 0;
    this->mTeams.clear();
    this->mLoadedPlayers.clear();
    { ESChecksum theChecksum(false); this->mChecksum = theChecksum; }
}

extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
extern long gDefaultTerrain;                // EETerrainTable.cpp

// FUNCTION: EXE 0x5306f2
// Builds a new game from the options: loads a saved game, scenario or campaign scenario (setting the error
// message on failure), or creates the players, teams, managers and random map; returns whether it succeeded
bool EEGameState::Create(EEGameOptions& inOptions, UWideString& outErrorMessage)
{
    std::vector<EEPlayer*> thePlayers;
    URandomIntegers theRandom;
    EEGameStateLoader theLoader(&gGameState);
    EEGameOptionsPlayer theSlot;
    EEGameOptionsPlayer theOther;
    FSArchive theArchive;
    UWideString theCampaignFile;
    UWideString theMapName;
    UWideString theComputerName;
    GE3DPoint theCenter;
    EECampaign theCampaign;
    FSFileSpec* theFile = new FSFileSpec;
    FSFileSpec* theArchiveFile = NULL;
    unsigned long theLocalIndex = 0;
    bool theResult = true;

    this->Init();
    if (gPhysics != NULL)
        EEComplexUnit::SetPoolSize(500, 40);
    if (inOptions.mLoadType == 5)
        inOptions.mLoadType = 3;
    if (inOptions.mLoadType != 0 && inOptions.mLoadType != 3 && inOptions.mLoadType != 5) {
        gUserInterface->SetLoadProgress(0.1f);
        gUserInterface->SetLoadingText(IDS_LOADING_GAME);
        theFile->SetVolume(gStartupVolume);
        if (inOptions.mLoadType == 2) {
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
            theFile->SetFile((const char*)inOptions.mMapType);
        } else if (inOptions.mLoadType == 4) {
            delete theFile;
            theFile = NULL;
            long theError = gDataPath->FindFile(UString(inOptions.mMapType), theFile);
            if (theError != 0) {
                theCampaignFile = UWideString("Campaigns\\") + inOptions.mCampaignName + gCampaignExtension;
                if (theCampaign.Load(theCampaignFile, outErrorMessage, false)) {
                    delete theArchiveFile;
                    theArchiveFile = NULL;
                    theError = gDataPath->FindFile(UString(theCampaignFile), theArchiveFile);
                    if (theError == 0) {
                        theArchive.SetArchive(*theArchiveFile);
                        gDataPath->AddArchive(theArchive, true);
                        EESetDataPaths();
                        delete theArchiveFile;
                        delete theFile;
                        theFile = NULL;
                        gDataPath->FindFile(UString(inOptions.mMapType), theFile);
                    } else {
                        gResource->GetString(keeCouldntOpenFile, outErrorMessage);
                        sprintf(outErrorMessage, "%s\n\"%s\"", (const char*)outErrorMessage,
                                (const char*)theCampaignFile);
                    }
                } else {
                    gResource->GetString(keeCouldntOpenFile, outErrorMessage);
                    sprintf(outErrorMessage, "%s\n\"%s\"", (const char*)outErrorMessage,
                            (const char*)theCampaignFile);
                }
            }
        } else {
            theFile->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
            theFile->SetFile((const char*)inOptions.mMapType);
        }
        theResult = false;
        long theError = theFile != NULL ? theFile->Open(kFileAccessRead) : 9;
        if (theError == 0) {
            if (EEValidateSaveGameFile(*theFile, outErrorMessage)) {
                if (theLoader.Load(*theFile)) {
                    theResult = true;
                } else {
                    ESVERIFY_AT(::gResource->GetString(keeslSaveGameInvalid, outErrorMessage) == keseOK, 759);
                }
            }
        } else {
            ESVERIFY_AT(::gResource->GetString(keeCouldntOpenFile, outErrorMessage) == keseOK, 765);
        }
        if (theResult) {
            if (inOptions.mLoadType == 2) {
                if (this->mGameOptions.mCampaignName.GetLength() > 0 && this->mGameOptions.mCampaignChecksum > 0) {
                    theCampaignFile = UWideString("Campaigns\\") + this->mGameOptions.mCampaignName + gCampaignExtension;
                    if (theCampaign.Load(theCampaignFile, outErrorMessage, false)) {
                        delete theFile;
                        theFile = NULL;
                        if (gDataPath->FindFile(UString(theCampaignFile), theFile) == 0) {
                            theArchive.SetArchive(*theFile);
                            gDataPath->AddArchive(theArchive, true);
                            EESetDataPaths();
                        } else {
                            theResult = false;
                            gResource->GetString(keeCouldntOpenFile, outErrorMessage);
                            sprintf(outErrorMessage, "%s\n\"%s\"", (const char*)outErrorMessage,
                                    (const char*)theCampaignFile);
                        }
                    } else {
                        gResource->GetString(keeCouldntOpenFile, outErrorMessage);
                        sprintf(outErrorMessage, "%s\n\"%s\"", (const char*)outErrorMessage,
                                (const char*)theCampaignFile);
                        theResult = false;
                    }
                }
            } else if (inOptions.mLoadType == 4) {
                if (!inOptions.GetCampaignMapName(theMapName))
                    theMapName = gU4E5B98;
                this->mGameOptions.mCampaignChecksum = inOptions.mCampaignChecksum;
                this->mGameOptions.mCampaignName = inOptions.mCampaignName;
                this->mGameOptions.mScenarioName = inOptions.mScenarioName;
                this->mGameOptions.mMapName = theMapName;
                this->mGameOptions.mIsScenarioGame = true;
                this->mGameOptions.mDifficulty = inOptions.mDifficulty;
            } else {
                this->mGameOptions.mIsScenarioGame = true;
                unsigned long thePlayerIndex = 1;
                for (unsigned long i = 0; i < 16; i++) {
                    theSlot = inOptions.mPlayers[i];
                    if (theSlot.mType != 1 && theSlot.mType != 2)
                        continue;
                    unsigned long k;
                    for (k = thePlayerIndex - 1; k < 16 && k != 0 && this->mGameOptions.mPlayers[k].mB36; k++)
                        ;
                    thePlayerIndex = k + 1;
                    if (this->mPlayers[thePlayerIndex]->IsHuman()) {
                        this->mPlayers[thePlayerIndex]->mName = theSlot.mName;
                    } else {
                        this->mPlayers[thePlayerIndex]->mName = theSlot.mName;
                        this->mPlayers[thePlayerIndex]->mComputerPlayer->mReplaceOnLoad = true;
                    }
                    theOther = this->mGameOptions.mPlayers[i];
                    theOther.mType = theSlot.mType;
                    theOther.mName = theSlot.mName;
                    this->mGameOptions.mPlayers[i] = theOther;
                    thePlayerIndex++;
                }
            }
        }
    } else {
        gUserInterface->SetLoadProgress(0.1f);
        gUserInterface->SetLoadingText(IDS_CREATING_WORLD);
        bool theUsedColors[16];
        memset(theUsedColors, 0, sizeof(theUsedColors));
        unsigned long i;
        for (i = 0; i < 16; i++) {
            theSlot = inOptions.mPlayers[i];
            if (theSlot.mType == 1)
                theUsedColors[theSlot.mColor] = true;
        }
        this->mPlayers.resize(1);
        this->mNumPlayersMade = 1;
        this->mPlayers[0] = new EEHumanPlayer(UWideString("World"), 0);
        unsigned long thePlayerID = 1;
        unsigned long theColor = 0;
        long theEngines[16];   // port: the computer slots' resolved engines (-1: not a computer slot)
        for (i = 0; i < 16; i++)
            theEngines[i] = -1;
        for (i = 0; i < 16; i++) {
            theSlot = inOptions.mPlayers[i];
            if (theSlot.mType != 1 && theSlot.mType != 0 && theSlot.mType != 2)
                continue;
            EEPlayer* thePlayer;
            if (theSlot.mType == 0) {
                while (theUsedColors[theColor])
                    theColor++;
                theSlot.mColor = theColor++;
                gResource->GetString(IDS_COMPUTER, theComputerName);
                EEComputerPlayer* theComputer = new EEComputerPlayer(theComputerName, thePlayerID);
                // port: the slot's engine, random drawn here (the seed is every peer's; ee/EECPEngine.h)
                theComputer->mEngine = (unsigned char)EECPEngineResolve(theSlot.mEngine, gUserInterface->mRandomSeed,
                                                                        i, thePlayerID);
                theEngines[i] = (long)theComputer->mEngine;
                thePlayer = theComputer;
            } else {
                if (theSlot.mType == 2) {
                    while (theUsedColors[theColor])
                        theColor++;
                    theSlot.mColor = theColor++;
                }
                thePlayer = new EEHumanPlayer(theSlot.mName, thePlayerID);
                if (theSlot.mIsLocal)
                    this->mLocalPlayer = thePlayerID;
                if (theSlot.mType == 2) {
                    thePlayer->mObserverMode = true;
                    thePlayer->mSeesAll = true;
                    theSlot.mTeam = 0;
                }
            }
            thePlayer->SetColor(EEGetPlayerColor(theSlot.mColor));
            if (inOptions.mLoadType == 0 && !thePlayer->mObserverMode)
                thePlayer->SetCivilizationPoints(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
            thePlayer->mMaxStartingCitizens = theSlot.mStartingCitizens;
            for (long j = 1; j < 6; j++)
                thePlayer->mResourceCaps[j + 1] = EEGetResourceLevel(inOptions.mResources)->mAmounts[j - 1];
            this->mPlayers.resize(thePlayerID + 1);
            this->mNumPlayersMade = thePlayerID + 1;
            this->mPlayers[thePlayerID] = thePlayer;
            thePlayerID++;
        }
        thePlayerID = 1;
        for (i = 0; i < 16; i++) {
            theSlot = inOptions.mPlayers[i];
            if (theSlot.mType != 1 && theSlot.mType != 0 && theSlot.mType != 2)
                continue;
            if (theSlot.mTeam > 0 && theSlot.mType != 2) {
                unsigned long theOtherID = 1;
                for (unsigned long j = 0; j < 16; j++) {
                    theOther = inOptions.mPlayers[j];
                    if (theOther.mType != 1 && theOther.mType != 0 && theOther.mType != 2)
                        continue;
                    if (theOther.mType != 2 && j != i && theOther.mTeam == theSlot.mTeam
                        && this->mPlayers[thePlayerID] != this->mPlayers[theOtherID])
                        this->mPlayers[thePlayerID]->SetDiplomaticStance(this->mPlayers[theOtherID], 0, NULL);
                    theOtherID++;
                }
            }
            thePlayerID++;
        }
        for (i = 0; i < this->mPlayers.size(); i++) {
            this->mPlayers[i]->mChatType = 0;
            if (this->mPlayers[i]->mAllies.size() > 0) {
                for (unsigned long j = 0; j < this->mPlayers[i]->mAllies.size(); j++) {
                    if (this->mPlayers[i]->mAllies[j]->IsHuman() == true) {
                        this->mPlayers[i]->mChatType = 1;
                        break;
                    }
                }
            }
        }
        {   // the speed option's factor (gameoptions.yaml speeds; an unknown option: the default speed's)
            const EEGameOptionRules& theRules = EEGetRules().mGameOptions;
            unsigned long theSpeed = inOptions.mGameSpeed < theRules.mSpeeds.size() ? inOptions.mGameSpeed
                                                                                   : theRules.mSpeedDefault;
            this->mGameSpeed = EERuleFloat(theRules.mSpeeds[theSpeed].mFactor);
        }
        long theWidth;
        long theHeight;
        inOptions.GetMapDimensions(theWidth, theHeight);
        this->mMap = new EEMap(theWidth, theHeight, this->mPlayers.size(), gDefaultTerrain);
        float theMiddle = 0.5f + (unsigned long)(theWidth + theHeight) * 0.5f;
        this->mMapCenter.mX = theMiddle;
        this->mMapCenter.mY = theMiddle;
        this->mMapCenter.mZ = 0.0f;
        this->mScenarioManager = new EEScenarioManager;
        this->mEffectManager = new EEEffectManager;
        this->mCalamityHelper = new EEPCalamityHelper;
        this->mEnvironmentManager = new EEEnvironmentManager;
        for (i = 0; i < this->mPlayers.size(); i++) {
            EEPlayer* thePlayer = this->mPlayers[i];
            if (i == 0)
                thePlayer->mSlot = kSlotNone;
            if (i == 1)
                thePlayer->mSlot = kSlotServer;
        }
        if (inOptions.mLoadType == 0) {
            for (i = 0; i < this->mPlayers.size(); i++) {
                if (this->mPlayers[i]->mObserverMode)
                    continue;
                thePlayers.push_back(this->mPlayers[i]);
                if (i == this->mLocalPlayer)
                    theLocalIndex = thePlayers.size() - 1;
            }
            this->mRandomSeed = gUserInterface->mRandomSeed;
            EERandomMap::CreateRandomMap(&inOptions, thePlayers, theLocalIndex, inOptions.mMapType, this->mRandomSeed, this->mMap,
                                         theCenter);
            this->mMapCenter = theCenter;
        }
        if (inOptions.mLoadType == 3)
            this->mNumPlayersMade = 3;
        this->mGameOptions = inOptions;
        for (i = 0; i < 16; i++)   // port: the game's options hold the engines played (a saved game shows them)
            if (theEngines[i] >= 0)
                this->mGameOptions.mPlayers[i].mEngine = (unsigned long)theEngines[i];
        if (inOptions.mEpochStart == EEEpochCount()) {   // random
            theRandom.SetSeed(gUserInterface->mRandomSeed);
            this->mGameOptions.mEpochStart = theRandom.Generate(0, inOptions.mEpochEnd);
        }
        if (inOptions.mRevealMap)
            this->mRevealMap = true;
        gGameOptions.mLock->WriteLock();
        if (inOptions.mLoadType == 3) {
            gGameOptions.mLoadType = 1;
            gGameOptions.mMapType = UWideString(UString("ScenDefault") + gScenarioExtension);
        }
        gGameOptions.mLock->WriteUnlock();
    }
    delete theFile;
    return theResult;
}

// Compiler-generated: EECampaign's implicit dtor (theCampaign)
// TEMPLATE: EXE 0x5317a9

// FUNCTION: EXE 0x5317eb
// Forgets the map, the managers and the player list without deleting them
void EEGameState::Reset()
{
    this->mMap = NULL;
    this->mScenarioManager = NULL;
    this->mEnvironmentManager = NULL;
    this->mEffectManager = NULL;
    this->mPlayers.clear();
    this->mCalamityHelper = NULL;
}

// FUNCTION: EXE 0x53181d
// Replaces the calamity helper, resetting and deleting the old one
void EEGameState::SetCalamityHelper(EEPCalamityHelper* inHelper)
{
    if (inHelper == this->mCalamityHelper)
        return;
    if (this->mCalamityHelper != NULL) {
        this->mCalamityHelper->Reset();
        delete this->mCalamityHelper;
    }
    this->mCalamityHelper = inHelper;
}

// FUNCTION: EXE 0x531854
// Replaces the weather manager, deleting the old one
void EEGameState::SetEnvironmentManager(EEEnvironmentManager* inManager)
{
    delete this->mEnvironmentManager;
    this->mEnvironmentManager = inManager;
}

// RTSPlayer::SetColor (inline; out of line after Create, its first user here)
// TEMPLATE: EXE 0x531872

// FUNCTION: EXE 0x5318bd
// Returns the colour of a player colour index (the default colour past the table's end)
const GEColor& EEGetPlayerColor(unsigned long inIndex)
{
    return inIndex < EEColorTable::sTable.size() ? EEColorTable::sTable[inIndex]->mColor : EEColorTable::sColor;
}

// FUNCTION: EXE 0x5318e6
// Returns a starting resource level by index
EEStartingResources* EEGetResourceLevel(long inIndex)
{
    return EEStartingResources::sTable[inIndex];
}

// TEMPLATE: EXE 0x5318f4
// TEMPLATE: EXE 0x531a5e
// TEMPLATE: EXE 0x531a99

#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"

// FUNCTION: EXE 0x531b02
// Destroys the team, deleting its victory condition
EETeam::~EETeam()
{
    delete this->mVictoryCondition;
}

// TEMPLATE: EXE 0x531b22

// Does nothing after a game is loaded (no address: empty, ICF-folded into 0x163d6f; EEGameStateLoader::LoadGame
// 0x14a5ba calls it with the save's versions: `push hdr.mMinorVersion; mov ecx, [esi] (the EEGameState*); push
// hdr.mMajorVersion; call 0x163d6f`, a direct thiscall on the state; provisional name)
void EEGameState::FinishLoad(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
}

// TEMPLATE: EXE 0x531b5f
// ??_GESChecksumEntry@@QAEPAXI@Z
