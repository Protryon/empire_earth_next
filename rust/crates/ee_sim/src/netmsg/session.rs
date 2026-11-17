//! The game's lock-step server and client (`EEServer.cpp`, `EEClient.cpp`; game messages round 6): the connection
//! handling (the join request's refusals and acceptance, the boots, the removals), the lobby's slots and players, the
//! communications ticks' inputs, the edict votes, the laggers, the slot info, the host migration's restart and the
//! client's answers to the server, as decisions over one C++ host (`EESessionHost` in EEServer.cpp) whose operations
//! are the world, lobby, UI and engine calls those members made, each in the original's order.
//!
//! The C++ classes keep their members (the engine's virtuals forward here); the lobby slots the bodies work on are
//! the host's registers (`EE_NETMSG_SESS_R_PLAYER`, `EE_NETMSG_SESS_R_OTHER`: two `EEGameOptionsPlayer`s in the forwarder's frame, and
//! `EE_NETMSG_SESS_R_IN`: a join request's player). Messages are the host's pointers (`u64`). The golden: `tests/net/
//! ee_session_test.cpp` (the original units over a stand-in world, 6,000 random calls; the same driver over these).

use std::ffi::c_void;

/// One host call: the forwarder's frame (the communications object, the registers) and the values.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeNetSessCall {
    pub frame: *mut c_void,
    pub v: [i64; 8],
}

/// The C++ host (EEServer.cpp's `EESessionHost`).
pub type EeNetSessHostFn = unsafe extern "C" fn(op: u32, call: *mut EeNetSessCall) -> i64;

// ---- the host's operations (v0: the kind, then the values) ----
/// a lock entered (v1 1) or left (v1 0): `EE_NETMSG_SESS_LOCK_*`
pub const EE_NETMSG_SESS_OP_LOCK: u32 = 1;
/// a fact (`EE_NETMSG_SESS_FACT_*`, v1 its argument)
pub const EE_NETMSG_SESS_OP_FACT: u32 = 2;
/// the communications object's member v0 (`EE_NETMSG_SESS_SELF_*`; v1 an index)
pub const EE_NETMSG_SESS_OP_SELF_GET: u32 = 3;
/// sets the communications object's member v0 (v1 an index) to v2
pub const EE_NETMSG_SESS_OP_SELF_SET: u32 = 4;
/// a lobby slot register (v0 the register, v1 `EE_NETMSG_SESS_REG_*`, then its values)
pub const EE_NETMSG_SESS_OP_REG: u32 = 5;
/// a message sent (`EE_NETMSG_SESS_SEND_*`)
pub const EE_NETMSG_SESS_OP_SEND: u32 = 6;
/// a message's member or call (`EE_NETMSG_SESS_MSG_*`, v1 the message)
pub const EE_NETMSG_SESS_OP_MSG: u32 = 7;
/// the UI (`EE_NETMSG_SESS_UI_*`)
pub const EE_NETMSG_SESS_OP_UI: u32 = 8;
/// WON's game info (`EE_NETMSG_SESS_WON_*`; only when gWonNet is there)
pub const EE_NETMSG_SESS_OP_WON: u32 = 9;
/// a sound: v0 0 the server's (v1 out of sync), else `EELoadSound` of sound v0 (1 out of sync, 2 resending, 3 booted)
pub const EE_NETMSG_SESS_OP_SOUND: u32 = 10;
/// the world (`EE_NETMSG_SESS_WORLD_*`)
pub const EE_NETMSG_SESS_OP_WORLD: u32 = 11;
/// the engine's server or client (`EE_NETMSG_SESS_COMMS_*`)
pub const EE_NETMSG_SESS_OP_COMMS: u32 = 12;
/// the game start (`EE_NETMSG_SESS_START_*`)
pub const EE_NETMSG_SESS_OP_START: u32 = 13;

// locks
/// the server's `LockSendCommunicationsTick` / `Unlock...`
pub const EE_NETMSG_SESS_LOCK_CT: i64 = 0;
/// `gGameOptions.mLock` for writing
pub const EE_NETMSG_SESS_LOCK_OPTIONS_WRITE: i64 = 1;
/// the server's `mSearchForSlotLock`
pub const EE_NETMSG_SESS_LOCK_SEARCH: i64 = 2;
/// the server's `mAttemptToRemovePlayerLock`
pub const EE_NETMSG_SESS_LOCK_ATTEMPT: i64 = 3;
/// the server's or the client's `mFastSelectionLock`
pub const EE_NETMSG_SESS_LOCK_FAST: i64 = 4;
/// `gPhysics->mServerMessageQueueLock`
pub const EE_NETMSG_SESS_LOCK_PHYSICS: i64 = 5;
/// `gWorld.mLock` for writing
pub const EE_NETMSG_SESS_LOCK_WORLD_WRITE: i64 = 6;
/// the server's ping: `StopPing` (enter), `StartPing` (leave)
pub const EE_NETMSG_SESS_LOCK_PING: i64 = 7;

// facts
/// `EEIsMultiplayerGame(true)`
pub const EE_NETMSG_SESS_FACT_MULTIPLAYER_GAME: i64 = 0;
/// `gGameOptions.mIsMultiplayer`
pub const EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER: i64 = 1;
/// `gGameOptions.mLoadType`
pub const EE_NETMSG_SESS_FACT_LOAD_TYPE: i64 = 2;
/// the server's `GetGameState()`
pub const EE_NETMSG_SESS_FACT_GAME_STATE: i64 = 3;
/// `gDefaultPlayerColors[v1]`
pub const EE_NETMSG_SESS_FACT_DEFAULT_COLOR: i64 = 4;
/// the world table's starting citizens default
pub const EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT: i64 = 5;
/// the join request's version equals `gGameVersion`
pub const EE_NETMSG_SESS_FACT_VERSION_MATCH: i64 = 6;
/// `gBinaryChecksum`
pub const EE_NETMSG_SESS_FACT_BINARY_CHECKSUM: i64 = 7;
/// `GetCurrentCommunicationsTick()` (the virtual)
pub const EE_NETMSG_SESS_FACT_CURRENT_CT: i64 = 8;
/// `GetNumberOfCTsProcessedInPhysics()`
pub const EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS: i64 = 9;
/// `GetNumHumanPlayers()`
pub const EE_NETMSG_SESS_FACT_HUMANS: i64 = 10;
/// `GetIsMultiplayerSession()`
pub const EE_NETMSG_SESS_FACT_MP_SESSION: i64 = 11;
/// `GetPlayerNumber()`
pub const EE_NETMSG_SESS_FACT_PLAYER_NUMBER: i64 = 12;
/// the client's `GetCurrentPhysicsTick()`
pub const EE_NETMSG_SESS_FACT_CURRENT_PT: i64 = 13;
/// the client's `GetMySendToSlot()`
pub const EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT: i64 = 14;
/// the server's `GetNumberOfPlayers()`
pub const EE_NETMSG_SESS_FACT_NUM_PLAYERS: i64 = 15;
/// `gWorld.mNumPlayers`
pub const EE_NETMSG_SESS_FACT_WORLD_PLAYERS: i64 = 16;
/// `TMGetTime()`
pub const EE_NETMSG_SESS_FACT_TIME: i64 = 17;
/// `gWonNet != NULL`
pub const EE_NETMSG_SESS_FACT_WON: i64 = 18;
/// `gGameScreenActive`
pub const EE_NETMSG_SESS_FACT_SCREEN_ACTIVE: i64 = 19;
/// `EECPEngineChoiceCount()`
pub const EE_NETMSG_SESS_FACT_ENGINE_CHOICES: i64 = 20;
/// the current form is form v1 (v2 0: the form looked up first, 1: the current form first)
pub const EE_NETMSG_SESS_FACT_FORM_IS_CURRENT: i64 = 21;
/// form v1 is visible
pub const EE_NETMSG_SESS_FACT_FORM_VISIBLE: i64 = 22;
/// the server's `GetSendToSlot(v1)`
pub const EE_NETMSG_SESS_FACT_SEND_TO_SLOT: i64 = 23;
/// the server's `GetPlayerSendToSlot(v1)` (the virtual)
pub const EE_NETMSG_SESS_FACT_PLAYER_SEND_TO: i64 = 24;

// the communications object's members
pub const EE_NETMSG_SESS_SELF_HAS_LEFT: i64 = 0;
pub const EE_NETMSG_SESS_SELF_MIGRATING: i64 = 1;
pub const EE_NETMSG_SESS_SELF_FAST_ID: i64 = 2;
pub const EE_NETMSG_SESS_SELF_EXECUTING: i64 = 3;
/// the server's
pub const EE_NETMSG_SESS_SELF_LOAD_STATE: i64 = 4;
pub const EE_NETMSG_SESS_SELF_DROPPED_LOADERS: i64 = 5;
pub const EE_NETMSG_SESS_SELF_VOTE_EDICT: i64 = 6;
pub const EE_NETMSG_SESS_SELF_YES_VOTES: i64 = 7;
pub const EE_NETMSG_SESS_SELF_NO_VOTES: i64 = 8;
pub const EE_NETMSG_SESS_SELF_NUM_VOTES: i64 = 9;
/// `mVoted[v1]` (set: v1 -1 clears all 256)
pub const EE_NETMSG_SESS_SELF_VOTED: i64 = 10;
pub const EE_NETMSG_SESS_SELF_LAG_MASK: i64 = 11;
/// the client's
pub const EE_NETMSG_SESS_SELF_UI_SLOT: i64 = 12;
pub const EE_NETMSG_SESS_SELF_ALLOW_MIGRATION: i64 = 13;
pub const EE_NETMSG_SESS_SELF_IGNORE_TIMEOUT: i64 = 14;
pub const EE_NETMSG_SESS_SELF_SERVER_LOST: i64 = 15;
pub const EE_NETMSG_SESS_SELF_HAS_LOADED: i64 = 16;
pub const EE_NETMSG_SESS_SELF_LOST_AFTER_LOADING: i64 = 17;
pub const EE_NETMSG_SESS_SELF_WAS_BOOTED: i64 = 18;

// the registers
/// `thePlayer`
pub const EE_NETMSG_SESS_R_PLAYER: i64 = 0;
/// `theOther`
pub const EE_NETMSG_SESS_R_OTHER: i64 = 1;
/// the join request's player (read only)
pub const EE_NETMSG_SESS_R_IN: i64 = 2;

/// the register = `gGameOptions.mPlayers[v2]`
pub const EE_NETMSG_SESS_REG_LOAD_OPTIONS: i64 = 0;
/// the register = `gWorld.GetLobbyOptions().mPlayers[v2]`
pub const EE_NETMSG_SESS_REG_LOAD_LOBBY: i64 = 1;
/// the register = register v2
pub const EE_NETMSG_SESS_REG_LOAD_REG: i64 = 2;
/// `gGameOptions.mPlayers[v2]` = the register
pub const EE_NETMSG_SESS_REG_STORE_OPTIONS: i64 = 3;
/// `gWorld.GetLobbyOptions().mPlayers[v2]` = the register
pub const EE_NETMSG_SESS_REG_STORE_LOBBY: i64 = 4;
/// field v2 (`EE_NETMSG_SESS_FIELD_*`; the ping as its float bits)
pub const EE_NETMSG_SESS_REG_GET: i64 = 5;
/// field v2 = v3
pub const EE_NETMSG_SESS_REG_SET: i64 = 6;
/// the name = ""
pub const EE_NETMSG_SESS_REG_NAME_CLEAR: i64 = 7;
/// the name = register v2's
pub const EE_NETMSG_SESS_REG_NAME_COPY: i64 = 8;
/// the name equals register v2's ignoring case (`PlatMbsICmp` == 0)
pub const EE_NETMSG_SESS_REG_NAME_ICMP: i64 = 9;
/// the name equals register v2's (`==`)
pub const EE_NETMSG_SESS_REG_NAME_EQ: i64 = 10;
/// the name = message v2's string (`EEMRecord::mString`)
pub const EE_NETMSG_SESS_REG_NAME_FROM_MSG: i64 = 11;
/// the name equals world player v2's ignoring case
pub const EE_NETMSG_SESS_REG_NAME_ICMP_PLAYER: i64 = 12;

pub const EE_NETMSG_SESS_FIELD_TYPE: i64 = 0;
pub const EE_NETMSG_SESS_FIELD_LOADED_TYPE: i64 = 1;
pub const EE_NETMSG_SESS_FIELD_PING: i64 = 2;
pub const EE_NETMSG_SESS_FIELD_SLOT: i64 = 3;
pub const EE_NETMSG_SESS_FIELD_COLOR: i64 = 4;
pub const EE_NETMSG_SESS_FIELD_TEAM: i64 = 5;
pub const EE_NETMSG_SESS_FIELD_CITIZENS: i64 = 6;
pub const EE_NETMSG_SESS_FIELD_TRANSFER_ID: i64 = 7;
pub const EE_NETMSG_SESS_FIELD_LOCAL: i64 = 8;
pub const EE_NETMSG_SESS_FIELD_READY: i64 = 9;
pub const EE_NETMSG_SESS_FIELD_B36: i64 = 10;
pub const EE_NETMSG_SESS_FIELD_ENGINE: i64 = 11;

