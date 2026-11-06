// C:\EEX\Empire Earth\AI Thread\EECPResourceManager.h (RECONSTRUCTION; file name inferred; moved out of
// ee/EECPCivilizationManager.h). A computer player's resource manager (RTTI EECPResourceManager, no base;
// vtable 0x43c28c: the dtor only; new 0x18c by EECPCivilizationManager's ctor).
// EECPResourceManager.cpp 0x8e702..0x9350f. Layout from the ctor 0x8e8f1, the dtor 0x8e9e8 and Load/Save;
// member names are provisional. Requires <vector>, <deque>, ee/EECPCivilizationManager.h.
#ifndef EE_EECPRESOURCEMANAGER_H
#define EE_EECPRESOURCEMANAGER_H

#include <deque>
#include <vector>

class EEComplexUnit;
class EEUnit;
class EECPCivilizationManager;
class EETechTreeNode;
class FSFile;
struct EEPUnitSwap;

class EECPResourceManager {
public:
    virtual ~EECPResourceManager();         // +0x00 0x8e9e8 (vtable 0x43c28c; ??_G 0x8e9cc)
    EECPResourceManager(EECPCivilizationManager* inManager);   // 0x8e8f1

    // the gatherer queues (provisional names)
    void AssignIdleGatherers(std::deque<EEComplexUnit*>& ioUnits);  // 0x8eb29 (ret 4)
    void AssignIdleBoats(std::deque<EEComplexUnit*>& ioUnits);  // 0x8ec34 (ret 4)
    void RequestFerry(std::vector<EEComplexUnit*>& inUnits, EEUnit* inResource);  // 0x8ecfd (ret 8): a transport for inUnits
    void AddGatherer(EEComplexUnit* inUnit, long inResource);   // 0x8ee67 (ret 8): referenced, once per queue
    void AssignGatherers();                 // 0x8eecf (Update's main pass, every 10 s)
    void CitizenLost(EEComplexUnit* inUnit);  // 0x8fca6 (ret 4)
    void AddIdleGatherer(EEComplexUnit* inUnit);  // 0x8fd15 (ret 4; EEComputerPlayer 0x3f121, EECPUnitManager 0x9486e)
    bool BuildSettlementFor(EEComplexUnit* inUnit);  // 0x8fd38 (EEUGGather::FindStorage: true keeps the gatherer) (provisional name)
    void RemoveGatherer(EEComplexUnit* inUnit);   // 0x9007f (ret 4): out of every queue, dereferenced
    void Spend(long inResource, unsigned long inAmount);   // 0x90105 (ret 8; EEComputerPlayer 0x2d1bf)
    void RequeueIdleGatherers();            // 0x90122
    void RefreshResources();                // 0x90195: the forests and mines by distance from the town center
    void AddCosts(EETechTreeNode* inNode, long inCount);  // 0x90406 (ret 8)
    void FindMines(unsigned long inCount, bool inNear, std::vector<EEComplexUnit*>& outUnits);  // 0x90458 (ret 0xc)
    EEUnit* FindFarm(bool inNeedsDropSite, bool inNear, EEComplexUnit* inUnit);  // 0x90602 (ret 0xc)
    EEUnit* FindFish(EEUnit* inUnit);       // 0x90819 (ret 4)
    void GetUnfinishedFarms(long inMax, std::vector<EEComplexUnit*>& outUnits);  // 0x909d7 (ret 8)
    void GetIdleFarms(long inMax, std::vector<EEComplexUnit*>& outUnits);  // 0x90a63 (ret 8)
    EEUnit* FindForestTarget(long inResource, EEComplexUnit* inUnit);  // 0x90af3 (ret 8)
    EEUnit* FindResourceTarget(long inResource, EEComplexUnit* inUnit, bool inArg);  // 0x90cb5 (ret 0xc)
    EEUnit* FindNearestDock(EEComplexUnit* inUnit); // 0x90ec6 (ret 4)
    EEComplexUnit* FindNearestTownCenter(EEComplexUnit* inUnit);  // 0x90f72 (ret 4): the nearest of the player's 0x2b/0x23/0x29 units
    void Earn(long inResource, unsigned long inAmount);   // 0x91037 (ret 8; EEComputerPlayer 0x3eea8)
    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // 0x91054
    bool IsSettlementQueued(long inArg);    // 0x911ea (ret 4)
    void Refresh();                         // 0x911fd (EEComputerPlayer 0x3ceb8): RefreshResources
    void UnitsSwapped(std::vector<EEPUnitSwap>& inSwaps);  // 0x91202 (ret 4) (provisional name)
    void SendToGather(EEComplexUnit* inUnit, EEUnit* inResource, EEUnit* inStorage);  // 0x912be (ret 0xc): sends inUnit to gather
    void Reset();                           // 0x91430
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);   // 0x9144d
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // 0x9152a
    bool FerryGatherers(std::vector<EEComplexUnit*>& inUnits, EEComplexUnit* inTransport, EEUnit* inResource);  // 0x916e6 (ret 0xc)
    void TransportIdle(EEComplexUnit* inUnit);  // 0x91965 (ret 4; EEComputerPlayer 0x3f072)
    void RemoveCosts(EETechTreeNode* inNode, long inCount);  // 0x91b26 (ret 8; EECPBuildManager 0x6b07d, EECPUnitManager 0x93acc): AddCosts's Spend twin
    bool UpdateWantedGatherers();           // 0x91b78: the wanted gatherers per resource; true if one changed
    // new (docs/AI_RESEARCH.md §8.8): the gatherers the held resource sites take (the active resource centers'
    // resource units x the mining limit; wood a fixed allowance), in all or for one resource
    unsigned long GetGatherCapacity(long inResource = -1);
    void RebalanceGatherers();              // new: frees a few gatherers of over-served resources for AssignGatherers
    void Update(unsigned long inTime);      // 0x91d34

    static const UString sData;             // "EECPResourceManager-Data" (0x4e5d78, guard 0x4e5d70; chain 0x8e702)

    std::vector<unsigned long> mDropSiteClasses[7];  // +0x04 per resource (Load/Save: a count, then Read/Write(unsigned long))
    std::vector<unsigned short> mForests;   // +0x74 (RefreshResources: the continent manager's forest ids)
    std::vector<EEComplexUnit*> mMines;     // +0x84 (referenced; nature's units of class 0x11)
    std::vector<EEComplexUnit*> mFish;      // +0x94 (referenced; nature's units of class 0x12)
    std::deque<EEComplexUnit*> mGatherers[7];   // +0xa4 per resource (referenced)
    EECPCivilizationManager* mManager;      // +0x130 (the ctor argument)
    long mNeeds[7];                         // +0x134 per resource (Earn/Spend; Read/Write(long))
    unsigned long mWantedGatherers[7];      // +0x150 per resource (Reset zeroes it)
    unsigned long mNextFishingBoatTime;     // +0x16c (not saved)
    unsigned long mTime170;                 // +0x170 (saved relative to gPhysics' time)
    unsigned long mNextWantedGatherersTime;  // +0x174
    unsigned long mNextAssignGatherersTime;  // +0x178
    unsigned long mNextResourceCheckTime;   // +0x17c
    bool mB180[7];                          // +0x180 per resource (Reset clears it)
    bool mInitialized;                      // +0x187 (Restore clears it)
    bool mSettlementsEnabled;               // +0x188
};

#endif
