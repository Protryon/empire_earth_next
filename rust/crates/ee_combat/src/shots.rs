//! The shots' new actions and ticks and the heal goal's work (R5b.2, seventh round): `EEUCProjectile` and
//! `EEUCMissile::ActionChanged` and `Update`, `EEUCTorpedo::Update`, `EEUGHealUnit::Work`.
//!
//! `ActionChanged` and `Update` are plans: each statement of the original is an operation issued to the C++ host
//! ([`ShotHost`]) in the original's order, each read made where the original reads (a physics step, an effect, a
//! shot or a goal change runs between them). The world's objects (effects, sounds, cargo, calamities, goals,
//! remains) stay C++; the decisions and the timers' arithmetic are here. `gPhysics->mRandom`'s draw of the flaming
//! arrows' roll is a host read at the original's place. The heal goal's work is a decision over a plain record.

use std::ffi::c_void;

use crate::{heal, heal_step, EeCombatHealIn, EeCombatHealStepIn};

/// `EEUCProjectile` (and `EEUCBomb`, which runs its action and update).
pub const EE_COMBAT_SHOT_PROJECTILE: i32 = 0;
/// `EEUCMissile`.
pub const EE_COMBAT_SHOT_MISSILE: i32 = 1;
/// `EEUCTorpedo` (its update: the missile's, then the water plane).
pub const EE_COMBAT_SHOT_TORPEDO: i32 = 2;

// ---- the reads (the answer: an integer, 0/1, or an f32's bits)

/// `mCommand ? mCommand->mType : 0`.
pub const EE_COMBAT_SHOT_R_COMMAND: i32 = 1;
/// the class's `mRottingTime`.
pub const EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME: i32 = 2;
/// `mPhysicalModel.GetLocation()`'s x, y, z (f32).
pub const EE_COMBAT_SHOT_R_LOCATION_X: i32 = 3;
pub const EE_COMBAT_SHOT_R_LOCATION_Y: i32 = 4;
pub const EE_COMBAT_SHOT_R_LOCATION_Z: i32 = 5;
/// `mTile->GetHeight()` (f32).
pub const EE_COMBAT_SHOT_R_TILE_HEIGHT: i32 = 6;
/// `mGoalState->mKind`.
pub const EE_COMBAT_SHOT_R_GOAL_KIND: i32 = 7;
/// the class's `mSplashEffect`, `mDeathEffect` and the stats' `mDeathSound` are there.
pub const EE_COMBAT_SHOT_R_SPLASH_EFFECT: i32 = 8;
pub const EE_COMBAT_SHOT_R_DEATH_EFFECT: i32 = 9;
pub const EE_COMBAT_SHOT_R_DEATH_SOUND: i32 = 10;
/// `mUnitClass->mUnitType`.
pub const EE_COMBAT_SHOT_R_UNIT_TYPE: i32 = 11;
/// `GetNumCargoUnits()`.
pub const EE_COMBAT_SHOT_R_CARGO: i32 = 12;
/// `GetLineOfSight()` (f32).
pub const EE_COMBAT_SHOT_R_LINE_OF_SIGHT: i32 = 13;
/// `mComplexUnit->mGoalState->mKind`.
pub const EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND: i32 = 14;
/// the flaming arrows' calamity class (0x17)'s unit set holds the attack goal's target's type.
pub const EE_COMBAT_SHOT_R_FLAMING_SET: i32 = 15;
/// `gPhysics->mRandom.Generate()` (f32; a draw).
pub const EE_COMBAT_SHOT_R_RANDOM: i32 = 16;
/// the flaming arrows' power record's `mChance` (f32).
pub const EE_COMBAT_SHOT_R_FLAMING_CHANCE: i32 = 17;
/// the class's `mRemainsClass` is there, its `mIsProjectile`, `gWorldClearing`, `gFreeCamera`.
pub const EE_COMBAT_SHOT_R_REMAINS: i32 = 18;
pub const EE_COMBAT_SHOT_R_IS_PROJECTILE: i32 = 19;
pub const EE_COMBAT_SHOT_R_WORLD_CLEARING: i32 = 20;
pub const EE_COMBAT_SHOT_R_FREE_CAMERA: i32 = 21;
/// the attack goal's target's `mHitPoints`.
pub const EE_COMBAT_SHOT_R_TARGET_HIT_POINTS: i32 = 22;
/// `mHitPoints`.
pub const EE_COMBAT_SHOT_R_HIT_POINTS: i32 = 23;
/// the class's `GetPopulationCost()`.
pub const EE_COMBAT_SHOT_R_POPULATION_COST: i32 = 24;
/// the class's `mPhysicsModel` is there.
pub const EE_COMBAT_SHOT_R_PHYSICS_MODEL: i32 = 25;
/// `mTileX`.
pub const EE_COMBAT_SHOT_R_TILE_X: i32 = 26;
/// `mRottingAwayTime`, `gPhysics->mTickTimeMs`, `mFlightTime`, `mCloaked`.
pub const EE_COMBAT_SHOT_R_ROTTING: i32 = 27;
pub const EE_COMBAT_SHOT_R_TICK: i32 = 28;
pub const EE_COMBAT_SHOT_R_FLIGHT_TIME: i32 = 29;
pub const EE_COMBAT_SHOT_R_CLOAKED: i32 = 30;
/// `GetPosition(point)`'s z (f32).
pub const EE_COMBAT_SHOT_R_POSITION_Z: i32 = 31;

