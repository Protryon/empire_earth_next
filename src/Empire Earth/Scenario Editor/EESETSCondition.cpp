// C:\EEX\Empire Earth\Scenario Editor\EESETSCondition.cpp (file name inferred: no __FILE__; RTTI class EESETSCondition).
// The trigger screen's condition sub-screen: 0x17e49e..0x17ef10 (ctor, ??_G, dtor, ChangeConditionType, the slots in
// vtable order of their bodies, then the COMDATs this unit is the first user of: EESETriggerScreen::GetName 0x17eaab,
// the events' ??_G, EESETSCTPlayerResource's ctor and the condition types' ??_G). After the condition controls' unit
// (the EESETSCComboBox members and EESETSConditionControls' dtor 0x17e3d7), before EESETSConditionType.cpp
// (0x17ef10). Unit flags: the exe module set.
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

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESMCondition.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSConditionType.h"
#include "ee/EESETSCondition.h"
#include "ee/EESEEvents.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
// FUNCTION: EXE 0x57e49e
// Creates the condition sub-screen: shared controls, title label, condition-type combo and the seven condition
// types, starting on the object condition
EESETSCondition::EESETSCondition(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    UWideString theText;
    std::vector<UWideString> theTypeNames;
    GE2DPoint thePosition;
    GEText* theGEText;
    thePosition.SetXY(329.0f, 504.0f);
    this->mControls = new EESETSConditionControls(inFont, inForm, ioIndex);
    gResource->GetString(IDS_TYPE_OF_CONDITION, theText);
    this->mTitleLabel = new UITextLabel(theText, inFont, kSETSConditionTitleLabel, GERectangle(thePosition, 200.0f, 20.0f),
                                  inForm, 0, ioIndex, 0);
    this->mTitleLabel->SetBgOpacity(0.4f);
    theGEText = this->mTitleLabel->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mTitleLabel, 12);
    ioIndex++;
    float theTop = this->mTitleLabel->GetRectangle().GetBottom() + 4.0f;
    thePosition.SetY(theTop);
    theTypeNames.resize(6);
    for (unsigned long i = 0; i < 6; i++)
        gResource->GetString(IDS_TRIGGER_CONDITION_TYPE_FIRST + i, theTypeNames[i]);
    this->mTypeComboBox = new UIComboBox(kSETSConditionTypeComboBox, 1, theTypeNames, inFont, GERectangle(thePosition, 200.0f, 20.0f),
                                   0x190, inForm, ioIndex, 0, 2);
    UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SE_CHANGE_CONDITION_TYPE);
    this->mTypeComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    this->mTypeComboBox->SetTextBoxBgOpacity(0.4f);
    this->mTypeComboBox->SetListBoxAlwaysDirty(true);
    theGEText = this->mTypeComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    this->mTypeComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mTypeComboBox->SetBgOpacity(1.0f);
    inForm->AddControl(this->mTypeComboBox, 12);
    ioIndex++;
    this->mTypes[kSMConditionChatMessageContains] = new EESETSCTChatMessage;
    this->mTypes[kSMConditionTriggerAttribute] = new EESETSCTTriggerAttribute;
    this->mTypes[kSMConditionPlayerAttribute] = new EESETSCTPlayerResource;
    this->mTypes[kSMConditionPlayerTechnology] = new EESETSCTPlayerResearch;
    this->mTypes[kSMConditionAlwaysTrue] = new EESETSCTAlwaysTrue;
    this->mTypes[kSMConditionPlayerDiplomacy] = new EESETSCTDiplomacy;
    this->mTypes[kSMConditionGameAttribute] = new EESETSCTGame;
    this->mType = kSMConditionObject;
}

// SYNTHETIC: EXE 0x57e80b
// (EESETSCondition's scalar deleting destructor)

// FUNCTION: EXE 0x57e827
// Destroys the condition sub-screen with its controls and the seven condition types
EESETSCondition::~EESETSCondition()
{
    delete this->mControls;
    delete this->mTypeComboBox;
    delete this->mTitleLabel;
    delete this->mTypes[7];
    delete this->mTypes[4];
    delete this->mTypes[2];
    delete this->mTypes[3];
    delete this->mTypes[0];
    delete this->mTypes[5];
    delete this->mTypes[6];
}

