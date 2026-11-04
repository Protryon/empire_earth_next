// C:\EEX\Empire Earth\Empire Earth\EEWorld.cpp (name from RTTI EEWorld; no __FILE__). 0x137ba3..0x13e967: the game
// world (ctor, ??_G, dtor, the team/diplomacy helpers, the game setup from an EEGameState, the end-of-game statistics,
// the owning setters, the vtable slots), then a file-scope UString("default") whose init chain sits mid-file at
// 0x13c740, the rest of the members (0x13c772..0x13cdb9) and the unit's template COMDATs up to Empire Earth.cpp's
// chains (0x13e967). EEVoting.cpp ends at 0x137ba3. Unit flags: the exe's module set. Names are provisional unless a
// header names them.
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
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EEGameState.h"
#include "ee/EEGameOptions.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUserInterface.h"
#include "ee/EECommunications.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEPhysics.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamityManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEPathManager.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEButtonObject.h"
#include "ee/EEGameSound.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EECampaignProgress.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EECivilization.h"
#include "ee/EEGameVariant.h"
#include "ee/EEColorTable.h"
#include "ee/EEStartingResources.h"
#include "ee/EEUIEPostGameSummaryEvents.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAEManager.h"
#include "ee/EEE2DLightGlow.h"
#include "ee/EEStringIDs.h"

// Unit-local declarations of functions and globals no header declares yet (provisional names)
extern bool gLocalPlayerOut;                // 0x4e6099 (ee/EEMessage.h)
extern bool gSightUpdatesEnabled;           // 0x4c76ec
extern bool gAmbientSoundEnabled;           // 0x5308de (the ambient sound switch)
extern bool gDisplayWeatherEffects;         // 0x5308e6
extern long gL4E5E40;                       // 0x4e5e40 (only use: cleared with `and dword ptr, 0`: an integer)
extern float gViewportHalfWidth;            // 0x530b5c
extern float gViewportXCenter;              // 0x530b64
extern float gViewportYCenter;              // 0x530b68
extern UWideString gWonPlayerName;          // 0x53077c (the WON player name)
extern UString gDefaultCampaignUser;        // 0x52a170 ("default"; defined below, its init chain mid-file)
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
void EEStopAllSounds();                     // 0x140ceb
void EEDumpGraphicObjectTable(FSFileSpec& inFile);  // 0xd7acf
void EEDumpLeakedUnits(FSFileSpec& inFile);  // 0x1f8e7c
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362
void EEUpdateLandContinents();              // 0x23d76c
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);  // 0x24f0e9

// EEGameState's team count: inline (its refused copy 0x13cff7 sits in this unit's template tail); defined here where
// EETeam is complete
inline unsigned long EEGameState::GetNumTeams() const
{
    return this->mTeams.size();
}

// FUNCTION: EXE 0x537ba3
// Creates the game world with default settings (time of day 0.35, cheats and unit improvements allowed), no managers
// and empty statistics
EEWorld::EEWorld()
{
    this->mWindYaw = 0.0f;
    this->mEffectManager = NULL;
    this->mUB80 = 0;
    this->mEnvironmentManager = NULL;
    this->mCalamityHelper = NULL;
    this->SetTimeOfDay(0.35f);
    TSScheduler::CreateCriticalSection(this->mGhostUnitsLock);
    for (long i = 0; i < 10; i++)
        this->mUB30[i] = 1;
    this->mSavedGameSpeed = 0.0f;
    this->mUB58 = 2;
    this->mCurrentPlayer = 0;
    this->mNumberOfWondersToWin = 1;
    this->mInfluenceTable = NULL;
    this->mScenarioManager = NULL;
    this->mGameScreenActive = false;
    this->mMaxPlayers = 3;
    this->mLoadingSavedGame = false;
    this->mUnitImprovementsAllowed = true;
    this->mCheatsEnabled = true;
    this->mTeamsLocked = false;
    this->mTownIDCounter = 0;
    this->mHerdIDCounter = 0;
    this->mClearing = false;
    this->mStartTime = 0;
    this->mLagStatus = 0;
    this->mLoadingPlayerMedia = false;
    this->mUserPaused = false;
    this->mUsingCarbon = false;
    this->mInCinematic = false;
    this->mShowCollisionSpheres = false;
    this->mBB9D = true;
    this->mMediaLoaded = false;
    this->mCollisionDetectionFlags = 15;
    this->mExecutingLocalSelection = false;
    this->mStatistics.Reset();
}

// The statistics history's ctor (inline in the real header; its refused copy follows its first user):
// FUNCTION: EXE 0x537e06
// Creates an empty statistics history
EEStatisticsHistory::EEStatisticsHistory()
{
    this->Reset();
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x537e2c

// FUNCTION: EXE 0x537e48
// Destroys the world, deleting its influence table, ghost units and effect, scenario, environment and calamity managers
EEWorld::~EEWorld()
{
    TSScheduler::DestroyCriticalSection(this->mGhostUnitsLock);
    delete this->mInfluenceTable;
    delete this->mEffectManager;
    for (std::vector<EEGhostUnit*>::iterator it = this->mGhostUnits.begin(); it != this->mGhostUnits.end(); ++it)
        delete *it;
    this->mGhostUnits.clear();
    delete this->mScenarioManager;
    delete this->mEnvironmentManager;
    delete this->mCalamityHelper;
}

// FUNCTION: EXE 0x537f68
// Appends a ghost unit to the world's ghost list
void EEWorld::AddGhostUnit(EEGhostUnit* inGhost)
{
    this->mGhostUnits.push_back(inGhost);
}

// FUNCTION: EXE 0x537f7b
// Adds a team of the given players, first merging in every existing team whose players are allied both ways with them
void EEWorld::AddTeam(const std::vector<unsigned long>& inPlayers)
{
    EETeam theTeam(inPlayers);
    for (unsigned long i = 0; i < this->mTeams.size(); i++) {
        bool theDisjoint = true;
        for (unsigned long j = 0; j < this->mTeams[i].mPlayers.size() && theDisjoint; j++) {
            EEPlayer* thePlayer = this->mPlayers[this->mTeams[i].mPlayers[j]];
            if (thePlayer == NULL)
                continue;
            for (unsigned long k = 0; k < theTeam.GetNumPlayers(); k++) {
                if (this->mPlayers[theTeam.mPlayers[k]] != NULL) {
                    unsigned long theID = thePlayer->GetPlayerID();
                    if (this->mPlayers[theTeam.mPlayers[k]]->mEnemies[theID] != 0) {
                        theDisjoint = false;
                        break;
                    }
                }
                if (thePlayer->mEnemies[theTeam.mPlayers[k]] != 0) {
                    theDisjoint = false;
                    break;
                }
            }
        }
        if (!theDisjoint)
            continue;
        theTeam.Merge(this->mTeams[i]);
        this->mTeams.erase(this->mTeams.begin() + i);
        i--;
    }
    this->mTeams.push_back(theTeam);
}

// FUNCTION: EXE 0x538185
// Puts two players on one team, merging in every existing team whose players are allied both ways with both of them
void EEWorld::AddAlliance(long inPlayer1, long inPlayer2)
{
    EETeam theTeam(inPlayer1, inPlayer2);
    EETeam theOther;
    for (unsigned long i = 0; i < this->mTeams.size(); i++) {
        theOther.mPlayers = this->mTeams[i].mPlayers;
        EEPlayer* thePlayer1 = this->mPlayers[inPlayer1];
        EEPlayer* thePlayer2 = this->mPlayers[inPlayer2];
        unsigned long j;
        for (j = 0; j < theOther.GetNumPlayers(); j++) {
            EEPlayer* thePlayer = this->mPlayers[theOther.mPlayers[j]];
            if (thePlayer1->mEnemies[thePlayer->GetPlayerID()] != 0 || thePlayer->mEnemies[inPlayer1] != 0
                || thePlayer2->mEnemies[thePlayer->GetPlayerID()] != 0 || thePlayer->mEnemies[inPlayer2] != 0)
                break;
        }
        if (!(j >= theOther.GetNumPlayers()))
            continue;
        theTeam.Merge(this->mTeams[i]);
        this->mTeams.erase(this->mTeams.begin() + i);
        i--;
    }
    this->mTeams.push_back(theTeam);
}

// FUNCTION: EXE 0x538304
// Grants each player the wonder power 4 population share bonus when they hold it with no enemy holder or only allies
// hold it, and takes it away when only enemies hold it (mixed allies and enemies: no change)
void EEWorld::ApplyShareBonus()
{
    for (unsigned long i = 1; i < this->mNumPlayers; i++) {
        EEPlayer* thePlayer = this->mPlayers[i];
        long theBalance = 0;
        if (thePlayer == NULL)
            continue;
        std::vector<long>::iterator itEnd = thePlayer->mWonderPowers.end();
        std::vector<long>::iterator it = thePlayer->mWonderPowers.begin();
        while (true) {
            if (it == itEnd || *it == 4)
                break;
            it++;
        }
        if (it != itEnd) {
            bool theFound = false;
            for (unsigned long j = 1; j < this->mNumPlayers; j++) {
                EEPlayer* theOther = this->mPlayers[j];
                if (j != i && theOther != NULL && thePlayer->mEnemies[j] == 1) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        theFound = true;
                        break;
                    }
                }
            }
            if (theFound)
                continue;
            theBalance = 1;
        } else {
            for (unsigned long j = 1; j < this->mNumPlayers; j++) {
                EEPlayer* theOther = this->mPlayers[j];
                if (j == i || theOther == NULL)
                    continue;
                if (thePlayer->mEnemies[j] == 0) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        if (theBalance < 0)
                            goto next;
                        theBalance = 1;
                    }
                } else if (thePlayer->mEnemies[j] == 1) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        if (theBalance > 0) {
                            theBalance = 0;
                            break;
                        }
                        theBalance = -1;
                    }
                }
            }
        }
        if (theBalance > 0) {
            thePlayer->mShareBonus += (long)(gWorldTable.GetFloatValue(0x24) * gWorld.mPlayerPopulationLimit);
        } else if (theBalance < 0) {
            thePlayer->mShareBonus -= (long)(gWorldTable.GetFloatValue2(0x24) * gWorld.mPlayerPopulationLimit);
        }
next:
        ;
    }
}

// FUNCTION: EXE 0x5384dd
// Sets a player's stance toward another; joins their teams when the alliance becomes mutual, splits them when a mutual
// alliance ends
void EEWorld::SetDiplomaticStance(unsigned long inPlayerID, long inOther, long inStance)
{
    EEPlayer* thePlayer = this->mPlayers[inPlayerID];
    EEPlayer* theOther = this->mPlayers[inOther];
    bool theWasAllied = thePlayer->mEnemies[inOther] == 0;
    thePlayer->SetDiplomaticStance(theOther, inStance, this->mMap);
    if (inStance == 0) {
        if (!theWasAllied && theOther->mEnemies[inPlayerID] == 0)
            this->AddAlliance(inPlayerID, inOther);
    } else if (theWasAllied && theOther->mEnemies[inPlayerID] == 0) {
        this->BreakAlliance(inPlayerID, inOther);
    }
}

