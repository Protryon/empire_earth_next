//! The C ABI behind the C++ forwarders of `src/Empire Earth/WONAPI` (WONBuffer.h's buffers and text helpers, CRC16,
//! MD5, MD5Digest, Blowfish, ElGamal, the auth blocks, CDKey).
//!
//! Variable-length results go to a sink the caller passes (the forwarders copy them into a `ByteBuffer` or a
//! `std::wstring`); fixed-size state (`Md5`, `Blowfish`) is embedded by value in the C++ objects; keys and
//! auth blocks are boxed Rust objects behind opaque pointers, freed with their `_free` function.

use std::ffi::{c_char, c_void};
use std::slice;

use crate::auth::{AuthBlock, Body, Kind};
use crate::blowfish::Blowfish;
use crate::elgamal::ElGamal;
use crate::md5::Md5;
use crate::{cdkey, crc16, text, wire};

/// Receives bytes; valid only during the call.
pub type EeWonByteSink = unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize);
/// Receives UTF-16 units; valid only during the call.
pub type EeWonUnitSink = unsafe extern "C" fn(context: *mut c_void, units: *const u16, count: usize);

/// An auth block's kind for `ee_won_auth_unpack`: the header only.
pub const EE_WON_AUTH_BASE: u32 = 0;
/// `AuthCertificate`.
pub const EE_WON_AUTH_CERTIFICATE: u32 = 1;
/// `Auth2Certificate`.
pub const EE_WON_AUTH_CERTIFICATE2: u32 = 2;
/// `AuthPubKeyBlock`.
pub const EE_WON_AUTH_PUB_KEY_BLOCK: u32 = 3;

pub(crate) unsafe fn bytes<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if length == 0 || data.is_null() {
        &[]
    } else {
        // SAFETY: the caller passes a buffer of `length` bytes (each function's contract)
        unsafe { slice::from_raw_parts(data, length) }
    }
}

unsafe fn units<'a>(data: *const u16, count: usize) -> &'a [u16] {
    if count == 0 || data.is_null() {
        &[]
    } else {
        // SAFETY: as `bytes`
        unsafe { slice::from_raw_parts(data, count) }
    }
}

pub(crate) unsafe fn give(sink: EeWonByteSink, context: *mut c_void, data: &[u8]) {
    // SAFETY: the sink's contract: `data` is valid during the call
    unsafe { sink(context, data.as_ptr(), data.len()) }
}

unsafe fn give_units(sink: EeWonUnitSink, context: *mut c_void, data: &[u16]) {
    // SAFETY: as `give`
    unsafe { sink(context, data.as_ptr(), data.len()) }
}

/// Copies `src` into `out` (up to `capacity`) and returns `src`'s length.
unsafe fn copy_out<T: Copy>(src: &[T], out: *mut T, capacity: usize) -> usize {
    if !out.is_null() {
        let n = src.len().min(capacity);
        // SAFETY: `out` has room for `capacity` items (each function's contract)
        unsafe { std::ptr::copy_nonoverlapping(src.as_ptr(), out, n) };
    }
    src.len()
}

/// The UTF-16 units of `wchar_t` text (`wchar_size` 2: the units as they are; 4: code points).
unsafe fn wide_units(wide: *const c_void, wchar_size: u32, count: usize) -> Vec<u16> {
    if count == 0 || wide.is_null() {
        return Vec::new();
    }
    if wchar_size == 2 {
        // SAFETY: `count` units of 2 bytes
        unsafe { slice::from_raw_parts(wide as *const u16, count) }.to_vec()
    } else {
        // SAFETY: `count` code points of 4 bytes
        text::utf16_from_utf32(unsafe { slice::from_raw_parts(wide as *const u32, count) })
    }
}

// ---- text (WONBuffer.h's WideUtf16Length, WideAppendUtf16LE, WideAppendUtf16Unit)

/// The UTF-16 little-endian bytes of `count` `wchar_t`s of `wchar_size` bytes, written to `out` (up to `capacity`
/// bytes); returns the byte count. `out` may be null to measure.
///
/// # Safety
/// `wide` holds `count` characters of `wchar_size` (2 or 4) bytes; `out` has `capacity` bytes or is null.
#[no_mangle]
pub unsafe extern "C" fn ee_won_utf16le_from_wide(
    wide: *const c_void,
    wchar_size: u32,
    count: usize,
    out: *mut u8,
    capacity: usize,
) -> usize {
    let le = text::utf16_to_le_bytes(&unsafe { wide_units(wide, wchar_size, count) });
    unsafe { copy_out(&le, out, capacity) }
}

/// The code point a UTF-16 unit makes with the last character of a UTF-32 text: the joined code point when `last`
/// is a high surrogate and `unit` a low one, else 0 (append the unit as it is).
#[no_mangle]
pub extern "C" fn ee_won_utf16_join(last: u32, unit: u16) -> u32 {
    let mut t = vec![last];
    text::push_utf16_unit(&mut t, unit);
    if t.len() == 1 {
        t[0]
    } else {
        0
    }
}

