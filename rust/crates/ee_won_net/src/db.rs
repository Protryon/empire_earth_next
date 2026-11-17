//! WONDB (`src/Empire Earth/WONAPI/WONDB`): the account profile (`ProfileData`, wholly here: the C++ class is a
//! shim over a [`EeWonProfile`]), the DBProxy service's requests and replies (`DBProxyOp`, `CreateAccountOp`,
//! `GetAccountOp`, `UpdateAccountOp`, `GetUserIdFromNameOp`, the two email requests), the contest call
//! (`ContestCallOp`) and the event report (`EventReportOp`).
//!
//! The ops stay C++ objects (`ServerRequestOp`s over the C++ socket machinery, the encryption keys C++
//! objects); they hand their members over and act on the verdicts. Reached by nothing in the game: the WON
//! servers are gone.

use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::dirops::EeWonDirBytes;
use crate::ffi::EeWonByteSink;
use crate::wire::{PastEnd, Reader, Writer};

// ---- ProfileData

/// An account's profile: newsletter subscriptions, a custom action and a birth date.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonProfile {
    pub birth_month: u8,
    pub birth_day: u8,
    pub birth_year: u16,
    pub newsletters: BTreeMap<u16, bool>,
    pub custom_action_id: u16,
    /// `None`: a NULL buffer
    pub custom_action_data: Option<Vec<u8>>,
}

impl EeWonProfile {
    /// `GetNewsletterSub`: false for an unknown newsletter.
    pub fn newsletter(&self, id: u16) -> bool {
        self.newsletters.get(&id).copied().unwrap_or(false)
    }

    /// `GetBirthDate`: month, day << 8, year << 16.
    pub fn birth_date(&self) -> u32 {
        u32::from(self.birth_month) | u32::from(self.birth_day) << 8 | u32::from(self.birth_year) << 16
    }

    /// `PackRequestData`: the field count, each newsletter (as set or as get), the custom action (when its id is
    /// not 0), the birth date.
    pub fn pack(&self, w: &mut Writer, is_set: bool) {
        let at = w.data().len();
        w.append_short(0);
        let mut count: u16 = 0;
        for (&id, &on) in &self.newsletters {
            w.append_short(1);
            w.append_short(if is_set { 3 } else { 2 });
            w.append_short(id as i16);
            if is_set {
                w.append_bool(on);
            }
            count = count.wrapping_add(1);
        }
        if self.custom_action_id != 0 {
            w.append_short(2);
            let n = self.custom_action_data.as_ref().map_or(0, |d| d.len());
            w.append_short((n as u32).wrapping_add(2) as i16);
            w.append_short(self.custom_action_id as i16);
            if let Some(d) = &self.custom_action_data {
                w.append_bytes(d);
            }
            count = count.wrapping_add(1);
        }
        w.append_short(3);
        w.append_short(4);
        w.append_byte(self.birth_month);
        w.append_byte(self.birth_day);
        w.append_short(self.birth_year as i16);
        count = count.wrapping_add(1);
        w.set_bytes(at, &count.to_le_bytes());
    }

    /// `UnpackGetData`: reads the fields until the reply runs short (what was read stays). Returns the reader's
    /// position.
    pub fn unpack(&mut self, data: &[u8], pos: usize) -> usize {
        let mut r = Reader::new(data);
        let _ = r.read_bytes(pos.min(data.len()));
        let _ = self.unpack_fields(&mut r);
        r.pos()
    }

    fn unpack_fields(&mut self, r: &mut Reader) -> Result<(), PastEnd> {
        let count = r.read_short()? as u16;
        for _ in 0..count {
            let field = r.read_short()? as u16;
            let length = r.read_short()? as u16;
            match field {
                1 => {
                    let id = r.read_short()? as u16;
                    // the entry exists (false) before its value is read
                    let entry = self.newsletters.entry(id).or_insert(false);
                    *entry = r.read_bool()?;
                }
                2 => {
                    // the action id is read and dropped; a length under 2 is past the end
                    r.read_short()?;
                    let n = (length as usize).checked_sub(2).ok_or(PastEnd)?;
                    self.custom_action_data = Some(r.read_bytes(n)?.to_vec());
                }
                3 => {
                    self.birth_month = r.read_byte()?;
                    self.birth_day = r.read_byte()?;
                    self.birth_year = r.read_short()? as u16;
                }
                // an unknown field's payload is not skipped
                _ => {}
            }
        }
        Ok(())
    }
}

