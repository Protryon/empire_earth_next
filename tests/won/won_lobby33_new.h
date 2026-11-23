// the hosts' statements R6 WON round 33 added (by tests/won/won_lobby33_gen.py from the working tree), over
// won_lobby33_world.h's stand-ins
#ifndef WON_LOBBY33_NEW_H
#define WON_LOBBY33_NEW_H
namespace L33New {
using namespace L33World;

// the methods' statements (lobbyprv.rs composes them, R6 WON round 33); Init's locals are these statics
static ConfigParser* sPrvParser;
static SmartPtr<ContainerConfig> sPrvComponentConfig;
static ResourceConfigTablePtr sPrvTable;
static std::map<std::string, std::string, StringLessNoCase>::iterator sPrvDefine;
static CRC16 sPrvCrc;
// Init's try: a config error is the init error (0); 1 when f returned
template <class F> static intptr_t PrvConfigTry(WONLobbyPrv* p, F f)
{
    try {
        f();
        return 1;
    } catch (ConfigObjectException& error) {
        p->mInitError = GUIString(error.mText);
        return 0;
    }
}
static intptr_t PrvHost(void* inPrv, EeWonLobbyPrvOp inOp, intptr_t a)
{
    typedef EeWonLobbyPrvOp Op;
    WONLobbyPrv* p = static_cast<WONLobbyPrv*>(inPrv);
    switch (inOp) {
    case Op::InitErrorClear: p->mInitError = GetEmptyGUIStringData(); return 0;
    case Op::ParserNew: sPrvParser = new ConfigParser; return 0;
    case Op::ParserWarningsAsErrors: sPrvParser->SetWarningsAsErrors(true); return 0;
    case Op::ParserOpenDefault: return WonGuard([&] { return sPrvParser->OpenFile(WONAPICore::GetDefaultFileDirectory() + LobbyConfig::mDefaultFileName); });
    case Op::ErrorNotFound: p->mInitError = GUIString("Couldn't find " + LobbyConfig::mDefaultFileName); return 0;
    case Op::ParseTopLevel: return WonGuard([&] { return p->mConfig->ParseTopLevel(*sPrvParser, NULL); });
    case Op::ErrorParseHead: p->mInitError = GUIString(LobbyConfig::mDefaultFileName + " error: "); return 0;
    case Op::ErrorParseReason: p->mInitError.append(GUIString(sPrvParser->GetAbortReason())); return 0;
    case Op::LobbyPortFromConfig: p->mLobbyPort = p->mConfig->mLobbyPort; return 0;
    case Op::ComponentConfigNew: sPrvComponentConfig = new ContainerConfig; return 0;
    case Op::LobbyContainerNew: p->mLobbyContainer = new LobbyContainer; return 0;
    case Op::SetResourceFolder: ComponentConfig::SetResourceFolder(WONAPICore::GetDefaultFileDirectory() + p->mConfig->mResourceDirectory); return 0;
    case Op::DefinesBegin: sPrvDefine = p->mConfig->mDefines.begin(); return 0;
    case Op::DefinesAtEnd: return (sPrvDefine == p->mConfig->mDefines.end()) ? 1 : 0;
    case Op::DefineAdd: sPrvParser->AddDefine(sPrvDefine->first, sPrvDefine->second, false); return 0;
    case Op::DefineNext: ++sPrvDefine; return 0;
    case Op::ReadMaster: return WonGuard([&] { return sPrvComponentConfig->ReadConfig_Unsafe(p->mLobbyContainer.get(), "_LobbyMaster.cfg", sPrvTable.get(), sPrvParser); });
    case Op::ErrorFromConfig: p->mInitError = sPrvComponentConfig->mConfigError; return 0;
    case Op::LogicMgrInit: return WonGuard([&] { return PrvConfigTry(p, [&] { p->mLogicMgr.Init(sPrvComponentConfig.get()); }); });
    case Op::InitFuncRun: return WonGuard([&] { return PrvConfigTry(p, [&] { reinterpret_cast<void (*)(ComponentConfig*)>(a)(sPrvComponentConfig.get()); }); });
    case Op::CrcNew: sPrvCrc = CRC16(); return 0;
    case Op::CrcPutProductName: sPrvCrc.Put(p->mConfig->mProductName.data(), p->mConfig->mProductName.length()); return 0;
    case Op::CrcPutVersion: sPrvCrc.Put(p->mConfig->mVersion.data(), p->mConfig->mVersion.length()); return 0;
    case Op::ProductIdFromCrc: p->mProductId = sPrvCrc.Get(); return 0;
    case Op::CrcReset: sPrvCrc.Reset(); return 0;
    case Op::CrcPutLanVersion: sPrvCrc.Put(p->mConfig->mLanVersion.data(), p->mConfig->mLanVersion.length()); return 0;
    case Op::LanProductIdFromCrc: p->mLanProductId = sPrvCrc.Get(); return 0;
    case Op::ChatParserInitCommands: p->mChatCommandParser->InitCommands(); return 0;
    case Op::ResDone: sPrvTable = NULL; return 0;
    case Op::ComponentConfigDone: sPrvComponentConfig = NULL; return 0;
    case Op::ParserFree: delete sPrvParser; return 0;
    case Op::LogicMgrKill: p->mLogicMgr.Kill(); return 0;
    case Op::UdpIs: return (p->mUDPManager.get() != NULL) ? 1 : 0;
    case Op::UdpClose: p->mUDPManager->Close(); return 0;
    case Op::UdpClear: p->mUDPManager = NULL; return 0;
    case Op::AuthIs: return (p->mAuthContext.get() != NULL) ? 1 : 0;
    case Op::AuthKill: p->mAuthContext->Kill(); return 0;
    case Op::AuthClear: p->mAuthContext = NULL; return 0;
    case Op::HashSetClear: p->mHasSetHashFileList = false; return 0;
    case Op::HashListClear: p->mHashFileList.clear(); return 0;
    case Op::HashListRead: StringParser(reinterpret_cast<const char*>(a)).ReadValue(p->mHashFileList); return 0;
    case Op::CommunitySet: p->mLoginCommunity = *reinterpret_cast<const std::wstring*>(a); return 0;
    case Op::AuthSetCommunity: p->mAuthContext->SetCommunity(p->mLoginCommunity); return 0;
    case Op::CdKeyCheck: return (p->mConfig->mCDKeyCheck) ? 1 : 0;
    case Op::CdKeySetProduct: p->mCDKey.SetProductString(p->mConfig->mProductName); return 0;
    case Op::CdKeyLoad: p->mCDKey.LoadFromRegistry(); return 0;
    case Op::AuthSetCdKey: p->mAuthContext->SetCDKey(p->mLoginCommunity, p->mCDKey); return 0;
    case Op::LobbyPort8888: p->mLobbyPort = 8888; return 0;
    case Op::TouTimesClear: p->mSysTOUTime = p->mGameTOUTime = 0; return 0;
    case Op::BehindFirewallClear: p->mBehindFirewall = false; return 0;
    case Op::FirewallTestDoneClear: p->mFirewallTestDone = false; return 0;
    case Op::SuspendedClear: p->mSuspended = false; return 0;
    case Op::CurGameTypeClear: p->mCurGameType = (LobbyGameType)0; return 0;
    case Op::LobbyStatusClear: p->mLobbyStatus = (LobbyStatus)0; return 0;
    case Op::RunModeOne: p->mRunMode = (LobbyRunMode)1; return 0;
    case Op::ProductIdClear: p->mProductId = 0; return 0;
    case Op::LanProductIdClear: p->mLanProductId = 0; return 0;
    case Op::SysTouPath: p->mSysTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wonsystou_m.txt"; return 0;
    case Op::GameTouPath: p->mGameTOUPath = WONAPICore::GetDefaultFileDirectory() + "_wongametou_m.txt"; return 0;
    case Op::ConfigNew: p->mConfig = LobbyConfig::mAllocator(); return 0;
    case Op::PersistentDataNew: p->mPersistentData = new LobbyPersistentData; return 0;
    case Op::ChatParserNew: p->mChatCommandParser = new ChatCommandParser; return 0;
    case Op::UdpNew: p->mUDPManager = new UDPManager; return 0;
    case Op::AccountServersNew: p->mAccountServers = new ServerContext; return 0;
    case Op::AuthServersNew: p->mAuthServers = new ServerContext; return 0;
    case Op::DirServersNew: p->mDirServers = new ServerContext; return 0;
    case Op::RoomDirServersNew: p->mRoomDirServers = new ServerContext; return 0;
    case Op::FirewallServersNew: p->mFirewallDetectServers = new ServerContext; return 0;
    case Op::PatchServersNew: p->mPatchServers = new ServerContext; return 0;
    case Op::VersionStatusNone: p->mVersionStatus = WS_None; return 0;
    case Op::AuthContextNew: p->mAuthContext = new AuthContext; return 0;
    case Op::AuthSetServers: p->mAuthContext->SetServerContext(p->mAuthServers.get()); return 0;
    case Op::AccountLogicNew: p->mAccountLogic = new AccountLogic; return 0;
    case Op::DialogLogicNew: p->mDialogLogic = new DialogLogic; return 0;
    case Op::InitLogicNew: p->mInitLogic = new InitLogic; return 0;
    case Op::RoutingLogicNew: p->mRoutingLogic = new RoutingLogic; return 0;
    case Op::StagingLogicNew: p->mStagingLogic = new StagingLogic; return 0;
    case Op::LanLogicNew: p->mLanLogic = new LanLogic; return 0;
    case Op::PingLogicNew: p->mPingLogic = new PingLogic; return 0;
    case Op::AddAccountLogic: p->mLogicMgr.AddLogic(p->mAccountLogic.get()); return 0;
    case Op::AddDialogLogic: p->mLogicMgr.AddLogic(p->mDialogLogic.get()); return 0;
    case Op::AddStagingLogic: p->mLogicMgr.AddLogic(p->mStagingLogic.get()); return 0;
    case Op::AddInitLogic: p->mLogicMgr.AddLogic(p->mInitLogic.get()); return 0;
    case Op::AddLanLogic: p->mLogicMgr.AddLogic(p->mLanLogic.get()); return 0;
    case Op::AddRoutingLogic: p->mLogicMgr.AddLogic(p->mRoutingLogic.get()); return 0;
    case Op::AddPingLogic: p->mLogicMgr.AddLogic(p->mPingLogic.get()); return 0;
    case Op::ResInit: sPrvTable = LobbyResource_Init(reinterpret_cast<ResourceConfigTable*>(a)); return 0;
    case Op::ResGuiInit: WONGUIConfig_Init(sPrvTable.get()); return 0;
    case Op::AllocBrowserComponent: sPrvTable->AddAllocator("BrowserComponent",NewConfigOf<EeWonCfgKind::Browser>,NewComponentOf<BrowserComponent>); return 0;
    case Op::AllocChatCtrl: sPrvTable->AddAllocator("ChatCtrl",NewConfigOf<ChatCtrlConfig>,NewComponentOf<ChatCtrl>); return 0;
    case Op::AllocCreateAccountCtrl: sPrvTable->AddAllocator("CreateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::CreateAccount>); return 0;
    case Op::AllocDirectConnectCtrl: sPrvTable->AddAllocator("DirectConnectCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::DirectConnect>); return 0;
    case Op::AllocFriendsListCtrl: sPrvTable->AddAllocator("FriendsListCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Friends>); return 0;
    case Op::AllocGameBrowserCtrl: sPrvTable->AddAllocator("GameBrowserCtrl",NewConfigOf<GameBrowserCtrlConfig>,NewComponentOf<GameBrowserCtrl>); return 0;
    case Op::AllocGameStagingCtrl: sPrvTable->AddAllocator("GameStagingCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Staging>); return 0;
    case Op::AllocGameUserListCtrl: sPrvTable->AddAllocator("GameUserListCtrl",NewConfigOf<GameUserListCtrlConfig>,GameUserListCtrlConfig::CompFactory); return 0;
    case Op::AllocInitFailedCtrl: sPrvTable->AddAllocator("InitFailedCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::InitFailed>); return 0;
    case Op::AllocInternetOptionsCtrl: sPrvTable->AddAllocator("InternetOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::NetOptions>); return 0;
    case Op::AllocInternetScreen: sPrvTable->AddAllocator("InternetScreen",NewConfigOf<ContainerConfig>,ScreenFactory<false>); return 0;
    case Op::AllocLanOptionsCtrl: sPrvTable->AddAllocator("LanOptionsCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LanOptions>); return 0;
    case Op::AllocLanScreen: sPrvTable->AddAllocator("LanScreen",NewConfigOf<ContainerConfig>,ScreenFactory<true>); return 0;
    case Op::AllocLoginCtrl: sPrvTable->AddAllocator("LoginCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::Login>); return 0;
    case Op::AllocLoginHelpCtrl: sPrvTable->AddAllocator("LoginHelpCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<LoginHelpCtrl>); return 0;
    case Op::AllocLoginScreen: sPrvTable->AddAllocator("LoginScreen",NewConfigOf<ContainerConfig>,NewComponentOf<LoginScreen>); return 0;
    case Op::AllocLoginStatusCtrl: sPrvTable->AddAllocator("LoginStatusCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LoginStatus>); return 0;
    case Op::AllocLostPasswordCtrl: sPrvTable->AddAllocator("LostPasswordCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostPassword>); return 0;
    case Op::AllocLostUserNameCtrl: sPrvTable->AddAllocator("LostUserNameCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::LostUserName>); return 0;
    case Op::AllocMOTDCtrl: sPrvTable->AddAllocator("MOTDCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<MOTDCtrl>); return 0;
    case Op::AllocQuickPlayCtrl: sPrvTable->AddAllocator("QuickPlayCtrl",NewConfigOf<EeWonCfgKind::Dialog>,DlgCtlFactory<Ck::QuickPlay>); return 0;
    case Op::AllocRoomCtrl: sPrvTable->AddAllocator("RoomCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::Room>); return 0;
    case Op::AllocRoomLabelCtrl: sPrvTable->AddAllocator("RoomLabelCtrl",NewConfigOf<RoomLabelCtrlConfig>,NewComponentOf<RoomLabelCtrl>); return 0;
    case Op::AllocServerListCtrl: sPrvTable->AddAllocator("ServerListCtrl",NewConfigOf<ServerListCtrlConfig>,NewComponentOf<ServerListCtrl>); return 0;
    case Op::AllocUpdateAccountCtrl: sPrvTable->AddAllocator("UpdateAccountCtrl",NewConfigOf<ContainerConfig>,CtlFactory<Ck::UpdateAccount>); return 0;
    case Op::AllocUserListCtrl: sPrvTable->AddAllocator("UserListCtrl",NewConfigOf<UserListCtrlConfig>,UserListCtrlConfig::CompFactory); return 0;
    case Op::AllocAddFriendDialog: sPrvTable->AddAllocator("AddFriendDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AddFriend>); return 0;
    case Op::AllocAdminActionDialog: sPrvTable->AddAllocator("AdminActionDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::AdminAction>); return 0;
    case Op::AllocBadUserDialog: sPrvTable->AddAllocator("BadUserDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::BadUser>); return 0;
    case Op::AllocChooseNetworkAdapterDialog: sPrvTable->AddAllocator("ChooseNetworkAdapterDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::ChooseAdapter>); return 0;
    case Op::AllocCreditsDialog: sPrvTable->AddAllocator("CreditsDialog",NewConfigOf<CreditsDialogConfig>,NewLobbyDialog<EeWonLobbyDlgKind::Credits>); return 0;
    case Op::AllocCreateGameDialog: sPrvTable->AddAllocator("CreateGameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateGame>); return 0;
    case Op::AllocCreateRoomDialog: sPrvTable->AddAllocator("CreateRoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::CreateRoom>); return 0;
    case Op::AllocDirectConnectDialog: sPrvTable->AddAllocator("DirectConnectDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::DirectConnect>); return 0;
    case Op::AllocLanNameDialog: sPrvTable->AddAllocator("LanNameDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanName>); return 0;
    case Op::AllocLanPortDialog: sPrvTable->AddAllocator("LanPortDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::LanPort>); return 0;
    case Op::AllocOkCancelDialog: sPrvTable->AddAllocator("OkCancelDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::OkCancel>); return 0;
    case Op::AllocPasswordDialog: sPrvTable->AddAllocator("PasswordDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Password>); return 0;
    case Op::AllocRoomDialog: sPrvTable->AddAllocator("RoomDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Room>); return 0;
    case Op::AllocStatusDialog: sPrvTable->AddAllocator("StatusDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Status>); return 0;
    case Op::AllocTOUDialog: sPrvTable->AddAllocator("TOUDialog",NewConfigOf<EeWonCfgKind::Dialog>,NewLobbyDialog<EeWonLobbyDlgKind::Tou>); return 0;
    case Op::AllocCrossPromotionButton: sPrvTable->AddAllocator("CrossPromotionButton",NewConfigOf<CrossPromotionButtonConfig>,NewComponentOf<CrossPromotionButton>); return 0;
    case Op::AllocImageTextButton: sPrvTable->AddAllocator("ImageTextButton",NewConfigOf<ImageTextButtonConfig>,NewComponentOf<ImageTextButton>); return 0;
    case Op::AllocMaxSplitterButton: sPrvTable->AddAllocator("MaxSplitterButton",NewConfigOf<EeWonCfgKind::MsArrowButton>,NewShellOf<EeWonShellKind::MaxSplitterButton,Direction_Up>); return 0;
    case Op::AllocGameOptionsCtrl: sPrvTable->AddAllocator("GameOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>); return 0;
    case Op::AllocPlayerOptionsCtrl: sPrvTable->AddAllocator("PlayerOptionsCtrl",NewConfigOf<ContainerConfig>,NewComponentOf<Container>); return 0;
    }
    return 0;
}

static std::wstring UnitsText(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    WonWideSink(&aText, inUnits, inCount);
    return aText;
}

static intptr_t HeldConst(const RefCount* inObj) { return Held(const_cast<RefCount*>(inObj)); }
static intptr_t HeldText(const GUIString& inText) { return Held(inText.mData.get()); }
static std::string Utf16LE(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}
static void SinkBytes(intptr_t inSink, const std::string& inBytes) { ee_won_lobby_sink(reinterpret_cast<void*>(inSink), reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.length()); }
// a read's answer, or THREW (modelbuf.rs) for a read past the buffer's end
template <class F> static intptr_t ReadOr(F f)
{
    try {
        return f();
    } catch (ReadBufferException&) {
        return INTPTR_MIN;
    }
}
static intptr_t BufHost(void* inObj, EeWonModelBufOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonModelBufOp Op;
    LobbyGame* g = static_cast<LobbyGame*>(inObj);
    LobbyPlayer* pl = static_cast<LobbyPlayer*>(inObj);
    ReadBuffer* rb = reinterpret_cast<ReadBuffer*>(a);
    WriteBuffer* wb = reinterpret_cast<WriteBuffer*>(a);
    switch (inOp) {
    case Op::BufPos: return (intptr_t)(rb->pos());
    case Op::BufData: return (intptr_t)(rb->data());
    case Op::BufLength: return (intptr_t)(rb->length());
    case Op::BufSetPos: rb->SetPos((int)b); return 0;
    case Op::ReadShort: return ReadOr([&] { return (intptr_t)rb->ReadShort(); });
    case Op::ReadBool: return ReadOr([&] { return (intptr_t)rb->ReadBool(); });
    case Op::WriteNew: return (intptr_t)(new WriteBuffer(0, true));
    case Op::WriteFree: delete wb; return 0;
    case Op::AppendBytes: wb->AppendBytes(reinterpret_cast<const void*>(b), (unsigned long)c); return 0;
    case Op::AppendByte: wb->AppendByte((unsigned char)b); return 0;
    case Op::AppendShort: wb->AppendShort((short)b); return 0;
    case Op::SetShort: wb->SetShort((unsigned long)b, (short)c); return 0;
    case Op::ToByteBuffer: return (intptr_t)(HeldConst(wb->ToByteBuffer(true).get()));
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::TextOfUnits: return (intptr_t)(Held(new GUIStringData(UnitsText(reinterpret_cast<const uint16_t*>(a), (size_t)b))));
    case Op::TextOfBytes: return (intptr_t)(Held(new GUIStringData(reinterpret_cast<const char*>(a))));
    case Op::TextEmpty: return (intptr_t)(GetEmptyGUIStringData());
    case Op::TextLength: return (intptr_t)(reinterpret_cast<GUIStringData*>(a)->mLength);
    case Op::GameType: return (intptr_t)(g->GetGameType());
    case Op::GameAddressSixByte: memcpy(reinterpret_cast<void*>(a), g->GetAddress().GetSixByte(), 6); return 0;
    case Op::ProductId: return (intptr_t)(GetLobbyProductId());
    case Op::InProgress: return (g->IsInProgress()) ? 1 : 0;
    case Op::HasPassword: return (g->GetPassword().length() != 0) ? 1 : 0;
    case Op::InviteOnly: return (g->GetInviteOnly()) ? 1 : 0;
    case Op::AskToJoin: return (g->GetAskToJoin()) ? 1 : 0;
    case Op::Skill: return (intptr_t)(g->GetSkill());
    case Op::NumPlayers: return (intptr_t)(g->GetNumPlayers());
    case Op::MaxPlayers: return (intptr_t)(g->GetMaxPlayers());
    case Op::NameUtf16: SinkBytes(a, Utf16LE((std::wstring)g->GetName())); return 0;
    case Op::WriteSummaryHook: g->WriteSummaryHook(*wb); return 0;
    case Op::StoreSummary: g->M(LobbyMC::GameStoreSummary, a, b); return 0;
    case Op::ReadSummaryHook: return ReadOr([&] { return (intptr_t)g->ReadSummaryHook(*rb); });
    case Op::ReadDetailsHook: return ReadOr([&] { return (intptr_t)g->ReadDetailsHook(*rb); });
    case Op::SetHaveDetails: g->MSet(LobbyMF::GameHaveDetails, true); return 0;
    case Op::Ping: return (intptr_t)(g->GetPing());
    case Op::JoinRequestHook: g->GetJoinGameRequestHook(*wb); return 0;
    case Op::IsFull: return (g->IsFull()) ? 1 : 0;
    case Op::ConfigGet: return (intptr_t)(GetLobbyConfig());
    case Op::ConfigAllowDuplicates: return (reinterpret_cast<LobbyConfig*>(a)->mAllowDuplicateNames) ? 1 : 0;
    case Op::ClientsGet: return (intptr_t)(Held(g->GetClients().get()));
    case Op::ListCount: return (intptr_t)(reinterpret_cast<LobbyClientList*>(a)->GetCount());
    case Op::ListAt: return (intptr_t)(reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b));
    case Op::ClientSameName: return (reinterpret_cast<LobbyClient*>(a)->GetName() == reinterpret_cast<LobbyClient*>(b)->GetName()) ? 1 : 0;
    case Op::PlayerCreate: return (intptr_t)(Held(LobbyPlayer::CreatePlayer()));
    case Op::ClientSetPlayer: reinterpret_cast<LobbyClient*>(a)->SetPlayer(reinterpret_cast<LobbyPlayer*>(b)); return 0;
    case Op::PlayerSetPing: reinterpret_cast<LobbyPlayer*>(a)->SetPing((unsigned short)b); return 0;
    case Op::JoinRequestHookRead: return ReadOr([&] { return (intptr_t)g->HandleJoinGameRequestHook(*rb, reinterpret_cast<LobbyClient*>(b)); });
    case Op::SetNumPlayers: g->MSet(LobbyMF::GameNumPlayers, (long)a); return 0;
    case Op::IterNew: return (intptr_t)(new LobbyClientSetIterator(reinterpret_cast<LobbyClientList*>(a)));
    case Op::IterHasMore: return (reinterpret_cast<LobbyClientSetIterator*>(a)->HasMore()) ? 1 : 0;
    case Op::IterNext: return (intptr_t)(reinterpret_cast<LobbyClientSetIterator*>(a)->Next(reinterpret_cast<LobbyClient**>(b)));
    case Op::IterFree: delete reinterpret_cast<LobbyClientSetIterator*>(a); return 0;
    case Op::ClientId: return (intptr_t)(reinterpret_cast<LobbyClient*>(a)->GetClientId());
    case Op::PlayerWrite: reinterpret_cast<LobbyPlayer*>(a)->WriteToBuffer(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case Op::JoinReplyHook: g->GetJoinGameReplyHook(reinterpret_cast<LobbyClient*>(a), *reinterpret_cast<WriteBuffer*>(b)); return 0;
    case Op::HandleJoinRequest: return (intptr_t)(g->HandleJoinRequest(*rb, reinterpret_cast<LobbyClient*>(b), *reinterpret_cast<WriteBuffer*>(c)));
    case Op::FindClient: return (intptr_t)(g->FindClient((unsigned short)a));
    case Op::PlayerReadFrom: return (reinterpret_cast<LobbyPlayer*>(a)->ReadFromBuffer(*reinterpret_cast<ReadBuffer*>(b))) ? 1 : 0;
    case Op::JoinReplyHookRead: return ReadOr([&] { g->HandleJoinGameReplyHook(*rb); return (intptr_t)0; });
    case Op::StatusSuccess: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_Success_String)));
    case Op::StatusGameFull: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_GameFull_String)));
    case Op::StatusGameInProgress: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_GameInProgress_String)));
    case Op::StatusUnpackFailure: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_UnpackFailure_String)));
    case Op::StatusNotInvited: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_NotInvited_String)));
    case Op::StatusCaptainRejectedYou: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_CaptainRejectedYou_String)));
    case Op::StatusInvalidPassword: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_InvalidPassword_String)));
    case Op::StatusDuplicateName: return (intptr_t)(HeldText(LOBBY_STRING(LobbyGameStatus_DuplicateName_String)));
    case Op::StatusHook: return (intptr_t)(HeldText(g->GetStatusStringHook((short)a)));
    case Op::PlayerReadData: return ReadOr([&] { return (intptr_t)pl->ReadData(*rb); });
    case Op::SelfSetPing: pl->SetPing((unsigned short)a); return 0;
    case Op::SelfSetReady: pl->SetReady(a != 0); return 0;
    case Op::PlayerPing: return (intptr_t)(pl->GetPing());
    case Op::PlayerReady: return (pl->GetReady()) ? 1 : 0;
    case Op::PlayerWriteData: pl->WriteData(*wb); return 0;
    }
    return 0;
}

static intptr_t ChatLinkHost(void* inLink, EeWonChatLinkOp inOp, intptr_t a)
{
    typedef EeWonChatLinkOp Op;
    ChatLink* k = static_cast<ChatLink*>(inLink);
    switch (inOp) {
    case Op::SetClient: k->mClient = (LobbyClient*)a; return 0;
    case Op::TextClientName: k->mText = k->mClient->GetName(); return 0;
    case Op::SetText: k->mText = Txt(a); return 0;
    case Op::SetGroupId: k->mGroupId = (unsigned short)a; return 0;
    case Op::AddressOfServer: k->mAddress = ((LobbyServer*)a)->GetAddress(); return 0;
    case Op::SetFriend: k->mFriend = (LobbyFriend*)a; return 0;
    case Op::TextFriendName: k->mText = k->mFriend->GetName(); return 0;
    case Op::AddressClone: k->mAddress = IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)a)); return 0;
    case Op::SetColor: k->mColor = (int)a; return 0;
    case Op::SetFont: k->mFont = (Font*)a; return 0;
    case Op::SetEnabled: k->mEnabled = true; return 0;
    case Op::ClearPrevious: k->mPrevious = NULL; return 0;
    }
    return 0;
}

static RegKey* sConfRegistry;
static DWORD sConfRegistryValue;
static std::string sConfName, sConfValue;

static intptr_t ConfHost(void* inConfig, EeWonLobbyConfOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyConfOp Op;
    LobbyConfig* c = static_cast<LobbyConfig*>(inConfig);
    switch (inOp) {
    case Op::KeyDirServer: c->KeyAdd("DirServer", c->mDirServers, NULL, true); return 0;
    case Op::KeyRoomDirServer: c->KeyAdd("RoomDirServer", c->mRoomDirServers, NULL, true); return 0;
    case Op::KeyAuthServers: c->KeyAdd("AuthServers", c->mAuthServers, NULL, true); return 0;
    case Op::KeyProductName: c->KeyAdd("ProductName", c->mProductName, NULL); return 0;
    case Op::KeyProductDirectory: c->KeyAdd("ProductDirectory", c->mProductDirectory, NULL); return 0;
    case Op::KeyResourceDirectory: c->KeyAdd("ResourceDirectory", c->mResourceDirectory, NULL); return 0;
    case Op::KeyVersion: c->KeyAdd("Version", c->mVersion, NULL); return 0;
    case Op::KeyLanVersion: c->KeyAdd("LanVersion", c->mLanVersion, NULL); return 0;
    case Op::KeyLanguage: c->KeyAdd("Language", c->mLanguage, ""); return 0;
    case Op::KeyCrcCheck: c->KeyAdd("CRCCheck", c->mCRCCheck, true); return 0;
    case Op::KeyCdKeyCheck: c->KeyAdd("CDKeyCheck", c->mCDKeyCheck, true); return 0;
    case Op::KeyPublic: c->KeyAdd("Public", c->mPublic, false); return 0;
    case Op::KeyLanBroadcastPort: c->KeyAdd("LanBroadcastPort", c->mLanBroadcastPort, 8585); return 0;
    case Op::KeyNoPingInProgress: c->KeyAdd("NoPingInProgress", c->mNoPingInProgress, true); return 0;
    case Op::KeyLobbyPort: c->KeyAdd("LobbyPort", c->mLobbyPort, 8888); return 0;
    case Op::KeyCheckForInternet: c->KeyAdd("CheckForInternet", c->mCheckForInternet, true); return 0;
    case Op::KeyPingCeiling: c->KeyAdd("PingCeiling", c->mPingCeiling, 1000); return 0;
    case Op::KeyAllowDuplicateNames: c->KeyAdd("AllowDuplicateNames", c->mAllowDuplicateNames, true); return 0;
    case Op::KeyAppendNumberToDefaultLanName: c->KeyAdd("AppendNumberToDefaultLanName", c->mAppendNumberToDefaultLanName, true); return 0;
    case Op::KeyAllowJoinGameWithoutPing: c->KeyAdd("AllowJoinGameWithoutPing", c->mAllowJoinGameWithoutPing, true); return 0;
    case Op::RegistryOpen: sConfRegistry = new RegKey(2, "SOFTWARE\\Sierra\\WONLobby", false, true); return 0;
    case Op::RegistryValueClear: sConfRegistryValue = 0; return 0;
    case Op::RegistryQuery: return (intptr_t)(sConfRegistry->QueryDWORD("UseLocalIP", sConfRegistryValue));
    case Op::RegistryValue: return (intptr_t)(sConfRegistryValue);
    case Op::SetUseLocalIp: c->mUseLocalIP = a != 0; return 0;
    case Op::KeyUseLocalIp: c->KeyAdd("UseLocalIP", gLobbyUseLocalIP, a != 0); return 0;
    case Op::RegistryClose: delete sConfRegistry; return 0;
    case Op::KeyIsLobbyDefine: return (*(const std::string*)a == "LOBBYDEFINE") ? 1 : 0;
    case Op::DefineLocalsClear: sConfName = sConfValue = std::string(); return 0;
    case Op::DefineReadName: ((StringParser*)a)->ReadString(sConfName, true); return 0;
    case Op::DefineReadValue: ((StringParser*)a)->ReadString(sConfValue, false); return 0;
    case Op::DefineNameEmpty: return (sConfName.empty()) ? 1 : 0;
    case Op::DefineStore: c->mDefines[sConfName] = sConfValue; return 0;
    case Op::BaseHandleValue: return WonGuard([&] { return c->ConfigObject::HandleValueInstruction((ConfigParser*)a, *(const std::string*)b, *(StringParser*)cc); });
    }
    return 0;
}

}

#endif

