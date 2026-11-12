//! The calamity classes' own updates (`src/Empire Earth/Calamities/EECC*.cpp`, every class's `Update`): the timing,
//! the damage ticks and their arithmetic, which units a tick hits, the camera shake's and the meteors' random
//! intervals, the phases of the time warp, the teleport and the logic bomb. Every world effect (damage, units,
//! graphic effects, sounds, the camera, the contagion helpers, random draws) is a call to the host
//! ([`CalamityHost`]), which the C++ (`EECalamityUpdate.cpp`) answers on the C++ objects.
//!
//! The calamity's numeric members travel as [`EeCalamityState`], a copy the C++ fills before the update and takes
//! back after it. The C++ copies it into the `EECalamity` before every host call and back after it, so a world step
//! that reads or writes the calamity (a time warp's `Depart`, a hurricane's storm) sees what the C++ update saw.
//! Every float operation is the C++'s, in its order; `lrintf` is `UMFloatToLong`, `ftol` the C cast, `min`/`max`
//! the Windows macros' comparisons.

// the C++'s own tests (`!(a >= b)` differs from `a < b` for NaN), branches and literals, kept as written
#![allow(clippy::neg_cmp_op_on_partial_ord, clippy::if_same_then_else, clippy::approx_constant)]

use crate::calamity::update_time_alive;
use crate::ftol;
use ee_ge::um::lrintf;

/// The calamity classes (`EECalamityEnum`, `ee/EECalamityManager.h`).
pub(crate) mod kind {
    pub(crate) const EARTHQUAKE: i32 = 3;
    pub(crate) const HURRICANE: i32 = 4;
    pub(crate) const PLAGUE: i32 = 7;
    pub(crate) const LEPROSY: i32 = 8;
    pub(crate) const VOLCANO: i32 = 9;
    pub(crate) const FIRE: i32 = 10;
    pub(crate) const MORALE: i32 = 13;
    pub(crate) const TIME_WARP: i32 = 14;
    pub(crate) const TELEPORT: i32 = 15;
    pub(crate) const AREA_EFFECT: i32 = 16;
    pub(crate) const AE_EXTRA_DAMAGE: i32 = 17;
    pub(crate) const SPECIAL_ATTACK: i32 = 18;
    pub(crate) const AE_MIND_CONTROL: i32 = 19;
    pub(crate) const CONSCRIPTION: i32 = 20;
    pub(crate) const LOGIC_BOMB: i32 = 21;
    pub(crate) const DROP_OBJECT: i32 = 22;
    pub(crate) const CLOAKING: i32 = 23;
    pub(crate) const METEOR_STORM: i32 = 24;
    pub(crate) const LINE_OF_SIGHT: i32 = 25;
    pub(crate) const SPECIAL_LOCATION: i32 = 26;
}

