//! The unit utilities' C++ adapters' bodies (EEFOFindNewTargetNearOldTarget.cpp): each turned the C++ call's
//! `long`s, `long*`s, `EEUnit**`s and `bool&`s into the uu entry point's 32-bit tiles and handles and back; here they
//! take the C++ types (`c_long`, pointers) and call the same entry point (uu_ffi.rs, with its UUTL note), so each
//! C++ function is one call.

#![allow(clippy::unnecessary_cast)] // c_long is 32 bits on Windows, 64 elsewhere

use core::ffi::c_long;

pub use crate::units::goal_calls::vector_pointers;

/// The uu entry points the adapters call (uu_ffi.rs's, each with its note).
pub trait UuCalls {
    fn find_free_farm(&mut self, unit: u64, free_only: bool, ring: &mut i32) -> u64;
    fn nearest_footprint_tile(&mut self, unit: u64, point: [f32; 3], out: &mut [i32; 2]) -> f32;
    fn tile_in_direction(&mut self, x: i32, y: i32, direction: i32, distance: i32, out: &mut [i32; 2]);
    fn slot_tile(&mut self, x: i32, y: i32, direction: i32, out: &mut [i32; 2]);
    fn path_length_to(&mut self, unit: u64, tile: &mut [i32; 2], limit: u32, walls: bool) -> f32;
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool, wall: &mut u64) -> bool;
    /// the wall written only when `written` is set
    fn line_blocked(
        &mut self,
        from: [i32; 2],
        to: [i32; 2],
        wall: &mut u64,
        written: &mut u8,
        last: &mut [i32; 2],
    ) -> bool;
    /// `flag` 2 until the tile's blockers were asked
    #[allow(clippy::too_many_arguments)]
    fn can_place_unit_on_tile(
        &mut self,
        player: u64,
        unit: u64,
        unit_type: i32,
        x: i32,
        y: i32,
        skip: bool,
        ignore: i32,
        flag: &mut u8,
    ) -> bool;
    fn editor_can_place(&mut self, player: u64, unit_type: i32, at: [f32; 3], x: i32, y: i32) -> bool;
}

/// `EEFindFreeFarm(unit, freeOnly, ioRing)`: the ring through an `int32_t` both ways.
pub fn find_free_farm<U: UuCalls + ?Sized>(u: &mut U, unit: u64, free_only: bool, ring: &mut c_long) -> u64 {
    let mut r = *ring as i32;
    let farm = u.find_free_farm(unit, free_only, &mut r);
    *ring = r as c_long;
    farm
}

/// `EEGetFootprintDistance(unit, x, y, outX, outY)`: from the tile's centre.
pub fn footprint_distance<U: UuCalls + ?Sized>(
    u: &mut U,
    unit: u64,
    x: c_long,
    y: c_long,
    out: (&mut c_long, &mut c_long),
) -> f32 {
    let mut tile = [0i32; 2];
    let d = u.nearest_footprint_tile(unit, [x as f32 + 0.5, y as f32 + 0.5, 0.0], &mut tile);
    *out.0 = tile[0] as c_long;
    *out.1 = tile[1] as c_long;
    d
}

/// `EEGetPathLengthTo(unit, ioTile, limit, walls)`: the tile in and out.
pub fn path_length<U: UuCalls + ?Sized>(
    u: &mut U,
    unit: u64,
    io: (&mut c_long, &mut c_long),
    limit: u32,
    walls: bool,
) -> f32 {
    let mut t = [*io.0 as i32, *io.1 as i32];
    let r = u.path_length_to(unit, &mut t, limit, walls);
    *io.0 = t[0] as c_long;
    *io.1 = t[1] as c_long;
    r
}

/// `EEIsLineBlocked(x1, y1, x2, y2, outWall, outX, outY)`: the wall written when a tile was asked, the last free tile
/// always.
pub fn line_blocked<U: UuCalls + ?Sized>(
    u: &mut U,
    from: [c_long; 2],
    to: [c_long; 2],
    wall: &mut u64,
    out: (&mut c_long, &mut c_long),
) -> bool {
    let (mut found, mut written, mut last) = (0u64, 0u8, [0i32; 2]);
    let r = u.line_blocked(
        [from[0] as i32, from[1] as i32],
        [to[0] as i32, to[1] as i32],
        &mut found,
        &mut written,
        &mut last,
    );
    if written != 0 {
        *wall = found;
    }
    *out.0 = last[0] as c_long;
    *out.1 = last[1] as c_long;
    r
}

