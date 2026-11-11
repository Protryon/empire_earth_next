//! Whether a calamity can be cast (`src/Empire Earth/Calamities`: `EECalamityClass::CanCastAt` and its overrides in
//! `EEGlobalCalamityClass`, `EELocationCalamityClass`, `EEUnitCalamityClass`, `EECCLineOfSight`,
//! `EECCSpecialAttack`; every class's `IsValidTarget` and `IsValidLocation`). The C++ overrides fill an
//! [`EeCalamityCast`] and answer the world's questions through the host ([`CalamityHost`], the same call and reply
//! as the updates'; the state is unused): the caster's power and sight, the tiles, the area effects, the target's
//! flags, the continents, the units near a tile. The class's `IsValidLocation`/`IsValidTarget` are asked through
//! the host too (they are virtual: the C++ override answers, itself through [`valid_location`] and
//! [`valid_target`]). The C++'s short circuits are kept: a question is asked only where the C++ asked it.

use crate::calamity_update::{kind, CalamityHost, EeCalamityCall, EeCalamityOp, EeCalamityReply, EeCalamityState};

/// Which `CanCastAt` runs (`EeCalamityCast::mode`).
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeCalamityCastMode {
    /// `EECalamityClass::CanCastAt`: the caster, its power, the tile, its sight, the area effects.
    Base = 0,
    /// `EEGlobalCalamityClass` and `EECCLineOfSight`: always 0.
    Global,
    /// `EELocationCalamityClass`: the base, then the class's location.
    Location,
    /// `EEUnitCalamityClass`: the base, then the class's target.
    Unit,
    /// `EECCSpecialAttack`: the base with 2 allowed, then the target.
    SpecialAttack,
}

/// One `CanCastAt`, `IsValidLocation` or `IsValidTarget` question.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeCalamityCast {
    pub mode: EeCalamityCastMode,
    pub class_enum: i32,
    pub x: u32,
    pub y: u32,
    /// `GetCost()`.
    pub cost: i32,
    /// The area effects' and the leprosy's radius in tiles (unsigned), the hurricane's `mArea`.
    pub radius: u32,
    pub area: i32,
    /// `kTerrainSpace` (the meteor storm's tiles).
    pub space_terrain: i32,
    pub check: bool,
    pub has_caster: bool,
    pub blocked_by_immunity: bool,
    /// The hurricane only over water (`mOverWater`).
    pub over_water: bool,
}

/// A host call of the cast tests (through [`EeCalamityOp`], from `CasterPlayerIndex` on).
#[allow(clippy::too_many_arguments)]
fn ask<H: CalamityHost>(
    h: &mut H,
    op: EeCalamityOp,
    index: i32,
    value: i32,
    x: f32,
    y: f32,
    z: f32,
    w: f32,
) -> EeCalamityReply {
    let mut state = EeCalamityState::default();
    h.call(&mut state, &EeCalamityCall { op, index, value, index2: 0, x, y, z, w })
}

/// A question about tile (x, y): index x, index2 y.
fn ask_tile<H: CalamityHost>(h: &mut H, op: EeCalamityOp, x: i32, y: i32, value: i32) -> EeCalamityReply {
    let mut state = EeCalamityState::default();
    h.call(&mut state, &EeCalamityCall { index: x, index2: y, value, ..EeCalamityCall::new(op) })
}

/// `CanCastAt`: 0 allowed, 1 refused, 2 protected by an area effect (the base's).
pub fn can_cast_at<H: CalamityHost>(h: &mut H, c: &EeCalamityCast) -> i32 {
    match c.mode {
        EeCalamityCastMode::Global => 0,
        EeCalamityCastMode::Base => base(h, c),
        EeCalamityCastMode::Location => {
            let result = base(h, c);
            if result == 0 && !ask_tile(h, EeCalamityOp::ValidLocation, c.x as i32, c.y as i32, 0).ok {
                1
            } else {
                result
            }
        }
        EeCalamityCastMode::Unit => {
            let result = base(h, c);
            if result == 0 {
                target(h)
            } else {
                result
            }
        }
        EeCalamityCastMode::SpecialAttack => {
            let mut result = base(h, c);
            if result == 2 {
                result = 0;
            }
            if result == 0 {
                target(h)
            } else {
                result
            }
        }
    }
}