// FUNCTION: EXE 0x538551
// Tears down the current game: records the end statistics, unloads media, removes every unit, ghost, effect,
// calamity and manager, frees the minimap and active unit lists, and returns campaign games to the campaign screen
void EEWorld::Clear()
{
    this->mClearing = true;
    if (!UShutdownInProgress() && this->mMap != NULL) {
        this->RecordStatistics();
        gUnitPickerLock->Enter();
        gUnitPicker.Reset();
        gUnitPickerLock->Leave();
    }
    if (gUIManager != NULL && gUIManager->mScenarioEditorForm != NULL)
        gUIManager->mScenarioEditorForm->ClearSelection();
    if (this->mGameOptions.mCampaignName.GetLength() > 0 && this->mGameOptions.mCampaignChecksum > 0 && !this->mLoadingSavedGame)
        EEShowSinglePlayerScreen();
    this->UnloadMedia();
    gWorld.mGhostUnitsLock->Enter();
    this->ClearGhostUnits();
    gWorld.mGhostUnitsLock->Leave();
    if (this->mEnvironmentManager != NULL) {
        this->mEnvironmentManager->SetEnabled(false);
        delete this->mEnvironmentManager;
        this->mEnvironmentManager = NULL;
    }
    gPathManager.FreePaths();
    if (this->mEffectManager != NULL)
        this->mEffectManager->ClearEffects();
    this->RemoveAllUnits();
    gAEManager.ClearEffects();
    delete this->mScenarioManager;
    this->mScenarioManager = NULL;
    this->mTeams.clear();
    USingleton<EECalamityManager>::GetInstance()->DestroyCalamities();
    for (unsigned long i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] == NULL || !(this->mPlayers[i]->mHackUnitClass < 0x367))
            continue;
        for (unsigned long j = 1; j < this->mNumPlayers; j++) {
            if (this->mPlayers[j] != NULL) {
                this->mPlayers[j]->UnloadUnitClassMedia(this->mPlayers[i]->mHackUnitClass);
                this->mPlayers[j]->UnloadUnitClassButtons(this->mPlayers[i]->mHackUnitClass);
            }
        }
        this->mPlayers[i]->SetHackUnitClass(0x367);
    }
    RTSWorld::Clear();
    if (gUserInterface != NULL)
        gUserInterface->PostUpdateBackgroundMedia();
    EEStopAllSounds();
    delete gMiniMap;
    gMiniMap = NULL;
    gPathManager.Reset();
    gLocalPlayerOut = false;
    std::deque<EEComplexUnit*>::iterator it;
    for (it = this->mActivateQueue.begin(); it != this->mActivateQueue.end(); ++it)
        (*it)->Dereference();
    for (it = this->mDeactivateQueue.begin(); it != this->mDeactivateQueue.end(); ++it)
        (*it)->Dereference();
    std::deque<EEComplexUnit*>::iterator ct;
    for (ct = this->mUpdateUnits.begin(); ct != this->mUpdateUnits.end(); ++ct)
        (*ct)->Dereference();
    std::map<long, EEComplexUnit*>::iterator mt;
    for (mt = this->mActiveUnits.begin(); mt != this->mActiveUnits.end(); ++mt)
        mt->second->Dereference();
    this->mActivateQueue.clear();
    this->mDeactivateQueue.clear();
    this->mUpdateUnits.clear();
    this->mActiveUnits.clear();
    EEPPatrolGroup::ResetIDs();
    EECPWallManager::ResetSizes();
    gCollisionDetectionUnit = 2000;
    delete this->mCalamityHelper;
    this->mCalamityHelper = NULL;
    delete this->mInfluenceTable;
    this->mInfluenceTable = NULL;
    this->mLagStatus = 0;
    if (gSoundManager != NULL)
        gSoundManager->SetIsMuted(false);
    if (gWonNet != NULL)
        gWonNet->ClearGameList();
    delete gEditableCivilization;
    gEditableCivilization = NULL;
    this->mClearing = false;
}

// FUNCTION: EXE 0x5388ee
// Turns every unit's sight back on and refreshes the local player's terrain visibility
void EEWorld::RestartUnits()
{
    for (EEUnit* theUnit = (EEUnit*)this->mUnits.mFirst; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext))
        theUnit->SightOnHere();
    this->GetCurrentPlayer()->RefreshTerrainVisibility();
}

// FUNCTION: EXE 0x538919
// Replaces a player with a human player copied from it by a save and load, killing the old player's units and moving
// its alliances, pending units and media over to the copy
void EEWorld::ReplacePlayer(unsigned long inIndex, unsigned long inMajorVersion, unsigned long inMinorVersion, EEMap* inMap)
{
    FSMemoryFile theFile(1000, true, 1);
    EEPlayer* theOld = this->mPlayers[inIndex];
    this->FlushQueuedPlacements();
    EEPlayer* theNew = new EEHumanPlayer;
    theOld->EEPlayer::Save(inMajorVersion, inMinorVersion, theFile);
    theFile.SetPosition(0, kFileOriginBegin, false);
    theNew->EEPlayer::Load(inMajorVersion, inMinorVersion, theFile);
    EEUnit* theUnit = theOld->mFirstUnit;
    while (theUnit != NULL) {
        EEUnit* theNext = static_cast<EEUnit*>(theUnit->mNext);
        if (theUnit->mComplexUnit != NULL)
            theUnit->mComplexUnit->ClearCollisionState();
        theUnit->Kill(false, false, false);
        theOld->KillUnit(theUnit, false);
        theUnit = theNext;
    }
    theOld->RemoveKilledUnits();
    theOld->ReleasePendingUnits();
    unsigned long i;
    for (i = 0; i < this->mQueuedPlacements.size(); i++)
        this->mQueuedPlacements[i].mUnit->Dereference();
    this->mQueuedPlacements.clear();
    for (i = 0; i < this->mNumPlayers; i++) {
        unsigned long j;
        if (theOld != this->mPlayers[i]) {
            for (j = 0; j < this->mPlayers[i]->mAllies.size(); j++) {
                if (this->mPlayers[i]->mAllies[j] != theOld)
                    continue;
                this->mPlayers[i]->RemoveAlly(theOld);
                EEPlayer* theAlly = theNew;
                this->mPlayers[i]->mAllies.push_back(theAlly);
            }
        } else {
            for (j = 0; j < this->mPlayers[i]->mAllies.size(); j++) {
                EEPlayer* theAlly = theOld->mAllies[j];
                theNew->mAllies.push_back(theAlly);
            }
        }
    }
    if (!this->mMediaLoaded && theOld->GetPlayerID() > 0) {
        (*(theOld->mUnitClassUpgrades.begin() + 0x136))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x132))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x133))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x168))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x2cd))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x20))->UnloadMedia(theOld);
        (*(theOld->mUnitClassUpgrades.begin() + 0x1e0))->UnloadMedia(theOld);
    }
    this->SetPlayer(inIndex, theNew);
    EETechnologyTable* theTechnologies = theNew->mTechnologies;
    theTechnologies->SetPlayer(theNew);
    for (unsigned long k = 0; k < theNew->GetNumPendingUnits(); k++) {
        RTSUnit* theOldUnit = theNew->GetPendingUnit(k);
        theOldUnit->UpdateTile(inMap);
        this->AddUnitEntry(theOldUnit, NULL);
    }
    theNew->ReleasePendingUnits();
    theTechnologies->ApplySavedState(gLoadingSavedGame, true);
    if (!this->mMediaLoaded && theNew->GetPlayerID() > 0) {
        theNew->mUnitClassUpgrades[kUnitTypeFoundation1x1]->LoadMedia(theNew, 0x7f);
        theNew->mUnitClassUpgrades[kUnitTypeFoundation2x2]->LoadMedia(theNew, 0x7f);
        theNew->mUnitClassUpgrades[kUnitTypeFoundation3x3]->LoadMedia(theNew, 0x7f);
        (*(theNew->mUnitClassUpgrades.begin() + 0x168))->LoadMedia(theNew, 0x7f);
        (*(theNew->mUnitClassUpgrades.begin() + 0x2cd))->LoadMedia(theNew, 0x7f);
        (*(theNew->mUnitClassUpgrades.begin() + 0x20))->LoadMedia(theNew, 0x7f);
        (*(theNew->mUnitClassUpgrades.begin() + 0x1e0))->LoadMedia(theNew, 0x7f);
    }
    for (theUnit = theNew->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        theUnit->LoadMedia(theUnit->IsFoundation());
        if (theUnit->mUnitClass->IsLandBuilding() && theUnit->mComplexUnit->mIsBuilt == true)
            theUnit->mPlayer->LoadUnitClassButtons(theUnit->mUnitClass->mUnitType);
    }
    theNew->RepairPointers(inMajorVersion, inMinorVersion);
}

// FUNCTION: EXE 0x538e29
// Resets the unit's collision state, collision tile and collision resolution count
// (an EEComplexUnit member defined here: ReplacePlayer and the unit change call it; provisional name)
void EEComplexUnit::ClearCollisionState()
{
    this->mCollisionState = 0;
    this->mCollisionTile.mY = this->mCollisionTile.mX = -10000;
    this->mCollisionResolutionCount = 0.0f;
}

// FUNCTION: EXE 0x538e5e
// Rebuilds the teams from diplomacy (one per non-observer player, merged by mutual alliances) and sets Gaia and every
// player at war with each other
void EEWorld::RebuildTeams()
{
    EETeam theTeam;
    EEPlayer* theGaia = this->mPlayers[0];
    std::vector<long> theUnused;
    this->mTeams.clear();
    if (this->mTeams.size() == 0) {
        for (unsigned long i = 1; i < this->mNumPlayers; i++) {
            if (this->mPlayers[i]->mObserverMode)
                continue;
            theTeam.AddPlayer(i);
            this->mTeams.push_back(theTeam);
            theTeam.mPlayers.clear();
        }
    }
    for (unsigned long i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i]->mObserverMode)
            continue;
        for (unsigned long j = i + 1; j < this->mNumPlayers; j++) {
            if (!this->mPlayers[j]->mObserverMode
                && (this->mPlayers[j] == this->mPlayers[i] || this->mPlayers[i]->mEnemies[this->mPlayers[j]->GetPlayerID()] == 0))
                this->AddAlliance(i, j);
        }
    }
    for (unsigned long k = 1; k < this->mNumPlayers; k++)
        theGaia->SetDiplomaticStance(this->mPlayers[k], 1, this->mMap);
    for (unsigned long m = 1; m < this->mNumPlayers; m++) {
        if (this->mPlayers[m] != NULL)
            this->mPlayers[m]->SetDiplomaticStance(this->mPlayers[0], 1, this->mMap);
    }
    this->TeamsChanged();
}

// FUNCTION: EXE 0x538fe5
// Counts one gatherer fewer on a resource unit (an untracked unit is ignored)
void EEWorld::ReleaseResourceUser(EEUnit* inUnit)
{
    std::map<EEUnit*, unsigned long>::iterator it = this->mResourceUsers.find(inUnit);
    // an untracked unit has no count (the map's head node used to be decremented instead)
    if (it != this->mResourceUsers.end() && it->second > 0)
        it->second--;
}

// FUNCTION: EXE 0x53900d
// Kills and removes every player's units (pending ones included) and the queued placements, emptying the unit list
void EEWorld::RemoveAllUnits()
{
    bool theWasClear = false;
    if (!this->mClearing) {
        this->mClearing = true;
        theWasClear = true;
    }
    unsigned long theCount = this->mNumPlayers;
    for (unsigned long n = 0; n < theCount; n++) {
        EEPlayer* thePlayer = this->mPlayers[n];
        if (thePlayer == NULL)
            continue;
        thePlayer->AddPendingUnits(false, true);
        EEUnit* theUnit = thePlayer->mFirstUnit;
        while (theUnit != NULL) {
            EEUnit* theNext = static_cast<EEUnit*>(theUnit->mNext);
            if (theUnit->mComplexUnit != NULL)
                theUnit->mComplexUnit->ClearCollisionState();
            theUnit->Kill(false, false, false);
            thePlayer->KillUnit(theUnit, false);
            theUnit = theNext;
        }
        thePlayer->RemoveKilledUnits();
        thePlayer->ReleasePendingUnits();
    }
    for (unsigned long i = 0; i < this->mQueuedPlacements.size(); i++)
        this->mQueuedPlacements[i].mUnit->Dereference();
    this->mQueuedPlacements.clear();
    this->mUnits.mFirst = NULL;
    if (theWasClear)
        this->mClearing = false;
}

