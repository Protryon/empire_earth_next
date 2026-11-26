//! `RTSWorld`'s bodies (LLE round 8, from `src/RTS Engine/RTS Engine/RTSWorld.cpp`): the world's construction and
//! teardown, the population limit split, the players' and the map's clearing, the checksum, the player and unit
//! lookups, the placement queue and the player slots. Simulation code (the oracle's seeds run every one of them).
//!
//! The class stays C++ (`ee/RTSWorld.h`: EEWorld derives from it); its members are reached through one-statement
//! shims of `RTSWorld.cpp` ([`WorldCpp`]): the players array and the counters through their addresses, the players'
//! and the map's virtuals by call.

use core::ffi::{c_long, c_ulong, c_void};

ee_base::cpp_shims! {
    trait WorldCpp;
    fn players(world: usize) -> *mut usize = ee_rtswcpp_players;
    fn num_players(world: usize) -> *mut c_ulong = ee_rtswcpp_num_players;
    fn population_limit(world: usize) -> *mut c_long = ee_rtswcpp_population_limit;
    fn player_population_limit(world: usize) -> *mut c_long = ee_rtswcpp_player_population_limit;
    fn map_slot(world: usize) -> *mut usize = ee_rtswcpp_map_slot;
    fn lock_slot(world: usize) -> *mut *mut c_void = ee_rtswcpp_lock_slot;
    fn game_speed(world: usize) -> *mut f32 = ee_rtswcpp_game_speed;
    fn camera_yaw(world: usize) -> *mut f32 = ee_rtswcpp_camera_yaw;
    fn camera_pitch(world: usize) -> *mut f32 = ee_rtswcpp_camera_pitch;
    fn player_is_active(player: usize) -> bool = ee_rtswcpp_player_is_active;
    fn player_recount(player: usize) = ee_rtswcpp_player_recount;
    fn player_delete(player: usize) = ee_rtswcpp_player_delete;
    fn player_clear(player: usize) = ee_rtswcpp_player_clear;
    fn player_checksum(player: usize, checksum: usize) = ee_rtswcpp_player_checksum;
    fn player_unit(player: usize, id: c_ulong) -> usize = ee_rtswcpp_player_unit;
    fn map_delete(map: usize) = ee_rtswcpp_map_delete;
    fn map_checksum(map: usize, checksum: usize) = ee_rtswcpp_map_checksum;
    fn checksum_add_world(checksum: usize, players: c_ulong) = ee_rtswcpp_checksum_add_world;
    fn update_population_limit(world: usize, recount: bool) = ee_rtswcpp_update_population_limit;
    fn place_queued_units(world: usize) = ee_rtswcpp_place_queued_units;
    fn unit_reference(unit: usize) = ee_rtswcpp_unit_reference;
    fn unit_dereference(unit: usize) = ee_rtswcpp_unit_dereference;
}

/// The players array's length (`mPlayers[17]`).
const PLAYERS: usize = 17;

fn player<C: WorldCpp>(c: &mut C, world: usize, i: usize) -> usize {
    // SAFETY: the C++ object's array of 17
    unsafe { *c.players(world).add(i) }
}
fn set_player_slot<C: WorldCpp>(c: &mut C, world: usize, i: usize, p: usize) {
    // SAFETY: as player
    unsafe { *c.players(world).add(i) = p };
}
fn count<C: WorldCpp>(c: &mut C, world: usize) -> c_ulong {
    // SAFETY: the C++ object's member
    unsafe { *c.num_players(world) }
}

/// `RTSWorld::RTSWorld`: the lock, no players or map, normal game speed and view, a population limit of 800.
///
/// # Safety
/// `world` is the C++ object under construction.
pub unsafe fn construct<C: WorldCpp>(
    c: &mut C,
    world: usize,
    new_lock: &mut dyn FnMut(*mut *mut c_void) -> c_long,
    verify: &mut dyn FnMut(bool),
) {
    let r = new_lock(c.lock_slot(world));
    verify(r == 0);
    for i in 0..PLAYERS {
        set_player_slot(c, world, i, 0);
    }
    unsafe {
        *c.game_speed(world) = 1.0;
        *c.camera_yaw(world) = 0.0;
        *c.map_slot(world) = 0;
        *c.camera_pitch(world) = 0.0;
        *c.num_players(world) = 0;
        *c.population_limit(world) = 800;
    }
}