/// `EECanPlaceUnitOnTile(player, unit, type, x, y, skip, ignoreOwner, outFlag)`: the flag written only when the
/// tile's blockers were asked (as the C++ wrote it).
#[allow(clippy::too_many_arguments)]
pub fn can_place_on_tile<U: UuCalls + ?Sized>(
    u: &mut U,
    player: u64,
    unit: u64,
    unit_type: c_long,
    tile: [c_long; 2],
    skip: bool,
    ignore: c_long,
    flag: &mut bool,
) -> bool {
    let mut f = 2u8;
    let r = u.can_place_unit_on_tile(
        player,
        unit,
        unit_type as i32,
        tile[0] as i32,
        tile[1] as i32,
        skip,
        ignore as i32,
        &mut f,
    );
    if f != 2 {
        *flag = f != 0;
    }
    r
}

/// `EECanPlace(player, type, location, x, y, skip, ignoreOwner)`: the editor's test under the free camera
/// (`gFreeCamera`), else the tile's (its flag dropped).
#[allow(clippy::too_many_arguments)]
pub fn can_place<U: UuCalls + ?Sized>(
    u: &mut U,
    free_camera: bool,
    player: u64,
    unit_type: c_long,
    at: [f32; 3],
    tile: [c_long; 2],
    skip: bool,
    ignore: c_long,
) -> bool {
    if free_camera {
        return u.editor_can_place(player, unit_type as i32, at, tile[0] as i32, tile[1] as i32);
    }
    let mut flag = false;
    can_place_on_tile(u, player, 0, unit_type, tile, skip, ignore, &mut flag)
}

/// cbindgen:ignore
#[cfg(not(test))]
mod game {
    use super::*;
    use crate::units::uu_ffi as f;

    pub struct Game;

    // SAFETY (every call): local arguments; the C++ caller's live handles
    impl UuCalls for Game {
        fn find_free_farm(&mut self, unit: u64, free_only: bool, ring: &mut i32) -> u64 {
            unsafe { f::ee_units_uu_find_free_farm(unit, u8::from(free_only), ring) }
        }
        fn nearest_footprint_tile(&mut self, unit: u64, point: [f32; 3], out: &mut [i32; 2]) -> f32 {
            unsafe { f::ee_units_uu_nearest_footprint_tile(unit, &point, out) }
        }
        fn tile_in_direction(&mut self, x: i32, y: i32, direction: i32, distance: i32, out: &mut [i32; 2]) {
            unsafe { f::ee_units_uu_tile_in_direction(x, y, direction, distance, out) }
        }
        fn slot_tile(&mut self, x: i32, y: i32, direction: i32, out: &mut [i32; 2]) {
            unsafe { f::ee_units_uu_slot_tile(x, y, direction, out) }
        }
        fn path_length_to(&mut self, unit: u64, tile: &mut [i32; 2], limit: u32, walls: bool) -> f32 {
            unsafe { f::ee_units_uu_path_length_to(unit, tile, limit, u8::from(walls)) }
        }
        fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool, wall: &mut u64) -> bool {
            unsafe { f::ee_units_uu_tile_top_above(x, y, height, u8::from(bare), wall) != 0 }
        }
        fn line_blocked(
            &mut self,
            from: [i32; 2],
            to: [i32; 2],
            wall: &mut u64,
            written: &mut u8,
            last: &mut [i32; 2],
        ) -> bool {
            unsafe { f::ee_units_uu_line_blocked(from[0], from[1], to[0], to[1], wall, written, last) != 0 }
        }
        fn can_place_unit_on_tile(
            &mut self,
            player: u64,
            unit: u64,
            unit_type: i32,
            x: i32,
            y: i32,
            skip: bool,
            ignore: i32,
            flag: &mut u8,
        ) -> bool {
            unsafe {
                f::ee_units_uu_can_place_unit_on_tile(player, unit, unit_type, x, y, u8::from(skip), ignore, flag) != 0
            }
        }
        fn editor_can_place(&mut self, player: u64, unit_type: i32, at: [f32; 3], x: i32, y: i32) -> bool {
            unsafe { f::ee_units_uu_editor_can_place(player, unit_type, &at, x, y) != 0 }
        }
    }
}

