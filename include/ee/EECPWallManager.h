// C:\EEX\Empire Earth\AI Thread\EECPWallManager.h (RECONSTRUCTION; file name inferred; moved out of
// ee/EECPCivilizationManager.h; members decoded from the ctor 0x9cc24, the dtor 0x9ccde, Load 0x9e59c / Save
// 0x9eea6 and the unit's code). A computer player's wall builder.
// Requires ee/EECPCivilizationManager.h, ee/EEMap.h (RTSMapTileRun), lle/U2DPoint.h, <vector>.
#ifndef EE_EECPWALLMANAGER_H
#define EE_EECPWALLMANAGER_H

class EEAutoPathRequest;
class EEComplexUnit;
class EEPlayer;

// A wall to build towards an enemy (8 bytes; the +0x04 vector; Save writes the unit's id and the player's id,
// Restore turns them back by RTSWorld::GetUnit / gWorld.mPlayers) (provisional names)
struct EECPWMWall {
    EECPWMWall() {}
    EECPWMWall(EEComplexUnit* inUnit, EEPlayer* inEnemy) : mUnit(inUnit), mEnemy(inEnemy) {}
    EEComplexUnit* mUnit;                  // +0x00 (referenced)
    EEPlayer* mEnemy;                       // +0x04
};

// RTTI EECPWallManager (vtable 0x43c2c8, one slot: the deleting dtor 0x9ccc2; ctor 0x9cc24, new 0x88). Names are
// provisional
class EECPWallManager {
public:
    // save key (the unit's `$E` chain 0x9c833, guard byte 0x4e5da0, object 0x4e5da8)
    static const UString sData;             // "EECPWallManager-Data"
public:
    EECPWallManager(EECPCivilizationManager* inManager);   // 0x9cc24
    virtual ~EECPWallManager();             // +0x00 0x9ccde
    // the units' wall gaps closed by gates (EEAIArea.cpp 0x2e585 / EEUGBuildRepair.cpp 0x21e4fb call it) (provisional
    // name)
    void AddGates(EEComplexUnit* inUnit);   // 0x9d437 (ret 4)
    // the wall sizes table (EEWorld.cpp 0x13883f) (provisional name)
    static void ResetSizes();               // 0x9ee89
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x9e59c
    void UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps);  // 0x9ec12 (ret 4) (provisional name)
    void Reset();                           // 0x9ecc6
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x9eccb
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x9eea6
    void UnitAdded(EEComplexUnit* inUnit);  // 0x9f058 (ret 4)
    void UnitLost(EEComplexUnit* inUnit, bool inSwapped);  // 0x9f09d (ret 8; the body tests the argument as a byte: bool?)
    void Update(unsigned long inTime);      // 0x9f244

    // the unit's helpers (provisional names)
    bool IsPathBlocked(std::vector<U2DPoint<long> >& inTiles);  // 0x9cd0e (ret 4)
    void AddWalls(EEComplexUnit* inUnit);   // 0x9cf2a (ret 4)
    void BuildWall(U2DPoint<long>& inFrom, U2DPoint<long>& inTo, long inUnitClass, bool inBuildingPlannedWall);  // 0x9d05f (ret 0x10)
    bool IsRunClear(U2DPoint<long>& inFrom, U2DPoint<long>& inTo, bool inIgnoreResourceCenters, bool inSkipPathCheck);  // 0x9d1e7 (ret 0x10)
    void Clear();                           // 0x9d371
    void ClearRequest();                    // 0x9d3f2
    void StartNextWall();                   // 0x9d6cc
    EEComplexUnit* GetCenter(EEPlayer* inPlayer);   // 0x9d815 (ret 4; the player's town center, else its first unit)
    bool FindGate(std::vector<U2DPoint<long> >& inTiles, long& outSide, long& outIndex);   // 0x9d851 (ret 0xc)
    bool PlanWall();                        // 0x9d9e9
    bool IsRequestDone();                   // 0x9e58e
    bool IsPathUsable();                    // 0x9e6fe
    bool ChooseRun(std::vector<RTSMapTileRun>& inRuns, U2DPoint<long>& outFrom, U2DPoint<long>& outTo);  // 0x9e7fe (ret 0xc)
    bool ChooseRunNearGates(std::vector<RTSMapTileRun>& inRuns, U2DPoint<long>& outFrom, U2DPoint<long>& outTo);  // 0x9e99b (ret 0xc)

    std::vector<EECPWMWall> mWalls;         // +0x04
    std::vector<EEComplexUnit*> mGates;     // +0x14 (saved as unit ids)
    std::vector<RTSMapTileRun> mRuns;       // +0x24
    std::vector<U2DPoint<long> > mPath;     // +0x34
    U2DPoint<long> mGateTile;               // +0x44 (saved)
    EECPCivilizationManager* GetManager() const { return this->mManager; }
    EECPCivilizationManager* mManager;      // +0x4c
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x50 (the manager's +0x4c)
    EEAutoPathRequest* mRequest;            // +0x54 (referenced)
    EEComplexUnit* GetFrom() const { return this->mFrom; }
    EEComplexUnit* mFrom;                   // +0x58 (referenced)
    EEComplexUnit* mTo;                     // +0x5c (referenced)
    U2DPoint<long> mWallStart;              // +0x60 (saved)
    U2DPoint<long> mWallEnd;                // +0x68 (saved)
    float mF70;                             // +0x70 (saved)
    float mF74;                             // +0x74 (saved)
    unsigned long mNextTime;                // +0x78 (saved relative to gPhysics +0xd0)
    unsigned long mL7C;                     // +0x7c (saved; Read(unsigned long&))
    long mSize;                             // +0x80 (index into the sizes table)
    bool mBuildingPlannedWall;              // +0x84 (saved)
    bool mPathing;                          // +0x85
};

#endif
