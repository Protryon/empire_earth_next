//! The scenario editor's paint areas over the generic control shell (`EEUIShellControl`, `shells.rs`; UI cluster
//! round 4): once `EEUIFSEPaint` (the brush square painted along the mouse's path), `EEUIFSEPaintElevation` (the
//! elevation tab's: the level to paint, the right-button drag of a tile's height) and `EEUIFSETerrainPaint` (the
//! terrain tab's: its action is the tab's, `editor_terrain.rs`). The world (the picked tile, the map's heights, the
//! screen line, the paint actions, the brush highlight, the mouse and scroll bounds) is the host's; the geometry is
//! here, in the original's float order.

use core::cell::Cell;
use core::ffi::CStr;

use ee_base::math::types::{Ge2DPoint, Ge3DLine, Ge3DPlane, Ge3DPoint, GeVector};
use ee_base::math::um;

use crate::screens::shells::*;
use crate::screens::shells_tech::EE_SCREENS_SH_FORM_CHECKED;
use crate::screens::shells_unit::EE_SCREENS_SH_ALWAYS_DIRTY;

/// `EEUIFSEPaintElevation` (a control; args: the scroll action, the brush group).
pub const EE_SCREENS_SK_PAINT_ELEVATION: u32 = 0x60;
/// `EEUIFSETerrainPaint` (a control; args: the scroll action, the brush group, the terrain tab).
pub const EE_SCREENS_SK_TERRAIN_PAINT: u32 = 0x61;

/// `mIsCentered` / `SetIsCentered(a)`.
pub const EE_SCREENS_SKOP_PAINT_CENTERED: u32 = 0x60;
pub const EE_SCREENS_SKOP_PAINT_SET_CENTERED: u32 = 0x61;
/// `StoreMouseTile()`.
pub const EE_SCREENS_SKOP_PAINT_STORE_TILE: u32 = 0x62;
/// `CreatePaintAction()` -> the new action (an `EESEAPaint*`; NULL: none).
pub const EE_SCREENS_SKOP_PAINT_CREATE: u32 = 0x63;

/// The point under `f[0..2]` (`EEPointUnitOrTile`): -> whether a tile is there, `f[0..3]` the point. `a` 1:
/// `EEPointUnitOrTileXY` (-> its result, `f[0..2]` the tile, `f[2]` its height); `a` 2: the map's `GetLocationAt`
/// (-> whether a tile, `f[0..3]` the point, `f[3]` the tile's height).
pub const EE_SCREENS_SH_PICK: u32 = 0x80;
/// -> whether the map has the tile (`a`, `b`); `f[0]` its height.
pub const EE_SCREENS_SH_MAP_TILE: u32 = 0x81;
/// `f[0]` = the map's `GetF1955F4()`.
pub const EE_SCREENS_SH_MAP_HALF: u32 = 0x82;
/// The map's `GetScreenLine(f[0..2])` into the six floats at `a` (start, direction).
pub const EE_SCREENS_SH_SCREEN_LINE: u32 = 0x83;
/// `f[0..2]`: the control's mouse location (`a` 0) or `gMouse->mPosition` (1).
pub const EE_SCREENS_SH_MOUSE_AT: u32 = 0x84;
/// `f[0..4]`: the game view's rectangle (`gViewport`).
pub const EE_SCREENS_SH_VIEW_RECT: u32 = 0x85;
/// -> the game window's width | height << 32.
pub const EE_SCREENS_SH_WINDOW_SIZE: u32 = 0x86;
/// `gMouse->SetBounds(f[0..4])`.
pub const EE_SCREENS_SH_MOUSE_BOUNDS: u32 = 0x87;
/// The scroll action `a`'s `mEdges = f[0..4]`.
pub const EE_SCREENS_SH_SCROLL_EDGES: u32 = 0x88;
/// `CaptureMouse()`.
pub const EE_SCREENS_SH_CAPTURE_MOUSE: u32 = 0x89;
/// -> 1 the right button down, 2 the left, 4 the control has the mouse.
pub const EE_SCREENS_SH_MOUSE_BUTTONS: u32 = 0x8a;
/// -> `new EESEAPaintRust(a, b)` (an elevation's level, a terrain's type and colour: b's halves).
pub const EE_SCREENS_SH_PAINT_NEW: u32 = 0x8b;
/// -> the action `a`'s `SetArea` of the four `long`s at `b`.
pub const EE_SCREENS_SH_PAINT_AREA: u32 = 0x8c;
/// `gUserInterface->PostAction(a)`.
pub const EE_SCREENS_SH_ACTION_POST: u32 = 0x8d;
/// The action `a`'s `Dereference()`.
pub const EE_SCREENS_SH_ACTION_RELEASE: u32 = 0x8e;
/// The editor's brush group's `SetHighlight(f[0], f[1], a)`.
pub const EE_SCREENS_SH_BRUSH_HIGHLIGHT: u32 = 0x8f;
/// -> the brush group `a`'s `mBrushSize`.
pub const EE_SCREENS_SH_BRUSH_GROUP_SIZE: u32 = 0x90;
/// -> whether the Space key is held (`GetEntity`'s result 0 and its value above 0).
pub const EE_SCREENS_SH_SPACE: u32 = 0x91;
/// -> the form's slider of id `a`: its `GetMaxRange()` << 32 | `GetCurrentRangeValue()` (both `long`s).
pub const EE_SCREENS_SH_SLIDER_OF: u32 = 0x92;
/// -> the terrain tab `a`'s Rust (`EEUIFSETerrain::mRust`: made after its paint area).
pub const EE_SCREENS_SH_TERRAIN_RUST: u32 = 0x93;

