// C:\EEX\Empire Earth\Communications\EEWonNet.cpp (__FILE__ 0x4bc110): the game's WON front end, 0xcd4ee..0xd3ff4
// 0xcd4ee (the error helper) opens the unit: only EEWonNet code calls it (EEServer.cpp's registered range still
// covers it). The WONAPI routing API it calls is in ee/wonapi/WONRouting.h (inferred). Unit flags: the exe's
// module set.
#include <windows.h>
#include <mbstring.h>
#include <string>
#include <list>
#include <map>
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
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEAJoinOrHostAGame.h"
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
#include "ee/wonapi/WONRouting.h"
#include "ee/EEUIEWonNetArenaRoomEvents.h"
#include "ee/EEUIEWonNetArenaGameEvents.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEStringIDs.h"
using namespace WONAPI;

char* kWonGameKey = "GAME";                          // 0x4bba5c
char* kWonGameObject = "EE Game Object";             // 0x4bba60 (the data objects that describe the games)
char* kWonRoomObject = "EE Room Object";             // 0x4bba64 (the data objects that describe the rooms)
char* kWonProductName = "EmpireEarth";               // 0x4bba68
wchar_t* kWonAuthDirectory = L"/TitanServers/Auth";             // 0x4bba6c
wchar_t* kWonDBProxyDirectory = L"/TitanServers/DBProxy";       // 0x4bba70
wchar_t* kWonFirewallDirectory = L"/TitanServers/Firewall";     // 0x4bba74
wchar_t* kWonRoutingServerType = L"TitanRoutingServerG2";       // 0x4bba78
wchar_t* kWonProductDirectory = L"/EmpireEarth";                // 0x4bba7c
wchar_t* kWonCommunityName = L"EmpireEarth";                    // 0x4bba80

extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
unsigned long gWonClientId;                 // 0x4e60b8 (RegisterCallback stores the server's client id)
wchar_t gEmptyWideString[2];         // 0x4e60bc (L"": zero .bss right after gWonClientId; size = the gap to 0x4e60c0)
extern unsigned long& gU5308A0;              // 0x5308a0 (the adapter of the local address)

static const unsigned short kEEWonNetGameInfoVersion = 1;   // 0x43e480 (ExportGameInfo's record version)

static void CloseMessageCallback(UIForm* inForm);
static void CancelConnectCallback(UIForm* inForm);
static void CancelOpCallback(UIForm* inForm);

// a completion that calls a member of gWonNet (vtable 0x43e494)
class EEWonNetCompletion : public CompletionBase<AsyncOpPtr> {
public:
    EEWonNetCompletion(EEWonNet::Callback inCallback) : mCallback(inCallback) {}
// FUNCTION: EXE 0x4ce068
// Calls the completion's EEWonNet member on gWonNet with the finished op (nothing when gWonNet is gone)
    virtual void Complete(AsyncOpPtr inOp)
    {
        if (gWonNet != NULL)
            (gWonNet->*mCallback)((RoutingOp*)inOp.get());
    }

    EEWonNet::Callback mCallback;           // +0x08
};

// a completion for the ops of gWonNet's own routing connection (vtable 0x43e488)
class RoutingCompletion : public CompletionBase<AsyncOpPtr> {
public:
    RoutingCompletion(EEWonNet::Callback inCallback) : mCallback(inCallback) {}
// FUNCTION: EXE 0x4ce08c
// Calls the completion's EEWonNet member with the finished op, only for ops of gWonNet's own routing connection
    virtual void Complete(AsyncOpPtr inOp)
    {
        if (gWonNet == NULL)
            return;
        if (((RoutingOp*)inOp.get())->mConnection == gWonNet->mRouting.get())
            (gWonNet->*mCallback)((RoutingOp*)inOp.get());
    }

    EEWonNet::Callback mCallback;           // +0x08
};

// FUNCTION: EXE 0x4cd4ee
// Reports a WON error (the status text is computed and dropped)
void EEWonNetError(const char* inWhere, WONStatus inStatus)
{
    WONStatusToString(inStatus);
}

// FUNCTION: EXE 0x4cd4f9
// Returns the name of a routing op type ("Unknown Routing Type" when out of range)
const char* RoutingOpTypeToString(RoutingOpType inType)
{
    switch (inType) {
    case 0: return "RoutingOp_Undefined";
    case 1: return "RoutingOp_AbortServerShutdown";
    case 2: return "RoutingOp_AcceptClient";
    case 3: return "RoutingOp_BanClient";
    case 4: return "RoutingOp_BecomeModerator";
    case 5: return "RoutingOp_CancelJoinGroup";
    case 6: return "RoutingOp_ClientFlagsChanged";
    case 7: return "RoutingOp_ClientJoinedGroup";
    case 8: return "RoutingOp_ClientJoinedServer";
    case 9: return "RoutingOp_ClientLeftGroup";
    case 10: return "RoutingOp_ClientLeftServer";
    case 11: return "RoutingOp_CreateDataObject";
    case 12: return "RoutingOp_CreateGroup";
    case 13: return "RoutingOp_DataObjectCreated";
    case 14: return "RoutingOp_DataObjectDeleted";
    case 15: return "RoutingOp_DataObjectModified";
    case 16: return "RoutingOp_DeleteDataObject";
    case 17: return "RoutingOp_DetectFirewall";
    case 18: return "RoutingOp_DisconnectClient";
    case 19: return "RoutingOp_GetBadUserList";
    case 20: return "RoutingOp_GetClientList";
    case 21: return "RoutingOp_GetGroupList";
    case 22: return "RoutingOp_GetMembersOfGroup";
    case 23: return "RoutingOp_GroupCaptainChanged";
    case 24: return "RoutingOp_GroupCreated";
    case 25: return "RoutingOp_GroupDeleted";
    case 26: return "RoutingOp_GroupFlagsChanged";
    case 27: return "RoutingOp_GroupInvitation";
    case 28: return "RoutingOp_GroupJoinAttempt";
    case 29: return "RoutingOp_GroupMaxPlayersChanged";
    case 30: return "RoutingOp_GroupMemberCount";
    case 31: return "RoutingOp_GroupNameChanged";
    case 32: return "RoutingOp_GroupObserverCount";
    case 33: return "RoutingOp_InviteClient";
    case 34: return "RoutingOp_JoinGroup";
    case 35: return "RoutingOp_LeaveGroup";
    case 36: return "RoutingOp_MemberFlagsChanged";
    case 37: return "RoutingOp_ModifyDataObject";
    case 38: return "RoutingOp_MuteClient";
    case 39: return "RoutingOp_PeerChat";
    case 40: return "RoutingOp_PeerData";
    case 41: return "RoutingOp_ReadDataObject";
    case 42: return "RoutingOp_RegisterClient";
    case 43: return "RoutingOp_RelinquishCaptaincy";
    case 44: return "RoutingOp_SendChat";
    case 45: return "RoutingOp_SendComplaint";
    case 46: return "RoutingOp_SendData";
    case 47: return "RoutingOp_SendServerAlert";
    case 48: return "RoutingOp_ServerAlert";
    case 49: return "RoutingOp_ServerShutdownAborted";
    case 50: return "RoutingOp_ServerShutdownStarted";
    case 51: return "RoutingOp_SetClientFlags";
    case 52: return "RoutingOp_SetGroupFlags";
    case 53: return "RoutingOp_SetGroupMaxPlayers";
    case 54: return "RoutingOp_SetGroupName";
    case 55: return "RoutingOp_SetGroupPassword";
    case 56: return "RoutingOp_StartServerShutdown";
    case 57: return "RoutingOp_UnsubscribeDataObject";
    case 58: return "RoutingOp_YouWereBanned";
    case 59: return "RoutingOp_YouWereMuted";
    case 60: return "RoutingOp_DerivedServerOp";
    }
    return "Unknown Routing Type";
}

// DEAD-IN-AOC: base exe 0x4c55df (ctor) / 0x4c5924 (dtor). Nothing creates gWonNet in AoC (only null stores,
// 0x544382/0x5e2123; no .?AVEEWonNet@@ RTTI), so /OPT:REF dropped both and the vtable. Base neighbours match:
// RoutingOpTypeToString's jump table, then the ctor, ??_G, dtor, A2W (0x3e), ConnectToLobby (0x114). AoC proof
// they were compiled: Release 0x4cd775 right after RoutingOpTypeToString (first used by the dtor), and the
// list<DirEntityPtr> dtor/erase/_Destroy 0x4d17ea/0x4d2df7/0x4d3f4c and vector<std::string> erase 0x4d1f5e,
// copy 0x4d3e35 in the template tail (WONAPI uses them too). No credit. Partial: the base ctor also nulls the
// SmartPtr members (+0x0c..+0x1c, +0x28, +0x38..+0x50; AoC declares +0x14, +0x1c and +0x44..+0x4c as gaps and
// +0x18/+0x28 as raw pointers). (r25pre-won; r25pre-won-2: lobby teardown and the mDirectoryServers clears)
EEWonNet::EEWonNet()
{
    long theResult;

    theResult = TSScheduler::CreateCriticalSection(this->mLock);
    ESVERIFY_AT(theResult == keseOK, 195);
    theResult = TSScheduler::CreateCriticalSection(this->mLock8);
    ESVERIFY_AT(theResult == keseOK, 197);
    this->mLobby = NULL;
    this->mLobbyWindow = NULL;
    this->mServers.clear();
    this->mPassword.erase();
    this->mGameName.erase();
    this->mRoomName.erase();
    this->mServerName.erase();
    this->mGames.clear();
    this->mRooms.clear();
    this->mDirectoryServers.clear();
    this->mJoinGroupId = 0xffff;
    this->mGameGroupId = 0xffff;
    this->mReplyClientId = 0xffff;
    this->mJoinRoomId = 0xffff;
    this->mGameListPort = 0xffff;
    this->mRoomGroupId = 0xffff;
    this->mCurrentRoom.erase();
    this->mIsBehindFirewall = false;
    this->mB187 = false;
    this->mCancelled = false;
    this->mJoinRoomCancelled = false;
    this->mIgnoreClose = false;
    this->mIsListingPlayers = false;
    this->mRoomListChanged = false;
    this->mB18E = false;
    this->mGamesChanged = false;
    this->mL16C = 0;
    this->mL168 = 0;
}

