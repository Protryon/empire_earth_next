// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationFiles.cpp (file name inferred: no __FILE__ or RTTI of its
// own). 0x27fdfe..0x2813f4, after EECivilizationBuilder.cpp and before EECivilizationManager.cpp:
// seven init chains (".civ", "Civilizations", "Users", the file spec, "default", the two button sounds), the
// civilization files (EEGetCivilizationDirectory, save Fn28015D/Fn2802EF, EELoadCivilization, Fn2806E7), the
// builder's open/close (Fn280BE9, Fn280FAF, Fn280FFD) and its button handlers. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>

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

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"

#include "ee/EECBUIListBox.h"

#include "ee/EECivilizationManager.h"
#include "ee/EEUIEPDCSEvents.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIFileListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EECBCivilizationCreator.h"
#include "ee/EECivilization.h"
#include "ee/EEGameOptions.h"
#include "ee/EEAction.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")

// chain at 0x67fdfe (wrapper)
//   object 0x965478  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gU565478(".civ");
// SYNTHETIC: EXE 0x67fdfe   (wrapper)
// SYNTHETIC: EXE 0x67fe08   (ctor)
// SYNTHETIC: EXE 0x67fe19   (registrar)
// SYNTHETIC: EXE 0x67fe25   (dtor)

// chain at 0x67fe30 (wrapper)
//   object 0x965498  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gU565498("Civilizations");
// SYNTHETIC: EXE 0x67fe30   (wrapper)
// SYNTHETIC: EXE 0x67fe3a   (ctor)
// SYNTHETIC: EXE 0x67fe4b   (registrar)
// SYNTHETIC: EXE 0x67fe57   (dtor)

// chain at 0x67fe62 (wrapper)
//   object 0x965488  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gU565488("Users");
// SYNTHETIC: EXE 0x67fe62   (wrapper)
// SYNTHETIC: EXE 0x67fe6c   (ctor)
// SYNTHETIC: EXE 0x67fe7d   (registrar)
// SYNTHETIC: EXE 0x67fe89   (dtor)

// chain at 0x67fe94 (wrapper)
//   object 0x965518  ctor __imp_??0FSFileSpec@@QAE@XZ  dtor __imp_??1FSFileSpec@@UAE@XZ
// the civilization file being saved
FSFileSpec gU565518;
// SYNTHETIC: EXE 0x67fe94   (wrapper)
// SYNTHETIC: EXE 0x67fe9e   (ctor)
// SYNTHETIC: EXE 0x67fea9   (registrar)
// SYNTHETIC: EXE 0x67feb5   (dtor)

// chain at 0x67fec0 (wrapper)
//   object 0x965508  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gDefaultUserFolder("default");
// SYNTHETIC: EXE 0x67fec0   (wrapper)
// SYNTHETIC: EXE 0x67feca   (ctor)
// SYNTHETIC: EXE 0x67fedb   (registrar)
// SYNTHETIC: EXE 0x67fee7   (dtor)

// chain at 0x67fef2 (wrapper)
//   object 0x9654f8  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gU5654F8("sounds\\button1.wav");
// SYNTHETIC: EXE 0x67fef2   (wrapper)
// SYNTHETIC: EXE 0x67fefc   (ctor)
// SYNTHETIC: EXE 0x67ff0d   (registrar)
// SYNTHETIC: EXE 0x67ff19   (dtor)

// chain at 0x67ff24 (wrapper)
//   object 0x9654e8  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
UString gU5654E8("sounds\\button2.wav");
// SYNTHETIC: EXE 0x67ff24   (wrapper)
// SYNTHETIC: EXE 0x67ff2e   (ctor)
// SYNTHETIC: EXE 0x67ff3f   (registrar)
// SYNTHETIC: EXE 0x67ff4b   (dtor)

unsigned long gU5654E4;                     // 0x5654e4 the buy/sell sound
unsigned long gU5654E0;                     // 0x5654e0 the refused sound
long gL5654B8;                              // 0x5654b8
EECBEditableCivilization* gEditableCivilization;   // 0x565560
bool gB565564;                              // 0x565564 the builder paused the game
bool gB565565;                              // 0x565565 the players using the saved file reload it
bool gB565566;                              // 0x565566 the upgrade lists are read-only
bool gB565567;                              // 0x565567 a premade civilization was loaded

