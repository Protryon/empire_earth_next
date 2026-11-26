// The lobby's root screen: the tab control (lobby or LAN and direct connect, game, help, news) and the lower bar.
// Its bodies are Rust (ee_won_lobby rootscreen.rs, R6 WON round 32): each method one forward, each statement one
// case of RootHost.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <string>
#include <list>
#include <map>
#include <vector>
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_lobby/rootscreen.h"
namespace WONAPI {

typedef EeWonRootScreenOp RO;
typedef EeWonRootScreenCall RC;

// InitComponent's local tab and the SWAT resize's window and rectangles (the original's locals)
static SmartPtr<Container> sRootTab;
static WONRectangle sRootWindowRect;
static WONRectangle sRootClientRect;
static Window* sRootWindow;

static Component* RootPart(RootScreen* s, intptr_t inPart)
{
    switch ((EeWonRootScreenPart)inPart) {
    case EeWonRootScreenPart::LowerLayoutPoint: return s->mLowerLayoutPoint.get();
    case EeWonRootScreenPart::LowerDivider: return s->mLowerDivider.get();
    case EeWonRootScreenPart::SierraLogo: return s->mSierraLogo.get();
    case EeWonRootScreenPart::LowerBanner: return s->mLowerBanner.get();
    case EeWonRootScreenPart::LowerBannerFrame: return s->mLowerBannerFrame.get();
    case EeWonRootScreenPart::BackButton: return s->mBackButton.get();
    }
    return NULL;
}

static intptr_t RootField(RootScreen* s, intptr_t inField, bool inSet, intptr_t v)
{
    typedef EeWonRootScreenField F;
    switch ((F)inField) {
    case F::IsLan: if (inSet) s->mIsLan = v != 0; return s->mIsLan;
    case F::HideLowerControls: if (inSet) s->mHideLowerControls = v != 0; return s->mHideLowerControls;
    case F::IsSwat: if (inSet) s->mIsSwat = v != 0; return s->mIsSwat;
    case F::LastJoinTab: if (inSet) s->mLastJoinTab = (int)v; return s->mLastJoinTab;
    case F::GameTab: if (inSet) s->mGameTab = (int)v; return s->mGameTab;
    case F::DirectConnectTab: if (inSet) s->mDirectConnectTab = (int)v; return s->mDirectConnectTab;
    case F::LanTab: if (inSet) s->mLanTab = (int)v; return s->mLanTab;
    case F::LobbyTab: if (inSet) s->mLobbyTab = (int)v; return s->mLobbyTab;
    case F::RoomSpecFlags: if (inSet) s->mRoomSpecFlags = (int)v; return s->mRoomSpecFlags;
    }
    return 0;
}

static intptr_t RootHost(void* inScreen, EeWonRootScreenOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    RootScreen* s = static_cast<RootScreen*>(inScreen);
    ComponentConfig* aConfig = reinterpret_cast<ComponentConfig*>(a);
    switch (inOp) {
    case RO::FieldGet: return RootField(s, a, false, 0);
    case RO::FieldSet: return RootField(s, a, true, b);
    case RO::TabCurId: return s->mTabCtrl->GetCurTabId();
    case RO::TabEnable: s->mTabCtrl->EnableTab((int)a, b != 0); return 0;
    case RO::TabSetCur: s->mTabCtrl->SetCurTab((int)a, true); return 0;
    case RO::TabSetCurByPref: s->mTabCtrl->SetCurTabByName(LobbyGetTabSelect(), true); return 0;
    case RO::TabIdOfFound: return s->mTabCtrl->GetTabIdForContainer(sRootTab.get());
    case RO::TabFoundClear: sRootTab = NULL; return 0;
    case RO::PartExists: return RootPart(s, a) != NULL;
    case RO::PartSetVisible: RootPart(s, a)->SetVisible(b != 0); return 0;
    case RO::PartHeight: return RootPart(s, a)->Height();
    case RO::PartWidth: return RootPart(s, a)->Width();
    case RO::PartLeft: return RootPart(s, a)->Left();
    case RO::PartSetPos: RootPart(s, a)->SetPos((int)b, (int)c); return 0;
    case RO::PartSetSize: RootPart(s, a)->SetSize((int)b, (int)c); return 0;
    case RO::FrameThickness: return s->mLowerBannerFrame->mThickness;
    case RO::BaseSizeChanged: s->Container::Virtual08(); return 0;
    case RO::Height: return s->Height();
    case RO::Relayout: s->Relayout(); return 0;
    case RO::BaseEvent: s->Component::HandleComponentEvent(reinterpret_cast<ComponentEvent*>(a)); return 0;
    case RO::EventControlId: return reinterpret_cast<ComponentEvent*>(a)->GetControlId();
    case RO::EventFromTabs: return reinterpret_cast<ComponentEvent*>(a)->mComponent == s->mTabCtrl.get();
    case RO::MyGame: return (intptr_t)GetMyLobbyGame();
    case RO::GameMatches: return reinterpret_cast<LobbyGame*>(a)->MatchesRoomSpec((int)b) ? 1 : 0;
    case RO::CrossPromotion: return LobbyGetCrossPromotion() ? 1 : 0;
    case RO::SaveUserPrefs: LobbySaveUserPrefs(false); return 0;
    case RO::HelpReset: s->mHelpTab->ResetBrowser(); return 0;
    case RO::NewsExists: return s->mNewsTab.get() != NULL;
    case RO::NewsReset: s->mNewsTab->ResetBrowser(); return 0;
    case RO::InitType: return reinterpret_cast<ComponentInit*>(a)->mType;
    case RO::InitConfig: return (intptr_t) static_cast<ComponentConfigInit*>(reinterpret_cast<ComponentInit*>(a))->mConfig;
    case RO::ConfigHasSwat: return aConfig->Fn386860("SWATUJDEF") != NULL;
    case RO::FindTabCtrl: SafeGetComponent(aConfig, s->mTabCtrl, "TabCtrl", __FILE__, 79); return 0;
    case RO::FindLowerLayoutPoint: SafeGetComponent(aConfig, s->mLowerLayoutPoint, "LowerLayoutPoint", __FILE__, 80); return 0;
    case RO::FindLowerDivider: SafeGetShell(aConfig, s->mLowerDivider, EeWonShellKind::MsSeperator, "LowerDivider", __FILE__, 81); return 0;
    case RO::FindSierraLogo: SafeGetComponent(aConfig, s->mSierraLogo, "SierraLogo", __FILE__, 82); return 0;
    case RO::FindLowerBanner: SafeGetComponent(aConfig, s->mLowerBanner, "LowerBanner", __FILE__, 83); return 0;
    case RO::FindLowerBannerFrame: SafeGetComponent(aConfig, s->mLowerBannerFrame, "LowerBannerFrame", __FILE__, 84); return 0;
    case RO::FindBackButton: SafeGetComponent(aConfig, s->mBackButton, "BackButton", __FILE__, 85); return 0;
    case RO::FindHelpTab: SafeGetComponent(aConfig, s->mHelpTab, "HelpTab", __FILE__, 86); return 0;
    case RO::FindGameTab: SafeGetComponent(aConfig, sRootTab, "GameTab", __FILE__, 89); return 0;
    case RO::FindNewsTab: SafeGetComponent(aConfig, s->mNewsTab, "NewsTab", __FILE__, 95); return 0;
    case RO::FindLobbyTab: SafeGetComponent(aConfig, sRootTab, "LobbyTab", __FILE__, 96); return 0;
    case RO::FindLanTab: SafeGetComponent(aConfig, sRootTab, "LanTab", __FILE__, 101); return 0;
    case RO::FindDirectConnectTab: SafeGetComponent(aConfig, sRootTab, "DirectConnectTab", __FILE__, 102); return 0;
    case RO::BroadcastRoomSpec: s->BroadcastEventDown(SetRoomSpecFlagsEvent((int)a)); return 0;
    case RO::Subscribe: s->SubscribeToBroadcast(1); return 0;
    case RO::WindowGet: sRootWindow = s->GetRoot(); return 0;
    case RO::WindowRectGet: sRootWindow->GetRect(sRootWindowRect, false); return 0;
    case RO::ClientRectGet: sRootWindow->GetRect(sRootClientRect, true); return 0;
    case RO::WindowHeight: return sRootWindowRect.height;
    case RO::ClientHeight: return sRootClientRect.height;
    case RO::WindowSetHeight: sRootWindow->SetPosSize(sRootWindowRect.x, sRootWindowRect.y, sRootWindowRect.width, (int)a); return 0;
    }
    return 0;
}

static intptr_t RootCall(RootScreen* s, EeWonRootScreenCall inCall, intptr_t a = 0, intptr_t b = 0) { return ee_won_rootscreen_call(s, RootHost, inCall, a, b); }

void RootScreen::EnterGameTab(bool enter) { RootCall(this, RC::EnterGameTab, enter); }
void RootScreen::ShowLobbyTab() { RootCall(this, RC::ShowLobbyTab); }
void RootScreen::HandleJoinedGame() { RootCall(this, RC::JoinedGame); }
void RootScreen::HandleLeftGame() { RootCall(this, RC::LeftGame); }
void RootScreen::Virtual08() { RootCall(this, RC::SizeChanged); }
void RootScreen::HandleComponentEvent(ComponentEvent* inEvent) { RootCall(this, RC::Event, (intptr_t)inEvent, inEvent->mType); }
RootScreen::RootScreen(bool inIsLan) { RootCall(this, RC::Construct, inIsLan); }
// Destroys the root screen (members only)
RootScreen::~RootScreen() {}
bool RootScreen::OnDirectConnectTab() { return RootCall(this, RC::OnDirectConnectTab) != 0; }
bool RootScreen::OnLanTab() { return RootCall(this, RC::OnLanTab) != 0; }
void RootScreen::HandleEnteredScreen() { RootCall(this, RC::EnteredScreen); }
void RootScreen::InitComponent(ComponentInit& inInit) { RootCall(this, RC::InitComponent, (intptr_t)&inInit); }
void RootScreen::HandleTabCtrlTabSelected(ComponentEvent* inEvent) { RootCall(this, RC::TabSelected, (intptr_t)inEvent); }

}
