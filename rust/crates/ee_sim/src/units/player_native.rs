//! The player's body operations the Rust does itself (the player/tech lane, round 2): the ones of
//! `ee_units_player_body_host` (EEPlayer.cpp) that only read or wrote a pointer member of the player, a field of its
//! Rust records (`EeUnitsPlayerSaved`, `EeUnitsPlayerFields`) or called another Rust export. The game's
//! [`PlayerBodyHost`](crate::units::player_body::PlayerBodyHost) asks [`body`] before calling the C++; the bodies'
//! unit tests' hosts answer every operation themselves, so the bodies and their tests are unchanged.

use crate::units::player_body::*;
use crate::units::player_rest::{
    EE_UNITS_PLAYER_BODY_ATC_UPDATE, EE_UNITS_PLAYER_BODY_CLOAKED, EE_UNITS_PLAYER_BODY_DETACHED,
    EE_UNITS_PLAYER_BODY_GAME_TIME, EE_UNITS_PLAYER_BODY_SET_CLOAKED, EE_UNITS_PLAYER_BODY_SET_DETACHED,
    EE_UNITS_PLAYER_BODY_TOWN_CONTINENT,
};
use crate::units::town_split::{
    EE_UNITS_PLAYER_BODY_TOWN_COUNTER, EE_UNITS_PLAYER_BODY_TOWN_ID, EE_UNITS_PLAYER_BODY_TOWN_NEAR_EDGE,
    EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER, EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT,
    EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER, EE_UNITS_PLAYER_BODY_TOWN_SET_ID, EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_EDGE,
    EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_WATER,
};
use crate::world::layout::layout;
use crate::world::player_fields::{EeUnitsPlayerFields, EeUnitsPlayerSaved};
use std::ffi::c_void;
use std::os::raw::c_long;

/// The pointer member at byte offset `offset` of player `p`.
fn slot(p: u64, offset: u64) -> *mut u64 {
    (p as usize + offset as usize) as *mut u64
}

/// Player `p`'s records.
fn saved(p: u64) -> *mut EeUnitsPlayerSaved {
    (p as usize + layout().player_saved as usize) as *mut EeUnitsPlayerSaved
}
fn fields(p: u64) -> *mut EeUnitsPlayerFields {
    (p as usize + layout().player_fields as usize) as *mut EeUnitsPlayerFields
}

/// Complex unit `u`'s record (`u` an `RTSUnit*` address: the complex unit's own).
fn complex(u: u64) -> *mut crate::world::unit_fields::EeUnitsComplexUnitFields {
    (u as usize + layout().complex_unit_fields as usize) as *mut _
}

/// Town `t`'s record (an `EETown*`).
fn town(t: u64) -> *mut crate::cp::town::EeTownFields {
    (t as usize + layout().town_fields as usize) as *mut _
}

/// Town `t`'s counter `i` (the towns' save's order: the value (a `long`), the walls, the buildings, the wonders, the
/// docks, the airports, the units, the reach; the original `EETownCounter`).
///
/// # Safety
/// `t` is a live town; `i` is 0..8.
unsafe fn town_counter(t: u64, i: i64) -> *mut c_long {
    // SAFETY: the caller's contract
    let f = unsafe { &mut *town(t) };
    match i {
        0 => &mut f.mValue as *mut _ as *mut c_long,
        1 => &mut f.mNumWalls,
        2 => &mut f.mL80,
        3 => &mut f.mNumWonders,
        4 => &mut f.mNumDocks,
        5 => &mut f.mNumAirports,
        6 => &mut f.mNumUnits,
        _ => &mut f.mReach,
    }
}

/// Tech tree node `n`'s record (an `EETechTreeNode*`).
fn node(n: u64) -> *mut crate::tech::nodes::EeTechNodeFields {
    (n as usize + layout().node_fields as usize) as *mut _
}

