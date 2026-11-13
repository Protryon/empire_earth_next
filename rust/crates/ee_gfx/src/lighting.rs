//! The capture's lighting set-ups (`EERenderObject.cpp`: `SetSceneFog` ... `DimUnitLighting`) as lists of the
//! rasterizer calls they make, in their order (`EeGfxRasterOps`): the C++ applies a list to `GERasterizer` and its
//! one shared light (`gU4E9198`, at its slot `gU4E9190`). The values (the environment's colours and intensities,
//! the light fade, the sun's direction, the view's rear distance) are the C++'s, passed in for each call.
//!
//! Drawn only (the renderer's thread; EE-SIM never lights).

use ee_ge::types::GeVector;

use crate::effects::EeGfxColor;

/// The ambient scale of the map objects' second light set (0x441358).
const MAP_OBJECT_AMBIENT: f32 = 0.6;
/// Its sun's z scale (0x44135c).
const MAP_OBJECT_SUN_Z: f32 = 10.0;
/// The fog band per zoom lock and its base (0x441364, 0x441368).
const FOG_BAND: f32 = 4.0;
const FOG_BASE: f32 = 4.0;

/// The set-ups, one per C++ function.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeGfxLighting {
    /// `SetSceneFog`
    SceneFog = 0,
    /// `SetFogOff`
    FogOff = 1,
    /// `SetLightingOff`
    Off = 2,
    /// `SetPrelitLighting`
    Prelit = 3,
    /// `SetCloudLighting`
    Cloud = 4,
    /// `SetWhiteLighting`
    White = 5,
    /// `SetTerrainLighting`
    Terrain = 6,
    /// `SetMapObjectLighting`
    MapObject = 7,
    /// `SetUnitLighting`
    Unit = 8,
    /// `RestoreUnitLighting`
    RestoreUnit = 9,
    /// `DimUnitLighting`
    DimUnit = 10,
}

/// One rasterizer call (or a change of the shared light) and its arguments.
#[repr(u32)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub enum EeGfxRasterOpKind {
    #[default]
    /// `SetFog(value)`
    Fog = 0,
    /// `SetFogColor(color)`
    FogColor = 1,
    /// `SetFogRange(f[0], f[1])`
    FogRange = 2,
    /// `SetFogType(value)`
    FogType = 3,
    /// `SetAmbientColor(color)`
    AmbientColor = 4,
    /// `SetAmbientIntensity(f[0])`
    AmbientIntensity = 5,
    /// `RemoveAllLights()`
    RemoveLights = 6,
    /// `SetLighting(value)`
    Lighting = 7,
    /// the shared light's `SetColor(color)`
    LightColor = 8,
    /// the shared light's `SetIntensity(f[0])`
    LightIntensity = 9,
    /// the shared light's `SetDirection(f[0..3])`
    LightDirection = 10,
    /// `AddLight(light, screen, slot)`
    AddLight = 11,
    /// `UpdateLight(slot, light, screen)`
    UpdateLight = 12,
}

/// A call: `value` for the integer argument, `f` for the floats (a colour's red, green, blue; a direction's x, y, z;
/// a range's near and far).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxRasterOp {
    pub kind: EeGfxRasterOpKind,
    pub value: u32,
    pub f: [f32; 3],
}

/// The calls of one set-up, in order.
pub const EE_GFX_RASTER_OPS: usize = 8;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxRasterOps {
    pub count: u32,
    pub ops: [EeGfxRasterOp; EE_GFX_RASTER_OPS],
}

impl EeGfxRasterOps {
    fn push(&mut self, kind: EeGfxRasterOpKind, value: u32, f: [f32; 3]) {
        self.ops[self.count as usize] = EeGfxRasterOp { kind, value, f };
        self.count += 1;
    }
    fn int(&mut self, kind: EeGfxRasterOpKind, value: u32) {
        self.push(kind, value, [0.0; 3]);
    }
    fn float(&mut self, kind: EeGfxRasterOpKind, value: f32) {
        self.push(kind, 0, [value, 0.0, 0.0]);
    }
    fn color(&mut self, kind: EeGfxRasterOpKind, c: EeGfxColor) {
        self.push(kind, 0, [c.red, c.green, c.blue]);
    }
    pub fn as_slice(&self) -> &[EeGfxRasterOp] {
        &self.ops[..self.count as usize]
    }
}

/// What the set-ups read: the light fade, the fog colour (`gU4E9220`), the view, the environment
/// (`gRenderEnvironment`), the sun (`gU4E9210`) and whether units are lit (`gUnitLighting`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeGfxLightingInputs {
    pub light_fade: f32,
    pub fog_color: EeGfxColor,
    pub orthographic: bool,
    pub unit_lighting: bool,
    pub rear_clipping: f32,
    pub zoom_lock: f32,
    pub prelit: EeGfxColor,
    pub dim_prelit: EeGfxColor,
    pub cloud: EeGfxColor,
    pub terrain_ambient: EeGfxColor,
    pub terrain_light: EeGfxColor,
    pub unit_ambient: EeGfxColor,
    pub unit_light: EeGfxColor,
    pub terrain_ambient_intensity: f32,
    pub terrain_light_intensity: f32,
    pub unit_ambient_intensity: f32,
    pub unit_light_intensity: f32,
    pub sun: GeVector,
}