// FUNCTION: EXE 0x5390e8
// Deletes every ghost unit under the ghost lock
void EEWorld::ClearGhostUnits()
{
    gWorld.mGhostUnitsLock->Enter();
    while (this->mGhostUnits.size() > 0) {
        EEGhostUnit* theGhost = this->mGhostUnits.back();
        this->mGhostUnits.pop_back();
        delete theGhost;
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x53913e
// Writes a report of all loaded media (players, effects, graphic objects, sounds, buttons, terrain, textures, models)
// to a file, plus the leaked units and references when inLeaks is set
void EEWorld::DumpMedia(FSFileSpec& inFile, bool inLeaks)
{
    UWideString theString;
    for (unsigned long i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL)
            this->mPlayers[i]->DumpMedia(inFile);
        fprintf(inFile, "\n\n");
    }
    EEGraphicEffect::DumpIDs(inFile);
    EEDumpGraphicObjectTable(inFile);
    EESoundObject::Dump(inFile);
    EEButtonObject::DumpAllLoadedMedia(inFile);
    EETerrainTable::DumpAllLoadedMedia(inFile);
    gTextureManager->DumpAllLoadedMedia(inFile);
    gModelManager->DumpAllLoadedMedia(inFile);
    if (inLeaks == true) {
        EEDumpLeakedUnits(inFile);
        UReferenceCounter::DumpAllLeakedReferences(inFile);
    }
}

// FUNCTION: EXE 0x5391f2
// Takes a player out of the game: marks them out, drops them from their teams, reveals the map to them (except a lost
// scenario), stamps their end time and ends any cinematic; for the local player it redraws the minimap
void EEWorld::RemovePlayerFromGame(bool inHasWon, unsigned long inPlayerID)
{
    EEPlayer* thePlayer = this->mPlayers[inPlayerID];
    this->MarkPlayerOut(inPlayerID);
    this->RemovePlayerFromTeams(inPlayerID);
    if (!this->mGameOptions.mIsScenarioGame || inHasWon == true)
        thePlayer->mSeesAll = true;
    thePlayer->mSavedGroups.mEndTime = gPhysics->mGameTime;
    gWorld.mScenarioManager->EndCinematic();
    if (thePlayer == this->mPlayers[this->mCurrentPlayer]) {
        gMiniMap->Redraw();
        gLocalPlayerOut = true;
    }
}

// FUNCTION: EXE 0x539264
// Fills the empty player slots after the game state's players with computer players in unused colours and 999,999
// of each resource (scenario editor), loading their foundation media
void EEWorld::AddComputerPlayers(EEGameState* inState)
{
    UWideString theName;
    char theUsed[17];
    memset(theUsed, 0, sizeof(theUsed));
    unsigned long i;
    for (i = 0; i < inState->GetNumPlayers(); i++) {
        for (unsigned long c = 0; c <= 16; c++) {
            if (EEGetPlayerColor(c) == this->mPlayers[i]->mColor) {
                theUsed[c] = true;
                break;
            }
        }
    }
    inState->GetNumPlayers();
    for (i = inState->GetNumPlayers(); i <= 16; i++) {
        EEPlayer* thePlayer = NULL;
        if (this->mPlayers[i] != NULL)
            continue;
        gResource->GetString(IDS_COMPUTER, theName);
        thePlayer = new EEComputerPlayer(theName, i);
        for (unsigned long c = 0; c <= 16; c++) {
            if (theUsed[c])
                continue;
            thePlayer->SetColor(EEGetPlayerColor(c));
            theUsed[c] = true;
            break;
        }
        thePlayer->SetAllResources(999999);
        this->SetPlayer(i, thePlayer);
        if (this->mMediaLoaded || i <= 0)
            continue;
        thePlayer->mUnitClassUpgrades[kUnitTypeFoundation1x1]->LoadMedia(thePlayer, 0x7f);
        thePlayer->mUnitClassUpgrades[kUnitTypeFoundation2x2]->LoadMedia(thePlayer, 0x7f);
        thePlayer->mUnitClassUpgrades[kUnitTypeFoundation3x3]->LoadMedia(thePlayer, 0x7f);
        thePlayer->mUnitClassUpgrades[kUnitTypeFoundation6x6]->LoadMedia(thePlayer, 0x7f);
        thePlayer->mUnitClassUpgrades[kUnitTypeFoundationAirport]->LoadMedia(thePlayer, 0x7f);
        (*(thePlayer->mUnitClassUpgrades.begin() + 0x20))->LoadMedia(thePlayer, 0x7f);
        (*(thePlayer->mUnitClassUpgrades.begin() + 0x1e0))->LoadMedia(thePlayer, 0x7f);
    }
}

// FUNCTION: EXE 0x5394b2
// Moves the world and viewport camera, unless a scenario camera move is running and the move is not forced
void EEWorld::SetCamera(const GETransformation& inTransformation, bool inForce)
{
    if (inForce || this->mScenarioManager == NULL || !this->mScenarioManager->mIsMovingCamera) {
        RTSWorld::SetCameraTransformation(inTransformation);
        gViewport.SetCamera(inTransformation);
    }
}

// FUNCTION: EXE 0x5394e7
// Adds the world's state and the game's random seed to a sync checksum
void EEWorld::GetChecksum(ESChecksum& ioChecksum)
{
    RTSWorld::GetChecksum(ioChecksum);
    unsigned long theSeed = gUserInterface->mRandomSeed;
    ioChecksum.AddChecksum("EEWorld", theSeed);
}

// FUNCTION: EXE 0x539528
// Returns the calamity helper's contagion (plague) manager
EEContagionManager* EEWorld::GetContagionManager()
{
    return (static_cast<EEPCalamityHelper*>(this->mCalamityHelper))->mContagionManager;
}

// FUNCTION: EXE 0x539532
// Returns how many gatherers use a resource unit, starting its count at 0 when it is not tracked yet
unsigned long EEWorld::GetResourceUsers(EEUnit* inUnit)
{
    std::map<EEUnit*, unsigned long>::iterator it = this->mResourceUsers.find(inUnit);
    unsigned long theResult = 0;
    if (it == this->mResourceUsers.end()) {
        this->mResourceUsers[inUnit] = 0;
    } else {
        theResult = it->second;
    }
    return theResult;
}

// FUNCTION: EXE 0x539589
// Returns whether a map tile's height is below 0
bool EEWorld::IsTileBlocked(unsigned long inX, unsigned long inY)
{
    return this->mMap->GetTile(inX, inY)->GetHeight() < 0.0f;
}

// FUNCTION: EXE 0x5395cf
// Converts a screen point to a point on the orthographic view plane (Y = 0) around the viewport centre
void EEWorld::ScreenToWorld(const GE2DPoint& inPoint, GE3DPoint& outPoint)
{
    float theScale = 1.0f / gViewport.GetHalfWidth();
    float theSize = gViewport.GetOrthographicRectangle().GetWidth() * 0.5f;
    GE2DPoint thePoint((inPoint.GetX() - gViewport.GetXCenter()) * theScale,
                       (inPoint.GetY() - gViewport.GetYCenter()) * theScale);
    outPoint.SetX(thePoint.GetX() * theSize);
    outPoint.SetY(0.0f);
    outPoint.SetZ(-(thePoint.GetY() * theSize));
}

// FUNCTION: EXE 0x539631
// Copies the world's player pointers, up to the maximum player count, into a vector
void EEWorld::CopyPlayers(std::vector<EEPlayer*>& outPlayers)
{
    outPlayers.resize(this->mMaxPlayers, 0);
    for (unsigned long i = 0; i < (unsigned long)this->mMaxPlayers; i++)
        outPlayers[i] = this->mPlayers[i];
}

// FUNCTION: EXE 0x539670
// Returns the player (Gaia excluded) in a lobby slot, or NULL
EEPlayer* EEWorld::GetPlayerBySlot(unsigned char inSlot)
{
    EEPlayer* theResult = NULL;
    for (unsigned long i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i]->mSlot == inSlot) {
            theResult = this->mPlayers[i];
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5396a9
// Returns the queued (not yet placed) waypoint on a tile whose hit points field holds the given unit id, or NULL
EEComplexUnit* EEWorld::FindQueuedUnitAt(long inX, long inY, unsigned long inUnitID)
{
    EEComplexUnit* theResult = NULL;
    for (std::vector<RTSWorldUnitEntry>::iterator it = this->mQueuedPlacements.begin(); it != this->mQueuedPlacements.end(); ++it) {
        EEComplexUnit* theUnit = (EEComplexUnit*)it->mUnit;
        if (theUnit != NULL && theUnit->mTileX == inX && theUnit->mTileY == inY
            && theUnit->mUnitClass->mUnitType == kUnitTypeWaypoint && theUnit->mHitPoints == (long)inUnitID) {
            theResult = theUnit;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x5396f3
// Counts one more gatherer on a resource unit
void EEWorld::AddResourceUser(EEUnit* inUnit)
{
    std::map<EEUnit*, unsigned long>::iterator it = this->mResourceUsers.find(inUnit);
    if (it == this->mResourceUsers.end())
        this->mResourceUsers[inUnit] = 1;
    else
        it->second++;
}

// the load progress steps (a unit-local .rdata block after the vtable, in reverse order of use)
static const float kWorldSunTilt = 0.62831855f;
static const float kWorldLoadProgress29 = 0.96666664f;
static const float kWorldLoadProgress28 = 0.93333334f;
static const float kWorldLoadProgress27 = 0.89999998f;
static const float kWorldLoadProgress26 = 0.86666667f;
static const float kWorldLoadProgress25 = 0.83333331f;
static const float kWorldLoadProgress24 = 0.80000001f;
static const float kWorldLoadProgress23 = 0.76666665f;
static const float kWorldLoadProgress22 = 0.73333335f;
static const float kWorldLoadProgress21 = 0.69999999f;
static const float kWorldLoadProgress20 = 0.66666669f;
static const float kWorldLoadProgress19 = 0.63333333f;
static const float kWorldLoadProgress18 = 0.60000002f;
static const float kWorldLoadProgress17 = 0.56666666f;
static const float kWorldLoadProgress16 = 0.53333336f;

// FUNCTION: EXE 0x539749
// Sets up the world from a game state (map, players, teams, local player, camera, sight, minimap, calamities, music,
// media), reporting load progress; in multiplayer it finds the local player by name
void EEWorld::StartGame(EEGameState* inState, bool inLocked)
{
    EECampaignProgress theProgress;
    EEGameOptionsPlayer theOptionsPlayer;
    UWideString theCivilizationName;
    UWideString theLocalName;
    unsigned long theLocal = 0;
    bool theLoaded = inState->mIsLoadedGame;
    bool theMoved = false;
    if (!inLocked)
        gWorld.mLock->WriteLock();
    this->mStatistics.Reset();
    inState->ReadLock();
    gUserInterface->SetLoadProgress(0.5f);
    this->mGameOptions = gGameState.mGameOptions;
    gSightUpdatesEnabled = false;
    gUserInterface->mRandomSeed = inState->mRandomSeed;
    this->SetMap(inState->mMap);
    gGraphics->PostReleaseScene();
    this->SetEffectManager(inState->mEffectManager);
    if (inState->mInScenarioEditor == true)
        this->SetNumPlayers(17);
    else
        this->SetNumPlayers(inState->GetNumPlayers());
    gUserInterface->SetLoadProgress(kWorldLoadProgress16);
    this->mInCinematic = false;
    this->mSavedGameSpeed = 0.0f;
    this->mMaxPlayers = inState->mNumPlayersMade;
    this->mTownIDCounter = inState->mTownIDCounter;
    this->mHerdIDCounter = inState->mHerdIDCounter;
    gL4E5E40 = 0;
    this->CreateInfluenceTable(this->mMap);
    gUserInterface->SetLoadProgress(kWorldLoadProgress17);
    if (gCommunications->GetNumHumanPlayers() > 1) {
        if (gWonNet != NULL) theLocalName = gWonPlayerName; else theLocalName = gPlayerName;
        for (unsigned long i = 0; i < inState->GetNumPlayers(); i++) {
            if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)inState->mPlayers[i]->mName),
                    reinterpret_cast<const unsigned char*>((const char*)theLocalName)) != 0)
                continue;
            theLocal = i;
            if (inState->mLocalPlayer != (long)i)
                theMoved = true;
            break;
        }
    } else {
        theLocal = inState->mLocalPlayer;
    }
    this->mCurrentPlayer = theLocal;
    gCommunications->SetPlayerNumber(theLocal);
    this->SetPopulationLimit(inState->mGameOptions.mUnitLimit);
    gUserInterface->SetLoadProgress(kWorldLoadProgress18);
    this->ClearContinents();
    gUserInterface->SetLoadProgress(kWorldLoadProgress19);
    this->CreateMiniMap(inState);
    gUserInterface->SetLoadProgress(kWorldLoadProgress20);
    this->mUsingCarbon = inState->mUsingCarbon;
    this->CreatePlayers(inState);
    gUserInterface->SetLoadProgress(kWorldLoadProgress21);
    if (inState->mInScenarioEditor == true)
        this->SetNumPlayers(17);
    else
        this->SetNumPlayers(inState->GetNumPlayers());
    unsigned long i;
    for (i = 0; i < this->mNumPlayers; i++)
        this->mPlayers[i]->mCivilization.ApplyDeferred(this->mPlayers[i], inState->mMajorVersion, inState->mMinorVersion);
    gArtificialIntelligence->CollectComputerPlayers();
    gUserInterface->SetLoadProgress(kWorldLoadProgress22);
    if (inState->GetNumTeams() > 0) {
        for (i = 0; i < inState->GetNumTeams(); i++)
            this->mTeams.push_back(inState->mTeams[i]);
        for (i = this->mNumPlayers; i <= 16; i++) {
            if (this->mPlayers[i] == NULL)
                this->RemovePlayerFromTeams(i);
        }
    } else {
        this->RebuildTeams();
    }
    this->mNumberOfWondersToWin = inState->mGameOptions.mWondersForVictory;
    this->mVictoryEnabled = inState->mGameOptions.mVictory;
    this->SetTimeOfDay(inState->mTimeOfDay);
    gWorld.SetGameSpeed(inState->mGameSpeed);
    gLocalPlayerOut = gWorld.GetCurrentPlayer()->mObserverMode;
    this->SetScenarioManager(inState->mScenarioManager);
    this->SetEnvironmentManager(inState->mEnvironmentManager);
    this->mTeamsLocked = inState->mGameOptions.mLockTeams;
    this->mUnitImprovementsAllowed = inState->mUnitImprovementsAllowed;
    this->mCheatsEnabled = inState->mGameOptions.mCheatCodes;
    this->mCameraControl.ClearUnitB();
    this->mCameraControl.ClearUnitA();
    this->mCameraControl.ResetPath();
    this->mCameraControl.ResetZoom();
    if (theMoved == true) {
        long theWidth = this->mMap->mSize;
        long theHeight = this->mMap->mSize;
        GE3DPoint theCenter(theWidth * 0.5f, theHeight * 0.5f, 0.0f);
        this->mCameraControl.SetLocation(theCenter, true, true);
    } else {
        this->mCameraControl.SetLocation(inState->mMapCenter, false, true);
    }
    this->mCameraControl.SetChanged();
    inState->RepairRestorables();
    gUserInterface->SetLoadProgress(kWorldLoadProgress23);
    gSightUpdatesEnabled = true;
    if (theLoaded) {
        UString theMessage("Game loaded.");
        EEReportLoadFailure(theMessage, false, &inState->mChecksum);
    } else {
        UString theMessage("Game initialized.");
        EEReportLoadFailure(theMessage, false, NULL);
    }
    if (this->mNumPlayers != this->mMap->mU19561C)
        this->mMap->SetPlayerCount(this->mNumPlayers);
    gMiniMap->mViewingPlayer = this->mPlayers[this->mCurrentPlayer];
    if (inState->mRevealMap == true) {
        for (long y = 0; y < this->mMap->mSize; y++) {
            for (long x = 0; x < this->mMap->mSize; x++) {
                EEMapTile* theTile = this->mMap->GetTile(x, y);
                if (theTile == NULL)
                    continue;
                this->mMap->mTerrainMesh.ReferenceTile(x, y);
                this->mMap->mTerrainMesh.DereferenceTile(x, y);
                for (unsigned long p = 1; p < this->mNumPlayers; p++) {
                    this->mPlayers[p]->AddOwnSight(theTile, x, y);
                    this->mPlayers[p]->RemoveOwnSight(theTile);
                }
            }
        }
    }
    gMiniMap->RedrawAll();
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    this->RestartUnits();
    gMiniMap->RedrawAll();
    gMiniMap->Redraw();
    gMiniMap->Refresh();
    gUserInterface->SetLoadProgress(kWorldLoadProgress24);
    gContinentManager.BuildRegions(this->mMap);
    gPathManager.Reset();
    gUserInterface->SetLoadProgress(kWorldLoadProgress25);
    if (this->mEnvironmentManager != NULL)
        this->mEnvironmentManager->SetEnabled(gDisplayWeatherEffects && !gCommunications->GetIsMultiplayerSession());
    if (this->mEnvironmentManager != NULL)
        this->mEnvironmentManager->FnE123D();
    this->SetCalamityHelper(inState->mCalamityHelper);
    std::vector<EECalamity*>* theCalamities = static_cast<EEPCalamityHelper*>(this->mCalamityHelper)->mCalamities;
    if (theCalamities != NULL)
        USingleton<EECalamityManager>::GetInstance()->Restore(inState->mMajorVersion, inState->mMinorVersion, *theCalamities);
    gUserInterface->SetLoadProgress(kWorldLoadProgress26);
    inState->ReadUnlock();
    gMusicManager->SetIntensity(1);
    gMusicManager->Start();
    gAmbientSoundManager.SetEnabled(gAmbientSoundEnabled);
    gAmbientSoundManager.Restart();
    gGraphics->PostReleaseScene();
    gGraphics->PostInitialiseScene();
    gUnitPickerLock->Enter();
    gUnitPicker.Reset();
    gUnitPickerLock->Leave();
    gMiniMap->Refresh();
    gPathManager.RequestPendingPaths(this->mMap);
    gUserInterface->SetLoadProgress(kWorldLoadProgress27);
    for (i = 1; i < this->mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (thePlayer == NULL)
            continue;
        if (thePlayer->mComputerPlayer != NULL)
            thePlayer->mComputerPlayer->RefreshResourceCenters();
        else
            thePlayer->RefreshTownGroups();
    }
    gPhysics->mGameStarted = true;
    gPhysics->mAutoSaveClock = 0;
    if (inState->mMajorVersion == 0 && inState->mMinorVersion <= 0x98) {
        for (i = 1; i < this->mNumPlayers; i++) {
            theOptionsPlayer = this->mGameOptions.mPlayers[i - 1];
            theOptionsPlayer.mColor = EEColorTable::FindIndex(this->mPlayers[i]->mColor);
            this->mGameOptions.mPlayers[i - 1] = theOptionsPlayer;
        }
    }
    if (gGameOptions.mCampaignName.GetLength() > 0 && gGameOptions.mCampaignChecksum > 0)
        theProgress.RestoreCivilization(UWideString(gDefaultCampaignUser), inState->mGameOptions.mCampaignName, inState->mGameOptions.mScenarioName, inState->mGameOptions.mCampaignChecksum);
    if (gGameOptions.GetCampaignMapName(theCivilizationName))
        this->GetCurrentPlayer()->mCivilization.SetName(theCivilizationName);
    if (inState->mGameOptions.mLoadType == 0 || inState->mGameOptions.mLoadType == 3)
        EEUIECLEFormManager::Clear();
    this->LoadMedia();
    gUserInterface->SetLoadProgress(kWorldLoadProgress28);
    if (!gUIManager->mInScenarioEditor)
        this->mScenarioManager->Prepare();
    this->mScenarioManager->ApplyPendingEffects();
    gUserInterface->SetLoadProgress(kWorldLoadProgress29);
    this->RebuildActiveUnits();
    if (gUIManager->mInScenarioEditor == true && this->mGameScreenActive == true)
        this->SetNumPlayers(inState->mNumPlayersMade);
    if (inState->mIsLoadedGame == true)
        this->NotifyPlayersLoaded(inState->mMajorVersion, inState->mMinorVersion);
    for (i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->RestartTowns();
            this->mPlayers[i]->RestorePatrols();
        }
    }
    this->mLoadingPlayerMedia = true;
    for (i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL)
            this->mPlayers[i]->KillStrandedUnits();
    }
    this->mLoadingPlayerMedia = false;
    EEUpdateLandContinents();
    this->mUserPaused = false;
    this->mLagStatus = 0;
    if (!inLocked)
        gWorld.mLock->WriteUnlock();
    gGameOptions.Fn130272();
}

