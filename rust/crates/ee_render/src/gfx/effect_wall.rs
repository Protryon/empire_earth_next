//! The energy wall (kind 0x34) whole in Rust (`EEE3DEnergyWall`, deleted; `wall` below): the posts' segment ends,
//! the segments found around the posts every 500 ms, the queue's alpha, colour and texture frame, the ground strips
//! of pass 6, Load (and the files before 0.250) and Save. The C++ host answers the posts' places on the unit (a
//! gate's two end parts), the friendly wall pieces on the tiles, the gate's opening and the owner's colour;
//! `EEE3DRust::IsTileVisible` marks the wall seen through a doppelganger.
//!
//! Simulation state (RUST_PLAN 2.1): the C++'s f32 operations in order.

use ee_base::math::types::Ge3DPoint;
use ee_base::math::um;

use crate::gfx::effect_rising::{EeGfxEffect3DOp, EeGfxGraphicFacts, Effect3DHost};
use crate::gfx::effect_state::{load_point, random_long, save_point, EeGfxEffectRecord, EffectFile};
use crate::gfx::effects::EeGfxColor;
use crate::gfx::raster::Raster;

/// `kEffect3DEnergyWall`
pub const EE_GFX_EFFECT_KIND_ENERGY_WALL: i32 = 0x34;

/// `kErrorOK`.
const OK: i32 = 0;

/// The segment ends around a post (`sEnergyWallOffsetX/Y`, `sEnergyWallTileX/Y`: 1..4 around the first post, 5..8
/// around a gate's second).
const OFFSET_X: [i32; 9] = [0, -1, -1, 0, 1, -1, -1, 0, 1];
const OFFSET_Y: [i32; 9] = [0, 0, -1, -1, -1, 0, -1, -1, -1];
/// ms between two searches for the segments.
const SEGMENT_TIME: i32 = 500;

/// `EEE3DEnergyWall`'s state.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectWall {
    /// `mPoints`: [0] and [9] the posts, [1..4] and [5..8] the segment ends (saved)
    pub points: [Ge3DPoint; 10],
    /// `mSegments`: bit i, segment i is drawn (saved)
    pub segments: u32,
    /// `mTimer`: ms to the next search (saved)
    pub timer: i32,
    /// `mPlaced` (saved)
    pub placed: bool,
    /// `mGate`: two posts (saved)
    pub gate: bool,
    /// `mSeen`: seen only through a doppelganger since the last queue (the texture frame restarts)
    pub seen: bool,
    /// `mAlpha`, `mGateAlpha`: the strips' and the gate's (the queue's)
    pub alpha: f32,
    pub gate_alpha: f32,
    /// `mTextureOffset`: the animation frame's (the queue's)
    pub offset_u: f32,
    pub offset_v: f32,
    /// `mColor`: the owner's (the queue's; `GEColor()`'s white before)
    pub color: EeGfxColor,
}

impl Default for EeGfxEffectWall {
    fn default() -> EeGfxEffectWall {
        EeGfxEffectWall {
            points: [Ge3DPoint::default(); 10],
            segments: 0,
            timer: 0,
            placed: false,
            gate: false,
            seen: false,
            alpha: 0.0,
            gate_alpha: 0.0,
            offset_u: 0.0,
            offset_v: 0.0,
            color: EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 },
        }
    }
}

/// The friendly wall pieces a segment reaches.
pub trait WallHost {
    /// the height of the last living, finished wall piece or gate side part of the wall's owner on the tile
    fn wall_piece(&mut self, x: i32, y: i32) -> Option<f32>;
}

/// `Place`'s part after the C++ set the posts (and `gate`): the segment ends around them, no segment, the timer
/// due, placed.
pub fn wall_place(r: &mut EeGfxEffectRecord) {
    let w = &mut r.wall;
    for i in 1..5 {
        w.points[i].x = w.points[0].x + OFFSET_X[i] as f32;
        w.points[i].y = w.points[0].y + OFFSET_Y[i] as f32;
    }
    if w.gate {
        for i in 5..9 {
            w.points[i].x = w.points[9].x + OFFSET_X[i] as f32;
            w.points[i].y = w.points[9].y + OFFSET_Y[i] as f32;
        }
    }
    w.timer = 0;
    w.segments = 0;
    w.placed = true;
}