/// The answer of body operation `op` of player `p` (None: the C++ does it).
///
/// # Safety
/// `p` is a live `EEPlayer` and `a`, `b`, `c` the operation's live objects and values, as the C++ host takes them.
pub unsafe fn body(op: i32, p: u64, a: u64, b: i64, c: i64) -> Option<i64> {
    let l = crate::world::layout::get()?;
    // SAFETY (every block below): the caller's contract; the offsets are the build's (EEWorldLayout.cpp), the records
    // the player's own
    unsafe {
        let set = |offset: u64, v: u64| *slot(p, offset) = v;
        let atc = |i: i64| slot(p, l.player_air_traffic + 8 * i as u64);
        match op {
            EE_UNITS_PLAYER_BODY_SET_ACTIVE => set(l.player_active_group, a),
            EE_UNITS_PLAYER_BODY_SET_SELECTED => set(l.player_selected_group, a),
            EE_UNITS_PLAYER_BODY_SET_HOT_KEY => set(l.player_groups + 8 * b as u64, a),
            EE_UNITS_PLAYER_BODY_SELECTED_VIEWED => return Some(*slot(p, l.player_selected_viewed_unit) as i64),
            EE_UNITS_PLAYER_BODY_SET_SELECTED_VIEWED => set(l.player_selected_viewed_unit, a),
            EE_UNITS_PLAYER_BODY_SET_VIEWED => set(l.player_viewed_unit, a),
            EE_UNITS_PLAYER_BODY_LAST_UNIT => return Some(*slot(p, l.player_last_unit) as i64),
            EE_UNITS_PLAYER_BODY_ATC_SET => *atc(b) = a,
            EE_UNITS_PLAYER_BODY_ATC_DELETE => crate::units::atc::ee_units_atc_delete(*atc(b) as usize as *mut _),
            EE_UNITS_PLAYER_BODY_ATC_REPAIR => {
                return Some(crate::units::atc::ee_units_atc_repair_pointers(*atc(b) as usize as *mut _) as i64)
            }
            EE_UNITS_PLAYER_BODY_ATC_UPDATE => crate::units::atc::ee_units_atc_update(*atc(b) as usize as *mut _),
            EE_UNITS_PLAYER_BODY_STORE_TABLE => crate::units::us_fill::game::ee_units_store_player_table(p),
            EE_UNITS_PLAYER_BODY_HEALED_CLEAR => set(l.player_healed_unit_set, 0),
            EE_UNITS_PLAYER_BODY_TECH_NULL => set(l.player_technologies, 0),
            EE_UNITS_PLAYER_BODY_COMPUTER_CLEAR => set(l.player_computer_player, 0),
            EE_UNITS_PLAYER_BODY_HUMAN_CLEAR => set(l.player_human_player, 0),
            EE_UNITS_PLAYER_BODY_QUEUED_NEW => (*fields(p)).mQueued = crate::units::player::ee_units_queued_new(),
            EE_UNITS_PLAYER_BODY_QUEUED_FREE => crate::units::player::ee_units_queued_free((*fields(p)).mQueued),
            EE_UNITS_PLAYER_BODY_WONDER_STATE_NEW => {
                (*fields(p)).mWonderState = crate::units::player_units::ee_units_wonders_new()
            }
            EE_UNITS_PLAYER_BODY_WONDER_STATE_FREE => {
                crate::units::player_units::ee_units_wonders_free((*fields(p)).mWonderState)
            }
            EE_UNITS_PLAYER_BODY_LISTS_NEW => {
                set(l.player_lists, crate::units::player_lists::ee_units_lists_new() as u64)
            }
            EE_UNITS_PLAYER_BODY_LISTS_FREE => {
                crate::units::player_lists::ee_units_lists_free(*slot(p, l.player_lists) as usize as *mut _)
            }
            EE_UNITS_PLAYER_BODY_GROUP_LISTS_NEW => {
                set(l.player_group_lists, crate::units::group_obj::ee_units_player_groups_new() as u64)
            }
            EE_UNITS_PLAYER_BODY_GROUP_LISTS_FREE => {
                crate::units::group_obj::ee_units_player_groups_free(*slot(p, l.player_group_lists) as usize as *mut _)
            }
            EE_UNITS_PLAYER_BODY_BB44_CLEAR => (*fields(p)).mBB44 = false,
            EE_UNITS_PLAYER_BODY_POPULATION_BONUS_CLEAR => (*fields(p)).mPopulationLimitBonus = 0,
            EE_UNITS_PLAYER_BODY_TRIBUTE_RESET => {
                (*fields(p)).mTributeFee = crate::units::player::EE_UNITS_PLAYER_TRIBUTE_FEE
            }
            EE_UNITS_PLAYER_BODY_DAMAGE_ADD => (*fields(p)).mDamageFrom[b as usize] += c as c_long,
            EE_UNITS_PLAYER_BODY_SET_COMBAT => (*saved(p)).mCombatState = b as _,
            EE_UNITS_PLAYER_BODY_CAPITOLS => return Some((*saved(p)).mNumCapitols as i64),
            EE_UNITS_PLAYER_BODY_TOWN_ID => return Some((*town(a)).mTownID as i64),
            EE_UNITS_PLAYER_BODY_TOWN_SET_ID => (*town(a)).mTownID = b as _,
            EE_UNITS_PLAYER_BODY_TOWN_COUNTER => return Some(*town_counter(a, b) as i64),
            EE_UNITS_PLAYER_BODY_TOWN_SET_COUNTER => *town_counter(a, b) = c as _,
            EE_UNITS_PLAYER_BODY_TOWN_NEAR_WATER => return Some(i64::from((*town(a)).mNearWater)),
            EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_WATER => (*town(a)).mNearWater = b != 0,
            EE_UNITS_PLAYER_BODY_TOWN_NEAR_EDGE => return Some(i64::from((*town(a)).mNearMapEdge)),
            EE_UNITS_PLAYER_BODY_TOWN_SET_NEAR_EDGE => (*town(a)).mNearMapEdge = b != 0,
            EE_UNITS_PLAYER_BODY_TOWN_CONTINENT => return Some(i64::from((*town(a)).mContinent)),
            EE_UNITS_PLAYER_BODY_TOWN_SET_CONTINENT => (*town(a)).mContinent = b as u16,
            EE_UNITS_PLAYER_BODY_DETACHED => return Some(i64::from((*complex(a)).mIsDetachedFromAI)),
            EE_UNITS_PLAYER_BODY_SET_DETACHED => (*complex(a)).mIsDetachedFromAI = b != 0,
            EE_UNITS_PLAYER_BODY_DISABLED => return Some(i64::from((*complex(a)).mIsDisabled)),
            EE_UNITS_PLAYER_BODY_CLOAKED => return Some((*complex(a)).mCloaked as i64),
            EE_UNITS_PLAYER_BODY_SET_CLOAKED => (*complex(a)).mCloaked = b as _,
            EE_UNITS_PLAYER_BODY_SET_CAMOUFLAGE_TIMER => (*complex(a)).mCamouflageTimer = b as _,
            EE_UNITS_PLAYER_BODY_CLEAR_U32C => (*complex(a)).mU32C = 0,
            EE_UNITS_PLAYER_BODY_NEXT_UNIT
            | EE_UNITS_PLAYER_BODY_COMPLEX_OF
            | EE_UNITS_PLAYER_BODY_TILE_X
            | EE_UNITS_PLAYER_BODY_TILE_Y
            | EE_UNITS_PLAYER_BODY_SIGHT_ZERO
            | EE_UNITS_PLAYER_BODY_IS_LOCAL
            | EE_UNITS_PLAYER_BODY_GAME_TIME
            | EE_UNITS_PLAYER_BODY_CLASS_COUNT => {
                let t = crate::world::WorldRead::begin();
                let unit = || crate::world::UnitView::of(&t, a);
                return Some(match op {
                    EE_UNITS_PLAYER_BODY_NEXT_UNIT => {
                        let next =
                            if a == 0 { crate::world::PlayerView::of(&t, p)?.first_unit() } else { unit()?.next() };
                        next.map_or(0, |u| u.handle() as i64)
                    }
                    EE_UNITS_PLAYER_BODY_COMPLEX_OF => unit()?.complex_unit_handle() as i64,
                    EE_UNITS_PLAYER_BODY_TILE_X => unit()?.tile_xy().0,
                    EE_UNITS_PLAYER_BODY_TILE_Y => unit()?.tile_xy().1,
                    EE_UNITS_PLAYER_BODY_SIGHT_ZERO => {
                        (*complex(unit()?.complex_unit_handle())).mSight = 0.0;
                        0
                    }
                    EE_UNITS_PLAYER_BODY_GAME_TIME => crate::world::WorldView::global(&t)?.game_time(),
                    EE_UNITS_PLAYER_BODY_IS_LOCAL => {
                        let w = crate::world::WorldView::global(&t)?;
                        i64::from(w.current_player().is_some_and(|c| c.handle() == p))
                    }
                    _ => crate::world::ClassView::count(&t) as i64,
                });
            }
            EE_UNITS_PLAYER_BODY_IS_SELF | EE_UNITS_PLAYER_BODY_ENEMY_OF => {
                let t = crate::world::WorldRead::begin();
                let other = crate::world::WorldView::global(&t)?.player(b as usize);
                return Some(if op == EE_UNITS_PLAYER_BODY_IS_SELF {
                    i64::from(other.is_some_and(|o| o.handle() == p))
                } else {
                    (*saved(p)).mEnemies[other.map_or(0, |o| o.id()) as usize] as i64
                });
            }
            EE_UNITS_PLAYER_BODY_NEXT_COMPLEX => {
                return Some(game::ee_units_player_body(
                    p as *mut c_void,
                    EE_UNITS_PLAYER_DO_NEXT_COMPLEX,
                    a,
                    0,
                    0,
                    0.0,
                ))
            }
            EE_UNITS_PLAYER_BODY_UNLOAD_ALL_MEDIA => {
                game::ee_units_player_body(p as *mut c_void, EE_UNITS_PLAYER_DO_UNLOAD_ALL_MEDIA, 0, 0, 0, 0.0);
            }
            _ => return None,
        }
    }
    Some(0)
}

