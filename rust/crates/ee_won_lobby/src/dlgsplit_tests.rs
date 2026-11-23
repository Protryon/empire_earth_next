use super::*;
use std::cell::{Cell, RefCell};

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
    static NEXT: Cell<isize> = const { Cell::new(0x7000) };
}

fn log(s: String) {
    LOG.with(|l| l.borrow_mut().push(s));
}

fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn next() -> isize {
    NEXT.with(|n| {
        n.set(n.get() + 0x10);
        n.get()
    })
}

// the rows' host: shells, texts and releases logged; the selection off
unsafe extern "C" fn rows_world(_: *mut c_void, op: COp, a: isize, b: isize, _: isize) -> isize {
    match op {
        COp::RowShellNew => {
            let i = next();
            log(format!("NewRow {}", b));
            i
        }
        COp::TextNew => {
            // SAFETY: the caller's bytes
            let t = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
            log(format!("TextNew {}", String::from_utf8_lossy(t)));
            next()
        }
        COp::ListInsert => {
            log(format!("Insert {a:#x} {b:#x}"));
            0
        }
        COp::Release => {
            log(format!("Release {a:#x}"));
            0
        }
        COp::ItemGet => 0,
        _ => {
            log(format!("row op {}", op as i32));
            0
        }
    }
}

unsafe extern "C" fn dlg_world(_: *mut c_void, op: Op, a: isize, b: isize, c: isize) -> isize {
    match op {
        Op::TextRecord => {
            // SAFETY: the caller's units
            let u = unsafe { std::slice::from_raw_parts(a as *const u16, b as usize) };
            log(format!("TextRecord {}", String::from_utf16_lossy(u)));
            next()
        }
        Op::TextFormatRes => {
            log(format!("Format {a} {b:#x}"));
            next()
        }
        Op::NameLength => {
            // SAFETY: a NUL-terminated name
            let n = unsafe { std::ffi::CStr::from_ptr(a as *const c_char) }.to_bytes().len() as isize;
            n
        }
        Op::FileListInsertName => {
            // SAFETY: a NUL-terminated name
            let n = unsafe { std::ffi::CStr::from_ptr(b as *const c_char) }.to_string_lossy().into_owned();
            log(format!("File {}", &n[..c as usize]));
            0
        }
        _ => {
            log(format!("dlg op {} {a:#x} {b:#x} {c:#x}", op as i32));
            0x100
        }
    }
}

fn units(s: &str) -> Vec<u16> {
    s.encode_utf16().collect()
}

#[test]
fn expiry_units() {
    assert_eq!(expiry(0), (0, R::BadUserDialog_Infinite_String));
    assert_eq!(expiry(1), (1, R::BadUserDialog_OneSecond_String));
    assert_eq!(expiry(60), (60, R::BadUserDialog_XSeconds_String));
    assert_eq!(expiry(61), (1, R::BadUserDialog_OneMinute_String));
    assert_eq!(expiry(3600), (60, R::BadUserDialog_XMinutes_String));
    assert_eq!(expiry(3601), (1, R::BadUserDialog_OneHour_String));
    assert_eq!(expiry(7200), (2, R::BadUserDialog_XHours_String));
    assert_eq!(expiry(86400), (24, R::BadUserDialog_XHours_String));
    assert_eq!(expiry(86401), (1, R::BadUserDialog_OneDay_String));
    assert_eq!(expiry(u32::MAX), (49710, R::BadUserDialog_XDays_String));
}

#[test]
fn bad_user_rows_texts_and_ids() {
    crate::rows::ee_won_lobby_rows_host_world(Some(rows_world));
    take();
    let r = BadUserRec {
        name: units("Ann"),
        user_id: 0xffff_fffe,
        expiration: 7200,
        moderator_name: units("Mod"),
        moderator_id: 7,
        comment: units("spam"),
    };
    let out = out_of(Some(dlg_world));
    bad_users_insert(0x40, std::slice::from_ref(&r), true, &out, Some(dlg_world));
    let log = take();
    assert_eq!(log[0], "NewRow 0");
    assert_eq!(log[1], "TextRecord Ann");
    assert_eq!(log[2], "TextNew -2");
    assert_eq!(log[3], "TextNew 2");
    assert_eq!(log[4], format!("Format {} {:#x}", R::BadUserDialog_XHours_String as isize, 0x7000 + 0x40));
    assert!(log[5].starts_with("Release"));
    assert_eq!(&log[6..9], ["TextRecord Mod", "TextNew 7", "TextRecord spam"]);
    assert!(log[9].starts_with("Insert 0x40"));
    // the row's WON id, read in Rust
    let item = 0x7010;
    assert_eq!(ee_won_lobbydlg_split_op(Op::BadUserId, item, 0, 0, Some(dlg_world)), 0xffff_fffe);
    assert_eq!(ee_won_lobbydlg_split_op(Op::BadUserId, 0x1234, 0, 0, Some(dlg_world)), 0);
    crate::rows::ee_won_lobby_rows_host_world(None);
}

#[test]
fn admin_title_and_default_adapter() {
    take();
    assert_eq!(ee_won_lobbydlg_split_op(Op::AdminTitleRes, 0x10, 5, 0x30, Some(dlg_world)), 0);
    let t = Op::TitleNew as i32;
    assert_eq!(
        take(),
        [
            format!("dlg op {t} 0x5 0x0 0x0"),
            format!("dlg op {} 0x100 0x0 0x0", Op::TitleAppendSpace as i32),
            format!("dlg op {} 0x100 0x30 0x0", Op::TitleAppendName as i32),
            format!("dlg op {} 0x10 0x100 0x0", Op::LabelSetText as i32),
            format!("dlg op {} 0x100 0x0 0x0", Op::TitleFree as i32),
        ]
    );
    ee_won_lobbydlg_split_op(Op::AdapterAddDefault, 0x20, 0, 0, Some(dlg_world));
    assert_eq!(
        take(),
        [
            format!("dlg op {} 0x20 0x0 0x0", Op::AdapterInsertRow as i32),
            format!("dlg op {} 0x20 0x0 0x0", Op::AdapterSetDefaultName as i32)
        ]
    );
    assert_eq!(ee_won_lobbydlg_split_op(Op::Relayout, 0, 0, 0, Some(dlg_world)), isize::MIN);
}

#[test]
fn file_search_lists_a_directory() {
    let dir = std::env::temp_dir().join(format!("ee_dlgsplit_{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    for n in ["one.ees", "two.ees", "x.txt"] {
        std::fs::write(dir.join(n), b"").unwrap();
    }
    let pattern = format!("{}\\*.ees\0", dir.display());
    let op = |o: Op, a: isize, b: isize, c: isize| ee_won_lobbydlg_split_op(o, a, b, c, Some(dlg_world));
    take();
    let find = op(Op::FindFirst, pattern.as_ptr() as isize, 0, 0);
    assert_ne!(find, 0);
    let mut names = Vec::new();
    loop {
        let length = op(Op::FindNameLength, find, 0, 0);
        if length > 4 {
            op(Op::FileListInsert, 0x50, find, length - 4);
        }
        if op(Op::FindNext, find, 0, 0) == 0 {
            break;
        }
    }
    op(Op::FindClose, find, 0, 0);
    names.extend(take());
    names.sort();
    assert_eq!(names, ["File one", "File two"]);
    let none = format!("{}\\*.none\0", dir.display());
    assert_eq!(op(Op::FindFirst, none.as_ptr() as isize, 0, 0), 0);
    std::fs::remove_dir_all(&dir).unwrap();
}
