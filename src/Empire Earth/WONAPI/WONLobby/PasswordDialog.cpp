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

// Password dialog: asks for the password of a protected room or game
PasswordDialog::PasswordDialog() {}



// FUNCTION: WON 0x7f9050
// Looks up the password field and the OK and cancel buttons from the layout
void PasswordDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mPasswordInput, "PasswordInput", __FILE__, 24);
    SafeGetComponent(aConfig, this->mOkButton, "OkButton", __FILE__, 25);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 26);
}

// FUNCTION: WON 0x7f90c0
// Returns the entered password
const GUIString& PasswordDialog::GetPassword()
{
    return this->mPasswordInput->GetText();
}

}

namespace WONAPI {
// Clears the password and focuses it (WON SDK function; its body is shared with an identical one elsewhere)
void PasswordDialog::Reset()
{
    this->mPasswordInput->Clear(false);
    this->mPasswordInput->RequestFocus();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8f50
// (public: virtual void * __thiscall WONAPI::PasswordDialog::`scalar deleting destructor'(unsigned int)    [??_GPasswordDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f8f70
// (public: virtual __thiscall WONAPI::PasswordDialog::~PasswordDialog(void)    [??1PasswordDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8f20
// (public: __thiscall WONAPI::PasswordDialog::PasswordDialog(void)    [??0PasswordDialog@WONAPI@@QAE@XZ])
