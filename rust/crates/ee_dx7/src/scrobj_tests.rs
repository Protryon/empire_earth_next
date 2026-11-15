//! `DX7Screen`/`DX7Surface` objects over the recorder and a test host (the C++ shells' operations: surfaces as boxed
//! test records with a reference count, names as boxed strings, windows as numbers).

use core::cell::RefCell;
use core::ffi::c_void;

use crate::api::test_recorder::*;
use crate::api::*;
use crate::screen::VideoMode;
use crate::scrobj::{self, EeDx7Screen};
use crate::surface::LogicalMetrics;
use crate::surfobj::{EeDx7ObjOp, EeDx7Surface};

/// A test `DX7Surface`: the object, the reference count, the base's logical members.
struct TestSurface {
    object: *mut EeDx7Surface,
    refs: u32,
    logical: (u32, u32, u32),
}

thread_local! {
    static EVENTS: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static PROBE_FAILS: RefCell<bool> = const { RefCell::new(false) };
}

fn event(s: String) {
    EVENTS.with(|e| e.borrow_mut().push(s));
}

fn events() -> Vec<String> {
    EVENTS.with(|e| core::mem::take(&mut *e.borrow_mut()))
}

fn test_surface(h: H) -> &'static mut TestSurface {
    unsafe { &mut *(h as *mut TestSurface) }
}

/// Deletes a surface as `~DX7Surface` does (the object's destructor over a recorder that logs nothing).
fn delete_surface(h: H) {
    let s = unsafe { Box::from_raw(h as *mut TestSurface) };
    let (mut gx, _) = Recorder::pair();
    unsafe { EeDx7Surface::destroy(s.object, &mut gx) };
    event(format!("delete {}", h as usize % 100_000));
}

unsafe extern "C" fn host(op: EeDx7ObjOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    match op {
        EeDx7ObjOp::NameNew => {
            let name = unsafe { &*(obj as *const String) }.clone();
            Box::into_raw(Box::new(name)) as isize
        }
        EeDx7ObjOp::NameDelete => {
            drop(unsafe { Box::from_raw(obj as *mut String) });
            0
        }
        EeDx7ObjOp::SurfaceNew => {
            let desc = unsafe { *(a as *const SurfaceDescription) };
            let s = Box::into_raw(Box::new(TestSurface { object: core::ptr::null_mut(), refs: 1, logical: (0, 0, 0) }));
            unsafe { (*s).object = EeDx7Surface::create(host, s as H, obj, desc, b as H) };
            event(format!("new surface gx {}", obj as usize));
            s as isize
        }
        EeDx7ObjOp::SurfaceObject => test_surface(obj).object as isize,
        EeDx7ObjOp::SurfaceReference => {
            test_surface(obj).refs += 1;
            0
        }
        EeDx7ObjOp::SurfaceDereference => {
            let s = test_surface(obj);
            s.refs -= 1;
            if s.refs == 0 {
                delete_surface(obj);
            }
            0
        }
        EeDx7ObjOp::SurfaceAllocatedSize => unsafe { EeDx7Surface::allocated_size(test_surface(obj).object) as isize },
        EeDx7ObjOp::SurfaceMetrics => {
            let s = test_surface(obj);
            let d = unsafe { (*s.object).desc };
            let m = LogicalMetrics {
                logical_width: s.logical.0,
                logical_height: s.logical.1,
                pixel_width: d.mWidth as u32,
                pixel_height: d.mHeight as u32,
                logical_shift: s.logical.2,
            };
            unsafe { *(a as *mut LogicalMetrics) = m };
            0
        }
        EeDx7ObjOp::ProbeWindow => {
            if PROBE_FAILS.with(|f| *f.borrow()) {
                return K_NATIVE as isize;
            }
            unsafe { *(a as *mut H) = 77 as H };
            event("probe window".into());
            0
        }
        EeDx7ObjOp::WindowOs => obj as isize + 1000,
        EeDx7ObjOp::WindowDelete => {
            event(format!("delete window {}", obj as usize));
            0
        }
        EeDx7ObjOp::DispatchEvents => {
            event("dispatch".into());
            0
        }
    }
}

