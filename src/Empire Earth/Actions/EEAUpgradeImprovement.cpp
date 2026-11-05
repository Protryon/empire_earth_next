// C:\EEX\Empire Earth\Actions\EEAUpgradeImprovement.cpp (file name inferred: no __FILE__; RTTI class
// EEAUpgradeImprovement, vtable 0x4390d4). The improvement action: 0x505b..0x5280 (EEAServerMigration's code ends at
// 0x505b, EEAUserPauseGame's Virtual4 starts at 0x5280). ctor, ??_G, dtor, Virtual4 with the inline COMDATs it uses
// first (EEMImproveUnit::GetMessageLabel, EEUIFScenarioEditor::GetUnitScreen, EESoundObject::Get).
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
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGameSound.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassImprovement.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEAUpgradeImprovement.h"

// FUNCTION: EXE 0x40505b
// Creates the improvement action for a unit (holding a reference) and posts it
EEAUpgradeImprovement::EEAUpgradeImprovement(EEUnit* inUnit, long inImprovement, bool inRaise, bool inIsInGame)
    : mUnit(inUnit)
{
    this->mUnit->Reference();
    this->mImprovement = inImprovement;
    this->mRaise = inRaise;
    this->mIsInGame = inIsInGame;
    gUserInterface->PostAction(this);
}

// SYNTHETIC: EXE 0x4050a0
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4050bc
// Destroys the improvement action, releasing its unit
EEAUpgradeImprovement::~EEAUpgradeImprovement()
{
    this->mUnit->Dereference();
}

// FUNCTION: EXE 0x4050dc
// Raises or lowers a unit's improvement: by message in a game, directly in the scenario editor (refreshing its screen)
void EEAUpgradeImprovement::Virtual4()
{
    EEUnitClassUpgrade* theStats = this->mUnit->mStats;
    if (this->mIsInGame == true) {
        if (this->mRaise == true)
            gCommunications->ProcessLocalUserInput(new EEMImproveUnit(this->mUnit->GetUnitID(), this->mImprovement), kSlotNone);
        else
            gCommunications->ProcessLocalUserInput(new EEMRemoveImprovement(this->mUnit->GetUnitID(), this->mImprovement), kSlotNone);
    } else if (this->mRaise == true) {
        this->mUnit->GetPlayer()->ApplyImprovement(this->mUnit->GetUnitClass()->GetUnitType(), this->mImprovement, false);
        gUIManager->mScenarioEditorForm->GetUnitScreen()->ShowUnitStatus(this->mUnit);
        gUIManager->mScenarioEditorForm->UpdateHelpString();
    } else if (theStats->mImprovement.GetLevel(this->mImprovement) > 1) {
        this->mUnit->GetPlayer()->ApplyImprovement(this->mUnit->GetUnitClass()->GetUnitType(), this->mImprovement, true);
        static_cast<EESEUnit*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabUnits])->ShowUnitStatus(this->mUnit);
        gUIManager->mScenarioEditorForm->UpdateHelpString();
        gSoundManager->Play(EESoundObject::Get(kSoundButton1)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    } else {
        gSoundManager->Play(EESoundObject::Get(kSoundButton2)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
    }
    gUserInterface->UpdateJITManufacturing();
}

// FUNCTION: EXE 0x40524d
// Returns the improve-unit message's label
// (EEMImproveUnit::GetMessageLabel, inline in ee/EEMessage.h)

// FUNCTION: EXE 0x405253
// Returns the scenario editor's unit screen
// (EEUIFScenarioEditor::GetUnitScreen, inline in ee/EEUIFScenarioEditor.h)

// FUNCTION: EXE 0x40525d
// Returns the sound object at an index
// (EESoundObject::Get, inline in ee/EESoundObject.h)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAUpgradeImprovement::Virtual8()
{
}