// ---- requests

/// `DBProxyOp::GetNextRequest`: the DBProxy header, the message and sub-message types, the request data.
pub fn proxy_request(field_size: u8, message: i16, sub: i16, data: Option<&[u8]>) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    w.append_byte(5);
    w.append_short(14);
    w.append_short(message);
    w.append_short(sub);
    if let Some(d) = data {
        w.append_bytes(d);
    }
    w.to_message()
}

fn small_header(w: &mut Writer, message: i16, sub: i16) {
    w.append_byte(5);
    w.append_short(14);
    w.append_short(message);
    w.append_short(sub);
}

/// `CreateAccountOp`'s public key request.
pub fn create_key_request(field_size: u8) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    small_header(&mut w, 2, 1);
    w.to_message()
}

/// `CreateAccountOp`'s session key message: the session key encrypted with the server's public key.
pub fn create_session_request(field_size: u8, encrypted_key: Option<&[u8]>) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    small_header(&mut w, 4, 1);
    w.append_buffer(encrypted_key, 2);
    w.to_message()
}

/// `CreateAccountOp`'s account data before its encryption: name, password, email and profile.
pub fn create_account_data(name: &[u16], password: &[u16], email: &[u8], profile: &EeWonProfile) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    small_header(&mut w, 1002, 2);
    w.append_wstring(name, 2);
    w.append_wstring(password, 2);
    w.append_string(email, 2);
    profile.pack(&mut w, true);
    w.data().to_vec()
}

/// `CreateAccountOp`'s account message: the encrypted account data.
pub fn create_account_request(field_size: u8, encrypted: Option<&[u8]>) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    small_header(&mut w, 5, 1);
    if let Some(d) = encrypted {
        w.append_bytes(d);
    }
    w.to_message()
}

/// `GetAccountOp::GetNextRequest`: the profile fields wanted.
pub fn get_account_request(field_size: u8, profile: &EeWonProfile) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    small_header(&mut w, 1002, 6);
    profile.pack(&mut w, false);
    w.to_message()
}

/// `UpdateAccountOp::GetNextRequest`: the new password, the email and the profile.
pub fn update_account_request(field_size: u8, password: &[u16], email: &[u8], profile: &EeWonProfile) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    small_header(&mut w, 1002, 4);
    w.append_wstring(password, 2);
    w.append_string(email, 2);
    profile.pack(&mut w, true);
    w.to_message()
}

/// `GetUserIdFromNameOp::RunHook`'s request data: the name.
pub fn user_id_data(name: &[u16]) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    w.append_wstring(name, 2);
    w.to_message()
}

/// The email requests' data (`RequestPasswordEmailOp`: the user name; `RequestAccountInfoEmailOp`: the email
/// address), then the profile.
pub fn email_data(name: Option<&[u16]>, email: &[u8], profile: &EeWonProfile) -> Vec<u8> {
    let mut w = Writer::new(0, true);
    match name {
        Some(n) => w.append_wstring(n, 2),
        None => w.append_string(email, 2),
    }
    profile.pack(&mut w, true);
    w.to_message()
}

/// `ContestCallOp::GetNextRequest`: the user, the procedure, a zero GUID and the data.
pub fn contest_request(field_size: u8, user_id: u32, proc_num: i16, data: Option<&[u8]>) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    w.append_byte(3);
    w.append_byte(4);
    w.append_byte(1);
    w.append_long(user_id as i32);
    w.append_short(proc_num);
    for _ in 0..16 {
        w.append_byte(0);
    }
    w.append_buffer(data, 0);
    w.to_message()
}

/// An event detail: a number or a text.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Detail {
    pub detail_type: u16,
    /// the data type as the op stores it (0 a number, 1 a text, another: no value)
    pub data_type: i32,
    pub number: u32,
    pub text: Vec<u16>,
}

