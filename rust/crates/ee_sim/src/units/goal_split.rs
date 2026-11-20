//! The goal host's compound operations split into the C++ statements they were (the units' lane, round 6; were
//! EEUGRust.cpp's `GoalOp` cases): every goal body (rust_goal.rs, the kinds' modules, the combat goals) still asks
//! the operation it asked (`EE_UNITS_GOAL_HOST_START_ACTION`, `..._CALAMITY_CLASS`, ...); [`split`], called by the
//! base fields' wrapper (goal_obj.rs `base_host_op`) for every operation, answers a compound one here: its choices,
//! loops and arithmetic in Rust, each C++ statement one `EE_UNITS_GOAL_HOST_X_*` case of `GoalOp`, asked in the
//! original's order. The plain member reads are the world views' ([`GoalSplitHost::read`]).

use crate::units::rust_goal::*;
use ee_base::math::types::Ge3DPoint;
use std::os::raw::c_long;

// ---- the statements (`GoalOp`'s `EE_UNITS_GOAL_HOST_X_*` cases; `a`, `b` the handles, `io` the rest)
/// `a->StartAction(i[0])`
pub const EE_UNITS_GOAL_HOST_X_START_ACTION: i32 = 600;
/// `EEUnitCast<EEComplexUnit>(gWorld.GetUnit(a))`
pub const EE_UNITS_GOAL_HOST_X_FIND_COMPLEX: i32 = 601;
/// `gWorld.GetUnit(a)`
pub const EE_UNITS_GOAL_HOST_X_FIND_PLAIN: i32 = 602;
/// `a->mActiveGroup->TellGroup(a, f[0..3])`
pub const EE_UNITS_GOAL_HOST_X_TELL_GROUP: i32 = 603;
/// the file's `Write((uint32_t)i[1])`
pub const EE_UNITS_GOAL_HOST_X_WRITE_U32: i32 = 604;
/// the file's `Write((int32_t)i[1])`
pub const EE_UNITS_GOAL_HOST_X_WRITE_I32: i32 = 605;
/// the file's `Write(float of the bits i[1])`
pub const EE_UNITS_GOAL_HOST_X_WRITE_F32: i32 = 606;
/// the file's `Write(i[1] != 0)`
pub const EE_UNITS_GOAL_HOST_X_WRITE_BOOL: i32 = 607;
/// the file's `Read` of a `uint32_t` from and into `i[1]`
pub const EE_UNITS_GOAL_HOST_X_READ_U32: i32 = 608;
/// the file's `Read` of an `int32_t`
pub const EE_UNITS_GOAL_HOST_X_READ_I32: i32 = 609;
/// the file's `Read` of a float (its bits)
pub const EE_UNITS_GOAL_HOST_X_READ_F32: i32 = 610;
/// the file's `Read` of a `bool`
pub const EE_UNITS_GOAL_HOST_X_READ_BOOL: i32 = 611;
/// the context's `mText != NULL`
pub const EE_UNITS_GOAL_HOST_X_HAS_TEXT: i32 = 612;
/// `*mText += text`
pub const EE_UNITS_GOAL_HOST_X_APPEND_TEXT: i32 = 613;
/// `fputs(text, mDump)`
pub const EE_UNITS_GOAL_HOST_X_APPEND_DUMP: i32 = 614;
/// `a->mPlayer->mTechnologies->Get(i[0])`
pub const EE_UNITS_GOAL_HOST_X_PLAYER_TECH: i32 = 615;
/// node `a`'s `mData->mCalamity`
pub const EE_UNITS_GOAL_HOST_X_NODE_CALAMITY: i32 = 616;
/// `EECalamityManager::GetInstance()->GetCalamityClass(i[0])`
pub const EE_UNITS_GOAL_HOST_X_CALAMITY_CLASS_OF: i32 = 617;
/// calamity class `a`'s `GetTargetKind()`
pub const EE_UNITS_GOAL_HOST_X_CALAMITY_TARGET_KIND: i32 = 618;
/// calamity class `a`'s `GetCost()`
pub const EE_UNITS_GOAL_HOST_X_CALAMITY_COST: i32 = 619;
/// calamity class `a`'s `mRange`
pub const EE_UNITS_GOAL_HOST_X_CALAMITY_RANGE: i32 = 620;
/// `a->mPlayer->GetPlayerID()`
pub const EE_UNITS_GOAL_HOST_X_UNIT_PLAYER_ID: i32 = 621;
/// `CreateUnitCalamity(i[0], i[1], i[2], i[3])`
pub const EE_UNITS_GOAL_HOST_X_CREATE_UNIT_CALAMITY: i32 = 622;
/// `CreateLocationCalamity(i[0], i[1], i[2], i[3], i[4])`
pub const EE_UNITS_GOAL_HOST_X_CREATE_LOCATION_CALAMITY: i32 = 623;
/// `CreateGlobalCalamity(i[0], i[1], i[3])`
pub const EE_UNITS_GOAL_HOST_X_CREATE_GLOBAL_CALAMITY: i32 = 624;
/// `EEIsInContactRange(a, b, f[0])`
pub const EE_UNITS_GOAL_HOST_X_IN_CONTACT_RANGE: i32 = 625;
/// `gContinentManager.GetPathAreaAt(a)`
pub const EE_UNITS_GOAL_HOST_X_PATH_AREA_AT: i32 = 626;
/// path area `a`'s `GetHullBounds().mTop`
pub const EE_UNITS_GOAL_HOST_X_AREA_TOP: i32 = 627;
/// `.mBottom`
pub const EE_UNITS_GOAL_HOST_X_AREA_BOTTOM: i32 = 628;
/// `.mLeft`
pub const EE_UNITS_GOAL_HOST_X_AREA_LEFT: i32 = 629;
/// `.mRight`
pub const EE_UNITS_GOAL_HOST_X_AREA_RIGHT: i32 = 630;
/// `gWorld.mMap->GetTile(i[0], i[1])`
pub const EE_UNITS_GOAL_HOST_X_MAP_TILE: i32 = 631;
/// `gWorld.mMap->IsEdgeTile(i[0], i[1])`
pub const EE_UNITS_GOAL_HOST_X_EDGE_TILE: i32 = 632;
/// tile `b`'s `IsVisibleTo(a->mPlayer)`
pub const EE_UNITS_GOAL_HOST_X_TILE_VISIBLE: i32 = 633;
/// `a->mUnitClass->mDomains`
pub const EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS: i32 = 634;
/// `gWorld.mMap->mTerrainMesh.GetIsSpaceMap()`
pub const EE_UNITS_GOAL_HOST_X_SPACE_MAP: i32 = 635;
/// `a->mUnitClass->CanMoveOnTerrain(a, i[0])`
pub const EE_UNITS_GOAL_HOST_X_CAN_MOVE_ON_TERRAIN: i32 = 636;
/// `gMapWidth`
pub const EE_UNITS_GOAL_HOST_X_MAP_WIDTH: i32 = 637;
/// `gMapHeight`
pub const EE_UNITS_GOAL_HOST_X_MAP_HEIGHT: i32 = 638;
/// `gWorld.mMap->mL195614`
pub const EE_UNITS_GOAL_HOST_X_MAP_L195614: i32 = 639;
/// `gWorld.mMap->mL195610`
pub const EE_UNITS_GOAL_HOST_X_MAP_L195610: i32 = 640;
/// `a->mUnitClass->mUnitType`
pub const EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE: i32 = 641;
/// `gContinentManager.GetContinentAt(a)` (as an `EEAIArea*`)
pub const EE_UNITS_GOAL_HOST_X_CONTINENT_AT: i32 = 642;
/// `gContinentManager.GetRandomContinent(kContinentCategoryNone, gPhysics->mRandom)` (a draw)
pub const EE_UNITS_GOAL_HOST_X_RANDOM_CONTINENT: i32 = 643;
/// area `a`'s `GetRandomPoint()` into `i[0]`, `i[1]`
pub const EE_UNITS_GOAL_HOST_X_AREA_RANDOM_POINT: i32 = 644;
/// `EEFindNearbyTarget(a)`
pub const EE_UNITS_GOAL_HOST_X_FIND_NEARBY_TARGET: i32 = 645;
/// `a->mPlayer->NoticeForeignUnit(b)`
pub const EE_UNITS_GOAL_HOST_X_NOTICE_FOREIGN_UNIT: i32 = 646;
/// `a`'s class is a transport (`EEUnitClassOfKind<EE_CLASS_KIND_TRANSPORT>` not NULL)
pub const EE_UNITS_GOAL_HOST_X_IS_TRANSPORT: i32 = 647;
/// the transport class's `CanUnloadAt(a, i[0], i[1])`
pub const EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_UNLOAD_AT: i32 = 648;
/// `EEIsTileFree(a, i[0], i[1])`
pub const EE_UNITS_GOAL_HOST_X_TILE_FREE: i32 = 649;
/// the transport class's `FindUnloadTiles(a, the four tiles of `text`)`
pub const EE_UNITS_GOAL_HOST_X_TRANSPORT_UNLOAD_TILES: i32 = 650;
/// the airplane class's `FindUnloadTiles`
pub const EE_UNITS_GOAL_HOST_X_AIRPLANE_UNLOAD_TILES: i32 = 651;
/// the transport class's `CanUnloadNear(a, a->GetCargo(0), the three tiles of `text`)`
pub const EE_UNITS_GOAL_HOST_X_CAN_UNLOAD_NEAR: i32 = 652;
/// `gSpaceUnloadSoundID`
pub const EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND: i32 = 653;
/// `EESoundObject::Get(i[0])`
pub const EE_UNITS_GOAL_HOST_X_SOUND_GET: i32 = 654;
/// `a->mUnitClass->GetSoundParameters(a, b, &f[0], &f[1], &f[2])`
pub const EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS: i32 = 655;
/// sound `a`'s `Play(f[0], f[1], f[2])`
pub const EE_UNITS_GOAL_HOST_X_SOUND_PLAY: i32 = 656;
/// sound `a`'s `Play()`
pub const EE_UNITS_GOAL_HOST_X_SOUND_PLAY_PLAIN: i32 = 657;
/// `a->mWaypoints.empty()`
pub const EE_UNITS_GOAL_HOST_X_NO_WAYPOINTS: i32 = 658;
/// the transport class's `CanLoad(a, b)`
pub const EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_LOAD: i32 = 659;
/// the transport class's `FindRendezvousTiles(list, a, the two tiles of `text`)`
pub const EE_UNITS_GOAL_HOST_X_FIND_RENDEZVOUS: i32 = 660;
/// `a->mPlayer->GetTechTree()->Get(i[0])`
pub const EE_UNITS_GOAL_HOST_X_TECH_TREE_GET: i32 = 661;
/// node `a`'s researched event's first effect's `mNewUnitClass`
pub const EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS: i32 = 662;
/// `gUnitClassTable[i[0]]`
pub const EE_UNITS_GOAL_HOST_X_CLASS_TABLE: i32 = 663;
/// class `a`'s `GetTechnology()`
pub const EE_UNITS_GOAL_HOST_X_CLASS_TECHNOLOGY: i32 = 664;
/// node `a`'s `GetIsAvailable()`
pub const EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE: i32 = 665;
/// `b->CanTake(a, i[0])`
pub const EE_UNITS_GOAL_HOST_X_CAN_TAKE: i32 = 666;
/// `EEReplaceUnit(a, a->mPlayer->mPlayerID, class b)`
pub const EE_UNITS_GOAL_HOST_X_REPLACE_UNIT: i32 = 667;
/// node `a`'s `mData->mID`
pub const EE_UNITS_GOAL_HOST_X_NODE_ID: i32 = 668;
/// node `a`'s `mData->mKind`
pub const EE_UNITS_GOAL_HOST_X_NODE_KIND: i32 = 669;
/// node `a`'s `GetBuildTime()`
pub const EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME: i32 = 670;
/// `a->mPlayer`
pub const EE_UNITS_GOAL_HOST_X_UNIT_PLAYER: i32 = 671;
/// player `a`'s `mTechnologies != NULL`
pub const EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH: i32 = 672;
/// player `a`'s `mTechnologies->Research(b->mUnitClass->GetTechnology(), NULL)`
pub const EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS: i32 = 673;
/// player `a`'s `CheckButtonsPreloaded(i[0])`
pub const EE_UNITS_GOAL_HOST_X_CHECK_BUTTONS: i32 = 674;
/// `a->SetBuilt(1, 1, 1)`
pub const EE_UNITS_GOAL_HOST_X_SET_BUILT: i32 = 675;
/// player `a`'s `mTechnologies->Get(i[0])->GetIsAvailable()`
pub const EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE: i32 = 676;
/// `EECreateSpaceWallEnergyWall(a)`
pub const EE_UNITS_GOAL_HOST_X_SPACE_WALL: i32 = 677;
/// `gWorld.GetCurrentPlayer()`
pub const EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER: i32 = 678;
/// `a->mStats->mCreationSound`
pub const EE_UNITS_GOAL_HOST_X_CREATION_SOUND: i32 = 679;
/// `gSoundManager->Play(sound a's sample, its volume, 0, 1, false, NULL)`
pub const EE_UNITS_GOAL_HOST_X_PLAY_SAMPLE: i32 = 680;
/// `gUIManager->GetMainGameForm()`
pub const EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM: i32 = 681;
/// `gUIManager->GetMainGameForm()->mRefreshGroup = true`
pub const EE_UNITS_GOAL_HOST_X_REFRESH_GROUP: i32 = 682;
/// `a->mNextPart`
pub const EE_UNITS_GOAL_HOST_X_NEXT_PART: i32 = 683;
/// `a->SetHitPoints(a->mStats->mMaxHitPoints)`
pub const EE_UNITS_GOAL_HOST_X_FULL_HIT_POINTS: i32 = 684;
/// `a->Kill(0, 0, 0)`
pub const EE_UNITS_GOAL_HOST_X_KILL: i32 = 685;
/// `a->GetPlayer()->ScaleCost(node b's GetCost(i[0], 0x20, 1.0f))`
pub const EE_UNITS_GOAL_HOST_X_SCALED_COST: i32 = 686;
/// `a->GetPlayer()->AddResource(i[0], i[1])`
pub const EE_UNITS_GOAL_HOST_X_ADD_RESOURCE: i32 = 687;
/// player `a`'s `mEvents.AddEvent(b's tile, 2, a, i[0], false)`
pub const EE_UNITS_GOAL_HOST_X_ADD_EVENT: i32 = 688;
/// the wonder's chat line (another player's `a` started `b`)
pub const EE_UNITS_GOAL_HOST_X_WONDER_CHAT: i32 = 689;
/// `a->mPlayer->LoadUnitClassMedia(b->mUnitClass->GetUnitType())`
pub const EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA: i32 = 690;
/// `a->LoadMedia(false)`
pub const EE_UNITS_GOAL_HOST_X_LOAD_MEDIA: i32 = 691;
/// `gDataPreLoading->PostUnloadUnitClass(a's player id, a's unit type)`
pub const EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS: i32 = 692;
/// `a->PartFinished()`
pub const EE_UNITS_GOAL_HOST_X_PART_FINISHED: i32 = 693;
/// `a->mPlayer->IsHuman()`
pub const EE_UNITS_GOAL_HOST_X_IS_HUMAN: i32 = 694;
/// `a->mPlayer->mComputerPlayer`
pub const EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER: i32 = 695;
/// computer player `a`'s `mActiveGroup`
pub const EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP: i32 = 696;
/// selected group `a`'s `GetNumUnits()`
pub const EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS: i32 = 697;
/// selected group `a`'s `Clear()`
pub const EE_UNITS_GOAL_HOST_X_GROUP_CLEAR: i32 = 698;
/// the basic citizen class's `FindStorage(a, i[0] != 0)`
pub const EE_UNITS_GOAL_HOST_X_CITIZEN_FIND_STORAGE: i32 = 699;
/// `a->mUnitClass->FindAction(i[0])`
pub const EE_UNITS_GOAL_HOST_X_FIND_ACTION: i32 = 700;
/// `a->SetGoal(EEUGRust::NewAttackUnit(a, b, i[0] != 0, false), 1, 0)`
pub const EE_UNITS_GOAL_HOST_X_SET_ATTACK_GOAL: i32 = 701;
/// `EEFindLoadedGhost(a)`
pub const EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST: i32 = 702;
/// `EEFindLoadedGhostUnit(a)`
pub const EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST_UNIT: i32 = 703;
/// `a->DepleteResource(1)`
pub const EE_UNITS_GOAL_HOST_X_DEPLETE_ONE: i32 = 704;
/// `a->AddResourceAmount(1)`
pub const EE_UNITS_GOAL_HOST_X_ADD_RESOURCE_AMOUNT: i32 = 705;
/// `a->SetLastResource(b)`
pub const EE_UNITS_GOAL_HOST_X_SET_LAST_RESOURCE: i32 = 706;
/// `a->mStats->mGatherTimes[i[0]]`
pub const EE_UNITS_GOAL_HOST_X_GATHER_TIME: i32 = 707;
/// the game variant's `mWorkTimeFactor` into `f[0]`
pub const EE_UNITS_GOAL_HOST_X_WORK_TIME_FACTOR: i32 = 708;
/// `a->SetWorkTimer(i[0])`
pub const EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER: i32 = 709;