// DEAD-IN-AOC: base exe 0x4c5924 (see the ctor)
EEWonNet::~EEWonNet()
{
    this->mIgnoreClose = true;
    if (this->mLobbyWindow != NULL) {
        this->mLobbyWindow->Close(true);
        while (!this->mLobbyWindow->mClosed)
            ;
        delete this->mLobbyWindow;
    }
    delete this->mLobby;
    this->mGames.clear();
    this->mRooms.clear();
    this->mDirectoryServers.clear();
    TSScheduler::DestroyCriticalSection(this->mLock);
    TSScheduler::DestroyCriticalSection(this->mLock8);
}

// FUNCTION: EXE 0x4cd794
// Converts inLength multibyte characters to wide into the caller's buffer
wchar_t* EEWonNet::A2W(wchar_t* outString, const char* inString, int inLength)
{
    int theBytes = PlatMbsNBCnt(reinterpret_cast<const unsigned char*>(inString), inLength);
    if (outString == NULL)
        return NULL;
    outString[0] = 0;
    if (inString != NULL)
        MultiByteToWideChar(CP_ACP, 0, inString, theBytes, outString, inLength);
    return outString;
}

// FUNCTION: EXE 0x4cd7d2
// Resets two counters and walks every routing group and member, converting each name into a scratch buffer
void EEWonNet::ConnectToLobby()
{
    RoutingGroupMap::const_iterator theGroup;
    RoutingMemberMap::const_iterator theMember;
    char theName[1024];
    UWideString theString;

    if (this->mRouting.get() == NULL)
        return;
    this->mL16C = 0;
    this->mL168 = 0;
    for (theGroup = this->mRouting->GetGroupMap().begin(); theGroup != this->mRouting->GetGroupMap().end(); theGroup++) {
        memset(theName, 0, 1024);
        wcstombs(theName, theGroup->second->mName.c_str(), 1024);
        for (theMember = theGroup->second->mMembers.begin(); theMember != theGroup->second->mMembers.end();
             theMember++) {
            memset(theName, 0, 1024);
            wcstombs(theName, theMember->second->mClientInfo->mName.c_str(), 1024);
        }
    }
}

// FUNCTION: EXE 0x4cd8e6
// Replaces the game list with the game objects read from the server and marks the game and room lists changed
void EEWonNet::ListGamesCallback(RoutingOp* inOp)
{
    EEWonNetGameInfo theInfo;

    if (inOp->GetStatus() == 0) {
        this->mLock->Enter();
        this->mGames.clear();
        for (RoutingDataObjectList::iterator theObject = inOp->mDataObjects.begin();
             theObject != inOp->mDataObjects.end(); theObject++) {
            this->ImportGameInfo((unsigned char*)(*theObject)->GetData(), theInfo);
            this->mGames[(*theObject)->mLinkId] = theInfo;
            this->mGamesChanged = true;
        }
        this->mRoomListChanged = true;
        this->mB18E = true;
        this->mGamesChanged = true;
        this->mLock->Leave();
    } else {
        EEWonNetError("EEWonNet::ListGamesCallback", inOp->GetStatus());
    }
}

// FUNCTION: EXE 0x4cda04
// Rebuilds the room list from the server's chat-room groups (name, maximum players) and the rooms' descriptions
// from their data objects
void EEWonNet::ListRoomCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    RoutingGroupMap::const_iterator theGroup;
    RoutingDataObjectList::iterator theObject = inOp->mDataObjects.begin();
    RoutingDataObjectList::iterator theData = theObject;
    char theName[1024];
    UWideString theString;
    std::wstring theWide;
    unsigned long theMaxPlayers;

    TMGetTime();
    if (theStatus == 0 && this->mRouting.get() != NULL) {
        this->mLock->Enter();
        EnterCriticalSection(&this->mRouting->mDataLock);
        this->mRooms.clear();
        for (theGroup = this->mRouting->GetGroupMap().begin(); theGroup != this->mRouting->mGroupMap.end(); theGroup++) {
            if (!(theGroup->second->mFlags & 0x800))
                continue;
            memset(theName, 0, 1024);
            wcstombs(theName, theGroup->second->mName.c_str(), 1024);
            theString = UWideString(theName);
            this->mRooms[theGroup->second->mId].mId = theGroup->second->mId;
            this->mRooms[theGroup->second->mId].mName = theString;
            theMaxPlayers = theGroup->second->mMaxPlayers;
            this->mRooms[theGroup->second->mId].mMaxPlayers = theMaxPlayers;
        }
        for (; theData != inOp->mDataObjects.end(); theData++) {
            theWide = gEmptyWideString;
            theWide.assign((const wchar_t*)(*theData)->GetData(), (*theData)->GetDataLength() / 2);
            memset(theName, 0, 1024);
            // Fix: the description is the data object's text; this used to read the name of the past-the-end
            // group left over from the loop above.
            wcstombs(theName, theWide.c_str(), 1023);
            theString = UWideString(theName);
            this->mRooms[(*theData)->mLinkId].mDescription = theString;
        }
        this->mRoomListChanged = true;
        LeaveCriticalSection(&this->mRouting->mDataLock);
        this->mLock->Leave();
        TMGetTime();
    } else {
        EEWonNetError("EEWonNet::ListRoomCallback", theStatus);
    }
}

// FUNCTION: EXE 0x4cdcbb
// Handles the routing connection closing: reports the lost Sierra connection (unless expected) and disconnects
void EEWonNet::CloseCallback(RoutingOp* inOp)
{
    UWideString theMessage;

    if (this->mIgnoreClose == false) {
        gResource->GetString(IDS_YOU_HAVE_LOST_YOUR_CONNECTION_TO_SIERRA, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, CloseMessageCallback, true);
    }
    this->mIgnoreClose = false;
    if (inOp->mConnection54 == this->mRouting.get())
        this->Disconnect();
}

// FUNCTION: EXE 0x4cdd2c
// Reports a failed routing op (the catch-all handler)
void EEWonNet::TempCallback(RoutingOp* inOp)
{
    inOp->GetType();
    RoutingOpTypeToString(inOp->GetType());
    if (inOp->GetStatus() != 0)
        EEWonNetError("EEWonNet::TempCallback", inOp->GetStatus());
}

// FUNCTION: EXE 0x4cdd5e
// Connects to a routing server: installs the connect, close, chat and data-object handlers, shows a cancel box
void EEWonNet::SelectServer(const std::wstring& inName, IPAddr inAddress)
{
    UWideString theMessage;

    this->Disconnect();
    this->mRouting = new RoutingConnection;
    this->mRouting->Fn2AB0A0(this->mUser, 2);
    this->mRouting->SetConnectCompletion(new EEWonNetCompletion(&EEWonNet::ConnectCallback));
    this->mRouting->SetCloseCompletion(new EEWonNetCompletion(&EEWonNet::CloseCallback));
    this->mRouting->mCompletion164 = new RoutingCompletion(&EEWonNet::OnChat);
    this->mRouting->mCompletionFC = new RoutingCompletion(&EEWonNet::OnDataObjectCreated);
    this->mRouting->mCompletion100 = new RoutingCompletion(&EEWonNet::OnDataObjectDeleted);
    this->mRouting->mCompletion104 = new RoutingCompletion(&EEWonNet::OnDataObjectModified);
    this->mRouting->mCompletionE4 = new RoutingCompletion(&EEWonNet::OnDataObjectChanged);
    this->mRouting->mCompletionEC = new RoutingCompletion(&EEWonNet::OnDataObjectChanged);
    this->mRouting->mCompletion140 = new RoutingCompletion(&EEWonNet::OnGroupMaxPlayersChanged);
    this->mRouting->mCompletion1BC = new RoutingCompletion(&EEWonNet::TempCallback);
    this->mCancelled = false;
    this->mServerName = inName;
    this->mRouting->ClearProductName();
    this->mRouting->Connect(inAddress, 15000);
    gResource->GetString(IDS_CONNECTING_TO_SERVER, theMessage);
    gUIManager->DisplayCancel(theMessage, 1, (UIForm*)&this->mCancelled, CancelConnectCallback,
                              gUIManager->GetFormByID(kFormSelectServer));
}

// FUNCTION: EXE 0x4ce0c2
// Handles the connection result: registers the client, or disconnects and reports the error; when cancelled or
// stale only closes the server selection form
void EEWonNet::ConnectCallback(RoutingOp* inOp)
{
    UWideString theMessage;

    if (inOp->mConnection78 == this->mRouting.get() && inOp->GetStatus() != 100002 && this->mCancelled == false) {
        if (inOp->GetStatus() != 0) {
            this->Disconnect();
            WONStatusToString(inOp->GetStatus());
            gUserInterface->CloseForm(gUIManager->GetFormByID(kFormSelectServer));
            gResource->GetString(IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME, theMessage);
            gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
        } else {
            this->mIgnoreClose = false;
            CompletionBase<AsyncOpPtr>* theCompletion = new RoutingCompletion(&EEWonNet::RegisterCallback);
            this->mRouting->Register(gEmptyWideString, gEmptyWideString, gWonClientId, 0, 5, this->mB187 ? 0 : 4, theCompletion);
        }
    } else {
        gUserInterface->CloseForm(gUIManager->GetFormByID(kFormSelectServer));
    }
}

