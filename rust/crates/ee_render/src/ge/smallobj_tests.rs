//! `smallobj.rs`'s tests: the four objects' host calls in the originals' order over a recording host with in-memory
//! strings, reference counts, platform fonts and windows.

use super::*;
use crate::ge::cppobj::{self, testing};
use std::cell::RefCell;
use std::ffi::c_char;

#[derive(Default)]
struct World {
    log: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World::default());
}

fn with<R>(f: impl FnOnce(&mut World) -> R) -> R {
    WORLD.with(|w| f(&mut w.borrow_mut()))
}

fn log(s: String) {
    with(|w| w.log.push(s));
}

fn take_log() -> Vec<String> {
    with(|w| std::mem::take(&mut w.log))
}

/// A C++-layout `UString` (`cppobj::testing`).
fn string(bytes: &[u8]) -> usize {
    testing::new_ustring(std::str::from_utf8(bytes).unwrap())
}

/// A C++-layout `UWideString`.
fn wide(bytes: &[u8]) -> usize {
    testing::new_uwstring_text(std::str::from_utf8(bytes).unwrap())
}

fn text_of(s: usize) -> String {
    // SAFETY: a live string of the tests
    unsafe { testing::text(s) }
}

fn wtext_of(s: usize) -> String {
    // SAFETY: a live wide string of the tests
    unsafe { testing::wtext(s) }
}

/// A reference-counted C++-layout object (a texture, a font bitmap) or a font around its Rust object.
fn counted(rust: *mut c_void) -> usize {
    testing::new_counted(rust)
}

fn refcount(o: usize) -> c_ulong {
    // SAFETY: a live object of the tests
    unsafe { cppobj::reference_count(o) }
}

unsafe extern "C" fn decode(text: *const c_char, out_length: *mut u32) -> u32 {
    // SAFETY: a NUL-terminated text
    unsafe {
        let c = *text as u8;
        *out_length = (c != 0) as u32;
        c as u32
    }
}

unsafe extern "C" fn next_chars(text: *const u8, characters: usize) -> *mut u8 {
    // SAFETY: a NUL-terminated text
    unsafe {
        let mut p = text;
        for _ in 0..characters {
            if *p == 0 {
                break;
            }
            p = p.add(1);
        }
        p as *mut u8
    }
}

extern "C" fn cp1252(c: u32) -> std::ffi::c_int {
    if c < 0x100 {
        c as std::ffi::c_int
    } else {
        -1
    }
}

unsafe extern "C" fn measure(_font: *const c_void, _text: *const c_char, length: u32) -> i32 {
    (length * 7) as i32
}

static OPS: GeTextOps = GeTextOps { decode, next_chars, cp1252_byte_of: cp1252, measure };

unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeObjOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    match op {
        EeGeObjOp::StrNew => {
            log(format!("{op:?} {a}"));
            testing::new_ustring_default() as isize
        }
        EeGeObjOp::WStrNew => {
            log(format!("{op:?} {a}"));
            testing::new_uwstring(a as c_ulong) as isize
        }
        EeGeObjOp::StrCopy => {
            let bytes = text_of(a as usize);
            log(format!("copy '{bytes}'"));
            string(bytes.as_bytes()) as isize
        }
        EeGeObjOp::StrDelete => {
            log(format!("{op:?} '{}'", text_of(o)));
            // SAFETY: the object's own string
            unsafe { testing::delete_ustring(o) };
            0
        }
        EeGeObjOp::WStrDelete => {
            log(format!("{op:?} '{}'", wtext_of(o)));
            // SAFETY: the object's own string
            unsafe { testing::delete_uwstring(o) };
            0
        }
        EeGeObjOp::TexDereference | EeGeObjOp::RefDereference => {
            // SAFETY: a live counted object (the tests keep theirs above 0)
            unsafe { (*(o as *mut cppobj::CppCounted)).count -= 1 };
            log(format!("deref {}", refcount(o)));
            0
        }
        EeGeObjOp::PlatFontCreate => {
            // SAFETY: a NUL-terminated face
            let face = unsafe { std::ffi::CStr::from_ptr(obj as *const c_char) }.to_string_lossy().into_owned();
            log(format!("create '{face}' {a} {b:#x}"));
            if face.starts_with('X') {
                0
            } else {
                0x7000
            }
        }
        EeGeObjOp::PlatFontDestroy => {
            log(format!("destroy {o:#x}"));
            0
        }
        EeGeObjOp::PlatFontHeight => {
            log("metrics".into());
            15
        }
        EeGeObjOp::TextOps => &OPS as *const GeTextOps as isize,
        EeGeObjOp::PlatWindowDestroy => {
            log(format!("window destroy {o:#x}"));
            0
        }
        EeGeObjOp::PlatWindowSetSize => {
            log(format!("window size {a} {b}"));
            0
        }
        EeGeObjOp::PlatWindowPosition => {
            // SAFETY: the two outputs
            unsafe {
                *(a as *mut i32) = 12;
                *(b as *mut i32) = -4;
            }
            log("window position".into());
            0
        }
    }
}

