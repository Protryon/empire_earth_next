// the bodies R6 WON round 33 moved to Rust, as they were at fb32b70d8 (by tests/won/won_lobby33_gen.py),
// over won_lobby33_world.h's stand-ins: a member function as a free function of `self`
#ifndef WON_LOBBY33_ORIG_H
#define WON_LOBBY33_ORIG_H
namespace L33Orig {
using namespace L33World;

static ResourceConfigTablePtr GetLobbyResourceConfigTable(ResourceConfigTable *theTable)
{
	ResourceConfigTablePtr aTable = LobbyResource_Init(theTable);
	WONGUIConfig_Init(aTable.get());	
	aTable->AddAllocator("BrowserComponent",NewConfigOf<EeWonCfgKind::Browser>,NewComponentOf<BrowserComponent>);

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
	aTable->AddAllocator("LoginCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::Login>);
	aTable->AddAllocator("LoginHelpCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<LoginHelpCtrl>);	
	aTable->AddAllocator("LoginScreen",NewConfigOf<ContainerConfig>,NewComponentOf<LoginScreen>);
	aTable->AddAllocator("LoginStatusCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LoginStatus>);
	aTable->AddAllocator("LostPasswordCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostPassword>);
	aTable->AddAllocator("LostUserNameCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostUserName>);
	aTable->AddAllocator("MOTDCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<MOTDCtrl>);
	aTable->AddAllocator("QuickPlayCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::QuickPlay>);
	aTable->AddAllocator("RoomCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Room>);
	aTable->AddAllocator("RoomLabelCtrl",NewConfigOf<RoomLabelCtrlConfig>,NewComponentOf<RoomLabelCtrl>);
	aTable->AddAllocator("ServerListCtrl",NewConfigOf<ServerListCtrlConfig>,NewComponentOf<ServerListCtrl>);
	aTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>);
	aTable->AddAllocator("UserListCtrl",NewConfigOf<UserListCtrlConfig>,UserListCtrlConfig::CompFactory);

	aTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>);
	aTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>);
	aTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>);
	aTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>);
	aTable->AddAllocator("CreditsDialog",NewConfigOf<CreditsDialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>);
	aTable->AddAllocator("CreateGameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateGame>);
	aTable->AddAllocator("CreateRoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateRoom>);
	aTable->AddAllocator("DirectConnectDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::DirectConnect>);
	aTable->AddAllocator("LanNameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanName>);
	aTable->AddAllocator("LanPortDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanPort>);
	aTable->AddAllocator("OkCancelDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::OkCancel>);
	aTable->AddAllocator("PasswordDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Password>);
	aTable->AddAllocator("RoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Room>);
	aTable->AddAllocator("StatusDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Status>);
	aTable->AddAllocator("TOUDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Tou>);


	aTable->AddAllocator("CrossPromotionButton",NewConfigOf<CrossPromotionButtonConfig>,NewComponentOf<CrossPromotionButton>);
	aTable->AddAllocator("ImageTextButton",NewConfigOf<ImageTextButtonConfig>,NewComponentOf<ImageTextButton>);
	aTable->AddAllocator("MaxSplitterButton",NewConfigOf<EeWonCfgKind::MsArrowButton>,NewShellOf<EeWonShellKind::MaxSplitterButton,Direction_Up>);

	aTable->AddAllocator("GameOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>);
	aTable->AddAllocator("PlayerOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>);

	return aTable;
}

static bool PrvInit(WONLobbyPrv* self, ResourceConfigTable* input,void (*initFunc)(ComponentConfig*))
{
    self->mInitError=GetEmptyGUIStringData();
    ConfigParser parser;
    parser.SetWarningsAsErrors(true);
    if(!parser.OpenFile(WONAPICore::GetDefaultFileDirectory()+LobbyConfig::mDefaultFileName)) {
        self->mInitError=GUIString("Couldn't find "+LobbyConfig::mDefaultFileName);
        return false;
    }
    if(!self->mConfig->ParseTopLevel(parser,NULL)) {
        self->mInitError=GUIString(LobbyConfig::mDefaultFileName+" error: ");
        self->mInitError.append(GUIString(parser.GetAbortReason()));
        return false;
    }
    self->mLobbyPort=self->mConfig->mLobbyPort;
    SmartPtr<ContainerConfig> componentConfig=new ContainerConfig;
    self->mLobbyContainer=new LobbyContainer;
    ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory()+self->mConfig->mResourceDirectory);
    SmartPtr<ResourceConfigTable> resources=GetLobbyResourceConfigTable(input);
    ee_won_lobbyctl_init_columns();
    std::map<std::string,std::string,StringLessNoCase>::iterator itr=self->mConfig->mDefines.begin();
    while(itr!=self->mConfig->mDefines.end()) {
        parser.AddDefine(itr->first,itr->second,false);
        ++itr;
    }
    if(!componentConfig->ReadConfig_Unsafe(self->mLobbyContainer.get(),"_LobbyMaster.cfg",resources.get(),&parser)) {
        self->mInitError=componentConfig->mConfigError;
        return false;
    }
    try {
        self->mLogicMgr.Init(componentConfig.get());
        if(initFunc!=NULL) initFunc(componentConfig.get());
    }
    catch(ConfigObjectException& error) {
        self->mInitError=GUIString(error.mText);
        return false;
    }
    CRC16 crc;
    crc.Put(self->mConfig->mProductName.data(),self->mConfig->mProductName.length());
    crc.Put(self->mConfig->mVersion.data(),self->mConfig->mVersion.length());
    self->mProductId=crc.Get();
    crc.Reset();
    crc.Put(self->mConfig->mProductName.data(),self->mConfig->mProductName.length());
    crc.Put(self->mConfig->mLanVersion.data(),self->mConfig->mLanVersion.length());
    self->mLanProductId=crc.Get();
    self->mChatCommandParser->InitCommands();
    return true;
}

static void PrvDestroy(WONLobbyPrv* self)
{
    self->mLogicMgr.Kill();
    if(self->mUDPManager.get()!=NULL) {
        self->mUDPManager->Close();
        self->mUDPManager=NULL;
    }
    if(self->mAuthContext.get()!=NULL) {
        self->mAuthContext->Kill();
        self->mAuthContext=NULL;
    }
}

static void PrvInitAuth(WONLobbyPrv* self, const std::wstring& loginCommunity,const char* hashFileListStr)
{
    self->mHasSetHashFileList=false;
    self->mHashFileList.clear();
    if(hashFileListStr!=NULL) {
        StringParser parser(hashFileListStr);
        parser.ReadValue(self->mHashFileList);
    }
    self->mLoginCommunity=loginCommunity;
    self->mAuthContext->SetCommunity(self->mLoginCommunity);
    if(!self->mConfig->mCDKeyCheck)
        return;
    self->mCDKey.SetProductString(self->mConfig->mProductName);
    self->mCDKey.LoadFromRegistry();
    self->mAuthContext->SetCDKey(self->mLoginCommunity,self->mCDKey);
}

static void PrvConstruct(WONLobbyPrv* self)
{
    self->mLobbyPort=8888;
    self->mSysTOUTime=self->mGameTOUTime=0;
    self->mBehindFirewall=false;
    self->mFirewallTestDone=false;
    self->mSuspended=false;
    self->mCurGameType=(LobbyGameType)0;
    self->mLobbyStatus=(LobbyStatus)0;
    self->mRunMode=(LobbyRunMode)1;
    self->mProductId=0;
    self->mLanProductId=0;

    self->mSysTOUPath=WONAPICore::GetDefaultFileDirectory()+"_wonsystou_m.txt";
    self->mGameTOUPath=WONAPICore::GetDefaultFileDirectory()+"_wongametou_m.txt";

    self->mConfig=LobbyConfig::mAllocator();
    self->mPersistentData=new LobbyPersistentData;
    self->mChatCommandParser=new ChatCommandParser;
    self->mUDPManager=new UDPManager;
    self->mAccountServers=new ServerContext;
    self->mAuthServers=new ServerContext;
    self->mDirServers=new ServerContext;
    self->mRoomDirServers=new ServerContext;
    self->mFirewallDetectServers=new ServerContext;
    self->mPatchServers=new ServerContext;
    self->mVersionStatus=WS_None;
    self->mAuthContext=new AuthContext;
    self->mAuthContext->SetServerContext(self->mAuthServers.get());

    self->mAccountLogic=new AccountLogic;
    self->mDialogLogic=new DialogLogic;
    self->mInitLogic=new InitLogic;
    self->mRoutingLogic=new RoutingLogic;
    self->mStagingLogic=new StagingLogic;
    self->mLanLogic=new LanLogic;
    self->mPingLogic=new PingLogic;

    self->mLogicMgr.AddLogic(self->mAccountLogic.get());
    self->mLogicMgr.AddLogic(self->mDialogLogic.get());
    self->mLogicMgr.AddLogic(self->mStagingLogic.get());
    self->mLogicMgr.AddLogic(self->mInitLogic.get());
    self->mLogicMgr.AddLogic(self->mLanLogic.get());
    self->mLogicMgr.AddLogic(self->mRoutingLogic.get());
    self->mLogicMgr.AddLogic(self->mPingLogic.get());
}

static void ModelAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void ModelAppendUnits(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring* anOut = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        WideAppendUtf16Unit(*anOut, inUnits[i]);
}

static void WriteSummary(LobbyGame* self, WriteBuffer& outBuffer)
{
    EeWonGameSummary aSummary;
    memset(&aSummary, 0, sizeof(aSummary));
    aSummary.game_type = (int32_t)self->GetGameType();
    IPAddr anAddress = self->GetAddress();
    if (aSummary.game_type == 2)
        memcpy(aSummary.address, anAddress.GetSixByte(), 6);
    else
        aSummary.product_id = GetLobbyProductId();
    aSummary.in_progress = self->IsInProgress();
    aSummary.has_password = self->GetPassword().length() != 0;
    aSummary.invite_only = self->GetInviteOnly();
    aSummary.ask_to_join = self->GetAskToJoin();
    aSummary.skill = (int32_t)self->GetSkill();
    aSummary.players = (int32_t)self->GetNumPlayers();
    aSummary.open_slots = (int32_t)self->GetMaxPlayers();
    std::string aName, aBytes;
    if (aSummary.game_type != 2)
        WideAppendUtf16LE(aName, (std::wstring)self->GetName());
    ee_won_game_write_summary(&aSummary, reinterpret_cast<const uint8_t*>(aName.data()), aName.length(),
                              ModelAppendBytes, &aBytes);
    outBuffer.AppendBytes(aBytes.data(), aBytes.length());
    self->WriteSummaryHook(outBuffer);
}

static bool ReadSummary(LobbyGame* self, ReadBuffer& inBuffer)
{
    long aStart = inBuffer.pos();
    EeWonGameSummary aSummary;
    std::wstring aName;
    ee_won_game_read_summary(reinterpret_cast<const uint8_t*>(inBuffer.data()) + aStart,
                             (size_t)(inBuffer.length() - aStart), (int32_t)self->GetGameType(), GetLobbyProductId(),
                             &aSummary, ModelAppendUnits, &aName);
    GUIString aText;
    if (aSummary.read & 4) aText = GUIString(new GUIStringData(aName));
    self->M(LobbyMC::GameStoreSummary, (intptr_t)&aSummary, (intptr_t)aText.mData.get());
    if (!aSummary.ok) return false;
    inBuffer.SetPos((int)(aStart + aSummary.rest));
    try {
        return self->ReadSummaryHook(inBuffer);
    }
    catch (ReadBufferException&) { return false; }
}

static bool ReadDetails(LobbyGame* self, ReadBuffer& inBuffer)
{
    try {
        if (self->ReadDetailsHook(inBuffer)) {
            self->MSet(LobbyMF::GameHaveDetails, true);
            return true;
        }
        return false;
    }
    catch (ReadBufferException&) { return false; }
}

static ConstSmartPtr<ByteBuffer> GetJoinRequest(LobbyGame* self)
{
    WriteBuffer buffer(0,true);
    EeWonRoomMsg head;
    memset(&head,0,sizeof(head));
    head.kind=1;
    head.ping=self->GetPing();
    std::string bytes;
    ee_won_room_message(&head,ModelAppendBytes,&bytes);
    buffer.AppendBytes(bytes.data(),bytes.length());
    self->GetJoinGameRequestHook(buffer);
    return buffer.ToByteBuffer(true);
}

static short HandleJoinRequest(LobbyGame* self, ReadBuffer& inBuffer,LobbyClient* inClient,WriteBuffer& outBuffer)
{
    SmartPtr<LobbyPlayer> player;
    short check=ee_won_game_host_join_check(self->IsFull(),self->IsInProgress());
    if (check!=0) return check;
    LobbyConfig* config=GetLobbyConfig();
    SmartPtr<LobbyClientList> clients=self->GetClients();
    if (config!=NULL && !config->mAllowDuplicateNames) {
        for (size_t i=0; i<clients->GetCount(); ++i) {
            LobbyClient* aMember=clients->ClientAt(i);
            if (aMember->GetName()==inClient->GetName() && aMember!=inClient) return 8;
        }
    }
    player=LobbyPlayer::CreatePlayer();
    inClient->SetPlayer(player.get());
    try {
        short anId=inBuffer.ReadShort();
        player->SetPing(anId);
        short result=self->HandleJoinGameRequestHook(inBuffer,inClient);
        if (result!=0) { inClient->SetPlayer(NULL); return result; }
    } catch (ReadBufferException&) {
        inClient->SetPlayer(NULL);
        return 4;
    }
    long aPlayers=self->GetNumPlayers()+1;
    self->MSet(LobbyMF::GameNumPlayers, aPlayers);
    outBuffer.AppendShort(aPlayers);
    SmartPtr<LobbyClientList> aMembers=self->GetClients();
    LobbyClientSetIterator it(aMembers.get());
    while (it.HasMore()) {
        LobbyClient* client;
        LobbyPlayer* otherPlayer=it.Next(&client);
        if (otherPlayer!=NULL) {
            outBuffer.AppendShort(client->GetClientId());
            otherPlayer->WriteToBuffer(outBuffer);
        }
    }
    self->GetJoinGameReplyHook(inClient,outBuffer);
    return 0;
}

static ConstSmartPtr<ByteBuffer> GetJoinReply(LobbyGame* self, ReadBuffer& inBuffer,LobbyClient* inClient)
{
    WriteBuffer buffer(0,true);
    buffer.AppendByte(2);
    buffer.AppendShort(0);
    buffer.SetShort(1,self->HandleJoinRequest(inBuffer,inClient,buffer));
    return buffer.ToByteBuffer(true);
}

static short HandleJoinReply(LobbyGame* self, ReadBuffer& inBuffer)
{
    short result;
    try {
        result=inBuffer.ReadShort();
        if (result!=0) return result;
        unsigned short count=inBuffer.ReadShort();
        for (int i=0; i<count; ++i) {
            LobbyClient* client=self->FindClient(inBuffer.ReadShort());
            if (client!=NULL) {
                SmartPtr<LobbyPlayer> player=LobbyPlayer::CreatePlayer();
                if (player->ReadFromBuffer(inBuffer)) client->SetPlayer(player.get());
            }
        }
        self->HandleJoinGameReplyHook(inBuffer);
    } catch (ReadBufferException&) { result=4; }
    return result;
}

static GUIString GetStatusString(LobbyGame* self, short status)
{
    switch(ee_won_model_status_text(status)) {
    case 0: return LOBBY_STRING(LobbyGameStatus_Success_String);
    case 1: return LOBBY_STRING(LobbyGameStatus_GameFull_String);
    case 2: return LOBBY_STRING(LobbyGameStatus_GameInProgress_String);
    case 3: return LOBBY_STRING(LobbyGameStatus_UnpackFailure_String);
    case 4: return LOBBY_STRING(LobbyGameStatus_NotInvited_String);
    case 5: return LOBBY_STRING(LobbyGameStatus_CaptainRejectedYou_String);
    case 6: return LOBBY_STRING(LobbyGameStatus_InvalidPassword_String);
    case 7: return LOBBY_STRING(LobbyGameStatus_DuplicateName_String);
    }
    GUIString custom=self->GetStatusStringHook(status);
    if(custom.length()!=0) return custom;
    char buffer[50];
    sprintf(buffer,"%d",static_cast<int>(status));
    return GUIString(new GUIStringData(buffer));
}

static bool ReadFromBuffer(LobbyPlayer* self, ReadBuffer& inBuffer)
{
    try {
        self->SetPing(inBuffer.ReadShort());
        self->SetReady(inBuffer.ReadBool());
        return self->ReadData(inBuffer);
    }
    catch (ReadBufferException&) {
        return false;
    }
}

static void WriteToBuffer(LobbyPlayer* self, WriteBuffer& outBuffer)
{
    std::string aHead;
    ee_won_room_player_head(self->GetPing(), self->GetReady(), ModelAppendBytes, &aHead);
    outBuffer.AppendBytes(aHead.data(), aHead.length());
    self->WriteData(outBuffer);
}

static void ChatLinkInit(ChatLink* self, const EeWonLobbyCtlLink& l)
{
    self->mKind = l.kind;
    {
        // the original's five constructors, by kind
        switch (l.kind) {
        case 3: self->mClient = (LobbyClient*)l.obj; self->mText = self->mClient->GetName(); break;
        case 0:
            self->mText = Txt(l.text);
            self->mGroupId = (unsigned short)l.id;
            if (l.addr != 0) self->mAddress = ((LobbyServer*)l.addr)->GetAddress();
            break;
        case 4: self->mFriend = (LobbyFriend*)l.obj; self->mText = self->mFriend->GetName(); break;
        case 5:
            self->mFriend = (LobbyFriend*)l.obj;
            self->mAddress = IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)l.addr));
            self->mText = Txt(l.text);
            break;
        default: self->mClient = (LobbyClient*)l.obj; self->mText = Txt(l.text); break;
        }
        self->mColor = l.color;
        self->mFont = (Font*)l.font;
        self->mEnabled = true;
        self->mPrevious = NULL;
    }
}

}

#endif

