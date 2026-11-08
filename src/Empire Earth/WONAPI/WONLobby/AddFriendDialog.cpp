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

AddFriendDialog::AddFriendDialog() {}



// FUNCTION: WON 0x7f4980
// Finds the friend name input and the OK and cancel buttons in the layout
void AddFriendDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mNameInput, "NameInput", __FILE__, 25);
    SafeGetComponent(aConfig, this->mOkButton, "OkButton", __FILE__, 26);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 27);
}

const GUIString& AddFriendDialog::GetName()
{
    return this->mNameInput->GetText();
}

// FUNCTION: WON 0x7f49f0
// Enables OK only when a friend name is typed
void AddFriendDialog::CheckInput()
{
    this->mOkButton->SetEnabled(this->mNameInput->mText.length() != 0);
}

// FUNCTION: WON 0x7f4a20
// Clears the friend name and focuses it
void AddFriendDialog::Reset()
{
    this->mNameInput->Clear(true);
    this->mNameInput->RequestFocus();
}

// FUNCTION: WON 0x7f4a40
// Rechecks OK when the name changes; passes other events to the dialog
void AddFriendDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_InputTextChanged)
        this->CheckInput();
    else
        Dialog::HandleComponentEvent(inEvent);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f4880
// (public: virtual void * __thiscall WONAPI::AddFriendDialog::`scalar deleting destructor'(unsigned int)    [??_GAddFriendDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f48a0
// (public: virtual __thiscall WONAPI::AddFriendDialog::~AddFriendDialog(void)    [??1AddFriendDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f4850
// (public: __thiscall WONAPI::AddFriendDialog::AddFriendDialog(void)    [??0AddFriendDialog@WONAPI@@QAE@XZ])
