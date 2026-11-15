// C:\EEX\Empire Earth\Scenario Editor\EESETSEffect.cpp (file name inferred: no __FILE__; RTTI class EESETSEffect). The
// trigger screen's effect sub-screen: 0x182629..0x182dda (ctor, ??_G, dtor, ChangeEffectAction/Type, the slots in the
// order of their bodies, the GetEffect COMDAT and EESEEChangeEffectType's ??_G), the twin of EESETSCondition.cpp.
// After EESETSEComboBox.cpp, before EESETSETAI.cpp. Unit flags: the exe module set.
// Its seven effect types (EESETSEffectType and every EESETSET* class, once EESETSETAI.cpp .. EESETSETSetObjectStance.cpp,
// 0x182dda..0x18efb7) are whole in Rust (ee_screens editor_effects.rs), as the condition screen's seven condition
// types (EESETSConditionType and its EESETSCT* classes, once EESETSConditionType.cpp; editor_conditions.rs): this
// unit keeps their two hosts, ee_screenscpp_seet and ee_screenscpp_sect, one template over the effect or condition
// screen's controls by array and index.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <map>
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
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEComboBox.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EESETSCComboBox.h"
#include "ee/EESMCondition.h"
#include "ee/EESEARust.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EESETSEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMTrigger.h"
#include "ee/EECPBehavior.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UIESliderChange.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScenarioIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
// The screen is whole in Rust (ee_screens editor_subscreens.rs) with its seven effect types (editor_effects.rs): the
// constructor (0x582629), the destructor (0x5829a4), ChangeEffectAction (0x582a3b), ChangeEffectType (0x582a69),
// Sort, MoveDown, MoveUp, Delete, Duplicate, Activate (0x582b22), Store (0x582b96), GetNames, Load (0x582c55), New,
// RefreshControls, Show and Hide are its slots; the class forwards.

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// FUNCTION: EXE 0x582629
// Creates the effect sub-screen: shared controls, title label, effect-type combo and the seven effect types,
// starting on the object effect (editor_subscreens.rs)
EESETSEffect::EESETSEffect(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_sefx_new(inForm, &theIndex);
    ioIndex = theIndex;
}

// SYNTHETIC: EXE 0x582988
// (EESETSEffect's scalar deleting destructor)

// FUNCTION: EXE 0x5829a4
// Destroys the effect sub-screen with its controls and the seven effect types (editor_subscreens.rs)
EESETSEffect::~EESETSEffect()
{
    ee_screens_sefx_free(this->mRust);
}

void EESETSEffect::Call(unsigned long inWhat)
{
    ee_screens_sefx_call(this->mRust, (uint32_t)inWhat, (int32_t)this->mSelection);
}

void EESETSEffect::ChangeEffectAction() { this->Call(EE_SCREENS_SESC_CHANGE_ACTION); }
void EESETSEffect::ChangeEffectType() { this->Call(EE_SCREENS_SESC_CHANGE_TYPE); }
void EESETSEffect::Sort() { this->Call(EE_SCREENS_SESC_SORT); }
void EESETSEffect::MoveDown() { this->Call(EE_SCREENS_SESC_MOVE_DOWN); }
void EESETSEffect::MoveUp() { this->Call(EE_SCREENS_SESC_MOVE_UP); }
void EESETSEffect::Delete() { this->Call(EE_SCREENS_SESC_DELETE); }
void EESETSEffect::Duplicate() { this->Call(EE_SCREENS_SESC_DUPLICATE); }
void EESETSEffect::Activate() { this->Call(EE_SCREENS_SESC_ACTIVATE); }
void EESETSEffect::Store() { this->Call(EE_SCREENS_SESC_STORE); }
void EESETSEffect::Load() { this->Call(EE_SCREENS_SESC_LOAD); }
void EESETSEffect::New() { this->Call(EE_SCREENS_SESC_NEW); }
void EESETSEffect::RefreshControls() { this->Call(EE_SCREENS_SESC_REFRESH); }
void EESETSEffect::Show() { this->Call(EE_SCREENS_SESC_SHOW); }
// ICF-FOLDED: EXE 0x57c5d9
// (folded with EESETriggerScreen::HideText in the original: the same two member offsets)
void EESETSEffect::Hide() { this->Call(EE_SCREENS_SESC_HIDE); }
void EESETSEffect::GetNames(std::vector<UWideString>& outNames) { ee_screens_sefx_names(this->mRust, &outNames); }

EESETSEffectControls* EESETSEffect::GetControls() const
{
    return static_cast<EESETSEffectControls*>(ee_screens_sefx_handle(this->mRust, 0));
}

UIComboBox* EESETSEffect::GetTypeComboBox() const
{
    return static_cast<UIComboBox*>(static_cast<UIControl*>(ee_screens_sefx_handle(this->mRust, 1)));
}

// FUNCTION: EXE 0x582d82
// Returns the scenario effect with the given index (EEScenarioManager::GetEffect)
// (inline, ee/EEScenarioManager.h)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESETSEffect::Deactivate()
{
}

// ------------------------------------------------------------------------------------------------ the types' host

void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);   // EESEEvents.cpp