/// `FindSegments`: the segments towards the friendly wall pieces around each post, their ends raised to the piece
/// (`height`: the graphic effect's `GetParam6()`).
pub fn wall_find_segments<H: WallHost>(r: &mut EeGfxEffectRecord, height: f32, host: &mut H) {
    let w = &mut r.wall;
    w.segments = 0;
    let posts: &[(usize, usize, usize)] = if w.gate { &[(0, 1, 5), (9, 5, 9)] } else { &[(0, 1, 5)] };
    for &(post, from, to) in posts {
        let x = um::floor(w.points[post].x);
        let y = um::floor(w.points[post].y);
        for i in from..to {
            if let Some(z) = host.wall_piece(x.wrapping_add(OFFSET_X[i]), y.wrapping_add(OFFSET_Y[i])) {
                w.segments |= 1 << i;
                w.points[i].z = height + z;
            }
        }
    }
}

/// `Step`'s timer (after the C++ placed an unplaced wall): whether the segments are due again (every 500 ms).
pub fn wall_step(r: &mut EeGfxEffectRecord, tick_time: f32) -> bool {
    let w = &mut r.wall;
    w.timer = w.timer.wrapping_sub(tick_time as i32);
    if w.timer > 0 {
        return false;
    }
    w.timer = SEGMENT_TIME;
    true
}

/// `Load`'s own reads (0.250 on) after `EEEffect3D::Load`'s and the graphic's media load (`media`: its result, the
/// reads only after it succeeded): the gate flag, the ten points, the segments, the timer, the placed flag; the
/// first error.
pub fn load_wall<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, media: i32) -> i32 {
    let w = &mut r.wall;
    let mut res = media;
    if res == OK {
        res = f.read_bool(&mut w.gate);
    }
    if res == OK {
        for p in &mut w.points {
            res = load_point(f, p);
            if res != OK {
                break;
            }
        }
    }
    if res == OK {
        res = f.read_u32(&mut w.segments);
    }
    if res == OK {
        res = f.read_i32(&mut w.timer);
    }
    if res == OK {
        res = f.read_bool(&mut w.placed);
    }
    res
}

/// `LoadOldVersion`'s reads (files before 0.250) after the media load: two points, a float, the placed flag (all
/// dropped), the gate flag, 18 points (dropped), the segments, the timer; the wall is placed again.
pub fn load_wall_old<F: EffectFile>(r: &mut EeGfxEffectRecord, f: &mut F, media: i32) -> i32 {
    let w = &mut r.wall;
    let mut res = media;
    let mut point = Ge3DPoint::default();
    if res == OK {
        res = load_point(f, &mut point);
        if res == OK {
            res = load_point(f, &mut point);
        }
    }
    if res == OK {
        let mut value = 0.0f32;
        res = f.read_f32(&mut value);
    }
    if res == OK {
        res = f.read_bool(&mut w.placed);
    }
    if res == OK {
        res = f.read_bool(&mut w.gate);
        if res == OK {
            for _ in 0..18 {
                res = load_point(f, &mut point);
                if res != OK {
                    break;
                }
            }
            if res == OK {
                res = f.read_u32(&mut w.segments);
            }
            if res == OK {
                res = f.read_i32(&mut w.timer);
            }
        }
    } else {
        w.gate = false;
    }
    w.placed = false;
    res
}

/// `Save`'s own writes after `EEEffect3D::Save`'s.
pub fn save_wall<F: EffectFile>(r: &EeGfxEffectRecord, f: &mut F) -> i32 {
    let w = &r.wall;
    let mut res = f.write_bool(w.gate);
    if res != OK {
        return res;
    }
    for p in &w.points {
        res = save_point(f, p);
        if res != OK {
            return res;
        }
    }
    res = f.write_u32(w.segments);
    if res != OK {
        return res;
    }
    res = f.write_i32(w.timer);
    if res != OK {
        return res;
    }
    f.write_bool(w.placed)
}

/// The segment search over the effect host's wall pieces.
struct Pieces<'a, H: Effect3DHost>(&'a mut H);

