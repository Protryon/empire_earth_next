//! The unit utilities' target searches over their C++ statements (the units' lane, round 8; were
//! EEFOFindNewTargetNearOldTarget.cpp): `EESearchTarget`, `EESearchTargetAlongList`, `EEGetPursuerOf` and
//! `EEFindNewTarget`, with their candidates' tests, their inputs and their capture records (CBST, CBSL, CBPU, CBFT).
//! The walks and scores are ee_combat's (`search_target`, `search_along_list`, `pursuer_of`, `units_in_area`,
//! `choose_target` over the views); the tests' reads, the searching unit's calls and the retarget are one
//! `EE_UNITS_SR_*` statement each of `ee_units_sr_host` (EEFOFindNewTargetNearOldTarget.cpp), asked in the
//! original's order. Handles are the C++ pointers (a region's units are complex units; a list's are `EEUnit`s whose
//! `mComplexUnit` may be missing).

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::c_long;

use crate::app::capture_record::Record;
use crate::combat::search::*;
use crate::combat::search_ffi::SearchCallbacks;
use crate::combat::walks::{
    EeCombatFindTargetIn, TARGET_TEST_ATTACKED_BY, TARGET_TEST_CAN_ATTACK_CLASS, TARGET_TEST_CAN_TARGET_CLASS,
    TARGET_TEST_DISABLED, TARGET_TEST_FOUNDATION, TARGET_TEST_IGNORED, TARGET_TEST_NOT_ENEMY,
    TARGET_TEST_PART_LEFT_OUT, TARGET_TEST_SEEN, TARGET_TEST_TECH_KIND_6,
};
use crate::combat::walks_ffi::TargetCallbacks;

