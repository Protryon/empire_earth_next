//! `EEUnitClass`'s kind dispatchers that are more than a switch of calls (the units' lane, round 8; were
//! EEUnitClassKinds.cpp): `ActionChanged`, `Update`, `UpdateEffects`, `EvaluateCondition`, `HasArrived`, `HasRoom`,
//! `SelectUnitToRelease`, `FindBoardingLocation`, `GetSurroundingTiles` (the UCBD notes of the last five stay in the
//! C++ members). The class's kind bits and dispatch rows (kinds.rs, cls_dispatch.rs) choose; each body the choice
//! runs is one `EE_UNITS_UCK_*` case of `ee_units_uck_host` (EEUnitClassKinds.cpp; the class host's statements pass
//! through to EEUnitClass.cpp's).

use crate::units::actions::*;
use crate::units::cls_dispatch::*;
use crate::units::kinds::*;
use crate::units::uc_body::UcHost;
use crate::units::uc_create::S;
use crate::units::update::*;

// ---- the statements (2200..2399)
/// `EEUnitClassKinds(class)`
pub const EE_UNITS_UCK_KINDS: i32 = 2200;
/// `EEClassBody(class, b)`
pub const EE_UNITS_UCK_BODY: i32 = 2201;
/// `class->MissileActionChanged(unit)`
pub const EE_UNITS_UCK_MISSILE_ACTION_CHANGED: i32 = 2202;
/// `class->ProjectileActionChanged(unit)`
pub const EE_UNITS_UCK_PROJECTILE_ACTION_CHANGED: i32 = 2203;
/// `EEUnitClassActionChanged(b, class, unit)`
pub const EE_UNITS_UCK_ACTION_CHANGED: i32 = 2204;
/// `EEUnitClassUpdate(b, class, unit)`
pub const EE_UNITS_UCK_UPDATE: i32 = 2205;
/// `class->BombUpdate(unit)`
pub const EE_UNITS_UCK_BOMB_UPDATE: i32 = 2206;
/// `!unit->IsFoundation() || unit->mHitPoints <= 0 || unit->mCurrentPhysicsAction == 2`
pub const EE_UNITS_UCK_PRIEST_UPDATES: i32 = 2207;
/// `class->PriestVirtualB8(unit)`
pub const EE_UNITS_UCK_PRIEST_UPDATE: i32 = 2208;
/// `class->TorpedoUpdate(unit)`
pub const EE_UNITS_UCK_TORPEDO_UPDATE: i32 = 2209;
/// `class->UpdateCitizen(unit)`
pub const EE_UNITS_UCK_CITIZEN_UPDATE: i32 = 2210;
/// `class->MissileUpdate(unit)`
pub const EE_UNITS_UCK_MISSILE_UPDATE: i32 = 2211;
/// `class->ProjectileUpdate(unit)`
pub const EE_UNITS_UCK_PROJECTILE_UPDATE: i32 = 2212;
/// `class->mRippleEffect != NULL && unit->mUnitClass->mUnitType == kUnitTypeCatalina && unit->mHitPoints != 0 && class->mRippleEffect->mDetailLevel <= gEffectDetail` (`UpdateEffects` body 2)
pub const EE_UNITS_UCK_RIPPLES_DUE: i32 = 2213;
/// body 3's: `class->mRippleEffect != NULL && unit->mHitPoints != 0 && (unsigned long)class->mRippleEffect->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCK_RIPPLES_DUE_3: i32 = 2235;
/// body 4's: a dock's or navy yard's, `class->mRippleEffect != NULL && unit->mHitPoints != 0 && ...->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCK_RIPPLES_DUE_4: i32 = 2236;
/// body 5's: `class->mRippleEffect != NULL && unit->mHitPoints > 0 && ...->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCK_RIPPLES_DUE_5: i32 = 2237;
/// `class->UpdateRipples(unit)`
pub const EE_UNITS_UCK_UPDATE_RIPPLES: i32 = 2214;
/// `unit->mHitPoints > 0 && class->mTrailEffect != NULL && unit->mVelocity != 0.0f && class->mTrailEffect->mDetailLevel <= gEffectDetail`
pub const EE_UNITS_UCK_TRAIL_DUE: i32 = 2215;
/// `class->UpdateTrail(unit)`
pub const EE_UNITS_UCK_UPDATE_TRAIL: i32 = 2216;
/// `class->ProjectileUpdateEffects(unit)`
pub const EE_UNITS_UCK_PROJECTILE_EFFECTS: i32 = 2217;
/// `ee_units_cls_condition(class, unit, b)`
pub const EE_UNITS_UCK_CONDITION: i32 = 2218;
/// `EECombatCondition(EE_COMBAT_GOAL_TORPEDO, unit, b, EECombatShotTarget(unit), NULL, class)`
pub const EE_UNITS_UCK_TORPEDO_CONDITION: i32 = 2219;
/// `class->MissileEvaluateCondition(unit, b)`
pub const EE_UNITS_UCK_MISSILE_CONDITION: i32 = 2220;
/// `EECombatCondition(EE_COMBAT_GOAL_PROJECTILE, unit, b, EECombatShotTarget(unit), NULL, class)`
pub const EE_UNITS_UCK_PROJECTILE_CONDITION: i32 = 2221;
/// `ee_units_cls_landing_has_arrived(class, unit, f[0], b != 0)`
pub const EE_UNITS_UCK_LANDING_ARRIVED: i32 = 2222;
/// `class->SpaceBuildingHasArrived(unit, f[0])`
pub const EE_UNITS_UCK_SPACE_BUILDING_ARRIVED: i32 = 2223;
/// `ee_units_tr_naval_has_arrived(unit, f[0])`
pub const EE_UNITS_UCK_NAVAL_TRANSPORT_ARRIVED: i32 = 2224;
/// `class->NavalHasArrived(unit, f[0])`
pub const EE_UNITS_UCK_NAVAL_ARRIVED: i32 = 2225;
/// `class->SubmergedNavalHasArrived(unit, f[0])`
pub const EE_UNITS_UCK_SUBMERGED_ARRIVED: i32 = 2226;
/// `class->BaseHasArrived(unit, f[0])`
pub const EE_UNITS_UCK_BASE_ARRIVED: i32 = 2227;
/// `class->FortressHasRoom(unit, complex unit a)`
pub const EE_UNITS_UCK_FORTRESS_HAS_ROOM: i32 = 2228;
/// `class->NavalBuildingSelectUnitToRelease(unit)`
pub const EE_UNITS_UCK_NAVAL_SELECT: i32 = 2229;
/// `class->BuildingSelectUnitToRelease(unit)`
pub const EE_UNITS_UCK_BUILDING_SELECT: i32 = 2230;
/// `class->TransportFindBoardingLocation(unit, complex unit a, the point at b, the point at c)`
pub const EE_UNITS_UCK_BOARDING_LOCATION: i32 = 2231;
/// `class->GateGetSurroundingTiles(unit, the tiles at a, b, c != 0)`
pub const EE_UNITS_UCK_GATE_SURROUNDING: i32 = 2232;
/// `class->BuildingGetSurroundingTiles(unit, the tiles at a, b, c != 0)`
pub const EE_UNITS_UCK_BUILDING_SURROUNDING: i32 = 2233;
/// `class->BaseGetSurroundingTiles(unit, the tiles at a, b, c != 0)`
pub const EE_UNITS_UCK_BASE_SURROUNDING: i32 = 2234;

