// C:\EEX\Empire Earth\UI Events\EEUIEMainGameEvents.cpp (__FILE__ 0x4cd3b0, ESVERIFY line 2006). The main game form's
// events (class prefix EEUIEMG): 0x25fdb3..0x260c27, after EEUICWonNetStatBox.cpp and before EEUIFError.cpp (the file
// sorts inside the User Interface Thread group). The messages' inline ctors are COMDATs after their first users.
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

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
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
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
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UIBlinkingButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEPhysics.h"
#include "ee/EEServer.h"
#include "ee/EEAction.h"
#include "ee/EEAUserPauseGame.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIManager.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"   // the in-game options menu's open event (main_game_options.rs)
#include "ee/EEGamePreferences.h"
#include "ee/EEMouse.h"
#include "ee/EECameraControl.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnit.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIFMGUnitHack.h"
#include "ee/EEUIManagerEvents.h"
#include "ee/EEUIEMGHotKeyStatusButton.h"
#include "ee/EEUIEMainGameEvents.h"
#include "lle/NEProtocol.h"
#include "ee/EERenderObject.h"
#include "ee/EEStringIDs.h"
// EEFn13C5AD is EEWorld::MovePlayerToSlot (ee/EEWorld.h)

// FUNCTION: EXE 0x65fdb3
// Cancels the current main-game mode (placing buildings, gates, flares ...): clears ghost units, the pointer and the
// cursor lock and hides the cancel button
long EEUIEMGCancelUIMode::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    if (theForm->mUIState != 0xb) {
        if (gWorld.GetNumGhostUnits() > 0) {
            gWorld.mGhostUnitsLock->Enter();
            gWorld.ClearGhostUnits();
            gWorld.mGhostUnitsLock->Leave();
        }
    } else
        theForm->mBuildGate.ClearGhostUnits();
    theForm->SetUIState(0);
    gMouse->mPointer = 0;
    theForm->mIsCursorLocked = false;
    theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeHidden);
    if (gUIManager->mMainGameForm)
        gUIManager->mMainGameForm->mRefreshGroup = true;
    return kErrorOK;
}

// FUNCTION: EXE 0x65fe4d
// Writes the loaded media (and leaks when asked) to Media Dump.txt in the startup folder, optionally replacing it
long EEUIEMGDumpLoadedMedia::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFile.SetFile("Media Dump.txt");
    if (this->mFlag)
        theFile.Delete();
    theFile.Create();
    long theResult = theFile.FOpen("a+");
    if (theResult == 0) {
        gUserInterface->AddChatLogEntry("Dumped Media Information.", theResult);
        gWorld.DumpMedia(theFile, this->mDumpLeaks);
        theFile.Close();
    }
    return theResult;
}

// FUNCTION: EXE 0x65ff23
// Asks the simulation to show the end-of-scenario dialog
long EEUIEMGEndScenarioDialog::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return gPhysics->PostShowDialogMessage();
}

// FUNCTION: EXE 0x65ff31
// Shows the statistics group on the main game form
long EEUIEMGDisplayStatistics::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->mCurrentGroup = 1;
    theForm->SetEntityGroup(2);
    theForm->FnE123D();
    return kErrorOK;
}

// FUNCTION: EXE 0x65ff56
// Posts the request to open the civilization builder; returns the post's result
// it returns Post's result (PostDisplayCivBuilder 0x27947d)
long EEUIEMGDisplayCivBuilder::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return gUserInterface->PostDisplayCivBuilder();
}

// FUNCTION: EXE 0x65ff64
// Switches to the chat form, if there is a current player
long EEUIEMGDisplayChat::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWorld.GetCurrentPlayer())
        gUserInterface->SwitchToForm(gUIManager->GetFormByID(kFormChat));
    return kErrorOK;
}

// FUNCTION: EXE 0x65ff92
// Switches to the diplomacy form, if there is a current player
long EEUIEMGDisplayDiplomacy::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gWorld.GetCurrentPlayer())
        gUserInterface->SwitchToForm(gUIManager->GetFormByID(kFormDiplomacy));
    return kErrorOK;
}

// FUNCTION: EXE 0x65ffc0
// Switches to the in-game options form, with the event that sets it up when it opens
long EEUIEMGDisplayOptions::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_GO_OPEN);
    inForm->GetManager()->GetFormByID(kFormGameOptions)->SetFormOpenEvent(theEvent);
    theEvent->Dereference();
    gUserInterface->SwitchToForm(inForm->GetManager()->GetFormByID(kFormGameOptions));
    return kErrorOK;
}