// FUNCTION: EXE 0x57e8be
// Switches the selected condition to the type picked in the combo: hides the old type, shows and loads the new
void EESETSCondition::ChangeConditionType()
{
    EESMCondition* theCondition = gWorld.mScenarioManager->GetCondition(this->mSelection + 1);
    unsigned long theSelection = 0;
    this->mTypeComboBox->GetCurrentSelection(theSelection);
    unsigned long theType = theSelection + 2;
    if (theType == this->mType)
        return;
    this->mTypes[this->mType]->Hide(this->mControls);
    this->mType = theType;
    this->mTypes[this->mType]->Show(this->mControls);
    this->mTypes[this->mType]->Load(theCondition, this->mControls);
    this->RefreshControls();
}

// FUNCTION: EXE 0x57e91d
// Sorts the scenario's conditions
void EESETSCondition::Sort()
{
    gWorld.mScenarioManager->SortConditions();
}

// FUNCTION: EXE 0x57e928
// Moves the selected condition one place down the list
void EESETSCondition::MoveDown()
{
    gWorld.mScenarioManager->MoveConditionDown(this->mSelection + 1);
}

// FUNCTION: EXE 0x57e939
// Moves the selected condition one place up the list
void EESETSCondition::MoveUp()
{
    gWorld.mScenarioManager->MoveConditionUp(this->mSelection + 1);
}

// FUNCTION: EXE 0x57e94a
// Deletes the selected condition
void EESETSCondition::Delete()
{
    gWorld.mScenarioManager->DeleteCondition(this->mSelection + 1);
}

// FUNCTION: EXE 0x57e95b
// Duplicates the selected condition
void EESETSCondition::Duplicate()
{
    gWorld.mScenarioManager->DuplicateCondition(this->mSelection + 1);
}

// FUNCTION: EXE 0x57e96c
// Opens the condition page: sets the player pickers' player count, hides the text box, shows the current
// type (player attribute in place of always-true/object) and refreshes the name
void EESETSCondition::Activate()
{
    unsigned long thePlayer =
        ((EESEPlayerScreen*)(static_cast<EEUIFScenarioEditor*>(this->mTitleLabel->GetForm()))->mTabGroups[kEditorTabPlayers])->GetPlayerCount();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowAlphaSort();
    for (long i = 0; i < 2; i++)
        this->mControls->mPlayers[i]->mPlayerCount = thePlayer;
    if (this->mType == kSMConditionAlwaysTrue || this->mType == kSMConditionObject)
        this->mType = kSMConditionPlayerAttribute;
    this->mTypes[this->mType]->Show(this->mControls);
    this->RefreshControls();
}

// FUNCTION: EXE 0x57e9fb
// Writes the page's name, text and type settings into the selected condition (not into an always-true one)
void EESETSCondition::Store()
{
    UWideString theText;
    EESMCondition* theCondition = gWorld.mScenarioManager->GetCondition(this->mSelection + 1);
    if (theCondition->GetType() != kSMConditionAlwaysTrue) {
        theCondition->SetName(UWideString(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName()));
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
        theCondition->SetText(theText);
        this->mTypes[this->mType]->Store(theCondition, this->mControls);
        theCondition->SetType(this->mType);
    }
}

// FUNCTION: EXE 0x57eaab
// Returns the trigger screen's name edit box text (EESETriggerScreen::GetName)
// (inline, ee/EESETriggerScreen.h)

// FUNCTION: EXE 0x57eab8
// Lists the names of the scenario's user conditions
void EESETSCondition::GetNames(std::vector<UWideString>& outNames)
{
    gWorld.mScenarioManager->GetUserConditionNames(outNames);
}

// FUNCTION: EXE 0x57eaca
// Hides the condition page's title and type combo
void EESETSCondition::Hide()
{
    this->mTitleLabel->SetDisplayMode(kDisplayModeHidden);
    this->mTypeComboBox->SetDisplayMode(kDisplayModeHidden);
}

