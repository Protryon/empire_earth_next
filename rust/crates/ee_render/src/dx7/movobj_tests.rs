//! `DX7Movie`'s object over a test host (the thread system, the mixer and the surfaces as logged numbers) and a fake
//! pl_mpeg: the constructor's order, the playback thread's paths, the render, the frame conversion and the sound.

use core::cell::RefCell;
use core::ffi::c_void;

use super::*;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static LOCK_FAILS: RefCell<bool> = const { RefCell::new(false) };
    static FRAME: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take() -> Vec<String> {
    LOG.with(|l| core::mem::take(&mut *l.borrow_mut()))
}

unsafe extern "C" fn host(op: EeDx7MovieOp, obj: *mut c_void, a: isize, b: isize) -> isize {
    let o = obj as usize;
    match op {
        EeDx7MovieOp::CsNew => {
            log(format!("cs new {a}"));
            0x100 + a
        }
        EeDx7MovieOp::EventNew => {
            log(format!("event new {a}"));
            0x200 + a
        }
        EeDx7MovieOp::ThreadNew => {
            log(format!("thread new {a}"));
            0x300
        }
        EeDx7MovieOp::FileName => {
            let name = b"C:\\Movies\\intro.bik\0";
            unsafe { core::ptr::copy_nonoverlapping(name.as_ptr(), a as *mut u8, name.len()) };
            0
        }
        EeDx7MovieOp::Open => {
            let path =
                unsafe { core::ffi::CStr::from_ptr(a as *const core::ffi::c_char) }.to_string_lossy().into_owned();
            log(format!("open {path}"));
            if path.contains("missing") {
                0
            } else {
                0x400
            }
        }
        EeDx7MovieOp::MixerRate => 48000,
        EeDx7MovieOp::FpGet => 0,
        EeDx7MovieOp::ActiveScreen => 0x500,
        EeDx7MovieOp::ActiveTarget => 0x600,
        EeDx7MovieOp::SurfaceInfo => {
            let info: [u32; 4] = if o == 0x600 { [7, 16, 640, 480] } else { [7, 16, 4, 2] };
            unsafe { core::ptr::copy_nonoverlapping(info.as_ptr(), a as *mut u32, 4) };
            0
        }
        EeDx7MovieOp::ScreenCreateSurface => {
            let d = unsafe { &*(a as *const SurfaceDescription) };
            log(format!("create surface {} {} {} {}", d.mType, d.mPixelFormat, d.mWidth, d.mHeight));
            unsafe { *(b as *mut H) = 0x700 as H };
            FRAME.with(|f| *f.borrow_mut() = vec![0; 16]);
            0
        }
        EeDx7MovieOp::SurfaceLock => LOCK_FAILS.with(|f| if *f.borrow() { 0xd } else { 0 }),
        EeDx7MovieOp::SurfaceMemory => {
            let p = FRAME.with(|f| f.borrow_mut().as_mut_ptr()) as usize;
            unsafe {
                *(a as *mut usize) = p;
                *(a as *mut usize).add(1) = 8;
            }
            0
        }
        EeDx7MovieOp::SurfaceCopy => {
            let r = unsafe { core::slice::from_raw_parts(b as *const f32, 8) };
            log(format!("copy {o:x} to {a:x} {r:?}"));
            0
        }
        _ => {
            log(format!("{op:?} {o:x} {a}"));
            0
        }
    }
}

#[derive(Default)]
struct Plm {
    decodes: u32,
    ended_after: u32,
    log: Vec<String>,
}

impl MovieSys for Plm {
    fn video_streams(&mut self, _plm: H) -> i32 {
        1
    }
    fn width(&mut self, _plm: H) -> i32 {
        4
    }
    fn height(&mut self, _plm: H) -> i32 {
        2
    }
    fn set_loop(&mut self, _plm: H, looping: bool) {
        self.log.push(format!("loop {looping}"));
    }
    fn audio_streams(&mut self, _plm: H) -> i32 {
        1
    }
    fn sample_rate(&mut self, _plm: H) -> i32 {
        24000
    }
    fn set_audio_enabled(&mut self, _plm: H, enabled: bool) {
        self.log.push(format!("audio {enabled}"));
    }
    fn decode(&mut self, _plm: H, _seconds: f64) {
        self.decodes += 1;
    }
    fn has_ended(&mut self, _plm: H) -> bool {
        self.decodes >= self.ended_after
    }
    fn destroy(&mut self, plm: H) {
        self.log.push(format!("destroy {:x}", plm as usize));
    }
    fn counter_frequency(&mut self) -> i64 {
        1000
    }
    fn counter_ticks(&mut self) -> i64 {
        10 * i64::from(self.decodes)
    }
    fn sleep(&mut self, _milliseconds: u32) {}
}

