// Room dialog: a dialog that holds one room control (RoomCtrl); its InitComponent does nothing
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
RoomDialog::RoomDialog() {}
// FUNCTION: WON 0x7f9110
// Destroys the room dialog (members only)
RoomDialog::~RoomDialog() {}

void RoomDialog::InitComponent(ComponentInit&) {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f90d0
// (public: __thiscall WONAPI::RoomDialog::RoomDialog(void)    [??0RoomDialog@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007f90f0
// (protected: virtual void * __thiscall WONAPI::RoomDialog::`scalar deleting destructor'(unsigned int)    [??_GRoomDialog@WONAPI@@MAEPAXI@Z])