/// The answer of player host operation `op` (`EEPlayerHost`'s) of player `p` that is a call of another of the
/// player's Rust bodies, or a read of a unit or a unit class (None: the C++ does it).
///
/// # Safety
/// As [`body`]; `io` the operation's operands.
pub unsafe fn player(op: i32, p: u64, io: &mut crate::units::player::EeUnitsPlayerIo) -> Option<i64> {
    use crate::units::player_diplomacy::EE_UNITS_PLAYER_RESET_WONDER_TIMERS;
    use crate::units::player_rest::{
        game::ee_units_player_rest, EE_UNITS_PLAYER_REST_CLEAR_FOOTPRINT, EE_UNITS_PLAYER_REST_RESET_WONDER_TIMERS,
    };
    use crate::units::player_split::*;
    use crate::units::player_split_more::*;
    use crate::units::player_units::{EE_UNITS_PLAYER_CLEAR_FOOTPRINT, EE_UNITS_PLAYER_NEXT_COMPLEX};
    use crate::world::{ClassView, UnitView, WorldRead, WorldView};
    crate::world::layout::get()?;
    let a0 = io.i[0];
    let pv = p as *mut c_void;
    // SAFETY (every block below): the caller's contract; the C++ host read `a[0]` as the same unit, class or player
    unsafe {
        match op {
            EE_UNITS_PLAYER_NEXT_COMPLEX => {
                return Some(game::ee_units_player_body(pv, EE_UNITS_PLAYER_DO_NEXT_COMPLEX, a0 as u64, 0, 0, 0.0))
            }
            EE_UNITS_PLAYER_X_CAMOUFLAGE_ON | EE_UNITS_PLAYER_X_CAMOUFLAGE_OFF => {
                let on = i64::from(op == EE_UNITS_PLAYER_X_CAMOUFLAGE_ON);
                game::ee_units_player_body(pv, EE_UNITS_PLAYER_DO_CAMOUFLAGE, 0, on, 0, 0.0);
                return Some(0);
            }
            EE_UNITS_PLAYER_RESET_WONDER_TIMERS => {
                let other = if a0 != 0 { a0 as u64 } else { p };
                ee_units_player_rest(pv, EE_UNITS_PLAYER_REST_RESET_WONDER_TIMERS, other, 0, 0);
                return Some(0);
            }
            crate::units::player_diplomacy::EE_UNITS_PLAYER_UNVIEW => {
                unview(pv);
                return Some(0);
            }
            crate::units::player_diplomacy::EE_UNITS_PLAYER_STORE_TABLE => {
                crate::units::us_fill::game::ee_units_store_player_table(p);
                return Some(0);
            }
            EE_UNITS_PLAYER_X_CLOAKED_CLEAR => {
                (*complex(a0 as u64)).mCloaked = 0;
                return Some(0);
            }
            crate::units::player_units::EE_UNITS_PLAYER_NODE_IS_AVAILABLE => {
                return Some(i64::from((*node(a0 as u64)).mIsAvailable))
            }
            EE_UNITS_PLAYER_X_NODE_ALLOW => {
                (*node(a0 as u64)).mIsAllowed = io.i[1] != 0;
                return Some(0);
            }
            EE_UNITS_PLAYER_X_RESOURCE_CLEAR => {
                (*((a0 as usize + layout().unit_fields as usize)
                    as *mut crate::world::unit_fields::EeUnitsUnitFields))
                    .mResourceAmount = 0;
                return Some(0);
            }
            EE_UNITS_PLAYER_X_CLOAKED_NOW => {
                let t = WorldRead::begin();
                (*complex(a0 as u64)).mCloaked = WorldView::global(&t)?.game_time() as _;
                return Some(0);
            }
            EE_UNITS_PLAYER_X_TABLE_STEP | EE_UNITS_PLAYER_X_TABLE_COST_FACTOR | EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL => {
                let table = &*(a0 as usize as *const ee_data::tables::EeDataUpgradeFields);
                return Some(match op {
                    EE_UNITS_PLAYER_X_TABLE_MAX_LEVEL => table.mMaxLevel as i64,
                    _ => {
                        let f = table.mFactors[io.i[1] as usize];
                        io.f[0] = if op == EE_UNITS_PLAYER_X_TABLE_STEP { f.mOffset } else { f.mScale };
                        0
                    }
                });
            }
            EE_UNITS_PLAYER_CLEAR_FOOTPRINT => {
                ee_units_player_rest(pv, EE_UNITS_PLAYER_REST_CLEAR_FOOTPRINT, a0 as u64, 0, 0);
                return Some(0);
            }
            _ => {}
        }
    }
    // SAFETY: as above
    let t = unsafe { WorldRead::begin() };
    let unit = || unsafe { UnitView::of(&t, a0 as u64) };
    let class = || unsafe { ClassView::of(&t, a0 as u64) };
    let stats = || unsafe { crate::world::StatsView::of(&t, a0 as u64) };
    Some(match op {
        EE_UNITS_PLAYER_X_FIRST_UNIT => {
            let other = if a0 != 0 { a0 as u64 } else { p };
            // SAFETY: a live player (the C++ host's `theOther`)
            unsafe { crate::world::PlayerView::of(&t, other) }?.first_unit().map_or(0, |u| u.handle() as i64)
        }
        EE_UNITS_PLAYER_X_NEXT_UNIT => unit()?.next().map_or(0, |u| u.handle() as i64),
        EE_UNITS_PLAYER_X_IS_SELF => {
            i64::from(WorldView::global(&t)?.player(a0 as usize).is_some_and(|o| o.handle() == p))
        }
        EE_UNITS_PLAYER_X_TILE_X => unit()?.tile_xy().0,
        EE_UNITS_PLAYER_X_TILE_Y => unit()?.tile_xy().1,
        EE_UNITS_PLAYER_X_RESOURCE => unit()?.resource_amount(),
        EE_UNITS_PLAYER_X_LAND_BUILDING => i64::from(unit()?.class()?.land_building()),
        EE_UNITS_PLAYER_X_UPGRADE_UNIT_TYPE => stats()?.class()?.unit_type() as i64,
        EE_UNITS_PLAYER_X_UP_MAX_HIT_POINTS => stats()?.max_hit_points(),
        EE_UNITS_PLAYER_X_UP_FLIGHT_TIME => {
            let v: c_long = crate::world_field!(stats()?.fields(), mFlightTime);
            v as i64
        }
        EE_UNITS_PLAYER_X_UP_POWER => {
            let v: c_long = crate::world_field!(stats()?.fields(), mPower);
            v as i64
        }
        EE_UNITS_PLAYER_X_COMPLEX => unit()?.complex_unit_handle() as i64,
        EE_UNITS_PLAYER_X_CLASS_COUNT => ClassView::count(&t) as i64,
        EE_UNITS_PLAYER_X_CLASS_AT => ClassView::by_index(&t, a0 as usize).map_or(0, |c| c.handle() as i64),
        EE_UNITS_PLAYER_X_CLASS_TECH => class()?.technology(),
        EE_UNITS_PLAYER_X_CLASS_TYPE => class()?.unit_type() as i64,
        EE_UNITS_PLAYER_X_CLASS_AI_KIND => class()?.ai_kind() as i64,
        EE_UNITS_PLAYER_X_CLASS_WEIGHT => {
            let v: c_long = crate::world_field!(class()?.fields(), mWeight);
            v as i64
        }
        EE_UNITS_PLAYER_X_CLASS_FLIGHT_TIME => {
            let v: c_long = crate::world_field!(class()?.fields(), mFlightTime);
            v as i64
        }
        _ => return None,
    })
}

