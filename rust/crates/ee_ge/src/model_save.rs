//! `GEModel::Save`'s chunk writers ('GEModel Save.cpp'): the `.cem` 2.0 header, counts, name and centre, every
//! LOD's polygons, the materials, the tag point names and each frame's radius, vertices, tag points, transform and
//! box, in the C++'s order, through the file's own typed writes, with its stop-at-the-first-failure rules (and its
//! quirks: a short tag write still succeeds; a failed LOD does not stop the next LOD's count). The C++ keeps the
//! preparation of a top model (bounds, tag point order, scale removal), the rewind on failure and the sub-models.

use std::ffi::c_long;

use crate::io::{FieldIo, FAILED, OK};
use crate::types::{Ge3DPoint, GeTransformation, GeVector};

/// One material as `SaveMaterialsChunk` writes it.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SaveMaterial {
    pub name: Vec<u8>,
    pub texture_index: u32,
    /// Per LOD, the polygon start and length.
    pub ranges: Vec<(u32, u32)>,
    pub vertex_start: u32,
    pub vertex_length: u32,
    pub texture_filename: Vec<u8>,
}

/// One frame as `SaveFrameChunk` writes it.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SaveFrame {
    pub radius: f32,
    pub points: Vec<Ge3DPoint>,
    pub normals: Vec<GeVector>,
    pub uvs: Vec<[f32; 2]>,
    pub tag_points: Vec<Ge3DPoint>,
    pub transform: GeTransformation,
    /// The box's two corners.
    pub bbox: [Ge3DPoint; 2],
}

/// A model's own data as `GEModel::Save` writes it (its sub-models follow, each saved on its own).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct SaveModel {
    /// Polygons, vertices, tag points, materials, frames, sub-models, LODs.
    pub counts: [u32; 7],
    pub name: Vec<u8>,
    pub center: Ge3DPoint,
    /// Per LOD, the vertex indices (three per polygon; the count written is the array's size).
    pub lods: Vec<Vec<u32>>,
    pub materials: Vec<SaveMaterial>,
    pub tag_point_names: Vec<Vec<u8>>,
    pub frames: Vec<SaveFrame>,
}

/// `UString::Save`: the length with its NUL, then the text and the NUL; a short write is `kErrorFailed`.
pub fn save_string(io: &mut dyn FieldIo, text: &[u8]) -> c_long {
    let length = text.len() as u32 + 1;
    let r = io.write_u32(length);
    if r != OK {
        return r;
    }
    let mut bytes = text.to_vec();
    bytes.push(0);
    let mut written = 0u32;
    let r = io.write_bytes(&bytes, &mut written);
    if r == OK && written < length {
        return FAILED;
    }
    r
}

fn save_floats(io: &mut dyn FieldIo, values: &[f32]) -> c_long {
    for &v in values {
        let r = io.write_f32(v);
        if r != OK {
            return r;
        }
    }
    OK
}

fn save_point(io: &mut dyn FieldIo, p: Ge3DPoint) -> c_long {
    save_floats(io, &[p.x, p.y, p.z])
}

/// `GETextureCoordinate::Save`: u, then v; a failure puts the file back where the coordinate started.
fn save_texture_coordinate(io: &mut dyn FieldIo, uv: [f32; 2]) -> c_long {
    let mut position: c_long = 0;
    let mut r = io.get_position(&mut position);
    if r == OK {
        r = io.write_f32(uv[0]);
        if r == OK {
            r = io.write_f32(uv[1]);
        }
        if r != OK {
            io.set_position(position);
        }
    }
    r
}

/// The chunks of `GEModel::Save` (header to frames); the first failure's result, or `kErrorOK`.
pub fn save(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    for chunk in
        [save_header, save_counts, save_miscellaneous, save_polygons, save_materials, save_tag_point_names, save_frames]
    {
        let r = chunk(io, model);
        if r != OK {
            return r;
        }
    }
    OK
}

/// `SaveHeaderChunk`: "SSMF" and version 2.0; a short tag write returns the write's own success.
fn save_header(io: &mut dyn FieldIo, _: &SaveModel) -> c_long {
    let mut written = 0u32;
    let r = io.write_bytes(b"SSMF", &mut written);
    if r != OK || written != 4 {
        return r;
    }
    let r = io.write_u16(2);
    if r != OK {
        return r;
    }
    io.write_u16(0)
}

fn save_counts(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    for &c in &model.counts {
        let r = io.write_u32(c);
        if r != OK {
            return r;
        }
    }
    OK
}

fn save_miscellaneous(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    let r = save_string(io, &model.name);
    if r != OK {
        return r;
    }
    save_point(io, model.center)
}

/// `SavePolygonsChunk`: per LOD the count, then three indices per polygon; a failure stops that LOD only.
fn save_polygons(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    let mut result = OK;
    for indices in &model.lods {
        result = io.write_u32((indices.len() / 3) as u32);
        if result != OK {
            continue;
        }
        'polygons: for t in indices.as_chunks::<3>().0 {
            for &i in t {
                result = io.write_u32(i);
                if result != OK {
                    break 'polygons;
                }
            }
        }
    }
    result
}

/// `SaveMaterialsChunk`: each material's name, texture index, ranges per LOD, vertex range and texture file name.
fn save_materials(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    let mut result = OK;
    for m in &model.materials {
        result = save_string(io, &m.name);
        if result != OK {
            break;
        }
        result = io.write_u32(m.texture_index);
        if result != OK {
            break;
        }
        for &(start, length) in &m.ranges {
            result = io.write_u32(start);
            if result == OK {
                result = io.write_u32(length);
            }
            if result != OK {
                break;
            }
        }
        if result != OK {
            break;
        }
        result = io.write_u32(m.vertex_start);
        if result != OK {
            break;
        }
        result = io.write_u32(m.vertex_length);
        if result == OK {
            result = save_string(io, &m.texture_filename);
        }
        if result != OK {
            break;
        }
    }
    result
}

