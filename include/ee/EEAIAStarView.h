// New (port): the world as the AI's tile A* searches see it (docs/RUST_PLAN.md R5b.1, PORT_LEARNINGS "R5b.1"). Every
// read the tile goals (EEAIAStar*Goal: AcceptTile, GetCost, ClassifyTile) make of the live world goes through one
// EEAIAStarView, one query per fact of one tile, with a plain answer and no side effects. The goals keep their own
// state and side effects (flood counts, bounds, the collected tiles, mCheckCorners) and decide from the answers; the
// view only reads. Off a goal's hot path it is the seam a search capture records (tools/sim_capture.py) and a goal
// written elsewhere would query.
//
// The path goals' modifiers (EEAIPathGoalModifierBase and its subclasses) are asked through CanEnter /
// GetModifierCost, one noted query each; a modifier reads the world through a quiet view of the world map (the
// modifier queries below), whose answers are not noted: the modifier's answer is the noted fact. The search set-up's
// reads (EEAIAStarPathfindingGoalBase::Prepare: the walker's own and blocked tiles) are not per tile and stay with the
// goal.
//
// Requires ee/EETileCoordinate.h's U2DPoint<long>.
#ifndef EE_EEAIASTARVIEW_H
#define EE_EEAIASTARVIEW_H

#include "rust/ee_rust.h"   // EeAStarViewCallbacks

class EEMap;
class EEComplexUnit;
class EEAIPathGoalModifierBase;
class EEAIAStarPathAreaStorage;

// A continent, by tile (EEAIContinentManager::GetContinentAt, which the goals dereference unchecked)
struct EEAIAStarContinentInfo {
    unsigned short mID;
    unsigned char mCategory;                // EEAIContinentCategory
};

// A path area (EEAIPathArea), by tile or by id
struct EEAIAStarPathAreaInfo {
    bool mFound;                            // false: no area at the tile
    unsigned short mID;
    unsigned short mContinentID;
    long mKind;                             // EEAITileKind
};

// The first unit on a tile that blocks it for the area fills: (it blocks tiles or is a gate), it is not a
// foundation, and it is alive or a resource
struct EEAIAStarBlocker {
    bool mFound;
    long mUnitID;
    long mPlayerID;
    bool mIsGate;                           // category kUnitCategoryGate
    bool mIsWall;                           // its class's mIsWall
};

class EEAIAStarView {
public:
    EEAIAStarView() : mMap(0), mQuiet(false) {}
    // a modifier's view: the given map, its queries not noted when inQuiet
    EEAIAStarView(EEMap* inMap, bool inQuiet) : mMap(inMap), mQuiet(inQuiet) {}
    void SetMap(EEMap* inMap) { this->mMap = inMap; }
    // the queries the Rust goals and modifiers make (ee_astar's EeAStarViewCallbacks over this view)
    EeAStarViewCallbacks Callbacks();

    // The tile (inX, inY) of the search's map
    bool HasTile(long inX, long inY);
    long GetTerrain(long inX, long inY);    // on the map only
    bool IsEdgeTile(long inX, long inY);    // EEMap::IsEdgeTile (a test of the coordinates)
    bool HasUnits(long inX, long inY);      // on the map only
    long GetAIValue(long inX, long inY);    // on the map only
    long GetHeightLevel(long inX, long inY);   // on the map only: floor(height / (the level step / 2) + 1e-5)

    // The continent manager's analysis of the tile
    unsigned short GetContinentID(long inX, long inY);
    EEAIAStarContinentInfo GetContinentAt(long inX, long inY);
    long GetTileFlag(long inX, long inY);
    long GetTileValue(long inX, long inY);  // the manager's value grid, GetTileFlag while it is not built
    EEAIAStarPathAreaInfo GetPathAreaAt(long inX, long inY);
    EEAIAStarPathAreaInfo GetPathArea(unsigned short inID);   // dereferenced unchecked, as the sector fill does
    // the path-area fill's id grid (the sector fill reads the storage of the fill before it)
    unsigned short GetStoredPathAreaID(const EEAIAStarPathAreaStorage* inStorage, long inX, long inY);
    // the choke point pass's grids at index inIndex (EEAIChokePoint.cpp): a flagged tile that is not an area tile
    // (kind 0x36); *outSerial its area serial when it is
    bool IsChokeTile(long inIndex, unsigned long* outSerial);