// The entry points (each the C++ adapter's whole body; every handle live or 0, every pointer writable).

/// `EEGetDopplerPitch(unit, sound, outVolume, outPan, outPitch)`: the pitch of the unit's complex unit's velocity
/// and location from the camera (`camera` the world's transformation's position).
///
/// # Safety
/// `unit` a live unit with a complex unit; `pitch` writable.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_doppler_pitch_of(
    unit: u64,
    volume: f32,
    cx: f32,
    cy: f32,
    cz: f32,
    pitch: *mut f32,
) {
    let (velocity, location) = crate::units::unit_geom::with_unit(unit, |u| {
        let c = u.complex_unit().expect("a unit's complex unit");
        (crate::world_field!(c.complex(), mVelocity), c.transform())
    })
    .expect("a live unit");
    // SAFETY: local arguments; the caller's pointer
    unsafe { crate::units::uu_ffi::ee_units_uu_doppler_pitch(unit, volume, velocity, &location, &[cx, cy, cz], pitch) }
}

/// `EEFindFreeFarm`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_find_free_farm_at(unit: u64, free_only: bool, ring: *mut c_long) -> u64 {
    find_free_farm(&mut game::Game, unit, free_only, unsafe { &mut *ring })
}

/// `EEGetFootprintDistance`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_footprint_distance(
    unit: u64,
    x: c_long,
    y: c_long,
    out_x: *mut c_long,
    out_y: *mut c_long,
) -> f32 {
    footprint_distance(&mut game::Game, unit, x, y, unsafe { (&mut *out_x, &mut *out_y) })
}

/// `EEGetNextTile(inTile, outTile, direction, distance)`
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
pub unsafe extern "C" fn ee_units_uu_next_tile(
    x: c_long,
    y: c_long,
    direction: c_long,
    distance: c_long,
    out_x: *mut c_long,
    out_y: *mut c_long,
) {
    let mut t = [0i32; 2];
    game::Game.tile_in_direction(x as i32, y as i32, direction as i32, distance as i32, &mut t);
    unsafe {
        *out_x = t[0] as c_long;
        *out_y = t[1] as c_long;
    }
}

/// `EEIsLineBlocked` (`wall` an `EEUnit**`)
/// # Safety
/// As above.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_line_blocked_at(
    x1: c_long,
    y1: c_long,
    x2: c_long,
    y2: c_long,
    wall: *mut u64,
    out_x: *mut c_long,
    out_y: *mut c_long,
) -> bool {
    line_blocked(&mut game::Game, [x1, y1], [x2, y2], unsafe { &mut *wall }, unsafe { (&mut *out_x, &mut *out_y) })
}

