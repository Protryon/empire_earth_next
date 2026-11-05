// C:\EEX\Empire Earth\Scenario Editor\EESETSEffect.cpp (file name inferred: no __FILE__; RTTI class EESETSEffect). The
// trigger screen's effect sub-screen: 0x182629..0x182dda (ctor, ??_G, dtor, ChangeEffectAction/Type, the slots in the
// order of their bodies, the GetEffect COMDAT and EESEEChangeEffectType's ??_G), the twin of EESETSCondition.cpp.
// After EESETSEComboBox.cpp, before EESETSETAI.cpp. Unit flags: the exe module set.
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
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESETSEffectType.h"
#include "ee/EESETSETNoEffect.h"
#include "ee/EESETSETObject.h"
#include "ee/EESETSETPlayer.h"
#include "ee/EESETSETTrigger.h"
#include "ee/EESETSETMedia.h"
#include "ee/EESETSETGame.h"
#include "ee/EESETSETAI.h"
#include "ee/EESETSEffect.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
// FUNCTION: EXE 0x582629
// Creates the effect sub-screen: shared controls, title label, effect-type combo and the seven effect types,
// starting on the object effect
EESETSEffect::EESETSEffect(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    UWideString theText;
    std::vector<UWideString> theTypeNames;
    GE2DPoint thePosition(329.0f, 504.0f);
    GEText* theGEText;
    this->mControls = new EESETSEffectControls(inFont, inForm, ioIndex);
    gResource->GetString(IDS_TYPE_OF_EFFECT, theText);
    this->mTitleLabel = new UITextLabel(theText, inFont, kSETSEffectTitleLabel, GERectangle(thePosition, 200.0f, 20.0f),
                                  inForm, 0, ioIndex, 0);
    this->mTitleLabel->SetBgOpacity(0.4f);
    theGEText = this->mTitleLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitleLabel, 13);
    ioIndex++;
    float theTop = this->mTitleLabel->GetRectangle().GetBottom() + 4.0f;
    thePosition.SetY(theTop);
    theTypeNames.resize(6);
    for (unsigned long i = 0; i < 6; i++)
        gResource->GetString(IDS_TRIGGER_EFFECT_TYPE_FIRST + i, theTypeNames[i]);
    this->mTypeComboBox = new UIComboBox(kSETSEffectTypeComboBox, 1, theTypeNames, inFont, GERectangle(thePosition, 200.0f, 20.0f),
                                   0x190, inForm, ioIndex, 0, 2);
    UIEvent* theEvent = new EESEEChangeEffectType;
    this->mTypeComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mTypeComboBox->SetTextBoxBgOpacity(0.4f);
    this->mTypeComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mTypeComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mTypeComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mTypeComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTypeComboBox, 13);
    ioIndex++;
    this->mTypes[kSMEffectNone] = new EESETSETNoEffect;
    this->mTypes[kSMEffectObject] = new EESETSETObject;
    this->mTypes[kSMEffectPlayer] = new EESETSETPlayer;
    this->mTypes[kSMEffectTrigger] = new EESETSETTrigger;
    this->mTypes[kSMEffectMedia] = new EESETSETMedia;
    this->mTypes[kSMEffectGame] = new EESETSETGame;
    this->mTypes[kSMEffectAI] = new EESETSETAI;
    this->mType = 1;
}

// SYNTHETIC: EXE 0x582988
// (EESETSEffect's scalar deleting destructor)

// FUNCTION: EXE 0x5829a4
// Destroys the effect sub-screen with its controls and the seven effect types
EESETSEffect::~EESETSEffect()
{
    delete this->mControls;
    delete this->mTitleLabel;
    delete this->mTypeComboBox;
    delete this->mTypes[0];
    delete this->mTypes[1];
    delete this->mTypes[2];
    delete this->mTypes[3];
    delete this->mTypes[4];
    delete this->mTypes[5];
    delete this->mTypes[6];
}

// FUNCTION: EXE 0x582a3b
// Passes an action change on the effect page to the current effect type for the selected effect
void EESETSEffect::ChangeEffectAction()
{
    unsigned long theIndex = this->mSelection + 1;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    this->mTypes[this->mType]->ChangeAction(theManager->GetEffect(theIndex), this->mControls);
}

// FUNCTION: EXE 0x582a69
// Switches the selected effect to the type picked in the combo: hides the old type, shows and loads the new
void EESETSEffect::ChangeEffectType()
{
    EESMEffect* theEffect = gWorld.mScenarioManager->GetEffect(this->mSelection + 1);
    unsigned long theSelection = 0;
    this->mTypeComboBox->GetCurrentSelection(theSelection);
    unsigned long theType = theSelection + 1;
    if (theType == this->mType)
        return;
    this->mTypes[this->mType]->VirtualC(this->mControls);
    this->mType = theType;
    this->mTypes[this->mType]->Show(this->mControls);
    this->mTypes[this->mType]->Load(theEffect, this->mControls);
}

