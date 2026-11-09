//! `GEModel::ConvertLegacyModel` ('GEModel Legacy.cpp'): a version-1 model (points shared by vertices, a texture
//! coordinate per polygon corner, materials and "player color" polygon groups as polygon lists) becomes the current
//! layout: a vertex is split for every other texture coordinate its corners use, the polygons are regrouped by
//! material (a material only partly in a "player color" group is split in two), and the vertices are regrouped by
//! the materials that use them.
//!
//! The steps, their loops and their quirks are the C++'s: a vertex used by two materials is copied into both, a
//! polygon listed twice is placed twice, the group names are the materials' names (which version 1 never sets) or
//! "player color". Indices past an array's end were undefined behaviour in the C++ (reads of whatever was there,
//! writes past the end); here such a read gives 0 and such a write is dropped.

use crate::model_file::{CemModel, CemPolygons, SharedFrames, PLAYER_COLOR};
use crate::types::{Ge3DPoint, GeVector};

/// A version-1 polygon: per corner a texture coordinate and a vertex index.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LegacyPolygon {
    pub uvs: [[f32; 2]; 3],
    pub vertices: [u32; 3],
}

/// The version-1 data the loader reads (`GEModel`'s `mLegacy*` members).
#[derive(Clone, Debug, Default, PartialEq)]
pub struct LegacyModel {
    pub polygons: Vec<LegacyPolygon>,
    /// Per frame, the points.
    pub points: Vec<Vec<Ge3DPoint>>,
    /// Per frame, the vertex normals.
    pub normals: Vec<Vec<GeVector>>,
    /// Per material, its polygons.
    pub materials: Vec<Vec<u32>>,
    /// Per polygon group, its polygons (kept only for "player color" groups).
    pub groups: Vec<Vec<u32>>,
    /// Per vertex, its point.
    pub vertex_points: Vec<u32>,
}

#[derive(Clone, Copy, Debug, Default)]
struct Vertex {
    point: Ge3DPoint,
    normal: GeVector,
    uv: [f32; 2],
}

#[derive(Clone, Debug, Default)]
struct Group {
    name: Vec<u8>,
    texture_filename: Vec<u8>,
    polygons: Vec<u32>,
    texture_index: u32,
}

/// A split vertex: its texture coordinate and the (polygon, corner) pairs that use it.
type Split = ([f32; 2], Vec<(usize, usize)>);

/// `GETextureCoordinate::operator==`.
fn same_uv(a: [f32; 2], b: [f32; 2]) -> bool {
    a[0] == b[0] && a[1] == b[1]
}

