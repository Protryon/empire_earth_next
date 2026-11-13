//! The scenario editor's unit placement and move (`EESEAPlaceUnit`, `EESEAMoveUnit`), whole in Rust behind an
//! `EESEARust` (`editor_whole.rs`'s kinds `EE_SCREENS_SEW_PLACE_UNIT` and `EE_SCREENS_SEW_MOVE_UNIT`):
//! - the placement queues a unit for each placeable ghost (a building on its tiles, a unit boarding the fitting
//!   transport under the mouse; a blocked granary ghost among others cancels all; an airport's ghost ends the list),
//!   then creates them: each pitched with the terrain when its class pitches (the pitch kept for the next units, as
//!   the original's), turned to its yaw, made, built and powered, loaded into its transport;
//! - the move picks a unit up (every part), lifts it off the map and shows its ghost at the mouse; put down, each part
//!   goes where its ghost is (into a transport under the mouse, or on its tile pitched and levelled, an airport's
//!   parts aligned); cancelled, the picked-up unit is deleted.
//!
//! Units are held by id (the original's `EEUnitHandle`s), referenced while held. The world is the host's
//! ([`SepHost`]); every call in the original's order, the floats the original's.

use ee_ge::transform::RotationOrder;
use ee_ge::types::GeTransformation;

// ---- the host's ops (`ee_screenscpp_sep`: `t` a transformation, `i` up to 8 integers, `f` up to 4 floats)

/// The ghosts' lock taken (i0 != 0) or let go.
pub const EE_SCREENS_SEP_GHOST_LOCK: u32 = 1;
/// -> the ghosts' count.
pub const EE_SCREENS_SEP_GHOST_COUNT: u32 = 2;
/// The ghost i0 -> whether it can be placed (0 also when there is no such ghost); i1 its class's type, i2, i3 its
/// tile, i4 its class's tile size, i5 its animation, i6 flags ([`EE_SCREENS_SEP_F_CLASS`] ..), f0 the class's cruise altitude;
/// t its transformation.
pub const EE_SCREENS_SEP_GHOST: u32 = 3;
/// -> whether the tile (i0, i1) is on the map; f0 its height.
pub const EE_SCREENS_SEP_TILE_HEIGHT: u32 = 4;
/// -> the unit tab's unit under the mouse (-1: none).
pub const EE_SCREENS_SEP_UNDER_MOUSE: u32 = 5;
/// The unit i0 (under the mouse) as a transport for a unit of type i2 of the player i1 (i1 -1: the player of the
/// unit i3) -> whether it is one; i4 its id, referenced.
pub const EE_SCREENS_SEP_BOARD: u32 = 6;
/// The player i0 loads the media of the unit type i1.
pub const EE_SCREENS_SEP_LOAD_MEDIA: u32 = 7;
/// The action posted (p: its `EESEARust`).
pub const EE_SCREENS_SEP_POST: u32 = 8;
/// -> whether the unit type i0's class pitches with the terrain.
pub const EE_SCREENS_SEP_PITCHES: u32 = 9;
/// -> the terrain's pitch under t (`GetTerrainPitch`) in f0.
pub const EE_SCREENS_SEP_TERRAIN_PITCH: u32 = 10;
/// A unit of type i1 for the player i0 made at t (a camera object cloaked on t's nearest tile), its media released,
/// a land building built with the animation i2, its power given; when i3 is a transport's id (not -1), loaded into
/// it, the transport selected and the unit tab's selection updated, the transport released.
pub const EE_SCREENS_SEP_CREATE: u32 = 11;
/// The unit i0 released.
pub const EE_SCREENS_SEP_RELEASE: u32 = 12;
/// The unit i0's parts' ids (each referenced) into p (i32s, room for i1) -> their count (0: no such unit).
pub const EE_SCREENS_SEP_PARTS: u32 = 13;
/// The unit i0 moved off the map (-10000, -10000).
pub const EE_SCREENS_SEP_LIFT: u32 = 14;
/// The ghosts cleared.
pub const EE_SCREENS_SEP_CLEAR_GHOSTS: u32 = 15;
/// The unit i0 -> flags ([`EE_SCREENS_SEP_U_EXISTS`] ..); t its complex unit's location.
pub const EE_SCREENS_SEP_UNIT: u32 = 16;
/// The unit i0's ghost made at the mouse turned to f0 (`EEGhostUnit::Create`, a move).
pub const EE_SCREENS_SEP_GHOST_FOR: u32 = 17;
/// The unit i0 loaded into the transport i1, the transport selected, the selection updated, the transport released.
pub const EE_SCREENS_SEP_LOAD_INTO: u32 = 18;
/// The unit i0 moved to the tile (i1, i2) (`MoveTo`, as its complex unit when it has one).
pub const EE_SCREENS_SEP_MOVE_TO: u32 = 19;
/// The unit i0's location set to t.
pub const EE_SCREENS_SEP_SET_LOCATION: u32 = 20;
/// The airport i0's parts turned to its facing quarter and their ground levelled (f0: the height in and out).
pub const EE_SCREENS_SEP_AIRPORT: u32 = 21;
/// The unit i0's class levels the ground at the tile (i1, i2) (f0: the height in and out).
pub const EE_SCREENS_SEP_LEVEL: u32 = 22;
/// The unit i0 deleted (`new EESEADeleteUnit(i0)`, posted).
pub const EE_SCREENS_SEP_DELETE: u32 = 23;

