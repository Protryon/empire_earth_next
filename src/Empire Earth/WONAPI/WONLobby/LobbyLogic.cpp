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
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyCore.h"

namespace WONAPI {
// Defined in another unit; returns void
void LobbyRunOp(AsyncOp* inOp, unsigned long inTimeout);

// The tracker's maps are built by an out-of-line constructor call, as in the original
// Creates the logic object, not killed, with an empty operation tracker
LobbyLogic::LobbyLogic() { this->mKilled = false; }

// Destroys the logic object (members only)
LobbyLogic::~LobbyLogic() {}


void LobbyLogic::KillHook() {}
void LobbyLogic::InitComponent(ComponentConfig*) {}
void LobbyLogic::Reset() {}
void LobbyLogic::HandleControlEvent(int) {}
void LobbyLogic::HandleComponentEvent(ComponentEvent*) {}

// Marks the logic killed, kills every tracked operation and runs the subclass kill hook
void LobbyLogic::Kill()
{
    this->mKilled = true;
    this->mTracker.KillAll();
    this->KillHook();
}

// Runs a network operation with its completion, tracking it so Kill can cancel it
void LobbyLogic::RunTrackedOp(AsyncOp* inOp, LogicCompletionBase* inCompletion,
                             unsigned long inTimeout)
{
    inOp->SetCompletion(inCompletion);
    if (inCompletion != NULL)
        inCompletion->mCheckTracker = true;
    this->mTracker.AddOp(inOp, 0);
    LobbyRunOp(inOp, inTimeout);
}

// Runs a network operation with its completion, untracked
void LobbyLogic::RunOp(AsyncOp* inOp, CompletionBase<AsyncOpPtr>* inCompletion,
                      unsigned long inTimeout)
{
    inOp->SetCompletion(inCompletion);
    LobbyRunOp(inOp, inTimeout);
}

// Stops tracking an operation; returns whether it was tracked
bool LobbyLogic::RemoveTrackedOp(AsyncOp* inOp)
{
    return this->mTracker.RemoveOp(inOp);
}

// ---- RustLogic: the shell of the six logics that are Rust objects (logicshell.rs routes its calls)

// a dialog of the kind's InitComponent into its slot (SafeGetDialog throws for a missing one: kept for the entry)
static intptr_t ShellFindDialog(void* inLogic, void* inConfig, int32_t inSlot, EeWonLobbyDlgKind inKind, const char* inName,
                                const char* inFile, int32_t inLine)
{
    return WON_GUARDED_V(SafeGetDialog(static_cast<ComponentConfig*>(inConfig), static_cast<RustLogic*>(inLogic)->mDialog[inSlot], inKind, inName, inFile, inLine));
}
RustLogic::RustLogic(EeWonLogicShellKind inKind, Maker inMake) : mKind(inKind), mRust(inMake(this)) {}
// Frees the Rust object first (it gives back what it holds), then the slots
RustLogic::~RustLogic() { ee_won_logicshell_free(this->mKind, this->mRust); }
void RustLogic::HandleComponentEvent(ComponentEvent* inEvent) { this->V(EeWonLogicShellV::HandleComponentEvent, (intptr_t)inEvent, inEvent->mType); }
// LanLogic's Resume left the current game when the lobby resumes (the others' are their Rust objects')
void RustLogic::Resume() { if (this->V(EeWonLogicShellV::Resume) != 0) LobbyLeaveGame(0); }
intptr_t RustLogic::V(EeWonLogicShellV inVirtual, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ee_won_logicshell_virtual(this->mKind, this->mRust, inVirtual, a, b, c, d, this, ShellFindDialog);
}
}