/// `kSEAPaintElevation` (ee/EESEAPaint.h).
const PAINT_ELEVATION: i64 = 1;
/// `kSEElevationRaiseButton`, `kSEElevationLowerButton`, `kSEElevationElevationSlider`, `kSEElevationStrengthSlider`.
const RAISE_BUTTON: i64 = 0x96;
const LOWER_BUTTON: i64 = 0x97;
const ELEVATION_SLIDER: i64 = 0x9d;
const STRENGTH_SLIDER: i64 = 0xa2;
/// `kSEPELevelHeight` (0x445618) and the scales (the unit's float literals).
const LEVEL_HEIGHT: f32 = 0.204_124_14;
const RAISE_SCALE: f32 = 4.898_979_7;
const LOWER_SCALE: f32 = 1.224_744_9;

/// The x64 float to `long` conversion (`cvttss2si`): truncation, `i32::MIN` for NaN and out of range.
pub(crate) fn cvtt(value: f32) -> i32 {
    if value.is_nan() || !(-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        i32::MIN
    } else {
        value as i32
    }
}

/// `(char)UMFloor(v)`.
fn floor_char(v: f32) -> i8 {
    um::floor(v) as i8
}

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    let arg = |i: usize| args.get(i).copied().unwrap_or(0);
    let paint = |centered| Paint {
        brushes: arg(1),
        scroll: arg(0),
        radius: Cell::new(0),
        pac: Cell::new(0),
        last: Cell::new((0, 0)),
        centered: Cell::new(centered),
    };
    Some(match kind {
        EE_SCREENS_SK_PAINT_ELEVATION => Box::new(Elevation {
            paint: paint(false),
            plane: Cell::new(Ge3DPlane::default()),
            tile: Cell::new((0, 0)),
            brush: Cell::new(0),
            level: Cell::new(0),
        }),
        EE_SCREENS_SK_TERRAIN_PAINT => Box::new(TerrainPaint { paint: paint(true), tab: arg(2) }),
        _ => return None,
    })
}

/// The ops the paint kinds call, in their shapes.
trait PaintCtx {
    fn floats(&self, op: u32, a: i64, f: [f32; 4]) -> (i64, [f32; 4]);
    fn rect(&self, op: u32) -> [f32; 4];
    /// The window's rectangle (0, 0, width, height).
    fn window(&self) -> [f32; 4];
    fn buttons(&self) -> i64;
}

