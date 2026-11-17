//! The WGSL of the blits and of the fixed-function pipeline (the C++'s `kBlitShader` and `kGXFixedFunctionShader`
//! verbatim; the `INTERP` token becomes nothing or `@interpolate(flat)` in the two variants).

pub const BLIT_SHADER: &str = r#"
struct B { dst: vec4f, src: vec4f, key: vec4f, color: vec4f, mode: vec4u, misc: vec4f };
@group(0) @binding(0) var<uniform> b: B;
@group(0) @binding(1) var t: texture_2d<f32>;
@group(0) @binding(2) var s: sampler;
struct V { @builtin(position) p: vec4f, @location(0) uv: vec2f };
@vertex fn vertex_main(@builtin(vertex_index) i: u32) -> V {
    var c = array<vec2f, 6>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0), vec2f(0.0, 1.0), vec2f(1.0, 0.0),
                            vec2f(1.0, 1.0));
    let k = c[i];
    var o: V;
    o.p = vec4f(mix(b.dst.x, b.dst.z, k.x), mix(b.dst.y, b.dst.w, k.y), b.misc.x, 1.0);
    o.uv = mix(b.src.xy, b.src.zw, k);
    return o;
}
@fragment fn fragment_main(v: V) -> @location(0) vec4f {
    let c = textureSampleLevel(t, s, v.uv, 0.0);
    if (b.mode.x == 2u) { return b.color; }
    if (b.mode.x == 1u && all(abs(c.rgb - b.key.rgb) < vec3f(0.002))) { discard; }
    if (b.mode.y == 1u) {   // presentation: opaque, with the brightness gamma (misc.y; 0 or 1: none)
        if (b.misc.y > 0.0 && b.misc.y != 1.0) { return vec4f(pow(c.rgb, vec3f(1.0 / b.misc.y)), 1.0); }
        return vec4f(c.rgb, 1.0);
    }
    return c;
}
"#;

const FIXED_FUNCTION_SHADER: &str = r#"
struct Light { diffuse: vec4f, specular: vec4f, ambient: vec4f, position: vec4f, direction: vec4f, atten: vec4f,
               kind: vec4u };
struct FF {
    wvp: mat4x4f, wv: mat4x4f,
    matDiffuse: vec4f, matAmbient: vec4f, matSpecular: vec4f, matEmissive: vec4f,
    ambient: vec4f, fogColor: vec4f,
    fog: vec4f,      // start, end, enabled, specular power
    pixel: vec4f,    // 1 / viewport width, 1 / viewport height, alpha reference (0..255), mip bias
    mode: vec4u,     // lighting, diffuse from vertex, ambient from vertex, specular
    stage: vec4u,    // colour op, colour arg 1, colour arg 2, alpha op
    stage2: vec4u,   // alpha arg 1, alpha arg 2, alpha function (0 = no test), colour key
    lights: array<Light, 8>,
};
@group(0) @binding(0) var<uniform> u: FF;
@group(0) @binding(1) var tex: texture_2d<f32>;
@group(0) @binding(2) var smp: sampler;

struct VIn { @location(0) pos: vec3f, @location(1) nrm: vec3f, @location(2) bgra: vec4f, @location(3) uv: vec2f };
struct VOut {
    @builtin(position) pos: vec4f,
    @location(0) INTERP diffuse: vec4f,
    @location(1) INTERP specular: vec4f,
    @location(2) uv: vec2f,
    @location(3) fog: f32,
};

@vertex fn vertex_main(v: VIn) -> VOut {
    var o: VOut;
    var clip = u.wvp * vec4f(v.pos, 1.0);
    clip.x = clip.x + u.pixel.x * clip.w;   // Direct3D 7 pixel centres: half a pixel right and down
    clip.y = clip.y - u.pixel.y * clip.w;
    o.pos = clip;
    o.uv = v.uv;
    let P = (u.wv * vec4f(v.pos, 1.0)).xyz;
    let col = v.bgra.zyxw;   // the ARGB dword's bytes are B, G, R, A
    var diffuse = col;
    var spec = vec4f(0.0);
    if (u.mode.x != 0u) {
        let N = normalize((u.wv * vec4f(v.nrm, 0.0)).xyz);
        let md = select(u.matDiffuse, col, u.mode.y != 0u);
        let ma = select(u.matAmbient, col, u.mode.z != 0u);
        var acc = u.matEmissive.rgb + ma.rgb * u.ambient.rgb;
        var sacc = vec3f(0.0);
        for (var i = 0u; i < 8u; i = i + 1u) {
            let L = u.lights[i];
            if (L.kind.y == 0u) { continue; }
            var dir = -L.direction.xyz;
            var att = 1.0;
            if (L.kind.x != 3u) {
                let d = L.position.xyz - P;
                let dist = length(d);
                if (dist > L.atten.w) { continue; }
                dir = d / max(dist, 1e-6);
                att = 1.0 / max(L.atten.x + L.atten.y * dist + L.atten.z * dist * dist, 1e-6);
            }
            let ndl = dot(N, dir);
            acc = acc + att * (ma.rgb * L.ambient.rgb + md.rgb * L.diffuse.rgb * max(ndl, 0.0));
            if (u.mode.w != 0u && ndl > 0.0) {
                let H = normalize(dir + vec3f(0.0, 0.0, -1.0));
                sacc = sacc + att * u.matSpecular.rgb * L.specular.rgb * pow(max(dot(N, H), 0.0), u.fog.w);
            }
        }
        diffuse = vec4f(clamp(acc, vec3f(0.0), vec3f(1.0)), md.a);
        spec = vec4f(clamp(sacc, vec3f(0.0), vec3f(1.0)), 0.0);
    }
    o.diffuse = diffuse;
    o.specular = spec;
    o.fog = 1.0;
    if (u.fog.z != 0.0) {
        o.fog = clamp((u.fog.y - P.z) / max(u.fog.y - u.fog.x, 1e-6), 0.0, 1.0);
    }
    return o;
}