fn desc(kind: u32, format: u32, depth: u32, w: u32, h: u32) -> SurfaceDescription {
    SurfaceDescription {
        mType: kind as _,
        mPixelFormat: format as _,
        mSurfaceVolatility: 0,
        mWidth: w as _,
        mHeight: h as _,
        m14: 0,
        mMipLevelCount: 1,
        mPixelDepth: depth as _,
        mIs3DRenderable: false,
        mIsTransparencyEnabled: false,
        mNoAlpha: false,
    }
}

/// A test surface over GX surface `gx` with a 16-bit 8 x 4 description (`kind` its type).
fn new_surface(gx: usize, kind: u32) -> H {
    let d = desc(kind, 7, 16, 8, 4);
    unsafe { host(EeDx7ObjOp::SurfaceNew, gx as H, &d as *const SurfaceDescription as isize, 0) as H }
}

fn obj(h: H) -> *mut EeDx7Surface {
    test_surface(h).object
}

#[test]
fn a_new_surface_is_unlocked_white_and_alone() {
    let s = new_surface(5, 1);
    let o = unsafe { &*obj(s) };
    assert_eq!(o.transparency, [1.0, 1.0, 1.0]);
    assert!(!o.locked && !o.text_locked && o.attached.is_empty() && o.memory.is_null());
    assert_eq!((o.gx as usize, o.bytes_per_row), (5, 0));
    unsafe { host(EeDx7ObjOp::SurfaceDereference, s, 0, 0) };
    events();
}

#[test]
fn attach_references_and_detach_releases() {
    let (mut gx, mut h) = Recorder::pair();
    let (a, b) = (new_surface(10, 1), new_surface(11, 1));
    assert_eq!(unsafe { EeDx7Surface::attach(obj(a), &mut gx, &mut h, b) }, K_OK);
    assert_eq!(test_surface(b).refs, 2);
    assert_eq!(unsafe { &(*obj(a)).attached }, &vec![b]);
    // the size: 2 bytes x 8 x 4, plus the attached one's
    assert_eq!(unsafe { EeDx7Surface::allocated_size(obj(a)) }, 128);
    gx.queue("delete_attached", &[GXERR_SURFACELOST]);
    assert_eq!(unsafe { EeDx7Surface::detach(obj(a), &mut gx, &mut h, 0) }, K_NATIVE);
    assert_eq!(test_surface(b).refs, 2);
    assert_eq!(unsafe { EeDx7Surface::detach(obj(a), &mut gx, &mut h, 0) }, K_OK);
    assert_eq!(test_surface(b).refs, 1);
    gx.queue("add_attached", &[GXERR_SURFACELOST]);
    assert_eq!(unsafe { EeDx7Surface::attach(obj(a), &mut gx, &mut h, b) }, K_NATIVE);
    assert_eq!(test_surface(b).refs, 1);
    assert_eq!(
        gx.log.borrow().clone(),
        [
            "surface_add_attached 10 11",
            "surface_delete_attached 10 11",
            "native_error 887601c2",
            "surface_delete_attached 10 11",
            "surface_add_attached 10 11",
            "native_error 887601c2",
        ]
    );
    for s in [a, b] {
        unsafe { host(EeDx7ObjOp::SurfaceDereference, s, 0, 0) };
    }
    events();
}

#[test]
fn destroy_dereferences_the_attached_and_keeps_back_buffers() {
    let (mut gx, mut h) = Recorder::pair();
    let (a, b) = (new_surface(20, 2), new_surface(21, 7));
    unsafe { EeDx7Surface::attach(obj(a), &mut gx, &mut h, b) };
    unsafe { host(EeDx7ObjOp::SurfaceDereference, b, 0, 0) }; // the driver's own reference
    events();
    let (mut gx2, _) = Recorder::pair();
    let (ta, tb) = (obj(a), obj(b));
    // the back buffer's GX surface is never released; the primary's is
    unsafe { (*ta).text_locked = true };
    let held = unsafe { Box::from_raw(a as *mut TestSurface) };
    unsafe { EeDx7Surface::destroy(held.object, &mut gx2) };
    assert_eq!(events(), [format!("delete {}", b as usize % 100_000)]);
    assert_eq!(gx2.log.borrow().clone(), ["surface_release 20"]);
    let _ = tb;
}

