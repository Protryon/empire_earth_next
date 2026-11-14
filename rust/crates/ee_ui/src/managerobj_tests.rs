//! The manager object's tests: its golden (the round's base UIManager over random call sequences, `build/ui17`), the
//! first 25 sequences replayed, and changed answers that must fail.

use super::*;
use crate::obj::replay::{fnv, Replay};

const GOLDEN: &str = include_str!("golden_managerobj.txt");

/// The driver's music file names (`StartMusic`'s), NUL-terminated.
const MUSIC: [&[u8]; 5] =
    [b"music/menu.mp3\0", b"MUSIC/Menu.MP3\0", b"sounds\\eeintro.mp3\0", b"none\0", b"musique/th\xc3\xa9me.mp3\0"];

/// The driver's translated keys (`UpdateTranslatedKey`'s one character), NUL-terminated.
const KEYS: [&[u8]; 4] = [b"a\0", b"Z\0", b"\xc3\0", b"1\0"];

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the golden's numbers are EeUiManagerCall's (the driver printed them from it)
    let call: EeUiManagerCall = unsafe { core::mem::transmute(call) };
    // the string arguments travel as their hash (StartMusic) or their byte (UpdateTranslatedKey)
    let a = match call {
        EeUiManagerCall::StartMusic => {
            // SAFETY: the names are NUL-terminated
            MUSIC.iter().find(|m| unsafe { fnv(m.as_ptr() as isize) } == a).expect("a driver music name").as_ptr()
                as isize
        }
        EeUiManagerCall::UpdateTranslatedKey => {
            KEYS.iter().find(|k| k[0] as isize == a).expect("a driver key").as_ptr() as isize
        }
        _ => a,
    };
    // AddEventToTrigger's entity and ChangeCandidateWindow's count travel in f[0] (the driver's calls have two
    // integers)
    // SAFETY: four live floats
    let c = match call {
        EeUiManagerCall::AddEventToTrigger | EeUiManagerCall::ChangeCandidateWindow => (unsafe { *f }) as isize,
        _ => 0,
    };
    // SAFETY: the target is the live manager the test made; `f` is four live floats
    unsafe { (*(target as *const EeUiManager)).call(call, a, b, c, f) }
}

/// Replays a golden -> the lines checked.
fn run(golden: &str) -> usize {
    let replay = Replay::new(golden, dispatch);
    let mut manager: Option<Box<EeUiManager>> = None;
    replay.run(|r, w| match w[0] {
        "new" => {
            let n = |i: usize| w[i].parse::<i64>().expect("an integer");
            let (obj, f) = r.host();
            let c = Config {
                rasterizer: n(1) as isize,
                sound: n(2) as isize,
                surfaces: n(3) as c_ulong,
                depth: n(4) as c_ulong,
                string_table: n(5) as isize,
                screen: n(6) as c_ulong,
            };
            let b = Box::new(EeUiManager::new(Host { obj, f }, c));
            r.target.set(&*b as *const EeUiManager as *const c_void);
            manager = Some(b);
            true
        }
        "free" => {
            manager.take().expect("a manager").release();
            true
        }
        _ => false,
    });
    replay.checked.get()
}

#[test]
fn replays_the_managers_golden() {
    let checked = run(GOLDEN);
    assert!(checked > 3_000, "{checked} lines checked");
}

#[test]
#[should_panic]
fn a_changed_texture_size_fails() {
    // the UI texture answers another width: every bitmap's coordinates differ
    let i = GOLDEN.find("\nset 410 36864 0 ").expect("the texture's size");
    let changed = format!("{}\nset 410 36864 0 42800000 {}", &GOLDEN[..i], &GOLDEN[i + 26..]);
    run(&changed);
}

#[test]
#[should_panic]
fn a_changed_lock_fails() {
    // the UI lock is made as another handle: every Enter and Leave of it names that handle
    let i = GOLDEN.find("\n= 45057 ").expect("the UI lock made");
    let changed = format!("{}\n= 45059 {}", &GOLDEN[..i], &GOLDEN[i + 9..]);
    run(&changed);
}

#[test]
fn a_manager_without_a_host_answers_its_getters() {
    let c = Config { rasterizer: 5, sound: 0, surfaces: 2, depth: 16, string_table: 9, screen: 1 };
    let m = EeUiManager::new(Host { obj: core::ptr::null_mut(), f: None }, c);
    // SAFETY: no float buffer and no pointer argument
    unsafe {
        let call = |c: EeUiManagerCall, a: isize, b: isize| m.call(c, a, b, 0, core::ptr::null_mut());
        assert_eq!(call(EeUiManagerCall::Rasterizer, 0, 0), 5);
        assert_eq!(call(EeUiManagerCall::NumberOfSurfaces, 0, 0), 2);
        assert_eq!(call(EeUiManagerCall::CurrentForm, 0, 0), 0);
        assert_eq!(call(EeUiManagerCall::Push, 0x40, 0), 0);
        assert_eq!(call(EeUiManagerCall::CurrentForm, 0, 0), 0x40);
        assert_eq!(call(EeUiManagerCall::IsFormOnStack, 0x40, 0), 1);
        assert_eq!(call(EeUiManagerCall::IsFormVisible, 0x40, 0), 1);
        assert_eq!(call(EeUiManagerCall::FormCount, 0, 0), 1);
        call(EeUiManagerCall::SetNumberOfFonts, 3, 0);
        call(EeUiManagerCall::SetFont, 2, 0x77);
        assert_eq!(call(EeUiManagerCall::Font, 2, 0), 0x77);
        // past the fonts: NULL (the original read past the vector)
        assert_eq!(call(EeUiManagerCall::Font, 3, 0), 0);
        call(EeUiManagerCall::Pop, 0, 0);
        assert_eq!(call(EeUiManagerCall::FormCount, 0, 0), 0);
        // a pop of an empty stack does nothing
        call(EeUiManagerCall::Pop, 0, 0);
        assert_eq!(call(EeUiManagerCall::VisibleFormCount, 0, 0), 0);
    }
}

#[test]
fn the_bitmap_table_cuts_every_bitmap_once() {
    let mut seen = [false; BITMAPS];
    for (b, _) in CUTS {
        assert!(!seen[b as usize]);
        seen[b as usize] = true;
    }
    let mut released = [false; BITMAPS];
    for b in RELEASES {
        assert!(!released[b as usize]);
        released[b as usize] = true;
    }
    assert!(seen.iter().all(|&s| s) && released.iter().all(|&s| s));
}