/// The host calls (`EeCalamityCall::op`). Each names what the C++ does and what it answers.
#[repr(i32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeCalamityOp {
    /// The contagion helper `index`'s `Update(x)` (if it exists).
    HelperUpdate = 1,
    /// `UpdateSound(calamity, sound index (0 the class's sound, 1 its secondary), volume x, falloff y, primary
    /// node when value is 1)`.
    Sound = 2,
    /// `EEGetUnitsNear((x, y, z), w)`: value its result, id the list's size; the host keeps the list for the unit calls.
    UnitsNear = 3,
    /// The caster's continent (`kAIPathNonSpace`): value.
    CasterContinent = 4,
    /// Unit `index` of the list for the earthquake: flags bit 0 a land building, bit 1 alive; value its continent.
    QuakeUnit = 5,
    /// Unit `index`'s immunity (effect kind 3): true when it has one (the host plays its effect).
    Immune = 6,
    /// Unit `index` is carried: ok, and key the outermost carrier.
    Carried = 7,
    /// Unit `index` takes `value` damage from the caster (x 0) or the storm (x 1), for the calamity's player,
    /// mode 2 when blocked by immunity else 3.
    TakeHit = 8,
    /// The earthquake's cracks.
    Cracks = 9,
    /// The distance from the camera to the target in the view plane: x.
    CameraDistance = 10,
    /// `gPhysics->GetRandom()`: x.
    Random = 11,
    /// The local player's camera shake at angle x by amplitude y (once per player and update).
    Shake = 12,
    /// The calamity has a target unit: ok.
    TargetExists = 13,
    /// The hurricane's `CreateStorm`.
    CreateStorm = 14,
    /// The class's `PlaySound(calamity)`.
    PlayClassSound = 15,
    /// The storm unit made undeletable: its location x, y, z.
    StormPrepare = 16,
    /// Unit `index` of the list for the hurricane: flags bit 0 the storm itself, bit 1 the storm's player, bit 2
    /// an enemy of it, bit 3 in the class's unit set, bit 4 alive, bit 5 nature's; x the distance squared to the
    /// storm, y its radius against the storm.
    StormUnit = 17,
    /// The hurricane's strike effect on unit `index`.
    Strike = 18,
    /// The meteor storm's `LaunchMeteor`.
    LaunchMeteor = 19,
    /// `gPhysics->mRandom.Generate()`: x.
    RandomGenerate = 20,
    /// `gPhysics->GetRandom(x, y)`: x.
    RandomRange = 21,
    /// The volcano's `CreateVolcano` and its two effects: value the last effect's result.
    CreateVolcano = 22,
    /// The target unit's hit points: value.
    TargetHitPoints = 23,
    /// The time warp's departure: the unit tinted, its departure effect and sound.
    WarpTint = 24,
    /// The time warp's placed effect where the unit leaves.
    WarpAwayEffect = 25,
    /// `gEffectManager->DestroyEffect(value)`.
    DestroyEffect = 26,
    /// The time warp's `Depart`.
    WarpDepart = 27,
    /// The time warp's `Return`.
    WarpReturn = 28,
    /// The target unit is tinted: ok.
    TargetTinted = 29,
    /// The time warp's colour blink.
    WarpBlink = 30,
    /// The caster's hit points: value.
    CasterHitPoints = 31,
    /// The calamity has a caster: ok.
    CasterExists = 32,
    /// The teleport's `HoldCaster`.
    Hold = 33,
    /// The teleport's `Teleport`.
    Teleport = 34,
    /// The logic bomb's placed mark at the target.
    LogicBombMark = 35,
    /// The logic bomb's disabling of its target.
    LogicBombDisable = 36,
    /// The target unit exists, is complex and has no hit points: ok.
    TargetDead = 37,
    /// A contagion helper for the class's functor at the calamity: value its id.
    CreateHelper = 38,
    /// The class's `ApplyEffect(calamity)`.
    ApplyEffect = 39,
    /// The terrain mesh's height at (x, y): x.
    MeshZ = 40,
    /// The class's second graphic effect placed at (x, y, z): id its handle.
    PlacedEffect = 41,
    /// The morale's start on its target (effect, counter, sound).
    Morale = 42,
    /// The target unit exists and is alive: ok.
    TargetAlive = 43,
    /// The special attack's order to the caster.
    SpecialAttack = 44,
    /// The special location's order to the caster.
    SpecialLocation = 45,
    // the casts' (calamity_cast.rs): a tile is (index, index2)
    /// The caster's player index (`EEWorld::GetPlayerIndex`): value.
    CasterPlayerIndex = 46,
    /// The caster's power: value.
    CasterPower = 47,
    /// The tile exists: ok.
    TileExists = 48,
    /// The tile is visible to the caster's player: ok.
    TileVisible = 49,
    /// An area effect of kind 3 at the tile against player `value`: ok.
    AreaEffect = 50,
    /// The unit with the target's id (`gWorld.GetUnit`): ok when there is one.
    TargetLookup = 51,
    /// The class's `IsValidLocation(caster, tile)`: ok.
    ValidLocation = 52,
    /// The class's `IsValidTarget(caster, the looked-up unit)`: ok.
    ValidTarget = 53,
    /// The target's flags (`EE_CALAMITY_TARGET_*`).
    TargetView = 54,
    /// `EEMap::IsAreaOnMap(tile, value)`: ok.
    AreaOnMap = 55,
    /// The terrain mesh's `GetIsValidTile(tile)`: ok.
    ValidTile = 56,
    /// The terrain mesh is a space map: ok.
    SpaceMap = 57,
    /// The tile's continent (`kAIPathNonSpace`): value.
    TileContinent = 58,
    /// `EEWorld::IsTileBlocked(tile)`: ok.
    TileBlocked = 59,
    /// The tile's terrain type: ok when the tile exists, value the type.
    TerrainType = 60,
    /// The class's unit class's inner collision sphere: x.
    InnerSphere = 61,
    /// Unit `index` of the list is a living hurricane or anti-matter storm: ok.
    NearStorm = 62,
    /// The class's unit class's tile size: value.
    TileSize = 63,
    /// `EEMap::IsEdgeTile(tile)`: ok.
    EdgeTile = 64,
    /// `EEIsTileUnblocked` for the class's unit class at the tile (the caster's player, else nature's): ok.
    TileUnblocked = 65,
    /// The caster is a radio man: ok.
    CasterIsRadioMan = 66,
    /// `EEIsTileFree(tile)`: ok.
    TileFree = 67,
    // the starts and stops' (calamity_start.rs)
    /// `StopSound` (index 0) or `StopSecondarySound` (index 1).
    StopSound = 68,
    /// `ESVERIFY_AT(DestroyContagionHelper(index), value)`.
    DestroyHelper = 69,
    /// A contagion helper for the class's functor at the calamity, no unit: value its id, ok when it exists.
    StartHelper = 70,
    /// The class's sound started for the calamity: at its volume (value 0) or with the volume, pan and pitch
    /// `EEComputeSoundParameters` gives at (x, y, z) (value 1).
    StartSound = 71,
    /// The calamity's height (`mHeight`) set to x.
    SetHeight = 72,
    /// The target unit killed, deselected and cleared.
    KillTarget = 73,
    /// The caster's `SightOffHere`.
    CasterSightOff = 74,
    /// The caster's sight: x.
    CasterSight = 75,
    /// The caster's sight set to x, then `SightOnHere`.
    CasterSightOn = 76,
    /// The target unit is protected (`IsProtected`): ok.
    TargetProtected = 77,
    /// The terrain type of the tile (index, index2): ok when the tile exists, value the type.
    TileTerrain = 78,
    /// The target unit exists, is complex and alive: ok.
    TargetComplexAlive = 79,
    /// The class's own start or stop step `index` (its unit's `StartStep`), with (x, y, z) and value.
    ClassStep = 80,
}

/// The calamity's members an update reads and writes (`EECalamity`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityState {
    pub time_alive: f32,
    pub float1: f32,
    pub float2: f32,
    pub int1: i32,
    pub int2: i32,
    pub target: [f32; 3],
    pub epicenter_factor: f32,
    pub effect_handle: u32,
    pub bool1: bool,
    pub bool2: bool,
    pub pad: [u8; 2],
}

/// The class's members its update reads (`EECalamityClass` and the subclass's), read before the update.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityParams {
    pub class_enum: i32,
    pub lifetime: f32,
    /// `EE_CALAMITY_*` bits.
    pub flags: u32,
    // the earthquake
    pub quake_damage: i32,
    pub quake_tick_time: i32,
    pub quake_shake: i32,
    pub quake_duration: f32,
    pub quake_radius: f32,
    pub quake_shake_interval: f32,
    pub quake_volume: f32,
    pub quake_quake_volume: f32,
    pub quake_minimum_factor: f32,
    pub quake_quake_minimum_factor: f32,
    pub quake_speed: f32,
    pub quake_falloff: f32,
    /// The game variant's earthquake damage factor.
    pub quake_damage_factor: f32,
    // the hurricane
    pub storm_damage: i32,
    pub storm_tick_time: u32,
    pub storm_radius: f32,
    /// Its first effect's duration (the storm's delay).
    pub storm_delay: u32,
    // the meteor storm
    pub meteor_minimum_interval: f32,
    pub meteor_maximum_interval: f32,
    /// The volcano's first effect's delay (`mTime1`).
    pub volcano_delay: u32,
    // the time warp
    pub warp_depart_time: i32,
    pub warp_away_time: i32,
    /// The teleport's hold time.
    pub teleport_hold_time: i32,
    // the logic bomb
    pub bomb_time2: u32,
    pub bomb_time3: u32,
    /// The area effects' and the leprosy's first effect's duration.
    pub area_delay: u32,
    /// The extra damage's and the mind control's second time.
    pub area_time2: i32,
    /// The morale's effect duration.
    pub morale_delay: i32,
    // the starts' (calamity_start.rs)
    /// The hurricane's second effect's size, the volcano's first's (the calamity's height above its ground).
    pub start_size: f32,
    /// The line of sight's bonus.
    pub sight_bonus: f32,
    /// The earthquake's source effect speed.
    pub quake_source_speed: f32,
}

