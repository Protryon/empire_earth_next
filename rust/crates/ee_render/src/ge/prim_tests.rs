//! `prim.rs`'s tests: the constructors', destructor's, `SetTexture`'s and the rhombus's host calls in the originals'
//! order over a recording host with in-memory vertex arrays and reference-counted textures.

use super::*;
use std::cell::RefCell;
use std::collections::BTreeMap;

const RASTERIZER: usize = 0x5000;
const SCREENS: isize = 2;

#[derive(Clone, Copy, Default)]
struct Vertex {
    color: [f32; 3],
    point: Ge3DPoint,
    normal: [f32; 3],
    opacity: f32,
    uv: [f32; 2],
}

#[derive(Default)]
struct World {
    arrays: BTreeMap<usize, Vec<Vertex>>,
    refs: BTreeMap<usize, i32>,
    next: usize,
    log: Vec<String>,
}

thread_local! {
    static WORLD: RefCell<World> = RefCell::new(World { next: 0x100, ..World::default() });
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

unsafe extern "C" fn host(_owner: *mut c_void, op: EeGeBmpOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    let i = a as usize;
    // SAFETY: the pointers are the tests' own values of the right type
    unsafe {
        match op {
            EeGeBmpOp::RasterActive => RASTERIZER as isize,
            EeGeBmpOp::RasterScreenCount => {
                log("screens".into());
                SCREENS
            }
            EeGeBmpOp::RasterVertexArray => with(|w| {
                w.next += 0x10;
                let v = w.next;
                w.arrays.insert(v, vec![Vertex::default(); b as usize]);
                w.log.push(format!("new {v:x} type {a} count {b}"));
                v as isize
            }),
            EeGeBmpOp::VertexDelete => with(|w| {
                w.arrays.remove(&o);
                w.log.push(format!("delete {o:x}"));
                0
            }),
            EeGeBmpOp::VertexSetPoint => {
                let point = *(b as *const Ge3DPoint);
                with(|w| w.arrays.get_mut(&o).unwrap()[i].point = point);
                log(format!("point {i} {} {} {}", point.x, point.y, point.z));
                0
            }
            EeGeBmpOp::VertexGetPoint => {
                *(b as *mut Ge3DPoint) = with(|w| w.arrays[&o][i].point);
                log(format!("get point {i}"));
                0
            }
            EeGeBmpOp::VertexSetNormal => {
                let n = *(b as *const [f32; 3]);
                with(|w| w.arrays.get_mut(&o).unwrap()[i].normal = n);
                log(format!("normal {i} {:?}", n));
                0
            }
            EeGeBmpOp::VertexSetColor => {
                let c = *(b as *const [f32; 3]);
                with(|w| w.arrays.get_mut(&o).unwrap()[i].color = c);
                log(format!("color {i} {:?}", c));
                0
            }
            EeGeBmpOp::VertexGetColor => {
                *(b as *mut [f32; 3]) = with(|w| w.arrays[&o][i].color);
                0
            }
            EeGeBmpOp::VertexSetOpacity => {
                let v = f32::from_bits(b as u32);
                with(|w| w.arrays.get_mut(&o).unwrap()[i].opacity = v);
                log(format!("opacity {i} {v}"));
                0
            }
            EeGeBmpOp::VertexGetOpacity => with(|w| w.arrays[&o][i].opacity.to_bits() as isize),
            EeGeBmpOp::VertexSetUv => {
                let uv = *(b as *const [f32; 2]);
                with(|w| w.arrays.get_mut(&o).unwrap()[i].uv = uv);
                log(format!("uv {i} {:?}", uv));
                0
            }
            EeGeBmpOp::VertexGetUv => {
                *(b as *mut [f32; 2]) = with(|w| w.arrays[&o][i].uv);
                0
            }
            EeGeBmpOp::VertexCopyFrom => with(|w| {
                let from = w.arrays[&(b as usize)].clone();
                w.arrays.insert(o, from);
                w.log.push(format!("copy {:x} <- {:x}", o, b));
                0
            }),
            EeGeBmpOp::TexReference => with(|w| {
                *w.refs.entry(o).or_insert(1) += 1;
                w.log.push(format!("ref {o:x}"));
                0
            }),
            EeGeBmpOp::TexDereference => with(|w| {
                *w.refs.entry(o).or_insert(1) -= 1;
                w.log.push(format!("deref {o:x}"));
                0
            }),
            _ => panic!("unexpected {op:?}"),
        }
    }
}

fn vertices(p: *mut EeGePrim) -> Vec<Vertex> {
    // SAFETY: a live test object
    let v = unsafe { ee_ge_prim_vertices(p) } as usize;
    with(|w| w.arrays[&v].clone())
}

#[test]
fn a_default_line_is_two_white_opaque_vertices_in_the_originals_order() {
    take_log();
    let p = unsafe { ee_ge_prim_new(host, std::ptr::null_mut(), EeGePrimKind::Line) };
    let log = take_log();
    assert_eq!(log[0], "screens");
    assert!(log[1].ends_with("type 1 count 2"));
    assert_eq!(
        &log[2..],
        [
            "point 0 0 0 0",
            "point 1 0 0 0",
            "normal 0 [0.0, 0.0, -1.0]",
            "normal 1 [0.0, 0.0, -1.0]",
            "color 0 [1.0, 1.0, 1.0]",
            "color 1 [1.0, 1.0, 1.0]",
            "opacity 0 1",
            "opacity 1 1",
            "uv 0 [0.0, 0.0]",
            "uv 1 [1.0, 1.0]",
        ]
    );
    unsafe {
        assert!(ee_ge_prim_texture(p, 0).is_null() && ee_ge_prim_texture(p, 1).is_null());
        assert!(ee_ge_prim_texture(p, 2).is_null());
        ee_ge_prim_destroy(p);
    }
}

#[test]
fn a_line_from_two_points_takes_their_colours_and_one_opacity() {
    take_log();
    let (s, e) = (Ge2DPoint::new(1.0, 2.0), Ge2DPoint::new(3.0, 4.0));
    let (cs, ce) = ([0.1f32, 0.2, 0.3], [0.4f32, 0.5, 0.6]);
    let p =
        unsafe { ee_ge_prim_new_line(host, std::ptr::null_mut(), &s, &e, cs.as_ptr().cast(), ce.as_ptr().cast(), 0.5) };
    let log = take_log();
    assert_eq!(log.len(), 2 + 8);
    assert_eq!(log[2], "point 0 1 2 0");
    assert_eq!(log[3], "point 1 3 4 0");
    let v = vertices(p);
    assert_eq!((v[0].color, v[1].color, v[0].opacity, v[1].opacity), (cs, ce, 0.5, 0.5));
    assert_eq!((v[0].uv, v[1].uv), ([0.0, 0.0], [1.0, 1.0]));
    unsafe {
        let mut out = Ge2DPoint::default();
        ee_ge_prim_xy(p, 1, &mut out);
        assert_eq!(out, e);
        ee_ge_prim_set_xy(p, 1, &Ge2DPoint::new(7.0, 8.0));
        assert_eq!(vertices(p)[1].point, Ge3DPoint { x: 7.0, y: 8.0, z: 0.0 });
        ee_ge_prim_destroy(p);
    }
}

#[test]
fn a_point_is_one_vertex_and_its_texture_slots_release_on_destruction() {
    take_log();
    let c = [0.25f32, 0.5, 0.75];
    let p =
        unsafe { ee_ge_prim_new_point(host, std::ptr::null_mut(), &Ge2DPoint::new(5.0, 6.0), c.as_ptr().cast(), 0.75) };
    let log = take_log();
    assert!(log[1].ends_with("type 1 count 1"));
    assert_eq!(&log[2..], ["point 0 5 6 0", "color 0 [0.25, 0.5, 0.75]", "opacity 0 0.75", "uv 0 [0.0, 0.0]"]);
    unsafe {
        assert_eq!(ee_ge_prim_opacity(p, 0), 0.75);
        ee_ge_prim_set_texture(p, 0x77 as *mut c_void, 1);
        // the same texture again: released, then referenced (no early return, as the original)
        ee_ge_prim_set_texture(p, 0x77 as *mut c_void, 1);
        ee_ge_prim_set_texture(p, 0x88 as *mut c_void, 0);
        assert_eq!(ee_ge_prim_texture(p, 1) as usize, 0x77);
        // past the screens: nothing (the original wrote past its array)
        ee_ge_prim_set_texture(p, 0x99 as *mut c_void, 5);
        take_log();
        let v = ee_ge_prim_vertices(p) as usize;
        ee_ge_prim_destroy(p);
        assert_eq!(take_log(), [format!("delete {v:x}"), "deref 88".into(), "deref 77".into()]);
    }
    let q = unsafe { ee_ge_prim_new(host, std::ptr::null_mut(), EeGePrimKind::Point) };
    let log = take_log();
    assert_eq!(&log[2..], ["opacity 0 1", "uv 0 [0.0, 0.0]"]);
    unsafe { ee_ge_prim_destroy(q) };
}

#[test]
fn a_rhombus_takes_its_rectangles_corners_and_keeps_its_depths() {
    take_log();
    let p = unsafe { ee_ge_prim_new(host, std::ptr::null_mut(), EeGePrimKind::Rhombus) };
    let log = take_log();
    // no screen count: a rhombus has no textures
    assert!(log[0].ends_with("type 1 count 4"));
    assert_eq!(log.iter().filter(|l| l.starts_with("get point")).count(), 4);
    unsafe {
        ee_ge_prim_set_depths(p, &[1.0, 2.0, 3.0, 4.0]);
        let r = GeRectangle { bottom: 8.0, left: 1.0, right: 5.0, top: 2.0 };
        ee_ge_prim_place(p, &r, 0.0);
        assert_eq!(*ee_ge_prim_rectangle(p), r);
        let v = vertices(p);
        let corners: Vec<_> = v.iter().map(|v| (v.point.x, v.point.y, v.point.z)).collect();
        assert_eq!(corners, [(1.0, 2.0, 1.0), (5.0, 2.0, 2.0), (1.0, 8.0, 3.0), (5.0, 8.0, 4.0)]);
        ee_ge_prim_place(p, std::ptr::null(), std::f32::consts::PI);
        let turned = vertices(p);
        assert!((turned[0].point.x - 5.0).abs() < 1e-3 && (turned[0].point.y - 8.0).abs() < 1e-3);
        assert_eq!(turned[0].point.z, 1.0);

        let q = ee_ge_prim_new(host, std::ptr::null_mut(), EeGePrimKind::Rhombus);
        take_log();
        ee_ge_prim_assign(q, p);
        assert_eq!(take_log().len(), 1);
        assert_eq!(*ee_ge_prim_rectangle(q), r);
        assert_eq!(vertices(q)[3].point, turned[3].point);
        ee_ge_prim_destroy(q);
        ee_ge_prim_destroy(p);
    }
}
