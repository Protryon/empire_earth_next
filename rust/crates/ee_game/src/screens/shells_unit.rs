//! The scenario editor unit tab's control kinds over the generic shells (`shells.rs`; once `EESEUnit.cpp`'s helper
//! classes): the unit class list (sorted inserts by name, epoch or family), the cargo list (sorted by name), the
//! six unit type buttons (the palette mode), the rotation button (eight 45-degree steps) and the remove cargo
//! button. The world (ghosts, the selection, the unit classes) and the keyboard are the host's.

use core::cell::{Cell, RefCell};
use core::ffi::CStr;
use core::sync::atomic::{AtomicI32, Ordering};

use crate::screens::shells::*;
use crate::screens::shells_editor::{
    EE_SCREENS_SH_BITMAP_COORDS, EE_SCREENS_SH_BITMAP_RELEASE, EE_SCREENS_SH_BITMAP_SCALE, EE_SCREENS_SH_NEW_BITMAP,
    EE_SCREENS_SH_SET_NORMAL, EE_SCREENS_SH_SET_PRESSED, EE_SCREENS_SH_TEXTURE_SIZE,
};
use crate::screens::shells_tech::{
    EE_SCREENS_SH_FORM_CONTROL, EE_SCREENS_SH_KIND_OP, EE_SCREENS_SH_LIST_CURRENT, EE_SCREENS_SH_LIST_DELETE_ALL,
    EE_SCREENS_SH_LIST_LINE_COUNT,
};

/// `EEUIFSELBUnitList` (a string list box).
pub const EE_SCREENS_SK_UNIT_CLASS_LIST: u32 = 0x50;
/// `EEUIFSELBCargoList` (a string list box).
pub const EE_SCREENS_SK_CARGO_LIST: u32 = 0x51;
/// `EEUIFSEBUnitType` (a radio button).
pub const EE_SCREENS_SK_UNIT_TYPE_BUTTON: u32 = 0x52;
/// `EEUIFSEBRotation` (a button).
pub const EE_SCREENS_SK_ROTATION: u32 = 0x53;
/// `EEUIFSEBRemoveCargo` (a button).
pub const EE_SCREENS_SK_REMOVE_CARGO: u32 = 0x54;

/// The unit class list's `SetSortMode(a)`.
pub const EE_SCREENS_SKOP_UL_SORT: u32 = 0x50;
/// The unit class list's `mUnitTypes.clear()` and `DeleteAllEntries()`.
pub const EE_SCREENS_SKOP_UL_CLEAR: u32 = 0x51;
/// The unit class list's `AddUnitType(a)`.
pub const EE_SCREENS_SKOP_UL_ADD: u32 = 0x52;
/// The unit class list's `mUnitTypes[a]`.
pub const EE_SCREENS_SKOP_UL_TYPE_AT: u32 = 0x53;
/// The cargo list's `mCargo.clear()` and `DeleteAllEntries()`.
pub const EE_SCREENS_SKOP_CARGO_CLEAR: u32 = 0x54;
/// The cargo list's `AddCargo((EEUnit*)a)`.
pub const EE_SCREENS_SKOP_CARGO_ADD: u32 = 0x55;
/// The cargo list's `RemoveSelected()`.
pub const EE_SCREENS_SKOP_CARGO_REMOVE: u32 = 0x56;
/// The rotation button's `GetAngle()` (its `f32` bits).
pub const EE_SCREENS_SKOP_ROTATION_ANGLE: u32 = 0x57;

