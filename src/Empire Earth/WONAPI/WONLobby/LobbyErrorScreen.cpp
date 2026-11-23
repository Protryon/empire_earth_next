// The lobby's top container, or the "Lobby Error" dialog shown when the lobby failed to start
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_lobby/errscreen.h"
namespace WONAPI {
extern SmartPtr<Dialog> gLobbyFailureDialog;
extern GUIString gLobbyInitError;
extern bool gLobbyInitFailed;
// the error dialog's locals (errscreen.rs builds it over these statements)
static SmartPtr<Dialog> sErrDialog;
static SmartPtr<Component> sErrBackground;
static SmartPtr<Label> sErrLabel;
static GUIString sErrText;
static GUIString sErrOkText;
static SmartPtr<Button> sErrButton;
static CenterLayout* sErrVertical;
static intptr_t ErrorScreenHost(EeWonErrScreenOp inOp)
{
    typedef EeWonErrScreenOp Op;
    switch (inOp) {
    case Op::PrvExists: return gLobbyData.get() != NULL ? 1 : 0;
    case Op::PrvContainer: return (intptr_t)static_cast<Container*>(gLobbyData->mLobbyContainer.get());
    case Op::InitFailed: return gLobbyInitFailed ? 1 : 0;
    case Op::FailureExists: return gLobbyFailureDialog.get() != NULL ? 1 : 0;
    case Op::FailureGet: return (intptr_t)static_cast<Container*>(gLobbyFailureDialog.get());
    case Op::DialogNew: sErrDialog = new Dialog; return 0;
    case Op::DialogNoEscape: sErrDialog->SetEndOnEscape(0); return 0;
    case Op::BackgroundNew: sErrBackground = WONShellNew(EeWonShellKind::MsDlgBack); return 0;
    case Op::LabelNew: sErrLabel = new Label; return 0;
    case Op::LabelWrap: sErrLabel->SetWrap(true); return 0;
    case Op::LabelCenter: sErrLabel->SetHorzAlign(HorzAlign_Center); return 0;
    case Op::LabelFit: sErrLabel->SetAdjustHeightToFitText(true); return 0;
    case Op::TextNew: sErrText = GUIString("Lobby Error\n\n"); return 0;
    case Op::TextAppendError: sErrText.append(gLobbyInitError); return 0;
    case Op::LabelSetText: sErrLabel->SetText(sErrText, true); return 0;
    case Op::OkTextNew: sErrOkText = GUIString("Ok"); return 0;
    case Op::ButtonNew: sErrButton = static_cast<Button*>(WONShellNew(EeWonShellKind::MsButton, (intptr_t)&sErrOkText)); return 0;
    case Op::ButtonFlags: sErrButton->SetButtonFlags(0x100, true); return 0;
    case Op::ButtonSize: sErrButton->SetSizeAtLeast(100, 0); return 0;
    case Op::ButtonFocus: sErrButton->RequestFocus(); return 0;
    case Op::LayoutBackground: sErrDialog->Fn2F6140(sErrBackground.get(), 3, sErrDialog.get(), 0, 0, 0, 0, 0); return 0;
    case Op::LayoutLabel: sErrDialog->Fn2F6140(sErrLabel.get(), 0x1001, sErrDialog.get(), 10, 0, -20, 0, 0); return 0;
    case Op::CenterButton: sErrDialog->AddChildLayout(new CenterLayout(sErrDialog.get(), sErrButton.get(), true, false), 0); return 0;
    case Op::LayoutButton: sErrDialog->Fn2F6140(sErrButton.get(), 0x200, sErrLabel.get(), 0, 20, 0, 0, 0); return 0;
    case Op::VerticalNew: sErrVertical = new CenterLayout(sErrDialog.get(), sErrLabel.get(), false, true); return 0;
    case Op::VerticalAdd: sErrVertical->Add(sErrButton.get()); return 0;
    case Op::VerticalLayout: sErrDialog->AddChildLayout(sErrVertical, 0); return 0;
    case Op::AddBackground: sErrDialog->AddChild(sErrBackground.get()); return 0;
    case Op::AddLabel: sErrDialog->AddChild(sErrLabel.get()); return 0;
    case Op::AddButton: sErrDialog->AddChild(sErrButton.get()); return 0;
    case Op::FailureSet: gLobbyFailureDialog = sErrDialog; return 0;
    case Op::ButtonDrop: sErrButton = NULL; return 0;
    case Op::OkTextDrop: sErrOkText = GUIString(); return 0;
    case Op::TextDrop: sErrText = GUIString(); return 0;
    case Op::LabelDrop: sErrLabel = NULL; return 0;
    case Op::BackgroundDrop: sErrBackground = NULL; return 0;
    case Op::DialogDrop: sErrDialog = NULL; return 0;
    }
    return 0;
}
// Returns the lobby container; after a failed start builds once and returns a dialog showing the error and Ok (the
// choices and the order are ee_won_lobby errscreen.rs's, R6 WON lobby and network round 35)
Container* LobbyGetContainer() { return reinterpret_cast<Container*>(ee_won_lobby_error_container(ErrorScreenHost)); }
}