impl<H: Effect3DHost> WallHost for Pieces<'_, H> {
    fn wall_piece(&mut self, x: i32, y: i32) -> Option<f32> {
        self.0.wall_piece(x, y)
    }
}

/// `Place`: the posts (a gate's two end parts, else the unit) raised by param 6, the segment ends, the effect at the
/// unit's place with its location's orientation, its bounding radius.
fn place<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    let position = h.complex_position();
    let posts = h.wall_gate_posts();
    // SAFETY (every access below): the virtuals' contract; no reference to the record lives across a host call
    unsafe {
        let w = &mut (*r).wall;
        w.gate = false;
        if let Some((first, second)) = posts {
            w.points[0] = first;
            w.points[9] = second;
            w.points[0].z = g.param6 + first.z;
            w.points[9].z = g.param6 + second.z;
            w.gate = true;
        } else {
            w.points[0].x = position.x;
            w.points[0].y = position.y;
            w.points[0].z = g.param6 + position.z;
        }
        wall_place(&mut *r);
    }
    let mut t = h.unit_location();
    let at = h.complex_position();
    t.m[0][3] = at.x;
    t.m[1][3] = at.y;
    t.m[2][3] = at.z;
    unsafe { (*r).transformation = t };
    h.move_to();
    unsafe { (*r).bounding_radius = 2.828 };
}

/// `FindSegments` (the posts' friendly wall pieces through the host).
fn find_segments<H: Effect3DHost>(r: *mut EeGfxEffectRecord, g: &EeGfxGraphicFacts, h: &mut H) {
    // SAFETY: the virtuals' contract; the host's wall pieces do not read the record
    unsafe { wall_find_segments(&mut *r, g.param6, &mut Pieces(h)) }
}

/// The energy wall's virtuals (kind 0x34, `EEE3DEnergyWall`, deleted):
/// - made on a unit, it is placed and finds its segments; a load-time wall unplaced by an old file is placed again
///   at its next step, and the segments are found again every 500 ms;
/// - queued (4) once placed: its segments again under a free camera, a gate's alpha by how open it is, the alpha
///   (0 for a cloaked enemy), the owner's colour and the texture frame (a random one under a free camera, 0 once
///   after the wall was seen only through a doppelganger, else the game time);
/// - pass 6: a ground strip from each post to each segment end found, and across a gate.
///
/// # Safety
/// As `effect_rising::effect3d`.
pub unsafe fn wall<H: Effect3DHost, R: Raster>(
    op: EeGfxEffect3DOp,
    r: *mut EeGfxEffectRecord,
    g: &EeGfxGraphicFacts,
    h: &mut H,
    x: &mut R,
) -> i32 {
    match op {
        EeGfxEffect3DOp::Create => {
            place(r, g, h);
            find_segments(r, g, h);
            0
        }
        EeGfxEffect3DOp::SetUp => h.load_media(),
        EeGfxEffect3DOp::Queue => {
            // SAFETY (every access below): the function's contract; no reference lives across a host call
            if !unsafe { (*r).wall.placed } {
                return 0;
            }
            let res = h.base_queue();
            if res != 0 {
                return res;
            }
            if g.free_camera {
                find_segments(r, g, h);
            }
            if let Some(open) = h.gate_open() {
                unsafe { (*r).wall.gate_alpha = (1.0 - open) * (g.param3 - g.param2) + g.param2 };
            }
            unsafe { (*r).wall.alpha = g.param3 };
            if h.unit_hidden() {
                unsafe {
                    (*r).wall.alpha = 0.0;
                    (*r).wall.gate_alpha = 0.0;
                }
            }
            let color = h.player_color();
            let w = unsafe { &mut (*r).wall };
            w.color = color;
            let frame = if g.free_camera {
                random_long(0, 0x20)
            } else if w.seen {
                w.seen = false;
                0
            } else {
                g.game_time_bits as i32
            };
            w.offset_u = (frame & 7) as f32 * 0.14;
            w.offset_v = (frame >> 2 & 7) as f32 * 0.14;
            h.queue(4);
            0
        }
        EeGfxEffect3DOp::DrawPass6 => {
            let w = unsafe { (*r).wall };
            let size = (g.param4, g.param5);
            let offset = (w.offset_u, w.offset_v);
            for i in 1..5 {
                if w.segments & (1 << i) != 0 {
                    x.ground_strip(
                        &w.points[0],
                        &w.points[i],
                        g.param1,
                        w.color,
                        offset,
                        size,
                        w.alpha,
                        1.0,
                        g.alpha_ref,
                    );
                }
            }
            if !w.gate {
                return 0;
            }
            x.ground_strip(&w.points[0], &w.points[9], g.param1, w.color, offset, size, w.gate_alpha, 1.0, g.alpha_ref);
            for i in 5..9 {
                if w.segments & (1 << i) != 0 {
                    x.ground_strip(
                        &w.points[9],
                        &w.points[i],
                        g.param1,
                        w.color,
                        offset,
                        size,
                        w.alpha,
                        1.0,
                        g.alpha_ref,
                    );
                }
            }
            0
        }
        EeGfxEffect3DOp::Step => {
            if !unsafe { (*r).wall.placed } {
                place(r, g, h);
            }
            if wall_step(unsafe { &mut *r }, g.tick_time) {
                find_segments(r, g, h);
            }
            0
        }
        _ => 0,
    }
}