fn new_movie(rect: Option<[f32; 4]>, looping: bool) -> *mut EeDx7Movie {
    MOVIE_SECTION.store(NULL, SeqCst);
    let p = unsafe { EeDx7Movie::create(host, 0x42 as H, 0x43 as H, 0, rect, looping) };
    take();
    p
}

#[test]
fn the_constructor_names_references_and_makes_its_objects_in_order() {
    MOVIE_SECTION.store(NULL, SeqCst);
    let p = unsafe { EeDx7Movie::create(host, 0x42 as H, 0x43 as H, 1, None, false) };
    assert_eq!(
        take(),
        [
            "cs new 0",
            "MovieReference 42 0",
            "cs new 183",
            "cs new 183",
            "event new 185",
            "event new 186",
            "EventClear 2ba 0",
            "thread new 184"
        ]
    );
    let m = unsafe { &*p };
    assert_eq!(m.filename, b"C:\\Movies\\intro.bik\0");
    assert!(m.just_started.load(SeqCst) && !m.is_playing.load(SeqCst) && !m.has_rectangle);
    unsafe { EeDx7Movie::destroy(p) };
    assert_eq!(
        take(),
        ["EventDelete 2ba 0", "EventDelete 2b9 0", "ThreadDelete 300 0", "CsDelete 1b7 0", "CsDelete 1b7 0"]
    );
}

#[test]
fn a_missing_file_ends_the_thread_at_once() {
    let p = new_movie(None, false);
    unsafe { (*p).filename = b"missing.bik\0".to_vec() };
    let mut plm = Plm::default();
    unsafe { EeDx7Movie::play(p, &mut plm) };
    assert_eq!(
        take(),
        [
            "CsEnter 100 0",
            "ThreadPriority 300 0",
            "FpSet 0 0",
            "open missing.mpg",
            "MovieDereference 42 0",
            "CsLeave 100 0",
            "EventSet 2ba 0"
        ]
    );
    assert!(!unsafe { (*p).just_started.load(SeqCst) });
    unsafe { EeDx7Movie::destroy(p) };
    take();
}

#[test]
fn a_movie_plays_to_its_end_and_tears_down() {
    let p = new_movie(None, false);
    let mut plm = Plm { ended_after: 3, ..Default::default() };
    unsafe { EeDx7Movie::play(p, &mut plm) };
    let log = take();
    assert!(log.contains(&"create surface 4 7 4 2".to_string()));
    assert!(log.contains(&"MixerSource 0 0".to_string()));
    assert!(log.contains(&"SurfaceDereference 700 0".to_string()));
    assert_eq!(plm.decodes, 3);
    assert_eq!(plm.log, ["loop false", "audio true", "destroy 400"]);
    let m = unsafe { &*p };
    assert!(m.is_finished.load(SeqCst) && !m.is_playing.load(SeqCst) && m.frame_buffer.load(SeqCst).is_null());
    // the destination defaults to the frame's size; the source is the decoded size
    assert_eq!(unsafe { *m.destination.get() }, [0.0, 0.0, 4.0, 2.0]);
    assert_eq!(unsafe { *m.source.get() }, [0.0, 0.0, 4.0, 2.0]);
    assert_eq!(unsafe { EeDx7Movie::render(p) }, 7);
    unsafe { EeDx7Movie::destroy(p) };
    take();
}

