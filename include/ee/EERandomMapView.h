// The game passes' view of the random map state (ee_rmap game.rs, EeRmapGameState): the four grids by their rows
// (no copy: the Rust pass reads and writes the C++ cells), the lists the passes read as flat copies, the two random
// streams' seeds (StoreSeeds takes them back). A template over the state's name: the exe sees the struct as
// EERandomMapData (ee/EERandomMapClass.h), the generator and tools/rmap_gate/rmap_dump.cpp as EERandomMap.
// Requires ee/EERandomMap.h (either name), rust/ee_rmap.h, <string.h>.
#ifndef EE_EERANDOMMAPVIEW_H
#define EE_EERANDOMMAPVIEW_H

#include <stdint.h>
#include <string.h>
#include <vector>
#include "rust/ee_rmap.h"

template <class T> inline void EERMRows(std::vector<std::vector<T> >& inGrid, std::vector<T*>& outRows)
{
    outRows.resize(inGrid.size());
    for (unsigned long x = 0; x < inGrid.size(); x++)
        outRows[x] = inGrid[x].data();
}

// The Rust passes' view of the state: the grids' rows and the lists' copies; StoreSeeds takes the two streams back
struct EERMGameView {
    std::vector<float*> mHeights;
    std::vector<unsigned long*> mTiles;
    std::vector<unsigned char*> mClaimed;
    std::vector<unsigned char*> mValid;
    std::vector<EeRmapGamePlayer> mPlayers;
    std::vector<EeRmapFeature> mFeatures;
    std::vector<EeRmapGameWall> mWalls;
    std::vector<EeRmapGameUnitName> mUnitNames;
    std::vector<EeRmapGameTerrain> mTerrains;
    std::vector<EeRmapGameRegion> mRegions;
    std::vector<EeRmapObject> mObjects;
    std::vector<EeRmapGameForest> mForests;
    std::vector<uint32_t> mForestTreeTypes;
    std::vector<uint32_t> mAnimals;
    EeRmapGameState mState;

    template <class M> EERMGameView(M& m, float inTileUnit, unsigned long inSpaceTerrain);
    template <class M> void StoreSeeds(M& m) const
    {
        m.mRandom.mSeed = this->mState.random_seed;
        m.mRandomIntegers.mSeed = this->mState.random_integers_seed;
    }
};

