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


// FUNCTION: WON 0x00763300
// Creates a CreateAccountCtrl for the lobby's resource config table
static Component* CreateAccountCtrlFactory()				{ return new CreateAccountCtrl; }
// FUNCTION: WON 0x00763360
// Creates a DirectConnectCtrl for the lobby's resource config table
static Component* DirectConnectCtrlFactory()				{ return new DirectConnectCtrl; }
// FUNCTION: WON 0x007633c0
// Creates a GameStagingCtrl for the lobby's resource config table
static Component* GameStagingCtrlFactory()					{ return new GameStagingCtrl; }
// FUNCTION: WON 0x00763420
// Creates an InitFailedCtrl for the lobby's resource config table
static Component* InitFailedCtrlFactory()					{ return new InitFailedCtrl; }
// FUNCTION: WON 0x00763480
// Creates a LobbyOptionsCtrl for the internet options for the lobby's resource config table
static Component* InternetOptionsCtrlFactory()				{ return new LobbyOptionsCtrl(false); }
// FUNCTION: WON 0x007634e0
// Creates a RootScreen for the internet lobby for the lobby's resource config table
static Component* InternetScreenFactory()					{ return new RootScreen(false); }
// FUNCTION: WON 0x00763540
// Creates a LobbyOptionsCtrl for the LAN options for the lobby's resource config table
static Component* LanOptionsCtrlFactory()					{ return new LobbyOptionsCtrl(true); }
// FUNCTION: WON 0x007635a0
// Creates a RootScreen for the LAN lobby for the lobby's resource config table
static Component* LanScreenFactory()						{ return new RootScreen(true); }
// FUNCTION: WON 0x00763600
// Creates a LoginCtrl for the lobby's resource config table
static Component* LoginCtrlFactory()						{ return new LoginCtrl; }
// FUNCTION: WON 0x00763660
// Creates a LoginHelpCtrl for the lobby's resource config table
static Component* LoginHelpCtrlFactory()					{ return new LoginHelpCtrl; }
// FUNCTION: WON 0x007636c0
// Creates a LoginScreen for the lobby's resource config table
static Component* LoginScreenFactory()						{ return new LoginScreen; }
// FUNCTION: WON 0x00763720
// Creates a LoginStatusCtrl for the lobby's resource config table
static Component* LoginStatusCtrlFactory()					{ return new LoginStatusCtrl; }
// FUNCTION: WON 0x00763780
// Creates a LostPasswordCtrl for the lobby's resource config table
static Component* LostPasswordCtrlFactory()				{ return new LostPasswordCtrl; }
// FUNCTION: WON 0x007637e0
// Creates a LostUserNameCtrl for the lobby's resource config table
static Component* LostUserNameCtrlFactory()				{ return new LostUserNameCtrl; }
// FUNCTION: WON 0x00763840
// Creates a MOTDCtrl for the lobby's resource config table
static Component* MOTDCtrlFactory()						{ return new MOTDCtrl; }
// FUNCTION: WON 0x007638a0
// Creates a QuickPlayCtrl for the lobby's resource config table
static Component* QuickPlayCtrlFactory()					{ return new QuickPlayCtrl; }
// FUNCTION: WON 0x00763900
// Creates a RoomCtrl for the lobby's resource config table
static Component* RoomCtrlFactory()						{ return new RoomCtrl; }
// FUNCTION: WON 0x00763960
// Creates an UpdateAccountCtrl for the lobby's resource config table
static Component* UpdateAccountCtrlFactory()				{ return new UpdateAccountCtrl; }

