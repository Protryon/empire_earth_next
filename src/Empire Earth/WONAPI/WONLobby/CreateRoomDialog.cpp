// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
CreateRoomDialog::CreateRoomDialog() {}

// FUNCTION: WON 0x7f6bc0
// Finds the name, password and create controls in the layout and checks the form
void CreateRoomDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mNameInput, "RoomNameInput", __FILE__, 25);
    SafeGetComponent(aConfig, this->mPasswordInput, "PasswordInput", __FILE__, 26);
    SafeGetComponent(aConfig, this->mPasswordLabel, "PasswordLabel", __FILE__, 27);
    SafeGetComponent(aConfig, this->mPasswordCheck, "PasswordCheck", __FILE__, 28);
    SafeGetComponent(aConfig, this->mCreateButton, "CreateButton", __FILE__, 29);
    this->CheckInput();
}
// FUNCTION: WON 0x7f6c70
// Focuses the room name; when asked, clears the form to an open room
void CreateRoomDialog::Reset(bool inClear)
{
    this->mNameInput->RequestFocus();
    if (inClear) {
        this->mNameInput->Clear(false);
        this->mPasswordInput->Clear(false);
        this->mPasswordCheck->SetCheck(false, false);
        this->UpdatePasswordVisibility();
    }
    this->CheckInput();
}
// FUNCTION: WON 0x7f6cd0
// Shows the password field only while the password box is checked
void CreateRoomDialog::UpdatePasswordVisibility()
{
    this->mPasswordInput->SetVisible(this->mPasswordCheck->mChecked);
    this->mPasswordLabel->SetVisible(this->mPasswordCheck->mChecked);
    this->CheckInput();
}
// FUNCTION: WON 0x7f6d20
// Enables the create button only with a room name and, when shown, a password
void CreateRoomDialog::CheckInput()
{
    bool aEnable = true;
    if (this->mNameInput->mText.length() == 0)
        aEnable = false;
    else if (this->mPasswordInput->IsVisible() && this->mPasswordInput->mText.length() == 0)
        aEnable = false;
    this->mCreateButton->SetEnabled(aEnable);
}
// FUNCTION: WON 0x7f6d70
// Returns the typed password, or an empty string when the room has no password
const GUIString& CreateRoomDialog::GetPassword()
{
    if (this->mPasswordCheck->IsChecked())
        return this->mPasswordInput->GetText();
    return gEmptyGUIString;
}
// FUNCTION: WON 0x7f6da0
// Updates the password field when its box is clicked and rechecks the form on text changes
void CreateRoomDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_ButtonPressed && inEvent->mComponent == this->mPasswordCheck.get())
        this->UpdatePasswordVisibility();
    else if (inEvent->mType == ComponentEvent_InputTextChanged)
        this->CheckInput();
    else
        Dialog::HandleComponentEvent(inEvent);
}
// Returns the typed room name; its body is identical to another getter's, so it has no address of its own
const GUIString& CreateRoomDialog::GetRoomName()
{
    return this->mNameInput->GetText();
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f6a90
// (public: virtual __thiscall WONAPI::CreateRoomDialog::~CreateRoomDialog(void)    [??1CreateRoomDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f6a70
// (public: virtual void * __thiscall WONAPI::CreateRoomDialog::`scalar deleting destructor'(unsigned int)    [??_GCreateRoomDialog@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f6a30
// (public: __thiscall WONAPI::CreateRoomDialog::CreateRoomDialog(void)    [??0CreateRoomDialog@WONAPI@@QAE@XZ])
