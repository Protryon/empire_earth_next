// LAN and direct-connect lobby logic: hosting and joining peer games over TCP (captain relays chat and
// staging), and UDP broadcast discovery of LAN games
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONLobbyEventTypes.h"

namespace WONAPI {
AuthContext* GetLobbyAuthContext(); // the lobby's authentication context (cdecl)

// FUNCTION: WON 0x007dfb90
// Links the socket and a LAN client both ways, unlinking the previous client
void LobbyLanSocket::SetClient(LobbyLanClient* inClient)
{
    if (this->mClient.get() != NULL)
        this->mClient->mSocket = NULL;
    this->mClient = inClient;
    if (this->mClient.get() != NULL)
        this->mClient->mSocket = this;
}
// FUNCTION: WON 0x007dfc00
// Creates the LAN logic with an empty client list and empty LAN and direct-connect game lists
LanLogic::LanLogic()
{
    this->mNextClientId = 0;
    this->mLastServerPing = 0;
    this->mIsCaptain = false;
    this->mRequirePeerAuth = false;
    this->mClients = new LobbyClientList;
    this->mLanGames = new LobbyGameList(1);
    this->mDirectGames = new LobbyGameList(3);
}

// FUNCTION: WON 0x007dfef0
// Destroys the LAN logic (members only)
LanLogic::~LanLogic() {}


// FUNCTION: WON 0x007e00a0
// Looks up the direct-connect, LAN name, LAN port and password dialogs from the lobby layout



























































void LanLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mDirectConnectDialog, "DirectConnectDialog", __FILE__, 105);
    SafeGetComponent(inConfig, this->mLanNameDialog, "LanNameDialog", __FILE__, 106);
    SafeGetComponent(inConfig, this->mLanPortDialog, "LanPortDialog", __FILE__, 107);
    SafeGetComponent(inConfig, this->mPasswordDialog, "PasswordDialog", __FILE__, 108);
}

// FUNCTION: WON 0x007e0110
// Returns the next LAN client id, skipping 0 and 0xffff
unsigned short LanLogic::GetNextClientId()
{
    if (this->mNextClientId == 0xffff || this->mNextClientId == 0)
        ++this->mNextClientId;
    return this->mNextClientId++;
}

// FUNCTION: WON 0x007e0140
// Returns true while hosting (listening) or connected to a host
bool LanLogic::IsConnected()
{
    return this->mListenSocket.get() != NULL || this->mServerSocket.get() != NULL;
}

// FUNCTION: WON 0x007e0160
// Returns true when this player hosts the LAN game
bool LanLogic::IsCaptain() { return this->mIsCaptain; }

// FUNCTION: WON 0x007e0170
// Returns the players in the LAN game
LobbyClientList* LanLogic::GetClientList() { return this->mClients.get(); }

// FUNCTION: WON 0x007e0180
// Returns the local player's client, or NULL
LobbyClient* LanLogic::GetMyClient() { return this->mMyClient.get(); }

// FUNCTION: WON 0x007e0190
// Leaves the LAN game: stops certificate refresh, closes the listen, host and player sockets, clears the players
void LanLogic::CloseConnections()
{
    AuthContext* anAuthContext = GetLobbyAuthContext();
    if (anAuthContext != NULL)
        anAuthContext->SetDoAutoRefresh(false);
    this->mNextClientId = 0;
    this->mIsCaptain = false;
    this->mRequirePeerAuth = false;
    if (this->mListenSocket.get() != NULL) {
        this->mListenSocket->Kill();
        this->mListenSocket = NULL;
    }
    if (this->mServerSocket.get() != NULL) {
        this->mServerSocket->QueueClose(-1);
        this->mServerSocket = NULL;
    }
    if (this->mNetworkTimer.get() != NULL) {
        this->mNetworkTimer->Kill(true);
        this->mNetworkTimer = NULL;
    }
    for (std::set<SmartPtr<LobbyLanSocket> >::iterator anItr = this->mPendingSockets.begin();
         anItr != this->mPendingSockets.end(); ++anItr)
        (*anItr)->QueueClose(-1);
    this->mMyClient = NULL;
    this->mPendingSockets.clear();
    this->mClients->Clear();
}

// FUNCTION: WON 0x007e0300
// Host: announces the departed player and drops its socket; joiner: reports the lost host connection
void LanLogic::HandleSocketClosed(AsyncOp* inOp)
{
    LobbyLanSocket* aSocket = static_cast<LobbyLanSocket*>(
        static_cast<SocketOp*>(inOp)->mSocket.get());
    if (aSocket == NULL)
        return;
    if (this->IsCaptain()) {
        if (aSocket->mClient.get() != NULL) {
            WriteBuffer aBuffer(2, true);
            aBuffer.AppendByte(4);
            aBuffer.AppendShort(aSocket->mClient->GetClientId());
            aSocket->SetClient(NULL);
            this->BroadcastMessage(aBuffer.ToByteBuffer(true).get(), true);
        }
        this->mPendingSockets.erase(aSocket);
    } else if (aSocket == this->mServerSocket.get()) {
        this->mServerSocket->Kill();
        this->mServerSocket = NULL;
        LobbyGameDisconnected(true);
    }
}

// FUNCTION: WON 0x007e04e0
// Host: drops sockets silent for 5 minutes; joiner: pings the host after 2 idle minutes; then rearms
void LanLogic::HandleNetworkTimer(AsyncOp* inOp)
{
    if (inOp != this->mNetworkTimer.get())
        return;
    if (this->IsCaptain()) {
        long aTime = time(NULL);
        for (std::set<SmartPtr<LobbyLanSocket> >::iterator anItr = this->mPendingSockets.begin();
             anItr != this->mPendingSockets.end(); ++anItr)
            if (aTime - (*anItr)->mLastMessageTime > 300)
                (*anItr)->QueueClose(1000);
    } else if (this->mServerSocket.get() != NULL && time(NULL) - this->mLastServerPing >= 120) {
        WriteBuffer aBuffer(2, true);
        aBuffer.AppendByte(7);
        this->SendMessageToCaptain(aBuffer.ToByteBuffer(true).get());
    }
    this->StartNetworkTimer();
}
// FUNCTION: WON 0x007e0620
// Starts (creating if needed) the 2-minute network timer
void LanLogic::StartNetworkTimer()
{
    if (this->mNetworkTimer.get() == NULL) {
        this->mNetworkTimer = new AsyncOp;
        this->mNetworkTimer->mCompletion = new LogicCompletion<LanLogic>(&LanLogic::HandleNetworkTimer, this);
    }
    this->mNetworkTimer->RunAsTimer(120000);
}