// sends
/// `SendConnectionResponse(v1 slot, v2 error, v3 send-to slot, v4 lobby slot, v5 unregister)`
pub const EE_NETMSG_SESS_SEND_RESPONSE: i64 = 0;
/// `ProcessLocalUserInput(NewConnectionResponse(v1 error, v2 send-to slot, v3 lobby slot), v4)`
pub const EE_NETMSG_SESS_SEND_LOCAL_RESPONSE: i64 = 1;
/// `gCommunications->ProcessLocalUserInput(EENewFullGameOptions(gGameOptions), v1)`
pub const EE_NETMSG_SESS_SEND_FULL_OPTIONS: i64 = 2;
/// `ProcessLocalUserInput(EENewFullPlayerInformation(register v1), v2)`, from the server's slot when v3
pub const EE_NETMSG_SESS_SEND_PLAYER_INFO: i64 = 3;
/// a new `EEMRecord(v1 type, {v3 .. v3 + v2})`: answers the message
pub const EE_NETMSG_SESS_SEND_NEW_RECORD: i64 = 4;
/// the server's `SendPingStat(v1, the float bits v2)`
pub const EE_NETMSG_SESS_SEND_PING_STAT: i64 = 5;
/// the server's slot info of player v1 at send-to slot v2 (`gCommunications->ProcessLocalUserInput`, no slot)
pub const EE_NETMSG_SESS_SEND_SLOT_INFO: i64 = 6;
/// a new physics tick (v1 inputs, v2 ticks, v3 checksum, v4 dump, v5 PTs per second): answers the message
pub const EE_NETMSG_SESS_SEND_PHYSICS_TICK: i64 = 7;
/// the client's connection check's answer (v1 the ping's start), `ProcessLocalUserInput` to no slot
pub const EE_NETMSG_SESS_SEND_CHECK_RESPONSE: i64 = 8;

// messages
pub const EE_NETMSG_SESS_MSG_TYPE: i64 = 0;
pub const EE_NETMSG_SESS_MSG_ORIGIN: i64 = 1;
pub const EE_NETMSG_SESS_MSG_FOR_SLOT: i64 = 2;
/// an engine message's member v2 (`NEMRecord::Value`)
pub const EE_NETMSG_SESS_MSG_NE_VALUE: i64 = 3;
/// a game message's record copied to v2 (64 bytes)
pub const EE_NETMSG_SESS_MSG_RECORD: i64 = 4;
pub const EE_NETMSG_SESS_MSG_INPUT_COUNT: i64 = 5;
/// the communications tick's input v2
pub const EE_NETMSG_SESS_MSG_INPUT: i64 = 6;
pub const EE_NETMSG_SESS_MSG_REFERENCE: i64 = 7;
pub const EE_NETMSG_SESS_MSG_DEREFERENCE: i64 = 8;
pub const EE_NETMSG_SESS_MSG_EXECUTE: i64 = 9;
/// the game message's selection id = v2
pub const EE_NETMSG_SESS_MSG_SET_FAST_ID: i64 = 10;
/// `EEGameMessageCast(message) != NULL`
pub const EE_NETMSG_SESS_MSG_IS_GAME: i64 = 11;
/// `gPhysics->QueueServerMessage`
pub const EE_NETMSG_SESS_MSG_QUEUE: i64 = 12;
/// the server's `LogResponse`
pub const EE_NETMSG_SESS_MSG_LOG_RESPONSE: i64 = 13;
/// `ProcessLocalUserInput(message, v2)`
pub const EE_NETMSG_SESS_MSG_LOCAL: i64 = 14;
/// from the server's slot, `BroadcastMessage`
pub const EE_NETMSG_SESS_MSG_BROADCAST: i64 = 15;
/// from the server's slot, `SendMessageToAll(message, false)`, released
pub const EE_NETMSG_SESS_MSG_SEND_ALL: i64 = 16;
/// the server's `TakeConnectionRequest`
pub const EE_NETMSG_SESS_MSG_TAKE_REQUEST: i64 = 17;

// UI
/// `PostLobbyUpdatePlayer(v1, 8)`
pub const EE_NETMSG_SESS_UI_LOBBY_PLAYER: i64 = 0;
/// `PostAction(new EEARemovePlayer(v1, v2))`
pub const EE_NETMSG_SESS_UI_ACTION_REMOVE: i64 = 1;
/// `PostAction(new EEABootClientFromCanceledGame(v1))`
pub const EE_NETMSG_SESS_UI_ACTION_BOOT: i64 = 2;
/// `PostAction(new EEAServerMigration)`
pub const EE_NETMSG_SESS_UI_ACTION_MIGRATION: i64 = 3;
/// the main game form's pause event
pub const EE_NETMSG_SESS_UI_PAUSE_EVENT: i64 = 4;
/// string v1 shown (`DisplayMessage(text, true, NULL, v2 ? CancelGameCallback : NULL, v3)`)
pub const EE_NETMSG_SESS_UI_DISPLAY: i64 = 5;
/// the game's chat line: string v1 with player v2's name
pub const EE_NETMSG_SESS_UI_CHAT_PLAYER: i64 = 6;
/// `AddChatLogEntry(string v1, v2)`
pub const EE_NETMSG_SESS_UI_CHAT_TEXT: i64 = 7;
pub const EE_NETMSG_SESS_UI_CLOSE_FORM: i64 = 8;
/// `OpenForm(GetFormByID(v1))`
pub const EE_NETMSG_SESS_UI_OPEN_FORM: i64 = 9;
/// `Push(GetFormByID(v1))`
pub const EE_NETMSG_SESS_UI_PUSH_FORM: i64 = 10;
pub const EE_NETMSG_SESS_UI_CANCEL_GAME: i64 = 11;
/// `CancelGameCallback`
pub const EE_NETMSG_SESS_UI_CANCEL_CALLBACK: i64 = 12;
pub const EE_NETMSG_SESS_UI_LOBBY_FORM: i64 = 13;
pub const EE_NETMSG_SESS_UI_HOST_MIGRATED: i64 = 14;
/// `PostLobbyShowTransferProgress(v1, v2)`
pub const EE_NETMSG_SESS_UI_TRANSFER: i64 = 15;
/// `EELobbyPostConnectStatus(v1)`
pub const EE_NETMSG_SESS_UI_CONNECT_STATUS: i64 = 16;
/// the connecting box with the client's retries
pub const EE_NETMSG_SESS_UI_RETRY: i64 = 17;
/// the client leaves the game: string v1, way v2 (`LEAVE_*`), the response v3 (its version); disconnected
pub const EE_NETMSG_SESS_UI_LEAVE: i64 = 18;
/// the client's `HandleSlotUpdate(v1, register v2)`
pub const EE_NETMSG_SESS_UI_SLOT_UPDATE: i64 = 19;

pub const EE_NETMSG_SESS_LEAVE_SHOW: i64 = 0;
pub const EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS: i64 = 1;
pub const EE_NETMSG_SESS_LEAVE_BOOT: i64 = 2;
pub const EE_NETMSG_SESS_LEAVE_QUIET: i64 = 3;

// WON
/// the game info's players v1 and humans v2
pub const EE_NETMSG_SESS_WON_COUNT: i64 = 0;
/// one human fewer
pub const EE_NETMSG_SESS_WON_HUMANS_DEC: i64 = 1;
pub const EE_NETMSG_SESS_WON_JOINED: i64 = 2;
/// back to the game list (`ClearGameList`, `RejoinRoom(false)`)
pub const EE_NETMSG_SESS_WON_LEAVE: i64 = 3;

// the world
/// `gWorld.mPlayers[v1]`
pub const EE_NETMSG_SESS_WORLD_PLAYER: i64 = 0;
/// `gWorld.GetPlayerBySlot(v1)`
pub const EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT: i64 = 1;
/// player v1's `IsHuman()`
pub const EE_NETMSG_SESS_WORLD_IS_HUMAN: i64 = 2;
/// player v1's `mSlot`
pub const EE_NETMSG_SESS_WORLD_SLOT: i64 = 3;
/// player v1's `mHumanPlayer != NULL`
pub const EE_NETMSG_SESS_WORLD_HAS_HUMAN: i64 = 4;
/// player v1's `mPlayerID`
pub const EE_NETMSG_SESS_WORLD_ID: i64 = 5;
/// player v1's `mSlot` = v2
pub const EE_NETMSG_SESS_WORLD_SET_SLOT: i64 = 6;
/// player v1's `mBehindFirewall` = v2
pub const EE_NETMSG_SESS_WORLD_SET_FIREWALL: i64 = 7;
/// human player v1's name = message v2's string
pub const EE_NETMSG_SESS_WORLD_NAME_FROM_MSG: i64 = 8;
/// `gWorld.GetPlayer(v1)->mSlot = v2`
pub const EE_NETMSG_SESS_WORLD_RECONNECT: i64 = 9;
/// `gWorld.RemoveDisconnectedPlayer(v1)`
pub const EE_NETMSG_SESS_WORLD_REMOVE: i64 = 10;
/// the current player's `ExecuteSelectionMessage(v1)`
pub const EE_NETMSG_SESS_WORLD_EXECUTE_SELECTION: i64 = 11;

// the engine
pub const EE_NETMSG_SESS_COMMS_SET_CT: i64 = 0;
pub const EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS: i64 = 1;
pub const EE_NETMSG_SESS_COMMS_INCREMENT_CTS: i64 = 2;
pub const EE_NETMSG_SESS_COMMS_SET_MP_SESSION: i64 = 3;
pub const EE_NETMSG_SESS_COMMS_SET_HUMANS: i64 = 4;
pub const EE_NETMSG_SESS_COMMS_SET_PT: i64 = 5;
pub const EE_NETMSG_SESS_COMMS_REGISTER: i64 = 6;
pub const EE_NETMSG_SESS_COMMS_UNREGISTER: i64 = 7;
/// the server's `mSendToSlots.clear()`
pub const EE_NETMSG_SESS_COMMS_SEND_TO_CLEAR: i64 = 8;
/// `mSendToSlots.insert(v1, v2)`
pub const EE_NETMSG_SESS_COMMS_SEND_TO_INSERT: i64 = 9;
/// `mSendToSlots.erase(v1)`
pub const EE_NETMSG_SESS_COMMS_SEND_TO_ERASE: i64 = 10;
pub const EE_NETMSG_SESS_COMMS_ABORT_XFER: i64 = 11;
/// `RemovePlayerFromGameInProgress(v1, v2, false, v3)`
pub const EE_NETMSG_SESS_COMMS_REMOVE_IN_PROGRESS: i64 = 12;
/// the saved game or scenario (load type v1) sent to lobby slot v2's client
pub const EE_NETMSG_SESS_COMMS_XFER_GAME_FILE: i64 = 13;
pub const EE_NETMSG_SESS_COMMS_START_GAME: i64 = 14;
pub const EE_NETMSG_SESS_COMMS_PAUSE_GAME: i64 = 15;
/// the host removed: the server deleted, a client made (`gCommunications`)
pub const EE_NETMSG_SESS_COMMS_BECOME_CLIENT: i64 = 16;
pub const EE_NETMSG_SESS_COMMS_LOWEST_CTS: i64 = 17;
pub const EE_NETMSG_SESS_COMMS_DISCONNECT: i64 = 18;
/// `gLocalPlayerOut` = v1
pub const EE_NETMSG_SESS_COMMS_LOCAL_OUT: i64 = 19;
/// `PlatSleep(v1)`
pub const EE_NETMSG_SESS_COMMS_SLEEP: i64 = 20;
/// protocol 0 kept for the slot info's local address (`NEProtocol::GetProtocol`)
pub const EE_NETMSG_SESS_COMMS_PROTOCOL: i64 = 21;

// the game start
/// the start message v1's time, the physics' random draw, the teams, the protocol's status
pub const EE_NETMSG_SESS_START_PREP: i64 = 0;
/// the graphics resumed, waited for
pub const EE_NETMSG_SESS_START_RESUME: i64 = 1;
/// the physics' timer started, waited for; the main game form's state 0
pub const EE_NETMSG_SESS_START_PHYSICS: i64 = 2;