impl PaintCtx for Ctx<'_> {
    fn floats(&self, op: u32, a: i64, f: [f32; 4]) -> (i64, [f32; 4]) {
        let mut f = f;
        let r = self.host.call(self.ctl, op, a, 0, &mut f);
        (r, f)
    }
    fn rect(&self, op: u32) -> [f32; 4] {
        self.floats(op, 0, [0.0; 4]).1
    }
    fn window(&self) -> [f32; 4] {
        let s = self.call(EE_SCREENS_SH_WINDOW_SIZE, 0, 0);
        [0.0, 0.0, (s & 0xffff_ffff) as u32 as f32, (s >> 32) as u32 as f32]
    }
    fn buttons(&self) -> i64 {
        self.call(EE_SCREENS_SH_MOUSE_BUTTONS, 0, 0)
    }
}

const RIGHT_DOWN: i64 = 1;
const LEFT_DOWN: i64 = 2;
const HAS_CAPTURE: i64 = 4;

/// `EEUIFSEPaint`'s members and its non-virtual body.
struct Paint {
    /// `mBrushes`, `mScroll` (C++ objects).
    brushes: i64,
    scroll: i64,
    /// `mBrushRadius`.
    radius: Cell<i32>,
    /// `mPac` (the action in progress, an `EESEAPaint*`).
    pac: Cell<i64>,
    /// `mLastTileX`, `mLastTileY`.
    last: Cell<(i32, i32)>,
    /// `mIsCentered`.
    centered: Cell<bool>,
}

impl Paint {
    /// The mouse bounded by `r` and the scroll's edges one pixel inside (`l, t, r - 1, b - 1`).
    fn bound(&self, c: &Ctx, r: [f32; 4], edges_left_top: [f32; 2]) {
        c.floats(EE_SCREENS_SH_MOUSE_BOUNDS, 0, r);
        c.floats(
            EE_SCREENS_SH_SCROLL_EDGES,
            self.scroll,
            [edges_left_top[0], edges_left_top[1], r[2] - 1.0, r[3] - 1.0],
        );
    }
    /// The tile of a picked point (centred: shifted by half a tile and looked up again).
    fn tile_of(&self, c: &Ctx, found: bool, x: f32, y: f32) -> (bool, i32, i32) {
        if self.centered.get() {
            let (tx, ty) = (cvtt(x - 0.5), cvtt(y - 0.5));
            (c.call(EE_SCREENS_SH_MAP_TILE, i64::from(tx), i64::from(ty)) != 0, tx, ty)
        } else {
            (found, cvtt(x), cvtt(y))
        }
    }
    /// `SetArea(x - r, y + r, x + r, y - r)` on the action.
    fn area(c: &Ctx, action: i64, x: i32, y: i32, r: i32) -> bool {
        let a = [x.wrapping_sub(r), y.wrapping_add(r), x.wrapping_add(r), y.wrapping_sub(r)];
        c.call(EE_SCREENS_SH_PAINT_AREA, action, a.as_ptr() as i64) != 0
    }
    fn store_mouse_tile(&self, c: &Ctx) {
        let (_, m) = c.floats(EE_SCREENS_SH_MOUSE_AT, 0, [0.0; 4]);
        let (ok, f) = c.floats(EE_SCREENS_SH_PICK, 1, m);
        if ok != 0 {
            self.last.set((f[0] as i32, f[1] as i32));
        }
    }
    /// `MouseLButtonDown`: the action `make` makes is painted at the clicked tile.
    fn l_down(&self, c: &Ctx, make: impl FnOnce() -> i64) -> i64 {
        let bounds = c.rect(EE_SCREENS_SH_VIEW_RECT);
        c.call(EE_SCREENS_SH_CAPTURE_MOUSE, 0, 0);
        let (_, m) = c.floats(EE_SCREENS_SH_MOUSE_AT, 0, [0.0; 4]);
        let (found, p) = c.floats(EE_SCREENS_SH_PICK, 0, m);
        self.bound(c, bounds, [bounds[0], bounds[1]]);
        let action = make();
        self.pac.set(action);
        let (tile, x, y) = self.tile_of(c, found != 0, p[0], p[1]);
        // (the original paints a NULL action: the terrain tab without a type; none here)
        if tile && action != 0 {
            let r = c.call(EE_SCREENS_SH_BRUSH_GROUP_SIZE, self.brushes, 0) as u32 as i32;
            self.radius.set(r);
            if Self::area(c, action, x, y, r) {
                c.call(EE_SCREENS_SH_ACTION_POST, action, 0);
            }
        }
        self.last.set((x, y));
        0
    }
    fn l_up(&self, c: &Ctx) -> i64 {
        let w = c.window();
        c.call(EE_SCREENS_SH_RELEASE_MOUSE, 0, 0);
        let pac = self.pac.replace(0);
        if pac != 0 {
            c.call(EE_SCREENS_SH_ACTION_RELEASE, pac, 0);
        }
        self.bound(c, w, [0.0, 0.0]);
        0
    }
    fn mouse_move(&self, c: &Ctx, at: [f32; 2]) -> i64 {
        let pac = self.pac.get();
        if pac == 0 {
            return 0;
        }
        let (found, p) = c.floats(EE_SCREENS_SH_PICK, 0, [at[0], at[1], 0.0, 0.0]);
        let (tile, mut x, mut y) = self.tile_of(c, found != 0, p[0], p[1]);
        let (lx, ly) = self.last.get();
        if tile && (x != lx || y != ly) {
            let mut points = Vec::new();
            Ge2DPoint::new(lx as f32, ly as f32)
                .interpolate_to(Ge2DPoint::new(x as f32, y as f32), 1.0, |q| points.push(q));
            let mut changed = false;
            for q in points.iter().skip(1) {
                x = cvtt(q.x);
                y = cvtt(q.y);
                if Self::area(c, pac, x, y, self.radius.get()) {
                    changed = true;
                }
            }
            if changed {
                c.call(EE_SCREENS_SH_ACTION_POST, pac, 0);
            }
        }
        self.last.set((x, y));
        c.floats(EE_SCREENS_SH_BRUSH_HIGHLIGHT, i64::from(self.centered.get()), [p[0], p[1], 0.0, 0.0]);
        0
    }
    /// `CopyData`: the mouse move again at the mouse, then the base's.
    fn copy_data(&self, c: &Ctx, kind: &dyn Kind) -> i64 {
        let (_, m) = c.floats(EE_SCREENS_SH_MOUSE_AT, 1, [0.0; 4]);
        kind.virt(c, EE_SCREENS_SV_MOUSE_MOVE, 0, 0, &mut [m[0], m[1], 0.0, 0.0]);
        c.base0(EE_SCREENS_SV_COPY_DATA)
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, create: impl FnOnce() -> i64) -> i64 {
        match op {
            EE_SCREENS_SKOP_PAINT_CENTERED => i64::from(self.centered.get()),
            EE_SCREENS_SKOP_PAINT_SET_CENTERED => {
                self.centered.set(a != 0);
                0
            }
            EE_SCREENS_SKOP_PAINT_STORE_TILE => {
                self.store_mouse_tile(c);
                0
            }
            EE_SCREENS_SKOP_PAINT_CREATE => create(),
            _ => 0,
        }
    }
    fn free(&self, c: &Ctx) {
        let pac = self.pac.get();
        if pac != 0 {
            c.call(EE_SCREENS_SH_ACTION_RELEASE, pac, 0);
        }
    }
}

