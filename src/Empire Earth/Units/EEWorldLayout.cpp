// New (port): the world's layout for Rust's views (ee/EEWorldLayout.h; ee_sim world layout.rs). The record bases and
// member offsets as this build's compiler placed them (offsetof and the base casts: nothing hard-coded per ABI),
// registered once by the world's constructor. It replaced the unit store's slots (EEUnitStore.cpp, rounds 27-28).
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitAction.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEWorldLayout.h"
#include "rust/sim/world.h"

// the classes are not standard-layout; offsetof of a non-virtual base's member is the member's place in every object
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

// what the views read as plain values (view.rs): a byte, an array of 17 pointers, three floats, a 4 x 4 matrix
static_assert(sizeof(bool) == 1, "the views read a bool as its byte");
static_assert(sizeof(RTSUnitObjectKind) == 1, "the object kind is a byte");
static_assert(sizeof(((RTSTile*)0)->mAIValue) == 1, "the AI value is a signed byte");
static_assert(sizeof(((EEUnitGoal*)0)->mKind) == 1, "a goal's kind is a byte");
static_assert(sizeof(((EEUnitAction*)0)->mType) == 4, "an action's type is 32 bits");
static_assert(sizeof(((RTSWorld*)0)->mPlayers) == 17 * sizeof(void*), "the world's 17 player slots");
static_assert(sizeof(GEColor) == 3 * sizeof(float), "a colour is red, green, blue");
static_assert(sizeof(GETransformation) >= 16 * sizeof(float) && offsetof(GETransformation, mMatrix) == 0,
              "a transformation starts with its 4 x 4 matrix");
static_assert(sizeof(std::vector<EEComplexUnit*>) == 3 * sizeof(void*), "a vector is three words");

namespace {

// where Base sits in a Derived object (a cast of a made-up address: nothing is read)
template <class Derived, class Base> uint64_t BaseOffset()
{
    Derived* theObject = reinterpret_cast<Derived*>(static_cast<uintptr_t>(0x10000));
    return reinterpret_cast<uintptr_t>(static_cast<Base*>(theObject)) - 0x10000;
}

void Fail(const char* inWhat)
{
    fprintf(stderr, "EE-WORLD-LAYOUT: %s\n", inWhat);
    abort();
}

}  // namespace