// ---- the operations

/// `SetAnimation(arg, -1, 0)`.
pub const EE_COMBAT_SHOT_SET_ANIMATION: i32 = 40;
/// `SetRottingAwayTime(arg)`.
pub const EE_COMBAT_SHOT_SET_ROTTING: i32 = 41;
/// the splash effect placed at the location's x, y and z 0.
pub const EE_COMBAT_SHOT_SPLASH: i32 = 42;
/// the location set to the ground point under it (`GetGroundPoint`, `SetLocationXYZ`).
pub const EE_COMBAT_SHOT_GROUND_LOCATION: i32 = 43;
/// the location set to itself (`SetLocationXYZ(position)`).
pub const EE_COMBAT_SHOT_SAME_LOCATION: i32 = 44;
/// the death effect created on the unit.
pub const EE_COMBAT_SHOT_DEATH_EFFECT: i32 = 45;
/// the death sound played (`GetSoundParameters`, `Play`).
pub const EE_COMBAT_SHOT_DEATH_SOUND: i32 = 46;
/// `SetCurrentPhysicsAction(arg)`.
pub const EE_COMBAT_SHOT_PHYSICS_ACTION: i32 = 47;
/// `DropPath()`.
pub const EE_COMBAT_SHOT_DROP_PATH: i32 = 48;
/// `UnloadCargoAtTile((long)x, (long)y, false)` of the position's x and y (arg, arg1: f32 bits).
pub const EE_COMBAT_SHOT_UNLOAD_CARGO: i32 = 49;
/// `KillCargo()`.
pub const EE_COMBAT_SHOT_KILL_CARGO: i32 = 50;
/// `FireShot(inUnit)`.
pub const EE_COMBAT_SHOT_FIRE_SHOT: i32 = 51;
/// the attack goal's complex target's `SetIsTinted(false)` (when it is one).
pub const EE_COMBAT_SHOT_UNTINT_TARGET: i32 = 52;
/// `SightOffHere()`, `SetHitPoints(arg)`, `SightOnHere()`.
pub const EE_COMBAT_SHOT_SIGHT_OFF: i32 = 53;
pub const EE_COMBAT_SHOT_SET_HIT_POINTS: i32 = 54;
pub const EE_COMBAT_SHOT_SIGHT_ON: i32 = 55;
/// the burning calamity (0x18) on the attack goal's target for the unit's player.
pub const EE_COMBAT_SHOT_CREATE_CALAMITY: i32 = 56;
/// `StartAction(arg)`.
pub const EE_COMBAT_SHOT_START_ACTION: i32 = 57;
/// `this->ActionChanged(inUnit)` (virtual: it may run this plan again).
pub const EE_COMBAT_SHOT_ACTION_CHANGED: i32 = 58;
/// `CreateRemainsUnit(inUnit)`.
pub const EE_COMBAT_SHOT_CREATE_REMAINS: i32 = 59;
/// a new attack-ground goal on the dead target's tile (its height when the tile is there).
pub const EE_COMBAT_SHOT_ATTACK_TARGET_TILE: i32 = 60;
/// the attack goal retargeted to its target's container.
pub const EE_COMBAT_SHOT_RETARGET_CONTAINER: i32 = 61;
/// `HandleActionChanged(inUnit)`.
pub const EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED: i32 = 62;
/// a new attack-ground goal on the goal's location (`GetGoalLocation`, `GetB1C`).
pub const EE_COMBAT_SHOT_ATTACK_GOAL_LOCATION: i32 = 63;
/// `mPlayer->mPopulationCount -= arg`.
pub const EE_COMBAT_SHOT_POPULATION: i32 = 64;
/// `gMiniMap->MoveUnit(inUnit, mTileX, mTileY, -10000, -10000)`.
pub const EE_COMBAT_SHOT_MINIMAP_OFF: i32 = 65;
/// `mPhysicsModel->Step(inUnit)`, `UpdateOrbitals()`, `UpdateEffects(inUnit)`, `RefreshSound(inUnit)`.
pub const EE_COMBAT_SHOT_STEP_PHYSICS: i32 = 66;
pub const EE_COMBAT_SHOT_ORBITALS: i32 = 67;
pub const EE_COMBAT_SHOT_EFFECTS: i32 = 68;
pub const EE_COMBAT_SHOT_REFRESH_SOUND: i32 = 69;
/// `SetOpacityChain(arg as f32 bits)`.
pub const EE_COMBAT_SHOT_SET_OPACITY: i32 = 70;
/// `SetFlightTime(arg)`, `mCloaked = arg`.
pub const EE_COMBAT_SHOT_SET_FLIGHT_TIME: i32 = 71;
pub const EE_COMBAT_SHOT_SET_CLOAKED: i32 = 72;
/// the torpedo held at the water plane: the location with z 0 (`SetTransformation(t, true)`).
pub const EE_COMBAT_SHOT_WATER_PLANE: i32 = 73;