/// `kMovementAir`
const MOVEMENT_AIR: i64 = 1;
/// `kUnitTypeSp15SpaceFighter`
const UNIT_TYPE_SP15_SPACE_FIGHTER: i64 = 0x2f6;
/// `kUnitTypeWallSpace`
const UNIT_TYPE_WALL_SPACE: i64 = 0x2fe;
/// `kTechWallSpace`
const TECH_WALL_SPACE: i64 = 15013;
/// `kSoundWonderClock`
const SOUND_WONDER_CLOCK: i64 = 0x3be;
/// `kActionFindStorage`, `kActionFindNewStorage`
const ACTION_FIND_STORAGE: u32 = 4021;
const ACTION_FIND_NEW_STORAGE: u32 = 4007;
/// `kResourceStone`
const RESOURCE_STONE: u64 = 4;
/// the hunt's action (0x7e0)
const ACTION_HUNT: i64 = 0x7e0;

/// A unit's plain member the splits read (the world views').
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum UnitRead {
    ActiveGroup,
    HitPoints,
    ResourceAmount,
    ResourceType,
    CommandType,
    GarrisonCount,
    CargoRoom,
    IsDisabled,
    Animation,
    TileX,
    TileY,
    Id,
}

/// What a split asks: the C++ statements (`raw`) and the Rust reads and calls.
pub trait GoalSplitHost {
    /// one `GoalOp` case
    fn raw(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64;
    /// unit `unit`'s member
    fn read(&mut self, unit: u64, what: UnitRead) -> i64;
    /// `GetPosition`
    fn position(&mut self, unit: u64) -> [f32; 3];
    /// `ee_units_uu_deposit_load(unit, container)` (uu_world.rs)
    fn deposit_load(&mut self, unit: u64, container: u64);
    /// `ee_units_uu_resource_index(unit)`
    fn resource_index(&mut self, unit: u64) -> i64;
}

fn op0<H: GoalSplitHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64) -> i64 {
    h.raw(op, a, b, &mut EeUnitsGoalIo::default())
}

