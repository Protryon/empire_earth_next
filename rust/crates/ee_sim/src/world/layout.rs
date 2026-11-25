//! Where the C++ compiler put the world's records and pointer members ([`EeWorldLayout`]): registered once at
//! start-up by `EEWorldLayout.cpp` (`EEWorldLayoutRegister`, called by the world's constructor), from `offsetof` and
//! the base casts of the build's own classes, so nothing here is hard-coded per ABI. The views (view.rs) read the
//! objects through it.
//!
//! It replaced the unit store's slots (`EeUnitsStoreSlots`, rounds 27 and 28): the store reads the units' pointer
//! members and the stats record through it too.

use std::sync::OnceLock;

/// The byte offsets of the records and members the views read, from the start of their object (an `RTSUnit*`,
/// `EEUnitClass*`, `EEUnitClassUpgrade*`, `RTSPlayer*` (= `EEPlayer*`), `RTSWorld*` (= `EEWorld*`), `RTSMap*`
/// (= `EEMap*`), `RTSTile*` (= `EETile*`), `RTSTUnitList*`, `EEUnitGoal*` or `EEUnitAction*` address).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeWorldLayout {
    /// `sizeof(EeWorldLayout)` as the C++ compiled it (a stale header is refused)
    pub size: u64,
    // ---- the Rust records' bases in their classes
    /// `EeUnitsRtsUnitFields` in `RTSUnit`
    pub rts_unit_fields: u64,
    /// `EeUnitsUnitFields` in `EEUnit`
    pub unit_fields: u64,
    /// `EeUnitsComplexUnitFields` in `EEComplexUnit`
    pub complex_unit_fields: u64,
    /// `EeUnitsUnitClassFields` in `EEUnitClass`
    pub unit_class_fields: u64,
    /// `EeUnitsUnitClassUpgradeFields` in `EEUnitClassUpgrade` (the stats)
    pub stats_fields: u64,
    /// `EeUnitsRtsPlayerFields`, `EeUnitsPlayerSaved` and `EeUnitsPlayerFields` in `EEPlayer`
    pub rts_player_fields: u64,
    pub player_saved: u64,
    pub player_fields: u64,
    /// `EeUnitsRtsMapFields` and `EeUnitsMapFields` in `EEMap`
    pub rts_map_fields: u64,
    pub map_fields: u64,
    // ---- RTSUnit
    /// `mObjectKind` (a byte: ee_sim::units::kinds' `EE_OBJECT_*`)
    pub unit_object_kind: u64,
    /// `mTileNode` (the world's unit chain), `mNext` (the owner's list), `mTile`, `mPlayer`
    pub unit_tile_node: u64,
    pub unit_next: u64,
    pub unit_tile: u64,
    pub unit_player: u64,
    // ---- EEUnit
    /// `mUnitClass`, `mComplexUnit`, `mStats`
    pub unit_class: u64,
    pub unit_complex_unit: u64,
    pub unit_stats: u64,
    // ---- EEComplexUnit
    /// `mPhysicalModel`'s location: a `GETransformation` (row-major 4 x 4 floats; x, y, z the last column's)
    pub complex_location: u64,
    /// `mActiveGroup`, `mContainer`, `mCommand`, `mGoalState`, `mActiveGoal`, `mHerd`
    pub complex_active_group: u64,
    pub complex_container: u64,
    pub complex_command: u64,
    pub complex_goal_state: u64,
    /// `mGoalQueues` (a vector of `EEUnitVectorItem`s, each its Rust queue's pointer)
    pub complex_goal_queues: u64,
    pub complex_active_goal: u64,
    pub complex_herd: u64,
    /// `mCargo`: a `std::vector<EEComplexUnit*>`, whose first two words are its begin and end (the C++ checks it)
    pub complex_cargo: u64,
    /// `mPreviousPart`, `mNextPart` (a linked group's parts)
    pub complex_previous_part: u64,
    pub complex_next_part: u64,
    /// `mBuildQueue`: a `std::vector<EEProductionEntry>` (begin and end its first two words); an entry's size and its
    /// `mTechnology` (a `long`)
    pub complex_build_queue: u64,
    pub build_entry_stride: u64,
    pub build_entry_technology: u64,
    // ---- behind the members
    /// `EEUnitGoal::mKind` (a byte), `EEUnitGoal::mID` (an `unsigned long`), `EEUnitAction::mType` (a `uint32_t`)
    pub goal_kind: u64,
    pub goal_id: u64,
    pub action_type: u64,
    // ---- RTSPlayer / EEPlayer
    /// `mColor` (a `GEColor`: red, green, blue floats), `mFirstUnit`, `EEPlayer::mSelectedGroup`
    pub player_color: u64,
    pub player_first_unit: u64,
    pub player_selected_group: u64,
    /// `EEPlayer::mComputerPlayer` (an `EEComputerPlayer*`, 0 for a human)
    pub player_computer_player: u64,
    /// lane P: the `std::vector<RTSUnit*>` `mPendingUnits` (the allies, sight sharers, towns and herds are the
    /// player's Rust lists: `player_lists`)
    pub player_pending_units: u64,
    // ---- RTSWorld / EEWorld
    /// `mPlayers` (17 pointers), `mMap`, `mNumPlayers` (an `unsigned long`), `EEWorld::mCurrentPlayer` (a `long`)
    pub world_players: u64,
    pub world_map: u64,
    pub world_num_players: u64,
    pub world_current_player: u64,
    /// `RTSWorld::mGameSpeed` (a float), `mPopulationLimit` (a `long`); `EEWorld::mNumberOfWondersToWin` (an
    /// `unsigned long`), `mGameOptions` (its address: an `EEGameOptions`), `mGhostUnits` (a vector of pointers)
    pub world_game_speed: u64,
    pub world_population_limit: u64,
    pub world_wonders_to_win: u64,
    pub world_game_options: u64,
    pub world_ghost_units: u64,
    /// `EEWorld`'s flags (bools): `mVictoryEnabled`, `mGameScreenActive`, `mLoadingSavedGame`, `mCheatsEnabled`,
    /// `mInCinematic`, `mUserPaused`, `mUsingCarbon`
    pub world_victory_enabled: u64,
    pub world_game_screen_active: u64,
    pub world_loading_saved_game: u64,
    pub world_cheats_enabled: u64,
    pub world_in_cinematic: u64,
    pub world_user_paused: u64,
    pub world_using_carbon: u64,
    /// `EEWorld::mTeamsLocked` (a bool)
    pub world_teams_locked: u64,
    /// `&gWorld` (an address, not an offset: the one world's handle) and `&gUnitClassTable` (the address of the
    /// `std::vector<EEUnitClass*>`; its begin and end are its first two words)
    pub world_address: u64,
    pub unit_class_table: u64,
    // ---- RTSMap
    /// `mTiles` (the `EETile*` array, row-major, `mSize` x `mSize`)
    pub map_tiles: u64,
    /// lane P: `EEMap::mTileArray` (the `EETile` array), `mVisibilityWords` (the tiles' visibility words)
    pub map_tile_array: u64,
    pub map_visibility_words: u64,
    /// the unit utilities lane: `EEMap::mUnitRegions` (the `EeCombatUnitRegions*`)
    pub map_unit_regions: u64,
    // ---- RTSTile / EETile
    /// `sizeof(EETile)` (`EEMap::mTileArray`'s element)
    pub tile_stride: u64,
    /// `mTerrainType` (a `long`), `mUnits` (the first node), `mVisibility` (`uint32_t*`), `mHeight`, `mAIValue` (a
    /// `signed char`)
    pub tile_terrain_type: u64,
    pub tile_units: u64,
    pub tile_visibility: u64,
    pub tile_height: u64,
    pub tile_ai_value: u64,
    // ---- RTSTUnitList (a tile's unit node)
    /// `mUnit` (an `EEUnit*`), `mNext`
    pub unit_node_unit: u64,
    pub unit_node_next: u64,
    // ---- EEPlayer (the renderer's reads, Lane R)
    /// `mViewedUnit`, `mSelectedViewedUnit` (`RTSUnit*`)
    pub player_viewed_unit: u64,
    pub player_selected_viewed_unit: u64,
    // ---- the tech tree and the computer player (lane A round 2: the Lua AI's bindings; tech_view.rs)
    /// `EEPlayer::mTechnologies` (an `EETechTree*`)
    pub player_technologies: u64,
    /// `EeTechTreeFields` in `EETechTree`, `EeTechNodeFields` in `EETechTreeNode`
    pub tech_tree_fields: u64,
    pub node_fields: u64,
    /// `EETechTreeNode::mData` (an `RTSTechTreeNodeData*`: the record `EeTechNodeDataFields` itself)
    pub node_data: u64,
    /// `&EETechHost` (an address: the tree walks' host, whose lookups the tree view makes)
    pub tech_host: u64,
    /// `EeCpFields` in `EEComputerPlayer`
    pub cp_fields: u64,
    // ---- the HUD's reads (the UI thread lane, round 1)
    /// `EEUnitClassUpgrade::mUnitClass` (an `EEUnitClass*`: the stats' own class) and `mButton` (an
    /// `EEButtonObject*`)
    pub stats_unit_class: u64,
    pub stats_button: u64,
    /// `EeUnitsUnitClassUpgradeLinks` in `EEUnitClassUpgrade` (units round 11: the two above are derived from it)
    pub stats_links: u64,
    // ---- the physics (the units' round 1: the game time and tick of the views)
    /// `&gPhysics` (the pointer variable's address) and `EeUnitsPhysicsSaved`'s base in `EEPhysics`
    pub physics_pointer: u64,
    pub physics_saved: u64,
    // ---- the units' saves (the units' state lane, round 1: unit_save_views.rs)
    /// `EEComplexUnit`'s pointer members `mLauncherClass` (an `EEUnitClass*`), `mPatrolGroup` (an
    /// `EEPPatrolGroup*`), `mHomeBase`, `mGatheredResource`, `mLastAttacker`, `mOwner`, `mMine`, `mHolder`,
    /// `mBehavior` (an `EEUnitBehavior*`), `mLastResource` (units: `RTSUnit*` addresses)
    pub complex_launcher_class: u64,
    pub complex_patrol_group: u64,
    pub complex_home_base: u64,
    pub complex_gathered_resource: u64,
    pub complex_last_attacker: u64,
    pub complex_owner: u64,
    pub complex_mine: u64,
    pub complex_holder: u64,
    pub complex_behavior: u64,
    pub complex_last_resource: u64,
    /// the vectors `mHeldUnits` (`EEComplexUnit*`), `mUnitVariables` (`long`), `mBoundedPathTiles`
    /// (`U2DPoint<long>`: two `long`s); begin and end their first two words
    pub complex_held_units: u64,
    pub complex_unit_variables: u64,
    pub complex_bounded_path_tiles: u64,
    /// `mWaypoints` (an `EEWaypointList`: its first word the Rust list's handle, ee_sim::units waypoints.rs)
    pub complex_waypoints: u64,
    /// `mContacts` (a `UList<EEComplexUnit*>`): its `mHead` and `mCount` (an `unsigned long`); a node's `mData` and
    /// `mNext`
    pub complex_contacts_head: u64,
    pub complex_contacts_count: u64,
    pub contact_node_data: u64,
    pub contact_node_next: u64,
    /// `mCollisionTile` (two `long`s), `mInitialContactLocation` (three floats)
    pub complex_collision_tile: u64,
    pub complex_contact_location: u64,
    /// `mPhysicalModel`'s scale (a float) and frame (an `unsigned long`)
    pub complex_scale: u64,
    pub complex_frame: u64,
    /// the effect handles `mConversionEffect`, `mHealEffect` (`uint32_t` values)
    pub complex_conversion_effect: u64,
    pub complex_heal_effect: u64,
    /// `mAngularVelocity`, `mOrientation` (`GEOrientation`s: yaw, pitch, roll)
    pub complex_angular_velocity: u64,
    pub complex_orientation: u64,
    /// `EEProductionEntry::mFlag` (a `long`)
    pub build_entry_flag: u64,
    /// `EEHerd::mID` (an `unsigned short`)
    pub herd_id: u64,
    /// `EEUnitClass::mProjectileClass` (an `EEUnitClass*`) and `mAttachedEffect` (the first of the class's 29
    /// `EEGraphicEffect*` members, `GetEffects()`)
    pub class_projectile_class: u64,
    pub class_effects: u64,
    /// `EeUnitsUnitClassLinks` in `EEUnitClass` (units round 11: the two above are derived from it)
    pub class_links: u64,
    /// `EeUnitsActiveGroupFields` in `EEActiveGroup` (its second base) and `EEActiveGroup::mGoal` (an
    /// `EEUnitGoal*`)
    pub active_group_fields: u64,
    pub active_group_goal: u64,
    // ---- the computer player's placement (the game's leaves, round 2: cp place_capture.rs)
    /// `EEComputerPlayer::mPlacement` (an `EECPPlacement*`) from the `EEPlayer*`; `EECPPlacement::mTownCenterTile`
    /// (a `U2DPoint<long>`) and `mRust` (an `EeCpPlacement*`)
    pub cp_placement: u64,
    pub placement_town: u64,
    pub placement_rust: u64,
    // ---- the groups lane, round 2: the player's group lists (group_obj.rs `EeUnitsPlayerGroups`)
    /// `EEPlayer::mGroupLists` (an `EeUnitsPlayerGroups*`)
    pub player_group_lists: u64,
    /// `EEPlayer::mLists` (an `EeUnitsPlayerLists*`: player_body.rs)
    pub player_lists: u64,
}

