//! The main group's status box (`EEUIFMGStatusBox`, `User Interface Thread/EEUIFMGStatusBox.cpp`): `Fill` (a new
//! selection), `Refresh` (each update of it), `FillArmorAttributes`, `UpdateArmorTexts`, `UpdateQueuedCounts`,
//! `HideBackgrounds` and `LoadIcons` (the 50 icon cells of the three unit improvement textures).
//!
//! The numbers are [`crate::hud`]'s; here is the order of every call on the box's controls. The C++ class keeps its
//! members and controls; every read and call is one host op ([`BoxHost`], `EE_SCREENS_BOX_OP_*`) on a control
//! (`EE_SCREENS_BOX_CTL_*`: the nine icons, their nine labels, then the named controls), a member
//! (`EE_SCREENS_BOX_FIELD_*`) or the host's one held text (the original's `theText`), asked where and as often as
//! the original asked it.

use core::ffi::c_long;

use crate::screens::hud::{self, EeScreensHudIcon, EeScreensHudUnit};

/// The host: one op with up to three arguments and its answer.
pub trait BoxHost {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64;
    /// `EEStatusBoxView` of the call's unit (`fill`: whether the owner's friendliness is read).
    fn view(&mut self, fill: bool) -> EeScreensHudUnit;
}

/// The icons `mIcons[0..9]` are controls 0..8.
pub const EE_SCREENS_BOX_CTL_ICON: i64 = 0;
/// The labels `mLabels[0..9]` are controls 9..17.
pub const EE_SCREENS_BOX_CTL_LABEL: i64 = 9;
/// `mC84`: the hit points bar (control 0x1e).
pub const EE_SCREENS_BOX_CTL_LEFT_BAR: i64 = 18;
/// `mC88`: the second bar (control 0x1f).
pub const EE_SCREENS_BOX_CTL_MIDDLE_BAR: i64 = 19;
/// `mC8C`: the portrait.
pub const EE_SCREENS_BOX_CTL_PORTRAIT: i64 = 20;
/// `mC90`: the unit's name.
pub const EE_SCREENS_BOX_CTL_NAME: i64 = 21;
/// `mC94`: the civilization's name (absent in the scenario editor).
pub const EE_SCREENS_BOX_CTL_CIV: i64 = 22;
/// `mC98`: the armor title.
pub const EE_SCREENS_BOX_CTL_ARMOR_TITLE: i64 = 23;
/// `mCA8`: the third bar (control 0x33; absent in the scenario editor).
pub const EE_SCREENS_BOX_CTL_THIRD_BAR: i64 = 24;
/// `mCAC`: the fourth bar (control 0x32; absent in the scenario editor).
pub const EE_SCREENS_BOX_CTL_FOURTH_BAR: i64 = 25;

/// `mAttributeCount`.
pub const EE_SCREENS_BOX_FIELD_COUNT: i64 = 0;
/// `mArmorAttributeCount`.
pub const EE_SCREENS_BOX_FIELD_ARMOR_COUNT: i64 = 1;
/// `mIsResourceShown`.
pub const EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN: i64 = 2;
/// `mAttributes[i]` is field 16 + i.
pub const EE_SCREENS_BOX_FIELD_ATTRIBUTES: i64 = 16;

