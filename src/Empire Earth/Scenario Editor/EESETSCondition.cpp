// C:\EEX\Empire Earth\Scenario Editor\EESETSCondition.cpp (file name inferred: no __FILE__; RTTI class EESETSCondition).
// The trigger screen's condition sub-screen: 0x17e49e..0x17ef10 (ctor, ??_G, dtor, ChangeConditionType, the slots in
// vtable order of their bodies, then the COMDATs this unit is the first user of: EESETriggerScreen::GetName 0x17eaab,
// the events' ??_G, EESETSCTPlayerResource's ctor and the condition types' ??_G). After the condition controls' unit
// (the EESETSCComboBox members and EESETSConditionControls' dtor 0x17e3d7), before EESETSConditionType.cpp
// (0x17ef10). Unit flags: the exe module set.
// The screen is whole in Rust (ee_screens editor_subscreens.rs) with its seven condition types
// (editor_conditions.rs): the constructor (0x57e49e), the destructor (0x57e827), ChangeConditionType (0x57e8be),
// Sort, MoveDown, MoveUp, Delete, Duplicate, Activate (0x57e96c), Store (0x57e9fb), GetNames, Hide, Load
// (0x57eae3), New, RefreshControls (0x57ebc2) and Show are its slots; the class forwards. Its hosts are the shared
// maker (EESERBUnitTypes.cpp) and the types' (EESETSEffect.cpp).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UICheckBox.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/RTSWorld.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETSCondition.h"
#include "rust/ee_screens.h"

extern GEFont* gSeplScreenFont;   // the screen's font while the object makes its controls (EESERBUnitTypes.cpp)

// FUNCTION: EXE 0x57e49e
// Creates the condition sub-screen: shared controls, title label, condition-type combo and the seven condition
// types, starting on the object condition (editor_subscreens.rs)
EESETSCondition::EESETSCondition(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_secd_new(inForm, &theIndex);
    ioIndex = theIndex;
}

// SYNTHETIC: EXE 0x57e80b
// (EESETSCondition's scalar deleting destructor)

// FUNCTION: EXE 0x57e827
// Destroys the condition sub-screen with its controls and the seven condition types (editor_subscreens.rs)
EESETSCondition::~EESETSCondition()
{
    ee_screens_secd_free(this->mRust);
}

void EESETSCondition::Call(unsigned long inWhat)
{
    ee_screens_secd_call(this->mRust, (uint32_t)inWhat, (int32_t)this->mSelection);
}

void EESETSCondition::ChangeConditionType() { this->Call(EE_SCREENS_SESC_CHANGE_TYPE); }
void EESETSCondition::Sort() { this->Call(EE_SCREENS_SESC_SORT); }
void EESETSCondition::MoveDown() { this->Call(EE_SCREENS_SESC_MOVE_DOWN); }
void EESETSCondition::MoveUp() { this->Call(EE_SCREENS_SESC_MOVE_UP); }
void EESETSCondition::Delete() { this->Call(EE_SCREENS_SESC_DELETE); }
void EESETSCondition::Duplicate() { this->Call(EE_SCREENS_SESC_DUPLICATE); }
void EESETSCondition::Activate() { this->Call(EE_SCREENS_SESC_ACTIVATE); }
void EESETSCondition::Store() { this->Call(EE_SCREENS_SESC_STORE); }
void EESETSCondition::Hide() { this->Call(EE_SCREENS_SESC_HIDE); }
void EESETSCondition::Load() { this->Call(EE_SCREENS_SESC_LOAD); }
void EESETSCondition::New() { this->Call(EE_SCREENS_SESC_NEW); }
void EESETSCondition::RefreshControls() { this->Call(EE_SCREENS_SESC_REFRESH); }
void EESETSCondition::Show() { this->Call(EE_SCREENS_SESC_SHOW); }
void EESETSCondition::GetNames(std::vector<UWideString>& outNames) { ee_screens_secd_names(this->mRust, &outNames); }

EESETSConditionControls* EESETSCondition::GetControls() const
{
    return static_cast<EESETSConditionControls*>(ee_screens_secd_handle(this->mRust, 0));
}

UIComboBox* EESETSCondition::GetTypeComboBox() const
{
    return static_cast<UIComboBox*>(static_cast<UIControl*>(ee_screens_secd_handle(this->mRust, 1)));
}

// FUNCTION: EXE 0x57eaab
// Returns the trigger screen's name edit box text (EESETriggerScreen::GetName)
// (inline, ee/EESETriggerScreen.h)

// FUNCTION: EXE 0x57ee40
// Creates the player-attribute condition type (EESETSCTPlayerResource ctor; ee_screens editor_conditions.rs)

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