// FUNCTION: WON 0x007e0760
// Host: answers a join request: rejects other subnets (LAN) or a wrong password, else adds and announces the player
void LanLogic::HandleJoinRequest(ReadBuffer& inBuffer, LobbyLanSocket* inSocket)
{
    if (this->mMyClient.get() == NULL)
        return;
    LobbyGame* aGame = GetMyLobbyGame();
    if (aGame == NULL)
        return;
    std::wstring aName;
    std::wstring aPassword;
    inBuffer.ReadWString(aName, 2);
    inBuffer.ReadWString(aPassword, 2);
    WriteBuffer aReply(2, true);
    aReply.AppendByte(2);
    if (aGame->GetGameType() == 1) {
        IPAddr aLocalAddr = IPAddr::GetLocalAddr();
        const char* aPeerBytes = inSocket->GetDestAddr().GetSixByte();
        const char* aLocalBytes = aLocalAddr.GetSixByte();
        if (aPeerBytes[2] != aLocalBytes[2] || aPeerBytes[3] != aLocalBytes[3]) {
            aReply.AppendShort(3);
            inSocket->QueueOp(new SendMsgOp(aReply.ToByteBuffer(true).get()), -1);
            return;
        }
    }
    if (aPassword != std::wstring(aGame->mPassword)) {
        aReply.AppendShort(2);
        inSocket->QueueOp(new SendMsgOp(aReply.ToByteBuffer(true).get()), -1);
        return;
    }
    SmartPtr<LobbyLanClient> aClient = new LobbyLanClient;
    aClient->mName = aName;
    aClient->mClientId = this->GetNextClientId();
    this->mClients->AddClient(aClient.get());
    WriteBuffer aNotify(2, true);
    aNotify.AppendByte(3);
    aNotify.AppendShort(aClient->GetClientId());
    aNotify.AppendWString(aClient->mName, 2);
    this->BroadcastMessage(aNotify.ToByteBuffer(true).get(), true);
    inSocket->SetClient(aClient.get());
    aReply.AppendShort(0);
    aReply.AppendShort(this->mMyClient->GetClientId());
    aReply.AppendShort(aClient->GetClientId());
    aReply.AppendShort(this->mClients->mClients.size());
    for (LobbyClientMap::iterator anItr = this->mClients->mClients.begin();
         anItr != this->mClients->mClients.end(); ++anItr) {
        LobbyClient* aListedClient = anItr->second.get();
        aReply.AppendShort(aListedClient->GetClientId());
        aReply.AppendWString(aListedClient->mName, 2);
    }
    this->SendMessageToClient(aClient.get(), aReply.ToByteBuffer(true).get());
}
// FUNCTION: WON 0x007e0f90
// Reads the host's join reply into the player list and the game's captain; returns its status, 1 on bad data
int LanLogic::ReadJoinReply(const ByteBuffer* inBuffer, LobbyGame* inGame)
{
    if (inBuffer == NULL || this->mServerSocket.get() == NULL || inGame == NULL)
        return 1;
    ReadBuffer aBuffer(inBuffer->data(), inBuffer->length());
    try {
        if (aBuffer.ReadByte() != 2)
            return 1;
        short aStatus = aBuffer.ReadShort();
        if (aStatus != 0)
            return aStatus;
        unsigned short aCaptainId = aBuffer.ReadShort();
        unsigned short aMyId = aBuffer.ReadShort();
        inGame->mCaptainId = aCaptainId;
        unsigned short aNumClients = aBuffer.ReadShort();
        for (int i = 0; i < aNumClients; ++i) {
            unsigned short anId = aBuffer.ReadShort();
            std::wstring aName;
            aBuffer.ReadWString(aName, 2);
            SmartPtr<LobbyLanClient> aClient = new LobbyLanClient;
            aClient->mClientId = anId;
            aClient->mName = GUIString(new GUIStringData(aName));
            this->mClients->AddClient(aClient.get());
            if (anId == aMyId)
                this->mMyClient = aClient;
            else if (anId == aCaptainId) {
                this->mServerSocket->SetClient(aClient.get());
                aClient->SetCaptain(true, true);
            }
        }
        return 0;
    } catch (ReadBufferException&) {
    }
    return 1;
}
// FUNCTION: WON 0x007e1210
// Host: handles the first message of a socket that has no player yet (only join requests)
void LanLogic::HandleNewSocketMessage(const ByteBuffer* inBuffer, LobbyLanSocket* inSocket)
{
    if (inBuffer == NULL || inSocket == NULL || !this->IsCaptain())
        return;
    ReadBuffer aBuffer(inBuffer->data(), inBuffer->length());
    try {
        switch (aBuffer.ReadByte()) {
        case 1: this->HandleJoinRequest(aBuffer, inSocket); break;
        }
    } catch (ReadBufferException&) {
    }
}

// FUNCTION: WON 0x007e12a0
// Handles a received LAN message: stamps the socket's activity (host) and dispatches it
void LanLogic::HandleRecvMsg(AsyncOp* inOp)
{
    if (inOp->mStatus != WS_Success || inOp->mKilled)
        return;
    RecvMsgOp* anOp = static_cast<RecvMsgOp*>(inOp);
    LobbyLanSocket* aSocket = static_cast<LobbyLanSocket*>(anOp->mSocket.get());
    if (this->IsCaptain()) {
        aSocket->mLastMessageTime = time(NULL);
        if (aSocket->mClient.get() == NULL) {
            this->HandleNewSocketMessage(anOp->GetMsg().get(), aSocket);
            return;
        }
    }
    this->HandleMessage(anOp->GetMsg().get(), aSocket->mClient.get());
}
// FUNCTION: WON 0x007e13b0
// Host: starts receiving messages on an accepted socket and keeps it in the socket set
void LanLogic::AddPendingSocket(LobbyLanSocket* inSocket)
{
    inSocket->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed, this));
    inSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg, this));
    inSocket->SetRepeatOp(new RecvMsgOp);
    inSocket->mLastMessageTime = time(NULL);
    this->mPendingSockets.insert(inSocket);
}
// FUNCTION: WON 0x007e1840
// Host: sends a message to one player, handling it directly when that is the host itself
void LanLogic::SendMessageToClient(LobbyClient* inClient, const ByteBuffer* inBuffer)
{
    if (!this->IsCaptain() || !this->IsConnected())
        return;
    if (inClient == this->mMyClient.get()) {
        this->HandleMessage(inBuffer, this->mMyClient.get());
        return;
    }
    LobbyLanClient* aClient = static_cast<LobbyLanClient*>(inClient);
    if (aClient->mSocket == NULL)
        return;
    aClient->mSocket->QueueOp(new SendMsgOp(inBuffer), -1);
}

