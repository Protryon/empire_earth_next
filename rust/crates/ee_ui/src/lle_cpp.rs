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

/// A host's call (`ee_ui_*_host`, declared for C++ by their modules' generated headers): the moved ops are `call`'s
/// (`fn(&mut impl Cpp, &HostArgs) -> Option<isize>`), the others the class's tables' (`lle_hosts.rs`: the C++ cases'
/// one-statement shims).
///
/// # Safety
/// The C++ class's host contract: `obj` its live object, `f` the live floats its op reads and writes.
#[cfg(not(any(test, feature = "no_cpp")))]
#[allow(clippy::too_many_arguments)]
pub(crate) unsafe fn host(
    call: fn(&mut Ffi, &HostArgs) -> Option<isize>,
    tables: &[crate::lle_hosts::Table],
    obj: *mut c_void,
    op: EeUiOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    f: *mut f32,
) -> isize {
    let args = HostArgs { obj: obj as usize, op, a, b, c, d, f };
    match call(&mut Ffi, &args) {
        Some(r) => r,
        // SAFETY: the host contract, the arguments as given
        None => unsafe { crate::lle_hosts::chain(tables, obj, op, a, b, c, d, f) },
    }
}

/// The FFI implementation of [`Cpp`]: the shims themselves.
#[cfg(not(any(test, feature = "no_cpp")))]
pub(crate) struct Ffi;

