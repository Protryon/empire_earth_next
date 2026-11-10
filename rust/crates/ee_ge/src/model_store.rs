//! The model `GEModel::StoreLoaded` stores ('GEModel Load.cpp'): the counts, names, polygon lists, material ranges,
//! frames and sub-models, as the original's `.cem` chunk readers left them. The game's models are glTF files only
//! (docs/RUST_PLAN.md D9): `ee_openformats` decodes one into a [`CemModel`] (`ee_of_gltf_to_model`), and the C++
//! walks it through `model_ffi`'s views. The `.cem` reader that filled it before D9 is gone.
//!
//! The lists keep the reader's shape: a model the reader refused has no counts and stores nothing, and a material's
//! values come in the order the C++ sets them.

use std::ffi::c_long;

use crate::types::{Ge3DPoint, GeVector};

/// The polygon, vertex, tag point, material, frame, sub-model and LOD counts (`GEModel`'s members).
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Counts {
    pub polygon: u32,
    pub vertex: u32,
    pub tag_point: u32,
    pub material: u32,
    pub frame: u32,
    pub sub_model: u32,
    pub lod: u32,
}

/// The frames a model shares with a neighbour (`ioTextures`, a `std::map<unsigned long, long>` from frame to offset),
/// sorted by frame.
#[derive(Clone, Copy, Debug, Default)]
pub struct SharedFrames<'a>(pub &'a [(u32, i32)]);

impl SharedFrames<'_> {
    pub fn get(&self, frame: u32) -> Option<i32> {
        self.0.binary_search_by_key(&frame, |e| e.0).ok().map(|i| self.0[i].1)
    }
}

/// One material: its name, then the values in the order the C++ sets them (texture index, the polygon start and
/// length of each LOD, the vertex start, the vertex length), and the texture file name (`mTextureFilenames[i]`).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemMaterial {
    pub name: Vec<u8>,
    pub values: Vec<u32>,
    pub texture_filename: Vec<u8>,
}

/// One LOD's polygons: the array LOD 0 gets from the allocation, a later LOD's from its count (`created`); the
/// vertex indices set, three per polygon.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemPolygons {
    pub created: Option<u32>,
    pub indices: Vec<u32>,
}

/// One frame. The vertices and tag points go into the arrays of frame `target` (the last frame for a frame the
/// model shares, where the original's `LoadFrameChunk` put them); the radius, transform (16 elements, row by row)
/// and box (the two corners) are the frame's own; the box's dimensions follow when all six values are there.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemFrame {
    pub radius: Option<f32>,
    pub target: u32,
    pub points: Vec<Ge3DPoint>,
    pub normals: Vec<GeVector>,
    pub uvs: Vec<[f32; 2]>,
    pub tag_points: Vec<Ge3DPoint>,
    pub transform: Vec<f32>,
    pub bbox: Vec<f32>,
}

/// A model as `GEModel::StoreLoaded` takes it.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CemModel {
    /// The model's result (`kErrorOK` = 0).
    pub result: c_long,
    /// The counts the model's arrays are allocated with (`AllocateInternalData`); none for a refused model.
    pub counts: Option<Counts>,
    pub name: Vec<u8>,
    /// The centre point (x, y, z).
    pub center: Vec<f32>,
    pub lods: Vec<CemPolygons>,
    pub materials: Vec<CemMaterial>,
    pub tag_point_names: Vec<Vec<u8>>,
    pub frames: Vec<CemFrame>,
    /// The sub-models (each with its own result).
    pub sub_models: Vec<CemModel>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn shared_frames_are_looked_up_by_frame() {
        let entries = [(1, -1), (3, -1), (7, 2)];
        let s = SharedFrames(&entries);
        assert_eq!(s.get(1), Some(-1));
        assert_eq!(s.get(7), Some(2));
        assert_eq!(s.get(0), None);
        assert_eq!(s.get(4), None);
        assert_eq!(SharedFrames::default().get(0), None);
    }

    #[test]
    fn a_refused_model_stores_nothing() {
        let m = CemModel { result: 9, ..CemModel::default() };
        assert!(m.counts.is_none() && m.frames.is_empty() && m.sub_models.is_empty());
    }
}