fn save_tag_point_names(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    for name in &model.tag_point_names {
        let r = save_string(io, name);
        if r != OK {
            return r;
        }
    }
    OK
}

/// `SaveFrameChunk`: per frame the radius, the vertices (point, normal, texture coordinate), the tag points, the
/// transform (16 values row by row) and the box's corners.
fn save_frames(io: &mut dyn FieldIo, model: &SaveModel) -> c_long {
    for f in &model.frames {
        let r = io.write_f32(f.radius);
        if r != OK {
            return r;
        }
        for ((p, n), uv) in f.points.iter().zip(&f.normals).zip(&f.uvs) {
            let mut r = save_point(io, *p);
            if r == OK {
                r = save_floats(io, &[n.x, n.y, n.z]);
            }
            if r == OK {
                r = save_texture_coordinate(io, *uv);
            }
            if r != OK {
                return r;
            }
        }
        for &p in &f.tag_points {
            let r = save_point(io, p);
            if r != OK {
                return r;
            }
        }
        let r = save_floats(io, f.transform.m.as_flattened());
        if r != OK {
            return r;
        }
        let r = save_point(io, f.bbox[0]);
        if r != OK {
            return r;
        }
        let r = save_point(io, f.bbox[1]);
        if r != OK {
            return r;
        }
    }
    OK
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::io::BufferIo;
    use crate::model_file::{load, tests::current, SharedFrames};

    /// The SaveModel of what `load` read (a model whose arrays hold exactly what the file held).
    fn from_file(bytes: &[u8]) -> SaveModel {
        let mut io = BufferIo::reader(bytes.to_vec());
        let m = load(&mut io, SharedFrames(&[]));
        assert_eq!(m.result, OK);
        let c = m.counts.unwrap();
        let lod = c.lod as usize;
        SaveModel {
            counts: [c.polygon, c.vertex, c.tag_point, c.material, c.frame, 0, c.lod],
            name: m.name.clone(),
            center: Ge3DPoint::new(m.center[0], m.center[1], m.center[2]),
            lods: m.lods.iter().map(|l| l.indices.clone()).collect(),
            materials: m
                .materials
                .iter()
                .map(|t| SaveMaterial {
                    name: t.name.clone(),
                    texture_index: t.values[0],
                    ranges: (0..lod).map(|j| (t.values[1 + 2 * j], t.values[2 + 2 * j])).collect(),
                    vertex_start: t.values[1 + 2 * lod],
                    vertex_length: t.values[2 + 2 * lod],
                    texture_filename: t.texture_filename.clone(),
                })
                .collect(),
            tag_point_names: m.tag_point_names.clone(),
            frames: m
                .frames
                .iter()
                .map(|f| {
                    let mut transform = GeTransformation::default();
                    for (k, &v) in f.transform.iter().enumerate() {
                        transform.m[k / 4][k % 4] = v;
                    }
                    let b = &f.bbox;
                    SaveFrame {
                        radius: f.radius.unwrap(),
                        points: f.points.clone(),
                        normals: f.normals.clone(),
                        uvs: f.uvs.clone(),
                        tag_points: f.tag_points.clone(),
                        transform,
                        bbox: [Ge3DPoint::new(b[0], b[1], b[2]), Ge3DPoint::new(b[3], b[4], b[5])],
                    }
                })
                .collect(),
        }
    }

    #[test]
    fn a_saved_model_is_the_file_it_came_from() {
        let bytes = current(2, 0).0;
        let model = from_file(&bytes);
        let mut out = BufferIo::writer();
        assert_eq!(save(&mut out, &model), OK);
        assert_eq!(out.bytes, bytes);
    }

    #[test]
    fn a_failed_write_stops_the_model_where_the_cpp_stops() {
        let bytes = current(1, 0).0;
        let model = from_file(&bytes);
        for limit in 0..bytes.len() {
            let mut out = BufferIo { write_limit: limit, ..BufferIo::writer() };
            let r = save(&mut out, &model);
            if limit < 4 {
                // the short tag write: the write failed outright, so its result comes back
                assert_eq!(r, FAILED);
                continue;
            }
            assert_eq!(r, FAILED, "limit {limit}");
            // everything written is the file's prefix (a failed texture coordinate puts the position back)
            assert!(bytes.starts_with(&out.bytes[..out.bytes.len().min(limit)]), "limit {limit}");
        }
    }

    #[test]
    fn strings_carry_their_nul() {
        let mut out = BufferIo::writer();
        assert_eq!(save_string(&mut out, b"ab"), OK);
        assert_eq!(out.bytes, vec![3, 0, 0, 0, b'a', b'b', 0]);
        let mut out = BufferIo { write_limit: 6, ..BufferIo::writer() };
        assert_eq!(save_string(&mut out, b"ab"), FAILED);
    }

    #[test]
    fn a_failed_lod_does_not_stop_the_next_count() {
        let model = SaveModel { lods: vec![vec![1, 2, 3], vec![4, 5, 6]], ..SaveModel::default() };
        // room for the first count and one index: the second LOD's count then fails too
        let mut out = BufferIo { write_limit: 8, ..BufferIo::writer() };
        assert_eq!(save_polygons(&mut out, &model), FAILED);
        assert_eq!(out.bytes, vec![1, 0, 0, 0, 1, 0, 0, 0]);
    }
}