const PAINT_OVERRIDES: u64 = (1 << EE_SCREENS_SV_COPY_DATA)
    | (1 << EE_SCREENS_SV_MOUSE_L_DOWN)
    | (1 << EE_SCREENS_SV_MOUSE_L_UP)
    | (1 << EE_SCREENS_SV_MOUSE_MOVE);

/// `EEUIFSETerrainPaint`: centred; its action the terrain tab's (`editor_terrain.rs`).
struct TerrainPaint {
    paint: Paint,
    /// The terrain tab (`EEUIFSETerrain*`).
    tab: i64,
}

impl TerrainPaint {
    fn create(&self, c: &Ctx) -> i64 {
        #[cfg(not(test))]
        {
            let tab = c.call(EE_SCREENS_SH_TERRAIN_RUST, self.tab, 0);
            // SAFETY: the tab's Rust outlives its paint area (NULL: none)
            unsafe { crate::screens::editor_terrain_ffi::ee_screens_setn_paint(tab as *const _, c.ctl) as i64 }
        }
        #[cfg(test)]
        {
            let _ = c;
            self.tab
        }
    }
}

impl Kind for TerrainPaint {
    fn name(&self) -> &'static CStr {
        c"EEUIFSETerrainPaint"
    }
    fn overrides(&self) -> u64 {
        PAINT_OVERRIDES
    }
    fn init(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_ALWAYS_DIRTY, 1, 0);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_COPY_DATA => self.paint.copy_data(c, self),
            EE_SCREENS_SV_MOUSE_L_DOWN => self.paint.l_down(c, || self.create(c)),
            EE_SCREENS_SV_MOUSE_L_UP => self.paint.l_up(c),
            EE_SCREENS_SV_MOUSE_MOVE => self.paint.mouse_move(c, [f[0], f[1]]),
            _ => c.base(v, a, b, f),
        }
    }
    fn free(&self, c: &Ctx) {
        self.paint.free(c);
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        self.paint.op(c, op, a, || self.create(c))
    }
}