// ---- the bodies (`ee_units_uck_call`'s `what`)
pub const EE_UNITS_UCK_DO_ACTION_CHANGED: i32 = 1;
pub const EE_UNITS_UCK_DO_UPDATE: i32 = 2;
pub const EE_UNITS_UCK_DO_UPDATE_EFFECTS: i32 = 3;
pub const EE_UNITS_UCK_DO_EVALUATE_CONDITION: i32 = 4;
pub const EE_UNITS_UCK_DO_HAS_ARRIVED: i32 = 5;
pub const EE_UNITS_UCK_DO_HAS_ROOM: i32 = 6;
pub const EE_UNITS_UCK_DO_SELECT_TO_RELEASE: i32 = 7;
pub const EE_UNITS_UCK_DO_BOARDING_LOCATION: i32 = 8;
pub const EE_UNITS_UCK_DO_SURROUNDING_TILES: i32 = 9;

/// `ActionChanged`'s body kinds (actions.rs) by its dispatch row's answer; 15 and 17 the shots' members
const ACTIONS: [i32; 20] = [
    EE_UNITS_ACTION_BASE,
    EE_UNITS_ACTION_AIR_ANIMAL,
    EE_UNITS_ACTION_AIRPLANE,
    EE_UNITS_ACTION_BUILDING_PRIEST,
    EE_UNITS_ACTION_GATE,
    EE_UNITS_ACTION_HELICOPTER_PRIEST,
    EE_UNITS_ACTION_HELICOPTER_TRANSPORT,
    EE_UNITS_ACTION_MINING_BUILDING,
    EE_UNITS_ACTION_PLANET_SPACESHIP,
    EE_UNITS_ACTION_CITIZEN,
    EE_UNITS_ACTION_BASIC_PRIEST,
    EE_UNITS_ACTION_BUILDING,
    EE_UNITS_ACTION_HELICOPTER,
    EE_UNITS_ACTION_LAND,
    EE_UNITS_ACTION_LANDABLE,
    EE_UNITS_ACTION_BASE,
    EE_UNITS_ACTION_NAVAL,
    EE_UNITS_ACTION_BASE,
    EE_UNITS_ACTION_SPACESHIP,
    EE_UNITS_ACTION_SUBMERGED_NAVAL,
];