template <class M> EERMGameView::EERMGameView(M& m, float inTileUnit, unsigned long inSpaceTerrain)
{
    unsigned long i;
    memset(&this->mState, 0, sizeof this->mState);
    EERMRows(m.mHeights, this->mHeights);
    EERMRows(m.mTiles, this->mTiles);
    EERMRows(m.mClaimedTiles, this->mClaimed);
    EERMRows(m.mValidTiles, this->mValid);
    for (i = 0; i < m.mPlayers.size(); i++) {
        EeRmapGamePlayer p;
        memset(&p, 0, sizeof p);
        p.position.x = m.mPlayers[i].mPosition.mX;
        p.position.y = m.mPlayers[i].mPosition.mY;
        p.index = (uint32_t)m.mPlayers[i].mIndex;
        p.is_neutral = m.mPlayers[i].mIsNeutral;
        this->mPlayers.push_back(p);
    }
    for (i = 0; i < m.mFeatures.size(); i++) {
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
        this->mFeatures.push_back(r);
    }
    for (i = 0; i < m.mWalls.size(); i++) {
        const EERMWall& w = m.mWalls[i];
        EeRmapGameWall r = {(int32_t)w.mStartX, (int32_t)w.mStartY, (int32_t)w.mEndX, (int32_t)w.mEndY,
                            (int32_t)w.mUnitType};
        this->mWalls.push_back(r);
    }
    for (i = 0; i < m.mUnitNames.size(); i++) {
        EeRmapGameUnitName r = {(int32_t)m.mUnitNames[i].mUnitClass, (uint32_t)m.mUnitNames[i].mSize};
        this->mUnitNames.push_back(r);
    }
    for (i = 0; i < m.mTerrains.size(); i++) {
        EeRmapGameTerrain r;
        for (unsigned long e = 0; e < 31; e++)
            r.textures[e] = (int32_t)m.mTerrains[i].mTextures[e];
        this->mTerrains.push_back(r);
    }
    for (i = 0; i < m.mTerrainRegions.size(); i++) {
        const EERMTerrainRegion& t = m.mTerrainRegions[i];
        EeRmapGameRegion r = {(int32_t)t.mFrom, (int32_t)t.mTo, (uint32_t)t.mTerrain};
        this->mRegions.push_back(r);
    }
    for (i = 0; i < m.mObjects.size(); i++) {
        EeRmapObject r;
        r.location.x = m.mObjects[i].mLocation.mX;
        r.location.y = m.mObjects[i].mLocation.mY;
        r.kind = (int32_t)m.mObjects[i].mKind;
        r.object_type = (int32_t)m.mObjects[i].mType;
        this->mObjects.push_back(r);
    }
    for (i = 0; i < m.mForests.size(); i++) {
        const EERMForest& f = m.mForests[i];
        EeRmapGameForest r = {(uint32_t)this->mForestTreeTypes.size(), (uint32_t)f.mTreeTypes.size(),
                              (int32_t)f.mTerrainTexture};
        for (unsigned long t = 0; t < f.mTreeTypes.size(); t++)
            this->mForestTreeTypes.push_back((uint32_t)f.mTreeTypes[t]);
        this->mForests.push_back(r);
    }
    for (i = 0; i < m.mAvailableAnimals.size(); i++)
        this->mAnimals.push_back((uint32_t)m.mAvailableAnimals[i]);

    EeRmapGameState& s = this->mState;
    s.grid_n = (uint32_t)m.mTiles.size();
    s.heights = this->mHeights.data();
    s.tiles = this->mTiles.data();
    s.claimed_tiles = this->mClaimed.data();
    s.valid_tiles = this->mValid.data();
    s.grid_size = (uint32_t)m.mGridSize;
    s.maximum_y = (uint32_t)m.mMaximumY;
    s.map_characteristics = (uint32_t)m.mMapCharacteristics;
    s.is_space_map = m.mIsSpaceMap;
    s.half_map_size = m.mHalfMapSize;
    s.water_border = (int32_t)m.mWaterBorder;
    s.ring_center.x = m.mRing.mCenter.mX;
    s.ring_center.y = m.mRing.mCenter.mY;
    s.flat_area.left = m.mFlatArea.GetLeft();
    s.flat_area.bottom = m.mFlatArea.GetBottom();
    s.flat_area.right = m.mFlatArea.GetRight();
    s.flat_area.top = m.mFlatArea.GetTop();
    s.min_int_elevation = (int32_t)m.mMinIntElevation;
    s.max_int_elevation = (int32_t)m.mMaxIntElevation;
    s.height_map_smoothness = m.mHeightMapSmoothness;
    s.terrain_chaos = m.mTerrainChaos;
    s.seed = (uint32_t)m.mSeed;
    s.num_terrain_bands = (uint32_t)m.mNumTerrainBands;
    s.terrain_band_offset = (uint32_t)m.mTerrainBandOffset;
    s.max_loop_count = (uint32_t)m.mMaxLoopCount;
    s.tile_unit = inTileUnit;
    s.space_terrain = (int32_t)inSpaceTerrain;
    s.random_seed = m.mRandom.mSeed;
    s.random_integers_seed = m.mRandomIntegers.mSeed;
    for (i = 0; i < 7; i++)
        s.resource_terrain_texture[i] = (int32_t)m.mResourceTerrainTexture[i];
    s.players = this->mPlayers.data();
    s.players_len = (uint32_t)this->mPlayers.size();
    s.features = this->mFeatures.data();
    s.features_len = (uint32_t)this->mFeatures.size();
    s.walls = this->mWalls.data();
    s.walls_len = (uint32_t)this->mWalls.size();
    s.unit_names = this->mUnitNames.data();
    s.unit_names_len = (uint32_t)this->mUnitNames.size();
    s.terrains = this->mTerrains.data();
    s.terrains_len = (uint32_t)this->mTerrains.size();
    s.terrain_regions = this->mRegions.data();
    s.terrain_regions_len = (uint32_t)this->mRegions.size();
    s.objects = this->mObjects.data();
    s.objects_len = (uint32_t)this->mObjects.size();
    s.forests = this->mForests.data();
    s.forests_len = (uint32_t)this->mForests.size();
    s.forest_tree_types = this->mForestTreeTypes.data();
    s.forest_tree_types_len = (uint32_t)this->mForestTreeTypes.size();
    s.available_animals = this->mAnimals.data();
    s.available_animals_len = (uint32_t)this->mAnimals.size();
}

#endif