/// Converts a loaded version-1 model in place: `model` holds the counts the version-1 data was allocated with,
/// its materials' names and texture file names, and the frames read; it leaves with the converted counts,
/// materials, LOD 0 polygons and the vertices of every frame the model does not share.
pub fn convert(legacy: &LegacyModel, shared: SharedFrames, model: &mut CemModel) {
    let mut counts = model.counts.expect("a converted model was allocated");
    let frame_count = counts.frame as usize;
    let polygon_count = counts.polygon as usize;
    let vertex_count = legacy.vertex_points.len();
    let physical = shared.physical_frames(counts.frame);
    let is_shared = |f: usize| shared.get(f as u32).is_some();

    // the polygons into LOD 0, the points, normals and corner texture coordinates into the frames' arrays
    let mut polygons: Vec<[u32; 3]> = legacy.polygons.iter().map(|p| p.vertices).collect();
    let mut arrays: Vec<Vec<Vertex>> = vec![vec![Vertex::default(); vertex_count]; frame_count];
    for f in 0..frame_count {
        if is_shared(f) {
            continue;
        }
        let array = &mut arrays[f];
        for (m, vertex) in array.iter_mut().enumerate() {
            vertex.point = legacy.points[f].get(legacy.vertex_points[m] as usize).copied().unwrap_or_default();
            vertex.normal = legacy.normals[f][m];
        }
        for p in &legacy.polygons {
            for k in 0..3 {
                if let Some(vertex) = array.get_mut(p.vertices[k] as usize) {
                    vertex.uv = p.uvs[k];
                }
            }
        }
    }

    // a vertex whose corners disagree on the texture coordinate is split, one new vertex per other coordinate
    let mut new_vertices: Vec<([f32; 2], usize)> = Vec::new();
    let first = physical.first().copied().unwrap_or(0);
    for i in 0..vertex_count {
        let coordinate = arrays.get(first).and_then(|a| a.get(i)).map_or([0.0; 2], |v| v.uv);
        let mut splits: Vec<Split> = Vec::new();
        for (j, p) in legacy.polygons.iter().enumerate() {
            let Some(k) = (0..3).find(|&k| p.vertices[k] == i as u32) else {
                continue;
            };
            if same_uv(p.uvs[k], coordinate) {
                continue;
            }
            match splits.iter().position(|s| same_uv(p.uvs[k], s.0)) {
                Some(l) => splits[l].1.push((j, k)),
                None => splits.push((p.uvs[k], vec![(j, k)])),
            }
        }
        for (uv, corners) in splits {
            new_vertices.push((uv, i));
            let index = (new_vertices.len() + vertex_count - 1) as u32;
            for (j, k) in corners {
                if let Some(p) = polygons.get_mut(j) {
                    p[k] = index;
                }
            }
        }
    }
    let split_count = new_vertices.len() + vertex_count;
    for f in 0..frame_count {
        if is_shared(f) {
            continue;
        }
        let old = &arrays[f];
        let mut array = old.clone();
        for &(uv, from) in &new_vertices {
            let v = old[from];
            array.push(Vertex { point: v.point, normal: v.normal, uv });
        }
        arrays[f] = array;
    }

    // one material per legacy material with polygons, split in two where only some take the player colour
    let mut groups: Vec<Group> = Vec::new();
    let mut texture_index = 0u32;
    let in_groups = |polygon: u32| legacy.groups.iter().flatten().filter(|&&g| g == polygon).count();
    for (i, indices) in legacy.materials.iter().enumerate() {
        if indices.is_empty() {
            continue;
        }
        let material = &model.materials[i];
        let group = Group {
            name: material.name.clone(),
            texture_filename: material.texture_filename.clone(),
            polygons: Vec::new(),
            texture_index,
        };
        texture_index = texture_index.wrapping_add(1);
        groups.push(group.clone());
        let count: usize = indices.iter().map(|&p| in_groups(p)).sum();
        if count == 0 {
            groups.last_mut().unwrap().polygons.extend_from_slice(indices);
        } else if count as u32 == indices.len() as u32 {
            let last = groups.last_mut().unwrap();
            last.name = PLAYER_COLOR.to_vec();
            last.polygons.extend_from_slice(indices);
        } else {
            groups.push(Group { name: PLAYER_COLOR.to_vec(), ..group });
            let n = groups.len();
            for &p in indices {
                let found = in_groups(p);
                for _ in 0..found {
                    groups[n - 1].polygons.push(p);
                }
                if found == 0 {
                    groups[n - 2].polygons.push(p);
                }
            }
        }
    }

    // the polygons reordered by material
    let indices = polygons.clone();
    let mut l = 0usize;
    for g in &groups {
        for &p in &g.polygons {
            let value = indices.get(p as usize).copied().unwrap_or_default();
            if let Some(slot) = polygons.get_mut(l) {
                *slot = value;
            }
            l += 1;
        }
    }

    // and the vertices by the materials that use them (a vertex once per material)
    let mut vertex_groups: Vec<Vec<u32>> = vec![Vec::new(); groups.len()];
    for i in 0..split_count as u32 {
        for (k, g) in groups.iter().enumerate() {
            if g.polygons.iter().any(|&p| indices.get(p as usize).copied().unwrap_or_default().contains(&i)) {
                vertex_groups[k].push(i);
            }
        }
    }
    let final_count: usize = vertex_groups.iter().map(|g| g.len()).sum();
    for f in 0..frame_count {
        if is_shared(f) {
            continue;
        }
        let old = &arrays[f];
        arrays[f] = vertex_groups.iter().flatten().map(|&v| old[v as usize]).collect();
    }

    // the polygons' indices into their material's vertex range
    let indices = polygons.clone();
    let mut l = 0usize;
    for (g, vertices) in groups.iter().zip(&vertex_groups) {
        for (k, &vertex) in vertices.iter().enumerate() {
            for j in 0..g.polygons.len() {
                let Some(old) = indices.get(l + j) else {
                    continue;
                };
                let corner = if old[0] == vertex {
                    0
                } else if old[1] == vertex {
                    1
                } else if old[2] == vertex {
                    2
                } else {
                    continue;
                };
                polygons[l + j][corner] = k as u32;
            }
        }
        l += g.polygons.len();
    }

    // the converted model
    counts.vertex = final_count as u32;
    counts.material = groups.len() as u32;
    model.counts = Some(counts);
    let (mut polygon_start, mut vertex_start) = (0u32, 0u32);
    model.materials = groups
        .iter()
        .zip(&vertex_groups)
        .map(|(g, vertices)| {
            let values =
                vec![g.texture_index, polygon_start, g.polygons.len() as u32, vertex_start, vertices.len() as u32];
            polygon_start = polygon_start.wrapping_add(g.polygons.len() as u32);
            vertex_start = vertex_start.wrapping_add(vertices.len() as u32);
            crate::model_file::CemMaterial {
                name: g.name.clone(),
                values,
                texture_filename: g.texture_filename.clone(),
            }
        })
        .collect();
    debug_assert_eq!(polygons.len(), polygon_count);
    model.lods = vec![CemPolygons { created: None, indices: polygons.iter().flatten().copied().collect() }];
    for f in 0..frame_count {
        if is_shared(f) {
            continue;
        }
        let frame = &mut model.frames[f];
        frame.points = arrays[f].iter().map(|v| v.point).collect();
        frame.normals = arrays[f].iter().map(|v| v.normal).collect();
        frame.uvs = arrays[f].iter().map(|v| v.uv).collect();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::io::BufferIo;
    use crate::model_file::tests::{Cem, IDENTITY};
    use crate::model_file::{load, CemFrame, CemMaterial, Counts};

    fn corner(c: &mut Cem, vertex: u32, uv: [f32; 2]) {
        c.u32(vertex).fs(&uv).fs(&[1.0, 1.0, 1.0]).fs(&[1.0, 0.5, 0.5, 0.0]);
    }

    /// A 1.3 model: four vertices on three points, two triangles, vertex 0's corners on two texture coordinates;
    /// material 0 holds triangle 0 (textured), material 1 triangle 1, which the "player color" group holds.
    fn legacy_13() -> Cem {
        let mut c = Cem::header(1, 3);
        c.u32(1).u32(2).u32(3).u32(2).u32(1).u32(4).u32(0).u32(0);
        c.str("old").fs(&[0.0, 0.0, 0.0]).u8(0).u32(0).u32(0).u32(0);
        corner(&mut c, 0, [0.0, 0.0]);
        corner(&mut c, 1, [1.0, 0.0]);
        corner(&mut c, 2, [0.0, 1.0]);
        corner(&mut c, 0, [0.5, 0.5]);
        corner(&mut c, 2, [0.0, 1.0]);
        corner(&mut c, 3, [1.0, 1.0]);
        c.str("player color").u32(1).u32(1);
        c.u32(1).u32(0).u8(1).str("a.tga").u32(0);
        c.u32(1).u32(1).u8(0);
        for p in [0, 1, 2, 2] {
            c.u32(p).f32(0.0);
        }
        c.f32(7.0).fs(&[0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0]);
        c.u16(0).u16(0).u16(0).u16(0xffff);
        c.fs(&IDENTITY).fs(&[0.0, 0.0, 0.0, 1.0, 1.0, 1.0]);
        c
    }

    #[test]
    fn a_legacy_model_is_converted() {
        let bytes = legacy_13().0;
        let n = bytes.len();
        let mut io = BufferIo::reader(bytes);
        let m = load(&mut io, SharedFrames(&[]));
        assert_eq!((m.result, io.position), (0, n));
        assert_eq!(
            m.counts,
            Some(Counts { polygon: 2, vertex: 6, tag_point: 0, material: 2, frame: 1, sub_model: 0, lod: 1 })
        );
        assert_eq!(m.name, b"old");
        assert_eq!(m.lods[0].indices, vec![2, 0, 1, 0, 1, 2]);
        assert_eq!(
            m.materials,
            vec![
                CemMaterial { name: vec![], values: vec![0, 0, 1, 0, 3], texture_filename: b"a.tga".to_vec() },
                CemMaterial { name: PLAYER_COLOR.to_vec(), values: vec![1, 1, 1, 3, 3], texture_filename: vec![] },
            ]
        );
        let (p0, p1, p2) =
            (Ge3DPoint::new(0.0, 0.0, 0.0), Ge3DPoint::new(1.0, 0.0, 0.0), Ge3DPoint::new(0.0, 1.0, 0.0));
        let f: &CemFrame = &m.frames[0];
        assert_eq!(f.points, vec![p1, p2, p0, p0, p2, p2]);
        assert_eq!(f.uvs, vec![[1.0, 0.0], [0.0, 1.0], [0.0, 0.0], [0.5, 0.5], [0.0, 1.0], [1.0, 1.0]]);
        let q = crate::model_file::tests_quantized(0);
        assert_eq!(f.normals, vec![q, q, q, q, q, GeVector::default()]);
        assert_eq!((f.radius, f.transform.len(), f.bbox.len()), (Some(7.0), 16, 6));
    }

    #[test]
    fn a_cut_legacy_model_keeps_its_members_but_not_its_geometry() {
        let bytes = legacy_13().0;
        for cut in 8..bytes.len() {
            let mut io = BufferIo::reader(bytes[..cut].to_vec());
            let m = load(&mut io, SharedFrames(&[]));
            assert_ne!(m.result, 0, "cut {cut}");
            assert_eq!(io.position, 0);
            if let Some(c) = m.counts {
                // allocated with the version-1 counts; nothing converted
                assert_eq!((c.vertex, c.material), (4, 2));
                assert!(m.frames[0].points.is_empty() && m.lods[0].indices.is_empty());
            }
        }
    }

    #[test]
    fn a_material_partly_in_the_player_colour_is_split() {
        let legacy = LegacyModel {
            polygons: [[0, 1, 2], [2, 3, 4], [4, 5, 0]]
                .iter()
                .map(|&vertices| LegacyPolygon { uvs: [[0.0; 2]; 3], vertices })
                .collect(),
            points: vec![(0..6).map(|i| Ge3DPoint::new(i as f32, 0.0, 0.0)).collect()],
            normals: vec![vec![GeVector::default(); 6]],
            materials: vec![vec![0, 1, 2]],
            groups: vec![vec![1, 1]],
            vertex_points: (0..6).collect(),
        };
        let mut m = CemModel {
            counts: Some(Counts { polygon: 3, vertex: 6, tag_point: 0, material: 1, frame: 1, sub_model: 0, lod: 1 }),
            materials: vec![CemMaterial { texture_filename: b"t".to_vec(), ..CemMaterial::default() }],
            frames: vec![CemFrame::default()],
            ..CemModel::default()
        };
        convert(&legacy, SharedFrames(&[]), &mut m);
        // polygon 1 is listed twice: placed twice, the second time past the end
        assert_eq!(m.lods[0].indices, vec![0, 1, 2, 3, 4, 0, 0, 1, 2]);
        assert_eq!(m.materials[0].values, vec![0, 0, 2, 0, 5]);
        assert_eq!(m.materials[1].values, vec![0, 2, 2, 5, 3]);
        assert_eq!(m.materials[1].name, PLAYER_COLOR);
        assert_eq!(m.materials[1].texture_filename, b"t");
        let xs: Vec<f32> = m.frames[0].points.iter().map(|p| p.x).collect();
        assert_eq!(xs, vec![0.0, 1.0, 2.0, 4.0, 5.0, 2.0, 3.0, 4.0]);
        assert_eq!(m.counts.unwrap().vertex, 8);
    }

    #[test]
    fn shared_frames_get_no_vertices() {
        let legacy = LegacyModel {
            polygons: vec![LegacyPolygon { uvs: [[0.0; 2]; 3], vertices: [0, 1, 2] }],
            points: vec![vec![Ge3DPoint::new(1.0, 2.0, 3.0); 3]; 3],
            normals: vec![vec![GeVector::default(); 3]; 3],
            materials: vec![vec![0]],
            groups: vec![],
            vertex_points: vec![0, 1, 2],
        };
        let mut m = CemModel {
            counts: Some(Counts { polygon: 1, vertex: 3, tag_point: 0, material: 1, frame: 3, sub_model: 0, lod: 1 }),
            materials: vec![CemMaterial::default()],
            frames: vec![CemFrame::default(); 3],
            ..CemModel::default()
        };
        convert(&legacy, SharedFrames(&[(1, -1)]), &mut m);
        assert_eq!(m.frames.iter().map(|f| f.points.len()).collect::<Vec<_>>(), vec![3, 0, 3]);
    }
}
