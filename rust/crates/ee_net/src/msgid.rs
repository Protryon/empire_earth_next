//! The LLE's named integer constants, as plain (unscoped) C enums for its C++ headers; each comment gives the
//! evidence for its name, "(provisional name)" marks an inferred one.
#![allow(non_camel_case_types, clippy::enum_variant_names, clippy::upper_case_acronyms)]

/// NEMClientConnectionRequestResponse::mError: the server's answer to a connection request (EEServer::ProcessConnectionRequest / BootPlayer / RefuseClient send it; EEClient::HandleMessage case 0x23 reacts).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NEConnectionResult {
    /// EEServer::ProcessConnectionRequest posts it locally after registering the new player's slot; EEClient::FnB80EF case 0x23 then PostLobbyUpdateForm + PlayerJoined (provisional name)
    kConnectionPlayerJoined = 0,
    /// sent when the join succeeds; EEClient opens the lobby form (0x1a), NESynchronousClient takes mLastTickReceived from its CT (provisional name)
    kConnectionAccepted = 1,
    /// sent when a player of the same name exists (_mbsicmp); client shows string 10001 'Player name already exist in game.' (provisional name)
    kConnectionNameInUse = 2,
    /// sent when theIndex >= 16 / no saved-game seat; client shows string 10002 'No open slots left in game!' (provisional name)
    kConnectionGameFull = 3,
    /// sent when inVersion != gGameVersion; client shows string 10008 'To join this game you need version %s' (provisional name)
    kConnectionVersionMismatch = 4,
    /// BootPlayer(inA) / RefuseClient; client posts EEABootClientFromCanceledGame(10007 'The server has removed you from the game.') (provisional name)
    kConnectionRemovedByHost = 5,
    /// BootPlayer of the host slot 0xfe sends it to every client; client posts EEABootClientFromCanceledGame(10011 'The host of this game has gone away!') (provisional name)
    kConnectionHostGone = 7,
    /// sent when inChecksum != gBinaryChecksum; client shows string 30445 'One or more of your files are not the correct version or corrupt.' (provisional name)
    kConnectionBadChecksum = 8,
    /// client shows string 10065 'The Game Is In Progress' (the server sends 0x2751 instead: original bug at 0x4caad8) (provisional name)
    kConnectionGameInProgress = 9,
    /// what EEServer::ProcessConnectionRequest actually sends for a closed (running) game: string id 10065 in place of kConnectionGameInProgress; kept on the wire for compatibility, the client accepts both
    kConnectionGameInProgressAsSent = 0x2751,
}