/// An event attachment.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Attachment<'a> {
    pub description: Vec<u16>,
    pub content_type: u8,
    pub data: Option<&'a [u8]>,
}

/// An event report's fields.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct Event<'a> {
    pub activity_type: u16,
    pub server: Option<(u16, Vec<u16>, &'a [u8])>,
    pub client: Option<(Vec<u16>, &'a [u8])>,
    pub user: Option<(u32, Vec<u16>)>,
    pub details: Vec<Detail>,
    pub attachments: Vec<Attachment<'a>>,
}

/// `EventReportOp::GetNextRequest`.
pub fn event_request(field_size: u8, e: &Event) -> Vec<u8> {
    let mut w = Writer::new(field_size, true);
    w.append_byte(5);
    w.append_short(4);
    w.append_short(1);
    let mut flags = 0u8;
    if e.server.is_some() {
        flags |= 2;
    }
    if e.client.is_some() {
        flags |= 4;
    }
    if e.user.is_some() {
        flags |= 8;
    }
    w.append_byte(flags);
    w.append_byte(e.details.len() as u8);
    w.append_byte(e.attachments.len() as u8);
    w.append_short(e.activity_type as i16);
    if let Some((t, name, addr)) = &e.server {
        w.append_short(*t as i16);
        w.append_wstring(name, 2);
        w.append_string(addr, 2);
    }
    if let Some((name, addr)) = &e.client {
        w.append_wstring(name, 2);
        w.append_string(addr, 2);
    }
    if let Some((id, name)) = &e.user {
        w.append_short(1);
        w.append_long(*id as i32);
        w.append_wstring(name, 2);
    }
    for d in &e.details {
        w.append_short(d.detail_type as i16);
        w.append_byte(d.data_type as u8);
        match d.data_type {
            0 => w.append_long(d.number as i32),
            1 => w.append_wstring(&d.text, 2),
            _ => {}
        }
    }
    for a in &e.attachments {
        w.append_wstring(&a.description, 2);
        w.append_byte(a.content_type);
        w.append_buffer(a.data, 4);
    }
    w.to_message()
}

// ---- replies

/// The reply kinds (`ee_won_db_reply`).
pub const EE_WON_DB_PROXY: i32 = 0;
/// `CreateAccountOp` waiting for the server's public key
pub const EE_WON_DB_CREATE_KEY: i32 = 1;
/// `CreateAccountOp` past the key
pub const EE_WON_DB_CREATE: i32 = 2;
pub const EE_WON_DB_GET_ACCOUNT: i32 = 3;
pub const EE_WON_DB_UPDATE_ACCOUNT: i32 = 4;
pub const EE_WON_DB_CONTEST: i32 = 5;
pub const EE_WON_DB_EVENT: i32 = 6;

/// What the op does after the read.
/// Return `status`.
pub const EE_WON_DB_RETURN: i32 = 0;
/// Return `InvalidReplyHeader()`.
pub const EE_WON_DB_INVALID_HEADER: i32 = 1;
/// DBProxy: keep the rest (`rest_*`) and receive the next reply.
pub const EE_WON_DB_RECEIVE: i32 = 2;
/// DBProxy: keep the rest, then take the collected data as the reply and return `status`.
pub const EE_WON_DB_DONE: i32 = 3;
/// CreateAccount: set the public key at `key_*` (its failure: an invalid key, the next server; else the
/// session key goes next)
pub const EE_WON_DB_PUBLIC_KEY: i32 = 4;

/// A DBProxy-style reply read for the C++.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWonDbReply {
    /// how many of the header's fields were read whole (header, service, message, sub-message, status, error
    /// string, flags): the members the original assigned on the way are the message (3), the sub-message (4)
    /// and the error string (6)
    pub read: u8,
    /// the error string's length was read but not its bytes: the original erased the string
    pub error_erased: bool,
    pub message: i16,
    pub sub: i16,
    pub status: i32,
    pub error_at: u32,
    pub error_len: u32,
    /// the original threw `ReadBufferException` after the assignments
    pub cut: bool,
    pub verdict: i32,
    /// what the op returns with `EE_WON_DB_RETURN` / `EE_WON_DB_DONE`
    pub result: i32,
    /// the rest of the reply (DBProxy's data, the contest's reply data)
    pub rest_at: u32,
    pub rest_len: u32,
    /// the server's public key (create account)
    pub key_at: u32,
    pub key_len: u32,
    /// the email (get account): 0 not read, 1 erased, 2 read whole at `email_*`
    pub email: u8,
    pub email_at: u32,
    pub email_len: u32,
    /// where the profile starts (get account)
    pub profile_at: u32,
}