// the original's constants (ee/EEUnitAction.h, ee/EEUnitGoal.h, ee/EEUnitClassIDs.h)
const A_SWITCH_TO_ATTACK_GROUND: u32 = 2033;
const A_DYING: i32 = 6000;
const A_UNIT_ROTTING: u32 = 6002;
const A_DEAD: i32 = 6003;
const A_HIT_TARGET: u32 = 6007;
const A_FADING_OUT_IN_SPACE: u32 = 6009;
const A_ADVANCE: i32 = 10003;
const A_SELECT_NEW_TARGET: u32 = 14000;
const A_HEAL_UNIT: u32 = 16001;
const GOAL_ATTACK_UNIT: i32 = 2;
const GOAL_ATTACK_GROUND: i32 = 3;
const TYPE_PARACHUTE_BOMB: i32 = 0xd5;
const TYPE_FLAMING_ARROW: i32 = 0x2d5;
const TYPE_CP_ICBM: i32 = 0x2ee;
const TYPE_BOMB_FOR_SAS: i32 = 0x305;
const OFF_MAP: i32 = -10000;

/// The world of a shot's plan (`EeCombatShotHost` over the C++, a mock in the tests).
pub trait ShotHost {
    /// A read (`EE_COMBAT_SHOT_R_*`) or an operation (the others, answering 0) with its arguments.
    fn op(&mut self, op: i32, arg: i32, arg1: i32) -> i32;
}

/// The C++ host of a shot's plan: `op` reads or acts on the unit and its class over `ctx`.
#[repr(C)]
#[derive(Clone, Copy)]
pub struct EeCombatShotHost {
    pub ctx: *mut c_void,
    pub op: unsafe extern "C" fn(ctx: *mut c_void, op: i32, arg: i32, arg1: i32) -> i32,
}

