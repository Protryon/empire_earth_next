//! The one-statement C++ shims the toolkit's moved host cases call (UI lane round 10: "calling C++ from Rust is
//! fine"). Each shim is an `extern "C"` function of ONE statement in the toolkit's own C++ file (`ee_uicpp_*`, in
//! `src/Low-Level Engine/User Interface`): a call on a C++ object (a form's virtuals, a bitmap, a texture, the
//! rasterizer, the sound manager, the database files), no logic. The logic that called them is Rust's (the `lle_*`
//! modules), generic over [`Cpp`] so that its tests run on [`Fake`], which logs every call.
//!
//! The moved logic plugs into a host before the C++ one: the C++ class hands Rust the host `ee_ui_*_host` (a Rust
//! `extern "C"` function, absent from a test binary), which answers the moved ops and passes the others to the C++
//! host (`ee_uicpp_*_host`, one statement). The goldens build their objects over their own hosts, so they do not
//! see it.
//!
//! [`shims!`] makes, from one list, the trait, the FFI implementation ([`Ffi`], absent from a test binary) and the
//! fake (the same macro as ee_game's `screens/game_cpp.rs`). A shim's arguments are plain numbers, pointers
//! (`usize`), [`EeUiShimText`] (bytes handed in) and [`EeUiShimOut`] (a text the shim appends to with
//! `ee_ui_shim_out`).

// the logic is the FFI hosts' (absent with no_cpp: a crate's tests that call the toolkit)
#![cfg_attr(feature = "no_cpp", allow(dead_code))]

use core::ffi::c_void;
#[allow(unused_imports)]
use core::ffi::{c_long, c_ulong};
use core::fmt;

use crate::obj::EeUiOp;

/// Bytes handed to a shim (valid for the call): `ee/EEScreensShims.h`'s `EeUiShimText`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUiShimText {
    pub ptr: *const u8,
    pub len: usize,
}

impl EeUiShimText {
    pub fn of(bytes: &[u8]) -> Self {
        EeUiShimText { ptr: bytes.as_ptr(), len: bytes.len() }
    }
}

impl fmt::Debug for EeUiShimText {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // SAFETY: a text lives for the call it is handed to (`of`'s slice)
        let bytes = if self.len == 0 { &[][..] } else { unsafe { core::slice::from_raw_parts(self.ptr, self.len) } };
        write!(f, "{:?}", String::from_utf8_lossy(bytes))
    }
}

/// A record handed to a shim (valid for the call; the C++ reads it as its struct): the C++ side's `EeUiShimText`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeUiShimBytes {
    pub ptr: *const u8,
    pub len: usize,
}

impl EeUiShimBytes {
    pub fn of(bytes: &[u8]) -> Self {
        EeUiShimBytes { ptr: bytes.as_ptr(), len: bytes.len() }
    }
}

impl fmt::Debug for EeUiShimBytes {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "bytes({})", self.len)
    }
}

/// A text a shim writes: the `Vec<u8>` the shim's `ee_ui_shim_out` calls append to.
#[repr(transparent)]
#[derive(Clone, Copy)]
pub struct EeUiShimOut(pub *mut c_void);

impl EeUiShimOut {
    pub fn of(out: &mut Vec<u8>) -> Self {
        EeUiShimOut(out as *mut Vec<u8> as *mut c_void)
    }
}

impl fmt::Debug for EeUiShimOut {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("out")
    }
}