/// `RTSWorld::~RTSWorld`: the lock, then the map.
///
/// # Safety
/// `world` is the C++ object being destroyed.
pub unsafe fn destruct<C: WorldCpp>(c: &mut C, world: usize, free_lock: &mut dyn FnMut(*mut c_void)) {
    free_lock(unsafe { *c.lock_slot(world) });
    let map = unsafe { *c.map_slot(world) };
    if map != 0 {
        c.map_delete(map);
    }
}

/// `RTSWorld::UpdatePlayerPopulationLimit`: the limit split among the active players (player 0 excluded; 0 with
/// none), then optionally every player's population recounted.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn update_population_limit<C: WorldCpp>(c: &mut C, world: usize, recount: bool) {
    let n = count(c, world);
    let mut active: c_ulong = 0;
    for i in 1..n as usize {
        let p = player(c, world, i);
        if p != 0 && c.player_is_active(p) {
            active += 1;
        }
    }
    unsafe {
        *c.player_population_limit(world) =
            (*c.population_limit(world) as c_ulong).checked_div(active).map_or(0, |v| v as c_long);
    }
    if !recount {
        return;
    }
    for i in 1..n as usize {
        let p = player(c, world, i);
        if p != 0 {
            c.player_recount(p);
        }
    }
}

/// `RTSWorld::Clear`: every player deleted, then the map.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn clear<C: WorldCpp>(c: &mut C, world: usize) {
    for i in 0..PLAYERS {
        let p = player(c, world, i);
        if p != 0 {
            c.player_delete(p);
        }
        set_player_slot(c, world, i, 0);
    }
    unsafe { delete_map(c, world) };
}

unsafe fn delete_map<C: WorldCpp>(c: &mut C, world: usize) {
    let map = unsafe { *c.map_slot(world) };
    if map != 0 {
        c.map_delete(map);
    }
    unsafe { *c.map_slot(world) = 0 };
}

/// `RTSWorld::Reset`: every player cleared (kept), the map deleted.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn reset<C: WorldCpp>(c: &mut C, world: usize) {
    for i in 0..PLAYERS {
        let p = player(c, world, i);
        if p != 0 {
            c.player_clear(p);
        }
    }
    unsafe { delete_map(c, world) };
}

/// `RTSWorld::GetChecksum`: every player's, the map's, then ("RTSWorld", the player count).
///
/// # Safety
/// `world` is the live object, `checksum` an `ESChecksum*`.
pub unsafe fn checksum<C: WorldCpp>(c: &mut C, world: usize, checksum: usize) {
    for i in 0..count(c, world) as usize {
        let p = player(c, world, i);
        if p != 0 {
            c.player_checksum(p, checksum);
        }
    }
    let map = unsafe { *c.map_slot(world) };
    if map != 0 {
        c.map_checksum(map, checksum);
    }
    let n = count(c, world);
    c.checksum_add_world(checksum, n);
}

/// `RTSWorld::GetPlayerIndex`: the last index of the player among the counted ones, -1 if absent.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn player_index<C: WorldCpp>(c: &mut C, world: usize, wanted: usize) -> c_long {
    let mut result = -1;
    for i in 0..count(c, world) as usize {
        if player(c, world, i) == wanted {
            result = i as c_long;
        }
    }
    result
}

/// `RTSWorld::GetUnit`: the unit of an id through its player (the id's top byte), NULL past the counted players.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn unit<C: WorldCpp>(c: &mut C, world: usize, id: c_ulong) -> usize {
    let index = (id >> 24) as usize;
    if (index as c_ulong) < count(c, world) {
        let p = player(c, world, index);
        return c.player_unit(p, id);
    }
    0
}

/// The placement queue (the C++'s `RTSWorld::mQueuedPlacements`, a `std::vector` at +0x04 that put every later
/// member's offset at the standard library's mercy): (the unit, the unit to place it after), referenced while queued.
/// One world (gWorld).
static QUEUED: std::sync::Mutex<Vec<(usize, usize)>> = std::sync::Mutex::new(Vec::new());

fn queue() -> std::sync::MutexGuard<'static, Vec<(usize, usize)>> {
    QUEUED.lock().unwrap_or_else(|e| e.into_inner())
}

/// The queued placements in their order (EEWorld::PlaceQueuedUnits' walk).
pub fn queued() -> Vec<(usize, usize)> {
    queue().clone()
}

/// The queue emptied -> its entries (the caller releases their units).
pub fn take_queued() -> Vec<(usize, usize)> {
    std::mem::take(&mut *queue())
}