macro_rules! shims {
    ($( fn $name:ident ( $($arg:ident : $ty:ty),* ) $(-> $ret:ty)? = $sym:ident ; )*
     rust: $( fn $rname:ident ( $($rarg:ident : $rty:ty),* ) $(-> $rret:ty)? = $rsym:path ; )*) => {
        /// The C++ shims (one statement each), and the calls that were one but are Rust's own now (`rust:`, the
        /// Rust objects of other crates the shim called: [`own`]); the fake logs both alike.
        #[allow(clippy::too_many_arguments)]
        #[cfg_attr(any(test, feature = "no_cpp"), allow(dead_code))]
        pub(crate) trait Cpp {
            $( fn $name(&mut self, $($arg: $ty),*) $(-> $ret)?; )*
            $( fn $rname(&mut self, $($rarg: $rty),*) $(-> $rret)?; )*
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
            $( #[allow(unused_unsafe)]
            fn $rname(&mut self, $($rarg: $rty),*) $(-> $rret)? {
                // SAFETY: as the shims': the moved logic's pointers are the live objects the C++ shim had
                unsafe { $rsym($($rarg),*) }
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
            $( fn $rname(&mut self, $($rarg: $rty),*) $(-> $rret)? {
                let args: Vec<String> = vec![$(format!("{:?}", $rarg)),*];
                $( ShimArg::fake($rarg, stringify!($rname), self); )*
                self.log.push(format!("{}({})", stringify!($rname), args.join(", ")));
                ShimRet::from_fake(self.next(stringify!($rname)))
            } )*
        }
    };
}

shims! {
    // UIManager.cpp's (lle_manager.rs); the events file's count is UIHotKey.cpp's
    fn file_read_count(file: usize, count: EeUiShimULongOut) -> c_long = ee_uicpp_file_read_count;
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
    fn es_verify(line: u32, expression: EeUiShimText) = ee_uicpp_es_verify;
    fn instance_reference(instance: usize) = ee_uicpp_instance_reference;
    fn instance_dereference(instance: usize) = ee_uicpp_instance_dereference;
    fn db_find(path: usize, directory: usize, name: EeUiShimText, file: EeUiShimPtrOut) -> c_long = ee_uicpp_db_find;
    fn file_delete(file: usize) = ee_uicpp_file_delete;
    fn mouse_left_handed() -> bool = ee_ui_mouse_left_handed;
    fn mouse_set_left_handed(left: bool) = ee_ui_mouse_set_left_handed;
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
    fn event_uninitialize(event: usize) = ee_uicpp_event_uninitialize;
    fn event_initialize(event: usize) = ee_uicpp_event_initialize;
    fn control_ime_result(control: usize, text: usize) = ee_uicpp_control_ime_result;
    fn control_ime_change(control: usize, text: usize, attributes: usize, cursor: u32) -> c_long = ee_uicpp_control_ime_change;
    fn control_ime_close(control: usize) -> c_long = ee_uicpp_control_ime_close;
    fn control_ime_open(control: usize) -> c_long = ee_uicpp_control_ime_open;
    fn control_translated_key(control: usize, key: usize, repeat: i32) -> c_long = ee_uicpp_control_translated_key;
    fn mgr_create_form(m: usize, data: EeUiShimBytes) -> usize = ee_uicpp_mgr_create_form;
    // UIControl.cpp's (lle_control.rs, mouse_ffi.rs)
    fn control_mouse_left_double(control: usize) -> c_long = ee_uicpp_control_mouse_left_double;
    fn control_mouse_left_up(control: usize) -> c_long = ee_uicpp_control_mouse_left_up;
    fn control_mouse_left_down(control: usize) -> c_long = ee_uicpp_control_mouse_left_down;
    fn control_mouse_right_up(control: usize) -> c_long = ee_uicpp_control_mouse_right_up;
    fn control_mouse_right_down(control: usize) -> c_long = ee_uicpp_control_mouse_right_down;
    fn control_mouse_move(control: usize, location: usize) -> c_long = ee_uicpp_control_mouse_move;
    // UIControlsTable.cpp's (lle_table.rs)
    fn string_new() -> usize = ee_uicpp_string_new;
    fn mgr_string(m: usize, id: u32, string: usize) = ee_uicpp_mgr_string;
    fn strings_new() -> usize = ee_uicpp_strings_new;
    fn strings_clear(strings: usize) = ee_uicpp_strings_clear;
    fn strings_resize(strings: usize, count: u32) = ee_uicpp_strings_resize;
    fn strings_count(strings: usize) -> u32 = ee_uicpp_strings_count;
    fn mgr_string_at(m: usize, id: i32, strings: usize, index: u32) = ee_uicpp_mgr_string_at;
    fn table_bitmap_normal(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_normal;
    fn table_bitmap_pressed(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_pressed;
    fn table_bitmap_focused(t: usize, m: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_table_bitmap_focused;
    fn bitmap_dereference(b: usize) = ee_uicpp_bitmap_dereference;
    fn file_list_add_extension(control: usize, strings: usize, index: u32) = ee_uicpp_file_list_add_extension;
    fn mgr_texture_manager_by_screen(m: usize) -> usize = ee_uicpp_mgr_texture_manager_by_screen;
    fn texture_load_ui_string(manager: usize, name: usize, depth: u32, alpha: bool) -> usize = ee_uicpp_texture_load_ui_string;
    fn bitmap_stretch(b: usize, rect: usize, texture_rect: usize, screen: u32) = ee_uicpp_bitmap_stretch;
    // UIComboBox.cpp's (lle_combo.rs)
    fn label_set_text_line(label: usize, list: usize, line: u32) = ee_uicpp_label_set_text_line;
    fn edit_set_text_line(edit: usize, list: usize, line: u32) = ee_uicpp_edit_set_text_line;
    fn label_clear_text(label: usize) = ee_uicpp_label_clear_text;
    fn edit_clear_text(edit: usize) = ee_uicpp_edit_clear_text;
    fn label_delete_first_line(label: usize) = ee_uicpp_label_delete_first_line;
    fn edit_delete_text(edit: usize) = ee_uicpp_edit_delete_text;
    fn control_tabbed_to(control: usize) = ee_uicpp_control_tabbed_to;
    fn control_tabbed_from(control: usize) = ee_uicpp_control_tabbed_from;
    fn list_text(list: usize) -> usize = ee_uicpp_list_text;
    fn combo_process_down(combo: usize) = ee_uicpp_combo_process_down;
    fn combo_process_up(combo: usize) = ee_uicpp_combo_process_up;
    fn combo_new_drop(combo: usize, normal: usize, pressed: usize, left: f32, top: f32, right: f32, bottom: f32) -> usize = ee_uicpp_combo_new_drop;
    fn combo_new_label(combo: usize, font: usize, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_label;
    fn combo_new_edit(combo: usize, font: usize, max_chars: c_ulong, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_edit;
    fn combo_new_list(combo: usize, data: usize, font: usize, left: f32, top: f32, right: f32, bottom: f32, border: c_ulong) -> usize = ee_uicpp_combo_new_list;
    fn list_selected_color(list: usize, rgb: EeUiShimF32Out) = ee_uicpp_list_selected_color;
    fn list_line_count(list: usize) -> u32 = ee_uicpp_list_line_count;
    fn combo_set_rust(combo: usize, rust: usize) = ee_uicpp_combo_set_rust;
    fn list_change_selection(list: usize, index: u32, a: bool, b: bool) = ee_uicpp_list_change_selection;
    fn control_set_bg_opacity(control: usize, opacity: f32) = ee_uicpp_control_set_bg_opacity;
    fn control_set_bg_color(control: usize, red: f32, green: f32, blue: f32) = ee_uicpp_control_set_bg_color;
    // UI lane round 12: the backgrounds' and edit boxes' moved host cases (lle_bg_edit.rs)
    fn bg_texture_load(record: usize, name: EeUiShimText, description: usize) -> usize = ee_uicpp_bg_texture_load;
    fn edit_scroll_bar_new(edit: usize, x: f32, y: f32, length: f32) -> usize = ee_uicpp_edit_scroll_bar_new;
    fn line_new_black() -> usize = ee_uicpp_line_new_black;
    fn line_new() -> usize = ee_uicpp_line_new;
    rust:
    // the input entities, the key slots, the texts (LLE/UI round 2: their shims were one call on a Rust object)
    fn mouse_buttons_ready() -> bool = own::mouse_buttons_ready;
    fn mouse_swap_buttons() = own::mouse_swap_buttons;
    fn entity_value(entity: usize) -> u32 = own::entity_value;
    fn entity_just_pressed(entity: usize) -> bool = own::entity_just_pressed;
    fn entity_just_unpressed(entity: usize) -> bool = own::entity_just_unpressed;
    fn entity_just_double_pressed(entity: usize) -> bool = own::entity_just_double_pressed;
    fn control_keys_ready() -> bool = own::control_keys_ready;
    fn mouse_l_button() -> usize = own::mouse_l_button;
    fn mouse_r_button() -> usize = own::mouse_r_button;
    fn key_down_pressed() -> bool = own::key_down_pressed;
    fn key_up_pressed() -> bool = own::key_up_pressed;
    fn key_home_pressed() -> bool = own::key_home_pressed;
    fn key_end_pressed() -> bool = own::key_end_pressed;
    fn key_enter_value() -> bool = own::key_enter_value;
    fn text_copy_fg(to: usize, from: usize) = own::text_copy_fg;
    fn text_copy_bg(to: usize, from: usize) = own::text_copy_bg;
    fn text_copy_transparent(to: usize, from: usize) = own::text_copy_transparent;
    fn text_fg_red(text: usize) -> f32 = own::text_fg_red;
    fn text_fg_green(text: usize) -> f32 = own::text_fg_green;
    fn text_fg_blue(text: usize) -> f32 = own::text_fg_blue;
    // ui13a: the key predicates (lle_control.rs key_pressed)
    fn entity_long_value(entity: usize) -> c_long = own::entity_long_value;
    fn num_lock_off() -> bool = own::num_lock_off;
    // the keyboard's and mouse's entities into the key slots (LLE/UI round 3: gISKeyboard, gISMouse are ee_base's)
    fn mouse_button_fetch(which: u32, button: u32) = own::mouse_button_fetch;
    fn form_fetch_tab_key() = own::form_fetch_tab_key;
    fn form_fetch_shift_key() = own::form_fetch_shift_key;
    fn control_key_fetch(slot: u32, code: u32) = own::control_key_fetch;
    fn keyboard_entity(key: u32, out: EeUiShimPtrOut) = own::keyboard_entity;
    // the controls' button-down members (their records', lle_hosts.rs fields)
    fn control_left_down(control: usize) -> bool = crate::lle_hosts::control_left_down;
    fn control_right_down(control: usize) -> bool = crate::lle_hosts::control_right_down;
    fn control_set_left_down(control: usize, down: bool) = crate::lle_hosts::control_set_left_down;
    fn control_set_right_down(control: usize, down: bool) = crate::lle_hosts::control_set_right_down;
    // the manager's UI lock (TSScheduler::CreateCriticalSection: ee_base's section)
    fn lock_create(lock: EeUiShimPtrOut) -> bool = own::lock_create;
    // the forms', managers' and controls' chains of forwarders (LLE/UI round 2 pass 2: lle_chains.rs)
    fn mgr_screen(m: usize) -> u32 = crate::lle_hosts::chains::mgr_screen;
    fn form_screen(form: usize) -> u32 = crate::lle_hosts::chains::form_screen;
    fn mgr_ui_depth(m: usize) -> u32 = crate::lle_hosts::chains::mgr_ui_depth;
    fn mgr_form_by_id(m: usize, id: u32) -> usize = crate::lle_hosts::chains::mgr_form_by_id;
    fn mgr_add_form(m: usize, form: usize) = crate::lle_hosts::chains::mgr_add_form;
    fn form_add_event(form: usize, id: u32, kind: u32) = crate::lle_hosts::chains::form_add_event;
    fn form_keyboard_captured(form: usize) -> usize = crate::lle_hosts::chains::form_keyboard_captured;
    fn form_enable_group0(form: usize) = crate::lle_hosts::chains::form_enable_group0;
    fn form_add_database_control(form: usize, index: u32) = crate::lle_hosts::chains::form_add_database_control;
    fn control_rect(control: usize, rect: EeUiShimF32Out) = crate::lle_hosts::chains::control_rect;
    fn control_mouse_x(control: usize) -> f32 = crate::lle_hosts::chains::control_mouse_x;
    fn control_mouse_y(control: usize) -> f32 = crate::lle_hosts::chains::control_mouse_y;
    fn control_width(control: usize) -> u32 = crate::lle_hosts::chains::control_width;
    fn form_scaled_manager_bitmap(form: usize, index: u32, left: f32, top: f32, right: f32, bottom: f32) -> usize = crate::lle_hosts::chains::form_scaled_manager_bitmap;
    fn edit_form_mouse_x(edit: usize) -> f32 = crate::lle_hosts::chains::control_mouse_x;
    fn edit_form_mouse_y(edit: usize) -> f32 = crate::lle_hosts::chains::control_mouse_y;
    fn table_string_begin(t: usize) -> i32 = crate::lle_hosts::chains::table_string_begin;
    fn table_string_end(t: usize) -> i32 = crate::lle_hosts::chains::table_string_end;
    fn table_get(index: u32) -> usize = crate::lle_hosts::chains::table_get;
    fn table_add_event(table: usize, id: u32, kind: u32) = crate::lle_hosts::chains::table_add_event;
    fn db_fonts(file: usize, m: usize) -> c_long = crate::lle_hosts::chains::db_fonts;
    fn db_forms(file: usize, m: usize) -> c_long = crate::lle_hosts::chains::db_forms;
    fn db_backgrounds(file: usize) -> c_long = crate::lle_hosts::chains::db_backgrounds;
    fn db_create_backgrounds(m: usize) = crate::lle_hosts::chains::db_create_backgrounds;
    fn db_controls(file: usize, m: usize) = crate::lle_hosts::chains::db_controls;
    fn db_control_events(file: usize) = crate::lle_hosts::chains::db_control_events;
    fn db_form_events(file: usize, m: usize) = crate::lle_hosts::chains::db_form_events;
    fn db_hot_keys(file: usize) = crate::lle_hosts::chains::db_hot_keys;
    fn db_remove_controls() = crate::lle_hosts::chains::db_remove_controls;
    fn db_remove_backgrounds() = crate::lle_hosts::chains::db_remove_backgrounds;
    fn db_remove_fonts() = crate::lle_hosts::chains::db_remove_fonts;
    fn combo_part(combo: usize, part: u32) -> usize = crate::lle_hosts::chains::combo_part;
    fn combo_base_set_rect(combo: usize, text_box: usize, drop: usize) = crate::lle_hosts::chains::combo_base_set_rect;
    fn list_scroll_bar(list: usize) -> usize = crate::lle_hosts::chains::list_scroll_bar;
    fn list_selection(list: usize, selection: EeUiShimULongOut) -> c_long = crate::lle_hosts::chains::list_selection;
    fn list_set_scroll_bg_opacity(list: usize, opacity: f32) = crate::lle_hosts::chains::list_set_scroll_bg_opacity;
    fn list_set_scroll_bg_color(list: usize, red: f32, green: f32, blue: f32) = crate::lle_hosts::chains::list_set_scroll_bg_color;
    fn radio_set_down_unchecked(control: usize, bitmap: usize) = crate::lle_hosts::chains::radio_set_down_unchecked;
    fn radio_set_down_checked(control: usize, bitmap: usize) = crate::lle_hosts::chains::radio_set_down_checked;
    fn radio_set_up_checked(control: usize, bitmap: usize) = crate::lle_hosts::chains::radio_set_up_checked;
    fn label_text(label: usize) -> usize = crate::lle_hosts::chains::label_text;
    fn edit_text(edit: usize) -> usize = crate::lle_hosts::chains::edit_text;
}

/// The calls of `shims!`'s `rust:` part: what their C++ shims called, Rust's own (the input entities are ee_base's
/// records, a `GEText`'s only member is ee_render's text, the key slots are [`crate::lle_control`]'s).
#[cfg(not(any(test, feature = "no_cpp")))]
mod own {
    use core::ffi::{c_long, c_ulong};

    use ee_base::input::ffi::{self as input, EeInputEntity};
    use ee_render::ge::smallobj::{self as ge, EeGeText};

    use crate::lle_control::{ee_ui_key_pressed, ee_ui_key_slot, key, key_slot_at, EeUiKey, EeUiKeyPress};

    fn entity(e: usize) -> *const EeInputEntity {
        e as *const EeInputEntity
    }
    pub(super) fn entity_value(e: usize) -> u32 {
        input::ee_input_entity_value(entity(e)) as u32
    }
    pub(super) fn entity_long_value(e: usize) -> c_long {
        input::ee_input_entity_value(entity(e))
    }
    pub(super) fn entity_just_pressed(e: usize) -> bool {
        input::ee_input_entity_just_pressed(entity(e))
    }
    pub(super) fn entity_just_unpressed(e: usize) -> bool {
        input::ee_input_entity_just_unpressed(entity(e))
    }
    pub(super) fn entity_just_double_pressed(e: usize) -> bool {
        input::ee_input_entity_just_double_pressed(entity(e))
    }

    /// `UIForm::GetNumLockOff`: Num Lock not toggled on.
    pub(super) fn num_lock_off() -> bool {
        extern "C" {
            fn PlatKeyIsToggled(virtual_key: u32) -> core::ffi::c_int;
        }
        const VK_NUMLOCK: u32 = 0x90;
        // SAFETY: the platform's query, no arguments to keep alive
        unsafe { PlatKeyIsToggled(VK_NUMLOCK) == 0 }
    }

    // the shared keys' slots (`UIControl::Key`)
    pub(super) fn control_keys_ready() -> bool {
        key(EeUiKey::Space) != 0
    }
    pub(super) fn mouse_buttons_ready() -> bool {
        key(EeUiKey::LeftMouse) != 0
    }
    pub(super) fn mouse_l_button() -> usize {
        key(EeUiKey::LeftMouse)
    }
    pub(super) fn mouse_r_button() -> usize {
        key(EeUiKey::RightMouse)
    }
    /// The left-handed mouse's swap of the two buttons' entities.
    pub(super) fn mouse_swap_buttons() {
        // SAFETY: two distinct live slots
        unsafe { core::ptr::swap(ee_ui_key_slot(EeUiKey::LeftMouse), ee_ui_key_slot(EeUiKey::RightMouse)) }
    }
    pub(super) fn key_down_pressed() -> bool {
        ee_ui_key_pressed(EeUiKeyPress::Down)
    }
    pub(super) fn key_up_pressed() -> bool {
        ee_ui_key_pressed(EeUiKeyPress::Up)
    }
    pub(super) fn key_home_pressed() -> bool {
        ee_ui_key_pressed(EeUiKeyPress::Home)
    }
    pub(super) fn key_end_pressed() -> bool {
        ee_ui_key_pressed(EeUiKeyPress::End)
    }
    pub(super) fn key_enter_value() -> bool {
        entity_value(key(EeUiKey::Enter)) != 0
    }

    /// `gISKeyboard->GetEntity(code, UIControl::Key(slot))` (a slot of [`EeUiKey`]'s order).
    pub(super) fn control_key_fetch(slot: u32, code: u32) {
        // SAFETY: the key slot lives for the process
        unsafe { crate::lle_hosts::input::keyboard_entity(code as c_ulong, key_slot_at(slot)) };
    }
    /// `gISMouse->GetEntity(button, UIControl::Key(LeftMouse + which))`.
    pub(super) fn mouse_button_fetch(which: u32, button: u32) {
        // SAFETY: as control_key_fetch's
        unsafe { crate::lle_hosts::input::mouse_entity(button as c_ulong, key_slot_at(EeUiKey::LeftMouse as u32 + which)) };
    }
    /// `gISKeyboard->GetEntity(kKeyTab, UIControl::Key(Tab))`.
    pub(super) fn form_fetch_tab_key() {
        control_key_fetch(EeUiKey::Tab as u32, 0x0f);
    }
    /// `gISKeyboard->GetEntity(kKeyShift, UIControl::Key(Shift))`.
    pub(super) fn form_fetch_shift_key() {
        control_key_fetch(EeUiKey::Shift as u32, 0xe1);
    }
    /// `gISKeyboard->GetEntity(key, *out)`.
    ///
    /// # Safety
    /// `out` a live pointer.
    pub(super) unsafe fn keyboard_entity(key: u32, out: super::EeUiShimPtrOut) {
        // SAFETY: the contract
        unsafe { crate::lle_hosts::input::keyboard_entity(key as c_ulong, out.0.cast()) };
    }

    /// `TSScheduler::CreateCriticalSection(*lock)`: a new section (always made).
    ///
    /// # Safety
    /// `lock` is a live out pointer.
    pub(super) unsafe fn lock_create(lock: super::EeUiShimPtrOut) -> bool {
        // SAFETY: the contract
        unsafe { *lock.0 = ee_base::ts::ee_ts_section_new() as usize };
        true
    }

    /// A C++ `GEText*`'s text (the class's one member).
    ///
    /// # Safety
    /// `text` is a live `GEText`.
    unsafe fn text(text: usize) -> *mut EeGeText {
        // SAFETY: the contract
        unsafe { *(text as *const *mut EeGeText) }
    }
    /// # Safety
    /// `t` is a live `GEText`.
    unsafe fn fg(t: usize, i: usize) -> f32 {
        // SAFETY: the contract; the colour's three floats
        unsafe { *ge::ee_ge_text_color(text(t), true).add(i) }
    }
    /// # Safety
    /// `t` is a live `GEText`.
    pub(super) unsafe fn text_fg_red(t: usize) -> f32 {
        // SAFETY: the contract
        unsafe { fg(t, 0) }
    }
    /// # Safety
    /// `t` is a live `GEText`.
    pub(super) unsafe fn text_fg_green(t: usize) -> f32 {
        // SAFETY: the contract
        unsafe { fg(t, 1) }
    }
    /// # Safety
    /// `t` is a live `GEText`.
    pub(super) unsafe fn text_fg_blue(t: usize) -> f32 {
        // SAFETY: the contract
        unsafe { fg(t, 2) }
    }
    /// # Safety
    /// `to` and `from` are live `GEText`s.
    unsafe fn copy_color(to: usize, from: usize, foreground: bool) {
        // SAFETY: the contract; each colour three floats
        unsafe {
            core::ptr::copy(ge::ee_ge_text_color(text(from), foreground), ge::ee_ge_text_color(text(to), foreground), 3)
        }
    }
    /// `to->SetFgColor(from->GetFgColor())`.
    ///
    /// # Safety
    /// `to` and `from` are live `GEText`s.
    pub(super) unsafe fn text_copy_fg(to: usize, from: usize) {
        // SAFETY: the contract
        unsafe { copy_color(to, from, true) }
    }
    /// `to->SetBgColor(from->GetBgColor())`.
    ///
    /// # Safety
    /// `to` and `from` are live `GEText`s.
    pub(super) unsafe fn text_copy_bg(to: usize, from: usize) {
        // SAFETY: the contract
        unsafe { copy_color(to, from, false) }
    }
    /// `to->SetIsTransparent(from->GetIsTransparent())`.
    ///
    /// # Safety
    /// `to` and `from` are live `GEText`s.
    pub(super) unsafe fn text_copy_transparent(to: usize, from: usize) {
        // SAFETY: the contract
        unsafe { ge::ee_ge_text_set_transparent(text(to), ge::ee_ge_text_transparent(text(from))) }
    }
}
