//! The calamity hosts' plain reads answered from the world views (calamities lane, round 4): what the update host
//! (EECalamityUpdate.cpp) read of the calamity's target and caster, what the cast tests' host (EECalamityCast.cpp)
//! read of the caster, and the map tiles' terrain. The C++ hosts answer the three handles
//! ([`EeCalamityOp::TargetHandle`], [`EeCalamityOp::CasterHandle`], [`EeCalamityOp::WorldHandle`]) and every world
//! call; a read whose object is missing goes on to the C++ (which read through it as the original did).

#[cfg(not(test))]
use crate::tech::calamity_update::EeCalamityCall;
use crate::tech::calamity_update::{EeCalamityOp, EeCalamityReply};

/// What a reply of the views holds for `op` from the facts read ([`Facts`]); None for an op the views do not answer.
pub fn reply(op: EeCalamityOp, f: &Facts) -> Option<EeCalamityReply> {
    use EeCalamityOp::*;
    let mut r = EeCalamityReply::default();
    match op {
        TargetExists => r.ok = f.target.is_some(),
        CasterExists => r.ok = f.caster.is_some(),
        TargetHitPoints => r.value = f.target?.complex?.hit_points as i32,
        TargetTinted => r.ok = f.target?.complex?.tinted,
        CasterHitPoints => r.value = f.caster?.hit_points as i32,
        CasterPower => r.value = f.caster?.complex?.power as i32,
        TargetDead => r.ok = f.target.is_some_and(|t| t.complex.is_some_and(|c| c.hit_points <= 0)),
        TargetAlive => r.ok = f.target.is_some_and(|t| t.hit_points > 0),
        TargetComplexAlive => r.ok = f.target.is_some_and(|t| t.complex.is_some_and(|c| c.hit_points > 0)),
        TileExists => r.ok = f.terrain.is_some(),
        TileTerrain | TerrainType => {
            r.ok = f.terrain.is_some();
            r.value = f.terrain.unwrap_or(0) as i32;
        }
        _ => return None,
    }
    Some(r)
}

/// The ops [`reply`] answers, and what each needs read.
fn needs(op: EeCalamityOp) -> Option<(bool, bool, bool)> {
    use EeCalamityOp::*;
    Some(match op {
        TargetExists | TargetHitPoints | TargetTinted | TargetDead | TargetAlive | TargetComplexAlive => {
            (true, false, false)
        }
        CasterExists | CasterHitPoints | CasterPower => (false, true, false),
        TileExists | TileTerrain | TerrainType => (false, false, true),
        _ => return None,
    })
}

/// A unit's members the replies read.
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct UnitFacts {
    pub hit_points: i64,
    pub complex: Option<ComplexFacts>,
}

/// A complex unit's (`mComplexUnit`'s).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct ComplexFacts {
    pub hit_points: i64,
    pub tinted: bool,
    pub power: i64,
}

/// What a call's reply reads: the target, the caster, the tile's terrain type (None: no such tile).
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Facts {
    pub target: Option<UnitFacts>,
    pub caster: Option<UnitFacts>,
    pub terrain: Option<i64>,
}

/// The views' answer to `call`, the C++ host asked for the handles through `handle(op)` (its reply's key); None:
/// the C++ answers.
///
/// # Safety
/// Inside a call from C++ on the simulation thread; the handles the host gives are live objects.
#[cfg(not(test))]
pub unsafe fn answer(handle: &mut dyn FnMut(EeCalamityOp) -> u64, call: &EeCalamityCall) -> Option<EeCalamityReply> {
    use crate::world::{UnitView, WorldRead, WorldView};
    let (target, caster, tile) = needs(call.op)?;
    // SAFETY: the caller's contract
    let t = unsafe { WorldRead::begin() };
    let unit = |u: Option<UnitView>| {
        u.map(|u| UnitFacts {
            hit_points: u.hit_points(),
            complex: u.complex_unit().map(|c| ComplexFacts {
                hit_points: c.hit_points(),
                tinted: crate::world_field!(c.complex(), mIsTinted),
                power: c.power(),
            }),
        })
    };
    let mut f = Facts::default();
    if target {
        // SAFETY: as above
        f.target = unit(unsafe { UnitView::of(&t, handle(EeCalamityOp::TargetHandle)) });
    }
    if caster {
        // SAFETY: as above
        f.caster = unit(unsafe { UnitView::of(&t, handle(EeCalamityOp::CasterHandle)) });
    }
    if tile {
        // SAFETY: as above (`&gWorld`)
        let w = unsafe { WorldView::of(&t, handle(EeCalamityOp::WorldHandle)) }?;
        // the tile (index, index2) as `GetTile` takes them: longs, compared unsigned
        f.terrain = w.map()?.tile(call.index as i64, call.index2 as i64).map(|t| t.terrain_type());
    }
    reply(call.op, &f)
}

#[cfg(test)]
mod tests {
    use super::*;
    use EeCalamityOp::*;

    fn unit(hp: i64, complex: Option<(i64, bool, i64)>) -> Option<UnitFacts> {
        Some(UnitFacts {
            hit_points: hp,
            complex: complex.map(|(h, tinted, power)| ComplexFacts { hit_points: h, tinted, power }),
        })
    }

    #[test]
    fn the_target_replies_read_the_unit_and_its_complex_unit() {
        let f = Facts { target: unit(0, Some((-3, true, 0))), ..Default::default() };
        assert!(reply(TargetExists, &f).unwrap().ok);
        assert_eq!(reply(TargetHitPoints, &f).unwrap().value, -3);
        assert!(reply(TargetTinted, &f).unwrap().ok);
        assert!(reply(TargetDead, &f).unwrap().ok);
        assert!(!reply(TargetAlive, &f).unwrap().ok);
        assert!(!reply(TargetComplexAlive, &f).unwrap().ok);
        let none = Facts::default();
        assert!(!reply(TargetExists, &none).unwrap().ok && !reply(TargetDead, &none).unwrap().ok);
        assert_eq!(reply(TargetHitPoints, &none), None, "the C++ reads through a missing target as it did");
        assert_eq!(reply(TargetTinted, &Facts { target: unit(5, None), ..Default::default() }), None);
    }

    #[test]
    fn the_caster_and_tile_replies() {
        let f = Facts { caster: unit(40, Some((40, false, 25))), terrain: Some(10), ..Default::default() };
        assert_eq!(reply(CasterHitPoints, &f).unwrap().value, 40);
        assert_eq!(reply(CasterPower, &f).unwrap().value, 25);
        assert!(reply(CasterExists, &f).unwrap().ok);
        let tile = reply(TileTerrain, &f).unwrap();
        assert!(tile.ok && tile.value == 10);
        let off = reply(TerrainType, &Facts::default()).unwrap();
        assert!(!off.ok && off.value == 0);
        assert!(!reply(TileExists, &Facts::default()).unwrap().ok);
        assert_eq!(reply(Random, &f), None);
        assert_eq!(needs(Shake), None);
    }
}
