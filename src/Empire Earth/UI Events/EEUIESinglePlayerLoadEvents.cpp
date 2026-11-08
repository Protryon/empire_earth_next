// C:\EEX\Empire Earth\UI Events\EEUIESinglePlayerLoadEvents.cpp (file name inferred: no __FILE__; class prefixes
// EEUIESPL/SPLE). The single-player load (saved game) screen's events: 0x1e8e7d..0x1e93df, the twin of
// EEUIEScenarioLoadEvents.cpp (the SPE unit ends at 0x1e8e7d; the HotKey unit starts at 0x1e93df). EEUIESPLLoadScreen's
// ctor and dtor are out of line after its first user (Open), the ??_G between them. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
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
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/FSRestorable.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/UIEditBox.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EEServer.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIESinglePlayerLoadEvents.h"
#include "ee/EEGameState.h"

// FUNCTION: EXE 0x5e8e7d
// Fills the saved-game list from the event's folder and extension and preselects the last loaded game or scenario
long EEUIESPLLoadScreen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theDirectory;
    FSPath* thePath = new FSPath;
    UWideString theName;
    UString theVolume(gStartupVolume);
    UString theStartup(gStartupDirectory);
    UIForm* theForm = gUIManager->GetFormByID(kFormSinglePlayerLoadGame);
    unsigned long theIndex = 0;
    UIFileListBox* theList = static_cast<UIFileListBox*>(theForm->GetControl(kSPLoadGameFileListBox));
    if (theList) {
        theList->AddValidExtension(this->mExtension);
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + this->mDirectory);
        thePath->AddDirectory(theDirectory, true);
        theList->SetSearchPaths(thePath);
        theList->CreateEntries(1);
        theName = gUIManager->mInScenarioEditor ? gLoadScenarioName : gLoadSavedGameName;
        if (theList->FindString(theName, theIndex) == true) {
            theList->ChangeSelectionIndex(theIndex, true, false);
            theList->SetScrollBarCurrentRange(theIndex);
        }
    }
    thePath->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e900e
// Loads the selected saved game: validates it, sets up the lobby options and a new local server and starts loading
// (an invalid file shows the reason; the file spec is freed on every path)
long EEUIESPLELoadFile::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UWideString theMessage;
    UString theName;
    UString theFileName;
    FSFileSpec* theFile = NULL;
    UIFileListBox* theList = static_cast<UIFileListBox*>(inForm->GetControl(kSPLoadGameFileListBox));
    unsigned long theSelection;
    if (theList->GetCurrentSelection(theSelection) == kErrorOK) {
        theName = theList->GetData(theSelection);
        theFileName = theName + this->mExtension;
        if (EEOpenSaveGameFile(theFileName, this->mDirectory, theFile) == kErrorOK && theFile->Open(kFileAccessRead) == kErrorOK) {
            if (EEValidateSaveGameFile(*theFile, theMessage)) {
                gWorld.GetLobbyOptions().GetLock()->WriteLock();
                gWorld.GetLobbyOptions().SetDefaults();
                gWorld.GetLobbyOptions().SetLoadType(2);
                gWorld.GetLobbyOptions().SetMapType((const char*)theFile->GetFile());
                gWorld.GetLobbyOptions().GetLock()->WriteUnlock();
                gLoadSavedGameName = theFile->GetFile().GetIndexedSubstring(0, theFile->GetFile().GetLength() - 5);
                if (theFile)
                    delete theFile;
                gGameState.mGameOptions.SetIsMultiplayer(false);
                if (gCommunications)
                    delete gCommunications;
                gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
                static_cast<EEServer*>(gCommunications)->StartThreads();
                static_cast<EEServer*>(gCommunications)->SetNumLoadResponses(false);
                gGameStateLoadType = 2;
                static_cast<EEServer*>(gCommunications)->StopPing();
                static_cast<EEServer*>(gCommunications)->SetLoadState(true);
                gCommunications->ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()), kSlotNone);
            } else {
                gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                delete theFile;     // an invalid file is freed (and closed) too; it leaked
            }
        } else if (theFile != NULL) {
            delete theFile;         // found but not opened: freed too (it leaked)
        }
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5e92a1
// Posts the event that fills the load screen with saved games from data\Saved Games
long EEUIESPLEOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UString theExtension;
    UString theDirectory;
    theExtension = gSavedGameExtension;
    theDirectory = UString("data\\") + UString("Saved Games\\");
    UIEvent* theEvent = new EEUIESPLLoadScreen(theExtension, theDirectory);
    gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return kErrorOK;
}

// FUNCTION: EXE 0x5e9356
// Creates the load-screen fill event for a file extension and folder
// (EEUIESPLLoadScreen's ctor, then its ??_G and dtor)

// FUNCTION: EXE 0x5e9398
// Destroys the load-screen fill event and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x5e93b4
// Destroys the load-screen fill event, releasing its extension and folder strings