const INVALID: i32 = EE_WON_DB_INVALID_HEADER;

fn header(r: &mut Reader, out: &mut EeWonDbReply) -> Result<(u8, u16), PastEnd> {
    let h = r.read_byte()?;
    out.read = 1;
    let s = r.read_short()? as u16;
    out.read = 2;
    out.message = r.read_short()?;
    out.read = 3;
    out.sub = r.read_short()?;
    out.read = 4;
    out.status = i32::from(r.read_short()?);
    out.read = 5;
    let n = r.read_length(2)? as usize;
    if n > r.available() {
        out.error_erased = true;
        return Err(PastEnd);
    }
    out.error_at = r.pos() as u32;
    out.error_len = n as u32;
    r.read_bytes(n)?;
    out.read = 6;
    Ok((h, s))
}

fn read(kind: i32, r: &mut Reader, out: &mut EeWonDbReply) -> Result<(), PastEnd> {
    match kind {
        EE_WON_DB_CONTEST => {
            let h = r.read_byte()?;
            let s = r.read_byte()?;
            let m = r.read_byte()?;
            if h != 3 || s != 4 || m != 2 {
                out.verdict = INVALID;
                return Ok(());
            }
            out.result = i32::from(r.read_short()?);
            out.rest_at = r.pos() as u32;
            out.rest_len = r.available() as u32;
            return Ok(());
        }
        EE_WON_DB_EVENT => {
            let h = r.read_byte()?;
            let s = r.read_short()? as u16;
            let m = r.read_short()? as u16;
            if h != 5 || s != 4 || m != 2 {
                out.verdict = INVALID;
                return Ok(());
            }
            out.result = i32::from(r.read_short()?);
            return Ok(());
        }
        _ => {}
    }
    let (h, s) = header(r, out)?;
    let flags = r.read_byte()?;
    out.read = 7;
    if h != 5 || s != 14 {
        out.verdict = INVALID;
        return Ok(());
    }
    let (message, sub) = (out.message as u16, out.sub as u16);
    if kind == EE_WON_DB_PROXY {
        out.rest_at = r.pos() as u32;
        out.rest_len = r.available() as u32;
        if flags & 0x80 != 0 {
            out.verdict = EE_WON_DB_DONE;
            out.result = out.status;
        } else {
            out.verdict = EE_WON_DB_RECEIVE;
        }
        return Ok(());
    }
    if out.status != 0 {
        out.result = out.status;
        return Ok(());
    }
    match kind {
        EE_WON_DB_CREATE_KEY => {
            if message != 3 {
                out.verdict = INVALID;
                return Ok(());
            }
            let n = r.read_short()? as u16;
            out.key_at = r.pos() as u32;
            r.read_bytes(usize::from(n))?;
            out.key_len = u32::from(n);
            out.verdict = EE_WON_DB_PUBLIC_KEY;
        }
        EE_WON_DB_CREATE if message != 1002 || sub != 3 => out.verdict = INVALID,
        EE_WON_DB_GET_ACCOUNT => {
            if message != 1002 || sub != 7 {
                out.verdict = INVALID;
                return Ok(());
            }
            let n = r.read_length(1)? as usize;
            if n > r.available() {
                out.email = 1;
                return Err(PastEnd);
            }
            out.email_at = r.pos() as u32;
            out.email_len = n as u32;
            r.read_bytes(n)?;
            out.email = 2;
            out.profile_at = r.pos() as u32;
        }
        EE_WON_DB_UPDATE_ACCOUNT if message != 1002 || sub != 5 => out.verdict = INVALID,
        _ => {}
    }
    Ok(())
}