// ---- the strings and codes ----
const IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME: i64 = 10001;
const IDS_NO_OPEN_SLOTS_LEFT_IN_GAME: i64 = 10002;
const IDS_SERVER_HAS_REMOVED_YOU_FROM_GAME: i64 = 10007;
const IDS_TO_JOIN_THIS_GAME_YOU: i64 = 10008;
const IDS_HOST_OF_THIS_GAME_HAS_GONE: i64 = 10011;
const IDS_COULD_NOT_CONNECT_TO_SELECTED: i64 = 10012;
const IDS_FILE_TRANSFER_FAILED: i64 = 10064;
const IDS_GAME_IS_IN_PROGRESS: i64 = 10065;
const IDS_ONE_OR_MORE_OF_YOUR_FILES: i64 = 30445;
const IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME: i64 = 30612;
const IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT: i64 = 30621;
const IDS_NEW_HOST_ESTABLISHED: i64 = 30634;
const IDS_PLAYER_IS_OUT_OF_SYNCH: i64 = 31708;
const IDS_HOST_IS_OUT_OF_SYNCH: i64 = 31709;
const IDS_YOU_ARE_OUT_OF_SYNCH: i64 = 31710;
const IDS_YOU_HAVE_BEEN_BOOTED_DUE: i64 = 31711;
const IDS_CONNECTING_TO_SERVER: i64 = 31848;

const CONNECTION_PLAYER_JOINED: i64 = 0;
const CONNECTION_ACCEPTED: i64 = 1;
const CONNECTION_NAME_IN_USE: i64 = 2;
const CONNECTION_GAME_FULL: i64 = 3;
const CONNECTION_VERSION_MISMATCH: i64 = 4;
const CONNECTION_REMOVED_BY_HOST: i64 = 5;
const CONNECTION_GAME_EXITING: i64 = 6;
const CONNECTION_HOST_GONE: i64 = 7;
const CONNECTION_BAD_CHECKSUM: i64 = 8;
const CONNECTION_GAME_IN_PROGRESS: i64 = 9;
const CONNECTION_GAME_IN_PROGRESS_AS_SENT: i64 = 0x2751;

const SLOT_SERVER: i64 = 0xfe;
const SLOT_NONE: i64 = 0xff;
const FORM_MAIN_GAME: i64 = 11;
const FORM_LOBBY: i64 = 26;
const FORM_AVAILABLE_GAMES: i64 = 32;

// the message types read here
const NE_CONNECTION_REQUEST: u8 = 0x22;
const NE_CONNECTION_RESPONSE: u8 = 0x23;
const NE_DISCONNECTION: u8 = 0x24;
const NE_COMMUNICATIONS_TICK: u8 = 0x26;
const NE_CONNECTION_CHECK: u8 = 0x29;
const NE_CONNECTION_CHECK_RESPONSE: u8 = 0x2a;
const NE_START_GAME: u8 = 0x2d;
const NE_RECONNECTION_REQUEST: u8 = 0x37;
const NE_RECONNECTION_RESPONSE: u8 = 0x38;
const NE_REQUEST_PAUSE: u8 = 0x39;
const NE_XFER_STATUS: u8 = 0x3c;
const MSG_EDICT_PROPOSAL: u8 = 0x41;
const MSG_EDICT_VOTE: u8 = 0x42;
const MSG_NEW_EDICT_ENACTED: u8 = 0x43;
const MSG_FULL_PLAYER_INFORMATION: u8 = 0x5b;
const MSG_GAME_OPTIONS: u8 = 0x60;
const MSG_TOGGLE_DAY_NIGHT: u8 = 0x62;
const MSG_SNAP_TO_DAYLIGHT: u8 = 0x63;
const MSG_LOAD_OR_SAVE: u8 = 0x64;
const MSG_PLAYER_SLOT_INFO: u8 = 0x6a;
const MSG_MIGRATION_SLOT_INFO: u8 = 0x7c;
const MSG_GAME_RESTARTING: u8 = 0x91;
const MSG_LOAD_MAIN_SCREEN_RESPONSE: u8 = 0x98;
const MSG_LOAD_OR_SAVE_RESPONSE: u8 = 0x9a;
const MSG_START_LOADING_RESPONSE: u8 = 0x5e;
const MSG_LAG_STATUS: u8 = 0xa8;
const MSG_PLAYER_INFORMATION: u8 = 0xaa;

/// The selection messages: they take a new selection id and run at once (`PrepareLocalInput`, `HandleUserInput`).
const SELECTION_TYPES: [u8; 11] = [0x48, 0x49, 0x4a, 0x4b, 0x4e, 0x4f, 0x50, 0x76, 0x82, 0x84, 0x96];

/// The host as the bodies see it (the game's: `GameHost`; the tests': a script).
pub trait SessHost {
    fn op(&mut self, op: u32, v: [i64; 8]) -> i64;
}

struct GameHost {
    host: EeNetSessHostFn,
    frame: *mut c_void,
}

impl SessHost for GameHost {
    fn op(&mut self, op: u32, v: [i64; 8]) -> i64 {
        let mut call = EeNetSessCall { frame: self.frame, v };
        // SAFETY: the host is the C++ one the forwarder passed, the frame its own
        unsafe { (self.host)(op, &mut call) }
    }
}

fn args(values: &[i64]) -> [i64; 8] {
    let mut v = [0i64; 8];
    v[..values.len()].copy_from_slice(values);
    v
}

/// The bodies' view of the host: one method per kind of operation.
struct S<'a, H: SessHost + ?Sized> {
    h: &'a mut H,
    server: bool,
}

impl<H: SessHost + ?Sized> S<'_, H> {
    fn call(&mut self, op: u32, values: &[i64]) -> i64 {
        self.h.op(op, args(values))
    }
    fn lock(&mut self, kind: i64, enter: bool) {
        self.call(EE_NETMSG_SESS_OP_LOCK, &[kind, enter as i64]);
    }
    fn fact(&mut self, kind: i64) -> i64 {
        self.call(EE_NETMSG_SESS_OP_FACT, &[kind])
    }
    fn fact1(&mut self, kind: i64, a: i64) -> i64 {
        self.call(EE_NETMSG_SESS_OP_FACT, &[kind, a])
    }
    fn get(&mut self, member: i64) -> i64 {
        self.call(EE_NETMSG_SESS_OP_SELF_GET, &[member, 0])
    }
    fn set(&mut self, member: i64, value: i64) {
        self.call(EE_NETMSG_SESS_OP_SELF_SET, &[member, 0, value]);
    }
    fn reg(&mut self, reg: i64, kind: i64, values: &[i64]) -> i64 {
        let mut v = vec![reg, kind];
        v.extend_from_slice(values);
        self.call(EE_NETMSG_SESS_OP_REG, &v)
    }
    fn reg_get(&mut self, reg: i64, field: i64) -> i64 {
        self.reg(reg, EE_NETMSG_SESS_REG_GET, &[field])
    }
    fn reg_set(&mut self, reg: i64, field: i64, value: i64) {
        self.reg(reg, EE_NETMSG_SESS_REG_SET, &[field, value]);
    }
    fn send(&mut self, values: &[i64]) -> i64 {
        self.call(EE_NETMSG_SESS_OP_SEND, values)
    }
    fn response(&mut self, slot: i64, error: i64, send_to: i64, ui: i64, unregister: bool) {
        self.send(&[EE_NETMSG_SESS_SEND_RESPONSE, slot, error, send_to, ui, unregister as i64]);
    }
    fn new_record(&mut self, message_type: u8, values: &[i64]) -> u64 {
        let mut v = vec![EE_NETMSG_SESS_SEND_NEW_RECORD, message_type as i64, values.len() as i64];
        v.extend_from_slice(values);
        self.send(&v) as u64
    }
    fn msg(&mut self, kind: i64, m: u64) -> i64 {
        self.call(EE_NETMSG_SESS_OP_MSG, &[kind, m as i64])
    }
    fn msg2(&mut self, kind: i64, m: u64, a: i64) -> i64 {
        self.call(EE_NETMSG_SESS_OP_MSG, &[kind, m as i64, a])
    }
    fn ne(&mut self, m: u64, index: i64) -> i64 {
        self.msg2(EE_NETMSG_SESS_MSG_NE_VALUE, m, index)
    }
    fn record(&mut self, m: u64) -> Record {
        let mut bytes = [0u8; crate::netmsg::records::EE_NETMSG_RECORD_BYTES];
        let message_type = self.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8;
        self.msg2(EE_NETMSG_SESS_MSG_RECORD, m, bytes.as_mut_ptr() as i64);
        Record { message_type, bytes }
    }
    fn ui(&mut self, values: &[i64]) {
        self.call(EE_NETMSG_SESS_OP_UI, values);
    }
    fn won(&mut self, values: &[i64]) {
        self.call(EE_NETMSG_SESS_OP_WON, values);
    }
    fn world(&mut self, values: &[i64]) -> i64 {
        self.call(EE_NETMSG_SESS_OP_WORLD, values)
    }
    fn comms(&mut self, values: &[i64]) -> i64 {
        self.call(EE_NETMSG_SESS_OP_COMMS, values)
    }
    fn start(&mut self, values: &[i64]) {
        self.call(EE_NETMSG_SESS_OP_START, values);
    }
    fn multiplayer(&mut self) -> bool {
        self.fact(EE_NETMSG_SESS_FACT_MULTIPLAYER_GAME) != 0
    }
    fn send_to(&mut self, lobby_slot: i64) -> i64 {
        self.fact1(EE_NETMSG_SESS_FACT_SEND_TO_SLOT, lobby_slot & 0xff)
    }
    fn game_state(&mut self) -> i64 {
        self.fact(EE_NETMSG_SESS_FACT_GAME_STATE)
    }
    fn won_present(&mut self) -> bool {
        self.fact(EE_NETMSG_SESS_FACT_WON) != 0
    }
    /// the slot's input filter: for every slot, or for the server's (the server) or this client's send-to slot
    fn for_me(&mut self, input: u64) -> bool {
        let slot = self.msg(EE_NETMSG_SESS_MSG_FOR_SLOT, input);
        if slot == SLOT_NONE {
            return true;
        }
        if self.server {
            slot == SLOT_SERVER
        } else {
            slot == self.fact(EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT)
        }
    }
}

/// A game message's record (`EEMRecord::mRecord`) read by member (1 = the first after the selection id).
struct Record {
    message_type: u8,
    bytes: [u8; crate::netmsg::records::EE_NETMSG_RECORD_BYTES],
}

impl Record {
    fn raw(&self, index: u32) -> u64 {
        let f = crate::netmsg::records::record_member(self.message_type, index);
        let mut v = [0u8; 8];
        let n = (f.size as usize).min(8);
        v[..n].copy_from_slice(&self.bytes[f.offset as usize..f.offset as usize + n]);
        u64::from_le_bytes(v)
    }
    fn i32(&self, index: u32) -> i64 {
        self.raw(index) as u32 as i32 as i64
    }
    fn u8(&self, index: u32) -> i64 {
        self.raw(index) as u8 as i64
    }
    fn u16(&self, index: u32) -> i64 {
        self.raw(index) as u16 as i64
    }
    fn bool(&self, index: u32) -> bool {
        self.raw(index) as u8 != 0
    }
    /// a float member's bits
    fn f32_bits(&self, index: u32) -> i64 {
        self.raw(index) as u32 as i64
    }
}

// ---- the server (EEServer.cpp) ----

