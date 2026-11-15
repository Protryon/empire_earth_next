// C:\EEX\Empire Earth\Campaign\EECampaignFormManager.cpp (file name inferred: no __FILE__). The
// campaign selection form's manager (RTTI EECampaignFormManager > UIFormManager, vtable 0x44c060): 0x28f0cb..0x28f39a.
// Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <set>
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
#include "lle/FSMemoryFile.h"
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
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"

// FUNCTION: EXE 0x68f0cb
// Creates the campaign selection form's manager at the current difficulty, with no campaign chosen
EECampaignFormManager::EECampaignFormManager()
    : mAllScenariosOpen(false)
{
    this->mDifficultyLevel = gDifficultyLevel;
    this->mCampaignName = "";
    this->mScenarioName = "";
    this->mFirstScenario = "";
    this->mChecksum = 0;
}

// SYNTHETIC: EXE 0x68f150
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x68f16c
// Destroys the campaign selection form's manager
EECampaignFormManager::~EECampaignFormManager()
{
}

// FUNCTION: EXE 0x68f1ae
// Returns the campaign scenario selection form's manager, creating and attaching it on first use
EECampaignFormManager* EECampaignFormManager::Get()
{
    UIForm* theForm = gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    EECampaignFormManager* theManager;
    if (theForm->GetFormManager() != NULL)
        theManager = static_cast<EECampaignFormManager*>(theForm->GetFormManager());
    else {
        theManager = new EECampaignFormManager;
        theForm->SetFormManager(theManager);
    }
    return theManager;
}

// FUNCTION: EXE 0x68f1f8
// Returns the expected checksum of a built-in campaign (the three AoC campaigns, the demo and the tutorial), else 0
unsigned long EECampaignFormManager::GetBuiltInCampaign(const UWideString& inName)
{
    unsigned long theResult = 0;
    if (inName.EqualsIgnoreCase(gCampaignAOCRoman))
        theResult = gAOCRomanChecksum;
    if (inName.EqualsIgnoreCase(gCampaignAOCPacific))
        theResult = gAOCPacificChecksum;
    if (inName.EqualsIgnoreCase(gCampaignAOCAsian))
        theResult = gAOCAsianChecksum;
    if (inName.EqualsIgnoreCase(gCampaignEEDemo))
        theResult = gEEDemoChecksum;
    if (inName.EqualsIgnoreCase(gCampaignEELearning))
        theResult = gEELearningChecksum;
    return theResult;
}

// FUNCTION: EXE 0x68f285
// Recomputes the checksum of the chosen campaign's file (0 when it is not found)
void EECampaignFormManager::UpdateChecksum()
{
    UWideString thePath;
    UString theName;
    theName = (const char*)(this->mCampaignName + (const char*)gCampaignExtension);
    thePath = (const char*)("Campaigns\\" + theName);
    FSFileSpec* theSpec;
    long theError = gDataPath->FindFile(UString((const char*)thePath), theSpec);
    if (theError == kErrorOK) {
        theSpec->SetFile(UString(""));
        this->mChecksum = EEGetFileChecksum(*theSpec, theName, 0);
        delete theSpec;
    } else
        this->mChecksum = 0;
}
