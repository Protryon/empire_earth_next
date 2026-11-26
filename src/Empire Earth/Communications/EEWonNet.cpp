// C:\EEX\Empire Earth\Communications\EEWonNet.cpp (__FILE__ 0x4bc110): the game's WON front end, 0xcd4ee..0xd3ff4
// R6 WON round 31: whole in Rust (ee_won_lobby wonnet.rs): this unit keeps the C++ objects' side, the completions,
// the request helpers, the forwards and the host (ee/EEWonNetHost.inl).
// 0xcd4ee (the error helper) opens the unit: only EEWonNet code calls it (EEServer.cpp's registered range still
// covers it). The WONAPI routing API it calls is in ee/wonapi/WONRouting.h (inferred). Unit flags: the exe's
// module set.
#include <windows.h>
#include <mbstring.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UICheckBox.h"
#include "lle/UIEditBox.h"
#include "lle/UResourceFile.h"
#include "lle/NEProtocol.h"
#include "lle/FSFileSpec.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "ee/EEAWonNet.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONAPICore.h"
#include "ee/EEUIEWonNetEvent.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEStringIDs.h"
#include "ee/wonapi/WONRoutingOpTypes.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/won_net/httpdoc.h"
#include "rust/won_net/reqdir.h"
#include "rust/won_net/routeop.h"
#include "rust/won_net/smartroute.h"
#include "rust/won_lobby/wonnet.h"
using namespace WONAPI;

char* kWonGameKey = "GAME";
char* kWonGameObject = "EE Game Object";             // 0x4bba60 (the data objects that describe the games)
char* kWonRoomObject = "EE Room Object";             // 0x4bba64 (the data objects that describe the rooms)
char* kWonProductName = "EmpireEarth";
wchar_t* kWonAuthDirectory = L"/TitanServers/Auth";
wchar_t* kWonDBProxyDirectory = L"/TitanServers/DBProxy";
wchar_t* kWonFirewallDirectory = L"/TitanServers/Firewall";
wchar_t* kWonRoutingServerType = L"TitanRoutingServerG2";