// FUNCTION: EXE 0x53a1fb
// Replaces the world's map, deleting the old one
void EEWorld::SetMap(EEMap* inMap)
{
    delete this->mMap;
    this->mMap = inMap;
}

// FUNCTION: EXE 0x53a219
// Replaces the effect manager, deleting the old one
void EEWorld::SetEffectManager(EEEffectManager* inManager)
{
    delete this->mEffectManager;
    this->mEffectManager = inManager;
}

// FUNCTION: EXE 0x53a237
// Replaces the scenario manager, deleting the old one
void EEWorld::SetScenarioManager(EEScenarioManager* inManager)
{
    delete this->mScenarioManager;
    this->mScenarioManager = inManager;
}

// FUNCTION: EXE 0x53a255
// Creates the influence table for a map and the world's player count
void EEWorld::CreateInfluenceTable(EEMap* inMap)
{
    this->SetInfluenceTable(new EEInfluenceTable(inMap, this->mNumPlayers));
}

// FUNCTION: EXE 0x53a285
// Replaces the minimap with a new one for the world's map, viewed by the game state's local player
void EEWorld::CreateMiniMap(EEGameState* inState)
{
    delete gMiniMap;
    gMiniMap = new EEMiniMap(GERasterizer::GetActiveRasterizer()->GetScreen(gScreenIndex), 0, inState->GetPlayer(inState->mLocalPlayer));
    gMiniMap->SetMap(this->mMap);
    EEUIMiniMap::Update(gMiniMap);
}

// FUNCTION: EXE 0x53a312
// Installs the game state's players: places their pending units, applies tech trees, map restrictions and variant hit
// points and loads media; for a loaded game it repairs pointers and replaces flagged computer players
void EEWorld::CreatePlayers(EEGameState* inState)
{
    unsigned long i;
    for (i = 0; i < inState->mPlayers.size(); i++) {
        EEPlayer* thePlayer = inState->mPlayers[i];
        this->SetPlayer(i, thePlayer);
        EETechTree* theTechTree = thePlayer->GetTechTree();
        theTechTree->SetPlayer(thePlayer);
        if (!theTechTree->mIsLoaded)
            theTechTree->ResetEpochRange(inState->mGameOptions.mEpochStart, inState->mGameOptions.mEpochEnd);
        for (unsigned long j = 0; j < thePlayer->GetNumPendingUnits(); j++) {
            RTSUnit* theUnit = thePlayer->GetPendingUnit(j);
            theUnit->UpdateTile(inState->mMap);
            if (!inState->mIsLoadedGame && theUnit->mTileNode != NULL)
                (static_cast<EEComplexUnit*>(theUnit))->MakeAreaEffects();
            this->AddUnitEntry(theUnit, NULL);
        }
        if (inState->mIsLoadedGame)
            thePlayer->ReleasePendingUnits();
        else
            thePlayer->AddPendingUnits(false, true);
        thePlayer->ApplyMapRestrictions();
        if (inState->mIsLoadedGame && thePlayer->GetPlayerID() < inState->mMap->mU19561C)
            inState->mMap->FogSeen(thePlayer);
        theTechTree->ApplySavedState(gLoadingSavedGame, true);
        for (unsigned long k = 0; k < 0x367; k++) {
            EEUnitClassUpgrade* theUpgrade = thePlayer->mUnitClassUpgrades[k];
            if (theUpgrade == NULL)
                continue;
            long theType = theUpgrade->GetUnitClass()->mUnitType;
            EEUpgradeHitPoints(theUpgrade, theType,
                EEGameVariant::GetGameVariant(this->mGameOptions.GetVariant())->GetFactor(theType), thePlayer);
        }
        if (!this->mMediaLoaded && i != 0) {
            thePlayer->mUnitClassUpgrades[kUnitTypeFoundation1x1]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeFoundation2x2]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeFoundation3x3]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeFoundation6x6]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeFoundationAirport]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeCitizen]->LoadMedia(thePlayer, 0x7f);
            thePlayer->mUnitClassUpgrades[kUnitTypeFemaleCitizen]->LoadMedia(thePlayer, 0x7f);
        }
        for (EEUnit* theUnit2 = thePlayer->mFirstUnit; theUnit2 != NULL; theUnit2 = static_cast<EEUnit*>(theUnit2->mNext)) {
            theUnit2->LoadMedia(theUnit2->IsFoundation());
            if (theUnit2->mUnitClass->IsLandBuilding() && theUnit2->mComplexUnit->mIsBuilt == true)
                thePlayer->LoadUnitClassButtons(theUnit2->mUnitClass->mUnitType);
        }
    }
    this->mPlayers[0]->AddPendingUnits(false, false);
    if (inState->mIsLoadedGame) {
        for (i = 0; i < inState->GetNumPlayers(); i++) {
            EEPlayer* thePlayer = inState->mPlayers[i];
            thePlayer->RepairPointers(inState->mMajorVersion, inState->mMinorVersion);
            if (thePlayer->mComputerPlayer != NULL && thePlayer->mComputerPlayer->mReplaceOnLoad == true)
                this->ReplacePlayer(i, inState->mMajorVersion, inState->mMinorVersion, inState->mMap);
        }
    }
    this->FlushQueuedPlacements();
    if (inState->mInScenarioEditor == true)
        this->AddComputerPlayers(inState);
    for (i = 0; i < inState->mLoadedPlayers.size(); i++) {
        inState->mLoadedPlayers[i]->RepairPointers(inState->mMajorVersion, inState->mMinorVersion);
        delete inState->mLoadedPlayers[i];
    }
}