// FUNCTION: WON 0x007e1980
// Sends a message to the host, or handles it directly when this player is the host
void LanLogic::SendMessageToCaptain(const ByteBuffer* inBuffer)
{
    if (!this->IsConnected())
        return;
    if (this->IsCaptain()) {
        this->HandleMessage(inBuffer, this->mMyClient.get());
        return;
    }
    if (this->mServerSocket.get() != NULL) {
        this->mLastServerPing = time(NULL);
        this->mServerSocket->QueueOp(new SendMsgOp(inBuffer), -1);
    }
}

// FUNCTION: WON 0x007e1ad0
// Host: sends a message to every joined player, optionally handling it locally too
void LanLogic::BroadcastMessage(const ByteBuffer* inBuffer, bool inDeliverLocal)
{
    if (!this->IsCaptain())
        return;
    if (inDeliverLocal)
        this->HandleMessage(inBuffer, this->mMyClient.get());
    for (std::set<SmartPtr<LobbyLanSocket> >::iterator anItr = this->mPendingSockets.begin();
         anItr != this->mPendingSockets.end(); ++anItr) {
        LobbyLanSocket* aSocket = anItr->get();
        if (aSocket->mClient.get() != NULL)
            aSocket->QueueOp(new SendMsgOp(inBuffer), -1);
    }
}

