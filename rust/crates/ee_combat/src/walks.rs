//! The target walks and screens (R5b.2, second round): the line of fire (`EEIsLineToUnitBlocked`), splash damage's
//! choice of victims (`EEUnitClass::ApplyAreaDamage`) and the new target search (`EEFindNewTarget` with the region
//! walk under it, `EEUUGetUnitsInArea`).
//!
//! The walks' order and arithmetic are here; the world stays in C++ behind a host (`LineHost`, `AreaHost`,
//! `TargetHost`; the FFI tables `EeCombat*Host` in `ffi.rs`). Units, tile nodes and the screen are opaque handles
//! (`u64`, the C++ pointers): this code compares them for identity and hands them back, never dereferences them.
//! No Rust reference lives across a host call: each call takes and returns plain values.

use crate::ceil;

/// An opaque C++ object (a unit, a tile node); 0 is none.
pub type Handle = u64;

// ---- EEIsLineToUnitBlocked

/// `EEIsLineToUnitBlocked`'s inputs (`inUnit` shooting at `inTarget`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatLineIn {
    pub from_x: i32,
    pub from_y: i32,
    pub to_x: i32,
    pub to_y: i32,
    /// the shooter's class `mSkipsLineOfFireCheck`
    pub skips_check: i32,
    /// the target's hit points are positive
    pub target_alive: i32,
    /// the shooter's class `mIsMelee`: the walls' bare test, no shot height
    pub bare: i32,
}

/// The target's top, as the C++ reads it (`EeCombatLineHost::target_top`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatTargetTop {
    /// the target is a complex unit (else nothing below is read)
    pub present: i32,
    /// the z of its model's bounding box's top right back corner at its frame
    pub box_top: f32,
    /// its stats' `GetScale`
    pub scale: f32,
    pub built: i32,
    pub hit_points: i32,
    pub max_hit_points: i32,
    /// its location's z
    pub z: f32,
}

/// The result: blocked, and the last tile reached.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatLineOut {
    pub blocked: i32,
    pub x: i32,
    pub y: i32,
}

/// The world `line_blocked` asks.
pub trait LineHost {
    /// The shot's start height: the shooter's z, or (not bare) its weapon tag point's z.
    fn start_height(&mut self, bare: bool) -> f32;
    /// `EEGetTileLine(from, to, inEnds 0, inThin 1)`.
    fn tile_line(&mut self, from_x: i32, from_y: i32, to_x: i32, to_y: i32) -> Vec<(i32, i32)>;
    fn target_top(&mut self) -> EeCombatTargetTop;
    /// `EEIsTileTopAbove(x, y, height, &blocker, bare)` (the host keeps the blocker for the caller).
    fn tile_top_above(&mut self, x: i32, y: i32, height: f32, bare: bool) -> bool;
}

/// `EEIsLineToUnitBlocked`: the walls on the tile line from the shooter to the target, at the shot's height rising
/// from the weapon to the target's top (a diagonal step is blocked by two pieces at its corners).
pub fn line_blocked(i: &EeCombatLineIn, host: &mut dyn LineHost) -> EeCombatLineOut {
    let bare = i.bare != 0;
    let mut blocked = false;
    let (mut x, mut y) = (0i32, 0i32);
    let distance =
        i.from_x.wrapping_sub(i.to_x).wrapping_abs().wrapping_add(i.from_y.wrapping_sub(i.to_y).wrapping_abs());
    if i.skips_check == 0 && distance > 1 && i.target_alive != 0 {
        let height = host.start_height(bare);
        let tiles = host.tile_line(i.from_x, i.from_y, i.to_x, i.to_y);
        let mut step = 0.0f32;
        if !bare {
            let t = host.target_top();
            if t.present != 0 {
                let mut top = t.box_top * t.scale;
                if t.built == 0 {
                    top *= t.hit_points as f32 / t.max_hit_points as f32;
                }
                top += t.z;
                // `(theTop - theHeight) / (theTiles.size() + 1)`: the count converted to float
                step = if !tiles.is_empty() { (top - height) / (tiles.len() + 1) as f32 } else { 0.0 };
            }
        }
        x = i.from_x;
        y = i.from_y;
        let mut z = height;
        for (n, &(tile_x, tile_y)) in tiles.iter().enumerate() {
            if blocked {
                break;
            }
            if !bare {
                z = (n + 1) as f32 * step + height;
            }
            if tile_x.wrapping_sub(x).wrapping_abs() == 1 && tile_y.wrapping_sub(y).wrapping_abs() == 1 {
                let corner = z - step * 0.5;
                if host.tile_top_above(tile_x, y, corner, bare) && host.tile_top_above(x, tile_y, corner, bare) {
                    blocked = true;
                    x = tile_x;
                    continue;
                }
            }
            blocked = host.tile_top_above(tile_x, tile_y, z, bare);
            x = tile_x;
            y = tile_y;
        }
        if !blocked
            && i.to_x.wrapping_sub(x).wrapping_abs() == 1
            && i.to_y.wrapping_sub(y).wrapping_abs() == 1
            && host.tile_top_above(i.to_x, y, z, bare)
            && host.tile_top_above(x, i.to_y, z, bare)
        {
            blocked = true;
            x = i.to_x;
        }
    }
    EeCombatLineOut { blocked: blocked as i32, x, y }
}