/// `Update`'s sequences (update.rs) by its dispatch row's answer; -1 the members
const UPDATES: [i32; 27] = [
    -1,
    EE_UNITS_UPDATE_AIR_ANIMAL,
    EE_UNITS_UPDATE_FORTRESS,
    -1,
    -1,
    EE_UNITS_UPDATE_GATE,
    EE_UNITS_UPDATE_HELICOPTER_TRANSPORT,
    EE_UNITS_UPDATE_LAND_CITIZEN,
    EE_UNITS_UPDATE_LAND_TRANSPORT,
    EE_UNITS_UPDATE_MINING_BUILDING,
    EE_UNITS_UPDATE_NAVAL_BUILDING,
    EE_UNITS_UPDATE_NAVAL_CITIZEN,
    EE_UNITS_UPDATE_SPACE_BUILDING,
    -1,
    EE_UNITS_UPDATE_AMBIENT,
    -1,
    EE_UNITS_UPDATE_BASIC_PRIEST,
    EE_UNITS_UPDATE_BUILDING,
    EE_UNITS_UPDATE_HELICOPTER,
    EE_UNITS_UPDATE_LAND,
    EE_UNITS_UPDATE_LANDABLE,
    -1,
    EE_UNITS_UPDATE_NAVAL,
    -1,
    EE_UNITS_UPDATE_SPACESHIP,
    EE_UNITS_UPDATE_SUBMERGED_NAVAL,
    EE_UNITS_UPDATE_TRANSPORT,
];

fn body<H: UcHost + ?Sized>(s: &mut S<'_, H>, row: usize) -> usize {
    s.b(EE_UNITS_UCK_BODY, row as i64) as usize
}

fn kinds<H: UcHost + ?Sized>(s: &mut S<'_, H>) -> u64 {
    s.s(EE_UNITS_UCK_KINDS) as u64
}

/// `ActionChanged(unit)`: the body of the class's kind.
pub fn action_changed<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    match body(&mut s, EE_UNITS_BODY_ACTION_CHANGED) {
        15 => s.s(EE_UNITS_UCK_MISSILE_ACTION_CHANGED),
        17 => s.s(EE_UNITS_UCK_PROJECTILE_ACTION_CHANGED),
        b => s.b(EE_UNITS_UCK_ACTION_CHANGED, i64::from(ACTIONS[b])),
    };
}

/// `Update(unit)`: the sequence or member of the class's kind.
pub fn update<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    let b = body(&mut s, EE_UNITS_BODY_UPDATE);
    if UPDATES[b] >= 0 {
        s.b(EE_UNITS_UCK_UPDATE, i64::from(UPDATES[b]));
        return;
    }
    match b {
        3 => {
            s.s(EE_UNITS_UCK_BOMB_UPDATE);
        }
        4 => {
            if s.s(EE_UNITS_UCK_PRIEST_UPDATES) != 0 {
                s.s(EE_UNITS_UCK_PRIEST_UPDATE);
            }
        }
        13 => {
            s.s(EE_UNITS_UCK_TORPEDO_UPDATE);
        }
        15 => {
            s.s(EE_UNITS_UCK_CITIZEN_UPDATE);
        }
        21 => {
            s.s(EE_UNITS_UCK_MISSILE_UPDATE);
        }
        23 => {
            s.s(EE_UNITS_UCK_PROJECTILE_UPDATE);
        }
        _ => {}
    }
}

