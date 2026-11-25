//! The game messages' host (was EEMessage.cpp's `EENetMsgHost`): what ee_sim's message bodies (netmsg bodies.rs,
//! `EE_NETMSG_OP_*`) ask of the world, the units, the players and the options. Each C++ member or call is a
//! one-statement case of EEMessage.cpp's `EECore11OpC4` (the `C11Nm` ops, `p` the host's call); the bodies' compound
//! answers are gamecpp message.rs's (called here directly).

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::netmsg::bodies::*;
use ee_sim::netmsg::decisions::ee_netmsg_capture_decision;

use super::message::*;
use super::{h, rec, O, P};
use crate::app::game_options::EeAppOptionsFields;

/// A unit's fact (`EE_NETMSG_UNIT_*`) as its op; None for another.
fn unit_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_UNIT_HIT_POINTS => O::C11NmUnitHitPoints,
        EE_NETMSG_UNIT_TYPE => O::C11NmUnitType,
        EE_NETMSG_UNIT_CATEGORY => O::C11NmUnitCategory,
        EE_NETMSG_UNIT_MAX_HIT_POINTS => O::C11NmUnitMaxHitPoints,
        EE_NETMSG_UNIT_CAN_DISBAND => O::C11NmUnitCanDisband,
        EE_NETMSG_UNIT_IS_PACKED => O::C11NmUnitIsPacked,
        EE_NETMSG_UNIT_PACKED_CLASS => O::C11NmUnitPackedClass,
        EE_NETMSG_UNIT_FLIGHT_TIME => O::C11NmUnitFlightTime,
        EE_NETMSG_UNIT_DEATH_SPAWN => O::C11NmUnitDeathSpawn,
        EE_NETMSG_UNIT_CAN_CHANGE_BEHAVIOR => O::C11NmUnitCanChangeBehavior,
        EE_NETMSG_UNIT_COMPLEX => O::C11NmUnitComplex,
        EE_NETMSG_UNIT_TILE_NODE => O::C11NmUnitTileNode,
        EE_NETMSG_UNIT_OWNER => O::C11NmUnitOwner,
        EE_NETMSG_UNIT_LAND_BUILDING => O::C11NmUnitLandBuilding,
        EE_NETMSG_UNIT_PAID_FOR => O::C11NmUnitPaidFor,
        EE_NETMSG_UNIT_BUILT => O::C11NmUnitBuilt,
        EE_NETMSG_UNIT_CAN_DELETE => O::C11NmUnitCanDelete,
        EE_NETMSG_UNIT_TILE_X => O::C11NmUnitTileX,
        EE_NETMSG_UNIT_TILE_Y => O::C11NmUnitTileY,
        EE_NETMSG_UNIT_DOMAINS => O::C11NmUnitDomains,
        EE_NETMSG_UNIT_NEXT => O::C11NmUnitNext,
        EE_NETMSG_UNIT_AS_COMPLEX => O::C11NmUnitAsComplex,
        EE_NETMSG_UNIT_IN_SET => O::C11NmUnitInSet,
        _ => return None,
    })
}

/// A player's fact (`EE_NETMSG_PLAYER_*`).
fn player_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_PLAYER_OBSERVER => O::C11NmPlayerObserver,
        EE_NETMSG_PLAYER_CIV_CHOSEN => O::C11NmPlayerCivChosen,
        EE_NETMSG_PLAYER_SLOT => O::C11NmPlayerSlot,
        EE_NETMSG_PLAYER_CHAT_TYPE => O::C11NmPlayerChatType,
        EE_NETMSG_PLAYER_ENEMY => O::C11NmPlayerEnemy,
        EE_NETMSG_PLAYER_SENDS_CHAT => O::C11NmPlayerSendsChat,
        EE_NETMSG_PLAYER_ID => O::C11NmPlayerId,
        EE_NETMSG_PLAYER_RESOURCE => O::C11NmPlayerResource,
        EE_NETMSG_PLAYER_FIRST_UNIT => O::C11NmPlayerFirstUnit,
        EE_NETMSG_PLAYER_TRADE_PRICE => O::C11NmPlayerTradePrice,
        _ => return None,
    })
}

/// The world's fact (`EE_NETMSG_WORLD_*`).
fn world_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_WORLD_NUM_PLAYERS => O::C11NmWorldNumPlayers,
        EE_NETMSG_WORLD_CURRENT_INDEX => O::C11NmWorldCurrentIndex,
        EE_NETMSG_WORLD_STRING_LENGTH => O::C11NmWorldStringLength,
        EE_NETMSG_WORLD_COLLISION_SPHERE_MODE => O::C11NmWorldCollisionSphereMode,
        _ => return None,
    })
}