// ---- the statements (`unit` the searching complex unit, `a` an object, `c` a value or a pointer, `f` floats)
/// unit `a`'s `mPlayer == unit->mPlayer`
pub const EE_UNITS_SR_SAME_PLAYER: i32 = 1;
/// `unit->mPlayer->mEnemies[unit a's mPlayer->GetPlayerID()] != 0`
pub const EE_UNITS_SR_ENEMY: i32 = 2;
/// complex unit `a`'s `mHitPoints`
pub const EE_UNITS_SR_HIT_POINTS: i32 = 3;
/// complex unit `a`'s `mIsDisabled`
pub const EE_UNITS_SR_DISABLED: i32 = 4;
/// complex unit `a`'s `IsProtected()`
pub const EE_UNITS_SR_PROTECTED: i32 = 5;
/// `EECombatWorld(EE_COMBAT_WORLD_ATTACKED_BY, unit, unit a) != 0`
pub const EE_UNITS_SR_ATTACKED_BY: i32 = 6;
/// unit `a`'s `GetUnitClass()`
pub const EE_UNITS_SR_CLASS: i32 = 7;
/// complex unit `a`'s `IsFoundation()`
pub const EE_UNITS_SR_IS_FOUNDATION: i32 = 8;
/// `EECombatIsSeenBy(complex unit a, unit->GetPlayer(), true)`
pub const EE_UNITS_SR_SEEN: i32 = 9;
/// `unit->IsOnOtherContinentThan(complex unit a)`
pub const EE_UNITS_SR_OTHER_CONTINENT: i32 = 10;
/// unit `a`'s `GetUnitClass()->IsPartLeftOut(complex unit c)`
pub const EE_UNITS_SR_PART_LEFT_OUT: i32 = 11;
/// class `a`'s `IsPriest()`
pub const EE_UNITS_SR_CLASS_PRIEST: i32 = 12;
/// `EECanPriestConvert(unit, complex unit a)`
pub const EE_UNITS_SR_CAN_PRIEST_CONVERT: i32 = 13;
/// `unit->CanAttackClass(class a)`
pub const EE_UNITS_SR_CAN_ATTACK_CLASS: i32 = 14;
/// `unit->mIgnoredTargets.size()`
pub const EE_UNITS_SR_IGNORED_COUNT: i32 = 15;
/// `unit->IsTargetIgnored(complex unit a, c != 0)`
pub const EE_UNITS_SR_IS_IGNORED: i32 = 16;
/// complex unit `a`'s `GetGoal()`
pub const EE_UNITS_SR_GOAL: i32 = 17;
/// `EEAttackUnitCast(goal a)`
pub const EE_UNITS_SR_ATTACK_CAST: i32 = 18;
/// goal `a`'s `GetAttackTarget()->mComplexUnit`
pub const EE_UNITS_SR_ATTACK_TARGET_COMPLEX: i32 = 19;
/// unit `a`'s `mPlayer->mTechnologies->Get(its GetUnitClass()->mTechnology)`
pub const EE_UNITS_SR_TECH_NODE: i32 = 20;
/// tech node `a`'s `mData->mKind`
pub const EE_UNITS_SR_NODE_KIND: i32 = 21;
/// complex unit `a`'s `mLauncherClass != NULL`
pub const EE_UNITS_SR_LAUNCHER: i32 = 22;
/// complex unit `a`'s `mGoalState`
pub const EE_UNITS_SR_GOAL_STATE: i32 = 23;
/// `EEGatherGoalCast(goal a)`
pub const EE_UNITS_SR_GATHER_CAST: i32 = 24;
/// goal `a`'s `GetGatherResource()`
pub const EE_UNITS_SR_GATHER_RESOURCE: i32 = 25;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_SR_COMPLEX: i32 = 26;
/// unit `a`'s `mTileNode->mNext` (as an `EEUnit*`)
pub const EE_UNITS_SR_LIST_NEXT: i32 = 27;
/// unit `a`'s `mTileNode->mPrev` (as an `EEUnit*`)
pub const EE_UNITS_SR_LIST_PREV: i32 = 28;
/// `RTSDummyHead(unit a) != NULL`
pub const EE_UNITS_SR_DUMMY_HEAD: i32 = 29;
/// `f[0] = EECombatHitDistanceSquared(unit, unit a, f[0], f[1])`
pub const EE_UNITS_SR_HIT_DISTANCE: i32 = 30;
/// `f[0] = gToHitTable.GetAttackFactor(unit's class's mAmmoType, unit a's class, the players' ids)`
pub const EE_UNITS_SR_ATTACK_FACTOR: i32 = 31;
/// `EEIsLineToUnitBlocked(unit, unit a, &c->mBlocker, &c->mX, &c->mY)` (`c` an `EeUnitsSrLine`)
pub const EE_UNITS_SR_LINE_BLOCKED: i32 = 32;
/// `EEIsAttackReachable(unit, complex unit a, the tile at c)`
pub const EE_UNITS_SR_REACHABLE: i32 = 33;
/// `unit->IgnoreTarget(complex unit a)`
pub const EE_UNITS_SR_IGNORE: i32 = 34;
/// `unit->GetNearestFootprintTile(unit a, the two longs at c)`
pub const EE_UNITS_SR_FOOTPRINT_TILE: i32 = 35;
/// unit `a`'s `GetUnitID()`
pub const EE_UNITS_SR_UNIT_ID: i32 = 36;
/// `f[0] = unit->mStats->GetAttack()`
pub const EE_UNITS_SR_ATTACK: i32 = 37;
/// `f[0] = unit->GetLineOfSight()`
pub const EE_UNITS_SR_LINE_OF_SIGHT: i32 = 38;
/// `f[0] = unit->mStats->GetMaxVelocity()`
pub const EE_UNITS_SR_MAX_VELOCITY: i32 = 39;
/// `f[0] = unit->mStats->GetRange()`
pub const EE_UNITS_SR_RANGE: i32 = 40;
/// `f[0] = unit->mUnitClass->mMinRange`
pub const EE_UNITS_SR_MIN_RANGE: i32 = 41;
/// `unit->mUnitClass->mIsMelee`
pub const EE_UNITS_SR_MELEE: i32 = 42;
/// `unit->mUnitClass->mShootsHigh`
pub const EE_UNITS_SR_SHOOTS_HIGH: i32 = 43;
/// `unit->mStats->mCanFireOverWalls`
pub const EE_UNITS_SR_OVER_WALLS: i32 = 44;
/// `unit->mTileX`
pub const EE_UNITS_SR_TILE_X: i32 = 45;
/// `unit->mTileY`
pub const EE_UNITS_SR_TILE_Y: i32 = 46;
/// `gPhysics->mTickRandom`
pub const EE_UNITS_SR_TICK_RANDOM: i32 = 47;
/// `f[0] = unit->mPhysicalModel.GetLocation().GetX()`
pub const EE_UNITS_SR_POSITION_X: i32 = 48;
/// `f[0] = unit->mPhysicalModel.GetLocation().GetY()`
pub const EE_UNITS_SR_POSITION_Y: i32 = 49;
/// `gWorld.mMap->mUnitRegions`
pub const EE_UNITS_SR_UNIT_REGIONS: i32 = 50;
/// screen `a`'s `Virtual08(complex unit c)`
pub const EE_UNITS_SR_SCREEN: i32 = 51;
/// `EERetargetAttack(unit, goal a, complex unit c)`
pub const EE_UNITS_SR_RETARGET: i32 = 52;
/// `unit->GetPlayer()->GetTechTree()->Get(unit a's GetUnitClass()->GetTechnology())`
pub const EE_UNITS_SR_OWN_TECHNOLOGY: i32 = 53;
/// technology `a`'s `mData->mKind`
pub const EE_UNITS_SR_TECHNOLOGY_KIND: i32 = 54;

/// `EEIsLineToUnitBlocked`'s outputs for a search (the blocker and two tiles, from -10000).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeUnitsSrLine {
    pub blocker: u64,
    pub x: c_long,
    pub y: c_long,
}