/// `wchar_t` text of UTF-16 units (`wchar_size` 2: unchanged; 4: surrogate pairs joined) written to `out` (up to
/// `capacity` characters, which `count` always suffices for); returns the character count.
///
/// # Safety
/// `data` holds `count` units; `out` has room for `capacity` characters of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wide_from_utf16(
    data: *const u16,
    count: usize,
    wchar_size: u32,
    out: *mut c_void,
    capacity: usize,
) -> usize {
    let src = unsafe { units(data, count) };
    if wchar_size == 2 {
        unsafe { copy_out(src, out as *mut u16, capacity) }
    } else {
        unsafe { copy_out(&text::utf32_from_utf16(src), out as *mut u32, capacity) }
    }
}

/// The UTF-16 units of little-endian bytes (`count` units read from `data`, which holds `2 * count` bytes) as
/// `wchar_t` text; see `ee_won_wide_from_utf16`.
///
/// # Safety
/// `data` holds `2 * count` bytes; `out` as for `ee_won_wide_from_utf16`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wide_from_utf16le(
    data: *const u8,
    count: usize,
    wchar_size: u32,
    out: *mut c_void,
    capacity: usize,
) -> usize {
    let u = text::utf16_from_le_bytes(unsafe { bytes(data, count * 2) });
    unsafe { ee_won_wide_from_utf16(u.as_ptr(), u.len(), wchar_size, out, capacity) }
}

/// The UTF-16 units of `wchar_t` text, to a sink.
///
/// # Safety
/// As `ee_won_utf16le_from_wide`; the sink is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_utf16_from_wide(
    wide: *const c_void,
    wchar_size: u32,
    count: usize,
    sink: EeWonUnitSink,
    context: *mut c_void,
) {
    let u = unsafe { wide_units(wide, wchar_size, count) };
    unsafe { give_units(sink, context, &u) }
}

/// Receives `wchar_t` text (`wchar_size` bytes a character); valid only during the call.
pub type EeWonWideSink = unsafe extern "C" fn(context: *mut c_void, text: *const c_void, count: usize);

/// The game's text (UTF-8 bytes, decision D6) as `wchar_t` text (`wchar_size` 2: UTF-16 units; 4: code points),
/// given to the sink once (`StringToWString`). A stray byte becomes U+FFFD; embedded zeros are characters.
///
/// # Safety
/// `text` holds `length` bytes; the sink is called with `count` characters of `wchar_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_wide_from_utf8(
    text: *const u8,
    length: usize,
    wchar_size: u32,
    sink: EeWonWideSink,
    context: *mut c_void,
) {
    let s = unsafe { bytes(text, length) };
    if wchar_size == 2 {
        let u = text::utf16_from_game(s);
        unsafe { sink(context, u.as_ptr().cast(), u.len()) }
    } else {
        let u = text::utf32_from_game(s);
        unsafe { sink(context, u.as_ptr().cast(), u.len()) }
    }
}

/// `wchar_t` text as the game's text (UTF-8), given to the sink once (`WStringToString`). A lone surrogate becomes
/// U+FFFD; embedded zeros are characters.
///
/// # Safety
/// `wide` holds `count` characters of `wchar_size` (2 or 4) bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_utf8_from_wide(
    wide: *const c_void,
    wchar_size: u32,
    count: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) {
    let out = if wchar_size == 2 {
        text::game_from_utf16(&unsafe { wide_units(wide, wchar_size, count) })
    } else if count == 0 || wide.is_null() {
        Vec::new()
    } else {
        // SAFETY: `count` code points of 4 bytes
        text::game_from_utf32(unsafe { slice::from_raw_parts(wide as *const u32, count) })
    };
    unsafe { give(sink, context, &out) }
}

// ---- WriteBuffer / ReadBuffer

/// `CheckWrite`: the capacity for `length + add` bytes, or 0 when the current one suffices.
#[no_mangle]
pub extern "C" fn ee_won_write_grow(capacity: u64, length: u64, add: u64) -> u64 {
    wire::grown_capacity(capacity, length, add).unwrap_or(0)
}

/// `ReserveChunk`: the capacity for `total` bytes capped at `max`, or 0 when the current one suffices.
#[no_mangle]
pub extern "C" fn ee_won_write_chunk(capacity: u64, total: u64, max: u64) -> u64 {
    wire::chunk_capacity(capacity, total, max).unwrap_or(0)
}

/// `AppendLength`: the field's bytes into `out` (4 bytes of room); returns their count (0 for another size).
///
/// # Safety
/// `out` has 4 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_encode_length(length: u32, size: u8, out: *mut u8) -> usize {
    unsafe { copy_out(&wire::encode_length(length, size), out, 4) }
}

/// `ToByteBuffer`'s prefix for `length` data bytes into `out` (6 bytes of room); returns its length.
///
/// # Safety
/// `out` has 6 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_length_prefix(
    field_size: u8,
    includes_field: bool,
    length: u32,
    out: *mut u8,
) -> usize {
    unsafe { copy_out(&wire::length_prefix(field_size, includes_field, length), out, 6) }
}

/// `ReadBytes`'s bound: advances `*pos` by `amount` and returns true when that many bytes remain before `length`.
///
/// # Safety
/// `pos` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_read_take(length: u64, pos: *mut i64, amount: u64) -> bool {
    let p = unsafe { *pos };
    if length.wrapping_sub(p as u64) < amount {
        return false;
    }
    unsafe { *pos = p.wrapping_add(amount as i64) };
    true
}

