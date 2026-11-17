//! The effect kinds' classes (`EEEffect`, since R4.4's twenty-fifth round one C++ class for every kind): which kinds
//! were the original's 2D effects (`EEEffect2D`'s leaves: a shared plane model, parts, a position; `effect_2d.rs`) and
//! which its 3D ones (`EEEffect3D`'s: a transformation; `effect_rising.rs`), and which a saved game holds (the
//! manager's Load switch, `EEEffectManager::Load` 0x106978: a load-time effect of the kind).

/// The 2D kinds: the exhaust trail 3, explosion 4, fire 5, impacts 6 and 7, lightning 8, muzzle flash 9, plasma pulse
/// 0xa, smoke puffs 0xb and 0xc, light glow 0xd, fire trail 0xe, marching icons 0xf and 0x10, shockwave 0x12, smoke
/// muzzle blast 0x14, winter breath 0x16, water ripple 0x22, the pooled particle 0x2e, pulse wave 0x2f, 2D projectile
/// 0x30, vortex 0x31, battlefield decal 0x32, the damage controller 0x3e and the command confirmation 0x41.
pub const KINDS_2D: [i32; 25] = [
    3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x12, 0x14, 0x16, 0x22, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0x3e, 0x41,
];

/// The 3D kinds: the icon 2, blast wave 0x18, earthquake 0x1a, water splash 0x1b, destruction smoke 0x1e, volcano 0x20,
/// tracers 0x21, disease 0x23, hurricane 0x24, linear projectile 0x25, shells 0x26, additive pass 0x27, nuclear
/// explosion 0x28, farm 0x29, rain 0x2a, smoke generators 0x2b and 0x2c, marching line 0x2d, citizen beam 0x33,
/// energy wall 0x34, damage vent 0x35, 3D particle 0x39, particle emitter 0x3a, multi-effect 0x3b, goal 0x3c, snow
/// 0x3d, teleport 0x3f, tracer beam 0x40, virus 0x42, pulse cannon ball 0x43, time warp 0x44, shield 0x45, screen
/// flash 0x46, energy wave 0x47.
pub const KINDS_3D: [i32; 34] = [
    2, 0x18, 0x1a, 0x1b, 0x1e, 0x20, 0x21, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x33,
    0x34, 0x35, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3f, 0x40, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
];

/// The multi-effect (never saved) and the command confirmation (never saved): no load-time effect.
const UNSAVED: [i32; 2] = [0x3b, 0x41];

/// A 2D kind.
pub fn is_2d(kind: i32) -> bool {
    KINDS_2D.contains(&kind)
}

/// A kind a saved game holds: every 2D and 3D kind but the unsaved ones (0x11 and unknown kinds have none).
pub fn loadable(kind: i32) -> bool {
    (is_2d(kind) || KINDS_3D.contains(&kind)) && !UNSAVED.contains(&kind)
}

/// `is_2d` for the C++ (`EEEffect::mIs2D`).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_is_2d(kind: i32) -> bool {
    is_2d(kind)
}

/// `loadable` for the C++ (the manager's load-time effects).
#[no_mangle]
pub extern "C" fn ee_gfx_effect_loadable(kind: i32) -> bool {
    loadable(kind)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_classes_part_the_kinds() {
        assert!(KINDS_2D.iter().all(|k| !KINDS_3D.contains(k)));
        assert!(is_2d(0x2e) && is_2d(0x41) && !is_2d(0x39) && !is_2d(0x2a));
        assert_eq!(KINDS_2D.iter().chain(KINDS_3D.iter()).filter(|k| loadable(**k)).count(), 57);
        assert!(!loadable(0x11) && !loadable(0x3b) && !loadable(0x41) && loadable(0x3e) && loadable(0x2d));
    }
}