// FUNCTION: EXE 0x4ce244
// Opens the room list screen once the group list arrived and reads the room and game objects
void EEWonNet::ChatRoomListCallback(RoutingOp* inOp)
{
    if (inOp->GetStatus() != 0) {
        EEWonNetError("EEWonNet::ChatRoomListCallback", inOp->GetStatus());
        return;
    }
    gUIManager->LockUIForCopyData();
    if (gWonNet != NULL && this->mCancelled == false) {
        gUIManager->PopTo(gUIManager->GetFormByID(kFormMultiplayer));
        UIEvent* theEvent = new EEUIEWNAROpenScreen;
        gUIManager->GetFormByID(kFormAvailableRooms)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        gUIManager->Push(gUIManager->GetFormByID(kFormAvailableRooms));
        this->mRouting->ReadDataObject(0xffff, kWonRoomObject, true, new RoutingCompletion(&EEWonNet::ListRoomCallback));
        this->mRouting->ReadDataObject(0xffff, kWonGameObject, true, new RoutingCompletion(&EEWonNet::ListGamesCallback));
    }
    gUIManager->UnlockUIForCopyData();
}

// FUNCTION: EXE 0x4ce421
// Stores the created game's info and starts hosting it; on failure leaves the game and reports it
void EEWonNet::JoinGameDataCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    UWideString theMessage;
    EEWonNetGameInfo theInfo;

    this->mLock->Enter();
    RoutingOpTypeToString(inOp->GetType());
    if (theStatus != 0) {
        this->ClearGames();
        this->RejoinRoom(false);
        gResource->GetString(IDS_FAILED_TO_CREATE_GAME, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    } else {
        unsigned char* theData = new unsigned char[inOp->GetData()->length()];
        memcpy(theData, inOp->GetData()->data(), inOp->GetData()->length());
        this->ImportGameInfo(theData, theInfo);
        this->mGames[inOp->GetLinkId()] = theInfo;
        delete theData;
        this->mGameInfo = this->mGames[inOp->GetLinkId()];
        this->mGamesChanged = true;
        EEAction* theAction = new EEAJoinOrHostAGame(4, UWideString(this->ToString(this->mUser->mName).c_str()), UWideString(this->ToString(this->mGameName).c_str()));
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    }
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4ce695
// Publishes the hosted game's data object (name, address, password, flags) once its group exists, or reports
// the failure or a duplicate game name
void EEWonNet::CreateGameCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    EEWonNetGameInfo theInfo;
    std::string theType = kWonGameObject;
    NEProtocol* theProtocol = NULL;
    UWideString theMessage;
    unsigned char* theData;
    unsigned long theSize;

    if (theStatus != -2517) {
        if (theStatus != 0) {
            gResource->GetString(IDS_FAILED_TO_CREATE_GAME, theMessage);
            gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
        } else {
            this->mRoomGroupId = this->mGameListPort;
            this->mCurrentRoom = this->mRoomName;
            this->mIgnoreClose = true;
            this->mRouting->Fn2BF2E0(0xffff, kWonGameObject, true, NULL);
            this->LeaveGroups();
            this->mGameGroupId = inOp->mU64;
            if (this->mPassword.length() != 0)
                theInfo.mPassword = UWideString(this->ToString(this->mPassword).c_str());
            else
                theInfo.mPassword = UWideString(gU4E5B98);
            NEProtocol::GetProtocol(0, theProtocol);
            theInfo.mGameName = UWideString(this->ToString(this->mGameName).c_str());
            theInfo.mAddress = theProtocol->GetLocalAddress(gU5308A0);
            theInfo.mHumans = 0;
            theInfo.mPlayers = 8;
            theInfo.mHostClientId = this->mRouting->mClientId;
            theInfo.mFlags = 1;
            if (this->mIsBehindFirewall)
                theInfo.mFlags = 3;
            theInfo.mRoomGroupId = this->mRoomGroupId;
            this->ExportGameInfo(theInfo, theData, theSize);
            this->mRouting->CreateDataObject(true, this->mGameGroupId, theType, this->mGameName, new ByteBuffer(theData, theSize, false),
                                       new RoutingCompletion(&EEWonNet::JoinGameDataCallback));
            delete theData;
        }
    } else {
        gResource->GetString(IDS_GAME_WITH_THAT_NAME_ALREADY_EXISTS, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    }
}

// FUNCTION: EXE 0x4ce995
// Creates the game group (the callback of the firewall warning box)
static void HostGameCallback(UIForm* inForm)
{
    reinterpret_cast<EEWonNet*>(inForm)->CreateGame();
}

// FUNCTION: EXE 0x4ce99e
// Asks the routing server to create the game's group (8 players, with the password)
void EEWonNet::CreateGame()
{
    this->mRouting->CreateGroup(0xffff, this->mGameName, this->mPassword, 8, 0, true, 0,
                          new RoutingCompletion(&EEWonNet::CreateGameCallback));
}

// FUNCTION: EXE 0x4ce9e1
// Hosts a WON game: stores its name and password, then creates the group (after a warning when behind a firewall)
void EEWonNet::HostGame(const char* inName, const char* inPassword)
{
    UWideString theMessage;

    gHostedGameName = UWideString(inName);
    gGamePreferences.Save();
    this->ClearGames();
    this->mGameName = this->ToWString(inName);
    if (inPassword != NULL)
        this->mPassword = this->ToWString(inPassword);
    else
        this->mPassword.erase();
    if (this->mRouting.get() == NULL)
        return;
    if (this->mIsBehindFirewall == true) {
        gResource->GetString(IDS_SIERRA_COM_IS_UNABLE_TO_VERIFY, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, reinterpret_cast<UIForm*>(this), HostGameCallback, true);
    } else {
        this->CreateGame();
    }
}

// FUNCTION: EXE 0x4ceb2d
// Rewrites the joined game's data object when this client is its host
void EEWonNet::UpdateGameObject()
{
    std::string theType = kWonGameObject;

    if (this->mGameGroupId != 0xffff) {
        std::map<unsigned short, EEWonNetGameInfo>::iterator theGame = this->mGames.find(this->mGameGroupId);
        if (theGame != this->mGames.end() && theGame->second.mHostClientId == this->mRouting->mClientId)
            this->mRouting->ReplaceDataObject(theGame->first, theType, this->mGameName, NULL);
    }
}

// FUNCTION: EXE 0x4cebbd
// Writes a game's info (version 1: name, address, password, counts, host, flags, room) into a new buffer
void EEWonNet::ExportGameInfo(EEWonNetGameInfo& inInfo, unsigned char*& outData, unsigned long& outSize)
{
    unsigned long theMessageSize = inInfo.mGameName.GetExportSize() + inInfo.mAddress.GetExportSize()
                                   + inInfo.mPassword.GetExportSize() + 0x14;
    outData = new unsigned char[theMessageSize];
    if (outData == NULL)
        return;
    unsigned long theLocation;
    memcpy(outData, &kEEWonNetGameInfoVersion, 2);
    inInfo.mGameName.Export(outData + 2);
    theLocation = inInfo.mGameName.GetExportSize() + 2;
    inInfo.mAddress.Export(outData + theLocation);
    theLocation += inInfo.mAddress.GetExportSize();
    inInfo.mPassword.Export(outData + theLocation);
    theLocation += inInfo.mPassword.GetExportSize();
    memcpy(outData + theLocation, &inInfo.mHumans, 4);
    theLocation += 4;
    memcpy(outData + theLocation, &inInfo.mPlayers, 4);
    theLocation += 4;
    memcpy(outData + theLocation, &inInfo.mHostClientId, 4);
    theLocation += 4;
    memcpy(outData + theLocation, &inInfo.mFlags, 4);
    theLocation += 4;
    memcpy(outData + theLocation, &inInfo.mRoomGroupId, 2);
    theLocation += 2;
    ESVERIFY_AT(theLocation == theMessageSize, 1576);
    outSize = theMessageSize;
}

// FUNCTION: EXE 0x4cece1
// Reads a game's info from a buffer (version 1 strings, else C strings), then counts, host, flags and room
void EEWonNet::ImportGameInfo(unsigned char* inMemoryPointer, EEWonNetGameInfo& outInfo)
{
    unsigned long theLocation;
    unsigned short theVersion;

    outInfo.mFlags = 0;
    ESVERIFY_AT(inMemoryPointer != NULL, 1595);
    memcpy(&theVersion, inMemoryPointer, 2);
    if (theVersion == 1) {
        theLocation = outInfo.mGameName.Import(&inMemoryPointer[2]) + 2;
        theLocation += outInfo.mAddress.Import(&inMemoryPointer[theLocation]);
        theLocation += outInfo.mPassword.Import(&inMemoryPointer[theLocation]);
    } else {
        outInfo.mGameName = reinterpret_cast<const char*>(inMemoryPointer);
        theLocation = outInfo.mGameName.GetLength() + 1;
        outInfo.mAddress = reinterpret_cast<const char*>(&inMemoryPointer[theLocation]);
        theLocation += outInfo.mAddress.GetLength() + 1;
        outInfo.mPassword = reinterpret_cast<const char*>(&inMemoryPointer[theLocation]);
        theLocation += outInfo.mPassword.GetLength() + 1;
    }
    memcpy(&outInfo.mHumans, &inMemoryPointer[theLocation], 4);
    theLocation += 4;
    memcpy(&outInfo.mPlayers, &inMemoryPointer[theLocation], 4);
    theLocation += 4;
    memcpy(&outInfo.mHostClientId, &inMemoryPointer[theLocation], 4);
    theLocation += 4;
    memcpy(&outInfo.mFlags, &inMemoryPointer[theLocation], 4);
    theLocation += 4;
    memcpy(&outInfo.mRoomGroupId, &inMemoryPointer[theLocation], 2);
}

// FUNCTION: EXE 0x4cede5
// Joins the named game group with the password, or reports that it was not found
void EEWonNet::JoinGame(const char* inName, const char* inPassword)
{
    UWideString theMessage;
    RoutingGroupMap theGroups;
    RoutingGroupMap::const_iterator theGroup;
    std::wstring thePassword;
    std::string theName;
    bool theFound;

    if (this->mRouting.get() == NULL)
        return;
    EnterCriticalSection(&this->mRouting->mDataLock);
    theGroups = this->mRouting->GetGroupMap();
    if (theGroups.size() == 0) {
        // Fix: release the routing data lock on this early exit too (it used to stay held).
        LeaveCriticalSection(&this->mRouting->mDataLock);
        return;
    }
    for (theGroup = theGroups.begin(); theGroup != theGroups.end(); theGroup++) {
        if (PlatMbsCmp((const unsigned char*)this->ToString(theGroup->second->mName).c_str(),
                    reinterpret_cast<const unsigned char*>(inName)) == 0
            && (theGroup->second->mFlags & 0x800) == 0)
            theFound = true;
        else
            theFound = false;
        if (theFound)
            break;
    }
    if (theFound == false) {
        gResource->GetString(IDS_COULDNT_FIND_GAME_REQUESTED, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    } else {
        theName = this->ToString(theGroup->second->mName);
        this->ClearGames();
        this->mJoinGroupId = theGroup->first;
        if (inPassword != NULL)
            thePassword = this->ToWString(inPassword);
        this->mRouting->JoinGroup(theGroup->first, thePassword, std::wstring(gEmptyWideString), 0, 0,
                            new RoutingCompletion(&EEWonNet::JoinGroupCallback));
    }
    LeaveCriticalSection(&this->mRouting->mDataLock);
}

// FUNCTION: EXE 0x4cf02c
// Handles a game join: leaves the room and reads the game's object, or reports a bad password or failure
void EEWonNet::JoinGroupCallback(RoutingOp* inOp)
{
    RoutingGroupInfoPtr theGroup;
    UWideString theMessage;

    if (inOp->mU64 != this->mJoinGroupId) {
        if (inOp->GetStatus() == 0)
            this->mRouting->LeaveGroup(inOp->mU64, false);
    } else {
        this->mJoinGroupId = 0xffff;
        if (inOp->GetStatus() != 0) {
            if (inOp->GetStatus() == -2524)
                gResource->GetString(IDS_PASSWORD_IS_INCORRECT, theMessage);
            else
                gResource->GetString(IDS_COULDNT_JOIN_GAME, theMessage);
            gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
        } else {
            this->mRoomGroupId = this->mGameListPort;
            this->mCurrentRoom = this->mRoomName;
            this->mIgnoreClose = true;
            this->LeaveGroups();
            this->mGameGroupId = inOp->mU64;
            theGroup = this->mRouting->GetGroup(this->mGameListPort);
            this->mRouting->Fn2BF0C0(this->mGameGroupId, kWonGameObject, true,
                               new RoutingCompletion(&EEWonNet::JoinGameObjectsCallback));
            this->mRouting->Fn2BF2E0(0xffff, kWonGameObject, true, NULL);
        }
    }
}

// FUNCTION: EXE 0x4cf1fc
// Joins the host of each game object read, or rejoins the room and reports that the game was not found
void EEWonNet::JoinGameObjectsCallback(RoutingOp* inOp)
{
    UWideString theMessage;
    RoutingDataObjectList::iterator theObject = inOp->mDataObjects.begin();
    UWideString theString;
    EEWonNetGameInfo theInfo;
    bool theFound = false;

    if (inOp->GetStatus() == 0) {
        for (; theObject != inOp->mDataObjects.end(); theObject++) {
            if ((*theObject)->mDataType.compare(0, (*theObject)->mDataType.length(), kWonGameObject,
                                                strlen(kWonGameObject)) != 0)
                continue;
            unsigned char* theData = new unsigned char[(*theObject)->GetDataLength()];
            memcpy(theData, (*theObject)->GetData(), (*theObject)->GetDataLength());
            this->ImportGameInfo(theData, theInfo);
            delete theData;
            EEAction* theAction = new EEAJoinOrHostAGame(3, UWideString(this->ToString(this->mUser->mName).c_str()), theInfo.mAddress);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
            theFound = true;
        }
    } else {
        EEWonNetError(gU4E5B98, inOp->GetStatus());
    }
    if (theFound == false) {
        this->ClearGames();
        this->RejoinRoom(false);
        gResource->GetString(IDS_SPECIFIED_GAME_NOT_FOUND, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    }
}

// FUNCTION: EXE 0x4cf3f4
// Rejoins the last room and rereads its games; returns false (reporting the lost connection) when there is none
bool EEWonNet::RejoinRoom(bool inFlag)
{
    UWideString theMessage;
    std::wstring thePassword;
    bool theResult = true;

    if (this->mRouting.get() != NULL && this->mRoomGroupId != 0xffff) {
        this->mRouting->JoinGroup(this->mRoomGroupId, thePassword, std::wstring(gEmptyWideString), 0, 0, NULL);
        this->mGameListPort = this->mRoomGroupId;
        this->mRoomName = this->mCurrentRoom;
        this->mIgnoreClose = false;
        this->mRouting->ReadDataObject(0xffff, kWonGameObject, true, new RoutingCompletion(&EEWonNet::ListGamesCallback));
    } else {
        gResource->GetString(IDS_YOU_HAVE_LOST_YOUR_CONNECTION_TO_SIERRA, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, CloseMessageCallback, true);
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x4cf570
// Leaves the current groups and joins the named chat room with the password, showing a cancel box
void EEWonNet::JoinRoom(const char* inRoom, const char* inPassword)
{
    RoutingGroupMap theGroups;
    RoutingGroupMap::const_iterator theGroup;
    UWideString theMessage;
    std::wstring thePassword;
    std::string theName;
    bool theFound;

    if (this->mRouting.get() != NULL) {
        theGroups = this->mRouting->GetGroupMap();
        if (theGroups.size() != 0) {
            for (theGroup = theGroups.begin(); theGroup != theGroups.end(); theGroup++) {
                if (PlatMbsCmp((const unsigned char*)this->ToString(theGroup->second->mName).c_str(),
                            reinterpret_cast<const unsigned char*>(inRoom)) == 0
                    && (theGroup->second->mFlags & 0x800) != 0)
                    theFound = true;
                else
                    theFound = false;
                if (!theFound)
                    continue;
                theName = this->ToString(theGroup->second->mName);
                this->mCurrentRoom = this->mRoomName;
                this->LeaveGroups();
                this->mJoinRoomId = theGroup->first;
                this->mRoomName = theGroup->second->mName;
                if (inPassword != NULL)
                    thePassword = this->ToWString(inPassword);
                this->mRouting->JoinGroup(theGroup->first, thePassword, std::wstring(gEmptyWideString), 0, 0,
                                    new RoutingCompletion(&EEWonNet::JoinRoomCallback));
                this->mJoinRoomCancelled = false;
                gResource->GetString(IDS_JOINING_ROOM, theMessage);
                gUIManager->DisplayCancel(theMessage, 1, (UIForm*)&this->mJoinRoomCancelled, CancelConnectCallback,
                                          gUIManager->GetFormByID(kFormAvailableRooms));
                break;
            }
        }
    }
}

// FUNCTION: EXE 0x4cf7ec
// Handles a room join: opens the game list screen, or reports a bad password or failure; leaves stale joins
void EEWonNet::JoinRoomCallback(RoutingOp* inOp)
{
    UWideString theMessage;

    if (inOp->mU64 != this->mJoinRoomId) {
        if (inOp->GetStatus() == 0)
            this->mRouting->LeaveGroup(inOp->mU64, false);
    } else {
        this->mJoinRoomId = 0xffff;
        if (this->mJoinRoomCancelled == false) {
            gUserInterface->CloseForm(NULL);
            WONStatus theStatus = inOp->GetStatus();
            if (theStatus != 0 && theStatus != -2504) {
                EEWonNetError("EEWonNet::JoinRoomCompletion", theStatus);
                if (inOp->GetStatus() == -2524)
                    gResource->GetString(IDS_PASSWORD_IS_INCORRECT, theMessage);
                else
                    gResource->GetString(IDS_COULDNT_JOIN_ROOM, theMessage);
                gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
                this->mRoomName.erase();
            } else {
                this->mGameListPort = inOp->mU64;
                UIEvent* theEvent = new EEUIEWNAGOpenScreen;
                gUIManager->GetFormByID(kFormAvailableGames)->SetFormOpenEvent(theEvent);
                theEvent->Dereference();
                gUserInterface->SwitchToForm(gUIManager->GetFormByID(kFormAvailableGames));
            }
        }
    }
}

// TEMPLATE: EXE 0x4cf927

// FUNCTION: EXE 0x4cf952
// Closes the routing connection, leaving the room and the game
void EEWonNet::Disconnect()
{
    if (this->mRouting.get() == NULL)
        return;
    this->mIgnoreClose = true;
    this->mRouting->Close();
    this->mRouting = NULL;
    this->LeaveGroups();
    this->ClearGames();
    this->mServerName.erase();
}

// FUNCTION: EXE 0x4cf99b
// Cancels a pending room join and leaves the current room
void EEWonNet::LeaveGroups()
{
    if (this->mRouting.get() != NULL) {
        if (this->mJoinRoomId != 0xffff)
            this->mRouting->CancelJoinGroup(this->mJoinRoomId, true, false);
        if (this->mGameListPort != 0xffff)
            this->mRouting->LeaveGroup(this->mGameListPort, false);
    }
    this->mJoinRoomId = 0xffff;
    this->mGameListPort = 0xffff;
    this->mRoomName.erase();
}

// FUNCTION: EXE 0x4cf9fe
// Updates the hosted game object, cancels a pending game join and leaves the current game group
void EEWonNet::ClearGames()
{
    if (this->mRouting.get() != NULL) {
        this->UpdateGameObject();
        if (this->mJoinGroupId != 0xffff)
            this->mRouting->CancelJoinGroup(this->mJoinGroupId, true, false);
        if (this->mGameGroupId != 0xffff)
            this->mRouting->LeaveGroup(this->mGameGroupId, false);
    }
    this->mJoinGroupId = 0xffff;
    this->mGameGroupId = 0xffff;
}

// FUNCTION: EXE 0x4cfa56
// Refills the games box with the room's games that pass the in-progress, password and private filters
void EEWonNet::UpdateGameList()
{
    UWideString theName;
    std::map<unsigned short, EEWonNetGameInfo>::iterator theGame;
    bool theShowClosed;
    bool theShowPassworded;
    bool theShowFirewalled;
    bool thePassworded;
    EEAction* theAction;

    if (gUIManager->GetCurrentForm() != gUIManager->GetFormByID(kFormAvailableGames))
        return;
    theShowClosed = static_cast<UICheckBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesInProgressToggle))->GetIsChecked();
    theShowPassworded = static_cast<UICheckBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesPasswordToggle))->GetIsChecked();
    theShowFirewalled = static_cast<UICheckBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesPrivateToggle))->GetIsChecked();
    theAction = new EEAWonNet(3, UWideString("@@@@"), 0, 0, false, false, false);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    theGame = this->mGames.begin();
    while (true) {
        if (theGame == this->mGames.end())
            break;
        if (theGame->second.mRoomGroupId == this->mGameListPort) {
            theName = theGame->second.mGameName;
            thePassworded = theGame->second.mPassword.GetLength() > 0;
            if ((theShowClosed || (theGame->second.mFlags & 1)) && (theShowPassworded || !thePassworded)
                && (theShowFirewalled || !(theGame->second.mFlags & 2))) {
                theAction = new EEAWonNet(3, theName, theGame->second.mHumans, theGame->second.mPlayers,
                                          thePassworded, (theGame->second.mFlags & 2) != 0,
                                          !(theGame->second.mFlags & 1));
                gUserInterface->PostAction(theAction);
                theAction->Dereference();
            }
        }
        theGame++;
    }
    theAction = new EEAWonNet(3, UWideString("@@@"), 0, 0, false, false, false);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    this->mGamesChanged = false;
}

