use super::*;

/// a zeroed row (every field of the generated record is an integer, float or byte array)
fn row() -> EEUnitClassRecord {
    // SAFETY: all-zero bytes are a valid value of every field
    unsafe { std::mem::zeroed() }
}

fn input() -> EeUnitsUnitClassFillIn {
    EeUnitsUnitClassFillIn { is_complex: 1, ..EeUnitsUnitClassFillIn::default() }
}

#[test]
fn the_record_has_the_c_layout_of_its_members() {
    let long = std::mem::size_of::<c_long>();
    let f = EeUnitsUnitClassFields::default();
    let base = std::ptr::addr_of!(f) as usize;
    assert_eq!(std::ptr::addr_of!(f.mAirTrafficType) as usize - base, 0);
    assert_eq!(std::ptr::addr_of!(f.mCommands) as usize - base, long);
    assert_eq!(std::ptr::addr_of!(f.mCruiseAltitude) as usize - base, 10 * long);
    // the floats to mLaunchPitch, then a long
    assert_eq!(std::ptr::addr_of!(f.mL180) as usize - base, (10 * long + 25 * 4).next_multiple_of(long));
    // the bools end the record, mDomains first
    assert!(std::ptr::addr_of!(f.mHasSpecialAttack) as usize > std::ptr::addr_of!(f.mDomains) as usize);
    assert_eq!(std::ptr::addr_of!(f.mB2D1) as usize, std::ptr::addr_of!(f.mDomains) as usize + 1);
}

#[test]
fn a_zero_row_takes_the_constructors_defaults() {
    let f = fill(&row(), &input());
    assert_eq!(f.mIsComplex, 1);
    assert_eq!(f.mScale, 0.0);
    assert_eq!(f.mFiringArc.to_bits(), 0.349_065_87f32.to_bits());
    assert_eq!(f.mF19C, -20.0);
    assert_eq!((f.mBuildTimeScale, f.mCostScale, f.mAreaEffectScale), (1.0, 1.0, 1.0));
    assert_eq!(f.mAnimalID, 1000);
    assert!(f.mB2D1);
    // category 0 (ambient), type 0: no kind, no domain; resource 0 is not wood
    assert_eq!((f.mKindFlags, f.mDomains, f.mIsTree), (0, 0, false));
    assert!(!f.mIsMedic && !f.mIsFlare && !f.mIsBoobyTrap && !f.mIsParatrooperBomber);
}

#[test]
fn the_rows_numbers_convert_as_the_original() {
    let mut r = row();
    r.line_of_sight = 40.0;
    r.range = 33.0;
    r.scale = 0.0001;
    r.reload_time = 1.5;
    r.flight_time = 3;
    r.rotting_time = 0.0025;
    r.missile_delay_time = 2.9999;
    r.resource_amount = -7.9;
    r.conversion_rate = 35;
    r.launch_pitch = 90.0;
    r.bob_pitch = 180.0;
    r.unused_defaulted_value = 4.5f32.to_ne_bytes();
    r.unused_shadow_offset = 0.25f32.to_ne_bytes();
    r.unused_unit_group = (-2i32).to_ne_bytes();
    r.family = -1;
    r.category = CATEGORY_LAND as i32;
    r.wood_gather_time = 4.9;
    r.forage_gather_time = 6.0;
    r.farm_gather_time = 7.0;
    let f = fill(&r, &input());
    assert_eq!(f.mLineOfSight, 32.0);
    // the range against the clamped line of sight
    assert_eq!(f.mKindFlags, UNIT_KIND_LONG_RANGE_MILITARY);
    assert_eq!(f.mScale, 0.0);
    assert_eq!(f.mReloadTime, 1500);
    assert_eq!(f.mFlightTime, 3000);
    assert_eq!(f.mRottingTime, 2);
    assert_eq!(f.mMissileDelayTime, 2999);
    assert_eq!(f.mResourceAmount, -7);
    assert_eq!(f.mConversionRate.to_bits(), (35.0f32 * 0.01).to_bits());
    assert_eq!(f.mLaunchPitch.to_bits(), (90.0f32 * 0.017_453_292).to_bits());
    assert_eq!(f.mBobPitch.to_bits(), (180.0f32 * 0.017_453_292).to_bits());
    assert_eq!(f.mF19C, 4.5);
    assert_eq!(f.mL170, 0.25);
    assert_eq!(f.mL208, -2);
    // an unsigned long from an int32: sign-extended
    assert_eq!(f.mFamily, c_ulong::MAX);
    assert_eq!(f.mDomains, MOVEMENT_LAND);
    // wood, stone, gold, iron, hunt, forage, farm
    assert_eq!(f.mGatherTimes, [4, 0, 0, 0, 0, 6, 7]);
}

#[test]
fn the_terrain_table_copies_land_into_snow() {
    let mut r = row();
    r.can_move_on_land = 1;
    r.can_cross_forest = 1;
    r.can_move_in_space = 2;
    let f = fill(&r, &input());
    assert!(f.mTerrainTypes[0] && f.mTerrainTypes[TERRAIN_SNOW] && f.mTerrainTypes[14] && f.mCanCrossForest);
    // a byte other than 0/1 is true
    assert!(f.mTerrainTypes[TERRAIN_SPACE]);
    assert_eq!(f.mTerrainTypes.iter().filter(|b| **b).count(), 4);
}

