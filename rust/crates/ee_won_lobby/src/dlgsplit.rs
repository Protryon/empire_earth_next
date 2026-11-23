//! The lobby dialog host's compound cases composed in Rust (R6 WON round 34): an op the dialogs' Rust object sends
//! to the C++ host (LobbyDialogHost.cpp) that was a run of C++ statements is answered here in the game, its decisions
//! in Rust and each C++ statement one case of the host (or a row op, rows.rs). Reached from lobbydlg.rs's `op` for a
//! game dialog (`ee_won_lobbydlg_new_model`) while the network core is bound; the drivers' scripted hosts never reach
//! it, so the dialogs' goldens see the ops as before. won_dlg34_test runs the deleted cases (as they were) against
//! these over one stand-in world.
//!
//! - BadUsersInsert / BadUserId: BadUserItem is a Rust row (rows.rs `BadUser`); its constructor's texts (the record's
//!   name, moderator and comment, the ids as `%d`, the expiry in days, hours, minutes or seconds) are made here.
//! - AdminTitleRes: the title text built over the host's `GUIString` local.
//! - AdapterAddDefault: the default row and its name.
//! - The file dialog's search (FindFirst .. FindClose): the platform search is Rust's (ee_base's PlatFind); the
//!   names' lengths and the rows are the host's.

use std::ffi::{c_char, c_void};

use crate::lobbyctl::EeWonLobbyCtlOp as COp;
use crate::lobbydlg::{EeWonLobbyDlgHostFn, EeWonLobbyDlgOp as Op};
use crate::resource::EeWonLobbyRes as R;
use ee_base::plat_sys::constants::PlatFindData;
use ee_won_net::routeop::{EeWonRouteRecord, EE_WON_RTR_BAD_USER};

/// The calls out: a dialog op to the host the dialog was made with.
pub(crate) type Out<'a> = &'a dyn Fn(Op, isize, isize, isize) -> isize;

/// cbindgen:ignore
// the platform layer's search (ee_base plat_sys, `include/platform/PlatFile.h`)
mod sys {
    use ee_base::plat_sys::constants::PlatFindData;
    use std::ffi::{c_char, c_void};
    extern "C" {
        pub fn PlatFindFirst(pattern: *const c_char, out: *mut PlatFindData) -> *mut c_void;
        pub fn PlatFindNext(find: *mut c_void, out: *mut PlatFindData) -> i32;
        pub fn PlatFindClose(find: *mut c_void);
    }
}
use sys::*;

/// The file dialog's search record (EELobbyFind as it was: the search and its entry).
struct Find {
    find: *mut c_void,
    data: PlatFindData,
}

/// One bad user record's members the row reads (owned copies of its texts).
pub(crate) struct BadUserRec {
    pub name: Vec<u16>,
    pub user_id: u32,
    pub expiration: u32,
    pub moderator_name: Vec<u16>,
    pub moderator_id: u32,
    pub comment: Vec<u16>,
}

impl BadUserRec {
    /// The record's copy.
    ///
    /// # Safety
    /// The record's text pointers hold their lengths' units (or are NULL with length 0).
    pub(crate) unsafe fn of(r: &EeWonRouteRecord) -> Self {
        let units = |p: *const u16, n: usize| {
            if p.is_null() || n == 0 {
                Vec::new()
            } else {
                // SAFETY: the contract
                unsafe { std::slice::from_raw_parts(p, n) }.to_vec()
            }
        };
        Self {
            name: units(r.name, r.name_len),
            user_id: r.user_id,
            expiration: r.expiration,
            moderator_name: units(r.moderator_name, r.moderator_name_len),
            moderator_id: r.moderator_id,
            comment: units(r.comment, r.comment_len),
        }
    }
}

