//! The game's models and textures on the data path (R4.3, fifteenth part; GEOpenFormatsFile.cpp before): a model's
//! `<name>.glb`, else `<name>.gltf` with its external buffers, and a texture's `<name>.png` with its further frames
//! `<name>@1.png`, `<name>@2.png`, ... (up to the first missing one), read whole and decoded here (`gltf_to_model`,
//! `png_to_texture`). The path's own order applies among the files found. A file that does not decode stops the game
//! with its name and the problem (`ESVerify`).
//!
//! The path and its files stay C++ (`FSPath::FindFile`, an `FSFileSpec` whose virtuals a memory file overrides):
//! every call on them is one host operation (`EeOfFileOp`, GEModel.cpp's `GEOpenFormatsHost`).

use std::ffi::{c_char, c_long, c_ulong, c_void, CStr};

use crate::ge::model_ffi::EeGeSharedFrame;
use crate::ge::model_store::{CemModel, SharedFrames};

use crate::openformats::gltf_to_model;
use ee_base::png::png_to_texture;
use ee_base::png::PngTexture;

/// The calls the lookups make on the path and its files. `obj` the path or a file (`FSFileSpec*`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeOfFileOp {
    /// `path->FindFile(UString((const char*)a), file)` -> the file (`new`) when found, else NULL.
    PathFind = 1,
    /// `file->Open(kFileAccessRead)` -> the result.
    FileOpen,
    /// `file->GetLength(*(unsigned long*)a)` -> the result.
    FileLength,
    /// `file->Read((unsigned char*)a, b, read)` -> the bytes read, or -1 when the read fails.
    FileRead,
    /// `file->Close()`.
    FileClose,
    /// `delete file`.
    FileDelete,
    /// `ESVerify(__FILE__, __LINE__, (const char*)a)`: a file that does not decode stops the game.
    Verify,
}

/// The host.
pub type EeOfFileHostFn = unsafe extern "C" fn(op: EeOfFileOp, obj: *mut c_void, a: isize, b: isize) -> isize;

const K_ERROR_OK: c_long = 0;
const K_ERROR_NOT_FOUND: c_long = 3;
const K_ERROR_FAILED: c_long = 9;
const K_FILE_ACCESS_READ: isize = 1;

struct Path {
    host: EeOfFileHostFn,
    path: *mut c_void,
}

impl Path {
    fn call(&self, op: EeOfFileOp, obj: *mut c_void, a: isize, b: isize) -> isize {
        // SAFETY: the host's contract (the path and its files are live C++ objects)
        unsafe { (self.host)(op, obj, a, b) }
    }

    /// A whole file's bytes; None when it cannot be read (it is closed after a successful open).
    fn read_whole(&self, file: *mut c_void) -> Option<Vec<u8>> {
        if self.call(EeOfFileOp::FileOpen, file, K_FILE_ACCESS_READ, 0) as c_long != K_ERROR_OK {
            return None;
        }
        let mut length: c_ulong = 0;
        let mut ok =
            self.call(EeOfFileOp::FileLength, file, &mut length as *mut c_ulong as isize, 0) as c_long == K_ERROR_OK;
        let mut data = Vec::new();
        if ok {
            data.resize(length as usize, 0);
            ok = length == 0
                || self.call(EeOfFileOp::FileRead, file, data.as_mut_ptr() as isize, length as isize)
                    == length as isize;
        }
        self.call(EeOfFileOp::FileClose, file, 0, 0);
        ok.then_some(data)
    }

    /// A file found on the path and read; None when it is not there (or cannot be read).
    fn read(&self, name: &[u8]) -> Option<Vec<u8>> {
        let mut c = name.to_vec();
        c.push(0);
        let file = self.call(EeOfFileOp::PathFind, self.path, c.as_ptr() as isize, 0) as *mut c_void;
        if file.is_null() {
            return None;
        }
        let data = self.read_whole(file);
        self.call(EeOfFileOp::FileDelete, file, 0, 0);
        data
    }

    /// A file that does not decode: the game stops with `<file>: <problem>`.
    fn failed(&self, file: &[u8], problem: &[u8]) -> c_long {
        let mut message = file.to_vec();
        message.extend_from_slice(b": ");
        message.extend_from_slice(problem);
        message.retain(|&b| b != 0);
        message.push(0);
        self.call(EeOfFileOp::Verify, std::ptr::null_mut(), message.as_ptr() as isize, 0);
        K_ERROR_FAILED
    }
}