void EEWorldLayoutRegister()
{
    // a handle is the first base's address: RTSUnit, RTSPlayer, RTSMap, RTSTile, RTSTUnitList at offset 0
    if (BaseOffset<EEUnit, RTSUnit>() != 0 || BaseOffset<EEComplexUnit, RTSUnit>() != 0
        || BaseOffset<EEPlayer, RTSPlayer>() != 0 || BaseOffset<EEMap, RTSMap>() != 0
        || BaseOffset<EETile, RTSTile>() != 0 || BaseOffset<EETUnitList, RTSTUnitList>() != 0)
        Fail("a first base is not at offset 0");
    // the library's vector: its begin and end are its first two words (MSVC's _Myfirst, _Mylast; libc++'s __begin_,
    // __end_; libstdc++'s _M_start, _M_finish)
    std::vector<EEComplexUnit*> theProbe(2);
    void* const* theWords = reinterpret_cast<void* const*>(&theProbe);
    if (theWords[0] != static_cast<void*>(theProbe.data()) || theWords[1] != static_cast<void*>(theProbe.data() + 2))
        Fail("a std::vector's first two words are not its begin and end");
    const GEPhysicalModel* theModel = reinterpret_cast<const GEPhysicalModel*>(static_cast<uintptr_t>(0x10000));
    uint64_t theLocation = reinterpret_cast<uintptr_t>(&theModel->GetLocation()) - 0x10000;

    EeWorldLayout theLayout = {};
    theLayout.size = sizeof(EeWorldLayout);
    theLayout.rts_unit_fields = BaseOffset<RTSUnit, EeUnitsRtsUnitFields>();
    theLayout.unit_fields = BaseOffset<EEUnit, EeUnitsUnitFields>();
    theLayout.complex_unit_fields = BaseOffset<EEComplexUnit, EeUnitsComplexUnitFields>();
    theLayout.unit_class_fields = BaseOffset<EEUnitClass, EeUnitsUnitClassFields>();
    theLayout.stats_fields = BaseOffset<EEUnitClassUpgrade, EeUnitsUnitClassUpgradeFields>();
    theLayout.rts_player_fields = BaseOffset<EEPlayer, EeUnitsRtsPlayerFields>();
    theLayout.player_saved = BaseOffset<EEPlayer, EeUnitsPlayerSaved>();
    theLayout.player_fields = BaseOffset<EEPlayer, EeUnitsPlayerFields>();
    theLayout.rts_map_fields = BaseOffset<EEMap, EeUnitsRtsMapFields>();
    theLayout.map_fields = BaseOffset<EEMap, EeUnitsMapFields>();
    theLayout.unit_object_kind = offsetof(RTSUnit, mObjectKind);
    theLayout.unit_tile_node = offsetof(RTSUnit, mTileNode);
    theLayout.unit_next = offsetof(RTSUnit, mNext);
    theLayout.unit_tile = offsetof(RTSUnit, mTile);
    theLayout.unit_player = offsetof(RTSUnit, mPlayer);
    theLayout.unit_class = offsetof(EEUnit, mUnitClass);
    theLayout.unit_complex_unit = offsetof(EEUnit, mComplexUnit);
    theLayout.unit_stats = offsetof(EEUnit, mStats);
    theLayout.complex_location = offsetof(EEComplexUnit, mPhysicalModel) + theLocation;
    theLayout.complex_active_group = offsetof(EEComplexUnit, mActiveGroup);
    theLayout.complex_container = offsetof(EEComplexUnit, mContainer);
    theLayout.complex_command = offsetof(EEComplexUnit, mCommand);
    theLayout.complex_goal_state = offsetof(EEComplexUnit, mGoalState);
    theLayout.complex_active_goal = offsetof(EEComplexUnit, mActiveGoal);
    theLayout.complex_herd = offsetof(EEComplexUnit, mHerd);
    theLayout.complex_cargo = offsetof(EEComplexUnit, mCargo);
    theLayout.complex_previous_part = offsetof(EEComplexUnit, mPreviousPart);
    theLayout.complex_next_part = offsetof(EEComplexUnit, mNextPart);
    theLayout.complex_build_queue = offsetof(EEComplexUnit, mBuildQueue);
    theLayout.build_entry_stride = sizeof(EEProductionEntry);
    theLayout.build_entry_technology = offsetof(EEProductionEntry, mTechnology);
    theLayout.goal_kind = offsetof(EEUnitGoal, mKind);
    theLayout.goal_id = offsetof(EEUnitGoal, mID);
    theLayout.action_type = offsetof(EEUnitAction, mType);
    theLayout.player_color = offsetof(EEPlayer, mColor);
    theLayout.player_first_unit = offsetof(EEPlayer, mFirstUnit);
    theLayout.player_selected_group = offsetof(EEPlayer, mSelectedGroup);
    theLayout.player_allies = offsetof(EEPlayer, mAllies);   // lane P
    theLayout.player_sight_sharers = offsetof(EEPlayer, mSightSharers);
    theLayout.player_pending_units = offsetof(EEPlayer, mPendingUnits);
    theLayout.player_towns = offsetof(EEPlayer, mTowns);
    theLayout.player_herds = offsetof(EEPlayer, mHerds);
    theLayout.player_viewed_unit = offsetof(EEPlayer, mViewedUnit);
    theLayout.world_players = offsetof(EEWorld, mPlayers);
    theLayout.world_map = offsetof(EEWorld, mMap);
    theLayout.world_num_players = offsetof(EEWorld, mNumPlayers);
    theLayout.world_current_player = offsetof(EEWorld, mCurrentPlayer);
    theLayout.world_game_speed = offsetof(EEWorld, mGameSpeed);
    theLayout.world_population_limit = offsetof(EEWorld, mPopulationLimit);
    theLayout.world_wonders_to_win = offsetof(EEWorld, mNumberOfWondersToWin);
    theLayout.world_game_options = offsetof(EEWorld, mGameOptions);
    theLayout.world_ghost_units = offsetof(EEWorld, mGhostUnits);
    theLayout.world_victory_enabled = offsetof(EEWorld, mVictoryEnabled);
    theLayout.world_game_screen_active = offsetof(EEWorld, mGameScreenActive);
    theLayout.world_loading_saved_game = offsetof(EEWorld, mLoadingSavedGame);
    theLayout.world_cheats_enabled = offsetof(EEWorld, mCheatsEnabled);
    theLayout.world_in_cinematic = offsetof(EEWorld, mInCinematic);
    theLayout.world_user_paused = offsetof(EEWorld, mUserPaused);
    theLayout.world_using_carbon = offsetof(EEWorld, mUsingCarbon);
    theLayout.map_tiles = offsetof(EEMap, mTiles);
    theLayout.map_tile_array = offsetof(EEMap, mTileArray);   // lane P
    theLayout.map_visibility_words = offsetof(EEMap, mVisibilityWords);
    theLayout.tile_stride = sizeof(EETile);
    theLayout.tile_terrain_type = offsetof(EETile, mTerrainType);
    theLayout.tile_units = offsetof(EETile, mUnits);
    theLayout.tile_visibility = offsetof(EETile, mVisibility);
    theLayout.tile_height = offsetof(EETile, mHeight);
    theLayout.tile_ai_value = offsetof(EETile, mAIValue);
    theLayout.unit_node_unit = offsetof(RTSTUnitList, mUnit);
    theLayout.unit_node_next = offsetof(RTSTUnitList, mNext);
    if (!ee_world_set_layout(&theLayout))
        Fail("Rust refused the layout (a stale rust/sim/world.h, or a second, different layout)");
}
