//! The C ABI behind the C++ `UDataCompression` and `UEncryption` forwarders.

use std::ffi::c_void;
use std::slice;

use crate::compression;
use crate::encryption;

/// Receives output bytes (the forwarder copies them into a `UString`). Valid only during the call.
pub type EeCodecSink = unsafe extern "C" fn(context: *mut c_void, bytes: *const u8, length: usize);

unsafe fn bytes<'a>(data: *const u8, length: usize) -> &'a [u8] {
    if length == 0 {
        &[]
    } else {
        // SAFETY: the caller passes a valid buffer of `length` bytes (each function's contract)
        unsafe { slice::from_raw_parts(data, length) }
    }
}

unsafe fn give(sink: EeCodecSink, context: *mut c_void, data: &[u8]) {
    // SAFETY: the sink's contract: `data` is valid during the call
    unsafe { sink(context, data.as_ptr(), data.len()) }
}

/// `UDataCompression::Compress`.
///
/// # Safety
/// `input` is valid for `input_length` bytes, `output` for `capacity` bytes, `out_length` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_compress(
    input: *const u8,
    input_length: u32,
    output: *mut u8,
    capacity: u32,
    out_length: *mut u32,
    level: u32,
) -> i32 {
    let (input, output) =
        unsafe { (bytes(input, input_length as usize), slice::from_raw_parts_mut(output, capacity as usize)) };
    let (code, length) = compression::compress(input, output, level);
    unsafe { *out_length = length };
    code
}

/// `UDataCompression::Decompress`.
///
/// # Safety
/// `input` is valid for `input_length` bytes, `output` for `capacity` bytes, `out_length` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_decompress(
    input: *const u8,
    input_length: u32,
    output: *mut u8,
    capacity: u32,
    out_length: *mut u32,
) -> i32 {
    let (input, output) =
        unsafe { (bytes(input, input_length as usize), slice::from_raw_parts_mut(output, capacity as usize)) };
    let (code, length) = compression::decompress(input, output);
    unsafe { *out_length = length };
    code
}

/// `UDataCompression::GetDecompressedSize`.
///
/// # Safety
/// `input` is valid for `input_length` bytes, `out_size` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_decompressed_size(input: *const u8, input_length: u32, out_size: *mut u32) -> i32 {
    let (code, size) = compression::decompressed_size(unsafe { bytes(input, input_length as usize) });
    unsafe { *out_size = size };
    code
}

/// `UDataCompression::GetIsCompressed`.
///
/// # Safety
/// `input` is valid for `input_length` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_is_compressed(input: *const u8, input_length: u32) -> bool {
    compression::is_compressed(unsafe { bytes(input, input_length as usize) })
}

/// `UEEncryptUString`. `capacity` is the input `UString`'s allocated size.
///
/// # Safety
/// `text` is valid for `length` bytes; `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_encrypt_ustring(
    text: *const u8,
    length: usize,
    capacity: usize,
    sink: EeCodecSink,
    context: *mut c_void,
) {
    let out = encryption::encrypt_ustring(unsafe { bytes(text, length) }, capacity);
    unsafe { give(sink, context, &out) };
}

/// `UEDecryptUString`. Returns whether the checksum matches.
///
/// # Safety
/// `text` is valid for `length` bytes; `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_decrypt_ustring(
    text: *const u8,
    length: usize,
    sink: EeCodecSink,
    context: *mut c_void,
) -> bool {
    let (out, ok) = encryption::decrypt_ustring(unsafe { bytes(text, length) });
    unsafe { give(sink, context, &out) };
    ok
}

/// `UEEncryptAuthorizationCode`.
///
/// # Safety
/// `code` is valid for `length` bytes; `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_encrypt_authorization_code(
    code: *const u8,
    length: usize,
    sink: EeCodecSink,
    context: *mut c_void,
) {
    let out = encryption::encrypt_authorization_code(unsafe { bytes(code, length) });
    unsafe { give(sink, context, &out) };
}

/// `UEDecryptAuthorizationCode`.
///
/// # Safety
/// `code` is valid for `length` bytes; `first` and `second` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_decrypt_authorization_code(
    code: *const u8,
    length: usize,
    first: *mut u32,
    second: *mut u32,
) {
    let (a, b) = encryption::decrypt_authorization_code(unsafe { bytes(code, length) });
    unsafe {
        *first = a;
        *second = b;
    }
}

/// `UEEncryptExpiration`.
///
/// # Safety
/// `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_encrypt_expiration(first: u32, second: u32, sink: EeCodecSink, context: *mut c_void) {
    let out = encryption::encrypt_expiration(first, second);
    unsafe { give(sink, context, &out) };
}

/// `UEDecryptExpiration`. Returns whether the checksum matches.
///
/// # Safety
/// `text` is valid for `length` bytes; `first` and `second` for a write.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_decrypt_expiration(
    text: *const u8,
    length: usize,
    first: *mut u32,
    second: *mut u32,
) -> bool {
    let (a, b, ok) = encryption::decrypt_expiration(unsafe { bytes(text, length) });
    unsafe {
        *first = a;
        *second = b;
    }
    ok
}