/// `EECalamityClass::CanCastAt`.
fn base<H: CalamityHost>(h: &mut H, c: &EeCalamityCast) -> i32 {
    let player = if c.has_caster { ask(h, EeCalamityOp::CasterPlayerIndex, 0, 0, 0.0, 0.0, 0.0, 0.0).value } else { 0 };
    let mut result = 1;
    if c.check
        && (player == 0
            || (c.has_caster
                && ask(h, EeCalamityOp::CasterHitPoints, 0, 0, 0.0, 0.0, 0.0, 0.0).value > 0
                && ask(h, EeCalamityOp::CasterPower, 0, 0, 0.0, 0.0, 0.0, 0.0).value >= c.cost))
        && ask_tile(h, EeCalamityOp::TileExists, c.x as i32, c.y as i32, 0).ok
        && (!c.has_caster || ask_tile(h, EeCalamityOp::TileVisible, c.x as i32, c.y as i32, 0).ok)
    {
        result = if c.blocked_by_immunity && ask_tile(h, EeCalamityOp::AreaEffect, c.x as i32, c.y as i32, player).ok {
            2
        } else {
            0
        };
    }
    result
}

/// The unit classes' target test: the unit with the target's id, then the class's `IsValidTarget`.
fn target<H: CalamityHost>(h: &mut H) -> i32 {
    if ask(h, EeCalamityOp::TargetLookup, 0, 0, 0.0, 0.0, 0.0, 0.0).ok
        && ask(h, EeCalamityOp::ValidTarget, 0, 0, 0.0, 0.0, 0.0, 0.0).ok
    {
        0
    } else {
        1
    }
}

/// [`EeCalamityOp::TargetView`]'s flags.
pub const EE_CALAMITY_TARGET_ON_MAP: u32 = 1;
pub const EE_CALAMITY_TARGET_IS_CASTER: u32 = 2;
pub const EE_CALAMITY_TARGET_SAME_PLAYER: u32 = 4;
pub const EE_CALAMITY_TARGET_ENEMY: u32 = 8;
pub const EE_CALAMITY_TARGET_IN_SET: u32 = 16;
pub const EE_CALAMITY_TARGET_FOUNDATION: u32 = 32;
pub const EE_CALAMITY_TARGET_BUILT: u32 = 64;
pub const EE_CALAMITY_TARGET_DISABLED: u32 = 128;
pub const EE_CALAMITY_TARGET_VOLCANO: u32 = 256;

/// The class's `IsValidTarget(caster, target)` (the fire's and the plague's stay their functor's `CanInfect`, C++).
pub fn valid_target<H: CalamityHost>(h: &mut H, c: &EeCalamityCast) -> bool {
    let f = ask(h, EeCalamityOp::TargetView, 0, 0, 0.0, 0.0, 0.0, 0.0).flags;
    let has = |b: u32| f & b != 0;
    match c.class_enum {
        kind::MORALE => has(EE_CALAMITY_TARGET_ON_MAP) && has(EE_CALAMITY_TARGET_IN_SET),
        kind::TIME_WARP => {
            has(EE_CALAMITY_TARGET_ON_MAP) && !has(EE_CALAMITY_TARGET_IS_CASTER) && has(EE_CALAMITY_TARGET_IN_SET)
        }
        kind::SPECIAL_ATTACK => {
            has(EE_CALAMITY_TARGET_IN_SET) && !has(EE_CALAMITY_TARGET_SAME_PLAYER) && has(EE_CALAMITY_TARGET_ENEMY)
        }
        kind::CONSCRIPTION => {
            has(EE_CALAMITY_TARGET_ON_MAP)
                && !has(EE_CALAMITY_TARGET_IS_CASTER)
                && has(EE_CALAMITY_TARGET_SAME_PLAYER)
                && has(EE_CALAMITY_TARGET_IN_SET)
        }
        kind::LOGIC_BOMB => {
            has(EE_CALAMITY_TARGET_ON_MAP)
                && !has(EE_CALAMITY_TARGET_SAME_PLAYER)
                && has(EE_CALAMITY_TARGET_ENEMY)
                && !has(EE_CALAMITY_TARGET_FOUNDATION)
                && has(EE_CALAMITY_TARGET_BUILT)
                && !has(EE_CALAMITY_TARGET_DISABLED)
                && !has(EE_CALAMITY_TARGET_VOLCANO)
                && has(EE_CALAMITY_TARGET_IN_SET)
        }
        kind::DROP_OBJECT => {
            has(EE_CALAMITY_TARGET_ON_MAP)
                && !has(EE_CALAMITY_TARGET_SAME_PLAYER)
                && has(EE_CALAMITY_TARGET_ENEMY)
                && !has(EE_CALAMITY_TARGET_FOUNDATION)
                && has(EE_CALAMITY_TARGET_BUILT)
                && has(EE_CALAMITY_TARGET_IN_SET)
        }
        _ => false,
    }
}