/// `EEServer::ProcessConnectionRequest`: refuses a wrong version or checksum, a closed game, a name in use or a full
/// game; else gives the client a slot, sends it the options and players and starts the saved game's or scenario's
/// transfer (the join request's player is `EE_NETMSG_SESS_R_IN`).
fn connection_request<H: SessHost + ?Sized>(s: &mut S<H>, slot: i64, checksum: i64) {
    let mut ok = true;
    s.lock(EE_NETMSG_SESS_LOCK_CT, true);
    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, true);
    s.lock(EE_NETMSG_SESS_LOCK_PING, true);
    s.lock(EE_NETMSG_SESS_LOCK_SEARCH, true);
    if s.fact(EE_NETMSG_SESS_FACT_VERSION_MATCH) == 0 {
        s.response(slot, CONNECTION_VERSION_MISMATCH, slot, 0xff, true);
        ok = false;
    } else if checksum != s.fact(EE_NETMSG_SESS_FACT_BINARY_CHECKSUM) {
        s.response(slot, CONNECTION_BAD_CHECKSUM, slot, 0xff, true);
        ok = false;
    }
    if slot == SLOT_SERVER {
        s.comms(&[EE_NETMSG_SESS_COMMS_SEND_TO_CLEAR]);
        s.comms(&[EE_NETMSG_SESS_COMMS_LOCAL_OUT, 0]);
        let count = if s.fact(EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER) != 0 { 16 } else { 2 };
        s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, 3);
        if !s.multiplayer() {
            for i in 0..16i64 {
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[i]);
                let color = s.fact1(EE_NETMSG_SESS_FACT_DEFAULT_COLOR, i);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_COLOR, color);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, 0);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT, i);
                let citizens = s.fact(EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_CITIZENS, citizens);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_READY, 0);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_PING, 0);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_CLEAR, &[]);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TEAM, 0);
                if i + 1 > count || i >= 8 {
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, 4);
                } else if i == 1 && s.fact(EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER) == 0 {
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TEAM, 0);
                } else {
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, 3);
                }
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_OPTIONS, &[i]);
            }
        }
    }
    if ok && s.game_state() == 1 {
        s.response(slot, CONNECTION_GAME_IN_PROGRESS_AS_SENT, slot, 0xff, true);
        ok = false;
    }
    'done: {
        if !s.multiplayer() {
            if !ok {
                break 'done;
            }
            for i in 0..16i64 {
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[i]);
                let t = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE);
                if (t == 1 || t == 2)
                    && s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_ICMP, &[EE_NETMSG_SESS_R_IN]) != 0
                {
                    s.response(slot, CONNECTION_NAME_IN_USE, slot, 0xff, true);
                    break 'done;
                }
            }
            if s.fact(EE_NETMSG_SESS_FACT_LOAD_TYPE) == 2 {
                ok = false;
                for i in 0..16i64 {
                    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[i]);
                    if s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_EQ, &[EE_NETMSG_SESS_R_IN]) != 0
                        && s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE) == 3
                    {
                        ok = true;
                        break;
                    }
                }
                if !ok {
                    s.response(slot, CONNECTION_GAME_FULL, slot, 0xff, true);
                }
            }
        }
        if !ok {
            break 'done;
        }
        let mut index = (slot != SLOT_SERVER) as i64;
        while index < 16 {
            s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[index]);
            let t = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE);
            if s.multiplayer() {
                if t != 1 && t != 2 {
                    index += 1;
                    continue;
                }
            } else {
                if t != 3 {
                    index += 1;
                    continue;
                }
                if s.fact(EE_NETMSG_SESS_FACT_LOAD_TYPE) != 2 {
                    break;
                }
            }
            if s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_EQ, &[EE_NETMSG_SESS_R_IN]) != 0 {
                break;
            }
            index += 1;
        }
        if index >= 16 {
            s.response(slot, CONNECTION_GAME_FULL, slot, 0xff, true);
            break 'done;
        }
        s.response(slot, CONNECTION_ACCEPTED, slot, index, false);
        if !s.multiplayer() {
            let load_type = s.fact(EE_NETMSG_SESS_FACT_LOAD_TYPE);
            let loaded = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOADED_TYPE);
            if load_type == 0 {
                let t = s.reg_get(EE_NETMSG_SESS_R_IN, EE_NETMSG_SESS_FIELD_TYPE);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, t);
                let color = s.fact1(EE_NETMSG_SESS_FACT_DEFAULT_COLOR, index);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_COLOR, color);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_COPY, &[EE_NETMSG_SESS_R_IN]);
                for f in [EE_NETMSG_SESS_FIELD_TEAM, EE_NETMSG_SESS_FIELD_PING, EE_NETMSG_SESS_FIELD_READY] {
                    let v = s.reg_get(EE_NETMSG_SESS_R_IN, f);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, f, v);
                }
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT, index);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, (index == 0) as i64);
            } else if load_type == 2 && (loaded == 1 || loaded == 2) {
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, loaded);
            } else {
                let t = s.reg_get(EE_NETMSG_SESS_R_IN, EE_NETMSG_SESS_FIELD_TYPE);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, t);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_COPY, &[EE_NETMSG_SESS_R_IN]);
            }
        } else {
            s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_READY, 1);
        }
        s.comms(&[EE_NETMSG_SESS_COMMS_SEND_TO_INSERT, index, slot]);
        s.fact(EE_NETMSG_SESS_FACT_CURRENT_CT);
        s.send(&[EE_NETMSG_SESS_SEND_FULL_OPTIONS, slot]);
        for i in 0..16i64 {
            if i != index {
                s.reg(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_REG_LOAD_LOBBY, &[i]);
            } else {
                s.reg(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_REG_LOAD_REG, &[EE_NETMSG_SESS_R_PLAYER]);
            }
            s.fact(EE_NETMSG_SESS_FACT_CURRENT_CT);
            s.send(&[EE_NETMSG_SESS_SEND_PLAYER_INFO, EE_NETMSG_SESS_R_OTHER, slot, 0]);
            let other_slot = s.reg_get(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_FIELD_SLOT);
            s.ui(&[EE_NETMSG_SESS_UI_LOBBY_PLAYER, other_slot]);
        }
        for i in 0..16i64 {
            let send_slot = s.send_to(i);
            if send_slot != SLOT_NONE {
                s.send(&[EE_NETMSG_SESS_SEND_PLAYER_INFO, EE_NETMSG_SESS_R_PLAYER, send_slot, 0]);
                let player_slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
                s.ui(&[EE_NETMSG_SESS_UI_LOBBY_PLAYER, player_slot]);
            }
        }
        let player_slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
        let send_slot = s.send_to(player_slot);
        s.comms(&[EE_NETMSG_SESS_COMMS_REGISTER, send_slot]);
        s.send(&[EE_NETMSG_SESS_SEND_LOCAL_RESPONSE, CONNECTION_PLAYER_JOINED, slot, index, slot]);
        let load_type = s.fact(EE_NETMSG_SESS_FACT_LOAD_TYPE);
        if load_type == 2 || load_type == 1 {
            s.comms(&[EE_NETMSG_SESS_COMMS_XFER_GAME_FILE, load_type, player_slot]);
        }
        let player_slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
        s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_LOBBY, &[player_slot]);
        won_count(s, EE_NETMSG_SESS_REG_LOAD_LOBBY);
    }
    s.lock(EE_NETMSG_SESS_LOCK_SEARCH, false);
    s.lock(EE_NETMSG_SESS_LOCK_PING, false);
    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, false);
    s.lock(EE_NETMSG_SESS_LOCK_CT, false);
}

/// WON's game info: the slots that hold a player (open computer or human) and the humans, of the lobby's or the
/// options' slots (`EE_NETMSG_SESS_R_PLAYER` is overwritten, as `thePlayer` was)
fn won_count<H: SessHost + ?Sized>(s: &mut S<H>, source: i64) {
    if !s.won_present() {
        return;
    }
    let mut players = 0i64;
    let mut humans = 0i64;
    for i in 0..16i64 {
        s.reg(EE_NETMSG_SESS_R_PLAYER, source, &[i]);
        let t = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE);
        if t == 3 || t == 1 || t == 2 {
            players += 1;
        }
        if t == 1 || t == 2 {
            humans += 1;
        }
    }
    s.won(&[EE_NETMSG_SESS_WON_COUNT, players, humans]);
}

/// `EEServer::BootPlayer`: in a running game drops the player; the host leaving tells every client the host is gone;
/// in the lobby reopens the slot, tells the client why and stands in for its load response.
fn boot<H: SessHost + ?Sized>(s: &mut S<H>, slot: i64, booted: bool, exiting: bool) {
    let in_game = s.game_state() != 2
        && (s.game_state() != 0 || s.get(EE_NETMSG_SESS_SELF_MIGRATING) != 0)
        && s.game_state() != 1;
    if in_game {
        s.comms(&[EE_NETMSG_SESS_COMMS_REMOVE_IN_PROGRESS, slot, booted as i64, exiting as i64]);
        if s.won_present() {
            s.won(&[EE_NETMSG_SESS_WON_HUMANS_DEC]);
        }
    } else if slot == SLOT_SERVER {
        for i in 1..16i64 {
            let send_slot = s.send_to(i);
            if send_slot == SLOT_NONE {
                continue;
            }
            s.response(send_slot, CONNECTION_HOST_GONE, send_slot, 0xff, true);
        }
        let error = if exiting { CONNECTION_GAME_EXITING } else { CONNECTION_REMOVED_BY_HOST };
        s.send(&[EE_NETMSG_SESS_SEND_LOCAL_RESPONSE, error, SLOT_SERVER, 0xff, SLOT_SERVER]);
    } else {
        s.lock(EE_NETMSG_SESS_LOCK_ATTEMPT, true);
        for i in 0..16i64 {
            if s.send_to(i) != slot {
                continue;
            }
            if s.game_state() == 0 {
                s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, true);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[i]);
                let xfer = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TRANSFER_ID);
                s.comms(&[EE_NETMSG_SESS_COMMS_ABORT_XFER, xfer]);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_READY, 0);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, 0);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TRANSFER_ID, 0);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, 3);
                if s.fact(EE_NETMSG_SESS_FACT_LOAD_TYPE) == 0 {
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOADED_TYPE, 3);
                    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_CLEAR, &[]);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_PING, 0);
                    let color = s.fact1(EE_NETMSG_SESS_FACT_DEFAULT_COLOR, i);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_COLOR, color);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TEAM, 0);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_B36, 0);
                    let citizens = s.fact(EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_CITIZENS, citizens);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_ENGINE, 0);
                    // port: kEECPChoiceClassic
                }
                s.send(&[EE_NETMSG_SESS_SEND_PLAYER_INFO, EE_NETMSG_SESS_R_PLAYER, SLOT_NONE, 1]);
                let player_slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
                s.ui(&[EE_NETMSG_SESS_UI_LOBBY_PLAYER, player_slot]);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_OPTIONS, &[player_slot]);
                s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, false);
            }
            if exiting {
                s.response(slot, CONNECTION_GAME_EXITING, slot, 0xff, false);
            } else if booted {
                s.response(slot, CONNECTION_REMOVED_BY_HOST, slot, 0xff, false);
            }
            if s.game_state() == 1 || s.game_state() == 2 {
                let m = match s.get(EE_NETMSG_SESS_SELF_LOAD_STATE) {
                    2 => s.new_record(MSG_LOAD_MAIN_SCREEN_RESPONSE, &[slot, 0xff]),
                    1 => s.new_record(MSG_START_LOADING_RESPONSE, &[slot, 0xff, 0]),
                    _ => 0,
                };
                if m != 0 {
                    s.msg2(EE_NETMSG_SESS_MSG_LOCAL, m, SLOT_SERVER);
                    let dropped = s.get(EE_NETMSG_SESS_SELF_DROPPED_LOADERS);
                    s.set(EE_NETMSG_SESS_SELF_DROPPED_LOADERS, (dropped + 1) & 0xff);
                }
            }
            s.comms(&[EE_NETMSG_SESS_COMMS_UNREGISTER, slot]);
            s.comms(&[EE_NETMSG_SESS_COMMS_SEND_TO_ERASE, i]);
            if s.won_present() {
                s.won(&[EE_NETMSG_SESS_WON_HUMANS_DEC]);
            }
            break;
        }
        s.lock(EE_NETMSG_SESS_LOCK_ATTEMPT, false);
    }
}

/// `EEServer::ExecuteRemovePlayer`: ours shows the booted or out-of-sync box and ends the game, another's is
/// announced in the chat log, unregistered and removed; the matching sound.
fn server_remove<H: SessHost + ?Sized>(s: &mut S<H>, player: i64, booted: bool, oos: bool, exiting: bool) {
    if s.get(EE_NETMSG_SESS_SELF_HAS_LEFT) != 0 {
        return;
    }
    if player == SLOT_SERVER {
        s.set(EE_NETMSG_SESS_SELF_HAS_LEFT, 1);
        if exiting {
            s.ui(&[EE_NETMSG_SESS_UI_CANCEL_CALLBACK]);
        } else {
            let text = if oos { IDS_YOU_ARE_OUT_OF_SYNCH } else { IDS_YOU_HAVE_BEEN_BOOTED_DUE };
            s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, text, 1, 1]);
        }
    } else {
        let text = if oos { IDS_PLAYER_IS_OUT_OF_SYNCH } else { IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME };
        let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, player]);
        s.ui(&[EE_NETMSG_SESS_UI_CHAT_PLAYER, text, p]);
        if !booted {
            s.lock(EE_NETMSG_SESS_LOCK_CT, true);
            let send_slot = s.fact1(EE_NETMSG_SESS_FACT_PLAYER_SEND_TO, player);
            s.comms(&[EE_NETMSG_SESS_COMMS_UNREGISTER, send_slot]);
            s.lock(EE_NETMSG_SESS_LOCK_CT, false);
        }
        s.world(&[EE_NETMSG_SESS_WORLD_REMOVE, player]);
    }
    if !booted {
        let humans = s.fact(EE_NETMSG_SESS_FACT_HUMANS);
        s.comms(&[EE_NETMSG_SESS_COMMS_SET_HUMANS, (humans - 1) & 0xff]);
    }
    if !exiting {
        s.call(EE_NETMSG_SESS_OP_SOUND, &[0, oos as i64]);
    }
}

/// The connection quality of a ping's answer time (ms; half the round trip)
pub fn ping_quality(ping: u32) -> f32 {
    if ping > 750 {
        0.0
    } else if ping > 500 {
        0.2
    } else if ping > 250 {
        0.4
    } else if ping > 150 {
        0.6
    } else if ping > 50 {
        0.8
    } else {
        1.0
    }
}

