// New (port; UI cluster round 3): the generic C++ shells' base calls and their host (ee/EEUIShell.h; ee_game screens
// shells.rs): the game's C++ each kind's Rust calls, one case per step of the deleted classes' code.
#include <windows.h>
#include <stdio.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIRadioButton.h"
#include "lle/UICheckBox.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UISlider.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESETSObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEUIShell.h"

int64_t EEUIShellButton::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIButton)
    EE_SHELL_BUTTON_BASE(UIButton)
    }
    return 0;
}

int64_t EEUIShellRadio::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIRadioButton)
    EE_SHELL_BUTTON_BASE(UIRadioButton)
    case EE_SCREENS_SV_CHECK: UIRadioButton::Check(); return 0;
    case EE_SCREENS_SV_UNCHECK: UIRadioButton::UnCheck(); return 0;
    }
    return 0;
}

int64_t EEUIShellCheck::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UICheckBox)
    EE_SHELL_BUTTON_BASE(UICheckBox)
    }
    return 0;
}

int64_t EEUIShellCombo::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIComboBox)
    case EE_SCREENS_SV_CHANGE_SELECTION: UIComboBox::ChangeSelection(); return 0;
    }
    return 0;
}

int64_t EEUIShellList::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UILBString)
    case EE_SCREENS_SV_BUTTON_UP: UILBString::ButtonUp(); return 0;
    case EE_SCREENS_SV_CHANGE_SELECTION_INDEX:
        UILBString::ChangeSelectionIndex((unsigned long)inA, (inB & 1) != 0, (inB & 2) != 0);
        return 0;
    }
    return 0;
}

int64_t EEUIShellEdit::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UIEditBox)
    case EE_SCREENS_SV_PROCESS_ENTER: UIEditBox::ProcessEnterKey(); return 0;
    }
    return 0;
}

int64_t EEUIShellSlider::BaseCall(uint32_t inVirtual, int64_t inA, int64_t inB, float* ioF)
{
    switch (inVirtual) {
    EE_SHELL_COMMON_BASE(UISlider)
    case EE_SCREENS_SV_CHANGE_RANGE: UISlider::ChangeRange((unsigned long)inA); return 0;
    }
    return 0;
}

// the control's text (inWhich 0: its own GEText, 1: its label's)
static GEText* EEShellText(UIControl* inControl, int64_t inWhich)
{
    if (UIRadioButton* r = dynamic_cast<UIRadioButton*>(inControl))
        return inWhich ? r->GetTextLabel()->GetGEText() : r->GetGEText();
    if (UICheckBox* k = dynamic_cast<UICheckBox*>(inControl))
        return inWhich ? k->GetTextLabel()->GetGEText() : k->GetGEText();
    if (UIButton* b = dynamic_cast<UIButton*>(inControl))
        return b->GetGEText();
    if (UIComboBox* c = dynamic_cast<UIComboBox*>(inControl))
        return c->GetGEText();
    if (UILBString* l = dynamic_cast<UILBString*>(inControl))
        return l->GetGEText();
    return static_cast<UIEditBox*>(inControl)->GetGEText();
}

static EESETriggerScreen* EEShellTriggers()
{
    return gUIManager->GetScenarioEditorForm()->GetTriggerScreen();
}