/// `ReadLength`: reads a 1, 2 or 4-byte length at `*pos` (sign-extended; 0 for another size); false past the end.
///
/// # Safety
/// `data` holds `length` bytes; `pos` and `out` are valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_read_length(
    data: *const u8,
    length: u64,
    pos: *mut i64,
    size: u8,
    out: *mut u32,
) -> bool {
    let all = unsafe { bytes(data, length as usize) };
    let p = unsafe { *pos } as usize;
    if p > all.len() {
        return false;
    }
    let mut r = wire::Reader::new(&all[p..]);
    match r.read_length(size) {
        Ok(v) => {
            unsafe {
                *out = v;
                *pos += r.pos() as i64;
            }
            true
        }
        Err(_) => false,
    }
}

// ---- CRC16, MD5

/// `CRC16::Put`: the register after the bytes.
///
/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_crc16_put(register: u16, data: *const u8, length: usize) -> u16 {
    crc16::put(register, unsafe { bytes(data, length) })
}

/// Resets an MD5 state (also its initialisation).
///
/// # Safety
/// `state` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_md5_reset(state: *mut Md5) {
    unsafe { (*state).reset() }
}

/// Adds bytes.
///
/// # Safety
/// `state` is valid; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_md5_update(state: *mut Md5, data: *const u8, length: usize) {
    let d = unsafe { bytes(data, length) };
    unsafe { (*state).update(d) }
}

/// Writes the 16-byte digest and resets.
///
/// # Safety
/// `state` is valid; `out` has 16 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_md5_finish(state: *mut Md5, out: *mut u8) {
    let d = unsafe { (*state).finish() };
    unsafe { copy_out(&d, out, 16) };
}

// ---- Blowfish

/// No key.
///
/// # Safety
/// `bf` is valid for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_init(bf: *mut Blowfish) {
    unsafe { bf.write(Blowfish::new()) }
}

/// `SetKey`.
///
/// # Safety
/// `bf` is valid; `key` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_set_key(bf: *mut Blowfish, key: *const u8, length: usize) -> bool {
    let k = unsafe { bytes(key, length) };
    unsafe { (*bf).set_key(k) }
}

/// `Create`: a random key.
///
/// # Safety
/// `bf` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_create(bf: *mut Blowfish, length: i32) -> bool {
    length >= 0 && unsafe { (*bf).create(length as usize) }
}

/// The raw key (`GetKey`); its length is `ee_won_blowfish_key_length`.
///
/// # Safety
/// `bf` is valid; the pointer lives as long as `*bf` is unchanged.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_key(bf: *const Blowfish) -> *const u8 {
    unsafe { (*bf).key().as_ptr() }
}

/// `GetKeyLen`.
///
/// # Safety
/// `bf` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_key_length(bf: *const Blowfish) -> i32 {
    unsafe { (*bf).key().len() as i32 }
}

/// `Encrypt`: the cipher text to the sink; false (no call) without a key.
///
/// # Safety
/// `bf` is valid; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_encrypt(
    bf: *const Blowfish,
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { (*bf).encrypt(bytes(data, length)) } {
        Some(out) => {
            unsafe { give(sink, context, &out) };
            true
        }
        None => false,
    }
}

/// `Decrypt`: the plain text to the sink; false (no call) when refused.
///
/// # Safety
/// As `ee_won_blowfish_encrypt`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_blowfish_decrypt(
    bf: *const Blowfish,
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    match unsafe { (*bf).decrypt(bytes(data, length)) } {
        Some(out) => {
            unsafe { give(sink, context, &out) };
            true
        }
        None => false,
    }
}

// ---- ElGamal

/// A key with no key material, its generator seeded from the system.
#[no_mangle]
pub extern "C" fn ee_won_elgamal_new() -> *mut ElGamal {
    Box::into_raw(Box::new(ElGamal::new()))
}

/// A copy (generator state included).
///
/// # Safety
/// `key` is a live key.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_clone(key: *const ElGamal) -> *mut ElGamal {
    Box::into_raw(Box::new(unsafe { (*key).clone() }))
}

/// Frees a key (null: nothing).
///
/// # Safety
/// `key` came from this module and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_free(key: *mut ElGamal) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

/// `SetPrivateKey` (`is_private` true) or `SetPublicKey`.
///
/// # Safety
/// `key` is live; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_set_key(
    key: *mut ElGamal,
    data: *const u8,
    length: usize,
    is_private: bool,
) -> bool {
    let d = unsafe { bytes(data, length) };
    let k = unsafe { &mut *key };
    if is_private {
        k.set_private_key(d)
    } else {
        k.set_public_key(d)
    }
}

/// `Invalidate`.
///
/// # Safety
/// `key` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_invalidate(key: *mut ElGamal) {
    unsafe { (*key).invalidate() }
}

/// `IsPrivate`.
///
/// # Safety
/// `key` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_is_private(key: *const ElGamal) -> bool {
    unsafe { (*key).is_private() }
}

/// `IsPublic`.
///
/// # Safety
/// `key` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_is_public(key: *const ElGamal) -> bool {
    unsafe { (*key).is_public() }
}

/// What an ElGamal call computes into a sink.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonElGamalOp {
    Encrypt = 0,
    Decrypt = 1,
    Sign = 2,
    RawPrivateKey = 3,
    RawPublicKey = 4,
}