/// [`EeCalamityParams::flags`]: the class is blocked by immunity (`mIsBlockedByImmunity`).
pub const EE_CALAMITY_BLOCKED_BY_IMMUNITY: u32 = 1;
/// The class has a sound.
pub const EE_CALAMITY_SOUND: u32 = 2;
/// The class has a secondary sound.
pub const EE_CALAMITY_SECONDARY_SOUND: u32 = 4;
/// The class has its first graphic effect.
pub const EE_CALAMITY_EFFECT1: u32 = 8;
/// The class has its second graphic effect.
pub const EE_CALAMITY_EFFECT2: u32 = 16;

/// One host call.
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EeCalamityCall {
    pub op: EeCalamityOp,
    pub index: i32,
    pub value: i32,
    /// A second index (the casts' tile y).
    pub index2: i32,
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl EeCalamityCall {
    /// A call with no arguments.
    pub fn new(op: EeCalamityOp) -> Self {
        EeCalamityCall { op, index: 0, value: 0, index2: 0, x: 0.0, y: 0.0, z: 0.0, w: 0.0 }
    }
}

/// The host's answer.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeCalamityReply {
    pub ok: bool,
    pub pad: [u8; 3],
    pub flags: u32,
    pub value: i32,
    pub id: u32,
    pub key: u64,
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// The world, as the C++ answers an update's calls (a test host in the tests).
pub trait CalamityHost {
    fn call(&mut self, state: &mut EeCalamityState, call: &EeCalamityCall) -> EeCalamityReply;
}

fn call<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, op: EeCalamityOp) -> EeCalamityReply {
    h.call(s, &EeCalamityCall::new(op))
}

fn call_index<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, op: EeCalamityOp, index: i32) -> EeCalamityReply {
    h.call(s, &EeCalamityCall { index, ..EeCalamityCall::new(op) })
}

/// The Windows `max(a, b)`: `(a > b) ? a : b`.
#[inline]
fn wmax(a: f32, b: f32) -> f32 {
    if a > b {
        a
    } else {
        b
    }
}

/// The Windows `min(a, b)`: `(a < b) ? a : b`.
#[inline]
fn wmin(a: f32, b: f32) -> f32 {
    if a < b {
        a
    } else {
        b
    }
}

/// `EECalamityClass::UpdateTimeAlive`: the age, the helper's update (in the C++'s order: the age computed before
/// it, stored after), 7 at the end of the lifetime.
fn time_alive<H: CalamityHost>(
    h: &mut H,
    s: &mut EeCalamityState,
    p: &EeCalamityParams,
    elapsed: f32,
    helper: i32,
) -> i32 {
    let age = update_time_alive(s.time_alive, elapsed, p.lifetime);
    if helper != -1 {
        h.call(s, &EeCalamityCall { index: helper, x: elapsed, ..EeCalamityCall::new(EeCalamityOp::HelperUpdate) });
    }
    s.time_alive = age.time_alive;
    if age.done {
        7
    } else {
        0
    }
}

fn sound<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, which: i32, volume: f32, falloff: f32, primary: bool) {
    h.call(
        s,
        &EeCalamityCall {
            index: which,
            value: primary as i32,
            x: volume,
            y: falloff,
            ..EeCalamityCall::new(EeCalamityOp::Sound)
        },
    );
}