// FUNCTION: EXE 0x58b1b0
// Switches a scenario combo to areas or objects: sets its label, fills it with the names and selects the first silently
// (once EESETSETPlayer.cpp; the camera and flare types' loads and their radios' event)
void FillAreaObjectComboBox(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, bool inAreas)
{
    std::vector<UWideString> theNames;
    UWideString theText;
    if (inAreas) {
        inComboBox->mMode = kSETriggerScreenArea;
        gResource->GetString(IDS_AREA, theText);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_AREAS, theNames);
    } else {
        inComboBox->mMode = kSETriggerScreenObject;
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theText);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_OBJECTS, theNames);
    }
    inLabel->SetText(theText);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
    UIEvent* theAction = inComboBox->GetTriggerEvent(kControlTriggerSelectionChanged);
    if (theAction)
        theAction->Reference();
    inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
    inComboBox->SetCurrentSelection(0);
    if (theAction) {
        inComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x584a90
// Sets the place label to 'Area' or 'Object' and fills the place combo with the scenario's area or object names
// (once EESETSETGame.cpp; the place radios' event, ee_screens scenario_editor.rs, calls it through EESEEvents.cpp)
void FillPlaceComboBox(bool inAreas, UITextLabel* inLabel, UIComboBox* inComboBox)
{
    std::vector<UWideString> theNames;
    UWideString theTitle;
    if (inAreas) {
        gResource->GetString(IDS_AREA, theTitle);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_AREAS, theNames);
    } else {
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_OBJECT, theTitle);
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_OBJECTS, theNames);
    }
    inLabel->SetText(theTitle);
    inComboBox->DeleteAllEntries();
    for (unsigned long i = 0; i < theNames.size(); i++)
        inComboBox->AppendLine(theNames[i]);
}

namespace {

static_assert(kSETriggerScreenTrigger == 0 && kSETriggerScreenObject == 3 && kSETriggerScreenArea == 4 &&
                  kSETSEffectTitleLabel == 0x960 && kSETSEffectTypeComboBox == 0x961 &&
                  kSETriggerScreenNameEditBox == 0x7db && kSETriggerScreenNameLabel == 0x7dc &&
                  kSETriggerScreenDuplicateButton == 0x7d6 && kSETriggerScreenDeleteButton == 0x7d7 &&
                  kControlTriggerRangeChanged == 0x3e8 && kControlTriggerChecked == 0x3e9 &&
                  kControlTriggerSelectionChanged == 0x3eb && kControlTriggerButtonUp == 0x3f0 &&
                  kTextAlignLeft == 0 && kTextAlignCenter == 1 && kTextAlignRight == 2 &&
                  kSMEffectNone == 0 && kSMEffectObject == 1 && kSMEffectPlayer == 2 && kSMEffectTrigger == 3 &&
                  kSMEffectMedia == 4 && kSMEffectGame == 5 && kSMEffectAI == 6,
              "editor_effects.rs's values");
static_assert(kSMConditionAlwaysTrue == 0 && kSMConditionPlayerAttribute == 2 && kSMConditionPlayerTechnology == 3 &&
                  kSMConditionTriggerAttribute == 4 && kSMConditionPlayerDiplomacy == 5 &&
                  kSMConditionGameAttribute == 6 && kSMConditionChatMessageContains == 7 &&
                  kSETSConditionTitleLabel == 0x8fc && kSETSConditionTypeComboBox == 0x8fd &&
                  kControlTriggerUnchecked == 0x3ea && kControlTriggerEnterPressed == 0x3f1 &&
                  kSEECheckDisplay == EE_SCREENS_SEE_CHECK_DISPLAY,
              "editor_conditions.rs's values");

// a technology's name in the editor (as EESETSConditionType.cpp's EESETSCTResearchNameLess; once EESETSETPlayer.cpp)
UWideString SeetTechName(long inID)
{
    UWideString theName;
    EETechTreeNode* theNode = gTechTree.Get(inID);
    if (theNode) {
        RTSTechTreeNodeData* theData = theNode->mData;
        if (theData->mUnitClass >= 0 && gUnitClassTable[theData->mUnitClass])
            gResource->GetString(gUnitClassTable[theData->mUnitClass]->mNameStringID, theName);
        else
            gResource->GetString(theData->mNameID, theName);
    } else
        theName = "";
    return theName;
}

struct SeetTechNameLess {
    bool operator()(const long& inLeft, const long& inRight) const { return SeetTechName(inLeft) < SeetTechName(inRight); }
};

}   // namespace

// TEST ONLY: the alignment each label was last given through the host (the editor gate's `align`; the toolkit's label
// keeps it without a getter)
std::map<const void*, unsigned long> gSeetAlignments;

namespace {

std::vector<UWideString> sSeetNames;   // EE_SCREENS_SEET_OP_NAMES' kept names
std::vector<long> sSeetTechs;          // EE_SCREENS_SEET_OP_TECHS' kept technologies

int32_t SeetWrite(void* outBuffer, int32_t inCapacity, const char* inText)
{
    size_t theLength = strlen(inText);
    memcpy(outBuffer, inText, (std::min)(theLength, (size_t)(inCapacity > 0 ? inCapacity : 0)));
    return (int32_t)theLength;
}

UWideString SeetBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}

// the controls by array (EE_SCREENS_SEET_A_*): the effect screen's and the condition screen's (one slider, no
// effect combo boxes, second edit boxes or list boxes)
UISlider* SeetSlider(EESETSEffectControls* c, int32_t k) { return c->mSliders[k]; }
UISlider* SeetSlider(EESETSConditionControls* c, int32_t) { return c->mSlider; }
UILBString* SeetList(EESETSEffectControls* c, int32_t k) { return c->mListBoxes[k]; }
UILBString* SeetList(EESETSConditionControls*, int32_t) { return NULL; }