/// The communications tick (the server's `ProcessSystemMessage`, the client's `HandleMessage`): its inputs for this
/// side to the physics with a physics tick, or to the lobby before the game (ticks to perform 0xff).
fn communications_tick<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    let count = s.msg(EE_NETMSG_SESS_MSG_INPUT_COUNT, m);
    let ticks = s.ne(m, 1) & 0xff;
    let mut inputs = 0i64;
    for i in 0..count {
        let input = s.msg2(EE_NETMSG_SESS_MSG_INPUT, m, i) as u64;
        if s.for_me(input) {
            inputs += 1;
        }
    }
    if ticks != 0xff {
        if !s.server {
            let pt = s.fact(EE_NETMSG_SESS_FACT_CURRENT_PT);
            s.comms(&[EE_NETMSG_SESS_COMMS_SET_PT, (pt as u64).wrapping_add(ticks as u64) as i64]);
        }
        let checksum = (s.ne(m, 3) != 0) as i64;
        let dump = (s.ne(m, 4) != 0) as i64;
        let rate = s.ne(m, 2) & 0xff;
        let tick = s.send(&[EE_NETMSG_SESS_SEND_PHYSICS_TICK, inputs, ticks, checksum, dump, rate]) as u64;
        s.lock(EE_NETMSG_SESS_LOCK_PHYSICS, true);
        s.msg(EE_NETMSG_SESS_MSG_QUEUE, tick);
        for i in 0..count {
            let input = s.msg2(EE_NETMSG_SESS_MSG_INPUT, m, i) as u64;
            if s.for_me(input) {
                if s.server {
                    s.msg(EE_NETMSG_SESS_MSG_QUEUE, input);
                    s.msg(EE_NETMSG_SESS_MSG_REFERENCE, input);
                } else {
                    s.msg(EE_NETMSG_SESS_MSG_REFERENCE, input);
                    s.msg(EE_NETMSG_SESS_MSG_QUEUE, input);
                }
            }
        }
        s.lock(EE_NETMSG_SESS_LOCK_PHYSICS, false);
    } else {
        s.lock(EE_NETMSG_SESS_LOCK_PHYSICS, true);
        if s.server {
            s.comms(&[EE_NETMSG_SESS_COMMS_INCREMENT_CTS]);
            s.lock(EE_NETMSG_SESS_LOCK_PHYSICS, false);
            s.fact(EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS); // the original reads it and drops it
        } else {
            s.fact(EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS);
            s.comms(&[EE_NETMSG_SESS_COMMS_INCREMENT_CTS]);
            s.lock(EE_NETMSG_SESS_LOCK_PHYSICS, false);
        }
        for i in 0..count {
            let input = s.msg2(EE_NETMSG_SESS_MSG_INPUT, m, i) as u64;
            if s.for_me(input) {
                lobby_input(s, input);
            }
        }
    }
}

/// `EEServer::ProcessSystemMessage`: ping answers (sent on as quality stats), communications ticks, join requests,
/// disconnections (the player removed), reconnections; load/save and lag messages execute.
fn system_message<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    match s.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8 {
        NE_CONNECTION_CHECK_RESPONSE => {
            s.lock(EE_NETMSG_SESS_LOCK_SEARCH, true);
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            let mut i = 0i64;
            while i < 16 {
                if s.send_to(i) == origin {
                    break;
                }
                i += 1;
            }
            if i < 16 {
                let start = s.ne(m, 0) as u32;
                let ping = (s.fact(EE_NETMSG_SESS_FACT_TIME) as u32).wrapping_sub(start) / 2;
                let quality = ping_quality(ping);
                s.send(&[EE_NETMSG_SESS_SEND_PING_STAT, i, quality.to_bits() as i64]);
            }
            s.lock(EE_NETMSG_SESS_LOCK_SEARCH, false);
        }
        NE_COMMUNICATIONS_TICK => communications_tick(s, m),
        NE_CONNECTION_REQUEST => {
            s.msg(EE_NETMSG_SESS_MSG_TAKE_REQUEST, m);
        }
        NE_DISCONNECTION => {
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            let exiting = (s.ne(m, 0) != 0) as i64;
            s.ui(&[EE_NETMSG_SESS_UI_ACTION_REMOVE, origin, exiting]);
            s.msg(EE_NETMSG_SESS_MSG_DEREFERENCE, m);
        }
        NE_RECONNECTION_REQUEST => {
            let player = s.ne(m, 1);
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            s.world(&[EE_NETMSG_SESS_WORLD_RECONNECT, player, origin]);
        }
        MSG_NEW_EDICT_ENACTED => {}
        MSG_LOAD_OR_SAVE | MSG_LAG_STATUS => {
            s.msg(EE_NETMSG_SESS_MSG_EXECUTE, m);
        }
        _ => {}
    }
}

/// `EEServer::FilterPlayerInput`: takes votes, pause and join requests; when the host itself is removed it turns
/// into a client. Whether to pass the message on.
fn filter_input<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) -> bool {
    match s.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8 {
        MSG_LOAD_OR_SAVE_RESPONSE => {
            s.msg(EE_NETMSG_SESS_MSG_LOG_RESPONSE, m);
            s.msg(EE_NETMSG_SESS_MSG_DEREFERENCE, m);
            false
        }
        MSG_EDICT_VOTE => {
            let r = s.record(m);
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            cast_vote(s, origin, r.bool(2), r.u8(3));
            false
        }
        MSG_EDICT_PROPOSAL => {
            let r = s.record(m);
            start_vote(s, r.u16(1))
        }
        NE_REQUEST_PAUSE => {
            s.ui(&[EE_NETMSG_SESS_UI_PAUSE_EVENT]);
            s.msg(EE_NETMSG_SESS_MSG_DEREFERENCE, m);
            false
        }
        NE_CONNECTION_RESPONSE => {
            if s.msg(EE_NETMSG_SESS_MSG_FOR_SLOT, m) != SLOT_SERVER {
                return true;
            }
            let error = s.ne(m, 0) as u32 as i32 as i64;
            if error == CONNECTION_REMOVED_BY_HOST || error == CONNECTION_GAME_EXITING {
                // the server is deleted here: nothing of it is touched after
                s.comms(&[EE_NETMSG_SESS_COMMS_BECOME_CLIENT]);
                false
            } else if error == CONNECTION_PLAYER_JOINED {
                if !s.multiplayer() {
                    s.ui(&[EE_NETMSG_SESS_UI_PUSH_FORM, FORM_LOBBY]);
                }
                true
            } else {
                false
            }
        }
        NE_CONNECTION_REQUEST => {
            s.msg(EE_NETMSG_SESS_MSG_TAKE_REQUEST, m);
            false
        }
        _ => true,
    }
}

/// `EEServer::PrepareLocalInput`, `EEClient::HandleUserInput`: stamps the local input with the fast-selection
/// number; a selection message takes a new number and runs at once.
fn local_input<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    let message_type = s.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8;
    if SELECTION_TYPES.contains(&message_type) {
        if s.get(EE_NETMSG_SESS_SELF_MIGRATING) == 0 {
            s.lock(EE_NETMSG_SESS_LOCK_FAST, true);
            let id = (s.get(EE_NETMSG_SESS_SELF_FAST_ID) + 1) & 0xff;
            s.set(EE_NETMSG_SESS_SELF_FAST_ID, id);
            s.msg2(EE_NETMSG_SESS_MSG_SET_FAST_ID, m, id);
            s.set(EE_NETMSG_SESS_SELF_EXECUTING, 1);
            s.world(&[EE_NETMSG_SESS_WORLD_EXECUTE_SELECTION, m as i64]);
            s.set(EE_NETMSG_SESS_SELF_EXECUTING, 0);
            s.lock(EE_NETMSG_SESS_LOCK_FAST, false);
        }
    } else if s.msg(EE_NETMSG_SESS_MSG_IS_GAME, m) != 0 {
        s.lock(EE_NETMSG_SESS_LOCK_FAST, true);
        let id = s.get(EE_NETMSG_SESS_SELF_FAST_ID);
        s.msg2(EE_NETMSG_SESS_MSG_SET_FAST_ID, m, id);
        s.lock(EE_NETMSG_SESS_LOCK_FAST, false);
    }
}

/// a lobby slot's full player information into `EE_NETMSG_SESS_R_PLAYER` (the record's members: slot 1, type 2, loaded type 3,
/// the name, ping 5, colour 6, team 7, citizens 8, ready 9, local 10, mB36 11, engine 12), the server's order
fn player_from_record<H: SessHost + ?Sized>(s: &mut S<H>, m: u64, r: &Record) {
    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_COLOR, r.i32(6));
    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE, r.i32(2));
    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TEAM, r.i32(7));
    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_FROM_MSG, &[m as i64]);
}

/// The game start (the start game message): the server starts its loop and pauses outside the game screen; the
/// client starts its own; humans without a slot are dropped (the server's only in a multiplayer session).
fn start_game<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    s.start(&[EE_NETMSG_SESS_START_PREP, m as i64]);
    if s.server {
        let humans = s.fact(EE_NETMSG_SESS_FACT_HUMANS);
        s.comms(&[EE_NETMSG_SESS_COMMS_SET_MP_SESSION, (humans != 1) as i64]);
        s.start(&[EE_NETMSG_SESS_START_RESUME]);
        s.comms(&[EE_NETMSG_SESS_COMMS_START_GAME]);
        if s.fact(EE_NETMSG_SESS_FACT_SCREEN_ACTIVE) == 0 {
            s.comms(&[EE_NETMSG_SESS_COMMS_PAUSE_GAME]);
        } else {
            s.start(&[EE_NETMSG_SESS_START_PHYSICS]);
        }
        if s.fact(EE_NETMSG_SESS_FACT_MP_SESSION) != 0 {
            drop_slotless(s);
        }
    } else {
        s.comms(&[EE_NETMSG_SESS_COMMS_SET_MP_SESSION, 1]);
        client_start_game(s);
    }
}

/// the humans without a slot dropped, each announced
fn drop_slotless<H: SessHost + ?Sized>(s: &mut S<H>) {
    let mut k = 1i64;
    while k < s.fact(EE_NETMSG_SESS_FACT_WORLD_PLAYERS) {
        let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, k]);
        if s.world(&[EE_NETMSG_SESS_WORLD_IS_HUMAN, p]) == 0 || s.world(&[EE_NETMSG_SESS_WORLD_SLOT, p]) != SLOT_NONE {
            k += 1;
            continue;
        }
        s.ui(&[EE_NETMSG_SESS_UI_CHAT_PLAYER, IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME, p]);
        s.world(&[EE_NETMSG_SESS_WORLD_REMOVE, k]);
        k += 1;
    }
}

/// `EEClient::HandleStartGame`: the graphics resumed, the physics loop started, the main game UI, the humans without
/// a slot dropped.
fn client_start_game<H: SessHost + ?Sized>(s: &mut S<H>) {
    s.start(&[EE_NETMSG_SESS_START_RESUME]);
    s.start(&[EE_NETMSG_SESS_START_PHYSICS]);
    drop_slotless(s);
}

/// The player slot info (the player, its send-to slot, its firewall flag, its address) applied to the world's player
fn slot_info<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    let r = s.record(m);
    s.lock(EE_NETMSG_SESS_LOCK_WORLD_WRITE, true);
    let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, r.u8(1)]);
    if p != 0 {
        s.world(&[EE_NETMSG_SESS_WORLD_SET_SLOT, p, r.u8(2)]);
        if s.world(&[EE_NETMSG_SESS_WORLD_IS_HUMAN, p]) != 0 {
            s.world(&[EE_NETMSG_SESS_WORLD_NAME_FROM_MSG, p, m as i64]);
            s.world(&[EE_NETMSG_SESS_WORLD_SET_FIREWALL, p, r.bool(3) as i64]);
        }
    }
    s.lock(EE_NETMSG_SESS_LOCK_WORLD_WRITE, false);
}