/// Encrypt, decrypt, sign (the input is the message) or write a raw key (no input): the result to the sink; false
/// (no call) when the key cannot (`Encrypt`/`Sign` advance the key's generator, as the C++'s `mutable Random`).
///
/// # Safety
/// `key` is live; `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_op(
    key: *mut ElGamal,
    op: EeWonElGamalOp,
    data: *const u8,
    length: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    let d = unsafe { bytes(data, length) };
    let k = unsafe { &mut *key };
    let out = match op {
        EeWonElGamalOp::Encrypt => k.encrypt(d),
        EeWonElGamalOp::Decrypt => k.decrypt(d),
        EeWonElGamalOp::Sign => k.sign(d),
        EeWonElGamalOp::RawPrivateKey => k.raw_private_key(),
        EeWonElGamalOp::RawPublicKey => k.raw_public_key(),
    };
    match out {
        Some(o) => {
            unsafe { give(sink, context, &o) };
            true
        }
        None => false,
    }
}

/// `Verify`.
///
/// # Safety
/// `key` is live; the buffers hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_elgamal_verify(
    key: *const ElGamal,
    message: *const u8,
    message_length: usize,
    signature: *const u8,
    signature_length: usize,
) -> bool {
    unsafe { (*key).verify(bytes(message, message_length), bytes(signature, signature_length)) }
}

// ---- auth blocks

/// An auth block's header and validity.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonAuthHeader {
    pub valid: bool,
    pub family: u16,
    pub data_len: u16,
    pub issue_time: i32,
    pub expire_time: i32,
    /// a certificate's user id
    pub user_id: u32,
    /// a key block's id
    pub key_block_id: u16,
}

/// Unpacks a block of `kind` (`EE_WON_AUTH_*`).
///
/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_unpack(kind: u32, data: *const u8, length: usize) -> *mut AuthBlock {
    let kind = match kind {
        EE_WON_AUTH_CERTIFICATE => Kind::Certificate,
        EE_WON_AUTH_CERTIFICATE2 => Kind::Certificate2,
        EE_WON_AUTH_PUB_KEY_BLOCK => Kind::PubKeyBlock,
        _ => Kind::Base,
    };
    Box::into_raw(Box::new(AuthBlock::unpack(kind, unsafe { bytes(data, length) })))
}

/// Frees a block (null: nothing).
///
/// # Safety
/// `block` came from `ee_won_auth_unpack` and is not used afterwards.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_free(block: *mut AuthBlock) {
    if !block.is_null() {
        drop(unsafe { Box::from_raw(block) });
    }
}

/// The header fields.
///
/// # Safety
/// `block` is live; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_header(block: *const AuthBlock, out: *mut EeWonAuthHeader) {
    let b = unsafe { &*block };
    let key_block_id = match &b.body {
        Body::PubKeyBlock(k) => k.id,
        _ => 0,
    };
    unsafe {
        *out = EeWonAuthHeader {
            valid: b.valid,
            family: b.family,
            data_len: b.data_len,
            issue_time: b.issue_time,
            expire_time: b.expire_time,
            user_id: b.user_id(),
            key_block_id,
        }
    }
}

/// `AuthBase::Verify(key)`.
///
/// # Safety
/// `block` and `key` are live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_verify(block: *const AuthBlock, key: *const ElGamal) -> bool {
    unsafe { (*block).verify(&*key) }
}

/// `IsExpired(extra)` at `now` (seconds).
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_is_expired(block: *const AuthBlock, now: i64, extra: i64) -> bool {
    unsafe { (*block).is_expired(now, extra) }
}

/// `VerifyPermissions`.
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_verify_permissions(
    block: *const AuthBlock,
    user_id: u32,
    community: u32,
    min_trust: u16,
) -> bool {
    unsafe { (*block).verify_permissions(user_id, community, min_trust) }
}

/// The number of keys a block carries: a certificate's public key (0 or 1) or a key block's keys.
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_key_count(block: *const AuthBlock) -> usize {
    match unsafe { &(*block).body } {
        Body::PubKeyBlock(k) => k.keys.len(),
        Body::Certificate(c) => usize::from(c.pub_key.is_some()),
        Body::Certificate2(c) => usize::from(c.pub_key.is_some()),
        Body::Base => 0,
    }
}

/// A copy of key `index` (null past the count); free it with `ee_won_elgamal_free`.
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_key(block: *const AuthBlock, index: usize) -> *mut ElGamal {
    let key = match unsafe { &(*block).body } {
        Body::PubKeyBlock(k) => k.keys.get(index),
        Body::Certificate(c) if index == 0 => c.pub_key.as_ref(),
        Body::Certificate2(c) if index == 0 => c.pub_key.as_ref(),
        _ => None,
    };
    key.map_or(std::ptr::null_mut(), |k| Box::into_raw(Box::new(k.clone())))
}

/// `AuthPubKeyBlock::Verify(block)`.
///
/// # Safety
/// Both are live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_verify_with_block(key_block: *const AuthBlock, block: *const AuthBlock) -> bool {
    unsafe { (*key_block).verify_with_block(&*block) }
}

fn cert2(block: &AuthBlock) -> Option<&crate::auth::Certificate2> {
    match &block.body {
        Body::Certificate2(c) => Some(c),
        _ => None,
    }
}

