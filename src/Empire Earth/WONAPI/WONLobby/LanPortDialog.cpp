// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Dialog that asks for the LAN broadcast port
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
// Empty constructor; its code is the compiler-generated one at 0x7f8580 (listed at the end of this file)
LanPortDialog::LanPortDialog() {}
// FUNCTION: WON 0x7f85d0
// Destroys the LAN port dialog (members only)
LanPortDialog::~LanPortDialog() {}
// FUNCTION: WON 0x7f86c0
// Looks up the numeric port input, game port label and OK button from the dialog layout
void LanPortDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mPortInput, "PortInput", __FILE__, 32);
    this->mPortInput->mInputFilter = InputBox::NumericFilter;
    SafeGetComponent(aConfig, this->mGamePortLabel, "GamePortLabel", __FILE__, 35);
    SafeGetComponent(aConfig, this->mOkButton, "OKButton", __FILE__, 36);
}
// FUNCTION: WON 0x7f8730
// Focuses the port input and updates the OK button
void LanPortDialog::Reset()
{
    this->mPortInput->RequestFocus();
    this->CheckInput();
}
// FUNCTION: WON 0x7f8750
// Shows a port in the input, all text selected
void LanPortDialog::SetPort(unsigned short inPort)
{
    char aBuf[10];
    sprintf(aBuf, "%d", inPort);
    this->mPortInput->SetText(GUIString(new GUIStringData(aBuf)), true);
    this->mPortInput->SelectRange(0, -1);
}
// FUNCTION: WON 0x7f8840
// Shows the game (lobby) port, which the LAN port may not equal
void LanPortDialog::SetGamePort(unsigned short inPort)
{
    this->mGamePort = inPort;
    char aBuf[10];
    sprintf(aBuf, "%d", inPort);
    this->mGamePortLabel->SetText(GUIString(new GUIStringData(aBuf)), true);
}
// FUNCTION: WON 0x7f8920
// Returns the typed port
unsigned short LanPortDialog::GetPort()
{
    return this->mPortInput->GetText().mData->ToInt(0);
}
// FUNCTION: WON 0x7f8940
// Enables OK only for a port of 5000 to 65535 that differs from the game port
void LanPortDialog::CheckInput()
{
    int aPort = this->mPortInput->GetText().mData->ToInt(0);
    if (aPort != this->mGamePort && aPort > 4999 && aPort < 65536) {
        if (this->mOkButton->IsDisabled())
            this->mOkButton->SetEnabled(true);
    }
    else if (!this->mOkButton->IsDisabled())
        this->mOkButton->SetEnabled(false);
}
// FUNCTION: WON 0x7f89b0
// Rechecks the port on each edit of the input; passes other events to the dialog
void LanPortDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_InputTextChanged && inEvent->mComponent == this->mPortInput.get())
        this->CheckInput();
    else
        Dialog::HandleComponentEvent(inEvent);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8580
// (public: __thiscall WONAPI::LanPortDialog::LanPortDialog(void)    [??0LanPortDialog@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f85b0
// (public: virtual void * __thiscall WONAPI::LanPortDialog::`scalar deleting destructor'(unsigned int)    [??_GLanPortDialog@WONAPI@@UAEPAXI@Z])