extern "C" int64_t ee_screenscpp_shell(void* inControl, uint32_t inOp, int64_t a, int64_t b, float* f)
{
    UIControl* c = static_cast<UIControl*>(inControl);
    UWideString theText;
    if (inOp >= EE_SCREENS_SH_BASE)
        return EEUIShell::Of(c)->BaseCall(inOp - EE_SCREENS_SH_BASE, a, b, f);
    switch (inOp) {
    case EE_SCREENS_SH_BG_OPACITY: c->SetBgOpacity(f[0]); break;
    case EE_SCREENS_SH_BG_COLOR: c->SetBgColor(GEColor(f[0], f[1], f[2])); break;
    case EE_SCREENS_SH_BG_PLAYER_COLOR: c->SetBgColor(EEGetPlayerColor((unsigned long)a)); break;
    case EE_SCREENS_SH_TEXT_FG: {
        GEText* t = EEShellText(c, a);
        t->SetFgColor(GEColor(f[0], f[1], f[2]));
        t->SetIsTransparent(true);
        break;
    }
    case EE_SCREENS_SH_SELECTED_COLOR:
        static_cast<UILBString*>(c)->GetSelectedBackground()->SetColor(GEColor(f[0], f[1], f[2]));
        break;
    case EE_SCREENS_SH_SELECTED_OPACITY: static_cast<UILBString*>(c)->GetSelectedBackground()->SetOpacity(f[0]); break;
    case EE_SCREENS_SH_BUTTON_TEXT:
        gResource->GetString((unsigned long)a, theText);
        static_cast<UIButton*>(c)->SetText(theText, reinterpret_cast<GEFont*>(b));
        break;
    case EE_SCREENS_SH_SELECTION_INDEX: return (int64_t)(uint32_t)static_cast<EEUIShellCombo*>(c)->Selection();
    case EE_SCREENS_SH_SET_SELECTION: static_cast<UIComboBox*>(c)->SetCurrentSelection((unsigned long)a); break;
    case EE_SCREENS_SH_DELETE_ALL: static_cast<UIComboBox*>(c)->DeleteAllEntries(); break;
    case EE_SCREENS_SH_APPEND_STRING:
        gResource->GetString((unsigned long)a, theText);
        static_cast<UIComboBox*>(c)->AppendLine(theText);
        break;
    case EE_SCREENS_SH_APPEND_NUMBERED: {
        UWideString theLine;
        gResource->GetString((unsigned long)a, theText);
        for (unsigned long i = 1; i <= (unsigned long)b; i++) {
            sprintf(theLine, theText, i);
            static_cast<UIComboBox*>(c)->AppendLine(theLine);
        }
        break;
    }
    case EE_SCREENS_SH_SCROLL_CAPTURED: return static_cast<UILBString*>(c)->GetScrollBarCaptured();
    case EE_SCREENS_SH_RELEASE_MOUSE: c->ReleaseCapturedMouse(); break;
    case EE_SCREENS_SH_FIRE: c->FireTriggerEvent((unsigned long)a); break;
    case EE_SCREENS_SH_SLIDER_VALUE: return (int64_t)(uint32_t)static_cast<UISlider*>(c)->GetCurrentRangeValue();
    case EE_SCREENS_SH_SLIDER_MAX: return static_cast<UISlider*>(c)->GetMaxRange();
    case EE_SCREENS_SH_LABEL_NUMBER:
        static_cast<UITextLabel*>(c->GetForm()->GetControlById((unsigned long)a))->SetText((long)b);
        break;
    // the game's
    case EE_SCREENS_SH_TRIGGER_REFRESH: EEShellTriggers()->mScreens[a]->RefreshControls(); break;
    case EE_SCREENS_SH_TRIGGER_REFRESH_SCREEN: EEShellTriggers()->RefreshScreen(); break;
    case EE_SCREENS_SH_TRIGGER_JUMP: EESETriggerJump((unsigned long)a, (unsigned long)b); break;
    case EE_SCREENS_SH_OBJECT_UPDATE_SELECTION:
        static_cast<EESETSObject*>(EEShellTriggers()->GetScreen(kSETriggerScreenObject))->UpdateSelection();
        break;
    case EE_SCREENS_SH_OBJECT_STORE: EEShellTriggers()->GetScreen(kSETriggerScreenObject)->Store(); break;
    case EE_SCREENS_SH_COPY_FIRST_TO_ALL:
        ((EESEPlayerScreen*)static_cast<EEUIFScenarioEditor*>(c->GetForm())->GetTabGroup(kEditorTabPlayers))->CopyFirstPlayerToAll();
        break;
    case EE_SCREENS_SH_BRUSH_SIZE: reinterpret_cast<EEUIFSEBrushGroup*>(a)->mBrushSize = (long)b; break;
    }
    return 0;
}