// FUNCTION: EXE 0x582ac0
// Sorts the scenario's effects
void EESETSEffect::Sort()
{
    gWorld.mScenarioManager->SortEffects();
}

// FUNCTION: EXE 0x582acb
// Moves the selected effect one place down the list
void EESETSEffect::MoveDown()
{
    gWorld.mScenarioManager->MoveEffectDown(this->mSelection + 1);
}

// FUNCTION: EXE 0x582adc
// Moves the selected effect one place up the list
void EESETSEffect::MoveUp()
{
    gWorld.mScenarioManager->MoveEffectUp(this->mSelection + 1);
}

// FUNCTION: EXE 0x582aed
// Deletes the selected effect and hides the current effect type's controls
void EESETSEffect::Delete()
{
    gWorld.mScenarioManager->DeleteEffect(this->mSelection + 1);
    this->mTypes[this->mType]->VirtualC(this->mControls);
}

// FUNCTION: EXE 0x582b11
// Duplicates the selected effect
void EESETSEffect::Duplicate()
{
    gWorld.mScenarioManager->DuplicateEffect(this->mSelection + 1);
}

// FUNCTION: EXE 0x582b22
// Opens the effect page: sets the player pickers' player count, shows the current type, hides the text box
void EESETSEffect::Activate()
{
    unsigned long thePlayer =
        ((EESEPlayerScreen*)(static_cast<EEUIFScenarioEditor*>(this->mTitleLabel->GetForm()))->mTabGroups[kEditorTabPlayers])->GetPlayerCount();
    for (long i = 0; i < 3; i++)
        this->mControls->mPlayers[i]->mPlayerCount = thePlayer;
    this->mTypes[this->mType]->Show(this->mControls);
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowAlphaSort();
}

// FUNCTION: EXE 0x582b96
// Writes the page's name, text, type and type settings into the selected effect
void EESETSEffect::Store()
{
    UWideString theText;
    EESMEffect* theEffect = gWorld.mScenarioManager->GetEffect(this->mSelection + 1);
    theEffect->SetName(UWideString(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName()));
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
    theEffect->SetText(theText);
    this->mTypes[this->mType]->Store(theEffect, this->mControls);
    theEffect->SetType(this->mType);
}

// FUNCTION: EXE 0x582c43
// Lists the names of the scenario's user effects
void EESETSEffect::GetNames(std::vector<UWideString>& outNames)
{
    gWorld.mScenarioManager->GetUserEffectNames(outNames);
}

// FUNCTION: EXE 0x582c55
// Shows the selected effect's name and text, switches the page to its type and loads its settings
// (the no-effect type is shown twice)
void EESETSEffect::Load()
{
    EESMEffect* theEffect = gWorld.mScenarioManager->GetEffect(this->mSelection + 1);
    if (theEffect != NULL) {
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowText();
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theEffect->GetName());
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->SetText(theEffect->GetText());
        this->mTypes[this->mType]->VirtualC(this->mControls);
        this->mType = theEffect->GetType();
        if (this->mType != 0) {
            this->mTypeComboBox->SetCurrentSelection(this->mType - 1);
            unsigned long theSelection;
            this->mTypeComboBox->GetCurrentSelection(theSelection);
            if (this->mType == theSelection + 1)
                this->mTypes[this->mType]->Show(this->mControls);
        } else
            this->mTypes[this->mType]->Show(this->mControls);
        if (this->mType == 0)
            this->mTypes[this->mType]->Show(this->mControls);
        this->mTypes[this->mType]->Load(theEffect, this->mControls);
    }
}

// FUNCTION: EXE 0x582d35
// Adds a new effect to the scenario
void EESETSEffect::New()
{
    gWorld.mScenarioManager->NewEffect();
}

// FUNCTION: EXE 0x582d40
// Lets the current effect type regenerate the selected effect's automatic name
void EESETSEffect::RefreshControls()
{
    EESMEffect* theEffect = gWorld.mScenarioManager->GetEffect(this->mSelection + 1);
    if (theEffect != NULL)
        this->mTypes[this->mType]->UpdateName(theEffect, this->mControls);
}

// FUNCTION: EXE 0x582d69
// Shows the effect page's type combo and title
void EESETSEffect::Show()
{
    this->mTypeComboBox->SetDisplayMode(kDisplayModeShown);
    this->mTitleLabel->SetDisplayMode(kDisplayModeShown);
}

// ICF-FOLDED: EXE 0x57c5d9
// Hides the effect page's type combo and title (folded with EESETriggerScreen::HideText in the original: the same
// two member offsets)
void EESETSEffect::Hide()
{
    this->mTypeComboBox->SetDisplayMode(kDisplayModeHidden);
    this->mTitleLabel->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x582d82
// Returns the scenario effect with the given index (EEScenarioManager::GetEffect)
// (inline, ee/EEScenarioManager.h)

// SYNTHETIC: EXE 0x582daf
// (EESEEChangeEffectType's scalar deleting destructor)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESETSEffect::Deactivate()
{
}
