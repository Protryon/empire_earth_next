//! `UIHotKey` whole in Rust (R6 ui round 20): [`EeUiHotKeyFields`] is the C++ class's private base (the field names
//! the members'; `UIForm.cpp`, a friend, reads them in place), and every method but `GetHotKeyName` (a C++ walk of the
//! keys' names over round 9's `hot_key_name`) runs here: the constructors' state, the event setters, `SetModifier`,
//! `SetKey`, `CalculateKeyCount`, `FireEvents` and the destructor; the shared table (`sHotKey`) is a Rust list
//! `CreateHotKeys` fills and `DeleteHotKeys` empties. The logic is round 5's and 9's (`hotkey.rs`, pinned by their
//! goldens); this object is its host.
//!
//! Every call out is an [`EeUiOp`] (570..581) on the record's host (`UIHotKey.cpp`'s `UIHotKeyHost::Op`, the object the
//! record): the keys' questions (pure), the keyboard's entities, the events' calls and the hot keys' `new`/`delete`.

use core::ffi::{c_long, c_ulong, c_void};
use std::sync::Mutex;

#[cfg(not(any(test, feature = "no_cpp")))]
use crate::hotkey::CreateHost;
use crate::hotkey::{self, EventCall, EventSlotHost, FireHost, HotKeyEvent, KeysHost, KEY_SLOT};
use crate::obj::{EeUiHostFn, EeUiOp as Op, Host};

/// `UIHotKey`'s members (`include/lle/UIHotKey.h`), the C++ class's base.
#[repr(C)]
#[allow(non_snake_case)]
#[derive(Debug)]
pub struct EeUiHotKeyFields {
    /// The host of the calls out (its object is the record).
    pub mHost: EeUiHostFn,
    /// The key (`ISEntity*`).
    pub mKey: *mut c_void,
    /// The modifiers (`ISEntity*`, NULL for none).
    pub mModifiers: [*mut c_void; 3],
    /// The events (`UIEvent*`, referenced).
    pub mDoublePressedEvent: *mut c_void,
    pub mJustPressedEvent: *mut c_void,
    pub mJustUnpressedEvent: *mut c_void,
    pub mPressedEvent: *mut c_void,
    /// The form (`UIForm*`).
    pub mForm: *mut c_void,
    pub mModifierState: c_ulong,
    pub mKeyCount: c_ulong,
    pub mEnabled: bool,
    pub mAlwaysFire: bool,
}

/// The inward calls (the C++ class's methods that forward here).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiHotKeyCall {
    /// The default constructor's state.
    Init = 1,
    /// The copy constructor `UIHotKey(form b, *(record a))`.
    Copy = 2,
    /// The destructor: the four events released.
    Release = 3,
    /// `Set*Event(b)`: a 0 double-pressed, 1 just-pressed, 2 pressed, 3 just-unpressed (`HotKeyEvent`).
    SetEvent = 4,
    /// `SetModifier(a, slot b)`.
    SetModifier = 5,
    /// `SetKey(a)`.
    SetKey = 6,
    /// `CalculateKeyCount()`.
    CountKeys = 7,
    /// `FireEvents()`.
    FireEvents = 8,
}

/// A hot key's record, reached by address: every access reads or writes the C++ object's base afresh.
#[derive(Clone, Copy)]
pub struct Hk(*mut EeUiHotKeyFields);

impl Hk {
    /// # Safety
    /// `p` is a live record (a C++ hot key's base) for as long as the value is used.
    pub unsafe fn new(p: *mut EeUiHotKeyFields) -> Self {
        Hk(p)
    }

    fn get<T>(&self, f: impl FnOnce(&EeUiHotKeyFields) -> T) -> T {
        // SAFETY: live (the constructor's contract); the borrow ends before any call out
        f(unsafe { &*self.0 })
    }

    fn set(&self, f: impl FnOnce(&mut EeUiHotKeyFields)) {
        // SAFETY: as get
        f(unsafe { &mut *self.0 })
    }

    fn host(&self) -> Host {
        Host { obj: self.0.cast(), f: self.get(|r| r.mHost) }
    }

    fn op(&self, op: Op, a: isize, b: isize) -> isize {
        self.host().op(op, a, b, 0, 0)
    }

    fn event(&self, e: HotKeyEvent) -> *mut c_void {
        self.get(|r| match e {
            HotKeyEvent::DoublePressed => r.mDoublePressedEvent,
            HotKeyEvent::JustPressed => r.mJustPressedEvent,
            HotKeyEvent::Pressed => r.mPressedEvent,
            HotKeyEvent::JustUnpressed => r.mJustUnpressedEvent,
        })
    }

    fn set_event(&self, e: HotKeyEvent, event: *mut c_void) {
        hotkey::set_event(&mut Slot(*self, e), event);
    }

