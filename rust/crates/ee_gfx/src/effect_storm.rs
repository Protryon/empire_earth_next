//! The weather storms on the effects' records (`EEE3DStorm`, `EEE3DRain`, `EEE3DSnow`,
//! `src/Empire Earth/Graphics Thread/EEE3DStorm.cpp`, `EEE3DRain.cpp`, `EEE3DSnow.cpp`): the storm's tiles, centre,
//! move and bounds, the set-up's centre and radius, the drops and flakes (spawned over random tiles, falling, the
//! flakes spinning and drifting), the rain's thunder timer, the respawn after a load, Load and Save. The tiles and the
//! drops are a Rust value the record owns (`EeGfxEffectStorm::lists`, made on first use, freed with the record).
//! The C++ keeps the sounds, the shared drop and flake models, the draws and every call into the world.
//!
//! Simulation state (RUST_PLAN 2.1): the C++'s f32 operations in order, the effects' random draws in the original's
//! order and count, `lrintf` for `UMFloatToLong`.

use ee_ge::types::Ge3DPoint;
use ee_ge::um;

use crate::effect_state::{load_point, random, random_float, random_long, save_point, EeGfxEffectRecord, EffectFile};

/// `kErrorOK`.
const OK: i32 = 0;

/// The rain (`EEE3DRain`).
pub const EE_GFX_EFFECT_KIND_RAIN: i32 = 0x2a;
/// The snow (`EEE3DSnow`).
pub const EE_GFX_EFFECT_KIND_SNOW: i32 = 0x3d;

/// The rain's ms to full strength, its spawn density and its thunder delay.
const RAIN_FADE_TIME: f32 = 6500.0;
const RAIN_DENSITY: f32 = 0.4;
const RAIN_RUMBLE_DELAY: f32 = 9000.0;
/// The snow's ms to full strength, its spawn density, a flake's chance to turn, its drift and its spin per tick.
const SNOW_FADE_TIME: f32 = 500.0;
const SNOW_DENSITY: f32 = 0.2;
const SNOW_TURN_CHANCE: f32 = 0.05;
const SNOW_DRIFT: f32 = 0.005;
const SNOW_SPIN: f32 = 0.043633234;
/// A drop's random turn (2 pi as the original's float).
const TWO_PI: f32 = 6.2831855;
/// At most this many new drops a tick, none while more than `MAX_DROPS` fall.
const MAX_NEW_DROPS: u32 = 0x41;
const MAX_DROPS: usize = 0x578;

/// A storm's state (`EEE3DStorm`'s, `EEE3DRain`'s and `EEE3DSnow`'s plain members).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeGfxEffectStorm {
    /// the tiles and the drops (owned: made on first use, freed with the record)
    pub lists: *mut EeGfxStormLists,
    /// `mCenter`: the tiles' mean (saved)
    pub center_x: i32,
    pub center_y: i32,
    /// `mFallSpeed`, `mSpawnHeight`, `mDensity`: the graphic effect's params 1-3
    pub fall_speed: f32,
    pub spawn_height: f32,
    pub density: f32,
    /// `mIntensity` (1; saved; the weather event sets it)
    pub intensity: f32,
    /// `mDuration` (ms; 5000 until the graphic effect's is read)
    pub duration: u32,
    /// `mElapsedTime` (ms; saved)
    pub elapsed: f32,
    /// `mIsRestored`: loaded this tick (the rain's sound restarts)
    pub restored: bool,
    /// `mIsSustained`: the weather event keeps it going (saved)
    pub sustained: bool,
    /// `mDropCount` / `mFlakeCount`: drops spawned so far (saved)
    pub count: u32,
    /// `mLoadedDrops` / `mLoadedFlakes`: the saved number of drops, respawned by RepairPointers
    pub loaded: u32,
    /// the rain's `mRumbleDelay`: ms to the next thunder (saved)
    pub rumble_delay: f32,
}

impl Default for EeGfxEffectStorm {
    fn default() -> EeGfxEffectStorm {
        EeGfxEffectStorm {
            lists: std::ptr::null_mut(),
            center_x: 0,
            center_y: 0,
            fall_speed: 0.0,
            spawn_height: 0.0,
            density: 0.0,
            intensity: 1.0,
            duration: 5000,
            elapsed: 0.0,
            restored: false,
            sustained: false,
            count: 0,
            loaded: 0,
            rumble_delay: 0.0,
        }
    }
}

/// A falling drop or flake (`EE3DRainDrop`, `EEE3DSnowFlake`).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxStormDrop {
    pub position: Ge3DPoint,
    /// the ground it melts or splashes at (0 over water)
    pub ground_z: f32,
    /// a random turn (the flake's spin)
    pub angle: f32,
    /// the rain's drop is over water (it may ripple)
    pub water: bool,
    /// the flake's sideways drift (x, y in -1..1)
    pub drift: Ge3DPoint,
}