UIControl* SeetControl(EESETSEffectControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_LABEL: return c->mLabels[k];
    case EE_SCREENS_SEET_A_CHECK: return c->mCheckBoxes[k];
    case EE_SCREENS_SEET_A_COMBO: return c->mComboBoxes[k];
    case EE_SCREENS_SEET_A_ECOMBO: return c->mEffectComboBoxes[k];
    case EE_SCREENS_SEET_A_SCEN: return c->mScenarioComboBoxes[k];
    case EE_SCREENS_SEET_A_EDIT: return c->mEditBoxes[k];
    case EE_SCREENS_SEET_A_EDIT2: return c->mEditBoxes2[k];
    case EE_SCREENS_SEET_A_TEXT: return c->mTextEditBox;
    case EE_SCREENS_SEET_A_RADIO: return c->mRadioButtons[k];
    case EE_SCREENS_SEET_A_SLIDER: return c->mSliders[k];
    case EE_SCREENS_SEET_A_PLAYER: return c->mPlayers[k];
    case EE_SCREENS_SEET_A_LIST: return c->mListBoxes[k];
    }
    return NULL;
}

UIControl* SeetControl(EESETSConditionControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_LABEL: return c->mLabels[k];
    case EE_SCREENS_SEET_A_CHECK: return c->mCheckBoxes[k];
    case EE_SCREENS_SEET_A_COMBO: return c->mComboBoxes[k];
    case EE_SCREENS_SEET_A_SCEN: return c->mScenarioComboBoxes[k];
    case EE_SCREENS_SEET_A_EDIT: return c->mEditBoxes[k];
    case EE_SCREENS_SEET_A_TEXT: return c->mTextEditBox;
    case EE_SCREENS_SEET_A_RADIO: return c->mRadioButtons[k];
    case EE_SCREENS_SEET_A_SLIDER: return c->mSlider;
    case EE_SCREENS_SEET_A_PLAYER: return c->mPlayers[k];
    }
    return NULL;
}

UIComboBox* SeetCombo(EESETSEffectControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_COMBO: return c->mComboBoxes[k];
    case EE_SCREENS_SEET_A_ECOMBO: return c->mEffectComboBoxes[k];
    case EE_SCREENS_SEET_A_SCEN: return c->mScenarioComboBoxes[k];
    case EE_SCREENS_SEET_A_PLAYER: return c->mPlayers[k];
    }
    return NULL;
}

UIComboBox* SeetCombo(EESETSConditionControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_COMBO: return c->mComboBoxes[k];
    case EE_SCREENS_SEET_A_SCEN: return c->mScenarioComboBoxes[k];
    case EE_SCREENS_SEET_A_PLAYER: return c->mPlayers[k];
    }
    return NULL;
}

UIEditBox* SeetEdit(EESETSEffectControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_EDIT: return c->mEditBoxes[k];
    case EE_SCREENS_SEET_A_EDIT2: return c->mEditBoxes2[k];
    case EE_SCREENS_SEET_A_TEXT: return c->mTextEditBox;
    }
    return NULL;
}

UIEditBox* SeetEdit(EESETSConditionControls* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_EDIT: return c->mEditBoxes[k];
    case EE_SCREENS_SEET_A_TEXT: return c->mTextEditBox;
    }
    return NULL;
}

template <class C>
GEText* SeetLabelText(C* c, int32_t a, int32_t k)
{
    switch (a) {
    case EE_SCREENS_SEET_A_CHECK: return c->mCheckBoxes[k]->GetTextLabel()->GetGEText();
    case EE_SCREENS_SEET_A_RADIO: return c->mRadioButtons[k]->GetTextLabel()->GetGEText();
    case EE_SCREENS_SEET_A_LIST: return SeetList(c, k)->GetGEText();
    }
    return NULL;
}