#[test]
fn render_copies_the_frame_into_the_rectangle_while_playing() {
    let p = new_movie(Some([2.0, 4.0, 6.0, 1.0]), true); // left, top, right, bottom
    unsafe {
        (*p).is_playing.store(true, SeqCst);
        (*p).just_started.store(false, SeqCst);
        (*p).frame_buffer.store(0x700 as H, SeqCst);
        *(*p).source.get() = [0.0, 0.0, 4.0, 2.0];
    }
    assert_eq!(unsafe { EeDx7Movie::render(p) }, 0);
    let log = take();
    assert_eq!(log[0], "EventWait 2b9 0");
    assert!(log.contains(&"copy 700 to 600 [0.0, 0.0, 4.0, 2.0, 2.0, 4.0, 6.0, 1.0]".to_string()));
    assert!(
        log.contains(&"SurfaceReference 700 0".to_string()) && log.contains(&"SurfaceDereference 700 0".to_string())
    );
    unsafe { EeDx7Movie::destroy(p) };
    take();
}

#[test]
fn a_decoded_frame_is_packed_and_signalled_unless_the_lock_fails() {
    let p = new_movie(None, false);
    FRAME.with(|f| *f.borrow_mut() = vec![0; 16]);
    unsafe {
        (*p).frame_buffer.store(0x700 as H, SeqCst);
        *(*p).source.get() = [0.0, 0.0, 2.0, 2.0];
    }
    let bgra: [u8; 16] = [0xff, 0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0xff, 0, 0xff, 0xff, 0xff, 0];
    unsafe { EeDx7Movie::decoded_frame(p, bgra.as_ptr(), 8) };
    // R5G6B5 rows of 8 bytes: blue, green; red, white
    assert_eq!(FRAME.with(|f| f.borrow()[..4].to_vec()), [0x1f, 0, 0xe0, 0x07]);
    assert_eq!(FRAME.with(|f| f.borrow()[8..12].to_vec()), [0, 0xf8, 0xff, 0xff]);
    assert!(unsafe { (*p).is_ready.load(SeqCst) });
    assert!(take().contains(&"EventSet 2b9 0".to_string()));
    LOCK_FAILS.with(|f| *f.borrow_mut() = true);
    unsafe { (*p).is_ready.store(false, SeqCst) };
    unsafe { EeDx7Movie::decoded_frame(p, bgra.as_ptr(), 8) };
    assert!(!unsafe { (*p).is_ready.load(SeqCst) });
    LOCK_FAILS.with(|f| *f.borrow_mut() = false);
    unsafe { EeDx7Movie::destroy(p) };
    take();
}

#[test]
fn pack_rows_cover_every_format() {
    let px = [0x10u8, 0x80, 0xf0, 0x55];
    let mut out = [0u8; 4];
    let pack = |format: c_ulong, out: &mut [u8; 4]| unsafe { pack_bgrx_row(px.as_ptr(), out.as_mut_ptr(), 1, format) };
    pack(0, &mut out);
    assert_eq!(u32::from_le_bytes(out), 0xfff0_8010);
    pack(2, &mut out);
    assert_eq!(u16::from_le_bytes([out[0], out[1]]), 0xff81);
    pack(3, &mut out);
    assert_eq!(u16::from_le_bytes([out[0], out[1]]), 0x8000 | (0xf0 >> 3) << 10 | (0x80 >> 3) << 5 | 0x10 >> 3);
    pack(6, &mut out);
    assert_eq!(u16::from_le_bytes([out[0], out[1]]), (0xf0 >> 3) << 11 | (0x80 >> 3) << 6 | (0x10 >> 3) << 1 | 1);
    pack(7, &mut out);
    assert_eq!(u16::from_le_bytes([out[0], out[1]]), (0xf0 >> 3) << 11 | (0x80 >> 2) << 5 | 0x10 >> 3);
}

#[test]
fn decoded_sound_is_mixed_under_the_sound_section() {
    let p = new_movie(None, false);
    unsafe { (*(*p).ring.get()).set_step(1.0) };
    let samples = [0.5f32; 8];
    unsafe { EeDx7Movie::decoded_samples(p, samples.as_ptr(), 4) };
    let mut out = [0.0f32; 4];
    unsafe { EeDx7Movie::mix(p, out.as_mut_ptr(), 2) };
    assert!(out.iter().all(|v| *v > 0.0));
    assert_eq!(take(), ["CsEnter 1b7 0", "CsLeave 1b7 0", "CsEnter 1b7 0", "CsLeave 1b7 0"]);
    unsafe { EeDx7Movie::destroy(p) };
    take();
}