/// A reply of kind `kind`, read as the op's `CheckResponse` did.
pub fn reply(kind: i32, data: &[u8]) -> EeWonDbReply {
    let mut out = EeWonDbReply::default();
    let mut r = Reader::new(data);
    if read(kind, &mut r, &mut out).is_err() {
        out.cut = true;
    }
    out
}

/// `GetUserIdFromNameOp::CheckResponse`'s id: the reply data's first long (`None`: cut short, a throw).
pub fn user_id(data: &[u8]) -> Option<i32> {
    Reader::new(data).read_long().ok()
}

/// The email requests' and the user id lookup's check of the sub-message reply: true when it answers `sub`.
pub fn answers(reply_sub: i16, sub: u16) -> bool {
    i32::from(reply_sub) == i32::from(sub) + 1
}

// ---- the C ABI

/// `new ProfileData`.
#[no_mangle]
pub extern "C" fn ee_won_profile_new() -> *mut EeWonProfile {
    Box::into_raw(Box::default())
}

/// # Safety
/// `p` came from `ee_won_profile_new` and is not used again.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_free(p: *mut EeWonProfile) {
    if !p.is_null() {
        drop(unsafe { Box::from_raw(p) });
    }
}

/// `AddNewsletterSub`.
///
/// # Safety
/// `p` is a live profile.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_set_newsletter(p: *mut EeWonProfile, id: u16, on: bool) {
    unsafe { (*p).newsletters.insert(id, on) };
}

/// `GetNewsletterSub`.
///
/// # Safety
/// `p` is a live profile.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_newsletter(p: *const EeWonProfile, id: u16) -> bool {
    unsafe { (*p).newsletter(id) }
}

/// `ClearNewsletterSub` (`all`: `ClearNewsletterSubs`).
///
/// # Safety
/// `p` is a live profile.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_clear_newsletter(p: *mut EeWonProfile, id: u16, all: bool) {
    let p = unsafe { &mut *p };
    if all {
        p.newsletters.clear();
    } else {
        p.newsletters.remove(&id);
    }
}

/// `SetCustomAction` (`data` NULL: no data) and `ClearCustomAction` (id 0, NULL).
///
/// # Safety
/// `p` is a live profile; `data` holds `length` bytes when not NULL.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_set_custom_action(
    p: *mut EeWonProfile,
    id: u16,
    data: *const u8,
    length: usize,
    present: bool,
) {
    let p = unsafe { &mut *p };
    p.custom_action_id = id;
    p.custom_action_data = if present { Some(unsafe { crate::ffi::bytes(data, length) }.to_vec()) } else { None };
}

/// `SetBirthDate`.
///
/// # Safety
/// `p` is a live profile.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_set_birth_date(p: *mut EeWonProfile, month: u8, day: u8, year: u16) {
    let p = unsafe { &mut *p };
    p.birth_month = month;
    p.birth_day = day;
    p.birth_year = year;
}

/// `GetBirthDate`.
///
/// # Safety
/// `p` is a live profile.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_birth_date(p: *const EeWonProfile) -> i32 {
    // the original computed it in a signed int: a year from 32768 makes it negative
    unsafe { (*p).birth_date() as i32 }
}

/// `PackRequestData`'s bytes (to append) into `sink`.
///
/// # Safety
/// `p` is a live profile; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_pack(
    p: *const EeWonProfile,
    is_set: bool,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let mut w = Writer::new(0, true);
    unsafe { (*p).pack(&mut w, is_set) };
    unsafe { sink(context, w.data().as_ptr(), w.data().len()) }
}

/// `UnpackGetData` from `pos`; returns the reader's position after it.
///
/// # Safety
/// `p` is a live profile; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_profile_unpack(
    p: *mut EeWonProfile,
    data: *const u8,
    length: usize,
    pos: usize,
) -> usize {
    let d = unsafe { crate::ffi::bytes(data, length) };
    unsafe { (*p).unpack(d, pos) }
}