// FUNCTION: WON 0x007639c0
// Creates an AddFriendDialog for the lobby's resource config table
static Component* AddFriendDialogFactory()					{ return new AddFriendDialog; }
// FUNCTION: WON 0x00763a20
// Creates an AdminActionDialog for the lobby's resource config table
static Component* AdminActionDialogFactory()				{ return new AdminActionDialog; }
// FUNCTION: WON 0x00763a80
// Creates a BadUserDialog for the lobby's resource config table
static Component* BadUserDialogFactory()					{ return new BadUserDialog; }
// FUNCTION: WON 0x00763ae0
// Creates a ChooseNetworkAdapterDialog for the lobby's resource config table
static Component* ChooseNetworkAdapterDialogFactory()		{ return new ChooseNetworkAdapterDialog; }
// FUNCTION: WON 0x00763b40
// Creates a CreateGameDialog for the lobby's resource config table
static Component* CreateGameDialogFactory()				{ return new CreateGameDialog; }
// FUNCTION: WON 0x00763ba0
// Creates a CreateRoomDialog for the lobby's resource config table
static Component* CreateRoomDialogFactory()				{ return new CreateRoomDialog; }
// FUNCTION: WON 0x00763c00
// Creates a CreditsDialog for the lobby's resource config table
static Component* CreditsDialogFactory()					{ return new CreditsDialog; }
// FUNCTION: WON 0x00763c60
// Creates a DirectConnectDialog for the lobby's resource config table
static Component* DirectConnectDialogFactory()				{ return new DirectConnectDialog; }
// FUNCTION: WON 0x00763cc0
// Creates a LanNameDialog for the lobby's resource config table
static Component* LanNameDialogFactory()					{ return new LanNameDialog; }
// FUNCTION: WON 0x00763d20
// Creates a LanPortDialog for the lobby's resource config table
static Component* LanPortDialogFactory()					{ return new LanPortDialog; }
// FUNCTION: WON 0x00763d80
// Creates an OkCancelDialog for the lobby's resource config table
static Component* OkCancelDialogFactory()					{ return new OkCancelDialog; }
// FUNCTION: WON 0x00763de0
// Creates a PasswordDialog for the lobby's resource config table
static Component* PasswordDialogFactory()					{ return new PasswordDialog; }
// FUNCTION: WON 0x00763e40
// Creates a RoomDialog for the lobby's resource config table
static Component* RoomDialogFactory()						{ return new RoomDialog; }
// FUNCTION: WON 0x00763ea0
// Creates a StatusDialog for the lobby's resource config table
static Component* StatusDialogFactory()					{ return new StatusDialog; }
// FUNCTION: WON 0x00763f00
// Creates a TOUDialog for the lobby's resource config table
static Component* TOUDialogFactory()						{ return new TOUDialog; }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00763f60
// Builds the lobby's resource config table: the base GUI and browser types plus every lobby screen, control,
// dialog and button type the _Lobby*.cfg files can name
static ResourceConfigTablePtr GetLobbyResourceConfigTable(ResourceConfigTable *theTable)
{
	ResourceConfigTablePtr aTable = LobbyResource_Init(theTable);
	WONGUIConfig_Init(aTable.get());	
	BrowserComponentConfig::Init(aTable.get());

	aTable->AddAllocator("ChatCtrl",ChatCtrlConfig::CfgFactory,ChatCtrlConfig::CompFactory);
	aTable->AddAllocator("CreateAccountCtrl",ContainerConfig::CfgFactory,CreateAccountCtrlFactory);
	aTable->AddAllocator("DirectConnectCtrl",ContainerConfig::CfgFactory,DirectConnectCtrlFactory);
	aTable->AddAllocator("FriendsListCtrl",FriendsListCtrl::CfgFactory,FriendsListCtrl::CompFactory);
	aTable->AddAllocator("GameBrowserCtrl",GameBrowserCtrlConfig::CfgFactory,GameBrowserCtrlConfig::CompFactory);
	aTable->AddAllocator("GameStagingCtrl",ContainerConfig::CfgFactory,GameStagingCtrlFactory);
	aTable->AddAllocator("GameUserListCtrl",GameUserListCtrlConfig::CfgFactory,GameUserListCtrlConfig::CompFactory);
	aTable->AddAllocator("InitFailedCtrl",ContainerConfig::CfgFactory,InitFailedCtrlFactory);
	aTable->AddAllocator("InternetOptionsCtrl",ContainerConfig::CfgFactory,InternetOptionsCtrlFactory);
	aTable->AddAllocator("InternetScreen",ContainerConfig::CfgFactory,InternetScreenFactory);
	aTable->AddAllocator("LanOptionsCtrl",ContainerConfig::CfgFactory,LanOptionsCtrlFactory);
	aTable->AddAllocator("LanScreen",ContainerConfig::CfgFactory,LanScreenFactory);
	aTable->AddAllocator("LoginCtrl",DialogConfig::CfgFactory,LoginCtrlFactory);
	aTable->AddAllocator("LoginHelpCtrl",ContainerConfig::CfgFactory,LoginHelpCtrlFactory);	
	aTable->AddAllocator("LoginScreen",ContainerConfig::CfgFactory,LoginScreenFactory);
	aTable->AddAllocator("LoginStatusCtrl",ContainerConfig::CfgFactory,LoginStatusCtrlFactory);
	aTable->AddAllocator("LostPasswordCtrl",ContainerConfig::CfgFactory,LostPasswordCtrlFactory);
	aTable->AddAllocator("LostUserNameCtrl",ContainerConfig::CfgFactory,LostUserNameCtrlFactory);
	aTable->AddAllocator("MOTDCtrl",ContainerConfig::CfgFactory,MOTDCtrlFactory);
	aTable->AddAllocator("QuickPlayCtrl",DialogConfig::CfgFactory,QuickPlayCtrlFactory);
	aTable->AddAllocator("RoomCtrl",ContainerConfig::CfgFactory,RoomCtrlFactory);
	aTable->AddAllocator("RoomLabelCtrl",RoomLabelCtrlConfig::CfgFactory,RoomLabelCtrlConfig::CompFactory);
	aTable->AddAllocator("ServerListCtrl",ServerListCtrlConfig::CfgFactory,ServerListCtrlConfig::CompFactory);
	aTable->AddAllocator("UpdateAccountCtrl",ContainerConfig::CfgFactory,UpdateAccountCtrlFactory);
	aTable->AddAllocator("UserListCtrl",UserListCtrlConfig::CfgFactory,UserListCtrlConfig::CompFactory);

	aTable->AddAllocator("AddFriendDialog",DialogConfig::CfgFactory,AddFriendDialogFactory);
	aTable->AddAllocator("AdminActionDialog",DialogConfig::CfgFactory,AdminActionDialogFactory);
	aTable->AddAllocator("BadUserDialog",DialogConfig::CfgFactory,BadUserDialogFactory);
	aTable->AddAllocator("ChooseNetworkAdapterDialog",DialogConfig::CfgFactory,ChooseNetworkAdapterDialogFactory);
	aTable->AddAllocator("CreditsDialog",CreditsDialogConfig::CfgFactory,CreditsDialogFactory);
	aTable->AddAllocator("CreateGameDialog",DialogConfig::CfgFactory,CreateGameDialogFactory);
	aTable->AddAllocator("CreateRoomDialog",DialogConfig::CfgFactory,CreateRoomDialogFactory);
	aTable->AddAllocator("DirectConnectDialog",DialogConfig::CfgFactory,DirectConnectDialogFactory);
	aTable->AddAllocator("LanNameDialog",DialogConfig::CfgFactory,LanNameDialogFactory);
	aTable->AddAllocator("LanPortDialog",DialogConfig::CfgFactory,LanPortDialogFactory);
	aTable->AddAllocator("OkCancelDialog",DialogConfig::CfgFactory,OkCancelDialogFactory);
	aTable->AddAllocator("PasswordDialog",DialogConfig::CfgFactory,PasswordDialogFactory);
	aTable->AddAllocator("RoomDialog",DialogConfig::CfgFactory,RoomDialogFactory);
	aTable->AddAllocator("StatusDialog",DialogConfig::CfgFactory,StatusDialogFactory);
	aTable->AddAllocator("TOUDialog",DialogConfig::CfgFactory,TOUDialogFactory);


	aTable->AddAllocator("CrossPromotionButton",CrossPromotionButtonConfig::CfgFactory,CrossPromotionButtonConfig::CompFactory);
	aTable->AddAllocator("ImageTextButton",ImageTextButtonConfig::CfgFactory,ImageTextButtonConfig::CompFactory);
	aTable->AddAllocator("MaxSplitterButton",MaxSplitterButtonConfig::CfgFactory,MaxSplitterButtonConfig::CompFactory);

	aTable->AddAllocator("GameOptionsCtrl",ContainerConfig::CfgFactory,ContainerConfig::CompFactory);
	aTable->AddAllocator("PlayerOptionsCtrl",ContainerConfig::CfgFactory,ContainerConfig::CompFactory);

	return aTable;
}


