//! `GEModel::Save`'s chunks against golden values from the real C++ (`golden_model_save.txt`). The scratch driver of
//! `golden_model.rs` loaded 600 synthetic models (its generator, seeded 13) with the original C++, marked each as a
//! sub-model (so `Save` writes it as loaded, without a top model's preparation, which `golden_model_geom.rs` pins)
//! and saved it into a memory file whole and into files that refuse writes ending past three random lengths,
//! printing the result, the position, the length and a digest of the bytes. This test reads each model with
//! `model_file::load`, lays it out as the C++ holds it and saves it with `model_save::save` (the sub-models after
//! it, each put back on failure, as `GEModel::Save` does). The driver relinked over the forwarders printed the
//! same lines, and so did it over every model of the game data, top-model preparation included.

use std::ffi::c_long;

use crate::golden_model::{Fnv, Gen};
use crate::io::{BufferIo, FieldIo, OK};
use crate::model_file::{load, CemModel, SharedFrames};
use crate::model_save::{save, SaveFrame, SaveMaterial, SaveModel};
use crate::types::{Ge3DPoint, GeTransformation};

const GOLDEN: &str = include_str!("golden_model_save.txt");

/// A loaded model as the C++ holds it (no shared frames).
fn held(m: &CemModel) -> (SaveModel, Vec<SaveModel>) {
    let c = m.counts.unwrap();
    let lod = c.lod as usize;
    let lods = m
        .lods
        .iter()
        .enumerate()
        .map(|(i, l)| {
            let size = if i == 0 { c.polygon } else { l.created.unwrap_or(0) } as usize;
            let mut indices = vec![0u32; size * 3];
            for (k, &v) in l.indices.iter().enumerate() {
                if let Some(slot) = indices.get_mut(k) {
                    *slot = v;
                }
            }
            indices
        })
        .collect();
    let materials = m
        .materials
        .iter()
        .map(|t| {
            let value = |k: usize| t.values.get(k).copied().unwrap_or(0);
            SaveMaterial {
                name: t.name.clone(),
                texture_index: value(0),
                ranges: (0..lod).map(|j| (value(1 + 2 * j), value(2 + 2 * j))).collect(),
                vertex_start: value(1 + 2 * lod),
                vertex_length: value(2 + 2 * lod),
                texture_filename: t.texture_filename.clone(),
            }
        })
        .collect();
    let frames = m
        .frames
        .iter()
        .map(|f| {
            let mut transform = GeTransformation::IDENTITY;
            for (k, &v) in f.transform.iter().enumerate() {
                transform.m[k / 4][k % 4] = v;
            }
            // GEBoundingBox(): (0, 0, 0) to (1, 1, 1), then the corner values read
            let mut b = [0.0, 0.0, 0.0, 1.0, 1.0, 1.0];
            b[..f.bbox.len()].copy_from_slice(&f.bbox);
            SaveFrame {
                radius: f.radius.unwrap_or(0.0),
                points: f.points.clone(),
                normals: f.normals.clone(),
                uvs: f.uvs.clone(),
                tag_points: f.tag_points.clone(),
                transform,
                bbox: [Ge3DPoint::new(b[0], b[1], b[2]), Ge3DPoint::new(b[3], b[4], b[5])],
            }
        })
        .collect();
    let model = SaveModel {
        counts: [c.polygon, c.vertex, c.tag_point, c.material, c.frame, c.sub_model, c.lod],
        name: m.name.clone(),
        center: Ge3DPoint::new(m.center[0], m.center[1], m.center[2]),
        lods,
        materials,
        tag_point_names: m.tag_point_names.clone(),
        frames,
    };
    let subs = m.sub_models.iter().map(|s| held(s).0).collect();
    (model, subs)
}

/// `GEModel::Save` of a model with a parent: the chunks, put back on failure; its sub-models after success.
fn save_model(io: &mut BufferIo, model: &SaveModel, subs: &[SaveModel]) -> c_long {
    let mut position: c_long = 0;
    let r = io.get_position(&mut position);
    if r != OK {
        return r;
    }
    let r = save(io, model);
    if r == OK {
        for s in subs {
            save_model(io, s, &[]);
        }
    } else {
        io.set_position(position);
    }
    r
}

#[test]
fn saves_equal_the_cpp() {
    let mut g = Gen::new(13);
    let mut lines = Vec::new();
    for i in 0..600 {
        let bytes = g.model_bytes();
        let n = bytes.len() as u32;
        let limits = [u32::MAX, g.r(n), g.r(n), g.r(n)];
        let mut io = BufferIo::reader(bytes);
        let m = load(&mut io, SharedFrames(&[]));
        assert_eq!(m.result, OK);
        let (model, subs) = held(&m);
        for limit in limits {
            let mut out = BufferIo { write_limit: limit as usize, ..BufferIo::writer() };
            let r = save_model(&mut out, &model, &subs);
            let mut d = Fnv::new();
            d.b(&out.bytes);
            lines.push(format!("save{i}<{limit} r{:x} pos{} len{} {:016x}", r, out.position, out.bytes.len(), d.0));
        }
    }
    if let Ok(path) = std::env::var("EE_GE_MODEL_SAVE_LINES") {
        let text = std::fs::read_to_string(path).unwrap();
        for (k, (a, b)) in text.lines().zip(&lines).enumerate() {
            assert_eq!(a, b, "case {k}");
        }
    }
    let mut d = Fnv::new();
    for l in &lines {
        d.b(l.as_bytes());
        d.b(b"\n");
    }
    let ok = lines.iter().filter(|l| l.contains(" r0 ")).count();
    let mut want = format!("# model_save {} ok {} {:016x}\n", lines.len(), ok, d.0);
    for l in lines.iter().take(4) {
        want += l;
        want += "\n";
    }
    assert_eq!(want, GOLDEN.replace("\r\n", "\n"));
}
