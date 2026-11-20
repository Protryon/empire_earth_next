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
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "ee/wonapi/WONLobbyDialogLogic.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyPing.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONCRC16.h"
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/WONStatus.h"
using namespace WONAPI;


// The resource table's factories: a lobby control kind (the one-line subclasses LoginStatusCtrl, CreateAccountCtrl,
// ... were these shells made with their kinds), a root screen, a dialog class
template <EeWonLobbyCtlKind K> static Component* CtlFactory() { return new LobbyRustCtrl(K); }
template <EeWonLobbyCtlKind K> static Component* DlgCtlFactory() { return new LobbyRustDialogCtrl(K); }
template <bool Lan> static Component* ScreenFactory() { return new RootScreen(Lan); }
typedef EeWonLobbyCtlKind Ck;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Builds the lobby's resource config table: the base GUI and browser types plus every lobby screen, control,
// dialog and button type the _Lobby*.cfg files can name
static ResourceConfigTablePtr GetLobbyResourceConfigTable(ResourceConfigTable *theTable)
{
	ResourceConfigTablePtr aTable = LobbyResource_Init(theTable);
	WONGUIConfig_Init(aTable.get());	
	BrowserComponentConfig::Init(aTable.get());

	aTable->AddAllocator("ChatCtrl",NewConfigOf<ChatCtrlConfig>,NewComponentOf<ChatCtrl>);
	aTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>);
	aTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>);
	aTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>);
	aTable->AddAllocator("GameBrowserCtrl",NewConfigOf<GameBrowserCtrlConfig>,NewComponentOf<GameBrowserCtrl>);
	aTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>);
	aTable->AddAllocator("GameUserListCtrl",NewConfigOf<GameUserListCtrlConfig>,GameUserListCtrlConfig::CompFactory);
	aTable->AddAllocator("InitFailedCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::InitFailed>);
	aTable->AddAllocator("InternetOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::NetOptions>);
	aTable->AddAllocator("InternetScreen",NewConfigOf<ContainerConfig>,ScreenFactory<false>);
	aTable->AddAllocator("LanOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LanOptions>);
	aTable->AddAllocator("LanScreen",NewConfigOf<ContainerConfig>,ScreenFactory<true>);
	aTable->AddAllocator("LoginCtrl",NewConfigOf<DialogConfig>,DlgCtlFactory<Ck::Login>);
	aTable->AddAllocator("LoginHelpCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<LoginHelpCtrl>);	
	aTable->AddAllocator("LoginScreen",NewConfigOf<ContainerConfig>,NewComponentOf<LoginScreen>);
	aTable->AddAllocator("LoginStatusCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LoginStatus>);
	aTable->AddAllocator("LostPasswordCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostPassword>);
	aTable->AddAllocator("LostUserNameCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostUserName>);
	aTable->AddAllocator("MOTDCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<MOTDCtrl>);
	aTable->AddAllocator("QuickPlayCtrl",NewConfigOf<DialogConfig>,DlgCtlFactory<Ck::QuickPlay>);
	aTable->AddAllocator("RoomCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Room>);
	aTable->AddAllocator("RoomLabelCtrl",NewConfigOf<RoomLabelCtrlConfig>,NewComponentOf<RoomLabelCtrl>);
	aTable->AddAllocator("ServerListCtrl",NewConfigOf<ServerListCtrlConfig>,NewComponentOf<ServerListCtrl>);
	aTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>);
	aTable->AddAllocator("UserListCtrl",NewConfigOf<UserListCtrlConfig>,UserListCtrlConfig::CompFactory);

	aTable->AddAllocator("AddFriendDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>);
	aTable->AddAllocator("AdminActionDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>);
	aTable->AddAllocator("BadUserDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>);
	aTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>);
	aTable->AddAllocator("CreditsDialog",NewConfigOf<CreditsDialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>);
	aTable->AddAllocator("CreateGameDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::CreateGame>);
	aTable->AddAllocator("CreateRoomDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::CreateRoom>);
	aTable->AddAllocator("DirectConnectDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::DirectConnect>);
	aTable->AddAllocator("LanNameDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::LanName>);
	aTable->AddAllocator("LanPortDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::LanPort>);
	aTable->AddAllocator("OkCancelDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::OkCancel>);
	aTable->AddAllocator("PasswordDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Password>);
	aTable->AddAllocator("RoomDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Room>);
	aTable->AddAllocator("StatusDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Status>);
	aTable->AddAllocator("TOUDialog",NewConfigOf<DialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Tou>);


	aTable->AddAllocator("CrossPromotionButton",NewConfigOf<CrossPromotionButtonConfig>,NewComponentOf<CrossPromotionButton>);
	aTable->AddAllocator("ImageTextButton",NewConfigOf<ImageTextButtonConfig>,NewComponentOf<ImageTextButton>);
	aTable->AddAllocator("MaxSplitterButton",NewConfigOf<MaxSplitterButtonConfig>,MaxSplitterButtonConfig::CompFactory);

	aTable->AddAllocator("GameOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>);
	aTable->AddAllocator("PlayerOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>);

	return aTable;
}


namespace WONAPI {

// Starts the lobby: parses WONLobby.cfg, builds the lobby screens from _LobbyMaster.cfg, inits the logic objects
// and derives the internet and LAN product ids; on failure stores the reason in mInitError and returns false
bool WONLobbyPrv::Init(ResourceConfigTable* input,void (*initFunc)(ComponentConfig*))
{
    this->mInitError=GetEmptyGUIStringData();
    ConfigParser parser;
    parser.SetWarningsAsErrors(true);
    if(!parser.OpenFile(WONAPICore::GetDefaultFileDirectory()+LobbyConfig::mDefaultFileName)) {
        this->mInitError=GUIString("Couldn't find "+LobbyConfig::mDefaultFileName);
        return false;
    }
    if(!this->mConfig->ParseTopLevel(parser,NULL)) {
        this->mInitError=GUIString(LobbyConfig::mDefaultFileName+" error: ");
        this->mInitError.append(GUIString(parser.GetAbortReason()));
        return false;
    }
    this->mLobbyPort=this->mConfig->mLobbyPort;
    SmartPtr<ContainerConfig> componentConfig=new ContainerConfig;
    this->mLobbyContainer=new LobbyContainer;
    ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory()+this->mConfig->mResourceDirectory);
    SmartPtr<ResourceConfigTable> resources=GetLobbyResourceConfigTable(input);
    ee_won_lobbyctl_init_columns();
    std::map<std::string,std::string,StringLessNoCase>::iterator itr=this->mConfig->mDefines.begin();
    while(itr!=this->mConfig->mDefines.end()) {
        parser.AddDefine(itr->first,itr->second,false);
        ++itr;
    }
    if(!componentConfig->ReadConfig_Unsafe(this->mLobbyContainer.get(),"_LobbyMaster.cfg",resources.get(),&parser)) {
        this->mInitError=componentConfig->mConfigError;
        return false;
    }
    try {
        this->mLogicMgr.Init(componentConfig.get());
        if(initFunc!=NULL) initFunc(componentConfig.get());
    }
    catch(ConfigObjectException& error) {
        this->mInitError=GUIString(error.mText);
        return false;
    }
    CRC16 crc;
    crc.Put(this->mConfig->mProductName.data(),this->mConfig->mProductName.length());
    crc.Put(this->mConfig->mVersion.data(),this->mConfig->mVersion.length());
    this->mProductId=crc.Get();
    crc.Reset();
    crc.Put(this->mConfig->mProductName.data(),this->mConfig->mProductName.length());
    crc.Put(this->mConfig->mLanVersion.data(),this->mConfig->mLanVersion.length());
    this->mLanProductId=crc.Get();
    this->mChatCommandParser->InitCommands();
    return true;
}

// Shuts the lobby down: stops the logic objects and releases the UDP manager and the auth context
void WONLobbyPrv::Destroy()
{
    this->mLogicMgr.Kill();
    if(this->mUDPManager.get()!=NULL) {
        this->mUDPManager->Close();
        this->mUDPManager=NULL;
    }
    if(this->mAuthContext.get()!=NULL) {
        this->mAuthContext->Kill();
        this->mAuthContext=NULL;
    }
}

// Sets the login community and the list of files to hash for authentication; with CD key checks on, loads
// the product's CD key from the registry into the auth context
void WONLobbyPrv::InitAuth(const std::wstring& loginCommunity,const char* hashFileListStr)
{
    this->mHasSetHashFileList=false;
    this->mHashFileList.clear();
    if(hashFileListStr!=NULL) {
        StringParser parser(hashFileListStr);
        parser.ReadValue(this->mHashFileList);
    }
    this->mLoginCommunity=loginCommunity;
    this->mAuthContext->SetCommunity(this->mLoginCommunity);
    if(!this->mConfig->mCDKeyCheck)
        return;
    this->mCDKey.SetProductString(this->mConfig->mProductName);
    this->mCDKey.LoadFromRegistry();
    this->mAuthContext->SetCDKey(this->mLoginCommunity,this->mCDKey);
}


// Members not set here are left to their own constructors, as in the original
// Creates the lobby's private state: configuration, saved settings, server contexts, auth context and the
// account, dialog, staging, init, LAN, routing and ping logic objects (port 8888)
WONLobbyPrv::WONLobbyPrv()
{
    this->mLobbyPort=8888;
    this->mSysTOUTime=this->mGameTOUTime=0;
    this->mBehindFirewall=false;
    this->mFirewallTestDone=false;
    this->mSuspended=false;
    this->mCurGameType=(LobbyGameType)0;
    this->mLobbyStatus=(LobbyStatus)0;
    this->mRunMode=(LobbyRunMode)1;
    this->mProductId=0;
    this->mLanProductId=0;

    this->mSysTOUPath=WONAPICore::GetDefaultFileDirectory()+"_wonsystou_m.txt";
    this->mGameTOUPath=WONAPICore::GetDefaultFileDirectory()+"_wongametou_m.txt";

    this->mConfig=LobbyConfig::mAllocator();
    this->mPersistentData=new LobbyPersistentData;
    this->mChatCommandParser=new ChatCommandParser;
    this->mUDPManager=new UDPManager;
    this->mAccountServers=new ServerContext;
    this->mAuthServers=new ServerContext;
    this->mDirServers=new ServerContext;
    this->mRoomDirServers=new ServerContext;
    this->mFirewallDetectServers=new ServerContext;
    this->mPatchServers=new ServerContext;
    this->mVersionStatus=WS_None;
    this->mAuthContext=new AuthContext;
    this->mAuthContext->SetServerContext(this->mAuthServers.get());

    this->mAccountLogic=new AccountLogic;
    this->mDialogLogic=new DialogLogic;
    this->mInitLogic=new InitLogic;
    this->mRoutingLogic=new RoutingLogic;
    this->mStagingLogic=new StagingLogic;
    this->mLanLogic=new LanLogic;
    this->mPingLogic=new PingLogic;

    this->mLogicMgr.AddLogic(this->mAccountLogic.get());
    this->mLogicMgr.AddLogic(this->mDialogLogic.get());
    this->mLogicMgr.AddLogic(this->mStagingLogic.get());
    this->mLogicMgr.AddLogic(this->mInitLogic.get());
    this->mLogicMgr.AddLogic(this->mLanLogic.get());
    this->mLogicMgr.AddLogic(this->mRoutingLogic.get());
    this->mLogicMgr.AddLogic(this->mPingLogic.get());
}

}