/// One calamity's update: the class's `Update(calamity, elapsed)`; the result (7: done).
pub fn update<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams, elapsed: f32) -> i32 {
    let has = |f: u32| p.flags & f != 0;
    match p.class_enum {
        kind::EARTHQUAKE => earthquake(h, s, p, elapsed),
        kind::HURRICANE => hurricane(h, s, p, elapsed),
        kind::PLAGUE => {
            let helper = s.int1;
            time_alive(h, s, p, elapsed, helper)
        }
        kind::LEPROSY => {
            let helper = s.int1;
            let result = time_alive(h, s, p, elapsed, helper);
            if s.time_alive >= p.area_delay as f32 && !s.bool1 {
                call(h, s, EeCalamityOp::ApplyEffect);
                s.bool1 = true;
            }
            sound(h, s, 0, -1.0, 4.0, true);
            result
        }
        kind::VOLCANO => {
            let time = s.time_alive + elapsed;
            let mut result = 0;
            sound(h, s, 0, -1.0, 4.0, true);
            s.time_alive = time;
            if (p.volcano_delay as f32) < time && !call(h, s, EeCalamityOp::TargetExists).ok {
                result = call(h, s, EeCalamityOp::CreateVolcano).value;
            }
            if time > p.lifetime {
                result = 7;
            }
            result
        }
        kind::FIRE => {
            let helper = s.int1;
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            time_alive(h, s, p, elapsed, helper)
        }
        kind::MORALE => {
            let result = time_alive(h, s, p, elapsed, -1);
            if s.time_alive >= p.morale_delay as f32 {
                if has(EE_CALAMITY_SECONDARY_SOUND) {
                    sound(h, s, 1, -1.0, 15.0, true);
                }
                if !s.bool1 {
                    s.bool1 = true;
                    call(h, s, EeCalamityOp::Morale);
                }
            } else {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            result
        }
        kind::TIME_WARP => time_warp(h, s, p, elapsed),
        kind::TELEPORT => {
            let mut result = time_alive(h, s, p, elapsed, -1);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            if has(EE_CALAMITY_SECONDARY_SOUND) {
                sound(h, s, 1, -1.0, 15.0, false);
            }
            if call(h, s, EeCalamityOp::CasterHitPoints).value > 0 {
                let hold = p.teleport_hold_time as f32;
                if s.time_alive >= hold && !s.bool1 {
                    call(h, s, EeCalamityOp::Hold);
                    s.bool1 = true;
                }
                if s.time_alive >= p.lifetime - hold && !s.bool2 {
                    if call(h, s, EeCalamityOp::CasterExists).ok {
                        call(h, s, EeCalamityOp::Teleport);
                    }
                    s.bool2 = true;
                }
            } else {
                result = 7;
            }
            result
        }
        kind::AREA_EFFECT => {
            let helper = if s.bool1 { s.int1 } else { -1 };
            let result = time_alive(h, s, p, elapsed, helper);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            if !(s.time_alive >= p.area_delay as f32) || s.bool1 {
                return result;
            }
            let id = call(h, s, EeCalamityOp::CreateHelper).value;
            s.int1 = id;
            call(h, s, EeCalamityOp::ApplyEffect);
            s.bool1 = true;
            result
        }
        kind::AE_EXTRA_DAMAGE | kind::AE_MIND_CONTROL => {
            let helper = s.int1;
            let result = time_alive(h, s, p, elapsed, helper);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            if s.time_alive >= p.area_delay as f32 && !s.bool1 {
                s.bool1 = true;
                if has(EE_CALAMITY_EFFECT2) {
                    let [x, y, _] = s.target;
                    let ground = h.call(s, &EeCalamityCall { x, y, ..EeCalamityCall::new(EeCalamityOp::MeshZ) }).x;
                    let z = ground + 0.05;
                    let handle =
                        h.call(s, &EeCalamityCall { x, y, z, ..EeCalamityCall::new(EeCalamityOp::PlacedEffect) }).id;
                    s.effect_handle = handle;
                }
            }
            if s.time_alive >= p.area_time2 as f32 && !s.bool2 {
                s.bool2 = true;
                call(h, s, EeCalamityOp::ApplyEffect);
            }
            result
        }
        kind::SPECIAL_ATTACK => {
            if call(h, s, EeCalamityOp::TargetAlive).ok {
                call(h, s, EeCalamityOp::SpecialAttack);
            }
            7
        }
        kind::CONSCRIPTION => 7,
        kind::LOGIC_BOMB => {
            let mut result = time_alive(h, s, p, elapsed, -1);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            if s.time_alive >= p.bomb_time2 as f32 && !s.bool1 {
                s.bool1 = true;
                if has(EE_CALAMITY_EFFECT2) {
                    call(h, s, EeCalamityOp::LogicBombMark);
                }
            }
            if s.time_alive >= p.bomb_time3 as f32 && !s.bool2 {
                s.bool2 = true;
                call(h, s, EeCalamityOp::LogicBombDisable);
            }
            if call(h, s, EeCalamityOp::TargetDead).ok {
                result = 7;
            }
            result
        }
        kind::DROP_OBJECT => {
            let result = time_alive(h, s, p, elapsed, -1);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 15.0, true);
            }
            result
        }
        kind::CLOAKING => {
            let mut result = time_alive(h, s, p, elapsed, -1);
            if has(EE_CALAMITY_SOUND) {
                sound(h, s, 0, -1.0, 4.0, true);
            }
            if s.time_alive >= p.lifetime {
                result = 7;
            }
            result
        }
        kind::METEOR_STORM => {
            let time = s.time_alive + elapsed;
            s.time_alive = time;
            if time < p.lifetime {
                if time > s.float1 {
                    call(h, s, EeCalamityOp::LaunchMeteor);
                    let next = s.float1;
                    let random = call(h, s, EeCalamityOp::RandomGenerate).x;
                    s.float1 = next
                        + (p.meteor_minimum_interval
                            + (p.meteor_maximum_interval - p.meteor_minimum_interval) * random);
                    if s.float2 < time {
                        call(h, s, EeCalamityOp::PlayClassSound);
                        let delay = h
                            .call(
                                s,
                                &EeCalamityCall {
                                    x: 500.0,
                                    y: 2000.0,
                                    ..EeCalamityCall::new(EeCalamityOp::RandomRange)
                                },
                            )
                            .x;
                        s.float2 = delay + time;
                    }
                }
                0
            } else {
                7
            }
        }
        kind::LINE_OF_SIGHT => time_alive(h, s, p, elapsed, -1),
        kind::SPECIAL_LOCATION => {
            call(h, s, EeCalamityOp::SpecialLocation);
            7
        }
        // no other class has an update (the manager creates none)
        _ => 0,
    }
}