// AppendBuffer's second argument is the byte size of the length prefix (0: none), not a bool
// FUNCTION: WON 0x007e1c30
// Wraps a staging message and sends it to the host
void LanLogic::SendToCaptain(const ByteBuffer* inBuffer)
{
    WriteBuffer aMessage(2, true);
    aMessage.AppendByte(8);
    aMessage.AppendBuffer(inBuffer, 0);
    this->SendMessageToCaptain(aMessage.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007e1cf0
// Wraps a staging message and sends it to every other player (host only)
void LanLogic::BroadcastToGame(const ByteBuffer* inBuffer)
{
    WriteBuffer aMessage(2, true);
    aMessage.AppendByte(8);
    aMessage.AppendBuffer(inBuffer, 0);
    this->BroadcastMessage(aMessage.ToByteBuffer(true).get(), false);
}

// FUNCTION: WON 0x007e1db0
// Wraps a staging message and sends it to one player (host only)
void LanLogic::SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer)
{
    WriteBuffer aMessage(2, true);
    aMessage.AppendByte(8);
    aMessage.AppendBuffer(inBuffer, 0);
    this->SendMessageToClient(inClient, aMessage.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007e1e70
// Adds the announced player (the host already has it) and broadcasts a client-added event
void LanLogic::HandleClientJoined(ReadBuffer& inBuffer)
{
    unsigned short anId = inBuffer.ReadShort();
    std::wstring aName;
    inBuffer.ReadWString(aName, 2);
    SmartPtr<LobbyClient> aClient;
    if (this->IsCaptain())
        aClient = this->mClients->FindClient(anId);
    else {
        SmartPtr<LobbyLanClient> aNewClient = new LobbyLanClient;
        aNewClient->mClientId = anId;
        aNewClient->mName = GUIString(new GUIStringData(aName));
        aClient = aNewClient.get();
        this->mClients->AddClient(aClient.get());
    }
    if (aClient.get() != NULL) {
        LobbyBroadcastEvent(new ClientChangedEvent(aClient.get(), this->GetRoomSpec(), 0), true);
    }
}

// FUNCTION: WON 0x007e2080
// Removes the announced player and tells the lobby the player left the game
void LanLogic::HandleClientLeft(ReadBuffer& inBuffer)
{
    unsigned short aClientId = inBuffer.ReadShort();
    SmartPtr<LobbyClient> aClient = this->mClients->RemoveClient(aClientId);
    if (aClient.get() != NULL) {
        LobbyBroadcastEvent(new ClientChangedEvent(aClient.get(), this->GetRoomSpec(), 1), true);
        LobbyClientLeftGame(aClient.get());
    }
}

// FUNCTION: WON 0x007e2160
// Host: relays a chat line to its whisper target and sender, to the sender's team, or to everyone
void LanLogic::HandleChatRequest(ReadBuffer& inBuffer, LobbyClient* inSender)
{
    if (!this->IsCaptain())
        return;
    unsigned short aFlags = inBuffer.ReadShort();
    std::wstring aText;
    inBuffer.ReadWString(aText, 2);
    unsigned short aTargetId = inBuffer.ReadShort();
    WriteBuffer aBuffer(2, true);
    aBuffer.AppendByte(6);
    aBuffer.AppendShort(aFlags);
    aBuffer.AppendShort(inSender->GetClientId());
    aBuffer.AppendWString(aText, 2);
    aBuffer.AppendShort(aTargetId);
    ByteBufferPtr aMessage = aBuffer.ToByteBuffer(true);
    if (aFlags & 4) {
        LobbyClient* aTarget = this->mClients->FindClient(aTargetId);
        if (aTarget == NULL)
            return;
        this->SendMessageToClient(aTarget, aMessage.get());
        if (inSender != aTarget)
            this->SendMessageToClient(inSender, aMessage.get());
    }
    else if (aFlags & 0x100) {
        LobbyClientIterator anItr(this->mClients.get());
        while (anItr.HasMore()) {
            LobbyClient* aClient = anItr.Next();
            if (aClient->IsTeammate(inSender))
                this->SendMessageToClient(aClient, aMessage.get());
        }
    }
    else
        this->BroadcastMessage(aMessage.get(), true);
}
// FUNCTION: WON 0x007e23a0
// Shows a relayed chat line unless the sender is ignored; whispers play a sound and set the reply name
void LanLogic::HandleChatMessage(ReadBuffer& inBuffer)
{
    unsigned short aFlags = inBuffer.ReadShort();
    unsigned short aSenderId = inBuffer.ReadShort();
    std::wstring aText;
    inBuffer.ReadWString(aText, 2);
    unsigned short aTargetId = inBuffer.ReadShort();
    LobbyClient* aSender = this->mClients->FindClient(aSenderId);
    if (aSender == NULL)
        return;
    if (aSender->mIgnored)
        return;
    LobbyClient* aTarget = NULL;
    if (aFlags & 4) {
        aFlags = 4;
        aTarget = this->mClients->FindClient(aTargetId);
        if (aTarget == NULL)
            return;
        if (aSender != this->mMyClient.get()) {
            if (LobbyGetChatSoundEffects())
                LobbyPlaySound(::LobbyGlobal_WhisperRecv_Sound.get());
            ChatCommandParser* aParser = LobbyGetChatCommandParser();
            if (aParser != NULL)
                aParser->mInput = aSender->mName;
        }
    }
    LobbyBroadcastEvent(new RecvChatEvent(8, aFlags, aSender,
        aText, aTarget), true);
}

// FUNCTION: WON 0x007e25f0
// Passes a staging message to the game room
void LanLogic::HandleStagingMessage(ReadBuffer& inBuffer, LobbyClient* inSender)
{
    LobbyHandleStagingReadBuffer(inBuffer, inSender);
}
// FUNCTION: WON 0x007e2610
// Dispatches a LAN message by type: player joined/left, chat request, chat line, staging
void LanLogic::HandleMessage(const ByteBuffer* inBuffer, LobbyClient* inClient)
{
    if (inBuffer == NULL || inClient == NULL)
        return;
    unsigned long anOffset = 0;
    if (this->IsCaptain() && inClient == this->mMyClient.get())
        anOffset = 2;
    ReadBuffer aBuffer(inBuffer->data() + anOffset, inBuffer->length() - anOffset);
    try {
        switch (aBuffer.ReadByte()) {
        case 3: this->HandleClientJoined(aBuffer); break;
        case 4: this->HandleClientLeft(aBuffer); break;
        case 5: this->HandleChatRequest(aBuffer, inClient); break;
        case 6: this->HandleChatMessage(aBuffer); break;
        case 8: this->HandleStagingMessage(aBuffer, inClient); break;
        }
    }
    catch (ReadBufferException&) {}
}
// FUNCTION: WON 0x007e2760
// Handles the LAN name, LAN port, refresh game list and show LAN screen commands
void LanLogic::HandleControlEvent(int inControlId)
{
    switch (inControlId) {
    case ID_AddLANName: this->ChangeLanName(); return;
    case ID_ChangeLANBroadcast: this->ChangeLanPort(); return;
    case ID_RefreshGameList: this->RefreshGames(); return;
    case ID_ShowLanScreen: this->EnterLanScreen(); return;
    }
}

// FUNCTION: WON 0x007e27c0
// Handles client actions, direct-connect list changes and chat commands
void LanLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    switch (inEvent->mType) {
    case LobbyEvent_ClientAction: this->HandleClientAction(static_cast<ClientActionEvent*>(inEvent)); break;
    case LobbyEvent_ChangeDirectConnect: this->HandleChangeDirectConnect(static_cast<ChangeDirectConnectEvent*>(inEvent)); break;
    case 0x186ac: this->HandleChatCommand(inEvent->mComponent); break;
    }
}

// FUNCTION: WON 0x007e2800
// Asks for a new LAN player name and saves it when confirmed and not empty
void LanLogic::ChangeLanName()
{
    this->mLanNameDialog->Reset();
    if (LobbyDoDialog(this->mLanNameDialog.get()) == 0x186a1 && this->mLanNameDialog->GetName().length() != 0) {
        LobbySetLanName(this->mLanNameDialog->GetName());
        LobbyBroadcastLobbyEvent(0x186d2, true);
    }
}

// FUNCTION: WON 0x007e2850
// Asks for a new LAN broadcast port; on change saves it and restarts discovery
void LanLogic::ChangeLanPort()
{
    int aPort = LobbyGetLanBroadcastPort();
    this->mLanPortDialog->SetGamePort(LobbyGetLobbyPort());
    this->mLanPortDialog->SetPort(aPort);
    this->mLanPortDialog->Reset();
    if (LobbyDoDialog(this->mLanPortDialog.get()) != 0x186a1)
        return;
    int aNewPort = this->mLanPortDialog->GetPort();
    if (aNewPort == aPort)
        return;
    LobbySetLanBroadcastPort(aNewPort);
    LobbyBroadcastLobbyEvent(0x186d2, true);
    this->StartDiscovery();
}
// FUNCTION: WON 0x007e28d0
// Removes a saved direct-connect game, or asks for an address (resolving DNS) and adds or renames one
void LanLogic::HandleChangeDirectConnect(ChangeDirectConnectEvent* inEvent)
{
    if (inEvent->IsDelete()) {
        if (inEvent->mGame.get() != NULL) {
            LobbyRemoveDirectConnect(inEvent->mGame->mAddress);
            this->mDirectGames->RemoveGame(inEvent->mGame->mAddress);
            LobbyBroadcastEvent(new GameChangedEvent(inEvent->mGame.get(), 1), true);
        }
    }
    else if (inEvent->IsAdd()) {
        this->mDirectConnectDialog->Reset();
        if (LobbyDoDialog(this->mDirectConnectDialog.get()) == 0x186a1) {
        IPAddr anAddress;
        LobbyConfig* aConfig = GetLobbyConfig();
        int aDefaultPort = LobbyGetLobbyPort();
        if (aConfig != NULL)
            aDefaultPort = aConfig->mLobbyPort;
        anAddress.SetWithDefaultPortNoDNS(this->mDirectConnectDialog->GetAddress(), aDefaultPort);
        if (!anAddress.IsValid()) {
            SmartPtr<DNSTestOp> anOp = new DNSTestOp(this->mDirectConnectDialog->GetAddress());
            LobbyRunOpWithStatus(anOp.get(), ::LANLogic_CheckingAddressTitle_String, ::LANLogic_CheckingAddressDetails_String, 1, 0);
            if (!anOp->Succeeded())
                return;
            anAddress = anOp->GetHostAddr();
            if (anAddress.GetPort() == 0)
                anAddress.SetPort(aDefaultPort);
        }
        GUIString aName = this->mDirectConnectDialog->GetName();
        if (aName.length() == 0)
            aName = this->mDirectConnectDialog->GetAddress();
        bool aFlag = false;
        LobbyGame* aGame = this->mDirectGames->GetOrCreateGame(anAddress, &aFlag);
        aGame->mName = aName;
        LobbyAddDirectConnect(anAddress, aName);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, aFlag ? 2 : 0), true);
        }
    }
}
// FUNCTION: WON 0x007e2ce0
// Opens the options popup for a player
void LanLogic::ShowClientOptions(LobbyClient* inClient, int inRoomSpec)
{
    if (this->mMyClient.get() == NULL || inClient == NULL)
        return;
    SmartPtr<ClientOptionsPopup> aPopup = new ClientOptionsPopup;
    aPopup->Init(inClient, this->mMyClient.get(), inRoomSpec);
    LobbyOpenPopup(aPopup.get(), PopupFlag_StandardPopup, -1, -1);
}

