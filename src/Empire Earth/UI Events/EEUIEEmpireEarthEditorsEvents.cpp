// C:\EEX\Empire Earth\UI Events\EEUIEEmpireEarthEditorsEvents.cpp (file name inferred: no __FILE__; from the class
// prefix EEUIEEEE, sorted between EEUIEDiplomacyScreenEvents and EEUIEFormationChange). 0x1d6775..0x1d6886: the init
// chain of the file-static civilization handed to the civilization builder (0x962f08), then the five ProcessEvents
// in vtable-slot address order. Unit flags: the exe's module set.
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
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivilization.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEEmpireEarthEditorsEvents.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"

static EECivilization sCivilization;        // 0x962f08

// SYNTHETIC: EXE 0x5d6775   _$E59 (wrapper)
// SYNTHETIC: EXE 0x5d677f   _$E56 (ctor)
// SYNTHETIC: EXE 0x5d6789   _$E58 (registrar)
// SYNTHETIC: EXE 0x5d6795   _$E57 (dtor)

// FUNCTION: EXE 0x5d679f
// Opens the game tools submenu as single player, outside the scenario editor
long EEUIEEEESubMenuOpen::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static_cast<EEUIManager*>(inForm->GetManager())->mInScenarioEditor = false;
    gGameState.mGameOptions.mIsMultiplayer = false;
    inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormGameTools));
    return kErrorOK;
}

// FUNCTION: EXE 0x5d67d8
// Opens the campaign editor as single player, outside the scenario editor
long EEUIEEEEEditor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static_cast<EEUIManager*>(inForm->GetManager())->mInScenarioEditor = false;
    gGameState.mGameOptions.mIsMultiplayer = false;
    inForm->GetManager()->Push(inForm->GetManager()->GetFormByID(kFormCampaignEditor));
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6811
// Returns to the main menu
long EEUIEEEEMainMenu::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetManager()->Pop();
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6826
// Creates and starts the scenario editor
long EEUIEEEEScenarioEditor::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    theManager->CreateScenarioEditor();
    theManager->StartScenarioEditor();
    return kErrorOK;
}

// FUNCTION: EXE 0x5d6845
// Opens the civilization builder on a fresh custom civilization with the world table's starting points
long EEUIEEEECivBuilder::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    sCivilization.mPurchasedUpgrades.clear();
    sCivilization.mAvailablePoints = sCivilization.mMaxPoints;
    sCivilization.SetPointsAndMax(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
    gUserInterface->OpenCivilizationBuilder(&sCivilization, false);
    return kErrorOK;
}
