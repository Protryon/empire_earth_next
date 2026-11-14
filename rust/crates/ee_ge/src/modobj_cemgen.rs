// (included by modobj_tests.rs and the scratch golden driver's library)
// A decoded model drawn from a seed (GEModel's golden driver and its cargo replay share this generator): counts,
// names, centre, LOD polygons (LOD 0 from the allocation or re-created), material values, tag point names and frames
// (points, normals, UVs, tag points, transform, box; some short), sub-models two levels deep. One top model in ten
// is refused (no counts); sub-models are always allocated (the original called through a refused sub-model).

struct CemLcg(u64);

impl CemLcg {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn below(&mut self, n: u32) -> u32 {
        if n == 0 {
            0
        } else {
            self.next() % n
        }
    }
    fn float(&mut self) -> f32 {
        (self.below(2001) as f32 - 1000.0) / 64.0
    }
    fn name(&mut self) -> Vec<u8> {
        const NAMES: [&str; 10] = ["", "t1", "t2", "x9", "abc", "Zed", "b", "tag", "mud", "3d"];
        NAMES[self.below(10) as usize].as_bytes().to_vec()
    }
}

pub fn cem_random(seed: u64) -> CemModel {
    let mut r = CemLcg(seed ^ 0x9e37_79b9_7f4a_7c15);
    cem_draw(&mut r, true, 0)
}

fn cem_draw(r: &mut CemLcg, top: bool, depth: u32) -> CemModel {
    if top && r.below(10) == 0 {
        return CemModel { result: 9, ..CemModel::default() };
    }
    let frames = 1 + r.below(3);
    let vertex = 1 + r.below(6);
    let polygon = 1 + r.below(4);
    let tags = r.below(3);
    let materials = r.below(3);
    let lods = 1 + r.below(2);
    let subs = if depth < 2 && r.below(2) == 0 { r.below(3) } else { 0 };
    let result = if r.below(8) == 0 { 5 } else { 0 };
    let name = r.name();
    let center = (0..[3, 3, 3, 0, 2][r.below(5) as usize]).map(|_| r.float()).collect();
    let mut lod_list = Vec::new();
    for l in 0..lods {
        let created = if l == 0 { if r.below(5) == 0 { Some(polygon) } else { None } } else { Some(1 + r.below(4)) };
        let count = created.unwrap_or(polygon);
        let mut n = count * 3;
        if r.below(4) == 0 {
            n -= r.below(3).min(n);
        }
        let indices = (0..n).map(|_| r.below(vertex)).collect();
        lod_list.push(CemPolygons { created, indices });
    }
    let mut material_list = Vec::new();
    for _ in 0..materials {
        let name = r.name();
        let mut values = vec![r.below(5)];
        for _ in 0..lods {
            let start = r.below(polygon);
            values.push(start);
            values.push(r.below(polygon - start + 1));
        }
        values.push(r.below(2));
        values.push(r.below(vertex + 1));
        let keep = values.len() as u32 - if r.below(5) == 0 { r.below(3) } else { 0 };
        values.truncate(keep as usize);
        let texture_filename = r.name();
        material_list.push(CemMaterial { name, values, texture_filename });
    }
    let tag_point_names = (0..tags).map(|_| r.name()).collect();
    let mut frame_list = Vec::new();
    for f in 0..frames {
        let radius = if r.below(10) < 7 { Some(r.float()) } else { None };
        let target = if r.below(5) == 0 { r.below(frames) } else { f };
        let points_n = if r.below(5) == 0 { r.below(vertex + 1) } else { vertex };
        let points = (0..points_n).map(|_| Ge3DPoint::new(r.float(), r.float(), r.float())).collect();
        let normals_n = if r.below(3) == 0 { 0 } else { vertex };
        let normals = (0..normals_n).map(|_| GeVector::new(r.float(), r.float(), r.float())).collect();
        let uvs_n = if r.below(3) == 0 { 0 } else { vertex };
        let uvs = (0..uvs_n).map(|_| [r.float(), r.float()]).collect();
        let tags_n = if r.below(4) == 0 { r.below(tags + 1) } else { tags };
        let tag_points = (0..tags_n).map(|_| Ge3DPoint::new(r.float(), r.float(), r.float())).collect();
        let transform_n = [16, 16, 16, 0, 12][r.below(5) as usize];
        let transform = (0..transform_n)
            .map(|k| if r.below(3) == 0 { [1.0, 0.0, 0.0, 0.0, 0.0][k % 5] } else { r.float() })
            .collect();
        let bbox_n = [6, 6, 0, 3][r.below(4) as usize];
        let bbox = (0..bbox_n).map(|_| r.float()).collect();
        frame_list.push(CemFrame { radius, target, points, normals, uvs, tag_points, transform, bbox });
    }
    let sub_models = (0..subs).map(|_| cem_draw(r, false, depth + 1)).collect();
    CemModel {
        result,
        counts: Some(Counts {
            polygon,
            vertex,
            tag_point: tags,
            material: materials,
            frame: frames,
            sub_model: subs,
            lod: lods,
        }),
        name,
        center,
        lods: lod_list,
        materials: material_list,
        tag_point_names,
        frames: frame_list,
        sub_models,
    }
}
