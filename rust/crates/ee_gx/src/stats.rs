//! `EE_GX_STATS`: the summary of the 3D draws, as the C++ logged it.

use crate::abi::*;
use crate::cformat::format_g;
use crate::uniform::{as_float, multiply};

/// One vertex's size in the renderer's single format.
pub const VERTEX_SIZE: usize = 36;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct DrawStats {
    pub scenes: u32,
    pub draws: [u32; 3],
    pub primitives: [u32; 3],
    pub lit: u32,
    pub textured: u32,
    pub blended: u32,
    pub vertices: u32,
    pub in_front: u32,
    pub inside: u32,
    pub clears: u32,
    pub fogged: u32,
    pub z: u32,
    pub fog_color: u32,
    pub ambient: u32,
    pub fog_start: f32,
    pub fog_end: f32,
    pub fog_zone: [u32; 3],
    pub min: [u8; 4],
    pub max: [u8; 4],
    pub min_clip: [f32; 3],
    pub max_clip: [f32; 3],
}

impl Default for DrawStats {
    fn default() -> Self {
        DrawStats {
            scenes: 0,
            draws: [0; 3],
            primitives: [0; 3],
            lit: 0,
            textured: 0,
            blended: 0,
            vertices: 0,
            in_front: 0,
            inside: 0,
            clears: 0,
            fogged: 0,
            z: 0,
            fog_color: 0,
            ambient: 0,
            fog_start: 0.0,
            fog_end: 0.0,
            fog_zone: [0; 3],
            min: [255; 4],
            max: [0; 4],
            min_clip: [3e38; 3],
            max_clip: [-3e38; 3],
        }
    }
}

/// The three lines `StatsScene` logs.
pub fn stats_lines(s: &DrawStats) -> [String; 3] {
    let g = |v: f32| format_g(f64::from(v));
    [
        format!(
            "GX stats: {} scenes, {} clears; draws points {} lines {} triangles {} (primitives {}/{}/{}); lit {} textured {} blended {} fogged {} z {}",
            s.scenes,
            s.clears,
            s.draws[0],
            s.draws[1],
            s.draws[2],
            s.primitives[0],
            s.primitives[1],
            s.primitives[2],
            s.lit,
            s.textured,
            s.blended,
            s.fogged,
            s.z
        ),
        format!(
            "GX stats: vertices {}, in front {}, inside {}; clip x {}..{} y {}..{} w {}..{}",
            s.vertices,
            s.in_front,
            s.inside,
            g(s.min_clip[0]),
            g(s.max_clip[0]),
            g(s.min_clip[1]),
            g(s.max_clip[1]),
            g(s.min_clip[2]),
            g(s.max_clip[2])
        ),
        format!(
            "GX stats: BGRA min {},{},{},{} max {},{},{},{}; fog {}..{} colour {:08x} (view z before/in/past {}/{}/{}); ambient {:08x}",
            s.min[0],
            s.min[1],
            s.min[2],
            s.min[3],
            s.max[0],
            s.max[1],
            s.max[2],
            s.max[3],
            g(s.fog_start),
            g(s.fog_end),
            s.fog_color,
            s.fog_zone[0],
            s.fog_zone[1],
            s.fog_zone[2],
            s.ambient
        ),
    ]
}

/// The device state `DrawStats` reads.
pub struct StatsInput<'a> {
    pub world: &'a [f32; 16],
    pub view: &'a [f32; 16],
    pub projection: &'a [f32; 16],
    pub render_state: &'a [u64],
}

/// `DrawStats`: one draw's numbers, including every vertex after the world-view-projection transform.
pub fn account_draw(
    s: &mut DrawStats,
    primitive_type: u32,
    input: &StatsInput,
    vertices: &[u8],
    count: u32,
    index_count: Option<u32>,
    textured: bool,
) {
    let rs = input.render_state;
    let topology = topology_of(primitive_type);
    let elements = index_count.unwrap_or(count);
    s.draws[topology] += 1;
    s.primitives[topology] += match topology {
        0 => elements,
        1 => elements / 2,
        _ => elements / 3,
    };
    s.lit += u32::from(rs[GXRS_LIGHTING] != 0);
    s.textured += u32::from(textured);
    s.blended += u32::from(rs[GXRS_ALPHABLENDENABLE] != 0);
    s.z += u32::from(rs[GXRS_ZENABLE] != 0);
    s.ambient = rs[GXRS_AMBIENT] as u32;
    let fog = rs[GXRS_FOGENABLE] != 0;
    if fog {
        s.fogged += 1;
        s.fog_start = as_float(rs[GXRS_FOGSTART]);
        s.fog_end = as_float(rs[GXRS_FOGEND]);
        s.fog_color = rs[GXRS_FOGCOLOR] as u32;
    }
    let wv = multiply(input.world, input.view);
    let wvp = multiply(&wv, input.projection);
    for i in 0..count as usize {
        let v = &vertices[i * VERTEX_SIZE..(i + 1) * VERTEX_SIZE];
        let p = [read_f32(v, 0), read_f32(v, 4), read_f32(v, 8)];
        let mut c = [0.0f32; 4];
        for (k, out) in c.iter_mut().enumerate() {
            *out = p[0] * wvp[k] + p[1] * wvp[4 + k] + p[2] * wvp[8 + k] + wvp[12 + k];
        }
        s.vertices += 1;
        if fog {
            let z = p[0] * wv[2] + p[1] * wv[6] + p[2] * wv[10] + wv[14];
            let zone = if z < s.fog_start {
                0
            } else if z <= s.fog_end {
                1
            } else {
                2
            };
            s.fog_zone[zone] += 1;
        }
        if c[3] > 0.0 {
            s.in_front += 1;
            if c[0].abs() <= c[3] && c[1].abs() <= c[3] && c[2] >= 0.0 && c[2] <= c[3] {
                s.inside += 1;
            }
        }
        let clip = [if c[3] != 0.0 { c[0] / c[3] } else { 0.0 }, if c[3] != 0.0 { c[1] / c[3] } else { 0.0 }, c[3]];
        for (k, value) in clip.iter().enumerate() {
            s.min_clip[k] = s.min_clip[k].min(*value);
            s.max_clip[k] = s.max_clip[k].max(*value);
        }
        for k in 0..4 {
            s.min[k] = s.min[k].min(v[24 + k]);
            s.max[k] = s.max[k].max(v[24 + k]);
        }
    }
}

