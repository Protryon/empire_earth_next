// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby's small dialogs' shell and host (R6 WONLobby round 1): LanNameDialog, PasswordDialog, AddFriendDialog,
// DirectConnectDialog, LanPortDialog, CreateRoomDialog, CreateGameDialog and AdminActionDialog are one Rust object
// each (rust/crates/ee_won/src/lobbydlg.rs). This unit is the C++ shell's two virtuals and the host: each op is the
// original's statements on a control, the dialog's base, a lobby game or a resource string, with no decision.
#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/ee_won.h"

namespace WONAPI {

static intptr_t LobbyDialogHost(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c);

LobbyRustDialog::LobbyRustDialog(EeWonLobbyDlgKind inKind)
    : mLobbyDlg(ee_won_lobbydlg_new(this, inKind, LobbyDialogHost)) {}

// the controls are released in reverse member order (the original SmartPtr members), before the Dialog base
LobbyRustDialog::~LobbyRustDialog() { ee_won_lobbydlg_free(this->mLobbyDlg); }

// the originals looked their controls up for a config init only and did not call the base
void LobbyRustDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    this->LCall(EeWonLobbyDlgCall::Init, (intptr_t) static_cast<ComponentConfigInit&>(inInit).mConfig);
}

void LobbyRustDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    EeWonLobbyDlgEvent aType = EeWonLobbyDlgEvent::Other;
    if (inEvent->mType == ComponentEvent_InputTextChanged)
        aType = EeWonLobbyDlgEvent::InputTextChanged;
    else if (inEvent->mType == ComponentEvent_ButtonPressed)
        aType = EeWonLobbyDlgEvent::ButtonPressed;
    else if (inEvent->mType == ComponentEvent_ComboSelChanged)
        aType = EeWonLobbyDlgEvent::ComboSelChanged;
    this->LCall(EeWonLobbyDlgCall::Event, (intptr_t)inEvent, (intptr_t)aType, (intptr_t)inEvent->mComponent);
}

const GUIString& LobbyRustDialog::InputText(int inMember)
{
    return reinterpret_cast<InputBox*>(this->LCall(EeWonLobbyDlgCall::Slot, inMember))->GetText();
}

const GUIString& LobbyRustDialog::InputTextOf(intptr_t inInput)
{
    if (inInput == 0)
        return gEmptyGUIString;
    return reinterpret_cast<InputBox*>(inInput)->GetText();
}

SmartPtr<LobbyGame> CreateGameDialog::GetGame()
{
    LobbyGame* aHeld = reinterpret_cast<LobbyGame*>(this->LCall(EeWonLobbyDlgCall::GetGame));
    SmartPtr<LobbyGame> aGame(aHeld);
    aHeld->Release(); // the op's reference: the SmartPtr holds the game now
    return aGame;
}

template <class T> static Component* FindHeld(ComponentConfig* inConfig, const EeWonLobbyDlgSlot* inSlot)
{
    SmartPtr<T> aPtr;
    SafeGetComponent(inConfig, aPtr, inSlot->name, inSlot->file, inSlot->line);
    Component* aComponent = aPtr.get();
    if (aComponent != NULL)
        aComponent->AddRef();
    return aComponent;
}

static const GUIString& AdminString(intptr_t inString)
{
    static GUIString sEmpty;
    switch ((EeWonLobbyDlgString)inString) {
    case EeWonLobbyDlgString::Warn: return ::AdminActionDialog_Warn_String;
    case EeWonLobbyDlgString::Mute: return ::AdminActionDialog_Mute_String;
    case EeWonLobbyDlgString::UnMute: return ::AdminActionDialog_UnMute_String;
    case EeWonLobbyDlgString::Kick: return ::AdminActionDialog_Kick_String;
    case EeWonLobbyDlgString::Ban: return ::AdminActionDialog_Ban_String;
    case EeWonLobbyDlgString::ExplanationOpt: return ::AdminActionDialog_ExplanationOpt_String;
    case EeWonLobbyDlgString::ExplanationReq: return ::AdminActionDialog_ExplanationReq_String;
    default: return sEmpty;
    }
}