fn owner() -> *mut c_void {
    0x10 as *mut c_void
}

#[test]
fn material_textures_name_fields_and_destruction() {
    take_log();
    // SAFETY: the recording host serves the material
    unsafe {
        let m = ee_ge_mat_new(host, owner(), 2, 3);
        assert_eq!(take_log(), ["StrNew 0"]);
        assert_eq!(*ee_ge_mat_color(m).add(2), 1.0);
        assert_eq!(ee_ge_mat_opacity(m), 1.0);
        assert_eq!(ee_ge_mat_get(m, EeGeMatField::LodCount, 0), 3);
        let (t1, t2) = (counted(std::ptr::null_mut()), counted(std::ptr::null_mut()));
        ee_ge_mat_set_texture(m, t1 as *mut c_void, 1);
        assert_eq!(refcount(t1), 2);
        ee_ge_mat_set_texture(m, t2 as *mut c_void, 1);
        ee_ge_mat_set_texture(m, std::ptr::null_mut(), 0);
        assert_eq!(take_log(), ["deref 1"]);
        assert_eq!((refcount(t1), refcount(t2)), (1, 2));
        assert_eq!(ee_ge_mat_texture(m, 1), t2 as *mut c_void);
        assert!(ee_ge_mat_texture(m, 5).is_null());
        ee_ge_mat_set(m, EeGeMatField::PolygonStart, 2, 44);
        ee_ge_mat_set(m, EeGeMatField::PolygonLength, 9, 1);
        ee_ge_mat_set(m, EeGeMatField::VertexLength, 0, 7);
        assert_eq!(ee_ge_mat_get(m, EeGeMatField::PolygonStart, 2), 44);
        assert_eq!(ee_ge_mat_get(m, EeGeMatField::PolygonLength, 9), 0);
        assert_eq!(ee_ge_mat_get(m, EeGeMatField::VertexLength, 0), 7);
        let n = string(b"stone");
        ee_ge_mat_set_name(m, n as *const c_void);
        assert_eq!(text_of(ee_ge_mat_name(m) as usize), "stone");
        let name_size = cppobj::str_size_of(ee_ge_mat_name(m) as usize);
        let size = ee_ge_mat_size_of(m, 0x48, 0x10, 4, 4);
        assert_eq!(size, crate::ge::texture::material_size_of(0x48, 0x10, name_size, 2, 4, 3, 4));
        ee_ge_mat_destroy(m);
        assert_eq!(take_log(), ["deref 1", "StrDelete 'stone'"]);
        testing::delete_ustring(n);
        testing::delete_counted(t1);
        testing::delete_counted(t2);
    }
}

#[test]
fn font_creation_compare_bitmap_and_destruction() {
    take_log();
    // SAFETY: the recording host serves the fonts
    unsafe {
        let name = string(b"Arial");
        let f = ee_ge_font_new(host, owner(), name as *const c_void, 12, 0b101, 2);
        assert_eq!(take_log(), ["copy 'Arial'", "create 'Arial' -12 0xd", "metrics"]);
        assert_eq!(ee_ge_font_value(f, EeGeFontValue::Height), 15);
        assert_eq!(ee_ge_font_value(f, EeGeFontValue::Style), 5);
        assert_eq!(text_of(ee_ge_font_name(f) as usize), "Arial");
        assert!(!ee_ge_font_compare(f, name as *const c_void, 5, 1, 12));
        assert!(ee_ge_font_compare(f, name as *const c_void, 5, 2, 12));
        let other = string(b"Arial Bold");
        assert!(!ee_ge_font_compare(f, other as *const c_void, 5, 2, 12));
        assert_eq!(take_log(), Vec::<String>::new());
        let bitmap = counted(std::ptr::null_mut());
        ee_ge_font_set_bitmap(f, bitmap as *mut c_void);
        assert_eq!(ee_ge_font_bitmap(f), bitmap as *mut c_void);
        assert_eq!(refcount(bitmap), 2);
        ee_ge_font_destroy(f);
        assert_eq!(take_log(), ["destroy 0x7000", "deref 1", "StrDelete 'Arial'"]);
        let missing = string(b"Xnone");
        let g = ee_ge_font_new(host, owner(), missing as *const c_void, 3, 0, 0);
        assert_eq!(take_log(), ["copy 'Xnone'", "create 'Xnone' -3 0x0"]);
        assert_eq!(ee_ge_font_value(g, EeGeFontValue::Height), 0);
        assert!(ee_ge_font_os(g).is_null());
        ee_ge_font_destroy(g);
        assert_eq!(take_log(), ["destroy 0x0", "StrDelete 'Xnone'"]);
        for s in [name, other, missing] {
            testing::delete_ustring(s);
        }
        testing::delete_counted(bitmap);
    }
}

