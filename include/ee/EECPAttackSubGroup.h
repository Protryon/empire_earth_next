// C:\EEX\Empire Earth\AI Thread\EECPAttackSubGroup.h (RECONSTRUCTION; file name inferred). A computer player's attack
// sub group (RTTI EECPAttackSubGroup > UReferenceCounter; vtable 0x43bd20, 119 slots, two of them _purecall
// (+0x158, +0x178): abstract; sizeof 0x1e8: EECPASGAir/Sea/Spaceship add no member, the transport kinds put their
// EECPASGTransport at +0x1e8; EECPAttackSubGroup.cpp 0x5ac57..0x62f8a): ctor
// 0x5adb2 (inGroup, inArg2), ??_G 0x5afdc, dtor 0x5aff8. The kinds (one header each) override slots; EECPASGLand
// adds slots +0x1dc..+0x24c. The targets (+0x30, +0x40) come from a UMemoryPool<EECPASTargetUnit> (0x4e5c88; its
// guarded template-static chain 0x5ac57 opens this unit; 0x5d08f takes one, 0x5d0b3 gives one back).
// Slot signatures from every implementation (`ret N`, the first reads, the value left at `ret`): `void*` marks a
// pointer argument whose pointee is not decoded yet. Slot and member
// names are provisional unless a comment names the evidence. Requires lle/LowLevelEngine.h (UReferenceCounter,
// FSFile, <vector>), ee/EECPILocation.h.
#ifndef EE_EECPATTACKSUBGROUP_H
#define EE_EECPATTACKSUBGROUP_H

#include <map>
#include <deque>
#include <string.h>

#include "ee/EECPAGTask.h"
#include "ee/EECPASGStats.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EETileLocation.h"

template <class T> class U2DRectangle;
typedef U2DRectangle<long> EEAIAStarBounds;   // ee/EEAIAStarStorage.h
template <class T> class UMemoryPool;         // lle/UMemoryPool.h (EECPAttackSubGroup::sTargetPool)
class EEAIGraphPathGoalModifierBase;
class EEAutoPathRequest;
class EEComplexUnit;
class EEComputerPlayer;
class EECPASTargetUnit;
class EECPAttackFormation;
class EECPAttackGroup;
class EECPAttackSubGroupUnit;
class EECPPersonnelRequest;
class EECPIntelligencePlayerFile;
class EEUnit;
class EEUnitGoal;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

// What the sub group counts of its units (no vtable; 0x34 bytes at EECPAttackSubGroup +0x130). The sub group's ctor
// and 0x5b3ac call Clear 0x5ad6e (so the ctor is an inline one calling it); 0x5b3ac then Adds every unit. Names
// (provisional name)
struct EECPASGUnitCounts {
    EECPASGUnitCounts() { this->Clear(); }
    void Clear();                           // 0x5ad6e
    // 0x5ac9c (ret 8; the second argument unread): counts inUnit by its class's flags (+0x2d0) and kinds
    void Add(EEComplexUnit* inUnit, long inArg2);

    float mTotalAttack;                     // +0x00 (+= the unit's +0x34 object's +0xd8)
    long mNumBuildings;                     // +0x04
    long mNumWonders;                       // +0x08
    long mNumPlagueTargets;                 // +0x0c
    long mNumCyborgs;                       // +0x10
    long mNumShips;                         // +0x14
    long mNumAircraft;                      // +0x18
    long mNumBombers;                       // +0x1c
    long mL20;                              // +0x20
    long mNumCitizens;                      // +0x24
    long mNumSubmarines;                    // +0x28
    long mNumSpaceUnits;                    // +0x2c
    bool mHasCalamity;                      // +0x30 (the unit's +0x254 is set)
};

// A long and 17 flags (0x18 bytes at EECPAttackSubGroup +0x164; slot +0x19c builds one on its stack the same way:
// memset of the flags, then the long; a unit's +0x2c object's slot +0x28 fills it). And 0x4c2c8 keeps a flag only
// where inOther has it too. Names are provisional.
struct EECPASGFlags {
    EECPASGFlags()
    {
        memset(this->mFlags, 0, sizeof(this->mFlags));
        this->mPlayerID = 0;
    }
    EECPASGFlags(const EECPASGFlags& inOther)
    {
        memcpy(this->mFlags, inOther.mFlags, sizeof(this->mFlags));
        this->mPlayerID = 0;
    }
    EECPASGFlags& operator=(const EECPASGFlags& inOther)
    {
        memcpy(this->mFlags, inOther.mFlags, sizeof(this->mFlags));
        this->mPlayerID = inOther.mPlayerID;
        return *this;
    }
    void And(const EECPASGFlags& inOther);  // 0x4c2c8

    long mPlayerID;                         // +0x00
    bool mFlags[17];                        // +0x04
};

// A deque element of EECPAttackSubGroup (+0x60; 0xc bytes: 0x155 per block). The zeroing default ctor is resize's
// T() (0x61406); operator= is out of line (0x5ad99, returns this: the fill/copy_backward helpers call it);
// Load/Save go through the tile's Load/Save (U2DPoint<long>'s ICF-folded 0x12e04/0x12e38), then mL08. Names
// (provisional name)
struct EECPASGPathPoint {
    EECPASGPathPoint() : mSectorKind(0) {}
    EECPASGPathPoint(const EECPASGPathPoint& inPoint) : mTile(inPoint.mTile), mSectorKind(inPoint.mSectorKind) {}
    EECPASGPathPoint& operator=(const EECPASGPathPoint& inPoint);   // 0x5ad99 (EECPAttackSubGroup.cpp)