#[test]
fn lock_records_the_memory_and_pitch_and_unlock_clears() {
    let (mut gx, mut h) = Recorder::pair();
    let s = new_surface(30, 1);
    assert_eq!(unsafe { EeDx7Surface::lock(obj(s), &mut gx, &mut h) }, K_OK);
    let o = unsafe { &*obj(s) };
    assert!(o.locked);
    assert_eq!((o.memory as usize, o.bytes_per_row), (0x1000, 1280));
    gx.queue("unlock", &[GXERR_SURFACELOST]);
    assert_eq!(unsafe { EeDx7Surface::unlock(obj(s), &mut gx, &mut h) }, K_NATIVE);
    assert!(unsafe { (*obj(s)).locked });
    gx.queue("unlock", &[GXERR_NOTLOCKED]);
    assert_eq!(unsafe { EeDx7Surface::unlock(obj(s), &mut gx, &mut h) }, K_OK);
    assert!(!unsafe { (*obj(s)).locked });
    // a lost lock keeps the old address
    gx.queue("lock", &[GXERR_SURFACELOST]);
    assert_eq!(unsafe { EeDx7Surface::lock(obj(s), &mut gx, &mut h) }, K_NOT_SUPPORTED);
    assert_eq!(unsafe { (*obj(s)).memory } as usize, 0x1000);
    unsafe { host(EeDx7ObjOp::SurfaceDereference, s, 0, 0) };
    events();
}

#[test]
fn text_lock_twice_is_already() {
    let s = new_surface(40, 1);
    unsafe {
        assert_eq!(EeDx7Surface::text_lock(obj(s)), K_OK);
        assert_eq!(EeDx7Surface::text_lock(obj(s)), K_ALREADY);
        assert_eq!(EeDx7Surface::text_unlock(obj(s)), K_OK);
        assert_eq!(EeDx7Surface::text_unlock(obj(s)), K_ALREADY);
        host(EeDx7ObjOp::SurfaceDereference, s, 0, 0);
    }
    events();
}

#[test]
fn logical_size_prefers_the_explicit_size_then_the_shift() {
    let s = new_surface(50, 1);
    unsafe {
        assert_eq!(EeDx7Surface::logical_size(obj(s), 0, 0, false), 8);
        assert_eq!(EeDx7Surface::logical_size(obj(s), 0, 1, true), 2);
        assert_eq!(EeDx7Surface::logical_size(obj(s), 0, 3, true), 4); // 4 >> 3 is 0: the pixels
        assert_eq!(EeDx7Surface::logical_size(obj(s), 99, 1, false), 99);
        host(EeDx7ObjOp::SurfaceDereference, s, 0, 0);
    }
    events();
}