/// `EEUIFSEPaintElevation`: the level from the drag, the Space key or the sliders; the right button drags the
/// grabbed tile's square to the height under the mouse.
struct Elevation {
    paint: Paint,
    /// `mPlane` (the drag's vertical plane through the grabbed tile).
    plane: Cell<Ge3DPlane>,
    /// `mTileX`, `mTileY`.
    tile: Cell<(i32, i32)>,
    /// `mBrushSize` (the brush group's at the right press).
    brush: Cell<i32>,
    /// `mLevel` (the grabbed tile's level).
    level: Cell<i8>,
}

impl Elevation {
    fn checked(c: &Ctx, id: i64) -> bool {
        c.call(EE_SCREENS_SH_FORM_CHECKED, id, 0) != 0
    }
    fn tile_height(&self, c: &Ctx) -> f32 {
        let (x, y) = self.tile.get();
        let mut f = [0.0f32; 4];
        c.host.call(c.ctl, EE_SCREENS_SH_MAP_TILE, i64::from(x), i64::from(y), &mut f);
        f[0]
    }
    fn slider(c: &Ctx, id: i64) -> (i32, i32) {
        let s = c.call(EE_SCREENS_SH_SLIDER_OF, id, 0);
        ((s >> 32) as i32, s as i32)
    }
    /// `CreatePaintAction`.
    fn create(&self, c: &Ctx) -> i64 {
        let space = c.call(EE_SCREENS_SH_SPACE, 0, 0) != 0;
        let mut level: i8 = 0;
        if c.buttons() & RIGHT_DOWN != 0 {
            if Self::checked(c, RAISE_BUTTON) {
                level = floor_char(self.tile_height(c) * RAISE_SCALE + 0.00001);
            } else if Self::checked(c, LOWER_BUTTON) {
                level = floor_char(self.tile_height(c) * LOWER_SCALE + 0.00001);
            }
        } else if space {
            let (_, m) = c.floats(EE_SCREENS_SH_MOUSE_AT, 0, [0.0; 4]);
            let (_, t) = c.floats(EE_SCREENS_SH_PICK, 1, m);
            let (_, half) = c.floats(EE_SCREENS_SH_MAP_HALF, 0, [0.0; 4]);
            level = floor_char(t[2] / (half[0] * 0.5) + 0.00001);
        } else if Self::checked(c, RAISE_BUTTON) {
            let (max, value) = Self::slider(c, ELEVATION_SLIDER);
            level = max.wrapping_sub(value).wrapping_sub(0x18) as i8;
        } else {
            let (max, value) = Self::slider(c, STRENGTH_SLIDER);
            level = cvtt(max.wrapping_sub(value) as f32 - 6.0) as i8;
        }
        let action = c.call(EE_SCREENS_SH_PAINT_NEW, PAINT_ELEVATION, i64::from(level));
        self.paint.centered.set(false);
        action
    }
    fn r_down(&self, c: &Ctx) -> i64 {
        let rect = c.rect(EE_SCREENS_SH_VIEW_RECT);
        let (_, m) = c.floats(EE_SCREENS_SH_MOUSE_AT, 0, [0.0; 4]);
        let (tile, p) = c.floats(EE_SCREENS_SH_PICK, 2, m);
        if tile == 0 {
            return 0;
        }
        c.call(EE_SCREENS_SH_CAPTURE_MOUSE, 0, 0);
        self.paint.bound(c, rect, [rect[0], rect[1]]);
        self.tile.set((cvtt(p[0]), cvtt(p[1])));
        let height = p[3];
        self.level.set(floor_char(height * RAISE_SCALE + 0.00001));
        self.plane.set(Ge3DPlane { point: Ge3DPoint::new(p[0], p[1], height), normal: GeVector::new(1.0, -1.0, 0.0) });
        self.brush.set(c.call(EE_SCREENS_SH_BRUSH_GROUP_SIZE, self.paint.brushes, 0) as u32 as i32);
        0
    }
    fn r_up(&self, c: &Ctx) -> i64 {
        let w = c.window();
        let b = c.buttons();
        if b & HAS_CAPTURE == 0 || b & LEFT_DOWN != 0 {
            return 0;
        }
        c.call(EE_SCREENS_SH_RELEASE_MOUSE, 0, 0);
        self.paint.bound(c, w, [0.0, 0.0]);
        0
    }
    fn mouse_move(&self, c: &Ctx, at: [f32; 2]) -> i64 {
        let b = c.buttons();
        if !(b & HAS_CAPTURE != 0 && b & RIGHT_DOWN != 0 && b & LEFT_DOWN == 0) {
            return self.paint.mouse_move(c, at);
        }
        let mut line = [0.0f32; 6];
        c.floats(EE_SCREENS_SH_SCREEN_LINE, line.as_mut_ptr() as i64, [at[0], at[1], 0.0, 0.0]);
        let line = Ge3DLine {
            start: Ge3DPoint::new(line[0], line[1], line[2]),
            direction: GeVector::new(line[3], line[4], line[5]),
        };
        let plane = self.plane.get();
        // (parallel: the original reads an uninitialized parameter; 0 here)
        let t = line.intersects_plane(&plane).unwrap_or(0.0);
        let point = line.point_at(t);
        let t = f32::from(self.level.get()) * LEVEL_HEIGHT + point.z - plane.point.z;
        let (new, check, old) = if Self::checked(c, RAISE_BUTTON) {
            let new = um::floor(t * RAISE_SCALE + 0.00001) as f32 as i32;
            (new, new, um::floor(self.tile_height(c) * RAISE_SCALE + 0.00001) as f32 as i32)
        } else if Self::checked(c, LOWER_BUTTON) {
            let new = um::floor(t * LOWER_SCALE + 0.00001) as f32 as i32;
            (new, new.wrapping_mul(4), um::floor(self.tile_height(c) * LOWER_SCALE + 0.00001) as f32 as i32)
        } else {
            (0, 0, 0)
        };
        if new != old && (-0x18..=0x28).contains(&check) {
            let action = c.call(EE_SCREENS_SH_PAINT_NEW, PAINT_ELEVATION, i64::from(new as i8));
            let (x, y) = self.tile.get();
            if Paint::area(c, action, x, y, self.brush.get()) {
                c.call(EE_SCREENS_SH_ACTION_POST, action, 0);
            }
            c.call(EE_SCREENS_SH_ACTION_RELEASE, action, 0);
        }
        0
    }
}

impl Kind for Elevation {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEPaintElevation"
    }
    fn overrides(&self) -> u64 {
        PAINT_OVERRIDES | (1 << EE_SCREENS_SV_MOUSE_R_DOWN) | (1 << EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn init(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_ALWAYS_DIRTY, 1, 0);
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_COPY_DATA => self.paint.copy_data(c, self),
            EE_SCREENS_SV_MOUSE_L_DOWN => self.paint.l_down(c, || self.create(c)),
            EE_SCREENS_SV_MOUSE_L_UP => {
                let r = self.paint.l_up(c);
                if c.buttons() & RIGHT_DOWN != 0 {
                    self.r_down(c)
                } else {
                    r
                }
            }
            EE_SCREENS_SV_MOUSE_MOVE => self.mouse_move(c, [f[0], f[1]]),
            EE_SCREENS_SV_MOUSE_R_DOWN => self.r_down(c),
            EE_SCREENS_SV_MOUSE_R_UP => self.r_up(c),
            _ => c.base(v, a, b, f),
        }
    }
    fn free(&self, c: &Ctx) {
        self.paint.free(c);
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        self.paint.op(c, op, a, || self.create(c))
    }
}

#[cfg(test)]
mod tests;