/// The op answered here (`None`: the C++ host's).
pub(crate) fn game_op(op: Op, a: isize, b: isize, c: isize, out: Out, host: EeWonLobbyDlgHostFn) -> Option<isize> {
    match op {
        Op::BadUsersInsert => {
            let recs = route_bad_users(b as *const c_void);
            bad_users_insert(a, &recs, c != 0, out, host);
            Some(0)
        }
        // the original read the id (0x7f5ca5) before allocating the list node
        Op::BadUserId => Some(crate::rows::bad_user_of(a).map_or(0, |s| s.values[1].get() as isize)),
        Op::AdminTitleRes => {
            let title = out(Op::TitleNew, b, 0, 0);
            out(Op::TitleAppendSpace, title, 0, 0);
            out(Op::TitleAppendName, title, c, 0);
            out(Op::LabelSetText, a, title, 0);
            out(Op::TitleFree, title, 0, 0);
            Some(0)
        }
        Op::AdapterAddDefault => {
            out(Op::AdapterInsertRow, a, 0, 0);
            out(Op::AdapterSetDefaultName, a, 0, 0);
            Some(0)
        }
        Op::FindFirst => {
            // SAFETY: a plain record of bytes and a number
            let mut f = Box::new(Find { find: std::ptr::null_mut(), data: unsafe { std::mem::zeroed() } });
            // SAFETY: a NUL-terminated pattern (the caller's), the record's entry
            f.find = unsafe { PlatFindFirst(a as *const c_char, &mut f.data) };
            Some(if f.find.is_null() { 0 } else { Box::into_raw(f) as isize })
        }
        Op::FindNameLength => Some(out(Op::NameLength, find_name(a), 0, 0)),
        Op::FileListInsert => Some(out(Op::FileListInsertName, a, find_name(b), c)),
        Op::FindNext => {
            // SAFETY: a record FindFirst made
            let f = unsafe { &mut *(a as *mut Find) };
            // SAFETY: its live search
            Some((unsafe { PlatFindNext(f.find, &mut f.data) } == 0) as isize)
        }
        Op::FindClose => {
            // SAFETY: a record FindFirst made, freed once
            let f = unsafe { Box::from_raw(a as *mut Find) };
            // SAFETY: its live search
            unsafe { PlatFindClose(f.find) };
            Some(0)
        }
        _ => None,
    }
}

/// A search record's entry name (NUL-terminated).
fn find_name(find: isize) -> isize {
    // SAFETY: a record FindFirst made
    unsafe { (*(find as *const Find)).data.mName.as_ptr() as isize }
}

/// The bad user records of a routing op, copied.
fn route_bad_users(op: *const c_void) -> Vec<BadUserRec> {
    unsafe extern "C" fn sink(context: *mut c_void, r: *const EeWonRouteRecord) {
        // SAFETY: the vector below; a record the op gives for the call
        unsafe { (*(context as *mut Vec<BadUserRec>)).push(BadUserRec::of(&*r)) }
    }
    let mut recs: Vec<BadUserRec> = Vec::new();
    if !op.is_null() {
        // SAFETY: the routing op the dialog was given (its Rust object), the sink above
        unsafe {
            ee_won_net::routeop::ee_won_routeop_records(
                op as *const ee_won_net::routeop::EeWonRouteOp,
                EE_WON_RTR_BAD_USER,
                sink,
                &mut recs as *mut Vec<BadUserRec> as *mut c_void,
            )
        };
    }
    recs
}

/// `aList->InsertItem(new BadUserItem(record, simple), -1)` for each record.
pub(crate) fn bad_users_insert(list: isize, recs: &[BadUserRec], simple: bool, out: Out, host: EeWonLobbyDlgHostFn) {
    for r in recs {
        let item = bad_user_new(r, simple, out, host);
        crate::rows::ctl(COp::ListInsert, list, item, 0);
    }
}