// FUNCTION: EXE 0x4cfc91
// Starts listing the room's players (requests the client list) unless a listing is running
void EEWonNet::RefreshGameList(UIControl* inGamesBox)
{
    if (this->mIsListingPlayers != false || this->mRouting.get() == NULL)
        return;
    this->mIsListingPlayers = true;
    this->mClients.clear();
    this->mRouting->GetClientList(new RoutingCompletion(&EEWonNet::ClientListCallback));
}

// FUNCTION: EXE 0x4cfce5
// Stores the client list and requests the room's members
void EEWonNet::ClientListCallback(RoutingOp* inOp)
{
    if (inOp->GetStatus() == 0) {
        this->mClients = reinterpret_cast<RoutingGetClientListOp*>(inOp)->mClientMap;
        this->mRouting->GetMembersOfGroup(this->mGameListPort, new RoutingCompletion(&EEWonNet::MembersCallback));
    } else {
        this->mIsListingPlayers = false;
    }
}

// FUNCTION: EXE 0x4cfd42
// Refills the player list box with the names of the room's members
void EEWonNet::MembersCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    RoutingMemberMap theMembers(reinterpret_cast<RoutingGetMembersOfGroupOp*>(inOp)->mMembers);
    RoutingMemberMap::const_iterator theMember = theMembers.begin();
    EEAction* theAction;

    if (theStatus != 0) {
        this->mIsListingPlayers = false;
    } else {
        theAction = new EEAWonNet(4, NULL, UWideString("@@@@"), false);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        for (; theMember != theMembers.end(); theMember++) {
            if (this->mClients.find(theMember->second->mClientId) == this->mClients.end())
                continue;
            theAction = new EEAWonNet(4, NULL,
                                      UWideString(this->ToString(this->mClients[theMember->second->mClientId]->mName).c_str()), false);
            gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
        theAction = new EEAWonNet(4, NULL, UWideString("@@@"), false);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        this->mIsListingPlayers = false;
    }
}