/// The member a (`EE_SCREENS_BOX_FIELD_*`).
pub const EE_SCREENS_BOX_OP_GET: u32 = 1;
/// The member a = b.
pub const EE_SCREENS_BOX_OP_SET: u32 = 2;
/// Whether control a is set.
pub const EE_SCREENS_BOX_OP_PRESENT: u32 = 3;
/// Control a's `GetDisplayMode()`.
pub const EE_SCREENS_BOX_OP_MODE: u32 = 4;
/// Control a's `SetDisplayMode(b)`.
pub const EE_SCREENS_BOX_OP_SET_MODE: u32 = 5;
/// Button a's queued count b: `SetDirty(true)` when it changes, then `mNumQueued = b`.
pub const EE_SCREENS_BOX_OP_QUEUED: u32 = 6;
/// Button a's `mAttribute`.
pub const EE_SCREENS_BOX_OP_ATTRIBUTE: u32 = 7;
/// Button a's `mAttribute = b`.
pub const EE_SCREENS_BOX_OP_SET_ATTRIBUTE: u32 = 8;
/// Button a's `mCostLabel` = control b (-1: NULL).
pub const EE_SCREENS_BOX_OP_COST_LABEL: u32 = 9;
/// The held text = the unit's name with the gate's lock state (a = 0: `Fill`'s test of the stats' name, 1:
/// `Refresh`'s).
pub const EE_SCREENS_BOX_OP_NAME: u32 = 10;
/// The held text = the unit's owner's civilization's name.
pub const EE_SCREENS_BOX_OP_CIV: u32 = 11;
/// Label a's `SetText(held)`.
pub const EE_SCREENS_BOX_OP_SET_TEXT: u32 = 12;
/// Whether label a's first line differs from the held text.
pub const EE_SCREENS_BOX_OP_TEXT_DIFFERS: u32 = 13;
/// Bar a's `SetIconBitmap` with icon bitmap b.
pub const EE_SCREENS_BOX_OP_BAR_ICON: u32 = 14;
/// Bar a's `mBar->SetRange(0, b)`.
pub const EE_SCREENS_BOX_OP_BAR_RANGE: u32 = 15;
/// Bar a's `mBar->SetCurrentRangeValue(b)`.
pub const EE_SCREENS_BOX_OP_BAR_VALUE: u32 = 16;
/// Control a's `SetDirty(true)`.
pub const EE_SCREENS_BOX_OP_DIRTY: u32 = 17;
/// Bar a's `mBar->SetStatusBarColor` of the box's colour b (1 blue, 2 yellow, 3 green).
pub const EE_SCREENS_BOX_OP_BAR_COLOR: u32 = 18;
/// Bar a's `mEnabled = b`.
pub const EE_SCREENS_BOX_OP_BAR_ENABLED: u32 = 19;
/// Control a's `SetStringTableID(b)`.
pub const EE_SCREENS_BOX_OP_STRING_ID: u32 = 20;
/// Icon bitmap a's `ComputeStretchedCoordsAndScale` over the icon rectangle of attribute b, on control c's screen.
pub const EE_SCREENS_BOX_OP_STRETCH: u32 = 21;
/// Button a's normal (b = 0) or pressed (b = 1) bitmap = icon bitmap c.
pub const EE_SCREENS_BOX_OP_SET_BITMAP: u32 = 22;
/// Whether button a's normal (b = 0) or pressed (b = 1) bitmap is icon bitmap c.
pub const EE_SCREENS_BOX_OP_BITMAP_IS: u32 = 23;
/// The portrait's `SetUnit` (a = 1: the call's unit, 0: NULL).
pub const EE_SCREENS_BOX_OP_PORTRAIT: u32 = 24;
/// The portrait's `Update()`.
pub const EE_SCREENS_BOX_OP_PORTRAIT_UPDATE: u32 = 25;
/// The original's `ESVerify` at line a (367: the armor count, 708: the attribute count).
pub const EE_SCREENS_BOX_OP_VERIFY: u32 = 26;
/// `gWorld.mInCinematic`.
pub const EE_SCREENS_BOX_OP_IN_CINEMATIC: u32 = 27;
/// The main game form's background a made visible (b != 0) or hidden, when it has one.
pub const EE_SCREENS_BOX_OP_BACKGROUND: u32 = 28;
/// The held text = format a (`EE_SCREENS_BOX_FMT_*`) of b and c.
pub const EE_SCREENS_BOX_OP_FORMAT: u32 = 29;
/// Whether bar a's value label b (0 the first, 1 the second) differs from the held text.
pub const EE_SCREENS_BOX_OP_VALUE_DIFFERS: u32 = 30;
/// Bar a's value label b's `SetText(held)`.
pub const EE_SCREENS_BOX_OP_SET_VALUE: u32 = 31;
/// Button a's `UpdateCost(held)`.
pub const EE_SCREENS_BOX_OP_UPDATE_COST: u32 = 32;
/// Armor kind a of the unit's stats (b = 0, f32 bits), its class (1) or the stats' class (2).
pub const EE_SCREENS_BOX_OP_ARMOR: u32 = 33;
/// The complex unit's class's `mUnitType`.
pub const EE_SCREENS_BOX_OP_UNIT_TYPE: u32 = 34;
/// The complex unit's player's `CountQueuedImprovements(a, b)`.
pub const EE_SCREENS_BOX_OP_COUNT_QUEUED: u32 = 35;
/// `sIconBitmaps[a].mBitmap = new GEBitmap`.
pub const EE_SCREENS_BOX_OP_NEW_BITMAP: u32 = 36;
/// `LoadIcons`' texture description (the pixel depth, the primary surface's width).
pub const EE_SCREENS_BOX_OP_DESCRIPTION: u32 = 37;
/// Texture a (1..3) loaded with the description, its seven stage modes set.
pub const EE_SCREENS_BOX_OP_LOAD_TEXTURE: u32 = 38;
/// `sIconBitmaps[a].mRectangle` from the corners b and c (two f32s each, x low).
pub const EE_SCREENS_BOX_OP_SET_RECT: u32 = 39;
/// Texture a's surface's width (high 32 bits) and height (low).
pub const EE_SCREENS_BOX_OP_TEXTURE_SIZE: u32 = 40;
/// `sIconBitmaps[a & 0xff]`'s `SetTexture(texture a >> 8, gScreenIndex)` and `SetTextureCoordinates` of the top
/// left b and bottom right c (u low, v high).
pub const EE_SCREENS_BOX_OP_BITMAP_TEXTURE: u32 = 41;
/// `sIconBitmaps[a]`'s `SetXYScale` (b: x low, y high).
pub const EE_SCREENS_BOX_OP_BITMAP_SCALE: u32 = 42;
/// `sIconsLoaded = true`.
pub const EE_SCREENS_BOX_OP_ICONS_LOADED: u32 = 43;

/// "%ld/%ld".
pub const EE_SCREENS_BOX_FMT_PAIR: i64 = 0;
/// "+ %ld".
pub const EE_SCREENS_BOX_FMT_BONUS: i64 = 1;
/// "%ld+%ld".
pub const EE_SCREENS_BOX_FMT_SUM: i64 = 2;
/// "%ld".
pub const EE_SCREENS_BOX_FMT_ONE: i64 = 3;
/// "".
pub const EE_SCREENS_BOX_FMT_EMPTY: i64 = 4;