/// The answer of tick operation `op` (`EEPlayerTickOperation`'s) of player `p` that is a call of another Rust body
/// or a read of the player's units or tech tree (None: the C++ does it).
///
/// # Safety
/// As [`body`]; `io` the operation's operands (`io.i` a unit of the player, 0 for none).
pub unsafe fn tick(op: i32, p: u64, io: &mut crate::units::player_save::EeUnitsPlayerSaveIo) -> Option<i32> {
    use crate::units::player_rest::{game::ee_units_player_rest, EE_UNITS_PLAYER_REST_UPDATE_AIR_TRAFFIC};
    use crate::units::player_tick::*;
    use crate::world::{PlayerView, UnitView, WorldRead};
    crate::world::layout::get()?;
    let pv = p as *mut c_void;
    // SAFETY (every block): the caller's contract
    unsafe {
        match op {
            EE_UNITS_PLAYER_TICK_X_VALIDATE_GROUPS => crate::units::group_host::ee_units_groups_player(pv, 4),
            EE_UNITS_PLAYER_TICK_X_UNVIEW => unview(pv),
            EE_UNITS_PLAYER_TICK_X_SELECTED_UNVIEW => {
                game::ee_units_player_body(pv, EE_UNITS_PLAYER_DO_SET_SELECTED_VIEWED, 0, 0, 0, 0.0);
            }
            EE_UNITS_PLAYER_TICK_X_AIR => {
                ee_units_player_rest(pv, EE_UNITS_PLAYER_REST_UPDATE_AIR_TRAFFIC, 0, 0, 0);
            }
            EE_UNITS_PLAYER_TICK_GAME_TIME | EE_UNITS_PLAYER_TICK_TICK_MS => {
                let t = WorldRead::begin();
                let w = crate::world::WorldView::global(&t)?;
                io.i = if op == EE_UNITS_PLAYER_TICK_GAME_TIME { w.game_time() } else { w.tick_ms() };
            }
            EE_UNITS_PLAYER_TICK_X_EPOCH | EE_UNITS_PLAYER_TICK_X_NEXT_UNIT => {
                let t = WorldRead::begin();
                let me = PlayerView::of(&t, p)?;
                io.i = if op == EE_UNITS_PLAYER_TICK_X_EPOCH {
                    me.tech_tree()?.current_epoch()
                } else if io.i == 0 {
                    me.first_unit().map_or(0, |u| u.handle() as i64)
                } else {
                    UnitView::of(&t, io.i as u64)?.next().map_or(0, |u| u.handle() as i64)
                };
            }
            _ => return None,
        }
    }
    Some(0)
}

