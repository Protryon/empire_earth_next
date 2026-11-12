// C:\EEX\Empire Earth\Random Map\Random Map Generators\Default\Default.cpp: the plug-in's entry points, the "rmap" PIM.
// The generator itself is Rust (rust/crates/ee_rmap, RUST_PLAN R3.4): each step copies EERandomMap into flat arrays,
// runs in Rust and copies the result back (ee_rmap::ffi, EeRmapState); the .rmv script reader stays in C++ (Unit10.cpp).
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DGeometry.h"
#include "ee/EERandomMap.h"
#include "rust/ee_rmap.h"
#include "Unit10.h"
#include "ee/EERandomMapIDs.h"

static UString gName("Default");
static unsigned long gType = 'rmap';

// BuildPlayerOrder's order (the C++ globals of the former Unit12): the Rust generator gets and returns it, and
// SetRandomMapValues resets it for each script read
static std::vector<long> gPlayerOrder;
static unsigned long gPlayerOrderIndex;

void ResetPlayerOrder()
{
    gPlayerOrder.clear();
    gPlayerOrderIndex = 0;
}

// A flat view of an EERandomMap for one Rust step: the buffers the EeRmapState input points into.
struct RMFlatState {
    std::vector<unsigned char> mFlat, mClaimed, mValid;
    std::vector<float> mHeights;
    std::vector<uint32_t> mTiles;
    std::vector<EeRmapPlayer> mPlayers;
    std::vector<EeRmapPoint> mPlayerPoints;
    std::vector<EeRmapTeam> mTeams;
    std::vector<int32_t> mTeamPlayers;
    std::vector<EeRmapPoint> mTeamPositions;
    std::vector<EeRmapObject> mObjects;
    std::vector<EeRmapPoint> mWaterPoints;
    std::vector<EeRmapFeature> mFeatures;
    std::vector<uint32_t> mCircleCounts;
    std::vector<EeRmapCircle> mCircles;
    std::vector<int32_t> mTreeTypes;
    std::vector<EeRmapResourceKind> mResourceKinds;
    std::vector<int32_t> mResourceKindIds;
    std::vector<EeRmapResourceSlot> mResourceSlots;
    std::vector<EeRmapSpacingRule> mSpacingRules;
    std::vector<int32_t> mPlayerOrder;
    EeRmapState mState;
};

static EeRmapPoint ToPoint(const GE2DPoint& inPoint)
{
    EeRmapPoint thePoint;
    thePoint.x = inPoint.mX;
    thePoint.y = inPoint.mY;
    return thePoint;
}

static GE2DPoint FromPoint(const EeRmapPoint& inPoint)
{
    GE2DPoint thePoint;
    thePoint.mX = inPoint.x;
    thePoint.mY = inPoint.y;
    return thePoint;
}

template <class T, class U>
static void FlattenGrid(const std::vector<std::vector<T> >& inGrid, std::vector<U>& outFlat)
{
    size_t theN = inGrid.size();
    outFlat.resize(theN * theN);
    for (size_t x = 0; x < theN; x++)
        for (size_t y = 0; y < theN; y++)
            outFlat[x * theN + y] = (U)inGrid[x][y];
}

template <class T, class U>
static void UnflattenGrid(const U* inFlat, std::vector<std::vector<T> >& ioGrid)
{
    size_t theN = ioGrid.size();
    for (size_t x = 0; x < theN; x++)
        for (size_t y = 0; y < theN; y++)
            ioGrid[x][y] = (T)inFlat[x * theN + y];
}