/// [`EE_SCREENS_SEP_GHOST`]'s flags: the class exists (`gUnitClassTable`); it is small (tile size up to 1, no land
/// building); an airport; a granary; it flies over.
pub const EE_SCREENS_SEP_F_CLASS: i32 = 1;
pub const EE_SCREENS_SEP_F_SMALL: i32 = 2;
pub const EE_SCREENS_SEP_F_AIRPORT: i32 = 4;
pub const EE_SCREENS_SEP_F_GRANARY: i32 = 8;
pub const EE_SCREENS_SEP_F_FLIES: i32 = 16;
/// [`EE_SCREENS_SEP_UNIT`]'s flags: the unit exists; it is a complex unit; its class flies over; pitches; an airport;
/// a land building other than a camera object.
pub const EE_SCREENS_SEP_U_EXISTS: i32 = 1;
pub const EE_SCREENS_SEP_U_COMPLEX: i32 = 2;
pub const EE_SCREENS_SEP_U_FLIES: i32 = 4;
pub const EE_SCREENS_SEP_U_PITCHES: i32 = 8;
pub const EE_SCREENS_SEP_U_AIRPORT: i32 = 16;
pub const EE_SCREENS_SEP_U_BUILDING: i32 = 32;

/// No unit (`-1`).
const SEP_NONE: i32 = -1;

