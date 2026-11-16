// C:\EEX\Empire Earth\Civilization Builder\EECivilizationBuilder.cpp (file name inferred: no __FILE__ or RTTI).
// 0x27efca..0x27fdfe: the civilization builder forms' helpers (form 7 the builder, 0xf the name dialog, 0x10/0x11
// the save/load dialogs, 0x23 the in-game civilization selection) and EEProcessTimerExpired. The builder is Rust's
// whole (ee_screens civ_whole.rs with civ_edit.rs, the edited civilization, and civ_list.rs, the list box): its
// helpers, its state, the edited civilization, its files' writing, its open, close, buys, returns, loads, deletes and
// accept. This unit keeps the entry points the game calls and the builder's host, ee_screenscpp_cb: one toolkit,
// file or game call per op. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>
#include <string>

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
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/UIFileListBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivilization.h"
#include "ee/EEGameOptions.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEGameSound.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
#include "EEDataText.h"   // QuoteText: the keyed .civ text's name
#include "rust/ee_screens.h"   // the builder (civ_whole.rs) and its host's ops (EE_SCREENS_CBW_*)

// the civilization file being saved or loaded (EECBCivilizationFiles.cpp's file spec 0x565518: EELoadCivilization
// shares it)
extern FSFileSpec gU565518;

// ---- the entry points (the builder's Rust)

long EEProcessTimerExpired()
{
    return ee_screens_cb_timer_expired();
}

long EESetCivBuilderButtonBlinking(bool inBlinking)
{
    return ee_screens_cb_set_blinking(inBlinking);
}

long CivBuilderEditCivilization()
{
    return ee_screens_cb_edit_civilization();
}

long CivBuilderCreateCivilization(EECivilization* inCivilization, bool inReload)
{
    return ee_screens_cb_create_civilization(inCivilization, inReload);
}

long CivBuilderSaveCivilization(const UString& inName, bool inOverwrite)
{
    return ee_screens_cb_save_civilization(reinterpret_cast<const uint8_t*>((const char*)inName), strlen(inName),
                                           inOverwrite);
}

void CivBuilderEditCurrentCivilization()
{
    ee_screens_cb_edit_current();
}

void CivBuilderFreeCivilization()
{
    ee_screens_cb_free_civilization();
}

// the questions' Yes (DisplayQuestion's callbacks): the file written over, the selected file deleted, the changes
// accepted
static void EECBAnswerWrite(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_WRITE, inForm);
}

static void EECBAnswerDelete(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_DELETE, inForm);
}

static void EECBAnswerAccept(UIForm* inForm)
{
    ee_screens_cb_answer(EE_SCREENS_CBW_ANSWER_ACCEPT, inForm);
}

// ---- the host

static void EECBPut(EeScreensCbOut* outText, const char* inText)
{
    ee_screens_cb_out_text(outText, reinterpret_cast<const uint8_t*>(inText), strlen(inText));
}

static UIControl* EECBControl(int64_t inForm, int64_t inControl)
{
    UIForm* theForm = gUIManager->GetFormByID((unsigned long)inForm);
    return theForm == NULL ? NULL : theForm->GetControlById((unsigned long)inControl);
}

static UIFileListBox* EECBFileList(void* inForm, int64_t inFormID, int64_t inControl)
{
    UIForm* theForm = inForm != NULL ? static_cast<UIForm*>(inForm) : gUIManager->GetFormByID((unsigned long)inFormID);
    return theForm == NULL ? NULL : dynamic_cast<UIFileListBox*>(theForm->GetControlById((unsigned long)inControl));
}

static UIControl* EECBMainButton(int64_t inWhich)
{
    if (inWhich == 0) {
        UIForm* theForm = gUIManager->GetFormByID(kFormMainGame);
        return theForm == NULL ? NULL : dynamic_cast<UIBlinkingButton*>(theForm->GetControl(kMainGameCivSelectorBuilder));
    }
    return gUIManager->mMainGameForm->GetControl(kMainGameLargeCivBuilderAlert);
}