/// `UpdateEffects(unit)`: the ripples (bodies 2..5, each with its own condition), the trail (6) or the projectile's
/// effects (7); the others have none.
pub fn update_effects<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    match body(&mut s, EE_UNITS_BODY_UPDATE_EFFECTS) {
        b @ 2..=5 => {
            let due = [
                EE_UNITS_UCK_RIPPLES_DUE,
                EE_UNITS_UCK_RIPPLES_DUE_3,
                EE_UNITS_UCK_RIPPLES_DUE_4,
                EE_UNITS_UCK_RIPPLES_DUE_5,
            ];
            if s.s(due[b - 2]) != 0 {
                s.s(EE_UNITS_UCK_UPDATE_RIPPLES);
            }
        }
        6 => {
            if s.s(EE_UNITS_UCK_TRAIL_DUE) != 0 {
                s.s(EE_UNITS_UCK_UPDATE_TRAIL);
            }
        }
        7 => {
            s.s(EE_UNITS_UCK_PROJECTILE_EFFECTS);
        }
        _ => {}
    }
}

/// `EvaluateCondition(unit, event)`: the kinds' answer (cls_condition.rs), else the shots' (-1 torpedo, -2 missile,
/// -3 projectile).
pub fn evaluate_condition<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, event: i64) -> i64 {
    let mut s = S { h, class, unit };
    let answer = s.b(EE_UNITS_UCK_CONDITION, event) as i32;
    match answer {
        -1 => s.b(EE_UNITS_UCK_TORPEDO_CONDITION, event),
        -2 => s.b(EE_UNITS_UCK_MISSILE_CONDITION, event),
        -3 => s.b(EE_UNITS_UCK_PROJECTILE_CONDITION, event),
        _ => i64::from(answer),
    }
}

/// `HasArrived(unit, value)`: the test of the class's deepest kind with one.
pub fn has_arrived<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, value: f32) -> bool {
    let mut s = S { h, class, unit };
    let k = kinds(&mut s);
    let arrived = |s: &mut S<'_, H>, op: i32, b: i64| s.f(op, 0, b, 0, &mut [value, 0.0, 0.0, 0.0]) != 0;
    if k & EE_CLASS_KIND_AIR_ANIMAL != 0 {
        return arrived(&mut s, EE_UNITS_UCK_LANDING_ARRIVED, 1);
    }
    if k & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING) != 0 {
        return arrived(&mut s, EE_UNITS_UCK_SPACE_BUILDING_ARRIVED, 0);
    }
    if k & EE_CLASS_KIND_NAVAL_TRANSPORT != 0 {
        return arrived(&mut s, EE_UNITS_UCK_NAVAL_TRANSPORT_ARRIVED, 0);
    }
    if k & EE_CLASS_KIND_LANDABLE != 0 {
        return arrived(&mut s, EE_UNITS_UCK_LANDING_ARRIVED, 0);
    }
    if k & EE_CLASS_KIND_NAVAL != 0 {
        return arrived(&mut s, EE_UNITS_UCK_NAVAL_ARRIVED, 0);
    }
    if k & EE_CLASS_KIND_SUBMERGED_NAVAL != 0 {
        return arrived(&mut s, EE_UNITS_UCK_SUBMERGED_ARRIVED, 0);
    }
    arrived(&mut s, EE_UNITS_UCK_BASE_ARRIVED, 0)
}

/// `HasRoom(unit, cargo)`: a fortress's test; any transport has room; the others none.
pub fn has_room<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, cargo: u64) -> bool {
    let mut s = S { h, class, unit };
    let k = kinds(&mut s);
    if k & EE_CLASS_KIND_FORTRESS != 0 {
        return s.a(EE_UNITS_UCK_FORTRESS_HAS_ROOM, cargo) != 0;
    }
    k & (EE_CLASS_KIND_HELICOPTER_TRANSPORT
        | EE_CLASS_KIND_LAND_TRANSPORT
        | EE_CLASS_KIND_NAVAL_TRANSPORT
        | EE_CLASS_KIND_SPACE_TRANSPORT)
        != 0
}