// FUNCTION: EXE 0x67ff56
// Tells whether the edited civilization may still change
bool Fn27FF56()
{
    return gEditableCivilization->CanChange();
}

// FUNCTION: EXE 0x67ff61
// Sets the edited civilization's name
void Fn27FF61(const UWideString& inName)
{
    gEditableCivilization->SetName(inName);
}

// FUNCTION: EXE 0x67ff71
// Tells whether any upgrade or a new name was chosen
bool Fn27FF71()
{
    return gEditableCivilization->HasChoices();
}

// FUNCTION: EXE 0x67ff7c
// Closes the builder (applying the edits when asked), resuming a game it paused, and unloads its sounds
long Fn27FF7C(bool inApply)
{
    if (gB565564)
        ::gWorld.UnpauseGame();
    if (inApply && gEditableCivilization != NULL)
        gEditableCivilization->Apply();
    Fn27F340();
    return Fn2800B3();
}

// FUNCTION: EXE 0x67ffae
// Returns the civilization choice time limit (300 s)
unsigned long EEGetCivTimerLimit()
{
    return 0x493e0;
}

// FUNCTION: EXE 0x67ffb4
// Returns the time of the civilization timer's first warning (240 s)
unsigned long EEGetCivTimerFirstWarning()
{
    return 0x3a980;
}

// FUNCTION: EXE 0x67ffba
// Returns the time of the civilization timer's second warning (270 s)
unsigned long EEGetCivTimerSecondWarning()
{
    return 0x41eb0;
}

// FUNCTION: EXE 0x67ffc0
// Returns the civilization folder: the user's full path (Users\default\Civilizations\) or the relative one
UString EEGetCivilizationDirectory(bool inX)
{
    UString theDirectory;
    if (inX)
        theDirectory = gStartupDirectory + "Users\\default\\" + kCivilizationsKey + "\\";
    else
        theDirectory = kCivilizationsKey + "\\";
    return theDirectory;
}

// FUNCTION: EXE 0x68006b
// Loads the builder's buy/sell and refused button sounds; returns 3 when either fails
long Fn28006B()
{
    long theResult = 0;
    gU5654E4 = gSoundManager->Load(gU5654F8, 0, 1);
    gU5654E0 = gSoundManager->Load(gU5654E8, 0, 1);
    if (gU5654E4 == 0 || gU5654E0 == 0)
        theResult = 3;
    return theResult;
}

// FUNCTION: EXE 0x6800b3
// Unloads the builder's two button sounds
long Fn2800B3()
{
    gSoundManager->Unload(gU5654E4);
    gSoundManager->Unload(gU5654E0);
    return 0;
}

// FUNCTION: EXE 0x6800da
// Returns the user civilization directory (Users\default\Civilizations\ under the startup directory)
UString Fn2800DA()
{
    UString theDirectory;
    theDirectory = gStartupDirectory + "Users\\" + gDefaultUserFolder + "\\Civilizations\\";
    return theDirectory;
}

