//! The player host's plain operations answered through the world views (the object-storage plan's lane P): the
//! reads of a unit, its class and complex unit, the player and the world that `EEPlayerHost` (EEPlayer.cpp) used to
//! answer. [`FnHost`](crate::units::player::FnHost) asks [`answer`] before calling the C++; the other operations
//! (virtuals, the towns and herds, the tech tree, the RNG, the sound and UI) stay host calls. The unit tests' hosts
//! answer every operation themselves, so the bodies' tests are unchanged.

use crate::units::cls_dispatch::{EE_UNITS_BODY_IS_LAND_BUILDING, EE_UNIT_CLASS_BODIES};
use crate::units::player::{EeUnitsPlayerIo, EE_UNITS_PLAYER_IS_LOCAL};
use crate::units::player_areas::{EE_UNITS_PLAYER_AREA_AT, EE_UNITS_PLAYER_AREA_COUNT};
use crate::units::player_diplomacy::{EE_UNITS_PLAYER_IS_CURRENT, EE_UNITS_PLAYER_VIEW, EE_UNITS_PLAYER_VIEWED_OF};
use crate::units::player_units::{
    EeUnitsPlayerUnit, EE_UNITS_PLAYER_AT, EE_UNITS_PLAYER_BUILD_QUEUE_TECH, EE_UNITS_PLAYER_COMPLEX_VALUES,
    EE_UNITS_PLAYER_CONTAINER_CATEGORY, EE_UNITS_PLAYER_ID, EE_UNITS_PLAYER_NEXT_UNIT, EE_UNITS_PLAYER_NUM_PLAYERS,
    EE_UNITS_PLAYER_PENDING_COUNT, EE_UNITS_PLAYER_PENDING_POPULATION, EE_UNITS_PLAYER_UNIT_INFO,
};
use crate::world::{world_handle, ClassView, PlayerView, UnitView, WorldRead, WorldView};
use std::os::raw::{c_long, c_ulong};

/// `GetPopulationCost()`: every class kind's body is `mWeight`
fn population(c: &ClassView<'_>) -> c_long {
    crate::world_field!(c.fields(), mWeight)
}

/// `IsLandBuilding()`: the class body table's answer for its category (1 or 3)
pub(crate) fn land_building(c: &ClassView<'_>) -> bool {
    let category = c.category();
    let body = if category < 256 { EE_UNIT_CLASS_BODIES[EE_UNITS_BODY_IS_LAND_BUILDING][category as usize] } else { 0 };
    body == 1 || body == 3
}

/// `EE_UNITS_PLAYER_UNIT_INFO`'s record of unit `u` (the C++ read the class, then the unit, then its complex unit)
fn unit_info(u: &UnitView<'_>) -> EeUnitsPlayerUnit {
    let mut r = EeUnitsPlayerUnit::default();
    let x = u.complex_unit();
    r.complex = u.complex_unit_handle();
    if let Some(c) = u.class() {
        r.unit_type = c.unit_type() as c_ulong;
        r.technology = c.technology() as c_long;
        r.population = population(&c);
        r.category = c.category() as c_ulong;
        r.ai_kind = c.ai_kind() as c_ulong;
        r.wonder_bonus = crate::world_field!(c.fields(), mWonderBonus);
        r.wonder_unit_set = crate::world_field!(c.fields(), mWonderUnitSet);
        r.land_building = land_building(&c);
        r.vital = c.vital();
        r.inactive = c.inactive();
        r.blocks_tile = c.blocks_tile();
        r.wall = c.wall();
        r.tree = c.tree();
    }
    r.on_tile = u.has_tile_node();
    r.built = x.is_some_and(|x| x.is_built());
    r.first_part = x.is_some_and(|x| x.previous_part_handle() == 0);
    r.hit_points = u.hit_points() as c_long;
    r.tile_x = u.tile_xy().0 as c_long;
    r.resource_amount = u.resource_amount() as c_long;
    r.container = x.map_or(0, |x| x.container_handle());
    r.current_research = x.map_or(0, |x| x.current_research() as c_long);
    r.build_queue = x.map_or(0, |x| x.build_queue_len() as c_long);
    r
}

