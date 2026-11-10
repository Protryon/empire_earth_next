//! The game's models and textures (docs/MODEL_FORMATS.md, docs/RUST_PLAN.md R2.4 and D9): a glTF 2.0 model (.gltf/.glb)
//! -> the model `GEModel::StoreLoaded` stores, and PNG frames -> the frames `GETexture::Load` stores (and one PNG ->
//! the Targa a picture read as data gets). The Rust side of GEOpenFormats.cpp and its vendored readers (cgltf,
//! stb_image): the gate is `eemodel.py cppgate`, the stored models and textures equal to digests recorded from the
//! loaders before D9 (the .cem / .sst bytes and their readers, retired), and each case loaded or refused as the
//! Python converter decides.
//!
//! The readers are hand ports, not the `gltf` and `png` crates: cgltf's and stb_image's acceptance rules decide which
//! edited files load, and the gate compares them case by case. `cgltf.rs`, `jsmn.rs` and `png.rs` follow those
//! libraries (including a file's walk order and its failure points); `import.rs` and `json.rs` follow GEOpenFormats.cpp.

mod cgltf;
mod ffi;
mod import;
mod jsmn;
mod json;
mod png;

pub use ffi::*;
pub use import::{gltf_to_model, Resolver};
pub use png::{frame_to_tga, png_to_texture, png_to_tga, PngFrame, PngTexture};

/// Why a conversion failed: the message (bytes, as names in it may not be UTF-8).
#[derive(Debug, PartialEq, Eq)]
pub struct Failure(pub Vec<u8>);

pub(crate) fn fail<T>(message: &str) -> Result<T, Failure> {
    Err(Failure(message.as_bytes().to_vec()))
}

#[cfg(test)]
mod tests {
    /// Standard base64 for test documents' data URIs (with padding omitted, as cgltf reads exactly the declared size).
    pub(crate) fn base64(data: &[u8]) -> String {
        const ALPHABET: &[u8; 64] = b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        let mut out = String::new();
        for chunk in data.chunks(3) {
            let n = (chunk[0] as u32) << 16
                | (chunk.get(1).copied().unwrap_or(0) as u32) << 8
                | chunk.get(2).copied().unwrap_or(0) as u32;
            out.push(ALPHABET[(n >> 18) as usize & 63] as char);
            out.push(ALPHABET[(n >> 12) as usize & 63] as char);
            if chunk.len() > 1 {
                out.push(ALPHABET[(n >> 6) as usize & 63] as char);
            }
            if chunk.len() > 2 {
                out.push(ALPHABET[n as usize & 63] as char);
            }
        }
        out
    }
}