// FUNCTION: WON 0x007e2dc0
// Handles a player action in the LAN room: options popup, ignore, unignore, whisper or kick
void LanLogic::HandleClientAction(ClientActionEvent* inEvent)
{
    if (!this->IsConnected() || !((inEvent->mRoomSpec & 8) ? true : false))
        return;
    LobbyClient* aClient = inEvent->mClient.get();
    if (aClient == NULL)
        return;
    switch (inEvent->mAction) {
    case 18:
        this->ShowClientOptions(aClient, inEvent->mRoomSpec);
        return;
    case 5:
        LobbySetClientIgnored(aClient, true, 8);
        return;
    case 12:
        LobbySetClientIgnored(aClient, false, 8);
        return;
    case 16:
        LobbyBroadcastEvent(new SetChatInputEvent(2, aClient->mName,
            inEvent->mRoomSpec, false), true);
        return;
    case 7:
        LobbyKickClient(aClient, false);
        return;
    }
}

// FUNCTION: WON 0x007e2f40
// Sends a chat request (text, flags, whisper target) to the host
void LanLogic::SendChat(const GUIString& inText, unsigned short inFlags,
                       unsigned short inTarget)
{
    WriteBuffer aBuffer(2, true);
    aBuffer.AppendByte(5);
    aBuffer.AppendShort(inFlags);
    aBuffer.AppendWString(inText, 2);
    aBuffer.AppendShort(inTarget);
    this->SendMessageToCaptain(aBuffer.ToByteBuffer(true).get());
}

// FUNCTION: WON 0x007e3050
// Sends a typed LAN chat line, emote (team-only if flagged) or whisper, or toggles ignore; else reports an error
void LanLogic::HandleChatCommand(Component* inTarget)
{
    if (!this->IsConnected())
        return;
    ChatCommandParser* aParser = LobbyGetChatCommandParser();
    if (aParser == NULL || !(aParser->mRoomFlags & 8))
        return;
    LobbyClient* aClient = aParser->mTarget.get();
    switch (aParser->mCommand) {
    case LobbyChatCommand_Broadcast: case LobbyChatCommand_Emote: {
        unsigned short aFlags = aParser->mCommand == LobbyChatCommand_Emote ? 2 : 0;
        if (aParser->mRoomFlags & 0x10)
            aFlags |= 0x100;
        this->SendChat(aParser->mText, aFlags, 0);
        return;
    }
    case LobbyChatCommand_Whisper:
        if (aClient != NULL)
            this->SendChat(aParser->mText, 4, aClient->mClientId);
        return;
    case LobbyChatCommand_Ignore:
        if (aClient != NULL)
            LobbySetClientIgnored(aClient, !aClient->mIgnored, 4);
        return;
    default:
        LobbySendEventTo(new ChatCommandErrorEvent(::RoutingLogic_InvalidCommand_String), inTarget);
        break;
    }
}
// FUNCTION: WON 0x007e3290
// LAN screen: clears the games and broadcasts a discovery query; direct connect: pings the shown games
void LanLogic::RefreshGames()
{
    int aKind = LobbyGetGameScreenType();
    if (aKind == 1) {
        this->mLanGames->Clear();
        LobbyBroadcastEvent(new SyncGameListEvent(this->mLanGames.get()), true);
        if (this->mDiscoverySocket.get() == NULL)
            return;
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendByte(4);
        this->mDiscoverySocket->QueueOp(new SendBytesToOp(
            aBuffer.ToByteBuffer(true).get(), this->mDiscoveryAddress), -1);
    } else if (aKind == 3) {
        this->mDirectGames->Rewind();
        while (this->mDirectGames->HasMore()) {
            LobbyGame* aGame = this->mDirectGames->Next();
            if (!aGame->GetIsFiltered())
                LobbyPingGame(aGame, false);
        }
        LobbyBroadcastLobbyEvent(0x186da, true);
    }
}
// FUNCTION: WON 0x007e3480
// Enters the LAN screen: loads the saved direct-connect games and starts LAN discovery
void LanLogic::EnterLanScreen()
{
    LobbySetCurrentUser(NULL);
    this->mLanGames->Clear();
    this->mDirectGames->Clear();
    const LobbyDirectConnectMap& aSavedGames = LobbyFn35D8E0();
    LobbyDirectConnectMap::const_iterator anItr;
    for (anItr = aSavedGames.begin(); anItr != aSavedGames.end(); ++anItr) {
        LobbyGame* aGame = this->mDirectGames->GetOrCreateGame(anItr->first, NULL);
        aGame->mName = anItr->second;
    }
    LobbyBroadcastEvent(new SyncGameListEvent(this->mDirectGames.get()), true);
    this->StartDiscovery();
    LobbyShowLanScreen();
}
// FUNCTION: WON 0x007e37a0
// Hosts a LAN game: certificate check for direct connect, listens on the lobby port, joins as captain
bool LanLogic::HostGame(LobbyGame* inGame)
{
    this->CloseConnections();
    if (inGame->GetGameType() == 3 && LobbyDoCDKeyCheck()) {
        this->mRequirePeerAuth = true;
        if (!this->RefreshCertificate())
            return false;
    }
    LobbyKillFirewallTest();
    this->mIsCaptain = true;
    this->mMyClient = new LobbyLanClient;
    this->mMyClient->mClientId = this->GetNextClientId();
    this->mMyClient->mName = LobbyGetLanName();
    this->mMyClient->SetCaptain(true, true);
    this->mClients->AddClient(this->mMyClient.get());
    inGame->SetClients(this->mClients.get());
    this->mListenSocket = new LobbyLanSocket;
    WONStatus aStatus = this->mListenSocket->Fn2B0D90(LobbyGetLobbyPort(), 0);
    if (aStatus != 0) {
        LobbyDoErrorDialog(::LANLogic_SocketBindError_String, aStatus);
        return false;
    }
    aStatus = this->mListenSocket->Listen(1000);
    if (aStatus != 0) {
        LobbyDoErrorDialog(::LANLogic_SocketListenError_String, aStatus);
        return false;
    }
    this->mListenSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleAccept, this));
    this->mListenSocket->SetRepeatOp(new AcceptOp(NULL));
    LobbyBroadcastEvent(new JoinedRoomEvent("",
        inGame->mName, 10, this->mMyClient->GetClientId(), inGame->mClients.get()), true);
    this->mLastBroadcastTime = 0;
    this->StartNetworkTimer();
    return true;
}
// FUNCTION: WON 0x007e4590
// Host: closes the player's socket
void LanLogic::KickClient(LobbyClient* inClient, bool)
{
    if (this->IsCaptain())
        static_cast<LobbyLanClient*>(inClient)->mSocket->QueueClose(1000);
}

