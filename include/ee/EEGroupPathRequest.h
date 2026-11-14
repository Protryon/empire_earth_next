// EEGroupPathRequest.h (RECONSTRUCTION from RTTI/vtables/ctors; layout from EEGroup.cpp's users; provisional names).
// Classes: EEGroupPathRequest.
// A path request an EEGroup keeps per path (EEGroup::mPathRequests, keyed by the path's id): the units' ids, the units'
// common flags, the path manager's request and a kind. Requires lle/LowLevelEngine.h, <vector>,
// ee/EECPAttackSubGroup.h (EECPASGFlags), ee/EEPathManager.h (EEAutoPathRequest).
#ifndef EE_EEGROUPPATHREQUEST_H
#define EE_EEGROUPPATHREQUEST_H

class EEAutoPathRequest;

// vtable 0x446f18; sizeof 0x34; ctor 0x205f60; ??_G 0x205fbb; dtor 0x205fd7
class EEGroupPathRequest {
public:
    // 0x205f60 (EEGroup::Fn206581: the path manager's request, the units' ids, 0)
    EEGroupPathRequest(EEAutoPathRequest* inPath, const std::vector<long>& inUnitIDs, long inArg3);
    virtual ~EEGroupPathRequest();          // +0x00 dtor 0x205fd7
    void Cancel();                          // 0x205ff7: the path manager drops mPath
    void RemoveUnit(long inUnitID);         // 0x1efa8e (EEActiveGroup.cpp: EEActiveGroup::RemoveUnit) (provisional name)

    std::vector<long> mUnitIDs;             // +0x04
    EECPASGFlags mFlags;                    // +0x14 (default: zero)
    EEAutoPathRequest* mPath;               // +0x2c (referenced)
    long mL30;                              // +0x30
};

#endif