const BOX_HIDDEN: i64 = 0;
const BOX_SHOWN: i64 = 1;
const LEFT: i64 = EE_SCREENS_BOX_CTL_LEFT_BAR;
const MIDDLE: i64 = EE_SCREENS_BOX_CTL_MIDDLE_BAR;
const THIRD: i64 = EE_SCREENS_BOX_CTL_THIRD_BAR;
const FOURTH: i64 = EE_SCREENS_BOX_CTL_FOURTH_BAR;
const PORTRAIT: i64 = EE_SCREENS_BOX_CTL_PORTRAIT;
const NAME: i64 = EE_SCREENS_BOX_CTL_NAME;
const CIV: i64 = EE_SCREENS_BOX_CTL_CIV;
const BARS: [i64; 3] = [MIDDLE, THIRD, FOURTH];
const fn icon(i: i64) -> i64 {
    EE_SCREENS_BOX_CTL_ICON + i
}
const fn label(i: i64) -> i64 {
    EE_SCREENS_BOX_CTL_LABEL + i
}

// The improvements (`ee/EEImprovementIDs.h`) and string ids (`ee/EEStringIDs.h`) the box sets.
/// `kImprovementFlightTimeOrPower`.
pub const EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER: i64 = 8;
/// `kImprovementHitPoints`.
pub const EE_SCREENS_BOX_IMPROVEMENT_HIT_POINTS: i64 = 9;
/// `kImprovementTransport`.
pub const EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT: i64 = 15;
/// `IDS_IMPROVEMENT_GARRISON`.
pub const EE_SCREENS_BOX_IDS_GARRISON: i64 = 30023;

/// The background panels `Fill` and `HideBackgrounds` show or hide (the last, 124, is always shown).
const BACKGROUNDS: [i64; 5] = [100, 111, 112, 113, 124];

struct H<'a, T: BoxHost + ?Sized>(&'a mut T);

impl<T: BoxHost + ?Sized> H<'_, T> {
    fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
        self.0.op(op, a, b, c)
    }
    fn get(&mut self, field: i64) -> i64 {
        self.op(EE_SCREENS_BOX_OP_GET, field, 0, 0)
    }
    fn set(&mut self, field: i64, v: i64) {
        self.op(EE_SCREENS_BOX_OP_SET, field, v, 0);
    }
    fn present(&mut self, ctl: i64) -> bool {
        self.op(EE_SCREENS_BOX_OP_PRESENT, ctl, 0, 0) != 0
    }
    fn mode(&mut self, ctl: i64, mode: i64) {
        self.op(EE_SCREENS_BOX_OP_SET_MODE, ctl, mode, 0);
    }
    fn hidden(&mut self, ctl: i64) -> bool {
        self.op(EE_SCREENS_BOX_OP_MODE, ctl, 0, 0) == BOX_HIDDEN
    }
    fn queued(&mut self, ctl: i64, n: i64) {
        self.op(EE_SCREENS_BOX_OP_QUEUED, ctl, n, 0);
    }
    fn a(&mut self, op: u32, a: i64) -> i64 {
        self.op(op, a, 0, 0)
    }
    fn ab(&mut self, op: u32, a: i64, b: i64) -> i64 {
        self.op(op, a, b, 0)
    }
    fn format(&mut self, fmt: i64, a: c_long, b: c_long) {
        self.op(EE_SCREENS_BOX_OP_FORMAT, fmt, a as i64, b as i64);
    }
    fn cost(&mut self, ctl: i64) {
        self.a(EE_SCREENS_BOX_OP_UPDATE_COST, ctl);
    }
    /// The name label (and the civilization's, when the box has it): set (`Fill`) or set when changed
    /// (`Refresh`).
    fn names(&mut self, refresh: bool) {
        self.a(EE_SCREENS_BOX_OP_NAME, refresh as i64);
        if !refresh {
            self.a(EE_SCREENS_BOX_OP_SET_TEXT, NAME);
            self.mode(NAME, BOX_SHOWN);
        } else if self.a(EE_SCREENS_BOX_OP_TEXT_DIFFERS, NAME) != 0 {
            self.a(EE_SCREENS_BOX_OP_SET_TEXT, NAME);
        }
        if self.present(CIV) {
            self.a(EE_SCREENS_BOX_OP_CIV, 0);
            if !refresh {
                self.a(EE_SCREENS_BOX_OP_SET_TEXT, CIV);
                self.mode(CIV, BOX_SHOWN);
            } else if self.a(EE_SCREENS_BOX_OP_TEXT_DIFFERS, CIV) != 0 {
                self.a(EE_SCREENS_BOX_OP_SET_TEXT, CIV);
            }
        }
    }
    /// One bar's two value labels as `Refresh` updates them: "%ld/%ld" (`first`) and the second text (`second`),
    /// each set with the bar's value (and range) only when changed, then the second text as the bar's cost.
    fn bar_texts(&mut self, bar: i64, first: [c_long; 2], second: Option<c_long>, max: c_long, current: c_long) {
        self.format(EE_SCREENS_BOX_FMT_PAIR, first[0], first[1]);
        if self.ab(EE_SCREENS_BOX_OP_VALUE_DIFFERS, bar, 0) != 0 {
            self.ab(EE_SCREENS_BOX_OP_SET_VALUE, bar, 0);
            self.ab(EE_SCREENS_BOX_OP_BAR_VALUE, bar, current as i64);
        }
        match second {
            Some(bonus) => self.format(EE_SCREENS_BOX_FMT_BONUS, bonus, 0),
            None => self.format(EE_SCREENS_BOX_FMT_EMPTY, 0, 0),
        }
        if self.ab(EE_SCREENS_BOX_OP_VALUE_DIFFERS, bar, 1) != 0 {
            self.ab(EE_SCREENS_BOX_OP_SET_VALUE, bar, 1);
            self.ab(EE_SCREENS_BOX_OP_BAR_RANGE, bar, max as i64);
            self.ab(EE_SCREENS_BOX_OP_BAR_VALUE, bar, current as i64);
        }
        self.cost(bar);
    }
    /// A bar's icon, colour, range and value as `Fill` sets them up.
    fn bar_setup(&mut self, bar: i64, bitmap: i64, color: Option<i64>, max: c_long, current: c_long) {
        self.ab(EE_SCREENS_BOX_OP_BAR_ICON, bar, bitmap);
        if let Some(c) = color {
            self.ab(EE_SCREENS_BOX_OP_BAR_COLOR, bar, c);
        }
        self.ab(EE_SCREENS_BOX_OP_BAR_RANGE, bar, max as i64);
        self.ab(EE_SCREENS_BOX_OP_BAR_VALUE, bar, current as i64);
    }
}

