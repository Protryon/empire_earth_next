//! `UIControlsTable.cpp`'s C++ logic in Rust over one-statement shims (UI lane round 10): the table's host's cases
//! of more than one call ([`table_call`], plugged in as `ee_ui_table_host` before `UIControlsTable::HostOp`), the
//! description's bitmaps (`CreateBitmap`) and its string-table range (`SetupStringArray`). The table's records and
//! factories were already Rust (`tableobj.rs`).

use core::ffi::c_ulong;

use crate::controls_table as ct;
use crate::lle_cpp::{lle_host, Cpp, EeUiShimU8Out as U8Out, EeUiShimULongOut as ULongOut, HostArgs};
use crate::lle_manager::ui_texture_stages;
use crate::obj::EeUiOp as Op;
use crate::tableobj::EeUiTableSet as Set;

/// The table's host's moved cases (`UIControlsTable::HostOp`; `d` the manager where the op needs one); `None` for
/// the C++ host's.
pub(crate) fn table_call<C: Cpp + ?Sized>(c: &mut C, h: &HostArgs) -> Option<isize> {
    let (t, m) = (h.obj, h.d as usize);
    Some(match h.op {
        Op::TblFileReadCount => {
            let mut count: c_ulong = 0;
            let result = c.file_read_count(h.pa(), ULongOut::of(&mut count));
            ((count as u64) << 32 | result as u32 as u64) as isize
        }
        Op::TblFileRead => c.file_read(h.pa(), U8Out(h.b as *mut u8), h.c as c_ulong) as isize,
        Op::TblAddDatabaseControl => {
            c.form_add_database_control(h.pa(), h.b as u32);
            0
        }
        Op::TblStringNew => {
            let string = c.string_new();
            if h.a > 0 {
                c.mgr_string(m, h.a as u32, string);
            }
            string as isize
        }
        Op::TblStringsNew => {
            let strings = c.strings_new();
            setup_strings(c, t, m, strings);
            strings as isize
        }
        Op::TblBitmapNew => {
            // the rectangle in f: bottom, left, right, top
            let [bottom, left, right, top] = h.f4();
            (match h.a {
                0 => c.table_bitmap_normal(t, m, left, top, right, bottom),
                1 => c.table_bitmap_pressed(t, m, left, top, right, bottom),
                _ => c.table_bitmap_focused(t, m, left, top, right, bottom),
            }) as isize
        }
        // the original's radio button released a bitmap that failed to load (a NULL read); none is released here
        Op::TblBitmapRelease => {
            if h.a != 0 {
                c.bitmap_dereference(h.pa());
            }
            0
        }
        Op::TblSet if h.b == Set::FileListExtensions as isize => {
            let strings = h.c as usize;
            let count = c.strings_count(strings);
            for i in 0..count {
                c.file_list_add_extension(h.pa(), strings, i);
            }
            0
        }
        Op::TblSet if h.b == Set::RadioPressed as isize => {
            let bitmap = h.c as usize;
            c.radio_set_down_unchecked(h.pa(), bitmap);
            c.radio_set_down_checked(h.pa(), bitmap);
            c.radio_set_up_checked(h.pa(), bitmap);
            0
        }
        _ => return None,
    })
}

lle_host!(
    /// The table's host: [`table_call`], then `UIControlsTable::HostOp`.
    ee_ui_table_host => table_call, ee_uicpp_table_host
);

/// `UIControlsTable::SetupStringArray(m, strings)`: the list emptied, then filled with the description's
/// string-table range (none when the range is unset).
pub(crate) fn setup_strings<C: Cpp + ?Sized>(c: &mut C, t: usize, m: usize, strings: usize) {
    c.strings_clear(strings);
    let begin = c.table_string_begin(t);
    let end = c.table_string_end(t);
    let count = ct::string_count(begin as _, end as _) as u32;
    if count == 0 {
        return;
    }
    c.strings_resize(strings, count);
    for i in 0..count {
        let begin = c.table_string_begin(t);
        c.mgr_string_at(m, begin.wrapping_add(i as i32), strings, i);
    }
}

/// `UIControlsTable::CreateBitmap(m, rect, name, alpha, texture rect)`: the UI texture loaded (the UI bit depth,
/// the given alpha, clamped, point MIP filter) and a bitmap stretched over the rectangle; 0 when it does not load.
pub(crate) fn create_bitmap<C: Cpp + ?Sized>(
    c: &mut C,
    m: usize,
    rect: usize,
    name: usize,
    alpha: bool,
    texture_rect: usize,
) -> usize {
    let manager = c.mgr_texture_manager_by_screen(m);
    let depth = c.mgr_ui_depth(m);
    let texture = c.texture_load_ui_string(manager, name, depth, alpha);
    if texture == 0 {
        return 0;
    }
    ui_texture_stages(c, texture);
    let bitmap = c.bitmap_new();
    let screen = c.mgr_screen(m);
    c.bitmap_set_texture(bitmap, texture, screen);
    let screen = c.mgr_screen(m);
    c.bitmap_stretch(bitmap, rect, texture_rect, screen);
    bitmap
}

/// `UIControlsTable::SetupStringArray` (the C++ member's body).
///
/// # Safety
/// `table`, `manager` and `strings` (a `std::vector<UWideString>*`) are the C++ caller's live objects.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_table_setup_strings(
    table: *const core::ffi::c_void,
    manager: *mut core::ffi::c_void,
    strings: *mut core::ffi::c_void,
) {
    setup_strings(&mut crate::lle_cpp::Ffi, table as usize, manager as usize, strings as usize);
}