/// A storm's tiles (`mTiles`) and its drops or flakes (`mDrops`, `mFlakes`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct EeGfxStormLists {
    pub tiles: Vec<[i32; 2]>,
    pub drops: Vec<EeGfxStormDrop>,
}

/// A drop to draw this frame (`EE3DRainDraw`, `EE3DSnowDraw`: `Queue` copies the drops).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxStormDraw {
    pub position: Ge3DPoint,
    pub angle: f32,
    pub water: bool,
}

/// The world a storm reads and calls into.
pub trait StormHost {
    /// the map has the tile (`GetTile`)
    fn on_map(&mut self, x: i32, y: i32) -> bool;
    /// the tile is deep or shallow water (false off the map)
    fn is_water(&mut self, x: i32, y: i32) -> bool;
    /// the terrain mesh's `GetZ(x, y)`
    fn ground_z(&mut self, x: i32, y: i32) -> f32;
    /// `EEGetNextTile(from, EEGetTileDirection(from, to), 1)`: the tile one step from `from` towards `to`
    fn step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2];
    /// the land ripple placed where a drop hit the water (`CreatePlacedEffect`)
    fn ripple(&mut self, x: f32, y: f32);
}

/// The storm's tiles over the given ones (the constructor; the record holds the other defaults).
pub fn storm_new(lists: &mut EeGfxStormLists, tiles: &[[i32; 2]]) {
    lists.tiles = tiles.to_vec();
}

/// The rain's constructor: the first thunder within 2.25 s.
pub fn rain_new(r: &mut EeGfxEffectRecord) {
    r.storm.rumble_delay = random_float(0.0, 0.25) * RAIN_RUMBLE_DELAY;
}

/// The tiles' mean the original divides as unsigned.
fn mean(sum: i32, count: usize) -> i32 {
    ((sum as u32 as u64) / count as u64) as i32
}

/// `EEE3DStorm::GetCenter`: the centre becomes the tiles' mean; the point on the terrain there for the C++'s
/// `SetTransformation` (none without tiles).
pub fn storm_center<H: StormHost>(r: &mut EeGfxEffectRecord, l: &EeGfxStormLists, host: &mut H) -> Option<Ge3DPoint> {
    if l.tiles.is_empty() {
        return None;
    }
    let mut sx = 0i32;
    let mut sy = 0i32;
    for t in l.tiles.iter().rev() {
        sx = sx.wrapping_add(t[0]);
        sy = sy.wrapping_add(t[1]);
    }
    let s = &mut r.storm;
    s.center_y = mean(sy, l.tiles.len());
    s.center_x = mean(sx, l.tiles.len());
    let z = host.ground_z(s.center_x, s.center_y);
    Some(Ge3DPoint { x: s.center_x as f32, y: s.center_y as f32, z })
}

/// Extends `b` (left, top, right, bottom) by a tile (`U2DRectangle::Extend`).
fn extend(b: &mut [i32; 4], t: [i32; 2]) {
    b[0] = b[0].min(t[0]);
    b[2] = b[2].max(t[0]);
    b[1] = b[1].min(t[1]);
    b[3] = b[3].max(t[1]);
}

/// `EEE3DStorm::Move`: every tile one step towards its place around the new centre (one that would leave the map
/// stays), the new centre, the tiles' bounds in `bounds` (left, top, right, bottom; from the old centre), the radius
/// half the longer side; the point on the terrain at the centre for the C++'s `SetTransformation` (none and nothing
/// changed without tiles).
pub fn storm_move<H: StormHost>(
    r: &mut EeGfxEffectRecord,
    l: &mut EeGfxStormLists,
    to: [i32; 2],
    bounds: &mut [i32; 4],
    host: &mut H,
) -> Option<Ge3DPoint> {
    if l.tiles.is_empty() {
        return None;
    }
    let (cx, cy) = (r.storm.center_x, r.storm.center_y);
    *bounds = [cx, cy, cx, cy];
    let mut sx = 0i32;
    let mut sy = 0i32;
    for i in (0..l.tiles.len()).rev() {
        let t = l.tiles[i];
        let target = [to[0].wrapping_add(t[0].wrapping_sub(cx)), to[1].wrapping_add(t[1].wrapping_sub(cy))];
        let next = host.step_toward(t, target);
        if host.on_map(next[0], next[1]) {
            l.tiles[i] = next;
        }
        sx = sx.wrapping_add(l.tiles[i][0]);
        sy = sy.wrapping_add(l.tiles[i][1]);
        extend(bounds, l.tiles[i]);
    }
    r.storm.center_y = mean(sy, l.tiles.len());
    r.storm.center_x = mean(sx, l.tiles.len());
    let z = host.ground_z(r.storm.center_x, r.storm.center_y);
    let radius = (bounds[2].wrapping_sub(bounds[0]) / 2).max(bounds[3].wrapping_sub(bounds[1]) / 2);
    r.bounding_radius = radius as f32;
    Some(Ge3DPoint { x: r.storm.center_x as f32, y: r.storm.center_y as f32, z })
}

