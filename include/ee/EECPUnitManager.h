// C:\EEX\Empire Earth\AI Thread\EECPUnitManager.h (RECONSTRUCTION; file name inferred). A
// computer player's unit manager (no RTTI: not polymorphic; sizeof 0x158, new'd by EECPCivilizationManager's ctor
// 0x6e917, ctor 0x935e6). EECPUnitManager.cpp 0x9350f..0x979ff. Its first member is a map from unit class to the
// player's units of that class (0x94608/0x945de: find in the map at +0, end = its head +4). Names are provisional.
// Requires <map>, <deque>.
#ifndef EE_EECPUNITMANAGER_H
#define EE_EECPUNITMANAGER_H

#include <deque>
#include <map>
#include <vector>
#include "ee/EETileCoordinate.h"

class EEComplexUnit;
class EECPCivilizationManager;
class EECPUMGatherUnitsScreenBase;         // ee/EECPUMGatherUnitsScreenBase.h
class EEComputerPlayer;
class EEUnit;
class FSFile;
class GE2DPoint;
template <class T> class U2DPoint;
// The request FillGarrison hands EECPUnitManager::StartProduction (16 bytes built field by field on the stack;
// provisional names; moved here from ee/EECPCivilizationManager.h: mRequests holds them by value)
struct EECPUMRequest {
    unsigned long mUnitClass;               // +0x00
    unsigned long mCount;                   // +0x04
    long mTownID;                           // +0x08
    unsigned short mContinent;              // +0x0c
};

// Orders units by their tile's distance from mCenter (EECPCivilizationManager::RepairBuildings sorts its workers with
// it; the sort COMDATs 0x7168b.. call its operator() 0x93552, which lies in EECPUnitManager.cpp's range and was built
// without /Og like EETileDistanceLess's 0x21e104; its copy ctor is ICF-folded into EETileDistanceLess's 0x4d28b).
// Name and owner inferred. Requires ee/EETileCoordinate.h.
struct EECPUnitDistanceLess {
    EECPUnitDistanceLess(long inX, long inY) : mCenter(inX, inY) {}
    EECPUnitDistanceLess(const EECPUnitDistanceLess& inOther) : mCenter(inOther.mCenter) {}
    ~EECPUnitDistanceLess() {}
    bool operator()(EEComplexUnit* inFirst, EEComplexUnit* inSecond) const;

    EETileCoordinate mCenter;
};

class EECPUnitManager {
public:
    // save key (EECPUnitManager.cpp's `$E` chain 0x9350f, guard byte 0x4e5d88, object 0x4e5d90)
    static const UString sData;             // "EECPUnitManager-Data"
    unsigned long GetCount(long inUnitClass);   // 0x945de: GetUnits(inUnitClass)'s size, 0 if none (unsigned: ChooseResearch's seta)
    std::deque<EEComplexUnit*>* GetUnits(long inUnitClass);    // 0x94608: 0 if none
    void RequestFishingBoats(long inCount);  // 0x9487b (ret 4; EECPResourceManager::AssignGatherers) (provisional name)
    void RequestUnits(long inUnitClass, long inArg2, long inTownID, unsigned short inArg4);
    // 0x93ed4 (ret 4): the units, requests and upgrades to ioGatherer's slots (EECPDefenseCommander)
    void GatherUnits(EECPUMGatherUnitsScreenBase& ioGatherer);
    void BalanceProduction(EEComplexUnit* inUnit);  // 0x93b97 (ret 4; EEComputerPlayer::Virtual68 0x2e1e9) (provisional name)
    void AddAllUnits();                     // 0x9482b (EEComputerPlayer::SetActive: activated) (provisional name)
    void ReleaseUnits();                    // 0x93b20 (EEComputerPlayer::SetActive: deactivated) (provisional name)
    long CountMilitaryUnits(bool inByClass, bool inRequests);  // 0x945a7 (ret 8; EECPCombatManager::GetUnitsAvailable)
    // (EECPCivilizationManager.cpp's calls; ret N from the bodies; provisional names)
    EECPUnitManager(EECPCivilizationManager* inManager);
    ~EECPUnitManager();                     // 0x93660 (not virtual: the civilization manager calls it, then delete)
    bool IsRequested(long inUnitClass);
    bool IsInProduction(long inUnitClass);  // 0x936ec (ret 4; EECPResourceManager 0x8ecfd) (provisional name)
    long GetWantedCitizens();
    void GetIdleCitizens(std::vector<EEComplexUnit*>& outUnits);
    void GetCitizens(std::vector<EEComplexUnit*>& outUnits);
    bool IsCitizenAvailable(EEComplexUnit* inUnit);
    unsigned long GetNumWorkers();
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    bool HasLaterEpochRequests();
    void Reset();
    void DropObsoleteRequests(long inEpoch);
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    bool StartProduction(EECPUMRequest& inRequest, long& outMade);  // 0x94f42 (ret 8; returns al)
    void UnitAdded(EEUnit* inUnit, long inArg);  // 0x95440 (ret 8; reads the argument's +0x30 complex unit)
    void UnitLost(EEUnit* inUnit);          // 0x956b9 (ret 4; likewise)
    bool CancelRequest(long inUnitClass, unsigned long inCount, long inTownID, unsigned short inContinent);
    // EECPWallManager.cpp's calls (provisional names)
    EEComplexUnit* FindBuilder(GE2DPoint& inLocation, long inUnitClass);  // 0x941bb (ret 8): a builder for inLocation
    EEComplexUnit* FindBuilderForTile(U2DPoint<long>& inTile, long inUnitClass);  // 0x94174 (ret 8; returns the worker)
    // EECPBuildManager.cpp's calls (ret N from the bodies) (provisional names)
    bool HasUnfinished(long inUnitClass);
    EEComplexUnit* FindStalledBuilder();    // 0x93de4: the next idle builder (0 when none)
    EEComplexUnit* FindBuilderForTarget(EEUnit* inTarget, long inUnitClass);
    void Update(unsigned long inTime);

    std::map<long, std::deque<EEComplexUnit*>*> mUnits;   // +0x00
    EEEpochArray<std::deque<EECPUMRequest>> mRequests;   // +0x0c the requests by epoch (the original: 15)
    // +0x138 count per unit class (0x948e6: insert(pair(class, 0)).first->second += n; Load/Save read and write key and
    // value as unsigned long; its dtor COMDAT 0x95e70 is ~_Tree<unsigned long, unsigned long>)
    std::map<unsigned long, unsigned long> mWaitingCounts;
    EECPCivilizationManager* mManager;      // +0x144 (the ctor's argument)
    EEComputerPlayer* mPlayer;              // +0x148 (mManager->+0x4c)
    long mNumWaiting;                       // +0x14c
    long mNumInProduction;                  // +0x150
    bool mProductionEnabled;                // +0x154
    char _gap155[3];                        // to sizeof 0x158 (the civilization manager news 0x158)
};

#endif