/// `EEStatusBoxShowIcon`: one attribute icon of a `Fill` plan.
fn show_icon<T: BoxHost + ?Sized>(h: &mut H<T>, i: &EeScreensHudIcon) {
    h.set(EE_SCREENS_BOX_FIELD_ATTRIBUTES + i.slot as i64, i.icon as i64);
    if i.icon == -1 {
        return;
    }
    let ctl = icon(i.icon as i64);
    let rect = icon(i.rect_icon as i64);
    let bitmap = i.bitmap as i64;
    h.op(EE_SCREENS_BOX_OP_STRETCH, bitmap, rect, ctl);
    h.op(EE_SCREENS_BOX_OP_STRETCH, bitmap + 1, rect, ctl);
    h.op(EE_SCREENS_BOX_OP_SET_BITMAP, ctl, 0, bitmap);
    h.op(EE_SCREENS_BOX_OP_SET_BITMAP, ctl, 1, bitmap + 1);
    h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, ctl, i.improvement as i64);
    h.queued(ctl, 0);
    h.mode(ctl, BOX_SHOWN);
    h.ab(EE_SCREENS_BOX_OP_COST_LABEL, ctl, label(i.icon as i64));
}

/// `EEUIFMGStatusBox::FillArmorAttributes` (`unit`: the call's unit is set).
pub fn fill_armor_attributes<T: BoxHost + ?Sized>(host: &mut T, unit: bool) {
    let mut h = H(host);
    let count = h.get(EE_SCREENS_BOX_FIELD_COUNT);
    h.set(EE_SCREENS_BOX_FIELD_ARMOR_COUNT, count);
    if unit {
        let view = h.0.view(false);
        let icons = hud::armor_icons(count as c_long, view.upgrades_armor);
        for i in &icons.icons[..icons.count as usize] {
            show_icon(&mut h, i);
        }
        h.set(EE_SCREENS_BOX_FIELD_ARMOR_COUNT, icons.next as i64);
    }
    let armor = h.get(EE_SCREENS_BOX_FIELD_ARMOR_COUNT);
    h.set(EE_SCREENS_BOX_FIELD_COUNT, armor);
    if armor as c_long as u32 > 9 {
        h.a(EE_SCREENS_BOX_OP_VERIFY, 367);
    }
}

