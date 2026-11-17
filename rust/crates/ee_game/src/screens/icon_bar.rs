//! The main game form's selected-units icon bar (`EEUICUnitIconBar`) whole: its state (the icons' buttons, health
//! values, the Shift-collected selection, the icon rectangle and spacing, and their snapshots for drawing), its
//! refill from the selection, its snapshot, its drawing (each icon with its health bar and a red outline when
//! marked) and its clicks (select, select the type, collect, drop from the selection, unload a cargo unit, scramble
//! an aircraft). Over a host ([`IconBarHost`], `ee_screenscpp_icon_bar` in `User Interface Thread/EEUICUnitIconBar.cpp`)
//! whose ops are each one read of the world or one toolkit, rasterizer or message call; units and buttons cross as
//! opaque handles. The C++ class keeps its bitmaps, sounds and the mouse captures.

// ---- the host's ops (ee_screenscpp_icon_bar(bar, op, a, b, f)): units are handles (0: none), f the op's floats.
/// -> the local player's selected group's unit count (-1: no group).
pub const EE_SCREENS_IB_OP_GROUP_COUNT: u32 = 0;
/// (i) -> the group's unit i.
pub const EE_SCREENS_IB_OP_GROUP_UNIT: u32 = 1;
/// (unit) -> its cargo count.
pub const EE_SCREENS_IB_OP_CARGO_COUNT: u32 = 2;
/// (unit, j) -> its cargo unit j.
pub const EE_SCREENS_IB_OP_CARGO: u32 = 3;
/// (unit) -> its id.
pub const EE_SCREENS_IB_OP_UNIT_ID: u32 = 4;
/// (unit) -> its class's unit type.
pub const EE_SCREENS_IB_OP_UNIT_TYPE: u32 = 5;
/// (unit) -> its class: `EE_SCREENS_IB_CLASS_*` flags | the family << 8.
pub const EE_SCREENS_IB_OP_CLASS: u32 = 6;
/// (unit, k) -> whether its queued goal k is set.
pub const EE_SCREENS_IB_OP_QUEUED_GOAL: u32 = 7;
/// (unit) -> whether its goal queue 1 is empty or has no front.
pub const EE_SCREENS_IB_OP_NO_GOALS: u32 = 8;
/// (unit) -> its tile: x (low 32 bits) | y << 32.
pub const EE_SCREENS_IB_OP_TILE: u32 = 9;
/// (unit) -> its hit points.
pub const EE_SCREENS_IB_OP_HIT_POINTS: u32 = 10;
/// (unit) -> its stats' maximum hit points.
pub const EE_SCREENS_IB_OP_MAX_HIT_POINTS: u32 = 11;
/// (unit) -> its stats' button.
pub const EE_SCREENS_IB_OP_BUTTON: u32 = 12;
/// (ids, count) sends "select these units" (a pointer to `count` i32 ids).
pub const EE_SCREENS_IB_OP_SELECT_THESE: u32 = 13;
/// (id) sends "select the unit".
pub const EE_SCREENS_IB_OP_SELECT: u32 = 14;
/// (transport id, cargo id) sends the immediate cargo unload.
pub const EE_SCREENS_IB_OP_UNLOAD: u32 = 15;
/// (id, tile as [`EE_SCREENS_IB_OP_TILE`]) sends "scramble the aircraft".
pub const EE_SCREENS_IB_OP_SCRAMBLE: u32 = 16;
/// (id) sends "remove the unit from the selection".
pub const EE_SCREENS_IB_OP_REMOVE: u32 = 17;
/// (0 the press sound, 1 the collect sound) plays it.
pub const EE_SCREENS_IB_OP_SOUND: u32 = 18;
/// `SetDirty(true)`.
pub const EE_SCREENS_IB_OP_DIRTY: u32 = 19;
/// -> Ctrl held.
pub const EE_SCREENS_IB_OP_CTRL: u32 = 20;
/// -> Shift held.
pub const EE_SCREENS_IB_OP_SHIFT: u32 = 21;
/// -> f[0] the mouse's x.
pub const EE_SCREENS_IB_OP_MOUSE: u32 = 22;
/// -> whether the bar's rectangle contains the mouse.
pub const EE_SCREENS_IB_OP_CONTAINS: u32 = 23;
/// -> f[0..4] the bar's rectangle (left, top, right, bottom).
pub const EE_SCREENS_IB_OP_RECT: u32 = 24;
/// Releases the captured mouse.
pub const EE_SCREENS_IB_OP_RELEASE_MOUSE: u32 = 25;
/// (button) `Acquire`.
pub const EE_SCREENS_IB_OP_ACQUIRE: u32 = 26;
/// (button) `Release`.
pub const EE_SCREENS_IB_OP_RELEASE: u32 = 27;
/// -> `UIControl::CopyData()`.
pub const EE_SCREENS_IB_OP_COPY_BASE: u32 = 28;
/// -> `UIControl::DrawPolygons()`.
pub const EE_SCREENS_IB_OP_DRAW_BASE: u32 = 29;
/// -> f[0..4] the copied draw rectangle.
pub const EE_SCREENS_IB_OP_DRAW_RECT: u32 = 30;
/// (button) the icon bitmap in the button's texture and face over the rectangle f[2..6], at f[0..2], drawn.
pub const EE_SCREENS_IB_OP_ICON: u32 = 31;
/// (colour: 0 black, 1 red, 2 green) a rhombus over the rectangle f[0..4] drawn.
pub const EE_SCREENS_IB_OP_RHOMBUS: u32 = 32;
/// -> the frame bitmap at f[0..2] drawn: the draw's result.
pub const EE_SCREENS_IB_OP_FRAME: u32 = 33;
/// -> a red line from f[0..2] to f[2..4] drawn: the draw's result.
pub const EE_SCREENS_IB_OP_LINE: u32 = 34;