/// The calls of a set-up.
pub fn lighting(kind: EeGfxLighting, i: &EeGfxLightingInputs) -> EeGfxRasterOps {
    use EeGfxRasterOpKind as K;
    let mut o = EeGfxRasterOps::default();
    let fade = i.light_fade;
    let white = EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 };
    match kind {
        EeGfxLighting::SceneFog => {
            let c = EeGfxColor {
                red: i.fog_color.red * fade,
                green: i.fog_color.green * fade,
                blue: i.fog_color.blue * fade,
            };
            if !i.orthographic {
                o.int(K::Fog, 1);
                o.color(K::FogColor, c);
                o.push(K::FogRange, 0, [i.rear_clipping - (FOG_BAND * i.zoom_lock + FOG_BASE), i.rear_clipping, 0.0]);
                o.int(K::FogType, 1);
            } else {
                o.int(K::Fog, 0);
            }
        }
        EeGfxLighting::FogOff => o.int(K::Fog, 0),
        EeGfxLighting::Off => {
            o.float(K::AmbientIntensity, 0.0);
            o.int(K::RemoveLights, 0);
            o.int(K::Lighting, 0);
        }
        EeGfxLighting::Prelit | EeGfxLighting::Cloud | EeGfxLighting::White => {
            let c = match kind {
                EeGfxLighting::Prelit => i.prelit,
                EeGfxLighting::Cloud => i.cloud,
                _ => white,
            };
            o.color(K::AmbientColor, c);
            o.float(K::AmbientIntensity, fade);
            o.int(K::RemoveLights, 0);
            o.int(K::Lighting, 1);
        }
        EeGfxLighting::Terrain => {
            o.int(K::RemoveLights, 0);
            o.int(K::Lighting, 1);
            o.color(K::AmbientColor, i.terrain_ambient);
            o.float(K::AmbientIntensity, i.terrain_ambient_intensity * fade);
            o.color(K::LightColor, i.terrain_light);
            o.float(K::LightIntensity, i.terrain_light_intensity * fade);
            o.push(K::LightDirection, 0, [i.sun.x, i.sun.y, i.sun.z]);
            o.int(K::AddLight, 0);
        }
        EeGfxLighting::MapObject | EeGfxLighting::Unit => {
            let map_object = kind == EeGfxLighting::MapObject;
            o.int(K::RemoveLights, 0);
            o.int(K::Lighting, 1);
            if i.unit_lighting {
                let mut direction = i.sun;
                let ambient = if map_object {
                    direction.z = i.sun.z * MAP_OBJECT_SUN_Z;
                    direction.normalize();
                    i.unit_ambient_intensity * (fade * MAP_OBJECT_AMBIENT)
                } else {
                    i.unit_ambient_intensity * fade
                };
                o.color(K::AmbientColor, i.unit_ambient);
                o.float(K::AmbientIntensity, ambient);
                o.color(K::LightColor, i.unit_light);
                o.float(K::LightIntensity, i.unit_light_intensity * fade);
                o.push(K::LightDirection, 0, [direction.x, direction.y, direction.z]);
                o.int(K::AddLight, 0);
            } else {
                o.color(K::AmbientColor, i.prelit);
                o.float(K::AmbientIntensity, if map_object { MAP_OBJECT_AMBIENT } else { fade });
            }
        }
        EeGfxLighting::RestoreUnit | EeGfxLighting::DimUnit => {
            let dim = kind == EeGfxLighting::DimUnit;
            if i.unit_lighting {
                let (ambient, light) = if dim {
                    (i.unit_ambient_intensity * fade * 0.5, i.unit_light_intensity * fade * 0.5)
                } else {
                    (i.unit_ambient_intensity * fade, i.unit_light_intensity * fade)
                };
                o.float(K::AmbientIntensity, ambient);
                o.float(K::LightIntensity, light);
                o.int(K::UpdateLight, 0);
            } else {
                o.color(K::AmbientColor, if dim { i.dim_prelit } else { i.prelit });
                o.float(K::AmbientIntensity, fade);
            }
        }
    }
    o
}

#[cfg(test)]
mod tests {
    use super::*;
    use EeGfxRasterOpKind as K;

    fn kinds(o: &EeGfxRasterOps) -> Vec<EeGfxRasterOpKind> {
        o.as_slice().iter().map(|op| op.kind).collect()
    }

    fn inputs() -> EeGfxLightingInputs {
        let c = |r: f32| EeGfxColor { red: r, green: r + 0.1, blue: r + 0.2 };
        EeGfxLightingInputs {
            light_fade: 0.5,
            fog_color: c(0.4),
            orthographic: false,
            unit_lighting: true,
            rear_clipping: 100.0,
            zoom_lock: 2.0,
            prelit: c(0.1),
            dim_prelit: c(0.05),
            cloud: c(0.3),
            terrain_ambient: c(0.2),
            terrain_light: c(0.6),
            unit_ambient: c(0.25),
            unit_light: c(0.7),
            terrain_ambient_intensity: 0.8,
            terrain_light_intensity: 1.2,
            unit_ambient_intensity: 0.9,
            unit_light_intensity: 1.1,
            sun: GeVector { x: 0.0, y: 0.6, z: -0.08 },
        }
    }