static void FillSettings(const EERandomMap& m, EeRmapSettings& s)
{
    memset(&s, 0, sizeof s);
    s.random_seed = m.mRandom.mSeed;
    s.random_integers_seed = m.mRandomIntegers.mSeed;
    s.player_order_index = (uint32_t)gPlayerOrderIndex;
    s.seed = (uint32_t)m.mSeed;
    s.maximum_x = (uint32_t)m.mMaximumX;
    s.maximum_y = (uint32_t)m.mMaximumY;
    s.grid_size = (uint32_t)m.mGridSize;
    s.tile_count = (uint32_t)m.mTileCount;
    s.map_characteristics = (uint32_t)m.mMapCharacteristics;
    s.river_width = (uint32_t)m.mRiverWidth;
    s.num_team_players = (uint32_t)m.mNumTeamPlayers;
    s.num_moons = (uint32_t)m.mNumMoons;
    s.num_inner_neutrals = (uint32_t)m.mNumInnerNeutrals;
    s.num_outer_neutrals = (uint32_t)m.mNumOuterNeutrals;
    s.moon_size = (uint32_t)m.mMoonSize;
    s.moon_resources = (uint32_t)m.mMoonResources;
    s.num_terrain_bands = (uint32_t)m.mNumTerrainBands;
    s.terrain_band_offset = (uint32_t)m.mTerrainBandOffset;
    s.max_loop_count = (uint32_t)m.mMaxLoopCount;
    s.max_tree_loop_count = (uint32_t)m.mMaxTreeLoopCount;
    s.max_clumps_per_forest = (uint32_t)m.mMaxClumpsPerForest;
    s.forests_per_player = (uint32_t)m.mForestsPerPlayer;
    s.inner_neutral_flat_clumps = (uint32_t)m.mInnerNeutralFlatClumps;
    s.inner_neutral_land_clumps = (uint32_t)m.mInnerNeutralLandClumps;
    s.outer_neutral_flat_clumps = (uint32_t)m.mOuterNeutralFlatClumps;
    s.outer_neutral_land_clumps = (uint32_t)m.mOuterNeutralLandClumps;
    s.player_flat_clumps = (uint32_t)m.mPlayerFlatClumps;
    s.player_land_clumps = (uint32_t)m.mPlayerLandClumps;
    s.resource_to_edge_distance = (uint32_t)m.mResourceToEdgeDistance;
    s.resource_to_water_distance = (uint32_t)m.mResourceToWaterDistance;
    s.max_iterations_clump_growth = (int32_t)m.mMaxIterationsClumpGrowth;
    s.water_border = (int32_t)m.mWaterBorder;
    s.max_int_elevation = (int32_t)m.mMaxIntElevation;
    s.min_int_elevation = (int32_t)m.mMinIntElevation;
    s.shallows_width = (int32_t)m.mShallowsWidth;
    s.max_shallow_connections_per_player = (int32_t)m.mMaxShallowConnectionsPerPlayer;
    s.outer_push_from_map_center = (int32_t)m.mOuterPushFromMapCenter;
    s.inner_push_from_map_center = (int32_t)m.mInnerPushFromMapCenter;
    s.minimum_start_position_to_map_edge = (int32_t)m.mMinimumStartPositionToMapEdge;
    for (int i = 0; i < 7; i++) {
        s.resource_per_player[i] = (int32_t)m.mResourcePerPlayer[i];
        s.resource_per_neutral_land[i] = (uint32_t)m.mResourcePerNeutralLand[i];
        s.kind_spacing[i][0] = (uint32_t)m.mKindSpacing[i][0];
        s.kind_spacing[i][1] = (uint32_t)m.mKindSpacing[i][1];
    }
    s.coastal_to_deeper_water_percentage = m.mCoastalToDeeperWaterPercentage;
    s.convergent_sample_size = m.mConvergentSampleSize;
    s.convergent_threshold = m.mConvergentThreshold;
    s.elevation_scale = m.mElevationScale;
    s.elevation_unit = m.mElevationUnit;
    s.flat_elevation = m.mFlatElevation;
    s.forest_chaos_level = m.mForestChaosLevel;
    s.forest_free_radius = m.mForestFreeRadius;
    s.initial_elevation = m.mInitialElevation;
    s.inner_neutral_flat_chaos = m.mInnerNeutralFlatChaos;
    s.inner_neutral_land_chaos = m.mInnerNeutralLandChaos;
    s.inner_neutral_percent_flat = m.mInnerNeutralPercentFlat;
    s.inner_neutral_percent_land = m.mInnerNeutralPercentLand;
    s.inner_neutral_tree_percentage = m.mInnerNeutralTreePercentage;
    s.land_elevation_threshold = m.mLandElevationThreshold;
    s.max_angle_between_team_members = m.mMaxAngleBetweenTeamMembers;
    s.max_resource_elevation = m.mMaxResourceElevation;
    s.max_tree_elevation = m.mMaxTreeElevation;
    s.outer_neutral_flat_chaos = m.mOuterNeutralFlatChaos;
    s.outer_neutral_land_chaos = m.mOuterNeutralLandChaos;
    s.outer_neutral_percent_flat = m.mOuterNeutralPercentFlat;
    s.outer_neutral_percent_land = m.mOuterNeutralPercentLand;
    s.outer_neutral_tree_percentage = m.mOuterNeutralTreePercentage;
    s.percent_land = m.mPercentLand;
    s.player_flat_chaos = m.mPlayerFlatChaos;
    s.player_land_chaos = m.mPlayerLandChaos;
    s.player_percent_flat = m.mPlayerPercentFlat;
    s.player_tree_percentage = m.mPlayerTreePercentage;
    s.shallows_elevation = m.mShallowsElevation;
    s.height_map_chaos = m.mHeightMapChaos;
    s.landfill_percentage = m.mLandfillPercentage;
    s.resource_seperation = m.mResourceSeperation;
    s.half_map_size = m.mHalfMapSize;
    s.flat_area.left = m.mFlatArea.GetLeft();
    s.flat_area.bottom = m.mFlatArea.GetBottom();
    s.flat_area.right = m.mFlatArea.GetRight();
    s.flat_area.top = m.mFlatArea.GetTop();
    const EERMRing* theRings[3] = {&m.mRing, &m.mNeutralRing, &m.mMoonRing};
    EeRmapRing* theOut[3] = {&s.ring, &s.neutral_ring, &s.moon_ring};
    for (int i = 0; i < 3; i++) {
        theOut[i]->center = ToPoint(theRings[i]->mCenter);
        theOut[i]->min_radius = theRings[i]->mMinRadius;
        theOut[i]->max_radius = theRings[i]->mMaxRadius;
    }
    s.team_growth = m.mTeamGrowth;
    s.generate_shallows = m.mGenerateShallows;
    s.is_space_map = m.mIsSpaceMap;
}