// FUNCTION: EXE 0x68015d
// Saves the civilization under a file name: refuses a premade civilization's name and asks before
// overwriting an existing file
long Fn28015D(const UString& inName, bool inOverwrite)
{
    UWideString theFormat;
    UWideString theText;
    if (!inOverwrite) {
        for (unsigned long i = 0; i < gCivilizationManager.GetNumCivilizations(); i++) {
            EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[i];
            if (_stricmp(inName, theEntry.mFileName) != 0)
                continue;
            gResource->GetString(IDS_FILE_NAME_YOU_SPECIFIED_IS_RESERVED, theFormat);
            sprintf(theText, theFormat, (const char*)inName);
            gUIManager->DisplayMessage(theText, 1, NULL, NULL, true);
            return 9;
        }
    }
    gU565518.SetVolume(gStartupVolume);
    gU565518.SetDirectory(Fn2800DA());
    gU565518.SetFile(inName + ".civ");
    long theResult;
    if (gU565518.Open(kFileAccessRead) == 0 && !inOverwrite) {
        theResult = gResource->GetString(IDS_CIVILIZATION_WITH_THAT_NAME_EXISTS_OVERWRITE, theText);
        if (theResult == 0) {
            bool theClose = false;
            gUIManager->DisplayQuestion(theText, 1, (UIForm*)&theClose, Fn2802EF, NULL);
        }
    } else {
        bool theClose = inOverwrite == false;
        Fn2802EF((UIForm*)&theClose);
        theResult = 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x6802ef
// Writes the civilization file (name, upgrade count, upgrades), tells players using that file to reload it,
// and reports errors; the answer to the overwrite question, its form argument the close-the-builder flag
// (a checksum of the name and upgrades that was summed up but never written or used has been removed)
void Fn2802EF(UIForm* inClose)
{
    std::vector<unsigned long> thePlayers;
    UWideString theName(*gEditableCivilization->GetName());
    UWideString theMessage;
    UString theFile(gU565518.GetFile());
    std::vector<unsigned short> theUpgrades;
    bool theClose = *(bool*)inClose;
    unsigned long theWritten = 0;
    unsigned long theLength = 0;
    long theResult;
    if (gU565518.Close() == 0) {
        gU565518.Delete();
        theResult = gU565518.Create();
        if (theResult == 0) {
            theResult = gU565518.Open(kFileAccessWrite);
            if (theResult == 0) {
                theLength = _mbsnbcnt(reinterpret_cast<const unsigned char*>((const char*)theName), theName.GetLength());
                theResult = gU565518.Write(theLength);
                if (theResult == 0) {
                    theResult = gU565518.Write((const char*)theName, theLength, theWritten);
                    if (theResult == 0) {
                        theUpgrades = gEditableCivilization->CopyUpgradeIDs();
                        unsigned long theValue = theUpgrades.size();
                        theResult = gU565518.Write(theValue);
                        if (theResult == 0) {
                            for (std::vector<unsigned short>::iterator j = theUpgrades.begin(); j != theUpgrades.end();
                                 j++) {
                                if (theResult != 0)
                                    break;
                                theValue = *j;
                                theResult = gU565518.Write(theValue);
                            }
                        }
                    }
                }
                if (theResult != 0)
                    gResource->GetString(IDS_ERROR_SAVING_FILE, theMessage);
            } else {
                gResource->GetString(IDS_ERROR_SAVING_FILE, theMessage);
            }
        } else if (theResult == 9) {
            gResource->GetString(IDS_INVALID_FILE_NAME, theMessage);
        } else {
            gResource->GetString(IDS_ERROR_CREATING_FILE, theMessage);
        }
        if (theResult != 0) {
            gU565518.Delete();
        } else {
            if (gB565565 == true) {
                for (unsigned long p = 1; p < ::gWorld.mNumPlayers; p++) {
                    if (::gWorld.mPlayers[p]->mCivilizationFile + ".civ" == theFile)
                        thePlayers.push_back(p);
                }
                if (thePlayers.size() > 0) {
                    EEAResetAllPlayerInfo* theAction = new EEAResetAllPlayerInfo(thePlayers);
                    gUserInterface->PostAction(theAction);
                    theAction->Dereference();
                }
            }
        }
    } else {
        gU565518.Delete();
    }
    gU565518.Close();
    if (theMessage.GetLength() == 0) {
        if (theClose)
            Fn27F340();
    } else {
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    }
}

// FUNCTION: EXE 0x68060c
// Loads a civilization file (the game's, else the user's) into the builder: resets the edit, then buys its
// upgrades again; fails when neither file loads
long Fn28060C(const UString& inName)
{
    std::vector<unsigned short> theUpgrades;
    UWideString theName;
    long theResult = EELoadCivilization(inName, EEGetCivilizationDirectory(false), theName, theUpgrades);
    if (theResult != 0
        && (theResult = EELoadCivilization(inName, EEGetCivilizationDirectory(true), theName, theUpgrades)) != 0)
        return theResult;
    gEditableCivilization->ResetToStart();
    gEditableCivilization->SetName(theName);
    gB565567 = true;
    if (Fn27FBA5()) {
        for (std::vector<unsigned short>::iterator i = theUpgrades.begin(); i != theUpgrades.end(); i++)
            Fn28105E(*i);
    } else {
        theResult = 3;
    }
    return theResult;
}

// FUNCTION: EXE 0x6806e7
// Gives a player the civilization from a file (the game's, else the user's), buying its upgrades at no cost;
// a game file's premade civilization keeps its localized name
long Fn2806E7(UString& ioName, EEPlayer* inPlayer)
{
    long theNameID;
    unsigned long theCost;
    std::vector<unsigned short> theUpgrades;
    UWideString theName;
    long theResult = EELoadCivilization(ioName, EEGetCivilizationDirectory(false), theName, theUpgrades);
    if (theResult != 0) {
        theResult = EELoadCivilization(ioName, EEGetCivilizationDirectory(true), theName, theUpgrades);
    } else {
        gCivilizationManager.MatchesPremadeCivilization(UWideString((const char*)ioName), theName, theUpgrades, &theNameID);
        if (theNameID != 0)
            gResource->GetString(theNameID, theName);
    }
    if (theResult == 0) {
        EECivilization& theCivilization = inPlayer->mCivilization;
        theCivilization.mPurchasedUpgrades.clear();
        theCivilization.mAvailablePoints = theCivilization.mMaxPoints;
        theCivilization.SetPointsAndMax(0);
        theCivilization.SetName(theName);
        for (std::vector<unsigned short>::iterator i = theUpgrades.begin(); i != theUpgrades.end(); i++) {
            theCivilization.CanAfford(*i, &theCost, NULL);
            theCivilization.SetPointsAndMax(theCivilization.GetMaxPoints() + theCost);
            theCivilization.Buy(*i, inPlayer->mPlayerID, true);
        }
        theCivilization.SetPoints(0);
    }
    return theResult;
}

// FUNCTION: EXE 0x680840
// Reads a civilization file's name and upgrade ids from the data path or the given directory
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<unsigned short>& outUpgrades)
{
    FSFileSpec* theFile = NULL;
    unsigned long theLength = 0;
    unsigned long theRead = 0;
    unsigned long theCount = 0;
    unsigned long theUpgrade;
    char theBuffer[0x200];
    long theResult;
    gU565518.Close();
    bool theMissing = gDataPath->FindFile(inDirectory + inName + kCivKey, theFile) != 0;
    if (theMissing) {
        theFile = new FSFileSpec;
        gU565518.SetVolume(gStartupVolume);
        gU565518.SetDirectory(inDirectory);
        gU565518.SetFile(inName + kCivKey);
        *theFile = gU565518;
    }
    theResult = theFile->Open(kFileAccessRead);
    if (theResult == 0) {
        theResult = theFile->Read(theLength);
        if (theResult == 0) {
            if (theLength == 0) {
                outName = gU4E5B98;
            } else {
                memset(theBuffer, 0, sizeof(theBuffer));
                if (theFile->Read(theBuffer, theLength, theRead) == kErrorOK)
                    outName = theBuffer;
            }
            theResult = theFile->Read(theCount);
            if (theResult == 0) {
                for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
                    theResult = theFile->Read(theUpgrade);
                    if (theResult == 0) {
                        unsigned short theId = (unsigned short)theUpgrade;
                        outUpgrades.push_back(theId);
                    }
                }
            }
        }
    }
    if (theFile != NULL) {
        theFile->Close();
        delete theFile;
    }
    return theResult;
}

