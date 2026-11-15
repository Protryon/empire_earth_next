//! The file lists' golden (`golden_filelistobj.txt`): the first sequences the round-21 driver (`build/ui21`) printed
//! over UIFileListBox.cpp and UIFileComboBox.cpp at the round's base (a world of directories and file names, the
//! selection, its line and the file lookups), replayed op for op against the Rust object. Calls are
//! `EeUiFileListCall`'s, `AddExtension`'s argument an index into the driver's extensions; `new 0` a list box, `new 1`
//! a combo box.

use super::*;
use crate::obj::replay::{host_current, Replay};

const GOLDEN: &str = include_str!("golden_filelistobj.txt");
const EXTENSIONS: [&core::ffi::CStr; 6] = [c".bmp", c".sav", c".scn", c".SCN", c".ab", c".bmpx"];

unsafe fn dispatch(target: *const c_void, call: u32, a: isize, _b: isize, _f: *mut f32) -> isize {
    // SAFETY: the test's live object; the call numbers are the enum's (the golden's)
    unsafe {
        let l = *(target as *const *mut EeUiFileList);
        let call = core::mem::transmute::<u32, EeUiFileListCall>(call);
        let a = if call == EeUiFileListCall::AddExtension { EXTENSIONS[a as usize].as_ptr() as isize } else { a };
        ee_ui_file_list_call(l, call, a)
    }
}

fn replay(golden: &str) -> usize {
    let r = Replay::new(golden, dispatch);
    let slot: *mut *mut EeUiFileList = Box::into_raw(Box::new(core::ptr::null_mut()));
    r.target.set(slot as *const c_void);
    r.run(|_, w| match w[0] {
        "new" => {
            // SAFETY: the test's slot
            unsafe { *slot = ee_ui_file_list_new(core::ptr::null_mut(), Some(host_current), w[1] == "1") };
            true
        }
        "free" => {
            // SAFETY: made by the new line
            unsafe { ee_ui_file_list_free(*slot) };
            true
        }
        _ => false,
    });
    // SAFETY: made above
    drop(unsafe { Box::from_raw(slot) });
    r.checked.get()
}

#[test]
fn golden_replays() {
    assert!(replay(GOLDEN) > 500);
}

#[test]
#[should_panic(expected = "golden line")]
fn a_file_never_found_fails() {
    // every lookup answered "not found": the deletes the original made are missing
    let g: Vec<String> = GOLDEN.lines().map(|l| l.replace("= 0 00000001 ", "= 3 00000000 ")).collect();
    replay(&g.join("\n"));
}

#[test]
fn an_extension_matches_only_whole() {
    let l = EeUiFileList::new(Host { obj: core::ptr::null_mut(), f: None }, false);
    // SAFETY: NUL-terminated bytes
    unsafe { l.call(EeUiFileListCall::AddExtension, c".sav".as_ptr() as isize) };
    assert_eq!(l.extensions.borrow().as_slice(), &[b".sav".to_vec()]);
    // SAFETY: as above (no host: no call out)
    assert_eq!(unsafe { l.call(EeUiFileListCall::FilePath, 0) }, 0);
}
