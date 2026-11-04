// lle/NEMessageID.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_NEMESSAGEID_H
#define EE_NEMESSAGEID_H

// NEMClientConnectionRequestResponse::mError: the server's answer to a connection request (EEServer::ProcessConnectionRequest / BootPlayer / RefuseClient send it; EEClient::HandleMessage case 0x23 reacts).
enum NEConnectionResult {
    kConnectionPlayerJoined = 0,   // EEServer::ProcessConnectionRequest posts it locally after registering the new player's slot; EEClient::FnB80EF case 0x23 then PostLobbyUpdateForm + PlayerJoined (provisional name)
    kConnectionAccepted = 1,       // sent when the join succeeds; EEClient opens the lobby form (0x1a), NESynchronousClient takes mLastTickReceived from its CT (provisional name)
    kConnectionNameInUse = 2,      // sent when a player of the same name exists (_mbsicmp); client shows string 10001 'Player name already exist in game.' (provisional name)
    kConnectionGameFull = 3,       // sent when theIndex >= 16 / no saved-game seat; client shows string 10002 'No open slots left in game!' (provisional name)
    kConnectionVersionMismatch = 4,  // sent when inVersion != gGameVersion; client shows string 10008 'To join this game you need version %s' (provisional name)
    kConnectionRemovedByHost = 5,  // BootPlayer(inA) / RefuseClient; client posts EEABootClientFromCanceledGame(10007 'The server has removed you from the game.') (provisional name)
    kConnectionHostGone = 7,       // BootPlayer of the host slot 0xfe sends it to every client; client posts EEABootClientFromCanceledGame(10011 'The host of this game has gone away!') (provisional name)
    kConnectionBadChecksum = 8,    // sent when inChecksum != gBinaryChecksum; client shows string 30445 'One or more of your files are not the correct version or corrupt.' (provisional name)
    kConnectionGameInProgress = 9,  // client shows string 10065 'The Game Is In Progress' (the server sends 0x2751 instead: original bug at 0x4caad8) (provisional name)
    kConnectionGameInProgressAsSent = 0x2751,  // what EEServer::ProcessConnectionRequest actually sends for a closed (running) game: string id 10065 in place of kConnectionGameInProgress; kept on the wire for compatibility, the client accepts both
};