impl ShotHost for EeCombatShotHost {
    fn op(&mut self, op: i32, arg: i32, arg1: i32) -> i32 {
        // SAFETY: the table's contract: `ctx` is live for the call that handed it over
        unsafe { (self.op)(self.ctx, op, arg, arg1) }
    }
}

struct Plan<'a, H: ShotHost> {
    host: &'a mut H,
}

impl<H: ShotHost> Plan<'_, H> {
    fn read(&mut self, code: i32) -> i32 {
        self.host.op(code, 0, 0)
    }
    fn flag(&mut self, code: i32) -> bool {
        self.read(code) != 0
    }
    fn real(&mut self, code: i32) -> f32 {
        f32::from_bits(self.read(code) as u32)
    }
    fn act(&mut self, op: i32, arg: i32) {
        self.host.op(op, arg, 0);
    }

    /// The hit's first part (both classes): the splash or death effect, the sound, the death animation; the position
    /// read (x, y) for the parachute's cargo.
    fn hit(&mut self) -> (f32, f32) {
        let x = self.real(EE_COMBAT_SHOT_R_LOCATION_X);
        let y = self.real(EE_COMBAT_SHOT_R_LOCATION_Y);
        let z = self.real(EE_COMBAT_SHOT_R_LOCATION_Z);
        let splash = z < 0.0
            && self.real(EE_COMBAT_SHOT_R_TILE_HEIGHT) < 0.0
            && self.read(EE_COMBAT_SHOT_R_GOAL_KIND) != GOAL_ATTACK_UNIT
            && self.flag(EE_COMBAT_SHOT_R_SPLASH_EFFECT);
        if splash {
            self.act(EE_COMBAT_SHOT_SPLASH, 0);
        } else if self.flag(EE_COMBAT_SHOT_R_DEATH_EFFECT) {
            if z < self.real(EE_COMBAT_SHOT_R_TILE_HEIGHT) {
                self.act(EE_COMBAT_SHOT_GROUND_LOCATION, 0);
            } else {
                self.act(EE_COMBAT_SHOT_SAME_LOCATION, 0);
            }
            self.act(EE_COMBAT_SHOT_DEATH_EFFECT, 0);
        }
        if self.flag(EE_COMBAT_SHOT_R_DEATH_SOUND) {
            self.act(EE_COMBAT_SHOT_DEATH_SOUND, 0);
        }
        self.act(EE_COMBAT_SHOT_PHYSICS_ACTION, 2);
        self.act(EE_COMBAT_SHOT_SET_ANIMATION, 4);
        self.act(EE_COMBAT_SHOT_DROP_PATH, 0);
        (x, y)
    }

    /// Rotting: the rotting animation and the class's time.
    fn rot(&mut self) {
        self.act(EE_COMBAT_SHOT_SET_ANIMATION, 5);
        let time = self.read(EE_COMBAT_SHOT_R_CLASS_ROTTING_TIME);
        self.act(EE_COMBAT_SHOT_SET_ROTTING, time);
    }

    /// The rotting time's count down by the tick (at 0).
    fn rotting(&mut self) {
        if self.read(EE_COMBAT_SHOT_R_COMMAND) as u32 == A_UNIT_ROTTING {
            let time = self.read(EE_COMBAT_SHOT_R_ROTTING).wrapping_sub(self.read(EE_COMBAT_SHOT_R_TICK)).max(0);
            self.act(EE_COMBAT_SHOT_SET_ROTTING, time);
        }
    }

    /// The physics step, when the class has a model and the unit is on the map.
    fn step(&mut self) {
        if self.flag(EE_COMBAT_SHOT_R_PHYSICS_MODEL) && self.read(EE_COMBAT_SHOT_R_TILE_X) != OFF_MAP {
            self.act(EE_COMBAT_SHOT_STEP_PHYSICS, 0);
        }
    }

    /// The elapsed tick, at least 1 ms.
    fn elapsed(&mut self) -> i32 {
        match self.read(EE_COMBAT_SHOT_R_TICK) {
            0 => 1,
            tick => tick,
        }
    }
}

