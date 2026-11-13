//! `lighting.rs` against the unmodified C++ (`golden_lighting.txt`). A scratch driver (`build/gx17/light`;
//! PORT_LEARNINGS "R4.4, eleventh round") took `SetSceneFog` ... `DimUnitLighting` verbatim out of
//! `EERenderObject.cpp` of the commit before this work (the real `GEColor`, `GEVector` and `GELight` headers,
//! `GEVector::GetMagnitude` from `ee_ge`) over a rasterizer that logs every call and the shared light at each
//! `AddLight`/`UpdateLight`. From an LCG seeded 1 it drew 600 cases (random colours a little outside [0, 1], fades,
//! intensities, suns, a near-zero sun every 37th case, the view's rear distance, the zoom lock, the orthographic
//! view, unit lighting, the screen and the light's slot) and ran one set-up each. This test draws the same values,
//! runs `lighting`, applies the calls to a model of `GELight` (the colour clamped, the computed colour the unclamped
//! one times the intensity, the direction negated and normalized) and prints the driver's line; the driver rebuilt
//! from the new unit over the forwarders printed the same bytes.
// the driver's arithmetic as written
#![allow(clippy::manual_is_multiple_of, clippy::manual_clamp, clippy::field_reassign_with_default)]

use ee_ge::types::GeVector;

use crate::effects::EeGfxColor;
use crate::lighting::{lighting, EeGfxLighting, EeGfxLightingInputs, EeGfxRasterOpKind as K};

const GOLDEN: &str = include_str!("golden_lighting.txt");

struct Lcg(u64);

impl Lcg {
    fn next(&mut self) -> u32 {
        self.0 = self.0.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        (self.0 >> 33) as u32
    }
    fn unit(&mut self) -> f32 {
        (self.next() & 0xffffff) as f32 / 16777216.0
    }
    fn ch(&mut self) -> f32 {
        self.unit() * 1.4 - 0.2
    }
    fn col(&mut self) -> EeGfxColor {
        let red = self.ch();
        let green = self.ch();
        let blue = self.ch();
        EeGfxColor { red, green, blue }
    }
    fn sg(&mut self) -> f32 {
        self.unit() * 2.0 - 1.0
    }
}

fn b(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

fn c(c: [f32; 3]) -> String {
    format!(" {} {} {}", b(c[0]), b(c[1]), b(c[2]))
}

fn clamp(v: f32) -> f32 {
    if v > 1.0 {
        1.0
    } else if v < 0.0 {
        0.0
    } else {
        v
    }
}

/// `GELight`'s members the set-ups change.
struct Light {
    color: [f32; 3],
    computed: [f32; 3],
    intensity: f32,
    direction: GeVector,
}

impl Light {
    fn text(&self) -> String {
        format!(
            "{}{} {} {} {} {}",
            c(self.color),
            c(self.computed),
            b(self.intensity),
            b(self.direction.x),
            b(self.direction.y),
            b(self.direction.z)
        )
    }
}

#[test]
fn the_lighting_set_ups_match_the_cpp() {
    const KINDS: [EeGfxLighting; 11] = [
        EeGfxLighting::SceneFog,
        EeGfxLighting::FogOff,
        EeGfxLighting::Off,
        EeGfxLighting::Prelit,
        EeGfxLighting::Cloud,
        EeGfxLighting::White,
        EeGfxLighting::Terrain,
        EeGfxLighting::MapObject,
        EeGfxLighting::Unit,
        EeGfxLighting::RestoreUnit,
        EeGfxLighting::DimUnit,
    ];
    let mut r = Lcg(1);
    let mut light = Light { color: [1.0; 3], computed: [1.0; 3], intensity: 1.0, direction: GeVector::default() };
    let mut lines = GOLDEN.lines();
    for case in 0..600 {
        let mut i = EeGfxLightingInputs::default();
        i.light_fade = if r.next() % 5 == 0 { 1.0 } else { r.unit() };
        i.fog_color = r.col();
        i.prelit = r.col();
        i.dim_prelit = r.col();
        i.cloud = r.col();
        i.terrain_ambient = r.col();
        i.terrain_light = r.col();
        i.unit_ambient = r.col();
        i.unit_light = r.col();
        i.terrain_ambient_intensity = r.unit() * 2.0;
        i.terrain_light_intensity = r.unit() * 2.0;
        i.unit_ambient_intensity = r.unit() * 2.0;
        i.unit_light_intensity = r.unit() * 2.0;
        let x = r.sg();
        let y = r.sg();
        let z = r.sg();
        i.sun = GeVector { x, y, z };
        if case % 37 == 0 {
            i.sun = GeVector { x: 0.0, y: 0.0, z: (r.unit() - 0.5) * 0.001 };
        }
        i.rear_clipping = r.unit() * 300.0;
        i.zoom_lock = r.unit() * 8.0;
        i.orthographic = r.next() % 4 == 0;
        i.unit_lighting = r.next() % 3 != 0;
        let screen = r.next() % 2;
        let mut slot = r.next() % 4;
        let kind = r.next() % 11;
        let mut text =
            format!("{case} {kind} {} {} {} |", i.orthographic as i32, i.unit_lighting as i32, b(i.light_fade));
        let ops = lighting(KINDS[kind as usize], &i);
        for op in ops.as_slice() {
            let f3 = [op.f[0], op.f[1], op.f[2]];
            let t = match op.kind {
                K::Fog => format!(" fog {} {screen}", op.value),
                K::FogColor => format!(" fogc{} {screen}", c(f3)),
                K::FogRange => format!(" fogr {} {} {screen}", b(op.f[0]), b(op.f[1])),
                K::FogType => format!(" fogt {} {screen}", op.value),
                K::AmbientColor => format!(" ambc{} {screen}", c(f3)),
                K::AmbientIntensity => format!(" ambi {} {screen}", b(op.f[0])),
                K::RemoveLights => format!(" rem {screen}"),
                K::Lighting => format!(" lit {} {screen}", op.value),
                K::LightColor => {
                    light.color = [clamp(f3[0]), clamp(f3[1]), clamp(f3[2])];
                    light.computed = [f3[0] * light.intensity, f3[1] * light.intensity, f3[2] * light.intensity];
                    String::new()
                }
                K::LightIntensity => {
                    light.intensity = op.f[0];
                    light.computed = [light.color[0] * op.f[0], light.color[1] * op.f[0], light.color[2] * op.f[0]];
                    String::new()
                }
                K::LightDirection => {
                    light.direction = GeVector { x: -op.f[0], y: -op.f[1], z: -op.f[2] };
                    light.direction.normalize();
                    String::new()
                }
                K::AddLight => {
                    slot = 7;
                    format!(" add {screen}{}", light.text())
                }
                K::UpdateLight => format!(" upd {slot} {screen}{}", light.text()),
            };
            text.push_str(&t);
        }
        text.push_str(&format!(" | {slot}{}", light.text()));
        assert_eq!(text, lines.next().expect("a golden line"), "case {case}");
    }
}
