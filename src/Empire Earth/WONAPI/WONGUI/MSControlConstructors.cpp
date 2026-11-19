// MSDialog's constructor (file name inferred; three of the original's units, their include lists joined); the other
// Windows-style controls are shell kinds (ee_won_gui shell_ms.rs, shell_mscont.rs, R6 WONLobby round 20)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "ee/wonapi/WONSkinLayout.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyMSMessageBox.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
namespace WONAPI {
// Creates a dialog with a dialog background that fills it (emitted at 0x00700550)
MSDialog::MSDialog()
{
    SmartPtr<Component> back=WONShellNew(EeWonShellKind::MsDlgBack);
    this->Fn2F6140(back.get(),3,this,0,0,0,0,0);
    this->AddChild(back.get());
}

}
