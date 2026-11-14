//! The C ABI of the model `GEModel::StoreLoaded` stores (`model_store.rs`; the Rust model, `modobj.rs`, reads it).
//! `ee_openformats` decodes a glTF into a [`CemModel`] (`ee_of_gltf_to_model`, sub-models included), which the C++
//! hands to the model (opaque to it as `EeGeCemModel`) and frees with `ee_ge_cem_free`. The views the C++ walked it
//! through and the geometry's wrappers went with the C++ `GEModel` (R4.3, ninth part).

use crate::model_store::CemModel;

/// One entry of the shared frames map (`std::map<unsigned long, long>`: frame, offset to the frame it shares).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeGeSharedFrame {
    pub frame: u32,
    pub offset: i32,
}

/// Frees a model (`ee_of_gltf_to_model`'s; not a sub-model's handle).
///
/// # Safety
/// `model` came from `ee_of_gltf_to_model` and is freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_ge_cem_free(model: *mut CemModel) {
    if !model.is_null() {
        // SAFETY: the caller's contract
        drop(unsafe { Box::from_raw(model) });
    }
}