// DEAD-IN-AOC: base exe 0x4c91a3 (AoC: the CD key's implicit dtor 0x4cff2a survives, WONAPI uses it too). Partial:
// the base body also stores the name, fills the user's name, password and community, and shows a cancellable
// "logging in" message; a missing key gets a message of its own. (r25pre-won-2)
void EEWonNet::Login(const char* inName, const char* inPassword)
{
    CDKey theKey(kWonProductName);

    if (theKey.LoadFromRegistry(2, gCDKeyRegistryKey) && theKey.mValid) {
        this->mLoginOp = new GetCertOp(this->mUser);
        this->mLoginOp->mCompletion = new EEWonNetCompletion(&EEWonNet::LoginCallback);
        ((GetCertOp*)this->mLoginOp.get())->mB11D8 = false;
        this->mLoginOp->Run(15000);
    }
}

// DEAD-IN-AOC: base exe 0x4c9414. Partial: the base body also reports the failures by status (-1510..-1501) and
// stores the key back. (r25pre-won-2)
void EEWonNet::LoginCallback(RoutingOp* inOp)
{
    CDKey theKey(kWonProductName);

    if (static_cast<AsyncOp*>(inOp) != this->mLoginOp.get())
        return;
    if (inOp->GetStatus() == 0)
        gUserInterface->PostLoginDone();
    else
        theKey.SaveToRegistry(2, gCDKeyRegistryKey);
}

// DEAD-IN-AOC: base exe 0x4c9612 (AoC: GetHTTPDocumentOp's and AsyncOpWithTracker's implicit dtors 0x4cff46/
// 0x4cff95 and the tracker family 0x4d157f.. survive, WONAPI uses them too; GetMOTDOp itself is gone). (r25pre-won-2)
void EEWonNet::RequestMOTD()
{
    this->mMOTDOp = new GetMOTDOp(kWonProductName);
    this->mMOTDOp->mCompletion = new EEWonNetCompletion(&EEWonNet::MOTDCallback);
    this->mMOTDOp->Run(15000);
}

// DEAD-IN-AOC: base exe 0x4c975d (hands the game's and the system's message of the day to the UI thread: message
// 0x440, whose AoC handler is EEUserInterface::ShowMessageOfTheDay). (r25pre-won-2)
void EEWonNet::MOTDCallback(RoutingOp* inOp)
{
    UWideString theGameMOTD;
    UWideString theSystemMOTD;
    ByteBufferPtr theBuffer;
    GetMOTDOp* theOp = static_cast<GetMOTDOp*>((AsyncOp*)inOp);
    bool theFound = false;

    if (static_cast<AsyncOp*>(inOp) == (AsyncOp*)this->mMOTDOp.get() && inOp->GetStatus() == 0) {
        if (theOp->mIsNew[0]) {
            theBuffer = theOp->GetMOTD(false);
            theGameMOTD = GetBufferData(theBuffer);
            theFound = true;
        }
        if (theOp->mIsNew[1]) {
            theBuffer = theOp->GetMOTD(true);
            theSystemMOTD = GetBufferData(theBuffer);
            theFound = true;
        }
        if (theFound == true)
            gUserInterface->PostMOTD(theGameMOTD, theSystemMOTD);
    }
}

// DEAD-IN-AOC: base exe 0x4c9883 (asks the directory servers for the Titan servers). Partial: the base body also
// creates the user (+0x28) and the two ServerContexts (+0x18/+0x1c), adds four directory paths and shows a
// cancellable "connecting" message. (r25pre-won-2)
void EEWonNet::GetTitanServerList()
{
    this->mDirOp = new GetDirOp(this->mDirServers.get());
    this->mDirOp->mFlags = 0x703003a;
    this->mDirOp->AddPath(kWonProductDirectory);
    this->mDirOp->mCompletion = new EEWonNetCompletion(&EEWonNet::GetTitanServerListCompletion);
    this->mDirOp->Run(15000);
}

// DEAD-IN-AOC: base exe 0x4c9ad9 (AoC: the list<DirEntityPtr> push_back COMDATs _Insert 0x4d1e96 / _Incsize
// 0x4d2e31 survive, WONAPI uses them too). Partial: the base body also files the auth, DB proxy and firewall servers
// into their ServerContexts, starts the MOTD request and reports through the UI. (r25pre-won-2)
void EEWonNet::GetTitanServerListCompletion(RoutingOp* inOp)
{
    GetDirOp* theOp = static_cast<GetDirOp*>((AsyncOp*)inOp);
    DirEntityList::const_iterator theEntity;

    this->mServers.clear();
    if (static_cast<AsyncOp*>(inOp) != (AsyncOp*)this->mDirOp.get() || inOp->GetStatus() != 0)
        return;
    for (theEntity = theOp->mDirEntityList.begin(); theEntity != theOp->mDirEntityList.end(); theEntity++) {
        if ((*theEntity)->mName == kWonRoutingServerType)
            this->mServers.push_back(*theEntity);
    }
}

// DEAD-IN-AOC: base exe 0x4c9e16 (between the server-list completion 0x4c9ad9 and the account ops; AoC: the
// vector<std::string> push_back/clear COMDATs 0x4d1f0e.. and ServerContext's ??_G/dtor 0x4ceaec/0x4ceb08 survive
// because WONAPI uses them too). Partial: the base body first creates the lobby host and window (+0x20/+0x24); its
// directory comes from an exe UString (base 0x919364), here gStartupDirectory. (r25pre-won-2)
bool EEWonNet::Initialize()
{
    FSFileSpec theFile;
    UWideString theUnused;
    char theLine[256];
    unsigned long theCount = 0;
    unsigned long theIndex;

    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory);
    theFile.SetFile(UString("SierraCom.cfg"));
    this->mDirectoryServers.clear();
    if (theFile.FOpen("r") == 0) {
        while (fgets(theLine, 256, theFile) != NULL && theCount < 12) {
            if (theLine[strlen(theLine) - 1] == '\n')
                theLine[strlen(theLine) - 1] = 0;
            this->mDirectoryServers.push_back(std::string(theLine));
            theCount++;
        }
        theFile.Close();
    }
    this->mDirServers = new ServerContext;
    for (theIndex = 0; theIndex < this->mDirectoryServers.size(); theIndex++)
        this->mDirServers->AddAddress(IPAddr(this->mDirectoryServers[theIndex]));
    this->mIsBehindFirewall = false;
    this->GetTitanServerList();
    this->mRouting = new RoutingConnection;
    this->mJoinGroupId = 0xffff;
    this->mGameGroupId = 0xffff;
    this->mB187 = false;
    return true;
}