/// `EEUCProjectile::ActionChanged`.
fn projectile_action<H: ShotHost>(p: &mut Plan<'_, H>) {
    match p.read(EE_COMBAT_SHOT_R_COMMAND) as u32 {
        A_UNIT_ROTTING => p.rot(),
        A_HIT_TARGET => {
            let (x, y) = p.hit();
            if p.read(EE_COMBAT_SHOT_R_UNIT_TYPE) == TYPE_PARACHUTE_BOMB {
                p.host.op(EE_COMBAT_SHOT_UNLOAD_CARGO, x.to_bits() as i32, y.to_bits() as i32);
                if p.read(EE_COMBAT_SHOT_R_CARGO) != 0 {
                    p.act(EE_COMBAT_SHOT_KILL_CARGO, 0);
                }
            } else {
                p.act(EE_COMBAT_SHOT_FIRE_SHOT, 0);
            }
            // the goal is read again: the shot may have changed it
            if p.read(EE_COMBAT_SHOT_R_UNIT_TYPE) == TYPE_BOMB_FOR_SAS
                && p.read(EE_COMBAT_SHOT_R_GOAL_KIND) == GOAL_ATTACK_UNIT
            {
                p.act(EE_COMBAT_SHOT_UNTINT_TARGET, 0);
            }
            let alive = p.real(EE_COMBAT_SHOT_R_LINE_OF_SIGHT) > 0.0;
            if alive {
                p.act(EE_COMBAT_SHOT_SIGHT_OFF, 0);
            }
            p.act(EE_COMBAT_SHOT_SET_HIT_POINTS, 0);
            if alive {
                p.act(EE_COMBAT_SHOT_SIGHT_ON, 0);
            }
            if p.read(EE_COMBAT_SHOT_R_UNIT_TYPE) == TYPE_FLAMING_ARROW
                && p.read(EE_COMBAT_SHOT_R_COMPLEX_GOAL_KIND) == GOAL_ATTACK_UNIT
                && p.flag(EE_COMBAT_SHOT_R_FLAMING_SET)
            {
                // `Generate() * 100.0f <= mChance`: the draw, then the power record's chance
                let roll = p.real(EE_COMBAT_SHOT_R_RANDOM) * 100.0;
                if roll <= p.real(EE_COMBAT_SHOT_R_FLAMING_CHANCE) {
                    p.act(EE_COMBAT_SHOT_CREATE_CALAMITY, 0);
                }
            }
            let dead = if p.read(EE_COMBAT_SHOT_R_GOAL_KIND) == GOAL_ATTACK_UNIT { A_DEAD } else { A_DYING };
            p.act(EE_COMBAT_SHOT_START_ACTION, dead);
            p.act(EE_COMBAT_SHOT_ACTION_CHANGED, 0);
            if p.flag(EE_COMBAT_SHOT_R_REMAINS)
                && p.flag(EE_COMBAT_SHOT_R_IS_PROJECTILE)
                && !p.flag(EE_COMBAT_SHOT_R_WORLD_CLEARING)
                && !p.flag(EE_COMBAT_SHOT_R_FREE_CAMERA)
            {
                p.act(EE_COMBAT_SHOT_CREATE_REMAINS, 0);
            }
        }
        A_SELECT_NEW_TARGET => {
            if p.read(EE_COMBAT_SHOT_R_TARGET_HIT_POINTS) < 1 {
                p.act(EE_COMBAT_SHOT_ATTACK_TARGET_TILE, 0);
            } else {
                p.act(EE_COMBAT_SHOT_RETARGET_CONTAINER, 0);
            }
        }
        _ => p.act(EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED, 0),
    }
}