// The routing requests the game's Internet front end makes: RoutingConnection's request helpers it called (gone from
// WONRouting.h since WONLobby round 14), each a RoutingRequestValues run (routecpp.rs's ee_won_routeconn_request) in
// the helper's parameter order.
namespace {
class WonRequests {
public:
    explicit WonRequests(WONAPI::RoutingConnection* inConn) : mConn(inConn) {}
    void SubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(1).N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_READ_DATA, c); } // 2bf1d0
    void UnsubscribeDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_UNSUBSCRIBE_DATA, c); } // 2bf2e0
    void Register(const std::wstring& inName, const std::wstring& inPassword, unsigned long inReconnectId,
                  unsigned long inAsyncFlags, unsigned long inRegisterFlags, unsigned long inFlags, WONAPI::RouteCompletion* c)
    {
        WONAPI::RoutingRequestValues().W(inName).W(inPassword).N(inReconnectId).N(inAsyncFlags).N(inRegisterFlags).N(inFlags)
            .Run(mConn, EE_WON_RTQ_REGISTER, c);
    } // 2bcc30
    void CreateGroup(unsigned short inGroupId, const std::wstring& inName, const std::wstring& inPassword,
                     unsigned short inMaxPlayers, unsigned long inGroupFlags, unsigned char inJoinFlags,
                     unsigned long inAsyncFlags, WONAPI::RouteCompletion* c)
    {
        WONAPI::RoutingRequestValues().N(inGroupId).W(inName).W(inPassword).N(inMaxPlayers).N(inGroupFlags).N(inJoinFlags)
            .N(inAsyncFlags).Run(mConn, EE_WON_RTQ_CREATE_GROUP, c);
    } // 2bd430
    void CreateDataObject(unsigned char inFlags, unsigned short inLinkId, const std::string& inDataType,
                          const std::wstring& inName, const WONAPI::ByteBuffer* inData, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inFlags).N(inLinkId).S(inDataType).W(inName).D(inData).Run(mConn, EE_WON_RTQ_CREATE_DATA, c); } // 2becd0
    // 2bee30: the delete request
    void ReplaceDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).Run(mConn, EE_WON_RTQ_DELETE_DATA, c); }
    void GetGroupList(unsigned char inFlags, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inFlags).Run(mConn, EE_WON_RTQ_GROUP_LIST, c); } // 2bcf90
    void SendChat(const std::wstring& inText, unsigned long inFlags, unsigned short inRecipient, WONAPI::RouteCompletion* c)
    { this->SendChatTo(inText, (unsigned short)inFlags, std::list<unsigned short>(1, inRecipient), c); } // 2be540
    void SendChatTo(const std::wstring& inText, unsigned short inFlags, const std::list<unsigned short>& inClients,
                    WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().W(inText).N(inFlags).L(inClients).Run(mConn, EE_WON_RTQ_CHAT, c); } // 2be6a0
    void ModifyDataObject(unsigned short inLinkId, const std::string& inDataType, const std::wstring& inName,
                          unsigned short inOffset, bool inInsert, const WONAPI::ByteBuffer* inData, WONAPI::RouteCompletion* c)
    {
        WONAPI::RoutingRequestValues().N(inLinkId).S(inDataType).W(inName).N(inOffset).N(inInsert).D(inData)
            .Run(mConn, EE_WON_RTQ_MODIFY_DATA, c);
    } // 2bef50
    void JoinGroup(unsigned short inId, const std::wstring& inPassword, const std::wstring& inComment,
                   unsigned char inJoinFlags, unsigned char inMemberFlags, WONAPI::RouteCompletion* c)
    {
        WONAPI::RoutingRequestValues().N(inId).W(inPassword).W(inComment).N(inJoinFlags).N(inMemberFlags)
            .Run(mConn, EE_WON_RTQ_JOIN_GROUP, c);
    } // 2bd580
    void CancelJoinGroup(unsigned short inId, bool inLeave, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inId).N(inLeave).Run(mConn, EE_WON_RTQ_CANCEL_JOIN, c); } // 2bd6b0
    void LeaveGroup(unsigned short inId, WONAPI::RouteCompletion* c) { WONAPI::RoutingRequestValues().N(inId).Run(mConn, EE_WON_RTQ_LEAVE, c); } // 2bd850
    void ReadDataObject(unsigned short inLinkId, const std::string& inDataType, unsigned long inFlag, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(0).N(inLinkId).S(inDataType).N(inFlag).Run(mConn, EE_WON_RTQ_READ_DATA, c); } // 2bf0c0
    void GetClientList(WONAPI::RouteCompletion* c) { WONAPI::RoutingRequestValues().Run(mConn, EE_WON_RTQ_CLIENT_LIST, c); } // 2bcd40
    void GetMembersOfGroup(unsigned short inId, WONAPI::RouteCompletion* c)
    { WONAPI::RoutingRequestValues().N(inId).Run(mConn, EE_WON_RTQ_MEMBERS, c); } // 2bd1e0
private:
    WONAPI::RoutingConnection* mConn;
};
}
wchar_t* kWonProductDirectory = L"/EmpireEarth";
wchar_t* kWonCommunityName = L"EmpireEarth";

unsigned long gWonClientId;                 // 0x4e60b8 (RegisterCallback stores the server's client id)
extern unsigned long& gNetworkAdapter;      // 0x5308a0 (the adapter of the local address)

typedef EeWonNetCall Call;

// the completions (vtables 0x43e494, 0x43e488: a member of gWonNet; the routing one only for gWonNet's own routing
// connection's ops): the call made on gWonNet's Rust object (ee_won_wonnet_complete)
class WonNetCompletion : public CompletionBase<AsyncOpPtr> {
public:
    WonNetCompletion(Call inCall, bool inRouting) : mCall(inCall), mRouting(inRouting) {}
    virtual void Complete(AsyncOpPtr inOp) { ee_won_wonnet_complete(gWonNet != NULL ? gWonNet->mRust : NULL, mCall, mRouting, (intptr_t)inOp.get()); }

    Call mCall;
    bool mRouting;
};
static CompletionBase<AsyncOpPtr>* Completion(intptr_t inCall, bool inRouting) { return inCall < 0 ? NULL : new WonNetCompletion((Call)inCall, inRouting); }

// the cancel boxes' and message boxes' callbacks
static void CancelConnectCallback(UIForm* inForm) { *(bool*)inForm = true; }
static void CloseMessageCallback(UIForm* inForm) { gUserInterface->CloseForm(gUIManager->GetFormByID(kFormMainMenu)); }
static void CancelOpCallback(UIForm* inForm) { reinterpret_cast<AsyncOp*>(inForm)->Kill(true); }
static void HostGameCallback(UIForm* inForm) { ee_won_wonnet_call(reinterpret_cast<EEWonNet*>(inForm)->mRust, Call::CreateGame, 0, 0, 0); }