// FUNCTION: EXE 0x57eae3
// Shows the selected condition's name and text and switches the page to its type, loading its settings
// (object conditions keep their own page)
void EESETSCondition::Load()
{
    EESMCondition* theCondition = gWorld.mScenarioManager->GetCondition(this->mSelection + 1);
    if (theCondition != NULL) {
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowText();
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theCondition->GetName());
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->SetText(theCondition->GetText());
        if (this->mType != theCondition->GetType()) {
            this->mTypes[this->mType]->Hide(this->mControls);
            this->mType = theCondition->GetType();
            if (this->mType > kSMConditionObject)
                this->mTypeComboBox->SetCurrentSelection(theCondition->GetType() - 2);
            if (theCondition->GetType() != kSMConditionObject)
                this->mTypes[this->mType]->Show(this->mControls);
        }
        if (theCondition->GetType() != kSMConditionObject) {
            this->mTypes[this->mType]->Load(theCondition, this->mControls);
            this->RefreshControls();
        }
    }
}

// FUNCTION: EXE 0x57ebb7
// Adds a new condition to the scenario
void EESETSCondition::New()
{
    gWorld.mScenarioManager->NewCondition();
}

// FUNCTION: EXE 0x57ebc2
// Regenerates an automatic '<...>' condition name from its type, player, chat text or trigger and shows it
void EESETSCondition::RefreshControls()
{
    UWideString theName;
    UWideString theTypeName;
    UWideString thePlayerName;
    unsigned long theIndex = this->mSelection + 1;
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    EESMCondition* theCondition = theManager->GetCondition(theIndex);
    if (theCondition != NULL) {
        theName = theCondition->GetName();
        if (theName.GetLength() > 0 && theName[0] == '<') {
            gResource->GetString(this->mType + IDS_TRIGGER_CONDITION_TYPE_BASE, theTypeName);
            unsigned long theSpace = theTypeName.Find(" ", 0);
            if (theSpace > 0)
                theName.CopyString(theTypeName, theSpace);
            else
                theName = theTypeName;
            if (this->mType == kSMConditionPlayerTechnology || this->mType == kSMConditionPlayerDiplomacy || this->mType == kSMConditionPlayerAttribute) {
                unsigned long thePlayer = this->mControls->mPlayers[0]->GetSelectedPlayer();
                if (thePlayer <= 6)
                    gResource->GetString(thePlayer + IDS_TRIGGER_PLAYER_FIRST, thePlayerName);
                else {
                    gResource->GetString(IDS_TRIGGER_PLAYER, thePlayerName);
                    sprintf(thePlayerName, thePlayerName, thePlayer - 6);
                }
                theName += " ";
                theName += thePlayerName;
            }
            switch (this->mType) {
            case kSMConditionChatMessageContains:
                theName += "=\"";
                theName += this->mControls->mTextEditBox->GetText(0);
                theName += "\"";
                break;
            case kSMConditionTriggerAttribute:
                theName += this->mControls->mScenarioComboBoxes[0]->GetTextLabel()->GetText(0);
                break;
            }
            theName = "<" + theName + ">";
            theCondition->SetName(theName);
            gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(theName);
        }
    }
}

// FUNCTION: EXE 0x57edfc
// Shows the condition page's title and type combo
void EESETSCondition::Show()
{
    this->mTitleLabel->SetDisplayMode(kDisplayModeShown);
    this->mTypeComboBox->SetDisplayMode(kDisplayModeShown);
}

// FUNCTION: EXE 0x57ee40
// Creates the player-attribute condition type (EESETSCTPlayerResource ctor)
// (inline, ee/EESETSConditionType.h)

// SYNTHETIC: EXE 0x57ee9c
// (EESETSCTPlayerResource's scalar deleting destructor)

// SYNTHETIC: EXE 0x57eec8
// (EESETSCTPlayerResearch's scalar deleting destructor)

// SYNTHETIC: EXE 0x57eef3
// (EESETSConditionType's scalar deleting destructor, shared by the vptr-only types)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESETSCondition::Deactivate()
{
}