// FUNCTION: EXE 0x660051
// Sends the enter-shift-mode message at the current game time
long EEUIEMGEnterShiftMode::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMEnterShiftMode(gPhysics->mGameTime), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x66008a
// Creates the message that enters shift mode at a game time
// (EEMEnterShiftMode's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x6600b7
// Sends the exit-shift-mode message at the current game time
long EEUIEMGExitShiftMode::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gCommunications->ProcessLocalUserInput(new EEMExitShiftMode(gPhysics->mGameTime), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x6600f0
// Enters flare mode on the main game form and shows the cancel button
long EEUIEMGFlareMode::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    theForm->SetUIState(0x12);
    theForm->GetControl(kMainGameCancelUIMode)->SetDisplayMode(kDisplayModeShown);
    if (gUIManager->mMainGameForm)
        gUIManager->mMainGameForm->mRefreshGroup = true;
    return kErrorOK;
}

// FUNCTION: EXE 0x66012d
// Sends the rally-timer increment once if an airport is among the selected units
long EEUIEMGIncrementRallyTimer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
        EEComplexUnit* theUnit = theGroup->GetUnit(i);
        if (theUnit && theUnit->mUnitClass->mUnitType == kUnitTypeAirport) {
            gCommunications->ProcessLocalUserInput(new EEMIncrementRallyTimer(this->mTimer), kSlotNone);
            break;
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x6601ad
// Creates the message that increments an airport's rally timer
// (EEMIncrementRallyTimer's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x6601da
// Presses or releases a status button by hot key (pressing also resets the saved-group state)
long EEUIEMGHotKeyStatusButton::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (!this->mButton)
        return kErrorOK;
    if (this->mPressed) {
        this->mButton->HotKeyDown();
        gWorld.GetCurrentPlayer()->mSavedGroups.Reset(0x14, 1);
    } else
        this->mButton->HotKeyUp();
    return kErrorOK;
}

// FUNCTION: EXE 0x660215
// Returns from a single-player test game to the scenario editor: clears ghost units, moves the player back to the
// editor slot, stops the simulation and posts the editor switch and pause information
long EEUIEMGEnterScenarioEditor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    NEProtocol* theProtocol = NULL;
    long theResult = 0;
    if (gCommunications->IsServer() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0
        && gCommunications->GetIsMultiplayerSession() == false
        && (gUIManager->mInScenarioEditor || gUIManager->mMainGameForm->mHotKeyMode > 0)) {
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        NEProtocol::GetProtocol(0, theProtocol);
        gWorld.MovePlayerToSlot(0xfe, gWorld.GetCurrentPlayer()->GetSlot(), 1, theProtocol->GetLocalAddress(gNetworkAdapter), 0);
        gPhysics->mThread->Post(0x402, NULL, NULL, true);
        theResult = gUserInterface->PostEnterScenarioEditor(gUIManager->GetInScenarioEditor());
        gUserInterface->PostPauseInformation(true);
    }
    return theResult;
}

