// The lobby's root screen: the tab control (lobby or LAN and direct connect, game, help, news) and the lower bar
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include <list>
#include <map>
#include <vector>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
namespace WONAPI {
// The original reloads mTabCtrl and its bottom tab bar for every call
// FUNCTION: WON 0x007c4880
// Enables the game tab and switches to it, or switches back to the previous tab and disables the game tab
void RootScreen::EnterGameTab(bool enter)
{
    if(enter)
    {
        this->mTabCtrl->EnableTab(this->mGameTab,true);
        if(this->mTabCtrl->mBottomTabBar->GetCurTabId()!=this->mGameTab)
        {
            this->mLastJoinTab=this->mTabCtrl->mBottomTabBar->GetCurTabId();
            this->mTabCtrl->SetCurTab(this->mGameTab,true);
        }
    }
    else
    {
        if(this->mTabCtrl->mBottomTabBar->GetCurTabId()==this->mGameTab)
            this->mTabCtrl->SetCurTab(this->mLastJoinTab,true);
        this->mTabCtrl->EnableTab(this->mGameTab,false);
    }
}
// FUNCTION: WON 0x007c4930
// Switches to the Internet lobby tab
void RootScreen::ShowLobbyTab(){this->mTabCtrl->SetCurTab(this->mLobbyTab,true);}
// FUNCTION: WON 0x007c4950
// Opens the game tab when the player has joined a game of this screen's kind (Internet or LAN)
void RootScreen::HandleJoinedGame()
{
    LobbyGame* aGame=GetMyLobbyGame();
    if(aGame!=NULL && aGame->MatchesRoomSpec(this->mRoomSpecFlags)) this->EnterGameTab(true);
}
// FUNCTION: WON 0x007c4980
// Leaves and disables the game tab after the player left the game
void RootScreen::HandleLeftGame(){this->EnterGameTab(false);}
// The banner height is a function-local static, read from the layout on the first call
// FUNCTION: WON 0x007c46d0
// Lays out the lower bar: hidden, sized for the cross-promotion banner, or for the logo and back button
void RootScreen::Virtual08()
{
    if(this->mLowerLayoutPoint.get()!=NULL)
    {
        if(this->mHideLowerControls)
        {
            this->mLowerDivider->SetVisible(false);
            this->mSierraLogo->SetVisible(false);
            this->mLowerBanner->SetVisible(false);
            this->mLowerBannerFrame->SetVisible(false);
            this->mBackButton->SetVisible(false);
            this->mLowerLayoutPoint->SetPos(this->mLowerLayoutPoint->Left(),this->Height());
            Container::Virtual08();
            return;
        }
        static int aBannerHeight=this->mLowerBanner->Height();
        this->mLowerDivider->SetVisible(true);
        this->mSierraLogo->SetVisible(true);
        this->mBackButton->SetVisible(true);
        int aLowerHeight=this->mLowerDivider->Height();
        if(LobbyGetCrossPromotion())
        {
            this->mLowerBannerFrame->SetVisible(true);
            this->mLowerBanner->SetVisible(true);
            this->mLowerBanner->SetSize(this->mLowerBanner->Width(),aBannerHeight);
            aLowerHeight+=aBannerHeight+2*this->mLowerBannerFrame->mThickness;
        }
        else
        {
            this->mLowerBannerFrame->SetVisible(false);
            this->mLowerBanner->SetVisible(false);
            int aHeight=this->mBackButton->Height();
            if(this->mSierraLogo->Height()>aHeight) aHeight=this->mSierraLogo->Height();
            aLowerHeight+=aHeight;
            this->mLowerBanner->SetSize(this->mLowerBanner->Width(),aHeight);
        }
        this->mLowerLayoutPoint->SetPos(this->mLowerLayoutPoint->Left(),this->Height()-(this->mLowerDivider->Height()-2)/2-aLowerHeight);
    }
    Container::Virtual08();
}
// A tab-selected event still goes on to Component::HandleComponentEvent after its handler
// FUNCTION: WON 0x007c4ae0
// Handles tab changes, the relayout button and entered-screen/joined-game/left-game events
void RootScreen::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType)
    {
    case ComponentEvent_TabCtrlTabSelected: this->HandleTabCtrlTabSelected(inEvent); break;
    case ComponentEvent_ButtonPressed:
        if(inEvent->GetControlId()==3) { this->Relayout(); return; }
        break;
    case 100014: this->HandleEnteredScreen(); return;
    case 100029: this->HandleJoinedGame(); return;
    case 100032: this->HandleLeftGame(); return;
    }
    Component::HandleComponentEvent(inEvent);
}
// The original sets these members in the constructor body, not in an initializer list
// FUNCTION: WON 0x007c3c00
// Creates the root screen for the Internet or the LAN lobby, with no tab ids known yet
RootScreen::RootScreen(bool inIsLan)
{
    this->mIsLan=inIsLan;
    if(inIsLan) this->mRoomSpecFlags=8;
    else this->mRoomSpecFlags=4;
    this->mLobbyTab=this->mLanTab=this->mDirectConnectTab=this->mLastJoinTab=-1;
    this->mHideLowerControls=false;
    this->mIsSwat=false;
}
// The original stores the class's own vtable, then destroys the members
// FUNCTION: WON 0x007c3d10
// Destroys the root screen (members only)
RootScreen::~RootScreen() {}
// FUNCTION: WON 0x007c3ec0
// Returns whether the LAN screen is showing its direct connect tab
bool RootScreen::OnDirectConnectTab()
{
    return this->mIsLan && this->mTabCtrl->mBottomTabBar->GetCurTabId()==this->mDirectConnectTab;
}
// FUNCTION: WON 0x007c3f00
// Returns whether the LAN screen is showing its LAN games tab
bool RootScreen::OnLanTab()
{
    return this->mIsLan && this->mTabCtrl->mBottomTabBar->GetCurTabId()==this->mLanTab;
}
// FUNCTION: WON 0x007c4990
// Resets the help and news browsers and selects the tab saved in the user's preferences
void RootScreen::HandleEnteredScreen()
{
    this->mHelpTab->ResetBrowser();
    if(this->mNewsTab.get()!=NULL) this->mNewsTab->ResetBrowser();
    TabCtrl* aTabs=this->mTabCtrl.get();
    aTabs->SetCurTabByName(LobbyGetTabSelect(),true);
}