/// The class flags of [`EE_SCREENS_IB_OP_CLASS`].
pub const EE_SCREENS_IB_CLASS_TRANSPORT: i64 = 1;
pub const EE_SCREENS_IB_CLASS_AIR_BASE: i64 = 2;
pub const EE_SCREENS_IB_CLASS_PARATROOPER_BOMBER: i64 = 4;

/// The families an air base scrambles (`kUnitFamily*`).
const FAMILY_AIRCRAFT: i64 = 5;
const FAMILY_BOMBER: i64 = 0x14;
const FAMILY_ATOMIC_BOMBER: i64 = 0x2a;
const FAMILY_SPACE_FIGHTER: i64 = 0x39;
/// The most icons shown; the "no icon" index.
const MAX_ICONS: usize = 35;
const NO_ICON: i32 = 100;
const SOUND_DOWN: i64 = 0;
const SOUND_UP: i64 = 1;
const RHOMBUS_BLACK: i64 = 0;
const RHOMBUS_RED: i64 = 1;
const RHOMBUS_GREEN: i64 = 2;

/// The bar's calls.
pub trait IconBarHost {
    /// `EE_SCREENS_IB_OP_*` with its arguments and floats; its answer.
    fn op(&mut self, op: u32, a: i64, b: i64, f: &mut [f32]) -> i64;
}

/// A rectangle as the toolkit's (left, top, right, bottom).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Rect {
    pub left: f32,
    pub top: f32,
    pub right: f32,
    pub bottom: f32,
}

impl Rect {
    fn width(&self) -> f32 {
        self.right - self.left
    }
    fn height(&self) -> f32 {
        self.bottom - self.top
    }
    fn floats(&self) -> [f32; 4] {
        [self.left, self.top, self.right, self.bottom]
    }
}

/// The bar's state (the C++ class's members past `UIControl`, but the bitmaps).
#[derive(Debug, Default)]
pub struct IconBar {
    /// One button per icon (the units' or the cargo's).
    pub buttons: Vec<i64>,
    /// `buttons` as of the last snapshot (each acquired).
    pub copied_buttons: Vec<i64>,
    /// The Shift-collected unit ids and their icon indices.
    pub select_ids: Vec<i32>,
    pub select_icons: Vec<i32>,
    pub copied_select_icons: Vec<i32>,
    /// Hit points over the maximum per icon.
    pub health: Vec<f32>,
    pub copied_health: Vec<f32>,
    /// One icon's rectangle (from the origin).
    pub icon: Rect,
    pub copied_icon: Rect,
    /// The step between icons as a fraction of an icon (at most 1).
    pub spacing: f32,
    pub copied_spacing: f32,
    /// The icons are the lone selected transport's cargo.
    pub show_cargo: bool,
    /// Shift is collecting a selection.
    pub selecting: bool,
}