#[test]
fn text_extent_font_switch_assignment_and_destruction() {
    take_log();
    // SAFETY: the recording host serves the fonts and texts
    unsafe {
        let name = string(b"Arial");
        let f = ee_ge_font_new(host, owner(), name as *const c_void, 10, 0, 0);
        let none = string(b"Xno");
        let g = ee_ge_font_new(host, owner(), none as *const c_void, 10, 0, 0);
        // the C++ fonts around the objects (their Rust objects read in place)
        let (cf, cg) = (counted(f as *mut c_void), counted(g as *mut c_void));
        take_log();
        let t = ee_ge_text_new(host, owner(), cf as *mut c_void);
        assert_eq!(take_log(), ["WStrNew 256"]);
        assert_eq!(refcount(cf), 2);
        assert_eq!(*ee_ge_text_color(t, true), 1.0);
        assert_eq!(ee_ge_text_alignment(t), 0);
        let s = wide(b"abcd");
        ee_ge_text_set_text(t, s as *const c_void);
        assert_eq!(take_log(), ["metrics"]);
        assert_eq!(wtext_of(ee_ge_text_string(t) as usize), "abcd");
        assert_eq!((*ee_ge_text_extent_of(t)).right, 28.0);
        assert_eq!((*ee_ge_text_extent_of(t)).bottom, 15.0);
        ee_ge_text_set_font(t, cg as *mut c_void);
        assert_eq!(take_log(), ["deref 1"]);
        assert_eq!((refcount(cf), refcount(cg)), (1, 2));
        // no platform font: the extent is kept
        assert_eq!((*ee_ge_text_extent_of(t)).right, 28.0);
        let u = ee_ge_text_new(host, owner(), cf as *mut c_void);
        (*ee_ge_text_location(u)).x = 3.5;
        ee_ge_text_set_alignment(u, 2);
        ee_ge_text_set_transparent(u, true);
        take_log();
        ee_ge_text_assign(t, u);
        assert_eq!(take_log(), ["deref 1"]);
        assert_eq!(wtext_of(ee_ge_text_string(t) as usize), "");
        assert_eq!((*ee_ge_text_location(t)).x, 3.5);
        assert_eq!(ee_ge_text_alignment(t), 2);
        assert!(ee_ge_text_transparent(t));
        assert_eq!(ee_ge_text_font(t), cf as *mut c_void);
        assert_eq!(refcount(cf), 3);
        ee_ge_text_destroy(t);
        assert_eq!(take_log(), ["deref 2", "WStrDelete ''"]);
        ee_ge_text_destroy(u);
        ee_ge_font_destroy(f);
        ee_ge_font_destroy(g);
        testing::delete_uwstring(s);
        for o in [cf, cg] {
            testing::delete_counted(o);
        }
        for o in [name, none] {
            testing::delete_ustring(o);
        }
    }
}

#[test]
fn window_origin_resize_and_destruction() {
    take_log();
    // SAFETY: the recording host serves the windows
    unsafe {
        let w = ee_ge_win_new(host, owner(), 0x400 as *mut c_void, 640, 480, 1, true);
        assert_eq!(ee_ge_win_size(w, false), 640);
        assert_eq!(ee_ge_win_size(w, true), 480);
        assert_eq!(ee_ge_win_os(w), 0x400 as *mut c_void);
        let o = ee_ge_win_origin(w);
        assert_eq!(*o, Ge2DPoint { x: 12.0, y: -4.0 });
        ee_ge_win_resize(w, 800, 600);
        assert_eq!(ee_ge_win_size(w, false), 800);
        assert_eq!(take_log(), ["window position", "window size 800 600"]);
        ee_ge_win_destroy(w);
        assert_eq!(take_log(), ["window destroy 0x400"]);
        let v = ee_ge_win_new(host, owner(), 0x500 as *mut c_void, 1, 1, 0, false);
        ee_ge_win_destroy(v);
        assert_eq!(take_log(), Vec::<String>::new());
    }
}