    EETileLocation mTile;                   // +0x00 (slot +0x180 copies the front's into mNextWaypoint's tile)
    long mSectorKind;                       // +0x08
};

// Slot +0x54's third argument (0x3c bytes: 15 floats). The base 0x5cc2d zeroes it (`rep stosd`) and sets
// +0x10 = mHealThreshold, EECPASGAir 0x41d18 sets +0x28 = 1.0f; 0x4c2b8 (ret 4) fills each with a value. Names are
// provisional.
struct EECPASGWeights {
    // the influence-cost modifier ctor 0x40fe8f zeroes mWeights (rep stosd = Fill) before its member
    // stores and the vptr store, i.e. in the member-init phase: mWeights(0.0f) (provisional name)
    EECPASGWeights() {}
    EECPASGWeights(float inValue) { this->Fill(inValue); }
    void Fill(float inValue) { for (long i = 0; i < 15; i++) this->mValues[i] = inValue; }
    float mValues[15];
};

class EECPAttackSubGroup : public UReferenceCounter {
public:
    // 0x5adb2: mGroup = inGroup; mPlayer = mPlayer2 = inGroup->mCommander's +0x18; mID from that player's +0xbc0
    // object's counter (+0x5c, post-increment); mTask.mPlayer from inGroup->mTask.GetPlayerID(); mFormation = new
    // EECPAttackFormation(this); mCounterKind2 = inArg2
    EECPAttackSubGroup(EECPAttackGroup* inGroup, long inKind);
    virtual ~EECPAttackSubGroup();          // +0x00 (0x5afdc)
    virtual void AddUnit(EEComplexUnit* inUnit);   // +0x04 0x5b075 (EECPAttackGroup 0x54933/0x54c2b pass a unit)
    virtual void RequestUnits(std::vector<long>& inClasses, std::vector<long>& inUnitIDs, long inContinent, long inTownID); // +0x08 0x5b2c4
    virtual bool HasGathered();             // +0x0c 0x5b421
    virtual bool IsRequestFilled();         // +0x10 0x5b3f1 (EECPASGAir::TestCondition returns its al)
    virtual void SetTask(const EECPAGTask& inTask);     // +0x14 0x5b4d7 (EECPAGTask::operator= on it)
    virtual void MoveToNearestTown();       // +0x18 0x5b996
    // +0x1c 0x5b5e6
    virtual void AttackWith(EECPAttackSubGroupUnit* inUnit, EEComplexUnit* inTarget, EETileCoordinate* inTile);
    // +0x20 0x5bbc5 (`mov ecx, dword ptr [esp + 4]; call 0x465c2f`)
    virtual void DropTargetOf(EECPAttackSubGroupUnit* inUnit);
    virtual void SetOutOfFight(bool inIsOutOfFight);  // +0x24 0x5bbd1
    virtual EETileLocation ComputeCenterPoint(long inMustBeInList);  // +0x28 0x5bc18 (hidden result: EEFn08282 of the units' tiles)
    virtual bool RandomizeTaskLocation();   // +0x2c 0x5bcd0
    virtual void Shutdown();                // +0x30 0x5bd63
    virtual void CancelPath();              // +0x34 0x5be5a
    virtual void ClearTargets();            // +0x38 0x5bdeb
    virtual float GetGateWeight();          // +0x3c 0x5be8d
    virtual bool FindPosition(EETileLocation& outTile, bool inArg2, bool inForce); // +0x40 0x5c0d7
    virtual float GetDangerRatio(long inArg1);  // +0x44 0x5c774
    virtual void GetLocation(EECPILocation& outLocation); // +0x48 0x5c7e9
    virtual void UpdateAreaEffect();        // +0x4c 0x5c829
    virtual void BuildMoveMask();           // +0x50 0x5c925 (no value at ret)
    virtual void GetPathCosts(void* inFrom, void* inTo, EECPASGWeights& outWeights);   // +0x54 0x5cc2d (no value at ret)
    virtual float GetWallWeight();          // +0x58 0x5cc4c
    // +0x5c 0x5ce19: copies its fields into inOther, sub group units of inUnits for it, RemoveUnits(inUnits)
    virtual bool SplitInto(EECPAttackSubGroup* inOther, std::vector<EEComplexUnit*>& inUnits);
    virtual long CountFightingUnits();      // +0x60 0x5cddf
    virtual long GetNumTargets();           // +0x64 0x5cdb1
    virtual void Disband();                 // +0x68 0x5d0e3
    virtual float DistanceTo(EECPAttackSubGroup* inOther);  // +0x6c 0x5d349 (calls inOther's slot +0x40)
    virtual float DistanceSquaredTo(EECPAttackSubGroup* inOther);  // +0x70 0x5d3b3 (the same shape)
    // +0x74 0x5d41d: the units by continent id (GetContinentID; new vectors; EECPASGLand::TakeRequestedUnits deletes
    // the values; the element type inferred)
    virtual long GroupUnitsByContinent(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>& outUnits, bool inSkipCarried);
    // +0x78 0x5d5a2: EnabledChanged's body keyed by the unit's GetFootprintContinent(inArg2)
    virtual long GroupUnitsByWaterArea(std::map<unsigned short, std::vector<EECPAttackSubGroupUnit*>*>& outUnits, long inKind,
                           bool inSkipCarried);
    virtual void RequestSplit(bool inResetState);  // +0x7c 0x5d725
    virtual bool MoveToTask(bool inForce);  // +0x80 0x5d790
    // +0x84 0x5d7de: gPathManager.RequestPath(mPlayer's id, inFrom, inTo, &mFlags, 1, inArg3, inArg4, 1), then the attack
    // commander's mL2C++
    virtual EEAutoPathRequest* NewPathRequest(const EETileCoordinate& inFrom, const EETileCoordinate& inTo, long inRevs,
                                         EEAIGraphPathGoalModifierBase* inModifier);
    virtual EETileLocation GetRallyPoint(bool inIncludeAllies);  // +0x88 0x5cdc4 (`ret 8`: hidden result = mRallyPoint's tile)
    virtual bool HasUnit(EEComplexUnit* inUnit);    // +0x8c 0x5d829 (inUnit is one of mUnits' units)
    virtual void ScanTargets(bool inArg1);  // +0x90 0x5da23 (bool: passed on to CollectTargets)
    virtual void PickSplitUnits(std::vector<EEComplexUnit*>& outUnits);  // +0x94 0x5db99 (EECPASGLand's: clear, push_back)
    virtual bool IsUnseenByTaskPlayer();    // +0x98 0x5e05b
    virtual bool Virtual9C();               // +0x9c 0x27c0b4 (`xor al, al; ret`)
    // +0xa0 0x5e0c4 (ret 8): the centre of inUnits' units' bounds (EEGetSelectionBounds), (0, 0) without units
    virtual void GetUnitsCenter(EETileLocation& outTile, std::vector<EECPAttackSubGroupUnit*>& inUnits);
    virtual long GetCargoSize();            // +0xa4 0x5e53e (the units' classes' mL258 summed)
    virtual float GetGuardChokepointPercentage();  // +0xa8 0xac7e2 (`fld dword ptr [0x838edc]; ret`)
    virtual float GetGuardHighGroundPercentage();  // +0xac 0xac7e2 (`fld dword ptr [0x838edc]; ret`)
    virtual float GetGuardContinentPercentage();  // +0xb0 0xac7e2 (`fld dword ptr [0x838edc]; ret`)
    virtual float GetGuardTownPercentage();  // +0xb4 0xac7e2 (`fld dword ptr [0x838edc]; ret`)
    virtual unsigned short GetMainContinent();  // +0xb8 0x5e1b6 (a continent id: EECPAttackGroup 0x551d6 passes it as one)
    virtual bool VirtualBC();               // +0xbc 0xac9f3 (`xor al, al; ret`)
    virtual bool AreAllUnitsSettled();      // +0xc0 0x5e354
    virtual bool HasNoTargets();            // +0xc4 0x5e424
    virtual bool CanReach(const EECPILocation* inLocation);    // +0xc8 0x5e4c9
    virtual bool IsAt(const EECPILocation& inLocation);         // +0xcc 0x5e430 (GetPathSubSector on it)
    virtual bool IsImportantUnit(EEComplexUnit* inUnit);  // +0xd0 0x5e4d7 (inUnit's sub group unit's mHasParentalVengeance)
    virtual EETileLocation GetPosition();   // +0xd4 0x5e513 (Virtual40(tile, true, false), the tile by value)
    virtual float GetDistanceLeftRatio();   // +0xd8 0x5e576
    virtual bool ShouldRefreshTask();       // +0xdc 0x5e5dd
    // +0xe0 0x5e656: push_backs the multimap's units into outUnits (ComputeLoadDistance passes a local vector)
    virtual void PickByUpgrade(std::vector<EECPAttackSubGroupUnit*>& outUnits, bool inFastestFirst, float inFraction);
    virtual bool GiveGoal(EEComplexUnit* inUnit, EEUnitGoal* inGoal, bool inKeepCurrent, long inAction);  // +0xe4 0x5e7f9
    virtual void SetUnitsState(long inAIState);  // +0xe8 0x5e852
    virtual void SetState(long inArg1, long inArg2);  // +0xec 0x60318
    virtual bool IsTaskActive(const EECPAGTask& inTask); // +0xf0 0x607e2 (`mov eax, dword ptr [esp + 4]; mov al, byte p`)
    // +0xf4 0x5eb96: the sub group unit that inUnit's EEUGAttackUnit goal (mL1EC) targets, 0 if none
    virtual EECPAttackSubGroupUnit* FindUnitTargetedBy(EEComplexUnit* inUnit);
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0xf8 0x5ebf9
    virtual void SetDestination(const EECPILocation& inLocation);  // +0xfc 0x5f14a
    // +0x100 0x5f16a
    virtual void SendUnitTo(EECPAttackSubGroupUnit* inUnit, const EECPILocation& inLocation, bool inFlag);
    virtual void RefreshTaskLocation();     // +0x104 0x5f233
    virtual void ReleaseUnits();            // +0x108 0x607ec
    // +0x10c 0x5f536: releases the sub group units of inUnits' units
    virtual void RemoveUnits(std::vector<EEComplexUnit*>& inUnits);
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion); // +0x110 0x5f622
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x114 0x5f895
    virtual void Virtual118(long inArg1);   // +0x118 0x157269 (`ret 4`)
    virtual void ReturnToRallyPoint();      // +0x11c 0x5fbcf
    virtual bool Virtual120();              // +0x120 0x6025e
    virtual bool IsAwayFromTowns();         // +0x124 0x5ff6c
    virtual bool ReplaceUnit(EEComplexUnit* inUnit, EEComplexUnit* inNewUnit);  // +0x128 0x609dd
    virtual bool TestCondition(long inArg1, long inArg2); // +0x12c 0x604cf
    virtual bool RemoveUnit(EEComplexUnit* inUnit); // +0x130 0x60b67 (reads inUnit's id +0x24)
    virtual bool CheckTask(long inTime);    // +0x134 0x60dd4 (EECPASGAir::TestCondition returns its al)
    virtual void Virtual138(long inArg1);   // +0x138 0x157269 (`ret 4`)
    virtual void ReturnToTask(long inTime);  // +0x13c 0x5b5a5
    virtual void RequestPathToTask();       // +0x140 0xe123d (`ret`)
    virtual bool RegroupHere();             // +0x144 0x5c991
    virtual float RollAggression();         // +0x148 0x5cb74
    virtual void RankUnits();               // +0x14c 0x5c5e2
    virtual void PlaceFormation(const EECPILocation& inLocation);  // +0x150 0x5d8c3 (distance to its tile +4)
    virtual bool TakeRequestedUnits();      // +0x154 0x5d92f
    virtual void CollectTargets(EEAIAStarBounds* inBounds, bool inIncludeEnemies) = 0; // +0x158 _purecall (0x5da23 pushes &bounds, inTime)
    virtual void HoldAtTask(unsigned long inTime);   // +0x15c 0x602b9 (no value at ret)
    virtual void RequestRemoval();          // +0x160 0x5e890
    virtual void CountTargetNeighbors();    // +0x164 0x6072e
    virtual void Update(long inTime);       // +0x168 0x607d0
    virtual bool DecideSplit();             // +0x16c 0x5e8d2
    virtual void MoveToDestination();       // +0x170 0x5ef56
    virtual bool AssignTargets();           // +0x174 0x5f06b
    virtual void RequestPath(void* inFrom, void* inTo) = 0;   // +0x178 _purecall
    virtual void ApplyPathResult();         // +0x17c 0x5f272
    virtual void RefreshUnits();            // +0x180 0x5f3ca
    virtual void PrunePath();               // +0x184 0xe123d (`ret`)
    virtual bool RemoveUnitByID(long inUnitID);  // +0x188 0x5f423
    virtual void Virtual18C(long inArg1);   // +0x18c 0x5f855
    virtual void FollowTaskUnit(long inArg1);  // +0x190 0x6087f
    virtual void SetPath(const std::vector<EECPASGPathPoint>& inPath);    // +0x194 0x5fd38
    virtual void AssignPrejudices();        // +0x198 0x1720c6 (`ret`)
    virtual void UpdateCommonMoveFlags();   // +0x19c 0x5fd81
    virtual bool Virtual1A0(long inArg1);   // +0x1a0 0xbb118 (`xor al, al; ret 4`)
    virtual bool HasReachedTerritory(long inTime);  // +0x1a4 0x5fe93
    virtual bool HasReachedLocation(long inTime);  // +0x1a8 0x60140
    virtual bool Virtual1AC(long inArg1);   // +0x1ac 0xbb118 (`xor al, al; ret 4`)
    virtual bool HasTaskUnitMoved(long inTime);  // +0x1b0 0x601e3
    virtual bool Virtual1B4(long inArg1);   // +0x1b4 0xbb118 (`xor al, al; ret 4`)
    virtual bool CheckTaskArrival(long inArg1);  // +0x1b8 0x60a27
    virtual void UpdateUnits(long inArg1);  // +0x1bc 0x60d85
    virtual void DropLostTargets(long inTime);  // +0x1c0 0x60ba9
    virtual bool IsTerritoryTaskValid(long inTime);  // +0x1c4 0x60e65
    virtual bool Virtual1C8(long inArg1);   // +0x1c8 0xb3257 (`mov al, 1; ret 4`)
    virtual bool IsLocationTaskValid(long inTime);  // +0x1cc 0x6111a
    virtual bool Virtual1D0(long inArg1);   // +0x1d0 0xbb118 (`xor al, al; ret 4`)
    virtual bool IsUnitTaskValid(long inTime);  // +0x1d4 0x4343c
    virtual void Virtual1D8(unsigned long inTime); // +0x1d8 0x6131a

    // 0x5e01b: the unit of mUnits whose mUnit is inUnit (searched from the back), 0 if none (provisional name)
    EECPAttackSubGroupUnit* FindUnit(EEComplexUnit* inUnit);
    void CountTargets();
    // 0x5e2c8 (ret 4; EECPAttackGroup::CollectUnits): the units not done and alive into outUnits (provisional name)
    void CollectUnits(std::vector<EEComplexUnit*>& outUnits);
    // 0x5e328: the intelligence file of mTask's player (EECPASGLand 0x4aa22) (provisional name)
    EECPIntelligencePlayerFile* GetTargetPlayerFile();
    // 0x5d865 (ret 8; EECPAttackGroup::ListUnit): this into outGroups once per unit of inUnit; returns outGroups.size()
    long ListIfHasUnit(EEComplexUnit* inUnit, std::vector<EECPAttackSubGroup*>& outGroups);
    // 0x5d08f / 0x5d0b3 / 0x5d0cb (cdecl; the derived kinds call them): a pooled target for inUnit (mL0C = mL10 = 0),
    // a target given back (Clear, then the pool), the pool's stats and release (provisional names)
    static EECPASTargetUnit* NewTarget(EEComplexUnit* inUnit);
    static void ReleaseTarget(EECPASTargetUnit* inTarget);
    static void ReleaseTargetPool();                    // 0x5b3ac: mCounts from mTargets' units (Virtual90) (provisional name)
    // 0x4e5c88: the targets' pool. A class static: its `$E` chain 0x45ac57 registers a GUARDED dtor (0x45ac77, bit 1
    // of 0x4e5c80), as for class statics (EECPBuildManager::sData), not a file-scope global; the users are this
    // class's static target functions and SetAttackTargetPoolSize. Name is provisional.
    static UMemoryPool<EECPASTargetUnit> sTargetPool;

    // 0x6a9b / 0x6aae / 0x6ad7 (EECPASGUMechPriest.cpp's range holds their COMDATs; the real header has them inline,
    // the exe never expands them; provisional names): mUnits.size(), and the bounds-checked
    // mUnits[inIndex]->mUnit / mUnits[inIndex] (0 when out of range)
    unsigned long GetNumUnits() const { return this->mUnits.size(); }  // inline (out-of-line COMDAT 0x6a9b, 47 callers)
    EEComplexUnit* GetUnitUnit(unsigned long inIndex) const;
    EECPAttackSubGroupUnit* GetUnitAt(unsigned long inIndex) const;
    // 0x4bc40 (EECPASGLand.cpp's range; called by EECPASGLand 0x4bb7a/0x4bf28 and this class 0x60188/0x60212): no path
    // request, or one not flagged at +0x4c (inline in the real header?; declared out of line here) (provisional name)
    bool IsPathPending();                   // (`push 1; pop eax`: a bool from an || expression)

    // inline: the attack group orders save it through a register (0x414c6)
    unsigned long GetID() const { return this->mID; }

    // 0x50e0d: original-backed destination selection, called by SeaTransport 0x4e829.
    void ScatterDestination();

    std::map<long, long> mU08;              // +0x08 (node 0x18: an 8-byte value, no element dtor; _Init 0x617b6)
    std::map<long, long> mU14;              // +0x14
    std::vector<long> mCommonMoveFlags;     // +0x20 (slot +0x19c clears it, then push_back 4 / 3)
    EECPASTargetUnit* GetTargetsAt(unsigned long inIndex) const { return this->mTargets[inIndex]; }
    std::vector<EECPASTargetUnit*> mTargets;        // +0x30 (slot +0x38 gives each back to the pool: 0x5d0b3)
    std::vector<EECPASTargetUnit*> mSiegeTargets;   // +0x40 (the same)
    // +0x50: the units (Load: 0x5661e makes each by its saved kind, then its Load; Save writes their mType first)
    EECPAttackSubGroupUnit* GetUnitsAt(unsigned long inIndex) const { return this->mUnits[inIndex]; }
    std::vector<EECPAttackSubGroupUnit*> mUnits;
    std::deque<EECPASGPathPoint> mPath;     // +0x60 (dtor 0x2003a4; slot +0x194 refills it; saved: count, points)
    // +0x74: U2DRectangle<long>-shaped (the ctor's +0xc/+4/+0/+8 zero stores; Load 0x12e72 = U2DRectangle<long>::
    // Load's shape, Save 0x12ec4, the centre 0x61340), but this unit has none of U2DRectangle.h's key chains
    EETileArea mArea;
    EECPASGStats mStats;                    // +0x84 (slot +0x14c: mStats.Compute(mUnits))
    EECPILocation mNextWaypoint;            // +0xcc (Load/Restore/Save)
    EECPILocation mRallyPoint;              // +0xe4 (Load/Restore/Save; its tile is copied into mPosition)
    // +0xfc (Load/Restore/Save through its slots; the dtor expands its inline dtor). The ASGU kinds pass mTask's
    // location (+0x100) to slot +0x100 when mTask.mPlayer (+0x12c) is set; the ctor sets mTask.mPlayer (+0x11c).
    EECPAGTask mTask;
    EECPASGUnitCounts mCounts;              // +0x130
    EECPASGFlags mFlags;                    // +0x164 (slot +0x50 Ands the units' flags into it)
    EETileLocation mPosition;               // +0x17c (zeroed before the vptr: an inline member ctor)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x184 (the commander's +0x18; its +0x9cc is EEPlayer::mTechnologies)
    EECPAttackFormation* mFormation;        // +0x188 (new 0x1c bytes, 0x529e7(this); deleted through its vptr)
    // +0x18c (referenced; saved as its id +0x3c, restored through the commander's +0x1c manager 0x5f82b, released
    // through 0x86e3c; the type is inferred: EECPPersonnelRequest > UReferenceCounter, sizeof 0x40)
    EECPPersonnelRequest* GetRequest() const { return this->mRequest; }
    EECPPersonnelRequest* mRequest;
    EECPAttackGroup* mGroup;                // +0x190 (the ctor's first argument)
    EEComputerPlayer* GetPlayer2() const { return this->mPlayer2; }
    EEComputerPlayer* mPlayer2;             // +0x194 (the commander's +0x18 again; slot +0x5c copies it)
    // +0x198 (referenced; gPathManager 0xa896b releases it unless its +0x4c flag is set; the type is inferred:
    // EEAutoPathRequest > UReferenceCounter, sizeof 0x50)
    EEAutoPathRequest* mPathRequest;
    long mScanRange;                        // +0x19c (= 0xd; saved)
    // inline: WriteDetailsHook 0x45cc2d / Virtual14 0x45b4d7 copy it through fld/fstp (a float accessor value)
    float GetAggression() const { return this->mAggression; }
    void SetAggression(float inValue) { this->mAggression = inValue; }
    float mAggression;                      // +0x1a0 (= 0.5f; saved)
    unsigned long mID;                      // +0x1a4 (the counter's value; saved; the attack group orders save it)
    unsigned long mNextScanTime;            // +0x1a8 (saved)
    unsigned long mNextUpdateTime;          // +0x1ac (saved)
    unsigned long mU1B0;                    // +0x1b0 (saved)
    unsigned long mU1B4;                    // +0x1b4 (saved; slot +0x1d8: inTime + a gPhysics random 3000..22000)
    unsigned long mNextFollowTime;          // +0x1b8 (saved)
    unsigned long mU1BC;                    // +0x1bc (saved)
    unsigned long mNextTaskCheckTime;       // +0x1c0 (saved)
    unsigned long mNextPositionTime;        // +0x1c4 (compared with gPhysics's time +0xd0)
    unsigned long mGatherDeadline;          // +0x1c8 (Restore: gPhysics's time + 10000 when 0 and mState == 1001)
    long mNumSplits;                        // +0x1cc (saved)
    long mSplitKind;                        // +0x1d0 (saved; slot +0x94 tests 4)
    unsigned long mState;                   // +0x1d4 (saved; 1001, 3004, ...; Restore: into each unit's mUnit +0x330)
    long GetKind() const { return this->mKind; }   // by value: EEAIFOSubGroupFilter 0x42a66d copies it to a temp ([ebp+8]) before set::find's const long&
    long mKind;                             // +0x1d8 (the ctor's second argument; saved)
    void SetMission(long inMission) { this->mMission = inMission; }
    long GetMission() const { return this->mMission; }
    long mMission;                          // +0x1dc (saved)
    bool mB1E0;                             // +0x1e0 (saved)
    bool mB1E1;                             // +0x1e1 (saved; slot +0x5c copies +0x1e1..+0x1e3)
    bool mB1E2;                             // +0x1e2 (saved)
    bool mIsOutOfFight;                     // +0x1e3 (saved)
    bool mHasAreaEffect;                    // +0x1e4 (slot +0x4c clears it first)
    bool mCanAttackShore;                   // +0x1e5 (= true)
};