fn read_f32(bytes: &[u8], offset: usize) -> f32 {
    f32::from_le_bytes([bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]])
}

/// Points 0, lines 1, triangles 2 (`GXPT_*` to the pipeline key's topology).
pub fn topology_of(primitive_type: u32) -> usize {
    match primitive_type {
        GXPT_POINTLIST => 0,
        GXPT_LINELIST => 1,
        _ => 2,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn vertex(x: f32, y: f32, z: f32, bgra: [u8; 4]) -> Vec<u8> {
        let mut v = Vec::new();
        for value in [x, y, z, 0.0, 0.0, 0.0] {
            v.extend_from_slice(&f32::to_le_bytes(value));
        }
        v.extend_from_slice(&bgra);
        v.extend_from_slice(&[0; 8]);
        v
    }

    #[test]
    fn default_statistics_start_at_the_c_values() {
        let d = DrawStats::default();
        assert_eq!(d.min, [255; 4]);
        assert_eq!(d.max, [0; 4]);
        assert_eq!(d.min_clip, [3e38; 3]);
        assert_eq!(d.max_clip, [-3e38; 3]);
    }

    /// golden_stats.txt: the C++ `StatsScene` lines for a sample draw (including every format specifier).
    #[test]
    fn golden_stats_lines_from_the_cpp() {
        let mut lines = include_str!("golden_stats.txt").lines();
        let s = DrawStats {
            scenes: 7,
            clears: 3,
            draws: [1, 2, 3],
            primitives: [4, 5, 6],
            lit: 1,
            textured: 2,
            blended: 3,
            fogged: 4,
            z: 5,
            vertices: 100,
            in_front: 90,
            inside: 80,
            min_clip: [-1.5, 0.25, 0.1],
            max_clip: [1.5, 1000000.0, 2.5e-7],
            min: [1, 2, 3, 4],
            max: [5, 6, 7, 8],
            fog_start: 0.5,
            fog_end: 100.0,
            fog_color: 0xabcdef,
            ambient: 0x12,
            fog_zone: [1, 2, 3],
        };
        for line in stats_lines(&s) {
            assert_eq!(line, lines.next().unwrap());
        }
    }

    #[test]
    fn account_draw_counts_and_transforms() {
        let identity = GxMatrix::identity().m;
        let mut rs = vec![0u64; GXRS_COUNT];
        rs[GXRS_LIGHTING] = 1;
        rs[GXRS_ZENABLE] = 1;
        rs[GXRS_FOGENABLE] = 1;
        rs[GXRS_FOGSTART] = u64::from(0.0f32.to_bits());
        rs[GXRS_FOGEND] = u64::from(10.0f32.to_bits());
        let input = StatsInput { world: &identity, view: &identity, projection: &identity, render_state: &rs };
        let mut vertices = vertex(0.5, 0.5, 0.5, [10, 20, 30, 40]);
        vertices.extend(vertex(2.0, 0.0, 0.5, [50, 60, 70, 80]));
        vertices.extend(vertex(0.0, 0.0, 20.0, [1, 2, 3, 4]));
        let mut s = DrawStats::default();
        account_draw(&mut s, GXPT_TRIANGLELIST, &input, &vertices, 3, None, true);
        assert_eq!(s.draws, [0, 0, 1]);
        assert_eq!(s.primitives, [0, 0, 1]);
        assert_eq!((s.lit, s.textured, s.blended, s.z, s.fogged), (1, 1, 0, 1, 1));
        assert_eq!(s.vertices, 3);
        assert_eq!(s.in_front, 3); // identity: w is 1
        assert_eq!(s.inside, 1); // x 2 is outside, z 20 is outside
        assert_eq!(s.fog_zone, [0, 2, 1]);
        assert_eq!(s.min, [1, 2, 3, 4]);
        assert_eq!(s.max, [50, 60, 70, 80]);
    }

    #[test]
    fn account_draw_indexed_counts_primitives_from_indices() {
        let identity = GxMatrix::identity().m;
        let rs = vec![0u64; GXRS_COUNT];
        let input = StatsInput { world: &identity, view: &identity, projection: &identity, render_state: &rs };
        let vertices = vertex(0.0, 0.0, 0.0, [0, 0, 0, 0]);
        let mut s = DrawStats::default();
        account_draw(&mut s, GXPT_LINELIST, &input, &vertices, 1, Some(4), false);
        assert_eq!(s.primitives, [0, 2, 0]);
        account_draw(&mut s, GXPT_POINTLIST, &input, &vertices, 1, Some(4), false);
        assert_eq!(s.primitives, [4, 2, 0]);
    }

    #[test]
    fn topology_maps_the_primitive_types() {
        assert_eq!(topology_of(GXPT_POINTLIST), 0);
        assert_eq!(topology_of(GXPT_LINELIST), 1);
        assert_eq!(topology_of(GXPT_TRIANGLELIST), 2);
    }
}