/// `EECCEarthquake::Update` with `UpdateEpicenterFactor`, `ShakeCamera` and `UpdateQuakeSound`.
fn earthquake<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams, elapsed: f32) -> i32 {
    let continent = call(h, s, EeCalamityOp::CasterContinent).value;
    let mut result = 0;
    let time_alive = s.time_alive + elapsed;
    let seconds = elapsed * 0.001;
    let delay = s.float2;
    let time = s.time_alive * 0.001;
    let new_time = time + seconds;
    let radius = wmax(0.0, wmin((time - delay) * p.quake_speed, p.quake_radius));
    if new_time >= delay && !s.bool1 {
        call(h, s, EeCalamityOp::Cracks);
        s.bool1 = true;
    }
    if s.bool1 {
        s.int1 = s.int1.wrapping_add(lrintf(elapsed));
        if s.int1 > p.quake_tick_time {
            // the tick time is positive here (the counter, above it, was not negative before)
            let ticks = s.int1.checked_div(p.quake_tick_time).unwrap_or(0);
            let damage = ftol(p.quake_damage_factor * p.quake_damage as f32 * ticks as f32);
            s.int1 = s.int1.wrapping_sub(p.quake_tick_time.wrapping_mul(ticks));
            let [x, y, z] = s.target;
            let near =
                h.call(s, &EeCalamityCall { x, y, z, w: radius, ..EeCalamityCall::new(EeCalamityOp::UnitsNear) });
            if near.value > 0 {
                let mut carriers: Vec<u64> = Vec::new();
                for i in (0..near.id as i32).rev() {
                    let unit = call_index(h, s, EeCalamityOp::QuakeUnit, i);
                    if unit.flags & 1 == 0 || unit.flags & 2 == 0 || unit.value != continent {
                        continue;
                    }
                    let mut protected = false;
                    if p.flags & EE_CALAMITY_BLOCKED_BY_IMMUNITY != 0 && call_index(h, s, EeCalamityOp::Immune, i).ok {
                        protected = true;
                    }
                    let carried = call_index(h, s, EeCalamityOp::Carried, i);
                    if protected && !carried.ok {
                        continue;
                    }
                    if carried.ok {
                        if carriers.contains(&carried.key) {
                            continue;
                        }
                        carriers.push(carried.key);
                    }
                    h.call(
                        s,
                        &EeCalamityCall {
                            index: i,
                            value: damage,
                            x: 0.0,
                            ..EeCalamityCall::new(EeCalamityOp::TakeHit)
                        },
                    );
                }
            }
        }
    }
    // UpdateEpicenterFactor
    let distance = call(h, s, EeCalamityOp::CameraDistance).x;
    let factor = if s.bool1 { p.quake_quake_volume } else { p.quake_volume };
    let minimum = if s.bool1 { p.quake_quake_minimum_factor } else { p.quake_minimum_factor };
    let factor = factor * (1.0 / (distance * p.quake_falloff + 1.0));
    s.epicenter_factor = wmax(factor, minimum);
    // ShakeCamera
    let mut shake = s.float1 + seconds;
    if shake > p.quake_shake_interval {
        shake -= p.quake_shake_interval;
        let angle = call(h, s, EeCalamityOp::Random).x * 6.283_185_5 - 3.141_592_7;
        if s.epicenter_factor >= 0.11 {
            let amplitude = (p.quake_shake as f32 * seconds) * s.epicenter_factor;
            h.call(s, &EeCalamityCall { x: angle, y: amplitude, ..EeCalamityCall::new(EeCalamityOp::Shake) });
        }
    }
    s.float1 = shake;
    // UpdateQuakeSound
    let volume = if s.bool1 { p.quake_quake_volume } else { p.quake_volume };
    let volume = wmax(0.0, volume);
    let volume = wmin(1.0, volume);
    sound(h, s, 0, volume, 1.0, true);
    s.time_alive = time_alive;
    if delay + p.quake_duration < new_time {
        result = 7;
    }
    result
}

/// `EECCHurricane::Update`.
fn hurricane<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams, elapsed: f32) -> i32 {
    if p.flags & EE_CALAMITY_EFFECT1 == 0 || s.time_alive >= p.storm_delay as f32 {
        if !call(h, s, EeCalamityOp::TargetExists).ok {
            call(h, s, EeCalamityOp::CreateStorm);
            call(h, s, EeCalamityOp::PlayClassSound);
        } else {
            sound(h, s, 0, -1.0, 4.0, true);
        }
        let at = call(h, s, EeCalamityOp::StormPrepare);
        let (x, y, z) = (at.x, at.y, at.z);
        s.target = [x, y, z];
        let time = s.int1 as f32 + elapsed;
        s.int1 = lrintf(time);
        let tick = p.storm_tick_time as f32;
        if time >= tick {
            let ticks = lrintf(time / tick);
            let near = h.call(
                s,
                &EeCalamityCall { x, y, z, w: p.storm_radius, ..EeCalamityCall::new(EeCalamityOp::UnitsNear) },
            );
            if near.value > 0 {
                let damage = p.storm_damage.wrapping_mul(ticks);
                for i in (0..near.id as i32).rev() {
                    let unit = call_index(h, s, EeCalamityOp::StormUnit, i);
                    // the storm itself, its player's, no enemy of it, outside the set, dead
                    if unit.flags & 1 != 0
                        || unit.flags & 2 != 0
                        || unit.flags & 4 == 0
                        || unit.flags & 8 == 0
                        || unit.flags & 16 == 0
                    {
                        continue;
                    }
                    let distance = unit.x;
                    let reach = unit.y + p.storm_radius;
                    if !(reach * reach > distance) {
                        continue;
                    }
                    call_index(h, s, EeCalamityOp::Strike, i);
                    if unit.flags & 32 != 0 {
                        continue;
                    }
                    if p.flags & EE_CALAMITY_BLOCKED_BY_IMMUNITY != 0 && call_index(h, s, EeCalamityOp::Immune, i).ok {
                        // the immunity's effect played; no damage
                    } else {
                        h.call(
                            s,
                            &EeCalamityCall {
                                index: i,
                                value: damage,
                                x: 1.0,
                                ..EeCalamityCall::new(EeCalamityOp::TakeHit)
                            },
                        );
                    }
                }
            }
            s.int1 = lrintf(time - tick * ticks as f32);
        }
    }
    time_alive(h, s, p, elapsed, -1)
}

