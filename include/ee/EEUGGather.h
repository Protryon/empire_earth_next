// EEUGGather.h (RECONSTRUCTION; file name inferred: one header per goal class like ee/EEUGExplore.h; split out of
// ee/EEUnitGoal.h, no __FILE__ evidence either way). Requires lle/GE3DPoint.h (the members), ee/EEUnit.h (the inline
// Virtual70).
#ifndef EE_EEUGGATHER_H
#define EE_EEUGGATHER_H

#include "ee/EEUnitGoal.h"
#include "ee/EEUnitClassIDs.h"

class EEComplexUnit;
class EEUnit;
class FSFile;
class GE3DPoint;
class UWideString;

// Gathering a resource (RTTI EEUGGather > EEUnitGoal; vtable 0x4475a8; ctor 0x221e95: EEUnitGoal(6, inArg3), both
// units referenced; sizeof 0x44: `new 0x44` in EEMGatherResource::Execute; EEUGGather.cpp 0x221d98..0x22528d). Member
// names from its out-of-synch dump 0x2223aa; (provisional names)
class EEUGGather : public EEUnitGoal {
public:
    EEUGGather(EEUnit* inResource, EEUnit* inStorage, bool inArg3);   // 0x221e95
    // inline (COMDAT 0x23c63f after its first user, EEUnitGoal::CreateGoal)
    EEUGGather(bool inArg) : EEUnitGoal(kGoalGather, inArg), mResource(NULL), mStorage(NULL), mGathering(false), mWaiting(false) {}
    virtual ~EEUGGather();                  // +0x00 0x221f80 (??_G 0x221f64)
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation);  // +0x08 0x221fb5
    virtual EEUnitGoal* Clone();            // +0x10 0x22234a
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x14 0x2223aa
    virtual long GetOutOfSynchChecksum();   // +0x18 0x222489
    virtual void GetDebugLine(UWideString& ioText);  // +0x1c 0x222ce6
    virtual void GetDebugText(UWideString& ioText);  // +0x20 0x222d9b
    virtual void GetGoalLocation(GE3DPoint& outLocation);  // +0x28 0x222e3c (the waiting area or the destination)
    virtual long GetStartAction();          // +0x30 0x222e84 (0xfb7 gathering, 0xfa6/0x1f49 by the storage)
    virtual void Step(EEComplexUnit* inUnit);        // +0x40 0x222ea2
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x44 0x222f70
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);  // +0x50 0x223a18 (repairs the pointers)
    virtual void Start(EEComplexUnit* inUnit);       // +0x54 0x223a91
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x58 0x223b69
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent);  // +0x64 0x223d21
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*);  // +0x6c 0x2240dc
    virtual void Work(EEComplexUnit* inUnit);        // +0x74 0x22410b
    void SetResource(EEUnit* inResource);   // 0x223c3d (referenced) (provisional name)
    void SetStorage(EEUnit* inStorage);     // 0x223c5e (referenced) (provisional name)
    // Virtual40's command handlers (by command: 0xfa4, 0xfa6, 0xfa7/0xfb5, 0xfb7, 0xfb8, 0xfba, 0xfbd, 0x1f48, 0x1f49)
    // (provisional names)
    void BeginGathering(EEComplexUnit* inUnit);  // 0x222ae2 (0xfa4)
    void FindNewResource(EEComplexUnit* inUnit);  // 0x222500 (0xfa6)
    void FindStorage(EEComplexUnit* inUnit);  // 0x222612 (0xfa7, 0xfb5)
    void ArriveAtResource(EEComplexUnit* inUnit);  // 0x2234b7 (0xfb7)
    void FinishGathering(EEComplexUnit* inUnit);  // 0x22371b (0xfb8)
    void SearchNavalResource(EEComplexUnit* inUnit);  // 0x224250 (0xfba)
    void SendToNextFarm(EEComplexUnit* inUnit);  // 0x222593 (0xfbd)
    void GatherNextTree(EEComplexUnit* inUnit);  // 0x222ff0 (0x1f48)
    void BuildFarms(EEComplexUnit* inUnit);  // 0x22333b (0x1f49)
    bool IsAtStorage(EEComplexUnit* inUnit);  // 0x223c7f: inUnit within sqrt(2) storage radii of the storage (provisional name)
    EEUnit* FindUnfinishedFarm(EEComplexUnit* inUnit);  // 0x223675 (ret 4; EECPResourceManager::BuildSettlementFor tests it == 0) (provisional name)
    // (provisional names)
    bool IsAtResourceSpot(EEComplexUnit* inUnit);  // 0x222271 (Virtual64: inUnit next to the storage)
    virtual void PathTaken(EEComplexUnit* inUnit);   // +0x48 0x223966 (mB04: the path's end reached)
    bool IsAtResource(EEComplexUnit* inUnit);  // 0x223abf (Virtual64: at the resource)
    bool FindGatherSpot(EEComplexUnit* inUnit);  // 0x2226cf (0xfb7: a place to stand by the resource; EEComplexUnit 0x1fb9a0 too)
    bool HasGatherRoom(EEComplexUnit* inUnit, EEUnit* inResource);  // 0x223757 (0xfb7, Virtual64)
    bool IsResourceInReach(const GE3DPoint& inLocation, EEComplexUnit* inUnit);  // 0x223b44 (Virtual48 while gathering)
    bool IsStorageInReach(const GE3DPoint& inLocation, EEComplexUnit* inUnit);  // 0x223cbe (Virtual48 while dropping off)
    // inline: emitted after their first user, the ctor (0x221f29, 0x221f3f, 0x221f43), before the ??_G
    virtual bool IsActive(unsigned long* outValue)    // +0x0c 0x221f29 (5000 while not mB04)
    {
        if (!this->mIsActive)
            *outValue = 5000;
        return this->mIsActive;
    }
    virtual GE3DPoint* GetActualDestination() { return &this->mDestination; }  // +0x78 NEW
    virtual bool Suits(EEUnit* inUnit)      // +0x70 (resource categories 6 and 0x16)
    {
        long theCategory = inUnit->mUnitClass->mCategory;
        return theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen;
    }

    GE3DPoint mWaitingArea;                 // +0x20 "Waiting Area" (= mDestination after 0x11c5fb)
    GE3DPoint mDestination;                 // +0x2c "Actual Destination" (the map location of the target's tile)
    EEUnit* mResource;                      // +0x38 "Resource Target" (referenced)
    EEUnit* mStorage;                       // +0x3c "Storage Target" (referenced)
    bool mGathering;                        // +0x40 "Gathering Flag" (= false)
    bool mWaiting;                          // +0x41 "Waiting Flag" (= false)

    static const UString sData;             // "EEUGGather-Data" (init chain 0x221d98)
};

#endif