#[test]
fn clear_and_copy_scale_the_logical_rectangles() {
    let (mut gx, mut h) = Recorder::pair();
    let (a, b) = (new_surface(60, 1), new_surface(61, 1));
    test_surface(a).logical = (0, 0, 1); // 2x pixels
    test_surface(b).logical = (4, 2, 0); // an explicit 4 x 2 logical size over 8 x 4 pixels
    let r = [3.0f32, 1.0, 2.0, 0.5]; // GERectangle: bottom, left, right, top
    unsafe {
        EeDx7Surface::clear(obj(a), &mut gx, &mut h, r.as_ptr(), [1.0, 0.0, 0.0], 1.0);
        EeDx7Surface::clear(obj(a), &mut gx, &mut h, core::ptr::null(), [0.0, 0.0, 1.0], 1.0);
        EeDx7Surface::copy_to(obj(a), &mut gx, &mut h, b, r.as_ptr(), r.as_ptr(), true);
        EeDx7Surface::set_transparency_color(obj(a), &mut gx, [0.0, 1.0, 0.0]);
    }
    assert_eq!(unsafe { (*obj(a)).transparency }, [0.0, 1.0, 0.0]);
    assert_eq!(
        gx.log.borrow().clone(),
        [
            "surface_blt 60 dst=2,1,4,6 src=0 srcr=- flags=400 fill=f800",
            "surface_blt 60 dst=- src=0 srcr=- flags=400 fill=1f",
            "surface_blt 61 dst=2,1,4,6 src=60 srcr=2,1,4,6 flags=8008000 fill=-",
            "surface_set_color_key 60 7e0",
        ]
    );
    for s in [a, b] {
        unsafe { host(EeDx7ObjOp::SurfaceDereference, s, 0, 0) };
    }
    events();
}

fn mode_desc(w: u32, h: u32, bits: u32) -> SurfaceDesc {
    SurfaceDesc {
        dwWidth: w,
        dwHeight: h,
        format: PixelFormat {
            dwFlags: 0x40,
            dwRGBBitCount: bits,
            dwRBitMask: if bits == 32 { 0xff0000 } else { 0xf800 },
            dwGBitMask: if bits == 32 { 0xff00 } else { 0x7e0 },
            dwBBitMask: if bits == 32 { 0xff } else { 0x1f },
            ..Default::default()
        },
        ..Default::default()
    }
}

#[test]
fn a_screen_lists_its_modes_finds_the_current_one_and_registers() {
    let (mut gx, mut h) = Recorder::pair();
    gx.modes = vec![mode_desc(640, 480, 16), mode_desc(800, 600, 16), mode_desc(1024, 768, 32)];
    gx.display_mode = mode_desc(1024, 768, 32);
    PROBE_FAILS.with(|f| *f.borrow_mut() = true);
    let name = String::from("primary");
    let owner = 0x5150 as H;
    let p = unsafe { EeDx7Screen::create(host, owner, NULL, 3 as H, &name as *const String as H, true, false) };
    let before = scrobj::screen_count();
    unsafe { EeDx7Screen::start(p, &mut gx, &mut h) };
    let s = unsafe { &*p };
    assert_eq!(s.modes.len(), 3);
    assert_eq!(s.current_mode, 2);
    assert_eq!(unsafe { &*(s.name as *const String) }, "primary");
    // the probe window failed: no 32-bit support, nothing else asked
    assert!(!s.device32 && !s.texture32 && s.attached_to_desktop && !s.shares_primary);
    assert!(scrobj::screen_count() > before);
    assert_eq!(unsafe { *EeDx7Screen::desktop_rectangle(p) }, 768.0);
    assert_eq!(unsafe { *EeDx7Screen::desktop_rectangle(p).add(2) }, 1024.0);
    // SetMode does nothing windowed; full screen it records the index even when the mode change fails
    assert_eq!(unsafe { EeDx7Screen::set_mode(p, &mut gx, &mut h, 0) }, K_OK);
    assert_eq!(unsafe { (*p).current_mode }, 2);
    unsafe { (*p).full_screen = true };
    gx.queue("set_mode", &[GXERR_SURFACELOST]);
    assert_eq!(unsafe { EeDx7Screen::set_mode(p, &mut gx, &mut h, 1) }, K_NATIVE);
    assert_eq!(unsafe { (*p).current_mode }, 1);
    unsafe { EeDx7Screen::destroy(p, &mut gx) };
    assert!(!(0..scrobj::screen_count()).any(|i| scrobj::screen_at(i) == owner));
    assert_eq!(gx.log.borrow().last().map(String::as_str), Some("dd_release 3"));
    PROBE_FAILS.with(|f| *f.borrow_mut() = false);
    events();
}