/// `EE_COMBAT_TAG(a, b, c, d)`: the first letter in the low byte.
const fn tag(t: &[u8; 4]) -> u32 {
    u32::from_le_bytes(*t)
}
pub(crate) const TAG_CBST: u32 = tag(b"CBST");
pub(crate) const TAG_CBSL: u32 = tag(b"CBSL");
pub(crate) const TAG_CBPU: u32 = tag(b"CBPU");
pub(crate) const TAG_CBFT: u32 = tag(b"CBFT");

/// The searches' C++ statements and the Rust they call.
pub trait SrHost {
    /// one statement (`EE_UNITS_SR_*`)
    fn sr(&mut self, op: i32, unit: u64, a: u64, c: i64, f: &mut [f32; 4]) -> i64;
    /// `ee_units_uu_targetable(unit, class, 1)`
    fn targetable(&mut self, unit: u64, class: u64) -> bool;
    /// `ee_data_ai_targeting_table` (zero when not loaded)
    fn targeting_table(&mut self) -> [i32; 30];
    /// `ee_combat_regions_size(regions)`
    fn regions_size(&mut self, regions: u64) -> i32;
    /// `EESimCaptureRecord::Active()`
    fn capture(&mut self) -> bool;
    /// a record's `Commit()`
    fn commit(&mut self, record: &Record);
}

fn op<H: SrHost + ?Sized>(h: &mut H, code: i32, unit: u64, a: u64, c: i64) -> i64 {
    h.sr(code, unit, a, c, &mut [0.0; 4])
}

fn opf<H: SrHost + ?Sized>(h: &mut H, code: i32, unit: u64, a: u64) -> f32 {
    let mut f = [0.0; 4];
    h.sr(code, unit, a, 0, &mut f);
    f[0]
}

#[derive(Clone, Copy, Debug, PartialEq)]
enum Kind {
    Regions,
    AlongList,
    Pursuer,
}

/// A search's state (the C++'s `EESearchContext`): the kind, the searching unit, the list search's reach tile
/// (kept across its calls), the capture's trace.
struct Search<'h, H: SrHost + ?Sized> {
    h: &'h mut H,
    kind: Kind,
    unit: u64,
    tile: [c_long; 2],
    capture: bool,
    trace: Vec<i32>,
}

