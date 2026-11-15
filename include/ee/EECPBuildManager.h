// C:\EEX\Empire Earth\AI Thread\EECPBuildManager.h (RECONSTRUCTION; file name inferred; moved out of
// ee/EECPCivilizationManager.h, which includes it at the same place). A computer player's build manager
// (RTTI EECPBuildManager, no base; vtable 0x43c050, one slot: ??_G 0x66cfb). EECPBuildManager.cpp 0x66af0..0x6e8d4:
// the key "EECPBuildManager-Data" (guarded chain 0x66af0; the static is not declared: proposals), the members in
// address order, the deque/vector/heap COMDATs last. sizeof 0x170 (the civilization manager news 0x170). Member names
// are provisional. Requires <deque>, <vector>, <utility>, lle/U2DPoint.h (through lle/LowLevelEngine.h),
// ee/EETileCoordinate.h.
#ifndef EE_EECPBUILDMANAGER_H
#define EE_EECPBUILDMANAGER_H

#include <deque>
#include <vector>
#include <utility>
#include "ee/EETileCoordinate.h"

class EECPCivilizationManager;
class EEComputerPlayer;
class EEPlayer;
class EEComplexUnit;
class EEUnit;
class FSFile;

// A queued building: its unit class and the tile asked for (0, 0: anywhere). The pair's (const&, const&) ctor is the
// out-of-line COMDAT 0x6c5f3; the deque iterator's operator* is 0x4c700 (ICF, EECPASGLand.cpp's range).
typedef std::pair<unsigned long, U2DPoint<long> > EECPBuildRequest;   // first: unsigned (Load reads it with Read(unsigned long&), Save writes it)
typedef std::deque<EECPBuildRequest> EECPBuildQueue;

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