// FUNCTION: EXE 0x53a719
// Loads the shared command confirmation effects and button sounds, once per game
void EEWorld::LoadMedia()
{
    if (this->mMediaLoaded)
        return;
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMove)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationAttack)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationGatherOther)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectHorseBreathUnits)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectBasicMarchingLine)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectWaypointIndicator)->Load(false);
    EEGraphicEffect::Get(kGraphicEffectRallyFlagBuilding)->Load(false);
    EESoundObject::Get(kSoundButton1)->LoadMedia();
    EESoundObject::Get(kSoundButton2)->LoadMedia();
    this->mMediaLoaded = true;
}

// FUNCTION: EXE 0x53a7e8
// Pauses a running single-player game on the server; returns 2 when it is already paused, else 0
long EEWorld::PauseGame()
{
    long theResult = 0;
    if (gCommunications->IsServer() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0 && !gCommunications->GetIsMultiplayerSession()) {
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        if (!theServer->GetIsGamePaused()) {
            theServer->PauseGame();
            gUserInterface->PostPauseInformation(true);
        } else
            theResult = 2;
    }
    return theResult;
}

// FUNCTION: EXE 0x53a845
// Builds the post-game statistics: tops up computer players' resources against the best human, scores every player,
// sorts them by score and fills the records (computer play times scaled from the longest human one)
void EEWorld::RecordStatistics()
{
    EEStatisticsRecord* theRecordPtr = NULL;
    std::multimap<long, long, std::greater<long> > theScores;
    std::map<unsigned long, EEStatisticsRecord> theRecords;
    EEStatisticsRecord theRecord;
    UWideString theName;
    UWideString theDefault;
    std::vector<long> thePlayers;
    bool theLocalPending = false;
    unsigned long theHumans = 0;
    this->mStatistics.mStartTime = this->mStartTime;
    this->mStatistics.mTileCount = this->mMap->GetTileCount();
    this->mStatistics.mVictoryEnabled = this->mVictoryEnabled;
    this->mStatistics.mTeamsLocked = this->mTeamsLocked;
    this->mStatistics.mIsScenario = this->mGameOptions.mIsScenarioGame || (this->mGameOptions.mCampaignName.GetLength() > 0 && this->mGameOptions.mCampaignChecksum > 0);
    this->mStatistics.mIsCampaign = this->mGameOptions.mCampaignName.GetLength() > 0 && this->mGameOptions.mCampaignChecksum > 0;
    this->mStatistics.mEndTime = gPhysics->mGameTime;
    if (this->mStatistics.mIsCampaign)
        this->mStatistics.mCampaignName = this->mGameOptions.mCampaignName;
    thePlayers.clear();
    unsigned long i;
    for (i = 0; i < this->mNumPlayers; i++) {
        EEPlayer* thePlayer = this->mPlayers[i];
        if (!thePlayer->mObserverMode || thePlayer->mGameResult != 0) {
            thePlayers.push_back(i);
        } else {
            if (this->mPlayers[this->mCurrentPlayer] == thePlayer)
                theLocalPending = true;
            if (thePlayer->mDisconnected)
                thePlayers.push_back(i);
        }
    }
    long theComputerCount = 0;
    theScores.clear();
    for (i = 0; i < thePlayers.size(); i++) {
        EEPlayer* thePlayer = this->mPlayers[thePlayers[i]];
        if (!thePlayer->IsHuman()) {
            gResource->GetString(IDS_COMPUTER, theDefault);
            if (_mbscmp(reinterpret_cast<const unsigned char*>((const char*)thePlayer->mName), reinterpret_cast<const unsigned char*>((const char*)theDefault)) == 0) {
                theComputerCount++;
                sprintf(theRecord.mNameSuffix, " #%d", theComputerCount);
            } else {
                theRecord.mNameSuffix = gU4E5B98;
            }
            for (unsigned long r = 2; r < 7; r++) {
                long theBase = EEStartingResources::sTable[this->mGameOptions.mResources]->mAmounts[r - 2];
                long theSeries;
                if (r == 2)
                    theSeries = 7;
                else if (r == 3)
                    theSeries = 8;
                else if (r == 4)
                    theSeries = 10;
                else if (r == 5)
                    theSeries = 9;
                else if (r == 6)
                    theSeries = 11;
                else
                    continue;
                EEPlayer* theBest = NULL;
                for (unsigned long j = 0; j < thePlayers.size(); j++) {
                    EEPlayer* theOther = this->mPlayers[thePlayers[j]];
                    if (theOther->IsHuman()) {
                        if (theBest == NULL || theBest->mSavedGroups.GetValue(theSeries) < theOther->mSavedGroups.GetValue(theSeries))
                            theBest = theOther;
                    }
                }
                // no human among the recorded players: there is no one to top up against (theBest used to be
                // read through NULL)
                if (theBest == NULL)
                    continue;
                long theBestValue = theBest->mSavedGroups.GetValue(theSeries);
                long theGathered = theBestValue - theBest->mResourceCaps[r] + theBase;
                if (thePlayer != theBest)
                    theBest->mEnemies[thePlayer->GetPlayerID()];
                float theFactor = gPhysics->GetRandom(0.5f, 1.5f);
                unsigned long theValue;
                unsigned long theTarget;
                if (theBestValue < 1) {
                    theValue = thePlayer->mSavedGroups.GetValue(theSeries);
                    theTarget = theValue;
                } else {
                    theValue = UMFloatToLong(theBestValue * theFactor);
                    theTarget = UMFloatToLong(theGathered * theFactor);
                }
                long theExcess = theTarget - theBase - theValue;
                if (theExcess > 0)
                    theValue += theExcess;
                long theSpent = theTarget - UMFloatToLong(gPhysics->GetRandom(0.03f, 0.06f) * theTarget);
                if (thePlayer->mSavedGroups.GetValue(theSeries) < theValue)
                    thePlayer->mSavedGroups.Set(theSeries, theValue);
                else
                    theValue = thePlayer->mSavedGroups.GetValue(theSeries);
                thePlayer->mResourceCaps[r] = theBase - theSpent + theValue;
            }
        } else {
            theRecord.mNameSuffix = gU4E5B98;
        }
        unsigned long theScore = 0;
        long theMilitary = 0;
        long theUnits = 0;
        long theEconomy = 0;
        long theTech = 0;
        long theGathered = 0;
        long theStart = 0;
        if (thePlayer->GetPlayerID() == 0) {
            theScore = 0x7fffffff;
        } else {
            thePlayer->mSavedGroups.CompactEpochHistory();
            for (long s = 0; s < 31; s++) {
                unsigned long theValue = thePlayer->mSavedGroups.GetValue(s);
                unsigned long thePoints = thePlayer->mSavedGroups.GetSeries(s).mPoints.size();
                switch (s) {
                case 1:
                    theEconomy += theValue * -15;
                    break;
                case 2:
                    theEconomy += theValue * 50;
                    break;
                case 3:
                case 5:
                    theEconomy += theValue * 20;
                    break;
                case 4:
                    theEconomy += theValue * -5;
                    break;
                case 7:
                    theGathered += theValue;
                    theMilitary += theValue / 10;
                    theScore += thePlayer->mResourceCaps[kResourceFood];
                    theStart += EEGetResourceLevel(this->mGameOptions.mResources)->mAmounts[0];
                    break;
                case 8:
                    theGathered += theValue;
                    theMilitary += theValue / 10;
                    theScore += thePlayer->mResourceCaps[kResourceWood];
                    theStart += EEGetResourceLevel(this->mGameOptions.mResources)->mAmounts[1];
                    break;
                case 9:
                    theGathered += theValue;
                    theMilitary += theValue >> 3;
                    theScore += thePlayer->mResourceCaps[kResourceGold];
                    theStart += EEGetResourceLevel(this->mGameOptions.mResources)->mAmounts[3];
                    break;
                case 10:
                    theGathered += theValue;
                    theMilitary += theValue / 10;
                    theScore += thePlayer->mResourceCaps[kResourceStone];
                    theStart += EEGetResourceLevel(this->mGameOptions.mResources)->mAmounts[2];
                    break;
                case 11:
                    theGathered += theValue;
                    theMilitary += theValue >> 3;
                    theScore += thePlayer->mResourceCaps[kResourceIron];
                    theStart += EEGetResourceLevel(this->mGameOptions.mResources)->mAmounts[4];
                    break;
                case 12: {
                    unsigned long theCount = thePlayer->mSavedGroups.GetSeries(s).mPoints.size();
                    unsigned long theFirst;
                    if (theCount <= 1)
                        theFirst = 0;
                    else
                        theFirst = (thePlayer->mSavedGroups.GetSeries(s).mPoints.begin() + 1)->mValue;
                    for (unsigned long p = 0; p < thePoints; p++) {
                        unsigned long theEpoch = thePlayer->mSavedGroups.GetSeries(s).mPoints[p].mTime + 1;
                        unsigned long theTime = (thePlayer->mSavedGroups.GetSeries(s).mPoints.begin() + p)->mValue;
                        if (theTime < theFirst)
                            theTime = theFirst;
                        theUnits += (long)((double)(unsigned long)this->mStatistics.mEndTime / theEpoch * (theTime - theFirst) * 100.0 + 0.5);
                    }
                    break;
                }
                case 13:
                    theEconomy += theValue * 10;
                    break;
                case 14:
                    theTech += theValue * 10;
                    break;
                case 15:
                    theTech += thePlayer->mSavedGroups.CountIncreases(s) * 100;
                    break;
                case 16:
                    theTech += theValue * 500;
                    break;
                case 18:
                    theGathered += theValue;
                    break;
                case 19:
                    theTech += theValue * 2;
                    break;
                }
            }
            long theBonus = (long)(theGathered - theScore + theStart) / 4;
            long theFloor = this->mStatistics.mIsScenario ? theScore : 0;
            if (theBonus <= theFloor)
                theBonus = theFloor;
            theMilitary += theBonus;
            theScore = theTech + theEconomy + theUnits + theMilitary;
        }
        theRecord.mScore = theScore;
        theRecord.mEconomyScore = theMilitary;
        theRecord.mEpochScore = theUnits;
        theRecord.mMilitaryScore = theEconomy;
        theRecord.mTechScore = theTech;
        theScores.insert(std::multimap<long, long, std::greater<long> >::value_type(theScore, thePlayers[i]));
        theRecords.insert(std::make_pair(*(thePlayers.begin() + i), theRecord));
    }
    this->mStatistics.mRecords.clear();
    this->mStatistics.mRecords.resize(theScores.size());
    unsigned long theIndex = 0;
    unsigned long theMaxTime = 0;
    std::multimap<long, long, std::greater<long> >::iterator it;
    for (it = theScores.begin(); it != theScores.end(); ++it, theIndex++) {
        EEPlayer* thePlayer = this->mPlayers[it->second];
        EEStatisticsRecord& theSource = (*theRecords.find(it->second)).second;
        thePlayer->mSavedGroups.Set(14, thePlayer->mSavedGroups.GetLastValue(14));
        thePlayer->mSavedGroups.Set(13, thePlayer->mSavedGroups.GetLastValue(13));
        (this->mStatistics.mRecords.begin() + theIndex)->mScore = theSource.mScore;
        (this->mStatistics.mRecords.begin() + theIndex)->mEconomyScore = theSource.mEconomyScore;
        this->mStatistics.mRecords[theIndex].mEpochScore = theSource.mEpochScore;
        this->mStatistics.mRecords[theIndex].mMilitaryScore = theSource.mMilitaryScore;
        this->mStatistics.mRecords[theIndex].mTechScore = theSource.mTechScore;
        this->mStatistics.mRecords[theIndex].mColor = thePlayer->mColor;
        this->mStatistics.mRecords[theIndex].mName = thePlayer->mName + theSource.mNameSuffix;
        this->mStatistics.mRecords[theIndex].mStatistics = thePlayer->mSavedGroups;
        this->mStatistics.mRecords[theIndex].mSlot = thePlayer->mSlot;
        this->mStatistics.mRecords[theIndex].mResult = thePlayer->mGameResult;
        long theTime = thePlayer->mSavedGroups.mEndTime;
        if (theTime == 0)
            theTime = this->mStatistics.mEndTime;
        this->mStatistics.mRecords[theIndex].mEndTime = theTime;
        if (thePlayer->GetPlayerID() == 0)
            this->mStatistics.mRecords[theIndex].mColorIndex = 0;
        else
            this->mStatistics.mRecords[theIndex].mColorIndex = this->mGameOptions.mPlayers[thePlayer->GetPlayerID()].mColor;
        for (unsigned long r = 2; r < 7; r++)
            this->mStatistics.mRecords[theIndex].mResources[r] = thePlayer->mResourceCaps[r];
        if (thePlayer->GetPlayerID() == (unsigned long)this->mCurrentPlayer) {
            this->mStatistics.mRecords[theIndex].mIsLocal = true;
            this->mStatistics.mCurrentRecordIndex = theIndex;
        } else if (theLocalPending) {
            theLocalPending = false;
            this->mStatistics.mRecords[theIndex].mIsLocal = true;
            this->mStatistics.mCurrentRecordIndex = theIndex;
        } else {
            this->mStatistics.mRecords[theIndex].mIsLocal = false;
        }
        if (thePlayer->IsHuman()) {
            this->mStatistics.mRecords[theIndex].mIsHuman = true;
            if (theIndex > 0)
                theHumans++;
            if (theMaxTime < this->mStatistics.mRecords[theIndex].mStatistics.GetValue(0x18))
                theMaxTime = this->mStatistics.mRecords[theIndex].mStatistics.GetValue(0x18);
        } else {
            this->mStatistics.mRecords[theIndex].mIsHuman = false;
        }
    }
    theIndex = 0;
    for (it = theScores.begin(); it != theScores.end(); ++it, theIndex++) {
        EEPlayer* thePlayer = this->mPlayers[it->second];
        if (thePlayer->IsHuman())
            continue;
        float theFactor;
        if (thePlayer->mGameResult == 1)
            theFactor = gPhysics->GetRandom(0.75f, 0.9f);
        else if (thePlayer->mGameResult == 2)
            theFactor = gPhysics->GetRandom(0.6f, 0.75f);
        else
            theFactor = gPhysics->GetRandom(0.6f, 0.9f);
        this->mStatistics.mRecords[theIndex].mStatistics.Set(0x18, (long)(theFactor * theMaxTime + 0.5f));
    }
    if (theHumans > 1 && this->mStatistics.mIsScenario == true)
        this->mStatistics.mIsScenario = false;
}

