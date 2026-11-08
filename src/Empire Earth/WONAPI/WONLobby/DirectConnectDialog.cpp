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
// Add-direct-connection dialog: a name and an address for a server to connect to directly
DirectConnectDialog::DirectConnectDialog() {}

// FUNCTION: WON 0x7f7aa0
// Looks up the name and address fields and the add and cancel buttons from the layout
void DirectConnectDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mNameInput, "NameInput", __FILE__, 25);
    SafeGetComponent(aConfig, this->mAddressInput, "AddressInput", __FILE__, 26);
    SafeGetComponent(aConfig, this->mAddButton, "AddButton", __FILE__, 27);
    SafeGetComponent(aConfig, this->mCancelButton, "CancelButton", __FILE__, 28);
}
// FUNCTION: WON 0x7f7b20
// Clears both fields and puts the focus on the name
void DirectConnectDialog::Reset()
{
    this->mNameInput->RequestFocus();
    this->mNameInput->Clear(false);
    this->mAddressInput->Clear(false);
}
// FUNCTION: WON 0x7f7b50
// Returns the entered address
const GUIString& DirectConnectDialog::GetAddress()
{
    return this->mAddressInput->GetText();
}
// Returns the entered name; the linker folds it into GetAddress above (same code)
const GUIString& DirectConnectDialog::GetName()
{
    return this->mNameInput->GetText();
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f7970
// (public: virtual void * __thiscall WONAPI::DirectConnectDialog::`scalar deleting destructor'(unsigned int)    [??_GDirectConnectDialog@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x007f7990
// (public: virtual __thiscall WONAPI::DirectConnectDialog::~DirectConnectDialog(void)    [??1DirectConnectDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f7940
// (public: __thiscall WONAPI::DirectConnectDialog::DirectConnectDialog(void)    [??0DirectConnectDialog@WONAPI@@QAE@XZ])