/// `EECanPlace` (`free_camera` the C++'s `gFreeCamera`)
/// # Safety
/// As above; `at` three readable floats.
#[cfg(not(test))]
#[no_mangle]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn ee_units_uu_can_place_at(
    free_camera: bool,
    player: u64,
    unit_type: c_long,
    at: *const [f32; 3],
    x: c_long,
    y: c_long,
    skip_player_checks: bool,
    ignore_owner: c_long,
) -> bool {
    can_place(&mut game::Game, free_camera, player, unit_type, unsafe { *at }, [x, y], skip_player_checks, ignore_owner)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Mock {
        log: Vec<String>,
        flag: u8,
        written: u8,
    }

    impl UuCalls for Mock {
        fn find_free_farm(&mut self, unit: u64, free_only: bool, ring: &mut i32) -> u64 {
            self.log.push(format!("farm {unit} {free_only} {ring}"));
            *ring += 1;
            0x40
        }
        fn nearest_footprint_tile(&mut self, unit: u64, point: [f32; 3], out: &mut [i32; 2]) -> f32 {
            self.log.push(format!("footprint {unit} {point:?}"));
            *out = [7, 8];
            2.5
        }
        fn tile_in_direction(&mut self, _: i32, _: i32, _: i32, _: i32, _: &mut [i32; 2]) {}
        fn slot_tile(&mut self, _: i32, _: i32, _: i32, _: &mut [i32; 2]) {}
        fn path_length_to(&mut self, unit: u64, tile: &mut [i32; 2], limit: u32, walls: bool) -> f32 {
            self.log.push(format!("path {unit} {tile:?} {limit} {walls}"));
            *tile = [tile[0] + 1, tile[1] + 1];
            4.0
        }
        fn tile_top_above(&mut self, _: i32, _: i32, _: f32, _: bool, _: &mut u64) -> bool {
            false
        }
        fn line_blocked(
            &mut self,
            from: [i32; 2],
            to: [i32; 2],
            wall: &mut u64,
            written: &mut u8,
            last: &mut [i32; 2],
        ) -> bool {
            self.log.push(format!("line {from:?} {to:?}"));
            *wall = 0x99;
            *written = self.written;
            *last = [3, 4];
            true
        }
        fn can_place_unit_on_tile(
            &mut self,
            player: u64,
            unit: u64,
            t: i32,
            x: i32,
            y: i32,
            skip: bool,
            ignore: i32,
            flag: &mut u8,
        ) -> bool {
            self.log.push(format!("place {player} {unit} {t} {x} {y} {skip} {ignore}"));
            *flag = self.flag;
            true
        }
        fn editor_can_place(&mut self, player: u64, t: i32, at: [f32; 3], x: i32, y: i32) -> bool {
            self.log.push(format!("editor {player} {t} {at:?} {x} {y}"));
            false
        }
    }

    #[test]
    fn a_vectors_pointers_are_read_from_its_begin_and_end() {
        let items: Vec<usize> = vec![0x10, 0x20, 0x30];
        let words = [items.as_ptr() as usize, items.as_ptr() as usize + 3 * std::mem::size_of::<usize>(), 0];
        // SAFETY: a vector-shaped local
        assert_eq!(unsafe { vector_pointers(words.as_ptr() as *const core::ffi::c_void) }, [0x10, 0x20, 0x30]);
        let empty = [0usize; 3];
        // SAFETY: as above
        assert!(unsafe { vector_pointers(empty.as_ptr() as *const core::ffi::c_void) }.is_empty());
    }

    #[test]
    fn the_ring_and_tiles_go_through_32_bits_both_ways() {
        let mut m = Mock::default();
        let mut ring: c_long = 3;
        assert_eq!(find_free_farm(&mut m, 5, true, &mut ring), 0x40);
        assert_eq!(ring, 4);
        let (mut x, mut y): (c_long, c_long) = (0, 0);
        assert_eq!(footprint_distance(&mut m, 5, 10, 11, (&mut x, &mut y)), 2.5);
        assert_eq!((x, y), (7, 8));
        let (mut x, mut y): (c_long, c_long) = (20, 21);
        assert_eq!(path_length(&mut m, 5, (&mut x, &mut y), 9, true), 4.0);
        assert_eq!((x, y), (21, 22));
        assert_eq!(m.log, ["farm 5 true 3", "footprint 5 [10.5, 11.5, 0.0]", "path 5 [20, 21] 9 true"]);
    }

    #[test]
    fn the_line_writes_its_wall_only_when_a_tile_was_asked() {
        let mut m = Mock::default();
        let (mut wall, mut x, mut y): (u64, c_long, c_long) = (1, 0, 0);
        assert!(line_blocked(&mut m, [1, 2], [3, 4], &mut wall, (&mut x, &mut y)));
        assert_eq!((wall, x, y), (1, 3, 4));
        m.written = 1;
        assert!(line_blocked(&mut m, [1, 2], [3, 4], &mut wall, (&mut x, &mut y)));
        assert_eq!(wall, 0x99);
    }

    #[test]
    fn the_place_flag_is_written_only_when_asked_and_the_free_camera_asks_the_editor() {
        let mut m = Mock { flag: 2, ..Default::default() };
        let mut flag = true;
        assert!(can_place_on_tile(&mut m, 1, 2, 3, [4, 5], true, 6, &mut flag));
        assert!(flag);
        m.flag = 0;
        assert!(can_place_on_tile(&mut m, 1, 2, 3, [4, 5], true, 6, &mut flag));
        assert!(!flag);
        let mut m = Mock::default();
        assert!(!can_place(&mut m, true, 1, 3, [0.5, 1.5, 0.0], [4, 5], false, 0));
        assert!(can_place(&mut m, false, 1, 3, [0.5, 1.5, 0.0], [4, 5], false, 0));
        assert_eq!(m.log, ["editor 1 3 [0.5, 1.5, 0.0] 4 5", "place 1 0 3 4 5 false 0"]);
    }
}
