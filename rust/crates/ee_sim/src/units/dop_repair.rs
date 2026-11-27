//! `EEComplexUnit::RepairPointers` over its C++ statements (the units' lane, round 7; was EEComplexDoppelganger.cpp).
//! Every C++ statement of the original is one operation of [`DopHost::dop`] (the game's: `ee_units_dop_host`,
//! EEComplexDoppelganger.cpp, one statement a case), asked in the original's order; the choices, loops and the
//! result's bookkeeping are here. `unit` is the `EEComplexUnit*`, `a` an object (a goal, a unit, a node), `b`, `c`
//! values.

// ---- the statements (`op`; the answer)
/// `EEUnit::RepairPointers(b, c)`
pub const EE_UNITS_DOP_BASE_REPAIR: i32 = 1;
/// `mCommand == NULL`
pub const EE_UNITS_DOP_COMMAND_IS_NULL: i32 = 2;
/// `GetUnitClass() != NULL`
pub const EE_UNITS_DOP_HAS_CLASS: i32 = 3;
/// `StartAction(b)`
pub const EE_UNITS_DOP_START_ACTION: i32 = 4;
/// `mPatrolGroup = a`
pub const EE_UNITS_DOP_SET_PATROL_GROUP: i32 = 5;
/// `mCurrentResearch = b`
pub const EE_UNITS_DOP_SET_CURRENT_RESEARCH: i32 = 6;
/// `mBuildQueue.clear()`
pub const EE_UNITS_DOP_BUILD_QUEUE_CLEAR: i32 = 7;
/// `mCitizenBuildQueue.clear()`
pub const EE_UNITS_DOP_CITIZEN_QUEUE_CLEAR: i32 = 8;
/// `mCargo.clear()`
pub const EE_UNITS_DOP_CARGO_CLEAR: i32 = 9;
/// `mPreviousPart = a`
pub const EE_UNITS_DOP_SET_PREVIOUS_PART: i32 = 10;
/// `mNextPart = a`
pub const EE_UNITS_DOP_SET_NEXT_PART: i32 = 11;
/// `mHomeBase = a`
pub const EE_UNITS_DOP_SET_HOME_BASE: i32 = 12;
/// `mContainer = a`
pub const EE_UNITS_DOP_SET_CONTAINER: i32 = 13;
/// `mGatheredResource = a`
pub const EE_UNITS_DOP_SET_GATHERED_FIELD: i32 = 14;
/// `mLastResource = a`
pub const EE_UNITS_DOP_SET_LAST_RESOURCE_FIELD: i32 = 15;
/// `mLastAttacker = a`
pub const EE_UNITS_DOP_SET_LAST_ATTACKER: i32 = 16;
/// `mOwner = a`
pub const EE_UNITS_DOP_SET_OWNER: i32 = 17;
/// `mMine = a`
pub const EE_UNITS_DOP_SET_MINE: i32 = 18;
/// `mHolder = a`
pub const EE_UNITS_DOP_SET_HOLDER: i32 = 19;
/// `mBehavior = a`
pub const EE_UNITS_DOP_SET_BEHAVIOR: i32 = 20;
/// `mHerd = a`
pub const EE_UNITS_DOP_SET_HERD: i32 = 21;
/// `mHeldUnits.size()`
pub const EE_UNITS_DOP_HELD_COUNT: i32 = 22;
/// `mHeldUnits[b] = a`
pub const EE_UNITS_DOP_SET_HELD: i32 = 23;
/// `mGoalQueues.size()`
pub const EE_UNITS_DOP_GOAL_QUEUE_COUNT: i32 = 24;
/// `mGoalQueues[b].mGoals.size()`
pub const EE_UNITS_DOP_GOAL_COUNT: i32 = 25;
/// `mGoalQueues[b].mGoals[c]`
pub const EE_UNITS_DOP_GOAL_AT: i32 = 26;
/// goal `a`'s `RepairPointers(b, c)`
pub const EE_UNITS_DOP_GOAL_REPAIR: i32 = 27;
/// goal `a`'s `mKind`
pub const EE_UNITS_DOP_GOAL_KIND: i32 = 28;
/// attack goal `a`'s `GetAttacker() != this`
pub const EE_UNITS_DOP_ATTACKER_IS_OTHER: i32 = 29;
/// attack goal `a`'s `SetAttacker(this)`
pub const EE_UNITS_DOP_SET_ATTACKER: i32 = 30;
/// `mGoalQueues[b].Clear()`
pub const EE_UNITS_DOP_GOAL_QUEUE_CLEAR: i32 = 31;
/// `mCitizenBuildQueue.size()`
pub const EE_UNITS_DOP_CITIZEN_COUNT: i32 = 32;
/// `mCitizenBuildQueue[b] = static_cast<EEUnit*>(gWorld.GetUnit(USavedID(mCitizenBuildQueue[b])))`
pub const EE_UNITS_DOP_CITIZEN_RESOLVE: i32 = 33;
/// `mCitizenBuildQueue[b]->Reference()`
pub const EE_UNITS_DOP_CITIZEN_REFERENCE: i32 = 34;
/// `mCargo.size()`
pub const EE_UNITS_DOP_CARGO_COUNT: i32 = 35;
/// `USavedID(mCargo[b])`
pub const EE_UNITS_DOP_CARGO_ID: i32 = 36;
/// `GetPlayer()->HasLoadedUnitID(b)`
pub const EE_UNITS_DOP_HAS_LOADED_ID: i32 = 37;
/// `EEUnitCast<EEComplexUnit>(gWorld.GetUnit(b))`
pub const EE_UNITS_DOP_WORLD_COMPLEX: i32 = 38;
/// unit `a`'s `Reference()`
pub const EE_UNITS_DOP_REFERENCE: i32 = 39;
/// the scratch cargo's `clear()` (the original's local `theCargo`)
pub const EE_UNITS_DOP_SCRATCH_CARGO_CLEAR: i32 = 40;
/// the scratch cargo's `push_back(a)`
pub const EE_UNITS_DOP_SCRATCH_CARGO_PUSH: i32 = 41;
/// `mCargo = the scratch cargo`
pub const EE_UNITS_DOP_CARGO_ASSIGN: i32 = 42;
/// `GetPlayer()->AddUnitToRepatrol(this)`
pub const EE_UNITS_DOP_ADD_REPATROL: i32 = 43;
/// `EELocationGoalCast(goal a)->GetIsAutoWaypoint() == true`
pub const EE_UNITS_DOP_LOCATION_AUTO: i32 = 44;
/// build goal `a`'s `GetBuildTechTreeID()`
pub const EE_UNITS_DOP_BUILD_TECH: i32 = 45;
/// `GetCommandType()`
pub const EE_UNITS_DOP_COMMAND_TYPE: i32 = 46;
/// `mCurrentPhysicsAction`
pub const EE_UNITS_DOP_PHYSICS_ACTION: i32 = 47;
/// `mCurrentPhysicsAction = b`
pub const EE_UNITS_DOP_SET_PHYSICS_ACTION: i32 = 48;
/// `mPatrolGroup == (EEPPatrolGroup*)-1`
pub const EE_UNITS_DOP_PATROL_IS_UNSET: i32 = 49;
/// `GetPlayer()->GetPatrolGroup(USavedID(mPatrolGroup))`
pub const EE_UNITS_DOP_PLAYER_PATROL_GROUP: i32 = 50;
/// `mPatrolGroup`
pub const EE_UNITS_DOP_PATROL_GROUP: i32 = 51;
/// `ee_units_pg_reference(mPatrolGroup)`
pub const EE_UNITS_DOP_PATROL_REFERENCE: i32 = 52;
/// `USavedID(mPreviousPart)`
pub const EE_UNITS_DOP_PREVIOUS_PART_ID: i32 = 53;
/// `USavedID(mNextPart)`
pub const EE_UNITS_DOP_NEXT_PART_ID: i32 = 54;
/// `USavedID(mHomeBase)`
pub const EE_UNITS_DOP_HOME_BASE_ID: i32 = 55;
/// `USavedID(mContainer)`
pub const EE_UNITS_DOP_CONTAINER_ID: i32 = 56;
/// complex unit `a`'s `mUnitClass->mTransportCapacity`
pub const EE_UNITS_DOP_TRANSPORT_CAPACITY: i32 = 57;
/// complex unit `a`'s `HasCargo(this)`
pub const EE_UNITS_DOP_HAS_CARGO_SELF: i32 = 58;
/// complex unit `a`'s `HasCargo(reinterpret_cast<EEComplexUnit*>(mUnitID))`
pub const EE_UNITS_DOP_HAS_CARGO_ID: i32 = 59;
/// `mCurrentResearch`
pub const EE_UNITS_DOP_CURRENT_RESEARCH: i32 = 60;
/// `mProductionTime = b`
pub const EE_UNITS_DOP_SET_PRODUCTION_TIME: i32 = 61;
/// `mPlayer->mTechnologies->Get(b)`
pub const EE_UNITS_DOP_TECH_NODE: i32 = 62;
/// `GetUnitClass()->IsLandBuilding()`
pub const EE_UNITS_DOP_IS_LAND_BUILDING: i32 = 63;
/// node `a`'s `mData->mUnitClass < 0 || gUnitClassTable[mData->mUnitClass] == NULL`
pub const EE_UNITS_DOP_NODE_CLASS_MISSING: i32 = 64;
/// node `a`'s `mIsAllowed = false`
pub const EE_UNITS_DOP_NODE_DISALLOW: i32 = 65;
/// `mBuildQueue.size()`
pub const EE_UNITS_DOP_BUILD_QUEUE_COUNT: i32 = 66;
/// `mBuildQueue[b].mTechnology`
pub const EE_UNITS_DOP_BUILD_QUEUE_TECH: i32 = 67;
/// the scratch research's `clear()` (the original's local `theResearch`)
pub const EE_UNITS_DOP_SCRATCH_RESEARCH_CLEAR: i32 = 68;
/// the scratch research's `push_back(mBuildQueue[b])`
pub const EE_UNITS_DOP_SCRATCH_RESEARCH_PUSH: i32 = 69;
/// `mBuildQueue = the scratch research`
pub const EE_UNITS_DOP_BUILD_QUEUE_ASSIGN: i32 = 70;
/// `dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(mGatheredResource)))`
pub const EE_UNITS_DOP_GATHERED_LINK: i32 = 71;
/// unit `a`'s `GetUnitID() != 0 || mResourceType != kResourceNone`
pub const EE_UNITS_DOP_LINK_USABLE: i32 = 72;
/// `SetGatheredResource(a)`
pub const EE_UNITS_DOP_SET_GATHERED_RESOURCE: i32 = 73;
/// `USavedID(mLastAttacker)`
pub const EE_UNITS_DOP_LAST_ATTACKER_ID: i32 = 74;
/// `EESaveHasGhosts(b, c)`
pub const EE_UNITS_DOP_HAS_GHOSTS: i32 = 75;
/// `EEFindLoadedGhost(b)`
pub const EE_UNITS_DOP_FIND_LOADED_GHOST: i32 = 76;
/// `mOwner == (EEComplexUnit*)-1 || mOwner == NULL`
pub const EE_UNITS_DOP_OWNER_IS_UNSET: i32 = 77;
/// `USavedID(mOwner)`
pub const EE_UNITS_DOP_OWNER_ID: i32 = 78;
/// `mMine == (EEUnit*)-1`
pub const EE_UNITS_DOP_MINE_IS_UNSET: i32 = 79;
/// `dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(mMine)))`
pub const EE_UNITS_DOP_MINE_LINK: i32 = 80;
/// `GetModel()->GetNumParts()`
pub const EE_UNITS_DOP_MODEL_PARTS: i32 = 81;
/// `mNumTurretAngles`
pub const EE_UNITS_DOP_NUM_TURRET_ANGLES: i32 = 82;
/// the scratch turret angles `= b == 0 ? NULL : new float[b]`
pub const EE_UNITS_DOP_SCRATCH_TURRETS_NEW: i32 = 83;
/// `mTurretAngles[b]` (its bits)
pub const EE_UNITS_DOP_TURRET_ANGLE: i32 = 84;
/// the scratch turret angles' `[b] = the float of the bits c`
pub const EE_UNITS_DOP_SCRATCH_TURRET_SET: i32 = 85;
/// `delete[] mTurretAngles`
pub const EE_UNITS_DOP_TURRETS_DELETE: i32 = 86;
/// `mTurretAngles = the scratch turret angles`
pub const EE_UNITS_DOP_TURRETS_ASSIGN: i32 = 87;
/// `GetStats()->mModel->mLastFrames[mAnimation] <= mPhysicalModel.GetFrame()`
pub const EE_UNITS_DOP_PAST_LAST_FRAME: i32 = 88;
/// `SetAnimationFrame(mStats->mModel->mStartFrames[mAnimation])`
pub const EE_UNITS_DOP_RESTART_ANIMATION: i32 = 89;
/// `gDataPreLoading->PostLoadTechnology(GetPlayer()->mPlayerID, b)`
pub const EE_UNITS_DOP_POST_LOAD_TECHNOLOGY: i32 = 90;
/// `GetTechTreeNode()`
pub const EE_UNITS_DOP_OWN_TECH_NODE: i32 = 91;
/// node `a`'s `mData->mKind`
pub const EE_UNITS_DOP_NODE_KIND: i32 = 92;
/// `GetHitPoints()`
pub const EE_UNITS_DOP_HIT_POINTS: i32 = 93;
/// `mIsBuilt == true`
pub const EE_UNITS_DOP_IS_BUILT: i32 = 94;
/// `GetPlayer()->AddWonder(this, b != 0)`
pub const EE_UNITS_DOP_ADD_WONDER: i32 = 95;
/// `mHeldUnits[b] = EEUnitCast<EEComplexUnit>(gWorld.GetUnit(USavedID(mHeldUnits[b])))`
pub const EE_UNITS_DOP_HELD_RESOLVE: i32 = 96;
/// `mHeldUnits[b]`
pub const EE_UNITS_DOP_HELD_AT: i32 = 97;
/// `mHeldUnits.erase(mHeldUnits.begin() + b)`
pub const EE_UNITS_DOP_HELD_ERASE: i32 = 98;
/// `mHolder == (EEComplexUnit*)-1`
pub const EE_UNITS_DOP_HOLDER_IS_UNSET: i32 = 99;
/// `USavedID(mHolder)`
pub const EE_UNITS_DOP_HOLDER_ID: i32 = 100;
/// `GetPlayer()->AddStrandedUnit(this)`
pub const EE_UNITS_DOP_ADD_STRANDED: i32 = 101;
/// `USavedID(mBehavior)`
pub const EE_UNITS_DOP_BEHAVIOR_ID: i32 = 102;
/// `EESaveHasLiveState(b, c)`
pub const EE_UNITS_DOP_HAS_LIVE_STATE: i32 = 103;
/// `EEUnitBehavior::Get(b)`
pub const EE_UNITS_DOP_BEHAVIOR_GET: i32 = 104;
/// `mBehavior`
pub const EE_UNITS_DOP_BEHAVIOR: i32 = 105;
/// `GetUnitClass()->mBehavior`
pub const EE_UNITS_DOP_CLASS_BEHAVIOR: i32 = 106;
/// `EEUnitBehavior::sTable[5]`
pub const EE_UNITS_DOP_DEFAULT_BEHAVIOR: i32 = 107;
/// `GetPlayer() != NULL`
pub const EE_UNITS_DOP_HAS_PLAYER: i32 = 108;
/// the animation's tag points (`mStats->mModel->mTagPoints[mAnimation]`) are there and at most `mWeaponTagPoint`
pub const EE_UNITS_DOP_TAGS_EXCEEDED: i32 = 109;
/// `mWeaponTagPoint = b`
pub const EE_UNITS_DOP_SET_WEAPON_TAG_POINT: i32 = 110;
/// `dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(mLastResource)))`
pub const EE_UNITS_DOP_LAST_RESOURCE_LINK: i32 = 111;
/// `SetLastResource(a)`
pub const EE_UNITS_DOP_SET_LAST_RESOURCE: i32 = 112;
/// `mAreaEffects.Clear()`
pub const EE_UNITS_DOP_AREA_EFFECTS_CLEAR: i32 = 113;
/// `mAreaEffects.mItems.clear()`
pub const EE_UNITS_DOP_AREA_EFFECTS_ITEMS_CLEAR: i32 = 114;
/// `mNumAreaEffects`
pub const EE_UNITS_DOP_NUM_AREA_EFFECTS: i32 = 115;
/// `MakeAreaEffects()`
pub const EE_UNITS_DOP_MAKE_AREA_EFFECTS: i32 = 116;
/// `EESaveHasRunningState(b, c)`
pub const EE_UNITS_DOP_HAS_RUNNING_STATE: i32 = 117;
/// `mNumAreaEffects = b`
pub const EE_UNITS_DOP_SET_NUM_AREA_EFFECTS: i32 = 118;
/// `RefreshGoals()`
pub const EE_UNITS_DOP_REFRESH_GOALS: i32 = 119;
/// `RefreshActiveGoal()`
pub const EE_UNITS_DOP_REFRESH_ACTIVE_GOAL: i32 = 120;
/// `mGoalQueues[0].mGoals.back()`
pub const EE_UNITS_DOP_FIRST_QUEUE_BACK: i32 = 121;
/// goal `a` is a location goal (`EELocationGoalCast`) with `GetIsAutoWaypoint()`
pub const EE_UNITS_DOP_IS_AUTO_WAYPOINT_GOAL: i32 = 122;
/// `mGoalQueues[0].PopBack()`
pub const EE_UNITS_DOP_FIRST_QUEUE_POP_BACK: i32 = 123;
/// `GetUnitClass()->mRottingTime`
pub const EE_UNITS_DOP_CLASS_ROTTING_TIME: i32 = 124;
/// `mRottingAwayTime`
pub const EE_UNITS_DOP_ROTTING_AWAY_TIME: i32 = 125;
/// `mRottingAwayTime = b`
pub const EE_UNITS_DOP_SET_ROTTING_AWAY_TIME: i32 = 126;
/// `mIsDeletable = GetUnitClass()->mIsDeletable`
pub const EE_UNITS_DOP_DELETABLE_FROM_CLASS: i32 = 127;
/// `GetUnitClass()->mCategory`
pub const EE_UNITS_DOP_CATEGORY: i32 = 128;
/// `mCommand->mType`
pub const EE_UNITS_DOP_COMMAND_KIND: i32 = 129;
/// `mResourceAmount`
pub const EE_UNITS_DOP_RESOURCE_AMOUNT: i32 = 130;
/// `mResourceAmount = b`
pub const EE_UNITS_DOP_SET_RESOURCE_AMOUNT: i32 = 131;

