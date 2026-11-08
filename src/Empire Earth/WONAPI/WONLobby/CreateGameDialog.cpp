// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
// FUNCTION: WON 0x7f63e0
// Creates the create-game dialog with no game type chosen
CreateGameDialog::CreateGameDialog() : mGameType(0) {}
// FUNCTION: WON 0x7f6450
// Destroys the create-game dialog (members only)
CreateGameDialog::~CreateGameDialog() {}
// FUNCTION: WON 0x7f6600
// Finds the name, skill, password, access radio and create controls in the layout
void CreateGameDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType != 2)
        return;
    ComponentConfig* aConfig = static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig, this->mNameInput, "GameNameInput", __FILE__, 34);
    SafeGetComponent(aConfig, this->mSkillCombo, "SkillCombo", __FILE__, 35);
    SafeGetComponent(aConfig, this->mPasswordInput, "PasswordInput", __FILE__, 36);
    SafeGetComponent(aConfig, this->mPasswordLabel, "PasswordLabel", __FILE__, 37);
    SafeGetComponent(aConfig, this->mOpenToAllRadio, "OpenToAllRadio", __FILE__, 38);
    SafeGetComponent(aConfig, this->mPasswordRadio, "PasswordRadio", __FILE__, 39);
    SafeGetComponent(aConfig, this->mInviteOnlyRadio, "InviteOnlyRadio", __FILE__, 40);
    SafeGetComponent(aConfig, this->mAskToJoinRadio, "AskToJoinRadio", __FILE__, 41);
    SafeGetComponent(aConfig, this->mCreateButton, "CreateButton", __FILE__, 42);
}
// FUNCTION: WON 0x7f6710
// Focuses the game name; when asked, clears the form back to an open game of the first skill level
void CreateGameDialog::Reset(bool inClear)
{
    this->mNameInput->RequestFocus();
    if (inClear) {
        this->mOpenToAllRadio->SetCheck(true, false);
        this->mSkillCombo->SetSelItem(0, false);
        this->mNameInput->Clear(false);
        this->mPasswordInput->Clear(false);
        this->UpdatePasswordVisibility();
    }
    this->CheckInput();
}
// FUNCTION: WON 0x7f6770
// Sets the game type; invite-only and ask-to-join exist only for Internet games (else open to all)
void CreateGameDialog::SetGameType(int inType)
{
    this->mGameType = inType;
    bool aInternet = inType == 2;
    if (this->mInviteOnlyRadio->IsChecked() || this->mAskToJoinRadio->IsChecked())
        this->mOpenToAllRadio->SetCheck(true, false);
    this->mInviteOnlyRadio->SetVisible(aInternet);
    this->mAskToJoinRadio->SetVisible(aInternet);
}
// FUNCTION: WON 0x7f67e0
// Creates a lobby game from the form: name, password if chosen, access mode and skill level
SmartPtr<LobbyGame> CreateGameDialog::GetGame()
{
    SmartPtr<LobbyGame> aGame(LobbyGame::CreateGame(static_cast<LobbyGameType>(this->mGameType)));
    aGame->SetName(this->mNameInput->GetText());
    if (this->mPasswordRadio->IsChecked())
        aGame->SetPassword(this->mPasswordInput->GetText());
    aGame->SetAskToJoin(this->mAskToJoinRadio->IsChecked());
    aGame->SetInviteOnly(this->mInviteOnlyRadio->IsChecked());
    int aSkill = this->mSkillCombo->GetSelItem();
    if (aSkill < 0 || aSkill >= 5)
        aSkill = 0;
    aGame->SetSkill(aSkill);
    return aGame;
}
// FUNCTION: WON 0x7f6920
// Enables the create button only with a game name and, when shown, a password
void CreateGameDialog::CheckInput()
{
    bool aEnable = true;
    if (this->mNameInput->GetText().length() == 0)
        aEnable = false;
    else if (this->mPasswordInput->IsVisible() && this->mPasswordInput->mText.length() == 0)
        aEnable = false;
    this->mCreateButton->SetEnabled(aEnable);
}
// FUNCTION: WON 0x7f6980
// Shows the password field only while the password radio is checked
void CreateGameDialog::UpdatePasswordVisibility()
{
    this->mPasswordInput->SetVisible(this->mPasswordRadio->mChecked);
    this->mPasswordLabel->SetVisible(this->mPasswordRadio->mChecked);
    this->CheckInput();
}
// FUNCTION: WON 0x7f69d0
// Updates the password field on a button press and rechecks the form on text or skill changes
void CreateGameDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_ButtonPressed)
        this->UpdatePasswordVisibility();
    else if (inEvent->mType == ComponentEvent_InputTextChanged) {
        this->CheckInput();
        return;
    }
    else if (inEvent->mType == ComponentEvent_ComboSelChanged && inEvent->mComponent == this->mSkillCombo.get()) {
        this->CheckInput();
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f6430
// (protected: virtual void * __thiscall WONAPI::CreateGameDialog::`scalar deleting destructor'(unsigned int)    [??_GCreateGameDialog@WONAPI@@MAEPAXI@Z])
