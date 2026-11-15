// C:\EEX\Empire Earth\AI Thread\EECPASGLand.h (RECONSTRUCTION; file name inferred). RTTI
// EECPASGLand > EECPAttackSubGroup > UReferenceCounter; vtable 0x43ad7c; sizeof 0x250; ctor 0x45ee9 (inGroup,
// inArg2: EECPAttackSubGroup's, then the members), ??_G 0x45f80, dtor 0x45f9c. A land attack sub group. Adds the
// slots +0x1dc..+0x24c and the members +0x1e8..+0x24c (ctor: the vector, the two locations, three zeroing tile
// ctors, then the init list; dtor 0x45f9c: the vector's _Tidy (ICF-shared with vector<EETileCoordinate>'s 0xa9c2c),
// the locations' vptrs, EECPAttackSubGroup's dtor). Slot signatures: the introducing class's
// (ee/EECPAttackSubGroup.h, checked over every body). Names are provisional.
#ifndef EE_EECPASGLAND_H
#define EE_EECPASGLAND_H

#include "ee/EECPAttackSubGroup.h"

class EECPASGTransport;
class EEAIContinent;
class EEAIPathSubSector;

// mAvoidedTiles's element (0xc bytes): a tile and an expiry time. Not EECPASGPathPoint: the time is
// unsigned (0x46570 `jbe` against gPhysics's time; Load reads an unsigned long), the deque's reads a long.
struct EECPASGLandPathPoint {
    EECPASGLandPathPoint() : mTime(0) {}
    // user-declared, members assigned in the body: vector::insert 0x4c989's temporary default-constructs mTile, then
    // assigns it (out-of-line 0x4d28b)
    EECPASGLandPathPoint(const EECPASGLandPathPoint& inOther)
    {
        this->mTile = inOther.mTile;
        this->mTime = inOther.mTime;
    }

    EETileLocation mTile;                   // +0x00
    unsigned long mTime;                    // +0x08 (gPhysics's time + 30000)
};

