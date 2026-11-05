// C:\EEX\Empire Earth\Communications\EEServer.h (RECONSTRUCTION; file name inferred). The game's lock-step server
// (RTTI EEServer > NESynchronousServer > NESynchronousCommunications; vtable 0x43e384; ctor 0xca90f; sizeof 0x350).
// The lobby only dynamic_casts gCommunications to it.
// Requires lle/NESynchronousServer.h.
#ifndef EE_EESERVER_H
#define EE_EESERVER_H

#include <map>
#include <set>

class EEGameOptionsPlayer;

class EEServer : public NESynchronousServer {
public:
    // 0xca90f (argument names are provisional; the SP random map: (UWideString("EEName"), gU5308A0, 0, 0, true, true))
    EEServer(UWideString& inName, unsigned long inNetworkAdapter, unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick, bool inAllowNewConnections, bool inAnnounce);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    EEServer(const UWideString& inName, unsigned long inNetworkAdapter, unsigned long inCurrentCommunicationsTick,
             unsigned long inCurrentPhysicsTick, bool inAllowNewConnections, bool inAnnounce)
        : EEServer(const_cast<UWideString&>(inName), inNetworkAdapter, inCurrentCommunicationsTick, inCurrentPhysicsTick,
                   inAllowNewConnections, inAnnounce) {}
#endif
    virtual ~EEServer();                    // +0x00 0xcaa86 (??_G 0xcaa6a)
    // the engine's pure callbacks, implemented by the game (vtable 0x43e384; EEServer.cpp 0xca90f..0xcd4f9; names by
    // base slot, signatures from the bodies)
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting);   // +0x04 0xcb8f6
    virtual bool IsClient();                // +0x0c (ICF: 0xac9f3, false)
    virtual bool IsServer();                // +0x10 (ICF: 0x18aa9, true: the server)
    virtual void BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting);  // +0x24 0xcb405
    virtual bool ServerSlot28();            // +0x28 0xcbb41
    virtual bool IsMigrating();             // +0x2c 0xcbb65
    virtual void BeforeRemovingPlayers();   // +0x30 (ICF: the empty 0xe123d)
    virtual void HandleReconnectWaitTimedOut();  // +0x34 0xcc9f1
    virtual void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID,
                              unsigned char& inStatusID);   // +0x38 0xccf3a
    virtual void PlayWarningSound(long inSound);  // +0x44 0xcc2e7
    virtual void ServerSlot48();            // +0x48 0xcd3e8
    virtual unsigned char GetFreePlayerSlot();  // +0x4c 0xcbb17 (the first free player slot, 0xff if none)
    virtual bool GetDebugOOSHandling();     // +0x50 0xcd411
    virtual const UWideString& ServerSlot54(unsigned char inPlayer);   // +0x54 0xcbb6c (the player's name)
    virtual unsigned char GetPlayerSendToSlot(unsigned char inPlayer);  // +0x58 0xcbbd9 (the player's slot, 0xff if none)
    virtual bool FilterPlayerInput(NEMessage* inMessage);  // +0x5c 0xcbfd0
    virtual void PrepareLocalInput(NEMessage* inMessage);  // +0x60 0xcc1f9
    virtual bool IsPlayerPresent(unsigned char inPlayer);  // +0x64 0xcc2ca (player present)
    virtual void ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag);  // +0x68 0xccb66
    virtual void ProcessSystemMessage(NEMessage* inMessage);  // +0x6c 0xcbc77

    // non-virtual members (provisional names)
    // 0xcaad8: a client's connection request (FilterPlayerInput/60; the version and the binary checksum are checked)
    void ProcessConnectionRequest(unsigned char inSlot, const EEGameOptionsPlayer& inPlayer,
                                  const UWideString& inVersion, unsigned long inChecksum);
    void RefuseClient(EEGameOptionsPlayer& inPlayer);  // 0xcb850 (the lobby's opened/closed slot 0x1db1ce)
    unsigned char GetSendToSlot(unsigned char inPlayer);  // 0xcbc02 (mU214's value for inPlayer, 0xff if none)
    unsigned char GetLobbySlot(unsigned char inValue);  // 0xcbc35 (the key whose mU214 value is inValue, 0 if none)
    void HandleLobbyInput(NEMessage* inMessage);  // 0xcc34e (ProcessSystemMessage)
    bool StartVote(unsigned short inCount);  // 0xcc937 (FilterPlayerInput)
    // 0xcc9ce: clears the fast-selection state (the ctor's last call; EEClient has the same member)
    void ResetFastSelection();
    void SendPlayerSlotInfo();              // 0xcccc8 (EEMStartLoadingResponse: everybody has loaded)
    void CastVote(unsigned char inIndex, bool inYes, unsigned char inCount);  // 0xccfe7 (a vote on mU32C)
    void SendFullPlayerInformation(unsigned char inPlayer, EEGameOptionsPlayer& inOptions);  // 0xcd144 (RefuseClient)
    void SendPingStat(unsigned char inPlayer, float inPing);  // 0xcd1dc (ProcessSystemMessage: a ping statistic)

    // Layout from the ctor 0xca90f (the locks are named by its asserts). Other names are provisional.
    TSCriticalSection* mAttemptToRemovePlayerLock;   // +0x208
    TSCriticalSection* mSearchForSlotLock;  // +0x20c
    TSCriticalSection* mFastSelectionLock;  // +0x210 (EEMessage's selection executes hold it)
    std::map<unsigned char, unsigned char> mSendToSlots;  // +0x214 (GetSendToSlot reads the mapped byte +0xd; cleared by
                                            // ProcessConnectionRequest; provisional name)
    std::map<unsigned char, bool> mBehindFirewall;  // +0x220 (FilterPlayerInput: each client slot's NEMClientConnectionRequest
                                            //  firewall flag; its own _Tree COMDATs: a second map type)
    bool mVoted[256];                       // +0x22c (CastVote: who voted, by index; zeroed by one `rep stosd` of 0x40)
    unsigned short mVoteEdict;              // +0x32c
    unsigned short mYesVotes;               // +0x32e
    unsigned short mNoVotes;                // +0x330
    unsigned short mLagMask;                // +0x332
    unsigned char mNumVotes;                // +0x334 (CastVote: the votes so far)
    unsigned char mNumLoadResponses;        // +0x335 the lobby's Start clears it; the load responses so far (EEMStartLoadingResponse)
    unsigned char mLoadState;               // +0x336 the lobby's Start sets it; 2 when all have loaded, 0 at the start (EEMessage)
    unsigned char mNumDroppedLoaders;       // +0x337 (BootPlayer counts the dropped loaders)
    // the fast selection (ResetFastSelection clears +0x338..+0x34a; EEMessage 0xb8cf7/0xb8e86; provisional names)
    unsigned char mFastSelection[16];       // +0x338 the last selection id of each player (indexed by player id)
    unsigned char mU348;                    // +0x348
    unsigned char mFastSelectionID;         // +0x349 the local player's
    bool mIsExecutingSelection;             // +0x34a (some selection messages execute twice when set)
    bool mIsMigrating;                      // +0x34b
    bool mHasLeftGame;                      // +0x34c (sizeof 0x350: the SP random map's operator new)
};

// (FnB7992, the callback after the host left: ee/EEClient.h)
class NEProtocol;
// 0x296c89 (User Interface Thread; provisional name): the local address to publish (the protocol's, or WON's)
UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter);

// 0x4bb878 (.data; EEServer.cpp's; also read by the lobby): the default colour of each player slot
// {0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15} (provisional name)
extern long gDefaultPlayerColors[16];

#endif