// The post-game series copy (out of line: EEPlayerStatistics::GetSeries's by-value result):
// TEMPLATE: EXE 0x53b64b
// (compiler-generated ??0EEStatisticsSeries@@QAE@ABU0@@Z; name in analysis/exe/names.txt)

// FUNCTION: EXE 0x53b66c
// Creates an empty statistics record
EEStatisticsRecord::EEStatisticsRecord()
{
    this->Clear();
}

// FUNCTION: EXE 0x53b6ab
// Clears the AI continent data for the world's map
void EEWorld::ClearContinents()
{
    gContinentManager.Clear(this->mMap);
}

// FUNCTION: EXE 0x53b6bc
// Applies the queued unit activations and deactivations to the list of units updated every tick
void EEWorld::UpdateActiveUnits()
{
    std::deque<EEComplexUnit*>::iterator theFound;
    std::deque<EEComplexUnit*>::iterator it;
    std::map<long, EEComplexUnit*>::iterator mt;
    if (this->mActivateQueue.size() != 0) {
        for (it = this->mActivateQueue.begin(); it != this->mActivateQueue.end(); ++it) {
            long theID = (*it)->GetUnitID();
            mt = this->mActiveUnits.find(theID);
            if (mt == this->mActiveUnits.end()) {
                this->mUpdateUnits.push_back(*it);
                (*it)->Reference();
                (*it)->Reference();
                long theKey = (*it)->GetUnitID();
                this->mActiveUnits[theKey] = *it;
            }
            (*it)->Dereference();
        }
        this->mActivateQueue.clear();
    }
    if (this->mDeactivateQueue.size() == 0)
        return;
    for (it = this->mDeactivateQueue.begin(); it != this->mDeactivateQueue.end(); ++it) {
        long theID = (*it)->GetUnitID();
        mt = this->mActiveUnits.find(theID);
        if (mt != this->mActiveUnits.end()) {
            theFound = std::find(this->mUpdateUnits.begin(), this->mUpdateUnits.end(), *it);
            if (theFound != this->mUpdateUnits.end()) {
                (*it)->Dereference();
                (*it)->Dereference();
                this->mUpdateUnits.erase(theFound);
                this->mActiveUnits.erase(mt);
            }
        }
        (*it)->Dereference();
    }
    this->mDeactivateQueue.clear();
}

// FUNCTION: EXE 0x53b8e8
// Queues a unit for activation unless it is NULL, has no id, is a path point or is already queued
void EEWorld::ActivateUnit(EEComplexUnit* inUnit)
{
    if (inUnit == NULL || inUnit->mUnitID == (unsigned long)-1 || inUnit->mUnitClass->mUnitType == kUnitTypePathPoint)
        return;
    std::deque<EEComplexUnit*>::iterator it = std::find(this->mActivateQueue.begin(), this->mActivateQueue.end(), inUnit);
    if (it == this->mActivateQueue.end()) {
        inUnit->Reference();
        this->mActivateQueue.push_back(inUnit);
    }
}

// FUNCTION: EXE 0x53b965
// Queues a unit for deactivation unless it is a path point or is already queued
void EEWorld::DeactivateUnit(EEComplexUnit* inUnit)
{
    if (inUnit->mUnitClass->mUnitType == kUnitTypePathPoint)
        return;
    std::deque<EEComplexUnit*>::iterator it = std::find(this->mDeactivateQueue.begin(), this->mDeactivateQueue.end(), inUnit);
    if (it == this->mDeactivateQueue.end()) {
        this->mDeactivateQueue.push_back(inUnit);
        inUnit->Reference();
    }
}

// FUNCTION: EXE 0x53b9db
// Rebuilds the active unit list from the placed units (first parts; units of inactive classes only when dead),
// dropping the pending activation queues
void EEWorld::RebuildActiveUnits()
{
    EEUnit* theUnit = (EEUnit*)gWorld.mUnits.mFirst;

    for (std::deque<EEComplexUnit*>::iterator it = this->mActivateQueue.begin(); it != this->mActivateQueue.end(); ++it)
        (*it)->Dereference();
    for (it = this->mDeactivateQueue.begin(); it != this->mDeactivateQueue.end(); ++it)
        (*it)->Dereference();
    std::deque<EEComplexUnit*>::iterator ct;
    for (ct = this->mUpdateUnits.begin(); ct != this->mUpdateUnits.end(); ++ct)
        (*ct)->Dereference();

    for (std::map<long, EEComplexUnit*>::iterator mt = this->mActiveUnits.begin(); mt != this->mActiveUnits.end(); ++mt)
        mt->second->Dereference();
    this->mActivateQueue.clear();
    this->mDeactivateQueue.clear();
    this->mUpdateUnits.clear();
    this->mActiveUnits.clear();
    for (; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext)) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        if (theComplex == NULL || theComplex->mPreviousPart != NULL || theComplex->mTileX == -10000
            || theComplex->mUnitClass->mUnitType == kUnitTypePathPoint
            || (theUnit->mUnitClass->mIsInactive && !(theComplex->mHitPoints <= 0)))
            continue;
        this->mUpdateUnits.push_back(theComplex);
        long theKey = theComplex->mUnitID;
        this->mActiveUnits[theKey] = theComplex;
        theComplex->Reference();
        theComplex->Reference();
    }
}

// FUNCTION: EXE 0x53bba0
// Resets the world: clears effects and calamities, gives it a fresh calamity helper, removes every unit and drops the
// scenario and environment managers
void EEWorld::Reset()
{
    this->mEffectManager->ClearEffects();
    USingleton<EECalamityManager>::GetInstance()->DestroyCalamities();
    this->SetCalamityHelper(new EEPCalamityHelper);
    this->RemoveAllUnits();
    delete this->mScenarioManager;
    this->mScenarioManager = NULL;
    delete this->mEnvironmentManager;
    this->mEnvironmentManager = NULL;
    RTSWorld::Reset();
}

// FUNCTION: EXE 0x53bc1f
// Turns every unit's sight off and refreshes the local player's terrain visibility
void EEWorld::RefreshUnits()
{
    for (EEUnit* theUnit = (EEUnit*)this->mUnits.mFirst; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext))
        theUnit->SightOffHere();
    this->GetCurrentPlayer()->RefreshTerrainVisibility();
}

// FUNCTION: EXE 0x53bc4a
// Takes a disconnected player out: flags them disconnected (unless observing), drops them from their teams, marks them
// out and frees their slot
void EEWorld::RemoveDisconnectedPlayer(unsigned long inPlayer)
{
    EEPlayer* thePlayer = this->mPlayers[inPlayer];
    if (!thePlayer->mObserverMode)
        thePlayer->mDisconnected = true;
    this->RemovePlayerFromTeams(inPlayer);
    gWorld.MarkPlayerOut(inPlayer);
    thePlayer->mSlot = kSlotNone;
}

// FUNCTION: EXE 0x53bc81
// Removes a player from every team, re-adding each team's remaining players as a new team
void EEWorld::RemovePlayerFromTeams(unsigned long inPlayer)
{
    std::vector<std::vector<unsigned long> > theTeams;
    std::vector<unsigned long> thePlayers;
    unsigned long i;
    for (i = 0; i < this->mTeams.size(); i++) {
        if (!this->mTeams[i].ContainsPlayer(inPlayer))
            continue;
        this->mTeams[i].RemovePlayer(inPlayer, thePlayers);
        theTeams.push_back(thePlayers);
        this->mTeams.erase(this->mTeams.begin() + i);
        i--;
    }
    for (i = 0; i < theTeams.size(); i++)
        this->AddTeam(theTeams[i]);
    this->TeamsChanged();
}

// FUNCTION: EXE 0x53bd9d
// Undoes the wonder power 4 population share bonus ApplyShareBonus gave or took for every player
void EEWorld::RemoveShareBonus()
{
    for (unsigned long i = 1; i < this->mNumPlayers; i++) {
        EEPlayer* thePlayer = this->mPlayers[i];
        long theBalance = 0;
        if (thePlayer == NULL)
            continue;
        std::vector<long>::iterator itEnd = thePlayer->mWonderPowers.end();
        std::vector<long>::iterator it = thePlayer->mWonderPowers.begin();
        while (true) {
            if (it == itEnd || *it == 4)
                break;
            it++;
        }
        if (it != itEnd) {
            bool theFound = false;
            for (unsigned long j = 1; j < this->mNumPlayers; j++) {
                EEPlayer* theOther = this->mPlayers[j];
                if (j != i && theOther != NULL && thePlayer->mEnemies[j] == 1) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        theFound = true;
                        break;
                    }
                }
            }
            if (theFound)
                continue;
            theBalance = 1;
        } else {
            for (unsigned long j = 1; j < this->mNumPlayers; j++) {
                EEPlayer* theOther = this->mPlayers[j];
                if (j == i || theOther == NULL)
                    continue;
                if (thePlayer->mEnemies[j] == 0) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        if (theBalance < 0)
                            goto next;
                        theBalance = 1;
                    }
                } else if (thePlayer->mEnemies[j] == 1) {
                    std::vector<long>::iterator jtEnd = theOther->mWonderPowers.end();
                    std::vector<long>::iterator jt = theOther->mWonderPowers.begin();
                    while (true) {
                        if (jt == jtEnd || *jt == 4)
                            break;
                        jt++;
                    }
                    if (jt != jtEnd) {
                        if (theBalance > 0) {
                            theBalance = 0;
                            break;
                        }
                        theBalance = -1;
                    }
                }
            }
        }
        if (theBalance > 0)
            thePlayer->mShareBonus -= (long)(gWorldTable.GetFloatValue(0x24) * gWorld.mPlayerPopulationLimit);
        else if (theBalance < 0)
            thePlayer->mShareBonus += (long)(gWorldTable.GetFloatValue2(0x24) * gWorld.mPlayerPopulationLimit);