/// `EEE3DStorm::GetBounds`: the tiles' bounding box (false and `bounds` unchanged without tiles).
pub fn storm_bounds(l: &EeGfxStormLists, bounds: &mut [i32; 4]) -> bool {
    let Some(&last) = l.tiles.last() else {
        return false;
    };
    *bounds = [last[0], last[1], last[0], last[1]];
    for t in l.tiles[..l.tiles.len() - 1].iter().rev() {
        extend(bounds, *t);
    }
    true
}

/// `SetUp` (the rain's and the snow's, one body): the tiles' mean as floats, the radius reaching the farthest tile
/// in x or y; the point on the terrain there for the C++'s `SetTransformation`.
pub fn storm_set_up<H: StormHost>(r: &mut EeGfxEffectRecord, l: &EeGfxStormLists, host: &mut H) -> Ge3DPoint {
    let mut sx = 0i32;
    let mut sy = 0i32;
    for t in &l.tiles {
        sx = sx.wrapping_add(t[0]);
        sy = sy.wrapping_add(t[1]);
    }
    let n = l.tiles.len() as f32;
    let cx = sx as f32 / n;
    let cy = sy as f32 / n;
    let z = host.ground_z(um::floor(cx), um::floor(cy));
    let mut radius = 0.0f32;
    for t in &l.tiles {
        let d = (t[0] as f32 - cx).abs();
        if d > radius {
            radius = d;
        }
        let d = (t[1] as f32 - cy).abs();
        if d > radius {
            radius = d;
        }
    }
    r.bounding_radius = radius;
    Ge3DPoint { x: cx, y: cy, z }
}

/// `AddDrop` / `AddFlake`: a drop at a random point of a random tile at the spawn height, falling to the ground (0
/// over water); a flake also draws its drift.
pub fn add_drop<H: StormHost>(r: &EeGfxEffectRecord, l: &mut EeGfxStormLists, host: &mut H) {
    if l.tiles.is_empty() {
        return;
    }
    let index = random_long(0, (l.tiles.len() as i32).wrapping_sub(1));
    let tile = l.tiles[(index.max(0) as usize).min(l.tiles.len() - 1)];
    let rx = random_float(0.0, 1.0);
    let x = tile[0] as f32 + rx;
    let ry = random_float(0.0, 1.0);
    let y = tile[1] as f32 + ry;
    let water = host.is_water(tile[0], tile[1]);
    let ground_z = if water { 0.0 } else { host.ground_z(tile[0], tile[1]) };
    let angle = ry * TWO_PI;
    let mut drift = Ge3DPoint::default();
    if r.kind == EE_GFX_EFFECT_KIND_SNOW {
        drift.x = random_float(-1.0, 1.0);
        drift.y = random_float(-1.0, 1.0);
    }
    l.drops.push(EeGfxStormDrop {
        position: Ge3DPoint { x, y, z: r.storm.spawn_height },
        ground_z,
        angle,
        water,
        drift,
    });
}

/// The rain's thunder timer (`StartSounds`' part): the delay counts down by the tick; when it runs out, the next
/// is 5 to 14 s away. 0: no thunder; 1 or 2: the graphic effect's rumble sound 1 or 2 plays.
pub fn rain_rumble(r: &mut EeGfxEffectRecord, tick_time: f32) -> i32 {
    r.storm.rumble_delay -= tick_time;
    if r.storm.rumble_delay > 0.0 {
        return 0;
    }
    let which = if random_long(0, 10) > 4 { 1 } else { 2 };
    r.storm.rumble_delay = um::floor(random_float(0.0, 1.0) * RAIN_RUMBLE_DELAY).wrapping_add(5000) as f32;
    which
}