// Attack subgroup conditions (EECPAttackSubGroup::TestCondition 0x4604cf and the EECPASG* overrides; a transport tests
// its cargo group where the base tests its task). Names are provisional (by behaviour).
enum EECPASGCondition {
    kASGConditionAlways = 0,                   // TestCondition 0x4604cf case 0: true (provisional name)
    kASGConditionRequestFilled = 1000,         // IsRequestFilled (provisional name)
    kASGConditionGathered = 1001,              // HasGathered (provisional name)
    kASGConditionTaskValid = 1002,             // CheckTask 0x460dd4 (transports: the cargo group has units) (provisional name)
    kASGConditionAtRallyPoint = 1003,          // no units or IsAt(mRallyPoint); tested in 6007 (return to the rally point) before removal (provisional name)
    kASGConditionPathReady = 1004,             // EECPASGLand::TestCondition: no path pending (the request's result) (provisional name)
    kASGConditionAllUnitsSettled = 1006,       // EECPASGLand: AreAllUnitsSettled (provisional name)
    kASGConditionHasTargets = 1008,            // GetNumTargets() > 0 (provisional name)
    kASGConditionHasUnits = 1015,              // mUnits.size() != 0; false -> 1000 wait for units (provisional name)
    kASGConditionTaskInOtherSubSector = 1018,  // Virtual120 0x46025e: the task location's path sub sector differs from the subgroup's (provisional name)
    kASGConditionPathEmpty = 1019,             // EECPASGLand: mPath.empty() (provisional name)
    kASGConditionShouldSplit = 1020,           // units and DecideSplit (provisional name)
    kASGConditionRequestEmpty = 1021,          // mRequest && mRequest->IsEmpty(); -> 1002 remove (provisional name)
    kASGConditionAwayFromTowns = 1023,         // IsAwayFromTowns; 3000 -> 3004 retreat (provisional name)
    kASGConditionNoTargets = 2001,             // EECPASGAir::TestCondition: GetNumTargets() == 0 (provisional name)
    kASGConditionTaskInvalid = 2002,           // EECPASGAir: !CheckTask; transports: no cargo group (mLand == NULL) (provisional name)
    kASGConditionHasUnitTask = 3003,           // an active task of type 7 with a task unit (provisional name)
    kASGConditionHasLocationTask = 3007,       // an active task of type 1, 10 or 11 (provisional name)
    kASGConditionShouldReturn = 3009,          // no units (transports: or no cargo group) or an active task of type 8; -> 6007 / 1002 (provisional name)
    kASGConditionShouldRefreshTask = 3010,     // ShouldRefreshTask; 6002 -> 6008 (provisional name)
    kASGConditionTerritoryTaskInvalid = 3011,  // !IsTerritoryTaskValid; 10000 -> idle (provisional name)
    kASGConditionAllAboard = 4000,             // CountUnitsAboard() == the unit count (transports: of the cargo group) (provisional name)
    kASGConditionAllUnloaded = 4002,           // CountUnitsAboard() == 0 (transports: or no cargo group; after unloading, mCargoStage 2) (provisional name)
    kASGConditionHasCargo = 4003,              // transports: the cargo group has units (provisional name)
    kASGConditionAnyAboard = 4004,             // EECPASGLand: CountUnitsAboard() != 0 (transports: false) (provisional name)
    kASGConditionHasTransport = 4005,          // EECPASGLand: a transport subgroup that is still requested (no units, a request) or has fighting units (provisional name)
    kASGConditionTransportRequestFilled = 4008,  // EECPASGLand: the transport subgroup's request is filled (provisional name)
    kASGConditionUnloadPointChosen = 4010,     // EECPASGLand: ChooseUnloadPoint (provisional name)
    kASGConditionTransportUnitsWaiting = 5001,  // EECPASGLand: AreTransportUnitsWaiting (then chooses an unload point) (provisional name)
    kASGConditionTargetsAtNextSubSector = 5002,  // EECPASGLand: HasTargetsAtNextSubSector (provisional name)
    kASGConditionOffMap = 5003,                // EECPASGLand: position x == -10000 (provisional name)
    kASGConditionNoneCarriable = 5004,          // EECPASGLand: CountUnitsNotCarriable() == the unit count: none can be carried (provisional name)
    kASGConditionLoadingBlocked = 5005,        // sea/space transports: IsLoadingBlocked; 8000 -> 9003 (provisional name)
    kASGConditionUnitsSettled = 5006,          // EECPASGLand: AreUnitsSettled (provisional name)
    kASGConditionNextSubSectorValid = 5007,    // EECPASGLand: CheckNextSubSector (provisional name)
    kASGConditionNight = 10000,                // gWorld.mTimeOfDay >= 0.5 (provisional name)
    kASGConditionUnseenByTaskPlayer = 10001,   // IsUnseenByTaskPlayer (provisional name)
    kASGConditionAtTask = 10004,               // IsAt(mTask.mLocation) (provisional name)
};