// the host's statics (the original's locals that are C++ objects) and sinks
static DirList sEntities;
static size_t sEntity; // the entity at
static FSFileSpec sFile;
static NEProtocol* sProtocol;
static EEUICWNRBLine sRoomLine;
static SmartPtr<RustServerOp> sUpdateOp;
static intptr_t WideOut(const std::wstring& inText, intptr_t inSink) { ee_won_wonnet_sink((void*)inSink, inText.data(), inText.length(), sizeof(wchar_t)); return 0; }
static intptr_t BytesOut(const std::string& inText, intptr_t inSink) { ee_won_wonnet_sink((void*)inSink, inText.data(), inText.length(), 1); return 0; }
static intptr_t BytesOut(const UWideString& inText, intptr_t inSink) { ee_won_wonnet_sink((void*)inSink, (const char*)inText, inText.GetTCHARLength(), 1); return 0; }
static intptr_t DataOut(const ByteBufferPtr& inData, intptr_t inSink) { return inData.get() == NULL ? 0 : (ee_won_wonnet_sink((void*)inSink, inData->data(), inData->length(), 1), 1); }
static intptr_t TextOut(long inId, intptr_t inSink)
{
    UWideString theText;
    gResource->GetString(inId, theText);
    return BytesOut(theText, inSink);
}
static bool KeyValid()
{
    CDKey theKey(kWonProductName);
    return theKey.LoadFromRegistry() && theKey.IsValid();
}

#include "ee/EEWonNetHost.inl"

static intptr_t WonNetHost(void* inCpp, EeWonNetOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return WonNetHostCall((EEWonNet*)inCpp, inOp, a, b, c, d); }
static const bool sWonNetHostSet = (ee_won_wonnet_set_host(WonNetHost), true);

// the class: the C++ objects here, every method a forward to the Rust object (DEAD-IN-AOC: base exe 0x4c55df ctor,
// 0x4c5924 dtor; nothing creates gWonNet in AoC)
EEWonNet::EEWonNet() : mRust(ee_won_wonnet_new(this)) { ee_won_wonnet_call(this->mRust, Call::Construct, 0, 0, 0); }
EEWonNet::~EEWonNet() { ee_won_wonnet_free(this->mRust); }
void EEWonNet::FnCFFDD(long inArg) { ee_won_wonnet_call(this->mRust, Call::FnCFFDD, inArg, 0, 0); }
void EEWonNet::FnCFFF2() { ee_won_wonnet_call(this->mRust, Call::FnCFFF2, 0, 0, 0); }
void EEWonNet::SelectServer(const std::wstring& inName, IPAddr inAddress) { ee_won_wonnet_call(this->mRust, Call::SelectServer, (intptr_t)inName.data(), inName.length(), (intptr_t)&inAddress); }
void EEWonNet::LeaveRoom() { ee_won_wonnet_call(this->mRust, Call::LeaveRoom, 0, 0, 0); }
void EEWonNet::GetGameInfo(EEWonNetGameInfo& outInfo) { ee_won_wonnet_call(this->mRust, Call::GetGameInfo, (intptr_t)&outInfo, 0, 0); }
void EEWonNet::SetGameInfo(EEWonNetGameInfo& inInfo) { ee_won_wonnet_call(this->mRust, Call::SetGameInfo, (intptr_t)&inInfo, 0, 0); }
void EEWonNet::ClearGameList() { ee_won_wonnet_call(this->mRust, Call::ClearGameList, 0, 0, 0); }
bool EEWonNet::RejoinRoom(bool inFlag) { return ee_won_wonnet_call(this->mRust, Call::RejoinRoom, inFlag, 0, 0) != 0; }
void EEWonNet::PlayerJoined() {}
bool EEWonNet::GetIsBehindFirewall() { return ee_won_wonnet_call(this->mRust, Call::IsBehindFirewall, 0, 0, 0) != 0; }
std::wstring EEWonNet::GetRoomName()
{
    std::wstring theName;
    ee_won_wonnet_call(this->mRust, Call::RoomName, (intptr_t)&theName, 0, 0);
    return theName;
}

// DEAD-IN-AOC: base exe 0x6a2220, only the dead dtor calls it. Partial: the base body also waits when inFlag is set
// (0x6a2270).
void WONLobbyWindow::Close(bool inFlag)
{
    if (!this->mClosed) {
        this->mClosed = true;
        this->Virtual18();
    }
}