/// `kActionIdle`, `kActionDying`, `kActionResourcesRotting`
const ACTION_IDLE: i64 = 5000;
const ACTION_DYING: i64 = 6000;
const ACTION_RESOURCES_ROTTING: i64 = 6001;
/// `kGoalLocation`, `kGoalAttackUnit`, `kGoalBuildRepair`, `kGoalPatrol`
const GOAL_LOCATION: i64 = 1;
const GOAL_ATTACK_UNIT: i64 = 2;
const GOAL_BUILD_REPAIR: i64 = 4;
const GOAL_PATROL: i64 = 9;
/// `kUnitCategoryLandCitizen`, `kUnitCategoryNavalCitizen`
const CATEGORY_LAND_CITIZEN: i64 = 6;
const CATEGORY_NAVAL_CITIZEN: i64 = 0x16;
/// `kTechNone`
const TECH_NONE: i64 = 100000;
/// the technologies (below) and the units' production ids (from 200000 on)
const TECH_LIMIT: i64 = 200000;
/// a tech tree node's kind of a wonder
const NODE_KIND_WONDER: i64 = 4;
/// the physics action RepairPointers forces after a failed first queue
const PHYSICS_ACTION_REPAIRED: i64 = 3;
/// the saved behaviours are indices below this; `EEUnitBehavior::sTable`'s size
const BEHAVIOR_COUNT: i64 = 8;
/// the harass and cautious behaviours
const BEHAVIOR_HARASS: i64 = 3;
const BEHAVIOR_CAUTIOUS: i64 = 4;