fn op_ints<H: GoalSplitHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, ints: &[i64]) -> i64 {
    let mut io = EeUnitsGoalIo::default();
    io.i[..ints.len()].copy_from_slice(ints);
    h.raw(op, a, b, &mut io)
}

/// A call over tiles handed in place: `io.i[2k], io.i[2k + 1]` in and out through a `U2DPoint<long>` buffer.
fn with_tiles<H: GoalSplitHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo, n: usize) -> i64 {
    let mut tiles = [[0 as c_long; 2]; 4];
    for (k, t) in tiles.iter_mut().enumerate().take(n) {
        *t = [io.i[2 * k] as c_long, io.i[2 * k + 1] as c_long];
    }
    let saved = io.text;
    io.text = tiles.as_mut_ptr() as *const u8;
    let result = h.raw(op, a, b, io);
    io.text = saved;
    for (k, t) in tiles.iter().enumerate().take(n) {
        io.i[2 * k] = t[0] as i64;
        io.i[2 * k + 1] = t[1] as i64;
    }
    result
}

/// `GetSoundParameters` then `Play` of sound `sound` for unit `unit` (nothing for no sound).
fn play_unit_sound<H: GoalSplitHost + ?Sized>(h: &mut H, unit: u64, sound_id: i64) {
    let sound = op_ints(h, EE_UNITS_GOAL_HOST_X_SOUND_GET, 0, 0, &[sound_id]) as u64;
    if sound != 0 {
        let mut io = EeUnitsGoalIo::default();
        h.raw(EE_UNITS_GOAL_HOST_X_SOUND_PARAMETERS, unit, sound, &mut io);
        h.raw(EE_UNITS_GOAL_HOST_X_SOUND_PLAY, sound, 0, &mut io);
    }
}