next:
        ;
    }
}

// FUNCTION: EXE 0x53bf76
// Splits every team holding both players into new teams without each of them
void EEWorld::BreakAlliance(long inPlayer1, long inPlayer2)
{
    std::vector<std::vector<unsigned long> > theTeams;
    std::vector<unsigned long> thePlayers;
    unsigned long i;
    for (i = 0; i < this->mTeams.size(); i++) {
        if (!this->mTeams[i].Contains(inPlayer1, inPlayer2))
            continue;
        this->mTeams[i].RemovePlayer(inPlayer1, thePlayers);
        theTeams.push_back(thePlayers);
        this->mTeams[i].RemovePlayer(inPlayer2, thePlayers);
        theTeams.push_back(thePlayers);
        this->mTeams.erase(this->mTeams.begin() + i);
        i--;
    }
    for (i = 0; i < theTeams.size(); i++)
        this->AddTeam(theTeams[i]);
    this->TeamsChanged();
}

// FUNCTION: EXE 0x53c0cb
// Lets every player finish loading its units after a saved game was read
void EEWorld::NotifyPlayersLoaded(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    for (unsigned long i = 0; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL)
            this->mPlayers[i]->FinishUnitsLoad(inMajorVersion, inMinorVersion);
    }
}

// FUNCTION: EXE 0x53c105
// Moves a unit to its place in the world's unit list, which is sorted by tile x
void EEWorld::ResortUnit(RTSUnit* inUnit)
{
    EEComplexUnit* theComplex = static_cast<EEUnit*>(inUnit)->mComplexUnit;
    if (theComplex == NULL)
        return;
    EEUnit* thePrev = static_cast<EEUnit*>(theComplex->mTileNode->mPrev);
    RTSDummyUnit* theDummy = dynamic_cast<RTSDummyUnit*>(thePrev);
    EEUnit* theNext = static_cast<EEUnit*>(theComplex->mTileNode->mNext);
    if (theDummy != NULL && theNext == NULL)
        return;
    theComplex->Unlink();
    EEUnit* theAt;
    if (theDummy != NULL) {
        theAt = theNext;
    } else {
        theAt = thePrev;
        if (theNext == NULL)
            theNext = thePrev;
    }
    RTSDummyUnit* theStop = NULL;
    while (theNext != NULL) {
        // one bracket test (inUnit's x between theAt's and theNext's), then a step back or forward
        if (theAt->mTileX <= theComplex->mTileX && theNext->mTileX >= theComplex->mTileX)
            break;
        if (theAt->mTileX > theComplex->mTileX) {
            theNext = theAt;
            theAt = static_cast<EEUnit*>(theAt->mTileNode->mPrev);
            theStop = dynamic_cast<RTSDummyUnit*>(theAt);
        } else {
            theAt = theNext;
            theNext = static_cast<EEUnit*>(theNext->mTileNode->mNext);
        }
        if (theStop != NULL)
            break;
    }
    theComplex->Link(theAt);
}

// FUNCTION: EXE 0x53c1ad
// Replaces the calamity helper, deleting the old one unless it is the same object
void EEWorld::SetCalamityHelper(FSPersistent* inObject)
{
    if (inObject != this->mCalamityHelper) {
        delete this->mCalamityHelper;
        this->mCalamityHelper = inObject;
    }
}

// FUNCTION: EXE 0x53c1d5
// Moves the camera unless a scenario camera move is running
void EEWorld::SetCameraTransformation(const GETransformation& inTransformation)
{
    this->SetCamera(inTransformation, false);
}

// FUNCTION: EXE 0x53c1e3
// Replaces the environment (weather) manager, deleting the old one
void EEWorld::SetEnvironmentManager(EEEnvironmentManager* inManager)
{
    delete this->mEnvironmentManager;
    this->mEnvironmentManager = inManager;
}

// FUNCTION: EXE 0x53c201
// Replaces the influence table, deleting the old one
void EEWorld::SetInfluenceTable(EEInfluenceTable* inTable)
{
    delete this->mInfluenceTable;
    this->mInfluenceTable = inTable;
}

// FUNCTION: EXE 0x53c21e
// Sets the population limit and recomputes every player's share bonus and civilization population bonus
void EEWorld::SetPopulationLimit(unsigned long inLimit)
{
    RTSWorld::SetPopulationLimit(inLimit);
    for (long i = 1; i < 17; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->mShareBonus = 0;
            this->mPlayers[i]->mCivilization.ApplyLevel21ToWorld();
        }
    }
    gWorld.ApplyShareBonus();
}

// FUNCTION: EXE 0x53c260
// Sets the player count and recomputes every player's share bonus and civilization population bonus
void EEWorld::SetNumPlayers(unsigned long inNumPlayers)
{
    RTSWorld::SetNumPlayers(inNumPlayers);
    for (long i = 1; i < 17; i++) {
        if (this->mPlayers[i] != NULL) {
            this->mPlayers[i]->mShareBonus = 0;
            this->mPlayers[i]->mCivilization.ApplyLevel21ToWorld();
        }
    }
    gWorld.ApplyShareBonus();
}

// FUNCTION: EXE 0x53c2a2
// Puts a player into observer mode and marks their game options entry as out (type 2)
void EEWorld::MarkPlayerOut(unsigned long inPlayer)
{
    EEGameOptionsPlayer thePlayer;
    this->mPlayers[inPlayer]->mObserverMode = true;
    if (inPlayer > 0)
        thePlayer = gWorld.mGameOptions.mPlayers[inPlayer - 1];
    thePlayer.mType = 2;
    gWorld.mGameOptions.mPlayers[inPlayer - 1] = thePlayer;
}

// FUNCTION: EXE 0x53c301
// Sets the time of day and recomputes the sun and moon light directions
void EEWorld::SetTimeOfDay(float inTimeOfDay)
{
    GETransformation theTransformation;
    GEVector theDown(-1.0f, 0.0f, 0.0f);
    this->mTimeOfDay = inTimeOfDay;
    GEVector theSun(0.0f, 0.0f, 0.0f);
    GEVector theMoon(0.0f, 0.0f, 0.0f);
    GEVector theSun2(0.0f, 0.0f, 0.0f);
    GEVector theMoon2(0.0f, 0.0f, 0.0f);
    theTransformation.SetOrientationYPR(0.0f, 0.0f, inTimeOfDay * 6.2831855f);
    theTransformation.Apply(theDown, theSun);
    theMoon.SetX(theSun.GetX() * -1.0f);
    theMoon.SetY(theSun.GetY() * -1.0f);
    theMoon.SetZ(theSun.GetZ() * -1.0f);
    theTransformation.SetOrientationYPR(0.0f, kWorldSunTilt, 0.0f);
    theTransformation.Apply(theSun, theSun2);
    theTransformation.Apply(theMoon, theMoon2);
    theTransformation.SetOrientationYPR(0.78539819f, 0.0f, 0.0f);
    theTransformation.Apply(theSun2, this->mSunDirection);
    theTransformation.Apply(theMoon2, this->mMoonDirection);
}

// FUNCTION: EXE 0x53c445
// Places the queued units: adds them to their tiles, the minimap, regions and influence table, links them into the
// x-sorted unit list and finds their contacts
void EEWorld::PlaceQueuedUnits()
{
    unsigned long theCount = this->mQueuedPlacements.size();
    for (unsigned long i = 0; i < theCount; i++) {
        EEUnit* theUnit = static_cast<EEUnit*>(this->mQueuedPlacements[i].mUnit);
        RTSUnit* theAt = this->mQueuedPlacements[i].mInsertAfter;
        long theX = theUnit->mTileX;
        theUnit->mUnitClass->AddToTile(theUnit);
        gMiniMap->MoveUnit(theUnit, -10000, -10000, theUnit->mTileX, theUnit->mTileY);
        if (theUnit->mTileNode == NULL)
            continue;
        if (theUnit->mTile != NULL) {
            theUnit->mUnitClass->AddToRegion(theUnit);
            if (theUnit->mUnitClass->mInfluence != 0)
                this->mInfluenceTable->AddUnit(theUnit, theUnit->mTileX, theUnit->mTileY);
        }
        if (theAt == NULL) {
            RTSUnit* theHead = reinterpret_cast<RTSUnit*>(&this->mUnits);
            for (RTSUnit* theNode = (RTSUnit*)this->mUnits.mFirst; theNode != NULL && theX > theNode->mTileX;
                 theNode = theNode->mTileNode->mNext)
                theHead = theNode;
            theAt = theHead;
        } else if (theX != theAt->mTileX) {
            if (theX < theAt->mTileX) {
                theAt = theAt->mTileNode->mPrev;
                bool theDone = false;
                while (!theDone) {
                    if (dynamic_cast<RTSDummyUnit*>(theAt) == 0 && theX < theAt->mTileX)
                        theAt = theAt->mTileNode->mPrev;
                    else
                        theDone = true;
                }
            } else {
                while (theAt->mTileNode->mNext != NULL && theX > theAt->mTileNode->mNext->mTileX)
                    theAt = theAt->mTileNode->mNext;
            }
        }
        theUnit->Link(theAt);
        if (theUnit->mHitPoints > 0 && static_cast<EEComplexUnit*>(theUnit)->mIsCollidable != 0) {
            static_cast<EEComplexUnit*>(theUnit)->ClearContacts();
            static_cast<EEComplexUnit*>(theUnit)->FindContacts();
        }
    }
}

// FUNCTION: EXE 0x53c5ad
// Moves a player to a new lobby slot and name, handing its old slot and name to the slot's previous holder; switches
// the local view to it when it takes the local slot
void EEWorld::MovePlayerToSlot(unsigned char inSlot, char inNewSlot, unsigned char inPlayer, UWideString& inName, long inUnused)
{
    EEComplexUnit* theSelected = NULL;
    EEPlayer* theOld = gWorld.GetPlayerBySlot(inSlot);
    UWideString theName;
    unsigned long thePlayerIndex = inPlayer;
    if (!(thePlayerIndex < gWorld.mNumPlayers))
        return;
    EEPlayer* thePlayer = gWorld.GetPlayer(thePlayerIndex);
    char theSlot = thePlayer->mSlot;
    if (thePlayer->IsHuman())
        theName = static_cast<EEHumanPlayer*>(thePlayer)->GetName();
    thePlayer->mSlot = inNewSlot;
    if (thePlayer->IsHuman())
        static_cast<EEHumanPlayer*>(thePlayer)->SetName(inName);
    if (thePlayer->mSlot == gWorld.GetCurrentPlayer()->mSlot) {
        for (EEUnit* theUnit = gWorld.mPlayers[thePlayerIndex]->mFirstUnit;
             theUnit != NULL && theUnit->mUnitClass->mUnitType != kUnitTypeTownCenter; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mTile != NULL || (theUnit->mComplexUnit != NULL && theUnit->mComplexUnit->mContainer != NULL))
                theSelected = static_cast<EEComplexUnit*>(theUnit);
        }
        gWorld.mCurrentPlayer = thePlayerIndex;
        gMiniMap->mViewingPlayer = gWorld.mPlayers[thePlayerIndex];
        gMiniMap->Redraw();
        gMiniMap->Refresh();
        thePlayer->RefreshTerrainVisibility();
        thePlayer->GetTechTree()->UpdateInterface(true);
        gUserInterface->PostUpdateBackgroundMedia();
        gCameraFollowing = false;
    }
    theOld->mSlot = theSlot;
    if (gCommunications->IsServer())
        (static_cast<EEServer*>(gCommunications))->mFastSelectionID = static_cast<EEServer*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->GetPlayerID()];
    else
        (static_cast<EEClient*>(gCommunications))->mFastSelectionID = static_cast<EEClient*>(gCommunications)->mFastSelection[(unsigned char)thePlayer->GetPlayerID()];
    if (theOld->IsHuman())
        static_cast<EEHumanPlayer*>(theOld)->SetName(theName);
}