static void Flatten(const EERandomMap& m, RMFlatState& f)
{
    FillSettings(m, f.mState.settings);
    FlattenGrid(m.mFlatTiles, f.mFlat);
    FlattenGrid(m.mClaimedTiles, f.mClaimed);
    FlattenGrid(m.mValidTiles, f.mValid);
    FlattenGrid(m.mHeights, f.mHeights);
    FlattenGrid(m.mTiles, f.mTiles);
    f.mState.grid_n = (uint32_t)m.mTiles.size();
    f.mState.flat_tiles = f.mFlat.data();
    f.mState.claimed_tiles = f.mClaimed.data();
    f.mState.valid_tiles = f.mValid.data();
    f.mState.heights = f.mHeights.data();
    f.mState.tiles = f.mTiles.data();

    for (size_t i = 0; i < m.mPlayers.size(); i++) {
        const EERMPlayer& p = m.mPlayers[i];
        EeRmapPlayer r;
        memset(&r, 0, sizeof r);
        r.position = ToPoint(p.mPosition);
        r.radius = p.mRadius;
        r.index = (uint32_t)p.mIndex;
        r.team = (int32_t)p.mTeam;
        r.tile_count = (int32_t)p.mTileCount;
        r.is_neutral = p.mIsNeutral;
        r.is_outer = p.mIsOuter;
        r.is_moon = p.mIsMoon;
        r.is_placed = p.mIsPlaced;
        r.tiles_start = (uint32_t)f.mPlayerPoints.size();
        r.tiles_len = (uint32_t)p.mTiles.size();
        for (size_t j = 0; j < p.mTiles.size(); j++)
            f.mPlayerPoints.push_back(ToPoint(p.mTiles[j]));
        r.centers_start = (uint32_t)f.mPlayerPoints.size();
        r.centers_len = (uint32_t)p.mClumpCenters.size();
        for (size_t j = 0; j < p.mClumpCenters.size(); j++)
            f.mPlayerPoints.push_back(ToPoint(p.mClumpCenters[j]));
        f.mPlayers.push_back(r);
    }
    f.mState.players = f.mPlayers.data();
    f.mState.players_len = (uint32_t)f.mPlayers.size();
    f.mState.player_points = f.mPlayerPoints.data();
    f.mState.player_points_len = (uint32_t)f.mPlayerPoints.size();

    for (size_t i = 0; i < m.mTeams.size(); i++) {
        EeRmapTeam t;
        t.start = (uint32_t)f.mTeamPlayers.size();
        t.len = (uint32_t)m.mTeams[i].mPlayers.size();
        for (size_t j = 0; j < m.mTeams[i].mPlayers.size(); j++) {
            f.mTeamPlayers.push_back((int32_t)m.mTeams[i].mPlayers[j]);
            f.mTeamPositions.push_back(ToPoint(m.mTeams[i].mPositions[j]));
        }
        f.mTeams.push_back(t);
    }
    f.mState.teams = f.mTeams.data();
    f.mState.teams_len = (uint32_t)f.mTeams.size();
    f.mState.team_players = f.mTeamPlayers.data();
    f.mState.team_positions = f.mTeamPositions.data();
    f.mState.team_members_len = (uint32_t)f.mTeamPlayers.size();

    for (size_t i = 0; i < m.mObjects.size(); i++) {
        EeRmapObject o;
        o.location = ToPoint(m.mObjects[i].mLocation);
        o.kind = (int32_t)m.mObjects[i].mKind;
        o.object_type = (int32_t)m.mObjects[i].mType;
        f.mObjects.push_back(o);
    }
    f.mState.objects = f.mObjects.data();
    f.mState.objects_len = (uint32_t)f.mObjects.size();

    for (size_t i = 0; i < m.mWaterPoints.size(); i++)
        f.mWaterPoints.push_back(ToPoint(m.mWaterPoints[i]));
    f.mState.water_points = f.mWaterPoints.data();
    f.mState.water_points_len = (uint32_t)f.mWaterPoints.size();

    for (size_t i = 0; i < m.mFeatures.size(); i++) {
        const EERMFeature& e = m.mFeatures[i];
        EeRmapFeature r;
        memset(&r, 0, sizeof r);
        r.angle = e.mAngle;
        r.position.x = e.mPosition.mX;
        r.position.y = e.mPosition.mY;
        r.unit_type = (int32_t)e.mUnitType;
        r.index = (int32_t)e.mIndex;
        r.is_world_unit = e.mIsWorldUnit;
        r.in_ring = e.mInRing;
        r.is_herd = e.mIsHerd;
        f.mFeatures.push_back(r);
    }
    f.mState.features = f.mFeatures.data();
    f.mState.features_len = (uint32_t)f.mFeatures.size();

    for (size_t i = 0; i < m.mClumpCircles.size(); i++) {
        f.mCircleCounts.push_back((uint32_t)m.mClumpCircles[i].size());
        for (size_t j = 0; j < m.mClumpCircles[i].size(); j++) {
            EeRmapCircle c;
            c.center = ToPoint(m.mClumpCircles[i][j].mCenter);
            c.radius = m.mClumpCircles[i][j].mRadius;
            f.mCircles.push_back(c);
        }
    }
    f.mState.circle_counts = f.mCircleCounts.data();
    f.mState.circle_counts_len = (uint32_t)f.mCircleCounts.size();
    f.mState.circles = f.mCircles.data();
    f.mState.circles_len = (uint32_t)f.mCircles.size();

    for (size_t i = 0; i < m.mTreeTypes.size(); i++)
        f.mTreeTypes.push_back((int32_t)m.mTreeTypes[i]);
    f.mState.tree_types = f.mTreeTypes.data();
    f.mState.tree_types_len = (uint32_t)f.mTreeTypes.size();

    for (size_t i = 0; i < m.mResourceKinds.size(); i++) {
        EeRmapResourceKind k;
        k.minimum = (int32_t)m.mResourceKinds[i].mMinimum;
        k.maximum = (int32_t)m.mResourceKinds[i].mMaximum;
        k.id = (int32_t)m.mResourceKinds[i].mId;
        k.unit_type = (int32_t)m.mResourceKinds[i].mUnitType;
        k.area = (uint32_t)m.mResourceKinds[i].mArea;
        f.mResourceKinds.push_back(k);
    }
    f.mState.resource_kinds = f.mResourceKinds.data();
    f.mState.resource_kinds_len = (uint32_t)f.mResourceKinds.size();

    for (size_t i = 0; i < m.mResourceKindIds.size(); i++)
        f.mResourceKindIds.push_back((int32_t)m.mResourceKindIds[i]);
    f.mState.resource_kind_ids = f.mResourceKindIds.data();
    f.mState.resource_kind_ids_len = (uint32_t)f.mResourceKindIds.size();

    for (size_t i = 0; i < m.mResourceSlots.size(); i++) {
        EeRmapResourceSlot s;
        memset(&s, 0, sizeof s);
        s.id = (int32_t)m.mResourceSlots[i].mId;
        s.per_player = m.mResourceSlots[i].mPerPlayer;
        s.random_kind = m.mResourceSlots[i].mRandomKind;
        f.mResourceSlots.push_back(s);
    }
    f.mState.resource_slots = f.mResourceSlots.data();
    f.mState.resource_slots_len = (uint32_t)f.mResourceSlots.size();

    for (size_t i = 0; i < m.mSpacingRules.size(); i++) {
        EeRmapSpacingRule s;
        s.kind = (int32_t)m.mSpacingRules[i].mKind;
        s.minimum = (int32_t)m.mSpacingRules[i].mMinimum;
        s.maximum = (int32_t)m.mSpacingRules[i].mMaximum;
        f.mSpacingRules.push_back(s);
    }
    f.mState.spacing_rules = f.mSpacingRules.data();
    f.mState.spacing_rules_len = (uint32_t)f.mSpacingRules.size();

    for (size_t i = 0; i < gPlayerOrder.size(); i++)
        f.mPlayerOrder.push_back((int32_t)gPlayerOrder[i]);
    f.mState.player_order = f.mPlayerOrder.data();
    f.mState.player_order_len = (uint32_t)f.mPlayerOrder.size();
}