    #[test]
    fn the_scene_fog_ends_at_the_rear_distance_unless_orthographic() {
        let mut i = inputs();
        let o = lighting(EeGfxLighting::SceneFog, &i);
        assert_eq!(kinds(&o), [K::Fog, K::FogColor, K::FogRange, K::FogType]);
        assert_eq!(o.ops[0].value, 1);
        assert_eq!(o.ops[1].f, [0.2, 0.5 * 0.5, 0.6 * 0.5]);
        assert_eq!(o.ops[2].f[..2], [100.0 - 12.0, 100.0]);
        i.orthographic = true;
        let o = lighting(EeGfxLighting::SceneFog, &i);
        assert_eq!(kinds(&o), [K::Fog]);
        assert_eq!(o.ops[0].value, 0);
        assert_eq!(lighting(EeGfxLighting::FogOff, &i).as_slice(), o.as_slice());
    }

    #[test]
    fn the_flat_set_ups_light_by_one_colour() {
        let i = inputs();
        let o = lighting(EeGfxLighting::Off, &i);
        assert_eq!(kinds(&o), [K::AmbientIntensity, K::RemoveLights, K::Lighting]);
        assert_eq!((o.ops[0].f[0], o.ops[2].value), (0.0, 0));
        for (kind, colour) in [
            (EeGfxLighting::Prelit, i.prelit),
            (EeGfxLighting::Cloud, i.cloud),
            (EeGfxLighting::White, EeGfxColor { red: 1.0, green: 1.0, blue: 1.0 }),
        ] {
            let o = lighting(kind, &i);
            assert_eq!(kinds(&o), [K::AmbientColor, K::AmbientIntensity, K::RemoveLights, K::Lighting]);
            assert_eq!(o.ops[0].f, [colour.red, colour.green, colour.blue]);
            assert_eq!((o.ops[1].f[0], o.ops[3].value), (0.5, 1));
        }
    }

    #[test]
    fn the_terrain_and_units_add_the_sun() {
        let mut i = inputs();
        let o = lighting(EeGfxLighting::Terrain, &i);
        assert_eq!(
            kinds(&o),
            [
                K::RemoveLights,
                K::Lighting,
                K::AmbientColor,
                K::AmbientIntensity,
                K::LightColor,
                K::LightIntensity,
                K::LightDirection,
                K::AddLight
            ]
        );
        assert_eq!((o.ops[3].f[0], o.ops[5].f[0]), (0.4, 0.6));
        assert_eq!(o.ops[6].f, [0.0, 0.6, -0.08]);
        let o = lighting(EeGfxLighting::Unit, &i);
        assert_eq!(o.count, 8);
        assert_eq!((o.ops[3].f[0], o.ops[5].f[0]), (0.45, 0.55));
        assert_eq!(o.ops[6].f, [0.0, 0.6, -0.08]);
        // the map objects flatten the sun (z times 10, normalized) and dim the ambient light
        let o = lighting(EeGfxLighting::MapObject, &i);
        assert_eq!(o.ops[3].f[0], 0.9 * (0.5 * 0.6));
        assert!((o.ops[6].f[1] - 0.6).abs() < 1e-6 && (o.ops[6].f[2] + 0.8).abs() < 1e-6);
        // without unit lighting: the prelit colour, the map objects at 0.6, the units at the fade
        i.unit_lighting = false;
        let o = lighting(EeGfxLighting::MapObject, &i);
        assert_eq!(kinds(&o), [K::RemoveLights, K::Lighting, K::AmbientColor, K::AmbientIntensity]);
        assert_eq!(o.ops[3].f[0], 0.6);
        assert_eq!(lighting(EeGfxLighting::Unit, &i).ops[3].f[0], 0.5);
    }

    #[test]
    fn restore_and_dim_update_the_light() {
        let mut i = inputs();
        let o = lighting(EeGfxLighting::RestoreUnit, &i);
        assert_eq!(kinds(&o), [K::AmbientIntensity, K::LightIntensity, K::UpdateLight]);
        assert_eq!((o.ops[0].f[0], o.ops[1].f[0]), (0.45, 0.55));
        let o = lighting(EeGfxLighting::DimUnit, &i);
        assert_eq!((o.ops[0].f[0], o.ops[1].f[0]), (0.225, 0.275));
        i.unit_lighting = false;
        let o = lighting(EeGfxLighting::DimUnit, &i);
        assert_eq!(kinds(&o), [K::AmbientColor, K::AmbientIntensity]);
        assert_eq!(o.ops[0].f[0], 0.05);
        assert_eq!(lighting(EeGfxLighting::RestoreUnit, &i).ops[0].f[0], 0.1);
    }
}
