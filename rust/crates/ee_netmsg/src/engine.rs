//! The engine messages' wire layouts (`NEMessage.cpp`, 28 classes, the types 0x20..0x3c but 0x2c): the members in wire
//! order after the 4-byte header, as `classes` is for the game messages. Their class ids follow the game classes'
//! (`classes::CLASS_COUNT` on). The two communications ticks end with the players' inputs (`Kind::Inputs`: a u8
//! count, then each nested message, game or engine, in its own layout). Written from the original
//! `ExportMessage`/`ImportMessage` bodies (RUST_PLAN R5b.4); every fixed class's sum is its `ExportSize` constant.

use crate::classes::CLASS_COUNT;
use crate::schema::{Field, Kind::*, MsgClass};

/// The first engine class id.
pub const EE_NETMSG_ENGINE_BASE: u32 = CLASS_COUNT as u32;
/// How many engine classes there are.
pub const EE_NETMSG_ENGINE_CLASS_COUNT: usize = 28;

/// `NEMAddPlayer`
pub const EE_NETMSG_NE_ADD_PLAYER: u32 = EE_NETMSG_ENGINE_BASE;
/// `NEMAddPlayerResponse`
pub const EE_NETMSG_NE_ADD_PLAYER_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 1;
/// `NEMClientConnectionRequest`
pub const EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST: u32 = EE_NETMSG_ENGINE_BASE + 2;
/// `NEMClientConnectionRequestResponse`
pub const EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 3;
/// `NEMClientDisconnectionNotification`
pub const EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION: u32 = EE_NETMSG_ENGINE_BASE + 4;
/// `NEMClientDisconnectionNotificationResponse`
pub const EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 5;
/// `NEMClientReconnectionRequest`
pub const EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST: u32 = EE_NETMSG_ENGINE_BASE + 6;
/// `NEMClientReconnectionRequestResponse`
pub const EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 7;
/// `NEMClientRequestPause`
pub const EE_NETMSG_NE_CLIENT_REQUEST_PAUSE: u32 = EE_NETMSG_ENGINE_BASE + 8;
/// `NEMCommunicationsTick`
pub const EE_NETMSG_NE_COMMUNICATIONS_TICK: u32 = EE_NETMSG_ENGINE_BASE + 9;
/// `NEMCommunicationsTickResponse`
pub const EE_NETMSG_NE_COMMUNICATIONS_TICK_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 10;
/// `NEMPauseGame`
pub const EE_NETMSG_NE_PAUSE_GAME: u32 = EE_NETMSG_ENGINE_BASE + 11;
/// `NEMPauseGameResponse`
pub const EE_NETMSG_NE_PAUSE_GAME_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 12;
/// `NEMPhysicsTick`
pub const EE_NETMSG_NE_PHYSICS_TICK: u32 = EE_NETMSG_ENGINE_BASE + 13;
/// `NEMPlayerConnectionCheck`
pub const EE_NETMSG_NE_PLAYER_CONNECTION_CHECK: u32 = EE_NETMSG_ENGINE_BASE + 14;
/// `NEMPlayerConnectionCheckResponse`
pub const EE_NETMSG_NE_PLAYER_CONNECTION_CHECK_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 15;
/// `NEMRemovePlayer`
pub const EE_NETMSG_NE_REMOVE_PLAYER: u32 = EE_NETMSG_ENGINE_BASE + 16;
/// `NEMServerIsAlive`
pub const EE_NETMSG_NE_SERVER_IS_ALIVE: u32 = EE_NETMSG_ENGINE_BASE + 17;
/// `NEMStartGame`
pub const EE_NETMSG_NE_START_GAME: u32 = EE_NETMSG_ENGINE_BASE + 18;
/// `NEMStartGameResponse`
pub const EE_NETMSG_NE_START_GAME_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 19;
/// `NEMStopGame`
pub const EE_NETMSG_NE_STOP_GAME: u32 = EE_NETMSG_ENGINE_BASE + 20;
/// `NEMStopGameResponse`
pub const EE_NETMSG_NE_STOP_GAME_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 21;
/// `NEMUnPauseGame`
pub const EE_NETMSG_NE_UN_PAUSE_GAME: u32 = EE_NETMSG_ENGINE_BASE + 22;
/// `NEMUnPauseGameResponse`
pub const EE_NETMSG_NE_UN_PAUSE_GAME_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 23;
/// `NEMWorldDatabaseChecksum`
pub const EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM: u32 = EE_NETMSG_ENGINE_BASE + 24;
/// `NEMWorldDatabaseChecksumResponse`
pub const EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM_RESPONSE: u32 = EE_NETMSG_ENGINE_BASE + 25;
/// `NEMXferMessage`
pub const EE_NETMSG_NE_XFER_MESSAGE: u32 = EE_NETMSG_ENGINE_BASE + 26;
/// `NEMXferStatus`
pub const EE_NETMSG_NE_XFER_STATUS: u32 = EE_NETMSG_ENGINE_BASE + 27;

const fn class(name: &'static str, fields: &'static [Field]) -> MsgClass {
    MsgClass { name, fields, import_fields: fields.len() }
}

