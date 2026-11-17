//! The effects' plain reads of their unit through `ee_sim::world`'s views (the object plan's Lane R): what
//! `EEEffectHost.cpp`'s `Ask` answered with a field or two of the unit, its class, stats or owner. The 3D and 2D FFI
//! hosts (effect_rising.rs, effect_2d.rs) carry the effect's unit (`mUnit`) and the current player and answer these
//! here; the calls into C++ objects stay `Ask`'s.

use std::os::raw::{c_long, c_ulong};

use ee_base::math::types::GeTransformation;
use ee_sim::world::{PlayerView, UnitView, WorldRead};
use ee_sim::world_field;

use crate::gfx::effect_farm::EeGfxFarmUnit;
use crate::gfx::effect_nuclear::EeGfxUnitTile;
use crate::gfx::effect_vent::EeGfxVentUnit;
use crate::gfx::effects::EeGfxColor;

const EE_GFX_FILL_ACTION_DEAD: u32 = 6003;
const EE_GFX_FILL_TYPE_MARKET_CP: u64 = 0x321;

/// The effect's unit (`unit`, an `EEUnit*`, 0: none) and the current player (`player`) under a token.
///
/// # Safety
/// The caller holds the world; the handles are live or 0.
unsafe fn with<R>(unit: u64, player: u64, f: impl FnOnce(Option<UnitView<'_>>, Option<PlayerView<'_>>) -> R) -> R {
    // SAFETY: the function's contract
    unsafe {
        let t = WorldRead::begin();
        f(UnitView::of(&t, unit), PlayerView::of(&t, player))
    }
}

fn the(unit: Option<UnitView<'_>>) -> UnitView<'_> {
    unit.expect("the effect's unit")
}

fn complex(unit: Option<UnitView<'_>>) -> UnitView<'_> {
    the(unit).complex_unit().expect("the effect's unit's complex unit")
}

/// A cloaked complex unit of another player, the current player's enemy (`EEE3DCitizenBeam::Step`, the energy
/// wave's Queue: the unit's cloak only when it can cloak).
fn hidden(c: UnitView<'_>, current: Option<PlayerView<'_>>) -> bool {
    let cloaked = if c.cloakable() { c.cloaked() } else { false };
    if !cloaked {
        return false;
    }
    let owner = c.player().expect("a unit's owner");
    let current = current.expect("a current player");
    owner.handle() != current.handle() && current.enemy(owner.id() as usize) != 0
}

/// `mUnit->mStats->GetScale()` (also the 2D `mStats->mScale`)
///
/// # Safety
/// As [`with`].
pub unsafe fn unit_scale(unit: u64) -> f32 {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| the(u).stats().expect("a unit's stats").scale()) }
}

/// `mUnit->mComplexUnit->mPhysicalModel.GetLocation()`
///
/// # Safety
/// As [`with`].
pub unsafe fn unit_location(unit: u64) -> GeTransformation {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| GeTransformation { m: complex(u).transform() }) }
}

/// The citizen beam's unit (`dynamic_cast<EEComplexUnit*>(mUnit)`) is hidden.
///
/// # Safety
/// As [`with`].
pub unsafe fn beam_hidden(unit: u64, player: u64) -> bool {
    // SAFETY: the function's contract
    unsafe {
        with(unit, player, |u, p| {
            let u = the(u);
            debug_assert!(u.is_complex(), "the beam's unit is a complex unit");
            hidden(u, p)
        })
    }
}

/// The energy wave's unit (`mUnit->mComplexUnit`) is hidden.
///
/// # Safety
/// As [`with`].
pub unsafe fn unit_hidden(unit: u64, player: u64) -> bool {
    // SAFETY: the function's contract
    unsafe { with(unit, player, |u, p| hidden(complex(u), p)) }
}

/// `mUnit->mComplexUnit->mPlayer->mColor`
///
/// # Safety
/// As [`with`].
pub unsafe fn player_color(unit: u64) -> EeGfxColor {
    // SAFETY: the function's contract
    unsafe {
        with(unit, 0, |u, _| {
            let c = complex(u).player().expect("a unit's owner").color();
            EeGfxColor { red: c[0], green: c[1], blue: c[2] }
        })
    }
}