impl<H: SrHost + ?Sized> Search<'_, H> {
    fn op(&mut self, code: i32, a: u64) -> i64 {
        op(self.h, code, self.unit, a, 0)
    }
    fn is(&mut self, code: i32, a: u64) -> bool {
        self.op(code, a) != 0
    }
    /// a handle's complex unit: the region's own, a list unit's `mComplexUnit`
    fn complex(&mut self, handle: u64) -> u64 {
        if self.kind == Kind::Regions {
            handle
        } else {
            self.op(EE_UNITS_SR_COMPLEX, handle) as u64
        }
    }
    /// `EEAttackUnitCast(goal)` attacks the searching unit
    fn attacks_me(&mut self, goal: u64) -> bool {
        let attack = self.op(EE_UNITS_SR_ATTACK_CAST, goal) as u64;
        attack != 0 && self.op(EE_UNITS_SR_ATTACK_TARGET_COMPLEX, attack) as u64 == self.unit
    }
    fn ignored(&mut self, complex: u64) -> bool {
        op(self.h, EE_UNITS_SR_IS_IGNORED, self.unit, complex, 1) != 0
    }

    /// `EEUUTargetSearch::IsCandidate` and `Score`'s tests of a region's complex unit
    fn region_test(&mut self, test: u32, t: u64) -> bool {
        match test {
            EE_COMBAT_SEARCH_TEST_CANDIDATE => {
                !self.is(EE_UNITS_SR_SAME_PLAYER, t)
                    && self.is(EE_UNITS_SR_ENEMY, t)
                    && self.op(EE_UNITS_SR_HIT_POINTS, t) as c_long > 0
                    && (!self.is(EE_UNITS_SR_DISABLED, t) || self.is(EE_UNITS_SR_PROTECTED, t))
            }
            EE_COMBAT_SEARCH_TEST_ATTACKED_BY => self.is(EE_UNITS_SR_ATTACKED_BY, t),
            EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS => {
                let class = self.op(EE_UNITS_SR_CLASS, t) as u64;
                self.h.targetable(self.unit, class)
            }
            EE_COMBAT_SEARCH_TEST_FOUNDATION => self.is(EE_UNITS_SR_IS_FOUNDATION, t),
            EE_COMBAT_SEARCH_TEST_SEEN => self.is(EE_UNITS_SR_SEEN, t),
            EE_COMBAT_SEARCH_TEST_OTHER_CONTINENT => self.is(EE_UNITS_SR_OTHER_CONTINENT, t),
            EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT => op(self.h, EE_UNITS_SR_PART_LEFT_OUT, self.unit, t, t as i64) != 0,
            EE_COMBAT_SEARCH_TEST_PRIEST => {
                // (the candidate's class, as the original's)
                let class = self.op(EE_UNITS_SR_CLASS, t) as u64;
                self.is(EE_UNITS_SR_CLASS_PRIEST, class)
            }
            EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT => self.is(EE_UNITS_SR_CAN_PRIEST_CONVERT, t),
            EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS => {
                let class = self.op(EE_UNITS_SR_CLASS, t) as u64;
                self.is(EE_UNITS_SR_CAN_ATTACK_CLASS, class)
            }
            EE_COMBAT_SEARCH_TEST_IGNORED => self.op(EE_UNITS_SR_IGNORED_COUNT, 0) != 0 && self.ignored(t),
            EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT => {
                let goal = self.op(EE_UNITS_SR_GOAL, t) as u64;
                self.attacks_me(goal)
            }
            _ => false,
        }
    }

    /// `EESearchTargetAlongList`'s and `EEGetPursuerOf`'s tests of a list unit (its complex unit may be missing)
    fn list_test(&mut self, test: u32, t: u64) -> bool {
        match test {
            EE_COMBAT_SEARCH_TEST_NOT_ENEMY => !(!self.is(EE_UNITS_SR_SAME_PLAYER, t) && self.is(EE_UNITS_SR_ENEMY, t)),
            EE_COMBAT_SEARCH_TEST_ATTACKED_BY => self.is(EE_UNITS_SR_ATTACKED_BY, t),
            EE_COMBAT_SEARCH_TEST_TECH_KIND_6 => {
                let node = self.op(EE_UNITS_SR_TECH_NODE, t) as u64;
                let kind = if node != 0 { self.op(EE_UNITS_SR_NODE_KIND, node) as c_long } else { -1 };
                kind == 6
            }
            EE_COMBAT_SEARCH_TEST_CAN_TARGET_CLASS => {
                let class = self.op(EE_UNITS_SR_CLASS, t) as u64;
                self.h.targetable(self.unit, class)
            }
            EE_COMBAT_SEARCH_TEST_FOUNDATION => {
                let x = self.complex(t);
                x != 0 && self.is(EE_UNITS_SR_IS_FOUNDATION, x)
            }
            EE_COMBAT_SEARCH_TEST_SEEN => {
                let x = self.complex(t);
                self.is(EE_UNITS_SR_SEEN, x)
            }
            EE_COMBAT_SEARCH_TEST_PART_LEFT_OUT => {
                let x = self.complex(t);
                op(self.h, EE_UNITS_SR_PART_LEFT_OUT, self.unit, t, x as i64) != 0
            }
            EE_COMBAT_SEARCH_TEST_PRIEST => {
                // (the searching unit's class, as the original's)
                let class = self.op(EE_UNITS_SR_CLASS, self.unit) as u64;
                self.is(EE_UNITS_SR_CLASS_PRIEST, class)
            }
            EE_COMBAT_SEARCH_TEST_CAN_PRIEST_CONVERT => {
                let x = self.complex(t);
                x != 0 && self.is(EE_UNITS_SR_CAN_PRIEST_CONVERT, x)
            }
            EE_COMBAT_SEARCH_TEST_CAN_ATTACK_CLASS => {
                // (a list unit without a complex unit: the original's NULL class read)
                let x = self.complex(t);
                let class = self.op(EE_UNITS_SR_CLASS, x) as u64;
                self.is(EE_UNITS_SR_CAN_ATTACK_CLASS, class)
            }
            EE_COMBAT_SEARCH_TEST_IGNORED => {
                if self.op(EE_UNITS_SR_IGNORED_COUNT, 0) == 0 {
                    return false;
                }
                let x = self.complex(t);
                x != 0 && self.ignored(x)
            }
            EE_COMBAT_SEARCH_TEST_ATTACKS_UNIT => {
                let x = self.complex(t);
                if x == 0 {
                    return false;
                }
                let goal = self.op(EE_UNITS_SR_GOAL, x) as u64;
                self.attacks_me(goal)
            }
            EE_COMBAT_SEARCH_TEST_PURSUES => {
                let x = self.complex(t);
                if x == 0 || self.is(EE_UNITS_SR_LAUNCHER, x) || self.op(EE_UNITS_SR_GOAL_STATE, x) == 0 {
                    return false;
                }
                let goal = self.op(EE_UNITS_SR_GOAL_STATE, x) as u64;
                let attack = self.op(EE_UNITS_SR_ATTACK_CAST, goal) as u64;
                let gather = self.op(EE_UNITS_SR_GATHER_CAST, goal) as u64;
                (attack != 0 && self.op(EE_UNITS_SR_ATTACK_TARGET_COMPLEX, attack) as u64 == self.unit)
                    || (gather != 0 && self.op(EE_UNITS_SR_GATHER_RESOURCE, gather) as u64 == self.unit)
            }
            _ => false,
        }
    }
}