/// `EEUIFMGStatusBox::Fill` (`unit`: the call's unit is set; without one the box is cleared).
pub fn fill<T: BoxHost + ?Sized>(host: &mut T, unit: bool) {
    let mut h = H(host);
    h.set(EE_SCREENS_BOX_FIELD_COUNT, 0);
    h.set(EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN, 0);
    for bar in [LEFT, MIDDLE] {
        h.mode(bar, BOX_HIDDEN);
        h.queued(bar, 0);
    }
    for bar in [THIRD, FOURTH] {
        if h.present(bar) {
            h.mode(bar, BOX_HIDDEN);
            h.queued(bar, 0);
        }
    }
    h.mode(PORTRAIT, BOX_HIDDEN);
    h.mode(NAME, BOX_HIDDEN);
    if h.present(CIV) {
        h.mode(CIV, BOX_HIDDEN);
    }
    h.mode(EE_SCREENS_BOX_CTL_ARMOR_TITLE, BOX_HIDDEN);
    for i in 0..9 {
        h.mode(icon(i), BOX_HIDDEN);
        h.mode(label(i), BOX_HIDDEN);
    }
    if unit {
        h.names(false);
        let view = h.0.view(true);
        let bars = hud::fill_bars(&view);
        if bars.hit_points {
            h.bar_setup(LEFT, 24, None, view.max_hit_points, view.hit_points);
            h.a(EE_SCREENS_BOX_OP_DIRTY, LEFT);
            h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, LEFT, EE_SCREENS_BOX_IMPROVEMENT_HIT_POINTS);
            h.mode(LEFT, BOX_SHOWN);
        }
        let icons = hud::fill_icons(&view);
        for i in &icons.icons[..icons.count as usize] {
            show_icon(&mut h, i);
        }
        h.set(EE_SCREENS_BOX_FIELD_COUNT, icons.next as i64);
        h.set(EE_SCREENS_BOX_FIELD_ATTRIBUTES + 11, -1);
        fill_armor_attributes(h.0, true);
        let count = h.get(EE_SCREENS_BOX_FIELD_COUNT);
        let icons = hud::transport_icon(count as c_long, &view);
        show_icon(&mut h, &icons.icons[0]);
        h.set(EE_SCREENS_BOX_FIELD_COUNT, icons.next as i64);
        h.mode(PORTRAIT, BOX_SHOWN);
        h.a(EE_SCREENS_BOX_OP_PORTRAIT, 1);
        if bars.kind == 1 {
            h.bar_setup(MIDDLE, 26, Some(3), bars.max, bars.current);
            h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, MIDDLE, EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER);
            h.mode(MIDDLE, BOX_SHOWN);
            h.ab(EE_SCREENS_BOX_OP_BAR_ENABLED, MIDDLE, 1);
        } else if bars.kind == 2 {
            let bar = BARS[bars.bar as usize];
            h.bar_setup(bar, 42, Some(1), bars.max, bars.current);
            h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, bar, EE_SCREENS_BOX_IMPROVEMENT_FLIGHT_OR_POWER);
            h.mode(bar, BOX_SHOWN);
            h.ab(EE_SCREENS_BOX_OP_BAR_ENABLED, bar, 1);
            h.ab(EE_SCREENS_BOX_OP_STRING_ID, bar, EE_SCREENS_BOX_IDS_GARRISON);
        }
        if bars.garrison {
            h.bar_setup(MIDDLE, 44, Some(2), bars.garrison_max, bars.garrison_current);
            h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, MIDDLE, EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT);
            h.mode(MIDDLE, BOX_SHOWN);
            h.ab(EE_SCREENS_BOX_OP_BAR_ENABLED, MIDDLE, 0);
            h.ab(EE_SCREENS_BOX_OP_STRING_ID, MIDDLE, EE_SCREENS_BOX_IDS_GARRISON);
        }
        let count = h.get(EE_SCREENS_BOX_FIELD_COUNT) as c_long as u32;
        if count > 9 {
            h.a(EE_SCREENS_BOX_OP_VERIFY, 708);
        }
        for j in count..5 {
            h.mode(icon(j as i64), BOX_HIDDEN);
            h.mode(label(j as i64), BOX_HIDDEN);
            h.ab(EE_SCREENS_BOX_OP_COST_LABEL, icon(j as i64), -1);
        }
        refresh(h.0);
    }
    if h.a(EE_SCREENS_BOX_OP_IN_CINEMATIC, 0) == 0 {
        for id in BACKGROUNDS {
            h.ab(EE_SCREENS_BOX_OP_BACKGROUND, id, (unit || id == 124) as i64);
        }
        if !unit {
            h.a(EE_SCREENS_BOX_OP_PORTRAIT, 0);
        }
    }
}

/// `EEUIFMGStatusBox::HideBackgrounds`: the status box's panels hidden, all but 124.
pub fn hide_backgrounds<T: BoxHost + ?Sized>(host: &mut T) {
    for id in BACKGROUNDS {
        host.op(EE_SCREENS_BOX_OP_BACKGROUND, id, (id == 124) as i64, 0);
    }
}

/// `EEUIFMGStatusBox::UpdateArmorTexts`: the armor icons' "base+bonus".
pub fn update_armor_texts<T: BoxHost + ?Sized>(host: &mut T) {
    let mut h = H(host);
    for i in 0..6 {
        let index = h.get(EE_SCREENS_BOX_FIELD_ATTRIBUTES + 4 + i as i64);
        if index == -1 {
            continue;
        }
        let kind = hud::ARMOR_KINDS[i] as i64;
        let stat = f32::from_bits(h.ab(EE_SCREENS_BOX_OP_ARMOR, kind, 0) as u32);
        let class = h.ab(EE_SCREENS_BOX_OP_ARMOR, kind, 1) as c_long;
        let stats_class = h.ab(EE_SCREENS_BOX_OP_ARMOR, kind, 2) as c_long;
        let (base, bonus) = hud::armor_attribute(stat, class, stats_class);
        let (base, bonus) = hud::attribute_text(base, bonus);
        h.format(EE_SCREENS_BOX_FMT_SUM, base, bonus);
        h.cost(icon(index));
    }
}