/// `UIControlsTable::CreateBitmap` (the C++ member's body).
///
/// # Safety
/// `manager`, the rectangles (`const GERectangle*`) and `name` (a `const UString*`) are the caller's live objects.
#[cfg(not(any(test, feature = "no_cpp")))]
#[no_mangle]
pub unsafe extern "C" fn ee_ui_table_create_bitmap(
    manager: *mut core::ffi::c_void,
    rect: *const core::ffi::c_void,
    name: *const core::ffi::c_void,
    alpha: bool,
    texture_rect: *const core::ffi::c_void,
) -> *mut core::ffi::c_void {
    create_bitmap(
        &mut crate::lle_cpp::Ffi,
        manager as usize,
        rect as usize,
        name as usize,
        alpha,
        texture_rect as usize,
    ) as *mut core::ffi::c_void
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::lle_cpp::Fake;

    fn run(fake: &mut Fake, op: Op, abcd: [isize; 4], f: &mut [f32; 4]) -> Option<isize> {
        table_call(fake, &HostArgs::new(0x100, op, abcd, f))
    }

    #[test]
    fn files_strings_and_database_controls() {
        let mut fake = Fake::default();
        fake.out("file_read_count", 7).answer("file_read_count", &[2]).answer("string_new", &[0x10, 0x11]);
        fake.answer("strings_new", &[0x20]);
        let mut f = [0.0; 4];
        assert_eq!(run(&mut fake, Op::TblFileReadCount, [5, 0, 0, 0], &mut f), Some((7u64 << 32 | 2) as isize));
        run(&mut fake, Op::TblAddDatabaseControl, [6, 3, 0, 0], &mut f);
        assert_eq!(run(&mut fake, Op::TblStringNew, [0, 0, 0, 9], &mut f), Some(0x10));
        assert_eq!(run(&mut fake, Op::TblStringNew, [42, 0, 0, 9], &mut f), Some(0x11));
        assert_eq!(run(&mut fake, Op::TblStringsNew, [0, 0, 0, 9], &mut f), Some(0x20));
        assert_eq!(
            fake.calls(),
            "file_read_count(5, out)\nform_add_database_control(6, 3)\nstring_new()\nstring_new()\nmgr_string(9, 42, 17)\n\
             strings_new()\nstrings_clear(32)\ntable_string_begin(256)\ntable_string_end(256)"
        );
    }

    #[test]
    fn string_range_fills_the_list() {
        let mut fake = Fake::default();
        fake.answer("table_string_begin", &[10, 10, 10]).answer("table_string_end", &[11]);
        setup_strings(&mut fake, 1, 2, 3);
        assert_eq!(
            fake.calls(),
            "strings_clear(3)\ntable_string_begin(1)\ntable_string_end(1)\nstrings_resize(3, 2)\n\
             table_string_begin(1)\nmgr_string_at(2, 10, 3, 0)\ntable_string_begin(1)\nmgr_string_at(2, 11, 3, 1)"
        );
    }

    #[test]
    fn bitmaps_sets_and_releases() {
        let mut fake = Fake::default();
        let mut f = [4.0, 1.0, 3.0, 2.0];
        run(&mut fake, Op::TblBitmapNew, [0, 0, 0, 9], &mut f);
        run(&mut fake, Op::TblBitmapNew, [2, 0, 0, 9], &mut f);
        run(&mut fake, Op::TblBitmapRelease, [0, 0, 0, 0], &mut f);
        run(&mut fake, Op::TblBitmapRelease, [8, 0, 0, 0], &mut f);
        fake.answer("strings_count", &[2]);
        run(&mut fake, Op::TblSet, [5, Set::FileListExtensions as isize, 6, 0], &mut f);
        run(&mut fake, Op::TblSet, [5, Set::RadioPressed as isize, 7, 0], &mut f);
        assert_eq!(run(&mut fake, Op::TblSet, [5, Set::BgColor as isize, 7, 0], &mut f), None);
        assert_eq!(
            fake.calls(),
            "table_bitmap_normal(256, 9, 1.0, 2.0, 3.0, 4.0)\ntable_bitmap_focused(256, 9, 1.0, 2.0, 3.0, 4.0)\n\
             bitmap_dereference(8)\nstrings_count(6)\nfile_list_add_extension(5, 6, 0)\nfile_list_add_extension(5, 6, 1)\n\
             radio_set_down_unchecked(5, 7)\nradio_set_down_checked(5, 7)\nradio_set_up_checked(5, 7)"
        );
    }

    #[test]
    fn create_bitmap_loads_sets_the_stages_and_stretches() {
        let mut fake = Fake::default();
        assert_eq!(create_bitmap(&mut fake, 1, 2, 3, false, 4), 0);
        assert_eq!(
            fake.calls(),
            "mgr_texture_manager_by_screen(1)\nmgr_ui_depth(1)\ntexture_load_ui_string(0, 3, 0, false)"
        );
        let mut fake = Fake::default();
        fake.answer("texture_load_ui_string", &[0x50]).answer("bitmap_new", &[0x60]);
        assert_eq!(create_bitmap(&mut fake, 1, 2, 3, true, 4), 0x60);
        assert!(fake.calls().ends_with(
            "texture_stage_address(80, 0)\nbitmap_new()\nmgr_screen(1)\nbitmap_set_texture(96, 80, 0)\nmgr_screen(1)\n\
             bitmap_stretch(96, 2, 4, 0)"
        ));
    }
}