/// The x86 float to `long` conversion (`cvttss2si`): truncation, `i32::MIN` for NaN and out of range.
fn cvtt(value: f32) -> i32 {
    if value.is_nan() || !(-2_147_483_648.0..2_147_483_648.0).contains(&value) {
        i32::MIN
    } else {
        value as i32
    }
}

/// The `min` / `max` macros (`a < b ? a : b`, `a > b ? a : b`).
fn mac_min(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}
fn mac_max(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

fn read_rect<H: IconBarHost + ?Sized>(h: &mut H, op: u32) -> Rect {
    let mut f = [0.0f32; 4];
    h.op(op, 0, 0, &mut f);
    Rect { left: f[0], top: f[1], right: f[2], bottom: f[3] }
}

impl IconBar {
    /// The constructor's state for a bar `width` x `height`: one icon's rectangle, 1/25 of the bar's width less 24.
    pub fn new(width: f32, height: f32) -> IconBar {
        let icon = Rect { left: 0.0, top: 0.0, right: (width - 25.0 + 1.0) * 0.04, bottom: height };
        IconBar { icon, ..IconBar::default() }
    }

    /// The destructor's work: releases the snapshot's buttons.
    pub fn release<H: IconBarHost + ?Sized>(&mut self, h: &mut H) {
        for &b in &self.copied_buttons {
            h.op(EE_SCREENS_IB_OP_RELEASE, b, 0, &mut []);
        }
        self.copied_buttons.clear();
    }

    /// `CopyData`: the snapshot (the old buttons released, the new acquired), then the control's.
    pub fn copy_data<H: IconBarHost + ?Sized>(&mut self, h: &mut H) -> i64 {
        for &b in &self.copied_buttons {
            h.op(EE_SCREENS_IB_OP_RELEASE, b, 0, &mut []);
        }
        self.copied_buttons.clear();
        self.copied_health.clear();
        self.copied_select_icons.clear();
        for &b in &self.buttons {
            self.copied_buttons.push(b);
            h.op(EE_SCREENS_IB_OP_ACQUIRE, b, 0, &mut []);
        }
        self.copied_health.extend_from_slice(&self.health);
        self.copied_select_icons.extend_from_slice(&self.select_icons);
        self.copied_spacing = self.spacing;
        self.copied_icon = self.icon;
        h.op(EE_SCREENS_IB_OP_COPY_BASE, 0, 0, &mut [])
    }

    /// `DrawPolygons`: up to 35 icons left to right from the copied rectangle's corner, each with its black, red
    /// and green (health) bar above, the frame, and a red outline when marked; the result of the last frame or
    /// outline drawn (the control's when none).
    pub fn draw<H: IconBarHost + ?Sized>(&self, h: &mut H) -> i64 {
        let mut result = h.op(EE_SCREENS_IB_OP_DRAW_BASE, 0, 0, &mut []);
        let draw = read_rect(h, EE_SCREENS_IB_OP_DRAW_RECT);
        let offset = draw.height() * 0.1;
        let step = self.copied_icon.width() * self.copied_spacing;
        let (mut x, y) = (draw.left, draw.top);
        let mut icon_y = draw.height() * 0.18 + y + offset;
        let icon_top = icon_y;
        let icon = Rect {
            left: 0.0,
            top: icon_top,
            right: self.copied_icon.width(),
            bottom: self.copied_icon.height() + icon_top - (draw.height() * 0.18 + offset),
        };
        let black_top = y;
        let black_bottom = draw.height() * 0.18 + black_top + offset;
        let bar_top = draw.height() * 0.09 + y;
        let bar_bottom = bar_top + offset;
        let width = self.copied_icon.width();
        let height = self.copied_icon.height();
        for i in 0..self.copied_buttons.len().min(MAX_ICONS) {
            let black = Rect { left: x, top: black_top, right: width + x, bottom: black_bottom };
            let left = width * 0.1 + x;
            let green =
                Rect { left, top: bar_top, right: width * 0.8 * self.copied_health[i] + left, bottom: bar_bottom };
            let red = Rect { left: green.right, top: bar_top, right: width * 0.9 + x, bottom: bar_bottom };
            let r = icon.floats();
            h.op(EE_SCREENS_IB_OP_ICON, self.copied_buttons[i], 0, &mut [x, icon_y, r[0], r[1], r[2], r[3]]);
            h.op(EE_SCREENS_IB_OP_RHOMBUS, RHOMBUS_BLACK, 0, &mut black.floats());
            h.op(EE_SCREENS_IB_OP_RHOMBUS, RHOMBUS_RED, 0, &mut red.floats());
            h.op(EE_SCREENS_IB_OP_RHOMBUS, RHOMBUS_GREEN, 0, &mut green.floats());
            result = h.op(EE_SCREENS_IB_OP_FRAME, 0, 0, &mut [x, y]);
            if self.copied_select_icons.iter().any(|&s| s as u32 == i as u32) {
                let (right, bottom) = (width + x, height + y);
                h.op(EE_SCREENS_IB_OP_LINE, 0, 0, &mut [x, y, right, y]);
                h.op(EE_SCREENS_IB_OP_LINE, 0, 0, &mut [x, y, x, bottom]);
                h.op(EE_SCREENS_IB_OP_LINE, 0, 0, &mut [right, bottom, x, bottom]);
                result = h.op(EE_SCREENS_IB_OP_LINE, 0, 0, &mut [right, bottom, right, y]);
            }
            x = x + step + 1.0;
            icon_y = draw.height() * 0.18 + y + offset;
        }
        result
    }

    /// `GetUnit`: icon `index`'s unit (a cargo unit of the selected transport, or a selected unit); 0 for none.
    fn unit<H: IconBarHost + ?Sized>(&self, h: &mut H, index: u32) -> i64 {
        if index == NO_ICON as u32 {
            return 0;
        }
        let count = h.op(EE_SCREENS_IB_OP_GROUP_COUNT, 0, 0, &mut []);
        if count <= 0 {
            return 0;
        }
        let first = h.op(EE_SCREENS_IB_OP_GROUP_UNIT, 0, 0, &mut []);
        if self.show_cargo && (index as i64) < h.op(EE_SCREENS_IB_OP_CARGO_COUNT, first, 0, &mut []) {
            h.op(EE_SCREENS_IB_OP_CARGO, first, index as i64, &mut [])
        } else if (index as i64) < count {
            h.op(EE_SCREENS_IB_OP_GROUP_UNIT, index as i64, 0, &mut [])
        } else {
            0
        }
    }

    /// `GetIconIndex`: the icon under the mouse (its x from the bar's left over the icon pitch), 100 when empty.
    fn icon_index<H: IconBarHost + ?Sized>(&self, h: &mut H) -> i32 {
        if self.buttons.is_empty() {
            return NO_ICON;
        }
        let mut m = [0.0f32; 1];
        h.op(EE_SCREENS_IB_OP_MOUSE, 0, 0, &mut m);
        let rect = read_rect(h, EE_SCREENS_IB_OP_RECT);
        cvtt((m[0] - rect.left) / (self.icon.width() * self.spacing + 1.0))
    }

    /// `GetUnitIDs`: the ids of the listed units of `unit_type`.
    fn unit_ids<H: IconBarHost + ?Sized>(&self, h: &mut H, unit_type: i64, ids: &mut Vec<i32>) {
        for i in 0..self.buttons.len() {
            let unit = self.unit(h, i as u32);
            if unit != 0 && h.op(EE_SCREENS_IB_OP_UNIT_TYPE, unit, 0, &mut []) == unit_type {
                ids.push(h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []) as i32);
            }
        }
    }

    /// `MouseLButtonUp`: on an icon, selects its unit (Ctrl: all of its type; Shift or collecting: marks it), or
    /// unloads the cargo unit, or scrambles the idle aircraft from the air base; the mouse released.
    pub fn mouse_l_up<H: IconBarHost + ?Sized>(&mut self, h: &mut H) -> i64 {
        if h.op(EE_SCREENS_IB_OP_CONTAINS, 0, 0, &mut []) != 0 {
            let index = self.icon_index(h);
            let unit = self.unit(h, index as u32);
            if unit != 0 {
                let first = h.op(EE_SCREENS_IB_OP_GROUP_UNIT, 0, 0, &mut []);
                if !self.show_cargo {
                    if h.op(EE_SCREENS_IB_OP_CTRL, 0, 0, &mut []) != 0 {
                        let mut ids = Vec::new();
                        let unit_type = h.op(EE_SCREENS_IB_OP_UNIT_TYPE, unit, 0, &mut []);
                        self.unit_ids(h, unit_type, &mut ids);
                        h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
                        h.op(EE_SCREENS_IB_OP_SELECT_THESE, ids.as_ptr() as i64, ids.len() as i64, &mut []);
                    } else if !self.selecting && h.op(EE_SCREENS_IB_OP_SHIFT, 0, 0, &mut []) == 0 {
                        let id = h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []);
                        h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
                        h.op(EE_SCREENS_IB_OP_SELECT, id, 0, &mut []);
                    } else {
                        self.selecting = true;
                        let id = h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []);
                        self.select_ids.push(id as i32);
                        self.select_icons.push(index);
                        h.op(EE_SCREENS_IB_OP_SOUND, SOUND_UP, 0, &mut []);
                        h.op(EE_SCREENS_IB_OP_DIRTY, 0, 0, &mut []);
                    }
                } else {
                    let class = h.op(EE_SCREENS_IB_OP_CLASS, first, 0, &mut []);
                    if class & EE_SCREENS_IB_CLASS_TRANSPORT != 0 {
                        let (transport, cargo) = (
                            h.op(EE_SCREENS_IB_OP_UNIT_ID, first, 0, &mut []),
                            h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []),
                        );
                        h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
                        h.op(EE_SCREENS_IB_OP_UNLOAD, transport, cargo, &mut []);
                    } else if class & EE_SCREENS_IB_CLASS_AIR_BASE != 0 {
                        let class = h.op(EE_SCREENS_IB_OP_CLASS, unit, 0, &mut []);
                        let family = class >> 8;
                        let goal = if family == FAMILY_AIRCRAFT || family == FAMILY_SPACE_FIGHTER {
                            h.op(EE_SCREENS_IB_OP_QUEUED_GOAL, first, 1, &mut []) != 0
                        } else if family == FAMILY_BOMBER {
                            class & EE_SCREENS_IB_CLASS_PARATROOPER_BOMBER == 0
                                && h.op(EE_SCREENS_IB_OP_QUEUED_GOAL, first, 2, &mut []) != 0
                        } else if family == FAMILY_ATOMIC_BOMBER {
                            h.op(EE_SCREENS_IB_OP_QUEUED_GOAL, first, 3, &mut []) != 0
                        } else {
                            false
                        };
                        if !goal && h.op(EE_SCREENS_IB_OP_NO_GOALS, unit, 0, &mut []) != 0 {
                            let id = h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []);
                            let tile = h.op(EE_SCREENS_IB_OP_TILE, first, 0, &mut []);
                            h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
                            h.op(EE_SCREENS_IB_OP_SCRAMBLE, id, tile, &mut []);
                        }
                    }
                }
            }
        }
        h.op(EE_SCREENS_IB_OP_RELEASE_MOUSE, 0, 0, &mut []);
        0
    }

    /// `MouseRButtonUp`: on an icon, drops its unit from the selection, or unloads the cargo unit; the mouse
    /// released.
    pub fn mouse_r_up<H: IconBarHost + ?Sized>(&mut self, h: &mut H) -> i64 {
        if h.op(EE_SCREENS_IB_OP_CONTAINS, 0, 0, &mut []) != 0 {
            let index = self.icon_index(h);
            let unit = self.unit(h, index as u32);
            if unit != 0 && !self.show_cargo {
                let id = h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []);
                h.op(EE_SCREENS_IB_OP_REMOVE, id, 0, &mut []);
                h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
            } else if self.show_cargo && unit != 0 {
                let transport = h.op(EE_SCREENS_IB_OP_GROUP_UNIT, 0, 0, &mut []);
                if transport != 0
                    && h.op(EE_SCREENS_IB_OP_CLASS, transport, 0, &mut []) & EE_SCREENS_IB_CLASS_TRANSPORT != 0
                {
                    let (t, c) = (
                        h.op(EE_SCREENS_IB_OP_UNIT_ID, transport, 0, &mut []),
                        h.op(EE_SCREENS_IB_OP_UNIT_ID, unit, 0, &mut []),
                    );
                    h.op(EE_SCREENS_IB_OP_UNLOAD, t, c, &mut []);
                    h.op(EE_SCREENS_IB_OP_SOUND, SOUND_DOWN, 0, &mut []);
                }
            }
        }
        h.op(EE_SCREENS_IB_OP_RELEASE_MOUSE, 0, 0, &mut []);
        0
    }

    /// `MouseMove`: while collecting, sends the marked units as the selection once Shift is up.
    pub fn mouse_move<H: IconBarHost + ?Sized>(&mut self, h: &mut H) -> i64 {
        if !self.selecting {
            return 0;
        }
        self.selecting = h.op(EE_SCREENS_IB_OP_SHIFT, 0, 0, &mut []) != 0;
        if self.selecting {
            return 0;
        }
        h.op(EE_SCREENS_IB_OP_SELECT_THESE, self.select_ids.as_ptr() as i64, self.select_ids.len() as i64, &mut []);
        self.select_ids.clear();
        self.select_icons.clear();
        0
    }

    /// `Update`: the icons from the selection (or the lone selected transport's cargo), their health, the spacing
    /// that fits up to 35 in the bar; the collected selection dropped; dirty.
    pub fn update<H: IconBarHost + ?Sized>(&mut self, h: &mut H) {
        // (the original reads a missing group; it never is: the player always has one)
        let count = h.op(EE_SCREENS_IB_OP_GROUP_COUNT, 0, 0, &mut []).max(0);
        let first = if count > 0 { h.op(EE_SCREENS_IB_OP_GROUP_UNIT, 0, 0, &mut []) } else { 0 };
        let cargo = if count == 1 && first != 0 { h.op(EE_SCREENS_IB_OP_CARGO_COUNT, first, 0, &mut []) } else { 0 };
        let shown = if count == 1 && first != 0 && cargo != 0 {
            cargo.min(MAX_ICONS as i64)
        } else {
            count.min(MAX_ICONS as i64)
        };
        let width = read_rect(h, EE_SCREENS_IB_OP_RECT).width();
        let icon = self.icon.width();
        self.spacing = mac_min(1.0, ((width - icon - 25.0 + 1.0) / icon - 1.0) / (shown as u32 as f32 - 1.0));
        self.buttons.clear();
        self.health.clear();
        self.select_ids.clear();
        self.select_icons.clear();
        self.show_cargo = false;
        for i in 0..count {
            let unit = h.op(EE_SCREENS_IB_OP_GROUP_UNIT, i, 0, &mut []);
            let cargo = if count == 1 { h.op(EE_SCREENS_IB_OP_CARGO_COUNT, unit, 0, &mut []) } else { 0 };
            if count == 1 && cargo != 0 {
                for j in 0..cargo {
                    let c = h.op(EE_SCREENS_IB_OP_CARGO, unit, j, &mut []);
                    self.buttons.push(h.op(EE_SCREENS_IB_OP_BUTTON, c, 0, &mut []));
                    let health = h.op(EE_SCREENS_IB_OP_HIT_POINTS, c, 0, &mut []) as i32 as f32
                        / h.op(EE_SCREENS_IB_OP_MAX_HIT_POINTS, c, 0, &mut []) as i32 as f32;
                    self.health.push(mac_max(mac_min(health, 1.0), 0.0));
                    self.show_cargo = true;
                }
            } else {
                self.buttons.push(h.op(EE_SCREENS_IB_OP_BUTTON, unit, 0, &mut []));
                self.health.push(
                    h.op(EE_SCREENS_IB_OP_HIT_POINTS, unit, 0, &mut []) as i32 as f32
                        / h.op(EE_SCREENS_IB_OP_MAX_HIT_POINTS, unit, 0, &mut []) as i32 as f32,
                );
            }
        }
        h.op(EE_SCREENS_IB_OP_DIRTY, 0, 0, &mut []);
    }
}