/// The energy wall's `Load` (after the base's, whose result is ignored: the media's result, then the wall's reads,
/// the files before 0.250 through `load_wall_old`) and `Save` (the base's error stops it).
#[allow(clippy::too_many_arguments)]
pub fn wall_file<H: Effect3DHost, F: EffectFile>(
    load: bool,
    base: i32,
    r: &mut EeGfxEffectRecord,
    f: &mut F,
    major: u32,
    minor: u32,
    h: &mut H,
) -> i32 {
    if !load {
        return if base != OK { base } else { save_wall(r, f) };
    }
    let media = h.load_media();
    if major == 0 && minor < 0xfa {
        load_wall_old(r, f, media)
    } else {
        load_wall(r, f, media)
    }
}

#[cfg(test)]
#[path = "effect_wall_tests.rs"]
mod effect_tests;

#[cfg(test)]
mod tests {
    use super::*;

    /// Wall pieces at height 2 on the tiles left of and above-left of (10, 10) and at height 5 left of (20, 10).
    struct Map(Vec<(i32, i32)>);

    impl WallHost for Map {
        fn wall_piece(&mut self, x: i32, y: i32) -> Option<f32> {
            self.0.push((x, y));
            match (x, y) {
                (9, 10) | (9, 9) => Some(2.0),
                (19, 10) => Some(5.0),
                _ => None,
            }
        }
    }

    fn wall(gate: bool) -> EeGfxEffectRecord {
        let mut r = EeGfxEffectRecord::new(0x37);
        r.wall.points[0] = Ge3DPoint { x: 10.5, y: 10.5, z: 1.0 };
        r.wall.points[9] = Ge3DPoint { x: 20.5, y: 10.5, z: 1.0 };
        r.wall.gate = gate;
        r.wall.segments = 0xff;
        r.wall.timer = 77;
        wall_place(&mut r);
        r
    }

    #[test]
    fn place_sets_the_ends_around_the_posts() {
        let r = wall(false);
        assert_eq!((r.wall.timer, r.wall.segments, r.wall.placed), (0, 0, true));
        assert_eq!((r.wall.points[1].x, r.wall.points[1].y), (9.5, 10.5));
        assert_eq!((r.wall.points[4].x, r.wall.points[4].y), (11.5, 9.5));
        assert_eq!(r.wall.points[5], Ge3DPoint::default());
        let g = wall(true);
        assert_eq!((g.wall.points[6].x, g.wall.points[6].y), (19.5, 9.5));
        assert_eq!((g.wall.points[8].x, g.wall.points[8].y), (21.5, 9.5));
    }

