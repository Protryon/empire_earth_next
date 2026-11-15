// C:\EEX\Empire Earth\Scenario Editor Actions\EEAQuickSaveLoad.cpp (file name inferred: no __FILE__; RTTI class
// EEAQuickSaveLoad, the base of EESEAQuickSaveLoad). The quick save/load action: 0x1a5529..0x1a5881 (Virtual4, the
// FSMemoryFile ??_G its new emits, SaveToFile). After EEUIFSEUpgradePanel.cpp (the Scenario Editor group), the first
// unit of the Scenario Editor Actions group (EEA* sorts before EESEA*).
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
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
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/FSMemoryFile.h"
#include "lle/SEMixer.h"
#include "lle/NESynchronousServer.h"
#include "ee/EEPhysics.h"
#include "ee/EECommunications.h"
#include "ee/EEGameSound.h"
#include "ee/EESaveLoad.h"
#include "ee/EEAQuickSaveLoad.h"

// FUNCTION: EXE 0x5a5529
// Quick-loads (flag mIsLoad set) or quick-saves EEQuickSave<name> with sound muted and a hosted game paused: loads
// from data\Scenarios (error box on failure), or saves into a new memory file and, if asked, writes it to disk
void EEAQuickSaveLoad::Virtual4()
{
    FSFileSpec theFile;
    UWideString theMessage;
    UString theName;
    UString theFileName;
    bool thePaused = gCommunications->GetIsGamePaused();

    gUserInterface->UpdateJITManufacturing();
    theName = "EEQuickSave";
    theName += this->mName;
    float theMasterVolume = gSoundManager->GetMasterVolume();
    float theMusicVolume = gConductor->GetConductor().GetVolume();
    gSoundManager->SetMasterVolume(0.0f);
    gConductor->GetConductor().SetVolume(0.0f);
    if (!thePaused && gCommunications->IsServer()) {
        static_cast<NESynchronousServer*>(gCommunications)->PauseGame();
        gUserInterface->PostPauseInformation(true);
    }
    if (this->mIsLoad) {
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        theFile.SetFile(theName);
        if (LoadSavedGameFile(theFile, true, true, true) != 0)
            gUIManager->DisplayMessage(gLoadSaveErrorMessage, 1, NULL, NULL, true);
    } else {
        if (this->mFile != NULL) {
            delete this->mFile;
            this->mFile = NULL;
        }
        this->mFile = new FSMemoryFile(0x400, true, 1000);
        theFileName = GetSaveTextName(theName, true);
        WriteSaveGame(theFileName, TMGetTime(), *this->mFile, *this->mFile);
        if (this->mWriteToFile == true)
            this->SaveToFile(theFileName);
    }
    if (!thePaused && gCommunications->IsServer()) {
        static_cast<NESynchronousServer*>(gCommunications)->UnpauseGame();
        gUserInterface->PostPauseInformation(false);
    }
    gSoundManager->SetMasterVolume(theMasterVolume);
    gConductor->GetConductor().SetVolume(theMusicVolume);
    gPhysics->mTicksOwed = 0;
}

// FUNCTION: EXE 0x5a57b6
// Destroys a memory file and frees it if asked (FSMemoryFile's scalar deleting destructor)
// (emitted by its first user Virtual4)

// FUNCTION: EXE 0x5a57d3
// Writes the quick save memory file to data\Scenarios\<inName>, if the file opens
void EEAQuickSaveLoad::SaveToFile(const UString& inName)
{
    FSFileSpec theFile;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    theFile.SetFile(inName);
    theFile.Create();
    if (theFile.Open(kFileAccessWrite) == 0) {
        this->mFile->Save(theFile);
        theFile.Close();
    }
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAQuickSaveLoad::Virtual8()
{
}