/// The world's facts about `gCommunications` (its engine handles and plain tails): None for the others.
fn net_fact(which: i64) -> Option<i64> {
    use ee_net::net::{ffi_client as cl, ffi_server as sv};
    let comms = super::net::global();
    let object = super::globals::get!(gCommunications) as P;
    let server = !comms.server.is_null();
    let playing = 3; // kRecordModePlaying
    Some(match which {
        EE_NETMSG_WORLD_EXECUTING_SELECTION => {
            ((comms.is_client() && super::session_objs::client_tail(object).is_executing_selection)
                || (server && super::session_objs::server_tail(object).is_executing_selection)) as i64
        }
        EE_NETMSG_WORLD_IS_SERVER => server as i64,
        EE_NETMSG_WORLD_HAS_COMMUNICATIONS => super::net::present() as i64,
        EE_NETMSG_WORLD_LOCAL_SEND_SLOT => {
            if comms.is_client() {
                cl::ee_net_cl_my_send_to_slot(comms.client) as i64
            } else {
                0xfe
            }
        }
        EE_NETMSG_WORLD_PLAYBACK => {
            ((comms.is_client() && cl::ee_net_cl_record_playback_mode(comms.client) == playing)
                || (server && sv::ee_net_sv_record_playback_mode(comms.server) == playing)) as i64
        }
        EE_NETMSG_WORLD_LOADED_HUMANS => {
            humans(comms) - super::session_objs::server_tail(object).num_dropped_loaders as i64
        }
        EE_NETMSG_WORLD_LOAD_RESPONSES => super::session_objs::server_tail(object).num_load_responses as i64,
        EE_NETMSG_WORLD_HUMANS => humans(comms),
        _ => return None,
    })
}

/// `GetNumHumanPlayers`.
fn humans(comms: super::net::Net) -> i64 {
    comms.get(ee_net::net::ffi_comms::EeNetCommsField::NumHumanPlayers) as u8 as i64
}

/// A fact's answer, 0 for an unknown fact.
fn fact(op: Option<O>, call: P) -> i64 {
    op.map_or(0, |op| h(op, call, &[]))
}

/// Option `which` of the lobby's game options set from the message's value (`EE_NETMSG_OP_OPTION`; 2 the map type,
/// the message's string).
fn option(o: &mut EeAppOptionsFields, which: i64, v: i64) -> bool {
    match which {
        0 => o.mLoadType = v as c_ulong,
        1 => o.mMapSize = v as c_ulong,
        3 => o.mResources = v as c_ulong,
        4 => o.mEpochStart = v as c_long,
        5 => o.mEpochEnd = v as c_long,
        6 => o.mUnitLimit = v as c_ulong,
        7 => o.mWondersForVictory = v as c_ulong,
        8 => o.mVariant = v as c_ulong,
        9 => o.mDifficulty = v as c_ulong,
        10 => o.mGameSpeed = v as c_ulong,
        11 => o.mRecordGames = v != 0,
        12 => o.mAllowCustomCivs = v != 0,
        13 => o.mRevealMap = v != 0,
        14 => o.mLockTeams = v != 0,
        15 => o.mLockSpeed = v != 0,
        16 => o.mCheatCodes = v != 0,
        17 => o.mVictory = v != 0,
        _ => return false,
    }
    true
}

