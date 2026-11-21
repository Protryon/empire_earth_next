//! TEST ONLY: the scenario editor gate's world digest (tools/sim_editor.py; was EESimEditor.cpp's `Digest`): the
//! map's tiles and mesh, every player's units, as lines. The editor's selection, paint area and the save's chunks stay
//! the C++'s (the editor form's and the save's objects). The C library's formats the lines used are kept: `%a` as the
//! Microsoft library prints it (13 hex digits, a zero as `0x0.0000000000000p+0`), the FNV-1a hashes of the C++.
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use ee_render::ge::tmobj::{ee_ge_tm_is_valid, ee_ge_tm_point, ee_ge_tm_tile, EeGeTerrainMesh};
use ee_sim::world::{WorldRead, WorldView};

/// The digests' 64-bit FNV-1a hash and a count.
pub struct Hash {
    pub value: u64,
    pub count: u64,
}

impl Default for Hash {
    fn default() -> Self {
        Hash { value: 1469598103934665603, count: 0 }
    }
}

impl Hash {
    pub fn bytes(&mut self, data: &[u8]) {
        for &b in data {
            self.value ^= b as u64;
            self.value = self.value.wrapping_mul(1099511628211);
        }
    }
    /// A C++ `long` widened to 64 bits.
    pub fn long(&mut self, value: i64) {
        self.bytes(&value.to_le_bytes());
    }
    pub fn float(&mut self, value: f32) {
        self.bytes(&value.to_bits().to_le_bytes());
    }
}

/// `%016llx`
pub fn hex(value: u64) -> String {
    format!("{value:016x}")
}

/// `printf("%a", value)` as the Microsoft C library prints a double: `[-]0x1.hhhhhhhhhhhhhp[+-]d`, a zero (and a
/// subnormal) with a leading 0 and the exponent of 1 less the bias (a zero: `p+0`).
pub fn msvc_a(value: f64) -> String {
    let bits = value.to_bits();
    let sign = if bits >> 63 != 0 { "-" } else { "" };
    let exponent = ((bits >> 52) & 0x7ff) as i64;
    let mantissa = bits & ((1u64 << 52) - 1);
    if exponent == 0x7ff {
        return format!("{sign}{}", if mantissa == 0 { "inf" } else { "nan" });
    }
    let (lead, e) = match (exponent, mantissa) {
        (0, 0) => (0, 0),
        (0, _) => (0, -1022),
        _ => (1, exponent - 1023),
    };
    let e_sign = if e < 0 { '-' } else { '+' };
    format!("{sign}0x{lead}.{mantissa:013x}p{e_sign}{}", e.unsigned_abs())
}

/// The map's lines (`map size=.. tiles=..`, `mesh points=..`) and the units' (`unit ..` per unit, `units N hash`);
/// false (with `map none`) without a map.
fn world_lines(world: &WorldView, mesh: *const EeGeTerrainMesh, out: &mut Vec<String>) -> bool {
    let Some(map) = world.map() else {
        out.push("  map none".into());
        return false;
    };
    let size = map.size();
    let mut tiles = Hash::default();
    let mut points = Hash::default();
    for y in 0..size {
        for x in 0..size {
            let at = (x | y << 16) as i32 as i64;
            if let Some(t) = map.tile(x, y) {
                tiles.count += 1;
                tiles.long(at);
                tiles.long(t.terrain_type() as i32 as i64);
                tiles.float(t.height());
                tiles.long(t.ai_value() as i64);
            }
            if mesh.is_null() {
                continue;
            }
            let (ux, uy) = (x as std::os::raw::c_ulong, y as std::os::raw::c_ulong);
            // SAFETY (each): the map's live mesh and a cell of it
            if unsafe { ee_ge_tm_is_valid(mesh, ux, uy, true) } {
                let tile = unsafe { &*ee_ge_tm_tile(mesh, ux, uy) };
                points.long(at);
                points.long(tile.texture_set_id as i64);
                points.long(tile.water_info as i64);
            }
            if unsafe { ee_ge_tm_is_valid(mesh, ux, uy, false) } {
                let point = unsafe { &*ee_ge_tm_point(mesh, ux, uy) };
                points.count += 1;
                points.long(at);
                points.float(point.z);
                points.long(point.color_table_index as i64);
            }
        }
    }
    out.push(format!("  map size={size} tiles={} {}", tiles.count, hex(tiles.value)));
    out.push(format!("  mesh points={} {}", points.count, hex(points.value)));
    let mut all = Hash::default();
    for i in 0..world.num_players() as usize {
        let Some(player) = world.player(i) else { continue };
        for u in player.units() {
            let c = u.complex_unit();
            let m = c.map(|c| c.transform());
            let mut h = Hash::default();
            if let Some(m) = m {
                for row in m.iter().take(3) {
                    for &v in row {
                        h.float(v);
                    }
                }
            }
            let at = |r: usize| m.map(|m| m[r][3] as f64).unwrap_or(0.0);
            let unit_type = u.class().map(|k| k.unit_type()).unwrap_or(0) as i32;
            let (tx, ty) = u.tile_xy();
            let selected: bool = ee_sim::world_field!(u.rts(), mIsEditorSelected);
            let line = format!(
                "  unit {} type={} player={} tile={},{} hp={} sel={} built={} cargo={} at={},{},{} m={}",
                u.id(),
                unit_type,
                i,
                tx as i32,
                ty as i32,
                u.hit_points() as i32,
                selected as i32,
                c.is_some_and(|c| c.is_built()) as i32,
                c.map(|c| c.cargo_len()).unwrap_or(0),
                msvc_a(at(0)),
                msvc_a(at(1)),
                msvc_a(at(2)),
                hex(h.value)
            );
            all.bytes(line.as_bytes());
            all.count += 1;
            out.push(line);
        }
    }
    out.push(format!("  units {} {}", all.count, hex(all.value)));
    true
}

/// The world's digest lines (the editor's `Digest`, under the world's write lock: a step); false without a map.
/// `mesh` is the map's terrain mesh (`GETerrainMesh::GetRust`, 0 without a map).
pub fn world_digest(mesh: usize, out: &mut Vec<String>) -> bool {
    // SAFETY: the caller (a step) holds the world
    let token = unsafe { WorldRead::begin() };
    match WorldView::global(&token) {
        Some(world) => world_lines(&world, mesh as *const EeGeTerrainMesh, out),
        None => {
            out.push("  map none".into());
            false
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formats_and_hash() {
        assert_eq!(msvc_a(0.0), "0x0.0000000000000p+0");
        assert_eq!(msvc_a(-10000.0), "-0x1.3880000000000p+13");
        assert_eq!(msvc_a(12.0), "0x1.8000000000000p+3");
        assert_eq!(msvc_a(32.0), "0x1.0000000000000p+5");
        assert_eq!(msvc_a(0.25), "0x1.0000000000000p-2");
        assert_eq!(msvc_a(f64::from(f32::from_bits(0x3ed105eb))), "0x1.a20bd60000000p-2");
        assert_eq!(hex(0x14650fb0739d0383), "14650fb0739d0383");
        let mut h = Hash::default();
        h.bytes(b"a");
        // the C++'s basis (one digit short of FNV's: the editor gate's golden hashes are over it)
        assert_eq!(hex(Hash::default().value), "14650fb0739d0383");
        assert_eq!(h.value, (1469598103934665603u64 ^ 0x61).wrapping_mul(1099511628211));
        let mut l = Hash::default();
        l.long(-1);
        let mut b = Hash::default();
        b.bytes(&[0xff; 8]);
        assert_eq!(l.value, b.value);
    }
}
