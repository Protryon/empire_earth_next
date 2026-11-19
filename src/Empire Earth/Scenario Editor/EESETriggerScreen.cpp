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
#include "ee/EEUIShell.h"
#include "lle/USingleton.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "ee/EESEARust.h"
#include "rust/game/screens.h"
extern char gEmptyString[]; // 0x4e5b98: the shared empty string
extern GEFont* gSeplScreenFont;   // the screen's font while the tab makes its controls (EESERBUnitTypes.cpp)

bool gB4C3220 = true;

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
        UICheckBox* t = new EEUIShellCheck(EE_SCREENS_SK_SHOW_IDS, {}, 0x7de, gEmptyString, gU531930,
                                           GERectangle(GE2DPoint(305.0f, f[0]),
                                                       (float)(unsigned long)theManager->GetCheckOn()->GetWidth(),
                                                       (float)(unsigned long)theManager->GetCheckOn()->GetHeight()),
                                           theForm, (unsigned long)i[0], 0, false);
        SetsEvent(t, kControlTriggerButtonUp, EE_SCREENS_SE_SHOW_DEBUG_IDS);
        theForm->AddControl(t, 14);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SETS_OP_FONT_HEIGHT: return (int32_t)gU531930->GetHeight();
    case EE_SCREENS_SETS_OP_NAMES: {
        std::vector<UWideString> theNames;
        switch (i[0]) {   // p1 the sub-screen's object (the Rust host's)
        case kSETriggerScreenTrigger: ee_screens_setr_get_names(static_cast<EeScreensSeTrigger*>(p[1]), &theNames); break;
        case kSETriggerScreenCondition: ee_screens_secd_names(static_cast<EeScreensSeConditionScreen*>(p[1]), &theNames); break;
        case kSETriggerScreenEffect: ee_screens_sefx_names(static_cast<EeScreensSeEffectScreen*>(p[1]), &theNames); break;
        case kSETriggerScreenObject: ee_screens_seob_get_names(static_cast<EeScreensSeObject*>(p[1]), &theNames); break;
        default: ee_screens_sear_names(static_cast<EeScreensSeArea*>(p[1]), &theNames); break;
        }
        theList->AppendStringArray(theNames);
        return 0;
    }
    case EE_SCREENS_SETS_OP_VIEW:
        SetsView(static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(p[0])->GetForm()), i[0] != 0);
        return 0;
    case EE_SCREENS_SETS_OP_DRAW: {
        GERectangle theClip;
        const GERectangle thePanel(0.0f, 448.0f, 1023.0f, 606.0f);   // the tab's +0x18
        EEUIFScenarioEditor* theEditor = gUIManager->GetScenarioEditorForm();
        if (!theEditor->GetWasUIHidden() && static_cast<const GERectangle*>(p[1])->And(thePanel, theClip))
            theEditor->UIForm::DrawBackground(theEditor->GetPanelBackground(), theClip,
                                              GERectangle(0.0f, 0.0f, 256.0f, 256.0f), thePanel);
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