extern "C" int64_t ee_screenscpp_cb(uint32_t inOp, uintptr_t inP, int64_t inA, int64_t inB, int64_t inC,
                                    const uint8_t* inText, size_t inLength, EeScreensCbOut* outText)
{
    std::string theBytes(reinterpret_cast<const char*>(inText), inText == NULL ? 0 : inLength);
    UWideString theText(theBytes.c_str());
    void* p = reinterpret_cast<void*>(inP);
    if (inOp >= EE_SCREENS_CBW_L_COUNT && inOp <= EE_SCREENS_CBW_L_DRAW_END)
        return EECBUIListBox::HostOp(inOp, static_cast<EECBUIListBox*>(p), inA, inB, inC, theText, outText);
    EECivilization* theCiv = static_cast<EECivilization*>(p);
    switch (inOp) {
    case EE_SCREENS_CBW_FORM_EXISTS:
        return gUIManager->GetFormByID((unsigned long)inA) != NULL;
    case EE_SCREENS_CBW_EDIT_TEXT:
    case EE_SCREENS_CBW_SET_EDIT_TEXT:
    case EE_SCREENS_CBW_EDIT_SELECT: {
        UIEditBox* theEdit = dynamic_cast<UIEditBox*>(EECBControl(inA, inB));
        if (theEdit == NULL)
            return 3;
        if (inOp == EE_SCREENS_CBW_EDIT_TEXT)
            EECBPut(outText, theEdit->GetText(0));
        else if (inOp == EE_SCREENS_CBW_SET_EDIT_TEXT)
            theEdit->SetText(theText);
        else {
            theEdit->SelectText();
            gUIManager->GetFormByID((unsigned long)inA)->SetTabStop(theEdit->GetIndex());
        }
        return 0;
    }
    case EE_SCREENS_CBW_LABEL_TEXT:
    case EE_SCREENS_CBW_SET_LABEL_TEXT:
    case EE_SCREENS_CBW_SET_LABEL_NUMBER: {
        UITextLabel* theLabel = dynamic_cast<UITextLabel*>(EECBControl(inA, inB));
        if (theLabel == NULL)
            return 3;
        if (inOp == EE_SCREENS_CBW_LABEL_TEXT)
            EECBPut(outText, theLabel->GetText(0));
        else if (inOp == EE_SCREENS_CBW_SET_LABEL_TEXT)
            theLabel->SetText(theText);
        else
            theLabel->SetText((long)inC);
        return 0;
    }
    case EE_SCREENS_CBW_DISPLAY:
    case EE_SCREENS_CBW_DISPLAY_INDEX: {
        UIForm* theForm = gUIManager->GetFormByID((unsigned long)inA);
        UIControl* theControl = theForm == NULL ? NULL
                                : inOp == EE_SCREENS_CBW_DISPLAY ? theForm->GetControlById((unsigned long)inB)
                                                                 : theForm->GetControl((unsigned long)inB);
        if (theControl != NULL)
            theControl->SetDisplayMode(inC ? kDisplayModeShown : kDisplayModeHidden);
        return 0;
    }
    case EE_SCREENS_CBW_PUSH: {
        UIForm* theForm = gUIManager->GetFormByID((unsigned long)inA);
        return theForm == NULL ? 3 : gUIManager->Push(theForm);
    }
    case EE_SCREENS_CBW_CLOSE_FORM:
        return gUserInterface->CloseForm(NULL);
    case EE_SCREENS_CBW_CURRENT_IS:
        return gUIManager->GetCurrentForm() == gUIManager->GetFormByID((unsigned long)inA);
    case EE_SCREENS_CBW_FILE_LIST_FILL: {
        // Users\default\Civilizations\*.civ
        UIFileListBox* theList = EECBFileList(NULL, inA, inB);
        FSPath* thePath = new FSPath;
        FSFileSpec theSpec;
        theSpec.SetVolume(gStartupVolume);
        theSpec.SetDirectory(gStartupDirectory + "Users\\default\\Civilizations\\");
        thePath->AddDirectory(theSpec, true);
        theList->SetSearchPaths(thePath);
        theList->AddValidExtension(UString(".civ"));
        theList->CreateEntries(1);
        thePath->Dereference();
        return 0;
    }
    case EE_SCREENS_CBW_FILE_SELECTION: {
        UIFileListBox* theList = EECBFileList(p, inA, inB);
        if (theList == NULL)
            return -1;
        unsigned long theIndex = 0;
        long theResult = theList->GetCurrentSelection(theIndex);
        ee_screens_cb_out_value(outText, (int64_t)theIndex);
        return theResult;
    }
    case EE_SCREENS_CBW_FILE_LINE:
        EECBPut(outText, EECBFileList(p, inA, inB)->GetData((unsigned long)inC));
        return 0;
    case EE_SCREENS_CBW_FILE_DELETE:
        return EECBFileList(NULL, inA, inB)->DeleteSelectedFile();
    case EE_SCREENS_CBW_IS_FORM:
        return static_cast<UIForm*>(p) == gUIManager->GetFormByID((unsigned long)inA);
    case EE_SCREENS_CBW_LIST: {
        EECBUIListBox* theList = dynamic_cast<EECBUIListBox*>(EECBControl(inA, inB));
        return (int64_t)reinterpret_cast<uintptr_t>(theList);
    }
    case EE_SCREENS_CBW_FORM_EVENTS: {
        UIForm* theForm = gUIManager->GetFormByID((unsigned long)inA);
        UIEvent* theEvent = EECreateScreenEvent((unsigned long)inB);
        theForm->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        theEvent = EECreateScreenEvent((unsigned long)inC);
        theForm->SetFormCloseEvent(theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_CBW_FORM_ID:
        return static_cast<UIForm*>(p)->GetFormID();
    case EE_SCREENS_CBW_STRING: {
        UWideString theString;
        long theResult = gResource->GetString(inA, theString);
        EECBPut(outText, theString);
        return theResult;
    }
    case EE_SCREENS_CBW_PERCENT: {
        static UWideString sFormat;
        UWideString theValue;
        uint32_t theBits = (uint32_t)inA;
        float theFloat;
        memcpy(&theFloat, &theBits, sizeof(theFloat));
        gResource->GetString(IDS_FORMAT_PERCENT_6_2G, sFormat);
        sprintf(theValue, sFormat, theFloat);
        EECBPut(outText, theValue);
        return 0;
    }
    case EE_SCREENS_CBW_RESERVED: {
        UString theName(theBytes.c_str());
        for (unsigned long i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
            if (PlatStrICmp(theName, gCivilizationManager.GetCivilization(i).mFileName) != 0)
                continue;
            UWideString theFormat;
            UWideString theMessage;
            gResource->GetString(IDS_FILE_NAME_YOU_SPECIFIED_IS_RESERVED, theFormat);
            sprintf(theMessage, theFormat, (const char*)theName);
            EECBPut(outText, theMessage);
            return 1;
        }
        return 0;
    }
    case EE_SCREENS_CBW_LOOKUP_LEVEL: {
        float theValue = 0.0f;
        float theBonus = 0.0f;
        bool theFound = gCivilizationManager.LookupLevel((EECivUpgrade)inA, &theValue, &theBonus);
        uint32_t theBits[2];
        memcpy(&theBits[0], &theValue, 4);
        memcpy(&theBits[1], &theBonus, 4);
        ee_screens_cb_out_value(outText, theBits[0]);
        ee_screens_cb_out_value(outText, theBits[1]);
        return theFound;
    }
    case EE_SCREENS_CBW_EXTRA_COST:
        return gCivilizationManager.GetExtraCost((EECivUpgrade)inA);
    case EE_SCREENS_CBW_CATEGORY_NAME_ID: {
        long theID = 0;
        bool theFound = gCivilizationManager.GetCategoryNameID((unsigned long)inA, &theID);
        ee_screens_cb_out_value(outText, theID);
        return theFound;
    }
    case EE_SCREENS_CBW_LEVEL_NAME_ID:
        return EECivilizationManager::LevelNameID((unsigned long)inA);
    case EE_SCREENS_CBW_IS_POWER_LEVEL:
        return EECivilizationManager::IsPowerLevel((unsigned long)inA);
    case EE_SCREENS_CBW_ALL_UPGRADES:
        for (EECivUpgrade theUpgrade : gCivilizationManager.GetUpgrades())
            ee_screens_cb_out_value(outText, theUpgrade);
        return 0;
    case EE_SCREENS_CBW_UPGRADE_KEY: {
        std::string theKey;
        bool theFound = gCivilizationManager.UpgradeKey((EECivUpgrade)inA, theKey);
        EECBPut(outText, theKey.c_str());
        return theFound;
    }
    case EE_SCREENS_CBW_QUOTE:
        EECBPut(outText, EEDataText::QuoteText(theBytes).c_str());
        return 0;
    case EE_SCREENS_CBW_C_BASE_COST:
        return theCiv->GetBaseCost((EECivUpgrade)inA);
    case EE_SCREENS_CBW_C_EXTRA_COST:
        return theCiv->GetExtraCost((EECivUpgrade)inA);
    case EE_SCREENS_CBW_C_COMPUTE:
        return theCiv->ComputeCost((long)inA, (long)inB, (long)inC);
    case EE_SCREENS_CBW_C_COUNT:
        return theCiv->CountInCategory((EECivUpgrade)inA, NULL);
    case EE_SCREENS_CBW_C_AVAILABLE:
        return theCiv->GetAvailablePoints();
    case EE_SCREENS_CBW_C_UNCHANGED:
        return theCiv->GetUnchanged();
    case EE_SCREENS_CBW_C_MAX:
        return theCiv->GetMaxPoints();
    case EE_SCREENS_CBW_C_PURCHASED:
        for (EECivUpgrade theUpgrade : theCiv->GetPurchasedUpgrades())
            ee_screens_cb_out_value(outText, theUpgrade);
        return 0;
    case EE_SCREENS_CBW_C_NAME:
        EECBPut(outText, theCiv->mName);
        return 0;
    case EE_SCREENS_CBW_REQUEST: {
        // the name's bytes, then the upgrades (little-endian u32s)
        UWideString theName(theBytes.substr(0, (size_t)inB).c_str());
        std::vector<EECivUpgrade> theUpgrades;
        for (size_t i = (size_t)inB; i + 4 <= theBytes.size(); i += 4) {
            const unsigned char* q = reinterpret_cast<const unsigned char*>(theBytes.data()) + i;
            theUpgrades.push_back((EECivUpgrade)(q[0] | q[1] << 8 | q[2] << 16 | (uint32_t)q[3] << 24));
        }
        unsigned char thePlayer = (unsigned char)inA;
        NEMessage* theMessage = new EEMRecord(kMessageCivUpgradesRequested, {thePlayer, 0, theUpgrades, theName});
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theMessage, kSlotServer);
        else {
            theMessage->Execute();
            theMessage->Dereference();
        }
        return 0;
    }
    case EE_SCREENS_CBW_MULTIPLAYER:
        return gCommunications->GetIsMultiplayerSession();
    case EE_SCREENS_CBW_GAME_RESULT:
        return gWorld.GetCurrentPlayer()->GetGameResult();
    case EE_SCREENS_CBW_PAUSE:
        return gWorld.PauseGame();
    case EE_SCREENS_CBW_UNPAUSE:
        return gWorld.UnpauseGame();
    case EE_SCREENS_CBW_PLAYER_CIV: {
        EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
        return thePlayer == NULL ? 0 : (int64_t)reinterpret_cast<uintptr_t>(&thePlayer->mCivilization);
    }
    case EE_SCREENS_CBW_PLAYER_ID:
        return (unsigned char)gWorld.mCurrentPlayer;
    case EE_SCREENS_CBW_PLAYER_FLAG:
        return inA == 0 ? gWorld.GetCurrentPlayer()->mCivilizationChosen : gWorld.GetCurrentPlayer()->mCivilizationLocked;
    case EE_SCREENS_CBW_SET_PLAYER_FLAG:
        (inA == 0 ? gWorld.GetCurrentPlayer()->mCivilizationChosen : gWorld.GetCurrentPlayer()->mCivilizationLocked) =
            inB != 0;
        return 0;
    case EE_SCREENS_CBW_FREE_CAMERA:
        return gFreeCamera;
    case EE_SCREENS_CBW_CAN_BUY:
        return gWorld.mGameOptions.CanBuyCivilization();
    case EE_SCREENS_CBW_CAMPAIGN_NAME: {
        UWideString theName;
        bool theFound = gWorld.mGameOptions.GetCampaignMapName(theName);
        if (theFound)
            EECBPut(outText, theName);
        return theFound;
    }
    case EE_SCREENS_CBW_CUSTOM_CIVS:
        return gAllowCustomCivs;
    case EE_SCREENS_CBW_SOUND_LOAD:
        return (int64_t)gSoundManager->Load(UString(inA ? "sounds\\button2.wav" : "sounds\\button1.wav"), 0, 1);
    case EE_SCREENS_CBW_SOUND_UNLOAD:
        gSoundManager->Unload((unsigned long)inA);
        return 0;
    case EE_SCREENS_CBW_SOUND_PLAY:
        gSoundManager->Play((unsigned long)inA, 1.0f, 0.0f, 1.0f, false, NULL);
        return 0;
    case EE_SCREENS_CBW_MESSAGE:
        gUIManager->DisplayMessage(theText, 1, NULL, NULL, true);
        return 0;
    case EE_SCREENS_CBW_QUESTION:
        gUIManager->DisplayQuestion(theText, 1, static_cast<UIForm*>(p),
                                    inA == EE_SCREENS_CBW_ANSWER_WRITE    ? EECBAnswerWrite
                                    : inA == EE_SCREENS_CBW_ANSWER_DELETE ? EECBAnswerDelete
                                                                          : EECBAnswerAccept,
                                    NULL);
        return 0;
    case EE_SCREENS_CBW_BLINK_FLAG:
        gUIManager->mMainGameForm->mIsButtonBlinking = inA != 0;
        return 0;
    case EE_SCREENS_CBW_MAIN_BUTTON:
        return EECBMainButton(inA) != NULL;
    case EE_SCREENS_CBW_MAIN_BLINK:
        static_cast<UIBlinkingButton*>(EECBMainButton(inA))->SetIsBlinking(inB != 0);
        return 0;
    case EE_SCREENS_CBW_MAIN_DISPLAY:
        EECBMainButton(inA)->SetDisplayMode(inB ? kDisplayModeShown : kDisplayModeHidden);
        return 0;
    case EE_SCREENS_CBW_F_DIRECTORY:
        gU565518.SetVolume(gStartupVolume);
        gU565518.SetDirectory(UString(theBytes.c_str()));
        return 0;
    case EE_SCREENS_CBW_F_NAME:
        gU565518.SetFile(UString(theBytes.c_str()));
        return 0;
    case EE_SCREENS_CBW_F_OPEN:
        return gU565518.Open(inA ? kFileAccessWrite : kFileAccessRead);
    case EE_SCREENS_CBW_F_CLOSE:
        return gU565518.Close();
    case EE_SCREENS_CBW_F_DELETE:
        return gU565518.Delete();
    case EE_SCREENS_CBW_F_CREATE:
        return gU565518.Create();
    case EE_SCREENS_CBW_F_WRITE: {
        unsigned long theWritten = 0;
        return gU565518.Write(theBytes.data(), (unsigned long)theBytes.size(), theWritten);
    }
    case EE_SCREENS_CBW_F_FILE:
        EECBPut(outText, gU565518.GetFile());
        return 0;
    case EE_SCREENS_CBW_STARTUP_DIRECTORY:
        EECBPut(outText, gStartupDirectory);
        return 0;
    case EE_SCREENS_CBW_RELOAD_PLAYERS: {
        UString theFile(theBytes.c_str());
        std::vector<unsigned long> thePlayers;
        for (unsigned long i = 1; i < gWorld.mNumPlayers; i++)
            if (gWorld.mPlayers[i]->mCivilizationFile + ".civ" == theFile)
                thePlayers.push_back(i);
        if (thePlayers.size() > 0) {
            EEAResetAllPlayerInfo* theAction = new EEAResetAllPlayerInfo(thePlayers);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
        return (int64_t)thePlayers.size();
    }
    case EE_SCREENS_CBW_LOAD_CIVILIZATION: {
        // the game's file, else (into the same name and upgrades) the user's
        UString theName(theBytes.c_str());
        UWideString theCivName;
        std::vector<EECivUpgrade> theUpgrades;
        long theResult = EELoadCivilization(theName, EEGetCivilizationDirectory(false), theCivName, theUpgrades);
        if (theResult != 0)
            theResult = EELoadCivilization(theName, EEGetCivilizationDirectory(true), theCivName, theUpgrades);
        EECBPut(outText, theCivName);
        for (EECivUpgrade theUpgrade : theUpgrades)
            ee_screens_cb_out_value(outText, theUpgrade);
        return theResult;
    }
    }
    return 0;
}
