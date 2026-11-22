//! CDKey's storage (AuthContext.cpp's `LoadFromRegistry`, `SaveToRegistry`, `ClearFromRegistry`, `SaveToFile` and
//! `LoadFromFile` until net round 29): the encrypted key as a binary value named by the product under a key path
//! (config.yaml through the Rust RegKey, regkey.rs), or as a file's bytes. The C++ CDKey encrypts and decrypts (its
//! header, over cdkey.rs); these move the encrypted bytes.

use std::ffi::{c_char, c_void, CStr};

use crate::ffi::{bytes, give, EeWonByteSink};
use crate::regkey::{Binary, EeWonRegKey, RegValue};

/// `RegKey(root, path, false, true)`: open when the block exists (the key path's).
fn open_key(root: i32, path: *const c_char) -> Option<EeWonRegKey> {
    let p: &[u8] = if path.is_null() {
        &[]
    } else {
        // SAFETY: the caller's C string
        unsafe { CStr::from_ptr(path) }.to_bytes()
    };
    let mut key = EeWonRegKey::default();
    key.open_new_key(root, p);
    key.open.then_some(key)
}

/// `LoadFromRegistry`'s read: the product's value as bytes to `sink` (false when the key or value is missing, not
/// hex, or empty: `QueryBinary` other than 0).
///
/// # Safety
/// `path` is a C string or NULL; `product` holds `product_len` bytes; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_registry_read(
    root: i32,
    path: *const c_char,
    product: *const u8,
    product_len: usize,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    let Some(key) = open_key(root, path) else { return false };
    // SAFETY: the caller's bytes
    match key.query_binary(unsafe { bytes(product, product_len) }) {
        Some(Binary::Bytes(b)) => {
            // SAFETY: the caller's sink
            unsafe { give(sink, context, &b) };
            true
        }
        _ => false,
    }
}

/// `SaveToRegistry`'s write: the encrypted bytes as the product's value (false when the key does not open or the
/// value is not set).
///
/// # Safety
/// `path` is a C string or NULL; `product` and `data` hold their lengths.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_registry_write(
    root: i32,
    path: *const c_char,
    product: *const u8,
    product_len: usize,
    data: *const u8,
    data_len: usize,
) -> bool {
    let Some(key) = open_key(root, path) else { return false };
    // SAFETY: the caller's bytes
    unsafe { key.set_value(bytes(product, product_len), RegValue::Binary(bytes(data, data_len))) }
}

/// `ClearFromRegistry`: the product's value deleted.
///
/// # Safety
/// `path` is a C string or NULL; `product` holds `product_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_registry_clear(
    root: i32,
    path: *const c_char,
    product: *const u8,
    product_len: usize,
) {
    if let Some(key) = open_key(root, path) {
        // SAFETY: the caller's bytes
        key.delete_value(unsafe { bytes(product, product_len) });
    }
}

/// `SaveToFile`'s write: the encrypted bytes through `PlatFOpen(path, "wb")` (false when it does not open or the
/// write is short; the file is closed either way).
///
/// # Safety
/// `path` is a C string; `data` holds `data_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_file_write(path: *const c_char, data: *const u8, data_len: usize) -> bool {
    // SAFETY: the caller's C string and bytes
    let (p, d) = unsafe { (CStr::from_ptr(path).to_bytes(), bytes(data, data_len)) };
    let f = crate::hostnative::plat_fopen(p, c"wb");
    if f.is_null() {
        return false;
    }
    // SAFETY: the open file, closed once
    let written = unsafe { crate::hostnative::fwrite(f, d) };
    unsafe { crate::hostnative::fclose(f) };
    written == d.len()
}

/// `LoadFromFile`'s read: up to 2,048 bytes of the file to `sink` (false when it does not open).
///
/// # Safety
/// `path` is a C string; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_file_read(
    path: *const c_char,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    // SAFETY: the caller's C string
    let p = unsafe { CStr::from_ptr(path) }.to_bytes();
    let Some(mut b) = crate::authlogin::read_file(p, c"rb") else { return false };
    b.truncate(2048);
    // SAFETY: the caller's sink
    unsafe { give(sink, context, &b) };
    true
}

