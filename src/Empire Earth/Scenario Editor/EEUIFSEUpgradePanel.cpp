// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEUpgradePanel.cpp (file name inferred: no __FILE__, no RTTI). The unit tab's
// upgrade panel: 0x1a4e99..0x1a5529 (ctor, dtor, ShowUnit). After EEUIFSEUndo.cpp (0x1a4dbe..0x1a4e99), before
// EESEACreateBlankMap.cpp (0x1a5529). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"


#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/USingleton.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIBUpgrade.h"
#include "ee/EEUIFMGAttribute.h"
#include "ee/EEUIFMGStatusBar.h"
#include "ee/EEUICPortrait.h"
#include "ee/EEUIFSEUpgradePanel.h"
#include "ee/EEStringIDs.h"
// the shared empty byte string (the original references it at 0x1a4eb4; no other string has its contents)
extern char gEmptyString[];
extern UWideString gSefxText;   // the editor's statements' text (EEUIFScenarioEditor.cpp)
#include "rust/game/screens.h"

// Builds the unit tab's status panel (portrait, unit name, two status bars, nine attribute rows and the armor
// title), all hidden, and wires them into its status box (ee_screens editor_tabsx.rs)
EEUIFSEUpgradePanel::EEUIFSEUpgradePanel(unsigned long& ioID, unsigned long& ioIndex,
                                        EEUIFScenarioEditor* inForm)
{
    ee_screens_seup_new(this, &ioID, &ioIndex, inForm);
}

// Destroys the upgrade panel, deleting its status box and all of its controls (editor_tabsx.rs)
EEUIFSEUpgradePanel::~EEUIFSEUpgradePanel()
{
    ee_screens_seup_free(this);
}

// Fills the panel's status box with the given unit's details
void EEUIFSEUpgradePanel::ShowUnit(EEUnit* inUnit)
{
    this->mStatusBox->Fill(inUnit);
}

namespace {

int32_t SeupxOut(int32_t* i, const void* inObject)
{
    i[0] = (int32_t)(uint32_t)(uintptr_t)inObject;
    i[1] = (int32_t)(uint32_t)((uint64_t)(uintptr_t)inObject >> 32);
    return 0;
}
GERectangle SeupxRect(const float* f) { return GERectangle(f[0], f[1], f[2], f[3]); }
UILayoutUtility* SeupxLayout(void* const* p) { return static_cast<UILayoutUtility*>(p[2]); }

}   // namespace

// the panel's single statements (ee_screens editor_tabsx.rs EE_SCREENS_SEUPX_*: p0 the panel, p1 the form)
extern "C" int32_t ee_screenscpp_seupx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EEUIFSEUpgradePanel* P = static_cast<EEUIFSEUpgradePanel*>(p[0]);
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEUPX_NEW_BOX: P->mStatusBox = new EEUIFMGStatusBox(theForm);
        return 0;
    case EE_SCREENS_SEUPX_NEW_PORTRAIT: return SeupxOut(i, static_cast<UIControl*>(P->mPortrait = new EEUICPortrait(theForm, SeupxRect(f), (unsigned long)i[0], (unsigned long)i[1], false)));
    case EE_SCREENS_SEUPX_PORTRAIT_RIGHT: f[0] = P->mPortrait->GetRectangle().GetRight();
        return 0;
    case EE_SCREENS_SEUPX_PORTRAIT_TOP: f[0] = P->mPortrait->GetRectangle().GetTop();
        return 0;
    case EE_SCREENS_SEUPX_NEW_NAME_LABEL: return SeupxOut(i, static_cast<UIControl*>(P->mUnitNameLabel = new UITextLabel(UWideString(gEmptyString), gFontTahoma12, (unsigned long)i[0], GERectangle(GE2DPoint(f[0], f[1]), f[2], f[3]), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_NEW_LAYOUT: return SeupxOut(i, new UILayoutUtility);
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
    case EE_SCREENS_SEUPX_NEW_LEFT_BAR: return SeupxOut(i, static_cast<UIControl*>(P->mLeftStatusBar = new EEUIFMGStatusBar((unsigned long)i[0], (unsigned long)i[1], SeupxLayout(p), 0x400, 0x300, SeupxRect(f), theForm, true, true, false)));
    case EE_SCREENS_SEUPX_NEW_MIDDLE_BAR: return SeupxOut(i, static_cast<UIControl*>(P->mMiddleStatusBar = new EEUIFMGStatusBar((unsigned long)i[0], (unsigned long)i[1], SeupxLayout(p), 0x400, 0x300, SeupxRect(f), theForm, true, true, false)));
    case EE_SCREENS_SEUPX_NEW_ATTRIBUTE: return SeupxOut(i, static_cast<UIControl*>(P->mAttributes[i[2]] = new EEUIFMGAttribute(gFontTahoma10, theForm, SeupxRect(f), (unsigned long)i[1], (unsigned long)i[0], GEColor(0.0f, 0.0f, 0.0f), true, 0)));
    case EE_SCREENS_SEUPX_NEW_ROW_LABEL: return SeupxOut(i, static_cast<UIControl*>(P->mLabels[i[2]] = new UITextLabel(UWideString(gEmptyString), gFontTahoma12, (unsigned long)i[0], GERectangle(GE2DPoint(f[0], f[1]), f[2], f[3]), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_BOX_ROW: P->mStatusBox->SetControls(i[2], P->mAttributes[i[2]], P->mLabels[i[2]]);
        return 0;
    case EE_SCREENS_SEUPX_NEW_ARMOR_LABEL: return SeupxOut(i, static_cast<UIControl*>(P->mArmorTitleLabel = new UITextLabel(gSefxText, gFontTahoma12, (unsigned long)i[0], GERectangle(156.0f, 634.0f, 240.0f, 650.0f), theForm, 0, (unsigned long)i[1], 0)));
    case EE_SCREENS_SEUPX_BOX_PORTRAIT: P->mStatusBox->mC8C = P->mPortrait;
        return 0;
    case EE_SCREENS_SEUPX_BOX_NAME: P->mStatusBox->mC90 = P->mUnitNameLabel;
        return 0;
    case EE_SCREENS_SEUPX_BOX_LEFT_BAR: P->mStatusBox->mC84 = P->mLeftStatusBar;
        return 0;
    case EE_SCREENS_SEUPX_BOX_MIDDLE_BAR: P->mStatusBox->mC88 = P->mMiddleStatusBar;
        return 0;
    case EE_SCREENS_SEUPX_BOX_ARMOR: P->mStatusBox->mC98 = P->mArmorTitleLabel;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_BOX: delete P->mStatusBox;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_PORTRAIT: delete P->mPortrait;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_ARMOR: delete P->mArmorTitleLabel;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_LEFT_BAR: delete P->mLeftStatusBar;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_MIDDLE_BAR: delete P->mMiddleStatusBar;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_NAME: delete P->mUnitNameLabel;
        return 0;
    case EE_SCREENS_SEUPX_DELETE_ATTRIBUTE: delete P->mAttributes[i[2]];
        return 0;
    case EE_SCREENS_SEUPX_DELETE_LABEL: delete P->mLabels[i[2]];
        return 0;
    }
    return 0;
}