/// `UpdateSelection(NULL, true)` of player `p` (the selection's op 0).
///
/// # Safety
/// `p` is a live player.
unsafe fn unview(p: *mut c_void) {
    // SAFETY: the caller's contract
    unsafe {
        crate::units::group_host::ee_units_selection(p, 0, std::ptr::null_mut(), 0, std::ptr::null(), 0, true, false)
    };
}

/// The answer of save operation `op` (`EEPlayerSaveOperation`'s) of player `p` that is a call of another Rust export
/// or a read of the player or the unit classes (None: the C++ does it).
///
/// # Safety
/// As [`body`]; `io` the operation's operands.
pub unsafe fn save(
    op: i32,
    p: u64,
    index: u32,
    io: &mut crate::units::player_save::EeUnitsPlayerSaveIo,
) -> Option<i32> {
    use crate::units::player_save::{EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE, EE_UNITS_PLAYER_SAVE_TECH_PRESENT};
    use crate::units::player_save_split::*;
    use crate::world::{ClassView, WorldRead};
    let l = crate::world::layout::get()?;
    // a unit's save or load (LoadUnit, SaveUnit: no player): the class table's and the unit's reads
    if matches!(
        op,
        EE_UNITS_PLAYER_SAVE_X_CLASS_THERE
            | EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE
            | EE_UNITS_PLAYER_SAVE_X_CLASS_COMPLEX
            | EE_UNITS_PLAYER_SAVE_X_UNIT_QUEUED_DELETE
    ) {
        // SAFETY: the caller's contract (io.i the class index, or the loaded unit)
        let t = unsafe { WorldRead::begin() };
        let class = ClassView::by_index(&t, io.i as u64 as usize);
        return Some(match op {
            EE_UNITS_PLAYER_SAVE_X_CLASS_THERE => i32::from(class.is_some()),
            EE_UNITS_PLAYER_SAVE_X_CLASS_TYPE => class?.unit_type() as i32,
            EE_UNITS_PLAYER_SAVE_X_CLASS_COMPLEX => {
                let v: c_long = crate::world_field!(class?.fields(), mIsComplex);
                v as i32
            }
            // SAFETY: the unit the load holds
            _ => i32::from(unsafe { crate::world::UnitView::of(&t, io.i as u64) }?.queued_for_delete()),
        });
    }
    if p == 0 {
        return None;
    }
    // SAFETY (every block): the caller's contract
    unsafe {
        match op {
            EE_UNITS_PLAYER_SAVE_STORE_PLAYER_TABLE => crate::units::us_fill::game::ee_units_store_player_table(p),
            EE_UNITS_PLAYER_SAVE_X_TECH_NULL => *slot(p, l.player_technologies) = 0,
            EE_UNITS_PLAYER_SAVE_TECH_PRESENT => return Some(i32::from(*slot(p, l.player_technologies) != 0)),
            EE_UNITS_PLAYER_SAVE_X_PATROL_NEW => io.i = crate::units::patrol_group::ee_units_pg_new() as i64,
            EE_UNITS_PLAYER_SAVE_X_PATROL_RELEASE => {
                crate::units::patrol_group::ee_units_pg_dereference(io.i as usize as *mut _)
            }
            EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT | EE_UNITS_PLAYER_SAVE_X_CLASS_PRESENT => {
                let t = WorldRead::begin();
                return Some(if op == EE_UNITS_PLAYER_SAVE_X_CLASS_COUNT {
                    ClassView::count(&t) as i32
                } else {
                    i32::from(ClassView::by_index(&t, index as usize).is_some())
                });
            }
            _ => return None,
        }
    }
    Some(0)
}