/// `SelectUnitToRelease(unit)`: the naval and space buildings' choice, the buildings', else none.
pub fn select_to_release<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> bool {
    let mut s = S { h, class, unit };
    let k = kinds(&mut s);
    if k & (EE_CLASS_KIND_NAVAL_BUILDING | EE_CLASS_KIND_SPACE_BUILDING) != 0 {
        return s.s(EE_UNITS_UCK_NAVAL_SELECT) != 0;
    }
    if k & EE_CLASS_KIND_BUILDING != 0 {
        return s.s(EE_UNITS_UCK_BUILDING_SELECT) != 0;
    }
    false
}

/// `FindBoardingLocation(unit, transport, location, out)`: a transport's, else none (`location`, `out` the caller's
/// points).
pub fn boarding_location<H: UcHost + ?Sized>(
    h: &mut H,
    class: u64,
    unit: u64,
    transport: u64,
    location: i64,
    out: i64,
) -> bool {
    let mut s = S { h, class, unit };
    if kinds(&mut s) & EE_CLASS_KIND_TRANSPORT != 0 {
        return s.abc(EE_UNITS_UCK_BOARDING_LOCATION, transport, location, out) != 0;
    }
    false
}

/// `GetSurroundingTiles(unit, tiles, corners, append)`: the gate's, the building's or the base body.
pub fn surrounding_tiles<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, tiles: u64, corners: i64, append: i64) {
    let mut s = S { h, class, unit };
    let k = kinds(&mut s);
    let op = if k & EE_CLASS_KIND_GATE != 0 {
        EE_UNITS_UCK_GATE_SURROUNDING
    } else if k & EE_CLASS_KIND_BUILDING != 0 {
        EE_UNITS_UCK_BUILDING_SURROUNDING
    } else {
        EE_UNITS_UCK_BASE_SURROUNDING
    };
    s.abc(op, tiles, corners, append);
}

/// Runs dispatcher `what` (`EE_UNITS_UCK_DO_*`) of `class` on `unit` with its arguments; the answer, or 0.
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: f32) -> i64 {
    match what {
        EE_UNITS_UCK_DO_ACTION_CHANGED => action_changed(h, class, unit),
        EE_UNITS_UCK_DO_UPDATE => update(h, class, unit),
        EE_UNITS_UCK_DO_UPDATE_EFFECTS => update_effects(h, class, unit),
        EE_UNITS_UCK_DO_EVALUATE_CONDITION => return evaluate_condition(h, class, unit, b),
        EE_UNITS_UCK_DO_HAS_ARRIVED => return i64::from(has_arrived(h, class, unit, f)),
        EE_UNITS_UCK_DO_HAS_ROOM => return i64::from(has_room(h, class, unit, a)),
        EE_UNITS_UCK_DO_SELECT_TO_RELEASE => return i64::from(select_to_release(h, class, unit)),
        EE_UNITS_UCK_DO_BOARDING_LOCATION => return i64::from(boarding_location(h, class, unit, a, b, c)),
        EE_UNITS_UCK_DO_SURROUNDING_TILES => surrounding_tiles(h, class, unit, a, b, c),
        _ => {}
    }
    0
}

#[cfg(not(test))]
mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_uck_host(
            op: i32,
            class: *mut c_void,
            unit: *mut c_void,
            a: u64,
            b: i64,
            c: i64,
            f: *mut [f32; 4],
        ) -> i64;
    }

    struct GameK;

    impl UcHost for GameK {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the dispatcher runs inside the C++ member that handed over its live class and unit
            unsafe { ee_units_uck_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The kind dispatcher `what` (`EE_UNITS_UCK_DO_*`) of `class` on `unit`: `a` an object (a cargo, a transport,
    /// a tile vector), `b`, `c` values or the caller's points, `f` a value; the answer, or 0.
    ///
    /// # Safety
    /// `class` is a live `EEUnitClass`, `unit` a live `EEComplexUnit` (or NULL), on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_uck_call(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: f32,
    ) -> i64 {
        run(&mut GameK, what, class as usize as u64, unit as usize as u64, a, b, c, f)
    }
}

#[cfg(test)]
#[path = "uck_kinds_tests.rs"]
mod tests;
