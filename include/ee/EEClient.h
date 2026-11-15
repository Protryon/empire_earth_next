// C:\EEX\Empire Earth\Communications\EEClient.h (RECONSTRUCTION; file name inferred). The game's lock-step client
// (RTTI EEClient > NESynchronousClient; vtable 0x43d054; ctor 0xb71e1; sizeof 0xfc: `new` 0xfc at 0x21ac). Layout from
// the ctor and 0xb8418; provisional names unless an assert names them.
// Requires lle/NESynchronousClient.h.
#ifndef EE_EECLIENT_H
#define EE_EECLIENT_H

class EEGameOptionsPlayer;

class EEClient : public NESynchronousClient {
public:
    EEClient();                             // 0xb71e1
    virtual ~EEClient();                    // +0x00 0xb7278 (??_G 0xb725c)
    // the engine's pure callbacks, implemented by the game (vtable 0x43d054; bodies in EEClient.cpp, names by base
    // slot; declared so that EEAJoinOrHostAGame.cpp can `new EEClient`)
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting);   // +0x04 0xb7722
    virtual bool IsClient();                // +0x0c 0x18aa9
    virtual bool IsServer();                // +0x10 (ICF: 0xac9f3)
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize);  // +0x24 0xb743b
    virtual bool IsPlaybackRecordingSession();   // +0x28 0xb799d
    virtual unsigned long GetConnectTimeout();   // +0x2c 0xb79a4
    virtual unsigned char GetClientSlot();  // +0x30 0xb79aa
    virtual bool IgnoreConnectionTimeout();  // +0x3c 0xb7300
    virtual void SetConnectionState(long inState);   // +0x40 0xb7307
    virtual void HandleMessage(NEMessage* inMessage);   // +0x44 0xb79b1
    virtual void HandleUserInput(NEMessage* inMessage);   // +0x48 0xb7ed4
    virtual void HandleConnectionLost();    // +0x4c 0xb7fa5
    virtual unsigned long HandleStartGame(unsigned long inParameter);   // +0x50 0xb843b
    virtual void HandleStopGame();          // +0x54 0xe123d
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining);  // +0x58 0xb8599
    // 0xb8418: clears the fast-selection state (as EEServer::ResetFastSelection 0xcc9ce)
    void ResetFastSelection();
    // 0xb729e: a lobby slot changed (the host's closed slot posts EEABootClientFromCanceledGame; provisional name)
    void HandleSlotUpdate(unsigned char inSlot, EEGameOptionsPlayer& inPlayer);
    void HandleLobbyInput(NEMessage* inMessage);  // 0xb80ef (HandleMessage's lobby messages; provisional name)

    TSCriticalSection* mFastSelectionLock;  // +0xdc (named by the ctor's assert)
    bool mHasLeftGame;                      // +0xe0 (the ctor clears it; set once the local player has left)
    unsigned char mUISlotNumber;            // +0xe1 (= 0xff)
    unsigned char mFastSelection[16];       // +0xe2 the last selection id of each player (indexed by player id)
    unsigned char mUF2;                     // +0xf2
    unsigned char mFastSelectionID;         // +0xf3 the local player's
    bool mIsExecutingSelection;             // +0xf4 (some selection messages execute twice when set)
    bool mIsMigrating;                      // +0xf5 (= false)
    bool mAllowServerMigration;             // +0xf6 (= true)
    bool mIgnoreConnectionTimeout;          // +0xf7
    bool mIsServerLost;                     // +0xf8
    bool mHasLoaded;                        // +0xf9
    bool mServerLostAfterLoading;           // +0xfa
    bool mWasBooted;                        // +0xfb
};

// 0xb86cd (cdecl; EEClient.cpp's last function): hands a new command group to ... (the command messages call it after
// SetGoal; provisional name and argument names)
class EEGroup;
class EEUnit;
// 0xb7992 (cdecl): the DisplayMessage callback after the host left (gUserInterface's 0x27921e; the argument is
// not read: the server and the client also call it with the address of a zeroed local; provisional name; types
// inferred)
void CancelGameCallback(void* inData);
void EESendGroupCommand(EEGroup* inGroup, EEUnit* inTarget, unsigned long inPlayerID, long inCommand, long inGoalQueue);

#endif