/// Answers the compound operation `op` (`Some`) from its statements, or `None`: `op` is one statement already.
pub fn split<H: GoalSplitHost + ?Sized>(h: &mut H, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> Option<i64> {
    Some(match op {
        EE_UNITS_GOAL_HOST_START_ACTION => {
            op_ints(h, EE_UNITS_GOAL_HOST_X_START_ACTION, a, 0, &[io.i[0]]);
            if io.i[1] == 0 {
                op0(h, EE_UNITS_GOAL_HOST_ACTION_CHANGED, a, 0);
            }
            0
        }
        EE_UNITS_GOAL_HOST_FIND_UNIT => {
            op0(h, if b != 0 { EE_UNITS_GOAL_HOST_X_FIND_COMPLEX } else { EE_UNITS_GOAL_HOST_X_FIND_PLAIN }, a, 0)
        }
        EE_UNITS_GOAL_HOST_TELL_GROUP => {
            if h.read(a, UnitRead::ActiveGroup) != 0 {
                h.raw(EE_UNITS_GOAL_HOST_X_TELL_GROUP, a, b, io);
            }
            0
        }
        EE_UNITS_GOAL_HOST_WRITE => {
            let op = match io.i[0] {
                0 => EE_UNITS_GOAL_HOST_X_WRITE_U32,
                1 => EE_UNITS_GOAL_HOST_X_WRITE_I32,
                3 => EE_UNITS_GOAL_HOST_X_WRITE_F32,
                _ => EE_UNITS_GOAL_HOST_X_WRITE_BOOL,
            };
            h.raw(op, a, b, io)
        }
        EE_UNITS_GOAL_HOST_READ => {
            let op = match io.i[0] {
                0 => EE_UNITS_GOAL_HOST_X_READ_U32,
                1 => EE_UNITS_GOAL_HOST_X_READ_I32,
                3 => EE_UNITS_GOAL_HOST_X_READ_F32,
                _ => EE_UNITS_GOAL_HOST_X_READ_BOOL,
            };
            h.raw(op, a, b, io)
        }
        EE_UNITS_GOAL_HOST_APPEND => {
            let op = if op0(h, EE_UNITS_GOAL_HOST_X_HAS_TEXT, 0, 0) != 0 {
                EE_UNITS_GOAL_HOST_X_APPEND_TEXT
            } else {
                EE_UNITS_GOAL_HOST_X_APPEND_DUMP
            };
            h.raw(op, a, b, io);
            0
        }
        // the cast calamity's
        EE_UNITS_GOAL_HOST_CALAMITY_CLASS => {
            let node = op_ints(h, EE_UNITS_GOAL_HOST_X_PLAYER_TECH, a, 0, &[io.i[0]]) as u64;
            if node == 0 {
                return Some(0);
            }
            let calamity = op0(h, EE_UNITS_GOAL_HOST_X_NODE_CALAMITY, node, 0);
            io.i[1] = calamity;
            io.i[2] = calamity;
            op_ints(h, EE_UNITS_GOAL_HOST_X_CALAMITY_CLASS_OF, 0, 0, &[calamity])
        }
        EE_UNITS_GOAL_HOST_CALAMITY_INFO => {
            io.i[0] = op0(h, EE_UNITS_GOAL_HOST_X_CALAMITY_TARGET_KIND, a, 0);
            io.i[1] = op0(h, EE_UNITS_GOAL_HOST_X_CALAMITY_COST, a, 0);
            io.i[2] = op0(h, EE_UNITS_GOAL_HOST_X_CALAMITY_RANGE, a, 0);
            0
        }
        EE_UNITS_GOAL_HOST_CREATE_CALAMITY => {
            let calamity = io.i[1];
            let source = h.read(a, UnitRead::Id);
            let player = op0(h, EE_UNITS_GOAL_HOST_X_UNIT_PLAYER_ID, io.i[4] as u64, 0);
            match io.i[0] {
                3 => {
                    let target = h.read(b, UnitRead::Id);
                    op_ints(h, EE_UNITS_GOAL_HOST_X_CREATE_UNIT_CALAMITY, 0, 0, &[calamity, source, target, player])
                }
                2 => op_ints(
                    h,
                    EE_UNITS_GOAL_HOST_X_CREATE_LOCATION_CALAMITY,
                    0,
                    0,
                    &[calamity, source, io.i[2], io.i[3], player],
                ),
                _ => op_ints(h, EE_UNITS_GOAL_HOST_X_CREATE_GLOBAL_CALAMITY, 0, 0, &[calamity, source, 0, player]),
            }
        }
        EE_UNITS_GOAL_HOST_CONTACT_RANGE => {
            let target = h.position(b);
            let point = h.position(a);
            let p = Ge3DPoint { x: point[0], y: point[1], z: point[2] };
            let distance = p.distance_to_squared(Ge3DPoint { x: target[0], y: target[1], z: target[2] });
            let mut args = EeUnitsGoalIo::default();
            args.f[0] = distance;
            h.raw(EE_UNITS_GOAL_HOST_X_IN_CONTACT_RANGE, a, b, &mut args)
        }
        // the explore goal's
        EE_UNITS_GOAL_HOST_PATH_AREA_BOUNDS => {
            let area = op0(h, EE_UNITS_GOAL_HOST_X_PATH_AREA_AT, a, 0) as u64;
            if area == 0 {
                return Some(0);
            }
            io.i[0] = op0(h, EE_UNITS_GOAL_HOST_X_AREA_TOP, area, 0);
            io.i[1] = op0(h, EE_UNITS_GOAL_HOST_X_AREA_BOTTOM, area, 0);
            io.i[2] = op0(h, EE_UNITS_GOAL_HOST_X_AREA_LEFT, area, 0);
            io.i[3] = op0(h, EE_UNITS_GOAL_HOST_X_AREA_RIGHT, area, 0);
            1
        }
        EE_UNITS_GOAL_HOST_TILE_SCREEN => {
            let (x, y) = (io.i[0], io.i[1]);
            let tile = op_ints(h, EE_UNITS_GOAL_HOST_X_MAP_TILE, 0, 0, &[x, y]) as u64;
            if tile == 0
                || op_ints(h, EE_UNITS_GOAL_HOST_X_EDGE_TILE, 0, 0, &[x, y]) != 0
                || op0(h, EE_UNITS_GOAL_HOST_X_TILE_VISIBLE, a, tile) != 0
            {
                return Some(1);
            }
            i64::from(
                (op0(h, EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS, a, 0) & MOVEMENT_AIR) != 0
                    && (op0(h, EE_UNITS_GOAL_HOST_X_SPACE_MAP, 0, 0) == 0
                        || op_ints(h, EE_UNITS_GOAL_HOST_X_CAN_MOVE_ON_TERRAIN, a, 0, &[12]) != 0),
            )
        }
        EE_UNITS_GOAL_HOST_MAP_DIMENSIONS => {
            io.i[0] = op0(h, EE_UNITS_GOAL_HOST_X_MAP_WIDTH, 0, 0);
            io.i[1] = op0(h, EE_UNITS_GOAL_HOST_X_MAP_HEIGHT, 0, 0);
            io.i[2] = op0(h, EE_UNITS_GOAL_HOST_X_MAP_L195614, 0, 0);
            io.i[3] = op0(h, EE_UNITS_GOAL_HOST_X_MAP_L195610, 0, 0);
            0
        }
        EE_UNITS_GOAL_HOST_CONTINENT_POINT => {
            let ground = ((op0(h, EE_UNITS_GOAL_HOST_X_CLASS_DOMAINS, a, 0) & MOVEMENT_AIR) == 0
                || (op0(h, EE_UNITS_GOAL_HOST_X_SPACE_MAP, 0, 0) != 0
                    && op_ints(h, EE_UNITS_GOAL_HOST_X_CAN_MOVE_ON_TERRAIN, a, 0, &[12]) == 0))
                && op0(h, EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE, a, 0) != UNIT_TYPE_SP15_SPACE_FIGHTER;
            let area = if ground {
                op0(h, EE_UNITS_GOAL_HOST_X_CONTINENT_AT, a, 0)
            } else {
                op0(h, EE_UNITS_GOAL_HOST_X_RANDOM_CONTINENT, 0, 0)
            } as u64;
            if area == 0 {
                return Some(0);
            }
            h.raw(EE_UNITS_GOAL_HOST_X_AREA_RANDOM_POINT, area, 0, io);
            1
        }
        EE_UNITS_GOAL_HOST_NOTICE_FOREIGN => {
            let target = op0(h, EE_UNITS_GOAL_HOST_X_FIND_NEARBY_TARGET, a, 0) as u64;
            if target != 0 {
                op0(h, EE_UNITS_GOAL_HOST_X_NOTICE_FOREIGN_UNIT, a, target);
            }
            0
        }
        // the unload transport's
        EE_UNITS_GOAL_HOST_CAN_UNLOAD_AT => {
            if op0(h, EE_UNITS_GOAL_HOST_X_IS_TRANSPORT, a, 0) != 0 {
                op_ints(h, EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_UNLOAD_AT, a, 0, &[io.i[0], io.i[1]])
            } else {
                op_ints(h, EE_UNITS_GOAL_HOST_X_TILE_FREE, a, 0, &[io.i[0], io.i[1]])
            }
        }
        EE_UNITS_GOAL_HOST_FIND_UNLOAD_TILES => {
            let op = if op0(h, EE_UNITS_GOAL_HOST_X_IS_TRANSPORT, a, 0) != 0 {
                EE_UNITS_GOAL_HOST_X_TRANSPORT_UNLOAD_TILES
            } else {
                EE_UNITS_GOAL_HOST_X_AIRPLANE_UNLOAD_TILES
            };
            with_tiles(h, op, a, 0, io, 4);
            0
        }
        EE_UNITS_GOAL_HOST_CAN_UNLOAD_NEAR => with_tiles(h, EE_UNITS_GOAL_HOST_X_CAN_UNLOAD_NEAR, a, 0, io, 3),
        EE_UNITS_GOAL_HOST_UNLOAD_SOUND | EE_UNITS_GOAL_HOST_PLAY_SOUND => {
            if op == EE_UNITS_GOAL_HOST_UNLOAD_SOUND {
                io.i[0] = if io.i[0] != 0 { op0(h, EE_UNITS_GOAL_HOST_X_SPACE_UNLOAD_SOUND, 0, 0) } else { 0x184 };
            }
            play_unit_sound(h, a, io.i[0]);
            0
        }
        // the load goals' (zeros for no unit)
        EE_UNITS_GOAL_HOST_LOAD_UNIT => {
            if a != 0 {
                let fields = [
                    h.read(a, UnitRead::GarrisonCount),
                    h.read(a, UnitRead::CargoRoom),
                    h.read(a, UnitRead::HitPoints),
                    h.read(a, UnitRead::IsDisabled),
                    op0(h, EE_UNITS_GOAL_HOST_X_NO_WAYPOINTS, a, 0),
                    h.read(a, UnitRead::Animation),
                ];
                io.i[..6].copy_from_slice(&fields);
            }
            0
        }
        EE_UNITS_GOAL_HOST_CAN_LOAD => i64::from(
            op0(h, EE_UNITS_GOAL_HOST_X_IS_TRANSPORT, a, 0) != 0
                && op0(h, EE_UNITS_GOAL_HOST_X_TRANSPORT_CAN_LOAD, a, b) != 0,
        ),
        EE_UNITS_GOAL_HOST_RENDEZVOUS => {
            if io.i[4] == 0 {
                with_tiles(h, EE_UNITS_GOAL_HOST_X_FIND_RENDEZVOUS, a, 0, io, 2);
            } else {
                // the transport base's rendezvous: its own tile for both
                let (x, y) = (h.read(a, UnitRead::TileX), h.read(a, UnitRead::TileY));
                io.i[..4].copy_from_slice(&[x, y, x, y]);
            }
            0
        }
        EE_UNITS_GOAL_HOST_GARRISON_UPGRADE => {
            let node = op_ints(h, EE_UNITS_GOAL_HOST_X_TECH_TREE_GET, a, 0, &[io.i[0]]) as u64;
            io.i[0] = i64::from(node != 0);
            if node == 0 {
                return Some(0);
            }
            let upgrade = op0(h, EE_UNITS_GOAL_HOST_X_NODE_NEW_CLASS, node, 0);
            let class = op_ints(h, EE_UNITS_GOAL_HOST_X_CLASS_TABLE, 0, 0, &[upgrade]) as u64;
            let available = if class != 0 {
                let technology = op0(h, EE_UNITS_GOAL_HOST_X_CLASS_TECHNOLOGY, class, 0);
                op_ints(h, EE_UNITS_GOAL_HOST_X_TECH_TREE_GET, a, 0, &[technology]) as u64
            } else {
                0
            };
            io.i[1] = upgrade;
            io.i[2] = i64::from(available != 0 && op0(h, EE_UNITS_GOAL_HOST_X_NODE_AVAILABLE, available, 0) != 0);
            0
        }
        EE_UNITS_GOAL_HOST_DEPOSIT => {
            if h.read(a, UnitRead::ResourceAmount) > 0 {
                let kind = h.read(a, UnitRead::ResourceType);
                if op_ints(h, EE_UNITS_GOAL_HOST_X_CAN_TAKE, a, b, &[kind]) != 0 {
                    h.deposit_load(a, b);
                }
            }
            0
        }
        EE_UNITS_GOAL_HOST_REPLACE_CLASS => {
            let class = op_ints(h, EE_UNITS_GOAL_HOST_X_CLASS_TABLE, 0, 0, &[io.i[0]]) as u64;
            if class != 0 {
                op0(h, EE_UNITS_GOAL_HOST_X_REPLACE_UNIT, a, class);
            }
            0
        }
        // the build/repair goal's
        EE_UNITS_GOAL_HOST_NODE_INFO => {
            if a != 0 {
                io.i[0] = op0(h, EE_UNITS_GOAL_HOST_X_NODE_ID, a, 0);
                io.i[1] = op0(h, EE_UNITS_GOAL_HOST_X_NODE_KIND, a, 0);
                io.i[2] = op0(h, EE_UNITS_GOAL_HOST_X_NODE_BUILD_TIME, a, 0);
            }
            0
        }
        EE_UNITS_GOAL_HOST_COMPLETE_BUILDING => {
            // FinishBuilding's completion of an unbuilt building by the worker's player
            let player = op0(h, EE_UNITS_GOAL_HOST_X_UNIT_PLAYER, a, 0) as u64;
            if player != 0 && op0(h, EE_UNITS_GOAL_HOST_X_PLAYER_HAS_TECH, player, 0) != 0 {
                op0(h, EE_UNITS_GOAL_HOST_X_RESEARCH_CLASS, player, b);
            }
            op_ints(h, EE_UNITS_GOAL_HOST_X_CHECK_BUTTONS, player, 0, &[io.i[0]]);
            op0(h, EE_UNITS_GOAL_HOST_X_SET_BUILT, b, 0);
            if op0(h, EE_UNITS_GOAL_HOST_X_CLASS_UNIT_TYPE, b, 0) == UNIT_TYPE_WALL_SPACE
                && op_ints(h, EE_UNITS_GOAL_HOST_X_PLAYER_TECH_AVAILABLE, player, 0, &[TECH_WALL_SPACE]) != 0
            {
                op0(h, EE_UNITS_GOAL_HOST_X_SPACE_WALL, b, 0);
            }
            if player == op0(h, EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER, 0, 0) as u64 {
                let sound = op0(h, EE_UNITS_GOAL_HOST_X_CREATION_SOUND, b, 0) as u64;
                if sound != 0 {
                    op0(h, EE_UNITS_GOAL_HOST_X_PLAY_SAMPLE, sound, 0);
                }
            }
            if op0(h, EE_UNITS_GOAL_HOST_X_MAIN_GAME_FORM, 0, 0) != 0 {
                op0(h, EE_UNITS_GOAL_HOST_X_REFRESH_GROUP, 0, 0);
            }
            0
        }
        EE_UNITS_GOAL_HOST_FILL_PARTS => {
            let mut part = op0(h, EE_UNITS_GOAL_HOST_X_NEXT_PART, a, 0) as u64;
            while part != 0 {
                op0(h, EE_UNITS_GOAL_HOST_X_FULL_HIT_POINTS, part, 0);
                part = op0(h, EE_UNITS_GOAL_HOST_X_NEXT_PART, part, 0) as u64;
            }
            0
        }
        EE_UNITS_GOAL_HOST_CANCEL_FOUNDATION => {
            // CancelFoundation: the foundation killed, its cost refunded to the worker's player
            let node = io.i[0] as u64;
            op0(h, EE_UNITS_GOAL_HOST_X_KILL, a, 0);
            for i in 2..7 {
                let value = op_ints(h, EE_UNITS_GOAL_HOST_X_SCALED_COST, b, node, &[i]);
                op_ints(h, EE_UNITS_GOAL_HOST_X_ADD_RESOURCE, b, 0, &[i, value]);
            }
            0
        }
        EE_UNITS_GOAL_HOST_BUILD_EVENT => {
            let event = if io.i[0] != 0 { 0x7c42 } else { 0x273a };
            op_ints(h, EE_UNITS_GOAL_HOST_X_ADD_EVENT, a, b, &[event]);
            0
        }
        EE_UNITS_GOAL_HOST_FOUNDATION_STARTED => {
            // Work's foundation become a building site: the wonder's notice and clock, the media, the parts
            if io.i[0] != 0 {
                if op0(h, EE_UNITS_GOAL_HOST_X_UNIT_PLAYER, a, 0) != op0(h, EE_UNITS_GOAL_HOST_X_CURRENT_PLAYER, 0, 0) {
                    op0(h, EE_UNITS_GOAL_HOST_X_WONDER_CHAT, a, b);
                }
                let sound = op_ints(h, EE_UNITS_GOAL_HOST_X_SOUND_GET, 0, 0, &[SOUND_WONDER_CLOCK]) as u64;
                if sound != 0 {
                    op0(h, EE_UNITS_GOAL_HOST_X_SOUND_PLAY_PLAIN, sound, 0);
                }
            }
            op0(h, EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA, a, b);
            op0(h, EE_UNITS_GOAL_HOST_X_LOAD_MEDIA, a, 0);
            op0(h, EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS, a, 0);
            let mut part = op0(h, EE_UNITS_GOAL_HOST_X_NEXT_PART, a, 0) as u64;
            while part != 0 {
                op_ints(h, EE_UNITS_GOAL_HOST_SET_HIT_POINTS, part, 0, &[2]);
                op0(h, EE_UNITS_GOAL_HOST_X_LOAD_CLASS_MEDIA, part, part);
                op0(h, EE_UNITS_GOAL_HOST_X_LOAD_MEDIA, part, 0);
                op0(h, EE_UNITS_GOAL_HOST_X_POST_UNLOAD_CLASS, part, 0);
                op0(h, EE_UNITS_GOAL_HOST_X_PART_FINISHED, part, 0);
                part = op0(h, EE_UNITS_GOAL_HOST_X_NEXT_PART, part, 0) as u64;
            }
            op0(h, EE_UNITS_GOAL_HOST_X_PART_FINISHED, a, 0);
            0
        }
        // the gather goal's
        EE_UNITS_GOAL_HOST_STORAGE_SEARCH => {
            // FindStorage's search (an AI may order a settlement for the gatherer instead: the resource manager's
            // BuildSettlementFor only cleared the computer player's selection; main's stage 6)
            let command = h.read(a, UnitRead::CommandType) as u32;
            op0(h, EE_UNITS_GOAL_HOST_RELEASE_FIRE, a, 0);
            if (command == ACTION_FIND_STORAGE || command == ACTION_FIND_NEW_STORAGE)
                && op0(h, EE_UNITS_GOAL_HOST_X_IS_HUMAN, a, 0) == 0
            {
                let cp = op0(h, EE_UNITS_GOAL_HOST_X_COMPUTER_PLAYER, a, 0) as u64;
                let group = if cp != 0 { op0(h, EE_UNITS_GOAL_HOST_X_CP_ACTIVE_GROUP, cp, 0) as u64 } else { 0 };
                if group != 0 && op0(h, EE_UNITS_GOAL_HOST_X_GROUP_NUM_UNITS, group, 0) > 0 {
                    op0(h, EE_UNITS_GOAL_HOST_X_GROUP_CLEAR, group, 0);
                }
            }
            let renew = h.read(a, UnitRead::CommandType) as u32 == ACTION_FIND_NEW_STORAGE;
            op_ints(h, EE_UNITS_GOAL_HOST_X_CITIZEN_FIND_STORAGE, a, 0, &[i64::from(renew)])
        }
        EE_UNITS_GOAL_HOST_HUNT => {
            if !(h.read(b, UnitRead::HitPoints) > 0
                && op_ints(h, EE_UNITS_GOAL_HOST_X_FIND_ACTION, a, 0, &[ACTION_HUNT]) != 0)
            {
                return Some(0);
            }
            op_ints(h, EE_UNITS_GOAL_HOST_X_SET_ATTACK_GOAL, a, b, &[io.i[0]]);
            op_ints(h, EE_UNITS_GOAL_HOST_X_START_ACTION, a, 0, &[ACTION_HUNT]);
            op0(h, EE_UNITS_GOAL_HOST_ACTION_CHANGED, a, 0);
            1
        }
        EE_UNITS_GOAL_HOST_FIND_GHOST => op0(
            h,
            if b != 0 { EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST } else { EE_UNITS_GOAL_HOST_X_FIND_LOADED_GHOST_UNIT },
            a,
            0,
        ),
        // the gathering's (world.rs)
        EE_UNITS_GOAL_HOST_TAKE_ONE => {
            op0(h, EE_UNITS_GOAL_HOST_X_DEPLETE_ONE, a, 0);
            op0(h, EE_UNITS_GOAL_HOST_X_ADD_RESOURCE_AMOUNT, b, 0);
            op0(h, EE_UNITS_GOAL_HOST_X_SET_LAST_RESOURCE, b, a);
            0
        }
        EE_UNITS_GOAL_HOST_WORK_TIMER => {
            // the gather time of the resource (variant-scaled except stone)
            let index = h.resource_index(b);
            let mut rate = op_ints(h, EE_UNITS_GOAL_HOST_X_GATHER_TIME, a, 0, &[index]) as i32;
            if h.read(b, UnitRead::ResourceType) as u64 != RESOURCE_STONE {
                let mut args = EeUnitsGoalIo::default();
                h.raw(EE_UNITS_GOAL_HOST_X_WORK_TIME_FACTOR, 0, 0, &mut args);
                rate = ee_base::math::um::lrintf(args.f[0] * rate as f32);
            }
            op_ints(h, EE_UNITS_GOAL_HOST_X_SET_WORK_TIMER, a, 0, &[i64::from(rate)]);
            0
        }
        _ => return None,
    })
}

/// The game's: `GoalOp` (the wrapped table) and the world views.
pub struct FfiSplit<'a>(pub &'a EeUnitsGoalHost);

