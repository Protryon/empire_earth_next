// Base class of the lobby logic objects: runs network operations and tracks them so they can be killed
// (file name inferred: no __FILE__)

// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyLogic.h"

namespace WONAPI {
// Defined in another unit; returns void
void LobbyRunOp(AsyncOp* inOp, unsigned long inTimeout);

// The tracker's maps are built by an out-of-line constructor call, as in the original
// FUNCTION: WON 0x007e5930
// Creates the logic object, not killed, with an empty operation tracker
LobbyLogic::LobbyLogic() { this->mKilled = false; }

// FUNCTION: WON 0x007e5a40
// Destroys the logic object (members only)
LobbyLogic::~LobbyLogic() {}


void LobbyLogic::KillHook() {}
void LobbyLogic::InitComponent(ComponentConfig*) {}
void LobbyLogic::Reset() {}
void LobbyLogic::HandleControlEvent(int) {}
void LobbyLogic::HandleComponentEvent(ComponentEvent*) {}

// FUNCTION: WON 0x007e5b00
// Marks the logic killed, kills every tracked operation and runs the subclass kill hook
void LobbyLogic::Kill()
{
    this->mKilled = true;
    this->mTracker.KillAll();
    this->KillHook();
}

// FUNCTION: WON 0x007e5b20
// Runs a network operation with its completion, tracking it so Kill can cancel it
void LobbyLogic::RunTrackedOp(AsyncOp* inOp, LogicCompletionBase* inCompletion,
                             unsigned long inTimeout)
{
    inOp->mCompletion = inCompletion;
    if (inCompletion != NULL)
        inCompletion->mCheckTracker = true;
    this->mTracker.AddOp(inOp, 0);
    LobbyRunOp(inOp, inTimeout);
}

// FUNCTION: WON 0x007e5ba0
// Runs a network operation with its completion, untracked
void LobbyLogic::RunOp(AsyncOp* inOp, CompletionBase<AsyncOpPtr>* inCompletion,
                      unsigned long inTimeout)
{
    inOp->mCompletion = inCompletion;
    LobbyRunOp(inOp, inTimeout);
}

// FUNCTION: WON 0x007e5c00
// Stops tracking an operation; returns whether it was tracked
bool LobbyLogic::RemoveTrackedOp(AsyncOp* inOp)
{
    return this->mTracker.RemoveOp(inOp);
}

// NetLogic's five-byte destructor (0x7dfe00) stores no vtable of its own: probably an implicit destructor
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e5a20
// (public: virtual void * __thiscall WONAPI::LobbyLogic::`scalar deleting destructor'(unsigned int)    [??_GLobbyLogic@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0076c4f0
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> con)
// TEMPLATE: WON 0x007672b0
// (public: __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::AsyncOp>,struct std::pair<class WONAPI::SmartPtr<class WONAPI::AsyncOp> const ,clas)