    fn count_keys(&self) {
        let n = self.get(|r| {
            hotkey::key_count(
                !r.mKey.is_null(),
                [!r.mModifiers[0].is_null(), !r.mModifiers[1].is_null(), !r.mModifiers[2].is_null()],
            )
        });
        self.set(|r| r.mKeyCount = n);
    }

    /// `UIHotKey()`: no keys or events, enabled.
    pub fn init(&self, host: EeUiHostFn) {
        self.set(|r| {
            r.mHost = host;
            r.mKey = core::ptr::null_mut();
            r.mModifiers = [core::ptr::null_mut(); 3];
            r.mDoublePressedEvent = core::ptr::null_mut();
            r.mJustPressedEvent = core::ptr::null_mut();
            r.mPressedEvent = core::ptr::null_mut();
            r.mJustUnpressedEvent = core::ptr::null_mut();
            r.mForm = core::ptr::null_mut();
        });
        self.count_keys();
        self.set(|r| {
            r.mModifierState = 0;
            r.mAlwaysFire = false;
            r.mEnabled = true;
        });
    }

    /// `UIHotKey(form, other)`: the other's keys, its events set (referenced) in the copy constructor's order.
    pub fn copy(&self, host: EeUiHostFn, other: Hk, form: *mut c_void) {
        let (key, modifiers) = other.get(|o| (o.mKey, o.mModifiers));
        self.set(|r| {
            r.mHost = host;
            r.mKey = key;
            r.mModifiers = modifiers;
            r.mDoublePressedEvent = core::ptr::null_mut();
            r.mJustPressedEvent = core::ptr::null_mut();
            r.mPressedEvent = core::ptr::null_mut();
            r.mJustUnpressedEvent = core::ptr::null_mut();
        });
        for e in
            [HotKeyEvent::DoublePressed, HotKeyEvent::JustPressed, HotKeyEvent::JustUnpressed, HotKeyEvent::Pressed]
        {
            self.set_event(e, other.event(e));
        }
        let (count, enabled, state, always) = other.get(|o| (o.mKeyCount, o.mEnabled, o.mModifierState, o.mAlwaysFire));
        self.set(|r| {
            r.mForm = form;
            r.mKeyCount = count;
            r.mEnabled = enabled;
            r.mModifierState = state;
            r.mAlwaysFire = always;
        });
    }

    /// One method ([`EeUiHotKeyCall`]); `Init` and `Copy` take the host as `b`'s and `a`'s partner (see the externs).
    pub fn call(&self, call: EeUiHotKeyCall, a: isize, b: isize) -> isize {
        use EeUiHotKeyCall as C;
        match call {
            C::Init | C::Copy => unreachable!("the constructors are ee_ui_hotkey_init and ee_ui_hotkey_copy"),
            C::Release => {
                for e in [
                    HotKeyEvent::DoublePressed,
                    HotKeyEvent::JustPressed,
                    HotKeyEvent::JustUnpressed,
                    HotKeyEvent::Pressed,
                ] {
                    self.set_event(e, core::ptr::null_mut());
                }
            }
            C::SetEvent => {
                let e = match a {
                    0 => HotKeyEvent::DoublePressed,
                    1 => HotKeyEvent::JustPressed,
                    2 => HotKeyEvent::Pressed,
                    _ => HotKeyEvent::JustUnpressed,
                };
                self.set_event(e, b as *mut c_void);
            }
            C::SetModifier => hotkey::set_modifier(&mut Keys(self.host()), self.0.cast(), a as c_ulong, b as usize),
            C::SetKey => {
                Keys(self.host()).get_entity(self.0.cast(), a as c_ulong, KEY_SLOT);
                self.count_keys();
            }
            C::CountKeys => self.count_keys(),
            C::FireEvents => return hotkey::fire_events(&mut Fire(*self)) as isize,
        }
        0
    }
}

/// One event member.
struct Slot(Hk, HotKeyEvent);

impl EventSlotHost for Slot {
    fn get(&self) -> hotkey::Event {
        self.0.event(self.1)
    }
    fn set(&mut self, e: hotkey::Event) {
        let which = self.1;
        self.0.set(|r| match which {
            HotKeyEvent::DoublePressed => r.mDoublePressedEvent = e,
            HotKeyEvent::JustPressed => r.mJustPressedEvent = e,
            HotKeyEvent::Pressed => r.mPressedEvent = e,
            HotKeyEvent::JustUnpressed => r.mJustUnpressedEvent = e,
        });
    }
    fn call(&mut self, e: hotkey::Event, c: EventCall) {
        self.0.op(Op::HkEventCall, e as isize, c as isize);
    }
}

/// `FireEvents`' view.
struct Fire(Hk);