namespace WONAPI {

// FUNCTION: WON 0x00765ac0
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
    ServerListCtrl::InitColumnMap();
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

// FUNCTION: WON 0x007660f0
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

// FUNCTION: WON 0x00766010
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
// FUNCTION: WON 0x007628c0
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

namespace WONAPI {
// FUNCTION: WON 0x007652c0
// Creates a friends list control
Component* FriendsListCtrl::CompFactory()
{
    return new FriendsListCtrl;
}

// FUNCTION: WON 0x00765320
// Creates the plain container config used for a friends list control
ComponentConfig* FriendsListCtrl::CfgFactory()
{
    return new ContainerConfig;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00763080
// (public: virtual void * __thiscall WONAPI::WONLobbyPrv::`scalar deleting destructor'(unsigned int)    [??_GWONLobbyPrv@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00765180
// (public: static class WONAPI::Component * __cdecl WONAPI::ChatCtrlConfig::CompFactory(void)    [?CompFactory@ChatCtrlConfig@WONAPI@@SAPAVComponent@2@XZ)
// TEMPLATE: WON 0x00765520
// (public: static class WONAPI::Component * __cdecl WONAPI::GameUserListCtrlConfig::CompFactory(void)    [?CompFactory@GameUserListCtrlConfig@WONAPI@@SAP)
// TEMPLATE: WON 0x00765580
// (public: static class WONAPI::Component * __cdecl WONAPI::RoomLabelCtrlConfig::CompFactory(void)    [?CompFactory@RoomLabelCtrlConfig@WONAPI@@SAPAVComp)
// TEMPLATE: WON 0x007659a0
// (public: static class WONAPI::Component * __cdecl WONAPI::CrossPromotionButtonConfig::CompFactory(void)    [?CompFactory@CrossPromotionButtonConfig@WON)
// TEMPLATE: WON 0x00765a00
// (public: static class WONAPI::Component * __cdecl WONAPI::UserListCtrlConfig::CompFactory(void)    [?CompFactory@UserListCtrlConfig@WONAPI@@SAPAVCompon)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00765260
// (public: static class WONAPI::ComponentConfig * __cdecl WONAPI::CreditsDialogConfig::CfgFactory(void)    [?CfgFactory@CreditsDialogConfig@WONAPI@@SAPAV)
// TEMPLATE: WON 0x00765380
// (public: static class WONAPI::Component * __cdecl WONAPI::GameBrowserCtrlConfig::CompFactory(void)    [?CompFactory@GameBrowserCtrlConfig@WONAPI@@SAPAV)
// TEMPLATE: WON 0x007656a0
// (public: static class WONAPI::Component * __cdecl WONAPI::ServerListCtrlConfig::CompFactory(void)    [?CompFactory@ServerListCtrlConfig@WONAPI@@SAPAVCo)
// TEMPLATE: WON 0x00765760
// (public: static class WONAPI::Component * __cdecl WONAPI::ImageTextButtonConfig::CompFactory(void)    [?CompFactory@ImageTextButtonConfig@WONAPI@@SAPAV)