/// The DBProxy family's requests (`EeWonDbRequest::kind`).
pub const EE_WON_DB_REQ_PROXY: i32 = 0;
pub const EE_WON_DB_REQ_CREATE_KEY: i32 = 1;
pub const EE_WON_DB_REQ_CREATE_SESSION: i32 = 2;
/// the account data to encrypt
pub const EE_WON_DB_REQ_CREATE_DATA: i32 = 3;
pub const EE_WON_DB_REQ_CREATE_ACCOUNT: i32 = 4;
pub const EE_WON_DB_REQ_GET_ACCOUNT: i32 = 5;
pub const EE_WON_DB_REQ_UPDATE_ACCOUNT: i32 = 6;
pub const EE_WON_DB_REQ_USER_ID: i32 = 7;
pub const EE_WON_DB_REQ_PASSWORD_EMAIL: i32 = 8;
pub const EE_WON_DB_REQ_INFO_EMAIL: i32 = 9;
pub const EE_WON_DB_REQ_CONTEST: i32 = 10;

/// A request's fields from the C++ (texts as UTF-16 LE bytes).
#[repr(C)]
pub struct EeWonDbRequest {
    pub kind: i32,
    pub field_size: u8,
    pub message: i16,
    pub sub: i16,
    pub user_id: u32,
    /// the request data, the encrypted key or account data
    pub data: EeWonDirBytes,
    pub name: EeWonDirBytes,
    pub password: EeWonDirBytes,
    pub email: EeWonDirBytes,
    pub profile: *const EeWonProfile,
}

unsafe fn opt<'a>(b: &EeWonDirBytes) -> Option<&'a [u8]> {
    if b.present {
        Some(unsafe { crate::ffi::bytes(b.data, b.len) })
    } else {
        None
    }
}

unsafe fn units(b: &EeWonDirBytes) -> Vec<u16> {
    crate::text::utf16_from_le_bytes(unsafe { crate::ffi::bytes(b.data, b.len) })
}

/// A request's message (or data) into `sink`.
///
/// # Safety
/// `r` and what it points to are valid (`profile` a live profile where the kind reads one); `sink` accepts
/// `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_db_request(r: *const EeWonDbRequest, sink: EeWonByteSink, context: *mut c_void) {
    let r = unsafe { &*r };
    let empty = EeWonProfile::default();
    let profile = if r.profile.is_null() { &empty } else { unsafe { &*r.profile } };
    let email = unsafe { opt(&r.email) }.unwrap_or(&[]);
    let m = unsafe {
        match r.kind {
            EE_WON_DB_REQ_PROXY => proxy_request(r.field_size, r.message, r.sub, opt(&r.data)),
            EE_WON_DB_REQ_CREATE_KEY => create_key_request(r.field_size),
            EE_WON_DB_REQ_CREATE_SESSION => create_session_request(r.field_size, opt(&r.data)),
            EE_WON_DB_REQ_CREATE_DATA => create_account_data(&units(&r.name), &units(&r.password), email, profile),
            EE_WON_DB_REQ_CREATE_ACCOUNT => create_account_request(r.field_size, opt(&r.data)),
            EE_WON_DB_REQ_GET_ACCOUNT => get_account_request(r.field_size, profile),
            EE_WON_DB_REQ_UPDATE_ACCOUNT => update_account_request(r.field_size, &units(&r.password), email, profile),
            EE_WON_DB_REQ_USER_ID => user_id_data(&units(&r.name)),
            EE_WON_DB_REQ_PASSWORD_EMAIL => email_data(Some(&units(&r.name)), email, profile),
            EE_WON_DB_REQ_INFO_EMAIL => email_data(None, email, profile),
            EE_WON_DB_REQ_CONTEST => contest_request(r.field_size, r.user_id, r.message, opt(&r.data)),
            _ => Vec::new(),
        }
    };
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// An event detail from the C++.
#[repr(C)]
pub struct EeWonDbDetail {
    pub detail_type: u16,
    pub data_type: i32,
    pub number: u32,
    pub text: EeWonDirBytes,
}

