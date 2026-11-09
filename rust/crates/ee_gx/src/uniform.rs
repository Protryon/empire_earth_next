//! The bytes the shaders read: the fixed-function uniform block and the blit uniform, from the device's state.

use crate::abi::*;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct LightUniform {
    pub diffuse: [f32; 4],
    pub specular: [f32; 4],
    pub ambient: [f32; 4],
    pub position: [f32; 4],
    pub direction: [f32; 4],
    pub attenuation: [f32; 4],
    pub kind: [u32; 4],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct FixedFunctionUniform {
    pub wvp: [f32; 16],
    pub wv: [f32; 16],
    pub material_diffuse: [f32; 4],
    pub material_ambient: [f32; 4],
    pub material_specular: [f32; 4],
    pub material_emissive: [f32; 4],
    pub ambient: [f32; 4],
    pub fog_color: [f32; 4],
    pub fog: [f32; 4],
    pub pixel: [f32; 4],
    pub mode: [u32; 4],
    pub stage: [u32; 4],
    pub stage2: [u32; 4],
    pub lights: [LightUniform; 8],
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct BlitUniform {
    pub destination: [f32; 4],
    pub source: [f32; 4],
    pub key: [f32; 4],
    pub color: [f32; 4],
    pub mode: [u32; 4],
    pub misc: [f32; 4],
}

/// The bytes of a plain-data uniform.
pub fn as_bytes<T: Copy>(value: &T) -> &[u8] {
    // SAFETY: T is one of this module's repr(C) arrays of f32/u32, all of whose bytes are initialised
    unsafe { std::slice::from_raw_parts(value as *const T as *const u8, std::mem::size_of::<T>()) }
}

/// Row-vector product (`a` then `b`).
pub fn multiply(a: &[f32; 16], b: &[f32; 16]) -> [f32; 16] {
    let mut out = [0.0; 16];
    for r in 0..4 {
        for c in 0..4 {
            out[r * 4 + c] =
                a[r * 4] * b[c] + a[r * 4 + 1] * b[4 + c] + a[r * 4 + 2] * b[8 + c] + a[r * 4 + 3] * b[12 + c];
        }
    }
    out
}

/// A D3D ARGB dword as r, g, b, a floats.
pub fn color(argb: u32) -> [f32; 4] {
    [
        ((argb >> 16) & 0xff) as f32 / 255.0,
        ((argb >> 8) & 0xff) as f32 / 255.0,
        (argb & 0xff) as f32 / 255.0,
        ((argb >> 24) & 0xff) as f32 / 255.0,
    ]
}

/// A render state's float, stored as its bits (`D3DRS_FOGSTART`'s dword).
pub fn as_float(value: u64) -> f32 {
    f32::from_bits(value as u32)
}

fn copy(c: &GxColorValue) -> [f32; 4] {
    [c.r, c.g, c.b, c.a]
}

/// The device state `BuildUniform` reads.
pub struct DeviceState<'a> {
    pub world: &'a GxMatrix,
    pub view: &'a GxMatrix,
    pub projection: &'a GxMatrix,
    pub viewport: &'a GxViewport,
    pub render_state: &'a [u64],
    pub stage_state: &'a [u64],
    pub material: &'a GxMaterial,
    pub lights: &'a [GxLight; 8],
    pub light_on: &'a [bool; 8],
    pub texture_has_key: bool,
}

pub fn build_fixed_function(d: &DeviceState) -> FixedFunctionUniform {
    let rs = d.render_state;
    let wv = multiply(&d.world.m, &d.view.m);
    let mut u = FixedFunctionUniform { wv, wvp: multiply(&wv, &d.projection.m), ..Default::default() };
    u.material_diffuse = copy(&d.material.diffuse);
    u.material_ambient = copy(&d.material.ambient);
    u.material_specular = copy(&d.material.specular);
    u.material_emissive = copy(&d.material.emissive);
    u.ambient = color(rs[GXRS_AMBIENT] as u32);
    u.fog_color = color(rs[GXRS_FOGCOLOR] as u32);
    u.fog[0] = as_float(rs[GXRS_FOGSTART]);
    u.fog[1] = as_float(rs[GXRS_FOGEND]);
    u.fog[2] = if rs[GXRS_FOGENABLE] != 0 && rs[GXRS_FOGVERTEXMODE] as u32 == GXFOG_LINEAR { 1.0 } else { 0.0 };
    u.fog[3] = d.material.power;
    u.pixel[0] = if d.viewport.dwWidth != 0 { 1.0 / d.viewport.dwWidth as f32 } else { 0.0 };
    u.pixel[1] = if d.viewport.dwHeight != 0 { 1.0 / d.viewport.dwHeight as f32 } else { 0.0 };
    u.pixel[2] = (rs[GXRS_ALPHAREF] & 0xff) as f32;
    u.pixel[3] = (-16.0f32).max(15.99f32.min(as_float(d.stage_state[GXTSS_MIPMAPLODBIAS])));
    let color_vertex = rs[GXRS_COLORVERTEX] != 0;
    u.mode[0] = u32::from(rs[GXRS_LIGHTING] != 0);
    u.mode[1] = u32::from(color_vertex && rs[GXRS_DIFFUSEMATERIALSOURCE] as u32 == GXMCS_COLOR1);
    u.mode[2] = u32::from(color_vertex && rs[GXRS_AMBIENTMATERIALSOURCE] as u32 == GXMCS_COLOR1);
    u.mode[3] = u32::from(rs[GXRS_LIGHTING] != 0 && rs[GXRS_SPECULARENABLE] != 0);
    let s = d.stage_state;
    u.stage = [s[GXTSS_COLOROP] as u32, s[GXTSS_COLORARG1] as u32, s[GXTSS_COLORARG2] as u32, s[GXTSS_ALPHAOP] as u32];
    u.stage2[0] = s[GXTSS_ALPHAARG1] as u32;
    u.stage2[1] = s[GXTSS_ALPHAARG2] as u32;
    u.stage2[2] = if rs[GXRS_ALPHATESTENABLE] != 0 {
        if rs[GXRS_ALPHAFUNC] != 0 {
            rs[GXRS_ALPHAFUNC] as u32
        } else {
            GXCMP_ALWAYS
        }
    } else {
        0
    };
    u.stage2[3] = u32::from(rs[GXRS_COLORKEYENABLE] != 0 && d.texture_has_key);
    let v = &d.view.m;
    for i in 0..8 {
        let light = &d.lights[i];
        let o = &mut u.lights[i];
        o.diffuse = copy(&light.dcvDiffuse);
        o.specular = copy(&light.dcvSpecular);
        o.ambient = copy(&light.dcvAmbient);
        // to view space: positions as points, directions as vectors (row vectors times View)
        let p = &light.dvPosition;
        o.position[0] = p.x * v[0] + p.y * v[4] + p.z * v[8] + v[12];
        o.position[1] = p.x * v[1] + p.y * v[5] + p.z * v[9] + v[13];
        o.position[2] = p.x * v[2] + p.y * v[6] + p.z * v[10] + v[14];
        let n = &light.dvDirection;
        let mut x = n.x * v[0] + n.y * v[4] + n.z * v[8];
        let mut y = n.x * v[1] + n.y * v[5] + n.z * v[9];
        let mut z = n.x * v[2] + n.y * v[6] + n.z * v[10];
        let length = (x * x + y * y + z * z).sqrt();
        if length > 0.0 {
            x /= length;
            y /= length;
            z /= length;
        }
        o.direction[0] = x;
        o.direction[1] = y;
        o.direction[2] = z;
        o.attenuation[0] = light.dvAttenuation0;
        o.attenuation[1] = light.dvAttenuation1;
        o.attenuation[2] = light.dvAttenuation2;
        o.attenuation[3] = if light.dvRange > 0.0 { light.dvRange } else { 3.4e38 };
        o.kind[0] = light.dltType as u32;
        o.kind[1] = u32::from(d.light_on[i]);
    }
    u
}

/// A rectangle in a surface's pixels as clip-space left, top, right, bottom.
pub fn to_ndc(rect: &Rect, width: u32, height: u32) -> [f32; 4] {
    [
        2.0 * rect.left as f32 / width as f32 - 1.0,
        1.0 - 2.0 * rect.top as f32 / height as f32,
        2.0 * rect.right as f32 / width as f32 - 1.0,
        1.0 - 2.0 * rect.bottom as f32 / height as f32,
    ]
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::mem::size_of;

    #[test]
    fn layouts_are_what_the_shaders_declare() {
        assert_eq!(size_of::<LightUniform>(), 112);
        assert_eq!(size_of::<FixedFunctionUniform>(), 128 + 8 * 16 + 3 * 16 + 8 * 112);
        assert_eq!(size_of::<BlitUniform>(), 96);
    }

    #[test]
    fn multiply_is_row_vector_a_then_b() {
        let mut a = GxMatrix::identity().m;
        a[12] = 5.0; // translate
        let mut b = GxMatrix::identity().m;
        b[0] = 2.0; // scale x
        let r = multiply(&a, &b);
        assert_eq!(r[0], 2.0);
        assert_eq!(r[12], 10.0, "translated, then scaled");
    }

    #[test]
    fn color_splits_argb() {
        assert_eq!(color(0xff00_00ff), [0.0, 0.0, 1.0, 1.0]);
        assert_eq!(color(0x8000_ff00), [0.0, 1.0, 0.0, 128.0 / 255.0]);
    }

    #[test]
    fn ndc_of_the_whole_surface_is_the_clip_square() {
        assert_eq!(to_ndc(&Rect::new(0, 0, 640, 480), 640, 480), [-1.0, 1.0, 1.0, -1.0]);
        assert_eq!(to_ndc(&Rect::new(160, 120, 480, 360), 640, 480), [-0.5, 0.5, 0.5, -0.5]);
    }

    #[test]
    fn as_float_reinterprets_the_bits() {
        assert_eq!(as_float(0x3f80_0000), 1.0);
        assert_eq!(as_float(0x1_3f80_0000), 1.0);
    }

    #[test]
    fn as_bytes_covers_the_whole_struct() {
        assert_eq!(as_bytes(&BlitUniform::default()).len(), 96);
    }

    /// golden_uniforms.txt: the C++ `BuildUniform` for several device states; each block is one case's inputs (see
    /// `golden_gen.py`'s generator comments) and the expected bytes in hex.
    #[test]
    fn golden_fixed_function_uniforms_from_the_cpp() {
        let text = include_str!("golden_uniforms.txt");
        let mut cases = 0;
        for block in text.split("\n===\n") {
            if block.trim().is_empty() {
                continue;
            }
            let mut lines = block.lines();
            let first = lines.next().unwrap();
            let n: Vec<f32> = first.split_whitespace().map(|f| f.parse().unwrap()).collect();
            let (world, view, projection) =
                (GxMatrix { m: mat(&n[..16]) }, GxMatrix { m: mat(&n[16..32]) }, GxMatrix { m: mat(&n[32..48]) });
            let viewport =
                GxViewport { dwX: 0, dwY: 0, dwWidth: n[48] as _, dwHeight: n[49] as _, dvMinZ: 0.0, dvMaxZ: 1.0 };
            let material = GxMaterial {
                diffuse: col(&n[50..54]),
                ambient: col(&n[54..58]),
                specular: col(&n[58..62]),
                emissive: col(&n[62..66]),
                power: n[66],
            };
            let mut render_state = vec![0u64; GXRS_COUNT];
            let mut stage_state = vec![0u64; GXTSS_COUNT];
            let pairs: Vec<u64> =
                lines.next().unwrap().split_whitespace().map(|f| u64::from_str_radix(f, 16).unwrap()).collect();
            for p in pairs.chunks(2) {
                render_state[p[0] as usize] = p[1];
            }
            let pairs: Vec<u64> =
                lines.next().unwrap().split_whitespace().map(|f| u64::from_str_radix(f, 16).unwrap()).collect();
            for p in pairs.chunks(2) {
                stage_state[p[0] as usize] = p[1];
            }
            let mut lights = [GxLight::default(); 8];
            let mut light_on = [false; 8];
            let l: Vec<f32> = lines.next().unwrap().split_whitespace().map(|f| f.parse().unwrap()).collect();
            for i in 0..2 {
                let o = i * 21;
                lights[i] = GxLight {
                    dltType: l[o] as _,
                    dcvDiffuse: col(&l[o + 1..o + 5]),
                    dcvSpecular: col(&l[o + 5..o + 9]),
                    dcvAmbient: col(&l[o + 9..o + 13]),
                    dvPosition: GxVector { x: l[o + 13], y: l[o + 14], z: l[o + 15] },
                    dvDirection: GxVector { x: l[o + 16], y: l[o + 17], z: l[o + 18] },
                    dvRange: l[o + 19],
                    dvAttenuation0: l[o + 20],
                    ..Default::default()
                };
                lights[i].dvAttenuation1 = 0.0;
                lights[i].dvAttenuation2 = 0.0;
                light_on[i] = i == 0;
            }
            let has_key = lines.next().unwrap().trim() == "1";
            let expected = hex(lines.next().unwrap());
            let u = build_fixed_function(&DeviceState {
                world: &world,
                view: &view,
                projection: &projection,
                viewport: &viewport,
                render_state: &render_state,
                stage_state: &stage_state,
                material: &material,
                lights: &lights,
                light_on: &light_on,
                texture_has_key: has_key,
            });
            assert_eq!(as_bytes(&u), expected.as_slice(), "case {cases}");
            cases += 1;
        }
        assert!(cases >= 4);
    }

    fn mat(v: &[f32]) -> [f32; 16] {
        let mut m = [0.0; 16];
        m.copy_from_slice(v);
        m
    }

    fn col(v: &[f32]) -> GxColorValue {
        GxColorValue { r: v[0], g: v[1], b: v[2], a: v[3] }
    }

    fn hex(text: &str) -> Vec<u8> {
        let text = text.trim();
        (0..text.len()).step_by(2).map(|i| u8::from_str_radix(&text[i..i + 2], 16).unwrap()).collect()
    }
}