/// The unit class `a`'s field: `b` 0 `mNameStringID`, 1 `mEpoch`, 2 `mFamily`.
pub const EE_SCREENS_SH_UNIT_CLASS: u32 = 0x70;
/// A list box's `InsertLine(UWideString(a bytes), b, UILBNoSort)`.
pub const EE_SCREENS_SH_LIST_INSERT_BYTES: u32 = 0x71;
/// A list box's line `a` (`GetData(a)`) copied into the buffer `b` (4500 bytes, NUL-ended).
pub const EE_SCREENS_SH_LIST_LINE: u32 = 0x72;
/// The unit tab's `SelectUnitType(the form's mouse location)`.
pub const EE_SCREENS_SH_UNIT_SELECT_TYPE: u32 = 0x73;
/// `UIControl::sEnterKey->GetValue()`.
pub const EE_SCREENS_SH_ENTER_KEY: u32 = 0x74;
/// The unit `a`'s class name string id << 32 | its unit id.
pub const EE_SCREENS_SH_UNIT_NAME_AND_ID: u32 = 0x75;
/// `EESEARust(kSEADeleteUnit, NULL, a)`, run and released.
pub const EE_SCREENS_SH_DELETE_UNIT: u32 = 0x76;
/// The bitmap `a`'s `SetRotation(f[0])`.
pub const EE_SCREENS_SH_BITMAP_ROTATION: u32 = 0x77;
/// Turns the placement ghosts (`a` 1) or the selected units (0) that can turn to yaw `f[0]`; -> 1 when there are
/// ghosts (`a` 2: asks only).
pub const EE_SCREENS_SH_ROTATE: u32 = 0x78;
/// The keyboard key `a` (0 comma, 1 period): 1 just pressed, 2 just released, 0 else; `<< 8` `GetEntity`'s result.
pub const EE_SCREENS_SH_KEY: u32 = 0x79;
/// The shell's own virtual `a` (`EE_SCREENS_SV_*`, through its vtable).
pub const EE_SCREENS_SH_SELF: u32 = 0x7a;
/// `CaptureKeyboard()` (`a` 1) or `ReleaseCapturedKeyboard()` (0).
pub const EE_SCREENS_SH_KEYBOARD: u32 = 0x7b;
/// `SetAlwaysDirty(a != 0)`.
pub const EE_SCREENS_SH_ALWAYS_DIRTY: u32 = 0x7c;
/// The scenario editor's button texture (`GetButtonTexture()`).
pub const EE_SCREENS_SH_EDITOR_TEXTURE: u32 = 0x7d;

/// `kUnitSortAlpha`, `kUnitSortEpoch`, `kUnitSortFamily` (ee/EEScenarioEditorIDs.h).
const SORT_ALPHA: i32 = 0;
const SORT_EPOCH: i32 = 1;
const SORT_FAMILY: i32 = 2;
/// The keys `EE_SCREENS_SH_KEY` reads: 0 `kKeyComma`, 1 `kKeyPeriod`.
const KEY_COMMA: i64 = 0;
const KEY_PERIOD: i64 = 1;
/// The cargo list's id.
const CARGO_LIST: i64 = 0x3f2;

/// `sUnitTypeMode`: the unit palette the six type buttons select (`kUnitPalette*`).
static UNIT_TYPE_MODE: AtomicI32 = AtomicI32::new(0);

/// `sUnitTypeMode` (the unit tab's host).
#[no_mangle]
pub extern "C" fn ee_screens_shell_unit_type_mode() -> i32 {
    UNIT_TYPE_MODE.load(Ordering::Relaxed)
}

pub(crate) fn make(kind: u32, _args: &[i64]) -> Option<Box<dyn Kind>> {
    Some(match kind {
        EE_SCREENS_SK_UNIT_CLASS_LIST => {
            Box::new(UnitClassList { types: RefCell::new(Vec::new()), mode: Cell::new(SORT_ALPHA) })
        }
        EE_SCREENS_SK_CARGO_LIST => Box::new(CargoList { cargo: RefCell::new(Vec::new()) }),
        EE_SCREENS_SK_UNIT_TYPE_BUTTON => Box::new(UnitTypeButton),
        EE_SCREENS_SK_ROTATION => {
            Box::new(Rotation { bitmaps: Cell::new([0; 4]), rotation: Cell::new(0), direction: Cell::new(0) })
        }
        EE_SCREENS_SK_REMOVE_CARGO => Box::new(RemoveCargo),
        _ => return None,
    })
}

