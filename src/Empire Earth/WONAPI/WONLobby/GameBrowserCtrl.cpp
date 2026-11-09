// The lobby's web browser tab (help and news): navigation buttons, find box and status line
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONLobbyScreens.h"
namespace WONAPI {
// The home location is assigned in the body, after the members are built
// FUNCTION: WON 0x007bbdc0
// Creates the browser tab with its home page, to be loaded on first paint
GameBrowserCtrl::GameBrowserCtrl(const GUIString& inHome)
{
    this->mHomeLocation=inHome;
    this->mIsReset=true;
}

// FUNCTION: WON 0x007bc0b0
// Finds the browser, navigation, find and status controls in the layout and numbers the buttons
void GameBrowserCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(config,this->mBrowser,"Browser",__FILE__,27);
    SafeGetComponent(config,this->mBackButton,"BackButton",__FILE__,28);
    SafeGetComponent(config,this->mForwardButton,"ForwardButton",__FILE__,29);
    SafeGetComponent(config,this->mStopButton,"StopButton",__FILE__,30);
    SafeGetComponent(config,this->mHomeButton,"HomeButton",__FILE__,31);
    SafeGetComponent(config,this->mRefreshButton,"RefreshButton",__FILE__,32);
    SafeGetComponent(config,this->mFindInput,"FindInput",__FILE__,33);
    SafeGetComponent(config,this->mFindNextButton,"FindNextButton",__FILE__,34);
    SafeGetComponent(config,this->mFindPrevButton,"FindPrevButton",__FILE__,35);
    SafeGetComponent(config,this->mCaseSensitiveCheck,"CaseSensitiveCheck",__FILE__,36);
    SafeGetComponent(config,this->mStatusLabel,"StatusLabel",__FILE__,37);
    this->mBackButton->mControlId=2;
    this->mForwardButton->mControlId=3;
    this->mStopButton->mControlId=5;
    this->mRefreshButton->mControlId=1;
    this->mHomeButton->mControlId=4;
    this->mFindNextButton->mControlId=7;
    this->mFindPrevButton->mControlId=8;
    this->EnableButtons();
}
// Each button pointer is loaded before the browser is asked
// FUNCTION: WON 0x007bc250
// Enables back, forward and stop as the browser allows
void GameBrowserCtrl::EnableButtons()
{
    Button* button=this->mBackButton.get();
    button->SetEnabled(this->mBrowser->BackButtonEnabled());
    button=this->mForwardButton.get();
    button->SetEnabled(this->mBrowser->ForwardButtonEnabled());
    BrowserComponent* browser=this->mBrowser.get();
    Button* stop=this->mStopButton.get();
    stop->SetEnabled(browser->StopButtonEnabled());
}
// The check box, input and browser pointers are loaded before GetText
// FUNCTION: WON 0x007bc2b0
// Finds the typed text forwards or backwards in the page, case-sensitive if ticked
void GameBrowserCtrl::DoFind(bool inNext)
{
    Button* check=this->mCaseSensitiveCheck.get();
    InputBox* input=this->mFindInput.get();
    BrowserComponent* browser=this->mBrowser.get();
    browser->FindStr(input->GetText(),inNext,check->mChecked!=0);
}
// FUNCTION: WON 0x007bc2f0
// Clears the browser so the home page loads again on the next paint
void GameBrowserCtrl::ResetBrowser()
{
    this->mBrowser->Reset();
    this->mIsReset=true;
}
// FUNCTION: WON 0x007bc310
// Loads the home page once after a reset
void GameBrowserCtrl::GoHomeIfReset()
{
    if(this->mIsReset) {
        this->mIsReset=false;
        this->GoHome();
    }
}
// FUNCTION: WON 0x007bc330
// Loads the home page
void GameBrowserCtrl::GoHome()
{
    this->mBrowser->Go(this->mHomeLocation,false,0,-1);
}
// FUNCTION: WON 0x007bc350
// Handles browser state changes and the navigation and find buttons; every other event is dropped
void GameBrowserCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_BrowserButtonChange: this->EnableButtons(); return;
    case ComponentEvent_BrowserStatusChange: this->mStatusLabel->SetText(this->mBrowser->GetCurStatus(),true); return;
    case ComponentEvent_ButtonPressed:
        switch(inEvent->GetControlId()) {
        case 2: this->mBrowser->Back(); break;
        case 3: this->mBrowser->Forward(); break;
        case 5: this->mBrowser->Stop(); break;
        case 1: this->mBrowser->Refresh(); break;
        case 4: this->GoHome(); break;
        case 7: this->DoFind(true); break;
        case 8: this->DoFind(false); break;
        }
        return;
    }
}
// FUNCTION: WON 0x007bc430
// Loads the home page if the browser was reset, then draws the tab
void GameBrowserCtrl::Paint(Graphics& inGraphics)
{
    this->GoHomeIfReset();
    Container::Paint(inGraphics);
}
}

namespace WONAPI {
// FUNCTION: WON 0x00765460
// Creates a config for a browser tab in the layout file
ComponentConfig* GameBrowserCtrlConfig::CfgFactory()
{
    return new GameBrowserCtrlConfig;
}
}

#include "ee/wonapi/WONURL.h"
namespace WONAPI {
// FUNCTION: WON 0x007bc450
// Reads the HOMELOCATION layout key, resolving a file URL to the resource file path
bool GameBrowserCtrlConfig::Virtual24(const std::string& instruction)
{
    if(ContainerConfig::Virtual24(instruction))
        return true;
    if(instruction=="HOMELOCATION")
    {
        std::string path=this->ReadString(false);
        URL url=path;
        if(url.GetType()==URLType_File)
            path=ComponentConfig::GetResourceFile(path);
        this->mBrowser->SetHomeLocation(path);
    }
    else
        return false;
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bc6b0
// (public: __thiscall WONAPI::URL::~URL(void)    [??1URL@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007cdd20

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bbec0
// (public: virtual void * __thiscall WONAPI::GameBrowserCtrl::`scalar deleting destructor'(unsigned int)    [??_GGameBrowserCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007bbee0
// (public: virtual __thiscall WONAPI::GameBrowserCtrl::~GameBrowserCtrl(void)    [??1GameBrowserCtrl@WONAPI@@UAE@XZ])