static LAYOUT: OnceLock<EeWorldLayout> = OnceLock::new();

#[cfg(any(test, feature = "test_layout"))]
thread_local! {
    /// a test's layout for its thread (tests run in parallel and the registered layout is the process's)
    static TEST_LAYOUT: std::cell::Cell<Option<&'static EeWorldLayout>> = const { std::cell::Cell::new(None) };
}

/// Registers the build's layout: the first call wins (the layout is the build's); false for a record of another
/// size or a second, different layout.
pub fn set(mut layout: EeWorldLayout) -> bool {
    derive_record_members(&mut layout);
    if layout.size != std::mem::size_of::<EeWorldLayout>() as u64 {
        return false;
    }
    *LAYOUT.get_or_init(|| layout) == layout
}

/// The offsets of the members the unit records hold (ee_sim world unit_fields.rs): the record's base plus the
/// member's place in it (the C++ registers only the bases; an offset already set is kept).
pub fn derive_record_members(l: &mut EeWorldLayout) {
    use crate::world::unit_fields::{EeUnitsComplexUnitFields, EeUnitsUnitFields};
    use std::mem::offset_of;
    // a layout that names a member itself (a test's made-up objects) keeps its own offset
    fn fill(at: &mut u64, base: u64, offset: usize) {
        if *at == 0 {
            *at = base + offset as u64;
        }
    }
    fill(&mut l.unit_class, l.unit_fields, offset_of!(EeUnitsUnitFields, mUnitClass));
    fill(&mut l.unit_complex_unit, l.unit_fields, offset_of!(EeUnitsUnitFields, mComplexUnit));
    fill(&mut l.unit_stats, l.unit_fields, offset_of!(EeUnitsUnitFields, mStats));
    fill(&mut l.complex_launcher_class, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mLauncherClass));
    fill(&mut l.complex_active_group, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mActiveGroup));
    fill(&mut l.complex_previous_part, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mPreviousPart));
    fill(&mut l.complex_next_part, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mNextPart));
    fill(&mut l.complex_home_base, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mHomeBase));
    fill(&mut l.complex_container, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mContainer));
    fill(&mut l.complex_last_attacker, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mLastAttacker));
    fill(&mut l.complex_owner, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mOwner));
    fill(&mut l.complex_holder, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mHolder));
    fill(&mut l.complex_command, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mCommand));
    fill(&mut l.complex_active_goal, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mActiveGoal));
    fill(&mut l.complex_goal_state, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mGoalState));
    fill(&mut l.complex_last_resource, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mLastResource));
    l.complex_gathered_resource =
        l.complex_unit_fields + offset_of!(EeUnitsComplexUnitFields, mGatheredResource) as u64;
    fill(&mut l.complex_mine, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mMine));
    fill(&mut l.complex_herd, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mHerd));
    fill(&mut l.complex_behavior, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mBehavior));
    // units round 12: the collision tile and two effect handles are record fields
    fill(&mut l.complex_collision_tile, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mCollisionTile));
    fill(
        &mut l.complex_conversion_effect,
        l.complex_unit_fields,
        offset_of!(EeUnitsComplexUnitFields, mConversionEffect),
    );
    fill(&mut l.complex_heal_effect, l.complex_unit_fields, offset_of!(EeUnitsComplexUnitFields, mHealEffect));
    {
        use crate::world::stats_fields::EeUnitsUnitClassUpgradeLinks as S;
        use crate::world::unit_class_fields::EeUnitsUnitClassLinks as C;
        fill(&mut l.class_projectile_class, l.class_links, offset_of!(C, mProjectileClass));
        fill(&mut l.class_effects, l.class_links, offset_of!(C, mAttachedEffect));
        fill(&mut l.stats_unit_class, l.stats_links, offset_of!(S, mUnitClass));
        fill(&mut l.stats_button, l.stats_links, offset_of!(S, mButton));
    }
    // a goal is its Rust object (goal_obj.rs: the C++ EEUnitGoal is EeUnitsGoalBase, asserted in EEUGRust.cpp); a build
    // queue entry the record EeUnitsProductionEntry (units round 11)
    use crate::units::goal_obj::EeUnitsGoalBase;
    use crate::world::unit_fields::EeUnitsProductionEntry;
    fill(&mut l.goal_kind, 0, offset_of!(EeUnitsGoalBase, kind));
    fill(&mut l.goal_id, 0, offset_of!(EeUnitsGoalBase, id));
    fill(&mut l.build_entry_stride, 0, std::mem::size_of::<EeUnitsProductionEntry>());
    fill(&mut l.build_entry_technology, 0, offset_of!(EeUnitsProductionEntry, mTechnology));
    fill(&mut l.build_entry_flag, 0, offset_of!(EeUnitsProductionEntry, mFlag));
}

/// The registered layout (None before the C++ registered it, as in the unit tests).
pub fn get() -> Option<&'static EeWorldLayout> {
    #[cfg(any(test, feature = "test_layout"))]
    if let Some(l) = TEST_LAYOUT.with(|t| t.get()) {
        return Some(l);
    }
    LAYOUT.get()
}

/// The registered layout. Panics before it is registered: a view of a C++ object needs it.
pub fn layout() -> &'static EeWorldLayout {
    get().expect("ee_sim::world: no layout registered (EEWorldLayoutRegister)")
}

/// TEST ONLY: `layout` for this thread until the guard drops.
#[cfg(any(test, feature = "test_layout"))]
pub fn with_test_layout(layout: EeWorldLayout) -> TestLayout {
    let l: &'static EeWorldLayout = Box::leak(Box::new(layout));
    TEST_LAYOUT.with(|t| t.set(Some(l)));
    TestLayout(())
}

#[cfg(any(test, feature = "test_layout"))]
pub struct TestLayout(());

#[cfg(any(test, feature = "test_layout"))]
impl Drop for TestLayout {
    fn drop(&mut self) {
        TEST_LAYOUT.with(|t| t.set(None));
    }
}