/// A resource string's bytes.
fn string(id: i64) -> Vec<u8> {
    #[cfg(not(test))]
    {
        crate::screens::hud_direct::resource_string(id).map(|s| s.to_vec()).unwrap_or_default()
    }
    #[cfg(test)]
    {
        format!("s{id}").into_bytes()
    }
}

fn nul(mut v: Vec<u8>) -> Vec<u8> {
    v.push(0);
    v
}

/// `EEUIFSELBUnitList`: the unit tab's unit classes (each line's class in `mUnitTypes`).
struct UnitClassList {
    types: RefCell<Vec<i32>>,
    /// `mSortMode` (the comparator follows it).
    mode: Cell<i32>,
}

impl UnitClassList {
    fn name(c: &Ctx, class: i32) -> Vec<u8> {
        string(c.call(EE_SCREENS_SH_UNIT_CLASS, i64::from(class), 0))
    }
    /// `mCompare(l, r)`: by name, or by epoch (family) then name.
    fn less(&self, c: &Ctx, l: i32, r: i32) -> bool {
        let field = match self.mode.get() {
            SORT_EPOCH => 1,
            SORT_FAMILY => 2,
            _ => 0,
        };
        if field != 0 {
            let (a, b) = (
                c.call(EE_SCREENS_SH_UNIT_CLASS, i64::from(l), field),
                c.call(EE_SCREENS_SH_UNIT_CLASS, i64::from(r), field),
            );
            if a > b {
                return false;
            }
            if a < b {
                return true;
            }
        }
        Self::name(c, l) < Self::name(c, r)
    }
    /// `AddUnitType`: inserted before the first line it sorts before.
    fn add(&self, c: &Ctx, class: i32) {
        let types = self.types.borrow().clone();
        let at = types.iter().position(|&t| self.less(c, class, t)).unwrap_or(types.len());
        let mut text = Vec::new();
        if self.mode.get() != SORT_ALPHA {
            let field = if self.mode.get() == SORT_FAMILY { 2 } else { 1 };
            let mut v = c.call(EE_SCREENS_SH_UNIT_CLASS, i64::from(class), field) as i32;
            if field == 1 {
                v = v.wrapping_add(1);
            }
            text.extend_from_slice(format!("{v}: ").as_bytes());
        }
        text.extend_from_slice(&Self::name(c, class));
        let text = nul(text);
        c.call(EE_SCREENS_SH_LIST_INSERT_BYTES, text.as_ptr() as i64, at as i64);
        self.types.borrow_mut().insert(at, class);
    }
}

impl Kind for UnitClassList {
    fn name(&self) -> &'static CStr {
        c"EEUIFSELBUnitList"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_UPDATE_KEYBOARD | 1 << EE_SCREENS_SV_BUTTON_UP
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_BUTTON_UP => {
                c.call(EE_SCREENS_SH_UNIT_SELECT_TYPE, 0, 0);
                c.call(EE_SCREENS_SH_FIRE, 0x3f0, 0);
                0
            }
            EE_SCREENS_SV_UPDATE_KEYBOARD => {
                let r = c.base(v, a, b, f);
                if c.call(EE_SCREENS_SH_ENTER_KEY, 0, 0) != 0 {
                    c.call(EE_SCREENS_SH_UNIT_SELECT_TYPE, 0, 0);
                }
                r
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_UL_SORT => self.mode.set(a as i32),
            EE_SCREENS_SKOP_UL_CLEAR => {
                self.types.borrow_mut().clear();
                c.call(EE_SCREENS_SH_LIST_DELETE_ALL, 0, 0);
            }
            EE_SCREENS_SKOP_UL_ADD => self.add(c, a as i32),
            EE_SCREENS_SKOP_UL_TYPE_AT => return i64::from(self.types.borrow().get(a as usize).copied().unwrap_or(-1)),
            _ => {}
        }
        0
    }
}

