//! The directory, account and group list requests the game makes (`RustServerOp::NewGetDir` and the others), their
//! members in each kind's order (net round 29; ServerOp.cpp's `ReqMembers` built them): numbers, wide texts as
//! UTF-16 LE bytes (`WideAppendUtf16LE`), byte strings. The ops are [`new_op`]'s (reqdir.rs, reqlast.rs).

use std::ffi::{c_char, c_void, CStr};

use crate::adminop::Member;
use crate::reqdir::{
    EE_WON_REQOPS_ACCOUNT_GET, EE_WON_REQOPS_ACCOUNT_UPDATE, EE_WON_REQOPS_DIR_GET, EE_WON_REQOPS_DIR_MULTI,
    EE_WON_REQOPS_DIR_SERVICE,
};
use crate::reqlast::{EE_WON_REQOPS_CREATE_ACCOUNT, EE_WON_REQOPS_GROUP_LIST};
use crate::reqop::EeWonReqOp;
use crate::reqops::new_op;

/// A request's members, in order.
#[derive(Default)]
struct Members(Vec<Member>);

impl Members {
    fn n(mut self, num: i64) -> Members {
        self.0.push(Member { num, bytes: Vec::new() });
        self
    }
    fn s(mut self, bytes: &[u8]) -> Members {
        self.0.push(Member { num: 0, bytes: bytes.to_vec() });
        self
    }
    /// a wide text's UTF-16 LE bytes
    fn w(self, text: &[u16]) -> Members {
        self.s(&crate::text::utf16_to_le_bytes(text))
    }
    /// an empty profile (no newsletters, birth date or custom action)
    fn profile(self) -> Members {
        self.n(0).n(0).n(0).n(0).n(0).n(0).s(&[])
    }
}

fn op(kind: i32, m: Members) -> *mut EeWonReqOp {
    match new_op(kind, &m.0) {
        Some(o) => EeWonReqOp::new(o),
        None => std::ptr::null_mut(),
    }
}

/// `count` `wchar_t`s of `wchar` bytes as UTF-16 units.
///
/// # Safety
/// `text` holds `count` characters of `wchar` (2 or 4) bytes, or `count` is 0.
unsafe fn wide(text: *const c_void, count: usize, wchar: u32) -> Vec<u16> {
    if text.is_null() || count == 0 {
        return Vec::new();
    }
    if wchar == 2 {
        // SAFETY: the caller's units
        unsafe { std::slice::from_raw_parts(text as *const u16, count) }.to_vec()
    } else {
        // SAFETY: the caller's code points
        crate::text::utf16_from_utf32(unsafe { std::slice::from_raw_parts(text as *const u32, count) })
    }
}

/// `GetDirOp(context, path, flags)` with its data types (C strings).
///
/// # Safety
/// `path` holds `path_len` characters of `wchar` bytes; `types` holds `type_count` C strings (or is NULL for 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqmake_get_dir(
    flags: u32,
    path: *const c_void,
    path_len: usize,
    wchar: u32,
    types: *const *const c_char,
    type_count: i32,
) -> *mut EeWonReqOp {
    // SAFETY: the caller's text
    let mut m =
        Members::default().n(i64::from(flags)).w(&unsafe { wide(path, path_len, wchar) }).n(i64::from(type_count));
    for i in 0..type_count.max(0) as usize {
        // SAFETY: the caller's C strings
        m = m.s(unsafe { CStr::from_ptr(*types.add(i)) }.to_bytes());
    }
    op(EE_WON_REQOPS_DIR_GET, m)
}

/// `GetMultiDirOp(context, flags)` with each path (`GetMultiDirOp::AddPath`: the op's flags, no data types).
///
/// # Safety
/// `paths` and `lengths` hold `count` entries; each path `lengths[i]` characters of `wchar` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqmake_get_multi_dir(
    flags: u32,
    paths: *const *const c_void,
    lengths: *const usize,
    count: i32,
    wchar: u32,
) -> *mut EeWonReqOp {
    let mut m = Members::default().n(i64::from(count));
    for i in 0..count.max(0) as usize {
        // SAFETY: the caller's arrays and texts
        let p = unsafe { wide(*paths.add(i), *lengths.add(i), wchar) };
        m = m.n(i64::from(flags)).w(&p).n(0);
    }
    op(EE_WON_REQOPS_DIR_MULTI, m)
}

/// `GetServiceOp(context, path, name, netaddr)` (`Init`'s flags: the services, their display names, data objects,
/// names and addresses).
///
/// # Safety
/// `path` and `name` hold their counts of `wchar`-byte characters; `addr` holds `addr_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqmake_get_service(
    path: *const c_void,
    path_len: usize,
    name: *const c_void,
    name_len: usize,
    wchar: u32,
    addr: *const u8,
    addr_len: usize,
) -> *mut EeWonReqOp {
    // SAFETY: the caller's texts and bytes
    let (p, n, a) =
        unsafe { (wide(path, path_len, wchar), wide(name, name_len, wchar), crate::ffi::bytes(addr, addr_len)) };
    op(EE_WON_REQOPS_DIR_SERVICE, Members::default().n(0x0600_0822).w(&p).w(&n).n(1).s(a).n(0))
}