// FUNCTION: EXE 0x4cffb5
// Copies the joined game's info, when there is one
void EEWonNet::GetGameInfo(EEWonNetGameInfo& outInfo)
{
    if (this->mGameGroupId != 0xffff)
        outInfo = this->mGameInfo;
}

// FUNCTION: EXE 0x4cffd3
// Leaves the current game (ClearGames)
void EEWonNet::ClearGameList()
{
    this->ClearGames();
}

// FUNCTION: EXE 0x4cffd8
// Leaves the current room (LeaveGroups)
void EEWonNet::LeaveRoom()
{
    this->LeaveGroups();
}

// FUNCTION: EXE 0x4cffdd
// Passes the value to the lobby host's Fn2F2D20, when a lobby exists
void EEWonNet::FnCFFDD(long inArg)
{
    if (this->mLobby != NULL)
        this->mLobby->Fn2F2D20(inArg, 0);
}

// FUNCTION: EXE 0x4cfff2
// Calls the lobby window's Virtual18 when the window exists
void EEWonNet::FnCFFF2()
{
    // Fix: test the window that is called (this used to test mLobby).
    if (this->mLobbyWindow != NULL)
        this->mLobbyWindow->Virtual18();
}

// FUNCTION: EXE 0x4d0001
// Starts a WON account update with the new name and password and the e-mail flag
void EEWonNet::UpdateAccount(const UWideString& inName, const UWideString& inPassword, bool inFlag)
{
    SmartPtr<WONAccountOp> theOp = new WONAccountOp(this->mAuthServers, this->mUser);

    theOp->mL80 = 1;
    theOp->mIdentity = this->mUser;
    theOp->mPassword = std::string(inPassword);
    theOp->mName = this->ToWString(inName);
    theOp->mCompletion = new EEWonNetCompletion(&EEWonNet::UpdateAccountCallback);
    theOp->mAccount->Fn2CDC20();
    theOp->mAccount->Fn2CD9E0(true, inFlag);
    theOp->Run(15000);
}

// FUNCTION: EXE 0x4d0141
// Reports the account update's result in a message box (nothing when cancelled)
void EEWonNet::UpdateAccountCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    UWideString theMessage;

    gUserInterface->CloseForm(NULL);
    if (theStatus == 100002)
        return;
    if (theStatus == 0) {
        gResource->GetString(IDS_SUCCESSFULLY_MODIFIED_YOUR_ACCOUNT, theMessage);
    } else {
        EEWonNetError("EEWonNet::ModifyWONAccount()", theStatus);
        gResource->GetString(IDS_COULD_NOT_MODIFY_YOUR_ACCOUNT, theMessage);
    }
    gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
}

// FUNCTION: EXE 0x4d01c3
// Requests the current account info and shows a cancel box
void EEWonNet::OpenUpdateAccount()
{
    UWideString theMessage;

    this->mAccountOp = new WONGetAccountOp(this->mAuthServers, this->mUser);
    this->mAccountOp->mL80 = 1;
    this->mAccountOp->mIdentity = this->mUser;
    this->mAccountOp->mCompletion = new EEWonNetCompletion(&EEWonNet::GetAccountCallback);
    this->mAccountOp->Run(15000);
    this->mAccountOp->mAccount->Fn2CD9E0(true, false);
    gResource->GetString(IDS_GETTING_CURRENT_USER_INFO, theMessage);
    gUIManager->DisplayCancel(theMessage, 1, (UIForm*)this->mAccountOp.get(), CancelOpCallback,
                              gUIManager->GetFormByID(kFormUpdateSierraAccount));
}

// FUNCTION: EXE 0x4d02ae
// Kills the pending WON op (the cancel box's callback)
static void CancelOpCallback(UIForm* inForm)
{
    reinterpret_cast<AsyncOp*>(inForm)->Kill(true);
}

// FUNCTION: EXE 0x4d02ba
// Fills the account form's e-mail box and checkbox from the account info, or reports the error
void EEWonNet::GetAccountCallback(RoutingOp* inOp)
{
    std::string theName;
    std::wstring theWide;
    WONStatus theStatus = inOp->GetStatus();
    UWideString theMessage;
    bool theChecked;

    if (theStatus != 100002) {
        if (theStatus == 0) {
            gUserInterface->CloseForm(NULL);
            theChecked = reinterpret_cast<WONGetAccountOp*>(inOp)->mAccount->Fn2CDAE0(true);
            theName = reinterpret_cast<WONGetAccountOp*>(inOp)->mEmail;
            static_cast<UICheckBox*>(gUIManager->GetFormByID(kFormUpdateSierraAccount)->GetControl(kUpdateAccountSendMeEmailCheckbox))->SetChecked(theChecked);
            static_cast<UIEditBox*>(gUIManager->GetFormByID(kFormUpdateSierraAccount)->GetControl(kUpdateAccountEnterEmailEditBox))->SetText(UWideString(theName.c_str()));
        } else {
            gUserInterface->CloseForm(NULL);
            gResource->GetString(IDS_THERE_WAS_ERROR_GETTING_CURRENT_USER, theMessage);
            gUIManager->DisplayCancel(theMessage, 1, (UIForm*)this->mAccountOp.get(), CancelOpCallback,
                                      gUIManager->GetFormByID(kFormUpdateSierraAccount));
        }
    }
}

// FUNCTION: EXE 0x4d040c
// Shows an incoming chat line in the games screen or lobby and remembers a whisper's sender for /r
void EEWonNet::OnChat(RoutingOp* inOp)
{
    UWideString theFrom;
    UWideString theU64;
    UWideString theU50;
    UWideString theText;
    RoutingClientInfoPtr theClient = this->mRouting->GetClient(reinterpret_cast<RoutingPeerChatOp*>(inOp)->mClientId);
    EEAction* theAction;

    if (theClient.get() != NULL && reinterpret_cast<RoutingPeerChatOp*>(inOp)->mL80 != 0) {
        theFrom = UWideString(this->ToString(theClient->mName).c_str());
        if (reinterpret_cast<RoutingPeerChatOp*>(inOp)->mFlags.mBit2
            && theClient->mClientId != this->mRouting->mClientId)
            this->mReplyClientId = reinterpret_cast<RoutingPeerChatOp*>(inOp)->mClientId;
        theText = UWideString(this->ToString(reinterpret_cast<RoutingPeerChatOp*>(inOp)->mText).c_str());
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableGames)) {
            theAction = new EEAWonNet(6, static_cast<UITextLabel*>(gUIManager->GetCurrentForm()->GetControl(15)), theFrom, theText,
                                      theClient->mClientId == this->mRouting->GetMyClientId());
        } else if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormLobby)) {
            theAction = new EEAWonNet(7, NULL, theFrom, theText, theClient->mClientId == this->mRouting->GetMyClientId());
        } else {
            return;
        }
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
    }
}

// FUNCTION: EXE 0x4d0612
// Does nothing (enters and leaves a lock)
void EEWonNet::OnDataObjectChanged(RoutingOp* inOp)
{
    this->mLock8->Enter();
    this->mLock8->Leave();
}

