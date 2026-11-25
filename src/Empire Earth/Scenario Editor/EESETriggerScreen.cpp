// C:\EEX\Empire Earth\Scenario Editor\EESETriggerScreen.cpp (file name inferred: no __FILE__; RTTI class
// EESETriggerScreen). The editor's trigger tab: 0x17b0b7..0x17ccf3 (ctor, the ??_G pair, dtor, the event handlers and
// slots in source order, the sub-screen factory, then the COMDATs this unit is the first user of: the list and radio
// button ??_G, EEScenarioManager::GetCondition, the events' ??_G, the hide-UI event's SetRectangles). After the "?" unit
// 0x17acf6..0x17b0b7 (a globals chain), before EESETSArea.cpp (0x17ccf3). Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIShell.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMiniMap.h"
#include "ee/EEGraphics.h"
#include "ee/EEScreenEvent.h"
#include "ee/EESEARust.h"

bool gB4C3220 = true;

// ---- the trigger tab's host (editor_tscreen.rs; EE_SCREENS_SETS_OP_*): its tab and bitmap buttons, the sub-screens,
// the views, the list


namespace {

UWideString SetsBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}

std::vector<UWideString> sSetsNames;                           // EE_SCREENS_SETS_OP_NAMES' kept names
GERectangle sSetsClip;                                          // EE_SCREENS_SETS_OP_DRAW's kept clip
const GERectangle kSetsPanel(0.0f, 448.0f, 1023.0f, 606.0f);   // the tab's +0x18

}   // namespace

// The ops (editor_tscreen.rs; p: three handles, i: four integers, f: four floats; editor_trigx.rs composes the
// sequences)
extern "C" int32_t ee_screenscpp_sets(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SETS_OP_FONT_HEIGHT: return (int32_t)gU531930->GetHeight();
    case EE_SCREENS_SETS_OP_DEBUG_IDS: return gWorld.mScenarioManager->mShowDebugIDs ? 1 : 0;
    case EE_SCREENS_SETS_OP_REDRAW: gUIManager->RedrawAll(); return 0;
    case EE_SCREENS_SETS_OP_STORING: gB4C3220 = i[0] != 0; return 0;
    }
    return 0;
}