impl<H: SrHost + ?Sized> SearchCallbacks for Search<'_, H> {
    fn list_next(&mut self, unit: u64, prev: bool) -> u64 {
        self.op(if prev { EE_UNITS_SR_LIST_PREV } else { EE_UNITS_SR_LIST_NEXT }, unit) as u64
    }
    fn dummy_head(&mut self, unit: u64) -> bool {
        self.is(EE_UNITS_SR_DUMMY_HEAD, unit)
    }
    fn test(&mut self, test: u32, unit: u64) -> bool {
        if self.kind == Kind::Regions {
            self.region_test(test, unit)
        } else {
            self.list_test(test, unit)
        }
    }
    fn hit_distance_squared(&mut self, unit: u64, x: f32, y: f32) -> f32 {
        let mut f = [x, y, 0.0, 0.0];
        self.h.sr(EE_UNITS_SR_HIT_DISTANCE, self.unit, unit, 0, &mut f);
        f[0]
    }
    fn attack_factor(&mut self, unit: u64) -> f32 {
        opf(self.h, EE_UNITS_SR_ATTACK_FACTOR, self.unit, unit)
    }
    fn line_blocked(&mut self, unit: u64) -> bool {
        let mut line = EeUnitsSrLine { blocker: 0, x: -10000, y: -10000 };
        op(self.h, EE_UNITS_SR_LINE_BLOCKED, self.unit, unit, &mut line as *mut EeUnitsSrLine as i64) != 0
    }
    fn reachable(&mut self, unit: u64) -> bool {
        if self.kind == Kind::Regions {
            let mut tile: [c_long; 2] = [0, 0];
            return op(self.h, EE_UNITS_SR_REACHABLE, self.unit, unit, tile.as_mut_ptr() as i64) != 0;
        }
        let x = self.op(EE_UNITS_SR_COMPLEX, unit) as u64;
        let tile = self.tile.as_mut_ptr() as i64;
        op(self.h, EE_UNITS_SR_REACHABLE, self.unit, x, tile) != 0
    }
    fn ignore(&mut self, unit: u64) {
        let x = self.complex(unit);
        self.op(EE_UNITS_SR_IGNORE, x);
    }
    fn footprint_tile(&mut self, unit: u64) -> (i32, i32) {
        let mut tile: [c_long; 2] = [0, 0];
        op(self.h, EE_UNITS_SR_FOOTPRINT_TILE, self.unit, unit, tile.as_mut_ptr() as i64);
        (tile[0] as i32, tile[1] as i32)
    }
    /// the capture's trace: the candidate's id (a pursuer's complex unit's), its outcome (a pursuer's distance), the
    /// score
    fn trace(&mut self, unit: u64, code: i32, score: i32) {
        if !self.capture {
            return;
        }
        let of = if self.kind == Kind::Pursuer { self.op(EE_UNITS_SR_COMPLEX, unit) as u64 } else { unit };
        let id = self.op(EE_UNITS_SR_UNIT_ID, of) as i32;
        self.trace.push(id);
        self.trace.push(code);
        if self.kind != Kind::Pursuer {
            self.trace.push(score);
        }
    }
}

/// The search's inputs from the unit (`EEUUTargetSearch`'s constructor's reads; `EESearchTargetAlongList`'s locals).
fn input<H: SrHost + ?Sized>(h: &mut H, unit: u64, x: c_long, y: c_long, scale: f32) -> EeCombatSearchIn {
    EeCombatSearchIn {
        attack: opf(h, EE_UNITS_SR_ATTACK, unit, 0),
        reach: opf(h, EE_UNITS_SR_LINE_OF_SIGHT, unit, 0),
        max_velocity: opf(h, EE_UNITS_SR_MAX_VELOCITY, unit, 0),
        range: opf(h, EE_UNITS_SR_RANGE, unit, 0),
        scale,
        min_range: opf(h, EE_UNITS_SR_MIN_RANGE, unit, 0),
        melee: op(h, EE_UNITS_SR_MELEE, unit, 0, 0) as i32,
        shoots_high: op(h, EE_UNITS_SR_SHOOTS_HIGH, unit, 0, 0) as i32,
        over_walls: op(h, EE_UNITS_SR_OVER_WALLS, unit, 0, 0) as i32,
        unit_x: op(h, EE_UNITS_SR_TILE_X, unit, 0, 0) as i32,
        unit_y: op(h, EE_UNITS_SR_TILE_Y, unit, 0, 0) as i32,
        x: x as i32,
        y: y as i32,
        table: h.targeting_table(),
        ..Default::default()
    }
}

