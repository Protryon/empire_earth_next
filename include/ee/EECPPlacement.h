// A computer player's placement searches (stage 6, docs/AI_LUA_VERBS.md): what was left of the original's build
// manager (EECPBuildManager, 0x66af0..0x6e8d4) once its queues, builders and runner moved into Lua (data/ai/lib/verbs.lua):
// the town centre tile, the shore and space site tiles, the "unit near" tests, CanBuildAt and FindSite with its finders.
// The Lua verbs call them through ai.place. Their bodies and the class's own data (the site tiles, the classes seen)
// are Rust's (ee_sim::cp::placement; AI lane B, round 2): the members below are the forwarders. Member names are
// provisional. Requires <deque>, <vector>, <utility>, lle/U2DPoint.h (through lle/LowLevelEngine.h), ee/EETileCoordinate.h.
#ifndef EE_EECPPLACEMENT_H
#define EE_EECPPLACEMENT_H

#include <deque>
#include <vector>
#include <utility>
#include "ee/EETileCoordinate.h"

class EEComputerPlayer;
class EEPlayer;
class EEComplexUnit;
class EEUnit;
class FSFile;
struct EeCpPlacement;                       // the Rust data (rust/sim/cp.h)

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
    bool HasUnitNear(long inUnitType, long inX, long inY, float inDistance);
    bool HasAntiAirNear(long inX, long inY);
    bool HasTownCenterNear(long inX, long inY, float inDistance);
    bool HasGateNear(long inX, long inY, float inRadius);
    bool HasTowerNear(long inX, long inY, float inRadius);
    bool HasWallNear(long inX, long inY, float inRadius);
    void CollectSiteTiles(char inKind);     // 0x67b87 (2: the dock sites, 4: the space dock sites)
    bool HasEnemyNear(long inX, long inY);
    bool FindAirportSite(EETileCoordinate& outTile);
    bool FindAntiAirSiteNear(EEUnit* inUnit, EETileCoordinate& outTile);
    bool FindAntiAirSiteAround(EETileCoordinate& inTile, long inSize, EETileCoordinate& outTile);
    bool FindAntiAirSite(EETileCoordinate& outTile);
    bool FindCenterSite(long inUnitClass, EETileCoordinate& outTile);
    bool FindGranarySite(EETileCoordinate& outTile);
    bool FindProductionSite(long inUnitClass, EETileCoordinate& outTile);
    bool FindShoreSite(long inUnitClass, long inSize, EETileCoordinate& outTile);
    bool FindSpaceSite(long inUnitClass, long inSize, EETileCoordinate& outTile);
    bool FindTowerSite(long inUnitClass, long inSize, EETileCoordinate& outTile);
    bool FindWonderSite(long inUnitClass, unsigned long inSize, EETileCoordinate& outTile);
    bool FindSite(long inUnitClass, EETileCoordinate& ioTile);
    bool IsNearAirportEnd(long inX, long inY, long inSize, float inDistance);
    // whether one of the player's citizens could build the class at the tile (FindBuilder's land rule, any citizen)
    bool HasBuilderFor(const U2DPoint<long>& inTile, long inUnitClass);
    void SetTownCenter(EEUnit* inUnit);
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

    U2DPoint<long> mTownCenterTile;         // (-10000: none)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;
    EeCpPlacement* mRust;                   // the dock and space site tiles (CollectSiteTiles(2), (4)), whether they
                                            // were collected, UnitAdded's classes (not saved)
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