/// `EECCTimeWarp::Update`.
fn time_warp<H: CalamityHost>(h: &mut H, s: &mut EeCalamityState, p: &EeCalamityParams, elapsed: f32) -> i32 {
    let mut result = time_alive(h, s, p, elapsed, -1);
    if p.flags & EE_CALAMITY_SECONDARY_SOUND != 0 {
        sound(h, s, 1, -1.0, 15.0, true);
        sound(h, s, 1, -1.0, 15.0, false);
    }
    if call(h, s, EeCalamityOp::TargetHitPoints).value > 0 {
        let depart = p.warp_depart_time as f32;
        let away = p.warp_away_time as f32;
        if s.time_alive - elapsed < depart && s.time_alive >= depart {
            call(h, s, EeCalamityOp::WarpTint);
        }
        if s.time_alive >= away && !s.bool1 {
            call(h, s, EeCalamityOp::WarpAwayEffect);
            if s.effect_handle != 0 {
                let handle = s.effect_handle;
                h.call(s, &EeCalamityCall { value: handle as i32, ..EeCalamityCall::new(EeCalamityOp::DestroyEffect) });
                s.effect_handle = 0;
            }
            call(h, s, EeCalamityOp::WarpDepart);
            s.bool1 = true;
        }
        if s.time_alive >= p.lifetime - away && !s.bool2 {
            call(h, s, EeCalamityOp::WarpReturn);
            s.bool2 = true;
        }
        if call(h, s, EeCalamityOp::TargetTinted).ok {
            s.float1 -= elapsed;
            if s.float1 <= 0.0 {
                call(h, s, EeCalamityOp::WarpBlink);
                if s.time_alive > away * 0.5 {
                    s.float1 = away * 0.025;
                } else if s.time_alive > 0.0 {
                    s.float1 = away * 0.1;
                } else {
                    s.float1 = away * 0.1;
                }
            }
        }
    } else {
        s.int2 = -1;
    }
    if s.int2 == -1 {
        result = 7;
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host that logs every call and answers from a script.
    #[derive(Default)]
    struct Host {
        log: Vec<(EeCalamityOp, i32, i32, f32, f32)>,
        replies: Vec<(EeCalamityOp, i32, EeCalamityReply)>,
    }

    impl Host {
        fn answer(&mut self, op: EeCalamityOp, index: i32, reply: EeCalamityReply) {
            self.replies.push((op, index, reply));
        }
        fn ops(&self) -> Vec<EeCalamityOp> {
            self.log.iter().map(|l| l.0).collect()
        }
    }

    impl CalamityHost for Host {
        fn call(&mut self, state: &mut EeCalamityState, call: &EeCalamityCall) -> EeCalamityReply {
            self.log.push((call.op, call.index, call.value, call.x, call.y));
            if call.op == EeCalamityOp::CreateHelper {
                state.int2 = 99; // a world step may change the calamity
            }
            self.replies.iter().rev().find(|r| r.0 == call.op && r.1 == call.index).map(|r| r.2).unwrap_or_default()
        }
    }

    fn ok() -> EeCalamityReply {
        EeCalamityReply { ok: true, ..Default::default() }
    }

    fn value(v: i32) -> EeCalamityReply {
        EeCalamityReply { value: v, ..Default::default() }
    }

    #[test]
    fn plain_classes_age_and_end() {
        let p = EeCalamityParams {
            class_enum: kind::CLOAKING,
            lifetime: 1000.0,
            flags: EE_CALAMITY_SOUND,
            ..Default::default()
        };
        let mut s = EeCalamityState { time_alive: 950.0, ..Default::default() };
        let mut h = Host::default();
        assert_eq!(update(&mut h, &mut s, &p, 40.0), 0);
        assert_eq!(h.ops(), vec![EeCalamityOp::Sound]);
        assert_eq!(update(&mut h, &mut s, &p, 40.0), 7);
        let p = EeCalamityParams { class_enum: kind::PLAGUE, lifetime: 1000.0, ..Default::default() };
        let mut s = EeCalamityState { int1: 5, ..Default::default() };
        let mut h = Host::default();
        assert_eq!(update(&mut h, &mut s, &p, 10.0), 0);
        assert_eq!(h.log, vec![(EeCalamityOp::HelperUpdate, 5, 0, 10.0, 0.0)]);
        assert_eq!(s.time_alive, 10.0);
        let p = EeCalamityParams { class_enum: kind::CONSCRIPTION, ..Default::default() };
        assert_eq!(update(&mut Host::default(), &mut EeCalamityState::default(), &p, 1.0), 7);
    }

    #[test]
    fn area_effects_start_once_after_the_first_effect() {
        let p =
            EeCalamityParams { class_enum: kind::AREA_EFFECT, lifetime: 5000.0, area_delay: 600, ..Default::default() };
        let mut s = EeCalamityState { time_alive: 590.0, int1: 3, ..Default::default() };
        let mut h = Host::default();
        h.answer(EeCalamityOp::CreateHelper, 0, value(12));
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 0);
        // no helper update before the start; then the helper, the effect
        assert_eq!(h.ops(), vec![EeCalamityOp::CreateHelper, EeCalamityOp::ApplyEffect]);
        assert!(s.bool1);
        assert_eq!(s.int1, 12);
        assert_eq!(s.int2, 99);
        h.log.clear();
        update(&mut h, &mut s, &p, 20.0);
        assert_eq!(h.log, vec![(EeCalamityOp::HelperUpdate, 12, 0, 20.0, 0.0)]);
    }

    #[test]
    fn earthquake_ticks_hit_land_buildings_on_the_continent_once_per_carrier() {
        let p = EeCalamityParams {
            class_enum: kind::EARTHQUAKE,
            flags: EE_CALAMITY_BLOCKED_BY_IMMUNITY,
            quake_damage: 10,
            quake_tick_time: 100,
            quake_duration: 5.0,
            quake_radius: 4.0,
            quake_speed: 2.0,
            quake_shake_interval: 0.5,
            quake_volume: 0.5,
            quake_quake_volume: 2.0,
            quake_minimum_factor: 0.1,
            quake_quake_minimum_factor: 0.2,
            quake_falloff: 0.5,
            quake_damage_factor: 1.5,
            quake_shake: 3,
            ..Default::default()
        };
        let mut s = EeCalamityState { time_alive: 2000.0, float2: 1.0, int1: 150, bool1: true, ..Default::default() };
        let mut h = Host::default();
        h.answer(EeCalamityOp::CasterContinent, 0, value(4));
        h.answer(EeCalamityOp::UnitsNear, 0, EeCalamityReply { value: 4, id: 4, ..Default::default() });
        let building = |continent| EeCalamityReply { flags: 3, value: continent, ..Default::default() };
        h.answer(EeCalamityOp::QuakeUnit, 3, building(4));
        h.answer(EeCalamityOp::QuakeUnit, 2, building(5)); // another continent
        h.answer(EeCalamityOp::QuakeUnit, 1, building(4));
        h.answer(EeCalamityOp::QuakeUnit, 0, building(4));
        let carried = EeCalamityReply { ok: true, key: 77, ..Default::default() };
        h.answer(EeCalamityOp::Carried, 1, carried);
        h.answer(EeCalamityOp::Carried, 0, carried);
        h.answer(EeCalamityOp::Immune, 3, ok());
        h.answer(EeCalamityOp::CameraDistance, 0, EeCalamityReply { x: 2.0, ..Default::default() });
        h.answer(EeCalamityOp::Random, 0, EeCalamityReply { x: 0.75, ..Default::default() });
        // 150 + 1000 ms = 11 ticks of 100: 1.5 * 10 * 11 = 165 damage, 50 left
        assert_eq!(update(&mut h, &mut s, &p, 1000.0), 0);
        let hits: Vec<_> = h.log.iter().filter(|l| l.0 == EeCalamityOp::TakeHit).map(|l| (l.1, l.2)).collect();
        // unit 3 is protected (and not carried), 2 on another continent, 0 shares 1's carrier
        assert_eq!(hits, vec![(1, 165)]);
        assert_eq!(s.int1, 50);
        // the epicenter: 2 / (2 * 0.5 + 1) = 1, above the minimum 0.2
        assert_eq!(s.epicenter_factor, 1.0);
        // the shake: one second past the interval of a half, the angle from the draw, amplitude 3 * 1 * 1
        let shake = h.log.iter().find(|l| l.0 == EeCalamityOp::Shake).unwrap();
        assert_eq!((shake.3, shake.4), (0.75 * 6.283_185_5 - 3.141_592_7, 3.0));
        assert_eq!(s.float1, 0.5);
        // the quake sound at the quake volume clamped to 1
        let sound = h.log.iter().find(|l| l.0 == EeCalamityOp::Sound).unwrap();
        assert_eq!((sound.3, sound.4), (1.0, 1.0));
        assert_eq!(s.time_alive, 3000.0);
        // the end: past the delay and the duration
        let mut s = EeCalamityState { time_alive: 5999.0, float2: 1.0, bool1: true, ..Default::default() };
        assert_eq!(update(&mut Host::default(), &mut s, &p, 2.0), 7);
    }

    #[test]
    fn earthquake_cracks_open_after_the_delay() {
        let p = EeCalamityParams {
            class_enum: kind::EARTHQUAKE,
            quake_tick_time: 100,
            quake_duration: 9.0,
            ..Default::default()
        };
        let mut s = EeCalamityState { time_alive: 450.0, float2: 0.5, ..Default::default() };
        let mut h = Host::default();
        update(&mut h, &mut s, &p, 40.0);
        assert!(!h.ops().contains(&EeCalamityOp::Cracks));
        update(&mut h, &mut s, &p, 40.0);
        assert!(h.ops().contains(&EeCalamityOp::Cracks));
        assert!(s.bool1);
        assert_eq!(s.int1, 40);
    }

    #[test]
    fn hurricane_strikes_enemies_in_reach_and_spares_nature_and_the_immune() {
        let p = EeCalamityParams {
            class_enum: kind::HURRICANE,
            lifetime: 10_000.0,
            flags: EE_CALAMITY_BLOCKED_BY_IMMUNITY,
            storm_damage: 7,
            storm_tick_time: 300,
            storm_radius: 2.0,
            ..Default::default()
        };
        let mut s = EeCalamityState { int1: 250, ..Default::default() };
        let mut h = Host::default();
        h.answer(EeCalamityOp::TargetExists, 0, ok());
        h.answer(EeCalamityOp::StormPrepare, 0, EeCalamityReply { x: 5.0, y: 6.0, z: 1.0, ..Default::default() });
        h.answer(EeCalamityOp::UnitsNear, 0, EeCalamityReply { value: 4, id: 4, ..Default::default() });
        let enemy = |flags, d| EeCalamityReply { flags, x: d, y: 0.5, ..Default::default() };
        h.answer(EeCalamityOp::StormUnit, 3, enemy(4 | 8 | 16, 6.0)); // reach 2.5: 6.25 > 6
        h.answer(EeCalamityOp::StormUnit, 2, enemy(4 | 8 | 16, 7.0)); // out of reach
        h.answer(EeCalamityOp::StormUnit, 1, enemy(4 | 8 | 16 | 32, 1.0)); // nature's: struck, no damage
        h.answer(EeCalamityOp::StormUnit, 0, enemy(4 | 8 | 16, 1.0));
        h.answer(EeCalamityOp::Immune, 0, ok());
        // 250 + 400 = 650: two ticks of 300 (lrintf(2.1666)), 14 damage, 50 left
        update(&mut h, &mut s, &p, 400.0);
        assert_eq!(s.target, [5.0, 6.0, 1.0]);
        let strikes: Vec<_> = h.log.iter().filter(|l| l.0 == EeCalamityOp::Strike).map(|l| l.1).collect();
        assert_eq!(strikes, vec![3, 1, 0]);
        let hits: Vec<_> = h.log.iter().filter(|l| l.0 == EeCalamityOp::TakeHit).map(|l| (l.1, l.2, l.3)).collect();
        assert_eq!(hits, vec![(3, 14, 1.0)]);
        assert_eq!(s.int1, 50);
        assert_eq!(s.time_alive, 400.0);
        // the storm is made when there is none, its sound started
        let mut s = EeCalamityState::default();
        let mut h = Host::default();
        update(&mut h, &mut s, &p, 10.0);
        assert_eq!(
            &h.ops()[..3],
            &[EeCalamityOp::TargetExists, EeCalamityOp::CreateStorm, EeCalamityOp::PlayClassSound]
        );
        assert_eq!(s.int1, 10);
    }

    #[test]
    fn meteors_fall_at_random_intervals_until_the_end() {
        let p = EeCalamityParams {
            class_enum: kind::METEOR_STORM,
            lifetime: 3000.0,
            meteor_minimum_interval: 100.0,
            meteor_maximum_interval: 300.0,
            ..Default::default()
        };
        let mut s = EeCalamityState::default();
        let mut h = Host::default();
        h.answer(EeCalamityOp::RandomGenerate, 0, EeCalamityReply { x: 0.5, ..Default::default() });
        h.answer(EeCalamityOp::RandomRange, 0, EeCalamityReply { x: 800.0, ..Default::default() });
        assert_eq!(update(&mut h, &mut s, &p, 50.0), 0);
        assert_eq!(
            h.ops(),
            vec![
                EeCalamityOp::LaunchMeteor,
                EeCalamityOp::RandomGenerate,
                EeCalamityOp::PlayClassSound,
                EeCalamityOp::RandomRange
            ]
        );
        assert_eq!((s.float1, s.float2), (200.0, 850.0));
        let mut s = EeCalamityState { time_alive: 2990.0, ..Default::default() };
        assert_eq!(update(&mut Host::default(), &mut s, &p, 20.0), 7);
    }

    #[test]
    fn time_warp_departs_returns_and_blinks() {
        let p = EeCalamityParams {
            class_enum: kind::TIME_WARP,
            lifetime: 10_000.0,
            warp_depart_time: 500,
            warp_away_time: 3000,
            ..Default::default()
        };
        let mut h = Host::default();
        h.answer(EeCalamityOp::TargetHitPoints, 0, value(10));
        let mut s = EeCalamityState { time_alive: 490.0, ..Default::default() };
        update(&mut h, &mut s, &p, 20.0);
        assert_eq!(h.ops(), vec![EeCalamityOp::TargetHitPoints, EeCalamityOp::WarpTint, EeCalamityOp::TargetTinted]);
        h.log.clear();
        h.answer(EeCalamityOp::TargetTinted, 0, ok());
        let mut s = EeCalamityState { time_alive: 2990.0, float1: 5.0, effect_handle: 9, ..Default::default() };
        update(&mut h, &mut s, &p, 20.0);
        assert_eq!(
            h.ops(),
            vec![
                EeCalamityOp::TargetHitPoints,
                EeCalamityOp::WarpAwayEffect,
                EeCalamityOp::DestroyEffect,
                EeCalamityOp::WarpDepart,
                EeCalamityOp::TargetTinted,
                EeCalamityOp::WarpBlink
            ]
        );
        assert_eq!(s.effect_handle, 0);
        assert!(s.bool1);
        // past half the away time: the fast blink
        assert_eq!(s.float1, 3000.0 * 0.025);
        h.log.clear();
        let mut s = EeCalamityState { time_alive: 6990.0, bool1: true, float1: 50.0, ..Default::default() };
        update(&mut h, &mut s, &p, 20.0);
        assert!(h.ops().contains(&EeCalamityOp::WarpReturn));
        assert_eq!(s.float1, 30.0);
        // a dead target ends it
        h.answer(EeCalamityOp::TargetHitPoints, 0, value(0));
        let mut s = EeCalamityState::default();
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 7);
        assert_eq!(s.int2, -1);
    }

    #[test]
    fn teleport_holds_then_jumps_and_ends_with_its_caster() {
        let p = EeCalamityParams {
            class_enum: kind::TELEPORT,
            lifetime: 5000.0,
            teleport_hold_time: 2000,
            ..Default::default()
        };
        let mut h = Host::default();
        h.answer(EeCalamityOp::CasterHitPoints, 0, value(5));
        h.answer(EeCalamityOp::CasterExists, 0, ok());
        let mut s = EeCalamityState { time_alive: 2990.0, ..Default::default() };
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 0);
        assert_eq!(
            h.ops(),
            vec![EeCalamityOp::CasterHitPoints, EeCalamityOp::Hold, EeCalamityOp::CasterExists, EeCalamityOp::Teleport]
        );
        assert!(s.bool1 && s.bool2);
        h.answer(EeCalamityOp::CasterHitPoints, 0, value(0));
        assert_eq!(update(&mut h, &mut EeCalamityState::default(), &p, 20.0), 7);
    }

    #[test]
    fn extra_damage_marks_the_ground_then_applies() {
        let p = EeCalamityParams {
            class_enum: kind::AE_EXTRA_DAMAGE,
            lifetime: 9000.0,
            flags: EE_CALAMITY_EFFECT2,
            area_delay: 600,
            area_time2: 1200,
            ..Default::default()
        };
        let mut h = Host::default();
        h.answer(EeCalamityOp::MeshZ, 0, EeCalamityReply { x: 2.0, ..Default::default() });
        h.answer(EeCalamityOp::PlacedEffect, 0, EeCalamityReply { id: 41, ..Default::default() });
        let mut s = EeCalamityState { time_alive: 1190.0, int1: -1, target: [3.0, 4.0, 0.0], ..Default::default() };
        update(&mut h, &mut s, &p, 20.0);
        assert_eq!(h.ops(), vec![EeCalamityOp::MeshZ, EeCalamityOp::PlacedEffect, EeCalamityOp::ApplyEffect]);
        assert_eq!(s.effect_handle, 41);
        assert!(s.bool1 && s.bool2);
    }

    #[test]
    fn volcano_appears_after_its_delay() {
        let p =
            EeCalamityParams { class_enum: kind::VOLCANO, lifetime: 8000.0, volcano_delay: 1000, ..Default::default() };
        let mut h = Host::default();
        h.answer(EeCalamityOp::CreateVolcano, 0, value(3));
        let mut s = EeCalamityState { time_alive: 990.0, ..Default::default() };
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 3);
        h.answer(EeCalamityOp::TargetExists, 0, ok());
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 0);
        let mut s = EeCalamityState { time_alive: 7990.0, ..Default::default() };
        assert_eq!(update(&mut h, &mut s, &p, 20.0), 7);
    }

    #[test]
    fn windows_min_and_max_pick_the_second_on_ties_and_nan() {
        assert_eq!(wmax(1.0, 2.0), 2.0);
        assert_eq!(wmax(f32::NAN, 2.0), 2.0);
        assert!(wmax(2.0, f32::NAN).is_nan());
        assert_eq!(wmin(f32::NAN, 2.0), 2.0);
        assert_eq!(wmin(1.0, 2.0), 1.0);
    }
}
