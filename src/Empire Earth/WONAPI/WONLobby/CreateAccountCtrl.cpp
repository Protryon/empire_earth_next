// Create-account dialog: user name, password, e-mail, birth date and newsletter fields with error labels
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
CreateAccountCtrl::CreateAccountCtrl() {}
// The implicit destructor (0x7ba320) releases the fourteen control pointers in reverse order, then
// the Container base
// FUNCTION: WON 0x007ba4f0
// Looks up the account fields, their error labels and the create button from the layout, then validates
void CreateAccountCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mUserNameInput,"UserNameInput",__FILE__,30);
    SafeGetComponent(config,this->mUserNameDesc,"UserNameDesc",__FILE__,31);
    SafeGetComponent(config,this->mPasswordInput,"PasswordInput",__FILE__,32);
    SafeGetComponent(config,this->mPasswordDesc,"PasswordDesc",__FILE__,33);
    SafeGetComponent(config,this->mConfirmPasswordInput,"ConfirmPasswordInput",__FILE__,34);
    SafeGetComponent(config,this->mConfirmPasswordDesc,"ConfirmPasswordDesc",__FILE__,35);
    SafeGetComponent(config,this->mEmailInput,"EmailInput",__FILE__,36);
    SafeGetComponent(config,this->mEmailDesc,"EmailDesc",__FILE__,37);
    SafeGetComponent(config,this->mMonthInput,"MonthInput",__FILE__,38);
    SafeGetComponent(config,this->mDayInput,"DayInput",__FILE__,39);
    SafeGetComponent(config,this->mYearInput,"YearInput",__FILE__,40);
    SafeGetComponent(config,this->mBirthdateDesc,"BirthdateDesc",__FILE__,41);
    SafeGetComponent(config,this->mNewsletterCheck,"NewsletterCheck",__FILE__,42);
    SafeGetComponent(config,this->mCreateButton,"CreateButton",__FILE__,43);
    this->UpdateCreateButton();
}
// Does not use the dialog itself
// FUNCTION: WON 0x007ba680
// Shows or hides an error label and returns whether it is shown
bool CreateAccountCtrl::ShowErrorLabel(bool inVisible,Label* inLabel)
{
    inLabel->SetVisible(inVisible);
    return inVisible;
}
// FUNCTION: WON 0x007ba6a0
// Returns whether an e-mail address has an @ after its first character and a dot with text after it
bool LobbyIsValidEmail(const std::string& inEmail)
{
    if(inEmail.empty()) return false;
    std::string::size_type at=inEmail.find('@',1);
    if(at==std::string::npos) return false;
    std::string::size_type dot=inEmail.find('.',at);
    if(dot==std::string::npos) return false;
    std::string::size_type length=inEmail.length();
    if(length>dot+1) return true;
    return false;
}
// All three fields are read before any of them is checked
// FUNCTION: WON 0x007ba790
// Returns whether month 1-12, day 1-31 and year 1890-1999 were entered (no per-month day check)
bool CreateAccountCtrl::IsValidBirthDate(InputBox* inMonth,InputBox* inDay,InputBox* inYear)
{
    int month=inMonth->GetText().mData->ToInt(0);
    int day=inDay->GetText().mData->ToInt(0);
    int year=inYear->GetText().mData->ToInt(0);
    if(month<=0||day<=0||year<=1889||month>12||day>31||year>1999) return false; return true;
}
// Every check runs (and updates its label) even after one has failed
// FUNCTION: WON 0x007ba800
// Shows an error label for each invalid field and enables the create button only when all are valid
void CreateAccountCtrl::UpdateCreateButton()
{
    bool invalid=false;
    if(this->ShowErrorLabel(this->mUserNameInput->GetText().length()<2,this->mUserNameDesc.get())) invalid=true;
    if(this->ShowErrorLabel(this->mPasswordInput->GetText().length()<4,this->mPasswordDesc.get())) invalid=true;
    {
        Label* desc=this->mConfirmPasswordDesc.get();
        InputBox* confirm=this->mConfirmPasswordInput.get();
        const GUIString& password=this->mPasswordInput->GetText();
        if(this->ShowErrorLabel(password!=confirm->GetText(),desc)) invalid=true;
    }
    bool emailInvalid;
    {
        const std::string& email=this->mEmailInput->GetText();
        Label* desc=this->mEmailDesc.get();
        emailInvalid=this->ShowErrorLabel(!LobbyIsValidEmail(email),desc);
    }
    if(emailInvalid) invalid=true;
    if(this->ShowErrorLabel(!this->IsValidBirthDate(this->mMonthInput.get(),this->mDayInput.get(),this->mYearInput.get()),this->mBirthdateDesc.get())) invalid=true;
    this->mCreateButton->SetEnabled(!invalid);
}
// FUNCTION: WON 0x007ba990
// Revalidates on typing; on the create button sends a create-account request with the entered details
void CreateAccountCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==ComponentEvent_InputTextChanged) { this->UpdateCreateButton(); return; }
    if(inEvent->mType==ComponentEvent_ButtonPressed&&inEvent->mComponent==this->mCreateButton.get()) {
        SmartPtr<CreateAccountEvent> event=new CreateAccountEvent;
        event->mUserName=this->mUserNameInput->GetText();
        event->mPassword=this->mPasswordInput->GetText();
        event->mEmail=this->mEmailInput->GetText();
        event->mDay=this->mDayInput->GetText().mData->ToInt(0);
        event->mMonth=this->mMonthInput->GetText().mData->ToInt(0);
        event->mYear=this->mYearInput->GetText().mData->ToInt(0);
        event->mNewsletter=this->mNewsletterCheck->mChecked;
        this->DeliverEvent(event.get(),false);
    } else Component::HandleComponentEvent(inEvent);
}
}
// CreateAccountEvent's destructor (0x7bac50) and deleting destructor (0x7bac30) are emitted here;
// the function after them (0x7bad40) is a ServerDetailsItem factory from another file


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bac10
// (public: __thiscall WONAPI::LobbyEvent::LobbyEvent(int)    [??0LobbyEvent@WONAPI@@QAE@H@Z])
// TEMPLATE: WON 0x007bac50
// (public: virtual __thiscall WONAPI::CreateAccountEvent::~CreateAccountEvent(void)    [??1CreateAccountEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ba300
// (public: virtual void * __thiscall WONAPI::CreateAccountCtrl::`scalar deleting destructor'(unsigned int)    [??_GCreateAccountCtrl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007bac30
// (public: virtual void * __thiscall WONAPI::CreateAccountEvent::`scalar deleting destructor'(unsigned int)    [??_GCreateAccountEvent@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ba320
// (public: virtual __thiscall WONAPI::CreateAccountCtrl::~CreateAccountCtrl(void)    [??1CreateAccountCtrl@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ba290
// (public: __thiscall WONAPI::CreateAccountCtrl::CreateAccountCtrl(void)    [??0CreateAccountCtrl@WONAPI@@QAE@XZ])