/// `EEServer::HandleLobbyInput`, `EEClient::HandleLobbyInput`: player info (the server also refreshes WON's game
/// info), transfer status, the game start, a connection check (the client answers), a joined player, a player's slot
/// info; the options and player info from others and the rest execute.
fn lobby_input<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    let message_type = s.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8;
    match message_type {
        MSG_FULL_PLAYER_INFORMATION => {
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            if s.server {
                if origin != SLOT_SERVER {
                    let r = s.record(m);
                    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, true);
                    player_from_record(s, m, &r);
                    let ping = r.f32_bits(5);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_CITIZENS, r.i32(8));
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOADED_TYPE, r.i32(3));
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_READY, r.bool(9) as i64);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, r.bool(10) as i64);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_B36, r.bool(11) as i64);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT, r.i32(1));
                    let slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
                    s.reg(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[slot]);
                    let xfer = s.reg_get(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_FIELD_TRANSFER_ID);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TRANSFER_ID, xfer);
                    let engine = s.reg_get(EE_NETMSG_SESS_R_OTHER, EE_NETMSG_SESS_FIELD_ENGINE); // port: the host's choice
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_ENGINE, engine);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_PING, ping);
                    let local = !(slot != 0 && s.send_to(slot) != SLOT_SERVER);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, local as i64);
                    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_LOBBY, &[slot]);
                    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, false);
                    s.ui(&[EE_NETMSG_SESS_UI_LOBBY_PLAYER, slot]);
                }
                won_count(s, EE_NETMSG_SESS_REG_LOAD_OPTIONS);
            } else if origin != s.fact(EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT) {
                let r = s.record(m);
                player_from_record(s, m, &r);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_PING, r.f32_bits(5));
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOADED_TYPE, r.i32(3));
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_CITIZENS, r.i32(8));
                let engine = r.i32(12);
                let choices = s.fact(EE_NETMSG_SESS_FACT_ENGINE_CHOICES);
                let engine = if (engine as u64) < (choices as u64) { engine } else { 0 }; // port: kEECPChoiceClassic
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_ENGINE, engine);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_B36, r.bool(11) as i64);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_READY, r.bool(9) as i64);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT, r.i32(1));
                let ui_slot = s.get(EE_NETMSG_SESS_SELF_UI_SLOT);
                s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_LOCAL, (r.i32(1) == ui_slot) as i64);
                s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, true);
                let slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_OPTIONS, &[slot]);
                s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, false);
                s.ui(&[EE_NETMSG_SESS_UI_SLOT_UPDATE, r.i32(1) & 0xff, EE_NETMSG_SESS_R_PLAYER]);
            }
        }
        NE_XFER_STATUS => {
            let slot = s.ne(m, 0) & 0xff;
            if s.server {
                let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
                if origin != SLOT_SERVER && s.ne(m, 3) & 0xff == 3 {
                    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, true);
                    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[slot]);
                    let mut xfer = s.ne(m, 2) as u32 as i64;
                    if xfer == 0xffff {
                        xfer = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TRANSFER_ID);
                    }
                    s.comms(&[EE_NETMSG_SESS_COMMS_ABORT_XFER, xfer]);
                    s.reg_set(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TRANSFER_ID, 0);
                    let player_slot = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_SLOT);
                    s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_STORE_OPTIONS, &[player_slot]);
                    s.lock(EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, false);
                }
                if s.ne(m, 3) & 0xff == 4 {
                    s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, IDS_FILE_TRANSFER_FAILED, 0, 1]);
                }
                let percent = s.ne(m, 1) & 0xff;
                s.ui(&[EE_NETMSG_SESS_UI_TRANSFER, slot, percent]);
            } else {
                let percent = s.ne(m, 1) & 0xff;
                s.ui(&[EE_NETMSG_SESS_UI_TRANSFER, slot, percent]);
                if s.ne(m, 3) & 0xff == 4 {
                    s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, IDS_FILE_TRANSFER_FAILED, 0, 1]);
                }
            }
        }
        NE_START_GAME => start_game(s, m),
        NE_CONNECTION_CHECK if !s.server => {
            let start = s.ne(m, 0);
            s.send(&[EE_NETMSG_SESS_SEND_CHECK_RESPONSE, start]);
        }
        NE_CONNECTION_RESPONSE => {
            if s.ne(m, 0) as u32 as i32 as i64 == CONNECTION_PLAYER_JOINED {
                s.ui(&[EE_NETMSG_SESS_UI_LOBBY_FORM]);
                if s.won_present() {
                    s.won(&[EE_NETMSG_SESS_WON_JOINED]);
                }
            }
        }
        MSG_PLAYER_INFORMATION | MSG_GAME_OPTIONS => {
            if s.server && message_type == MSG_PLAYER_INFORMATION {
                won_count(s, EE_NETMSG_SESS_REG_LOAD_OPTIONS);
            }
            let origin = s.msg(EE_NETMSG_SESS_MSG_ORIGIN, m);
            let mine = if s.server { SLOT_SERVER } else { s.fact(EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT) };
            if origin != mine {
                s.msg(EE_NETMSG_SESS_MSG_EXECUTE, m);
            }
        }
        MSG_PLAYER_SLOT_INFO => slot_info(s, m),
        _ => {
            s.msg(EE_NETMSG_SESS_MSG_EXECUTE, m);
        }
    }
}

/// `EEServer::StartVote`: a running vote is first closed with yes votes for those who have not voted; the votes
/// reset for the edict. True.
fn start_vote<H: SessHost + ?Sized>(s: &mut S<H>, edict: i64) -> bool {
    let mut index: u8 = 0;
    if s.get(EE_NETMSG_SESS_SELF_VOTE_EDICT) != 0 {
        let mut i = s.get(EE_NETMSG_SESS_SELF_NUM_VOTES) as u8;
        while (s.fact(EE_NETMSG_SESS_FACT_WORLD_PLAYERS) as u32).wrapping_sub(1) > i as u32 {
            let mut j = index.wrapping_add(1) as u16;
            while j < 0x100 {
                if s.call(EE_NETMSG_SESS_OP_SELF_GET, &[EE_NETMSG_SESS_SELF_VOTED, j as i64]) != 0 {
                    j += 1;
                    continue;
                }
                index = j as u8;
                cast_vote(s, index as i64, true, 0);
                i = i.wrapping_add(1);
                break;
            }
        }
    }
    s.set(EE_NETMSG_SESS_SELF_VOTE_EDICT, edict & 0xffff);
    s.set(EE_NETMSG_SESS_SELF_NUM_VOTES, 0);
    s.set(EE_NETMSG_SESS_SELF_NO_VOTES, 0);
    s.set(EE_NETMSG_SESS_SELF_YES_VOTES, 0);
    s.call(EE_NETMSG_SESS_OP_SELF_SET, &[EE_NETMSG_SESS_SELF_VOTED, -1, 0]);
    true
}

/// The vote's result: enacted on a yes majority (no votes at all and ties fail)
pub fn vote_passes(yes: u16, no: u16) -> bool {
    !((no == 0 && yes == 0) || yes == no) && yes > no
}

/// `EEServer::CastVote`: a slot's vote on the edict; once all have voted the result is broadcast and the vote reset.
fn cast_vote<H: SessHost + ?Sized>(s: &mut S<H>, index: i64, yes: bool, count: i64) {
    let index = index & 0xff;
    if s.call(EE_NETMSG_SESS_OP_SELF_GET, &[EE_NETMSG_SESS_SELF_VOTED, index]) != 0 {
        return;
    }
    let votes = (s.get(EE_NETMSG_SESS_SELF_NUM_VOTES) + 1) & 0xff;
    s.set(EE_NETMSG_SESS_SELF_NUM_VOTES, votes);
    s.call(EE_NETMSG_SESS_OP_SELF_SET, &[EE_NETMSG_SESS_SELF_VOTED, index, 1]);
    if count > 0 {
        let member = if yes { EE_NETMSG_SESS_SELF_YES_VOTES } else { EE_NETMSG_SESS_SELF_NO_VOTES };
        let v = s.get(member);
        s.set(member, (v + count) & 0xffff);
    }
    if votes != s.fact(EE_NETMSG_SESS_FACT_NUM_PLAYERS) {
        return;
    }
    let yes_votes = s.get(EE_NETMSG_SESS_SELF_YES_VOTES);
    let no_votes = s.get(EE_NETMSG_SESS_SELF_NO_VOTES);
    let edict = s.get(EE_NETMSG_SESS_SELF_VOTE_EDICT);
    let passes = vote_passes(yes_votes as u16, no_votes as u16) as i64;
    let m = s.new_record(MSG_NEW_EDICT_ENACTED, &[edict, yes_votes, no_votes, passes]);
    s.msg(EE_NETMSG_SESS_MSG_BROADCAST, m);
    system_message(s, m);
    s.msg(EE_NETMSG_SESS_MSG_DEREFERENCE, m);
    s.set(EE_NETMSG_SESS_SELF_NUM_VOTES, 0);
    s.set(EE_NETMSG_SESS_SELF_YES_VOTES, 0);
    s.set(EE_NETMSG_SESS_SELF_NO_VOTES, 0);
    s.set(EE_NETMSG_SESS_SELF_VOTE_EDICT, 0);
}

/// `EEServer::HandleReconnectWaitTimedOut`: the game restarts with this host: the humans' migration slots, the new
/// host announced, the restart.
fn reconnect_wait_timed_out<H: SessHost + ?Sized>(s: &mut S<H>) {
    let ct = s.fact(EE_NETMSG_SESS_FACT_CURRENT_CT);
    s.comms(&[EE_NETMSG_SESS_COMMS_LOWEST_CTS, (ct as u32).wrapping_sub(1) as i64]);
    let mut i = 1i64;
    while i < s.fact(EE_NETMSG_SESS_FACT_WORLD_PLAYERS) {
        let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, i]);
        if s.world(&[EE_NETMSG_SESS_WORLD_HAS_HUMAN, p]) != 0 {
            let id = s.world(&[EE_NETMSG_SESS_WORLD_ID, p]);
            let slot = s.world(&[EE_NETMSG_SESS_WORLD_SLOT, p]);
            let m = s.new_record(MSG_MIGRATION_SLOT_INFO, &[id, slot]);
            s.msg2(EE_NETMSG_SESS_MSG_LOCAL, m, SLOT_NONE);
        }
        i += 1;
    }
    let humans = s.fact(EE_NETMSG_SESS_FACT_HUMANS);
    s.comms(&[EE_NETMSG_SESS_COMMS_SET_MP_SESSION, (humans != 1) as i64]);
    s.ui(&[EE_NETMSG_SESS_UI_CHAT_TEXT, IDS_NEW_HOST_ESTABLISHED, 0]);
    s.ui(&[EE_NETMSG_SESS_UI_HOST_MIGRATED]);
    let m = s.new_record(MSG_GAME_RESTARTING, &[]);
    s.msg2(EE_NETMSG_SESS_MSG_LOCAL, m, SLOT_NONE);
    s.comms(&[EE_NETMSG_SESS_COMMS_START_GAME]);
}

/// A lagging player's bit (`(unsigned char)(1 << ((unsigned char)id - 1))`: the shift count as x86 masks it)
pub fn lag_bit(player_id: i64) -> u16 {
    let shift = (player_id as u8) as i32 - 1;
    1u32.wrapping_shl(shift as u32) as u8 as u16
}

/// `EEServer::ReportLaggers`: in multiplayer on the main game form the lagging players as a bit mask (when it changed
/// or forced), else in the lobby their count.
fn report_laggers<H: SessHost + ?Sized>(s: &mut S<H>, slots: &[u8], flag: bool) {
    let mut mask: u16 = 0;
    if s.fact(EE_NETMSG_SESS_FACT_MP_SESSION) != 0 {
        if s.call(EE_NETMSG_SESS_OP_FACT, &[EE_NETMSG_SESS_FACT_FORM_IS_CURRENT, FORM_MAIN_GAME, 0]) != 0 {
            for &slot in slots {
                let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT, slot as i64]);
                if p != 0 {
                    let id = s.world(&[EE_NETMSG_SESS_WORLD_ID, p]);
                    mask |= lag_bit(id);
                }
            }
            if mask as i64 != s.get(EE_NETMSG_SESS_SELF_LAG_MASK) || flag {
                s.set(EE_NETMSG_SESS_SELF_LAG_MASK, mask as i64);
                let m = s.new_record(MSG_LAG_STATUS, &[mask as i64, flag as i64]);
                s.msg(EE_NETMSG_SESS_MSG_SEND_ALL, m);
            }
        }
    } else if s.game_state() == 0 || s.game_state() == 1 {
        let m = s.new_record(MSG_LAG_STATUS, &[slots.len() as i64, flag as i64]);
        s.msg(EE_NETMSG_SESS_MSG_SEND_ALL, m);
    }
}

/// `EEServer::SendPlayerSlotInfo`: every human player's network slot, address and firewall flag, when more than one
/// human is left.
fn send_slot_info<H: SessHost + ?Sized>(s: &mut S<H>) {
    let humans = s.fact(EE_NETMSG_SESS_FACT_HUMANS);
    let dropped = s.get(EE_NETMSG_SESS_SELF_DROPPED_LOADERS);
    if humans - dropped <= 1 {
        return;
    }
    let mut index: u8 = 1;
    s.comms(&[EE_NETMSG_SESS_COMMS_PROTOCOL]);
    let mut i = 1i64;
    while i < s.fact(EE_NETMSG_SESS_FACT_WORLD_PLAYERS) {
        let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, i]);
        if s.world(&[EE_NETMSG_SESS_WORLD_HAS_HUMAN, p]) != 0 {
            for j in 0..16i64 {
                s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_LOAD_OPTIONS, &[j]);
                if s.reg(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_REG_NAME_ICMP_PLAYER, &[p]) == 0 {
                    continue;
                }
                let t = s.reg_get(EE_NETMSG_SESS_R_PLAYER, EE_NETMSG_SESS_FIELD_TYPE);
                if t != 1 && t != 2 {
                    continue;
                }
                let send_slot = s.send_to(j);
                s.send(&[EE_NETMSG_SESS_SEND_SLOT_INFO, index as i64, send_slot]);
                break;
            }
        }
        index = index.wrapping_add(1);
        i += 1;
    }
}