    // The units on the tile (on the map only)
    long GetResourceKind(long inX, long inY);   // EEAIHasResourceUnit: 0, or the resource kind 1..3
    bool HasBlocker(long inX, long inY);
    long GetFirstBlockerID(long inX, long inY); // -1 if none
    EEAIAStarBlocker GetFirstBlocker(long inX, long inY);   // with its player and kind (reads its player)
    // the first unit alive or a resource that (blocks tiles and is not a foundation) or is a gate; -1 if none
    long GetFirstObstacleID(long inX, long inY);
    // walking the units: true at a live wall of inPlayerID's (not a gate, not a foundation), false at a blocker first
    bool HasOwnWallFirst(long inX, long inY, long inPlayerID);
    // the area fill's gate test: the first unit (alive or a resource) that is a gate of inPlayerID's or a blocker
    long GetAreaGateOwner(long inX, long inY, long inPlayerID);
    // the sector fill's gate test: the first live gate of inPlayerID's or blocker
    long GetSectorGateOwner(long inX, long inY, long inPlayerID);
    bool HasLiveDock(long inX, long inY);   // a live dock or navy yard, not a foundation
    bool HasLiveSpaceDock(long inX, long inY);  // a live space dock, space turret or orbital station, not a foundation

    // The walker's view of the tile
    bool CanMoveOn(EEComplexUnit* inWalker, long inTerrain);   // its class's CanMoveOnTerrain
    bool ArePassable(EEComplexUnit* inWalker, long inX, long inY);   // every unit on the tile IsPassableBy it
    // the path goals' crowd cost of entering the tile: +50 per idle or attacking collidable unit (+150 for a big one
    // when the walker is small), not the walker, not a small unit of its group, not a foundation
    long GetCrowdCost(EEComplexUnit* inWalker, long inX, long inY);
    bool IsWide(EEComplexUnit* inWalker);   // its class's inner collision sphere is 1 or more
    long GetFormationRank(EEComplexUnit* inWalker);
    bool IgnoresFormation(EEComplexUnit* inWalker);   // its terrain mask's bit 14
    // a formation rank probe tile of the world map: on it and (an AI value, terrain the walker cannot move on, or
    // not on continent inContinentID)
    bool IsRankProbeBlocked(EEComplexUnit* inWalker, unsigned short inContinentID, long inX, long inY);

    // The goal's modifier (world-side policies for now: see the header comment)
    bool CanEnter(EEAIPathGoalModifierBase* inModifier, long inX, long inY, bool inGoal);
    long GetModifierCost(EEAIPathGoalModifierBase* inModifier, long inFromX, long inFromY, long inX, long inY,
                         long inParentX, long inParentY);

    // The modifiers' own reads (EEAIAStarBoundedPathGoalModifiers.cpp, EEEMStorm.cpp; asked of a quiet view)
    // the first unit on the tile (with a complex unit) that blocks the walker's step: stopped, collidable, not the
    // walker, outside its group; at sea (inSea) a sea-going unit (a naval building with air traffic in any group) as
    // submerged as the walker, else a space-going one
    bool HasStepBlocker(EEComplexUnit* inWalker, bool inSea, long inX, long inY);
    // the unit region of the tile ((inX, inY) / the region size, truncated): -1 none, 0 empty, 1 with units
    long GetUnitRegionState(long inX, long inY);
    long GetEpoch(EEComplexUnit* inWalker);     // its player's current epoch
    bool IsAlone(EEComplexUnit* inWalker);      // no active group, or one of at most one unit
    long GetWaterState(EEComplexUnit* inWalker);
    bool IsSeaGoing(EEComplexUnit* inWalker);   // its class's domains include the sea
    // every unit on the tile IsPassableBy the walker, skipping walls (inSkip kSkipWalls) or trees (kSkipTrees)
    enum { kSkipNone = 0, kSkipWalls = 1, kSkipTrees = 2 };
    bool ArePassableExcept(EEComplexUnit* inWalker, long inX, long inY, long inSkip);
    bool HasHill(long inX, long inY);           // the continent manager's hill at the tile
    bool RandomAtMost(float inChance);          // the physics stream's next Generate() <= inChance (one draw)

private:
    long Note(long inQuery, long inX, long inY, long inAnswer, long inArg = 0, long inArg2 = 0);

    EEMap* mMap;
    bool mQuiet;                            // a modifier's view: its answers are not noted
};

// A rectangle (U2DRectangle<long>) as ee_astar's EeAStarRect
template <class R>
inline EeAStarRect EEAIAStarRectOf(const R& inRect)
{
    EeAStarRect theRect;
    theRect.bottom = inRect.mBottom;
    theRect.left = inRect.mLeft;
    theRect.right = inRect.mRight;
    theRect.top = inRect.mTop;
    return theRect;
}

#endif