/// The world.
pub trait SepHost {
    /// One op of the `EE_SCREENS_SEP_*` list; `p` a pointer, `t` a transformation in and out.
    fn sep(&mut self, op: u32, p: usize, t: &mut GeTransformation, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32;
}

fn op<H: SepHost + ?Sized>(h: &mut H, code: u32, ints: &[i32]) -> i32 {
    let mut i = [0i32; 8];
    i[..ints.len()].copy_from_slice(ints);
    h.sep(code, 0, &mut GeTransformation::IDENTITY.clone(), &mut i, &mut [0.0; 4])
}

fn op_t<H: SepHost + ?Sized>(h: &mut H, code: u32, t: &mut GeTransformation, ints: &[i32], f0: f32) -> (i32, f32) {
    let mut i = [0i32; 8];
    i[..ints.len()].copy_from_slice(ints);
    let mut f = [f0, 0.0, 0.0, 0.0];
    let r = h.sep(code, 0, t, &mut i, &mut f);
    (r, f[0])
}

/// A ghost as the host reads it.
struct Ghost {
    can_place: bool,
    unit_type: i32,
    tile: (i32, i32),
    tile_size: i32,
    animation: i32,
    flags: i32,
    cruise: f32,
    t: GeTransformation,
}

fn ghost<H: SepHost + ?Sized>(h: &mut H, index: i32) -> Ghost {
    let mut i = [index, 0, 0, 0, 0, 0, 0, 0];
    let mut f = [0.0f32; 4];
    let mut t = GeTransformation::IDENTITY;
    let can_place = h.sep(EE_SCREENS_SEP_GHOST, 0, &mut t, &mut i, &mut f) != 0;
    Ghost {
        can_place,
        unit_type: i[1],
        tile: (i[2], i[3]),
        tile_size: i[4],
        animation: i[5],
        flags: i[6],
        cruise: f[0],
        t,
    }
}

/// The tile a large ghost puts its unit on: `(unsigned long)(tile - (long)(size * 0.5f)) + 0.5f`.
fn snapped(tile: i32, size: i32) -> f32 {
    let offset = (size as f32 * 0.5) as i32;
    tile.wrapping_sub(offset) as u32 as f32 + 0.5
}

fn set_position(t: &mut GeTransformation, x: f32, y: f32, z: f32) {
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
}

/// The transport under the mouse a unit of `unit_type` boards (`player`, or -1 and the player of `unit`): its id,
/// referenced.
fn board<H: SepHost + ?Sized>(h: &mut H, player: i32, unit: i32, unit_type: i32) -> i32 {
    let under = op(h, EE_SCREENS_SEP_UNDER_MOUSE, &[]);
    if under == SEP_NONE {
        return SEP_NONE;
    }
    let mut i = [under, player, unit_type, unit, 0, 0, 0, 0];
    if h.sep(EE_SCREENS_SEP_BOARD, 0, &mut GeTransformation::IDENTITY.clone(), &mut i, &mut [0.0; 4]) != 0 {
        i[4]
    } else {
        SEP_NONE
    }
}

/// One queued unit (`EESEAPlacedUnit`).
#[derive(Clone, Debug, PartialEq)]
pub struct SepPlaced {
    pub container: i32,
    pub t: GeTransformation,
    pub player: i32,
    pub unit_type: i32,
    pub animation: i32,
}

/// `EESEAPlaceUnit`'s queue.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SepPlace {
    pub units: Vec<SepPlaced>,
}

impl SepPlace {
    /// `Place(player)`: a unit queued for each placeable ghost and the action (`owner`) posted; whether any was.
    pub fn place<H: SepHost + ?Sized>(&mut self, h: &mut H, owner: usize, player: i32) -> bool {
        let mut placed = false;
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[1]);
        let count = op(h, EE_SCREENS_SEP_GHOST_COUNT, &[]);
        for k in 0..count {
            let g = ghost(h, k);
            if !g.can_place {
                if g.flags & EE_SCREENS_SEP_F_GRANARY != 0 && count > 1 {
                    self.units.clear();
                    placed = false;
                    break;
                }
                continue;
            }
            if g.flags & EE_SCREENS_SEP_F_CLASS == 0 {
                continue;
            }
            let mut t = g.t;
            let (x, y, z);
            if g.flags & EE_SCREENS_SEP_F_SMALL != 0 {
                (x, y, z) = (t.m[0][3], t.m[1][3], t.m[2][3]);
            } else {
                // a ghost at the map's low edge has its corner tile off the map: skipped (the port's guard)
                let offset = (g.tile_size as f32 * 0.5) as i32;
                if g.tile.0 < offset || g.tile.1 < offset {
                    continue;
                }
                x = snapped(g.tile.0, g.tile_size);
                y = snapped(g.tile.1, g.tile_size);
                let mut i = [x as i32, y as i32, 0, 0, 0, 0, 0, 0];
                let mut f = [0.0f32; 4];
                if h.sep(EE_SCREENS_SEP_TILE_HEIGHT, 0, &mut GeTransformation::IDENTITY.clone(), &mut i, &mut f) == 0 {
                    continue;
                }
                z = f[0];
            }
            set_position(&mut t, x, y, z);
            let container = board(h, player, SEP_NONE, g.unit_type);
            op(h, EE_SCREENS_SEP_LOAD_MEDIA, &[player, g.unit_type]);
            self.units.push(SepPlaced { container, t, player, unit_type: g.unit_type, animation: g.animation });
            placed = true;
            if g.flags & EE_SCREENS_SEP_F_AIRPORT != 0 {
                break;
            }
        }
        if placed {
            h.sep(EE_SCREENS_SEP_POST, owner, &mut GeTransformation::IDENTITY.clone(), &mut [0; 8], &mut [0.0; 4]);
        }
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[0]);
        placed
    }

    /// `Virtual4`: each queued unit made, the last queued first.
    pub fn run<H: SepHost + ?Sized>(&mut self, h: &mut H) {
        let mut pitch = 0.0f32;
        while let Some(mut e) = self.units.pop() {
            if op(h, EE_SCREENS_SEP_PITCHES, &[e.unit_type]) != 0 {
                pitch = op_t(h, EE_SCREENS_SEP_TERRAIN_PITCH, &mut e.t, &[], 0.0).1;
            }
            let yaw = e.t.orientation().yaw;
            e.t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
            op_t(h, EE_SCREENS_SEP_CREATE, &mut e.t, &[e.player, e.unit_type, e.animation, e.container], 0.0);
        }
    }
}