/// `RTSWorld::FlushQueuedPlacements`: the queued units placed (a virtual), then released and the queue emptied.
///
/// # Safety
/// `world` is the live object.
pub unsafe fn flush_queued_placements<C: WorldCpp>(c: &mut C, world: usize) {
    c.place_queued_units(world);
    for (u, _) in take_queued() {
        c.unit_dereference(u);
    }
}

/// `RTSWorld::AddUnitEntry`: the unit referenced and queued for placement after another.
pub fn add_unit_entry<C: WorldCpp>(c: &mut C, _world: usize, unit: usize, after: usize) {
    c.unit_reference(unit);
    queue().push((unit, after));
}

/// `RTSWorld::SetPlayer`: the old player deleted unless it is the same one.
///
/// # Safety
/// `world` is the live object; `index` < 17.
pub unsafe fn set_player<C: WorldCpp>(c: &mut C, world: usize, index: usize, p: usize) {
    let old = player(c, world, index);
    if p == old {
        return;
    }
    if old != 0 {
        c.player_delete(old);
    }
    set_player_slot(c, world, index, p);
}

/// `RTSWorld::SetNumPlayers`: the count, then the limit re-split with a recount (a virtual).
///
/// # Safety
/// `world` is the live object.
pub unsafe fn set_num_players<C: WorldCpp>(c: &mut C, world: usize, n: c_ulong) {
    unsafe { *c.num_players(world) = n };
    c.update_population_limit(world, true);
}

#[cfg(not(test))]
pub(crate) mod ffi {
    use super::*;
    use ee_base::shim::ShimFfi as Ffi;