/// Engine network message type ids (NEMessage::mType): NEMessage::BuildNEMessage case -> new NEM<Name>; each NEM ctor passes its id.
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NEMessageID {
    /// RTTI .?AVNEMAddPlayer@@; NEMAddPlayer ctor passes 0x20 / NEMessage::BuildNEMessage case 0x20 -> new NEMAddPlayer
    kNetMessageAddPlayer = 0x20,
    /// RTTI .?AVNEMAddPlayerResponse@@; NEMAddPlayerResponse ctor passes 0x21 / NEMessage::BuildNEMessage case 0x21 -> new NEMAddPlayerResponse
    kNetMessageAddPlayerResponse = 0x21,
    /// RTTI .?AVNEMClientConnectionRequest@@; NEMClientConnectionRequest ctor passes 0x22 / NEMessage::BuildNEMessage case 0x22 -> new NEMClientConnectionRequest
    kNetMessageClientConnectionRequest = 0x22,
    /// RTTI .?AVNEMClientConnectionRequestResponse@@; NEMClientConnectionRequestResponse ctor passes 0x23 / NEMessage::BuildNEMessage case 0x23 -> new NEMClientConnectionRequestResponse
    kNetMessageClientConnectionRequestResponse = 0x23,
    /// RTTI .?AVNEMClientDisconnectionNotification@@; NEMClientDisconnectionNotification ctor passes 0x24 / NEMessage::BuildNEMessage case 0x24 -> new NEMClientDisconnectionNotification
    kNetMessageClientDisconnectionNotification = 0x24,
    /// RTTI .?AVNEMClientDisconnectionNotificationResponse@@; NEMClientDisconnectionNotificationResponse ctor passes 0x25 / NEMessage::BuildNEMessage case 0x25 -> new NEMClientDisconnectionNotificationResponse
    kNetMessageClientDisconnectionNotificationResponse = 0x25,
    /// RTTI .?AVNEMCommunicationsTick@@; NEMCommunicationsTick ctor passes 0x26 / NEMessage::BuildNEMessage case 0x26 -> new NEMCommunicationsTick
    kNetMessageCommunicationsTick = 0x26,
    /// RTTI .?AVNEMCommunicationsTickResponse@@; NEMCommunicationsTickResponse ctor passes 0x27 / NEMessage::BuildNEMessage case 0x27 -> new NEMCommunicationsTickResponse
    kNetMessageCommunicationsTickResponse = 0x27,
    /// RTTI .?AVNEMPhysicsTick@@; NEMPhysicsTick ctor passes 0x28 / NEMessage::BuildNEMessage case 0x28 -> new NEMPhysicsTick
    kNetMessagePhysicsTick = 0x28,
    /// RTTI .?AVNEMPlayerConnectionCheck@@; NEMPlayerConnectionCheck ctor passes 0x29 / NEMessage::BuildNEMessage case 0x29 -> new NEMPlayerConnectionCheck
    kNetMessagePlayerConnectionCheck = 0x29,
    /// RTTI .?AVNEMPlayerConnectionCheckResponse@@; NEMPlayerConnectionCheckResponse ctor passes 0x2a / NEMessage::BuildNEMessage case 0x2a -> new NEMPlayerConnectionCheckResponse
    kNetMessagePlayerConnectionCheckResponse = 0x2a,
    /// RTTI .?AVNEMRemovePlayer@@; NEMRemovePlayer ctor passes 0x2b / NEMessage::BuildNEMessage case 0x2b -> new NEMRemovePlayer
    kNetMessageRemovePlayer = 0x2b,
    /// RTTI .?AVNEMStartGame@@; NEMStartGame ctor passes 0x2d / NEMessage::BuildNEMessage case 0x2d -> new NEMStartGame
    kNetMessageStartGame = 0x2d,
    /// RTTI .?AVNEMStartGameResponse@@; NEMStartGameResponse ctor passes 0x2e / NEMessage::BuildNEMessage case 0x2e -> new NEMStartGameResponse
    kNetMessageStartGameResponse = 0x2e,
    /// RTTI .?AVNEMStopGame@@; NEMStopGame ctor passes 0x2f / NEMessage::BuildNEMessage case 0x2f -> new NEMStopGame
    kNetMessageStopGame = 0x2f,
    /// RTTI .?AVNEMStopGameResponse@@; NEMStopGameResponse ctor passes 0x30 / NEMessage::BuildNEMessage case 0x30 -> new NEMStopGameResponse
    kNetMessageStopGameResponse = 0x30,
    /// RTTI .?AVNEMWorldDatabaseChecksum@@; NEMWorldDatabaseChecksum ctor passes 0x31 / NEMessage::BuildNEMessage case 0x31 -> new NEMWorldDatabaseChecksum
    kNetMessageWorldDatabaseChecksum = 0x31,
    /// RTTI .?AVNEMWorldDatabaseChecksumResponse@@; NEMWorldDatabaseChecksumResponse ctor passes 0x32 / NEMessage::BuildNEMessage case 0x32 -> new NEMWorldDatabaseChecksumResponse
    kNetMessageWorldDatabaseChecksumResponse = 0x32,
    /// RTTI .?AVNEMPauseGame@@; NEMPauseGame ctor passes 0x33 / NEMessage::BuildNEMessage case 0x33 -> new NEMPauseGame
    kNetMessagePauseGame = 0x33,
    /// RTTI .?AVNEMPauseGameResponse@@; NEMPauseGameResponse ctor passes 0x34 / NEMessage::BuildNEMessage case 0x34 -> new NEMPauseGameResponse
    kNetMessagePauseGameResponse = 0x34,
    /// RTTI .?AVNEMUnPauseGame@@; NEMUnPauseGame ctor passes 0x35 / NEMessage::BuildNEMessage case 0x35 -> new NEMUnPauseGame
    kNetMessageUnPauseGame = 0x35,
    /// RTTI .?AVNEMUnPauseGameResponse@@; NEMUnPauseGameResponse ctor passes 0x36 / NEMessage::BuildNEMessage case 0x36 -> new NEMUnPauseGameResponse
    kNetMessageUnPauseGameResponse = 0x36,
    /// RTTI .?AVNEMClientReconnectionRequest@@; NEMClientReconnectionRequest ctor passes 0x37 / NEMessage::BuildNEMessage case 0x37 -> new NEMClientReconnectionRequest
    kNetMessageClientReconnectionRequest = 0x37,
    /// RTTI .?AVNEMClientReconnectionRequestResponse@@; NEMClientReconnectionRequestResponse ctor passes 0x38 / NEMessage::BuildNEMessage case 0x38 -> new NEMClientReconnectionRequestResponse
    kNetMessageClientReconnectionRequestResponse = 0x38,
    /// RTTI .?AVNEMClientRequestPause@@; NEMClientRequestPause ctor passes 0x39 / NEMessage::BuildNEMessage case 0x39 -> new NEMClientRequestPause
    kNetMessageClientRequestPause = 0x39,
    /// RTTI .?AVNEMServerIsAlive@@; NEMServerIsAlive ctor passes 0x3a / NEMessage::BuildNEMessage case 0x3a -> new NEMServerIsAlive
    kNetMessageServerIsAlive = 0x3a,
    /// RTTI .?AVNEMXferMessage@@; NEMXferMessage ctor passes 0x3b / NEMessage::BuildNEMessage case 0x3b -> new NEMXferMessage
    kNetMessageXferMessage = 0x3b,
    /// RTTI .?AVNEMXferStatus@@; NEMXferStatus ctor passes 0x3c / NEMessage::BuildNEMessage case 0x3c -> new NEMXferStatus
    kNetMessageXferStatus = 0x3c,
}