/// What `ee_won_auth_cert2_query` asks of an `Auth2Certificate`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCert2Query {
    /// the community's key id
    KeyId = 0,
    /// the first community (0 if none)
    FirstCommunity = 1,
    /// the first community's trust
    FirstCommunityTrust = 2,
}

/// A number from an `Auth2Certificate` (0 for another block).
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_cert2_query(
    block: *const AuthBlock,
    query: EeWonCert2Query,
    community: u32,
) -> u32 {
    let Some(c) = cert2(unsafe { &*block }) else { return 0 };
    match query {
        EeWonCert2Query::KeyId => c.key_id(community),
        EeWonCert2Query::FirstCommunity => c.first_community(),
        EeWonCert2Query::FirstCommunityTrust => u32::from(c.first_community_trust()),
    }
}

/// `GetNickname(key)` (`first` false) or `GetFirstNickname` (`first` true): the units to the sink (empty when
/// absent).
///
/// # Safety
/// `block` is live; `key` holds `key_count` units.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_cert2_nickname(
    block: *const AuthBlock,
    first: bool,
    key: *const u16,
    key_count: usize,
    sink: EeWonUnitSink,
    context: *mut c_void,
) {
    let nick = cert2(unsafe { &*block })
        .map(|c| if first { c.first_nickname() } else { c.nickname(unsafe { units(key, key_count) }) })
        .unwrap_or_default();
    unsafe { give_units(sink, context, &nick) }
}

/// `GetUserData(community)` (`first` false) or `GetFirstUserData`: the bytes to the sink; false (no call) when
/// absent.
///
/// # Safety
/// `block` is live.
#[no_mangle]
pub unsafe extern "C" fn ee_won_auth_cert2_user_data(
    block: *const AuthBlock,
    first: bool,
    community: u32,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    let data = cert2(unsafe { &*block }).and_then(|c| {
        if first {
            c.user_data.values().next()
        } else {
            c.user_data.get(&community)
        }
    });
    match data {
        Some(d) => {
            unsafe { give(sink, context, d) };
            true
        }
        None => false,
    }
}

// ---- the lobby's pages as plain text

/// A page file's text (`plaintext::page_text`: HTML pages converted; `skip_first_if_text` drops a text page's first
/// character) as UTF-16 units to the sink.
///
/// # Safety
/// `data` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_page_text(
    data: *const u8,
    length: usize,
    skip_first_if_text: bool,
    sink: EeWonUnitSink,
    context: *mut c_void,
) {
    let page = crate::plaintext::page_text(unsafe { bytes(data, length) }, skip_first_if_text);
    unsafe { give_units(sink, context, &text::from_utf8(&page)) }
}

// ---- CD keys

unsafe fn raw8(raw: *const u8) -> [u8; 8] {
    let mut r = [0u8; 8];
    r.copy_from_slice(unsafe { bytes(raw, 8) });
    r
}

/// `CalcKeyFromBuffer`'s test of 8 raw bytes for a product.
///
/// # Safety
/// `product` holds `product_length` bytes; `raw` 8.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_is_valid(product: *const u8, product_length: usize, raw: *const u8) -> bool {
    cdkey::is_valid(unsafe { bytes(product, product_length) }, &unsafe { raw8(raw) })
}

/// `Init(string)`'s decoding: the raw bytes into `out` (8 bytes); false when the text is not a key.
///
/// # Safety
/// `text` holds `length` bytes; `out` has 8.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_parse(text: *const u8, length: usize, out: *mut u8) -> bool {
    match cdkey::parse(unsafe { bytes(text, length) }) {
        Some(raw) => {
            unsafe { copy_out(&raw, out, 8) };
            true
        }
        None => false,
    }
}

/// `GetString`: the 24 characters and a NUL into `out` (25 bytes).
///
/// # Safety
/// `raw` holds 8 bytes; `out` has 25.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_format(raw: *const u8, out: *mut c_char) {
    let mut s = cdkey::format(&unsafe { raw8(raw) }).into_bytes();
    s.push(0);
    unsafe { copy_out(&s, out as *mut u8, 25) };
}

/// `GetSymmetricKey`'s key bytes into `out` (12 bytes of room); returns their count.
///
/// # Safety
/// `product` holds `product_length` bytes; `out` has 12.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_symmetric_key(
    product: *const u8,
    product_length: usize,
    machine_id: u32,
    use_machine_id: bool,
    out: *mut u8,
) -> usize {
    let key = cdkey::symmetric_key(unsafe { bytes(product, product_length) }, use_machine_id.then_some(machine_id));
    unsafe { copy_out(&key, out, 12) }
}

/// `GetEncrypted`: the stored form of a raw key to the sink.
///
/// # Safety
/// As `ee_won_cdkey_symmetric_key`; `raw` holds 8 bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_encrypt(
    product: *const u8,
    product_length: usize,
    machine_id: u32,
    use_machine_id: bool,
    raw: *const u8,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    let p = unsafe { bytes(product, product_length) };
    match cdkey::encrypt(p, use_machine_id.then_some(machine_id), &unsafe { raw8(raw) }) {
        Some(e) => {
            unsafe { give(sink, context, &e) };
            true
        }
        None => false,
    }
}

/// What `ee_won_cdkey_decrypt` found.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeWonCdKeyDecoded {
    pub has_raw: bool,
    pub valid: bool,
    pub used_machine: bool,
    pub raw: [u8; 8],
}

