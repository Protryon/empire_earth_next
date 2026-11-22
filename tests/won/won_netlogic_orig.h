// The Internet logics' host cases R6 WON round 32 moved to Rust (ee_won_lobby netlogicops.rs), as they were
// (copied by script from AccountLogic.cpp and InitLogic.cpp at af02fb9a5), with the control maps of their
// HandleControlEvent, for won_lobbyops_test over won_lobbyops_world.h's and won_netlogic_world.h's stand-ins.
// InitHost's config is the world's (its GetLobbyConfig() call per op is not traced on either side).
#ifndef WON_NETLOGIC_ORIG_H
#define WON_NETLOGIC_ORIG_H
namespace NlOrig {
using namespace LoWorld;
using namespace NlWorld;
static RustServerOp* R(intptr_t a) { return static_cast<RustServerOp*>(reinterpret_cast<AsyncOp*>(a)); }
static GUIString* T(intptr_t a) { return reinterpret_cast<GUIString*>(a); }
static intptr_t Account(AccountLogic* aLogic, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    typedef LobbyEvtField EF;
    (void)c; (void)d;
    switch (inOp) {
    case O::OutOfDateDialog:
        return LobbyDoYesNoDialog(LOBBY_STRING(LobbyContainer_VersionOutOfDataTitle_String),
                                  LOBBY_STRING(LobbyContainer_VersionOutOfDataDetails_String), true);
    case O::ShowCredits:
        aLogic->mCreditsDialog->LCall(EeWonLobbyDlgCall::ResetScrolling);
        aLogic->mCreditsDialog->LCall(EeWonLobbyDlgCall::StartScrolling);
        LobbyDoDialog(aLogic->mCreditsDialog.get());
        aLogic->mCreditsDialog->LCall(EeWonLobbyDlgCall::StopScrolling);
        return 0;
    case O::ShowTOU:
        aLogic->mTOUDialog->LCall(EeWonLobbyDlgCall::Reset);
        return LobbyDoDialog(aLogic->mTOUDialog.get());
    case O::ShowScreen:
        if (a == 1) LobbyShowCreateAccount();
        else if (a == 2) LobbyShowLostPassword();
        else LobbyShowLostUsername();
        return 0;
    case O::RequestPasswordEmail: {
        // RequestPasswordEmailOp: a Rust op (ee_won adminop.rs)
        std::string aName;
        WideAppendUtf16LE(aName, LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(a), EF::UserName));
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_password_email(reinterpret_cast<const uint8_t*>(aName.data()), aName.size()),
            reinterpret_cast<ServerContext*>(b));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_RetrievePasswordTitle_String),
                             LOBBY_STRING(LobbyContainer_RetrievePasswordDetails_String), 0, 0);
        return 0;
    }
    case O::RequestInfoEmail: {
        // RequestAccountInfoEmailOp: a Rust op (ee_won adminop.rs)
        const std::string aEmail = LobbyEvtStr<std::string>(reinterpret_cast<ComponentEvent*>(a), EF::Email);
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_info_email(reinterpret_cast<const uint8_t*>(aEmail.data()), aEmail.size()),
            reinterpret_cast<ServerContext*>(b));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_RetrieveUsernameTitle_String),
                             LOBBY_STRING(LobbyContainer_RetrieveUsernameDetails_String), 0, 0);
        return 0;
    }
    case O::QueryAccount: {
        SmartPtr<RustServerOp> op = RustServerOp::NewGetAccount(reinterpret_cast<ServerContext*>(a),
                                                                reinterpret_cast<AuthContext*>(b));
        op->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1);
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_QueryAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_QueryAccountDetails_String), 1, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::UpdateAccount: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(c);
        SmartPtr<RustServerOp> op = RustServerOp::NewUpdateAccount(reinterpret_cast<ServerContext*>(a),
            reinterpret_cast<AuthContext*>(b), LobbyEvtText(event, EF::NewPassword), LobbyEvtText(event, EF::Email));
        op->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1 | (LobbyEvtGet(event, EF::Newsletter) ? 0x10000 : 0));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_UpdateAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_UpdateAccountDetails_String), 0, 0);
        return 0;
    }
    case O::CreateAccount: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(b);
        SmartPtr<RustServerOp> op = RustServerOp::NewCreateAccount(reinterpret_cast<ServerContext*>(a),
            LobbyEvtStr<std::wstring>(event, EF::UserName), LobbyEvtStr<std::wstring>(event, EF::Password),
            LobbyEvtStr<std::string>(event, EF::Email), LobbyEvtGet(event, EF::Newsletter) != 0,
            (unsigned char)LobbyEvtGet(event, EF::Month), (unsigned char)LobbyEvtGet(event, EF::Day),
            (unsigned short)LobbyEvtGet(event, EF::Year));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_CreateAccountTitle_String),
                             LOBBY_STRING(LobbyContainer_CreateAccountDetails_String), 0, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::AccountCreated: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        LobbyUserInfo* user = LobbyAddUser(LobbyEvtStr<std::wstring>(event, EF::UserName), "", false);
        if (user) {
            long aSys, aGame;
            LobbyGetTOUTimes(aSys, aGame);
            LobbyUserSetTOU(user, aSys, aGame);
        }
        LobbySaveGlobalPrefs(true);
        LobbyBroadcastLobbyEvent(100052, true);
        LobbyBroadcastEvent(SetLoginFieldsEvent(GUIString(LobbyEvtStr<std::wstring>(event, EF::UserName)),
                                                GUIString(LobbyEvtStr<std::wstring>(event, EF::Password)), false), true);
        return 0;
    }
    case O::LoginName: {
        // the event's own copy (lobbyevt.rs), live while the event is
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        *reinterpret_cast<size_t*>(b) = LobbyEvtStr<std::wstring>(event, EF::UserName).length();
        return LobbyEvtGet(event, EF::UserName);
    }
    case O::LoginStripAt: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        LobbyEvtSetStr(event, EF::UserName, LobbyEvtStr<std::wstring>(event, EF::UserName).substr(1));
        return 0;
    }
    case O::UserNeedsTOU: {
        LobbyUserInfo* user;
        {
            GUIString name(new GUIStringData(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(a), EF::UserName)));
            user = LobbyFindUser(name);
        }
        return !user || LobbyIsTOUNewer(LobbyUserTOU(user, 0), LobbyUserTOU(user, 1));
    }
    case O::SetCredentials: {
        AuthContext* auth = reinterpret_cast<AuthContext*>(a);
        auth->SetUserName(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(b), EF::UserName));
        auth->SetPassword(LobbyEvtStr<std::wstring>(reinterpret_cast<ComponentEvent*>(b), EF::Password));
        return 0;
    }
    case O::InvalidCDKeyBox:
        LobbyMessageBox(LOBBY_STRING(AccountLogic_InvalidCDKeyTitle_String), LOBBY_STRING(AccountLogic_InvalidCDKeyDetails_String));
        return 0;
    case O::SetAdmin:
        if (b) reinterpret_cast<AuthContext*>(a)->AddCommunity(std::wstring(L"Admin"));
        else reinterpret_cast<AuthContext*>(a)->RemoveCommunity(std::wstring(L"Admin"));
        return 0;
    case O::GetCert: {
        SmartPtr<GetCertOp> op = new GetCertOp(reinterpret_cast<AuthContext*>(a));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_LoginTitle_String), LOBBY_STRING(LobbyContainer_LoginDetails_String), 1, 0);
        op->AddRef();
        return (intptr_t)static_cast<AsyncOp*>(op.get());
    }
    case O::LoginSaveUser: {
        ComponentEvent* event = reinterpret_cast<ComponentEvent*>(a);
        GUIString password(GetEmptyGUIStringData());
        if (LobbyEvtGet(event, EF::Remember)) password = LobbyEvtStr<std::wstring>(event, EF::Password);
        LobbyUserInfo* user = LobbyAddUser(LobbyEvtStr<std::wstring>(event, EF::UserName), password, false);
        if (user) {
            long aSys, aGame;
            LobbyGetTOUTimes(aSys, aGame);
            LobbyUserSetTOU(user, aSys, aGame);
            LobbySaveGlobalPrefs(true);
            LobbySetCurrentUser(user);
        }
        return 0;
    }
    default: return INTPTR_MIN;
    }
}
static intptr_t Init(InitLogic* aLogic, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    LobbyConfig* config = sConfig;
    switch (inOp) {
    case O::InitStatus: {
        const GUIString& aDescription = b != 0 ? *T(b) : c >= 0 ? gLobbyResources[c].mString : gEmptyGUIString;
        LobbyBroadcastEvent(LoginInitStatusEvent(gLobbyResources[a].mString, aDescription, (d & 1) != 0, (int)(d >> 8)), true);
        return 0;
    }
    case O::ClearServerLists: {
        ServerContext* account = LobbyGetAccountServers();
        ServerContext* auth = LobbyGetAuthServers();
        ServerContext* firewall = LobbyGetFirewallDetectServers();
        ServerContext* routing = LobbyGetPatchServers();
        if (account != NULL) account->Clear();
        if (auth != NULL) auth->Clear();
        if (firewall != NULL) firewall->Clear();
        if (routing != NULL) routing->Clear();
        return 0;
    }
    case O::LoadTOUTimes: {
        WONFile file(LobbyGetSysTOUPath());
        long systemTime = file.GetModifyTime();
        file.SetFilePath(LobbyGetGameTOUPath());
        long gameTime = file.GetModifyTime();
        LobbySetTOUTimes(systemTime, gameTime);
        return 0;
    }
    case O::Music:
        if (a == 0) LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->SetLooping(true);
        else if (a == 1) LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Play();
        else LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop();
        return 0;
    case O::DnsReady:
        return config != NULL && LobbyGetDirServers() != NULL && LobbyGetRoomDirServers() != NULL && LobbyGetAuthServers() != NULL;
    case O::FillDnsServers: {
        // the original's address with mRememberHostString set (separate from the loops' addresses)
        IPAddr address;
        address.SetRememberHostString(true);
        LobbyGetDirServers()->RemoveAllAddresses();
        InitAddAll(LobbyGetDirServers(), config->mDirServers);
        InitAddAll(LobbyGetRoomDirServers(), config->mRoomDirServers);
        InitAddAll(LobbyGetAuthServers(), config->mAuthServers);
        return 0;
    }
    case O::PromotionImage: {
        SmartPtr<NativeImage> image = Window::GetDefaultWindow()->DecodeImage(R(a)->GetBytes(EE_WON_HTTPGET_LOCAL_PATH).c_str());
        LobbyBroadcastEvent(SetCrossPromotionEvent(image.get(), aLogic->mCrossPromotion), true);
        return 0;
    }
    case O::FetchPromotionImage: {
        SmartPtr<RustServerOp> op = RustServerOp::NewHttpGet(std::string(reinterpret_cast<const char*>(a), (size_t)b));
        // HTTPCache::GetGlobalCache(): the Rust cache in the WON directory (ee_won httpcache.rs), the op's reference
        const std::string& aDir = WONAPICore::GetDefaultFileDirectory();
        ee_won_hostop_set(op->GetRustOp(), EE_WON_HTTPGET_CACHE, 0, NULL, 0, NULL,
                          ee_won_http_cache_global(reinterpret_cast<const uint8_t*>(aDir.data()), aDir.size()));
        op->SetCompletion(new LogicCompletion<InitLogic>(&InitLogic::CrossPromotionImageCompletion, aLogic));
        aLogic->mHTTPSession->AddOp(op.get());
        return 0;
    }
    case O::DirReady:
        return config != NULL && LobbyGetAccountServers() != NULL && LobbyGetAuthServers() != NULL &&
               LobbyGetFirewallDetectServers() != NULL && LobbyGetDirServers() != NULL;
    case O::AddDirAddresses: {
        ServerContext* aList = InitList(a);
        if (aList != NULL)
            aList->AddAddressesFromDir(R(b)->GetEntities(EE_WON_REQOP_DIR_LIST, c), NULL);
        return 0;
    }
    case O::VersionNotifyFailed: {
        ServerContext* versions = LobbyGetPatchServers();
        if (versions != NULL)
            versions->NotifyFailed(R(a)->GetCurAddr());
        return 0;
    }
    case O::RestartAccept:
        aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(a))->GetSocket());
        aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic),
                      gLobbyAcceptTimeout);
        return 0;
    case O::StartAccept:
        aLogic->mFirewallAddress = static_cast<DetectFirewallOp*>(reinterpret_cast<AsyncOp*>(a))->GetCurAddr();
        aLogic->mAcceptOp = new SocketOp(EeWonCoreSocketOp::Accept, aLogic->mFirewallSocket.get());
        aLogic->RunOp(aLogic->mAcceptOp.get(), new LogicCompletion<InitLogic>(&InitLogic::AcceptCompletion, aLogic),
                      gLobbyAcceptTimeout);
        aLogic->mFirewallSocket = NULL;
        return 0;
    case O::RunDnsTest: {
        SmartPtr<DNSTestOp> op = new DNSTestOp(config->mDirServers.front());
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DNSCompletion, aLogic), 0);
        return 0;
    }
    case O::StartDocuments:
        if (config != NULL) {
            static const int kTypes[3] = {HTTPDocType_MOTD, HTTPDocType_TOU, HTTPDocType_ADV};
            SmartPtr<RustServerOp> ops[3];
            for (int i = 0; i < 3; i++) {
                ops[i] = RustServerOp::NewHttpDocument(config->mProductName, kTypes[i]);
                ops[i]->SetBytes(EE_WON_HTTPDOC_CONFIG, config->mLanguage);
                ops[i]->Set(EE_WON_HTTPDOC_SESSION, aLogic->mHTTPSession.get());
                aLogic->RunTrackedOp(ops[i].get(), new LogicCompletion<InitLogic>(&InitLogic::HTTPDocumentCompletion, aLogic), 0);
            }
        }
        return 0;
    case O::RunDirQuery: {
        std::wstring paths[4] = { L"/TitanServers/Auth", L"/TitanServers/DBProxy", L"/TitanServers/Firewall" };
        if (config != NULL)
            paths[3] = config->mProductDirectory + L"/Patch";
        SmartPtr<RustServerOp> op = RustServerOp::NewGetMultiDir(reinterpret_cast<ServerContext*>(a), 0x04000002, paths,
                                                                 config != NULL ? 4 : 3);
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::DirectoryCompletion, aLogic), 0);
        return 0;
    }
    case O::RunVersionCheck: {
        // CheckValidVersionOp: a Rust op (ee_won reqops.rs) with the product, config and version
        const std::string& aProduct = config->mProductName;
        const std::string& aConfig = config->mLanguage;
        const std::string& aVersion = config->mVersion;
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(ee_won_reqop_version_check(
            reinterpret_cast<const uint8_t*>(aProduct.data()), aProduct.size(), reinterpret_cast<const uint8_t*>(aConfig.data()),
            aConfig.size(), reinterpret_cast<const uint8_t*>(aVersion.data()), aVersion.size()), LobbyGetPatchServers());
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::VersionCompletion, aLogic), 0);
        return 0;
    }
    case O::RunFirewallDetect: {
        SmartPtr<DetectFirewallOp> op = new DetectFirewallOp(reinterpret_cast<ServerContext*>(a));
        op->SetListenPort(LobbyGetLobbyPort());
        op->SetDoListen(false);
        op->SetWaitForConnect(false);
        aLogic->RunTrackedOp(op.get(), new LogicCompletion<InitLogic>(&InitLogic::FirewallCompletion, aLogic), 0);
        return 0;
    }
    case O::KillAcceptOp:
        if (aLogic->mAcceptOp.get() != NULL) {
            aLogic->mAcceptOp->Kill(true);
            if (aLogic->mAcceptOp->GetSocket() != NULL)
                aLogic->mAcceptOp->GetSocket()->Close();
            aLogic->mAcceptOp = NULL;
        }
        return 0;
    case O::AdapterDialog:
        aLogic->mNetworkAdapterDialog->LCall(EeWonLobbyDlgCall::Reset);
        return LobbyDoDialog(aLogic->mNetworkAdapterDialog.get());
    default: return INTPTR_MIN;
    }
}
// the HandleControlEvent choices
static int AccountControl(int inControlId) { return inControlId == ID_ShowCreateAccount ? 1 : inControlId == ID_ShowLostPassword ? 2 : inControlId == ID_ShowLostUserName ? 3 : 0; }
static int InitControl(int inControlId) { return inControlId == ID_LoginScreenBack ? 1 : inControlId == ID_ShowInternetScreen ? 2 : inControlId == ID_ChooseNetworkAdapter ? 3 : 0; }
namespace Route {
static const GUIString& TX(intptr_t a) { return a != 0 ? *T(a) : gEmptyGUIString; }
static LobbyServer* S(intptr_t a) { return reinterpret_cast<LobbyServer*>(a); }
static LobbyGroup* G(intptr_t a) { return reinterpret_cast<LobbyGroup*>(a); }
static LobbyClient* C(intptr_t a) { return reinterpret_cast<LobbyClient*>(a); }
static LobbyGame* GM(intptr_t a) { return reinterpret_cast<LobbyGame*>(a); }
static RoutingOp* RO(intptr_t a) { return reinterpret_cast<RoutingOp*>(a); }
static RoutingConnection* RC(intptr_t a) { return reinterpret_cast<RoutingConnection*>(a); }
static AsyncOp* AO(intptr_t a) { return reinterpret_cast<AsyncOp*>(a); }
static RustServerOp* RS(intptr_t a) { return static_cast<RustServerOp*>(AO(a)); }
static RoutingGetUserStatusOp* US(intptr_t a) { return static_cast<RoutingGetUserStatusOp*>(AO(a)); }
template <class X> static intptr_t Held(const SmartPtr<X>& inPtr)
{
    if (inPtr.get() != NULL) inPtr->AddRef();
    return (intptr_t)inPtr.get();
}
static intptr_t Route(RoutingLogic* L, EeWonRouteLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t e)
{
    typedef EeWonRouteLogicOp O;
    (void)d; (void)e;
    switch (inOp) {
    case O::ListFree: // (kind 1, the data objects, is Rust's)
        if (b == 2) delete reinterpret_cast<DirEntityList*>(a);
        else delete reinterpret_cast<LobbyClientIterator*>(a);
        return 0;
    // ---- the lobby
    case O::Pref:
        switch (a) { // 1..7: the saved settings' (persistobj.rs route_op)
        case 8: return LobbyOnInternetScreen();
        case 9: return LobbyUserCanceledLastStatusOp();
        }
        return 0;
    case O::ConnDo:
        switch (b) {
        case 1: RC(a)->SetAuth(reinterpret_cast<AuthContext*>(c), AUTH_TYPE_PERSISTENT); return 0;
        case 2: {
            // the notices' op types and codes (routelogic.rs N_*), in the original's order
            static const int kTypes[20][2] = {{8, 1}, {10, 2}, {7, 3}, {9, 4}, {39, 5}, {40, 6}, {6, 7}, {36, 8},
                                              {24, 9}, {25, 10}, {23, 11}, {13, 12}, {14, 13}, {27, 14}, {28, 15},
                                              {58, 16}, {59, 17}, {48, 18}, {50, 19}, {49, 20}};
            for (int i = 0; i < 20; i++)
                RC(a)->SetOpCompletion(kTypes[i][0], new ::RoutingOpCompletion(kTypes[i][1], L));
            return 0;
        }
        case 3: RC(a)->SetCloseCompletion(new LogicCompletion<RoutingLogic>(&RoutingLogic::ConnectionClosed, L)); return 0;
        case 4: // SubscribeDataObject(0xffff, L->mGameDataType, false, new RoutingOpCompletion(21, L))
            RoutingRequestValues().N(1).N(0xffff).S(L->mGameDataType).N(false)
                .Run(RC(a), EE_WON_RTQ_READ_DATA, new ::RoutingOpCompletion(21, L));
            return 0;
        case 5: return RC(a)->IsConnected();
        }
        return 0;
    case O::ConnAdvertise: {
        WriteBuffer aBuffer(0, true);
        GM(b)->WriteSummary(aBuffer);
        // CreateDataObject(true, c, L->mGameDataType, gEmptyWideString, the summary, NULL)
        RoutingRequestValues().N(true).N((unsigned short)c).S(L->mGameDataType).W(gEmptyWideString)
            .D(aBuffer.ToByteBuffer(true).get()).Run(RC(a), EE_WON_RTQ_CREATE_DATA, NULL);
        return 0;
    }
    case O::EventNum: {
        ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
        // 1..12, the members, are read in Rust (lobbyevt.rs route_event_num)
        return b == 13 ? (intptr_t)anEvent->mComponent : 0;
    }
    case O::EventServer: {
        ComponentEvent* anEvent = reinterpret_cast<ComponentEvent*>(a);
        switch (anEvent->mType) {
        case LobbyEvent_CreateRoom:
        case LobbyEvent_JoinRoom:
        case LobbyEvent_QueryServerGroups:
            return (intptr_t)L->mServerList->FindServer(LobbyEvtAddr(anEvent, LobbyEvtField::Address));
        }
        return 0;
    }
    case O::EvAlert:
        Broadcast(AlertEvent(4, (int)a, a == 1 ? (unsigned long)RO(b)->Num(EE_WON_RTF_TIME) : 0,
                                 GUIString(new GUIStringData(RO(b)->Text(EE_WON_RTT_TEXT)))));
        return 0;
    case O::EvModerated:
        Broadcast(ClientModeratedEvent(C(a), (int)b, (d & 0x100) ? G(c)->GetName() : S(c)->GetName(), (int)(d & 0xff),
                                           (unsigned long)RO(e)->Num(EE_WON_RTF_TIME), true,
                                           GUIString(RO(e)->Text(EE_WON_RTT_COMMENT))));
        return 0;
    case O::StatusOpNew: {
        RoutingGetUserStatusOp* anOp =
            new RoutingGetUserStatusOp(L->mServerList->AddressAt((size_t)a), reinterpret_cast<UDPManager*>(b));
        anOp->AddRef();
        return (intptr_t) static_cast<AsyncOp*>(anOp);
    }
    case O::StatusOpSetMap: {
        RoutingGetUserStatusOp::FriendStatusMap aMap;
        const uint32_t* anIds = reinterpret_cast<const uint32_t*>(b);
        for (intptr_t i = 0; i < c; i++) aMap[anIds[i]] = 0;
        US(a)->SetFriendStatusMap(aMap);
        return 0;
    }
    case O::StatusOpMap: {
        RoutingGetUserStatusOp::FriendStatusMap aMap = US(a)->GetFriendStatusMap();
        for (RoutingGetUserStatusOp::FriendStatusMap::iterator i = aMap.begin(); i != aMap.end(); ++i) {
            ee_won_routelogic_push(reinterpret_cast<void*>(b), i->first);
            ee_won_routelogic_push(reinterpret_cast<void*>(b), i->second);
        }
        return 0;
    }
    case O::UserIdLookup: {
        // GetUserIdFromNameOp: a Rust op (ee_won reqops.rs) with the name, its id read back
        std::wstring aWide = *T(b);
        std::string aName;
        WideAppendUtf16LE(aName, aWide);
        SmartPtr<RustServerOp> op = RustServerOp::NewRequest(
            ee_won_reqop_user_id(reinterpret_cast<const uint8_t*>(aName.data()), aName.size()),
            reinterpret_cast<ServerContext*>(a));
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(AddFriendDialog_StatusTitle_String),
                             LOBBY_STRING(AddFriendDialog_StatusDetails_String), 1, 0);
        intptr_t aUserId = (intptr_t)(unsigned long)ee_won_hostop_num(op->GetRustOp(), EE_WON_REQOP_USER_ID, 0);
        *reinterpret_cast<int*>(c) = (int)op->GetStatus();
        return aUserId;
    }
    case O::GroupQueryRun: {
        SmartPtr<RustServerOp> op = RustServerOp::NewGroupListReq(S(a)->GetAddress(), 0x29);
        L->RunTrackedOp(op.get(), new LogicCompletion<RoutingLogic>(&RoutingLogic::QueryServerGroupsDone, L), 0);
        return 0;
    }
    case O::DirOpRun: {
        static const char* const kTypes[] = {"_C", "_L"};
        SmartPtr<RustServerOp> op = RustServerOp::NewGetDir(reinterpret_cast<ServerContext*>(a),
                                                            GetLobbyConfig()->mProductDirectory, 0x04000622, kTypes, 2);
        LobbyRunOpWithStatus(op.get(), LOBBY_STRING(LobbyContainer_GettingRoomsTitle_String),
                             LOBBY_STRING(LobbyContainer_GettingRoomsDetails_String), 1, 0);
        return Held(op);
    }
    case O::DirEntities: {
        DirEntityList* aList = new DirEntityList(RS(a)->GetEntities(EE_WON_REQOP_DIR_LIST));
        *reinterpret_cast<intptr_t*>(b) = aList->empty();
        return (intptr_t)aList;
    }
    case O::ConnectOpNew: {
        ServerConnectionConnectOp* anOp = new ServerConnectionConnectOp(RC(a), S(b)->GetAddress());
        anOp->AddRef();
        return (intptr_t)anOp;
    }
    // ---- the parser, the events, the dialogs
    case O::RNew: { RoutingOp* anOp = new RoutingOp(RC(a), (RoutingOpType)b); anOp->AddRef(); return (intptr_t)anOp; }
    default: return INTPTR_MIN;
    }
}
static int RouteControl(int controlId) { return controlId == ID_ChangeRoom ? 1 : controlId == ID_AddFriend ? 2 : controlId == ID_AnonymousToFriendsCheck ? 3 : controlId == ID_FilterDirtyWordsCheck ? 4 : controlId == ID_RefreshFriends ? 5 : controlId == ID_RefreshGameList ? 6 : 0; }
} // namespace Route
} // namespace NlOrig
#endif
