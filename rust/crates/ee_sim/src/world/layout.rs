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
    // ---- RTSMap
    /// `mTiles` (the `EETile*` array, row-major, `mSize` x `mSize`)
    pub map_tiles: u64,
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
}

static LAYOUT: OnceLock<EeWorldLayout> = OnceLock::new();

#[cfg(test)]
thread_local! {
    /// a test's layout for its thread (tests run in parallel and the registered layout is the process's)
    static TEST_LAYOUT: std::cell::Cell<Option<&'static EeWorldLayout>> = const { std::cell::Cell::new(None) };
}

/// Registers the build's layout: the first call wins (the layout is the build's); false for a record of another
/// size or a second, different layout.
pub fn set(layout: EeWorldLayout) -> bool {
    if layout.size != std::mem::size_of::<EeWorldLayout>() as u64 {
        return false;
    }
    *LAYOUT.get_or_init(|| layout) == layout
}

/// The registered layout (None before the C++ registered it, as in the unit tests).
pub fn get() -> Option<&'static EeWorldLayout> {
    #[cfg(test)]
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
#[cfg(test)]
pub fn with_test_layout(layout: EeWorldLayout) -> TestLayout {
    let l: &'static EeWorldLayout = Box::leak(Box::new(layout));
    TEST_LAYOUT.with(|t| t.set(Some(l)));
    TestLayout(())
}

#[cfg(test)]
pub struct TestLayout(());

#[cfg(test)]
impl Drop for TestLayout {
    fn drop(&mut self) {
        TEST_LAYOUT.with(|t| t.set(None));
    }
}