#[test]
fn capture_and_release_count_and_track_full_screen() {
    let (mut gx, mut h) = Recorder::pair();
    gx.modes = vec![mode_desc(800, 600, 16)];
    PROBE_FAILS.with(|f| *f.borrow_mut() = true);
    let name = String::from("s");
    let p = unsafe { EeDx7Screen::create(host, 0x6160 as H, NULL, 4 as H, &name as *const String as H, true, true) };
    unsafe { EeDx7Screen::start(p, &mut gx, &mut h) };
    let before = scrobj::capture_state().captured;
    assert_eq!(unsafe { EeDx7Screen::capture(p, &mut gx, &mut h, 5 as H, true) }, K_OK);
    let after = scrobj::capture_state();
    assert_eq!(after.captured, before.wrapping_add(1));
    assert!(unsafe { (*p).full_screen });
    assert_eq!(unsafe { EeDx7Screen::release(p, &mut gx, &mut h, 5 as H) }, K_OK);
    assert!(!unsafe { (*p).full_screen });
    assert_eq!(scrobj::capture_state().captured, before);
    // the window's platform window (the test host's WindowOs: + 1000) goes to the cooperative level
    assert!(gx.log.borrow().iter().any(|l| l.contains("1005")));
    unsafe { EeDx7Screen::destroy(p, &mut gx) };
    PROBE_FAILS.with(|f| *f.borrow_mut() = false);
    events();
}

#[test]
fn create_surface_builds_the_flip_chain_on_the_screen() {
    let (mut gx, mut h) = Recorder::pair();
    gx.modes = vec![mode_desc(800, 600, 16)];
    PROBE_FAILS.with(|f| *f.borrow_mut() = true);
    let name = String::from("s");
    let p = unsafe { EeDx7Screen::create(host, 0x7170 as H, NULL, 4 as H, &name as *const String as H, true, true) };
    unsafe { EeDx7Screen::start(p, &mut gx, &mut h) };
    events();
    let mut d = desc(2, 7, 16, 0, 0);
    d.m14 = 2;
    let mut out = NULL;
    assert_eq!(unsafe { EeDx7Screen::create_surface(p, &mut gx, &mut h, &mut d, &mut out) }, K_OK);
    // the primary, then two back buffers, each attached to the one before
    assert_eq!(events().len(), 3);
    assert_eq!(d.mType, 7);
    let primary = unsafe { &*obj(out) };
    assert_eq!(primary.parent as usize, 0x7170);
    assert_eq!(primary.attached.len(), 1);
    let back = primary.attached[0];
    assert_eq!(unsafe { (*obj(back)).attached.len() }, 1);
    assert_eq!(unsafe { (*obj(back)).desc.mType }, 7);
    unsafe { host(EeDx7ObjOp::SurfaceDereference, out, 0, 0) };
    assert_eq!(events().len(), 3); // the chain is deleted from the primary down
    unsafe { EeDx7Screen::destroy(p, &mut gx) };
    PROBE_FAILS.with(|f| *f.borrow_mut() = false);
}

#[test]
fn the_probe_window_is_captured_released_and_deleted() {
    let (mut gx, mut h) = Recorder::pair();
    gx.modes = vec![mode_desc(800, 600, 16)];
    let name = String::from("s");
    let p = unsafe { EeDx7Screen::create(host, 0x8180 as H, NULL, 4 as H, &name as *const String as H, true, true) };
    unsafe { EeDx7Screen::start(p, &mut gx, &mut h) };
    let e = events();
    assert_eq!(e.first().map(String::as_str), Some("probe window"));
    assert!(e.contains(&"delete window 77".to_string()));
    assert_eq!(e.last().map(String::as_str), Some("dispatch"));
    // the probe's 2 x 2 surface was made and dereferenced
    assert!(e.iter().any(|l| l.starts_with("new surface")) && e.iter().any(|l| l.starts_with("delete ")));
    let _ = VideoMode::default();
    unsafe { EeDx7Screen::destroy(p, &mut gx) };
}
