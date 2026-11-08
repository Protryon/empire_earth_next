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

// Empty constructor; its code is the compiler-generated one at 0x7f83b0 (listed at the end of this file)
LanNameDialog::LanNameDialog() {}

// FUNCTION: WON 0x7f8400
// Destroys the LAN name dialog (members only)
LanNameDialog::~LanNameDialog() {}

// FUNCTION: WON 0x7f84f0
// Looks up the name input and the OK and Cancel buttons from the dialog layout
void LanNameDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mNameInput, "LanNameInput", __FILE__, 31);
    SafeGetComponent(aConfig, this->mOkButton, "OkButton", __FILE__, 32);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 33);
}

// FUNCTION: WON 0x7f8560
// Clears the name input and focuses it
void LanNameDialog::Reset()
{
    this->mNameInput->Clear(false);
    this->mNameInput->RequestFocus();
}

}

namespace WONAPI {
// Returns the typed LAN name; no marker: the original's body is shared
const GUIString& LanNameDialog::GetName()
{
    return this->mNameInput->GetText();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f83b0
// (public: __thiscall WONAPI::LanNameDialog::LanNameDialog(void)    [??0LanNameDialog@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f83e0
// (public: virtual void * __thiscall WONAPI::LanNameDialog::`scalar deleting destructor'(unsigned int)    [??_GLanNameDialog@WONAPI@@UAEPAXI@Z])