impl GoalSplitHost for FfiSplit<'_> {
    fn raw(&mut self, op: i32, a: u64, b: u64, io: &mut EeUnitsGoalIo) -> i64 {
        // SAFETY: the table's contract: `context` and `op` are live for the FFI call that handed it out
        unsafe { (self.0.op)(self.0.context, op, a, b, io) }
    }
    fn read(&mut self, unit: u64, what: UnitRead) -> i64 {
        // SAFETY: a goal body runs inside the C++ call that handed out the host, which holds the world
        let token = unsafe { crate::world::WorldRead::begin() };
        // SAFETY: the host's unit handles are live units (the C++ dereferenced them as such)
        let u = unsafe { crate::world::UnitView::of(&token, unit) }.expect("a unit handle");
        match what {
            UnitRead::ActiveGroup => u.active_group_handle() as i64,
            UnitRead::HitPoints => u.hit_points(),
            UnitRead::ResourceAmount => u.resource_amount(),
            UnitRead::ResourceType => u.resource_type() as i64,
            UnitRead::CommandType => i64::from(u.command_type()),
            UnitRead::GarrisonCount => i64::from(crate::world_field!(u.complex(), mGarrisonCount)),
            UnitRead::CargoRoom => i64::from(crate::world_field!(u.complex(), mCargoRoom)),
            UnitRead::IsDisabled => {
                let v: bool = crate::world_field!(u.complex(), mIsDisabled);
                i64::from(v)
            }
            UnitRead::Animation => crate::world_field!(u.complex(), mAnimation) as i64,
            UnitRead::TileX => u.tile_xy().0,
            UnitRead::TileY => u.tile_xy().1,
            UnitRead::Id => i64::from(u.id()),
        }
    }
    fn position(&mut self, unit: u64) -> [f32; 3] {
        crate::units::unit_geom::position_of(unit)
    }
    fn deposit_load(&mut self, unit: u64, container: u64) {
        #[cfg(not(test))]
        crate::units::uu_ffi::ee_units_uu_deposit_load(unit, container);
        #[cfg(test)]
        let _ = (unit, container);
    }
    fn resource_index(&mut self, unit: u64) -> i64 {
        #[cfg(not(test))]
        return i64::from(crate::units::uu_ffi::ee_units_uu_resource_index(unit));
        #[cfg(test)]
        {
            let _ = unit;
            0
        }
    }
}

#[cfg(test)]
#[path = "goal_split_tests.rs"]
mod tests;