/// One picked-up part (`EESEAMoveUnitEntry`).
#[derive(Clone, Debug, PartialEq)]
pub struct SepMoved {
    pub t: GeTransformation,
    pub unit: i32,
    pub container: i32,
    pub tile: (i32, i32),
    pub yaw: f32,
}

/// `EESEAMoveUnit`'s parts and state.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SepMove {
    pub entries: Vec<SepMoved>,
    pub moving: bool,
}

impl SepMove {
    /// `PickUp(id)`: every part of the unit held and the move (`owner`) posted.
    pub fn pick_up<H: SepHost + ?Sized>(&mut self, h: &mut H, owner: usize, id: u32) {
        if id == u32::MAX {
            return;
        }
        let mut parts = vec![0i32; 64];
        let mut i = [id as i32, parts.len() as i32, 0, 0, 0, 0, 0, 0];
        let n = h.sep(
            EE_SCREENS_SEP_PARTS,
            parts.as_mut_ptr() as usize,
            &mut GeTransformation::IDENTITY.clone(),
            &mut i,
            &mut [0.0; 4],
        );
        if n <= 0 {
            return;
        }
        for &unit in &parts[..(n as usize).min(parts.len())] {
            self.entries.push(SepMoved {
                t: GeTransformation::IDENTITY,
                unit,
                container: SEP_NONE,
                tile: (0, 0),
                yaw: 0.0,
            });
        }
        self.moving = true;
        h.sep(EE_SCREENS_SEP_POST, owner, &mut GeTransformation::IDENTITY.clone(), &mut [0; 8], &mut [0.0; 4]);
    }