// FUNCTION: WON 0x007e45b0
// Leaves the game by closing every connection
void LanLogic::LeaveGame(LobbyGame*) { this->CloseConnections(); }

// FUNCTION: WON 0x007e45c0
// Broadcasts a LAN game's summary on the discovery port
void LanLogic::AdvertiseGame(LobbyGame* inGame)
{
    if (inGame->GetGameType() == 1)
        this->BroadcastGameSummary(inGame, this->mDiscoveryAddress);
}

// FUNCTION: WON 0x007e45e0
// Broadcasts that a LAN game is gone
void LanLogic::RemoveGameAdvertisement(LobbyGame* inGame)
{
    if (inGame->GetGameType() == 1)
        this->BroadcastGameRemoved(inGame);
}


void LanLogic::SetGameInProgress(bool) {}
void LanLogic::Suspend() {}

// FUNCTION: WON 0x007e4600
// Returns true when this player hosts a LAN game and discovery is running
bool LanLogic::CanBroadcastGame(LobbyGame* inGame)
{
    if (!this->IsCaptain() || this->mDiscoverySocket.get() == NULL || inGame == NULL)
        return false;
    return inGame->GetGameType() == 1 ? true : false;
}

// FUNCTION: WON 0x007e4630
// Sends the hosted game's port and summary to an address
void LanLogic::BroadcastGameSummary(LobbyGame* inGame, const IPAddr& inAddress)
{
    if (!this->CanBroadcastGame(inGame))
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(1);
    aBuffer.AppendShort(inGame->mAddress.GetPort());
    inGame->WriteSummary(aBuffer);
    this->mDiscoverySocket->QueueOp(new SendBytesToOp(aBuffer.ToByteBuffer(true).get(), inAddress), -1);
}

// FUNCTION: WON 0x007e4800
// Broadcasts a keep-alive for the hosted game, or its full summary every 30 seconds
void LanLogic::BroadcastGameKeepAlive(LobbyGame* inGame)
{
    if (!this->CanBroadcastGame(inGame))
        return;
    long aTime = time(NULL);
    if (this->mLastBroadcastTime > 0 && aTime - this->mLastBroadcastTime >= 30)
        this->BroadcastGameSummary(inGame, this->mDiscoveryAddress);
    else {
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendByte(2);
        aBuffer.AppendShort(inGame->mAddress.GetPort());
        this->mDiscoverySocket->QueueOp(new SendBytesToOp(aBuffer.ToByteBuffer(true).get(), this->mDiscoveryAddress), -1);
    }
    this->mLastBroadcastTime = aTime;
}

// FUNCTION: WON 0x007e4a10
// Broadcasts that the hosted game is gone
void LanLogic::BroadcastGameRemoved(LobbyGame* inGame)
{
    if (!this->CanBroadcastGame(inGame))
        return;
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(3);
    aBuffer.AppendShort(inGame->mAddress.GetPort());
    this->mDiscoverySocket->QueueOp(new SendBytesToOp(aBuffer.ToByteBuffer(true).get(), this->mDiscoveryAddress), -1);
}
// FUNCTION: WON 0x007e4be0
// Sends the host's keep-alive, drops LAN games unseen for 30 seconds, then rearms
void LanLogic::HandleDiscoveryTimer(AsyncOp* inOp)
{
    if (inOp != this->mDiscoveryTimer.get())
        return;
    if (this->IsCaptain()) {
        LobbyGame* aGame = GetMyLobbyGame();
        if (aGame != NULL)
            this->BroadcastGameKeepAlive(aGame);
    }
    if (this->mLanGames->RemoveStaleGames(30))
        LobbyBroadcastEvent(new SyncGameListEvent(this->mLanGames.get()), true);
    this->StartDiscoveryTimer();
}

// FUNCTION: WON 0x007e4c90
// Answers a discovery query with the hosted game's summary
void LanLogic::HandleDiscoveryQuery(ReadBuffer&, const IPAddr& inAddress)
{
    LobbyGame* aGame = GetMyLobbyGame();
    if (aGame != NULL)
        this->BroadcastGameSummary(aGame, inAddress);
}

// FUNCTION: WON 0x007e4cb0
// Adds or updates a LAN game from its summary; an unreadable one is removed but reported as modified
void LanLogic::HandleGameSummary(ReadBuffer& inBuffer, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inBuffer.ReadShort());
    bool aFlag = false;
    LobbyGame* aGame = this->mLanGames->GetOrCreateGame(anAddress, &aFlag);
    if (aGame->ReadSummary(inBuffer)) {
        aGame->mLastSeenTime = time(NULL);
        LobbyBroadcastEvent(new GameChangedEvent(aGame, aFlag ? 2 : 0), true);
    }
    else {
        SmartPtr<LobbyGame> aHeldGame = aGame;
        this->mLanGames->RemoveGame(anAddress);
        if (aFlag)
            LobbyBroadcastEvent(new GameChangedEvent(aGame, aFlag ? 2 : 1), true);
    }
}

// FUNCTION: WON 0x007e4ec0
// Marks a known LAN game as just seen
void LanLogic::HandleGameKeepAlive(ReadBuffer& inBuffer, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inBuffer.ReadShort());
    long anIP = inAddress.GetHost();
    if (anAddress.GetHost() != anIP)
        return;
    LobbyGame* aGame = this->mLanGames->GetGame(anAddress);
    if (aGame == NULL)
        return;
    aGame->mLastSeenTime = time(NULL);
}

