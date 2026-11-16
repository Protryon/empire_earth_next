// A computer player's placement searches (stage 6, docs/AI_LUA_VERBS.md): what was left of the original's build
// manager (EECPBuildManager, 0x66af0..0x6e8d4) once its queues, builders and runner moved into Lua (data/ai/lib/verbs.lua):
// the town centre tile, the shore and space site tiles, the "unit near" tests, CanBuildAt and FindSite with its finders.
// The Lua verbs call them through ai.place. Member names are provisional. Requires <deque>, <vector>, <utility>, lle/U2DPoint.h (through lle/LowLevelEngine.h),
// ee/EETileCoordinate.h.
#ifndef EE_EECPPLACEMENT_H
#define EE_EECPPLACEMENT_H

#include <deque>
#include <vector>
#include <utility>
#include <set>
#include "ee/EETileCoordinate.h"

class EEComputerPlayer;
class EEPlayer;
class EEComplexUnit;
class EEUnit;
class FSFile;

// The find_if predicate of the build manager's "any unit of that type near (x, y)" tests (12 bytes passed by value;
// its operator() 0x921f7 and find_if 0x921d0 sit in EECPResourceManager.cpp's range, the (tile, distance) ctor 0x6a20e
// after its first user 0x69ee0). The distance is squared by the callers. Name is provisional
struct EECPUnitWithinDistance {
    EECPUnitWithinDistance(long inX, long inY, float inDistanceSquared)
        : mCenter(inX, inY), mDistanceSquared(inDistanceSquared) {}
    EECPUnitWithinDistance(const U2DPoint<long>& inCenter, float inDistanceSquared)
        : mCenter(inCenter), mDistanceSquared(inDistanceSquared) {}
    bool operator()(EEUnit* inUnit) const;

    U2DPoint<long> mCenter;                 // +0x00
    float mDistanceSquared;                 // +0x08
};

// 0x6b6b2 (cdecl, EECPBuildManager.cpp's range): unit classes 0x24, 0x41, 0x2d8, 0x302, 0x31f (EECPUnitManager 0x941bb)
// (provisional name)
bool EECPIsSiteClass(long inUnitClass);
// 0x23d1f1 (cdecl; callers EECPBuildManager::FindSpaceSite for class 0x2d8 and 0x2037c6): whether (inX, inY) suits inPlayer's
// class-0x2d8 site (provisional name)
bool EEIsSpaceDockSiteOpen(long inX, long inY, EEPlayer* inPlayer);

class EECPPlacement {
public:
    EECPPlacement(EEComputerPlayer* inPlayer);
    ~EECPPlacement();
    static const UString sData;             // "EECPBuildManager-Data" (the original's key)
    bool HasUnitNear(long inUnitType, long inX, long inY, float inDistance);  // 0x66d50
    bool HasAntiAirNear(long inX, long inY);  // 0x66de3
    bool HasTownCenterNear(long inX, long inY, float inDistance);  // 0x66ee8
    bool HasGateNear(long inX, long inY, float inRadius);  // 0x67122
    bool HasTowerNear(long inX, long inY, float inRadius);  // 0x671c2
    bool HasWallNear(long inX, long inY, float inRadius);  // 0x6725e
    void CollectSiteTiles(char inKind);     // 0x67b87 (2: the dock sites, 4: the space dock sites)
    bool HasEnemyNear(long inX, long inY);  // 0x67e10
    bool FindAirportSite(EETileCoordinate& outTile);  // 0x67f97
    bool FindAntiAirSiteNear(EEUnit* inUnit, EETileCoordinate& outTile);  // 0x681b3
    bool FindAntiAirSiteAround(EETileCoordinate& inTile, long inSize, EETileCoordinate& outTile);  // 0x681e3
    bool FindAntiAirSite(EETileCoordinate& outTile);  // 0x6832b
    bool FindCenterSite(long inUnitClass, EETileCoordinate& outTile);  // 0x6865a
    bool FindGranarySite(EETileCoordinate& outTile);  // 0x68862
    bool FindProductionSite(long inUnitClass, EETileCoordinate& outTile);  // 0x689d0
    bool FindShoreSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x68a45
    bool FindSpaceSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x68c90
    bool FindTowerSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x69245
    bool FindWonderSite(long inUnitClass, unsigned long inSize, EETileCoordinate& outTile);  // 0x69d6c
    bool FindSite(long inUnitClass, EETileCoordinate& ioTile);  // 0x6a228
    bool IsNearAirportEnd(long inX, long inY, long inSize, float inDistance);  // 0x6a833
    // whether one of the player's citizens could build the class at the tile (FindBuilder's land rule, any citizen)
    bool HasBuilderFor(const U2DPoint<long>& inTile, long inUnitClass);
    void SetTownCenter(EEUnit* inUnit);     // 0x6aef3
    // a unit the player gained: the first of a class that is not a land building sets the town centre tile while
    // there is none (the original unit manager's UnitAdded rule)
    void UnitAdded(EEComplexUnit* inUnit);
    void Reset();                           // no town centre tile
    long Restore();                         // the site tiles collected again
    long Load(FSFile& inFile);              // the town centre tile
    long Save(FSFile& outFile);
    // 0x6b0fb: a site test (EECPResourceCenter's too)
    bool CanBuildAt(unsigned short inContinent, long inUnitClass, EETileCoordinate& inTile, long inSize, bool inArg5,
        bool inArg6, long inX, long inY);

    std::vector<U2DPoint<long> > mWaterSites;  // (CollectSiteTiles(2))
    std::vector<U2DPoint<long> > mSpaceSites;  // (CollectSiteTiles(4))
    U2DPoint<long> mTownCenterTile;         // (-10000: none)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;
    unsigned long mNextResourceCenter;      // (FindProductionSite's round robin)
    bool mSiteTilesCollected;
    std::set<long> mTypesSeen;              // (UnitAdded's classes; not saved)
};

// the epoch's classes (the original civilization manager's GetAntiAirClass, GetFishingBoatClass, GetGateClass,
// GetTowerClass, GetWallClass; 0: none in the epoch)
long EECPAntiAirClass(EEComputerPlayer* inPlayer);
long EECPFishingBoatClass(EEComputerPlayer* inPlayer);
long EECPGateClass(EEComputerPlayer* inPlayer);
long EECPTowerClass(EEComputerPlayer* inPlayer);
long EECPWallClass(EEComputerPlayer* inPlayer);
// the player's living units of a class
long EECPCountOwn(EEComputerPlayer* inPlayer, long inType);

#endif