/// `GetAccountOp` with an empty profile.
#[no_mangle]
pub extern "C" fn ee_won_reqmake_get_account() -> *mut EeWonReqOp {
    op(EE_WON_REQOPS_ACCOUNT_GET, Members::default().profile())
}

/// `UpdateAccountOp(password, email)` with an empty profile.
///
/// # Safety
/// `password` holds `password_len` characters of `wchar` bytes; `email` holds `email_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqmake_update_account(
    password: *const c_void,
    password_len: usize,
    wchar: u32,
    email: *const u8,
    email_len: usize,
) -> *mut EeWonReqOp {
    // SAFETY: the caller's text and bytes
    let (p, e) = unsafe { (wide(password, password_len, wchar), crate::ffi::bytes(email, email_len)) };
    op(EE_WON_REQOPS_ACCOUNT_UPDATE, Members::default().w(&p).s(e).profile())
}

/// `CreateAccountOp(user, password, email)` with the lobby's profile: newsletter 1 and the birth date.
///
/// # Safety
/// `user` and `password` hold their counts of `wchar`-byte characters; `email` holds `email_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_reqmake_create_account(
    user: *const c_void,
    user_len: usize,
    password: *const c_void,
    password_len: usize,
    wchar: u32,
    email: *const u8,
    email_len: usize,
    newsletter: bool,
    month: u8,
    day: u8,
    year: u16,
) -> *mut EeWonReqOp {
    // SAFETY: the caller's texts and bytes
    let (u, p, e) = unsafe {
        (wide(user, user_len, wchar), wide(password, password_len, wchar), crate::ffi::bytes(email, email_len))
    };
    let m = Members::default().w(&u).w(&p).s(e).n(1).n(1).n(i64::from(newsletter));
    op(EE_WON_REQOPS_CREATE_ACCOUNT, m.n(i64::from(month)).n(i64::from(day)).n(i64::from(year)).n(0).n(0).s(&[]))
}

/// A routing server's group list request with its flags.
#[no_mangle]
pub extern "C" fn ee_won_reqmake_group_list(flags: u8) -> *mut EeWonReqOp {
    op(EE_WON_REQOPS_GROUP_LIST, Members::default().n(i64::from(flags)))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn members(m: Members) -> Vec<(i64, Vec<u8>)> {
        m.0.into_iter().map(|x| (x.num, x.bytes)).collect()
    }

    #[test]
    fn the_members_are_in_each_kinds_order() {
        let w: Vec<u16> = "ab".encode_utf16().collect();
        assert_eq!(
            members(Members::default().n(0x10).w(&w).n(1).s(b"t")),
            vec![(0x10, vec![]), (0, b"a\0b\0".to_vec()), (1, vec![]), (0, b"t".to_vec())]
        );
        assert_eq!(members(Members::default().profile()).len(), 7);
        // UTF-32 text becomes UTF-16 units (a code point past the BMP: a surrogate pair)
        let cp: [u32; 2] = [0x41, 0x1F600];
        assert_eq!(unsafe { wide(cp.as_ptr().cast(), 2, 4) }, vec![0x41, 0xD83D, 0xDE00]);
        let units: [u16; 1] = [0x42];
        assert_eq!(unsafe { wide(units.as_ptr().cast(), 1, 2) }, vec![0x42]);
        assert!(unsafe { wide(std::ptr::null(), 3, 2) }.is_empty());
    }

    #[test]
    fn every_maker_makes_an_op() {
        let path: Vec<u16> = "/Dir".encode_utf16().collect();
        let types = [c"T1".as_ptr()];
        let ops = unsafe {
            [
                ee_won_reqmake_get_dir(0x10, path.as_ptr().cast(), path.len(), 2, types.as_ptr(), 1),
                ee_won_reqmake_get_multi_dir(0x10, [path.as_ptr().cast()].as_ptr(), [path.len()].as_ptr(), 1, 2),
                ee_won_reqmake_get_service(
                    path.as_ptr().cast(),
                    4,
                    path.as_ptr().cast(),
                    4,
                    2,
                    b"\x01\x02".as_ptr(),
                    2,
                ),
                ee_won_reqmake_get_account(),
                ee_won_reqmake_update_account(path.as_ptr().cast(), 4, 2, b"e@x".as_ptr(), 3),
                ee_won_reqmake_create_account(
                    path.as_ptr().cast(),
                    4,
                    path.as_ptr().cast(),
                    4,
                    2,
                    b"e".as_ptr(),
                    1,
                    true,
                    1,
                    2,
                    1990,
                ),
                ee_won_reqmake_group_list(3),
            ]
        };
        for o in ops {
            assert!(!o.is_null());
            unsafe { crate::reqop::ee_won_reqop_free(o) };
        }
    }
}
