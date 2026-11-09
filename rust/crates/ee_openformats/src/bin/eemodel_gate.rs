//! The open-format gate program (tools/eemodel/cppgate.py runs it): converts every case of a folder with the game's
//! loader code (`ee_openformats`) and writes the result for cppgate.py to compare with the Python converter's.
//!   eemodel_gate <dir>
//! Each line of <dir>/cases.txt is "model <name>.gltf|.glb" or "texture <name>.png" (further frames: <name>@1.png,
//! ...); the result goes to <dir>/<name>.out (the .cem / .sst bytes) or <dir>/<name>.err (the message).

use std::fs;
use std::path::Path;
use std::process::ExitCode;

use ee_openformats::{gltf_to_cem, png_to_sst, Resolver};

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
        let result: Result<Vec<u8>, Vec<u8>> = if kind == "model" {
            match fs::read(dir.join(file)) {
                Err(_) => Err(b"cannot read".to_vec()),
                Ok(data) => {
                    let mut read = |uri: &[u8]| -> Option<Vec<u8>> {
                        let name = String::from_utf8_lossy(uri).into_owned();
                        fs::read(dir.join(name)).ok()
                    };
                    let resolver: &mut Resolver = &mut read;
                    gltf_to_cem(&data, Some(resolver)).map_err(|f| f.0)
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
                    png_to_sst(&refs).map_err(|f| f.0)
                }
            }
        };
        match result {
            Ok(bytes) => {
                if fs::write(dir.join(format!("{stem}.out")), bytes).is_err() {
                    return ExitCode::from(3);
                }
            }
            Err(message) => {
                if fs::write(dir.join(format!("{stem}.err")), message).is_err() {
                    return ExitCode::from(3);
                }
            }
        }
    }
    ExitCode::SUCCESS
}
