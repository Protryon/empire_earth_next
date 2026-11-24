// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// DialogLogic: the lobby's modal dialogs (OK/Cancel, Yes/No, status and error boxes), inline or in popup windows.
// The class is one Rust object (R6 WONLobby round 2: rust/crates/ee_won_lobby/src/dlglogic.rs: the dialog stack, the
// status operation, every method); this unit is its C++ shell and host: each op is the original's statements on a
// window, a component, an operation or a lobby global, with no decision.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/won_lobby/dlglogic.h"
namespace WONAPI {

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

// The dialog logic's Rust object over the shell (its virtuals and its status completion: logicshell.rs)
void* DialogLogicMake(RustLogic* inSelf) { return ee_won_dlglogic_new(inSelf, DialogLogicHost); }

// ---- the host

static Component* FindDialog(ComponentConfig* inConfig, bool inStatus)
{
    SmartPtr<LobbyRustDialog> aPtr;
    if (inStatus) SafeGetDialog(inConfig, aPtr, EeWonLobbyDlgKind::Status, "StatusDialog", "DialogLogic.cpp", 49);
    else SafeGetDialog(inConfig, aPtr, EeWonLobbyDlgKind::OkCancel, "OkCancelDialog", "DialogLogic.cpp", 48);
    Component* aFound = aPtr.get();
    if (aFound != NULL)
        aFound->AddRef();
    return aFound;
}

// the composed cases' locals (logicsplit.rs): NewWindow's window, StatusSetStatus's text, OkCancelSetText's buttons
static SmartPtr<Window> sDlgNewWindow;
static GUIString sDlgStatusText;
static const GUIString* sDlgButtons[2];

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonDlgLogicOp Op;
    RustLogic* aLogic = static_cast<RustLogic*>(inLogic);
    Component* aComp = reinterpret_cast<Component*>(a);
    Window* aWindow = reinterpret_cast<Window*>(a);
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    switch (inOp) {
    case Op::FindOkCancel: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), false);
    case Op::FindStatus: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), true);
    case Op::ReleaseComponent: aComp->Release(); return 0;
    case Op::AddRefComponent: aComp->AddRef(); return 0;
    case Op::ReleaseWindow: aWindow->Release(); return 0;
    case Op::AddRefWindow: aWindow->AddRef(); return 0;
    case Op::ReleaseOp: anOp->Release(); return 0;
    case Op::AddRefOp: anOp->AddRef(); return 0;
    case Op::OpKill: anOp->Kill(true); return 0;
    case Op::OpForceFinishKilled: anOp->ForceFinish(WS_Killed); return 0;
    case Op::OpGetStatus: return (intptr_t)anOp->GetStatus();
    case Op::RunOp: aLogic->RunOp(anOp, new ShellCompletion(aLogic, EeWonLogicShellDone::DlgStatusOp), (unsigned long)b); return 0;
    case Op::EndDialog: GetLobbyWindow()->EndDialog(-1, NULL, aComp); return 0;
    case Op::LobbyRoot: return (intptr_t)static_cast<Component*>(GetLobbyRoot());
    case Op::RootWindow: return (intptr_t)LobbyGetRootWindow();
    case Op::Parent: return (intptr_t)static_cast<Component*>(aComp->mParent);
    case Op::RemoveChild:
        static_cast<Container*>(aComp)->RemoveChild(reinterpret_cast<Component*>(b));
        return 0;
    case Op::WindowClose: aWindow->Close(); return 0;
    case Op::Width: return aComp->Width();
    case Op::Height: return aComp->Height();
    case Op::SetClientSize: aWindow->SetClientSize((int)b, (int)c); return 0;
    case Op::WindowRoot: return (intptr_t)static_cast<Component*>(aWindow->mRoot.get());
    case Op::WindowCenter: aWindow->Center(true); return 0;
    case Op::CenterLayout:
        CenterLayout::Center(static_cast<Container*>(aComp), reinterpret_cast<Component*>(b), true, true);
        return 0;
    case Op::AddChild: static_cast<Container*>(aComp)->AddChild(reinterpret_cast<Component*>(b)); return 0;
    case Op::WindowShow: aWindow->Show(true); return 0;
    case Op::RunDialog: return aWindow->RunDialog(reinterpret_cast<Component*>(b));
    case Op::LobbySuspended: return LobbyIsSuspended() ? 1 : 0;
    case Op::StatusSetText:
        static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, d != 0);
        return 0;
    case Op::StatusSetStartTick: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStartTick, (intptr_t)WONTickCount()); return 0;
    case Op::SoundEffects: return LobbyGetLobbySoundEffects() ? 1 : 0;
    case Op::PlayErrorSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_Error_Sound).get()); return 0;
    case Op::RootWindowOf: return (intptr_t)aComp->GetRootWindow();
    // R6 WON lobby and network round 35: the statements of the cases logicsplit.rs composes (WmPopups, Bounds,
    // NewWindow, SetUpPopup, StatusSetStatus, OkCancelSetText); the locals are the statics above
    case Op::WmDefault: return (intptr_t)WONAPI::WindowManager::GetDefaultWindowManager();
    case Op::WmPopupsOf: return reinterpret_cast<WindowManager*>(a)->GetPopupsInOwnWindow() ? 1 : 0;
    case Op::BoundsX: return aComp->GetBounds().x;
    case Op::BoundsY: return aComp->GetBounds().y;
    case Op::BoundsWidth: return aComp->GetBounds().width;
    case Op::BoundsHeight: return aComp->GetBounds().height;
    case Op::NewWindowMake: sDlgNewWindow = CreateNativeWindow(); return 0;
    case Op::NewWindowOneComponent: sDlgNewWindow->SetOneComponentWindow(true); return 0;
    case Op::NewWindowGet: return (intptr_t)sDlgNewWindow.get();
    case Op::NewWindowDrop: sDlgNewWindow = NULL; return 0;
    case Op::WindowSetParent: aWindow->SetParent(reinterpret_cast<Window*>(b)); return 0;
    case Op::WindowSetPopupStyle: aWindow->SetStyle(0x22); return 0;
    case Op::WindowCreatePopup: aWindow->Create(WONRectangle(0, 0, 100, 100)); return 0;
    case Op::WindowListenRoot: LobbyAddLogicListener(aWindow->mRoot.get()); return 0;
    case Op::WindowRootSetScheme: aWindow->mRoot->SetColorScheme(LobbyGetColorScheme()); return 0;
    case Op::StatusTextOf: sDlgStatusText = GetWONStatusString((WONStatus)a); return 0;
    case Op::StatusSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetStatus, (intptr_t)&sDlgStatusText, false); return 0;
    case Op::StatusTextDrop: sDlgStatusText = GUIString(); return 0;
    case Op::ButtonTextSet: sDlgButtons[a] = &gLobbyResources[b].mString; return 0;
    case Op::OkCancelSetHeldText: static_cast<LobbyRustDialog*>(aComp)->LCall(EeWonLobbyDlgCall::SetText, b, c, (intptr_t)sDlgButtons); return 0;
    case Op::InvalidateRect: {
        const int* aRect = reinterpret_cast<const int*>(b);
        WONRectangle aBounds(aRect[0], aRect[1], aRect[2], aRect[3]);
        static_cast<Container*>(aComp)->InvalidateRect(aBounds);
        return 0;
    }
    }
    return 0;
}
}