    #[test]
    fn segments_reach_the_owners_pieces() {
        let mut r = wall(false);
        let mut m = Map(Vec::new());
        wall_find_segments(&mut r, 0.5, &mut m);
        assert_eq!(m.0, vec![(9, 10), (9, 9), (10, 9), (11, 9)]);
        assert_eq!(r.wall.segments, 0b110);
        assert_eq!((r.wall.points[1].z, r.wall.points[2].z), (2.5, 2.5));
        let mut g = wall(true);
        let mut m = Map(Vec::new());
        wall_find_segments(&mut g, 0.5, &mut m);
        assert_eq!(m.0.len(), 8);
        assert_eq!(g.wall.segments, 0b10_0110);
        assert_eq!(g.wall.points[5].z, 5.5);
    }

    #[test]
    fn the_timer_runs_every_500_ms() {
        let mut r = wall(false);
        assert!(wall_step(&mut r, 100.9));
        assert_eq!(r.wall.timer, 500);
        for _ in 0..4 {
            assert!(!wall_step(&mut r, 100.9));
        }
        assert!(wall_step(&mut r, 100.9));
    }

    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
    }

    impl Mem {
        fn read(&mut self, ty: char) -> Result<u32, i32> {
            let Some(&(t, v)) = self.items.get(self.at) else { return Err(3) };
            assert_eq!(t, ty, "item {}", self.at);
            self.at += 1;
            Ok(v)
        }
    }

    impl EffectFile for Mem {
        fn is_open(&mut self) -> bool {
            true
        }
        fn read_u32(&mut self, v: &mut u32) -> i32 {
            self.read('u').map(|x| *v = x).err().unwrap_or(0)
        }
        fn read_i32(&mut self, v: &mut i32) -> i32 {
            self.read('i').map(|x| *v = x as i32).err().unwrap_or(0)
        }
        fn read_f32(&mut self, v: &mut f32) -> i32 {
            self.read('f').map(|x| *v = f32::from_bits(x)).err().unwrap_or(0)
        }
        fn read_bool(&mut self, v: &mut bool) -> i32 {
            self.read('b').map(|x| *v = x != 0).err().unwrap_or(0)
        }
        fn write_u32(&mut self, v: u32) -> i32 {
            self.items.push(('u', v));
            0
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.items.push(('i', v as u32));
            0
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.items.push(('f', v.to_bits()));
            0
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.items.push(('b', v as u32));
            0
        }
    }

    #[test]
    fn save_and_load_round_trip() {
        let mut r = wall(true);
        r.wall.segments = 0b1010;
        r.wall.timer = 123;
        let mut m = Mem::default();
        assert_eq!(save_wall(&r, &mut m), 0);
        assert_eq!(m.items.len(), 1 + 30 + 3);
        let mut back = EeGfxEffectRecord::new(0x37);
        assert_eq!(load_wall(&mut back, &mut m, 0), 0);
        assert_eq!(back.wall, r.wall);
        // a failed media load: nothing read
        let mut none = EeGfxEffectRecord::new(0x37);
        let mut m2 = Mem::default();
        assert_eq!(load_wall(&mut none, &mut m2, 6), 6);
        assert_eq!(m2.at, 0);
    }

    #[test]
    fn old_files_drop_their_points_and_place_again() {
        let mut m = Mem::default();
        for _ in 0..6 {
            m.items.push(('f', 1.0f32.to_bits()));
        }
        m.items.push(('f', 0));
        m.items.push(('b', 1));
        m.items.push(('b', 1));
        for _ in 0..54 {
            m.items.push(('f', 2.0f32.to_bits()));
        }
        m.items.push(('u', 0b11));
        m.items.push(('i', 250));
        let mut r = EeGfxEffectRecord::new(0x37);
        assert_eq!(load_wall_old(&mut r, &mut m, 0), 0);
        assert_eq!(m.at, m.items.len());
        assert_eq!((r.wall.gate, r.wall.placed, r.wall.segments, r.wall.timer), (true, false, 0b11, 250));
        assert_eq!(r.wall.points, [Ge3DPoint::default(); 10]);
        // a short file before the gate flag: no gate
        let mut short = Mem { items: vec![('f', 0)], at: 0 };
        let mut s = EeGfxEffectRecord::new(0x37);
        s.wall.gate = true;
        assert_eq!(load_wall_old(&mut s, &mut short, 0), 3);
        assert!(!s.wall.gate);
    }
}