/// BadUserItem's constructor: the row made, then its texts and keys in the original's order.
fn bad_user_new(r: &BadUserRec, simple: bool, out: Out, host: EeWonLobbyDlgHostFn) -> isize {
    let item = crate::rows::bad_user_new(simple, host);
    let Some(s) = crate::rows::bad_user_of(item) else {
        return item;
    };
    let record = |u: &[u16]| out(Op::TextRecord, u.as_ptr() as isize, u.len() as isize, 0);
    let number = |n: u32| {
        let t = format!("{}", n as i32);
        crate::rows::ctl(COp::TextNew, t.as_ptr() as isize, t.len() as isize, 0)
    };
    s.strings[0].set(record(&r.name));
    s.strings[1].set(number(r.user_id));
    s.values[1].set(r.user_id);
    let (count, unit) = expiry(r.expiration);
    s.values[2].set(r.expiration);
    let count = number(count);
    s.strings[2].set(out(Op::TextFormatRes, unit as isize, count, 0));
    crate::rows::ctl(COp::Release, count, 0, 0);
    s.strings[3].set(record(&r.moderator_name));
    s.strings[4].set(number(r.moderator_id));
    s.values[4].set(r.moderator_id);
    s.strings[5].set(record(&r.comment));
    item
}

/// The expiry as a count and its unit's text: days, hours, minutes or seconds (one or several), or infinite.
pub(crate) fn expiry(seconds: u32) -> (u32, R) {
    let one_or_many = |n: u32, one: R, many: R| if n > 1 { many } else { one };
    if seconds > 86400 {
        let d = seconds / 86400;
        (d, one_or_many(d, R::BadUserDialog_OneDay_String, R::BadUserDialog_XDays_String))
    } else if seconds > 3600 {
        let d = seconds / 3600;
        (d, one_or_many(d, R::BadUserDialog_OneHour_String, R::BadUserDialog_XHours_String))
    } else if seconds > 60 {
        let d = seconds / 60;
        (d, one_or_many(d, R::BadUserDialog_OneMinute_String, R::BadUserDialog_XMinutes_String))
    } else if seconds > 0 {
        (seconds, one_or_many(seconds, R::BadUserDialog_OneSecond_String, R::BadUserDialog_XSeconds_String))
    } else {
        (seconds, R::BadUserDialog_Infinite_String)
    }
}

fn out_of(host: EeWonLobbyDlgHostFn) -> impl Fn(Op, isize, isize, isize) -> isize {
    move |op, a, b, c| match host {
        // SAFETY: the driver's host answers its ops with no dialog
        Some(f) => unsafe { f(std::ptr::null_mut(), op, a, b, c) },
        None => 0,
    }
}

/// `game_op` for the drivers (won_dlg34_test): the op over the host `host` (its row ops over the rows' world,
/// `ee_won_lobby_rows_host_world`); `INTPTR_MIN` when the op is not one of these.
#[no_mangle]
pub extern "C" fn ee_won_lobbydlg_split_op(
    op: crate::lobbydlg::EeWonLobbyDlgOp,
    a: isize,
    b: isize,
    c: isize,
    host: EeWonLobbyDlgHostFn,
) -> isize {
    let out = out_of(host);
    game_op(op, a, b, c, &out, host).unwrap_or(isize::MIN)
}

/// BadUsersInsert for the drivers: the records given directly (`n` of them at `recs`), as the routing op gave them.
///
/// # Safety
/// `recs` holds `n` records whose texts hold their lengths' units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbydlg_split_bad_users(
    list: isize,
    recs: *const EeWonRouteRecord,
    n: usize,
    simple: bool,
    host: EeWonLobbyDlgHostFn,
) {
    let recs: Vec<BadUserRec> = if recs.is_null() {
        Vec::new()
    } else {
        // SAFETY: the contract
        unsafe { std::slice::from_raw_parts(recs, n) }.iter().map(|r| unsafe { BadUserRec::of(r) }).collect()
    };
    let out = out_of(host);
    bad_users_insert(list, &recs, simple, &out, host);
}

#[cfg(test)]
#[path = "dlgsplit_tests.rs"]
mod tests;