// Copies the result of a step back; what the generator never changes is left alone
static void Unflatten(const EeRmapState& s, EERandomMap& m)
{
    const EeRmapSettings& t = s.settings;
    m.mRandom.mSeed = t.random_seed;
    m.mRandomIntegers.mSeed = t.random_integers_seed;
    m.mNumTeamPlayers = t.num_team_players;
    UnflattenGrid(s.flat_tiles, m.mFlatTiles);
    UnflattenGrid(s.claimed_tiles, m.mClaimedTiles);
    UnflattenGrid(s.heights, m.mHeights);
    UnflattenGrid(s.tiles, m.mTiles);

    m.mPlayers.resize(s.players_len);
    for (size_t i = 0; i < s.players_len; i++) {
        const EeRmapPlayer& r = s.players[i];
        EERMPlayer& p = m.mPlayers[i];
        p.mPosition = FromPoint(r.position);
        p.mRadius = r.radius;
        p.mIndex = r.index;
        p.mTeam = r.team;
        p.mTileCount = r.tile_count;
        p.mIsNeutral = r.is_neutral != 0;
        p.mIsOuter = r.is_outer != 0;
        p.mIsMoon = r.is_moon != 0;
        p.mIsPlaced = r.is_placed != 0;
        p.mTiles.resize(r.tiles_len);
        for (size_t j = 0; j < r.tiles_len; j++)
            p.mTiles[j] = FromPoint(s.player_points[r.tiles_start + j]);
        p.mClumpCenters.resize(r.centers_len);
        for (size_t j = 0; j < r.centers_len; j++)
            p.mClumpCenters[j] = FromPoint(s.player_points[r.centers_start + j]);
    }
    for (size_t i = 0; i < s.teams_len && i < m.mTeams.size(); i++)
        for (size_t j = 0; j < s.teams[i].len && j < m.mTeams[i].mPositions.size(); j++)
            m.mTeams[i].mPositions[j] = FromPoint(s.team_positions[s.teams[i].start + j]);

    m.mObjects.resize(s.objects_len);
    for (size_t i = 0; i < s.objects_len; i++) {
        m.mObjects[i].mLocation = FromPoint(s.objects[i].location);
        m.mObjects[i].mKind = s.objects[i].kind;
        m.mObjects[i].mType = s.objects[i].object_type;
    }
    m.mFeatures.resize(s.features_len);
    for (size_t i = 0; i < s.features_len; i++) {
        const EeRmapFeature& r = s.features[i];
        m.mFeatures[i].mAngle = r.angle;
        m.mFeatures[i].mPosition.mX = r.position.x;
        m.mFeatures[i].mPosition.mY = r.position.y;
        m.mFeatures[i].mUnitType = r.unit_type;
        m.mFeatures[i].mIndex = r.index;
        m.mFeatures[i].mIsWorldUnit = r.is_world_unit != 0;
        m.mFeatures[i].mInRing = r.in_ring != 0;
        m.mFeatures[i].mIsHerd = r.is_herd != 0;
    }
    m.mClumpCircles.resize(s.circle_counts_len);
    size_t theCircle = 0;
    for (size_t i = 0; i < s.circle_counts_len; i++) {
        m.mClumpCircles[i].resize(s.circle_counts[i]);
        for (size_t j = 0; j < s.circle_counts[i]; j++, theCircle++) {
            m.mClumpCircles[i][j].mCenter = FromPoint(s.circles[theCircle].center);
            m.mClumpCircles[i][j].mRadius = s.circles[theCircle].radius;
        }
    }
    gPlayerOrder.resize(s.player_order_len);
    for (size_t i = 0; i < s.player_order_len; i++)
        gPlayerOrder[i] = s.player_order[i];
    gPlayerOrderIndex = t.player_order_index;
}

