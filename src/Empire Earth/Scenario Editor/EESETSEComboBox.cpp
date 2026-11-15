// C:\EEX\Empire Earth\Scenario Editor\EESETSEComboBox.cpp (file name inferred: no __FILE__). The effect screen's
// controls: 0x181792..0x182629 (EESETSEffectControls' ctor, EESETSEComboBox's ctor + ChangeSelection + ??_G,
// EESETSEffectControls' dtor; the controls' ctor and dtor are ee_screens editor_controls.rs). After
// EESETSConditionType.cpp, before EESETSEffect.cpp. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIManager.h"
#include <vector>
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EEScenarioEditorIDs.h"
#include "ee/EESETSEComboBox.h"

// FUNCTION: EXE 0x581792
// (EESETSEffectControls' constructor: ee_screens editor_controls.rs's table, made through the shared control maker)

// FUNCTION: EXE 0x582445
// Creates an effect-page combo box of the given width, 20 pixels high
EESETSEComboBox::EESETSEComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString>& inData,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight,
                 inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// FUNCTION: EXE 0x5824bd
// Changes the selection and refreshes the effect page's generated name
void EESETSEComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETriggerScreen*>(gUIManager->mScenarioEditorForm->GetTabGroup(kEditorTabTriggers))->GetScreen(kSETriggerScreenEffect)->RefreshControls();
}

// SYNTHETIC: EXE 0x5824df
// (the scalar deleting destructor of EESETSEComboBox's implicit dtor; ICF-shared with EESETSCComboBox's)

// FUNCTION: EXE 0x5824fc
// (EESETSEffectControls' destructor: ee_screens editor_controls.rs, the controls deleted in its order)