// ---- the client (EEClient.cpp) ----

/// `EEClient::SetConnectionState`: 0 could not connect (back to the game list), 1 connecting, 4 connecting with
/// retries.
fn connection_state<H: SessHost + ?Sized>(s: &mut S<H>, state: i64) {
    match state {
        0 => {
            if s.multiplayer() {
                s.ui(&[EE_NETMSG_SESS_UI_CONNECT_STATUS, 0]);
            } else {
                s.ui(&[EE_NETMSG_SESS_UI_CLOSE_FORM]);
                if s.won_present() && s.fact1(EE_NETMSG_SESS_FACT_FORM_VISIBLE, FORM_AVAILABLE_GAMES) != 0 {
                    s.won(&[EE_NETMSG_SESS_WON_LEAVE]);
                }
                s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, IDS_COULD_NOT_CONNECT_TO_SELECTED, 0, 1]);
            }
        }
        1 => {
            if !s.multiplayer() {
                s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, IDS_CONNECTING_TO_SERVER, 0, 0]);
            }
        }
        4 => {
            s.ui(&[EE_NETMSG_SESS_UI_CLOSE_FORM]);
            s.ui(&[EE_NETMSG_SESS_UI_RETRY, IDS_CONNECTING_TO_SERVER]);
        }
        _ => {}
    }
}

/// `EEClient::ExecuteRemovePlayer`: the host's starts the server migration, ours shows the booted or out-of-sync box
/// and cancels the game, anyone else's is announced and removed; the matching sound.
fn client_remove<H: SessHost + ?Sized>(s: &mut S<H>, player: i64, oos: bool, exiting: bool) {
    if s.get(EE_NETMSG_SESS_SELF_WAS_BOOTED) != 0 || s.get(EE_NETMSG_SESS_SELF_HAS_LEFT) != 0 {
        return;
    }
    if player == SLOT_SERVER {
        if oos {
            let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT, SLOT_SERVER]);
            s.ui(&[EE_NETMSG_SESS_UI_CHAT_PLAYER, IDS_HOST_IS_OUT_OF_SYNCH, p]);
        }
        if s.get(EE_NETMSG_SESS_SELF_ALLOW_MIGRATION) == 0 {
            s.set(EE_NETMSG_SESS_SELF_SERVER_LOST, 1);
        }
        connection_lost(s);
    } else if player as u64 == s.fact(EE_NETMSG_SESS_FACT_PLAYER_NUMBER) as u64 {
        if exiting {
            s.set(EE_NETMSG_SESS_SELF_HAS_LEFT, 1);
            s.ui(&[EE_NETMSG_SESS_UI_CANCEL_CALLBACK]);
        } else {
            s.set(EE_NETMSG_SESS_SELF_IGNORE_TIMEOUT, 1);
            s.set(EE_NETMSG_SESS_SELF_WAS_BOOTED, 1);
            s.set(EE_NETMSG_SESS_SELF_HAS_LEFT, 1);
            let text = if oos { IDS_YOU_ARE_OUT_OF_SYNCH } else { IDS_YOU_HAVE_BEEN_BOOTED_DUE };
            s.ui(&[EE_NETMSG_SESS_UI_DISPLAY, text, 1, 1]);
        }
    } else {
        let text = if oos { IDS_PLAYER_IS_OUT_OF_SYNCH } else { IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME };
        let p = s.world(&[EE_NETMSG_SESS_WORLD_PLAYER, player]);
        s.ui(&[EE_NETMSG_SESS_UI_CHAT_PLAYER, text, p]);
        s.world(&[EE_NETMSG_SESS_WORLD_REMOVE, player]);
    }
    if !exiting {
        s.call(EE_NETMSG_SESS_OP_SOUND, &[if oos { 1 } else { 3 }]);
    }
}

/// The client's answer to a connection response's error: the string and the way it leaves (a refusal while
/// joining, a removal, the host gone), or None (accepted; or the error has no case)
pub fn refusal(error: i64, joining: bool) -> Option<(i64, i64)> {
    if joining {
        return match error {
            CONNECTION_NAME_IN_USE => Some((IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME, EE_NETMSG_SESS_LEAVE_SHOW)),
            CONNECTION_BAD_CHECKSUM => Some((IDS_ONE_OR_MORE_OF_YOUR_FILES, EE_NETMSG_SESS_LEAVE_SHOW)),
            CONNECTION_GAME_FULL => Some((IDS_NO_OPEN_SLOTS_LEFT_IN_GAME, EE_NETMSG_SESS_LEAVE_SHOW)),
            CONNECTION_VERSION_MISMATCH => Some((IDS_TO_JOIN_THIS_GAME_YOU, EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS)),
            // the game in progress also under the code the host sends (a fix: the original had no case for either
            // while joining and left the player waiting)
            CONNECTION_GAME_IN_PROGRESS | CONNECTION_GAME_IN_PROGRESS_AS_SENT => {
                Some((IDS_GAME_IS_IN_PROGRESS, EE_NETMSG_SESS_LEAVE_SHOW))
            }
            _ => None,
        };
    }
    match error {
        CONNECTION_REMOVED_BY_HOST => Some((IDS_SERVER_HAS_REMOVED_YOU_FROM_GAME, EE_NETMSG_SESS_LEAVE_BOOT)),
        CONNECTION_HOST_GONE => Some((IDS_HOST_OF_THIS_GAME_HAS_GONE, EE_NETMSG_SESS_LEAVE_BOOT)),
        CONNECTION_GAME_IN_PROGRESS => Some((IDS_GAME_IS_IN_PROGRESS, EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS)),
        CONNECTION_GAME_EXITING => Some((0, EE_NETMSG_SESS_LEAVE_QUIET)),
        _ => None,
    }
}

/// `EEClient::HandleMessage`: connection responses (the lobby, or a refusal shown), reconnection, communications
/// ticks; load/save and lag messages execute at once.
fn client_message<H: SessHost + ?Sized>(s: &mut S<H>, m: u64) {
    match s.msg(EE_NETMSG_SESS_MSG_TYPE, m) as u8 {
        NE_RECONNECTION_RESPONSE => {
            let tick = s.ne(m, 2);
            s.comms(&[EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS, tick]);
            s.comms(&[EE_NETMSG_SESS_COMMS_SET_CT, tick]);
            s.fact(EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS); // the original reads it and drops it
            s.fact(EE_NETMSG_SESS_FACT_CURRENT_CT);
        }
        NE_COMMUNICATIONS_TICK => communications_tick(s, m),
        NE_CONNECTION_RESPONSE => {
            let error = s.ne(m, 0) as u32 as i32 as i64;
            if error == CONNECTION_ACCEPTED {
                let ui_slot = s.ne(m, 1) & 0xff;
                s.set(EE_NETMSG_SESS_SELF_UI_SLOT, ui_slot);
                s.comms(&[EE_NETMSG_SESS_COMMS_LOCAL_OUT, 0]);
                if s.multiplayer() {
                    s.ui(&[EE_NETMSG_SESS_UI_CONNECT_STATUS, 1]);
                } else {
                    s.ui(&[EE_NETMSG_SESS_UI_OPEN_FORM, FORM_LOBBY]);
                }
                let tick = s.ne(m, 3);
                s.comms(&[EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS, tick]);
                s.comms(&[EE_NETMSG_SESS_COMMS_SET_CT, tick]);
                s.fact(EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS);
            } else if s.get(EE_NETMSG_SESS_SELF_UI_SLOT) == 0xff {
                if s.multiplayer() {
                    s.ui(&[EE_NETMSG_SESS_UI_CONNECT_STATUS, 0]);
                } else if let Some((text, how)) = refusal(error, true) {
                    s.ui(&[EE_NETMSG_SESS_UI_LEAVE, text, how, m as i64]);
                }
            } else if let Some((text, how)) = refusal(error, false) {
                s.ui(&[EE_NETMSG_SESS_UI_LEAVE, text, how, m as i64]);
            }
        }
        MSG_NEW_EDICT_ENACTED => {}
        MSG_LOAD_OR_SAVE | MSG_LAG_STATUS => {
            s.msg(EE_NETMSG_SESS_MSG_EXECUTE, m);
            s.msg(EE_NETMSG_SESS_MSG_DEREFERENCE, m);
        }
        MSG_TOGGLE_DAY_NIGHT | MSG_SNAP_TO_DAYLIGHT => {
            s.msg(EE_NETMSG_SESS_MSG_EXECUTE, m);
        }
        _ => {}
    }
}

/// `EEClient::HandleConnectionLost`: in a game the server migration (if allowed); after loading the game cancelled;
/// in the lobby back to the game list.
fn connection_lost<H: SessHost + ?Sized>(s: &mut S<H>) {
    if s.fact(EE_NETMSG_SESS_FACT_CURRENT_PT) > 0 {
        if s.get(EE_NETMSG_SESS_SELF_ALLOW_MIGRATION) != 0 {
            s.ui(&[EE_NETMSG_SESS_UI_CHAT_TEXT, IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT, 1]);
            s.comms(&[EE_NETMSG_SESS_COMMS_SLEEP, 500]);
            s.ui(&[EE_NETMSG_SESS_UI_ACTION_MIGRATION]);
        }
    } else if s.get(EE_NETMSG_SESS_SELF_HAS_LEFT) == 0 {
        if s.get(EE_NETMSG_SESS_SELF_HAS_LOADED) != 0 {
            s.set(EE_NETMSG_SESS_SELF_LOST_AFTER_LOADING, 1);
            s.set(EE_NETMSG_SESS_SELF_HAS_LEFT, 1);
            s.ui(&[EE_NETMSG_SESS_UI_CANCEL_GAME]);
        } else if s.call(EE_NETMSG_SESS_OP_FACT, &[EE_NETMSG_SESS_FACT_FORM_IS_CURRENT, FORM_LOBBY, 1]) != 0 {
            if s.won_present() {
                s.won(&[EE_NETMSG_SESS_WON_LEAVE]);
            }
            s.ui(&[EE_NETMSG_SESS_UI_ACTION_BOOT, IDS_HOST_OF_THIS_GAME_HAS_GONE]);
            s.comms(&[EE_NETMSG_SESS_COMMS_DISCONNECT]);
        }
    }
}

// ---- the C ABI: one entry per member; `server` says which object the frame's is ----

/// The server's and client's members that moved, by `EE_NETMSG_SESS_DO_*`: a, b, c, d their arguments (a message:
/// its pointer); the answer (FilterPlayerInput's pass, StartVote's, HandleStartGame's 0).
pub const EE_NETMSG_SESS_DO_CONNECTION_REQUEST: u32 = 1;
pub const EE_NETMSG_SESS_DO_BOOT: u32 = 2;
pub const EE_NETMSG_SESS_DO_REMOVE: u32 = 3;
pub const EE_NETMSG_SESS_DO_SYSTEM_MESSAGE: u32 = 4;
pub const EE_NETMSG_SESS_DO_FILTER: u32 = 5;
pub const EE_NETMSG_SESS_DO_LOCAL_INPUT: u32 = 6;
pub const EE_NETMSG_SESS_DO_LOBBY_INPUT: u32 = 7;
pub const EE_NETMSG_SESS_DO_START_VOTE: u32 = 8;
pub const EE_NETMSG_SESS_DO_CAST_VOTE: u32 = 9;
pub const EE_NETMSG_SESS_DO_RECONNECT_TIMED_OUT: u32 = 10;
/// a: the slots, b: their count, c: the flag
pub const EE_NETMSG_SESS_DO_LAGGERS: u32 = 11;
pub const EE_NETMSG_SESS_DO_SLOT_INFO: u32 = 12;
pub const EE_NETMSG_SESS_DO_CONNECTION_STATE: u32 = 13;
pub const EE_NETMSG_SESS_DO_CLIENT_MESSAGE: u32 = 14;
pub const EE_NETMSG_SESS_DO_CONNECTION_LOST: u32 = 15;
pub const EE_NETMSG_SESS_DO_START_GAME: u32 = 16;

