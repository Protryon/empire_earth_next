// C:\EEX\Empire Earth\Scenario Editor\EESETSCComboBox.cpp (file name inferred: no __FILE__). The condition screen's
// controls: 0x17daa8..0x17e49e (EESETSConditionControls' ctor, EESETSEScenarioComboBox's ctor + ??_G, EESETSCComboBox's
// ctor + ChangeSelection, EESETSConditionControls' dtor; the controls' ctor and dtor are ee_screens
// editor_controls.rs). After EESETSArea.cpp's COMDATs (the ??_G 0x17da35..), before EESETSCondition.cpp.
// Unit flags: the exe module set.
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
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSCComboBox.h"

// Creates a trigger-screen combo box listing scenario items for the given screen and mode, 20 pixels high
EESETSEScenarioComboBox::EESETSEScenarioComboBox(long inScreen, unsigned long inMode, unsigned long inId,
                                                 unsigned long inTextBoxType, std::vector<UWideString> inData,
                                                 GEFont* inFont, const GE2DPoint& inLocation, float inWidth,
                                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, inWidth, 20.0f), inMaxListBoxHeight, inForm,
                 inIndex, inMaxChars, inBorderThickness)
{
    this->mMode = inMode;
    this->mScreen = inScreen;
}

// Creates a condition-page combo box of the given width, 20 pixels high
EESETSCComboBox::EESETSCComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString>& inData,
                                 GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                                 unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                                 unsigned long inMaxChars, unsigned long inBorderThickness)
    : UIComboBox(inId, inTextBoxType, inData, inFont, GERectangle(inLocation, (float)inWidth, 20.0f), inMaxListBoxHeight,
                 inForm, inIndex, inMaxChars, inBorderThickness)
{
}

// Changes the selection and refreshes the condition page's controls and generated name
void EESETSCComboBox::ChangeSelection()
{
    UIComboBox::ChangeSelection();
    static_cast<EESETriggerScreen*>(gUIManager->mScenarioEditorForm->mTabGroups[kEditorTabTriggers])->GetScreen(kSETriggerScreenCondition)->RefreshControls();
}
