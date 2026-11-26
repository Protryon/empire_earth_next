//! The doppelganger manager's, the active groups' and the groups' hosts' plain statements over the world views (the
//! units' lane, round 13, second leg): each statement of `ee_units_dm_host` (EEDoppelgangerManager.cpp),
//! `ee_units_ag_host` (EEActiveGroup.cpp) and `ee_units_group_host` (EEGroup.cpp) that reads or stores one member
//! of a Rust record (a unit's, its class's, a player's, a tile's, a tile node's, the world's) or that called a unit
//! or class member whose inline body is one call of a Rust entry point, answered here before the C++ host is asked
//! (dm_body.rs `Game`, ag_host.rs `AgGame`, group_host.rs `GameGroups`); their cases left the C++.
#![cfg(not(test))]
// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_view::word;
use crate::world::layout::layout;
use crate::world::view::{ClassView, PlayerView, TileView, UnitView, WorldRead, WorldView};
use crate::world_field;
use std::os::raw::{c_long, c_ulong, c_void};

fn p(h: u64) -> *mut c_void {
    h as usize as *mut c_void
}

/// `kResourceAny`
const RESOURCE_ANY: u64 = 1;

/// `EEComplexUnit::CanTake(cargo, kind)`: the unit takes any resource or this one.
fn can_take(u: &UnitView<'_>, kind: i64) -> i64 {
    let r = u.resource_type();
    i64::from(r == RESOURCE_ANY || r == kind as c_ulong as u64)
}

fn cu_body(what: i32, unit: u64, a: i64, b: i64, c: i64) -> i64 {
    // SAFETY: the member's call on the live unit
    unsafe { crate::units::cu_body::game::ee_units_cu_body(what, p(unit), a, b, c) }
}

fn set_hit_points(unit: u64, value: i64) {
    let w = crate::units::eu_body::EE_UNITS_EU_DO_SET_HIT_POINTS;
    // SAFETY: the member's call on the live unit
    unsafe { crate::units::eu_body::game::ee_units_eu_body(w, p(unit), value, 0, 0) };
}

/// `ee_units_dm_host`'s statement `op` (`a` a tile, a unit, a player or a tile node; `b`, `c` values).
pub(crate) fn dm_plain(op: i32, a: u64, b: i64, c: i64) -> Option<i64> {
    use crate::units::dm_body::*;
    // SAFETY: the manager's bodies run inside the C++ caller's hold of the world; the handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, a) };
    // SAFETY: as above
    let pl = || unsafe { PlayerView::of(&t, a) };
    let w = || WorldView::global(&t);
    Some(match op {
        EE_UNITS_DM_PLAYER_AT => w()?.player(b as usize).map_or(0, |q| q.handle() as i64),
        EE_UNITS_DM_IS_CURRENT => {
            let w = w()?;
            i64::from(w.player(w.current_player_index() as usize).map_or(0, |q| q.handle()) == a)
        }
        EE_UNITS_DM_PLAYER_ID => i64::from(pl()?.id()),
        EE_UNITS_DM_SAVES_VIS_MAP => i64::from(pl()?.save_vis_map()),
        // SAFETY: a live tile's node list
        EE_UNITS_DM_TILE_FIRST_NODE => unsafe { word(a, layout().tile_units) as i64 },
        // SAFETY: a live tile
        EE_UNITS_DM_TILE_VISIBILITY => i64::from(unsafe { TileView::of(&t, a) }?.visibility(b as c_ulong as usize)),
        // SAFETY: a live tile node's members
        EE_UNITS_DM_NODE_UNIT => unsafe { word(a, layout().unit_node_unit) as i64 },
        // SAFETY: as above
        EE_UNITS_DM_NODE_NEXT => unsafe { word(a, layout().unit_node_next) as i64 },
        EE_UNITS_DM_UNIT_TYPE => u()?.class()?.unit_type() as i64,
        EE_UNITS_DM_CLASS_FLAG => {
            let k = u()?.class()?;
            i64::from(if b == 0 { world_field!(k.fields(), mIsFlare) } else { world_field!(k.fields(), mIsBoobyTrap) })
        }
        EE_UNITS_DM_CATEGORY => u()?.class()?.category() as i64,
        EE_UNITS_DM_FAMILY => u()?.class()?.family() as i64,
        EE_UNITS_DM_HIT_POINTS => u()?.hit_points(),
        EE_UNITS_DM_UNIT_PLAYER | EE_UNITS_DM_GET_PLAYER => u()?.player().map_or(0, |q| q.handle() as i64),
        EE_UNITS_DM_UNIT_TILE => u()?.tile().map_or(0, |k| k.handle() as i64),
        EE_UNITS_DM_UNIT_COMPLEX => u()?.complex_unit_handle() as i64,
        EE_UNITS_DM_PREVIOUS_PART => u()?.previous_part_handle() as i64,
        EE_UNITS_DM_NEXT_PART => u()?.next_part_handle() as i64,
        EE_UNITS_DM_HAS_SCALE => i64::from(world_field!(u()?.class()?.fields(), mScale) != 0.0),
        EE_UNITS_DM_UNIT_TILE_SIZE => u()?.class()?.tile_size(),
        EE_UNITS_DM_UNIT_TILE_XY => {
            let (x, y) = u()?.tile_xy();
            if b == 0 {
                x
            } else {
                y
            }
        }
        EE_UNITS_DM_IS_CLOAKED => {
            let u = u()?;
            i64::from(u.cloakable() && u.cloaked())
        }
        EE_UNITS_DM_IS_CLOAKABLE => i64::from(u()?.complex_unit()?.cloakable()),
        EE_UNITS_DM_HAS_MAP => i64::from(w()?.map().is_some()),
        EE_UNITS_DM_MAP_SIZE => w()?.map()?.size(),
        EE_UNITS_DM_MAP_TILE => {
            w()?.map()?.tile(b as c_long as i64, c as c_long as i64).map_or(0, |k| k.handle() as i64)
        }
        _ => return None,
    })
}