/// A number a shim writes (an out argument of the call it makes: `unsigned long&`, `FSFileSpec*&`, `int32_t*`).
macro_rules! out_types {
    ($($(#[$m:meta])* $name:ident: $t:ty),*) => {$(
        $(#[$m])*
        #[repr(transparent)]
        #[derive(Clone, Copy)]
        pub struct $name(pub *mut $t);

        impl $name {
            #[allow(dead_code)]
            pub fn of(out: &mut $t) -> Self {
                $name(out)
            }
        }

        impl fmt::Debug for $name {
            fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
                f.write_str("out")
            }
        }

        impl ShimArg for $name {
            #[cfg(test)]
            fn fake(self, name: &'static str, fake: &mut Fake) {
                let v = fake.outs.get_mut(name).and_then(|q| if q.is_empty() { None } else { Some(q.remove(0)) });
                // SAFETY: the out number of the call
                unsafe { *self.0 = v.unwrap_or(0) as $t };
            }
        }
    )*};
}
out_types!(
    /// An `unsigned long&`.
    EeUiShimULongOut: core::ffi::c_ulong,
    /// A pointer out (`FSFileSpec*&`, as an address).
    EeUiShimPtrOut: usize,
    /// An `int32_t*`.
    EeUiShimI32Out: i32,
    /// A `long&`.
    EeUiShimLongOut: core::ffi::c_long,
    /// A `float*` (the C++ writes as many floats as the call says: four texture coordinates are eight).
    EeUiShimF32Out: f32,
    /// A `uint8_t*` (a record the C++ reads into).
    EeUiShimU8Out: u8
);

/// Appends `length` bytes at `text` to the shim's out text (`EeUiShimOut`).
///
/// # Safety
/// `out` is an `EeUiShimOut` of a live `Vec<u8>`; `text` holds `length` bytes (or `length` is 0).
#[no_mangle]
pub unsafe extern "C" fn ee_ui_shim_out(out: *mut c_void, text: *const u8, length: usize) {
    if out.is_null() || text.is_null() || length == 0 {
        return;
    }
    // SAFETY: the contract
    unsafe { (*(out as *mut Vec<u8>)).extend_from_slice(core::slice::from_raw_parts(text, length)) };
}

/// A shim's argument type (only the fake asks it).
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) trait ShimArg: Copy + fmt::Debug {
    /// The fake's part of the call (an out text gets the fake's next text for the shim).
    #[cfg(test)]
    fn fake(self, _name: &'static str, _fake: &mut Fake) {}
}

macro_rules! plain_args {
    ($($t:ty),*) => { $(impl ShimArg for $t {})* };
}
plain_args!(u8, u32, i32, u64, i64, usize, isize, f32, bool, EeUiShimText, EeUiShimBytes);

impl ShimArg for EeUiShimOut {
    #[cfg(test)]
    fn fake(self, name: &'static str, fake: &mut Fake) {
        let text = fake.texts.get_mut(name).and_then(|q| if q.is_empty() { None } else { Some(q.remove(0)) });
        // SAFETY: the out text of the call (a live Vec<u8>)
        unsafe { (*(self.0 as *mut Vec<u8>)).extend_from_slice(&text.unwrap_or_default()) };
    }
}

/// A shim's answer type (the fake's answers are i64s).
#[cfg_attr(not(test), allow(dead_code))]
pub(crate) trait ShimRet {
    fn from_fake(v: i64) -> Self;
}

macro_rules! rets {
    ($($t:ty),*) => { $(impl ShimRet for $t { fn from_fake(v: i64) -> Self { v as $t } })* };
}
rets!(u8, u32, i32, u64, i64, usize, isize, f32);

impl ShimRet for () {
    fn from_fake(_: i64) -> Self {}
}

impl ShimRet for bool {
    fn from_fake(v: i64) -> Self {
        v != 0
    }
}

/// The recording fake: every call logged as `name(args)`; answers and out texts from per-shim queues (0 and an
/// empty text once a queue is empty).
#[cfg(test)]
#[derive(Default)]
pub(crate) struct Fake {
    pub log: Vec<String>,
    pub answers: std::collections::BTreeMap<&'static str, Vec<i64>>,
    pub texts: std::collections::BTreeMap<&'static str, Vec<Vec<u8>>>,
    pub outs: std::collections::BTreeMap<&'static str, Vec<i64>>,
}

#[cfg(test)]
impl Fake {
    /// The shim `name` answers `values`, one per call.
    pub fn answer(&mut self, name: &'static str, values: &[i64]) -> &mut Self {
        self.answers.entry(name).or_default().extend_from_slice(values);
        self
    }
    /// The shim `name`'s out text is `text` (one per call).
    #[allow(dead_code)]
    pub fn text(&mut self, name: &'static str, text: &[u8]) -> &mut Self {
        self.texts.entry(name).or_default().push(text.to_vec());
        self
    }
    /// The shim `name`'s out number is `value` (one per call).
    pub fn out(&mut self, name: &'static str, value: i64) -> &mut Self {
        self.outs.entry(name).or_default().push(value);
        self
    }
    fn next(&mut self, name: &'static str) -> i64 {
        match self.answers.get_mut(name) {
            Some(q) if !q.is_empty() => q.remove(0),
            _ => 0,
        }
    }
    /// The log, each call on its own line.
    pub fn calls(&self) -> String {
        self.log.join("\n")
    }
}

/// One call of a host (`EeUiHostFn`'s arguments): `obj` the C++ object, `f` its float buffer (four floats, twelve
/// for a wide op).
pub struct HostArgs {
    pub obj: usize,
    pub op: EeUiOp,
    pub a: isize,
    pub b: isize,
    pub c: isize,
    pub d: isize,
    pub f: *mut f32,
}

impl HostArgs {
    /// A call over a local buffer (the tests').
    pub fn new(obj: usize, op: EeUiOp, abcd: [isize; 4], f: &mut [f32]) -> Self {
        HostArgs { obj, op, a: abcd[0], b: abcd[1], c: abcd[2], d: abcd[3], f: f.as_mut_ptr() }
    }
    /// `f[i]` (0 without a buffer).
    pub fn f(&self, i: usize) -> f32 {
        if self.f.is_null() {
            return 0.0;
        }
        // SAFETY: the host's buffer holds the floats its op reads
        unsafe { *self.f.add(i) }
    }
    /// `f[i] = v`.
    pub fn set_f(&self, i: usize, v: f32) {
        if !self.f.is_null() {
            // SAFETY: the host's buffer holds the floats its op writes
            unsafe { *self.f.add(i) = v }
        }
    }
    /// `f[i]`'s bits set to `bits` (a number answered in the float buffer).
    pub fn set_f_bits(&self, i: usize, bits: u32) {
        self.set_f(i, f32::from_bits(bits));
    }
    /// `a` as an address.
    pub fn pa(&self) -> usize {
        self.a as usize
    }
    /// `b` as an address.
    pub fn pb(&self) -> usize {
        self.b as usize
    }
    /// The four floats from `f[0]` (a rectangle: bottom, left, right, top).
    pub fn f4(&self) -> [f32; 4] {
        [self.f(0), self.f(1), self.f(2), self.f(3)]
    }
}

/// A host that answers the moved ops (`$call`, `fn(&mut impl Cpp, &HostArgs) -> Option<isize>`) and passes the
/// others to the C++ host `$cpp` (the class's `HostOp`, through one statement). Declared for C++ in `lle/UIShims.h`.
macro_rules! lle_host {
    ($(#[$m:meta])* $name:ident => $call:path, $cpp:ident) => {
        $(#[$m])*
        ///
        /// # Safety
        /// The C++ class's host contract: `obj` its live object, `f` the live floats its op reads and writes.
        #[cfg(not(any(test, feature = "no_cpp")))]
        #[no_mangle]
        pub unsafe extern "C" fn $name(
            obj: *mut core::ffi::c_void,
            op: crate::obj::EeUiOp,
            a: isize,
            b: isize,
            c: isize,
            d: isize,
            f: *mut f32,
        ) -> isize {
            extern "C" {
                fn $cpp(
                    obj: *mut core::ffi::c_void,
                    op: crate::obj::EeUiOp,
                    a: isize,
                    b: isize,
                    c: isize,
                    d: isize,
                    f: *mut f32,
                ) -> isize;
            }
            let args = crate::lle_cpp::HostArgs { obj: obj as usize, op, a, b, c, d, f };
            match $call(&mut crate::lle_cpp::Ffi, &args) {
                Some(r) => r,
                // SAFETY: the C++ host's own contract, the arguments as given
                None => unsafe { $cpp(obj, op, a, b, c, d, f) },
            }
        }
    };
}
pub(crate) use lle_host;

/// The FFI implementation of [`Cpp`]: the shims themselves.
#[cfg(not(any(test, feature = "no_cpp")))]
pub(crate) struct Ffi;

macro_rules! shims {
    ($( fn $name:ident ( $($arg:ident : $ty:ty),* ) $(-> $ret:ty)? = $sym:ident ; )*) => {
        /// The C++ shims (one statement each).
        #[allow(clippy::too_many_arguments)]
        #[cfg_attr(any(test, feature = "no_cpp"), allow(dead_code))]
        pub(crate) trait Cpp {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)?; )*
        }

        #[cfg(not(any(test, feature = "no_cpp")))]
        mod sys {
            #[allow(unused_imports)]
            use super::*;
            extern "C" {
                $( pub fn $sym($($arg: $ty),*) $(-> $ret)?; )*
            }
        }

        #[cfg(not(any(test, feature = "no_cpp")))]
        impl Cpp for Ffi {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                // SAFETY: the shim's own contract: the pointers the moved logic hands it are the C++ host's live
                // objects, as the C++ case had them
                unsafe { sys::$sym($($arg),*) }
            } )*
        }

        #[cfg(test)]
        impl Cpp for Fake {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)? {
                let args: Vec<String> = vec![$(format!("{:?}", $arg)),*];
                $( ShimArg::fake($arg, stringify!($name), self); )*
                self.log.push(format!("{}({})", stringify!($name), args.join(", ")));
                ShimRet::from_fake(self.next(stringify!($name)))
            } )*
        }
    };
}