class EECPBuildManager {
public:
    EECPBuildManager(EECPCivilizationManager* inManager);   // 0x66c5d
    virtual ~EECPBuildManager();            // +0x00 0x66d17 (vtable 0x43c050; ??_G 0x66cfb)
    static const UString sData;             // "EECPBuildManager-Data" (0x4e5d00, guard 0x4e5cf8 bit 1; chain 0x66af0..0x66b33)
    bool HasUnitNear(long inUnitType, long inX, long inY, float inDistance);  // 0x66d50 (ret 0x10)
    bool HasAntiAirNear(long inX, long inY);  // 0x66de3 (ret 8)
    bool HasTownCenterNear(long inX, long inY, float inDistance);  // 0x66ee8 (ret 0xc; EECPResearchManager 0x90015 too)
    // EECPWallManager.cpp's calls (ret N from the bodies) (provisional names)
    bool HasGateNear(long inX, long inY, float inRadius);  // 0x67122 (ret 0xc)
    bool HasTowerNear(long inX, long inY, float inRadius);  // 0x671c2 (ret 0xc)
    bool HasWallNear(long inX, long inY, float inRadius);  // 0x6725e (ret 0xc)
    bool IsQueued(long inUnitClass);        // 0x672fa (ret 4): is the class queued
    void BuilderLost(EEComplexUnit* inUnit);  // 0x67356 (ret 4)
    bool StartBuilding(EECPBuildRequest inRequest);  // 0x674c4 (ret 0xc; Update passes a queue front by value)
    void AssignBuilders();                  // 0x677c6
    void CancelStalledBuilds();             // 0x679d7
    void CollectSiteTiles(char inKind);     // 0x67b87 (ret 4; Restore passes 2 and 4)
    bool HasEnemyNear(long inX, long inY);  // 0x67e10 (ret 8)
    bool FindAirportSite(EETileCoordinate& outTile);  // 0x67f97 (ret 4)
    bool FindAntiAirSiteNear(EEUnit* inUnit, EETileCoordinate& outTile);  // 0x681b3 (ret 8)
    bool FindAntiAirSiteAround(EETileCoordinate& inTile, long inSize, EETileCoordinate& outTile);  // 0x681e3 (ret 0xc; uses no member)
    bool FindAntiAirSite(EETileCoordinate& outTile);  // 0x6832b (ret 4)
    bool FindCenterSite(long inUnitClass, EETileCoordinate& outTile);  // 0x6865a (ret 8)
    bool FindGranarySite(EETileCoordinate& outTile);  // 0x68862 (ret 4)
    bool FindProductionSite(long inUnitClass, EETileCoordinate& outTile);  // 0x689d0 (ret 8)
    bool FindShoreSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x68a45 (ret 0xc; the class's +0x28c)
    bool FindSpaceSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x68c90 (ret 0xc)
    bool FindTowerSite(long inUnitClass, long inSize, EETileCoordinate& outTile);  // 0x69245 (ret 0xc)
    bool FindWonderSite(long inUnitClass, unsigned long inSize, EETileCoordinate& outTile);  // 0x69d6c (ret 0xc; FindSite passes 10, 8, .., 2)
    bool FindSpotNearTownCenter(EEUnit* inUnit, EETileCoordinate& outTile);  // (the body reads inUnit->mUnitClass) // 0x69ee0 (ret 8; EECPResearchManager 0x88283)
    bool FindSite(long inUnitClass, EETileCoordinate& ioTile);  // 0x6a228 (ret 8; stores the found tile)
    bool FindSpotNearDock(EEUnit* inUnit, EETileCoordinate& outTile);  // (the body reads inUnit->mUnitClass) // 0x6a473 (ret 8; EECPResearchManager 0x882d1)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x6a73b
    bool IsNearAirportEnd(long inX, long inY, long inSize, float inDistance);  // 0x6a833 (ret 0x10; CanBuildAt): a wall gate near
    bool HasLaterEpochRequests();           // 0x6a989: anything queued above the current epoch
    void QueueBuilding(unsigned long inUnitClass, bool inAppend, bool inFront);  // 0x6a9c1 (ret 0xc; the body tests bytes: bools)
    void QueueBuildingAt(long inUnitClass, long inX, long inY);  // 0x6ab26 (ret 0xc)
    void BuildingAbandoned(EEComplexUnit* inUnit);  // 0x6abb5 (ret 4; EEUGBuildRepair 0x21f212)
    // its +0x150/+0x154 tile (EECPWallManager 0x9eb08 reads the fields directly; the members are not decoded, so an
    // accessor stands in) (provisional name)
    U2DPoint<long>& GetCenter();
    void Reset();                           // 0x6acb8
    void DropObsoleteRequests(long inEpoch);  // 0x6ad23 (ret 4)
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x6add5
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x6ae17
    void SetTownCenter(EEUnit* inUnit);     // 0x6aef3 (ret 4; EECPUnitManager::UnitAdded)
    void Update(unsigned long inTime);      // 0x6af0d
    // 0x6b0fb (ret 0x20): a site test (EECPResourceCenter 0x8bdfa calls it through the civilization manager's +0x3c)
    bool CanBuildAt(unsigned short inContinent, long inUnitClass, EETileCoordinate& inTile, long inSize, bool inArg5,
        bool inArg6, long inX, long inY);

    EEEpochArray<EECPBuildQueue> mQueues;   // +0x004 one per epoch: the class's tech's epoch (EETechTreeEpochOf)
    std::vector<U2DPoint<long> > mWaterSites;  // +0x130 (CollectSiteTiles(2))
    std::vector<U2DPoint<long> > mSpaceSites;  // +0x140 (CollectSiteTiles(4))
    U2DPoint<long> mTownCenterTile;         // +0x150 (U2DPoint<long>: Load/Save call its Load/Save; the ctor zeroes it) (-10000: none) (provisional name)
    EECPCivilizationManager* GetManager() const { return this->mManager; }
    EECPCivilizationManager* mManager;      // +0x158 (the ctor's argument)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x15c (mManager->mPlayer)
    unsigned long mNextResourceCenter;      // +0x160 (FindProductionSite's round robin)
    unsigned long mNextBuilderCheckTime;    // +0x164
    unsigned long mNextUpdateTime;          // +0x168 (a time: saved relative to gPhysics' +0xd0)
    bool mSiteTilesCollected;               // +0x16c
};

#endif