// FUNCTION: EXE 0x66031d
// Quick-saves (timestamped name, sent to all players) or quick-/auto-loads in single player, as the host; tells
// clients that quick save and load are host only
long EEUIEMGInGameLoadAndSave::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theName;
    UWideString theMessage;
    TMTime theTime;
    if (gCommunications->IsServer() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) {
        NESynchronousServer* theServer = dynamic_cast<NESynchronousServer*>(gCommunications);
        if (this->mMode == 0) {
            TMGetTimeOfDay(theTime);
            sprintf(theName, "%s %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds%s", "QuickSave", theTime.mYear, theTime.mMonth + 1,
                    theTime.mDay, theTime.mHour, theTime.mMinute, theTime.mSecond, (const char*)gSavedGameExtension);
            gGamePreferences.mSettings.mLastQuickSave = theName;
            gGamePreferences.Save();
            theServer->SendToAllAndWait(new EEMLoadOrSaveGame(theName, TMGetTime(), 0, 1,
                                                              gCommunications->GetNumberOfCTsProcessedInPhysics(), 0),
                                        15000, 12);
        } else {
            if (this->mMode == 1)
                theName = gGamePreferences.mSettings.mLastQuickSave;
            else
                theName = gGamePreferences.mSettings.mLastAutoSave;
            if (gCommunications->GetIsMultiplayerSession() == false) {
                FSFileSpec* theFile = new FSFileSpec;
                long theResult = EEOpenSaveGameFile((const char*)theName, "Saved Games\\", theFile);
                if (theResult == kErrorOK) {
                    theFile->SetFile((const char*)theName);
                    gUserInterface->PostLoadSavedGame(*theFile);
                } else {
                    gResource->GetString(IDS_FILE_NAME_NOT_FOUND, theMessage);
                    gUIManager->DisplayMessage(theMessage, 1, NULL, 0, true);
                }
                if (theFile)
                    delete theFile;
            } else {
                gResource->GetString(IDS_QUICK_LOAD_IS_FOR_SINGLE_PLAYER, theMessage);
                gUserInterface->AddChatLogEntry(theMessage, 0);
            }
        }
    } else if (gCommunications->IsClient() && gCommunications->GetNumberOfCTsProcessedInPhysics() > 0) {
        gResource->GetString(IDS_QUICK_SAVE_AND_LOAD_ARE_ONLY, theMessage);
        gUserInterface->AddChatLogEntry(theMessage, 0);
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x660566
// Posts the user-pause action, as the host of a running game
long EEUIEMGPauseGame::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gCommunications->GetNumberOfCTsProcessedInPhysics() > 0 && gCommunications->IsServer()) {
        EEAction* theAction = new EEAUserPauseGame;
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x6605ba
// Cancels the current main-game mode (clearing ghost units and the pointer); with no mode, dismisses a showing
// scenario message, else opens the in-game menu (divergence: the original only posted the scenario-dialog message,
// so Escape never opened the menu; the menu's own Escape hot key closes it again, see EEUIEMGOEOpenOptionsScreen)
long EEUIEMGCancelUpdateState::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    if (theForm->mUIState != 0) {
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        theForm->SetUIState(0);
        gMouse->mPointer = 0;
        theForm->mIsCursorLocked = false;
        return kErrorOK;
    }
    UIControl* theMessage = theForm->GetControl(kMainGameDialogMessage);
    if ((theMessage != NULL && theMessage->GetDisplayMode() == kDisplayModeShown) || gWorld.mInCinematic)
        return gPhysics->PostShowDialogMessage();
    UIEvent* theOpen = new EEUIEMGDisplayOptions;
    long theResult = theOpen->ProcessEvent(inForm, inEntity);
    theOpen->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x66061c
// Cycles the main game clock display through its three modes
long EEUIEMGToggleGameClock::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMainGame* theForm = static_cast<EEUIFMainGame*>(inForm);
    switch (theForm->mClockMode) {
    case 1:
        theForm->mClockMode = 2;
        break;
    case 2:
        theForm->mClockMode = 0;
        break;
    default:
        theForm->mClockMode = 1;
        break;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x660650
// Sends the kill-unit cheat message with the event's flag; returns its result
long EEUIEMGKillUnits::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return gCommunications->ProcessLocalUserInput(new EEMKillUnit(this->mFlag), kSlotNone);
}

// FUNCTION: EXE 0x660685
// Creates the kill-unit message
// (EEMKillUnit's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x6606b2
// Sends the attack-players cheat message for the current player, single player only; returns its result
long EEUIEMGAttackPlayer::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    long theResult = 0;
    if (gCommunications->GetIsMultiplayerSession() == false)
        theResult = gCommunications->ProcessLocalUserInput(new EEMAttackPlayers(gWorld.mCurrentPlayer, this->mFlag), kSlotNone);
    return theResult;
}

// FUNCTION: EXE 0x6606fb
// Creates the attack-players message
// (EEMAttackPlayers' inline ctor, out of line after its first user)

// FUNCTION: EXE 0x66072f
// Cycles the hidden-units mode (normal with every unit marked moved, buildings transparent, buildings opaque) and
// reports the new mode in the chat log
long EEUIEMGToggleShowHiddenUnits::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    if (gHiddenUnitsMode == 0) {
        gHiddenUnitsMode = 1;
        gResource->GetString(IDS_NORMAL_HIDDEN_UNITS_MODE, theText);
        for (EEUnit* theUnit = (EEUnit*)gWorld.mUnits.mFirst; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex && theComplex->mCanBeHidden)
                theComplex->mHiddenFlags |= kHiddenFlagMoved;
        }
    } else if (gHiddenUnitsMode == 1) {
        gHiddenUnitsMode = 2;
        gResource->GetString(IDS_BUILDINGS_ALWAYS_TRANSPARENT, theText);
    } else {
        gHiddenUnitsMode = 0;
        gResource->GetString(IDS_BUILDINGS_ALWAYS_OPAQUE, theText);
    }
    gUserInterface->AddChatLogEntry(theText, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x6607e0
// Makes the camera follow the current player's lead unit, or stops following
long EEUIEMGFollowUnit::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUnit* theUnit = gWorld.GetUnit(EEGetLeadUnitID(gWorld.GetCurrentPlayer()));
    gWorld.mCameraControl.Follow(theUnit, this->mFlag, 1.0f, 0.0f, 0.0f);
    if (this->mFlag == false)
        gWorld.mCameraControl.SetChanged();
    return kErrorOK;
}