// The ops both hosts share (the controls, the texts and the world; editor_effects.rs: p0 the controls, p1 the entry
// or a handle, p2 a buffer or an out word; i0 the control array, i1 its index, i2 and i3 the arguments; f the
// floats). outHandled false: not one of them.
template <class C>
int32_t SeetOp(uint32_t inOp, C* c, void* const* p, int32_t* i, float* f, bool& outHandled)
{
    int32_t a = i[0];
    int32_t k = i[1];
    UWideString theText;
    outHandled = true;
    switch (inOp) {
    case EE_SCREENS_SEET_OP_DISPLAY: SeetControl(c, a, k)->SetDisplayMode((unsigned long)i[2]); return 0;
    case EE_SCREENS_SEET_OP_RECT: {
        const GERectangle& r = SeetControl(c, a, k)->GetRectangle();
        f[0] = r.GetLeft(); f[1] = r.GetTop(); f[2] = r.GetRight(); f[3] = r.GetBottom();
        return 0;
    }
    case EE_SCREENS_SEET_OP_SET_RECT:
        SeetControl(c, a, k)->SetRectangle(GERectangle(GE2DPoint(f[0], f[1]), GE2DPoint(f[2], f[3])));
        return 0;
    case EE_SCREENS_SEET_OP_LOCATION: SeetControl(c, a, k)->SetLocation(GE2DPoint(f[0], f[1])); return 0;
    case EE_SCREENS_SEET_OP_ALIGN:
        c->mLabels[k]->SetAlignment((unsigned long)i[2]);
        gSeetAlignments[c->mLabels[k]] = (unsigned long)i[2];
        return 0;
    case EE_SCREENS_SEET_OP_TEXT_STRING:
    case EE_SCREENS_SEET_OP_TEXT:
        if (inOp == EE_SCREENS_SEET_OP_TEXT_STRING)
            gResource->GetString((unsigned long)i[2], theText);
        else
            theText = SeetBytes(p[2], i[2]);
        if (a == EE_SCREENS_SEET_A_LABEL) c->mLabels[k]->SetText(theText);
        else SeetEdit(c, a, k)->SetText(theText);
        return 0;
    case EE_SCREENS_SEET_OP_NUMBER:
        if (a == EE_SCREENS_SEET_A_LABEL) c->mLabels[k]->SetText((long)i[2]);
        else SeetEdit(c, a, k)->SetText((unsigned long)(uint32_t)i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_GET_TEXT:
        if (a == EE_SCREENS_SEET_A_LABEL) theText = c->mLabels[k]->GetText(0);
        else if (UIComboBox* b = SeetCombo(c, a, k)) theText = b->GetTextLabel()->GetText(0);
        else theText = SeetEdit(c, a, k)->GetText(0);
        return SeetWrite(p[2], i[3], theText);
    case EE_SCREENS_SEET_OP_GET_ALL_TEXT:
        SeetEdit(c, a, k)->GetAllText(theText);
        return SeetWrite(p[2], i[3], theText);
    case EE_SCREENS_SEET_OP_LIST_DATA:
        theText = SeetList(c, k)->GetData((unsigned long)i[2]);
        return SeetWrite(p[2], i[3], theText);
    case EE_SCREENS_SEET_OP_TEXT_INT:
        if (a == EE_SCREENS_SEET_A_LABEL) return (int32_t)c->mLabels[k]->GetTextAsInt(0);
        return (int32_t)SeetEdit(c, a, k)->GetTextAsInt(0);
    case EE_SCREENS_SEET_OP_WIDTH:
        if (a == EE_SCREENS_SEET_A_CHECK) c->mCheckBoxes[k]->ChangeWidth((unsigned long)i[2]);
        else if (UIEditBox* e = SeetEdit(c, a, k)) e->ChangeWidth((unsigned long)i[2]);
        else SeetCombo(c, a, k)->ChangeWidth((unsigned long)i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_GET_WIDTH: return (int32_t)SeetEdit(c, a, k)->GetWidth();
    case EE_SCREENS_SEET_OP_CLEAR:
        if (a == EE_SCREENS_SEET_A_LIST) SeetList(c, k)->DeleteAllEntries();
        else SeetCombo(c, a, k)->DeleteAllEntries();
        return 0;
    case EE_SCREENS_SEET_OP_APPEND_STRING:
    case EE_SCREENS_SEET_OP_APPEND:
        if (inOp == EE_SCREENS_SEET_OP_APPEND_STRING)
            gResource->GetString((unsigned long)i[2], theText);
        else
            theText = SeetBytes(p[2], i[2]);
        if (a == EE_SCREENS_SEET_A_LIST) SeetList(c, k)->AppendLine(theText);
        else SeetCombo(c, a, k)->AppendLine(theText);
        return 0;
    case EE_SCREENS_SEET_OP_SELECTION: {
        unsigned long theSelection;
        return SeetCombo(c, a, k)->GetCurrentSelection(theSelection) == kErrorOK ? (int32_t)theSelection : -1;
    }
    case EE_SCREENS_SEET_OP_SET_SELECTION:
        SeetCombo(c, a, k)->SetCurrentSelection((unsigned long)(uint32_t)i[2]);
        return 0;
    case EE_SCREENS_SEET_OP_LINE_COUNT: return (int32_t)SeetCombo(c, a, k)->GetListBox()->GetLineCount();
    case EE_SCREENS_SEET_OP_EVENT_HOLD: {
        UIControl* theControl = SeetControl(c, a, k);
        UIEvent* theEvent = theControl->GetTriggerEvent((unsigned long)i[2]);
        if (theEvent) {
            theEvent->Reference();
            theControl->SetTriggerEvent((unsigned long)i[2], NULL);
        }
        *static_cast<void**>(p[2]) = theEvent;
        return 0;
    }
    case EE_SCREENS_SEET_OP_EVENT_RESTORE: {
        UIEvent* theEvent = static_cast<UIEvent*>(p[1]);
        SeetControl(c, a, k)->SetTriggerEvent((unsigned long)i[2], theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_SEET_OP_EVENT_NULL: SeetControl(c, a, k)->SetTriggerEvent((unsigned long)i[2], NULL); return 0;
    case EE_SCREENS_SEET_OP_EVENT_SET:
        SeetControl(c, a, k)->SetTriggerEvent((unsigned long)i[2], static_cast<UIEvent*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PUSH:
        EESEEPushControl(static_cast<EESEERust*>(static_cast<UIEvent*>(p[1])), SeetControl(c, a, k));
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_PROCESS:
        static_cast<UIEvent*>(p[1])->ProcessEvent(gUIManager->mScenarioEditorForm, SeetControl(c, a, k));
        return 0;
    case EE_SCREENS_SEET_OP_CHECKED:
        if (a == EE_SCREENS_SEET_A_RADIO) return c->mRadioButtons[k]->GetIsChecked() ? 1 : 0;
        return c->mCheckBoxes[k]->GetIsChecked() ? 1 : 0;
    case EE_SCREENS_SEET_OP_SET_CHECKED: c->mCheckBoxes[k]->SetChecked(i[2] != 0); return 0;
    case EE_SCREENS_SEET_OP_CHECK: c->mRadioButtons[k]->Check(); return 0;
    case EE_SCREENS_SEET_OP_TEXT_FONT: {
        GEFont* theFont = i[3] == EE_SCREENS_SEPL_FONT_538 ? gU531938 : gU531934;
        gResource->GetString((unsigned long)i[2], theText);
        if (a == EE_SCREENS_SEET_A_RADIO) c->mRadioButtons[k]->SetText(theText, theFont);
        else c->mCheckBoxes[k]->SetText(theText, theFont);
        return 0;
    }
    case EE_SCREENS_SEET_OP_YELLOW: {
        GEText* theGEText = SeetLabelText(c, a, k);
        theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
        theGEText->SetIsTransparent(true);
        return 0;
    }
    case EE_SCREENS_SEET_OP_SLIDER: return (int32_t)SeetSlider(c, k)->GetCurrentRangeValue();
    case EE_SCREENS_SEET_OP_SET_SLIDER: SeetSlider(c, k)->SetCurrentRangeValue(i[2]); return 0;
    case EE_SCREENS_SEET_OP_RANGE: SeetSlider(c, k)->SetRange(i[2], i[3]); return 0;
    case EE_SCREENS_SEET_OP_PAGE_SIZE: SeetSlider(c, k)->SetPageSize(i[2]); return 0;
    case EE_SCREENS_SEET_OP_FIRE_RANGE: SeetSlider(c, k)->FireTriggerEvent(kControlTriggerRangeChanged); return 0;
    case EE_SCREENS_SEET_OP_STRING_TABLE: SeetControl(c, a, k)->SetStringTableID((unsigned long)i[2]); return 0;
    case EE_SCREENS_SEET_OP_FONT_530:
        if (UIEditBox* e = SeetEdit(c, a, k)) e->GetGEText()->SetFont(gU531930);
        else {
            SeetCombo(c, a, k)->GetGEText()->SetFont(gU531930);
            if (i[2] != 0)
                SeetCombo(c, a, k)->GetListBox()->GetGEText()->SetFont(gU531930);
        }
        return 0;
    case EE_SCREENS_SEET_OP_PLAYER: return (int32_t)c->mPlayers[k]->GetSelectedPlayer();
    case EE_SCREENS_SEET_OP_SET_PLAYER: c->mPlayers[k]->SetSelectedPlayer((unsigned long)(uint32_t)i[2]); return 0;
    case EE_SCREENS_SEET_OP_PLAYER_FLAGS: c->mPlayers[k]->SetFlags((unsigned long)i[2]); return 0;
    case EE_SCREENS_SEET_OP_SCEN_MODE: c->mScenarioComboBoxes[k]->mMode = (unsigned long)i[2]; return 0;
    case EE_SCREENS_SEET_OP_FILL_NAMES: {
        std::vector<UWideString> theNames;
        UIComboBox* theCombo = SeetCombo(c, a, k);
        gWorld.mScenarioManager->GetNames((unsigned long)i[2], theNames);
        if (i[3] != 0)
            theCombo->AppendStringArray(theNames);
        else
            for (unsigned long n = 0; n < theNames.size(); n++)
                theCombo->AppendLine(theNames[n]);
        return 0;
    }
    case EE_SCREENS_SEET_OP_FILL_AREA_OBJECT:
        FillAreaObjectComboBox(c->mLabels[i[2]], c->mScenarioComboBoxes[k], i[3] != 0);
        return 0;
    case EE_SCREENS_SEET_OP_FILL_SCENARIO:
        FillScenarioComboBox(c->mScenarioComboBoxes[k], c->mLabels[i[2]], i[3] != 0);
        return 0;
    case EE_SCREENS_SEET_OP_FILL_PLACE:
        FillPlaceComboBox(i[3] != 0, c->mLabels[i[2]], SeetCombo(c, EE_SCREENS_SEET_A_COMBO, k));
        return 0;
    case EE_SCREENS_SEET_OP_LIST_OPACITY:
        SeetList(c, k)->SetBgOpacity(f[0]);
        SeetList(c, k)->SetScrollBarBgOpacity(f[0]);
        return 0;
    case EE_SCREENS_SEET_OP_LIST_INSERT:
        SeetList(c, k)->InsertLine(SeetBytes(p[2], i[2]), (unsigned long)i[3]);
        return 0;
    case EE_SCREENS_SEET_OP_LIST_DELETE: SeetList(c, k)->DeleteLine((unsigned long)i[2]); return 0;
    case EE_SCREENS_SEET_OP_FORM_DISPLAY:
        c->mTextEditBox->GetForm()->GetControlById((unsigned long)i[2])->SetDisplayMode((unsigned long)i[3]);
        return 0;
    case EE_SCREENS_SEET_OP_SLIDER_MODIFIER:
        static_cast<UIESliderChange*>(SeetSlider(c, k)->GetTriggerEvent(kControlTriggerRangeChanged))->SetModifier(f[0]);
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_NEW: {
        UIEvent* theEvent = NULL;
        switch (i[0]) {
        case EE_SCREENS_SEET_EV_SCREEN: theEvent = EECreateScreenEvent((uint32_t)i[1]); break;
        case EE_SCREENS_SEET_EV_SEE: theEvent = new EESEERust((uint32_t)i[1]); break;
        case EE_SCREENS_SEET_EV_SLIDER: theEvent = new UIESliderChange(c->mLabels[i[1]], f[0], (long)i[2]); break;
        case EE_SCREENS_SEET_EV_SLIDER_FORMAT:
            theEvent = EESEENewSliderChange(c->mLabels[i[1]], i[2] != 0 ? "%#2.1f" : "%2.0f", f[0], f[1]);
            break;
        case EE_SCREENS_SEET_EV_USE_OBJECT_AREA:
            theEvent = EESEENewUseObjectArea(c->mLabels[i[1]], c->mScenarioComboBoxes[i[2]], c->mRadioButtons[i[3]]);
            break;
        case EE_SCREENS_SEET_EV_OBJECTS: theEvent = EESEENewObjects((uint32_t)i[1], p[1], c); break;
        }
        *static_cast<void**>(p[2]) = theEvent;
        return 0;
    }
    case EE_SCREENS_SEET_OP_EVENT_STRING:
        if (i[3] != 0)
            gResource->GetString((unsigned long)i[2], theText);
        else
            theText = SeetBytes(p[2], i[2]);
        static_cast<UIESliderChange*>(static_cast<UIEvent*>(p[1]))->AddString(f[0], theText);
        return 0;
    case EE_SCREENS_SEET_OP_EVENT_RELEASE: static_cast<UIEvent*>(p[1])->Dereference(); return 0;
    case EE_SCREENS_SEET_OP_STRING:
        gResource->GetString((unsigned long)i[2], theText);
        return SeetWrite(p[2], k, theText);
    case EE_SCREENS_SEET_OP_FORMAT_NUMBER: {
        UWideString theFormat;
        gResource->GetString((unsigned long)i[2], theFormat);
        sprintf(theText, theFormat, (unsigned long)(uint32_t)i[3]);
        return SeetWrite(p[2], k, theText);
    }
    case EE_SCREENS_SEET_OP_FORMAT_FLOAT:
        sprintf(theText, static_cast<const char*>(p[1]), (double)f[0]);
        return SeetWrite(p[2], k, theText);
    case EE_SCREENS_SEET_OP_SET_NAME_BOX:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetNameEditBox()->SetText(SeetBytes(p[2], i[2]));
        return 0;
    case EE_SCREENS_SEET_OP_NAME_BOX:
        return SeetWrite(p[2], k, gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName());
    case EE_SCREENS_SEET_OP_NAMES:
        sSeetNames.clear();
        gWorld.mScenarioManager->GetNames((unsigned long)i[2], sSeetNames);
        return (int32_t)sSeetNames.size();
    case EE_SCREENS_SEET_OP_NAME: return SeetWrite(p[2], k, sSeetNames[(size_t)i[2]]);
    case EE_SCREENS_SEET_OP_OBJECT_NAME: {
        EESMObject* theObject = gWorld.mScenarioManager->GetScenarioObject((unsigned long)(uint32_t)i[2]);
        return theObject != NULL ? SeetWrite(p[2], k, theObject->mName) : -1;
    }
    case EE_SCREENS_SEET_OP_TRIGGER_NAME: {
        EESMTrigger* theTrigger = gWorld.mScenarioManager->GetTrigger((unsigned long)(uint32_t)i[2]);
        return theTrigger != NULL ? SeetWrite(p[2], k, theTrigger->mName) : -1;
    }
    case EE_SCREENS_SEET_OP_CALAMITY_COUNT: return (int32_t)EECalamityManager::GetInstance()->GetCalamityClassCount();
    case EE_SCREENS_SEET_OP_CALAMITY: {
        EECalamityClass* theClass = EECalamityManager::GetInstance()->GetCalamityClass((unsigned long)i[2]);
        if (!theClass)
            return 0;
        i[2] = (int32_t)theClass->mID;
        i[3] = (int32_t)theClass->mNameID;
        return 1;
    }
    case EE_SCREENS_SEET_OP_GRAPHIC_COUNT: return (int32_t)EEGraphicEffect::GetCount();
    case EE_SCREENS_SEET_OP_GRAPHIC: {
        EEGraphicEffect* theGraphic = EEGraphicEffect::Get((unsigned long)i[2]);
        if (!theGraphic || theGraphic->mIsScenarioListed != true)
            return 0;
        i[2] = (int32_t)theGraphic->mID;
        i[3] = (int32_t)theGraphic->mNameStringId;
        return 1;
    }
    case EE_SCREENS_SEET_OP_BEHAVIOR_COUNT: return (int32_t)(sizeof(gCPBehaviors) / sizeof(EECPBehavior));
    case EE_SCREENS_SEET_OP_BEHAVIOR: {
        const EECPBehavior& b = gCPBehaviors[i[2]];
        switch (i[3]) {
        case EE_SCREENS_SEET_B_SETTABLE: return b.mIsSettable == true ? 1 : 0;
        case EE_SCREENS_SEET_B_INT1_MIN: return b.mIntValue1Min;
        case EE_SCREENS_SEET_B_INT1_MAX: return b.mIntValue1Max;
        case EE_SCREENS_SEET_B_INT2_MIN: return b.mIntValue2Min;
        case EE_SCREENS_SEET_B_INT2_MAX: return b.mIntValue2Max;
        case EE_SCREENS_SEET_B_FLOAT_MIN: return b.mFloatSliderMin;
        case EE_SCREENS_SEET_B_FLOAT_MAX: return b.mFloatSliderMax;
        default: return b.mFloatSliderScale;
        }
    }
    case EE_SCREENS_SEET_OP_UNIT_TYPE_COUNT: return (int32_t)EEUnitClass::GetUnitTypes().size();
    case EE_SCREENS_SEET_OP_UNIT_TYPE: {
        EEUnitClass* theClass = gUnitClassTable[EEUnitClass::GetUnitTypes()[(size_t)i[2]]];
        if (theClass->mIsScenarioSelectable != true)
            return -1;
        i[2] = (int32_t)theClass->mUnitType;
        if ((theClass->mDomains & kMovementAir) == kMovementAir)
            return EE_SCREENS_SEET_D_AIR;
        if ((theClass->mDomains & kMovementSea) == kMovementSea)
            return EE_SCREENS_SEET_D_SEA;
        return EE_SCREENS_SEET_D_LAND;
    }
    case EE_SCREENS_SEET_OP_UNIT_NAME: return (int32_t)gUnitClassTable[i[2]]->mNameStringID;
    case EE_SCREENS_SEET_OP_TECHS: {
        // the tree's nodes and the epochs, those shown in the editor (i2: and making no unit, the condition
        // types'), sorted by name (the originals' first Show)
        sSeetTechs.clear();
        gTechTree.GetNodeIDs(sSeetTechs);
        for (unsigned long theEpoch = 0; theEpoch < (unsigned long)EEEpochCount(); theEpoch++)
            sSeetTechs.push_back((long)theEpoch);
        unsigned long theIndex = 0;
        while (theIndex < sSeetTechs.size()) {
            RTSTechTreeNodeData* theData = gTechTree.Get(sSeetTechs[theIndex])->mData;
            if (theData->mShowInEditor == true && (i[2] == 0 || theData->mMakesUnit == false))
                theIndex++;
            else
                sSeetTechs.erase(sSeetTechs.begin() + theIndex);
        }
        std::sort(sSeetTechs.begin(), sSeetTechs.end(), SeetTechNameLess());
        return (int32_t)sSeetTechs.size();
    }
    case EE_SCREENS_SEET_OP_TECH: return (int32_t)sSeetTechs[(size_t)i[2]];
    case EE_SCREENS_SEET_OP_TECH_LINE: {
        EETechTreeNode* theNode = gTechTree.Get(i[2]);
        theText = SeetTechName(i[2]);
        if (theNode && theText == "FONTS")
            sprintf(theText, "BAD Node %ld - (name not available in release)", theNode->mData->mID);
        return SeetWrite(p[2], k, theText);
    }
    case EE_SCREENS_SEET_OP_TECH_NAME: return SeetWrite(p[2], k, SeetTechName(i[2]));
    case EE_SCREENS_SEET_OP_TSCREEN: {
        EESETriggerScreen* theScreen = gUIManager->mScenarioEditorForm->GetTriggerScreen();
        if (i[2] == EE_SCREENS_SEET_TS_HIDE_TEXT) theScreen->HideText();
        else if (i[2] == EE_SCREENS_SEET_TS_SHOW_ALPHA_SORT) theScreen->ShowAlphaSort();
        else theScreen->ShowText();
        return 0;
    }
    case EE_SCREENS_SEET_OP_TEXT_BOX:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
        return SeetWrite(p[2], k, theText);
    case EE_SCREENS_SEET_OP_SET_TEXT_BOX:
        gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->SetText(SeetBytes(p[2], i[2]));
        return 0;
    case EE_SCREENS_SEET_OP_PLAYER_COUNT:
        return (int32_t)static_cast<EEUIFScenarioEditor*>(c->mTextEditBox->GetForm())->GetPlayerScreen()->GetPlayerCount();
    case EE_SCREENS_SEET_OP_SET_PLAYER_COUNT: c->mPlayers[k]->mPlayerCount = (unsigned long)i[2]; return 0;
    case EE_SCREENS_SEET_OP_HANDLE_SELECTION: {
        unsigned long theSelection;
        UIComboBox* theCombo = static_cast<UIComboBox*>(static_cast<UIControl*>(p[1]));
        return theCombo->GetCurrentSelection(theSelection) == kErrorOK ? (int32_t)theSelection : -1;
    }
    }
    outHandled = false;
    return 0;
}

}   // namespace

// the scenario's list inList's operation inOp (EE_SCREENS_SEOB_SM_*) on the entry inEntry (the screens' list buttons)
void SeetEntryList(unsigned long inList, int32_t inOp, int32_t inEntry)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    unsigned long theEntry = (unsigned long)(uint32_t)inEntry;
    switch (inOp) {
    case EE_SCREENS_SEOB_SM_SORT: theManager->SortList(inList); break;
    case EE_SCREENS_SEOB_SM_DOWN: theManager->MoveEntry(inList, theEntry, false); break;
    case EE_SCREENS_SEOB_SM_UP: theManager->MoveEntry(inList, theEntry, true); break;
    case EE_SCREENS_SEOB_SM_DELETE: theManager->DeleteEntry(inList, theEntry); break;
    case EE_SCREENS_SEOB_SM_DUPLICATE: theManager->DuplicateEntry(inList, theEntry); break;
    default: theManager->NewEntry(inList); break;
    }
}

// The effect screen's and types' host (editor_subscreens.rs, editor_effects.rs): the shared ops over the effect
// screen's controls; the controls made and deleted, the effect, its texts and objects, the effects' list
extern "C" int32_t ee_screenscpp_seet(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESETSEffectControls* c = static_cast<EESETSEffectControls*>(p[0]);
    EESMEffect* theEffect = static_cast<EESMEffect*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEET_OP_CONTROLS_NEW: {
        unsigned long theIndex = (unsigned long)(uint32_t)i[2];
        *static_cast<void**>(p[2]) = new EESETSEffectControls(gSeplScreenFont, static_cast<UIForm*>(p[1]), theIndex);
        i[2] = (int32_t)theIndex;
        return 0;
    }
    case EE_SCREENS_SEET_OP_CONTROLS_FREE: delete c; return 0;
    case EE_SCREENS_SEET_OP_ENTRY: {
        EESMEffect* theEntry = gWorld.mScenarioManager->GetEffect((unsigned long)(uint32_t)i[2]);
        static_cast<void**>(p[2])[0] = theEntry;
        static_cast<void**>(p[2])[1] = theEntry != NULL ? static_cast<EeScenarioEffectFields*>(theEntry) : NULL;
        return 0;
    }
    case EE_SCREENS_SEET_OP_LIST: SeetEntryList(EE_SCENARIO_LIST_EFFECTS, i[2], i[3]); return 0;
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_EFFECTS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_EFFECT_TEXT:
    case EE_SCREENS_SEET_OP_SET_EFFECT_TEXT: {
        UWideString* theField = i[2] == EE_SCREENS_SEET_S_NEW_NAME      ? &theEffect->mNewName
                                : i[2] == EE_SCREENS_SEET_S_MESSAGE     ? &theEffect->mMessage
                                : i[2] == EE_SCREENS_SEET_S_FILE_NAME   ? &theEffect->mFileName
                                : i[2] == EE_SCREENS_SEET_S_DESCRIPTION ? &theEffect->mDescription
                                : i[2] == EE_SCREENS_SEET_S_TEXT        ? &theEffect->mText
                                                                        : &theEffect->mName;
        if (inOp == EE_SCREENS_SEET_OP_EFFECT_TEXT)
            return SeetWrite(p[2], i[1], *theField);
        *theField = SeetBytes(p[2], i[3]);
        return 0;
    }
    case EE_SCREENS_SEET_OP_OBJECT_COUNT: return (int32_t)theEffect->mObjects.size();
    case EE_SCREENS_SEET_OP_OBJECT: return (int32_t)theEffect->mObjects[(size_t)i[2]];
    case EE_SCREENS_SEET_OP_SET_OBJECTS: {
        const uint32_t* theObjects = static_cast<const uint32_t*>(p[2]);
        theEffect->mObjects.assign(theObjects, theObjects + (i[2] > 0 ? i[2] : 0));
        return 0;
    }
    }
    bool theHandled;
    return SeetOp(inOp, c, p, i, f, theHandled);
}

// The condition screen's and types' host (editor_subscreens.rs, editor_conditions.rs): the shared ops over the
// condition screen's controls; the controls made and deleted, the condition, its texts, the conditions' list
extern "C" int32_t ee_screenscpp_sect(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESETSConditionControls* c = static_cast<EESETSConditionControls*>(p[0]);
    EESMCondition* theCondition = static_cast<EESMCondition*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEET_OP_CONTROLS_NEW: {
        unsigned long theIndex = (unsigned long)(uint32_t)i[2];
        *static_cast<void**>(p[2]) = new EESETSConditionControls(gSeplScreenFont, static_cast<UIForm*>(p[1]), theIndex);
        i[2] = (int32_t)theIndex;
        return 0;
    }
    case EE_SCREENS_SEET_OP_CONTROLS_FREE: delete c; return 0;
    case EE_SCREENS_SEET_OP_ENTRY: {
        EESMCondition* theEntry = gWorld.mScenarioManager->GetCondition((unsigned long)(uint32_t)i[2]);
        static_cast<void**>(p[2])[0] = theEntry;
        static_cast<void**>(p[2])[1] = theEntry != NULL ? static_cast<EeScenarioConditionFields*>(theEntry) : NULL;
        return 0;
    }
    case EE_SCREENS_SEET_OP_LIST: SeetEntryList(EE_SCENARIO_LIST_CONDITIONS, i[2], i[3]); return 0;
    case EE_SCREENS_SEET_OP_GET_NAMES:
        gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_CONDITIONS, *static_cast<std::vector<UWideString>*>(p[1]));
        return 0;
    case EE_SCREENS_SEET_OP_EFFECT_TEXT:
    case EE_SCREENS_SEET_OP_SET_EFFECT_TEXT: {
        UWideString* theField = i[2] == EE_SCREENS_SEET_S_CHAT   ? &theCondition->mChatText
                                : i[2] == EE_SCREENS_SEET_S_TEXT ? &theCondition->mText
                                                                 : &theCondition->mName;
        if (inOp == EE_SCREENS_SEET_OP_EFFECT_TEXT)
            return SeetWrite(p[2], i[1], *theField);
        *theField = SeetBytes(p[2], i[3]);
        return 0;
    }
    }
    bool theHandled;
    return SeetOp(inOp, c, p, i, f, theHandled);
}