/// `UpdateDrops` / `UpdateFlakes`: new drops towards a count that ramps up (6.5 s for rain, 0.5 s for snow), then
/// keeps growing (at most 65 a tick, none above 1400); the drops fall (the snow by the tick over the game speed,
/// spinning and drifting), and those below the ground go (a rain drop over water ripples one time in 16).
fn update_drops<H: StormHost>(
    r: &mut EeGfxEffectRecord,
    l: &mut EeGfxStormLists,
    tick_time: f32,
    game_speed: f32,
    host: &mut H,
) {
    let snow = r.kind == EE_GFX_EFFECT_KIND_SNOW;
    let s = r.storm;
    let n = l.tiles.len() as f32;
    let (elapsed_s, target, fade) = if snow {
        let time = if s.elapsed < SNOW_FADE_TIME { s.elapsed } else { SNOW_FADE_TIME };
        (tick_time * 0.001 / game_speed, um::lrintf(time * 0.001 * n * s.density * SNOW_DENSITY), SNOW_FADE_TIME)
    } else {
        let time = if s.elapsed < RAIN_FADE_TIME { s.elapsed } else { RAIN_FADE_TIME };
        (tick_time * 0.001, um::lrintf(n * time * RAIN_DENSITY * s.density * 0.001), RAIN_FADE_TIME)
    };
    let mut extra = 0u32;
    if s.elapsed > fade {
        let v = n * (s.density * ((s.elapsed - fade) * 0.001));
        extra = um::lrintf(if v > 0.0 { v } else { 0.0 }) as u32;
    }
    let mut missing = 0u32;
    if s.duration as f32 > s.elapsed || s.sustained {
        missing = (target as u32).wrapping_sub(s.count).wrapping_add(extra);
        if (missing as i32) < 0 {
            missing = (missing as i32).wrapping_neg() as u32;
        }
    }
    let mut count = um::lrintf(missing as f32 * s.intensity) as u32;
    if count >= MAX_NEW_DROPS {
        count = MAX_NEW_DROPS;
    }
    if l.drops.len() > MAX_DROPS {
        count = 0;
    }
    for _ in 0..count {
        add_drop(r, l, host);
        r.storm.count = r.storm.count.wrapping_add(1);
    }
    let fall = elapsed_s * s.fall_speed;
    let mut i = 0;
    while i < l.drops.len() {
        let d = &mut l.drops[i];
        let gone = if snow {
            d.position.z = fall + d.position.z;
            d.angle += SNOW_SPIN;
            if random() < SNOW_TURN_CHANCE {
                d.drift.x = random_float(-1.0, 1.0);
                d.drift.y = random_float(-1.0, 1.0);
                d.drift.z = 0.0;
            }
            d.position.x = d.drift.x * SNOW_DRIFT + d.position.x;
            d.position.y = d.drift.y * SNOW_DRIFT + d.position.y;
            d.ground_z > d.position.z
        } else {
            d.position.z += fall;
            d.position.z < d.ground_z
        };
        if !gone {
            i += 1;
            continue;
        }
        let d = l.drops.remove(i);
        if !snow && random_long(0, 15) == 0 && d.water {
            host.ripple(d.position.x, d.position.y);
        }
    }
}

/// `Step` (after the rain's sounds): the drops, the storm's age; 7 once its duration has passed unless sustained
/// (the snow also waits for its last flake).
pub fn storm_step<H: StormHost>(
    r: &mut EeGfxEffectRecord,
    l: &mut EeGfxStormLists,
    tick_time: f32,
    game_speed: f32,
    host: &mut H,
) -> i32 {
    update_drops(r, l, tick_time, game_speed, host);
    r.storm.elapsed += tick_time;
    let mut res = 0;
    if r.storm.elapsed >= r.storm.duration as f32
        && !r.storm.sustained
        && (r.kind != EE_GFX_EFFECT_KIND_SNOW || l.drops.is_empty())
    {
        res = 7;
    }
    r.storm.restored = false;
    res
}

/// `RepairPointers`' part (after the C++ read the graphic effect again): the saved number of drops respawned at
/// random heights, the storm restored.
pub fn storm_repair<H: StormHost>(r: &mut EeGfxEffectRecord, l: &mut EeGfxStormLists, host: &mut H) {
    for _ in 0..r.storm.loaded {
        add_drop(r, l, host);
    }
    for d in &mut l.drops {
        d.position.z = random() * r.storm.spawn_height;
    }
    r.storm.loaded = 0;
    r.storm.restored = true;
}

/// `U2DPoint<long>::Load(FSFile&)`: 4 on a closed file.
fn load_center<F: EffectFile>(f: &mut F, x: &mut i32, y: &mut i32) -> i32 {
    if !f.is_open() {
        return 4;
    }
    let mut res = f.read_i32(x);
    if res == OK {
        res = f.read_i32(y);
    }
    res
}

/// `EEE3DStorm::Load`'s and the leaf's own reads after `EEEffect3D::Load`'s: the age, the tiles, the centre, the
/// intensity, the sustained flag; the count, the rain's thunder delay, a position (discarded), the radius and the
/// number of drops to respawn; the first error.
pub fn load_storm<F: EffectFile>(r: &mut EeGfxEffectRecord, l: &mut EeGfxStormLists, f: &mut F) -> i32 {
    let s = &mut r.storm;
    let mut res = f.read_f32(&mut s.elapsed);
    let mut count = 0u32;
    if res == OK {
        res = f.read_u32(&mut count);
    }
    if res == OK {
        let mut i = 0;
        while i < count && res == OK {
            i += 1;
            let mut x = 0i32;
            let mut y = 0i32;
            res = f.read_i32(&mut x);
            if res != OK {
                continue;
            }
            res = f.read_i32(&mut y);
            if res != OK {
                continue;
            }
            l.tiles.push([x, y]);
        }
    }
    if res == OK {
        res = load_center(f, &mut s.center_x, &mut s.center_y);
    }
    if res == OK {
        res = f.read_f32(&mut s.intensity);
    }
    if res == OK {
        res = f.read_bool(&mut s.sustained);
    }
    if res == OK {
        res = f.read_u32(&mut s.count);
    }
    if res == OK && r.kind == EE_GFX_EFFECT_KIND_RAIN {
        res = f.read_f32(&mut s.rumble_delay);
    }
    let mut position = Ge3DPoint::default();
    if res == OK {
        res = load_point(f, &mut position);
    }
    let mut radius = 0.0f32;
    if res == OK {
        res = f.read_f32(&mut radius);
    }
    if res == OK {
        r.bounding_radius = radius;
        res = f.read_u32(&mut s.loaded);
    }
    res
}