// the tab's single statements (editor_trigx.rs EE_SCREENS_SETSX_*, 400..: p0 and p1 the objects, p2 the out word of
// a made or found one, i and f the numbers)
extern "C" int32_t ee_screenscpp_setsx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    UIForm* theForm = static_cast<UIForm*>(p[0]);
    UIRadioButton* t = static_cast<UIRadioButton*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SETSX_NEW_TAB:
        *theOut = new UIRadioButton(2000 + (unsigned long)i[0], SetsBytes(p[1], i[2]), gU531930,
                                    GERectangle(f[0], f[1], f[2], f[3]), theForm, (unsigned long)i[1], 2, true, i[0] == 0,
                                    false);
        return 0;
    case EE_SCREENS_SETSX_UP_CHECKED_NULL: t->SetUpCheckedBitmap(NULL); return 0;
    case EE_SCREENS_SETSX_DOWN_CHECKED_NULL: t->SetDownCheckedBitmap(NULL); return 0;
    case EE_SCREENS_SETSX_DOWN_UNCHECKED_NULL: t->SetDownUncheckedBitmap(NULL); return 0;
    case EE_SCREENS_SETSX_UP_UNCHECKED_NULL: t->SetUpUncheckedBitmap(NULL); return 0;
    case EE_SCREENS_SETSX_LABEL_RECT: t->GetTextLabel()->SetRectangle(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SETSX_BG_OPACITY: t->SetBgOpacity(f[0]); return 0;
    case EE_SCREENS_SETSX_RADIO_GETEXT: *theOut = t->GetTextLabel()->GetGEText(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 0: *theOut = theForm->GetManager()->GetAlphaSortUpBitmap(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 1: *theOut = theForm->GetManager()->GetAlphaSortDownBitmap(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 2: *theOut = theForm->GetManager()->GetSBBitmapUpNormal(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 3: *theOut = theForm->GetManager()->GetSBBitmapUpPressed(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 4: *theOut = theForm->GetManager()->GetSBBitmapDownNormal(); return 0;
    case EE_SCREENS_SETSX_BITMAP + 5: *theOut = theForm->GetManager()->GetSBBitmapDownPressed(); return 0;
    case EE_SCREENS_SETSX_NEW_IMAGE_BUTTON:
        *theOut = new UIButton(i[0], static_cast<GEBitmap*>(p[1]), NULL, static_cast<GEBitmap*>(*theOut),
                               GERectangle(GE2DPoint(305.0f, f[0]), static_cast<GEBitmap*>(p[1])->GetWidth(),
                                           static_cast<GEBitmap*>(p[1])->GetHeight()),
                               theForm, (unsigned long)i[1], 0);
        return 0;
    case EE_SCREENS_SETSX_NEW_SHOW_IDS:
        *theOut = static_cast<UICheckBox*>(new EEUIShellCheck(
            EE_SCREENS_SK_SHOW_IDS, {}, 0x7de, gEmptyString, gU531930,
            GERectangle(GE2DPoint(305.0f, f[0]), (float)(unsigned long)theForm->GetManager()->GetCheckOn()->GetWidth(),
                        (float)(unsigned long)theForm->GetManager()->GetCheckOn()->GetHeight()),
            theForm, (unsigned long)i[0], 0, false));
        return 0;
    case EE_SCREENS_SETSX_SCREEN_EVENT: *theOut = EECreateScreenEvent((uint32_t)i[0]); return 0;
    case EE_SCREENS_SETSX_SET_TRIGGER: static_cast<UIControl*>(p[0])->SetTriggerEvent((unsigned long)i[0], static_cast<UIEvent*>(p[1])); return 0;
    case EE_SCREENS_SETSX_EVENT_RELEASE: static_cast<UIEvent*>(p[0])->Dereference(); return 0;
    case EE_SCREENS_SETSX_ADD_CONTROL: theForm->AddControl(static_cast<UIControl*>(p[1]), 14); return 0;
    case EE_SCREENS_SETSX_NAMES_CLEAR: sSetsNames.clear(); return 0;
    case EE_SCREENS_SETSX_SUB_NAMES + EE_SCREENS_SE_TRIGGER_SCREEN_TRIGGER:
        ee_screens_setr_get_names(static_cast<EeScreensSeTrigger*>(p[1]), &sSetsNames);
        return 0;
    case EE_SCREENS_SETSX_SUB_NAMES + EE_SCREENS_SE_TRIGGER_SCREEN_CONDITION:
        ee_screens_secd_names(static_cast<EeScreensSeConditionScreen*>(p[1]), &sSetsNames);
        return 0;
    case EE_SCREENS_SETSX_SUB_NAMES + EE_SCREENS_SE_TRIGGER_SCREEN_EFFECT:
        ee_screens_sefx_names(static_cast<EeScreensSeEffectScreen*>(p[1]), &sSetsNames);
        return 0;
    case EE_SCREENS_SETSX_SUB_NAMES + EE_SCREENS_SE_TRIGGER_SCREEN_OBJECT:
        ee_screens_seob_get_names(static_cast<EeScreensSeObject*>(p[1]), &sSetsNames);
        return 0;
    case EE_SCREENS_SETSX_SUB_NAMES + EE_SCREENS_SE_TRIGGER_SCREEN_AREA: ee_screens_sear_names(static_cast<EeScreensSeArea*>(p[1]), &sSetsNames); return 0;
    case EE_SCREENS_SETSX_LIST_APPEND_NAMES: static_cast<UILBString*>(p[0])->AppendStringArray(sSetsNames); return 0;
    case EE_SCREENS_SETSX_EDITOR: *theOut = static_cast<EEUIFScenarioEditor*>(static_cast<UIControl*>(p[0])->GetForm()); return 0;
    case EE_SCREENS_SETSX_CONTROL_BY_ID: *theOut = static_cast<EEUIFScenarioEditor*>(p[0])->GetControlById((unsigned long)i[0]); return 0;
    case EE_SCREENS_SETSX_SET_DISPLAY: static_cast<UIControl*>(p[0])->SetDisplayMode((unsigned long)i[0]); return 0;
    case EE_SCREENS_SETSX_SET_GAME_VIEW: static_cast<EEUIFScenarioEditor*>(p[0])->SetGameView(GERectangle(f[0], f[1], f[2], f[3]), false); return 0;
    case EE_SCREENS_SETSX_GAME_VIEW_RECT: EESetGameViewRectangle(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SETSX_CHECKED_EVENT:   // (the hide-UI box's event, an EESEERust: its Rust event)
        *theOut = static_cast<EESEERust*>(t->GetTriggerEvent(kControlTriggerChecked))->mEvent;
        return 0;
    case EE_SCREENS_SETSX_SET_CHECKED: static_cast<UICheckBox*>(p[0])->SetChecked(true); return 0;
    case EE_SCREENS_SETSX_GROUP_ENABLE: static_cast<EEUIFScenarioEditor*>(p[0])->GetEntityGroupById((unsigned long)i[0])->Enable(); return 0;
    case EE_SCREENS_SETSX_CLEAR_529066: gB529066 = false; return 0;
    case EE_SCREENS_SETSX_CLEAR_FOLLOWING: gCameraFollowing = false; return 0;
    case EE_SCREENS_SETSX_CLEAR_529068: gB529068 = false; return 0;
    case EE_SCREENS_SETSX_MINIMAP: return gMiniMap != NULL;
    case EE_SCREENS_SETSX_MINIMAP_REFRESH: gMiniMap->RefreshView(); return 0;
    case EE_SCREENS_SETSX_REDRAW_ALL: gUIManager->RedrawAll(); return 0;
    case EE_SCREENS_SETSX_ALL_DIRTY: gUIManager->GetCurrentForm()->SetAllControlsDirty(); return 0;
    case EE_SCREENS_SETSX_WAS_UI_HIDDEN: return gUIManager->GetScenarioEditorForm()->GetWasUIHidden();
    case EE_SCREENS_SETSX_CLIP_PANEL: return static_cast<const GERectangle*>(p[0])->And(kSetsPanel, sSetsClip);
    case EE_SCREENS_SETSX_DRAW_PANEL:
        gUIManager->GetScenarioEditorForm()->UIForm::DrawBackground(gUIManager->GetScenarioEditorForm()->GetPanelBackground(),
                                                                    sSetsClip, GERectangle(0.0f, 0.0f, 256.0f, 256.0f),
                                                                    kSetsPanel);
        return 0;
    }
    return 0;
}

// MISSING 0x57cadf: EEScenarioManager::GetCondition's COMDAT (inline, ee/EEScenarioManager.h); no user written in this
// unit yet, so it is not emitted here