// ---- CDKey whole (R6 WON round 33): the product, the raw key and its validity are this object's; the C++ CDKey
// (ee/wonapi/WONAuthContext.h) is a shim owning one, every method a call here. The class's two virtuals had no
// override: the new encryption method is always used (`GetUseNewEncryptMethod` true) and the old method's notice
// (`NotifyUsedOldMethodToDecrypt`) did nothing.

/// The CDKey's data (0x1c bytes in the original: the product string, the key's ByteBuffer, the validity).
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct EeWonCdKeyObj {
    product: Vec<u8>,
    key: Option<[u8; 8]>,
    valid: bool,
}

/// `CDKey::GetMachineId`: the machine's id, `None` without one.
fn machine_id() -> Option<u32> {
    let mut found = 0;
    // SAFETY: an out integer
    let id = unsafe { ee_base::plat_core::message::PlatSystemMachineId(&mut found) };
    (found != 0).then_some(id)
}

impl EeWonCdKeyObj {
    pub fn new(product: &[u8]) -> Self {
        Self { product: product.to_vec(), key: None, valid: false }
    }

    /// `CalcKeyFromBuffer`: the 8 raw bytes kept, valid when their check byte is right.
    fn calc(&mut self, raw: [u8; 8]) -> bool {
        self.key = Some(raw);
        self.valid = crate::cdkey::is_valid(&self.product, &raw);
        self.valid
    }

    /// `InitEncrypted`: a stored key decrypted with the machine-bound or the old key (`None`: a NULL pointer).
    pub fn init_encrypted(&mut self, data: Option<&[u8]>) -> bool {
        self.valid = false;
        self.key = None;
        let Some(data) = data else { return false };
        let d = crate::cdkey::decrypt(&self.product, machine_id(), true, data);
        if let Some(raw) = d.raw {
            self.key = Some(raw);
            self.valid = d.valid;
        }
        self.valid
    }

    /// `Init(text)`: the key's text, true when its check byte is right.
    pub fn init(&mut self, text: &[u8]) -> bool {
        self.valid = false;
        self.key = None;
        crate::cdkey::parse(text).is_some_and(|raw| self.calc(raw))
    }

    /// `InitRaw`.
    pub fn init_raw(&mut self, raw: [u8; 8]) -> bool {
        self.valid = false;
        self.key = None;
        self.calc(raw)
    }

    /// `GetString`: the key's text, empty without a key.
    pub fn text(&self) -> String {
        self.key.map(|k| crate::cdkey::format(&k)).unwrap_or_default()
    }

    /// `GetEncrypted`: the key encrypted for storage (with the machine's id when it has one); `None` when invalid.
    pub fn encrypted(&self) -> Option<Vec<u8>> {
        let key = self.key.filter(|_| self.valid)?;
        crate::cdkey::encrypt(&self.product, machine_id(), &key)
    }

    /// `Flag`: the beta (0) and force-check (1) flags.
    pub fn flag(&self, which: u32) -> bool {
        self.key.is_some_and(|k| if which == 0 { crate::cdkey::is_beta(&k) } else { crate::cdkey::is_force_check(&k) })
    }

    /// `LoadFromRegistry`: the product's key from the registry value, false when absent or invalid.
    fn load_registry(&mut self, root: i32, path: *const c_char) -> bool {
        if self.product.is_empty() {
            return false;
        }
        let mut b: Vec<u8> = Vec::new();
        // SAFETY: the caller's path; the sink fills `b`
        let read = unsafe {
            ee_won_cdkey_registry_read(
                root,
                path,
                self.product.as_ptr(),
                self.product.len(),
                keep_bytes,
                &mut b as *mut Vec<u8> as *mut c_void,
            )
        };
        read && self.init_encrypted(Some(&b))
    }

    /// `SaveToRegistry`.
    fn save_registry(&self, root: i32, path: *const c_char) -> bool {
        let Some(e) = self.encrypted() else { return false };
        // SAFETY: the caller's path, our bytes
        unsafe {
            ee_won_cdkey_registry_write(root, path, self.product.as_ptr(), self.product.len(), e.as_ptr(), e.len())
        }
    }