/// `UEEncryptGUID`. `sink_guid` receives the padded GUID (written back into the caller's string), then `sink_out` the
/// encrypted text.
///
/// # Safety
/// `guid` is valid for `length` bytes; each sink is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_encrypt_guid(
    value: u32,
    guid: *const u8,
    length: usize,
    capacity: usize,
    key: u32,
    sink_guid: EeCodecSink,
    context_guid: *mut c_void,
    sink_out: EeCodecSink,
    context_out: *mut c_void,
) {
    let (padded, out) = encryption::encrypt_guid(value, unsafe { bytes(guid, length) }, capacity, key);
    unsafe {
        give(sink_guid, context_guid, &padded);
        give(sink_out, context_out, &out);
    }
}

/// `UEDecryptGUID`: the GUID is appended to `prefix` (capped at `capacity`). Returns whether the checksum matches.
///
/// # Safety
/// `text` is valid for `length` bytes, `prefix` for `prefix_length`, `value` for a write; `sink` is called once.
#[no_mangle]
pub unsafe extern "C" fn ee_ue_decrypt_guid(
    text: *const u8,
    length: usize,
    key: u32,
    value: *mut u32,
    prefix: *const u8,
    prefix_length: usize,
    capacity: usize,
    sink: EeCodecSink,
    context: *mut c_void,
) -> bool {
    let (v, guid, ok) = encryption::decrypt_guid(
        unsafe { bytes(text, length) },
        key,
        unsafe { bytes(prefix, prefix_length) },
        capacity,
    );
    unsafe {
        *value = v;
        give(sink, context, &guid);
    }
    ok
}

#[cfg(test)]
mod tests {
    use super::*;

    unsafe extern "C" fn collect(context: *mut c_void, data: *const u8, length: usize) {
        let out = unsafe { &mut *(context as *mut Vec<u8>) };
        out.extend_from_slice(unsafe { bytes(data, length) });
    }

    #[test]
    fn compress_entry_points_round_trip() {
        let data = b"round trip through the C ABI, round trip through the C ABI";
        let mut out = [0u8; 200];
        let mut length = 0u32;
        let code = unsafe { ee_compress(data.as_ptr(), data.len() as u32, out.as_mut_ptr(), 200, &mut length, 0x1000) };
        assert_eq!(code, 0);
        assert!(unsafe { ee_is_compressed(out.as_ptr(), length) });
        let mut size = 0u32;
        assert_eq!(unsafe { ee_decompressed_size(out.as_ptr(), length, &mut size) }, 0);
        assert_eq!(size, data.len() as u32);
        let mut back = [0u8; 100];
        let mut back_length = 0u32;
        let code = unsafe { ee_decompress(out.as_ptr(), length, back.as_mut_ptr(), 100, &mut back_length) };
        assert_eq!((code, back_length), (0, data.len() as u32));
        assert_eq!(&back[..data.len()], data);
    }

    #[test]
    fn empty_input_is_accepted_with_a_null_pointer() {
        assert!(!unsafe { ee_is_compressed(std::ptr::null(), 0) });
    }

    #[test]
    fn encryption_entry_points_round_trip() {
        let mut out = Vec::new();
        let context = &mut out as *mut Vec<u8> as *mut c_void;
        unsafe { ee_ue_encrypt_ustring(b"hello".as_ptr(), 5, 256, collect, context) };
        let mut text = Vec::new();
        let context2 = &mut text as *mut Vec<u8> as *mut c_void;
        assert!(unsafe { ee_ue_decrypt_ustring(out.as_ptr(), out.len(), collect, context2) });
        assert_eq!(text, b"hello");

        let mut enc = Vec::new();
        let ctx = &mut enc as *mut Vec<u8> as *mut c_void;
        unsafe { ee_ue_encrypt_expiration(1, 2, collect, ctx) };
        let (mut a, mut b) = (0u32, 0u32);
        assert!(unsafe { ee_ue_decrypt_expiration(enc.as_ptr(), enc.len(), &mut a, &mut b) });
        assert_eq!((a, b), (1, 2));

        let mut code = Vec::new();
        let ctx = &mut code as *mut Vec<u8> as *mut c_void;
        unsafe { ee_ue_encrypt_authorization_code(b"0123456789ABCDEF".as_ptr(), 16, collect, ctx) };
        unsafe { ee_ue_decrypt_authorization_code(code.as_ptr(), code.len(), &mut a, &mut b) };
        assert_eq!((a, b), (0x01234567, 0x89ABCDEF));

        let mut padded = Vec::new();
        let mut enc = Vec::new();
        let (p, e) = (&mut padded as *mut Vec<u8> as *mut c_void, &mut enc as *mut Vec<u8> as *mut c_void);
        unsafe { ee_ue_encrypt_guid(7, b"xy".as_ptr(), 2, 16, 2, collect, p, collect, e) };
        assert_eq!(padded, b"xy  ");
        let mut guid = Vec::new();
        let ctx = &mut guid as *mut Vec<u8> as *mut c_void;
        let mut value = 0u32;
        let ok =
            unsafe { ee_ue_decrypt_guid(enc.as_ptr(), enc.len(), 2, &mut value, b"".as_ptr(), 0, 256, collect, ctx) };
        assert!(ok);
        assert_eq!((value, guid.as_slice()), (7, b"xy  ".as_slice()));
    }
}
