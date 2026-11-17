//! `values.rs`'s tests: the copies keep every bit, the comparisons are IEEE's, the setters keep the position, and the
//! codecs stop at the first error with what they read so far.

use super::*;
use crate::ge::io::BufferIo;
use std::ffi::c_void;

unsafe extern "C" fn r8(f: *mut c_void, o: *mut u8) -> c_long {
    // SAFETY: the table's file is a BufferIo
    unsafe { (*(f as *mut BufferIo)).read_u8(&mut *o) }
}
unsafe extern "C" fn r16(f: *mut c_void, o: *mut u16) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_u16(&mut *o) }
}
unsafe extern "C" fn r32(f: *mut c_void, o: *mut u32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_u32(&mut *o) }
}
unsafe extern "C" fn rf(f: *mut c_void, o: *mut f32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).read_f32(&mut *o) }
}
unsafe extern "C" fn w8(f: *mut c_void, v: u8) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u8(v) }
}
unsafe extern "C" fn w16(f: *mut c_void, v: u16) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u16(v) }
}
unsafe extern "C" fn w32(f: *mut c_void, v: u32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_u32(v) }
}
unsafe extern "C" fn wf(f: *mut c_void, v: f32) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).write_f32(v) }
}
unsafe extern "C" fn rb(_: *mut c_void, _: *mut u8, _: u32, _: *mut u32) -> c_long {
    9
}
unsafe extern "C" fn wb(_: *mut c_void, _: *const u8, _: u32, _: *mut u32) -> c_long {
    9
}
unsafe extern "C" fn gp(f: *mut c_void, o: *mut c_long) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).get_position(&mut *o) }
}
unsafe extern "C" fn sp(f: *mut c_void, v: c_long) -> c_long {
    // SAFETY: as above
    unsafe { (*(f as *mut BufferIo)).set_position(v) }
}

fn table(io: &mut BufferIo) -> EeGeFieldIo {
    EeGeFieldIo {
        file: io as *mut BufferIo as *mut c_void,
        read_u8: r8,
        read_u16: r16,
        read_u32: r32,
        read_f32: rf,
        write_u8: w8,
        write_u16: w16,
        write_u32: w32,
        write_f32: wf,
        read_bytes: rb,
        write_bytes: wb,
        get_position: gp,
        set_position: sp,
    }
}

fn sample() -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    for (i, row) in t.m.iter_mut().enumerate() {
        for (j, e) in row.iter_mut().enumerate() {
            *e = (i * 4 + j) as f32 * 0.5 - 3.0;
        }
    }
    t.m[1][2] = -0.0;
    t.m[2][1] = f32::from_bits(0x7fc0_1234);
    t
}

fn bits(t: &GeTransformation) -> Vec<u32> {
    t.m.iter().flatten().map(|e| e.to_bits()).collect()
}

#[test]
fn a_copy_keeps_every_bit_and_a_self_copy_changes_nothing() {
    let s = sample();
    let mut t = GeTransformation::IDENTITY;
    // SAFETY: valid matrices
    unsafe {
        ee_ge_t_copy(&mut t, &s.m);
        assert_eq!(bits(&t), bits(&s));
        let p: *mut GeTransformation = &mut t;
        ee_ge_t_copy(p, &(*p).m);
        assert_eq!(bits(&t), bits(&s));
        ee_ge_t_identity(&mut t);
    }
    assert_eq!(t.m, GeTransformation::IDENTITY.m);
}

#[test]
fn equality_is_element_by_element_and_a_nan_never_equals() {
    let s = sample();
    let mut t = s;
    // SAFETY: valid matrices
    unsafe {
        assert!(!ee_ge_t_equal(&s, &t), "the NaN element");
        t.m[2][1] = 1.0;
        let mut u = t;
        assert!(ee_ge_t_equal(&t, &u));
        u.m[1][2] = 0.0; // -0 == 0
        assert!(ee_ge_t_equal(&t, &u));
        u.m[3][3] = 2.0;
        assert!(!ee_ge_t_equal(&t, &u));
        let at = Ge3DPoint { x: t.m[0][3], y: t.m[1][3], z: t.m[2][3] };
        assert!(ee_ge_t_at_point(&t, &at));
        assert!(!ee_ge_t_at_point(&t, &Ge3DPoint { z: 99.0, ..at }));
    }
}

#[test]
fn the_scale_and_zero_orientation_setters_keep_the_position() {
    let s = sample();
    let mut t = s;
    // SAFETY: valid matrices
    unsafe {
        ee_ge_t_set_scale(&mut t, 2.0, 3.0, 4.0);
        assert_eq!(
            t.m,
            [[2.0, 0.0, 0.0, s.m[0][3]], [0.0, 3.0, 0.0, s.m[1][3]], [0.0, 0.0, 4.0, s.m[2][3]], [0.0, 0.0, 0.0, 1.0]]
        );
        let mut out = GeTransformation::IDENTITY;
        ee_ge_t_zero_orientation(&s, &mut out);
        assert_eq!(
            out.m,
            [[1.0, 0.0, 0.0, s.m[0][3]], [0.0, 1.0, 0.0, s.m[1][3]], [0.0, 0.0, 1.0, s.m[2][3]], [0.0, 0.0, 0.0, 1.0]]
        );
        let mut t = s;
        let p: *mut GeTransformation = &mut t;
        ee_ge_t_zero_orientation(p, p);
        assert_eq!(t.m, out.m);
    }
}

