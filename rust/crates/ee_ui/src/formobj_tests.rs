//! The form object's tests: its golden (the census commit's UIForm over random call sequences, `build/ui16`), the
//! first 25 sequences replayed, and a changed answer that must fail.

use super::*;
use crate::obj::replay::Replay;

const GOLDEN: &str = include_str!("golden_formobj.txt");

/// The driver's music file names (`SetMusicFileName` and the record's), NUL-terminated (the last one's accent is the
/// driver source's UTF-8 read as Windows-1252 and written back as UTF-8: two non-ASCII characters).
const MUSIC: [&[u8]; 4] = [b"\0", b"none\0", b"music/menu.mp3\0", "musique/th\u{c3}\u{a8}me.mp3\0".as_bytes()];

fn fnv(bytes: &[u8]) -> isize {
    let mut h: u32 = 2166136261;
    for &b in bytes.iter().take_while(|&&b| b != 0) {
        h = (h ^ b as u32).wrapping_mul(16777619);
    }
    h as isize
}

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, b: isize, f: *mut f32) -> isize {
    // SAFETY: the golden's numbers are EeUiFormCall's (the driver printed them from it)
    let call: EeUiFormCall = unsafe { core::mem::transmute(call) };
    // SetMusicFileName's bytes travel as their hash
    let a = if call == EeUiFormCall::SetMusicFileName {
        MUSIC.iter().find(|m| fnv(m) == a).expect("a driver music name").as_ptr() as isize
    } else {
        a
    };
    // SAFETY: the target is the live form the test made; `f` is four live floats
    let r = unsafe { (*(target as *const EeUiForm)).call(call, a, b, f) };
    // the address answers as their values (the driver's ObjCall)
    // SAFETY: the addresses are the form's live members
    unsafe {
        match call {
            EeUiFormCall::MouseLocation => {
                let p = *(r as *const UiPoint);
                write_buf(f, [p.x, p.y, read_buf(f)[2], read_buf(f)[3]]);
                0
            }
            EeUiFormCall::Rectangle => {
                write_buf(f, rect_buf(*(r as *const UiRect)));
                0
            }
            EeUiFormCall::MusicFileName => fnv(core::ffi::CStr::from_ptr(r as *const c_char).to_bytes()),
            _ => r,
        }
    }
}

fn hexf(w: &str) -> f32 {
    f32::from_bits(u32::from_str_radix(w, 16).expect("hex float"))
}

/// Replays a golden -> the lines checked.
fn run(golden: &str) -> usize {
    let replay = Replay::new(golden, dispatch);
    let mut form: Option<Box<EeUiForm>> = None;
    replay.run(|r, w| match w[0] {
        "new" => {
            let n = |i: usize| w[i].parse::<i64>().expect("an integer");
            let (obj, f) = r.host();
            let host = Host { obj, f };
            let made = if n(1) == 1 {
                EeUiForm::new(host, n(2) as c_ulong, n(3) as c_ulong, 0x77, n(4) as c_ulong, n(5) != 0)
            } else {
                let d = DbForm {
                    label: [0; 0x64],
                    music: [0; 0x64],
                    unused_id: 0,
                    form_id: n(4) as i32,
                    control_count: n(2) as u32,
                    group_count: n(3) as u32,
                    left: hexf(w[15]),
                    right: hexf(w[16]),
                    bottom: hexf(w[18]),
                    top: hexf(w[17]),
                    designed_width: 800,
                    designed_height: 600,
                    help_delay: n(7) as u32,
                    unused: 0,
                    left_type: n(11) as u32,
                    right_type: n(12) as u32,
                    top_type: n(13) as u32,
                    bottom_type: n(14) as u32,
                    tabbing: n(9) != 0,
                    tool_tips: n(8) != 0,
                    exclusive: n(10) != 0,
                };
                EeUiForm::new_db(host, 0x77, &d, MUSIC[n(6) as usize], n(5) != 0)
            };
            let b = Box::new(made);
            r.target.set(&*b as *const EeUiForm as *const c_void);
            form = Some(b);
            true
        }
        "free" => {
            form.take().expect("a form").release();
            true
        }
        _ => false,
    });
    replay.checked.get()
}

#[test]
fn replays_the_census_forms_golden() {
    let checked = run(GOLDEN);
    assert!(checked > 3_000, "{checked} lines checked");
}

#[test]
#[should_panic]
fn a_changed_group_handle_fails() {
    // the first group the form makes answers another handle: the ops naming it differ
    let i = GOLDEN.find("\n= 8192 ").expect("a group made");
    let changed = format!("{}\n= 8193 {}", &GOLDEN[..i], &GOLDEN[i + 8..]);
    run(&changed);
}

#[test]
#[should_panic]
fn a_changed_control_index_fails() {
    // a control's index answers another slot: the form files it elsewhere
    let i = GOLDEN.find("\nset 205 4096 0 ").expect("the first control's index");
    let changed = format!("{}\nset 205 4096 1 {}", &GOLDEN[..i], &GOLDEN[i + 16..]);
    run(&changed);
}

#[test]
fn a_form_without_a_host_answers_its_getters() {
    let f = EeUiForm::new(Host { obj: core::ptr::null_mut(), f: None }, 3, 2, 0, 7, true);
    // SAFETY: no float buffer
    unsafe {
        assert_eq!(f.call(EeUiFormCall::ControlCount, 0, 0, core::ptr::null_mut()), 3);
        assert_eq!(f.call(EeUiFormCall::FormId, 0, 0, core::ptr::null_mut()), 7);
        assert_eq!(f.call(EeUiFormCall::UseGdiForText, 0, 0, core::ptr::null_mut()), 1);
        assert_eq!(f.call(EeUiFormCall::ReserveControlIndex, 5, 0, core::ptr::null_mut()), 0);
        assert_eq!(f.call(EeUiFormCall::ControlCount, 0, 0, core::ptr::null_mut()), 6);
        assert_eq!(f.call(EeUiFormCall::AvailableIndex, 0, 0, core::ptr::null_mut()), -1);
    }
}