/// The C++ statements of the body.
pub trait DopHost {
    /// one statement (`EE_UNITS_DOP_*`) on the complex unit `unit`
    fn dop(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64;
}

struct S<'h, H: DopHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: DopHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.dop(op, self.unit, 0, 0, 0)
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.dop(op, self.unit, a, 0, 0)
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.dop(op, self.unit, 0, b, 0)
    }
    fn qab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.dop(op, self.unit, a, b, 0)
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.dop(op, self.unit, 0, b, c)
    }
    fn qabc(&mut self, op: i32, a: u64, b: i64, c: i64) -> i64 {
        self.h.dop(op, self.unit, a, b, c)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
    fn obj(&mut self, op: i32) -> u64 {
        self.q(op) as u64
    }
    /// a saved id's complex unit, referenced when there (`EEUnitCast` of `gWorld.GetUnit`)
    fn complex_of(&mut self, id_op: i32) -> u64 {
        let id = self.q(id_op);
        self.qb(EE_UNITS_DOP_WORLD_COMPLEX, id) as u64
    }
}

/// The repair of one goal: the old saves' (< 0xe0) attack goals take this unit as their attacker again
fn fix_attacker<H: DopHost + ?Sized>(s: &mut S<'_, H>, goal: u64, major: i64, minor: i64) {
    if s.qa(EE_UNITS_DOP_GOAL_KIND, goal) == GOAL_ATTACK_UNIT
        && major == 0
        && minor < 0xe0
        && s.qa(EE_UNITS_DOP_ATTACKER_IS_OTHER, goal) != 0
    {
        s.qa(EE_UNITS_DOP_SET_ATTACKER, goal);
    }
}