// ---- EEUnitClass::ApplyAreaDamage

/// `ApplyAreaDamage`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatAreaIn {
    /// the attacker's tile
    pub tile_x: i32,
    pub tile_y: i32,
    pub radius: f32,
    /// the damaging class's (the launcher's) `mFriendlyDamageFactor`
    pub friendly_factor: f32,
    /// the direct target's complex unit (counted as hit already), or 0 (a `Handle`)
    pub target: u64,
}

/// The world `area_victims` walks.
pub trait AreaHost {
    /// The first unit node of tile (x, y), 0 for none or no tile.
    fn tile_first(&mut self, x: i32, y: i32) -> Handle;
    /// The node after `node` (read after the previous victim's hit, as the C++ did), 0 at the end.
    fn node_next(&mut self, node: Handle) -> Handle;
    /// The node's unit's complex unit, 0 for none.
    fn node_unit(&mut self, node: Handle) -> Handle;
    /// `inUnit->DistanceSquaredTo(unit)`.
    fn distance_squared(&mut self, unit: Handle) -> f32;
    /// `EEUUGetIsUnitClassTargetable(inUnit, unit's class)`.
    fn targetable(&mut self, unit: Handle) -> bool;
    /// The unit is the attacker's player's.
    fn same_player(&mut self, unit: Handle) -> bool;
    /// The unit is a victim: the C++ applies the hit (the volcano's effect, the damage, kills and food).
    fn hit(&mut self, unit: Handle);
}

/// `ApplyAreaDamage`'s walk: every tile of the square of `ceil(radius)` around the attacker, column by column, each
/// tile's units in list order; a unit is a victim once, within the radius, targetable, and the own player's only
/// with a positive friendly factor. Each victim is hit before the walk goes on.
pub fn area_victims(i: &EeCombatAreaIn, host: &mut dyn AreaHost) {
    let mut hit: Vec<Handle> = Vec::new();
    let radius_squared = i.radius * i.radius;
    if i.target != 0 {
        hit.push(i.target);
    }
    let range = ceil(i.radius);
    let mut x = i.tile_x.wrapping_sub(range);
    while x <= i.tile_x.wrapping_add(range) {
        let mut y = i.tile_y.wrapping_sub(range);
        while y <= i.tile_y.wrapping_add(range) {
            let mut node = host.tile_first(x, y);
            while node != 0 {
                let unit = host.node_unit(node);
                if unit != 0
                    && !hit.contains(&unit)
                    && !(host.distance_squared(unit) > radius_squared)
                    && host.targetable(unit)
                    && !(host.same_player(unit) && !(i.friendly_factor > 0.0))
                {
                    hit.push(unit);
                    host.hit(unit);
                }
                node = host.node_next(node);
            }
            y = y.wrapping_add(1);
        }
        x = x.wrapping_add(1);
    }
}

// ---- EEFindNewTarget

/// The tests `find_new_target` asks of a candidate (`TargetHost::test`), in its order.
/// `EETechnology::mData->mKind` of the candidate class's technology in the unit's tree is 6 (asked only when the
/// call's `inArg6` is false; no technology: -1).
pub const TARGET_TEST_TECH_KIND_6: u32 = 1;
/// The candidate's owner is the unit's player or not its enemy.
pub const TARGET_TEST_NOT_ENEMY: u32 = 2;
/// The candidate is disabled and not protected.
pub const TARGET_TEST_DISABLED: u32 = 3;
/// The candidate's owner is player 0 (nature).
pub const TARGET_TEST_NATURE: u32 = 4;
/// `EEIsAttackedBy(unit, candidate)`.
pub const TARGET_TEST_ATTACKED_BY: u32 = 5;
/// `EECanTargetClass(unit, candidate's class)`.
pub const TARGET_TEST_CAN_TARGET_CLASS: u32 = 6;
/// The candidate `IsFoundation`.
pub const TARGET_TEST_FOUNDATION: u32 = 7;
/// `EEIsSeenBy(candidate, unit's player, true)`.
pub const TARGET_TEST_SEEN: u32 = 8;
/// The candidate's class `IsPartLeftOut(candidate)`.
pub const TARGET_TEST_PART_LEFT_OUT: u32 = 9;
/// `unit->CanAttackClass(candidate's class)`.
pub const TARGET_TEST_CAN_ATTACK_CLASS: u32 = 10;
/// The unit has ignored targets and `IsTargetIgnored(candidate, inArg7 != 0)` (which may drop expired ones).
pub const TARGET_TEST_IGNORED: u32 = 11;