class EECPASGLand : public EECPAttackSubGroup {
public:
    EECPASGLand(EECPAttackGroup* inGroup, long inKind);   // 0x45ee9
    virtual ~EECPASGLand();                 // +0x00 (0x45f80)
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x46065
    // +0x1c 0x46094
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    virtual void Shutdown();                // +0x30 0x4646c
    virtual float GetDangerRatio(long inArg1);  // +0x44 0x46605
    virtual void BuildMoveMask();           // +0x50 0x466dd
    virtual void GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights);   // +0x54 0x46776
    virtual bool SplitInto(EECPAttackSubGroup* inOther, std::vector<EEComplexUnit*>& inUnits); // +0x5c 0x46c72
    virtual void PickSplitUnits(std::vector<EEComplexUnit*>& outUnits);  // +0x94 0x47231
    virtual float GetGuardChokepointPercentage();  // +0xa8 0x475cf
    virtual float GetGuardHighGroundPercentage();  // +0xac 0x475e3
    virtual float GetGuardContinentPercentage();  // +0xb0 0x475f7
    virtual float GetGuardTownPercentage();  // +0xb4 0x4760b
    virtual bool CanReach(const EECPILocation* inLocation);    // +0xc8 0x478bd
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x4862a
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x4a186
    virtual void SetDestination(const EECPILocation& inLocation);  // +0xfc 0x4a81c
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion); // +0x110 0x4ba29
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x114 0x4bab9
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x4be92
    virtual void RequestPathToTask();       // +0x140 0x460b5
    virtual bool TakeRequestedUnits();      // +0x154 0x47145
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies);  // +0x158 0x4761f
    virtual void Update(long inArg1);       // +0x168 0x488ce
    virtual void RequestPath(void* inFrom, void* inTo);   // +0x178 0x4b3c7
    virtual void PrunePath();               // +0x184 0x4b53d
    virtual bool HasReachedTerritory(long inTime);  // +0x1a4 0x4bc95
    virtual bool CheckTaskArrival(long inTime);  // +0x1b8 0x4be6e
    virtual void ClearAvoidedTiles();       // +0x1dc 0x4647d
    virtual bool CanCarryAllUnits();        // +0x1e0 0x46422
    virtual void DropTransport();           // +0x1e4 0x46178
    virtual bool AdvanceSubSector();        // +0x1e8 0x49387
    virtual long CountUnitsAboard();        // +0x1ec 0x46f78
    virtual long CountUnitsNotCarriable();  // +0x1f0 0x47015
    // +0x1f4 0x4705c (ret 0x10): the units' class +0x258 summed by the class flags +0x2fe/+0x2ff/+0x2fd/+0x300;
    // returns how many have one of them
    virtual long GetTransportNeeds(long& outA, long& outB, long& outC, long& outD);
    virtual float GetTransportDistance();   // +0x1f8 0x4759b (Virtual6C(the transport), 0 without one)
    virtual bool HasIdleUnitToLoad();       // +0x1fc 0x47c96
    // +0x200 0x4820e / +0x204 0x48262 / +0x208 0x483b7: fill outUnits (push_back of vector<EEComplexUnit*>; the
    // callers' local vector is a vector<long> by ICF)
    virtual void GetUnitsNotAboard(std::vector<EEComplexUnit*>& outUnits);
    virtual void GetUnitsNotCarriable(std::vector<EEComplexUnit*>& outUnits);
    virtual void GetUnitsLeftBehind(std::vector<EEComplexUnit*>& outUnits);
    virtual bool BoardTransports(std::vector<EEComplexUnit*>* inUnits);  // +0x20c 0x4a3a9
    virtual bool ChooseAirUnloadPoint();    // +0x210 0x4aa22
    virtual bool Virtual214();              // +0x214 0x27c0b4 (`xor al, al; ret`)
    virtual bool ChooseShoreUnloadPoint(bool inWater);  // +0x218 0x4afce (bool: type census, `mov cl, byte [ebp+8]` and the dword passed unconverted to FindCrossingTile)
    virtual bool ChooseSpaceUnloadPoint();  // +0x21c 0x4b3bc (ChooseUnloadPoint returns its al)
    virtual void StartTransportLoading();   // +0x220 0x4b971
    virtual void SetTransport(EECPASGTransport* inTransport);  // +0x224 0x4bc57 (sets mTransport, referenced)
    virtual bool ChooseUnloadPoint();       // +0x228 0x465a5
    // +0x22c 0x4665b (ret 0xc; the third argument unread): the continent to go for
    virtual EEAIContinent* GetTargetContinent(unsigned short inID, unsigned short inOtherID, long inArg3);
    virtual bool GetUncarriableUnits(std::vector<EEComplexUnit*>& outUnits);  // +0x230 0x473aa
    virtual bool IsGateBlocked(EEAIPathSubSector* inSector);  // +0x234 0x47b93 (a gate/wall sub-sector blocks)
    virtual bool FindCrossingTile(EEAIContinent* inFromContinent, EEAIContinent* inToContinent, const EETileLocation& inFrom, EETileLocation& outTile, bool inFlag); // +0x238 0x47f72
    virtual bool CanTransportLandAt(const EETileLocation& inTile);  // +0x23c 0x4a0db (the tile is free for the transport)
    virtual bool Virtual240();              // +0x240 0x27c0b4 (`xor al, al; ret`)
    virtual bool Virtual244();              // +0x244 0x27c0b4 (`xor al, al; ret`)
    virtual bool ChooseNearestUnloadPoint(bool inWater);  // +0x248 0x4a836 (bool: passed unconverted to FindCrossingTile)
    virtual bool RequestTransport(const EETileLocation& inTile);  // +0x24c 0x4b981 (asks mGroup for a transport to inTile)

    // 0x46488: CountSubmarines(), then mTransport = 0, its ReleaseLand when its mLand is this, and its Dereference
    // (through dynamic_cast<EECPAttackSubGroup*>); ends with 0x464e4 (provisional name)
    void ReleaseTransport();
    void ClearLocations();                  // 0x464e4 (ReleaseTransport's tail; provisional name)
    void AvoidTile(const EETileLocation& inTile);  // 0x45fca: mAvoidedTiles's entry for inTile expires in 30 s (added if new)
    bool IsTileAvoided(const EETileLocation& inTile);  // 0x46539: inTile's entry has not expired (an expired one is erased)
    bool IsGatePassable(EEAIPathSubSector* inSector);  // 0x47aa0 (PrunePath: a gate/wall sub-sector (kind 12..14) is passable)
    bool AreTransportUnitsWaiting();        // 0x4c169: a unit of the transport carries cargo and has no path (TestCondition)
    // Original success/condition flags in AL: direct callers in TestCondition (0x4be92).
    bool HasTargetsAtNextSubSector();       // 0x4617d: targets at the current wall/sector location
    bool CheckNextSubSector(long inTime);   // 0x49b5d: timed graph-path/blocked-link check
    bool AreUnitsSettled();                 // 0x4bd80: fraction of units ready to continue

    // +0x1e8: Load reads a count, then each point's tile (U2DPoint<long>'s Load 0x12e04) and an unsigned long into
    // its +8 (the base's deque reads a long there: the element type is inferred)
    std::vector<EECPASGLandPathPoint> mAvoidedTiles;
    EECPILocation mPickupLocation;          // +0x1f8 (Load/Restore/Save)
    EECPILocation mUnloadLocation;          // +0x210 (Load/Restore/Save)
    EETileLocation mEmbarkTile;             // +0x228 (Load/Save)
    EETileLocation mLandingTile;            // +0x230 (Load/Save)
    EETileLocation mNextSubSectorTile;      // +0x238
    // +0x240 (init list 0): the transport carrying it (EECPASGTransport::mLand points back; Save writes
    // dynamic_cast<EECPAttackSubGroup*>(mTransport)->mID, Restore: dynamic_cast<EECPASGTransport*> of the commander's
    // GetSubGroup(id))
    EECPASGTransport* GetTransport() const { return this->mTransport; }
    EECPASGTransport* mTransport;
    long mTransportStage;                   // +0x244 (init list 0; saved)
    long mNextSubSectorCheckTime;           // +0x248 (init list 0)
    bool GetNoTransport() const { return this->mNoTransport; }
    bool mNoTransport;                      // +0x24c (init list false)
};

#endif
