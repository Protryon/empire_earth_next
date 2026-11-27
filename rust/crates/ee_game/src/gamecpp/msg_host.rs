//! The game messages' host (was EEMessage.cpp's `EENetMsgHost`): what ee_sim's message bodies (netmsg bodies.rs,
//! `EE_NETMSG_OP_*`) ask of the world, the units, the players and the options. Each C++ member or call is a
//! one-statement case of EEMessage.cpp's `EECore11OpC4` (the `C11Nm` ops, `p` the host's call); the bodies' compound
//! answers are gamecpp message.rs's (called here directly).

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::netmsg::bodies::*;
use ee_sim::netmsg::decisions::ee_netmsg_capture_decision;

use super::message::*;
use super::{h, hp, rec, O, P};
use crate::app::game_options::EeAppOptionsFields;
use ee_sim::world::stats_fields::EeUnitsUnitClassUpgradeFields;
use ee_sim::world::unit_class_fields::EeUnitsUnitClassLinks;
use ee_sim::world::view::{PlayerView, UnitView, WorldRead, WorldView};

/// A unit's fact (`EE_NETMSG_UNIT_*`) as its op; None for another.
fn unit_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_UNIT_LAND_BUILDING => O::C11NmUnitLandBuilding,
        EE_NETMSG_UNIT_AS_COMPLEX => O::C11NmUnitAsComplex,
        EE_NETMSG_UNIT_IN_SET => O::C11NmUnitInSet,
        _ => return None,
    })
}

/// A player's fact (`EE_NETMSG_PLAYER_*`).
fn player_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_PLAYER_TRADE_PRICE => O::C11NmPlayerTradePrice,
        _ => return None,
    })
}