// FUNCTION: WON 0x007e5050
// Removes a LAN game its host announced as gone
void LanLogic::HandleGameRemoved(ReadBuffer& inBuffer, const IPAddr& inAddress)
{
    IPAddr anAddress = inAddress;
    anAddress.SetPort(inBuffer.ReadShort());
    long aBroadcast = inAddress.GetHost();
    if (anAddress.GetHost() != aBroadcast)
        return;
    SmartPtr<LobbyGame> aGame = this->mLanGames->RemoveGame(anAddress);
    if (aGame.get() != NULL)
        LobbyBroadcastEvent(new GameChangedEvent(aGame.get(), 1), true);
}
// FUNCTION: WON 0x007e5200
// Dispatches a discovery packet: game summary, keep-alive, removal or query
void LanLogic::HandleDiscoveryRecv(AsyncOp* inOp)
{
    RecvBytesFromOp* anOp = static_cast<RecvBytesFromOp*>(inOp);
    if (anOp->mSocket.get() != this->mDiscoverySocket.get() || !anOp->Succeeded())
        return;
    ReadBuffer aBuffer(anOp->GetBytes()->data(), anOp->GetBytes()->length());
    const IPAddr& aFrom = anOp->mFromAddress;
    try {
        switch (aBuffer.ReadByte()) {
        case 1: this->HandleGameSummary(aBuffer, aFrom); break;
        case 2: this->HandleGameKeepAlive(aBuffer, aFrom); break;
        case 3: this->HandleGameRemoved(aBuffer, aFrom); break;
        case 4: this->HandleDiscoveryQuery(aBuffer, aFrom); break;
        }
    }
    catch (ReadBufferException&) {}
}
// FUNCTION: WON 0x007e53a0
// Starts (creating if needed) the 15-second discovery timer
void LanLogic::StartDiscoveryTimer()
{
    if (this->mDiscoveryTimer.get() == NULL) {
        this->mDiscoveryTimer = new AsyncOp;
        this->mDiscoveryTimer->mCompletion = new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryTimer, this);
    }
    this->mDiscoveryTimer->RunAsTimer(15000);
}

// FUNCTION: WON 0x007e54e0
// Opens the broadcast discovery socket, queries for LAN games and advertises the hosted game
void LanLogic::StartDiscovery()
{
    this->StopDiscovery();
    this->mDiscoveryAddress = IPAddr::GetBroadcastAddr(LobbyGetLanBroadcastPort());
    this->mDiscoverySocket = new QueueSocket(1);
    long aResult = this->mDiscoverySocket->Fn2B0D90(LobbyGetLanBroadcastPort(), 0);
    if (aResult != 0) {
        this->StopDiscovery();
        return;
    }
    this->mDiscoverySocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleDiscoveryRecv, this));
    this->mDiscoverySocket->SetRepeatOp(new RecvBytesFromOp(1024, NULL));
    this->StartDiscoveryTimer();
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(4);
    this->mDiscoverySocket->QueueOp(new SendBytesToOp(
        aBuffer.ToByteBuffer(true).get(), this->mDiscoveryAddress), -1);
    if (!this->IsCaptain())
        return;
    LobbyGame* aGame = GetMyLobbyGame();
    if (aGame != NULL)
        this->BroadcastGameSummary(aGame, this->mDiscoveryAddress);
}
// FUNCTION: WON 0x007e5810
// Clears the LAN games and closes the discovery timer and socket
void LanLogic::StopDiscovery()
{
    if (!this->mLanGames->IsEmpty()) {
        this->mLanGames->Clear();
        LobbyBroadcastEvent(new SyncGameListEvent(this->mLanGames.get()), true);
    }
    if (this->mDiscoveryTimer.get() != NULL) {
        this->mDiscoveryTimer->Kill(true);
        this->mDiscoveryTimer = NULL;
    }
    if (this->mDiscoverySocket.get() != NULL) {
        this->mDiscoverySocket->Kill();
        this->mDiscoverySocket = NULL;
    }
}
// FUNCTION: WON 0x007e5900
// Leaves the current game when the lobby resumes
void LanLogic::Resume() { LobbyLeaveGame(0); }

// FUNCTION: WON 0x007e5910
// Stops LAN discovery and clears the direct-connect games
void LanLogic::Logout()
{
    this->StopDiscovery();
    this->mDirectGames->Clear();
}
}