/// `EEUCMissile::ActionChanged`.
fn missile_action<H: ShotHost>(p: &mut Plan<'_, H>) {
    match p.read(EE_COMBAT_SHOT_R_COMMAND) as u32 {
        A_SWITCH_TO_ATTACK_GROUND => {
            p.act(EE_COMBAT_SHOT_ATTACK_GOAL_LOCATION, 0);
            p.act(EE_COMBAT_SHOT_START_ACTION, A_ADVANCE);
            p.act(EE_COMBAT_SHOT_ACTION_CHANGED, 0);
        }
        A_HIT_TARGET => {
            p.hit();
            if p.read(EE_COMBAT_SHOT_R_HIT_POINTS) > 0 {
                p.act(EE_COMBAT_SHOT_FIRE_SHOT, 0);
            }
            let goal = p.read(EE_COMBAT_SHOT_R_GOAL_KIND);
            if goal == GOAL_ATTACK_UNIT
                || (p.read(EE_COMBAT_SHOT_R_UNIT_TYPE) == TYPE_CP_ICBM
                    && p.read(EE_COMBAT_SHOT_R_GOAL_KIND) == GOAL_ATTACK_GROUND)
            {
                p.act(EE_COMBAT_SHOT_START_ACTION, A_DEAD);
                p.act(EE_COMBAT_SHOT_ACTION_CHANGED, 0);
                let cost = p.read(EE_COMBAT_SHOT_R_POPULATION_COST);
                if cost > 0 {
                    let cost = p.read(EE_COMBAT_SHOT_R_POPULATION_COST);
                    p.act(EE_COMBAT_SHOT_POPULATION, cost);
                }
                p.act(EE_COMBAT_SHOT_MINIMAP_OFF, 0);
            }
        }
        A_SELECT_NEW_TARGET => {
            if p.read(EE_COMBAT_SHOT_R_TARGET_HIT_POINTS) < 1 {
                p.act(EE_COMBAT_SHOT_START_ACTION, A_DYING);
                p.act(EE_COMBAT_SHOT_ACTION_CHANGED, 0);
            } else {
                p.act(EE_COMBAT_SHOT_RETARGET_CONTAINER, 0);
            }
        }
        _ => p.act(EE_COMBAT_SHOT_HANDLE_ACTION_CHANGED, 0),
    }
}

/// A shot's `ActionChanged` (`kind`: `EE_COMBAT_SHOT_PROJECTILE` or `_MISSILE`), issued to `host`.
pub fn action_changed(kind: i32, host: &mut impl ShotHost) {
    let mut p = Plan { host };
    if kind == EE_COMBAT_SHOT_MISSILE {
        missile_action(&mut p);
    } else {
        projectile_action(&mut p);
    }
}

/// `EEUCMissile::Update`: the step, effects and sound, the rotting, launch delay and lifetime count downs.
fn missile_update<H: ShotHost>(p: &mut Plan<'_, H>) {
    p.step();
    p.act(EE_COMBAT_SHOT_EFFECTS, 0);
    p.act(EE_COMBAT_SHOT_REFRESH_SOUND, 0);
    p.rotting();
    let elapsed = p.elapsed();
    let flight = p.read(EE_COMBAT_SHOT_R_FLIGHT_TIME);
    if flight != 0 {
        p.act(EE_COMBAT_SHOT_SET_FLIGHT_TIME, flight.wrapping_sub(elapsed).max(0));
    }
    // `(long)mCloaked <= 0`
    let cloaked = p.read(EE_COMBAT_SHOT_R_CLOAKED);
    if cloaked <= 0 {
        return;
    }
    p.act(EE_COMBAT_SHOT_SET_CLOAKED, cloaked.wrapping_sub(elapsed).max(0));
}