/// "models\x" -> "models\" (the directory part, with its separator).
fn directory_of(name: &[u8]) -> &[u8] {
    match name.iter().rposition(|&b| b == b'\\' || b == b'/') {
        Some(i) => &name[..=i],
        None => &[],
    }
}

/// A model on the path: `<name>.glb`, else `<name>.gltf` (its external buffers relative to it), decoded with the
/// frames the model shares; `*out` the model (freed with `ee_ge_cem_free`) -> kErrorOK, kErrorNotFound when neither
/// file is there, kErrorFailed (after `ESVerify`) when the file does not convert.
///
/// # Safety
/// `host` serves `path`; `name` is a C string; `shared` holds `shared_count` entries (or is NULL when 0); `out` is
/// valid for writing.
#[no_mangle]
pub unsafe extern "C" fn ee_of_find_model(
    host: EeOfFileHostFn,
    path: *mut c_void,
    name: *const c_char,
    shared: *const EeGeSharedFrame,
    shared_count: usize,
    out: *mut *mut CemModel,
) -> c_long {
    let p = Path { host, path };
    // SAFETY: the caller's contract
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    let entries: Vec<(u32, i32)> = if shared_count == 0 || shared.is_null() {
        Vec::new()
    } else {
        // SAFETY: the caller's contract
        unsafe { std::slice::from_raw_parts(shared, shared_count) }.iter().map(|e| (e.frame, e.offset)).collect()
    };
    // SAFETY: the caller's contract
    unsafe { *out = std::ptr::null_mut() };
    for extension in [&b".glb"[..], b".gltf"] {
        let mut file = name.to_vec();
        file.extend_from_slice(extension);
        let Some(source) = p.read(&file) else { continue };
        let directory = directory_of(name);
        // a glTF's external buffer: its uri relative to the .gltf, with Windows separators
        let mut fetch = |uri: &[u8]| -> Option<Vec<u8>> {
            let mut buffer = directory.to_vec();
            buffer.extend(uri.iter().map(|&b| if b == b'/' { b'\\' } else { b }));
            p.read(&buffer)
        };
        return match gltf_to_model(&source, Some(&mut fetch), SharedFrames(&entries)) {
            Ok(model) => {
                // SAFETY: the caller's contract
                unsafe { *out = Box::into_raw(Box::new(model)) };
                K_ERROR_OK
            }
            Err(f) => p.failed(&file, &f.0),
        };
    }
    K_ERROR_NOT_FOUND
}

/// A texture on the path: `<name>.png` and its further frames `<name>@1.png`, ... (up to the first missing one),
/// decoded; `*out` the texture (freed with `ee_of_texture_free`) -> kErrorOK, kErrorNotFound when `<name>.png` is not
/// there, kErrorFailed (after `ESVerify`) when a frame does not decode.
///
/// # Safety
/// `host` serves `path`; `name` is a C string; `out` is valid for writing.
#[no_mangle]
pub unsafe extern "C" fn ee_of_find_texture(
    host: EeOfFileHostFn,
    path: *mut c_void,
    name: *const c_char,
    out: *mut *mut PngTexture,
) -> c_long {
    let p = Path { host, path };
    // SAFETY: the caller's contract
    let name = unsafe { CStr::from_ptr(name) }.to_bytes();
    // SAFETY: the caller's contract
    unsafe { *out = std::ptr::null_mut() };
    let mut first = name.to_vec();
    first.extend_from_slice(b".png");
    let Some(frame) = p.read(&first) else { return K_ERROR_NOT_FOUND };
    let mut frames = vec![frame];
    for i in 1u32.. {
        let mut file = name.to_vec();
        file.extend_from_slice(format!("@{i}.png").as_bytes());
        let Some(frame) = p.read(&file) else { break };
        frames.push(frame);
    }
    let slices: Vec<&[u8]> = frames.iter().map(|f| f.as_slice()).collect();
    match png_to_texture(&slices) {
        Ok(texture) => {
            // SAFETY: the caller's contract
            unsafe { *out = Box::into_raw(Box::new(texture)) };
            K_ERROR_OK
        }
        Err(f) => p.failed(&first, &f.0),
    }
}

#[cfg(test)]
#[path = "find_tests.rs"]
mod tests;