    /// `ClearFromRegistry`.
    fn clear_registry(&self, root: i32, path: *const c_char) {
        if !self.product.is_empty() {
            // SAFETY: as above
            unsafe { ee_won_cdkey_registry_clear(root, path, self.product.as_ptr(), self.product.len()) }
        }
    }

    /// `SaveToFile`.
    fn save_file(&self, path: *const c_char) -> bool {
        let Some(e) = self.encrypted() else { return false };
        // SAFETY: the caller's C string, our bytes
        unsafe { ee_won_cdkey_file_write(path, e.as_ptr(), e.len()) }
    }

    /// `LoadFromFile`.
    fn load_file(&mut self, path: *const c_char) -> bool {
        let mut b: Vec<u8> = Vec::new();
        // SAFETY: the caller's C string; the sink fills `b`
        let read = unsafe { ee_won_cdkey_file_read(path, keep_bytes, &mut b as *mut Vec<u8> as *mut c_void) };
        read && self.init_encrypted(Some(&b))
    }
}

unsafe extern "C" fn keep_bytes(context: *mut c_void, data: *const u8, length: usize) {
    // SAFETY: the Vec the caller passed, the sink's bytes
    unsafe { *(context as *mut Vec<u8>) = bytes(data, length).to_vec() };
}

/// The CDKey's calls (`ee_won_cdkey_obj_call`).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCdKeyCall {
    /// `IsValid()`.
    IsValid,
    /// `Invalidate()`.
    Invalidate,
    /// the key is present (`mKey.get() != NULL`)
    HasKey,
    /// `Flag(a)`.
    Flag,
    /// `InitRaw(data)` (8 bytes).
    InitRaw,
    /// `Init(data, a)`: the key's text.
    Init,
    /// `InitEncrypted(data, a)` (`data` NULL: false).
    InitEncrypted,
    /// `SetProductString(data, a)`: invalidated, the product set.
    SetProduct,
    /// `LoadFromRegistry(a, data)`: `data` the key path (a C string).
    LoadRegistry,
    /// `SaveToRegistry(a, data)`.
    SaveRegistry,
    /// `ClearFromRegistry(a, data)`.
    ClearRegistry,
    /// `SaveToFile(data)`: `data` the file name (a C string).
    SaveFile,
    /// `LoadFromFile(data)`.
    LoadFile,
}

/// `CDKey::GetMachineId`: the machine's id (0 without one), `ok` (when not NULL) whether it has one.
///
/// # Safety
/// `ok` is NULL or a bool.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_machine_id(ok: *mut bool) -> u32 {
    let mut found = 0;
    // SAFETY: an out integer
    let id = unsafe { ee_base::plat_core::message::PlatSystemMachineId(&mut found) };
    if !ok.is_null() {
        // SAFETY: the caller's bool
        unsafe { *ok = found != 0 };
    }
    id
}

/// A new CDKey of the product (`product_len` bytes).
///
/// # Safety
/// `product` holds `product_len` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_new(product: *const u8, product_len: usize) -> *mut EeWonCdKeyObj {
    // SAFETY: the caller's bytes
    Box::into_raw(Box::new(EeWonCdKeyObj::new(unsafe { bytes(product, product_len) })))
}

/// A copy (the C++ copy constructor).
///
/// # Safety
/// `k` is a live CDKey.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_clone(k: *const EeWonCdKeyObj) -> *mut EeWonCdKeyObj {
    // SAFETY: the caller's key
    Box::into_raw(Box::new(unsafe { &*k }.clone()))
}

/// `*to = *from` (the C++ assignment).
///
/// # Safety
/// Both are live CDKeys.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_assign(to: *mut EeWonCdKeyObj, from: *const EeWonCdKeyObj) {
    if !std::ptr::eq(to, from) {
        // SAFETY: the caller's keys, two distinct objects
        unsafe { (*to).clone_from(&*from) };
    }
}