/// A shot's `Update` (`kind`: `EE_COMBAT_SHOT_*`), issued to `host`.
pub fn update(kind: i32, host: &mut impl ShotHost) {
    let mut p = Plan { host };
    match kind {
        EE_COMBAT_SHOT_PROJECTILE => {
            p.step();
            p.act(EE_COMBAT_SHOT_ORBITALS, 0);
            p.act(EE_COMBAT_SHOT_EFFECTS, 0);
            p.act(EE_COMBAT_SHOT_REFRESH_SOUND, 0);
            match p.read(EE_COMBAT_SHOT_R_COMMAND) as u32 {
                A_UNIT_ROTTING => {
                    let time = p.read(EE_COMBAT_SHOT_R_ROTTING).wrapping_sub(p.read(EE_COMBAT_SHOT_R_TICK)).max(0);
                    p.act(EE_COMBAT_SHOT_SET_ROTTING, time);
                }
                A_FADING_OUT_IN_SPACE => {
                    let z = p.real(EE_COMBAT_SHOT_R_POSITION_Z);
                    if z < 0.0 {
                        let opacity = 1.0 - z * -0.16666667;
                        p.act(EE_COMBAT_SHOT_SET_OPACITY, opacity.to_bits() as i32);
                    }
                }
                _ => {}
            }
            let elapsed = p.elapsed();
            let flight = p.read(EE_COMBAT_SHOT_R_FLIGHT_TIME);
            p.act(EE_COMBAT_SHOT_SET_FLIGHT_TIME, flight.wrapping_add(elapsed));
        }
        EE_COMBAT_SHOT_MISSILE => missile_update(&mut p),
        EE_COMBAT_SHOT_TORPEDO => {
            missile_update(&mut p);
            if p.real(EE_COMBAT_SHOT_R_LOCATION_Z) > 0.0 {
                p.act(EE_COMBAT_SHOT_WATER_PLANE, 0);
            }
        }
        _ => {}
    }
}

/// `EEUGHealUnit::Work`'s reads (plain; the target's fields 0 without one).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatHealWorkIn {
    /// the healer's command's `mType` (0 without one)
    pub command: u32,
    pub has_target: i32,
    /// `gWorld.mInCinematic`
    pub cinematic: i32,
    pub target_hp: i32,
    /// the target's player is the healer's; at war with the healer's (`mEnemies[healer's player] != 0`)
    pub same_player: i32,
    pub target_enemy_of: i32,
    /// the goal's `mElapsedTime`, `gPhysics->mTickTimeMs`
    pub elapsed: u32,
    pub tick_ms: i32,
    /// the healer's `mStats->mHealingPoints`, the target's `mStats->mMaxHitPoints`
    pub healing: i32,
    pub max_hp: i32,
}

/// What the work does: the goal's new elapsed time (`work`), and the target's new hit points (`heal`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeCombatHealWorkOut {
    pub work: i32,
    pub elapsed: u32,
    pub heal: i32,
    pub hit_points: i32,
}

/// `EEUGHealUnit::Work`: while healing a living target that is not an enemy (not in cinematics), the healing points
/// each full second, up to the target's maximum.
pub fn heal_work(i: &EeCombatHealWorkIn) -> EeCombatHealWorkOut {
    let mut out = EeCombatHealWorkOut::default();
    if i.command != A_HEAL_UNIT
        || i.has_target == 0
        || i.cinematic != 0
        || i.target_hp <= 0
        || (i.same_player == 0 && i.target_enemy_of != 0)
    {
        return out;
    }
    let step = heal_step(&EeCombatHealStepIn { elapsed: i.elapsed, tick_ms: i.tick_ms });
    out.work = 1;
    out.elapsed = step.elapsed;
    if step.due != 0 {
        out.heal = 1;
        out.hit_points =
            heal(&EeCombatHealIn { hit_points: i.target_hp, healing: i.healing, max_hit_points: i.max_hp }).hit_points;
    }
    out
}

/// A shot's `ActionChanged` over the C++ host.
///
/// # Safety
/// `host` must point to a valid table whose function may be called with its context.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_shot_action_changed(kind: i32, host: *const EeCombatShotHost) {
    let mut h = unsafe { *host };
    action_changed(kind, &mut h);
}

/// A shot's `Update` over the C++ host.
///
/// # Safety
/// As [`ee_combat_shot_action_changed`].
#[no_mangle]
pub unsafe extern "C" fn ee_combat_shot_update(kind: i32, host: *const EeCombatShotHost) {
    let mut h = unsafe { *host };
    update(kind, &mut h);
}

/// `EEUGHealUnit::Work` over its reads.
///
/// # Safety
/// `input` must point to a valid record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_heal_work(input: *const EeCombatHealWorkIn) -> EeCombatHealWorkOut {
    heal_work(unsafe { &*input })
}

#[cfg(test)]
#[path = "shots_tests.rs"]
mod tests;
