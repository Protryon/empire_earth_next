// The lobby options tab: sounds, music, confirmations, connection speed, filters and network address details
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233

#include <windows.h>
#include <stdio.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyControls.h"
namespace WONAPI {
// FUNCTION: WON 0x007be930
// Returns the connection speed value (5, 15, 25, 35, 45 or 55) for a speed combo box entry
static int GetConnectionSpeed(int inSelection)
{
    if (inSelection<0) return 5;
    switch (inSelection) {
    case 0:return 5;
    case 1:return 15;
    case 2:return 25;
    case 3:return 35;
    case 4:return 45;
    default:return 55;
    }
}
// FUNCTION: WON 0x007bef30
// Returns the speed combo box entry for a saved connection speed value
static int GetConnectionSpeedIndex(int inSpeed)
{
    int i;
    for (i=1;i<6;i++)
        if (GetConnectionSpeed(i)>inSpeed) break;
    return i-1;
}
// FUNCTION: WON 0x007bdfc0
// Creates the options tab for the Internet or the LAN lobby
LobbyOptionsCtrl::LobbyOptionsCtrl(bool inIsLan) { this->mIsLan = inIsLan; }
// The destructor (0x7be070) releases the 18 control pointers in reverse order, then the container base
LobbyOptionsCtrl::~LobbyOptionsCtrl() {} // the original stores its own vtable at 0x7be08f
// One local button pointer holds the reset, restore and remove buttons in turn
// FUNCTION: WON 0x007be260
// Finds the option controls in the layout (LAN or Internet ones) and shows the local address
void LobbyOptionsCtrl::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType!=2)
        return;
    ComponentConfig* config=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SmartPtr<Button> button;
    SafeGetComponent(config,this->mChatSoundEffectsCheck,"ChatSoundEffectsCheck",2,__FILE__,95);
    SafeGetComponent(config,this->mConnectionSpeedCombo,"ConnectionSpeedCombo",__FILE__,96);
    SafeGetComponent(config,this->mCrossPromotionCheck,"CrossPromotionCheck",3,__FILE__,97);
    SafeGetComponent(config,this->mFirewalledIcon,"FirewalledIcon",__FILE__,98);
    SafeGetComponent(config,this->mLobbyMusicCheck,"LobbyMusicCheck",5,__FILE__,99);
    SafeGetComponent(config,this->mLobbySoundEffectsCheck,"LobbySoundEffectsCheck",6,__FILE__,100);
    SafeGetComponent(config,this->mLocalAddressDetailLabel,"LocalAddressDetailLabel",__FILE__,101);
    SafeGetComponent(config,this->mPublicAddressDetailLabel,"PublicAddressDetailLabel",__FILE__,102);
    SafeGetComponent(config,button,"ResetFiltersButton",8,__FILE__,103);
    SafeGetComponent(config,button,"RestoreDefaultsButton",9,__FILE__,104);
    SafeGetComponent(config,this->mShowConfirmationsCheck,"ShowConfirmationsCheck",10,__FILE__,105);
    SafeGetComponent(config,this->mShowGamesInProgressCheck,"ShowGamesInProgressCheck",11,__FILE__,106);
    SafeGetComponent(config,this->mStagingSoundEffectsCheck,"StagingSoundEffectsCheck",12,__FILE__,107);
    SafeGetComponent(config,this->mTabSelectCombo,"TabSelectCombo",__FILE__,108);
    if (this->mIsLan) {
        SafeGetComponent(config,this->mLANBroadcastDetailLabel,"LANBroadcastDetailLabel",__FILE__,112);
        SafeGetComponent(config,button,"RemoveLanNameButton",7,__FILE__,113);
        SafeGetComponent(config,this->mUserNameCombo,"UserNameCombo",__FILE__,114);
    } else {
        SafeGetComponent(config,this->mAnonymousToFriendsCheck,"AnonymousToFriendsCheck",0x30d56,__FILE__,118);
        SafeGetComponent(config,this->mAutoPingGamesCheck,"AutoPingGamesCheck",1,__FILE__,119);
        SafeGetComponent(config,this->mDoAutoAFKCheck,"DoAutoAFKCheck",4,__FILE__,120);
        SafeGetComponent(config,this->mFilterDirtyWordsCheck,"FilterDirtyWordsCheck",0x30d55,__FILE__,121);
    }
    this->mLocalAddressDetailLabel->SetText(IPAddr::GetLocalAddr().GetHostString(true),true);
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x007be840
// Saves tab, speed and LAN name choices, handles option buttons and refreshes on lobby events
void LobbyOptionsCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch(inEvent->mType) {
    case ComponentEvent_ComboSelChanged:
        if (inEvent->mComponent==this->mTabSelectCombo.get()) {
            LobbySetTabSelect(this->mTabSelectCombo->mEdit->GetText());return;
        } else if(inEvent->mComponent==this->mConnectionSpeedCombo.get()) {
            LobbySetConnectionSpeed(GetConnectionSpeed(this->mConnectionSpeedCombo->GetSelItem()));return;
        } else if(inEvent->mComponent==this->mUserNameCombo.get()) {
            LobbySetLanName(this->mUserNameCombo->mEdit->GetText());return;
        }
        break;
    case ComponentEvent_ButtonPressed:
        if(this->HandleButton(inEvent->mComponent)) return;
        break;
    case 100014: this->SyncOptions();return;
    case 100050: this->RefreshLanOptions();return;
    case 100055: this->RefreshPublicAddress();return;
    }
    Component::HandleComponentEvent(inEvent);
}
// The saved LAN names are a case-insensitive map<GUIString,long>
// FUNCTION: WON 0x007bec80
// Refills the LAN name combo box from the saved names and shows the LAN name and broadcast port
void LobbyOptionsCtrl::RefreshLanOptions() {
    if(!this->mIsLan)
        return;
    this->mUserNameCombo->Clear(true);
    const LobbyLanNameInfoMap& names=LobbyFn35D660();
    LobbyLanNameInfoMap::const_iterator i=names.begin();
    while(i!=names.end()) {
        this->mUserNameCombo->AddString(i->first,-1);
        ++i;
    }
    ComboBox* combo=this->mUserNameCombo.get(); // the original keeps the combo box in esi (0x7bed24) before calling the getter
    combo->SetTextAndSel(LobbyGetLanName(),true);
    char port[20]; // a 20-byte buffer, as in the original
    sprintf(port,"%d",LobbyGetLanBroadcastPort());
    this->mLANBroadcastDetailLabel->SetText(port,true);
}
// The notify flag differs per control as in the original (dirty-word and auto-AFK checks don't notify)
// FUNCTION: WON 0x007bedf0
// Sets every option control from the saved user preferences
void LobbyOptionsCtrl::RefreshOptions()
{
    if(this->mIsLan) this->RefreshLanOptions();
    else {
        this->mFilterDirtyWordsCheck->SetCheck(LobbyGetFilterDirtyWords(),false);
        this->mDoAutoAFKCheck->SetCheck(LobbyGetDoAutoAFK(),false);
        this->mAnonymousToFriendsCheck->SetCheck(LobbyGetAnonymousToFriends(),true);
        this->mAutoPingGamesCheck->SetCheck(LobbyGetAutoPingGames(),true);
    }
    this->mChatSoundEffectsCheck->SetCheck(LobbyGetChatSoundEffects(),true);
    this->mConnectionSpeedCombo->SetSelItem(GetConnectionSpeedIndex(LobbyGetConnectionSpeed()),false);
    this->mTabSelectCombo->SetTextAndSel(LobbyGetTabSelect(),false);
    this->mLobbyMusicCheck->SetCheck(LobbyGetLobbyMusic(),true);
    this->mLobbySoundEffectsCheck->SetCheck(LobbyGetLobbySoundEffects(),true);
    this->mShowConfirmationsCheck->SetCheck(LobbyGetShowConfirmations(),true);
    this->mCrossPromotionCheck->SetCheck(LobbyGetCrossPromotion(),true);
    this->mShowGamesInProgressCheck->SetCheck(LobbyGetShowGamesInProgress(),true);
    this->mStagingSoundEffectsCheck->SetCheck(LobbyGetStagingSoundEffects(),true);
}
//3bef60,34B.
// FUNCTION: WON 0x007bef60
// Refreshes the options when this tab's screen (Internet or LAN) is showing and not the login screen
void LobbyOptionsCtrl::SyncOptions()
{
    if(!LobbyOnLoginScreen() && LobbyOnLanScreen()==this->mIsLan) this->RefreshOptions();
}

}
namespace WONAPI {
// FUNCTION: WON 0x007be980
// Saves a toggled option and applies it (music, sounds, reset filters, restore defaults, remove LAN name);
// returns false for cross promotion, dirty-word filter and anonymous-to-friends, so the base sees them too
bool LobbyOptionsCtrl::HandleButton(Component* inComponent)
{
    if(inComponent==NULL) return false;
    Button* button=dynamic_cast<Button*>(inComponent);
    if(button==NULL) return false;
    bool checked=button->mChecked!=0;
    switch(inComponent->mControlId) {
    case 3:LobbySetCrossPromotion(checked);break;
    case 4:LobbySetDoAutoAFK(checked);return true;
    case 1:LobbySetAutoPingGames(checked);return true;
    case 11:
        LobbySetShowGamesInProgress(checked);
        LobbyBroadcastLobbyEvent(100059,true);
        return true;
    case 9:
        LobbyRestoreDefaults();
        LobbySetNetworkAdapter(LobbyGetNetworkAdapter());
        this->RefreshOptions();
        return true;
    case 8: {
        LobbyResetFilters();
        Component* screen=LobbyGetCurrentScreen();
        if(screen!=NULL) screen->BroadcastEventDown(new LobbyEvent(100034));
        return true;
    }
    case 5:
        LobbySetLobbyMusic(checked);
        if(checked) ::LobbyGlobal_LobbyMusic_Sound->Resume();
        else ::LobbyGlobal_LobbyMusic_Sound->Stop();
        return true;
    case 6:
        LobbySetLobbySoundEffects(checked);
        ::LobbyGlobal_ButtonClick_Sound->SetMuted(!checked);
        if(!checked) ::LobbyGlobal_SierraLogon_Sound->Stop();
        return true;
    case 2:LobbySetChatSoundEffects(checked);return true;
    case 12:LobbySetStagingSoundEffects(checked);return true;
    case 10:LobbySetShowConfirmations(checked);return true;
    case 7:LobbyRemoveLanName();this->RefreshLanOptions();return true;
    case 0x30d55:LobbySetFilterDirtyWords(checked);break;
    case 0x30d56:LobbySetAnonymousToFriends(checked);break;
    }
    return false;
}

// FUNCTION: WON 0x007bef90
// Shows the firewall icon (searching, firewalled or hidden) and the public address or its status text
void LobbyOptionsCtrl::RefreshPublicAddress()
{
    if(LobbyIsFirewallTestDone()) {
        if(LobbyIsBehindFirewall()) {
            this->mFirewalledIcon->SetImage(::LobbyGlobal_Firewall_Image.get());
            this->mFirewalledIcon->Invalidate();
            this->mFirewalledIcon->SetVisible(true);
        } else this->mFirewalledIcon->SetVisible(false);
    } else {
        this->mFirewalledIcon->SetImage(::LobbyGlobal_Searching_Image.get());
        this->mFirewalledIcon->Invalidate();
        this->mFirewalledIcon->SetVisible(true);
    }
    IPAddr address=LobbyGetPublicAddress();
    if(!address.IsValid()) {
        if(!LobbyIsFirewallTestDone())
            this->mPublicAddressDetailLabel->SetText(::LobbyOptions_CheckingAddress_String,true);
        else this->mPublicAddressDetailLabel->SetText(::LobbyOptions_AddressUnavailable_String,true);
    } else {
        this->mPublicAddressDetailLabel->SetText(address.GetHostString(true),true);
    }
}
} //namespace WONAPI


// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ce1a0
// (void __cdecl WONAPI::SafeGetComponent(class WONAPI::ComponentConfig *,class WONAPI::SmartPtr<class WONAPI::Button> &,char const *,int,char const *,int)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007be050
// (public: virtual void * __thiscall WONAPI::LobbyOptionsCtrl::`scalar deleting destructor'(unsigned int)    [??_GLobbyOptionsCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007be070
// (public: virtual __thiscall WONAPI::LobbyOptionsCtrl::~LobbyOptionsCtrl(void)    [??1LobbyOptionsCtrl@WONAPI@@UAE@XZ])
