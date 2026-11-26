#include "rust/won_lobby/wonnet.h"
#include "rust/won_net/certhost.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/smartroute.h"
// The WON front end's host (R6 WON round 31): every call the Rust EEWonNet (ee_won_lobby wonnet.rs) makes on a C++
// object, one statement each, in the original's order (EeWonNetOp documents them). Included by EEWonNet.cpp and by
// tests/won/won_wonnet_test.cpp (over its stand-ins), so both run these very statements. Needs, in scope: EEWonNet,
// the completions' `Completion(call, routing)`, `WonRequests`, the statics `sEntities`, `sEntity`, `sFile`,
// `sProtocol`, `sRoomLine`, `sUpdateOp`, the sinks `WideOut`, `BytesOut`, `TextOut`, `DataOut`, `KeyValid`, and the
// game's globals the statements name.
//
// R6 WON lobby and network round 35: the C++ objects the class held are the Rust object's slots (EeWonNetObjects,
// `o` below); the counted ones are held as their RefCount, the others as their own class.
static RefCount* Ref(void* inSlot) { return static_cast<RefCount*>(inSlot); }
static SmartRoutingConnection* Routing(EeWonNetObjects* o) { return static_cast<SmartRoutingConnection*>(Ref(o->routing)); }
static ServerContext* DirServers(EeWonNetObjects* o) { return static_cast<ServerContext*>(Ref(o->dir_servers)); }
static RustServerOp* LoginOp(EeWonNetObjects* o) { return static_cast<RustServerOp*>(Ref(o->login_op)); }
static RustServerOp* DirOp(EeWonNetObjects* o) { return static_cast<RustServerOp*>(Ref(o->dir_op)); }
static RustServerOp* AccountOp(EeWonNetObjects* o) { return static_cast<RustServerOp*>(Ref(o->account_op)); }
static DirList* Servers(EeWonNetObjects* o) { return static_cast<DirList*>(o->servers); }
static TSCriticalSection*& Lock(void*& ioSlot) { return reinterpret_cast<TSCriticalSection*&>(ioSlot); }
static WONAPICore* Lobby(EeWonNetObjects* o) { return static_cast<WONAPICore*>(o->lobby); }
static AuthContext* User(EeWonNetObjects* o) { return static_cast<AuthContext*>(o->user); }
static ServerContext* AuthServers(EeWonNetObjects* o) { return static_cast<ServerContext*>(o->auth_servers); }
static intptr_t WonNetHostCall(EEWonNet* w, EeWonNetOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonNetOp Op;
    EeWonNetObjects* o = ee_won_wonnet_objects(w->mRust);
    switch (inOp) {
    case Op::LockCreate: ESVERIFY_AT(TSScheduler::CreateCriticalSection(Lock(o->lock)) == keseOK, 195); return 0;
    case Op::Lock8Create: ESVERIFY_AT(TSScheduler::CreateCriticalSection(Lock(o->lock8)) == keseOK, 197); return 0;
    case Op::LockDestroy: TSScheduler::DestroyCriticalSection(Lock(o->lock)); return 0;
    case Op::Lock8Destroy: TSScheduler::DestroyCriticalSection(Lock(o->lock8)); return 0;
    case Op::LockEnter: Lock(o->lock)->Enter(); return 0;
    case Op::LockLeave: Lock(o->lock)->Leave(); return 0;
    case Op::Lock8Enter: Lock(o->lock8)->Enter(); return 0;
    case Op::Lock8Leave: Lock(o->lock8)->Leave(); return 0;
    case Op::LobbyDelete: delete Lobby(o); return 0;
    case Op::LobbyPump: Lobby(o)->Pump((unsigned long)a, 0); return 0;
    case Op::WonNetExists: return gWonNet != NULL;
    case Op::ClientIdSet: gWonClientId = (unsigned long)a; return 0;
    case Op::RoutingNew: return (intptr_t)static_cast<RefCount*>(new SmartRoutingConnection);
    case Op::RoutingKill: Routing(o)->Kill(); return 0;
    case Op::RoutingSetAuth: Routing(o)->SetAuth(User(o), AUTH_TYPE_PERSISTENT); return 0;
    case Op::RoutingSetConnectCompletion: Routing(o)->SetConnectCompletion(Completion(a, false)); return 0;
    case Op::RoutingSetCloseCompletion: Routing(o)->SetCloseCompletion(Completion(a, false)); return 0;
    case Op::RoutingSetOpCompletion: Routing(o)->SetOpCompletion((int)a, Completion(b, true)); return 0;
    case Op::RoutingSetDefaultOpCompletion: Routing(o)->SetDefaultOpCompletion(Completion(a, true)); return 0;
    case Op::RoutingClearProductName: Routing(o)->ClearProductName(); return 0;
    case Op::RoutingConnect: Routing(o)->ConnectAsync(*(IPAddr*)a, 15000); return 0;
    case Op::RoutingMyClientId: return Routing(o)->GetMyClientId();
    case Op::RoutingIgnoreClient: Routing(o)->IgnoreClient((unsigned short)a, b != 0); return 0;
    case Op::DataLock: PlatMutexLock(Routing(o)->GetDataLock()); return 0;
    case Op::DataUnlock: PlatMutexUnlock(Routing(o)->GetDataLock()); return 0;
    case Op::StartGroupItr: Routing(o)->StartGroupItr(); return 0;
    case Op::StartMemberItr: return Routing(o)->StartMemberItr((unsigned short)a);
    case Op::SmartGet: return ee_won_smart_get(Routing(o)->mModel, (EeWonSmartGet)a, (unsigned short)b, 0, (EeWonSmartRecord*)c);
    case Op::SmartFind: ee_won_smart_find(Routing(o)->mModel, (const wchar_t*)a, (size_t)b, (EeWonSmartRecord*)c, (bool*)d); return 0;
    case Op::OpStatus: return ((AsyncOp*)a)->GetStatus();
    case Op::OpNum: return (intptr_t)((RoutingOp*)(AsyncOp*)a)->Num((int)b);
    case Op::OpType: return ((RoutingOp*)(AsyncOp*)a)->GetType();
    case Op::OpText: return WideOut(((RoutingOp*)(AsyncOp*)a)->Text((int)b), c);
    case Op::OpRecipientsEmpty: return ((RoutingOp*)(AsyncOp*)a)->Recipients().empty();
    case Op::OpData: return DataOut(((RoutingOp*)(AsyncOp*)a)->GetData(), b);
    case Op::OpRecords: ee_won_routeop_records(((RoutingOp*)(AsyncOp*)a)->mRust, (int)b, ee_won_wonnet_record, (void*)c); return 0;
    case Op::OpSocketIsRouting: return ((SocketOp*)(AsyncOp*)a)->GetSocket() == Routing(o);
    case Op::OpPeerSocketIsRouting: return ((RustServerOp*)(AsyncOp*)a)->GetSocket() == Routing(o);
    case Op::OpOnRouting: return ((RoutingOp*)(AsyncOp*)a)->mConnection.get() == Routing(o);
    case Op::UserName: return WideOut(User(o)->GetName(), a);
    case Op::TextLoad: return TextOut((long)a, b);
    case Op::Wcstombs: return (intptr_t)wcstombs((char*)a, (const wchar_t*)b, (size_t)c);
    case Op::ShowMessage: gUIManager->DisplayMessage(UWideString((const char*)a), 1, NULL, NULL, true); return 0;
    case Op::ShowMessageLost: gUIManager->DisplayMessage(UWideString((const char*)a), 1, NULL, CloseMessageCallback, true); return 0;
    case Op::ShowMessageHost: gUIManager->DisplayMessage(UWideString((const char*)a), 1, reinterpret_cast<UIForm*>(w), HostGameCallback, true); return 0;
    case Op::ShowCancelConnect: gUIManager->DisplayCancel(UWideString((const char*)a), 1, (UIForm*)&o->cancelled, CancelConnectCallback, gUIManager->GetFormByID(b)); return 0;
    case Op::ShowCancelJoinRoom: gUIManager->DisplayCancel(UWideString((const char*)a), 1, (UIForm*)&o->join_room_cancelled, CancelConnectCallback, gUIManager->GetFormByID(b)); return 0;
    case Op::ShowCancelAccount: gUIManager->DisplayCancel(UWideString((const char*)a), 1, (UIForm*)AccountOp(o), CancelOpCallback, gUIManager->GetFormByID(b)); return 0;
    case Op::SavePreferences: gGamePreferences.Save(); return 0;
    case Op::SetHostedGameName: gHostedGameName = UWideString((const char*)a); return 0;
    case Op::FormById: return (intptr_t)gUIManager->GetFormByID(a);
    case Op::WideOut: ((std::wstring*)a)->assign((const wchar_t*)b, (size_t)c); return 0;
    case Op::CurrentForm: return (intptr_t)gUIManager->GetCurrentForm();
    case Op::FormControl: return (intptr_t)((UIForm*)a)->GetControl(b);
    case Op::CloseForm: gUserInterface->CloseForm((UIForm*)a); return 0;
    case Op::LockUi: gUIManager->LockUIForCopyData(); return 0;
    case Op::UnlockUi: gUIManager->UnlockUIForCopyData(); return 0;
    case Op::PopTo: gUIManager->PopTo((UIForm*)a); return 0;
    case Op::Push: gUIManager->Push((UIForm*)a); return 0;
    case Op::SwitchToForm: gUserInterface->SwitchToForm((UIForm*)a); return 0;
    case Op::NewRoomsOpenEvent: return (intptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::ArOpenScreen));
    case Op::NewGamesOpenEvent: return (intptr_t)static_cast<UIEvent*>(new EEUIEWonNetEvent(EeWonFrontCall::AgOpenScreen));
    case Op::FormSetOpenEvent: ((UIForm*)a)->SetFormOpenEvent((UIEvent*)b); return 0;
    case Op::EventDereference: ((UIEvent*)a)->Dereference(); return 0;
    case Op::CheckBoxChecked: return ((UICheckBox*)a)->GetIsChecked();
    case Op::CheckBoxSetChecked: ((UICheckBox*)a)->SetChecked(b != 0); return 0;
    case Op::EditBoxSetText: ((UIEditBox*)a)->SetText(UWideString((const char*)b)); return 0;
    case Op::LabelSetText: ((UITextLabel*)a)->SetText(UWideString((const char*)b)); return 0;
    case Op::NewJoinOrHost: return (intptr_t)static_cast<EEAction*>(new EEAJoinOrHostAGame(a, UWideString((const char*)b), UWideString((const char*)c)));
    case Op::NewWonAction: return (intptr_t)static_cast<EEAction*>(new EEAWonNet(reinterpret_cast<EeWonAction*>(a)));
    case Op::PostAction: gUserInterface->PostAction((EEAction*)a); return 0;
    case Op::ActionDereference: ((EEAction*)a)->Dereference(); return 0;
    case Op::PostLoginDone: gUserInterface->PostLoginDone(); return 0;
    case Op::RoomsBoxSelection: return ((EEUICWonNetRoomsBox*)a)->GetCurrentSelection(*(unsigned long*)b);
    case Op::RoomsBoxClear: ((EEUICWonNetRoomsBox*)a)->DeleteAllEntries(); return 0;
    case Op::RoomsBoxLineCount: return ((EEUICWonNetRoomsBox*)a)->GetLineCount();
    case Op::RoomsBoxSelect: ((EEUICWonNetRoomsBox*)a)->ChangeSelectionIndex((unsigned long)b, true, false); return 0;
    case Op::RoomsBoxAppend: ((EEUICWonNetRoomsBox*)a)->AppendLine(sRoomLine); return 0;
    case Op::RoomLineDescription: sRoomLine.mDescription = UWideString((const char*)a); return 0;
    case Op::RoomLineName: sRoomLine.mName = UWideString((const char*)a); return 0;
    case Op::RoomLinePlayers: sRoomLine.mNumPlayers = (long)a; return 0;
    case Op::RoomLineGames: sRoomLine.mNumGames = (long)a; return 0;
    case Op::ProtocolGet: NEProtocol::GetProtocol(0, sProtocol); return 0;
    case Op::ProtocolLocalAddress: return BytesOut(sProtocol->GetLocalAddress(gNetworkAdapter), a);
    case Op::ReqRegister: WonRequests(Routing(o)).Register(gEmptyWideString, gEmptyWideString, gWonClientId, 0, 5, (unsigned long)a, Completion(b, true)); return 0;
    case Op::ReqSubscribe: WonRequests(Routing(o)).SubscribeDataObject(0xffff, a ? kWonRoomObject : kWonGameObject, true, Completion(b, true)); return 0;
    case Op::ReqUnsubscribeGames: WonRequests(Routing(o)).UnsubscribeDataObject(0xffff, kWonGameObject, true, NULL); return 0;
    case Op::ReqCreateGroup: WonRequests(Routing(o)).CreateGroup(0xffff, (const wchar_t*)a, (const wchar_t*)b, 8, 0, true, 0, Completion(c, true)); return 0;
    case Op::ReqCreateGameObject: WonRequests(Routing(o)).CreateDataObject(true, (unsigned short)a, kWonGameObject, (const wchar_t*)b, new ByteBuffer((const void*)c, (unsigned long)d, false), Completion((intptr_t)EeWonNetCall::JoinGameDataCallback, true)); return 0;
    case Op::ReqReplaceGameObject: WonRequests(Routing(o)).ReplaceDataObject((unsigned short)a, kWonGameObject, (const wchar_t*)b, NULL); return 0;
    case Op::ReqModifyGameObject: WonRequests(Routing(o)).ModifyDataObject((unsigned short)a, kWonGameObject, (const wchar_t*)b, 0, 0, new ByteBuffer((const void*)c, (unsigned long)d, false), NULL); return 0;
    case Op::ReqReadGameObject: WonRequests(Routing(o)).ReadDataObject((unsigned short)a, kWonGameObject, true, Completion(b, true)); return 0;
    case Op::ReqJoinGroup: WonRequests(Routing(o)).JoinGroup((unsigned short)a, (const wchar_t*)b, std::wstring(gEmptyWideString), 0, 0, Completion(c, true)); return 0;
    case Op::ReqCancelJoinGroup: WonRequests(Routing(o)).CancelJoinGroup((unsigned short)a, true, NULL); return 0;
    case Op::ReqLeaveGroup: WonRequests(Routing(o)).LeaveGroup((unsigned short)a, NULL); return 0;
    case Op::ReqGroupList: WonRequests(Routing(o)).GetGroupList(0xff, Completion(a, true)); return 0;
    case Op::ReqClientList: WonRequests(Routing(o)).GetClientList(Completion(a, true)); return 0;
    case Op::ReqMembers: WonRequests(Routing(o)).GetMembersOfGroup((unsigned short)a, Completion(b, true)); return 0;
    case Op::ReqSendChat: WonRequests(Routing(o)).SendChat((const wchar_t*)a, 0, (unsigned short)b, Completion(c, true)); return 0;
    case Op::ReqSendChatTo: WonRequests(Routing(o)).SendChatTo((const wchar_t*)a, 4, std::list<unsigned short>((const unsigned short*)b, (const unsigned short*)b + c), NULL); return 0;
    case Op::KeyValid: return KeyValid();
    case Op::KeySave: CDKey(kWonProductName).SaveToRegistry(); return 0;
    case Op::LoginOpNew: return (intptr_t)static_cast<RefCount*>(RustServerOp::NewGetCert(User(o)));
    case Op::LoginOpSetCompletion: LoginOp(o)->SetCompletion(Completion(a, false)); return 0;
    case Op::LoginOpNoCreate: LoginOp(o)->SetNum(EE_WON_GETCERT_CREATE_ACCOUNT, false); return 0;
    case Op::LoginOpRun: LoginOp(o)->RunAsync(15000); return 0;
    case Op::LoginOpIs: return (AsyncOp*)a == (AsyncOp*)LoginOp(o);
    case Op::DirOpNew: return (intptr_t)static_cast<RefCount*>(RustServerOp::NewGetDir(DirServers(o), kWonProductDirectory, 0x703003a));
    case Op::DirOpSetCompletion: DirOp(o)->SetCompletion(Completion(a, false)); return 0;
    case Op::DirOpRun: DirOp(o)->RunAsync(15000); return 0;
    case Op::DirOpIs: return (AsyncOp*)a == (AsyncOp*)DirOp(o);
    case Op::ServersClear: Servers(o)->clear(); return 0;
    case Op::EntitiesLoad: sEntities = ((RustServerOp*)(AsyncOp*)a)->GetEntities(EE_WON_REQOP_DIR_LIST); return 0;
    case Op::EntityFirst: sEntity = 0; return 0;
    case Op::EntityAtEnd: return sEntity == sEntities.size();
    case Op::EntityIsRouting: return sEntities.Name(sEntity) == kWonRoutingServerType;
    case Op::EntityKeep: Servers(o)->Keep(sEntities, sEntity); return 0;
    case Op::EntityNext: sEntity++; return 0;
    case Op::FileSetVolume: sFile.SetVolume(gStartupVolume); return 0;
    case Op::FileSetDirectory: sFile.SetDirectory(gStartupDirectory); return 0;
    case Op::FileSetName: sFile.SetFile(UString("SierraCom.cfg")); return 0;
    case Op::FileOpen: return sFile.FOpen("r") == 0;
    case Op::FileGets: return fgets((char*)a, 256, sFile) != NULL;
    case Op::FileClose: sFile.Close(); return 0;
    case Op::DirServersNew: return (intptr_t)static_cast<RefCount*>(new ServerContext);
    case Op::DirServersAdd: DirServers(o)->AddAddress(IPAddr(std::string((const char*)a))); return 0;
    case Op::UpdateOpNew: sUpdateOp = RustServerOp::NewUpdateAccount(AuthServers(o), User(o), (const wchar_t*)a, std::string((const char*)b)); return 0;
    case Op::UpdateOpSetCompletion: sUpdateOp->SetCompletion(Completion(a, false)); return 0;
    case Op::UpdateOpClearNewsletters: sUpdateOp->SetNum(EE_WON_REQOP_ACCOUNT_CLEAR_NEWSLETTERS, 0); return 0;
    case Op::UpdateOpNewsletter: sUpdateOp->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, a); return 0;
    case Op::UpdateOpRun: sUpdateOp->RunAsync(15000); return 0;
    case Op::UpdateOpRelease: sUpdateOp = NULL; return 0;
    case Op::AccountOpNew: return (intptr_t)static_cast<RefCount*>(RustServerOp::NewGetAccount(AuthServers(o), User(o)));
    case Op::AccountOpSetCompletion: AccountOp(o)->SetCompletion(Completion(a, false)); return 0;
    case Op::AccountOpRun: AccountOp(o)->RunAsync(15000); return 0;
    case Op::AccountOpNewsletter: AccountOp(o)->SetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1); return 0;
    case Op::AccountNewsletter: return (intptr_t)((RustServerOp*)(AsyncOp*)a)->GetNum(EE_WON_REQOP_ACCOUNT_NEWSLETTER, 1);
    case Op::AccountEmail: return BytesOut(((RustServerOp*)(AsyncOp*)a)->GetBytes(EE_WON_REQOP_ACCOUNT_EMAIL), b);
    case Op::InfoName: return (intptr_t)(const char*)((EEWonNetGameInfo*)a)->mGameName;
    case Op::InfoAddress: return (intptr_t)(const char*)((EEWonNetGameInfo*)a)->mAddress;
    case Op::InfoPassword: return (intptr_t)(const char*)((EEWonNetGameInfo*)a)->mPassword;
    case Op::InfoHumans: return ((EEWonNetGameInfo*)a)->mHumans;
    case Op::InfoPlayers: return ((EEWonNetGameInfo*)a)->mPlayers;
    case Op::InfoHost: return ((EEWonNetGameInfo*)a)->mHostClientId;
    case Op::InfoFlags: return ((EEWonNetGameInfo*)a)->mFlags;
    case Op::InfoRoom: return ((EEWonNetGameInfo*)a)->mRoomGroupId;
    case Op::InfoSetName: ((EEWonNetGameInfo*)a)->mGameName = (const char*)b; return 0;
    case Op::InfoSetAddress: ((EEWonNetGameInfo*)a)->mAddress = (const char*)b; return 0;
    case Op::InfoSetPassword: ((EEWonNetGameInfo*)a)->mPassword = (const char*)b; return 0;
    case Op::InfoSetHumans: ((EEWonNetGameInfo*)a)->mHumans = (uint32_t)b; return 0;
    case Op::InfoSetPlayers: ((EEWonNetGameInfo*)a)->mPlayers = (uint32_t)b; return 0;
    case Op::InfoSetHost: ((EEWonNetGameInfo*)a)->mHostClientId = (uint32_t)b; return 0;
    case Op::InfoSetFlags: ((EEWonNetGameInfo*)a)->mFlags = (uint32_t)b; return 0;
    case Op::InfoSetRoom: ((EEWonNetGameInfo*)a)->mRoomGroupId = (unsigned short)b; return 0;
    case Op::RefAdd: Ref((void*)a)->AddRef(); return 0;
    case Op::RefRelease: Ref((void*)a)->Release(); return 0;
    case Op::ServersNew: return (intptr_t)new DirList;
    case Op::ServersDelete: delete (DirList*)a; return 0;
    }
    return 0;
}
