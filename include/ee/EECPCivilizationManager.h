// C:\EEX\Empire Earth\AI Thread\EECPCivilizationManager.h (RECONSTRUCTION; file name inferred). A computer player's
// civilization manager (RTTI EECPCivilizationManager, no base; vtable 0x43c068, 2 slots: ??_G 0x6f0e3 and Virtual04
// 0x702d4). EECPCivilizationManager.cpp 0x6e8d4..0x71c7f: the key
// "EECPCivilizationManager-Data" (guarded chain 0x6e8d4), ctor 0x6e917 (three maps, a vector, mPlayer = the argument,
// then it news its five managers), the members in address order, the map/sort COMDATs last. Its ESDebugOut strings
// name AttemptEpochAdvance, SetNextEpochAdvanceTime and Update; the other names are provisional. sizeof 0x84 (Load and
// Save end with +0x80). Requires <map>, <vector>, lle/LowLevelEngine.h (UString), lle/FSFile.h.
#ifndef EE_EECPCIVILIZATIONMANAGER_H
#define EE_EECPCIVILIZATIONMANAGER_H

#include <map>
#include <vector>
#include "ee/EETileCoordinate.h"

class EEComplexUnit;
class EEComputerPlayer;
class EECPBuildManager;
class EECPResearchManager;
class EECPResourceManager;
class EECPUnitManager;
class EECPWallManager;
class EETechTreeNode;
class FSFile;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;

struct EEPUnitSwap;                         // ee/EEPhysics.h (Virtual04's vector: EEComputerPlayer::Virtual9C passes its own)

struct EECPUMRequest;                       // ee/EECPUnitManager.h