// 0x26083a: EEMExplore's inline ctor COMDAT (type 0x79). No body of this unit news an EEMExplore, so its first user
// here is unknown (an event whose ProcessEvent was ICF-folded?); left unclaimed

// FUNCTION: EXE 0x66085e
// Cycles the camera through normal, epic and isometric zoom, remembers the choice and reports it in the chat log
long EEUIEMGTogglePerspective::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    if (gWorld.mCameraControl.mMode == 0) {
        gWorld.mCameraControl.SetMode(1);
        gResource->GetString(IDS_EPIC_ZOOM_MODE, theText);
    } else if (gWorld.mCameraControl.mMode == 1) {
        gWorld.mCameraControl.SetMode(2);
        gResource->GetString(IDS_ISOMETRIC_ZOOM_MODE, theText);
    } else {
        gWorld.mCameraControl.SetMode(0);
        gResource->GetString(IDS_NORMAL_ZOOM_MODE, theText);
    }
    gGamePreferences.mSettings.mZoomStyle = gWorld.mCameraControl.mMode;
    gUserInterface->AddChatLogEntry(theText, 0);
    return kErrorOK;
}

// FUNCTION: EXE 0x6608f4
// Raises or lowers the game speed by the world table's step within its limits (unless the speed is locked) and
// sends the new speed
long EEUIEMGChangeGameSpeed::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    float theDelta = 0.0f;
    if (gWorld.mGameOptions.GetLockSpeed() != false)
        return kErrorOK;
    if (this->mFaster) {
        if (gWorld.GetGameSpeed() + gWorldTable.GetFloatValue(kWorldGameSpeedStep) <= gWorldTable.GetFloatValue2(kWorldGameSpeedRange))
            theDelta = gWorldTable.GetFloatValue(kWorldGameSpeedStep);
    } else if (gWorld.GetGameSpeed() - gWorldTable.GetFloatValue(kWorldGameSpeedStep) >= gWorldTable.GetFloatValue(kWorldGameSpeedRange))
        theDelta = -gWorldTable.GetFloatValue(kWorldGameSpeedStep);
    gCommunications->ProcessLocalUserInput(new EEMGameSpeed(gWorld.GetGameSpeed() + theDelta), kSlotNone);
    return kErrorOK;
}

// FUNCTION: EXE 0x6609e5
// Creates the message that sets the game speed
// (EEMGameSpeed's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x660a12
// Sends a unit behaviour for the selection, closes the behaviour menu and shows the chosen behaviour's icon on the
// selected unit's behaviour command button
long EEUIEMGUnitBehavior::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(gUIManager->mMainGameForm->mGroups[0]);
    EEMSetUnitBehavior* theMessage = new EEMSetUnitBehavior(this->mBehavior);
    EEUnit* theUnit = theGroup->GetSelectedUnit();
    EEComplexUnit* theComplex = theUnit ? theUnit->mComplexUnit : 0;
    EEUnitClass* theClass = theUnit ? theUnit->mUnitClass : 0;
    gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
    gUIManager->mMainGameForm->GetEntityGroup(9)->Disable();
    gUIManager->mMainGameForm->GetBackgroundByID(0x97)->SetVisibility(false);
    if (!theComplex || !theClass)
        return kErrorOK;
    for (unsigned long i = 0; i < 9; i++) {
        unsigned long theCommand = theClass->mCommands[i];
        if (!(theCommand >= 2) || !(theCommand < 0x32) || theCommand != 0x1d)
            continue;
        UIButton* theButton = static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(0x4e + i));
        if (this->mBehavior == 0) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorScout))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorScout))->GetPressedBitmap());
        } else if (this->mBehavior == 1) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorStandGround))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorStandGround))->GetPressedBitmap());
        } else if (this->mBehavior == 2) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorGuard))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorGuard))->GetPressedBitmap());
        } else if (this->mBehavior == 5) {
            theButton->SetNormalBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorAggressive))->GetNormalBitmap());
            theButton->SetPressedBitmap(static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(kMainGameBehaviorAggressive))->GetPressedBitmap());
        } else

            ESVERIFY_AT(false, 2006);
        break;
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x660c08
// Switches to the chat log form (previous messages)
long EEUIEMGShowPreviousMessages::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    gUserInterface->SwitchToForm(gUIManager->GetFormByID(kFormChatLog));
    return kErrorOK;
}