/// `InitEncrypted`'s decoding: `has_machine_id` false when the machine has none.
///
/// # Safety
/// `product` and `data` hold their lengths; `out` is valid.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_decrypt(
    product: *const u8,
    product_length: usize,
    machine_id: u32,
    has_machine_id: bool,
    prefer_machine: bool,
    data: *const u8,
    length: usize,
    out: *mut EeWonCdKeyDecoded,
) {
    let d = cdkey::decrypt(
        unsafe { bytes(product, product_length) },
        has_machine_id.then_some(machine_id),
        prefer_machine,
        unsafe { bytes(data, length) },
    );
    unsafe {
        *out = EeWonCdKeyDecoded {
            has_raw: d.raw.is_some(),
            valid: d.valid,
            used_machine: d.used_machine,
            raw: d.raw.unwrap_or_default(),
        }
    }
}

/// `IsBeta` (`which` 0) or `IsForceCheck` (`which` 1) of a raw key of `length` bytes.
///
/// # Safety
/// `raw` holds `length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_flag(raw: *const u8, length: usize, which: u32) -> bool {
    let r = unsafe { bytes(raw, length) };
    if which == 0 {
        cdkey::is_beta(r)
    } else {
        cdkey::is_force_check(r)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn collect(context: *mut c_void, data: *const u8, length: usize) {
        unsafe { (*(context as *mut Vec<u8>)).extend_from_slice(bytes(data, length)) }
    }

    unsafe extern "C" fn collect_units(context: *mut c_void, data: *const u16, count: usize) {
        unsafe { (*(context as *mut Vec<u16>)).extend_from_slice(units(data, count)) }
    }

    unsafe extern "C" fn collect_wide16(context: *mut c_void, data: *const c_void, count: usize) {
        unsafe { (*(context as *mut Vec<u16>)).extend_from_slice(units(data.cast(), count)) }
    }

    unsafe extern "C" fn collect_wide32(context: *mut c_void, data: *const c_void, count: usize) {
        unsafe { (*(context as *mut Vec<u32>)).extend_from_slice(slice::from_raw_parts(data.cast::<u32>(), count)) }
    }

    #[test]
    fn the_games_utf8_crosses_to_wide_and_back_at_both_wchar_sizes() {
        let game = "Caf\u{e9} \u{20ac}\u{1f600}".as_bytes();
        let mut w16: Vec<u16> = Vec::new();
        unsafe {
            ee_won_wide_from_utf8(game.as_ptr(), game.len(), 2, collect_wide16, (&mut w16 as *mut Vec<u16>).cast())
        };
        assert_eq!(w16, vec![0x43, 0x61, 0x66, 0xe9, 0x20, 0x20ac, 0xd83d, 0xde00]);
        let mut w32: Vec<u32> = Vec::new();
        unsafe {
            ee_won_wide_from_utf8(game.as_ptr(), game.len(), 4, collect_wide32, (&mut w32 as *mut Vec<u32>).cast())
        };
        assert_eq!(w32, vec![0x43, 0x61, 0x66, 0xe9, 0x20, 0x20ac, 0x1f600]);
        let mut back = Vec::new();
        unsafe {
            ee_won_utf8_from_wide(w16.as_ptr().cast(), 2, w16.len(), collect, (&mut back as *mut Vec<u8>).cast())
        };
        assert_eq!(back, game);
        back.clear();
        unsafe {
            ee_won_utf8_from_wide(w32.as_ptr().cast(), 4, w32.len(), collect, (&mut back as *mut Vec<u8>).cast())
        };
        assert_eq!(back, game);
        // an empty text gives the sink nothing to keep; a stray byte and a lone surrogate become U+FFFD
        back.clear();
        unsafe { ee_won_utf8_from_wide(std::ptr::null(), 2, 0, collect, (&mut back as *mut Vec<u8>).cast()) };
        assert!(back.is_empty());
        w16.clear();
        unsafe { ee_won_wide_from_utf8(b"a\xe9".as_ptr(), 2, 2, collect_wide16, (&mut w16 as *mut Vec<u16>).cast()) };
        assert_eq!(w16, vec![0x61, 0xfffd]);
        let lone = [0x41u16, 0xd800];
        back.clear();
        unsafe { ee_won_utf8_from_wide(lone.as_ptr().cast(), 2, 2, collect, (&mut back as *mut Vec<u8>).cast()) };
        assert_eq!(back, "A\u{fffd}".as_bytes());
    }

    #[test]
    fn text_through_both_wchar_widths() {
        let wide32: [u32; 3] = [0x41, 0x20ac, 0x1f600];
        let n = unsafe { ee_won_utf16le_from_wide(wide32.as_ptr().cast(), 4, 3, std::ptr::null_mut(), 0) };
        assert_eq!(n, 8);
        let mut out = [0u8; 8];
        unsafe { ee_won_utf16le_from_wide(wide32.as_ptr().cast(), 4, 3, out.as_mut_ptr(), 8) };
        assert_eq!(out, [0x41, 0, 0xac, 0x20, 0x3d, 0xd8, 0x00, 0xde]);
        let mut back = [0u32; 4];
        let n = unsafe { ee_won_wide_from_utf16le(out.as_ptr(), 4, 4, back.as_mut_ptr().cast(), 4) };
        assert_eq!(&back[..n], &wide32);
        let mut back16 = [0u16; 4];
        let n = unsafe { ee_won_wide_from_utf16le(out.as_ptr(), 4, 2, back16.as_mut_ptr().cast(), 4) };
        assert_eq!(&back16[..n], &[0x41, 0x20ac, 0xd83d, 0xde00]);
        assert_eq!(ee_won_utf16_join(0xd83d, 0xde00), 0x1f600);
        assert_eq!(ee_won_utf16_join(0x41, 0xde00), 0);
        let mut u = Vec::<u16>::new();
        unsafe {
            ee_won_utf16_from_wide(wide32.as_ptr().cast(), 4, 3, collect_units, (&mut u as *mut Vec<u16>).cast())
        };
        assert_eq!(u, vec![0x41, 0x20ac, 0xd83d, 0xde00]);
    }

    #[test]
    fn buffer_helpers() {
        let mut out = [0u8; 6];
        assert_eq!(unsafe { ee_won_length_prefix(2, false, 0x10000, out.as_mut_ptr()) }, 6);
        assert_eq!(out, [0xff, 0xff, 0, 0, 1, 0]);
        assert_eq!(unsafe { ee_won_encode_length(0x1234, 2, out.as_mut_ptr()) }, 2);
        assert_eq!(&out[..2], &[0x34, 0x12]);
        assert_eq!(ee_won_write_grow(0, 0, 5), 32);
        assert_eq!(ee_won_write_grow(32, 0, 5), 0);
        assert_eq!(ee_won_write_chunk(32, 100, 0), 128);
        let mut pos = 2i64;
        assert!(unsafe { ee_won_read_take(4, &mut pos, 2) });
        assert_eq!(pos, 4);
        assert!(!unsafe { ee_won_read_take(4, &mut pos, 1) });
        let data = [0xff, 0xff, 7];
        let (mut pos, mut v) = (0i64, 0u32);
        assert!(unsafe { ee_won_read_length(data.as_ptr(), 3, &mut pos, 2, &mut v) });
        assert_eq!((pos, v), (2, 0xffff_ffff));
        assert!(!unsafe { ee_won_read_length(data.as_ptr(), 3, &mut pos, 4, &mut v) });
        assert_eq!(pos, 2);
        assert_eq!(unsafe { ee_won_crc16_put(0, b"123456789".as_ptr(), 9) }, 0xfee8);
    }

    #[test]
    fn md5_and_blowfish_state() {
        let mut m = Md5::new();
        unsafe {
            ee_won_md5_reset(&mut m);
            ee_won_md5_update(&mut m, b"abc".as_ptr(), 3);
        }
        let mut d = [0u8; 16];
        unsafe { ee_won_md5_finish(&mut m, d.as_mut_ptr()) };
        assert_eq!(d, Md5::digest(b"abc"));
        let mut bf = std::mem::MaybeUninit::<Blowfish>::uninit();
        unsafe { ee_won_blowfish_init(bf.as_mut_ptr()) };
        let mut bf = unsafe { bf.assume_init() };
        let mut out = Vec::<u8>::new();
        let ctx = (&mut out as *mut Vec<u8>).cast();
        assert!(!unsafe { ee_won_blowfish_encrypt(&bf, b"x".as_ptr(), 1, collect, ctx) });
        assert!(unsafe { ee_won_blowfish_set_key(&mut bf, b"12345".as_ptr(), 5) });
        assert_eq!(unsafe { ee_won_blowfish_key_length(&bf) }, 5);
        assert!(unsafe { ee_won_blowfish_encrypt(&bf, b"hello".as_ptr(), 5, collect, ctx) });
        let enc = std::mem::take(&mut out);
        assert!(unsafe { ee_won_blowfish_decrypt(&bf, enc.as_ptr(), enc.len(), collect, ctx) });
        assert_eq!(out, b"hello");
        assert!(unsafe { ee_won_blowfish_create(&mut bf, 8) });
        assert!(!unsafe { ee_won_blowfish_create(&mut bf, -1) });
    }

    #[test]
    fn keys_and_blocks() {
        use crate::bigint::BigInt;
        let p = BigInt::from_i32(1_073_761_061);
        let q = BigInt::from_i32(1019);
        let g = BigInt::from_i32(611_659_888);
        let x = BigInt::from_i32(77);
        let y = g.mod_exp(&x, &p).unwrap();
        let private = crate::der::insert(&[&p, &q, &g, &y, &x]);
        let k = ee_won_elgamal_new();
        assert!(!unsafe { ee_won_elgamal_is_public(k) });
        assert!(unsafe { ee_won_elgamal_set_key(k, private.as_ptr(), private.len(), true) });
        assert!(unsafe { ee_won_elgamal_is_private(k) });
        let mut sig = Vec::<u8>::new();
        let ctx = (&mut sig as *mut Vec<u8>).cast();
        assert!(unsafe { ee_won_elgamal_op(k, EeWonElGamalOp::Sign, b"m".as_ptr(), 1, collect, ctx) });
        let c = unsafe { ee_won_elgamal_clone(k) };
        assert!(unsafe { ee_won_elgamal_verify(c, b"m".as_ptr(), 1, sig.as_ptr(), sig.len()) });
        unsafe { ee_won_elgamal_invalidate(c) };
        assert!(!unsafe { ee_won_elgamal_verify(c, b"m".as_ptr(), 1, sig.as_ptr(), sig.len()) });
        let mut raw = Vec::<u8>::new();
        assert!(unsafe {
            ee_won_elgamal_op(
                k,
                EeWonElGamalOp::RawPrivateKey,
                std::ptr::null(),
                0,
                collect,
                (&mut raw as *mut Vec<u8>).cast(),
            )
        });
        assert_eq!(raw, private);

        // a key block holding the public key, signed by it
        let public = crate::der::insert(&[&p, &q, &g, &y]);
        let mut w = wire::Writer::new(0, false);
        w.append_bytes(&[3, 0, 1, 0, 0, 0, 2, 0, 0, 0, 17, 0, 1, 0]);
        w.append_short(public.len() as i16);
        w.append_bytes(&public);
        let mut block = w.data().to_vec();
        let mut s = Vec::<u8>::new();
        unsafe {
            ee_won_elgamal_op(
                k,
                EeWonElGamalOp::Sign,
                block.as_ptr(),
                block.len(),
                collect,
                (&mut s as *mut Vec<u8>).cast(),
            )
        };
        block.extend_from_slice(&s);
        let b = unsafe { ee_won_auth_unpack(EE_WON_AUTH_PUB_KEY_BLOCK, block.as_ptr(), block.len()) };
        let mut h = EeWonAuthHeader::default();
        unsafe { ee_won_auth_header(b, &mut h) };
        assert!(h.valid);
        assert_eq!((h.family, h.issue_time, h.expire_time, h.key_block_id), (3, 1, 2, 17));
        assert_eq!(unsafe { ee_won_auth_key_count(b) }, 1);
        let key = unsafe { ee_won_auth_key(b, 0) };
        assert!(!key.is_null() && unsafe { ee_won_auth_key(b, 1) }.is_null());
        assert!(unsafe { ee_won_auth_verify(b, key) });
        assert!(unsafe { ee_won_auth_verify_with_block(b, b) });
        assert!(!unsafe { ee_won_auth_is_expired(b, 0, 0) });
        assert!(!unsafe { ee_won_auth_verify_permissions(b, 0, 0, 0) });
        assert_eq!(unsafe { ee_won_auth_cert2_query(b, EeWonCert2Query::FirstCommunity, 0) }, 0);
        let mut nick = Vec::<u16>::new();
        unsafe {
            ee_won_auth_cert2_nickname(b, true, std::ptr::null(), 0, collect_units, (&mut nick as *mut Vec<u16>).cast())
        };
        assert!(nick.is_empty());
        let mut ud = Vec::<u8>::new();
        assert!(!unsafe { ee_won_auth_cert2_user_data(b, true, 0, collect, (&mut ud as *mut Vec<u8>).cast()) });
        unsafe {
            ee_won_auth_free(b);
            ee_won_elgamal_free(key);
            ee_won_elgamal_free(c);
            ee_won_elgamal_free(k);
            ee_won_elgamal_free(std::ptr::null_mut());
            ee_won_auth_free(std::ptr::null_mut());
        }
    }

    #[test]
    fn page_text_call() {
        let mut u = Vec::<u16>::new();
        let page = b"<p>Hi &amp; \x93bye\x94</p>";
        unsafe { ee_won_page_text(page.as_ptr(), page.len(), true, collect_units, (&mut u as *mut Vec<u16>).cast()) };
        assert_eq!(text::to_display(&u), "Hi & \u{201c}bye\u{201d}");
    }

    #[test]
    fn cd_key_calls() {
        let mut raw = [1u8, 2, 3, 0, 5, 6, 7, 8];
        raw[3] = cdkey::check_byte(b"AoC", &raw);
        assert!(unsafe { ee_won_cdkey_is_valid(b"AoC".as_ptr(), 3, raw.as_ptr()) });
        let mut text = [0 as c_char; 25];
        unsafe { ee_won_cdkey_format(raw.as_ptr(), text.as_mut_ptr()) };
        let mut back = [0u8; 8];
        assert!(unsafe { ee_won_cdkey_parse(text.as_ptr().cast(), 24, back.as_mut_ptr()) });
        assert_eq!(back, raw);
        assert_eq!(text[24], 0);
        let mut key = [0u8; 12];
        assert_eq!(unsafe { ee_won_cdkey_symmetric_key(b"AoC".as_ptr(), 3, 7, true, key.as_mut_ptr()) }, 12);
        let mut enc = Vec::<u8>::new();
        assert!(unsafe {
            ee_won_cdkey_encrypt(b"AoC".as_ptr(), 3, 7, true, raw.as_ptr(), collect, (&mut enc as *mut Vec<u8>).cast())
        });
        let mut d = EeWonCdKeyDecoded::default();
        unsafe { ee_won_cdkey_decrypt(b"AoC".as_ptr(), 3, 7, true, true, enc.as_ptr(), enc.len(), &mut d) };
        assert!(d.has_raw && d.valid && d.used_machine);
        assert_eq!(d.raw, raw);
        assert!(unsafe { ee_won_cdkey_flag(raw.as_ptr(), 8, 0) });
        assert!(!unsafe { ee_won_cdkey_flag(raw.as_ptr(), 8, 1) });
    }
}