/// `EEUIFSELBCargoList`: the cargo of the selected units, by name (the units by id).
struct CargoList {
    cargo: RefCell<Vec<i64>>,
}

impl Kind for CargoList {
    fn name(&self) -> &'static CStr {
        c"EEUIFSELBCargoList"
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_CARGO_CLEAR => {
                self.cargo.borrow_mut().clear();
                c.call(EE_SCREENS_SH_LIST_DELETE_ALL, 0, 0);
            }
            EE_SCREENS_SKOP_CARGO_ADD => {
                let w = c.call(EE_SCREENS_SH_UNIT_NAME_AND_ID, a, 0);
                let name = string(w >> 32);
                let id = i64::from(w as i32);
                let n = self.cargo.borrow().len();
                let mut at = 0usize;
                if c.call(EE_SCREENS_SH_LIST_LINE_COUNT, 0, 0) != 0 {
                    let mut buf = vec![0u8; 4500];
                    while at < n {
                        c.call(EE_SCREENS_SH_LIST_LINE, at as i64, buf.as_mut_ptr() as i64);
                        let line = &buf[..buf.iter().position(|&x| x == 0).unwrap_or(buf.len())];
                        if name.as_slice() < line {
                            break;
                        }
                        at += 1;
                    }
                }
                self.cargo.borrow_mut().insert(at, id);
                let text = nul(name);
                c.call(EE_SCREENS_SH_LIST_INSERT_BYTES, text.as_ptr() as i64, at as i64);
            }
            EE_SCREENS_SKOP_CARGO_REMOVE if c.call(EE_SCREENS_SH_LIST_LINE_COUNT, 0, 0) > 0 => {
                let s = c.call(EE_SCREENS_SH_LIST_CURRENT, 0, 0);
                if s >= 0 {
                    let id = self.cargo.borrow().get(s as usize).copied();
                    if let Some(id) = id {
                        c.call(EE_SCREENS_SH_DELETE_UNIT, id, 0);
                    }
                }
            }
            _ => {}
        }
        0
    }
}

/// `EEUIFSEBUnitType`: the six palette buttons (ids 0x406 .. 0x40b: land, air, naval, structures, heroes,
/// resources = `kUnitPalette*` 0 .. 5).
struct UnitTypeButton;

impl Kind for UnitTypeButton {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEBUnitType"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_CHECK
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_CHECK {
            let id = c.call(EE_SCREENS_SH_ID, 0, 0);
            if (0x406..=0x40b).contains(&id) {
                UNIT_TYPE_MODE.store((id - 0x406) as i32, Ordering::Relaxed);
            }
        }
        c.base(v, a, b, f)
    }
}

/// `EEUIFSEBRotation`: the placement rotation (left click one step forward, right click back; comma and period).
struct Rotation {
    /// `mBitmaps`: (straight, diagonal) x (normal, pressed).
    bitmaps: Cell<[i64; 4]>,
    /// `mRotation` 0..7, `mDirection` +1 / -1 while pressed.
    rotation: Cell<i32>,
    direction: Cell<i32>,
}

/// The yaw by step (`GetAngle`; step 1 is 0): the original's float literals.
#[allow(clippy::approx_constant)]
const ANGLES: [f32; 8] = [0.785_398_2, 0.0, -0.785_398_2, -1.570_796_4, -2.356_194_5, 3.141_592_7, 2.356_194_5, 1.570_796_4];

impl Rotation {
    fn update_bitmaps(&self, c: &Ctx) {
        let k = ((self.rotation.get() & 1) * 2) as usize;
        let b = self.bitmaps.get();
        c.call(EE_SCREENS_SH_SET_NORMAL, b[k], 0);
        c.call(EE_SCREENS_SH_SET_PRESSED, b[k + 1], 0);
    }
    fn angle(&self) -> f32 {
        ANGLES.get(self.rotation.get() as usize).copied().unwrap_or(0.0)
    }
}

