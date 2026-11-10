//! The open-format gate program (tools/eemodel/cppgate.py runs it): decodes every case of a folder with the game's
//! loader code (`ee_openformats`) into what the game stores and writes a digest of it for cppgate.py to compare with
//! the recorded digests (and the Python converter's accept/refuse decision).
//!   eemodel_gate <dir>
//! Each line of <dir>/cases.txt is "model <name>.gltf|.glb" or "texture <name>.png" (further frames: <name>@1.png,
//! ...); the result goes to <dir>/<name>.out (the digest, 16 hex digits; a model's for three shared-frame maps, none,
//! the odd frames and all frames, one per line) or <dir>/<name>.err (the message).
//!
//! The recorded digests (tools/eemodel/cppgate/golden_store.json) were taken before D9's deletion with an --old mode
//! that also decoded each case the old way (the .cem / .sst bytes, the .cem reader, the .sst frames as GETexture::Load
//! and the Targa reader took them) and found every digest equal.

use std::fs;
use std::path::Path;
use std::process::ExitCode;

use ee_ge::model_store::{CemModel, SharedFrames};
use ee_openformats::{gltf_to_model, png_to_texture, PngTexture, Resolver};

/// FNV-1a over every field the game stores, floats by their bits.
struct Hash(u64);

impl Hash {
    fn new() -> Hash {
        Hash(0xcbf2_9ce4_8422_2325)
    }
    fn bytes(&mut self, data: &[u8]) {
        for &b in data {
            self.0 ^= b as u64;
            self.0 = self.0.wrapping_mul(0x100_0000_01b3);
        }
    }
    fn u(&mut self, v: u64) {
        self.bytes(&v.to_le_bytes());
    }
    fn f(&mut self, v: f32) {
        self.u(v.to_bits() as u64);
    }
    fn s(&mut self, v: &[u8]) {
        self.u(v.len() as u64);
        self.bytes(v);
    }
}

fn hash_model(h: &mut Hash, m: &CemModel) {
    h.u(m.result as u64);
    match m.counts {
        None => h.u(0),
        Some(c) => {
            h.u(1);
            for v in [c.polygon, c.vertex, c.tag_point, c.material, c.frame, c.sub_model, c.lod] {
                h.u(v as u64);
            }
        }
    }
    h.s(&m.name);
    h.u(m.center.len() as u64);
    m.center.iter().for_each(|&v| h.f(v));
    h.u(m.lods.len() as u64);
    for l in &m.lods {
        h.u(l.created.map_or(u64::MAX, |c| c as u64));
        h.u(l.indices.len() as u64);
        l.indices.iter().for_each(|&v| h.u(v as u64));
    }
    h.u(m.materials.len() as u64);
    for mat in &m.materials {
        h.s(&mat.name);
        h.u(mat.values.len() as u64);
        mat.values.iter().for_each(|&v| h.u(v as u64));
        h.s(&mat.texture_filename);
    }
    h.u(m.tag_point_names.len() as u64);
    m.tag_point_names.iter().for_each(|n| h.s(n));
    h.u(m.frames.len() as u64);
    for f in &m.frames {
        h.u(f.radius.map_or(u64::MAX, |r| r.to_bits() as u64));
        h.u(f.target as u64);
        let points = |h: &mut Hash, p: &[ee_ge::types::Ge3DPoint]| {
            h.u(p.len() as u64);
            p.iter().for_each(|p| [p.x, p.y, p.z].iter().for_each(|&v| h.f(v)));
        };
        points(h, &f.points);
        h.u(f.normals.len() as u64);
        f.normals.iter().for_each(|n| [n.x, n.y, n.z].iter().for_each(|&v| h.f(v)));
        h.u(f.uvs.len() as u64);
        f.uvs.iter().for_each(|uv| uv.iter().for_each(|&v| h.f(v)));
        points(h, &f.tag_points);
        h.u(f.transform.len() as u64);
        f.transform.iter().for_each(|&v| h.f(v));
        h.u(f.bbox.len() as u64);
        f.bbox.iter().for_each(|&v| h.f(v));
    }
    h.u(m.sub_models.len() as u64);
    m.sub_models.iter().for_each(|s| hash_model(h, s));
}

fn hash_texture(t: &PngTexture) -> u64 {
    let mut h = Hash::new();
    h.u(t.width as u64);
    h.u(t.height as u64);
    h.u(t.frames.len() as u64);
    for f in &t.frames {
        h.u(f.width as u64);
        h.u(f.height as u64);
        h.u(f.channels as u64);
        h.s(&f.pixels);
    }
    h.0
}

/// The shared-frame maps the gate stores each model with (EEGraphicObject's: frame -> -1).
fn sharings() -> [Vec<(u32, i32)>; 3] {
    [Vec::new(), (1..512).filter(|f| f % 2 == 1).map(|f| (f, -1)).collect(), (1..512).map(|f| (f, -1)).collect()]
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    let [_, dir] = args.as_slice() else {
        eprintln!("usage: eemodel_gate <dir>");
        return ExitCode::from(2);
    };
    let dir = Path::new(dir);
    let Ok(list) = fs::read_to_string(dir.join("cases.txt")) else {
        eprintln!("no cases.txt");
        return ExitCode::from(2);
    };
    for line in list.lines() {
        let Some((kind, file)) = line.split_once(' ') else { continue };
        let stem = file.rsplit_once('.').map_or(file, |(s, _)| s);
        let result: Result<String, Vec<u8>> = if kind == "model" {
            match fs::read(dir.join(file)) {
                Err(_) => Err(b"cannot read".to_vec()),
                Ok(data) => {
                    let mut out = String::new();
                    let mut failure = None;
                    for shared in sharings() {
                        let mut read = |uri: &[u8]| -> Option<Vec<u8>> {
                            let name = String::from_utf8_lossy(uri).into_owned();
                            fs::read(dir.join(name)).ok()
                        };
                        let resolver: &mut Resolver = &mut read;
                        match gltf_to_model(&data, Some(resolver), SharedFrames(&shared)) {
                            Ok(m) => {
                                let mut h = Hash::new();
                                hash_model(&mut h, &m);
                                out += &format!("{:016x}\n", h.0);
                            }
                            Err(f) => {
                                failure = Some(f.0);
                                break;
                            }
                        }
                    }
                    match failure {
                        Some(f) => Err(f),
                        None => Ok(out),
                    }
                }
            }
        } else {
            match fs::read(dir.join(file)) {
                Err(_) => Err(b"cannot read".to_vec()),
                Ok(first) => {
                    let mut frames = vec![first];
                    let mut k = 1;
                    while let Ok(f) = fs::read(dir.join(format!("{stem}@{k}.png"))) {
                        frames.push(f);
                        k += 1;
                    }
                    let refs: Vec<&[u8]> = frames.iter().map(|f| f.as_slice()).collect();
                    png_to_texture(&refs).map(|t| format!("{:016x}\n", hash_texture(&t))).map_err(|f| f.0)
                }
            }
        };
        let written = match result {
            Ok(digest) => fs::write(dir.join(format!("{stem}.out")), digest),
            Err(message) => fs::write(dir.join(format!("{stem}.err")), message),
        };
        if written.is_err() {
            return ExitCode::from(3);
        }
    }
    ExitCode::SUCCESS
}
