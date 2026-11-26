// C:\EEX\Empire Earth\Communications\EEClient.h (RECONSTRUCTION; file name inferred). The game's lock-step client
// (RTTI EEClient > NESynchronousClient; vtable 0x43d054; ctor 0xb71e1; sizeof 0xfc: `new` 0xfc at 0x21ac). Layout from
// the ctor and 0xb8418; provisional names unless an assert names them.
// Requires lle/NESynchronousClient.h.
#ifndef EE_EECLIENT_H
#define EE_EECLIENT_H

#include "rust/game/app.h"
#include "rust/game/gamecpp.h"
#include "lle/NESynchronousClient.h"
class EEGameOptionsPlayer;

class EEClient : public NESynchronousClient {
public:
    // Creates the game client: no lobby slot yet, server migration allowed, fast selection reset, its lock created
    EEClient() : NESynchronousClient(GameHost<EEClient>) { ee_gamecpp_client_init(this); }
    // Destroys the client, stopping its threads and releasing the fast-selection lock
    // +0x00 0xb7278 (??_G 0xb725c)
    virtual ~EEClient() { ee_gamecpp_client_drop(this); }
    // the engine's pure callbacks, implemented by the game (vtable 0x43d054; bodies in EEClient.cpp, names by base
    // slot; declared so that EEAJoinOrHostAGame.cpp can `new EEClient`)
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting);   // +0x04 0xb7722
    // Creates the local file for an incoming transfer in the directory of its extension (saved game, scenario, campaign)
    // Returns NULL for a short name, an unknown extension or a file of the same size already there
    // NEAR: SetFile takes the implicit UString from (const char*)inName, built in place; the original calls
    // theFile->GetFile() at every exit and drops the result (a compiled-away trace; EE_LOST_TRACE does not make it
    // exact, so it is not used); on the short-name exit theFile is NULL there, so the original crashes; exit tails differ
    // +0x24 0xb743b
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize) { return (FSFile*)ee_gamecpp_client_xfer_file(&inName, inSize); }
    // Returns whether a server migration is in progress
    // +0x28 0xb799d
    virtual bool IsPlaybackRecordingSession() { return this->mIsMigrating; }
    virtual unsigned long GetConnectTimeout();   // +0x2c 0xb79a4
    // Returns the client's lobby slot (0xff until the server accepts it)
    // +0x30 0xb79aa
    virtual unsigned char GetClientSlot() { return this->mUISlotNumber; }
    // Returns whether connection timeouts are ignored (set once this client is booted)
    // +0x3c 0xb7300
    virtual bool IgnoreConnectionTimeout() { return this->mIgnoreConnectionTimeout; }
    virtual void SetConnectionState(long inState);   // +0x40 0xb7307
    virtual void HandleMessage(NEMessage* inMessage);   // +0x44 0xb79b1
    virtual void HandleUserInput(NEMessage* inMessage);   // +0x48 0xb7ed4
    virtual void HandleConnectionLost();    // +0x4c 0xb7fa5
    virtual unsigned long HandleStartGame(unsigned long inParameter);   // +0x50 0xb843b
    // +0x54 0xe123d
    virtual void HandleStopGame() {  }
    // Shows the server migration's countdown in the chat log (host not responding, %d seconds)
    // +0x58 0xb8599
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining) { ee_gamecpp_client_timeout_warning(inRemaining); }
    // 0xb8418: clears the fast-selection state (as EEServer::ResetFastSelection 0xcc9ce)
    // Clears the fast-selection state (no selection executing, id 0, empty table)
    void ResetFastSelection() { ee_gamecpp_client_reset_fast(this); }
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