/// The class's `IsValidLocation(caster, x, y)`.
pub fn valid_location<H: CalamityHost>(h: &mut H, c: &EeCalamityCast) -> bool {
    let (x, y) = (c.x as i32, c.y as i32);
    match c.class_enum {
        kind::AREA_EFFECT | kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL | kind::LEPROSY => {
            ask_tile(h, EeCalamityOp::AreaOnMap, x, y, (c.radius.wrapping_add(1) / 2) as i32).ok
        }
        kind::EARTHQUAKE => {
            if !ask_tile(h, EeCalamityOp::ValidTile, x, y, 0).ok {
                return false;
            }
            if ask(h, EeCalamityOp::SpaceMap, 0, 0, 0.0, 0.0, 0.0, 0.0).ok {
                let caster = ask(h, EeCalamityOp::CasterContinent, 0, 0, 0.0, 0.0, 0.0, 0.0).value;
                return caster == ask_tile(h, EeCalamityOp::TileContinent, x, y, 0).value;
            }
            true
        }
        kind::HURRICANE => {
            if c.over_water && !ask_tile(h, EeCalamityOp::TileBlocked, x, y, 0).ok {
                return false;
            }
            // the tile exists (the base's test came first)
            if ask_tile(h, EeCalamityOp::TerrainType, x, y, 0).value == 12 {
                return false;
            }
            if !ask_tile(h, EeCalamityOp::AreaOnMap, x, y, c.area).ok {
                return false;
            }
            let radius = ask(h, EeCalamityOp::InnerSphere, 0, 0, 0.0, 0.0, 0.0, 0.0).x * 1.5;
            let center_x = x as f32 + 0.5;
            let center_y = y as f32 + 0.5;
            let near = ask(h, EeCalamityOp::UnitsNear, 0, 0, center_x, center_y, 0.0, radius);
            for i in (0..near.id as i32).rev() {
                // a living hurricane or anti-matter storm there already
                if ask(h, EeCalamityOp::NearStorm, i, 0, 0.0, 0.0, 0.0, 0.0).ok {
                    return false;
                }
            }
            true
        }
        kind::METEOR_STORM => {
            let tile = ask_tile(h, EeCalamityOp::TerrainType, x, y, 0);
            tile.ok && tile.value == c.space_terrain
        }
        kind::VOLCANO => {
            let size = ask(h, EeCalamityOp::TileSize, 0, 0, 0.0, 0.0, 0.0, 0.0).value;
            // `inX - theSize / 2`: unsigned, then a long
            let start_x = (c.x.wrapping_sub((size / 2) as u32)) as i32;
            let start_y = (c.y.wrapping_sub((size / 2) as u32)) as i32;
            let mut result = true;
            let mut tx = start_x;
            while tx < start_x.wrapping_add(size) && result {
                let mut ty = start_y;
                while ty < start_y.wrapping_add(size) && result {
                    result = ask_tile(h, EeCalamityOp::TileExists, tx, ty, 0).ok;
                    if result {
                        result = !ask_tile(h, EeCalamityOp::EdgeTile, tx, ty, 0).ok;
                        if result {
                            result = ask_tile(h, EeCalamityOp::TileUnblocked, tx, ty, 0).ok;
                        }
                    }
                    ty = ty.wrapping_add(1);
                }
                tx = tx.wrapping_add(1);
            }
            result
        }
        kind::TELEPORT => {
            !ask_tile(h, EeCalamityOp::TileBlocked, x, y, 0).ok && ask_tile(h, EeCalamityOp::AreaOnMap, x, y, 2).ok
        }
        kind::SPECIAL_LOCATION => {
            if ask(h, EeCalamityOp::CasterIsRadioMan, 0, 0, 0.0, 0.0, 0.0, 0.0).ok {
                ask_tile(h, EeCalamityOp::TileFree, x, y, 0).ok
            } else {
                true
            }
        }
        _ => false,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// An answer for an op, at any tile or at one.
    type Reply = (EeCalamityOp, Option<(i32, i32)>, EeCalamityReply);

    /// Answers by op (and tile), logs every op.
    #[derive(Default)]
    struct Host {
        log: Vec<EeCalamityOp>,
        replies: Vec<Reply>,
    }

    impl Host {
        fn set(&mut self, op: EeCalamityOp, reply: EeCalamityReply) {
            self.replies.push((op, None, reply));
        }
        fn set_tile(&mut self, op: EeCalamityOp, x: i32, y: i32, reply: EeCalamityReply) {
            self.replies.push((op, Some((x, y)), reply));
        }
    }

    impl CalamityHost for Host {
        fn call(&mut self, _state: &mut EeCalamityState, call: &EeCalamityCall) -> EeCalamityReply {
            self.log.push(call.op);
            let tile = (call.index, call.index2);
            self.replies
                .iter()
                .rev()
                .find(|r| r.0 == call.op && r.1.is_none_or(|t| t == tile))
                .map(|r| r.2)
                .unwrap_or_default()
        }
    }

    fn yes() -> EeCalamityReply {
        EeCalamityReply { ok: true, ..Default::default() }
    }

    fn value(v: i32) -> EeCalamityReply {
        EeCalamityReply { value: v, ..Default::default() }
    }

    fn cast(mode: EeCalamityCastMode, class_enum: i32) -> EeCalamityCast {
        EeCalamityCast {
            mode,
            class_enum,
            x: 4,
            y: 5,
            cost: 10,
            radius: 0,
            area: 0,
            space_terrain: 0,
            check: true,
            has_caster: true,
            blocked_by_immunity: true,
            over_water: false,
        }
    }

    #[test]
    fn the_base_needs_a_living_caster_with_power_and_sight() {
        let c = cast(EeCalamityCastMode::Base, kind::EARTHQUAKE);
        let mut h = Host::default();
        h.set(EeCalamityOp::CasterPlayerIndex, value(2));
        h.set(EeCalamityOp::CasterHitPoints, value(5));
        h.set(EeCalamityOp::CasterPower, value(10));
        h.set(EeCalamityOp::TileExists, yes());
        h.set(EeCalamityOp::TileVisible, yes());
        assert_eq!(can_cast_at(&mut h, &c), 0);
        h.set(EeCalamityOp::AreaEffect, yes());
        assert_eq!(can_cast_at(&mut h, &c), 2);
        h.set(EeCalamityOp::CasterPower, value(9));
        assert_eq!(can_cast_at(&mut h, &c), 1);
        // without the check: 1, and nothing asked but the caster's player
        let mut h = Host::default();
        assert_eq!(can_cast_at(&mut h, &EeCalamityCast { check: false, ..c }), 1);
        assert_eq!(h.log, vec![EeCalamityOp::CasterPlayerIndex]);
        // nature's cast: no power, no sight asked
        let mut h = Host::default();
        h.set(EeCalamityOp::TileExists, yes());
        assert_eq!(can_cast_at(&mut h, &EeCalamityCast { has_caster: false, ..c }), 0);
        assert_eq!(h.log, vec![EeCalamityOp::TileExists, EeCalamityOp::AreaEffect]);
        assert_eq!(can_cast_at(&mut Host::default(), &cast(EeCalamityCastMode::Global, kind::CLOAKING)), 0);
    }

    fn allowed() -> Host {
        let mut h = Host::default();
        h.set(EeCalamityOp::TileExists, yes());
        h
    }

    #[test]
    fn location_and_unit_classes_add_their_own_tests() {
        let nature = |mode, class_enum| EeCalamityCast {
            has_caster: false,
            blocked_by_immunity: false,
            ..cast(mode, class_enum)
        };
        let mut h = allowed();
        assert_eq!(can_cast_at(&mut h, &nature(EeCalamityCastMode::Location, kind::TELEPORT)), 1);
        h.set(EeCalamityOp::ValidLocation, yes());
        assert_eq!(can_cast_at(&mut h, &nature(EeCalamityCastMode::Location, kind::TELEPORT)), 0);
        let mut h = allowed();
        h.set(EeCalamityOp::TargetLookup, yes());
        assert_eq!(can_cast_at(&mut h, &nature(EeCalamityCastMode::Unit, kind::MORALE)), 1);
        h.set(EeCalamityOp::ValidTarget, yes());
        assert_eq!(can_cast_at(&mut h, &nature(EeCalamityCastMode::Unit, kind::MORALE)), 0);
        // the special attack ignores the area effects
        let mut h = allowed();
        h.set(EeCalamityOp::AreaEffect, yes());
        h.set(EeCalamityOp::TargetLookup, yes());
        h.set(EeCalamityOp::ValidTarget, yes());
        let c = EeCalamityCast {
            blocked_by_immunity: true,
            ..nature(EeCalamityCastMode::SpecialAttack, kind::SPECIAL_ATTACK)
        };
        assert_eq!(can_cast_at(&mut h, &c), 0);
        assert_eq!(can_cast_at(&mut h, &EeCalamityCast { mode: EeCalamityCastMode::Unit, ..c }), 2);
    }

    #[test]
    fn targets_by_class() {
        let flags = |f| EeCalamityReply { flags: f, ..Default::default() };
        let enemy =
            EE_CALAMITY_TARGET_ON_MAP | EE_CALAMITY_TARGET_ENEMY | EE_CALAMITY_TARGET_IN_SET | EE_CALAMITY_TARGET_BUILT;
        let check = |class_enum, f| {
            let mut h = Host::default();
            h.set(EeCalamityOp::TargetView, flags(f));
            valid_target(&mut h, &cast(EeCalamityCastMode::Unit, class_enum))
        };
        assert!(check(kind::LOGIC_BOMB, enemy));
        assert!(!check(kind::LOGIC_BOMB, enemy | EE_CALAMITY_TARGET_DISABLED));
        assert!(!check(kind::LOGIC_BOMB, enemy | EE_CALAMITY_TARGET_VOLCANO));
        assert!(check(kind::DROP_OBJECT, enemy | EE_CALAMITY_TARGET_DISABLED));
        assert!(!check(kind::DROP_OBJECT, enemy | EE_CALAMITY_TARGET_FOUNDATION));
        assert!(check(kind::SPECIAL_ATTACK, EE_CALAMITY_TARGET_ENEMY | EE_CALAMITY_TARGET_IN_SET));
        assert!(!check(
            kind::SPECIAL_ATTACK,
            EE_CALAMITY_TARGET_ENEMY | EE_CALAMITY_TARGET_IN_SET | EE_CALAMITY_TARGET_SAME_PLAYER
        ));
        let own = EE_CALAMITY_TARGET_ON_MAP | EE_CALAMITY_TARGET_SAME_PLAYER | EE_CALAMITY_TARGET_IN_SET;
        assert!(check(kind::CONSCRIPTION, own));
        assert!(!check(kind::CONSCRIPTION, own | EE_CALAMITY_TARGET_IS_CASTER));
        assert!(check(kind::TIME_WARP, own));
        assert!(!check(kind::TIME_WARP, own | EE_CALAMITY_TARGET_IS_CASTER));
        assert!(check(kind::MORALE, EE_CALAMITY_TARGET_ON_MAP | EE_CALAMITY_TARGET_IN_SET));
        assert!(!check(kind::MORALE, EE_CALAMITY_TARGET_IN_SET));
    }

    #[test]
    fn locations_by_class() {
        let base = cast(EeCalamityCastMode::Location, kind::AREA_EFFECT);
        // the area effects: the area of half the radius, rounded up, on the map
        let mut h = Host::default();
        h.set_tile(EeCalamityOp::AreaOnMap, 4, 5, yes());
        assert!(valid_location(&mut h, &EeCalamityCast { radius: 5, ..base }));
        // the earthquake on a space map: the caster's continent
        let mut h = Host::default();
        h.set(EeCalamityOp::ValidTile, yes());
        h.set(EeCalamityOp::SpaceMap, yes());
        h.set(EeCalamityOp::CasterContinent, value(3));
        h.set(EeCalamityOp::TileContinent, value(4));
        let quake = EeCalamityCast { class_enum: kind::EARTHQUAKE, ..base };
        assert!(!valid_location(&mut h, &quake));
        h.set(EeCalamityOp::TileContinent, value(3));
        assert!(valid_location(&mut h, &quake));
        // the hurricane: no other storm near
        let storm = EeCalamityCast { class_enum: kind::HURRICANE, area: 3, ..base };
        let mut h = Host::default();
        h.set(EeCalamityOp::AreaOnMap, yes());
        h.set(EeCalamityOp::UnitsNear, EeCalamityReply { value: 2, id: 2, ..Default::default() });
        assert!(valid_location(&mut h, &storm));
        h.replies.push((EeCalamityOp::NearStorm, Some((0, 0)), yes()));
        assert!(!valid_location(&mut h, &storm));
        h.set(EeCalamityOp::TerrainType, value(12));
        assert!(!valid_location(&mut h, &storm));
        assert!(!valid_location(&mut Host::default(), &EeCalamityCast { over_water: true, ..storm }));
        // the volcano: its whole footprint (size 3 at (4, 5): tiles 3..5 x 4..6), the edge refused
        let volcano = EeCalamityCast { class_enum: kind::VOLCANO, ..base };
        let mut h = Host::default();
        h.set(EeCalamityOp::TileSize, value(3));
        h.set(EeCalamityOp::TileExists, yes());
        h.set(EeCalamityOp::TileUnblocked, yes());
        assert!(valid_location(&mut h, &volcano));
        assert_eq!(h.log.iter().filter(|o| **o == EeCalamityOp::TileUnblocked).count(), 9);
        h.set_tile(EeCalamityOp::EdgeTile, 5, 6, yes());
        assert!(!valid_location(&mut h, &volcano));
        // the meteor storm: space tiles only
        let meteor = EeCalamityCast { class_enum: kind::METEOR_STORM, space_terrain: 9, ..base };
        let mut h = Host::default();
        h.set(EeCalamityOp::TerrainType, EeCalamityReply { ok: true, value: 9, ..Default::default() });
        assert!(valid_location(&mut h, &meteor));
        // the teleport: free and with room; the special location: a radio man needs a free tile
        let mut h = Host::default();
        h.set(EeCalamityOp::AreaOnMap, yes());
        assert!(valid_location(&mut h, &EeCalamityCast { class_enum: kind::TELEPORT, ..base }));
        let special = EeCalamityCast { class_enum: kind::SPECIAL_LOCATION, ..base };
        assert!(valid_location(&mut Host::default(), &special));
        let mut h = Host::default();
        h.set(EeCalamityOp::CasterIsRadioMan, yes());
        assert!(!valid_location(&mut h, &special));
    }
}