impl FireHost for Fire {
    fn enabled(&self) -> bool {
        self.0.get(|r| r.mEnabled)
    }
    fn modifier_value(&mut self, i: usize) -> Option<c_long> {
        let m = self.0.get(|r| r.mModifiers[i]);
        (!m.is_null()).then(|| self.0.op(Op::HkValue, m as isize, 0) as c_long)
    }
    fn keyboard_state(&mut self) -> c_ulong {
        self.0.op(Op::HkKeyboardState, 0, 0) as c_ulong
    }
    fn own_state(&self) -> c_ulong {
        self.0.get(|r| r.mModifierState)
    }
    fn key_state(&mut self) -> c_ulong {
        let index = self.0.op(Op::HkKeyIndex, self.0.get(|r| r.mKey as isize), 0);
        self.0.op(Op::HkModifierState, index, 0) as c_ulong
    }
    fn has_event(&self, e: HotKeyEvent) -> bool {
        !self.0.event(e).is_null()
    }
    fn key_test(&mut self, e: HotKeyEvent) -> bool {
        let key = self.0.get(|r| r.mKey as isize);
        match e {
            HotKeyEvent::DoublePressed => self.0.op(Op::HkDoublePressed, key, 0) != 0,
            HotKeyEvent::JustPressed => self.0.op(Op::HkJustPressed, key, 0) != 0,
            HotKeyEvent::Pressed => self.0.op(Op::HkValue, key, 0) > 0,
            HotKeyEvent::JustUnpressed => self.0.op(Op::HkJustUnpressed, key, 0) != 0,
        }
    }
    fn process(&mut self, e: HotKeyEvent) {
        self.0.op(Op::HkProcess, self.0.event(e) as isize, 0);
    }
}

/// The keys' view (`SetModifier`, `CreateHotKeys`): a hot key is its record; the host answers the keyboard's
/// questions (its object is a hot key's record, or NULL for `CreateHotKeys`' own).
struct Keys(Host);

fn hk(key: hotkey::HotKey) -> Hk {
    Hk(key.cast())
}

impl KeysHost for Keys {
    fn get_entity(&mut self, key: hotkey::HotKey, code: c_ulong, member: usize) -> c_long {
        // `gISKeyboard->GetEntity(code, member)`: the entity (NULL for a code without a key) and the result
        let entity = self.0.op(Op::HkEntity, code as isize, 0, 0, 0) as *mut c_void;
        hk(key).set(|r| match member {
            KEY_SLOT => r.mKey = entity,
            m => r.mModifiers[m] = entity,
        });
        self.0.op(Op::HkEntityResult, code as isize, 0, 0, 0) as c_long
    }
    fn modifier_state(&mut self, code: c_ulong) -> c_ulong {
        self.0.op(Op::HkModifierState, code as isize, 0, 0, 0) as c_ulong
    }
    fn state(&self, key: hotkey::HotKey) -> c_ulong {
        hk(key).get(|r| r.mModifierState)
    }
    fn set_state(&mut self, key: hotkey::HotKey, state: c_ulong) {
        hk(key).set(|r| r.mModifierState = state);
    }
    fn clear_modifier(&mut self, key: hotkey::HotKey, slot: usize) {
        hk(key).set(|r| r.mModifiers[slot] = core::ptr::null_mut());
    }
    fn count_keys(&mut self, key: hotkey::HotKey) {
        hk(key).count_keys();
    }
}

/// The shared table (`UIHotKey::sHotKey`): the records of the hot keys `CreateHotKeys` made (0 for an unused record).
static HOT_KEYS: Mutex<Vec<usize>> = Mutex::new(Vec::new());

fn table() -> std::sync::MutexGuard<'static, Vec<usize>> {
    HOT_KEYS.lock().unwrap_or_else(|e| e.into_inner())
}

/// `CreateHotKeys`' file (`FSFileSpec&`, read through `UIEvent.cpp`'s file calls) and the table.
#[cfg(not(any(test, feature = "no_cpp")))]
struct Create {
    keys: Keys,
    file: *mut c_void,
}

#[cfg(not(any(test, feature = "no_cpp")))]
mod file_sys {
    use core::ffi::{c_long, c_ulong, c_void};

    extern "C" {
        pub fn ee_uicpp_file_open(file: *mut c_void);
        pub fn ee_uicpp_file_read_count(file: *mut c_void, count: *mut c_ulong) -> c_long;
        pub fn ee_uicpp_file_read(file: *mut c_void, data: *mut u8, length: c_ulong) -> c_long;
        pub fn ee_uicpp_file_close(file: *mut c_void);
    }
}

