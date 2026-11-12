// C:\EEX\Empire Earth\AI Thread\EEAutoPathRequest.h (RECONSTRUCTION; file name inferred).
// A queued path request (RTTI EEAutoPathRequest > UReferenceCounter; vtable 0x43c430, one slot: the deleting dtor;
// ctor 0xa5c31 first called from 0xa6948 in EEPathManager.cpp; the manager's gPathManager.CancelAutoPathRequest releases them).
// Requires lle/UReferenceCounter.h, <vector>, ee/EETileCoordinate.h. Names are provisional.
#ifndef EE_EEAUTOPATHREQUEST_H
#define EE_EEAUTOPATHREQUEST_H

#include <vector>
#include "ee/EETileCoordinate.h"

class EEAutoPathRequest : public UReferenceCounter {
public:
    EEAutoPathRequest(long inRequestID, long inPlayerID, const EETileCoordinate& inFrom, const EETileCoordinate& inTo,
                      const EECPASGFlags& inFlags);   // 0xa5c31 (ret 0x14)
    virtual ~EEAutoPathRequest();           // 0xa5cc6 (??_G 0xa5caa)

    std::vector<EETileCoordinate> mPath;    // +0x08
    // the walker flags: the ctor copy-constructs them (EECPASGFlags copy ctor: memcpy, mL00 = 0); StartGraphPath
    // 0xa69e2 assigns them to its goal's mPathFlags (0xaa1da). 0xa5ce0/0xa5cf6 (+0x18 Dereference/Reference) are
    // EEPathManagerNode's ReleaseRequest/SetRequest, not the request's (every caller is node code)
    EECPASGFlags mFlags;                    // +0x18
    EETileCoordinate mFrom;                 // +0x30
    EETileCoordinate mTo;                   // +0x38
    long mPlayerID;                         // +0x40
    long mRequestID;                        // +0x44
    long mPathLength;                       // +0x48
    bool mIsDone;                           // +0x4c
};

// port (save version 0.277): a request as the saved game holds it (every field; EEPathManager.cpp), for the owners
// that keep one across a save (EECPWallManager, the graph searches in progress). The load's is a new request (one
// reference, the caller's)
class FSFile;
void EESaveAutoPathRequest(FSFile& outFile, const EEAutoPathRequest& inRequest);
long EELoadAutoPathRequest(FSFile& inFile, EEAutoPathRequest*& outRequest);

#endif
