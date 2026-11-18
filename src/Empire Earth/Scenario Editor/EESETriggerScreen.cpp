// C:\EEX\Empire Earth\Scenario Editor\EESETriggerScreen.cpp (file name inferred: no __FILE__; RTTI class
// EESETriggerScreen). The editor's trigger tab: 0x17b0b7..0x17ccf3 (ctor, the ??_G pair, dtor, the event handlers and
// slots in source order, the sub-screen factory, then the COMDATs this unit is the first user of: the list and radio
// button ??_G, EEScenarioManager::GetCondition, the events' ??_G, the hide-UI event's SetRectangles). After the "?" unit
// 0x17acf6..0x17b0b7 (a globals chain), before EESETSArea.cpp (0x17ccf3). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EESETSCondition.h"
#include "ee/EESETSEffect.h"
#include "ee/EESETSObject.h"
#include "ee/EESETSArea.h"
#include "ee/EESETSTrigger.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "ee/EESEARust.h"
#include "rust/game/screens.h"
extern char gEmptyString[]; // 0x4e5b98: the shared empty string
extern GEFont* gSeplScreenFont;   // the screen's font while the tab makes its controls (EESERBUnitTypes.cpp)

bool gB4C3220 = true;

// Builds the trigger tab (ee_screens editor_tscreen.rs: the entry list, the five sub-tab buttons, new/duplicate/
// delete, the sort and priority buttons, the debug-id check box, the name and description boxes), then the five
// sub-screens, then disables their groups
EESETriggerScreen::EESETriggerScreen(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    this->mR18 = GERectangle(0.0f, 448.0f, 1023.0f, 606.0f);
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_sets_new(this, inForm, &theIndex);
    ioIndex = theIndex;
    for (unsigned long i = 0; i < 5; i++)
        this->mScreens[i] = this->CreateScreen(i, inFont, inForm, ioIndex);
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_FINISH, 0, 0.0f, 0.0f, NULL);
}

// Destroys the trigger tab's list, tab buttons, sub-screens, name and description controls and buttons (the
// original's order: the sub-screens between the tabs and the name box)
EESETriggerScreen::~EESETriggerScreen()
{
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_DESTROY_FIRST, 0, 0.0f, 0.0f, NULL);
    for (unsigned long i = 0; i < 5; i++)
        delete this->mScreens[i];
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_DESTROY_REST, 0, 0.0f, 0.0f, NULL);
    ee_screens_sets_free(this->mRust);
}