// One generator step through the C ABI (the numbers: ee_rmap_run)
static long RunStep(EERandomMap* ioMap, int32_t inStep)
{
    RMFlatState theFlat;
    EeRmapState theOutput;
    Flatten(*ioMap, theFlat);
    EeRmapResult* theResult = ee_rmap_run(inStep, &theFlat.mState, &theOutput);
    if (theResult == NULL)
        return 1;
    Unflatten(theOutput, *ioMap);
    ee_rmap_free(theResult);
    return 0;
}

static const UString* PIMGetName()
{
    return &gName;
}

static unsigned long PIMGetType()
{
    return gType;
}

static bool PIMIsValid()
{
    return true;
}

extern "C" long EERMPIMCreateFlatMap(EERandomMap* ioMap)
{
    return RunStep(ioMap, 3);
}

extern "C" long EERMPIMCreateHeightMap(EERandomMap* ioMap)
{
    return RunStep(ioMap, 4);
}

// Does nothing for neutral land and water; returns success
extern "C" long EERMPIMCreateNeutralLandWaterMap(EERandomMap* ioMap)
{
    return 0;
}

extern "C" long EERMPIMCreatePlayerLandWaterMap(EERandomMap* ioMap)
{
    return RunStep(ioMap, 2);
}

extern "C" long EERMPIMCreateTrees(EERandomMap* ioMap)
{
    return RunStep(ioMap, 8);
}