static intptr_t LobbyDialogHost(void* inDialog, EeWonLobbyDlgOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonLobbyDlgOp Op;
    LobbyRustDialog* aDialog = static_cast<LobbyRustDialog*>(inDialog);
    Component* aComp = reinterpret_cast<Component*>(a);
    InputBox* anInput = reinterpret_cast<InputBox*>(a);
    Button* aButton = reinterpret_cast<Button*>(a);
    ComboBox* aCombo = reinterpret_cast<ComboBox*>(a);
    Label* aLabel = reinterpret_cast<Label*>(a);
    LobbyGame* aGame = reinterpret_cast<LobbyGame*>(a);
    char aBuf[10];
    switch (inOp) {
    case Op::Find: {
        ComponentConfig* aConfig = reinterpret_cast<ComponentConfig*>(a);
        const EeWonLobbyDlgSlot* aSlot = reinterpret_cast<const EeWonLobbyDlgSlot*>(b);
        switch (aSlot->ty) {
        case EeWonLobbyDlgType::Input: return (intptr_t)FindHeld<InputBox>(aConfig, aSlot);
        case EeWonLobbyDlgType::Button: return (intptr_t)FindHeld<Button>(aConfig, aSlot);
        case EeWonLobbyDlgType::Label: return (intptr_t)FindHeld<Label>(aConfig, aSlot);
        case EeWonLobbyDlgType::Combo: return (intptr_t)FindHeld<ComboBox>(aConfig, aSlot);
        }
        return 0;
    }
    // a held control (the end of the original's SmartPtr)
    case Op::Release: aComp->Release(); return 0;
    case Op::BaseEvent: aDialog->Dialog::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(a)); return 0;
    case Op::Relayout: aDialog->Relayout(); return 0;
    case Op::RequestFocus: aComp->RequestFocus(); return 0;
    case Op::SetEnabled: aComp->SetEnabled(b != 0); return 0;
    case Op::SetVisible: aComp->SetVisible(b != 0); return 0;
    case Op::IsVisible: return aComp->IsVisible();
    case Op::InputClear: anInput->Clear(b != 0); return 0;
    case Op::InputTextLength: return (intptr_t)anInput->GetText().length();
    case Op::InputShownLength: return (intptr_t)anInput->mText.length();
    case Op::InputToInt: return anInput->GetText().mData->ToInt(0);
    case Op::InputSetNumeric: anInput->mInputFilter = InputBox::NumericFilter; return 0;
    case Op::InputSetNumber:
        sprintf(aBuf, "%d", (int)b);
        anInput->SetText(GUIString(new GUIStringData(aBuf)), c != 0);
        return 0;
    case Op::InputSetEmpty: anInput->SetText(GUIString(""), b != 0); return 0;
    case Op::InputSelectRange: anInput->SelectRange((int)b, (int)c); return 0;
    case Op::LabelSetNumber:
        sprintf(aBuf, "%d", (int)b);
        aLabel->SetText(GUIString(new GUIStringData(aBuf)), true);
        return 0;
    case Op::LabelSetString: aLabel->SetText(AdminString(b), true); return 0;
    case Op::ButtonSetString: aButton->SetText(AdminString(b), true); return 0;
    case Op::AdminTitle: {
        GUIString aTitle = AdminString(b);
        aTitle.append(GUIString(new GUIStringData(" ")));
        aTitle.append(reinterpret_cast<LobbyClient*>(c)->GetName());
        aLabel->SetText(aTitle, true);
        return 0;
    }
    case Op::ButtonIsDisabled: return aButton->IsDisabled();
    case Op::ButtonIsChecked: return aButton->IsChecked();
    case Op::ButtonSetCheck: aButton->SetCheck(b != 0, c != 0); return 0;
    case Op::ComboGetSel: return aCombo->GetSelItem();
    case Op::ComboSetSel: aCombo->SetSelItem((int)b, c != 0); return 0;
    case Op::GameCreate: {
        LobbyGame* aNew = LobbyGame::CreateGame(static_cast<LobbyGameType>(b));
        aNew->AddRef();
        return (intptr_t)aNew;
    }
    case Op::GameSetName: aGame->SetName(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetPassword: aGame->SetPassword(reinterpret_cast<InputBox*>(b)->GetText()); return 0;
    case Op::GameSetAskToJoin: aGame->SetAskToJoin(b != 0); return 0;
    case Op::GameSetInviteOnly: aGame->SetInviteOnly(b != 0); return 0;
    case Op::GameSetSkill: aGame->SetSkill((long)b); return 0;
    }
    return 0;
}

}