/// The host ee_sim's bodies call (`EeNetMsgHostFn`).
///
/// # Safety
/// `c` is the body's live call; its objects are the world's.
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_netmsg_host(op: u32, c: *mut c_void) -> i64 {
    let call = c as P;
    let c = &*(c as *const EeNetMsgCall);
    let (m, a, b) = (c.message as *mut c_void, c.a as *mut c_void, c.b as *mut c_void);
    let i = c.i;
    match op {
        EE_NETMSG_OP_SENDER => h(O::C11NmSender, call, &[]),
        EE_NETMSG_OP_PLAYER => h(O::C11NmPlayer, call, &[]),
        EE_NETMSG_OP_CURRENT => h(O::C11NmCurrent, call, &[]),
        EE_NETMSG_OP_GROUP => h(O::C11NmGroup, call, &[]),
        EE_NETMSG_OP_SAVED_GROUP => h(O::C11NmSavedGroup, call, &[]),
        EE_NETMSG_OP_GROUP_COUNT => h(O::C11NmGroupCount, call, &[]),
        EE_NETMSG_OP_GROUP_UNIT => h(O::C11NmGroupUnit, call, &[]),
        EE_NETMSG_OP_GROUP_FIRST => h(O::C11NmGroupFirst, call, &[]),
        EE_NETMSG_OP_GROUP_CONTAINS => h(O::C11NmGroupContains, call, &[]),
        EE_NETMSG_OP_UNIT => h(O::C11NmUnit, call, &[]),
        EE_NETMSG_OP_CLASS_FACT => h(O::C11NmClassFact, call, &[]),
        EE_NETMSG_OP_GROUP_ADD => {
            h(O::C11NmGroupAdd, call, &[]);
            0
        }
        EE_NETMSG_OP_GROUP_CLEAR => {
            h(O::C11NmGroupClear, call, &[]);
            0
        }
        EE_NETMSG_OP_KILL => {
            h(O::C11NmKill, call, &[]);
            0
        }
        EE_NETMSG_OP_UNIT_FACT => fact(unit_fact(i[0]), call),
        EE_NETMSG_OP_PLAYER_FACT => fact(player_fact(i[0]), call),
        EE_NETMSG_OP_WORLD_FACT => net_fact(i[0]).unwrap_or_else(|| fact(world_fact(i[0]), call)),
        EE_NETMSG_OP_DECISION => {
            ee_netmsg_capture_decision(i[0] as u32, c.i.as_ptr().add(2), i[1] as u32);
            0
        }
        EE_NETMSG_OP_CHAT_TEXT => {
            ee_gamecpp_msg_chat_text(i[0], i[1], i[2]);
            0
        }
        EE_NETMSG_OP_SOUND => {
            ee_gamecpp_msg_sound(i[0]);
            0
        }
        EE_NETMSG_OP_BODY => {
            ee_gamecpp_msg_body(i[0], m);
            0
        }
        EE_NETMSG_OP_SELECT_LIST => {
            ee_gamecpp_msg_select_list(a, i[0], b as *const i32, i[1]);
            0
        }
        EE_NETMSG_OP_SELECT => {
            ee_gamecpp_msg_select(a, i[0], i[1]);
            0
        }
        EE_NETMSG_OP_RESOURCE => {
            ee_gamecpp_msg_resource(a, i[0], i[1], i[2], i[3]);
            0
        }
        EE_NETMSG_OP_DISBAND => {
            ee_gamecpp_msg_disband(a, b, c.f[0]);
            0
        }
        EE_NETMSG_OP_DELETE_REFUND => {
            ee_gamecpp_msg_delete_refund(b);
            0
        }
        EE_NETMSG_OP_UNIT_ACT => {
            ee_gamecpp_msg_unit_act(c.i.as_ptr(), a, b);
            0
        }
        EE_NETMSG_OP_PLAYER_ACT => {
            ee_gamecpp_msg_player_act(c.i.as_ptr(), a);
            0
        }
        EE_NETMSG_OP_WORLD_ACT => {
            ee_gamecpp_msg_world_act(m, c.i.as_ptr(), c.f.as_ptr(), a, b);
            0
        }
        EE_NETMSG_OP_OPTION => {
            if i[0] == 2 {
                h(O::C11NmMapType, call, &[]);
            } else {
                option(&mut *rec::<EeAppOptionsFields>(O::LobbyOptions, 0), i[0], i[1]);
            }
            0
        }
        EE_NETMSG_OP_SELECTION => ee_gamecpp_msg_selection(c.i.as_ptr(), a),
        EE_NETMSG_OP_CAN_PLACE => ee_gamecpp_msg_can_place(a, i[0], i[1], i[2], i[3], i[4]) as i64,
        _ => 0,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_facts_map_to_their_ops() {
        assert_eq!(unit_fact(EE_NETMSG_UNIT_IN_SET), Some(O::C11NmUnitInSet));
        assert_eq!(player_fact(EE_NETMSG_PLAYER_TRADE_PRICE), Some(O::C11NmPlayerTradePrice));
        assert_eq!(world_fact(EE_NETMSG_WORLD_NUM_PLAYERS), Some(O::C11NmWorldNumPlayers));
        assert_eq!(world_fact(EE_NETMSG_WORLD_HUMANS), None);
        assert_eq!(world_fact(EE_NETMSG_WORLD_OPTIONS_LOCK), None);
        assert_eq!(fact(None, 0), 0);
    }

    #[test]
    fn the_options_are_set_as_the_cpp_set_them() {
        let mut o = EeAppOptionsFields::default();
        assert!(option(&mut o, 4, -1));
        assert_eq!(o.mEpochStart, -1);
        assert!(option(&mut o, 13, 5));
        assert!(o.mRevealMap);
        assert!(option(&mut o, 0, 2));
        assert_eq!(o.mLoadType, 2);
        assert!(!option(&mut o, 2, 1));
        assert!(!option(&mut o, 18, 1));
    }
}