fn search<H: SrHost + ?Sized>(h: &mut H, kind: Kind, unit: u64) -> Search<'_, H> {
    let capture = h.capture();
    Search { h, kind, unit, tile: [0, 0], capture, trace: Vec::new() }
}

/// a unit's id in the CB** records (-1: none)
fn capture_id<H: SrHost + ?Sized>(h: &mut H, unit: u64) -> i32 {
    if unit == 0 {
        -1
    } else {
        op(h, EE_UNITS_SR_UNIT_ID, 0, unit, 0) as i32
    }
}

/// A search over the views: `ee_combat`'s walk with its inputs, the unit regions and the callbacks.
pub type Walk<'w> = dyn FnMut(&EeCombatSearchIn, u64, &mut dyn SearchCallbacks) -> EeCombatSearchOut + 'w;

/// `EESearchTarget(unit, area, x, y, scale)`: the best target around (x, y) in the area's regions, scored by an
/// `EEUUTargetSearch` (`walk`: ee_combat's `search_target`); CBST records it.
#[allow(clippy::too_many_arguments)]
pub fn search_target<H: SrHost + ?Sized>(
    h: &mut H,
    unit: u64,
    area: [c_long; 4],
    x: c_long,
    y: c_long,
    scale: f32,
    walk: &mut Walk<'_>,
) -> u64 {
    let mut i = input(h, unit, x, y, scale);
    i.left = area[0] as i32;
    i.top = area[1] as i32;
    i.right = area[2] as i32;
    i.bottom = area[3] as i32;
    let regions = op(h, EE_UNITS_SR_UNIT_REGIONS, unit, 0, 0) as u64;
    i.region_size = h.regions_size(regions);
    i.tick_random = op(h, EE_UNITS_SR_TICK_RANDOM, unit, 0, 0) as i32;
    i.position_x = opf(h, EE_UNITS_SR_POSITION_X, unit, 0);
    i.position_y = opf(h, EE_UNITS_SR_POSITION_Y, unit, 0);
    let mut s = search(h, Kind::Regions, unit);
    let out = walk(&i, regions, &mut s);
    let trace = std::mem::take(&mut s.trace);
    if s.capture {
        let mut r = Record::new(TAG_CBST);
        r.i32(capture_id(h, unit));
        r.i32(x as i32);
        r.i32(y as i32);
        r.f32(scale);
        for v in area {
            r.i32(v as i32);
        }
        r.i32(i.melee);
        r.i32(i.tick_random);
        r.i32((trace.len() / 3) as i32);
        trace.iter().for_each(|&v| r.i32(v));
        r.i32(capture_id(h, out.best));
        r.i32(out.score);
        r.i32(out.done);
        h.commit(&r);
    }
    out.best
}

/// `EESearchTargetAlongList(unit, x, y, scale)`: the best target around (x, y) along the x-sorted unit list (`walk`:
/// ee_combat's `search_along_list` from the unit); CBSL records it.
pub fn search_along_list<H: SrHost + ?Sized>(
    h: &mut H,
    unit: u64,
    x: c_long,
    y: c_long,
    scale: f32,
    walk: &mut Walk<'_>,
) -> u64 {
    let i = input(h, unit, x, y, scale);
    let mut s = search(h, Kind::AlongList, unit);
    let out = walk(&i, 0, &mut s);
    let trace = std::mem::take(&mut s.trace);
    if s.capture {
        let mut r = Record::new(TAG_CBSL);
        r.i32(capture_id(h, unit));
        r.i32(x as i32);
        r.i32(y as i32);
        // the scaled range, as the C++ search's record had it (it wrote its inScale after `inScale = theRange * inScale`)
        r.f32(i.range * scale);
        r.i32((trace.len() / 3) as i32);
        trace.iter().for_each(|&v| r.i32(v));
        r.i32(capture_id(h, out.best));
        r.i32(out.score);
        h.commit(&r);
    }
    out.best
}

/// The pursuer walk: ee_combat's `pursuer_of(unit, x, y)` over the callbacks.
pub type PursuerWalk<'w> = dyn FnMut(u64, i32, i32, &mut dyn SearchCallbacks) -> u64 + 'w;

/// `EEGetPursuerOf(unit)`: the nearest unit that attacks or gathers from the unit (its complex unit); CBPU records it.
pub fn pursuer_of<H: SrHost + ?Sized>(h: &mut H, unit: u64, walk: &mut PursuerWalk<'_>) -> u64 {
    let x = op(h, EE_UNITS_SR_TILE_X, unit, 0, 0) as i32;
    let y = op(h, EE_UNITS_SR_TILE_Y, unit, 0, 0) as i32;
    let mut s = search(h, Kind::Pursuer, unit);
    let pursuer = walk(unit, x, y, &mut s);
    let trace = std::mem::take(&mut s.trace);
    let capture = s.capture;
    let result = if pursuer != 0 { op(h, EE_UNITS_SR_COMPLEX, unit, pursuer, 0) as u64 } else { 0 };
    if capture {
        let mut r = Record::new(TAG_CBPU);
        r.i32(capture_id(h, unit));
        r.i32(x);
        r.i32(y);
        r.i32((trace.len() / 2) as i32);
        trace.iter().for_each(|&v| r.i32(v));
        r.i32(capture_id(h, result));
        h.commit(&r);
    }
    result
}