// A file-scope global defined mid-file (its init chain sits between MovePlayerToSlot and UnloadMedia; no reader):
// SYNTHETIC: EXE 0x53c740   (wrapper)
// SYNTHETIC: EXE 0x53c74a   (ctor)
// SYNTHETIC: EXE 0x53c75b   (registrar)
// SYNTHETIC: EXE 0x53c767   (dtor)
UString gDefaultCampaignUser("default");

// FUNCTION: EXE 0x53c772
// Unloads the per-player foundation media and the shared effects, button sounds and light glow model of the game
void EEWorld::UnloadMedia()
{
    if (this->mMediaLoaded != true)
        return;
    EEPlayer** thePlayers = &this->mPlayers[1];
    for (long n = 16; n != 0; n--) {
        if (*thePlayers != NULL) {
            (*thePlayers)->mUnitClassUpgrades[kUnitTypeFoundation1x1]->UnloadMedia(*thePlayers);
            (*thePlayers)->mUnitClassUpgrades[kUnitTypeFoundation2x2]->UnloadMedia(*thePlayers);
            (*thePlayers)->mUnitClassUpgrades[kUnitTypeFoundation3x3]->UnloadMedia(*thePlayers);
            (*thePlayers)->mUnitClassUpgrades[0x168]->UnloadMedia(*thePlayers);
            (*thePlayers)->mUnitClassUpgrades[0x2cd]->UnloadMedia(*thePlayers);
            (*thePlayers)->mUnitClassUpgrades[0x20]->UnloadMedia(*thePlayers);
            (*((*thePlayers)->mUnitClassUpgrades.begin() + 0x1e0))->UnloadMedia(*thePlayers);
        }
        thePlayers++;
    }
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationMove)->Unload();
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationAttack)->Unload();
    EEGraphicEffect::Get(kGraphicEffectCommandConfirmationGatherOther)->Unload();
    EEGraphicEffect::Get(kGraphicEffectDisbandUnitEmitter)->Unload();
    EEGraphicEffect::Get(kGraphicEffectBasicMarchingLine)->Unload();
    EEGraphicEffect::Get(kGraphicEffectWaypointIndicator)->Unload();
    EEGraphicEffect::Get(kGraphicEffectRallyFlagBuilding)->Unload();
    EEGraphicEffect::Get(kGraphicEffectHorseBreathUnits)->Unload();
    EESoundObject::Get(kSoundButton1)->UnloadMedia();
    EESoundObject::Get(kSoundButton2)->UnloadMedia();
    EEE2DLightGlow::ReleaseModel(true);
    this->mMediaLoaded = false;
}

// FUNCTION: EXE 0x53c964
// Unpauses a running single-player game on the server; always returns 0
long EEWorld::UnpauseGame()
{
    if (!gCommunications->IsServer() || !(gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) || gCommunications->GetIsMultiplayerSession())
        return 0;
    EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
    theServer->UnpauseGame();
    gUserInterface->PostPauseInformation(false);
    return 0;
}

// The game result entries (player, result) the end-of-game check collects (8 bytes; its vector's COMDATs are
// ICF-folded with vector<EETileCoordinate>'s; provisional name).
struct EEWorldGameResult {
    unsigned long mPlayer;
    long mResult;
};

// FUNCTION: EXE 0x53c9b4
// Ends the game when decided: applies team victories, defeats players out of units and lets a last team win; returns
// 2 when someone won, 1 when no human (or only one player) is left, else 0
long EEWorld::CheckGameOver()
{
    std::vector<EEWorldGameResult> theResults;
    EEGameOptionsPlayer theUnused;
    unsigned long i = 0;
    long theResult = 0;
    long theHumans = 0;
    if (this->mVictoryEnabled) {
        for (i = 0; i < this->mTeams.size(); i++) {
            long theState = this->mTeams[i].GetVictoryState();
            if (theState != 0) {
                theResult = 0;
                for (unsigned long j = 0; j < (this->mTeams.begin() + i)->mPlayers.size(); j++) {
                    EEWorldGameResult theEntry;
                    theEntry.mPlayer = (this->mTeams.begin() + i)->mPlayers[j];
                    theEntry.mResult = theState;
                    theResults.push_back(theEntry);
                }
                theResult = 2;
            }
        }
        if (theResult != 0)
            goto results;
    }
    i = 1;
    if (this->mNumPlayers < 2)
        goto alone;
    for (; i < this->mNumPlayers; i++) {
        EEPlayer* thePlayer = this->mPlayers[i];
        if (thePlayer != NULL) {
            if (!thePlayer->mObserverMode) {
                if (this->mVictoryEnabled && thePlayer->mEndOfGameCount <= 0) {
                    this->RemovePlayerFromGame(false, i);
                    thePlayer->SetGameResult(2);
                }
                if (this->mPlayers[i]->IsHuman())
                    theHumans++;
            }
            if (this->mPlayers[i]->mGameResult == 1)
                theResult = 2;
        }
    }
    if (theResult == 0) {
        if (theHumans == 0)
            goto alone;
        if (!this->mVictoryEnabled || this->mTeams.size() > 1 || this->mTeams.size() == 0)
            goto done;
        theResult = 2;
        for (i = 0; i < this->mTeams.begin()->GetNumPlayers(); i++) {
            EEWorldGameResult theEntry;
            theEntry.mPlayer = *(this->mTeams.begin()->mPlayers.begin() + i);
            theEntry.mResult = 1;
            theResults.push_back(theEntry);
        }
    }
    goto results;
alone:
    theResult = 1;
    for (i = 1; i < this->mNumPlayers; i++) {
        if (this->mPlayers[i] != NULL && this->mPlayers[i]->mGameResult == 1) {
            theResult = 2;
            break;
        }
    }
results:
    for (i = 0; i < theResults.size(); i++) {
        if (this->mPlayers[theResults[i].mPlayer] != NULL && this->mPlayers[theResults[i].mPlayer]->mGameResult == 0) {
            this->RemovePlayerFromGame(theResults[i].mResult == 1, theResults[i].mPlayer);
            this->mPlayers[theResults[i].mPlayer]->SetGameResult(theResults[i].mResult);
        }
    }
    for (i = 1; i < this->GetNumPlayers(); i++) {
        if (this->mPlayers[i] != NULL && this->mPlayers[i]->mGameResult == 0) {
            this->RemovePlayerFromGame(theResult == 1, i);
            this->mPlayers[i]->SetGameResult(theResult);
        }
    }
done:
    return theResult;
}

// FUNCTION: EXE 0x53cdb9
// Updates the physics timing after the game speed changed
void EEWorld::GameSpeedChanged()
{
    gPhysics->UpdateTiming();
}

// EETeam's ctors: COMDATs here after their first users (each news the team's EEVCWonder)
// FUNCTION: EXE 0x53d013
// Creates an empty team with a wonder victory condition
EETeam::EETeam()
{
    this->mVictoryCondition = new EEVCWonder(this);
}

// FUNCTION: EXE 0x53d076
// Creates a team of two players with a wonder victory condition
EETeam::EETeam(long inPlayer1, long inPlayer2)
{
    this->AddPlayer(inPlayer1);
    this->AddPlayer(inPlayer2);
    this->mVictoryCondition = new EEVCWonder(this);
}

// FUNCTION: EXE 0x53d0d3
// Creates a team of the given players with a wonder victory condition
EETeam::EETeam(const std::vector<unsigned long>& inPlayers)
{
    this->mPlayers = inPlayers;
    this->mVictoryCondition = new EEVCWonder(this);
}

// The unit's template COMDATs and refused inline copies (0x13cdc4..0x13e967; names from range pairing):
// TEMPLATE: EXE 0x53cdc4
// TEMPLATE: EXE 0x53cdf0
// TEMPLATE: EXE 0x53ce3e
// TEMPLATE: EXE 0x53ce79
// TEMPLATE: EXE 0x53cf3f
// TEMPLATE: EXE 0x53cfde
// TEMPLATE: EXE 0x53cff7
// TEMPLATE: EXE 0x53d194
// TEMPLATE: EXE 0x53d50b
// TEMPLATE: EXE 0x53d5fd
// TEMPLATE: EXE 0x53d6e9
// TEMPLATE: EXE 0x53d75a
// TEMPLATE: EXE 0x53dc16
// TEMPLATE: EXE 0x53dc48
// TEMPLATE: EXE 0x53dc5a
// TEMPLATE: EXE 0x53dc94
// TEMPLATE: EXE 0x53dcfe
// TEMPLATE: EXE 0x53e056
// TEMPLATE: EXE 0x53e096
// TEMPLATE: EXE 0x53e109
// TEMPLATE: EXE 0x53e14e
// TEMPLATE: EXE 0x53e195
// TEMPLATE: EXE 0x53e22a
// TEMPLATE: EXE 0x53e26e
// TEMPLATE: EXE 0x53e2ea
// TEMPLATE: EXE 0x53e316
// TEMPLATE: EXE 0x53e342
// TEMPLATE: EXE 0x53e35f
// TEMPLATE: EXE 0x53e410
// TEMPLATE: EXE 0x53e42d
// TEMPLATE: EXE 0x53e558

// TEMPLATE: EXE 0x5388c6
// TEMPLATE: EXE 0x538e4b
// TEMPLATE: EXE 0x53cea4
// TEMPLATE: EXE 0x53cecc
// TEMPLATE: EXE 0x53cee0
// TEMPLATE: EXE 0x53cf13
// TEMPLATE: EXE 0x53cfb2
// TEMPLATE: EXE 0x53d12a
// TEMPLATE: EXE 0x53d155
// TEMPLATE: EXE 0x53d169
// TEMPLATE: EXE 0x53d3ff
// TEMPLATE: EXE 0x53d58c
// TEMPLATE: EXE 0x53d657
// TEMPLATE: EXE 0x53dccc

// TEMPLATE: EXE 0x53e2a7
// ?copy@std@@YA?AV?$_Ptrit@VEETeam@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z
// TEMPLATE: EXE 0x53e831
// ??0EETeam@@QAE@ABV0@@Z (EETeam.h inline copy ctor)
// TEMPLATE: EXE 0x53e88c
// ?_Ufill@?$vector@VEETeam@@V?$allocator@VEETeam@@@std@@@std@@IAEPAVEETeam@@PAV3@IABV3@@Z
// TEMPLATE: EXE 0x53e8b7
// ?_Ucopy@?$vector@VEETeam@@V?$allocator@VEETeam@@@std@@@std@@IAEPAVEETeam@@V?$_Ptrit@VEETeam@@HPAV1@AAV1@PAV1@AAV1@@2@0PAV3@@Z
// TEMPLATE: EXE 0x53e8e3
// ?fill@std@@YAXV?$_Ptrit@VEETeam@@HPAV1@AAV1@PAV1@AAV1@@1@0ABVEETeam@@@Z
// TEMPLATE: EXE 0x53e912
// ?copy_backward@std@@YA?AV?$_Ptrit@VEETeam@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z
// TEMPLATE: EXE 0x53e955
// ?_Construct@std@@YAXPAVEETeam@@ABV2@@Z

// RecordStatistics' vector<EEStatisticsRecord> / map<unsigned long, EEStatisticsRecord> COMDATs
// TEMPLATE: EXE 0x53ce1c
// TEMPLATE: EXE 0x53d250
// TEMPLATE: EXE 0x53d44f
// TEMPLATE: EXE 0x53d577
// TEMPLATE: EXE 0x53d91c
// TEMPLATE: EXE 0x53e0da
// TEMPLATE: EXE 0x53e1e5
// TEMPLATE: EXE 0x53e213
// TEMPLATE: EXE 0x53e38b
// TEMPLATE: EXE 0x53e3bf
// TEMPLATE: EXE 0x53e3df
// TEMPLATE: EXE 0x53e43f
// TEMPLATE: EXE 0x53d3e3
// TEMPLATE: EXE 0x53d28a
// TEMPLATE: EXE 0x53d36d
// TEMPLATE: EXE 0x53e451
// TEMPLATE: EXE 0x53d059
