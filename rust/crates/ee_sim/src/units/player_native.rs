//! The player's body operations the Rust does itself (the player/tech lane, round 2): the ones of
//! `ee_units_player_body_host` (EEPlayer.cpp) that only read or wrote a pointer member of the player, a field of its
//! Rust records (`EeUnitsPlayerSaved`, `EeUnitsPlayerFields`) or called another Rust export. The game's
//! [`PlayerBodyHost`](crate::units::player_body::PlayerBodyHost) asks [`body`] before calling the C++; the bodies'
//! unit tests' hosts answer every operation themselves, so the bodies and their tests are unchanged.

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use crate::units::player_body::*;
use crate::units::player_rest::{
    EE_UNITS_PLAYER_BODY_ATC_UPDATE, EE_UNITS_PLAYER_BODY_CLOAKED, EE_UNITS_PLAYER_BODY_DETACHED,
    EE_UNITS_PLAYER_BODY_GAME_TIME, EE_UNITS_PLAYER_BODY_SET_CLOAKED, EE_UNITS_PLAYER_BODY_SET_DETACHED,
    EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED, EE_UNITS_PLAYER_BODY_TECH_LOAD, EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE,
    EE_UNITS_PLAYER_BODY_TECH_SAVE, EE_UNITS_PLAYER_BODY_TOWN_CONTINENT,
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
use std::os::raw::{c_long, c_ulong};

/// The pointer member at byte offset `offset` of player `p`.
fn slot(p: u64, offset: u64) -> *mut u64 {
    (p as usize + offset as usize) as *mut u64
}

/// Herd `h`'s record (an `EEHerd*`).
fn herd(h: u64) -> *mut crate::cp::herd::EeHerdFields {
    (h as usize + layout().herd_fields as usize) as *mut _
}

/// The world table's row `row`: `GetLongValue` (`unsigned long`), `GetLongValue2`, `GetFloatValue`.
fn world_long(row: i64) -> i64 {
    ee_data::tables::ee_data_world_entry(row as u32).mLongValue as c_ulong as i64
}
fn world_long2(row: i64) -> c_ulong {
    ee_data::tables::ee_data_world_entry(row as u32).mLongValue2 as c_ulong
}
fn world_float(row: i64) -> f32 {
    ee_data::tables::ee_data_world_entry(row as u32).mFloatValue
}
use ee_data::world_constants::EEWorldConstant as W;

/// Area `x`'s (a town, herd: `herd`) continent manager operation (EEAIContinentManager.h, `gContinentManager`'s
/// record): 0 `SetSerial(NewSerial())`, 1 `AddArea`, 2 `RemoveArea`.
///
/// # Safety
/// `x` a live town or herd; the manager's record registered.
unsafe fn area_op(x: u64, herd: bool, what: u8) {
    let l = layout();
    let area = x as usize + if herd { l.herd_area } else { l.town_area } as usize;
    let r = crate::astar::world_view::cm_ptr();
    // SAFETY: the caller's contract; an EEAIArea is its record (it has no vtable)
    unsafe {
        match what {
            0 => {
                (*r).last_serial += 1;
                (*(area as *mut crate::astar::area::EeAStarAreaRecord)).serial = (*r).last_serial;
            }
            1 => crate::astar::cm_queries::ee_astar_cm_add_object_area(r, area as u64),
            _ => {
                crate::astar::cm_queries::ee_astar_cm_remove_object_area(r, area as u64);
            }
        }
    }
}

/// Town `t`'s three addresses as ee_cp's town bodies take them (EETown.h: the object, its `EEAIArea`, its record).
fn town_parts(t: u64) -> (*mut c_void, *mut c_void, *mut crate::cp::town::EeTownFields) {
    let area = (t as usize + layout().town_area as usize) as *mut c_void;
    (t as usize as *mut c_void, area, town(t))
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

/// `mTechnologies->SetPlayerIndex(mPlayerID)` of player `p`, with `SetPlayer(this)` when `with_player` (None: no
/// such player).
///
/// # Safety
/// `p` is a live player with a tree.
unsafe fn set_tree_player(p: u64, with_player: bool) -> Option<()> {
    // SAFETY: the contract: the player's tree's record
    unsafe {
        let t = *slot(p, layout().player_technologies) as *mut crate::tech::nodes::EeTechTreeFields;
        if with_player {
            (*t).mPlayer = p as usize as *mut _;
        }
        let w = crate::world::WorldRead::begin();
        (*t).mPlayerIndex = crate::world::PlayerView::of(&w, p)?.id() as c_long;
    }
    Some(())
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
            EE_UNITS_PLAYER_BODY_TECH_NEW
            | EE_UNITS_PLAYER_BODY_TECH_COPY
            | EE_UNITS_PLAYER_BODY_TECH_DELETE
            | EE_UNITS_PLAYER_BODY_TECH_OBSOLETE
            | EE_UNITS_PLAYER_BODY_TECH_ALLOW
            | EE_UNITS_PLAYER_BODY_TECH_SET_INDEX
            | EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER
            | EE_UNITS_PLAYER_BODY_TECH_SAVE
            | EE_UNITS_PLAYER_BODY_TECH_LOAD
            | EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE
            | EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED => return body_tech(op, p, a, b, c),
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
            | EE_UNITS_PLAYER_BODY_PATROL_OF
            | EE_UNITS_PLAYER_BODY_COMMAND_TYPE
            | EE_UNITS_PLAYER_BODY_UNBUILT_WALL
            | EE_UNITS_PLAYER_BODY_MAP_SIZE
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
                    // a complex unit's mPatrolGroup, `mCommand ? mCommand->mType : 0`, an unbuilt wall
                    EE_UNITS_PLAYER_BODY_PATROL_OF => unit()?.patrol_group_handle() as i64,
                    EE_UNITS_PLAYER_BODY_COMMAND_TYPE => unit()?.command_type() as i64,
                    EE_UNITS_PLAYER_BODY_UNBUILT_WALL => {
                        let u = unit()?;
                        i64::from(u.class()?.wall() && !u.is_built())
                    }
                    EE_UNITS_PLAYER_BODY_MAP_SIZE => crate::world::WorldView::global(&t)?.map()?.size(),
                    EE_UNITS_PLAYER_BODY_IS_LOCAL => {
                        let w = crate::world::WorldView::global(&t)?;
                        i64::from(w.current_player().is_some_and(|c| c.handle() == p))
                    }
                    _ => crate::world::ClassView::count(&t) as i64,
                });
            }
            // a herd's animal, a town's patrol group, the world table, the teams, a patrol group's nearest point
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_HERD_ANIMAL => return Some((*herd(a)).mAnimalID as i64),
            EE_UNITS_PLAYER_BODY_TOWN_PATROL => return Some(*slot(a, l.town_patrol_group) as i64),
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_TOWN_SET_PATROL => *slot(a, l.town_patrol_group) = b as u64,
            EE_UNITS_PLAYER_BODY_WORLD_LONG => return Some(world_long(b)),
            EE_UNITS_PLAYER_BODY_TEAM_COUNT => {
                return Some(crate::app::teams_ffi::ee_app_teams_get(usize::MAX, std::ptr::null_mut(), 0) as i64)
            }
            EE_UNITS_PLAYER_BODY_TEAM_HAS => {
                return Some(i64::from(crate::app::teams_ffi::ee_app_teams_contains(b as usize, c as u32)))
            }
            EE_UNITS_PLAYER_BODY_PATROL_NEAREST => {
                return Some(crate::units::patrol_group::ee_units_pg_nearest(a as usize as *mut _, b as i32, c as i32)
                    as usize as i64)
            }
            // an upgrade's (EEUnitStats') terrain mask, button loads, class's unit type
            EE_UNITS_PLAYER_BODY_TERRAIN_MASK
            | EE_UNITS_PLAYER_BODY_BUTTON_LOADS
            | crate::units::player_rest::EE_UNITS_PLAYER_BODY_STATS_TYPE => {
                let t = crate::world::WorldRead::begin();
                let s = crate::world::StatsView::of(&t, a)?;
                return Some(match op {
                    EE_UNITS_PLAYER_BODY_TERRAIN_MASK => (*s.fields().ptr()).mTerrainMask as i64,
                    EE_UNITS_PLAYER_BODY_BUTTON_LOADS => (*s.fields().ptr()).mButtonLoadCount as i64,
                    _ => s.class()?.unit_type() as i64,
                });
            }
            // the saved game's live state (save_versions.rs); town `a`'s hull (its EEAIArea's record: the point
            // count, point b packed); unit `a`'s line of sight (its stats', the float's bits)
            EE_UNITS_PLAYER_BODY_LIVE_STATE => {
                return Some(i64::from(crate::save_versions::EESaveHasLiveState(b as c_ulong, c as c_ulong)))
            }
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_HULL_COUNT
            | crate::units::player_rest::EE_UNITS_PLAYER_BODY_HULL_POINT => {
                let area = (a as usize + l.town_area as usize) as *const crate::astar::area::EeAStarAreaRecord;
                let mut n = 0usize;
                let points = crate::astar::area::ee_astar_area_hull_points(area, &mut n);
                return Some(if op == crate::units::player_rest::EE_UNITS_PLAYER_BODY_HULL_COUNT {
                    n as c_ulong as i64
                } else {
                    let pt = *points.add(b as c_long as usize);
                    ((u64::from(pt.x as u32) << 32) | u64::from(pt.y as u32)) as i64
                });
            }
            // complex unit `a`'s mActiveGroup (an EEActiveGroup is its EeUnitsGroup: no vtable before it)
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_ACTIVE_GROUP => {
                let t = crate::world::WorldRead::begin();
                return Some(crate::world::UnitView::of(&t, a)?.active_group_handle() as i64);
            }
            EE_UNITS_PLAYER_BODY_LINE_OF_SIGHT => {
                let t = crate::world::WorldRead::begin();
                let s = crate::world::UnitView::of(&t, a)?.stats()?;
                return Some((*s.fields().ptr()).mLineOfSight.to_bits() as i32 as i64);
            }
            // the unit sets (ee_sim app unit_sets.rs; EEUnitSetManager.h): set `a` holds type b, set c holds type b
            EE_UNITS_PLAYER_BODY_SET_HAS => {
                return Some(i64::from(crate::app::unit_sets::ee_unit_set_contains(a as usize as *const _, b as u32)))
            }
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_SET_MANAGER_CONTAINS => {
                return Some(i64::from(crate::app::unit_sets::ee_unit_sets_contains(c as c_ulong as usize, b as u32)))
            }
            // the continent manager's: a town's or herd's serial and area, the continent of tile (b, c)
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_TOWN_SERIAL => area_op(a, false, 0),
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_TOWN_ADD_AREA => area_op(a, false, 1),
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_HERD_SERIAL => area_op(a, true, 0),
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_HERD_ADD_AREA => area_op(a, true, 1),
            crate::units::player_rest::EE_UNITS_PLAYER_BODY_CONTINENT_ID => {
                let r = crate::astar::world_view::cm_ptr();
                return Some(i64::from(crate::astar::cm_inline::ee_astar_cm_continent_id_at(
                    r,
                    b as c_long,
                    c as c_long,
                )));
            }
            // town `a`'s RefreshPatrolGroup, herd `a`'s RepairPointers(b, c) (ee_cp's bodies)
            EE_UNITS_PLAYER_BODY_TOWN_REFRESH => {
                let (t, area, f) = town_parts(a);
                crate::cp::town_game::ee_cp_town_refresh_patrol_group(t, area, f);
            }
            EE_UNITS_PLAYER_BODY_HERD_REPAIR => {
                crate::cp::herd_game::ee_cp_herd_repair_pointers(
                    a as usize as *mut c_void,
                    herd(a),
                    b as u32,
                    c as u32,
                );
            }
            // the map's waterless flag, its tile (b, c) (`GetTile`: none off the map)
            EE_UNITS_PLAYER_BODY_WATERLESS | EE_UNITS_PLAYER_BODY_TILE_PRESENT => {
                let t = crate::world::WorldRead::begin();
                let map = crate::world::WorldView::global(&t)?.map()?;
                return Some(if op == EE_UNITS_PLAYER_BODY_WATERLESS {
                    i64::from((*map.fields().ptr()).mMapCharacteristics & 2 == 2)
                } else {
                    i64::from(map.tile(b as c_ulong as i64, c as c_ulong as i64).is_some())
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

/// The body operations on player `p`'s tech tree (`mTechnologies`): made, copied from gTechTree and freed by ee_game's
/// bodies (ee_sim tech tree_objects.rs `TreeBodies`), its nodes' flags, its player, its walks and its save to and
/// load from a memory file `a` in the current saved game version.
///
/// # Safety
/// As [`body`]; the player's tree is live (but for TECH_NEW).
unsafe fn body_tech(op: i32, p: u64, a: u64, b: i64, c: i64) -> Option<i64> {
    use crate::tech::nodes::EeTechTreeOp;
    use crate::tech::tree_objects as to;
    let tb = to::tree_bodies()?;
    let slot = slot(p, layout().player_technologies);
    // SAFETY (every block): the caller's contract
    unsafe {
        let tree = *slot as usize;
        let node = |id: i64| to::tree_get(tree as *mut _, id as c_long) as usize;
        match op {
            EE_UNITS_PLAYER_BODY_TECH_NEW => *slot = (tb.new)() as u64,
            EE_UNITS_PLAYER_BODY_TECH_COPY => (tb.copy_global)(tree),
            EE_UNITS_PLAYER_BODY_TECH_DELETE => (tb.delete)(tree),
            EE_UNITS_PLAYER_BODY_TECH_OBSOLETE => return Some(i64::from(to::is_obsolete(node(b)))),
            EE_UNITS_PLAYER_BODY_TECH_ALLOW => (*to::node(node(b))).mIsAllowed = c != 0,
            EE_UNITS_PLAYER_BODY_TECH_SET_INDEX => {
                (*(tree as *mut crate::tech::nodes::EeTechTreeFields)).mPlayerIndex = b as c_long
            }
            EE_UNITS_PLAYER_BODY_TECH_SET_PLAYER => set_tree_player(p, true)?,
            EE_UNITS_PLAYER_BODY_TECH_RESET_RANGE => {
                to::walk_tree(tree, EeTechTreeOp::ResetEpochRange, b as i32, c as i32, 0);
            }
            EE_UNITS_PLAYER_BODY_TECH_APPLY_SAVED => {
                to::walk_tree(tree, EeTechTreeOp::ApplySavedState, 1, i32::from(b != 0), 0);
            }
            _ => {
                // TECH_SAVE, TECH_LOAD: the memory file (an FSFile) in the current version
                let load = op == EE_UNITS_PLAYER_BODY_TECH_LOAD;
                let (major, minor) = (tb.save_version)();
                let r = (tb.file)(tree, a as usize, load, major, minor);
                return Some(if load { r as i64 } else { 0 });
            }
        }
    }
    Some(0)
}

/// The answer of player host operation `op` (`EEPlayerHost`'s) of player `p` that is a call of another of the
/// player's Rust bodies, or a read of a unit or a unit class (None: the C++ does it).
///
/// # Safety
/// As [`body`]; `io` the operation's operands.
#[cfg_attr(test, allow(unused_variables))]
pub unsafe fn player(
    op: i32,
    p: u64,
    io: &mut crate::units::player::EeUnitsPlayerIo,
    host: (crate::units::player::EeUnitsPlayerFn, *const crate::units::player_stats::EeUnitsPlayerStats),
) -> Option<i64> {
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
            // the virtuals EEComputerPlayer overrides, dispatched here on the player's `mComputerPlayer`
            // (ee/EEComputerPlayer.h): ScaleCost, TakeResource; GetExtraPopulation is 1 and WonderStarted empty for
            // every player
            // EEPlayer's virtuals that EEComputerPlayer overrides, dispatched on `mComputerPlayer` (player_econ.rs)
            #[cfg(not(test))]
            crate::units::player::EE_UNITS_PLAYER_POPULATION_LIMIT
            | crate::units::player::EE_UNITS_PLAYER_HERO_COUNT
            | crate::units::player::EE_UNITS_PLAYER_CAN_AFFORD => {
                let refs = crate::units::player::EeUnitsPlayerRefs {
                    player: pv,
                    host: host.0,
                    saved: saved(p),
                    fields: fields(p),
                    stats: host.1,
                };
                let (object, a, b) = match op {
                    crate::units::player::EE_UNITS_PLAYER_POPULATION_LIMIT => (0, 0, 0),
                    crate::units::player::EE_UNITS_PLAYER_HERO_COUNT => {
                        (0, i64::from(a0 != 0), i64::from(io.i[1] != 0))
                    }
                    _ => (a0, io.i[1] as c_long as i64, io.i[2] as c_long as i64),
                };
                return Some(crate::units::player_econ::ee_units_player_virtual(
                    &refs,
                    op,
                    object as usize as *mut c_void,
                    a,
                    b,
                    false,
                ));
            }
            crate::units::player::EE_UNITS_PLAYER_SCALE_COST => {
                let l = layout();
                return Some(if *slot(p, l.player_computer_player) != 0 {
                    crate::cp::ffi::ee_cp_scale_cost((p as usize + l.cp_fields as usize) as *const _, a0 as c_long)
                        as i64
                } else {
                    a0
                });
            }
            crate::units::player::EE_UNITS_PLAYER_TAKE => {
                if *slot(p, layout().player_computer_player) != 0 {
                    crate::cp::player_game::ee_cp_pl_take_resource(p, a0 as c_long, io.i[1] as c_long);
                } else {
                    (*saved(p)).mResourceCaps[a0 as usize] -= io.i[1] as c_long;
                }
                return Some(0);
            }
            crate::units::player::EE_UNITS_PLAYER_EXTRA_POPULATION => return Some(1),
            // a complex unit's mHolder
            crate::units::player_areas::EE_UNITS_PLAYER_HOLDER => {
                let t = WorldRead::begin();
                return Some(UnitView::of(&t, a0 as u64)?.holder_handle() as i64);
            }
            EE_UNITS_PLAYER_X_WONDER_STARTED => return Some(0),
            // the dead unit's statistic (player_rest.rs)
            #[cfg(not(test))]
            crate::units::player_units::EE_UNITS_PLAYER_DEAD_STATISTIC => {
                return Some(crate::units::player_rest::game::ee_units_dead_unit_statistic(a0 as u64) as i64)
            }
            // set a[0] holds type a[1]; the healed units' set (a[1]: set a[0], else none)
            crate::units::player_units::EE_UNITS_PLAYER_SET_CONTAINS => {
                let set = crate::app::unit_sets::ee_unit_sets_get(a0 as c_ulong as usize);
                return Some(i64::from(
                    !set.is_null() && crate::app::unit_sets::ee_unit_set_contains(set, io.i[1] as c_ulong as u32),
                ));
            }
            crate::units::player_units::EE_UNITS_PLAYER_HEALED_SET => {
                let set = if io.i[1] != 0 {
                    crate::app::unit_sets::ee_unit_sets_get(a0 as c_ulong as usize)
                } else {
                    std::ptr::null()
                };
                *slot(p, layout().player_healed_unit_set) = set as usize as u64;
                return Some(0);
            }
            // the unit added to or removed from its town or herd (player_areas.rs)
            #[cfg(not(test))]
            crate::units::player_units::EE_UNITS_PLAYER_ADD_TO_TOWN
            | crate::units::player_units::EE_UNITS_PLAYER_ADD_TO_HERD
            | crate::units::player_units::EE_UNITS_PLAYER_REMOVE_FROM_TOWN
            | crate::units::player_units::EE_UNITS_PLAYER_REMOVE_FROM_HERD => {
                use crate::units::player_units as pu;
                let refs = crate::units::player::EeUnitsPlayerRefs {
                    player: pv,
                    host: host.0,
                    saved: saved(p),
                    fields: fields(p),
                    stats: host.1,
                };
                let what = match op {
                    pu::EE_UNITS_PLAYER_ADD_TO_TOWN => 0,
                    pu::EE_UNITS_PLAYER_ADD_TO_HERD => 1,
                    pu::EE_UNITS_PLAYER_REMOVE_FROM_TOWN => 2,
                    _ => 3,
                };
                let r = crate::units::player_areas::ee_units_player_areas(&refs, what, a0 as usize as *mut c_void);
                return Some(if what < 2 { i64::from(r) } else { 0 });
            }
            // the towns' and herds' bodies (ee_cp town_game.rs, herd_game.rs; EETown.h, EEHerd.h): a[0] 0 a town, 1 a
            // herd (the list), the area, the unit
            crate::units::player_areas::EE_UNITS_PLAYER_AREA_CAN_TAKE
            | crate::units::player_areas::EE_UNITS_PLAYER_AREA_ADD => {
                let (area, unit) = (io.i[1] as u64, io.i[2] as u64);
                let add = op == crate::units::player_areas::EE_UNITS_PLAYER_AREA_ADD;
                let h = area as usize as *mut c_void;
                return Some(i64::from(match (a0 == 0, add) {
                    (true, false) => {
                        let (t, ar, f) = town_parts(area);
                        crate::cp::town_game::ee_cp_town_can_take(t, ar, f, unit)
                    }
                    (true, true) => {
                        let (t, ar, f) = town_parts(area);
                        crate::cp::town_game::ee_cp_town_add_unit(t, ar, f, unit)
                    }
                    (false, false) => crate::cp::herd_game::ee_cp_herd_can_take(h, herd(area), unit, true),
                    (false, true) => crate::cp::herd_game::ee_cp_herd_add_unit(h, herd(area), unit),
                }));
            }
            crate::units::player_areas::EE_UNITS_PLAYER_HERD_HAS => {
                let h = a0 as u64;
                return Some(i64::from(crate::cp::herd_game::ee_cp_herd_has_unit(
                    h as usize as *mut c_void,
                    herd(h),
                    io.i[1] as u64,
                )));
            }
            EE_UNITS_PLAYER_X_TOWN_SERIAL
            | EE_UNITS_PLAYER_X_TOWN_ADD_AREA
            | EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA
            | EE_UNITS_PLAYER_X_HERD_SERIAL
            | EE_UNITS_PLAYER_X_HERD_ADD_AREA
            | EE_UNITS_PLAYER_X_HERD_REMOVE_AREA => {
                let (herd, what) = match op {
                    EE_UNITS_PLAYER_X_TOWN_SERIAL => (false, 0),
                    EE_UNITS_PLAYER_X_TOWN_ADD_AREA => (false, 1),
                    EE_UNITS_PLAYER_X_TOWN_REMOVE_AREA => (false, 2),
                    EE_UNITS_PLAYER_X_HERD_SERIAL => (true, 0),
                    EE_UNITS_PLAYER_X_HERD_ADD_AREA => (true, 1),
                    _ => (true, 2),
                };
                area_op(a0 as u64, herd, what);
                return Some(0);
            }
            EE_UNITS_PLAYER_X_TOWN_REMOVE_UNIT => {
                let (t, ar, f) = town_parts(a0 as u64);
                crate::cp::town_game::ee_cp_town_remove_unit(t, ar, f, io.i[1] as u64);
                return Some(0);
            }
            EE_UNITS_PLAYER_X_HERD_REMOVE_UNIT => {
                let h = a0 as u64;
                crate::cp::herd_game::ee_cp_herd_remove_unit(h as usize as *mut c_void, herd(h), io.i[1] as u64);
                return Some(0);
            }
            EE_UNITS_PLAYER_X_TOWN_MERGE => {
                let (t, ar, f) = town_parts(a0 as u64);
                let other = io.i[1] as u64;
                crate::cp::town_game::ee_cp_town_merge(t, ar, f, other, town(other));
                return Some(0);
            }
            EE_UNITS_PLAYER_X_TOWN_REFRESH_PATROL => {
                let (t, ar, f) = town_parts(a0 as u64);
                crate::cp::town_game::ee_cp_town_refresh_patrol_group(t, ar, f);
                return Some(0);
            }
            // a town's patrol group; the world table's rows
            EE_UNITS_PLAYER_X_TOWN_PATROL_OF => return Some(*slot(a0 as u64, layout().town_patrol_group) as i64),
            EE_UNITS_PLAYER_X_POINTS_MAX => return Some(world_long(W::kWorldUnitImprovementPointsMax as i64)),
            EE_UNITS_PLAYER_X_WONDER_HIT_POINT_BONUS => {
                io.f[0] = world_float(W::kWorldWonderHitPointBonus as i64);
                return Some(0);
            }
            EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1 | EE_UNITS_PLAYER_X_IMPROVEMENT_TIME2 => {
                // `(long)` of GetLongValue(2)(EEWorldImprovementTime(a[0]))
                let row = W::kWorldImprovementTimeAreaEffect as i64 + a0 as c_long as i64;
                let v = if op == EE_UNITS_PLAYER_X_IMPROVEMENT_TIME1 {
                    world_long(row) as c_ulong
                } else {
                    world_long2(row)
                };
                return Some(v as c_long as i64);
            }
            _ => {}
        }
    }
    if let Some(v) = unsafe { tech(op, p, io) } {
        return Some(v);
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

/// The player host's tech tree operations (player/tech lane, round 3): the tree, its nodes and their walks are Rust's
/// (ee_sim tech tree_objects.rs) behind the player's `mTechnologies`.
///
/// # Safety
/// As [`player`].
unsafe fn tech(op: i32, p: u64, io: &mut crate::units::player::EeUnitsPlayerIo) -> Option<i64> {
    use crate::tech::nodes::{EeTechNodeOp, EeTechTreeOp};
    use crate::tech::tree_objects as to;
    use crate::units::player::{EE_UNITS_PLAYER_NODE, EE_UNITS_PLAYER_NODE_AVAILABLE, EE_UNITS_PLAYER_NODE_UNIT_CLASS};
    use crate::units::player_split::EE_UNITS_PLAYER_X_NODE;
    use crate::units::player_units::{
        EE_UNITS_PLAYER_CITIZEN_CONTAINS, EE_UNITS_PLAYER_RESEARCH, EE_UNITS_PLAYER_SET_NODE_AVAILABLE,
        EE_UNITS_PLAYER_TECH_OBSOLETE, EE_UNITS_PLAYER_UNIT_NODE,
    };
    let l = crate::world::layout::get()?;
    let a = io.i;
    // SAFETY (every block): the caller's contract; the player's tree is live (the C++ host dereferenced it)
    unsafe {
        let tree = *slot(p, l.player_technologies) as usize;
        let get = |id: i64| to::tree_get(tree as *mut _, id as c_long) as usize;
        Some(match op {
            EE_UNITS_PLAYER_NODE | EE_UNITS_PLAYER_X_NODE => get(a[0]) as i64,
            EE_UNITS_PLAYER_NODE_AVAILABLE => {
                to::walk_node(a[0] as usize, EeTechNodeOp::IsAvailable, 0, false, 0) as i64
            }
            EE_UNITS_PLAYER_NODE_UNIT_CLASS => to::unit_class(a[0] as usize, a[1] as usize) as i64,
            EE_UNITS_PLAYER_SET_NODE_AVAILABLE => {
                to::walk_tree(tree, EeTechTreeOp::SetNodeAvailable, a[0] as i32, i32::from(a[1] != 0), 0);
                0
            }
            EE_UNITS_PLAYER_RESEARCH => {
                to::walk_tree(tree, EeTechTreeOp::Research, a[0] as i32, 0, 0);
                0
            }
            EE_UNITS_PLAYER_CITIZEN_CONTAINS => {
                let d = (*to::node(get(crate::tech_tree_ids::EETechTreeTechID::kTechCitizen as i64))).mData;
                i64::from(crate::tech::nodes::ee_tech_node_classes_contains(d, a[0] as c_long))
            }
            EE_UNITS_PLAYER_TECH_OBSOLETE => {
                let n = get(a[0]);
                i64::from(n != 0 && to::is_obsolete(n))
            }
            EE_UNITS_PLAYER_UNIT_NODE => {
                let t = crate::world::WorldRead::begin();
                let u = crate::world::UnitView::of(&t, a[0] as u64)?;
                let owner = *slot(u.player()?.handle(), l.player_technologies) as usize;
                to::tree_get(owner as *mut _, u.class()?.technology() as c_long) as i64
            }
            _ => return None,
        })
    }
}

/// The answer of tick operation `op` (`EEPlayerTickOperation`'s) of player `p` that is a call of another Rust body
/// or a read of the player's units or tech tree (None: the C++ does it).
///
/// # Safety
/// As [`body`]; `io` the operation's operands (`io.i` a unit of the player, 0 for none).
pub unsafe fn tick(
    op: i32,
    p: u64,
    index: u32,
    io: &mut crate::units::player_save::EeUnitsPlayerSaveIo,
) -> Option<i32> {
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
            // herd `index`'s due time and activity, its update, its state
            EE_UNITS_PLAYER_TICK_HERD_DUE | EE_UNITS_PLAYER_TICK_HERD_UPDATE | EE_UNITS_PLAYER_TICK_HERD_STATE => {
                let lists = crate::units::player_lists::player_lists(p);
                let h = crate::units::player_lists::ee_units_lists_at(
                    lists,
                    crate::units::player_lists::EE_UNITS_LISTS_HERDS,
                    index as usize,
                );
                let f = herd(h);
                match op {
                    EE_UNITS_PLAYER_TICK_HERD_DUE => {
                        io.i = (*f).mNextUpdateTime as i64;
                        io.b = u8::from((*f).mIsActive);
                    }
                    EE_UNITS_PLAYER_TICK_HERD_UPDATE => {
                        crate::cp::herd_game::ee_cp_herd_update(h as usize as *mut c_void, f, io.i as c_ulong)
                    }
                    _ => return Some((*f).mState as i32),
                }
            }
            // the wonders' heal: unit io.i's type in the healed units' set; the world table's
            EE_UNITS_PLAYER_TICK_X_HEALED => {
                let set = *slot(p, layout().player_healed_unit_set) as usize as *const _;
                let t = WorldRead::begin();
                let unit_type = UnitView::of(&t, io.i as u64)?.class()?.unit_type();
                return Some(i32::from(crate::app::unit_sets::ee_unit_set_contains(set, unit_type as u32)));
            }
            EE_UNITS_PLAYER_TICK_X_HEAL_AMOUNT => io.i = world_long(W::kWorldWonderHealAmount as i64),
            EE_UNITS_PLAYER_TICK_X_HEAL_STEP => io.i = world_long(W::kWorldWonderHealEpochStep as i64),
            EE_UNITS_PLAYER_TICK_X_HEAL_FACTOR => io.f = world_float(W::kWorldWonderHealEpochFactor as i64),
            // gPhysics->GetTickTime()
            EE_UNITS_PLAYER_TICK_TICK_TIME => {
                let t = WorldRead::begin();
                io.f = (*crate::world::WorldView::global(&t)?.physics()?.ptr()).mTickTime;
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
    file: crate::units::player_save::SaveFile,
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
            EE_UNITS_PLAYER_SAVE_X_TECH_INDEX | EE_UNITS_PLAYER_SAVE_X_TECH_PLAYER => {
                // SetPlayerIndex(mPlayerID), SetPlayer(this) (its index too)
                set_tree_player(p, op == EE_UNITS_PLAYER_SAVE_X_TECH_PLAYER)?;
            }
            // the player's tree made, copied from gTechTree, loaded, saved and freed (ee_game's bodies)
            EE_UNITS_PLAYER_SAVE_X_TECH_NEW
            | EE_UNITS_PLAYER_SAVE_X_TECH_COPY
            | EE_UNITS_PLAYER_SAVE_X_TECH_DELETE
            | EE_UNITS_PLAYER_SAVE_X_TECH_LOAD
            | EE_UNITS_PLAYER_SAVE_X_TECH_SAVE => {
                let tb = crate::tech::tree_objects::tree_bodies()?;
                let tree = slot(p, l.player_technologies);
                match op {
                    EE_UNITS_PLAYER_SAVE_X_TECH_NEW => *tree = (tb.new)() as u64,
                    EE_UNITS_PLAYER_SAVE_X_TECH_COPY => (tb.copy_global)(*tree as usize),
                    EE_UNITS_PLAYER_SAVE_X_TECH_DELETE => (tb.delete)(*tree as usize),
                    _ => {
                        let load = op == EE_UNITS_PLAYER_SAVE_X_TECH_LOAD;
                        let (major, minor) = (file.major as c_ulong, file.minor as c_ulong);
                        return Some((tb.file)(*tree as usize, file.file, load, major, minor) as i32);
                    }
                }
            }
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