// FUNCTION: EXE 0x4d062b
// Adds a new game object to the game list (and to the joined game's info) and marks the list changed
void EEWonNet::OnDataObjectCreated(RoutingOp* inOp)
{
    this->mLock->Enter();
    this->ImportGameInfo((unsigned char*)GetBufferData(inOp->GetData()), this->mGames[inOp->GetU64()]);
    {
        unsigned short theId = inOp->GetU64();
        this->mGames[inOp->GetU64()];
        this->mGames[theId];
    }
    if (this->mGameGroupId == inOp->GetU64())
        this->mGameInfo = this->mGames[inOp->GetU64()];
    this->mGamesChanged = true;
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4d0714
// Removes a deleted game from the list; leaves it and rejoins the room when it was the joined game
void EEWonNet::OnDataObjectDeleted(RoutingOp* inOp)
{
    this->mLock->Enter();
    {
        unsigned short theId = inOp->GetU64();
        this->mGames[inOp->GetU64()];
        this->mGames[theId];
    }
    if (this->mGameGroupId == inOp->mU64) {
        this->ClearGames();
        this->RejoinRoom(false);
    }
    if (this->mGames.find(inOp->GetU64()) != this->mGames.end())
        this->mGames.erase(inOp->GetU64());
    this->mGamesChanged = true;
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4d07c2
// Updates a modified game object in the game list (and the joined game's info)
void EEWonNet::OnDataObjectModified(RoutingOp* inOp)
{
    this->mLock->Enter();
    this->ImportGameInfo((unsigned char*)GetBufferData(reinterpret_cast<RoutingDataObjectModifiedOp*>(inOp)->GetData()),
                   this->mGames[inOp->GetU64()]);
    {
        unsigned short theId = inOp->GetU64();
        this->mGames[inOp->GetU64()];
        this->mGames[theId];
    }
    if (this->mGameGroupId == inOp->GetU64())
        this->mGameInfo = this->mGames[inOp->GetU64()];
    this->mGamesChanged = true;
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4d08ab
// Updates a room's maximum players and flags the room list when it is the current room
void EEWonNet::OnGroupMaxPlayersChanged(RoutingOp* inOp)
{
    this->mLock->Enter();
    if (this->mRooms.find(inOp->GetU64()) != this->mRooms.end())
        this->mRooms[inOp->GetU64()].mMaxPlayers = inOp->mLinkId;
    if (this->mGameListPort == inOp->mU64)
        this->mB18E = true;
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4d0929
// Handles the client registration: requests the group list, or reports the failure (name in use) and disconnects
void EEWonNet::RegisterCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();
    UWideString theMessage;

    if (theStatus == 100002)
        return;
    if (inOp->GetStatus() != 0) {
        if (theStatus != -2503)
            gResource->GetString(IDS_UNABLE_TO_ACCESS_SERVER_AT_THIS_TIME, theMessage);
        else
            gResource->GetString(IDS_SIERRA_COM_USER_IS_ALREADY, theMessage);
        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
        this->Disconnect();
    } else {
        gWonClientId = reinterpret_cast<RoutingRegisterClientOp*>(inOp)->mClientId;
        this->mRouting->GetGroupList(0xff, new RoutingCompletion(&EEWonNet::ChatRoomListCallback));
    }
    if (theStatus != 0)
        EEWonNetError("ESReport::RegisterCompletion", theStatus);
}

// FUNCTION: EXE 0x4d09f9
// Reports a failed chat send
void EEWonNet::SendChatCallback(RoutingOp* inOp)
{
    WONStatus theStatus = inOp->GetStatus();

    if (theStatus != 0)
        EEWonNetError("EEWonNet::SendChatMessage", theStatus);
}

// FUNCTION: EXE 0x4d0a14
// Sends a chat line to the room; /i and /u ignore or unignore a user, /t whispers, /r replies to the last whisper
void EEWonNet::SendChat(const UWideString& inText)
{
    std::list<unsigned short> theRecipients;
    UWideString theName;
    UWideString theMessage;
    UWideString theText;
    RoutingClientInfoPtr theClient;
    int theEnd;
    EEAction* theAction;

    if (inText.GetLength() >= 4 && inText[2] == '/') {
        switch (inText[3]) {
        case 'I':
        case 'i':
        case 'U':
        case 'u':
            if (inText.GetLength() >= 6) {
                if (inText[5] == '"') {
                    for (theEnd = 6; theEnd < inText.GetLength() && inText[theEnd] != '"'; theEnd++)
                        ;
                    theName.CopyString((const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), 6),
                                       theEnd - 6);
                } else {
                    for (theEnd = 5; theEnd < inText.GetLength() && inText[theEnd] != ' '; theEnd++)
                        ;
                    theName.CopyString((const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), 5),
                                       theEnd - 5);
                }
                this->mRouting->FindClient(this->ToWString(theName), theClient);
                if (theClient.get() != NULL) {
                    if (inText[3] == 'i' || inText[3] == 'I') {
                        this->mRouting->IgnoreClient(theClient, true);
                        gResource->GetString(IDS_IS_NOW_IGNORING, theMessage);
                    } else {
                        this->mRouting->IgnoreClient(theClient, false);
                        gResource->GetString(IDS_IS_NO_LONGER_IGNORING, theMessage);
                    }
                }
                theMessage += theName;
                theAction = new EEAWonNet(6, static_cast<UITextLabel*>(gUIManager->GetCurrentForm()->GetControl(15)),
                                          UWideString(this->ToString(this->mUser->mName).c_str()), theMessage, true);
                gUserInterface->PostAction(theAction);
                theAction->Dereference();
            }
            break;
        case 'T':
        case 't':
            if (inText.GetLength() >= 8) {
                if (inText[5] == '"') {
                    for (theEnd = 6; theEnd < inText.GetLength() && inText[theEnd] != '"'; theEnd++)
                        ;
                    theName.CopyString((const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), 6),
                                       theEnd - 6);
                    theEnd += 2;
                } else {
                    for (theEnd = 5; theEnd < inText.GetLength() && inText[theEnd] != ' '; theEnd++)
                        ;
                    theName.CopyString((const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), 5),
                                       theEnd - 5);
                    theEnd += 1;
                }
                unsigned char* theRest = PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), theEnd);
                this->mRouting->FindClient(this->ToWString(theName), theClient);
                if (theClient.get() != NULL) {
                    gResource->GetString(IDS_WHISPERED, theMessage);
                    theMessage += reinterpret_cast<const char*>(theRest);
                    theRecipients.push_back(this->mRouting->mClientId);
                    theRecipients.push_back(theClient->mClientId);
                    this->mRouting->SendChatTo(4, this->ToWString(theMessage), theRecipients, false);
                }
            }
            break;
        case 'R':
        case 'r':
            // Fix: "no reply target yet" is 0xffff (the constructor's value), not 0.
            if (inText.GetLength() >= 6 && this->mReplyClientId != 0xffff) {
                unsigned char* theRest = PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)inText), 5);
                gResource->GetString(IDS_WHISPERED, theMessage);
                theMessage += reinterpret_cast<const char*>(theRest);
                theRecipients.push_back(this->mRouting->mClientId);
                theRecipients.push_back(this->mReplyClientId);
                this->mRouting->SendChatTo(4, this->ToWString(theMessage), theRecipients, false);
            }
            break;
        case 'W':
        case 'w':
            break;
        default:
            this->mRouting->SendChat(this->ToWString(inText), 0, this->mGameListPort, NULL);
            break;
        }
    } else {
        this->mRouting->FindClient(this->ToWString(theName), theClient);
        this->mRouting->SendChat(this->ToWString(inText), 0, this->mGameListPort, new RoutingCompletion(&EEWonNet::SendChatCallback));
    }
}

// FUNCTION: EXE 0x4d0f8c
// Converts a multibyte string to a wide string (empty for NULL or "")
std::wstring EEWonNet::ToWString(const char* inString)
{
    if (inString == NULL || *inString == 0)
        return std::wstring();
    int theLength = PlatMbsLen(reinterpret_cast<const unsigned char*>(inString));
    wchar_t* theBuffer = new wchar_t[theLength + 1];
    this->A2W(theBuffer, inString, theLength);
    theBuffer[theLength] = 0;
    std::wstring theString(theBuffer);
    delete[] theBuffer;
    return theString;
}

// FUNCTION: EXE 0x4d104d
// Publishes the joined game's info to its data object and keeps a copy
void EEWonNet::SetGameInfo(EEWonNetGameInfo& inInfo)
{
    std::string theType = kWonGameObject;
    unsigned char* theData;
    unsigned long theSize;

    if (this->mGameGroupId == 0xffff)
        return;
    this->ExportGameInfo(inInfo, theData, theSize);
    this->mRouting->ModifyDataObject(this->mGameGroupId, theType, this->mGameName, 0, 0, new ByteBuffer(theData, theSize, false),
                               NULL);
    delete theData;
    this->mGameInfo = inInfo;
}

// FUNCTION: EXE 0x4d1104
// Refills the rooms box (games and players per room), keeps the selection and shows the server and totals
void EEWonNet::UpdateRoomList(EEUICWonNetRoomsBox* inRooms, UITextLabel* inLabel16, UITextLabel* inLabel14)
{
    UWideString theServer(this->ToString(this->mServerName).c_str());
    UWideString theFormat;
    UWideString theText;
    EEUICWNRBLine theLine;
    std::map<unsigned short, unsigned long> theGameCounts;
    std::map<unsigned short, unsigned long> thePlayerCounts;

    unsigned long theSelection;
    unsigned long theGames = 0;
    unsigned long thePlayers = 0;

    this->mLock->Enter();
    long theNoSelection = inRooms->GetCurrentSelection(theSelection);
    for (std::map<unsigned short, EEWonNetGameInfo>::iterator theGame = this->mGames.begin(); theGame != this->mGames.end(); theGame++) {
        if ((theGame->second.mFlags & 1) != 0) {
            if (theGameCounts.find(theGame->second.mRoomGroupId) != theGameCounts.end()) {
                theGameCounts[theGame->second.mRoomGroupId]++;
                thePlayerCounts[theGame->second.mRoomGroupId] += theGame->second.mHumans;
            } else {
                theGameCounts[theGame->second.mRoomGroupId] = 1;
                thePlayerCounts[theGame->second.mRoomGroupId] = theGame->second.mHumans;
            }
        } else {
            theGames++;
            thePlayers += theGame->second.mHumans;
        }
    }
    inRooms->DeleteAllEntries();
    for (std::map<unsigned short, EEWonNetRoomInfo>::iterator theRoom = this->mRooms.begin(); theRoom != this->mRooms.end(); theRoom++) {
        theLine.mDescription = theRoom->second.mDescription;
        theLine.mName = theRoom->second.mName;
        theLine.mNumPlayers = theRoom->second.mMaxPlayers;
        if (theGameCounts.find((unsigned short)theRoom->second.mId) != theGameCounts.end())
            theLine.mNumGames = (*theGameCounts.insert(std::map<unsigned short, unsigned long>::value_type(
                                (unsigned short)theRoom->second.mId, 0)).first).second;
        else
            theLine.mNumGames = 0;
        if (thePlayerCounts.find((unsigned short)theRoom->second.mId) != thePlayerCounts.end())
            theLine.mNumPlayers += (*thePlayerCounts.insert(std::map<unsigned short, unsigned long>::value_type(
                                 (unsigned short)theRoom->second.mId, 0)).first).second;
        inRooms->AppendLine(theLine);
    }
    if (theNoSelection == 0 && theSelection < inRooms->GetLineCount())
        inRooms->ChangeSelectionIndex(theSelection, true, false);
    gResource->GetString(IDS_SETUP_PLAYERS, theFormat);
    sprintf(theText, "%d  (%d %s)", theGames, thePlayers, (const char*)theFormat);
    inLabel14->SetText(theText);
    inLabel16->SetText(theServer);
    this->mRoomListChanged = false;
    this->mLock->Leave();
}