/// `EEFindNewTarget`'s screen and tests (the C++'s `EENewTargetHost`).
struct NewTarget<'h, H: SrHost + ?Sized> {
    h: &'h mut H,
    screen: u64,
    unit: u64,
    ignore_argument: i64,
}

impl<H: SrHost + ?Sized> NewTarget<'_, H> {
    fn is(&mut self, code: i32, a: u64) -> bool {
        op(self.h, code, self.unit, a, 0) != 0
    }
}

impl<H: SrHost + ?Sized> TargetCallbacks for NewTarget<'_, H> {
    fn screen(&mut self, unit: u64) -> bool {
        op(self.h, EE_UNITS_SR_SCREEN, self.unit, self.screen, unit as i64) != 0
    }
    fn test(&mut self, test: u32, t: u64) -> bool {
        match test {
            TARGET_TEST_TECH_KIND_6 => {
                let technology = op(self.h, EE_UNITS_SR_OWN_TECHNOLOGY, self.unit, t, 0) as u64;
                technology != 0 && op(self.h, EE_UNITS_SR_TECHNOLOGY_KIND, self.unit, technology, 0) as c_long == 6
            }
            TARGET_TEST_NOT_ENEMY => self.is(EE_UNITS_SR_SAME_PLAYER, t) || !self.is(EE_UNITS_SR_ENEMY, t),
            TARGET_TEST_DISABLED => self.is(EE_UNITS_SR_DISABLED, t) && !self.is(EE_UNITS_SR_PROTECTED, t),
            TARGET_TEST_ATTACKED_BY => self.is(EE_UNITS_SR_ATTACKED_BY, t),
            TARGET_TEST_CAN_TARGET_CLASS => {
                let class = op(self.h, EE_UNITS_SR_CLASS, self.unit, t, 0) as u64;
                self.h.targetable(self.unit, class)
            }
            TARGET_TEST_FOUNDATION => self.is(EE_UNITS_SR_IS_FOUNDATION, t),
            TARGET_TEST_SEEN => self.is(EE_UNITS_SR_SEEN, t),
            TARGET_TEST_PART_LEFT_OUT => op(self.h, EE_UNITS_SR_PART_LEFT_OUT, self.unit, t, t as i64) != 0,
            TARGET_TEST_CAN_ATTACK_CLASS => {
                let class = op(self.h, EE_UNITS_SR_CLASS, self.unit, t, 0) as u64;
                self.is(EE_UNITS_SR_CAN_ATTACK_CLASS, class)
            }
            TARGET_TEST_IGNORED => {
                op(self.h, EE_UNITS_SR_IGNORED_COUNT, self.unit, 0, 0) != 0
                    && op(self.h, EE_UNITS_SR_IS_IGNORED, self.unit, t, i64::from(self.ignore_argument != 0)) != 0
            }
            _ => false,
        }
    }
}

/// The new target walk: ee_combat's `units_in_area` then `choose_target` (the candidates, the index chosen).
pub type TargetWalk<'w> =
    dyn FnMut(&EeCombatFindTargetIn, u64, &mut dyn TargetCallbacks) -> (Vec<u64>, Option<usize>) + 'w;

/// `EEFindNewTarget(screen, goal, unit, tile, radius, arg6, arg7)`: gives the unit's goal the first enemy near the
/// tile that passes the screen (`EERetargetAttack`); false when none. CBFT records it.
#[allow(clippy::too_many_arguments)]
pub fn find_new_target<H: SrHost + ?Sized>(
    h: &mut H,
    screen: u64,
    goal: u64,
    unit: u64,
    tile: (c_long, c_long),
    radius: c_long,
    skip_kind: bool,
    ignore_argument: i64,
    walk: &mut TargetWalk<'_>,
) -> bool {
    let i = EeCombatFindTargetIn {
        left: tile.0.wrapping_sub(radius) as i32,
        top: tile.1.wrapping_sub(radius) as i32,
        right: tile.0.wrapping_add(radius) as i32,
        bottom: tile.1.wrapping_add(radius) as i32,
        skip_kind: i32::from(skip_kind),
    };
    let regions = op(h, EE_UNITS_SR_UNIT_REGIONS, unit, 0, 0) as u64;
    let (units, choice) = walk(&i, regions, &mut NewTarget { h, screen, unit, ignore_argument });
    if let Some(n) = choice {
        op(h, EE_UNITS_SR_RETARGET, unit, goal, units[n] as i64);
    }
    if h.capture() {
        let mut r = Record::new(TAG_CBFT);
        r.i32(capture_id(h, unit));
        r.i32(tile.0 as i32);
        r.i32(tile.1 as i32);
        r.i32(radius as i32);
        r.i32(i32::from(skip_kind));
        r.i32(ignore_argument as i32);
        r.i32(units.len() as i32);
        for &u in &units {
            let id = capture_id(h, u);
            r.i32(id);
        }
        r.i32(i32::from(choice.is_some()));
        let chosen = choice.map_or(0, |n| units[n]);
        r.i32(capture_id(h, chosen));
        h.commit(&r);
    }
    choice.is_some()
}