#[cfg(not(any(test, feature = "no_cpp")))]
impl KeysHost for Create {
    fn get_entity(&mut self, key: hotkey::HotKey, code: c_ulong, member: usize) -> c_long {
        self.keys.get_entity(key, code, member)
    }
    fn modifier_state(&mut self, code: c_ulong) -> c_ulong {
        self.keys.modifier_state(code)
    }
    fn state(&self, key: hotkey::HotKey) -> c_ulong {
        self.keys.state(key)
    }
    fn set_state(&mut self, key: hotkey::HotKey, state: c_ulong) {
        self.keys.set_state(key, state)
    }
    fn clear_modifier(&mut self, key: hotkey::HotKey, slot: usize) {
        self.keys.clear_modifier(key, slot)
    }
    fn count_keys(&mut self, key: hotkey::HotKey) {
        self.keys.count_keys(key)
    }
}

#[cfg(not(any(test, feature = "no_cpp")))]
impl CreateHost for Create {
    fn open(&mut self) {
        // SAFETY: the C++ caller's live file
        unsafe { file_sys::ee_uicpp_file_open(self.file) }
    }
    fn read_count(&mut self, count: &mut c_ulong) -> c_long {
        // SAFETY: as open
        unsafe { file_sys::ee_uicpp_file_read_count(self.file, count) }
    }
    fn read_record(&mut self, record: &mut hotkey::DbHotKey) -> c_long {
        let p = (record as *mut hotkey::DbHotKey).cast::<u8>();
        let n = core::mem::size_of::<hotkey::DbHotKey>() as c_ulong;
        // SAFETY: as open; the record is n bytes
        unsafe { file_sys::ee_uicpp_file_read(self.file, p, n) }
    }
    fn create(&mut self) -> hotkey::HotKey {
        self.keys.0.op(Op::HkNew, 0, 0, 0, 0) as hotkey::HotKey
    }
    fn push(&mut self, key: hotkey::HotKey) {
        table().push(key as usize);
    }
    fn close(&mut self) {
        // SAFETY: as open
        unsafe { file_sys::ee_uicpp_file_close(self.file) }
    }
}

/// `UIHotKey()` over its record.
///
/// # Safety
/// `rec` is the C++ hot key's base.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_init(rec: *mut EeUiHotKeyFields, host: EeUiHostFn) {
    // SAFETY: the contract
    unsafe { Hk::new(rec) }.init(host)
}

/// `UIHotKey(form, *other)` over its record.
///
/// # Safety
/// `rec` is the C++ hot key's base, `other` a live hot key's.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_copy(
    rec: *mut EeUiHotKeyFields,
    host: EeUiHostFn,
    other: *mut EeUiHotKeyFields,
    form: *mut c_void,
) {
    // SAFETY: the contract
    unsafe { Hk::new(rec).copy(host, Hk::new(other), form) }
}

/// One of `UIHotKey`'s methods ([`EeUiHotKeyCall`], not the constructors).
///
/// # Safety
/// `rec` is a live record [`ee_ui_hotkey_init`] or [`ee_ui_hotkey_copy`] set up.
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_call(
    rec: *mut EeUiHotKeyFields,
    call: EeUiHotKeyCall,
    a: isize,
    b: isize,
) -> isize {
    // SAFETY: the contract
    unsafe { Hk::new(rec) }.call(call, a, b)
}

/// `UIHotKey::CreateHotKeys(*file)`: the table filled from the file (`host` answers `HkNew` and the keyboard's
/// questions for the new hot keys).
///
/// # Safety
/// `file` is a live `FSFileSpec`.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_hotkey_create_all(file: *mut c_void, host: EeUiHostFn) -> c_long {
    let keys = Keys(Host { obj: core::ptr::null_mut(), f: host });
    hotkey::create(&mut Create { keys, file })
}

/// `UIHotKey::DeleteHotKeys()`: each hot key of the table deleted (`HkDelete`), in order; the table emptied.
#[no_mangle]
pub extern "C" fn ee_ui_hotkey_delete_all(host: EeUiHostFn) {
    let all = core::mem::take(&mut *table());
    let h = Host { obj: core::ptr::null_mut(), f: host };
    for k in all {
        if k != 0 {
            h.op(Op::HkDelete, k as isize, 0, 0, 0);
        }
    }
}

/// `UIHotKey::GetHotKeyCount()`.
#[no_mangle]
pub extern "C" fn ee_ui_hotkey_count() -> c_ulong {
    table().len() as c_ulong
}

/// `UIHotKey::GetHotKey(i)`: its record (NULL for an unused record).
#[no_mangle]
pub extern "C" fn ee_ui_hotkey_at(i: c_ulong) -> *mut EeUiHotKeyFields {
    table()[i as usize] as *mut EeUiHotKeyFields
}

#[cfg(test)]
#[path = "hotkeyobj_tests.rs"]
mod tests;
