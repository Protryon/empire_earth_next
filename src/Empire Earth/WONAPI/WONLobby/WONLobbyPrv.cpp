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
template <class T> static Component* New() { return new T; }
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

	aTable->AddAllocator("ChatCtrl",ChatCtrlConfig::CfgFactory,ChatCtrlConfig::CompFactory);
	aTable->AddAllocator("CreateAccountCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::CreateAccount>);
	aTable->AddAllocator("DirectConnectCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::DirectConnect>);
	aTable->AddAllocator("FriendsListCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::Friends>);
	aTable->AddAllocator("GameBrowserCtrl",GameBrowserCtrlConfig::CfgFactory,GameBrowserCtrlConfig::CompFactory);
	aTable->AddAllocator("GameStagingCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::Staging>);
	aTable->AddAllocator("GameUserListCtrl",GameUserListCtrlConfig::CfgFactory,GameUserListCtrlConfig::CompFactory);
	aTable->AddAllocator("InitFailedCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::InitFailed>);
	aTable->AddAllocator("InternetOptionsCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::NetOptions>);
	aTable->AddAllocator("InternetScreen",ContainerConfig::CfgFactory,ScreenFactory<false>);
	aTable->AddAllocator("LanOptionsCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::LanOptions>);
	aTable->AddAllocator("LanScreen",ContainerConfig::CfgFactory,ScreenFactory<true>);
	aTable->AddAllocator("LoginCtrl",DialogConfig::CfgFactory,DlgCtlFactory<Ck::Login>);
	aTable->AddAllocator("LoginHelpCtrl",ContainerConfig::CfgFactory,New<LoginHelpCtrl>);	
	aTable->AddAllocator("LoginScreen",ContainerConfig::CfgFactory,New<LoginScreen>);
	aTable->AddAllocator("LoginStatusCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::LoginStatus>);
	aTable->AddAllocator("LostPasswordCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::LostPassword>);
	aTable->AddAllocator("LostUserNameCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::LostUserName>);
	aTable->AddAllocator("MOTDCtrl",ContainerConfig::CfgFactory,New<MOTDCtrl>);
	aTable->AddAllocator("QuickPlayCtrl",DialogConfig::CfgFactory,DlgCtlFactory<Ck::QuickPlay>);
	aTable->AddAllocator("RoomCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::Room>);
	aTable->AddAllocator("RoomLabelCtrl",RoomLabelCtrlConfig::CfgFactory,RoomLabelCtrlConfig::CompFactory);
	aTable->AddAllocator("ServerListCtrl",ServerListCtrlConfig::CfgFactory,ServerListCtrlConfig::CompFactory);
	aTable->AddAllocator("UpdateAccountCtrl",ContainerConfig::CfgFactory,CtlFactory<Ck::UpdateAccount>);
	aTable->AddAllocator("UserListCtrl",UserListCtrlConfig::CfgFactory,UserListCtrlConfig::CompFactory);

	aTable->AddAllocator("AddFriendDialog",DialogConfig::CfgFactory,New<AddFriendDialog>);
	aTable->AddAllocator("AdminActionDialog",DialogConfig::CfgFactory,New<AdminActionDialog>);
	aTable->AddAllocator("BadUserDialog",DialogConfig::CfgFactory,New<BadUserDialog>);
	aTable->AddAllocator("ChooseNetworkAdapterDialog",DialogConfig::CfgFactory,New<ChooseNetworkAdapterDialog>);
	aTable->AddAllocator("CreditsDialog",CreditsDialogConfig::CfgFactory,New<CreditsDialog>);
	aTable->AddAllocator("CreateGameDialog",DialogConfig::CfgFactory,New<CreateGameDialog>);
	aTable->AddAllocator("CreateRoomDialog",DialogConfig::CfgFactory,New<CreateRoomDialog>);
	aTable->AddAllocator("DirectConnectDialog",DialogConfig::CfgFactory,New<DirectConnectDialog>);
	aTable->AddAllocator("LanNameDialog",DialogConfig::CfgFactory,New<LanNameDialog>);
	aTable->AddAllocator("LanPortDialog",DialogConfig::CfgFactory,New<LanPortDialog>);
	aTable->AddAllocator("OkCancelDialog",DialogConfig::CfgFactory,New<OkCancelDialog>);
	aTable->AddAllocator("PasswordDialog",DialogConfig::CfgFactory,New<PasswordDialog>);
	aTable->AddAllocator("RoomDialog",DialogConfig::CfgFactory,New<RoomDialog>);
	aTable->AddAllocator("StatusDialog",DialogConfig::CfgFactory,New<StatusDialog>);
	aTable->AddAllocator("TOUDialog",DialogConfig::CfgFactory,New<TOUDialog>);


	aTable->AddAllocator("CrossPromotionButton",CrossPromotionButtonConfig::CfgFactory,CrossPromotionButtonConfig::CompFactory);
	aTable->AddAllocator("ImageTextButton",ImageTextButtonConfig::CfgFactory,ImageTextButtonConfig::CompFactory);
	aTable->AddAllocator("MaxSplitterButton",MaxSplitterButtonConfig::CfgFactory,MaxSplitterButtonConfig::CompFactory);

	aTable->AddAllocator("GameOptionsCtrl",ContainerConfig::CfgFactory,ContainerConfig::CompFactory);
	aTable->AddAllocator("PlayerOptionsCtrl",ContainerConfig::CfgFactory,ContainerConfig::CompFactory);

	return aTable;
}


namespace WONAPI {

// Starts the lobby: parses WONLobby.cfg, builds the lobby screens from _LobbyMaster.cfg, inits the logic objects
// and derives the internet and LAN product ids; on failure stores the reason in mInitError and returns false
bool WONLobbyPrv::Init(ResourceConfigTable* input,void (*initFunc)(ComponentConfig*))
{
    this->mInitError=GetEmptyGUIStringData();
    ConfigParser parser;
    parser.mWarningsAsErrors=true;
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