/// `ee_units_ag_host`'s statement `op` (`a` the unit, `b` a goal or a unit, `c` a value).
pub(crate) fn ag_plain(op: i32, a: u64, b: u64, c: i64) -> Option<i64> {
    use crate::units::ag_host::*;
    use crate::units::cu_body::*;
    use crate::units::cu_life::*;
    use crate::units::cu_misc::*;
    // SAFETY: the active group's bodies run inside the C++ caller's hold of the world; the handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let u = || unsafe { UnitView::of(&t, a) };
    Some(match op {
        EE_UNITS_AG_CLEAR_QUEUE => {
            cu_body(EE_UNITS_CU_DO_CLEAR_GOAL_QUEUE, a, c as c_ulong as i64, 0, 0);
            0
        }
        EE_UNITS_AG_CLEAR_GOALS => {
            cu_body(EE_UNITS_CU_DO_CLEAR_GOALS, a, c as c_long as i64, 0, 0);
            0
        }
        EE_UNITS_AG_SET_GOAL => {
            crate::units::goals::ee_units_unit_set_goal(a, b, false, c as c_long as i32);
            0
        }
        EE_UNITS_AG_CLEAR_GATHERED => {
            cu_body(EE_UNITS_CU_DO_SET_GATHERED_RESOURCE, a, 0, 0, 0);
            0
        }
        // `CanTake` asks the unit's own resource type (the cargo is not read)
        EE_UNITS_AG_CAN_TAKE => can_take(&u()?, c),
        EE_UNITS_AG_REFRESH_GOALS => {
            crate::units::goals::ee_units_unit_refresh_goals(a);
            0
        }
        EE_UNITS_AG_REFRESH_ACTIVE_GOAL => {
            crate::units::goals::ee_units_unit_refresh_active_goal(a);
            0
        }
        EE_UNITS_AG_START_ACTION => {
            cu_body(EE_UNITS_CU_DO_START_ACTION, a, c as c_long as i64, 0, 0);
            0
        }
        EE_UNITS_AG_ACTION_CHANGED => {
            let class = u()?.class()?.handle();
            let w = crate::units::uck_kinds::EE_UNITS_UCK_DO_ACTION_CHANGED;
            // SAFETY: the member's call on the live unit and its class
            unsafe { crate::units::uck_kinds::game::ee_units_uck_call(w, p(class), p(a), 0, 0, 0, 0.0) };
            0
        }
        EE_UNITS_AG_POWER => u()?.power(),
        _ => return None,
    })
}

/// `ee_units_group_host`'s statement `op` (`a` a unit or a player, `c` a value).
pub(crate) fn group_plain(op: i32, a: u64, c: i64) -> Option<i64> {
    use crate::units::group_host::*;
    // SAFETY: the groups' bodies run inside the C++ caller's hold of the world; the handles are live
    let t = unsafe { WorldRead::begin() };
    // SAFETY: as above
    let pl = || unsafe { PlayerView::of(&t, a) };
    Some(match op {
        EE_UNITS_GROUP_OP_SET_HIT_POINTS => {
            set_hit_points(a, c as c_long as i64);
            0
        }
        EE_UNITS_GROUP_OP_SET_FLIGHT_TIME => {
            // SAFETY: the live unit
            let u = unsafe { UnitView::of(&t, a) }?;
            let cx = crate::units::uc_view::complex_mut(&u);
            // SAFETY: the unit's member
            unsafe { (*cx).mFlightTime = c as c_long };
            0
        }
        EE_UNITS_GROUP_OP_CLASS_COUNT => ClassView::count(&t) as c_ulong as i64,
        EE_UNITS_GROUP_OP_PLAYER_GROUP => {
            if !(0..20).contains(&c) {
                return Some(0);
            }
            let at = layout().player_groups + (c as u64) * std::mem::size_of::<usize>() as u64;
            // SAFETY: the player's group pointer c (below 20)
            unsafe { word(pl()?.handle(), at) as i64 }
        }
        EE_UNITS_GROUP_OP_PLAYER_SELECTION => {
            let at = if c == 0 { layout().player_active_group } else { layout().player_selected_group };
            // SAFETY: the player's group pointer
            unsafe { word(pl()?.handle(), at) as i64 }
        }
        _ => return None,
    })
}