/// The answer of a plain operation `op` of player `player`'s host (None: the C++ answers it).
///
/// # Safety
/// `player` is a live `EEPlayer` and the operation's handles are the live objects the C++ host would have read; the
/// caller holds the world (a call from the simulation into the player's Rust).
pub unsafe fn answer(player: u64, op: i32, io: &mut EeUnitsPlayerIo) -> Option<i64> {
    let world = world_handle();
    if world == 0 {
        return None;
    }
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    // SAFETY: `&gWorld`, registered by its constructor
    let w = unsafe { WorldView::of(&t, world) }?;
    // SAFETY: the caller's contract
    let me = unsafe { PlayerView::of(&t, player) }?;
    let (a0, a1) = (io.i[0], io.i[1]);
    // the diplomacy's operand: 0 this player
    let other = if a0 != 0 { a0 as u64 } else { player };
    // SAFETY: the operation's unit (the C++ host's `a[0]` read as an RTSUnit*)
    let unit = |h: i64| unsafe { UnitView::of(&t, h as u64) };
    let current = w.current_player().map_or(0, |p| p.handle());
    let a = &mut io.i;
    Some(match op {
        EE_UNITS_PLAYER_CONTAINER_CATEGORY => unit(a0)?.class()?.category() as i64,
        EE_UNITS_PLAYER_NEXT_UNIT => {
            let next = if a0 == 0 { me.first_unit() } else { unit(a0)?.next() };
            next.map_or(0, |u| u.handle() as i64)
        }
        EE_UNITS_PLAYER_BUILD_QUEUE_TECH => unit(a0)?.build_queue_technology(a1 as usize).unwrap_or(0),
        EE_UNITS_PLAYER_PENDING_COUNT => me.pending_units_len() as i64,
        EE_UNITS_PLAYER_PENDING_POPULATION => {
            let pending = me.pending_unit_handle(a0 as usize) as i64;
            population(&unit(pending)?.class()?) as i64
        }
        EE_UNITS_PLAYER_COMPLEX_VALUES => {
            let x = unit(a0)?;
            let flight: c_long = crate::world_field!(x.complex(), mFlightTime);
            a[0] = flight as i64;
            a[1] = x.power();
            0
        }
        EE_UNITS_PLAYER_UNIT_INFO => {
            let r = unit_info(&unit(a0)?);
            // SAFETY: the record the caller passed at `i[1]`
            unsafe { (a1 as *mut EeUnitsPlayerUnit).write(r) };
            0
        }
        EE_UNITS_PLAYER_IS_LOCAL => i64::from(player == current),
        EE_UNITS_PLAYER_IS_CURRENT => i64::from(other == current),
        EE_UNITS_PLAYER_NUM_PLAYERS => w.num_players() as i64,
        EE_UNITS_PLAYER_ID => i64::from(me.id()),
        EE_UNITS_PLAYER_AT => {
            if (a0 as u64) < w.num_players() {
                w.player(a0 as usize).map_or(0, |p| p.handle() as i64)
            } else {
                0
            }
        }
        EE_UNITS_PLAYER_VIEW => {
            // SAFETY: a live player (the diplomacy's operand)
            let p = unsafe { PlayerView::of(&t, other) }?;
            a[2] = p.enemy(a1 as usize) as i64;
            a[0] = i64::from(p.id());
            a[1] = i64::from(p.save_vis_map());
            0
        }
        EE_UNITS_PLAYER_VIEWED_OF => {
            let viewed = me.viewed_unit_handle();
            let owner = if viewed != 0 { unit(viewed as i64)?.player().map_or(0, |p| p.handle()) } else { 0 };
            i64::from(viewed != 0 && owner == other)
        }
        EE_UNITS_PLAYER_AREA_COUNT => (if a0 == 0 { me.towns_len() } else { me.herds_len() }) as i64,
        EE_UNITS_PLAYER_AREA_AT => {
            (if a0 == 0 { me.town_handle(a1 as usize) } else { me.herd_handle(a1 as usize) }) as i64
        }
        _ => return None,
    })
}