class EECPCivilizationManager {
public:
    EECPCivilizationManager(EEComputerPlayer* inPlayer);   // 0x6e917
    virtual ~EECPCivilizationManager();     // +0x00 0x6f0ff (??_G 0x6f0e3)
    // +0x04 0x702d4 (ret 4): replaced units (EEComputerPlayer::Virtual9C's swaps) that are mGarrison/mFortress release
    // those, then the resource and wall managers get the list (provisional name)
    virtual void UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps);

    // 0x6f18d (ret 8): adds inDelta to the count of inUnitType (and its groups); true if a count changed
    bool AdjustUnitCount(long inUnitType, long inDelta);
    void AttemptEpochAdvance(unsigned long inTime);   // 0x6f3bc (named by its ESDebugOut strings)
    void FillGarrison(EEComplexUnit* inUnit);  // 0x6f4d4: fills inUnit (a fortress) with garrison units, or orders them
    void CheckBuildingCounts();             // 0x6f7b8 (named by Update's ESDebugOut string)
    void QueueMoraleBuilding();             // 0x6fb5e
    long ChooseTransportClass();            // 0x6fbe2 (returns GetTransportClass's result: EECPResourceManager 0x8ecfd uses it)
    long GetAntiAirClass();                 // 0x6fd9c: the unit type for the epoch (0x91/0x93/0x300)
    long GetFishingBoatClass();             // 0x6fdd3: likewise (0x6c/0x6d/0xd3)
    long GetGateClass();                    // 0x6fe05: likewise (a switch on the epoch)
    long GetTowerClass();                   // 0x6fe86: likewise
    long GetTransportClass(bool inSea, bool inAir, bool inSpace);   // 0x6ff07 (ret 0xc)
    long GetWallClass();                    // 0x6ffdc: likewise
    long GetMaxEpoch();                     // 0x7005a: the highest epoch of the other players (ecx unused)
    bool IsBehindInEpoch();                 // 0x700a9: GetMaxEpoch() is ahead of ours
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x700cb
    // 0x702b6 (ret 8): inAmount < the player's mResourceCaps[inResource]
    bool IsBelowResourceCap(long inResource, long inAmount);
    void Reset();                           // 0x7037c: the managers' resets, then the epoch flags
    void RepairBuildings();                 // 0x703b4: sends workers to repair damaged buildings
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x70765
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x708e7
    void SetNextEpochAdvanceTime(unsigned long inTime, bool inActual);   // 0x70b04 (named by its ESDebugOut strings)
    void ReleaseGarrison();                 // 0x70bf6: releases mGarrison's garrison
    void ReleaseFortress();                 // 0x70c9d: releases mFortress's garrison
    void UnitAdded(EEComplexUnit* inUnit);  // 0x70d44: a new unit
    void UnitLost(EEComplexUnit* inUnit, bool inSwapped);  // 0x70ed1: a lost unit
    void Update(unsigned long inTime);      // 0x70f52 (named by its ESDebugOut string)
    void ChooseGarrison();                  // 0x711ac: picks a town center to garrison (mGarrison)
    void ChooseFortress();                  // 0x714c2: picks a fortress to garrison (mFortress)

    EECPUnitManager* GetUnitManager() { return this->mUnitManager; }  // inline (ChooseResearch loads it before the argument call) (provisional name)
    EECPBuildManager* GetBuildManager() { return this->mBuildManager; }  // inline (provisional name)
    EECPResourceManager* GetResourceManager() { return this->mResourceManager; }  // inline (provisional name)

    // 0x4e5d18 (guard byte 0x4e5d10 bit 1). Only EECPCivilizationManager.cpp sees it: its $E number would renumber the
    // includers' chains (as EEButtonObject::sTable)
    static const UString sData;             // "EECPCivilizationManager-Data"

    std::map<long, long> mUnitCounts;       // +0x04 by unit type (the ctor zeroes 19 keys; AdjustUnitCount adds to them)
    std::map<long, long> mMaxCounts;        // +0x10 (the player's +0xdd0.. values; CheckBuildingCounts' cap)
    std::map<long, long> mUnitsPer;         // +0x1c (the player's +0xe8c.. values; CheckBuildingCounts divides by it)
    std::vector<EEComplexUnit*> mRepairing; // +0x28 (RepairBuildings copies it and erases the repaired target)
    EECPResourceManager* mResourceManager;  // +0x38 (new 0x18c, ctor 0x8e8f1)
    EECPBuildManager* mBuildManager;        // +0x3c (new 0x170, ctor 0x66c5d)
    EECPResearchManager* mResearchManager;  // +0x40 (new 0x12c, ctor 0x8a007)
    EECPUnitManager* mUnitManager;          // +0x44 (new 0x158, ctor 0x935e6)
    EECPWallManager* mWallManager;          // +0x48 (new 0x88, ctor 0x9cc24; provisional class name)
    EEComputerPlayer* GetPlayer() const { return this->mPlayer; }
    EEComputerPlayer* mPlayer;              // +0x4c (the ctor argument)
    EEComplexUnit* mGarrison;               // +0x50 referenced (saved as its unit id) (provisional name)
    EEComplexUnit* mFortress;               // +0x54 referenced (saved as its unit id) (provisional name)
    unsigned long mNextEpochAdvanceTime;    // +0x58 (saved relative to gPhysics' time)
    unsigned long mLastCountCheck;          // +0x5c (saved)
    unsigned long mNextRepairTime;          // +0x60
    unsigned long mNextGarrisonTime;        // +0x64 (saved relative)
    unsigned long mNextFortressTime;        // +0x68 (saved relative)
    unsigned long mNextMoraleBuildingTime;  // +0x6c (saved relative)
    unsigned long mNextAirportSiteTime;     // +0x70
    unsigned long mNextSpaceDockSiteTime;   // +0x74 (the ctor's init list)
    bool mBuildingCountsChecked;            // +0x78 the building counts were checked
    bool mFirstUpdateDone;                  // +0x79 (Update's first pass done)
    bool mEpochResearchRequested;           // +0x7a (AttemptEpochAdvance asked for research)
    bool mB7B;                              // +0x7b
    bool mAdvancingEpoch;                   // +0x7c (EECPResearchManager 0x8b24e/0x8aeaa clear it ...)
    bool mScheduleEpochAdvance;             // +0x7d (... and set this one)
    bool mEpochAdvanceEnabled;              // +0x7e
    bool mBuildingCountsEnabled;            // +0x7f
    bool mMoraleBuildingsEnabled;           // +0x80 (not set by the ctor)
};

// The managers' members other units call (their classes are not decoded yet; provisional names)
// EECPBuildManager: see ee/EECPBuildManager.h (moved there)
#include "ee/EECPBuildManager.h"

// EECPResourceCenter: see ee/EECPResourceCenter.h (moved there; it has a class static)
class EECPResourceCenter;

#endif