fn arg(a: u32, t: vec4f, d: vec4f) -> vec4f {
    if (a == 2u) { return t; }
    return d;   // diffuse; stage 0's current is the diffuse colour
}

fn op(o: u32, a1: vec4f, a2: vec4f) -> vec4f {
    if (o == 2u) { return a1; }
    if (o == 3u) { return a2; }
    if (o == 4u) { return a1 * a2; }
    return a2;
}

fn passes(f: u32, a: f32, r: f32) -> bool {
    switch f {
        case 1u: { return false; }
        case 2u: { return a < r; }
        case 3u: { return a == r; }
        case 4u: { return a <= r; }
        case 5u: { return a > r; }
        case 6u: { return a != r; }
        case 7u: { return a >= r; }
        default: { return true; }
    }
}

@fragment fn fragment_main(i: VOut) -> @location(0) vec4f {
    let t = textureSampleBias(tex, smp, i.uv, u.pixel.w);
    var c = i.diffuse;
    if (u.stage.x != 1u) {
        let rgb = op(u.stage.x, arg(u.stage.y, t, i.diffuse), arg(u.stage.z, t, i.diffuse)).rgb;
        var a = i.diffuse.a;
        if (u.stage.w != 1u) {
            a = op(u.stage.w, arg(u.stage2.x, t, i.diffuse), arg(u.stage2.y, t, i.diffuse)).a;
        }
        c = vec4f(rgb, a);
        if (u.stage2.w != 0u && t.a == 0.0) { discard; }
    }
    var rgb = c.rgb;
    if (u.mode.w != 0u) { rgb = min(rgb + i.specular.rgb, vec3f(1.0)); }
    rgb = mix(u.fogColor.rgb, rgb, i.fog);
    if (u.stage2.z != 0u && !passes(u.stage2.z, round(c.a * 255.0), u.pixel.z)) { discard; }
    return vec4f(rgb, c.a);
}
"#;

/// The fixed-function shader, in its smooth (`flat` false) or flat variant.
pub fn fixed_function_shader(flat: bool) -> String {
    FIXED_FUNCTION_SHADER.replace("INTERP", if flat { "@interpolate(flat)" } else { "" })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn validate(source: &str) {
        let module = naga::front::wgsl::parse_str(source).expect("the WGSL parses");
        naga::valid::Validator::new(naga::valid::ValidationFlags::all(), naga::valid::Capabilities::empty())
            .validate(&module)
            .expect("the WGSL validates");
    }

    #[test]
    fn the_blit_shader_compiles() {
        validate(BLIT_SHADER);
    }

    #[test]
    fn both_fixed_function_variants_compile() {
        validate(&fixed_function_shader(false));
        validate(&fixed_function_shader(true));
    }

    #[test]
    fn the_variants_differ_only_in_interpolation() {
        let (smooth, flat) = (fixed_function_shader(false), fixed_function_shader(true));
        assert!(!smooth.contains("INTERP") && !flat.contains("INTERP"));
        assert_eq!(flat.matches("@interpolate(flat)").count(), 2);
        assert_eq!(flat.replace("@interpolate(flat)", ""), smooth);
    }

    /// Pins the shaders to the C++ sources they were taken from (GXDevice.cpp, GX.cpp) by a checksum of their text.
    #[test]
    fn shader_text_matches_the_cpp_checksum() {
        let sum = |text: &str| text.bytes().fold(0u32, |h, b| h.wrapping_mul(31).wrapping_add(u32::from(b)));
        let golden = include_str!("golden_shaders.txt");
        let mut lines = golden.lines();
        assert_eq!(sum(BLIT_SHADER), lines.next().unwrap().parse::<u32>().unwrap(), "blit");
        assert_eq!(sum(FIXED_FUNCTION_SHADER), lines.next().unwrap().parse::<u32>().unwrap(), "fixed function");
    }
}
