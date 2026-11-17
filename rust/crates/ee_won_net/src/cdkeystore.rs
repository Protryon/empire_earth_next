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