/// An event attachment from the C++.
#[repr(C)]
pub struct EeWonDbAttachment {
    pub description: EeWonDirBytes,
    pub content_type: u8,
    pub data: EeWonDirBytes,
}

/// An event report from the C++ (`present` false on a part: not related).
#[repr(C)]
pub struct EeWonDbEvent {
    pub field_size: u8,
    pub activity_type: u16,
    pub has_server: bool,
    pub server_type: u16,
    pub server_name: EeWonDirBytes,
    pub server_addr: EeWonDirBytes,
    pub has_client: bool,
    pub client_name: EeWonDirBytes,
    pub client_addr: EeWonDirBytes,
    pub has_user: bool,
    pub user_id: u32,
    pub user_name: EeWonDirBytes,
    pub details: *const EeWonDbDetail,
    pub detail_count: usize,
    pub attachments: *const EeWonDbAttachment,
    pub attachment_count: usize,
}

unsafe fn list<'a, T>(p: *const T, n: usize) -> &'a [T] {
    if p.is_null() || n == 0 {
        &[]
    } else {
        // SAFETY: the C++ hands `n` entries
        unsafe { std::slice::from_raw_parts(p, n) }
    }
}

/// `EventReportOp::GetNextRequest` into `sink`.
///
/// # Safety
/// `e` and what it points to are valid; `sink` accepts `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_db_event_request(e: *const EeWonDbEvent, sink: EeWonByteSink, context: *mut c_void) {
    let e = unsafe { &*e };
    let b = |x: &EeWonDirBytes| unsafe { crate::ffi::bytes(x.data, x.len) };
    let ev = unsafe {
        Event {
            activity_type: e.activity_type,
            server: e.has_server.then(|| (e.server_type, units(&e.server_name), b(&e.server_addr))),
            client: e.has_client.then(|| (units(&e.client_name), b(&e.client_addr))),
            user: e.has_user.then(|| (e.user_id, units(&e.user_name))),
            details: list(e.details, e.detail_count)
                .iter()
                .map(|d| Detail {
                    detail_type: d.detail_type,
                    data_type: d.data_type,
                    number: d.number,
                    text: units(&d.text),
                })
                .collect(),
            attachments: list(e.attachments, e.attachment_count)
                .iter()
                .map(|a| Attachment {
                    description: units(&a.description),
                    content_type: a.content_type,
                    data: opt(&a.data),
                })
                .collect(),
        }
    };
    let m = event_request(e.field_size, &ev);
    unsafe { sink(context, m.as_ptr(), m.len()) }
}

/// A reply's read into `out`.
///
/// # Safety
/// `data` holds `length` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_db_reply(kind: i32, data: *const u8, length: usize, out: *mut EeWonDbReply) {
    unsafe { *out = reply(kind, crate::ffi::bytes(data, length)) };
}

/// `GetUserIdFromNameOp`'s id from the reply data; false when it is cut short.
///
/// # Safety
/// `data` holds `length` bytes; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_db_user_id(data: *const u8, length: usize, out: *mut i32) -> bool {
    match user_id(unsafe { crate::ffi::bytes(data, length) }) {
        Some(v) => {
            unsafe { *out = v };
            true
        }
        None => false,
    }
}