// FUNCTION: WON 0x007c3f40
// Finds the screen's tabs and lower bar controls in the layout and tells the children the room kind
void RootScreen::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    this->mIsSwat=aConfig->Fn386860("SWATUJDEF")!=NULL;
    SafeGetComponent(aConfig,this->mTabCtrl,"TabCtrl",__FILE__,79);
    SafeGetComponent(aConfig,this->mLowerLayoutPoint,"LowerLayoutPoint",__FILE__,80);
    SafeGetComponent(aConfig,this->mLowerDivider,"LowerDivider",__FILE__,81);
    SafeGetComponent(aConfig,this->mSierraLogo,"SierraLogo",__FILE__,82);
    SafeGetComponent(aConfig,this->mLowerBanner,"LowerBanner",__FILE__,83);
    SafeGetComponent(aConfig,this->mLowerBannerFrame,"LowerBannerFrame",__FILE__,84);
    SafeGetComponent(aConfig,this->mBackButton,"BackButton",__FILE__,85);
    SafeGetComponent(aConfig,this->mHelpTab,"HelpTab",__FILE__,86);
    SmartPtr<Container> aTab;
    SafeGetComponent(aConfig,aTab,"GameTab",__FILE__,89);
    this->mGameTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
    this->mTabCtrl->EnableTab(this->mGameTab,false);
    if(!this->mIsLan)
    {
        SafeGetComponent(aConfig,this->mNewsTab,"NewsTab",__FILE__,95);
        SafeGetComponent(aConfig,aTab,"LobbyTab",__FILE__,96);
        this->mLobbyTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
        this->BroadcastEventDown(new SetRoomSpecFlagsEvent(4));
    }
    else
    {
        SafeGetComponent(aConfig,aTab,"LanTab",__FILE__,101);
        this->mLanTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
        SafeGetComponent(aConfig,aTab,"DirectConnectTab",__FILE__,102);
        this->mDirectConnectTab=this->mTabCtrl->GetTabIdForContainer(aTab.get());
        this->BroadcastEventDown(new SetRoomSpecFlagsEvent(8));
    }
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007c49d0
// Saves the user's preferences on a tab change; in SWAT mode shrinks the window on tab 2 and restores it
void RootScreen::HandleTabCtrlTabSelected(ComponentEvent* inEvent)
{
    if(inEvent->mComponent!=this->mTabCtrl.get()) return;
    if(this->mIsSwat)
    {
        static bool aResized=false;
        static int aOldHeight;
        Window* aWindow=this->GetRoot();
        WONRectangle aWindowRect;
        WONRectangle aClientRect;
        aWindow->GetRect(aWindowRect,false);
        aWindow->GetRect(aClientRect,true);
        int aWindowHeight=aWindowRect.height;
        int aClientHeight=aClientRect.height;
        if(this->mTabCtrl->mBottomTabBar->GetCurTabId()==2)
        {
            this->mHideLowerControls=true;
            aOldHeight=aWindowRect.height;
            aResized=true;
            aWindow->SetPosSize(aWindowRect.x,aWindowRect.y,aWindowRect.width,
                aWindowHeight-aClientHeight+150);
        }
        else if(aResized)
        {
            this->mHideLowerControls=false;
            aResized=false;
            aWindow->SetPosSize(aWindowRect.x,aWindowRect.y,aWindowRect.width,aOldHeight);
        }
        this->Relayout();
    }
    LobbySaveUserPrefs(false);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c3cf0
// (public: virtual void * __thiscall WONAPI::RootScreen::`scalar deleting destructor'(unsigned int)    [??_GRootScreen@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c4620
// (public: __thiscall WONAPI::SetRoomSpecFlagsEvent::SetRoomSpecFlagsEvent(int))
// TEMPLATE: WON 0x007c4650
// (public: virtual void * __thiscall WONAPI::SetRoomSpecFlagsEvent::`scalar deleting destructor'(unsigned int)    [??_GSetRoomSpecFlagsEvent@WONAPI@@UAEP)
// TEMPLATE: WON 0x007c4670
// (public: virtual __thiscall WONAPI::SetRoomSpecFlagsEvent::~SetRoomSpecFlagsEvent(void)    [??1SetRoomSpecFlagsEvent@WONAPI@@UAE@XZ])