/// Special send-to slot ids (NEMessage originating / for-specific slot, RTSPlayer::mSlot, NEMessageManager slots).
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NESlot {
    /// the host's own slot: NESynchronousServer::ProcessLocalUserInput sets SetOriginatingSlot(0xfe) on every server-local message, RegisterSlot(0xfe) registers the server, RTSPlayer::mSlot 0xfe = the hosting player (provisional name)
    kSlotServer = 0xfe,
    /// no slot: GetSendToSlot / WhoHasNotRespondedNext return 0xff for none, RTSPlayer::mSlot 0xff = no connection, ForSpecificSlot 0xff = for every slot (EEClient counts inputs with ForSpecificSlot == 0xff or its own) (provisional name)
    kSlotNone = 0xff,
}

/// The recorder's mode (ee_net recplay.rs, NERecordPlayback whole in Rust; the game only ever passes)
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NERecordPlaybackMode {
    /// messages go straight to the callback
    kRecordModePassing = 1,
    kRecordModeRecording = 2,
    kRecordModePlaying = 3,
}

/// NESynchronousServer's game state (GetGameState); 2 is compared by EEServer only and stays unnamed
/// cbindgen:enum-class=false
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NEServerGameState {
    /// setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(true); Main start
    kServerStateOpen = 0,
    /// setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(false)
    kServerStateClosed = 1,
    /// setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mStartGameEvent
    kServerStateRunning = 3,
    /// setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mPauseGameEvent
    kServerStatePaused = 4,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_values_are_the_originals() {
        for (value, original) in [
            (NEMessageID::kNetMessageAddPlayer as u32, 0x20),
            (NEMessageID::kNetMessageXferStatus as u32, 0x3c),
            (NEConnectionResult::kConnectionGameInProgressAsSent as u32, 0x2751),
            (NESlot::kSlotServer as u32, 0xfe),
            (NESlot::kSlotNone as u32, 0xff),
            (NERecordPlaybackMode::kRecordModePlaying as u32, 3),
            (NEServerGameState::kServerStatePaused as u32, 4),
        ] {
            assert_eq!(value, original);
        }
        assert_eq!(std::mem::size_of::<NEMessageID>(), 4);
    }
}