// Attack subgroup kinds (EECPAttackSubGroup::mKind): EECPAttackGroup::GetSubGroupKind 0x456bee picks one per unit class; MakeSubGroups 0x455dbd / Load 0x456cc8 create the EECPASG* class of the kind (names: those RTTI classes). 7 (sea, unit type 0xb6) and 8 (helicopter, unit type 0x294) are second kinds of the same classes (unnamed).
enum EECPASGKind {
    kASGKindLand = 0,           // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 0: new EECPASGLand (RTTI)
    kASGKindSea = 1,            // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 1: new EECPASGSea (RTTI)
    kASGKindAir = 2,            // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 2: new EECPASGAir (RTTI)
    kASGKindSeaTransport = 3,   // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 3: new EECPASGSeaTransport (RTTI)
    kASGKindAirTransport = 4,   // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 4: new EECPASGAirTransport (RTTI)
    kASGKindHelicopter = 5,     // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 5: new EECPASGHelicopter (RTTI)
    kASGKindSubmarine = 6,      // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 6: new EECPASGSubmarine (RTTI)
    kASGKindSpaceship = 9,      // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 9: new EECPASGSpaceship (RTTI)
    kASGKindSpaceTransport = 10,  // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 10: new EECPASGSpaceTransport (RTTI)
    kASGKindSpaceFighter = 11,  // EECPAttackGroup::MakeSubGroups 0x455dbd / Load 0x456cc8 case 11: new EECPASGSpaceFighter (RTTI)
};