/// `EEUIFMGStatusBox::Refresh`: the texts and bars of the same selection.
pub fn refresh<T: BoxHost + ?Sized>(host: &mut T) {
    let mut h = H(host);
    h.names(true);
    update_armor_texts(h.0);
    let view = h.0.view(false);
    let plan = hud::refresh(&view);
    if plan.hit_points {
        h.bar_texts(LEFT, plan.hit_points_text, Some(plan.hit_points_bonus), view.max_hit_points, view.hit_points);
    }
    for (i, slot) in [0, 1, 2, 3, 10].into_iter().enumerate() {
        let index = h.get(EE_SCREENS_BOX_FIELD_ATTRIBUTES + slot);
        if index != -1 {
            h.format(EE_SCREENS_BOX_FMT_SUM, plan.attributes[i][0], plan.attributes[i][1]);
            h.cost(icon(index));
        }
    }
    h.a(EE_SCREENS_BOX_OP_PORTRAIT_UPDATE, 0);
    if view.complex {
        let index = h.get(EE_SCREENS_BOX_FIELD_ATTRIBUTES + 12);
        if index != -1 {
            h.format(EE_SCREENS_BOX_FMT_PAIR, plan.cargo[0], plan.cargo[1]);
            h.cost(icon(index));
        }
        if plan.kind != 0 {
            let bar = BARS[plan.bar as usize];
            h.bar_texts(bar, plan.bar_text, Some(plan.bar_bonus), plan.bar_max, plan.bar_current);
        }
        if plan.garrison {
            h.bar_texts(MIDDLE, plan.garrison_text, None, plan.garrison_max, plan.garrison_current);
        }
    }
    if plan.resource {
        let count = h.get(EE_SCREENS_BOX_FIELD_COUNT);
        let ctl = icon(count);
        if h.get(EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN) == 0 {
            h.ab(EE_SCREENS_BOX_OP_SET_ATTRIBUTE, ctl, EE_SCREENS_BOX_IMPROVEMENT_TRANSPORT);
            h.ab(EE_SCREENS_BOX_OP_COST_LABEL, ctl, label(count));
            h.mode(ctl, BOX_SHOWN);
            h.mode(label(count), BOX_SHOWN);
            h.set(EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN, 1);
        }
        let bitmap = plan.resource_bitmap as i64;
        if bitmap != -1 {
            h.op(EE_SCREENS_BOX_OP_STRETCH, bitmap, ctl, ctl);
            h.op(EE_SCREENS_BOX_OP_STRETCH, bitmap + 1, ctl, ctl);
            for (which, b) in [(0, bitmap), (1, bitmap + 1)] {
                if h.op(EE_SCREENS_BOX_OP_BITMAP_IS, ctl, which, b) == 0 {
                    h.op(EE_SCREENS_BOX_OP_SET_BITMAP, ctl, which, b);
                }
            }
        }
        if plan.resource_pair {
            h.format(EE_SCREENS_BOX_FMT_PAIR, plan.resource_text[0], plan.resource_text[1]);
        } else {
            h.format(EE_SCREENS_BOX_FMT_ONE, plan.resource_text[0], 0);
        }
        h.cost(ctl);
    } else if h.get(EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN) != 0 {
        let count = h.get(EE_SCREENS_BOX_FIELD_COUNT);
        h.mode(icon(count), BOX_HIDDEN);
        h.mode(label(count), BOX_HIDDEN);
        h.set(EE_SCREENS_BOX_FIELD_RESOURCE_SHOWN, 0);
    }
}

/// `EEUIFMGStatusBox::UpdateQueuedCounts`: each shown improvement button's queued count.
pub fn update_queued_counts<T: BoxHost + ?Sized>(host: &mut T) {
    let mut h = H(host);
    let kind = h.a(EE_SCREENS_BOX_OP_UNIT_TYPE, 0);
    let one = |h: &mut H<T>, ctl: i64| {
        let attribute = h.a(EE_SCREENS_BOX_OP_ATTRIBUTE, ctl);
        let level = h.ab(EE_SCREENS_BOX_OP_COUNT_QUEUED, kind, attribute);
        h.queued(ctl, level);
    };
    for bar in [LEFT, MIDDLE, THIRD, FOURTH] {
        if (bar == LEFT || bar == MIDDLE || h.present(bar)) && !h.hidden(bar) {
            one(&mut h, bar);
        }
    }
    let mut i: u32 = 0;
    while i < h.get(EE_SCREENS_BOX_FIELD_COUNT) as c_long as u32 {
        if !h.hidden(icon(i as i64)) {
            one(&mut h, icon(i as i64));
        }
        i += 1;
    }
    let mut i: u32 = 5;
    while i < h.get(EE_SCREENS_BOX_FIELD_ARMOR_COUNT) as c_long as u32 {
        if !h.hidden(icon(i as i64)) {
            one(&mut h, icon(i as i64));
        }
        i += 1;
    }
}

/// One cell of the three unit improvement textures: its corners and size.
struct Cell {
    top_left: (f32, f32),
    bottom_right: (f32, f32),
    size: (i32, i32),
}

const fn c(l: f32, t: f32, r: f32, b: f32, w: i32, h: i32) -> Cell {
    Cell { top_left: (l, t), bottom_right: (r, b), size: (w, h) }
}