/// Runs a member over a host.
pub fn run<H: SessHost + ?Sized>(h: &mut H, server: bool, what: u32, a: i64, b: i64, c: i64, d: i64) -> i64 {
    let mut s = S { h, server };
    match what {
        EE_NETMSG_SESS_DO_CONNECTION_REQUEST => connection_request(&mut s, a, b),
        EE_NETMSG_SESS_DO_BOOT => boot(&mut s, a, b != 0, c != 0),
        EE_NETMSG_SESS_DO_REMOVE => {
            if server {
                server_remove(&mut s, a, b != 0, c != 0, d != 0)
            } else {
                client_remove(&mut s, a, c != 0, d != 0)
            }
        }
        EE_NETMSG_SESS_DO_SYSTEM_MESSAGE => system_message(&mut s, a as u64),
        EE_NETMSG_SESS_DO_FILTER => return filter_input(&mut s, a as u64) as i64,
        EE_NETMSG_SESS_DO_LOCAL_INPUT => local_input(&mut s, a as u64),
        EE_NETMSG_SESS_DO_LOBBY_INPUT => lobby_input(&mut s, a as u64),
        EE_NETMSG_SESS_DO_START_VOTE => return start_vote(&mut s, a) as i64,
        EE_NETMSG_SESS_DO_CAST_VOTE => cast_vote(&mut s, a, b != 0, c),
        EE_NETMSG_SESS_DO_RECONNECT_TIMED_OUT => reconnect_wait_timed_out(&mut s),
        EE_NETMSG_SESS_DO_LAGGERS => {
            let slots: &[u8] = if b == 0 {
                &[]
            } else {
                // SAFETY: the forwarder's vector, alive for the call
                unsafe { std::slice::from_raw_parts(a as usize as *const u8, b as usize) }
            };
            let slots = slots.to_vec();
            report_laggers(&mut s, &slots, c != 0)
        }
        EE_NETMSG_SESS_DO_SLOT_INFO => send_slot_info(&mut s),
        EE_NETMSG_SESS_DO_CONNECTION_STATE => connection_state(&mut s, a),
        EE_NETMSG_SESS_DO_CLIENT_MESSAGE => client_message(&mut s, a as u64),
        EE_NETMSG_SESS_DO_CONNECTION_LOST => connection_lost(&mut s),
        EE_NETMSG_SESS_DO_START_GAME => client_start_game(&mut s),
        _ => {}
    }
    0
}

/// The C ABI: a server's (`server`) or client's member `what` over the C++ host, with the forwarder's frame.
///
/// # Safety
/// `host` is the C++ host and `frame` its frame; a message argument is a live message; the laggers' slots are
/// `b` bytes at `a`.
#[no_mangle]
pub unsafe extern "C" fn ee_netmsg_session(
    host: EeNetSessHostFn,
    frame: *mut c_void,
    server: bool,
    what: u32,
    a: i64,
    b: i64,
    c: i64,
    d: i64,
) -> i64 {
    let mut h = GameHost { host, frame };
    run(&mut h, server, what, a, b, c, d)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host that records every call and answers by a function of (op, values).
    struct Script<F: FnMut(u32, &[i64; 8]) -> i64> {
        calls: Vec<(u32, [i64; 8])>,
        answer: F,
    }

    impl<F: FnMut(u32, &[i64; 8]) -> i64> SessHost for Script<F> {
        fn op(&mut self, op: u32, v: [i64; 8]) -> i64 {
            self.calls.push((op, v));
            (self.answer)(op, &v)
        }
    }

    fn script<F: FnMut(u32, &[i64; 8]) -> i64>(answer: F) -> Script<F> {
        Script { calls: Vec::new(), answer }
    }

    fn sends(calls: &[(u32, [i64; 8])]) -> Vec<[i64; 8]> {
        calls.iter().filter(|c| c.0 == EE_NETMSG_SESS_OP_SEND).map(|c| c.1).collect()
    }

    #[test]
    fn the_tables() {
        assert_eq!(ping_quality(0), 1.0);
        assert_eq!(ping_quality(50), 1.0);
        assert_eq!(ping_quality(51), 0.8);
        assert_eq!(ping_quality(150), 0.8);
        assert_eq!(ping_quality(250), 0.6);
        assert_eq!(ping_quality(500), 0.4);
        assert_eq!(ping_quality(750), 0.2);
        assert_eq!(ping_quality(751), 0.0);
        assert!(!vote_passes(0, 0));
        assert!(!vote_passes(2, 2));
        assert!(vote_passes(3, 2));
        assert!(!vote_passes(1, 2));
        assert_eq!(lag_bit(1), 1);
        assert_eq!(lag_bit(8), 0x80);
        assert_eq!(lag_bit(9), 0); // the byte cast drops the bit
        assert_eq!(lag_bit(0), 0); // a shift of -1 is x86's 31
        assert_eq!(
            refusal(CONNECTION_NAME_IN_USE, true),
            Some((IDS_PLAYER_NAME_ALREADY_EXIST_IN_GAME, EE_NETMSG_SESS_LEAVE_SHOW))
        );
        assert_eq!(
            refusal(CONNECTION_GAME_IN_PROGRESS_AS_SENT, true),
            Some((IDS_GAME_IS_IN_PROGRESS, EE_NETMSG_SESS_LEAVE_SHOW))
        );
        assert_eq!(
            refusal(CONNECTION_GAME_IN_PROGRESS, false),
            Some((IDS_GAME_IS_IN_PROGRESS, EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS))
        );
        assert_eq!(refusal(CONNECTION_GAME_IN_PROGRESS_AS_SENT, false), None);
        assert_eq!(refusal(CONNECTION_GAME_EXITING, false), Some((0, EE_NETMSG_SESS_LEAVE_QUIET)));
        assert_eq!(refusal(CONNECTION_REMOVED_BY_HOST, true), None);
    }

    #[test]
    fn a_wrong_version_is_refused_and_unregistered() {
        // multiplayer, the version different: one response, the slot unregistered, the locks left in reverse
        let mut h = script(|op, v| match (op, v[0]) {
            (EE_NETMSG_SESS_OP_FACT, EE_NETMSG_SESS_FACT_VERSION_MATCH) => 0,
            (EE_NETMSG_SESS_OP_FACT, EE_NETMSG_SESS_FACT_MULTIPLAYER_GAME) => 1,
            _ => 0,
        });
        run(&mut h, true, EE_NETMSG_SESS_DO_CONNECTION_REQUEST, 3, 0x1234, 0, 0);
        let s = sends(&h.calls);
        assert_eq!(s.len(), 1);
        assert_eq!(&s[0][..6], &[EE_NETMSG_SESS_SEND_RESPONSE, 3, CONNECTION_VERSION_MISMATCH, 3, 0xff, 1]);
        let locks: Vec<(i64, i64)> =
            h.calls.iter().filter(|c| c.0 == EE_NETMSG_SESS_OP_LOCK).map(|c| (c.1[0], c.1[1])).collect();
        assert_eq!(
            locks,
            vec![
                (EE_NETMSG_SESS_LOCK_CT, 1),
                (EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, 1),
                (EE_NETMSG_SESS_LOCK_PING, 1),
                (EE_NETMSG_SESS_LOCK_SEARCH, 1),
                (EE_NETMSG_SESS_LOCK_SEARCH, 0),
                (EE_NETMSG_SESS_LOCK_PING, 0),
                (EE_NETMSG_SESS_LOCK_OPTIONS_WRITE, 0),
                (EE_NETMSG_SESS_LOCK_CT, 0),
            ]
        );
    }

    #[test]
    fn a_full_game_is_refused() {
        // a single player lobby, every slot a human with another name: no name in use, no open slot
        let mut h = script(|op, v| match (op, v[0]) {
            (EE_NETMSG_SESS_OP_FACT, EE_NETMSG_SESS_FACT_VERSION_MATCH) => 1,
            (EE_NETMSG_SESS_OP_FACT, EE_NETMSG_SESS_FACT_BINARY_CHECKSUM) => 0x1234,
            (EE_NETMSG_SESS_OP_REG, _) if v[1] == EE_NETMSG_SESS_REG_GET => 1,
            _ => 0,
        });
        run(&mut h, true, EE_NETMSG_SESS_DO_CONNECTION_REQUEST, 2, 0x1234, 0, 0);
        let s = sends(&h.calls);
        assert_eq!(s.len(), 1);
        assert_eq!(&s[0][..6], &[EE_NETMSG_SESS_SEND_RESPONSE, 2, CONNECTION_GAME_FULL, 2, 0xff, 1]);
    }

    #[test]
    fn the_host_removed_becomes_a_client_and_stops() {
        let mut h = script(|op, v| match (op, v[0]) {
            (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_TYPE) => NE_CONNECTION_RESPONSE as i64,
            (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_FOR_SLOT) => SLOT_SERVER,
            (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_NE_VALUE) => CONNECTION_REMOVED_BY_HOST,
            _ => 0,
        });
        assert_eq!(run(&mut h, true, EE_NETMSG_SESS_DO_FILTER, 77, 0, 0, 0), 0);
        let last = h.calls.last().unwrap();
        assert_eq!((last.0, last.1[0]), (EE_NETMSG_SESS_OP_COMMS, EE_NETMSG_SESS_COMMS_BECOME_CLIENT));
    }

    #[test]
    fn a_vote_closes_when_everyone_voted() {
        // two players voted yes and no before; the third yes passes it, the votes reset
        let mut voted = [false; 256];
        let mut members: Vec<(i64, i64)> = vec![
            (EE_NETMSG_SESS_SELF_NUM_VOTES, 2),
            (EE_NETMSG_SESS_SELF_YES_VOTES, 1),
            (EE_NETMSG_SESS_SELF_NO_VOTES, 1),
            (EE_NETMSG_SESS_SELF_VOTE_EDICT, 7),
        ];
        let mut h = script(move |op, v| match op {
            EE_NETMSG_SESS_OP_SELF_GET if v[0] == EE_NETMSG_SESS_SELF_VOTED => voted[v[1] as usize] as i64,
            EE_NETMSG_SESS_OP_SELF_SET if v[0] == EE_NETMSG_SESS_SELF_VOTED => {
                voted[v[1] as usize] = v[2] != 0;
                0
            }
            EE_NETMSG_SESS_OP_SELF_GET => members.iter().find(|m| m.0 == v[0]).map_or(0, |m| m.1),
            EE_NETMSG_SESS_OP_SELF_SET => {
                members.retain(|m| m.0 != v[0]);
                members.push((v[0], v[2]));
                0
            }
            EE_NETMSG_SESS_OP_FACT if v[0] == EE_NETMSG_SESS_FACT_NUM_PLAYERS => 3,
            EE_NETMSG_SESS_OP_SEND => 0x1000,
            _ => 0,
        });
        run(&mut h, true, EE_NETMSG_SESS_DO_CAST_VOTE, 4, 1, 2, 0);
        let s = sends(&h.calls);
        assert_eq!(&s[0][..7], &[EE_NETMSG_SESS_SEND_NEW_RECORD, MSG_NEW_EDICT_ENACTED as i64, 4, 7, 3, 1, 1]);
        assert!(h.calls.iter().any(|c| c.0 == EE_NETMSG_SESS_OP_MSG && c.1[0] == EE_NETMSG_SESS_MSG_BROADCAST));
        assert!(h.calls.iter().any(|c| c.0 == EE_NETMSG_SESS_OP_MSG && c.1[0] == EE_NETMSG_SESS_MSG_DEREFERENCE));
    }

    #[test]
    fn a_refusal_while_joining_leaves() {
        for (error, text, how) in [
            (CONNECTION_BAD_CHECKSUM, IDS_ONE_OR_MORE_OF_YOUR_FILES, EE_NETMSG_SESS_LEAVE_SHOW),
            (CONNECTION_VERSION_MISMATCH, IDS_TO_JOIN_THIS_GAME_YOU, EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS),
        ] {
            let mut h = script(move |op, v| match (op, v[0]) {
                (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_TYPE) => NE_CONNECTION_RESPONSE as i64,
                (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_NE_VALUE) => error,
                (EE_NETMSG_SESS_OP_SELF_GET, EE_NETMSG_SESS_SELF_UI_SLOT) => 0xff,
                _ => 0,
            });
            run(&mut h, false, EE_NETMSG_SESS_DO_CLIENT_MESSAGE, 9, 0, 0, 0);
            let ui: Vec<_> = h.calls.iter().filter(|c| c.0 == EE_NETMSG_SESS_OP_UI).collect();
            assert_eq!(ui.len(), 1);
            assert_eq!(&ui[0].1[..4], &[EE_NETMSG_SESS_UI_LEAVE, text, how, 9]);
        }
    }

    #[test]
    fn a_selection_takes_a_new_id() {
        let mut fast = 0xffi64;
        let mut h = script(move |op, v| match (op, v[0]) {
            (EE_NETMSG_SESS_OP_MSG, EE_NETMSG_SESS_MSG_TYPE) => 0x4e,
            (EE_NETMSG_SESS_OP_SELF_GET, EE_NETMSG_SESS_SELF_FAST_ID) => fast,
            (EE_NETMSG_SESS_OP_SELF_SET, EE_NETMSG_SESS_SELF_FAST_ID) => {
                fast = v[2];
                0
            }
            _ => 0,
        });
        run(&mut h, false, EE_NETMSG_SESS_DO_LOCAL_INPUT, 5, 0, 0, 0);
        assert!(h
            .calls
            .iter()
            .any(|c| c.0 == EE_NETMSG_SESS_OP_MSG && c.1[..3] == [EE_NETMSG_SESS_MSG_SET_FAST_ID, 5, 0]));
        assert!(h
            .calls
            .iter()
            .any(|c| c.0 == EE_NETMSG_SESS_OP_WORLD && c.1[0] == EE_NETMSG_SESS_WORLD_EXECUTE_SELECTION));
    }
}