#include "ee/wonapi/EELobbyGameCode.h" // LobbyMessageBox
extern unsigned short gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in EEWonNet.cpp)
namespace WONAPI {
// FUNCTION: WON 0x007e1600
// Host: accepts a peer-authenticated socket whose certificate has the lobby's community, else closes it
void LanLogic::HandlePeerAuthDone(AsyncOp* inOp)
{
    PeerAuthServerOp* op=static_cast<PeerAuthServerOp*>(inOp);
    LobbyLanSocket* socket=static_cast<LobbyLanSocket*>(op->GetSocket());
    if(!op->Succeeded()) {
        socket->QueueClose(1000);
        return;
    }
    const AuthCertificateBase* certificate=op->mPeerAuthServer.GetClientCertificate();
    AuthContext* auth=GetLobbyAuthContext();
    if(!auth) return;
    DWORD community=auth->GetPeerData()->GetCertificate2()->GetFirstCommunity();
    if(!certificate->VerifyPermissions(0,community,0)) {
        socket->QueueClose(1000);
        return;
    }
    this->AddPendingSocket(socket);
}
// FUNCTION: WON 0x007e1700
// Host: takes an accepted connection, running peer authentication first when required
void LanLogic::HandleAccept(AsyncOp* inOp)
{
    if(inOp->GetStatus()!=WS_Success || inOp->mKilled) return;
    LobbyLanSocket* socket=static_cast<LobbyLanSocket*>(static_cast<AcceptOp*>(inOp)->mAcceptedSocket.get());
    if(this->mRequirePeerAuth) {
        AuthContext* auth=GetLobbyAuthContext();
        if(!auth) return;
        SmartPtr<PeerAuthServerOp> server=new PeerAuthServerOp(auth,socket);
        this->RunTrackedOp(server.get(),new LogicCompletion<LanLogic>(&LanLogic::HandlePeerAuthDone,this),30000);
    } else
        this->AddPendingSocket(socket);
}
// FUNCTION: WON 0x007e35a0
// Gets a fresh login certificate (anonymous) unless the current one is valid; false on bad CD key or failure
bool LanLogic::RefreshCertificate()
{
    AuthContext* auth=GetLobbyAuthContext();
    if(!auth) return false;
    if(!LobbyIsCDKeyValid()) {
        LobbyMessageBox(::AccountLogic_InvalidCDKeyTitle_String,::AccountLogic_InvalidCDKeyDetails_String);
        return false;
    }
    if(!auth->IsExpired(120)) {
        auth->SetDoAutoRefresh(true);
        return true;
    }
    auth->SetUserName(std::wstring(gEmptyWideString));
    auth->SetPassword(std::wstring(gEmptyWideString));
    SmartPtr<GetCertOp> op=new GetCertOp(auth);
    LobbyRunOpWithStatus(op.get(),::LobbyContainer_LoginTitle_String,::LobbyContainer_LoginDetails_String,1,0);
    if(op->GetStatus()==WS_Success) {
        auth->SetDoAutoRefresh(true);
        return true;
    }
    return false;
}
// FUNCTION: WON 0x007e3d20
// Joins a LAN or direct-connect game: connects (peer auth if needed), asks again on a wrong password
bool LanLogic::JoinGame(LobbyGame* game)
{
    this->CloseConnections();
    SmartPtr<AuthContext> auth;
    if(game->GetGameType()==3 && LobbyDoCDKeyCheck()) {
        this->mRequirePeerAuth=true;
        if(!this->RefreshCertificate()) return false;
        auth=GetLobbyAuthContext();
    }
    this->mServerSocket=new LobbyLanSocket;
    bool joined=false;
    bool needPassword=game->mHasPassword;
    bool needConnect=true;
    while(!joined) {
        GUIString password(GetEmptyGUIStringData());
        if(needPassword) {
            this->mPasswordDialog->Reset();
            if(LobbyDoDialog(this->mPasswordDialog.get())!=100001) return false;
            password=this->mPasswordDialog->GetPassword();
        }
        if(needConnect) {
            SmartPtr<AsyncOp> connect;
            if(this->mRequirePeerAuth) connect=new PeerAuthOp(game->mAddress,auth.get(),AUTH_TYPE_PERSISTENT,this->mServerSocket.get(),true);
            else connect=new ConnectOp(game->mAddress,this->mServerSocket.get());
            LobbyRunOpWithStatus(connect.get(),::LANLogic_ConnectToGameTitle_String,::LANLogic_ConnectToGameDetails_String,1,0);
            if(connect->GetStatus()!=WS_Success) return false;
            needConnect=false;
        }
        WriteBuffer request(2,true);
        request.AppendByte(1);
        request.AppendWString(LobbyGetLanName(),2);
        request.AppendWString(password,2);
        this->mServerSocket->QueueOp(new SendMsgOp(request.ToByteBuffer(true).get()),-1);
        this->mLastServerPing=time(NULL);
        SmartPtr<RecvMsgOp> reply=new RecvMsgOp(this->mServerSocket.get());
        LobbyRunOpWithStatus(reply.get(),::LANLogic_RegisterTitle_String,::LANLogic_RegisterDetails_String,1,0);
        if(reply->GetStatus()!=WS_Success) return false;
        int status=this->ReadJoinReply(reply->GetMsg().get(),game);
        switch(status) {
        case 0: joined=true;break;
        case 1: return false;
        case 2:
            LobbyMessageBox(::GameStagingLogic_JoinGameErrorTitle_String,::LobbyGameStatus_InvalidPassword_String);
            needPassword=true;
            break;
        case 3:
            LobbyMessageBox(::LANLogic_RegisterTitle_String,::LANLogic_InvalidSubnet_String);
            return false;
        }
    }
    this->mServerSocket->SetCloseCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleSocketClosed,this));
    this->mServerSocket->SetRepeatCompletion(new LogicCompletion<LanLogic>(&LanLogic::HandleRecvMsg,this));
    this->mServerSocket->SetRepeatOp(new RecvMsgOp);
    this->StartNetworkTimer();
    game->SetClients(this->mClients.get());
    LobbyBroadcastEvent(new JoinedRoomEvent(GUIString(new GUIStringData("")),game->mName,10,this->mMyClient->GetClientId(),game->mClients.get()),true);
    return true;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dfde0
// (public: virtual void * __thiscall WONAPI::LanLogic::`scalar deleting destructor'(unsigned int)    [??_GLanLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007dfe00
// (public: virtual __thiscall WONAPI::NetLogic::~NetLogic(void)    [??1NetLogic@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007e0e80
// (public: __thiscall WONAPI::SendBytesOp::SendBytesOp(class WONAPI::const ByteBuffer*,class WONAPI::AsyncSocket *)    [??0SendBytesOp@WONAPI@@QAE@PAVByteBuff)
// TEMPLATE: WON 0x007e0f80
// (public: virtual __thiscall WONAPI::LobbyLanClient::~LobbyLanClient(void)    [??1LobbyLanClient@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f20d0
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>,class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>,struct std::set<cla)
// TEMPLATE: WON 0x007f47e0
// (void __cdecl std::_Distance(class std::_Tree<class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>,class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>)
// TEMPLATE: WON 0x007f3850
// TEMPLATE: WON 0x007f39d0
// TEMPLATE: WON 0x007f3b50
// TEMPLATE: WON 0x007f3cd0

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e3af0
// (public: virtual class WONAPI::AsyncSocket * __thiscall WONAPI::LobbyLanSocket::Duplicate(void)    [?Duplicate@LobbyLanSocket@WONAPI@@UAEPAVAsyncSocket)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dfed0
// (public: virtual void * __thiscall WONAPI::NetLogic::`scalar deleting destructor'(unsigned int)    [??_GNetLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e0f60
// (public: virtual void * __thiscall WONAPI::LobbyLanClient::`scalar deleting destructor'(unsigned int)    [??_GLobbyLanClient@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e31e0
// (public: virtual void * __thiscall WONAPI::ChatCommandErrorEvent::`scalar deleting destructor'(unsigned int)    [??_GChatCommandErrorEvent@WONAPI@@UAEP)
// TEMPLATE: WON 0x007e3200
// (public: virtual __thiscall WONAPI::ChatCommandErrorEvent::~ChatCommandErrorEvent(void)    [??1ChatCommandErrorEvent@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007e3b80
// (public: virtual void * __thiscall WONAPI::LobbyLanSocket::`scalar deleting destructor'(unsigned int)    [??_GLobbyLanSocket@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e3ba0
// (public: virtual __thiscall WONAPI::LobbyLanSocket::~LobbyLanSocket(void)    [??1LobbyLanSocket@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1450
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::LanLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicCompletio)
// TEMPLATE: WON 0x007f1570
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::LanLogic>::~LogicCompletion<class WONAPI::LanLogic>(void)    [??1?$LogicCompletion@VL)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007dfe10
// (public: __thiscall std::set<class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>,struct std::less<class WONAPI::SmartPtr<class WONAPI::LobbyLanSocket>)
// TEMPLATE: WON 0x007f1750
// (std::_Tree::erase)
// TEMPLATE: WON 0x007f1800
// (std::_Tree::_Insert)
// TEMPLATE: WON 0x007f25c0
// (std::_Tree::_Erase)
// TEMPLATE: WON 0x007f2600
// (std::_Tree::_Init)
// TEMPLATE: WON 0x007f2640
// ( ?? ?? :: ?? ::const_iterator::_Dec)