pub static ENGINE_CLASSES: [MsgClass; EE_NETMSG_ENGINE_CLASS_COUNT] = [
    class("NEMAddPlayer", &[Field::new("mPlayerIndex", U8), Field::new("mPlayerName", WStr)]),
    class("NEMAddPlayerResponse", &[]),
    class(
        "NEMClientConnectionRequest",
        &[
            Field::new("mIsBehindFirewall", Bool),
            Field::new("mVersion", WStr),
            Field::new("mPlayerName", WStr),
            Field::new("mBinaryChecksum", U32),
        ],
    ),
    class(
        "NEMClientConnectionRequestResponse",
        &[
            Field::new("mError", I32),
            Field::new("mUISlotNumber", U8),
            Field::new("mSendToSlot", U8),
            Field::new("mCurrentCT", U32),
            Field::new("mVersion", WStr),
        ],
    ),
    class("NEMClientDisconnectionNotification", &[Field::new("mIsExiting", Bool)]),
    class("NEMClientDisconnectionNotificationResponse", &[Field::new("mError", I32)]),
    class("NEMClientReconnectionRequest", &[Field::new("mCurrentCT", U32), Field::new("mPlayerNumber", U32)]),
    class(
        "NEMClientReconnectionRequestResponse",
        &[Field::new("mError", I32), Field::new("mNewSendToSlot", U8), Field::new("mCurrentCT", U32)],
    ),
    class("NEMClientRequestPause", &[]),
    class(
        "NEMCommunicationsTick",
        &[
            Field::new("mCurrentCommunicationsTick", U32),
            Field::new("mPhysicsTicksToPerform", U8),
            Field::new("mPhysicsTicksPerSecond", U8),
            Field::new("mGenerateChecksum", Bool),
            Field::new("mDumpTheWorld", Bool),
            Field::new("mPlayerInputs", Inputs),
        ],
    ),
    class(
        "NEMCommunicationsTickResponse",
        &[
            Field::new("mLastCommunicationsTickProcessed", U32),
            Field::new("mServersCommunicationTick", U32),
            Field::new("mAveragePhysicsTickTime", U16),
            Field::new("mPlayerInputs", Inputs),
        ],
    ),
    class("NEMPauseGame", &[Field::new("mUniqueID", U32), Field::new("mSendToAllAndWaitMessageID", U32)]),
    class("NEMPauseGameResponse", &[Field::new("mUniqueID", U32)]),
    // (mPhysicsTicksPerSecond is not on the wire)
    class(
        "NEMPhysicsTick",
        &[
            Field::new("mNumberOfPlayerInputs", U32),
            Field::new("mPhysicsTicksToPerform", U8),
            Field::new("mGenerateChecksum", Bool),
            Field::new("mDumpTheWorld", Bool),
        ],
    ),
    class("NEMPlayerConnectionCheck", &[Field::new("mPingTimeStart", U32)]),
    class("NEMPlayerConnectionCheckResponse", &[Field::new("mPingTimeStart", U32)]),
    class(
        "NEMRemovePlayer",
        &[
            Field::new("mPlayerIndex", U8),
            Field::new("mPlayerIsOOS", Bool),
            Field::new("mGameIsExiting", Bool),
            Field::new("mWasBooted", Bool),
        ],
    ),
    class("NEMServerIsAlive", &[]),
    class("NEMStartGame", &[Field::new("mOffsetStartTime", U32)]),
    class("NEMStartGameResponse", &[Field::new("mApplicationTimeAtStart", U32)]),
    class("NEMStopGame", &[]),
    class("NEMStopGameResponse", &[]),
    class("NEMUnPauseGame", &[Field::new("mUniqueID", U32), Field::new("mSendToAllAndWaitMessageID", U32)]),
    class("NEMUnPauseGameResponse", &[Field::new("mUniqueID", U32)]),
    class("NEMWorldDatabaseChecksum", &[Field::new("mPhysicsTickTrigger", U32), Field::new("mDumpWorld", Bool)]),
    class(
        "NEMWorldDatabaseChecksumResponse",
        &[Field::new("mCommunicationsTick", U32), Field::new("mWorldDatabaseChecksum", U32)],
    ),
    // the whole 1 KB block travels, whatever mDataSize says
    class("NEMXferMessage", &[Field::new("mID", U8), Field::new("mDataSize", U32), Field::new("mData", Raw(0x400))]),
    class(
        "NEMXferStatus",
        &[
            Field::new("mClientSlot", U8),
            Field::new("mPercentDone", U8),
            Field::new("mXferID", U32),
            Field::new("mStatusID", U8),
        ],
    ),
];