/// `EEFindNewTarget`'s inputs.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCombatFindTargetIn {
    /// the area's tiles, inclusive (the tile less and plus the radius)
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    /// `inArg6`: skip the technology kind test
    pub skip_kind: i32,
}

/// The world `find_new_target` walks.
pub trait TargetHost {
    /// `mUnitRegions->GetRegions(area)`: the regions' rectangle (left, top, right, bottom).
    fn regions(&mut self, left: i32, top: i32, right: i32, bottom: i32) -> (i32, i32, i32, i32);
    /// The size of region (x, y), -1 for none.
    fn region_len(&mut self, x: i32, y: i32) -> i32;
    /// The `index`th unit of region (x, y).
    fn region_unit(&mut self, x: i32, y: i32, index: i32) -> Handle;
    /// The unit's tile.
    fn unit_tile(&mut self, unit: Handle) -> (i32, i32);
    /// The screen's `Virtual08(unit)`.
    fn screen(&mut self, unit: Handle) -> bool;
    /// The unit's class's `mTileSize > 1`.
    fn multiple(&mut self, unit: Handle) -> bool;
    /// The unit's id (`mUnitID`).
    fn unit_id(&mut self, unit: Handle) -> u32;
    /// One of the `TARGET_TEST_*` tests of a candidate.
    fn test(&mut self, test: u32, unit: Handle) -> bool;
}

/// `EEUUGetUnitsInArea`: the units of the area's unit regions (column by column, each region from its back) whose
/// tile is in the area and that pass the screen; sorted by id and made unique when one of a class bigger than a tile
/// was among them.
pub fn units_in_area(i: &EeCombatFindTargetIn, host: &mut dyn TargetHost) -> Vec<Handle> {
    let mut out = Vec::new();
    let (left, top, right, bottom) = host.regions(i.left, i.top, i.right, i.bottom);
    let mut multiple = false;
    let mut x = left;
    while x <= right {
        let mut y = top;
        while y <= bottom {
            let len = host.region_len(x, y);
            let mut n = len.wrapping_sub(1);
            while len >= 0 && n >= 0 {
                let unit = host.region_unit(x, y, n);
                let (tile_x, tile_y) = host.unit_tile(unit);
                let inside = tile_x >= i.left && tile_x <= i.right && tile_y >= i.top && tile_y <= i.bottom;
                if inside && host.screen(unit) {
                    if host.multiple(unit) {
                        multiple = true;
                    }
                    out.push(unit);
                }
                n -= 1;
            }
            y = y.wrapping_add(1);
        }
        x = x.wrapping_add(1);
    }
    if multiple {
        // `EESortUnique(EEUUUnitIDLess, EEUUUnitIDEqual)`: equal ids are the same unit, so any sort gives this
        let mut keyed: Vec<(u32, Handle)> = out.iter().map(|&u| (host.unit_id(u), u)).collect();
        keyed.sort_by_key(|&(id, _)| id);
        keyed.dedup_by_key(|&mut (id, _)| id);
        out = keyed.into_iter().map(|(_, u)| u).collect();
    }
    out
}

/// `EEFindNewTarget`'s choice among the candidates (in their order): the first enemy that is not disabled (nor an
/// animal that did not attack the unit), not of a kind-6 technology, targetable, no foundation, seen, not a part
/// left out, of a class the unit attacks and not ignored. Returns its index.
pub fn choose_target(candidates: &[Handle], skip_kind: bool, host: &mut dyn TargetHost) -> Option<usize> {
    // `theKind` keeps its last value: -1 until asked, never asked with inArg6
    let mut kind_6 = false;
    for (n, &unit) in candidates.iter().enumerate() {
        if !skip_kind {
            kind_6 = host.test(TARGET_TEST_TECH_KIND_6, unit);
        }
        if host.test(TARGET_TEST_NOT_ENEMY, unit) || host.test(TARGET_TEST_DISABLED, unit) {
            continue;
        }
        if host.test(TARGET_TEST_NATURE, unit) && !host.test(TARGET_TEST_ATTACKED_BY, unit) {
            continue;
        }
        if kind_6 || !host.test(TARGET_TEST_CAN_TARGET_CLASS, unit) {
            continue;
        }
        if host.test(TARGET_TEST_FOUNDATION, unit)
            || !host.test(TARGET_TEST_SEEN, unit)
            || host.test(TARGET_TEST_PART_LEFT_OUT, unit)
            || !host.test(TARGET_TEST_CAN_ATTACK_CLASS, unit)
        {
            continue;
        }
        if host.test(TARGET_TEST_IGNORED, unit) {
            continue;
        }
        return Some(n);
    }
    None
}