/// `EEE3DStorm::Save`'s and the leaf's own writes after `EEEffect3D::Save`'s (the position from the
/// transformation); the first error.
pub fn save_storm<F: EffectFile>(r: &EeGfxEffectRecord, l: &EeGfxStormLists, f: &mut F) -> i32 {
    let s = &r.storm;
    let mut res = f.write_f32(s.elapsed);
    if res == OK {
        res = f.write_u32(l.tiles.len() as u32);
    }
    if res == OK {
        for t in &l.tiles {
            if res != OK {
                break;
            }
            res = f.write_i32(t[0]);
            if res == OK {
                res = f.write_i32(t[1]);
            }
        }
    }
    if res == OK {
        res = if f.is_open() {
            let mut v = f.write_i32(s.center_x);
            if v == OK {
                v = f.write_i32(s.center_y);
            }
            v
        } else {
            4
        };
    }
    if res == OK {
        res = f.write_f32(s.intensity);
    }
    if res == OK {
        res = f.write_bool(s.sustained);
    }
    if res == OK {
        res = f.write_u32(s.count);
    }
    if res == OK && r.kind == EE_GFX_EFFECT_KIND_RAIN {
        res = f.write_f32(s.rumble_delay);
    }
    if res == OK {
        let t = &r.transformation.m;
        res = save_point(f, &Ge3DPoint { x: t[0][3], y: t[1][3], z: t[2][3] });
    }
    if res == OK {
        res = f.write_f32(r.bounding_radius);
    }
    if res == OK {
        res = f.write_u32(l.drops.len() as u32);
    }
    res
}

