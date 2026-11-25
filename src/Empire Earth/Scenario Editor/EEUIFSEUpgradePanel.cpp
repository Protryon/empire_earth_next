// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUpgradePanel.cpp (file name inferred: no __FILE__, no RTTI). The unit tab's
// upgrade panel: 0x1a4e99..0x1a5529 (ctor, dtor, ShowUnit). After EEUIFSEUndo.cpp (0x1a4dbe..0x1a4e99), before
// EESEACreateBlankMap.cpp (0x1a5529). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UICheckBox.h"


#include "ee/EmpireEarth.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUICPortrait.h"
// the shared empty byte string (the original references it at 0x1a4eb4; no other string has its contents)

// The panel whole in Rust (ee_screens editor_tabsx.rs SeupPanel: the status box and its controls as handles; once
// EEUIFSEUpgradePanel, a class of those pointers with its ctor, dtor and ShowUnit): its single statements here

namespace {

GERectangle SeupxRect(const float* f) { return GERectangle(f[0], f[1], f[2], f[3]); }
UILayoutUtility* SeupxLayout(void* const* p) { return static_cast<UILayoutUtility*>(p[2]); }
EEUIFMGStatusBox* SeupxBox(void* const* p) { return static_cast<EEUIFMGStatusBox*>(p[0]); }
// a control's handle (its UIControl*) as its own class
template <class T>
T* SeupxAs(void* inControl) { return static_cast<T*>(static_cast<UIControl*>(inControl)); }

}   // namespace

// the panel's single statements (ee_screens editor_tabsx.rs EE_SCREENS_SEUPX_*: p0 the box or a control, p1 the
// form or a control, p2 the layout or a control)
extern "C" int32_t ee_screenscpp_seupx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEUPX_NEW_BOX: return EESEOut(i, new EEUIFMGStatusBox(theForm));
    case EE_SCREENS_SEUPX_NEW_PORTRAIT: return EESEOut(i, static_cast<UIControl*>(new EEUICPortrait(theForm, SeupxRect(f), (unsigned long)i[0], (unsigned long)i[1], false)));
    case EE_SCREENS_SEUPX_PORTRAIT_RIGHT: f[0] = static_cast<UIControl*>(p[0])->GetRectangle().GetRight();
        return 0;
    case EE_SCREENS_SEUPX_PORTRAIT_TOP: f[0] = static_cast<UIControl*>(p[0])->GetRectangle().GetTop();
        return 0;
    case EE_SCREENS_SEUPX_NEW_NAME_LABEL: return EESEOut(i, static_cast<UIControl*>(new UITextLabel(UWideString(gEmptyString), gFontTahoma12, (unsigned long)i[0], GERectangle(GE2DPoint(f[0], f[1]), f[2], f[3]), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_NEW_LAYOUT: return EESEOut(i, new UILayoutUtility);
    case EE_SCREENS_SEUPX_LAYOUT_LEFT: SeupxLayout(p)->SetLeft(0, f[0]);
        return 0;
    case EE_SCREENS_SEUPX_LAYOUT_RIGHT: SeupxLayout(p)->SetRight(0, f[0]);
        return 0;
    case EE_SCREENS_SEUPX_LAYOUT_TOP: SeupxLayout(p)->SetTop(3, f[0]);
        return 0;
    case EE_SCREENS_SEUPX_LAYOUT_BOTTOM: SeupxLayout(p)->SetBottom(3, f[0]);
        return 0;
    case EE_SCREENS_SEUPX_LAYOUT_CALCULATE: SeupxLayout(p)->CalculateRectangle(theForm->GetRectangle(), *reinterpret_cast<GERectangle*>(f), false);
        return 0;
    case EE_SCREENS_SEUPX_LAYOUT_DELETE: delete SeupxLayout(p);
        return 0;
    case EE_SCREENS_SEUPX_NEW_BAR: return EESEOut(i, static_cast<UIControl*>(new EEUIFMGStatusBar((unsigned long)i[0], (unsigned long)i[1], SeupxLayout(p), 0x400, 0x300, SeupxRect(f), theForm, true, true, false)));
    case EE_SCREENS_SEUPX_NEW_ATTRIBUTE: return EESEOut(i, static_cast<UIControl*>(new EEUIFMGAttribute(gFontTahoma10, theForm, SeupxRect(f), (unsigned long)i[1], (unsigned long)i[0], GEColor(0.0f, 0.0f, 0.0f), true, 0)));
    case EE_SCREENS_SEUPX_NEW_ROW_LABEL: return EESEOut(i, static_cast<UIControl*>(new UITextLabel(UWideString(gEmptyString), gFontTahoma12, (unsigned long)i[0], GERectangle(GE2DPoint(f[0], f[1]), f[2], f[3]), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_BOX_ROW: SeupxBox(p)->SetControls(i[2], static_cast<UIControl*>(p[1]), static_cast<UIControl*>(p[2]));
        return 0;
    case EE_SCREENS_SEUPX_NEW_ARMOR_LABEL: return EESEOut(i, static_cast<UIControl*>(new UITextLabel(gSefxText, gFontTahoma12, (unsigned long)i[0], GERectangle(156.0f, 634.0f, 240.0f, 650.0f), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_BOX_PORTRAIT: SeupxBox(p)->mC8C = SeupxAs<EEUICPortrait>(p[1]);
        return 0;
    case EE_SCREENS_SEUPX_BOX_NAME: SeupxBox(p)->mC90 = SeupxAs<UITextLabel>(p[1]);
        return 0;
    case EE_SCREENS_SEUPX_BOX_LEFT_BAR: SeupxBox(p)->mC84 = SeupxAs<EEUIFMGStatusBar>(p[1]);
        return 0;
    case EE_SCREENS_SEUPX_BOX_MIDDLE_BAR: SeupxBox(p)->mC88 = SeupxAs<EEUIFMGStatusBar>(p[1]);
        return 0;
    case EE_SCREENS_SEUPX_BOX_ARMOR: SeupxBox(p)->mC98 = SeupxAs<UITextLabel>(p[1]);
        return 0;
    case EE_SCREENS_SEUPX_DELETE_BOX: delete SeupxBox(p);
        return 0;
    case EE_SCREENS_SEUPX_DELETE_PORTRAIT: delete SeupxAs<EEUICPortrait>(p[0]);
        return 0;
    case EE_SCREENS_SEUPX_DELETE_LABEL: delete SeupxAs<UITextLabel>(p[0]);
        return 0;
    case EE_SCREENS_SEUPX_DELETE_BAR: delete SeupxAs<EEUIFMGStatusBar>(p[0]);
        return 0;
    case EE_SCREENS_SEUPX_DELETE_ATTRIBUTE: delete SeupxAs<EEUIFMGAttribute>(p[0]);
        return 0;
    case EE_SCREENS_SEUPX_FILL: SeupxBox(p)->Fill(static_cast<EEUnit*>(p[1]));
        return 0;
    }
    return 0;
}