// The members (editor_tscreen.rs): 0x57c057 AlphaSort, 0x57c0ad SelectEntry, 0x57c130 Leave, 0x57c1d9 Update,
// 0x57c2ae DecPriority, 0x57c31b DeleteEntry, 0x57c399 Draw, 0x57c411 DuplicateEntry, 0x57c467 ShowTab, 0x57c599
// StoreEntry, 0x57c5b3 Disable, 0x57c5d9 HideText, 0x582d69 ShowText (ICF-folded), 0x57c5f2 HideAlphaSort, 0x57c5fd
// ShowSelection, 0x57c6e0 IncPriority, 0x57c743 HideGroups, 0x57c78b Load (ShowSelection), 0x57c790 NewEntry,
// 0x57c7d0 Enter, 0x57c92e SelectName, 0x57c99f FillList, 0x57ca30 Enable, 0x57ca56 ShowAlphaSort, 0x57ca61 Store
// (StoreEntry), 0x57ca66 UpdateKeys, 0x57ca79 MouseMove, 0x57ca92 RefreshScreen
#define EE_SETS_CALL(inWhat) ee_screens_sets_call(this->mRust, (inWhat), 0, 0.0f, 0.0f, NULL)
void EESETriggerScreen::AlphaSort() { EE_SETS_CALL(EE_SCREENS_SETS_ALPHA_SORT); }
void EESETriggerScreen::SelectEntry() { EE_SETS_CALL(EE_SCREENS_SETS_SELECT_ENTRY); }
void EESETriggerScreen::Leave() { EE_SETS_CALL(EE_SCREENS_SETS_LEAVE); }
void EESETriggerScreen::Update() { EE_SETS_CALL(EE_SCREENS_SETS_UPDATE); }
void EESETriggerScreen::DecPriority() { EE_SETS_CALL(EE_SCREENS_SETS_DEC_PRIORITY); }
void EESETriggerScreen::DeleteEntry() { EE_SETS_CALL(EE_SCREENS_SETS_DELETE_ENTRY); }
void EESETriggerScreen::Draw(const GERectangle& inRectangle)
{
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_DRAW, 0, 0.0f, 0.0f, const_cast<GERectangle*>(&inRectangle));
}
void EESETriggerScreen::DuplicateEntry() { EE_SETS_CALL(EE_SCREENS_SETS_DUPLICATE_ENTRY); }
void EESETriggerScreen::ShowTab(unsigned long inControlID)
{
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_SHOW_TAB, (uint32_t)inControlID, 0.0f, 0.0f, NULL);
}
void EESETriggerScreen::Disable() { EE_SETS_CALL(EE_SCREENS_SETS_DISABLE); }
void EESETriggerScreen::HideText() { EE_SETS_CALL(EE_SCREENS_SETS_HIDE_TEXT); }
void EESETriggerScreen::ShowText() { EE_SETS_CALL(EE_SCREENS_SETS_SHOW_TEXT); }
void EESETriggerScreen::HideAlphaSort() { EE_SETS_CALL(EE_SCREENS_SETS_HIDE_ALPHA_SORT); }
void EESETriggerScreen::IncPriority() { EE_SETS_CALL(EE_SCREENS_SETS_INC_PRIORITY); }
void EESETriggerScreen::Load() { EE_SETS_CALL(EE_SCREENS_SETS_SHOW_SELECTION); }
void EESETriggerScreen::NewEntry() { EE_SETS_CALL(EE_SCREENS_SETS_NEW_ENTRY); }
void EESETriggerScreen::Enter() { EE_SETS_CALL(EE_SCREENS_SETS_ENTER); }
void EESETriggerScreen::SelectName() { EE_SETS_CALL(EE_SCREENS_SETS_SELECT_NAME); }
void EESETriggerScreen::FillList() { EE_SETS_CALL(EE_SCREENS_SETS_FILL_LIST); }
void EESETriggerScreen::Enable() { EE_SETS_CALL(EE_SCREENS_SETS_ENABLE); }
void EESETriggerScreen::ShowAlphaSort() { EE_SETS_CALL(EE_SCREENS_SETS_SHOW_ALPHA_SORT); }
void EESETriggerScreen::Store() { EE_SETS_CALL(EE_SCREENS_SETS_STORE_ENTRY); }
void EESETriggerScreen::UpdateKeys() { EE_SETS_CALL(EE_SCREENS_SETS_UPDATE_KEYS); }
void EESETriggerScreen::MouseMove(const GE2DPoint& inLocation)
{
    ee_screens_sets_call(this->mRust, EE_SCREENS_SETS_MOUSE_MOVE, 0, inLocation.GetX(), inLocation.GetY(), NULL);
}
void EESETriggerScreen::RefreshScreen() { EE_SETS_CALL(EE_SCREENS_SETS_REFRESH_SCREEN); }
#undef EE_SETS_CALL

UIEditBox* EESETriggerScreen::GetNameEditBox() const
{
    return static_cast<UIEditBox*>(ee_screens_sets_control(this->mRust, EE_SCREENS_SETS_NAME_EDIT, 0));
}
UIEditBox* EESETriggerScreen::GetTextEditBox() const
{
    return static_cast<UIEditBox*>(ee_screens_sets_control(this->mRust, EE_SCREENS_SETS_TEXT_EDIT, 0));
}
UILBString* EESETriggerScreen::GetList() const
{
    return static_cast<UILBString*>(ee_screens_sets_control(this->mRust, EE_SCREENS_SETS_LIST, 0));
}
UIRadioButton* EESETriggerScreen::GetTab(long inScreen) const
{
    return static_cast<UIRadioButton*>(ee_screens_sets_control(this->mRust, EE_SCREENS_SETS_TAB, (uint32_t)inScreen));
}
long EESETriggerScreen::GetCurrentScreen() const
{
    return (long)(intptr_t)ee_screens_sets_control(this->mRust, EE_SCREENS_SETS_CURRENT_SCREEN, 0);
}