impl Kind for Rotation {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEBRotation"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_UPDATE_KEYBOARD
            | 1 << EE_SCREENS_SV_BUTTON_UP
            | 1 << EE_SCREENS_SV_MOUSE_L_DOWN
            | 1 << EE_SCREENS_SV_MOUSE_R_DOWN
            | 1 << EE_SCREENS_SV_MOUSE_R_UP
    }
    fn init(&self, c: &Ctx) {
        let texture = c.call(EE_SCREENS_SH_EDITOR_TEXTURE, 0, 0);
        let mut f = [0.0f32; 4];
        c.host.call(c.ctl, EE_SCREENS_SH_TEXTURE_SIZE, texture, 0, &mut f);
        let size = f[0];
        let mut b = [0i64; 4];
        for x in b.iter_mut() {
            *x = c.call(EE_SCREENS_SH_NEW_BITMAP, texture, 1);
        }
        for i in 0..2 {
            // straight cells at x 0 and 50, diagonal at 100 and 150 (50x50)
            let x0 = if i & 1 != 0 { 100.0f32 } else { 0.0 };
            for k in 0..2 {
                let l = x0 + 50.0 * k as f32;
                c.call_f(EE_SCREENS_SH_BITMAP_COORDS, b[i * 2 + k], [l / size, 0.0 / size, (l + 50.0) / size, 50.0 / size]);
            }
            for k in 0..2 {
                c.call_f(EE_SCREENS_SH_BITMAP_SCALE, b[i * 2 + k], [50.0 / size, 50.0 / size, 0.0, 0.0]);
            }
        }
        self.bitmaps.set(b);
        self.update_bitmaps(c);
        c.call(EE_SCREENS_SH_ALWAYS_DIRTY, 1, 0);
    }
    fn free(&self, c: &Ctx) {
        for b in self.bitmaps.get() {
            c.call(EE_SCREENS_SH_BITMAP_RELEASE, b, 0);
        }
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_BUTTON_UP => {
                let mut r = self.rotation.get().wrapping_add(self.direction.get());
                if r > 7 {
                    r = 0;
                } else if r < 0 {
                    r = 7;
                }
                self.rotation.set(r);
                let angle = ((r >> 1) as u32) as f32 * 1.570_796_4f32;
                for bm in self.bitmaps.get() {
                    c.call_f(EE_SCREENS_SH_BITMAP_ROTATION, bm, [angle, 0.0, 0.0, 0.0]);
                }
                let ghosts = c.call(EE_SCREENS_SH_ROTATE, 2, 0) != 0;
                c.call_f(EE_SCREENS_SH_ROTATE, i64::from(ghosts), [self.angle(), 0.0, 0.0, 0.0]);
                self.update_bitmaps(c);
                self.direction.set(0);
                c.base(v, a, b, f)
            }
            EE_SCREENS_SV_MOUSE_L_DOWN | EE_SCREENS_SV_MOUSE_R_DOWN => {
                self.direction.set(if v == EE_SCREENS_SV_MOUSE_L_DOWN { 1 } else { -1 });
                c.base0(EE_SCREENS_SV_MOUSE_L_DOWN)
            }
            EE_SCREENS_SV_MOUSE_R_UP => c.base0(EE_SCREENS_SV_MOUSE_L_UP),
            EE_SCREENS_SV_UPDATE_KEYBOARD => {
                let mut result = 0;
                for (key, dir) in [(KEY_COMMA, -1), (KEY_PERIOD, 1)] {
                    let k = c.call(EE_SCREENS_SH_KEY, key, 0);
                    result = k >> 8;
                    match k & 0xff {
                        1 => {
                            self.direction.set(dir);
                            c.call(EE_SCREENS_SH_SELF, i64::from(EE_SCREENS_SV_BUTTON_DOWN), 0);
                            c.call(EE_SCREENS_SH_KEYBOARD, 1, 0);
                        }
                        2 => {
                            c.call(EE_SCREENS_SH_SELF, i64::from(EE_SCREENS_SV_BUTTON_UP), 0);
                            c.call(EE_SCREENS_SH_KEYBOARD, 0, 0);
                        }
                        _ => {}
                    }
                }
                result
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, _c: &Ctx, op: u32, _a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        if op == EE_SCREENS_SKOP_ROTATION_ANGLE {
            i64::from(self.angle().to_bits())
        } else {
            0
        }
    }
}