// FUNCTION: EXE 0x680a05
// Grants a player's civilization upgrade request when allowed, valid and affordable (broadcast in multiplayer);
// otherwise posts a chat line naming the refused civilization
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<unsigned short>& inUpgrades)
{
    UWideString theFormat;
    UWideString theText;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    long theNameID = 0;
    bool theAllowed;
    if (::gWorld.mGameOptions.CanBuyCivilization())
        theAllowed = thePlayer->mCivUpgradesPurchased == false;
    else
        theAllowed = true;
    bool theValid;
    if (!gAllowCustomCivs && !gIsScenarioGame && !gUIManager->GetInScenarioEditor())
        theValid = gCivilizationManager.MatchesPremadeCivilization(inName, inName, inUpgrades, &theNameID);
    else
        theValid = true;
    if (theAllowed && theValid && thePlayer->mCivilization.CanAffordAll(inUpgrades)) {
        EEMCivUpgradesPurchased* theMessage = new EEMCivUpgradesPurchased(inPlayer, inUpgrades, inName, theNameID);
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        else {
            theMessage->Execute();
            theMessage->Dereference();
        }
    } else {
        gResource->GetString(IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH, theFormat);
        sprintf(theText, theFormat, (const char*)thePlayer->mName, (const char*)inName,
                (const char*)::gWorld.GetCurrentPlayer()->mName);
        gCommunications->ProcessLocalUserInput(new EEMSelectPlayerChatType(0L), kSlotNone);
        gCommunications->ProcessLocalUserInput(new EEMInGameChat(theText), kSlotNone);
        gPhysics->GetRandom(0L, 1L);
    }
}

