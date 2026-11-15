// C:\EEX\Empire Earth\AI Thread\EECPAGOrder.h (RECONSTRUCTION; file name inferred). An order of a
// computer player's attack group (RTTI EECPAGOrder, no base; vtable 0x43a6ac: the destructor only; sizeof 0x3c;
// EECPAGOrder.cpp 0x412b1..0x4162f). Its two sub groups are referenced, saved as their ids and swizzled back through the
// owning group's commander. Names are provisional (by offset). Requires ee/EECPILocation.h, lle/FSFile.h.
#ifndef EE_EECPAGORDER_H
#define EE_EECPAGORDER_H

class EECPAttackGroup;
class EECPAttackSubGroup;

class EECPAGOrder {
public:
    EECPAGOrder();                          // 0x412b1
    EECPAGOrder(const EECPAGOrder& inOrder);   // 0x412fd
    virtual ~EECPAGOrder();                 // +0x00 (??_G 0x412e1, dtor 0x41369)

    long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // 0x413a4
    void ReleaseOtherSubGroup();            // 0x4144b
    void ReleaseSubGroup();                 // 0x41461
    long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion, EECPAttackGroup* inGroup);   // 0x41477
    long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // 0x414c6
    void SetOtherSubGroup(EECPAttackSubGroup* inSubGroup);  // 0x4159a (referenced)
    void SetSubGroup(EECPAttackSubGroup* inSubGroup);  // 0x415bb (referenced)
    EECPAGOrder& operator=(const EECPAGOrder& inOrder);   // 0x415dc

    EECPILocation mLocation;                // +0x04
    EECPAttackSubGroup* mSubGroup;          // +0x1c (referenced; saved as its id)
    EECPAttackSubGroup* mOtherSubGroup;     // +0x20 (referenced; saved as its id)
    unsigned long GetUnitID() const { return this->mUnitID; }  // inline (CheckUnit 0x4577e0 loads it before the RTTI pushes)
    unsigned long mUnitID;                  // +0x24 (= -1)
    long mL28;                              // +0x28
    long mType;                             // +0x2c
    long mSplitKind;                        // +0x30
    unsigned long mState;                   // +0x34
    bool mIsActive;                         // +0x38
};

// Attack group order types (EECPAGOrder::mType): EECPAttackGroup::ExecuteOrder 0x457727. Names are provisional (by
// behaviour).
enum EECPAGOrderType {
    kAGOrderNone = 0,              // EECPAGOrder ctor (provisional name)
    kAGOrderRemoveSubGroup = 1,    // ExecuteOrder 0x457727: RemoveSubGroup (provisional name)
    kAGOrderSplitSubGroup = 2,     // ExecuteOrder: SplitSubGroup (provisional name)
    kAGOrderMergeSubGroups = 3,    // ExecuteOrder: MergeSubGroups (provisional name)
    kAGOrderReturnToRallyPoint = 4,  // ExecuteOrder: SetState(6007) (provisional name)
    kAGOrderPrepareTravel = 5,     // ExecuteOrder: SetState(1004) (provisional name)
    kAGOrderAssignTask = 6,        // ExecuteOrder: AssignTask (provisional name)
    kAGOrderSetState = 7,          // ExecuteOrder: SetState(the order's mState) (provisional name)
    kAGOrderCheckUnit = 10,        // ExecuteOrder: CheckUnit (provisional name)
};

#endif