/// The original's `sIconCells` (0x564890): every cell's texture flag is set.
#[rustfmt::skip]
const CELLS: [Cell; 50] = [
    c(1.0, 1.0, 80.0, 80.0, 80, 80), c(0.0, 0.0, 79.0, 79.0, 80, 80),
    c(161.0, 1.0, 240.0, 80.0, 80, 80), c(160.0, 0.0, 239.0, 79.0, 80, 80),
    c(81.0, 1.0, 160.0, 80.0, 80, 80), c(80.0, 0.0, 159.0, 79.0, 80, 80),
    c(161.0, 161.0, 240.0, 240.0, 80, 80), c(160.0, 160.0, 239.0, 239.0, 80, 80),
    c(1.0, 81.0, 80.0, 160.0, 80, 80), c(0.0, 80.0, 79.0, 159.0, 80, 80),
    c(161.0, 81.0, 240.0, 160.0, 80, 80), c(160.0, 80.0, 239.0, 159.0, 80, 80),
    c(81.0, 81.0, 160.0, 160.0, 80, 80), c(80.0, 80.0, 159.0, 159.0, 80, 80),
    c(1.0, 161.0, 80.0, 240.0, 80, 80), c(0.0, 160.0, 79.0, 239.0, 80, 80),
    c(0.0, 40.0, 35.0, 57.0, 35, 17), c(35.0, 40.0, 70.0, 57.0, 35, 17),
    c(81.0, 161.0, 160.0, 240.0, 80, 80), c(80.0, 160.0, 159.0, 239.0, 80, 80),
    c(96.0, 0.0, 131.0, 17.0, 35, 17), c(131.0, 0.0, 166.0, 17.0, 35, 17),
    c(96.0, 0.0, 131.0, 17.0, 35, 17), c(131.0, 0.0, 166.0, 17.0, 35, 17),
    c(1.0, 81.0, 63.0, 143.0, 63, 63), c(0.0, 80.0, 62.0, 142.0, 63, 63),
    c(1.0, 144.0, 63.0, 206.0, 63, 63), c(0.0, 143.0, 62.0, 205.0, 63, 63),
    c(160.0, 1.0, 239.0, 80.0, 80, 80), c(160.0, 1.0, 239.0, 80.0, 80, 80),
    c(1.0, 80.0, 80.0, 159.0, 80, 80), c(1.0, 80.0, 80.0, 159.0, 80, 80),
    c(81.0, 1.0, 160.0, 80.0, 80, 80), c(81.0, 1.0, 160.0, 80.0, 80, 80),
    c(1.0, 1.0, 80.0, 80.0, 80, 80), c(1.0, 1.0, 80.0, 80.0, 80, 80),
    c(81.0, 80.0, 160.0, 159.0, 80, 80), c(81.0, 80.0, 160.0, 159.0, 80, 80),
    c(196.0, 143.0, 218.0, 160.0, 22, 17), c(196.0, 143.0, 218.0, 160.0, 22, 17),
    c(196.0, 41.0, 218.0, 58.0, 22, 17), c(196.0, 41.0, 218.0, 58.0, 22, 17),
    c(81.0, 1.0, 160.0, 81.0, 80, 80), c(80.0, 0.0, 159.0, 80.0, 80, 80),
    c(1.0, 1.0, 80.0, 81.0, 80, 80), c(0.0, 0.0, 79.0, 80.0, 80, 80),
    c(1.0, 1.0, 80.0, 81.0, 80, 80), c(0.0, 0.0, 79.0, 80.0, 80, 80),
    c(160.0, 80.0, 239.0, 159.0, 80, 80), c(160.0, 80.0, 239.0, 159.0, 80, 80),
];

/// The texture (1..3) of icon cell `i`.
pub fn cell_texture(i: usize) -> i64 {
    match i {
        24..=27 | 42..=47 => 2,
        28..=37 | 48 | 49 => 3,
        _ => 1,
    }
}

/// Two f32s as one argument (`lo` in the low 32 bits).
pub fn pack(lo: f32, hi: f32) -> i64 {
    ((hi.to_bits() as u64) << 32 | lo.to_bits() as u64) as i64
}