/// The op as the shared control host's (`EEUIRustControl`, `ee_screenscpp_hud_ctl`): the toolkit's calls it answers
/// itself (dirty, the rectangles, the base's draws, the mouse's release) as its ops, the others as the class's own
/// (`EE_SCREENS_HC_CLASS + op`, `RustOp`); with its a.
pub fn hud_op(op: u32, a: i64) -> (u32, i64) {
    use crate::screens::hud_ctl::{
        EE_SCREENS_HC_CLASS, EE_SCREENS_HC_COPIED_RECT, EE_SCREENS_HC_COPY_BASE, EE_SCREENS_HC_DRAW_BASE,
        EE_SCREENS_HC_RECT, EE_SCREENS_HC_RELEASE_MOUSE, EE_SCREENS_HC_SET_DIRTY,
    };
    match op {
        EE_SCREENS_IB_OP_DIRTY => (EE_SCREENS_HC_SET_DIRTY, 1),
        EE_SCREENS_IB_OP_RECT => (EE_SCREENS_HC_RECT, a),
        EE_SCREENS_IB_OP_DRAW_RECT => (EE_SCREENS_HC_COPIED_RECT, a),
        EE_SCREENS_IB_OP_COPY_BASE => (EE_SCREENS_HC_COPY_BASE, a),
        EE_SCREENS_IB_OP_DRAW_BASE => (EE_SCREENS_HC_DRAW_BASE, a),
        EE_SCREENS_IB_OP_RELEASE_MOUSE => (EE_SCREENS_HC_RELEASE_MOUSE, a),
        _ => (EE_SCREENS_HC_CLASS + op, a),
    }
}