shims! {
    // UIManager.cpp's (lle_manager.rs); the events file's count is UIHotKey.cpp's
    fn file_read_count(file: usize, count: EeUiShimULongOut) -> c_long = ee_uicpp_file_read_count;
    fn mgr_screen(m: usize) -> u32 = ee_uicpp_mgr_screen;
    fn mgr_rasterizer_screen(m: usize) = ee_uicpp_mgr_rasterizer_screen;
    fn mgr_window(m: usize) -> usize = ee_uicpp_mgr_window;
    fn window_origin_x(w: usize) -> f32 = ee_uicpp_window_origin_x;
    fn window_origin_y(w: usize) -> f32 = ee_uicpp_window_origin_y;
    fn window_width(w: usize) -> u32 = ee_uicpp_window_width;
    fn window_height(w: usize) -> u32 = ee_uicpp_window_height;
    fn mgr_blending(m: usize, source: EeUiShimULongOut, destination: EeUiShimULongOut) = ee_uicpp_mgr_blending;
    fn key_copy(key: usize) = ee_uicpp_key_copy;
    fn form_translated_key(form: usize, repeat: i32) -> c_long = ee_uicpp_form_translated_key;
    fn event_reference(event: usize) = ee_uicpp_event_reference;
    fn event_dereference(event: usize) = ee_uicpp_event_dereference;
    fn callback_manager(callback: usize, m: usize, id: u32) -> usize = ee_uicpp_callback_manager;
    fn callback_form(callback: usize, form: usize, id: u32) -> usize = ee_uicpp_callback_form;
    fn mgr_texture_manager(m: usize) -> usize = ee_uicpp_mgr_texture_manager;
    fn mgr_ui_depth(m: usize) -> u32 = ee_uicpp_mgr_ui_depth;
    fn texture_load_ui(manager: usize, name: EeUiShimText, depth: u32, alpha: bool) -> usize = ee_uicpp_texture_load_ui;
    fn texture_reference(t: usize) = ee_uicpp_texture_reference;
    fn texture_stage_alpha(t: usize, mode: u32) = ee_uicpp_texture_stage_alpha;
    fn texture_stage_color(t: usize, mode: u32) = ee_uicpp_texture_stage_color;
    fn texture_stage_min(t: usize, mode: u32) = ee_uicpp_texture_stage_min;
    fn texture_stage_mag(t: usize, mode: u32) = ee_uicpp_texture_stage_mag;
    fn texture_stage_mip(t: usize, mode: u32) = ee_uicpp_texture_stage_mip;
    fn texture_stage_bias(t: usize, bias: f32) = ee_uicpp_texture_stage_bias;
    fn texture_stage_address(t: usize, mode: u32) = ee_uicpp_texture_stage_address;
    fn texture_width(t: usize) -> u32 = ee_uicpp_texture_width;
    fn texture_height(t: usize) -> u32 = ee_uicpp_texture_height;
    fn bitmap_new() -> usize = ee_uicpp_bitmap_new;
    fn bitmap_set_texture(b: usize, t: usize, screen: u32) = ee_uicpp_bitmap_set_texture;
    fn bitmap_texture(b: usize, screen: u32) -> usize = ee_uicpp_bitmap_texture;
    fn bitmap_set_opacity(b: usize, opacity: f32) = ee_uicpp_bitmap_set_opacity;
    fn bitmap_uvs(b: usize, uvs: EeUiShimF32Out) = ee_uicpp_bitmap_uvs;
    fn bitmap_set_uvs(b: usize, u0: f32, v0: f32, u1: f32, v1: f32, u2: f32, v2: f32, u3: f32, v3: f32) = ee_uicpp_bitmap_set_uvs;
    fn lock_create(lock: EeUiShimPtrOut) -> bool = ee_uicpp_lock_create;
    fn es_verify(line: u32, expression: EeUiShimText) = ee_uicpp_es_verify;
    fn instance_reference(instance: usize) = ee_uicpp_instance_reference;
    fn instance_dereference(instance: usize) = ee_uicpp_instance_dereference;
    fn table_get(index: u32) -> usize = ee_uicpp_table_get;
    fn mgr_form_by_id(m: usize, id: u32) -> usize = ee_uicpp_mgr_form_by_id;
    fn table_add_event(table: usize, id: u32, kind: u32) = ee_uicpp_table_add_event;
    fn form_add_event(form: usize, id: u32, kind: u32) = ee_uicpp_form_add_event;
    fn db_find(path: usize, directory: usize, table: u32, file: EeUiShimPtrOut) -> c_long = ee_uicpp_db_find;
    fn db_fonts(file: usize, m: usize) -> c_long = ee_uicpp_db_fonts;
    fn db_forms(file: usize, m: usize) -> c_long = ee_uicpp_db_forms;
    fn db_backgrounds(file: usize) -> c_long = ee_uicpp_db_backgrounds;
    fn db_create_backgrounds(m: usize) = ee_uicpp_db_create_backgrounds;
    fn db_controls(file: usize, m: usize) = ee_uicpp_db_controls;
    fn db_control_events(file: usize) = ee_uicpp_db_control_events;
    fn db_form_events(file: usize, m: usize) = ee_uicpp_db_form_events;
    fn db_hot_keys(file: usize) = ee_uicpp_db_hot_keys;
    fn file_delete(file: usize) = ee_uicpp_file_delete;
    fn db_remove_controls() = ee_uicpp_db_remove_controls;
    fn db_remove_backgrounds() = ee_uicpp_db_remove_backgrounds;
    fn db_remove_fonts() = ee_uicpp_db_remove_fonts;
    fn mouse_buttons_ready() -> bool = ee_uicpp_mouse_buttons_ready;
    fn mouse_button_fetch(which: u32) = ee_uicpp_mouse_button_fetch;
    fn mouse_left_handed() -> bool = ee_uicpp_mouse_left_handed;
    fn mouse_swap_buttons() = ee_uicpp_mouse_swap_buttons;
    fn mouse_set_left_handed(left: bool) = ee_uicpp_mouse_set_left_handed;
    // UIForm.cpp's (lle_form.rs); the file's open, read and close are UIHotKey.cpp's
    fn file_open(file: usize) = ee_uicpp_file_open;
    fn file_read(file: usize, data: EeUiShimU8Out, length: c_ulong) -> c_long = ee_uicpp_file_read;
    fn file_close(file: usize) = ee_uicpp_file_close;
    fn form_help_clear() = ee_uicpp_form_help_clear;
    fn control_help_string(control: usize) -> bool = ee_uicpp_control_help_string;
    fn hot_key_set_just_pressed(key: usize, event: usize) = ee_uicpp_hot_key_set_just_pressed;
    fn hot_key_set_pressed(key: usize, event: usize) = ee_uicpp_hot_key_set_pressed;
    fn hot_key_set_just_unpressed(key: usize, event: usize) = ee_uicpp_hot_key_set_just_unpressed;
    fn hot_key_set_double_pressed(key: usize, event: usize) = ee_uicpp_hot_key_set_double_pressed;
    fn form_fetch_tab_key() = ee_uicpp_form_fetch_tab_key;
    fn form_fetch_shift_key() = ee_uicpp_form_fetch_shift_key;
    fn event_uninitialize(event: usize) = ee_uicpp_event_uninitialize;
    fn event_initialize(event: usize) = ee_uicpp_event_initialize;
    fn entity_value(entity: usize) -> u32 = ee_uicpp_entity_value;
    fn entity_just_pressed(entity: usize) -> bool = ee_uicpp_entity_just_pressed;
    fn entity_just_unpressed(entity: usize) -> bool = ee_uicpp_entity_just_unpressed;
    fn entity_just_double_pressed(entity: usize) -> bool = ee_uicpp_entity_just_double_pressed;
    fn form_screen(form: usize) -> u32 = ee_uicpp_form_screen;
    fn form_keyboard_captured(form: usize) -> usize = ee_uicpp_form_keyboard_captured;
    fn control_ime_result(control: usize, text: usize) = ee_uicpp_control_ime_result;
    fn control_ime_change(control: usize, text: usize, attributes: usize, cursor: u32) -> c_long = ee_uicpp_control_ime_change;
    fn control_ime_close(control: usize) -> c_long = ee_uicpp_control_ime_close;
    fn control_ime_open(control: usize) -> c_long = ee_uicpp_control_ime_open;
    fn control_translated_key(control: usize, key: usize, repeat: i32) -> c_long = ee_uicpp_control_translated_key;
    fn mgr_create_form(m: usize, data: EeUiShimBytes) -> usize = ee_uicpp_mgr_create_form;
    fn mgr_add_form(m: usize, form: usize) = ee_uicpp_mgr_add_form;
    fn form_enable_group0(form: usize) = ee_uicpp_form_enable_group0;
    // UIControl.cpp's (lle_control.rs, mouse_ffi.rs)
    fn control_keys_ready() -> bool = ee_uicpp_control_keys_ready;
    fn control_key_fetch(slot: u32, code: u32) = ee_uicpp_control_key_fetch;
    fn mouse_l_button() -> usize = ee_uicpp_mouse_l_button;
    fn mouse_r_button() -> usize = ee_uicpp_mouse_r_button;
    fn control_left_down(control: usize) -> bool = ee_uicpp_control_left_down;
    fn control_right_down(control: usize) -> bool = ee_uicpp_control_right_down;
    fn control_set_left_down(control: usize, down: bool) = ee_uicpp_control_set_left_down;
    fn control_set_right_down(control: usize, down: bool) = ee_uicpp_control_set_right_down;
    fn control_mouse_left_double(control: usize) -> c_long = ee_uicpp_control_mouse_left_double;
    fn control_mouse_left_up(control: usize) -> c_long = ee_uicpp_control_mouse_left_up;
    fn control_mouse_left_down(control: usize) -> c_long = ee_uicpp_control_mouse_left_down;
    fn control_mouse_right_up(control: usize) -> c_long = ee_uicpp_control_mouse_right_up;
    fn control_mouse_right_down(control: usize) -> c_long = ee_uicpp_control_mouse_right_down;
    fn control_mouse_move(control: usize, location: usize) -> c_long = ee_uicpp_control_mouse_move;
    // UIControlsTable.cpp's (lle_table.rs)
    fn form_add_database_control(form: usize, index: u32) = ee_uicpp_form_add_database_control;
    fn string_new() -> usize = ee_uicpp_string_new;
    fn mgr_string(m: usize, id: u32, string: usize) = ee_uicpp_mgr_string;
    fn strings_new() -> usize = ee_uicpp_strings_new;
    fn strings_clear(strings: usize) = ee_uicpp_strings_clear;
    fn strings_resize(strings: usize, count: u32) = ee_uicpp_strings_resize;
    fn strings_count(strings: usize) -> u32 = ee_uicpp_strings_count;
    fn mgr_string_at(m: usize, id: i32, strings: usize, index: u32) = ee_uicpp_mgr_string_at;
    fn table_string_begin(t: usize) -> i32 = ee_uicpp_table_string_begin;
    fn table_string_end(t: usize) -> i32 = ee_uicpp_table_string_end;
    fn table_bitmap_normal(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_normal;
    fn table_bitmap_pressed(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_pressed;
    fn table_bitmap_focused(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_focused;
    fn bitmap_dereference(b: usize) = ee_uicpp_bitmap_dereference;
    fn file_list_add_extension(control: usize, strings: usize, index: u32) = ee_uicpp_file_list_add_extension;
    fn radio_set_down_unchecked(control: usize, bitmap: usize) = ee_uicpp_radio_set_down_unchecked;
    fn radio_set_down_checked(control: usize, bitmap: usize) = ee_uicpp_radio_set_down_checked;
    fn radio_set_up_checked(control: usize, bitmap: usize) = ee_uicpp_radio_set_up_checked;
    fn mgr_texture_manager_by_screen(m: usize) -> usize = ee_uicpp_mgr_texture_manager_by_screen;
    fn texture_load_ui_string(manager: usize, name: usize, depth: u32, alpha: bool) -> usize = ee_uicpp_texture_load_ui_string;
    fn bitmap_stretch(b: usize, rect: usize, texture_rect: usize, screen: u32) = ee_uicpp_bitmap_stretch;
    // UIComboBox.cpp's (lle_combo.rs)
    fn combo_part(combo: usize, part: u32) -> usize = ee_uicpp_combo_part;
    fn list_scroll_bar(list: usize) -> usize = ee_uicpp_list_scroll_bar;
    fn control_rect(control: usize, rect: EeUiShimF32Out) = ee_uicpp_control_rect;
    fn list_selection(list: usize, selection: EeUiShimULongOut) -> c_long = ee_uicpp_list_selection;
    fn control_mouse_x(control: usize) -> f32 = ee_uicpp_control_mouse_x;
    fn control_mouse_y(control: usize) -> f32 = ee_uicpp_control_mouse_y;
    fn key_down_pressed() -> bool = ee_uicpp_key_down_pressed;
    fn key_up_pressed() -> bool = ee_uicpp_key_up_pressed;
    fn key_home_pressed() -> bool = ee_uicpp_key_home_pressed;
    fn key_end_pressed() -> bool = ee_uicpp_key_end_pressed;
    fn key_enter_value() -> bool = ee_uicpp_key_enter_value;
    fn label_set_text_line(label: usize, list: usize, line: u32) = ee_uicpp_label_set_text_line;
    fn edit_set_text_line(edit: usize, list: usize, line: u32) = ee_uicpp_edit_set_text_line;
    fn label_clear_text(label: usize) = ee_uicpp_label_clear_text;
    fn edit_clear_text(edit: usize) = ee_uicpp_edit_clear_text;
    fn label_delete_first_line(label: usize) = ee_uicpp_label_delete_first_line;
    fn edit_delete_text(edit: usize) = ee_uicpp_edit_delete_text;
    fn control_tabbed_to(control: usize) = ee_uicpp_control_tabbed_to;
    fn control_tabbed_from(control: usize) = ee_uicpp_control_tabbed_from;
    fn label_text(label: usize) -> usize = ee_uicpp_label_text;
    fn edit_text(edit: usize) -> usize = ee_uicpp_edit_text;
    fn list_text(list: usize) -> usize = ee_uicpp_list_text;
    fn text_copy_fg(to: usize, from: usize) = ee_uicpp_text_copy_fg;
    fn text_copy_bg(to: usize, from: usize) = ee_uicpp_text_copy_bg;
    fn text_copy_transparent(to: usize, from: usize) = ee_uicpp_text_copy_transparent;
    fn combo_process_down(combo: usize) = ee_uicpp_combo_process_down;
    fn combo_process_up(combo: usize) = ee_uicpp_combo_process_up;
    fn form_scaled_manager_bitmap(form: usize, index: u32, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_form_scaled_manager_bitmap;
    fn combo_new_drop(combo: usize, normal: usize, pressed: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_combo_new_drop;
    fn control_width(control: usize) -> u32 = ee_uicpp_control_width;
    fn combo_new_label(combo: usize, font: usize, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_label;
    fn combo_new_edit(combo: usize, font: usize, max_chars: c_ulong, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_edit;
    fn combo_new_list(combo: usize, data: usize, font: usize, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_list;
    fn list_selected_color(list: usize, rgb: EeUiShimF32Out) = ee_uicpp_list_selected_color;
    fn list_line_count(list: usize) -> u32 = ee_uicpp_list_line_count;
    fn combo_set_rust(combo: usize, rust: usize) = ee_uicpp_combo_set_rust;
    fn list_change_selection(list: usize, index: u32, a: bool, b: bool) = ee_uicpp_list_change_selection;
    fn control_set_bg_opacity(control: usize, opacity: f32) = ee_uicpp_control_set_bg_opacity;
    fn control_set_bg_color(control: usize, red: f32, green: f32, blue: f32) = ee_uicpp_control_set_bg_color;
    fn list_set_scroll_bg_opacity(list: usize, opacity: f32) = ee_uicpp_list_set_scroll_bg_opacity;
    fn list_set_scroll_bg_color(list: usize, red: f32, green: f32, blue: f32) = ee_uicpp_list_set_scroll_bg_color;
    fn combo_base_set_rect(combo: usize, text_box: usize, drop: usize) = ee_uicpp_combo_base_set_rect;
    // UI lane round 12: the backgrounds' and edit boxes' moved host cases (lle_bg_edit.rs)
    fn bg_texture_load(record: usize, name: EeUiShimText, description: usize) -> usize = ee_uicpp_bg_texture_load;
    fn edit_key_fetch(which: u32, out: EeUiShimPtrOut) = ee_uicpp_edit_key_fetch;
    fn edit_scroll_bar_new(edit: usize, x: f32, y: f32, length: f32) -> usize = ee_uicpp_edit_scroll_bar_new;
    fn edit_form_mouse_x(edit: usize) -> f32 = ee_uicpp_edit_form_mouse_x;
    fn edit_form_mouse_y(edit: usize) -> f32 = ee_uicpp_edit_form_mouse_y;
    fn line_new_black() -> usize = ee_uicpp_line_new_black;
    fn line_new() -> usize = ee_uicpp_line_new;
    fn text_fg_red(text: usize) -> f32 = ee_uicpp_text_fg_red;
    fn text_fg_green(text: usize) -> f32 = ee_uicpp_text_fg_green;
    fn text_fg_blue(text: usize) -> f32 = ee_uicpp_text_fg_blue;
}
