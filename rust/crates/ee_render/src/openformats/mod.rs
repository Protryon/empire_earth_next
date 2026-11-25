//! The game's models and textures (docs/MODEL_FORMATS.md, docs/RUST_PLAN.md R2.4 and D9): a glTF 2.0 model (.gltf/.glb)
//! -> the model `GEModel::StoreLoaded` stores, and PNG frames -> the frames `GETexture::Load` stores (and one PNG ->
//! the Targa a picture read as data gets). The Rust side of GEOpenFormats.cpp and its vendored readers (cgltf,
//! stb_image): the gate is `eemodel.py cppgate`, the stored models and textures equal to digests recorded from the
//! loaders before D9 (the .cem / .sst bytes and their readers, retired), and each case loaded or refused as the
//! Python converter decides.
//!
//! The files are read by crates (docs/progress/deps.md): the glTF document by `gltf` (its GLB container and its
//! serde structures) and `serde_json`, a PNG by `png` (ee_base png.rs). What the game needs past them is ours:
//! `document.rs` (the references checked, the buffers, the accessors read with every bound checked: a loader of edited
//! files must refuse, never panic), and the pixels as stb_image gave them (16-bit, palette, tRNS). `import.rs`
//! follows GEOpenFormats.cpp. The refusals of the gate's edited models are import.rs's and unchanged.

mod document;
mod ffi;
pub mod find;
mod import;
mod json;
pub mod surface_file;

pub use ee_base::png::{png_to_picture, png_to_texture, Picture, PngFrame, PngTexture};
pub use ffi::*;
pub use import::{gltf_to_model, Resolver};

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