// Creates the trigger, condition, effect, object or area sub-screen for a sub-tab index
EESETSBaseObject* EESETriggerScreen::CreateScreen(unsigned long inType, GEFont* inFont, UIForm* inForm,
                                                  unsigned long& ioIndex)
{
    EESETSBaseObject* theScreen = NULL;
    switch (inType) {
    case kSETriggerScreenTrigger:
        theScreen = new EESETSTrigger(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenCondition:
        theScreen = new EESETSCondition(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenEffect:
        theScreen = new EESETSEffect(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenObject:
        theScreen = new EESETSObject(inFont, inForm, ioIndex);
        break;
    case kSETriggerScreenArea:
        theScreen = new EESETSArea(inFont, inForm, ioIndex);
        break;
    }
    return theScreen;
}


// ---- the trigger tab's host (editor_tscreen.rs; EE_SCREENS_SETS_OP_*): its tab and bitmap buttons, the sub-screens,
// the views, the list

static_assert(EE_SCREENS_SETS_IDS_NEW == IDS_TRIGGER_EDITOR_NEW && EE_SCREENS_SETS_IDS_NEW + 1 == IDS_DUPLICATE &&
                  EE_SCREENS_SETS_IDS_NEW + 2 == IDS_TRIGGER_EDITOR_DELETE &&
                  EE_SCREENS_SETS_IDS_NEW + 3 == IDS_TRIGGER_EDITOR_NAME &&
                  EE_SCREENS_SETS_IDS_DESCRIPTION == IDS_DESCRIPTION_76A8,
              "the strings");
static_assert(EE_SCREENS_SETS_TAB_FIRST == kSETriggerTabTrigger && kSETriggerTabArea == kSETriggerTabTrigger + 4 &&
                  EE_SCREENS_SETS_GROUPS[0] == kEntityGroupTrigger && EE_SCREENS_SETS_GROUPS[1] == kEntityGroupCondition &&
                  EE_SCREENS_SETS_GROUPS[2] == kEntityGroupEffect && EE_SCREENS_SETS_GROUPS[3] == kEntityGroupObject &&
                  EE_SCREENS_SETS_GROUPS[4] == kEntityGroupArea,
              "the sub-tabs");

namespace {

void SetsYellow(GEText* inText)
{
    inText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    inText->SetIsTransparent(true);
}

void SetsEvent(UIControl* inControl, unsigned long inTrigger, uint32_t inCode)
{
    UIEvent* theEvent = EECreateScreenEvent(inCode);
    inControl->SetTriggerEvent(inTrigger, theEvent);
    theEvent->Dereference();
}

void SetsScreenCall(EESETSBaseObject* inScreen, int32_t inWhat, const float* f)
{
    switch (inWhat) {
    case EE_SCREENS_SETS_SCREEN_SORT: inScreen->Sort(); break;
    case EE_SCREENS_SETS_SCREEN_MOVE_DOWN: inScreen->MoveDown(); break;
    case EE_SCREENS_SETS_SCREEN_DELETE: inScreen->Delete(); break;
    case EE_SCREENS_SETS_SCREEN_DEACTIVATE: inScreen->Deactivate(); break;
    case EE_SCREENS_SETS_SCREEN_DUPLICATE: inScreen->Duplicate(); break;
    case EE_SCREENS_SETS_SCREEN_ACTIVATE: inScreen->Activate(); break;
    case EE_SCREENS_SETS_SCREEN_STORE: inScreen->Store(); break;
    case EE_SCREENS_SETS_SCREEN_HIDE: inScreen->Hide(); break;
    case EE_SCREENS_SETS_SCREEN_LOAD: inScreen->Load(); break;
    case EE_SCREENS_SETS_SCREEN_MOVE_UP: inScreen->MoveUp(); break;
    case EE_SCREENS_SETS_SCREEN_NEW: inScreen->New(); break;
    case EE_SCREENS_SETS_SCREEN_SHOW: inScreen->Show(); break;
    case EE_SCREENS_SETS_SCREEN_UPDATE: inScreen->Update(); break;
    case EE_SCREENS_SETS_SCREEN_UPDATE_KEYS: inScreen->UpdateKeys(); break;
    case EE_SCREENS_SETS_SCREEN_MOUSE_MOVE: inScreen->MouseMove(GE2DPoint(f[0], f[1])); break;
    case EE_SCREENS_SETS_SCREEN_REFRESH: inScreen->RefreshControls(); break;
    }
}

// Leave's and Enter's views (the hide-UI box, the game view, the camera flags, the mini map, the redraw)
void SetsView(EEUIFScenarioEditor* inEditor, bool inEnter)
{
    UIControl* theHideUI = inEditor->GetControlById(kScenarioEditorHideUICheckBox);
    if (!inEnter) {
        GERectangle theRectangle(0.0f, 0.0f, 1024.0f, 606.0f);
        theHideUI->SetDisplayMode(kDisplayModeHidden);
        inEditor->SetGameView(theRectangle, false);
    } else {
        GERectangle theRectangle(0.0f, 0.0f, 1024.0f, 447.0f);
        GERectangle theEdges(0.0f, 0.0f, 1024.0f, 606.0f);
        EESetGameViewRectangle(theRectangle);
        theHideUI->SetDisplayMode(kDisplayModeShown);
        // the hide-UI event's two views (the original's inline SetRectangles; the event is an EESEERust since it moved)
        EESEERust* theEvent = static_cast<EESEERust*>(theHideUI->GetTriggerEvent(kControlTriggerChecked));
        const float theUnchecked[4] = {theEdges.GetLeft(), theEdges.GetTop(), theEdges.GetRight(), theEdges.GetBottom()};
        const float theChecked[4] = {theRectangle.GetLeft(), theRectangle.GetTop(), theRectangle.GetRight(),
                                     theRectangle.GetBottom()};
        ee_screens_see_hide_ui_views(theEvent->mEvent, theUnchecked, theChecked);
        inEditor->SetGameView(theRectangle, false);
        static_cast<UICheckBox*>(inEditor->GetControlById(kScenarioEditorHideUICheckBox))->SetChecked(true);
        inEditor->GetEntityGroupById(14)->Enable();
    }
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (gMiniMap != NULL)
        gMiniMap->RefreshView();
    gUIManager->RedrawAll();
    gUIManager->GetCurrentForm()->SetAllControlsDirty();
}

}   // namespace

// The ops (editor_tscreen.rs; p: three handles, i: four integers, f: four floats)
extern "C" int32_t ee_screenscpp_sets(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EESETriggerScreen* theTab = static_cast<EESETriggerScreen*>(p[0]);
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    UILBString* theList = static_cast<UILBString*>(p[0]);
    void** theOut = static_cast<void**>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SETS_OP_TAB: {
        UWideString theText;
        unsigned long k = (unsigned long)i[1];
        gResource->GetString(IDS_TRIGGER_SCREEN_TAB_FIRST + k, theText);
        float theX = (float)k * 60.0f + 4.0f;
        UIRadioButton* t = new UIRadioButton(2000 + k, theText, gU531930, GERectangle(theX, 458.0f, theX + 57.0f, 477.0f),
                                             theForm, (unsigned long)i[0], 2, true, k == 0, false);
        t->SetUpCheckedBitmap(NULL);
        t->SetDownCheckedBitmap(NULL);
        t->SetDownUncheckedBitmap(NULL);
        t->SetUpUncheckedBitmap(NULL);
        t->GetTextLabel()->SetRectangle(GERectangle(theX + 2.0f, 460.0f, theX + 54.0f, 474.0f));
        t->SetBgOpacity(0.4f);
        SetsEvent(t, kControlTriggerChecked, EE_SCREENS_SE_SHOW_TAB);
        SetsYellow(t->GetTextLabel()->GetGEText());
        theForm->AddControl(t, 14);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SETS_OP_IMAGE_BUTTON: {
        UIManager* theManager = theForm->GetManager();
        GEBitmap* theUp = i[1] == 0 ? theManager->GetAlphaSortUpBitmap()
                        : i[1] == 1 ? theManager->GetSBBitmapUpNormal() : theManager->GetSBBitmapDownNormal();
        GEBitmap* theDown = i[1] == 0 ? theManager->GetAlphaSortDownBitmap()
                          : i[1] == 1 ? theManager->GetSBBitmapUpPressed() : theManager->GetSBBitmapDownPressed();
        UIButton* t = new UIButton(i[1] == 0 ? 0x7dd : 0x7d8 + i[1] - 1, theUp, NULL, theDown,
                                   GERectangle(GE2DPoint(305.0f, f[0]), theUp->GetWidth(), theUp->GetHeight()), theForm,
                                   (unsigned long)i[0], 0);
        SetsEvent(t, kControlTriggerButtonUp, (uint32_t)i[2]);
        theForm->AddControl(t, 14);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SETS_OP_SHOW_IDS: {
        UIManager* theManager = theForm->GetManager();
        EESETSShowIDsCheckBox* t = new EESETSShowIDsCheckBox(0x7de, gEmptyString, gU531930, GE2DPoint(305.0f, f[0]),
                                                             theManager->GetCheckOn()->GetWidth(),
                                                             theManager->GetCheckOn()->GetHeight(), theForm,
                                                             (unsigned long)i[0]);
        SetsEvent(t, kControlTriggerButtonUp, EE_SCREENS_SE_SHOW_DEBUG_IDS);
        theForm->AddControl(t, 14);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SETS_OP_FONT_HEIGHT: return (int32_t)gU531930->GetHeight();
    case EE_SCREENS_SETS_OP_SCREEN: SetsScreenCall(theTab->mScreens[i[0]], i[1], f); return 0;
    case EE_SCREENS_SETS_OP_SELECTION:
        i[1] = theTab->mScreens[i[0]]->mHasSelection;
        return (int32_t)theTab->mScreens[i[0]]->mSelection;
    case EE_SCREENS_SETS_OP_SET_SELECTION:
        if (i[1] >= 0)
            theTab->mScreens[i[0]]->mSelection = (unsigned long)i[1];
        if (i[2] >= 0)
            theTab->mScreens[i[0]]->mHasSelection = i[2] != 0;
        return 0;
    case EE_SCREENS_SETS_OP_NAMES: {
        std::vector<UWideString> theNames;
        static_cast<EESETriggerScreen*>(p[1])->mScreens[i[0]]->GetNames(theNames);
        theList->AppendStringArray(theNames);
        return 0;
    }
    case EE_SCREENS_SETS_OP_VIEW:
        SetsView(static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(p[0])->GetForm()), i[0] != 0);
        return 0;
    case EE_SCREENS_SETS_OP_DRAW: {
        GERectangle theClip;
        EEUIFScenarioEditor* theEditor = gUIManager->mScenarioEditorForm;
        if (!theEditor->GetWasUIHidden() && static_cast<const GERectangle*>(p[1])->And(theTab->mR18, theClip))
            theEditor->UIForm::DrawBackground(theEditor->GetPanelBackground(), theClip,
                                              GERectangle(0.0f, 0.0f, 256.0f, 256.0f), theTab->mR18);
        return 0;
    }
    case EE_SCREENS_SETS_OP_DEBUG_IDS: return gWorld.mScenarioManager->mShowDebugIDs ? 1 : 0;
    case EE_SCREENS_SETS_OP_LIST_SELECTION: {
        unsigned long theSelection;
        return theList->GetCurrentSelection(theSelection) == kErrorOK ? (int32_t)theSelection : -1;
    }
    case EE_SCREENS_SETS_OP_LIST_COUNT: return (int32_t)theList->GetLineCount();
    case EE_SCREENS_SETS_OP_LIST_SELECT: theList->ChangeSelectionIndex((unsigned long)i[0], i[1] != 0, i[2] != 0); return 0;
    case EE_SCREENS_SETS_OP_CARET_END: {
        UWideString theText;
        static_cast<UIEditBox*>(p[0])->GetAllText(theText);
        static_cast<UIEditBox*>(p[0])->SetCaretLocation(theText.GetLength(), 0);
        return 0;
    }
    case EE_SCREENS_SETS_OP_REDRAW: gUIManager->RedrawAll(); return 0;
    case EE_SCREENS_SETS_OP_STORING: gB4C3220 = i[0] != 0; return 0;
    case EE_SCREENS_SETS_OP_GROUP:
        if (i[1])
            static_cast<UIControl*>(p[0])->GetForm()->GetEntityGroup(i[0])->Enable();
        else
            static_cast<UIControl*>(p[0])->GetForm()->GetEntityGroup(i[0])->Disable();
        return 0;
    }
    return 0;
}

// MISSING 0x57cadf: EEScenarioManager::GetCondition's COMDAT (inline, ee/EEScenarioManager.h); no user written in this
// unit yet, so it is not emitted here