/// The nuclear explosion's place off the map: whether the unit is on it, its class's tile size, its tile.
///
/// # Safety
/// As [`with`].
pub unsafe fn unit_tile(unit: u64) -> EeGfxUnitTile {
    // SAFETY: the function's contract
    unsafe {
        with(unit, 0, |u, _| {
            let u = the(u);
            let size: c_long = world_field!(u.class().expect("a unit's class").fields(), mTileSize);
            let (x, y) = u.tile_xy();
            EeGfxUnitTile { on_map: u.has_tile_node(), size: size as f32, x: x as c_long as f32, y: y as c_long as f32 }
        })
    }
}

/// The damage vent's unit: dead (its complex unit's command), its stats' maximum hit points.
///
/// # Safety
/// As [`with`].
pub unsafe fn vent_unit(unit: u64) -> EeGfxVentUnit {
    // SAFETY: the function's contract
    unsafe {
        with(unit, 0, |u, _| {
            let max = the(u).stats().expect("a unit's stats").max_hit_points() as c_long;
            EeGfxVentUnit { dead: complex(u).command_type() == EE_GFX_FILL_ACTION_DEAD, max_hit_points: max as f32 }
        })
    }
}

/// `mUnit->GetClass()->GetScale()`
///
/// # Safety
/// As [`with`].
pub unsafe fn unit_class_scale(unit: u64) -> f32 {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| world_field!(the(u).class().expect("a unit's class").fields(), mScale)) }
}

/// The icon is not shown over an unbuilt market: the unit is a complex unit of the market type, neither built nor a
/// foundation.
///
/// # Safety
/// As [`with`].
pub unsafe fn market_unbuilt(unit: u64) -> bool {
    // SAFETY: the function's contract
    unsafe {
        with(unit, 0, |u, _| {
            let Some(u) = u.filter(|u| u.is_complex()) else { return false };
            let foundation: bool = world_field!(u.complex(), mIsFoundation);
            u.class().expect("a unit's class").unit_type() == EE_GFX_FILL_TYPE_MARKET_CP && !u.is_built() && !foundation
        })
    }
}

/// The farm's unit: its tile, its class's tile size, its complex unit's build state and crop variant (`mCloaked`).
///
/// # Safety
/// As [`with`].
pub unsafe fn farm_unit(unit: u64) -> EeGfxFarmUnit {
    // SAFETY: the function's contract
    unsafe {
        with(unit, 0, |u, _| {
            let unit = the(u);
            let c = complex(u);
            let (x, y) = unit.tile_xy();
            let size: c_long = world_field!(unit.class().expect("a unit's class").fields(), mTileSize);
            let variant: c_ulong = world_field!(c.complex(), mCloaked);
            EeGfxFarmUnit {
                tile_x: x as i32,
                tile_y: y as i32,
                tile_size: size as i32,
                built: c.is_built(),
                cloaked: variant as i32,
            }
        })
    }
}

/// `mUnit->mUnitClass->mSelectionSize`
///
/// # Safety
/// As [`with`].
pub unsafe fn selection_size(unit: u64) -> f32 {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| world_field!(the(u).class().expect("a unit's class").fields(), mSelectionSize)) }
}

/// An attack's target belongs to another player, not player 0, the current player's enemy.
///
/// # Safety
/// As [`with`].
pub unsafe fn confirm_enemy(unit: u64, player: u64) -> bool {
    // SAFETY: the function's contract
    unsafe {
        with(unit, player, |u, p| {
            let owner = the(u).player().expect("a unit's owner");
            let current = p.expect("a current player");
            owner.handle() != current.handle() && current.enemy(owner.id() as usize) != 0 && owner.id() != 0
        })
    }
}

/// The effect has a unit with a complex unit.
///
/// # Safety
/// As [`with`].
pub unsafe fn has_complex(unit: u64) -> bool {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| u.is_some_and(|u| u.complex_unit_handle() != 0)) }
}

/// `mUnit->mStats->mMaxHitPoints`
///
/// # Safety
/// As [`with`].
pub unsafe fn max_hit_points(unit: u64) -> i32 {
    // SAFETY: the function's contract
    unsafe { with(unit, 0, |u, _| the(u).stats().expect("a unit's stats").max_hit_points() as i32) }
}

/// The marching icons' owner: the complex unit's player is the current player and it is built.
///
/// # Safety
/// As [`with`].
pub unsafe fn icons_owner(unit: u64, player: u64) -> bool {
    // SAFETY: the function's contract
    unsafe {
        with(unit, player, |u, p| {
            let c = complex(u);
            c.player().map(|o| o.handle()) == p.map(|p| p.handle()) && c.is_built()
        })
    }
}