#[cfg(test)]
mod hud_op_tests {
    use super::*;
    use crate::screens::hud_ctl::*;

    #[test]
    fn the_shared_ops_are_the_hosts_and_the_others_the_classs() {
        assert_eq!(hud_op(EE_SCREENS_IB_OP_DIRTY, 0), (EE_SCREENS_HC_SET_DIRTY, 1));
        assert_eq!(hud_op(EE_SCREENS_IB_OP_RECT, 0), (EE_SCREENS_HC_RECT, 0));
        assert_eq!(hud_op(EE_SCREENS_IB_OP_DRAW_RECT, 0), (EE_SCREENS_HC_COPIED_RECT, 0));
        assert_eq!(hud_op(EE_SCREENS_IB_OP_COPY_BASE, 0), (EE_SCREENS_HC_COPY_BASE, 0));
        assert_eq!(hud_op(EE_SCREENS_IB_OP_DRAW_BASE, 0), (EE_SCREENS_HC_DRAW_BASE, 0));
        assert_eq!(hud_op(EE_SCREENS_IB_OP_RELEASE_MOUSE, 0), (EE_SCREENS_HC_RELEASE_MOUSE, 0));
        let shared = [
            EE_SCREENS_IB_OP_DIRTY,
            EE_SCREENS_IB_OP_RECT,
            EE_SCREENS_IB_OP_DRAW_RECT,
            EE_SCREENS_IB_OP_COPY_BASE,
            EE_SCREENS_IB_OP_DRAW_BASE,
            EE_SCREENS_IB_OP_RELEASE_MOUSE,
        ];
        for op in (0..=34).filter(|op| !shared.contains(op)) {
            assert_eq!(hud_op(op, -3), (EE_SCREENS_HC_CLASS + op, -3));
        }
    }
}

#[cfg(test)]
mod golden;