    /// `PutDown`: when the first ghost can be placed, each part's place from its ghost (and the transport under the
    /// mouse it may board), then the drop posted.
    pub fn put_down<H: SepHost + ?Sized>(&mut self, h: &mut H, owner: usize) {
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[1]);
        if ghost(h, 0).can_place {
            self.moving = false;
            for (k, e) in self.entries.iter_mut().enumerate() {
                let g = ghost(h, k as i32);
                e.container = SEP_NONE;
                e.t = g.t;
                if g.flags & EE_SCREENS_SEP_F_FLIES != 0 {
                    e.t.m[2][3] = g.cruise;
                }
                if g.flags & EE_SCREENS_SEP_F_SMALL != 0 {
                    e.tile = g.tile;
                } else {
                    e.tile = (snapped(g.tile.0, g.tile_size) as i32, snapped(g.tile.1, g.tile_size) as i32);
                }
                e.yaw = g.t.orientation().yaw;
                e.container = board(h, -1, e.unit, g.unit_type);
            }
            h.sep(EE_SCREENS_SEP_POST, owner, &mut GeTransformation::IDENTITY.clone(), &mut [0; 8], &mut [0.0; 4]);
        }
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[0]);
    }

    /// `Cancel`: the picked-up unit deleted, every part let go.
    pub fn cancel<H: SepHost + ?Sized>(&mut self, h: &mut H) {
        if !self.moving || self.entries.is_empty() {
            return;
        }
        let first = self.entries.remove(0);
        op(h, EE_SCREENS_SEP_DELETE, &[first.unit]);
        op(h, EE_SCREENS_SEP_RELEASE, &[first.unit]);
        while let Some(e) = self.entries.pop() {
            if e.container != SEP_NONE {
                op(h, EE_SCREENS_SEP_RELEASE, &[e.container]);
            }
            op(h, EE_SCREENS_SEP_RELEASE, &[e.unit]);
        }
        self.moving = false;
    }

    /// `Virtual4`: lifting the parts and showing the first one's ghost, or putting each down.
    pub fn run<H: SepHost + ?Sized>(&mut self, h: &mut H) {
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[1]);
        if self.moving {
            for e in &self.entries {
                op(h, EE_SCREENS_SEP_LIFT, &[e.unit]);
            }
            if let Some(first) = self.entries.first() {
                let unit = first.unit;
                op(h, EE_SCREENS_SEP_CLEAR_GHOSTS, &[]);
                let mut t = GeTransformation::IDENTITY;
                let (flags, _) = op_t(h, EE_SCREENS_SEP_UNIT, &mut t, &[unit], 0.0);
                let yaw = if flags & EE_SCREENS_SEP_U_COMPLEX != 0 { t.orientation().yaw } else { 0.0 };
                op_t(h, EE_SCREENS_SEP_GHOST_FOR, &mut t, &[unit], yaw);
            }
        } else {
            // the height the levelling passes on (the original's one variable for every part)
            let mut z = 0.0f32;
            while let Some(mut e) = self.entries.pop() {
                let mut location = GeTransformation::IDENTITY;
                let (flags, _) = op_t(h, EE_SCREENS_SEP_UNIT, &mut location, &[e.unit], 0.0);
                if flags & EE_SCREENS_SEP_U_COMPLEX != 0 {
                    if e.container != SEP_NONE {
                        op(h, EE_SCREENS_SEP_LOAD_INTO, &[e.unit, e.container]);
                    } else {
                        let yaw = e.t.orientation().yaw;
                        let pitch = if flags & EE_SCREENS_SEP_U_FLIES == 0 && flags & EE_SCREENS_SEP_U_PITCHES != 0 {
                            op_t(h, EE_SCREENS_SEP_TERRAIN_PITCH, &mut e.t, &[], 0.0).1
                        } else {
                            0.0
                        };
                        e.t.set_orientation(RotationOrder::Ypr, yaw, pitch, 0.0);
                        op(h, EE_SCREENS_SEP_MOVE_TO, &[e.unit, e.tile.0, e.tile.1]);
                        op_t(h, EE_SCREENS_SEP_SET_LOCATION, &mut e.t, &[e.unit], 0.0);
                        if flags & EE_SCREENS_SEP_U_AIRPORT != 0 {
                            z = op_t(h, EE_SCREENS_SEP_AIRPORT, &mut e.t, &[e.unit], z).1;
                        }
                        if flags & EE_SCREENS_SEP_U_BUILDING != 0 {
                            z = e.t.m[2][3];
                            z = op_t(h, EE_SCREENS_SEP_LEVEL, &mut e.t, &[e.unit, e.tile.0, e.tile.1], z).1;
                            e.t.m[2][3] = z;
                            op_t(h, EE_SCREENS_SEP_SET_LOCATION, &mut e.t, &[e.unit], 0.0);
                        }
                    }
                } else {
                    // (long)x, (long)y: truncated
                    op(h, EE_SCREENS_SEP_MOVE_TO, &[e.unit, e.t.m[0][3] as i32, e.t.m[1][3] as i32]);
                }
                op(h, EE_SCREENS_SEP_RELEASE, &[e.unit]);
            }
            op(h, EE_SCREENS_SEP_CLEAR_GHOSTS, &[]);
        }
        op(h, EE_SCREENS_SEP_GHOST_LOCK, &[0]);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// can place, type, tile, tile size, flags, animation, position
    type GhostRow = (bool, i32, (i32, i32), i32, i32, i32, [f32; 3]);

    struct Rec {
        log: Vec<String>,
        ghosts: Vec<GhostRow>,
        under: i32,
        units: Vec<(i32, i32)>,
    }

    impl SepHost for Rec {
        fn sep(&mut self, op: u32, p: usize, t: &mut GeTransformation, i: &mut [i32; 8], f: &mut [f32; 4]) -> i32 {
            match op {
                EE_SCREENS_SEP_GHOST_COUNT => return self.ghosts.len() as i32,
                EE_SCREENS_SEP_GHOST => {
                    let Some(&(ok, ty, tile, size, flags, anim, pos)) = self.ghosts.get(i[0] as usize) else {
                        return 0;
                    };
                    i[1] = ty;
                    i[2] = tile.0;
                    i[3] = tile.1;
                    i[4] = size;
                    i[5] = anim;
                    i[6] = flags;
                    f[0] = 30.0;
                    *t = GeTransformation::IDENTITY;
                    t.set_orientation(RotationOrder::Ypr, 0.5, 0.0, 0.0);
                    set_position(t, pos[0], pos[1], pos[2]);
                    return i32::from(ok);
                }
                EE_SCREENS_SEP_TILE_HEIGHT => {
                    f[0] = 2.5;
                    return 1;
                }
                EE_SCREENS_SEP_UNDER_MOUSE => return self.under,
                EE_SCREENS_SEP_BOARD => {
                    i[4] = i[0] + 1000;
                    self.log.push(format!("board {} {} {} {}", i[0], i[1], i[2], i[3]));
                    return 1;
                }
                EE_SCREENS_SEP_PARTS => {
                    let out = unsafe { core::slice::from_raw_parts_mut(p as *mut i32, i[1] as usize) };
                    let n = self.units.iter().filter(|u| u.0 == i[0]).count();
                    for (k, u) in self.units.iter().filter(|u| u.0 == i[0]).enumerate() {
                        out[k] = u.1;
                    }
                    return n as i32;
                }
                EE_SCREENS_SEP_PITCHES => return 1,
                EE_SCREENS_SEP_TERRAIN_PITCH => {
                    f[0] = 0.25;
                    return 0;
                }
                EE_SCREENS_SEP_UNIT => {
                    return EE_SCREENS_SEP_U_EXISTS
                        | EE_SCREENS_SEP_U_COMPLEX
                        | EE_SCREENS_SEP_U_PITCHES
                        | if i[0] == 7 { EE_SCREENS_SEP_U_BUILDING } else { 0 }
                }
                EE_SCREENS_SEP_LEVEL => {
                    f[0] += 1.0;
                }
                _ => {}
            }
            self.log.push(format!("{op} {:?} {} {} {}", &i[..4], t.m[0][3], t.m[1][3], t.m[2][3]));
            0
        }
    }

    fn rec() -> Rec {
        Rec { log: Vec::new(), ghosts: Vec::new(), under: -1, units: Vec::new() }
    }

    #[test]
    fn place_snaps_buildings_and_boards_transports() {
        let mut h = rec();
        // a house (size 2) at tile (10, 12); a citizen (small) at (4.5, 5.5, 1)
        h.ghosts = vec![
            (true, 37, (10, 12), 2, 0, 3, [0.0; 3]),
            (true, 32, (4, 5), 1, EE_SCREENS_SEP_F_CLASS | EE_SCREENS_SEP_F_SMALL, 7, [4.5, 5.5, 1.0]),
        ];
        h.ghosts[0].4 = EE_SCREENS_SEP_F_CLASS;
        h.under = 42;
        let mut p = SepPlace::default();
        assert!(p.place(&mut h, 0x77, 1));
        assert_eq!(p.units.len(), 2);
        assert_eq!((p.units[0].t.m[0][3], p.units[0].t.m[1][3], p.units[0].t.m[2][3]), (9.5, 11.5, 2.5));
        assert_eq!((p.units[1].t.m[0][3], p.units[1].t.m[1][3], p.units[1].t.m[2][3]), (4.5, 5.5, 1.0));
        assert_eq!(p.units[1].container, 1042);
        assert_eq!(p.units[0].animation, 3);
        assert!(h.log.contains(&"board 42 1 32 -1".to_string()));
        h.log.clear();
        p.run(&mut h);
        // the citizen first (the last queued), pitched
        assert!(h.log[0].starts_with("11 [1, 32, 7, 1042]"), "{:?}", h.log);
        assert!(p.units.is_empty());
    }

    #[test]
    fn a_blocked_granary_among_ghosts_cancels_and_an_airport_ends_the_list() {
        let mut h = rec();
        h.ghosts = vec![
            (true, 32, (4, 5), 1, EE_SCREENS_SEP_F_CLASS | EE_SCREENS_SEP_F_SMALL, 7, [1.0; 3]),
            (false, 5, (1, 1), 3, EE_SCREENS_SEP_F_GRANARY, 7, [0.0; 3]),
        ];
        let mut p = SepPlace::default();
        assert!(!p.place(&mut h, 1, 1));
        assert!(p.units.is_empty());
        assert!(!h.log.iter().any(|l| l.starts_with("8 ")));
        h.ghosts = vec![
            (true, 66, (20, 20), 4, EE_SCREENS_SEP_F_CLASS | EE_SCREENS_SEP_F_AIRPORT, 7, [0.0; 3]),
            (true, 67, (24, 20), 2, EE_SCREENS_SEP_F_CLASS, 7, [0.0; 3]),
        ];
        assert!(p.place(&mut h, 1, 1));
        assert_eq!(p.units.len(), 1);
        // a large ghost at the low edge is skipped
        let mut q = SepPlace::default();
        h.ghosts = vec![(true, 37, (0, 5), 2, EE_SCREENS_SEP_F_CLASS, 7, [0.0; 3])];
        assert!(!q.place(&mut h, 1, 1));
    }

    #[test]
    fn move_picks_up_puts_down_and_cancels() {
        let mut h = rec();
        h.units = vec![(5, 5), (5, 6)];
        let mut m = SepMove::default();
        m.pick_up(&mut h, 9, 5);
        assert!(m.moving);
        assert_eq!(m.entries.iter().map(|e| e.unit).collect::<Vec<_>>(), [5, 6]);
        h.log.clear();
        m.run(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|l| l.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["1", "14", "14", "15", "17", "1"]);
        // put down on placeable ghosts (a flier's altitude kept from its class)
        h.ghosts = vec![
            (
                true,
                32,
                (8, 9),
                1,
                EE_SCREENS_SEP_F_CLASS | EE_SCREENS_SEP_F_SMALL | EE_SCREENS_SEP_F_FLIES,
                7,
                [8.5, 9.5, 0.0],
            ),
            (true, 32, (8, 9), 2, EE_SCREENS_SEP_F_CLASS, 7, [0.0; 3]),
        ];
        m.put_down(&mut h, 9);
        assert!(!m.moving);
        assert_eq!(m.entries[0].tile, (8, 9));
        assert_eq!(m.entries[0].t.m[2][3], 30.0);
        assert_eq!(m.entries[1].tile, (7, 8));
        assert_eq!(m.entries[0].yaw, m.entries[0].t.orientation().yaw);
        h.log.clear();
        m.run(&mut h);
        assert!(m.entries.is_empty());
        assert!(h.log.iter().any(|l| l.starts_with("19 [6, 7, 8, 0]")), "{:?}", h.log);
        // cancel deletes the first part and lets the rest go
        let mut c = SepMove::default();
        c.pick_up(&mut h, 9, 5);
        h.log.clear();
        c.cancel(&mut h);
        let ops: Vec<&str> = h.log.iter().map(|l| l.split(' ').next().unwrap()).collect();
        assert_eq!(ops, ["23", "12", "12"]);
        assert!(!c.moving);
    }

    #[test]
    fn a_building_put_down_is_levelled() {
        let mut h = rec();
        let mut m = SepMove::default();
        m.entries.push(SepMoved {
            t: GeTransformation::IDENTITY,
            unit: 7,
            container: SEP_NONE,
            tile: (3, 4),
            yaw: 0.0,
        });
        m.run(&mut h);
        assert!(h.log.iter().any(|l| l.starts_with("22 [7, 3, 4, 0]")));
        // the level raised z by one: the second location has it
        let sets: Vec<&String> = h.log.iter().filter(|l| l.starts_with("20 ")).collect();
        assert_eq!(sets.len(), 2);
        assert!(sets[1].ends_with(" 1"), "{sets:?}");
    }

    #[test]
    fn snapping_is_the_originals_unsigned_arithmetic() {
        assert_eq!(snapped(10, 2), 9.5);
        assert_eq!(snapped(10, 3), 9.5);
        assert_eq!(snapped(10, 4), 8.5);
    }
}