#[test]
fn a_transformation_saves_row_by_row_and_a_short_file_keeps_what_was_read() {
    let s = sample();
    let mut out = BufferIo::writer();
    // SAFETY: valid matrices and tables
    unsafe {
        let mut src = s;
        assert_eq!(ee_ge_t_io(&mut src, &table(&mut out), true), OK);
        assert_eq!(out.bytes.len(), 64);
        assert_eq!(&out.bytes[4..8], &s.m[0][1].to_le_bytes());
        let mut back = GeTransformation::IDENTITY;
        assert_eq!(ee_ge_t_io(&mut back, &table(&mut BufferIo::reader(out.bytes.clone())), false), OK);
        assert_eq!(bits(&back), bits(&s));
        // 5 floats and a half: rows 0 and [1][0] read, the rest as they were
        let mut part = GeTransformation::IDENTITY;
        let r = ee_ge_t_io(&mut part, &table(&mut BufferIo::reader(out.bytes[..22].to_vec())), false);
        assert_eq!(r, crate::ge::io::END_OF_FILE);
        assert_eq!(part.m[0], s.m[0]);
        assert_eq!(part.m[1][0], s.m[1][0]);
        assert_eq!(part.m[1][1..], GeTransformation::IDENTITY.m[1][1..]);
        assert_eq!(part.m[3], GeTransformation::IDENTITY.m[3]);
        // a write refused at the third float stops there
        let mut limited = BufferIo { write_limit: 10, ..BufferIo::writer() };
        assert_eq!(ee_ge_t_io(&mut src, &table(&mut limited), true), crate::ge::io::FAILED);
        assert_eq!(limited.bytes.len(), 8);
    }
}

fn fields(v: &mut [f32]) -> Vec<*mut f32> {
    v.iter_mut().map(|x| x as *mut f32).collect()
}

#[test]
fn a_closed_file_is_not_open_and_reads_nothing() {
    let mut v = [1.0f32, 2.0, 3.0];
    let mut io = BufferIo::reader(vec![0; 12]);
    let f = fields(&mut v);
    // SAFETY: valid pointers and table
    let r = unsafe { ee_ge_fields_io(f.as_ptr(), 3, &table(&mut io), 0, false, false) };
    assert_eq!(r, 4);
    assert_eq!(io.position, 0);
    assert_eq!(v, [1.0, 2.0, 3.0]);
}

#[test]
fn the_fields_go_in_the_order_given_and_stop_at_the_first_error() {
    // GERectangle: left, top, right, bottom (not its member order)
    let mut rect = [10.0f32, 20.0, 30.0, 40.0];
    let order = [1usize, 0, 2, 3];
    let mut out = BufferIo::writer();
    let f: Vec<*mut f32> = order.iter().map(|&i| &mut rect[i] as *mut f32).collect();
    // SAFETY: valid pointers and tables
    unsafe {
        assert_eq!(ee_ge_fields_io(f.as_ptr(), 4, &table(&mut out), 1, true, false), OK);
        assert_eq!(&out.bytes[0..4], &20.0f32.to_le_bytes());
        // an unasked open state (-1, GEColor) reads all; a short file keeps what was read
        let mut back = [0.0f32; 4];
        let b = fields(&mut back);
        let r =
            ee_ge_fields_io(b.as_ptr(), 4, &table(&mut BufferIo::reader(out.bytes[..9].to_vec())), -1, false, false);
        assert_eq!(r, crate::ge::io::END_OF_FILE);
        assert_eq!(back, [20.0, 10.0, 0.0, 0.0]);
    }
}

#[test]
fn a_failed_texture_coordinate_sets_the_position_back() {
    let mut uv = [0.5f32, 0.25];
    let f = fields(&mut uv);
    let mut io = BufferIo::reader(vec![0; 6]);
    io.position = 2;
    // SAFETY: valid pointers and table
    unsafe {
        let r = ee_ge_fields_io(f.as_ptr(), 2, &table(&mut io), 1, false, true);
        assert_eq!(r, crate::ge::io::END_OF_FILE);
        assert_eq!(io.position, 2);
        assert_eq!(uv, [0.0, 0.25]);
        // without the restore the position stays after the read field
        io.position = 2;
        ee_ge_fields_io(f.as_ptr(), 2, &table(&mut io), 1, false, false);
        assert_eq!(io.position, 6);
    }
}

#[test]
fn a_viewport_copy_is_every_field() {
    let v = GeViewport { half_width: 3.0, far_depth: -0.0, ..GeViewport::default() };
    let mut out = GeViewport::default();
    // SAFETY: valid viewports
    unsafe { ee_ge_viewport_copy(&mut out, &v) };
    assert_eq!(out, v);
    assert_eq!(out.far_depth.to_bits(), 0x8000_0000);
}