// Engine network message type ids (NEMessage::mType): NEMessage::BuildNEMessage case -> new NEM<Name>; each NEM ctor passes its id.
enum NEMessageID {
    kNetMessageAddPlayer = 0x20,                              // RTTI .?AVNEMAddPlayer@@; NEMAddPlayer ctor passes 0x20 / NEMessage::BuildNEMessage case 0x20 -> new NEMAddPlayer
    kNetMessageAddPlayerResponse = 0x21,                      // RTTI .?AVNEMAddPlayerResponse@@; NEMAddPlayerResponse ctor passes 0x21 / NEMessage::BuildNEMessage case 0x21 -> new NEMAddPlayerResponse
    kNetMessageClientConnectionRequest = 0x22,                // RTTI .?AVNEMClientConnectionRequest@@; NEMClientConnectionRequest ctor passes 0x22 / NEMessage::BuildNEMessage case 0x22 -> new NEMClientConnectionRequest
    kNetMessageClientConnectionRequestResponse = 0x23,        // RTTI .?AVNEMClientConnectionRequestResponse@@; NEMClientConnectionRequestResponse ctor passes 0x23 / NEMessage::BuildNEMessage case 0x23 -> new NEMClientConnectionRequestResponse
    kNetMessageClientDisconnectionNotification = 0x24,        // RTTI .?AVNEMClientDisconnectionNotification@@; NEMClientDisconnectionNotification ctor passes 0x24 / NEMessage::BuildNEMessage case 0x24 -> new NEMClientDisconnectionNotification
    kNetMessageClientDisconnectionNotificationResponse = 0x25,  // RTTI .?AVNEMClientDisconnectionNotificationResponse@@; NEMClientDisconnectionNotificationResponse ctor passes 0x25 / NEMessage::BuildNEMessage case 0x25 -> new NEMClientDisconnectionNotificationResponse
    kNetMessageCommunicationsTick = 0x26,                     // RTTI .?AVNEMCommunicationsTick@@; NEMCommunicationsTick ctor passes 0x26 / NEMessage::BuildNEMessage case 0x26 -> new NEMCommunicationsTick
    kNetMessageCommunicationsTickResponse = 0x27,             // RTTI .?AVNEMCommunicationsTickResponse@@; NEMCommunicationsTickResponse ctor passes 0x27 / NEMessage::BuildNEMessage case 0x27 -> new NEMCommunicationsTickResponse
    kNetMessagePhysicsTick = 0x28,                            // RTTI .?AVNEMPhysicsTick@@; NEMPhysicsTick ctor passes 0x28 / NEMessage::BuildNEMessage case 0x28 -> new NEMPhysicsTick
    kNetMessagePlayerConnectionCheck = 0x29,                  // RTTI .?AVNEMPlayerConnectionCheck@@; NEMPlayerConnectionCheck ctor passes 0x29 / NEMessage::BuildNEMessage case 0x29 -> new NEMPlayerConnectionCheck
    kNetMessagePlayerConnectionCheckResponse = 0x2a,          // RTTI .?AVNEMPlayerConnectionCheckResponse@@; NEMPlayerConnectionCheckResponse ctor passes 0x2a / NEMessage::BuildNEMessage case 0x2a -> new NEMPlayerConnectionCheckResponse
    kNetMessageRemovePlayer = 0x2b,                           // RTTI .?AVNEMRemovePlayer@@; NEMRemovePlayer ctor passes 0x2b / NEMessage::BuildNEMessage case 0x2b -> new NEMRemovePlayer
    kNetMessageStartGame = 0x2d,                              // RTTI .?AVNEMStartGame@@; NEMStartGame ctor passes 0x2d / NEMessage::BuildNEMessage case 0x2d -> new NEMStartGame
    kNetMessageStartGameResponse = 0x2e,                      // RTTI .?AVNEMStartGameResponse@@; NEMStartGameResponse ctor passes 0x2e / NEMessage::BuildNEMessage case 0x2e -> new NEMStartGameResponse
    kNetMessageStopGame = 0x2f,                               // RTTI .?AVNEMStopGame@@; NEMStopGame ctor passes 0x2f / NEMessage::BuildNEMessage case 0x2f -> new NEMStopGame
    kNetMessageStopGameResponse = 0x30,                       // RTTI .?AVNEMStopGameResponse@@; NEMStopGameResponse ctor passes 0x30 / NEMessage::BuildNEMessage case 0x30 -> new NEMStopGameResponse
    kNetMessageWorldDatabaseChecksum = 0x31,                  // RTTI .?AVNEMWorldDatabaseChecksum@@; NEMWorldDatabaseChecksum ctor passes 0x31 / NEMessage::BuildNEMessage case 0x31 -> new NEMWorldDatabaseChecksum
    kNetMessageWorldDatabaseChecksumResponse = 0x32,          // RTTI .?AVNEMWorldDatabaseChecksumResponse@@; NEMWorldDatabaseChecksumResponse ctor passes 0x32 / NEMessage::BuildNEMessage case 0x32 -> new NEMWorldDatabaseChecksumResponse
    kNetMessagePauseGame = 0x33,                              // RTTI .?AVNEMPauseGame@@; NEMPauseGame ctor passes 0x33 / NEMessage::BuildNEMessage case 0x33 -> new NEMPauseGame
    kNetMessagePauseGameResponse = 0x34,                      // RTTI .?AVNEMPauseGameResponse@@; NEMPauseGameResponse ctor passes 0x34 / NEMessage::BuildNEMessage case 0x34 -> new NEMPauseGameResponse
    kNetMessageUnPauseGame = 0x35,                            // RTTI .?AVNEMUnPauseGame@@; NEMUnPauseGame ctor passes 0x35 / NEMessage::BuildNEMessage case 0x35 -> new NEMUnPauseGame
    kNetMessageUnPauseGameResponse = 0x36,                    // RTTI .?AVNEMUnPauseGameResponse@@; NEMUnPauseGameResponse ctor passes 0x36 / NEMessage::BuildNEMessage case 0x36 -> new NEMUnPauseGameResponse
    kNetMessageClientReconnectionRequest = 0x37,              // RTTI .?AVNEMClientReconnectionRequest@@; NEMClientReconnectionRequest ctor passes 0x37 / NEMessage::BuildNEMessage case 0x37 -> new NEMClientReconnectionRequest
    kNetMessageClientReconnectionRequestResponse = 0x38,      // RTTI .?AVNEMClientReconnectionRequestResponse@@; NEMClientReconnectionRequestResponse ctor passes 0x38 / NEMessage::BuildNEMessage case 0x38 -> new NEMClientReconnectionRequestResponse
    kNetMessageClientRequestPause = 0x39,                     // RTTI .?AVNEMClientRequestPause@@; NEMClientRequestPause ctor passes 0x39 / NEMessage::BuildNEMessage case 0x39 -> new NEMClientRequestPause
    kNetMessageServerIsAlive = 0x3a,                          // RTTI .?AVNEMServerIsAlive@@; NEMServerIsAlive ctor passes 0x3a / NEMessage::BuildNEMessage case 0x3a -> new NEMServerIsAlive
    kNetMessageXferMessage = 0x3b,                            // RTTI .?AVNEMXferMessage@@; NEMXferMessage ctor passes 0x3b / NEMessage::BuildNEMessage case 0x3b -> new NEMXferMessage
    kNetMessageXferStatus = 0x3c,                             // RTTI .?AVNEMXferStatus@@; NEMXferStatus ctor passes 0x3c / NEMessage::BuildNEMessage case 0x3c -> new NEMXferStatus
};

// Special send-to slot ids (NEMessage originating / for-specific slot, RTSPlayer::mSlot, NEMessageManager slots).
enum NESlot {
    kSlotServer = 0xfe,  // the host's own slot: NESynchronousServer::ProcessLocalUserInput sets SetOriginatingSlot(0xfe) on every server-local message, RegisterSlot(0xfe) registers the server, RTSPlayer::mSlot 0xfe = the hosting player (provisional name)
    kSlotNone = 0xff,  // no slot: GetSendToSlot / WhoHasNotRespondedNext return 0xff for none, RTSPlayer::mSlot 0xff = no connection, ForSpecificSlot 0xff = for every slot (EEClient counts inputs with ForSpecificSlot == 0xff or its own) (provisional name)
};

#endif // EE_NEMESSAGEID_H