// FUNCTION: EXE 0x680ba7
// Sets whether players using the saved civilization file reload it
void Fn280BA7(bool inFlag)
{
    gB565565 = inFlag;
}

// FUNCTION: EXE 0x680bb1
// Opens the current player's civilization for editing
void Fn280BB1()
{
    gEditableCivilization = new EECBEditableCivilization(&::gWorld.GetCurrentPlayer()->mCivilization,
                                                         ::gWorld.mCurrentPlayer);
}

// FUNCTION: EXE 0x680be9
// Opens the builder form for the current player (buttons chosen by game kind), fills its lists, name and
// points and loads its sounds
long Fn280BE9()
{
    UWideString theName;
    UString theDirectory;
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    EEPlayer* thePlayer = ::gWorld.GetCurrentPlayer();
    EEUIECBShowOrHideButtons* theEvent;
    if (thePlayer != NULL) {
        if (gFreeCamera) {
            gB565566 = false;
            gB565567 = false;
            theEvent = new EEUIECBShowOrHideButtons(false, true, true, true, true);
        } else {
            gB565566 = thePlayer->mCivilizationLocked;
            if (::gWorld.mGameOptions.CanBuyCivilization()) {
                gB565567 = false;
                bool theWasOpen = thePlayer->mCivilizationChosen;
                thePlayer->mCivilizationLocked = true;
                if (!theWasOpen)
                    theEvent = new EEUIECBShowOrHideButtons(true, false, true, false, false);
                else
                    theEvent = new EEUIECBShowOrHideButtons(false, true, false, false, false);
            } else {
                theEvent = new EEUIECBShowOrHideButtons(true, true, false, false, false);
            }
        }
    } else {
        theEvent = new EEUIECBShowOrHideButtons(false, true, true, true, false);
        gB565566 = false;
    }
    theForm->SetFormOpenEvent(theEvent);
    if (theEvent != NULL)
        theEvent->Dereference();
    UIEvent* theClose = new EEUIECBCleanUp;
    theForm->SetFormCloseEvent(theClose);
    theClose->Dereference();
    long theResult = Fn27F3FC();
    if (theResult == 0) {
        theResult = Fn28006B();
        if (theResult == 0) {
            theResult = Fn27F34E(gEditableCivilization->GetCivilizationUpgrades());
            if (theResult == 0) {
                if (!::gWorld.mGameOptions.GetCampaignMapName(theName))
                    theName = *gEditableCivilization->GetName();
                theResult = Fn27F43F(theName);
                if (theResult == 0) {
                    EECBTrace(Fn27FF56());
                    EEShowCivBuilderPoints(gEditableCivilization->GetPointsLeft(), gEditableCivilization->GetMaxPoints());
                    gU565518.SetVolume(gStartupVolume);
                    theDirectory = Fn2800DA();
                    gU565518.SetDirectory(theDirectory);
                    EESetCivBuilderButtonBlinking(false);
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x680e8e
// Deletes the edited civilization when the builder closes
long EEUIECBCleanUp::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    delete gEditableCivilization;
    gEditableCivilization = NULL;
    return kErrorOK;
}

// FUNCTION: EXE 0x680eab
// Shows or hides the builder's accept, exit, load and save buttons, and optionally hides its points controls
long EEUIECBShowOrHideButtons::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCivBuilder);
    if (this->mB0C)
        theForm->GetControl(kCivBuilderCivAcceptChangesButton)->SetDisplayMode(kDisplayModeShown);
    else
        theForm->GetControl(kCivBuilderCivAcceptChangesButton)->SetDisplayMode(kDisplayModeHidden);
    if (this->mB0D)
        theForm->GetControl(kCivBuilderCivExitButton)->SetDisplayMode(kDisplayModeShown);
    else
        theForm->GetControl(kCivBuilderCivExitButton)->SetDisplayMode(kDisplayModeHidden);
    if (this->mB0E)
        theForm->GetControl(kCivBuilderLoadCivButton)->SetDisplayMode(kDisplayModeShown);
    else
        theForm->GetControl(kCivBuilderLoadCivButton)->SetDisplayMode(kDisplayModeHidden);
    if (this->mB0F)
        theForm->GetControl(kCivBuilderCivSaveButton)->SetDisplayMode(kDisplayModeShown);
    else
        theForm->GetControl(kCivBuilderCivSaveButton)->SetDisplayMode(kDisplayModeHidden);
    if (this->mB10)
        Fn27F2E8();
    return kErrorOK;
}

// FUNCTION: EXE 0x680f34
// Opens the load dialog, or says (string 0x7458) that the civilization may no longer change
long Fn280F34()
{
    std::vector<UWideString> theUnused;
    UWideString theText;
    long theResult;
    if (Fn27FF56()) {
        theResult = Fn27F7B1();
    } else {
        theResult = gResource->GetString(IDS_YOU_CANT_LOAD_CIVILIZATION_AFTER, theText);
        gUIManager->DisplayMessage(theText, 1, NULL, NULL, true);
    }
    return theResult;
}

// FUNCTION: EXE 0x680faa
// Opens the save dialog
long Fn280FAA()
{
    return Fn27F824();
}

// FUNCTION: EXE 0x680faf
// Pauses the game and opens the builder on the current player's civilization, marking it chosen
long Fn280FAF()
{
    long theResult = ::gWorld.PauseGame();
    gB565564 = theResult == 0;
    if (theResult != 9 && theResult != 2 && theResult != 0)
        return theResult;
    Fn280BB1();
    Fn280BA7(false);
    theResult = Fn280BE9();
    if (theResult == 0)
        ::gWorld.GetCurrentPlayer()->mCivilizationChosen = true;
    return theResult;
}

// FUNCTION: EXE 0x680ffd
// Pauses the game and opens the builder on a new civilization to create from the given one
long Fn280FFD(EECivilization* inCivilization, bool inReload)
{
    long theResult = ::gWorld.PauseGame();
    gB565564 = theResult == 0;
    if (theResult == 9 || theResult == 2 || theResult == 0) {
        gEditableCivilization = new EECBCivilizationCreator(inCivilization, gFreeCamera);
        Fn280BA7(inReload);
        theResult = Fn280BE9();
    }
    return theResult;
}

// FUNCTION: EXE 0x68105e
// Buys an upgrade (the selected available one when 0) and updates the points; plays the refused sound when
// it is not affordable
long Fn28105E(unsigned short inUpgrade)
{
    unsigned short theUpgrade = 0;
    unsigned long theBonus;
    unsigned long theCost;
    long theResult = 0xd;
    EECBTrace(Fn27FF56());
    if (Fn27FF56()) {
        theResult = 0;
        if (inUpgrade == 0) {
            if (!Fn27F299(true, &theUpgrade, &theCost, &theBonus))
                theResult = 3;
        } else {
            theUpgrade = inUpgrade;
        }
        if (theResult == 0) {
            if (gEditableCivilization->AddUpgrade(theUpgrade)) {
                theResult = Fn27FD5A(inUpgrade);
                if (theResult == 0)
                    EEShowCivBuilderPoints(gEditableCivilization->GetPointsLeft(), gEditableCivilization->GetMaxPoints());
            } else {
                gSoundManager->Play(gU5654E0, 1.0f, 0.0f, 1.0f, false, NULL);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x681120
// Sells the selected bought upgrade back, unless the civilization already owned it before the edit
long Fn281120()
{
    std::vector<unsigned short> theOwn;
    long theResult = 3;
    EECBTrace(Fn27FF56());
    if (!Fn27FF56())
        return theResult;
    unsigned short theUpgrade;
    unsigned long theBonus;
    unsigned long theCost;
    if (Fn27F299(false, &theUpgrade, &theBonus, &theCost)) {
        theOwn = gEditableCivilization->GetCivilizationUpgrades();
        std::vector<unsigned short>::iterator i = theOwn.begin();
        while (true) {
            if (i == theOwn.end())
                break;
            if (*i == theUpgrade)
                return theResult;
            i++;
        }
        gEditableCivilization->RemoveUpgrade(theUpgrade);
        theResult = Fn27FDAB(0);
        if (theResult == 0)
            EEShowCivBuilderPoints(gEditableCivilization->GetPointsLeft(), gEditableCivilization->GetMaxPoints());
    }
    return theResult;
}

// FUNCTION: EXE 0x6811d9
// Asks whether to delete the selected civilization file
long Fn2811D9(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theText;
    long theResult = gResource->GetString(IDS_ARE_YOU_SURE_YOU_WANT_TO_DELETE, theText);
    if (theResult == 0)
        gUIManager->DisplayQuestion(theText, 1, inForm, Fn28122C, NULL);
    return theResult;
}

// FUNCTION: EXE 0x68122c
// Deletes the civilization file selected in the dialog's list, reporting a failure
void Fn28122C(UIForm* inForm)
{
    UWideString theText;
    FSFileSpec theUnused;
    UIFileListBox* theList = NULL;
    unsigned long theIndex = 0;
    if (Fn27F08F(inForm, &theList) && theList->GetCurrentSelection(theIndex) == 0 &&
        theList->DeleteSelectedFile() != 0) {
        gResource->GetString(IDS_ERROR_DELETING_FILE, theText);
        gUIManager->DisplayMessage(theText, 1, NULL, NULL, true);
    }
}

// FUNCTION: EXE 0x6812bc
// Warns that not all points were spent and asks before closing the builder
long Fn2812BC(UIForm* inForm)
{
    UWideString theText;
    long theResult = gResource->GetString(IDS_YOU_HAVE_NOT_SPENT_ALL, theText);
    if (theResult == 0)
        gUIManager->DisplayQuestion(theText, 1, inForm, Fn28130F, NULL);
    return theResult;
}

// FUNCTION: EXE 0x68130f
// Closes the builder, applying a changed civilization (an unchanged one named from the campaign map, else
// string 0x771c unless a premade one was loaded)
void Fn28130F(UIForm* inForm)
{
    UWideString theName;
    bool theApply;
    if (Fn27FF56() && Fn27FF71()) {
        theApply = true;
        if (gEditableCivilization != NULL && gEditableCivilization->GetIsUnchanged()) {
            if (::gWorld.mGameOptions.GetCampaignMapName(theName)) {
                if (Fn27F43F(theName) == 0) {
                    Fn27FF61(theName);
                    Fn27FF7C(true);
                }
            } else {
                if (!gB565567) {
                    gResource->GetString(IDS_CUSTOM_CIVILIZATION, theName);
                    Fn27FF61(theName);
                }
                Fn27FF7C(true);
            }
            return;
        }
    } else {
        theApply = false;
    }
    Fn27FF7C(theApply);
}

// FUNCTION: EXE 0x6813be
// Creates the action that resets the given players' info (the out-of-line inline constructor)
// (EEAResetAllPlayerInfo's inline ctor, out of line after its first user Fn2802EF)
