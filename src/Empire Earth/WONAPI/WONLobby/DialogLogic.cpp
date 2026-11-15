// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// DialogLogic: the lobby's modal dialogs (OK/Cancel, Yes/No, status and error boxes), inline or in popup windows.
// The class is one Rust object (R6 WONLobby round 2: rust/crates/ee_won/src/dlglogic.rs: the dialog stack, the
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
#include "rust/ee_won.h"
namespace WONAPI {

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

DialogLogic::DialogLogic() : mRust(ee_won_dlglogic_new(this, DialogLogicHost)) {}
DialogLogic::~DialogLogic() { ee_won_dlglogic_free(this->mRust); }
void DialogLogic::InitComponent(ComponentConfig* inConfig) { this->Call(EeWonDlgLogicCall::Init, (intptr_t)inConfig); }
void DialogLogic::KillHook() { this->Call(EeWonDlgLogicCall::KillHook); }
void DialogLogic::StatusOpComplete(AsyncOp* inOp) { this->Call(EeWonDlgLogicCall::StatusOpComplete, (intptr_t)inOp); }
void DialogLogic::ClearDialogs() { this->Call(EeWonDlgLogicCall::ClearDialogs); }
int DialogLogic::DoDialog(Component* inDialog) { return (int)this->Call(EeWonDlgLogicCall::DoDialog, (intptr_t)inDialog); }
void DialogLogic::RunOpWithStatus(AsyncOp* inOp, unsigned long, const GUIString& inTitle, const GUIString& inStatus,
                                  int inStatusMode, unsigned long inTimeout)
{
    this->Call(EeWonDlgLogicCall::RunOpWithStatus, (intptr_t)inOp, (intptr_t)&inTitle, (intptr_t)&inStatus,
               inStatusMode, (intptr_t)inTimeout);
}
void DialogLogic::ShowStatus(const GUIString& inTitle, const GUIString& inStatus, bool inCancel)
{
    this->Call(EeWonDlgLogicCall::ShowStatus, (intptr_t)&inTitle, (intptr_t)&inStatus, inCancel);
}
void DialogLogic::ShowError(const GUIString& inTitle, const GUIString& inStatus)
{
    this->Call(EeWonDlgLogicCall::ShowError, (intptr_t)&inTitle, (intptr_t)&inStatus);
}
// An error box with a WON status's text (no sound)
void DialogLogic::ShowError(const GUIString& inTitle, WONStatus inStatus)
{
    this->ShowStatus(inTitle, GetWONStatusString(inStatus), false);
}
int DialogLogic::ShowOkCancel(const GUIString& inTitle, const GUIString& inText, bool inPlaySound)
{
    return (int)this->Call(EeWonDlgLogicCall::ShowOkCancel, (intptr_t)&inTitle, (intptr_t)&inText, inPlaySound);
}
int DialogLogic::ShowYesNo(const GUIString& inTitle, const GUIString& inText, bool inPlaySound)
{
    return (int)this->Call(EeWonDlgLogicCall::ShowYesNo, (intptr_t)&inTitle, (intptr_t)&inText, inPlaySound);
}
void DialogLogic::Reset() { this->Call(EeWonDlgLogicCall::Reset); }
void DialogLogic::UpdateStatus(WONStatus inStatus) { this->Call(EeWonDlgLogicCall::UpdateStatus, (intptr_t)inStatus); }
bool DialogLogic::GetStatusCancelled() { return this->Call(EeWonDlgLogicCall::Field, 0) != 0; }

// ---- the host

static Component* FindDialog(ComponentConfig* inConfig, bool inStatus)
{
    Component* aFound;
    if (inStatus) {
        SmartPtr<StatusDialog> aPtr;
        SafeGetComponent(inConfig, aPtr, "StatusDialog", "DialogLogic.cpp", 49);
        aFound = aPtr.get();
    }
    else {
        SmartPtr<OkCancelDialog> aPtr;
        SafeGetComponent(inConfig, aPtr, "OkCancelDialog", "DialogLogic.cpp", 48);
        aFound = aPtr.get();
    }
    if (aFound != NULL)
        aFound->AddRef();
    return aFound;
}

static intptr_t DialogLogicHost(void* inLogic, EeWonDlgLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonDlgLogicOp Op;
    DialogLogic* aLogic = static_cast<DialogLogic*>(inLogic);
    Component* aComp = reinterpret_cast<Component*>(a);
    Window* aWindow = reinterpret_cast<Window*>(a);
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    switch (inOp) {
    case Op::FindOkCancel: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), false);
    case Op::FindStatus: return (intptr_t)FindDialog(reinterpret_cast<ComponentConfig*>(a), true);
    case Op::WmPopups: {
        if (WONAPI::WindowManager::GetDefaultWindowManager() == NULL)
            return -1;
        return WONAPI::WindowManager::GetDefaultWindowManager()->GetPopupsInOwnWindow() ? 1 : 0;
    }
    case Op::ReleaseComponent: aComp->Release(); return 0;
    case Op::AddRefComponent: aComp->AddRef(); return 0;
    case Op::ReleaseWindow: aWindow->Release(); return 0;
    case Op::AddRefWindow: aWindow->AddRef(); return 0;
    case Op::ReleaseOp: anOp->Release(); return 0;
    case Op::AddRefOp: anOp->AddRef(); return 0;
    case Op::OpKill: anOp->Kill(true); return 0;
    case Op::OpForceFinishKilled: anOp->ForceFinish(WS_Killed); return 0;
    case Op::OpGetStatus: return (intptr_t)anOp->GetStatus();
    case Op::RunOp:
        aLogic->RunOp(anOp, new LogicCompletion<DialogLogic>(&DialogLogic::StatusOpComplete, aLogic),
                      (unsigned long)b);
        return 0;
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
    case Op::Bounds: {
        const WONRectangle& aBounds = aComp->GetBounds();
        int* anOut = reinterpret_cast<int*>(b);
        anOut[0] = aBounds.x;
        anOut[1] = aBounds.y;
        anOut[2] = aBounds.width;
        anOut[3] = aBounds.height;
        return 0;
    }
    case Op::NewWindow: {
        SmartPtr<Window> aNew(CreateNativeWindow());
        aNew->SetOneComponentWindow(true);
        aNew->AddRef(); // the Rust's reference
        return (intptr_t)aNew.get();
    }
    case Op::SetUpPopup:
        aWindow->SetParent(reinterpret_cast<Window*>(b));
        aWindow->SetStyle(0x22);
        aWindow->Create(WONRectangle(0, 0, 100, 100));
        LobbyAddLogicListener(aWindow->mRoot.get());
        aWindow->mRoot->SetColorScheme(LobbyGetColorScheme());
        return 0;
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
        static_cast<StatusDialog*>(aComp)->SetText(*reinterpret_cast<const GUIString*>(b),
                                                   *reinterpret_cast<const GUIString*>(c), d != 0);
        return 0;
    case Op::StatusSetStartTick: static_cast<StatusDialog*>(aComp)->SetStartTick(WONTickCount()); return 0;
    case Op::StatusSetStatus:
        static_cast<StatusDialog*>(aComp)->SetStatus(GetWONStatusString((WONStatus)b), false);
        return 0;
    case Op::SoundEffects: return LobbyGetLobbySoundEffects() ? 1 : 0;
    case Op::PlayErrorSound: LobbyPlaySound(LOBBY_SOUND(LobbyGlobal_Error_Sound).get()); return 0;
    case Op::OkCancelSetText:
        static_cast<OkCancelDialog*>(aComp)->SetText(*reinterpret_cast<const GUIString*>(b),
                                                     *reinterpret_cast<const GUIString*>(c),
                                                     d != 0 ? LOBBY_STRING(LobbyGlobal_Yes_String) : LOBBY_STRING(LobbyGlobal_OK_String),
                                                     d != 0 ? LOBBY_STRING(LobbyGlobal_No_String) : LOBBY_STRING(LobbyGlobal_Cancel_String));
        return 0;
    case Op::RootWindowOf: return (intptr_t)aComp->GetRootWindow();
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