/// `Queue`'s copy of the drops into the draw list (as many as fit in `out`).
pub fn storm_draws(l: &EeGfxStormLists, out: &mut [EeGfxStormDraw]) {
    for (o, d) in out.iter_mut().zip(&l.drops) {
        *o = EeGfxStormDraw { position: d.position, angle: d.angle, water: d.water };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::effect_state::{random_seed, set_random_seed, RANDOM_TESTS};
    use ee_random::URandom;

    /// A map of land tiles at height x + y, water where x is 7, 0..20 square; the moves step one tile in x towards
    /// the target and the ripples are kept.
    #[derive(Default)]
    struct World {
        ripples: Vec<(f32, f32)>,
        calls: Vec<char>,
    }

    impl StormHost for World {
        fn on_map(&mut self, x: i32, y: i32) -> bool {
            self.calls.push('m');
            (0..20).contains(&x) && (0..20).contains(&y)
        }
        fn is_water(&mut self, x: i32, _y: i32) -> bool {
            self.calls.push('w');
            x == 7
        }
        fn ground_z(&mut self, x: i32, y: i32) -> f32 {
            self.calls.push('z');
            (x + y) as f32
        }
        fn step_toward(&mut self, from: [i32; 2], to: [i32; 2]) -> [i32; 2] {
            self.calls.push('s');
            [from[0] + (to[0] - from[0]).signum(), from[1] + (to[1] - from[1]).signum()]
        }
        fn ripple(&mut self, x: f32, y: f32) {
            self.ripples.push((x, y));
        }
    }

    /// A file of typed values (each write and its type).
    #[derive(Default)]
    struct Mem {
        items: Vec<(char, u32)>,
        at: usize,
        fail_at: Option<usize>,
    }

    impl Mem {
        fn read(&mut self, ty: char) -> Result<u32, i32> {
            if self.fail_at == Some(self.at) || self.at >= self.items.len() {
                return Err(3);
            }
            let (t, v) = self.items[self.at];
            assert_eq!(t, ty, "item {}", self.at);
            self.at += 1;
            Ok(v)
        }
        fn write(&mut self, ty: char, v: u32) -> i32 {
            if self.fail_at == Some(self.items.len()) {
                return 5;
            }
            self.items.push((ty, v));
            0
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
            self.write('u', v)
        }
        fn write_i32(&mut self, v: i32) -> i32 {
            self.write('i', v as u32)
        }
        fn write_f32(&mut self, v: f32) -> i32 {
            self.write('f', v.to_bits())
        }
        fn write_bool(&mut self, v: bool) -> i32 {
            self.write('b', v as u32)
        }
    }

    fn storm(kind: i32, tiles: &[[i32; 2]]) -> (EeGfxEffectRecord, EeGfxStormLists) {
        let r = EeGfxEffectRecord::new(kind);
        let mut l = EeGfxStormLists::default();
        storm_new(&mut l, tiles);
        (r, l)
    }

    /// The original's `GetEffectRandomLong` over a generator of its own.
    fn long(g: &mut URandom, min: i32, max: i32) -> i32 {
        let v = um::lrintf(g.generate() * (max - min) as f32 + min as f32);
        v.min(max)
    }

    #[test]
    fn defaults_center_and_bounds() {
        let (mut r, l) = storm(EE_GFX_EFFECT_KIND_RAIN, &[[4, 5], [6, 9], [2, 7]]);
        assert_eq!(r.storm.intensity, 1.0);
        assert_eq!(r.storm.duration, 5000);
        let mut w = World::default();
        let p = storm_center(&mut r, &l, &mut w).unwrap();
        assert_eq!((r.storm.center_x, r.storm.center_y), (4, 7));
        assert_eq!(p, Ge3DPoint { x: 4.0, y: 7.0, z: 11.0 });
        let mut b = [99; 4];
        assert!(storm_bounds(&l, &mut b));
        assert_eq!(b, [2, 5, 6, 9]);
        let empty = EeGfxStormLists::default();
        assert!(!storm_bounds(&empty, &mut b));
        assert_eq!(b, [2, 5, 6, 9]);
        assert!(storm_center(&mut r, &empty, &mut w).is_none());
        assert_eq!((r.storm.center_x, r.storm.center_y), (4, 7));
    }

    #[test]
    fn move_steps_each_tile_once_and_keeps_tiles_on_the_map() {
        let (mut r, mut l) = storm(EE_GFX_EFFECT_KIND_SNOW, &[[0, 0], [2, 0], [1, 2]]);
        let mut w = World::default();
        storm_center(&mut r, &l, &mut w);
        assert_eq!((r.storm.center_x, r.storm.center_y), (1, 0));
        let mut b = [0; 4];
        // towards (-5, 3): x would leave the map for the first tile, which stays
        let p = storm_move(&mut r, &mut l, [-5, 3], &mut b, &mut w).unwrap();
        assert_eq!(l.tiles, vec![[0, 0], [1, 1], [0, 3]]);
        // the bounds start at the old centre
        assert_eq!(b, [0, 0, 1, 3]);
        assert_eq!((r.storm.center_x, r.storm.center_y), (0, 1));
        assert_eq!(r.bounding_radius, 1.0);
        assert_eq!(p, Ge3DPoint { x: 0.0, y: 1.0, z: 1.0 });
        let mut e = EeGfxStormLists::default();
        assert!(storm_move(&mut r, &mut e, [3, 3], &mut b, &mut w).is_none());
    }

    #[test]
    fn set_up_center_and_radius() {
        let (mut r, l) = storm(EE_GFX_EFFECT_KIND_RAIN, &[[3, 4], [4, 4], [6, 5]]);
        let mut w = World::default();
        let p = storm_set_up(&mut r, &l, &mut w);
        let cx = 13.0f32 / 3.0;
        let cy = 13.0f32 / 3.0;
        assert_eq!((p.x, p.y), (cx, cy));
        assert_eq!(p.z, 8.0);
        assert_eq!(r.bounding_radius, (6.0 - cx).abs());
    }

    #[test]
    fn drops_spawn_fall_and_ripple_in_the_original_draw_order() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(77);
        let (mut r, mut l) = storm(EE_GFX_EFFECT_KIND_RAIN, &[[7, 1], [3, 2]]);
        r.storm.fall_speed = -40.0;
        r.storm.spawn_height = 10.0;
        r.storm.density = 1.0;
        r.storm.duration = 100_000;
        let mut w = World::default();
        let mut g = URandom { seed: 77 };
        // tick 1: elapsed 0: no target, no extra
        assert_eq!(storm_step(&mut r, &mut l, 100.0, 1.0, &mut w), 0);
        assert!(l.drops.is_empty());
        assert_eq!(random_seed(), 77);
        // tick 2 at 100 ms: target lrintf(2 * 100 * 0.4 * 1 * 0.001) = 0
        r.storm.elapsed = 5000.0;
        // target lrintf(2 * 5000 * 0.4 * 0.001) = 4
        assert_eq!(storm_step(&mut r, &mut l, 100.0, 1.0, &mut w), 0);
        assert_eq!(r.storm.count, 4);
        assert_eq!(l.drops.len(), 4);
        for d in &l.drops {
            let i = long(&mut g, 0, 1);
            let tile = [[7, 1], [3, 2]][i as usize];
            let rx = g.generate_range(0.0, 1.0);
            let ry = g.generate_range(0.0, 1.0);
            assert_eq!(d.position.x, tile[0] as f32 + rx);
            assert_eq!(d.position.y, tile[1] as f32 + ry);
            assert_eq!(d.angle, ry * TWO_PI);
            assert_eq!(d.water, tile[0] == 7);
            // fell 4 by the same tick
            assert_eq!(d.position.z, 10.0 + 0.1 * -40.0);
            assert_eq!(d.ground_z, if d.water { 0.0 } else { (tile[0] + tile[1]) as f32 });
        }
        assert_eq!(r.storm.elapsed, 5100.0);
        // the drops reach the ground (z 6 - 8 = -2): each draws 0..15, a water one ripples on 0
        r.storm.fall_speed = -80.0;
        r.storm.duration = 0;
        let drops = l.drops.clone();
        assert_eq!(storm_step(&mut r, &mut l, 100.0, 1.0, &mut w), 7);
        let mut ripples = Vec::new();
        for d in &drops {
            if long(&mut g, 0, 15) == 0 && d.water {
                ripples.push((d.position.x, d.position.y));
            }
        }
        assert!(l.drops.is_empty());
        assert_eq!(w.ripples, ripples);
        assert_eq!(random_seed(), g.seed);
    }

    #[test]
    fn missing_count_is_unsigned_and_capped() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        let (mut r, mut l) = storm(EE_GFX_EFFECT_KIND_RAIN, &[[3, 3]]);
        r.storm.density = 1.0;
        r.storm.elapsed = 6500.0;
        r.storm.count = 10;
        r.storm.fall_speed = 0.0;
        r.storm.spawn_height = 100.0;
        r.storm.duration = 10_000_000;
        let mut w = World::default();
        // target lrintf(1 * 6500 * 0.4 * 0.001) = 3: missing |3 - 10| = 7
        storm_step(&mut r, &mut l, 0.0, 1.0, &mut w);
        assert_eq!(r.storm.count, 17);
        assert_eq!(l.drops.len(), 7);
        // far behind: at most 65
        r.storm.count = 0;
        r.storm.elapsed = 6500.0 + 1_000_000.0;
        storm_step(&mut r, &mut l, 0.0, 1.0, &mut w);
        assert_eq!(r.storm.count, 65);
        // none above 1400 drops
        l.drops.resize(1401, EeGfxStormDrop::default());
        storm_step(&mut r, &mut l, 0.0, 1.0, &mut w);
        assert_eq!(r.storm.count, 65);
        // past its duration and not sustained: none
        l.drops.clear();
        r.storm.duration = 0;
        r.storm.elapsed = 10.0;
        assert_eq!(storm_step(&mut r, &mut l, 0.0, 1.0, &mut w), 7);
        assert!(l.drops.is_empty());
        r.storm.sustained = true;
        assert_eq!(storm_step(&mut r, &mut l, 0.0, 1.0, &mut w), 0);
        assert!(!l.drops.is_empty());
    }

    #[test]
    fn snow_drifts_spins_and_waits_for_its_last_flake() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(5);
        let (mut r, mut l) = storm(EE_GFX_EFFECT_KIND_SNOW, &[[2, 2]]);
        r.storm.spawn_height = 3.0;
        r.storm.fall_speed = -1.0;
        let mut w = World::default();
        let mut g = URandom { seed: 5 };
        add_drop(&r, &mut l, &mut w);
        let i = long(&mut g, 0, 0);
        assert_eq!(i, 0);
        let rx = g.generate_range(0.0, 1.0);
        let ry = g.generate_range(0.0, 1.0);
        let dx = g.generate_range(-1.0, 1.0);
        let dy = g.generate_range(-1.0, 1.0);
        let d = l.drops[0];
        assert_eq!(d.position, Ge3DPoint { x: 2.0 + rx, y: 2.0 + ry, z: 3.0 });
        assert_eq!(d.drift, Ge3DPoint { x: dx, y: dy, z: 0.0 });
        assert_eq!(d.ground_z, 4.0);
        r.storm.duration = 0;
        // game speed 2: half the fall, below the ground 4 from 3 - 0.5: gone, and over (past its duration, no flake)
        assert_eq!(storm_step(&mut r, &mut l, 1000.0, 2.0, &mut w), 7);
        if g.generate() < SNOW_TURN_CHANCE {
            g.generate_range(-1.0, 1.0);
            g.generate_range(-1.0, 1.0);
        }
        assert!(l.drops.is_empty());
        assert_eq!(random_seed(), g.seed);
        // a flake above the ground drifts and spins
        l.drops.push(EeGfxStormDrop {
            position: Ge3DPoint { x: 1.0, y: 1.0, z: 9.0 },
            ground_z: 0.0,
            angle: 0.5,
            water: false,
            drift: Ge3DPoint { x: 1.0, y: -1.0, z: 0.0 },
        });
        assert_eq!(storm_step(&mut r, &mut l, 1000.0, 1.0, &mut w), 0);
        let f = l.drops[0];
        assert_eq!(f.position.z, 8.0);
        assert_eq!(f.angle, 0.5 + SNOW_SPIN);
        let turned = g.generate() < SNOW_TURN_CHANCE;
        let (mx, my) = if turned { (g.generate_range(-1.0, 1.0), g.generate_range(-1.0, 1.0)) } else { (1.0, -1.0) };
        assert_eq!(f.position.x, mx * SNOW_DRIFT + 1.0);
        assert_eq!(f.position.y, my * SNOW_DRIFT + 1.0);
    }

    #[test]
    fn rumble_timer() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(9);
        let mut r = EeGfxEffectRecord::new(EE_GFX_EFFECT_KIND_RAIN);
        let mut g = URandom { seed: 9 };
        rain_new(&mut r);
        assert_eq!(r.storm.rumble_delay, g.generate_range(0.0, 0.25) * RAIN_RUMBLE_DELAY);
        r.storm.rumble_delay = 150.0;
        assert_eq!(rain_rumble(&mut r, 100.0), 0);
        assert_eq!(r.storm.rumble_delay, 50.0);
        let which = rain_rumble(&mut r, 100.0);
        assert_eq!(which, if long(&mut g, 0, 10) > 4 { 1 } else { 2 });
        let next = (um::floor(g.generate_range(0.0, 1.0) * RAIN_RUMBLE_DELAY) + 5000) as f32;
        assert_eq!(r.storm.rumble_delay, next);
        assert_eq!(random_seed(), g.seed);
    }

    #[test]
    fn repair_respawns_at_random_heights() {
        let _turn = RANDOM_TESTS.lock().unwrap_or_else(|e| e.into_inner());
        set_random_seed(31);
        let (mut r, mut l) = storm(EE_GFX_EFFECT_KIND_RAIN, &[[1, 1], [2, 2]]);
        r.storm.loaded = 3;
        r.storm.spawn_height = 20.0;
        let mut w = World::default();
        let mut g = URandom { seed: 31 };
        storm_repair(&mut r, &mut l, &mut w);
        assert_eq!(l.drops.len(), 3);
        for _ in 0..3 {
            long(&mut g, 0, 1);
            g.generate_range(0.0, 1.0);
            g.generate_range(0.0, 1.0);
        }
        for d in &l.drops {
            assert_eq!(d.position.z, g.generate() * 20.0);
        }
        assert_eq!(r.storm.loaded, 0);
        assert!(r.storm.restored);
        assert_eq!(r.storm.count, 0);
        assert_eq!(random_seed(), g.seed);
    }

    #[test]
    fn save_and_load_round_trip_both_kinds() {
        for kind in [EE_GFX_EFFECT_KIND_RAIN, EE_GFX_EFFECT_KIND_SNOW] {
            let (mut r, mut l) = storm(kind, &[[1, 2], [3, 4]]);
            r.storm.elapsed = 1234.5;
            r.storm.center_x = 2;
            r.storm.center_y = 3;
            r.storm.intensity = 0.75;
            r.storm.sustained = true;
            r.storm.count = 42;
            r.storm.rumble_delay = 777.0;
            r.transformation.m[0][3] = 2.0;
            r.transformation.m[1][3] = 3.0;
            r.transformation.m[2][3] = 1.5;
            r.bounding_radius = 1.25;
            l.drops.resize(5, EeGfxStormDrop::default());
            let mut m = Mem::default();
            assert_eq!(save_storm(&r, &l, &mut m), 0);
            let types: String = m.items.iter().map(|x| x.0).collect();
            let rain = if kind == EE_GFX_EFFECT_KIND_RAIN { "f" } else { "" };
            assert_eq!(types, format!("fuiiiiiifbu{rain}ffffu"));
            let mut back = EeGfxEffectRecord::new(kind);
            let mut bl = EeGfxStormLists::default();
            assert_eq!(load_storm(&mut back, &mut bl, &mut m), 0);
            assert_eq!(bl.tiles, l.tiles);
            assert!(bl.drops.is_empty());
            assert_eq!(back.storm.elapsed, 1234.5);
            assert_eq!((back.storm.center_x, back.storm.center_y), (2, 3));
            assert_eq!(back.storm.intensity, 0.75);
            assert!(back.storm.sustained);
            assert_eq!(back.storm.count, 42);
            assert_eq!(back.storm.rumble_delay, if kind == EE_GFX_EFFECT_KIND_RAIN { 777.0 } else { 0.0 });
            assert_eq!(back.bounding_radius, 1.25);
            assert_eq!(back.storm.loaded, 5);
            // a failed write stops there; a failed read too, the tiles read so far kept
            let mut f = Mem { fail_at: Some(3), ..Mem::default() };
            assert_eq!(save_storm(&r, &l, &mut f), 5);
            assert_eq!(f.items.len(), 3);
            let mut g = Mem { items: m.items.clone(), fail_at: Some(4), ..Mem::default() };
            let mut gl = EeGfxStormLists::default();
            assert_eq!(load_storm(&mut EeGfxEffectRecord::new(kind), &mut gl, &mut g), 3);
            assert_eq!(gl.tiles, vec![[1, 2]]);
        }
    }

    #[test]
    fn draws_copy_the_drops() {
        let mut l = EeGfxStormLists::default();
        l.drops.push(EeGfxStormDrop {
            position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 },
            ground_z: 0.0,
            angle: 0.5,
            water: true,
            drift: Ge3DPoint::default(),
        });
        let mut out = [EeGfxStormDraw::default(); 1];
        storm_draws(&l, &mut out);
        assert_eq!(out[0], EeGfxStormDraw { position: Ge3DPoint { x: 1.0, y: 2.0, z: 3.0 }, angle: 0.5, water: true });
    }
}
