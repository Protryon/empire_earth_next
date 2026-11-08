// EEUGBuildRepair.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class). Classes: EEUGBuildRepair. Layout
// proven: EEUGBuildRepair sizeof 0x5c = its new-site size, ctor 0x23c5e3 exact in EEUnitGoal.cpp. Member names are
// provisional.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEUGBUILDREPAIR_H
#define EE_EEUGBUILDREPAIR_H

#include "ee/EEUnitGoal.h"
#include "ee/EETechTreeTechID.h"

class EEUnit;
class EEUnitGoal;

class EEPlayer;
class EEUnitClass;
class EETechTreeNode;
class GE2DPoint;

// vtable 0x44740c; bases EEUGBuildRepair > EEUnitGoal; sizeof 0x5c; ctor 0x21e164 0x23c5e3; ??_G 0x21e2d6; dtor 0x21e2f2.
// Member names from its out-of-synch dump 0x21e985 (provisional name).
class EEUGBuildRepair : public EEUnitGoal {
public:
    // 0x21e164: inTarget's outermost carrier, referenced (provisional names)
    EEUGBuildRepair(EEPlayer* inPlayer, EEUnit* inTarget, bool inArg);
    // inline (COMDAT 0x23c5e3 after its first user, EEUnitGoal::CreateGoal)
    EEUGBuildRepair(bool inArg)
        : EEUnitGoal(kGoalBuildRepair, inArg), mPlayer(NULL), mTarget(NULL), mTargetClass(NULL), mTargetNode(NULL), mTargetType(0),
          mTargetScale(0), mFirstTickAmount(0), mWorkerCountIncremented(false), mFirstTickAdjusted(false)
    {
        this->mTechTreeID = kTechNone;
    }
    virtual ~EEUGBuildRepair();             // +0x00 dtor 0x21e2f2
    virtual void GetDestination(EEComplexUnit* inUnit, GE3DPoint& outLocation); // +0x08 0x21e5ea
    virtual EEUnitGoal* Clone();     // +0x10 0x21e955
    virtual void DumpOutOfSynch(FILE* inFile); // +0x14 0x21e985
    virtual long GetOutOfSynchChecksum(); // +0x18 0x21ea99
    virtual void GetDebugLine(UWideString& ioText); // +0x1c 0x21ef11
    virtual void GetDebugText(UWideString& ioText); // +0x20 0x21ef9e
    virtual void GetGoalLocation(GE3DPoint& outLocation); // +0x28 0x21d0b4 (ICF: EEUGAttackGround, EEUGAttackUnit...)
    virtual long GetStartAction(); // +0x30 0x21e298 trivial: mov eax, 0x1f51; ret
    virtual void Step(EEComplexUnit* inUnit);      // +0x40 0x21f09d
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile); // +0x44 0x21f177
    virtual void Abandon(EEComplexUnit* inUnit);     // +0x4c 0x21f1e4
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond); // +0x50 0x21f350 (repairs the pointers)
    virtual void Start(EEComplexUnit* inUnit);     // +0x54 0x21f451
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile); // +0x58 0x21f47f
    virtual bool EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent); // +0x64 0x21fc4b
    virtual void ReplaceUnit(EEComplexUnit* inOld, EEComplexUnit* inNew, EEComplexUnit*); // +0x6c 0x21feb2
    virtual bool Suits(EEUnit* inUnit);     // +0x70 0x221f43 (ICF: EEUGGather)
    virtual void Work(EEComplexUnit* inUnit);       // +0x74 0x21fefe
    // non-virtual members (provisional names)
    void SetTargetLocation(const GE2DPoint& inLocation);   // 0x21e29e: z = 0, then UpdateTargetCenter
    void UpdateTargetCenter();              // 0x21e2ba: the location plus half the target's scale
    bool IsAtBuildSite(EEComplexUnit* inUnit);  // 0x21e342
    // 0x21e680: the hit points inWorker adds this tick (inPreview: without the first-tick bookkeeping) (provisional
    // name)
    long ComputeBuildAmount(EEComplexUnit* inWorker, bool inPreview);
    bool FinishBuilding(EEComplexUnit* inUnit);  // 0x21e410 (the target is finished: research, effects, sounds) (provisional name)
    bool IsBuildSiteTaken();                // 0x21f4f5 (EECPUnitManager 0x93de4) (provisional name)
    bool IsFootprintClear();                // 0x21f5fc (provisional name)
    bool IsSiteFreeOfUnits();               // 0x21f85e (provisional name)
    void RetargetGatePart(EEComplexUnit* inUnit);  // 0x21e81f (category 0x19: Virtual6C with the unit's 0x246f93 result)
    bool IsGateClosed(EEComplexUnit* inUnit);  // 0x21f03d
    void AddBuildEvent();                   // 0x21f138: the player's event list (0x273a, or 0x7c42 for a repair)
    // Virtual40's command handlers (provisional names)
    void CancelFoundation(EEComplexUnit* inUnit);  // 0x21e853 (command 0xfb3)
    void BeginBuilding(EEComplexUnit* inUnit);  // 0x21f21a (command 0xfa2)
    // cdecl helpers of FindNextWork (static members; provisional names)
    // All three return their success flag in AL (0x21ec50, 0x21ed58, 0x21ef09);
    // FindNextWork currently ignores it.
    static bool HelpBuildNearby(EEComplexUnit* inUnit, EEUnit* inTarget);  // 0x21eb31
    static bool TendNextFarm(EEComplexUnit* inUnit, EEComplexUnit* inTarget);  // 0x21ec58
    static bool GatherNearBuilding(EEComplexUnit* inUnit, EEComplexUnit* inTarget);  // 0x21ed5e
    void FindNextWork(EEComplexUnit* inUnit);  // 0x21faa0 (command 0xfbc, for players whose Virtual74 is set)
    bool IsShiftQueueNext(EEComplexUnit* inUnit);  // port: a shift queue, not FindNextWork, follows the building
    void ClearBuildSite(EEComplexUnit* inUnit);  // 0x21fc1e (EEStepAsideFromSite on the target's tile)
    void BeginRepair(EEComplexUnit* inUnit);  // 0x220216 (command 0xfac)

    GE3DPoint mTargetLocation;              // +0x020 "Target Location"
    GE3DPoint mTargetCenter;                // +0x02c "Target Center"
    EEPlayer* mPlayer;                      // +0x038 "Player Number" (its +0x45c)
    EEUnit* mTarget;                        // +0x03c "Target Unit" (referenced; saved as its id)
    EEUnitClass* mTargetClass;              // +0x040 "Target Class" (its +0x1bc)
    EETechTreeNode* mTargetNode;            // +0x044 "Target Tech Tree Node" (its data's +0x4c)
    long mTechTreeID;                       // +0x048 "Target Tech Tree ID" (100000: none)
    long mTargetType;                       // +0x04c "Target Type" (the class's mUnitType)
    long mTargetScale;                      // +0x050 "Target Scale" (the class's +0x28c)
    long mFirstTickAmount;                  // +0x054 "Amount added to First Tick" (saved)
    bool mWorkerCountIncremented;           // +0x058 "Construction Worker Count Incremented Flag" (saved)
    bool mFirstTickAdjusted;                // +0x059 "First Tick Adjustment Made Flag" (saved)
    bool mIsRepair;                         // +0x05a "Is Repair Flag" (saved)
    bool mB05B;                             // +0x05b untouched

    static const UString sData;             // "EEUGBuildRepair-Data" (init chain 0x21e0c1)
};

#endif