/// `EEUIFMGStatusBox::LoadIcons`.
pub fn load_icons<T: BoxHost + ?Sized>(host: &mut T) {
    let mut h = H(host);
    for i in 0..50 {
        h.a(EE_SCREENS_BOX_OP_NEW_BITMAP, i);
    }
    h.a(EE_SCREENS_BOX_OP_DESCRIPTION, 0);
    for t in 1..=3 {
        h.a(EE_SCREENS_BOX_OP_LOAD_TEXTURE, t);
    }
    for (j, cell) in CELLS.iter().enumerate() {
        let (l, t) = cell.top_left;
        let (r, b) = cell.bottom_right;
        h.op(EE_SCREENS_BOX_OP_SET_RECT, j as i64, pack(l, t), pack(r, b));
        let texture = cell_texture(j);
        let size = h.a(EE_SCREENS_BOX_OP_TEXTURE_SIZE, texture);
        let width = (size >> 32) as i32 as f32;
        let height = size as i32 as f32;
        let top_left = pack(l / width, t / height);
        let bottom_right = pack(r / width, b / height);
        h.op(EE_SCREENS_BOX_OP_BITMAP_TEXTURE, j as i64 | texture << 8, top_left, bottom_right);
        h.ab(EE_SCREENS_BOX_OP_BITMAP_SCALE, j as i64, pack(cell.size.0 as f32 / width, cell.size.1 as f32 / height));
    }
    h.a(EE_SCREENS_BOX_OP_ICONS_LOADED, 0);
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The golden's host: each op's answer as the driver recorded it, the view's bytes for `view`.
    struct Replay<'a> {
        ops: std::vec::IntoIter<(u32, i64, i64, i64, i64)>,
        views: std::vec::IntoIter<EeScreensHudUnit>,
        line: &'a str,
    }

    impl BoxHost for Replay<'_> {
        fn op(&mut self, op: u32, a: i64, b: i64, c: i64) -> i64 {
            let (o, ea, eb, ec, r) =
                self.ops.next().unwrap_or_else(|| panic!("extra op {op} {a} {b} {c}: {}", self.line));
            assert_eq!((o, ea, eb, ec), (op, a, b, c), "{}", self.line);
            r
        }
        fn view(&mut self, _fill: bool) -> EeScreensHudUnit {
            if let Some(o) = self.ops.next() {
                assert_eq!(o.0, 0, "view expected: {}", self.line);
            }
            self.views.next().expect("a view")
        }
    }

    fn decode_view(hex: &str) -> EeScreensHudUnit {
        let bytes: Vec<u8> =
            (0..hex.len()).step_by(2).map(|i| u8::from_str_radix(&hex[i..i + 2], 16).unwrap()).collect();
        assert_eq!(bytes.len(), core::mem::size_of::<EeScreensHudUnit>());
        let mut v = EeScreensHudUnit::default();
        // SAFETY: the driver wrote the struct's bytes (bools 0 or 1).
        unsafe {
            core::ptr::copy_nonoverlapping(bytes.as_ptr(), (&mut v as *mut EeScreensHudUnit).cast::<u8>(), bytes.len())
        };
        v
    }

    /// `golden_status_box.txt`: per case its calls (F fill, f fill without a unit, R refresh, Q queued counts, H
    /// hide backgrounds, L load icons; e.g. "FR") and the host's ops `|op a b c answer` (a view: `|V hex`), from
    /// the C++ golden driver (`build/sc23/box`) whose original side is the status box before hud.rs (efa5c5ad^).
    #[test]
    fn golden_status_box() {
        let text = include_str!("golden_status_box.txt");
        let mut n = 0;
        for line in text.lines() {
            let mut parts = line.split('|');
            let call = parts.next().unwrap();
            let mut ops = vec![];
            let mut views = vec![];
            for p in parts {
                let f: Vec<&str> = p.split(' ').collect();
                if f[0] == "V" {
                    views.push(decode_view(f[1]));
                    ops.push((0, 0, 0, 0, 0));
                } else {
                    let v: Vec<i64> = f.iter().map(|s| s.parse().unwrap()).collect();
                    ops.push((v[0] as u32, v[1], v[2], v[3], v[4]));
                }
            }
            let mut h = Replay { ops: ops.into_iter(), views: views.into_iter(), line };
            for c in call.chars() {
                match c {
                    'F' => fill(&mut h, true),
                    'f' => fill(&mut h, false),
                    'R' => refresh(&mut h),
                    'Q' => update_queued_counts(&mut h),
                    'H' => hide_backgrounds(&mut h),
                    'L' => load_icons(&mut h),
                    _ => panic!("call {c}"),
                }
            }
            assert!(h.ops.next().is_none(), "ops left: {line}");
            n += 1;
        }
        assert!(n >= 1200, "{n} cases");
    }

    struct Count(Vec<u32>);
    impl BoxHost for Count {
        fn op(&mut self, op: u32, a: i64, _b: i64, _c: i64) -> i64 {
            self.0.push(op);
            match op {
                EE_SCREENS_BOX_OP_TEXTURE_SIZE => 256 << 32 | 256,
                EE_SCREENS_BOX_OP_GET if a >= EE_SCREENS_BOX_FIELD_ATTRIBUTES => -1,
                _ => 0,
            }
        }
        fn view(&mut self, _fill: bool) -> EeScreensHudUnit {
            EeScreensHudUnit::default()
        }
    }

    #[test]
    fn the_cells_textures_and_coordinates() {
        assert_eq!((0..50).filter(|&i| cell_texture(i) == 2).count(), 10);
        assert_eq!((0..50).filter(|&i| cell_texture(i) == 3).count(), 12);
        let mut h = Count(vec![]);
        load_icons(&mut h);
        assert_eq!(h.0.iter().filter(|&&o| o == EE_SCREENS_BOX_OP_NEW_BITMAP).count(), 50);
        assert_eq!(h.0.iter().filter(|&&o| o == EE_SCREENS_BOX_OP_BITMAP_SCALE).count(), 50);
        assert_eq!(*h.0.last().unwrap(), EE_SCREENS_BOX_OP_ICONS_LOADED);
        assert_eq!(pack(1.0, 2.0), (2.0f32.to_bits() as i64) << 32 | 1.0f32.to_bits() as i64);
    }

    #[test]
    fn an_empty_fill_shows_only_the_last_panel() {
        let mut h = Count(vec![]);
        fill(&mut h, false);
        assert_eq!(h.0.iter().filter(|&&o| o == EE_SCREENS_BOX_OP_BACKGROUND).count(), 5);
        assert_eq!(*h.0.last().unwrap(), EE_SCREENS_BOX_OP_PORTRAIT);
        assert!(!h.0.contains(&EE_SCREENS_BOX_OP_NAME));
    }
}