#[cfg(not(test))]
mod game {
    use super::*;
    use crate::combat::search_ffi::SearchWorld;
    use crate::combat::walks_ffi::TargetWorld;

    extern "C" {
        fn ee_units_sr_host(op: i32, unit: u64, a: u64, c: i64, f: *mut [f32; 4]) -> i64;
    }

    struct Game;

    impl SrHost for Game {
        fn sr(&mut self, op: i32, unit: u64, a: u64, c: i64, f: &mut [f32; 4]) -> i64 {
            // SAFETY: the search runs inside the C++ call that handed over its live objects
            unsafe { ee_units_sr_host(op, unit, a, c, f) }
        }
        fn targetable(&mut self, unit: u64, class: u64) -> bool {
            crate::units::uu_ffi::ee_units_uu_targetable(unit, class, 1) != 0
        }
        fn targeting_table(&mut self) -> [i32; 30] {
            let mut t = [0i32; 30];
            // SAFETY: room for the table's 30 values
            unsafe { ee_data::tables::ee_data_ai_targeting_table(t.as_mut_ptr()) };
            t
        }
        fn regions_size(&mut self, regions: u64) -> i32 {
            // SAFETY: the map's live unit regions
            unsafe { crate::combat::regions::ee_combat_regions_size(regions as usize as *const _) }
        }
        fn capture(&mut self) -> bool {
            crate::app::capture_record::active()
        }
        fn commit(&mut self, record: &Record) {
            record.commit();
        }
    }

    fn regions(r: u64) -> *const crate::combat::regions::EeCombatUnitRegions {
        r as usize as *const _
    }

    /// `EESearchTarget` (the area left, top, right, bottom; the scale's bits).
    #[no_mangle]
    pub extern "C" fn ee_units_sr_search_target(
        unit: u64,
        left: i32,
        top: i32,
        right: i32,
        bottom: i32,
        x: i32,
        y: i32,
        scale: f32,
    ) -> u64 {
        let area = [left as c_long, top as c_long, right as c_long, bottom as c_long];
        search_target(&mut Game, unit, area, x as c_long, y as c_long, scale, &mut |i, r, calls| {
            crate::combat::search::search_target(i, &mut SearchWorld { unit_regions: regions(r), calls })
        })
    }

    /// `EESearchTargetAlongList` (`unit` the searching unit, as an `EEUnit*` in the list).
    #[no_mangle]
    pub extern "C" fn ee_units_sr_search_along_list(unit: u64, x: i32, y: i32, scale: f32) -> u64 {
        search_along_list(&mut Game, unit, x as c_long, y as c_long, scale, &mut |i, _r, calls| {
            crate::combat::search::search_along_list(
                i,
                unit,
                &mut SearchWorld { unit_regions: std::ptr::null(), calls },
            )
        })
    }

    /// `EEGetPursuerOf`
    #[no_mangle]
    pub extern "C" fn ee_units_sr_pursuer_of(unit: u64) -> u64 {
        pursuer_of(&mut Game, unit, &mut |u, x, y, calls| {
            crate::combat::pursuer::pursuer_of(u, x, y, &mut SearchWorld { unit_regions: std::ptr::null(), calls })
        })
    }

    /// `EEFindNewTarget`
    #[no_mangle]
    #[allow(clippy::too_many_arguments)]
    pub extern "C" fn ee_units_sr_find_new_target(
        screen: u64,
        goal: u64,
        unit: u64,
        x: i32,
        y: i32,
        radius: i32,
        skip_kind: u8,
        ignore_argument: i32,
    ) -> u8 {
        let tile = (x as c_long, y as c_long);
        u8::from(find_new_target(
            &mut Game,
            screen,
            goal,
            unit,
            tile,
            radius as c_long,
            skip_kind != 0,
            i64::from(ignore_argument),
            &mut |i, r, calls| {
                let mut w = TargetWorld { unit_regions: regions(r), calls };
                let units = crate::combat::walks::units_in_area(i, &mut w);
                let choice = crate::combat::walks::choose_target(&units, i.skip_kind != 0, &mut w);
                (units, choice)
            },
        ))
    }
}

#[cfg(test)]
#[path = "uu_search_tests.rs"]
mod tests;