// FUNCTION: EXE 0x4d146d
// Converts a wide string to multibyte into the caller's buffer
char* EEWonNet::W2A(char* outString, int inSize, const wchar_t* inString, int inLength)
{
    if (outString == NULL)
        return NULL;
    outString[0] = 0;
    if (inString == NULL)
        return outString;
    unsigned long theLength = WideCharToMultiByte(CP_ACP, 0, inString, inLength, outString, inSize, NULL, NULL);
    if (theLength > 0)
        outString[theLength] = 0;
    return outString;
}

// TEMPLATE: EXE 0x4d14a9
// (EEWonNet::ToString, the class's inline)

// FUNCTION: EXE 0x4cf7e4
// Sets the cancel flag it was given (the cancel box of a connect or join)
static void CancelConnectCallback(UIForm* inForm)
{
    *(bool*)inForm = true;
}

// FUNCTION: EXE 0x4d1564
// Closes back to the main menu (after the lost-connection message)
static void CloseMessageCallback(UIForm* inForm)
{
    gUserInterface->CloseForm(gUIManager->GetFormByID(kFormMainMenu));
}

// TEMPLATE: EXE 0x4cd775
// TEMPLATE: EXE 0x4cd9ba
// TEMPLATE: EXE 0x4ce00f
// TEMPLATE: EXE 0x4ce01c
// TEMPLATE: EXE 0x4ce3f6
// TEMPLATE: EXE 0x4d15d2
// TEMPLATE: EXE 0x4d165a
// TEMPLATE: EXE 0x4d1685
// TEMPLATE: EXE 0x4d16d8
// TEMPLATE: EXE 0x4d1710
// TEMPLATE: EXE 0x4d1744
// TEMPLATE: EXE 0x4d182a
// TEMPLATE: EXE 0x4d1880
// TEMPLATE: EXE 0x4d18d1
// TEMPLATE: EXE 0x4d18e5
// TEMPLATE: EXE 0x4d1921
// TEMPLATE: EXE 0x4d1a12
// TEMPLATE: EXE 0x4d1a2e
// TEMPLATE: EXE 0x4d1ac0
// TEMPLATE: EXE 0x4d1ba2
// TEMPLATE: EXE 0x4d1c02
// TEMPLATE: EXE 0x4d1e5e
// TEMPLATE: EXE 0x4d1e96
// TEMPLATE: EXE 0x4d1eea
// TEMPLATE: EXE 0x4d1fca
// TEMPLATE: EXE 0x4d20f5
// TEMPLATE: EXE 0x4d212d
// TEMPLATE: EXE 0x4d22a4
// TEMPLATE: EXE 0x4d26e3
// TEMPLATE: EXE 0x4d299b
// TEMPLATE: EXE 0x4d2a1e
// TEMPLATE: EXE 0x4d2b19
// TEMPLATE: EXE 0x4d2d6c
// TEMPLATE: EXE 0x4d318f
// TEMPLATE: EXE 0x4d3445
// TEMPLATE: EXE 0x4d347f
// TEMPLATE: EXE 0x4d3613
// TEMPLATE: EXE 0x4d363e
// TEMPLATE: EXE 0x4d38f6
// TEMPLATE: EXE 0x4d3932
// TEMPLATE: EXE 0x4d3ac6
// TEMPLATE: EXE 0x4d3b5f
// TEMPLATE: EXE 0x4d3ba1
// TEMPLATE: EXE 0x4d3c0c
// TEMPLATE: EXE 0x4d3c4e
// TEMPLATE: EXE 0x4d3c92
// TEMPLATE: EXE 0x4d17ea
// TEMPLATE: EXE 0x4d2df7
// TEMPLATE: EXE 0x4d3f4c
// TEMPLATE: EXE 0x4d3cbd
// TEMPLATE: EXE 0x4d3cfe
// TEMPLATE: EXE 0x4d3d40
// TEMPLATE: EXE 0x4d3d84
// TEMPLATE: EXE 0x4d3dfd
// TEMPLATE: EXE 0x4d3f29
// TEMPLATE: EXE 0x4d3f3b
// TEMPLATE: EXE 0x4d3f5c
// TEMPLATE: EXE 0x4d3f9a
// TEMPLATE: EXE 0x4d3fc7
// TEMPLATE: EXE 0x4d16ad
// ??1?$_Tree@GU?$pair@$$CBGV?$SmartPtr@URoutingMemberInfo@WONAPI@@@WONAPI@@@std@@U_Kfn@?$map@GV?$SmartPtr@URoutingMemberInfo@WONAPI@@@WONAPI@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingMemberInfo@WONAPI@@@WONAPI@@@std@@@4@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingMemberInfo@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ
// TEMPLATE: EXE 0x4d1758
// ??1?$_Tree@GU?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@U_Kfn@?$map@GV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@@4@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAE@XZ
// TEMPLATE: EXE 0x4d1783
// ??4?$_Tree@GU?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@U_Kfn@?$map@GV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@@4@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingGroupInfo@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@QAEAAV01@ABV01@@Z
// TEMPLATE: EXE 0x4d17b7
// ??1?$list@GV?$allocator@G@std@@@std@@QAE@XZ
// TEMPLATE: EXE 0x4d1985
// ?clear@?$_Tree@GU?$pair@$$CBGUEEWonNetRoomInfo@@@std@@U_Kfn@?$map@GUEEWonNetRoomInfo@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@3@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@2@$0A@@std@@QAEXXZ
// TEMPLATE: EXE 0x4d1999
// ?find@?$_Tree@GU?$pair@$$CBGUEEWonNetRoomInfo@@@std@@U_Kfn@?$map@GUEEWonNetRoomInfo@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@3@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@2@$0A@@std@@QAE?AViterator@12@ABG@Z
// TEMPLATE: EXE 0x4d1cbc
// ?erase@?$_Tree@GU?$pair@$$CBGUEEWonNetRoomInfo@@@std@@U_Kfn@?$map@GUEEWonNetRoomInfo@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@3@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGUEEWonNetRoomInfo@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@0@Z
// TEMPLATE: EXE 0x4d1d2d
// ?_Copy@?$_Tree@GU?$pair@$$CBGV?$SmartPtr@URoutingClientInfo@WONAPI@@@WONAPI@@@std@@U_Kfn@?$map@GV?$SmartPtr@URoutingClientInfo@WONAPI@@@WONAPI@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingClientInfo@WONAPI@@@WONAPI@@@std@@@4@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGV?$SmartPtr@URoutingClientInfo@WONAPI@@@WONAPI@@@std@@@2@$0A@@std@@IAEXABV12@@Z
// TEMPLATE: EXE 0x4d2084
// ?erase@?$_Tree@GU?$pair@$$CBGUEEWonNetGameInfo@@@std@@U_Kfn@?$map@GUEEWonNetGameInfo@@U?$less@G@std@@V?$allocator@U?$pair@$$CBGUEEWonNetGameInfo@@@std@@@3@@2@U?$less@G@2@V?$allocator@U?$pair@$$CBGUEEWonNetGameInfo@@@std@@@2@$0A@@std@@QAE?AViterator@12@V312@0@Z
// TEMPLATE: EXE 0x4d2258
// ??0EEWonNetGameInfo@@QAE@ABU0@@Z
// TEMPLATE: EXE 0x4d23ef
// ?erase@?$_Tree@VIPAddr@WONAPI@@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@U_Kfn@?$map@VIPAddr@WONAPI@@KU?$less@VIPAddr@WONAPI@@@std@@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@@4@U?$less@VIPAddr@WONAPI@@@4@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@$0A@@std@@QAE?AViterator@12@V312@@Z
// TEMPLATE: EXE 0x4d26a7
// ?_Erase@?$_Tree@VIPAddr@WONAPI@@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@U_Kfn@?$map@VIPAddr@WONAPI@@KU?$less@VIPAddr@WONAPI@@@std@@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@@4@U?$less@VIPAddr@WONAPI@@@4@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@$0A@@std@@IAEXPAU_Node@12@@Z
// TEMPLATE: EXE 0x4d3dc5
// ?_Inc@const_iterator@?$_Tree@VIPAddr@WONAPI@@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@U_Kfn@?$map@VIPAddr@WONAPI@@KU?$less@VIPAddr@WONAPI@@@std@@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@@4@U?$less@VIPAddr@WONAPI@@@4@V?$allocator@U?$pair@$$CBVIPAddr@WONAPI@@K@std@@@4@$0A@@std@@QAEXXZ
// TEMPLATE: EXE 0x4d3f19
// ?_Destroy@std@@YAXPAU?$pair@$$CBVIPAddr@WONAPI@@K@1@@Z
// TEMPLATE: EXE 0x4d3f7b
// ?_Construct@std@@YAXPAU?$pair@$$CBGUEEWonNetRoomInfo@@@1@ABU21@@Z
// TEMPLATE: EXE 0x4ceaec
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4ceb08
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d15f7
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d161a
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d1f0e
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d1f5e
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d23b5
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d2ebc
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3be5
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3e35
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3e63
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3e94
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3ebc
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d3ee3
// (r25pre-won-2: emitted by the DEAD-IN-AOC Initialize; ServerContext / vector<std::string>)
// TEMPLATE: EXE 0x4d1b31
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d1d8d
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d1dfe
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d21e7
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d2a5a
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d2add
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d2cad
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4d2d30
// (a map family's member, named by its caller)
// TEMPLATE: EXE 0x4cff2a
// (CDKey's implicit dtor, emitted by the DEAD-IN-AOC Login/LoginCallback)
// TEMPLATE: EXE 0x4cff46
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4cff95
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d157f
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d1a6a
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d1a95
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d22d3
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d2344
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d2e31
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d3af1
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)
// TEMPLATE: EXE 0x4d3b23
// (r25pre-won-2: emitted by the DEAD-IN-AOC MOTD request (GetHTTPDocumentOp / tracker) or server list)

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body of EEE2DMarchingIcons::PrepareQueue)
void EEWonNet::PlayerJoined()
{
}