/// The world's fact (`EE_NETMSG_WORLD_*`).
fn world_fact(which: i64) -> Option<O> {
    Some(match which {
        EE_NETMSG_WORLD_COLLISION_SPHERE_MODE => O::RoCollisionSphereMode,
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

/// A class's related classes (its `EeUnitsUnitClassLinks` record, at the world layout's offset).
fn class_links<'a>(uc: P) -> &'a EeUnitsUnitClassLinks {
    // SAFETY: a live class's record, read before the next C++ call
    unsafe { &*((uc + ee_sim::world::layout::layout().class_links as usize) as *const EeUnitsUnitClassLinks) }
}

/// A unit's facts its records hold (the unit, its complex unit's and its class's plain members); None for the
/// others.
fn unit_record_fact(which: i64, u: P) -> Option<i64> {
    let class = || ucf(uf(u).mUnitClass as P);
    Some(match which {
        EE_NETMSG_UNIT_HIT_POINTS => uf(u).mHitPoints as i64,
        EE_NETMSG_UNIT_TYPE => class().mUnitType as i64,
        EE_NETMSG_UNIT_CATEGORY => class().mCategory as i64,
        EE_NETMSG_UNIT_MAX_HIT_POINTS => {
            // SAFETY: the unit's upgrade's record (mStats), read before the next C++ call
            let stats = unsafe { &*rec::<EeUnitsUnitClassUpgradeFields>(O::DcStats, u) };
            stats.mMaxHitPoints as i64
        }
        EE_NETMSG_UNIT_CAN_DISBAND => class().mCanDisband as i64,
        EE_NETMSG_UNIT_IS_PACKED => cf(u).mIsPacked as i64,
        EE_NETMSG_UNIT_FLIGHT_TIME => cf(u).mFlightTime as i64,
        EE_NETMSG_UNIT_CAN_CHANGE_BEHAVIOR => class().mCanChangeBehavior as i64,
        EE_NETMSG_UNIT_COMPLEX => uf(u).mComplexUnit as P as i64,
        EE_NETMSG_UNIT_PAID_FOR => cf(u).mIsPaidFor as i64,
        EE_NETMSG_UNIT_BUILT => cf(u).mIsBuilt as i64,
        EE_NETMSG_UNIT_DOMAINS => class().mDomains as i64,
        EE_NETMSG_UNIT_CAN_DELETE => (cu(ee_sim::units::cu_world::EE_UNITS_CU_DO_CAN_DELETE, u, 0) != 0) as i64,
        EE_NETMSG_UNIT_PACKED_CLASS => class_links(uf(u).mUnitClass as P).mPackedClass as P as i64,
        EE_NETMSG_UNIT_DEATH_SPAWN => !class_links(uf(u).mUnitClass as P).mDeathSpawnClass.is_null() as i64,
        EE_NETMSG_UNIT_TILE_X => rts(u).mTileX as i64,
        EE_NETMSG_UNIT_TILE_Y => rts(u).mTileY as i64,
        EE_NETMSG_UNIT_TILE_NODE | EE_NETMSG_UNIT_OWNER | EE_NETMSG_UNIT_NEXT => {
            // the RTSUnit's pointer members (the world layout's: the view's reads)
            // SAFETY: the host's call holds the world; `u` a live unit
            let token = unsafe { WorldRead::begin() };
            // SAFETY: as above
            let v = unsafe { UnitView::of(&token, u as u64) }?;
            match which {
                EE_NETMSG_UNIT_TILE_NODE => v.has_tile_node() as i64,
                EE_NETMSG_UNIT_OWNER => v.player().map_or(0, |pl| pl.handle() as i64),
                _ => v.next().map_or(0, |n| n.handle() as i64),
            }
        }
        _ => return None,
    })
}

/// A player's facts its records hold (`index` an array member's index); None for the others.
fn player_record_fact(which: i64, pl: P, index: i64) -> Option<i64> {
    let i = index as usize;
    Some(match which {
        EE_NETMSG_PLAYER_OBSERVER => prts(pl).mObserverMode as i64,
        EE_NETMSG_PLAYER_CIV_CHOSEN => psaved(pl).mCivilizationChosen as i64,
        EE_NETMSG_PLAYER_SLOT => prts(pl).mSlot as i64,
        EE_NETMSG_PLAYER_CHAT_TYPE => psaved(pl).mChatType as i64,
        EE_NETMSG_PLAYER_ENEMY => psaved(pl).mEnemies[i] as i64,
        EE_NETMSG_PLAYER_SENDS_CHAT => psaved(pl).mSendChatMessage[i] as i64,
        EE_NETMSG_PLAYER_ID => prts(pl).mPlayerID as i64,
        EE_NETMSG_PLAYER_RESOURCE => psaved(pl).mResourceCaps[i] as i64,
        EE_NETMSG_PLAYER_FIRST_UNIT => {
            // SAFETY: the host's call holds the world; `pl` a live player
            let token = unsafe { WorldRead::begin() };
            // SAFETY: as above
            unsafe { PlayerView::of(&token, pl as u64) }?.first_unit().map_or(0, |u| u.handle() as i64)
        }
        _ => return None,
    })
}

/// The world's facts its members hold (the world view's reads); None for the others.
fn world_record_fact(which: i64, message: P) -> Option<i64> {
    // SAFETY: the host's call holds the world
    let token = unsafe { WorldRead::begin() };
    let w = WorldView::global(&token)?;
    Some(match which {
        EE_NETMSG_WORLD_NUM_PLAYERS => w.num_players() as c_ulong as i64,
        EE_NETMSG_WORLD_CURRENT_INDEX => w.current_player_index(),
        // the message's string's length (`mString.GetLength()`)
        EE_NETMSG_WORLD_STRING_LENGTH => h(O::StrLength, hp(O::MsgString, message, &[]), &[]),
        _ => return None,
    })
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
        EE_NETMSG_OP_SENDER => player_by_slot(h(O::MsgOrigin, m as P, &[])) as i64,
        EE_NETMSG_OP_PLAYER => h(O::WorldPlayer, 0, &[i[0]]),
        EE_NETMSG_OP_CURRENT => h(O::WorldCurrentPlayer, 0, &[]),
        EE_NETMSG_OP_GROUP => h(O::PlActiveGroup, a as P, &[]),
        EE_NETMSG_OP_SAVED_GROUP => h(O::PlSavedGroupAt, a as P, &[i[0]]),
        EE_NETMSG_OP_GROUP_COUNT => group_len(a as P) as i64,
        EE_NETMSG_OP_GROUP_UNIT => group_unit(a as P, i[0] as c_long) as i64,
        EE_NETMSG_OP_GROUP_FIRST => group_unit(a as P, 0) as i64,
        EE_NETMSG_OP_GROUP_CONTAINS => group_contains(a as P, b as P) as i64,
        EE_NETMSG_OP_UNIT => h(O::WorldUnit, 0, &[i[0]]),
        EE_NETMSG_OP_CLASS_FACT => {
            let class = ucf(a as P);
            if i[0] == EE_NETMSG_CLASS_TYPE {
                class.mUnitType as i64
            } else {
                class.mTileSize as i64
            }
        }
        EE_NETMSG_OP_GROUP_ADD => {
            group_do(a as P, 0, b as P);
            0
        }
        EE_NETMSG_OP_GROUP_CLEAR => {
            group_do(a as P, 2, 0);
            0
        }
        EE_NETMSG_OP_KILL => {
            h(O::C11NmKill, call, &[]);
            0
        }
        EE_NETMSG_OP_UNIT_FACT => unit_record_fact(i[0], b as P).unwrap_or_else(|| fact(unit_fact(i[0]), call)),
        EE_NETMSG_OP_PLAYER_FACT => {
            player_record_fact(i[0], a as P, i[1]).unwrap_or_else(|| fact(player_fact(i[0]), call))
        }
        EE_NETMSG_OP_WORLD_FACT => {
            world_record_fact(i[0], m as P).or_else(|| net_fact(i[0])).unwrap_or_else(|| fact(world_fact(i[0]), call))
        }
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
        assert_eq!(world_fact(EE_NETMSG_WORLD_COLLISION_SPHERE_MODE), Some(O::RoCollisionSphereMode));
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