/// The engine factory's types (`NEMessage::BuildNEMessage`) and their classes.
pub static ENGINE_TYPES: [(u8, u32); EE_NETMSG_ENGINE_CLASS_COUNT] = [
    (0x20, EE_NETMSG_NE_ADD_PLAYER),
    (0x21, EE_NETMSG_NE_ADD_PLAYER_RESPONSE),
    (0x22, EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST),
    (0x23, EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST_RESPONSE),
    (0x24, EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION),
    (0x25, EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION_RESPONSE),
    (0x26, EE_NETMSG_NE_COMMUNICATIONS_TICK),
    (0x27, EE_NETMSG_NE_COMMUNICATIONS_TICK_RESPONSE),
    (0x28, EE_NETMSG_NE_PHYSICS_TICK),
    (0x29, EE_NETMSG_NE_PLAYER_CONNECTION_CHECK),
    (0x2a, EE_NETMSG_NE_PLAYER_CONNECTION_CHECK_RESPONSE),
    (0x2b, EE_NETMSG_NE_REMOVE_PLAYER),
    (0x2d, EE_NETMSG_NE_START_GAME),
    (0x2e, EE_NETMSG_NE_START_GAME_RESPONSE),
    (0x2f, EE_NETMSG_NE_STOP_GAME),
    (0x30, EE_NETMSG_NE_STOP_GAME_RESPONSE),
    (0x31, EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM),
    (0x32, EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM_RESPONSE),
    (0x33, EE_NETMSG_NE_PAUSE_GAME),
    (0x34, EE_NETMSG_NE_PAUSE_GAME_RESPONSE),
    (0x35, EE_NETMSG_NE_UN_PAUSE_GAME),
    (0x36, EE_NETMSG_NE_UN_PAUSE_GAME_RESPONSE),
    (0x37, EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST),
    (0x38, EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST_RESPONSE),
    (0x39, EE_NETMSG_NE_CLIENT_REQUEST_PAUSE),
    (0x3a, EE_NETMSG_NE_SERVER_IS_ALIVE),
    (0x3b, EE_NETMSG_NE_XFER_MESSAGE),
    (0x3c, EE_NETMSG_NE_XFER_STATUS),
];

#[cfg(test)]
mod tests {
    use super::*;

    fn size(name: &str) -> Option<usize> {
        ENGINE_CLASSES.iter().find(|c| c.name == name).unwrap().fixed_size()
    }

    #[test]
    fn the_original_export_sizes() {
        // the ExportSize constants of NEMessage.cpp
        for (name, n) in [
            ("NEMAddPlayerResponse", 4),
            ("NEMClientDisconnectionNotification", 5),
            ("NEMClientDisconnectionNotificationResponse", 8),
            ("NEMClientReconnectionRequest", 12),
            ("NEMClientReconnectionRequestResponse", 13),
            ("NEMClientRequestPause", 4),
            ("NEMPauseGame", 12),
            ("NEMPauseGameResponse", 8),
            ("NEMPhysicsTick", 11),
            ("NEMPlayerConnectionCheck", 8),
            ("NEMPlayerConnectionCheckResponse", 8),
            ("NEMRemovePlayer", 8),
            ("NEMServerIsAlive", 4),
            ("NEMStartGame", 8),
            ("NEMStartGameResponse", 8),
            ("NEMStopGame", 4),
            ("NEMStopGameResponse", 4),
            ("NEMUnPauseGame", 12),
            ("NEMUnPauseGameResponse", 8),
            ("NEMWorldDatabaseChecksum", 9),
            ("NEMWorldDatabaseChecksumResponse", 12),
            ("NEMXferMessage", 4 + 1 + 4 + 0x400),
            ("NEMXferStatus", 11),
        ] {
            assert_eq!(size(name), Some(n), "{name}");
        }
        for name in [
            "NEMAddPlayer",
            "NEMClientConnectionRequest",
            "NEMClientConnectionRequestResponse",
            "NEMCommunicationsTick",
            "NEMCommunicationsTickResponse",
        ] {
            assert_eq!(size(name), None, "{name}");
        }
    }

    #[test]
    fn the_variable_classes_fixed_parts() {
        // the constants their ExportSize adds to the strings' and inputs' sizes
        let fixed = |name: &str| -> usize {
            let c = ENGINE_CLASSES.iter().find(|c| c.name == name).unwrap();
            4 + c.fields.iter().filter_map(|f| f.kind.fixed_size()).sum::<usize>()
        };
        assert_eq!(fixed("NEMAddPlayer"), 5);
        assert_eq!(fixed("NEMClientConnectionRequest"), 9);
        assert_eq!(fixed("NEMClientConnectionRequestResponse"), 14);
        assert_eq!(fixed("NEMCommunicationsTick"), 4 + 8);
        assert_eq!(fixed("NEMCommunicationsTickResponse"), 4 + 10);
    }

    #[test]
    fn types_and_ids() {
        let mut seen = std::collections::BTreeSet::new();
        for &(t, c) in &ENGINE_TYPES {
            assert!((0x20..=0x3c).contains(&t) && t != 0x2c);
            assert!(seen.insert(t));
            assert!(c >= EE_NETMSG_ENGINE_BASE && ((c - EE_NETMSG_ENGINE_BASE) as usize) < ENGINE_CLASSES.len());
        }
        assert_eq!(seen.len(), 28);
        // the inputs, where a class has them, come last
        for c in &ENGINE_CLASSES {
            for (i, f) in c.fields.iter().enumerate() {
                if f.kind == Inputs {
                    assert_eq!(i, c.fields.len() - 1, "{}", c.name);
                }
            }
        }
    }
}