// Attack subgroup states (EECPAttackSubGroup::mState): SetState 0x460318 and the EECPASG* overrides run the entry
// action, each subclass Update switches on it; numbered by phase (1000 group, 2000 move/transport, 3000 combat, 6000
// task arrival, 8000 transport cargo). Names are provisional (by behaviour).
enum EECPASGState {
    kASGStateIdle = 0,                  // SetState 0x460318 case 0: an idle subgroup with units asks its group for a task (AssignTask); Update case 0 decides the next state (provisional name)
    kASGStateWaitForUnits = 1000,       // entered when the subgroup has no units (condition 1015 false); leaves when its request is filled (-> 1001) or empty (-> 1002) (provisional name)
    kASGStateGather = 1001,             // SetState 0x460318: TakeRequestedUnits and a 30 s gather deadline; -> idle when HasGathered (provisional name)
    kASGStateRemove = 1002,             // SetState 0x460318: RequestRemoval (the group removes the subgroup) (provisional name)
    kASGStatePrepareTravel = 1004,      // entered when the task lies in another path sub sector (condition 1018, Virtual120 0x46025e); EECPASGLand::Update 0x4488ce -> 2000 (request a path) while the task is valid; EECPAttackGroup::ExecuteOrder 0x457727 order 5 (provisional name)
    kASGStateSplit = 1006,              // SetState: RequestSplit(true) (a split order plus a set-state order); waits for condition 0, then idle (provisional name)
    kASGStateSplitOff = 1007,           // EECPASGLand::SetState 0x44862a: RequestSplit(false) (the split order only); entered from 2005 when condition 4007 fails (provisional name)
    kASGStateRequestPath = 2000,        // EECPASGLand::SetState 0x44862a: RequestPathToTask (provisional name)
    kASGStateMove = 2001,               // SetState 0x460318 and the sea, space and transport overrides: MoveToDestination (provisional name)
    kASGStateClearSubSector = 2002,     // EECPASGLand::Update 0x4488ce: entered when the next path sub sector holds targets; scans every update while condition 5002 (HasTargetsAtNextSubSector) holds (provisional name)
    kASGStateNextWaypoint = 2003,       // EECPASGLand::SetState 0x44862a: pops the path's front tile into mNextWaypoint; -> 2001 move (provisional name)
    kASGStateApproachTask = 2004,       // EECPASGLand::Update 0x4488ce: entered at the end of the path; checks task arrival (CheckTaskArrival 0x460a27) (provisional name)
    kASGStateBoardTransport = 2005,     // EECPASGLand::SetState 0x44862a: StartTransportLoading (provisional name)
    kASGStateInTransit = 2006,          // EECPASGLand::SetState 0x44862a: aboard the transport; chooses the unload point and sends the transport there (mTransportStage 2) (provisional name)
    kASGStateUnload = 2007,             // EECPASGLand::Update 0x4488ce: entered when the unload point is chosen (conditions 4010/5001); -> idle when all are unloaded (provisional name)
    kASGStateAttack = 3000,             // entered whenever the subgroup has targets (condition 1008); -> idle when none are left; EECPASGLand::SetState -> 3004 when away from towns (provisional name)
    kASGStateAttackWhileBoarding = 3003,  // EECPASGLand::Update 0x4488ce: targets while waiting for or boarding the transport; -> 2005 when none are left (provisional name)
    kASGStateRetreatToTown = 3004,      // SetState 0x460318: MoveToNearestTown + ClearTargets (provisional name)
    kASGStateLaunch = 4001,             // EECPASGAir::SetState 0x44249f / EECPASGSpaceFighter: Launch; the transports enter it from idle with a cargo group and go on to 8000 (provisional name)
    kASGStateHoldAtTask = 6002,         // CheckTaskArrival 0x460a27: location tasks (types 1, 10, 11) on arrival; SetState: HoldAtTask (provisional name)
    kASGStateFollowTaskUnit = 6006,     // CheckTaskArrival 0x460a27: unit tasks (types 7, 12) when the task unit moved; SetState: FollowTaskUnit (provisional name)
    kASGStateReturnToRallyPoint = 6007,  // CheckTaskArrival 0x460a27: task type 8; SetState 0x460318: ReturnToRallyPoint; ExecuteOrder 0x457727 order 4 (provisional name)
    kASGStateRepositionAtTask = 6008,   // SetState 0x460318: RandomizeTaskLocation + SetDestination (entered from 6002 when ShouldRefreshTask) (provisional name)
    kASGStateLoad = 8000,               // transports (EECPASGSeaTransport::SetState 0x44e829 etc.): LoadCargo; EECPASGLand: waits for its transport (condition 4005) and boards when the transport's request is filled (4008 -> 2005) (provisional name)
    kASGStateCarryCargo = 8001,         // transports: entered when all cargo is aboard (condition 4000); SetState puts the land subgroup in 2006 (in transit) (provisional name)
    kASGStateReleaseCargo = 8002,       // transports: entered when all cargo is unloaded (condition 4002); SetState releases the land subgroup (provisional name)
    kASGStateRedirectCargo = 8003,      // EECPASGLand::Virtual248 puts the transport in it after choosing a new unload point (FindCrossingTile -> mUnloadLocation); the transports' Update handles it like 8001; EECPASGLand::SetState 0x44862a re-sends it on entering 2006; GetDangerRatio skips it (provisional name)
    kASGStateNoPath = 9001,             // EECPASGLand::RequestPathToTask stores it when no path can be requested and there is no transport (with a transport: 2006); Update 0x4488ce returns to idle unless the group is off the map (condition 5003) (provisional name)
    kASGStateSplitStragglers = 9002,    // EECPASGLand::SetState 0x44862a: mSplitKind 11 + RequestSplit(false); entered in transit when neither all are aboard nor all unloaded (provisional name)
    kASGStateScatter = 9003,            // sea/space transports: entered when loading is blocked (condition 5005); SetState: ScatterDestination (provisional name)
    kASGStateHoldTerritory = 10000,     // CheckTaskArrival 0x460a27: territory task (type 2) on arrival; leaves when the territory task is no longer valid (condition 3011) (provisional name)
};

#endif