    /// `RTSWorld::RTSWorld`.
    ///
    /// # Safety
    /// `world` is the C++ object under construction.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_construct(world: *mut c_void) {
        let mut lock = |slot: *mut *mut c_void| unsafe { ee_base::ts::ee_ts_lock_new(slot.cast()) };
        let mut verify = |ok: bool| {
            if !ok {
                // SAFETY: C strings
                unsafe {
                    ee_base::error::ee_error_verify(
                        c"C:\\EEX\\RTS Engine\\RTS Engine\\RTSWorld.cpp".as_ptr(),
                        49,
                        c"TSScheduler::CreateReadWriteLock(this->mLock) == keseOK".as_ptr(),
                    )
                }
            }
        };
        unsafe { construct(&mut Ffi, world as usize, &mut lock, &mut verify) }
    }

    /// `RTSWorld::~RTSWorld`.
    ///
    /// # Safety
    /// `world` is the C++ object being destroyed.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_destruct(world: *mut c_void) {
        let mut free = |l: *mut c_void| unsafe { ee_base::ts::ee_ts_lock_free(l.cast()) };
        unsafe { destruct(&mut Ffi, world as usize, &mut free) }
    }

    /// `RTSWorld::UpdatePlayerPopulationLimit`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_update_population_limit(world: *mut c_void, recount: bool) {
        unsafe { update_population_limit(&mut Ffi, world as usize, recount) }
    }

    /// `RTSWorld::Clear`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_clear(world: *mut c_void) {
        unsafe { clear(&mut Ffi, world as usize) }
    }

    /// `RTSWorld::Reset`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_reset(world: *mut c_void) {
        unsafe { reset(&mut Ffi, world as usize) }
    }

    /// `RTSWorld::GetChecksum`.
    ///
    /// # Safety
    /// `world` is the live object, `checksum` an `ESChecksum*`.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_checksum(world: *mut c_void, checksum: *mut c_void) {
        unsafe { super::checksum(&mut Ffi, world as usize, checksum as usize) }
    }

    /// `RTSWorld::GetUnit`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_unit(world: *mut c_void, id: c_ulong) -> *mut c_void {
        unsafe { unit(&mut Ffi, world as usize, id) as *mut c_void }
    }

    /// `RTSWorld::FlushQueuedPlacements`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_flush_queued_placements(world: *mut c_void) {
        unsafe { flush_queued_placements(&mut Ffi, world as usize) }
    }

    /// `RTSWorld::AddUnitEntry`.
    ///
    /// # Safety
    /// `world` is the live object, `unit` a live unit.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_add_unit_entry(world: *mut c_void, unit: *mut c_void, after: *mut c_void) {
        add_unit_entry(&mut Ffi, world as usize, unit as usize, after as usize)
    }

    /// `RTSWorld::SetPlayer`.
    ///
    /// # Safety
    /// `world` is the live object; `index` < 17.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_set_player(world: *mut c_void, index: c_long, player: *mut c_void) {
        unsafe { set_player(&mut Ffi, world as usize, index as usize, player as usize) }
    }

    /// `RTSWorld::SetNumPlayers`.
    ///
    /// # Safety
    /// `world` is the live object.
    #[no_mangle]
    pub unsafe extern "C" fn ee_rts_world_set_num_players(world: *mut c_void, n: c_ulong) {
        unsafe { set_num_players(&mut Ffi, world as usize, n) }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use ee_base::shim::ShimFake;

    struct World {
        players: [usize; 17],
        n: c_ulong,
        limit: c_long,
        per_player: c_long,
        map: usize,
    }

    fn fake(w: &mut World) -> ShimFake {
        let mut c = ShimFake::default();
        c.answer("players", &[w.players.as_mut_ptr() as i64; 128]);
        c.answer("num_players", &[&mut w.n as *mut _ as i64; 64]);
        c.answer("population_limit", &[&mut w.limit as *mut _ as i64; 8]);
        c.answer("player_population_limit", &[&mut w.per_player as *mut _ as i64; 8]);
        c.answer("map_slot", &[&mut w.map as *mut _ as i64; 8]);
        c
    }

    fn world() -> Box<World> {
        let mut players = [0; 17];
        players[0] = 10;
        players[1] = 11;
        players[3] = 13;
        Box::new(World { players, n: 4, limit: 800, per_player: 0, map: 0x70 })
    }

    #[test]
    fn the_limit_is_split_among_the_active_players_but_player_0() {
        let mut w = world();
        let mut c = fake(&mut w);
        c.answer("player_is_active", &[1, 1]);
        unsafe { update_population_limit(&mut c, 1, true) };
        assert_eq!(w.per_player, 400);
        let calls = c.calls();
        assert!(calls.contains("player_is_active(11)") && calls.contains("player_is_active(13)"));
        assert!(!calls.contains("player_is_active(10)"));
        assert_eq!(c.count("player_recount"), 2);
        let mut c = fake(&mut w);
        unsafe { update_population_limit(&mut c, 1, false) };
        assert_eq!(w.per_player, 0, "no active player");
        assert_eq!(c.count("player_recount"), 0);
    }

    #[test]
    fn lookups_checksum_and_slots() {
        let mut w = world();
        let mut c = fake(&mut w);
        assert_eq!(unsafe { player_index(&mut c, 1, 13) }, 3);
        assert_eq!(unsafe { player_index(&mut c, 1, 99) }, -1);
        c.answer("player_unit", &[0x500]);
        assert_eq!(unsafe { unit(&mut c, 1, 0x0300_0005) }, 0x500);
        assert!(c.calls().contains("player_unit(13, 50331653)"));
        assert_eq!(unsafe { unit(&mut c, 1, 0x0700_0005) }, 0);
        let mut c = fake(&mut w);
        unsafe { checksum(&mut c, 1, 9) };
        let names: Vec<&str> =
            c.log.iter().map(|l| l.split('(').next().unwrap()).filter(|n| n.contains("checksum")).collect();
        assert_eq!(
            names,
            ["player_checksum", "player_checksum", "player_checksum", "map_checksum", "checksum_add_world"]
        );
        let mut c = fake(&mut w);
        unsafe { set_player(&mut c, 1, 3, 13) };
        assert_eq!(c.count("player_delete"), 0);
        unsafe { set_player(&mut c, 1, 3, 14) };
        assert!(c.calls().contains("player_delete(13)"));
        assert_eq!(w.players[3], 14);
    }

    #[test]
    fn clear_deletes_and_reset_keeps_the_players() {
        let mut w = world();
        let mut c = fake(&mut w);
        unsafe { reset(&mut c, 1) };
        assert_eq!(c.count("player_clear"), 3);
        assert_eq!(w.map, 0);
        assert_eq!(w.players[1], 11);
        w.map = 0x71;
        let mut c = fake(&mut w);
        unsafe { clear(&mut c, 1) };
        assert_eq!(c.count("player_delete"), 3);
        assert!(w.players.iter().all(|&p| p == 0));
        assert!(c.calls().contains("map_delete(113)"));
    }

    #[test]
    fn the_queue_is_placed_released_and_emptied() {
        let mut c = ShimFake::default();
        add_unit_entry(&mut c, 1, 5, 8);
        add_unit_entry(&mut c, 1, 6, 5);
        assert_eq!(queued(), [(5, 8), (6, 5)]);
        unsafe { flush_queued_placements(&mut c, 1) };
        assert_eq!(
            c.calls(),
            "unit_reference(5)\nunit_reference(6)\nplace_queued_units(1)\nunit_dereference(5)\nunit_dereference(6)"
        );
        assert!(queued().is_empty());
    }
}