/// Frees a CDKey.
///
/// # Safety
/// `k` came from `ee_won_cdkey_obj_new` or `_clone`, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_free(k: *mut EeWonCdKeyObj) {
    if !k.is_null() {
        // SAFETY: as documented
        drop(unsafe { Box::from_raw(k) });
    }
}

/// One call on a CDKey (`EeWonCdKeyCall`): its truth value.
///
/// # Safety
/// `k` is a live CDKey; `data` as the call documents (`a` bytes, a C string or 8 bytes).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_call(
    k: *mut EeWonCdKeyObj,
    call: EeWonCdKeyCall,
    data: *const u8,
    a: isize,
) -> bool {
    // SAFETY: the caller's key
    let k = unsafe { &mut *k };
    type C = EeWonCdKeyCall;
    let path = data as *const c_char;
    match call {
        C::IsValid => k.valid,
        C::Invalidate => {
            k.valid = false;
            false
        }
        C::HasKey => k.key.is_some(),
        C::Flag => k.flag(a as u32),
        // SAFETY: 8 bytes (the call's contract)
        C::InitRaw => k.init_raw(unsafe { bytes(data, 8) }.try_into().unwrap()),
        // SAFETY: `a` bytes
        C::Init => k.init(unsafe { bytes(data, a as usize) }),
        // SAFETY: as above
        C::InitEncrypted => k.init_encrypted((!data.is_null()).then(|| unsafe { bytes(data, a as usize) })),
        C::SetProduct => {
            k.valid = false;
            // SAFETY: as above
            k.product = unsafe { bytes(data, a as usize) }.to_vec();
            false
        }
        C::LoadRegistry => k.load_registry(a as i32, path),
        C::SaveRegistry => k.save_registry(a as i32, path),
        C::ClearRegistry => {
            k.clear_registry(a as i32, path);
            false
        }
        C::SaveFile => k.save_file(path),
        C::LoadFile => k.load_file(path),
    }
}

/// The CDKey's bytes to `sink`: 0 the product, 1 the raw key (none without one), 2 the text (`GetString`), 3 the
/// encrypted key (`GetEncrypted`; none when invalid). Returns whether any were given.
///
/// # Safety
/// `k` is a live CDKey; `sink` is valid with `context`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_cdkey_obj_bytes(
    k: *const EeWonCdKeyObj,
    which: i32,
    sink: EeWonByteSink,
    context: *mut c_void,
) -> bool {
    // SAFETY: the caller's key
    let k = unsafe { &*k };
    let b = match which {
        0 => Some(k.product.clone()),
        1 => k.key.map(|x| x.to_vec()),
        2 => Some(k.text().into_bytes()),
        _ => k.encrypted(),
    };
    match b {
        Some(b) => {
            // SAFETY: the caller's sink
            unsafe { give(sink, context, &b) };
            true
        }
        None => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn keep(context: *mut c_void, data: *const u8, length: usize) {
        unsafe { *(context as *mut Vec<u8>) = bytes(data, length).to_vec() };
    }

    #[test]
    fn a_file_round_trips_its_first_2048_bytes() {
        let dir = std::env::temp_dir().join(format!("ee_won_cdkeystore_{}", std::process::id()));
        let _ = std::fs::create_dir_all(&dir);
        let f = std::ffi::CString::new(dir.join("key.bin").to_str().unwrap()).unwrap();
        let data: Vec<u8> = (0..3000u32).map(|i| i as u8).collect();
        assert!(unsafe { ee_won_cdkey_file_write(f.as_ptr(), data.as_ptr(), data.len()) });
        let mut out = Vec::new();
        assert!(unsafe { ee_won_cdkey_file_read(f.as_ptr(), keep, &mut out as *mut Vec<u8> as *mut c_void) });
        assert_eq!(out, data[..2048]);
        let missing = std::ffi::CString::new(dir.join("none.bin").to_str().unwrap()).unwrap();
        assert!(!unsafe { ee_won_cdkey_file_read(missing.as_ptr(), keep, &mut out as *mut Vec<u8> as *mut c_void) });
        let _ = std::fs::remove_dir_all(&dir);
    }
}