/// `EEComplexUnit::RepairPointers(major, minor)`: the loaded ids back into units, groups, research and behaviour,
/// dropping the missing ones; a unit whose transport or owner vanished is stranded; a goal queue that fails to
/// repair is cleared. The last status (a goal's repair or the patrol group's 3).
pub fn repair_pointers<H: DopHost + ?Sized>(h: &mut H, unit: u64, major: i64, minor: i64) -> i64 {
    let s = &mut S { h, unit };
    let mut lost_container = false;
    let mut auto_waypoint = false;
    let mut result = s.qbc(EE_UNITS_DOP_BASE_REPAIR, major, minor);
    if result != 0 {
        return result;
    }
    if s.yes(EE_UNITS_DOP_COMMAND_IS_NULL) && s.yes(EE_UNITS_DOP_HAS_CLASS) {
        s.qb(EE_UNITS_DOP_START_ACTION, ACTION_IDLE);
    }
    if !s.yes(EE_UNITS_DOP_HAS_CLASS) {
        s.qa(EE_UNITS_DOP_SET_PATROL_GROUP, 0);
        s.qb(EE_UNITS_DOP_SET_CURRENT_RESEARCH, TECH_NONE);
        s.q(EE_UNITS_DOP_BUILD_QUEUE_CLEAR);
        s.q(EE_UNITS_DOP_CITIZEN_QUEUE_CLEAR);
        s.q(EE_UNITS_DOP_CARGO_CLEAR);
        for op in [
            EE_UNITS_DOP_SET_PREVIOUS_PART,
            EE_UNITS_DOP_SET_NEXT_PART,
            EE_UNITS_DOP_SET_HOME_BASE,
            EE_UNITS_DOP_SET_CONTAINER,
            EE_UNITS_DOP_SET_GATHERED_FIELD,
            EE_UNITS_DOP_SET_LAST_RESOURCE_FIELD,
            EE_UNITS_DOP_SET_LAST_ATTACKER,
            EE_UNITS_DOP_SET_OWNER,
            EE_UNITS_DOP_SET_MINE,
            EE_UNITS_DOP_SET_HOLDER,
            EE_UNITS_DOP_SET_BEHAVIOR,
            EE_UNITS_DOP_SET_HERD,
        ] {
            s.qa(op, 0);
        }
        let mut empty = s.q(EE_UNITS_DOP_HELD_COUNT) - 1;
        while empty >= 0 {
            s.qab(EE_UNITS_DOP_SET_HELD, 0, empty);
            empty -= 1;
        }
        let mut item = 0;
        while item < s.q(EE_UNITS_DOP_GOAL_QUEUE_COUNT) {
            let mut failed = false;
            let mut goal = 0;
            while goal < s.qb(EE_UNITS_DOP_GOAL_COUNT, item) {
                let g = s.qbc(EE_UNITS_DOP_GOAL_AT, item, goal) as u64;
                result = s.qabc(EE_UNITS_DOP_GOAL_REPAIR, g, major, minor);
                if result == 0 {
                    fix_attacker(s, g, major, minor);
                }
                if !failed {
                    failed = result != 0;
                }
                goal += 1;
            }
            if failed {
                s.qb(EE_UNITS_DOP_GOAL_QUEUE_CLEAR, item);
            }
            item += 1;
        }
        return result;
    }
    let count = s.q(EE_UNITS_DOP_CITIZEN_COUNT);
    for k in 0..count {
        s.qb(EE_UNITS_DOP_CITIZEN_RESOLVE, k);
        s.qb(EE_UNITS_DOP_CITIZEN_REFERENCE, k);
    }
    s.q(EE_UNITS_DOP_SCRATCH_CARGO_CLEAR);
    let count = s.q(EE_UNITS_DOP_CARGO_COUNT);
    for k in 0..count {
        let id = s.qb(EE_UNITS_DOP_CARGO_ID, k);
        if s.qb(EE_UNITS_DOP_HAS_LOADED_ID, id) != 0 {
            continue;
        }
        let id = s.qb(EE_UNITS_DOP_CARGO_ID, k);
        let cargo = s.qb(EE_UNITS_DOP_WORLD_COMPLEX, id) as u64;
        s.qa(EE_UNITS_DOP_REFERENCE, cargo);
        s.qa(EE_UNITS_DOP_SCRATCH_CARGO_PUSH, cargo);
    }
    s.q(EE_UNITS_DOP_CARGO_CLEAR);
    s.q(EE_UNITS_DOP_CARGO_ASSIGN);
    let mut item = 0;
    while item < s.q(EE_UNITS_DOP_GOAL_QUEUE_COUNT) {
        let mut failed = false;
        let mut goal = 0;
        while goal < s.qb(EE_UNITS_DOP_GOAL_COUNT, item) {
            let g = s.qbc(EE_UNITS_DOP_GOAL_AT, item, goal) as u64;
            result = s.qabc(EE_UNITS_DOP_GOAL_REPAIR, g, major, minor);
            if result == 0 {
                fix_attacker(s, g, major, minor);
            } else if s.qa(EE_UNITS_DOP_GOAL_KIND, g) == GOAL_PATROL {
                s.q(EE_UNITS_DOP_ADD_REPATROL);
            }
            if !failed {
                failed = result != 0;
            }
            auto_waypoint =
                s.qa(EE_UNITS_DOP_GOAL_KIND, g) == GOAL_LOCATION && s.qa(EE_UNITS_DOP_LOCATION_AUTO, g) != 0;
            if result == 0 && item == 0 && s.qa(EE_UNITS_DOP_GOAL_KIND, g) == GOAL_BUILD_REPAIR {
                let tech = s.qa(EE_UNITS_DOP_BUILD_TECH, g);
                s.qb(EE_UNITS_DOP_SET_CURRENT_RESEARCH, tech);
            }
            goal += 1;
        }
        if failed {
            s.qb(EE_UNITS_DOP_GOAL_QUEUE_CLEAR, item);
            if !(item != 0 || !s.yes(EE_UNITS_DOP_HAS_CLASS) || s.q(EE_UNITS_DOP_COMMAND_TYPE) == ACTION_DYING) {
                s.qb(EE_UNITS_DOP_START_ACTION, ACTION_IDLE);
                if s.q(EE_UNITS_DOP_PHYSICS_ACTION) != PHYSICS_ACTION_REPAIRED {
                    s.qb(EE_UNITS_DOP_SET_PHYSICS_ACTION, PHYSICS_ACTION_REPAIRED);
                }
            }
        }
        item += 1;
    }
    if s.yes(EE_UNITS_DOP_PATROL_IS_UNSET) {
        s.qa(EE_UNITS_DOP_SET_PATROL_GROUP, 0);
    } else {
        let group = s.obj(EE_UNITS_DOP_PLAYER_PATROL_GROUP);
        s.qa(EE_UNITS_DOP_SET_PATROL_GROUP, group);
        if s.obj(EE_UNITS_DOP_PATROL_GROUP) != 0 {
            s.q(EE_UNITS_DOP_PATROL_REFERENCE);
        } else {
            result = 3;
        }
    }
    for (id_op, set_op) in [
        (EE_UNITS_DOP_PREVIOUS_PART_ID, EE_UNITS_DOP_SET_PREVIOUS_PART),
        (EE_UNITS_DOP_NEXT_PART_ID, EE_UNITS_DOP_SET_NEXT_PART),
        (EE_UNITS_DOP_HOME_BASE_ID, EE_UNITS_DOP_SET_HOME_BASE),
    ] {
        let part = s.complex_of(id_op);
        s.qa(set_op, part);
        if part != 0 {
            s.qa(EE_UNITS_DOP_REFERENCE, part);
        }
    }
    let container = s.complex_of(EE_UNITS_DOP_CONTAINER_ID);
    s.qa(EE_UNITS_DOP_SET_CONTAINER, container);
    if container != 0 {
        // a not-yet-repaired cargo vector still holds ids; a repaired one the units
        if s.qa(EE_UNITS_DOP_TRANSPORT_CAPACITY, container) < 1
            || (s.qa(EE_UNITS_DOP_HAS_CARGO_SELF, container) == 0 && s.qa(EE_UNITS_DOP_HAS_CARGO_ID, container) == 0)
        {
            s.qa(EE_UNITS_DOP_SET_CONTAINER, 0);
            lost_container = true;
        } else {
            s.qa(EE_UNITS_DOP_REFERENCE, container);
        }
    }
    let research = s.q(EE_UNITS_DOP_CURRENT_RESEARCH);
    if research == TECH_NONE {
        s.qb(EE_UNITS_DOP_SET_PRODUCTION_TIME, 0);
    } else if research < TECH_LIMIT && s.qb(EE_UNITS_DOP_TECH_NODE, research) == 0 {
        s.qb(EE_UNITS_DOP_SET_CURRENT_RESEARCH, TECH_NONE);
        s.qb(EE_UNITS_DOP_SET_PRODUCTION_TIME, 0);
    }
    if s.yes(EE_UNITS_DOP_IS_LAND_BUILDING) {
        let research = s.q(EE_UNITS_DOP_CURRENT_RESEARCH);
        if research != TECH_NONE && research < TECH_LIMIT {
            let node = s.qb(EE_UNITS_DOP_TECH_NODE, research) as u64;
            if node != 0 && s.qa(EE_UNITS_DOP_NODE_CLASS_MISSING, node) != 0 {
                s.qa(EE_UNITS_DOP_NODE_DISALLOW, node);
            }
        }
    }
    s.q(EE_UNITS_DOP_SCRATCH_RESEARCH_CLEAR);
    let mut k = 0;
    while k < s.q(EE_UNITS_DOP_BUILD_QUEUE_COUNT) {
        let tech = s.qb(EE_UNITS_DOP_BUILD_QUEUE_TECH, k);
        if tech < TECH_LIMIT {
            let node = s.qb(EE_UNITS_DOP_TECH_NODE, tech) as u64;
            if node != 0 {
                if s.qa(EE_UNITS_DOP_NODE_CLASS_MISSING, node) != 0 {
                    s.qa(EE_UNITS_DOP_NODE_DISALLOW, node);
                }
                s.qb(EE_UNITS_DOP_SCRATCH_RESEARCH_PUSH, k);
            }
        }
        k += 1;
    }
    s.q(EE_UNITS_DOP_BUILD_QUEUE_CLEAR);
    s.q(EE_UNITS_DOP_BUILD_QUEUE_ASSIGN);
    let link = s.obj(EE_UNITS_DOP_GATHERED_LINK);
    s.qa(EE_UNITS_DOP_SET_GATHERED_FIELD, 0);
    if link != 0 && s.qa(EE_UNITS_DOP_LINK_USABLE, link) != 0 {
        s.qa(EE_UNITS_DOP_SET_GATHERED_RESOURCE, link);
    }
    let attacker_id = s.q(EE_UNITS_DOP_LAST_ATTACKER_ID);
    let mut attacker = s.qb(EE_UNITS_DOP_WORLD_COMPLEX, attacker_id) as u64;
    s.qa(EE_UNITS_DOP_SET_LAST_ATTACKER, attacker);
    if attacker == 0 && s.qbc(EE_UNITS_DOP_HAS_GHOSTS, major, minor) != 0 {
        // port (0.262): an attacker that died, as the saved game kept it
        attacker = s.qb(EE_UNITS_DOP_FIND_LOADED_GHOST, attacker_id) as u64;
        s.qa(EE_UNITS_DOP_SET_LAST_ATTACKER, attacker);
    }
    if attacker != 0 {
        s.qa(EE_UNITS_DOP_REFERENCE, attacker);
    }
    if s.yes(EE_UNITS_DOP_OWNER_IS_UNSET) {
        s.qa(EE_UNITS_DOP_SET_OWNER, 0);
    } else {
        let owner = s.complex_of(EE_UNITS_DOP_OWNER_ID);
        s.qa(EE_UNITS_DOP_SET_OWNER, owner);
        if owner != 0 {
            s.qa(EE_UNITS_DOP_REFERENCE, owner);
        } else {
            lost_container = true;
        }
    }
    if s.yes(EE_UNITS_DOP_MINE_IS_UNSET) || !s.yes(EE_UNITS_DOP_HAS_CLASS) {
        s.qa(EE_UNITS_DOP_SET_MINE, 0);
    } else {
        let mine = s.obj(EE_UNITS_DOP_MINE_LINK);
        s.qa(EE_UNITS_DOP_SET_MINE, mine);
        if mine != 0 {
            s.qa(EE_UNITS_DOP_REFERENCE, mine);
        }
    }
    let parts = s.q(EE_UNITS_DOP_MODEL_PARTS) as u32;
    let old = s.q(EE_UNITS_DOP_NUM_TURRET_ANGLES);
    if i64::from(parts) != old {
        s.qb(EE_UNITS_DOP_SCRATCH_TURRETS_NEW, i64::from(parts));
        for part in 0..parts {
            // `part < (unsigned long)mNumTurretAngles`
            let bits = if part < old as u32 { s.qb(EE_UNITS_DOP_TURRET_ANGLE, i64::from(part)) } else { 0 };
            s.qbc(EE_UNITS_DOP_SCRATCH_TURRET_SET, i64::from(part), bits);
        }
        s.q(EE_UNITS_DOP_TURRETS_DELETE);
        s.q(EE_UNITS_DOP_TURRETS_ASSIGN);
    }
    if s.yes(EE_UNITS_DOP_PAST_LAST_FRAME) {
        s.q(EE_UNITS_DOP_RESTART_ANIMATION);
    }
    let research = s.q(EE_UNITS_DOP_CURRENT_RESEARCH);
    if research != TECH_NONE {
        s.qb(EE_UNITS_DOP_POST_LOAD_TECHNOLOGY, research);
    }
    let node = s.obj(EE_UNITS_DOP_OWN_TECH_NODE);
    if node != 0
        && s.qa(EE_UNITS_DOP_NODE_KIND, node) == NODE_KIND_WONDER
        && s.q(EE_UNITS_DOP_HIT_POINTS) > 0
        && s.yes(EE_UNITS_DOP_IS_BUILT)
    {
        s.qb(EE_UNITS_DOP_ADD_WONDER, i64::from(!(major == 0 && minor < 0xe2)));
    }
    s.qa(EE_UNITS_DOP_SET_HERD, 0);
    let mut linked = s.q(EE_UNITS_DOP_HELD_COUNT) - 1;
    while linked >= 0 {
        s.qb(EE_UNITS_DOP_HELD_RESOLVE, linked);
        let held = s.qb(EE_UNITS_DOP_HELD_AT, linked) as u64;
        if held != 0 {
            s.qa(EE_UNITS_DOP_REFERENCE, held);
        } else {
            s.qb(EE_UNITS_DOP_HELD_ERASE, linked);
        }
        linked -= 1;
    }
    if s.yes(EE_UNITS_DOP_HOLDER_IS_UNSET) {
        s.qa(EE_UNITS_DOP_SET_HOLDER, 0);
    } else {
        let holder = s.complex_of(EE_UNITS_DOP_HOLDER_ID);
        s.qa(EE_UNITS_DOP_SET_HOLDER, holder);
        if holder != 0 {
            s.qa(EE_UNITS_DOP_REFERENCE, holder);
        }
    }
    if lost_container {
        s.q(EE_UNITS_DOP_ADD_STRANDED);
    }
    let behavior = s.q(EE_UNITS_DOP_BEHAVIOR_ID);
    if behavior < BEHAVIOR_COUNT {
        // port (saves from 0.254 on): the harass and cautious behaviours stay as saved (see the C++ history)
        if (behavior == BEHAVIOR_HARASS || behavior == BEHAVIOR_CAUTIOUS)
            && s.qbc(EE_UNITS_DOP_HAS_LIVE_STATE, major, minor) == 0
        {
            s.qa(EE_UNITS_DOP_SET_BEHAVIOR, 0);
        } else {
            let b = s.qb(EE_UNITS_DOP_BEHAVIOR_GET, behavior) as u64;
            s.qa(EE_UNITS_DOP_SET_BEHAVIOR, b);
        }
    } else {
        let mut k = 0;
        while k < BEHAVIOR_COUNT {
            let current = s.obj(EE_UNITS_DOP_BEHAVIOR);
            if current == s.qb(EE_UNITS_DOP_BEHAVIOR_GET, k) as u64 {
                break;
            }
            k += 1;
        }
        if k == BEHAVIOR_COUNT {
            s.qa(EE_UNITS_DOP_SET_BEHAVIOR, 0);
        }
    }
    if s.obj(EE_UNITS_DOP_BEHAVIOR) == 0 {
        if s.yes(EE_UNITS_DOP_HAS_CLASS) {
            let b = s.obj(EE_UNITS_DOP_CLASS_BEHAVIOR);
            s.qa(EE_UNITS_DOP_SET_BEHAVIOR, b);
        }
        if s.obj(EE_UNITS_DOP_BEHAVIOR) == 0 {
            let b = s.obj(EE_UNITS_DOP_DEFAULT_BEHAVIOR);
            s.qa(EE_UNITS_DOP_SET_BEHAVIOR, b);
        }
    }
    if s.yes(EE_UNITS_DOP_HAS_PLAYER) && s.yes(EE_UNITS_DOP_TAGS_EXCEEDED) {
        s.qb(EE_UNITS_DOP_SET_WEAPON_TAG_POINT, 0);
    }
    let link = s.obj(EE_UNITS_DOP_LAST_RESOURCE_LINK);
    s.qa(EE_UNITS_DOP_SET_LAST_RESOURCE_FIELD, 0);
    if link != 0 && s.qa(EE_UNITS_DOP_LINK_USABLE, link) != 0 {
        s.qa(EE_UNITS_DOP_SET_LAST_RESOURCE, link);
    }
    if s.q(EE_UNITS_DOP_HIT_POINTS) < 1 {
        s.q(EE_UNITS_DOP_AREA_EFFECTS_CLEAR);
        s.q(EE_UNITS_DOP_AREA_EFFECTS_ITEMS_CLEAR);
    } else {
        // port (0.256): the saved count stays (MakeAreaEffects counts the templates)
        let saved = s.q(EE_UNITS_DOP_NUM_AREA_EFFECTS);
        s.q(EE_UNITS_DOP_MAKE_AREA_EFFECTS);
        if s.qbc(EE_UNITS_DOP_HAS_RUNNING_STATE, major, minor) != 0 {
            s.qb(EE_UNITS_DOP_SET_NUM_AREA_EFFECTS, saved);
        }
    }
    s.q(EE_UNITS_DOP_REFRESH_GOALS);
    s.q(EE_UNITS_DOP_REFRESH_ACTIVE_GOAL);
    if auto_waypoint {
        let mut k = s.qb(EE_UNITS_DOP_GOAL_COUNT, 0) - 1;
        while k >= 0 {
            let goal = s.obj(EE_UNITS_DOP_FIRST_QUEUE_BACK);
            if goal == 0 || s.qa(EE_UNITS_DOP_IS_AUTO_WAYPOINT_GOAL, goal) == 0 {
                break;
            }
            s.q(EE_UNITS_DOP_FIRST_QUEUE_POP_BACK);
            k -= 1;
        }
    }
    let rotting = s.q(EE_UNITS_DOP_CLASS_ROTTING_TIME);
    if rotting < s.q(EE_UNITS_DOP_ROTTING_AWAY_TIME) {
        s.qb(EE_UNITS_DOP_SET_ROTTING_AWAY_TIME, rotting);
    }
    if major == 0 && minor <= 0xca {
        s.q(EE_UNITS_DOP_DELETABLE_FROM_CLASS);
    }
    let category = s.q(EE_UNITS_DOP_CATEGORY);
    if (category == CATEGORY_LAND_CITIZEN || category == CATEGORY_NAVAL_CITIZEN)
        && s.q(EE_UNITS_DOP_COMMAND_KIND) == ACTION_RESOURCES_ROTTING
        && s.q(EE_UNITS_DOP_RESOURCE_AMOUNT) != 0
    {
        s.qb(EE_UNITS_DOP_SET_RESOURCE_AMOUNT, 0);
    }
    result
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_dop_host(op: i32, unit: *mut c_void, a: u64, b: i64, c: i64) -> i64;
    }

    /// the game's host (dop_load.rs's live state uses it too: one module declares the import)
    pub(crate) struct Game;

    impl DopHost for Game {
        fn dop(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live unit (the plain record reads
            // and stores first: cu_view.rs; a doppelganger's record from the host)
            unsafe {
                let mut fields = || {
                    ee_units_dop_host(
                        crate::units::dop_file::EE_UNITS_DOP_FIELDS,
                        unit as usize as *mut c_void,
                        0,
                        0,
                        0,
                    ) as usize as *mut crate::units::obj_fields::EeUnitsComplexDopFields
                };
                if let Some(v) = crate::units::cu_view::dop_view(op, unit, a, b, c, &mut fields) {
                    return v;
                }
                ee_units_dop_host(op, unit as usize as *mut c_void, a, b, c)
            }
        }
    }

    /// `EEComplexUnit::RepairPointers(major, minor)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEComplexUnit` being loaded, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_dop_repair_pointers(unit: *mut c_void, major: u32, minor: u32) -> i64 {
        repair_pointers(&mut Game, unit as usize as u64, i64::from(major), i64::from(minor))
    }
}

#[cfg(test)]
#[path = "dop_repair_tests.rs"]
mod tests;