extern "C" long EERMPIMFilterHeightMap(EERandomMap* ioMap)
{
    return RunStep(ioMap, 5);
}

extern "C" long EERMPIMGenerateNeutralPositions(EERandomMap* ioMap)
{
    return RunStep(ioMap, 1);
}

extern "C" long EERMPIMGeneratePlayerPositions(EERandomMap* ioMap)
{
    return RunStep(ioMap, 0);
}

extern "C" long EERMPIMGenerateResources(EERandomMap* ioMap)
{
    return RunStep(ioMap, 7);
}

extern "C" long EERMPIMGenerateShallows(EERandomMap* ioMap)
{
    return RunStep(ioMap, 6);
}

// Reads the named .rmv script into the map settings (SetRandomMapValues, the C++ reader in Unit10.cpp)
extern "C" long EERMPIMSetRandomMapValues(EERandomMap* ioMap, const UString& inScriptName)
{
    return SetRandomMapValues(ioMap, inScriptName);
}

#include "lle/PIMModule.h"

// The plug-in's entry points (its DLL's export table before; registered in PIMModule.cpp)
static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    PIM_ENTRY(PIMGetType),
    PIM_ENTRY(PIMIsValid),
    PIM_ENTRY(EERMPIMCreateFlatMap),
    PIM_ENTRY(EERMPIMCreateHeightMap),
    PIM_ENTRY(EERMPIMCreateNeutralLandWaterMap),
    PIM_ENTRY(EERMPIMCreatePlayerLandWaterMap),
    PIM_ENTRY(EERMPIMCreateTrees),
    PIM_ENTRY(EERMPIMFilterHeightMap),
    PIM_ENTRY(EERMPIMGenerateNeutralPositions),
    PIM_ENTRY(EERMPIMGeneratePlayerPositions),
    PIM_ENTRY(EERMPIMGenerateResources),
    PIM_ENTRY(EERMPIMGenerateShallows),
    PIM_ENTRY(EERMPIMSetRandomMapValues),
    { NULL, NULL },
};
extern const PIMPlugin gPIMPluginDefaultRandomMap = { "Default.dll", sEntries };