/// `EEUIFSEBRemoveCargo`: deletes the cargo list's selected unit.
struct RemoveCargo;

impl Kind for RemoveCargo {
    fn name(&self) -> &'static CStr {
        c"EEUIFSEBRemoveCargo"
    }
    fn overrides(&self) -> u64 {
        1 << EE_SCREENS_SV_BUTTON_UP
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        if v == EE_SCREENS_SV_BUTTON_UP {
            let list = c.call(EE_SCREENS_SH_FORM_CONTROL, CARGO_LIST, 0);
            c.call(EE_SCREENS_SH_KIND_OP, list, i64::from(EE_SCREENS_SKOP_CARGO_REMOVE) << 32);
        }
        c.base(v, a, b, f)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    type Call = (u32, i64, i64, [f32; 4]);

    struct Rec {
        calls: RefCell<Vec<Call>>,
        answer: fn(u32, i64, i64) -> i64,
    }

    impl ShellHost for Rec {
        fn call(&self, _ctl: Cpp, op: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
            if op == EE_SCREENS_SH_TEXTURE_SIZE {
                f[0] = 200.0;
            }
            self.calls.borrow_mut().push((op, a, b, *f));
            (self.answer)(op, a, b)
        }
    }

    #[test]
    fn the_unit_class_list_inserts_in_order() {
        // class k's name is "s<k>" (the test strings), its epoch 9 - k
        let h = Rec {
            calls: RefCell::new(Vec::new()),
            answer: |op, a, b| if op == EE_SCREENS_SH_UNIT_CLASS { if b == 0 { a } else { 9 - a } } else { 0 },
        };
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let l = UnitClassList { types: RefCell::new(Vec::new()), mode: Cell::new(SORT_ALPHA) };
        for k in [5, 2, 7] {
            l.add(&c, k);
        }
        assert_eq!(*l.types.borrow(), vec![2, 5, 7]);
        l.mode.set(SORT_EPOCH);
        l.add(&c, 8);
        // epoch 1 sorts before every listed class (epochs 7, 4, 2)
        assert_eq!(*l.types.borrow(), vec![8, 2, 5, 7]);
    }

    #[test]
    fn the_rotation_steps_and_wraps() {
        let h = Rec { calls: RefCell::new(Vec::new()), answer: |_, _, _| 0 };
        let c = Ctx { ctl: core::ptr::null_mut(), host: &h };
        let r = Rotation { bitmaps: Cell::new([1, 2, 3, 4]), rotation: Cell::new(0), direction: Cell::new(-1) };
        r.virt(&c, EE_SCREENS_SV_BUTTON_UP, 0, 0, &mut [0.0; 4]);
        assert_eq!(r.rotation.get(), 7);
        assert_eq!(r.angle(), 1.570_796_4);
        // the bitmaps turned by (7 >> 1) quarter turns, then the selection rotated (no ghosts)
        let calls = h.calls.borrow();
        assert_eq!(calls[0].3[0], 3.0 * 1.570_796_4);
        assert!(calls.iter().any(|x| x.0 == EE_SCREENS_SH_ROTATE && x.1 == 0 && x.3[0] == 1.570_796_4));
        assert_eq!(r.direction.get(), 0);
    }
}