/// Whether a DBProxy reply's sub-message answers the request's `sub` (the email requests).
#[no_mangle]
pub extern "C" fn ee_won_db_answers(reply_sub: i16, sub: u16) -> bool {
    answers(reply_sub, sub)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_profile_packs_and_unpacks() {
        let mut p = EeWonProfile { birth_month: 3, birth_day: 4, birth_year: 1990, ..Default::default() };
        p.newsletters.insert(1, true);
        p.custom_action_id = 9;
        p.custom_action_data = Some(vec![7, 8]);
        let mut w = Writer::new(0, true);
        p.pack(&mut w, true);
        assert_eq!(w.data(), &[3, 0, 1, 0, 3, 0, 1, 0, 1, 2, 0, 4, 0, 9, 0, 7, 8, 3, 0, 4, 0, 3, 4, 0xc6, 0x07]);
        let mut w = Writer::new(0, true);
        p.pack(&mut w, false);
        assert_eq!(&w.data()[..9], &[3, 0, 1, 0, 2, 0, 1, 0, 2]);
        assert_eq!(p.birth_date(), 3 | 4 << 8 | 1990 << 16);
        // a get reply: a newsletter, a custom action, the birth date
        let reply = [3, 0, 1, 0, 3, 0, 5, 0, 1, 2, 0, 4, 0, 9, 0, 0xaa, 0xbb, 3, 0, 4, 0, 1, 2, 3, 0];
        let mut q = EeWonProfile::default();
        assert_eq!(q.unpack(&reply, 0), reply.len());
        assert!(q.newsletter(5));
        assert_eq!(q.custom_action_data, Some(vec![0xaa, 0xbb]));
        assert_eq!(q.custom_action_id, 0);
        assert_eq!((q.birth_month, q.birth_day, q.birth_year), (1, 2, 3));
        // cut in a newsletter's value: the entry exists, false
        let mut q = EeWonProfile::default();
        q.unpack(&reply[..8], 0);
        assert_eq!(q.newsletters.get(&5), Some(&false));
        // a custom action shorter than its id: past the end
        let mut q = EeWonProfile::default();
        q.unpack(&[1, 0, 2, 0, 1, 0, 9, 0], 0);
        assert_eq!(q.custom_action_data, None);
    }

    #[test]
    fn requests() {
        assert_eq!(proxy_request(0, 1002, 16, Some(&[9])), vec![5, 14, 0, 0xea, 3, 16, 0, 9]);
        assert_eq!(proxy_request(2, 1, 2, None), vec![9, 0, 5, 14, 0, 1, 0, 2, 0]);
        let c = contest_request(0, 7, 3, None);
        assert_eq!(c.len(), 3 + 4 + 2 + 16);
        assert_eq!(user_id_data(&[0x41]), vec![1, 0, 0x41, 0]);
        let e = Event { activity_type: 2, user: Some((5, vec![])), ..Default::default() };
        assert_eq!(event_request(0, &e), vec![5, 4, 0, 1, 0, 8, 0, 0, 2, 0, 1, 0, 5, 0, 0, 0, 0, 0]);
    }

    #[test]
    fn replies() {
        let ok = [5, 14, 0, 0xea, 3, 7, 0, 0, 0, 0, 0, 0x80, 2, b'a', b'b', 9];
        let r = reply(EE_WON_DB_GET_ACCOUNT, &ok);
        assert_eq!((r.read, r.verdict, r.email, r.profile_at), (7, EE_WON_DB_RETURN, 2, 15));
        let r = reply(EE_WON_DB_PROXY, &ok);
        assert_eq!((r.verdict, r.rest_at, r.rest_len), (EE_WON_DB_DONE, 12, 4));
        let r = reply(EE_WON_DB_UPDATE_ACCOUNT, &ok);
        assert_eq!(r.verdict, EE_WON_DB_INVALID_HEADER);
        // the error string cut: erased, then the throw
        let r = reply(EE_WON_DB_PROXY, &[5, 14, 0, 1, 0, 2, 0, 0, 0, 3, 0, b'x']);
        assert!(r.cut && r.error_erased && r.read == 5);
        let r = reply(EE_WON_DB_CONTEST, &[3, 4, 2, 1, 0, 9]);
        assert_eq!((r.result, r.rest_at, r.rest_len), (1, 5, 1));
        assert_eq!(reply(EE_WON_DB_EVENT, &[5, 4, 0, 3, 0]).verdict, EE_WON_DB_INVALID_HEADER);
        let key = [5, 14, 0, 3, 0, 1, 0, 0, 0, 0, 0, 0, 2, 0, 1, 2];
        let r = reply(EE_WON_DB_CREATE_KEY, &key);
        assert_eq!((r.verdict, r.key_at, r.key_len), (EE_WON_DB_PUBLIC_KEY, 14, 2));
        assert!(reply(EE_WON_DB_CREATE_KEY, &key[..15]).cut);
        assert_eq!(user_id(&[1, 0, 0, 0]), Some(1));
        assert!(answers(17, 16) && !answers(16, 16));
    }
}