#[test]
fn towers_buildings_and_transports() {
    let mut r = row();
    r.category = CATEGORY_BUILDING as i32;
    r.unit_type = UNIT_TYPE_GUARD_TOWER_DIGITAL as i32;
    r.can_enter_sea_transports = 1;
    let f = fill(&r, &input());
    assert!(f.mIsTower);
    assert_eq!(f.mDomains, MOVEMENT_LAND);
    // the sea flag also sets the space one
    assert!(f.mCanEnterSpaceTransports);
    assert_eq!(
        f.mKindFlags,
        UNIT_KIND_MILITARY | UNIT_KIND_CAN_ENTER_SEA_TRANSPORTS | UNIT_KIND_CAN_ENTER_SPACE_TRANSPORTS
    );
    r.unit_type = (UNIT_TYPE_AMB_STARFISH3) as i32;
    let f = fill(&r, &input());
    assert!(!f.mIsTower);
    assert_eq!(f.mDomains, 0);
    // a naval building with air traffic is military
    r.category = CATEGORY_NAVAL_BUILDING as i32;
    r.air_traffic_type = 2;
    r.can_enter_sea_transports = 0;
    let f = fill(&r, &input());
    assert_eq!((f.mKindFlags, f.mDomains, f.mAirTrafficType), (UNIT_KIND_MILITARY, MOVEMENT_SEA, 2));
    r.air_traffic_type = 0;
    assert_eq!(fill(&r, &input()).mKindFlags, UNIT_KIND_BUILDING);
    // the prophet is military, other priests are priests
    r.category = CATEGORY_LAND_PRIEST as i32;
    r.unit_type = UNIT_TYPE_PROPHET as i32;
    assert_eq!(fill(&r, &input()).mKindFlags, UNIT_KIND_MILITARY);
    r.unit_type = 1;
    assert_eq!(fill(&r, &input()).mKindFlags, UNIT_KIND_PRIEST);
    r.category = CATEGORY_TORPEDO as i32;
    let f = fill(&r, &input());
    assert!(f.mIsProjectile);
    assert_eq!(f.mKindFlags, 0);
    r.category = CATEGORY_AMBIENT as i32;
    r.unit_type = UNIT_TYPE_PATH_POINT as i32;
    assert_eq!(fill(&r, &input()).mKindFlags, UNIT_KIND_PATH_POINT);
    r.unit_type = UNIT_TYPE_RES_DOLPHIN as i32;
    r.is_collidable = 1;
    assert!(!fill(&r, &input()).mIsCollidable);
    r.unit_type = UNIT_TYPE_CATALINA as i32;
    assert!(fill(&r, &input()).mCanEnterNavyYards);
}

#[test]
fn the_lookups_passed_in_and_the_influence() {
    let mut r = row();
    r.epoch = 4;
    r.attack = 3;
    // wood
    r.resource_type = 3;
    let mut i = input();
    assert_eq!(fill(&r, &i).mEpoch, 4);
    i.has_tech_node = true;
    i.tech_epoch = 9;
    i.has_animal = true;
    i.animal_id = 17;
    i.has_calamity_powers = true;
    let f = fill(&r, &i);
    assert_eq!((f.mEpoch, f.mAnimalID, f.mHasCalamityPowers), (9, 17, true));
    // no influence and an attack: 1
    assert_eq!(f.mInfluence, 1);
    assert!(f.mIsTree);
    r.influence = 5;
    assert_eq!(fill(&r, &i).mInfluence, 5);
}

struct OneScript;
impl crate::script::ScriptSource for OneScript {
    fn read(&mut self, name: &[u8]) -> Option<Vec<u8>> {
        (name == b"ucf_test_patrol").then(|| b"GetNextPatrolPoint\n{\n}\n".to_vec())
    }
}

#[test]
fn behaviour_and_patrol_from_the_first_eight_commands() {
    let mut r = row();
    r.command_8 = COMMAND_BEHAVIOR as i32;
    r.command_7 = COMMAND_PATROL as i32;
    let f = fill(&r, &input());
    // the ninth command is not looked at; patrol needs the script's action
    assert!(!f.mCanChangeBehavior);
    assert!(!f.mCanPatrol);
    assert_eq!(f.mCommands[8], COMMAND_BEHAVIOR);
    let script = crate::script::get_script(b"ucf_test_patrol", &mut OneScript);
    assert_ne!(crate::script::find_action(script, ACTION_GET_NEXT_PATROL_POINT), 0);
    let f = fill(&r, &EeUnitsUnitClassFillIn { ai_script: script, ..input() });
    assert!(f.mCanPatrol);
    r.command_0 = COMMAND_BEHAVIOR as i32;
    assert!(fill(&r, &input()).mCanChangeBehavior);
}

#[test]
fn the_ffi_writes_the_whole_record() {
    let mut r = row();
    r.hit_points = 250;
    let mut out = std::mem::MaybeUninit::<EeUnitsUnitClassFields>::uninit();
    // SAFETY: a writable record, a readable row and input
    unsafe { ee_units_unit_class_fill(out.as_mut_ptr(), std::ptr::addr_of!(r).cast(), &input()) };
    // SAFETY: written whole by the call
    let out = unsafe { out.assume_init() };
    assert_eq!(out, fill(&r, &input()));
    assert_eq!(out.mHitPoints, 250);
}
